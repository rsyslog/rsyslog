/*
 * Unit coverage for the dormant module reload capability contract.  The
 * oracle is an exact capability result from synthetic module records, so no
 * daemon configuration, activation, or timing is involved.
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rsyslog.h"
#include "modules.h"

/* Keep this small API unit independent of the full runtime link closure. */
#include "../../runtime/module-reload.c"

#define CHECK(condition)                                                                    \
    do {                                                                                    \
        if (!(condition)) {                                                                 \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            exit(1);                                                                        \
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

int main(void) {
    modInfo_t legacy;
    memset(&legacy, 0, sizeof(legacy));
    CHECK(!modReloadHasLifecycleHooks(&legacy));
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

    puts("module reload capability tests passed");
    return 0;
}
