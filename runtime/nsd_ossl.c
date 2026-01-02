/* nsd_ossl.c
 *
 * An implementation of the nsd interface for OpenSSL.
 *
 * Copyright 2018-2025 Adiscon GmbH.
 * Author: Andre Lorbach
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the rsyslog runtime library.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 * A copy of the LGPL can be found in the file "COPYING.LESSER" in this distribution.
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

#include "rsyslog.h"
#include "syslogd-types.h"
#include "module-template.h"
#include "cfsysline.h"
#include "obj.h"
#include "stringbuf.h"
#include "errmsg.h"
#include "net.h"
#include "netstrm.h"
#include "netstrms.h"
#include "datetime.h"
#include "net_ossl.h"  // Include OpenSSL Helpers
#include "nsd_ptcp.h"
#include "nsd_ossl.h"
#include "unicode-helper.h"
#include "rsconf.h"

MODULE_TYPE_LIB
MODULE_TYPE_KEEP;

/* static data */
DEFobjStaticHelpers;
DEFobjCurrIf(glbl) DEFobjCurrIf(net) DEFobjCurrIf(datetime) DEFobjCurrIf(nsd_ptcp) DEFobjCurrIf(net_ossl)

    /* Some prototypes for helper functions used inside openssl driver */
    static rsRetVal applyGnutlsPriorityString(nsd_ossl_t *const pNsd);

/* retry an interrupted OSSL operation */
static rsRetVal doRetry(nsd_ossl_t *pNsd) {
    DEFiRet;
    nsd_ossl_t *pNsdOSSL = (nsd_ossl_t *)pNsd;

    dbgprintf("doRetry: requested retry of %d operation - executing\n", pNsd->rtryCall);

    /* We follow a common scheme here: first, we do the systen call and
     * then we check the result. So far, the result is checked after the
     * switch, because the result check is the same for all calls. Note that
     * this may change once we deal with the read and write calls (but
     * probably this becomes an issue only when we begin to work on TLS
     * for relp). -- rgerhards, 2008-04-30
     */
    switch (pNsd->rtryCall) {
        case osslRtry_handshake:
            dbgprintf("doRetry: start osslHandshakeCheck, nsd: %p\n", pNsd);
            /* Reset retry flag before calling handshake check.
             * If it needs more retries, it will set it again.
             */
            pNsd->rtryCall = osslRtry_None;
            /* Do the handshake again*/
            CHKiRet(osslHandshakeCheck(pNsdOSSL));
            break;
        case osslRtry_recv:
        case osslRtry_None:
        default:
            assert(0); /* this shall not happen! */
            dbgprintf("doRetry: ERROR, pNsd->rtryCall invalid in nsdsel_ossl.c:%d\n", __LINE__);
            break;
    }
finalize_it:
    if (iRet != RS_RET_OK && iRet != RS_RET_CLOSED && iRet != RS_RET_RETRY) pNsd->bAbortConn = 1; /* request abort */
    RETiRet;
}

/*--------------------------------------OpenSSL helpers ------------------------------------------*/
void nsd_ossl_lastOpenSSLErrorMsg(
    nsd_ossl_t const *pThis, const int ret, SSL *ssl, int severity, const char *pszCallSource, const char *pszOsslApi) {
    uchar *fromHost = NULL;
    int errno_store = errno;

    if (pThis != NULL) {
        nsd_ptcp.GetRemoteHName((nsd_t *)pThis->pTcp, &fromHost);
    }

    // Call helper in net_ossl
    net_ossl.osslLastOpenSSLErrorMsg(fromHost, ret, ssl, severity, pszCallSource, pszOsslApi);

    free(fromHost);
    errno = errno_store;
}

#if OPENSSL_VERSION_NUMBER >= 0x30000000L && !defined(LIBRESSL_VERSION_NUMBER)
long BIO_debug_callback_ex(BIO *bio,
                           int cmd,
                           const char __attribute__((unused)) * argp,
                           size_t __attribute__((unused)) len,
                           int argi,
                           long __attribute__((unused)) argl,
                           int ret,
                           size_t __attribute__((unused)) * processed)
#else
long BIO_debug_callback(
    BIO *bio, int cmd, const char __attribute__((unused)) * argp, int argi, long __attribute__((unused)) argl, long ret)
#endif
{
    long ret2 = ret;  // Helper value to avoid printf compile errors long<>int
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

/* try to receive a record from the remote peer. This works with
 * our own abstraction and handles local buffering and EAGAIN.
 * See details on local buffering in Rcv(9 header-comment.
 * This function MUST only be called when the local buffer is
 * empty. Calling it otherwise will cause losss of current buffer
 * data.
 * rgerhards, 2008-06-24
 */
rsRetVal osslRecordRecv(nsd_ossl_t *pThis, unsigned *const nextIODirection) {
    ssize_t lenRcvd;
    DEFiRet;
    int err;

    ISOBJ_TYPE_assert(pThis, nsd_ossl);
    DBGPRINTF("osslRecordRecv: start\n");

    lenRcvd = SSL_read(pThis->pNetOssl->ssl, pThis->pszRcvBuf, NSD_OSSL_MAX_RCVBUF);
    if (lenRcvd > 0) {
        DBGPRINTF("osslRecordRecv: SSL_read received %zd bytes\n", lenRcvd);
        pThis->lenRcvBuf = lenRcvd;
        pThis->ptrRcvBuf = 0;

        /* Check for additional data in SSL buffer */
        int iBytesLeft = SSL_pending(pThis->pNetOssl->ssl);
        if (iBytesLeft > 0) {
            DBGPRINTF("osslRecordRecv: %d Bytes pending after SSL_Read, expand buffer.\n", iBytesLeft);
            /* realloc buffer size and preserve char content */
            char *const newbuf = realloc(pThis->pszRcvBuf, NSD_OSSL_MAX_RCVBUF + iBytesLeft);
            CHKmalloc(newbuf);
            pThis->pszRcvBuf = newbuf;

            /* 2nd read will read missing bytes from the current SSL Packet */
            lenRcvd = SSL_read(pThis->pNetOssl->ssl, pThis->pszRcvBuf + NSD_OSSL_MAX_RCVBUF, iBytesLeft);
            if (lenRcvd > 0) {
                DBGPRINTF("osslRecordRecv: 2nd SSL_read received %zd bytes\n", (NSD_OSSL_MAX_RCVBUF + lenRcvd));
                pThis->lenRcvBuf = NSD_OSSL_MAX_RCVBUF + lenRcvd;
            } else {
                goto sslerr;
            }
        }
    } else {
    sslerr:
        err = SSL_get_error(pThis->pNetOssl->ssl, lenRcvd);
        if (err == SSL_ERROR_ZERO_RETURN) {
            DBGPRINTF("osslRecordRecv: SSL_ERROR_ZERO_RETURN received, connection may closed already\n");
            ABORT_FINALIZE(RS_RET_RETRY);
        } else if (err == SSL_ERROR_SYSCALL) {
            /* Output error and abort */
            nsd_ossl_lastOpenSSLErrorMsg(pThis, lenRcvd, pThis->pNetOssl->ssl, LOG_INFO, "osslRecordRecv",
                                         "SSL_read 1");
            iRet = RS_RET_TLS_ERR_SYSCALL;
            /* Check for underlaying socket errors **/
            if (errno == ECONNRESET) {
                DBGPRINTF("osslRecordRecv: SSL_ERROR_SYSCALL Errno %d, connection reset by peer\n", errno);
                /* Connection was dropped from remote site */
                iRet = RS_RET_CLOSED;
            } else {
                DBGPRINTF("osslRecordRecv: SSL_ERROR_SYSCALL Errno %d\n", errno);
            }
            ABORT_FINALIZE(iRet);
        } else if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
            DBGPRINTF("osslRecordRecv: SSL_get_error #1 = %d, lenRcvd=%zd\n", err, lenRcvd);
            /* Output OpenSSL error*/
            nsd_ossl_lastOpenSSLErrorMsg(pThis, lenRcvd, pThis->pNetOssl->ssl, LOG_ERR, "osslRecordRecv", "SSL_read 2");
            ABORT_FINALIZE(RS_RET_NO_ERRCODE);
        } else {
            DBGPRINTF("osslRecordRecv: SSL_get_error #2 = %d, lenRcvd=%zd\n", err, lenRcvd);
            pThis->rtryCall = osslRtry_recv;
            pThis->rtryOsslErr = err; /* Store SSL ErrorCode into*/
            ABORT_FINALIZE(RS_RET_RETRY);
        }
    }


finalize_it:
    if (pThis->rtryCall != osslRtry_None && pThis->rtryOsslErr == SSL_ERROR_WANT_WRITE) {
        *nextIODirection = NSDSEL_WR;
    } else {
        *nextIODirection = NSDSEL_RD;
    }
    dbgprintf("osslRecordRecv return. nsd %p, iRet %d, lenRcvd %zd, lenRcvBuf %d, ptrRcvBuf %d\n", pThis, iRet, lenRcvd,
              pThis->lenRcvBuf, pThis->ptrRcvBuf);
    RETiRet;
}

