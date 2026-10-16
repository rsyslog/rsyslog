/** SPDX-License-Identifier: Apache-2.0
 * @file
 * @brief Main-thread coordinator for fail-closed reload request accounting.
 *
 * SIGHUP handling remains in rsyslogd.c. The option plumbing calls
 * shadowReloadConfigure(), which records the requested policy and master
 * configuration path. Validate mode produces a side-effect-free,
 * source-syntactic diff report; semantic preparation and activation remain
 * fail-closed.
 */
#ifndef INCLUDED_SHADOW_RELOAD_H
#define INCLUDED_SHADOW_RELOAD_H

#include <stddef.h>

#include "typedefs.h"
#include "rsconf.h"
#include "reload-normalized-graph.h"

/** @brief Initialize main-thread accounting and register owned statistics.
 * @return RS_RET_OK on success; otherwise unwind acquired statistics resources.
 * Call exit only after successful initialization.
 */
rsRetVal shadowReloadInit(void);
/** @brief Release statistics after successful init, with no concurrent users. */
void shadowReloadExit(void);

/** @brief Configure private candidate reporting after the active configuration exists.
 * @param mode Parsed HUP mode; the active configuration must already exist.
 * @param configPath Borrowed master path; a private copy is retained.
 * @param sourceGraphBuilder Ownership transfers on every return path.
 * @return RS_RET_OK, including an unavailable baseline reported fail-closed,
 * or RS_RET_OUT_OF_MEMORY when copying the path fails. Main-thread only;
 * does not activate a candidate or change active runtime objects.
 */
rsRetVal shadowReloadConfigure(reloadOnHUPMode_t mode,
                               const char *configPath,
                               rsReloadNormalizedGraphBuilderV1_t *sourceGraphBuilder);

/** @brief Borrow the active generation's normalized ruleset fingerprint.
 * This control-path accessor lets diagnostics avoid rebuilding the graph.
 * @param name Borrowed ruleset name to find in the active graph.
 * @param ppFingerprint Output borrowed string, valid until manager teardown.
 * @return RS_RET_OK on success; parameter, allocation or not-found error
 * otherwise. No ownership is transferred.
 */
rsRetVal shadowReloadGetRulesetFingerprint(const char *name, const char **ppFingerprint);

/** @brief Copy the last terminal request result into a caller-owned buffer.
 * @param buffer Writable caller-owned destination; no ownership transfers.
 * @param bufferSize Available bytes, including the terminating NUL.
 * @return RS_RET_OK, RS_RET_PARAM_ERROR for invalid storage, or RS_RET_OUT_OF_MEMORY
 * when the supplied buffer cannot hold the full status. Control-plane only.
 */
rsRetVal shadowReloadGetStatus(char *buffer, size_t bufferSize);

/** @brief Record a coalesced signal-safe request without logging or allocation.
 * This is called from the SIGHUP handler. It must remain limited to setting
 * a sig_atomic_t flag; the main thread performs all accounting and logging. */
void shadowReloadRequestFromSignal(void);

/** @brief Begin main-thread accounting exactly once before the legacy HUP cycle.
 * Begin pre-legacy accounting for the HUP cycle that the main loop is about
 * to run. Private candidate parsing and structural reporting preparation occur
 * at this seam; no active runtime objects are changed. */
void shadowReloadBeginRequest(void);

/** @brief Complete accounting after legacy hooks; enabled modes fail closed.
 * Main-thread only, paired with begin; a call without begin is a no-op.
 * Publishes the prepared structural result without activating a candidate
 * or undoing legacy-hook effects.
 */
void shadowReloadProcess(void);

#endif /* INCLUDED_SHADOW_RELOAD_H */
