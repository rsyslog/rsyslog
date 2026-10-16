/** SPDX-License-Identifier: Apache-2.0
 * @file
 * @brief Transactional reload manager for rulesets and existing input policy.
 *
 * This owns request state, observability, and the active generation's
 * source-syntactic configuration graph. It parses candidates into an owned
 * representation without dispatching configuration objects into modules or
 * runtime globals. Existing ruleset plans, explicitly classified imtcp
 * profiles, and the independently lowered config.reloadOnHUP policy can be
 * prepared and atomically activated; all other changes remain fail-closed.
 * Historic HUP hooks remain outside this manager.
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
#include "dirty.h"
#include "glbl.h"
#include "obj.h"
#include "reload-candidate.h"
#include "reload-report.h"
#include "reload-ruleset-materializer.h"
#include "shadow_reload.h"
#include "statsobj.h"
#include "modules.h"
#include "shadow_reload_lifecycle.h"
#include "shadow_reload_retirement.h"
#include "shadow_reload_policy.h"

DEFobjStaticHelpers;
DEFobjCurrIf(statsobj);

static volatile sig_atomic_t signalRequestPending = 0;
static reloadOnHUPMode_t configuredMode = RELOAD_ON_HUP_OFF;

/* The main thread owns these fields; impstats also reads registered counters.
 * Counter updates use STATSCOUNTER macros, and gauge writes use atomic stores
 * matching statsobj's atomic reads. statusMut serializes status snapshots. */
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
static intctr_t quiescePauseTotalUsec = 0;
static intctr_t lastQuiescePauseUsec = 0;
static unsigned activeGeneration = 0;
static int requestInProgress = 0;
static int retirementBlockedRequest = 0; /* Main-thread cycle, never a deferred candidate. */
static int pendingGauge = 0;
static uint64_t requestStartedUsec = 0;
static statsobj_t *reloadStats = NULL;
static rsReloadNormalizedGraphBuilderV1_t *activeRulesetGraphBuilder = NULL;
static rsReloadCandidate_t *activeSourceObjectCatalog = NULL;
static rsReloadCandidate_t *retiredSourceObjectCatalog = NULL;
static rsReloadNormalizedGraphV1_t activeRulesetGraph;
static char *candidateConfigPath = NULL;
static rsReloadCandidate_t *pendingCandidate = NULL;
static rsReloadCandidate_t *pendingSourceObjectCatalog = NULL;
static rsReloadReportV1_t *pendingReport = NULL;
static rsReloadRulesetPlanV1_t *pendingPlan = NULL;
static rsReloadNormalizedGraphBuilderV1_t *pendingRulesetGraphBuilder = NULL;
static rsReloadNormalizedGraphV1_t pendingRulesetGraph;
static rsReloadNormalizedGraphBuilderV1_t *retiredRulesetGraphBuilder = NULL;
static modInfo_t *pendingSourceModule = NULL;
static void *pendingActiveSourceModuleCnf = NULL;
static void *pendingSourceModuleCnf = NULL;
static void *pendingModuleReloadState = NULL;
static int pendingModuleReloadCommitted = 0;
/* Only the main/control thread owns pending resume and retirement state. */
static int pendingModuleReloadResume = 0;
static shadowReloadRetirement_t retirementSchedule;
static shadowReloadRetirement_t retirementStatus; /* Observer copy protected by statusMut. */
static rsRetVal lastLoggedRetirementError = RS_RET_OK;
static int pendingGenerationActivated = 0;
static reloadOnHUPMode_t pendingReloadMode = RELOAD_ON_HUP_OFF;
static int pendingReportChildProcessExits = REPORT_CHILD_PROCESS_EXITS_ERRORS;
static int pendingBaseAuthorized = 0;
static int pendingReloadModeUpdate = 0;
static int pendingReportChildProcessExitsUpdate = 0;
static int pendingReportOversizeMsg = 1;
static int pendingReportOversizeMsgUpdate = 0;
static int pendingCompactJsonString = 0;
static int pendingCompactJsonStringUpdate = 0;
static int pendingParserDropTrailingLF = 1;
static int pendingParserDropTrailingLFUpdate = 0;
static int pendingParserDropTrailingCR = 0;
static int pendingParserDropTrailingCRUpdate = 0;
static int pendingParserEscapeControlCharacters = 1;
static int pendingParserEscapeControlCharactersUpdate = 0;
static int pendingParserSpaceLF = 0;
static int pendingParserSpaceLFUpdate = 0;
static int pendingParserEscape8BitCharacters = 0;
static int pendingParserEscape8BitCharactersUpdate = 0;
static int pendingParserEscapeControlCharacterTab = 1;
static int pendingParserEscapeControlCharacterTabUpdate = 0;
static int pendingParserEscapeControlCharactersCStyle = 0;
static int pendingParserEscapeControlCharactersCStyleUpdate = 0;
static int pendingParserPermitSlashInProgramName = 0;
static int pendingParserPermitSlashInProgramNameUpdate = 0;
static uchar pendingParserControlCharacterEscapePrefix = '#';
static int pendingParserControlCharacterEscapePrefixUpdate = 0;
static eModReloadCapability_t pendingSourceModuleCapability = eMOD_RELOAD_RESTART_REQUIRED;
static int pendingSourceModuleCapabilityEvaluated = 0;
static rsRetVal pendingCandidateResult = RS_RET_OK;
static size_t pendingCandidateObjects = 0;
static uint64_t pendingReportHash = 0;
static size_t lastUnchangedCount = 0;
static size_t lastAddedCount = 0;
static size_t lastRemovedCount = 0;
static size_t lastModifiedCount = 0;
static size_t lastInvalidCount = 0;
static eModReloadCapability_t lastSourceModuleCapability = eMOD_RELOAD_RESTART_REQUIRED;
static int lastSourceModuleCapabilityEvaluated = 0;
static pthread_mutex_t statusMut = PTHREAD_MUTEX_INITIALIZER;
static int baselineAvailable = 0;
enum shadowReloadResult_e {
    SHADOW_RELOAD_IDLE = 0,
    SHADOW_RELOAD_IN_PROGRESS,
    SHADOW_RELOAD_IGNORED,
    SHADOW_RELOAD_REPORTED,
    SHADOW_RELOAD_ACTIVATED,
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
    SHADOW_RELOAD_FAILURE_CAPABILITY,
    SHADOW_RELOAD_FAILURE_ACTIVATION
};
static enum shadowReloadFailurePhase_e pendingFailurePhase = SHADOW_RELOAD_FAILURE_NONE;

static rsRetVal destructPendingSourceModuleNow(void *const context ATTR_UNUSED) {
    if (pendingModuleReloadState != NULL) {
        if (pendingModuleReloadResume) {
            const rsRetVal ret = shadowReloadResumePending(pendingSourceModule, pendingModuleReloadState,
                                                           &pendingModuleReloadResume, 0, NULL);
            if (ret != RS_RET_OK) {
                return ret;
            }
        }
        if (pendingModuleReloadCommitted) {
            const rsRetVal ret = modReloadRetire(pendingSourceModule, pendingModuleReloadState);
            if (ret != RS_RET_OK) {
                return ret;
            }
        } else {
            modReloadAbort(pendingSourceModule, pendingModuleReloadState);
        }
        pendingModuleReloadState = NULL;
        pendingModuleReloadCommitted = 0;
    }
    modReloadDestructSourceCandidateV1(pendingSourceModule, &pendingActiveSourceModuleCnf);
    modReloadDestructSourceCandidateV1(pendingSourceModule, &pendingSourceModuleCnf);
    pendingSourceModule = NULL;
    return RS_RET_OK;
}

/** @brief Publish observer-only retirement fields without holding locks in callbacks. */
static void publishRetirementStatus(void) {
    pthread_mutex_lock(&statusMut);
    retirementStatus = retirementSchedule;
    pthread_mutex_unlock(&statusMut);
}

static sbool monotonicUsec(uint64_t *value);

/** @brief Common due-check for every normal cleanup/retry entry point. */
static rsRetVal destructPendingSourceModule(const int finalShutdown) {
    uint64_t now = 0;
    const int clockValid = monotonicUsec(&now);
    const rsRetVal ret = shadowReloadRetirementTry(&retirementSchedule, now, clockValid, finalShutdown,
                                                   destructPendingSourceModuleNow, NULL);
    publishRetirementStatus();
    return ret;
}

