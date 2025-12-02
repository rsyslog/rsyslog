/* omuxsock.c
 * This is the implementation of datgram unix domain socket forwarding.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * Copyright 2010-2016 Adiscon GmbH.
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
#include "rsyslog.h"
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include "conf.h"
#include "srUtils.h"
#include "template.h"
#include "msg.h"
#include "cfsysline.h"
#include "module-template.h"
#include "glbl.h"
#include "errmsg.h"
#include "unicode-helper.h"
#include "net.h"

MODULE_TYPE_OUTPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME("omuxsock")

/* internal structures
 */
DEF_OMOD_STATIC_DATA;
DEFobjCurrIf(glbl) DEFobjCurrIf(net)
#define INVLD_SOCK -1

    typedef struct _instanceData {
    permittedPeers_t *pPermPeers;
    uchar *tplName; /**< Template name */
    uchar *sockName; /**< Socket name */
    char *namespace; /**< Network namespace */
    int sockType; /**< Socket type (DGRAM, STREAM, SEQPACKET) */
    int bAbstract; /**< True if an abstract socket address */
    int bConnected; /**< True if a connection oriented (STREAM, SEQPACKET) */
    int sock; /**< Socket descriptor */
    struct sockaddr_un addr; /**< Unix socket address */
    socklen_t addrLen; /**< The socket address length */
} instanceData;


typedef struct wrkrInstanceData {
    instanceData *pData;
} wrkrInstanceData_t;

/* config data */
typedef struct configSettings_s {
    uchar *tplName; /**< Name of the default template to use */
    uchar *sockName; /**< Name of the default socket to use */
    char *namespace; /**< Network namespace for abstract addresses */
    int sockType; /**< Socket type (DGRAM, STREAM, SEQPACKET) */
    int bAbstract; /**< True if default socket is abstract socket */
    int bConnected; /**< True if connection oriented (STREAM, SEQPACKET) */
} configSettings_t;
static configSettings_t cs;

/* module-global parameters */
static struct cnfparamdescr modpdescr[] = {
    {"template", eCmdHdlrGetWord, 0},        {"abstract", eCmdHdlrInt, 0},
    {"socketname", eCmdHdlrString, 0},       {"sockettype", eCmdHdlrString, 0},
    {"networknamespace", eCmdHdlrString, 0},
};
static struct cnfparamblk modpblk = {CNFPARAMBLK_VERSION, sizeof(modpdescr) / sizeof(struct cnfparamdescr), modpdescr};

/* tables for interfacing with the v6 config system */
/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {
    {"template", eCmdHdlrGetWord, 0},        {"abstract", eCmdHdlrInt, 0},
    {"socketname", eCmdHdlrString, 0},       {"sockettype", eCmdHdlrString, 0},
    {"networknamespace", eCmdHdlrString, 0},
};
static struct cnfparamblk actpblk = {CNFPARAMBLK_VERSION, sizeof(actpdescr) / sizeof(struct cnfparamdescr), actpdescr};

struct modConfData_s {
    rsconf_t *pConf; /**< our overall config object */
    uchar *tplName; /**< default template */
    uchar *sockName; /**< Socket name */
    char *namespace; /**< Network namespace */
    int sockType; /**< Socket type (DGRAM, STREAM, SEQPACKET) */
    int bAbstract; /**< True if socket name is abstract */
    int bConnected; /**< True if socket is connection oriented (STREAM, SEQPACKET) */
};

static modConfData_t *loadModConf = NULL; /* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL; /* modConf ptr to use for the current exec process */

static pthread_mutex_t mutDoAct = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief A structure to map identifiers to socket information
 * @details Socket information includes the socket type, and
 *          whether it is a connection-oriented socket type.
 */
static struct {
    const char *id; /**< The identifier used in configuration */
    int value; /**< The underlying socket type for this identifier */
    int connected; /**< True if the socket type is connection oriented */
} socketType_map[] = {
    {"DGRAM", SOCK_DGRAM, 0},
    {"STREAM", SOCK_STREAM, 1},
#ifdef SOCK_SEQPACKET
    {"SEQPACKET", SOCK_SEQPACKET, 1},
#endif /* def SOCK_SEQPACKET */
};
#define ARRAY_SIZE(n) (sizeof(n) / sizeof((n)[0]))

