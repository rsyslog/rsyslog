/* SPDX-License-Identifier: Apache-2.0 */
/** @file imtcp-reload-resume.h
 * @brief Cold-path resume guard for borrowed runtime endpoints.
 */
#ifndef INCLUDED_IMTCP_RELOAD_RESUME_H
#define INCLUDED_IMTCP_RELOAD_RESUME_H

#include <stdint.h>
#include "rsyslog.h"

/** @brief Release a borrowed endpoint's fence, dereferencing it only on invocation. */
typedef rsRetVal (*imtcpReloadReleaseFenceV1_t)(void *runtime, uint64_t token);

/** @brief Resume one held fence only while its runtime generation remains live.
 * @param capturedGeneration Generation captured by private preparation.
 * @param currentGeneration Main-thread-owned registry generation.
 * @param runtime Opaque borrowed endpoint; may be stale after generation change.
 * @param token Fence token owned by the reload controller.
 * @param acquired Caller-owned flag; retained on same-generation release error.
 * @param release Callback allowed to dereference runtime only for a live generation.
 * @return Release result, or RS_RET_OK for unheld/stopped fences. A stopped
 * generation clears the held flag without invoking release or touching runtime.
 * The caller clears its stopped runtime borrow but retains ownership of its
 * prepared state until normal abort/retire. No locks are taken: preparation,
 * resume and afterRun are serialized by the core main-thread lifecycle, and
 * afterRun invalidates the generation only after input/worker joins.
 * Ordinary static, not inline: optimized sanitizer builds may keep it out of line.
 */
static rsRetVal imtcpReloadResumeHeldV1(const uint64_t capturedGeneration,
                                        const uint64_t currentGeneration,
                                        void *const runtime,
                                        const uint64_t token,
                                        int *const acquired,
                                        const imtcpReloadReleaseFenceV1_t release) {
    if (!*acquired) return RS_RET_OK;
    if (capturedGeneration != currentGeneration) {
        *acquired = 0;
        return RS_RET_OK;
    }
    const rsRetVal ret = release(runtime, token);
    if (ret == RS_RET_OK) *acquired = 0;
    return ret;
}

#endif
