/* Release B reload-manager foundation.
 *
 * This owns request state, observability, and the active generation's
 * source-syntactic configuration graph. It parses candidates into an owned
 * representation without dispatching configuration objects into
 * modules or runtime globals. Semantic preparation and activation remain
 * disabled. Historic HUP hooks are outside this manager and unconditional.
 */
#include "config.h"

#include <inttypes.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

#include "rsyslog.h"
#include "obj.h"
#include "reload-candidate.h"
#include "reload-report.h"
#include "reload-ruleset-materializer.h"
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
static intctr_t capabilityRejectedTotal = 0;
static intctr_t legacyHookTotal = 0;
static intctr_t durationTotalUsec = 0;
static intctr_t lastDurationUsec = 0;
static unsigned activeGeneration = 0;
static int requestInProgress = 0;
static int pendingGauge = 0;
static uint64_t requestStartedUsec = 0;
static statsobj_t *reloadStats = NULL;
static rsReloadNormalizedGraphBuilderV1_t *activeRulesetGraphBuilder = NULL;
static rsReloadNormalizedGraphV1_t activeRulesetGraph;
static char *candidateConfigPath = NULL;
static rsReloadCandidate_t *pendingCandidate = NULL;
static rsReloadReportV1_t *pendingReport = NULL;
static rsReloadRulesetPlanV1_t *pendingPlan = NULL;
static rsRetVal pendingCandidateResult = RS_RET_OK;
static size_t pendingCandidateObjects = 0;
static uint64_t pendingReportHash = 0;
static size_t lastUnchangedCount = 0;
static size_t lastAddedCount = 0;
static size_t lastRemovedCount = 0;
static size_t lastModifiedCount = 0;
static size_t lastInvalidCount = 0;
static pthread_mutex_t statusMut = PTHREAD_MUTEX_INITIALIZER;
static int baselineAvailable = 0;
enum shadowReloadResult_e {
    SHADOW_RELOAD_IDLE = 0,
    SHADOW_RELOAD_IN_PROGRESS,
    SHADOW_RELOAD_IGNORED,
    SHADOW_RELOAD_REPORTED,
    SHADOW_RELOAD_REJECTED_IO,
    SHADOW_RELOAD_REJECTED_RESOURCE,
    SHADOW_RELOAD_REJECTED_INTERNAL,
    SHADOW_RELOAD_REJECTED_PARSE,
    SHADOW_RELOAD_REJECTED_NORMALIZE,
    SHADOW_RELOAD_REJECTED_BASELINE,
    SHADOW_RELOAD_REJECTED_REPORT,
    SHADOW_RELOAD_REJECTED_CAPABILITY,
    SHADOW_RELOAD_REJECTED_ACTIVATION
};
static int lastResult = SHADOW_RELOAD_IDLE;
enum shadowReloadFailurePhase_e {
    SHADOW_RELOAD_FAILURE_NONE = 0,
    SHADOW_RELOAD_FAILURE_PARSE,
    SHADOW_RELOAD_FAILURE_NORMALIZE,
    SHADOW_RELOAD_FAILURE_BASELINE,
    SHADOW_RELOAD_FAILURE_REPORT,
    SHADOW_RELOAD_FAILURE_CAPABILITY
};
static enum shadowReloadFailurePhase_e pendingFailurePhase = SHADOW_RELOAD_FAILURE_NONE;

