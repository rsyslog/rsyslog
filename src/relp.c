/* The RELP (reliable event logging protocol) core protocol library.
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
#include <sys/select.h>
#include <string.h>
#include <assert.h>
#include "relp.h"
#include "relpsrv.h"
#include "relpclt.h"
#include "relpframe.h"
#include "relpsess.h"
#include "cmdif.h"
#include "sendq.h"
#include "dbllinklist.h"


/* ------------------------------ some internal functions ------------------------------ */

/* add an entry to our server list. The server object is handed over and must
 * no longer be accessed by the caller.
 * rgerhards, 2008-03-17
 */
static relpRetVal
relpEngineAddToSrvList(relpEngine_t *pThis, relpSrv_t *pSrv)
{
	relpEngSrvLst_t *pSrvLstEntry;

	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Engine);
	RELPOBJ_assert(pSrv, Srv);

	if((pSrvLstEntry = calloc(1, sizeof(relpEngSrvLst_t))) == NULL)
		ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);

	pSrvLstEntry->pSrv = pSrv;

	pthread_mutex_lock(&pThis->mutSrvLst);
	DLL_Add(pSrvLstEntry, pThis->pSrvLstRoot, pThis->pSrvLstLast);
	++pThis->lenSrvLst;
	pthread_mutex_unlock(&pThis->mutSrvLst);

finalize_it:
	LEAVE_RELPFUNC;
}


/* add an entry to our session list. The session object is handed over and must
 * no longer be accessed by the caller.
 * rgerhards, 2008-03-17
 */
static relpRetVal
relpEngineAddToSess(relpEngine_t *pThis, relpSess_t *pSess)
{
	relpEngSessLst_t *pSessLstEntry;

	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Engine);
	RELPOBJ_assert(pSess, Sess);

	if((pSessLstEntry = calloc(1, sizeof(relpEngSessLst_t))) == NULL)
		ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);

	pSessLstEntry->pSess = pSess;

	pthread_mutex_lock(&pThis->mutSessLst);
	DLL_Add(pSessLstEntry, pThis->pSessLstRoot, pThis->pSessLstLast);
	++pThis->lenSessLst;
	pthread_mutex_unlock(&pThis->mutSessLst);

finalize_it:
	LEAVE_RELPFUNC;
}


/* Delete an entry from our session list. The session object is destructed.
 * rgerhards, 2008-03-17
 */
static relpRetVal
relpEngineDelSess(relpEngine_t *pThis, relpEngSessLst_t *pSessLstEntry)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Engine);
	assert(pSessLstEntry != NULL);

	pthread_mutex_lock(&pThis->mutSessLst);
	DLL_Del(pSessLstEntry, pThis->pSessLstRoot, pThis->pSessLstLast);
	--pThis->lenSessLst;
	pthread_mutex_unlock(&pThis->mutSessLst);

	relpSessDestruct(&pSessLstEntry->pSess);
	free(pSessLstEntry);

	LEAVE_RELPFUNC;
}

/* ------------------------------ end of internal functions ------------------------------ */

/** Construct a RELP engine instance
 * This is the first thing that a caller must do before calling any
 * RELP function. The relp engine must only destructed after all RELP
 * operations have been finished.
 */
relpRetVal
relpEngineConstruct(relpEngine_t **ppThis)
{
	relpEngine_t *pThis;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	if((pThis = calloc(1, sizeof(relpEngine_t))) == NULL) {
		ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}

	RELP_CORE_CONSTRUCTOR(pThis, Engine);
	pThis->protocolVersion = RELP_CURR_PROTOCOL_VERSION;
	pthread_mutex_init(&pThis->mutSrvLst, NULL);
	pthread_mutex_init(&pThis->mutSessLst, NULL);

	*ppThis = pThis;

finalize_it:
	LEAVE_RELPFUNC;
}


/** Destruct a RELP engine instance
 * Should be called only a after all RELP functions have been terminated.
 * Terminates librelp operations, no calls are permitted after engine 
 * destruction.
 */
