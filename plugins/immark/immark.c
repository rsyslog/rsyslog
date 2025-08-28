/* immark.c
 * This is the implementation of the build-in mark message input module.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * File begun on 2007-07-20 by RGerhards (extracted from syslogd.c)
 * This file is under development and has not yet arrived at being fully
 * self-contained and a real object. So far, it is mostly an excerpt
 * of the "old" message code without any modifications. However, it
 * helps to have things at the right place one we go to the meat of it.
 *
 * Copyright 2007-2020 Adiscon GmbH.
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
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include "dirty.h"
#include "cfsysline.h"
#include "module-template.h"
#include "errmsg.h"
#include "msg.h"
#include "srUtils.h"
#include "glbl.h"
#include "unicode-helper.h"
#include "ruleset.h"
#include "prop.h"

MODULE_TYPE_INPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME("immark")

/* defines */
#define DEFAULT_MARK_PERIOD (20 * 60)

/* Module static data */
DEF_IMOD_STATIC_DATA;
DEFobjCurrIf(glbl) DEFobjCurrIf(prop) DEFobjCurrIf(ruleset)

    static int iMarkMessagePeriod = DEFAULT_MARK_PERIOD;
struct modConfData_s {
    rsconf_t *pConf; /* our overall config object */
    const char *pszMarkMsgText;
    size_t lenMarkMsgText;
    uchar *pszBindRuleset;
    ruleset_t *pBindRuleset;
    int flags;
    int bUseMarkFlag;
    int bUseSyslogAPI;
    int iMarkMessagePeriod;
    sbool configSetViaV2Method;
};

/* module-global parameters */
static struct cnfparamdescr modpdescr[] = {{"ruleset", eCmdHdlrString, 0},
                                           {"markmessagetext", eCmdHdlrString, 0},
                                           {"use.syslogcall", eCmdHdlrBinary, 0},
                                           {"use.markflag", eCmdHdlrBinary, 0},
                                           {"interval", eCmdHdlrInt, 0}};
static struct cnfparamblk modpblk = {CNFPARAMBLK_VERSION, sizeof(modpdescr) / sizeof(struct cnfparamdescr), modpdescr};


static modConfData_t *loadModConf = NULL; /* modConf ptr to use for the current load process */
static int bLegacyCnfModGlobalsPermitted; /* are legacy module-global config parameters permitted? */
static prop_t *pInternalInputName = NULL;


BEGINisCompatibleWithFeature
    CODESTARTisCompatibleWithFeature;
    if (eFeat == sFEATURENonCancelInputTermination) iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINafterRun
    CODESTARTafterRun;
ENDafterRun


BEGINbeginCnfLoad
    CODESTARTbeginCnfLoad;
    loadModConf = pModConf;
    pModConf->pConf = pConf;
    /* init our settings */
    pModConf->pszMarkMsgText = NULL;
    pModConf->iMarkMessagePeriod = DEFAULT_MARK_PERIOD;
    pModConf->bUseSyslogAPI = 1;
    pModConf->bUseMarkFlag = 1;
    pModConf->pszBindRuleset = NULL;
    pModConf->pBindRuleset = NULL;
    loadModConf->configSetViaV2Method = 0;
    bLegacyCnfModGlobalsPermitted = 1;
ENDbeginCnfLoad

static rsRetVal checkRuleset(modConfData_t *modConf) {
    ruleset_t *pRuleset;
    rsRetVal localRet;
    DEFiRet;

    if (modConf->pszBindRuleset == NULL) FINALIZE;

    localRet = ruleset.GetRuleset(modConf->pConf, &pRuleset, modConf->pszBindRuleset);
    if (localRet == RS_RET_NOT_FOUND) {
        LogError(0, NO_ERRCODE,
                 "immark: ruleset '%s' not found - "
                 "using default ruleset instead",
                 modConf->pszBindRuleset);
    }
    CHKiRet(localRet);
    modConf->pBindRuleset = pRuleset;

finalize_it:
    RETiRet;
}

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
        dbgprintf("module (global) param blk for immark:\n");
        cnfparamsPrint(&modpblk, pvals);
    }

    for (i = 0; i < modpblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;
        if (!strcmp(modpblk.descr[i].name, "interval")) {
            loadModConf->iMarkMessagePeriod = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "use.syslogcall")) {
            loadModConf->bUseSyslogAPI = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "use.markflag")) {
            loadModConf->bUseMarkFlag = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "ruleset")) {
            loadModConf->pszBindRuleset = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(modpblk.descr[i].name, "markmessagetext")) {
            loadModConf->pszMarkMsgText = es_str2cstr(pvals[i].val.d.estr, NULL);
        } else {
            dbgprintf(
                "immark: program error, non-handled "
                "param '%s' in beginCnfLoad\n",
                modpblk.descr[i].name);
        }
    }

    /* disable legacy module-global config directives */
    bLegacyCnfModGlobalsPermitted = 0;
    loadModConf->configSetViaV2Method = 1;

finalize_it:
    if (pvals != NULL) cnfparamvalsDestruct(pvals, &modpblk);
ENDsetModCnf


BEGINendCnfLoad
    CODESTARTendCnfLoad;
    if (!loadModConf->configSetViaV2Method) {
        pModConf->iMarkMessagePeriod = iMarkMessagePeriod;
    }
ENDendCnfLoad


