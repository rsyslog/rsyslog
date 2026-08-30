/* Definitions for tcpsrv class.
 *
 * Copyright 2008-2026 Adiscon GmbH.
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

#include <time.h>

#if defined(ENABLE_IMTCP_EPOLL) && defined(HAVE_SYS_EPOLL_H)
    #include <sys/epoll.h>
#endif

#include "obj.h"
#include "prop.h"
#include "net.h"
#include "tcps_sess.h"
#include "statsobj.h"
#include "regexp.h"

/* support for framing anomalies */
typedef enum ETCPsyslogFramingAnomaly {
    frame_normal = 0,
    frame_NetScreen = 1,
    frame_CiscoIOS = 2
} eTCPsyslogFramingAnomaly;


/**
 * Config parameters for TCP listeners.
 *
 * Ownership: the caller allocates and populates this struct, then hands it to
 * tcpsrv via `configureTCPListen()`. tcpsrv becomes the owner (including any
 * dynamically allocated fields) and will free it on error or when the listener
 * is torn down.
 */
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
    sbool bMultiLine; /**< support multi-line messages */
    uchar *pszStartRegex; /**< regex that indicates start of frame */
    uchar *pszRatelimitName; /**< name of rate limit configuration */
    struct AllowedSenders *pAllowedSenderRoot; /**< source-address ACL list */
    sbool bUseLegacyAllowedSender; /**< if true, use protocol-global legacy ACLs */
    sbool bDeferListen; /**< control-path prepare binds without accepting connections */
    sbool bOwnAllowedSenderRoot; /**< listener owns and destroys its private ACL clone */
};

/* list of tcp listen ports */
struct tcpLstnPortList_s {
    tcpLstnParams_t *cnf_params; /**< listener config parameters */
    tcpsrv_t *pSrv; /**< pointer to higher-level server instance */
    statsobj_t *stats; /**< associated stats object */
    ratelimit_t *ratelimiter;
    STATSCOUNTER_DEF(ctrSubmit, mutCtrSubmit)
    STATSCOUNTER_DEF(ctrReloadAclDropped, mutCtrReloadAclDropped)
    STATSCOUNTER_DEF(ctrBytesRcvd, mutCtrBytesRcvd)
    STATSCOUNTER_DEF(ctrBytesDecompressed, mutCtrBytesDecompressed)
    STATSCOUNTER_DEF(ctrDecompressErr, mutCtrDecompressErr)
    uint8_t compressionMode;
    uint8_t compressionDriver;
    uint64_t compressionMaxExpansionRatio;
    uint64_t compressionMaxDecompressedBytesPerReceive;
    uint64_t compressionMaxTotalZstdWindowBytes;
    /* Concurrency & Locking: sessions have independent decoder state and share
     * only this listener-wide zstd window counter. Native 64-bit atomics avoid
     * serialization; the helper mutex exists only on no-64-bit-atomics builds. */
    uint64 compressionZstdWindowBytesInUse;
    DEF_ATOMIC_HELPER_MUT64(mutCompressionZstdWindow);
#ifdef FEATURE_REGEXP
    regex_t start_preg;
    sbool bHasStartRegex;
#endif
    tcpLstnPortList_t *pNext; /**< next port or NULL */
#ifdef FEATURE_REGEXP
    /* Whether start_preg itself owns compiled state. bHasStartRegex describes
     * the accept profile and may remain true after a reload transfers only a
     * prevalidated pattern for session-local compilation. */
    sbool bStartRegexCompiled;
#endif
};

/* Fully validated control-path values applied while a reload fence is held.
 * The operation is deliberately infallible: preparation owns validation and
 * resolution, while commit only copies scalars and stable runtime pointers. */
typedef struct tcpsrv_reload_profile_s {
    int useFlowControl;
    unsigned starvationMaxReads;
    int applyRateLimitScalars;
    unsigned ratelimitInterval;
    unsigned ratelimitBurst;
    int notifyOnConnectionClose;
    int notifyOnConnectionOpen;
    int preserveCase;
    int keepAlive;
    int keepAliveInterval;
    int keepAliveProbes;
    int keepAliveTime;
    int framingFix;
    int additionalFrameDelimiter;
    int maxFrameSize;
    int disableLFDelimiter;
    int discardTruncatedMessage;
    int supportOctetCountedFraming;
    int compressionMode;
    int compressionDriver;
    uint64_t compressionMaxExpansionRatio;
    uint64_t compressionMaxDecompressedBytesPerReceive;
    uint64_t compressionMaxTotalZstdWindowBytes;
    int multiLine;
    uchar *startRegex; /* prepared ownership transfers during commit */
    uchar defaultTZ[8];
    ruleset_t *ruleset;
} tcpsrv_reload_profile_t;

