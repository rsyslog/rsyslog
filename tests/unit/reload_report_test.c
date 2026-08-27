/*
 * Unit coverage for deterministic frontend-neutral reload reports.  The
 * oracle is the ordered report produced from synthetic normalized graphs; no
 * configuration activation, daemon, or message path is involved.
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rsyslog.h"
#include "reload-report.h"

/* Keep this API unit independent of the full runtime link closure. */
#include "../../runtime/module-reload.c"
#include "../../runtime/reload-report.c"

#define CHECK(condition)                                                                    \
    do {                                                                                    \
        if (!(condition)) {                                                                 \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            exit(1);                                                                        \
        }                                                                                   \
    } while (0)

typedef struct graphFixture_s {
    const rsReloadNormalizedNodeV1_t *nodes;
    size_t count;
} graphFixture_t;

static rsRetVal enumerateFixture(void *context, rsReloadGraphVisitorV1_t visitor, void *visitorContext) {
    graphFixture_t *fixture = context;
    size_t i;

    for (i = 0; i < fixture->count; ++i) {
        const rsRetVal ret = visitor(&fixture->nodes[i], visitorContext);
        if (ret != RS_RET_OK) return ret;
    }
    return RS_RET_OK;
}

static rsReloadNormalizedGraphV1_t graphFor(const rsReloadNormalizedNodeV1_t *nodes, size_t count) {
    static graphFixture_t fixtures[24];
    static size_t next;
    graphFixture_t *fixture;
    rsReloadNormalizedGraphV1_t graph;

    CHECK(next < sizeof(fixtures) / sizeof(fixtures[0]));
    fixture = &fixtures[next++];
    fixture->nodes = nodes;
    fixture->count = count;
    graph.version = RS_RELOAD_NORMALIZED_GRAPH_V1;
    graph.structSize = sizeof(graph);
    graph.context = fixture;
    graph.enumerate = enumerateFixture;
    return graph;
}

static rsRetVal enumerateFailure(void *context, rsReloadGraphVisitorV1_t visitor, void *visitorContext) {
    (void)context;
    (void)visitor;
    (void)visitorContext;
    return RS_RET_ERR;
}

static rsRetVal classifyLiveSwap(const void *oldCnf, const void *newCnf, eModReloadCapability_t *capability) {
    (void)oldCnf;
    (void)newCnf;
    *capability = eMOD_RELOAD_LIVE_SWAP;
    return RS_RET_OK;
}

static rsReloadNormalizedNodeV1_t node(rsReloadObjectKind_t kind,
                                       const char *identity,
                                       const char *fingerprint,
                                       modInfo_t *module) {
    const rsReloadNormalizedNodeV1_t value = {
        .version = RS_RELOAD_NORMALIZED_NODE_V1,
        .structSize = sizeof(rsReloadNormalizedNodeV1_t),
        .objectKind = kind,
        .identity = identity,
        .fingerprint = fingerprint,
        .pModule = module,
    };
    return value;
}

