/* impstats_push.c
 * Native Prometheus Remote Write push support for impstats
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
#include "config.h"

#ifdef ENABLE_IMPSTATS_PUSH

    #include <stdlib.h>
    #include <string.h>
    #include <sys/time.h>
    #include <curl/curl.h>

    #include "rsyslog.h"
    #include "errmsg.h"
    #include "statsobj.h"
    #include "impstats_push.h"
    #include "prometheus_remote_write.h"

/* Module static data */
static CURL *curl_handle = NULL;
static impstats_push_config_t *static_config = NULL;
static statsobj_if_t *static_statsobj = NULL;
static int push_initialized = 0; /* Track if curl_global_init was called */

/* libcurl write callback (we ignore response body) */
static size_t push_write_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
    (void)ptr;
    (void)userdata;
    return size * nmemb; /* Tell curl we handled it */
}

/* Initialize push subsystem */
rsRetVal impstats_push_init(void) {
    DEFiRet;
    CURLcode curl_ret;

    if (push_initialized) {
        /* Already initialized, don't re-init curl_global */
        FINALIZE;
    }

    /* Initialize libcurl globally (once) */
    curl_ret = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (curl_ret != CURLE_OK) {
        LogError(0, RS_RET_ERR, "impstats_push: curl_global_init failed: %s", curl_easy_strerror(curl_ret));
        ABORT_FINALIZE(RS_RET_ERR);
    }

    /* Create a reusable CURL handle */
    curl_handle = curl_easy_init();
    if (curl_handle == NULL) {
        LogError(0, RS_RET_ERR, "impstats_push: curl_easy_init failed");
        curl_global_cleanup();
        ABORT_FINALIZE(RS_RET_ERR);
    }

    /* Set common options */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, push_write_callback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, NULL);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "rsyslog-impstats");

    push_initialized = 1;

finalize_it:
    RETiRet;
}

/* Callback to collect Prometheus text format lines */
static rsRetVal stats_line_cb(void *ctx, const char *line) {
    DEFiRet;
    prom_write_request_t *wr = (prom_write_request_t *)ctx;

    /* Line may contain multiple newline-separated entries */
    char *line_copy;
    CHKmalloc(line_copy = strdup(line));

    char *saveptr = NULL;
    char *token = strtok_r(line_copy, "\n", &saveptr);

    while (token != NULL) {
        /* Skip HELP and TYPE lines */
        if (token[0] != '#' && token[0] != '\0') {
            /* Parse: metric_name value */
            const char *space = strchr(token, ' ');
            if (space != NULL) {
                size_t metric_len = space - token;
                char *metric_buf = NULL;

                /* Allocate buffer based on actual metric name length */
                CHKmalloc(metric_buf = malloc(metric_len + 1));
                memcpy(metric_buf, token, metric_len);
                metric_buf[metric_len] = '\0';

                double value = atof(space + 1);

                struct timeval tv;
                gettimeofday(&tv, NULL);
                int64_t timestamp_ms = (int64_t)tv.tv_sec * 1000 + (int64_t)tv.tv_usec / 1000;

                rsRetVal localRet = prom_write_request_add_metric(
                    wr, metric_buf, value, timestamp_ms, (const char **)static_config->labels, static_config->nLabels);
                if (localRet != RS_RET_OK) {
                    DBGPRINTF("impstats_push: failed to add metric %s, error %d\n", metric_buf, localRet);
                    /* Continue processing other metrics even if one fails */
                }

                free(metric_buf);
            }
        }
        token = strtok_r(NULL, "\n", &saveptr);
    }

finalize_it:
    free(line_copy);
    RETiRet;
}

/* Send current statistics to remote endpoint */
rsRetVal impstats_push_send(impstats_push_config_t *config, statsobj_if_t *statsobj_if) {
    DEFiRet;
    prom_write_request_t *wr = NULL;
    uint8_t *encoded_data = NULL;
    size_t encoded_size = 0;
    CURLcode curl_ret;
    long http_code = 0;
    struct curl_slist *headers = NULL;

    /* Store config and statsobj interface for use by callbacks */
    static_config = config;
    static_statsobj = statsobj_if;

    /* Create protobuf write request */
    wr = prom_write_request_new();
    if (wr == NULL) {
        LogError(0, RS_RET_OUT_OF_MEMORY, "impstats_push: failed to create write request");
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    /* Collect stats using Prometheus format */
    iRet = static_statsobj->GetAllStatsLines(stats_line_cb, wr, statsFmt_Prometheus, 0);
    if (iRet != RS_RET_OK) {
        LogError(0, iRet, "impstats_push: failed to collect stats");
        FINALIZE;
    }

    /* Encode to protobuf + snappy */
    iRet = prom_write_request_encode(wr, &encoded_data, &encoded_size);
    if (iRet != RS_RET_OK) {
        LogError(0, iRet, "impstats_push: failed to encode protobuf");
        FINALIZE;
    }

    /* Setup HTTP POST */
    curl_easy_setopt(curl_handle, CURLOPT_URL, (const char *)config->url);
    curl_easy_setopt(curl_handle, CURLOPT_POST, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, encoded_data);
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, (long)encoded_size);
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT_MS, config->timeoutMs);

    /* Set required headers */
    headers = curl_slist_append(headers, "Content-Type: application/x-protobuf");
    headers = curl_slist_append(headers, "Content-Encoding: snappy");
    headers = curl_slist_append(headers, "X-Prometheus-Remote-Write-Version: 0.1.0");
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);

    /* Perform request */
    curl_ret = curl_easy_perform(curl_handle);
    if (curl_ret != CURLE_OK) {
        LogError(0, RS_RET_ERR, "impstats_push: HTTP request failed: %s", curl_easy_strerror(curl_ret));
        iRet = RS_RET_SUSPENDED; /* Retryable */
        FINALIZE;
    }

    /* Check HTTP status */
    curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code < 200 || http_code >= 300) {
        LogError(0, RS_RET_ERR, "impstats_push: HTTP %ld from remote endpoint", http_code);
        iRet = (http_code >= 500) ? RS_RET_SUSPENDED : RS_RET_ERR;
        FINALIZE;
    }

    DBGPRINTF("impstats_push: successfully pushed %zu bytes\n", encoded_size);

finalize_it:
    if (headers) curl_slist_free_all(headers);
    if (encoded_data) free(encoded_data);
    if (wr) prom_write_request_free(wr);
    static_config = NULL;
    static_statsobj = NULL;
    RETiRet;
}

/* Cleanup push subsystem */
void impstats_push_cleanup(void) {
    if (!push_initialized) {
        /* Never initialized, nothing to clean up */
        return;
    }

    if (curl_handle) {
        curl_easy_cleanup(curl_handle);
        curl_handle = NULL;
    }
    curl_global_cleanup();
    push_initialized = 0;
}

#endif /* ENABLE_IMPSTATS_PUSH */