/* Listener table storage prepared off-path and transferred while the event
 * loop and workers are parked at the reload fence. The table entries remain
 * borrowed runtime objects; only the three pointer arrays are owned here. */
typedef struct tcpsrv_listener_tables_s {
    netstrm_t **streams;
    tcpLstnPortList_t **ports;
    tcpsrv_io_descr_t **descriptors;
    int capacity;
} tcpsrv_listener_tables_t;

typedef rsRetVal (*tcpsrv_reload_session_policy_eval_t)(tcps_sess_t *session, void *context, int *allowed);


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
    int stop; /* server-local worker shutdown, independent of global TERM */
} workQueue_t;

/**
 * The following structure is a descriptor for tcpsrv i/o. It is
 * primarily used together with epoll at the moment.
 */
struct tcpsrv_io_descr_s {
    int id; /* index into listener or session table, depending on ptrType */
    int sock; /* socket descriptor we need to "monitor" */
    unsigned ioDirection;
    enum { NSD_PTR_TYPE_LSTN, NSD_PTR_TYPE_SESS, NSD_PTR_TYPE_FENCE, NSD_PTR_TYPE_CONTROL } ptrType;
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
        int DrvrTlsRevocationCheck; /**< Enable TLS revocation checking (OCSP/CRL) */
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
        uint8_t compressionMode;
        uint8_t compressionDriver;
        uint64_t compressionMaxExpansionRatio;
        uint64_t compressionMaxDecompressedBytesPerReceive;
        uint64_t compressionMaxTotalZstdWindowBytes;
        int bDisableLFDelim; /**< if 1, standard LF frame delimiter is disabled (*very dangerous*) */
        int discardTruncatedMsg; /**< discard msg part that has been truncated*/
        sbool bPreserveCase; /**< preserve case in fromhost */
        int iSynBacklog;
        unsigned int ratelimitInterval;
        unsigned int ratelimitBurst;
        tcps_sess_t **pSessions; /**< session slots shared by accept and worker threads */
        DEF_ATOMIC_HELPER_MUT(mut_sessions);
        unsigned int starvationMaxReads;
        void *pUsr; /**< a user-settable pointer (provides extensibility for "derived classes")*/
        /* callbacks */
        int (*pIsPermittedHost)(struct sockaddr *addr, char *fromHostFQDN, void *pUsrSrv, void *pUsrSess);
        rsRetVal (*pRcvData)(tcps_sess_t *, char *, size_t, ssize_t *, int *, unsigned *);
        rsRetVal (*OpenLstnSocks)(struct tcpsrv_s *);
        rsRetVal (*pOnListenDeinit)(void *);
        rsRetVal (*OnDestruct)(void *);
        rsRetVal (*pOnRegularClose)(tcps_sess_t *pSess);
        rsRetVal (*pOnErrClose)(tcps_sess_t *pSess);
        /* session specific callbacks */
        rsRetVal (*pOnSessAccept)(tcpsrv_t *, tcps_sess_t *, char *connInfo);
#define TCPSRV_CONNINFO_SIZE (2 * (INET_ADDRSTRLEN + 20))
        rsRetVal (*OnSessConstructFinalize)(void *);
        rsRetVal (*pOnSessDestruct)(void *);
        rsRetVal (*OnMsgReceive)(tcps_sess_t *, uchar *pszMsg, int iLenMsg); /* submit message callback */
        /* work queue */
        workQueue_t workQueue;
        int currWrkrs;
        /* Append-only control-path state. Existing instance-field offsets stay stable. */
        int controlPipe[2]; /**< nonblocking self-pipe used to wake poll/epoll */
        tcpsrv_io_descr_t controlDescr;
        tcpsrv_io_descr_t *fenceItems; /**< stable FIFO sentinels, one per worker */
        pthread_mutex_t fenceMut;
        pthread_cond_t fenceCond;
        uint64_t fenceGeneration;
        unsigned fenceAcks;
        unsigned fenceParked;
        unsigned fenceOutstanding;
        sbool fenceEventLoopParked;
        pthread_t fenceOwner;
        sbool fenceOwnerValid;
        sbool fenceRequested;
        sbool fenceActive;
        sbool fenceRelease;
        sbool fenceAcquired;
        sbool fenceReleaseCommitted;
        sbool fenceSyncInitialized;
        sbool fenceReady;
        sbool strictListenerInit; /**< candidate prepare requires every resolved socket */
        sbool retireWhenDrained; /**< accept is disabled; stop after the last session closes */
};


