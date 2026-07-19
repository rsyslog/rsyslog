/* ratelimit.c
 * support for rate-limiting sources, including "last message
 * repeated n times" processing.
 *
 * Copyright 2012-2026 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *       -or-
 *       see COPYING.ASL20 in the source distribution
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#ifdef HAVE_SYS_INOTIFY_H
    #include <sys/inotify.h>
#endif

#include "rsyslog.h"
#include "errmsg.h"
#include "ratelimit.h"
#include "datetime.h"
#include "atomic.h"
#include "parser.h"
#include "unicode-helper.h"
#include "msg.h"
#include "rsconf.h"
#include "dirty.h"
#include "hashtable.h"
#include "statsobj.h"
#include "template.h"
#include "hashtable_itr.h"
#include "srUtils.h"
#ifdef HAVE_LIBYAML
    #include <yaml.h>
#endif

/* definitions for objects we access */
DEFobjStaticHelpers;
DEFobjCurrIf(glbl) DEFobjCurrIf(datetime) DEFobjCurrIf(parser) DEFobjCurrIf(statsobj)


    static inline unsigned int ratelimitSharedLoadUInt(unsigned int *value, pthread_mutex_t *mut) {
    (void)mut;
    return ATOMIC_LOAD_32BIT_unsigned(value, mut);
}

static inline void ratelimitSharedStoreUInt(unsigned int *value, pthread_mutex_t *mut, unsigned int newval) {
    (void)mut;
    ATOMIC_STORE_32BIT_unsigned(value, mut, newval);
}

static inline int ratelimitSharedLoadSeverity(int *value, pthread_mutex_t *mut) {
    (void)mut;
    return ATOMIC_LOAD_32BIT(value, mut);
}

static inline void ratelimitSharedStoreSeverity(int *value, pthread_mutex_t *mut, int newval) {
    (void)mut;
    ATOMIC_STORE_32BIT(value, mut, newval);
}

static inline unsigned int ratelimitPerSourceLoadEnabled(unsigned int *value, pthread_mutex_t *mut) {
    (void)mut;
    return ATOMIC_LOAD_32BIT_unsigned(value, mut);
}

static inline void ratelimitPerSourceStoreEnabled(unsigned int *value, pthread_mutex_t *mut, unsigned int newval) {
    (void)mut;
    ATOMIC_STORE_32BIT_unsigned(value, mut, newval);
}

static void ratelimitUnregisterSharedWatchers(ratelimit_shared_t *shared);
static void ratelimitDestroyPerSourceShards(ratelimit_shared_t *shared);
static rsRetVal ratelimitInitPerSourceShards(ratelimit_shared_t *shared, unsigned int max_states);
static void ratelimitSetPerSourceShardCaps(ratelimit_shared_t *shared, unsigned int max_states);
static void ratelimitFreePerSourceEntry(void *ptr);

static void ratelimitSharedRefsInit(ratelimit_shared_t *const shared, const unsigned int refcnt) {
    pthread_mutex_init(&shared->ref_mut, NULL);
    pthread_cond_init(&shared->ref_cond, NULL);
    shared->refcnt = refcnt;
    shared->ref_initialized = 1;
}

static void ratelimitSharedAddRef(ratelimit_shared_t *const shared) {
    pthread_mutex_lock(&shared->ref_mut);
    assert(shared->refcnt > 0);
    assert(shared->refcnt < UINT_MAX);
    shared->refcnt++;
    pthread_mutex_unlock(&shared->ref_mut);
}

static void ratelimitSharedReleaseRef(ratelimit_shared_t *const shared) {
    pthread_mutex_lock(&shared->ref_mut);
    assert(shared->refcnt > 0);
    shared->refcnt--;
    if (shared->refcnt == 0) {
        pthread_cond_broadcast(&shared->ref_cond);
    }
    pthread_mutex_unlock(&shared->ref_mut);
}

static void ratelimitSharedDropRegistryRefAndWait(ratelimit_shared_t *const shared) {
    pthread_mutex_lock(&shared->ref_mut);
    assert(shared->refcnt > 0);
    shared->refcnt--;
    while (shared->refcnt != 0) {
        pthread_cond_wait(&shared->ref_cond, &shared->ref_mut);
    }
    pthread_mutex_unlock(&shared->ref_mut);
}

static void ratelimitSharedRefsDestroy(ratelimit_shared_t *const shared) {
    if (shared->ref_initialized) {
        pthread_cond_destroy(&shared->ref_cond);
        pthread_mutex_destroy(&shared->ref_mut);
        shared->ref_initialized = 0;
    }
}

static ratelimit_cfg_bucket_t *ratelimitCfgBucket(ratelimit_cfgs_t *const cfgs,
                                                  const char *const name,
                                                  unsigned int *const hashvalue) {
    unsigned int local_hash;
    assert(cfgs != NULL);
    assert(name != NULL);
    assert(cfgs->shards[0].ht != NULL);
    /* Every shard uses the same hashtable hash function; compute the mixed
     * hash once and reuse it for both shard selection and in-shard lookup.
     */
    local_hash = hashtable_hash(cfgs->shards[0].ht, (void *)name);
    if (hashvalue != NULL) {
        *hashvalue = local_hash;
    }
    return &cfgs->shards[local_hash % RATELIMIT_CFG_SHARDS];
}

/**
 * Classify the per-source key template for parsing requirements.
 *
 * Allowed exceptions (no parse):
 * - %fromhost-ip%
 * - %fromhost%
 * - %fromhost-ip%:%fromhost-port%
 * - %fromhost%:%fromhost-port%
 *
 * Everything else falls back to template evaluation and requires parsing.
 */
static enum ratelimit_ps_key_mode perSourceKeyModeFromTemplate(const struct template *const pTpl) {
    const struct templateEntry *entry;

    if (pTpl == NULL) {
        return RL_PS_KEY_TPL;
    }
    if (pTpl->pStrgen != NULL || pTpl->bHaveSubtree) {
        return RL_PS_KEY_TPL;
    }
    if (pTpl->pEntryRoot == NULL) {
        return RL_PS_KEY_TPL;
    }
    if (pTpl->tpenElements == 1) {
        entry = pTpl->pEntryRoot;
        if (entry->pNext != NULL) {
            return RL_PS_KEY_TPL;
        }
        if (entry->eEntryType != FIELD) {
            return RL_PS_KEY_TPL;
        }
        if (entry->bComplexProcessing) {
            return RL_PS_KEY_TPL;
        }
        if (entry->data.field.msgProp.id == PROP_FROMHOST_IP || entry->data.field.msgProp.id == PROP_FROMHOST) {
            return (entry->data.field.msgProp.id == PROP_FROMHOST_IP) ? RL_PS_KEY_FROMHOST_IP : RL_PS_KEY_FROMHOST;
        }
        return RL_PS_KEY_TPL;
    }
    if (pTpl->tpenElements == 3) {
        const struct templateEntry *const first = pTpl->pEntryRoot;
        const struct templateEntry *const second = first->pNext;
        const struct templateEntry *const third = second ? second->pNext : NULL;

        if (third == NULL || third->pNext != NULL) {
            return RL_PS_KEY_TPL;
        }
        if (first->eEntryType != FIELD || second->eEntryType != CONSTANT || third->eEntryType != FIELD) {
            return RL_PS_KEY_TPL;
        }
        if (first->bComplexProcessing || second->bComplexProcessing || third->bComplexProcessing) {
            return RL_PS_KEY_TPL;
        }
        if (second->data.constant.iLenConstant != 1 || second->data.constant.pConstant[0] != ':') {
            return RL_PS_KEY_TPL;
        }
        if ((first->data.field.msgProp.id == PROP_FROMHOST_IP || first->data.field.msgProp.id == PROP_FROMHOST) &&
            third->data.field.msgProp.id == PROP_FROMHOST_PORT) {
            return (first->data.field.msgProp.id == PROP_FROMHOST_IP) ? RL_PS_KEY_FROMHOST_IP_PORT
                                                                      : RL_PS_KEY_FROMHOST_PORT;
        }
        return RL_PS_KEY_TPL;
    }

    return RL_PS_KEY_TPL;
}

static rsRetVal ratelimitLookupPerSourceTemplate(rsconf_t *conf,
                                                 const char *tpl_name,
                                                 struct template **tpl,
                                                 sbool *is_default) {
    const char *lookup_name;
    DEFiRet;

    if (conf == NULL || tpl == NULL || is_default == NULL) ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    lookup_name = (tpl_name == NULL) ? "RSYSLOG_PerSourceKey" : tpl_name;
    *tpl = tplFind(conf, (char *)lookup_name, (int)strlen(lookup_name));
    if (*tpl == NULL) {
        ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
    }
    *is_default = (tpl_name == NULL);

finalize_it:
    RETiRet;
}

static void ratelimitFreeShared(void *ptr) {
    ratelimit_shared_t *shared = (ratelimit_shared_t *)ptr;
    if (shared == NULL) return;
    ratelimitSharedDropRegistryRefAndWait(shared);
    ratelimitUnregisterSharedWatchers(shared);
    pthread_mutex_destroy(&shared->mut);
    free(shared->policy_file);
    ratelimitDestroyPerSourceShards(shared);
    if (shared->per_source_stats != NULL) {
        statsobj.Destruct(&shared->per_source_stats);
    }
    free(shared->per_source_top_ctrs);
    free(shared->per_source_top_values);
    if (shared->per_source_top_keys != NULL) {
        for (unsigned int i = 0; i < shared->per_source_topn; ++i) {
            free(shared->per_source_top_keys[i]);
        }
    }
    free(shared->per_source_top_keys);
    free(shared->per_source_key_tpl_name);
    pthread_mutex_destroy(&shared->per_source_key_policy_mut);
    pthread_mutex_destroy(&shared->per_source_policy_mut);
    free(shared->per_source_policy_file);
    ratelimitSharedRefsDestroy(shared);
    /* shared->name is the key, freed by hashtable_destroy via freekey(); do not free here */
    free(shared);
}

rsRetVal ratelimit_cfgsInit(ratelimit_cfgs_t *cfgs) {
    DEFiRet;

    memset(cfgs, 0, sizeof(*cfgs));
    for (unsigned int i = 0; i < RATELIMIT_CFG_SHARDS; ++i) {
        if (pthread_rwlock_init(&cfgs->shards[i].lock, NULL) != 0) {
            ABORT_FINALIZE(RS_RET_ERR);
        }
        cfgs->shards[i].lock_initialized = 1;
        CHKmalloc(cfgs->shards[i].ht = create_hashtable(16, hash_from_string, key_equals_string, ratelimitFreeShared));
    }

finalize_it:
    if (iRet != RS_RET_OK) {
        ratelimit_cfgsDestruct(cfgs);
    }
    RETiRet;
}

void ratelimit_cfgsDestruct(ratelimit_cfgs_t *cfgs) {
    for (unsigned int i = 0; i < RATELIMIT_CFG_SHARDS; ++i) {
        if (cfgs->shards[i].ht != NULL && hashtable_count(cfgs->shards[i].ht) > 0) {
            struct hashtable_itr *itr = hashtable_iterator(cfgs->shards[i].ht);
            if (itr != NULL) {
                do {
                    ratelimit_shared_t *shared = (ratelimit_shared_t *)hashtable_iterator_value(itr);
                    ratelimitUnregisterSharedWatchers(shared);
                } while (hashtable_iterator_advance(itr));
            }
            free(itr);
        }
        if (cfgs->shards[i].ht != NULL) {
            hashtable_destroy(cfgs->shards[i].ht, 1); /* 1 = free values */
            cfgs->shards[i].ht = NULL;
        }
        if (cfgs->shards[i].lock_initialized) {
            pthread_rwlock_destroy(&cfgs->shards[i].lock);
            cfgs->shards[i].lock_initialized = 0;
        }
    }
}


/*
 * Concurrency & Locking
 * - ratelimit_shared_t->mut guards shared policy updates for global limits.
 * - per-source limits use ratelimit_shared_t->per_source_policy_mut to guard
 *   low-frequency policy metadata and fallback atomics. Versioned source-key
 *   policy fields are atomically published; per_source_key_policy_mut is used
 *   only as the no-atomics fallback. Message-time source accounting is
 *   partitioned across ratelimit_shared_t->per_source_shards; each shard mutex
 *   guards that shard's hashtable, LRU list, and counters.
 * - ratelimit_cfgs_t shards guard the named configuration registry. Config
 *   lookup and insertion lock only the shard selected by the config name.
 * - ratelimit_t->mut guards per-instance counters when thread-safe mode is enabled.
 */

#define RATELIMIT_PERSOURCE_DEFAULT_MAX_STATES 10000U
#define RATELIMIT_PERSOURCE_DEFAULT_TOPN 10U
#define RATELIMIT_POLICY_WATCH_DEBOUNCE_DFLT_MS 5000U

struct ratelimit_ps_override_s {
    char *key;
    unsigned int max;
    unsigned int window;
    sbool has_max;
    sbool has_window;
};

typedef struct ratelimit_ps_override_s ratelimit_ps_override_t;

struct ratelimit_ps_entry_s {
    char *key;
    unsigned int count;
    time_t window_start;
    uint64_t allowed;
    uint64_t dropped;
    time_t last_seen;
    sbool state_active;
    sbool has_override;
    unsigned int max;
    unsigned int window;
    sbool has_max;
    sbool has_window;
    struct ratelimit_ps_entry_s *lru_prev;
    struct ratelimit_ps_entry_s *lru_next;
};

typedef struct ratelimit_ps_entry_s ratelimit_ps_entry_t;

struct ratelimit_ps_policy_s {
    unsigned int default_max;
    unsigned int default_window;
    struct hashtable *overrides;
};

typedef struct ratelimit_ps_policy_s ratelimit_ps_policy_t;

typedef struct ratelimit_ps_key_policy_s {
    sbool key_tpl_default;
    sbool key_needs_parsing;
    enum ratelimit_ps_key_mode key_mode;
    struct template *key_tpl;
} ratelimit_ps_key_policy_t;

#define RL_PS_KEY_POLICY_MODE_MASK 0xffU
#define RL_PS_KEY_POLICY_TPL_DEFAULT (1U << 8)
#define RL_PS_KEY_POLICY_NEEDS_PARSING (1U << 9)

typedef struct ratelimit_policy_file_s {
    sbool has_scope;
    sbool has_output_mode;
    sbool has_interval;
    sbool has_burst;
    sbool has_severity;
    ratelimit_scope_t scope;
    ratelimit_output_mode_t output_mode;
    unsigned int interval;
    unsigned int burst;
    int severity;
    sbool has_per_source;
    sbool per_source_enabled;
    char *per_source_key_tpl_name;
    unsigned int per_source_max_states;
    unsigned int per_source_topn;
    ratelimit_ps_policy_t *per_source_policy;
} ratelimit_policy_file_t;

enum ratelimit_watch_kind { RATELIMIT_WATCH_GLOBAL = 0, RATELIMIT_WATCH_PERSOURCE };

static rsRetVal parseDurationMillis(const char *value, unsigned int *out);
#ifdef HAVE_LIBYAML
static rsRetVal parseUnsignedInt(const char *value, unsigned int *out);
static rsRetVal parseDurationSeconds(const char *value, unsigned int *out);
static void ratelimitFreePerSourceOverrideValue(void *ptr);
static void ratelimitFreePerSourceOverrideDirect(void *ptr);
#endif
static rsRetVal ratelimitReloadPolicyFile(ratelimit_shared_t *shared, const char *trigger);
static rsRetVal ratelimitReloadPerSourcePolicyFile(ratelimit_shared_t *shared, const char *trigger);
static rsRetVal ratelimitRegisterWatchTargets(ratelimit_shared_t *shared);

/* The caller serializes publishers with per_source_policy_mut. The odd/even
 * sequence lets readers reject fields observed across an update. */
