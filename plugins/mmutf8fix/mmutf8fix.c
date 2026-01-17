/* mmutf8fix.c
 * fix invalid UTF8 sequences. This is begun as a very simple replacer
 * of non-control characters, and actually breaks some UTF-8 encoding
 * right now. If the module turns out to be useful, it should be enhanced
 * to support modes that really detect invalid UTF8. In the longer term
 * it could also be evolved into an any-charset-to-UTF8 converter. But
 * first let's see if it really gets into widespread enough use.
 *
 * Copyright 2013-2016 Adiscon GmbH.
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
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "msg.h"
#include "errmsg.h"

MODULE_TYPE_OUTPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME("mmutf8fix")


DEF_OMOD_STATIC_DATA;

/* define operation modes we have */
#define MODE_CC 0 /* just fix control characters */
#define MODE_UTF8 1 /* do real UTF-8 fixing */

/* config variables */
typedef struct _instanceData {
    uchar replChar;
    uint8_t mode; /* operations mode */
} instanceData;

typedef struct wrkrInstanceData {
    instanceData *pData;
} wrkrInstanceData_t;

struct modConfData_s {
    rsconf_t *pConf; /* our overall config object */
};
static modConfData_t *loadModConf = NULL; /* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL; /* modConf ptr to use for the current exec process */


/* tables for interfacing with the v6 config system */
/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {{"mode", eCmdHdlrGetWord, 0}, {"replacementchar", eCmdHdlrGetChar, 0}};
static struct cnfparamblk actpblk = {CNFPARAMBLK_VERSION, sizeof(actpdescr) / sizeof(struct cnfparamdescr), actpdescr};

BEGINbeginCnfLoad
    CODESTARTbeginCnfLoad;
    loadModConf = pModConf;
    pModConf->pConf = pConf;
ENDbeginCnfLoad

BEGINendCnfLoad
    CODESTARTendCnfLoad;
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
ENDfreeCnf


BEGINcreateInstance
    CODESTARTcreateInstance;
ENDcreateInstance


BEGINcreateWrkrInstance
    CODESTARTcreateWrkrInstance;
ENDcreateWrkrInstance


BEGINisCompatibleWithFeature
    CODESTARTisCompatibleWithFeature;
ENDisCompatibleWithFeature


BEGINfreeInstance
    CODESTARTfreeInstance;
ENDfreeInstance


BEGINfreeWrkrInstance
    CODESTARTfreeWrkrInstance;
ENDfreeWrkrInstance


static inline void setInstParamDefaults(instanceData *pData) {
    pData->mode = MODE_UTF8;
    pData->replChar = ' ';
}

BEGINnewActInst
    struct cnfparamvals *pvals;
    int i;
    CODESTARTnewActInst;
    DBGPRINTF("newActInst (mmutf8fix)\n");
    if ((pvals = nvlstGetParams(lst, &actpblk, NULL)) == NULL) {
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }

    CODE_STD_STRING_REQUESTnewActInst(1);
    CHKiRet(OMSRsetEntry(*ppOMSR, 0, NULL, OMSR_TPL_AS_MSG));
    CHKiRet(createInstance(&pData));
    setInstParamDefaults(pData);

    for (i = 0; i < actpblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;
        if (!strcmp(actpblk.descr[i].name, "mode")) {
            if (!es_strbufcmp(pvals[i].val.d.estr, (uchar *)"utf-8", sizeof("utf-8") - 1)) {
                pData->mode = MODE_UTF8;
            } else if (!es_strbufcmp(pvals[i].val.d.estr, (uchar *)"controlcharacters",
                                     sizeof("controlcharacters") - 1)) {
                pData->mode = MODE_CC;
            } else {
                char *cstr = es_str2cstr(pvals[i].val.d.estr, NULL);
                LogError(0, RS_RET_INVLD_MODE, "mmutf8fix: invalid mode '%s' - ignored", cstr);
                free(cstr);
            }
        } else if (!strcmp(actpblk.descr[i].name, "replacementchar")) {
            pData->replChar = es_getBufAddr(pvals[i].val.d.estr)[0];
        } else {
            dbgprintf(
                "mmutf8fix: program error, non-handled "
                "param '%s'\n",
                actpblk.descr[i].name);
        }
    }

    CODE_STD_FINALIZERnewActInst;
    cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst


BEGINdbgPrintInstInfo
    CODESTARTdbgPrintInstInfo;
ENDdbgPrintInstInfo


BEGINtryResume
    CODESTARTtryResume;
ENDtryResume


static void doCC(instanceData *pData, uchar *msg, int lenMsg) {
    int i;

    for (i = 0; i < lenMsg; ++i) {
        if (msg[i] < 32 || msg[i] > 126) {
            msg[i] = pData->replChar;
        }
    }
}