/**
 */
struct tcpsrv_workset_s {
    int idx; /**< index into session table (or -1 if listener) */
    void *pUsr;
};


/* interfaces */
BEGINinterface(tcpsrv) /* name must also be changed in ENDinterface macro! */
    INTERFACEObjDebugPrint(tcpsrv);
    rsRetVal (*Construct)(tcpsrv_t **ppThis);
    rsRetVal (*ConstructFinalize)(tcpsrv_t __attribute__((unused)) * pThis);
    rsRetVal (*Destruct)(tcpsrv_t **ppThis);
    /**
     * Configure a TCP listener using the provided configuration parameters.
     *
     * Ownership of `cnf_params` is always transferred to tcpsrv. On success,
     * tcpsrv retains the configuration for the listener. On failure (including
     * invalid port), tcpsrv frees `cnf_params` and its owned fields.
     */
    rsRetVal (*ATTR_NONNULL(1, 2) configureTCPListen)(tcpsrv_t *, tcpLstnParams_t *const cnf_params);
    rsRetVal (*create_tcp_socket)(tcpsrv_t *pThis);
    rsRetVal (*Run)(tcpsrv_t *pThis);
    /* set methods */
    rsRetVal (*SetAddtlFrameDelim)(tcpsrv_t *, int);
    rsRetVal (*SetMaxFrameSize)(tcpsrv_t *, int);
    rsRetVal (*SetCompressionMode)(tcpsrv_t *, int);
    rsRetVal (*SetCompressionDriver)(tcpsrv_t *, int);
    rsRetVal (*SetCompressionMaxExpansionRatio)(tcpsrv_t *, uint64_t);
    rsRetVal (*SetCompressionMaxDecompressedBytesPerReceive)(tcpsrv_t *, uint64_t);
    /* added v32 */
    rsRetVal (*SetCompressionMaxTotalZstdWindowBytes)(tcpsrv_t *, uint64_t);
    /**
     * Set the input name for a listener configuration.
     *
     * Ownership: the name is duplicated into `cnf_params` and managed by
     * tcpsrv along with the rest of `cnf_params` after ownership transfer.
     */
    rsRetVal (*SetInputName)(tcpsrv_t *const pThis, tcpLstnParams_t *const cnf_params, const uchar *const name);
    rsRetVal (*SetUsrP)(tcpsrv_t *, void *);
    rsRetVal (*SetCBIsPermittedHost)(tcpsrv_t *, int (*)(struct sockaddr *addr, char *, void *, void *));
    rsRetVal (*SetCBOpenLstnSocks)(tcpsrv_t *, rsRetVal (*)(tcpsrv_t *));
    rsRetVal (*SetCBRcvData)(tcpsrv_t *pThis,
                             rsRetVal (*pRcvData)(tcps_sess_t *, char *, size_t, ssize_t *, int *, unsigned *));
    rsRetVal (*SetCBOnListenDeinit)(tcpsrv_t *, rsRetVal (*)(void *));
    rsRetVal (*SetCBOnDestruct)(tcpsrv_t *, rsRetVal (*)(void *));
    rsRetVal (*SetCBOnRegularClose)(tcpsrv_t *, rsRetVal (*)(tcps_sess_t *));
    rsRetVal (*SetCBOnErrClose)(tcpsrv_t *, rsRetVal (*)(tcps_sess_t *));
    rsRetVal (*SetDrvrMode)(tcpsrv_t *pThis, int iMode);
    rsRetVal (*SetDrvrAuthMode)(tcpsrv_t *pThis, uchar *pszMode);
    rsRetVal (*SetDrvrPermitExpiredCerts)(tcpsrv_t *pThis, uchar *pszMode);
    rsRetVal (*SetDrvrPermPeers)(tcpsrv_t *pThis, permittedPeers_t *);
    /* session specifics */
    rsRetVal (*SetCBOnSessAccept)(tcpsrv_t *, rsRetVal (*)(tcpsrv_t *, tcps_sess_t *, char *));
    rsRetVal (*SetCBOnSessDestruct)(tcpsrv_t *, rsRetVal (*)(void *));
    rsRetVal (*SetCBOnSessConstructFinalize)(tcpsrv_t *, rsRetVal (*)(void *));
    /* added v5 */
    rsRetVal (*SetSessMax)(tcpsrv_t *pThis, int iMaxSess); /* 2009-04-09 */
    /* added v6 */
    rsRetVal (*SetOnMsgReceive)(tcpsrv_t *pThis,
                                rsRetVal (*OnMsgReceive)(tcps_sess_t *, uchar *, int)); /* 2009-05-24 */
    rsRetVal (*SetRuleset)(tcpsrv_t *pThis, ruleset_t *); /* 2009-06-12 */
    /* added v7 (accidentally named v8!) */
    rsRetVal (*SetLstnMax)(tcpsrv_t *pThis, int iMaxLstn); /* 2009-08-17 */
    rsRetVal (*SetNotificationOnRemoteClose)(tcpsrv_t *pThis, int bNewVal); /* 2009-10-01 */
    rsRetVal (*SetNotificationOnRemoteOpen)(tcpsrv_t *pThis, int bNewVal); /* 2022-08-23 */
    /* added v9 -- rgerhards, 2010-03-01 */
    rsRetVal (*SetbDisableLFDelim)(tcpsrv_t *, int);
    /* added v10 -- rgerhards, 2011-04-01 */
    rsRetVal (*SetDiscardTruncatedMsg)(tcpsrv_t *, int);
    rsRetVal (*SetUseFlowControl)(tcpsrv_t *, int);
    /* added v11 -- rgerhards, 2011-05-09 */
    rsRetVal (*SetKeepAlive)(tcpsrv_t *, int);
    /* added v13 -- rgerhards, 2012-10-15 */
    rsRetVal (*SetLinuxLikeRatelimiters)(tcpsrv_t *pThis, unsigned int interval, unsigned int burst);
    /* added v14 -- rgerhards, 2013-07-28 */
    rsRetVal (*SetDfltTZ)(tcpsrv_t *pThis, uchar *dfltTZ);
    /* added v15 -- rgerhards, 2013-09-17 */
    rsRetVal (*SetDrvrName)(tcpsrv_t *pThis, uchar *pszName);
    /* added v16 -- rgerhards, 2014-09-08 */
    rsRetVal (*SetOrigin)(tcpsrv_t *, uchar *);
    /* added v17 */
    rsRetVal (*SetKeepAliveIntvl)(tcpsrv_t *, int);
    rsRetVal (*SetKeepAliveProbes)(tcpsrv_t *, int);
    rsRetVal (*SetKeepAliveTime)(tcpsrv_t *, int);
    /* added v18 */
    rsRetVal (*SetbSPFramingFix)(tcpsrv_t *, sbool);
    /* added v19 -- PascalWithopf, 2017-08-08 */
    rsRetVal (*SetGnutlsPriorityString)(tcpsrv_t *, uchar *);
    /* added v21 -- Preserve case in fromhost, 2018-08-16 */
    rsRetVal (*SetPreserveCase)(tcpsrv_t *pThis, int bPreserveCase);
    /* added v23 -- Options for stricter driver behavior, 2019-08-16 */
    rsRetVal (*SetDrvrCheckExtendedKeyUsage)(tcpsrv_t *pThis, int ChkExtendedKeyUsage);
    rsRetVal (*SetDrvrPrioritizeSAN)(tcpsrv_t *pThis, int prioritizeSan);
    /* added v24 -- Options for TLS verify depth driver behavior, 2019-12-20 */
    rsRetVal (*SetDrvrTlsVerifyDepth)(tcpsrv_t *pThis, int verifyDepth);
    rsRetVal (*SetDrvrTlsRevocationCheck)(tcpsrv_t *pThis, int enabled);
    /* added v25 -- Options for TLS certificates, 2021-07-19 */
    rsRetVal (*SetDrvrCAFile)(tcpsrv_t *pThis, uchar *pszMode);
    rsRetVal (*SetDrvrKeyFile)(tcpsrv_t *pThis, uchar *pszMode);
    rsRetVal (*SetDrvrCertFile)(tcpsrv_t *pThis, uchar *pszMode);
    /* added v26 -- Options for TLS CRL file */
    rsRetVal (*SetDrvrCRLFile)(tcpsrv_t *pThis, uchar *pszMode);
    /* added v27 -- sync backlog for listen() */
    rsRetVal (*SetSynBacklog)(tcpsrv_t *pThis, int);
    /* added v28 */
    rsRetVal (*SetNumWrkr)(tcpsrv_t *pThis, int);
    rsRetVal (*SetStarvationMaxReads)(tcpsrv_t *pThis, unsigned int);
    /* added v29 */
    /*
     * @brief Set the Network Namespace into the listener parameters
     * @param pThis The associated TCP Server instance
     * @param cnf_params The listener parameters to configure
     * @param networkNamespace The namespace parameter to set into the
     *                         listener configuration parameters
     * @return RS_RET_OK on success, otherwise a failure code.
     * @details For platforms that do not support network namespaces,
     *          this function should fail for any non-null and non-empty
     *          namespace passed.  Note that the empty string is treated
     *          the same as a NULL, i.e. you are not allowed to actually
     *          use a network namespace with the empty string.  So both
     *          a NULL and an empty string "" both mean to use the
     *          original startup network namespace.
     */
    rsRetVal (*SetNetworkNamespace)(tcpsrv_t *pThis, tcpLstnParams_t *const cnf_params,
                                    const char *const networkNamespace);
    /* added v33 -- live-reload activation control-path fence */
    rsRetVal (*RequestFence)(tcpsrv_t *pThis, uint64_t *token);
    rsRetVal (*WaitFence)(tcpsrv_t *pThis, uint64_t token, const struct timespec *deadline);
    rsRetVal (*ReleaseFence)(tcpsrv_t *pThis, uint64_t token);
    /* added v34 -- update the accept profile while fenced */
    rsRetVal (*SetSupportOctetCountedFraming)(tcpsrv_t *pThis, int enabled);
    /* added v35 -- update the accept profile while fenced */
    rsRetVal (*SetMultiLineForNewSessions)(tcpsrv_t *pThis, int enabled);
    /* added v36 -- prepare and activate fixed listener sockets in two phases */
    rsRetVal (*ConstructFinalizePrepared)(tcpsrv_t *pThis);
    rsRetVal (*ActivatePreparedListeners)(tcpsrv_t *pThis);
    /* added v37 -- infallibly stop accepts while the reload fence is held */
    void (*DisableAcceptWhileFenced)(tcpsrv_t *pThis);
    /* added v38 -- infallibly publish a validated live/accept profile */
    void (*ApplyReloadProfile)(tcpsrv_t *pThis, const tcpsrv_reload_profile_t *profile);
    /* v39 extends the reload profile with a prevalidated session regex */
    /* v40 adds fenced ACL evaluation and infallible policy publication. */
    rsRetVal (*EvaluateSessionPolicyWhileFenced)(tcpsrv_t *server, tcpsrv_reload_session_policy_eval_t evaluate,
                                                 void *context, unsigned char *allowed, size_t allowedCount);
    void (*ApplySessionPolicyLive)(tcpsrv_t *server, const unsigned char *allowed, size_t allowedCount,
                                   rsRetVal (*blockedSubmit)(tcps_sess_t *, uchar *, int));
    void (*SwapAllowedSendersLive)(tcpsrv_t *server, struct AllowedSenders *preparedRoot, int useLegacy,
                                   struct AllowedSenders **retiredRoot, int *retiredOwned);
    /* v41 swaps a fully prepared listener-local rate limiter while fenced. */
    void (*SwapRateLimiterLive)(tcpsrv_t *server, ratelimit_t *preparedLimiter, uchar *preparedName,
                                ratelimit_t **retiredLimiter, uchar **retiredName);
    /* v42 validates and transfers listener pointer-table capacity. */
    rsRetVal (*ValidateListenerTableCapacity)(const tcpsrv_t *server, int capacity);
    void (*SwapListenerTablesLive)(tcpsrv_t *server, tcpsrv_listener_tables_t *prepared,
                                   tcpsrv_listener_tables_t *retired);

