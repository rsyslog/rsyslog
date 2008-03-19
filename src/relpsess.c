/* This module implements the relp sess object.
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
#include <assert.h>
#include "relp.h"
#include "relpsess.h"
#include "relpframe.h"
#include "sendq.h"

/** Construct a RELP sess instance
 */
relpRetVal
relpSessConstruct(relpSess_t **ppThis, relpEngine_t *pEngine, relpSrv_t *pSrv)
{
	relpSess_t *pThis;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	RELPOBJ_assert(pEngine, Engine);
	RELPOBJ_assert(pSrv, Srv);

	if((pThis = calloc(1, sizeof(relpSess_t))) == NULL) {
		ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}

	RELP_CORE_CONSTRUCTOR(pThis, Sess);
	pThis->pEngine = pEngine;
	pThis->pSrv = pSrv;
	pThis->maxDataSize = RELP_DFLT_MAX_DATA_SIZE;
	CHKRet(relpSendqConstruct(&pThis->pSendq, pThis->pEngine));
	pthread_mutex_init(&pThis->mutSend, NULL);

	*ppThis = pThis;

finalize_it:
	if(iRet != RELP_RET_OK) {
		if(pThis != NULL) {
			relpSessDestruct(&pThis);
		}
	}

	LEAVE_RELPFUNC;
}


/** Destruct a RELP sess instance
 */
relpRetVal
relpSessDestruct(relpSess_t **ppThis)
{
	relpSess_t *pThis;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	pThis = *ppThis;
	RELPOBJ_assert(pThis, Sess);

	if(pThis->pSendq != NULL)
		relpSendqDestruct(&pThis->pSendq);
	if(pThis->pTcp != NULL)
		relpTcpDestruct(&pThis->pTcp);
	pthread_mutex_destroy(&pThis->mutSend);
	/* done with de-init work, now free object itself */
	free(pThis);
	*ppThis = NULL;

	LEAVE_RELPFUNC;
}


/* This accepts a new session, and, if all goes well, constructs a new
 * session object and adds it to the engine's list of sessions.
 * rgerhards, 2008-03-17
 */
relpRetVal
relpSessAcceptAndConstruct(relpSess_t **ppThis, relpSrv_t *pSrv, int sock)
{
	relpSess_t *pThis;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	RELPOBJ_assert(pSrv, Srv);
	assert(sock >= 0);

	CHKRet(relpSessConstruct(&pThis, pSrv->pEngine, pSrv));
	CHKRet(relpTcpAcceptConnReq(&pThis->pTcp, sock, pThis->pEngine));

	/* TODO: check hostname against ACL (callback?) */
	/* TODO: check against max# sessions */

	*ppThis = pThis;

finalize_it:
pSrv->pEngine->dbgprint("relp session accepted with state %d\n", iRet);
	if(iRet != RELP_RET_OK) {
		if(pThis != NULL)
			relpSessDestruct(&pThis);
	}

	LEAVE_RELPFUNC;
}


/* receive data from a socket
 * The following function is called when the relp engine has detected
 * that data is available on the socket. This function reads the available
 * data and submits it for processing.
 * rgerhads, 2008-03-17
 */
relpRetVal
relpSessRcvData(relpSess_t *pThis)
{
	relpOctet_t rcvBuf[RELP_RCV_BUF_SIZE];
	ssize_t lenBuf;
	int i;

	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);

	lenBuf = RELP_RCV_BUF_SIZE;
	CHKRet(relpTcpRcv(pThis->pTcp, rcvBuf, &lenBuf));

rcvBuf[lenBuf] = '\0';
pThis->pEngine->dbgprint("relp session read %d octets, buf '%s'\n", (int) lenBuf, rcvBuf);
	if(lenBuf == 0) {
		ABORT_FINALIZE(RELP_RET_SESSION_CLOSED);
	} else if (lenBuf == -1) {
		ABORT_FINALIZE(RELP_RET_SESSION_BROKEN);
	} else {
		/* we have regular data, which we now can process */
		for(i = 0 ; i < lenBuf ; ++i) {
			CHKRet(relpFrameProcessOctetRcvd(&pThis->pCurrRcvFrame, rcvBuf[i], pThis));
		}
	}