static void ratelimitPublishPerSourceKeyPolicy(ratelimit_shared_t *shared,
                                               struct template *key_tpl,
                                               const sbool key_tpl_default) {
    const enum ratelimit_ps_key_mode key_mode = perSourceKeyModeFromTemplate(key_tpl);
    unsigned int bits = (unsigned int)key_mode;

    if (key_tpl_default) bits |= RL_PS_KEY_POLICY_TPL_DEFAULT;
    if (key_mode == RL_PS_KEY_TPL) bits |= RL_PS_KEY_POLICY_NEEDS_PARSING;

    ATOMIC_INC(&shared->per_source_key_policy_seq, &shared->per_source_key_policy_mut);
    ATOMIC_STORE_PTR((void **)&shared->per_source_key_tpl, &shared->per_source_key_policy_mut, key_tpl);
    ATOMIC_STORE_32BIT_unsigned(&shared->per_source_key_policy_bits, &shared->per_source_key_policy_mut, bits);
    ATOMIC_INC(&shared->per_source_key_policy_seq, &shared->per_source_key_policy_mut);
}

static void ratelimitLoadPerSourceKeyPolicy(ratelimit_shared_t *shared, ratelimit_ps_key_policy_t *policy) {
    unsigned int seq_before;
    unsigned int seq_after;
    unsigned int bits;
    struct template *key_tpl;

    for (;;) {
        seq_before = ATOMIC_LOAD_32BIT_unsigned(&shared->per_source_key_policy_seq, &shared->per_source_key_policy_mut);
        if (seq_before & 1U) continue;
        bits = ATOMIC_LOAD_32BIT_unsigned(&shared->per_source_key_policy_bits, &shared->per_source_key_policy_mut);
        key_tpl = (struct template *)ATOMIC_LOAD_PTR((void **)&shared->per_source_key_tpl,
                                                     &shared->per_source_key_policy_mut);
        seq_after = ATOMIC_LOAD_32BIT_unsigned(&shared->per_source_key_policy_seq, &shared->per_source_key_policy_mut);
        if (seq_before == seq_after && !(seq_after & 1U)) break;
    }

    policy->key_mode = (enum ratelimit_ps_key_mode)(bits & RL_PS_KEY_POLICY_MODE_MASK);
    policy->key_tpl_default = (bits & RL_PS_KEY_POLICY_TPL_DEFAULT) != 0;
    policy->key_needs_parsing = (bits & RL_PS_KEY_POLICY_NEEDS_PARSING) != 0;
    policy->key_tpl = key_tpl;
}

static rsRetVal parseDurationMillis(const char *value, unsigned int *out) {
    char *end = NULL;
    unsigned long val;
    unsigned long long multiplier = 1000ULL;
    unsigned long long total;
    DEFiRet;

    if (value == NULL || out == NULL) ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    while (isspace((unsigned char)*value)) value++;
    if (*value == '-') ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);

    errno = 0;
    val = strtoul(value, &end, 10);
    if (errno != 0 || end == value) {
        ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
    }
    while (isspace((unsigned char)*end)) end++;
    if (*end == '\0' || (!strcmp(end, "s"))) {
        multiplier = 1000ULL;
    } else if (!strcmp(end, "ms")) {
        multiplier = 1ULL;
    } else if (!strcmp(end, "m")) {
        multiplier = 60ULL * 1000ULL;
    } else if (!strcmp(end, "h")) {
        multiplier = 60ULL * 60ULL * 1000ULL;
    } else {
        ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
    }

    if ((unsigned long long)val > ((unsigned long long)UINT_MAX / multiplier)) {
        ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
    }

    total = (unsigned long long)val * multiplier;
    if (total > UINT_MAX) {
        ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
    }

    *out = (unsigned int)total;
finalize_it:
    RETiRet;
}

#ifdef HAVE_LIBYAML
static rsRetVal parseDurationSeconds(const char *value, unsigned int *out) {
    unsigned int ms = 0;
    DEFiRet;

    CHKiRet(parseDurationMillis(value, &ms));
    if (ms % 1000U != 0) {
        ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
    }
    *out = ms / 1000U;
finalize_it:
    RETiRet;
}
#endif

static const char *ratelimitWatchKindName(enum ratelimit_watch_kind kind) {
    return (kind == RATELIMIT_WATCH_PERSOURCE) ? "per-source policy" : "policy";
}

static void ratelimitWatchReloadPolicyCb(void *ctx, const char *trigger) {
    ratelimitReloadPolicyFile((ratelimit_shared_t *)ctx, trigger);
}

static void ratelimitWatchReloadPerSourcePolicyCb(void *ctx, const char *trigger) {
    ratelimitReloadPerSourcePolicyFile((ratelimit_shared_t *)ctx, trigger);
}

static void ratelimitUnregisterSharedWatchers(ratelimit_shared_t *shared) {
    if (shared == NULL) {
        return;
    }
    if (shared->policy_watch_handle != NULL) {
        rswatchUnregister(shared->policy_watch_handle);
        shared->policy_watch_handle = NULL;
    }
    if (shared->per_source_policy_watch_handle != NULL) {
        rswatchUnregister(shared->per_source_policy_watch_handle);
        shared->per_source_policy_watch_handle = NULL;
    }
}

static rsRetVal ratelimitRegisterWatchTargets(ratelimit_shared_t *shared) {
    DEFiRet;
    rsRetVal localRet;
    rswatch_desc_t desc;
    const char *per_source_policy_file = NULL;
    sbool per_source_policy_from_policy_file;

    if (shared == NULL || !shared->policy_watch) {
        FINALIZE;
    }

    pthread_mutex_lock(&shared->per_source_policy_mut);
    per_source_policy_file = shared->per_source_policy_file;
    per_source_policy_from_policy_file = shared->per_source_policy_from_policy_file;
    pthread_mutex_unlock(&shared->per_source_policy_mut);

    if (shared->policy_file == NULL && per_source_policy_file == NULL) {
        LogMsg(0, RS_RET_OK, LOG_WARNING,
               "ratelimit: policyWatch enabled for '%s' but no policy file is configured; using HUP-only reload",
               shared->name);
        FINALIZE;
    }

    if (shared->policy_file != NULL) {
        memset(&desc, 0, sizeof(desc));
        desc.id = shared->name;
        desc.path = shared->policy_file;
        desc.debounce_ms = shared->policy_watch_debounce_ms;
        desc.cb = ratelimitWatchReloadPolicyCb;
        desc.ctx = shared;
        localRet = rswatchRegister(&desc, &shared->policy_watch_handle);
        if (localRet != RS_RET_OK) {
            LogMsg(0, localRet, LOG_WARNING,
                   "ratelimit: policyWatch requested for '%s' but automatic reload unavailable for %s '%s'; using "
                   "HUP-only reload",
                   shared->name, ratelimitWatchKindName(RATELIMIT_WATCH_GLOBAL), shared->policy_file);
        }
    }
    if (per_source_policy_file != NULL && !per_source_policy_from_policy_file) {
        memset(&desc, 0, sizeof(desc));
        desc.id = shared->name;
        desc.path = per_source_policy_file;
        desc.debounce_ms = shared->policy_watch_debounce_ms;
        desc.cb = ratelimitWatchReloadPerSourcePolicyCb;
        desc.ctx = shared;
        localRet = rswatchRegister(&desc, &shared->per_source_policy_watch_handle);
        if (localRet != RS_RET_OK) {
            LogMsg(0, localRet, LOG_WARNING,
                   "ratelimit: policyWatch requested for '%s' but automatic reload unavailable for %s '%s'; using "
                   "HUP-only reload",
                   shared->name, ratelimitWatchKindName(RATELIMIT_WATCH_PERSOURCE), per_source_policy_file);
        }
    }

finalize_it:
    RETiRet;
}


static void ratelimitPolicyFileDestruct(ratelimit_policy_file_t *policy) {
    if (policy == NULL) return;
    free(policy->per_source_key_tpl_name);
    if (policy->per_source_policy != NULL) {
        if (policy->per_source_policy->overrides != NULL) hashtable_destroy(policy->per_source_policy->overrides, 1);
        free(policy->per_source_policy);
    }
    memset(policy, 0, sizeof(*policy));
}

static rsRetVal parseBoolValue(const char *value, sbool *out) {
    DEFiRet;

    if (value == NULL || out == NULL) ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    if (!strcasecmp(value, "on") || !strcasecmp(value, "yes") || !strcasecmp(value, "true") || !strcmp(value, "1")) {
        *out = 1;
    } else if (!strcasecmp(value, "off") || !strcasecmp(value, "no") || !strcasecmp(value, "false") ||
               !strcmp(value, "0")) {
        *out = 0;
    } else {
        ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
    }

finalize_it:
    RETiRet;
}

static rsRetVal parsePolicyScopeValue(const char *value, ratelimit_scope_t *out) {
    DEFiRet;

    if (value == NULL || out == NULL) ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    if (!strcasecmp(value, "input")) {
        *out = RATELIMIT_SCOPE_INPUT;
    } else if (!strcasecmp(value, "output")) {
        *out = RATELIMIT_SCOPE_OUTPUT;
    } else {
        ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
    }

finalize_it:
    RETiRet;
}

static rsRetVal parseOutputModeValue(const char *value, ratelimit_output_mode_t *out) {
    DEFiRet;

    if (value == NULL || out == NULL) ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    if (!strcasecmp(value, "drop")) {
        *out = RATELIMIT_OUTPUT_MODE_DROP;
    } else if (!strcasecmp(value, "pace")) {
        *out = RATELIMIT_OUTPUT_MODE_PACE;
    } else {
        ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
    }

finalize_it:
    RETiRet;
}

static rsRetVal parseSeverityValue(const char *value, int *out) {
    char *end = NULL;
    long numeric;
    int severity;
    DEFiRet;

    if (value == NULL || out == NULL) ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    while (isspace((unsigned char)*value)) value++;
    if (*value == '-' || isdigit((unsigned char)*value)) {
        errno = 0;
        numeric = strtol(value, &end, 10);
        if (errno != 0 || end == value || *end != '\0' || numeric < -1 || numeric > 127) {
            ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
        }
        severity = (int)numeric;
    } else {
        severity = decodeSyslogName((uchar *)value, syslogPriNames);
        if (severity < 0 || severity > 127) {
            ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
        }
    }
    *out = severity;

finalize_it:
    RETiRet;
}