static rsRetVal osslInitSession(nsd_ossl_t *pThis, osslSslState_t osslType) /* , nsd_ossl_t *pServer) */
{
    DEFiRet;
    BIO *conn;
    char pristringBuf[4096];
    nsd_ptcp_t *pPtcp = (nsd_ptcp_t *)pThis->pTcp;

    if (!(pThis->pNetOssl->ssl = SSL_new(pThis->pNetOssl->ctx))) {
        pThis->pNetOssl->ssl = NULL;
        nsd_ossl_lastOpenSSLErrorMsg(pThis, 0, pThis->pNetOssl->ssl, LOG_ERR, "osslInitSession", "SSL_new");
        ABORT_FINALIZE(RS_RET_TLS_BASEINIT_FAIL);
    }

    // Set SSL_MODE_AUTO_RETRY to SSL obj
    SSL_set_mode(pThis->pNetOssl->ssl, SSL_MODE_AUTO_RETRY);

    if (pThis->pNetOssl->authMode != OSSL_AUTH_CERTANON) {
        dbgprintf("osslInitSession: enable certificate checking (Mode=%d, VerifyDepth=%d)\n", pThis->pNetOssl->authMode,
                  pThis->DrvrVerifyDepth);
        /* Enable certificate valid checking */
        net_ossl.osslSetSslVerifyCallback(pThis->pNetOssl->ssl, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT);
        if (pThis->DrvrVerifyDepth != 0) {
            SSL_set_verify_depth(pThis->pNetOssl->ssl, pThis->DrvrVerifyDepth);
        }
    } else if (pThis->gnutlsPriorityString == NULL) {
/* Allow ANON Ciphers only in ANON Mode and if no custom priority string is defined */
#if OPENSSL_VERSION_NUMBER >= 0x10100000L && !defined(LIBRESSL_VERSION_NUMBER)
        /* NOTE: do never use: +eNULL, it DISABLES encryption! */
        strncpy(pristringBuf, "ALL:+COMPLEMENTOFDEFAULT:+ADH:+ECDH:+aNULL@SECLEVEL=0", sizeof(pristringBuf));
#else
        strncpy(pristringBuf, "ALL:+COMPLEMENTOFDEFAULT:+ADH:+ECDH:+aNULL", sizeof(pristringBuf));
#endif

        dbgprintf("osslInitSession: setting anon ciphers: %s\n", pristringBuf);
        if (SSL_set_cipher_list(pThis->pNetOssl->ssl, pristringBuf) == 0) {
            dbgprintf("osslInitSession: Error setting ciphers '%s'\n", pristringBuf);
            ABORT_FINALIZE(RS_RET_SYS_ERR);
        }
    }

    /* Create BIO from ptcp socket!
     * Use BIO_NOCLOSE to prevent OpenSSL from closing the socket when
     * SSL_free is called. The socket is owned and will be closed by ptcp.
     */
    conn = BIO_new_socket(pPtcp->sock, BIO_NOCLOSE);
    dbgprintf("osslInitSession: Init conn BIO[%p] done\n", (void *)conn);

    /* Set debug Callback for conn BIO as well! */
    net_ossl.osslSetBioCallback(conn);

    /* TODO: still needed? Set to NON blocking ! */
    BIO_set_nbio(conn, 1);
    SSL_set_bio(pThis->pNetOssl->ssl, conn, conn);

    if (osslType == osslServer) {
        /* Server Socket */
        SSL_set_accept_state(pThis->pNetOssl->ssl); /* sets ssl to work in server mode. */
        pThis->pNetOssl->sslState = osslServer; /*set Server state */
    } else {
        /* Client Socket */
        SSL_set_connect_state(pThis->pNetOssl->ssl); /*sets ssl to work in client mode.*/
        pThis->pNetOssl->sslState = osslClient; /*set Client state */
    }
    pThis->bHaveSess = 1;

    /* we are done */
    FINALIZE;

finalize_it:
    RETiRet;
}


/* check if it is OK to talk to the remote peer
 * rgerhards, 2008-05-21
 */
rsRetVal osslChkPeerAuth(nsd_ossl_t *pThis) {
    DEFiRet;
    X509 *certpeer = NULL;

    ISOBJ_TYPE_assert(pThis, nsd_ossl);
    uchar *fromHostIP = NULL;
    nsd_ptcp.GetRemoteHName((nsd_t *)pThis->pTcp, &fromHostIP);

    /* call the actual function based on current auth mode */
    switch (pThis->pNetOssl->authMode) {
        case OSSL_AUTH_CERTNAME:
            /* if we check the name, we must ensure the cert is valid */
            certpeer = net_ossl.osslGetpeercert(pThis->pNetOssl, pThis->pNetOssl->ssl, fromHostIP);
            dbgprintf("osslChkPeerAuth: Check peer certname[%p]=%s\n", (void *)pThis->pNetOssl->ssl,
                      (certpeer != NULL ? "VALID" : "NULL"));
            CHKiRet(net_ossl.osslChkpeercertvalidity(pThis->pNetOssl, pThis->pNetOssl->ssl, fromHostIP));
            CHKiRet(net_ossl.osslChkpeername(pThis->pNetOssl, certpeer, fromHostIP));
            break;
        case OSSL_AUTH_CERTFINGERPRINT:
            certpeer = net_ossl.osslGetpeercert(pThis->pNetOssl, pThis->pNetOssl->ssl, fromHostIP);
            dbgprintf("osslChkPeerAuth: Check peer fingerprint[%p]=%s\n", (void *)pThis->pNetOssl->ssl,
                      (certpeer != NULL ? "VALID" : "NULL"));
            CHKiRet(net_ossl.osslChkpeercertvalidity(pThis->pNetOssl, pThis->pNetOssl->ssl, fromHostIP));
            CHKiRet(net_ossl.osslPeerfingerprint(pThis->pNetOssl, certpeer, fromHostIP));

            break;
        case OSSL_AUTH_CERTVALID:
            certpeer = net_ossl.osslGetpeercert(pThis->pNetOssl, pThis->pNetOssl->ssl, fromHostIP);
            dbgprintf("osslChkPeerAuth: Check peer valid[%p]=%s\n", (void *)pThis->pNetOssl->ssl,
                      (certpeer != NULL ? "VALID" : "NULL"));
            CHKiRet(net_ossl.osslChkpeercertvalidity(pThis->pNetOssl, pThis->pNetOssl->ssl, fromHostIP));
            break;
        case OSSL_AUTH_CERTANON:
        default:
            break;
    }
finalize_it:
    if (certpeer != NULL) {
        X509_free(certpeer);
    }
    if (fromHostIP != NULL) {
        free(fromHostIP);
    }
    RETiRet;
}

/* end a OpenSSL session
 * The function checks if we have a session and ends it only if so. So it can
 * always be called, even if there currently is no session.
 */
