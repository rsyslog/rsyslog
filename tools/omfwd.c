/* omfwd.c
 * This is the implementation of the built-in forwarding output module.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *	 works!
 *
 * Copyright 2007-2024 Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *	 -or-
 *	 see COPYING.ASL20 in the source distribution
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "config.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fnmatch.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <zlib.h>
#include <pthread.h>
#include "rsyslog.h"
#include "syslogd.h"
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "net.h"
#include "netstrms.h"
#include "netstrm.h"
#include "omfwd.h"
#include "template.h"
#include "msg.h"
#include "tcpclt.h"
#include "cfsysline.h"
#include "module-template.h"
#include "glbl.h"
#include "errmsg.h"
#include "unicode-helper.h"
#include "parserif.h"
#include "ratelimit.h"
#include "statsobj.h"
#include "datetime.h"

MODULE_TYPE_OUTPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME("omfwd")

/* internal structures
 */
DEF_OMOD_STATIC_DATA;
DEFobjCurrIf(glbl) DEFobjCurrIf(net) DEFobjCurrIf(netstrms) DEFobjCurrIf(netstrm) DEFobjCurrIf(tcpclt)
    DEFobjCurrIf(statsobj) DEFobjCurrIf(datetime)
/* some local constants (just) for better readybility */
#define IS_FLUSH 1
#define NO_FLUSH 0

        typedef struct _targetStats {
    statsobj_t *stats;
    intctr_t sentBytes;
    intctr_t sentMsgs;
    DEF_ATOMIC_HELPER_MUT64(mut_sentBytes);
    DEF_ATOMIC_HELPER_MUT64(mut_sentMsgs);
} targetStats_t;

typedef struct _instanceData {
    uchar *tplName; /* name of assigned template */
    uchar *pszStrmDrvr;
    uchar *pszStrmDrvrAuthMode;
    uchar *pszStrmDrvrPermitExpiredCerts;
    permittedPeers_t *pPermPeers;
    int iStrmDrvrMode;
    int iStrmDrvrExtendedCertCheck; /* verify also purpose OID in certificate extended field */
    int iStrmDrvrSANPreference; /* ignore CN when any SAN set */
    int iStrmTlsVerifyDepth; /**< Verify Depth for certificate chains */
    const uchar *pszStrmDrvrCAFile;
    const uchar *pszStrmDrvrCRLFile;
    const uchar *pszStrmDrvrKeyFile;
    const uchar *pszStrmDrvrCertFile;
    int nTargets;
    int nActiveTargets; /* how many targets have been active the last time? */
    DEF_ATOMIC_HELPER_MUT(mut_nActiveTargets);
    char **target_name;
    int nPorts;
    char **ports;
    char *address;
    char *device;
    int compressionLevel; /* 0 - no compression, else level for zlib */
    int protocol;
    char *networkNamespace;
    int originalNamespace;
    int iRebindInterval; /* rebind interval */
    sbool bKeepAlive;
    int iKeepAliveIntvl;
    int iKeepAliveProbes;
    int iKeepAliveTime;
    int iConErrSkip; /* skipping excessive connection errors */
    uchar *gnutlsPriorityString;
    int ipfreebind;

#define FORW_UDP 0
#define FORW_TCP 1
    /* following fields for UDP-based delivery */
    int bSendToAll;
    int iUDPSendDelay;
    int UDPSendBuf;
    /* following fields for TCP-based delivery */
    TCPFRAMINGMODE tcp_framing;
    uchar tcp_framingDelimiter;
    int bResendLastOnRecon; /* should the last message be re-sent on a successful reconnect? */
    int bExtendedConnCheck; /* do extended connection checking? */
#define COMPRESS_NEVER 0
#define COMPRESS_SINGLE_MSG 1 /* old, single-message compression */
    /* all other settings are for stream-compression */
#define COMPRESS_STREAM_ALWAYS 2
    uint8_t compressionMode;
    sbool strmCompFlushOnTxEnd; /* flush stream compression on transaction end? */
    unsigned poolResumeInterval;
    unsigned int ratelimitInterval;
    unsigned int ratelimitBurst;
    ratelimit_t *ratelimiter;
    targetStats_t *target_stats;
} instanceData;

typedef struct targetData {
    instanceData *pData;
    struct wrkrInstanceData *pWrkrData; /* forward def of struct */
    netstrms_t *pNS; /* netstream subsystem */
    netstrm_t *pNetstrm; /* our output netstream */
    char *target_name;
    char *port;
    struct addrinfo *f_addr;
    int *pSockArray; /* sockets to use for UDP */
    int bIsConnected; /* are we connected to remote host? 0 - no, 1 - yes, UDP means addr resolved */
    int nXmit; /* number of transmissions since last (re-)bind */
    tcpclt_t *pTCPClt; /* our tcpclt object */
    sbool bzInitDone; /* did we do an init of zstrm already? */
    z_stream zstrm; /* zip stream to use for tcp compression */
    /* we know int is sufficient, as we have the fixed buffer size above! so no need for size_t */
    int maxLenSndBuf; /* max usable length of sendbuf - primarily for testing */
    int offsSndBuf; /* next free spot in send buffer */
    time_t ttResume;
    targetStats_t *pTargetStats;
/* sndBuf buffer size is intentionally fixed -- see no good reason to make configurable */
#define SNDBUF_FIXED_BUFFER_SIZE (16 * 1024)
    uchar sndBuf[SNDBUF_FIXED_BUFFER_SIZE];
} targetData_t;

typedef struct wrkrInstanceData {
    instanceData *pData;
    targetData_t *target;
    int nXmit; /* number of transmissions since last (re-)bind */
    unsigned actualTarget;
    unsigned wrkrID; /* an internal monotonically increasing id for correlating worker messages */
} wrkrInstanceData_t;
static unsigned wrkrID = 0;

/* config data */
typedef struct configSettings_s {
    uchar *pszTplName; /* name of the default template to use */
    uchar *pszStrmDrvr; /* name of the stream driver to use */
    int iStrmDrvrMode; /* mode for stream driver, driver-dependent (0 mostly means plain tcp) */
    int bResendLastOnRecon; /* should the last message be re-sent on a successful reconnect? */
    uchar *pszStrmDrvrAuthMode; /* authentication mode to use */
    uchar *pszStrmDrvrPermitExpiredCerts; /* control how to handly expired certificates */
    int iTCPRebindInterval; /* support for automatic re-binding (load balancers!). 0 - no rebind */
    int iUDPRebindInterval; /* support for automatic re-binding (load balancers!). 0 - no rebind */
    int bKeepAlive;
    int iKeepAliveIntvl;
    int iKeepAliveProbes;
    int iKeepAliveTime;
    int iConErrSkip;
    uchar *gnutlsPriorityString;
    permittedPeers_t *pPermPeers;
} configSettings_t;
static configSettings_t cs;

/* tables for interfacing with the v6 config system */
/* module-global parameters */
static struct cnfparamdescr modpdescr[] = {{"iobuffer.maxsize", eCmdHdlrNonNegInt, 0},
                                           {"template", eCmdHdlrGetWord, 0}};
static struct cnfparamblk modpblk = {CNFPARAMBLK_VERSION, sizeof(modpdescr) / sizeof(struct cnfparamdescr), modpdescr};

/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {
    {"target", eCmdHdlrArray, CNFPARAM_REQUIRED},
    {"address", eCmdHdlrGetWord, 0},
    {"device", eCmdHdlrGetWord, 0},
    {"port", eCmdHdlrArray, 0},
    {"protocol", eCmdHdlrGetWord, 0},
    {"networknamespace", eCmdHdlrGetWord, 0},
    {"tcp_framing", eCmdHdlrGetWord, 0},
    {"tcp_framedelimiter", eCmdHdlrInt, 0},
    {"ziplevel", eCmdHdlrInt, 0},
    {"compression.mode", eCmdHdlrGetWord, 0},
    {"compression.stream.flushontxend", eCmdHdlrBinary, 0},
    {"ipfreebind", eCmdHdlrInt, 0},
    {"maxerrormessages", eCmdHdlrInt, CNFPARAM_DEPRECATED},
    {"rebindinterval", eCmdHdlrInt, 0},
    {"keepalive", eCmdHdlrBinary, 0},
    {"keepalive.probes", eCmdHdlrNonNegInt, 0},
    {"keepalive.time", eCmdHdlrNonNegInt, 0},
    {"keepalive.interval", eCmdHdlrNonNegInt, 0},
    {"conerrskip", eCmdHdlrNonNegInt, 0},
    {"gnutlsprioritystring", eCmdHdlrString, 0},
    {"streamdriver", eCmdHdlrGetWord, 0},
    {"streamdriver.name", eCmdHdlrGetWord, 0}, /* alias for streamdriver */
    {"streamdrivermode", eCmdHdlrInt, 0},
    {"streamdriver.mode", eCmdHdlrInt, 0}, /* alias for streamdrivermode */
    {"streamdriverauthmode", eCmdHdlrGetWord, 0},
    {"streamdriver.authmode", eCmdHdlrGetWord, 0}, /* alias for streamdriverauthmode */
    {"streamdriverpermittedpeers", eCmdHdlrGetWord, 0},
    {"streamdriver.permitexpiredcerts", eCmdHdlrGetWord, 0},
    {"streamdriver.CheckExtendedKeyPurpose", eCmdHdlrBinary, 0},
    {"streamdriver.PrioritizeSAN", eCmdHdlrBinary, 0},
    {"streamdriver.TlsVerifyDepth", eCmdHdlrPositiveInt, 0},
    {"streamdriver.cafile", eCmdHdlrString, 0},
    {"streamdriver.crlfile", eCmdHdlrString, 0},
    {"streamdriver.keyfile", eCmdHdlrString, 0},
    {"streamdriver.certfile", eCmdHdlrString, 0},
    {"resendlastmsgonreconnect", eCmdHdlrBinary, 0},
    {"extendedconnectioncheck", eCmdHdlrBinary, 0},
    {"udp.sendtoall", eCmdHdlrBinary, 0},
    {"udp.senddelay", eCmdHdlrInt, 0},
    {"udp.sendbuf", eCmdHdlrSize, 0},
    {"template", eCmdHdlrGetWord, 0},
    {"pool.resumeinterval", eCmdHdlrPositiveInt, 0},
    {"ratelimit.interval", eCmdHdlrInt, 0},
    {"ratelimit.burst", eCmdHdlrInt, 0}};
static struct cnfparamblk actpblk = {CNFPARAMBLK_VERSION, sizeof(actpdescr) / sizeof(struct cnfparamdescr), actpdescr};

struct modConfData_s {
    rsconf_t *pConf; /* our overall config object */
    uchar *tplName; /* default template */
    int maxLenSndBuf; /* default max usable length of sendbuf - primarily for testing */
};

static modConfData_t *loadModConf = NULL; /* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL; /* modConf ptr to use for the current exec process */


static rsRetVal initTCP(wrkrInstanceData_t *pWrkrData);


BEGINinitConfVars /* (re)set config variables to default values */
    CODESTARTinitConfVars;
    cs.pszTplName = NULL; /* name of the default template to use */
    cs.pszStrmDrvr = NULL; /* name of the stream driver to use */
    cs.iStrmDrvrMode = 0; /* mode for stream driver, driver-dependent (0 mostly means plain tcp) */
    cs.bResendLastOnRecon = 0; /* should the last message be re-sent on a successful reconnect? */
    cs.pszStrmDrvrAuthMode = NULL; /* authentication mode to use */
    cs.iUDPRebindInterval = 0; /* support for automatic re-binding (load balancers!). 0 - no rebind */
    cs.iTCPRebindInterval = 0; /* support for automatic re-binding (load balancers!). 0 - no rebind */
    cs.pPermPeers = NULL;
ENDinitConfVars


static rsRetVal doTryResume(targetData_t *);
static rsRetVal doZipFinish(targetData_t *);

