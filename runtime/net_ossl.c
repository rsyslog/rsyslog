/* net.c
 * Implementation of network-related stuff.
 *
 * File begun on 2023-08-29 by Alorbach (extracted from net.c)
 *
 * Copyright 2023 Andre Lorbach and Adiscon GmbH.
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
#include <assert.h>
#include <string.h>
#include <strings.h>

#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/time.h>

#include "rsyslog.h"
#include "syslogd-types.h"
#include "module-template.h"
#include "parse.h"
#include "srUtils.h"
#include "obj.h"
#include "errmsg.h"
#include "net.h"
#include "net_ossl.h"
#include "nsd_ptcp.h"
#include "rsconf.h"

#define OCSP_TIMEOUT 5
#define OCSP_CACHE_MAX_ENTRIES 100
#define OCSP_CACHE_DEFAULT_TTL 3600

/* static data */
DEFobjStaticHelpers;
DEFobjCurrIf(glbl) DEFobjCurrIf(net) DEFobjCurrIf(nsd_ptcp)

    /* Prototypes for openssl helper functions */
    void net_ossl_lastOpenSSLErrorMsg(
        uchar *fromHost, int ret, SSL *ssl, int severity, const char *pszCallSource, const char *pszOsslApi);
void net_ossl_set_ssl_verify_callback(SSL *pSsl, int flags);
void net_ossl_set_ctx_verify_callback(SSL_CTX *pCtx, int flags);
void net_ossl_set_bio_callback(BIO *conn);
int net_ossl_verify_callback(int status, X509_STORE_CTX *store);
#if OPENSSL_VERSION_NUMBER >= 0x10002000L && !defined(LIBRESSL_VERSION_NUMBER) && !defined(ENABLE_WOLFSSL)
rsRetVal net_ossl_apply_tlscgfcmd(net_ossl_t *pThis, uchar *tlscfgcmd);
#endif  // OPENSSL_VERSION_NUMBER >= 0x10002000L
rsRetVal net_ossl_chkpeercertvalidity(net_ossl_t *pThis, SSL *ssl, uchar *fromHostIP);
X509 *net_ossl_getpeercert(net_ossl_t *pThis, SSL *ssl, uchar *fromHostIP);
rsRetVal net_ossl_peerfingerprint(net_ossl_t *pThis, X509 *certpeer, uchar *fromHostIP);
#ifndef ENABLE_WOLFSSL
void ocsp_cache_cleanup(void);
#endif
rsRetVal net_ossl_chkpeername(net_ossl_t *pThis, X509 *certpeer, uchar *fromHostIP);


/*--------------------------------------MT OpenSSL helpers ------------------------------------------*/
#ifndef ENABLE_WOLFSSL
static MUTEX_TYPE *mutex_buf = NULL;
static sbool openssl_initialized = 0;  // Avoid multiple initialization / deinitialization

void locking_function(int mode, int n, __attribute__((unused)) const char *file, __attribute__((unused)) int line) {
    if (mode & CRYPTO_LOCK)
        MUTEX_LOCK(mutex_buf[n]);
    else
        MUTEX_UNLOCK(mutex_buf[n]);
}

unsigned long id_function(void) {
    return ((unsigned long)THREAD_ID);
}


struct CRYPTO_dynlock_value *dyn_create_function(__attribute__((unused)) const char *file,
                                                 __attribute__((unused)) int line) {
    struct CRYPTO_dynlock_value *value;
    value = (struct CRYPTO_dynlock_value *)malloc(sizeof(struct CRYPTO_dynlock_value));
    if (!value) return NULL;

    MUTEX_SETUP(value->mutex);
    return value;
}

void dyn_lock_function(int mode,
                       struct CRYPTO_dynlock_value *l,
                       __attribute__((unused)) const char *file,
                       __attribute__((unused)) int line) {
    if (mode & CRYPTO_LOCK)
        MUTEX_LOCK(l->mutex);
    else
        MUTEX_UNLOCK(l->mutex);
}

void dyn_destroy_function(struct CRYPTO_dynlock_value *l,
                          __attribute__((unused)) const char *file,
                          __attribute__((unused)) int line) {
    MUTEX_CLEANUP(l->mutex);
    free(l);
}

/* set up support functions for openssl multi-threading. This must
 * be done at library initialisation. If the function fails,
 * processing can not continue normally. On failure, 0 is
 * returned, on success 1.
 */
int opensslh_THREAD_setup(void) {
    int i;
    if (openssl_initialized == 1) {
        DBGPRINTF("openssl: multithread setup already initialized\n");
        return 1;
    }

    mutex_buf = (MUTEX_TYPE *)malloc(CRYPTO_num_locks() * sizeof(MUTEX_TYPE));
    if (mutex_buf == NULL) return 0;
    for (i = 0; i < CRYPTO_num_locks(); i++) MUTEX_SETUP(mutex_buf[i]);

    #if OPENSSL_VERSION_NUMBER < 0x10100000L
    CRYPTO_set_id_callback(id_function);
    #endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */
    CRYPTO_set_locking_callback(locking_function);
    /* The following three CRYPTO_... functions are the OpenSSL functions
    for registering the callbacks we implemented above */
    CRYPTO_set_dynlock_create_callback(dyn_create_function);
    CRYPTO_set_dynlock_lock_callback(dyn_lock_function);
    CRYPTO_set_dynlock_destroy_callback(dyn_destroy_function);

    DBGPRINTF("openssl: multithread setup finished\n");
    openssl_initialized = 1;
    return 1;
}

/* shut down openssl - do this only when you are totally done
 * with openssl.
 */
int opensslh_THREAD_cleanup(void) {
    int i;
    if (openssl_initialized == 0) {
        DBGPRINTF("openssl: multithread cleanup already done\n");
        return 1;
    }
    if (!mutex_buf) return 0;

    #if OPENSSL_VERSION_NUMBER < 0x10100000L
    CRYPTO_set_id_callback(NULL);
    #endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */
    CRYPTO_set_locking_callback(NULL);
    CRYPTO_set_dynlock_create_callback(NULL);
    CRYPTO_set_dynlock_lock_callback(NULL);
    CRYPTO_set_dynlock_destroy_callback(NULL);

    for (i = 0; i < CRYPTO_num_locks(); i++) MUTEX_CLEANUP(mutex_buf[i]);

    free(mutex_buf);
    mutex_buf = NULL;

    DBGPRINTF("openssl: multithread cleanup finished\n");
    openssl_initialized = 0;
    return 1;
}
#else
int opensslh_THREAD_setup(void) {
    return 1;
}

int opensslh_THREAD_cleanup(void) {
    return 1;
}
#endif /* !ENABLE_WOLFSSL */
/*-------------------------------------- MT OpenSSL helpers -----------------------------------------*/


/*--------------------------------------OpenSSL helpers ------------------------------------------*/

/* globally initialize OpenSSL
 */
void osslGlblInit(void) {
    DBGPRINTF("osslGlblInit: ENTER\n");

#ifdef ENABLE_WOLFSSL
    if (!SSL_library_init()) {
        LogError(0, RS_RET_NO_ERRCODE, "Error: OpenSSL initialization failed!");
    }
#else
    if ((opensslh_THREAD_setup() == 0) ||
    #if OPENSSL_VERSION_NUMBER < 0x10100000L
        /* Setup OpenSSL library  < 1.1.0 */
        !SSL_library_init()
    #else
        /* Setup OpenSSL library >= 1.1.0 with system default settings */
        OPENSSL_init_ssl(0, NULL) == 0
    #endif
    ) {
        LogError(0, RS_RET_NO_ERRCODE, "Error: OpenSSL initialization failed!");
    }
#endif

#if defined(ENABLE_WOLFSSL) && defined(DEBUG_WOLFSSL)
    wolfSSL_Debugging_ON();
#endif

    /* Load readable error strings */
    SSL_load_error_strings();
#if OPENSSL_VERSION_NUMBER >= 0x30000000L && !defined(LIBRESSL_VERSION_NUMBER)
    /*
     * ERR_load_*(), ERR_func_error_string(), ERR_get_error_line(), ERR_get_error_line_data(), ERR_get_state()
     * OpenSSL now loads error strings automatically so these functions are not needed.
     * SEE FOR MORE:
     *	https://www.openssl.org/docs/manmaster/man7/migration_guide.html
     *
     */
#else
    /* Load error strings into mem*/
    ERR_load_BIO_strings();
    ERR_load_crypto_strings();
#endif

    PRAGMA_DIAGNOSTIC_PUSH
    PRAGMA_IGNORE_Wdeprecated_declarations

#ifndef OPENSSL_NO_ENGINE
    // Initialize OpenSSL engine library
    ENGINE_load_builtin_engines();
    /* Register all of them for every algorithm they collectively implement */
    ENGINE_register_all_complete();

    // Iterate through all available engines
    ENGINE *osslEngine = ENGINE_get_first();
    const char *engine_id = NULL;
    const char *engine_name = NULL;
    while (osslEngine) {
        // Print engine ID and name if the engine is loaded
        if (ENGINE_get_init_function(osslEngine)) {  // Check if engine is initialized
            engine_id = ENGINE_get_id(osslEngine);
            engine_name = ENGINE_get_name(osslEngine);
            DBGPRINTF("osslGlblInit: Loaded Engine: ID = %s, Name = %s\n", engine_id, engine_name);
        }
        osslEngine = ENGINE_get_next(osslEngine);
    }
    // Free the engine reference when done
    ENGINE_free(osslEngine);
#else
        DBGPRINTF("osslGlblInit: OpenSSL compiled without ENGINE support - ENGINE support disabled\n");
#endif /* OPENSSL_NO_ENGINE */

    PRAGMA_DIAGNOSTIC_POP
}

/* globally de-initialize OpenSSL */
void osslGlblExit(void) {
    DBGPRINTF("openssl: entering osslGlblExit\n");
#ifndef ENABLE_WOLFSSL
    ocsp_cache_cleanup();
#endif
#ifndef OPENSSL_NO_ENGINE
    ENGINE_cleanup();
#endif
    ERR_free_strings();
    EVP_cleanup();
    CRYPTO_cleanup_all_ex_data();
}


/* initialize openssl context; called on
 * - listener creation
 * - outbound connection creation
 * Once created, the ctx object is used by-subobjects (accepted inbound connections)
 */
