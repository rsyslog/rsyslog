/**
 * @file omotel_http.c
 * @brief HTTP client implementation for OTLP/HTTP JSON transport
 *
 * This file implements the HTTP client interface using libcurl. It provides
 * HTTP POST functionality with retry semantics, TLS/mTLS support, and proxy
 * configuration.
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
#include "config.h"

#include "omotel_http.h"

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "srUtils.h"

#include "errmsg.h"

struct omotel_http_client_s {
    CURL *handle;
    struct curl_slist *headers;
    char *url;
    long timeout_ms;
    char error_buffer[CURL_ERROR_SIZE];
    long retry_initial_ms;
    long retry_max_ms;
    unsigned int retry_max_retries;
    unsigned int retry_jitter_percent;
};

static int g_http_global_initialized = 0;

/**
 * @brief libcurl write callback that discards response body
 *
 * This callback is used when we don't need to process the HTTP response body.
 * It simply discards all received data and returns the number of bytes
 * processed to satisfy libcurl's requirements.
 */
static size_t discard_response(void *ptr, size_t size, size_t nmemb, void *userdata) {
    (void)ptr;
    (void)userdata;
    return size * nmemb;
}

/**
 * @brief Configure common libcurl options for the HTTP client
 *
 * Sets up libcurl options including URL, timeout, headers, TLS configuration,
 * and proxy settings. This function is called during client creation to
 * configure the curl handle.
 *
 * @param[in,out] client HTTP client to configure
 * @param[in] config Configuration parameters
 * @return RS_RET_OK on success, RS_RET_INTERNAL_ERROR on curl configuration failure
 */
