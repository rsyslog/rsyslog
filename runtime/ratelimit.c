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

#include "rsyslog.h"
#include "errmsg.h"
#include "ratelimit.h"
#include "datetime.h"
#include "parser.h"
#include "unicode-helper.h"
#include "msg.h"
#include "rsconf.h"
#include "dirty.h"
#include "hashtable.h"
#include "statsobj.h"
#include "template.h"
#include "hashtable_itr.h"
#ifdef HAVE_LIBYAML
    #include <yaml.h>
#endif

/* definitions for objects we access */
DEFobjStaticHelpers;
DEFobjCurrIf(glbl) DEFobjCurrIf(datetime) DEFobjCurrIf(parser) DEFobjCurrIf(statsobj)

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

static void ratelimitFreeShared(void *ptr) {
    ratelimit_shared_t *shared = (ratelimit_shared_t *)ptr;
    if (shared == NULL) return;
    pthread_mutex_destroy(&shared->mut);
    free(shared->policy_file);
    if (shared->per_source_overrides != NULL) {
        hashtable_destroy(shared->per_source_overrides, 1);
    }
    if (shared->per_source_states != NULL) {
        hashtable_destroy(shared->per_source_states, 1);
    }
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
    if (shared->per_source_enabled) {
        pthread_mutex_destroy(&shared->per_source_mut);
    }
    free(shared->per_source_policy_file);
    /* shared->name is the key, which is freed by hashtable */
    free(shared->name);
    free(shared);
}

void ratelimit_cfgsInit(ratelimit_cfgs_t *cfgs) {
    pthread_rwlock_init(&cfgs->lock, NULL);
    cfgs->ht = create_hashtable(16, hash_from_string, key_equals_string, ratelimitFreeShared);
}

void ratelimit_cfgsDestruct(ratelimit_cfgs_t *cfgs) {
    if (cfgs->ht != NULL) {
        hashtable_destroy(cfgs->ht, 1); /* 1 = free values */
    }
    pthread_rwlock_destroy(&cfgs->lock);
}


/*
 * Concurrency & Locking
 * - ratelimit_shared_t->mut guards shared policy updates for global limits.
 * - per-source limits use ratelimit_shared_t->per_source_mut to guard the
 *   override table, per-source state table, and LRU list.
 * - ratelimit_t->mut guards per-instance counters when thread-safe mode is enabled.
 */

#define RATELIMIT_PERSOURCE_DEFAULT_MAX_STATES 10000U
#define RATELIMIT_PERSOURCE_DEFAULT_TOPN 10U

struct ratelimit_ps_override_s {
    char *key;
    unsigned int max;
    unsigned int window;
    sbool has_max;
    sbool has_window;
};

typedef struct ratelimit_ps_override_s ratelimit_ps_override_t;

struct ratelimit_ps_state_s {
    char *key;
    unsigned int count;
    time_t window_start;
    uint64_t allowed;
    uint64_t dropped;
    time_t last_seen;
    struct ratelimit_ps_state_s *lru_prev;
    struct ratelimit_ps_state_s *lru_next;
};

typedef struct ratelimit_ps_state_s ratelimit_ps_state_t;

struct ratelimit_ps_policy_s {
    unsigned int default_max;
    unsigned int default_window;
    struct hashtable *overrides;
};

typedef struct ratelimit_ps_policy_s ratelimit_ps_policy_t;


