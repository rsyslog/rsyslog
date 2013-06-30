/* The relp send buffer object.
 *
 * Copyright 2008-2013 by Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of librelp.
 *
 * Librelp is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Librelp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Librelp.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 *
 * If the terms of the GPL are unsuitable for your needs, you may obtain
 * a commercial license from Adiscon. Contact sales@adiscon.com for further
 * details.
 *
 * ALL CONTRIBUTORS PLEASE NOTE that by sending contributions, you assign
 * your copyright to Adiscon GmbH, Germany. This is necessary to permit the
 * dual-licensing set forth here. Our apologies for this inconvenience, but
 * we sincerely believe that the dual-licensing model helps us provide great
 * free software while at the same time obtaining some funding for further
 * development.
 */
#include "config.h"
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <time.h>
#include "relp.h"
#include "sendbuf.h"


/** Construct a RELP sendbuf instance
 * This is the first thing that a caller must do before calling any
 * RELP function. The relp sendbuf must only destructed after all RELP
 * operations have been finished.
 */
relpRetVal
relpSendbufConstruct(relpSendbuf_t **ppThis, relpSess_t *pSess)
{
	relpSendbuf_t *pThis;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	if((pThis = calloc(1, sizeof(relpSendbuf_t))) == NULL) {
		ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}

	RELP_CORE_CONSTRUCTOR(pThis, Sendbuf);
	pThis->pSess = pSess;

	*ppThis = pThis;

finalize_it:
	LEAVE_RELPFUNC;
}


/** Destruct a RELP sendbuf instance
 */
relpRetVal
relpSendbufDestruct(relpSendbuf_t **ppThis)
{
	relpSendbuf_t *pThis;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	pThis = *ppThis;
	RELPOBJ_assert(pThis, Sendbuf);
pThis->pSess->pEngine->dbgprint("in destructor: sendbuf %p\n", pThis);

	if(pThis->pData != NULL)
		free(pThis->pData);

	/* done with de-init work, now free sendbuf object itself */
	free(pThis);
	*ppThis = NULL;

	LEAVE_RELPFUNC;
}


/* set send buffer contents. The provided data pointer is handed *over* to
 * the sendbuf, so the caller can no longer access it. Most importantly, the
 * caller MUST NOT free the buffer!
 * rgerhards, 2008-03-17
 */
relpRetVal
relpSendbufSetData(relpSendbuf_t *pThis, relpOctet_t *pData, size_t lenData)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Engine);
	assert(pData != NULL);
	assert(lenData > 0);

	pThis->pData = pData;
	pThis->lenData = lenData;
	pThis->bufPtr = 0;

	LEAVE_RELPFUNC;
}


/* Sends as much data from the send buffer as possible.
 * This function tries to send as much data from the send buffer
 * as possible. For partial writes, the sendbuffer is updated to
 * contain the correct "already written" count.
 * rgerhards, 2008-03-19
 */
relpRetVal
relpSendbufSend(relpSendbuf_t *pThis, relpTcp_t *pTcp)
{
	ssize_t lenToWrite;
	ssize_t lenWritten;
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sendbuf);
	RELPOBJ_assert(pTcp,  Tcp);

	lenToWrite = pThis->lenData - pThis->bufPtr;
	lenWritten = lenToWrite;
//pTcp->pEngine->dbgprint("sendbuf len %d, still to write %d\n", (int) pThis->lenData, (int) lenToWrite);

	CHKRet(relpTcpSend(pTcp, pThis->pData + (9 - pThis->lenTxnr) + pThis->bufPtr, &lenWritten));

	if(lenWritten != lenToWrite) {
		pThis->bufPtr += lenWritten;
		iRet = RELP_RET_PARTIAL_WRITE;
	}

finalize_it:
	LEAVE_RELPFUNC;
}


/* a portable way to put the current thread asleep. Note that
 * using the sleep() API family may result in the whole process
 * to be put asleep on some platforms.
 */
static inline void
doSleep(int iSeconds, int iuSeconds)
{
	struct timeval tvSelectTimeout;
	tvSelectTimeout.tv_sec = iSeconds;
	tvSelectTimeout.tv_usec = iuSeconds; /* micro seconds */
	select(0, NULL, NULL, NULL, &tvSelectTimeout);
}

