/* reload-report.c
 *
 * Deterministic, frontend-neutral configuration diff and capability reports.
 */
#include "config.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "rsyslog.h"
#include "reload-report.h"

typedef struct rsReloadCollectedNode_s {
    rsReloadNormalizedNodeV1_t node;
} rsReloadCollectedNode_t;

typedef struct rsReloadCollection_s {
    rsReloadCollectedNode_t *nodes;
    size_t count;
    size_t allocated;
} rsReloadCollection_t;

static void collectionDestruct(rsReloadCollection_t *collection) {
    size_t i;

    if (collection == NULL) return;
    for (i = 0; i < collection->count; ++i) {
        free((char *)collection->nodes[i].node.identity);
        free((char *)collection->nodes[i].node.fingerprint);
    }
    free(collection->nodes);
    memset(collection, 0, sizeof(*collection));
}

static rsRetVal collectNode(const rsReloadNormalizedNodeV1_t *node, void *context) {
    rsReloadCollection_t *collection = context;
    rsReloadCollectedNode_t *newNodes;

    if (node == NULL || collection == NULL) return RS_RET_PARAM_ERROR;
    if (node->version != RS_RELOAD_NORMALIZED_NODE_V1 || node->structSize < sizeof(*node)) {
        return RS_RET_PARAM_ERROR;
    }
    if (collection->count == collection->allocated) {
        const size_t newCount = collection->allocated == 0 ? 8 : collection->allocated * 2;
        if (collection->allocated > SIZE_MAX / 2 || newCount > SIZE_MAX / sizeof(*newNodes)) {
            return RS_RET_OUT_OF_MEMORY;
        }
        newNodes = realloc(collection->nodes, newCount * sizeof(*newNodes));
        if (newNodes == NULL) return RS_RET_OUT_OF_MEMORY;
        collection->nodes = newNodes;
        collection->allocated = newCount;
    }
    collection->nodes[collection->count].node = *node;
    if (node->identity != NULL) {
        collection->nodes[collection->count].node.identity = strdup(node->identity);
        if (collection->nodes[collection->count].node.identity == NULL) return RS_RET_OUT_OF_MEMORY;
    }
    if (node->fingerprint != NULL) {
        collection->nodes[collection->count].node.fingerprint = strdup(node->fingerprint);
        if (collection->nodes[collection->count].node.fingerprint == NULL) {
            free((char *)collection->nodes[collection->count].node.identity);
            collection->nodes[collection->count].node.identity = NULL;
            return RS_RET_OUT_OF_MEMORY;
        }
    }
    ++collection->count;
    return RS_RET_OK;
}

static int compareNodes(const void *left, const void *right) {
    const rsReloadCollectedNode_t *a = left;
    const rsReloadCollectedNode_t *b = right;

    if (a->node.objectKind != b->node.objectKind) return a->node.objectKind < b->node.objectKind ? -1 : 1;
    if (a->node.identity == NULL) return b->node.identity == NULL ? 0 : -1;
    if (b->node.identity == NULL) return 1;
    return strcmp(a->node.identity, b->node.identity);
}

static rsRetVal collectGraph(const rsReloadNormalizedGraphV1_t *graph, rsReloadCollection_t *collection) {
    DEFiRet;

    if (graph == NULL || collection == NULL || graph->version != RS_RELOAD_NORMALIZED_GRAPH_V1 ||
        graph->structSize < sizeof(*graph) || graph->enumerate == NULL) {
        return RS_RET_PARAM_ERROR;
    }
    CHKiRet(graph->enumerate(graph->context, collectNode, collection));
    qsort(collection->nodes, collection->count, sizeof(*collection->nodes), compareNodes);
finalize_it:
    return iRet;
}

static sbool nodeIsValid(const rsReloadNormalizedNodeV1_t *node) {
    return node != NULL && node->version == RS_RELOAD_NORMALIZED_NODE_V1 && node->structSize >= sizeof(*node) &&
           node->objectKind >= RS_RELOAD_OBJ_GLOBAL && node->objectKind < RS_RELOAD_OBJ_COUNT &&
           node->identity != NULL && *node->identity != '\0' && node->fingerprint != NULL;
}

