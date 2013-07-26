/* This module implements the relp sess object.
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
#include <ctype.h>
#include <assert.h>
#include <sys/select.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "relp.h"
#include "relpsess.h"
#include "relpframe.h"
#include "sendq.h"
#include "offers.h"
#include "dbllinklist.h"

/* forward definitions */
static relpRetVal relpSessCltDoDisconnect(relpSess_t *pThis);
static relpRetVal relpSessFixCmdStates(relpSess_t *pThis);
static relpRetVal relpSessSrvDoDisconnect(relpSess_t *pThis);

/* helper to free permittedPeer structure */
static inline void
relpSessFreePermittedPeers(relpSess_t *pThis)
{
	int i;
	for(i = 0 ; i < pThis->permittedPeers.nmemb ; ++i)
		free(pThis->permittedPeers.name[i]);
	pThis->permittedPeers.nmemb = 0;
}


/** Construct a RELP sess instance
 *  the pSrv parameter may be set to NULL if the session object is for a client.
 */
relpRetVal
relpSessConstruct(relpSess_t **ppThis, relpEngine_t *pEngine, int connType, void *pParent)
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
	/* use Engine's command enablement states as default */
	pThis->stateCmdSyslog = pEngine->stateCmdSyslog;
	if(connType == RELP_SRV_CONN) {
		pThis->pSrv = (relpSrv_t*) pParent;
	} else {
		pThis->pClt = (relpClt_t*) pParent;
	}
	pThis->txnr = 1; /* txnr start at 1 according to spec */
	pThis->timeout = 90;
	pThis->pUsr = NULL;
	pThis->sizeWindow = RELP_DFLT_WINDOW_SIZE;
	pThis->maxDataSize = RELP_DFLT_MAX_DATA_SIZE;
	pThis->authmode = eRelpAuthMode_None;
	pThis->pristring = NULL;
	pThis->caCertFile = NULL;
	pThis->ownCertFile = NULL;
	pThis->privKeyFile = NULL;
	pThis->permittedPeers.nmemb = 0;

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
	relpSessUnacked_t *pUnacked;
	relpSessUnacked_t *pUnackedToDel;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	pThis = *ppThis;
	RELPOBJ_assert(pThis, Sess);

	/* pTcp may be NULL if we run into the destructor due to an error that occured
	 * during construction.
	 */
	if(pThis->pTcp != NULL) {
		if(pThis->pSrv != NULL) {
			relpSessSrvDoDisconnect(pThis);
		} else {
			/* we are at the client side of the connection */
			if(   pThis->sessState != eRelpSessState_DISCONNECTED
			   && pThis->sessState != eRelpSessState_BROKEN) {
				relpSessCltDoDisconnect(pThis);
			}
		}
	}

	if(pThis->pSendq != NULL)
		relpSendqDestruct(&pThis->pSendq);
	if(pThis->pTcp != NULL)
		relpTcpDestruct(&pThis->pTcp);

	/* unacked list */
	for(pUnacked = pThis->pUnackedLstRoot ; pUnacked != NULL ; ) {
		pUnackedToDel = pUnacked;
		pUnacked = pUnacked->pNext;
		relpSendbufDestruct(&pUnackedToDel->pSendbuf);
		free(pUnackedToDel);
	}

	free(pThis->srvPort);
	free(pThis->srvAddr);
	free(pThis->clientIP);
	free(pThis->pristring);
	free(pThis->ownCertFile);
	free(pThis->privKeyFile);
	relpSessFreePermittedPeers(pThis);

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

	CHKRet(relpSessConstruct(&pThis, pSrv->pEngine, RELP_SRV_CONN, pSrv));
	CHKRet(relpTcpAcceptConnReq(&pThis->pTcp, sock, pSrv));

	/* TODO: check against max# sessions */

	*ppThis = pThis;

finalize_it:
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
	relpOctet_t rcvBuf[RELP_RCV_BUF_SIZE+1];
	ssize_t lenBuf;
	ssize_t i;

	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);

	lenBuf = RELP_RCV_BUF_SIZE;
	CHKRet(relpTcpRcv(pThis->pTcp, rcvBuf, &lenBuf));

	if(lenBuf == 0) {
		pThis->pEngine->dbgprint("server closed relp session %p, session broken\n", pThis);
		/* even though we had a "normal" close, it is unexpected at this
		 * stage. Consequently, we consider the session to be broken, because
		 * the recovery action is the same no matter how it is broken.
		 */
		pThis->sessState = eRelpSessState_BROKEN;
		ABORT_FINALIZE(RELP_RET_SESSION_BROKEN);
	} else if ((int) lenBuf == -1) { /* I don't know why we need to cast to int, but we must... */
		if(errno != EAGAIN) {
			pThis->pEngine->dbgprint("errno %d during relp session %p, session broken\n", errno,pThis);
			pThis->sessState = eRelpSessState_BROKEN;
			ABORT_FINALIZE(RELP_RET_SESSION_BROKEN);
		}
	} else {
		/* Terminate buffer and output received data to debug*/
		rcvBuf[lenBuf] = '\0';
		pThis->pEngine->dbgprint("relp session read %d octets, buf '%s'\n", (int) lenBuf, rcvBuf);

		/* we have regular data, which we now can process */
		for(i = 0 ; i < lenBuf ; ++i) {
			CHKRet(relpFrameProcessOctetRcvd(&pThis->pCurrRcvFrame, rcvBuf[i], pThis));
		}
	}

