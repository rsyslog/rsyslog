/* Main-thread coordinator for Release B shadow configuration validation.
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

rsRetVal shadowReloadInit(void);
void shadowReloadExit(void);

/* Configure the Release-B manager after the active configuration exists.
 * Ownership of sourceGraphBuilder is transferred on every return path. */
rsRetVal shadowReloadConfigure(reloadOnHUPMode_t mode,
                               const char *configPath,
                               rsReloadNormalizedGraphBuilderV1_t *sourceGraphBuilder);

/* Return the active generation's normalized ruleset fingerprint. The
 * returned pointer remains owned by the reload manager. This control-path
 * accessor exists so testbench diagnostics do not rebuild the graph. */
rsRetVal shadowReloadGetRulesetFingerprint(const char *name, const char **ppFingerprint);

/* Copy the last terminal request result into a caller-owned buffer. This is a
 * testbench/control-plane accessor and performs no message-path work. */
rsRetVal shadowReloadGetStatus(char *buffer, size_t bufferSize);

/* This is called from the SIGHUP handler. It must remain limited to setting
 * a sig_atomic_t flag; the main thread performs all accounting and logging. */
void shadowReloadRequestFromSignal(void);

/* Begin pre-legacy accounting for the HUP cycle that the main loop is about
 * to run. Future candidate validation must happen at this seam. */
void shadowReloadBeginRequest(void);

/* Called by doHUP() after the historic HUP hooks have completed. */
void shadowReloadProcess(void);

#endif /* INCLUDED_SHADOW_RELOAD_H */
