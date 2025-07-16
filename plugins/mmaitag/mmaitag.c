/**
 * @file mmaitag.c
 * @brief AI-based message classification plugin.
 *
 * This module contacts an external AI service to classify log
 * messages. The result of the classification is stored in a
 * message variable (defaults to `$.aitag`). Messages are processed
 * one by one through a pluggable provider interface. The
 * initial implementation contains a Gemini provider as well as a
 * mock backend used for testing.
 *
 * Copyright 2025 Adiscon GmbH.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 * -or-
 * see COPYING.ASL20 in the source distribution
 */
#include "config.h"
#include "rsyslog.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h> /* For strerror() */
#include <json.h>
#include "conf.h"
#include "syslogd-types.h"
#include "module-template.h"
#include "msg.h"
#include "cfsysline.h"
#include "ai_provider.h"

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("mmaitag")

DEF_OMOD_STATIC_DATA

static struct cnfparamdescr actpdescr[] = {
    {"provider", eCmdHdlrString, 0},      {"tag", eCmdHdlrString, 0},
    {"model", eCmdHdlrString, 0},         {"expert.initial_prompt", eCmdHdlrString, 0},
    {"inputproperty", eCmdHdlrString, 0}, {"apikey", eCmdHdlrString, 0},
    {"apikey_file", eCmdHdlrString, 0}};
static struct cnfparamblk actpblk = {CNFPARAMBLK_VERSION, sizeof(actpdescr) / sizeof(struct cnfparamdescr), actpdescr};

typedef struct _instanceData {
    char *provider_name;
    char *tag;
    char *model;
    char *prompt;
    msgPropDescr_t *inputProp;
    char *apikey;
    char *apikey_file;
    ai_provider_t *provider;
} instanceData;

typedef struct wrkrInstanceData {
    instanceData *pData;
} wrkrInstanceData_t;

static ai_provider_t *get_provider(const char *name);  // Forward declaration

BEGINcreateInstance
    CODESTARTcreateInstance
ENDcreateInstance

BEGINfreeInstance
    CODESTARTfreeInstance free(pData->provider_name);
    free(pData->tag);
    free(pData->model);
    free(pData->prompt);
    free(pData->apikey);
    free(pData->apikey_file);
    if (pData->inputProp) {
        msgPropDescrDestruct(pData->inputProp);
        free(pData->inputProp);
    }
    if (pData->provider && pData->provider->cleanup) pData->provider->cleanup(pData->provider);
ENDfreeInstance

BEGINcreateWrkrInstance
    CODESTARTcreateWrkrInstance pWrkrData->pData = pData;
ENDcreateWrkrInstance

BEGINfreeWrkrInstance
    CODESTARTfreeWrkrInstance
ENDfreeWrkrInstance

BEGINdbgPrintInstInfo
    CODESTARTdbgPrintInstInfo dbgprintf("mmaitag provider=%s tag=%s\n", pData->provider_name, pData->tag);
ENDdbgPrintInstInfo

BEGINtryResume
    CODESTARTtryResume
ENDtryResume

BEGINisCompatibleWithFeature
    CODESTARTisCompatibleWithFeature if (eFeat == sFEATURERepeatedMsgReduction) iRet = RS_RET_OK;
ENDisCompatibleWithFeature

static void setInstParamDefaults(instanceData *pData) {
    pData->provider_name = strdup("gemini");
    pData->tag = strdup(".aitag");
    pData->model = strdup("gemini-2.0-flash");
    pData->prompt = strdup(
        "Task: Classify the log message that follows. "
        "Output: Exactly one label from this list: NOISE, REGULAR, IMPORTANT, CRITICAL. "
        "Restrictions: No other text, explanations, formatting, or newline characters.");
    pData->inputProp = NULL;
    pData->apikey = NULL;
    pData->apikey_file = NULL;
    pData->provider = NULL;
}

BEGINdoAction_NoStrings
    smsg_t **ppMsg = (smsg_t **)pMsgData;
    smsg_t *pMsg = ppMsg[0];
    const char *msgs[1];
    char **tags = NULL;
    uchar *val;
    rs_size_t len;
    unsigned short freeBuf = 0;
    instanceData *const pData = pWrkrData->pData;
    CODESTARTdoAction if (pData->inputProp == NULL) getRawMsg(pMsg, &val, &len);
    else val = MsgGetProp(pMsg, NULL, pData->inputProp, &len, &freeBuf, NULL);
    msgs[0] = strndup((char *)val, len);
    if (freeBuf) free(val);
    if (pData->provider && pData->provider->classify)
        CHKiRet(pData->provider->classify(pData->provider, msgs, 1, &tags));
    if (tags && tags[0]) {
        struct json_object *j = json_object_new_string(tags[0]);
        msgAddJSON(pMsg, (uchar *)pData->tag, j, 0, 0);
    }