ENDinterface(tcpsrv)
#define tcpsrvCURR_IF_VERSION 42 /* increment whenever you change the interface structure! */
/* change for v4:
 * - SetAddtlFrameDelim() added -- rgerhards, 2008-12-10
 * - SetInputName() added -- rgerhards, 2008-12-10
 * change for v5 and up: see above
 * for v12: param bSuppOctetFram added to configureTCPListen
 * for v20: add oserr to setCBRcvData signature -- rgerhards, 2017-09-04
 */


/* prototypes */
PROTOTYPEObjFull(tcpsrv);
PROTOTYPEObjDebugPrint(tcpsrv);

/*
 * Request an exclusive event-loop/worker fence. The requesting thread owns
 * the token and must also Wait and Release it. A successful Wait guarantees
 * that the complete ready-I/O batch observed before the request and all work
 * queued ahead of the fence have finished, and that both the event loop and
 * every worker remain parked until Release.
 *
 * The Wait deadline is absolute CLOCK_REALTIME. A Wait error (including
 * timeout or termination) aborts and resumes the
 * fence automatically; the caller must not Release that token. While aborted
 * sentinels drain, a new Request fails closed. The caller must pin the tcpsrv
 * object and its Run lifetime through the final Wait/Release return. These are
 * control-path operations; normal TCP message processing never calls them.
 */
