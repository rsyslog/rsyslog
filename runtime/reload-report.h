/* reload-report.h
 *
 * Deterministic, frontend-neutral configuration diff and capability reports.
 * This API is report-only: it does not prepare, activate, or retire any
 * configuration or runtime object.
 */
#ifndef RELOAD_REPORT_H_INCLUDED
#define RELOAD_REPORT_H_INCLUDED 1

#include "reload-normalized-graph.h"

#define RS_RELOAD_REPORT_ENTRY_V1 1
#define RS_RELOAD_REPORT_V1 1

/* Append-only stable diff categories. */
typedef enum eRsReloadDiffKind_ {
    RS_RELOAD_DIFF_UNCHANGED,
    RS_RELOAD_DIFF_ADDED,
    RS_RELOAD_DIFF_REMOVED,
    RS_RELOAD_DIFF_MODIFIED,
    RS_RELOAD_DIFF_INVALID
} rsReloadDiffKind_t;

/* Append-only stable dispositions; none of them validates or activates a candidate. */
typedef enum eRsReloadReportDisposition_ {
    RS_RELOAD_DISPOSITION_UNCHANGED,
    RS_RELOAD_DISPOSITION_NEEDS_CLASSIFICATION,
    RS_RELOAD_DISPOSITION_RESTART_REQUIRED,
    RS_RELOAD_DISPOSITION_UNSUPPORTED,
    RS_RELOAD_DISPOSITION_INVALID
} rsReloadReportDisposition_t;

typedef enum eRsReloadReportReason_ {
    RS_RELOAD_REASON_NONE,
    RS_RELOAD_REASON_ADDED_OBJECT,
    RS_RELOAD_REASON_REMOVED_OBJECT,
    RS_RELOAD_REASON_MODULE_CHANGED,
    RS_RELOAD_REASON_CORE_OBJECT_CHANGED,
    RS_RELOAD_REASON_NO_MODULE_CAPABILITY,
    RS_RELOAD_REASON_MISSING_REQUIRED_FLAGS,
    RS_RELOAD_REASON_MISSING_CLASSIFIER,
    RS_RELOAD_REASON_CLASSIFICATION_REQUIRED,
    RS_RELOAD_REASON_INVALID_NODE,
    RS_RELOAD_REASON_DUPLICATE_IDENTITY
} rsReloadReportReason_t;

/* The V1 entry and report layouts are frozen; extensions require V2 types. */
typedef struct rsReloadReportEntryV1_s {
    unsigned version;
    size_t structSize;
    rsReloadObjectKind_t objectKind;
    rsReloadDiffKind_t diffKind;
    char *identity; /* report-owned canonical identity */
    unsigned requiredFlags;
    unsigned advertisedFlags;
    rsReloadReportDisposition_t disposition;
    rsReloadReportReason_t reason;
} rsReloadReportEntryV1_t;

typedef struct rsReloadReportV1_s {
    unsigned version;
    size_t structSize;
    rsReloadReportEntryV1_t *entries;
    size_t entryCount;
    size_t entryStride; /* byte stride for forward-compatible iteration */
    size_t unchangedCount;
    size_t addedCount;
    size_t removedCount;
    size_t modifiedCount;
    size_t invalidCount;
    size_t capabilityRejectedCount;
    rsReloadReportDisposition_t overallDisposition;
} rsReloadReportV1_t;

/*
 * Build a canonical report from two normalized configuration graphs.  The
 * graphs are read exactly through their enumerate callbacks; no candidate
 * object is changed and no module lifecycle hook runs. Report dispositions are
 * represented inside the returned report; rsRetVal is reserved for graph,
 * enumeration, and allocation failures. *ppReport must be NULL on entry; a
 * non-NULL owned report is preserved and rejected with RS_RET_PARAM_ERROR.
 */
rsRetVal rsReloadReportBuildV1(const rsReloadNormalizedGraphV1_t *oldGraph,
                               const rsReloadNormalizedGraphV1_t *newGraph,
                               rsReloadReportV1_t **ppReport);
void rsReloadReportDestructV1(rsReloadReportV1_t **ppReport);

#endif /* RELOAD_REPORT_H_INCLUDED */
