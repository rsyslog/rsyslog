/* pmrciscoios.c
 * This is a parser module for CISCO IOS "syslog" format.
 *
 * File begun on 2014-07-07 by RGerhards
 *
 * Copyright 2014 Rainer Gerhards and Adiscon GmbH.
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
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include "syslogd.h"
#include "conf.h"
#include "syslogd-types.h"
#include "template.h"
#include "msg.h"
#include "module-template.h"
#include "glbl.h"
#include "errmsg.h"
#include "parser.h"
#include "datetime.h"
#include "unicode-helper.h"

MODULE_TYPE_PARSER
MODULE_TYPE_NOKEEP
PARSER_NAME("rsyslog.ciscoios")

/* internal structures */
DEF_PMOD_STATIC_DATA
DEFobjCurrIf(errmsg)
DEFobjCurrIf(glbl)
DEFobjCurrIf(parser)
DEFobjCurrIf(datetime)


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATUREAutomaticSanitazion)
		iRet = RS_RET_OK;
	if(eFeat == sFEATUREAutomaticPRIParsing)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINparse
	uchar *p2parse;
	long long msgcounter;
	int lenMsg;
	int i;	/* general index for parsing */
	int j;	/* index for target buffers */
	uchar bufParseTAG[512];
	uchar bufParseHOSTNAME[CONF_HOSTNAME_MAXSIZE]; /* used by origin */
CODESTARTparse
	DBGPRINTF("Message will now be parsed by pmciscoios\n");
	assert(pMsg != NULL);
	assert(pMsg->pszRawMsg != NULL);
	lenMsg = pMsg->iLenRawMsg - pMsg->offAfterPRI; /* note: offAfterPRI is already the number of PRI chars (do not add one!) */
	p2parse = pMsg->pszRawMsg + pMsg->offAfterPRI; /* point to start of text, after PRI */

	/* first obtain the message counter. It must be numeric up until
	 * the ": " terminator sequence
	 */
	msgcounter = 0;
	while(i < lenMsg && (p2parse[i] >= '0' && p2parse[i] <= '9') ) {
		msgcounter = msgcounter * 10 + p2parse[i] - '0';
		++i;
	}
	DBGPRINTF("pmciscoios: msgcntr %lld\n", msgcounter);

	/* delimiter check */
	if(i+1 >= lenMsg || p2parse[i] != ':' || p2parse[i] != ' ')
		ABORT_FINALIZE(RS_RET_COULD_NOT_PARSE);
	i += 2;


	/* now parse year */
	if(datetime.ParseTIMESTAMP3164(&(pMsg->tTIMESTAMP), &p2parse, &lenMsg) == RS_RET_OK) {
		if(pMsg->dfltTZ[0] != '\0')
			applyDfltTZ(&pMsg->tTIMESTAMP, pMsg->dfltTZ);
	} else {
		ABORT_FINALIZE(RS_RET_COULD_NOT_PARSE);
	}

	/* delimiter check */
	if(i+1 >= lenMsg || p2parse[i] != ':' || p2parse[i] != ' ')
		ABORT_FINALIZE(RS_RET_COULD_NOT_PARSE);
	i += 2;

	/* parse syslog tag. must always start with '%', else we have a field mismatch */
	if(i >= lenMsg || p2parse[i] != '%')
		ABORT_FINALIZE(RS_RET_COULD_NOT_PARSE);


	j = 0;
	while(i < lenMsg && *p2parse != ':' && *p2parse != ' ' && i < (int) sizeof(bufParseTAG) - 2) {
		bufParseTAG[j++] = p2parse[i];
	}

	/* delimiter check */
	if(i+1 >= lenMsg || p2parse[i] != ':' || p2parse[i] != ' ')
		ABORT_FINALIZE(RS_RET_COULD_NOT_PARSE);

	if(i < lenMsg && p2parse[i] == ':') {
		++i; 
		bufParseTAG[j++] = ':';
	}
	bufParseTAG[j] = '\0';	/* terminate string */

	/* if we reach this point, we have a wellformed message and can persist the values */
	MsgSetMSGoffs(pMsg, i); /* The unparsed rest is the actual MSG (for consistency, start it with SP) */
	setProtocolVersion(pMsg, MSG_LEGACY_PROTOCOL);
	MsgSetTAG(pMsg, bufParseTAG, i);
finalize_it:
ENDparse


BEGINmodExit
CODESTARTmodExit
	/* release what we no longer need */
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(glbl, CORE_COMPONENT);
	objRelease(parser, CORE_COMPONENT);
	objRelease(datetime, CORE_COMPONENT);
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_PMOD_QUERIES
CODEqueryEtryPt_IsCompatibleWithFeature_IF_OMOD_QUERIES
ENDqueryEtryPt


BEGINmodInit(pmrfc3164)
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(parser, CORE_COMPONENT));
	CHKiRet(objUse(datetime, CORE_COMPONENT));

	DBGPRINTF("pmciscoios parser init called\n");
ENDmodInit

/* vim:set ai:
 */