rsRetVal tcpsrvRequestFence(tcpsrv_t *pThis, uint64_t *token);
rsRetVal tcpsrvWaitFence(tcpsrv_t *pThis, uint64_t token, const struct timespec *deadline);
rsRetVal tcpsrvReleaseFence(tcpsrv_t *pThis, uint64_t token);
void tcpsrvActivateFence(tcpsrv_t *pThis);
void tcpsrvParkAtFence(tcpsrv_t *pThis);
void tcpsrvAbortFenceLocked(tcpsrv_t *pThis);
int tcpsrvFenceTerminated(void);

/* Live control-path mutations. The caller must hold a successfully acquired
 * tcpsrv fence whenever the server is running. Snapshot-bearing settings also
 * update listener/session copies before the fence is released. */
void tcpsrvApplyFlowControlLive(tcpsrv_t *pThis, int useFlowControl);
void tcpsrvApplyStarvationMaxReadsLive(tcpsrv_t *pThis, unsigned maxReads);
void tcpsrvApplyRateLimitLive(tcpsrv_t *pThis, unsigned interval, unsigned burst);
void tcpsrvApplyNotificationsLive(tcpsrv_t *pThis, int onOpen, int onClose);
void tcpsrvApplyPreserveCaseForNewSessions(tcpsrv_t *pThis, int preserveCase);
void tcpsrvApplyKeepAliveForNewSessions(tcpsrv_t *pThis, int enabled, int interval, int probes, int time);
void tcpsrvApplyFramingForNewSessions(tcpsrv_t *pThis,
                                      int spFramingFix,
                                      int additionalDelimiter,
                                      int maxFrameSize,
                                      int disableLFDelimiter,
                                      int discardTruncatedMessage);
