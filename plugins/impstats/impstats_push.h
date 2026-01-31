/* impstats_push.h
 * Header for native Prometheus Remote Write push support in impstats
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
#ifndef IMPSTATS_PUSH_H_INCLUDED
#define IMPSTATS_PUSH_H_INCLUDED

#ifdef ENABLE_IMPSTATS_PUSH

    #include "rsyslog.h"

/* Push configuration structure */
typedef struct impstats_push_config_s {
    uchar *url; /* Full URL: http://host:8428/api/v1/write */
    uchar **labels; /* Array of additional label strings: "key=value" */
    size_t nLabels; /* Number of labels in the array */
    long timeoutMs; /* HTTP request timeout in milliseconds */
    sbool labelInstance; /* Add instance=<hostname> label */
    sbool labelOrigin; /* Add origin=<stats origin> label */
    sbool labelName; /* Add name=<stats object name> label */
    uchar *labelJob; /* Add job=<value> label (NULL disables) */
    uchar *labelInstanceValue; /* Cached hostname for instance label */
    uchar *tlsCaFile; /* CA bundle file for TLS verification */
    uchar *tlsCertFile; /* Client certificate for mTLS */
    uchar *tlsKeyFile; /* Client private key for mTLS */
    sbool tlsInsecureSkipVerify; /* Disable TLS verification (testing only) */

    /* Optional: batch configuration (future enhancement) */
    sbool batchEnabled;
    size_t batchMaxBytes;
    size_t batchMaxSeries;

    /* Optional: retry configuration (future enhancement) */
    int retryMaxAttempts;
    long retryInitialMs;
    long retryMaxMs;
} impstats_push_config_t;

/* Initialize push subsystem
 * Must be called once at module init before any push operations.
 * Returns: RS_RET_OK on success, error code otherwise
 */
rsRetVal impstats_push_init(void);

/* Send current statistics to remote endpoint
 * Iterates over all statsobj counters, converts to Prometheus Remote Write
 * format, and POSTs to the configured endpoint.
 *
 * config: Push configuration including URL and labels
 * statsobj_if: Pointer to statsobj interface for stats collection
 * Returns: RS_RET_OK on success
 *          RS_RET_SUSPENDED for retryable errors (5xx, timeouts)
 *          Error code for permanent failures
 */
rsRetVal impstats_push_send(impstats_push_config_t *config, statsobj_if_t *statsobj_if);

/* Cleanup push subsystem
 * Must be called at module exit.
 */
void impstats_push_cleanup(void);

#endif /* ENABLE_IMPSTATS_PUSH */

#endif /* IMPSTATS_PUSH_H_INCLUDED */
