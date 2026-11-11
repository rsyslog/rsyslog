/* SPDX-License-Identifier: Apache-2.0
 * reload-normalized-graph.h
 *
 * Owned normalized-graph builder foundation for reload reports.
 * The builder copies canonical identity and fingerprint strings.  It does not
 * inspect cnfobj or nvlst: both configuration frontends must later supply the
 * same canonical values through this deliberately narrow boundary.
 */
#ifndef RELOAD_NORMALIZED_GRAPH_H_INCLUDED
#define RELOAD_NORMALIZED_GRAPH_H_INCLUDED 1

#include <stddef.h>

#include "modules.h"

/** @file reload-normalized-graph.h
 * @brief Owned graph construction and borrowed V1 enumeration contracts.
 */

#define RS_RELOAD_NORMALIZED_GRAPH_V1 1
#define RS_RELOAD_NORMALIZED_NODE_V1 1
#define RS_RELOAD_NORMALIZED_GRAPH_BUILDER_V1 1

/** @brief Append-only stable graph object kinds. */
typedef enum eRsReloadObjectKind_ {
    RS_RELOAD_OBJ_GLOBAL,
    RS_RELOAD_OBJ_MODULE,
    RS_RELOAD_OBJ_INPUT,
    RS_RELOAD_OBJ_RULESET,
    RS_RELOAD_OBJ_TEMPLATE,
    RS_RELOAD_OBJ_ACTION,
    RS_RELOAD_OBJ_PARSER,
    RS_RELOAD_OBJ_MAIN_QUEUE,
    RS_RELOAD_OBJ_TIMEZONE,
    RS_RELOAD_OBJ_LOOKUP_TABLE,
    RS_RELOAD_OBJ_DYN_STATS,
    RS_RELOAD_OBJ_PERCTILE_STATS,
    RS_RELOAD_OBJ_RATELIMIT,
    RS_RELOAD_OBJ_PROPERTY,
    RS_RELOAD_OBJ_CONSTANT,
    RS_RELOAD_OBJ_LEGACY_OPAQUE,
    RS_RELOAD_OBJ_OPAQUE,
    RS_RELOAD_OBJ_COUNT
} rsReloadObjectKind_t;

/** @brief Borrowed canonical object description; V1 layout is frozen.
 * Strings and optional module/config pointers remain producer-owned. A visitor
 * must copy anything it retains beyond its callback; enumeration transfers no
 * ownership or module references.
 */
typedef struct rsReloadNormalizedNodeV1_s {
    unsigned version;
    size_t structSize;
    rsReloadObjectKind_t objectKind;
    const char *identity; /* canonical identity, owned by its graph producer */
    const char *fingerprint; /* canonical content fingerprint, owned by its graph producer */
    const modInfo_t *pModule; /* optional module owner */
    const void *pModuleCnf; /* optional opaque module configuration */
} rsReloadNormalizedNodeV1_t;

/**
 * @brief Consume one borrowed node synchronously during enumeration.
 * @param node Producer-owned node, valid for the callback's duration.
 * @param context Opaque visitor-owned state, passed through unchanged.
 * @return RS_RET_OK to continue; any other value stops enumeration unchanged.
 */
typedef rsRetVal (*rsReloadGraphVisitorV1_t)(const rsReloadNormalizedNodeV1_t *node, void *context);

/** @brief Borrowed graph view; keep its producer alive through enumeration.
 * The enumerate callback borrows context and both visitor arguments, calls the
 * visitor synchronously, and propagates its first failure. The V1 layout is frozen.
 */
typedef struct rsReloadNormalizedGraphV1_s {
    unsigned version;
    size_t structSize;
    void *context;
    rsRetVal (*enumerate)(void *context, rsReloadGraphVisitorV1_t visitor, void *visitorContext);
} rsReloadNormalizedGraphV1_t;

/** @brief Opaque owner of copied canonical nodes and any graph views. */
typedef struct rsReloadNormalizedGraphBuilderV1_s rsReloadNormalizedGraphBuilderV1_t;

/**
 * @brief Construct an owned normalized graph builder.
 * Construct an empty owned graph builder foundation.  The Add operation copies
 * identity and fingerprint and creates no borrowed module state.  A
 * future producer must establish an owned module-reference contract before it
 * can add module-owned nodes.
 * Duplicate identities are retained so rsReloadReportBuildV1 can report them
 * as invalid input.  This is not yet a RainerScript/YAML producer and makes no
 * frontend-parity claim.
 * @param ppBuilder Non-NULL output owner whose value must be NULL on entry.
 * @return RS_RET_OK, RS_RET_PARAM_ERROR, or RS_RET_OUT_OF_MEMORY. Failure
 * preserves the output owner. Release success with the matching Destruct API.
 */
rsRetVal rsReloadNormalizedGraphBuilderV1Construct(rsReloadNormalizedGraphBuilderV1_t **ppBuilder);
/**
 * @brief Append a copied node to an unsealed builder.
 * @param builder Borrowed, unsealed builder owned by the caller.
 * @param objectKind Valid normalized object category.
 * @param identity Borrowed canonical identity, copied before returning.
 * @param fingerprint Borrowed canonical fingerprint, copied before returning.
 * @return RS_RET_OK on success, parameter or allocation error otherwise.
 * Existing nodes remain owned by the builder on failure; duplicates are retained.
 */
rsRetVal rsReloadNormalizedGraphBuilderV1Add(rsReloadNormalizedGraphBuilderV1_t *builder,
                                             rsReloadObjectKind_t objectKind,
                                             const char *identity,
                                             const char *fingerprint);

/**
 * @brief Seal a builder and return a borrowed deterministic graph view.
 * Produce a graph view with deterministic (kind, identity, fingerprint)
 * enumeration.  Producing seals the builder; subsequent Add calls fail.  The
 * returned view borrows the builder and becomes invalid when the builder is
 * destroyed.  Enumeration is read-only and can be called repeatedly.
 * @param builder Borrowed builder; remains owned by its caller.
 * @param graph Non-NULL output view; no ownership is transferred.
 * @return RS_RET_OK on success, RS_RET_PARAM_ERROR for invalid arguments.
 */
rsRetVal rsReloadNormalizedGraphBuilderV1GetGraph(rsReloadNormalizedGraphBuilderV1_t *builder,
                                                  rsReloadNormalizedGraphV1_t *graph);
/**
 * @brief Release a builder, its copied strings, and invalidate its graph views.
 * @param ppBuilder Owner to release and reset to NULL; NULL/empty is accepted.
 */
void rsReloadNormalizedGraphBuilderV1Destruct(rsReloadNormalizedGraphBuilderV1_t **ppBuilder);

#endif /* RELOAD_NORMALIZED_GRAPH_H_INCLUDED */