int shadowReloadRetirementPending(void) {
    return retirementSchedule.pending;
}

void shadowReloadRetryRetirement(void) {
    if (!retirementSchedule.pending) return;
    const rsRetVal ret = destructPendingSourceModule(0);
    if (ret == RS_RET_OK) lastLoggedRetirementError = RS_RET_OK;
    if (ret != RS_RET_OK && ret != RS_RET_RETRY && ret != lastLoggedRetirementError) {
        LogError(0, ret, "shadow_reload: module reload resume or retirement failed and remains pending");
        lastLoggedRetirementError = ret;
    }
}

static void publishStatus(const int result, const rsReloadReportV1_t *const report) {
    pthread_mutex_lock(&statusMut);
    lastResult = result;
    lastUnchangedCount = report == NULL ? 0 : report->unchangedCount;
    lastAddedCount = report == NULL ? 0 : report->addedCount;
    lastRemovedCount = report == NULL ? 0 : report->removedCount;
    lastModifiedCount = report == NULL ? 0 : report->modifiedCount;
    lastInvalidCount = report == NULL ? 0 : report->invalidCount;
    lastSourceModuleCapability = pendingSourceModuleCapability;
    lastSourceModuleCapabilityEvaluated = pendingSourceModuleCapabilityEvaluated;
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
STATSCOUNTER_DEF(ctrQuiescePauseTotalUsec, mutCtrQuiescePauseTotalUsec)
STATSCOUNTER_DEF(ctrActiveGeneration, mutCtrActiveGeneration)

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

static const char *sourceCapabilityName(const eModReloadCapability_t capability, const int evaluated) {
    if (!evaluated) return "not_evaluated";
    switch (capability) {
        case eMOD_RELOAD_REUSE:
            return "reuse";
        case eMOD_RELOAD_LIVE_SWAP:
            return "live_swap";
        case eMOD_RELOAD_NEW_SESSIONS:
            return "new_sessions";
        case eMOD_RELOAD_LIVE_AND_NEW_SESSIONS:
            return "live_and_new_sessions";
        case eMOD_RELOAD_DRAIN_REPLACE:
            return "drain_replace";
        case eMOD_RELOAD_RESTART_REQUIRED:
        default:
            return "restart_required";
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
    LogMsg(
        0, !strcmp(result, "rejected") ? RS_RET_ERR : RS_RET_OK, !strcmp(result, "rejected") ? LOG_WARNING : LOG_INFO,
        "shadow_reload event=%s result=%s mode=%s candidate_objects=%zu unchanged=%zu added=%zu removed=%zu "
        "modified=%zu invalid=%zu disposition=%u report_hash=%016" PRIx64 " requests_total=%" PRIu64
        " reload_hup_off_total=%" PRIu64 " reload_validate_total=%" PRIu64 " reload_on_total=%" PRIu64
        " rejected_total=%" PRIu64 " reload_validate_rejected_total=%" PRIu64 " reload_on_rejected_total=%" PRIu64
        " reload_capability_rejected_total=%" PRIu64 " reload_legacy_hook_total=%" PRIu64
        " in_progress=%d pending=%d active_generation=%u duration_total_usec=%" PRIu64 " last_duration_usec=%" PRIu64
        " quiesce_pause_total_usec=%" PRIu64 " last_quiesce_pause_usec=%" PRIu64
        " source_capability=%s rejected_mode=%s rejected_reason=%s",
        event, result, modeName(configuredMode), candidateObjects, report == NULL ? 0 : report->unchangedCount,
        report == NULL ? 0 : report->addedCount, report == NULL ? 0 : report->removedCount,
        report == NULL ? 0 : report->modifiedCount, report == NULL ? 0 : report->invalidCount,
        report == NULL ? 0 : report->overallDisposition, reportHashValue, (uint64_t)requestsTotal, (uint64_t)offTotal,
        (uint64_t)validateTotal, (uint64_t)onTotal, (uint64_t)rejectedTotal, (uint64_t)rejectedValidateTotal,
        (uint64_t)rejectedOnTotal, (uint64_t)capabilityRejectedTotal, (uint64_t)legacyHookTotal, requestInProgress,
        signalRequestPending != 0, activeGeneration, (uint64_t)durationTotalUsec, (uint64_t)lastDurationUsec,
        (uint64_t)quiescePauseTotalUsec, (uint64_t)lastQuiescePauseUsec,
        sourceCapabilityName(pendingSourceModuleCapability, pendingSourceModuleCapabilityEvaluated), rejectedMode,
        rejectedReason);
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
    STATSCOUNTER_INIT(ctrQuiescePauseTotalUsec, mutCtrQuiescePauseTotalUsec);
    STATSCOUNTER_INIT(ctrActiveGeneration, mutCtrActiveGeneration);
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
    CHKiRet(statsobj.AddCounter(reloadStats, (uchar *)"reload_quiesce_pause_total_usec", ctrType_IntCtr, CTR_FLAG_NONE,
                                &ctrQuiescePauseTotalUsec));
    CHKiRet(statsobj.AddCounter(reloadStats, (uchar *)"reload_active_generation", ctrType_IntCtr, CTR_FLAG_NONE,
                                &ctrActiveGeneration));
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
    /* Inputs have joined: one final guarded cleanup may bypass the deadline. */
    rsReloadCandidateDestruct(&pendingCandidate);
    rsReloadCandidateDestruct(&pendingSourceObjectCatalog);
    if (destructPendingSourceModule(1) != RS_RET_OK)
        LogError(0, RS_RET_ERR, "shadow_reload: module reload retirement remained pending during shutdown");
    rsReloadReportDestructV1(&pendingReport);
    rsReloadRulesetPlanDestructV1(&pendingPlan);
    rsReloadNormalizedGraphBuilderV1Destruct(&pendingRulesetGraphBuilder);
    rsReloadNormalizedGraphBuilderV1Destruct(&retiredRulesetGraphBuilder);
    free(candidateConfigPath);
    candidateConfigPath = NULL;
    rsReloadNormalizedGraphBuilderV1Destruct(&activeRulesetGraphBuilder);
    rsReloadCandidateDestruct(&activeSourceObjectCatalog);
    rsReloadCandidateDestruct(&retiredSourceObjectCatalog);
    if (reloadStats != NULL) statsobj.Destruct(&reloadStats);
    objRelease(statsobj, CORE_COMPONENT);
}

rsRetVal shadowReloadConfigure(const reloadOnHUPMode_t mode,
                               const char *const configPath,
                               rsReloadNormalizedGraphBuilderV1_t *newBuilder,
                               rsReloadCandidate_t *sourceObjectCatalog) {
    rsReloadNormalizedGraphV1_t newGraph;
    rsRetVal graphRet;

    configuredMode = mode;
    free(candidateConfigPath);
    candidateConfigPath = configPath == NULL ? NULL : strdup(configPath);
    if (configPath != NULL && candidateConfigPath == NULL) {
        rsReloadNormalizedGraphBuilderV1Destruct(&newBuilder);
        rsReloadCandidateDestruct(&sourceObjectCatalog);
        return RS_RET_OUT_OF_MEMORY;
    }
    pthread_mutex_lock(&statusMut);
    PREFER_STORE_INT(&activeGeneration, 1);
    ATOMIC_STORE_uint64(&ctrActiveGeneration, &mutCtrActiveGeneration, activeGeneration);
    lastUnchangedCount = 0;
    lastAddedCount = 0;
    lastRemovedCount = 0;
    lastModifiedCount = 0;
    lastInvalidCount = 0;
    pthread_mutex_unlock(&statusMut);
    PREFER_STORE_INT(&pendingGauge, signalRequestPending != 0);
    memset(&newGraph, 0, sizeof(newGraph));
    graphRet = newBuilder == NULL || sourceObjectCatalog == NULL ? RS_RET_NOT_IMPLEMENTED : RS_RET_OK;
    if (graphRet == RS_RET_OK) {
        graphRet = rsReloadNormalizedGraphBuilderV1GetGraph(newBuilder, &newGraph);
    }
    if (graphRet != RS_RET_OK) {
        rsReloadNormalizedGraphBuilderV1Destruct(&newBuilder);
        rsReloadCandidateDestruct(&sourceObjectCatalog);
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
        rsReloadCandidateDestruct(&activeSourceObjectCatalog);
        activeSourceObjectCatalog = sourceObjectCatalog;
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

static int reportChangesObjectKind(const rsReloadReportV1_t *const report, const rsReloadObjectKind_t objectKind) {
    size_t i;
    if (report == NULL || report->entries == NULL || report->entryStride < sizeof(rsReloadReportEntryV1_t) ||
        report->entryStride % _Alignof(rsReloadReportEntryV1_t) != 0)
        return 1;
    for (i = 0; i < report->entryCount; ++i) {
        const uintptr_t address = (uintptr_t)(const void *)report->entries + i * report->entryStride;
        const rsReloadReportEntryV1_t *const entry = (const rsReloadReportEntryV1_t *)(const void *)address;
        if (entry->objectKind == objectKind && entry->diffKind != RS_RELOAD_DIFF_UNCHANGED) return 1;
    }
    return 0;
}

static cfgmodules_etry_t *findActiveInputModule(const char *const configName) {
    cfgmodules_etry_t *entry;
    if (runConf == NULL) return NULL;
    for (entry = runConf->modules.root; entry != NULL; entry = entry->next) {
        if (entry->pMod != NULL && entry->pMod->eType == eMOD_IN && entry->pMod->cnfName != NULL &&
            !strcasecmp((const char *)entry->pMod->cnfName, configName))
            return entry;
    }
    return NULL;
}

static rsRetVal parseCandidateReloadMode(const char *const value, reloadOnHUPMode_t *const mode) {
    if (mode == NULL) return RS_RET_PARAM_ERROR;
    if (value == NULL || !strcasecmp(value, "off")) {
        *mode = RELOAD_ON_HUP_OFF;
    } else if (!strcasecmp(value, "validate")) {
        *mode = RELOAD_ON_HUP_VALIDATE;
    } else if (!strcasecmp(value, "on")) {
        *mode = RELOAD_ON_HUP_ON;
    } else {
        return RS_RET_CONF_PARAM_INVLD;
    }
    return RS_RET_OK;
}

static rsRetVal parseCandidateReportChildProcessExits(const char *const value, int *const mode) {
    if (mode == NULL) return RS_RET_PARAM_ERROR;
    if (value == NULL || !strcasecmp(value, "errors")) {
        *mode = REPORT_CHILD_PROCESS_EXITS_ERRORS;
    } else if (!strcasecmp(value, "none")) {
        *mode = REPORT_CHILD_PROCESS_EXITS_NONE;
    } else if (!strcasecmp(value, "all")) {
        *mode = REPORT_CHILD_PROCESS_EXITS_ALL;
    } else {
        return RS_RET_CONF_PARAM_INVLD;
    }
    return RS_RET_OK;
}

static rsRetVal parseCandidateBinary(const char *const value, const int defaultValue, int *const parsed) {
    if (parsed == NULL) return RS_RET_PARAM_ERROR;
    if (value == NULL) {
        *parsed = defaultValue;
    } else if (!strcasecmp(value, "on")) {
        *parsed = 1;
    } else if (!strcasecmp(value, "off")) {
        *parsed = 0;
    } else {
        return RS_RET_CONF_PARAM_INVLD;
    }
    return RS_RET_OK;
}

/** @brief Authorize one scalar only after proving every other global value unchanged.
 * Both profiles are request-local owned strings. Runtime baseline checks reject
 * source/runtime drift before any pending value can reach the commit callback.
 */
static rsRetVal classifyReloadBase(const rsReloadReportV1_t *const report) {
    char *activeValue = NULL;
    char *candidateValue = NULL;
    char *activeOther = NULL;
    char *candidateOther = NULL;
    reloadOnHUPMode_t activeMode;
    int activeReportChildProcessExits;
    int activeReportOversizeMsg;
    int activeCompactJsonString;
    int activeParserDropTrailingLF;
    int activeParserDropTrailingCR;
    int activeParserEscapeControlCharacters;
    int activeParserSpaceLF;
    int activeParserEscape8BitCharacters;
    int activeParserEscapeControlCharacterTab;
    int activeParserEscapeControlCharactersCStyle;
    int activeParserPermitSlashInProgramName;
    uchar activeParserControlCharacterEscapePrefix;
    DEFiRet;

    if (!reportChangesObjectKind(report, RS_RELOAD_OBJ_GLOBAL)) return RS_RET_OK;
    if (activeSourceObjectCatalog == NULL || pendingSourceObjectCatalog == NULL) return RS_RET_NOT_IMPLEMENTED;
    CHKiRet(rsReloadCandidateGlobalStringProfileV1(activeSourceObjectCatalog, "config.reloadonhup", &activeValue,
                                                   &activeOther));
    CHKiRet(rsReloadCandidateGlobalStringProfileV1(pendingSourceObjectCatalog, "config.reloadonhup", &candidateValue,
                                                   &candidateOther));
    if (!strcmp(activeOther, candidateOther)) {
        CHKiRet(parseCandidateReloadMode(activeValue, &activeMode));
        CHKiRet(parseCandidateReloadMode(candidateValue, &pendingReloadMode));
        if (activeMode != configuredMode || activeMode != rsconfGetReloadOnHUPMode(runConf))
            ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
        pendingBaseAuthorized = 1;
        pendingReloadModeUpdate = 1;
        FINALIZE;
    }
    free(activeValue);
    activeValue = NULL;
    free(candidateValue);
    candidateValue = NULL;
    free(activeOther);
    activeOther = NULL;
    free(candidateOther);
    candidateOther = NULL;
    CHKiRet(rsReloadCandidateGlobalStringProfileV1(activeSourceObjectCatalog, "reportchildprocessexits", &activeValue,
                                                   &activeOther));
    CHKiRet(rsReloadCandidateGlobalStringProfileV1(pendingSourceObjectCatalog, "reportchildprocessexits",
                                                   &candidateValue, &candidateOther));
    if (!strcmp(activeOther, candidateOther)) {
        CHKiRet(parseCandidateReportChildProcessExits(activeValue, &activeReportChildProcessExits));
        CHKiRet(parseCandidateReportChildProcessExits(candidateValue, &pendingReportChildProcessExits));
        if (activeReportChildProcessExits != glblGetReportChildProcessExits(runConf))
            ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
        pendingBaseAuthorized = 1;
        pendingReportChildProcessExitsUpdate = 1;
        FINALIZE;
    }
    free(activeValue);
    activeValue = NULL;
    free(candidateValue);
    candidateValue = NULL;
    free(activeOther);
    activeOther = NULL;
    free(candidateOther);
    candidateOther = NULL;
    CHKiRet(rsReloadCandidateGlobalStringProfileV1(activeSourceObjectCatalog, "oversizemsg.report", &activeValue,
                                                   &activeOther));
    CHKiRet(rsReloadCandidateGlobalStringProfileV1(pendingSourceObjectCatalog, "oversizemsg.report", &candidateValue,
                                                   &candidateOther));
    if (!strcmp(activeOther, candidateOther)) {
        CHKiRet(parseCandidateBinary(activeValue, 1, &activeReportOversizeMsg));
        CHKiRet(parseCandidateBinary(candidateValue, 1, &pendingReportOversizeMsg));
        if (activeReportOversizeMsg != glblReportOversizeMessage(runConf)) ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
        pendingBaseAuthorized = 1;
        pendingReportOversizeMsgUpdate = 1;
        FINALIZE;
    }
    free(activeValue);
    activeValue = NULL;
    free(candidateValue);
    candidateValue = NULL;
    free(activeOther);
    activeOther = NULL;
    free(candidateOther);
    candidateOther = NULL;
    CHKiRet(rsReloadCandidateGlobalStringProfileV1(activeSourceObjectCatalog, "compactjsonstring", &activeValue,
                                                   &activeOther));
    CHKiRet(rsReloadCandidateGlobalStringProfileV1(pendingSourceObjectCatalog, "compactjsonstring", &candidateValue,
                                                   &candidateOther));
    if (!strcmp(activeOther, candidateOther)) {
        CHKiRet(parseCandidateBinary(activeValue, 0, &activeCompactJsonString));
        CHKiRet(parseCandidateBinary(candidateValue, 0, &pendingCompactJsonString));
        if (activeCompactJsonString != glblGetCompactJsonString()) ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
        pendingBaseAuthorized = 1;
        pendingCompactJsonStringUpdate = 1;
        FINALIZE;
    }
    free(activeValue);
    activeValue = NULL;
    free(candidateValue);
    candidateValue = NULL;
    free(activeOther);
    activeOther = NULL;
    free(candidateOther);
    candidateOther = NULL;
    CHKiRet(rsReloadCandidateGlobalStringProfileV1(activeSourceObjectCatalog, "parser.droptrailinglfonreception",
                                                   &activeValue, &activeOther));
    CHKiRet(rsReloadCandidateGlobalStringProfileV1(pendingSourceObjectCatalog, "parser.droptrailinglfonreception",
                                                   &candidateValue, &candidateOther));
    if (!strcmp(activeOther, candidateOther)) {
        CHKiRet(parseCandidateBinary(activeValue, 1, &activeParserDropTrailingLF));
        CHKiRet(parseCandidateBinary(candidateValue, 1, &pendingParserDropTrailingLF));
        if (activeParserDropTrailingLF != glblGetParserDropTrailingLFOnReception(runConf))
            ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
        pendingBaseAuthorized = 1;
        pendingParserDropTrailingLFUpdate = 1;
        FINALIZE;
    }
    free(activeValue);
    activeValue = NULL;
    free(candidateValue);
    candidateValue = NULL;
    free(activeOther);
    activeOther = NULL;
    free(candidateOther);
    candidateOther = NULL;
    CHKiRet(rsReloadCandidateGlobalStringProfileV1(activeSourceObjectCatalog, "parser.droptrailingcronreception",
                                                   &activeValue, &activeOther));
    CHKiRet(rsReloadCandidateGlobalStringProfileV1(pendingSourceObjectCatalog, "parser.droptrailingcronreception",
                                                   &candidateValue, &candidateOther));
    if (!strcmp(activeOther, candidateOther)) {
        CHKiRet(parseCandidateBinary(activeValue, 0, &activeParserDropTrailingCR));
        CHKiRet(parseCandidateBinary(candidateValue, 0, &pendingParserDropTrailingCR));
        if (activeParserDropTrailingCR != glblGetParserDropTrailingCROnReception(runConf))
            ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
        pendingBaseAuthorized = 1;
        pendingParserDropTrailingCRUpdate = 1;
        FINALIZE;
    }
    free(activeValue);
    activeValue = NULL;
    free(candidateValue);
    candidateValue = NULL;
    free(activeOther);
    activeOther = NULL;
    free(candidateOther);
    candidateOther = NULL;
    CHKiRet(rsReloadCandidateGlobalStringProfileV1(activeSourceObjectCatalog, "parser.escapecontrolcharactersonreceive",
                                                   &activeValue, &activeOther));
    CHKiRet(rsReloadCandidateGlobalStringProfileV1(
        pendingSourceObjectCatalog, "parser.escapecontrolcharactersonreceive", &candidateValue, &candidateOther));
    if (!strcmp(activeOther, candidateOther)) {
        CHKiRet(parseCandidateBinary(activeValue, 1, &activeParserEscapeControlCharacters));
        CHKiRet(parseCandidateBinary(candidateValue, 1, &pendingParserEscapeControlCharacters));
        if (activeParserEscapeControlCharacters != glblGetParserEscapeControlCharactersOnReceive(runConf))
            ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
        pendingBaseAuthorized = 1;
        pendingParserEscapeControlCharactersUpdate = 1;
        FINALIZE;
    }
    free(activeValue);
    activeValue = NULL;
    free(candidateValue);
    candidateValue = NULL;
    free(activeOther);
    activeOther = NULL;
    free(candidateOther);
    candidateOther = NULL;
    CHKiRet(rsReloadCandidateGlobalStringProfileV1(activeSourceObjectCatalog, "parser.spacelfonreceive", &activeValue,
                                                   &activeOther));
    CHKiRet(rsReloadCandidateGlobalStringProfileV1(pendingSourceObjectCatalog, "parser.spacelfonreceive",
                                                   &candidateValue, &candidateOther));
    if (!strcmp(activeOther, candidateOther)) {
        CHKiRet(parseCandidateBinary(activeValue, 0, &activeParserSpaceLF));
        CHKiRet(parseCandidateBinary(candidateValue, 0, &pendingParserSpaceLF));
        if (activeParserSpaceLF != glblGetParserSpaceLFOnReceive(runConf)) ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
        pendingBaseAuthorized = 1;
        pendingParserSpaceLFUpdate = 1;
        FINALIZE;
    }
    free(activeValue);
    activeValue = NULL;
    free(candidateValue);
    candidateValue = NULL;
    free(activeOther);
    activeOther = NULL;
    free(candidateOther);
    candidateOther = NULL;
    CHKiRet(rsReloadCandidateGlobalStringProfileV1(activeSourceObjectCatalog, "parser.escape8bitcharactersonreceive",
                                                   &activeValue, &activeOther));
    CHKiRet(rsReloadCandidateGlobalStringProfileV1(pendingSourceObjectCatalog, "parser.escape8bitcharactersonreceive",
                                                   &candidateValue, &candidateOther));
    if (!strcmp(activeOther, candidateOther)) {
        CHKiRet(parseCandidateBinary(activeValue, 0, &activeParserEscape8BitCharacters));
        CHKiRet(parseCandidateBinary(candidateValue, 0, &pendingParserEscape8BitCharacters));
        if (activeParserEscape8BitCharacters != glblGetParserEscape8BitCharactersOnReceive(runConf))
            ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
        pendingBaseAuthorized = 1;
        pendingParserEscape8BitCharactersUpdate = 1;
        FINALIZE;
    }
    free(activeValue);
    activeValue = NULL;
    free(candidateValue);
    candidateValue = NULL;
    free(activeOther);
    activeOther = NULL;
    free(candidateOther);
    candidateOther = NULL;
    CHKiRet(rsReloadCandidateGlobalStringProfileV1(activeSourceObjectCatalog, "parser.escapecontrolcharactertab",
                                                   &activeValue, &activeOther));
    CHKiRet(rsReloadCandidateGlobalStringProfileV1(pendingSourceObjectCatalog, "parser.escapecontrolcharactertab",
                                                   &candidateValue, &candidateOther));
    if (!strcmp(activeOther, candidateOther)) {
        CHKiRet(parseCandidateBinary(activeValue, 1, &activeParserEscapeControlCharacterTab));
        CHKiRet(parseCandidateBinary(candidateValue, 1, &pendingParserEscapeControlCharacterTab));
        if (activeParserEscapeControlCharacterTab != glblGetParserEscapeControlCharacterTab(runConf))
            ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
        pendingBaseAuthorized = 1;
        pendingParserEscapeControlCharacterTabUpdate = 1;
        FINALIZE;
    }
    free(activeValue);
    activeValue = NULL;
    free(candidateValue);
    candidateValue = NULL;
    free(activeOther);
    activeOther = NULL;
    free(candidateOther);
    candidateOther = NULL;
    CHKiRet(rsReloadCandidateGlobalStringProfileV1(activeSourceObjectCatalog, "parser.escapecontrolcharacterscstyle",
                                                   &activeValue, &activeOther));
    CHKiRet(rsReloadCandidateGlobalStringProfileV1(pendingSourceObjectCatalog, "parser.escapecontrolcharacterscstyle",
                                                   &candidateValue, &candidateOther));
    if (!strcmp(activeOther, candidateOther)) {
        CHKiRet(parseCandidateBinary(activeValue, 0, &activeParserEscapeControlCharactersCStyle));
        CHKiRet(parseCandidateBinary(candidateValue, 0, &pendingParserEscapeControlCharactersCStyle));
        if (activeParserEscapeControlCharactersCStyle != glblGetParserEscapeControlCharactersCStyle(runConf))
            ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
        pendingBaseAuthorized = 1;
        pendingParserEscapeControlCharactersCStyleUpdate = 1;
        FINALIZE;
    }
    free(activeValue);
    activeValue = NULL;
    free(candidateValue);
    candidateValue = NULL;
    free(activeOther);
    activeOther = NULL;
    free(candidateOther);
    candidateOther = NULL;
    CHKiRet(rsReloadCandidateGlobalStringProfileV1(activeSourceObjectCatalog, "parser.permitslashinprogramname",
                                                   &activeValue, &activeOther));
    CHKiRet(rsReloadCandidateGlobalStringProfileV1(pendingSourceObjectCatalog, "parser.permitslashinprogramname",
                                                   &candidateValue, &candidateOther));
    if (!strcmp(activeOther, candidateOther)) {
        CHKiRet(parseCandidateBinary(activeValue, 0, &activeParserPermitSlashInProgramName));
        CHKiRet(parseCandidateBinary(candidateValue, 0, &pendingParserPermitSlashInProgramName));
        if (activeParserPermitSlashInProgramName != glblGetParserPermitSlashInProgramName(runConf))
            ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
        pendingBaseAuthorized = 1;
        pendingParserPermitSlashInProgramNameUpdate = 1;
        FINALIZE;
    }
    free(activeValue);
    activeValue = NULL;
    free(candidateValue);
    candidateValue = NULL;
    free(activeOther);
    activeOther = NULL;
    free(candidateOther);
    candidateOther = NULL;
    CHKiRet(rsReloadCandidateGlobalStringProfileV1(activeSourceObjectCatalog, "parser.controlcharacterescapeprefix",
                                                   &activeValue, &activeOther));
    CHKiRet(rsReloadCandidateGlobalStringProfileV1(pendingSourceObjectCatalog, "parser.controlcharacterescapeprefix",
                                                   &candidateValue, &candidateOther));
    if (strcmp(activeOther, candidateOther)) ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
    CHKiRet(shadowReloadParseCharPolicy(activeValue, '#', &activeParserControlCharacterEscapePrefix));
    CHKiRet(shadowReloadParseCharPolicy(candidateValue, '#', &pendingParserControlCharacterEscapePrefix));
    if (activeParserControlCharacterEscapePrefix != glblGetParserControlCharacterEscapePrefix(runConf))
        ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
    pendingBaseAuthorized = 1;
    pendingParserControlCharacterEscapePrefixUpdate = 1;

finalize_it:
    free(activeValue);
    free(candidateValue);
    free(activeOther);
    free(candidateOther);
    RETiRet;
}

static rsRetVal lowerImtcpSourceCandidate(const rsReloadReportV1_t *const report) {
    cfgmodules_etry_t *const activeModule = findActiveInputModule("imtcp");
    modReloadSourceBuildContextV1_t context;

    if (activeModule == NULL || !modReloadHasValidSourceInterfaceV1(activeModule->pMod)) return RS_RET_OK;
    if (reportChangesObjectKind(report, RS_RELOAD_OBJ_GLOBAL) && !pendingBaseAuthorized) return RS_RET_OK;
    memset(&context, 0, sizeof(context));
    context.version = MOD_RELOAD_SOURCE_BUILD_CONTEXT_V1;
    context.structSize = sizeof(context);
    context.flags = MOD_RELOAD_SOURCE_BASE_UNCHANGED;
    context.activeBase = runConf;
    pendingSourceModule = activeModule->pMod;
    context.sourceCatalog = activeSourceObjectCatalog;
    rsRetVal ret = modReloadBuildSourceCandidateV1(pendingSourceModule, &context, &pendingActiveSourceModuleCnf);
    if (ret == RS_RET_NOT_FOUND || ret == RS_RET_NOT_IMPLEMENTED) {
        /* Legacy syntax and source settings that require side-effectful
         * runtime lowering have no private V1 snapshot. They remain governed
         * by the existing conservative scope gate. */
        pendingSourceModule = NULL;
        return RS_RET_OK;
    }
    if (ret != RS_RET_OK) return ret;
    context.sourceCatalog = pendingSourceObjectCatalog;
    ret = modReloadBuildSourceCandidateV1(pendingSourceModule, &context, &pendingSourceModuleCnf);
    if (ret != RS_RET_OK) return ret;
    ret = modReloadClassifySourceCandidateV1(pendingSourceModule, pendingActiveSourceModuleCnf, pendingSourceModuleCnf,
                                             &pendingSourceModuleCapability);
    if (ret == RS_RET_OK) pendingSourceModuleCapabilityEvaluated = 1;
    return ret;
}

static enum shadowReloadFailurePhase_e sourceLoweringFailurePhase(const rsRetVal ret) {
    if (ret == RS_RET_CONF_PARSE_ERROR || ret == RS_RET_CONF_PARAM_INVLD || ret == RS_RET_INVALID_PARAMS ||
        ret == RS_RET_PARAM_ERROR || ret == RS_RET_MODULE_ALREADY_IN_CONF || ret == RS_RET_NOT_FOUND)
        return SHADOW_RELOAD_FAILURE_NORMALIZE;
    return SHADOW_RELOAD_FAILURE_CAPABILITY;
}

rsRetVal shadowReloadGetRulesetFingerprint(const char *name, char **ppFingerprint) {
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
    if (iRet == RS_RET_OK) {
        if (lookup.fingerprint == NULL) {
            iRet = RS_RET_NOT_FOUND;
        } else if ((*ppFingerprint = strdup(lookup.fingerprint)) == NULL) {
            iRet = RS_RET_OUT_OF_MEMORY;
        }
    }
    pthread_mutex_unlock(&statusMut);

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
    eModReloadCapability_t sourceCapability;
    int sourceCapabilityEvaluated;
    shadowReloadRetirement_t retirement;
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
    sourceCapability = lastSourceModuleCapability;
    sourceCapabilityEvaluated = lastSourceModuleCapabilityEvaluated;
    retirement = retirementStatus;
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
        case SHADOW_RELOAD_ACTIVATED:
            result = "activated";
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
            result = "activation_failed";
            break;
        case SHADOW_RELOAD_IDLE:
        default:
            result = "idle";
            break;
    }
    n = snprintf(buffer, bufferSize,
                 "result=%s active_generation=%u unchanged=%zu added=%zu removed=%zu modified=%zu invalid=%zu "
                 "source_capability=%s retirement_pending=%d retirement_last_error=%d retirement_retry_count=%" PRIu64,
                 result, generation, unchanged, added, removed, modified, invalid,
                 sourceCapabilityName(sourceCapability, sourceCapabilityEvaluated), retirement.pending,
                 retirement.lastError, retirement.retryCount);
    return n < 0 || (size_t)n >= bufferSize ? RS_RET_OUT_OF_MEMORY : RS_RET_OK;
}

void shadowReloadRequestFromSignal(void) {
    /* Coalesce repeated signals. Do not count or log from signal context. */
    signalRequestPending = 1;
}

static int stopBeginForTermination(void) {
    if (!get_bFinished()) return 0;
    pendingFailurePhase = SHADOW_RELOAD_FAILURE_ACTIVATION;
    pendingCandidateResult = RS_RET_NO_RUN;
    return 1;
}

static void accountRequestClaim(void *const context ATTR_UNUSED) {
    /* This runs in lockstep with bHadHUP before doHUP(). A signal arriving
     * during doHUP() sets this flag again and is retained for the next legacy
     * HUP cycle rather than being cleared while this one completes. */
    signalRequestPending = 0;
    /* A claimed request remains pending through the legacy hooks until
     * shadowReloadProcess completes it; signal state tracks the next cycle. */
    PREFER_STORE_INT(&pendingGauge, 1);
    PREFER_STORE_INT(&requestInProgress, 1);
    ++requestsTotal;
    STATSCOUNTER_INC(ctrRequests, mutCtrRequests);
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
}

void shadowReloadBeginRequest(void) {
    rsRetVal moduleCleanupRet;
    /* Explicit rejection, not deferred/coalesced execution: main already owns
     * this HUP and still runs its legacy hooks. Preserve the published status. */
    if (shadowReloadRetirementClaim(&retirementSchedule, &retirementBlockedRequest, accountRequestClaim, NULL)) return;
    rsReloadCandidateDestruct(&pendingCandidate);
    rsReloadCandidateDestruct(&pendingSourceObjectCatalog);
    moduleCleanupRet = destructPendingSourceModule(0);
    rsReloadReportDestructV1(&pendingReport);
    rsReloadRulesetPlanDestructV1(&pendingPlan);
    rsReloadNormalizedGraphBuilderV1Destruct(&pendingRulesetGraphBuilder);
    memset(&pendingRulesetGraph, 0, sizeof(pendingRulesetGraph));
    pendingCandidateObjects = 0;
    pendingReportHash = 0;
    lastQuiescePauseUsec = 0;
    pendingFailurePhase = SHADOW_RELOAD_FAILURE_NONE;
    pendingSourceModuleCapability = eMOD_RELOAD_RESTART_REQUIRED;
    pendingSourceModuleCapabilityEvaluated = 0;
    pendingReloadMode = configuredMode;
    pendingReportChildProcessExits = glblGetReportChildProcessExits(runConf);
    pendingReportOversizeMsg = glblReportOversizeMessage(runConf);
    pendingCompactJsonString = glblGetCompactJsonString();
    pendingParserDropTrailingLF = glblGetParserDropTrailingLFOnReception(runConf);
    pendingParserDropTrailingCR = glblGetParserDropTrailingCROnReception(runConf);
    pendingParserEscapeControlCharacters = glblGetParserEscapeControlCharactersOnReceive(runConf);
    pendingParserSpaceLF = glblGetParserSpaceLFOnReceive(runConf);
    pendingParserEscape8BitCharacters = glblGetParserEscape8BitCharactersOnReceive(runConf);
    pendingParserEscapeControlCharacterTab = glblGetParserEscapeControlCharacterTab(runConf);
    pendingParserEscapeControlCharactersCStyle = glblGetParserEscapeControlCharactersCStyle(runConf);
    pendingParserPermitSlashInProgramName = glblGetParserPermitSlashInProgramName(runConf);
    pendingParserControlCharacterEscapePrefix = glblGetParserControlCharacterEscapePrefix(runConf);
    pendingBaseAuthorized = 0;
    pendingReloadModeUpdate = 0;
    pendingReportChildProcessExitsUpdate = 0;
    pendingReportOversizeMsgUpdate = 0;
    pendingCompactJsonStringUpdate = 0;
    pendingParserDropTrailingLFUpdate = 0;
    pendingParserDropTrailingCRUpdate = 0;
    pendingParserEscapeControlCharactersUpdate = 0;
    pendingParserSpaceLFUpdate = 0;
    pendingParserEscape8BitCharactersUpdate = 0;
    pendingParserEscapeControlCharacterTabUpdate = 0;
    pendingParserEscapeControlCharactersCStyleUpdate = 0;
    pendingParserPermitSlashInProgramNameUpdate = 0;
    pendingParserControlCharacterEscapePrefixUpdate = 0;
    pendingGenerationActivated = 0;
    publishStatus(SHADOW_RELOAD_IN_PROGRESS, NULL);
    pendingCandidateResult = moduleCleanupRet;
    if (moduleCleanupRet != RS_RET_OK) pendingFailurePhase = SHADOW_RELOAD_FAILURE_ACTIVATION;
    if (configuredMode != RELOAD_ON_HUP_OFF && pendingCandidateResult == RS_RET_OK) {
        pthread_mutex_lock(&statusMut);
        const int haveBaseline = baselineAvailable;
        pthread_mutex_unlock(&statusMut);
        if (stopBeginForTermination()) {
            /* TERM wins before candidate I/O begins. */
        } else if (!haveBaseline) {
            pendingFailurePhase = SHADOW_RELOAD_FAILURE_BASELINE;
            pendingCandidateResult = RS_RET_NOT_IMPLEMENTED;
        } else if (candidateConfigPath == NULL) {
            pendingFailurePhase = SHADOW_RELOAD_FAILURE_PARSE;
            pendingCandidateResult = RS_RET_CONF_FILE_NOT_FOUND;
        } else {
            pendingCandidateResult = rsReloadCandidateParse(candidateConfigPath, &pendingCandidate);
            if (pendingCandidateResult != RS_RET_OK) pendingFailurePhase = SHADOW_RELOAD_FAILURE_PARSE;
            if (pendingCandidateResult == RS_RET_OK && !stopBeginForTermination()) {
                rsReloadNormalizedGraphBuilderV1_t *candidateBuilder = NULL;
                rsReloadNormalizedGraphV1_t candidateGraph;

                pendingCandidateObjects = rsReloadCandidateObjectCount(pendingCandidate);
                memset(&candidateGraph, 0, sizeof(candidateGraph));
                pendingCandidateResult = rsReloadCandidateBuildNormalizedGraphV1(pendingCandidate, &candidateBuilder);
                if (pendingCandidateResult != RS_RET_OK) pendingFailurePhase = SHADOW_RELOAD_FAILURE_NORMALIZE;
                if (pendingCandidateResult == RS_RET_OK && !stopBeginForTermination()) {
                    pendingCandidateResult =
                        rsReloadNormalizedGraphBuilderV1GetGraph(candidateBuilder, &candidateGraph);
                    if (pendingCandidateResult != RS_RET_OK) pendingFailurePhase = SHADOW_RELOAD_FAILURE_NORMALIZE;
                }
                if (pendingCandidateResult == RS_RET_OK && !stopBeginForTermination() &&
                    activeRulesetGraphBuilder != NULL) {
                    pendingCandidateResult =
                        rsReloadReportBuildV1(&activeRulesetGraph, &candidateGraph, &pendingReport);
                    if (pendingCandidateResult != RS_RET_OK) pendingFailurePhase = SHADOW_RELOAD_FAILURE_REPORT;
                } else if (pendingCandidateResult == RS_RET_OK) {
                    pendingFailurePhase = SHADOW_RELOAD_FAILURE_BASELINE;
                    pendingCandidateResult = RS_RET_NOT_IMPLEMENTED;
                }
                if (pendingCandidateResult == RS_RET_OK && !stopBeginForTermination() &&
                    pendingReport->invalidCount == 0) {
                    pendingCandidateResult =
                        rsReloadCandidateBuildObjectCatalogV1(pendingCandidate, &pendingSourceObjectCatalog);
                    if (pendingCandidateResult != RS_RET_OK) {
                        pendingFailurePhase = SHADOW_RELOAD_FAILURE_NORMALIZE;
                        goto candidate_done;
                    }
                    if (configuredMode == RELOAD_ON_HUP_ON) {
                        pendingCandidateResult = classifyReloadBase(pendingReport);
                        if (pendingCandidateResult != RS_RET_OK) {
                            pendingFailurePhase = sourceLoweringFailurePhase(pendingCandidateResult);
                            goto candidate_done;
                        }
                    }
                    pendingCandidateResult = lowerImtcpSourceCandidate(pendingReport);
                    if (pendingCandidateResult != RS_RET_OK) {
                        pendingFailurePhase = sourceLoweringFailurePhase(pendingCandidateResult);
                        goto candidate_done;
                    }
                }
                if (pendingCandidateResult == RS_RET_OK && !stopBeginForTermination() &&
                    configuredMode == RELOAD_ON_HUP_ON && pendingReport->invalidCount == 0) {
                    /* Input-only commits can bypass ruleset-body preparation.
                     * Neither listener fences nor global queue barriers cover
                     * producer-local FE bindings or mixed queue boundaries. */
                    if (runConf->local_queue_graph != NULL &&
                        (pendingReport->modifiedCount != 0 || pendingReport->addedCount != 0 ||
                         pendingReport->removedCount != 0)) {
                        LogError(0, RS_RET_NOT_IMPLEMENTED,
                                 "reload unsupported for active local-queue graph, including mixed boundaries; "
                                 "restart rsyslog to apply configuration changes");
                        pendingCandidateResult = RS_RET_NOT_IMPLEMENTED;
                        pendingFailurePhase = SHADOW_RELOAD_FAILURE_CAPABILITY;
                        goto candidate_done;
                    }
                    const int sourceReloadable = pendingSourceModuleCapabilityEvaluated &&
                                                 (pendingSourceModuleCapability == eMOD_RELOAD_REUSE ||
                                                  pendingSourceModuleCapability == eMOD_RELOAD_LIVE_SWAP ||
                                                  pendingSourceModuleCapability == eMOD_RELOAD_NEW_SESSIONS ||
                                                  pendingSourceModuleCapability == eMOD_RELOAD_LIVE_AND_NEW_SESSIONS ||
                                                  pendingSourceModuleCapability == eMOD_RELOAD_DRAIN_REPLACE);
                    const int moduleNeedsCommit =
                        sourceReloadable && pendingSourceModuleCapability != eMOD_RELOAD_REUSE;
                    /* Private plan preparation owns source authorization. In
                     * particular, additions gain their scoped authorization
                     * only after all shells, roots and metadata are prepared. */
                    if (pendingCandidateResult == RS_RET_OK) {
                        if (stopBeginForTermination()) goto candidate_done;
                        pendingCandidateResult = rsReloadRulesetPlanPrepareV1(
                            runConf, activeSourceObjectCatalog, pendingCandidate, pendingReport,
                            sourceReloadable ? pendingSourceModuleCapability : eMOD_RELOAD_RESTART_REQUIRED,
                            pendingBaseAuthorized, &pendingPlan);
                        if (pendingCandidateResult != RS_RET_OK) pendingFailurePhase = SHADOW_RELOAD_FAILURE_CAPABILITY;
                        if (pendingCandidateResult == RS_RET_OK && moduleNeedsCommit) {
                            pendingCandidateResult =
                                modReloadPrepare(pendingSourceModule, pendingActiveSourceModuleCnf,
                                                 pendingSourceModuleCnf, &pendingModuleReloadState);
                            if (pendingCandidateResult != RS_RET_OK) {
                                /* Invalid prepared regexes and unavailable
                                 * listener endpoints are activation failures
                                 * after accepted source capability.
                                 * Unsupported constraints and unexpected
                                 * errors retain their existing classification. */
                                pendingFailurePhase = (pendingCandidateResult == RS_RET_INVALID_VALUE ||
                                                       pendingCandidateResult == RS_RET_COULD_NOT_BIND)
                                                          ? SHADOW_RELOAD_FAILURE_ACTIVATION
                                                          : SHADOW_RELOAD_FAILURE_CAPABILITY;
                            }
                        }
                        if (pendingCandidateResult == RS_RET_OK && pendingPlan != NULL &&
                            (pendingReport->modifiedCount != 0 || pendingReport->addedCount != 0 ||
                             pendingReport->removedCount != 0)) {
                            pendingRulesetGraphBuilder = candidateBuilder;
                            pendingRulesetGraph = candidateGraph;
                            candidateBuilder = NULL;
                        }
                    }
                }
            candidate_done:
                rsReloadNormalizedGraphBuilderV1Destruct(&candidateBuilder);
            }
        }
    }
}

void shadowReloadLegacyHooksCompleted(void) {
    ++legacyHookTotal;
    STATSCOUNTER_INC(ctrLegacyHooks, mutCtrLegacyHooks);
}

static void accountDuration(void) {
    uint64_t finishedUsec;

    lastDurationUsec = 0;
    if (requestStartedUsec == 0 || !monotonicUsec(&finishedUsec) || finishedUsec < requestStartedUsec) return;
    lastDurationUsec = finishedUsec - requestStartedUsec;
    durationTotalUsec += lastDurationUsec;
    STATSCOUNTER_ADD(ctrDurationTotalUsec, mutCtrDurationTotalUsec, lastDurationUsec);
}

static void accountQuiescePause(const uint64_t pauseUsec) {
    lastQuiescePauseUsec = (intctr_t)pauseUsec;
    quiescePauseTotalUsec += (intctr_t)pauseUsec;
    STATSCOUNTER_ADD(ctrQuiescePauseTotalUsec, mutCtrQuiescePauseTotalUsec, pauseUsec);
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
             pendingCandidateResult == RS_RET_CONF_PARAM_INVLD || pendingCandidateResult == RS_RET_CONF_PARSE_ERROR ||
             pendingCandidateResult == RS_RET_INVALID_PARAMS ||
             pendingCandidateResult == RS_RET_MODULE_ALREADY_IN_CONF || pendingCandidateResult == RS_RET_NOT_FOUND);
        const int expectedCapabilityFailure =
            pendingFailurePhase == SHADOW_RELOAD_FAILURE_CAPABILITY && pendingCandidateResult == RS_RET_NOT_IMPLEMENTED;
        const int expectedActivationFailure =
            pendingFailurePhase == SHADOW_RELOAD_FAILURE_ACTIVATION &&
            (pendingCandidateResult == RS_RET_NO_RUN || pendingCandidateResult == RS_RET_TIMED_OUT ||
             pendingCandidateResult == RS_RET_NOT_IMPLEMENTED || pendingCandidateResult == RS_RET_PARAM_ERROR ||
             pendingCandidateResult == RS_RET_INVALID_VALUE || pendingCandidateResult == RS_RET_COULD_NOT_BIND ||
             pendingCandidateResult == RS_RET_ERR);
        if (!expectedParseFailure && !expectedNormalizeFailure && !expectedCapabilityFailure &&
            !expectedActivationFailure)
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
        case SHADOW_RELOAD_FAILURE_ACTIVATION:
            return SHADOW_RELOAD_REJECTED_ACTIVATION;
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
            return "activation_failed";
    }
}

static sbool shadowReloadCancelled(void __attribute__((unused)) *const context) {
    return get_bFinished() != 0;
}

typedef struct reloadCommitSignalGuard_s {
    sigset_t previousMask;
    int active;
} reloadCommitSignalGuard_t;

static rsRetVal shadowReloadEnterCommit(void *const context) {
    reloadCommitSignalGuard_t *const guard = context;
    sigset_t terminationSignals;

    if (guard == NULL || guard->active) return RS_RET_PARAM_ERROR;
    sigemptyset(&terminationSignals);
    sigaddset(&terminationSignals, SIGTERM);
    sigaddset(&terminationSignals, SIGINT);
    sigaddset(&terminationSignals, SIGQUIT);
    if (pthread_sigmask(SIG_BLOCK, &terminationSignals, &guard->previousMask) != 0) return RS_RET_ERR;
    guard->active = 1;
    return RS_RET_OK;
}

static void shadowReloadLeaveCommit(void *const context) {
    reloadCommitSignalGuard_t *const guard = context;
    int maskRet;

    if (guard == NULL || !guard->active) return;
    maskRet = pthread_sigmask(SIG_SETMASK, &guard->previousMask, NULL);
    if (maskRet != 0) {
        LogError(maskRet, RS_RET_ERR, "shadow_reload: could not restore the termination-signal mask after commit");
        return;
    }
    guard->active = 0;
}

static void publishActivatedGraph(void __attribute__((unused)) *const context) {
    pthread_mutex_lock(&statusMut);
    retiredRulesetGraphBuilder = activeRulesetGraphBuilder;
    activeRulesetGraphBuilder = pendingRulesetGraphBuilder;
    activeRulesetGraph = pendingRulesetGraph;
    pendingRulesetGraphBuilder = NULL;
    retiredSourceObjectCatalog = activeSourceObjectCatalog;
    activeSourceObjectCatalog = pendingSourceObjectCatalog;
    pendingSourceObjectCatalog = NULL;
    memset(&pendingRulesetGraph, 0, sizeof(pendingRulesetGraph));
    PREFER_STORE_INT(&activeGeneration, PREFER_LOAD_INT(&activeGeneration) + 1U);
    ATOMIC_STORE_uint64(&ctrActiveGeneration, &mutCtrActiveGeneration, (unsigned)PREFER_LOAD_INT(&activeGeneration));
    lastResult = SHADOW_RELOAD_ACTIVATED;
    lastUnchangedCount = pendingReport == NULL ? 0 : pendingReport->unchangedCount;
    lastAddedCount = pendingReport == NULL ? 0 : pendingReport->addedCount;
    lastRemovedCount = pendingReport == NULL ? 0 : pendingReport->removedCount;
    lastModifiedCount = pendingReport == NULL ? 0 : pendingReport->modifiedCount;
    lastInvalidCount = pendingReport == NULL ? 0 : pendingReport->invalidCount;
    lastSourceModuleCapability = pendingSourceModuleCapability;
    lastSourceModuleCapabilityEvaluated = pendingSourceModuleCapabilityEvaluated;
    pendingGenerationActivated = 1;
    pthread_mutex_unlock(&statusMut);
}

static void publishActivatedGeneration(void *const context) {
    if (pendingModuleReloadState != NULL) {
        modReloadCommit(pendingSourceModule, pendingModuleReloadState);
        pendingModuleReloadCommitted = 1;
    }
    if (pendingReloadModeUpdate) {
        configuredMode = pendingReloadMode;
        glblSetReloadOnHUPMode(runConf, pendingReloadMode);
    }
    if (pendingReportChildProcessExitsUpdate) glblSetReportChildProcessExits(runConf, pendingReportChildProcessExits);
    if (pendingReportOversizeMsgUpdate) glblSetReportOversizeMessage(runConf, pendingReportOversizeMsg);
    if (pendingCompactJsonStringUpdate) glblSetCompactJsonString(pendingCompactJsonString);
    if (pendingParserDropTrailingLFUpdate) glblSetParserDropTrailingLFOnReception(runConf, pendingParserDropTrailingLF);
    if (pendingParserDropTrailingCRUpdate) glblSetParserDropTrailingCROnReception(runConf, pendingParserDropTrailingCR);
    if (pendingParserEscapeControlCharactersUpdate)
        glblSetParserEscapeControlCharactersOnReceive(runConf, pendingParserEscapeControlCharacters);
    if (pendingParserSpaceLFUpdate) glblSetParserSpaceLFOnReceive(runConf, pendingParserSpaceLF);
    if (pendingParserEscape8BitCharactersUpdate)
        glblSetParserEscape8BitCharactersOnReceive(runConf, pendingParserEscape8BitCharacters);
    if (pendingParserEscapeControlCharacterTabUpdate)
        glblSetParserEscapeControlCharacterTab(runConf, pendingParserEscapeControlCharacterTab);
    if (pendingParserEscapeControlCharactersCStyleUpdate)
        glblSetParserEscapeControlCharactersCStyle(runConf, pendingParserEscapeControlCharactersCStyle);
    if (pendingParserPermitSlashInProgramNameUpdate)
        glblSetParserPermitSlashInProgramName(runConf, pendingParserPermitSlashInProgramName);
    if (pendingParserControlCharacterEscapePrefixUpdate)
        glblSetParserControlCharacterEscapePrefix(runConf, pendingParserControlCharacterEscapePrefix);
    publishActivatedGraph(context);
}

static void accountRetirementRejectedValidate(void) {
    ++rejectedValidateTotal;
    STATSCOUNTER_INC(ctrRejectedValidate, mutCtrRejectedValidate);
}

static void accountRetirementRejectedOn(void) {
    ++rejectedOnTotal;
    STATSCOUNTER_INC(ctrRejectedOn, mutCtrRejectedOn);
}

/** @brief Reject a completed legacy HUP without replacing the published status. */
static void completeRetirementBlockedRequest(void *const context ATTR_UNUSED) {
    ++rejectedTotal;
    STATSCOUNTER_INC(ctrRejected, mutCtrRejected);
    shadowReloadRetirementReject(configuredMode, accountRetirementRejectedValidate, accountRetirementRejectedOn);
    accountDuration();
    PREFER_STORE_INT(&requestInProgress, 0);
    PREFER_STORE_INT(&pendingGauge, signalRequestPending != 0);
    logState("request", "rejected", modeName(configuredMode), "retirement_pending", 0, NULL, 0);
}

void shadowReloadProcess(void) {
    int terminalResult;
    if (!requestInProgress) {
        return;
    }

    if (shadowReloadRetirementComplete(&retirementBlockedRequest, completeRetirementBlockedRequest, NULL)) return;

    pendingReportHash = rsReloadReportDiagnosticHashV1(pendingReport);
    if (configuredMode == RELOAD_ON_HUP_VALIDATE && pendingCandidateResult == RS_RET_OK && pendingReport != NULL &&
        pendingReport->invalidCount == 0) {
        accountDuration();
        PREFER_STORE_INT(&requestInProgress, 0);
        PREFER_STORE_INT(&pendingGauge, signalRequestPending != 0);
        publishStatus(SHADOW_RELOAD_REPORTED, pendingReport);
        logState("request", "reported_only", "none", "none", pendingCandidateObjects, pendingReport, pendingReportHash);
    } else if (configuredMode == RELOAD_ON_HUP_ON && pendingCandidateResult == RS_RET_OK && pendingReport != NULL &&
               pendingReport->invalidCount == 0 && pendingPlan != NULL) {
        if (pendingRulesetGraphBuilder != NULL) {
            struct timespec deadline;
            reloadCommitSignalGuard_t commitGuard;
            uint64_t pauseUsec = 0;
            uint64_t inputPauseStartedUsec = 0;
            memset(&commitGuard, 0, sizeof(commitGuard));
            if (clock_gettime(CLOCK_REALTIME, &deadline) != 0) {
                pendingCandidateResult = RS_RET_ERR;
            } else {
                deadline.tv_sec += 5;
                if (pendingModuleReloadState != NULL) {
                    (void)monotonicUsec(&inputPauseStartedUsec);
                    /* A failed quiesce may still own partially acquired fences. */
                    pendingModuleReloadResume = 1;
                    pendingCandidateResult = modReloadQuiesce(pendingSourceModule, pendingModuleReloadState, &deadline);
                }
                if (pendingCandidateResult == RS_RET_OK)
                    pendingCandidateResult = rsReloadRulesetPlanActivateV1(
                        pendingPlan, &deadline, shadowReloadCancelled, NULL, shadowReloadEnterCommit,
                        shadowReloadLeaveCommit, &commitGuard, publishActivatedGeneration, NULL, &pauseUsec);
                if (pendingModuleReloadResume) {
                    const rsRetVal resumeRet = shadowReloadResumePending(
                        pendingSourceModule, pendingModuleReloadState, &pendingModuleReloadResume,
                        pendingGenerationActivated, &pendingCandidateResult);
                    if (resumeRet != RS_RET_OK) {
                        uint64_t retryStarted = 0;
                        (void)monotonicUsec(&retryStarted);
                        shadowReloadRetirementFailed(&retirementSchedule, retryStarted, resumeRet);
                        publishRetirementStatus();
                        LogError(0, resumeRet,
                                 "shadow_reload: module reload resume failed and remains pending; generation %s",
                                 pendingGenerationActivated ? "already activated" : "not activated");
                    }
                }
                if (inputPauseStartedUsec != 0) {
                    uint64_t inputPauseFinishedUsec;
                    if (monotonicUsec(&inputPauseFinishedUsec) && inputPauseFinishedUsec >= inputPauseStartedUsec)
                        pauseUsec = inputPauseFinishedUsec - inputPauseStartedUsec;
                }
                accountQuiescePause(pauseUsec);
            }
        }
        if (pendingCandidateResult == RS_RET_OK) {
            accountDuration();
            PREFER_STORE_INT(&requestInProgress, 0);
            PREFER_STORE_INT(&pendingGauge, signalRequestPending != 0);
            if (!pendingGenerationActivated) publishStatus(SHADOW_RELOAD_REPORTED, pendingReport);
            logState("request", pendingGenerationActivated ? "activated" : "reported_only", "none", "none",
                     pendingCandidateObjects, pendingReport, pendingReportHash);
        } else {
            pendingFailurePhase = SHADOW_RELOAD_FAILURE_ACTIVATION;
            goto rejected;
        }
    } else if (configuredMode == RELOAD_ON_HUP_VALIDATE || configuredMode == RELOAD_ON_HUP_ON) {
    rejected:
        terminalResult = rejectedResult();
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
        PREFER_STORE_INT(&requestInProgress, 0);
        PREFER_STORE_INT(&pendingGauge, signalRequestPending != 0);
        publishStatus(terminalResult, pendingReport);
        logState("request", "rejected", modeName(configuredMode), rejectedReason(terminalResult),
                 pendingCandidateObjects, pendingReport, pendingReportHash);
    } else {
        accountDuration();
        PREFER_STORE_INT(&requestInProgress, 0);
        PREFER_STORE_INT(&pendingGauge, signalRequestPending != 0);
        publishStatus(SHADOW_RELOAD_IGNORED, NULL);
        logState("request", "ignored", "none", "mode_off", 0, NULL, 0);
    }
    rsReloadCandidateDestruct(&pendingCandidate);
    rsReloadReportDestructV1(&pendingReport);
    rsReloadRulesetPlanDestructV1(&pendingPlan);
    rsReloadNormalizedGraphBuilderV1Destruct(&pendingRulesetGraphBuilder);
    rsReloadNormalizedGraphBuilderV1Destruct(&retiredRulesetGraphBuilder);
    rsReloadCandidateDestruct(&retiredSourceObjectCatalog);
    const rsRetVal cleanupRet = destructPendingSourceModule(0);
    if (cleanupRet != RS_RET_OK && cleanupRet != RS_RET_RETRY)
        LogError(0, cleanupRet, "shadow_reload: module reload retirement remains pending for retry");
    rsReloadCandidateDestruct(&pendingSourceObjectCatalog);
}
