/* nsd_mbedtls.c
 *
 * An implementation of the nsd interface for Mbed TLS.
 *
 * Copyright (C) 2007-2023 Rainer Gerhards and Adiscon GmbH.
 * Copyright (C) 2023 CS Group.
 *
 * This file is part of the rsyslog runtime library.
 *
 * The rsyslog runtime library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The rsyslog runtime library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the rsyslog runtime library.	If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 * A copy of the LGPL can be found in the file "COPYING.LESSER" in this distribution.
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <mbedtls/debug.h>
#include <mbedtls/error.h>
#include <mbedtls/oid.h>
#include <mbedtls/version.h>

#include "rsyslog.h"
#include "syslogd-types.h"
#include "module-template.h"
#include "obj.h"
#include "nsd_ptcp.h"
#include "nsd_mbedtls.h"
#include "rsconf.h"
#include "net.h"

MODULE_TYPE_LIB
MODULE_TYPE_KEEP

/* static data */
DEFobjStaticHelpers DEFobjCurrIf(glbl) DEFobjCurrIf(net) DEFobjCurrIf(nsd_ptcp)
/* Mbed TLS debug level (0..5)
 * 5 is the most logs.
 */
#define MBEDTLS_DEBUG_LEVEL 0

#define DEFAULT_MAX_DEPTH 5

#if MBEDTLS_DEBUG_LEVEL > 0
    static void debug(void __attribute__((unused)) * ctx,
                      int __attribute__((unused)) level,
                      const char *file,
                      int line,
                      const char *str) {
    dbgprintf("%s:%04d: %s", file, line, str);
}
#endif

#define logMbedtlsError(err, mbedtls_err)                                       \
    do {                                                                        \
        char error_buf[128];                                                    \
        int aux = (mbedtls_err < 0 ? -mbedtls_err : mbedtls_err);               \
        const char *sig = (mbedtls_err < 0 ? "-" : "");                         \
        mbedtls_strerror(mbedtls_err, error_buf, 100);                          \
        LogError(0, err, "Mbed TLS Error: %s0x%04X - %s", sig, aux, error_buf); \
    } while (0)