finalize_it:
pThis->pEngine->dbgprint("end relpSessRcvData, iRet %d\n", iRet);

	LEAVE_RELPFUNC;
}


#if 0 // maybe delete it...
/* Send a frame to the remote peer.
 * This function sends a frame, which must be completely constructed, to the remote
 * peer. It does not mangle with the frame, except that it adds the txnr, which is
 * only known during the send. The caller-provided frame is destructed when
 * this function returns (even in case of an error return).
 * rgerhards, 2008-03-18
 * TODO: do we still need this function? or can we delete it? rgerhards, 2008-03-19
 */
relpRetVal
relpSessSendFrame(relpSess_t *pThis, relpFrame_t *pFrame)
{
	relpOctet_t *pSendBuf = NULL;

	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);
	RELPOBJ_assert(pFrame, Frame);

	/* we must make sure that while we process the txnr, nobody else gets
	 * into our way.
	 */
	pthread_mutex_lock(&pThis->mutSend);
	CHKRet(relpFrameBuildMembufAndDestruct(pFrame, &pSendBuf, pThis->txnrSnd));
	pThis->txnrSnd = (pThis->txnrSnd + 1) / 1000000000; /* txnr used up, so on to next one (latching!) */
	pthread_mutex_unlock(&pThis->mutSend);

	// TODO: send buffer!

finalize_it:
	if(iRet != RELP_RET_OK) {
		if(pSendBuf != NULL)
			free(pSendBuf);
		/* the txnr is probably lost, but there is nothing we
		 * can do against that..
		 */
	}

	LEAVE_RELPFUNC;
}
#endif


/* Send a response to the remote peer.
 * rgerhards, 2008-03-19
 */
relpRetVal
relpSessSendResponse(relpSess_t *pThis, unsigned char *pData, size_t lenData)
{
	relpSendbuf_t *pSendbuf;

	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);

	/* we must make sure that while we process the txnr, nobody else gets
	 * into our way. Plaese note that the protection must be extened all the
	 * way down to enqueuing the sendbuf, because otherwise someone may get
	 * before us into the sendq, which would violate txnr requirements.
	 */
	/* TODO: cancel-safety! */
	pthread_mutex_lock(&pThis->mutSend);
	CHKRet(relpFrameBuildSendbuf(&pSendbuf, pThis->txnrSnd, (unsigned char*)"rsp", 3,
				     pData, lenData, pThis));
	pThis->txnrSnd = (pThis->txnrSnd + 1) % 1000000000; /* txnr used up, so on to next one (latching!) */
pThis->pEngine->dbgprint("SessSend new txnr %d\n", (int) pThis->txnrSnd);
	/* now enqueue it to the sendq (which means "send it" ;)) */
	CHKRet(relpSendqAddBuf(pThis->pSendq, pSendbuf));
	pthread_mutex_unlock(&pThis->mutSend);

finalize_it:
	if(iRet != RELP_RET_OK) {
		if(pSendbuf != NULL)
			relpSendbufDestruct(&pSendbuf);
		/* the txnr is probably lost, but there is nothing we
		 * can do against that..
		 */
	}

	LEAVE_RELPFUNC;
}


/* actually send to the remote peer
 * This function takes data from the sendq and sends as much as
 * possible to the remote peer.
 * rgerhards, 2008-03-19
 */
relpRetVal
relpSessSndData(relpSess_t *pThis)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);

	CHKRet(relpSendqSend(pThis->pSendq, pThis->pTcp));

finalize_it:
	LEAVE_RELPFUNC;
}