finalize_it:
    if (msgs[0]) free((void *)msgs[0]);
    if (tags && tags[0]) free(tags[0]);
    free(tags);
ENDdoAction


BEGINnewActInst
    struct cnfparamvals *pvals;
    int i;
    CODESTARTnewActInst if ((pvals = nvlstGetParams(lst, &actpblk, NULL)) == NULL) {
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }
    CODE_STD_STRING_REQUESTnewActInst(1);
    CHKiRet(OMSRsetEntry(*ppOMSR, 0, NULL, OMSR_TPL_AS_MSG));
    CHKiRet(createInstance(&pData));
    setInstParamDefaults(pData);
    for (i = 0; i < actpblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;
        if (!strcmp(actpblk.descr[i].name, "provider")) {
            free(pData->provider_name);
            pData->provider_name = es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "tag")) {
            free(pData->tag);
            pData->tag = es_str2cstr(pvals[i].val.d.estr, NULL);
            if (pData->tag[0] == '$') memmove(pData->tag, pData->tag + 1, strlen(pData->tag));
        } else if (!strcmp(actpblk.descr[i].name, "model")) {
            free(pData->model);
            pData->model = es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "expert.initial_prompt")) {
            free(pData->prompt);
            pData->prompt = es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "inputproperty")) {
            char *c = es_str2cstr(pvals[i].val.d.estr, NULL);
            CHKmalloc(pData->inputProp = malloc(sizeof(msgPropDescr_t)));
            CHKiRet(msgPropDescrFill(pData->inputProp, (uchar *)c, strlen(c)));
            free(c);
        } else if (!strcmp(actpblk.descr[i].name, "apikey")) {
            free(pData->apikey);
            pData->apikey = es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "apikey_file")) {
            free(pData->apikey_file);
            pData->apikey_file = es_str2cstr(pvals[i].val.d.estr, NULL);
        }
    }

    // ✅ --- Robust API Key File Reading ---
    if (pData->apikey == NULL && pData->apikey_file != NULL) {
        FILE *fp = fopen(pData->apikey_file, "r");
        if (fp == NULL) {
            LogError(0, RS_RET_ERR, "mmaitag: could not open apikey_file '%s': %s", pData->apikey_file,
                     strerror(errno));
        } else {
            char *line = NULL;
            size_t len = 0;
            if (getline(&line, &len, fp) != -1) {
                // Strip trailing newline characters
                line[strcspn(line, "\r\n")] = '\0';
                pData->apikey = line;  // Transfer ownership of allocated buffer
            } else {
                free(line);  // Free buffer if getline failed or file was empty
            }
            fclose(fp);
        }
    }

    if (pData->provider == NULL) {
        pData->provider = get_provider(pData->provider_name);
        if (pData->provider == NULL) {
            LogError(0, RS_RET_ERR, "mmaitag: unknown provider '%s'", pData->provider_name);
            ABORT_FINALIZE(RS_RET_ERR);
        }
        if (pData->provider->init)
            CHKiRet(pData->provider->init(pData->provider, pData->model, pData->apikey, pData->prompt));
    }
    CODE_STD_FINALIZERnewActInst cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst

static ai_provider_t *get_provider(const char *name) {
    // This function will need to be defined based on which providers are compiled in.
    // For now, it's just a placeholder. You will need to provide the actual
    // gemini_provider and gemini_mock_provider objects.
    if (!strcmp(name, "gemini")) return &gemini_provider;
    if (!strcmp(name, "gemini_mock")) return &gemini_mock_provider;
    return NULL;
}

NO_LEGACY_CONF_parseSelectorAct

    BEGINmodExit CODESTARTmodExit
ENDmodExit

BEGINqueryEtryPt
    CODESTARTqueryEtryPt CODEqueryEtryPt_STD_OMOD_QUERIES
        CODEqueryEtryPt_STD_OMOD8_QUERIES CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES
ENDqueryEtryPt

BEGINmodInit()
    CODESTARTmodInit;
    *ipIFVersProvided = CURR_MOD_IF_VERSION;
    CODEmodInit_QueryRegCFSLineHdlr
ENDmodInit
