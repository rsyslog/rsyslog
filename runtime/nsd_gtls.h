/* An implementation of the nsd interface for GnuTLS.
 *
 * Copyright 2008-2021 Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
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

#ifndef INCLUDED_NSD_GTLS_H
#define INCLUDED_NSD_GTLS_H

#include "nsd.h"

#define NSD_GTLS_MAX_RCVBUF 16 * 1024 + 1 /* TLS RFC 8449: max size of buffer for message reception */
#define NSD_GTLS_MAX_CERT 10 /* max number of certs in our chain */

typedef enum {
    gtlsRtry_None = 0, /**< no call needs to be retried */
    gtlsRtry_handshake = 1,
    gtlsRtry_recv = 2
} gtlsRtryCall_t; /**< IDs of calls that needs to be retried */

typedef enum {
    gtlsDir_READ = 0, /**< GNUTLS wants READ */
    gtlsDir_WRITE = 1 /**< GNUTLS wants WRITE */
} gtlsDirection_t;

typedef nsd_if_t nsd_gtls_if_t; /* we just *implement* this interface */

/* the nsd_gtls object */
struct nsd_gtls_s {
    BEGINobjInstance
        ; /* Data to implement generic object - MUST be the first data element! */
        nsd_t *pTcp; /**< our aggregated nsd_ptcp data */
        uchar *pszConnectHost; /**< hostname used for connect - may be used to
                       authenticate peer if no other name given */
        const uchar *pszCAFile;
        const uchar *pszCRLFile;
        const uchar *pszKeyFile;
        const uchar *pszCertFile;
        gnutls_certificate_credentials_t xcred;
        int xcred_is_copy;
        int iMode; /* 0 - plain tcp, 1 - TLS */
        int bAbortConn; /* if set, abort conncection (fatal error had happened) */
        enum {
            GTLS_AUTH_CERTNAME = 0,
            GTLS_AUTH_CERTFINGERPRINT = 1,
            GTLS_AUTH_CERTVALID = 2,
            GTLS_AUTH_CERTANON = 3
        } authMode;
        enum { GTLS_EXPIRED_PERMIT = 0, GTLS_EXPIRED_DENY = 1, GTLS_EXPIRED_WARN = 2 } permitExpiredCerts;
        enum { GTLS_NONE = 0, GTLS_PURPOSE = 1 } dataTypeCheck;
        int bSANpriority; /* if true, we do stricter checking (if any SAN present we do not cehck CN) */
        gtlsRtryCall_t rtryCall; /**< what must we retry? */
        int bIsInitiator; /**< 0 if socket is the server end (listener), 1 if it is the initiator */
        gnutls_session_t sess;
        int bHaveSess; /* as we don't know exactly which gnutls_session values
                   are invalid, we use this one to flag whether or
                   not we are in a session (same as -1 for a socket
                   meaning no sess) */
        int bReportAuthErr; /* only the first auth error is to be reported, this var triggers it. Initially, it is
                             * set to 1 and changed to 0 after the first report. It is changed back to 1 after
                             * one successful authentication. */
        permittedPeers_t *pPermPeers; /* permitted peers */
        uchar *gnutlsPriorityString; /* gnutls priority string */
        int DrvrVerifyDepth; /* Verify Depth for certificate chains */
        gnutls_x509_crt_t pOurCerts[NSD_GTLS_MAX_CERT]; /**< our certificate, if in client mode
                                (unused in server mode) */
        unsigned int nOurCerts; /* number of certificates in our chain */
        gnutls_x509_privkey_t ourKey; /**< our private key, if in client mode (unused in server mode) */
        short bOurCertIsInit; /**< 1 if our certificate is initialized and must be deinit on destruction */
        short bOurKeyIsInit; /**< 1 if our private key is initialized and must be deinit on destruction */
        unsigned short loggedWarnings; /**< bitfield of logged warnings */
        char *pszRcvBuf;
        int lenRcvBuf;
        /**< -1: empty, 0: connection closed, 1..NSD_GTLS_MAX_RCVBUF-1: data of that size present */
        int ptrRcvBuf; /**< offset for next recv operation if 0 < lenRcvBuf < NSD_GTLS_MAX_RCVBUF */
};

/* interface is defined in nsd.h, we just implement it! */
#define nsd_gtlsCURR_IF_VERSION nsdCURR_IF_VERSION

/* prototypes */
PROTOTYPEObj(nsd_gtls);
/* some prototypes for things used by our nsdsel_gtls helper class */
uchar *gtlsStrerror(int error);
rsRetVal gtlsChkPeerAuth(nsd_gtls_t *pThis);
rsRetVal gtlsRecordRecv(nsd_gtls_t *pThis, unsigned *);

/* the name of our library binary */
#define LM_NSD_GTLS_FILENAME "lmnsd_gtls"

#if GNUTLS_VERSION_NUMBER <= 0x00030000
    #define GTLS_ANON_PRIO_NOTLSV13 "NORMAL:-VERS-TLS1.3:+ANON-DH:+COMP-ALL"
    #define GTLS_ANON_PRIO "NORMAL:+ANON-DH:+COMP-ALL"
#else
    #define GTLS_ANON_PRIO_NOTLSV13 "NORMAL:-VERS-TLS1.3:+ANON-DH:+ANON-ECDH:+COMP-ALL"
    #define GTLS_ANON_PRIO "NORMAL:+ANON-DH:+ANON-ECDH:+COMP-ALL"
#endif

#if GNUTLS_VERSION_MAJOR > 3 || (GNUTLS_VERSION_MAJOR == 3 && GNUTLS_VERSION_MINOR >= 4)
    #define EXTENDED_CERT_CHECK_AVAILABLE
#endif

#endif /* #ifndef INCLUDED_NSD_GTLS_H */
