/* omtelegram.c
 *
 * This module sends messages to a Telegram chat via the Telegram Bot API.
 *
 * Concurrency & Locking:
 * - Each worker instance holds its own CURL handle.
 * - No shared mutable state exists, so no locks are required.
 *
 * Inspired by ommail for module structure.
 *
 * Copyright 2024.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "config.h"
#include "rsyslog.h"
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include "cfsysline.h"

MODULE_TYPE_OUTPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME("omtelegram")

DEF_OMOD_STATIC_DATA;

typedef struct _instanceData {
    uchar *token;
    uchar *chatid;
    uchar *tplName;
} instanceData;

typedef struct wrkrInstanceData {
    instanceData *pData;
    CURL *curl;
} wrkrInstanceData_t;

static struct cnfparamdescr actpdescr[] = {
    {"token", eCmdHdlrGetWord, 0},
    {"chatid", eCmdHdlrGetWord, 0},
    {"template", eCmdHdlrGetWord, 0},
};
static struct cnfparamblk actpblk = {CNFPARAMBLK_VERSION, sizeof(actpdescr) / sizeof(struct cnfparamdescr), actpdescr};

static void setInstParamDefaults(instanceData *pData) {
    pData->token = NULL;
    pData->chatid = NULL;
    pData->tplName = (uchar *)"RSYSLOG_FileFormat";
}

BEGINcreateInstance
    CODESTARTcreateInstance;
ENDcreateInstance

BEGINcreateWrkrInstance
    CODESTARTcreateWrkrInstance;
    pWrkrData->curl = curl_easy_init();
    if (pWrkrData->curl == NULL) {
        free(pWrkrData);
        *ppWrkrData = NULL;
        return RS_RET_OUT_OF_MEMORY;
    }
ENDcreateWrkrInstance

BEGINfreeInstance
    CODESTARTfreeInstance;
    free(pData->token);
    free(pData->chatid);
    free(pData->tplName);
ENDfreeInstance

BEGINfreeWrkrInstance
    CODESTARTfreeWrkrInstance;
    if (pWrkrData->curl != NULL) {
        curl_easy_cleanup(pWrkrData->curl);
    }
ENDfreeWrkrInstance

BEGINdbgPrintInstInfo
    CODESTARTdbgPrintInstInfo;
    dbgprintf("token='%s' chatid='%s' template='%s'\n", pData->token, pData->chatid, pData->tplName);
ENDdbgPrintInstInfo

BEGINtryResume
    CODESTARTtryResume;
ENDtryResume

BEGINisCompatibleWithFeature
    CODESTARTisCompatibleWithFeature;
    if (eFeat == sFEATURERepeatedMsgReduction) iRet = RS_RET_OK;
ENDisCompatibleWithFeature

BEGINnewActInst
    struct cnfparamvals *pvals;
    int i;
    int bDestructPValsOnExit;
    uchar *tplToUse;
    CODESTARTnewActInst;

    bDestructPValsOnExit = 0;
    pvals = nvlstGetParams(lst, &actpblk, NULL);
    if (pvals == NULL) {
        LogError(0, RS_RET_MISSING_CNFPARAMS, "omtelegram: error reading config parameters");
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }
    bDestructPValsOnExit = 1;

    CHKiRet(createInstance(&pData));
    setInstParamDefaults(pData);

    for (i = 0; i < actpblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;
        if (!strcmp(actpblk.descr[i].name, "token")) {
            pData->token = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "chatid")) {
            pData->chatid = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "template")) {
            pData->tplName = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        }
    }

    if (pData->token == NULL || pData->chatid == NULL) {
        LogError(0, RS_RET_PARAM_ERROR, "omtelegram: both 'token' and 'chatid' must be set");
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    tplToUse = (uchar *)strdup((char *)pData->tplName);
    CHKiRet(OMSRsetEntry(*ppOMSR, 0, tplToUse, OMSR_NO_RQD_TPL_OPTS));

    CODE_STD_FINALIZERnewActInst;
    if (bDestructPValsOnExit) cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst

BEGINparseSelectorAct
    CODESTARTparseSelectorAct;
    (void)pData;
    (void)p;
    ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
    CODE_STD_FINALIZERparseSelectorAct;
ENDparseSelectorAct

BEGINdoAction
    char *msg;
    char url[512];
    char post[2048];
    char *enc = NULL;
    CURLcode res;
    CODESTARTdoAction;

    msg = (char *)ppString[0];
    enc = curl_easy_escape(pWrkrData->curl, msg, 0);
    if (enc == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }
    snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/sendMessage", pWrkrData->pData->token);
    snprintf(post, sizeof(post), "chat_id=%s&text=%s", pWrkrData->pData->chatid, enc);
    curl_easy_setopt(pWrkrData->curl, CURLOPT_URL, url);
    curl_easy_setopt(pWrkrData->curl, CURLOPT_POSTFIELDS, post);
    res = curl_easy_perform(pWrkrData->curl);
    if (res != CURLE_OK) {
        LogError(0, RS_RET_ERR, "omtelegram: curl_easy_perform() failed: %s", curl_easy_strerror(res));
    }
finalize_it:
    if (enc != NULL) curl_free(enc);
ENDdoAction

BEGINmodExit
    CODESTARTmodExit;
ENDmodExit

BEGINqueryEtryPt
    CODESTARTqueryEtryPt;
    CODEqueryEtryPt_STD_OMOD_QUERIES;
    CODEqueryEtryPt_STD_OMOD8_QUERIES;
    CODEqueryEtryPt_STD_CONF2_CNFNAME_QUERIES;
    CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES;
ENDqueryEtryPt

BEGINmodInit()
    CODESTARTmodInit;
    *ipIFVersProvided = CURR_MOD_IF_VERSION;
ENDmodInit

/* vi:set ai: */
