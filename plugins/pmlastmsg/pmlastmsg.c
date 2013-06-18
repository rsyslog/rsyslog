/* pmlastmsg.c
 * This is a parser module specifically for those horrible
 * "<PRI>last message repeated n times" messages notoriously generated
 * by some syslog implementations. Note that this parser should be placed
 * on top of the parser stack -- it takes out only these messages and
 * leaves all others for processing by the other parsers.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * File begun on 2010-07-13 by RGerhards
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
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
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
MODULE_CNFNAME("pmlastmsg")
PARSER_NAME("rsyslog.lastline")

/* internal structures
 */
DEF_PMOD_STATIC_DATA
DEFobjCurrIf(errmsg)
DEFobjCurrIf(glbl)
DEFobjCurrIf(parser)
DEFobjCurrIf(datetime)


/* static data */
static int bParseHOSTNAMEandTAG;	/* cache for the equally-named global param - performance enhancement */


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATUREAutomaticSanitazion)
		iRet = RS_RET_OK;
	if(eFeat == sFEATUREAutomaticPRIParsing)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature


/* parse a legay-formatted syslog message.
 */
BEGINparse
	uchar *p2parse;
	int lenMsg;
#define OpeningText "last message repeated "
#define ClosingText " times"
CODESTARTparse
	dbgprintf("Message will now be parsed by \"last message repated n times\" parser.\n");
	assert(pMsg != NULL);
	assert(pMsg->pszRawMsg != NULL);
	lenMsg = pMsg->iLenRawMsg - pMsg->offAfterPRI; /* note: offAfterPRI is already the number of PRI chars (do not add one!) */
	p2parse = pMsg->pszRawMsg + pMsg->offAfterPRI; /* point to start of text, after PRI */

	/* check if this message is of the type we handle in this (very limited) parser */
	/* first, we permit SP */
	while(lenMsg && *p2parse == ' ') {
		--lenMsg;
		++p2parse;
	}
dbgprintf("pmlastmsg: msg to look at: [%d]'%s'\n", lenMsg, p2parse);
	if((unsigned) lenMsg < sizeof(OpeningText)-1 + sizeof(ClosingText)-1 + 1) {
		/* too short, can not be "our" message */
dbgprintf("msg too short!\n");
		ABORT_FINALIZE(RS_RET_COULD_NOT_PARSE);
	}

	if(strncasecmp((char*) p2parse, OpeningText, sizeof(OpeningText)-1) != 0) {
		/* wrong opening text */
dbgprintf("wrong opening text!\n");
		ABORT_FINALIZE(RS_RET_COULD_NOT_PARSE);
	}
	lenMsg -= sizeof(OpeningText) - 1;
	p2parse += sizeof(OpeningText) - 1;

	/* now we need an integer --> digits */
	while(lenMsg && isdigit(*p2parse)) {
		--lenMsg;
		++p2parse;
	}

	if(lenMsg != sizeof(ClosingText)-1) {
		/* size must fit, else it is not "our" message... */
		ABORT_FINALIZE(RS_RET_COULD_NOT_PARSE);
	}

	if(strncasecmp((char*) p2parse, ClosingText, lenMsg) != 0) {
		/* wrong closing text */
dbgprintf("strcasecmp: %d\n", strncasecmp((char*) p2parse, ClosingText, lenMsg));
dbgprintf("pmlastmsg: closing msg to look at: [%d]'%s', (%s)\n", lenMsg, p2parse, ClosingText);
dbgprintf("wrong closing text!\n");
		ABORT_FINALIZE(RS_RET_COULD_NOT_PARSE);
	}

	/* OK, now we know we need to process this message, so we do that
	 * (and it is fairly simple in our case...)
	 */
	DBGPRINTF("pmlastmsg detected a \"last message repeated n times\" message\n");

	setProtocolVersion(pMsg, 0);
	memcpy(&pMsg->tTIMESTAMP, &pMsg->tRcvdAt, sizeof(struct syslogTime));
	MsgSetMSGoffs(pMsg, pMsg->offAfterPRI); /* we don't have a header! */
	MsgSetTAG(pMsg, (uchar*)"", 0);

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


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(parser, CORE_COMPONENT));
	CHKiRet(objUse(datetime, CORE_COMPONENT));

	dbgprintf("lastmsg parser init called, compiled with version %s\n", VERSION);
 	bParseHOSTNAMEandTAG = glbl.GetParseHOSTNAMEandTAG(); /* cache value, is set only during rsyslogd option processing */


ENDmodInit

/* vim:set ai:
 */
