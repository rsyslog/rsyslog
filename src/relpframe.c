/* This module implements the relp frame object.
 *
 * Copyright 2008 by Rainer Gerhards and Adiscon GmbH.
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
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include "relp.h"
#include "relpframe.h"

/** Construct a RELP frame instance
 */
relpRetVal
relpFrameConstruct(relpFrame_t **ppThis, relpEngine_t *pEngine)
{
	relpFrame_t *pThis;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	if((pThis = calloc(1, sizeof(relpFrame_t))) == NULL) {
		ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}

	RELP_CORE_CONSTRUCTOR(pThis, Frame);
	pThis->pEngine = pEngine;

	*ppThis = pThis;

finalize_it:
	LEAVE_RELPFUNC;
}


/** Destruct a RELP frame instance
 */
relpRetVal
relpFrameDestruct(relpFrame_t **ppThis)
{
	relpFrame_t *pThis;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	pThis = *ppThis;
	RELPOBJ_assert(pThis, Frame);

	if(pThis->pData != NULL)
		free(pThis->pData);

	/* done with de-init work, now free object itself */
	free(pThis);
	*ppThis = NULL;

	LEAVE_RELPFUNC;
}


/* Process a received octet.
 * This is a state machine. The transport driver needs to pass in octets received
 * one after the other. This function builds frames and submits them for processing
 * as need arises. Please note that the frame pointed to be ppThis may change during
 * processing. It may be NULL after a frame has fully be processed. On Init, the 
 * caller can pass in a NULL pointer.
 * rgerhards, 2008-03-16
 */
relpRetVal
relpFrameProcessOctetRcvd(relpFrame_t **ppThis, relpOctet_t c, relpSess_t *pSess)
{
	relpFrame_t *pThis;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	pThis = *ppThis;

	/* we allow NULL pointers, as we would not like to have unprocessed frame. 
	 * Instead, a NULL frame pointer means that we have finished the previous frame
	 * (or did never before receive one) and so this character must be the first
	 * of a new frame. -- rgerhards, 2008-03-17
	 */
	if(pThis == NULL) {
		CHKRet(relpFrameConstruct(&pThis, pSess->pEngine));
		pThis->rcvState = eRelpFrameRcvState_BEGIN_FRAME;
	}

	switch(pThis->rcvState) {
		case eRelpFrameRcvState_BEGIN_FRAME:
			if(!isdigit(c))
				ABORT_FINALIZE(RELP_RET_INVALID_FRAME);
			pThis->rcvState = eRelpFrameRcvState_IN_TXNR;
			/* FALLTHROUGH */
		case eRelpFrameRcvState_IN_TXNR:
			if(isdigit(c)) {
				if(pThis->iRcv++ == 9) { /* more than max permitted nbr of digits? */
					ABORT_FINALIZE(RELP_RET_INVALID_FRAME);
				}
				pThis->txnr = pThis->txnr * 10 + c - '0';
			} else if(c == ' ') { /* field terminator */
				pThis->rcvState = eRelpFrameRcvState_IN_CMD;
				pThis->iRcv = 0;
			} else { /* oh, oh, invalid char! */
				ABORT_FINALIZE(RELP_RET_INVALID_FRAME);
			}
			break;
		case eRelpFrameRcvState_IN_CMD:
			if(isalpha(c)) {
				if(pThis->iRcv == 32) { /* more than max permitted nbr of digits? */
					ABORT_FINALIZE(RELP_RET_INVALID_FRAME);
				}
				pThis->cmd[pThis->iRcv] = c;
				++pThis->iRcv;
			} else if(c == ' ') { /* field terminator */
				pThis->cmd[pThis->iRcv] = '\0'; /* to make it suitable for strcmp() */
				pThis->rcvState = eRelpFrameRcvState_IN_DATALEN;
				pThis->iRcv = 0;
			} else { /* oh, oh, invalid char! */
				ABORT_FINALIZE(RELP_RET_INVALID_FRAME);
			}
			break;
		case eRelpFrameRcvState_IN_DATALEN:
			if(isdigit(c)) {
				if(pThis->iRcv++ == 9) { /* more than max permitted nbr of digits? */
					ABORT_FINALIZE(RELP_RET_INVALID_FRAME);
				}
				pThis->lenData = pThis->lenData * 10 + c - '0';
			} else {
				/* first check if we have no data, in which case
				 * we need to continue with the trailer check and skip
				 * the (non-present) data part. -- rgerhards, 2008-03-23
				 */
				if(pThis->lenData == 0) {
					pThis->rcvState = eRelpFrameRcvState_IN_TRAILER;
					pThis->iRcv = 0;
					goto do_trailer; /* not very clean, but... */
				}
				/* ok, we have data, so now let's do the usual checks... */
				if(c == ' ') { /* field terminator */
					/* we now can assign the buffer for our data */
					if(pThis->lenData > 0) {
						if((pThis->pData = malloc(pThis->lenData)) == NULL)
							ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
					}
					if(pThis->lenData > pSess->maxDataSize) {
						ABORT_FINALIZE(RELP_RET_DATA_TOO_LONG);
					}
					pThis->rcvState = eRelpFrameRcvState_IN_DATA;
					pThis->iRcv = 0;
				} else { /* oh, oh, invalid char! */
					ABORT_FINALIZE(RELP_RET_INVALID_FRAME);
				}
			}
			break;
		case eRelpFrameRcvState_IN_DATA:
			if(pThis->iRcv < pThis->lenData) {
				pThis->pData[pThis->iRcv] = c;
				++pThis->iRcv;
				break;
			} else { /* end of data */
				pThis->rcvState = eRelpFrameRcvState_IN_TRAILER;
				pThis->iRcv = 0;
				/*FALLTHROUGH*/
			}
		case eRelpFrameRcvState_IN_TRAILER:
		do_trailer:
			if(c != '\n')
				ABORT_FINALIZE(RELP_RET_INVALID_FRAME);
			pThis->rcvState = eRelpFrameRcvState_FINISHED;
			/* submit frame to processor */
			/* NOTE: do not abort in any case, because we need to destruct the frame! */
			if(pThis->txnr != 0 && pSess->sessType == eRelpSess_Server) {
				if(pThis->txnr != pSess->txnr)  {
					iRet = RELP_RET_INVALID_TXNR;
				} else {
					pSess->txnr = relpEngineNextTXNR(pSess->txnr);
				}
			}
			if(iRet == RELP_RET_OK) {
				iRet = relpEngineDispatchFrame(pSess->pEngine, pSess, pThis);
			}
			relpFrameDestruct(&pThis);
			break;
		case eRelpFrameRcvState_FINISHED:
			assert(0); /* this shall never happen */
			break;
	}

	*ppThis = pThis;

finalize_it:
//pSess->pEngine->dbgprint("end relp frame construct, iRet %d\n", iRet);
	LEAVE_RELPFUNC;
}