/* initialize Mbed TLS credential structure */
static rsRetVal mbedtlsInitCred(nsd_mbedtls_t *const pThis) {
    DEFiRet;
    const uchar *cafile;
    const uchar *crlfile;
    const uchar *keyFile;
    const uchar *certFile;
    int r;

    keyFile = (pThis->pszKeyFile == NULL) ? glbl.GetDfltNetstrmDrvrKeyFile(runConf) : pThis->pszKeyFile;
    if (keyFile != NULL) {
        mbedtls_pk_free(&(pThis->pkey));
        mbedtls_pk_init(&(pThis->pkey));
#if MBEDTLS_VERSION_MAJOR >= 3
        if ((r = mbedtls_pk_parse_keyfile(&(pThis->pkey), (const char *)keyFile, NULL, mbedtls_ctr_drbg_random,
                                          &(pThis->ctr_drbg))) != 0) {
#else
        if ((r = mbedtls_pk_parse_keyfile(&(pThis->pkey), (const char *)keyFile, NULL)) != 0) {
#endif
            logMbedtlsError(RS_RET_NO_ERRCODE, r);
            ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
        }
        pThis->bHaveKey = 1;
    }

    certFile = (pThis->pszCertFile == NULL) ? glbl.GetDfltNetstrmDrvrCertFile(runConf) : pThis->pszCertFile;
    if (certFile != NULL) {
        mbedtls_x509_crt_free(&(pThis->srvcert));
        mbedtls_x509_crt_init(&(pThis->srvcert));
        if ((r = mbedtls_x509_crt_parse_file(&(pThis->srvcert), (const char *)certFile)) != 0) {
            logMbedtlsError(RS_RET_NO_ERRCODE, r);
            ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
        }
        pThis->bHaveCert = 1;
    }

    cafile = (pThis->pszCAFile == NULL) ? glbl.GetDfltNetstrmDrvrCAF(runConf) : pThis->pszCAFile;
    if (cafile != NULL) {
        mbedtls_x509_crt_free(&(pThis->cacert));
        mbedtls_x509_crt_init(&(pThis->cacert));
        if ((r = mbedtls_x509_crt_parse_file(&(pThis->cacert), (const char *)cafile)) != 0) {
            logMbedtlsError(RS_RET_NO_ERRCODE, r);
            ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
        }
        pThis->bHaveCaCert = 1;
    }

    crlfile = (pThis->pszCRLFile == NULL) ? glbl.GetDfltNetstrmDrvrCRLF(runConf) : pThis->pszCRLFile;
    if (crlfile != NULL) {
        mbedtls_x509_crl_free(&(pThis->crl));
        mbedtls_x509_crl_init(&(pThis->crl));
        if ((r = mbedtls_x509_crl_parse_file(&(pThis->crl), (const char *)crlfile)) != 0) {
            logMbedtlsError(RS_RET_NO_ERRCODE, r);
            ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
        }
        pThis->bHaveCrl = 1;
    }

finalize_it:
    if (iRet) {
        LogMsg(0, RS_RET_ERR, LOG_ERR, "nsd mbedtls: error parsing crypto config");
    }

    RETiRet;
}

/* globally initialize Mbed TLS */
static rsRetVal mbedtlsGlblInit(void) {
    DEFiRet;

    dbgprintf("mbedtlsGlblInit: Running Version: '%#010x'\n", MBEDTLS_VERSION_NUMBER);

    RETiRet;
}

static rsRetVal get_custom_string(char **out) {
    DEFiRet;
    struct timeval tv;
    struct tm tm;

    CHKiRet(gettimeofday(&tv, NULL));
    if (localtime_r(&(tv.tv_sec), &tm) == NULL) {
        ABORT_FINALIZE(RS_RET_NO_ERRCODE);
    }
    if (asprintf(out, "nsd_mbedtls-%04d-%02d-%02d %02d:%02d:%02d:%08ld", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                 tm.tm_hour, tm.tm_min, tm.tm_sec, tv.tv_usec) == -1) {
        *out = NULL;
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

finalize_it:
    RETiRet;
}

static rsRetVal mbedtlsInitSession(nsd_mbedtls_t *pThis) {
    DEFiRet;
    char *cust = NULL;

    CHKiRet(get_custom_string(&cust));
    CHKiRet(mbedtls_ctr_drbg_seed(&(pThis->ctr_drbg), mbedtls_entropy_func, &(pThis->entropy),
                                  (const unsigned char *)cust, strlen(cust)));

finalize_it:
    if (iRet != RS_RET_OK) {
        LogError(0, iRet, "mbedtlsInitSession failed to INIT Session");
    }
    free(cust);

    RETiRet;
}

/* globally de-initialize Mbed TLS */
static rsRetVal mbedtlsGlblExit(void) {
    DEFiRet;
    RETiRet;
}

/* end a Mbed TLS session
 * The function checks if we have a session and ends it only if so. So it can
 * always be called, even if there currently is no session.
 */
static rsRetVal mbedtlsEndSess(nsd_mbedtls_t *pThis) {
    DEFiRet;
    int mbedtlsRet;

    if (pThis->bHaveSess) {
        while ((mbedtlsRet = mbedtls_ssl_close_notify(&(pThis->ssl))) != 0) {
            if (mbedtlsRet != MBEDTLS_ERR_SSL_WANT_READ && mbedtlsRet != MBEDTLS_ERR_SSL_WANT_WRITE) break;
        }
        pThis->bHaveSess = 0;
    }

    RETiRet;
}

/* Standard-Constructor */
BEGINobjConstruct(nsd_mbedtls) /* be sure to specify the object type also in END macro! */
    iRet = nsd_ptcp.Construct(&pThis->pTcp);

    mbedtls_ssl_init(&(pThis->ssl));
    mbedtls_ssl_config_init(&(pThis->conf));
    mbedtls_x509_crt_init(&(pThis->cacert));
    mbedtls_x509_crl_init(&(pThis->crl));
    mbedtls_pk_init(&(pThis->pkey));
    mbedtls_x509_crt_init(&(pThis->srvcert));
    mbedtls_ctr_drbg_init(&(pThis->ctr_drbg));
    mbedtls_entropy_init(&(pThis->entropy));
    pThis->bReportAuthErr = 1;

#if MBEDTLS_DEBUG_LEVEL > 0
    mbedtls_debug_set_threshold(MBEDTLS_DEBUG_LEVEL);
#endif

ENDobjConstruct(nsd_mbedtls)

/* destructor for the nsd_mbedtls object */
PROTOTYPEobjDestruct(nsd_mbedtls);
BEGINobjDestruct(nsd_mbedtls) /* be sure to specify the object type also in END and CODESTART macros! */
    CODESTARTobjDestruct(nsd_mbedtls) if (pThis->iMode == 1) {
        mbedtlsEndSess(pThis);
    }

    if (pThis->pTcp != NULL) {
        nsd_ptcp.Destruct(&pThis->pTcp);
    }

    free((void *)pThis->pszCAFile);
    free((void *)pThis->pszCRLFile);
    free((void *)pThis->pszKeyFile);
    free((void *)pThis->pszCertFile);
    free((void *)pThis->anzCipherSuites);
    free((void *)pThis->pszConnectHost);

    mbedtls_entropy_free(&(pThis->entropy));
    mbedtls_ctr_drbg_free(&(pThis->ctr_drbg));
    mbedtls_x509_crt_free(&(pThis->srvcert));
    mbedtls_pk_free(&(pThis->pkey));
    mbedtls_x509_crl_free(&(pThis->crl));
    mbedtls_x509_crt_free(&(pThis->cacert));
    mbedtls_ssl_config_free(&(pThis->conf));
    mbedtls_ssl_free(&(pThis->ssl));

ENDobjDestruct(nsd_mbedtls)

/* Set the driver mode. For us, this has the following meaning:
 * 0 - work in plain tcp mode, without tls (e.g. before a STARTTLS)
 * 1 - work in TLS mode
 * rgerhards, 2008-04-28
 */
static rsRetVal SetMode(nsd_t *const pNsd, const int mode) {
    DEFiRet;
    nsd_mbedtls_t *pThis = (nsd_mbedtls_t *)pNsd;

    ISOBJ_TYPE_assert((pThis), nsd_mbedtls);
    dbgprintf("(tls) mode: %d\n", mode);
    if (mode != 0 && mode != 1) {
        LogError(0, RS_RET_INVALID_DRVR_MODE,
                 "error: driver mode %d not supported by "
                 "mbedtls netstream driver",
                 mode);
        ABORT_FINALIZE(RS_RET_INVALID_DRVR_MODE);
    }

    pThis->iMode = mode;

finalize_it:
    RETiRet;
}

/* Set the authentication mode. For us, the following is supported:
 * anon - no certificate checks whatsoever (discouraged, but supported)
 * x509/certvalid - (just) check certificate validity
 * x509/fingerprint - certificate fingerprint
 * x509/name - cerfificate name check
 * mode == NULL is valid and defaults to x509/name
 * rgerhards, 2008-05-16
 */
static rsRetVal SetAuthMode(nsd_t *pNsd, uchar *mode) {
    DEFiRet;
    nsd_mbedtls_t *pThis = (nsd_mbedtls_t *)pNsd;

    ISOBJ_TYPE_assert((pThis), nsd_mbedtls);
    if (mode == NULL || !strcasecmp((char *)mode, "x509/name")) {
        pThis->authMode = MBEDTLS_AUTH_CERTNAME;
    } else if (!strcasecmp((char *)mode, "x509/fingerprint")) {
        pThis->authMode = MBEDTLS_AUTH_CERTFINGERPRINT;
    } else if (!strcasecmp((char *)mode, "x509/certvalid")) {
        pThis->authMode = MBEDTLS_AUTH_CERTVALID;
    } else if (!strcasecmp((char *)mode, "anon")) {
        pThis->authMode = MBEDTLS_AUTH_CERTANON;
    } else {
        LogError(0, RS_RET_VALUE_NOT_SUPPORTED,
                 "error: authentication mode '%s' not supported by "
                 "mbedtls netstream driver",
                 mode);
        ABORT_FINALIZE(RS_RET_VALUE_NOT_SUPPORTED);
    }

    dbgprintf("SetAuthMode to %s\n", (mode != NULL ? (char *)mode : "NULL"));

finalize_it:
    RETiRet;
}

/* Set the PermitExpiredCerts mode. For us, the following is supported:
 * on - fail if certificate is expired
 * off - ignore expired certificates
 * warn - warn if certificate is expired
 * alorbach, 2018-12-20
 */
static rsRetVal SetPermitExpiredCerts(nsd_t *pNsd, uchar *mode) {
    DEFiRet;
    nsd_mbedtls_t *pThis = (nsd_mbedtls_t *)pNsd;

    ISOBJ_TYPE_assert((pThis), nsd_mbedtls);
    /* default is set to off! */
    if (mode == NULL || !strcasecmp((char *)mode, "off")) {
        pThis->permitExpiredCerts = MBEDTLS_EXPIRED_DENY;
    } else if (!strcasecmp((char *)mode, "warn")) {
        pThis->permitExpiredCerts = MBEDTLS_EXPIRED_WARN;
    } else if (!strcasecmp((char *)mode, "on")) {
        pThis->permitExpiredCerts = MBEDTLS_EXPIRED_PERMIT;
    } else {
        LogError(0, RS_RET_VALUE_NOT_SUPPORTED,
                 "error: permitexpiredcerts mode '%s' not supported by "
                 "mbedtls netstream driver",
                 mode);
        ABORT_FINALIZE(RS_RET_VALUE_NOT_SUPPORTED);
    }

    dbgprintf("SetPermitExpiredCerts: Set Mode %s/%d\n", (mode != NULL ? (char *)mode : "NULL"),
              pThis->permitExpiredCerts);

    /* TODO: clear stored IDs! */

finalize_it:
    RETiRet;
}

/* Set permitted peers. It is depending on the auth mode if this are
 * fingerprints or names. -- rgerhards, 2008-05-19
 */
static rsRetVal SetPermPeers(nsd_t *pNsd, permittedPeers_t *pPermPeers) {
    DEFiRet;
    nsd_mbedtls_t *pThis = (nsd_mbedtls_t *)pNsd;

    ISOBJ_TYPE_assert((pThis), nsd_mbedtls);
    if (pPermPeers == NULL) FINALIZE;

    if (pThis->authMode != MBEDTLS_AUTH_CERTFINGERPRINT && pThis->authMode != MBEDTLS_AUTH_CERTNAME) {
        LogError(0, RS_RET_VALUE_NOT_IN_THIS_MODE,
                 "authentication not supported by "
                 "mbedtls netstream driver in the configured authentication mode - ignored");
        ABORT_FINALIZE(RS_RET_VALUE_NOT_IN_THIS_MODE);
    }

    pThis->pPermPeers = pPermPeers;

finalize_it:
    RETiRet;
}

/* Set the priority string, aka the cipher suite list in
 * decreasing priority order (most important priority first)
 */
static rsRetVal SetGnutlsPriorityString(nsd_t *pNsd, uchar *gnutlsPriorityString) {
    DEFiRet;
    nsd_mbedtls_t *pThis = (nsd_mbedtls_t *)pNsd;
    int nCipherSuiteId;
    char *pCurrentPos;
    char *pSave;
    int count = 0;
    char *str = NULL;
    int *tmp;
    const char *separators = ",; \n";

    ISOBJ_TYPE_assert((pThis), nsd_mbedtls);

    if (gnutlsPriorityString != NULL) {
        CHKmalloc(str = strdup((const char *)gnutlsPriorityString));

        for (pCurrentPos = strtok_r(str, separators, &pSave); pCurrentPos != NULL;
             pCurrentPos = strtok_r(NULL, separators, &pSave)) {
            if (*pCurrentPos == '\0') {
                continue;
            }
            nCipherSuiteId = mbedtls_ssl_get_ciphersuite_id(pCurrentPos);

            if (nCipherSuiteId == 0) {
                LogError(0, RS_RET_VALUE_NOT_SUPPORTED,
                         "SetGnutlsPriorityString: %s "
                         "is not supported",
                         pCurrentPos);
                ABORT_FINALIZE(RS_RET_VALUE_NOT_SUPPORTED);
            }
            CHKmalloc(tmp = realloc(pThis->anzCipherSuites, (count + 2) * sizeof(int)));
            pThis->anzCipherSuites = tmp;
            pThis->anzCipherSuites[count] = nCipherSuiteId;
            pThis->anzCipherSuites[count + 1] = 0;
            ++count;
        }
    }

finalize_it:
    free(str);

    RETiRet;
}

/* Set the driver cert extended key usage check setting
 * 0 - ignore contents of extended key usage
 * 1 - verify that cert contents is compatible with appropriate OID
 * jvymazal, 2019-08-16
 */
static rsRetVal SetCheckExtendedKeyUsage(nsd_t *pNsd, int ChkExtendedKeyUsage) {
    DEFiRet;
    nsd_mbedtls_t *pThis = (nsd_mbedtls_t *)pNsd;

    ISOBJ_TYPE_assert((pThis), nsd_mbedtls);
    if (ChkExtendedKeyUsage != 0 && ChkExtendedKeyUsage != 1) {
        LogError(0, RS_RET_VALUE_NOT_SUPPORTED,
                 "error: driver ChkExtendedKeyUsage %d "
                 "not supported by mbedtls netstream driver",
                 ChkExtendedKeyUsage);
        ABORT_FINALIZE(RS_RET_VALUE_NOT_SUPPORTED);
    }

    dbgprintf("SetCheckExtendedKeyUsage: %d\n", ChkExtendedKeyUsage);
    pThis->dataTypeCheck = ChkExtendedKeyUsage;

finalize_it:
    RETiRet;
}

/* Set the driver name checking strictness
 * 0 - less strict per RFC 5280, section 4.1.2.6 - either SAN or CN match is good
 * 1 - more strict per RFC 6125 - if any SAN present it must match (CN is ignored)
 * jvymazal, 2019-08-16
 */
static rsRetVal SetPrioritizeSAN(nsd_t *pNsd, int prioritizeSan) {
    DEFiRet;
    nsd_mbedtls_t *pThis = (nsd_mbedtls_t *)pNsd;

    ISOBJ_TYPE_assert((pThis), nsd_mbedtls);
    if (prioritizeSan != 0 && prioritizeSan != 1) {
        LogError(0, RS_RET_VALUE_NOT_SUPPORTED,
                 "error: driver prioritizeSan %d "
                 "not supported by mbedtls netstream driver",
                 prioritizeSan);
        ABORT_FINALIZE(RS_RET_VALUE_NOT_SUPPORTED);
    }

    pThis->bSANpriority = prioritizeSan;

finalize_it:
    RETiRet;
}

/* Set the driver tls  verifyDepth
 * alorbach, 2019-12-20
 */
static rsRetVal SetTlsVerifyDepth(nsd_t *pNsd, int verifyDepth) {
    DEFiRet;
    nsd_mbedtls_t *pThis = (nsd_mbedtls_t *)pNsd;

    ISOBJ_TYPE_assert((pThis), nsd_mbedtls);
    pThis->DrvrVerifyDepth = verifyDepth;
    dbgprintf("SetTlsVerifyDepth: %d\n", pThis->DrvrVerifyDepth);

    RETiRet;
}

static rsRetVal SetTlsCAFile(nsd_t *pNsd, const uchar *const caFile) {
    DEFiRet;
    nsd_mbedtls_t *pThis = (nsd_mbedtls_t *)pNsd;

    ISOBJ_TYPE_assert((pThis), nsd_mbedtls);

    free((void *)pThis->pszCAFile);

    if (caFile == NULL) {
        pThis->pszCAFile = NULL;
    } else {
        CHKmalloc(pThis->pszCAFile = (const uchar *)strdup((const char *)caFile));
    }

finalize_it:
    RETiRet;
}

static rsRetVal SetTlsCRLFile(nsd_t *pNsd, const uchar *const crlFile) {
    DEFiRet;
    nsd_mbedtls_t *pThis = (nsd_mbedtls_t *)pNsd;

    ISOBJ_TYPE_assert((pThis), nsd_mbedtls);

    free((void *)pThis->pszCRLFile);

    if (crlFile == NULL) {
        pThis->pszCRLFile = NULL;
    } else {
        CHKmalloc(pThis->pszCRLFile = (const uchar *)strdup((const char *)crlFile));
    }

finalize_it:
    RETiRet;
}

static rsRetVal SetTlsKeyFile(nsd_t *pNsd, const uchar *const pszFile) {
    DEFiRet;
    nsd_mbedtls_t *pThis = (nsd_mbedtls_t *)pNsd;

    ISOBJ_TYPE_assert((pThis), nsd_mbedtls);

    free((void *)pThis->pszKeyFile);

    if (pszFile == NULL) {
        pThis->pszKeyFile = NULL;
    } else {
        CHKmalloc(pThis->pszKeyFile = (const uchar *)strdup((const char *)pszFile));
    }

finalize_it:
    RETiRet;
}

static rsRetVal SetTlsCertFile(nsd_t *pNsd, const uchar *const pszFile) {
    DEFiRet;
    nsd_mbedtls_t *pThis = (nsd_mbedtls_t *)pNsd;

    ISOBJ_TYPE_assert((pThis), nsd_mbedtls);

    free((void *)pThis->pszCertFile);

    if (pszFile == NULL) {
        pThis->pszCertFile = NULL;
    } else {
        CHKmalloc(pThis->pszCertFile = (const uchar *)strdup((const char *)pszFile));
    }

finalize_it:
    RETiRet;
}

static rsRetVal SetTlsRevocationCheck(nsd_t *pNsd, int enabled) {
    DEFiRet;
    nsd_mbedtls_t *pThis = (nsd_mbedtls_t *)pNsd;

    ISOBJ_TYPE_assert(pThis, nsd_mbedtls);
    pThis->DrvrTlsRevocationCheck = (enabled != 0) ? 1 : 0;

    if (pThis->DrvrTlsRevocationCheck) {
        LogError(0, RS_RET_VALUE_NOT_SUPPORTED,
                 "error: TLS revocation checking not supported by "
                 "mbedtls netstream driver (mbedTLS does not implement OCSP checking)");
        ABORT_FINALIZE(RS_RET_VALUE_NOT_SUPPORTED);
    }

finalize_it:
    RETiRet;
}

/* Provide access to the underlying OS socket. This is primarily
 * useful for other drivers (like nsd_mbedtls) who utilize ourselfs
 * for some of their functionality. -- rgerhards, 2008-04-18
 */
static rsRetVal SetSock(nsd_t *pNsd, int sock) {
    DEFiRet;
    nsd_mbedtls_t *pThis = (nsd_mbedtls_t *)pNsd;

    ISOBJ_TYPE_assert((pThis), nsd_mbedtls);

    DBGPRINTF("SetSock for [%p]: Setting sock %d\n", pNsd, sock);
    nsd_ptcp.SetSock(pThis->pTcp, sock);

    RETiRet;
}

/* Keep Alive Options
 */
static rsRetVal SetKeepAliveIntvl(nsd_t *pNsd, int keepAliveIntvl) {
    DEFiRet;
    nsd_mbedtls_t *pThis = (nsd_mbedtls_t *)pNsd;

    ISOBJ_TYPE_assert((pThis), nsd_mbedtls);
    assert(keepAliveIntvl >= 0);

    nsd_ptcp.SetKeepAliveIntvl(pThis->pTcp, keepAliveIntvl);

    RETiRet;
}

/* Keep Alive Options
 */
static rsRetVal SetKeepAliveProbes(nsd_t *pNsd, int keepAliveProbes) {
    DEFiRet;
    nsd_mbedtls_t *pThis = (nsd_mbedtls_t *)pNsd;

    ISOBJ_TYPE_assert((pThis), nsd_mbedtls);
    assert(keepAliveProbes >= 0);

    nsd_ptcp.SetKeepAliveProbes(pThis->pTcp, keepAliveProbes);

    RETiRet;
}

/* Keep Alive Options
 */
static rsRetVal SetKeepAliveTime(nsd_t *pNsd, int keepAliveTime) {
    DEFiRet;
    nsd_mbedtls_t *pThis = (nsd_mbedtls_t *)pNsd;

    ISOBJ_TYPE_assert((pThis), nsd_mbedtls);
    assert(keepAliveTime >= 0);

    nsd_ptcp.SetKeepAliveTime(pThis->pTcp, keepAliveTime);

    RETiRet;
}

/* abort a connection. This is meant to be called immediately
 * before the Destruct call. -- rgerhards, 2008-03-24
 */
static rsRetVal Abort(nsd_t *pNsd) {
    nsd_mbedtls_t *pThis = (nsd_mbedtls_t *)pNsd;
    DEFiRet;

    ISOBJ_TYPE_assert((pThis), nsd_mbedtls);

    if (pThis->iMode == 0) {
        nsd_ptcp.Abort(pThis->pTcp);
    }

    RETiRet;
}

/* Allow Mbed TLS to read data from the network.
 */
static int mbedtlsNetRecv(void *ctx, unsigned char *buf, size_t len) {
    nsd_mbedtls_t *pThis = (nsd_mbedtls_t *)ctx;
    ssize_t slen = len;
    int oserr;
    DEFiRet;

    slen = recv(pThis->sock, buf, len, 0);
    oserr = errno;

    if (slen < 0) {
        switch (oserr) {
            case EAGAIN:
#if EAGAIN != EWOULDBLOCK
            case EWOULDBLOCK:
#endif
            case EINTR:
                ABORT_FINALIZE(MBEDTLS_ERR_SSL_WANT_READ);
            case EPIPE:
            case ECONNRESET:
                ABORT_FINALIZE(MBEDTLS_ERR_NET_CONN_RESET);
            default:
                ABORT_FINALIZE(MBEDTLS_ERR_NET_RECV_FAILED);
        }
    }
    iRet = slen;

finalize_it:
    RETiRet;
}

/* Allow Mbed TLS to write data to the network.
 */
static int mbedtlsNetSend(void *ctx, const unsigned char *buf, size_t len) {
    nsd_mbedtls_t *pThis = (nsd_mbedtls_t *)ctx;
    ssize_t slen = len;
    DEFiRet;

    iRet = nsd_ptcp.Send(pThis->pTcp, (unsigned char *)buf, &slen);

    if (iRet < RS_RET_OK) {
        ABORT_FINALIZE(MBEDTLS_ERR_NET_SEND_FAILED);
    }

    iRet = slen;

finalize_it:
    RETiRet;
}

static int mbedtlsAuthMode(nsd_mbedtls_t *const pThis) {
    int mode;

    switch (pThis->authMode) {
        case MBEDTLS_AUTH_CERTNAME:
        case MBEDTLS_AUTH_CERTVALID:
        case MBEDTLS_AUTH_CERTFINGERPRINT:
            if (pThis->dataTypeCheck == MBEDTLS_NONE)
                mode = MBEDTLS_SSL_VERIFY_OPTIONAL;
            else
                mode = MBEDTLS_SSL_VERIFY_REQUIRED;
            break;
        case MBEDTLS_AUTH_CERTANON:
            mode = MBEDTLS_SSL_VERIFY_NONE;
            break;
        default:
            assert(0); /* this shall not happen! */
            dbgprintf("ERROR: pThis->authMode %d invalid in nsd_mbedtls.c:%d\n", pThis->authMode, __LINE__);
            mode = MBEDTLS_SSL_VERIFY_REQUIRED;
            break;
    }
    return mode;
}

/* Obtain fingerprint of crt in buf of size *len.
 * *len will be filled with actual fingerprint length on output.
 */
static rsRetVal mbedtlsCertFingerprint(const mbedtls_x509_crt *crt, mbedtls_md_type_t mdType, uchar *buf, size_t *len) {
    const mbedtls_md_info_t *mdInfo;
    size_t mdLen = 0;
    DEFiRet;

    mdInfo = mbedtls_md_info_from_type(mdType);
    if (mdInfo == NULL) ABORT_FINALIZE(RS_RET_CODE_ERR);

    mdLen = mbedtls_md_get_size(mdInfo);
    if (*len < mdLen) ABORT_FINALIZE(RS_RET_PROVIDED_BUFFER_TOO_SMALL);

    if (crt == NULL || buf == NULL) ABORT_FINALIZE(RS_RET_CODE_ERR);

    if (mbedtls_md(mdInfo, crt->raw.p, crt->raw.len, buf) != 0) ABORT_FINALIZE(RS_RET_CODE_ERR);

finalize_it:
    *len = mdLen;

    RETiRet;
}

/* Convert a fingerprint to printable data. The  conversion is carried out
 * according IETF I-D syslog-transport-tls-12. The fingerprint string is
 * returned in a new cstr object. It is the caller's responsibility to
 * destruct that object.
 * rgerhards, 2008-05-08
 */
static rsRetVal GenFingerprintStr(uchar *pFingerprint, size_t sizeFingerprint, cstr_t **ppStr, const char *prefix) {
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

static rsRetVal mbedtlsChkPeerFingerprint(nsd_mbedtls_t *pThis, mbedtls_x509_crt *crt) {
    uchar fingerprint[20];
    uchar fingerprintSha256[32];
    size_t size;
    size_t sizeSha256;
    cstr_t *pstrFingerprint = NULL;
    cstr_t *pstrFingerprintSha256 = NULL;
    int bFoundPositiveMatch;
    permittedPeers_t *pPeer;
    DEFiRet;

    ISOBJ_TYPE_assert(pThis, nsd_mbedtls);

    /* obtain the SHA1 fingerprint */
    size = sizeof(fingerprint);
    sizeSha256 = sizeof(fingerprintSha256);
    CHKiRet(mbedtlsCertFingerprint(crt, MBEDTLS_MD_SHA1, fingerprint, &size));
    CHKiRet(mbedtlsCertFingerprint(crt, MBEDTLS_MD_SHA256, fingerprintSha256, &sizeSha256));
    CHKiRet(GenFingerprintStr(fingerprint, size, &pstrFingerprint, "SHA1"));
    CHKiRet(GenFingerprintStr(fingerprintSha256, sizeSha256, &pstrFingerprintSha256, "SHA256"));
    dbgprintf("peer's certificate SHA1 fingerprint: %s\n", cstrGetSzStrNoNULL(pstrFingerprint));
    dbgprintf("peer's certificate SHA256 fingerprint: %s\n", cstrGetSzStrNoNULL(pstrFingerprintSha256));


    /* now search through the permitted peers to see if we can find a permitted one */
    bFoundPositiveMatch = 0;
    pPeer = pThis->pPermPeers;
    while (pPeer != NULL && !bFoundPositiveMatch) {
        if (!rsCStrSzStrCmp(pstrFingerprint, pPeer->pszID, strlen((char *)pPeer->pszID))) {
            dbgprintf("mbedtlsChkPeerFingerprint: peer's certificate SHA1 MATCH found: %s\n", pPeer->pszID);
            bFoundPositiveMatch = 1;
        } else if (!rsCStrSzStrCmp(pstrFingerprintSha256, pPeer->pszID, strlen((char *)pPeer->pszID))) {
            dbgprintf("mbedtlsChkPeerFingerprint: peer's certificate SHA256 MATCH found: %s\n", pPeer->pszID);
            bFoundPositiveMatch = 1;
        } else {
            pPeer = pPeer->pNext;
        }
    }

    if (!bFoundPositiveMatch) {
        dbgprintf("invalid peer fingerprint, not permitted to talk to it\n");
        if (pThis->bReportAuthErr == 1) {
            errno = 0;
            LogError(0, RS_RET_INVALID_FINGERPRINT,
                     "error: peer fingerprint '%s' unknown - we are "
                     "not permitted to talk to it",
                     cstrGetSzStrNoNULL(pstrFingerprint));
            pThis->bReportAuthErr = 0;
        }
        ABORT_FINALIZE(RS_RET_INVALID_FINGERPRINT);
    }

finalize_it:
    if (pstrFingerprint != NULL) cstrDestruct(&pstrFingerprint);
    if (pstrFingerprintSha256 != NULL) cstrDestruct(&pstrFingerprintSha256);
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
static rsRetVal mbedtlsChkOnePeerName(nsd_mbedtls_t *pThis, uchar *pszPeerID, int *pbFoundPositiveMatch) {
    permittedPeers_t *pPeer;
    DEFiRet;

    ISOBJ_TYPE_assert(pThis, nsd_mbedtls);
    assert(pszPeerID != NULL);
    assert(pbFoundPositiveMatch != NULL);

    if (pThis->pPermPeers) { /* do we have configured peer IDs? */
        pPeer = pThis->pPermPeers;
        while (pPeer != NULL) {
            CHKiRet(net.PermittedPeerWildcardMatch(pPeer, pszPeerID, pbFoundPositiveMatch));
            if (*pbFoundPositiveMatch) break;
            pPeer = pPeer->pNext;
        }
    } else {
        /* we do not have configured peer IDs, so we use defaults */
        if (pThis->pszConnectHost && !strcmp((char *)pszPeerID, (char *)pThis->pszConnectHost)) {
            *pbFoundPositiveMatch = 1;
        }
    }

finalize_it:
    RETiRet;
}

/* Get the common name (CN) from a certificate
 * Caller must free() the returned string
 */
static char *mbedtlsCnFromCrt(const mbedtls_x509_crt *crt) {
    const mbedtls_x509_name *name;

    for (name = &crt->subject; name != NULL; name = name->next) {
        if (MBEDTLS_OID_CMP(MBEDTLS_OID_AT_CN, &name->oid) == 0) {
            return strndup((const char *)(name->val.p), name->val.len);
        }
    }
    return NULL;
}

static rsRetVal mbedtlsChkPeerName(nsd_mbedtls_t *pThis, mbedtls_x509_crt *crt) {
    uchar lnBuf[256];
    const mbedtls_x509_sequence *san;
    int bFoundPositiveMatch;
    char *str = NULL;
    int bHaveSAN = 0;
    cstr_t *pStr = NULL;
    DEFiRet;

    bFoundPositiveMatch = 0;
    CHKiRet(rsCStrConstruct(&pStr));

    /* Check SANs */
    for (san = &crt->subject_alt_names; !bFoundPositiveMatch && san != NULL && san->buf.p != NULL; san = san->next) {
        if (san->buf.tag == (MBEDTLS_ASN1_CONTEXT_SPECIFIC | MBEDTLS_X509_SAN_DNS_NAME)) {
            CHKmalloc(str = strndup((const char *)(san->buf.p), san->buf.len));
            bHaveSAN = 1;
            dbgprintf("subject alt dnsName: '%s'\n", str);
            snprintf((char *)lnBuf, sizeof(lnBuf), "DNSname: %s; ", str);
            CHKiRet(rsCStrAppendStr(pStr, lnBuf));
            CHKiRet(mbedtlsChkOnePeerName(pThis, (uchar *)str, &bFoundPositiveMatch));
            free(str);
            str = NULL;
        }
    }

    /* Check also CN only if not configured per stricter RFC 6125 or no SAN present*/
    if (!bFoundPositiveMatch && (!pThis->bSANpriority || !bHaveSAN)) {
        str = mbedtlsCnFromCrt(crt);
        if (str != NULL) { /* NULL if there was no CN present */
            dbgprintf("mbedtls now checking auth for CN '%s'\n", str);
            snprintf((char *)lnBuf, sizeof(lnBuf), "CN: %s; ", str);
            CHKiRet(rsCStrAppendStr(pStr, lnBuf));
            CHKiRet(mbedtlsChkOnePeerName(pThis, (uchar *)str, &bFoundPositiveMatch));
        }
    }

    if (!bFoundPositiveMatch) {
        dbgprintf("invalid peer name, not permitted to talk to it\n");
        if (pThis->bReportAuthErr == 1) {
            cstrFinalize(pStr);
            errno = 0;
            LogError(0, RS_RET_INVALID_FINGERPRINT,
                     "error: peer name not authorized -	"
                     "not permitted to talk to it. Names: %s",
                     cstrGetSzStrNoNULL(pStr));
            pThis->bReportAuthErr = 0;
        }
        ABORT_FINALIZE(RS_RET_INVALID_FINGERPRINT);
    }

finalize_it:
    if (pStr != NULL) rsCStrDestruct(&pStr);
    free(str);
    RETiRet;
}

/* This is a callback fonction to allow extra checks
 * in addition to default verifications already done by Mbed TLS's
 * default processing.
 */
static int verify(void *ctx, mbedtls_x509_crt *crt, int depth, uint32_t *flags) {
    nsd_mbedtls_t *pThis = (nsd_mbedtls_t *)ctx;
    DEFiRet;

    if (depth > (pThis->DrvrVerifyDepth != 0 ? pThis->DrvrVerifyDepth : DEFAULT_MAX_DEPTH))
        ABORT_FINALIZE(MBEDTLS_ERR_X509_FATAL_ERROR);

    if (depth > 0) FINALIZE; /* Nothing more to be done with issuer's certificates */

    /* Mbed TLS API states that if any of the following checks fails,
     * the verify callback should set the corresponding flag (or
     * clear it if the check is not cared about) and return 0.
     * (see mbedtls_x509_crt_verify() documentation)
     */
    if ((*flags) & MBEDTLS_X509_BADCERT_EXPIRED) {
        if (pThis->permitExpiredCerts == MBEDTLS_EXPIRED_DENY) {
            FINALIZE;
        }
        if (pThis->permitExpiredCerts == MBEDTLS_EXPIRED_WARN) {
            LogMsg(0, RS_RET_NO_ERRCODE, LOG_WARNING, "Warning, certificate expired but expired certs are permitted");
        } else {
            dbgprintf(
                "Mbed TLS gives MBEDTLS_X509_BADCERT_EXPIRED"
                ", but expired certs are permitted.\n");
        }
        *flags &= ~MBEDTLS_X509_BADCERT_EXPIRED;
    }

    if (pThis->authMode == MBEDTLS_AUTH_CERTFINGERPRINT) {
        if (mbedtlsChkPeerFingerprint(pThis, crt) != RS_RET_OK) {
            *flags |= MBEDTLS_X509_BADCERT_OTHER;
        }
    } else if (pThis->authMode == MBEDTLS_AUTH_CERTNAME) {
        if (mbedtlsChkPeerName(pThis, crt) != RS_RET_OK) {
            *flags |= MBEDTLS_X509_BADCERT_CN_MISMATCH;
        }
    } /* Other auth modes have already been handled by default processing */

finalize_it:
    RETiRet;
}

static rsRetVal CheckVerifyResult(nsd_mbedtls_t *pThis) {
    DEFiRet;
    uint32_t flags;

    flags = mbedtls_ssl_get_verify_result(&(pThis->ssl));

    if (mbedtlsAuthMode(pThis) != MBEDTLS_SSL_VERIFY_OPTIONAL) FINALIZE;

    if (pThis->dataTypeCheck == MBEDTLS_NONE && (flags & MBEDTLS_X509_BADCERT_EXT_KEY_USAGE)) {
        dbgprintf("Mbed TLS gives MBEDTLS_X509_BADCERT_EXT_KEY_USAGE but check disabled\n");
        flags &= ~MBEDTLS_X509_BADCERT_EXT_KEY_USAGE;
    }
    if (flags != 0) {
        logMbedtlsError(RS_RET_CERT_INVALID, MBEDTLS_ERR_X509_CERT_VERIFY_FAILED);
        ABORT_FINALIZE(RS_RET_CERT_INVALID);
    }

finalize_it:
    if (flags != 0) {
        char vrfy_buf[512];
        if (mbedtls_x509_crt_verify_info(vrfy_buf, sizeof(vrfy_buf), "Mbed TLS verify info : ", flags) < 0)
            LogError(0, RS_RET_PROVIDED_BUFFER_TOO_SMALL, "Can't get Mbed TLS verify info");
        else
            LogError(0, RS_RET_CERT_INVALID, "%s", vrfy_buf);
    }
    RETiRet;
}

static rsRetVal zeroTermIntArrayDup(int *array, int **copy) {
    int c, i;
    int *p = NULL;
    DEFiRet;

    c = 0;
    while (array[c] != 0) ++c;

    CHKmalloc(p = malloc((c + 1) * sizeof(int)));

    for (i = 0; i <= c; ++i) p[i] = array[i];

finalize_it:

    *copy = p;
    RETiRet;
}

/* accept an incoming connection request - here, we do the usual accept
 * handling. TLS specific handling is done thereafter (and if we run in TLS
 * mode at this time).
 * rgerhards, 2008-04-25
 */
static rsRetVal AcceptConnReq(nsd_t *pNsd, nsd_t **ppNew, char *const connInfo) {
    DEFiRet;
    nsd_mbedtls_t *pNew = NULL;
    nsd_mbedtls_t *pThis = (nsd_mbedtls_t *)pNsd;
    int mbedtlsRet;

    ISOBJ_TYPE_assert(pThis, nsd_mbedtls);
    CHKiRet(nsd_mbedtlsConstruct(&pNew));  // TODO: prevent construct/destruct!
    CHKiRet(nsd_ptcp.Destruct(&pNew->pTcp));
    CHKiRet(nsd_ptcp.AcceptConnReq(pThis->pTcp, &pNew->pTcp, connInfo));

    if (pThis->iMode == 0) {
        /* we are in non-TLS mode, so we are done */
        *ppNew = (nsd_t *)pNew;
        FINALIZE;
    }
    /* copy Properties to pnew first */
    pNew->authMode = pThis->authMode;
    pNew->bSANpriority = pThis->bSANpriority;
    pNew->pPermPeers = pThis->pPermPeers;
    pNew->DrvrVerifyDepth = pThis->DrvrVerifyDepth;
    pNew->DrvrTlsRevocationCheck = pThis->DrvrTlsRevocationCheck;
    pNew->dataTypeCheck = pThis->dataTypeCheck;
    pNew->permitExpiredCerts = pThis->permitExpiredCerts;
    if (pThis->anzCipherSuites) CHKiRet(zeroTermIntArrayDup(pThis->anzCipherSuites, &(pNew->anzCipherSuites)));
    if (pThis->pszCertFile) CHKmalloc(pNew->pszCertFile = (const uchar *)strdup((const char *)(pThis->pszCertFile)));
    if (pThis->pszKeyFile) CHKmalloc(pNew->pszKeyFile = (const uchar *)strdup((const char *)(pThis->pszKeyFile)));
    if (pThis->pszCAFile) CHKmalloc(pNew->pszCAFile = (const uchar *)strdup((const char *)(pThis->pszCAFile)));
    if (pThis->pszCRLFile) CHKmalloc(pNew->pszCRLFile = (const uchar *)strdup((const char *)(pThis->pszCRLFile)));
    pNew->iMode = pThis->iMode;

    /* if we reach this point, we are in TLS mode */
    CHKiRet(mbedtlsInitSession(pNew));
    CHKiRet(mbedtlsInitCred(pNew));
    CHKiRet(mbedtls_ssl_config_defaults(&(pNew->conf), MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM,
                                        MBEDTLS_SSL_PRESET_DEFAULT));

    mbedtls_ssl_conf_rng(&(pNew->conf), mbedtls_ctr_drbg_random, &(pNew->ctr_drbg));
    mbedtls_ssl_conf_authmode(&(pNew->conf), mbedtlsAuthMode(pNew));
    mbedtls_ssl_conf_ca_chain(&(pNew->conf), pNew->bHaveCaCert ? &(pNew->cacert) : NULL,
                              pNew->bHaveCrl ? &(pNew->crl) : NULL);

    mbedtls_ssl_conf_verify(&(pNew->conf), verify, pNew);

    if (pNew->bHaveKey && pNew->bHaveCert)
        CHKiRet(mbedtls_ssl_conf_own_cert(&(pNew->conf), &(pNew->srvcert), &(pNew->pkey)));

#if MBEDTLS_DEBUG_LEVEL > 0
    mbedtls_ssl_conf_dbg(&(pNew->conf), debug, stdout);
#endif
    if (pNew->anzCipherSuites != NULL) mbedtls_ssl_conf_ciphersuites(&(pNew->conf), pNew->anzCipherSuites);

    CHKiRet(mbedtls_ssl_setup(&(pNew->ssl), &(pNew->conf)));

    // peer id will be checked in verify() callback
    CHKiRet(mbedtls_ssl_set_hostname(&(pNew->ssl), NULL));

    CHKiRet(nsd_ptcp.GetSock(pNew->pTcp, &(pNew->sock)));
    mbedtls_ssl_set_bio(&(pNew->ssl), pNew, mbedtlsNetSend, mbedtlsNetRecv, NULL);

    mbedtlsRet = mbedtls_ssl_handshake(&(pNew->ssl));
    if (mbedtlsRet != 0 && mbedtlsRet != MBEDTLS_ERR_SSL_WANT_READ && mbedtlsRet != MBEDTLS_ERR_SSL_WANT_WRITE) {
        logMbedtlsError(RS_RET_TLS_HANDSHAKE_ERR, mbedtlsRet);
        ABORT_FINALIZE(RS_RET_TLS_HANDSHAKE_ERR);
    }
    CHKiRet(CheckVerifyResult(pNew));

    pNew->bHaveSess = 1;
    *ppNew = (nsd_t *)pNew;

finalize_it:
    if (iRet != RS_RET_OK) {
        if (pNew != NULL) nsd_mbedtlsDestruct(&pNew);
    }
    RETiRet;
}

/* receive data from a tcp socket
 * The lenBuf parameter must contain the max buffer size on entry and contains
 * the number of octets read on exit. This function
 * never blocks, not even when called on a blocking socket. That is important
 * for client sockets, which are set to block during send, but should not
 * block when trying to read data. -- rgerhards, 2008-03-17
 */
static rsRetVal Rcv(nsd_t *pNsd, uchar *pBuf, ssize_t *pLenBuf, int *const oserr, unsigned *const nextIODirection) {
    DEFiRet;
    nsd_mbedtls_t *pThis = (nsd_mbedtls_t *)pNsd;
    ISOBJ_TYPE_assert(pThis, nsd_mbedtls);
    int n = 0;

    if (pThis->bAbortConn) ABORT_FINALIZE(RS_RET_CONNECTION_ABORTREQ);

    if (pThis->iMode == 0) {
        CHKiRet(nsd_ptcp.Rcv(pThis->pTcp, pBuf, pLenBuf, oserr, nextIODirection));
        FINALIZE;
    }

    /* --- in TLS mode now --- */
    n = mbedtls_ssl_read(&(pThis->ssl), pBuf, *pLenBuf);

    if (n == MBEDTLS_ERR_SSL_WANT_READ || n == MBEDTLS_ERR_SSL_WANT_WRITE) ABORT_FINALIZE(RS_RET_RETRY);

    if (n == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) ABORT_FINALIZE(RS_RET_CLOSED);

    if (n < 0) {
        logMbedtlsError(RS_RET_RCV_ERR, n);
        ABORT_FINALIZE(RS_RET_RCV_ERR);
    }

    if (n == 0) ABORT_FINALIZE(RS_RET_RETRY);

    CHKiRet(CheckVerifyResult(pThis));

    *pLenBuf = n;

finalize_it:
    dbgprintf("mbedtlsRcv return. nsd %p, iRet %d, lenRcvBuf %ld\n", pThis, iRet, (long)(*pLenBuf));

    if (iRet != RS_RET_OK && iRet != RS_RET_RETRY) *pLenBuf = 0;

    RETiRet;
}

/* send a buffer. On entry, pLenBuf contains the number of octets to
 * write. On exit, it contains the number of octets actually written.
 * If this number is lower than on entry, only a partial buffer has
 * been written.
 * rgerhards, 2008-03-19
 */
static rsRetVal Send(nsd_t *pNsd, uchar *pBuf, ssize_t *pLenBuf) {
    int iSent;
    nsd_mbedtls_t *pThis = (nsd_mbedtls_t *)pNsd;
    DEFiRet;
    ISOBJ_TYPE_assert(pThis, nsd_mbedtls);

    if (pThis->bAbortConn) ABORT_FINALIZE(RS_RET_CONNECTION_ABORTREQ);

    if (pThis->iMode == 0) {
        CHKiRet(nsd_ptcp.Send(pThis->pTcp, pBuf, pLenBuf));
        FINALIZE;
    }

    /* in TLS mode now */
    iSent = mbedtls_ssl_write(&(pThis->ssl), pBuf, *pLenBuf);
    if (iSent == MBEDTLS_ERR_SSL_WANT_READ || iSent == MBEDTLS_ERR_SSL_WANT_WRITE) {
        ABORT_FINALIZE(RS_RET_RETRY);
    } else if (iSent <= 0) {
        logMbedtlsError(RS_RET_NO_ERRCODE, iSent);
        ABORT_FINALIZE(RS_RET_IO_ERROR);
    }
    CHKiRet(CheckVerifyResult(pThis));

    *pLenBuf = iSent;

finalize_it:
    RETiRet;
}

/* Enable KEEPALIVE handling on the socket.
 * rgerhards, 2009-06-02
 */
static rsRetVal EnableKeepAlive(nsd_t *pNsd) {
    nsd_mbedtls_t *pThis = (nsd_mbedtls_t *)pNsd;
    ISOBJ_TYPE_assert(pThis, nsd_mbedtls);
    return nsd_ptcp.EnableKeepAlive(pThis->pTcp);
}

/* open a connection to a remote host (server).
 */
static rsRetVal Connect(nsd_t *pNsd, int family, uchar *port, uchar *host, char *device) {
    DEFiRet;
    nsd_mbedtls_t *pThis = (nsd_mbedtls_t *)pNsd;
    int mbedtlsRet;

    dbgprintf("Connect to %s:%s\n", host, port);

    ISOBJ_TYPE_assert(pThis, nsd_mbedtls);
    assert(port != NULL);
    assert(host != NULL);

    CHKiRet(mbedtlsInitSession(pThis));
    CHKiRet(mbedtlsInitCred(pThis));
    CHKiRet(nsd_ptcp.Connect(pThis->pTcp, family, port, host, device));

    if (pThis->iMode == 0) FINALIZE;

    /* we reach this point if in TLS mode */
    CHKiRet(mbedtls_ssl_config_defaults(&(pThis->conf), MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                        MBEDTLS_SSL_PRESET_DEFAULT));

    mbedtls_ssl_conf_rng(&(pThis->conf), mbedtls_ctr_drbg_random, &(pThis->ctr_drbg));
    mbedtls_ssl_conf_authmode(&(pThis->conf), mbedtlsAuthMode(pThis));
    mbedtls_ssl_conf_ca_chain(&(pThis->conf), pThis->bHaveCaCert ? &(pThis->cacert) : NULL,
                              pThis->bHaveCrl ? &(pThis->crl) : NULL);

    mbedtls_ssl_conf_verify(&(pThis->conf), verify, pThis);

    if (pThis->bHaveKey && pThis->bHaveCert)
        CHKiRet(mbedtls_ssl_conf_own_cert(&(pThis->conf), &(pThis->srvcert), &(pThis->pkey)));

#if MBEDTLS_DEBUG_LEVEL > 0
    mbedtls_ssl_conf_dbg(&(pThis->conf), debug, stdout);
#endif
    if (pThis->anzCipherSuites != NULL) mbedtls_ssl_conf_ciphersuites(&(pThis->conf), pThis->anzCipherSuites);

    CHKiRet(mbedtls_ssl_setup(&(pThis->ssl), &(pThis->conf)));

    // peer id will be checked in verify() callback
    CHKiRet(mbedtls_ssl_set_hostname(&(pThis->ssl), NULL));
    CHKmalloc(pThis->pszConnectHost = (uchar *)strdup((char *)host));

    CHKiRet(nsd_ptcp.GetSock(pThis->pTcp, &(pThis->sock)));
    mbedtls_ssl_set_bio(&(pThis->ssl), pThis, mbedtlsNetSend, mbedtlsNetRecv, NULL);

    mbedtlsRet = mbedtls_ssl_handshake(&(pThis->ssl));
    if (mbedtlsRet != 0 && mbedtlsRet != MBEDTLS_ERR_SSL_WANT_READ && mbedtlsRet != MBEDTLS_ERR_SSL_WANT_WRITE) {
        logMbedtlsError(RS_RET_TLS_HANDSHAKE_ERR, mbedtlsRet);
        ABORT_FINALIZE(RS_RET_TLS_HANDSHAKE_ERR);
    }
    CHKiRet(CheckVerifyResult(pThis));

    pThis->bHaveSess = 1;

finalize_it:
    if (iRet != RS_RET_OK) {
        free(pThis->pszConnectHost);
        pThis->pszConnectHost = NULL;
    }

    RETiRet;
}

static rsRetVal ATTR_NONNULL(1, 3, 5) LstnInit(netstrms_t *pNS,
                                               void *pUsr,
                                               rsRetVal (*fAddLstn)(void *, netstrm_t *),
                                               const int iSessMax,
                                               const tcpLstnParams_t *cnf_params) {
    DEFiRet;

    iRet = nsd_ptcp.LstnInit(pNS, pUsr, fAddLstn, iSessMax, cnf_params);

    RETiRet;
}

/* This function checks if the connection is still alive - well, kind of...
 * This is a dummy here. For details, check function common in ptcp driver.
 * rgerhards, 2008-06-09
 */
static rsRetVal CheckConnection(nsd_t *pNsd) {
    nsd_mbedtls_t *pThis = (nsd_mbedtls_t *)pNsd;
    ISOBJ_TYPE_assert(pThis, nsd_mbedtls);

    dbgprintf("CheckConnection for %p\n", pNsd);
    return nsd_ptcp.CheckConnection(pThis->pTcp);
}

/* Provide access to the underlying OS socket.
 */
static rsRetVal GetSock(nsd_t *pNsd, int *pSock) {
    nsd_mbedtls_t *pThis = (nsd_mbedtls_t *)pNsd;
    ISOBJ_TYPE_assert(pThis, nsd_mbedtls);
    return nsd_ptcp.GetSock(pThis->pTcp, pSock);
}

/* get the remote hostname. The returned hostname must be freed by the caller.
 * rgerhards, 2008-04-25
 */
static rsRetVal GetRemoteHName(nsd_t *pNsd, uchar **ppszHName) {
    DEFiRet;
    nsd_mbedtls_t *pThis = (nsd_mbedtls_t *)pNsd;
    ISOBJ_TYPE_assert(pThis, nsd_mbedtls);
    iRet = nsd_ptcp.GetRemoteHName(pThis->pTcp, ppszHName);
    RETiRet;
}


/* Provide access to the sockaddr_storage of the remote peer. This
 * is needed by the legacy ACL system. --- gerhards, 2008-12-01
 */
static rsRetVal GetRemAddr(nsd_t *pNsd, struct sockaddr_storage **ppAddr) {
    DEFiRet;
    nsd_mbedtls_t *pThis = (nsd_mbedtls_t *)pNsd;
    ISOBJ_TYPE_assert(pThis, nsd_mbedtls);
    iRet = nsd_ptcp.GetRemAddr(pThis->pTcp, ppAddr);
    RETiRet;
}

/* get the remote host's IP address. Caller must Destruct the object. */
static rsRetVal GetRemoteIP(nsd_t *pNsd, prop_t **ip) {
    DEFiRet;
    nsd_mbedtls_t *pThis = (nsd_mbedtls_t *)pNsd;
    ISOBJ_TYPE_assert(pThis, nsd_mbedtls);
    iRet = nsd_ptcp.GetRemoteIP(pThis->pTcp, ip);
    RETiRet;
}

/* queryInterface function */
BEGINobjQueryInterface(nsd_mbedtls)
    CODESTARTobjQueryInterface(
        nsd_mbedtls) if (pIf->ifVersion !=
                         nsdCURR_IF_VERSION) { /* check for current version, increment on each change */
        ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
    }

    /* ok, we have the right interface, so let's fill it
     * Please note that we may also do some backwards-compatibility
     * work here (if we can support an older interface version - that,
     * of course, also affects the "if" above).
     */
    pIf->Construct = (rsRetVal(*)(nsd_t **))nsd_mbedtlsConstruct;
    pIf->Destruct = (rsRetVal(*)(nsd_t **))nsd_mbedtlsDestruct;
    pIf->Abort = Abort;
    pIf->LstnInit = LstnInit;
    pIf->AcceptConnReq = AcceptConnReq;
    pIf->Rcv = Rcv;
    pIf->Send = Send;
    pIf->Connect = Connect;
    pIf->GetSock = GetSock;
    pIf->SetSock = SetSock;
    pIf->SetMode = SetMode;
    pIf->SetAuthMode = SetAuthMode;
    pIf->SetPermitExpiredCerts = SetPermitExpiredCerts;
    pIf->SetPermPeers = SetPermPeers;
    pIf->CheckConnection = CheckConnection;
    pIf->GetRemoteHName = GetRemoteHName;
    pIf->GetRemoteIP = GetRemoteIP;
    pIf->GetRemAddr = GetRemAddr;
    pIf->EnableKeepAlive = EnableKeepAlive;
    pIf->SetKeepAliveIntvl = SetKeepAliveIntvl;
    pIf->SetKeepAliveProbes = SetKeepAliveProbes;
    pIf->SetKeepAliveTime = SetKeepAliveTime;
    pIf->SetGnutlsPriorityString = SetGnutlsPriorityString;
    pIf->SetCheckExtendedKeyUsage = SetCheckExtendedKeyUsage;
    pIf->SetPrioritizeSAN = SetPrioritizeSAN;
    pIf->SetTlsVerifyDepth = SetTlsVerifyDepth;
    pIf->SetTlsCAFile = SetTlsCAFile;
    pIf->SetTlsCRLFile = SetTlsCRLFile;
    pIf->SetTlsKeyFile = SetTlsKeyFile;
    pIf->SetTlsCertFile = SetTlsCertFile;
    pIf->SetTlsRevocationCheck = SetTlsRevocationCheck;
finalize_it:
ENDobjQueryInterface(nsd_mbedtls)

/* exit our class
 */
BEGINObjClassExit(nsd_mbedtls, OBJ_IS_LOADABLE_MODULE) /* CHANGE class also in END MACRO! */
    CODESTARTObjClassExit(nsd_mbedtls) mbedtlsGlblExit(); /* shut down Mbed TLS */

    /* release objects we no longer need */
    objRelease(nsd_ptcp, LM_NSD_PTCP_FILENAME);
    objRelease(net, LM_NET_FILENAME);
    objRelease(glbl, CORE_COMPONENT);
ENDObjClassExit(nsd_mbedtls)

/* Initialize the nsd_mbedtls class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINObjClassInit(nsd_mbedtls, 1, OBJ_IS_LOADABLE_MODULE) /* class, version */
    /* request objects we use */
    CHKiRet(objUse(glbl, CORE_COMPONENT));
    CHKiRet(objUse(nsd_ptcp, LM_NSD_PTCP_FILENAME));
    CHKiRet(objUse(net, LM_NET_FILENAME));

    /* now do global TLS init stuff */
    CHKiRet(mbedtlsGlblInit());
ENDObjClassInit(nsd_mbedtls)

/* --------------- here now comes the plumbing that makes as a library module --------------- */

BEGINmodExit
    CODESTARTmodExit nsd_mbedtlsClassExit();
ENDmodExit

BEGINqueryEtryPt
    CODESTARTqueryEtryPt CODEqueryEtryPt_STD_LIB_QUERIES
ENDqueryEtryPt

BEGINmodInit()
    CODESTARTmodInit;
    *ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */

    /* Initialize all classes that are in our module - this includes ourselfs */
    CHKiRet(nsd_mbedtlsClassInit(pModInfo)); /* must be done after tcps_sess, as we use it */
ENDmodInit
