/* reload-ruleset-materializer.h
 * Private, abort-safe preparation of existing ruleset plans. No activation.
 */
#ifndef RELOAD_RULESET_MATERIALIZER_H_INCLUDED
#define RELOAD_RULESET_MATERIALIZER_H_INCLUDED 1

#include "reload-candidate.h"
#include "reload-report.h"
#include "rsconf.h"

typedef struct rsReloadRulesetPlanV1_s rsReloadRulesetPlanV1_t;

rsRetVal rsReloadRulesetPlanPrepareV1(rsconf_t *active,
                                      const rsReloadCandidate_t *candidate,
                                      const rsReloadReportV1_t *report,
                                      rsReloadRulesetPlanV1_t **out);
void rsReloadRulesetPlanDestructV1(rsReloadRulesetPlanV1_t **plan);
size_t rsReloadRulesetPlanCountV1(const rsReloadRulesetPlanV1_t *plan);

#endif
