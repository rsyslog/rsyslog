/*
 * Copyright 2008-2026 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
 *
 * The rsyslog runtime library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The rsyslog runtime library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the rsyslog runtime library. If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 * A copy of the LGPL can be found in the file "COPYING.LESSER" in this distribution.
 */

/* Internal disk-assisted queue engine selection helpers. */
#ifndef INCLUDED_QUEUE_DA_H
#define INCLUDED_QUEUE_DA_H

#include "rsyslog.h"

typedef enum qda_engine_mode_e { QDA_ENGINE_AUTO = 0, QDA_ENGINE_DISK, QDA_ENGINE_SEGMENTED_DISK } qda_engine_mode_t;

typedef enum qda_selection_reason_e {
    QDA_REASON_CONFIGURED = 0,
    QDA_REASON_FRESH_DEFAULT,
    QDA_REASON_MARKER,
    QDA_REASON_CLASSIC_DATA,
    QDA_REASON_SEGMENTED_DATA,
    QDA_REASON_CLASSIC_FEATURE,
    QDA_REASON_AUTO_UPGRADE
} qda_selection_reason_t;

typedef struct qda_engine_config_s {
    const char *spool_dir;
    const char *file_prefix;
    qda_engine_mode_t requested;
    sbool auto_upgrade;
    sbool requires_classic_features;
} qda_engine_config_t;

typedef struct qda_engine_result_s {
    qda_engine_mode_t effective;
    qda_selection_reason_t reason;
    sbool classic_data;
    sbool segmented_data;
    sbool marker_present;
    qda_engine_mode_t marker_engine;
} qda_engine_result_t;

/* Fields which determine whether a disk-assisted queue can retain its
 * existing child across a configuration reload. */
typedef struct qda_lifecycle_config_s {
    qda_engine_mode_t engine;
    sbool auto_upgrade;
    int idle_timeout;
} qda_lifecycle_config_t;

rsRetVal qdaEngineResolve(const qda_engine_config_t *config, qda_engine_result_t *result);
rsRetVal qdaEngineWriteMarker(const char *spool_dir, const char *file_prefix, qda_engine_mode_t engine);
const char *qdaEngineName(qda_engine_mode_t engine);
sbool qdaLifecycleConfigEqual(const qda_lifecycle_config_t *left, const qda_lifecycle_config_t *right);

#endif
