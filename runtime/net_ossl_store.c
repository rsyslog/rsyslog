/**
 * @file net_ossl_store.c
 * @brief TLS object loading helpers for the OpenSSL netstream driver.
 *
 * This module centralizes loading of CA certificates, certificate chains, and
 * private keys for lmnsd_ossl. On OpenSSL 3 builds it uses the provider/store
 * API when a strict pkcs11: URI is supplied, while preserving the traditional
 * filesystem-backed PEM loading path for ordinary filenames.
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
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net_ossl_store.h"
#include "debug.h"

#if OPENSSL_VERSION_NUMBER >= 0x30000000L && !defined(ENABLE_WOLFSSL) && !defined(LIBRESSL_VERSION_NUMBER)
    #include <openssl/store.h>
#endif

/**
 * @brief Return the delimiter that terminates a sensitive URI attribute.
 * @param c Character under inspection.
 * @return Non-zero when @p c ends the current attribute value.
 */
static int net_ossl_is_uri_value_delim(int c) {
    return c == '\0' || c == ';' || c == ',';
}

/**
 * @brief Redact a sensitive URI attribute in a caller-supplied buffer.
 * @param buf Mutable string to update in place.
 * @param key Attribute key to locate.
 */
static void ATTR_NONNULL() net_ossl_redact_uri_value(char *buf, const char *key) {
    char *pos;
    char *value;
    char *end;

    pos = buf;
    while ((pos = strstr(pos, key)) != NULL) {
        value = pos + strlen(key);
        end = value;
        while (!net_ossl_is_uri_value_delim((unsigned char)*end)) {
            ++end;
        }

        memset(value, '*', (size_t)(end - value));
        pos = end;
    }
}

/**
 * @brief Redact sensitive URI or provider configuration values before they are
 * emitted to logs.
 * @param input Original path or URI to sanitize.
 * @param buf Caller-supplied scratch buffer used when redaction is required.
 * @param bufSize Size of @p buf in bytes.
 * @return Either @p input when no redaction was required, or @p buf containing
 * a redacted copy.
 */
const char *ATTR_NONNULL() net_ossl_sanitize_log_value(const char *input, char **sanitized) {
    static const char redactedFallback[] = "[redacted]";

    *sanitized = NULL;

    if (strstr(input, "pin-value=") == NULL && strstr(input, "pkcs11-module-token-pin=") == NULL) {
        return input;
    }

    *sanitized = strdup(input);
    if (*sanitized == NULL) {
        return redactedFallback;
    }

    net_ossl_redact_uri_value(*sanitized, "pin-value=");
    net_ossl_redact_uri_value(*sanitized, "pkcs11-module-token-pin=");
    return *sanitized;
}

void net_ossl_free_sanitized_log_value(char **sanitized) {
    if (sanitized != NULL) {
        free(*sanitized);
        *sanitized = NULL;
    }
}

/**
 * @brief Log the most recent OpenSSL error stack entry for a failed load
 * operation.
 * @param errCode rsyslog error code associated with the failure.
 * @param call OpenSSL API name that failed.
 * @param objType Human-readable object type being loaded.
 * @param input Original path or URI that triggered the failure.
 */