relpRetVal
relpEngineDestruct(relpEngine_t **ppThis)
{
	relpEngine_t *pThis;
	relpEngSrvLst_t *pSrvL, *pSrvLNxt;
	relpEngSessLst_t *pSessL, *pSessLNxt;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	pThis = *ppThis;
	RELPOBJ_assert(pThis, Engine);

	/* now destruct all currently existing sessions */
	for(pSessL = pThis->pSessLstRoot ; pSessL != NULL ; pSessL = pSessLNxt) {
		pSessLNxt = pSessL->pNext;
		relpSessDestruct(&pSessL->pSess);
		free(pSessL);
	}

	/* and now all existing servers... */
	for(pSrvL = pThis->pSrvLstRoot ; pSrvL != NULL ; pSrvL = pSrvLNxt) {
		pSrvLNxt = pSrvL->pNext;
		relpSrvDestruct(&pSrvL->pSrv);
		free(pSrvL);
	}

	pthread_mutex_destroy(&pThis->mutSrvLst);
	pthread_mutex_destroy(&pThis->mutSessLst);
	/* done with de-init work, now free engine object itself */
	free(pThis);
	*ppThis = NULL;

	LEAVE_RELPFUNC;
}


static void dbgprintDummy(char __attribute__((unused)) *fmt, ...) {}
/* set a pointer to the debug function inside the engine. To reset a debug
 * function that already has been set, provide a NULL function pointer.
 * rgerhards, 2008-03-17
 */
relpRetVal
relpEngineSetDbgprint(relpEngine_t *pThis, void (*dbgprint)(char *fmt, ...) __attribute__((format(printf, 1, 2))))
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Engine);

	pThis->dbgprint = (dbgprint == NULL) ? dbgprintDummy : dbgprint;
	LEAVE_RELPFUNC;
}


/* unfortunately, we need to duplicate some functionality to support the <= 0.1.2
 * callback interface (which did not contain a user pointer) and the >= 0.1.3 
 * callback interface. I have thought a lot about doing this smarter, but there
 * is no better way the API offers. The functions ending in ...2() are the new
 * interface. -- rgerhards, 2008-07-08
 */


/* a dummy for callbacks not set by the caller */
static relpRetVal relpSrvSyslogRcvDummy2(void __attribute__((unused)) *pUsr,
	unsigned char __attribute__((unused)) *pHostName,
	unsigned char __attribute__((unused)) *pIP, unsigned char __attribute__((unused)) *pMsg,
	size_t __attribute__((unused)) lenMsg)
{ return RELP_RET_NOT_IMPLEMENTED;
}
/* set the syslog receive callback. If NULL is provided, it is set to the
 * not implemented dummy.
 */
relpRetVal
relpEngineSetSyslogRcv2(relpEngine_t *pThis, relpRetVal (*pCB)(void *, unsigned char*, unsigned char*, unsigned char*, size_t))
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Engine);

	pThis->onSyslogRcv = NULL;
	pThis->onSyslogRcv2 = (pCB == NULL) ? relpSrvSyslogRcvDummy2 : pCB;
	LEAVE_RELPFUNC;
}

/* add a relp listener to the engine. The listen port must be provided.
 * The listen port may be NULL, in which case the default port is used.
 * rgerhards, 2008-03-17
 */
relpRetVal
relpEngineAddListner2(relpEngine_t *pThis, unsigned char *pLstnPort, void *pUsr)
{
	relpSrv_t *pSrv;
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Engine);

	CHKRet(relpSrvConstruct(&pSrv, pThis));
	CHKRet(relpSrvSetUsrPtr(pSrv, pUsr));
	CHKRet(relpSrvSetLstnPort(pSrv, pLstnPort));
	CHKRet(relpSrvRun(pSrv));

	/* all went well, so we can add the server to our server list */
	CHKRet(relpEngineAddToSrvList(pThis, pSrv));

finalize_it:
	LEAVE_RELPFUNC;
}


/* a dummy for callbacks not set by the caller */
static relpRetVal relpSrvSyslogRcvDummy(unsigned char __attribute__((unused)) *pHostName,
	unsigned char __attribute__((unused)) *pIP, unsigned char __attribute__((unused)) *pMsg,
	size_t __attribute__((unused)) lenMsg)
{ return RELP_RET_NOT_IMPLEMENTED;
}
/* set the syslog receive callback. If NULL is provided, it is set to the
 * not implemented dummy.
 */