static rsRetVal parsePolicyFile(const char *policy_file
#ifdef HAVE_LIBYAML
                                ,
                                unsigned int *interval,
                                unsigned int *burst,
                                intTiny *severity
#endif
) {
    DEFiRet;
#ifdef HAVE_LIBYAML
    FILE *fh = NULL;
    yaml_parser_t yamlParser;
    int bParserInitialized = 0;
    yaml_token_t token;
    int state = 0; /* 0: generic, 1: expect key, 2: expect value */
    char *curr_key = NULL;

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

    int done = 0;
    while (!done) {
        if (!yaml_parser_scan(&yamlParser, &token)) {
            LogError(0, RS_RET_CONF_PARAM_INVLD, "ratelimit: yaml parser error in policy file %s: %s", policy_file,
                     yamlParser.problem ? yamlParser.problem : "unknown error");
            ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
        }

        PRAGMA_DIAGNOSTIC_PUSH
        PRAGMA_IGNORE_Wswitch_enum switch (token.type) {
            case YAML_KEY_TOKEN:
                state = 1;
                break;
            case YAML_VALUE_TOKEN:
                state = 2;
                break;
            case YAML_SCALAR_TOKEN:
                if (state == 1) {
                    if (curr_key) free(curr_key);
                    curr_key = strdup((char *)token.data.scalar.value);
                } else if (state == 2) {
                    if (curr_key) {
                        char *endptr;
                        errno = 0;
                        long val = 0;
                        if (!strcmp(curr_key, "interval") || !strcmp(curr_key, "burst") ||
                            !strcmp(curr_key, "severity")) {
                            val = strtol((char *)token.data.scalar.value, &endptr, 10);
                            if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) || (errno != 0 && val == 0) ||
                                endptr == (char *)token.data.scalar.value || *endptr != '\0' || val < 0 ||
                                val > UINT_MAX) {
                                LogError(0, RS_RET_CONF_PARAM_INVLD, "ratelimit: invalid integer value for '%s': %s",
                                         curr_key, (char *)token.data.scalar.value);
                                /* ignore invalid values, keep parsing */
                            } else {
                                if (!strcmp(curr_key, "interval")) {
                                    *interval = (unsigned int)val;
                                } else if (!strcmp(curr_key, "burst")) {
                                    *burst = (unsigned int)val;
                                } else if (!strcmp(curr_key, "severity")) {
                                    if (val > 127) {
                                        LogError(0, RS_RET_CONF_PARAM_INVLD,
                                                 "ratelimit: severity value %ld out of range (max 127) in %s", val,
                                                 policy_file);
                                    } else {
                                        *severity = (intTiny)val;
                                    }
                                }
                            }
                        }
                    }
                }
                break;
            default:
                break;
        }
        PRAGMA_DIAGNOSTIC_POP

        if (token.type == YAML_STREAM_END_TOKEN) done = 1;
        yaml_token_delete(&token);
    }

finalize_it:
    if (bParserInitialized) {
        yaml_parser_delete(&yamlParser);
    }
    if (curr_key) free(curr_key);
    if (fh) fclose(fh);
#else
    LogError(0, RS_RET_CONF_PARAM_INVLD,
             "ratelimit: policy file '%s' specified but rsyslog "
             "was compiled without libyaml support. Policy settings ignored.",
             policy_file);
    iRet = RS_RET_CONF_PARAM_INVLD;
#endif
    RETiRet;
}

static void ratelimitFreePerSourceState(void *ptr) {
    ratelimit_ps_state_t *state = (ratelimit_ps_state_t *)ptr;
    if (state == NULL) return;
    free(state);
}

