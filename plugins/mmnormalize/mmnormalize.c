/* mmnormalize.c
 * This is a message modification module. It normalizes the input message with
 * the help of liblognorm. The messages EE event structure is updated.
 *
 * NOTE: read comments in module-template.h for details on the calling interface!
 *
 * File begun on 2010-01-01 by RGerhards
 *
 * Copyright 2010 Rainer Gerhards and Adiscon GmbH.
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
#include <libestr.h>
#include <libee/libee.h>
#include <liblognorm.h>
#include "conf.h"
#include "syslogd-types.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include "cfsysline.h"
#include "dirty.h"

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP

static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal);

/* static data */
DEFobjCurrIf(errmsg);

/* internal structures
 */
DEF_OMOD_STATIC_DATA

typedef struct _instanceData {
	sbool bUseRawMsg;	/**< use %rawmsg% instead of %msg% */
	ln_ctx ctxln;		/**< context to be used for liblognorm */
	ee_ctx ctxee;		/**< context to be used for libee */
} instanceData;

typedef struct configSettings_s {
	uchar *rulebase;		/**< name of normalization rulebase to use */
	sbool bUseRawMsg;	/**< use %rawmsg% instead of %msg% */
} configSettings_t;

SCOPING_SUPPORT; /* must be set AFTER configSettings_t is defined */

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
	ee_exitCtx(pData->ctxee);
	ln_exitCtx(pData->ctxln);
ENDfreeInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	dbgprintf("mmnormalize\n");
ENDdbgPrintInstInfo


BEGINtryResume
CODESTARTtryResume
ENDtryResume

BEGINdoAction
	msg_t *pMsg;
	es_str_t *str;
	uchar *buf;
	int len;
	int r;
CODESTARTdoAction
	pMsg = (msg_t*) ppString[0];
	/* note that we can performance-optimize the interface, but this also
	 * requires changes to the libraries. For now, we accept message
	 * duplication. -- rgerhards, 2010-12-01
	 */
	if(pData->bUseRawMsg) {
		getRawMsg(pMsg, &buf, &len);
	} else {
		buf = getMSG(pMsg);
		len = getMSGLen(pMsg);
	}
	str = es_newStrFromCStr((char*)buf, len);
	r = ln_normalize(pData->ctxln, str, &pMsg->event);
	if(r != 0) {
		DBGPRINTF("error %d during ln_normalize\n", r);
	}
	es_deleteStr(str);
	/***DEBUG***/ // TODO: remove after initial testing - 2010-12-01
			{
			char *cstr;
			ee_fmtEventToJSON(pMsg->event, &str);
			cstr = es_str2cstr(str, NULL);
			dbgprintf("mmnormalize generated: %s\n", cstr);
			free(cstr);
			es_deleteStr(str);
			}
	/***END DEBUG***/
ENDdoAction


BEGINparseSelectorAct
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	/* first check if this config line is actually for us */
	if(strncmp((char*) p, ":mmnormalize:", sizeof(":mmnormalize:") - 1)) {
		ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
	}

	if(cs.rulebase == NULL) {
		errmsg.LogError(0, RS_RET_NO_RULESET, "error: no normalization rulebase was specified, use "
				"$MMNormalizeSampleDB directive first!");
		ABORT_FINALIZE(RS_RET_NO_RULESET);
	}

	/* ok, if we reach this point, we have something for us */
	p += sizeof(":mmnormalize:") - 1; /* eat indicator sequence  (-1 because of '\0'!) */
	CHKiRet(createInstance(&pData));

	/* check if a non-standard template is to be applied */
	if(*(p-1) == ';')
		--p;
	/* we call the function below because we need to call it via our interface definition. However,
	 * the format specified (if any) is always ignored.
	 */
	CHKiRet(cflineParseTemplateName(&p, *ppOMSR, 0, OMSR_TPL_AS_MSG, (uchar*) "RSYSLOG_FileFormat"));

	/* finally build the instance */
	if((pData->ctxee = ee_initCtx()) == NULL) {
		errmsg.LogError(0, RS_RET_NO_RULESET, "error: could not initialize libee ctx, cannot "
				"activate action");
		ABORT_FINALIZE(RS_RET_ERR_LIBEE_INIT);
	}

	if((pData->ctxln = ln_initCtx()) == NULL) {
		errmsg.LogError(0, RS_RET_NO_RULESET, "error: could not initialize liblognorm ctx, cannot "
				"activate action");
		ee_exitCtx(pData->ctxee);
		ABORT_FINALIZE(RS_RET_ERR_LIBLOGNORM_INIT);
	}
	ln_setEECtx(pData->ctxln, pData->ctxee);
	if(ln_loadSamples(pData->ctxln, (char*) cs.rulebase) != 0) {
		errmsg.LogError(0, RS_RET_NO_RULESET, "error: normalization rulebase '%s' could not be loaded "
				"cannot activate action", cs.rulebase);
		ee_exitCtx(pData->ctxee);
		ln_exitCtx(pData->ctxln);
		ABORT_FINALIZE(RS_RET_ERR_LIBLOGNORM_SAMPDB_LOAD);
	}
	pData->bUseRawMsg = cs.bUseRawMsg;

	/* all config vars auto-reset! */
	cs.bUseRawMsg = 0;
	free(cs.rulebase);
	cs.rulebase = NULL;
CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINmodExit
CODESTARTmodExit
	objRelease(errmsg, CORE_COMPONENT);
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
ENDqueryEtryPt



/* Reset config variables for this module to default values.
 */
static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	DEFiRet;
	cs.rulebase = NULL;
	cs.bUseRawMsg = 0;
	RETiRet;
}

/* set the rulebase name */
static rsRetVal
setRuleBase(void __attribute__((unused)) *pVal, uchar *pszName)
{
	DEFiRet;
	cs.rulebase = pszName;
	pszName = NULL;
	RETiRet;
}

BEGINmodInit()
	rsRetVal localRet;
	rsRetVal (*pomsrGetSupportedTplOpts)(unsigned long *pOpts);
	unsigned long opts;
	int bMsgPassingSupported;
CODESTARTmodInit
SCOPINGmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION;
		/* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	/* check if the rsyslog core supports parameter passing code */
	bMsgPassingSupported = 0;
	localRet = pHostQueryEtryPt((uchar*)"OMSRgetSupportedTplOpts",
			&pomsrGetSupportedTplOpts);
	if(localRet == RS_RET_OK) {
		/* found entry point, so let's see if core supports msg passing */
		CHKiRet((*pomsrGetSupportedTplOpts)(&opts));
		if(opts & OMSR_TPL_AS_MSG)
			bMsgPassingSupported = 1;
	} else if(localRet != RS_RET_ENTRY_POINT_NOT_FOUND) {
		ABORT_FINALIZE(localRet); /* Something else went wrong, not acceptable */
	}
	
	if(!bMsgPassingSupported) {
		DBGPRINTF("mmnormalize: msg-passing is not supported by rsyslog core, "
			  "can not continue.\n");
		ABORT_FINALIZE(RS_RET_NO_MSG_PASSING);
	}

	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"mmnormalizerulebase", 0, eCmdHdlrGetWord,
				    setRuleBase, NULL, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"mmnormalizeuserawmsg", 0, eCmdHdlrInt,
				    NULL, &cs.bUseRawMsg, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler,
				    resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit

/* vi:set ai:
 */