relpRetVal
relpEngineSetSyslogRcv(relpEngine_t *pThis, relpRetVal (*pCB)(unsigned char*, unsigned char*, unsigned char*, size_t))
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Engine);

	pThis->onSyslogRcv = (pCB == NULL) ? relpSrvSyslogRcvDummy : pCB;
	pThis->onSyslogRcv2 = NULL;
	LEAVE_RELPFUNC;
}

/* add a relp listener to the engine. The listen port must be provided.
 * The listen port may be NULL, in which case the default port is used.
 * rgerhards, 2008-03-17
 */
relpRetVal
relpEngineAddListner(relpEngine_t *pThis, unsigned char *pLstnPort)
{
	relpSrv_t *pSrv;
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Engine);

	CHKRet(relpSrvConstruct(&pSrv, pThis));
	CHKRet(relpSrvSetLstnPort(pSrv, pLstnPort));
	CHKRet(relpSrvRun(pSrv));

	/* all went well, so we can add the server to our server list */
	CHKRet(relpEngineAddToSrvList(pThis, pSrv));

finalize_it:
	LEAVE_RELPFUNC;
}


/* The "Run" method starts the relp engine. Most importantly, this means the engine begins
 * to read and write data to its peers. This method must be called on its own thread as it
 * will not return until the engine is finished. Note that the engine itself may (or may
 * not ;)) spawn additional threads. This is an implementation detail not to be cared of by
 * caller.
 * Note that the engine MUST be running even if the caller intends to just SEND messages.
 * This is necessary because relp is a full-duplex protcol where acks and commands (e.g.
 * "abort" may be received at any time.
 *
 * This function is implemented as a select() server. I know that epoll() wold probably
 * be much better, but I implement the first version as select() because of portability.
 * Once everything has matured, we may begin to provide performance-optimized versions for
 * the several flavours of enhanced OS APIs.
 * rgerhards, 2008-03-17
 */