finalize_it:
	LEAVE_RELPFUNC;
}


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
				     pData, lenData, pThis, NULL));
	/* now enqueue it to the sendq (which means "send it" ;)) */
	CHKRet(relpSendqAddBuf(pThis->pSendq, pSendbuf, pThis->pTcp));

finalize_it:
	if(iRet != RELP_RET_OK) {
		if(iRet == RELP_RET_IO_ERR) {
			pThis->pEngine->dbgprint("relp session %p is broken, io error\n", pThis);
			pThis->sessState = eRelpSessState_BROKEN;
			}

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


/* Send a command hint to the remote peer. This function works for the 
 * server-side of the connection. A modified version must be used for the
 * client side (because we have different ways of sending the data). We do
 * not yet need the client side. If we do, way should think about a generic
 * approach, eg by using function pointers for the send function.
 * rgerhards, 2008-03-31
 */
static relpRetVal
relpSessSrvSendHint(relpSess_t *pThis, unsigned char *pHint, size_t lenHint,
		    unsigned char *pData, size_t lenData)
{
	relpSendbuf_t *pSendbuf = NULL;

	ENTER_RELPFUNC;
	assert(pHint != NULL);
	assert(lenHint != 0);
	RELPOBJ_assert(pThis, Sess);

	CHKRet(relpFrameBuildSendbuf(&pSendbuf, 0, pHint, lenHint, pData, lenData, pThis, NULL));
	/* now send it */
	pThis->pEngine->dbgprint("hint-frame to send: '%s'\n", pSendbuf->pData + (9 - pSendbuf->lenTxnr));
	CHKRet(relpSendbufSend(pSendbuf, pThis->pTcp));

finalize_it:
	if(pSendbuf != NULL)
		relpSendbufDestruct(&pSendbuf);
	LEAVE_RELPFUNC;
}


/* Disconnect from the client. After disconnect, the session object
 * is destructed. This is the server-side disconnect and must be called
 * for server sessions only.
 * rgerhards, 2008-03-31
 */
static relpRetVal
relpSessSrvDoDisconnect(relpSess_t *pThis)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);

	/* Try to hint the client that we are closing the session. If the hint does
	 * not make it through to the client, that's ok, too. Then, it'll notice when it
	 * tries to use the connection. In any case, it can handle that (but obviously its
	 * better if the client is able to receive the hint, as this cleans up things a bit
	 * faster).
	 */
	CHKRet(relpSessSrvSendHint(pThis, (unsigned char*)"serverclose", 11, (unsigned char*)"", 0));

finalize_it:
	LEAVE_RELPFUNC;
}


/* ------------------------------ client based code ------------------------------ */


/* add an entry to our unacked frame list. The sendbuf object is handed over and must
 * no longer be accessed by the caller.
 * NOTE: we do not need mutex locks. This changes when we have a background transfer
 * thread (which we currently do not have).
 * rgerhards, 2008-03-20
 */
relpRetVal
relpSessAddUnacked(relpSess_t *pThis, relpSendbuf_t *pSendbuf)
{
	relpSessUnacked_t *pUnackedLstEntry;

	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);
	RELPOBJ_assert(pSendbuf, Sendbuf);

	if((pUnackedLstEntry = calloc(1, sizeof(relpSessUnacked_t))) == NULL)
		ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);

	pUnackedLstEntry->pSendbuf = pSendbuf;

	DLL_Add(pUnackedLstEntry, pThis->pUnackedLstRoot, pThis->pUnackedLstLast);
	++pThis->lenUnackedLst;

	if(pThis->lenUnackedLst >= pThis->sizeWindow) {
		/* in theory, we would need to check if the session is initialized, as
		 * we would mess up session state in that case. However, as the init
		 * process is just one frame, we can never run into the situation that
		 * the window is exhausted during init, so we do not check it.
		 */
		relpSessSetSessState(pThis, eRelpSessState_WINDOW_FULL);
		if(pThis->lenUnackedLst >= pThis->sizeWindow)
			pThis->pEngine->dbgprint("Warning: exceeding window size, max %d, curr %d\n",
						 pThis->lenUnackedLst, pThis->sizeWindow);
	}
	pThis->pEngine->dbgprint("ADD sess %p unacked %d, sessState %d\n", pThis, pThis->lenUnackedLst, pThis->sessState);

