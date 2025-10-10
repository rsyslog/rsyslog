/* ratelimit.c
 * support for rate-limiting sources, including "last message
 * repeated n times" processing.
 *
 * Copyright 2012-2020 Rainer Gerhards and Adiscon GmbH.
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
#include <ctype.h>
#include <stdint.h>

#include "rsyslog.h"
#include "errmsg.h"
#include "ratelimit.h"
#include "rainerscript.h"
#include "datetime.h"
#include "parser.h"
#include "unicode-helper.h"
#include "msg.h"
#include "rsconf.h"
#include "dirty.h"

/**
 * @brief Overview of the ratelimit architecture.
 *
 * The ratelimit subsystem is structured into distinct layers so that
 * configuration parsing, registry management, and runtime enforcement remain
 * loosely coupled:
 * - During configuration loading the ratelimit() object handler stores each
 *   named policy in the per-configuration registry (a linked list of
 *   ::ratelimit_config_s entries owned by ::rsconf_s). Inline `ratelimit.*`
 *   parameters are promoted to ad-hoc entries in the same registry so they can
 *   be referenced later.
 * - Modules call ::ratelimitResolveFromValues (or ::ratelimitResolveConfig)
 *   whenever they need ratelimit settings. The helper enforces the "shared OR
 *   inline" contract, returning a pointer to the immutable configuration node.
 *   Whenever an ::rsconf_t is available, callers should prefer this path so
 *   the shared registry is populated for future reuse.
 * - At activation time workers create runtime instances via
 *   ::ratelimitNewFromConfig (shared/ad-hoc) when a configuration entry is
 *   available, or via ::ratelimitNew when no registry context exists (for
 *   example during bootstrap or in tooling). The runtime structure keeps the
 *   mutable counters, mutex state, and repeat suppression data while
 *   referencing the registry entry for the immutable parameters whenever
 *   possible.
 *
 * This file implements all three layers: registry maintenance, object parsing
 * and promotion, plus the actual rate-checking and repeat-suppression logic.
 */

/* Implementation note: the public API is documented in ratelimit.h. */

/* definitions for objects we access */
DEFobjStaticHelpers;
DEFobjCurrIf(glbl);
DEFobjCurrIf(datetime);
DEFobjCurrIf(parser);

/**
 * Immutable configuration entry stored inside the per-configuration
 * ratelimit registry. Each node captures the shared interval, burst, and
 * severity settings associated with a named ratelimit() object so runtime
 * instances can reference them by name.
 */
struct ratelimit_config_s {
    char *name;
    unsigned int interval;
    unsigned int burst;
    intTiny severity;
    ratelimit_config_t *next;
    sbool isAdHoc;
};

static ratelimit_config_t *ratelimitFindConfig(const rsconf_t *cnf, const char *name);
static rsRetVal ratelimitConfigValidateSpec(const ratelimit_config_spec_t *spec);
static char *ratelimitBuildInstanceName(const ratelimit_config_t *cfg, const char *instance_name);
static void ratelimitDropSuppressedIfAny(ratelimit_t *ratelimit);
static smsg_t *ratelimitGenRepMsg(ratelimit_t *ratelimit);

static struct cnfparamdescr ratelimitpdescr[] = {
    {"name", eCmdHdlrString, CNFPARAM_REQUIRED},
    {"interval", eCmdHdlrInt, CNFPARAM_REQUIRED},
    {"burst", eCmdHdlrInt, CNFPARAM_REQUIRED},
    {"severity", eCmdHdlrInt, 0},
};
static struct cnfparamblk ratelimitpblk = {CNFPARAMBLK_VERSION, sizeof(ratelimitpdescr) / sizeof(struct cnfparamdescr),
                                           ratelimitpdescr};

/* static data */

/**
 * @brief Implementation of ::ratelimitConfigSpecInit().
 * @see ratelimit.h for the caller contract.
 *
 * Resets the structure to a disabled limiter so callers can selectively
 * override fields without worrying about stale state.
 */
void ratelimitConfigSpecInit(ratelimit_config_spec_t *const spec) {
    if (spec == NULL) return;
    spec->interval = 0;
    spec->burst = 0;
    spec->severity = RATELIMIT_SEVERITY_UNSET;
}