static rsRetVal parsePolicyFile(const char *policy_file, ratelimit_policy_file_t *policy) {
    DEFiRet;
#ifdef HAVE_LIBYAML
    FILE *fh = NULL;
    yaml_parser_t yamlParser;
    int bParserInitialized = 0;
    yaml_event_t event;
    unsigned int depth = 0;
    enum policy_ctx { CTX_TOP = 0, CTX_PERSOURCE, CTX_PS_DEFAULT, CTX_PS_OVERRIDE_ITEM } ctxStack[8];
    int expectKey[8] = {0};
    char *last_key = NULL;
    sbool inOverridesSeq = 0;
    sbool default_max_seen = 0;
    sbool default_window_seen = 0;
    unsigned int override_count = 0;
    ratelimit_ps_override_t *current_override = NULL;

    if (policy == NULL) ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    memset(policy, 0, sizeof(*policy));

    fh = fopen(policy_file, "r");
    if (fh == NULL) {
        LogError(errno, RS_RET_FILE_NOT_FOUND, "ratelimit: error opening policy file %s", policy_file);
        ABORT_FINALIZE(RS_RET_FILE_NOT_FOUND);
    }

    if (!yaml_parser_initialize(&yamlParser)) {
        LogError(0, RS_RET_INTERNAL_ERROR, "ratelimit: failed to initialize yaml parser");
        ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
    }
    bParserInitialized = 1;

    yaml_parser_set_input_file(&yamlParser, fh);

    while (1) {
        if (!yaml_parser_parse(&yamlParser, &event)) {
            LogError(0, RS_RET_CONF_PARAM_INVLD, "ratelimit: yaml parser error in policy file %s: %s", policy_file,
                     yamlParser.problem ? yamlParser.problem : "unknown error");
            ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
        }

        PRAGMA_DIAGNOSTIC_PUSH
        PRAGMA_IGNORE_Wswitch_enum switch (event.type) {
            case YAML_NO_EVENT:
            case YAML_STREAM_START_EVENT:
            case YAML_DOCUMENT_START_EVENT:
            case YAML_DOCUMENT_END_EVENT:
            case YAML_ALIAS_EVENT:
                break;
            case YAML_MAPPING_START_EVENT:
                if (depth >= 8) {
                    LogError(0, RS_RET_CONF_PARAM_INVLD, "ratelimit: yaml nesting too deep in %s", policy_file);
                    ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
                }
                if (depth == 0) {
                    ctxStack[depth] = CTX_TOP;
                } else if (inOverridesSeq) {
                    if (override_count >= 10000U) {
                        LogError(0, RS_RET_CONF_PARAM_INVLD, "ratelimit: too many per-source overrides in %s (max %u)",
                                 policy_file, 10000U);
                        ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
                    }
                    ctxStack[depth] = CTX_PS_OVERRIDE_ITEM;
                    CHKmalloc(current_override = calloc(1, sizeof(*current_override)));
                    override_count++;
                } else if (last_key != NULL && !strcmp(last_key, "perSource")) {
                    ctxStack[depth] = CTX_PERSOURCE;
                    policy->has_per_source = 1;
                    policy->per_source_enabled = 1;
                    CHKmalloc(policy->per_source_policy = calloc(1, sizeof(*policy->per_source_policy)));
                    policy->per_source_policy->overrides =
                        create_hashtable(32, hash_from_string, key_equals_string, ratelimitFreePerSourceOverrideValue);
                    if (policy->per_source_policy->overrides == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
                } else if (depth > 0 && ctxStack[depth - 1] == CTX_PERSOURCE && last_key != NULL &&
                           !strcmp(last_key, "default")) {
                    ctxStack[depth] = CTX_PS_DEFAULT;
                } else {
                    ctxStack[depth] = CTX_TOP;
                }
                if (depth > 0 && last_key != NULL) {
                    expectKey[depth - 1] = 1;
                }
                expectKey[depth] = 1;
                depth++;
                free(last_key);
                last_key = NULL;
                break;
            case YAML_MAPPING_END_EVENT:
                if (depth > 0) {
                    depth--;
                    if (ctxStack[depth] == CTX_PS_OVERRIDE_ITEM && current_override != NULL) {
                        if (current_override->key == NULL) {
                            LogError(0, RS_RET_CONF_PARAM_INVLD, "ratelimit: per-source override missing key in %s",
                                     policy_file);
                            ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
                        }
                        if (hashtable_insert(policy->per_source_policy->overrides, current_override->key,
                                             current_override) == 0) {
                            LogError(0, RS_RET_OUT_OF_MEMORY, "ratelimit: error inserting per-source override '%s'",
                                     current_override->key);
                            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
                        }
                        current_override = NULL;
                    }
                }
                free(last_key);
                last_key = NULL;
                break;
            case YAML_SEQUENCE_START_EVENT:
                if (depth > 0 && ctxStack[depth - 1] == CTX_PERSOURCE && last_key != NULL &&
                    !strcmp(last_key, "overrides")) {
                    inOverridesSeq = 1;
                }
                if (depth > 0 && last_key != NULL) {
                    expectKey[depth - 1] = 1;
                }
                free(last_key);
                last_key = NULL;
                break;
            case YAML_SEQUENCE_END_EVENT:
                inOverridesSeq = 0;
                free(last_key);
                last_key = NULL;
                break;
            case YAML_SCALAR_EVENT:
                if (depth == 0) break;
                if (expectKey[depth - 1]) {
                    free(last_key);
                    CHKmalloc(last_key = strdup((char *)event.data.scalar.value));
                    expectKey[depth - 1] = 0;
                } else {
                    const char *val = (const char *)event.data.scalar.value;
                    unsigned int uintval = 0;
                    int severity = 0;
                    sbool boolval = 0;

                    if (ctxStack[depth - 1] == CTX_TOP) {
                        if (last_key != NULL && !strcmp(last_key, "scope")) {
                            if (parsePolicyScopeValue(val, &policy->scope) != RS_RET_OK) {
                                LogError(0, RS_RET_CONF_PARAM_INVLD, "ratelimit: invalid scope value '%s' in %s", val,
                                         policy_file);
                                ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
                            }
                            policy->has_scope = 1;
                        } else if (last_key != NULL && !strcmp(last_key, "mode")) {
                            if (parseOutputModeValue(val, &policy->output_mode) != RS_RET_OK) {
                                LogError(0, RS_RET_CONF_PARAM_INVLD, "ratelimit: invalid mode value '%s' in %s", val,
                                         policy_file);
                                ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
                            }
                            policy->has_output_mode = 1;
                        } else if (last_key != NULL && !strcmp(last_key, "interval")) {
                            if (parseUnsignedInt(val, &uintval) != RS_RET_OK) {
                                LogError(0, RS_RET_CONF_PARAM_INVLD, "ratelimit: invalid interval value '%s' in %s",
                                         val, policy_file);
                                ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
                            }
                            policy->interval = uintval;
                            policy->has_interval = 1;
                        } else if (last_key != NULL && !strcmp(last_key, "burst")) {
                            if (parseUnsignedInt(val, &uintval) != RS_RET_OK) {
                                LogError(0, RS_RET_CONF_PARAM_INVLD, "ratelimit: invalid burst value '%s' in %s", val,
                                         policy_file);
                                ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
                            }
                            policy->burst = uintval;
                            policy->has_burst = 1;
                        } else if (last_key != NULL && !strcmp(last_key, "severity")) {
                            if (parseSeverityValue(val, &severity) != RS_RET_OK) {
                                LogError(0, RS_RET_CONF_PARAM_INVLD, "ratelimit: invalid severity value '%s' in %s",
                                         val, policy_file);
                                ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
                            }
                            policy->severity = severity;
                            policy->has_severity = 1;
                        }
                    } else if (ctxStack[depth - 1] == CTX_PERSOURCE) {
                        if (last_key != NULL && !strcmp(last_key, "enabled")) {
                            if (parseBoolValue(val, &boolval) != RS_RET_OK) {
                                LogError(0, RS_RET_CONF_PARAM_INVLD,
                                         "ratelimit: invalid perSource.enabled value '%s' in %s", val, policy_file);
                                ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
                            }
                            policy->per_source_enabled = boolval;
                        } else if (last_key != NULL && !strcmp(last_key, "keyTemplate")) {
                            free(policy->per_source_key_tpl_name);
                            CHKmalloc(policy->per_source_key_tpl_name = strdup(val));
                        } else if (last_key != NULL && !strcmp(last_key, "maxStates")) {
                            if (parseUnsignedInt(val, &policy->per_source_max_states) != RS_RET_OK) {
                                LogError(0, RS_RET_CONF_PARAM_INVLD,
                                         "ratelimit: invalid perSource.maxStates value '%s' in %s", val, policy_file);
                                ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
                            }
                        } else if (last_key != NULL && !strcmp(last_key, "topN")) {
                            if (parseUnsignedInt(val, &policy->per_source_topn) != RS_RET_OK) {
                                LogError(0, RS_RET_CONF_PARAM_INVLD,
                                         "ratelimit: invalid perSource.topN value '%s' in %s", val, policy_file);
                                ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
                            }
                        }
                    } else if (ctxStack[depth - 1] == CTX_PS_DEFAULT && policy->per_source_policy != NULL) {
                        if (last_key != NULL && !strcmp(last_key, "max")) {
                            if (parseUnsignedInt(val, &policy->per_source_policy->default_max) != RS_RET_OK) {
                                LogError(0, RS_RET_CONF_PARAM_INVLD,
                                         "ratelimit: invalid perSource.default.max value '%s' in %s", val, policy_file);
                                ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
                            }
                            default_max_seen = 1;
                        } else if (last_key != NULL && !strcmp(last_key, "window")) {
                            if (parseDurationSeconds(val, &policy->per_source_policy->default_window) != RS_RET_OK) {
                                LogError(0, RS_RET_CONF_PARAM_INVLD,
                                         "ratelimit: invalid perSource.default.window value '%s' in %s", val,
                                         policy_file);
                                ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
                            }
                            default_window_seen = 1;
                        }
                    } else if (ctxStack[depth - 1] == CTX_PS_OVERRIDE_ITEM && current_override != NULL) {
                        if (last_key != NULL && !strcmp(last_key, "key")) {
                            free(current_override->key);
                            CHKmalloc(current_override->key = strdup(val));
                        } else if (last_key != NULL && !strcmp(last_key, "max")) {
                            if (parseUnsignedInt(val, &current_override->max) != RS_RET_OK) {
                                LogError(0, RS_RET_CONF_PARAM_INVLD,
                                         "ratelimit: invalid perSource override.max value '%s' in %s", val,
                                         policy_file);
                                ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
                            }
                            current_override->has_max = 1;
                        } else if (last_key != NULL && !strcmp(last_key, "window")) {
                            if (parseDurationSeconds(val, &current_override->window) != RS_RET_OK) {
                                LogError(0, RS_RET_CONF_PARAM_INVLD,
                                         "ratelimit: invalid perSource override.window value '%s' in %s", val,
                                         policy_file);
                                ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
                            }
                            current_override->has_window = 1;
                        }
                    }
                    expectKey[depth - 1] = 1;
                }
                break;
            case YAML_STREAM_END_EVENT:
                yaml_event_delete(&event);
                goto finalize_it;
            default:
                break;
        }
        PRAGMA_DIAGNOSTIC_POP
        yaml_event_delete(&event);
    }

finalize_it:
    if (bParserInitialized) {
        yaml_parser_delete(&yamlParser);
    }
    free(last_key);
    if (current_override != NULL) ratelimitFreePerSourceOverrideDirect(current_override);
    if (fh) fclose(fh);
    if (iRet == RS_RET_OK && policy->has_per_source && policy->per_source_enabled &&
        (!default_max_seen || !default_window_seen)) {
        LogError(0, RS_RET_CONF_PARAM_INVLD,
                 "ratelimit: policy file %s perSource section missing default.max or default.window", policy_file);
        iRet = RS_RET_CONF_PARAM_INVLD;
    }
    if (iRet != RS_RET_OK) {
        ratelimitPolicyFileDestruct(policy);
    }
#else
    LogError(0, RS_RET_CONF_PARAM_INVLD,
             "ratelimit: policy file '%s' specified but rsyslog "
             "was compiled without libyaml support. Policy settings ignored.",
             policy_file);
    iRet = RS_RET_CONF_PARAM_INVLD;
#endif
    RETiRet;
}

static void ratelimitFreePerSourceEntry(void *ptr) {
    ratelimit_ps_entry_t *entry = (ratelimit_ps_entry_t *)ptr;
    if (entry == NULL) return;
    free(entry);
}

static unsigned int ratelimitPerSourceShardCap(const unsigned int max_states, const unsigned int shard) {
    if (max_states == 0) return 0;
    if (max_states <= RATELIMIT_PERSOURCE_SHARDS) return 1;
    return (max_states / RATELIMIT_PERSOURCE_SHARDS) + (shard < (max_states % RATELIMIT_PERSOURCE_SHARDS));
}

static void ratelimitSetPerSourceShardCaps(ratelimit_shared_t *shared, unsigned int max_states) {
    if (shared == NULL) return;
    max_states = (max_states == 0) ? RATELIMIT_PERSOURCE_DEFAULT_MAX_STATES : max_states;
    shared->per_source_max_states = max_states;
    for (unsigned int i = 0; i < RATELIMIT_PERSOURCE_SHARDS; ++i) {
        if (shared->per_source_shards[i].lock_initialized) {
            pthread_mutex_lock(&shared->per_source_shards[i].mut);
        }
        shared->per_source_shards[i].max_states = ratelimitPerSourceShardCap(max_states, i);
        if (shared->per_source_shards[i].lock_initialized) {
            pthread_mutex_unlock(&shared->per_source_shards[i].mut);
        }
    }
}

static rsRetVal ratelimitInitPerSourceShards(ratelimit_shared_t *shared, unsigned int max_states) {
    DEFiRet;

    if (shared == NULL) ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    if (shared->per_source_shards_initialized) {
        ratelimitSetPerSourceShardCaps(shared, max_states);
        FINALIZE;
    }

    for (unsigned int i = 0; i < RATELIMIT_PERSOURCE_SHARDS; ++i) {
        if (pthread_mutex_init(&shared->per_source_shards[i].mut, NULL) != 0) {
            ABORT_FINALIZE(RS_RET_ERR);
        }
        shared->per_source_shards[i].lock_initialized = 1;
        shared->per_source_shards[i].ht =
            create_hashtable(16, hash_from_string, key_equals_string, ratelimitFreePerSourceEntry);
        if (shared->per_source_shards[i].ht == NULL) {
            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        }
    }
    shared->per_source_shards_initialized = 1;
    ratelimitSetPerSourceShardCaps(shared, max_states);

finalize_it:
    if (iRet != RS_RET_OK) {
        ratelimitDestroyPerSourceShards(shared);
    }
    RETiRet;
}

static void ratelimitDestroyPerSourceShards(ratelimit_shared_t *shared) {
    if (shared == NULL) return;
    for (unsigned int i = 0; i < RATELIMIT_PERSOURCE_SHARDS; ++i) {
        if (shared->per_source_shards[i].ht != NULL) {
            hashtable_destroy(shared->per_source_shards[i].ht, 1);
            shared->per_source_shards[i].ht = NULL;
        }
        if (shared->per_source_shards[i].lock_initialized) {
            pthread_mutex_destroy(&shared->per_source_shards[i].mut);
            shared->per_source_shards[i].lock_initialized = 0;
        }
        shared->per_source_shards[i].lru_head = NULL;
        shared->per_source_shards[i].lru_tail = NULL;
        shared->per_source_shards[i].active_states = 0;
        shared->per_source_shards[i].max_states = 0;
    }
    shared->per_source_shards_initialized = 0;
}

static ratelimit_ps_bucket_t *ratelimitPerSourceBucket(ratelimit_shared_t *shared,
                                                       const char *key,
                                                       unsigned int *hashvalue) {
    unsigned int local_hash;

    assert(shared != NULL);
    assert(key != NULL);
    assert(shared->per_source_shards[0].ht != NULL);
    local_hash = hashtable_hash(shared->per_source_shards[0].ht, (void *)key);
    if (hashvalue != NULL) {
        *hashvalue = local_hash;
    }
    return &shared->per_source_shards[local_hash % RATELIMIT_PERSOURCE_SHARDS];
}

static void ratelimitPerSourceLruRemove(ratelimit_ps_bucket_t *bucket, ratelimit_ps_entry_t *entry) {
    if (entry->lru_prev != NULL) entry->lru_prev->lru_next = entry->lru_next;
    if (entry->lru_next != NULL) entry->lru_next->lru_prev = entry->lru_prev;
    if (bucket->lru_head == entry) bucket->lru_head = entry->lru_next;
    if (bucket->lru_tail == entry) bucket->lru_tail = entry->lru_prev;
    entry->lru_prev = NULL;
    entry->lru_next = NULL;
}

static void ratelimitPerSourceLruTouch(ratelimit_ps_bucket_t *bucket, ratelimit_ps_entry_t *entry) {
    if (bucket->lru_tail == entry) return;
    if (entry->lru_prev != NULL || entry->lru_next != NULL || bucket->lru_head == entry) {
        ratelimitPerSourceLruRemove(bucket, entry);
    }
    entry->lru_prev = bucket->lru_tail;
    if (bucket->lru_tail != NULL) bucket->lru_tail->lru_next = entry;
    bucket->lru_tail = entry;
    if (bucket->lru_head == NULL) bucket->lru_head = entry;
}

static void ratelimitPerSourceEvictOne(ratelimit_shared_t *shared, ratelimit_ps_bucket_t *bucket) {
    ratelimit_ps_entry_t *evict = bucket->lru_head;
    ratelimit_ps_entry_t *removed;
    unsigned int hashvalue;

    if (evict == NULL) return;
    ratelimitPerSourceLruRemove(bucket, evict);
    if (bucket->active_states > 0) bucket->active_states--;
    evict->state_active = 0;
    STATSCOUNTER_INC(shared->ctrPerSourceEvicted, shared->mutCtrPerSourceEvicted);
    if (evict->has_override) return;

    hashvalue = hashtable_hash(bucket->ht, evict->key);
    removed = (ratelimit_ps_entry_t *)hashtable_remove_prehashed(bucket->ht, hashvalue, evict->key);
    ratelimitFreePerSourceEntry(removed);
}

#ifdef HAVE_LIBYAML
static void ratelimitFreePerSourceOverrideValue(void *ptr) {
    ratelimit_ps_override_t *ovr = (ratelimit_ps_override_t *)ptr;
    if (ovr == NULL) return;
    free(ovr);
}

static void ratelimitFreePerSourceOverrideDirect(void *ptr) {
    ratelimit_ps_override_t *ovr = (ratelimit_ps_override_t *)ptr;
    if (ovr == NULL) return;
    free(ovr->key);
    free(ovr);
}

static rsRetVal parseUnsignedInt(const char *value, unsigned int *out) {
    char *end = NULL;
    unsigned long val;
    DEFiRet;

    if (value == NULL || out == NULL) ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    while (isspace((unsigned char)*value)) value++;
    if (*value == '-') ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);

    errno = 0;
    val = strtoul(value, &end, 10);
    if (errno != 0 || end == value || val > UINT_MAX) {
        ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
    }
    if (*end != '\0') {
        ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
    }
    *out = (unsigned int)val;
finalize_it:
    RETiRet;
}
#endif /* HAVE_LIBYAML */

