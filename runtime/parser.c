/* parser.c
 * This module contains functions for message parsers. It still needs to be
 * converted into an object (and much extended).
 *
 * Module begun 2008-10-09 by Rainer Gerhards (based on previous code from syslogd.c)
 *
 * Copyright 2008 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
 *
 * The rsyslog runtime library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The rsyslog runtime library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the rsyslog runtime library.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 * A copy of the LGPL can be found in the file "COPYING.LESSER" in this distribution.
 */
#include "config.h"
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#ifdef USE_NETZIP
#include <zlib.h>
#endif

#include "rsyslog.h"
#include "dirty.h"
#include "msg.h"
#include "obj.h"
#include "datetime.h"
#include "errmsg.h"
#include "parser.h"
#include "unicode-helper.h"
#include "dirty.h"
#include "cfsysline.h"

/* some defines */
#define DEFUPRI		(LOG_USER|LOG_NOTICE)

/* definitions for objects we access */
DEFobjStaticHelpers
DEFobjCurrIf(glbl)
DEFobjCurrIf(errmsg)
DEFobjCurrIf(datetime)

/* static data */
static int bParseHOSTNAMEandTAG;	/* cache for the equally-named global param - performance enhancement */

/* config data */
static uchar cCCEscapeChar = '#';/* character to be used to start an escape sequence for control chars */
static int bEscapeCCOnRcv = 1; /* escape control characters on reception: 0 - no, 1 - yes */
static int bDropTrailingLF = 1; /* drop trailing LF's on reception? */

/* we create a small helper list of parsers so that we can obtain their
 * handles if the config needs one. This is also used to unload all modules on
 * shutdown.
 */
parserList_t *pParsLstRoot = NULL;


/* intialize (but NOT allocate) a parser list. Primarily meant as a hook
 * which can be used to extend the list in the future. So far, just sets
 * it to NULL.
 */
static rsRetVal
InitParserList(parserList_t **pListRoot)
{
	*pListRoot = NULL;
	return RS_RET_OK;
}


/* Add a parser to the list. We use a VERY simple and ineffcient algorithm,
 * but it is employed only for a few milliseconds during config processing. So
 * I prefer to keep it very simple and with simple data structures. Unfortunately,
 * we need to preserve the order, but I don't like to add a tail pointer as that
 * would require a container object. So I do the extra work to skip to the tail
 * when adding elements...
 * rgerhards, 2009-11-03
 */
static rsRetVal
AddParserToList(parserList_t **ppListRoot, parser_t *pParser)
{
	parserList_t *pThis;
	parserList_t *pTail;
	DEFiRet;

	CHKmalloc(pThis = MALLOC(sizeof(parserList_t)));
	pThis->pParser = pParser;
	pThis->pNext = NULL;

	if(*ppListRoot == NULL) {
		pThis->pNext = *ppListRoot;
		*ppListRoot = pThis;
	} else {
		/* find tail first */
		for(pTail = *ppListRoot ; pTail->pNext != NULL ; pTail = pTail->pNext)
			/* just search, do nothing else */;
		/* add at tail */
		pTail->pNext = pThis;
	}

finalize_it:
	RETiRet;
}


/* find a parser based on the provided name */
static rsRetVal
FindParser(parser_t **ppParser, uchar *pName)
{
	parserList_t *pThis;
	DEFiRet;
	
	for(pThis = pParsLstRoot ; pThis != NULL ; pThis = pThis->pNext) {
		if(ustrcmp(pThis->pParser->pName, pName) == 0) {
			*ppParser = pThis->pParser;
			FINALIZE;	/* found it, iRet still eq. OK! */
		}
	}

	iRet = RS_RET_PARSER_NOT_FOUND;

finalize_it:
	RETiRet;
}


/* --- END helper functions for parser list handling --- */


BEGINobjConstruct(parser) /* be sure to specify the object type also in END macro! */
ENDobjConstruct(parser)

/* ConstructionFinalizer. The most important chore is to add the parser object
 * to our global list of available parsers.
 * rgerhards, 2009-11-03
 */
rsRetVal parserConstructFinalize(parser_t *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, parser);
	CHKiRet(AddParserToList(&pParsLstRoot, pThis));
	DBGPRINTF("parser '%s' added to list of available parser\n", pThis->pName);

