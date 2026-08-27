/*
 * Unit coverage for the owned normalized-graph builder.  The oracle verifies
 * that copied identity/fingerprint input survives caller mutation and that the
 * graph view enumerates deterministically; no parser, activation, or daemon
 * lifecycle is involved.
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rsyslog.h"
#include "reload-normalized-graph.h"
#include "reload-report.h"

/* Keep this API unit independent of the full runtime link closure. */
#include "../../runtime/module-reload.c"
#include "../../runtime/reload-normalized-graph.c"
#include "../../runtime/reload-report.c"

#define CHECK(condition)                                                                    \
    do {                                                                                    \
        if (!(condition)) {                                                                 \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            exit(1);                                                                        \
        }                                                                                   \
    } while (0)

typedef struct observedNodes_s {
    const rsReloadNormalizedNodeV1_t *nodes[4];
    size_t count;
} observedNodes_t;

static rsRetVal observeNode(const rsReloadNormalizedNodeV1_t *node, void *context) {
    observedNodes_t *observed = context;

    CHECK(observed->count < sizeof(observed->nodes) / sizeof(observed->nodes[0]));
    observed->nodes[observed->count++] = node;
    return RS_RET_OK;
}

static rsRetVal failVisitor(const rsReloadNormalizedNodeV1_t *node, void *context) {
    (void)node;
    (void)context;
    return RS_RET_ERR;
}

