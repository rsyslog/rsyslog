/**
 * @file net_ossl_store.h
 * @brief Helper interfaces for loading TLS objects into the OpenSSL
 * netstream driver.
 *
 * The declarations in this header are private to the lmnsd_ossl runtime
 * module. They provide a single entry point for loading trust anchors,
 * certificates, and private keys from either traditional filesystem-backed
 * PEM files or provider-backed pkcs11: URIs.
 *
 * Copyright 2026 Cisco Systems, Inc., and/or its affiliates.
 * Copyright 2026 Adiscon GmbH.
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
#ifndef INCLUDED_NET_OSSL_STORE_H
#define INCLUDED_NET_OSSL_STORE_H

#include "rsyslog.h"
#include "net_ossl.h"

/**
 * @brief Load CA certificates into an SSL context.
 * @param ctx SSL context that receives the trust anchors.
 * @param caFile Path or pkcs11: URI describing the CA certificate source.
 * @return RS_RET_OK on success, or an rsyslog TLS certificate error code.
 */
rsRetVal net_ossl_load_ca_file(SSL_CTX *ctx, const char *caFile);

/**
 * @brief Load an end-entity certificate chain into an SSL context.
 * @param ctx SSL context that receives the certificate chain.
 * @param certFile Path or pkcs11: URI describing the certificate source.
 * @return RS_RET_OK on success, or an rsyslog TLS certificate error code.
 */
rsRetVal net_ossl_load_cert_file(SSL_CTX *ctx, const char *certFile);

/**
 * @brief Load a private key into an SSL context.
 * @param ctx SSL context that receives the private key.
 * @param keyFile Path or pkcs11: URI describing the private key source.
 * @return RS_RET_OK on success, or an rsyslog TLS key error code.
 */
rsRetVal net_ossl_load_key_file(SSL_CTX *ctx, const char *keyFile);

/**
 * @brief Redact sensitive fields from a TLS object locator before logging it.
 * @param input Original path or URI to sanitize.
 * @param sanitized Allocated sanitized copy when redaction is required, or
 * NULL when the original input is safe to log as-is.
 * @return Either @p input when no redaction was needed, an allocated sanitized
 * copy stored in @p sanitized, or a constant fallback redaction string when
 * memory allocation fails.
 */
const char *ATTR_NONNULL() net_ossl_sanitize_log_value(const char *input, char **sanitized);

/**
 * @brief Release a sanitized log value allocated by
 * net_ossl_sanitize_log_value().
 * @param sanitized Pointer to the allocated sanitized string, or NULL when no
 * allocation was needed.
 */
void net_ossl_free_sanitized_log_value(char **sanitized);

#endif /* INCLUDED_NET_OSSL_STORE_H */