relpRetVal
relpEngineRun(relpEngine_t *pThis)
{
	relpEngSrvLst_t *pSrvEtry;
	relpEngSessLst_t *pSessEtry;
	relpEngSessLst_t *pSessEtryNext;
	relpSess_t *pNewSess;
	relpRetVal localRet;
	int iSocks;
	int sock;
	int maxfds;
	int nfds;
	fd_set readfds;
	fd_set writefds;

	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Engine);

	/* this is an endless loop - TODO: decide how to terminate */
	while(1) {
	        maxfds = 0;
	        FD_ZERO(&readfds);
	        FD_ZERO(&writefds);

		/* Add the listen sockets to the list of read descriptors.  */
		for(pSrvEtry = pThis->pSrvLstRoot ; pSrvEtry != NULL ; pSrvEtry = pSrvEtry->pNext) {
			for(iSocks = 1 ; iSocks <= relpSrvGetNumLstnSocks(pSrvEtry->pSrv) ; ++iSocks) {
				sock = relpSrvGetLstnSock(pSrvEtry->pSrv, iSocks);
				FD_SET(sock, &readfds);
				if(sock > maxfds) maxfds = sock;
			}
		}

		/* Add all sessions for reception and sending (they all have just one socket) */
		for(pSessEtry = pThis->pSessLstRoot ; pSessEtry != NULL ; pSessEtry = pSessEtry->pNext) {
			sock = relpSessGetSock(pSessEtry->pSess);
			FD_SET(sock, &readfds);
			/* now check if a send request is outstanding and, if so, add it */
			if(!relpSendqIsEmpty(pSessEtry->pSess->pSendq)) {
				FD_SET(sock, &writefds);
			}
			if(sock > maxfds) maxfds = sock;
		}

		/* done adding all sockets */
		if(pThis->dbgprint != dbgprintDummy) {
			pThis->dbgprint("***<librelp> calling select, active file descriptors (max %d): ", maxfds);
			for(nfds = 0; nfds <= maxfds; ++nfds)
				if(FD_ISSET(nfds, &readfds))
					pThis->dbgprint("%d ", nfds);
			pThis->dbgprint("\n");
		}

		/* wait for io to become ready */
		nfds = select(maxfds+1, (fd_set *) &readfds, &writefds, NULL, NULL);
pThis->dbgprint("relp select returns, nfds %d\n", nfds);
	
		/* and then start again with the servers (new connection request) */
		for(pSrvEtry = pThis->pSrvLstRoot ; pSrvEtry != NULL ; pSrvEtry = pSrvEtry->pNext) {
			for(iSocks = 1 ; iSocks <= relpSrvGetNumLstnSocks(pSrvEtry->pSrv) ; ++iSocks) {
				sock = relpSrvGetLstnSock(pSrvEtry->pSrv, iSocks);
				if(FD_ISSET(sock, &readfds)) {
					pThis->dbgprint("new connect on RELP socket #%d\n", sock);
					localRet = relpSessAcceptAndConstruct(&pNewSess, pSrvEtry->pSrv, sock);
pThis->dbgprint("relp accept session returns, iRet %d\n", localRet);
					if(localRet == RELP_RET_OK) {
						localRet = relpEngineAddToSess(pThis, pNewSess);
					}
					/* TODO: check localret, emit error msg! */
					--nfds; /* indicate we have processed one */
				}
			}
		}

		/* now check if we have some action waiting for sessions */
		for(pSessEtry = pThis->pSessLstRoot ; pSessEtry != NULL ; ) {
			pSessEtryNext = pSessEtry->pNext; /* we need to cache this as we may delete the entry! */
			sock = relpSessGetSock(pSessEtry->pSess);
			/* read data waiting? */
			if(FD_ISSET(sock, &readfds)) {
				localRet = relpSessRcvData(pSessEtry->pSess); /* errors are handled there */
				/* if we had an error during processing, we must shut down the session. This
				 * is part of the protocol specification: errors are recovered by aborting the
				 * session, which may eventually be followed by a new connect.
				 */
				if(localRet != RELP_RET_OK) {
					pThis->dbgprint("relp session %d iRet %d, tearing it down\n",
						        sock, localRet);
					relpEngineDelSess(pThis, pSessEtry);
				}
				--nfds; /* indicate we have processed one */
			}
			/* are we able to write? */
			if(FD_ISSET(sock, &writefds)) {
				localRet = relpSessSndData(pSessEtry->pSess); /* errors are handled there */
				/* if we had an error during processing, we must shut down the session. This
				 * is part of the protocol specification: errors are recovered by aborting the
				 * session, which may eventually be followed by a new connect.
				 */
				if(localRet != RELP_RET_OK) {
					pThis->dbgprint("relp session %d iRet %d during send, tearing it down\n",
						        sock, localRet);
					relpEngineDelSess(pThis, pSessEtry);
				}
			}

			pSessEtry = pSessEtryNext;
		}

	}

	LEAVE_RELPFUNC;
}


/* as a quick hack, we provide our command handler prototypes here
 * right in front of the dispatcher. This saves us many otherwise-unneeded
 * header files (and will go away if we make them dynamically loadable).
 * rgerhards, 2008-03-17
 */
/* core (protocol) commands */
PROTOTYPEcommand(S, Init)
PROTOTYPEcommand(S, Close)
PROTOTYPEcommand(C, Serverclose)
PROTOTYPEcommand(S, Rsp)
/* extension commands */
PROTOTYPEcommand(S, Syslog)

/* process an incoming command
 * This function receives a RELP frame and dispatches it to the correct
 * command handler. If the command is unknown, it responds with an error
 * return state but does not process anything. The frame is NOT destructed
 * by this function - this must be done by the caller.
 * rgerhards, 2008-03-17
 */
