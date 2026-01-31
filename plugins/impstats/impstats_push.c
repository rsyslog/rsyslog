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
static int push_initialized = 0; /* Track if curl_global_init was called */

typedef struct impstats_push_ctx_s {
    impstats_push_config_t *config;
    prom_write_request_t *wr;
} impstats_push_ctx_t;

/* libcurl write callback (we ignore response body) */
static size_t push_write_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
    (void)ptr;
    (void)userdata;
    return size * nmemb; /* Tell curl we handled it */
}

static sbool label_key_exists(const uchar **labels, size_t n_labels, const char *key) {
    size_t key_len;
    size_t i;

    if (labels == NULL || key == NULL) {
        return 0;
    }

    key_len = strlen(key);
    for (i = 0; i < n_labels; i++) {
        const char *label = (const char *)labels[i];
        const char *eq;

        if (label == NULL) continue;
        eq = strchr(label, '=');
        if (eq == NULL) continue;
        if ((size_t)(eq - label) == key_len && strncmp(label, key, key_len) == 0) {
            return 1;
        }
    }
    return 0;
}

static rsRetVal build_label_kv(const char *key, const uchar *value, char **ppLabel) {
    DEFiRet;
    char *label = NULL;
    size_t key_len;
    size_t value_len;

    if (key == NULL || value == NULL || value[0] == '\0') {
        *ppLabel = NULL;
        FINALIZE;
    }

    key_len = strlen(key);
    value_len = strlen((const char *)value);
    CHKmalloc(label = malloc(key_len + 1 + value_len + 1));
    memcpy(label, key, key_len);
    label[key_len] = '=';
    memcpy(label + key_len + 1, value, value_len);
    label[key_len + 1 + value_len] = '\0';

    *ppLabel = label;
    label = NULL; /* ownership transferred */

finalize_it:
    free(label);
    RETiRet;
}

static void log_tls_error(CURLcode curl_ret, const impstats_push_config_t *config) {
    const char *url = config && config->url ? (const char *)config->url : "(null)";

    if (curl_ret == CURLE_PEER_FAILED_VERIFICATION || curl_ret == CURLE_SSL_CACERT_BADFILE) {
        LogError(0, RS_RET_ERR,
                 "impstats_push: TLS verification failed for %s. Provide push.tls.cafile or "
                 "set push.tls.insecureSkipVerify=\"on\" for testing",
                 url);
        return;
    }

    if (curl_ret == CURLE_SSL_CERTPROBLEM) {
        LogError(0, RS_RET_ERR,
                 "impstats_push: TLS client cert problem for %s. Check push.tls.certfile and push.tls.keyfile", url);
        return;
    }

    if (curl_ret == CURLE_SSL_CONNECT_ERROR) {
        LogError(0, RS_RET_ERR, "impstats_push: TLS connection error for %s", url);
        return;
    }

    if (curl_ret == CURLE_SSL_ENGINE_NOTFOUND || curl_ret == CURLE_SSL_ENGINE_SETFAILED) {
        LogError(0, RS_RET_ERR, "impstats_push: TLS engine error for %s", url);
        return;
    }
}

static rsRetVal send_write_request(prom_write_request_t *wr, impstats_push_config_t *config) {
    DEFiRet;
    uint8_t *encoded_data = NULL;
    size_t encoded_size = 0;
    CURLcode curl_ret;
    long http_code = 0;
    struct curl_slist *headers = NULL;

    if (wr == NULL || config == NULL) {
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    iRet = prom_write_request_encode(wr, &encoded_data, &encoded_size);
    if (iRet != RS_RET_OK) {
        LogError(0, iRet, "impstats_push: failed to encode protobuf");
        FINALIZE;
    }

    curl_easy_setopt(curl_handle, CURLOPT_URL, (const char *)config->url);
    curl_easy_setopt(curl_handle, CURLOPT_POST, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, encoded_data);
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, (long)encoded_size);
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT_MS, config->timeoutMs);
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, config->tlsInsecureSkipVerify ? 0L : 1L);
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, config->tlsInsecureSkipVerify ? 0L : 2L);

    if (config->tlsCaFile != NULL) {
        curl_easy_setopt(curl_handle, CURLOPT_CAINFO, (const char *)config->tlsCaFile);
    }
    if (config->tlsCertFile != NULL) {
        curl_easy_setopt(curl_handle, CURLOPT_SSLCERT, (const char *)config->tlsCertFile);
    }
    if (config->tlsKeyFile != NULL) {
        curl_easy_setopt(curl_handle, CURLOPT_SSLKEY, (const char *)config->tlsKeyFile);
    }

    struct curl_slist *tmp_headers;

    tmp_headers = curl_slist_append(headers, "Content-Type: application/x-protobuf");
    if (tmp_headers == NULL) {
        LogError(0, RS_RET_OUT_OF_MEMORY, "impstats_push: failed to allocate headers");
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }
    headers = tmp_headers;

    tmp_headers = curl_slist_append(headers, "Content-Encoding: snappy");
    if (tmp_headers == NULL) {
        LogError(0, RS_RET_OUT_OF_MEMORY, "impstats_push: failed to allocate headers");
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }
    headers = tmp_headers;

    tmp_headers = curl_slist_append(headers, "X-Prometheus-Remote-Write-Version: 0.1.0");
    if (tmp_headers == NULL) {
        LogError(0, RS_RET_OUT_OF_MEMORY, "impstats_push: failed to allocate headers");
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }
    headers = tmp_headers;
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);

    curl_ret = curl_easy_perform(curl_handle);
    if (curl_ret != CURLE_OK) {
        log_tls_error(curl_ret, config);
        LogError(0, RS_RET_ERR, "impstats_push: HTTP request failed: %s", curl_easy_strerror(curl_ret));
        iRet = RS_RET_SUSPENDED;
        FINALIZE;
    }

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
    RETiRet;
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

