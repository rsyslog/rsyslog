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
#include <limits.h>
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
    uchar *replSeq;
    size_t lenReplSeq;
    sbool useReplSeq;
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
static struct cnfparamdescr actpdescr[] = {
    {"mode", eCmdHdlrGetWord, 0}, {"replacementchar", eCmdHdlrGetChar, 0}, {"replacementsequence", eCmdHdlrString, 0}};
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
    free(pData->replSeq);
ENDfreeInstance


BEGINfreeWrkrInstance
    CODESTARTfreeWrkrInstance;
ENDfreeWrkrInstance


static inline void setInstParamDefaults(instanceData *pData) {
    pData->mode = MODE_UTF8;
    pData->replChar = ' ';
    pData->replSeq = NULL;
    pData->lenReplSeq = 0;
    pData->useReplSeq = 0;
}

BEGINnewActInst
    struct cnfparamvals *pvals;
    int i;
    sbool bReplCharSet = 0;
    sbool bReplSeqSet = 0;
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
            bReplCharSet = 1;
        } else if (!strcmp(actpblk.descr[i].name, "replacementsequence")) {
            pData->lenReplSeq = es_strlen(pvals[i].val.d.estr);
            if (pData->lenReplSeq == 0) {
                LogError(0, RS_RET_CONFIG_ERROR, "mmutf8fix: replacementSequence must not be empty");
                ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
            }
            CHKmalloc(pData->replSeq = malloc(pData->lenReplSeq));
            memcpy(pData->replSeq, es_getBufAddr(pvals[i].val.d.estr), pData->lenReplSeq);
            pData->useReplSeq = 1;
            bReplSeqSet = 1;
        } else {
            dbgprintf(
                "mmutf8fix: program error, non-handled "
                "param '%s'\n",
                actpblk.descr[i].name);
        }
    }
    if (bReplCharSet && bReplSeqSet) {
        LogError(0, RS_RET_CONFIG_ERROR, "mmutf8fix: replacementChar and replacementSequence are mutually exclusive");
        free(pData->replSeq);
        pData->replSeq = NULL;
        pData->lenReplSeq = 0;
        pData->useReplSeq = 0;
        ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
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

static rsRetVal ensureSeqBuf(instanceData *pData, int lenMsg, uchar **out) {
    DEFiRet;
    size_t cap;
    if (lenMsg <= 0) {
        *out = NULL;
        FINALIZE;
    }
    if (pData->lenReplSeq != 0 && (size_t)lenMsg > (SIZE_MAX - 1) / pData->lenReplSeq) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }
    cap = ((size_t)lenMsg * pData->lenReplSeq) + 1;
    CHKmalloc(*out = malloc(cap));

finalize_it:
    RETiRet;
}

static rsRetVal beginSeqChange(
    instanceData *pData, const uchar *msg, int lenMsg, uchar **out, size_t *pos, const int copyLen) {
    DEFiRet;
    if (*out == NULL) {
        CHKiRet(ensureSeqBuf(pData, lenMsg, out));
        if (*out == NULL) FINALIZE;
        if (copyLen > 0) {
            memcpy(*out, msg, (size_t)copyLen);
            *pos = (size_t)copyLen;
        }
    }

finalize_it:
    RETiRet;
}

static inline rsRetVal appendReplacement(instanceData *pData, uchar *out, size_t *outLen) {
    DEFiRet;
    if (out == NULL) {
        ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
    }
    memcpy(out + *outLen, pData->replSeq, pData->lenReplSeq);
    *outLen += pData->lenReplSeq;

finalize_it:
    RETiRet;
}

