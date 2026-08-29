/*
 * Unit coverage for the dormant module reload capability contract.  The
 * oracle is an exact capability result from synthetic module records, so no
 * daemon configuration, activation, or timing is involved.
 */
#include "config.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "rsyslog.h"
#include "modules.h"

/* Keep this small API unit independent of the full runtime link closure. */
#include "../../runtime/module-reload.c"

#define CHECK(condition)                                                                    \
    do {                                                                                    \
        if (!(condition)) {                                                                 \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            return 1;                                                                       \
        }                                                                                   \
    } while (0)

static rsRetVal classify_live_swap(const void __attribute__((unused)) * pOldCnf,
                                   const void __attribute__((unused)) * pNewCnf,
                                   eModReloadCapability_t *pCapability) {
    *pCapability = eMOD_RELOAD_LIVE_SWAP;
    return RS_RET_OK;
}

static rsRetVal classify_new_sessions(const void __attribute__((unused)) * pOldCnf,
                                      const void __attribute__((unused)) * pNewCnf,
                                      eModReloadCapability_t *pCapability) {
    *pCapability = eMOD_RELOAD_NEW_SESSIONS;
    return RS_RET_OK;
}

static rsRetVal classify_drain_replace(const void __attribute__((unused)) * pOldCnf,
                                       const void __attribute__((unused)) * pNewCnf,
                                       eModReloadCapability_t *pCapability) {
    *pCapability = eMOD_RELOAD_DRAIN_REPLACE;
    return RS_RET_OK;
}

static rsRetVal classify_invalid(const void __attribute__((unused)) * pOldCnf,
                                 const void __attribute__((unused)) * pNewCnf,
                                 eModReloadCapability_t *pCapability) {
    *pCapability = (eModReloadCapability_t)99;
    return RS_RET_OK;
}

static rsRetVal reload_prepare(const void __attribute__((unused)) * pOldCnf,
                               const void __attribute__((unused)) * pNewCnf,
                               void **pReloadState) {
    *pReloadState = NULL;
    return RS_RET_OK;
}

static void reload_commit_or_abort(void __attribute__((unused)) * pReloadState) {}

static rsRetVal reload_retire(void __attribute__((unused)) * pReloadState) {
    return RS_RET_OK;
}

static int sourceSentinel;
static int sourceDestructCount;

static rsRetVal source_build(const modReloadSourceBuildContextV1_t __attribute__((unused)) * context,
                             void **candidateCnf) {
    *candidateCnf = &sourceSentinel;
    return RS_RET_OK;
}

static rsRetVal source_build_null(const modReloadSourceBuildContextV1_t __attribute__((unused)) * context,
                                  void __attribute__((unused)) * *candidateCnf) {
    return RS_RET_OK;
}

static rsRetVal source_build_failure(const modReloadSourceBuildContextV1_t __attribute__((unused)) * context,
                                     void **candidateCnf) {
    *candidateCnf = &sourceSentinel;
    return RS_RET_OUT_OF_MEMORY;
}

static void source_destruct(void **candidateCnf) {
    ++sourceDestructCount;
    *candidateCnf = NULL;
}

static rsRetVal source_classify(const void *const oldCnf,
                                const void *const newCnf,
                                eModReloadCapability_t *const capability) {
    if (oldCnf == NULL || newCnf == NULL || capability == NULL) return RS_RET_PARAM_ERROR;
    *capability = oldCnf == newCnf ? eMOD_RELOAD_REUSE : eMOD_RELOAD_RESTART_REQUIRED;
    return RS_RET_OK;
}

static rsRetVal source_classify_failure(const void __attribute__((unused)) * oldCnf,
                                        const void __attribute__((unused)) * newCnf,
                                        eModReloadCapability_t *const capability) {
    *capability = eMOD_RELOAD_REUSE;
    return RS_RET_ERR;
}

static rsRetVal source_classify_invalid(const void __attribute__((unused)) * oldCnf,
                                        const void __attribute__((unused)) * newCnf,
                                        eModReloadCapability_t *const capability) {
    *capability = (eModReloadCapability_t)99;
    return RS_RET_OK;
}

