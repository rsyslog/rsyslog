/* Release B reload-manager foundation.
 *
 * This owns request state, observability, and the active generation's
 * normalized ruleset-plan graph. It does not parse, validate, construct, or
 * activate a candidate configuration generation. Those operations remain
 * unsafe until their process-global lifetime and side effects are isolated.
 * Historic HUP hooks are intentionally outside this manager and remain
 * unconditional.
 */
#include "config.h"

#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

#include "rsyslog.h"
#include "obj.h"
#include "reload-ruleset-graph.h"
#include "shadow_reload.h"
#include "statsobj.h"

DEFobjStaticHelpers;
DEFobjCurrIf(statsobj);

static volatile sig_atomic_t signalRequestPending = 0;
static reloadOnHUPMode_t configuredMode = RELOAD_ON_HUP_OFF;

/* These fields are accessed only by the main thread. They form the baseline
 * that a future generation-owning implementation can preserve and expose. */
static intctr_t requestsTotal = 0;
static intctr_t offTotal = 0;
static intctr_t validateTotal = 0;
static intctr_t onTotal = 0;
static intctr_t rejectedTotal = 0;
static intctr_t rejectedValidateTotal = 0;
static intctr_t rejectedOnTotal = 0;
static intctr_t legacyHookTotal = 0;
static intctr_t durationTotalUsec = 0;
static intctr_t lastDurationUsec = 0;
static int activeGeneration = 0;
static int requestInProgress = 0;
static int pendingGauge = 0;
static uint64_t requestStartedUsec = 0;
static statsobj_t *reloadStats = NULL;
static rsReloadNormalizedGraphBuilderV1_t *activeRulesetGraphBuilder = NULL;
static rsReloadNormalizedGraphV1_t activeRulesetGraph;
/* Scrapeable counters are separate from the always-on log state above. */
STATSCOUNTER_DEF(ctrRequests, mutCtrRequests)
STATSCOUNTER_DEF(ctrOff, mutCtrOff)
STATSCOUNTER_DEF(ctrValidate, mutCtrValidate)
STATSCOUNTER_DEF(ctrOn, mutCtrOn)
STATSCOUNTER_DEF(ctrRejected, mutCtrRejected)
STATSCOUNTER_DEF(ctrRejectedValidate, mutCtrRejectedValidate)
STATSCOUNTER_DEF(ctrRejectedOn, mutCtrRejectedOn)
STATSCOUNTER_DEF(ctrLegacyHooks, mutCtrLegacyHooks)
STATSCOUNTER_DEF(ctrDurationTotalUsec, mutCtrDurationTotalUsec)

static const char *modeName(const reloadOnHUPMode_t mode) {
    switch (mode) {
        case RELOAD_ON_HUP_VALIDATE:
            return "validate";
        case RELOAD_ON_HUP_ON:
            return "on";
        case RELOAD_ON_HUP_OFF:
        default:
            return "off";
    }
}

static uint64_t monotonicUsec(void) {
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return ((uint64_t)ts.tv_sec * 1000000ULL) + (uint64_t)(ts.tv_nsec / 1000ULL);
}

static void logState(const char *const event,
                     const char *const result,
                     const char *const rejectedMode,
                     const char *const rejectedReason) {
    LogMsg(0, !strcmp(result, "rejected") ? RS_RET_ERR : RS_RET_OK,
           !strcmp(result, "rejected") ? LOG_WARNING : LOG_INFO,
           "shadow_reload event=%s result=%s mode=%s requests_total=%" PRIu64 " reload_hup_off_total=%" PRIu64
           " reload_validate_total=%" PRIu64 " reload_on_total=%" PRIu64 " rejected_total=%" PRIu64
           " reload_validate_rejected_total=%" PRIu64 " reload_on_rejected_total=%" PRIu64
           " reload_legacy_hook_total=%" PRIu64
           " in_progress=%d pending=%d active_generation=%u duration_total_usec=%" PRIu64 " last_duration_usec=%" PRIu64
           " rejected_mode=%s rejected_reason=%s",
           event, result, modeName(configuredMode), (uint64_t)requestsTotal, (uint64_t)offTotal,
           (uint64_t)validateTotal, (uint64_t)onTotal, (uint64_t)rejectedTotal, (uint64_t)rejectedValidateTotal,
           (uint64_t)rejectedOnTotal, (uint64_t)legacyHookTotal, requestInProgress, signalRequestPending != 0,
           activeGeneration, (uint64_t)durationTotalUsec, (uint64_t)lastDurationUsec, rejectedMode, rejectedReason);
}