static rsRetVal net_ossl_osslCtxInit(net_ossl_t *pThis, const SSL_METHOD *method) {
    DEFiRet;
    int bHaveCA;
    int bHaveCRL;
    int bHaveCert;
    int bHaveKey;
    int bHaveExtraCAFiles;
    const char *caFile, *crlFile, *certFile, *keyFile;
    char *extraCaFiles, *extraCaFile;
    /* Setup certificates */
    caFile = (char *)((pThis->pszCAFile == NULL) ? glbl.GetDfltNetstrmDrvrCAF(runConf) : pThis->pszCAFile);
    if (caFile == NULL) {
        LogMsg(0, RS_RET_CA_CERT_MISSING, LOG_WARNING, "Warning: CA certificate is not set");
        bHaveCA = 0;
    } else {
        dbgprintf("osslCtxInit: OSSL CA file: '%s'\n", caFile);
        bHaveCA = 1;
    }
    crlFile = (char *)((pThis->pszCRLFile == NULL) ? glbl.GetDfltNetstrmDrvrCRLF(runConf) : pThis->pszCRLFile);
    if (crlFile == NULL) {
        bHaveCRL = 0;
    } else {
        dbgprintf("osslCtxInit: OSSL CRL file: '%s'\n", crlFile);
        bHaveCRL = 1;
    }
    certFile = (char *)((pThis->pszCertFile == NULL) ? glbl.GetDfltNetstrmDrvrCertFile(runConf) : pThis->pszCertFile);
    if (certFile == NULL) {
        LogMsg(0, RS_RET_CERT_MISSING, LOG_WARNING, "Warning: Certificate file is not set");
        bHaveCert = 0;
    } else {
        dbgprintf("osslCtxInit: OSSL CERT file: '%s'\n", certFile);
        bHaveCert = 1;
    }
    keyFile = (char *)((pThis->pszKeyFile == NULL) ? glbl.GetDfltNetstrmDrvrKeyFile(runConf) : pThis->pszKeyFile);
    if (keyFile == NULL) {
        LogMsg(0, RS_RET_CERTKEY_MISSING, LOG_WARNING, "Warning: Key file is not set");
        bHaveKey = 0;
    } else {
        dbgprintf("osslCtxInit: OSSL KEY file: '%s'\n", keyFile);
        bHaveKey = 1;
    }
    extraCaFiles =
        (char *)((pThis->pszExtraCAFiles == NULL) ? glbl.GetNetstrmDrvrCAExtraFiles(runConf) : pThis->pszExtraCAFiles);
    if (extraCaFiles == NULL) {
        bHaveExtraCAFiles = 0;
    } else {
        dbgprintf("osslCtxInit: OSSL EXTRA CA files: '%s'\n", extraCaFiles);
        bHaveExtraCAFiles = 1;
    }

    /* Create main CTX Object based on method parameter */
    if (pThis->ctx != NULL) {
        SSL_CTX_free(pThis->ctx);
        pThis->ctx = NULL;
    }
    pThis->ctx = SSL_CTX_new(method);

    if (bHaveExtraCAFiles == 1) {
        while ((extraCaFile = strsep(&extraCaFiles, ","))) {
            if (SSL_CTX_load_verify_locations(pThis->ctx, extraCaFile, NULL) != 1) {
                LogError(0, RS_RET_TLS_CERT_ERR,
                         "Error: Extra Certificate file could not be accessed. "
                         "Check at least: 1) file path is correct, 2) file exist, "
                         "3) permissions are correct, 4) file content is correct. "
                         "OpenSSL error info may follow in next messages");
                net_ossl_lastOpenSSLErrorMsg(NULL, 0, NULL, LOG_ERR, "osslCtxInit", "SSL_CTX_load_verify_locations");
                ABORT_FINALIZE(RS_RET_TLS_CERT_ERR);
            }
        }
    }
    if (bHaveCA == 1 && SSL_CTX_load_verify_locations(pThis->ctx, caFile, NULL) != 1) {
        LogError(0, RS_RET_TLS_CERT_ERR,
                 "Error: CA certificate could not be accessed. "
                 "Check at least: 1) file path is correct, 2) file exist, "
                 "3) permissions are correct, 4) file content is correct. "
                 "OpenSSL error info may follow in next messages");
        net_ossl_lastOpenSSLErrorMsg(NULL, 0, NULL, LOG_ERR, "osslCtxInit", "SSL_CTX_load_verify_locations");
        ABORT_FINALIZE(RS_RET_TLS_CERT_ERR);
    }
    if (bHaveCRL == 1) {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L && !defined(LIBRESSL_VERSION_NUMBER)
        // Get X509_STORE reference
        X509_STORE *store = SSL_CTX_get_cert_store(pThis->ctx);
        if (!X509_STORE_load_file(store, crlFile)) {
            LogError(0, RS_RET_CRL_INVALID,
                     "Error: CRL could not be accessed. "
                     "Check at least: 1) file path is correct, 2) file exist, "
                     "3) permissions are correct, 4) file content is correct. "
                     "OpenSSL error info may follow in next messages");
            net_ossl_lastOpenSSLErrorMsg(NULL, 0, NULL, LOG_ERR, "osslCtxInit", "X509_STORE_load_file");
            ABORT_FINALIZE(RS_RET_CRL_INVALID);
        }
        X509_STORE_set_flags(store, X509_V_FLAG_CRL_CHECK);
#else
    #if OPENSSL_VERSION_NUMBER >= 0x10002000L
        // Get X509_STORE reference
        X509_STORE *store = SSL_CTX_get_cert_store(pThis->ctx);
        // Load the CRL PEM file
        FILE *fp = fopen(crlFile, "r");
        if (fp == NULL) {
            LogError(0, RS_RET_CRL_MISSING,
                     "Error: CRL could not be accessed. "
                     "Check at least: 1) file path is correct, 2) file exist, "
                     "3) permissions are correct, 4) file content is correct. "
                     "OpenSSL error info may follow in next messages");
            net_ossl_lastOpenSSLErrorMsg(NULL, 0, NULL, LOG_ERR, "osslCtxInit", "fopen");
            ABORT_FINALIZE(RS_RET_CRL_MISSING);
        }
        X509_CRL *crl = PEM_read_X509_CRL(fp, NULL, NULL, NULL);
        fclose(fp);
        if (crl == NULL) {
            LogError(0, RS_RET_CRL_INVALID,
                     "Error: Unable to read CRL."
                     "OpenSSL error info may follow in next messages");
            net_ossl_lastOpenSSLErrorMsg(NULL, 0, NULL, LOG_ERR, "osslCtxInit", "PEM_read_X509_CRL");
            ABORT_FINALIZE(RS_RET_CRL_INVALID);
        }
        // Add the CRL to the X509_STORE
        if (!X509_STORE_add_crl(store, crl)) {
            LogError(0, RS_RET_CRL_INVALID,
                     "Error: Unable to add CRL to store."
                     "OpenSSL error info may follow in next messages");
            net_ossl_lastOpenSSLErrorMsg(NULL, 0, NULL, LOG_ERR, "osslCtxInit", "X509_STORE_add_crl");
            X509_CRL_free(crl);
            ABORT_FINALIZE(RS_RET_CRL_INVALID);
        }
        // Set the X509_STORE to the SSL_CTX
        // SSL_CTX_set_cert_store(pThis->ctx, store);
        // Enable CRL checking
        X509_VERIFY_PARAM *param = X509_VERIFY_PARAM_new();
        X509_VERIFY_PARAM_set_flags(param, X509_V_FLAG_CRL_CHECK);
        SSL_CTX_set1_param(pThis->ctx, param);
        X509_VERIFY_PARAM_free(param);
    #else
        LogError(0, RS_RET_SYS_ERR,
                 "Warning: TLS library does not support X509_STORE_load_file"
                 "(requires OpenSSL 3.x or higher). Cannot use Certificate revocation list (CRL) '%s'.",
                 crlFile);
    #endif
#endif
    }
    if (bHaveCert == 1 && SSL_CTX_use_certificate_chain_file(pThis->ctx, certFile) != 1) {
        LogError(0, RS_RET_TLS_CERT_ERR,
                 "Error: Certificate file could not be accessed. "
                 "Check at least: 1) file path is correct, 2) file exist, "
                 "3) permissions are correct, 4) file content is correct. "
                 "OpenSSL error info may follow in next messages");
        net_ossl_lastOpenSSLErrorMsg(NULL, 0, NULL, LOG_ERR, "osslCtxInit", "SSL_CTX_use_certificate_chain_file");
        ABORT_FINALIZE(RS_RET_TLS_CERT_ERR);
    }
    if (bHaveKey == 1 && SSL_CTX_use_PrivateKey_file(pThis->ctx, keyFile, SSL_FILETYPE_PEM) != 1) {
        LogError(0, RS_RET_TLS_KEY_ERR,
                 "Error: Key could not be accessed. "
                 "Check at least: 1) file path is correct, 2) file exist, "
                 "3) permissions are correct, 4) file content is correct. "
                 "OpenSSL error info may follow in next messages");
        net_ossl_lastOpenSSLErrorMsg(NULL, 0, NULL, LOG_ERR, "osslCtxInit", "SSL_CTX_use_PrivateKey_file");
        ABORT_FINALIZE(RS_RET_TLS_KEY_ERR);
    }

    /* Set CTX Options */
    SSL_CTX_set_options(pThis->ctx, SSL_OP_NO_SSLv2); /* Disable insecure SSLv2 Protocol */
    SSL_CTX_set_options(pThis->ctx, SSL_OP_NO_SSLv3); /* Disable insecure SSLv3 Protocol */
    SSL_CTX_sess_set_cache_size(pThis->ctx, 1024); /* TODO: make configurable? */

    /* Set default VERIFY Options for OpenSSL CTX - and CALLBACK */
    if (pThis->authMode == OSSL_AUTH_CERTANON) {
        dbgprintf("osslCtxInit: SSL_VERIFY_NONE\n");
        net_ossl_set_ctx_verify_callback(pThis->ctx, SSL_VERIFY_NONE);
    } else {
        dbgprintf("osslCtxInit: SSL_VERIFY_PEER\n");
        net_ossl_set_ctx_verify_callback(pThis->ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT);
    }

    SSL_CTX_set_timeout(pThis->ctx, 30); /* Default Session Timeout, TODO: Make configureable */
    SSL_CTX_set_mode(pThis->ctx, SSL_MODE_AUTO_RETRY);

#if OPENSSL_VERSION_NUMBER >= 0x10100000L && !defined(ENABLE_WOLFSSL)
    /* Enable Support for automatic ephemeral/temporary DH parameter selection. */
    SSL_CTX_set_dh_auto(pThis->ctx, 1);
#endif

#ifndef ENABLE_WOLFSSL
    #if OPENSSL_VERSION_NUMBER >= 0x10002000L
        #if OPENSSL_VERSION_NUMBER <= 0x101010FFL
    /* Enable Support for automatic EC temporary key parameter selection. */
    SSL_CTX_set_ecdh_auto(pThis->ctx, 1);
        #else
        /*
         * SSL_CTX_set_ecdh_auto and SSL_CTX_set_tmp_ecdh are depreceated in higher
         * OpenSSL Versions, so we no more need them - see for more:
         * https://www.openssl.org/docs/manmaster/man3/SSL_CTX_set_ecdh_auto.html
         */
        #endif
    #else
    dbgprintf(
        "osslCtxInit: openssl to old, cannot use SSL_CTX_set_ecdh_auto."
        "Using SSL_CTX_set_tmp_ecdh with NID_X9_62_prime256v1/() instead.\n");
    SSL_CTX_set_tmp_ecdh(pThis->ctx, EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
    #endif
#endif
finalize_it:
    RETiRet;
}

/* Helper function to print usefull OpenSSL errors
 */
void net_ossl_lastOpenSSLErrorMsg(
    uchar *fromHost, int ret, SSL *ssl, int severity, const char *pszCallSource, const char *pszOsslApi) {
    unsigned long un_error = 0;
    int iSSLErr = 0;
    if (ssl == NULL) {
        /* Output Error Info*/
        DBGPRINTF("lastOpenSSLErrorMsg: Error in '%s' with ret=%d\n", pszCallSource, ret);
    } else {
        /* if object is set, get error code */
        iSSLErr = SSL_get_error(ssl, ret);
        /* Output Debug as well */
        DBGPRINTF(
            "lastOpenSSLErrorMsg: %s Error in '%s': '%s(%d)' with ret=%d, errno=%d(%s), sslapi='%s'\n",
            (iSSLErr == SSL_ERROR_SSL ? "SSL_ERROR_SSL"
                                      : (iSSLErr == SSL_ERROR_SYSCALL ? "SSL_ERROR_SYSCALL" : "SSL_ERROR_UNKNOWN")),
            pszCallSource, ERR_error_string(iSSLErr, NULL), iSSLErr, ret, errno, strerror(errno), pszOsslApi);

        /* Output error message */
        LogMsg(0, RS_RET_NO_ERRCODE, severity, "%s Error in '%s': '%s(%d)' with ret=%d, errno=%d(%s), sslapi='%s'\n",
               (iSSLErr == SSL_ERROR_SSL ? "SSL_ERROR_SSL"
                                         : (iSSLErr == SSL_ERROR_SYSCALL ? "SSL_ERROR_SYSCALL" : "SSL_ERROR_UNKNOWN")),
               pszCallSource, ERR_error_string(iSSLErr, NULL), iSSLErr, ret, errno, strerror(errno), pszOsslApi);
    }

    /* Loop through ERR_get_error */
    while ((un_error = ERR_get_error()) > 0) {
        LogMsg(0, RS_RET_NO_ERRCODE, severity, "net_ossl:remote '%s' OpenSSL Error Stack: %s", fromHost,
               ERR_error_string(un_error, NULL));
    }
}

#if OPENSSL_VERSION_NUMBER >= 0x10002000L && !defined(LIBRESSL_VERSION_NUMBER) && !defined(ENABLE_WOLFSSL)
/* initialize tls config commands in openssl context
 */
rsRetVal net_ossl_apply_tlscgfcmd(net_ossl_t *pThis, uchar *tlscfgcmd) {
    DEFiRet;
    char *pCurrentPos;
    char *pNextPos;
    char *pszCmd;
    char *pszValue;
    int iConfErr;

    if (tlscfgcmd == NULL) {
        FINALIZE;
    }

    dbgprintf("net_ossl_apply_tlscgfcmd: Apply tlscfgcmd: '%s'\n", tlscfgcmd);

    /* Set working pointer */
    pCurrentPos = (char *)tlscfgcmd;
    if (pCurrentPos != NULL && strlen(pCurrentPos) > 0) {
        // Create CTX Config Helper
        SSL_CONF_CTX *cctx;
        cctx = SSL_CONF_CTX_new();
        if (pThis->sslState == osslServer) {
            SSL_CONF_CTX_set_flags(cctx, SSL_CONF_FLAG_SERVER);
        } else {
            SSL_CONF_CTX_set_flags(cctx, SSL_CONF_FLAG_CLIENT);
        }
        SSL_CONF_CTX_set_flags(cctx, SSL_CONF_FLAG_FILE);
        SSL_CONF_CTX_set_flags(cctx, SSL_CONF_FLAG_SHOW_ERRORS);
        SSL_CONF_CTX_set_ssl_ctx(cctx, pThis->ctx);

        do {
            pNextPos = index(pCurrentPos, '=');
            if (pNextPos != NULL) {
                while (*pCurrentPos != '\0' && (*pCurrentPos == ' ' || *pCurrentPos == '\t')) pCurrentPos++;
                pszCmd = strndup(pCurrentPos, pNextPos - pCurrentPos);
                pCurrentPos = pNextPos + 1;
                pNextPos = index(pCurrentPos, '\n');
                pNextPos = (pNextPos == NULL ? index(pCurrentPos, ';') : pNextPos);
                pszValue = (pNextPos == NULL ? strdup(pCurrentPos) : strndup(pCurrentPos, pNextPos - pCurrentPos));
                pCurrentPos = (pNextPos == NULL ? NULL : pNextPos + 1);

                /* Add SSL Conf Command */
                iConfErr = SSL_CONF_cmd(cctx, pszCmd, pszValue);
                if (iConfErr > 0) {
                    dbgprintf(
                        "net_ossl_apply_tlscgfcmd: Successfully added Command "
                        "'%s':'%s'\n",
                        pszCmd, pszValue);
                } else {
                    LogError(0, RS_RET_SYS_ERR,
                             "Failed to added Command: %s:'%s' "
                             "in net_ossl_apply_tlscgfcmd with error '%d'",
                             pszCmd, pszValue, iConfErr);
                }

                free(pszCmd);
                free(pszValue);
            } else {
                /* Abort further parsing */
                pCurrentPos = NULL;
            }
        } while (pCurrentPos != NULL);

        /* Finalize SSL Conf */
        iConfErr = SSL_CONF_CTX_finish(cctx);
        if (!iConfErr) {
            LogError(0, RS_RET_SYS_ERR,
                     "Error: setting openssl command parameters: %s"
                     "OpenSSL error info may follow in next messages",
                     tlscfgcmd);
            net_ossl_lastOpenSSLErrorMsg(NULL, 0, NULL, LOG_ERR, "net_ossl_apply_tlscgfcmd", "SSL_CONF_CTX_finish");
        }
        SSL_CONF_CTX_free(cctx);
    }

finalize_it:
    RETiRet;
}
#endif  // OPENSSL_VERSION_NUMBER >= 0x10002000L

/* Convert a fingerprint to printable data. The  conversion is carried out
 * according IETF I-D syslog-transport-tls-12. The fingerprint string is
 * returned in a new cstr object. It is the caller's responsibility to
 * destruct that object.
 * rgerhards, 2008-05-08
 */
static rsRetVal net_ossl_genfingerprintstr(uchar *pFingerprint,
                                           size_t sizeFingerprint,
                                           cstr_t **ppStr,
                                           const char *prefix) {
    cstr_t *pStr = NULL;
    uchar buf[4];
    size_t i;
    DEFiRet;

    CHKiRet(rsCStrConstruct(&pStr));
    CHKiRet(rsCStrAppendStrWithLen(pStr, (uchar *)prefix, strlen(prefix)));
    for (i = 0; i < sizeFingerprint; ++i) {
        snprintf((char *)buf, sizeof(buf), ":%2.2X", pFingerprint[i]);
        CHKiRet(rsCStrAppendStrWithLen(pStr, buf, 3));
    }
    cstrFinalize(pStr);

    *ppStr = pStr;

finalize_it:
    if (iRet != RS_RET_OK) {
        if (pStr != NULL) rsCStrDestruct(&pStr);
    }
    RETiRet;
}


/* Perform a match on ONE peer name obtained from the certificate. This name
 * is checked against the set of configured credentials. *pbFoundPositiveMatch is
 * set to 1 if the ID matches. *pbFoundPositiveMatch must have been initialized
 * to 0 by the caller (this is a performance enhancement as we expect to be
 * called multiple times).
 * TODO: implemet wildcards?
 * rgerhards, 2008-05-26
 */
static rsRetVal net_ossl_chkonepeername(net_ossl_t *pThis,
                                        X509 *certpeer,
                                        uchar *pszPeerID,
                                        int *pbFoundPositiveMatch) {
    permittedPeers_t *pPeer;
#if OPENSSL_VERSION_NUMBER >= 0x10002000L
    int osslRet;
    unsigned int x509flags = 0;
#endif
    char *x509name = NULL;
    DEFiRet;

    if (certpeer == NULL) {
        ABORT_FINALIZE(RS_RET_TLS_NO_CERT);
    }

    ISOBJ_TYPE_assert(pThis, net_ossl);
    assert(pszPeerID != NULL);
    assert(pbFoundPositiveMatch != NULL);

    /* Obtain Namex509 name from subject */
    x509name = X509_NAME_oneline(RSYSLOG_X509_NAME_oneline(certpeer), NULL, 0);

    if (pThis->pPermPeers) { /* do we have configured peer IDs? */
        pPeer = pThis->pPermPeers;
        while (pPeer != NULL) {
            CHKiRet(net.PermittedPeerWildcardMatch(pPeer, pszPeerID, pbFoundPositiveMatch));
            if (*pbFoundPositiveMatch) break;

#if OPENSSL_VERSION_NUMBER >= 0x10002000L
            /* if we did not succeed so far, try ossl X509_check_host
             * ( Includes check against SubjectAlternativeName )
             * if prioritizeSAN set, only check against SAN
             */
            if (pThis->bSANpriority == 1) {
    #if OPENSSL_VERSION_NUMBER >= 0x10100004L && !defined(LIBRESSL_VERSION_NUMBER)
                x509flags = X509_CHECK_FLAG_NEVER_CHECK_SUBJECT;
    #else
                dbgprintf("net_ossl_chkonepeername: PrioritizeSAN not supported before OpenSSL 1.1.0\n");
    #endif  // OPENSSL_VERSION_NUMBER >= 0x10100004L
            }
            osslRet = X509_check_host(certpeer, (const char *)pPeer->pszID, strlen((const char *)pPeer->pszID),
                                      x509flags, NULL);
            if (osslRet == 1) {
                /* Found Peer cert in allowed Peerslist */
                dbgprintf("net_ossl_chkonepeername: Client ('%s') is allowed (X509_check_host)\n", x509name);
                *pbFoundPositiveMatch = 1;
                break;
            } else if (osslRet < 0) {
                net_ossl_lastOpenSSLErrorMsg(NULL, osslRet, NULL, LOG_ERR, "net_ossl_chkonepeername",
                                             "X509_check_host");
                ABORT_FINALIZE(RS_RET_NO_ERRCODE);
            }
#endif
            /* Check next peer */
            pPeer = pPeer->pNext;
        }
    } else {
        LogMsg(0, RS_RET_TLS_NO_CERT, LOG_WARNING,
               "net_ossl_chkonepeername: Peername check could not be done: "
               "no peernames configured.");
    }
finalize_it:
    if (x509name != NULL) {
        OPENSSL_free(x509name);
    }

    RETiRet;
}


/* Check the peer's ID in fingerprint auth mode.
 * rgerhards, 2008-05-22
 */
rsRetVal net_ossl_peerfingerprint(net_ossl_t *pThis, X509 *certpeer, uchar *fromHostIP) {
    DEFiRet;
    unsigned int n;
    uchar fingerprint[20 /*EVP_MAX_MD_SIZE**/];
    uchar fingerprintSha256[32 /*EVP_MAX_MD_SIZE**/];
    size_t size;
    size_t sizeSha256;
    cstr_t *pstrFingerprint = NULL;
    cstr_t *pstrFingerprintSha256 = NULL;
    int bFoundPositiveMatch;
    permittedPeers_t *pPeer;
    const EVP_MD *fdig = EVP_sha1();
    const EVP_MD *fdigSha256 = EVP_sha256();

    ISOBJ_TYPE_assert(pThis, net_ossl);

    if (certpeer == NULL) {
        ABORT_FINALIZE(RS_RET_TLS_NO_CERT);
    }

    /* obtain the SHA1 fingerprint */
    size = sizeof(fingerprint);
    if (!X509_digest(certpeer, fdig, fingerprint, &n)) {
        dbgprintf("net_ossl_peerfingerprint: error X509cert is not valid!\n");
        ABORT_FINALIZE(RS_RET_INVALID_FINGERPRINT);
    }
    sizeSha256 = sizeof(fingerprintSha256);
    if (!X509_digest(certpeer, fdigSha256, fingerprintSha256, &n)) {
        dbgprintf("net_ossl_peerfingerprint: error X509cert is not valid!\n");
        ABORT_FINALIZE(RS_RET_INVALID_FINGERPRINT);
    }
    CHKiRet(net_ossl_genfingerprintstr(fingerprint, size, &pstrFingerprint, "SHA1"));
    dbgprintf("net_ossl_peerfingerprint: peer's certificate SHA1 fingerprint: %s\n",
              cstrGetSzStrNoNULL(pstrFingerprint));
    CHKiRet(net_ossl_genfingerprintstr(fingerprintSha256, sizeSha256, &pstrFingerprintSha256, "SHA256"));
    dbgprintf("net_ossl_peerfingerprint: peer's certificate SHA256 fingerprint: %s\n",
              cstrGetSzStrNoNULL(pstrFingerprintSha256));

    /* now search through the permitted peers to see if we can find a permitted one */
    bFoundPositiveMatch = 0;
    pPeer = pThis->pPermPeers;
    while (pPeer != NULL && !bFoundPositiveMatch) {
        if (!rsCStrSzStrCmp(pstrFingerprint, pPeer->pszID, strlen((char *)pPeer->pszID))) {
            dbgprintf("net_ossl_peerfingerprint: peer's certificate SHA1 MATCH found: %s\n", pPeer->pszID);
            bFoundPositiveMatch = 1;
        } else if (!rsCStrSzStrCmp(pstrFingerprintSha256, pPeer->pszID, strlen((char *)pPeer->pszID))) {
            dbgprintf("net_ossl_peerfingerprint: peer's certificate SHA256 MATCH found: %s\n", pPeer->pszID);
            bFoundPositiveMatch = 1;
        } else {
            dbgprintf("net_ossl_peerfingerprint: NOMATCH peer certificate: %s\n", pPeer->pszID);
            pPeer = pPeer->pNext;
        }
    }

    if (!bFoundPositiveMatch) {
        dbgprintf("net_ossl_peerfingerprint: invalid peer fingerprint, not permitted to talk to it\n");
        if (pThis->bReportAuthErr == 1) {
            errno = 0;
            LogMsg(0, RS_RET_INVALID_FINGERPRINT, LOG_WARNING,
                   "net_ossl:TLS session terminated with remote syslog server '%s': "
                   "Fingerprint check failed, not permitted to talk to %s",
                   fromHostIP, cstrGetSzStrNoNULL(pstrFingerprint));
            pThis->bReportAuthErr = 0;
        }
        ABORT_FINALIZE(RS_RET_INVALID_FINGERPRINT);
    }

finalize_it:
    if (pstrFingerprint != NULL) cstrDestruct(&pstrFingerprint);
    if (pstrFingerprintSha256 != NULL) cstrDestruct(&pstrFingerprintSha256);
    RETiRet;
}


/* Check the peer's ID in name auth mode.
 */
rsRetVal net_ossl_chkpeername(net_ossl_t *pThis, X509 *certpeer, uchar *fromHostIP) {
    DEFiRet;
    uchar lnBuf[256];
    int bFoundPositiveMatch;
    cstr_t *pStr = NULL;
    char *x509name = NULL;

    ISOBJ_TYPE_assert(pThis, net_ossl);

    bFoundPositiveMatch = 0;
    CHKiRet(rsCStrConstruct(&pStr));

    /* Obtain Namex509 name from subject */
    x509name = X509_NAME_oneline(RSYSLOG_X509_NAME_oneline(certpeer), NULL, 0);

    dbgprintf("net_ossl_chkpeername: checking - peername '%s' on server '%s'\n", x509name, fromHostIP);
    snprintf((char *)lnBuf, sizeof(lnBuf), "name: %s; ", x509name);
    CHKiRet(rsCStrAppendStr(pStr, lnBuf));
    CHKiRet(net_ossl_chkonepeername(pThis, certpeer, (uchar *)x509name, &bFoundPositiveMatch));

    if (!bFoundPositiveMatch) {
        dbgprintf("net_ossl_chkpeername: invalid peername, not permitted to talk to it\n");
        if (pThis->bReportAuthErr == 1) {
            cstrFinalize(pStr);
            errno = 0;
            LogMsg(0, RS_RET_INVALID_FINGERPRINT, LOG_WARNING,
                   "net_ossl:TLS session terminated with remote syslog server: "
                   "peer name not authorized, not permitted to talk to %s",
                   cstrGetSzStrNoNULL(pStr));
            pThis->bReportAuthErr = 0;
        }
        ABORT_FINALIZE(RS_RET_INVALID_FINGERPRINT);
    } else {
        dbgprintf("net_ossl_chkpeername: permitted to talk!\n");
    }

finalize_it:
    if (x509name != NULL) {
        OPENSSL_free(x509name);
    }

    if (pStr != NULL) rsCStrDestruct(&pStr);
    RETiRet;
}


/* check the ID of the remote peer - used for both fingerprint and
 * name authentication.
 */
X509 *net_ossl_getpeercert(net_ossl_t *pThis, SSL *ssl, uchar *fromHostIP) {
    X509 *certpeer;

    ISOBJ_TYPE_assert(pThis, net_ossl);

    /* Get peer certificate from SSL */
    certpeer = SSL_get_peer_certificate(ssl);
    if (certpeer == NULL) {
        if (pThis->bReportAuthErr == 1 && 1) {
            errno = 0;
            pThis->bReportAuthErr = 0;
            LogMsg(0, RS_RET_TLS_NO_CERT, LOG_WARNING,
                   "net_ossl:TLS session terminated with remote syslog server '%s': "
                   "Peer check failed, peer did not provide a certificate.",
                   fromHostIP);
        }
    }
    return certpeer;
}


/* Verify the validity of the remote peer's certificate.
 */
rsRetVal net_ossl_chkpeercertvalidity(net_ossl_t __attribute__((unused)) * pThis, SSL *ssl, uchar *fromHostIP) {
    DEFiRet;
    int iVerErr = X509_V_OK;

    ISOBJ_TYPE_assert(pThis, net_ossl);
    PermitExpiredCerts *pPermitExpiredCerts = (PermitExpiredCerts *)SSL_get_ex_data(ssl, 1);

#ifdef ENABLE_WOLFSSL
    iVerErr = wolfSSL_get_verify_result(ssl);
#else
    iVerErr = SSL_get_verify_result(ssl);
#endif
    if (iVerErr != X509_V_OK) {
        if (iVerErr == X509_V_ERR_CERT_HAS_EXPIRED) {
            if (pPermitExpiredCerts != NULL && *pPermitExpiredCerts == OSSL_EXPIRED_DENY) {
                LogMsg(0, RS_RET_CERT_EXPIRED, LOG_INFO,
                       "net_ossl:TLS session terminated with remote syslog server '%s': "
                       "not permitted to talk to peer, certificate invalid: "
                       "Certificate expired: %s",
                       fromHostIP, X509_verify_cert_error_string(iVerErr));
                ABORT_FINALIZE(RS_RET_CERT_EXPIRED);
            } else if (pPermitExpiredCerts != NULL && *pPermitExpiredCerts == OSSL_EXPIRED_WARN) {
                LogMsg(0, RS_RET_NO_ERRCODE, LOG_WARNING,
                       "net_ossl:CertValidity check - warning talking to peer '%s': "
                       "certificate expired: %s",
                       fromHostIP, X509_verify_cert_error_string(iVerErr));
            } else {
                dbgprintf(
                    "net_ossl_chkpeercertvalidity: talking to peer '%s': "
                    "certificate expired: %s\n",
                    fromHostIP, X509_verify_cert_error_string(iVerErr));
            } /* Else do nothing */
        } else if (iVerErr == X509_V_ERR_CERT_REVOKED) {
            LogMsg(0, RS_RET_CERT_REVOKED, LOG_INFO,
                   "net_ossl:TLS session terminated with remote syslog server '%s': "
                   "not permitted to talk to peer, certificate invalid: "
                   "certificate revoked '%s'",
                   fromHostIP, X509_verify_cert_error_string(iVerErr));
            ABORT_FINALIZE(RS_RET_CERT_EXPIRED);
        } else {
            LogMsg(0, RS_RET_CERT_INVALID, LOG_INFO,
                   "net_ossl:TLS session terminated with remote syslog server '%s': "
                   "not permitted to talk to peer, certificate validation failed: %s",
                   fromHostIP, X509_verify_cert_error_string(iVerErr));
            ABORT_FINALIZE(RS_RET_CERT_INVALID);
        }
    } else {
        dbgprintf("net_ossl_chkpeercertvalidity: client certificate validation success: %s\n",
                  X509_verify_cert_error_string(iVerErr));
    }

finalize_it:
    RETiRet;
}

/*--------------------------------------OCSP Support ------------------------------------------*/

#ifndef ENABLE_WOLFSSL
/* OCSP is not available in WolfSSL builds */

/* OCSP Response Cache
 *
 * Concurrency & Locking:
 * - Cache is shared across all workers (global state)
 * - Protected by ocsp_cache_mutex (pthread_mutex_t)
 * - All cache operations (lookup/store/cleanup) acquire mutex before access
 * - Cache entries store OCSP responses with expiration times
 *
 * Non-blocking I/O:
 * - Socket operations use non-blocking mode during connect with timeout
 * - Once connected, sockets use SO_RCVTIMEO/SO_SNDTIMEO for bounded I/O
 * - Maximum timeout per OCSP responder: OCSP_TIMEOUT (5 seconds)
 *
 * Cache Strategy:
 * - Key: certificate serial number + issuer name hash (SHA-256)
 * - TTL: Based on OCSP response nextUpdate field, or OCSP_CACHE_DEFAULT_TTL (1 hour)
 * - Eviction: Prefers expired entries, then FIFO from tail when cache reaches MAX_ENTRIES
 * - Expired entries removed during lookup to maintain cache efficiency
 * - Thread-safe: All cache operations protected by mutex
 */
typedef struct ocsp_cache_entry_s {
    char *cache_key; /* cert serial + issuer hash */
    int cert_status; /* V_OCSP_CERTSTATUS_* */
    time_t expires_at; /* when this entry expires */
    struct ocsp_cache_entry_s *next;
} ocsp_cache_entry_t;

static ocsp_cache_entry_t *ocsp_cache_head = NULL;
static int ocsp_cache_count = 0;
static pthread_mutex_t ocsp_cache_mutex = PTHREAD_MUTEX_INITIALIZER;

static char *ocsp_make_cache_key(X509 *cert, X509 *issuer) {
    DEFiRet;
    ASN1_INTEGER *serial = X509_get_serialNumber(cert);
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int md_len = 0;
    unsigned char key_md[EVP_MAX_MD_SIZE];
    unsigned int key_md_len = 0;
    char *key = NULL;
    int key_len;
    BIGNUM *bn = NULL;
    char *serial_hex = NULL;

    if (!serial || !issuer) ABORT_FINALIZE(RS_RET_ERR);

    /* Hash issuer name for compact key */
    X509_NAME *issuer_name = X509_get_subject_name(issuer);
    if (!X509_NAME_digest(issuer_name, EVP_sha256(), md, &md_len)) {
        ABORT_FINALIZE(RS_RET_ERR);
    }

    /* Hash issuer public key to avoid collisions across key rollovers */
    if (!X509_pubkey_digest(issuer, EVP_sha256(), key_md, &key_md_len)) {
        ABORT_FINALIZE(RS_RET_ERR);
    }

    /* Format: serial-number:issuer-name-hash:issuer-key-hash */
    bn = ASN1_INTEGER_to_BN(serial, NULL);
    if (!bn) ABORT_FINALIZE(RS_RET_ERR);

    serial_hex = BN_bn2hex(bn);
    if (!serial_hex) ABORT_FINALIZE(RS_RET_ERR);

    key_len = strlen(serial_hex) + (md_len * 2) + (key_md_len * 2) + 3;
    CHKmalloc(key = malloc(key_len));

    char *p = key;
    int written = snprintf(p, key_len, "%s:", serial_hex);
    if (written < 0 || written >= key_len) {
        ABORT_FINALIZE(RS_RET_ERR);
    }
    p += written;
    size_t remaining = (size_t)key_len - (size_t)written;
    for (unsigned int i = 0; i < md_len; i++) {
        if (remaining < 3) {
            ABORT_FINALIZE(RS_RET_ERR);
        }
        written = snprintf(p, remaining, "%02x", md[i]);
        if (written != 2) {
            ABORT_FINALIZE(RS_RET_ERR);
        }
        p += written;
        remaining -= (size_t)written;
    }
    if (remaining < 2) {
        ABORT_FINALIZE(RS_RET_ERR);
    }
    *p++ = ':';
    *p = '\0';
    remaining--;
    for (unsigned int i = 0; i < key_md_len; i++) {
        if (remaining < 3) {
            ABORT_FINALIZE(RS_RET_ERR);
        }
        written = snprintf(p, remaining, "%02x", key_md[i]);
        if (written != 2) {
            ABORT_FINALIZE(RS_RET_ERR);
        }
        p += written;
        remaining -= (size_t)written;
    }

finalize_it:
    if (iRet != RS_RET_OK) {
        free(key);
        key = NULL;
    }
    if (bn) BN_free(bn);
    if (serial_hex) OPENSSL_free(serial_hex);
    return key;
}

static int ocsp_cache_lookup(const char *cache_key, int *cert_status) {
    int found = 0;
    time_t now = time(NULL);

    pthread_mutex_lock(&ocsp_cache_mutex);

    ocsp_cache_entry_t **pp = &ocsp_cache_head;
    while (*pp) {
        ocsp_cache_entry_t *entry = *pp;
        if (strcmp(entry->cache_key, cache_key) == 0) {
            if (entry->expires_at > now) {
                *cert_status = entry->cert_status;
                found = 1;
                dbgprintf("OCSP: Cache hit for key %s, status=%d\n", cache_key, *cert_status);
            } else {
                /* Remove expired entry */
                dbgprintf("OCSP: Removing expired cache entry for key %s\n", cache_key);
                *pp = entry->next;
                free(entry->cache_key);
                free(entry);
                ocsp_cache_count--;
            }
            break;
        }
        pp = &(*pp)->next;
    }

    pthread_mutex_unlock(&ocsp_cache_mutex);
    return found;
}

static void ocsp_cache_store(const char *cache_key, int cert_status, time_t ttl) {
    DEFiRet;
    pthread_mutex_lock(&ocsp_cache_mutex);

    /* Check if already exists and update */
    ocsp_cache_entry_t *entry = ocsp_cache_head;
    while (entry) {
        if (strcmp(entry->cache_key, cache_key) == 0) {
            entry->cert_status = cert_status;
            entry->expires_at = time(NULL) + ttl;
            dbgprintf("OCSP: Cache updated for key %s\n", cache_key);
            pthread_mutex_unlock(&ocsp_cache_mutex);
            return;
        }
        entry = entry->next;
    }

    /* Try to evict expired entries first before removing valid ones */
    if (ocsp_cache_count >= OCSP_CACHE_MAX_ENTRIES) {
        time_t now = time(NULL);
        ocsp_cache_entry_t **pp = &ocsp_cache_head;
        ocsp_cache_entry_t *expired_entry = NULL;

        /* Find first expired entry */
        while (*pp) {
            if ((*pp)->expires_at <= now) {
                expired_entry = *pp;
                *pp = expired_entry->next;
                free(expired_entry->cache_key);
                free(expired_entry);
                ocsp_cache_count--;
                break;
            }
            pp = &(*pp)->next;
        }

        /* If no expired entries found, evict from tail (true FIFO) */
        if (!expired_entry && ocsp_cache_head) {
            pp = &ocsp_cache_head;
            while ((*pp)->next) {
                pp = &(*pp)->next;
            }
            ocsp_cache_entry_t *to_remove = *pp;
            *pp = NULL;
            free(to_remove->cache_key);
            free(to_remove);
            ocsp_cache_count--;
        }
    }

    /* Add new entry */
    ocsp_cache_entry_t *new_entry = NULL;
    CHKmalloc(new_entry = malloc(sizeof(ocsp_cache_entry_t)));
    CHKmalloc(new_entry->cache_key = strdup(cache_key));
    new_entry->cert_status = cert_status;
    new_entry->expires_at = time(NULL) + ttl;
    new_entry->next = ocsp_cache_head;
    ocsp_cache_head = new_entry;
    ocsp_cache_count++;
    dbgprintf("OCSP: Cache stored for key %s, count=%d\n", cache_key, ocsp_cache_count);

finalize_it:
    if (iRet == RS_RET_OUT_OF_MEMORY) {
        LogError(0, RS_RET_OUT_OF_MEMORY, "OCSP: Failed to allocate cache entry\n");
    }
    if (iRet != RS_RET_OK) {
        if (new_entry) {
            free(new_entry->cache_key);
            free(new_entry);
        }
    }
    pthread_mutex_unlock(&ocsp_cache_mutex);
}

void ocsp_cache_cleanup(void) {
    pthread_mutex_lock(&ocsp_cache_mutex);

    ocsp_cache_entry_t *entry = ocsp_cache_head;
    while (entry) {
        ocsp_cache_entry_t *next = entry->next;
        free(entry->cache_key);
        free(entry);
        entry = next;
    }
    ocsp_cache_head = NULL;
    ocsp_cache_count = 0;

    pthread_mutex_unlock(&ocsp_cache_mutex);
}

/*
 * CRL is not implemented!
 *
 * This is just a sanity-check stub, to fail on CRL-only certificates.
 * CRL-only certificate means: Certificate CRL DP, but not OCSP/AuthorityInfoAccess information.
 *
 * Returns:
 *  0: if CRL check failed (Error, or Revoked when is_revoked is set)
 *  1: if the certificate status is "GOOD"
 *  2: if the certificate holds no CRL URL
 */
static int crl_check(X509 *current_cert, int *is_revoked) {
    int ret = 0;
    STACK_OF(DIST_POINT) * crldp;
    STACK_OF(OPENSSL_STRING) * ocsp_uris;

    /* Reset revoked status. Needs to be set in case CRL gets implemented. */
    if (is_revoked) *is_revoked = 0;

    crldp = X509_get_ext_d2i(current_cert, NID_crl_distribution_points, NULL, NULL);
    ocsp_uris = X509_get1_ocsp(current_cert);
    if (crldp && ocsp_uris == NULL) {
        LogError(0, RS_RET_NO_ERRCODE,
                 "CRL support is not implemented. "
                 "CRL-only certificates can't be validated!\n");
        ret = 0; /* abort verification. CRL-only not supported.  */
    } else if (!crldp) {
        ret = 2;
    }

    if (ocsp_uris) X509_email_free(ocsp_uris);

    if (crldp) sk_DIST_POINT_pop_free(crldp, DIST_POINT_free);

    return ret;
}

/*
 * Find the issuer certificate, which is mandatory to generate
 * a OCSP request.
 *
 * Query first locally configured and trusted certificates (e.g CAs).
 *
 * Last resort: Use the certificate chain of the peer, to get hold of
 * untrusted certificates (e.g. intermediate certificates).
 *
 * The resulting issuer certificate might be an untrusted certificate,
 * which should be used to generate OCSP requests.
 */
static X509 *ocsp_find_issuer(X509 *target_cert,
                              const char *cert_name,
                              SSL_CTX *ctx,
                              STACK_OF(X509) * untrusted_peer_certs) {
    X509 *issuer = NULL;
    X509_STORE *store = SSL_CTX_get_cert_store(ctx);
    STACK_OF(X509_OBJECT) * objs;

    /* find issuer among local trusted issuers */
    if (store != NULL) {
    #if OPENSSL_VERSION_NUMBER >= 0x10100000L
        objs = X509_STORE_get0_objects(store);
        for (int i = 0; i < sk_X509_OBJECT_num(objs); i++) {
            X509 *cert = X509_OBJECT_get0_X509(sk_X509_OBJECT_value(objs, i));
            if (cert && X509_check_issued(cert, target_cert) == X509_V_OK) {
                issuer = cert;
                break;
            }
        }
    #else
        /* OpenSSL 1.0.2 compatibility: direct access to store fields */
        objs = store->objs;
        for (int i = 0; i < sk_X509_OBJECT_num(objs); i++) {
            X509_OBJECT *x509obj = sk_X509_OBJECT_value(objs, i);
            X509 *cert = x509obj->type == X509_LU_X509 ? x509obj->data.x509 : NULL;
            if (cert && X509_check_issued(cert, target_cert) == X509_V_OK) {
                issuer = cert;
                break;
            }
        }
    #endif
    }

    if (!issuer) {
        /* Look for intermediate certificates in the remote cert-chain */
        for (int i = 0; i < sk_X509_num(untrusted_peer_certs); i++) {
            X509 *cert = sk_X509_value(untrusted_peer_certs, i);
            if (X509_check_issued(cert, target_cert) == X509_V_OK) {
                issuer = cert;
                break;
            }
        }
    }

    /* No issuer, no revocation check possible */
    if (!issuer) {
        LogError(0, RS_RET_NO_ERRCODE, "OCSP: Could not find any issuer for: \"%s\"\n", cert_name);
    }

    return issuer;
}

static const char *ocsp_get_response_status_err(int c) {
    switch (c) {
        case OCSP_RESPONSE_STATUS_SUCCESSFUL:
            return "Successful";
        case OCSP_RESPONSE_STATUS_MALFORMEDREQUEST:
            return "Malformed request";
        case OCSP_RESPONSE_STATUS_INTERNALERROR:
            return "Internal error in OCSP responder";
        case OCSP_RESPONSE_STATUS_TRYLATER:
            return "Try again later";
        case OCSP_RESPONSE_STATUS_SIGREQUIRED:
            return "Must sign the request";
        case OCSP_RESPONSE_STATUS_UNAUTHORIZED:
            return "Request unauthorized";
        default:
            return "Unknown Response Status";
    }
}

/*
 * Returns 1 if the cert status is "GOOD"
 * Stores result in cache if successful
 */
static int ocsp_check_validate_response_and_cert(OCSP_RESPONSE *rsp,
                                                 OCSP_REQUEST *req,
                                                 STACK_OF(X509) * untrusted_peer_certs,
                                                 OCSP_CERTID *id,
                                                 SSL_CTX *ctx,
                                                 const char *cert_name,
                                                 X509 *cert,
                                                 X509 *issuer,
                                                 int *is_revoked) {
    int s, ret = 0;
    long leeway_sec = 300; /* 5 Minutes */
    long maxage = -1; /* No maximum age for ThisUpdate response */

    int status, reason;
    ASN1_GENERALIZEDTIME *rev, *thisupd, *nextupd;
    OCSP_BASICRESP *bs = NULL;
    X509_STORE *store = SSL_CTX_get_cert_store(ctx);

    bs = OCSP_response_get1_basic(rsp);
    if (bs == NULL) {
        LogError(0, RS_RET_NO_ERRCODE, "Failed to decode the basic OCSP response.\n");
        goto err;
    }

    s = OCSP_check_nonce(req, bs);
    if (s == -1) {
        LogMsg(0, RS_RET_NO_ERRCODE, LOG_WARNING,
               "Warning: no Nonce in OCSP response from peer. "
               "(No replay attack protection available)\n");
    } else if (s == 0) {
        LogError(0, RS_RET_NO_ERRCODE, "Nonce mismatch in OCSP response.\n");
        goto err;
    }

    s = OCSP_basic_verify(bs, untrusted_peer_certs, store, 0);
    if (s <= 0) {
        LogError(0, RS_RET_NO_ERRCODE, "OCSP response verification failed.\n");
        goto err;
    }

    if (!OCSP_resp_find_status(bs, id, &status, &reason, &rev, &thisupd, &nextupd)) {
        LogError(0, RS_RET_NO_ERRCODE, "Failed to get OCSP response status for: %s\n", cert_name);
        goto err;
    }

    s = OCSP_check_validity(thisupd, nextupd, leeway_sec, maxage);
    if (s == 0) {
        LogError(0, RS_RET_NO_ERRCODE, "OCSP response is not valid (either expired or not yet valid).\n");
        goto err;
    }

    /* Check certificate status */
    switch (status) {
        case V_OCSP_CERTSTATUS_GOOD:
            dbgprintf("OCSP: Certificate status is GOOD for: %s\n", cert_name);
            ret = 1;
            break;
        case V_OCSP_CERTSTATUS_REVOKED:
            LogMsg(0, RS_RET_CERT_REVOKED, LOG_ERR, "OCSP: Certificate status is REVOKED for: %s (reason: %s)\n",
                   cert_name, OCSP_crl_reason_str(reason));
            if (is_revoked) *is_revoked = 1;
            ret = 0;
            break;
        case V_OCSP_CERTSTATUS_UNKNOWN:
            LogError(0, RS_RET_NO_ERRCODE, "OCSP: Certificate status is UNKNOWN for: %s\n", cert_name);
            ret = 0;
            break;
        default:
            LogError(0, RS_RET_NO_ERRCODE, "OCSP: Unknown certificate status for: %s\n", cert_name);
            ret = 0;
            break;
    }

    /* Store result in cache */
    char *cache_key = ocsp_make_cache_key(cert, issuer);
    if (cache_key) {
        time_t cache_ttl = OCSP_CACHE_DEFAULT_TTL;
        /* Use nextUpdate if available for more accurate cache expiry */
        if (nextupd) {
            /* Use ASN1_TIME_diff() which properly handles UTC times without timezone issues */
            int pday, psec;
            if (ASN1_TIME_diff(&pday, &psec, NULL, nextupd) == 1) {
                /* nextupd is in the future */
                time_t seconds_until_expiry = (pday * 86400) + psec;
                if (seconds_until_expiry > 0) {
                    cache_ttl = seconds_until_expiry;
                }
            } else {
                dbgprintf("OCSP: ASN1_TIME_diff() failed, using default TTL\n");
            }
        }
        ocsp_cache_store(cache_key, status, cache_ttl);
        free(cache_key);
    }

err:
    if (bs) OCSP_BASICRESP_free(bs);

    return ret;
}

static BIO *ocsp_connect(const char *host, const char *port, const char *device) {
    BIO *bio = NULL;
    int sock = -1;
    struct addrinfo hints, *res = NULL, *rp;
    int s;
    int flags;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    /* TODO: DNS resolution is blocking with no timeout. A malicious certificate
     * with OCSP responder URLs pointing to slow/unresponsive DNS names can cause
     * the TLS handshake to block indefinitely. This is called from the TLS verify
     * callback. Consider using async DNS resolution or making OCSP optional.
     */
    s = getaddrinfo(host, port, &hints, &res);
    if (s != 0) {
        LogError(0, RS_RET_NO_ERRCODE, "OCSP: getaddrinfo failed for %s:%s: %s\n", host, port, gai_strerror(s));
        goto err;
    }

    for (rp = res; rp != NULL; rp = rp->ai_next) {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock == -1) continue;

        if (device && *device) {
    #ifdef SO_BINDTODEVICE
            if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, device, strlen(device)) < 0) {
                LogError(errno, RS_RET_NO_ERRCODE, "OCSP: Failed to bind socket to device %s\n", device);
                close(sock);
                sock = -1;
                continue;
            }
    #else
            LogMsg(0, RS_RET_NO_ERRCODE, LOG_WARNING,
                   "OCSP: SO_BINDTODEVICE not supported, ignoring device parameter\n");
    #endif
        }

        /* Set socket to non-blocking for timeout support */
        flags = fcntl(sock, F_GETFL, 0);
        if (flags != -1) {
            fcntl(sock, F_SETFL, flags | O_NONBLOCK);
        }

        if (connect(sock, rp->ai_addr, rp->ai_addrlen) == 0) {
            break; /* Success */
        }

        if (errno == EINPROGRESS) {
            fd_set wfds;
            struct timeval tv;
            FD_ZERO(&wfds);
            FD_SET(sock, &wfds);
            tv.tv_sec = OCSP_TIMEOUT;
            tv.tv_usec = 0;

            s = select(sock + 1, NULL, &wfds, NULL, &tv);
            if (s > 0) {
                int error = 0;
                socklen_t len = sizeof(error);
                if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len) == 0 && error == 0) {
                    break; /* Success */
                }
            }
        }

        close(sock);
        sock = -1;
    }

    if (sock == -1 || rp == NULL) {
        LogError(0, RS_RET_NO_ERRCODE, "OCSP: Failed to connect to %s:%s\n", host, port);
        goto err;
    }

    /* Set socket back to blocking mode with read/write timeout */
    flags = fcntl(sock, F_GETFL, 0);
    if (flags != -1) {
        fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
    }

    /* Set socket read and write timeouts to prevent indefinite blocking during OCSP I/O */
    struct timeval timeout;
    timeout.tv_sec = OCSP_TIMEOUT;
    timeout.tv_usec = 0;

    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        LogMsg(0, RS_RET_NO_ERRCODE, LOG_WARNING,
               "OCSP: Failed to set socket read timeout (errno %d), continuing without timeout\n", errno);
    }

    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        LogMsg(0, RS_RET_NO_ERRCODE, LOG_WARNING,
               "OCSP: Failed to set socket write timeout (errno %d), continuing without timeout\n", errno);
    }

    bio = BIO_new_socket(sock, BIO_CLOSE);
    if (!bio) {
        LogError(0, RS_RET_NO_ERRCODE, "OCSP: Failed to create BIO\n");
        close(sock);
        goto err;
    }