/**
 * @brief Lookup an identifier to obtain socket information
 * @param estr The identifer to lookup.  The lookup is case
 *             insensitive.
 * @param type The location to store the socket type
 * @param connected The location to store an indicator of whether
 *                  this socket type is connection oriented or not.
 * @return RS_RET_OK on success, otherwise a failure code.
 * @details This uses the above socketType_map structure to find
 *          information for a given socket type identifier.
 */
static rsRetVal ATTR_NONNULL() _decodeSockType(es_str_t *estr, int *type, int *connected) {
    DEFiRet;
    char *cstr = es_str2cstr(estr, NULL);
    size_t index;

    if (!cstr) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    for (index = 0; index < ARRAY_SIZE(socketType_map); ++index) {
        if (!strcasecmp(cstr, socketType_map[index].id)) {
            *type = socketType_map[index].value;
            *connected = socketType_map[index].connected;
            FINALIZE
        }
    }
    LogError(0, RS_RET_ERR, "omuxsock: bad socket type %s", cstr);
    ABORT_FINALIZE(RS_RET_ERR);

finalize_it:
    free(cstr);
    RETiRet;
}

BEGINinitConfVars /* (re)set config variables to default values */
    CODESTARTinitConfVars;
    cs.tplName = NULL;
    cs.sockName = NULL;
    cs.namespace = NULL;
    cs.sockType = SOCK_DGRAM;
    cs.bAbstract = 0;
    cs.bConnected = 0;
ENDinitConfVars


static rsRetVal doTryResume(instanceData *pData);


/* this function gets the default template. It coordinates action between
 * old-style and new-style configuration parts.
 */
static uchar *getDfltTpl(void) {
    if (loadModConf != NULL && loadModConf->tplName != NULL)
        return loadModConf->tplName;
    else if (cs.tplName == NULL)
        return (uchar *)"RSYSLOG_TraditionalForwardFormat";
    else
        return cs.tplName;
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
                 "omuxsock default template already set via module "
                 "global parameter - can no longer be changed");
        ABORT_FINALIZE(RS_RET_ERR);
    }
    free(cs.tplName);
    cs.tplName = newVal;
finalize_it:
    RETiRet;
}


static rsRetVal closeSocket(instanceData *pData) {
    DEFiRet;
    if (pData->sock != INVLD_SOCK) {
        close(pData->sock);
        pData->sock = INVLD_SOCK;
    }
    RETiRet;
}


BEGINbeginCnfLoad
    CODESTARTbeginCnfLoad;
    loadModConf = pModConf;
    pModConf->pConf = pConf;
    pModConf->tplName = NULL;
    pModConf->sockName = NULL;
    pModConf->namespace = NULL;
    pModConf->sockType = SOCK_DGRAM;
    pModConf->bAbstract = 0;
    pModConf->bConnected = 0;
ENDbeginCnfLoad

