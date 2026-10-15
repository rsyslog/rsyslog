/* SPDX-License-Identifier: Apache-2.0
 * reload-ruleset-graph.h
 *
 * Normalized ruleset-program graph producer for transactional reload reports.
 */
#ifndef RELOAD_RULESET_GRAPH_H_INCLUDED
#define RELOAD_RULESET_GRAPH_H_INCLUDED 1

#include "reload-normalized-graph.h"
#include "typedefs.h"

/** @file reload-ruleset-graph.h
 * @brief Owned fingerprints and graph producers for optimized rulesets.
 */

#define RS_RELOAD_RULESET_FINGERPRINT_V1 "ruleset-plan-v1"

/**
 * @brief Construct an owned canonical ruleset-program fingerprint.
 * Serialize the optimized ruleset program into an owned, frontend-neutral
 * fingerprint.  The fingerprint covers control flow, expressions, calls and
 * action slots.  It deliberately excludes action configuration, queues and
 * parser chains; those require separate normalized object producers.
 * @param ruleset Borrowed optimized ruleset, not modified by this call.
 * @param ppFingerprint Non-NULL output owner whose value must be NULL on entry.
 * @return RS_RET_OK on success; parameter, unsupported-program or allocation
 * error otherwise. Failure preserves the output owner; free successful output.
 */
rsRetVal rsReloadRulesetFingerprintV1(const ruleset_t *ruleset, char **ppFingerprint);

/**
 * @brief Construct an owned normalized graph of optimized ruleset programs.
 * Build an owned graph containing one RS_RELOAD_OBJ_RULESET node per ruleset.
 * The caller owns the returned builder and may obtain a report graph view from
 * it with rsReloadNormalizedGraphBuilderV1GetGraph().
 * @param conf Borrowed configuration, unchanged and valid throughout this call.
 * @param ppBuilder Non-NULL output owner whose value must be NULL on entry.
 * @return RS_RET_OK on success; parameter, fingerprint or allocation error
 * otherwise. Failure preserves the output owner. Release successful output
 * with rsReloadNormalizedGraphBuilderV1Destruct().
 */
rsRetVal rsReloadRulesetGraphBuildV1(const rsconf_t *conf, rsReloadNormalizedGraphBuilderV1_t **ppBuilder);

#endif /* RELOAD_RULESET_GRAPH_H_INCLUDED */