err:
    if (res) freeaddrinfo(res);

    return bio;
}

static OCSP_RESPONSE *ocsp_send_and_receive(BIO *bio, const char *host, const char *path, OCSP_REQUEST *req) {
    OCSP_RESPONSE *rsp = NULL;
    char *req_data = NULL;
    int req_len;
    int rv;

    /* Encode OCSP request */
    req_len = i2d_OCSP_REQUEST(req, (unsigned char **)&req_data);
    if (req_len <= 0) {
        LogError(0, RS_RET_NO_ERRCODE, "OCSP: Failed to encode OCSP request\n");
        goto err;
    }

    /* Send HTTP POST request */
    rv = BIO_printf(bio,
                    "POST %s HTTP/1.0\r\n"
                    "Host: %s\r\n"
                    "Content-Type: application/ocsp-request\r\n"
                    "Content-Length: %d\r\n"
                    "\r\n",
                    path, host, req_len);

    if (rv <= 0) {
        LogError(0, RS_RET_NO_ERRCODE, "OCSP: Failed to send HTTP header\n");
        goto err;
    }

    /* Send request data */
    if (BIO_write(bio, req_data, req_len) != req_len) {
        LogError(0, RS_RET_NO_ERRCODE, "OCSP: Failed to send OCSP request data\n");
        goto err;
    }

    if (BIO_flush(bio) != 1) {
        LogError(0, RS_RET_NO_ERRCODE, "OCSP: Failed to flush BIO\n");
        goto err;
    }

    /* Read HTTP response - skip headers */
    char buf[1024];
    int in_headers = 1;
    long content_length = -1;
    const long MAX_OCSP_RESPONSE_SIZE = 1024 * 1024; /* 1MB limit */

    while (in_headers) {
        rv = BIO_gets(bio, buf, sizeof(buf));
        if (rv <= 0) {
            LogError(0, RS_RET_NO_ERRCODE, "OCSP: Failed to read HTTP response headers\n");
            goto err;
        }

        /* Parse Content-Length header */
        if (strncasecmp(buf, "Content-Length:", 15) == 0) {
            content_length = atol(buf + 15);
            if (content_length > MAX_OCSP_RESPONSE_SIZE) {
                LogError(0, RS_RET_NO_ERRCODE,
                         "OCSP: Response Content-Length (%ld) exceeds maximum allowed size (%ld)\n", content_length,
                         MAX_OCSP_RESPONSE_SIZE);
                goto err;
            }
        }

        /* Empty line marks end of headers */
        if (strcmp(buf, "\r\n") == 0 || strcmp(buf, "\n") == 0) {
            in_headers = 0;
        }
    }

    /* Read OCSP response with size limit */
    if (content_length > 0 && content_length <= MAX_OCSP_RESPONSE_SIZE) {
        /* Size is within limits, proceed with reading */
        rsp = d2i_OCSP_RESPONSE_bio(bio, NULL);
    } else if (content_length == -1) {
        /* No Content-Length header - read with caution */
        LogMsg(0, RS_RET_NO_ERRCODE, LOG_WARNING,
               "OCSP: No Content-Length header in response, reading without size validation\n");
        rsp = d2i_OCSP_RESPONSE_bio(bio, NULL);
    }

    if (!rsp) {
        LogError(0, RS_RET_NO_ERRCODE, "OCSP: Failed to parse OCSP response\n");
        goto err;
    }

err:
    if (req_data) OPENSSL_free(req_data);

    return rsp;
}