finalize_it:
	LEAVE_RELPFUNC;
}


/* Delete an entry from our unacked list. The list entry is destructed, but
 * the sendbuf not.
 * rgerhards, 2008-03-20
 */
static relpRetVal
relpSessDelUnacked(relpSess_t *pThis, relpSessUnacked_t *pUnackedLstEntry)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);
	assert(pUnackedLstEntry != NULL);

	DLL_Del(pUnackedLstEntry, pThis->pUnackedLstRoot, pThis->pUnackedLstLast);
	--pThis->lenUnackedLst;

	if(   pThis->lenUnackedLst < pThis->sizeWindow
	   && relpSessGetSessState(pThis) == eRelpSessState_WINDOW_FULL) {
		/* here, we need to check if we had WINDOW_FULL condition. The reason is that
		 * we otherwise mess up the session init handling - contrary to ...AddUnacked(),
		 * we run into the situation of a session state change on init. So we
		 * need to make sure it works.
		 */
		relpSessSetSessState(pThis, eRelpSessState_READY_TO_SEND);
	}

	free(pUnackedLstEntry);

	pThis->pEngine->dbgprint("DEL sess %p unacked %d, sessState %d\n", pThis, pThis->lenUnackedLst, pThis->sessState);
	LEAVE_RELPFUNC;
}


/* find an entry in the unacked list and provide it to the caller. The entry is handed
 * over to the caller and removed from the queue of unacked entries. It is the caller's
 * duty to destruct the sendbuf when it is done with it.
 * rgerhards, 20080-03-20
 */
relpRetVal
relpSessGetUnacked(relpSess_t *pThis, relpSendbuf_t **ppSendbuf, relpTxnr_t txnr)
{
	relpSessUnacked_t *pUnackedEtry;

	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);
	assert(ppSendbuf != NULL);
	
	for(  pUnackedEtry = pThis->pUnackedLstRoot
	    ; pUnackedEtry != NULL && pUnackedEtry->pSendbuf->txnr != txnr
	    ; pUnackedEtry = pUnackedEtry->pNext)
	   	/*JUST SKIP*/;

	if(pUnackedEtry == NULL)
		ABORT_FINALIZE(RELP_RET_NOT_FOUND);

	*ppSendbuf = pUnackedEtry->pSendbuf;
	relpSessDelUnacked(pThis, pUnackedEtry);

finalize_it:
	LEAVE_RELPFUNC;
}


/* Wait for a specific state of the relp session. This function blocks
 * until the session is in that state, but runs a receive loop to get hold
 * of server responses (because otherwise the state would probably never change ;)).
 * If a timeout occurs, the session is considered broken and control returned to
 * the caller. Caller must check if the session state has changed to "BROKEN" and
 * in this case drop the session. The timeout value is in seconds. This function
 * may be called if the session already has the desired state. In that case, it
 * returns, but still tries to see if we received anything from the server. If so,
 * this data is received. The suggested use is to call the function before any
 * send operation, so that will keep our receive capability alive without
 * resorting to multi-threading.
 * rgerhards, 2008-03-20
 */
static relpRetVal
relpSessWaitState(relpSess_t *pThis, relpSessState_t stateExpected, int timeout)
{
	fd_set readfds;
	int sock;
	int nfds;
	struct timespec tCurr; /* current time */
	struct timespec tTimeout; /* absolute timeout value */
	struct timeval tvSelect;
	relpRetVal localRet;

	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);

	/* are we already ready? */
	if(pThis->sessState == stateExpected || pThis->sessState == eRelpSessState_BROKEN) {
		FINALIZE;
	}

	/* first read any outstanding data and process the packets. Note that this
	 * call DOES NOT block.
	 */
	localRet = relpSessRcvData(pThis);
	if(localRet != RELP_RET_OK && localRet != RELP_RET_SESSION_BROKEN)
		ABORT_FINALIZE(localRet);

	/* re-check if we are already in the desired state. If so, we can immediately
	 * return. That saves us doing a costly clock call to set the timeout. As a
	 * side-effect, the timeout is actually applied without the time needed for
	 * above reception. I think is is OK, even a bit logical ;)
	 */
	if(pThis->sessState == stateExpected || pThis->sessState == eRelpSessState_BROKEN) {
		FINALIZE;
	}

	/* ok, looks like we actually need to do a wait... */
	clock_gettime(CLOCK_REALTIME, &tCurr);
	memcpy(&tTimeout, &tCurr, sizeof(struct timespec));
	tTimeout.tv_sec += timeout;

	while(!relpEngineShouldStop(pThis->pEngine)) {
		sock = relpSessGetSock(pThis);
		tvSelect.tv_sec = tTimeout.tv_sec - tCurr.tv_sec;
		tvSelect.tv_usec = (tTimeout.tv_nsec - tCurr.tv_nsec) / 1000000;
		if(tvSelect.tv_usec < 0) {
			tvSelect.tv_usec += 1000000;
			tvSelect.tv_sec--;
		}
		if(tvSelect.tv_sec < 0) {
			ABORT_FINALIZE(RELP_RET_TIMED_OUT);
		}

		FD_ZERO(&readfds);
		FD_SET(sock, &readfds);
		pThis->pEngine->dbgprint("relpSessWaitRsp waiting for data on "
			"fd %d, timeout %d.%d\n", sock, (int) tvSelect.tv_sec,
			(int) tvSelect.tv_usec);
		nfds = select(sock+1, (fd_set *) &readfds, NULL, NULL, &tvSelect);
		pThis->pEngine->dbgprint("relpSessWaitRsp select returns, "
			"nfds %d, errno %d\n", nfds, errno);
		if(relpEngineShouldStop(pThis->pEngine))
			break;
		/* we don't check if we had a timeout-we give it one last chance*/
		CHKRet(relpSessRcvData(pThis));
		pThis->pEngine->dbgprint("iRet after relpSessRcvData %d\n", iRet);
		if(   pThis->sessState == stateExpected
		   || pThis->sessState == eRelpSessState_BROKEN) {
			FINALIZE;
		}

		clock_gettime(CLOCK_REALTIME, &tCurr);
	}