void tcpsrvApplyOctetCountedFramingForNewSessions(tcpsrv_t *pThis, int enabled);
void tcpsrvApplyCompressionForNewSessions(tcpsrv_t *pThis,
                                          int mode,
                                          int driver,
                                          uint64_t maxExpansionRatio,
                                          uint64_t maxDecompressedBytesPerReceive,
                                          uint64_t maxTotalZstdWindowBytes);
void tcpsrvApplyMultiLineForNewSessions(tcpsrv_t *pThis, int enabled);
void tcpsrvApplyStartRegexForNewSessions(tcpsrv_t *pThis, uchar *preparedRegex);
void tcpsrvApplyDefaultTZLive(tcpsrv_t *pThis, const uchar *defaultTZ);
void tcpsrvApplyRulesetLive(tcpsrv_t *pThis, ruleset_t *ruleset);
rsRetVal tcpsrvEvaluateSessionPolicyWhileFenced(tcpsrv_t *server,
                                                tcpsrv_reload_session_policy_eval_t evaluate,
                                                void *context,
                                                unsigned char *allowed,
                                                size_t allowedCount);
void tcpsrvApplySessionPolicyLive(tcpsrv_t *server,
                                  const unsigned char *allowed,
                                  size_t allowedCount,
                                  rsRetVal (*blockedSubmit)(tcps_sess_t *, uchar *, int));
void tcpsrvSwapAllowedSendersLive(tcpsrv_t *server,
                                  struct AllowedSenders *preparedRoot,
                                  int useLegacy,
                                  struct AllowedSenders **retiredRoot,
                                  int *retiredOwned);
void tcpsrvSwapRateLimiterLive(tcpsrv_t *server,
                               ratelimit_t *preparedLimiter,
                               uchar *preparedName,
                               ratelimit_t **retiredLimiter,
                               uchar **retiredName);
rsRetVal tcpsrvValidateListenerTableCapacity(const tcpsrv_t *server, int capacity);
void tcpsrvSwapListenerTablesLive(tcpsrv_t *server,
                                  tcpsrv_listener_tables_t *prepared,
                                  tcpsrv_listener_tables_t *retired);

/* the name of our library binary */
#define LM_TCPSRV_FILENAME "lmtcpsrv"

#endif /* #ifndef INCLUDED_TCPSRV_H */
