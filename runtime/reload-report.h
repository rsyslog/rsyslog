/* SPDX-License-Identifier: Apache-2.0
 * reload-report.h
 *
 * Deterministic, frontend-neutral configuration diff and capability reports.
 * This API is report-only: it does not prepare, activate, or retire any
 * configuration or runtime object.
 */
#ifndef RELOAD_REPORT_H_INCLUDED
#define RELOAD_REPORT_H_INCLUDED 1

#include <stdint.h>

#include "reload-normalized-graph.h"

/** @file reload-report.h
 * @brief Owned deterministic V1 report construction without activation.
 */

#define RS_RELOAD_REPORT_ENTRY_V1 1
#define RS_RELOAD_REPORT_V1 1

/** @brief Append-only stable diff categories. */
typedef enum eRsReloadDiffKind_ {
    RS_RELOAD_DIFF_UNCHANGED,
    RS_RELOAD_DIFF_ADDED,
    RS_RELOAD_DIFF_REMOVED,
    RS_RELOAD_DIFF_MODIFIED,
    RS_RELOAD_DIFF_INVALID
} rsReloadDiffKind_t;

/** @brief Stable dispositions; none validates or activates a candidate. */
typedef enum eRsReloadReportDisposition_ {
    RS_RELOAD_DISPOSITION_UNCHANGED,
    RS_RELOAD_DISPOSITION_NEEDS_CLASSIFICATION,
    RS_RELOAD_DISPOSITION_RESTART_REQUIRED,
    RS_RELOAD_DISPOSITION_UNSUPPORTED,
    RS_RELOAD_DISPOSITION_INVALID
} rsReloadReportDisposition_t;

/** @brief Stable explanation for a report entry's disposition. */
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

/** @brief Report-owned canonical diff row; V1 layout is frozen.
 * The identity string belongs to its enclosing report and must not be freed
 * separately. Capability flags describe compatibility, not completed hooks.
 */
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

/** @brief Owned rows and aggregate counts; V1 layout is frozen.
 * Iterate entryCount rows using entryStride bytes per row. Entries and copied
 * identities expire together when rsReloadReportDestructV1 releases the owner.
 */
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

/**
 * @brief Build an owned deterministic, report-only configuration diff.
 * Build a canonical report from two normalized configuration graphs.  The
 * graphs are read exactly through their enumerate callbacks; no candidate
 * object is changed and no module lifecycle hook runs. Report dispositions are
 * represented inside the returned report; rsRetVal is reserved for graph,
 * enumeration, and allocation failures. *ppReport must be NULL on entry; a
 * non-NULL owned report is preserved and rejected with RS_RET_PARAM_ERROR.
 * @param oldGraph Borrowed source graph, valid throughout this call.
 * @param newGraph Borrowed candidate graph, valid throughout this call.
 * @param ppReport Output owner; both the pointer and its NULL value are required.
 * @return RS_RET_OK on success; parameter, enumeration or allocation error
 * otherwise. On failure *ppReport remains unchanged. Destroy successful output
 * with rsReloadReportDestructV1; entries and identities belong to that report.
 */
rsRetVal rsReloadReportBuildV1(const rsReloadNormalizedGraphV1_t *oldGraph,
                               const rsReloadNormalizedGraphV1_t *newGraph,
                               rsReloadReportV1_t **ppReport);
/**
 * @brief Release a report and all owned entries and identity strings.
 * @param ppReport Owner returned by BuildV1; NULL and empty owners are accepted.
 * The owner is reset to NULL and borrowed entry pointers become invalid.
 */
void rsReloadReportDestructV1(rsReloadReportV1_t **ppReport);

/**
 * @brief Compute the deterministic, non-cryptographic V1 report diagnostic.
 * @param report Borrowed, well-formed report from rsReloadReportBuildV1(), or
 * an equivalent V1 view with valid entries and entryStride. No ownership moves.
 * @return FNV-1a-64 over the V1 diagnostic encoding, or zero for NULL.
 *
 * This is only an operational correlation value, not a security check,
 * configuration fingerprint, provenance record, or proof of equivalence.
 * Entries are read through entryStride, which must be at least
 * sizeof(rsReloadReportEntryV1_t). Storage must cover entryCount valid rows;
 * each non-NULL identity must be a valid NUL-terminated string. The caller owns
 * this storage and must keep the entire view valid during the call. No
 * allocation or mutation occurs. Malformed views are outside this API's contract.
 */
uint64_t rsReloadReportDiagnosticHashV1(const rsReloadReportV1_t *report);

#endif /* RELOAD_REPORT_H_INCLUDED */
