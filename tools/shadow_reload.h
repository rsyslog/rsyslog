/* Main-thread coordinator for Release B shadow configuration validation.
 *
 * SIGHUP handling remains in rsyslogd.c. The option plumbing calls
 * shadowReloadConfigure(), which records the requested policy and master
 * configuration path. Validate mode produces a side-effect-free,
 * source-syntactic diff report. ON mode can activate the narrow supported
 * existing-ruleset plan scope; all other changes remain fail-closed.
 */
#ifndef INCLUDED_SHADOW_RELOAD_H
#define INCLUDED_SHADOW_RELOAD_H

#include <stddef.h>

#include "typedefs.h"
#include "rsconf.h"
#include "reload-candidate.h"
#include "reload-normalized-graph.h"

rsRetVal shadowReloadInit(void);
void shadowReloadExit(void);
/* Release module-owned source snapshots after runConf teardown, while module
 * destructor callbacks are still loaded. */
void shadowReloadExitModuleSnapshots(void);

/* Configure the reload manager after the active configuration exists.
 * Ownership of both source artifacts is transferred on every return path. */
rsRetVal shadowReloadConfigure(reloadOnHUPMode_t mode,
                               const char *configPath,
                               rsReloadNormalizedGraphBuilderV1_t *sourceGraphBuilder,
                               rsReloadCandidate_t *sourceObjectCatalog);

/* Copy the active generation's normalized ruleset fingerprint. The caller
 * owns the returned string. This control-path accessor exists so testbench
 * diagnostics do not rebuild the graph. */
rsRetVal shadowReloadGetRulesetFingerprint(const char *name, char **ppFingerprint);

/* Copy the last terminal request result into a caller-owned buffer. This is a
 * testbench/control-plane accessor and performs no message-path work. */
rsRetVal shadowReloadGetStatus(char *buffer, size_t bufferSize);

/* This is called from the SIGHUP handler. It must remain limited to setting
 * a sig_atomic_t flag; the main thread performs all accounting and logging. */
void shadowReloadRequestFromSignal(void);

/* Begin pre-legacy accounting for the HUP cycle that the main loop is about
 * to run. Future candidate validation must happen at this seam. */
void shadowReloadBeginRequest(void);

/* Account for one actually completed historic HUP-hook cycle. */
void shadowReloadLegacyHooksCompleted(void);

/* Called by doHUP() after the historic HUP hooks have completed. */
void shadowReloadProcess(void);

/* The main thread retries committed module retirement independently of HUP so
 * drained listener generations are released within a bounded interval. */
int shadowReloadRetirementPending(void);
void shadowReloadRetryRetirement(void);

#endif /* INCLUDED_SHADOW_RELOAD_H */
