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
#include <string.h>
#include <errno.h>
#include <time.h>
#include "relp.h"
#include "relpsess.h"
#include "relpframe.h"
#include "sendq.h"
#include "dbllinklist.h"

/* forward definitions */
static relpRetVal relpSessDisconnect(relpSess_t *pThis);
relpRetVal relpSessTryReestablish(relpSess_t *pThis);


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
	pThis->timeout = 10; /* TODO: make configurable */
	pThis->sizeWindow = RELP_DFLT_WINDOW_SIZE; /* TODO: make configurable */
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
	relpSessUnacked_t *pUnacked;
	relpSessUnacked_t *pUnackedToDel;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	pThis = *ppThis;
	RELPOBJ_assert(pThis, Sess);

pThis->pEngine->dbgprint("relpSessionDestruct %p\n", pThis);
	if(   pThis->sessState != eRelpSessState_DISCONNECTED
	   && pThis->sessState != eRelpSessState_BROKEN) {
		relpSessDisconnect(pThis);
	}

	if(pThis->pSendq != NULL)
		relpSendqDestruct(&pThis->pSendq);
	if(pThis->pTcp != NULL)
		relpTcpDestruct(&pThis->pTcp);

	/* unacked list */
	for(pUnacked = pThis->pUnackedLstRoot ; pUnacked != NULL ; ) {
		pUnackedToDel = pUnacked;
		pUnacked = pUnacked->pNext;
pThis->pEngine->dbgprint("relpSessionDestruct Unacked %p, sendbuf %p\n", pUnackedToDel, pUnackedToDel->pSendbuf);
		relpSendbufDestruct(&pUnackedToDel->pSendbuf);
		free(pUnackedToDel);
	}

	if(pThis->srvPort != NULL)
		free(pThis->srvPort);
	if(pThis->srvAddr != NULL)
		free(pThis->srvAddr);

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
memset(rcvBuf, 0, RELP_RCV_BUF_SIZE);

	lenBuf = RELP_RCV_BUF_SIZE;
	CHKRet(relpTcpRcv(pThis->pTcp, rcvBuf, &lenBuf));

pThis->pEngine->dbgprint("relp session read %d octets, buf '%s'\n", (int) lenBuf, rcvBuf);
	if(lenBuf == 0) {
		pThis->pEngine->dbgprint("server closed relp session %p, session broken\n", pThis);
		/* even though we had a "normal" close, it is unexpected at this
		 * stage. Consequently, we consider the session to be broken, because
		 * the recovery action is the same no matter how it is broken.
		 */
		pThis->sessState = eRelpSessState_BROKEN;
		FINALIZE; /* a broken session is NO error return - the caller handles that */
	} else if ((int) lenBuf == -1) { /* I don't know why we need to cast to int, but we must... */
		if(errno != EAGAIN) {
			pThis->pEngine->dbgprint("errno %d during relp session %p, session broken\n", errno,pThis);
			pThis->sessState = eRelpSessState_BROKEN;
		FINALIZE; /* a broken session is NO error return - the caller handles that */
		}
	} else {
		/* we have regular data, which we now can process */
		for(i = 0 ; i < lenBuf ; ++i) {
			CHKRet(relpFrameProcessOctetRcvd(&pThis->pCurrRcvFrame, rcvBuf[i], pThis));
		}
	}

finalize_it:
pThis->pEngine->dbgprint("end relpSessRcvData, iRet %d, session state %d\n", iRet, pThis->sessState);
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

	if(pThis->lenUnackedLst == pThis->sizeWindow) {
		/* in theory, we would need to check if the session is initialized, as
		 * we would mess up session state in that case. However, as the init
		 * process is just one frame, we can never run into the situation that
		 * the window is exhausted during init, so we do not check it.
		 */
		relpSessSetSessState(pThis, eRelpSessState_WINDOW_FULL);
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

	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);

	/* first read any outstanding data and process the packets. Note that this
	 * call DOES NOT block.
	 */
	CHKRet(relpSessRcvData(pThis));

	/* check if we are already in the desired state. If so, we can immediately
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

	while(1) {
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
pThis->pEngine->dbgprint("relpSessWaitRsp waiting for data on fd %d, timeout %d.%d\n", sock, (int) tvSelect.tv_sec, (int) tvSelect.tv_usec);
		nfds = select(sock+1, (fd_set *) &readfds, NULL, NULL, &tvSelect);
pThis->pEngine->dbgprint("relpSessWaitRsp select returns, nfds %d, err %s\n", nfds, strerror(errno));
		/* we don't check if we had a timeout - we give it one last chance */
		CHKRet(relpSessRcvData(pThis));