finalize_it:
	pThis->pEngine->dbgprint("relpSessWaitState returns %d\n", iRet);
	if(iRet == RELP_RET_TIMED_OUT || relpEngineShouldStop(pThis->pEngine)) {
		/* the session is broken! */
		pThis->sessState = eRelpSessState_BROKEN;
	}

	LEAVE_RELPFUNC;
}


/* Send a command to the server.
 * This is a "raw" send function that just sends the command but does not
 * care about the receive loop or session state. This has been put into its
 * own function as some functionality (most importantly session init!) requires
 * handling that is other than the regular command/response mode.
 * rgerhards, 2008-03-19
 */
static relpRetVal
relpSessRawSendCommand(relpSess_t *pThis, unsigned char *pCmd, size_t lenCmd,
		    unsigned char *pData, size_t lenData, relpRetVal (*rspHdlr)(relpSess_t*,relpFrame_t*))
{
	relpSendbuf_t *pSendbuf;

	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);

	CHKRet(relpFrameBuildSendbuf(&pSendbuf, pThis->txnr, pCmd, lenCmd, pData, lenData, pThis, rspHdlr));
	pThis->txnr = relpEngineNextTXNR(pThis->txnr);
	/* now send it */
	pThis->pEngine->dbgprint("frame to send: '%s'\n", pSendbuf->pData + (9 - pSendbuf->lenTxnr));
	iRet = relpSendbufSendAll(pSendbuf, pThis, 1);

	if(iRet == RELP_RET_IO_ERR) {
		pThis->pEngine->dbgprint("relp session %p flagged as broken, IO error\n", pThis);
		pThis->sessState = eRelpSessState_BROKEN;
		ABORT_FINALIZE(RELP_RET_SESSION_BROKEN);
	}

finalize_it:
	LEAVE_RELPFUNC;
}


/* Send a command to the server.
 * The is the "regular" function which ensures that server messages are received
 * and messages are only sent when we have space left in our window. This function
 * must only be called after the session is fully initialized. Calling it before
 * initialization is finished will probably hang the client.
 * rgerhards, 2008-03-19
 */
relpRetVal
relpSessSendCommand(relpSess_t *pThis, unsigned char *pCmd, size_t lenCmd,
		    unsigned char *pData, size_t lenData, relpRetVal (*rspHdlr)(relpSess_t*,relpFrame_t*))
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);

	/* this both reads server responses as well as makes sure we have space
	 * left in our window.
	 */
	CHKRet(relpSessWaitState(pThis, eRelpSessState_READY_TO_SEND,
		pThis->timeout));

	/* re-try once if automatic retry mode is set */
	if(pThis->bAutoRetry && pThis->sessState == eRelpSessState_BROKEN) {
		CHKRet(relpSessTryReestablish(pThis));
	}

	/* then send our data */
	if(pThis->sessState == eRelpSessState_BROKEN)
		ABORT_FINALIZE(RELP_RET_SESSION_BROKEN);

	CHKRet(relpSessRawSendCommand(pThis, pCmd, lenCmd, pData, lenData, rspHdlr));

finalize_it:
	LEAVE_RELPFUNC;
}


/* Try to restablish a broken session. A single try is made and the result
 * reported back. RELP_RET_OK if we could get a new session, a RELP error state
 * otherwise (e.g. RELP_RET_SESSION_BROKEN, but could also be a more precise
 * error code). If the session can be re-established, any unsent frames are
 * resent. The function returns only after that happened.
 * rgerhards, 2008-03-22
 */