/* this function gets the default template. It coordinates action between
 * old-style and new-style configuration parts.
 */
static uchar *getDfltTpl(void) {
    if (loadModConf != NULL && loadModConf->tplName != NULL)
        return loadModConf->tplName;
    else if (cs.pszTplName == NULL)
        return (uchar *)"RSYSLOG_TraditionalForwardFormat";
    else
        return cs.pszTplName;
}


/* set the default template to be used
 * This is a module-global parameter, and as such needs special handling. It needs to
 * be coordinated with values set via the v2 config system (rsyslog v6+). What we do
 * is we do not permit this directive after the v2 config system has been used to set
 * the parameter.
 */
static rsRetVal setLegacyDfltTpl(void __attribute__((unused)) * pVal, uchar *newVal) {
    DEFiRet;

    if (loadModConf != NULL && loadModConf->tplName != NULL) {
        free(newVal);
        LogError(0, RS_RET_ERR,
                 "omfwd default template already set via module "
                 "global parameter - can no longer be changed");
        ABORT_FINALIZE(RS_RET_ERR);
    }
    free(cs.pszTplName);
    cs.pszTplName = newVal;
finalize_it:
    RETiRet;
}

/* Close the UDP sockets.
 * rgerhards, 2009-05-29
 */
static rsRetVal closeUDPSockets(wrkrInstanceData_t *pWrkrData) {
    DEFiRet;
    if (pWrkrData->target[0].pSockArray != NULL) {
        net.closeUDPListenSockets(pWrkrData->target[0].pSockArray);
        pWrkrData->target[0].pSockArray = NULL;
        freeaddrinfo(pWrkrData->target[0].f_addr);
        pWrkrData->target[0].f_addr = NULL;
    }
    pWrkrData->target[0].bIsConnected = 0;
    RETiRet;
}


static void DestructTCPTargetData(targetData_t *const pTarget) {
    // TODO: do we need to do a final send? if so, old bug!
    doZipFinish(pTarget);

    if (pTarget->pNetstrm != NULL) {
        netstrm.Destruct(&pTarget->pNetstrm);
    }
    if (pTarget->pNS != NULL) {
        netstrms.Destruct(&pTarget->pNS);
    }

    /* set resume time for internal retries */
    datetime.GetTime(&pTarget->ttResume);
    pTarget->ttResume += pTarget->pData->poolResumeInterval;
    pTarget->bIsConnected = 0;
    DBGPRINTF("omfwd: DestructTCPTargetData: %p %s:%s, connected %d, ttResume %lld\n", &pTarget, pTarget->target_name,
              pTarget->port, pTarget->bIsConnected, (long long)pTarget->ttResume);
}

/* destruct the TCP helper objects
 * This, for example, is needed after something went wrong.
 * This function is void because it "can not" fail.
 * rgerhards, 2008-06-04
 * Note that we DO NOT discard the current buffer contents
 * (if any). This permits us to save data between sessions. In
 * the worst case, some duplication occurs, but we do not
 * loose data.
 */
static void DestructTCPInstanceData(wrkrInstanceData_t *pWrkrData) {
    LogMsg(0, RS_RET_DEBUG, LOG_DEBUG, "omfwd: Destructing TCP target pool of %d targets (DestructTCPInstanceData)",
           pWrkrData->pData->nTargets);
    for (int j = 0; j < pWrkrData->pData->nTargets; ++j) {
        DestructTCPTargetData(&(pWrkrData->target[j]));
    }
}


BEGINbeginCnfLoad
    CODESTARTbeginCnfLoad;
    loadModConf = pModConf;
    pModConf->pConf = pConf;
    pModConf->tplName = NULL;
    pModConf->maxLenSndBuf = -1;
ENDbeginCnfLoad

BEGINsetModCnf
    int i;
    CODESTARTsetModCnf;
    const struct cnfparamvals *const __restrict__ pvals = nvlstGetParams(lst, &modpblk, NULL);
    if (pvals == NULL) {
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }

    if (Debug) {
        dbgprintf("module (global) param blk for omfwd:\n");
        cnfparamsPrint(&modpblk, pvals);
    }

    for (i = 0; i < modpblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;
        if (!strcmp(modpblk.descr[i].name, "template")) {
            loadModConf->tplName = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
            if (cs.pszTplName != NULL) {
                LogError(0, RS_RET_DUP_PARAM,
                         "omfwd: warning: default template "
                         "was already set via legacy directive - may lead to inconsistent "
                         "results.");
            }
        } else if (!strcmp(modpblk.descr[i].name, "iobuffer.maxsize")) {
            const int newLen = (int)pvals[i].val.d.n;
            if (newLen > SNDBUF_FIXED_BUFFER_SIZE) {
                LogMsg(0, RS_RET_PARAM_ERROR, LOG_WARNING,
                       "omfwd: module parameter \"iobuffer.maxsize\" specified larger "
                       "than actual buffer size (%d bytes) - ignored",
                       SNDBUF_FIXED_BUFFER_SIZE);
            } else {
                if (newLen > 0) {
                    loadModConf->maxLenSndBuf = newLen;
                }
            }
        } else {
            LogMsg(0, RS_RET_INTERNAL_ERROR, LOG_ERR, "omfwd: internal error, non-handled param '%s' in beginCnfLoad",
                   modpblk.descr[i].name);
        }
    }
finalize_it:
    if (pvals != NULL) cnfparamvalsDestruct(pvals, &modpblk);
ENDsetModCnf

BEGINendCnfLoad
    CODESTARTendCnfLoad;
    loadModConf = NULL; /* done loading */
    /* free legacy config vars */
    free(cs.pszTplName);
    cs.pszTplName = NULL;
ENDendCnfLoad

BEGINcheckCnf
    CODESTARTcheckCnf;
ENDcheckCnf

BEGINactivateCnf
    CODESTARTactivateCnf;
    runModConf = pModConf;
ENDactivateCnf

BEGINfreeCnf
    CODESTARTfreeCnf;
    free(pModConf->tplName);
ENDfreeCnf

BEGINcreateInstance
    CODESTARTcreateInstance;
    /* We always have at least one target and port */
    pData->nTargets = 1;
    pData->nActiveTargets = 0;
    INIT_ATOMIC_HELPER_MUT64(pData->mut_nActiveTargets);
    pData->nPorts = 1;
    pData->target_name = NULL;
    if (cs.pszStrmDrvr != NULL) CHKmalloc(pData->pszStrmDrvr = (uchar *)strdup((char *)cs.pszStrmDrvr));
    if (cs.pszStrmDrvrAuthMode != NULL)
        CHKmalloc(pData->pszStrmDrvrAuthMode = (uchar *)strdup((char *)cs.pszStrmDrvrAuthMode));
finalize_it:
ENDcreateInstance


/* Among others, all worker-specific targets are initialized here.
 */
BEGINcreateWrkrInstance
    CODESTARTcreateWrkrInstance;
    time_t ttNow;

    datetime.GetTime(&ttNow);
    ttNow--; /* make sure it is expired */

    assert(pData->nTargets > 0);
    pWrkrData->actualTarget = 0;
    pWrkrData->wrkrID = wrkrID++;
    CHKmalloc(pWrkrData->target = (targetData_t *)calloc(pData->nTargets, sizeof(targetData_t)));
    for (int i = 0; i < pData->nTargets; ++i) {
        pWrkrData->target[i].pData = pWrkrData->pData;
        pWrkrData->target[i].pWrkrData = pWrkrData;
        pWrkrData->target[i].target_name = pData->target_name[i];
        pWrkrData->target[i].pTargetStats = &(pData->target_stats[i]);
        /* if insufficient ports are configured, we use ports[0] for the
         * missing ports.
         */
        pWrkrData->target[i].port = pData->ports[(i < pData->nPorts) ? i : 0];
        pWrkrData->target[i].maxLenSndBuf =
            (runModConf->maxLenSndBuf == -1) ? SNDBUF_FIXED_BUFFER_SIZE : runModConf->maxLenSndBuf;
        pWrkrData->target[i].offsSndBuf = 0;
        pWrkrData->target[i].ttResume = ttNow;
    }
    iRet = initTCP(pWrkrData);
    LogMsg(0, RS_RET_DEBUG, LOG_DEBUG, "omfwd: worker with id %u initialized", pWrkrData->wrkrID);
finalize_it:
ENDcreateWrkrInstance


BEGINisCompatibleWithFeature
    CODESTARTisCompatibleWithFeature;
    if (eFeat == sFEATURERepeatedMsgReduction) iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINfreeInstance
    CODESTARTfreeInstance;
    free(pData->pszStrmDrvr);
    free(pData->pszStrmDrvrAuthMode);
    free(pData->pszStrmDrvrPermitExpiredCerts);
    free(pData->gnutlsPriorityString);
    free(pData->networkNamespace);
    if (pData->ports != NULL) { /* could happen in error case (very unlikely) */
        for (int j = 0; j < pData->nPorts; ++j) {
            free(pData->ports[j]);
        }
        free(pData->ports);
    }
    if (pData->target_stats != NULL) {
        for (int j = 0; j < pData->nTargets; ++j) {
            free(pData->target_name[j]);
            if (pData->target_stats[j].stats != NULL) statsobj.Destruct(&(pData->target_stats[j].stats));
        }
        free(pData->target_stats);
    }
    free(pData->target_name);
    free(pData->address);
    free(pData->device);
    free((void *)pData->pszStrmDrvrCAFile);
    free((void *)pData->pszStrmDrvrCRLFile);
    free((void *)pData->pszStrmDrvrKeyFile);
    free((void *)pData->pszStrmDrvrCertFile);
    net.DestructPermittedPeers(&pData->pPermPeers);
    if (pData->ratelimiter != NULL) {
        ratelimitDestruct(pData->ratelimiter);
        pData->ratelimiter = NULL;
    }
    DESTROY_ATOMIC_HELPER_MUT(pData->mut_nActiveTargets);

ENDfreeInstance


BEGINfreeWrkrInstance
    CODESTARTfreeWrkrInstance;
    LogMsg(0, RS_RET_DEBUG, LOG_DEBUG, "omfwd: [wrkr %u/%" PRIuPTR "] Destructing worker instance", pWrkrData->wrkrID,
           (uintptr_t)pthread_self());

    DestructTCPInstanceData(pWrkrData);
    closeUDPSockets(pWrkrData);

    if (pWrkrData->pData->protocol == FORW_TCP) {
        for (int i = 0; i < pWrkrData->pData->nTargets; ++i) {
            tcpclt.Destruct(&pWrkrData->target[i].pTCPClt);
        }
    }
    free(pWrkrData->target); /* note: this frees all target memory,calloc()ed array! */
ENDfreeWrkrInstance


BEGINdbgPrintInstInfo
    CODESTARTdbgPrintInstInfo;
    dbgprintf("omfwd\n");
ENDdbgPrintInstInfo


/* Send a message via UDP
 * rgehards, 2007-12-20
 */
