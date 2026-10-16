/** SPDX-License-Identifier: Apache-2.0
 * @file
 * @brief Main-thread coordinator for fail-closed reload request accounting.
 *
 * SIGHUP handling remains in rsyslogd.c. The option plumbing calls
 * shadowReloadConfigure(), which records whether the requested policy is
 * supported by this release. Candidate processing intentionally remains
 * disabled until validation can prevent all external configuration side
 * effects.
 */
#ifndef INCLUDED_SHADOW_RELOAD_H
#define INCLUDED_SHADOW_RELOAD_H

#include "typedefs.h"
#include "rsconf.h"

/** @brief Initialize main-thread accounting and register owned statistics.
 * @return RS_RET_OK on success; otherwise unwind acquired statistics resources.
 * Call exit only after successful initialization.
 */
rsRetVal shadowReloadInit(void);
/** @brief Release statistics after successful init, with no concurrent users. */
void shadowReloadExit(void);

/** @brief Select foundation reporting policy on the main thread.
 * @param mode Parsed HUP mode; the active configuration must already exist.
 * This only selects reporting policy; it neither parses nor activates a
 * candidate configuration. */
void shadowReloadConfigure(reloadOnHUPMode_t mode);

/** @brief Borrow the active generation's normalized ruleset fingerprint.
 * This control-path accessor lets diagnostics avoid rebuilding the graph.
 * @param name Borrowed ruleset name to find in the active graph.
 * @param ppFingerprint Output borrowed string, valid until manager teardown.
 * @return RS_RET_OK on success; parameter, allocation or not-found error
 * otherwise. No ownership is transferred.
 */
rsRetVal shadowReloadGetRulesetFingerprint(const char *name, const char **ppFingerprint);

/** @brief Record a coalesced signal-safe request without logging or allocation.
 * This is called from the SIGHUP handler. It must remain limited to setting
 * a sig_atomic_t flag; the main thread performs all accounting and logging. */
void shadowReloadRequestFromSignal(void);

/** @brief Begin main-thread accounting exactly once before the legacy HUP cycle.
 * Begin pre-legacy accounting for the HUP cycle that the main loop is about
 * to run. Future candidate validation must happen at this seam. */
void shadowReloadBeginRequest(void);

/** @brief Complete accounting after legacy hooks; enabled modes fail closed.
 * Main-thread only, paired with begin; a call without begin is a no-op.
 * Does not construct or activate a candidate or undo legacy-hook effects.
 */
void shadowReloadProcess(void);

#endif /* INCLUDED_SHADOW_RELOAD_H */