relpRetVal
relpSessTryReestablish(relpSess_t *pThis)
{
	relpSessUnacked_t *pUnackedEtry;

	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);
	assert(pThis->sessState = eRelpSessState_BROKEN);

	CHKRet(relpTcpAbortDestruct(&pThis->pTcp));
	CHKRet(relpSessConnect(pThis, pThis->protFamily, pThis->srvPort, pThis->srvAddr));
	/* if we reach this point, we could re-establish the session. We now
	 * need to resend any unacked data. Note that we need to patch in new txnr's
	 * into the existing frames. We need to do a special send command, as the usual
	 * one would maintain the unacked list, what we can not do right now (because
	 * it is not to be modified.
	 */
	pUnackedEtry = pThis->pUnackedLstRoot;
	if(pUnackedEtry != NULL)
		pThis->pEngine->dbgprint("relp session %p reestablished, now resending %d unacked frames\n",
					  pThis, pThis->lenUnackedLst);
	while(pUnackedEtry != NULL) {
		pThis->pEngine->dbgprint("resending frame '%s'\n", pUnackedEtry->pSendbuf->pData + 9
								   - pUnackedEtry->pSendbuf->lenTxnr);
		CHKRet(relpFrameRewriteTxnr(pUnackedEtry->pSendbuf, pThis->txnr));
		pThis->txnr = relpEngineNextTXNR(pThis->txnr);
		CHKRet(relpSendbufSendAll(pUnackedEtry->pSendbuf, pThis, 0));
		pUnackedEtry = pUnackedEtry->pNext;
	}

finalize_it:
	pThis->pEngine->dbgprint("after TryReestablish, sess state %d\n", pThis->sessState);
	LEAVE_RELPFUNC;
}


/* callback when the "open" command has been processed
 * Most importantly, this function needs to check if we are 
 * compatible with the server-provided offers and terminate if
 * not. If we are, we must set our own parameters to match the
 * server-provided ones. Please note that the offer processing here
 * is the last and final. So the ultimate decision is made and if we
 * are unhappy with something that we can not ignore, we must
 * terminate with an error status.
 * In such cases, we flag the session as broken, as theoretically
 * it is possible to fix it by restarting the server with a set
 * of different parameters. The question remains, though, if that's
 * the smartest route to take...
 * Note that offer-processing is very similiar to offer-processing
 * at the server end (copen.c). We may be able to combine some of
 * it in the future (or may not, depending on the subtly different
 * needs both parts have. For now I leave this to a ... TODO ;)).
 * rgerhards, 2008-03-25
 */
static relpRetVal
relpSessCBrspOpen(relpSess_t *pThis, relpFrame_t *pFrame)
{
	relpEngine_t *pEngine;
	relpOffers_t *pOffers = NULL;
	relpOffer_t *pOffer;
	relpOfferValue_t *pOfferVal;

	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);
	RELPOBJ_assert(pFrame, Frame);
	pEngine = pThis->pEngine;

	/* first get the offers list from the server response */
	CHKRet(relpOffersConstructFromFrame(&pOffers, pFrame));

	/* we loop through the offers and set session parameters. If we find
	 * something truely unacceptable, we break the session.
	 */
	for(pOffer = pOffers->pRoot ; pOffer != NULL ; pOffer = pOffer->pNext) {
		pEngine->dbgprint("processing server offer '%s'\n", pOffer->szName);
		if(!strcmp((char*)pOffer->szName, "relp_version")) {
			if(pOffer->pValueRoot == NULL)
				ABORT_FINALIZE(RELP_RET_INVALID_OFFER);
			if(pOffer->pValueRoot->intVal == -1)
				ABORT_FINALIZE(RELP_RET_INVALID_OFFER);
			if(pOffer->pValueRoot->intVal > pEngine->protocolVersion)
				ABORT_FINALIZE(RELP_RET_INCOMPAT_OFFERS);
			/* Once we support multiple versions, we may need to check what we
			 * are compatible with. For now, we accept anything, because there is
			 * nothing else yet ;)
			 */
			relpSessSetProtocolVersion(pThis, pOffer->pValueRoot->intVal);
		} else if(!strcmp((char*)pOffer->szName, "commands")) {
			for(pOfferVal = pOffer->pValueRoot ; pOfferVal != NULL ; pOfferVal = pOfferVal->pNext) {
				/* we do not care about return code in this case */
				relpSessSetEnableCmd(pThis, pOfferVal->szVal, eRelpCmdState_Enabled);
				pEngine->dbgprint("enabled command '%s'\n", pOfferVal->szVal);
			}
		} else if(!strcmp((char*)pOffer->szName, "relp_software")) {
			/* we know this parameter, but we do not do anything
			 * with it -- this may change if we need to emulate
			 * something based on known bad relp software behaviour.
			 */
		} else {
			/* if we do not know an offer name, we ignore it - in this
			 * case, we may simply not support it (but the client does and
			 * must now live without it...)
			 */
			pEngine->dbgprint("ignoring unknown server offer '%s'\n", pOffer->szName);
		}
	}
	relpSessSetSessState(pThis, eRelpSessState_INIT_RSP_RCVD);