#define UDP_MAX_MSGSIZE 65507 /* limit per RFC definition */
static rsRetVal UDPSend(wrkrInstanceData_t *__restrict__ const pWrkrData, uchar *__restrict__ const msg, size_t len) {
    DEFiRet;
    struct addrinfo *r;
    int i;
    ssize_t lsent = 0;
    sbool bSendSuccess;
    sbool reInit = RSFALSE;
    int lasterrno = ENOENT;
    int lasterr_sock = -1;
    targetData_t *const pTarget = &(pWrkrData->target[0]);
    targetStats_t *const pTargetStats = &(pWrkrData->pData->target_stats[0]);

    if (pWrkrData->pData->iRebindInterval && (pTarget->nXmit++ % pWrkrData->pData->iRebindInterval == 0)) {
        dbgprintf("omfwd dropping UDP 'connection' (as configured)\n");
        pTarget->nXmit = 1; /* else we have an addtl wrap at 2^31-1 */
        CHKiRet(closeUDPSockets(pWrkrData));
    }

    if (pWrkrData->target[0].pSockArray == NULL) {
        CHKiRet(doTryResume(pTarget)); /* for UDP, we have only a single tartget! */
    }

    if (pTarget->pSockArray == NULL) {
        FINALIZE;
    }


    if (len > UDP_MAX_MSGSIZE) {
        LogError(0, RS_RET_UDP_MSGSIZE_TOO_LARGE,
                 "omfwd/udp: message is %u "
                 "bytes long, but UDP can send at most %d bytes (by RFC limit) "
                 "- truncating message",
                 (unsigned)len, UDP_MAX_MSGSIZE);
        len = UDP_MAX_MSGSIZE;
    }

    /* we need to track if we have success sending to the remote
     * peer. Success is indicated by at least one sendto() call
     * succeeding. We track this be bSendSuccess. We can not simply
     * rely on lsent, as a call might initially work, but a later
     * call fails. Then, lsent has the error status, even though
     * the sendto() succeeded. -- rgerhards, 2007-06-22
     */
    bSendSuccess = RSFALSE;
    for (r = pTarget->f_addr; r; r = r->ai_next) {
        int runSockArrayLoop = 1;
        for (i = 0; runSockArrayLoop && (i < *pTarget->pSockArray); i++) {
            int try_send = 1;
            size_t lenThisTry = len;
            while (try_send) {
                lsent = sendto(pTarget->pSockArray[i + 1], msg, lenThisTry, 0, r->ai_addr, r->ai_addrlen);
                if (lsent == (ssize_t)lenThisTry) {
                    bSendSuccess = RSTRUE;
                    ATOMIC_ADD_uint64(&pTargetStats->sentBytes, &pTargetStats->mut_sentBytes, lenThisTry);
                    try_send = 0;
                    runSockArrayLoop = 0;
                } else if (errno == EMSGSIZE) {
                    const size_t newlen = (lenThisTry > 1024) ? lenThisTry - 1024 : 512;
                    LogError(0, RS_RET_UDP_MSGSIZE_TOO_LARGE,
                             "omfwd/udp: send failed due to message being too "
                             "large for this system. Message size was %u bytes. "
                             "Truncating to %u bytes and retrying.",
                             (unsigned)lenThisTry, (unsigned)newlen);
                    lenThisTry = newlen;
                } else {
                    reInit = RSTRUE;
                    lasterrno = errno;
                    lasterr_sock = pTarget->pSockArray[i + 1];
                    LogError(lasterrno, RS_RET_ERR_UDPSEND, "omfwd/udp: socket %d: sendto() error", lasterr_sock);
                    try_send = 0;
                }
            }
        }
        if (lsent == (ssize_t)len && !pWrkrData->pData->bSendToAll) break;
    }

    /* one or more send failures; close sockets and re-init */
    if (reInit == RSTRUE) {
        CHKiRet(closeUDPSockets(pWrkrData));
    }

    /* finished looping */
    if (bSendSuccess == RSTRUE) {
        if (pWrkrData->pData->iUDPSendDelay > 0) {
            srSleep(pWrkrData->pData->iUDPSendDelay / 1000000, pWrkrData->pData->iUDPSendDelay % 1000000);
        }
    } else {
        LogError(lasterrno, RS_RET_ERR_UDPSEND, "omfwd: socket %d: error %d sending via udp", lasterr_sock, lasterrno);
        iRet = RS_RET_SUSPENDED;
    }

finalize_it:
    RETiRet;
}


/* set the permitted peers -- rgerhards, 2008-05-19
 */
static rsRetVal setPermittedPeer(void __attribute__((unused)) * pVal, uchar *pszID) {
    DEFiRet;
    CHKiRet(net.AddPermittedPeer(&cs.pPermPeers, pszID));
    free(pszID); /* no longer needed, but we must free it as of interface def */
finalize_it:
    RETiRet;
}


/* CODE FOR SENDING TCP MESSAGES */

/* This is a common function so that we can emit consistent error
 * messages whenever we have trouble with a connection, e.g. when
 * sending or checking if it's broken.
 */
static void emitConnectionErrorMsg(const targetData_t *const pTarget, const rsRetVal iRet) {
    wrkrInstanceData_t *pWrkrData = (wrkrInstanceData_t *)pTarget->pWrkrData;

    if (iRet == RS_RET_IO_ERROR || iRet == RS_RET_PEER_CLOSED_CONN) {
        static unsigned int conErrCnt = 0;
        const int skipFactor = pWrkrData->pData->iConErrSkip;

        const char *actualErrMsg;
        if (iRet == RS_RET_PEER_CLOSED_CONN) {
            actualErrMsg = "remote server closed connection. ";
        } else {
            actualErrMsg =
                "we had a generic or IO error with the remote server. "
                "The actual error message should already have been provided. ";
        }
        if (skipFactor <= 1) {
            /* All the connection errors are printed. */
            LogError(0, iRet,
                     "omfwd: [wrkr %u/%" PRIuPTR
                     "] %s Server is %s:%s. "
                     "This can be caused by the remote server or an interim system like a load "
                     "balancer or firewall. Rsyslog will re-open the connection if configured "
                     "to do so.",
                     pTarget->pWrkrData->wrkrID, (uintptr_t)pthread_self(), actualErrMsg, pTarget->target_name,
                     pTarget->port);
        } else if ((conErrCnt++ % skipFactor) == 0) {
            /* Every N'th error message is printed where N is a skipFactor. */
            LogError(0, iRet,
                     "omfwd: [wrkr %u] %s Server is %s:%s. "
                     "This can be caused by the remote server or an interim system like a load "
                     "balancer or firewall. Rsyslog will re-open the connection if configured "
                     "to do so. Note that the next %d connection error messages will be "
                     "skipped.",
                     pTarget->pWrkrData->wrkrID, actualErrMsg, pTarget->target_name, pTarget->port, skipFactor - 1);
        }
    } else {
        LogError(0, iRet, "omfwd: TCPSendBuf error %d, destruct TCP Connection to %s:%s", iRet, pTarget->target_name,
                 pTarget->port);
    }
}


/* hack to check connections for plain tcp syslog - see ptcp driver for details */
static rsRetVal CheckConnection(targetData_t *const pTarget) {
    DEFiRet;

    if ((pTarget->pData->protocol == FORW_TCP) && (pTarget->pData->bExtendedConnCheck)) {
        CHKiRet(netstrm.CheckConnection(pTarget->pNetstrm));
    }

finalize_it:
    if (iRet != RS_RET_OK) {
        emitConnectionErrorMsg(pTarget, iRet);
        DestructTCPTargetData(pTarget);
        iRet = RS_RET_SUSPENDED;
    }
    RETiRet;
}


static rsRetVal TCPSendBufUncompressed(targetData_t *const pTarget, uchar *const buf, const unsigned len) {
    DEFiRet;
    unsigned alreadySent;
    ssize_t lenSend;

    alreadySent = 0;
    if (pTarget->pData->bExtendedConnCheck) {
        CHKiRet(netstrm.CheckConnection(pTarget->pNetstrm));
        /* hack for plain tcp syslog - see ptcp driver for details */
    }

    while (alreadySent != len) {
        lenSend = len - alreadySent;
        CHKiRet(netstrm.Send(pTarget->pNetstrm, buf + alreadySent, &lenSend));
        DBGPRINTF("omfwd: TCP sent %zd bytes, requested %u\n", lenSend, len - alreadySent);
        alreadySent += lenSend;
    }

    ATOMIC_ADD_uint64(&pTarget->pTargetStats->sentBytes, &pTarget->pTargetStats->mut_sentBytes, len);

finalize_it:
    if (iRet != RS_RET_OK) {
        emitConnectionErrorMsg(pTarget, iRet);
        DestructTCPTargetData(pTarget);
        iRet = RS_RET_SUSPENDED;
    }
    RETiRet;
}

static rsRetVal TCPSendBufCompressed(targetData_t *pTarget, uchar *const buf, unsigned len, sbool bIsFlush) {
    wrkrInstanceData_t *const pWrkrData = (wrkrInstanceData_t *)pTarget->pWrkrData;
    int zRet; /* zlib return state */
    unsigned outavail;
    uchar zipBuf[32 * 1024];
    int op;
    DEFiRet;

    if (!pTarget->bzInitDone) {
        /* allocate deflate state */
        pTarget->zstrm.zalloc = Z_NULL;
        pTarget->zstrm.zfree = Z_NULL;
        pTarget->zstrm.opaque = Z_NULL;
        /* see note in file header for the params we use with deflateInit2() */
        zRet = deflateInit(&pTarget->zstrm, pWrkrData->pData->compressionLevel);
        if (zRet != Z_OK) {
            DBGPRINTF("error %d returned from zlib/deflateInit()\n", zRet);
            ABORT_FINALIZE(RS_RET_ZLIB_ERR);
        }
        pTarget->bzInitDone = RSTRUE;
    }

    /* now doing the compression */
    pTarget->zstrm.next_in = (Bytef *)buf;
    pTarget->zstrm.avail_in = len;
    if (pWrkrData->pData->strmCompFlushOnTxEnd && bIsFlush)
        op = Z_SYNC_FLUSH;
    else
        op = Z_NO_FLUSH;
    /* run deflate() on buffer until everything has been compressed */
    do {
        DBGPRINTF("omfwd: in deflate() loop, avail_in %d, total_in %ld, isFlush %d\n", pTarget->zstrm.avail_in,
                  pTarget->zstrm.total_in, bIsFlush);
        pTarget->zstrm.avail_out = sizeof(zipBuf);
        pTarget->zstrm.next_out = zipBuf;
        zRet = deflate(&pTarget->zstrm, op); /* no bad return value */
        DBGPRINTF("after deflate, ret %d, avail_out %d\n", zRet, pTarget->zstrm.avail_out);
        outavail = sizeof(zipBuf) - pTarget->zstrm.avail_out;
        if (outavail != 0) {
            CHKiRet(TCPSendBufUncompressed(pTarget, zipBuf, outavail));
        }
    } while (pTarget->zstrm.avail_out == 0);

finalize_it:
    RETiRet;
}

static rsRetVal TCPSendBuf(targetData_t *pTarget, uchar *buf, unsigned len, sbool bIsFlush) {
    wrkrInstanceData_t *pWrkrData = (wrkrInstanceData_t *)pTarget->pWrkrData;
    DEFiRet;
    if (pWrkrData->pData->compressionMode >= COMPRESS_STREAM_ALWAYS)
        iRet = TCPSendBufCompressed(pTarget, buf, len, bIsFlush);
    else
        iRet = TCPSendBufUncompressed(pTarget, buf, len);
    RETiRet;
}

/* finish zlib buffer, to be called before closing the ZIP file (if
 * running in stream mode).
 */
static rsRetVal doZipFinish(targetData_t *pTarget) {
    int zRet; /* zlib return state */
    DEFiRet;
    unsigned outavail;
    uchar zipBuf[32 * 1024];

    if (!pTarget->bzInitDone) goto done;

    // TODO: can we get this into a single common function?
    pTarget->zstrm.avail_in = 0;
    /* run deflate() on buffer until everything has been compressed */
    do {
        DBGPRINTF("in deflate() loop, avail_in %d, total_in %ld\n", pTarget->zstrm.avail_in, pTarget->zstrm.total_in);
        pTarget->zstrm.avail_out = sizeof(zipBuf);
        pTarget->zstrm.next_out = zipBuf;
        zRet = deflate(&pTarget->zstrm, Z_FINISH); /* no bad return value */
        DBGPRINTF("after deflate, ret %d, avail_out %d\n", zRet, pTarget->zstrm.avail_out);
        outavail = sizeof(zipBuf) - pTarget->zstrm.avail_out;
        if (outavail != 0) {
            CHKiRet(TCPSendBufUncompressed(pTarget, zipBuf, outavail));
        }
    } while (pTarget->zstrm.avail_out == 0);

finalize_it:
    zRet = deflateEnd(&pTarget->zstrm);
    if (zRet != Z_OK) {
        DBGPRINTF("error %d returned from zlib/deflateEnd()\n", zRet);
    }

    pTarget->bzInitDone = 0;
done:
    RETiRet;
}