BEGINsetModCnf
    struct cnfparamvals *pvals = NULL;
    int i;
    CODESTARTsetModCnf;
    pvals = nvlstGetParams(lst, &modpblk, NULL);
    if (pvals == NULL) {
        LogError(0, RS_RET_MISSING_CNFPARAMS,
                 "error processing module "
                 "config parameters [module(...)]");
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }

    if (Debug) {
        dbgprintf("module (global) param blk for omuxsock:\n");
        cnfparamsPrint(&modpblk, pvals);
    }

    for (i = 0; i < modpblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;
        if (!strcmp(modpblk.descr[i].name, "template")) {
            loadModConf->tplName = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
            if (cs.tplName != NULL) {
                LogError(0, RS_RET_DUP_PARAM,
                         "omuxsock: default template "
                         "was already set via legacy directive - may lead to inconsistent "
                         "results.");
            }
        } else if (!strcmp(modpblk.descr[i].name, "abstract")) {
            loadModConf->bAbstract = !!(int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "socketname")) {
            loadModConf->sockName = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(modpblk.descr[i].name, "sockettype")) {
            CHKiRet(_decodeSockType(pvals[i].val.d.estr, &loadModConf->sockType, &loadModConf->bConnected));
        } else if (!strcmp(modpblk.descr[i].name, "networknamespace")) {
            loadModConf->namespace = es_str2cstr(pvals[i].val.d.estr, NULL);
        } else {
            dbgprintf(
                "omuxsock: program error, non-handled "
                "param '%s' in beginCnfLoad\n",
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
    free(cs.tplName);
    cs.tplName = NULL;
    free(cs.sockName);
    cs.sockName = NULL;
    free(cs.namespace);
    cs.namespace = NULL;
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
    free(pModConf->sockName);
    free(pModConf->namespace);
ENDfreeCnf

BEGINcreateInstance
    CODESTARTcreateInstance;
    pData->sock = INVLD_SOCK;
    pData->sockType = SOCK_DGRAM;
ENDcreateInstance

BEGINcreateWrkrInstance
    CODESTARTcreateWrkrInstance;
ENDcreateWrkrInstance


BEGINisCompatibleWithFeature
    CODESTARTisCompatibleWithFeature;
    if (eFeat == sFEATURERepeatedMsgReduction) iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINfreeInstance
    CODESTARTfreeInstance;
    /* final cleanup */
    closeSocket(pData);
    free(pData->tplName);
    free(pData->sockName);
    free(pData->namespace);
ENDfreeInstance

BEGINnewActInst
    struct cnfparamvals *pvals = NULL;
    int i;
    int bHaveAbstract = 0;
    int bHaveSocketType = 0;
    uchar *tplToUse;

    CODESTARTnewActInst;
    CODE_STD_STRING_REQUESTnewActInst(1);
    if ((pvals = nvlstGetParams(lst, &actpblk, NULL)) == NULL) {
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }
    CHKiRet(createInstance(&pData));

    for (i = 0; i < actpblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;
        if (!strcmp(actpblk.descr[i].name, "template")) {
            pData->tplName = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "abstract")) {
            pData->bAbstract = !!(int)pvals[i].val.d.n;
            bHaveAbstract = 1;
        } else if (!strcmp(actpblk.descr[i].name, "socketname")) {
            pData->sockName = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "sockettype")) {
            CHKiRet(_decodeSockType(pvals[i].val.d.estr, &pData->sockType, &pData->bConnected));
            bHaveSocketType = 1;
        } else if (!strcmp(actpblk.descr[i].name, "networknamespace")) {
            pData->namespace = es_str2cstr(pvals[i].val.d.estr, NULL);
        }
    }

    if (!pData->namespace && loadModConf->namespace) {
        CHKmalloc(pData->namespace = strdup(loadModConf->namespace));
    }
    if (!pData->sockName) {
        if (bHaveAbstract) {
            LogError(0, RS_RET_NO_SOCK_CONFIGURED, "omuxsock: abstract configured without socket name\n");
            ABORT_FINALIZE(RS_RET_NO_SOCK_CONFIGURED);
        }
        if (bHaveSocketType) {
            LogError(0, RS_RET_NO_SOCK_CONFIGURED, "omuxsock: socket type configured without socket name\n");
            ABORT_FINALIZE(RS_RET_NO_SOCK_CONFIGURED);
        }
        /*
         * Note that we explicitly want the semantics that these parameters be consumed
         * as a group, and not be individually used as a default if the corresponding
         * parameter is not provided in the instance.
         */
        if (loadModConf != NULL && loadModConf->sockName != NULL) {
            pData->sockName = ustrdup(loadModConf->sockName);
            pData->bAbstract = loadModConf->bAbstract;
            pData->sockType = loadModConf->sockType;
            pData->bConnected = loadModConf->bConnected;
        } else if (cs.sockName == NULL) {
            LogError(0, RS_RET_NO_SOCK_CONFIGURED, "No output socket configured for omuxsock\n");
            ABORT_FINALIZE(RS_RET_NO_SOCK_CONFIGURED);
        } else {
            /*
             * Ownership is transferred here.  There is only one default through cs structure.
             */
            pData->sockName = cs.sockName;
            pData->bAbstract = cs.bAbstract;
            pData->sockType = cs.sockType;
            pData->bConnected = cs.bConnected;
            cs.sockName = NULL; /* pData is now owner and will free it */
        }
    }

    tplToUse = ustrdup((pData->tplName == NULL) ? getDfltTpl() : pData->tplName);
    CHKiRet(OMSRsetEntry(*ppOMSR, 0, tplToUse, OMSR_NO_RQD_TPL_OPTS));

    CODE_STD_FINALIZERnewActInst cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst

BEGINfreeWrkrInstance
    CODESTARTfreeWrkrInstance;
ENDfreeWrkrInstance


BEGINdbgPrintInstInfo
    CODESTARTdbgPrintInstInfo;
    DBGPRINTF("%s", pData->sockName);
ENDdbgPrintInstInfo


/* Send a message
 * rgehards, 2007-12-20
 */
static rsRetVal sendMsg(instanceData *pData, char *msg, size_t len) {
    DEFiRet;
    unsigned lenSent = 0;

    if (pData->sock == INVLD_SOCK) {
        CHKiRet(doTryResume(pData));
    }

    if (pData->sock != INVLD_SOCK) {
        /*
         * This style is perhaps easier to follow.  However note that even for non-connection-oriented
         * sockets, a simple send can be used, as long as connect was called earlier.  The connect
         * parameters are simply used as the default in subsequent send() invocations.
         */
        if (pData->bConnected) {
            lenSent = send(pData->sock, msg, len, 0);
        } else {
            lenSent = sendto(pData->sock, msg, len, 0, (const struct sockaddr *)&pData->addr, pData->addrLen);
        }
        if (lenSent != len) {
            int eno = errno;
            char errStr[1024];

            /*
             * XXX/rgerhards: how is this message correct, in that we are returning OK still?
             * In reality, the remainder of the partially sent message (or never sent message)
             * is simply dropped, and we do not change the state of the socket.
             */
            DBGPRINTF("omuxsock suspending: send%s(), socket %d, error: %d = %s.\n", pData->bConnected ? "" : "to",
                      pData->sock, eno, rs_strerror_r(eno, errStr, sizeof(errStr)));
        }
    }

finalize_it:
    RETiRet;
}


/* open socket to remote system
 */
static rsRetVal openSocket(instanceData *pData) {
    DEFiRet;
    size_t nameLen;

    assert(pData->sock == INVLD_SOCK);

    CHKiRet(net.netns_socket(&pData->sock, AF_UNIX, pData->sockType, 0, pData->namespace));

    /* set up server address structure */
    memset(&pData->addr, 0, sizeof(pData->addr));
    /*
     * For pathname addresses, the +1 is the terminating \0.
     * For abstract addresses, the +1 is the leading \0 and note that there is NO terminating \0.
     */
    nameLen = strlen((char *)pData->sockName);
    if ((nameLen + 1) > sizeof(pData->addr.sun_path)) {
        LogError(0, RS_TRUNCAT_TOO_LARGE, "Socket name '%s' is too long", pData->sockName);
        ABORT_FINALIZE(RS_TRUNCAT_TOO_LARGE);
    }
    pData->addrLen = offsetof(struct sockaddr_un, sun_path) + 1 + nameLen;
    pData->addr.sun_family = AF_UNIX;
    /*
     * Note destination is all \0 initially, so a non-abstract name is properly terminated,
     * and an abstract name doesn't care what follows (and may consume the entire sun_path).
     */
    strncpy(pData->addr.sun_path + pData->bAbstract, (char *)pData->sockName, nameLen);

    /*
     * Note that connect is legal even for non-connected sockets, and the parameters so passed
     * become defaults for the send function.
     */
    if (pData->bConnected && connect(pData->sock, (struct sockaddr *)&pData->addr, pData->addrLen) == -1) {
        LogError(errno, RS_RET_NO_SOCKET, "Error connecting to %ssocket %s", pData->bAbstract ? "abstract " : "",
                 pData->sockName);
        ABORT_FINALIZE(RS_RET_NO_SOCKET);
    }

finalize_it:
    if (iRet != RS_RET_OK) {
        closeSocket(pData);
    }
    RETiRet;
}


/* try to resume connection if it is not ready
 */
static rsRetVal doTryResume(instanceData *pData) {
    DEFiRet;

    if (pData->sock == INVLD_SOCK) {
        DBGPRINTF("omuxsock trying to resume\n");
        iRet = openSocket(pData);

        if (iRet != RS_RET_OK) {
            iRet = RS_RET_SUSPENDED;
        }
    }

    RETiRet;
}


BEGINtryResume
    CODESTARTtryResume;
    iRet = doTryResume(pWrkrData->pData);
ENDtryResume

BEGINdoAction
    char *psz = NULL; /* temporary buffering */
    register unsigned l;
    int iMaxLine;
    CODESTARTdoAction;
    pthread_mutex_lock(&mutDoAct);
    CHKiRet(doTryResume(pWrkrData->pData));

    iMaxLine = glbl.GetMaxLine(runModConf->pConf);

    DBGPRINTF(" omuxsock:%s\n", pWrkrData->pData->sockName);

    psz = (char *)ppString[0];
    l = strlen((char *)psz);
    if ((int)l > iMaxLine) l = iMaxLine;

    CHKiRet(sendMsg(pWrkrData->pData, psz, l));

finalize_it:
    pthread_mutex_unlock(&mutDoAct);
ENDdoAction


BEGINparseSelectorAct
    CODESTARTparseSelectorAct;
    CODE_STD_STRING_REQUESTparseSelectorAct(1)

        /* first check if this config line is actually for us */
        if (strncmp((char *)p, ":omuxsock:", sizeof(":omuxsock:") - 1)) {
        ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
    }

    /* ok, if we reach this point, we have something for us */
    p += sizeof(":omuxsock:") - 1; /* eat indicator sequence (-1 because of '\0'!) */
    CHKiRet(createInstance(&pData));

    /* check if a non-standard template is to be applied */
    CHKiRet(cflineParseTemplateName(&p, *ppOMSR, 0, 0, getDfltTpl()));

    if (cs.sockName == NULL) {
        LogError(0, RS_RET_NO_SOCK_CONFIGURED, "No output socket configured for omuxsock\n");
        ABORT_FINALIZE(RS_RET_NO_SOCK_CONFIGURED);
    }

    pData->sockName = cs.sockName;
    pData->sockType = cs.sockType;
    pData->bAbstract = cs.bAbstract;
    pData->bConnected = cs.bConnected;
    cs.sockName = NULL; /* pData is now owner and will free it */

    CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


/* a common function to free our configuration variables - used both on exit
 * and on $ResetConfig processing. -- rgerhards, 2008-05-16
 */
static void freeConfigVars(void) {
    free(cs.tplName);
    cs.tplName = NULL;
    free(cs.sockName);
    cs.sockName = NULL;
}


BEGINmodExit
    CODESTARTmodExit;
    /* release what we no longer need */
    objRelease(glbl, CORE_COMPONENT);
    objRelease(net, LM_NET_FILENAME);

    freeConfigVars();
ENDmodExit


BEGINqueryEtryPt
    CODESTARTqueryEtryPt;
    CODEqueryEtryPt_STD_OMOD_QUERIES;
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
    return RS_RET_OK;
}


BEGINmodInit()
    CODESTARTmodInit;
    INITLegCnfVars;
    *ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
    CODEmodInit_QueryRegCFSLineHdlr;
    CHKiRet(objUse(glbl, CORE_COMPONENT));
    CHKiRet(objUse(net, LM_NET_FILENAME));

    CHKiRet(regCfSysLineHdlr((uchar *)"omuxsockdefaulttemplate", 0, eCmdHdlrGetWord, setLegacyDfltTpl, NULL, NULL));
    CHKiRet(regCfSysLineHdlr((uchar *)"omuxsocksocket", 0, eCmdHdlrGetWord, NULL, &cs.sockName, NULL));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL,
                               STD_LOADABLE_MODULE_ID));
ENDmodInit

/* vim:set ai:
 */