finalize_it:
	if(pOffers != NULL)
		relpOffersDestruct(&pOffers);

	LEAVE_RELPFUNC;
}


/* Check if the set of offers inclulded in our session is compatible with
 * what we need. By now, all session parameters should be set to enabled. If
 * there are some left as "Required", they are not supported by the server,
 * in which case we can not open the session. -- rgerhards, 2008-03-27
 */
relpRetVal
relpSessCltConnChkOffers(relpSess_t *pThis)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);

	if(pThis->stateCmdSyslog == eRelpCmdState_Required)
		ABORT_FINALIZE(RELP_RET_RQD_FEAT_MISSING);

finalize_it:
	if(iRet != RELP_RET_OK)
		pThis->sessState = eRelpSessState_BROKEN;

	LEAVE_RELPFUNC;
}


/* Connect to the server. All session parameters (like remote address) must
 * already have been set.
 * rgerhards, 2008-03-19
 */
relpRetVal
relpSessConnect(relpSess_t *pThis, int protFamily, unsigned char *port, unsigned char *host)
{
	relpOffers_t *pOffers;
	unsigned char *pszOffers = NULL;
	size_t lenOffers;
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);

	CHKRet(relpSessFixCmdStates(pThis));
	if(pThis->srvAddr == NULL) { /* initial connect, need to save params */
		pThis->protFamily = protFamily;
		if((pThis->srvPort = (unsigned char*) strdup((char*)port)) == NULL)
			ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
		if((pThis->srvAddr = (unsigned char*) strdup((char*)host)) == NULL)
			ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}

	/* (re-)init some counters */
	pThis->txnr = 1;
	pThis->sessType = eRelpSess_Client;	/* indicate we have a client session */

	CHKRet(relpTcpConstruct(&pThis->pTcp, pThis->pEngine, RELP_CLT_CONN, pThis->pClt));
	CHKRet(relpTcpSetUsrPtr(pThis->pTcp, pThis->pUsr));
	if(pThis->bEnableTLS) {
		CHKRet(relpTcpEnableTLS(pThis->pTcp));
		if(pThis->bEnableTLSZip) {
			CHKRet(relpTcpEnableTLSZip(pThis->pTcp));
		}
		CHKRet(relpTcpSetGnuTLSPriString(pThis->pTcp, pThis->pristring));
		CHKRet(relpTcpSetCACert(pThis->pTcp, pThis->caCertFile));
		CHKRet(relpTcpSetOwnCert(pThis->pTcp, pThis->ownCertFile));
		CHKRet(relpTcpSetPrivKey(pThis->pTcp, pThis->privKeyFile));
		CHKRet(relpTcpSetAuthMode(pThis->pTcp, pThis->authmode));
		CHKRet(relpTcpSetPermittedPeers(pThis->pTcp, &pThis->permittedPeers));
	}
	CHKRet(relpTcpConnect(pThis->pTcp, protFamily, port, host, pThis->clientIP));
	relpSessSetSessState(pThis, eRelpSessState_PRE_INIT);

	/* create offers */
	CHKRet(relpSessConstructOffers(pThis, &pOffers));
	CHKRet(relpOffersToString(pOffers, NULL, 0, &pszOffers, &lenOffers));
	CHKRet(relpOffersDestruct(&pOffers));

	CHKRet(relpSessRawSendCommand(pThis, (unsigned char*)"open", 4, pszOffers, lenOffers,
				      relpSessCBrspOpen));
	relpSessSetSessState(pThis, eRelpSessState_INIT_CMD_SENT);
	CHKRet(relpSessWaitState(pThis, eRelpSessState_INIT_RSP_RCVD, pThis->timeout));

	/* we now have received the server's response. Now is a good time to check if the offers
	 * received back are compatible with what we need - and, if not, terminate the session...
	 */
	CHKRet(relpSessCltConnChkOffers(pThis));
	/* TODO: flag sesssion as broken if we did not succeed? */

	/* if we reach this point, we have a valid relp session */
	relpSessSetSessState(pThis, eRelpSessState_READY_TO_SEND); /* indicate session startup */

finalize_it:
pThis->pEngine->dbgprint("end relpSessConnect, iRet %d\n", iRet);
	if(pszOffers != NULL)
		free(pszOffers);

	LEAVE_RELPFUNC;
}


/* callback when the "close" command has been processed
 * rgerhars, 2008-03-21
 */
static relpRetVal
relpSessCBrspClose(relpSess_t *pThis, relpFrame_t __attribute__((unused)) *pFrame)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);

	relpSessSetSessState(pThis, eRelpSessState_CLOSE_RSP_RCVD);
pThis->pEngine->dbgprint("CBrspClose, setting state CLOSE_RSP_RCVD\n");

	LEAVE_RELPFUNC;
}