static rsRetVal osslEndSess(nsd_ossl_t *pThis) {
    DEFiRet;
    uchar *fromHostIP = NULL;
    int ret;
    int err;

    /* try closing SSL Connection */
    if (pThis->bHaveSess) {
        DBGPRINTF("osslEndSess: closing SSL Session ...\n");
        ret = SSL_shutdown(pThis->pNetOssl->ssl);
        nsd_ptcp.GetRemoteHName((nsd_t *)pThis->pTcp, &fromHostIP);
        if (ret <= 0) {
            err = SSL_get_error(pThis->pNetOssl->ssl, ret);
            DBGPRINTF("osslEndSess: shutdown failed with err = %d\n", err);

            /* ignore those SSL Errors on shutdown */
            if (err != SSL_ERROR_SYSCALL && err != SSL_ERROR_ZERO_RETURN && err != SSL_ERROR_WANT_READ &&
                err != SSL_ERROR_WANT_WRITE) {
                /* Output Warning only */
                nsd_ossl_lastOpenSSLErrorMsg(pThis, ret, pThis->pNetOssl->ssl, LOG_WARNING, "osslEndSess",
                                             "SSL_shutdown");
            }
            /* Shutdown not finished, call SSL_read to do a bidirectional shutdown, see doc for more:
             *	https://www.openssl.org/docs/man1.1.1/man3/SSL_shutdown.html
             */
            char rcvBuf[NSD_OSSL_MAX_RCVBUF];
            int iBytesRet = SSL_read(pThis->pNetOssl->ssl, rcvBuf, NSD_OSSL_MAX_RCVBUF);
            DBGPRINTF("osslEndSess: Forcing ssl shutdown SSL_read (%d) to do a bidirectional shutdown\n", iBytesRet);
            if (ret < 0) {
                /* Unsuccessful shutdown, log as INFO */
                LogMsg(0, RS_RET_NO_ERRCODE, LOG_INFO,
                       "nsd_ossl: "
                       "TLS session terminated successfully to remote syslog server '%s' with SSL Error '%d':"
                       " End Session",
                       fromHostIP, ret);
            }
            dbgprintf(
                "osslEndSess: TLS session terminated successfully to remote syslog server '%s'"
                " End Session",
                fromHostIP);
        } else {
            dbgprintf(
                "osslEndSess: TLS session terminated successfully with remote syslog server '%s':"
                " End Session",
                fromHostIP);
        }

        /* Session closed */
        pThis->bHaveSess = 0;
    }

    if (fromHostIP != NULL) {
        free(fromHostIP);
    }
    RETiRet;
}
/* ---------------------------- end OpenSSL specifics ---------------------------- */


/* Standard-Constructor */
BEGINobjConstruct(nsd_ossl) /* be sure to specify the object type also in END macro! */
    DBGPRINTF("nsd_ossl_construct: [%p]\n", pThis);
    /* construct nsd_ptcp helper */
    CHKiRet(nsd_ptcp.Construct(&pThis->pTcp));
    /* construct net_ossl helper */
    CHKiRet(net_ossl.Construct(&pThis->pNetOssl));
finalize_it:
ENDobjConstruct(nsd_ossl)


/* destructor for the nsd_ossl object */
PROTOTYPEobjDestruct(nsd_ossl);
BEGINobjDestruct(nsd_ossl) /* be sure to specify the object type also in END and CODESTART macros! */
    CODESTARTobjDestruct(nsd_ossl);
    DBGPRINTF("nsd_ossl_destruct: [%p] Mode %d\n", pThis, pThis->iMode);
    if (pThis->iMode == 1) {
        osslEndSess(pThis);
    }
    /* TODO MOVE Free SSL obj also if we do not have a session - or are NOT in TLS mode! */
    if (pThis->pNetOssl->ssl != NULL) {
        DBGPRINTF("nsd_ossl_destruct: [%p] FREE pThis->pNetOssl->ssl \n", pThis);
        /* If pTcp is NULL, we configure the BIO to close it on SSL_free
           This is a purely defensive measure and is this case is not actually expected
           */
        assert(pThis->pTcp != NULL);
        if (pThis->pTcp == NULL) {
            BIO *bio = SSL_get_rbio(pThis->pNetOssl->ssl);
            if (bio != NULL) {
                BIO_set_close(bio, BIO_CLOSE);
            }
        }
        SSL_free(pThis->pNetOssl->ssl);
        pThis->pNetOssl->ssl = NULL;
    }
    /**/
    if (pThis->pTcp != NULL) {
        nsd_ptcp.Destruct(&pThis->pTcp);
    }
    if (pThis->pNetOssl != NULL) {
        net_ossl.Destruct(&pThis->pNetOssl);
    }

    free(pThis->pszConnectHost);
    free(pThis->pszRcvBuf);
ENDobjDestruct(nsd_ossl)


/* Set the driver mode. For us, this has the following meaning:
 * 0 - work in plain tcp mode, without tls (e.g. before a STARTTLS)
 * 1 - work in TLS mode
 * rgerhards, 2008-04-28
 */
