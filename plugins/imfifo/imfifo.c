/* imfifo.c
 * This is the named pipe (FIFO) input module for rsyslog.
 * It reads logs line-by-line from configured FIFOs.
 *
 * Copyright 2026 Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "config.h"
#include <stdio.h>
#include <syslog.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <poll.h>

#include "rsyslog.h"
#include "dirty.h"
#include "msg.h"
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "unicode-helper.h"
#include "errmsg.h"
#include "cfsysline.h"
#include "glbl.h"
#include "prop.h"
#include "ruleset.h"
#include "stringbuf.h"

MODULE_TYPE_INPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME("imfifo")

struct instanceConf_s {
    uchar *pszFileName; /* path to named pipe (FIFO) */
    uchar *pszTag; /* tag to apply to messages */
    size_t lenTag;
    int iFacility;
    int iSeverity;
    uchar *pszBindRuleset;
    ruleset_t *pBindRuleset; /* ruleset to bind listener to (use system default if unspecified) */
    int fd; /* open FIFO file descriptor */
    cstr_t *ppCStr; /* line accumulator buffer */
    struct instanceConf_s *next;
    struct instanceConf_s *prev;
};

/* config variables */
struct modConfData_s {
    rsconf_t *pConf; /* our overall config object */
};

/* Module static data */
DEF_IMOD_STATIC_DATA;
DEFobjCurrIf(glbl) DEFobjCurrIf(prop) DEFobjCurrIf(ruleset)

    static prop_t *pInputName = NULL;
static instanceConf_t *confRoot = NULL;
static fd_set rfds;
static int nfds = 0;

#include "im-helper.h" /* must be included AFTER the type definitions! */

static inline void std_checkRuleset_genErrMsg(__attribute__((unused)) modConfData_t *modConf, instanceConf_t *pInst) {
    LogError(0, NO_ERRCODE,
             "imfifo: ruleset '%s' for FIFO %s not found - "
             "using default ruleset instead",
             pInst->pszBindRuleset, pInst->pszFileName);
}

/* action (instance) parameters */
static struct cnfparamdescr inppdescr[] = {{"file", eCmdHdlrString, CNFPARAM_REQUIRED},
                                           {"tag", eCmdHdlrString, CNFPARAM_REQUIRED},
                                           {"severity", eCmdHdlrSeverity, 0},
                                           {"facility", eCmdHdlrFacility, 0},
                                           {"ruleset", eCmdHdlrString, 0}};
static struct cnfparamblk inppblk = {CNFPARAMBLK_VERSION, sizeof(inppdescr) / sizeof(struct cnfparamdescr), inppdescr};

/* create input instance, set default parameters, and
 * add it to the list of instances.
 */
static rsRetVal ATTR_NONNULL(1) createInstance(instanceConf_t **const ppInst) {
    instanceConf_t *pInst;
    DEFiRet;
    CHKmalloc(pInst = malloc(sizeof(instanceConf_t)));
    pInst->next = NULL;
    pInst->prev = NULL;
    pInst->pszBindRuleset = NULL;
    pInst->pBindRuleset = NULL;
    pInst->iSeverity = 5; /* notice */
    pInst->iFacility = 128; /* local0 */

    pInst->pszTag = NULL;
    pInst->lenTag = 0;

    pInst->fd = -1;
    pInst->ppCStr = NULL;
    pInst->pszFileName = NULL;

    *ppInst = pInst;
finalize_it:
    RETiRet;
}

/* This adds a new listener object to the bottom of the list, but
 * it does NOT initialize any data members except for the list
 * pointers themselves.
 */
static rsRetVal ATTR_NONNULL() lstnAdd(instanceConf_t *pInst) {
    DEFiRet;

    /* insert it at the begin of the list */
    pInst->prev = NULL;
    pInst->next = confRoot;

    if (confRoot != NULL) confRoot->prev = pInst;

    confRoot = pInst;

    RETiRet;
}

/* delete a listener object */
static void ATTR_NONNULL(1) lstnFree(instanceConf_t *pInst) {
    DBGPRINTF("lstnFree called for FIFO %s\n", pInst->pszFileName);
    if (pInst->pszFileName != NULL) free(pInst->pszFileName);
    if (pInst->pszTag != NULL) free(pInst->pszTag);
    if (pInst->pszBindRuleset != NULL) free(pInst->pszBindRuleset);
    if (pInst->ppCStr != NULL) rsCStrDestruct(&pInst->ppCStr);
    free(pInst);
}