/* Add frame to send buffer (or send, if required)
 */
static rsRetVal TCPSendFrame(void *pvData, char *msg, const size_t len) {
    DEFiRet;
    targetData_t *pTarget = (targetData_t *)pvData;

    DBGPRINTF("omfwd: add %zu bytes to send buffer (curr offs %u, max len %d) msg: %*s\n", len, pTarget->offsSndBuf,
              pTarget->maxLenSndBuf, (int)len, msg);
    if (pTarget->offsSndBuf != 0 && (pTarget->offsSndBuf + len) >= (size_t)pTarget->maxLenSndBuf) {
        /* no buffer space left, need to commit previous records. With the
         * current API, there unfortunately is no way to signal this
         * state transition to the upper layer.
         */
        DBGPRINTF(
            "omfwd: we need to do a tcp send due to buffer "
            "out of space. If the transaction fails, this will "
            "lead to duplication of messages");
        CHKiRet(TCPSendBuf(pTarget, pTarget->sndBuf, pTarget->offsSndBuf, NO_FLUSH));
        pTarget->offsSndBuf = 0;
    }

    /* check if the message is too large to fit into buffer */
    if (len > sizeof(pTarget->sndBuf)) {
        CHKiRet(TCPSendBuf(pTarget, (uchar *)msg, len, NO_FLUSH));
        ABORT_FINALIZE(RS_RET_OK); /* committed everything so far */
    }

    /* we now know the buffer has enough free space */
    memcpy(pTarget->sndBuf + pTarget->offsSndBuf, msg, len);
    pTarget->offsSndBuf += len;
    iRet = RS_RET_DEFER_COMMIT;

finalize_it:
    RETiRet;
}


/* initializes a TCP session to a single Target
 */
static rsRetVal TCPSendInitTarget(targetData_t *const pTarget) {
    DEFiRet;
    wrkrInstanceData_t *const pWrkrData = (wrkrInstanceData_t *)pTarget->pWrkrData;
    instanceData *pData = pWrkrData->pData;


    // TODO-RG: check error case - we need to make sure that we handle the situation correctly
    //	    when SOME calls fails - else we may get into big trouble during de-init
    if (pTarget->pNetstrm == NULL) {
        dbgprintf("TCPSendInitTarget CREATE %s:%s, conn %d\n", pTarget->target_name, pTarget->port,
                  pTarget->bIsConnected);
        CHKiRet(netstrms.Construct(&pTarget->pNS));
        /* the stream driver must be set before the object is finalized! */
        CHKiRet(netstrms.SetDrvrName(pTarget->pNS, pData->pszStrmDrvr));
        CHKiRet(netstrms.ConstructFinalize(pTarget->pNS));

        /* now create the actual stream and connect to the server */
        CHKiRet(netstrms.CreateStrm(pTarget->pNS, &pTarget->pNetstrm));
        CHKiRet(netstrm.ConstructFinalize(pTarget->pNetstrm));
        CHKiRet(netstrm.SetDrvrMode(pTarget->pNetstrm, pData->iStrmDrvrMode));
        CHKiRet(netstrm.SetDrvrCheckExtendedKeyUsage(pTarget->pNetstrm, pData->iStrmDrvrExtendedCertCheck));
        CHKiRet(netstrm.SetDrvrPrioritizeSAN(pTarget->pNetstrm, pData->iStrmDrvrSANPreference));
        CHKiRet(netstrm.SetDrvrTlsVerifyDepth(pTarget->pNetstrm, pData->iStrmTlsVerifyDepth));
        /* now set optional params, but only if they were actually configured */
        if (pData->pszStrmDrvrAuthMode != NULL) {
            CHKiRet(netstrm.SetDrvrAuthMode(pTarget->pNetstrm, pData->pszStrmDrvrAuthMode));
        }
        /* Call SetDrvrPermitExpiredCerts required
         * when param is NULL default handling for ExpiredCerts is set! */
        CHKiRet(netstrm.SetDrvrPermitExpiredCerts(pTarget->pNetstrm, pData->pszStrmDrvrPermitExpiredCerts));
        CHKiRet(netstrm.SetDrvrTlsCAFile(pTarget->pNetstrm, pData->pszStrmDrvrCAFile));
        CHKiRet(netstrm.SetDrvrTlsCRLFile(pTarget->pNetstrm, pData->pszStrmDrvrCRLFile));
        CHKiRet(netstrm.SetDrvrTlsKeyFile(pTarget->pNetstrm, pData->pszStrmDrvrKeyFile));
        CHKiRet(netstrm.SetDrvrTlsCertFile(pTarget->pNetstrm, pData->pszStrmDrvrCertFile));

        if (pData->pPermPeers != NULL) {
            CHKiRet(netstrm.SetDrvrPermPeers(pTarget->pNetstrm, pData->pPermPeers));
        }
        /* params set, now connect */
        if (pData->gnutlsPriorityString != NULL) {
            CHKiRet(netstrm.SetGnutlsPriorityString(pTarget->pNetstrm, pData->gnutlsPriorityString));
        }
        CHKiRet(netstrm.Connect(pTarget->pNetstrm, glbl.GetDefPFFamily(runModConf->pConf), (uchar *)pTarget->port,
                                (uchar *)pTarget->target_name, pData->device));

        /* set keep-alive if enabled */
        if (pData->bKeepAlive) {
            CHKiRet(netstrm.SetKeepAliveProbes(pTarget->pNetstrm, pData->iKeepAliveProbes));
            CHKiRet(netstrm.SetKeepAliveIntvl(pTarget->pNetstrm, pData->iKeepAliveIntvl));
            CHKiRet(netstrm.SetKeepAliveTime(pTarget->pNetstrm, pData->iKeepAliveTime));
            CHKiRet(netstrm.EnableKeepAlive(pTarget->pNetstrm));
        }

        LogMsg(0, RS_RET_DEBUG, LOG_DEBUG, "omfwd: [wrkr %u] TCPSendInitTarget established connection to %s:%s",
               pTarget->pWrkrData->wrkrID, pTarget->target_name, pTarget->port);
    }

finalize_it:
    if (iRet != RS_RET_OK) {
        dbgprintf("TCPSendInitTarget FAILED with %d.\n", iRet);
        DestructTCPTargetData(pTarget);
    }

    RETiRet;
}


/* Callback to initialize the provided target.
 * rgerhards, 2007-12-28
 */
static rsRetVal TCPSendInit(void *pvData) {
    return TCPSendInitTarget((targetData_t *)pvData);
}

/* This callback function is called immediately before a send retry is attempted.
 * It shall clean up whatever makes sense.
 * side-note: TCPSendInit() is called afterwards by the generic tcp client code.
 */
static rsRetVal TCPSendPrepRetry(void *pvData) {
    DestructTCPTargetData((targetData_t *)pvData);
    /* Even if the destruct fails, it does not help to provide this info to
     * the upper layer. Also, DestructTCPTargtData() does currently not
     * provide a return status.
     */
    return RS_RET_OK;
}


/* change to network namespace pData->networkNamespace and keep the file
 * descriptor to the original namespace.
 */
static rsRetVal changeToNs(instanceData *const pData __attribute__((unused))) {
    DEFiRet;
#ifdef HAVE_SETNS
    int iErr;
    int destinationNs = -1;
    char *nsPath = NULL;

    if (pData->networkNamespace) {
        /* keep file descriptor of original network namespace */
        pData->originalNamespace = open("/proc/self/ns/net", O_RDONLY);
        if (pData->originalNamespace < 0) {
            LogError(0, RS_RET_IO_ERROR, "omfwd: could not read /proc/self/ns/net");
            ABORT_FINALIZE(RS_RET_IO_ERROR);
        }

        /* build network namespace path */
        if (asprintf(&nsPath, "/var/run/netns/%s", pData->networkNamespace) == -1) {
            LogError(0, RS_RET_OUT_OF_MEMORY, "omfwd: asprintf failed");
            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        }

        /* keep file descriptor of destination network namespace */
        destinationNs = open(nsPath, 0);
        if (destinationNs < 0) {
            LogError(0, RS_RET_IO_ERROR, "omfwd: could not change to namespace '%s'", pData->networkNamespace);
            ABORT_FINALIZE(RS_RET_IO_ERROR);
        }

        /* actually change in the destination network namespace */
        if ((iErr = (setns(destinationNs, CLONE_NEWNET))) != 0) {
            LogError(0, RS_RET_IO_ERROR, "could not change to namespace '%s': %s", pData->networkNamespace,
                     gai_strerror(iErr));
            ABORT_FINALIZE(RS_RET_IO_ERROR);
        }
        dbgprintf("omfwd: changed to network namespace '%s'\n", pData->networkNamespace);
    }

finalize_it:
    free(nsPath);
    if (destinationNs >= 0) {
        close(destinationNs);
    }
#else /* #ifdef HAVE_SETNS */
    dbgprintf("omfwd: OS does not support network namespaces\n");
#endif /* #ifdef HAVE_SETNS */
    RETiRet;
}


/* return to the original network namespace. This should be called after
 * changeToNs().
 */
static rsRetVal returnToOriginalNs(instanceData *const pData __attribute__((unused))) {
    DEFiRet;
#ifdef HAVE_SETNS
    int iErr;

    /* only in case a network namespace is given and a file descriptor to
     * the original namespace exists */
    if (pData->networkNamespace && pData->originalNamespace >= 0) {
        /* actually change to the original network namespace */
        if ((iErr = (setns(pData->originalNamespace, CLONE_NEWNET))) != 0) {
            LogError(0, RS_RET_IO_ERROR, "could not return to original namespace: %s", gai_strerror(iErr));
            ABORT_FINALIZE(RS_RET_IO_ERROR);
        }

        close(pData->originalNamespace);
        dbgprintf("omfwd: returned to original network namespace\n");
    }

finalize_it:
#endif /* #ifdef HAVE_SETNS */
    RETiRet;
}


/* count the actual number of active targets.
 */
static void countActiveTargets(const wrkrInstanceData_t *const pWrkrData) {
    int activeTargets = 0;
    for (int j = 0; j < pWrkrData->pData->nTargets; ++j) {
        if (pWrkrData->target[j].bIsConnected) {
            activeTargets++;
        }
    }

    /* Use atomic CAS to update the value safely */
    int oldVal, newVal;
    do {
        oldVal = ATOMIC_FETCH_32BIT(&pWrkrData->pData->nActiveTargets, &pWrkrData->pData->mut_nActiveTargets);
        if (oldVal == activeTargets) {
            break; /* no change, so no log message either */
        }
        newVal = activeTargets;
    } while (!ATOMIC_CAS(&pWrkrData->pData->nActiveTargets, oldVal, newVal, &pWrkrData->pData->mut_nActiveTargets));

    if (oldVal != activeTargets) {
        LogMsg(0, RS_RET_DEBUG, LOG_DEBUG, "omfwd: [wrkr %u] number of active targets changed from %d to %d",
               pWrkrData->wrkrID, oldVal, activeTargets);
    }
}


/* check if the action as while is working (one target is OK) or suspended.
 * Try to resume initially not working targets along the way.
 */