static rsRetVal SetMode(nsd_t *pNsd, const int mode) {
    DEFiRet;
    nsd_ossl_t *pThis = (nsd_ossl_t *)pNsd;

    ISOBJ_TYPE_assert((pThis), nsd_ossl);
    if (mode != 0 && mode != 1) {
        LogError(0, RS_RET_INVALID_DRVR_MODE,
                 "error: driver mode %d not supported by"
                 " ossl netstream driver",
                 mode);
    }
    pThis->iMode = mode;

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
static rsRetVal SetAuthMode(nsd_t *const pNsd, uchar *const mode) {
    DEFiRet;
    nsd_ossl_t *pThis = (nsd_ossl_t *)pNsd;

    ISOBJ_TYPE_assert((pThis), nsd_ossl);
    if (mode == NULL || !strcasecmp((char *)mode, "x509/name")) {
        pThis->pNetOssl->authMode = OSSL_AUTH_CERTNAME;
    } else if (!strcasecmp((char *)mode, "x509/fingerprint")) {
        pThis->pNetOssl->authMode = OSSL_AUTH_CERTFINGERPRINT;
    } else if (!strcasecmp((char *)mode, "x509/certvalid")) {
        pThis->pNetOssl->authMode = OSSL_AUTH_CERTVALID;
    } else if (!strcasecmp((char *)mode, "anon")) {
        pThis->pNetOssl->authMode = OSSL_AUTH_CERTANON;

    } else {
        LogError(0, RS_RET_VALUE_NOT_SUPPORTED,
                 "error: authentication mode '%s' not supported by "
                 "ossl netstream driver",
                 mode);
        ABORT_FINALIZE(RS_RET_VALUE_NOT_SUPPORTED);
    }

    dbgprintf("SetAuthMode: Set Mode %s/%d\n", mode, pThis->pNetOssl->authMode);

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
    nsd_ossl_t *pThis = (nsd_ossl_t *)pNsd;

    ISOBJ_TYPE_assert((pThis), nsd_ossl);
    /* default is set to off! */
    if (mode == NULL || !strcasecmp((char *)mode, "off")) {
        pThis->permitExpiredCerts = OSSL_EXPIRED_DENY;
    } else if (!strcasecmp((char *)mode, "warn")) {
        pThis->permitExpiredCerts = OSSL_EXPIRED_WARN;
    } else if (!strcasecmp((char *)mode, "on")) {
        pThis->permitExpiredCerts = OSSL_EXPIRED_PERMIT;
    } else {
        LogError(0, RS_RET_VALUE_NOT_SUPPORTED,
                 "error: permitexpiredcerts mode '%s' not supported by "
                 "ossl netstream driver",
                 mode);
        ABORT_FINALIZE(RS_RET_VALUE_NOT_SUPPORTED);
    }

    dbgprintf("SetPermitExpiredCerts: Set Mode %s/%d\n", mode, pThis->permitExpiredCerts);

    /* TODO: clear stored IDs! */

finalize_it:
    RETiRet;
}


/* Set permitted peers. It is depending on the auth mode if this are
 * fingerprints or names. -- rgerhards, 2008-05-19
 */
static rsRetVal SetPermPeers(nsd_t *pNsd, permittedPeers_t *pPermPeers) {
    DEFiRet;
    nsd_ossl_t *pThis = (nsd_ossl_t *)pNsd;

    ISOBJ_TYPE_assert((pThis), nsd_ossl);
    if (pPermPeers == NULL) FINALIZE;

    if (pThis->pNetOssl->authMode != OSSL_AUTH_CERTFINGERPRINT && pThis->pNetOssl->authMode != OSSL_AUTH_CERTNAME) {
        LogError(0, RS_RET_VALUE_NOT_IN_THIS_MODE,
                 "authentication not supported by "
                 "ossl netstream driver in the configured authentication mode - ignored");
        ABORT_FINALIZE(RS_RET_VALUE_NOT_IN_THIS_MODE);
    }
    pThis->pNetOssl->pPermPeers = pPermPeers;

finalize_it:
    RETiRet;
}


/* Provide access to the underlying OS socket. This is primarily
 * useful for other drivers (like nsd_ossl) who utilize ourselfs
 * for some of their functionality. -- rgerhards, 2008-04-18
 */
static rsRetVal SetSock(nsd_t *pNsd, int sock) {
    DEFiRet;
    nsd_ossl_t *pThis = (nsd_ossl_t *)pNsd;
    ISOBJ_TYPE_assert((pThis), nsd_ossl);
    assert(sock >= 0);

    DBGPRINTF("SetSock for [%p]: Setting sock %d\n", pNsd, sock);
    nsd_ptcp.SetSock(pThis->pTcp, sock);

    RETiRet;
}


/* Keep Alive Options
 */
static rsRetVal SetKeepAliveIntvl(nsd_t *pNsd, int keepAliveIntvl) {
    DEFiRet;
    nsd_ossl_t *pThis = (nsd_ossl_t *)pNsd;

    ISOBJ_TYPE_assert((pThis), nsd_ossl);
    assert(keepAliveIntvl >= 0);

    dbgprintf("SetKeepAliveIntvl: keepAliveIntvl=%d\n", keepAliveIntvl);
    nsd_ptcp.SetKeepAliveIntvl(pThis->pTcp, keepAliveIntvl);

    RETiRet;
}


/* Keep Alive Options
 */
static rsRetVal SetKeepAliveProbes(nsd_t *pNsd, int keepAliveProbes) {
    DEFiRet;
    nsd_ossl_t *pThis = (nsd_ossl_t *)pNsd;

    ISOBJ_TYPE_assert((pThis), nsd_ossl);
    assert(keepAliveProbes >= 0);

    dbgprintf("SetKeepAliveProbes: keepAliveProbes=%d\n", keepAliveProbes);
    nsd_ptcp.SetKeepAliveProbes(pThis->pTcp, keepAliveProbes);

    RETiRet;
}


/* Keep Alive Options
 */
static rsRetVal SetKeepAliveTime(nsd_t *pNsd, int keepAliveTime) {
    DEFiRet;
    nsd_ossl_t *pThis = (nsd_ossl_t *)pNsd;

    ISOBJ_TYPE_assert((pThis), nsd_ossl);
    assert(keepAliveTime >= 0);

    dbgprintf("SetKeepAliveTime: keepAliveTime=%d\n", keepAliveTime);
    nsd_ptcp.SetKeepAliveTime(pThis->pTcp, keepAliveTime);

    RETiRet;
}


/* abort a connection. This is meant to be called immediately
 * before the Destruct call. -- rgerhards, 2008-03-24
 */
static rsRetVal Abort(nsd_t *pNsd) {
    nsd_ossl_t *pThis = (nsd_ossl_t *)pNsd;
    DEFiRet;

    ISOBJ_TYPE_assert((pThis), nsd_ossl);

    if (pThis->iMode == 0) {
        nsd_ptcp.Abort(pThis->pTcp);
    }

    RETiRet;
}


/* Callback after netstrm obj init in nsd_ptcp - permits us to add some data */
static rsRetVal LstnInitDrvr(netstrm_t *const pThis) {
    DEFiRet;
    nsd_ossl_t *pNsdOssl = (nsd_ossl_t *)pThis->pDrvrData;
    /* Create main CTX Object. Use SSLv23_method for < Openssl 1.1.0 and TLS_method for all newer versions! */
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    CHKiRet(net_ossl.osslCtxInit(pNsdOssl->pNetOssl, SSLv23_method()));
#else
    CHKiRet(net_ossl.osslCtxInit(pNsdOssl->pNetOssl, TLS_method()));
#endif
    // Apply PriorityString after Ctx Creation
    applyGnutlsPriorityString(pNsdOssl);
finalize_it:
    RETiRet;
}


/* initialize the tcp socket for a listner
 * Here, we use the ptcp driver - because there is nothing special
 * at this point with OpenSSL. Things become special once we accept
 * a session, but not during listener setup.
 */
static rsRetVal LstnInit(netstrms_t *pNS,
                         void *pUsr,
                         rsRetVal (*fAddLstn)(void *, netstrm_t *),
                         const int iSessMax,
                         const tcpLstnParams_t *const cnf_params) {
    DEFiRet;

    dbgprintf("LstnInit for openssl: entering LstnInit (%p) for %s:%s SessMax=%d\n", fAddLstn, cnf_params->pszAddr,
              cnf_params->pszPort, iSessMax);

    pNS->fLstnInitDrvr = LstnInitDrvr;
    /* Init TCP Listener using base ptcp class */
    iRet = nsd_ptcp.LstnInit(pNS, pUsr, fAddLstn, iSessMax, cnf_params);
    RETiRet;
}


/* This function checks if the connection is still alive - well, kind of...
 * This is a dummy here. For details, check function common in ptcp driver.
 * rgerhards, 2008-06-09
 */
static rsRetVal CheckConnection(nsd_t __attribute__((unused)) * pNsd) {
    nsd_ossl_t *pThis = (nsd_ossl_t *)pNsd;
    ISOBJ_TYPE_assert(pThis, nsd_ossl);

    dbgprintf("CheckConnection for %p\n", pNsd);
    return nsd_ptcp.CheckConnection(pThis->pTcp);
}


/* Provide access to the underlying OS socket.
 */
static rsRetVal GetSock(nsd_t *pNsd, int *pSock) {
    nsd_ossl_t *pThis = (nsd_ossl_t *)pNsd;
    return nsd_ptcp.GetSock(pThis->pTcp, pSock);
}

/* get the remote hostname. The returned hostname must be freed by the caller.
 * rgerhards, 2008-04-25
 */
static rsRetVal GetRemoteHName(nsd_t *pNsd, uchar **ppszHName) {
    DEFiRet;
    nsd_ossl_t *pThis = (nsd_ossl_t *)pNsd;
    ISOBJ_TYPE_assert(pThis, nsd_ossl);
    iRet = nsd_ptcp.GetRemoteHName(pThis->pTcp, ppszHName);
    dbgprintf("GetRemoteHName for %p = %s\n", pNsd, *ppszHName);
    RETiRet;
}


/* Provide access to the sockaddr_storage of the remote peer. This
 * is needed by the legacy ACL system. --- gerhards, 2008-12-01
 */
static rsRetVal GetRemAddr(nsd_t *pNsd, struct sockaddr_storage **ppAddr) {
    DEFiRet;
    nsd_ossl_t *pThis = (nsd_ossl_t *)pNsd;
    ISOBJ_TYPE_assert(pThis, nsd_ossl);
    iRet = nsd_ptcp.GetRemAddr(pThis->pTcp, ppAddr);
    dbgprintf("GetRemAddr for %p\n", pNsd);
    RETiRet;
}


/* get the remote host's IP address. Caller must Destruct the object. */
static rsRetVal GetRemoteIP(nsd_t *pNsd, prop_t **ip) {
    DEFiRet;
    nsd_ossl_t *pThis = (nsd_ossl_t *)pNsd;
    ISOBJ_TYPE_assert(pThis, nsd_ossl);
    iRet = nsd_ptcp.GetRemoteIP(pThis->pTcp, ip);
    dbgprintf("GetRemoteIP for %p\n", pNsd);
    RETiRet;
}


/* Perform all necessary checks after Handshake
 */
rsRetVal osslPostHandshakeCheck(nsd_ossl_t *pNsd) {
    DEFiRet;
    uchar *fromHostIP = NULL;
    char szDbg[255];
    const SSL_CIPHER *sslCipher;

    nsd_ptcp.GetRemoteHName((nsd_t *)pNsd->pTcp, &fromHostIP);

    /* Some extra output for debugging openssl */
    if (SSL_get_shared_ciphers(pNsd->pNetOssl->ssl, szDbg, sizeof szDbg) != NULL)
        dbgprintf("osslPostHandshakeCheck: Debug Shared ciphers = %s\n", szDbg);

#if OPENSSL_VERSION_NUMBER >= 0x10002000L
    if (SSL_get_shared_curve(pNsd->pNetOssl->ssl, -1) == 0) {
        // This is not a failure
        LogMsg(0, RS_RET_NO_ERRCODE, LOG_INFO,
               "nsd_ossl: "
               "Information, no shared curve between syslog client '%s' and server",
               fromHostIP);
    }
#endif
    dbgprintf("osslPostHandshakeCheck: Debug Protocol Version: %s\n", SSL_get_version(pNsd->pNetOssl->ssl));

    sslCipher = (const SSL_CIPHER *)SSL_get_current_cipher(pNsd->pNetOssl->ssl);
    if (sslCipher != NULL) {
        if (SSL_CIPHER_get_version(sslCipher) == NULL) {
            LogError(0, RS_RET_NO_ERRCODE,
                     "nsd_ossl:"
                     "TLS version mismatch between syslog client '%s' and server.",
                     fromHostIP);
        }
        dbgprintf("osslPostHandshakeCheck: Debug Cipher Version: %s Name: %s\n", SSL_CIPHER_get_version(sslCipher),
                  SSL_CIPHER_get_name(sslCipher));
    } else {
        LogError(0, RS_RET_NO_ERRCODE, "nsd_ossl:No shared ciphers between syslog client '%s' and server.", fromHostIP);
    }

    FINALIZE;

finalize_it:
    if (fromHostIP != NULL) {
        free(fromHostIP);
    }
    RETiRet;
}


/* Perform all necessary actions for Handshake
 */
rsRetVal osslHandshakeCheck(nsd_ossl_t *pNsd) {
    DEFiRet;
    uchar *fromHostIP = NULL;
    int remotePort = -1;
    uchar remotePortStr[8];
    int res, resErr;
    dbgprintf("osslHandshakeCheck: Starting TLS Handshake for ssl[%p]\n", (void *)pNsd->pNetOssl->ssl);

    if (pNsd->pNetOssl->sslState == osslServer) {
        /* Handle Server SSL Object */
        if ((res = SSL_accept(pNsd->pNetOssl->ssl)) <= 0) {
            /* Obtain SSL Error code */
            nsd_ptcp.GetRemoteHName(pNsd->pTcp, &fromHostIP);
            nsd_ptcp.GetRemotePort(pNsd->pTcp, &remotePort);
            resErr = SSL_get_error(pNsd->pNetOssl->ssl, res);
            if (resErr == SSL_ERROR_WANT_READ || resErr == SSL_ERROR_WANT_WRITE) {
                pNsd->rtryCall = osslRtry_handshake;
                pNsd->rtryOsslErr = resErr; /* Store SSL ErrorCode into*/
                dbgprintf(
                    "osslHandshakeCheck: OpenSSL Server handshake does not complete "
                    "immediately - setting to retry (this is OK and normal)\n");
                FINALIZE;
            } else {
                nsd_ptcp.FmtRemotePortStr(remotePort, remotePortStr, sizeof(remotePortStr));
                if (resErr == SSL_ERROR_SYSCALL) {
                    dbgprintf(
                        "osslHandshakeCheck: OpenSSL Server handshake failed with "
                        "SSL_ERROR_SYSCALL - Aborting handshake.\n");
                    nsd_ossl_lastOpenSSLErrorMsg(pNsd, res, pNsd->pNetOssl->ssl, LOG_WARNING,
                                                 "osslHandshakeCheck Server", "SSL_accept");
                    LogMsg(0, RS_RET_NO_ERRCODE, LOG_WARNING,
                           "nsd_ossl:TLS session terminated with remote client '%s:%s': "
                           "Handshake failed with SSL_ERROR_SYSCALL",
                           fromHostIP, remotePortStr);
                    ABORT_FINALIZE(RS_RET_NO_ERRCODE);
                } else {
                    nsd_ossl_lastOpenSSLErrorMsg(pNsd, res, pNsd->pNetOssl->ssl, LOG_ERR, "osslHandshakeCheck Server",
                                                 "SSL_accept");
                    LogMsg(0, RS_RET_NO_ERRCODE, LOG_WARNING,
                           "nsd_ossl:TLS session terminated with remote client '%s:%s': "
                           "Handshake failed with error code: %d",
                           fromHostIP, remotePortStr, resErr);
                    ABORT_FINALIZE(RS_RET_NO_ERRCODE);
                }
            }
        }
    } else {
        /* Handle Client SSL Object */
        if ((res = SSL_do_handshake(pNsd->pNetOssl->ssl)) <= 0) {
            /* Obtain SSL Error code */
            nsd_ptcp.GetRemoteHName(pNsd->pTcp, &fromHostIP);
            resErr = SSL_get_error(pNsd->pNetOssl->ssl, res);
            if (resErr == SSL_ERROR_WANT_READ || resErr == SSL_ERROR_WANT_WRITE) {
                pNsd->rtryCall = osslRtry_handshake;
                pNsd->rtryOsslErr = resErr; /* Store SSL ErrorCode into*/
                dbgprintf(
                    "osslHandshakeCheck: OpenSSL Client handshake does not complete "
                    "immediately - setting to retry (this is OK and normal)\n");
                FINALIZE;
            } else if (resErr == SSL_ERROR_SYSCALL) {
                dbgprintf(
                    "osslHandshakeCheck: OpenSSL Client handshake failed with SSL_ERROR_SYSCALL "
                    "- Aborting handshake.\n");
                nsd_ossl_lastOpenSSLErrorMsg(pNsd, res, pNsd->pNetOssl->ssl, LOG_WARNING, "osslHandshakeCheck Client",
                                             "SSL_do_handshake");
                ABORT_FINALIZE(RS_RET_NO_ERRCODE /*RS_RET_RETRY*/);
            } else {
                dbgprintf(
                    "osslHandshakeCheck: OpenSSL Client handshake failed with %d "
                    "- Aborting handshake.\n",
                    resErr);
                nsd_ossl_lastOpenSSLErrorMsg(pNsd, res, pNsd->pNetOssl->ssl, LOG_ERR, "osslHandshakeCheck Client",
                                             "SSL_do_handshake");
                nsd_ptcp.GetRemotePort(pNsd->pTcp, &remotePort);
                nsd_ptcp.FmtRemotePortStr(remotePort, remotePortStr, sizeof(remotePortStr));
                LogMsg(0, RS_RET_NO_ERRCODE, LOG_WARNING,
                       "nsd_ossl:TLS session terminated with remote syslog server '%s:%s':"
                       "Handshake failed with error code: %d",
                       fromHostIP, remotePortStr, resErr);
                ABORT_FINALIZE(RS_RET_NO_ERRCODE);
            }
        }
    }

    /* Do post handshake stuff */
    CHKiRet(osslPostHandshakeCheck(pNsd));

    /* Now check authorization */
    CHKiRet(osslChkPeerAuth(pNsd));
finalize_it:
    if (fromHostIP != NULL) {
        free(fromHostIP);
    }
    if (iRet == RS_RET_OK) {
        /* If no error occurred, set socket to SSL mode */
        pNsd->iMode = 1;
    }

    RETiRet;
}


/* accept an incoming connection request - here, we do the usual accept
 * handling. TLS specific handling is done thereafter (and if we run in TLS
 * mode at this time).
 * rgerhards, 2008-04-25
 */
static rsRetVal AcceptConnReq(nsd_t *pNsd, nsd_t **ppNew, char *const connInfo) {
    DEFiRet;
    nsd_ossl_t *pNew = NULL;
    nsd_ossl_t *pThis = (nsd_ossl_t *)pNsd;

    ISOBJ_TYPE_assert((pThis), nsd_ossl);
    CHKiRet(nsd_osslConstruct(&pNew));
    CHKiRet(nsd_ptcp.Destruct(&pNew->pTcp));
    dbgprintf("AcceptConnReq for [%p]: Accepting connection ... \n", (void *)pThis);
    CHKiRet(nsd_ptcp.AcceptConnReq(pThis->pTcp, &pNew->pTcp, connInfo));

    if (pThis->iMode == 0) {
        /*we are in non-TLS mode, so we are done */
        DBGPRINTF("AcceptConnReq: NOT in TLS mode!\n");
        *ppNew = (nsd_t *)pNew;
        FINALIZE;
    }

    /* If we reach this point, we are in TLS mode */
    pNew->pNetOssl->authMode = pThis->pNetOssl->authMode;
    pNew->permitExpiredCerts = pThis->permitExpiredCerts;
    pNew->pNetOssl->pPermPeers = pThis->pNetOssl->pPermPeers;
    pNew->pNetOssl->bSANpriority = pThis->pNetOssl->bSANpriority;
    pNew->DrvrVerifyDepth = pThis->DrvrVerifyDepth;
    pNew->gnutlsPriorityString = pThis->gnutlsPriorityString;
    pNew->pNetOssl->ctx = pThis->pNetOssl->ctx;
    pNew->pNetOssl->ctx_is_copy = 1;  // do not free on pNew Destruction
    CHKiRet(osslInitSession(pNew, osslServer));

    /* Store nsd_ossl_t* reference in SSL obj */
    SSL_set_ex_data(pNew->pNetOssl->ssl, 0, pThis->pTcp);
    SSL_set_ex_data(pNew->pNetOssl->ssl, 1, &pThis->permitExpiredCerts);

    /* We now do the handshake */
    CHKiRet(osslHandshakeCheck(pNew));

    *ppNew = (nsd_t *)pNew;
finalize_it:
    /* Accept appears to be done here */
    if (pNew != NULL) {
        DBGPRINTF("AcceptConnReq: END iRet = %d, pNew=[%p], pNsd->rtryCall=%d\n", iRet, pNew, pNew->rtryCall);
    }
    if (iRet != RS_RET_OK) {
        if (pNew != NULL) {
            nsd_osslDestruct(&pNew);
        }
    }
    RETiRet;
}


/* receive data from a tcp socket
 * The lenBuf parameter must contain the max buffer size on entry and contains
 * the number of octets read on exit. This function
 * never blocks, not even when called on a blocking socket. That is important
 * for client sockets, which are set to block during send, but should not
 * block when trying to read data. -- rgerhards, 2008-03-17
 * The function now follows the usual iRet calling sequence.
 * With GnuTLS, we may need to restart a recv() system call. If so, we need
 * to supply the SAME buffer on the retry. We can not assure this, as the
 * caller is free to call us with any buffer location (and in current
 * implementation, it is on the stack and extremely likely to change). To
 * work-around this problem, we allocate a buffer ourselfs and always receive
 * into that buffer. We pass data on to the caller only after we have received it.
 * To save some space, we allocate that internal buffer only when it is actually
 * needed, which means when we reach this function for the first time. To keep
 * the algorithm simple, we always supply data only from the internal buffer,
 * even if it is a single byte. As we have a stream, the caller must be prepared
 * to accept messages in any order, so we do not need to take care about this.
 * Please note that the logic also forces us to do some "faking" in select(), as
 * we must provide a fake "is ready for readign" status if we have data inside our
 * buffer. -- rgerhards, 2008-06-23
 */
static rsRetVal Rcv(nsd_t *pNsd, uchar *pBuf, ssize_t *pLenBuf, int *const oserr, unsigned *const nextIODirection) {
    DEFiRet;
    ssize_t iBytesCopy; /* how many bytes are to be copied to the client buffer? */
    nsd_ossl_t *pThis = (nsd_ossl_t *)pNsd;
    ISOBJ_TYPE_assert(pThis, nsd_ossl);
    DBGPRINTF("Rcv for %p\n", pNsd);

    if (pThis->bAbortConn) ABORT_FINALIZE(RS_RET_CONNECTION_ABORTREQ);

    if (pThis->iMode == 0) {
        CHKiRet(nsd_ptcp.Rcv(pThis->pTcp, pBuf, pLenBuf, oserr, nextIODirection));
        FINALIZE;
    }

    /* --- in TLS mode now --- */
    if (pThis->rtryCall == osslRtry_handshake) {
        /* note: we are in receive, so we acually will retry receive in any case */
        CHKiRet(doRetry(pThis));
        ABORT_FINALIZE(RS_RET_RETRY);
    }

    /* Buffer logic applies only if we are in TLS mode. Here we
     * assume that we will switch from plain to TLS, but never back. This
     * assumption may be unsafe, but it is the model for the time being and I
     * do not see any valid reason why we should switch back to plain TCP after
     * we were in TLS mode. However, in that case we may lose something that
     * is already in the receive buffer ... risk accepted. -- rgerhards, 2008-06-23
     */

    if (pThis->pszRcvBuf == NULL) {
        /* we have no buffer, so we need to malloc one */
        CHKmalloc(pThis->pszRcvBuf = malloc(NSD_OSSL_MAX_RCVBUF));
        pThis->lenRcvBuf = -1;
    }

    /* now check if we have something in our buffer. If so, we satisfy
     * the request from buffer contents.
     */
    if (pThis->lenRcvBuf == -1) { /* no data present, must read */
        CHKiRet(osslRecordRecv(pThis, nextIODirection));
    }

    if (pThis->lenRcvBuf == 0) { /* EOS */
        *oserr = errno;
        ABORT_FINALIZE(RS_RET_CLOSED);
    }

    /* if we reach this point, data is present in the buffer and must be copied */
    iBytesCopy = pThis->lenRcvBuf - pThis->ptrRcvBuf;
    if (iBytesCopy > *pLenBuf) {
        iBytesCopy = *pLenBuf;
    } else {
        pThis->lenRcvBuf = -1; /* buffer will be emptied below */
    }

    memcpy(pBuf, pThis->pszRcvBuf + pThis->ptrRcvBuf, iBytesCopy);
    pThis->ptrRcvBuf += iBytesCopy;
    *pLenBuf = iBytesCopy;

finalize_it:
    if (iRet != RS_RET_OK) {
        if (iRet == RS_RET_CLOSED) {
            if (pThis->pNetOssl->ssl != NULL) {
                /* Set SSL Shutdown */
                SSL_shutdown(pThis->pNetOssl->ssl);
                dbgprintf("osslRcv SSL_shutdown done\n");
            }
        } else if (iRet != RS_RET_RETRY) {
            /* We need to free the receive buffer in error error case unless a retry is wanted. , if we
             * allocated one. -- rgerhards, 2008-12-03 -- moved here by alorbach, 2015-12-01
             */
            *pLenBuf = 0;
            free(pThis->pszRcvBuf);
            pThis->pszRcvBuf = NULL;
        } else {
            /* RS_RET_RETRY | Check for SSL Shutdown */
            if (SSL_get_shutdown(pThis->pNetOssl->ssl) == SSL_RECEIVED_SHUTDOWN) {
                dbgprintf("osslRcv received SSL_RECEIVED_SHUTDOWN!\n");
                iRet = RS_RET_CLOSED;

                /* Send Shutdown message back */
                SSL_shutdown(pThis->pNetOssl->ssl);
            }
        }
    }
    dbgprintf("osslRcv return. nsd %p, iRet %d, lenRcvBuf %d, ptrRcvBuf %d\n", pThis, iRet, pThis->lenRcvBuf,
              pThis->ptrRcvBuf);
    RETiRet;
}


/* send a buffer. On entry, pLenBuf contains the number of octets to
 * write. On exit, it contains the number of octets actually written.
 * If this number is lower than on entry, only a partial buffer has
 * been written.
 * rgerhards, 2008-03-19
 */
static rsRetVal Send(nsd_t *pNsd, uchar *pBuf, ssize_t *pLenBuf) {
    DEFiRet;
    int iSent;
    int err;
    nsd_ossl_t *pThis = (nsd_ossl_t *)pNsd;
    DBGPRINTF("Send for %p\n", pNsd);

    ISOBJ_TYPE_assert(pThis, nsd_ossl);

    if (pThis->bAbortConn) ABORT_FINALIZE(RS_RET_CONNECTION_ABORTREQ);

    if (pThis->iMode == 0) {
        CHKiRet(nsd_ptcp.Send(pThis->pTcp, pBuf, pLenBuf));
        FINALIZE;
    }

    while (1) {
        iSent = SSL_write(pThis->pNetOssl->ssl, pBuf, *pLenBuf);
        if (iSent > 0) {
            *pLenBuf = iSent;
            break;
        } else {
            err = SSL_get_error(pThis->pNetOssl->ssl, iSent);
            if (err == SSL_ERROR_ZERO_RETURN) {
                DBGPRINTF("Send: SSL_ERROR_ZERO_RETURN received, connection closed by peer\n");
                ABORT_FINALIZE(RS_RET_CLOSED);
            } else if (err == SSL_ERROR_SYSCALL) {
                /* Output error and abort */
                nsd_ossl_lastOpenSSLErrorMsg(pThis, iSent, pThis->pNetOssl->ssl, LOG_INFO, "Send", "SSL_write");
                iRet = RS_RET_TLS_ERR_SYSCALL;
                /* Check for underlaying socket errors **/
                if (errno == ECONNRESET) {
                    dbgprintf("Send: SSL_ERROR_SYSCALL Connection was reset by remote\n");
                    /* Connection was dropped from remote site */
                    iRet = RS_RET_CLOSED;
                } else {
                    DBGPRINTF("Send: SSL_ERROR_SYSCALL Errno %d\n", errno);
                }
                ABORT_FINALIZE(iRet);
            } else if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
                /* Output error and abort */
                nsd_ossl_lastOpenSSLErrorMsg(pThis, iSent, pThis->pNetOssl->ssl, LOG_ERR, "Send", "SSL_write");
                ABORT_FINALIZE(RS_RET_NO_ERRCODE);
            } else {
                /*
                 * OpenSSL needs us to READ or WRITE to make progress.
                 * In TLS 1.3, servers may send post-handshake messages (e.g. KeyUpdate)
                 * which require the client to read before it can continue writing.
                 * Use the buffered receive helper to preserve any application data.
                 */
                if (err == SSL_ERROR_WANT_READ) {
                    unsigned nextIODirection ATTR_UNUSED;
                    rsRetVal rcvRet = osslRecordRecv(pThis, &nextIODirection);
                    if (rcvRet == RS_RET_CLOSED) {
                        ABORT_FINALIZE(RS_RET_CLOSED);
                    } else if (rcvRet != RS_RET_OK && rcvRet != RS_RET_RETRY) {
                        ABORT_FINALIZE(rcvRet);
                    }
                    if (SSL_get_shutdown(pThis->pNetOssl->ssl) == SSL_RECEIVED_SHUTDOWN) {
                        dbgprintf("Send: detected SSL_RECEIVED_SHUTDOWN while handling WANT_READ\n");
                        ABORT_FINALIZE(RS_RET_CLOSED);
                    }
                    /* Continue loop to retry SSL_write */
                } else {
                    /* Check for SSL Shutdown */
                    if (SSL_get_shutdown(pThis->pNetOssl->ssl) == SSL_RECEIVED_SHUTDOWN) {
                        dbgprintf("osslRcv received SSL_RECEIVED_SHUTDOWN!\n");
                        ABORT_FINALIZE(RS_RET_CLOSED);
                    }
                }
            }
        }
    }
finalize_it:
    RETiRet;
}


