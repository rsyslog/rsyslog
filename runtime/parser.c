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
#include "errmsg.h"

/* some defines */
#define DEFUPRI		(LOG_USER|LOG_NOTICE)

/* definitions for objects we access */
DEFobjStaticHelpers
DEFobjCurrIf(glbl)
DEFobjCurrIf(errmsg)

/* static data */


/* this is a dummy class init
 */
rsRetVal parserClassInit(void)
{
	DEFiRet;

	/* request objects we use */
	CHKiRet(objGetObjInterface(&obj)); /* this provides the root pointer for all other queries */
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
// TODO: free components! see action.c
finalize_it:
	RETiRet;
}


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
		CHKmalloc(deflateBuf = malloc(sizeof(uchar) * (iLenDefBuf + 1)));
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
sanitizeMessage(msg_t *pMsg)
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

	/* remove NUL character at end of message (see comment in function header)
	 * Note that we do not need to add a NUL character in this case, because it
	 * is already present ;)
	 */
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
		lenMsg--;
		pszMsg[lenMsg] = '\0';
		bUpdatedLen = TRUE;
	}

	/* it is much quicker to sweep over the message and see if it actually
	 * needs sanitation than to do the sanitation in any case. So we first do
	 * this and terminate when it is not needed - which is expectedly the case
	 * for the vast majority of messages. -- rgerhards, 2009-06-15
	 */
	int bNeedSanitize = 0;
	for(iSrc = 0 ; iSrc < lenMsg ; iSrc++) {
		if(iscntrl(pszMsg[iSrc])) {
			if(bSpaceLFOnRcv && pszMsg[iSrc] == '\n')
				pszMsg[iSrc] = ' ';
			else
			if(pszMsg[iSrc] == '\0' || bEscapeCCOnRcv) {
				bNeedSanitize = 1;
				if (!bSpaceLFOnRcv)
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
		CHKmalloc(pDst = malloc(sizeof(uchar) * (iMaxLine + 1)));
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
	pDst[iDst] = '\0';

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
rsRetVal parseMsg(msg_t *pMsg)
{
	DEFiRet;
	uchar *msg;
	int pri;
	int lenMsg;

	if(pMsg->iLenRawMsg == 0)
		ABORT_FINALIZE(RS_RET_EMPTY_MSG);

	CHKiRet(sanitizeMessage(pMsg));

	/* we needed to sanitize first, because we otherwise do not have a C-string we can print... */
	DBGPRINTF("msg parser: flags %x, from '%s', msg '%s'\n", pMsg->msgFlags, getRcvFrom(pMsg), pMsg->pszRawMsg);

	/* pull PRI */
	lenMsg = pMsg->iLenRawMsg;
	msg = pMsg->pszRawMsg;
	pri = DEFUPRI;
	if(pMsg->msgFlags & NO_PRI_IN_RAW) {
		/* In this case, simply do so as if the pri would be right at top */
		MsgSetAfterPRIOffs(pMsg, 0);
	} else {
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
	}

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