static rsRetVal poolTryResume(wrkrInstanceData_t *const pWrkrData) {
    DEFiRet;
    int oneTargetOK = 0;
    for (int j = 0; j < pWrkrData->pData->nTargets; ++j) {
        if (pWrkrData->target[j].bIsConnected) {
            if (CheckConnection(&(pWrkrData->target[j])) == RS_RET_OK) {
                oneTargetOK = 1;
            }
        } else {
            DBGPRINTF("omfwd: poolTryResume, calling tryResume, target %d\n", j);
            iRet = doTryResume(&(pWrkrData->target[j]));
            DBGPRINTF("omfwd: poolTryResume, done	 tryResume, target %d, iRet %d\n", j, iRet);
            if (iRet == RS_RET_OK) {
                oneTargetOK = 1;
            }
        }
    }
    if (oneTargetOK) {
        iRet = RS_RET_OK;
    }
    DBGPRINTF("poolTryResume: oneTargetOK %d, iRet %d\n", oneTargetOK, iRet);
    RETiRet;
}

/* try to resume connection if it is not ready
 * When done, we set the time of earliest resumption. This is to handle
 * suspend and resume within the target connection pool.
 * rgerhards, 2007-08-02
 */
static rsRetVal doTryResume(targetData_t *pTarget) {
    wrkrInstanceData_t *const pWrkrData = (wrkrInstanceData_t *)pTarget->pWrkrData;
    instanceData *const pData = pWrkrData->pData;
    ;
    int iErr;
    struct addrinfo *res = NULL;
    struct addrinfo hints;
    int bBindRequired = 0;
    int bNeedReturnNs = 0;
    const char *address;
    DEFiRet;

    const int nActiveTargets = ATOMIC_FETCH_32BIT(&pTarget->pData->nActiveTargets, &pTarget->pData->mut_nActiveTargets);

    DBGPRINTF("doTryResume: isConnected: %d, ttResume %lld, LastActiveTargets: %d\n", pTarget->bIsConnected,
              (long long)pTarget->ttResume, nActiveTargets);

    if (pTarget->bIsConnected) FINALIZE;

    /* we look at the resume counter only if we have active targets at all - otherwise
     * rsyslog core handles the retry timing.
     */
    if (pTarget->ttResume > 0 && nActiveTargets > 0) {
        time_t ttNow;
        datetime.GetTime(&ttNow);
        if (ttNow < pTarget->ttResume) {
            DBGPRINTF("omfwd: doTryResume: %s not yet time to retry, time %lld, ttResume %lld\n", pTarget->target_name,
                      (long long)ttNow, (long long)pTarget->ttResume);
            ABORT_FINALIZE(RS_RET_SUSPENDED);
        }
    }

    /* The remote address is not yet known and needs to be obtained */
    if (pData->protocol == FORW_UDP) {
        memset(&hints, 0, sizeof(hints));
        /* port must be numeric, because config file syntax requires this */
        hints.ai_flags = AI_NUMERICSERV;
        hints.ai_family = glbl.GetDefPFFamily(runModConf->pConf);
        hints.ai_socktype = SOCK_DGRAM;
        if ((iErr = (getaddrinfo(pTarget->target_name, pTarget->port, &hints, &res))) != 0) {
            LogError(0, RS_RET_SUSPENDED, "omfwd: could not get addrinfo for hostname '%s':'%s': %s",
                     pTarget->target_name, pTarget->port, gai_strerror(iErr));
            ABORT_FINALIZE(RS_RET_SUSPENDED);
        }
        address = pTarget->target_name;
        if (pData->address) {
            struct addrinfo *addr;
            /* The AF of the bind addr must match that of target */
            hints.ai_family = res->ai_family;
            hints.ai_flags |= AI_PASSIVE;
            iErr = getaddrinfo(pData->address, pTarget->port, &hints, &addr);
            if (iErr != 0) {
                LogError(0, RS_RET_SUSPENDED, "omfwd: cannot use bind address '%s' for host '%s': %s", pData->address,
                         pTarget->target_name, gai_strerror(iErr));
                ABORT_FINALIZE(RS_RET_SUSPENDED);
            }
            freeaddrinfo(addr);
            bBindRequired = 1;
            address = pData->address;
        }
        DBGPRINTF("%s found, resuming.\n", pTarget->target_name);
        pTarget->f_addr = res;
        res = NULL;
        if (pTarget->pSockArray == NULL) {
            CHKiRet(changeToNs(pData));
            bNeedReturnNs = 1;
            pTarget->pSockArray = net.create_udp_socket((uchar *)address, NULL, bBindRequired, 0, pData->UDPSendBuf,
                                                        pData->ipfreebind, pData->device);
            CHKiRet(returnToOriginalNs(pData));
            bNeedReturnNs = 0;
        }
        if (pTarget->pSockArray != NULL) {
            pTarget->bIsConnected = 1;
        }
    } else {
        CHKiRet(changeToNs(pData));
        bNeedReturnNs = 1;
        CHKiRet(TCPSendInitTarget((void *)pTarget));
        CHKiRet(returnToOriginalNs(pData));
        bNeedReturnNs = 0;
        pTarget->bIsConnected = 1;
    }

finalize_it:
    if (res != NULL) {
        freeaddrinfo(res);
    }
    if (iRet != RS_RET_OK) {
        if (bNeedReturnNs) {
            returnToOriginalNs(pData);
        }
        if (pTarget->f_addr != NULL) {
            freeaddrinfo(pTarget->f_addr);
            pTarget->f_addr = NULL;
        }
        iRet = RS_RET_SUSPENDED;
    }

    RETiRet;
}


BEGINtryResume
    CODESTARTtryResume;
    iRet = poolTryResume(pWrkrData);
    countActiveTargets(pWrkrData);
    const int nActiveTargets =
        ATOMIC_FETCH_32BIT(&pWrkrData->pData->nActiveTargets, &pWrkrData->pData->mut_nActiveTargets);

    LogMsg(0, RS_RET_DEBUG, LOG_DEBUG,
           "omfwd: [wrkr %u/%" PRIuPTR
           "] tryResume was called by rsyslog core: "
           "active targets: %d, overall return state %d",
           pWrkrData->wrkrID, (uintptr_t)pthread_self(), nActiveTargets, iRet);
ENDtryResume


BEGINbeginTransaction
    CODESTARTbeginTransaction;
    dbgprintf("omfwd: beginTransaction\n");
    /* note: we need to try resume so that we are at least at the start
     * of the transaction aware of target states. It is not useful to
     * start a transaction when we know it will most probably fail.
     */
    iRet = poolTryResume(pWrkrData);

    if (iRet == RS_RET_SUSPENDED) {
        LogMsg(0, RS_RET_SUSPENDED, LOG_WARNING,
               "omfwd: [wrkr %d/%" PRIuPTR
               "] no working target servers in "
               "pool available, suspending action (state: beginTx)",
               pWrkrData->wrkrID, (uintptr_t)pthread_self());
    }
ENDbeginTransaction


static rsRetVal processMsg(targetData_t *__restrict__ const pTarget, actWrkrIParams_t *__restrict__ const iparam) {
    wrkrInstanceData_t *const pWrkrData = (wrkrInstanceData_t *)pTarget->pWrkrData;
    uchar *psz; /* temporary buffering */
    register unsigned l;
    int iMaxLine;
    Bytef *out = NULL; /* for compression */
    instanceData *__restrict__ const pData = pWrkrData->pData;
    DEFiRet;

    iMaxLine = glbl.GetMaxLine(runModConf->pConf);

    psz = iparam->param;
    l = iparam->lenStr;
    if ((int)l > iMaxLine) l = iMaxLine;

    /* Check if we should compress and, if so, do it. We also
     * check if the message is large enough to justify compression.
     * The smaller the message, the less likely is a gain in compression.
     * To save CPU cycles, we do not try to compress very small messages.
     * What "very small" means needs to be configured. Currently, it is
     * hard-coded but this may be changed to a config parameter.
     * rgerhards, 2006-11-30
     */
    if (pData->compressionMode == COMPRESS_SINGLE_MSG && (l > CONF_MIN_SIZE_FOR_COMPRESS)) {
        uLongf destLen = iMaxLine + iMaxLine / 100 + 12; /* recommended value from zlib doc */
        uLong srcLen = l;
        int ret;
        CHKmalloc(out = (Bytef *)malloc(destLen));
        out[0] = 'z';
        out[1] = '\0';
        ret = compress2((Bytef *)out + 1, &destLen, (Bytef *)psz, srcLen, pData->compressionLevel);
        dbgprintf("Compressing message, length was %d now %d, return state  %d.\n", l, (int)destLen, ret);
        if (ret != Z_OK) {
            /* if we fail, we complain, but only in debug mode
             * Otherwise, we are silent. In any case, we ignore the
             * failed compression and just sent the uncompressed
             * data, which is still valid. So this is probably the
             * best course of action.
             * rgerhards, 2006-11-30
             */
            dbgprintf("Compression failed, sending uncompressed message\n");
        } else if (destLen + 1 < l) {
            /* only use compression if there is a gain in using it! */
            dbgprintf("there is gain in compression, so we do it\n");
            psz = out;
            l = destLen + 1; /* take care for the "z" at message start! */
        }
        ++destLen;
    }

    if (pData->protocol == FORW_UDP) {
        /* forward via UDP */
        CHKiRet(UDPSend(pWrkrData, psz, l));  // TODO-RG: always add "actualTarget"!
    } else {
        /* forward via TCP */
        iRet = tcpclt.Send(pTarget->pTCPClt, pTarget, (char *)psz, l);
        if (iRet != RS_RET_OK && iRet != RS_RET_DEFER_COMMIT && iRet != RS_RET_PREVIOUS_COMMITTED) {
            /* error! */
            LogError(0, iRet, "omfwd: error forwarding via tcp to %s:%s, suspending target", pTarget->target_name,
                     pTarget->port);
            DestructTCPTargetData(pTarget);
            iRet = RS_RET_SUSPENDED;
        }
    }

finalize_it:
    if (iRet == RS_RET_OK || iRet == RS_RET_DEFER_COMMIT || iRet == RS_RET_PREVIOUS_COMMITTED) {
        ATOMIC_INC_uint64(&pTarget->pTargetStats->sentMsgs, &pTarget->pTargetStats->mut_sentMsgs);
    }

    free(out); /* is NULL if it was never used... */
    RETiRet;
}