static int ocsp_is_supported_protocol(const char *url) {
    return (strncmp(url, "http://", 7) == 0);
}

static int ocsp_request_per_responder(const char *url,
                                      X509 *cert,
                                      X509 *issuer,
                                      STACK_OF(X509) * untrusted_peer_certs,
                                      SSL_CTX *ctx,
                                      const char *device,
                                      int *is_revoked) {
    int ret = 0;
    char *host = NULL, *port = NULL, *path = NULL;
    int use_ssl = 0;
    BIO *bio = NULL;
    OCSP_REQUEST *req = NULL;
    OCSP_RESPONSE *rsp = NULL;
    OCSP_CERTID *id = NULL;
    char cert_name[256];

    X509_NAME_oneline(X509_get_subject_name(cert), cert_name, sizeof(cert_name));

    /* Parse URL */
    if (!OCSP_parse_url((char *)url, &host, &port, &path, &use_ssl)) {
        LogError(0, RS_RET_NO_ERRCODE, "OCSP: Failed to parse URL: %s\n", url);
        goto err;
    }

    /* Only HTTP is supported */
    if (use_ssl) {
        dbgprintf("OCSP: HTTPS not supported, skipping %s\n", url);
        goto err;
    }

    dbgprintf("OCSP: Connecting to %s:%s%s\n", host, port, path);

    /* Connect to OCSP responder */
    bio = ocsp_connect(host, port, device);
    if (!bio) {
        goto err;
    }

    /* Create OCSP request */
    req = OCSP_REQUEST_new();
    if (!req) {
        LogError(0, RS_RET_NO_ERRCODE, "OCSP: Failed to create OCSP request\n");
        goto err;
    }

    id = OCSP_cert_to_id(NULL, cert, issuer);
    if (!id) {
        LogError(0, RS_RET_NO_ERRCODE, "OCSP: Failed to create OCSP certificate ID\n");
        goto err;
    }

    if (!OCSP_request_add0_id(req, id)) {
        LogError(0, RS_RET_NO_ERRCODE, "OCSP: Failed to add certificate ID to OCSP request\n");
        OCSP_CERTID_free(id);
        goto err;
    }

    /* Add nonce extension for replay attack protection */
    OCSP_request_add1_nonce(req, NULL, -1);

    /* Send request and receive response */
    rsp = ocsp_send_and_receive(bio, host, path, req);
    if (!rsp) {
        goto err;
    }

    /* Check response status */
    int response_status = OCSP_response_status(rsp);
    if (response_status != OCSP_RESPONSE_STATUS_SUCCESSFUL) {
        LogError(0, RS_RET_NO_ERRCODE, "OCSP: Response status: %s\n", ocsp_get_response_status_err(response_status));
        goto err;
    }

    /* Validate response and check certificate status */
    ret = ocsp_check_validate_response_and_cert(rsp, req, untrusted_peer_certs, id, ctx, cert_name, cert, issuer,
                                                is_revoked);

err:
    if (rsp) OCSP_RESPONSE_free(rsp);
    if (req) OCSP_REQUEST_free(req);
    if (bio) BIO_free_all(bio);
    if (host) OPENSSL_free(host);
    if (port) OPENSSL_free(port);
    if (path) OPENSSL_free(path);

    return ret;
}