/* Disconnect from the server. After disconnect, the session object
 * is destructed. This is the client-side disconnect and must be called
 * for client sessions only.
 * rgerhards, 2008-03-21
 */
static relpRetVal
relpSessCltDoDisconnect(relpSess_t *pThis)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);

	/* first wait to be permitted to send, this time a bit more impatient (after all,
	 * we may be terminated if we take too long). We do not care about the return state.
	 * Even if we are outside of the window, we still send the close request, because
	 * everything else is even worse. So let's try to be polite...
	 */
	relpSessWaitState(pThis, eRelpSessState_READY_TO_SEND, 1);

	CHKRet(relpSessRawSendCommand(pThis, (unsigned char*)"close", 5, (unsigned char*)"", 0,
				      relpSessCBrspClose));
	relpSessSetSessState(pThis, eRelpSessState_CLOSE_CMD_SENT);
	CHKRet(relpSessWaitState(pThis, eRelpSessState_CLOSE_RSP_RCVD, pThis->timeout));

	relpSessSetSessState(pThis, eRelpSessState_DISCONNECTED); /* indicate session startup */

finalize_it:
	LEAVE_RELPFUNC;
}


relpRetVal
relpSessSetWindowSize(relpSess_t *pThis, int sizeWindow)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);
	if(sizeWindow > 1)
		pThis->sizeWindow = sizeWindow;
	LEAVE_RELPFUNC;
}

relpRetVal
relpSessSetTimeout(relpSess_t *pThis, unsigned timeout)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);
	pThis->timeout = timeout;
	LEAVE_RELPFUNC;
}

relpRetVal
relpSessSetClientIP(relpSess_t *pThis, unsigned char *ip)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);
	free(pThis->clientIP);
	if(ip == NULL)
		pThis->clientIP = NULL;
	else
		pThis->clientIP = (unsigned char*) strdup((char*)  ip);
	LEAVE_RELPFUNC;
}

/* this copies a *complete* permitted peers structure into the
 * session object.
 */
relpRetVal
relpSessSetPermittedPeers(relpSess_t *pThis, relpPermittedPeers_t *pPeers)
{
	ENTER_RELPFUNC;
	int i;
	RELPOBJ_assert(pThis, Sess);
	
	relpSessFreePermittedPeers(pThis);
	if(pPeers->nmemb != 0) {
		if((pThis->permittedPeers.name =
			malloc(sizeof(char*) * pPeers->nmemb)) == NULL) {
			ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
		}
		for(i = 0 ; i < pPeers->nmemb ; ++i) {
			if((pThis->permittedPeers.name[i] = strdup(pPeers->name[i])) == NULL) {
				ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
			}
		}
	}
	pThis->permittedPeers.nmemb = pPeers->nmemb;
finalize_it:
	LEAVE_RELPFUNC;
}

/* Enable TLS mode. */
relpRetVal
relpSessEnableTLS(relpSess_t *pThis)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);
	pThis->bEnableTLS = 1;
	LEAVE_RELPFUNC;
}

relpRetVal
relpSessSetUsrPtr(relpSess_t *pThis, void *pUsr)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);
	pThis->pUsr = pUsr;
	LEAVE_RELPFUNC;
}

relpRetVal
relpSessSetAuthMode(relpSess_t *pThis, relpAuthMode_t authmode)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);
	pThis->authmode = authmode;
	LEAVE_RELPFUNC;
}

/* Enable TLS Zip mode. */
relpRetVal
relpSessEnableTLSZip(relpSess_t *pThis)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);
	pThis->bEnableTLSZip = 1;
	LEAVE_RELPFUNC;
}

relpRetVal
relpSessSetGnuTLSPriString(relpSess_t *pThis, char *pristr)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Tcp);
	
	free(pThis->pristring);
	if(pristr == NULL) {
		pThis->pristring = NULL;
	} else {
		if((pThis->pristring = strdup(pristr)) == NULL)
			ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}
finalize_it:
	LEAVE_RELPFUNC;
}

relpRetVal
relpSessSetCACert(relpSess_t *pThis, char *cert)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Tcp);
	
	free(pThis->caCertFile);
	if(cert == NULL) {
		pThis->caCertFile = NULL;
	} else {
		if((pThis->caCertFile = strdup(cert)) == NULL)
			ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}
finalize_it:
	LEAVE_RELPFUNC;
}

relpRetVal
relpSessSetOwnCert(relpSess_t *pThis, char *cert)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Tcp);
	
	free(pThis->ownCertFile);
	if(cert == NULL) {
		pThis->ownCertFile = NULL;
	} else {
		if((pThis->ownCertFile = strdup(cert)) == NULL)
			ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}
finalize_it:
	LEAVE_RELPFUNC;
}