int main(void) {
    rsReloadNormalizedGraphBuilderV1_t *oldBuilder = NULL;
    rsReloadNormalizedGraphBuilderV1_t *newBuilder = NULL;
    rsReloadNormalizedGraphV1_t oldGraph;
    rsReloadNormalizedGraphV1_t newGraph;
    rsReloadReportV1_t *report = NULL;
    observedNodes_t observed;
    char identity[16] = "z-rule";
    char fingerprint[16] = "fp-z";

    memset(&observed, 0, sizeof(observed));
    CHECK(rsReloadNormalizedGraphBuilderV1Construct(&oldBuilder) == RS_RET_OK);
    CHECK(rsReloadNormalizedGraphBuilderV1Construct(&oldBuilder) == RS_RET_PARAM_ERROR);
    CHECK(rsReloadNormalizedGraphBuilderV1Construct(&newBuilder) == RS_RET_OK);
    CHECK(rsReloadNormalizedGraphBuilderV1Add(oldBuilder, RS_RELOAD_OBJ_RULESET, identity, fingerprint) == RS_RET_OK);
    strcpy(identity, "changed");
    strcpy(fingerprint, "changed");
    CHECK(rsReloadNormalizedGraphBuilderV1Add(oldBuilder, RS_RELOAD_OBJ_ACTION, "a-action", "fp-a") == RS_RET_OK);
    CHECK(rsReloadNormalizedGraphBuilderV1Add(oldBuilder, RS_RELOAD_OBJ_RULESET, "a-rule", "fp-b") == RS_RET_OK);
    CHECK(rsReloadNormalizedGraphBuilderV1Add(oldBuilder, RS_RELOAD_OBJ_COUNT, "bad", "bad") == RS_RET_PARAM_ERROR);
    CHECK(rsReloadNormalizedGraphBuilderV1Add(oldBuilder, RS_RELOAD_OBJ_ACTION, "", "bad") == RS_RET_PARAM_ERROR);
    CHECK(rsReloadNormalizedGraphBuilderV1Add(oldBuilder, RS_RELOAD_OBJ_ACTION, "bad", NULL) == RS_RET_PARAM_ERROR);
    CHECK(rsReloadNormalizedGraphBuilderV1Add(newBuilder, RS_RELOAD_OBJ_ACTION, "a-action", "fp-a") == RS_RET_OK);
    CHECK(rsReloadNormalizedGraphBuilderV1Add(newBuilder, RS_RELOAD_OBJ_RULESET, "a-rule", "fp-new") == RS_RET_OK);
    CHECK(rsReloadNormalizedGraphBuilderV1Add(newBuilder, RS_RELOAD_OBJ_RULESET, "z-rule", "fp-z") == RS_RET_OK);
    CHECK(rsReloadNormalizedGraphBuilderV1GetGraph(oldBuilder, &oldGraph) == RS_RET_OK);
    CHECK(oldGraph.enumerate(oldGraph.context, observeNode, &observed) == RS_RET_OK);
    CHECK(observed.count == 3);
    CHECK(observed.nodes[0]->objectKind == RS_RELOAD_OBJ_RULESET);
    CHECK(!strcmp(observed.nodes[0]->identity, "a-rule"));
    CHECK(!strcmp(observed.nodes[1]->identity, "z-rule"));
    CHECK(!strcmp(observed.nodes[1]->fingerprint, "fp-z"));
    CHECK(observed.nodes[2]->objectKind == RS_RELOAD_OBJ_ACTION);
    CHECK(oldGraph.enumerate(oldGraph.context, NULL, NULL) == RS_RET_PARAM_ERROR);
    CHECK(oldGraph.enumerate(oldGraph.context, failVisitor, NULL) == RS_RET_ERR);
    CHECK(rsReloadNormalizedGraphBuilderV1Add(oldBuilder, RS_RELOAD_OBJ_ACTION, "after-view", "fp") ==
          RS_RET_PARAM_ERROR);
    CHECK(rsReloadNormalizedGraphBuilderV1GetGraph(newBuilder, &newGraph) == RS_RET_OK);
    CHECK(rsReloadReportBuildV1(&oldGraph, &newGraph, &report) == RS_RET_OK);
    rsReloadNormalizedGraphBuilderV1Destruct(&newBuilder);
    rsReloadNormalizedGraphBuilderV1Destruct(&oldBuilder);
    CHECK(newBuilder == NULL && oldBuilder == NULL);
    CHECK(report->entryCount == 3);
    CHECK(report->unchangedCount == 2);
    CHECK(report->modifiedCount == 1);
    CHECK(report->entries[0].objectKind == RS_RELOAD_OBJ_RULESET);
    CHECK(!strcmp(report->entries[0].identity, "a-rule"));
    CHECK(report->entries[0].diffKind == RS_RELOAD_DIFF_MODIFIED);
    CHECK(report->entries[1].diffKind == RS_RELOAD_DIFF_UNCHANGED);
    CHECK(report->entries[2].objectKind == RS_RELOAD_OBJ_ACTION);
    CHECK(report->entries[2].diffKind == RS_RELOAD_DIFF_UNCHANGED);
    rsReloadReportDestructV1(&report);

    CHECK(rsReloadNormalizedGraphBuilderV1Construct(&oldBuilder) == RS_RET_OK);
    CHECK(rsReloadNormalizedGraphBuilderV1Add(oldBuilder, RS_RELOAD_OBJ_RULESET, "duplicate", "one") == RS_RET_OK);
    CHECK(rsReloadNormalizedGraphBuilderV1Add(oldBuilder, RS_RELOAD_OBJ_RULESET, "duplicate", "two") == RS_RET_OK);
    CHECK(rsReloadNormalizedGraphBuilderV1GetGraph(oldBuilder, &oldGraph) == RS_RET_OK);
    CHECK(rsReloadNormalizedGraphBuilderV1Construct(&newBuilder) == RS_RET_OK);
    CHECK(rsReloadNormalizedGraphBuilderV1GetGraph(newBuilder, &newGraph) == RS_RET_OK);
    CHECK(rsReloadReportBuildV1(&oldGraph, &newGraph, &report) == RS_RET_OK);
    CHECK(report->invalidCount == 2);
    CHECK(report->entries[0].reason == RS_RELOAD_REASON_DUPLICATE_IDENTITY);
    rsReloadReportDestructV1(&report);
    rsReloadNormalizedGraphBuilderV1Destruct(&newBuilder);
    rsReloadNormalizedGraphBuilderV1Destruct(&oldBuilder);
    puts("reload normalized graph tests passed");
    return 0;
}