static int ocsp_check(
    X509 *current_cert, STACK_OF(X509) * untrusted_peer_certs, SSL_CTX *ctx, const char *device, int *is_revoked) {
    int ret = 0;
    char cert_name[256];
    X509 *issuer = NULL;
    STACK_OF(OPENSSL_STRING) *ocsp_responders = NULL;
    const char *url = NULL;
    int at_least_one_responder = 0;
    char *cache_key = NULL;
    int cached_status;

    X509_NAME_oneline(X509_get_subject_name(current_cert), cert_name, sizeof(cert_name));

    /* Reset revoked status */
    if (is_revoked) *is_revoked = 0;

    /*
     * 1. Lookup the issuer cert of the current certificate, required to marshal a OCSP request.
     */
    if (!(issuer = ocsp_find_issuer(current_cert, cert_name, ctx, untrusted_peer_certs))) goto err;

    /*
     * 2. Check cache first to avoid network I/O
     */
    cache_key = ocsp_make_cache_key(current_cert, issuer);
    if (cache_key && ocsp_cache_lookup(cache_key, &cached_status)) {
        if (cached_status == V_OCSP_CERTSTATUS_GOOD) {
            dbgprintf("OCSP: Cache indicates certificate is GOOD for: %s\n", cert_name);
            ret = 1;
            goto err;
        } else if (cached_status == V_OCSP_CERTSTATUS_REVOKED) {
            LogMsg(0, RS_RET_CERT_REVOKED, LOG_ERR, "OCSP: Cached status shows certificate is REVOKED for: %s\n",
                   cert_name);
            if (is_revoked) *is_revoked = 1;
            goto err;
        }
        /* UNKNOWN status: fall through to network check */
    }

    /*
     * 3. Sanity check prior generating OCSP request.
     *
     * - No OCSP URL, no OCSP revocation verification required, skip the current cert.
     * - Only HTTP protocol is supported. If no OCSP responder with a supported protocol
     *   is available, this MUST fail the OCSP verification and abort the TLS handshake.
     */
    ocsp_responders = X509_get1_ocsp(current_cert);
    if (ocsp_responders == NULL) {
        ret = 2;
        goto err; /* continue with cert path validation */
    }

    for (int i = 0; i < sk_OPENSSL_STRING_num(ocsp_responders); i++) {
        url = sk_OPENSSL_STRING_value(ocsp_responders, i);
        if (ocsp_is_supported_protocol(url)) {
            at_least_one_responder = 1;
            break;
        }
    }

    if (!at_least_one_responder) {
        LogError(0, RS_RET_NO_ERRCODE,
                 "None of the OCSP responders are supported by "
                 "this implementation:\n");
        for (int i = 0; i < sk_OPENSSL_STRING_num(ocsp_responders); i++) {
            url = sk_OPENSSL_STRING_value(ocsp_responders, i);
            LogError(0, RS_RET_NO_ERRCODE, "\t%s\n", url);
        }
        goto err;
    }

    /*
     * Try all supported OCSP responders one by one.
     *
     * One successful OCSP status response is enough. Not all responders
     * need to be queried.
     */
    for (int i = 0; i < sk_OPENSSL_STRING_num(ocsp_responders); i++) {
        url = sk_OPENSSL_STRING_value(ocsp_responders, i);
        if (ocsp_request_per_responder(url, current_cert, issuer, untrusted_peer_certs, ctx, device, is_revoked)) {
            ret = 1;
            goto err;
        }

        /* When revoked don't try other sources/OCSP responders */
        if (is_revoked && *is_revoked) goto err;

        /* If the status is unknown, try the next available responder */
    }

err:
    if (ocsp_responders) X509_email_free(ocsp_responders);
    if (cache_key) free(cache_key);

    return ret;
}