/* Sanitize a component for Prometheus metric name:
 * - Replace '-' with '_'
 * - Replace '.' with '_'
 * - Replace any non-[a-zA-Z0-9_] with '_'
 * Output parameter: ppOutput receives newly allocated string (caller must free)
 */
static rsRetVal sanitize_component(const uchar *input, char **ppOutput) {
    DEFiRet;
    char *output = NULL;

    if (input == NULL || input[0] == '\0') {
        *ppOutput = NULL;
        FINALIZE;
    }

    size_t len = strlen((const char *)input);
    CHKmalloc(output = malloc(len + 1));

    for (size_t i = 0; i < len; i++) {
        char c = input[i];
        if (c == '-' || c == '.') {
            output[i] = '_';
        } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
            output[i] = c;
        } else {
            output[i] = '_';
        }
    }
    output[len] = '\0';
    *ppOutput = output;
    output = NULL; /* ownership transferred */

finalize_it:
    free(output);
    RETiRet;
}

/* Build Prometheus metric name from components:
 * Format: <origin>_<name>_<counter>_total (name omitted if NULL/empty)
 * Output parameter: ppMetric receives newly allocated string (caller must free)
 */
static rsRetVal build_metric_name(const uchar *origin, const uchar *name, const uchar *counter, char **ppMetric) {
    DEFiRet;
    char *san_origin = NULL;
    char *san_name = NULL;
    char *san_counter = NULL;
    char *metric = NULL;
    size_t total_len;

    if (origin == NULL || counter == NULL) {
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    CHKiRet(sanitize_component(origin, &san_origin));
    if (san_origin == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    CHKiRet(sanitize_component(counter, &san_counter));
    if (san_counter == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    /* Check if name is non-empty */
    if (name != NULL && name[0] != '\0') {
        CHKiRet(sanitize_component(name, &san_name));
        if (san_name == NULL) {
            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        }
        /* origin + "_" + name + "_" + counter + "_total" + '\0' */
        total_len = strlen(san_origin) + 1 + strlen(san_name) + 1 + strlen(san_counter) + 7;
        CHKmalloc(metric = malloc(total_len));
        snprintf(metric, total_len, "%s_%s_%s_total", san_origin, san_name, san_counter);
    } else {
        /* origin + "_" + counter + "_total" + '\0' */
        total_len = strlen(san_origin) + 1 + strlen(san_counter) + 7;
        CHKmalloc(metric = malloc(total_len));
        snprintf(metric, total_len, "%s_%s_total", san_origin, san_counter);
    }

    *ppMetric = metric;
    metric = NULL; /* ownership transferred */

finalize_it:
    free(san_origin);
    free(san_name);
    free(san_counter);
    free(metric);
    RETiRet;
}

/* Native counter callback - receives raw counter values from statsobj */
static rsRetVal stats_counter_cb(void *ctx,
                                 const uchar *obj_name,
                                 const uchar *obj_origin,
                                 const uchar *ctr_name,
                                 statsCtrType_t ctr_type,
                                 uint64_t value,
                                 int8_t flags) {
    DEFiRet;
    impstats_push_ctx_t *push_ctx = (impstats_push_ctx_t *)ctx;
    prom_write_request_t *wr;
    impstats_push_config_t *config;
    char *metric_name = NULL;
    char *label_origin = NULL;
    char *label_name = NULL;
    char *label_instance = NULL;
    char *label_job = NULL;
    const char **all_labels = NULL;
    size_t all_label_count = 0;
    size_t extra_label_capacity = 0;
    size_t i;

    (void)ctr_type; /* unused - we accept both types */
    (void)flags; /* unused for now */

    if (push_ctx == NULL || push_ctx->config == NULL || push_ctx->wr == NULL) {
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    wr = push_ctx->wr;
    config = push_ctx->config;

    /* Build Prometheus-compliant metric name */
    CHKiRet(build_metric_name(obj_origin, obj_name, ctr_name, &metric_name));

    /* Get current timestamp */
    struct timeval tv;
    gettimeofday(&tv, NULL);
    int64_t timestamp_ms = (int64_t)tv.tv_sec * 1000 + (int64_t)tv.tv_usec / 1000;

    /* Assemble labels: user-provided labels + optional dynamic labels */
    extra_label_capacity = 4;
    CHKmalloc(all_labels = calloc(config->nLabels + extra_label_capacity, sizeof(char *)));

    for (i = 0; i < config->nLabels; i++) {
        all_labels[all_label_count++] = (const char *)config->labels[i];
    }

    if (config->labelInstance && config->labelInstanceValue &&
        !label_key_exists((const uchar **)all_labels, all_label_count, "instance")) {
        CHKiRet(build_label_kv("instance", config->labelInstanceValue, &label_instance));
        if (label_instance != NULL) {
            all_labels[all_label_count++] = label_instance;
        }
    }

    if (config->labelJob && config->labelJob[0] != '\0' &&
        !label_key_exists((const uchar **)all_labels, all_label_count, "job")) {
        CHKiRet(build_label_kv("job", config->labelJob, &label_job));
        if (label_job != NULL) {
            all_labels[all_label_count++] = label_job;
        }
    }

    if (config->labelOrigin && obj_origin != NULL && obj_origin[0] != '\0' &&
        !label_key_exists((const uchar **)all_labels, all_label_count, "origin")) {
        CHKiRet(build_label_kv("origin", obj_origin, &label_origin));
        if (label_origin != NULL) {
            all_labels[all_label_count++] = label_origin;
        }
    }

    if (config->labelName && obj_name != NULL && obj_name[0] != '\0' &&
        !label_key_exists((const uchar **)all_labels, all_label_count, "name")) {
        CHKiRet(build_label_kv("name", obj_name, &label_name));
        if (label_name != NULL) {
            all_labels[all_label_count++] = label_name;
        }
    }

    /* Add metric to protobuf request */
    rsRetVal localRet =
        prom_write_request_add_metric(wr, metric_name, (double)value, timestamp_ms, all_labels, all_label_count);

    if (localRet != RS_RET_OK) {
        DBGPRINTF("impstats_push: failed to add metric %s, error %d\n", metric_name, localRet);
        /* Continue processing other metrics even if one fails */
    }

finalize_it:
    free(metric_name);
    free(label_origin);
    free(label_name);
    free(label_instance);
    free(label_job);
    free(all_labels);
    RETiRet;
}

/* Send current statistics to remote endpoint */
rsRetVal impstats_push_send(impstats_push_config_t *config, statsobj_if_t *statsobj_if) {
    DEFiRet;
    prom_write_request_t *wr = NULL;
    impstats_push_ctx_t ctx;
    Prometheus__TimeSeries **series = NULL;
    size_t series_count = 0;
    size_t batch_max_series = 0;
    size_t idx = 0;
    size_t packed_size = 0;
    size_t i;

    if (config == NULL || statsobj_if == NULL) {
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    /* Create protobuf write request */
    wr = prom_write_request_new();
    if (wr == NULL) {
        LogError(0, RS_RET_OUT_OF_MEMORY, "impstats_push: failed to create write request");
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    ctx.config = config;
    ctx.wr = wr;

    /* Collect stats using Prometheus format */
    iRet = statsobj_if->GetAllCounters(stats_counter_cb, &ctx);
    if (iRet != RS_RET_OK) {
        LogError(0, iRet, "impstats_push: failed to collect stats");
        FINALIZE;
    }

    batch_max_series = config->batchMaxSeries;
    if (config->batchMaxBytes > 0) {
        packed_size = prom_write_request_get_packed_size(wr);
        if (packed_size > config->batchMaxBytes && prom_write_request_get_series_count(wr) > 0) {
            size_t est = (config->batchMaxBytes * prom_write_request_get_series_count(wr)) / packed_size;
            if (est == 0) est = 1;
            if (batch_max_series == 0 || est < batch_max_series) {
                batch_max_series = est;
            }
        }
    }

    if (config->batchEnabled == 0 || batch_max_series == 0) {
        iRet = send_write_request(wr, config);
        FINALIZE;
    }

    prom_write_request_detach_series(wr, &series, &series_count);
    prom_write_request_free(wr);
    wr = NULL;

    while (idx < series_count) {
        size_t remaining = series_count - idx;
        size_t batch_count = remaining < batch_max_series ? remaining : batch_max_series;
        Prometheus__TimeSeries **batch_series = NULL;
        prom_write_request_t *batch_wr = NULL;

        CHKmalloc(batch_series = calloc(batch_count, sizeof(Prometheus__TimeSeries *)));
        for (i = 0; i < batch_count; i++) {
            batch_series[i] = series[idx + i];
            series[idx + i] = NULL;
        }

        batch_wr = prom_write_request_new_with_series(batch_series, batch_count);
        if (batch_wr == NULL) {
            prom_write_request_free_series(batch_series, batch_count);
            free(batch_series);
            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        }

        iRet = send_write_request(batch_wr, config);
        prom_write_request_free(batch_wr);
        batch_wr = NULL;

        if (iRet != RS_RET_OK) {
            prom_write_request_free_series(series, series_count);
            series = NULL;
            ABORT_FINALIZE(iRet);
        }

        idx += batch_count;
    }

finalize_it:
    if (wr) prom_write_request_free(wr);
    if (series) {
        prom_write_request_free_series(series, series_count);
        free(series);
    }
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