BEGINcommitTransaction
    unsigned i;
    char namebuf[264]; /* 256 for FQDN, 5 for port and 3 for transport => 264 */
    CODESTARTcommitTransaction;
    /* if needed, rebind first. This ensure we can deliver to the rebound addresses.
     * Note that rebind requires reconnect to the new targets. This is done by the
     * poolTryResume(), which needs to be made in any case.
     */
    if (pWrkrData->pData->iRebindInterval && (pWrkrData->nXmit++ >= pWrkrData->pData->iRebindInterval)) {
        dbgprintf("REBIND (sent %d, interval %d) - omfwd dropping target connection (as configured)\n",
                  pWrkrData->nXmit, pWrkrData->pData->iRebindInterval);
        pWrkrData->nXmit = 0; /* else we have an addtl wrap at 2^31-1 */
        DestructTCPInstanceData(pWrkrData);
        initTCP(pWrkrData);
        LogMsg(0, RS_RET_PARAM_ERROR, LOG_WARNING, "omfwd: dropped connections due to configured rebind interval");
    }

    CHKiRet(poolTryResume(pWrkrData));
    countActiveTargets(pWrkrData);

    DBGPRINTF("pool %s:%s/%s\n", pWrkrData->pData->target_name[0], pWrkrData->pData->ports[0],
              pWrkrData->pData->protocol == FORW_UDP ? "udp" : "tcp");

    /* we use a rate-limiter based on the first pool members name. This looks
     * good enough. As an alternative, we may consider creating a special pool
     * name. But we leave this for when need actually arises.
     */
    if (pWrkrData->pData->ratelimiter) {
        snprintf(namebuf, sizeof namebuf, "%s:[%s]:%s", pWrkrData->pData->protocol == FORW_UDP ? "udp" : "tcp",
                 pWrkrData->pData->target_name[0], pWrkrData->pData->ports[0]);
    }

    for (i = 0; i < nParams; ++i) {
        /* If rate limiting is enabled, check whether this message has to be discarded */
        if (pWrkrData->pData->ratelimiter) {
            iRet = ratelimitMsgCount(pWrkrData->pData->ratelimiter, 0, namebuf);
            if (iRet == RS_RET_DISCARDMSG) {
                iRet = RS_RET_OK;
                continue;
            } else if (iRet != RS_RET_OK) {
                LogError(0, RS_RET_ERR, "omfwd: error during rate limit : %d.\n", iRet);
            }
        }

        int trynbr = 0;
        int dotry = 1;
        while (dotry && trynbr < pWrkrData->pData->nTargets) {
            /* In the future we may consider if we would like to have targets on
               a per-worker or global basis. We now use worker because otherwise we
               have thread interdependence, which hurts performance. But this
               can lead to uneven distribution of messages when multiple workers run.
             */
            const unsigned actualTarget = (pWrkrData->actualTarget++) % pWrkrData->pData->nTargets;
            targetData_t *pTarget = &(pWrkrData->target[actualTarget]);
            DBGPRINTF("load balancer: trying actualTarget %u [%u]: try %d, isConnected %d, wrkr %p\n", actualTarget,
                      (pWrkrData->actualTarget - 1), trynbr, pTarget->bIsConnected, pWrkrData);
            if (pTarget->bIsConnected) {
                DBGPRINTF("RGER: sending to actualTarget %d: try %d\n", actualTarget, trynbr);
                iRet = processMsg(pTarget, &actParam(pParams, 1, i, 0));
                if (iRet == RS_RET_OK || iRet == RS_RET_DEFER_COMMIT || iRet == RS_RET_PREVIOUS_COMMITTED) {
                    dotry = 0;
                }
            }
            trynbr++;
        }

        if (dotry == 1) {
            LogMsg(0, RS_RET_SUSPENDED, LOG_INFO,
                   "omfwd: [wrkr %u] found no working target server when trying to send "
                   "messages (main loop)",
                   pWrkrData->wrkrID);
            ABORT_FINALIZE(RS_RET_SUSPENDED);
        }
        pWrkrData->nXmit++;
    }

    for (int j = 0; j < pWrkrData->pData->nTargets; ++j) {
        if (pWrkrData->target[j].bIsConnected && pWrkrData->target[j].offsSndBuf != 0) {
            iRet = TCPSendBuf(&(pWrkrData->target[j]), pWrkrData->target[j].sndBuf, pWrkrData->target[j].offsSndBuf,
                              IS_FLUSH);
            if (iRet == RS_RET_OK || iRet == RS_RET_DEFER_COMMIT || iRet == RS_RET_PREVIOUS_COMMITTED) {
                pWrkrData->target[j].offsSndBuf = 0;
            } else {
                LogMsg(0, RS_RET_SUSPENDED, LOG_WARNING,
                       "omfwd: [wrkr %u] target %s:%s became unavailable during buffer flush. "
                       "Remaining messages will be sent when it is online again.",
                       pWrkrData->wrkrID, pWrkrData->target[j].target_name, pWrkrData->target[j].port);
                DestructTCPTargetData(&(pWrkrData->target[j]));
                /* Note: do not return RS_RET_SUSPENDED here. We only return SUSPENDED
                 * when no pool member is left active (see block below after pool stats).
                 * For multi-target pools, other active targets may continue to work
                 * and the remaining buffered data for this target will be flushed once
                 * it resumes on a subsequent transaction.
                 */
                iRet = RS_RET_OK;
            }
        }
    }

finalize_it:
    /*
     * Return semantics for commitTransaction:
     * - Only return RS_RET_SUSPENDED when no pool member is left active.
     *   We determine this by calling countActiveTargets() and inspecting
     *   the atomic nActiveTargets value. When it is zero, the whole pool
     *   is unavailable and the action engine must enter retry logic.
     * - If at least one target remains active, we keep returning OK here
     *   so that the action is not suspended at pool level. Any buffered
     *   frames for failed targets remain in that target's send buffer and
     *   will be flushed once doTryResume() re-establishes the connection
     *   on a subsequent transaction.
     */
    /* do pool stats */

    countActiveTargets(pWrkrData);
    const int nActiveTargets =
        ATOMIC_FETCH_32BIT(&pWrkrData->pData->nActiveTargets, &pWrkrData->pData->mut_nActiveTargets);

    if (nActiveTargets == 0) {
        /*
         * All pool members are currently unavailable. Only in this case we
         * return RS_RET_SUSPENDED from commitTransaction, as required by the
         * omfwd pool contract.
         */
        iRet = RS_RET_SUSPENDED;
    }

    if (iRet == RS_RET_SUSPENDED) {
        LogMsg(0, RS_RET_SUSPENDED, LOG_WARNING,
               "omfwd: [wrkr %d/%" PRIuPTR "] no working target servers in pool available, suspending action",
               pWrkrData->wrkrID, (uintptr_t)pthread_self());
    }
ENDcommitTransaction


/* This function loads TCP support, if not already loaded. It will be called
 * during config processing. To server resources, TCP support will only
 * be loaded if it actually is used. -- rgerhard, 2008-04-17
 */
static rsRetVal loadTCPSupport(void) {
    DEFiRet;
    CHKiRet(objUse(netstrms, LM_NETSTRMS_FILENAME));
    CHKiRet(objUse(netstrm, LM_NETSTRMS_FILENAME));
    CHKiRet(objUse(tcpclt, LM_TCPCLT_FILENAME));

finalize_it:
    RETiRet;
}


/* initialize TCP structures (if necessary) after the instance has been
 * created.
 */
static rsRetVal initTCP(wrkrInstanceData_t *pWrkrData) {
    instanceData *pData;
    DEFiRet;

    pData = pWrkrData->pData;
    if (pData->protocol == FORW_TCP) {
        for (int i = 0; i < pData->nTargets; ++i) {
            /* create our tcpclt */
            CHKiRet(tcpclt.Construct(&pWrkrData->target[i].pTCPClt));
            CHKiRet(tcpclt.SetResendLastOnRecon(pWrkrData->target[i].pTCPClt, pData->bResendLastOnRecon));
            CHKiRet(tcpclt.SetFraming(pWrkrData->target[i].pTCPClt, pData->tcp_framing));
            CHKiRet(tcpclt.SetFramingDelimiter(pWrkrData->target[i].pTCPClt, pData->tcp_framingDelimiter));
            /* and set callbacks */
            CHKiRet(tcpclt.SetSendInit(pWrkrData->target[i].pTCPClt, TCPSendInit));
            CHKiRet(tcpclt.SetSendFrame(pWrkrData->target[i].pTCPClt, TCPSendFrame));
            CHKiRet(tcpclt.SetSendPrepRetry(pWrkrData->target[i].pTCPClt, TCPSendPrepRetry));
        }
    }
finalize_it:
    RETiRet;
}


static void setInstParamDefaults(instanceData *pData) {
    pData->tplName = NULL;
    pData->protocol = FORW_UDP;
    pData->networkNamespace = NULL;
    pData->originalNamespace = -1;
    pData->tcp_framing = TCP_FRAMING_OCTET_STUFFING;
    pData->tcp_framingDelimiter = '\n';
    pData->pszStrmDrvr = NULL;
    pData->pszStrmDrvrAuthMode = NULL;
    pData->pszStrmDrvrPermitExpiredCerts = NULL;
    pData->iStrmDrvrMode = 0;
    pData->iStrmDrvrExtendedCertCheck = 0;
    pData->iStrmDrvrSANPreference = 0;
    pData->iStrmTlsVerifyDepth = 0;
    pData->pszStrmDrvrCAFile = NULL;
    pData->pszStrmDrvrCRLFile = NULL;
    pData->pszStrmDrvrKeyFile = NULL;
    pData->pszStrmDrvrCertFile = NULL;
    pData->iRebindInterval = 0;
    pData->bKeepAlive = 0;
    pData->iKeepAliveProbes = 0;
    pData->iKeepAliveIntvl = 0;
    pData->iKeepAliveTime = 0;
    pData->iConErrSkip = 0;
    pData->gnutlsPriorityString = NULL;
    pData->bResendLastOnRecon = 0;
    pData->bExtendedConnCheck = 1; /* traditionally enabled! */
    pData->bSendToAll = -1; /* unspecified */
    pData->iUDPSendDelay = 0;
    pData->UDPSendBuf = 0;
    pData->pPermPeers = NULL;
    pData->compressionLevel = 9;
    pData->strmCompFlushOnTxEnd = 1;
    pData->compressionMode = COMPRESS_NEVER;
    pData->ipfreebind = IPFREEBIND_ENABLED_WITH_LOG;
    pData->poolResumeInterval = 30;
    pData->ratelimiter = NULL;
    pData->ratelimitInterval = 0;
    pData->ratelimitBurst = 200;
}


/* support statistics gathering - note: counter names are the same as for
 * previous non-pool based code. This permits easy migration in our opinion.
 */
static rsRetVal setupInstStatsCtrs(instanceData *const pData) {
    uchar ctrName[512];
    DEFiRet;

    if (pData->target_name == NULL) {
        /* This is primarily introduced to keep the static analyzer happy. We
         * do not understand how this situation could actually happen.
         */
        LogError(0, RS_RET_INTERNAL_ERROR,
                 "internal error: target_name is NULL in setupInstStatsCtrs() -"
                 "statistics countes are disable. Report this error to the "
                 "rsyslog project please.");
        ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
    }

    CHKmalloc(pData->target_stats = (targetStats_t *)calloc(pData->nTargets, sizeof(targetStats_t)));

    for (int i = 0; i < pData->nTargets; ++i) {
        /* if insufficient ports are configured, we use ports[0] for the * missing ports.  */
        snprintf((char *)ctrName, sizeof(ctrName), "%s-%s-%s", (pData->protocol == FORW_TCP) ? "TCP" : "UDP",
                 pData->target_name[i], pData->ports[(i < pData->nPorts) ? i : 0]);
        ctrName[sizeof(ctrName) - 1] = '\0'; /* be on the save side */
        CHKiRet(statsobj.Construct(&(pData->target_stats[i].stats)));
        CHKiRet(statsobj.SetName(pData->target_stats[i].stats, ctrName));
        CHKiRet(statsobj.SetOrigin(pData->target_stats[i].stats, (uchar *)"omfwd"));

        pData->target_stats[i].sentBytes = 0;
        INIT_ATOMIC_HELPER_MUT64(pData->target_stats[i].mut_sentBytes);
        CHKiRet(statsobj.AddCounter(pData->target_stats[i].stats, UCHAR_CONSTANT("bytes.sent"), ctrType_IntCtr,
                                    CTR_FLAG_RESETTABLE, &(pData->target_stats[i].sentBytes)));

        pData->target_stats[i].sentMsgs = 0;
        INIT_ATOMIC_HELPER_MUT64(pData->target_stats[i].mut_sentMsgs);
        CHKiRet(statsobj.AddCounter(pData->target_stats[i].stats, UCHAR_CONSTANT("messages.sent"), ctrType_IntCtr,
                                    CTR_FLAG_RESETTABLE, &(pData->target_stats[i].sentMsgs)));

        CHKiRet(statsobj.ConstructFinalize(pData->target_stats[i].stats));
    }

finalize_it:
    RETiRet;
}