#endif /* !ENABLE_WOLFSSL */

/*--------------------------------------End OCSP Support ------------------------------------------*/

/* Verify Callback for X509 Certificate validation. Force visibility as this function is not called anywhere but
 *  only used as callback!
 */
int net_ossl_verify_callback(int status, X509_STORE_CTX *store) {
    char szdbgdata1[256];
    char szdbgdata2[256];

    dbgprintf("verify_callback: status %d\n", status);

    if (status == 0) {
        /* Retrieve all needed pointers */
        X509 *cert = X509_STORE_CTX_get_current_cert(store);
        int depth = X509_STORE_CTX_get_error_depth(store);
        SSL *ssl = X509_STORE_CTX_get_ex_data(store, SSL_get_ex_data_X509_STORE_CTX_idx());
        int err = X509_STORE_CTX_get_error(store);
        int iVerifyMode = SSL_get_verify_mode(ssl);
        nsd_t *pNsdTcp = (nsd_t *)SSL_get_ex_data(ssl, 0);
        PermitExpiredCerts *pPermitExpiredCerts = (PermitExpiredCerts *)SSL_get_ex_data(ssl, 1);

        dbgprintf("verify_callback: Certificate validation failed, Mode (%d)!\n", iVerifyMode);

        X509_NAME_oneline(X509_get_issuer_name(cert), szdbgdata1, sizeof(szdbgdata1));
        X509_NAME_oneline(RSYSLOG_X509_NAME_oneline(cert), szdbgdata2, sizeof(szdbgdata2));

        uchar *fromHost = NULL;
        if (pNsdTcp != NULL) {
            nsd_ptcp.GetRemoteHName(pNsdTcp, &fromHost);
        }

        if (iVerifyMode != SSL_VERIFY_NONE) {
            /* Handle expired Certificates **/
            if (err == X509_V_OK || err == X509_V_ERR_CERT_HAS_EXPIRED) {
                if (pPermitExpiredCerts != NULL && *pPermitExpiredCerts == OSSL_EXPIRED_PERMIT) {
                    dbgprintf(
                        "verify_callback: EXPIRED cert but PERMITTED at depth: %d \n\t"
                        "issuer  = %s\n\t"
                        "subject = %s\n\t"
                        "err %d:%s\n",
                        depth, szdbgdata1, szdbgdata2, err, X509_verify_cert_error_string(err));

                    /* Set Status to OK*/
                    status = 1;
                } else if (pPermitExpiredCerts != NULL && *pPermitExpiredCerts == OSSL_EXPIRED_WARN) {
                    LogMsg(0, RS_RET_CERT_EXPIRED, LOG_WARNING,
                           "Certificate EXPIRED warning at depth: %d \n\t"
                           "issuer  = %s\n\t"
                           "subject = %s\n\t"
                           "err %d:%s peer '%s'",
                           depth, szdbgdata1, szdbgdata2, err, X509_verify_cert_error_string(err), fromHost);

                    /* Set Status to OK*/
                    status = 1;
                } else /* also default - if (pPermitExpiredCerts == OSSL_EXPIRED_DENY)*/ {
                    LogMsg(0, RS_RET_CERT_EXPIRED, LOG_ERR,
                           "Certificate EXPIRED at depth: %d \n\t"
                           "issuer  = %s\n\t"
                           "subject = %s\n\t"
                           "err %d:%s\n\t"
                           "not permitted to talk to peer '%s', certificate invalid: "
                           "certificate expired",
                           depth, szdbgdata1, szdbgdata2, err, X509_verify_cert_error_string(err), fromHost);
                }
            } else if (err == X509_V_ERR_CERT_REVOKED) {
                LogMsg(0, RS_RET_CERT_REVOKED, LOG_ERR,
                       "Certificate REVOKED at depth: %d \n\t"
                       "issuer  = %s\n\t"
                       "subject = %s\n\t"
                       "err %d:%s\n\t"
                       "not permitted to talk to peer '%s', certificate invalid: "
                       "certificate revoked",
                       depth, szdbgdata1, szdbgdata2, err, X509_verify_cert_error_string(err), fromHost);
            } else {
                /* all other error codes cause failure */
                LogMsg(0, RS_RET_NO_ERRCODE, LOG_ERR,
                       "Certificate error at depth: %d \n\t"
                       "issuer  = %s\n\t"
                       "subject = %s\n\t"
                       "err %d:%s - peer '%s'",
                       depth, szdbgdata1, szdbgdata2, err, X509_verify_cert_error_string(err), fromHost);
            }
        } else {
            /* do not verify certs in ANON mode, just log into debug */
            dbgprintf(
                "verify_callback: Certificate validation DISABLED but Error at depth: %d \n\t"
                "issuer  = %s\n\t"
                "subject = %s\n\t"
                "err %d:%s\n",
                depth, szdbgdata1, szdbgdata2, err, X509_verify_cert_error_string(err));

            /* Set Status to OK*/
            status = 1;
        }
        free(fromHost);
    }

    if (status == 0) return 0; /* Verification failed */

#ifndef ENABLE_WOLFSSL
    /* OCSP revocation checking is not available in WolfSSL builds */

    /*
     * Certificate revocation checks (only if enabled via StreamDriver.TlsRevocationCheck)
     */
    X509 *cert = X509_STORE_CTX_get_current_cert(store);
    SSL *ssl = X509_STORE_CTX_get_ex_data(store, SSL_get_ex_data_X509_STORE_CTX_idx());

    /* Check if revocation checking is enabled
     * Note: Using index 3 to avoid collision with imdtls which uses index 2
     * Index allocation: 0=pTcp, 1=permitExpiredCerts, 2=imdtls instance, 3=revocationCheck
     */
    int *pTlsRevocationCheck = (int *)SSL_get_ex_data(ssl, 3);
    int tlsRevocationCheck = (pTlsRevocationCheck != NULL) ? *pTlsRevocationCheck : 0;

    if (tlsRevocationCheck == 0) {
        /* Revocation checking is disabled, skip OCSP/CRL checks */
        return status;
    }

    STACK_OF(X509) *untrusted_peer_certs = X509_STORE_CTX_get1_chain(store);
    SSL_CTX *ctx = SSL_get_SSL_CTX(ssl);
    int ret, is_revoked = 0;
    const char *device = NULL;

    /* Try to get device name from nsd object if available */
    /* Note: device binding is not currently supported in the refactored code */
    /* This is left here for future enhancement */

    /* 1. OCSP with caching and non-blocking I/O */
    /* Note: OCSP checks use non-blocking sockets with OCSP_TIMEOUT (5 sec) per responder.
     * Results are cached to minimize network I/O on subsequent TLS handshakes.
     * See: https://github.com/rsyslog/rsyslog/issues/6469
     */
    ret = ocsp_check(cert, untrusted_peer_certs, ctx, device, &is_revoked);
    if (ret == 1) {
        /* Status is OK */
        status = 1;
        goto done;
    } else if (ret == 2) {
        /* No OCSP URL, give CRL a chance */
        status = 1;
    } else if (is_revoked) {
        /* Cert is revoked, fail verification */
        status = 0;
        goto done;
    } else {
        /* If OCSP failed, but cert was not revoked, then the Status might be still OK.
         * Try alternative sources (e.g. CRL), in compliance with RFC 6960, Chapter 2.2, Page 7-8.
         */
        status = 0;
    }

    /* 2. CRL */

    /* CRL support is not implemented.
     * This stub will fail if the cert holds a CRL Distribution Point. */
    ret = crl_check(cert, &is_revoked);
    if (ret == 1) {
        /* Status is OK */
        status = 1;
    } else if (ret == 2) {
        /* No CRL, keep the existing status */
        ;
    } else {
        /* Cert is revoked, or check failed -> fail verification */
        status = 0;
    }

done:
    if (untrusted_peer_certs) sk_X509_pop_free(untrusted_peer_certs, X509_free);
#endif /* !ENABLE_WOLFSSL */

    return status;
}