static int dispositionRank(rsReloadReportDisposition_t disposition) {
    switch (disposition) {
        case RS_RELOAD_DISPOSITION_UNCHANGED:
            return 0;
        case RS_RELOAD_DISPOSITION_NEEDS_CLASSIFICATION:
            return 1;
        case RS_RELOAD_DISPOSITION_RESTART_REQUIRED:
            return 2;
        case RS_RELOAD_DISPOSITION_UNSUPPORTED:
            return 3;
        case RS_RELOAD_DISPOSITION_INVALID:
            return 4;
        default:
            return 4;
    }
}

static rsRetVal appendEntry(rsReloadReportV1_t *report,
                            rsReloadObjectKind_t kind,
                            rsReloadDiffKind_t diff,
                            const char *identity,
                            unsigned requiredFlags,
                            unsigned advertisedFlags,
                            rsReloadReportDisposition_t disposition,
                            rsReloadReportReason_t reason) {
    rsReloadReportEntryV1_t *entries;
    rsReloadReportEntryV1_t *entry;

    if (report->entryCount == SIZE_MAX / sizeof(*entries)) return RS_RET_OUT_OF_MEMORY;
    entries = realloc(report->entries, (report->entryCount + 1) * sizeof(*entries));
    if (entries == NULL) return RS_RET_OUT_OF_MEMORY;
    report->entries = entries;
    entry = &report->entries[report->entryCount++];
    memset(entry, 0, sizeof(*entry));
    entry->version = RS_RELOAD_REPORT_ENTRY_V1;
    entry->structSize = sizeof(*entry);
    entry->objectKind = kind;
    entry->diffKind = diff;
    entry->requiredFlags = requiredFlags;
    entry->advertisedFlags = advertisedFlags;
    entry->disposition = disposition;
    entry->reason = reason;
    if (identity != NULL) {
        entry->identity = strdup(identity);
        if (entry->identity == NULL) return RS_RET_OUT_OF_MEMORY;
    }
    switch (diff) {
        case RS_RELOAD_DIFF_UNCHANGED:
            ++report->unchangedCount;
            break;
        case RS_RELOAD_DIFF_ADDED:
            ++report->addedCount;
            break;
        case RS_RELOAD_DIFF_REMOVED:
            ++report->removedCount;
            break;
        case RS_RELOAD_DIFF_MODIFIED:
            ++report->modifiedCount;
            break;
        case RS_RELOAD_DIFF_INVALID:
            ++report->invalidCount;
            break;
        default:
            break;
    }
    if (reason == RS_RELOAD_REASON_NO_MODULE_CAPABILITY || reason == RS_RELOAD_REASON_MISSING_REQUIRED_FLAGS ||
        reason == RS_RELOAD_REASON_MISSING_CLASSIFIER) {
        ++report->capabilityRejectedCount;
    }
    if (dispositionRank(disposition) > dispositionRank(report->overallDisposition)) {
        report->overallDisposition = disposition;
    }
    return RS_RET_OK;
}

static rsRetVal appendInvalidNode(rsReloadReportV1_t *report,
                                  const rsReloadNormalizedNodeV1_t *node,
                                  rsReloadReportReason_t reason) {
    return appendEntry(report, node == NULL ? RS_RELOAD_OBJ_COUNT : node->objectKind, RS_RELOAD_DIFF_INVALID,
                       node == NULL ? NULL : node->identity, 0, 0, RS_RELOAD_DISPOSITION_INVALID, reason);
}

static size_t duplicateRunLength(const rsReloadCollection_t *collection, size_t index) {
    size_t end = index + 1;

    while (end < collection->count && compareNodes(&collection->nodes[index], &collection->nodes[end]) == 0) ++end;
    return end - index;
}

static rsRetVal appendDuplicateRun(rsReloadReportV1_t *report,
                                   const rsReloadCollection_t *collection,
                                   size_t index,
                                   size_t runLength) {
    size_t i;
    DEFiRet;

    for (i = 0; i < runLength; ++i) {
        CHKiRet(appendInvalidNode(report, &collection->nodes[index + i].node, RS_RELOAD_REASON_DUPLICATE_IDENTITY));
    }
finalize_it:
    return iRet;
}