/* Enable KEEPALIVE handling on the socket.
 * rgerhards, 2009-06-02
 */
static rsRetVal EnableKeepAlive(nsd_t *pNsd) {
    nsd_ossl_t *pThis = (nsd_ossl_t *)pNsd;
    ISOBJ_TYPE_assert(pThis, nsd_ossl);
    return nsd_ptcp.EnableKeepAlive(pThis->pTcp);
}


/* open a connection to a remote host (server). With OpenSSL, we always
 * open a plain tcp socket and then, if in TLS mode, do a handshake on it.
 */
static rsRetVal Connect(nsd_t *pNsd, int family, uchar *port, uchar *host, char *device) {
    DEFiRet;
    DBGPRINTF("openssl: entering Connect family=%d, device=%s\n", family, device);
    nsd_ossl_t *pThis = (nsd_ossl_t *)pNsd;
    uchar *fromHostIP = NULL;

    ISOBJ_TYPE_assert(pThis, nsd_ossl);
    assert(port != NULL);
    assert(host != NULL);

    /* Create main CTX Object. Use SSLv23_method for < Openssl 1.1.0 and TLS_method for all newer versions! */
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    CHKiRet(net_ossl.osslCtxInit(pThis->pNetOssl, SSLv23_method()));
#else
    CHKiRet(net_ossl.osslCtxInit(pThis->pNetOssl, TLS_method()));
#endif
    // Apply PriorityString after Ctx Creation
    applyGnutlsPriorityString(pThis);

    // Perform TCP Connect
    CHKiRet(nsd_ptcp.Connect(pThis->pTcp, family, port, host, device));

    if (pThis->iMode == 0) {
        /*we are in non-TLS mode, so we are done */
        DBGPRINTF("Connect: NOT in TLS mode!\n");
        FINALIZE;
    }

    nsd_ptcp.GetRemoteHName((nsd_t *)pThis->pTcp, &fromHostIP);


    LogMsg(0, RS_RET_NO_ERRCODE, LOG_INFO,
           "nsd_ossl: "
           "TLS Connection initiated with remote syslog server '%s'.",
           fromHostIP);
    /*if we reach this point we are in tls mode */
    DBGPRINTF("Connect: TLS Mode\n");

    /* Do SSL Session init */
    CHKiRet(osslInitSession(pThis, osslClient));

    /* Store nsd_ossl_t* reference in SSL obj */
    SSL_set_ex_data(pThis->pNetOssl->ssl, 0, pThis->pTcp);
    SSL_set_ex_data(pThis->pNetOssl->ssl, 1, &pThis->permitExpiredCerts);

    /* We now do the handshake */
    iRet = osslHandshakeCheck(pThis);
finalize_it:
    if (fromHostIP != NULL) {
        free(fromHostIP);
    }
    /* Connect appears to be done here */
    dbgprintf("Connect: END iRet = %d, pThis=[%p], pNsd->rtryCall=%d\n", iRet, pThis, pThis->rtryCall);
    if (iRet != RS_RET_OK) {
        if (pThis->bHaveSess) {
            pThis->bHaveSess = 0;
            SSL_free(pThis->pNetOssl->ssl);
            pThis->pNetOssl->ssl = NULL;
        }
    }
    RETiRet;
}

