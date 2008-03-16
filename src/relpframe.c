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
#include "relp.h"
#include "relpframe.h"

/** Construct a RELP frame instance
 */
relpRetVal
relpFrameConstruct(relpFrame_t **ppThis)
{
	relpFrame_t *pThis;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	if((pThis = calloc(1, sizeof(relpFrame_t))) == NULL) {
		ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}

	RELP_CORE_CONSTRUCTOR(pThis, Engine);
	/* all other initialization is done by calloc */

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

	/* TODO: check for pending operations -- rgerhards, 2008-03-16 */

	/* done with de-init work, now free engine object itself */
	free(pThis);
	*ppThis = NULL;

finalize_it:
	LEAVE_RELPFUNC;
}


/* Process a received octet.
 * This is a state machine. The transport driver needs to pass in octets received
 * one after the other. This function builds frames and submits them for processing
 * as need arises. Please note that the frame pointed to be ppThis may change during
 * processing.
 * rgerhards, 2008-03-16
 */
relpRetVal
relpProcessOctetRcvd(relpFrame_t **pThis, relpOctet_t c)
{
	relpFrame_t *pThis;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	pThis = *ppThis;
	RELPOBJ_assert(pThis, Frame);

	switch(pThis->rcvState) {
		case eRelpFrameRcvState_BEGIN_FRAME:
			if(!isdigit(c))
				ABORT_FINALIZE(RELP_RET_INVALID_FRAME);
			CHKRet(relpFrameConstruct(&pThis));
			pThis->rcvState = eRelpFrameRcvState_IN_TXNR;
			/* FALLTHROUGH */
		case eRelpFrameRcvState_IN_TXNR:
			if(isdigit(c)) {
				if(pThis->iRcv++ == 9) { /* more than max permitted nbr of digits? */
					ABORT_FINALIZE(RELP_RET_INVALID_FRAME);
				pThis->txnr = pThis->txnr * 10 + c - '0';
			} if(c == ' '){ /* field terminator */
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
				pThis->lenData = pThis->lenData * 10 + c - '0';
			} if(c == ' ') /* field terminator */
				/* we now can assign the buffer for our data */
				if((pThis->pData = malloc(pThis->lenData)) == NULL)
					ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
				pThis->rcvState = eRelpFrameRcvState_IN_DATA;
				pThis->iRcv = 0;
			} else { /* oh, oh, invalid char! */
				ABORT_FINALIZE(RELP_RET_INVALID_FRAME);
			break;
		case eRelpFrameRcvState_IN_DATA:
			if(pThis->iRcv < pThis->lenData) {
				pThis->pData[pThis->iRcv] = c;
				++pThis->iRcv;
			} else { /* end of data */
				pThis->rcvState = eRelpFrameRcvState_IN_TRAILER;
				pThis->iRcv = 0;
			}
			break;
		case eRelpFrameRcvState_IN_TRAILER:
			if(c != '\n')
				ABORT_FINALIZE(RELP_RET_INVALID_FRAME);
			/* TODO: submit frame to processor */
			pThis->rcvState = eRelpFrameRcvState_BEGIN_FRAME; /* next frame ;) */
			break;
		case eRelpFrameRcvState_FINISHED:
			assert(0); /* this shall never happen */
			break;
	}

	*ppThis = pThis;

finalize_it;
	LEAVE_RELPFUNC;
}