BEGINcheckCnf
    CODESTARTcheckCnf;
    pModConf->flags = (pModConf->bUseMarkFlag) ? MARK : 0;
    if (pModConf->pszMarkMsgText == NULL) {
        pModConf->pszMarkMsgText = strdup("-- MARK --");
    }
    pModConf->lenMarkMsgText = strlen(pModConf->pszMarkMsgText);
    if (pModConf->pszBindRuleset != NULL) {
        checkRuleset(pModConf);
        if (pModConf->bUseSyslogAPI) {
            LogError(0, NO_ERRCODE,
                     "immark: ruleset specified, but configured to log "
                     "via syslog call - switching to rsyslog-internal logging");
            pModConf->bUseSyslogAPI = 0;
        }
    }
    if (pModConf->iMarkMessagePeriod == 0) {
        LogError(0, NO_ERRCODE, "immark: mark message period must not be 0, can not run");
        ABORT_FINALIZE(RS_RET_NO_RUN); /* we can not run with this error */
    }
finalize_it:
ENDcheckCnf


BEGINactivateCnf
    CODESTARTactivateCnf;
    MarkInterval = pModConf->iMarkMessagePeriod;
    DBGPRINTF("immark set MarkInterval to %d\n", MarkInterval);
ENDactivateCnf


BEGINfreeCnf
    CODESTARTfreeCnf;
    /* free configuration strings allocated during load to avoid leaks */
    free((char *)pModConf->pszMarkMsgText);
    pModConf->pszMarkMsgText = NULL;
    free(pModConf->pszBindRuleset);
    pModConf->pszBindRuleset = NULL;
ENDfreeCnf


static rsRetVal injectMarkMessage(const int pri) {
    smsg_t *pMsg;
    DEFiRet;

    CHKiRet(msgConstruct(&pMsg));
    pMsg->msgFlags = loadModConf->flags;
    MsgSetInputName(pMsg, pInternalInputName);
    MsgSetRawMsg(pMsg, loadModConf->pszMarkMsgText, loadModConf->lenMarkMsgText);
    MsgSetHOSTNAME(pMsg, glbl.GetLocalHostName(), ustrlen(glbl.GetLocalHostName()));
    MsgSetRcvFrom(pMsg, glbl.GetLocalHostNameProp());
    MsgSetRcvFromIP(pMsg, glbl.GetLocalHostIP());
    MsgSetMSGoffs(pMsg, 0);
    MsgSetTAG(pMsg, (const uchar *)"rsyslogd:", sizeof("rsyslogd:") - 1);
    msgSetPRI(pMsg, pri);
    MsgSetRuleset(pMsg, loadModConf->pBindRuleset);
    submitMsg2(pMsg);
finalize_it:
    RETiRet;
}

/* This function is called to gather input. It must terminate only
 * a) on failure (iRet set accordingly)
 * b) on termination of the input module (as part of the unload process)
 * Code begun 2007-12-12 rgerhards
 *
 * This code must simply spawn emit a mark message at each mark interval.
 * We are running on our own thread, so this is extremely easy: we just
 * sleep MarkInterval seconds and each time we awake, we inject the message.
 * Please note that we do not do the other fancy things that sysklogd
 * (and pre 1.20.2 releases of rsyslog) did in mark procesing. They simply
 * do not belong here.
 */
BEGINrunInput
    CODESTARTrunInput;
    /* this is an endless loop - it is terminated when the thread is
     * signalled to do so. This, however, is handled by the framework,
     * right into the sleep below.
     */
    while (1) {
        srSleep(MarkInterval, 0); /* seconds, micro seconds */

        if (glbl.GetGlobalInputTermState() == 1) break; /* terminate input! */

        dbgprintf("immark: injecting mark message\n");
        if (loadModConf->bUseSyslogAPI) {
            logmsgInternal(NO_ERRCODE, LOG_SYSLOG | LOG_INFO, (uchar *)loadModConf->pszMarkMsgText, loadModConf->flags);
        } else {
            injectMarkMessage(LOG_SYSLOG | LOG_INFO);
        }
    }
ENDrunInput


BEGINwillRun
    CODESTARTwillRun;
ENDwillRun


BEGINmodExit
    CODESTARTmodExit;
    if (pInternalInputName != NULL) prop.Destruct(&pInternalInputName);
    objRelease(ruleset, CORE_COMPONENT);
    objRelease(prop, CORE_COMPONENT);
ENDmodExit


BEGINqueryEtryPt
    CODESTARTqueryEtryPt;
    CODEqueryEtryPt_STD_IMOD_QUERIES;
    CODEqueryEtryPt_STD_CONF2_QUERIES;
    CODEqueryEtryPt_STD_CONF2_setModCnf_QUERIES;
    CODEqueryEtryPt_IsCompatibleWithFeature_IF_OMOD_QUERIES;
ENDqueryEtryPt

static rsRetVal resetConfigVariables(uchar __attribute__((unused)) * pp, void __attribute__((unused)) * pVal) {
    iMarkMessagePeriod = DEFAULT_MARK_PERIOD;
    return RS_RET_OK;
}

BEGINmodInit()
    CODESTARTmodInit;
    *ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
    CODEmodInit_QueryRegCFSLineHdlr CHKiRet(objUse(glbl, CORE_COMPONENT));
    CHKiRet(objUse(prop, CORE_COMPONENT));
    CHKiRet(objUse(ruleset, CORE_COMPONENT));

    /* we need to create the inputName property (only once during our lifetime) */
    CHKiRet(prop.Construct(&pInternalInputName));
    CHKiRet(prop.SetString(pInternalInputName, UCHAR_CONSTANT("immark"), sizeof("immark") - 1));
    CHKiRet(prop.ConstructFinalize(pInternalInputName));

    /* legacy config handlers */
    CHKiRet(regCfSysLineHdlr2((uchar *)"markmessageperiod", 0, eCmdHdlrInt, NULL, &iMarkMessagePeriod,
                              STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL,
                               STD_LOADABLE_MODULE_ID));
ENDmodInit
