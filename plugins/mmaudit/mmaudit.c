/* mmaudit.c
 * This is a message modification module supporting Linux audit format
 * in various settings. The module tries to identify the provided
 * message as being a Linux audit record and, if so, converts it into
 * cee-enhanced syslog format.
 *
 * NOTE WELL:
 * Right now, we do not do any trust checks. So it is possible that a
 * malicous user emits something that looks like an audit record and
 * tries to fool the system with that. Solving this trust issue is NOT
 * an easy thing to do. This will be worked on, as the lumberjack effort
 * continues. Please consider the module in its current state as a proof
 * of concept.
 *
 * File begun on 2012-02-23 by RGerhards
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
MODULE_CNFNAME("mmaudit")

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
	dbgprintf("mmaudit\n");
ENDdbgPrintInstInfo


BEGINtryResume
CODESTARTtryResume
ENDtryResume


static inline void
skipWhitespace(uchar **buf)
{
	while(**buf && isspace(**buf))
		++(*buf);
}


static inline rsRetVal
parseName(uchar **buf, char *name, unsigned lenName)
{
	unsigned i;
	skipWhitespace(buf);
	--lenName; /* reserve space for '\0' */
	i = 0;
	while(**buf && **buf != '=' && lenName) {
//dbgprintf("parseNAme, buf: %s\n", *buf);
		name[i++] = **buf;
		++(*buf), --lenName;
	}
	name[i] = '\0';
	return RS_RET_OK;
}


static inline rsRetVal
parseValue(uchar **buf, char *val, unsigned lenval)
{
	char termc;
	unsigned i;
	DEFiRet;

	--lenval; /* reserve space for '\0' */
	i = 0;
	if(**buf == '\0') {
		FINALIZE;
	} else if(**buf == '\'') {
		termc = '\'';
		++(*buf);
	} else if(**buf == '"') {
		termc = '"';
		++(*buf);
	} else {
		termc = ' ';
	}

	while(**buf && **buf != termc && lenval) {
//dbgprintf("parseValue, termc '%c', buf: %s\n", termc, *buf);
		val[i++] = **buf;
		++(*buf), --lenval;
	}
	val[i] = '\0';

finalize_it:
	RETiRet;
}


/* parse the audit record and create libee structure
 */
static rsRetVal
audit_parse(instanceData *pData, uchar *buf, struct ee_event **event)
{
	es_str_t *estr;
	char name[1024];
	char val[1024];
	DEFiRet;

	*event = ee_newEvent(pData->ctxee);
	if(event == NULL) {
		ABORT_FINALIZE(RS_RET_ERR);
	}

	while(*buf) {
//dbgprintf("audit_parse, buf: '%s'\n", buf);
		CHKiRet(parseName(&buf, name, sizeof(name)));
		if(*buf != '=') {
			ABORT_FINALIZE(RS_RET_ERR);
		}
		++buf;
		CHKiRet(parseValue(&buf, val, sizeof(val)));

		estr = es_newStrFromCStr(val, strlen(val));
		ee_addStrFieldToEvent(*event, name, estr);
		es_deleteStr(estr);
dbgprintf("mmaudit: parsed %s=%s\n", name, val);
	}
	

finalize_it:
	RETiRet;
}


BEGINdoAction
	msg_t *pMsg;
	uchar *buf;
	int typeID;
	struct ee_event *event;
	int i;
	es_str_t *estr;
	char auditID[1024];
CODESTARTdoAction
	pMsg = (msg_t*) ppString[0];
	/* note that we can performance-optimize the interface, but this also
	 * requires changes to the libraries. For now, we accept message
	 * duplication. -- rgerhards, 2010-12-01
	 */
	buf = getMSG(pMsg);

dbgprintf("mmaudit: msg is '%s'\n", buf);
	while(*buf && isspace(*buf)) {
		++buf;
	}

	if(*buf == '\0' || strncmp((char*)buf, "type=", 5)) {
		DBGPRINTF("mmaudit: type= undetected: '%s'\n", buf);
		FINALIZE;
	}
	buf += 5;

	typeID = 0;
	while(*buf && isdigit(*buf)) {
		typeID = typeID * 10 + *buf - '0';
		++buf;
	}

	if(*buf == '\0' || strncmp((char*)buf, " audit(", sizeof(" audit(")-1)) {
		DBGPRINTF("mmaudit: audit( header not found: %s'\n", buf);
		FINALIZE;
	}
	buf += sizeof(" audit(");

	for(i = 0 ; i < (int) (sizeof(auditID)-2) && *buf && *buf != ')' ; ++i) {
		auditID[i] = *buf++;
	}
	auditID[i] = '\0';
	if(*buf != ')' || *(buf+1) != ':') {
		DBGPRINTF("mmaudit: trailer '):' not found, no audit record: %s'\n", buf);
		FINALIZE;
	}
	buf += 2;

dbgprintf("mmaudit: cookie found, type %d, auditID '%s', rest of message: '%s'\n", typeID, auditID, buf);
	audit_parse(pData, buf, &event);
	if(event == NULL) {
		DBGPRINTF("mmaudit: audit parse error, assuming no "
			  "audit message: '%s'\n", buf);
		FINALIZE;
	}

	/* we now need to shuffle the "outer" properties into that stream */
	estr = es_newStrFromCStr(auditID, strlen(auditID));
	ee_addStrFieldToEvent(event, "audithdr.auditid", estr);
	es_deleteStr(estr);

	/* we abuse auditID a bit to save space...  (TODO: change!) */
	snprintf(auditID, sizeof(auditID), "%d", typeID);
	estr = es_newStrFromCStr(auditID, strlen(auditID));
	ee_addStrFieldToEvent(event, "audithdr.type", estr);
	es_deleteStr(estr);

	/* TODO: in the long term, we need to think about merging & different
	   name spaces (probably best to add the newly-obtained event as a child to
	   the existing event...)
	*/
	if(pMsg->event != NULL) {
		ee_deleteEvent(pMsg->event);
	}
	pMsg->event = event;

#if 1
	/***DEBUG***/ // TODO: remove after initial testing - 2010-12-01
			{
			char *cstr;
			es_str_t *str;
			ee_fmtEventToJSON(pMsg->event, &str);
			cstr = es_str2cstr(str, NULL);
			dbgprintf("mmaudit generated: %s\n", cstr);
			free(cstr);
			es_deleteStr(str);
			}
	/***END DEBUG***/
#endif
finalize_it:
ENDdoAction


BEGINparseSelectorAct
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	/* first check if this config line is actually for us */
	if(strncmp((char*) p, ":mmaudit:", sizeof(":mmaudit:") - 1)) {
		ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
	}

	/* ok, if we reach this point, we have something for us */
	p += sizeof(":mmaudit:") - 1; /* eat indicator sequence  (-1 because of '\0'!) */
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
		DBGPRINTF("mmaudit: msg-passing is not supported by rsyslog core, "
			  "can not continue.\n");
		ABORT_FINALIZE(RS_RET_NO_MSG_PASSING);
	}

	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler,
				    resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit

/* vi:set ai:
 */