/* set the TXNR inside a relp frame
 * rgerhards, 2008-03-16
 */
relpRetVal
relpFrameSetTxnr(relpFrame_t *pThis, int txnr)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Frame);
	if(txnr < 0 || txnr > 999999999)
		ABORT_FINALIZE(RELP_RET_PARAM_ERROR);

	pThis->txnr = txnr;

finalize_it:
	LEAVE_RELPFUNC;
}


/* set the command inside a relp frame. The caller-provided buffer
 * is copied and NOT freed. This needs to be done by caller if
 * desired.
 * rgerhards, 2008-03-16
 */
relpRetVal
relpFrameSetCmd(relpFrame_t *pThis, relpOctet_t *pCmd)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Frame);
	if(pCmd == NULL || strlen((char*)pCmd) > 32)
		ABORT_FINALIZE(RELP_RET_PARAM_ERROR);

	strcpy((char*)pThis->cmd, (char*)pCmd);

finalize_it:
	LEAVE_RELPFUNC;
}


/* set the data part inside a relp frame. The caller-provided buffer
 * is taken over if "bHandoverBuffer" is set to 1. In this case, the caller
 * must no longer access (AND NOT FREE!) the buffer. If "bHandoverBuffer" is
 * set to 0, it is copied and the caller is responsible for freeing the buffer.
 * rgerhards, 2008-03-16
 */
relpRetVal
relpFrameSetData(relpFrame_t *pThis, relpOctet_t *pData, int lenData, int bHandoverBuffer)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Frame);
	if(pData == NULL && lenData != 0)
		ABORT_FINALIZE(RELP_RET_PARAM_ERROR);
	
	if(bHandoverBuffer || pData == NULL) {
		pThis->pData = pData;
	} else {
		/* we can not use the caller provided buffer and must copy it */
		if((pThis->pData = malloc(lenData)) == NULL)
			ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
		memcpy(pThis->pData, pData, lenData);
	}

	pThis->lenData = lenData;

finalize_it:
	LEAVE_RELPFUNC;
}


/* create a frame based on the provided command code and data. The frame is
 * meant to be consumed by sending it to the remote peer. A txnr is
 * NOT assigned, as this happens late in the process during the actual
 * send (only the send knows the right txnr to use). This actually is a
 * shortcut for calling the individual functions.
 * rgerhards, 2008-03-18
 */
