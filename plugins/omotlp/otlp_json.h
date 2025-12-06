/**
 * @file otlp_json.h
 * @brief OTLP JSON payload builder interface
 *
 * This header defines the API for building OpenTelemetry Protocol (OTLP)
 * JSON payloads from rsyslog log records. It handles the conversion of
 * rsyslog message properties to OTLP log record format, including resource
 * attributes, scope information, and log record attributes.
 *
 * Copyright 2025 Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *       -or-
 *       see COPYING.ASL20 in the source distribution
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef OMOTLP_OTLP_JSON_H
#define OMOTLP_OTLP_JSON_H

#include <stddef.h>
#include <stdint.h>

#include "rsyslog.h"
#include <json.h>

/**
 * @brief OTLP log record structure
 *
 * Contains all fields for a single OpenTelemetry log record. String fields
 * are pointers to external memory and are not freed by this module.
 */
typedef struct omotlp_log_record_s {
    uint64_t time_unix_nano; /**< Log timestamp in Unix nanoseconds */
    uint64_t observed_time_unix_nano; /**< Observation timestamp in Unix nanoseconds */
    uint32_t severity_number; /**< OTLP severity number (0-24) */
    const char *severity_text; /**< OTLP severity text (e.g., "INFO", "ERROR") */
    const char *body; /**< Log message body */
    const char *hostname; /**< Hostname from syslog message */
    const char *app_name; /**< Application name (tag) from syslog */
    const char *proc_id; /**< Process ID from syslog */
    const char *msg_id; /**< Message ID from syslog */
    const char *trace_id; /**< OpenTelemetry trace ID (32 hex chars) */
    const char *span_id; /**< OpenTelemetry span ID (16 hex chars) */
    uint8_t trace_flags; /**< OpenTelemetry trace flags */
    uint16_t facility; /**< Syslog facility number */
} omotlp_log_record_t;

/**
 * @brief OTLP resource attributes structure
 *
 * Contains resource-level attributes that are applied to all log records
 * in a batch. Custom attributes are provided as a parsed JSON object.
 */
typedef struct omotlp_resource_attrs_s {
    const char *service_instance_id; /**< Service instance identifier */
    const char *deployment_environment; /**< Deployment environment name */
    struct json_object *custom_attributes; /**< Parsed JSON object with custom resource attributes */
} omotlp_resource_attrs_t;

/* Forward declaration for attribute_map_t */
typedef struct attribute_map_s attribute_map_t;

/**
 * @brief Build OTLP/HTTP JSON export payload
 *
 * Converts an array of log records into an OTLP/HTTP JSON payload according
 * to the OpenTelemetry Protocol specification. The payload includes resource
 * logs, resource attributes, scope logs, and log records with their attributes.
 *
 * The function handles:
 * - Resource attribute mapping (service.name, service.instance.id, etc.)
 * - Custom attribute mapping via attribute_map
 * - Syslog property to OTLP attribute conversion
 * - Trace correlation data (trace_id, span_id, trace_flags)
 *
 * @param[in] records Array of log records to export
 * @param[in] record_count Number of records in the array
 * @param[in] resource_attrs Resource-level attributes to include
 * @param[in] attribute_map Optional mapping from rsyslog properties to OTLP attributes
 * @param[out] out_payload On success, contains the JSON payload string (caller must free)
 * @return RS_RET_OK on success, RS_RET_PARAM_ERROR for invalid parameters,
 *         RS_RET_OUT_OF_MEMORY on allocation failure
 */
rsRetVal omotlp_json_build_export(const omotlp_log_record_t *records,
                                  size_t record_count,
                                  const omotlp_resource_attrs_t *resource_attrs,
                                  const attribute_map_t *attribute_map,
                                  char **out_payload);

#endif /* OMOTLP_OTLP_JSON_H */
