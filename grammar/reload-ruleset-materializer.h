/* SPDX-License-Identifier: Apache-2.0
 */
/** @file reload-ruleset-materializer.h
 * @brief Private, abort-safe preparation and batch-boundary activation of named
 * ruleset plans.
 */
#ifndef RELOAD_RULESET_MATERIALIZER_H_INCLUDED
#define RELOAD_RULESET_MATERIALIZER_H_INCLUDED 1

#include "reload-candidate.h"
#include "reload-report.h"
#include "rsconf.h"

/** @brief Owned private roots and borrowed live actions for one activation. */
typedef struct rsReloadRulesetPlanV1_s rsReloadRulesetPlanV1_t;
/** @brief Read cancellation state without consuming caller-owned context. */
typedef sbool (*rsReloadCancellationCheckV1_t)(void *context);
/** @brief Publish caller-owned generation metadata inside the commit fence. */
typedef void (*rsReloadCommitPublishV1_t)(void *context);
/** @brief Enter caller's commit fence; failure forbids publication. */
typedef rsRetVal (*rsReloadCommitEnterV1_t)(void *context);
/** @brief Release the commit fence after every successful enter callback. */
typedef void (*rsReloadCommitLeaveV1_t)(void *context);

/** @brief Prepare supported named-ruleset bodies and name-only additions privately.
 * @param active Borrowed live configuration, kept alive until plan destruction.
 * @param activeSourceCatalog Borrowed catalog proving exclusive named-policy
 * usage when a rate-policy declaration changes.
 * @param candidate Borrowed immutable syntax; cloned roots become plan-owned.
 * @param report Borrowed source diff; unsupported non-ruleset objects must be unchanged.
 * @param sourceCapability Private effective classifier result authorizing imtcp
 * module/input entries only for REUSE, LIVE_SWAP, NEW_SESSIONS or
 * LIVE_AND_NEW_SESSIONS. Other values
 * retain the ruleset-only gate. REUSE may publish a new source baseline without
 * changing runtime objects.
 * @param out Initially NULL output, owned by caller only on success.
 * @return RS_RET_OK or a fail-closed capability/allocation error. No live state
 * changes on failure. Takes no locks; caller serializes configuration mutation.
 * Preparation does not instantiate actions; borrowed actions stay active-owned.
 * Additions own private shells/nodes until commit and may only call existing
 * actions through rulesets. No input changes accompany membership additions.
 */
rsRetVal rsReloadRulesetPlanPrepareV1(rsconf_t *active,
                                      const rsReloadCandidate_t *activeSourceCatalog,
                                      const rsReloadCandidate_t *candidate,
                                      const rsReloadReportV1_t *report,
                                      eModReloadCapability_t sourceCapability,
                                      int baseAuthorized,
                                      rsReloadRulesetPlanV1_t **out);
/** @brief Free private plan resources and NULL the caller's pointer.
 * Before commit, borrowed actions are not destroyed. After commit, roots and
 * transferred actions belong to active rulesets, not the plan. NULL is harmless.
 * Caller excludes concurrent use; no locks are acquired.
 */
void rsReloadRulesetPlanDestructV1(rsReloadRulesetPlanV1_t **plan);
/** @brief Return the prepared root count (zero for NULL), without taking locks. */
size_t rsReloadRulesetPlanCountV1(const rsReloadRulesetPlanV1_t *plan);

/** @brief Activate private roots together after queue batch-boundary barriers.
 * @param plan Caller-owned prepared plan; may commit only once.
 * @param deadline One absolute CLOCK_REALTIME deadline for all queue waits.
 * @param cancelled Optional cancellation probe, checked before entering commit.
 * @param cancelContext Borrowed cancellation context.
 * @param enterCommit Optional caller fence acquisition, paired with leaveCommit.
 * @param leaveCommit Release callback, called after every successful enter.
 * @param commitContext Borrowed fence context.
 * @param publish Optional infallible metadata publication inside the fence.
 * @param publishContext Borrowed publication context.
 * @param pauseUsec Optional output measuring queue pause time, including abort.
 * @return RS_RET_OK or an error with roots/action ownership unchanged. All
 * Main, every existing ruleset queue, and non-Direct action queues are
 * conservatively fenced; Direct entry queues and local graphs are unsupported.
 * All barriers begin before any wait, including disk-assisted child workers.
 * Acquired barriers are released on timeout, cancellation, and failure. Success
 * transfers roots/action ownership to active rulesets and retires old roots.
 * Caller serializes reloads and holds no queue/pool mutex; this function owns
 * barriers on its calling thread and acquires their mutexes internally.
 */
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
