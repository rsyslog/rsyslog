/* pmcisconames.c
 *
 * this detects logs sent by Cisco devices that mangle their syslog output when you tell them to log by name by adding ' :' between the name and the %XXX-X-XXXXXXX: tag
 * 
 * instead of actually parsing the message, this modifies the message and then falls through to allow a later parser to handle the now modified message
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
MODULE_CNFNAME("pmcisconames")
PARSER_NAME("rsyslog.cisconames")

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
#define OpeningText ": %"
CODESTARTparse
	dbgprintf("Message will now be parsed by fix Cisco Names parser.\n");
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
dbgprintf("pmcisconames: msg to look at: [%d]'%s'\n", lenMsg, p2parse);
	if((unsigned) lenMsg < 34) {
		/* too short, can not be "our" message */
                /* minimum message, 16 character timestamp, 1 character name, ' : %ASA-1-000000: '*/
dbgprintf("msg too short!\n");
		ABORT_FINALIZE(RS_RET_COULD_NOT_PARSE);
	}
	/* check if the timestamp is a 16 character or 21 character timestamp
		'Mmm DD HH:MM:SS ' spaces at 3,6,15 : at 9,12
		'Mmm DD YYYY HH:MM:SS ' spaces at 3,6,11,20 : at 14,17
	   check for the : first as that will differentiate the two conditions the fastest
	   this allows the compiler to short circuit the rst of the tests if it is the wrong timestamp
	   but still check the rest to see if it looks correct
	*/
	if ( *(p2parse + 9) == ':' && *(p2parse + 12) == ':' && *(p2parse + 3) == ' ' && *(p2parse + 6) == ' ' && *(p2parse + 15) == ' ') {
		/* skip over timestamp */
		dbgprintf("short timestamp found\n");
		lenMsg -=16;
		p2parse +=16;
	} else {
		if ( *(p2parse + 14) == ':' && *(p2parse + 17) == ':' && *(p2parse + 3) == ' ' && *(p2parse + 6) == ' ' && *(p2parse + 11) == ' ' && *(p2parse + 20) == ' ') {
			/* skip over timestamp */
			dbgprintf("long timestamp found\n");
			lenMsg -=21;
			p2parse +=21;
		} else {
			dbgprintf("timestamp is not one of the valid formats\n");
			ABORT_FINALIZE(RS_RET_COULD_NOT_PARSE);
		}
	}
	/* now look for the next space to walk past the hostname */
	while(lenMsg && *p2parse != ' ') {
		--lenMsg;
		++p2parse;
	}
	/* skip the space after the hostname */
	lenMsg -=1;
	p2parse +=1;
        /* if the syslog tag is : and the next thing starts with a % assume that this is a mangled cisco log and fix it */
	if(strncasecmp((char*) p2parse, OpeningText, sizeof(OpeningText)-1) != 0) {
		/* wrong opening text */
dbgprintf("not a cisco name mangled log!\n");
		ABORT_FINALIZE(RS_RET_COULD_NOT_PARSE);
	}
	/* bump the message portion up by two characters to overwrite the extra : */
	lenMsg -=2;
	memmove(p2parse, p2parse + 2, lenMsg);
	*(p2parse + lenMsg) = '\n';
	*(p2parse + lenMsg + 1)  = '\0';
	pMsg->iLenRawMsg -=2;
	pMsg->iLenMSG -=2;
	/* now, claim to abort so that something else can parse the now modified message */
	DBGPRINTF("pmcisconames: new mesage: [%d]'%s'\n", lenMsg, p2parse);
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

	DBGPRINTF("cisconames parser init called, compiled with version %s\n", VERSION);
 	bParseHOSTNAMEandTAG = glbl.GetParseHOSTNAMEandTAG(); /* cache value, is set only during rsyslogd option processing */


ENDmodInit

/* vim:set ai:
 */
