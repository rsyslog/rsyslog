/* Definitions for tcpsrv class.
 *
 * Copyright 2008-2025 Adiscon GmbH.
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
#ifndef INCLUDED_TCPSRV_H
#define INCLUDED_TCPSRV_H

#if defined(ENABLE_IMTCP_EPOLL) && defined(HAVE_SYS_EPOLL_H)
    #include <sys/epoll.h>
#endif

#include "obj.h"
#include "prop.h"
#include "net.h"
#include "tcps_sess.h"
#include "statsobj.h"
#include "persourceratelimit.h"
#include "ruleset.h"

/* support for framing anomalies */
typedef enum ETCPsyslogFramingAnomaly {
    frame_normal = 0,
    frame_NetScreen = 1,
    frame_CiscoIOS = 2
} eTCPsyslogFramingAnomaly;

#define TCPSRV_CONNINFO_SIZE 128
#define DEFAULT_STARVATIONMAXREADS 500


/* config parameters for TCP listeners */
struct tcpLstnParams_s {
    const uchar *pszPort; /**< the ports the listener shall listen on */
    const uchar *pszAddr; /**< the addrs the listener shall listen on */
    sbool bSuppOctetFram; /**< do we support octect-counted framing? (if no->legay only!)*/
    sbool bSPFramingFix; /**< support work-around for broken Cisco ASA framing? */
    sbool bPreserveCase; /**< preserve case in fromhost */
    const uchar *pszLstnPortFileName; /**< File in which the dynamic port is written */
    char *pszNetworkNamespace; /**< network namespace to use */
    uchar *pszStrmDrvrName; /**< stream driver to use */
    uchar *pszInputName; /**< value to be used as input name */
    prop_t *pInputName;
    ruleset_t *pRuleset; /**< associated ruleset */
    uchar dfltTZ[8]; /**< default TZ if none in timestamp; '\0' =No Default */
};

/* list of tcp listen ports */
struct tcpLstnPortList_s {
    tcpLstnParams_t *cnf_params; /**< listener config parameters */
    tcpsrv_t *pSrv; /**< pointer to higher-level server instance */
    statsobj_t *stats; /**< associated stats object */
    ratelimit_t *ratelimiter;
    STATSCOUNTER_DEF(ctrSubmit, mutCtrSubmit)
    tcpLstnPortList_t *pNext; /**< next port or NULL */
};


typedef struct tcpsrvWrkrData_s {
    statsobj_t *stats;
    STATSCOUNTER_DEF(ctrRuns, mutCtrRuns);
    STATSCOUNTER_DEF(ctrRead, mutCtrRead);
    STATSCOUNTER_DEF(ctrEmptyRead, mutCtrEmptyRead);
    STATSCOUNTER_DEF(ctrStarvation, mutCtrStarvation);
    STATSCOUNTER_DEF(ctrAccept, mutCtrAccept);
} tcpsrvWrkrData_t;

typedef struct workQueue_s {
    tcpsrv_io_descr_t *head;
    tcpsrv_io_descr_t *tail;
    pthread_mutex_t mut;
    pthread_cond_t workRdy;
    unsigned numWrkr; /* how many workers to spawn */
    pthread_t *wrkr_tids; /* array of thread IDs */
    tcpsrvWrkrData_t *wrkr_data;
} workQueue_t;

/**
 * The following structure is a descriptor for tcpsrv i/o. It is
 * primarily used together with epoll at the moment.
 */
struct tcpsrv_io_descr_s {
    int id; /* index into listener or session table, depending on ptrType */
    int sock; /* socket descriptor we need to "monitor" */
    unsigned ioDirection;
    enum { NSD_PTR_TYPE_LSTN, NSD_PTR_TYPE_SESS } ptrType;
    union {
        tcps_sess_t *pSess;
        netstrm_t **ppLstn; /**<  accept listener's netstream */
    } ptr;
    int isInError; /* boolean, if set, subsystem indicates we need to close because we had an
                    * unrecoverable error at the network layer. */
    tcpsrv_t *pSrv; /* our server object */
    tcpsrv_io_descr_t *next; /* for use in workQueue_t */
#if defined(ENABLE_IMTCP_EPOLL)
    struct epoll_event event; /* to re-enable EPOLLONESHOT */
#endif
    DEF_ATOMIC_HELPER_MUT(mut_isInError);
};