/* fix an invalid multibyte sequence */
static void fixInvldMBSeq(instanceData *pData, uchar *msg, int lenMsg, int strtIdx, int cnt) {
    int i, endIdx;

    /* Actually strtIdx + cnt will not exceed msgLen,
       but this check does bring peace of mind */
    endIdx = strtIdx + cnt;
    if (endIdx > lenMsg) endIdx = lenMsg;
    for (i = strtIdx; i < endIdx; ++i) msg[i] = pData->replChar;
}

static void doUTF8(instanceData *pData, uchar *msg, int lenMsg) {
    uchar c;
    int8_t bytesLeft = 0;
    uint32_t codepoint;
    int strtIdx = 0;
    int i;

    for (i = 0; i < lenMsg; ++i) {
        c = msg[i];
        if (bytesLeft) {
            if ((c & 0xc0) != 0x80) {
                /* invalid continuation byte, invalidate all bytes
                   up to (but not including) the current byte
                   startIdx is always set if bytesLeft is set */
                fixInvldMBSeq(pData, msg, lenMsg, strtIdx, i - strtIdx);
                bytesLeft = 0;
                goto startOfSequence;
            } else {
                codepoint = (codepoint << 6) | (c & 0x3f);
                --bytesLeft;
                if (bytesLeft == 0) {
                    int seqLen = i - strtIdx + 1;

                    if (
                        /* an overlong encoding? (a codepoint must use only
                           the minimum number of bytes to represent its value) */
                        (((2 == seqLen) && (codepoint < 0x80)) || ((3 == seqLen) && (codepoint < 0x800)) ||
                         ((4 == seqLen) && (codepoint < 0x10000))) ||
                        /* UTF-16 surrogates? */
                        ((codepoint >= 0xD800) && (codepoint <= 0xDFFF)) ||
                        /* too-large codepoint? */
                        (codepoint > 0x10FFFF)) {
                        /* sequence invalid, invalidate all bytes
                           startIdx is always set if bytesLeft is set */
                        fixInvldMBSeq(pData, msg, lenMsg, strtIdx, seqLen);
                    }
                }
            }
        } else {
        startOfSequence:
            if ((c & 0x80) == 0) {
                /* 1-byte sequence, US-ASCII */
                ; /* nothing to do, all well */
            } else if ((c & 0xe0) == 0xc0) {
                /* 2-byte sequence */
                strtIdx = i;
                bytesLeft = 1;
                codepoint = c & 0x1f;
            } else if ((c & 0xf0) == 0xe0) {
                /* 3-byte sequence */
                strtIdx = i;
                bytesLeft = 2;
                codepoint = c & 0x0f;
            } else if ((c & 0xf8) == 0xf0) {
                /* 4-byte sequence */
                strtIdx = i;
                bytesLeft = 3;
                codepoint = c & 0x07;
            } else { /* invalid, either:
                    - stray continuation byte (0x80 <= x <= 0xBF)
                    - 5&6 byte sequence start (x >= 0xF8) forbidden by RFC3629
                  */
                msg[i] = pData->replChar;
            }
        }
    }
    if (bytesLeft) {
        /* invalid, there was not enough bytes to complete a sequence
           startIdx is always set if bytesLeft is set */
        fixInvldMBSeq(pData, msg, lenMsg, strtIdx, i - strtIdx);
    }
}

BEGINdoAction_NoStrings
    smsg_t **ppMsg = (smsg_t **)pMsgData;
    smsg_t *pMsg = ppMsg[0];
    uchar *msg;
    int lenMsg;
    uchar *tag;
    int lenTag;
    CODESTARTdoAction;
    MsgLock(pMsg);
    lenMsg = getMSGLen(pMsg);
    msg = getMSG(pMsg);
    getTAG(pMsg, &tag, &lenTag, MUTEX_ALREADY_LOCKED);
    if (pWrkrData->pData->mode == MODE_CC) {
        doCC(pWrkrData->pData, msg, lenMsg);
        doCC(pWrkrData->pData, tag, lenTag);
    } else {
        doUTF8(pWrkrData->pData, msg, lenMsg);
        doUTF8(pWrkrData->pData, tag, lenTag);
    }
    MsgUnlock(pMsg);
ENDdoAction


NO_LEGACY_CONF_parseSelectorAct


    BEGINmodExit CODESTARTmodExit;
ENDmodExit


BEGINqueryEtryPt
    CODESTARTqueryEtryPt;
    CODEqueryEtryPt_STD_OMOD_QUERIES;
    CODEqueryEtryPt_STD_OMOD8_QUERIES;
    CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES;
    CODEqueryEtryPt_STD_CONF2_QUERIES;
ENDqueryEtryPt


BEGINmodInit()
    CODESTARTmodInit;
    *ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
    CODEmodInit_QueryRegCFSLineHdlr DBGPRINTF("mmutf8fix: module compiled with rsyslog version %s.\n", VERSION);
ENDmodInit