BEGINnewActInst
    struct cnfparamvals *pvals;
    uchar *tplToUse;
    char *cstr;
    int i;
    rsRetVal localRet;
    int complevel = -1;
    CODESTARTnewActInst;
    DBGPRINTF("newActInst (omfwd)\n");

    pvals = nvlstGetParams(lst, &actpblk, NULL);
    if (pvals == NULL) {
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }

    if (Debug) {
        dbgprintf("action param blk in omfwd:\n");
        cnfparamsPrint(&actpblk, pvals);
    }

    CHKiRet(createInstance(&pData));
    setInstParamDefaults(pData);

    pData->nPorts = 0; /* we need this to detect missing port param */

    for (i = 0; i < actpblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;
        if (!strcmp(actpblk.descr[i].name, "target")) {
            const int nTargets = pvals[i].val.d.ar->nmemb; /* keep static analyzer happy */
            pData->nTargets = nTargets;
            CHKmalloc(pData->target_name = (char **)calloc(pData->nTargets, sizeof(char *)));
            for (int j = 0; j < nTargets; ++j) {
                pData->target_name[j] = (char *)es_str2cstr(pvals[i].val.d.ar->arr[j], NULL);
            }
        } else if (!strcmp(actpblk.descr[i].name, "address")) {
            pData->address = es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "device")) {
            pData->device = es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "port")) {
            pData->nPorts = pvals[i].val.d.ar->nmemb;
            CHKmalloc(pData->ports = (char **)calloc(pData->nPorts, sizeof(char *)));
            for (int j = 0; j < pData->nPorts; ++j) {
                pData->ports[j] = (char *)es_str2cstr(pvals[i].val.d.ar->arr[j], NULL);
            }
        } else if (!strcmp(actpblk.descr[i].name, "protocol")) {
            if (!es_strcasebufcmp(pvals[i].val.d.estr, (uchar *)"udp", 3)) {
                pData->protocol = FORW_UDP;
            } else if (!es_strcasebufcmp(pvals[i].val.d.estr, (uchar *)"tcp", 3)) {
                localRet = loadTCPSupport();
                if (localRet != RS_RET_OK) {
                    LogError(0, localRet,
                             "could not activate network stream modules for TCP "
                             "(internal error %d) - are modules missing?",
                             localRet);
                    ABORT_FINALIZE(localRet);
                }
                pData->protocol = FORW_TCP;
            } else {
                uchar *str;
                str = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
                LogError(0, RS_RET_INVLD_PROTOCOL, "omfwd: invalid protocol \"%s\"", str);
                free(str);
                ABORT_FINALIZE(RS_RET_INVLD_PROTOCOL);
            }
        } else if (!strcmp(actpblk.descr[i].name, "networknamespace")) {
            pData->networkNamespace = es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "tcp_framing")) {
            if (!es_strcasebufcmp(pvals[i].val.d.estr, (uchar *)"traditional", 11)) {
                pData->tcp_framing = TCP_FRAMING_OCTET_STUFFING;
            } else if (!es_strcasebufcmp(pvals[i].val.d.estr, (uchar *)"octet-counted", 13)) {
                pData->tcp_framing = TCP_FRAMING_OCTET_COUNTING;
            } else {
                uchar *str;
                str = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
                LogError(0, RS_RET_CNF_INVLD_FRAMING, "omfwd: invalid framing \"%s\"", str);
                free(str);
                ABORT_FINALIZE(RS_RET_CNF_INVLD_FRAMING);
            }
        } else if (!strcmp(actpblk.descr[i].name, "rebindinterval")) {
            pData->iRebindInterval = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "keepalive")) {
            pData->bKeepAlive = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "keepalive.probes")) {
            pData->iKeepAliveProbes = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "keepalive.interval")) {
            pData->iKeepAliveIntvl = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "keepalive.time")) {
            pData->iKeepAliveTime = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "conerrskip")) {
            pData->iConErrSkip = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "gnutlsprioritystring")) {
            pData->gnutlsPriorityString = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "streamdriver") ||
                   !strcmp(actpblk.descr[i].name, "streamdriver.name")) {
            pData->pszStrmDrvr = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "streamdrivermode") ||
                   !strcmp(actpblk.descr[i].name, "streamdrivermode")) {
            pData->iStrmDrvrMode = pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "streamdriver.CheckExtendedKeyPurpose")) {
            pData->iStrmDrvrExtendedCertCheck = pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "streamdriver.PrioritizeSAN")) {
            pData->iStrmDrvrSANPreference = pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "streamdriver.TlsVerifyDepth")) {
            if (pvals[i].val.d.n >= 2) {
                pData->iStrmTlsVerifyDepth = pvals[i].val.d.n;
            } else {
                parser_errmsg("streamdriver.TlsVerifyDepth must be 2 or higher but is %d", (int)pvals[i].val.d.n);
            }
        } else if (!strcmp(actpblk.descr[i].name, "streamdriverauthmode") ||
                   !strcmp(actpblk.descr[i].name, "streamdriverauthmode")) {
            pData->pszStrmDrvrAuthMode = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "streamdriver.permitexpiredcerts")) {
            uchar *val = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
            if (es_strcasebufcmp(pvals[i].val.d.estr, (uchar *)"off", 3) &&
                es_strcasebufcmp(pvals[i].val.d.estr, (uchar *)"on", 2) &&
                es_strcasebufcmp(pvals[i].val.d.estr, (uchar *)"warn", 4)) {
                parser_errmsg(
                    "streamdriver.permitExpiredCerts must be 'warn', 'off' or 'on' "
                    "but is '%s' - ignoring parameter, using 'off' instead.",
                    val);
                free(val);
            } else {
                pData->pszStrmDrvrPermitExpiredCerts = val;
            }
        } else if (!strcmp(actpblk.descr[i].name, "streamdriver.cafile")) {
            pData->pszStrmDrvrCAFile = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "streamdriver.crlfile")) {
            pData->pszStrmDrvrCRLFile = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "streamdriver.keyfile")) {
            pData->pszStrmDrvrKeyFile = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "streamdriver.certfile")) {
            pData->pszStrmDrvrCertFile = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "streamdriverpermittedpeers")) {
            uchar *start, *str;
            uchar *p;
            int lenStr;
            str = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
            start = str;
            lenStr = ustrlen(start); /* we need length after '\0' has been dropped... */
            while (lenStr > 0) {
                p = start;
                while (*p && *p != ',' && lenStr--) p++;
                if (*p == ',') {
                    *p = '\0';
                }
                if (*start == '\0') {
                    DBGPRINTF("omfwd: ignoring empty permitted peer\n");
                } else {
                    dbgprintf("omfwd: adding permitted peer: '%s'\n", start);
                    CHKiRet(net.AddPermittedPeer(&(pData->pPermPeers), start));
                }
                start = p + 1;
                if (lenStr) --lenStr;
            }
            free(str);
        } else if (!strcmp(actpblk.descr[i].name, "ziplevel")) {
            complevel = pvals[i].val.d.n;
            if (complevel >= 0 && complevel <= 10) {
                pData->compressionLevel = complevel;
                pData->compressionMode = COMPRESS_SINGLE_MSG;
            } else {
                LogError(0, NO_ERRCODE,
                         "Invalid ziplevel %d specified in "
                         "forwarding action - NOT turning on compression.",
                         complevel);
            }
        } else if (!strcmp(actpblk.descr[i].name, "tcp_framedelimiter")) {
            if (pvals[i].val.d.n > 255) {
                parser_errmsg("tcp_frameDelimiter must be below 255 but is %d", (int)pvals[i].val.d.n);
                ABORT_FINALIZE(RS_RET_PARAM_ERROR);
            }
            pData->tcp_framingDelimiter = (uchar)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "resendlastmsgonreconnect")) {
            pData->bResendLastOnRecon = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "extendedconnectioncheck")) {
            pData->bExtendedConnCheck = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "udp.sendtoall")) {
            pData->bSendToAll = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "udp.senddelay")) {
            pData->iUDPSendDelay = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "udp.sendbuf")) {
            pData->UDPSendBuf = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "template")) {
            pData->tplName = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "compression.stream.flushontxend")) {
            pData->strmCompFlushOnTxEnd = (sbool)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "compression.mode")) {
            cstr = es_str2cstr(pvals[i].val.d.estr, NULL);
            if (!strcasecmp(cstr, "stream:always")) {
                pData->compressionMode = COMPRESS_STREAM_ALWAYS;
            } else if (!strcasecmp(cstr, "none")) {
                pData->compressionMode = COMPRESS_NEVER;
            } else if (!strcasecmp(cstr, "single")) {
                pData->compressionMode = COMPRESS_SINGLE_MSG;
            } else {
                LogError(0, RS_RET_PARAM_ERROR,
                         "omfwd: invalid value for 'compression.mode' "
                         "parameter (given is '%s')",
                         cstr);
                free(cstr);
                ABORT_FINALIZE(RS_RET_PARAM_ERROR);
            }
            free(cstr);
        } else if (!strcmp(actpblk.descr[i].name, "ipfreebind")) {
            pData->ipfreebind = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "pool.resumeinterval")) {
            pData->poolResumeInterval = (unsigned int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "ratelimit.burst")) {
            pData->ratelimitBurst = (unsigned int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "ratelimit.interval")) {
            pData->ratelimitInterval = (unsigned int)pvals[i].val.d.n;
        } else {
            LogError(0, RS_RET_INTERNAL_ERROR, "omfwd: program error, non-handled parameter '%s'",
                     actpblk.descr[i].name);
        }
    }

    if (pData->protocol == FORW_UDP && pData->nTargets > 1) {
        parser_warnmsg(
            "you have defined %d targets. Multiple targets are ONLY "
            "supported in TCP mode ignoring all but the first target in UDP mode",
            pData->nTargets);
    }

    /* check if no port is set. If so, we use the IANA-assigned port of 514 */
    if (pData->nPorts == 0) {
        pData->nPorts = 1;
        CHKmalloc(pData->ports = (char **)calloc(pData->nPorts, sizeof(char *)));
        CHKmalloc(pData->ports[0] = strdup("514"));
    }

    if (pData->nPorts > pData->nTargets) {
        parser_warnmsg(
            "defined %d ports, but only %d targets - ignoring "
            "extra ports",
            pData->nPorts, pData->nTargets);
    }

    if (pData->nPorts < pData->nTargets) {
        parser_warnmsg(
            "defined %d ports, but %d targets - using port %s "
            "as default for the additional targets",
            pData->nPorts, pData->nTargets, pData->ports[0]);
    }

    if (complevel != -1) {
        pData->compressionLevel = complevel;
        if (pData->compressionMode == COMPRESS_NEVER) {
            /* to keep compatible with pre-7.3.11, only setting the
             * compression level means old-style single-message mode.
             */
            pData->compressionMode = COMPRESS_SINGLE_MSG;
        }
    }

    CODE_STD_STRING_REQUESTnewActInst(1);

    tplToUse = ustrdup((pData->tplName == NULL) ? getDfltTpl() : pData->tplName);
    CHKiRet(OMSRsetEntry(*ppOMSR, 0, tplToUse, OMSR_NO_RQD_TPL_OPTS));

    if (pData->bSendToAll == -1) {
        pData->bSendToAll = send_to_all;
    } else {
        if (pData->protocol == FORW_TCP) {
            LogError(0, RS_RET_PARAM_ERROR,
                     "omfwd: parameter udp.sendToAll "
                     "cannot be used with tcp transport -- ignored");
        }
    }

    if (pData->address && (pData->protocol == FORW_TCP)) {
        LogError(0, RS_RET_PARAM_ERROR, "omfwd: parameter \"address\" not supported for tcp -- ignored");
    }

    if (pData->ratelimitInterval > 0) {
        CHKiRet(ratelimitNew(&pData->ratelimiter, "omfwd", NULL));
        ratelimitSetLinuxLike(pData->ratelimiter, pData->ratelimitInterval, pData->ratelimitBurst);
        ratelimitSetNoTimeCache(pData->ratelimiter);
    }

    setupInstStatsCtrs(pData);

    CODE_STD_FINALIZERnewActInst;
    cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst


