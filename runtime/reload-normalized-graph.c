/* reload-normalized-graph.c
 *
 * Owned normalized graph builder used by the report-only reload foundation.
 */
#include "config.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "rsyslog.h"
#include "reload-normalized-graph.h"

struct rsReloadNormalizedGraphBuilderV1_s {
    unsigned version;
    size_t structSize;
    rsReloadNormalizedNodeV1_t *nodes;
    size_t count;
    size_t allocated;
    sbool sealed;
};

static void nodeDestruct(rsReloadNormalizedNodeV1_t *node) {
    if (node == NULL) return;
    free((char *)node->identity);
    free((char *)node->fingerprint);
    memset(node, 0, sizeof(*node));
}

static int compareNormalizedNodes(const void *left, const void *right) {
    const rsReloadNormalizedNodeV1_t *a = left;
    const rsReloadNormalizedNodeV1_t *b = right;
    int result;

    if (a->objectKind != b->objectKind) return a->objectKind < b->objectKind ? -1 : 1;
    result = strcmp(a->identity, b->identity);
    if (result != 0) return result;
    return strcmp(a->fingerprint, b->fingerprint);
}

static rsRetVal enumerateBuilder(void *context, rsReloadGraphVisitorV1_t visitor, void *visitorContext) {
    const rsReloadNormalizedGraphBuilderV1_t *builder = context;
    size_t i;

    if (builder == NULL || visitor == NULL || builder->version != RS_RELOAD_NORMALIZED_GRAPH_BUILDER_V1 ||
        builder->structSize != sizeof(*builder)) {
        return RS_RET_PARAM_ERROR;
    }
    for (i = 0; i < builder->count; ++i) {
        const rsRetVal ret = visitor(&builder->nodes[i], visitorContext);
        if (ret != RS_RET_OK) return ret;
    }
    return RS_RET_OK;
}

rsRetVal rsReloadNormalizedGraphBuilderV1Construct(rsReloadNormalizedGraphBuilderV1_t **ppBuilder) {
    rsReloadNormalizedGraphBuilderV1_t *builder;

    if (ppBuilder == NULL || *ppBuilder != NULL) return RS_RET_PARAM_ERROR;
    builder = calloc(1, sizeof(*builder));
    if (builder == NULL) return RS_RET_OUT_OF_MEMORY;
    builder->version = RS_RELOAD_NORMALIZED_GRAPH_BUILDER_V1;
    builder->structSize = sizeof(*builder);
    *ppBuilder = builder;
    return RS_RET_OK;
}

rsRetVal rsReloadNormalizedGraphBuilderV1Add(rsReloadNormalizedGraphBuilderV1_t *builder,
                                             rsReloadObjectKind_t objectKind,
                                             const char *identity,
                                             const char *fingerprint) {
    rsReloadNormalizedNodeV1_t *nodes;
    char *identityCopy;
    char *fingerprintCopy;

    if (builder == NULL || builder->version != RS_RELOAD_NORMALIZED_GRAPH_BUILDER_V1 ||
        builder->structSize != sizeof(*builder) || objectKind < RS_RELOAD_OBJ_GLOBAL ||
        objectKind >= RS_RELOAD_OBJ_COUNT || identity == NULL || *identity == '\0' || fingerprint == NULL ||
        builder->sealed) {
        return RS_RET_PARAM_ERROR;
    }
    identityCopy = strdup(identity);
    if (identityCopy == NULL) return RS_RET_OUT_OF_MEMORY;
    fingerprintCopy = strdup(fingerprint);
    if (fingerprintCopy == NULL) {
        free(identityCopy);
        return RS_RET_OUT_OF_MEMORY;
    }
    if (builder->count == builder->allocated) {
        const size_t newCount = builder->allocated == 0 ? 8 : builder->allocated * 2;

        if (builder->allocated > SIZE_MAX / 2 || newCount > SIZE_MAX / sizeof(*nodes)) {
            free(identityCopy);
            free(fingerprintCopy);
            return RS_RET_OUT_OF_MEMORY;
        }
        nodes = realloc(builder->nodes, newCount * sizeof(*nodes));
        if (nodes == NULL) {
            free(identityCopy);
            free(fingerprintCopy);
            return RS_RET_OUT_OF_MEMORY;
        }
        builder->nodes = nodes;
        builder->allocated = newCount;
    }
    builder->nodes[builder->count] = (rsReloadNormalizedNodeV1_t){
        .version = RS_RELOAD_NORMALIZED_NODE_V1,
        .structSize = sizeof(*builder->nodes),
        .objectKind = objectKind,
        .identity = identityCopy,
        .fingerprint = fingerprintCopy,
        .pModule = NULL,
        .pModuleCnf = NULL,
    };
    ++builder->count;
    return RS_RET_OK;
}

rsRetVal rsReloadNormalizedGraphBuilderV1GetGraph(rsReloadNormalizedGraphBuilderV1_t *builder,
                                                  rsReloadNormalizedGraphV1_t *graph) {
    if (builder == NULL || graph == NULL || builder->version != RS_RELOAD_NORMALIZED_GRAPH_BUILDER_V1 ||
        builder->structSize != sizeof(*builder)) {
        return RS_RET_PARAM_ERROR;
    }
    if (!builder->sealed) {
        if (builder->count > 1) qsort(builder->nodes, builder->count, sizeof(*builder->nodes), compareNormalizedNodes);
        builder->sealed = 1;
    }
    *graph = (rsReloadNormalizedGraphV1_t){
        .version = RS_RELOAD_NORMALIZED_GRAPH_V1,
        .structSize = sizeof(*graph),
        .context = builder,
        .enumerate = enumerateBuilder,
    };
    return RS_RET_OK;
}

void rsReloadNormalizedGraphBuilderV1Destruct(rsReloadNormalizedGraphBuilderV1_t **ppBuilder) {
    rsReloadNormalizedGraphBuilderV1_t *builder;
    size_t i;

    if (ppBuilder == NULL || *ppBuilder == NULL) return;
    builder = *ppBuilder;
    for (i = 0; i < builder->count; ++i) nodeDestruct(&builder->nodes[i]);
    free(builder->nodes);
    free(builder);
    *ppBuilder = NULL;
}