static rsRetVal set_common_options(omotel_http_client_t *client, const omotel_http_client_config_t *config) {
    CURLcode rc;
    DEFiRet;

    if (client->url == NULL) {
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    rc = curl_easy_setopt(client->handle, CURLOPT_URL, client->url);
    if (rc != CURLE_OK) {
        LogError(0, RS_RET_INTERNAL_ERROR, "omotel/http: failed to set URL: %s", curl_easy_strerror(rc));
        ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
    }

    rc = curl_easy_setopt(client->handle, CURLOPT_POST, 1L);
    if (rc != CURLE_OK) {
        LogError(0, RS_RET_INTERNAL_ERROR, "omotel/http: failed to enable POST: %s", curl_easy_strerror(rc));
        ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
    }

    rc = curl_easy_setopt(client->handle, CURLOPT_WRITEFUNCTION, discard_response);
    if (rc != CURLE_OK) {
        LogError(0, RS_RET_INTERNAL_ERROR, "omotel/http: failed to install response sink: %s", curl_easy_strerror(rc));
        ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
    }

    rc = curl_easy_setopt(client->handle, CURLOPT_NOSIGNAL, 1L);
    if (rc != CURLE_OK) {
        LogError(0, RS_RET_INTERNAL_ERROR, "omotel/http: failed to disable signals: %s", curl_easy_strerror(rc));
        ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
    }

    if (config->timeout_ms > 0) {
        rc = curl_easy_setopt(client->handle, CURLOPT_TIMEOUT_MS, config->timeout_ms);
        if (rc != CURLE_OK) {
            LogError(0, RS_RET_INTERNAL_ERROR, "omotel/http: failed to set timeout: %s", curl_easy_strerror(rc));
            ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
        }
    }

    if (config->user_agent != NULL) {
        rc = curl_easy_setopt(client->handle, CURLOPT_USERAGENT, config->user_agent);
        if (rc != CURLE_OK) {
            LogError(0, RS_RET_INTERNAL_ERROR, "omotel/http: failed to set user-agent: %s", curl_easy_strerror(rc));
            ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
        }
    }

    rc = curl_easy_setopt(client->handle, CURLOPT_ERRORBUFFER, client->error_buffer);
    if (rc != CURLE_OK) {
        LogError(0, RS_RET_INTERNAL_ERROR, "omotel/http: failed to install error buffer: %s", curl_easy_strerror(rc));
        ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
    }

    rc = curl_easy_setopt(client->handle, CURLOPT_HTTPHEADER, client->headers);
    if (rc != CURLE_OK) {
        LogError(0, RS_RET_INTERNAL_ERROR, "omotel/http: failed to apply headers: %s", curl_easy_strerror(rc));
        ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
    }

    /* TLS/SSL configuration */
    if (config->tls_ca_cert_file != NULL && config->tls_ca_cert_file[0] != '\0') {
        rc = curl_easy_setopt(client->handle, CURLOPT_CAINFO, config->tls_ca_cert_file);
        if (rc != CURLE_OK) {
            LogError(0, RS_RET_INTERNAL_ERROR, "omotel/http: failed to set CA cert file: %s", curl_easy_strerror(rc));
            ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
        }
    }

    if (config->tls_ca_cert_dir != NULL && config->tls_ca_cert_dir[0] != '\0') {
        rc = curl_easy_setopt(client->handle, CURLOPT_CAPATH, config->tls_ca_cert_dir);
        if (rc != CURLE_OK) {
            LogError(0, RS_RET_INTERNAL_ERROR, "omotel/http: failed to set CA cert directory: %s",
                     curl_easy_strerror(rc));
            ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
        }
    }

    if (config->tls_client_cert_file != NULL && config->tls_client_cert_file[0] != '\0') {
        rc = curl_easy_setopt(client->handle, CURLOPT_SSLCERT, config->tls_client_cert_file);
        if (rc != CURLE_OK) {
            LogError(0, RS_RET_INTERNAL_ERROR, "omotel/http: failed to set client cert: %s", curl_easy_strerror(rc));
            ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
        }
    }

    if (config->tls_client_key_file != NULL && config->tls_client_key_file[0] != '\0') {
        rc = curl_easy_setopt(client->handle, CURLOPT_SSLKEY, config->tls_client_key_file);
        if (rc != CURLE_OK) {
            LogError(0, RS_RET_INTERNAL_ERROR, "omotel/http: failed to set client key: %s", curl_easy_strerror(rc));
            ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
        }
    }

    /* Hostname verification */
    rc = curl_easy_setopt(client->handle, CURLOPT_SSL_VERIFYHOST, config->tls_verify_hostname ? 2L : 0L);
    if (rc != CURLE_OK) {
        LogError(0, RS_RET_INTERNAL_ERROR, "omotel/http: failed to set hostname verification: %s",
                 curl_easy_strerror(rc));
        ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
    }

    /* Peer certificate verification */
    rc = curl_easy_setopt(client->handle, CURLOPT_SSL_VERIFYPEER, config->tls_verify_peer ? 1L : 0L);
    if (rc != CURLE_OK) {
        LogError(0, RS_RET_INTERNAL_ERROR, "omotel/http: failed to set peer verification: %s", curl_easy_strerror(rc));
        ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
    }

    /* Proxy configuration */
    if (config->proxy_url != NULL && config->proxy_url[0] != '\0') {
        rc = curl_easy_setopt(client->handle, CURLOPT_PROXY, config->proxy_url);
        if (rc != CURLE_OK) {
            LogError(0, RS_RET_INTERNAL_ERROR, "omotel/http: failed to set proxy URL: %s", curl_easy_strerror(rc));
            ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
        }

        /* Proxy authentication */
        if (config->proxy_user != NULL && config->proxy_user[0] != '\0') {
            char *userpwd = NULL;
            size_t user_len = strlen(config->proxy_user);
            size_t pwd_len = (config->proxy_password != NULL) ? strlen(config->proxy_password) : 0;
            size_t total = user_len + 1 + pwd_len + 1;

            userpwd = (char *)malloc(total);
            if (userpwd == NULL) {
                ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
            }

            snprintf(userpwd, total, "%s:%s", config->proxy_user, config->proxy_password ? config->proxy_password : "");

            rc = curl_easy_setopt(client->handle, CURLOPT_PROXYUSERPWD, userpwd);
            if (rc != CURLE_OK) {
                free(userpwd);
                LogError(0, RS_RET_INTERNAL_ERROR, "omotel/http: failed to set proxy credentials: %s",
                         curl_easy_strerror(rc));
                ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
            }

            free(userpwd);
        }
    }

finalize_it:
    RETiRet;
}

rsRetVal omotel_http_global_init(void) {
    if (!g_http_global_initialized) {
        CURLcode rc = curl_global_init(CURL_GLOBAL_DEFAULT);
        if (rc != CURLE_OK) {
            LogError(0, RS_RET_INTERNAL_ERROR, "omotel/http: curl_global_init failed: %s", curl_easy_strerror(rc));
            return RS_RET_INTERNAL_ERROR;
        }
        g_http_global_initialized = 1;
    }

    return RS_RET_OK;
}

void omotel_http_global_cleanup(void) {
    if (g_http_global_initialized) {
        curl_global_cleanup();
        g_http_global_initialized = 0;
    }
}

rsRetVal omotel_http_client_create(const omotel_http_client_config_t *config, omotel_http_client_t **out_client) {
    omotel_http_client_t *client = NULL;
    DEFiRet;

    if (config == NULL || out_client == NULL || config->url == NULL) {
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    client = calloc(1, sizeof(*client));
    if (client == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    client->url = strdup(config->url);
    if (client->url == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }
    client->timeout_ms = config->timeout_ms;

    client->headers = curl_slist_append(client->headers, "Content-Type: application/json");
    if (client->headers == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }
    client->headers = curl_slist_append(client->headers, "Accept: application/json");
    if (client->headers == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    if (config->headers != NULL && config->header_count > 0) {
        size_t i;
        for (i = 0; i < config->header_count; ++i) {
            client->headers = curl_slist_append(client->headers, config->headers[i]);
            if (client->headers == NULL) {
                ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
            }
        }
    }

    client->handle = curl_easy_init();
    if (client->handle == NULL) {
        LogError(0, RS_RET_INTERNAL_ERROR, "omotel/http: curl_easy_init failed");
        ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
    }

    client->error_buffer[0] = '\0';
    client->retry_initial_ms = config->retry_initial_ms;
    client->retry_max_ms = config->retry_max_ms;
    client->retry_max_retries = config->retry_max_retries;
    client->retry_jitter_percent = config->retry_jitter_percent;
    CHKiRet(set_common_options(client, config));

    *out_client = client;
    client = NULL;

finalize_it:
    if (iRet != RS_RET_OK) {
        omotel_http_client_destroy(&client);
    }

    RETiRet;
}

void omotel_http_client_destroy(omotel_http_client_t **client_ptr) {
    omotel_http_client_t *client;

    if (client_ptr == NULL || *client_ptr == NULL) {
        return;
    }

    client = *client_ptr;
    *client_ptr = NULL;

    if (client->headers != NULL) {
        curl_slist_free_all(client->headers);
    }
    if (client->handle != NULL) {
        curl_easy_cleanup(client->handle);
    }
    free(client->url);
    free(client);
}

/**
 * @brief Apply jitter to a retry delay value
 *
 * Adds random jitter to a base delay value to prevent thundering herd
 * problems when multiple clients retry simultaneously. The jitter is
 * calculated as a percentage of the base delay and applied as a random
 * offset in the range [-jitter, +jitter].
 *
 * @param[in] base_delay Base delay in milliseconds
 * @param[in] jitter_percent Jitter percentage (0-100)
 * @return Delay value with jitter applied (always >= 0)
 */
static long apply_jitter(long base_delay, unsigned int jitter_percent) {
    long range;
    long offset;

    if (base_delay <= 0 || jitter_percent == 0) {
        return base_delay;
    }

    range = (base_delay * (long)jitter_percent) / 100L;
    if (range <= 0) {
        return base_delay;
    }

    offset = (long)(labs(randomNumber()) % (range * 2L + 1L)) - range;
    base_delay += offset;
    if (base_delay < 0) {
        base_delay = 0;
    }

    return base_delay;
}

static void sleep_with_backoff(long delay_ms) {
    if (delay_ms <= 0) {
        return;
    }

    srSleep((int)(delay_ms / 1000L), (int)((delay_ms % 1000L) * 1000L));
}

static int should_retry_status(long status) {
    if (status == 0) {
        return 1;
    }

    if (status == 408 || status == 429) {
        return 1;
    }

    if (status >= 500) {
        return 1;
    }

    return 0;
}

rsRetVal omotel_http_client_post(omotel_http_client_t *client,
                                 const uint8_t *payload,
                                 size_t payload_len,
                                 long *out_status_code,
                                 long *out_latency_ms) {
    long status = 0;
    CURLcode rc;
    const uint8_t empty_payload[] = "";
    const uint8_t *payload_bytes;
    unsigned int retries = 0u;
    long delay_ms;
    long long start_ms = 0;
    long long end_ms = 0;
    DEFiRet;

    DBGPRINTF("omotel/http: omotel_http_client_post called, payload_len=%zu, url=%s", payload_len,
              client ? (client->url ? client->url : "(null)") : "(null client)");

    if (client == NULL) {
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    if (out_status_code != NULL) {
        *out_status_code = 0;
    }
    if (out_latency_ms != NULL) {
        *out_latency_ms = 0;
    }

    payload_bytes = payload != NULL ? payload : empty_payload;

    client->error_buffer[0] = '\0';

    rc = curl_easy_setopt(client->handle, CURLOPT_POSTFIELDS, (const char *)payload_bytes);
    if (rc != CURLE_OK) {
        LogError(0, RS_RET_INTERNAL_ERROR, "omotel/http: failed to set payload: %s", curl_easy_strerror(rc));
        ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
    }

    rc = curl_easy_setopt(client->handle, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)payload_len);
    if (rc != CURLE_OK) {
        LogError(0, RS_RET_INTERNAL_ERROR, "omotel/http: failed to set payload size: %s", curl_easy_strerror(rc));
        ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
    }

    delay_ms = client->retry_initial_ms;

    for (;;) {
        client->error_buffer[0] = '\0';
        status = 0;
        start_ms = currentTimeMills();

        DBGPRINTF("omotel/http: calling curl_easy_perform, attempt %u", retries + 1);
        rc = curl_easy_perform(client->handle);
        end_ms = currentTimeMills();
        DBGPRINTF("omotel/http: curl_easy_perform returned: %d (%s)", rc, curl_easy_strerror(rc));
        if (rc != CURLE_OK) {
            const char *err = client->error_buffer[0] != '\0' ? client->error_buffer : curl_easy_strerror(rc);
            LogError(0, RS_RET_SUSPENDED, "omotel/http: HTTP POST failed: %s", err);

            if (retries >= client->retry_max_retries) {
                ABORT_FINALIZE(RS_RET_SUSPENDED);
            }

            if (delay_ms > 0) {
                sleep_with_backoff(apply_jitter(delay_ms, client->retry_jitter_percent));
                delay_ms *= 2;
                if (client->retry_max_ms > 0 && delay_ms > client->retry_max_ms) {
                    delay_ms = client->retry_max_ms;
                }
            }
            ++retries;
            continue;
        }

        rc = curl_easy_getinfo(client->handle, CURLINFO_RESPONSE_CODE, &status);
        if (rc != CURLE_OK) {
            LogError(0, RS_RET_INTERNAL_ERROR, "omotel/http: failed to read response code: %s", curl_easy_strerror(rc));
            ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
        }

        DBGPRINTF("omotel/http: HTTP response status: %ld", status);
        if (status >= 200 && status < 300) {
            DBGPRINTF("omotel/http: HTTP POST successful (status %ld)", status);
            goto finalize_it;
        }

        if (should_retry_status(status)) {
            LogError(0, RS_RET_SUSPENDED, "omotel/http: collector returned status %ld; retrying batch", status);
            if (retries >= client->retry_max_retries) {
                ABORT_FINALIZE(RS_RET_SUSPENDED);
            }

            if (delay_ms > 0) {
                sleep_with_backoff(apply_jitter(delay_ms, client->retry_jitter_percent));
                delay_ms *= 2;
                if (client->retry_max_ms > 0 && delay_ms > client->retry_max_ms) {
                    delay_ms = client->retry_max_ms;
                }
            }
            ++retries;
            continue;
        }

        LogError(0, RS_RET_DISCARDMSG, "omotel/http: collector rejected payload with status %ld", status);
        ABORT_FINALIZE(RS_RET_DISCARDMSG);
    }

finalize_it:
    if (out_status_code != NULL) {
        *out_status_code = status;
    }
    if (out_latency_ms != NULL && start_ms > 0 && end_ms >= start_ms) {
        *out_latency_ms = (long)(end_ms - start_ms);
    }
    DBGPRINTF("omotel/http: omotel_http_client_post completed, iRet=%d", iRet);
    RETiRet;
}
