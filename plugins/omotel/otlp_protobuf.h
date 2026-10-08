/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file otlp_protobuf.h
 * @brief OTLP protobuf payload builder interface
 *
 * This header defines the API for building OpenTelemetry Protocol (OTLP)
 * protobuf payloads from rsyslog log records. It provides a parallel
 * encoding path to otlp_json.h for use with the http/protobuf protocol.
 *
 * Copyright 2025-2026 Adiscon GmbH.
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
#ifndef OMOTEL_OTLP_PROTOBUF_H
#define OMOTEL_OTLP_PROTOBUF_H

#include "rsyslog.h"
#include "otlp_json.h" /* Reuse omotel_log_record_t, omotel_resource_attrs_t, attribute_map_t */

/**
 * @brief Build OTLP/HTTP protobuf export payload
 *
 * Converts an array of log records into an OTLP/HTTP protobuf payload
 * (ExportLogsServiceRequest) according to the OpenTelemetry Protocol
 * specification.
 *
 * @param[in] records Array of log records to export
 * @param[in] record_count Number of records in the array
 * @param[in] resource_attrs Resource-level attributes to include
 * @param[in] attribute_map Optional mapping from rsyslog properties to OTLP attributes
 * @param[out] out_payload On success, serialized protobuf bytes (caller must free)
 * @param[out] out_len On success, length of the serialized payload
 * @return RS_RET_OK on success, RS_RET_PARAM_ERROR for invalid parameters,
 *         RS_RET_OUT_OF_MEMORY on allocation failure
 */
rsRetVal omotel_protobuf_build_export(const omotel_log_record_t *records,
                                      size_t record_count,
                                      const omotel_resource_attrs_t *resource_attrs,
                                      const attribute_map_t *attribute_map,
                                      uint8_t **out_payload,
                                      size_t *out_len);

#endif /* OMOTEL_OTLP_PROTOBUF_H */