relpRetVal
relpFrameConstructWithData(relpFrame_t **ppThis, relpEngine_t *pEngine, unsigned char *pCmd,
			   relpOctet_t *pData, size_t lenData, int bHandoverBuffer)
{
	relpFrame_t *pThis;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);

	CHKRet(relpFrameConstruct(&pThis, pEngine));
	CHKRet(relpFrameSetCmd(pThis, pCmd));
	CHKRet(relpFrameSetData(pThis, pData, lenData, bHandoverBuffer));

	*ppThis = pThis;

finalize_it:
	if(iRet != RELP_RET_OK) {
		if(pThis != NULL)
			relpFrameDestruct(&pThis);
	}

	LEAVE_RELPFUNC;
}


/* Update an already-existing Sendbuf with a new txnr. This function is needed
 * during recovery of failed connections where a queue of unsent messages
 * exists.
 * rgerhards, 2008-03-23
 */
relpRetVal
relpFrameRewriteTxnr(relpSendbuf_t *pSendbuf, relpTxnr_t txnr)
{
	char bufTxnr[16];
	size_t lenTxnr;
	relpOctet_t *ptrMembuf;

	ENTER_RELPFUNC;
	RELPOBJ_assert(pSendbuf, Sendbuf);
	
	pSendbuf->txnr = txnr;

	lenTxnr = snprintf(bufTxnr, sizeof(bufTxnr), "%d", (int) txnr);
	if(lenTxnr > 9)
		ABORT_FINALIZE(RELP_RET_INVALID_TXNR);

	/* we have always enough space inside the frame to patch in the new
	 * txnr - it was reserved during initial frame creation. So now patch it...
	 */
	ptrMembuf = pSendbuf->pData + 9 - lenTxnr; /* set ptr to start of area we intend to write to */
	pSendbuf->lenData = pSendbuf->lenData - pSendbuf->lenTxnr + lenTxnr;
	pSendbuf->lenTxnr = lenTxnr;
	memcpy(ptrMembuf, bufTxnr, lenTxnr); ptrMembuf += lenTxnr;

	/* the rest of the frame remains unchanged */

finalize_it:
	LEAVE_RELPFUNC;
}


/* create a relpSendbuf_t with a valid relp frame. While this is a frame object
 * function, it does not accept the frame object itself but rather the individual
 * frame parameters. The reason is that we want to save the overhead of constructing
 * and desructing a frame object in cases where the frame object itself is not
 * needed (e.g. when sending a "rsp" frame). This method
 * is meant to be invoked immediately before sending a frame. So the txnr must
 * be passed in, as it is at the top of the frame. Note that it should only be called
 * from within a mutex-protected region that ensures the txnr remains
 * consistent. The sendbuf is allocated inside this function
 * and a pointer to it passed to the caller. The caller is responsible
 * for destructing it. -- rgerhards, 2008-03-19
 */
relpRetVal
relpFrameBuildSendbuf(relpSendbuf_t **ppSendbuf, relpTxnr_t txnr, unsigned char *pCmd, size_t lenCmd,
		      relpOctet_t *pData, size_t lenData, relpSess_t *pSess,
		      relpRetVal (*rspHdlr)(relpSess_t*, relpFrame_t*))
{
	char bufTxnr[16];
	size_t lenTxnr;
	char bufDatalen[16];
	size_t lenDatalen;
	relpSendbuf_t *pSendbuf = NULL;
	relpOctet_t *ptrMembuf;

	ENTER_RELPFUNC;
	assert(ppSendbuf != NULL);
	
	CHKRet(relpSendbufConstruct(&pSendbuf, pSess));
	pSendbuf->txnr = txnr;
	pSendbuf->rspHdlr = rspHdlr;

	lenTxnr = snprintf(bufTxnr, sizeof(bufTxnr), "%d", (int) txnr);
	if(lenTxnr > 9)
		ABORT_FINALIZE(RELP_RET_INVALID_TXNR);

	lenDatalen = snprintf(bufDatalen, sizeof(bufDatalen), "%d", (int) lenData);
	if(lenDatalen > 9)
		ABORT_FINALIZE(RELP_RET_INVALID_DATALEN);
	
	/* we got everything, so now let's get our membuf [+1 for SP's and TRAILER]
	 * We do a trick in our calculation: we always assign 9 bytes for the txnr, even
	 * though it may be less than that. The reason is that on a sessin recovery case,
	 * we can put another, potentially longer, txnr into the frame. This saves us from
	 * copying the while frame just to adjust the txnr. -- rgerhards, 2008-03-23
	 */
	pSendbuf->lenData = lenTxnr + 1 + lenCmd + 1 + lenDatalen + 1;
	if(lenData > 0)
		pSendbuf->lenData += 1 + lenData;

if((pSendbuf->pData = malloc(pSendbuf->lenData + (9 - lenTxnr) + 1)) == NULL)
       // remove +1 above (for debugging only!)
		ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);

	ptrMembuf = pSendbuf->pData + 9 - lenTxnr; /* set ptr to start of area we intend to write to */
	pSendbuf->lenTxnr = lenTxnr;

	memcpy(ptrMembuf, bufTxnr, lenTxnr); ptrMembuf += lenTxnr;
	*ptrMembuf++ = ' ';
	memcpy(ptrMembuf, pCmd, lenCmd); ptrMembuf += lenCmd;
	*ptrMembuf++ = ' ';
	memcpy(ptrMembuf, bufDatalen, lenDatalen); ptrMembuf += lenDatalen;
	if(lenData > 0) {
		/* a data part (and its SP-delemiter) is only written if there is data... */
		*ptrMembuf++ = ' ';
		memcpy(ptrMembuf, pData, lenData); ptrMembuf += lenData;
	}

	*ptrMembuf = '\n';