/* read config  */
BEGINnewInpInst
    struct cnfparamvals *pvals;
    instanceConf_t *pInst = NULL;
    int i;
    CODESTARTnewInpInst;
    DBGPRINTF("newInpInst (imfifo)\n");

    pvals = nvlstGetParams(lst, &inppblk, NULL);
    if (pvals == NULL) {
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }

    if (Debug) {
        DBGPRINTF("input param blk in imfifo:\n");
        cnfparamsPrint(&inppblk, pvals);
    }

    CHKiRet(createInstance(&pInst));

    for (i = 0; i < inppblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;
        if (!strcmp(inppblk.descr[i].name, "file")) {
            CHKmalloc(pInst->pszFileName = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(inppblk.descr[i].name, "tag")) {
            CHKmalloc(pInst->pszTag = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
            pInst->lenTag = es_strlen(pvals[i].val.d.estr);
        } else if (!strcmp(inppblk.descr[i].name, "ruleset")) {
            CHKmalloc(pInst->pszBindRuleset = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(inppblk.descr[i].name, "severity")) {
            pInst->iSeverity = pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "facility")) {
            pInst->iFacility = pvals[i].val.d.n;
        } else {
            DBGPRINTF(
                "imfifo: program error, non-handled "
                "param '%s'\n",
                inppblk.descr[i].name);
        }
    }

    if (pInst->pszFileName == NULL) {
        LogError(0, RS_RET_FILE_NOT_SPECIFIED, "imfifo: named pipe path is not configured");
        ABORT_FINALIZE(RS_RET_FILE_NOT_SPECIFIED);
    }

    CHKiRet(cstrConstruct(&pInst->ppCStr));

    if ((iRet = lstnAdd(pInst)) != RS_RET_OK) {
        ABORT_FINALIZE(iRet);
    }

finalize_it:
    CODE_STD_FINALIZERnewInpInst if (pInst && iRet != RS_RET_OK) lstnFree(pInst);
    cnfparamvalsDestruct(pvals, &inppblk);
ENDnewInpInst

BEGINwillRun
    CODESTARTwillRun;
    /* we need to create the inputName property (only once during our lifetime) */
    CHKiRet(prop.Construct(&pInputName));
    CHKiRet(prop.SetString(pInputName, UCHAR_CONSTANT("imfifo"), sizeof("imfifo") - 1));
    CHKiRet(prop.ConstructFinalize(pInputName));
finalize_it:
ENDwillRun

static rsRetVal enqLine(instanceConf_t *const __restrict__ pInst) {
    DEFiRet;
    smsg_t *pMsg;

    if (cstrLen(pInst->ppCStr) == 0) {
        /* we do not process empty lines */
        FINALIZE;
    }

    CHKiRet(msgConstruct(&pMsg));

    MsgSetMSGoffs(pMsg, 0);
    MsgSetHOSTNAME(pMsg, glbl.GetLocalHostName(), ustrlen(glbl.GetLocalHostName()));
    MsgSetFlowControlType(pMsg, eFLOWCTL_FULL_DELAY);
    MsgSetInputName(pMsg, pInputName);
    MsgSetAPPNAME(pMsg, (const char *)pInst->pszTag);
    MsgSetTAG(pMsg, pInst->pszTag, pInst->lenTag);
    msgSetPRI(pMsg, pInst->iFacility | pInst->iSeverity);
    MsgSetRawMsg(pMsg, (const char *)rsCStrGetBufBeg(pInst->ppCStr), cstrLen(pInst->ppCStr));
    MsgSetRuleset(pMsg, pInst->pBindRuleset);

    submitMsg2(pMsg);
finalize_it:
    RETiRet;
}

static rsRetVal readFIFO(instanceConf_t *const pInst) {
    char buf[4096];
    ssize_t nRead;
    DEFiRet;

    nRead = read(pInst->fd, buf, sizeof(buf));
    if (nRead < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            FINALIZE;
        }
        ABORT_FINALIZE(RS_RET_IO_ERROR);
    } else if (nRead == 0) {
        /* EOF reached (should not happen if opened O_RDWR) */
        ABORT_FINALIZE(RS_RET_EOF);
    }

    for (ssize_t i = 0; i < nRead; ++i) {
        if (buf[i] == '\n') {
            CHKiRet(enqLine(pInst));
            rsCStrTruncate(pInst->ppCStr, cstrLen(pInst->ppCStr)); /* Reset buffer */
        } else {
            CHKiRet(cstrAppendChar(pInst->ppCStr, buf[i]));
        }
    }
finalize_it:
    RETiRet;
}

BEGINrunInput
    struct timeval tv;
    int retval;
    instanceConf_t *pInst;
    rsRetVal readRet;
    CODESTARTrunInput;
    FD_ZERO(&rfds);

    /* Open all pipes */
    for (pInst = confRoot; pInst != NULL; pInst = pInst->next) {
        /* Open O_RDWR to prevent blocking on startup and receiving EOF on disconnect */
        pInst->fd = open((char *)pInst->pszFileName, O_RDWR);
        if (pInst->fd == -1) {
            LogError(errno, RS_RET_FILE_NOT_FOUND, "imfifo: named pipe '%s' could not be opened", pInst->pszFileName);
            continue;
        }

        struct stat st;
        if (fstat(pInst->fd, &st) == 0) {
            if (!S_ISFIFO(st.st_mode)) {
                LogError(0, RS_RET_INVALID_VAR, "imfifo: '%s' is not a named pipe (FIFO)", pInst->pszFileName);
                close(pInst->fd);
                pInst->fd = -1;
                continue;
            }
        } else {
            LogError(errno, RS_RET_SYS_ERR, "imfifo: fstat failed on '%s'", pInst->pszFileName);
            close(pInst->fd);
            pInst->fd = -1;
            continue;
        }

        FD_SET(pInst->fd, &rfds);
        if (pInst->fd >= nfds) {
            nfds = pInst->fd + 1;
        }
        DBGPRINTF("imfifo: successfully opened FIFO '%s' on fd %d\n", pInst->pszFileName, pInst->fd);
    }

    /* Main read/select loop */
    tv.tv_usec = 100000; /* 0.1 second */
    while (glbl.GetGlobalInputTermState() == 0) {
        fd_set temp;
        memcpy(&temp, &rfds, sizeof(fd_set));
        tv.tv_sec = 0;
        tv.tv_usec = 100000;

        retval = select(nfds, &temp, NULL, NULL, &tv);

        while (retval > 0 && glbl.GetGlobalInputTermState() == 0) {
            for (pInst = confRoot; pInst != NULL; pInst = pInst->next) {
                if (pInst->fd != -1 && FD_ISSET(pInst->fd, &temp)) {
                    readRet = readFIFO(pInst);
                    if (readRet == RS_RET_EOF) {
                        /* EOF received, should not happen but close it */
                        close(pInst->fd);
                        FD_CLR(pInst->fd, &rfds);
                        pInst->fd = -1;
                    } else if (readRet != RS_RET_OK) {
                        LogError(errno, readRet, "imfifo: error reading from FIFO '%s'", pInst->pszFileName);
                    }
                    retval--;
                }
            }
        }
    }
    DBGPRINTF("imfifo: terminating upon request of rsyslog core\n");
ENDrunInput

BEGINafterRun
    CODESTARTafterRun;
    instanceConf_t *pInst = confRoot, *nextInst;
    confRoot = NULL;

    DBGPRINTF("imfifo: afterRun\n");

    while (pInst != NULL) {
        nextInst = pInst->next;

        if (pInst->fd != -1) {
            close(pInst->fd);
            pInst->fd = -1;
        }

        lstnFree(pInst);

        pInst = nextInst;
    }

    if (pInputName != NULL) prop.Destruct(&pInputName);
ENDafterRun

BEGINisCompatibleWithFeature
    CODESTARTisCompatibleWithFeature;
    if (eFeat == sFEATURERepeatedMsgReduction) {
        iRet = RS_RET_OK;
    }
ENDisCompatibleWithFeature

BEGINbeginCnfLoad
    CODESTARTbeginCnfLoad;
    pModConf->pConf = pConf;
ENDbeginCnfLoad

BEGINendCnfLoad
    CODESTARTendCnfLoad;
ENDendCnfLoad

BEGINcheckCnf
    instanceConf_t *pInst;
    CODESTARTcheckCnf;
    for (pInst = confRoot; pInst != NULL; pInst = pInst->next) {
        std_checkRuleset(pModConf, pInst);
    }
ENDcheckCnf

BEGINactivateCnf
    CODESTARTactivateCnf;
ENDactivateCnf

BEGINfreeCnf
    CODESTARTfreeCnf;
ENDfreeCnf

BEGINmodExit
    CODESTARTmodExit;
    objRelease(ruleset, CORE_COMPONENT);
    objRelease(glbl, CORE_COMPONENT);
    objRelease(prop, CORE_COMPONENT);
ENDmodExit

BEGINqueryEtryPt
    CODESTARTqueryEtryPt;
    CODEqueryEtryPt_STD_IMOD_QUERIES;
    CODEqueryEtryPt_STD_CONF2_QUERIES;
    CODEqueryEtryPt_STD_CONF2_IMOD_QUERIES;
    CODEqueryEtryPt_IsCompatibleWithFeature_IF_OMOD_QUERIES;
ENDqueryEtryPt

BEGINmodInit()
    CODESTARTmodInit;
    *ipIFVersProvided = CURR_MOD_IF_VERSION;
    CODEmodInit_QueryRegCFSLineHdlr CHKiRet(objUse(ruleset, CORE_COMPONENT));
    CHKiRet(objUse(glbl, CORE_COMPONENT));
    CHKiRet(objUse(prop, CORE_COMPONENT));
ENDmodInit
