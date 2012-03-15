/* mmjsonparse.c
 * This is a message modification module. If give, it extracts JSON data
 * and populates the EE event structure with it.
 *
 * NOTE: read comments in module-template.h for details on the calling interface!
 *
 * File begun on 2012-02-20 by RGerhards
 *
 * Copyright 2012 Adiscon GmbH.
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
#include <ctype.h>
#include <libestr.h>
#include <libee/libee.h>
#include "conf.h"
#include "syslogd-types.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include "cfsysline.h"
#include "dirty.h"

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("mmjsonparse")

static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal);

/* static data */
DEFobjCurrIf(errmsg);

/* internal structures
 */
DEF_OMOD_STATIC_DATA

typedef struct _instanceData {
	ee_ctx ctxee;		/**< context to be used for libee */
} instanceData;

typedef struct configSettings_s {
	int dummy; /* remove when the first real parameter is needed */
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
	ee_exitCtx(pData->ctxee);
ENDfreeInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	dbgprintf("mmjsonparse\n");
ENDdbgPrintInstInfo


BEGINtryResume
CODESTARTtryResume
ENDtryResume

#define COOKIE "@cee: "
#define LEN_COOKIE (sizeof(COOKIE)-1)
BEGINdoAction
	msg_t *pMsg;
	uchar *buf;
	struct ee_event *event;
	int bSuccess = 0;
CODESTARTdoAction
	pMsg = (msg_t*) ppString[0];
	/* note that we can performance-optimize the interface, but this also
	 * requires changes to the libraries. For now, we accept message
	 * duplication. -- rgerhards, 2010-12-01
	 */
	buf = getMSG(pMsg);

dbgprintf("mmjsonparse: msg is '%s'\n", buf);
	while(*buf && isspace(*buf)) {
		++buf;
	}

	if(*buf == '\0' || strncmp((char*)buf, COOKIE, LEN_COOKIE)) {
		DBGPRINTF("mmjsonparse: no JSON cookie: '%s'\n", buf);
		FINALIZE;
	}
	buf += LEN_COOKIE;
dbgprintf("mmjsonparse: cookie found, rest of message: '%s'\n", buf);
	event = ee_newEventFromJSON(pData->ctxee, (char*)buf);
	if(event == NULL) {
		DBGPRINTF("mmjsonparse: JSON parse error, assuming no "
			  "JSON-enhanced message: '%s'\n", buf);
		FINALIZE;
	}
	/* TODO: in the long term, we need to think about merging & different
	   name spaces (probably best to add the newly-obtained event as a child to
	   the existing event...)
	*/
	if(pMsg->event != NULL) {
		ee_deleteEvent(pMsg->event);
	}
	pMsg->event = event;
	bSuccess = 1;

#if 1
	/***DEBUG***/ // TODO: remove after initial testing - 2010-12-01
			{
			char *cstr;
			es_str_t *str;
			ee_fmtEventToJSON(pMsg->event, &str);
			cstr = es_str2cstr(str, NULL);
			dbgprintf("mmjsonparse generated: %s\n", cstr);
			free(cstr);
			es_deleteStr(str);
			}
	/***END DEBUG***/
#endif
finalize_it:
	MsgSetParseSuccess(pMsg, bSuccess);
ENDdoAction


BEGINparseSelectorAct
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	/* first check if this config line is actually for us */
	if(strncmp((char*) p, ":mmjsonparse:", sizeof(":mmjsonparse:") - 1)) {
		ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
	}

	/* ok, if we reach this point, we have something for us */
	p += sizeof(":mmjsonparse:") - 1; /* eat indicator sequence  (-1 because of '\0'!) */
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
	RETiRet;
}


BEGINmodInit()
	rsRetVal localRet;
	rsRetVal (*pomsrGetSupportedTplOpts)(unsigned long *pOpts);
	unsigned long opts;
	int bMsgPassingSupported;
CODESTARTmodInit
INITLegCnfVars
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
		DBGPRINTF("mmjsonparse: msg-passing is not supported by rsyslog core, "
			  "can not continue.\n");
		ABORT_FINALIZE(RS_RET_NO_MSG_PASSING);
	}

	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler,
				    resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit

/* vi:set ai:
 */