*++ptrMembuf = '\0'; /* just for  the dbgprint below */

	*ppSendbuf = pSendbuf; /* save new buffer */

finalize_it:
	if(iRet != RELP_RET_OK) {
		if(pSendbuf != NULL)
			relpSendbufDestruct(&pSendbuf);
	}

	LEAVE_RELPFUNC;
}


/* return the next character from the framebuf or indicate end of buffer (-1).
 * rgerhards, 2008-03-24
 */
relpRetVal
relpFrameGetNextC(relpFrame_t *pThis, unsigned char *pC)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Frame);
	assert(pC != NULL);

	if(pThis->idxData >= pThis->lenData)
		iRet = RELP_RET_END_OF_DATA;
	else
		*pC = pThis->pData[pThis->idxData++];

	LEAVE_RELPFUNC;
}


#if 0 // rgerhards, 2008-03-19: I think we do no longer need it, but keep it a bit...
/* create a memory buffer with a representation of this frame. This method
 * is meant to be invoked immediately before sending a frame. So the txnr must
 * be passed in, as it is at the top of the frame. The frame object itself
 * is destructed by this function. Note that it should only be called
 * from within a mutex-protected region that ensures the txnr remains
 * consistent. So most probably the caller must be a function inside
 * the sendbuf obj. The memory buffer is allocated inside this function
 * and a pointer to it passed to the caller. The caller is responsible
 * for freeing it. -- rgerhards, 2008-03-18
 */
relpRetVal
relpFrameBuildMembufAndDestruct(relpFrame_t **ppThis, relpOctet_t **ppMembuf, relpTxnr_t txnr)
{
	relpFrame_t *pThis;
	char bufTxnr[16];
	size_t lenTxnr;
	char bufDatalen[16];
	size_t lenDatalen;
	size_t lenCmd;
	relpOctet_t *pMembuf = NULL;
	relpOctet_t *ptrMembuf;
	size_t lenMembuf;

	ENTER_RELPFUNC;
	assert(ppMembuf != NULL);
	assert(ppThis != NULL);
	pThis = *ppThis;
	RELPOBJ_assert(pThis, Frame);
	assert(txnr < 1000000000);
	
	lenTxnr = snprintf(bufTxnr, sizeof(bufTxnr), "%d", (int) txnr);
	if(lenTxnr > 9)
		ABORT_FINALIZE(RELP_RET_INVALID_TXNR);

	lenDatalen = snprintf(bufDatalen, sizeof(bufDatalen), "%d", (int) pThis->lenData);
	if(lenDatalen > 9)
		ABORT_FINALIZE(RELP_RET_INVALID_DATALEN);
	
	lenCmd = strlen((char*) pThis->cmd);

	/* we got everything, so now let's get our membuf */
	lenMembuf = lenTxnr + 1 + lenCmd + 1 +
		    lenDatalen + 1 +  pThis->lenData + 1; /* +1 for SP and TRAILER */
        if((pMembuf = malloc(lenMembuf)) == NULL)
		ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);

	ptrMembuf = pMembuf;
	memcpy(ptrMembuf, bufTxnr, lenTxnr); ptrMembuf += lenTxnr;
	*ptrMembuf++ = ' ';
	memcpy(ptrMembuf, pThis->cmd, lenCmd); ptrMembuf += lenCmd;
	*ptrMembuf++ = ' ';
	memcpy(ptrMembuf, bufDatalen, lenDatalen); ptrMembuf += lenDatalen;
	*ptrMembuf++ = ' ';
	memcpy(ptrMembuf, pThis->pData, pThis->lenData); ptrMembuf += pThis->lenData;
	*ptrMembuf = '\n';

	/* buffer completed, we no longer need the original frame */

	CHKRet(relpFrameDestruct(ppThis));

	*ppMembuf = pMembuf;

finalize_it:
	if(iRet != RELP_RET_OK) {
		if(pMembuf != NULL)
			free(pMembuf);
	}

	LEAVE_RELPFUNC;
}
#endif