BEGINparseSelectorAct
    uchar *q;
    int i;
    rsRetVal localRet;
    struct addrinfo;
    TCPFRAMINGMODE tcp_framing = TCP_FRAMING_OCTET_STUFFING;
    CODESTARTparseSelectorAct;
    CODE_STD_STRING_REQUESTparseSelectorAct(1) if (*p != '@') ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);

    CHKiRet(createInstance(&pData));
    pData->tcp_framingDelimiter = '\n';

    ++p; /* eat '@' */
    if (*p == '@') { /* indicator for TCP! */
        localRet = loadTCPSupport();
        if (localRet != RS_RET_OK) {
            LogError(0, localRet,
                     "could not activate network stream modules for TCP "
                     "(internal error %d) - are modules missing?",
                     localRet);
            ABORT_FINALIZE(localRet);
        }
        pData->protocol = FORW_TCP;
        ++p; /* eat this '@', too */
    } else {
        pData->protocol = FORW_UDP;
    }
    /* we are now after the protocol indicator. Now check if we should
     * use compression. We begin to use a new option format for this:
     * @(option,option)host:port
     * The first option defined is "z[0..9]" where the digit indicates
     * the compression level. If it is not given, 9 (best compression) is
     * assumed. An example action statement might be:
     * @@(z5,o)127.0.0.1:1400
     * Which means send via TCP with medium (5) compression (z) to the local
     * host on port 1400. The '0' option means that octet-couting (as in
     * IETF I-D syslog-transport-tls) is to be used for framing (this option
     * applies to TCP-based syslog only and is ignored when specified with UDP).
     * That is not yet implemented.
     * rgerhards, 2006-12-07
     * In order to support IPv6 addresses, we must introduce an extension to
     * the hostname. If it is in square brackets, whatever is in them is treated as
     * the hostname - without any exceptions ;) -- rgerhards, 2008-08-05
     */
    if (*p == '(') {
        /* at this position, it *must* be an option indicator */
        do {
            ++p; /* eat '(' or ',' (depending on when called) */
            /* check options */
            if (*p == 'z') { /* compression */
                ++p; /* eat */
                if (isdigit((int)*p)) {
                    int iLevel;
                    iLevel = *p - '0';
                    ++p; /* eat */
                    pData->compressionLevel = iLevel;
                    pData->compressionMode = COMPRESS_SINGLE_MSG;
                } else {
                    LogError(0, NO_ERRCODE,
                             "Invalid compression level '%c' specified in "
                             "forwarding action - NOT turning on compression.",
                             *p);
                }
            } else if (*p == 'o') { /* octet-couting based TCP framing? */
                ++p; /* eat */
                /* no further options settable */
                tcp_framing = TCP_FRAMING_OCTET_COUNTING;
            } else { /* invalid option! Just skip it... */
                LogError(0, NO_ERRCODE, "Invalid option %c in forwarding action - ignoring.", *p);
                ++p; /* eat invalid option */
            }
            /* the option processing is done. We now do a generic skip
             * to either the next option or the end of the option
             * block.
             */
            while (*p && *p != ')' && *p != ',') ++p; /* just skip it */
        } while (*p && *p == ','); /* Attention: do.. while() */
        if (*p == ')')
            ++p; /* eat terminator, on to next */
        else
            /* we probably have end of string - leave it for the rest
             * of the code to handle it (but warn the user)
             */
            LogError(0, NO_ERRCODE, "Option block not terminated in forwarding action.");
    }

    /* extract the host first (we do a trick - we replace the ';' or ':' with a '\0')
     * now skip to port and then template name. rgerhards 2005-07-06
     */
    if (*p == '[') { /* everything is hostname upto ']' */
        ++p; /* skip '[' */
        for (q = p; *p && *p != ']'; ++p) /* JUST SKIP */
            ;
        if (*p == ']') {
            *p = '\0'; /* trick to obtain hostname (later)! */
            ++p; /* eat it */
        }
    } else { /* traditional view of hostname */
        for (q = p; *p && *p != ';' && *p != ':' && *p != '#'; ++p) /* JUST SKIP */
            ;
    }

    pData->tcp_framing = tcp_framing;
    pData->ports = NULL;
    pData->networkNamespace = NULL;

    CHKmalloc(pData->target_name = (char **)malloc(sizeof(char *) * pData->nTargets));
    CHKmalloc(pData->ports = (char **)calloc(pData->nTargets, sizeof(char *)));

    if (*p == ':') { /* process port */
        uchar *tmp;

        *p = '\0'; /* trick to obtain hostname (later)! */
        tmp = ++p;
        for (i = 0; *p && isdigit((int)*p); ++p, ++i) /* SKIP AND COUNT */
            ;
        pData->ports[0] = malloc(i + 1);
        if (pData->ports[0] == NULL) {
            LogError(0, NO_ERRCODE,
                     "Could not get memory to store syslog forwarding port, "
                     "using default port, results may not be what you intend");
            /* we leave f_forw.port set to NULL, this is then handled below */
        } else {
            memcpy(pData->ports[0], tmp, i);
            *(pData->ports[0] + i) = '\0';
        }
    }
    /* check if no port is set. If so, we use the IANA-assigned port of 514 */
    if (pData->ports[0] == NULL) {
        CHKmalloc(pData->ports[0] = strdup("514"));
    }

    /* now skip to template */
    while (*p && *p != ';' && *p != '#' && !isspace((int)*p)) ++p; /*JUST SKIP*/

    if (*p == ';' || *p == '#' || isspace(*p)) {
        uchar cTmp = *p;
        *p = '\0'; /* trick to obtain hostname (later)! */
        CHKmalloc(pData->target_name[0] = strdup((char *)q));
        *p = cTmp;
    } else {
        CHKmalloc(pData->target_name[0] = strdup((char *)q));
    }
    /* We have a port at ports[0], so we must tell the module it is allocated.
     * Otherwise, the port would not be freed.
     */
    pData->nPorts = 1;

    /* copy over config data as needed */
    pData->iRebindInterval = (pData->protocol == FORW_TCP) ? cs.iTCPRebindInterval : cs.iUDPRebindInterval;

    pData->bKeepAlive = cs.bKeepAlive;
    pData->iKeepAliveProbes = cs.iKeepAliveProbes;
    pData->iKeepAliveIntvl = cs.iKeepAliveIntvl;
    pData->iKeepAliveTime = cs.iKeepAliveTime;
    pData->iConErrSkip = cs.iConErrSkip;

    /* process template */
    CHKiRet(cflineParseTemplateName(&p, *ppOMSR, 0, OMSR_NO_RQD_TPL_OPTS, getDfltTpl()));

    if (pData->protocol == FORW_TCP) {
        pData->bResendLastOnRecon = cs.bResendLastOnRecon;
        pData->iStrmDrvrMode = cs.iStrmDrvrMode;
        if (cs.pPermPeers != NULL) {
            pData->pPermPeers = cs.pPermPeers;
            cs.pPermPeers = NULL;
        }
    }
    CODE_STD_FINALIZERparseSelectorAct if (iRet == RS_RET_OK) {
        setupInstStatsCtrs(pData);
    }
ENDparseSelectorAct


/* a common function to free our configuration variables - used both on exit
 * and on $ResetConfig processing. -- rgerhards, 2008-05-16
 */
static void freeConfigVars(void) {
    free(cs.pszStrmDrvr);
    cs.pszStrmDrvr = NULL;
    free(cs.pszStrmDrvrAuthMode);
    cs.pszStrmDrvrAuthMode = NULL;
    free(cs.pPermPeers);
    cs.pPermPeers = NULL;
}


BEGINmodExit
    CODESTARTmodExit;
    /* release what we no longer need */
    objRelease(glbl, CORE_COMPONENT);
    objRelease(net, LM_NET_FILENAME);
    objRelease(netstrm, LM_NETSTRMS_FILENAME);
    objRelease(netstrms, LM_NETSTRMS_FILENAME);
    objRelease(tcpclt, LM_TCPCLT_FILENAME);
    objRelease(statsobj, CORE_COMPONENT);
    freeConfigVars();
ENDmodExit


BEGINqueryEtryPt
    CODESTARTqueryEtryPt;
    CODEqueryEtryPt_STD_OMODTX_QUERIES;
    CODEqueryEtryPt_STD_OMOD8_QUERIES;
    CODEqueryEtryPt_STD_CONF2_QUERIES;
    CODEqueryEtryPt_STD_CONF2_setModCnf_QUERIES;
    CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES;
ENDqueryEtryPt


/* Reset config variables for this module to default values.
 * rgerhards, 2008-03-28
 */
static rsRetVal resetConfigVariables(uchar __attribute__((unused)) * pp, void __attribute__((unused)) * pVal) {
    freeConfigVars();

    /* we now must reset all non-string values */
    cs.iStrmDrvrMode = 0;
    cs.bResendLastOnRecon = 0;
    cs.iUDPRebindInterval = 0;
    cs.iTCPRebindInterval = 0;
    cs.bKeepAlive = 0;
    cs.iKeepAliveProbes = 0;
    cs.iKeepAliveIntvl = 0;
    cs.iKeepAliveTime = 0;
    cs.iConErrSkip = 0;

    return RS_RET_OK;
}


BEGINmodInit(Fwd)
    CODESTARTmodInit;
    INITLegCnfVars;
    *ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
    CODEmodInit_QueryRegCFSLineHdlr CHKiRet(objUse(glbl, CORE_COMPONENT));
    CHKiRet(objUse(net, LM_NET_FILENAME));
    CHKiRet(objUse(statsobj, CORE_COMPONENT));
    CHKiRet(objUse(datetime, CORE_COMPONENT));

    CHKiRet(
        regCfSysLineHdlr((uchar *)"actionforwarddefaulttemplate", 0, eCmdHdlrGetWord, setLegacyDfltTpl, NULL, NULL));
    CHKiRet(
        regCfSysLineHdlr((uchar *)"actionsendtcprebindinterval", 0, eCmdHdlrInt, NULL, &cs.iTCPRebindInterval, NULL));
    CHKiRet(
        regCfSysLineHdlr((uchar *)"actionsendudprebindinterval", 0, eCmdHdlrInt, NULL, &cs.iUDPRebindInterval, NULL));
    CHKiRet(regCfSysLineHdlr((uchar *)"actionsendtcpkeepalive", 0, eCmdHdlrBinary, NULL, &cs.bKeepAlive, NULL));
    CHKiRet(
        regCfSysLineHdlr((uchar *)"actionsendtcpkeepalive_probes", 0, eCmdHdlrInt, NULL, &cs.iKeepAliveProbes, NULL));
    CHKiRet(regCfSysLineHdlr((uchar *)"actionsendtcpkeepalive_intvl", 0, eCmdHdlrInt, NULL, &cs.iKeepAliveIntvl, NULL));
    CHKiRet(regCfSysLineHdlr((uchar *)"actionsendtcpkeepalive_time", 0, eCmdHdlrInt, NULL, &cs.iKeepAliveTime, NULL));
    CHKiRet(regCfSysLineHdlr((uchar *)"actionsendstreamdriver", 0, eCmdHdlrGetWord, NULL, &cs.pszStrmDrvr, NULL));
    CHKiRet(regCfSysLineHdlr((uchar *)"actionsendstreamdrivermode", 0, eCmdHdlrInt, NULL, &cs.iStrmDrvrMode, NULL));
    CHKiRet(regCfSysLineHdlr((uchar *)"actionsendstreamdriverauthmode", 0, eCmdHdlrGetWord, NULL,
                             &cs.pszStrmDrvrAuthMode, NULL));
    CHKiRet(regCfSysLineHdlr((uchar *)"actionsendstreamdriverpermittedpeer", 0, eCmdHdlrGetWord, setPermittedPeer, NULL,
                             NULL));
    CHKiRet(regCfSysLineHdlr((uchar *)"actionsendresendlastmsgonreconnect", 0, eCmdHdlrBinary, NULL,
                             &cs.bResendLastOnRecon, NULL));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL,
                               STD_LOADABLE_MODULE_ID));
ENDmodInit