static unsigned requiredValidationFlags(const rsReloadNormalizedNodeV1_t *node) {
    return node != NULL && node->pModule != NULL ? eMOD_RELOAD_CAP_VALIDATE_PRIVATE : 0;
}

static unsigned advertisedReloadFlags(const rsReloadNormalizedNodeV1_t *node) {
    return node != NULL && modReloadHasValidInterfaceV1(node->pModule) ? node->pModule->reloadV1.capabilityFlags : 0;
}

static rsRetVal buildReport(rsReloadReportV1_t *report,
                            const rsReloadCollection_t *oldCollection,
                            const rsReloadCollection_t *newCollection) {
    size_t oldIndex = 0;
    size_t newIndex = 0;
    DEFiRet;

    while (oldIndex < oldCollection->count || newIndex < newCollection->count) {
        const rsReloadNormalizedNodeV1_t *oldNode =
            oldIndex < oldCollection->count ? &oldCollection->nodes[oldIndex].node : NULL;
        const rsReloadNormalizedNodeV1_t *newNode =
            newIndex < newCollection->count ? &newCollection->nodes[newIndex].node : NULL;
        const size_t oldRun = oldNode != NULL && nodeIsValid(oldNode) ? duplicateRunLength(oldCollection, oldIndex) : 1;
        const size_t newRun = newNode != NULL && nodeIsValid(newNode) ? duplicateRunLength(newCollection, newIndex) : 1;
        int comparison = oldNode == NULL ? 1
                         : newNode == NULL
                             ? -1
                             : compareNodes(&oldCollection->nodes[oldIndex], &newCollection->nodes[newIndex]);

        if (comparison <= 0 && oldNode != NULL && !nodeIsValid(oldNode)) {
            CHKiRet(appendInvalidNode(report, oldNode, RS_RELOAD_REASON_INVALID_NODE));
            ++oldIndex;
            continue;
        }
        if (comparison >= 0 && newNode != NULL && !nodeIsValid(newNode)) {
            CHKiRet(appendInvalidNode(report, newNode, RS_RELOAD_REASON_INVALID_NODE));
            ++newIndex;
            continue;
        }
        if (comparison == 0 && (oldRun > 1 || newRun > 1)) {
            CHKiRet(appendDuplicateRun(report, oldCollection, oldIndex, oldRun));
            CHKiRet(appendDuplicateRun(report, newCollection, newIndex, newRun));
            oldIndex += oldRun;
            newIndex += newRun;
            continue;
        }
        if (comparison < 0 && oldRun > 1) {
            CHKiRet(appendDuplicateRun(report, oldCollection, oldIndex, oldRun));
            oldIndex += oldRun;
            continue;
        }
        if (comparison > 0 && newRun > 1) {
            CHKiRet(appendDuplicateRun(report, newCollection, newIndex, newRun));
            newIndex += newRun;
            continue;
        }
        if (oldNode == NULL) {
            if (newNode == NULL) ABORT_FINALIZE(RS_RET_PARAM_ERROR);
            CHKiRet(appendEntry(report, newNode->objectKind, RS_RELOAD_DIFF_ADDED, newNode->identity,
                                requiredValidationFlags(newNode), advertisedReloadFlags(newNode),
                                RS_RELOAD_DISPOSITION_RESTART_REQUIRED, RS_RELOAD_REASON_ADDED_OBJECT));
            ++newIndex;
            continue;
        }
        if (newNode == NULL) {
            CHKiRet(appendEntry(report, oldNode->objectKind, RS_RELOAD_DIFF_REMOVED, oldNode->identity,
                                requiredValidationFlags(oldNode), advertisedReloadFlags(oldNode),
                                RS_RELOAD_DISPOSITION_RESTART_REQUIRED, RS_RELOAD_REASON_REMOVED_OBJECT));
            ++oldIndex;
            continue;
        }
        if (comparison < 0) {
            CHKiRet(appendEntry(report, oldNode->objectKind, RS_RELOAD_DIFF_REMOVED, oldNode->identity,
                                requiredValidationFlags(oldNode), advertisedReloadFlags(oldNode),
                                RS_RELOAD_DISPOSITION_RESTART_REQUIRED, RS_RELOAD_REASON_REMOVED_OBJECT));
            ++oldIndex;
        } else if (comparison > 0) {
            CHKiRet(appendEntry(report, newNode->objectKind, RS_RELOAD_DIFF_ADDED, newNode->identity,
                                requiredValidationFlags(newNode), advertisedReloadFlags(newNode),
                                RS_RELOAD_DISPOSITION_RESTART_REQUIRED, RS_RELOAD_REASON_ADDED_OBJECT));
            ++newIndex;
        } else if (!strcmp(oldNode->fingerprint, newNode->fingerprint)) {
            CHKiRet(appendEntry(report, oldNode->objectKind, RS_RELOAD_DIFF_UNCHANGED, oldNode->identity, 0, 0,
                                RS_RELOAD_DISPOSITION_UNCHANGED, RS_RELOAD_REASON_NONE));
            ++oldIndex;
            ++newIndex;
        } else {
            const modInfo_t *module = oldNode->pModule == newNode->pModule ? oldNode->pModule : NULL;
            const unsigned requiredFlags = module == NULL ? 0 : requiredValidationFlags(oldNode);
            const unsigned advertisedFlags = module == NULL ? 0 : advertisedReloadFlags(oldNode);
            const rsReloadReportDisposition_t disposition =
                requiredFlags == 0 ? RS_RELOAD_DISPOSITION_RESTART_REQUIRED
                : !modReloadHasValidInterfaceV1(module) || module->reloadV1.classify == NULL
                    ? RS_RELOAD_DISPOSITION_UNSUPPORTED
                : (advertisedFlags & requiredFlags) == requiredFlags ? RS_RELOAD_DISPOSITION_NEEDS_CLASSIFICATION
                                                                     : RS_RELOAD_DISPOSITION_UNSUPPORTED;
            const rsReloadReportReason_t reason =
                disposition == RS_RELOAD_DISPOSITION_NEEDS_CLASSIFICATION ? RS_RELOAD_REASON_CLASSIFICATION_REQUIRED
                : oldNode->pModule == NULL && newNode->pModule == NULL    ? RS_RELOAD_REASON_CORE_OBJECT_CHANGED
                : oldNode->pModule != newNode->pModule                    ? RS_RELOAD_REASON_MODULE_CHANGED
                : advertisedFlags == 0                                    ? RS_RELOAD_REASON_NO_MODULE_CAPABILITY
                : module->reloadV1.classify == NULL                       ? RS_RELOAD_REASON_MISSING_CLASSIFIER
                                                                          : RS_RELOAD_REASON_MISSING_REQUIRED_FLAGS;
            CHKiRet(appendEntry(report, oldNode->objectKind, RS_RELOAD_DIFF_MODIFIED, oldNode->identity, requiredFlags,
                                advertisedFlags, disposition, reason));
            ++oldIndex;
            ++newIndex;
        }
    }
finalize_it:
    return iRet;
}