static void ATTR_NONNULL()
    net_ossl_store_log_error_impl(rsRetVal errCode, const char *call, const char *objType, const char *input) {
    unsigned long err;
    char errbuf[256];
    char *sanitized = NULL;
    const char *logInput;

    logInput = net_ossl_sanitize_log_value(input, &sanitized);

    err = ERR_get_error();
    if (err == 0) {
        LogError(0, errCode, "%s: %s load failed for '%s'. OpenSSL Error Stack: no OpenSSL error on stack", call,
                 objType, logInput);
        dbgprintf("%s: %s load failed for '%s'. OpenSSL Error Stack: no OpenSSL error on stack\n", call, objType,
                  logInput);
        net_ossl_free_sanitized_log_value(&sanitized);
        return;
    }

    while (err != 0) {
        ERR_error_string_n(err, errbuf, sizeof(errbuf));
        LogError(0, errCode, "%s: %s load failed for '%s'. OpenSSL Error Stack: err=0x%lx lib=%d reason=%d msg='%s'",
                 call, objType, logInput, err, ERR_GET_LIB(err), ERR_GET_REASON(err), errbuf);
        dbgprintf("%s: %s load failed for '%s'. OpenSSL Error Stack: err=0x%lx lib=%d reason=%d msg='%s'\n", call,
                  objType, logInput, err, ERR_GET_LIB(err), ERR_GET_REASON(err), errbuf);
        err = ERR_get_error();
    }
    net_ossl_free_sanitized_log_value(&sanitized);
}

/**
 * @brief Log the current OpenSSL error stack and return the supplied rsyslog
 * error code.
 * @param errCode rsyslog error code to return to the caller.
 * @param call OpenSSL API name that failed.
 * @param objType Human-readable object type being loaded.
 * @param input Original path or URI that triggered the failure.
 * @return The @p errCode argument.
 */
static rsRetVal ATTR_NONNULL()
    net_ossl_store_log_error(rsRetVal errCode, const char *call, const char *objType, const char *input) {
    net_ossl_store_log_error_impl(errCode, call, objType, input);
    return errCode;
}

#if OPENSSL_VERSION_NUMBER >= 0x30000000L && !defined(ENABLE_WOLFSSL) && !defined(LIBRESSL_VERSION_NUMBER)
/**
 * @brief Determine whether a configuration value should be treated as a PKCS#11
 * URI.
 * @param value Configuration value to inspect.
 * @return Non-zero when @p value begins with the strict @c pkcs11: prefix.
 */
static int net_ossl_is_uri(const char *value) {
    return value != NULL && strncmp(value, "pkcs11:", sizeof("pkcs11:") - 1) == 0;
}

/**
 * @brief Open an OpenSSL store loader for a URI-backed object source.
 * @param errCode rsyslog error code to report if opening the store fails.
 * @param uri Object locator passed to OSSL_STORE.
 * @param expect Expected OSSL_STORE object type.
 * @param store Receives the opened store context on success.
 * @return RS_RET_OK on success, or the caller-supplied rsyslog error code.
 */