finalize_it:
	RETiRet;
}

BEGINobjDestruct(parser) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(parser)
	free(pThis->pName);
	//TODO: free module!
ENDobjDestruct(parser)

/***************************RFC 5425 PARSER ******************************************************/


/* Helper to parseRFCSyslogMsg. This function parses a field up to
 * (and including) the SP character after it. The field contents is
 * returned in a caller-provided buffer. The parsepointer is advanced
 * to after the terminating SP. The caller must ensure that the 
 * provided buffer is large enough to hold the to be extracted value.
 * Returns 0 if everything is fine or 1 if either the field is not
 * SP-terminated or any other error occurs. -- rger, 2005-11-24
 * The function now receives the size of the string and makes sure
 * that it does not process more than that. The *pLenStr counter is
 * updated on exit. -- rgerhards, 2009-09-23
 */
static int parseRFCField(uchar **pp2parse, uchar *pResult, int *pLenStr)
{
	uchar *p2parse;
	int iRet = 0;

	assert(pp2parse != NULL);
	assert(*pp2parse != NULL);
	assert(pResult != NULL);

	p2parse = *pp2parse;

	/* this is the actual parsing loop */
	while(*pLenStr > 0  && *p2parse != ' ') {
		*pResult++ = *p2parse++;
		--(*pLenStr);
	}

	if(*pLenStr > 0 && *p2parse == ' ') {
		++p2parse; /* eat SP, but only if not at end of string */
		--(*pLenStr);
	} else {
		iRet = 1; /* there MUST be an SP! */
	}
	*pResult = '\0';

	/* set the new parse pointer */
	*pp2parse = p2parse;
	return 0;
}


/* Helper to parseRFCSyslogMsg. This function parses the structured
 * data field of a message. It does NOT parse inside structured data,
 * just gets the field as whole. Parsing the single entities is left
 * to other functions. The parsepointer is advanced
 * to after the terminating SP. The caller must ensure that the 
 * provided buffer is large enough to hold the to be extracted value.
 * Returns 0 if everything is fine or 1 if either the field is not
 * SP-terminated or any other error occurs. -- rger, 2005-11-24
 * The function now receives the size of the string and makes sure
 * that it does not process more than that. The *pLenStr counter is
 * updated on exit. -- rgerhards, 2009-09-23
 */
static int parseRFCStructuredData(uchar **pp2parse, uchar *pResult, int *pLenStr)
{
	uchar *p2parse;
	int bCont = 1;
	int iRet = 0;
	int lenStr;

	assert(pp2parse != NULL);
	assert(*pp2parse != NULL);
	assert(pResult != NULL);

	p2parse = *pp2parse;
	lenStr = *pLenStr;

	/* this is the actual parsing loop
	 * Remeber: structured data starts with [ and includes any characters
	 * until the first ] followed by a SP. There may be spaces inside
	 * structured data. There may also be \] inside the structured data, which
	 * do NOT terminate an element.
	 */
	if(lenStr == 0 || *p2parse != '[')
		return 1; /* this is NOT structured data! */

	if(*p2parse == '-') { /* empty structured data? */
		*pResult++ = '-';
		++p2parse;
		--lenStr;
	} else {
		while(bCont) {
			if(lenStr < 2) {
				/* we now need to check if we have only structured data */
				if(lenStr > 0 && *p2parse == ']') {
					*pResult++ = *p2parse;
					p2parse++;
					lenStr--;
					bCont = 0;
				} else {
					iRet = 1; /* this is not valid! */
					bCont = 0;
				}
			} else if(*p2parse == '\\' && *(p2parse+1) == ']') {
				/* this is escaped, need to copy both */
				*pResult++ = *p2parse++;
				*pResult++ = *p2parse++;
				lenStr -= 2;
			} else if(*p2parse == ']' && *(p2parse+1) == ' ') {
				/* found end, just need to copy the ] and eat the SP */
				*pResult++ = *p2parse;
				p2parse += 2;
				lenStr -= 2;
				bCont = 0;
			} else {
				*pResult++ = *p2parse++;
				--lenStr;
			}
		}
	}

	if(lenStr > 0 && *p2parse == ' ') {
		++p2parse; /* eat SP, but only if not at end of string */
		--lenStr;
	} else {
		iRet = 1; /* there MUST be an SP! */
	}
	*pResult = '\0';

	/* set the new parse pointer */
	*pp2parse = p2parse;
	*pLenStr = lenStr;
	return 0;
}