static rsRetVal SetGnutlsPriorityString(nsd_t *const pNsd, uchar *const gnutlsPriorityString) {
    DEFiRet;
    nsd_ossl_t *pThis = (nsd_ossl_t *)pNsd;
    ISOBJ_TYPE_assert(pThis, nsd_ossl);

#if OPENSSL_VERSION_NUMBER >= 0x10002000L && !defined(LIBRESSL_VERSION_NUMBER)
    sbool ApplySettings = 0;
    if ((gnutlsPriorityString != NULL && pThis->gnutlsPriorityString == NULL) ||
        (gnutlsPriorityString != NULL &&
         strcmp((const char *)pThis->gnutlsPriorityString, (const char *)gnutlsPriorityString) != 0)) {
        ApplySettings = 1;
    }

    pThis->gnutlsPriorityString = gnutlsPriorityString;
    dbgprintf("gnutlsPriorityString: set to '%s' Apply %s\n",
              (gnutlsPriorityString != NULL ? (char *)gnutlsPriorityString : "NULL"),
              (ApplySettings == 1 ? "TRUE" : "FALSE"));
    if (ApplySettings == 1) {
        /* Apply Settings if necessary */
        applyGnutlsPriorityString(pThis);
    }

#else
    LogError(0, RS_RET_SYS_ERR,
             "Warning: TLS library does not support SSL_CONF_cmd API"
             "(maybe it is too old?). Cannot use gnutlsPriorityString ('%s'). For more see: "
             "https://www.rsyslog.com/doc/master/configuration/modules/imtcp.html#gnutlsprioritystring",
             gnutlsPriorityString);
#endif
    RETiRet;
}


