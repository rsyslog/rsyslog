/* reload-ruleset-graph.h
 *
 * Normalized ruleset-program graph producer for transactional reload reports.
 */
#ifndef RELOAD_RULESET_GRAPH_H_INCLUDED
#define RELOAD_RULESET_GRAPH_H_INCLUDED 1

#include "reload-normalized-graph.h"
#include "typedefs.h"

#define RS_RELOAD_RULESET_FINGERPRINT_V1 "ruleset-plan-v1"

/*
 * Serialize the optimized ruleset program into an owned, frontend-neutral
 * fingerprint.  The fingerprint covers control flow, expressions, calls and
 * action slots.  It deliberately excludes action configuration, queues and
 * parser chains; those require separate normalized object producers.
 */
rsRetVal rsReloadRulesetFingerprintV1(const ruleset_t *ruleset, char **ppFingerprint);

/*
 * Build an owned graph containing one RS_RELOAD_OBJ_RULESET node per ruleset.
 * The caller owns the returned builder and may obtain a report graph view from
 * it with rsReloadNormalizedGraphBuilderV1GetGraph().
 */
rsRetVal rsReloadRulesetGraphBuildV1(const rsconf_t *conf, rsReloadNormalizedGraphBuilderV1_t **ppBuilder);

#endif /* RELOAD_RULESET_GRAPH_H_INCLUDED */