static rsRetVal parsePerSourcePolicyFile(const char *policy_file, ratelimit_ps_policy_t **policy_out) {
#ifndef HAVE_LIBYAML
    LogError(0, RS_RET_CONF_PARAM_INVLD,
             "ratelimit: per-source policy file '%s' specified but rsyslog "
             "was compiled without libyaml support.",
             policy_file);
    (void)policy_out;
    return RS_RET_CONF_PARAM_INVLD;
#else
    ratelimit_ps_policy_t *policy = NULL;
    ratelimit_ps_override_t *current_override = NULL;
    FILE *fh = NULL;
    yaml_parser_t yamlParser;
    yaml_event_t event;
    int parserInitialized = 0;
    unsigned int depth = 0;
    int expectKey[8] = {0};
    enum { CTX_TOP = 0, CTX_DEFAULT, CTX_OVERRIDE_ITEM } ctxStack[8];
    sbool inOverridesSeq = 0;
    char *last_key = NULL;
    sbool default_max_seen = 0;
    sbool default_window_seen = 0;
    unsigned int override_count = 0;
    const unsigned int MAX_OVERRIDES = 10000;
    DEFiRet;

    if (policy_out == NULL || policy_file == NULL) ABORT_FINALIZE(RS_RET_PARAM_ERROR);

    CHKmalloc(policy = calloc(1, sizeof(*policy)));
    CHKmalloc(policy->overrides =
                  create_hashtable(32, hash_from_string, key_equals_string, ratelimitFreePerSourceOverrideValue));

    fh = fopen(policy_file, "r");
    if (fh == NULL) {
        LogError(errno, RS_RET_FILE_NOT_FOUND, "ratelimit: error opening per-source policy file %s", policy_file);
        ABORT_FINALIZE(RS_RET_FILE_NOT_FOUND);
    }

    if (!yaml_parser_initialize(&yamlParser)) {
        LogError(0, RS_RET_INTERNAL_ERROR, "ratelimit: failed to initialize yaml parser");
        ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
    }
    parserInitialized = 1;
    yaml_parser_set_input_file(&yamlParser, fh);

    while (1) {
        if (!yaml_parser_parse(&yamlParser, &event)) {
            LogError(0, RS_RET_CONF_PARAM_INVLD, "ratelimit: yaml parser error in policy file %s: %s", policy_file,
                     yamlParser.problem ? yamlParser.problem : "unknown error");
            ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
        }

        switch (event.type) {
            case YAML_NO_EVENT:
            case YAML_STREAM_START_EVENT:
            case YAML_DOCUMENT_START_EVENT:
            case YAML_DOCUMENT_END_EVENT:
            case YAML_ALIAS_EVENT:
                break;
            case YAML_MAPPING_START_EVENT:
                if (depth >= 8) {
                    LogError(0, RS_RET_CONF_PARAM_INVLD, "ratelimit: yaml nesting too deep in %s", policy_file);
                    ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
                }
                if (depth == 0) {
                    ctxStack[depth] = CTX_TOP;
                } else if (inOverridesSeq) {
                    if (override_count >= MAX_OVERRIDES) {
                        LogError(0, RS_RET_CONF_PARAM_INVLD, "ratelimit: too many overrides in %s (max %u)",
                                 policy_file, MAX_OVERRIDES);
                        ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
                    }
                    ctxStack[depth] = CTX_OVERRIDE_ITEM;
                    CHKmalloc(current_override = calloc(1, sizeof(*current_override)));
                    override_count++;
                } else if (last_key != NULL && !strcmp(last_key, "default")) {
                    ctxStack[depth] = CTX_DEFAULT;
                } else {
                    ctxStack[depth] = CTX_TOP;
                }
                if (depth > 0 && last_key != NULL) {
                    expectKey[depth - 1] = 1;
                }
                expectKey[depth] = 1;
                depth++;
                free(last_key);
                last_key = NULL;
                break;
            case YAML_MAPPING_END_EVENT:
                if (depth > 0) {
                    depth--;
                    if (ctxStack[depth] == CTX_OVERRIDE_ITEM && current_override != NULL) {
                        if (current_override->key == NULL) {
                            LogError(0, RS_RET_CONF_PARAM_INVLD, "ratelimit: per-source override missing key in %s",
                                     policy_file);
                            ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
                        }
                        if (hashtable_insert(policy->overrides, current_override->key, current_override) == 0) {
                            LogError(0, RS_RET_OUT_OF_MEMORY, "ratelimit: error inserting per-source override '%s'",
                                     current_override->key);
                            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
                        }
                        current_override = NULL;
                    }
                }
                free(last_key);
                last_key = NULL;
                break;
            case YAML_SEQUENCE_START_EVENT:
                if (last_key != NULL && !strcmp(last_key, "overrides")) {
                    inOverridesSeq = 1;
                }
                if (depth > 0 && last_key != NULL) {
                    expectKey[depth - 1] = 1;
                }
                free(last_key);
                last_key = NULL;
                break;
            case YAML_SEQUENCE_END_EVENT:
                inOverridesSeq = 0;
                free(last_key);
                last_key = NULL;
                break;
            case YAML_SCALAR_EVENT:
                if (depth == 0) break;
                if (expectKey[depth - 1]) {
                    free(last_key);
                    last_key = strdup((char *)event.data.scalar.value);
                    expectKey[depth - 1] = 0;
                } else {
                    const char *val = (const char *)event.data.scalar.value;
                    if (ctxStack[depth - 1] == CTX_DEFAULT) {
                        if (last_key != NULL && !strcmp(last_key, "max")) {
                            unsigned int max = 0;
                            if (parseUnsignedInt(val, &max) != RS_RET_OK) {
                                LogError(0, RS_RET_CONF_PARAM_INVLD, "ratelimit: invalid default.max value '%s' in %s",
                                         val, policy_file);
                                ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
                            }
                            policy->default_max = max;
                            default_max_seen = 1;
                        } else if (last_key != NULL && !strcmp(last_key, "window")) {
                            unsigned int window = 0;
                            if (parseDurationSeconds(val, &window) != RS_RET_OK) {
                                LogError(0, RS_RET_CONF_PARAM_INVLD,
                                         "ratelimit: invalid default.window value '%s' in %s", val, policy_file);
                                ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
                            }
                            policy->default_window = window;
                            default_window_seen = 1;
                        }
                    } else if (ctxStack[depth - 1] == CTX_OVERRIDE_ITEM && current_override != NULL) {
                        if (last_key != NULL && !strcmp(last_key, "key")) {
                            free(current_override->key);
                            current_override->key = strdup(val);
                        } else if (last_key != NULL && !strcmp(last_key, "max")) {
                            unsigned int max = 0;
                            if (parseUnsignedInt(val, &max) != RS_RET_OK) {
                                LogError(0, RS_RET_CONF_PARAM_INVLD, "ratelimit: invalid override.max value '%s' in %s",
                                         val, policy_file);
                                ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
                            }
                            current_override->max = max;
                            current_override->has_max = 1;
                        } else if (last_key != NULL && !strcmp(last_key, "window")) {
                            unsigned int window = 0;
                            if (parseDurationSeconds(val, &window) != RS_RET_OK) {
                                LogError(0, RS_RET_CONF_PARAM_INVLD,
                                         "ratelimit: invalid override.window value '%s' in %s", val, policy_file);
                                ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
                            }
                            current_override->window = window;
                            current_override->has_window = 1;
                        }
                    }
                    expectKey[depth - 1] = 1;
                }
                break;
            case YAML_STREAM_END_EVENT:
                yaml_event_delete(&event);
                goto finalize_it;
            default:
                break;
        }
        yaml_event_delete(&event);
    }

finalize_it:
    if (parserInitialized) yaml_parser_delete(&yamlParser);
    if (fh) fclose(fh);
    free(last_key);
    if (iRet != RS_RET_OK) {
        if (current_override != NULL) ratelimitFreePerSourceOverrideDirect(current_override);
        if (policy != NULL) {
            if (policy->overrides != NULL) hashtable_destroy(policy->overrides, 1);
            free(policy);
        }
        return iRet;
    }
    if (!default_max_seen || !default_window_seen) {
        LogError(0, RS_RET_CONF_PARAM_INVLD,
                 "ratelimit: per-source policy file %s missing default.max or default.window", policy_file);
        if (policy != NULL) {
            if (policy->overrides != NULL) hashtable_destroy(policy->overrides, 1);
            free(policy);
        }
        return RS_RET_CONF_PARAM_INVLD;
    }
    *policy_out = policy;
    return RS_RET_OK;
#endif
}

static char *ratelimitSanitizeMetricName(const char *key) {
    size_t len = strlen(key);
    size_t maxLen = 128;
    size_t outLen = len > maxLen ? maxLen : len;
    char *out = malloc(outLen + 1);
    if (out == NULL) return NULL;
    for (size_t i = 0; i < outLen; ++i) {
        char c = key[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-') {
            out[i] = c;
        } else {
            out[i] = '_';
        }
    }
    out[outLen] = '\0';
    return out;
}

static void ratelimitPerSourceTopNAdd(
    uint64_t *values, char **keys, unsigned int topn, const char *key, uint64_t dropped) {
    if (dropped == 0) return;
    for (unsigned int i = 0; i < topn; ++i) {
        if (dropped > values[i]) {
            char *copy = strdup(key);
            if (copy == NULL) return;
            free(keys[topn - 1]);
            for (unsigned int j = topn - 1; j > i; --j) {
                values[j] = values[j - 1];
                keys[j] = keys[j - 1];
            }
            values[i] = dropped;
            keys[i] = copy;
            break;
        }
    }
}

static void ratelimitPerSourceUpdateTopN(ratelimit_shared_t *shared) {
    if (shared == NULL || shared->per_source_stats == NULL) return;

    pthread_mutex_lock(&shared->per_source_policy_mut);
    if (shared->per_source_topn == 0) {
        pthread_mutex_unlock(&shared->per_source_policy_mut);
        return;
    }

    unsigned int topn = shared->per_source_topn;
    pthread_mutex_unlock(&shared->per_source_policy_mut);

    uint64_t *values = calloc(topn, sizeof(uint64_t));
    char **keys = calloc(topn, sizeof(char *));
    if (values == NULL || keys == NULL) {
        free(values);
        free(keys);
        return;
    }

    for (unsigned int shard = 0; shard < RATELIMIT_PERSOURCE_SHARDS; ++shard) {
        ratelimit_ps_bucket_t *const bucket = &shared->per_source_shards[shard];
        struct hashtable_itr *itr = NULL;
        if (bucket->ht == NULL) continue;
        pthread_mutex_lock(&bucket->mut);
        if (hashtable_count(bucket->ht) > 0) {
            itr = hashtable_iterator(bucket->ht);
        }
        if (itr != NULL) {
            do {
                ratelimit_ps_entry_t *entry = (ratelimit_ps_entry_t *)hashtable_iterator_value(itr);
                if (entry != NULL && entry->state_active) {
                    ratelimitPerSourceTopNAdd(values, keys, topn, entry->key, entry->dropped);
                }
            } while (hashtable_iterator_advance(itr));
            free(itr);
        }
        pthread_mutex_unlock(&bucket->mut);
    }

    pthread_mutex_lock(&shared->per_source_policy_mut);
    if (shared->per_source_topn != topn) {
        pthread_mutex_unlock(&shared->per_source_policy_mut);
        for (unsigned int i = 0; i < topn; ++i) free(keys[i]);
        free(values);
        free(keys);
        return;
    }
    if (shared->per_source_top_ctrs == NULL) {
        shared->per_source_top_ctrs = calloc(topn, sizeof(ctr_t *));
        shared->per_source_top_values = calloc(topn, sizeof(intctr_t));
        shared->per_source_top_keys = calloc(topn, sizeof(char *));
        if (shared->per_source_top_ctrs == NULL || shared->per_source_top_values == NULL ||
            shared->per_source_top_keys == NULL) {
            free(shared->per_source_top_ctrs);
            free(shared->per_source_top_values);
            free(shared->per_source_top_keys);
            shared->per_source_top_ctrs = NULL;
            shared->per_source_top_values = NULL;
            shared->per_source_top_keys = NULL;
            free(values);
            for (unsigned int i = 0; i < topn; ++i) free(keys[i]);
            free(keys);
            pthread_mutex_unlock(&shared->per_source_policy_mut);
            return;
        }
    }

    sbool rebuild = 0;
    for (unsigned int i = 0; i < topn; ++i) {
        const char *old_key = shared->per_source_top_keys[i];
        const char *new_key = keys[i];
        if ((old_key == NULL && new_key != NULL) || (old_key != NULL && new_key == NULL) ||
            (old_key != NULL && new_key != NULL && strcmp(old_key, new_key) != 0)) {
            rebuild = 1;
            break;
        }
    }

    if (rebuild) {
        for (unsigned int i = 0; i < topn; ++i) {
            if (shared->per_source_top_ctrs[i] != NULL) {
                statsobj.DestructCounter(shared->per_source_stats, shared->per_source_top_ctrs[i]);
                shared->per_source_top_ctrs[i] = NULL;
            }
            free(shared->per_source_top_keys[i]);
            shared->per_source_top_keys[i] = NULL;
        }
        for (unsigned int i = 0; i < topn; ++i) {
            if (keys[i] == NULL) continue;
            char *sanitized = ratelimitSanitizeMetricName(keys[i]);
            if (sanitized == NULL) continue;
            char namebuf[256];
            snprintf(namebuf, sizeof(namebuf), "per_source_drop_%u_%s", i + 1, sanitized);
            free(sanitized);
            shared->per_source_top_values[i] = values[i];
            if (statsobj.AddManagedCounter(shared->per_source_stats, (uchar *)namebuf, ctrType_IntCtr, CTR_FLAG_NONE,
                                           &shared->per_source_top_values[i], &shared->per_source_top_ctrs[i],
                                           1) == RS_RET_OK) {
                shared->per_source_top_keys[i] = strdup(keys[i]);
            }
        }
    } else {
        for (unsigned int i = 0; i < topn; ++i) {
            shared->per_source_top_values[i] = values[i];
        }
    }

    pthread_mutex_unlock(&shared->per_source_policy_mut);
    free(values);
    for (unsigned int i = 0; i < topn; ++i) free(keys[i]);
    free(keys);
}

static void ratelimitPerSourceStatsRead(statsobj_t *stats, void *ctx) {
    ratelimit_shared_t *shared = (ratelimit_shared_t *)ctx;
    (void)stats;
    ratelimitPerSourceUpdateTopN(shared);
}

static rsRetVal ratelimitPerSourceCreateEntry(ratelimit_ps_bucket_t *bucket,
                                              const unsigned int hashvalue,
                                              const char *key,
                                              ratelimit_ps_entry_t **entry_out) {
    char *owned_key = NULL;
    ratelimit_ps_entry_t *entry = NULL;
    DEFiRet;

    if (bucket == NULL || key == NULL || entry_out == NULL) ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    CHKmalloc(entry = calloc(1, sizeof(*entry)));
    CHKmalloc(owned_key = strdup(key));
    entry->key = owned_key;
    if (hashtable_insert_prehashed(bucket->ht, hashvalue, owned_key, entry) == 0) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }
    owned_key = NULL;
    *entry_out = entry;
    entry = NULL;

finalize_it:
    free(owned_key);
    free(entry);
    RETiRet;
}

static rsRetVal ratelimitPerSourceApplyOverrides(ratelimit_shared_t *shared, struct hashtable *overrides) {
    DEFiRet;

    if (shared == NULL) ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    if (!shared->per_source_shards_initialized) FINALIZE;

    if (overrides != NULL && hashtable_count(overrides) > 0) {
        struct hashtable_itr *itr = hashtable_iterator(overrides);
        if (itr != NULL) {
            do {
                ratelimit_ps_override_t *override = (ratelimit_ps_override_t *)hashtable_iterator_value(itr);
                unsigned int hashvalue;
                ratelimit_ps_bucket_t *bucket;
                ratelimit_ps_entry_t *entry;

                if (override == NULL || override->key == NULL) continue;
                bucket = ratelimitPerSourceBucket(shared, override->key, &hashvalue);
                pthread_mutex_lock(&bucket->mut);
                entry = (ratelimit_ps_entry_t *)hashtable_search_prehashed(bucket->ht, hashvalue, override->key);
                if (entry == NULL) {
                    iRet = ratelimitPerSourceCreateEntry(bucket, hashvalue, override->key, &entry);
                }
                pthread_mutex_unlock(&bucket->mut);
                if (iRet != RS_RET_OK) {
                    free(itr);
                    FINALIZE;
                }
            } while (hashtable_iterator_advance(itr));
            free(itr);
        }
    }

    for (unsigned int shard = 0; shard < RATELIMIT_PERSOURCE_SHARDS; ++shard) {
        ratelimit_ps_bucket_t *const bucket = &shared->per_source_shards[shard];
        struct hashtable_itr *itr;
        int more;

        pthread_mutex_lock(&bucket->mut);
        itr = hashtable_iterator(bucket->ht);
        if (itr != NULL) {
            more = (hashtable_count(bucket->ht) > 0);
            while (more) {
                ratelimit_ps_entry_t *entry = (ratelimit_ps_entry_t *)hashtable_iterator_value(itr);
                if (entry != NULL) {
                    entry->has_override = 0;
                    entry->has_max = 0;
                    entry->has_window = 0;
                    entry->max = 0;
                    entry->window = 0;
                }
                more = hashtable_iterator_advance(itr);
            }
            free(itr);
        }

        if (overrides != NULL && hashtable_count(overrides) > 0) {
            itr = hashtable_iterator(overrides);
            if (itr != NULL) {
                do {
                    ratelimit_ps_override_t *override = (ratelimit_ps_override_t *)hashtable_iterator_value(itr);
                    unsigned int hashvalue;
                    ratelimit_ps_bucket_t *override_bucket;
                    ratelimit_ps_entry_t *entry;

                    if (override == NULL || override->key == NULL) continue;
                    override_bucket = ratelimitPerSourceBucket(shared, override->key, &hashvalue);
                    if (override_bucket != bucket) continue;
                    entry = (ratelimit_ps_entry_t *)hashtable_search_prehashed(bucket->ht, hashvalue, override->key);
                    if (entry == NULL) continue;
                    entry->has_override = 1;
                    entry->has_max = override->has_max;
                    entry->max = override->max;
                    entry->has_window = override->has_window;
                    entry->window = override->window;
                } while (hashtable_iterator_advance(itr));
                free(itr);
            }
        }

        itr = hashtable_iterator(bucket->ht);
        if (itr != NULL) {
            more = (hashtable_count(bucket->ht) > 0);
            while (more) {
                ratelimit_ps_entry_t *entry = (ratelimit_ps_entry_t *)hashtable_iterator_value(itr);
                if (entry != NULL && !entry->state_active && !entry->has_override) {
                    more = hashtable_iterator_remove(itr);
                    ratelimitFreePerSourceEntry(entry);
                } else {
                    more = hashtable_iterator_advance(itr);
                }
            }
            free(itr);
        }
        pthread_mutex_unlock(&bucket->mut);
    }

finalize_it:
    RETiRet;
}

