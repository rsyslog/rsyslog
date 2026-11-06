/* SPDX-License-Identifier: Apache-2.0
 * Main-thread-only, validate-only HUP checkpoint. No runtime publication.
 */
#ifndef RS_RELOAD_VALIDATE_H_INCLUDED
#define RS_RELOAD_VALIDATE_H_INCLUDED
#include "rsyslog.h"
struct cnfobj;
/* Startup observation precedes runtime dispatch. OFF allocates no graph. */
void rsReloadValidateObserveObject(const struct cnfobj *object);
void rsReloadValidateObserveOther(void);
/* Candidate-only mode checks; never changes the startup-selected mode. */
void rsReloadValidateCandidateBegin(void);
void rsReloadValidateCandidateFinish(void);
/* Owned absolute include path using the startup resolution directory. */
char *rsReloadValidateResolvePath(const char *path);
void rsReloadValidateCandidateObject(const struct cnfobj *object);
/* Seal the source graph after startup load; invalid mode selection falls back to OFF. */
rsRetVal rsReloadValidateStartupFinish(const char *path, rsRetVal loadResult, int unclean);
/* Called after the existing HUP hooks on the main thread, never a handler. */
void rsReloadValidateHUP(void);
void rsReloadValidateDestruct(void);
#endif
