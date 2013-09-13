/* omruleset.c
 * This is a very special output module. It permits to pass a message object
 * to another rule set. While this is a very simple action, it enables very
 * complex configurations, e.g. it supports high-speed "and" conditions, sending
 * data to the same file in a non-racy way, include functionality as well as
 * some high-performance optimizations (in case the rule sets have the necessary
 * queue definitions). So while this code is small, it is pretty important.
 *
 * NOTE: read comments in module-template.h for details on the calling interface!
 *
 * File begun on 2009-11-02 by RGerhards
 *
 * Copyright 2009 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Rsyslog is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rsyslog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Rsyslog.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
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
#include "conf.h"
#include "syslogd-types.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include "ruleset.h"
#include "cfsysline.h"
#include "dirty.h"

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("omruleset")

static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal);

/* static data */
DEFobjCurrIf(ruleset);
DEFobjCurrIf(errmsg);

/* internal structures
 */
DEF_OMOD_STATIC_DATA

/* config variables */


typedef struct _instanceData {
	ruleset_t *pRuleset;	/* ruleset to enqueue to */
	uchar *pszRulesetName;	/* primarily for debugging/display purposes */
} instanceData;

typedef struct configSettings_s {
	ruleset_t *pRuleset;	/* ruleset to enqueue message to (NULL = Default, not recommended) */
	uchar *pszRulesetName;
} configSettings_t;
static configSettings_t cs;

BEGINinitConfVars		/* (re)set config variables to default values */
CODESTARTinitConfVars 
	resetConfigVariables(NULL, NULL);
ENDinitConfVars


BEGINcreateInstance
CODESTARTcreateInstance
ENDcreateInstance


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
ENDisCompatibleWithFeature


BEGINfreeInstance
CODESTARTfreeInstance
	free(pData->pszRulesetName);
ENDfreeInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	dbgprintf("omruleset target %s[%p]\n", (char*) pData->pszRulesetName, pData->pRuleset);
ENDdbgPrintInstInfo


BEGINtryResume
CODESTARTtryResume
ENDtryResume

/* Note that we change the flow control type to "no delay", because at this point in 
 * rsyslog procesing we can not really slow down the producer any longer, as we already
 * work off a queue. So a delay would just block out execution for longer than needed.
 */
BEGINdoAction
	msg_t *pMsg;
CODESTARTdoAction
	CHKmalloc(pMsg = MsgDup((msg_t*) ppString[0]));
	DBGPRINTF(":omruleset: forwarding message %p to ruleset %s[%p]\n", pMsg,
		  (char*) pData->pszRulesetName, pData->pRuleset);
	MsgSetFlowControlType(pMsg, eFLOWCTL_NO_DELAY);
	MsgSetRuleset(pMsg, pData->pRuleset);
	/* Note: we intentionally use submitMsg2() here, as we process messages
	 * that were already run through the rate-limiter. So it is (at least)
	 * questionable if they were rate-limited again.
	 */
	submitMsg2(pMsg);
finalize_it:
ENDdoAction

/* set the ruleset name */
static rsRetVal
setRuleset(void __attribute__((unused)) *pVal, uchar *pszName)
{
	rsRetVal localRet;
	DEFiRet;

	localRet = ruleset.GetRuleset(ourConf, &cs.pRuleset, pszName);
	if(localRet == RS_RET_NOT_FOUND) {
		errmsg.LogError(0, RS_RET_RULESET_NOT_FOUND, "error: ruleset '%s' not found - ignored", pszName);
	}
	CHKiRet(localRet);
	cs.pszRulesetName = pszName; /* save for later display purposes */

finalize_it:
	if(iRet != RS_RET_OK) { /* cleanup needed? */
		free(pszName);
	}
	RETiRet;
}


BEGINparseSelectorAct
	int iTplOpts;
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	/* first check if this config line is actually for us */
	if(strncmp((char*) p, ":omruleset:", sizeof(":omruleset:") - 1)) {
		ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
	}

	if(cs.pRuleset == NULL) {
		errmsg.LogError(0, RS_RET_NO_RULESET, "error: no ruleset was specified, use "
				"$ActionOmrulesetRulesetName directive first!");
		ABORT_FINALIZE(RS_RET_NO_RULESET);
	}

	/* ok, if we reach this point, we have something for us */
	p += sizeof(":omruleset:") - 1; /* eat indicator sequence  (-1 because of '\0'!) */
	CHKiRet(createInstance(&pData));

	errmsg.LogError(0, RS_RET_DEPRECATED, "warning: omruleset is deprecated, consider "
			"using the 'call' statement instead");

	/* check if a non-standard template is to be applied */
	if(*(p-1) == ';')
		--p;
	iTplOpts = OMSR_TPL_AS_MSG;
	/* we call the message below because we need to call it via our interface definition. However,
	 * the format specified (if any) is always ignored.
	 */
	CHKiRet(cflineParseTemplateName(&p, *ppOMSR, 0, iTplOpts, (uchar*) "RSYSLOG_FileFormat"));
	pData->pRuleset = cs.pRuleset;
	pData->pszRulesetName = cs.pszRulesetName;
	cs.pRuleset = NULL; /* re-set, because there is a high risk of unwanted behavior if we leave it in! */
	cs.pszRulesetName = NULL; /* note: we must not free, as we handed over this pointer to the instanceDat to the instanceDataa! */
CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINmodExit
CODESTARTmodExit
	free(cs.pszRulesetName);
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(ruleset, CORE_COMPONENT);
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
CODEqueryEtryPt_STD_CONF2_CNFNAME_QUERIES 
ENDqueryEtryPt



/* Reset config variables for this module to default values.
 */
static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	DEFiRet;
	cs.pRuleset = NULL;
	free(cs.pszRulesetName);
	cs.pszRulesetName = NULL;
	RETiRet;
}


BEGINmodInit()
	rsRetVal localRet;
	rsRetVal (*pomsrGetSupportedTplOpts)(unsigned long *pOpts);
	unsigned long opts;
	int bMsgPassingSupported;		/* does core support template passing as an array? */
CODESTARTmodInit
INITLegCnfVars
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	/* check if the rsyslog core supports parameter passing code */
	bMsgPassingSupported = 0;
	localRet = pHostQueryEtryPt((uchar*)"OMSRgetSupportedTplOpts", &pomsrGetSupportedTplOpts);
	if(localRet == RS_RET_OK) {
		/* found entry point, so let's see if core supports msg passing */
		CHKiRet((*pomsrGetSupportedTplOpts)(&opts));
		if(opts & OMSR_TPL_AS_MSG)
			bMsgPassingSupported = 1;
	} else if(localRet != RS_RET_ENTRY_POINT_NOT_FOUND) {
		ABORT_FINALIZE(localRet); /* Something else went wrong, what is not acceptable */
	}
	
	if(!bMsgPassingSupported) {
		DBGPRINTF("omruleset: msg-passing is not supported by rsyslog core, can not continue.\n");
		ABORT_FINALIZE(RS_RET_NO_MSG_PASSING);
	}

	CHKiRet(objUse(ruleset, CORE_COMPONENT));
	CHKiRet(objUse(errmsg, CORE_COMPONENT));

	errmsg.LogError(0, RS_RET_DEPRECATED, "warning: omruleset is deprecated, consider "
			"using the 'call' statement instead");

	CHKiRet(omsdRegCFSLineHdlr((uchar *)"actionomrulesetrulesetname", 0, eCmdHdlrGetWord,
				    setRuleset, NULL, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler,
				    resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit

/* vi:set ai:
 */
