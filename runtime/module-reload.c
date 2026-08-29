/* module-reload.c
 *
 * Conservative capability helpers for the optional module reload lifecycle.
 * No current configuration path invokes these helpers.
 */
#include "config.h"
#include <stddef.h>
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

sbool modReloadHasValidSourceInterfaceV1(const modInfo_t *pMod) {
    const size_t minimumSize = offsetof(modReloadSourceInterfaceV1_t, destructCandidate) +
                               sizeof(((modReloadSourceInterfaceV1_t *)0)->destructCandidate);
    return pMod != NULL && pMod->reloadSourceV1.version == eMOD_RELOAD_SOURCE_INTERFACE_V1 &&
           pMod->reloadSourceV1.structSize >= minimumSize && pMod->reloadSourceV1.buildCandidate != NULL &&
           pMod->reloadSourceV1.destructCandidate != NULL;
}

rsRetVal modReloadBuildSourceCandidateV1(const modInfo_t *const pMod,
                                         const modReloadSourceBuildContextV1_t *const context,
                                         void **const pCandidateCnf) {
    rsRetVal ret;
    if (!modReloadHasValidSourceInterfaceV1(pMod) || context == NULL || pCandidateCnf == NULL || *pCandidateCnf != NULL)
        return RS_RET_PARAM_ERROR;
    ret = pMod->reloadSourceV1.buildCandidate(context, pCandidateCnf);
    if (ret != RS_RET_OK || *pCandidateCnf == NULL) {
        if (*pCandidateCnf != NULL) pMod->reloadSourceV1.destructCandidate(pCandidateCnf);
        return ret == RS_RET_OK ? RS_RET_ERR : ret;
    }
    return RS_RET_OK;
}

void modReloadDestructSourceCandidateV1(const modInfo_t *const pMod, void **const pCandidateCnf) {
    if (pCandidateCnf == NULL || *pCandidateCnf == NULL) return;
    if (modReloadHasValidSourceInterfaceV1(pMod)) pMod->reloadSourceV1.destructCandidate(pCandidateCnf);
}

sbool modReloadHasLifecycleHooks(const modInfo_t *pMod) {
    const unsigned requiredFlags = eMOD_RELOAD_CAP_VALIDATE_PRIVATE | eMOD_RELOAD_CAP_PREPARE | eMOD_RELOAD_CAP_REUSE |
                                   eMOD_RELOAD_CAP_COMMIT | eMOD_RELOAD_CAP_RETIRE;

    return modReloadHasValidInterfaceV1(pMod) && (pMod->reloadV1.capabilityFlags & requiredFlags) == requiredFlags &&
           pMod->reloadV1.classify != NULL && pMod->reloadV1.prepare != NULL && pMod->reloadV1.commit != NULL &&
           pMod->reloadV1.abort != NULL && pMod->reloadV1.retire != NULL;
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