static rsRetVal ATTR_NONNULL(2, 4)
    net_ossl_store_open(rsRetVal errCode, const char *uri, int expect, OSSL_STORE_CTX **store) {
    DEFiRet;

    assert(store != NULL);
    *store = OSSL_STORE_open_ex(uri, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    if (*store == NULL) {
        ABORT_FINALIZE(net_ossl_store_log_error(errCode, "OSSL_STORE_open_ex", "store", uri));
    }
    if (OSSL_STORE_expect(*store, expect) != 1) {
        /* Preference only; keep going if the loader ignores it. */
        net_ossl_store_log_error_impl(RS_RET_OK, "OSSL_STORE_expect", "store", uri);
        ERR_clear_error();
    }

finalize_it:
    RETiRet;
}

/**
 * @brief Load CA certificates from an OpenSSL store source into an SSL
 * context.
 * @param ctx SSL context that receives the trust anchors.
 * @param caFile PKCS#11 URI to open through OSSL_STORE.
 * @return RS_RET_OK on success, or an rsyslog TLS certificate error code.
 */
static rsRetVal ATTR_NONNULL() net_ossl_store_load_ca_file(SSL_CTX *ctx, const char *caFile) {
    DEFiRet;
    OSSL_STORE_CTX *store = NULL;
    OSSL_STORE_INFO *info = NULL;
    X509_STORE *x509_store;
    int loaded = 0;

    assert(ctx != NULL);
    assert(caFile != NULL);

    x509_store = SSL_CTX_get_cert_store(ctx);
    if (x509_store == NULL) {
        char *sanitized = NULL;
        const char *logCaFile = net_ossl_sanitize_log_value(caFile, &sanitized);
        LogError(0, RS_RET_TLS_CERT_ERR, "SSL_CTX_get_cert_store: CA load failed for '%s': no X509 store on SSL_CTX",
                 logCaFile);
        dbgprintf("SSL_CTX_get_cert_store: CA load failed for '%s': no X509 store on SSL_CTX\n", logCaFile);
        net_ossl_free_sanitized_log_value(&sanitized);
        ABORT_FINALIZE(RS_RET_TLS_CERT_ERR);
    }

    CHKiRet(net_ossl_store_open(RS_RET_TLS_CERT_ERR, caFile, OSSL_STORE_INFO_CERT, &store));

    while ((info = OSSL_STORE_load(store)) != NULL) {
        if (OSSL_STORE_INFO_get_type(info) == OSSL_STORE_INFO_CERT) {
            X509 *cert = OSSL_STORE_INFO_get1_CERT(info);
            if (cert != NULL) {
                if (X509_STORE_add_cert(x509_store, cert) == 1) {
                    loaded++;
                } else {
                    unsigned long err = ERR_peek_last_error();
                    if (ERR_GET_LIB(err) == ERR_LIB_X509 && ERR_GET_REASON(err) == X509_R_CERT_ALREADY_IN_HASH_TABLE) {
                        ERR_clear_error();
                        loaded++;
                    } else {
                        ABORT_FINALIZE(
                            net_ossl_store_log_error(RS_RET_TLS_CERT_ERR, "X509_STORE_add_cert", "CA", caFile));
                    }
                }
                X509_free(cert);
            }
        }
        OSSL_STORE_INFO_free(info);
        info = NULL;
    }

    if (OSSL_STORE_error(store)) {
        ABORT_FINALIZE(net_ossl_store_log_error(RS_RET_TLS_CERT_ERR, "OSSL_STORE_load", "CA", caFile));
    }
    if (loaded == 0) {
        char *sanitized = NULL;
        const char *logCaFile = net_ossl_sanitize_log_value(caFile, &sanitized);
        LogError(0, RS_RET_TLS_CERT_ERR, "OSSL store CA load failed for '%s': no certificate objects were loaded",
                 logCaFile);
        dbgprintf("OSSL store CA load failed for '%s': no certificate objects were loaded\n", logCaFile);
        net_ossl_free_sanitized_log_value(&sanitized);
        ABORT_FINALIZE(RS_RET_TLS_CERT_ERR);
    }

finalize_it:
    if (info != NULL) {
        OSSL_STORE_INFO_free(info);
    }
    if (store != NULL) {
        OSSL_STORE_close(store);
    }
    RETiRet;
}

/**
 * @brief Load a certificate chain from an OpenSSL store source into an SSL
 * context.
 * @param ctx SSL context that receives the certificate chain.
 * @param certFile PKCS#11 URI to open through OSSL_STORE.
 * @return RS_RET_OK on success, or an rsyslog TLS certificate error code.
 */
static int ATTR_NONNULL() net_ossl_store_cert_issues_other(X509 *candidate, const STACK_OF(X509) * certs) {
    int i;

    assert(candidate != NULL);
    assert(certs != NULL);

    for (i = 0; i < sk_X509_num(certs); ++i) {
        X509 *other = sk_X509_value(certs, i);
        if (other != NULL && other != candidate && X509_check_issued(candidate, other) == X509_V_OK) {
            return 1;
        }
    }

    return 0;
}

static rsRetVal ATTR_NONNULL(1, 2, 3)
    net_ossl_store_select_leaf_cert(STACK_OF(X509) * certs, const char *certFile, X509 **leaf) {
    DEFiRet;
    int i;
    X509 *fallback = NULL;
    int fallbackCount = 0;
    X509 *preferred = NULL;
    int preferredCount = 0;

    assert(certs != NULL);
    assert(certFile != NULL);
    assert(leaf != NULL);

    *leaf = NULL;

    for (i = 0; i < sk_X509_num(certs); ++i) {
        X509 *cert = sk_X509_value(certs, i);
        if (cert == NULL || net_ossl_store_cert_issues_other(cert, certs)) {
            continue;
        }

        fallback = cert;
        ++fallbackCount;
        if (X509_check_ca(cert) == 0) {
            preferred = cert;
            ++preferredCount;
        }
    }

    if (preferredCount == 1) {
        *leaf = preferred;
    } else if (preferredCount > 1) {
        char *sanitized = NULL;
        const char *logCertFile = net_ossl_sanitize_log_value(certFile, &sanitized);
        LogError(0, RS_RET_TLS_CERT_ERR,
                 "OSSL store certificate load failed for '%s': multiple end-entity certificates were loaded",
                 logCertFile);
        dbgprintf("OSSL store certificate load failed for '%s': multiple end-entity certificates were loaded\n",
                  logCertFile);
        net_ossl_free_sanitized_log_value(&sanitized);
        ABORT_FINALIZE(RS_RET_TLS_CERT_ERR);
    } else if (fallbackCount == 1) {
        *leaf = fallback;
    } else if (fallbackCount > 1) {
        char *sanitized = NULL;
        const char *logCertFile = net_ossl_sanitize_log_value(certFile, &sanitized);
        LogError(0, RS_RET_TLS_CERT_ERR,
                 "OSSL store certificate load failed for '%s': unable to identify a unique leaf certificate",
                 logCertFile);
        dbgprintf("OSSL store certificate load failed for '%s': unable to identify a unique leaf certificate\n",
                  logCertFile);
        net_ossl_free_sanitized_log_value(&sanitized);
        ABORT_FINALIZE(RS_RET_TLS_CERT_ERR);
    }

finalize_it:
    RETiRet;
}

static rsRetVal ATTR_NONNULL() net_ossl_store_load_cert_file(SSL_CTX *ctx, const char *certFile) {
    DEFiRet;
    OSSL_STORE_CTX *store = NULL;
    OSSL_STORE_INFO *info = NULL;
    STACK_OF(X509) *certs = NULL;
    X509 *leaf = NULL;
    int i;

    assert(ctx != NULL);
    assert(certFile != NULL);

    CHKiRet(net_ossl_store_open(RS_RET_TLS_CERT_ERR, certFile, OSSL_STORE_INFO_CERT, &store));
    certs = sk_X509_new_null();
    if (certs == NULL) {
        ABORT_FINALIZE(net_ossl_store_log_error(RS_RET_TLS_CERT_ERR, "sk_X509_new_null", "certificate", certFile));
    }

    while ((info = OSSL_STORE_load(store)) != NULL) {
        if (OSSL_STORE_INFO_get_type(info) == OSSL_STORE_INFO_CERT) {
            X509 *cert = OSSL_STORE_INFO_get1_CERT(info);
            if (cert != NULL && !sk_X509_push(certs, cert)) {
                X509_free(cert);
                ABORT_FINALIZE(net_ossl_store_log_error(RS_RET_TLS_CERT_ERR, "sk_X509_push", "certificate", certFile));
            }
        }
        OSSL_STORE_INFO_free(info);
        info = NULL;
    }

    if (OSSL_STORE_error(store)) {
        ABORT_FINALIZE(net_ossl_store_log_error(RS_RET_TLS_CERT_ERR, "OSSL_STORE_load", "certificate", certFile));
    }

    CHKiRet(net_ossl_store_select_leaf_cert(certs, certFile, &leaf));
    if (leaf == NULL) {
        char *sanitized = NULL;
        const char *logCertFile = net_ossl_sanitize_log_value(certFile, &sanitized);
        LogError(0, RS_RET_TLS_CERT_ERR,
                 "OSSL store certificate load failed for '%s': no leaf certificate object was loaded", logCertFile);
        dbgprintf("OSSL store certificate load failed for '%s': no leaf certificate object was loaded\n", logCertFile);
        net_ossl_free_sanitized_log_value(&sanitized);
        ABORT_FINALIZE(RS_RET_TLS_CERT_ERR);
    }

    for (i = 0; i < sk_X509_num(certs); ++i) {
        X509 *cert = sk_X509_value(certs, i);
        if (cert != NULL && cert != leaf) {
            if (SSL_CTX_add_extra_chain_cert(ctx, cert) != 1) {
                X509_free(cert);
                sk_X509_set(certs, i, NULL);
                ABORT_FINALIZE(net_ossl_store_log_error(RS_RET_TLS_CERT_ERR, "SSL_CTX_add_extra_chain_cert",
                                                        "certificate", certFile));
            }
            sk_X509_set(certs, i, NULL);
        }
    }

    if (SSL_CTX_use_certificate(ctx, leaf) != 1) {
        net_ossl_store_log_error_impl(RS_RET_TLS_CERT_ERR, "SSL_CTX_use_certificate", "certificate", certFile);
        ABORT_FINALIZE(RS_RET_TLS_CERT_ERR);
    }

finalize_it:
    if (certs != NULL) {
        sk_X509_pop_free(certs, X509_free);
    }
    if (info != NULL) {
        OSSL_STORE_INFO_free(info);
    }
    if (store != NULL) {
        OSSL_STORE_close(store);
    }
    RETiRet;
}

/**
 * @brief Load a private key from an OpenSSL store source into an SSL context.
 * @param ctx SSL context that receives the private key.
 * @param keyFile PKCS#11 URI to open through OSSL_STORE.
 * @return RS_RET_OK on success, or an rsyslog TLS key error code.
 */
static rsRetVal ATTR_NONNULL() net_ossl_store_load_key_file(SSL_CTX *ctx, const char *keyFile) {
    DEFiRet;
    OSSL_STORE_CTX *store = NULL;
    OSSL_STORE_INFO *info = NULL;
    EVP_PKEY *pkey = NULL;

    assert(ctx != NULL);
    assert(keyFile != NULL);

    CHKiRet(net_ossl_store_open(RS_RET_TLS_KEY_ERR, keyFile, OSSL_STORE_INFO_PKEY, &store));

    while ((info = OSSL_STORE_load(store)) != NULL) {
        if (OSSL_STORE_INFO_get_type(info) == OSSL_STORE_INFO_PKEY) {
            pkey = OSSL_STORE_INFO_get1_PKEY(info);
            OSSL_STORE_INFO_free(info);
            info = NULL;
            break;
        }
        OSSL_STORE_INFO_free(info);
        info = NULL;
    }

    if (OSSL_STORE_error(store)) {
        ABORT_FINALIZE(net_ossl_store_log_error(RS_RET_TLS_KEY_ERR, "OSSL_STORE_load", "private key", keyFile));
    }
    if (pkey == NULL || SSL_CTX_use_PrivateKey(ctx, pkey) != 1) {
        if (pkey == NULL) {
            char *sanitized = NULL;
            const char *logKeyFile = net_ossl_sanitize_log_value(keyFile, &sanitized);
            LogError(0, RS_RET_TLS_KEY_ERR, "OSSL store key load failed for '%s': no private key object was loaded",
                     logKeyFile);
            dbgprintf("OSSL store key load failed for '%s': no private key object was loaded\n", logKeyFile);
            net_ossl_free_sanitized_log_value(&sanitized);
        } else {
            net_ossl_store_log_error_impl(RS_RET_TLS_KEY_ERR, "SSL_CTX_use_PrivateKey", "private key", keyFile);
        }
        ABORT_FINALIZE(RS_RET_TLS_KEY_ERR);
    }

finalize_it:
    if (pkey != NULL) {
        EVP_PKEY_free(pkey);
    }
    if (info != NULL) {
        OSSL_STORE_INFO_free(info);
    }
    if (store != NULL) {
        OSSL_STORE_close(store);
    }
    RETiRet;
}
#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L && !defined(ENABLE_WOLFSSL) && !defined(LIBRESSL_VERSION_NUMBER) */

/**
 * @brief Load CA certificates into an SSL context from a file path or PKCS#11
 * URI.
 * @param ctx SSL context that receives the trust anchors.
 * @param caFile Path or pkcs11: URI describing the CA certificate source.
 * @return RS_RET_OK on success, or an rsyslog TLS certificate error code.
 */
rsRetVal net_ossl_load_ca_file(SSL_CTX *ctx, const char *caFile) {
    DEFiRet;

    assert(ctx != NULL);
    assert(caFile != NULL);

#if OPENSSL_VERSION_NUMBER >= 0x30000000L && !defined(ENABLE_WOLFSSL) && !defined(LIBRESSL_VERSION_NUMBER)
    if (net_ossl_is_uri(caFile)) {
        CHKiRet(net_ossl_store_load_ca_file(ctx, caFile));
        FINALIZE;
    }
#endif

    if (SSL_CTX_load_verify_locations(ctx, caFile, NULL) != 1) {
        ABORT_FINALIZE(net_ossl_store_log_error(RS_RET_TLS_CERT_ERR, "SSL_CTX_load_verify_locations", "CA", caFile));
    }

finalize_it:
    RETiRet;
}

/**
 * @brief Load a certificate chain into an SSL context from a file path or
 * PKCS#11 URI.
 * @param ctx SSL context that receives the certificate chain.
 * @param certFile Path or pkcs11: URI describing the certificate source.
 * @return RS_RET_OK on success, or an rsyslog TLS certificate error code.
 */
rsRetVal net_ossl_load_cert_file(SSL_CTX *ctx, const char *certFile) {
    DEFiRet;

    assert(ctx != NULL);
    assert(certFile != NULL);

#if OPENSSL_VERSION_NUMBER >= 0x30000000L && !defined(ENABLE_WOLFSSL) && !defined(LIBRESSL_VERSION_NUMBER)
    if (net_ossl_is_uri(certFile)) {
        CHKiRet(net_ossl_store_load_cert_file(ctx, certFile));
        FINALIZE;
    }
#endif

    if (SSL_CTX_use_certificate_chain_file(ctx, certFile) != 1) {
        ABORT_FINALIZE(net_ossl_store_log_error(RS_RET_TLS_CERT_ERR, "SSL_CTX_use_certificate_chain_file",
                                                "certificate", certFile));
    }

finalize_it:
    RETiRet;
}

/**
 * @brief Load a private key into an SSL context from a file path or PKCS#11
 * URI.
 * @param ctx SSL context that receives the private key.
 * @param keyFile Path or pkcs11: URI describing the private key source.
 * @return RS_RET_OK on success, or an rsyslog TLS key error code.
 */
rsRetVal net_ossl_load_key_file(SSL_CTX *ctx, const char *keyFile) {
    DEFiRet;

    assert(ctx != NULL);
    assert(keyFile != NULL);

#if OPENSSL_VERSION_NUMBER >= 0x30000000L && !defined(ENABLE_WOLFSSL) && !defined(LIBRESSL_VERSION_NUMBER)
    if (net_ossl_is_uri(keyFile)) {
        CHKiRet(net_ossl_store_load_key_file(ctx, keyFile));
        FINALIZE;
    }
#endif

    if (SSL_CTX_use_PrivateKey_file(ctx, keyFile, SSL_FILETYPE_PEM) != 1) {
        ABORT_FINALIZE(
            net_ossl_store_log_error(RS_RET_TLS_KEY_ERR, "SSL_CTX_use_PrivateKey_file", "private key", keyFile));
    }

finalize_it:
    RETiRet;
}