#define TCPSRV_NO_ADDTL_DELIMITER -1 /* specifies that no additional delimiter is to be used in TCP framing */

/* the tcpsrv object */
struct tcpsrv_s {
    BEGINobjInstance
        ; /**< Data to implement generic object - MUST be the first data element! */
        int bUseKeepAlive; /**< use socket layer KEEPALIVE handling? */
        int iKeepAliveIntvl; /**< socket layer KEEPALIVE interval */
        int iKeepAliveProbes; /**< socket layer KEEPALIVE probes */
        int iKeepAliveTime; /**< socket layer KEEPALIVE timeout */
        netstrms_t *pNS; /**< pointer to network stream subsystem */
        int iDrvrMode; /**< mode of the stream driver to use */
        int DrvrChkExtendedKeyUsage; /**< if true, verify extended key usage in certs */
        int DrvrPrioritizeSan; /**< if true, perform stricter checking of names in certs */
        int DrvrTlsVerifyDepth; /**< Verify Depth for certificate chains */
        uchar *gnutlsPriorityString; /**< priority string for gnutls */
        uchar *pszLstnPortFileName; /**< File in which the dynamic port is written */
        uchar *pszDrvrAuthMode; /**< auth mode of the stream driver to use */
        uchar *pszDrvrPermitExpiredCerts; /**< current driver setting for handlign expired certs */
        uchar *pszDrvrCAFile;
        uchar *pszDrvrCRLFile;
        uchar *pszDrvrKeyFile;
        uchar *pszDrvrCertFile;
        uchar *pszDrvrName; /**< name of stream driver to use */
        uchar *pszInputName; /**< value to be used as input name */
        uchar *pszOrigin; /**< module to be used as "origin" (e.g. for pstats) */
        ruleset_t *pRuleset; /**< ruleset to bind to */
        permittedPeers_t *pPermPeers; /**< driver's permitted peers */
        sbool bEmitMsgOnClose; /**< emit an informational message when the remote peer closes connection */
        sbool bEmitMsgOnOpen;
        sbool bUseFlowControl; /**< use flow control (make light delayable) */
        sbool bSPFramingFix; /**< support work-around for broken Cisco ASA framing? */
        int iLstnCurr; /**< max nbr of listeners currently supported */
        netstrm_t **ppLstn; /**< our netstream listeners */
        /* We could use conditional compilation, but that causes more complexity and is (proofen causing errors) */
        union {
            struct {
                int efd;
            } epoll;
            struct {
                uint32_t maxfds;
                uint32_t currfds;
                struct pollfd *fds;
            } poll;
        } evtdata;
        tcpLstnPortList_t **ppLstnPort; /**< pointer to relevant listen port description */
        tcpsrv_io_descr_t **ppioDescrPtr; /**< pointer to i/o descriptor object */
        int iLstnMax; /**< max number of listeners supported */
        int iSessMax; /**< max number of sessions supported */
        uchar dfltTZ[8]; /**< default TZ if none in timestamp; '\0' =No Default */
        tcpLstnPortList_t *pLstnPorts; /**< head pointer for listen ports */

        int addtlFrameDelim; /**< additional frame delimiter for plain TCP syslog
                     framing (e.g. to handle NetScreen) */
        int maxFrameSize; /**< max frame size for octet counted*/
        int bDisableLFDelim; /**< if 1, standard LF frame delimiter is disabled (*very dangerous*) */
        int discardTruncatedMsg; /**< discard msg part that has been truncated*/
        sbool bPreserveCase; /**< preserve case in fromhost */
        int iSynBacklog;
        unsigned int ratelimitInterval;
        unsigned int ratelimitBurst;
        perSourceRateLimiter_t *perSourceLimiter;
        struct template *perSourceKeyTpl;
        int bPerSourcePolicyReloadOnHUP;
        tcps_sess_t **pSessions; /**< array of all of our sessions */
        void *pUsr; /**< a user-pointer */

        workQueue_t workQueue;
        unsigned starvationMaxReads;
        int currWrkrs;