/* This functions sends a complete sendbuf (a blocking call). It
 * is intended for use by clients. Do NOT use it on servers as
 * that will block other activity. bAddToUnacked specifies if the
 * sendbuf should be linked to the unacked list (if 1). If it is 0
 * this shall NOT happen. Mode 0 is used for session reestablishment,
 * when the unacked list needs to be retransmitted.
 * rgerhards, 2008-03-19
 * The timeout handler may be one second off. This currently is good
 * enough for our needs, but we may revisit this if we make larger
 * changes to the lib. -- rgerhards, 2013-04-10
 */
relpRetVal
relpSendbufSendAll(relpSendbuf_t *pThis, relpSess_t *pSess, int bAddToUnacked)
{
	ssize_t lenToWrite;
	ssize_t lenWritten;
	struct timespec tCurr; /* absolute timeout value */
	struct timespec tTimeout; /* absolute timeout value */
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sendbuf);
	RELPOBJ_assert(pSess,  Sess);

	clock_gettime(CLOCK_REALTIME, &tTimeout);
	tTimeout.tv_sec += pSess->timeout;
	lenToWrite = pThis->lenData - pThis->bufPtr;
	/* we compute the absolute timeout, as we may need to use it multiple
	 * times in the loop below. Using the relative value would potentially
	 * prolong it for quite some time!
	 */
	while(lenToWrite != 0) {
		lenWritten = lenToWrite;
		CHKRet(relpTcpSend(pSess->pTcp, pThis->pData + (9 - pThis->lenTxnr) + pThis->bufPtr, &lenWritten));
		if(lenWritten == -1) {
			ABORT_FINALIZE(RELP_RET_IO_ERR);
		} else if(lenWritten == 0) {
			pSess->pEngine->dbgprint("relpSendbufSendAll() wrote 0 octets, waiting...\n");
			if(relpTcpWaitWriteable(pSess->pTcp, &tTimeout) == 0) {
				ABORT_FINALIZE(RELP_RET_IO_ERR); /* timed out! */
			}
		} else if(lenWritten == lenToWrite) {
			lenToWrite = 0;
		} else {
			pThis->bufPtr += lenWritten;
			lenToWrite = pThis->lenData - pThis->bufPtr;
		}
		if(lenToWrite != 0) {
			clock_gettime(CLOCK_REALTIME, &tCurr);
			if(   (tCurr.tv_sec > tTimeout.tv_sec)
			   || (tCurr.tv_sec == tTimeout.tv_sec && tCurr.tv_nsec >= tTimeout.tv_nsec)) {
				ABORT_FINALIZE(RELP_RET_IO_ERR);
			}
		}
	}

	/* ok, we now have sent the full buf. So we now need to add it to the unacked list, so that
	 * we know what to do when the "rsp" packet comes in. -- rgerhards, 2008-03-20
	 */
	if(bAddToUnacked) {
		if((iRet = relpSessAddUnacked(pSess, pThis)) != RELP_RET_OK) {
			relpSendbufDestruct(&pThis);
			FINALIZE;
		}
pSess->pEngine->dbgprint("sendbuf added to unacked list\n");
#if 0
{
	relpSessUnacked_t *pUnackedEtry;
	pUnackedEtry = pThis->pUnackedLstRoot;
	if(pUnackedEtry != NULL) {
pThis->pEngine->dbgprint("resending frame '%s'\n", pUnackedEtry->pSendbuf->pData + 9 - pUnackedEtry->pSendbuf->lenTxnr);
		CHKRet(relpFrameRewriteTxnr(pUnackedEtry->pSendbuf, pThis->txnr));
		pThis->txnr = relpEngineNextTXNR(pThis->txnr);
		CHKRet(relpSendbufSendAll(pUnackedEtry->pSendbuf, pThis, 0));
		pUnackedEtry = pUnackedEtry->pNext;
	}
}
#endif
	}
else pSess->pEngine->dbgprint("sendbuf NOT added to unacked list\n");

finalize_it:
	LEAVE_RELPFUNC;
}
