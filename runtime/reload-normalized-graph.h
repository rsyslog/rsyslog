/* reload-normalized-graph.h
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

#define RS_RELOAD_NORMALIZED_GRAPH_V1 1
#define RS_RELOAD_NORMALIZED_NODE_V1 1
#define RS_RELOAD_NORMALIZED_GRAPH_BUILDER_V1 1

/* Append-only stable graph object kinds. */
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

/* V1 node and graph layouts are frozen; extensions require V2 types. */
typedef struct rsReloadNormalizedNodeV1_s {
    unsigned version;
    size_t structSize;
    rsReloadObjectKind_t objectKind;
    const char *identity; /* canonical identity, owned by its graph producer */
    const char *fingerprint; /* canonical content fingerprint, owned by its graph producer */
    const modInfo_t *pModule; /* optional module owner */
    const void *pModuleCnf; /* optional opaque module configuration */
} rsReloadNormalizedNodeV1_t;

typedef rsRetVal (*rsReloadGraphVisitorV1_t)(const rsReloadNormalizedNodeV1_t *node, void *context);

typedef struct rsReloadNormalizedGraphV1_s {
    unsigned version;
    size_t structSize;
    void *context;
    rsRetVal (*enumerate)(void *context, rsReloadGraphVisitorV1_t visitor, void *visitorContext);
} rsReloadNormalizedGraphV1_t;

typedef struct rsReloadNormalizedGraphBuilderV1_s rsReloadNormalizedGraphBuilderV1_t;

/*
 * Construct an empty owned graph builder foundation.  The Add operation copies
 * identity and fingerprint and creates no borrowed module state.  A
 * future producer must establish an owned module-reference contract before it
 * can add module-owned nodes.
 * Duplicate identities are retained so rsReloadReportBuildV1 can report them
 * as invalid input.  This is not yet a RainerScript/YAML producer and makes no
 * frontend-parity claim.
 */
rsRetVal rsReloadNormalizedGraphBuilderV1Construct(rsReloadNormalizedGraphBuilderV1_t **ppBuilder);
rsRetVal rsReloadNormalizedGraphBuilderV1Add(rsReloadNormalizedGraphBuilderV1_t *builder,
                                             rsReloadObjectKind_t objectKind,
                                             const char *identity,
                                             const char *fingerprint);

/*
 * Produce a graph view with deterministic (kind, identity, fingerprint)
 * enumeration.  Producing seals the builder; subsequent Add calls fail.  The
 * returned view borrows the builder and becomes invalid when the builder is
 * destroyed.  Enumeration is read-only and can be called repeatedly.
 */
rsRetVal rsReloadNormalizedGraphBuilderV1GetGraph(rsReloadNormalizedGraphBuilderV1_t *builder,
                                                  rsReloadNormalizedGraphV1_t *graph);
void rsReloadNormalizedGraphBuilderV1Destruct(rsReloadNormalizedGraphBuilderV1_t **ppBuilder);

#endif /* RELOAD_NORMALIZED_GRAPH_H_INCLUDED */