rsRetVal rsReloadReportBuildV1(const rsReloadNormalizedGraphV1_t *oldGraph,
                               const rsReloadNormalizedGraphV1_t *newGraph,
                               rsReloadReportV1_t **ppReport) {
    rsReloadCollection_t oldCollection = {0};
    rsReloadCollection_t newCollection = {0};
    rsReloadReportV1_t *report = NULL;
    DEFiRet;

    if (ppReport == NULL || *ppReport != NULL) ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    CHKiRet(collectGraph(oldGraph, &oldCollection));
    CHKiRet(collectGraph(newGraph, &newCollection));
    CHKmalloc(report = calloc(1, sizeof(*report)));
    report->version = RS_RELOAD_REPORT_V1;
    report->structSize = sizeof(*report);
    report->entryStride = sizeof(*report->entries);
    CHKiRet(buildReport(report, &oldCollection, &newCollection));
    *ppReport = report;
    report = NULL;
finalize_it:
    collectionDestruct(&oldCollection);
    collectionDestruct(&newCollection);
    rsReloadReportDestructV1(&report);
    return iRet;
}

void rsReloadReportDestructV1(rsReloadReportV1_t **ppReport) {
    rsReloadReportV1_t *report;
    size_t i;

    if (ppReport == NULL || *ppReport == NULL) return;
    report = *ppReport;
    for (i = 0; i < report->entryCount; ++i) free(report->entries[i].identity);
    free(report->entries);
    free(report);
    *ppReport = NULL;
}