static void publishStatus(const int result, const rsReloadReportV1_t *const report) {
    pthread_mutex_lock(&statusMut);
    lastResult = result;
    lastUnchangedCount = report == NULL ? 0 : report->unchangedCount;
    lastAddedCount = report == NULL ? 0 : report->addedCount;
    lastRemovedCount = report == NULL ? 0 : report->removedCount;
    lastModifiedCount = report == NULL ? 0 : report->modifiedCount;
    lastInvalidCount = report == NULL ? 0 : report->invalidCount;
    pthread_mutex_unlock(&statusMut);
}
/* Scrapeable counters are separate from the always-on log state above. */
STATSCOUNTER_DEF(ctrRequests, mutCtrRequests)
STATSCOUNTER_DEF(ctrOff, mutCtrOff)
STATSCOUNTER_DEF(ctrValidate, mutCtrValidate)
STATSCOUNTER_DEF(ctrOn, mutCtrOn)
STATSCOUNTER_DEF(ctrRejected, mutCtrRejected)
STATSCOUNTER_DEF(ctrRejectedValidate, mutCtrRejectedValidate)
STATSCOUNTER_DEF(ctrRejectedOn, mutCtrRejectedOn)
STATSCOUNTER_DEF(ctrCapabilityRejected, mutCtrCapabilityRejected)
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

