/* prometheus_remote_write.h
 * Encoder for Prometheus Remote Write protocol (protobuf + snappy)
 *
 * Copyright 2024-2026 Adiscon GmbH and contributors
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
#ifndef PROMETHEUS_REMOTE_WRITE_H_INCLUDED
#define PROMETHEUS_REMOTE_WRITE_H_INCLUDED

#ifdef ENABLE_IMPSTATS_PUSH

    #include "rsyslog.h"
    #include "statsobj.h"
    #include "remote.pb-c.h"

/* Opaque handle for a write request builder */
typedef struct prom_write_request_s prom_write_request_t;

/* Create a new WriteRequest builder
 * Returns: Opaque handle or NULL on error
 */
prom_write_request_t *prom_write_request_new(void);
prom_write_request_t *prom_write_request_new_with_series(Prometheus__TimeSeries **series, size_t n_series);
void prom_write_request_detach_series(prom_write_request_t *request,
                                      Prometheus__TimeSeries ***series,
                                      size_t *n_series);
size_t prom_write_request_get_packed_size(prom_write_request_t *request);
size_t prom_write_request_get_series_count(prom_write_request_t *request);
void prom_write_request_free_series(Prometheus__TimeSeries **series, size_t n_series);

/* Add a time series to the write request
 *
 * request: WriteRequest builder handle
 * metric_name: Metric name (becomes __name__ label)
 * value: Metric value
 * timestamp_ms: Timestamp in milliseconds since Unix epoch
 * labels: Array of label strings in "key=value" format (may be NULL)
 * nLabels: Number of labels in the array
 *
 * Returns: RS_RET_OK on success, error code otherwise
 */
rsRetVal prom_write_request_add_metric(prom_write_request_t *request,
                                       const char *metric_name,
                                       double value,
                                       int64_t timestamp_ms,
                                       const char **labels,
                                       size_t nLabels);

/* Encode the WriteRequest to protobuf and compress with Snappy
 *
 * request: WriteRequest builder handle
 * out_data: Output buffer pointer (caller must free with free())
 * out_len: Output buffer length
 *
 * Returns: RS_RET_OK on success, error code otherwise
 */
rsRetVal prom_write_request_encode(prom_write_request_t *request, uint8_t **out_data, size_t *out_len);

/* Free a WriteRequest builder and all associated memory
 *
 * request: WriteRequest builder handle (may be NULL)
 */
void prom_write_request_free(prom_write_request_t *request);

#endif /* ENABLE_IMPSTATS_PUSH */

#endif /* PROMETHEUS_REMOTE_WRITE_H_INCLUDED */