/* parse a RFC5424-formatted syslog message. This function returns
 * 0 if processing of the message shall continue and 1 if something
 * went wrong and this messe should be ignored. This function has been
 * implemented in the effort to support syslog-protocol. Please note that
 * the name (parse *RFC*) stems from the hope that syslog-protocol will
 * some time become an RFC. Do not confuse this with informational
 * RFC 3164 (which is legacy syslog).
 *
 * currently supported format:
 *
 * <PRI>VERSION SP TIMESTAMP SP HOSTNAME SP APP-NAME SP PROCID SP MSGID SP [SD-ID]s SP MSG
 *
 * <PRI> is already stripped when this function is entered. VERSION already
 * has been confirmed to be "1", but has NOT been stripped from the message.
 *
 * rger, 2005-11-24
 */
static int parseRFCSyslogMsg(msg_t *pMsg, int flags)
{
	uchar *p2parse;
	uchar *pBuf;
	int lenMsg;
	int bContParse = 1;

	BEGINfunc
	assert(pMsg != NULL);
	assert(pMsg->pszRawMsg != NULL);
	p2parse = pMsg->pszRawMsg + pMsg->offAfterPRI; /* point to start of text, after PRI */
	lenMsg = pMsg->iLenRawMsg - pMsg->offAfterPRI;

	/* do a sanity check on the version and eat it (the caller checked this already) */
	assert(p2parse[0] == '1' && p2parse[1] == ' ');
	p2parse += 2;
	lenMsg -= 2;

	/* Now get us some memory we can use as a work buffer while parsing.
	 * We simply allocated a buffer sufficiently large to hold all of the
	 * message, so we can not run into any troubles. I think this is
	 * more wise then to use individual buffers.
	 */
	if((pBuf = MALLOC(sizeof(uchar) * (lenMsg + 1))) == NULL)
		return 1;
		
	/* IMPORTANT NOTE:
	 * Validation is not actually done below nor are any errors handled. I have
	 * NOT included this for the current proof of concept. However, it is strongly
	 * advisable to add it when this code actually goes into production.
	 * rgerhards, 2005-11-24
	 */

	/* TIMESTAMP */
	if(datetime.ParseTIMESTAMP3339(&(pMsg->tTIMESTAMP),  &p2parse, &lenMsg) == RS_RET_OK) {
		if(flags & IGNDATE) {
			/* we need to ignore the msg data, so simply copy over reception date */
			memcpy(&pMsg->tTIMESTAMP, &pMsg->tRcvdAt, sizeof(struct syslogTime));
		}
	} else {
		DBGPRINTF("no TIMESTAMP detected!\n");
		bContParse = 0;
	}

	/* HOSTNAME */
	if(bContParse) {
		parseRFCField(&p2parse, pBuf, &lenMsg);
		MsgSetHOSTNAME(pMsg, pBuf, ustrlen(pBuf));
	}

	/* APP-NAME */
	if(bContParse) {
		parseRFCField(&p2parse, pBuf, &lenMsg);
		MsgSetAPPNAME(pMsg, (char*)pBuf);
	}

	/* PROCID */
	if(bContParse) {
		parseRFCField(&p2parse, pBuf, &lenMsg);
		MsgSetPROCID(pMsg, (char*)pBuf);
	}

	/* MSGID */
	if(bContParse) {
		parseRFCField(&p2parse, pBuf, &lenMsg);
		MsgSetMSGID(pMsg, (char*)pBuf);
	}

	/* STRUCTURED-DATA */
	if(bContParse) {
		parseRFCStructuredData(&p2parse, pBuf, &lenMsg);
		MsgSetStructuredData(pMsg, (char*)pBuf);
	}

	/* MSG */
	MsgSetMSGoffs(pMsg, p2parse - pMsg->pszRawMsg);

	free(pBuf);
	ENDfunc
	return 0; /* all ok */
}