int main(void) {
    modInfo_t capableModule;
    modInfo_t legacyModule;
    rsReloadNormalizedNodeV1_t oldNodes[3];
    rsReloadNormalizedNodeV1_t newNodes[3];
    rsReloadNormalizedNodeV1_t reordered[3];
    rsReloadNormalizedNodeV1_t duplicates[2];
    rsReloadNormalizedNodeV1_t earlier[1];
    rsReloadNormalizedNodeV1_t coreOld[1];
    rsReloadNormalizedNodeV1_t coreNew[1];
    rsReloadNormalizedNodeV1_t addedModule[1];
    rsReloadNormalizedNodeV1_t shortNode;
    rsReloadReportV1_t *savedReport;
    rsReloadReportV1_t *report = NULL;
    rsReloadReportV1_t *reorderedReport = NULL;
    rsReloadReportV1_t *edgeReport = NULL;
    rsReloadNormalizedGraphV1_t oldGraph;
    rsReloadNormalizedGraphV1_t newGraph;

    memset(&capableModule, 0, sizeof(capableModule));
    capableModule.reloadV1.version = eMOD_RELOAD_INTERFACE_V1;
    capableModule.reloadV1.structSize = sizeof(capableModule.reloadV1);
    capableModule.reloadV1.capabilityFlags = eMOD_RELOAD_CAP_VALIDATE_PRIVATE | eMOD_RELOAD_CAP_PREPARE |
                                             eMOD_RELOAD_CAP_REUSE | eMOD_RELOAD_CAP_COMMIT | eMOD_RELOAD_CAP_RETIRE;
    capableModule.reloadV1.classify = classifyLiveSwap;
    memset(&legacyModule, 0, sizeof(legacyModule));

    oldNodes[0] = node(RS_RELOAD_OBJ_RULESET, "main", "rules-v1", NULL);
    oldNodes[1] = node(RS_RELOAD_OBJ_MODULE, "omtest", "module-v1", &capableModule);
    oldNodes[2] = node(RS_RELOAD_OBJ_TEMPLATE, "legacy", "template-v1", &legacyModule);
    newNodes[0] = node(RS_RELOAD_OBJ_RULESET, "main", "rules-v1", NULL);
    newNodes[1] = node(RS_RELOAD_OBJ_MODULE, "omtest", "module-v2", &capableModule);
    newNodes[2] = node(RS_RELOAD_OBJ_TEMPLATE, "legacy", "template-v2", &legacyModule);
    oldGraph = graphFor(oldNodes, 3);
    newGraph = graphFor(newNodes, 3);
    CHECK(rsReloadReportBuildV1(&oldGraph, &newGraph, &report) == RS_RET_OK);
    CHECK(report->version == RS_RELOAD_REPORT_V1);
    CHECK(report->entryStride == sizeof(rsReloadReportEntryV1_t));
    CHECK(report->entryCount == 3);
    CHECK(report->unchangedCount == 1 && report->modifiedCount == 2);
    CHECK(report->capabilityRejectedCount == 1);
    CHECK(report->overallDisposition == RS_RELOAD_DISPOSITION_UNSUPPORTED);
    CHECK(report->entries[0].objectKind == RS_RELOAD_OBJ_MODULE);
    CHECK(report->entries[0].disposition == RS_RELOAD_DISPOSITION_NEEDS_CLASSIFICATION);
    CHECK(report->entries[0].requiredFlags == eMOD_RELOAD_CAP_VALIDATE_PRIVATE);
    CHECK(report->entries[0].reason == RS_RELOAD_REASON_CLASSIFICATION_REQUIRED);
    CHECK(report->entries[1].objectKind == RS_RELOAD_OBJ_RULESET);
    CHECK(report->entries[1].diffKind == RS_RELOAD_DIFF_UNCHANGED);
    CHECK(report->entries[2].objectKind == RS_RELOAD_OBJ_TEMPLATE);
    CHECK(report->entries[2].disposition == RS_RELOAD_DISPOSITION_UNSUPPORTED);
    CHECK(report->entries[2].reason == RS_RELOAD_REASON_NO_MODULE_CAPABILITY);

    capableModule.reloadV1.classify = NULL;
    CHECK(rsReloadReportBuildV1(&oldGraph, &newGraph, &edgeReport) == RS_RET_OK);
    CHECK(edgeReport->entries[0].disposition == RS_RELOAD_DISPOSITION_UNSUPPORTED);
    CHECK(edgeReport->entries[0].reason == RS_RELOAD_REASON_MISSING_CLASSIFIER);
    rsReloadReportDestructV1(&edgeReport);
    capableModule.reloadV1.classify = classifyLiveSwap;

    coreOld[0] = node(RS_RELOAD_OBJ_RULESET, "changed-core", "v1", NULL);
    coreNew[0] = node(RS_RELOAD_OBJ_RULESET, "changed-core", "v2", NULL);
    oldGraph = graphFor(coreOld, 1);
    newGraph = graphFor(coreNew, 1);
    CHECK(rsReloadReportBuildV1(&oldGraph, &newGraph, &edgeReport) == RS_RET_OK);
    CHECK(edgeReport->entries[0].disposition == RS_RELOAD_DISPOSITION_RESTART_REQUIRED);
    CHECK(edgeReport->entries[0].reason == RS_RELOAD_REASON_CORE_OBJECT_CHANGED);
    rsReloadReportDestructV1(&edgeReport);

    addedModule[0] = node(RS_RELOAD_OBJ_MODULE, "added-module", "v1", &capableModule);
    oldGraph = graphFor(NULL, 0);
    newGraph = graphFor(addedModule, 1);
    CHECK(rsReloadReportBuildV1(&oldGraph, &newGraph, &edgeReport) == RS_RET_OK);
    CHECK(edgeReport->entries[0].diffKind == RS_RELOAD_DIFF_ADDED);
    CHECK(edgeReport->entries[0].requiredFlags == eMOD_RELOAD_CAP_VALIDATE_PRIVATE);
    CHECK(edgeReport->entries[0].advertisedFlags == capableModule.reloadV1.capabilityFlags);
    CHECK(edgeReport->entries[0].disposition == RS_RELOAD_DISPOSITION_RESTART_REQUIRED);
    rsReloadReportDestructV1(&edgeReport);

    oldGraph = graphFor(oldNodes, 3);
    newGraph = graphFor(newNodes, 3);

    reordered[0] = newNodes[2];
    reordered[1] = newNodes[0];
    reordered[2] = newNodes[1];
    newGraph = graphFor(reordered, 3);
    CHECK(rsReloadReportBuildV1(&oldGraph, &newGraph, &reorderedReport) == RS_RET_OK);
    CHECK(reorderedReport->entryCount == report->entryCount);
    for (size_t i = 0; i < report->entryCount; ++i) {
        CHECK(reorderedReport->entries[i].objectKind == report->entries[i].objectKind);
        CHECK(reorderedReport->entries[i].diffKind == report->entries[i].diffKind);
        CHECK(!strcmp(reorderedReport->entries[i].identity, report->entries[i].identity));
        CHECK(reorderedReport->entries[i].requiredFlags == report->entries[i].requiredFlags);
        CHECK(reorderedReport->entries[i].advertisedFlags == report->entries[i].advertisedFlags);
        CHECK(reorderedReport->entries[i].disposition == report->entries[i].disposition);
        CHECK(reorderedReport->entries[i].reason == report->entries[i].reason);
    }
    CHECK(reorderedReport->unchangedCount == report->unchangedCount);
    CHECK(reorderedReport->modifiedCount == report->modifiedCount);
    CHECK(reorderedReport->capabilityRejectedCount == report->capabilityRejectedCount);
    CHECK(reorderedReport->overallDisposition == report->overallDisposition);
    rsReloadReportDestructV1(&reorderedReport);

    newNodes[2].identity = NULL;
    newGraph = graphFor(newNodes, 3);
    CHECK(rsReloadReportBuildV1(&oldGraph, &newGraph, &reorderedReport) == RS_RET_OK);
    CHECK(reorderedReport->invalidCount == 1);
    CHECK(reorderedReport->entries[2].diffKind == RS_RELOAD_DIFF_INVALID);
    rsReloadReportDestructV1(&reorderedReport);
    rsReloadReportDestructV1(&report);
    CHECK(report == NULL);

    duplicates[0] = node(RS_RELOAD_OBJ_RULESET, "duplicate", "v1", NULL);
    duplicates[1] = node(RS_RELOAD_OBJ_RULESET, "duplicate", "v2", NULL);
    oldGraph = graphFor(duplicates, 2);
    newGraph = graphFor(NULL, 0);
    CHECK(rsReloadReportBuildV1(&oldGraph, &newGraph, &report) == RS_RET_OK);
    CHECK(report->invalidCount == 2);
    CHECK(report->overallDisposition == RS_RELOAD_DISPOSITION_INVALID);
    CHECK(report->entries[0].reason == RS_RELOAD_REASON_DUPLICATE_IDENTITY);
    CHECK(report->entries[1].reason == RS_RELOAD_REASON_DUPLICATE_IDENTITY);
    rsReloadReportDestructV1(&report);

    duplicates[0] = node(RS_RELOAD_OBJ_RULESET, "z-duplicate", "v1", NULL);
    duplicates[1] = node(RS_RELOAD_OBJ_RULESET, "z-duplicate", "v2", NULL);
    earlier[0] = node(RS_RELOAD_OBJ_RULESET, "a-added", "v1", NULL);
    oldGraph = graphFor(duplicates, 2);
    newGraph = graphFor(earlier, 1);
    CHECK(rsReloadReportBuildV1(&oldGraph, &newGraph, &report) == RS_RET_OK);
    CHECK(report->entryCount == 3);
    CHECK(!strcmp(report->entries[0].identity, "a-added"));
    CHECK(report->entries[0].diffKind == RS_RELOAD_DIFF_ADDED);
    CHECK(!strcmp(report->entries[1].identity, "z-duplicate"));
    CHECK(!strcmp(report->entries[2].identity, "z-duplicate"));
    CHECK(report->capabilityRejectedCount == 0);
    rsReloadReportDestructV1(&report);

    oldGraph = graphFor(NULL, 0);
    newGraph = graphFor(NULL, 0);
    CHECK(rsReloadReportBuildV1(&oldGraph, &newGraph, &report) == RS_RET_OK);
    CHECK(report->entryCount == 0);
    CHECK(report->overallDisposition == RS_RELOAD_DISPOSITION_UNCHANGED);
    savedReport = report;
    CHECK(rsReloadReportBuildV1(&oldGraph, &newGraph, &report) == RS_RET_PARAM_ERROR);
    CHECK(report == savedReport);
    rsReloadReportDestructV1(&report);

    shortNode = node(RS_RELOAD_OBJ_RULESET, "short", "v1", NULL);
    shortNode.structSize = sizeof(shortNode.version) + sizeof(shortNode.structSize);
    oldGraph = graphFor(&shortNode, 1);
    newGraph = graphFor(NULL, 0);
    CHECK(rsReloadReportBuildV1(&oldGraph, &newGraph, &report) == RS_RET_PARAM_ERROR);
    CHECK(report == NULL);

    oldGraph = graphFor(NULL, 0);
    oldGraph.enumerate = enumerateFailure;
    CHECK(rsReloadReportBuildV1(&oldGraph, &newGraph, &report) == RS_RET_ERR);
    CHECK(report == NULL);

    puts("reload report tests passed");
    return 0;
}
