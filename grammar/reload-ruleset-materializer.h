/* reload-ruleset-materializer.h
 * Private, abort-safe preparation and batch-boundary activation of existing
 * ruleset plans.
 */
#ifndef RELOAD_RULESET_MATERIALIZER_H_INCLUDED
#define RELOAD_RULESET_MATERIALIZER_H_INCLUDED 1

#include "reload-candidate.h"
#include "reload-report.h"
#include "rsconf.h"

typedef struct rsReloadRulesetPlanV1_s rsReloadRulesetPlanV1_t;
typedef sbool (*rsReloadCancellationCheckV1_t)(void *context);
typedef void (*rsReloadCommitPublishV1_t)(void *context);
typedef rsRetVal (*rsReloadCommitEnterV1_t)(void *context);
typedef void (*rsReloadCommitLeaveV1_t)(void *context);

/* sourceCapability authorizes additional imtcp module/input report entries
 * only when the caller has already obtained a reloadable result from the
 * private effective module classifier. reloadModeAuthorized independently
 * authorizes the global node only after the controller proved that
 * config.reloadOnHUP is the sole base change. Every other object kind remains
 * fail-closed. REUSE can publish a new source baseline without changing
 * runtime objects. */
rsRetVal rsReloadRulesetPlanPrepareV1(rsconf_t *active,
                                      const rsReloadCandidate_t *activeSourceCatalog,
                                      const rsReloadCandidate_t *candidate,
                                      const rsReloadReportV1_t *report,
                                      eModReloadCapability_t sourceCapability,
                                      int reloadModeAuthorized,
                                      rsReloadRulesetPlanV1_t **out);
void rsReloadRulesetPlanDestructV1(rsReloadRulesetPlanV1_t **plan);
size_t rsReloadRulesetPlanCountV1(const rsReloadRulesetPlanV1_t *plan);

/* Activate a fully prepared named-ruleset-only plan.  This is deliberately a
 * control-path operation: it first quiesces every affected consumer queue at
 * a batch boundary, rechecks cancellation, then performs only pointer/flag
 * transfers.  A non-OK return leaves the active roots and action ownership
 * unchanged.  The caller supplies one absolute CLOCK_REALTIME deadline. */
rsRetVal rsReloadRulesetPlanActivateV1(rsReloadRulesetPlanV1_t *plan,
                                       const struct timespec *deadline,
                                       rsReloadCancellationCheckV1_t cancelled,
                                       void *cancelContext,
                                       rsReloadCommitEnterV1_t enterCommit,
                                       rsReloadCommitLeaveV1_t leaveCommit,
                                       void *commitContext,
                                       rsReloadCommitPublishV1_t publish,
                                       void *publishContext,
                                       uint64_t *pauseUsec);

#endif