static ratelimit_config_t *ratelimitFindConfig(const rsconf_t *const cnf, const char *const name) {
    ratelimit_config_t *cfg;

    if (cnf == NULL || name == NULL) return NULL;

    for (cfg = cnf->ratelimits.head; cfg != NULL; cfg = cfg->next) {
        if (!strcmp(cfg->name, name)) {
            return cfg;
        }
    }

    return NULL;
}

static rsRetVal ratelimitConfigValidateSpec(const ratelimit_config_spec_t *const spec) {
    DEFiRet;

    if (spec == NULL) {
        ABORT_FINALIZE(RS_RET_INVALID_PARAMS);
    }

    if (spec->interval == 0) {
        /* burst has no meaning when interval is disabled, but zero is fine */
        FINALIZE;
    }

    if (spec->burst == 0) {
        LogError(0, RS_RET_INVALID_VALUE, "ratelimit: burst must be > 0 when interval is enabled");
        ABORT_FINALIZE(RS_RET_INVALID_VALUE);
    }

finalize_it:
    RETiRet;
}

/**
 * @brief Implementation of ::ratelimitStoreInit().
 * @see ratelimit.h for semantics.
 *
 * Sets up the per-configuration linked list so lookups and registrations
 * can operate in O(1) append time.
 */
void ratelimitStoreInit(rsconf_t *const cnf) {
    if (cnf == NULL) return;
    cnf->ratelimits.head = NULL;
    cnf->ratelimits.tail = NULL;
    cnf->ratelimits.next_auto_id = 1;
}

/**
 * @brief Implementation of ::ratelimitStoreDestruct().
 *
 * Walks the store and releases all configuration entries so that a
 * configuration teardown or reload does not leak memory.
 */
void ratelimitStoreDestruct(rsconf_t *const cnf) {
    ratelimit_config_t *cfg;
    ratelimit_config_t *nxt;

    if (cnf == NULL) return;

    cfg = cnf->ratelimits.head;
    while (cfg != NULL) {
        nxt = cfg->next;
        free(cfg->name);
        free(cfg);
        cfg = nxt;
    }

    cnf->ratelimits.head = NULL;
    cnf->ratelimits.tail = NULL;
    cnf->ratelimits.next_auto_id = 1;
}

static ratelimit_config_t *ratelimitConfigAlloc(const ratelimit_config_spec_t *const spec) {
    ratelimit_config_t *cfg = calloc(1, sizeof(*cfg));
    if (cfg == NULL) return NULL;

    cfg->interval = spec->interval;
    cfg->burst = spec->burst;
    cfg->severity = (spec->severity == RATELIMIT_SEVERITY_UNSET) ? 0 : (intTiny)spec->severity;
    cfg->isAdHoc = 0;
    return cfg;
}

static rsRetVal ratelimitConfigRegister(rsconf_t *const cnf, ratelimit_config_t *const cfg) {
    if (cnf->ratelimits.tail == NULL) {
        cnf->ratelimits.head = cfg;
    } else {
        cnf->ratelimits.tail->next = cfg;
    }
    cnf->ratelimits.tail = cfg;
    return RS_RET_OK;
}

/**
 * @brief Implementation of ::ratelimitConfigCreateNamed().
 *
 * Performs duplicate checking and list registration before returning the
 * immutable configuration to callers.
 */
rsRetVal ratelimitConfigCreateNamed(rsconf_t *const cnf,
                                    const char *const name,
                                    const ratelimit_config_spec_t *const spec,
                                    ratelimit_config_t **const cfg_out) {
    ratelimit_config_t *cfg = NULL;
    DEFiRet;

    if (cnf == NULL || name == NULL || spec == NULL || cfg_out == NULL) {
        ABORT_FINALIZE(RS_RET_INVALID_PARAMS);
    }

    CHKiRet(ratelimitConfigValidateSpec(spec));

    if (ratelimitFindConfig(cnf, name) != NULL) {
        LogError(0, RS_RET_INVALID_VALUE, "ratelimit '%s' is defined more than once", name);
        ABORT_FINALIZE(RS_RET_INVALID_VALUE);
    }

    CHKmalloc(cfg = ratelimitConfigAlloc(spec));
    CHKmalloc(cfg->name = strdup(name));

    CHKiRet(ratelimitConfigRegister(cnf, cfg));

    *cfg_out = cfg;
    cfg = NULL;

finalize_it:
    if (cfg != NULL) {
        free(cfg->name);
        free(cfg);
    }
    RETiRet;
}

