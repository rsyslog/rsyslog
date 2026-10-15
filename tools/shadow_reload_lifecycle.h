/* SPDX-License-Identifier: Apache-2.0 */
#ifndef INCLUDED_SHADOW_RELOAD_LIFECYCLE_H
#define INCLUDED_SHADOW_RELOAD_LIFECYCLE_H

/* Reload-control-only helper, deliberately not declared inline: optimized
 * sanitizer builds may keep this cold path out of line. A failed resume
 * retains state ownership for a later retry. Published generations cannot
 * be retroactively rejected. */
static rsRetVal shadowReloadResumePending(const modInfo_t *const module,
                                          void *const state,
                                          int *const pending,
                                          const int published,
                                          rsRetVal *const activationResult) {
    if (!*pending) return RS_RET_OK;
    const rsRetVal ret = modReloadResume(module, state);
    if (ret == RS_RET_OK)
        *pending = 0;
    else if (!published && activationResult != NULL && *activationResult == RS_RET_OK)
        *activationResult = ret;
    return ret;
}

#endif
