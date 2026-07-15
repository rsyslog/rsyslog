/* SPDX-License-Identifier: Apache-2.0 */

/*
 * Copyright 2026 Rainer Gerhards and Adiscon GmbH.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 */

/**
 * @file queue_da.h
 * @brief Internal API for disk-assisted queue engine selection and markers.
 *
 * These declarations encapsulate automatic classic-versus-segmented selection,
 * the persistent engine marker, and reload-lifecycle comparison for
 * disk-assisted child queues.
 */
#ifndef INCLUDED_QUEUE_DA_H
#define INCLUDED_QUEUE_DA_H

#include "rsyslog.h"

/**
 * @brief Configurable disk-store engine choices for a disk-assisted child.
 *
 * AUTO is a policy, not a persistent engine: qdaEngineResolve() maps it to
 * either DISK or SEGMENTED_DISK using existing spool data, the durable marker,
 * and features unsupported by the segmented store. Only the two concrete
 * values may be persisted in a marker.
 */
typedef enum qda_engine_mode_e {
    QDA_ENGINE_AUTO = 0, /**< Select a compatible concrete engine at startup. */
    QDA_ENGINE_DISK, /**< Use the established .qi and numeric-segment store. */
    QDA_ENGINE_SEGMENTED_DISK /**< Use the segmented .segq store. */
} qda_engine_mode_t;

/**
 * @brief Evidence that determined the selected effective engine.
 *
 * The reason is retained for diagnostics and warning policy. It records the
 * decisive input, not every probe performed while resolving the engine.
 */
typedef enum qda_selection_reason_e {
    QDA_REASON_CONFIGURED = 0, /**< An explicit concrete selector chose it. */
    QDA_REASON_FRESH_DEFAULT, /**< No prior store exists; choose segmented. */
    QDA_REASON_MARKER, /**< A retained marker keeps the empty queue sticky. */
    QDA_REASON_CLASSIC_DATA, /**< Classic state or numeric segments exist. */
    QDA_REASON_SEGMENTED_DATA, /**< Segmented state directory exists. */
    QDA_REASON_CLASSIC_FEATURE, /**< Requested options require the classic store. */
    QDA_REASON_AUTO_UPGRADE /**< A clean classic marker permits a restart upgrade. */
} qda_selection_reason_t;

/**
 * @brief Inputs used to choose a disk-assisted child engine.
 *
 * spool_dir and file_prefix identify the parent queue's persistent namespace.
 * The resolver examines only this namespace; it never creates, modifies, or
 * migrates a store. requires_classic_features is true for settings that the
 * segmented store intentionally does not implement (currently encryption or
 * an unsafe corruption policy).
 */
typedef struct qda_engine_config_s {
    const char *spool_dir; /**< Existing workDirectory; not owned by this object. */
    const char *file_prefix; /**< Queue filename prefix within spool_dir; not owned. */
    qda_engine_mode_t requested; /**< AUTO or an explicit concrete engine. */
    sbool auto_upgrade; /**< Permit a drained classic AUTO queue to switch at restart. */
    sbool requires_classic_features; /**< Force classic in AUTO; reject explicit segmented. */
} qda_engine_config_t;

/**
 * @brief Result and probe observations returned by qdaEngineResolve().
 *
 * The data-presence fields deliberately distinguish compatibility evidence
 * from the chosen engine. Callers use them to decide when a marker must be
 * replaced before a child is materialized. The structure is initialized by
 * qdaEngineResolve() and is meaningful only when that function succeeds.
 */
typedef struct qda_engine_result_s {
    qda_engine_mode_t effective; /**< Concrete engine to instantiate. */
    qda_selection_reason_t reason; /**< Decisive selection evidence. */
    sbool classic_data; /**< .qi or numeric classic segments were found. */
    sbool segmented_data; /**< .segq directory was found. */
    sbool marker_present; /**< A valid .da-engine marker was read. */
    qda_engine_mode_t marker_engine; /**< Concrete engine recorded in that marker. */
} qda_engine_result_t;

/**
 * @brief Configuration that controls a disk-assisted child's identity.
 *
 * Changing any field requires rebuilding the child on reload. These values
 * are deliberately separate from generic queue settings: they determine
 * persistent-store compatibility and idle dematerialization semantics.
 */
typedef struct qda_lifecycle_config_s {
    qda_engine_mode_t engine; /**< Configured selector, including AUTO. */
    sbool auto_upgrade; /**< AUTO-only clean-drain upgrade policy. */
    int idle_timeout; /**< Segmented child idle lifetime in milliseconds. */
} qda_lifecycle_config_t;

/**
 * @brief Resolve the concrete engine for a disk-assisted child without writing.
 *
 * Precedence is intentionally data-first: mixed stores and malformed markers
 * fail, existing classic or segmented data keeps its compatible engine, then
 * unsupported segmented features force classic in AUTO mode. A valid marker
 * makes an otherwise empty queue sticky, except when AUTO upgrade is enabled
 * and the old classic store has demonstrably drained. A fresh AUTO queue
 * resolves to segmentedDisk.
 *
 * @param config Non-NULL selection inputs and spool namespace.
 * @param result Non-NULL output, zeroed and populated by this call.
 * @return RS_RET_OK on a concrete selection; RS_RET_PARAM_ERROR for missing
 *         required parameters; RS_RET_INVALID_VALUE for mixed, malformed, or
 *         explicitly incompatible state; another error for I/O or allocation
 *         failure. The function never creates or changes files.
 */
rsRetVal qdaEngineResolve(const qda_engine_config_t *config, qda_engine_result_t *result);

/**
 * @brief Atomically publish the concrete engine marker for a queue namespace.
 *
 * The marker is a small versioned compatibility record, not queue data or
 * metadata. The helper writes a private temporary file, synchronizes it,
 * renames it over the marker, and synchronizes the containing directory. This
 * makes an engine selection durable before its first persistent records exist
 * and allows an empty dematerialized segmented child to remain sticky.
 *
 * @param spool_dir Existing parent workDirectory.
 * @param file_prefix Parent queue filename prefix.
 * @param engine Concrete DISK or SEGMENTED_DISK engine to publish; AUTO is invalid.
 * @return RS_RET_OK or an allocation, parameter, or I/O error.
 */
rsRetVal qdaEngineWriteMarker(const char *spool_dir, const char *file_prefix, qda_engine_mode_t engine);

/**
 * @brief Return the configuration/marker spelling for an engine value.
 *
 * The returned static string is never NULL and must not be freed. Invalid
 * values return "invalid" for diagnostics; callers must still validate values
 * before attempting to persist them.
 */
const char *qdaEngineName(qda_engine_mode_t engine);

/**
 * @brief Compare all settings that require DA child replacement on reload.
 *
 * NULL inputs are unequal. This function compares values only; it does not
 * inspect the spool or determine whether a replacement can safely occur.
 */
sbool qdaLifecycleConfigEqual(const qda_lifecycle_config_t *left, const qda_lifecycle_config_t *right);

#endif