relpRetVal
relpEngineDispatchFrame(relpEngine_t *pThis, relpSess_t *pSess, relpFrame_t *pFrame)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Engine);
	RELPOBJ_assert(pSess, Sess);
	RELPOBJ_assert(pFrame, Frame);

	pThis->dbgprint("relp engine is dispatching frame with command '%s'\n", pFrame->cmd);

	/* currently, we hardcode the commands. Over time, they may be dynamically 
	 * loaded and, when so, should come from a linked list. TODO -- rgerhards, 2008-03-17
	 */
	if(!strcmp((char*)pFrame->cmd, "open")) {
		CHKRet(relpSCInit(pFrame, pSess));
	} else if(!strcmp((char*)pFrame->cmd, "close")) {
		CHKRet(relpSCClose(pFrame, pSess));
	} else if(!strcmp((char*)pFrame->cmd, "serverclose")) {
		CHKRet(relpCCServerclose(pFrame, pSess));
	} else if(!strcmp((char*)pFrame->cmd, "syslog")) {
		CHKRet(relpSCSyslog(pFrame, pSess));
	} else if(!strcmp((char*)pFrame->cmd, "rsp")) {
		CHKRet(relpSCRsp(pFrame, pSess));
	} else {
		pThis->dbgprint("invalid or not supported relp command '%s'\n", pFrame->cmd);
		ABORT_FINALIZE(RELP_RET_INVALID_CMD);
	}

finalize_it:
	LEAVE_RELPFUNC;
}


/* This relp engine function constructs a relp client and adds it to any
 * necessary engine structures. As of now, there are no such structures to
 * which it needs to be added, but that may change in the future. So librelp
 * users should always call the client-generating functions inside the engine.
 * rgerhards, 2008-03-19
 */
relpRetVal
relpEngineCltConstruct(relpEngine_t *pThis, relpClt_t **ppClt)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Engine);
	assert(ppClt != NULL);

	CHKRet(relpCltConstruct(ppClt, pThis));

	pThis->dbgprint("relp engine create a new client (%p)\n", *ppClt);

finalize_it:
	LEAVE_RELPFUNC;
}


/* Destruct a relp client inside the engine. Counterpart to
 * relpEngineCltConstruct(), see comment there for details.
 * rgerhards, 2008-03-19
 */
relpRetVal
relpEngineCltDestruct(relpEngine_t *pThis, relpClt_t **ppClt)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Engine);
	assert(ppClt != NULL);
	RELPOBJ_assert(*ppClt, Clt);

	pThis->dbgprint("relp engine destructing a client (%p)\n", *ppClt);

	CHKRet(relpCltDestruct(ppClt));

finalize_it:
	LEAVE_RELPFUNC;
}


/* return a version string for librelp. This is also meant to be used during
 * a configure library entry point check.
 * rgerhards, 2008-03-25
 */
char *relpEngineGetVersion(void)
{
#	ifdef DEBUG
		return VERSION "(debug mode)";
#	else
		return VERSION;
#	endif
}


/* Enable or disable a command. Note that a command can not be enabled once
 * it has been set to forbidden! There will be no error return state in this
 * case.
 * rgerhards, 2008-03-27
 */
relpRetVal
relpEngineSetEnableCmd(relpEngine_t *pThis, unsigned char *pszCmd, relpCmdEnaState_t stateCmd)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Engine);
	assert(pszCmd != NULL);

pThis->dbgprint("ENGINE SetEnableCmd in syslog cmd state: %d\n", pThis->stateCmdSyslog);
	if(!strcmp((char*)pszCmd, "syslog")) {
		if(pThis->stateCmdSyslog != eRelpCmdState_Forbidden)
			pThis->stateCmdSyslog = stateCmd;
	} else {
		pThis->dbgprint("tried to set unknown command '%s' to %d\n", pszCmd, stateCmd);
		ABORT_FINALIZE(RELP_RET_UNKNOWN_CMD);
	}

finalize_it:
pThis->dbgprint("ENGINE SetEnableCmd out syslog cmd state: %d, iRet %d\n", pThis->stateCmdSyslog, iRet);
	LEAVE_RELPFUNC;
}


/* Enable (1) or Disable (0) DNS hostname resolution when accepting a remote
 * session. If disabled, the IP address will be used as the hostname.
 * rgerhards, 2008-03-31
 */
relpRetVal
relpEngineSetDnsLookupMode(relpEngine_t *pThis, int iMode)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Engine);

	if(iMode != 0 && iMode != 1)
		ABORT_FINALIZE(RELP_RET_INVALID_PARAM);

	pThis->bEnableDns = iMode;

finalize_it:
	LEAVE_RELPFUNC;
}
