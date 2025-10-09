/**
 * @file omotlp_http.h
 * @brief HTTP client interface for OTLP/HTTP JSON transport
 *
 * This header defines the HTTP client API used by the omotlp module to
 * communicate with OpenTelemetry collectors over HTTP/JSON. It provides
 * functions for client lifecycle management and HTTP POST operations with
 * retry semantics, TLS, and proxy support.
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
#ifndef OMOTLP_HTTP_H
#define OMOTLP_HTTP_H

#include <stddef.h>
#include <stdint.h>

#include "rsyslog.h"

/**
 * @brief HTTP client configuration structure
 *
 * Contains all configuration parameters needed to create and configure
 * an HTTP client for OTLP/HTTP JSON transport.
 */
typedef struct omotlp_http_client_config_s {
    const char *url; /**< Target URL for HTTP POST requests */
    long timeout_ms; /**< Request timeout in milliseconds */
    const char *user_agent; /**< User-Agent header value */
    const char *const *headers; /**< Array of additional HTTP headers (key:value format) */
    size_t header_count; /**< Number of headers in headers array */
    long retry_initial_ms; /**< Initial retry delay in milliseconds */
    long retry_max_ms; /**< Maximum retry delay in milliseconds */
    unsigned int retry_max_retries; /**< Maximum number of retry attempts */
    unsigned int retry_jitter_percent; /**< Jitter percentage for retry delays (0-100) */
    /* TLS configuration */
    const char *tls_ca_cert_file; /**< Path to CA certificate file */
    const char *tls_ca_cert_dir; /**< Path to directory containing CA certificates */
    const char *tls_client_cert_file; /**< Path to client certificate file */
    const char *tls_client_key_file; /**< Path to client private key file */
    int tls_verify_hostname; /**< Enable hostname verification (0=disabled, 1=enabled) */
    int tls_verify_peer; /**< Enable peer certificate verification (0=disabled, 1=enabled) */
    /* Proxy configuration */
    const char *proxy_url; /**< Proxy URL (http://, https://, socks4://, or socks5://) */
    const char *proxy_user; /**< Proxy username for authentication */
    const char *proxy_password; /**< Proxy password for authentication */
} omotlp_http_client_config_t;

/** @brief Opaque HTTP client handle */
typedef struct omotlp_http_client_s omotlp_http_client_t;

/**
 * @brief Initialize the HTTP client library
 *
 * This function must be called once before creating any HTTP clients.
 * It initializes the underlying libcurl library. Multiple calls are
 * safe and will only initialize once.
 *
 * @return RS_RET_OK on success, RS_RET_INTERNAL_ERROR on failure
 */
rsRetVal omotlp_http_global_init(void);

/**
 * @brief Cleanup the HTTP client library
 *
 * This function should be called during module shutdown to cleanup
 * the underlying libcurl library. It is safe to call multiple times.
 */
void omotlp_http_global_cleanup(void);

/**
 * @brief Create a new HTTP client instance
 *
 * Creates and configures an HTTP client with the provided configuration.
 * The client handle must be destroyed using omotlp_http_client_destroy()
 * when no longer needed.
 *
 * @param[in] config Configuration parameters for the HTTP client
 * @param[out] out_client On success, contains the created client handle
 * @return RS_RET_OK on success, RS_RET_PARAM_ERROR if parameters are invalid,
 *         RS_RET_OUT_OF_MEMORY on allocation failure, RS_RET_INTERNAL_ERROR
 *         on configuration failure
 */
rsRetVal omotlp_http_client_create(const omotlp_http_client_config_t *config, omotlp_http_client_t **out_client);

/**
 * @brief Destroy an HTTP client instance
 *
 * Releases all resources associated with the HTTP client. The client handle
 * is set to NULL after destruction. It is safe to call with a NULL pointer.
 *
 * @param[in,out] client_ptr Pointer to the client handle to destroy
 */
void omotlp_http_client_destroy(omotlp_http_client_t **client_ptr);

/**
 * @brief Send an HTTP POST request
 *
 * Sends the provided payload as an HTTP POST request to the configured URL.
 * The function implements retry logic with exponential backoff and jitter
 * for transient failures (5xx, 408, 429, network errors). On success or
 * permanent failure (4xx), the function returns immediately.
 *
 * @param[in] client HTTP client handle
 * @param[in] payload Payload data to send
 * @param[in] payload_len Length of payload data in bytes
 * @param[out] out_status_code HTTP status code (0 if no response received)
 * @param[out] out_latency_ms Request latency in milliseconds
 * @return RS_RET_OK on success (2xx response), RS_RET_SUSPENDED for retryable
 *         errors (5xx, 408, 429, network errors), RS_RET_INTERNAL_ERROR for
 *         configuration errors, RS_RET_PARAM_ERROR for invalid parameters
 */
rsRetVal omotlp_http_client_post(omotlp_http_client_t *client,
                                 const uint8_t *payload,
                                 size_t payload_len,
                                 long *out_status_code,
                                 long *out_latency_ms);

#endif /* OMOTLP_HTTP_H */