static void ratelimitSanitizeHint(const char *const hint, char *buf, const size_t buflen) {
    size_t len = 0;
    const char *src = hint;

    if (buflen == 0) return;

    if (src == NULL || *src == '\0') {
        if (buflen > 1) {
            buf[0] = 'r';
            buf[1] = '\0';
        } else {
            buf[0] = '\0';
        }
        return;
    }

    while (*src != '\0' && len + 1 < buflen) {
        unsigned char ch = (unsigned char)*src;
        if (isalnum(ch) || ch == '-' || ch == '_') {
            buf[len++] = (char)tolower(ch);
        } else {
            buf[len++] = '_';
        }
        ++src;
    }
    buf[len] = '\0';
    if (len == 0 && buflen > 1) {
        buf[0] = 'r';
        buf[1] = '\0';
    }
}

/**
 * @brief Implementation of ::ratelimitConfigCreateAdHoc().
 *
 * Synthesises a unique name while reusing the same allocation path as
 * explicitly named objects.
 */
rsRetVal ratelimitConfigCreateAdHoc(rsconf_t *const cnf,
                                    const char *const hint,
                                    const ratelimit_config_spec_t *const spec,
                                    ratelimit_config_t **const cfg_out) {
    char namebuf[256];
    char sanitized[128];
    ratelimit_config_t *cfg = NULL;
    DEFiRet;

    if (cnf == NULL || spec == NULL || cfg_out == NULL) {
        ABORT_FINALIZE(RS_RET_INVALID_PARAMS);
    }

    CHKiRet(ratelimitConfigValidateSpec(spec));

    ratelimitSanitizeHint(hint, sanitized, sizeof(sanitized));
    snprintf(namebuf, sizeof(namebuf), "adhoc.%s.%llu", sanitized, (unsigned long long)cnf->ratelimits.next_auto_id++);

    CHKiRet(ratelimitConfigCreateNamed(cnf, namebuf, spec, &cfg));
    cfg->isAdHoc = 1;
    *cfg_out = cfg;

finalize_it:
    RETiRet;
}

/**
 * @brief Implementation of ::ratelimitConfigLookup().
 *
 * Provides a NULL-safe wrapper around the linear search through the
 * configuration list.
 */
rsRetVal ratelimitConfigLookup(const rsconf_t *const cnf, const char *const name, ratelimit_config_t **const cfg) {
    ratelimit_config_t *found;
    DEFiRet;

    if (cfg == NULL) {
        ABORT_FINALIZE(RS_RET_INVALID_PARAMS);
    }

    *cfg = NULL;

    if (cnf == NULL || name == NULL) {
        ABORT_FINALIZE(RS_RET_NOT_FOUND);
    }

    found = ratelimitFindConfig(cnf, name);
    if (found == NULL) {
        ABORT_FINALIZE(RS_RET_NOT_FOUND);
    }

    *cfg = found;

finalize_it:
    RETiRet;
}

/**
 * @brief Implementation of ::ratelimitConfigName().
 *
 * Returns the stored identifier so diagnostics can mention the
 * configuration that was used.
 */
const char *ratelimitConfigName(const ratelimit_config_t *const cfg) {
    return (cfg == NULL) ? NULL : cfg->name;
}

/**
 * @brief Implementation of ::ratelimitConfigGetInterval().
 *
 * Provides callers with the resolved interval while gracefully handling
 * NULL configuration pointers.
 */
unsigned int ratelimitConfigGetInterval(const ratelimit_config_t *const cfg) {
    return (cfg == NULL) ? 0 : cfg->interval;
}