static rsRetVal ratelimitInitPerSourceShared(ratelimit_shared_t *shared,
                                             ratelimit_ps_policy_t *policy,
                                             const char *policy_file,
                                             unsigned int max_states,
                                             unsigned int topn) {
    char statname[256];
    sbool bLocked = 0;
    DEFiRet;

    if (shared == NULL || policy == NULL || policy_file == NULL) ABORT_FINALIZE(RS_RET_PARAM_ERROR);

    ratelimitPerSourceStoreEnabled(&shared->per_source_enabled, &shared->per_source_policy_mut, 0);
    pthread_mutex_lock(&shared->per_source_policy_mut);
    bLocked = 1;
    shared->per_source_policy_file = strdup(policy_file);
    if (shared->per_source_policy_file == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    ratelimitSharedStoreUInt(&shared->per_source_default_max, &shared->mut, policy->default_max);
    ratelimitSharedStoreUInt(&shared->per_source_default_window, &shared->mut, policy->default_window);
    shared->per_source_key_tpl_name = NULL;
    shared->per_source_topn = (topn == 0) ? RATELIMIT_PERSOURCE_DEFAULT_TOPN : topn;
    pthread_mutex_unlock(&shared->per_source_policy_mut);
    bLocked = 0;
    CHKiRet(ratelimitInitPerSourceShards(shared, max_states));
    CHKiRet(ratelimitPerSourceApplyOverrides(shared, policy->overrides));
    if (policy->overrides != NULL) {
        hashtable_destroy(policy->overrides, 1);
        policy->overrides = NULL;
    }

    STATSCOUNTER_INIT(shared->ctrPerSourceAllowed, shared->mutCtrPerSourceAllowed);
    STATSCOUNTER_INIT(shared->ctrPerSourceDropped, shared->mutCtrPerSourceDropped);
    STATSCOUNTER_INIT(shared->ctrPerSourceEvicted, shared->mutCtrPerSourceEvicted);
    STATSCOUNTER_INIT(shared->ctrPerSourceKeyTplEvals, shared->mutCtrPerSourceKeyTplEvals);
    STATSCOUNTER_INIT(shared->ctrPerSourceKeyParseEvals, shared->mutCtrPerSourceKeyParseEvals);

    CHKiRet(statsobj.Construct(&shared->per_source_stats));
    snprintf(statname, sizeof(statname), "ratelimit.%s.per_source", shared->name);
    statname[sizeof(statname) - 1] = '\0';
    CHKiRet(statsobj.SetName(shared->per_source_stats, (uchar *)statname));
    CHKiRet(statsobj.SetOrigin(shared->per_source_stats, (uchar *)"ratelimit"));
    CHKiRet(statsobj.AddCounter(shared->per_source_stats, UCHAR_CONSTANT("per_source_allowed"), ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &shared->ctrPerSourceAllowed));
    CHKiRet(statsobj.AddCounter(shared->per_source_stats, UCHAR_CONSTANT("per_source_dropped"), ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &shared->ctrPerSourceDropped));
    CHKiRet(statsobj.AddCounter(shared->per_source_stats, UCHAR_CONSTANT("per_source_evicted"), ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &shared->ctrPerSourceEvicted));
    CHKiRet(statsobj.AddCounter(shared->per_source_stats, UCHAR_CONSTANT("per_source_key_tpl_evals"), ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &shared->ctrPerSourceKeyTplEvals));
    CHKiRet(statsobj.AddCounter(shared->per_source_stats, UCHAR_CONSTANT("per_source_key_parse_evals"), ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &shared->ctrPerSourceKeyParseEvals));
    CHKiRet(statsobj.SetReadNotifier(shared->per_source_stats, ratelimitPerSourceStatsRead, shared));
    CHKiRet(statsobj.ConstructFinalize(shared->per_source_stats));

finalize_it:
    if (iRet != RS_RET_OK) {
        if (bLocked) {
            pthread_mutex_unlock(&shared->per_source_policy_mut);
        }
        if (shared->per_source_stats != NULL) statsobj.Destruct(&shared->per_source_stats);
        ratelimitDestroyPerSourceShards(shared);
        free(shared->per_source_policy_file);
        shared->per_source_policy_file = NULL;
        ratelimitPerSourceStoreEnabled(&shared->per_source_enabled, &shared->per_source_policy_mut, 0);
    }
    return iRet;
}

static rsRetVal ratelimitSwapPerSourcePolicy(ratelimit_shared_t *shared, ratelimit_ps_policy_t *policy) {
    DEFiRet;

    if (shared == NULL || policy == NULL) FINALIZE;
    CHKiRet(ratelimitPerSourceApplyOverrides(shared, policy->overrides));
    ratelimitSharedStoreUInt(&shared->per_source_default_max, &shared->mut, policy->default_max);
    ratelimitSharedStoreUInt(&shared->per_source_default_window, &shared->mut, policy->default_window);
    if (policy->overrides != NULL) {
        hashtable_destroy(policy->overrides, 1);
        policy->overrides = NULL;
    }

finalize_it:
    RETiRet;
}

static void ratelimitResetPerSourceTopN(ratelimit_shared_t *shared) {
    ctr_t **top_ctrs = NULL;
    intctr_t *top_values = NULL;
    char **top_keys = NULL;
    unsigned int topn;

    if (shared == NULL) return;

    pthread_mutex_lock(&shared->per_source_policy_mut);
    if (shared->per_source_top_ctrs == NULL) {
        pthread_mutex_unlock(&shared->per_source_policy_mut);
        return;
    }
    topn = shared->per_source_topn;
    top_ctrs = shared->per_source_top_ctrs;
    top_values = shared->per_source_top_values;
    top_keys = shared->per_source_top_keys;
    shared->per_source_top_ctrs = NULL;
    shared->per_source_top_values = NULL;
    shared->per_source_top_keys = NULL;
    pthread_mutex_unlock(&shared->per_source_policy_mut);

    for (unsigned int i = 0; i < topn; ++i) {
        if (top_ctrs[i] != NULL && shared->per_source_stats != NULL) {
            statsobj.DestructCounter(shared->per_source_stats, top_ctrs[i]);
        }
        free(top_keys[i]);
    }
    free(top_ctrs);
    free(top_values);
    free(top_keys);
}

static rsRetVal ratelimitApplyPolicyPerSource(ratelimit_shared_t *shared,
                                              ratelimit_policy_file_t *policy,
                                              rsconf_t *conf,
                                              const char *policy_file) {
    struct template *key_tpl = NULL;
    sbool key_tpl_default = 0;
    char *key_tpl_name = NULL;
    char *new_policy_file = NULL;
    unsigned int effective_topn;
    sbool have_per_source_policy_file;
    sbool have_per_source_shards;
    DEFiRet;

    if (shared == NULL || policy == NULL || !policy->has_per_source) FINALIZE;

    if (!policy->per_source_enabled) {
        ratelimitPerSourceStoreEnabled(&shared->per_source_enabled, &shared->per_source_policy_mut, 0);
        pthread_mutex_lock(&shared->per_source_policy_mut);
        shared->per_source_policy_from_policy_file = 1;
        pthread_mutex_unlock(&shared->per_source_policy_mut);
        FINALIZE;
    }

    CHKiRet(ratelimitLookupPerSourceTemplate(conf, policy->per_source_key_tpl_name, &key_tpl, &key_tpl_default));
    if (policy->per_source_key_tpl_name != NULL) {
        CHKmalloc(key_tpl_name = strdup(policy->per_source_key_tpl_name));
    }

    pthread_mutex_lock(&shared->per_source_policy_mut);
    have_per_source_shards = shared->per_source_shards_initialized;
    pthread_mutex_unlock(&shared->per_source_policy_mut);
    if (!have_per_source_shards) {
        CHKiRet(ratelimitInitPerSourceShared(shared, policy->per_source_policy, policy_file,
                                             policy->per_source_max_states, policy->per_source_topn));
    } else {
        CHKiRet(ratelimitSwapPerSourcePolicy(shared, policy->per_source_policy));
    }

    if (policy_file != NULL) {
        pthread_mutex_lock(&shared->per_source_policy_mut);
        have_per_source_policy_file = (shared->per_source_policy_file != NULL);
        pthread_mutex_unlock(&shared->per_source_policy_mut);
        if (!have_per_source_policy_file) {
            CHKmalloc(new_policy_file = strdup(policy_file));
        }
    }
    pthread_mutex_lock(&shared->per_source_policy_mut);
    if (shared->per_source_policy_file == NULL && new_policy_file != NULL) {
        shared->per_source_policy_file = new_policy_file;
        new_policy_file = NULL;
    }
    free(shared->per_source_key_tpl_name);
    shared->per_source_key_tpl_name = key_tpl_name;
    key_tpl_name = NULL;
    shared->per_source_policy_from_policy_file = 1;
    ratelimitSetPerSourceShardCaps(shared, policy->per_source_max_states);
    effective_topn = (policy->per_source_topn == 0) ? RATELIMIT_PERSOURCE_DEFAULT_TOPN : policy->per_source_topn;
    if (shared->per_source_topn != effective_topn) {
        pthread_mutex_unlock(&shared->per_source_policy_mut);
        ratelimitResetPerSourceTopN(shared);
        pthread_mutex_lock(&shared->per_source_policy_mut);
        shared->per_source_topn = effective_topn;
    }
    ratelimitPublishPerSourceKeyPolicy(shared, key_tpl, key_tpl_default);
    pthread_mutex_unlock(&shared->per_source_policy_mut);
    ratelimitPerSourceStoreEnabled(&shared->per_source_enabled, &shared->per_source_policy_mut, 1);

finalize_it:
    free(new_policy_file);
    free(key_tpl_name);
    RETiRet;
}

static rsRetVal ratelimitReloadPolicyFile(ratelimit_shared_t *shared, const char *trigger) {
    unsigned int interval;
    unsigned int burst;
    int severity;
    ratelimit_scope_t scope;
    ratelimit_output_mode_t output_mode;
    ratelimit_policy_file_t policy = {0};
    DEFiRet;

    if (shared == NULL || shared->policy_file == NULL) {
        FINALIZE;
    }

    interval = ratelimitSharedLoadUInt(&shared->interval, &shared->mut);
    burst = ratelimitSharedLoadUInt(&shared->burst, &shared->mut);
    severity = ratelimitSharedLoadSeverity(&shared->severity, &shared->mut);
    pthread_mutex_lock(&shared->mut);
    output_mode = shared->output_mode;
    pthread_mutex_unlock(&shared->mut);

    CHKiRet(parsePolicyFile(shared->policy_file, &policy));
    scope = policy.has_scope ? policy.scope : RATELIMIT_SCOPE_INPUT;
    if (scope != shared->scope) {
        LogError(0, RS_RET_CONFIG_ERROR,
                 "ratelimit: %s reload of policy '%s' from file '%s' rejected because scope changed", trigger,
                 shared->name, shared->policy_file);
        ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
    }
    if (shared->scope == RATELIMIT_SCOPE_OUTPUT) {
        if (!policy.has_output_mode) {
            LogError(0, RS_RET_CONFIG_ERROR,
                     "ratelimit: %s reload of output policy '%s' from file '%s' rejected because mode is missing",
                     trigger, shared->name, shared->policy_file);
            ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
        }
        if (!policy.has_interval || !policy.has_burst) {
            LogError(0, RS_RET_CONFIG_ERROR,
                     "ratelimit: %s reload of output policy '%s' from file '%s' rejected because interval or burst "
                     "is missing",
                     trigger, shared->name, shared->policy_file);
            ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
        }
        if (policy.has_severity || policy.has_per_source) {
            LogError(0, RS_RET_CONFIG_ERROR,
                     "ratelimit: %s reload of output policy '%s' from file '%s' rejected due to unsupported fields",
                     trigger, shared->name, shared->policy_file);
            ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
        }
        output_mode = policy.output_mode;
    } else if (policy.has_output_mode) {
        LogError(0, RS_RET_CONFIG_ERROR,
                 "ratelimit: %s reload of input policy '%s' from file '%s' rejected because output mode is set",
                 trigger, shared->name, shared->policy_file);
        ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
    }
    if (policy.has_interval) interval = policy.interval;
    if (policy.has_burst) burst = policy.burst;
    if (policy.has_severity) severity = policy.severity;
    if (shared->scope == RATELIMIT_SCOPE_OUTPUT && output_mode == RATELIMIT_OUTPUT_MODE_PACE) {
        sbool output_pace_forbidden;
        if (burst == 0) {
            LogError(0, RS_RET_CONFIG_ERROR,
                     "ratelimit: %s reload of output policy '%s' from file '%s' rejected because pace mode requires "
                     "burst greater than zero",
                     trigger, shared->name, shared->policy_file);
            ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
        }
        pthread_mutex_lock(&shared->mut);
        output_pace_forbidden = shared->output_pace_forbidden;
        pthread_mutex_unlock(&shared->mut);
        if (output_pace_forbidden) {
            LogError(0, RS_RET_CONFIG_ERROR,
                     "ratelimit: %s reload of output policy '%s' from file '%s' rejected because a direct action uses "
                     "this policy",
                     trigger, shared->name, shared->policy_file);
            ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
        }
    }
    if (policy.has_per_source) {
        CHKiRet(ratelimitApplyPolicyPerSource(shared, &policy, runConf, shared->policy_file));
    } else {
        pthread_mutex_lock(&shared->per_source_policy_mut);
        const sbool disable_per_source = shared->per_source_policy_from_policy_file;
        pthread_mutex_unlock(&shared->per_source_policy_mut);
        if (disable_per_source) {
            ratelimitPerSourceStoreEnabled(&shared->per_source_enabled, &shared->per_source_policy_mut, 0);
        }
    }

    ratelimitSharedStoreUInt(&shared->interval, &shared->mut, interval);
    ratelimitSharedStoreUInt(&shared->burst, &shared->mut, burst);
    ratelimitSharedStoreSeverity(&shared->severity, &shared->mut, severity);
    pthread_mutex_lock(&shared->mut);
    shared->output_mode = output_mode;
    pthread_mutex_unlock(&shared->mut);

    LogMsg(0, RS_RET_OK, LOG_INFO, "ratelimit: %s reloaded policy '%s' from file '%s'", trigger, shared->name,
           shared->policy_file);

finalize_it:
    ratelimitPolicyFileDestruct(&policy);
    if (iRet != RS_RET_OK && shared != NULL && shared->policy_file != NULL) {
        LogError(0, iRet, "ratelimit: %s failed to reload policy '%s' from file '%s', keeping old values", trigger,
                 shared->name, shared->policy_file);
    }
    RETiRet;
}

static rsRetVal ratelimitReloadPerSourcePolicyFile(ratelimit_shared_t *shared, const char *trigger) {
    ratelimit_ps_policy_t *policy = NULL;
    DEFiRet;

    if (shared == NULL || shared->per_source_policy_file == NULL) {
        FINALIZE;
    }

    CHKiRet(parsePerSourcePolicyFile(shared->per_source_policy_file, &policy));
    if (policy != NULL) {
        CHKiRet(ratelimitSwapPerSourcePolicy(shared, policy));
        if (policy->overrides != NULL) {
            policy->overrides = NULL;
        }
        free(policy);
        policy = NULL;
    }

    LogMsg(0, RS_RET_OK, LOG_INFO, "ratelimit: %s reloaded per-source policy '%s' from file '%s'", trigger,
           shared->name, shared->per_source_policy_file);

finalize_it:
    if (policy != NULL) {
        if (policy->overrides != NULL) {
            hashtable_destroy(policy->overrides, 1);
        }
        free(policy);
    }
    if (iRet != RS_RET_OK && shared != NULL && shared->per_source_policy_file != NULL) {
        LogError(0, iRet, "ratelimit: %s failed to reload per-source policy '%s' from file '%s', keeping old values",
                 trigger, shared->name, shared->per_source_policy_file);
    }
    RETiRet;
}

static rsRetVal ratelimitPerSourceCheck(ratelimit_t *ratelimit, const char *key, size_t key_len, time_t tt) {
    ratelimit_shared_t *shared;
    ratelimit_ps_bucket_t *bucket;
    ratelimit_ps_entry_t *entry;
    unsigned int hashvalue;
    unsigned int max;
    unsigned int window;
    sbool must_drop = 0;
    DEFiRet;

    if (ratelimit == NULL || ratelimit->pShared == NULL || key == NULL || key_len == 0) {
        FINALIZE;
    }
    shared = ratelimit->pShared;

    if (tt == 0) tt = time(NULL);

    if (!ratelimitPerSourceLoadEnabled(&shared->per_source_enabled, &shared->per_source_policy_mut)) FINALIZE;
    bucket = ratelimitPerSourceBucket(shared, key, &hashvalue);
    pthread_mutex_lock(&bucket->mut);
    entry = (ratelimit_ps_entry_t *)hashtable_search_prehashed(bucket->ht, hashvalue, (void *)key);
    if (entry == NULL) {
        iRet = ratelimitPerSourceCreateEntry(bucket, hashvalue, key, &entry);
        if (iRet != RS_RET_OK) {
            pthread_mutex_unlock(&bucket->mut);
            FINALIZE;
        }
    }
    if (!entry->state_active) {
        while (bucket->max_states > 0 && bucket->active_states >= bucket->max_states) {
            if (bucket->lru_head == NULL) break;
            ratelimitPerSourceEvictOne(shared, bucket);
        }
        entry->state_active = 1;
        entry->window_start = tt;
        entry->last_seen = tt;
        bucket->active_states++;
    }
    ratelimitPerSourceLruTouch(bucket, entry);

    max = (entry->has_override && entry->has_max)
              ? entry->max
              : ratelimitSharedLoadUInt(&shared->per_source_default_max, &shared->mut);
    window = (entry->has_override && entry->has_window)
                 ? entry->window
                 : ratelimitSharedLoadUInt(&shared->per_source_default_window, &shared->mut);

    if (max == 0 || window == 0) {
        entry->allowed++;
    } else {
        if (entry->window_start == 0 || (tt - entry->window_start) >= (time_t)window || tt < entry->window_start) {
            entry->window_start = tt;
            entry->count = 0;
        }
        if (entry->count < max) {
            entry->count++;
            entry->allowed++;
        } else {
            entry->dropped++;
            must_drop = 1;
        }
    }
    entry->last_seen = tt;
    if (must_drop) {
        STATSCOUNTER_INC(shared->ctrPerSourceDropped, shared->mutCtrPerSourceDropped);
    } else {
        STATSCOUNTER_INC(shared->ctrPerSourceAllowed, shared->mutCtrPerSourceAllowed);
    }
    pthread_mutex_unlock(&bucket->mut);

    if (must_drop) {
        ABORT_FINALIZE(RS_RET_DISCARDMSG);
    }

finalize_it:
    RETiRet;
}

rsRetVal ratelimitAddConfig(rsconf_t *conf,
                            const char *name,
                            unsigned int interval,
                            unsigned int burst,
                            int severity,
                            const char *policy_file,
                            sbool policy_watch,
                            const char *policy_watch_debounce,
                            sbool per_source_enabled,
                            const char *per_source_policy_file,
                            const char *per_source_key_tpl_name,
                            unsigned int per_source_max_states,
                            unsigned int per_source_topn,
                            sbool has_inline_policy_params,
                            sbool has_legacy_per_source_params) {
    DEFiRet;
    ratelimit_shared_t *shared = NULL;
    ratelimit_shared_t *existing_shared = NULL;
    struct template *per_source_key_tpl = NULL;
    sbool per_source_key_tpl_default = 0;
    char *key = NULL;
    ratelimit_ps_policy_t *per_source_policy = NULL;
    ratelimit_policy_file_t file_policy = {0};
    unsigned int debounce_ms = RATELIMIT_POLICY_WATCH_DEBOUNCE_DFLT_MS;
    sbool per_source_from_policy_file = 0;
    sbool bLocked = 0;
    ratelimit_cfg_bucket_t *bucket = NULL;
    unsigned int hashvalue = 0;
    ratelimit_scope_t scope = RATELIMIT_SCOPE_INPUT;
    ratelimit_output_mode_t output_mode = RATELIMIT_OUTPUT_MODE_DROP;


    if (name == NULL) {
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    if (policy_watch_debounce != NULL) {
        CHKiRet(parseDurationMillis(policy_watch_debounce, &debounce_ms));
    }

    if (policy_file != NULL) {
        CHKiRet(parsePolicyFile(policy_file, &file_policy));
        if (file_policy.has_scope) scope = file_policy.scope;
        if (file_policy.has_output_mode) output_mode = file_policy.output_mode;
        if (file_policy.has_interval) interval = file_policy.interval;
        if (file_policy.has_burst) burst = file_policy.burst;
        if (file_policy.has_severity) severity = file_policy.severity;
        if (file_policy.has_per_source) {
            if (per_source_enabled || per_source_policy_file != NULL || per_source_key_tpl_name != NULL ||
                per_source_max_states != 0 || per_source_topn != 0) {
                LogError(0, RS_RET_CONFIG_ERROR,
                         "ratelimit: policy file '%s' contains perSource for '%s'; do not mix it with legacy "
                         "perSource/perSourcePolicy/perSourceKeyTpl/maxStates/topN parameters",
                         policy_file, name);
                ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
            }
            per_source_from_policy_file = 1;
            per_source_enabled = file_policy.per_source_enabled;
            if (per_source_enabled) {
                per_source_policy = file_policy.per_source_policy;
                file_policy.per_source_policy = NULL;
                per_source_policy_file = policy_file;
                per_source_key_tpl_name = file_policy.per_source_key_tpl_name;
                file_policy.per_source_key_tpl_name = NULL;
                per_source_max_states = file_policy.per_source_max_states;
                per_source_topn = file_policy.per_source_topn;
            }
        }
    }

    if (scope == RATELIMIT_SCOPE_OUTPUT) {
        if (policy_file == NULL) {
            LogError(0, RS_RET_CONFIG_ERROR, "ratelimit: output policy '%s' must use a YAML policy file", name);
            ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
        }
        if (has_inline_policy_params || has_legacy_per_source_params) {
            LogError(0, RS_RET_CONFIG_ERROR,
                     "ratelimit: output policy '%s' must define limits only in the YAML policy file", name);
            ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
        }
        if (!file_policy.has_output_mode) {
            LogError(0, RS_RET_CONFIG_ERROR, "ratelimit: output policy '%s' must define mode in the YAML policy file",
                     name);
            ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
        }
        if (!file_policy.has_interval || !file_policy.has_burst) {
            LogError(0, RS_RET_CONFIG_ERROR,
                     "ratelimit: output policy '%s' must define interval and burst in the YAML policy file", name);
            ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
        }
        if (file_policy.has_severity || file_policy.has_per_source) {
            LogError(0, RS_RET_CONFIG_ERROR,
                     "ratelimit: output policy '%s' does not support severity or perSource in this version", name);
            ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
        }
        if (output_mode == RATELIMIT_OUTPUT_MODE_PACE && burst == 0) {
            LogError(0, RS_RET_CONFIG_ERROR, "ratelimit: output policy '%s' pace mode requires burst greater than zero",
                     name);
            ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
        }
    } else if (file_policy.has_output_mode) {
        LogError(0, RS_RET_CONFIG_ERROR, "ratelimit: input policy '%s' must not define output mode", name);
        ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
    }

    if (per_source_enabled && per_source_policy == NULL) {
        if (per_source_policy_file == NULL) {
            LogError(0, RS_RET_CONFIG_ERROR,
                     "ratelimit: per-source rate limiting enabled but perSourcePolicy is missing for '%s'", name);
            ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
        }
        CHKiRet(parsePerSourcePolicyFile(per_source_policy_file, &per_source_policy));
    }


    /* Logic:
     * 1. Search for existing shared object
     * 2. If exists:
     *    a. If policy_file != NULL, update it (lock, update, unlock)
     *    b. If policy_file == NULL, do nothing (read-only, keep existing)
     * 3. If not exists:
     *    a. Create new, fill values, insert into hashtable
     */

    bucket = ratelimitCfgBucket(&conf->ratelimit_cfgs, name, &hashvalue);
    pthread_rwlock_wrlock(&bucket->lock);
    bLocked = 1;
    existing_shared = (ratelimit_shared_t *)hashtable_search_prehashed(bucket->ht, hashvalue, (void *)name);

    if (existing_shared != NULL) {
        LogError(0, RS_RET_CONFIG_ERROR, "ratelimit: duplicate name '%s' in current config set", name);
        pthread_rwlock_unlock(&bucket->lock);
        bLocked = 0;
        ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
    }

    /* Not found - Create New */
    CHKmalloc(shared = calloc(1, sizeof(ratelimit_shared_t)));
    ratelimitSharedRefsInit(shared, 1);
    pthread_mutex_init(&shared->mut, NULL);
    pthread_mutex_init(&shared->per_source_policy_mut, NULL);
    pthread_mutex_init(&shared->per_source_key_policy_mut, NULL);
    CHKmalloc(key = strdup(name));
    shared->name = key;
    shared->scope = scope;
    shared->output_mode = output_mode;
    shared->interval = interval;
    shared->burst = burst;
    shared->severity = severity;
    shared->policy_watch = policy_watch;
    shared->policy_watch_debounce_ms = debounce_ms;
    if (policy_file) {
        CHKmalloc(shared->policy_file = strdup(policy_file));
    }
    if (per_source_enabled) {
        iRet = ratelimitInitPerSourceShared(shared, per_source_policy, per_source_policy_file, per_source_max_states,
                                            per_source_topn);
        free(per_source_policy);
        per_source_policy = NULL;
        CHKiRet(iRet);
        if (ratelimitLookupPerSourceTemplate(conf, per_source_key_tpl_name, &per_source_key_tpl,
                                             &per_source_key_tpl_default) != RS_RET_OK) {
            if (per_source_key_tpl_name != NULL) {
                LogError(0, RS_RET_CONFIG_ERROR, "ratelimit: perSourceKeyTpl '%s' not found for '%s'",
                         per_source_key_tpl_name, name);
            } else {
                LogError(0, RS_RET_CONFIG_ERROR,
                         "ratelimit: default perSourceKeyTpl 'RSYSLOG_PerSourceKey' not found for '%s'", name);
            }
            pthread_rwlock_unlock(&bucket->lock);
            bLocked = 0;
            ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
        }
        if (per_source_key_tpl_name != NULL) {
            CHKmalloc(shared->per_source_key_tpl_name = strdup(per_source_key_tpl_name));
        }
        pthread_mutex_lock(&shared->per_source_policy_mut);
        shared->per_source_policy_from_policy_file = per_source_from_policy_file;
        ratelimitPublishPerSourceKeyPolicy(shared, per_source_key_tpl, per_source_key_tpl_default);
        pthread_mutex_unlock(&shared->per_source_policy_mut);
        ratelimitPerSourceStoreEnabled(&shared->per_source_enabled, &shared->per_source_policy_mut, 1);
    }

    if (hashtable_insert_prehashed(bucket->ht, hashvalue, key, shared) == 0) {
        pthread_rwlock_unlock(&bucket->lock);
        bLocked = 0;
        LogError(0, RS_RET_OUT_OF_MEMORY, "ratelimit: error inserting config into hashtable");
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    /* shared ownership now in hashtable */
    existing_shared = shared;
    shared = NULL;
    pthread_rwlock_unlock(&bucket->lock);
    bLocked = 0;
    CHKiRet(ratelimitRegisterWatchTargets(existing_shared));

finalize_it:
    if (bLocked) {
        pthread_rwlock_unlock(&bucket->lock);
    }
    if (shared != NULL) {
        /* If we are here, shared was allocated but NOT inserted (error case before insert) */
        free(shared->name); /* key was assigned to name */
        free(shared->policy_file);
        ratelimitDestroyPerSourceShards(shared);
        if (shared->per_source_stats != NULL) {
            statsobj.Destruct(&shared->per_source_stats);
        }
        free(shared->per_source_top_ctrs);
        free(shared->per_source_top_values);
        if (shared->per_source_top_keys != NULL) {
            for (unsigned int i = 0; i < shared->per_source_topn; ++i) {
                free(shared->per_source_top_keys[i]);
            }
        }
        free(shared->per_source_top_keys);
        free(shared->per_source_key_tpl_name);
        pthread_mutex_destroy(&shared->per_source_key_policy_mut);
        pthread_mutex_destroy(&shared->per_source_policy_mut);
        free(shared->per_source_policy_file);
        pthread_mutex_destroy(&shared->mut);
        ratelimitSharedRefsDestroy(shared);
        free(shared);
    }
    if (per_source_policy != NULL) {
        if (per_source_policy->overrides != NULL) hashtable_destroy(per_source_policy->overrides, 1);
        free(per_source_policy);
    }
    ratelimitPolicyFileDestruct(&file_policy);

    RETiRet;
}

rsRetVal ratelimitNewFromConfigForScope(ratelimit_t **ppThis,
                                        rsconf_t *conf,
                                        const char *configname,
                                        const char *modname,
                                        const char *dynname,
                                        ratelimit_scope_t scope) {
    DEFiRet;
    ratelimit_shared_t *shared;
    ratelimit_cfg_bucket_t *bucket;
    unsigned int hashvalue;
    sbool shared_ref = 0;

    if (configname == NULL || conf == NULL) {
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    bucket = ratelimitCfgBucket(&conf->ratelimit_cfgs, configname, &hashvalue);
    pthread_rwlock_rdlock(&bucket->lock);
    shared = (ratelimit_shared_t *)hashtable_search_prehashed(bucket->ht, hashvalue, (void *)configname);
    if (shared == NULL) {
        pthread_rwlock_unlock(&bucket->lock);
        LogError(0, RS_RET_NOT_FOUND, "ratelimit config '%s' not found", configname);
        ABORT_FINALIZE(RS_RET_NOT_FOUND);
    }
    if (shared->scope != scope) {
        pthread_rwlock_unlock(&bucket->lock);
        LogError(0, RS_RET_CONFIG_ERROR, "ratelimit config '%s' has incompatible scope for %s", configname, modname);
        ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
    }
    ratelimitSharedAddRef(shared);
    shared_ref = 1;
    pthread_rwlock_unlock(&bucket->lock);

    /* We use ratelimitNew to allocate the instance, but we override the shared ptr */
    CHKiRet(ratelimitNew(ppThis, modname, dynname));

    /* Re-pointing to the global shared object */
    /* First, free the default one created by ratelimitNew */
    if ((*ppThis)->bOwnsShared) {
        pthread_mutex_destroy(&(*ppThis)->pShared->mut);
        ratelimitDestroyPerSourceShards((*ppThis)->pShared);
        pthread_mutex_destroy(&(*ppThis)->pShared->per_source_key_policy_mut);
        pthread_mutex_destroy(&(*ppThis)->pShared->per_source_policy_mut);
        free((*ppThis)->pShared);
    }

    (*ppThis)->pShared = shared;
    (*ppThis)->bOwnsShared = 0;
    shared_ref = 0;

finalize_it:
    if (shared_ref) {
        ratelimitSharedReleaseRef(shared);
    }
    RETiRet;
}

rsRetVal ratelimitNewFromConfig(
    ratelimit_t **ppThis, rsconf_t *conf, const char *configname, const char *modname, const char *dynname) {
    return ratelimitNewFromConfigForScope(ppThis, conf, configname, modname, dynname, RATELIMIT_SCOPE_INPUT);
}

/* generate a "repeated n times" message */
static smsg_t *ratelimitGenRepMsg(ratelimit_t *ratelimit) {
    smsg_t *repMsg;
    size_t lenRepMsg;
    uchar szRepMsg[1024];

    if (ratelimit->nsupp == 1) { /* we simply use the original message! */
        repMsg = MsgAddRef(ratelimit->pMsg);
    } else { /* we need to duplicate, original message may still be in use in other
              * parts of the system!  */
        if ((repMsg = MsgDup(ratelimit->pMsg)) == NULL) {
            DBGPRINTF("Message duplication failed, dropping repeat message.\n");
            goto done;
        }
        lenRepMsg = snprintf((char *)szRepMsg, sizeof(szRepMsg), " message repeated %d times: [%.800s]",
                             ratelimit->nsupp, getMSG(ratelimit->pMsg));
        MsgReplaceMSG(repMsg, szRepMsg, lenRepMsg);
    }

done:
    return repMsg;
}

static rsRetVal doLastMessageRepeatedNTimes(ratelimit_t *ratelimit, smsg_t *pMsg, smsg_t **ppRepMsg) {
    int bNeedUnlockMutex = 0;
    DEFiRet;

    if (ratelimit->bThreadSafe) {
        pthread_mutex_lock(&ratelimit->mut);
        bNeedUnlockMutex = 1;
    }

    if (ratelimit->pMsg != NULL && getMSGLen(pMsg) == getMSGLen(ratelimit->pMsg) &&
        !ustrcmp(getMSG(pMsg), getMSG(ratelimit->pMsg)) && !strcmp(getHOSTNAME(pMsg), getHOSTNAME(ratelimit->pMsg)) &&
        !strcmp(getPROCID(pMsg, LOCK_MUTEX), getPROCID(ratelimit->pMsg, LOCK_MUTEX)) &&
        !strcmp(getAPPNAME(pMsg, LOCK_MUTEX), getAPPNAME(ratelimit->pMsg, LOCK_MUTEX))) {
        ratelimit->nsupp++;
        DBGPRINTF("msg repeated %d times\n", ratelimit->nsupp);
        /* use current message, so we have the new timestamp
         * (means we need to discard previous one) */
        msgDestruct(&ratelimit->pMsg);
        ratelimit->pMsg = pMsg;
        ABORT_FINALIZE(RS_RET_DISCARDMSG);
    } else { /* new message, do "repeat processing" & save it */
        if (ratelimit->pMsg != NULL) {
            if (ratelimit->nsupp > 0) {
                *ppRepMsg = ratelimitGenRepMsg(ratelimit);
                ratelimit->nsupp = 0;
            }
            msgDestruct(&ratelimit->pMsg);
        }
        ratelimit->pMsg = MsgAddRef(pMsg);
    }

finalize_it:
    if (bNeedUnlockMutex) pthread_mutex_unlock(&ratelimit->mut);
    RETiRet;
}


/* helper: tell how many messages we lost due to linux-like ratelimiting */
static void tellLostCnt(ratelimit_t *ratelimit) {
    uchar msgbuf[1024];

    if (ratelimit->missed) {
        snprintf((char *)msgbuf, sizeof(msgbuf),
                 "%s: %u messages lost due to rate-limiting (%u allowed within %u seconds)", ratelimit->name,
                 ratelimit->missed, ratelimitSharedLoadUInt(&ratelimit->pShared->burst, &ratelimit->pShared->mut),
                 ratelimitSharedLoadUInt(&ratelimit->pShared->interval, &ratelimit->pShared->mut));
        ratelimit->missed = 0;
        logmsgInternal(RS_RET_RATE_LIMITED, LOG_SYSLOG | LOG_INFO, msgbuf, 0);
    }
}

/* Linux-like ratelimiting, modelled after the linux kernel
 * returns 1 if message is within rate limit and shall be
 * processed, 0 otherwise.
 * This implementation is NOT THREAD-SAFE and must not
 * be called concurrently.
 */
static int ATTR_NONNULL()
    withinRatelimit(ratelimit_t *__restrict__ const ratelimit, time_t tt, const char *const appname) {
    int ret;
    uchar msgbuf[1024];

    if (ratelimit->bThreadSafe) {
        pthread_mutex_lock(&ratelimit->mut);
    }

    unsigned int interval = ratelimitSharedLoadUInt(&ratelimit->pShared->interval, &ratelimit->pShared->mut);
    unsigned int burst = ratelimitSharedLoadUInt(&ratelimit->pShared->burst, &ratelimit->pShared->mut);

    if (interval == 0) {
        ret = 1;
        goto finalize_it;
    }

    /* we primarily need "NoTimeCache" mode for imjournal, as it
     * sets the message generation time to the journal timestamp.
     * As such, we do not get a proper indication of the actual
     * message rate. To prevent this, we need to query local
     * system time ourselvs.
     */
    if (ratelimit->bNoTimeCache) tt = time(NULL);


    /* assert(burst != 0); -- burst can be 0 for block-all policy */


    if (ratelimit->begin == 0) ratelimit->begin = tt;

    /* resume if we go out of time window or if time has gone backwards */
    if ((tt > (time_t)(ratelimit->begin + interval)) || (tt < ratelimit->begin)) {
        ratelimit->begin = 0;
        ratelimit->done = 0;
        tellLostCnt(ratelimit);
    }

    /* do actual limit check */
    if (burst > ratelimit->done) {
        ratelimit->done++;
        ret = 1;
    } else {
        ratelimit->missed++;
        if (ratelimit->missed == 1) {
            snprintf((char *)msgbuf, sizeof(msgbuf), "%s from <%s>: begin to drop messages due to rate-limiting",
                     ratelimit->name, appname);
            logmsgInternal(RS_RET_RATE_LIMITED, LOG_SYSLOG | LOG_INFO, msgbuf, 0);
        }
        ret = 0;
    }

finalize_it:
    if (ratelimit->bThreadSafe) {
        pthread_mutex_unlock(&ratelimit->mut);
    }

    return ret;
}

/* ratelimit a message based on message count
 * - handles only rate-limiting
 * This function returns RS_RET_OK, if the caller shall process
 * the message regularly and RS_RET_DISCARD if the caller must
 * discard the message. The caller should also discard the message
 * if another return status occurs.
 */
rsRetVal ratelimitMsgCount(ratelimit_t *__restrict__ const ratelimit, time_t tt, const char *const appname) {
    DEFiRet;
    if (ratelimitSharedLoadUInt(&ratelimit->pShared->interval, &ratelimit->pShared->mut)) {
        if (withinRatelimit(ratelimit, tt, appname) == 0) {
            ABORT_FINALIZE(RS_RET_DISCARDMSG);
        }
    }
finalize_it:
    if (Debug) {
        if (iRet == RS_RET_DISCARDMSG) DBGPRINTF("message discarded by ratelimiting\n");
    }
    RETiRet;
}

rsRetVal ratelimitMsgCountWait(ratelimit_t *__restrict__ const ratelimit,
                               time_t tt,
                               const char *const appname,
                               unsigned int *const wait_usec) {
    unsigned int interval;
    unsigned int burst;
    time_t resume_time;
    time_t wait_sec;
    DEFiRet;

    if (ratelimit == NULL || wait_usec == NULL) return RS_RET_PARAM_ERROR;
    (void)appname;
    *wait_usec = 0;

    if (ratelimit->bThreadSafe) {
        pthread_mutex_lock(&ratelimit->mut);
    }

    interval = ratelimitSharedLoadUInt(&ratelimit->pShared->interval, &ratelimit->pShared->mut);
    burst = ratelimitSharedLoadUInt(&ratelimit->pShared->burst, &ratelimit->pShared->mut);
    if (interval == 0) goto finalize_it;

    if (ratelimit->bNoTimeCache || tt == 0) tt = time(NULL);
    if (ratelimit->begin == 0) ratelimit->begin = tt;

    if ((tt >= (time_t)(ratelimit->begin + interval)) || (tt < ratelimit->begin)) {
        ratelimit->begin = tt;
        ratelimit->done = 0;
        tellLostCnt(ratelimit);
    }

    if (burst > ratelimit->done) {
        ratelimit->done++;
    } else {
        resume_time = ratelimit->begin + interval;
        wait_sec = (resume_time > tt) ? resume_time - tt : 1;
        if (wait_sec > (time_t)(UINT_MAX / 1000000U)) {
            *wait_usec = UINT_MAX;
        } else {
            *wait_usec = (unsigned int)wait_sec * 1000000U;
        }
        ABORT_FINALIZE(RS_RET_DISCARDMSG);
    }

finalize_it:
    if (ratelimit->bThreadSafe) {
        pthread_mutex_unlock(&ratelimit->mut);
    }
    RETiRet;
}

ratelimit_scope_t ratelimitGetScope(const ratelimit_t *ratelimit) {
    if (ratelimit == NULL || ratelimit->pShared == NULL) return RATELIMIT_SCOPE_INPUT;
    return ratelimit->pShared->scope;
}

ratelimit_output_mode_t ratelimitGetOutputMode(const ratelimit_t *ratelimit) {
    ratelimit_output_mode_t mode;
    if (ratelimit == NULL || ratelimit->pShared == NULL) return RATELIMIT_OUTPUT_MODE_DROP;
    pthread_mutex_lock(&ratelimit->pShared->mut);
    mode = ratelimit->pShared->output_mode;
    pthread_mutex_unlock(&ratelimit->pShared->mut);
    return mode;
}

void ratelimitForbidOutputPace(ratelimit_t *ratelimit) {
    if (ratelimit == NULL || ratelimit->pShared == NULL) return;
    pthread_mutex_lock(&ratelimit->pShared->mut);
    ratelimit->pShared->output_pace_forbidden = 1;
    pthread_mutex_unlock(&ratelimit->pShared->mut);
}

/* ratelimit a message, that means:
 * - handle "last message repeated n times" logic
 * - handle actual (discarding) rate-limiting
 * This function returns RS_RET_OK, if the caller shall process
 * the message regularly and RS_RET_DISCARD if the caller must
 * discard the message. The caller should also discard the message
 * if another return status occurs. This places some burden on the
 * caller logic, but provides best performance. Demanding this
 * cooperative mode can enable a faulty caller to thrash up part
 * of the system, but we accept that risk (a faulty caller can
 * always do all sorts of evil, so...)
 * If *ppRepMsg != NULL on return, the caller must enqueue that
 * message before the original message.
 */
rsRetVal ATTR_NONNULL(1, 2, 3)
    ratelimitMsg(ratelimit_t *__restrict__ const ratelimit, smsg_t *pMsg, smsg_t **ppRepMsg) {
    DEFiRet;
    rsRetVal localRet;
    int severity = 0;
    unsigned int interval;
    int threshold;

    assert(ratelimit != NULL);
    assert(pMsg != NULL);
    assert(ppRepMsg != NULL);
    *ppRepMsg = NULL;

    interval = ratelimitSharedLoadUInt(&ratelimit->pShared->interval, &ratelimit->pShared->mut);
    threshold = ratelimitSharedLoadSeverity(&ratelimit->pShared->severity, &ratelimit->pShared->mut);

    if (runConf->globals.bReduceRepeatMsgs || threshold > 0) {
        /* consider early parsing only if really needed */
        if ((pMsg->msgFlags & NEEDS_PARSING) != 0) {
            if ((localRet = parser.ParseMsg(pMsg)) != RS_RET_OK) {
                DBGPRINTF("Message discarded, parsing error %d\n", localRet);
                ABORT_FINALIZE(RS_RET_DISCARDMSG);
            }
        }
        severity = pMsg->iSeverity;
    }

    /* Only the messages having severity level at or below the
     * treshold (the value is >=) are subject to ratelimiting. */
    if (interval && (severity >= threshold)) {
        char namebuf[512]; /* 256 for FGDN adn 256 for APPNAME should be enough */
        snprintf(namebuf, sizeof namebuf, "%s:%s", getHOSTNAME(pMsg), getAPPNAME(pMsg, 0));
        if (withinRatelimit(ratelimit, pMsg->ttGenTime, namebuf) == 0) {
            msgDestruct(&pMsg);
            ABORT_FINALIZE(RS_RET_DISCARDMSG);
        }
    }
    if (runConf->globals.bReduceRepeatMsgs) {
        CHKiRet(doLastMessageRepeatedNTimes(ratelimit, pMsg, ppRepMsg));
    }
finalize_it:
    if (Debug) {
        if (iRet == RS_RET_DISCARDMSG) DBGPRINTF("message discarded by ratelimiting\n");
    }
    RETiRet;
}

/* returns 1, if the ratelimiter performs any checks and 0 otherwise */
int ratelimitChecked(ratelimit_t *ratelimit) {
    const unsigned int per_source_enabled = ratelimitPerSourceLoadEnabled(&ratelimit->pShared->per_source_enabled,
                                                                          &ratelimit->pShared->per_source_policy_mut);

    return ratelimitSharedLoadUInt(&ratelimit->pShared->interval, &ratelimit->pShared->mut) || per_source_enabled ||
           runConf->globals.bReduceRepeatMsgs;
}


/* add a message to a ratelimiter/multisubmit structure.
 * ratelimiting is automatically handled according to the ratelimit
 * settings.
 * if pMultiSub == NULL, a single-message enqueue happens (under reconsideration)
 */
typedef struct per_source_key_ctx_s {
    const char *key;
    size_t key_len;
    actWrkrIParams_t tpl_param;
    char *combined;
    uchar *free_host;
    uchar *free_port;
} per_source_key_ctx_t;

static void ratelimitPerSourceKeyCtxFree(per_source_key_ctx_t *ctx) {
    if (ctx == NULL) return;
    free(ctx->tpl_param.param);
    free(ctx->combined);
    free(ctx->free_host);
    free(ctx->free_port);
    memset(ctx, 0, sizeof(*ctx));
}

static rsRetVal ratelimitGetRcvFromString(
    smsg_t *pMsg, propid_t propid, const char **value, size_t *len, uchar **to_free) {
    uchar *prop_value;
    int prop_len;
    DEFiRet;

    if (pMsg == NULL || value == NULL || len == NULL || to_free == NULL) ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    *to_free = NULL;
    MsgGetRcvFromProp(pMsg, propid, &prop_value, &prop_len);
    *value = (const char *)prop_value;
    *len = (size_t)prop_len;

finalize_it:
    RETiRet;
}

static rsRetVal ratelimitComposePerSourceHostPort(
    per_source_key_ctx_t *ctx, const char *host, size_t host_len, const char *port, size_t port_len) {
    DEFiRet;

    if (ctx == NULL) ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    CHKmalloc(ctx->combined = malloc(host_len + 1 + port_len + 1));
    if (host_len > 0) memcpy(ctx->combined, host, host_len);
    ctx->combined[host_len] = ':';
    if (port_len > 0) memcpy(ctx->combined + host_len + 1, port, port_len);
    ctx->combined[host_len + 1 + port_len] = '\0';
    ctx->key = ctx->combined;
    ctx->key_len = host_len + 1 + port_len;

finalize_it:
    RETiRet;
}

static rsRetVal ratelimitComputePerSourceKey(ratelimit_t *ratelimit, smsg_t *pMsg, per_source_key_ctx_t *ctx) {
    ratelimit_shared_t *shared;
    ratelimit_ps_key_policy_t policy;
    const char *host = "";
    const char *port = "";
    size_t host_len = 0;
    size_t port_len = 0;
    DEFiRet;

    if (ratelimit == NULL || pMsg == NULL || ctx == NULL || ratelimit->pShared == NULL)
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    memset(ctx, 0, sizeof(*ctx));
    shared = ratelimit->pShared;
    if (!ratelimitPerSourceLoadEnabled(&shared->per_source_enabled, &shared->per_source_policy_mut)) FINALIZE;
    ratelimitLoadPerSourceKeyPolicy(shared, &policy);

    switch (policy.key_mode) {
        case RL_PS_KEY_FROMHOST_IP:
            CHKiRet(ratelimitGetRcvFromString(pMsg, PROP_FROMHOST_IP, &ctx->key, &ctx->key_len, &ctx->free_host));
            break;
        case RL_PS_KEY_FROMHOST:
            CHKiRet(ratelimitGetRcvFromString(pMsg, PROP_FROMHOST, &ctx->key, &ctx->key_len, &ctx->free_host));
            break;
        case RL_PS_KEY_FROMHOST_IP_PORT:
            CHKiRet(ratelimitGetRcvFromString(pMsg, PROP_FROMHOST_IP, &host, &host_len, &ctx->free_host));
            CHKiRet(ratelimitGetRcvFromString(pMsg, PROP_FROMHOST_PORT, &port, &port_len, &ctx->free_port));
            CHKiRet(ratelimitComposePerSourceHostPort(ctx, host, host_len, port, port_len));
            break;
        case RL_PS_KEY_FROMHOST_PORT:
            CHKiRet(ratelimitGetRcvFromString(pMsg, PROP_FROMHOST, &host, &host_len, &ctx->free_host));
            CHKiRet(ratelimitGetRcvFromString(pMsg, PROP_FROMHOST_PORT, &port, &port_len, &ctx->free_port));
            CHKiRet(ratelimitComposePerSourceHostPort(ctx, host, host_len, port, port_len));
            break;
        case RL_PS_KEY_TPL:
        default:
            if (policy.key_needs_parsing && (pMsg->msgFlags & NEEDS_PARSING) != 0) {
                STATSCOUNTER_INC(shared->ctrPerSourceKeyParseEvals, shared->mutCtrPerSourceKeyParseEvals);
                if (parser.ParseMsg(pMsg) != RS_RET_OK) {
                    FINALIZE;
                }
            }
            if (policy.key_tpl == NULL || policy.key_tpl_default) {
                ctx->key = getHOSTNAME(pMsg);
                ctx->key_len = getHOSTNAMELen(pMsg);
                break;
            }
            STATSCOUNTER_INC(shared->ctrPerSourceKeyTplEvals, shared->mutCtrPerSourceKeyTplEvals);
            if (tplToString(policy.key_tpl, pMsg, &ctx->tpl_param, NULL) == RS_RET_OK) {
                ctx->key = (const char *)ctx->tpl_param.param;
                ctx->key_len = ctx->tpl_param.lenStr;
            }
            break;
    }

finalize_it:
    RETiRet;
}

static rsRetVal ratelimitSubmitCheckedMsg(multi_submit_t *pMultiSub, smsg_t **ppMsg) {
    DEFiRet;

    if (ppMsg == NULL || *ppMsg == NULL) FINALIZE;

    if (pMultiSub == NULL) {
        CHKiRet(submitMsg2(*ppMsg));
        *ppMsg = NULL;
    } else {
        if ((*ppMsg)->iLenRawMsg > glblGetMaxLine(runConf)) {
            if (pMultiSub->nElem > 0) {
                CHKiRet(multiSubmitMsg2(pMultiSub));
            }
            CHKiRet(submitMsg2(*ppMsg));
            *ppMsg = NULL;
            FINALIZE;
        }
        pMultiSub->ppMsgs[pMultiSub->nElem++] = *ppMsg;
        *ppMsg = NULL;
        if (pMultiSub->nElem == pMultiSub->maxElem) CHKiRet(multiSubmitMsg2(pMultiSub));
    }

finalize_it:
    RETiRet;
}

static rsRetVal ratelimitSubmitReportMsg(multi_submit_t *pMultiSub, smsg_t **ppRepMsg) {
    DEFiRet;

    if (ppRepMsg == NULL || *ppRepMsg == NULL) FINALIZE;

    if (pMultiSub == NULL) {
        CHKiRet(submitMsg2(*ppRepMsg));
        *ppRepMsg = NULL;
    } else {
        pMultiSub->ppMsgs[pMultiSub->nElem++] = *ppRepMsg;
        *ppRepMsg = NULL;
        if (pMultiSub->nElem == pMultiSub->maxElem) CHKiRet(multiSubmitMsg2(pMultiSub));
    }

finalize_it:
    RETiRet;
}

rsRetVal ATTR_NONNULL(1, 3) ratelimitAddMsg(ratelimit_t *ratelimit, multi_submit_t *pMultiSub, smsg_t *pMsg) {
    rsRetVal localRet;
    smsg_t *repMsg = NULL;
    sbool msg_owned = 0;
    sbool per_source_dropped = 0;
    per_source_key_ctx_t per_source_key = {0};
    DEFiRet;

    assert(ratelimit != NULL);
    assert(pMsg != NULL);

    localRet = ratelimitMsg(ratelimit, pMsg, &repMsg);
    CHKiRet(localRet);
    msg_owned = 1;
    CHKiRet(ratelimitSubmitReportMsg(pMultiSub, &repMsg));
    CHKiRet(ratelimitComputePerSourceKey(ratelimit, pMsg, &per_source_key));
    if (per_source_key.key != NULL && per_source_key.key_len > 0) {
        if ((iRet = ratelimitPerSourceCheck(ratelimit, per_source_key.key, per_source_key.key_len, pMsg->ttGenTime)) !=
            RS_RET_OK) {
            per_source_dropped = 1;
            ABORT_FINALIZE(iRet);
        }
    }
    CHKiRet(ratelimitSubmitCheckedMsg(pMultiSub, &pMsg));

finalize_it:
    ratelimitPerSourceKeyCtxFree(&per_source_key);
    if (repMsg != NULL) {
        msgDestruct(&repMsg);
    }
    if (pMsg != NULL && (msg_owned || per_source_dropped)) {
        msgDestruct(&pMsg);
    }
    RETiRet;
}

/* modname must be a static name (usually expected to be the module
 * name and MUST be present. dynname may be NULL and can be used for
 * dynamic information, e.g. PID or listener IP, ...
 * Both values should be kept brief.
 */
rsRetVal ratelimitNew(ratelimit_t **ppThis, const char *modname, const char *dynname) {
    ratelimit_t *pThis = NULL;
    char namebuf[256];
    DEFiRet;

    CHKmalloc(pThis = calloc(1, sizeof(ratelimit_t)));
    if (modname == NULL) modname = "*ERROR:MODULE NAME MISSING*";

    if (dynname == NULL) {
        pThis->name = strdup(modname);
    } else {
        snprintf(namebuf, sizeof(namebuf), "%s[%s]", modname, dynname);
        namebuf[sizeof(namebuf) - 1] = '\0'; /* to be on safe side */
        pThis->name = strdup(namebuf);
    }

    /* Allocate default shared structure for standalone instance */
    CHKmalloc(pThis->pShared = calloc(1, sizeof(ratelimit_shared_t)));
    pThis->bOwnsShared = 1;
    pThis->pShared->scope = RATELIMIT_SCOPE_INPUT;
    pThis->pShared->output_mode = RATELIMIT_OUTPUT_MODE_DROP;
    pthread_mutex_init(&pThis->pShared->mut, NULL);
    pthread_mutex_init(&pThis->pShared->per_source_policy_mut, NULL);
    pthread_mutex_init(&pThis->pShared->per_source_key_policy_mut, NULL);

    DBGPRINTF("ratelimit:%s:new ratelimiter\n", pThis->name);
    *ppThis = pThis;
finalize_it:
    if (iRet != RS_RET_OK) {
        if (pThis != NULL) {
            free(pThis->name);
            free(pThis);
        }
    }
    RETiRet;
}


/* enable linux-like ratelimiting */
void ratelimitSetLinuxLike(ratelimit_t *ratelimit, unsigned int interval, unsigned int burst) {
    ratelimitSharedStoreUInt(&ratelimit->pShared->interval, &ratelimit->pShared->mut, interval);
    ratelimitSharedStoreUInt(&ratelimit->pShared->burst, &ratelimit->pShared->mut, burst);
    ratelimit->done = 0;
    ratelimit->missed = 0;
    ratelimit->begin = 0;
}


static void ratelimitEnsureMutexInitialized(ratelimit_t *ratelimit) {
    if (!ratelimit->bMutInitialized) {
        pthread_mutex_init(&ratelimit->mut, NULL);
        ratelimit->bMutInitialized = 1;
    }
}

/* enable thread-safe operations mode. This make sure that
 * a single ratelimiter can be called from multiple threads. As
 * this causes some overhead and is not always required, it needs
 * to be explicitely enabled. This operation cannot be undone
 * (think: why should one do that???)
 */
void ratelimitSetThreadSafe(ratelimit_t *ratelimit) {
    ratelimit->bThreadSafe = 1;
    ratelimitEnsureMutexInitialized(ratelimit);
}
void ratelimitSetNoTimeCache(ratelimit_t *ratelimit) {
    ratelimit->bNoTimeCache = 1;
    ratelimitEnsureMutexInitialized(ratelimit);
}

/* Severity level determines which messages are subject to
 * ratelimiting. Default (no value set) is all messages.
 */
void ratelimitSetSeverity(ratelimit_t *ratelimit, int severity) {
    ratelimitSharedStoreSeverity(&ratelimit->pShared->severity, &ratelimit->pShared->mut, severity);
}

void ratelimitDestruct(ratelimit_t *ratelimit) {
    smsg_t *pMsg;
    if (ratelimit->pMsg != NULL) {
        if (ratelimit->nsupp > 0) {
            pMsg = ratelimitGenRepMsg(ratelimit);
            if (pMsg != NULL) submitMsg2(pMsg);
        }
        msgDestruct(&ratelimit->pMsg);
    }
    tellLostCnt(ratelimit);
    if (ratelimit->bMutInitialized) pthread_mutex_destroy(&ratelimit->mut);

    if (ratelimit->bOwnsShared && ratelimit->pShared != NULL) {
        pthread_mutex_destroy(&ratelimit->pShared->mut);
        ratelimitDestroyPerSourceShards(ratelimit->pShared);
        pthread_mutex_destroy(&ratelimit->pShared->per_source_key_policy_mut);
        pthread_mutex_destroy(&ratelimit->pShared->per_source_policy_mut);
        free(ratelimit->pShared);
    } else if (ratelimit->pShared != NULL) {
        ratelimitSharedReleaseRef(ratelimit->pShared);
    }

    free(ratelimit->name);
    free(ratelimit);
}

void ratelimitModExit(void) {
    objRelease(datetime, CORE_COMPONENT);
    objRelease(glbl, CORE_COMPONENT);
    objRelease(parser, CORE_COMPONENT);
    objRelease(statsobj, CORE_COMPONENT);
}

rsRetVal ratelimitModInit(void) {
    DEFiRet;
    CHKiRet(objGetObjInterface(&obj));
    CHKiRet(objUse(glbl, CORE_COMPONENT));
    CHKiRet(objUse(datetime, CORE_COMPONENT));
    CHKiRet(objUse(parser, CORE_COMPONENT));
    CHKiRet(objUse(statsobj, CORE_COMPONENT));
finalize_it:
    RETiRet;
}

static void ratelimitDoHUPShared(ratelimit_shared_t *const shared) {
    sbool reload_per_source;

    if (shared->policy_file) {
        ratelimitReloadPolicyFile(shared, "HUP");
    }
    pthread_mutex_lock(&shared->per_source_policy_mut);
    reload_per_source = (shared->per_source_policy_file != NULL && !shared->per_source_policy_from_policy_file);
    pthread_mutex_unlock(&shared->per_source_policy_mut);
    if (reload_per_source) {
        ratelimitReloadPerSourcePolicyFile(shared, "HUP");
    }
}

static rsRetVal ratelimitCfgsAppendSnapshot(ratelimit_shared_t ***const snapshot,
                                            size_t *const count,
                                            size_t *const capacity,
                                            ratelimit_shared_t *const shared) {
    ratelimit_shared_t **new_snapshot;
    size_t new_capacity;
    DEFiRet;

    if (*count == *capacity) {
        new_capacity = (*capacity == 0) ? 16 : (*capacity * 2);
        if (new_capacity < *capacity || new_capacity > ((size_t)-1 / sizeof(ratelimit_shared_t *))) {
            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        }
        CHKmalloc(new_snapshot = realloc(*snapshot, new_capacity * sizeof(ratelimit_shared_t *)));
        *snapshot = new_snapshot;
        *capacity = new_capacity;
    }
    (*snapshot)[(*count)++] = shared;

finalize_it:
    RETiRet;
}

static void ratelimitCfgsReleaseSnapshot(ratelimit_shared_t **const snapshot, const size_t count) {
    if (snapshot != NULL) {
        for (size_t i = 0; i < count; ++i) {
            if (snapshot[i] != NULL) {
                ratelimitSharedReleaseRef(snapshot[i]);
            }
        }
        free(snapshot);
    }
}

static rsRetVal ratelimitCfgsSnapshot(ratelimit_cfgs_t *const cfgs,
                                      ratelimit_shared_t ***const snapshot,
                                      size_t *const count) {
    struct hashtable_itr *itr = NULL;
    size_t capacity = 0;
    unsigned int locked_shard = RATELIMIT_CFG_SHARDS;
    DEFiRet;

    *snapshot = NULL;
    *count = 0;

    for (unsigned int i = 0; i < RATELIMIT_CFG_SHARDS; ++i) {
        pthread_rwlock_rdlock(&cfgs->shards[i].lock);
        locked_shard = i;
        if (cfgs->shards[i].ht != NULL && hashtable_count(cfgs->shards[i].ht) > 0) {
            CHKmalloc(itr = hashtable_iterator(cfgs->shards[i].ht));
            do {
                ratelimit_shared_t *const shared = (ratelimit_shared_t *)hashtable_iterator_value(itr);
                if (shared != NULL) {
                    CHKiRet(ratelimitCfgsAppendSnapshot(snapshot, count, &capacity, shared));
                    ratelimitSharedAddRef(shared);
                }
            } while (hashtable_iterator_advance(itr));
            free(itr);
            itr = NULL;
        }
        pthread_rwlock_unlock(&cfgs->shards[i].lock);
        locked_shard = RATELIMIT_CFG_SHARDS;
    }

finalize_it:
    free(itr);
    if (locked_shard < RATELIMIT_CFG_SHARDS) {
        pthread_rwlock_unlock(&cfgs->shards[locked_shard].lock);
    }
    if (iRet != RS_RET_OK) {
        ratelimitCfgsReleaseSnapshot(*snapshot, *count);
        *snapshot = NULL;
        *count = 0;
    }
    RETiRet;
}

void ratelimitDoHUP(void) {
    ratelimit_shared_t **snapshot = NULL;
    size_t count = 0;

    if (runConf == NULL) {
        return;
    }

    if (ratelimitCfgsSnapshot(&runConf->ratelimit_cfgs, &snapshot, &count) != RS_RET_OK) {
        LogError(0, RS_RET_OUT_OF_MEMORY, "ratelimit: cannot snapshot configs for HUP reload");
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        if (snapshot[i] != NULL) {
            ratelimitDoHUPShared(snapshot[i]);
        }
    }
    ratelimitCfgsReleaseSnapshot(snapshot, count);
}