rsRetVal shadowReloadInit(void) {
    DEFiRet;

    CHKiRet(objGetObjInterface(&obj));
    CHKiRet(objUse(statsobj, CORE_COMPONENT));
    STATSCOUNTER_INIT(ctrRequests, mutCtrRequests);
    STATSCOUNTER_INIT(ctrOff, mutCtrOff);
    STATSCOUNTER_INIT(ctrValidate, mutCtrValidate);
    STATSCOUNTER_INIT(ctrOn, mutCtrOn);
    STATSCOUNTER_INIT(ctrRejected, mutCtrRejected);
    STATSCOUNTER_INIT(ctrRejectedValidate, mutCtrRejectedValidate);
    STATSCOUNTER_INIT(ctrRejectedOn, mutCtrRejectedOn);
    STATSCOUNTER_INIT(ctrLegacyHooks, mutCtrLegacyHooks);
    STATSCOUNTER_INIT(ctrDurationTotalUsec, mutCtrDurationTotalUsec);
    CHKiRet(statsobj.Construct(&reloadStats));
    CHKiRet(statsobj.SetName(reloadStats, (uchar *)"reload"));
    CHKiRet(statsobj.SetOrigin(reloadStats, (uchar *)"core.reload"));
    CHKiRet(statsobj.AddCounter(reloadStats, (uchar *)"reload_hup_requests_total", ctrType_IntCtr, CTR_FLAG_NONE,
                                &ctrRequests));
    CHKiRet(statsobj.AddCounter(reloadStats, (uchar *)"reload_hup_off_total", ctrType_IntCtr, CTR_FLAG_NONE, &ctrOff));
    CHKiRet(statsobj.AddCounter(reloadStats, (uchar *)"reload_validate_total", ctrType_IntCtr, CTR_FLAG_NONE,
                                &ctrValidate));
    CHKiRet(statsobj.AddCounter(reloadStats, (uchar *)"reload_on_total", ctrType_IntCtr, CTR_FLAG_NONE, &ctrOn));
    CHKiRet(statsobj.AddCounter(reloadStats, (uchar *)"reload_rejected_total", ctrType_IntCtr, CTR_FLAG_NONE,
                                &ctrRejected));
    CHKiRet(statsobj.AddCounter(reloadStats, (uchar *)"reload_validate_rejected_total", ctrType_IntCtr, CTR_FLAG_NONE,
                                &ctrRejectedValidate));
    CHKiRet(statsobj.AddCounter(reloadStats, (uchar *)"reload_on_rejected_total", ctrType_IntCtr, CTR_FLAG_NONE,
                                &ctrRejectedOn));
    CHKiRet(statsobj.AddCounter(reloadStats, (uchar *)"reload_legacy_hook_total", ctrType_IntCtr, CTR_FLAG_NONE,
                                &ctrLegacyHooks));
    CHKiRet(statsobj.AddCounter(reloadStats, (uchar *)"reload_duration_total_usec", ctrType_IntCtr, CTR_FLAG_NONE,
                                &ctrDurationTotalUsec));
    CHKiRet(statsobj.AddCounter(reloadStats, (uchar *)"reload_active_generation", ctrType_Int, CTR_FLAG_NONE,
                                &activeGeneration));
    CHKiRet(statsobj.AddCounter(reloadStats, (uchar *)"reload_in_progress", ctrType_Int, CTR_FLAG_NONE,
                                &requestInProgress));
    CHKiRet(statsobj.AddCounter(reloadStats, (uchar *)"reload_pending", ctrType_Int, CTR_FLAG_NONE, &pendingGauge));
    CHKiRet(statsobj.ConstructFinalize(reloadStats));
finalize_it:
    if (iRet != RS_RET_OK && reloadStats != NULL) statsobj.Destruct(&reloadStats);
    if (iRet != RS_RET_OK) objRelease(statsobj, CORE_COMPONENT);
    RETiRet;
}

void shadowReloadExit(void) {
    rsReloadNormalizedGraphBuilderV1Destruct(&activeRulesetGraphBuilder);
    if (reloadStats != NULL) statsobj.Destruct(&reloadStats);
    objRelease(statsobj, CORE_COMPONENT);
}

void shadowReloadConfigure(const reloadOnHUPMode_t mode) {
    rsReloadNormalizedGraphBuilderV1_t *newBuilder = NULL;
    rsReloadNormalizedGraphV1_t newGraph;
    rsRetVal graphRet;

    configuredMode = mode;
    activeGeneration = 1;
    pendingGauge = signalRequestPending != 0;
    memset(&newGraph, 0, sizeof(newGraph));
    graphRet = rsReloadRulesetGraphBuildV1(runConf, &newBuilder);
    if (graphRet == RS_RET_OK) {
        graphRet = rsReloadNormalizedGraphBuilderV1GetGraph(newBuilder, &newGraph);
    }
    if (graphRet != RS_RET_OK) {
        rsReloadNormalizedGraphBuilderV1Destruct(&newBuilder);
        LogError(0, graphRet, "shadow_reload: active ruleset graph unavailable");
    } else {
        rsReloadNormalizedGraphBuilderV1Destruct(&activeRulesetGraphBuilder);
        activeRulesetGraphBuilder = newBuilder;
        activeRulesetGraph = newGraph;
    }
    logState("configured", "idle", "none", "none");
}

