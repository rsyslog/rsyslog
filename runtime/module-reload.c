/* module-reload.c
 *
 * Conservative capability helpers for the optional module reload lifecycle.
 * No current configuration path invokes these helpers.
 */
#include "config.h"
#include "rsyslog.h"
#include "modules.h"

/*
 * Future reload orchestration must call these helpers instead of directly
 * trusting optional module callbacks.  They deliberately reduce absent,
 * failed, and partial extensions to the safe restart-required result.
 */
sbool modReloadHasValidInterfaceV1(const modInfo_t *pMod) {
    return pMod != NULL && pMod->reloadV1.version == eMOD_RELOAD_INTERFACE_V1 &&
           pMod->reloadV1.structSize >= sizeof(pMod->reloadV1);
}

sbool modReloadHasLifecycleHooks(const modInfo_t *pMod) {
    const unsigned requiredFlags = eMOD_RELOAD_CAP_VALIDATE_PRIVATE | eMOD_RELOAD_CAP_PREPARE | eMOD_RELOAD_CAP_REUSE |
                                   eMOD_RELOAD_CAP_COMMIT | eMOD_RELOAD_CAP_RETIRE;

    return modReloadHasValidInterfaceV1(pMod) && (pMod->reloadV1.capabilityFlags & requiredFlags) == requiredFlags &&
           pMod->reloadV1.prepare != NULL && pMod->reloadV1.commit != NULL && pMod->reloadV1.abort != NULL &&
           pMod->reloadV1.retire != NULL;
}

eModReloadCapability_t modReloadClassify(const modInfo_t *pMod, const void *pOldCnf, const void *pNewCnf) {
    eModReloadCapability_t capability = eMOD_RELOAD_RESTART_REQUIRED;

    if (!modReloadHasValidInterfaceV1(pMod) || pMod->reloadV1.classify == NULL) return capability;
    if (pMod->reloadV1.classify(pOldCnf, pNewCnf, &capability) != RS_RET_OK) return eMOD_RELOAD_RESTART_REQUIRED;

    switch (capability) {
        case eMOD_RELOAD_LIVE_SWAP:
        case eMOD_RELOAD_NEW_SESSIONS:
        case eMOD_RELOAD_DRAIN_REPLACE:
            return modReloadHasLifecycleHooks(pMod) ? capability : eMOD_RELOAD_RESTART_REQUIRED;
        case eMOD_RELOAD_RESTART_REQUIRED:
        default:
            return eMOD_RELOAD_RESTART_REQUIRED;
    }
}
