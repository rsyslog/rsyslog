/* ratelimit.c
 * support for rate-limiting sources, including "last message
 * repeated n times" processing.
 *
 * Copyright 2012 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
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
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "rsyslog.h"
#include "errmsg.h"
#include "ratelimit.h"
#include "datetime.h"
#include "parser.h"
#include "unicode-helper.h"
#include "msg.h"
#include "dirty.h"

/* definitions for objects we access */
DEFobjStaticHelpers
DEFobjCurrIf(errmsg)
DEFobjCurrIf(glbl)
DEFobjCurrIf(datetime)
DEFobjCurrIf(parser)

/* static data */

/* ratelimit a message, that means:
 * - handle "last message repeated n times" logic
 * - handle actual (discarding) rate-limiting
 * This function returns RS_RET_OK, if the caller shall process
 * the message regularly and RS_RET_DISCARD if the caller must
 * discard the message. The caller should also discard the message
 * if another return status occurs. This places some burden on the
 * caller logic, but provides best performance. Demanding this
 * cooperative mode can enable a faulty caller to thrash up part
 * of the system, but we accept that risk (a faulty caller can
 * always do all sorts of evil, so...)
 * Note that the message pointer may be updated upon return. In
 * this case, the ratelimiter is reponsible for handling the
 * original message.
 */
rsRetVal
ratelimitMsg(msg_t *pMsg, ratelimit_t *ratelimit)
{
	rsRetVal localRet;
	DEFiRet;

	if((pMsg->msgFlags & NEEDS_PARSING) != 0) {
		if((localRet = parser.ParseMsg(pMsg)) != RS_RET_OK)  {
			DBGPRINTF("Message discarded, parsing error %d\n", localRet);
			ABORT_FINALIZE(RS_RET_DISCARDMSG);
		}
	}


	/* suppress duplicate messages */
	if( ratelimit->pMsg != NULL &&
	    getMSGLen(pMsg) == getMSGLen(ratelimit->pMsg) &&
	    !ustrcmp(getMSG(pMsg), getMSG(ratelimit->pMsg)) &&
	    !strcmp(getHOSTNAME(pMsg), getHOSTNAME(ratelimit->pMsg)) &&
	    !strcmp(getPROCID(pMsg, LOCK_MUTEX), getPROCID(ratelimit->pMsg, LOCK_MUTEX)) &&
	    !strcmp(getAPPNAME(pMsg, LOCK_MUTEX), getAPPNAME(ratelimit->pMsg, LOCK_MUTEX))) {
		ratelimit->nsupp++;
		DBGPRINTF("msg repeated %d times\n", ratelimit->nsupp);
		/* use current message, so we have the new timestamp
		 * (means we need to discard previous one) */
		msgDestruct(&ratelimit->pMsg);
		ratelimit->pMsg = MsgAddRef(pMsg);
		ABORT_FINALIZE(RS_RET_DISCARDMSG);
	} else {/* new message, save it */
		/* first check if we have a previous message stored
		 * if so, emit and then discard it first
		 */
		if(ratelimit->pMsg != NULL) {
			if(ratelimit->nsupp > 0) {
				dbgprintf("DDDD: would need to emit 'repeat' message\n");
				if(ratelimit->repMsg != NULL) {
					dbgprintf("ratelimiter: call sequence error, have "
					  "previous repeat message - discarding\n");
					msgDestruct(&ratelimit->repMsg);
				}
				ratelimit->repMsg = ratelimit->pMsg;
				iRet = RS_RET_OK_HAVE_REPMSG;
			}
		}
		ratelimit->pMsg = MsgAddRef(pMsg);
	}

finalize_it:
dbgprintf("DDDD: in ratelimitMsg(): %d\n", iRet);
	RETiRet;
}

/* return the current repeat message or NULL, if none is present. This MUST
 * be called after ratelimitMsg() returned RS_RET_OK_HAVE_REPMSG. It is
 * important that the message returned by us is enqueued BEFORE the original
 * message, otherwise users my imply different order.
 * If a message object is returned, the caller must destruct it if no longer
 * needed.
 */
msg_t *
ratelimitGetRepeatMsg(ratelimit_t *ratelimit)
{
	msg_t *repMsg;
	size_t lenRepMsg;
	uchar szRepMsg[1024];
dbgprintf("DDDD: in ratelimitGetRepeatMsg()\n");

	/* we need to duplicate, original message may still be in use in other
	 * parts of the system!  */
	if((repMsg = MsgDup(ratelimit->repMsg)) == NULL) {
		DBGPRINTF("Message duplication failed, dropping repeat message.\n");
		goto done;
	}

	lenRepMsg = snprintf((char*)szRepMsg, sizeof(szRepMsg),
				" message repeated %d times: [%.800s]",
				ratelimit->nsupp, getMSG(ratelimit->repMsg));

	/* We now need to update the other message properties. Please note that digital
	 * signatures inside the message are invalidated.  */
	datetime.getCurrTime(&(repMsg->tRcvdAt), &(repMsg->ttGenTime));
	memcpy(&repMsg->tTIMESTAMP, &repMsg->tRcvdAt, sizeof(struct syslogTime));
	MsgReplaceMSG(repMsg, szRepMsg, lenRepMsg);

	if(ratelimit->repMsg != NULL) {
		ratelimit->repMsg = NULL;
		ratelimit->nsupp = 0;
	}
done:	return repMsg;
}


rsRetVal
ratelimitNew(ratelimit_t **ppThis)
{
	ratelimit_t *pThis;
	DEFiRet;

	CHKmalloc(pThis = calloc(1, sizeof(ratelimit_t)));
	*ppThis = pThis;
finalize_it:
	RETiRet;
}

void
ratelimitDestruct(ratelimit_t *ratelimit)
{
	msg_t *pMsg;
	if(ratelimit->pMsg != NULL) {
		if(ratelimit->nsupp > 0) {
			if(ratelimit->repMsg != NULL) {
				dbgprintf("ratelimiter/destuct: call sequence error, have "
				  "previous repeat message - discarding\n");
				msgDestruct(&ratelimit->repMsg);
			}
			ratelimit->repMsg = ratelimit->pMsg;
			pMsg = ratelimitGetRepeatMsg(ratelimit);
			if(pMsg != NULL)
				submitMsg(pMsg);
		}
	} else {
		msgDestruct(&ratelimit->pMsg);
	}
	free(ratelimit);
}

void
ratelimitModExit(void)
{
	objRelease(datetime, CORE_COMPONENT);
	objRelease(glbl, CORE_COMPONENT);
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(parser, CORE_COMPONENT);
}

rsRetVal
ratelimitModInit(void)
{
	DEFiRet;
	CHKiRet(objGetObjInterface(&obj));
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(datetime, CORE_COMPONENT));
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(parser, CORE_COMPONENT));
finalize_it:
	RETiRet;
}