/*********************** END RFC5425 PARSER ******************************************/

/***************************RFC 5425 PARSER ******************************************************/


/* parse a legay-formatted syslog message. This function returns
 * 0 if processing of the message shall continue and 1 if something
 * went wrong and this messe should be ignored. This function has been
 * implemented in the effort to support syslog-protocol.
 * rger, 2005-11-24
 * As of 2006-01-10, I am removing the logic to continue parsing only
 * when a valid TIMESTAMP is detected. Validity of other fields already
 * is ignored. This is due to the fact that the parser has grown smarter
 * and is now more able to understand different dialects of the syslog
 * message format. I do not expect any bad side effects of this change,
 * but I thought I log it in this comment.
 * rgerhards, 2006-01-10
 */
static int parseLegacySyslogMsg(msg_t *pMsg, int flags)
{
	uchar *p2parse;
	int lenMsg;
	int bTAGCharDetected;
	int i;	/* general index for parsing */
	uchar bufParseTAG[CONF_TAG_MAXSIZE];
	uchar bufParseHOSTNAME[CONF_TAG_HOSTNAME];
	BEGINfunc

	assert(pMsg != NULL);
	assert(pMsg->pszRawMsg != NULL);
	lenMsg = pMsg->iLenRawMsg - (pMsg->offAfterPRI + 1);
	p2parse = pMsg->pszRawMsg + pMsg->offAfterPRI; /* point to start of text, after PRI */

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

	if(flags & IGNDATE) {
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
	if(bParseHOSTNAMEandTAG && !(flags & INTERNAL_MSG)) {
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
		bTAGCharDetected = 0;
		if(lenMsg > 0 && flags & PARSE_HOSTNAME) {
			i = 0;
			while(i < lenMsg && (isalnum(p2parse[i]) || p2parse[i] == '.' || p2parse[i] == '.'
				|| p2parse[i] == '_' || p2parse[i] == '-') && i < CONF_TAG_MAXSIZE) {
				bufParseHOSTNAME[i] = p2parse[i];
				++i;
			}

			if(i > 0 && p2parse[i] == ' ' && isalnum(p2parse[i-1])) {
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
		while(lenMsg > 0 && *p2parse != ':' && *p2parse != ' ' && i < CONF_TAG_MAXSIZE) {
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
		if(!(flags & INTERNAL_MSG)) {
			DBGPRINTF("HOSTNAME and TAG not parsed by user configuraton.\n");
		}
	}

	/* The rest is the actual MSG */
	MsgSetMSGoffs(pMsg, p2parse - pMsg->pszRawMsg);

	ENDfunc
	return 0; /* all ok */
}


/***************************END RFC 5425 PARSER ******************************************************/


/* uncompress a received message if it is compressed.
 * pMsg->pszRawMsg buffer is updated.
 * rgerhards, 2008-10-09
 */
static inline rsRetVal uncompressMessage(msg_t *pMsg)
{
	DEFiRet;
#	ifdef USE_NETZIP
	uchar *deflateBuf = NULL;
	uLongf iLenDefBuf;
	uchar *pszMsg;
	size_t lenMsg;
	
	assert(pMsg != NULL);
	pszMsg = pMsg->pszRawMsg;
	lenMsg = pMsg->iLenRawMsg;

	/* we first need to check if we have a compressed record. If so,
	 * we must decompress it.
	 */
	if(lenMsg > 0 && *pszMsg == 'z') { /* compressed data present? (do NOT change order if conditions!) */
		/* we have compressed data, so let's deflate it. We support a maximum
		 * message size of iMaxLine. If it is larger, an error message is logged
		 * and the message is dropped. We do NOT try to decompress larger messages
		 * as such might be used for denial of service. It might happen to later
		 * builds that such functionality be added as an optional, operator-configurable
		 * feature.
		 */
		int ret;
		iLenDefBuf = glbl.GetMaxLine();
		CHKmalloc(deflateBuf = MALLOC(sizeof(uchar) * (iLenDefBuf + 1)));
		ret = uncompress((uchar *) deflateBuf, &iLenDefBuf, (uchar *) pszMsg+1, lenMsg-1);
		DBGPRINTF("Compressed message uncompressed with status %d, length: new %ld, old %d.\n",
		        ret, (long) iLenDefBuf, (int) (lenMsg-1));
		/* Now check if the uncompression worked. If not, there is not much we can do. In
		 * that case, we log an error message but ignore the message itself. Storing the
		 * compressed text is dangerous, as it contains control characters. So we do
		 * not do this. If someone would like to have a copy, this code here could be
		 * modified to do a hex-dump of the buffer in question. We do not include
		 * this functionality right now.
		 * rgerhards, 2006-12-07
		 */
		if(ret != Z_OK) {
			errmsg.LogError(0, NO_ERRCODE, "Uncompression of a message failed with return code %d "
			            "- enable debug logging if you need further information. "
				    "Message ignored.", ret);
			FINALIZE; /* unconditional exit, nothing left to do... */
		}
		MsgSetRawMsg(pMsg, (char*)deflateBuf, iLenDefBuf);
	}
finalize_it:
	if(deflateBuf != NULL)
		free(deflateBuf);

#	else /* ifdef USE_NETZIP */

	/* in this case, we still need to check if the message is compressed. If so, we must
	 * tell the user we can not accept it.
	 */
	if(pMsg->iLenRawMsg > 0 && *pMsg->pszRawMsg == 'z') {
		errmsg.LogError(0, NO_ERRCODE, "Received a compressed message, but rsyslogd does not have compression "
		         "support enabled. The message will be ignored.");
		ABORT_FINALIZE(RS_RET_NO_ZIP);
	}	

finalize_it:
#	endif /* ifdef USE_NETZIP */

	RETiRet;
}


/* sanitize a received message
 * if a message gets to large during sanitization, it is truncated. This is
 * as specified in the upcoming syslog RFC series.
 * rgerhards, 2008-10-09
 * We check if we have a NUL character at the very end of the
 * message. This seems to be a frequent problem with a number of senders.
 * So I have now decided to drop these NULs. However, if they are intentional,
 * that may cause us some problems, e.g. with syslog-sign. On the other hand,
 * current code always has problems with intentional NULs (as it needs to escape
 * them to prevent problems with the C string libraries), so that does not
 * really matter. Just to be on the save side, we'll log destruction of such
 * NULs in the debug log.
 * rgerhards, 2007-09-14
 */
static inline rsRetVal
SanitizeMsg(msg_t *pMsg)
{
	DEFiRet;
	uchar *pszMsg;
	uchar *pDst; /* destination for copy job */
	size_t lenMsg;
	size_t iSrc;
	size_t iDst;
	size_t iMaxLine;
	size_t maxDest;
	bool bUpdatedLen = FALSE;
	uchar szSanBuf[32*1024]; /* buffer used for sanitizing a string */

	assert(pMsg != NULL);
	assert(pMsg->iLenRawMsg > 0);

#	ifdef USE_NETZIP
	CHKiRet(uncompressMessage(pMsg));
#	endif

	pszMsg = pMsg->pszRawMsg;
	lenMsg = pMsg->iLenRawMsg;

	/* remove NUL character at end of message (see comment in function header) */
	if(pszMsg[lenMsg-1] == '\0') {
		DBGPRINTF("dropped NUL at very end of message\n");
		bUpdatedLen = TRUE;
		lenMsg--;
	}

	/* then we check if we need to drop trailing LFs, which often make
	 * their way into syslog messages unintentionally. In order to remain
	 * compatible to recent IETF developments, we allow the user to
	 * turn on/off this handling.  rgerhards, 2007-07-23
	 */
	if(bDropTrailingLF && pszMsg[lenMsg-1] == '\n') {
		DBGPRINTF("dropped LF at very end of message (DropTrailingLF is set)\n");
		bUpdatedLen = TRUE;
		lenMsg--;
	}

	/* it is much quicker to sweep over the message and see if it actually
	 * needs sanitation than to do the sanitation in any case. So we first do
	 * this and terminate when it is not needed - which is expectedly the case
	 * for the vast majority of messages. -- rgerhards, 2009-06-15
	 */
	int bNeedSanitize = 0;
	for(iSrc = 0 ; iSrc < lenMsg ; iSrc++) {
		if(iscntrl(pszMsg[iSrc])) {
			if(pszMsg[iSrc] == '\0' || bEscapeCCOnRcv) {
				bNeedSanitize = 1;
				break;
			}
		}
	}

	if(!bNeedSanitize) {
		if(bUpdatedLen == TRUE)
			MsgSetRawMsgSize(pMsg, lenMsg);
		FINALIZE;
	}

	/* now copy over the message and sanitize it */
	iMaxLine = glbl.GetMaxLine();
	maxDest = lenMsg * 4; /* message can grow at most four-fold */
	if(maxDest > iMaxLine)
		maxDest = iMaxLine;	/* but not more than the max size! */
	if(maxDest < sizeof(szSanBuf))
		pDst = szSanBuf;
	else 
		CHKmalloc(pDst = MALLOC(sizeof(uchar) * (iMaxLine + 1)));
	iSrc = iDst = 0;
	while(iSrc < lenMsg && iDst < maxDest - 3) { /* leave some space if last char must be escaped */
		if(iscntrl((int) pszMsg[iSrc])) {
			/* note: \0 must always be escaped, the rest of the code currently
			 * can not handle it! -- rgerhards, 2009-08-26
			 */
			if(pszMsg[iSrc] == '\0' || bEscapeCCOnRcv) {
				/* we are configured to escape control characters. Please note
				 * that this most probably break non-western character sets like
				 * Japanese, Korean or Chinese. rgerhards, 2007-07-17
				 */
				pDst[iDst++] = cCCEscapeChar;
				pDst[iDst++] = '0' + ((pszMsg[iSrc] & 0300) >> 6);
				pDst[iDst++] = '0' + ((pszMsg[iSrc] & 0070) >> 3);
				pDst[iDst++] = '0' + ((pszMsg[iSrc] & 0007));
			}
		} else {
			pDst[iDst++] = pszMsg[iSrc];
		}
		++iSrc;
	}

	MsgSetRawMsg(pMsg, (char*)pDst, iDst); /* save sanitized string */

	if(pDst != szSanBuf)
		free(pDst);

finalize_it:
	RETiRet;
}


/* Parse a received message. The object's rawmsg property is taken and
 * parsed according to the relevant standards. This can later be
 * extended to support configured parsers.
 * rgerhards, 2008-10-09
 */
static rsRetVal
ParseMsg(msg_t *pMsg)
{
	DEFiRet;
	uchar *msg;
	int pri;
	int lenMsg;
	int iPriText;

	if(pMsg->iLenRawMsg == 0)
		ABORT_FINALIZE(RS_RET_EMPTY_MSG);

	CHKiRet(SanitizeMsg(pMsg));

	/* we needed to sanitize first, because we otherwise do not have a C-string we can print... */
	DBGPRINTF("msg parser: flags %x, from '%s', msg '%s'\n", pMsg->msgFlags, getRcvFrom(pMsg), pMsg->pszRawMsg);

	/* pull PRI */
	lenMsg = pMsg->iLenRawMsg;
	msg = pMsg->pszRawMsg;
	pri = DEFUPRI;
	iPriText = 0;
	if(*msg == '<') {
		/* while we process the PRI, we also fill the PRI textual representation
		 * inside the msg object. This may not be ideal from an OOP point of view,
		 * but it offers us performance...
		 */
		pri = 0;
		while(--lenMsg > 0 && isdigit((int) *++msg)) {
			pri = 10 * pri + (*msg - '0');
		}
		if(*msg == '>')
			++msg;
		if(pri & ~(LOG_FACMASK|LOG_PRIMASK))
			pri = DEFUPRI;
	}
	pMsg->iFacility = LOG_FAC(pri);
	pMsg->iSeverity = LOG_PRI(pri);
	MsgSetAfterPRIOffs(pMsg, msg - pMsg->pszRawMsg);

	/* rger 2005-11-24 (happy thanksgiving!): we now need to check if we have
	 * a traditional syslog message or one formatted according to syslog-protocol.
	 * We need to apply different parsers depending on that. We use the
	 * -protocol VERSION field for the detection.
	 */
	if(msg[0] == '1' && msg[1] == ' ') {
		dbgprintf("Message has syslog-protocol format.\n");
		setProtocolVersion(pMsg, 1);
		if(parseRFCSyslogMsg(pMsg, pMsg->msgFlags) == 1) {
			msgDestruct(&pMsg);
			ABORT_FINALIZE(RS_RET_ERR); // TODO: we need to handle these cases!
		}
	} else { /* we have legacy syslog */
		dbgprintf("Message has legacy syslog format.\n");
		setProtocolVersion(pMsg, 0);
		if(parseLegacySyslogMsg(pMsg, pMsg->msgFlags) == 1) {
			msgDestruct(&pMsg);
			ABORT_FINALIZE(RS_RET_ERR); // TODO: we need to handle these cases!
		}
	}

	/* finalize message object */
	pMsg->msgFlags &= ~NEEDS_PARSING; /* this message is now parsed */

finalize_it:
	RETiRet;
}

/* set the parser name - string is copied over, call can continue to use it,
 * but must free it if desired.
 */
static rsRetVal
SetName(parser_t *pThis, uchar *name)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, parser);
	assert(name != NULL);

	if(pThis->pName != NULL) {
		free(pThis->pName);
		pThis->pName = NULL;
	}

	CHKmalloc(pThis->pName = ustrdup(name));

finalize_it:
	RETiRet;
}


/* set a pointer to "our" module. Note that no module
 * pointer must already be set.
 */
static rsRetVal
SetModPtr(parser_t *pThis, modInfo_t *pMod)
{
	ISOBJ_TYPE_assert(pThis, parser);
	assert(pMod != NULL);
	assert(pThis->pModule == NULL);
	pThis->pModule = pMod;
	return RS_RET_OK;
}


/* Specify if we should do standard message sanitazion before we pass the data
 * down to the parser.
 */
static rsRetVal
SetDoSanitazion(parser_t *pThis, int bDoIt)
{
	ISOBJ_TYPE_assert(pThis, parser);
	pThis->bDoSanitazion = bDoIt;
	return RS_RET_OK;
}


/* queryInterface function-- rgerhards, 2009-11-03
 */
BEGINobjQueryInterface(parser)
CODESTARTobjQueryInterface(parser)
	if(pIf->ifVersion != parserCURR_IF_VERSION) { /* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->Construct = parserConstruct;
	pIf->ConstructFinalize = parserConstructFinalize;
	pIf->Destruct = parserDestruct;
	pIf->SetName = SetName;
	pIf->SetModPtr = SetModPtr;
	pIf->SetDoSanitazion = SetDoSanitazion;
	pIf->ParseMsg = ParseMsg;
	pIf->SanitizeMsg = SanitizeMsg;
	pIf->InitParserList = InitParserList;
	pIf->AddParserToList = AddParserToList;
	pIf->FindParser = FindParser;
finalize_it:
ENDobjQueryInterface(parser)



/* Reset config variables to default values.
 * rgerhards, 2007-07-17
 */
static rsRetVal
resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	cCCEscapeChar = '#';
	bEscapeCCOnRcv = 1; /* default is to escape control characters */
	bDropTrailingLF = 1; /* default is to drop trailing LF's on reception */

	return RS_RET_OK;
}


/* Initialize the parser class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2009-11-02
 */
BEGINObjClassInit(parser, 1, OBJ_IS_CORE_MODULE) /* class, version */
//BEGINAbstractObjClassInit(parser, 1, OBJ_IS_CORE_MODULE) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(datetime, CORE_COMPONENT));

	bParseHOSTNAMEandTAG = glbl.GetParseHOSTNAMEandTAG(); /* cache value, is set only during rsyslogd option processing */

	CHKiRet(regCfSysLineHdlr((uchar *)"controlcharacterescapeprefix", 0, eCmdHdlrGetChar, NULL, &cCCEscapeChar, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"droptrailinglfonreception", 0, eCmdHdlrBinary, NULL, &bDropTrailingLF, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"escapecontrolcharactersonreceive", 0, eCmdHdlrBinary, NULL, &bEscapeCCOnRcv, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL, NULL));

	InitParserList(&pParsLstRoot);
ENDObjClassInit(parser)