        /* callbacks */
        int (*pIsPermittedHost)(struct sockaddr *addr, char *fromHostFQDN, void *pUsr, void *pSessUsr);
        rsRetVal (*DoSubmitMessage)(tcps_sess_t *pSess, struct syslogTime *stTime, time_t ttGenTime,
                                    multi_submit_t *pMultiSub);
        rsRetVal (*OpenLstnSocks)(tcpsrv_t *);
        rsRetVal (*pOnSessAccept)(tcpsrv_t *, tcps_sess_t *, char *);
        rsRetVal (*OnSessConstructFinalize)(void *);
        rsRetVal (*pOnSessDestruct)(void *);
        rsRetVal (*pOnRegularClose)(tcps_sess_t *pSess);
        rsRetVal (*pOnErrClose)(tcps_sess_t *pSess);
        rsRetVal (*pRcvData)(tcps_sess_t *, char *, size_t, ssize_t *, int *, unsigned *);
        rsRetVal (*OnMsgReceive)(tcps_sess_t *, uchar *, int);
        rsRetVal (*OnDestruct)(void *);
        rsRetVal (*pOnListenDeinit)(void *);

        /* v6 additions */
        rsRetVal (*OnPubKeyAuth)(tcps_sess_t *pSess, long unsigned eKeyUsage, uchar *x509_subject);
    };

/* ... */

