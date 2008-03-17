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
#include <ctype.h>
#include <string.h>
#include <assert.h>
#define RELP_DO_INTERNAL_PROTOTYPES
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

	/* done with de-init work, now free object itself */
	free(pThis);
	*ppThis = NULL;

finalize_it:
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
relpFrameProcessOctetRcvd(relpFrame_t **ppThis, relpOctet_t c, relpEngine_t *pEngine)
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
		CHKRet(relpFrameConstruct(&pThis, pEngine));
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
			} else if(c == ' ') { /* field terminator */
				/* we now can assign the buffer for our data */
				if((pThis->pData = malloc(pThis->lenData)) == NULL)
					ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
				pThis->rcvState = eRelpFrameRcvState_IN_DATA;
				pThis->iRcv = 0;
			} else { /* oh, oh, invalid char! */
				ABORT_FINALIZE(RELP_RET_INVALID_FRAME);
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
			if(c != '\n')
				ABORT_FINALIZE(RELP_RET_INVALID_FRAME);
			pThis->rcvState = eRelpFrameRcvState_FINISHED;
			/* TODO: submit frame to processor */
			iRet = relpEngineDispatchFrame(pEngine, pThis);
			/* do not abort, because we need to destruct the frame */
			relpFrameDestruct(&pThis);
			break;
		case eRelpFrameRcvState_FINISHED:
			assert(0); /* this shall never happen */
			break;
	}

	*ppThis = pThis;

finalize_it:
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
	if(pCmd == NULL || strlen(pCmd) > 32)
		ABORT_FINALIZE(RELP_RET_PARAM_ERROR);

	strcpy(pThis->cmd, pCmd);

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
	if(pData == NULL)
		ABORT_FINALIZE(RELP_RET_PARAM_ERROR);
	
	if(bHandoverBuffer) {
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