relpRetVal
relpSessSetPrivKey(relpSess_t *pThis, char *cert)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Tcp);
	
	free(pThis->privKeyFile);
	if(cert == NULL) {
		pThis->privKeyFile = NULL;
	} else {
		if((pThis->privKeyFile = strdup(cert)) == NULL)
			ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}
finalize_it:
	LEAVE_RELPFUNC;
}

/* set the protocol version to be used by this session
 * rgerhards, 2008-03-25
 */
relpRetVal
relpSessSetProtocolVersion(relpSess_t *pThis, int protocolVersion)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);
	pThis->protocolVersion = protocolVersion;
	LEAVE_RELPFUNC;
}


/* Enable or disable a command. Note that a command can not be enabled once
 * it has been set to forbidden! There will be no error return state in this
 * case.
 * rgerhards, 2008-03-25
 */
relpRetVal
relpSessSetEnableCmd(relpSess_t *pThis, unsigned char *pszCmd, relpCmdEnaState_t stateCmd)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);
	assert(pszCmd != NULL);

pThis->pEngine->dbgprint("SetEnableCmd in syslog cmd state: %d\n", pThis->stateCmdSyslog);
	if(!strcmp((char*)pszCmd, "syslog")) {
		if(pThis->stateCmdSyslog != eRelpCmdState_Forbidden)
			pThis->stateCmdSyslog = stateCmd;
	} else {
		pThis->pEngine->dbgprint("tried to set unknown command '%s' to %d\n", pszCmd, stateCmd);
		ABORT_FINALIZE(RELP_RET_UNKNOWN_CMD);
	}

finalize_it:
pThis->pEngine->dbgprint("SetEnableCmd out syslog cmd state: %d, iRet %d\n", pThis->stateCmdSyslog, iRet);
	LEAVE_RELPFUNC;
}


/* create an offers object based on the current session parameters we have.
 * rgerhards, 2008-03-25
 */
relpRetVal
relpSessConstructOffers(relpSess_t *pThis, relpOffers_t **ppOffers)
{
	relpOffers_t *pOffers;
	relpOffer_t *pOffer;

	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);
	assert(ppOffers != NULL);

	CHKRet(relpOffersConstruct(&pOffers, pThis->pEngine));

	/* now do the supported commands. Note that we must only send commands that
	 * are explicitely enabled or desired.
	 */
	CHKRet(relpOfferAdd(&pOffer, (unsigned char*) "commands", pOffers));
	if(   pThis->stateCmdSyslog == eRelpCmdState_Enabled
	   || pThis->stateCmdSyslog == eRelpCmdState_Desired
	   || pThis->stateCmdSyslog == eRelpCmdState_Required)
		CHKRet(relpOfferValueAdd((unsigned char*)"syslog", 0, pOffer));

	CHKRet(relpOfferAdd(&pOffer, (unsigned char*) "relp_software", pOffers));
	CHKRet(relpOfferValueAdd((unsigned char*) "http://librelp.adiscon.com", pThis->protocolVersion, pOffer));
	CHKRet(relpOfferValueAdd((unsigned char*) relpEngineGetVersion(), pThis->protocolVersion, pOffer));
	CHKRet(relpOfferValueAdd((unsigned char*) "librelp", pThis->protocolVersion, pOffer));

	/* just for cosmetic reasons: do relp_version last, so that it shows up
	 * at the top of the string.
	 */
	CHKRet(relpOfferAdd(&pOffer, (unsigned char*) "relp_version", pOffers));
	CHKRet(relpOfferValueAdd(NULL, pThis->protocolVersion, pOffer));

	*ppOffers = pOffers;

finalize_it:
	if(iRet != RELP_RET_OK) {
		if(pOffers != NULL)
			relpOffersDestruct(&pOffers);
	}

	LEAVE_RELPFUNC;
}


/* Convert unknown cmd state to forbidden.
 * This function mut be called after all command states have been set. Those
 * that have not expressively been set to desired or forbidden or now changed to
 * forbidden, so that they may not be selected by a peer.
 * rgerhards, 2008-03-25
 */
static relpRetVal
relpSessFixCmdStates(relpSess_t *pThis)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);

	if(pThis->stateCmdSyslog == eRelpCmdState_Unset)
		pThis->stateCmdSyslog = eRelpCmdState_Forbidden;

	LEAVE_RELPFUNC;
}



/* ------------------------------ command handlers ------------------------------ */


/* Send a syslog command.
 * rgerhards, 2008-03-25
 */
relpRetVal
relpSessSendSyslog(relpSess_t *pThis, unsigned char *pMsg, size_t lenMsg)
{

	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);

	if(pThis->stateCmdSyslog != eRelpCmdState_Enabled)
		ABORT_FINALIZE(RELP_RET_CMD_DISABLED);

	CHKRet(relpSessSendCommand(pThis, (unsigned char*)"syslog", 6, pMsg, lenMsg, NULL));

finalize_it:
	LEAVE_RELPFUNC;
}