BEGINinterface(tcpsrv) /* name must also be changed in ENDinterface macro! */
    rsRetVal (*Construct)(tcpsrv_t **ppThis);
    rsRetVal (*ConstructFinalize)(tcpsrv_t *pThis);
    rsRetVal (*Destruct)(tcpsrv_t **ppThis);
    rsRetVal (*DebugPrint)(tcpsrv_t *pThis);
    rsRetVal (*configureTCPListen)(tcpsrv_t *pThis, tcpLstnParams_t *cnf_params);
    rsRetVal (*create_tcp_socket)(tcpsrv_t *pThis);
    rsRetVal (*Run)(tcpsrv_t *pThis);
    rsRetVal (*SetKeepAlive)(tcpsrv_t *pThis, int iVal);
    rsRetVal (*SetKeepAliveIntvl)(tcpsrv_t *pThis, int iVal);
    rsRetVal (*SetKeepAliveProbes)(tcpsrv_t *pThis, int iVal);
    rsRetVal (*SetKeepAliveTime)(tcpsrv_t *pThis, int iVal);
    rsRetVal (*SetGnutlsPriorityString)(tcpsrv_t *pThis, uchar *pszPriorityString);
    rsRetVal (*SetUsrP)(tcpsrv_t *pThis, void *pUsr);
    rsRetVal (*SetInputName)(tcpsrv_t *pThis, tcpLstnParams_t *const cnf_params, const uchar *const name);
    rsRetVal (*SetOrigin)(tcpsrv_t *pThis, uchar *origin);
    rsRetVal (*SetDfltTZ)(tcpsrv_t *pThis, uchar *dfltTZ);
    rsRetVal (*SetbSPFramingFix)(tcpsrv_t *pThis, const sbool bSPFramingFix);
    rsRetVal (*SetAddtlFrameDelim)(tcpsrv_t *pThis, int iDelim);
    rsRetVal (*SetMaxFrameSize)(tcpsrv_t *pThis, int iMax);
    rsRetVal (*SetbDisableLFDelim)(tcpsrv_t *pThis, int bDisable);
    rsRetVal (*SetDiscardTruncatedMsg)(tcpsrv_t *pThis, int bDiscard);
    rsRetVal (*SetSessMax)(tcpsrv_t *pThis, int iMax);
    rsRetVal (*SetUseFlowControl)(tcpsrv_t *pThis, int bUse);
    rsRetVal (*SetLstnMax)(tcpsrv_t *pThis, int iMax);
    rsRetVal (*SetDrvrMode)(tcpsrv_t *pThis, int iMode);
    rsRetVal (*SetDrvrAuthMode)(tcpsrv_t *pThis, uchar *mode);
    rsRetVal (*SetDrvrPermitExpiredCerts)(tcpsrv_t *pThis, uchar *mode);
    rsRetVal (*SetDrvrCAFile)(tcpsrv_t *pThis, uchar *mode);
    rsRetVal (*SetDrvrCRLFile)(tcpsrv_t *pThis, uchar *mode);
    rsRetVal (*SetDrvrKeyFile)(tcpsrv_t *pThis, uchar *mode);
    rsRetVal (*SetDrvrCertFile)(tcpsrv_t *pThis, uchar *mode);
    rsRetVal (*SetDrvrName)(tcpsrv_t *pThis, uchar *name);
    rsRetVal (*SetDrvrPermPeers)(tcpsrv_t *pThis, permittedPeers_t *pPermPeers);
    rsRetVal (*SetCBIsPermittedHost)(tcpsrv_t *pThis, int (*pCB)(struct sockaddr *addr, char *fromHostFQDN, void *, void *));
    rsRetVal (*SetCBOpenLstnSocks)(tcpsrv_t *pThis, rsRetVal (*pCB)(tcpsrv_t *));
    rsRetVal (*SetCBRcvData)(tcpsrv_t *pThis, rsRetVal (*pRcvData)(tcps_sess_t *, char *, size_t, ssize_t *, int *, unsigned *));
    rsRetVal (*SetCBOnListenDeinit)(tcpsrv_t *pThis, rsRetVal (*pCB)(void *));
    rsRetVal (*SetCBOnSessAccept)(tcpsrv_t *pThis, rsRetVal (*pCB)(tcpsrv_t *, tcps_sess_t *, char *));
    rsRetVal (*SetCBOnSessConstructFinalize)(tcpsrv_t *pThis, rsRetVal (*pCB)(void *));
    rsRetVal (*SetCBOnSessDestruct)(tcpsrv_t *pThis, rsRetVal (*pCB)(void *));
    rsRetVal (*SetCBOnDestruct)(tcpsrv_t *pThis, rsRetVal (*pCB)(void *));
    rsRetVal (*SetCBOnRegularClose)(tcpsrv_t *pThis, rsRetVal (*pCB)(tcps_sess_t *));
    rsRetVal (*SetCBOnErrClose)(tcpsrv_t *pThis, rsRetVal (*pCB)(tcps_sess_t *));
    rsRetVal (*SetOnMsgReceive)(tcpsrv_t *pThis, rsRetVal (*OnMsgReceive)(tcps_sess_t *, uchar *, int));
    rsRetVal (*SetLinuxLikeRatelimiters)(tcpsrv_t *pThis, unsigned int ratelimitInterval, unsigned int ratelimitBurst);
    rsRetVal (*SetNotificationOnRemoteClose)(tcpsrv_t *pThis, int bNewVal);
    rsRetVal (*SetNotificationOnRemoteOpen)(tcpsrv_t *pThis, int bNewVal);
    rsRetVal (*SetPreserveCase)(tcpsrv_t *pThis, int bPreserveCase);
    rsRetVal (*SetDrvrCheckExtendedKeyUsage)(tcpsrv_t *pThis, int ChkExtendedKeyUsage);
    rsRetVal (*SetDrvrPrioritizeSAN)(tcpsrv_t *pThis, int prioritizeSan);
    rsRetVal (*SetDrvrTlsVerifyDepth)(tcpsrv_t *pThis, int verifyDepth);
    rsRetVal (*SetSynBacklog)(tcpsrv_t *pThis, int iSynBacklog);
    rsRetVal (*SetNumWrkr)(tcpsrv_t *pThis, int numWrkr);
    rsRetVal (*SetStarvationMaxReads)(tcpsrv_t *pThis, unsigned int maxReads);
    rsRetVal (*SetNetworkNamespace)(tcpsrv_t *pThis, tcpLstnParams_t *const cnf_params,
                                    const char *const networkNamespace);
    /* added v30 */
    rsRetVal (*SetPerSourceRateLimiter)(tcpsrv_t *pThis, perSourceRateLimiter_t *limiter);
    rsRetVal (*SetPerSourceKeyTpl)(tcpsrv_t *pThis, struct template *tpl);
    rsRetVal (*ReloadPerSourceRateLimiter)(tcpsrv_t *pThis);
    rsRetVal (*SetPerSourcePolicyReloadOnHUP)(tcpsrv_t *pThis, int bReload);

ENDinterface(tcpsrv)
#define tcpsrvCURR_IF_VERSION 30 /* increment whenever you change the interface structure! */
/* change for v4:
 * - SetAddtlFrameDelim() added -- rgerhards, 2008-12-10
 * - SetInputName() added -- rgerhards, 2008-12-10
 * change for v5 and up: see above
 * for v12: param bSuppOctetFram added to configureTCPListen
 * for v20: add oserr to setCBRcvData signature -- rgerhards, 2017-09-04
 */


/* prototypes */
PROTOTYPEObj(tcpsrv);

/* the name of our library binary */
#define LM_TCPSRV_FILENAME "lmtcpsrv"

#endif /* #ifndef INCLUDED_TCPSRV_H */