/**
 * @brief Implementation of ::ratelimitConfigGetBurst().
 *
 * Mirrors ::ratelimitConfigGetInterval() for the burst field.
 */
unsigned int ratelimitConfigGetBurst(const ratelimit_config_t *const cfg) {
    return (cfg == NULL) ? 0 : cfg->burst;
}

/**
 * @brief Implementation of ::ratelimitConfigGetSeverity().
 *
 * Exposes the stored severity threshold while defaulting to zero when no
 * configuration is supplied.
 */
int ratelimitConfigGetSeverity(const ratelimit_config_t *const cfg) {
    return (cfg == NULL) ? 0 : cfg->severity;
}

/**
 * @brief Implementation of ::ratelimitResolveConfig().
 *
 * Centralises the legacy-parameter guard rails so all call sites get
 * consistent error reporting.
 */
rsRetVal ratelimitResolveConfig(rsconf_t *const cnf,
                                const char *const hint,
                                const char *const name,
                                const sbool legacyParamsSpecified,
                                const ratelimit_config_spec_t *const spec,
                                ratelimit_config_t **const cfg_out) {
    DEFiRet;

    if (cfg_out == NULL || cnf == NULL || spec == NULL) {
        ABORT_FINALIZE(RS_RET_INVALID_PARAMS);
    }

    if (name != NULL && *name != '\0') {
        if (legacyParamsSpecified) {
            LogError(0, RS_RET_INVALID_VALUE,
                     "ratelimit '%s': ratelimit.name cannot be combined with inline ratelimit.* parameters", name);
            ABORT_FINALIZE(RS_RET_INVALID_VALUE);
        }
        CHKiRet(ratelimitConfigLookup(cnf, name, cfg_out));
    } else {
        CHKiRet(ratelimitConfigCreateAdHoc(cnf, hint, spec, cfg_out));
    }

finalize_it:
    RETiRet;
}

/**
 * @brief Implementation of ::ratelimitResolveFromValues().
 *
 * Promotes inline parameters to shared configurations so dynamically
 * created instances can reuse the same immutable spec.
 */
rsRetVal ratelimitResolveFromValues(rsconf_t *const cnf,
                                    const char *const hint,
                                    const char *const name,
                                    const sbool legacyParamsSpecified,
                                    unsigned int *const interval,
                                    unsigned int *const burst,
                                    int *const severity,
                                    ratelimit_config_t **const cfg_out) {
    ratelimit_config_spec_t spec;
    DEFiRet;

    if (interval == NULL || burst == NULL) {
        ABORT_FINALIZE(RS_RET_INVALID_PARAMS);
    }

    ratelimitConfigSpecInit(&spec);
    spec.interval = *interval;
    spec.burst = *burst;
    if (severity != NULL) spec.severity = (*severity < 0) ? RATELIMIT_SEVERITY_UNSET : *severity;

    CHKiRet(ratelimitResolveConfig(cnf, hint, name, legacyParamsSpecified, &spec, cfg_out));

    *interval = ratelimitConfigGetInterval(*cfg_out);
    *burst = ratelimitConfigGetBurst(*cfg_out);
    if (severity != NULL) *severity = ratelimitConfigGetSeverity(*cfg_out);

finalize_it:
    RETiRet;
}

static char *ratelimitBuildInstanceName(const ratelimit_config_t *const cfg, const char *const instance_name) {
    if (instance_name != NULL && *instance_name != '\0') {
        return strdup(instance_name);
    }
    if (cfg != NULL && cfg->name != NULL) {
        return strdup(cfg->name);
    }
    return strdup("ratelimit");
}

static void ratelimitDropSuppressedIfAny(ratelimit_t *const ratelimit) {
    if (ratelimit->pMsg != NULL) {
        if (ratelimit->nsupp > 0) {
            smsg_t *rep = ratelimitGenRepMsg(ratelimit);
            if (rep != NULL) submitMsg2(rep);
        }
        msgDestruct(&ratelimit->pMsg);
    }
}

static inline unsigned int ratelimitEffectiveInterval(const ratelimit_t *const ratelimit) {
    if (ratelimit->has_override) {
        return ratelimit->interval_override;
    }
    return (ratelimit->cfg != NULL) ? ratelimit->cfg->interval : 0;
}