pThis->pEngine->dbgprint("iRet after relpSessRcvData %d\n", iRet);
		if(pThis->sessState == stateExpected || pThis->sessState == eRelpSessState_BROKEN) {
			FINALIZE;
		}

		clock_gettime(CLOCK_REALTIME, &tCurr);
	}

finalize_it:
pThis->pEngine->dbgprint("relpSessWaitState returns%d\n", iRet);
	if(iRet == RELP_RET_TIMED_OUT) {
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
		    unsigned char *pData, size_t lenData, relpRetVal (*rspHdlr)(relpSess_t*))
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
		    unsigned char *pData, size_t lenData, relpRetVal (*rspHdlr)(relpSess_t*))
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);

	/* this both reads server responses as well as makes sure we have space left
	 * in our window. We provide a nearly eternal timeout (3 minutes). If we are not
	 * ready to send in that period, something is awfully wrong. TODO: we may want
	 * to make this timeout configurable, but I don't think it is a priority.
	 */
	CHKRet(relpSessWaitState(pThis, eRelpSessState_READY_TO_SEND, 2));
	//CHKRet(relpSessWaitState(pThis, eRelpSessState_READY_TO_SEND, 180));

	/* re-try once if automatic retry mode is set */
#warning "code missing - auto retry mode"
pThis->pEngine->dbgprint("send command relp sess state %d\n", pThis->sessState);
	if(1 && pThis->sessState == eRelpSessState_BROKEN) {
pThis->pEngine->dbgprint("SendCommand does auto-retry\n");
		CHKRet(relpSessTryReestablish(pThis));
	}

pThis->pEngine->dbgprint("sendcommand ready to send, relp sess state %d\n", pThis->sessState);
	/* then send our data */
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
pThis->pEngine->dbgprint("resending frame '%s'\n", pUnackedEtry->pSendbuf->pData + 9 - pUnackedEtry->pSendbuf->lenTxnr);
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
 * rgerhars, 2008-03-20
 */
static relpRetVal
relpSessCBrspInit(relpSess_t *pThis)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);

	// TODO: process offers!
	relpSessSetSessState(pThis, eRelpSessState_INIT_RSP_RCVD);
pThis->pEngine->dbgprint("CBrsp, setting state INIT_RSP_RCVD\n");

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

	CHKRet(relpTcpConstruct(&pThis->pTcp, pThis->pEngine));
	CHKRet(relpTcpConnect(pThis->pTcp, protFamily, port, host));
	relpSessSetSessState(pThis, eRelpSessState_PRE_INIT);

	CHKRet(relpSessRawSendCommand(pThis, (unsigned char*)"open", 4, (unsigned char*)"relp_version=0", 14,
				      relpSessCBrspInit));
	relpSessSetSessState(pThis, eRelpSessState_INIT_CMD_SENT);
	CHKRet(relpSessWaitState(pThis, eRelpSessState_INIT_RSP_RCVD, pThis->timeout));

	/* if we reach this point, we have a valid relp session */
	relpSessSetSessState(pThis, eRelpSessState_READY_TO_SEND); /* indicate session startup */

finalize_it:
	LEAVE_RELPFUNC;
}


/* callback when the "close" command has been processed
 * rgerhars, 2008-03-21
 */
static relpRetVal
relpSessCBrspClose(relpSess_t *pThis)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);

	relpSessSetSessState(pThis, eRelpSessState_CLOSE_RSP_RCVD);
pThis->pEngine->dbgprint("CBrspClose, setting state CLOSE_RSP_RCVD\n");

	LEAVE_RELPFUNC;
}


/* Disconnect from the server. After disconnect, the session object
 * is destructed.
 * rgerhards, 2008-03-21
 */
static relpRetVal
relpSessDisconnect(relpSess_t *pThis)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sess);

pThis->pEngine->dbgprint("SessDisconnect enter, sessState %d\n", pThis->sessState);
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