#ifdef HAVE_LIBYAML
static void ratelimitFreePerSourceOverride(void *ptr) {
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

static rsRetVal parseDurationSeconds(const char *value, unsigned int *out) {
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
    if (*end == 's' && *(end + 1) == '\0') {
        /* ok */
    } else if (*end != '\0') {
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
    int depth = 0;
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
                  create_hashtable(32, hash_from_string, key_equals_string, ratelimitFreePerSourceOverride));

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
        if (current_override != NULL) ratelimitFreePerSourceOverride(current_override);
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

static void ratelimitPerSourceUpdateTopN(ratelimit_shared_t *shared) {
    if (shared == NULL || shared->per_source_stats == NULL || shared->per_source_topn == 0) return;

    unsigned int topn = shared->per_source_topn;
    uint64_t *values = calloc(topn, sizeof(uint64_t));
    char **keys = calloc(topn, sizeof(char *));
    if (values == NULL || keys == NULL) {
        free(values);
        free(keys);
        return;
    }

    pthread_mutex_lock(&shared->per_source_mut);
    if (shared->per_source_states != NULL && hashtable_count(shared->per_source_states) > 0) {
        struct hashtable_itr *itr = hashtable_iterator(shared->per_source_states);
        if (itr != NULL) {
            do {
                ratelimit_ps_state_t *state = (ratelimit_ps_state_t *)hashtable_iterator_value(itr);
                if (state == NULL || state->dropped == 0) continue;
                for (unsigned int i = 0; i < topn; ++i) {
                    if (state->dropped > values[i]) {
                        for (unsigned int j = topn - 1; j > i; --j) {
                            values[j] = values[j - 1];
                            keys[j] = keys[j - 1];
                        }
                        values[i] = state->dropped;
                        keys[i] = state->key;
                        break;
                    }
                }
            } while (hashtable_iterator_advance(itr));
            free(itr);
        }
    }
    for (unsigned int i = 0; i < topn; ++i) {
        if (keys[i] != NULL) {
            keys[i] = strdup(keys[i]);
        }
    }
    pthread_mutex_unlock(&shared->per_source_mut);

    if (shared->per_source_top_ctrs == NULL) {
        shared->per_source_top_ctrs = calloc(topn, sizeof(ctr_t *));
        shared->per_source_top_values = calloc(topn, sizeof(intctr_t));
        shared->per_source_top_keys = calloc(topn, sizeof(char *));
        if (shared->per_source_top_ctrs == NULL || shared->per_source_top_values == NULL ||
            shared->per_source_top_keys == NULL) {
            free(values);
            for (unsigned int i = 0; i < topn; ++i) free(keys[i]);
            free(keys);
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

    free(values);
    for (unsigned int i = 0; i < topn; ++i) free(keys[i]);
    free(keys);
}

static void ratelimitPerSourceStatsRead(statsobj_t *stats, void *ctx) {
    ratelimit_shared_t *shared = (ratelimit_shared_t *)ctx;
    (void)stats;
    ratelimitPerSourceUpdateTopN(shared);
}

static rsRetVal ratelimitInitPerSourceShared(ratelimit_shared_t *shared,
                                             ratelimit_ps_policy_t *policy,
                                             const char *policy_file,
                                             unsigned int max_states,
                                             unsigned int topn) {
    char statname[256];
    DEFiRet;

    if (shared == NULL || policy == NULL || policy_file == NULL) ABORT_FINALIZE(RS_RET_PARAM_ERROR);

    shared->per_source_enabled = 1;
    shared->per_source_policy_file = strdup(policy_file);
    if (shared->per_source_policy_file == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    shared->per_source_default_max = policy->default_max;
    shared->per_source_default_window = policy->default_window;
    shared->per_source_overrides = policy->overrides;
    shared->per_source_key_tpl_default = 0;
    shared->per_source_key_tpl_name = NULL;
    shared->per_source_key_tpl = NULL;
    shared->per_source_key_needs_parsing = 1;
    shared->per_source_key_mode = RL_PS_KEY_TPL;
    shared->per_source_states = create_hashtable(128, hash_from_string, key_equals_string, ratelimitFreePerSourceState);
    if (shared->per_source_states == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    shared->per_source_max_states = (max_states == 0) ? RATELIMIT_PERSOURCE_DEFAULT_MAX_STATES : max_states;
    shared->per_source_topn = (topn == 0) ? RATELIMIT_PERSOURCE_DEFAULT_TOPN : topn;
    pthread_mutex_init(&shared->per_source_mut, NULL);

    STATSCOUNTER_INIT(shared->ctrPerSourceAllowed, shared->mutCtrPerSourceAllowed);
    STATSCOUNTER_INIT(shared->ctrPerSourceDropped, shared->mutCtrPerSourceDropped);

    CHKiRet(statsobj.Construct(&shared->per_source_stats));
    snprintf(statname, sizeof(statname), "ratelimit.%s.per_source", shared->name);
    statname[sizeof(statname) - 1] = '\0';
    CHKiRet(statsobj.SetName(shared->per_source_stats, (uchar *)statname));
    CHKiRet(statsobj.SetOrigin(shared->per_source_stats, (uchar *)"ratelimit"));
    CHKiRet(statsobj.AddCounter(shared->per_source_stats, UCHAR_CONSTANT("per_source_allowed"), ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &shared->ctrPerSourceAllowed));
    CHKiRet(statsobj.AddCounter(shared->per_source_stats, UCHAR_CONSTANT("per_source_dropped"), ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &shared->ctrPerSourceDropped));
    CHKiRet(statsobj.SetReadNotifier(shared->per_source_stats, ratelimitPerSourceStatsRead, shared));
    CHKiRet(statsobj.ConstructFinalize(shared->per_source_stats));

finalize_it:
    if (iRet != RS_RET_OK) {
        if (shared->per_source_stats != NULL) statsobj.Destruct(&shared->per_source_stats);
        if (shared->per_source_states != NULL) hashtable_destroy(shared->per_source_states, 1);
        if (shared->per_source_overrides != NULL) hashtable_destroy(shared->per_source_overrides, 1);
        free(shared->per_source_policy_file);
        shared->per_source_policy_file = NULL;
        pthread_mutex_destroy(&shared->per_source_mut);
        shared->per_source_enabled = 0;
    }
    return iRet;
}

static void ratelimitSwapPerSourcePolicy(ratelimit_shared_t *shared, ratelimit_ps_policy_t *policy) {
    if (shared == NULL || policy == NULL) return;
    pthread_mutex_lock(&shared->per_source_mut);
    if (shared->per_source_overrides != NULL) {
        hashtable_destroy(shared->per_source_overrides, 1);
    }
    shared->per_source_overrides = policy->overrides;
    shared->per_source_default_max = policy->default_max;
    shared->per_source_default_window = policy->default_window;
    pthread_mutex_unlock(&shared->per_source_mut);
}

static rsRetVal ratelimitPerSourceCheck(ratelimit_t *ratelimit, const char *key, size_t key_len, time_t tt) {
    ratelimit_shared_t *shared;
    ratelimit_ps_state_t *state;
    ratelimit_ps_override_t *override;
    unsigned int max;
    unsigned int window;
    sbool must_drop = 0;
    DEFiRet;

    if (ratelimit == NULL || ratelimit->pShared == NULL || key == NULL || key_len == 0) {
        FINALIZE;
    }
    shared = ratelimit->pShared;
    if (!shared->per_source_enabled) FINALIZE;

    if (tt == 0) tt = time(NULL);

    pthread_mutex_lock(&shared->per_source_mut);
    state = (ratelimit_ps_state_t *)hashtable_search(shared->per_source_states, (void *)key);
    if (state == NULL) {
        if (shared->per_source_max_states > 0 &&
            hashtable_count(shared->per_source_states) >= shared->per_source_max_states) {
            ratelimit_ps_state_t *evict = shared->per_source_lru_head;
            if (evict != NULL) {
                shared->per_source_lru_head = evict->lru_next;
                if (shared->per_source_lru_head != NULL) shared->per_source_lru_head->lru_prev = NULL;
                if (shared->per_source_lru_tail == evict) shared->per_source_lru_tail = NULL;
                ratelimit_ps_state_t *evicted =
                    (ratelimit_ps_state_t *)hashtable_remove(shared->per_source_states, (void *)evict->key);
                ratelimitFreePerSourceState(evicted);
            }
        }
        state = calloc(1, sizeof(*state));
        if (state != NULL) {
            state->key = strdup(key);
            state->window_start = tt;
            state->last_seen = tt;
            if (state->key != NULL && hashtable_insert(shared->per_source_states, state->key, state) == 0) {
                free(state->key);
                free(state);
                state = NULL;
            } else {
                state->lru_prev = shared->per_source_lru_tail;
                if (shared->per_source_lru_tail != NULL) shared->per_source_lru_tail->lru_next = state;
                shared->per_source_lru_tail = state;
                if (shared->per_source_lru_head == NULL) shared->per_source_lru_head = state;
            }
        }
    } else {
        if (shared->per_source_lru_tail != state) {
            if (state->lru_prev != NULL) state->lru_prev->lru_next = state->lru_next;
            if (state->lru_next != NULL) state->lru_next->lru_prev = state->lru_prev;
            if (shared->per_source_lru_head == state) shared->per_source_lru_head = state->lru_next;
            /* if (shared->per_source_lru_tail == state) - handled by outer if */

            state->lru_prev = shared->per_source_lru_tail;
            state->lru_next = NULL;
            if (shared->per_source_lru_tail != NULL) shared->per_source_lru_tail->lru_next = state;
            shared->per_source_lru_tail = state;
            if (shared->per_source_lru_head == NULL) shared->per_source_lru_head = state;
        }
    }

    if (state == NULL) {
        pthread_mutex_unlock(&shared->per_source_mut);
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    override = (ratelimit_ps_override_t *)hashtable_search(shared->per_source_overrides, (void *)key);
    max = (override != NULL && override->has_max) ? override->max : shared->per_source_default_max;
    window = (override != NULL && override->has_window) ? override->window : shared->per_source_default_window;

    if (max == 0 || window == 0) {
        state->allowed++;
    } else {
        if (state->window_start == 0 || (tt - state->window_start) >= (time_t)window || tt < state->window_start) {
            state->window_start = tt;
            state->count = 0;
        }
        if (state->count < max) {
            state->count++;
            state->allowed++;
        } else {
            state->dropped++;
            must_drop = 1;
        }
    }
    state->last_seen = tt;
    if (must_drop) {
        STATSCOUNTER_INC(shared->ctrPerSourceDropped, shared->mutCtrPerSourceDropped);
    } else {
        STATSCOUNTER_INC(shared->ctrPerSourceAllowed, shared->mutCtrPerSourceAllowed);
    }
    pthread_mutex_unlock(&shared->per_source_mut);

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
                            intTiny severity,
                            const char *policy_file,
                            sbool per_source_enabled,
                            const char *per_source_policy_file,
                            const char *per_source_key_tpl_name,
                            unsigned int per_source_max_states,
                            unsigned int per_source_topn) {
    DEFiRet;
    ratelimit_shared_t *shared = NULL;
    ratelimit_shared_t *existing_shared = NULL;
    char *key = NULL;
    ratelimit_ps_policy_t *per_source_policy = NULL;
    sbool bLocked = 0;


    if (name == NULL) {
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    if (policy_file != NULL) {
#ifdef HAVE_LIBYAML
        iRet = parsePolicyFile(policy_file, &interval, &burst, &severity);
#else
        iRet = parsePolicyFile(policy_file);
#endif
        if (iRet != RS_RET_OK) {
            ABORT_FINALIZE(iRet);
        }
    }

    if (per_source_enabled) {
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

    pthread_rwlock_wrlock(&conf->ratelimit_cfgs.lock);
    bLocked = 1;
    existing_shared = (ratelimit_shared_t *)hashtable_search(conf->ratelimit_cfgs.ht, (void *)name);

    if (existing_shared != NULL) {
        LogError(0, RS_RET_CONFIG_ERROR, "ratelimit: duplicate name '%s' in current config set", name);
        pthread_rwlock_unlock(&conf->ratelimit_cfgs.lock);
        bLocked = 0;
        ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
    }

    /* Not found - Create New */
    CHKmalloc(shared = calloc(1, sizeof(ratelimit_shared_t)));
    CHKmalloc(key = strdup(name));
    shared->name = key;
    shared->interval = interval;
    shared->burst = burst;
    shared->severity = severity;
    if (policy_file) {
        CHKmalloc(shared->policy_file = strdup(policy_file));
    }
    pthread_mutex_init(&shared->mut, NULL);
    if (per_source_enabled) {
        iRet = ratelimitInitPerSourceShared(shared, per_source_policy, per_source_policy_file, per_source_max_states,
                                            per_source_topn);
        free(per_source_policy);
        per_source_policy = NULL;
        CHKiRet(iRet);
        if (per_source_key_tpl_name != NULL) {
            CHKmalloc(shared->per_source_key_tpl_name = strdup(per_source_key_tpl_name));
            shared->per_source_key_tpl =
                tplFind(conf, shared->per_source_key_tpl_name, (int)strlen(shared->per_source_key_tpl_name));
            if (shared->per_source_key_tpl == NULL) {
                LogError(0, RS_RET_CONFIG_ERROR, "ratelimit: perSourceKeyTpl '%s' not found for '%s'",
                         per_source_key_tpl_name, name);
                pthread_rwlock_unlock(&conf->ratelimit_cfgs.lock);
                ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
            }
            shared->per_source_key_tpl_default = 0;
        } else {
            char default_tpl[] = "RSYSLOG_PerSourceKey";
            shared->per_source_key_tpl = tplFind(conf, default_tpl, (int)sizeof(default_tpl) - 1);
            if (shared->per_source_key_tpl == NULL) {
                LogError(0, RS_RET_CONFIG_ERROR,
                         "ratelimit: default perSourceKeyTpl 'RSYSLOG_PerSourceKey' not found for '%s'", name);
                pthread_rwlock_unlock(&conf->ratelimit_cfgs.lock);
                ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
            }
            shared->per_source_key_tpl_default = 1;
        }
        shared->per_source_key_mode = perSourceKeyModeFromTemplate(shared->per_source_key_tpl);
        shared->per_source_key_needs_parsing = (shared->per_source_key_mode == RL_PS_KEY_TPL);
    }

    if (hashtable_insert(conf->ratelimit_cfgs.ht, key, shared) == 0) {
        pthread_rwlock_unlock(&conf->ratelimit_cfgs.lock);
        bLocked = 0;
        LogError(0, RS_RET_OUT_OF_MEMORY, "ratelimit: error inserting config into hashtable");
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    /* shared ownership now in hashtable */
    shared = NULL;
    pthread_rwlock_unlock(&conf->ratelimit_cfgs.lock);
    bLocked = 0;

finalize_it:
    if (bLocked) {
        pthread_rwlock_unlock(&conf->ratelimit_cfgs.lock);
    }
    if (shared != NULL) {
        /* If we are here, shared was allocated but NOT inserted (error case before insert) */
        free(shared->name); /* key was assigned to name */
        free(shared->policy_file);
        if (shared->per_source_overrides != NULL) {
            hashtable_destroy(shared->per_source_overrides, 1);
        }
        if (shared->per_source_states != NULL) {
            hashtable_destroy(shared->per_source_states, 1);
        }
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
        if (shared->per_source_enabled) {
            pthread_mutex_destroy(&shared->per_source_mut);
        }
        free(shared->per_source_policy_file);
        pthread_mutex_destroy(&shared->mut);
        free(shared);
    }
    if (per_source_policy != NULL) {
        if (per_source_policy->overrides != NULL) hashtable_destroy(per_source_policy->overrides, 1);
        free(per_source_policy);
    }

    RETiRet;
}

rsRetVal ratelimitNewFromConfig(
    ratelimit_t **ppThis, rsconf_t *conf, const char *configname, const char *modname, const char *dynname) {
    DEFiRet;
    ratelimit_shared_t *shared;

    if (configname == NULL || conf == NULL) {
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    pthread_rwlock_rdlock(&conf->ratelimit_cfgs.lock);
    shared = (ratelimit_shared_t *)hashtable_search(conf->ratelimit_cfgs.ht, (void *)configname);
    if (shared == NULL) {
        pthread_rwlock_unlock(&conf->ratelimit_cfgs.lock);
        LogError(0, RS_RET_NOT_FOUND, "ratelimit config '%s' not found", configname);
        ABORT_FINALIZE(RS_RET_NOT_FOUND);
    }
    pthread_rwlock_unlock(&conf->ratelimit_cfgs.lock);

    /* We use ratelimitNew to allocate the instance, but we override the shared ptr */
    CHKiRet(ratelimitNew(ppThis, modname, dynname));

    /* Re-pointing to the global shared object */
    /* First, free the default one created by ratelimitNew */
    if ((*ppThis)->bOwnsShared) {
        pthread_mutex_destroy(&(*ppThis)->pShared->mut);
        free((*ppThis)->pShared);
    }

    (*ppThis)->pShared = shared;
    (*ppThis)->bOwnsShared = 0;

finalize_it:
    RETiRet;
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
                 ratelimit->missed, ratelimit->pShared->burst, ratelimit->pShared->interval);
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

    /* Snapshot shared values to ensure consistency during check */
    /* Note: we do not lock pShared->mut for reading as integer reads are atomic enough */
    unsigned int interval = ratelimit->pShared->interval;
    unsigned int burst = ratelimit->pShared->burst;

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
    if (ratelimit->pShared->interval) {
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

    assert(ratelimit != NULL);
    assert(pMsg != NULL);
    assert(ppRepMsg != NULL);
    *ppRepMsg = NULL;

    if (runConf->globals.bReduceRepeatMsgs || ratelimit->pShared->severity > 0) {
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
    if (ratelimit->pShared->interval && (severity >= ratelimit->pShared->severity)) {
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
    return ratelimit->pShared->interval || ratelimit->pShared->per_source_enabled || runConf->globals.bReduceRepeatMsgs;
}


/* add a message to a ratelimiter/multisubmit structure.
 * ratelimiting is automatically handled according to the ratelimit
 * settings.
 * if pMultiSub == NULL, a single-message enqueue happens (under reconsideration)
 */
rsRetVal ATTR_NONNULL(1, 3) ratelimitAddMsg(ratelimit_t *ratelimit, multi_submit_t *pMultiSub, smsg_t *pMsg) {
    rsRetVal localRet;
    smsg_t *repMsg;
    DEFiRet;

    assert(ratelimit != NULL);
    assert(pMsg != NULL);
    localRet = ratelimitMsg(ratelimit, pMsg, &repMsg);
    if (pMultiSub == NULL) {
        if (repMsg != NULL) CHKiRet(submitMsg2(repMsg));
        CHKiRet(localRet);
        CHKiRet(submitMsg2(pMsg));
    } else {
        if (repMsg != NULL) {
            pMultiSub->ppMsgs[pMultiSub->nElem++] = repMsg;
            if (pMultiSub->nElem == pMultiSub->maxElem) CHKiRet(multiSubmitMsg2(pMultiSub));
        }
        CHKiRet(localRet);
        if (pMsg->iLenRawMsg > glblGetMaxLine(runConf)) {
            /* oversize message needs special processing. We keep
             * at least the previous batch as batch...
             */
            if (pMultiSub->nElem > 0) {
                CHKiRet(multiSubmitMsg2(pMultiSub));
            }
            CHKiRet(submitMsg2(pMsg));
            FINALIZE;
        }
        pMultiSub->ppMsgs[pMultiSub->nElem++] = pMsg;
        if (pMultiSub->nElem == pMultiSub->maxElem) CHKiRet(multiSubmitMsg2(pMultiSub));
    }

finalize_it:
    RETiRet;
}

rsRetVal ratelimitAddMsgPerSource(ratelimit_t *ratelimit,
                                  multi_submit_t *pMultiSub,
                                  smsg_t *pMsg,
                                  const char *per_source_key,
                                  size_t per_source_key_len,
                                  time_t tt) {
    rsRetVal localRet;
    smsg_t *repMsg = NULL;
    sbool per_source_dropped = 0;
    DEFiRet;

    assert(ratelimit != NULL);
    assert(pMsg != NULL);
    assert(ratelimit->pShared != NULL);

    if (!ratelimit->pShared->per_source_enabled || per_source_key == NULL || per_source_key_len == 0) {
        return ratelimitAddMsg(ratelimit, pMultiSub, pMsg);
    }

    localRet = ratelimitMsg(ratelimit, pMsg, &repMsg);
    if (pMultiSub == NULL) {
        if (repMsg != NULL) CHKiRet(submitMsg2(repMsg));
        CHKiRet(localRet);
        if ((iRet = ratelimitPerSourceCheck(ratelimit, per_source_key, per_source_key_len, tt)) != RS_RET_OK) {
            per_source_dropped = 1;
            ABORT_FINALIZE(iRet);
        }
        CHKiRet(submitMsg2(pMsg));
    } else {
        if (repMsg != NULL) {
            pMultiSub->ppMsgs[pMultiSub->nElem++] = repMsg;
            if (pMultiSub->nElem == pMultiSub->maxElem) CHKiRet(multiSubmitMsg2(pMultiSub));
        }
        CHKiRet(localRet);
        if ((iRet = ratelimitPerSourceCheck(ratelimit, per_source_key, per_source_key_len, tt)) != RS_RET_OK) {
            per_source_dropped = 1;
            ABORT_FINALIZE(iRet);
        }
        if (pMsg->iLenRawMsg > glblGetMaxLine(runConf)) {
            if (pMultiSub->nElem > 0) {
                CHKiRet(multiSubmitMsg2(pMultiSub));
            }
            CHKiRet(submitMsg2(pMsg));
            FINALIZE;
        }
        pMultiSub->ppMsgs[pMultiSub->nElem++] = pMsg;
        if (pMultiSub->nElem == pMultiSub->maxElem) CHKiRet(multiSubmitMsg2(pMultiSub));
    }

finalize_it:
    if (per_source_dropped) {
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
    pthread_mutex_init(&pThis->pShared->mut, NULL);

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
    ratelimit->pShared->interval = interval;
    ratelimit->pShared->burst = burst;
    ratelimit->done = 0;
    ratelimit->missed = 0;
    ratelimit->begin = 0;
}


/* enable thread-safe operations mode. This make sure that
 * a single ratelimiter can be called from multiple threads. As
 * this causes some overhead and is not always required, it needs
 * to be explicitely enabled. This operation cannot be undone
 * (think: why should one do that???)
 */
void ratelimitSetThreadSafe(ratelimit_t *ratelimit) {
    ratelimit->bThreadSafe = 1;
    pthread_mutex_init(&ratelimit->mut, NULL);
}
void ratelimitSetNoTimeCache(ratelimit_t *ratelimit) {
    ratelimit->bNoTimeCache = 1;
    pthread_mutex_init(&ratelimit->mut, NULL);
}

/* Severity level determines which messages are subject to
 * ratelimiting. Default (no value set) is all messages.
 */
void ratelimitSetSeverity(ratelimit_t *ratelimit, intTiny severity) {
    ratelimit->pShared->severity = severity;
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
    if (ratelimit->bThreadSafe) pthread_mutex_destroy(&ratelimit->mut);

    if (ratelimit->bOwnsShared && ratelimit->pShared != NULL) {
        pthread_mutex_destroy(&ratelimit->pShared->mut);
        free(ratelimit->pShared);
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

void ratelimitDoHUP(void) {
    struct hashtable_itr *itr;
    ratelimit_shared_t *shared;
    unsigned int interval, burst;
    intTiny severity;
    rsRetVal iRet;

    if (runConf == NULL || runConf->ratelimit_cfgs.ht == NULL) {
        return;
    }

    /* We iterate the hashtable. Since we are doing HUP, we assume no other thread is modifying
     * the hashtable structure (insert/delete) at this point, which is true for rsyslogd's main loop HUP.
     * We only need to lock the individual shared objects when updating them.
     */
    pthread_rwlock_rdlock(&runConf->ratelimit_cfgs.lock);
    if (hashtable_count(runConf->ratelimit_cfgs.ht) > 0) {
        itr = hashtable_iterator(runConf->ratelimit_cfgs.ht);
        do {
            shared = (ratelimit_shared_t *)hashtable_iterator_value(itr);
            if (shared && shared->policy_file) {
                interval = shared->interval;
                burst = shared->burst;
                severity = shared->severity;

                /* Re-parse the file */
#ifdef HAVE_LIBYAML
                iRet = parsePolicyFile(shared->policy_file, &interval, &burst, &severity);
#else
                iRet = parsePolicyFile(shared->policy_file);
#endif

                if (iRet == RS_RET_OK) {
                    pthread_mutex_lock(&shared->mut);
                    shared->interval = interval;
                    shared->burst = burst;
                    shared->severity = severity;
                    pthread_mutex_unlock(&shared->mut);
                    DBGPRINTF("ratelimit: HUP updated policy '%s' from file '%s'\n", shared->name, shared->policy_file);
                } else {
                    LogError(0, iRet, "ratelimit: HUP failed to reload policy '%s' from file '%s', keeping old values",
                             shared->name, shared->policy_file);
                }
            }
            if (shared && shared->per_source_policy_file) {
                ratelimit_ps_policy_t *policy = NULL;
                iRet = parsePerSourcePolicyFile(shared->per_source_policy_file, &policy);
                if (iRet == RS_RET_OK && policy != NULL) {
                    ratelimitSwapPerSourcePolicy(shared, policy);
                    if (policy->overrides != NULL) {
                        policy->overrides = NULL;
                    }
                    free(policy);
                    DBGPRINTF("ratelimit: HUP updated per-source policy '%s' from file '%s'\n", shared->name,
                              shared->per_source_policy_file);
                } else {
                    LogError(
                        0, iRet,
                        "ratelimit: HUP failed to reload per-source policy '%s' from file '%s', keeping old values",
                        shared->name, shared->per_source_policy_file);
                }
            }
        } while (hashtable_iterator_advance(itr));
        free(itr);
    }
    pthread_rwlock_unlock(&runConf->ratelimit_cfgs.lock);
}