static rsRetVal applyGnutlsPriorityString(nsd_ossl_t *const pThis) {
    DEFiRet;
    ISOBJ_TYPE_assert(pThis, nsd_ossl);

#if OPENSSL_VERSION_NUMBER >= 0x10002000L && !defined(LIBRESSL_VERSION_NUMBER)
    /* Note: we disable unkonwn functions. The corresponding error message is
     * generated during SetGntuTLSPriorityString().
     */
    if (pThis->gnutlsPriorityString == NULL || pThis->pNetOssl->ctx == NULL) {
        FINALIZE;
    } else {
        CHKiRet(net_ossl.osslApplyTlscgfcmd(pThis->pNetOssl, pThis->gnutlsPriorityString));
    }
#endif

finalize_it:
    RETiRet;
}

/* Set the driver cert extended key usage check setting, for now it is empty wrapper.
 * TODO: implement openSSL version
 * jvymazal, 2019-08-16
 */
static rsRetVal SetCheckExtendedKeyUsage(nsd_t __attribute__((unused)) * pNsd, int ChkExtendedKeyUsage) {
    DEFiRet;
    if (ChkExtendedKeyUsage != 0) {
        LogError(0, RS_RET_VALUE_NOT_SUPPORTED,
                 "error: driver ChkExtendedKeyUsage %d "
                 "not supported by ossl netstream driver",
                 ChkExtendedKeyUsage);
        ABORT_FINALIZE(RS_RET_VALUE_NOT_SUPPORTED);
    }
finalize_it:
    RETiRet;
}