#ifndef ENABLE_WOLFSSL
    #if OPENSSL_VERSION_NUMBER >= 0x30000000L && !defined(LIBRESSL_VERSION_NUMBER)
static long RSYSLOG_BIO_debug_callback_ex(BIO *bio,
                                          int cmd,
                                          const char __attribute__((unused)) * argp,
                                          size_t __attribute__((unused)) len,
                                          int argi,
                                          long __attribute__((unused)) argl,
                                          int ret,
                                          size_t __attribute__((unused)) * processed)
    #else
static long RSYSLOG_BIO_debug_callback(
    BIO *bio, int cmd, const char __attribute__((unused)) * argp, int argi, long __attribute__((unused)) argl, long ret)
    #endif
{
    long ret2 = ret;
    long r = 1;
    if (BIO_CB_RETURN & cmd) r = ret;
    dbgprintf("openssl debugmsg: BIO[%p]: ", (void *)bio);
    switch (cmd) {
        case BIO_CB_FREE:
            dbgprintf("Free - %s\n", RSYSLOG_BIO_method_name(bio));
            break;
    /* Disabled due API changes for OpenSSL 1.1.0+ */
    #if OPENSSL_VERSION_NUMBER < 0x10100000L
        case BIO_CB_READ:
            if (bio->method->type & BIO_TYPE_DESCRIPTOR)
                dbgprintf("read(%d,%lu) - %s fd=%d\n", RSYSLOG_BIO_number_read(bio), (unsigned long)argi,
                          RSYSLOG_BIO_method_name(bio), RSYSLOG_BIO_number_read(bio));
            else
                dbgprintf("read(%d,%lu) - %s\n", RSYSLOG_BIO_number_read(bio), (unsigned long)argi,
                          RSYSLOG_BIO_method_name(bio));
            break;
        case BIO_CB_WRITE:
            if (bio->method->type & BIO_TYPE_DESCRIPTOR)
                dbgprintf("write(%d,%lu) - %s fd=%d\n", RSYSLOG_BIO_number_written(bio), (unsigned long)argi,
                          RSYSLOG_BIO_method_name(bio), RSYSLOG_BIO_number_written(bio));
            else
                dbgprintf("write(%d,%lu) - %s\n", RSYSLOG_BIO_number_written(bio), (unsigned long)argi,
                          RSYSLOG_BIO_method_name(bio));
            break;
    #else
        case BIO_CB_READ:
            dbgprintf("read %s\n", RSYSLOG_BIO_method_name(bio));
            break;
        case BIO_CB_WRITE:
            dbgprintf("write %s\n", RSYSLOG_BIO_method_name(bio));
            break;
    #endif
        case BIO_CB_PUTS:
            dbgprintf("puts() - %s\n", RSYSLOG_BIO_method_name(bio));
            break;
        case BIO_CB_GETS:
            dbgprintf("gets(%lu) - %s\n", (unsigned long)argi, RSYSLOG_BIO_method_name(bio));
            break;
        case BIO_CB_CTRL:
            dbgprintf("ctrl(%lu) - %s\n", (unsigned long)argi, RSYSLOG_BIO_method_name(bio));
            break;
        case BIO_CB_RETURN | BIO_CB_READ:
            dbgprintf("read return %ld\n", ret2);
            break;
        case BIO_CB_RETURN | BIO_CB_WRITE:
            dbgprintf("write return %ld\n", ret2);
            break;
        case BIO_CB_RETURN | BIO_CB_GETS:
            dbgprintf("gets return %ld\n", ret2);
            break;
        case BIO_CB_RETURN | BIO_CB_PUTS:
            dbgprintf("puts return %ld\n", ret2);
            break;
        case BIO_CB_RETURN | BIO_CB_CTRL:
            dbgprintf("ctrl return %ld\n", ret2);
            break;
        default:
            dbgprintf("bio callback - unknown type (%d)\n", cmd);
            break;
    }

    return (r);
}
#endif /* !ENABLE_WOLFSSL */