static rsRetVal doCCSeq(instanceData *pData, const uchar *msg, int lenMsg, uchar **out, int *outLen) {
    DEFiRet;
    size_t pos = 0;
    int i;

    *out = NULL;
    *outLen = lenMsg;

    for (i = 0; i < lenMsg; ++i) {
        if (msg[i] < 32 || msg[i] > 126) {
            CHKiRet(beginSeqChange(pData, msg, lenMsg, out, &pos, i));
            CHKiRet(appendReplacement(pData, *out, &pos));
        } else if (*out != NULL) {
            (*out)[pos++] = msg[i];
        }
    }
    if (*out == NULL) FINALIZE;

    (*out)[pos] = '\0';
    if (pos > INT_MAX) {
        free(*out);
        *out = NULL;
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    } else {
        *outLen = (int)pos;
    }

finalize_it:
    RETiRet;
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

static inline sbool invalidCodepoint(const uint32_t codepoint, const int seqLen) {
    return (((2 == seqLen) && (codepoint < 0x80)) || ((3 == seqLen) && (codepoint < 0x800)) ||
            ((4 == seqLen) && (codepoint < 0x10000)) || ((codepoint >= 0xD800) && (codepoint <= 0xDFFF)) ||
            (codepoint > 0x10FFFF));
}

static rsRetVal appendReplacements(instanceData *pData, uchar *out, size_t *pos, const int cnt) {
    DEFiRet;
    int i;

    for (i = 0; i < cnt; ++i) {
        CHKiRet(appendReplacement(pData, out, pos));
    }

finalize_it:
    RETiRet;
}

static rsRetVal doUTF8Seq(instanceData *pData, const uchar *msg, int lenMsg, uchar **out, int *outLen) {
    DEFiRet;
    size_t pos = 0;
    int i = 0;

    *out = NULL;
    *outLen = lenMsg;

    while (i < lenMsg) {
        const uchar c = msg[i];
        int seqLen = 0;
        uint32_t codepoint = 0;

        if ((c & 0x80) == 0) {
            if (*out != NULL) {
                (*out)[pos++] = c;
            }
            ++i;
            continue;
        } else if ((c & 0xe0) == 0xc0) {
            seqLen = 2;
            codepoint = c & 0x1f;
        } else if ((c & 0xf0) == 0xe0) {
            seqLen = 3;
            codepoint = c & 0x0f;
        } else if ((c & 0xf8) == 0xf0) {
            seqLen = 4;
            codepoint = c & 0x07;
        } else {
            CHKiRet(beginSeqChange(pData, msg, lenMsg, out, &pos, i));
            CHKiRet(appendReplacement(pData, *out, &pos));
            ++i;
            continue;
        }

        int j;
        for (j = 1; j < seqLen && i + j < lenMsg; ++j) {
            if ((msg[i + j] & 0xc0) != 0x80) break;
            codepoint = (codepoint << 6) | (msg[i + j] & 0x3f);
        }

        if (j < seqLen) {
            CHKiRet(beginSeqChange(pData, msg, lenMsg, out, &pos, i));
            /* Match doUTF8(): replace bytes accepted as part of the
             * malformed sequence and reprocess the non-continuation byte as
             * the start of the next sequence.
             */
            CHKiRet(appendReplacements(pData, *out, &pos, j));
            i += j;
        } else if (invalidCodepoint(codepoint, seqLen)) {
            CHKiRet(beginSeqChange(pData, msg, lenMsg, out, &pos, i));
            CHKiRet(appendReplacements(pData, *out, &pos, seqLen));
            i += seqLen;
        } else {
            if (*out != NULL) {
                memcpy(*out + pos, msg + i, seqLen);
                pos += seqLen;
            }
            i += seqLen;
        }
    }
    if (*out == NULL) FINALIZE;

    (*out)[pos] = '\0';
    if (pos > INT_MAX) {
        free(*out);
        *out = NULL;
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    } else {
        *outLen = (int)pos;
    }

finalize_it:
    RETiRet;
}

static rsRetVal fixSeq(instanceData *pData, const uchar *msg, int lenMsg, uchar **out, int *outLen) {
    if (pData->mode == MODE_CC) {
        return doCCSeq(pData, msg, lenMsg, out, outLen);
    } else {
        return doUTF8Seq(pData, msg, lenMsg, out, outLen);
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
    if (pWrkrData->pData->useReplSeq) {
        uchar *fixedMsg = NULL;
        uchar *fixedTag = NULL;
        uchar *fixedSD = NULL;
        char *sdCopy = NULL;
        int fixedMsgLen = 0;
        int fixedTagLen = 0;
        int fixedSDLen = 0;
        lenMsg = getMSGLen(pMsg);
        msg = getMSG(pMsg);
        iRet = fixSeq(pWrkrData->pData, msg, lenMsg, &fixedMsg, &fixedMsgLen);
        if (iRet != RS_RET_OK) goto done;
        getTAG(pMsg, &tag, &lenTag, MUTEX_ALREADY_LOCKED);
        iRet = fixSeq(pWrkrData->pData, tag, lenTag, &fixedTag, &fixedTagLen);
        if (iRet != RS_RET_OK) goto done;
        if (MsgHasStructuredData(pMsg)) {
            iRet = fixSeq(pWrkrData->pData, pMsg->pszStrucData, (int)pMsg->lenStrucData, &fixedSD, &fixedSDLen);
            if (iRet != RS_RET_OK) goto done;
        }
        if (fixedSD != NULL) {
            sdCopy = malloc((size_t)fixedSDLen + 1);
            if (sdCopy == NULL) {
                iRet = RS_RET_OUT_OF_MEMORY;
                goto done;
            }
            memcpy(sdCopy, fixedSD, (size_t)fixedSDLen + 1);
        }

        if (fixedMsg != NULL) {
            iRet = MsgReplaceMSG(pMsg, fixedMsg, fixedMsgLen);
            if (iRet != RS_RET_OK) goto done;
        }
        if (fixedTag != NULL) {
            MsgSetTAG(pMsg, fixedTag, (size_t)fixedTagLen);
        }
        if (sdCopy != NULL) {
            free(pMsg->pszStrucData);
            pMsg->pszStrucData = (uchar *)sdCopy;
            pMsg->lenStrucData = (rs_size_t)fixedSDLen;
            sdCopy = NULL;
        }
    done:
        free(fixedMsg);
        free(fixedTag);
        free(fixedSD);
        free(sdCopy);
    } else {
        lenMsg = getMSGLen(pMsg);
        msg = getMSG(pMsg);
        getTAG(pMsg, &tag, &lenTag, MUTEX_ALREADY_LOCKED);
        if (pWrkrData->pData->mode == MODE_CC) {
            doCC(pWrkrData->pData, msg, lenMsg);
            doCC(pWrkrData->pData, tag, lenTag);
            if (MsgHasStructuredData(pMsg)) doCC(pWrkrData->pData, pMsg->pszStrucData, (int)pMsg->lenStrucData);
        } else {
            doUTF8(pWrkrData->pData, msg, lenMsg);
            doUTF8(pWrkrData->pData, tag, lenTag);
            if (MsgHasStructuredData(pMsg)) doUTF8(pWrkrData->pData, pMsg->pszStrucData, (int)pMsg->lenStrucData);
        }
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