static sbool monotonicUsec(uint64_t *const value) {
    struct timespec ts;

    if (value == NULL || clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    *value = ((uint64_t)ts.tv_sec * 1000000ULL) + (uint64_t)(ts.tv_nsec / 1000ULL);
    return 1;
}

static void logState(const char *const event,
                     const char *const result,
                     const char *const rejectedMode,
                     const char *const rejectedReason,
                     const size_t candidateObjects,
                     const rsReloadReportV1_t *const report,
                     const uint64_t reportHashValue) {
    LogMsg(0, !strcmp(result, "rejected") ? RS_RET_ERR : RS_RET_OK,
           !strcmp(result, "rejected") ? LOG_WARNING : LOG_INFO,
           "shadow_reload event=%s result=%s mode=%s candidate_objects=%zu unchanged=%zu added=%zu removed=%zu "
           "modified=%zu invalid=%zu disposition=%u report_hash=%016" PRIx64 " requests_total=%" PRIu64
           " reload_hup_off_total=%" PRIu64 " reload_validate_total=%" PRIu64 " reload_on_total=%" PRIu64
           " rejected_total=%" PRIu64 " reload_validate_rejected_total=%" PRIu64 " reload_on_rejected_total=%" PRIu64
           " reload_capability_rejected_total=%" PRIu64 " reload_legacy_hook_total=%" PRIu64
           " in_progress=%d pending=%d active_generation=%u duration_total_usec=%" PRIu64 " last_duration_usec=%" PRIu64
           " rejected_mode=%s rejected_reason=%s",
           event, result, modeName(configuredMode), candidateObjects, report == NULL ? 0 : report->unchangedCount,
           report == NULL ? 0 : report->addedCount, report == NULL ? 0 : report->removedCount,
           report == NULL ? 0 : report->modifiedCount, report == NULL ? 0 : report->invalidCount,
           report == NULL ? 0 : report->overallDisposition, reportHashValue, (uint64_t)requestsTotal,
           (uint64_t)offTotal, (uint64_t)validateTotal, (uint64_t)onTotal, (uint64_t)rejectedTotal,
           (uint64_t)rejectedValidateTotal, (uint64_t)rejectedOnTotal, (uint64_t)capabilityRejectedTotal,
           (uint64_t)legacyHookTotal, requestInProgress, signalRequestPending != 0, activeGeneration,
           (uint64_t)durationTotalUsec, (uint64_t)lastDurationUsec, rejectedMode, rejectedReason);
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
    STATSCOUNTER_INIT(ctrCapabilityRejected, mutCtrCapabilityRejected);
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
    CHKiRet(statsobj.AddCounter(reloadStats, (uchar *)"reload_capability_rejected_total", ctrType_IntCtr, CTR_FLAG_NONE,
                                &ctrCapabilityRejected));
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
    rsReloadCandidateDestruct(&pendingCandidate);
    rsReloadReportDestructV1(&pendingReport);
    rsReloadRulesetPlanDestructV1(&pendingPlan);
    free(candidateConfigPath);
    candidateConfigPath = NULL;
    rsReloadNormalizedGraphBuilderV1Destruct(&activeRulesetGraphBuilder);
    if (reloadStats != NULL) statsobj.Destruct(&reloadStats);
    objRelease(statsobj, CORE_COMPONENT);
}

rsRetVal shadowReloadConfigure(const reloadOnHUPMode_t mode,
                               const char *const configPath,
                               rsReloadNormalizedGraphBuilderV1_t *newBuilder) {
    rsReloadNormalizedGraphV1_t newGraph;
    rsRetVal graphRet;

    configuredMode = mode;
    free(candidateConfigPath);
    candidateConfigPath = configPath == NULL ? NULL : strdup(configPath);
    if (configPath != NULL && candidateConfigPath == NULL) {
        rsReloadNormalizedGraphBuilderV1Destruct(&newBuilder);
        return RS_RET_OUT_OF_MEMORY;
    }
    pthread_mutex_lock(&statusMut);
    activeGeneration = 1;
    lastUnchangedCount = 0;
    lastAddedCount = 0;
    lastRemovedCount = 0;
    lastModifiedCount = 0;
    lastInvalidCount = 0;
    pthread_mutex_unlock(&statusMut);
    pendingGauge = signalRequestPending != 0;
    memset(&newGraph, 0, sizeof(newGraph));
    graphRet = newBuilder == NULL ? RS_RET_NOT_IMPLEMENTED : RS_RET_OK;
    if (graphRet == RS_RET_OK) {
        graphRet = rsReloadNormalizedGraphBuilderV1GetGraph(newBuilder, &newGraph);
    }
    if (graphRet != RS_RET_OK) {
        rsReloadNormalizedGraphBuilderV1Destruct(&newBuilder);
        pthread_mutex_lock(&statusMut);
        baselineAvailable = 0;
        lastResult = SHADOW_RELOAD_REJECTED_BASELINE;
        pthread_mutex_unlock(&statusMut);
        LogError(0, graphRet, "shadow_reload: active source graph unavailable");
        logState("configured", "rejected", modeName(configuredMode), "baseline_unavailable", 0, NULL, 0);
    } else {
        pthread_mutex_lock(&statusMut);
        rsReloadNormalizedGraphBuilderV1Destruct(&activeRulesetGraphBuilder);
        activeRulesetGraphBuilder = newBuilder;
        activeRulesetGraph = newGraph;
        baselineAvailable = 1;
        lastResult = SHADOW_RELOAD_IDLE;
        pthread_mutex_unlock(&statusMut);
        logState("configured", "idle", "none", "none", 0, NULL, 0);
    }
    return RS_RET_OK;
}

typedef struct reloadRulesetLookup_s {
    const char *identity;
    const char *fingerprint;
} reloadRulesetLookup_t;

static rsRetVal findReloadRuleset(const rsReloadNormalizedNodeV1_t *node, void *context) {
    reloadRulesetLookup_t *lookup = context;

    if (node->objectKind == RS_RELOAD_OBJ_RULESET && !strcasecmp(node->identity, lookup->identity)) {
        lookup->fingerprint = node->fingerprint;
    }
    return RS_RET_OK;
}

rsRetVal shadowReloadGetRulesetFingerprint(const char *name, const char **ppFingerprint) {
    reloadRulesetLookup_t lookup;
    char *identity = NULL;
    size_t identityLen;
    DEFiRet;

    if (name == NULL || *name == '\0' || ppFingerprint == NULL || *ppFingerprint != NULL) {
        return RS_RET_PARAM_ERROR;
    }
    if (strlen(name) > SIZE_MAX - sizeof("ruleset:")) return RS_RET_OUT_OF_MEMORY;
    identityLen = sizeof("ruleset:") + strlen(name);
    CHKmalloc(identity = malloc(identityLen));
    snprintf(identity, identityLen, "ruleset:%s", name);
    lookup.identity = identity;
    lookup.fingerprint = NULL;
    pthread_mutex_lock(&statusMut);
    if (activeRulesetGraphBuilder == NULL) {
        iRet = RS_RET_PARAM_ERROR;
    } else {
        iRet = activeRulesetGraph.enumerate(activeRulesetGraph.context, findReloadRuleset, &lookup);
    }
    pthread_mutex_unlock(&statusMut);
    CHKiRet(iRet);
    if (lookup.fingerprint == NULL) ABORT_FINALIZE(RS_RET_NOT_FOUND);
    *ppFingerprint = lookup.fingerprint;

finalize_it:
    free(identity);
    RETiRet;
}

rsRetVal shadowReloadGetStatus(char *const buffer, const size_t bufferSize) {
    const char *result;
    int resultCode;
    unsigned generation;
    size_t unchanged = 0;
    size_t added = 0;
    size_t removed = 0;
    size_t modified = 0;
    size_t invalid = 0;
    int n;

    if (buffer == NULL || bufferSize == 0) return RS_RET_PARAM_ERROR;
    pthread_mutex_lock(&statusMut);
    resultCode = lastResult;
    generation = activeGeneration;
    unchanged = lastUnchangedCount;
    added = lastAddedCount;
    removed = lastRemovedCount;
    modified = lastModifiedCount;
    invalid = lastInvalidCount;
    pthread_mutex_unlock(&statusMut);
    switch (resultCode) {
        case SHADOW_RELOAD_IN_PROGRESS:
            result = "in_progress";
            break;
        case SHADOW_RELOAD_IGNORED:
            result = "ignored";
            break;
        case SHADOW_RELOAD_REPORTED:
            result = "reported_only";
            break;
        case SHADOW_RELOAD_REJECTED_IO:
            result = "candidate_io_error";
            break;
        case SHADOW_RELOAD_REJECTED_RESOURCE:
            result = "candidate_resource_error";
            break;
        case SHADOW_RELOAD_REJECTED_INTERNAL:
            result = "candidate_internal_error";
            break;
        case SHADOW_RELOAD_REJECTED_PARSE:
            result = "candidate_parse_invalid";
            break;
        case SHADOW_RELOAD_REJECTED_NORMALIZE:
            result = "candidate_normalization_unsupported";
            break;
        case SHADOW_RELOAD_REJECTED_BASELINE:
            result = "baseline_unavailable";
            break;
        case SHADOW_RELOAD_REJECTED_REPORT:
            result = "candidate_report_invalid";
            break;
        case SHADOW_RELOAD_REJECTED_CAPABILITY:
            result = "candidate_scope_unsupported";
            break;
        case SHADOW_RELOAD_REJECTED_ACTIVATION:
            result = "activation_not_implemented";
            break;
        case SHADOW_RELOAD_IDLE:
        default:
            result = "idle";
            break;
    }
    n = snprintf(buffer, bufferSize,
                 "result=%s active_generation=%u unchanged=%zu added=%zu removed=%zu modified=%zu invalid=%zu", result,
                 generation, unchanged, added, removed, modified, invalid);
    return n < 0 || (size_t)n >= bufferSize ? RS_RET_OUT_OF_MEMORY : RS_RET_OK;
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
    if (!monotonicUsec(&requestStartedUsec)) requestStartedUsec = 0;
    rsReloadCandidateDestruct(&pendingCandidate);
    rsReloadReportDestructV1(&pendingReport);
    rsReloadRulesetPlanDestructV1(&pendingPlan);
    pendingCandidateObjects = 0;
    pendingReportHash = 0;
    pendingFailurePhase = SHADOW_RELOAD_FAILURE_NONE;
    publishStatus(SHADOW_RELOAD_IN_PROGRESS, NULL);
    pendingCandidateResult = RS_RET_OK;
    if (configuredMode != RELOAD_ON_HUP_OFF) {
        pthread_mutex_lock(&statusMut);
        const int haveBaseline = baselineAvailable;
        pthread_mutex_unlock(&statusMut);
        if (!haveBaseline) {
            pendingFailurePhase = SHADOW_RELOAD_FAILURE_BASELINE;
            pendingCandidateResult = RS_RET_NOT_IMPLEMENTED;
        } else if (candidateConfigPath == NULL) {
            pendingFailurePhase = SHADOW_RELOAD_FAILURE_PARSE;
            pendingCandidateResult = RS_RET_CONF_FILE_NOT_FOUND;
        } else {
            pendingCandidateResult = rsReloadCandidateParse(candidateConfigPath, &pendingCandidate);
            if (pendingCandidateResult != RS_RET_OK) pendingFailurePhase = SHADOW_RELOAD_FAILURE_PARSE;
            if (pendingCandidateResult == RS_RET_OK) {
                rsReloadNormalizedGraphBuilderV1_t *candidateBuilder = NULL;
                rsReloadNormalizedGraphV1_t candidateGraph;

                pendingCandidateObjects = rsReloadCandidateObjectCount(pendingCandidate);
                memset(&candidateGraph, 0, sizeof(candidateGraph));
                pendingCandidateResult = rsReloadCandidateBuildNormalizedGraphV1(pendingCandidate, &candidateBuilder);
                if (pendingCandidateResult != RS_RET_OK) pendingFailurePhase = SHADOW_RELOAD_FAILURE_NORMALIZE;
                if (pendingCandidateResult == RS_RET_OK) {
                    pendingCandidateResult =
                        rsReloadNormalizedGraphBuilderV1GetGraph(candidateBuilder, &candidateGraph);
                    if (pendingCandidateResult != RS_RET_OK) pendingFailurePhase = SHADOW_RELOAD_FAILURE_NORMALIZE;
                }
                if (pendingCandidateResult == RS_RET_OK && activeRulesetGraphBuilder != NULL) {
                    pendingCandidateResult =
                        rsReloadReportBuildV1(&activeRulesetGraph, &candidateGraph, &pendingReport);
                    if (pendingCandidateResult != RS_RET_OK) pendingFailurePhase = SHADOW_RELOAD_FAILURE_REPORT;
                } else if (pendingCandidateResult == RS_RET_OK) {
                    pendingFailurePhase = SHADOW_RELOAD_FAILURE_BASELINE;
                    pendingCandidateResult = RS_RET_NOT_IMPLEMENTED;
                }
                rsReloadNormalizedGraphBuilderV1Destruct(&candidateBuilder);
                if (pendingCandidateResult == RS_RET_OK && configuredMode == RELOAD_ON_HUP_ON &&
                    pendingReport->invalidCount == 0) {
                    pendingCandidateResult = rsReloadCandidateCheckRulesetOnlyReportV1(pendingReport);
                    if (pendingCandidateResult != RS_RET_OK) pendingFailurePhase = SHADOW_RELOAD_FAILURE_CAPABILITY;
                    if (pendingCandidateResult == RS_RET_OK) {
                        pendingCandidateResult =
                            rsReloadRulesetPlanPrepareV1(runConf, pendingCandidate, pendingReport, &pendingPlan);
                        if (pendingCandidateResult != RS_RET_OK) pendingFailurePhase = SHADOW_RELOAD_FAILURE_CAPABILITY;
                    }
                }
            }
        }
    }
}

static void accountDuration(void) {
    uint64_t finishedUsec;

    lastDurationUsec = 0;
    if (requestStartedUsec == 0 || !monotonicUsec(&finishedUsec) || finishedUsec < requestStartedUsec) return;
    lastDurationUsec = finishedUsec - requestStartedUsec;
    durationTotalUsec += lastDurationUsec;
    STATSCOUNTER_ADD(ctrDurationTotalUsec, mutCtrDurationTotalUsec, lastDurationUsec);
}

static uint64_t reportHashByte(const uint64_t hash, const unsigned char value) {
    /* FNV-1a multiplication modulo 2^64 without relying on an overflowing
     * C expression. This keeps the control-path digest clean under the
     * unsigned-overflow sanitizer used by CI. 1099511628211 = 2^40 + 435. */
    const uint32_t low = (uint32_t)(hash ^ value);
    const uint32_t high = (uint32_t)((hash ^ value) >> 32);
    const uint64_t lowProduct = (uint64_t)low * 435U;
    const uint32_t upper = (uint32_t)((uint64_t)high * 435U + (uint64_t)low * 256U + (lowProduct >> 32));
    return (uint64_t)(uint32_t)lowProduct | ((uint64_t)upper << 32);
}

static void reportHashUnsigned(uint64_t *const hash, uint64_t value) {
    unsigned i;

    for (i = 0; i < sizeof(value); ++i) {
        *hash = reportHashByte(*hash, (unsigned char)value);
        value >>= 8;
    }
}

static uint64_t reportHash(const rsReloadReportV1_t *report) {
    static const unsigned char domain[] = "rsyslog.reload.report.v1";
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t i;

    if (report == NULL) return 0;
    for (i = 0; i < sizeof(domain) - 1; ++i) hash = reportHashByte(hash, domain[i]);
    reportHashUnsigned(&hash, report->version);
    reportHashUnsigned(&hash, report->entryCount);
    for (i = 0; i < report->entryCount; ++i) {
        const rsReloadReportEntryV1_t *entry = &report->entries[i];
        const unsigned char *identity = (const unsigned char *)entry->identity;
        reportHashUnsigned(&hash, entry->objectKind);
        reportHashUnsigned(&hash, entry->diffKind);
        reportHashUnsigned(&hash, entry->requiredFlags);
        reportHashUnsigned(&hash, entry->advertisedFlags);
        reportHashUnsigned(&hash, entry->disposition);
        reportHashUnsigned(&hash, entry->reason);
        while (identity != NULL && *identity != '\0') hash = reportHashByte(hash, *identity++);
        hash = reportHashByte(hash, 0);
    }
    return hash;
}

static int rejectedResult(void) {
    if (pendingFailurePhase == SHADOW_RELOAD_FAILURE_BASELINE) return SHADOW_RELOAD_REJECTED_BASELINE;
    if (pendingCandidateResult == RS_RET_OUT_OF_MEMORY) return SHADOW_RELOAD_REJECTED_RESOURCE;
    if (pendingCandidateResult == RS_RET_CONF_FILE_NOT_FOUND || pendingCandidateResult == RS_RET_FILE_NOT_FOUND ||
        pendingCandidateResult == RS_RET_FILE_OPEN_ERROR || pendingCandidateResult == RS_RET_IO_ERROR ||
        pendingCandidateResult == RS_RET_FILENAME_INVALID) {
        return SHADOW_RELOAD_REJECTED_IO;
    }
    if (pendingCandidateResult != RS_RET_OK) {
        const int expectedParseFailure =
            pendingFailurePhase == SHADOW_RELOAD_FAILURE_PARSE && pendingCandidateResult == RS_RET_CONF_PARSE_ERROR;
        const int expectedNormalizeFailure =
            pendingFailurePhase == SHADOW_RELOAD_FAILURE_NORMALIZE &&
            (pendingCandidateResult == RS_RET_NOT_IMPLEMENTED || pendingCandidateResult == RS_RET_PARAM_ERROR ||
             pendingCandidateResult == RS_RET_CONF_PARAM_INVLD || pendingCandidateResult == RS_RET_CONF_PARSE_ERROR);
        const int expectedCapabilityFailure =
            pendingFailurePhase == SHADOW_RELOAD_FAILURE_CAPABILITY && pendingCandidateResult == RS_RET_NOT_IMPLEMENTED;
        if (!expectedParseFailure && !expectedNormalizeFailure && !expectedCapabilityFailure)
            return SHADOW_RELOAD_REJECTED_INTERNAL;
    }
    switch (pendingFailurePhase) {
        case SHADOW_RELOAD_FAILURE_PARSE:
            return SHADOW_RELOAD_REJECTED_PARSE;
        case SHADOW_RELOAD_FAILURE_NORMALIZE:
            return SHADOW_RELOAD_REJECTED_NORMALIZE;
        case SHADOW_RELOAD_FAILURE_BASELINE:
            return SHADOW_RELOAD_REJECTED_BASELINE;
        case SHADOW_RELOAD_FAILURE_REPORT:
            return SHADOW_RELOAD_REJECTED_REPORT;
        case SHADOW_RELOAD_FAILURE_CAPABILITY:
            return SHADOW_RELOAD_REJECTED_CAPABILITY;
        case SHADOW_RELOAD_FAILURE_NONE:
        default:
            if (pendingCandidateResult != RS_RET_OK) return SHADOW_RELOAD_REJECTED_INTERNAL;
            return pendingReport != NULL && pendingReport->invalidCount != 0 ? SHADOW_RELOAD_REJECTED_REPORT
                                                                             : SHADOW_RELOAD_REJECTED_ACTIVATION;
    }
}

static const char *rejectedReason(const int result) {
    switch (result) {
        case SHADOW_RELOAD_REJECTED_IO:
            return "candidate_io_error";
        case SHADOW_RELOAD_REJECTED_RESOURCE:
            return "candidate_resource_error";
        case SHADOW_RELOAD_REJECTED_INTERNAL:
            return "candidate_internal_error";
        case SHADOW_RELOAD_REJECTED_PARSE:
            return "candidate_parse_invalid";
        case SHADOW_RELOAD_REJECTED_NORMALIZE:
            return "candidate_normalization_unsupported";
        case SHADOW_RELOAD_REJECTED_BASELINE:
            return "baseline_unavailable";
        case SHADOW_RELOAD_REJECTED_REPORT:
            return "candidate_report_invalid";
        case SHADOW_RELOAD_REJECTED_CAPABILITY:
            return "candidate_scope_unsupported";
        case SHADOW_RELOAD_REJECTED_ACTIVATION:
        default:
            return "activation_not_implemented";
    }
}

void shadowReloadProcess(void) {
    if (!requestInProgress) {
        return;
    }

    pendingReportHash = reportHash(pendingReport);
    if (configuredMode == RELOAD_ON_HUP_VALIDATE && pendingCandidateResult == RS_RET_OK && pendingReport != NULL &&
        pendingReport->invalidCount == 0) {
        accountDuration();
        requestInProgress = 0;
        pendingGauge = signalRequestPending != 0;
        publishStatus(SHADOW_RELOAD_REPORTED, pendingReport);
        logState("request", "reported_only", "none", "none", pendingCandidateObjects, pendingReport, pendingReportHash);
    } else if (configuredMode == RELOAD_ON_HUP_VALIDATE || configuredMode == RELOAD_ON_HUP_ON) {
        const int terminalResult = rejectedResult();
        ++rejectedTotal;
        STATSCOUNTER_INC(ctrRejected, mutCtrRejected);
        if (configuredMode == RELOAD_ON_HUP_VALIDATE) {
            ++rejectedValidateTotal;
            STATSCOUNTER_INC(ctrRejectedValidate, mutCtrRejectedValidate);
        } else {
            ++rejectedOnTotal;
            STATSCOUNTER_INC(ctrRejectedOn, mutCtrRejectedOn);
        }
        if (terminalResult == SHADOW_RELOAD_REJECTED_CAPABILITY) {
            ++capabilityRejectedTotal;
            STATSCOUNTER_INC(ctrCapabilityRejected, mutCtrCapabilityRejected);
        }
        accountDuration();
        requestInProgress = 0;
        pendingGauge = signalRequestPending != 0;
        publishStatus(terminalResult, pendingReport);
        logState("request", "rejected", modeName(configuredMode), rejectedReason(terminalResult),
                 pendingCandidateObjects, pendingReport, pendingReportHash);
    } else {
        accountDuration();
        requestInProgress = 0;
        pendingGauge = signalRequestPending != 0;
        publishStatus(SHADOW_RELOAD_IGNORED, NULL);
        logState("request", "ignored", "none", "mode_off", 0, NULL, 0);
    }
    rsReloadCandidateDestruct(&pendingCandidate);
    rsReloadReportDestructV1(&pendingReport);
    rsReloadRulesetPlanDestructV1(&pendingPlan);
}
