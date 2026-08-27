/* Main-thread coordinator for Release B shadow configuration validation.
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

rsRetVal shadowReloadInit(void);
void shadowReloadExit(void);

/* Configure the Release-B manager after the active configuration exists.
 * This only selects reporting policy; it neither parses nor activates a
 * candidate configuration. */
void shadowReloadConfigure(reloadOnHUPMode_t mode);

/* This is called from the SIGHUP handler. It must remain limited to setting
 * a sig_atomic_t flag; the main thread performs all accounting and logging. */
void shadowReloadRequestFromSignal(void);

/* Begin pre-legacy accounting for the HUP cycle that the main loop is about
 * to run. Future candidate validation must happen at this seam. */
void shadowReloadBeginRequest(void);

/* Called by doHUP() after the historic HUP hooks have completed. */
void shadowReloadProcess(void);

#endif /* INCLUDED_SHADOW_RELOAD_H */