/* Set the driver name checking strictness
 * 0 - less strict per RFC 5280, section 4.1.2.6 - either SAN or CN match is good
 * 1 - more strict per RFC 6125 - if any SAN present it must match (CN is ignored)
 * jvymazal, 2019-08-16, csiltala, 2025-07-28
 */
static rsRetVal SetPrioritizeSAN(nsd_t *pNsd, int prioritizeSan) {
    DEFiRet;
    nsd_ossl_t *pThis = (nsd_ossl_t *)pNsd;

    ISOBJ_TYPE_assert((pThis), nsd_ossl);
    if (prioritizeSan != 0 && prioritizeSan != 1) {
        LogError(0, RS_RET_VALUE_NOT_SUPPORTED,
                 "error: driver prioritizeSan %d "
                 "not supported by ossl netstream driver",
                 prioritizeSan);
        ABORT_FINALIZE(RS_RET_VALUE_NOT_SUPPORTED);
    }

    pThis->pNetOssl->bSANpriority = prioritizeSan;

finalize_it:
    RETiRet;
}

/* Set the driver tls  verifyDepth
 * alorbach, 2019-12-20
 */
static rsRetVal SetTlsVerifyDepth(nsd_t *pNsd, int verifyDepth) {
    DEFiRet;
    nsd_ossl_t *pThis = (nsd_ossl_t *)pNsd;

    ISOBJ_TYPE_assert((pThis), nsd_ossl);
    if (verifyDepth == 0) {
        FINALIZE;
    }
    assert(verifyDepth >= 2);
    pThis->DrvrVerifyDepth = verifyDepth;

finalize_it:
    RETiRet;
}


static rsRetVal SetTlsCAFile(nsd_t *pNsd, const uchar *const caFile) {
    DEFiRet;
    nsd_ossl_t *const pThis = (nsd_ossl_t *)pNsd;

    ISOBJ_TYPE_assert((pThis), nsd_ossl);
    free((void *)pThis->pNetOssl->pszCAFile);
    pThis->pNetOssl->pszCAFile = NULL;

    if (caFile != NULL) {
        CHKmalloc(pThis->pNetOssl->pszCAFile = (const uchar *)strdup((const char *)caFile));
    }

finalize_it:
    RETiRet;
}

static rsRetVal SetTlsCRLFile(nsd_t *pNsd, const uchar *const crlFile) {
    DEFiRet;
    nsd_ossl_t *const pThis = (nsd_ossl_t *)pNsd;

    ISOBJ_TYPE_assert((pThis), nsd_ossl);
    free((void *)pThis->pNetOssl->pszCRLFile);
    pThis->pNetOssl->pszCRLFile = NULL;

    if (crlFile != NULL) {
        CHKmalloc(pThis->pNetOssl->pszCRLFile = (const uchar *)strdup((const char *)crlFile));
    }

finalize_it:
    RETiRet;
}


static rsRetVal SetTlsKeyFile(nsd_t *pNsd, const uchar *const pszFile) {
    DEFiRet;
    nsd_ossl_t *const pThis = (nsd_ossl_t *)pNsd;

    ISOBJ_TYPE_assert((pThis), nsd_ossl);
    free((void *)pThis->pNetOssl->pszKeyFile);
    pThis->pNetOssl->pszKeyFile = NULL;

    if (pszFile != NULL) {
        CHKmalloc(pThis->pNetOssl->pszKeyFile = (const uchar *)strdup((const char *)pszFile));
    }

finalize_it:
    RETiRet;
}

static rsRetVal SetTlsCertFile(nsd_t *pNsd, const uchar *const pszFile) {
    DEFiRet;
    nsd_ossl_t *const pThis = (nsd_ossl_t *)pNsd;

    ISOBJ_TYPE_assert((pThis), nsd_ossl);
    free((void *)pThis->pNetOssl->pszCertFile);
    pThis->pNetOssl->pszCertFile = NULL;

    if (pszFile != NULL) {
        CHKmalloc(pThis->pNetOssl->pszCertFile = (const uchar *)strdup((const char *)pszFile));
    }

finalize_it:
    RETiRet;
}


/* queryInterface function */
BEGINobjQueryInterface(nsd_ossl)
    CODESTARTobjQueryInterface(nsd_ossl);
    if (pIf->ifVersion != nsdCURR_IF_VERSION) { /* check for current version, increment on each change */
        ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
    }

    /* ok, we have the right interface, so let's fill it
     * Please note that we may also do some backwards-compatibility
     * work here (if we can support an older interface version - that,
     * of course, also affects the "if" above).
     */
    pIf->Construct = (rsRetVal(*)(nsd_t **))nsd_osslConstruct;
    pIf->Destruct = (rsRetVal(*)(nsd_t **))nsd_osslDestruct;
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
    pIf->SetGnutlsPriorityString = SetGnutlsPriorityString; /* we don't NEED this interface! */
    pIf->SetCheckExtendedKeyUsage = SetCheckExtendedKeyUsage; /* we don't NEED this interface! */
    pIf->SetPrioritizeSAN = SetPrioritizeSAN;
    pIf->SetTlsVerifyDepth = SetTlsVerifyDepth;
    pIf->SetTlsCAFile = SetTlsCAFile;
    pIf->SetTlsCRLFile = SetTlsCRLFile;
    pIf->SetTlsKeyFile = SetTlsKeyFile;
    pIf->SetTlsCertFile = SetTlsCertFile;

finalize_it:
ENDobjQueryInterface(nsd_ossl)


/* exit our class
 */
BEGINObjClassExit(nsd_ossl, OBJ_IS_LOADABLE_MODULE) /* CHANGE class also in END MACRO! */
    CODESTARTObjClassExit(nsd_ossl);
    /* release objects we no longer need */
    objRelease(net_ossl, CORE_COMPONENT);
    objRelease(nsd_ptcp, LM_NSD_PTCP_FILENAME);
    objRelease(net, LM_NET_FILENAME);
    objRelease(glbl, CORE_COMPONENT);
    objRelease(datetime, CORE_COMPONENT);
ENDObjClassExit(nsd_ossl)


/* Initialize the nsd_ossl class. Must be called as the very first method
 * before anything else is called inside this class.
 */
BEGINObjClassInit(nsd_ossl, 1, OBJ_IS_LOADABLE_MODULE) /* class, version */
    /* request objects we use */
    CHKiRet(objUse(datetime, CORE_COMPONENT));
    CHKiRet(objUse(glbl, CORE_COMPONENT));
    CHKiRet(objUse(net, LM_NET_FILENAME));
    CHKiRet(objUse(nsd_ptcp, LM_NSD_PTCP_FILENAME));
    CHKiRet(objUse(net_ossl, CORE_COMPONENT));
ENDObjClassInit(nsd_ossl)


/* --------------- here now comes the plumbing that makes as a library module --------------- */


BEGINmodExit
    CODESTARTmodExit;
    nsd_osslClassExit();
    net_osslClassExit();
ENDmodExit


BEGINqueryEtryPt
    CODESTARTqueryEtryPt;
    CODEqueryEtryPt_STD_LIB_QUERIES;
ENDqueryEtryPt


BEGINmodInit()
    CODESTARTmodInit;
    *ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */

    /* Initialize all classes that are in our module - this includes ourselfs */
    DBGPRINTF("modInit\n");
    CHKiRet(net_osslClassInit(pModInfo)); /* must be done after tcps_sess, as we use it */
    CHKiRet(nsd_osslClassInit(pModInfo)); /* must be done after tcps_sess, as we use it */
ENDmodInit