static inline unsigned int ratelimitEffectiveBurst(const ratelimit_t *const ratelimit) {
    if (ratelimit->has_override) {
        return ratelimit->burst_override;
    }
    return (ratelimit->cfg != NULL) ? ratelimit->cfg->burst : 0;
}

static inline intTiny ratelimitEffectiveSeverity(const ratelimit_t *const ratelimit) {
    if (ratelimit->has_override) {
        return ratelimit->severity_override;
    }
    return (ratelimit->cfg != NULL) ? ratelimit->cfg->severity : 0;
}

/**
 * @brief Implementation of ::ratelimitProcessCnf().
 *
 * Parses the RainerScript object definition and feeds the resulting spec
 * into the configuration store.
 */
rsRetVal ratelimitProcessCnf(struct cnfobj *const o) {
    struct cnfparamvals *pvals = NULL;
    ratelimit_config_spec_t spec;
    ratelimit_config_t *cfg = NULL;
    char *name = NULL;
    DEFiRet;

    if (o == NULL) {
        ABORT_FINALIZE(RS_RET_INVALID_PARAMS);
    }

    ratelimitConfigSpecInit(&spec);

    pvals = nvlstGetParams(o->nvlst, &ratelimitpblk, NULL);
    if (pvals == NULL) {
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }

    for (short i = 0; i < ratelimitpblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;
        const char *const pname = ratelimitpblk.descr[i].name;
        if (!strcmp(pname, "name")) {
            free(name);
            name = es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(pname, "interval")) {
            const long long val = pvals[i].val.d.n;
            if (val < 0) {
                LogError(0, RS_RET_INVALID_VALUE, "ratelimit: interval must be >= 0");
                ABORT_FINALIZE(RS_RET_INVALID_VALUE);
            }
            spec.interval = (unsigned int)val;
        } else if (!strcmp(pname, "burst")) {
            const long long val = pvals[i].val.d.n;
            if (val < 0) {
                LogError(0, RS_RET_INVALID_VALUE, "ratelimit: burst must be >= 0");
                ABORT_FINALIZE(RS_RET_INVALID_VALUE);
            }
            spec.burst = (unsigned int)val;
        } else if (!strcmp(pname, "severity")) {
            const long long val = pvals[i].val.d.n;
            if (val < 0 || val > 7) {
                LogError(0, RS_RET_INVALID_VALUE, "ratelimit: severity must be between 0 and 7");
                ABORT_FINALIZE(RS_RET_INVALID_VALUE);
            }
            spec.severity = (int)val;
        }
    }

    if (name == NULL) {
        LogError(0, RS_RET_INVALID_VALUE, "ratelimit: name parameter is required");
        ABORT_FINALIZE(RS_RET_INVALID_VALUE);
    }

    CHKiRet(ratelimitConfigCreateNamed(loadConf, name, &spec, &cfg));
    cfg = NULL;

finalize_it:
    free(name);
    cnfparamvalsDestruct(pvals, &ratelimitpblk);
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
static void tellLostCnt(ratelimit_t *const ratelimit) {
    uchar msgbuf[1024];
    if (ratelimit->missed) {
        const unsigned int burst = ratelimitEffectiveBurst(ratelimit);
        const unsigned int interval = ratelimitEffectiveInterval(ratelimit);
        snprintf((char *)msgbuf, sizeof(msgbuf),
                 "%s: %u messages lost due to rate-limiting (%u allowed within %u seconds)", ratelimit->name,
                 ratelimit->missed, burst, interval);
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
    const unsigned int interval = ratelimitEffectiveInterval(ratelimit);
    const unsigned int burst = ratelimitEffectiveBurst(ratelimit);

    if (ratelimit->bThreadSafe) {
        pthread_mutex_lock(&ratelimit->mut);
    }

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

    assert(burst != 0);

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
/**
 * @brief Implementation of ::ratelimitMsgCount().
 *
 * Separates the pure rate-limit code path from the repeat-message logic so
 * callers that do not need suppression can still share the limiter core.
 */
rsRetVal ratelimitMsgCount(ratelimit_t *__restrict__ const ratelimit, time_t tt, const char *const appname) {
    DEFiRet;
    if (ratelimitEffectiveInterval(ratelimit)) {
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
/**
 * @brief Implementation of ::ratelimitMsg().
 *
 * Applies both counting and repeat suppression while honouring the
 * severity threshold documented in the header.
 */
rsRetVal ratelimitMsg(ratelimit_t *__restrict__ const ratelimit, smsg_t *pMsg, smsg_t **ppRepMsg) {
    DEFiRet;
    rsRetVal localRet;
    int severity = 0;
    const intTiny severityThreshold = ratelimitEffectiveSeverity(ratelimit);
    const unsigned int interval = ratelimitEffectiveInterval(ratelimit);

    *ppRepMsg = NULL;

    if (runConf->globals.bReduceRepeatMsgs || severityThreshold > 0) {
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
    if (interval && (severity >= severityThreshold)) {
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

/**
 * @brief Implementation of ::ratelimitChecked().
 */
int ratelimitChecked(ratelimit_t *ratelimit) {
    if (ratelimit == NULL) return 0;
    return ratelimitEffectiveInterval(ratelimit) || runConf->globals.bReduceRepeatMsgs;
}


/* add a message to a ratelimiter/multisubmit structure.
 * ratelimiting is automatically handled according to the ratelimit
 * settings.
 * if pMultiSub == NULL, a single-message enqueue happens (under reconsideration)
 */
/**
 * @brief Implementation of ::ratelimitAddMsg().
 *
 * Bridges the limiter with the multi-submit batching helper while keeping
 * the caller oblivious to repeat messages.
 */
rsRetVal ratelimitAddMsg(ratelimit_t *ratelimit, multi_submit_t *pMultiSub, smsg_t *pMsg) {
    rsRetVal localRet;
    smsg_t *repMsg;
    DEFiRet;

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


/**
 * @brief Implementation of ::ratelimitNew().
 *
 * Constructs a legacy inline-configured limiter with a human-readable
 * diagnostic name.
 */
rsRetVal ratelimitNew(ratelimit_t **ppThis, const char *modname, const char *dynname) {
    ratelimit_t *pThis = NULL;
    char namebuf[256];
    DEFiRet;

    if (ppThis == NULL) {
        ABORT_FINALIZE(RS_RET_INVALID_PARAMS);
    }

    CHKmalloc(pThis = calloc(1, sizeof(*pThis)));
    pThis->cfg = NULL;
    pThis->interval_override = 0;
    pThis->burst_override = 0;
    pThis->severity_override = 0;
    pThis->has_override = 0;

    if (modname == NULL) modname = "*ERROR:MODULE NAME MISSING*";

    if (dynname == NULL) {
        CHKmalloc(pThis->name = strdup(modname));
    } else {
        snprintf(namebuf, sizeof(namebuf), "%s[%s]", modname, dynname);
        namebuf[sizeof(namebuf) - 1] = '\0';
        CHKmalloc(pThis->name = strdup(namebuf));
    }

    DBGPRINTF("ratelimit:%s:new ratelimiter\n", pThis->name);
    *ppThis = pThis;
    pThis = NULL;

finalize_it:
    if (pThis != NULL) {
        free(pThis->name);
        free(pThis);
    }
    RETiRet;
}
/**
 * @brief Implementation of ::ratelimitNewFromConfig().
 *
 * Binds a freshly allocated limiter to an immutable configuration entry
 * and gives it a meaningful diagnostic name.
 */
rsRetVal ratelimitNewFromConfig(ratelimit_t **ppThis,
                                const ratelimit_config_t *const cfg,
                                const char *const instance_name) {
    ratelimit_t *pThis = NULL;
    char *name = NULL;
    DEFiRet;

    if (ppThis == NULL || cfg == NULL) {
        ABORT_FINALIZE(RS_RET_INVALID_PARAMS);
    }

    CHKmalloc(pThis = calloc(1, sizeof(*pThis)));
    pThis->cfg = cfg;
    pThis->interval_override = 0;
    pThis->burst_override = 0;
    pThis->severity_override = 0;
    pThis->has_override = 0;

    name = ratelimitBuildInstanceName(cfg, instance_name);
    if (name == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }
    pThis->name = name;
    name = NULL;

    DBGPRINTF("ratelimit:%s:new ratelimiter\n", pThis->name);
    *ppThis = pThis;
    pThis = NULL;

finalize_it:
    free(name);
    if (pThis != NULL) {
        free(pThis->name);
        free(pThis);
    }
    RETiRet;
}


/* enable thread-safe operations mode. This make sure that
 * a single ratelimiter can be called from multiple threads. As
 * this causes some overhead and is not always required, it needs
 * to be explicitely enabled. This operation cannot be undone
 * (think: why should one do that???)
 */
/**
 * @brief Implementation of ::ratelimitSetThreadSafe().
 *
 * Initialises the mutex that protects shared counters so a single limiter
 * instance can be shared across worker threads.
 */
void ratelimitSetThreadSafe(ratelimit_t *ratelimit) {
    ratelimit->bThreadSafe = 1;
    pthread_mutex_init(&ratelimit->mut, NULL);
}
/**
 * @brief Implementation of ::ratelimitSetLinuxLike().
 *
 * Reconfigures the limiter with inline interval/burst values emulating the
 * kernel rate-limiter defaults.
 */
void ratelimitSetLinuxLike(ratelimit_t *ratelimit, unsigned int interval, unsigned int burst) {
    if (ratelimit == NULL) return;
    ratelimit->cfg = NULL;
    ratelimit->interval_override = interval;
    ratelimit->burst_override = burst;
    ratelimit->done = 0;
    ratelimit->missed = 0;
    ratelimit->begin = 0;
    ratelimit->has_override = 1;
}
/**
 * @brief Implementation of ::ratelimitSetNoTimeCache().
 *
 * Enables source-provided timestamps by forcing real-time queries instead
 * of relying on cached dispatcher time.
 */
void ratelimitSetNoTimeCache(ratelimit_t *ratelimit) {
    ratelimit->bNoTimeCache = 1;
    pthread_mutex_init(&ratelimit->mut, NULL);
}
/**
 * @brief Implementation of ::ratelimitSetSeverity().
 *
 * Stores the legacy inline severity override and marks the instance as
 * detached from shared configuration state.
 */
void ratelimitSetSeverity(ratelimit_t *ratelimit, intTiny severity) {
    if (ratelimit == NULL) return;
    ratelimit->cfg = NULL;
    ratelimit->severity_override = severity;
    ratelimit->has_override = 1;
}

/**
 * @brief Implementation of ::ratelimitDestruct().
 *
 * Ensures any pending repeat message summary is flushed before freeing
 * the limiter state.
 */
void ratelimitDestruct(ratelimit_t *ratelimit) {
    if (ratelimit == NULL) return;
    ratelimitDropSuppressedIfAny(ratelimit);
    tellLostCnt(ratelimit);
    if (ratelimit->bThreadSafe) pthread_mutex_destroy(&ratelimit->mut);
    free(ratelimit->name);
    free(ratelimit);
}

/**
 * @brief Implementation of ::ratelimitModExit().
 *
 * Releases the shared interfaces acquired in ::ratelimitModInit().
 */
void ratelimitModExit(void) {
    objRelease(datetime, CORE_COMPONENT);
    objRelease(glbl, CORE_COMPONENT);
    objRelease(parser, CORE_COMPONENT);
}

/**
 * @brief Implementation of ::ratelimitModInit().
 *
 * Registers the helper interfaces required during runtime operation so the
 * limiter can parse messages and write diagnostics.
 */
rsRetVal ratelimitModInit(void) {
    DEFiRet;
    CHKiRet(objGetObjInterface(&obj));
    CHKiRet(objUse(glbl, CORE_COMPONENT));
    CHKiRet(objUse(datetime, CORE_COMPONENT));
    CHKiRet(objUse(parser, CORE_COMPONENT));
finalize_it:
    RETiRet;
}