typedef struct reloadRulesetLookup_s {
    const char *identity;
    const char *fingerprint;
} reloadRulesetLookup_t;

static rsRetVal findReloadRuleset(const rsReloadNormalizedNodeV1_t *node, void *context) {
    reloadRulesetLookup_t *lookup = context;

    if (node->objectKind == RS_RELOAD_OBJ_RULESET && !strcmp(node->identity, lookup->identity)) {
        lookup->fingerprint = node->fingerprint;
    }
    return RS_RET_OK;
}

rsRetVal shadowReloadGetRulesetFingerprint(const char *name, const char **ppFingerprint) {
    reloadRulesetLookup_t lookup;
    char *identity = NULL;
    size_t identityLen;
    DEFiRet;

    if (name == NULL || *name == '\0' || ppFingerprint == NULL || *ppFingerprint != NULL ||
        activeRulesetGraphBuilder == NULL) {
        return RS_RET_PARAM_ERROR;
    }
    if (strlen(name) > SIZE_MAX - sizeof("ruleset:")) return RS_RET_OUT_OF_MEMORY;
    identityLen = sizeof("ruleset:") + strlen(name);
    CHKmalloc(identity = malloc(identityLen));
    snprintf(identity, identityLen, "ruleset:%s", name);
    lookup.identity = identity;
    lookup.fingerprint = NULL;
    CHKiRet(activeRulesetGraph.enumerate(activeRulesetGraph.context, findReloadRuleset, &lookup));
    if (lookup.fingerprint == NULL) ABORT_FINALIZE(RS_RET_NOT_FOUND);
    *ppFingerprint = lookup.fingerprint;

finalize_it:
    free(identity);
    RETiRet;
}

void shadowReloadRequestFromSignal(void) {
    /* Coalesce repeated signals. Do not count or log from signal context. */
    signalRequestPending = 1;
}

void shadowReloadBeginRequest(void) {
    /* This runs in lockstep with bHadHUP before doHUP(). A signal arriving
     * during doHUP() sets this flag again and is retained for the next legacy
     * HUP cycle rather than being cleared while this one completes. */
    signalRequestPending = 0;
    pendingGauge = 0;
    requestInProgress = 1;
    ++requestsTotal;
    STATSCOUNTER_INC(ctrRequests, mutCtrRequests);
    ++legacyHookTotal;
    STATSCOUNTER_INC(ctrLegacyHooks, mutCtrLegacyHooks);
    if (configuredMode == RELOAD_ON_HUP_OFF) {
        ++offTotal;
        STATSCOUNTER_INC(ctrOff, mutCtrOff);
    } else if (configuredMode == RELOAD_ON_HUP_VALIDATE) {
        ++validateTotal;
        STATSCOUNTER_INC(ctrValidate, mutCtrValidate);
    } else {
        ++onTotal;
        STATSCOUNTER_INC(ctrOn, mutCtrOn);
    }
    requestStartedUsec = monotonicUsec();
}

void shadowReloadProcess(void) {
    if (!requestInProgress) {
        return;
    }

    if (configuredMode == RELOAD_ON_HUP_VALIDATE || configuredMode == RELOAD_ON_HUP_ON) {
        ++rejectedTotal;
        STATSCOUNTER_INC(ctrRejected, mutCtrRejected);
        if (configuredMode == RELOAD_ON_HUP_VALIDATE) {
            ++rejectedValidateTotal;
            STATSCOUNTER_INC(ctrRejectedValidate, mutCtrRejectedValidate);
        } else {
            ++rejectedOnTotal;
            STATSCOUNTER_INC(ctrRejectedOn, mutCtrRejectedOn);
        }
        lastDurationUsec = monotonicUsec() - requestStartedUsec;
        durationTotalUsec += lastDurationUsec;
        STATSCOUNTER_ADD(ctrDurationTotalUsec, mutCtrDurationTotalUsec, lastDurationUsec);
        requestInProgress = 0;
        pendingGauge = signalRequestPending != 0;
        logState("request", "rejected", modeName(configuredMode), "unsupported_release_b");
    } else {
        lastDurationUsec = monotonicUsec() - requestStartedUsec;
        durationTotalUsec += lastDurationUsec;
        STATSCOUNTER_ADD(ctrDurationTotalUsec, mutCtrDurationTotalUsec, lastDurationUsec);
        requestInProgress = 0;
        pendingGauge = signalRequestPending != 0;
        logState("request", "ignored", "none", "mode_off");
    }
}