int main(void) {
    modInfo_t legacy;
    modReloadSourceBuildContextV1_t sourceContext;
    void *candidateCnf = NULL;
    memset(&legacy, 0, sizeof(legacy));
    CHECK(!modReloadHasLifecycleHooks(&legacy));
    CHECK(!modReloadHasValidSourceInterfaceV1(&legacy));
    CHECK(modReloadClassify(&legacy, NULL, NULL) == eMOD_RELOAD_RESTART_REQUIRED);
    CHECK(modReloadClassify(NULL, NULL, NULL) == eMOD_RELOAD_RESTART_REQUIRED);

    legacy.reloadV1.classify = classify_live_swap;
    CHECK(modReloadClassify(&legacy, NULL, NULL) == eMOD_RELOAD_RESTART_REQUIRED);

    legacy.reloadV1.prepare = reload_prepare;
    legacy.reloadV1.commit = reload_commit_or_abort;
    legacy.reloadV1.abort = reload_commit_or_abort;
    legacy.reloadV1.retire = reload_retire;
    legacy.reloadV1.version = eMOD_RELOAD_INTERFACE_V1;
    legacy.reloadV1.structSize = sizeof(legacy.reloadV1);
    CHECK(modReloadHasValidInterfaceV1(&legacy));
    CHECK(modReloadClassify(&legacy, NULL, NULL) == eMOD_RELOAD_RESTART_REQUIRED);
    legacy.reloadV1.capabilityFlags = eMOD_RELOAD_CAP_VALIDATE_PRIVATE | eMOD_RELOAD_CAP_PREPARE |
                                      eMOD_RELOAD_CAP_REUSE | eMOD_RELOAD_CAP_COMMIT | eMOD_RELOAD_CAP_RETIRE;
    CHECK(modReloadHasLifecycleHooks(&legacy));
    CHECK(modReloadClassify(&legacy, NULL, NULL) == eMOD_RELOAD_LIVE_SWAP);

    legacy.reloadV1.classify = classify_new_sessions;
    CHECK(modReloadClassify(&legacy, NULL, NULL) == eMOD_RELOAD_NEW_SESSIONS);
    legacy.reloadV1.classify = classify_drain_replace;
    CHECK(modReloadClassify(&legacy, NULL, NULL) == eMOD_RELOAD_DRAIN_REPLACE);
    legacy.reloadV1.retire = NULL;
    CHECK(!modReloadHasLifecycleHooks(&legacy));
    CHECK(modReloadClassify(&legacy, NULL, NULL) == eMOD_RELOAD_RESTART_REQUIRED);
    legacy.reloadV1.retire = reload_retire;

    legacy.reloadV1.classify = classify_invalid;
    CHECK(modReloadClassify(&legacy, NULL, NULL) == eMOD_RELOAD_RESTART_REQUIRED);

    legacy.reloadSourceV1.version = eMOD_RELOAD_SOURCE_INTERFACE_V1;
    legacy.reloadSourceV1.structSize = sizeof(legacy.reloadSourceV1);
    CHECK(!modReloadHasValidSourceInterfaceV1(&legacy));
    legacy.reloadSourceV1.buildCandidate = source_build;
    CHECK(!modReloadHasValidSourceInterfaceV1(&legacy));
    legacy.reloadSourceV1.destructCandidate = source_destruct;
    CHECK(modReloadHasValidSourceInterfaceV1(&legacy));
    CHECK(modReloadClassifySourceCandidateV1(&legacy, &sourceSentinel, &sourceSentinel, NULL) == RS_RET_PARAM_ERROR);
    legacy.reloadSourceV1.structSize = offsetof(modReloadSourceInterfaceV1_t, classifyCandidate);
    eModReloadCapability_t sourceCapability = eMOD_RELOAD_REUSE;
    CHECK(modReloadClassifySourceCandidateV1(&legacy, &sourceSentinel, &sourceSentinel, &sourceCapability) ==
          RS_RET_OK);
    CHECK(sourceCapability == eMOD_RELOAD_RESTART_REQUIRED);
    legacy.reloadSourceV1.structSize = sizeof(legacy.reloadSourceV1);
    legacy.reloadSourceV1.classifyCandidate = source_classify;
    memset(&sourceContext, 0, sizeof(sourceContext));
    sourceContext.version = MOD_RELOAD_SOURCE_BUILD_CONTEXT_V1;
    sourceContext.structSize = sizeof(sourceContext);
    CHECK(modReloadBuildSourceCandidateV1(&legacy, &sourceContext, &candidateCnf) == RS_RET_OK);
    CHECK(candidateCnf == &sourceSentinel);
    sourceCapability = eMOD_RELOAD_RESTART_REQUIRED;
    CHECK(modReloadClassifySourceCandidateV1(&legacy, candidateCnf, candidateCnf, &sourceCapability) == RS_RET_OK);
    CHECK(sourceCapability == eMOD_RELOAD_REUSE);
    legacy.reloadSourceV1.classifyCandidate = source_classify_failure;
    CHECK(modReloadClassifySourceCandidateV1(&legacy, candidateCnf, candidateCnf, &sourceCapability) == RS_RET_ERR);
    CHECK(sourceCapability == eMOD_RELOAD_RESTART_REQUIRED);
    legacy.reloadSourceV1.classifyCandidate = source_classify_invalid;
    CHECK(modReloadClassifySourceCandidateV1(&legacy, candidateCnf, candidateCnf, &sourceCapability) ==
          RS_RET_PARAM_ERROR);
    CHECK(sourceCapability == eMOD_RELOAD_RESTART_REQUIRED);
    legacy.reloadSourceV1.classifyCandidate = source_classify;
    modReloadDestructSourceCandidateV1(&legacy, &candidateCnf);
    CHECK(candidateCnf == NULL);
    CHECK(sourceDestructCount == 1);
    modReloadDestructSourceCandidateV1(&legacy, &candidateCnf);
    CHECK(sourceDestructCount == 1);

    legacy.reloadSourceV1.buildCandidate = source_build_null;
    CHECK(modReloadBuildSourceCandidateV1(&legacy, &sourceContext, &candidateCnf) == RS_RET_ERR);
    CHECK(candidateCnf == NULL);
    legacy.reloadSourceV1.buildCandidate = source_build_failure;
    CHECK(modReloadBuildSourceCandidateV1(&legacy, &sourceContext, &candidateCnf) == RS_RET_OUT_OF_MEMORY);
    CHECK(candidateCnf == NULL);
    CHECK(sourceDestructCount == 2);
    legacy.reloadSourceV1.buildCandidate = source_build;
    legacy.reloadSourceV1.structSize =
        offsetof(modReloadSourceInterfaceV1_t, destructCandidate) + sizeof(legacy.reloadSourceV1.destructCandidate) - 1;
    CHECK(!modReloadHasValidSourceInterfaceV1(&legacy));

    puts("module reload capability tests passed");
    return 0;
}
