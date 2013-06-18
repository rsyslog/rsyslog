/* pmsnare.c
 *
 * this detects logs sent by Snare and cleans them up so that they can be processed by the normal parser
 *
 * there are two variations of this, if the client is set to 'syslog' mode it sends
 *
 * <pri>timestamp<sp>hostname<sp>tag<tab>otherstuff
 *
 * if the client is not set to syslog it sends
 *
 * hostname<tab>tag<tab>otherstuff
 *
 * ToDo, take advantage of items in the message itself to set more friendly information
 * where the normal parser will find it by re-writing more of the message
 *
 * Intereting information includes:
 *
 * in the case of windows snare messages:
 *   the system hostname is field 12
 *   the severity is field 3 (criticality ranging form 0 to 4)
 *   the source of the log is field 4 and may be able to be mapped to facility
 * 
 * 
 * created 2010-12-13 by David Lang based on pmlastmsg
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
MODULE_CNFNAME("pmsnare")
PARSER_NAME("rsyslog.snare")

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


BEGINparse
	uchar *p2parse;
	int lenMsg;
	int snaremessage;
	int tablength;

CODESTARTparse
	#define TabRepresentation "#011"
	tablength=sizeof(TabRepresentation);
	dbgprintf("Message will now be parsed by fix Snare parser.\n");
	assert(pMsg != NULL);
	assert(pMsg->pszRawMsg != NULL);

	/* check if this message is of the type we handle in this (very limited) parser

	 find out if we have a space separated or tab separated for the first item
	 if tab separated see if the second word is one of our expected tags
	  if so replace the tabs with spaces so that hostname and syslog tag are going to be parsed properly
	   optionally replace the hostname at the beginning of the message with one from later in the message
	  else, wrong message, abort
	 else, assume that we have a valid timestamp, move over to the syslog tag
	  if that is tab separated from the rest of the message and one of our expected tags
	   if so, replace the tab with a space so that it will be parsed properly
	    optionally replace the hostname at the beginning of the message withone from later in the message 

	*/
	snaremessage=0;
	lenMsg = pMsg->iLenRawMsg - pMsg->offAfterPRI; /* note: offAfterPRI is already the number of PRI chars (do not add one!) */
	p2parse = pMsg->pszRawMsg + pMsg->offAfterPRI; /* point to start of text, after PRI */
	dbgprintf("pmsnare: msg to look at: [%d]'%s'\n", lenMsg, p2parse);
	if((unsigned) lenMsg < 30) {
		/* too short, can not be "our" message */
		dbgprintf("msg too short!\n");
		ABORT_FINALIZE(RS_RET_COULD_NOT_PARSE);
	}

	while(lenMsg && *p2parse != ' ' && *p2parse != '\t' && *p2parse != '#') {
		--lenMsg;
		++p2parse;
	}
	dbgprintf("pmsnare: separator [%d]'%s'  msg after the first separator: [%d]'%s'\n", tablength,TabRepresentation,lenMsg, p2parse);
	if ((lenMsg > tablength) && (*p2parse == '\t' || strncasecmp((char*) p2parse, TabRepresentation , tablength-1) == 0)) {
	//if ((lenMsg > tablength) && (*p2parse == '\t' || *p2parse == '#')) {
	dbgprintf("pmsnare: tab separated message\n");
		if(strncasecmp((char*) (p2parse + tablength - 1), "MSWinEventLog", 13) == 0) {
			snaremessage=13; /* 0 means not a snare message, a number is how long the tag is */
		}
		if(strncasecmp((char*) (p2parse + tablength - 1), "LinuxKAudit", 11) == 0) {
			snaremessage=11; /* 0 means not a snare message, a number is how long the tag is */
		}
		if(snaremessage) {
			/* replace the tab with a space and if needed move the message portion up by the length of TabRepresentation -2 characters to overwrite the extra : */
			*p2parse = ' ';
			lenMsg -=(tablength-2);
			p2parse++;
			lenMsg--;
			memmove(p2parse, p2parse + (tablength-2), lenMsg);
			*(p2parse + lenMsg) = '\n';
			*(p2parse + lenMsg + 1)  = '\0';
			pMsg->iLenRawMsg -=(tablength-2);
			pMsg->iLenMSG -=(tablength-2);
			p2parse += snaremessage;
			lenMsg -= snaremessage;
			*p2parse = ' ';
			p2parse++;
			lenMsg--;
			lenMsg -=(tablength-2);
			memmove(p2parse, p2parse + (tablength-2), lenMsg);
			*(p2parse + lenMsg) = '\n';
			*(p2parse + lenMsg + 1)  = '\0';
			pMsg->iLenRawMsg -=(tablength-2);
			pMsg->iLenMSG -=(tablength-2);
			dbgprintf("found a Snare message with snare not set to send syslog messages\n");
		}
	} else {
		/* go back to the beginning of the message */
		lenMsg = pMsg->iLenRawMsg - pMsg->offAfterPRI; /* note: offAfterPRI is already the number of PRI chars (do not add one!) */
		p2parse = pMsg->pszRawMsg + pMsg->offAfterPRI; /* point to start of text, after PRI */
		/* skip over timestamp and space*/
		lenMsg -=17;
		p2parse +=17;
		/* skip over what should be the hostname */
		while(lenMsg && *p2parse != ' ') {
			--lenMsg;
			++p2parse;
		}
		if (lenMsg){
			--lenMsg;
			++p2parse;
		}
		dbgprintf("pmsnare: separator [%d]'%s'  msg after the timestamp and hostname: [%d]'%s'\n", tablength,TabRepresentation,lenMsg, p2parse);
		if(lenMsg > 13 && strncasecmp((char*) p2parse, "MSWinEventLog", 13) == 0) {
			snaremessage=13; /* 0 means not a snare message, a number is how long the tag is */
		}
		if(lenMsg > 11 && strncasecmp((char*) p2parse, "LinuxKAudit", 11) == 0) {
			snaremessage=11; /* 0 means not a snare message, a number is how long the tag is */
		}
		if(snaremessage) {
			p2parse += snaremessage;
			lenMsg -= snaremessage;
			*p2parse = ' ';
			p2parse++;
			lenMsg--;
			lenMsg -=(tablength-2);
			memmove(p2parse, p2parse + (tablength-2), lenMsg);
			*(p2parse + lenMsg) = '\n';
			*(p2parse + lenMsg + 1)  = '\0';
			pMsg->iLenRawMsg -=(tablength-2);
			pMsg->iLenMSG -=(tablength-2);
			dbgprintf("found a Snare message with snare set to send syslog messages\n");
		}
		
	}
	DBGPRINTF("pmsnare: new message: [%d]'%s'\n", lenMsg, pMsg->pszRawMsg + pMsg->offAfterPRI);
	ABORT_FINALIZE(RS_RET_COULD_NOT_PARSE);

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

	DBGPRINTF("snare parser init called, compiled with version %s\n", VERSION);
 	bParseHOSTNAMEandTAG = glbl.GetParseHOSTNAMEandTAG(); /* cache value, is set only during rsyslogd option processing */


ENDmodInit

/* vim:set ai:
 */
