/* pmrfc3164.c
 * This is a parser module for RFC3164(legacy syslog)-formatted messages.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * File begun on 2009-11-04 by RGerhards
 *
 * Copyright 2007, 2009 Rainer Gerhards and Adiscon GmbH.
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
PARSER_NAME("rsyslog.rfc3164")

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
	int i;	/* general index for parsing */
	uchar bufParseTAG[CONF_TAG_MAXSIZE];
	uchar bufParseHOSTNAME[CONF_HOSTNAME_MAXSIZE];
CODESTARTparse
	DBGPRINTF("Message will now be parsed by the legacy syslog parser (one size fits all... ;)).\n");
	assert(pMsg != NULL);
	assert(pMsg->pszRawMsg != NULL);
	lenMsg = pMsg->iLenRawMsg - pMsg->offAfterPRI; /* note: offAfterPRI is already the number of PRI chars (do not add one!) */
	p2parse = pMsg->pszRawMsg + pMsg->offAfterPRI; /* point to start of text, after PRI */
	setProtocolVersion(pMsg, 0);

	/* Check to see if msg contains a timestamp. We start by assuming
	 * that the message timestamp is the time of reception (which we 
	 * generated ourselfs and then try to actually find one inside the
	 * message. There we go from high-to low precison and are done
	 * when we find a matching one. -- rgerhards, 2008-09-16
	 */
	if(datetime.ParseTIMESTAMP3339(&(pMsg->tTIMESTAMP), &p2parse, &lenMsg) == RS_RET_OK) {
		/* we are done - parse pointer is moved by ParseTIMESTAMP3339 */;
	} else if(datetime.ParseTIMESTAMP3164(&(pMsg->tTIMESTAMP), &p2parse, &lenMsg) == RS_RET_OK) {
		/* we are done - parse pointer is moved by ParseTIMESTAMP3164 */;
	} else if(*p2parse == ' ' && lenMsg > 1) { /* try to see if it is slighly malformed - HP procurve seems to do that sometimes */
		++p2parse;	/* move over space */
		--lenMsg;
		if(datetime.ParseTIMESTAMP3164(&(pMsg->tTIMESTAMP), &p2parse, &lenMsg) == RS_RET_OK) {
			/* indeed, we got it! */
			/* we are done - parse pointer is moved by ParseTIMESTAMP3164 */;
		} else {/* parse pointer needs to be restored, as we moved it off-by-one
			 * for this try.
			 */
			--p2parse;
			++lenMsg;
		}
	}

	if(pMsg->msgFlags & IGNDATE) {
		/* we need to ignore the msg data, so simply copy over reception date */
		memcpy(&pMsg->tTIMESTAMP, &pMsg->tRcvdAt, sizeof(struct syslogTime));
	}

	/* rgerhards, 2006-03-13: next, we parse the hostname and tag. But we 
	 * do this only when the user has not forbidden this. I now introduce some
	 * code that allows a user to configure rsyslogd to treat the rest of the
	 * message as MSG part completely. In this case, the hostname will be the
	 * machine that we received the message from and the tag will be empty. This
	 * is meant to be an interim solution, but for now it is in the code.
	 */
	if(bParseHOSTNAMEandTAG && !(pMsg->msgFlags & INTERNAL_MSG)) {
		/* parse HOSTNAME - but only if this is network-received!
		 * rger, 2005-11-14: we still have a problem with BSD messages. These messages
		 * do NOT include a host name. In most cases, this leads to the TAG to be treated
		 * as hostname and the first word of the message as the TAG. Clearly, this is not
		 * of advantage ;) I think I have now found a way to handle this situation: there
		 * are certain characters which are frequently used in TAG (e.g. ':'), which are
		 * *invalid* in host names. So while parsing the hostname, I check for these characters.
		 * If I find them, I set a simple flag but continue. After parsing, I check the flag.
		 * If it was set, then we most probably do not have a hostname but a TAG. Thus, I change
		 * the fields. I think this logic shall work with any type of syslog message.
		 * rgerhards, 2009-06-23: and I now have extended this logic to every character
		 * that is not a valid hostname.
		 */
		if(lenMsg > 0 && pMsg->msgFlags & PARSE_HOSTNAME) {
			i = 0;
			while(i < lenMsg && (isalnum(p2parse[i]) || p2parse[i] == '.'
				|| p2parse[i] == '_' || p2parse[i] == '-') && i < (CONF_HOSTNAME_MAXSIZE - 1)) {
				bufParseHOSTNAME[i] = p2parse[i];
				++i;
			}

			if(i == lenMsg) {
				/* we have a message that is empty immediately after the hostname,
				* but the hostname thus is valid! -- rgerhards, 2010-02-22
				*/
				p2parse += i;
				lenMsg -= i;
				bufParseHOSTNAME[i] = '\0';
				MsgSetHOSTNAME(pMsg, bufParseHOSTNAME, i);
			} else if(i > 0 && p2parse[i] == ' ' && isalnum(p2parse[i-1])) {
				/* we got a hostname! */
				p2parse += i + 1; /* "eat" it (including SP delimiter) */
				lenMsg -= i + 1;
				bufParseHOSTNAME[i] = '\0';
				MsgSetHOSTNAME(pMsg, bufParseHOSTNAME, i);
			}
		}

		/* now parse TAG - that should be present in message from all sources.
		 * This code is somewhat not compliant with RFC 3164. As of 3164,
		 * the TAG field is ended by any non-alphanumeric character. In
		 * practice, however, the TAG often contains dashes and other things,
		 * which would end the TAG. So it is not desirable. As such, we only
		 * accept colon and SP to be terminators. Even there is a slight difference:
		 * a colon is PART of the TAG, while a SP is NOT part of the tag
		 * (it is CONTENT). Starting 2008-04-04, we have removed the 32 character
		 * size limit (from RFC3164) on the tag. This had bad effects on existing
		 * envrionments, as sysklogd didn't obey it either (probably another bug
		 * in RFC3164...). We now receive the full size, but will modify the
		 * outputs so that only 32 characters max are used by default.
		 */
		i = 0;
		while(lenMsg > 0 && *p2parse != ':' && *p2parse != ' ' && i < CONF_TAG_MAXSIZE - 2) {
			bufParseTAG[i++] = *p2parse++;
			--lenMsg;
		}
		if(lenMsg > 0 && *p2parse == ':') {
			++p2parse; 
			--lenMsg;
			bufParseTAG[i++] = ':';
		}

		/* no TAG can only be detected if the message immediatly ends, in which case an empty TAG
		 * is considered OK. So we do not need to check for empty TAG. -- rgerhards, 2009-06-23
		 */
		bufParseTAG[i] = '\0';	/* terminate string */
		MsgSetTAG(pMsg, bufParseTAG, i);
	} else {/* we enter this code area when the user has instructed rsyslog NOT
		 * to parse HOSTNAME and TAG - rgerhards, 2006-03-13
		 */
		if(!(pMsg->msgFlags & INTERNAL_MSG)) {
			DBGPRINTF("HOSTNAME and TAG not parsed by user configuraton.\n");
		}
	}

	/* The rest is the actual MSG */
	MsgSetMSGoffs(pMsg, p2parse - pMsg->pszRawMsg);
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

	DBGPRINTF("rfc3164 parser init called\n");
 	bParseHOSTNAMEandTAG = glbl.GetParseHOSTNAMEandTAG(); /* cache value, is set only during rsyslogd option processing */


ENDmodInit

/* vim:set ai:
 */
