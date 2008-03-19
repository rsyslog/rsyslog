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
#include <sys/select.h>
#include <errno.h>
#include "relp.h"
#include "relpsess.h"
#include "relpframe.h"
#include "sendq.h"

/** Construct a RELP sess instance
 *  the pSrv parameter may be set to NULL if the session object is for a client.
 */
relpRetVal
relpSessConstruct(relpSess_t **ppThis, relpEngine_t *pEngine, relpSrv_t *pSrv)
{
	relpSess_t *pThis;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	RELPOBJ_assert(pEngine, Engine);

	if((pThis = calloc(1, sizeof(relpSess_t))) == NULL) {
		ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}

	RELP_CORE_CONSTRUCTOR(pThis, Sess);
	pThis->pEngine = pEngine;
	pThis->pSrv = pSrv;
	pThis->txnr = 1; /* txnr start at 1 according to spec */
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
 * rgerhards, 2008-03-17
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
		if(errno != EAGAIN)
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


/* Send a response to the client.
 * rgerhards, 2008-03-19
 */
relpRetVal
relpSessSendResponse(relpSess_t *pThis, relpTxnr_t txnr, unsigned char *pData, size_t lenData)
{
	relpSendbuf_t *pSendbuf;

	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);

	CHKRet(relpFrameBuildSendbuf(&pSendbuf, txnr, (unsigned char*)"rsp", 3,
				     pData, lenData, pThis));
	//pThis->txnr = relpEngineNextTXNR(pThis->txnr); /* txnr used up, so on to next one (latching!) */
pThis->pEngine->dbgprint("SessSend for txnr %d\n", (int) txnr);
	/* now enqueue it to the sendq (which means "send it" ;)) */
	CHKRet(relpSendqAddBuf(pThis->pSendq, pSendbuf));

finalize_it:
	if(iRet != RELP_RET_OK) {
		if(pSendbuf != NULL)
			relpSendbufDestruct(&pSendbuf);
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


/* ------------------------------ client based code ------------------------------ */


/* Wait for a response to a specifc TXNR. Blocks until it is received or a timeout
 * occurs. This is for client-side processing.
 * rgerhards, 2008-03-19
 */
static relpRetVal
relpSessWaitRsp(relpSess_t *pThis, relpTxnr_t txnr)
{
	fd_set readfds;
	int sock;
	int nfds;

	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);

	sock = relpSessGetSock(pThis);
	FD_ZERO(&readfds);
	FD_SET(sock, &readfds);
pThis->pEngine->dbgprint("relpSessWaitRsp waiting for data on fd %d\n", sock);
	nfds = select(sock+1, (fd_set *) &readfds, NULL, NULL, NULL);
pThis->pEngine->dbgprint("relpSessWaitRsp select returns, nfds %d\n", nfds);

finalize_it:
	LEAVE_RELPFUNC;
}


/* Connect to the server. All session parameters (like remote address) must
 * already have been set.
 * rgerhards, 2008-03-19
 */
relpRetVal
relpSessConnect(relpSess_t *pThis, int protFamily, unsigned char *port, unsigned char *host)
{

	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);

	CHKRet(relpTcpConstruct(&pThis->pTcp, pThis->pEngine));
	CHKRet(relpTcpConnect(pThis->pTcp, protFamily, port, host));

	CHKRet(relpSessSendCommand(pThis, (unsigned char*)"init", 4, (unsigned char*)"relp_version=1", 14));
	CHKRet(relpSessWaitRsp(pThis, 1)); /* "init" always has txnr 1 */

	CHKRet(relpSessSendCommand(pThis, (unsigned char*)"go", 2, (unsigned char*)"relp_version=1", 14));
	CHKRet(relpSessWaitRsp(pThis, 2)); /* and "go" always has txnr 2 */
	
	/* we are more or less finished, but need to wait for the (positive)
	 * response on initial session setup.
	 */
	CHKRet(relpSessRcvData(pThis));

finalize_it:
	LEAVE_RELPFUNC;
}


/* Send a command to the server.
 * In client-mode, we run on a single thread. As such, we need to pull any currently
 * existing server responses before we try to send our command.
 * We do not need mutex protection as no other thread shares this session.
 * rgerhards, 2008-03-19
 */
relpRetVal
relpSessSendCommand(relpSess_t *pThis, unsigned char *pCmd, size_t lenCmd,
		    unsigned char *pData, size_t lenData)
{
	relpSendbuf_t *pSendbuf;

	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);

	/* first read any outstanding data and process the packets. This is important
	 * because there may be responses which we need in order to get a send spot
	 * within our window.
	 */
	CHKRet(relpSessRcvData(pThis));

	/* then send our data */
	CHKRet(relpFrameBuildSendbuf(&pSendbuf, pThis->txnr, pCmd, lenCmd, pData, lenData, pThis));
	pThis->txnr = relpEngineNextTXNR(pThis->txnr); /* txnr used up, so on to next one (latching!) */
pThis->pEngine->dbgprint("send command with txnr %d\n", (int) pThis->txnr);
	/* now send it */
pThis->pEngine->dbgprint("frame to send: '%s'\n", pSendbuf->pData);
	CHKRet(relpSendbufSendAll(pSendbuf, pThis->pTcp));

finalize_it:
	if(iRet != RELP_RET_OK) {
		if(pSendbuf != NULL)
			relpSendbufDestruct(&pSendbuf);
	}

	LEAVE_RELPFUNC;
}