#if OPENSSL_VERSION_NUMBER >= 0x10100000L && !defined(ENABLE_WOLFSSL)
// Requires at least OpenSSL v1.1.1
PRAGMA_DIAGNOSTIC_PUSH
PRAGMA_IGNORE_Wunused_parameter static int net_ossl_generate_cookie(SSL *ssl,
                                                                    unsigned char *cookie,
                                                                    unsigned int *cookie_len) {
    unsigned char result[EVP_MAX_MD_SIZE];
    unsigned int resultlength;
    unsigned char *sslHello;
    unsigned int length;

    sslHello = (unsigned char *)"rsyslog";
    length = strlen((char *)sslHello);

    // Generate the cookie using SHA256 hash
    if (!EVP_Digest(sslHello, length, result, &resultlength, EVP_sha256(), NULL)) {
        return 0;
    }

    memcpy(cookie, result, resultlength);
    *cookie_len = resultlength;
    dbgprintf("net_ossl_generate_cookie: generate cookie SUCCESS\n");

    return 1;
}
PRAGMA_DIAGNOSTIC_POP

static int net_ossl_verify_cookie(SSL *ssl, const unsigned char *cookie, unsigned int cookie_len) {
    unsigned char cookie_gen[EVP_MAX_MD_SIZE];
    unsigned int cookie_gen_len;

    // Generate a cookie using the same method as in net_ossl_generate_cookie
    if (!net_ossl_generate_cookie(ssl, cookie_gen, &cookie_gen_len)) {
        dbgprintf("net_ossl_verify_cookie: generate cookie FAILED\n");
        return 0;
    }

    // Check if the generated cookie matches the cookie received
    if (cookie_len == cookie_gen_len && memcmp(cookie, cookie_gen, cookie_len) == 0) {
        dbgprintf("net_ossl_verify_cookie: compare cookie SUCCESS\n");
        return 1;
    }

    dbgprintf("net_ossl_verify_cookie: compare cookie FAILED\n");
    return 0;
}

static rsRetVal net_ossl_init_engine(__attribute__((unused)) net_ossl_t *pThis) {
    // OpenSSL Engine Support feature relies on an outdated version of OpenSSL and is
    // strictly experimental. No support or guarantees are provided. Use at your own risk.
    DEFiRet;
    #ifndef OPENSSL_NO_ENGINE
    const char *engine_id = NULL;
    const char *engine_name = NULL;

    PRAGMA_DIAGNOSTIC_PUSH
    PRAGMA_IGNORE_Wdeprecated_declarations
        // Get the default RSA engine
        ENGINE *default_engine = ENGINE_get_default_RSA();
    if (default_engine) {
        engine_id = ENGINE_get_id(default_engine);
        engine_name = ENGINE_get_name(default_engine);
        DBGPRINTF("net_ossl_init_engine: Default RSA Engine: ID = %s, Name = %s\n", engine_id, engine_name);

        // Free the engine reference when done
        ENGINE_free(default_engine);
    } else {
        DBGPRINTF("net_ossl_init_engine: No default RSA Engine set.\n");
    }

    /* Setting specific Engine */
    if (runConf != NULL && glbl.GetDfltOpensslEngine(runConf) != NULL) {
        default_engine = ENGINE_by_id((char *)glbl.GetDfltOpensslEngine(runConf));
        if (default_engine && ENGINE_init(default_engine)) {
            /* engine initialised */
            ENGINE_set_default_DSA(default_engine);
            ENGINE_set_default_ciphers(default_engine);

            /* Switch to Engine */
            DBGPRINTF("net_ossl_init_engine: Changed default Engine to %s\n", glbl.GetDfltOpensslEngine(runConf));

            /* Release the functional reference from ENGINE_init() */
            ENGINE_finish(default_engine);
        } else {
            LogError(0, RS_RET_VALUE_NOT_SUPPORTED,
                     "error: ENGINE_init failed to load Engine '%s'"
                     "ossl netstream driver",
                     glbl.GetDfltOpensslEngine(runConf));
            net_ossl_lastOpenSSLErrorMsg(NULL, 0, NULL, LOG_ERR, "net_ossl_init_engine", "ENGINE_init");
        }
        // Free the engine reference when done
        ENGINE_free(default_engine);
    } else {
        DBGPRINTF("net_ossl_init_engine: use openssl default Engine");
    }
    PRAGMA_DIAGNOSTIC_POP
    #else
    DBGPRINTF("net_ossl_init_engine: OpenSSL compiled without ENGINE support - ENGINE support disabled\n");
    #endif /* OPENSSL_NO_ENGINE */

    RETiRet;
}


static rsRetVal net_ossl_ctx_init_cookie(net_ossl_t *pThis) {
    DEFiRet;
    // Set our cookie generation and verification callbacks
    SSL_CTX_set_options(pThis->ctx, SSL_OP_COOKIE_EXCHANGE);
    SSL_CTX_set_cookie_generate_cb(pThis->ctx, net_ossl_generate_cookie);
    SSL_CTX_set_cookie_verify_cb(pThis->ctx, net_ossl_verify_cookie);
    RETiRet;
}
#endif  // OPENSSL_VERSION_NUMBER >= 0x10100000L

/* ------------------------------ end OpenSSL helpers ------------------------------------------*/

/* ------------------------------ OpenSSL Callback set helpers ---------------------------------*/
void net_ossl_set_ssl_verify_callback(SSL *pSsl, int flags) {
    /* Enable certificate valid checking */
    SSL_set_verify(pSsl, flags, net_ossl_verify_callback);
}

void net_ossl_set_ctx_verify_callback(SSL_CTX *pCtx, int flags) {
    /* Enable certificate valid checking */
    SSL_CTX_set_verify(pCtx, flags, net_ossl_verify_callback);
}

void net_ossl_set_bio_callback(BIO *conn) {
#ifndef ENABLE_WOLFSSL
    #if OPENSSL_VERSION_NUMBER >= 0x30000000L && !defined(LIBRESSL_VERSION_NUMBER)
    BIO_set_callback_ex(conn, RSYSLOG_BIO_debug_callback_ex);
    #else
    BIO_set_callback(conn, RSYSLOG_BIO_debug_callback);
    #endif  // OPENSSL_VERSION_NUMBER >= 0x10100000L
#else
    (void)conn;
#endif
}
/* ------------------------------ End OpenSSL Callback set helpers -----------------------------*/


/* Standard-Constructor */
BEGINobjConstruct(net_ossl) /* be sure to specify the object type also in END macro! */
    DBGPRINTF("net_ossl_construct: [%p]\n", pThis);
    pThis->bReportAuthErr = 1;
    pThis->bSANpriority = 0;
#if OPENSSL_VERSION_NUMBER >= 0x10100000L && !defined(ENABLE_WOLFSSL)
    CHKiRet(net_ossl_init_engine(pThis));
finalize_it:
#endif
ENDobjConstruct(net_ossl)

/* destructor for the net_ossl object */
BEGINobjDestruct(net_ossl) /* be sure to specify the object type also in END and CODESTART macros! */
    CODESTARTobjDestruct(net_ossl);
    DBGPRINTF("net_ossl_destruct: [%p]\n", pThis);
    /* Free SSL obj also if we do not have a session - or are NOT in TLS mode! */
    if (pThis->ssl != NULL) {
        DBGPRINTF("net_ossl_destruct: [%p] FREE pThis->ssl \n", pThis);
        SSL_free(pThis->ssl);
        pThis->ssl = NULL;
    }
    if (pThis->ctx != NULL && !pThis->ctx_is_copy) {
        SSL_CTX_free(pThis->ctx);
        pThis->ctx = NULL;
    }
    free((void *)pThis->pszCAFile);
    free((void *)pThis->pszCRLFile);
    free((void *)pThis->pszKeyFile);
    free((void *)pThis->pszCertFile);
    free((void *)pThis->pszExtraCAFiles);
ENDobjDestruct(net_ossl)

/* queryInterface function */
BEGINobjQueryInterface(net_ossl)
    CODESTARTobjQueryInterface(net_ossl);
    DBGPRINTF("netosslQueryInterface\n");
    if (pIf->ifVersion != net_osslCURR_IF_VERSION) { /* check for current version, increment on each change */
        ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
    }
    pIf->Construct = (rsRetVal(*)(net_ossl_t **))net_osslConstruct;
    pIf->Destruct = (rsRetVal(*)(net_ossl_t **))net_osslDestruct;
    pIf->osslCtxInit = net_ossl_osslCtxInit;
    pIf->osslChkpeername = net_ossl_chkpeername;
    pIf->osslPeerfingerprint = net_ossl_peerfingerprint;
    pIf->osslGetpeercert = net_ossl_getpeercert;
    pIf->osslChkpeercertvalidity = net_ossl_chkpeercertvalidity;
#if OPENSSL_VERSION_NUMBER >= 0x10002000L && !defined(LIBRESSL_VERSION_NUMBER) && !defined(ENABLE_WOLFSSL)
    pIf->osslApplyTlscgfcmd = net_ossl_apply_tlscgfcmd;
#endif  // OPENSSL_VERSION_NUMBER >= 0x10002000L
    pIf->osslSetBioCallback = net_ossl_set_bio_callback;
    pIf->osslSetCtxVerifyCallback = net_ossl_set_ctx_verify_callback;
    pIf->osslSetSslVerifyCallback = net_ossl_set_ssl_verify_callback;
    pIf->osslLastOpenSSLErrorMsg = net_ossl_lastOpenSSLErrorMsg;
#if OPENSSL_VERSION_NUMBER >= 0x10100000L && !defined(ENABLE_WOLFSSL)
    pIf->osslCtxInitCookie = net_ossl_ctx_init_cookie;
    pIf->osslInitEngine = net_ossl_init_engine;
#endif
finalize_it:
ENDobjQueryInterface(net_ossl)


/* exit our class
 */
BEGINObjClassExit(net_ossl, OBJ_IS_CORE_MODULE) /* CHANGE class also in END MACRO! */
    CODESTARTObjClassExit(net_ossl);
    DBGPRINTF("netosslClassExit\n");
    /* release objects we no longer need */
    objRelease(nsd_ptcp, LM_NSD_PTCP_FILENAME);
    objRelease(net, LM_NET_FILENAME);
    objRelease(glbl, CORE_COMPONENT);
    /* shut down OpenSSL */
    osslGlblExit();
ENDObjClassExit(net_ossl)


/* Initialize the net_ossl class. Must be called as the very first method
 * before anything else is called inside this class.
 */
BEGINObjClassInit(net_ossl, 1, OBJ_IS_CORE_MODULE) /* class, version */
    DBGPRINTF("net_osslClassInit\n");
    // request objects we use
    CHKiRet(objUse(glbl, CORE_COMPONENT));
    CHKiRet(objUse(net, LM_NET_FILENAME));
    CHKiRet(objUse(nsd_ptcp, LM_NSD_PTCP_FILENAME));
    // Do global TLS init stuff
    osslGlblInit();
ENDObjClassInit(net_ossl)

/* --------------- here now comes the plumbing that makes as a library module --------------- */

/* vi:set ai:
 */
