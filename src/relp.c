/* The RELP (reliable event logging protocol) core protocol library.
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
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/select.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
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

void __attribute__((format(printf, 4, 5)))
relpEngineCallOnGenericErr(relpEngine_t *pThis, char *eobj, relpRetVal ecode, char *fmt, ...)
{
	va_list ap;
	char emsg[1024];
	
	va_start(ap, fmt);
	vsnprintf(emsg, sizeof(emsg), fmt, ap);
	emsg[sizeof(emsg)/sizeof(char) - 1] = '\0'; /* just to be on the safe side... */
	va_end(ap);
	
	pThis->dbgprint("librelp: generic error: ecode %d, eobj %s,"
		"emsg '%s'\n", ecode, eobj, emsg);
	if(pThis->onGenericErr != NULL) {
		pThis->onGenericErr(eobj, emsg, ecode);
	}
}

static char *
relpEngine_strerror_r(int errnum, char *buf, size_t buflen) {
#ifndef HAVE_STRERROR_R
	char *p;
	p = strerror(errnum);
	strncpy(buf, emsg, buflen);
	buf[buflen-1] = '\0';
#else
#	ifdef STRERROR_R_CHAR_P
	char *p;
	p = strerror_r(errnum, buf, buflen);
	if(p != buf) {
		strncpy(buf, p, buflen);
		buf[buflen - 1] = '\0';
	}
#	else
	strerror_r(errnum, buf, buflen);
#	endif
#endif
	return buf;
}

#if defined(HAVE_EPOLL_CREATE1) || defined(HAVE_EPOLL_CREATE)
static relpRetVal
addToEpollSet(relpEngine_t *pThis, epolld_type_t typ, void *ptr, int sock, epolld_t **pepd)
{
	epolld_t *epd = NULL;
	ENTER_RELPFUNC;

	CHKmalloc(epd = calloc(sizeof(epolld_t), 1));
	epd->typ = typ;
	epd->ptr = ptr;
	epd->sock = sock;
	epd->ev.events = EPOLLIN;
	epd->ev.data.ptr = (void*) epd;

	pThis->dbgprint("librelp: add socket %d to epoll set (ptr %p)\n", sock, ptr);
	if(epoll_ctl(pThis->efd, EPOLL_CTL_ADD, sock, &epd->ev) != 0) {
		char errStr[1024];
		int eno = errno;
		relpEngineCallOnGenericErr(pThis, "librelp", RELP_RET_ERR_EPOLL_CTL,
			"os error (%d) during EPOLL_CTL_ADD: %s",
			eno, relpEngine_strerror_r(eno, errStr, sizeof(errStr)));
		ABORT_FINALIZE(RELP_RET_ERR_EPOLL_CTL);
	}
	*pepd = epd;

finalize_it:
	if(iRet != RELP_RET_OK) {
		free(epd);
	}
	LEAVE_RELPFUNC;
}

/* we do not return error states, as we are unable to handle them intelligently
 * in any case...
 */
static void
delFromEpollSet(relpEngine_t *pThis, epolld_t *epd)
{
	int r;
	pThis->dbgprint("librelp: delete sock %d from epoll set\n", epd->sock);
	if((r = epoll_ctl(pThis->efd, EPOLL_CTL_DEL, epd->sock, &epd->ev)) != 0) {
		char errStr[1024];
		int eno = errno;
		relpEngineCallOnGenericErr(pThis, "librelp", RELP_RET_ERR_EPOLL_CTL,
			"os error (%d) during EPOLL_CTL_DEL: %s",
			eno, relpEngine_strerror_r(eno, errStr, sizeof(errStr)));
	}
	free(epd);
}

static relpRetVal
addSessToEpoll(relpEngine_t *pThis, relpEngSessLst_t *pSessLstEntry)
{
	addToEpollSet(pThis, epolld_sess, pSessLstEntry,
		relpSessGetSock(pSessLstEntry->pSess), &pSessLstEntry->epevt);
	pSessLstEntry->epollState = epoll_rdonly;
	return RELP_RET_OK;
}

static void
delSessFromEpoll(relpEngine_t *pThis, relpEngSessLst_t *pSessEtry)
{
	delFromEpollSet(pThis, pSessEtry->epevt);
}
#endif

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
#	if defined(HAVE_EPOLL_CREATE1) || defined(HAVE_EPOLL_CREATE)
	addSessToEpoll(pThis, pSessLstEntry);
#	endif

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

#	if defined(HAVE_EPOLL_CREATE1) || defined(HAVE_EPOLL_CREATE)
	delSessFromEpoll(pThis, pSessLstEntry);
#	endif
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



/* create a new listener to be added to the engine. This is the new-style
 * calling convention, which permits us to set various properties before
 * the listener is actually started. New callers should use it. Sequence
 * is:
 * * relpEngineListnerConstruct()
 * * ... set properties ... (via relpSrv...() family)
 * * relgEngineListnerConstructFinalize()
 * rgerhards, 2013-05-14
 */
relpRetVal
relpEngineListnerConstruct(relpEngine_t *pThis, relpSrv_t **ppSrv)
{
	relpSrv_t *pSrv;
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Engine);

	CHKRet(relpSrvConstruct(&pSrv, pThis));
	CHKRet(relpSrvSetFamily(pSrv, pThis->ai_family));
	*ppSrv = pSrv;
finalize_it:
	LEAVE_RELPFUNC;
}
relpRetVal
relpEngineListnerConstructFinalize(relpEngine_t *pThis, relpSrv_t *pSrv)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Engine);
	CHKRet(relpSrvRun(pSrv));
	CHKRet(relpEngineAddToSrvList(pThis, pSrv));
finalize_it:
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

/**
 * Set an event handler that shall receive information when authentication
 * has been failed.
 *
 * Most importantly, this permits the caller to emit error messages on
 * failed authentication. It is also suggested to output the credentials,
 * so that the user knows what to do (e.g. add fingerprint for newly
 * setup peer).
 *
 * This handler will only be called in a mode with authenticaton.
 * Practically, this means when TLS support is enabled.
 * 
 * Callback parameters:
 *
 * pUsr     - the user pointer set
 * authinfo - the credentials that have been used to authenticate
 *            the remote peer. This may be a fingerprint or something 
 *            else, depending on authentication settings.
 * errmsg   - error message as far as librelp is concerned
 * errcode  - contains librelp error status that lead to the failed auth.
 */
relpRetVal
relpEngineSetOnAuthErr(relpEngine_t *pThis, void (*pCB)(void*pUsr, char *authinfo, char*errmsg, relpRetVal errcode) )
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Engine);
	pThis->onAuthErr = pCB;
	LEAVE_RELPFUNC;
}

/**
 * Set an event handler that shall receive information when some error
 * occured for which no special handler exists. Note that even if the
 * special handler (e.g. AuthErr) is not set, this handler here will
 * not be called. This permits the lib-user to set fine-grained control
 * on which error messages it intends to handle.
 *
 * Callback parameters:
 *
 * pUsr     - the user pointer set
 * objinfo  - some information identifying the object in error; depends
 *            on the actual error case.
 * errmsg   - error message as far as librelp is concerned
 * errcode  - contains librelp error status
 */
relpRetVal
relpEngineSetOnErr(relpEngine_t *pThis, void (*pCB)(void*pUsr, char *objinfo, char*errmsg, relpRetVal errcode) )
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Engine);
	pThis->onErr = pCB;
	LEAVE_RELPFUNC;
}

/**
 * Set an event handler that shall receive information when some GENERIC 
 * error occured for which no special handler exists. A generic error is
 * one that cannot be assigned to a specific listener or session.
 * Callback parameters:
 *
 * objinfo  - some information identifying the object in error; depends
 *            on the actual error case.
 * errmsg   - error message as far as librelp is concerned
 * errcode  - contains librelp error status
 */
relpRetVal
relpEngineSetOnGenericErr(relpEngine_t *pThis, void (*pCB)(char *objinfo, char*errmsg, relpRetVal errcode) )
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Engine);
	pThis->onGenericErr = pCB;
	LEAVE_RELPFUNC;
}

/* Deprecated, use relpEngineListnerConstruct() family of functions.
 * See there for further information.
 */
relpRetVal
relpEngineAddListner2(relpEngine_t *pThis, unsigned char *pLstnPort, void *pUsr)
{
	relpSrv_t *pSrv;
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Engine);
	CHKRet(relpEngineListnerConstruct(pThis, &pSrv));
	CHKRet(relpSrvSetUsrPtr(pSrv, pUsr));
	CHKRet(relpSrvSetLstnPort(pSrv, pLstnPort));
	CHKRet(relpEngineListnerConstructFinalize(pThis, pSrv));
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

/* Deprecated, use relpEngineListnerConstruct() family of functions.
 * See there for further information.
 */
relpRetVal
relpEngineAddListner(relpEngine_t *pThis, unsigned char *pLstnPort)
{
	relpSrv_t *pSrv;
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Engine);
	CHKRet(relpEngineListnerConstruct(pThis, &pSrv));
	CHKRet(relpSrvSetLstnPort(pSrv, pLstnPort));
	CHKRet(relpEngineListnerConstructFinalize(pThis, pSrv));
finalize_it:
	LEAVE_RELPFUNC;
}


/* the setStop method sets a flag that stops the server after the next select.
 * In order to interrupt the select(), it is suggested to send a signal. If no
 * signal is sent, it may take rather long to stop the server (until another
 * machine sends data).
 */
relpRetVal relpEngineSetStop(relpEngine_t *pThis)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Engine);
	pThis->bStop = 1;
	LEAVE_RELPFUNC;
}


/* set the socket family to use
 */
relpRetVal relpEngineSetFamily(relpEngine_t *pThis, int ai_family)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Engine);
	pThis->ai_family = ai_family;
	LEAVE_RELPFUNC;
}

/* helper for relpEngineRun; receives data when it is time to
 * do so. Includes all housekeeping, like closing the session.
 */
static inline relpRetVal
doRecv(relpEngine_t *pThis, relpEngSessLst_t *pSessEtry, int sock)
{
	relpRetVal localRet;
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
	return localRet;
}
/* helper for relpEngineRun; sends session data when it is time
 * to send. Includes all housekeeping, like closing the session.
 */
static inline void
doSend(relpEngine_t *pThis, relpEngSessLst_t *pSessEtry, int sock)
{
	relpRetVal localRet;
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

static void
handleConnectionRequest(relpEngine_t *pThis, relpSrv_t *pSrv, int sock)
{
	relpRetVal localRet;
	relpSess_t *pNewSess;

	pThis->dbgprint("new connect on RELP socket #%d\n", sock);
	localRet = relpSessAcceptAndConstruct(&pNewSess, pSrv, sock);
	if(localRet == RELP_RET_OK) {
		relpEngineAddToSess(pThis, pNewSess);
	}
}


#if defined(HAVE_EPOLL_CREATE1) || defined(HAVE_EPOLL_CREATE)

static relpRetVal
epoll_set_events(relpEngine_t *pThis, relpEngSessLst_t *pSessEtry, int sock, uint32_t events)
{
	ENTER_RELPFUNC;
	/* TODO: remove the status dbgprint's once we have some practice drill 2013-07-05 */
	pThis->dbgprint("librelp: epoll_set_events sock %d, target bits %2.2x, current %2.2x\n", sock, events, pSessEtry->epevt->ev.events);
	if(pSessEtry->epevt->ev.events != events) {
		pSessEtry->epevt->ev.events = events;
		pThis->dbgprint("librelp: epoll_set_events sock %d, setting new bits\n", sock);
		if(epoll_ctl(pThis->efd, EPOLL_CTL_MOD, sock, &pSessEtry->epevt->ev) != 0) {
			char errStr[1024];
			int eno = errno;
			relpEngineCallOnGenericErr(pThis, "librelp", RELP_RET_ERR_EPOLL_CTL,
				"os error (%d) during EPOLL_CTL_MOD: %s",
				eno, relpEngine_strerror_r(eno, errStr, sizeof(errStr)));
			ABORT_FINALIZE(RELP_RET_ERR_EPOLL_CTL);
		}
	}
finalize_it:
	LEAVE_RELPFUNC;
}

static inline relpRetVal
engineEventLoopInit(relpEngine_t __attribute__((unused)) *pThis)
{
#	define NUM_EPOLL_EVENTS 10
	relpEngSrvLst_t *pSrvEtry;
	int i;
	int nLstn;
	int sock;
	ENTER_RELPFUNC;
#if defined(EPOLL_CLOEXEC) && defined(HAVE_EPOLL_CREATE1)
	pThis->efd = epoll_create1(EPOLL_CLOEXEC);
	if(pThis->efd < 0 && errno == ENOSYS)
#endif
	{
		pThis->efd = epoll_create(NUM_EPOLL_EVENTS);
	}

	if(pThis->efd < 0) {
		pThis->dbgprint("epoll_create1() could not create fd\n");
		ABORT_FINALIZE(RELP_RET_IO_ERR);
	}

	/* Add the listen sockets to the epoll set. These
	 * never change, so we can do it just once in init.
	 */
	for(pSrvEtry = pThis->pSrvLstRoot ; pSrvEtry != NULL ; pSrvEtry = pSrvEtry->pNext) {
		nLstn = relpSrvGetNumLstnSocks(pSrvEtry->pSrv);
		CHKmalloc(pSrvEtry->epevts = malloc(sizeof(epolld_t) * nLstn));
		for(i = 0 ; i < nLstn ; ++i) {
			sock = relpSrvGetLstnSock(pSrvEtry->pSrv, i+1);
			addToEpollSet(pThis, epolld_lstn, pSrvEtry->pSrv, sock, &(pSrvEtry->epevts[i]));
		}
	}
finalize_it:
	LEAVE_RELPFUNC;
}
static inline relpRetVal
engineEventLoopExit(relpEngine_t __attribute__((unused)) *pThis)
{
	relpEngSrvLst_t *pSrvEtry;
	int i;
	int nLstn;
	ENTER_RELPFUNC;
	for(pSrvEtry = pThis->pSrvLstRoot ; pSrvEtry != NULL ; pSrvEtry = pSrvEtry->pNext) {
		nLstn = relpSrvGetNumLstnSocks(pSrvEtry->pSrv);
		for(i = 0 ; i < nLstn ; ++i) {
			delFromEpollSet(pThis, pSrvEtry->epevts[i]);
		}
		free(pSrvEtry->epevts);
	}
	if(pThis->efd != -1) {
		close(pThis->efd);
		pThis->efd = -1;
	}
	LEAVE_RELPFUNC;
}

static relpRetVal
handleSessIO(relpEngine_t *pThis, epolld_t *epd)
{
	relpEngSessLst_t *pSessEtry;
	relpTcp_t *pTcp;
#	ifdef ENABLE_TLS
	relpRetVal localRet;
#	endif

	pSessEtry = (relpEngSessLst_t*) epd->ptr;
	if(relpSessTcpRequiresRtry(pSessEtry->pSess)) {
		pTcp = pSessEtry->pSess->pTcp;
		if(relpTcpRtryOp(pTcp) == relpTCP_RETRY_send) {
			doSend(pThis, pSessEtry, epd->sock);
		} else if(relpTcpRtryOp(pTcp) == relpTCP_RETRY_recv) {
			doRecv(pThis, pSessEtry, epd->sock);
		} else {
#			ifdef ENABLE_TLS
				localRet = relpTcpRtryHandshake(pTcp);
				if(localRet != RELP_RET_OK) {
					pThis->dbgprint("relp session %d handshake iRet %d, tearing it down\n",
							epd->sock, localRet);
					relpEngineDelSess(pThis, pSessEtry);
				}
#			else
					pThis->dbgprint("librelp error: handshake retry requested in "
							"non-TLS mode");
				
#			endif /* #ifdef ENABLE_TLS */
		}
	} else {
		if(doRecv(pThis, pSessEtry, epd->sock) == RELP_RET_OK) {
			if(epd->ev.events & EPOLLOUT) {
				doSend(pThis, pSessEtry, epd->sock);
			}
		}
	}
	return RELP_RET_OK;
}

static relpRetVal
engineEventLoopRun(relpEngine_t *pThis)
{
	relpEngSessLst_t *pSessEtry;
	int i;
	int sock;
	struct epoll_event events[128];
	epolld_t *epd;
	int nEvents;

	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Engine);

	pThis->bStop = 0;
	while(!relpEngineShouldStop(pThis)) {
		/* very naive implementation, O(n) - can change this once things work...
		 * But even the naive implementation is better than select, e.g. has no
		 * limit on the number of sockets.
		 */
		for(pSessEtry = pThis->pSessLstRoot ; pSessEtry != NULL ; pSessEtry = pSessEtry->pNext) {
			sock = relpSessGetSock(pSessEtry->pSess);
#			ifdef ENABLE_TLS
			if(relpSessTcpRequiresRtry(pSessEtry->pSess)) {
				pThis->dbgprint("librelp: retry op requested for sock %d\n", sock);
				if(relpTcpGetRtryDirection(pSessEtry->pSess->pTcp) == 0) {
					epoll_set_events(pThis, pSessEtry, sock, EPOLLIN);
				} else {
					epoll_set_events(pThis, pSessEtry, sock, EPOLLOUT);
				}
			} else
#			endif /* #ifdef ENABLE_TLS */
			{
				/* now check if a send request is outstanding and, if so, add it */
				if(relpSendqIsEmpty(pSessEtry->pSess->pSendq)) {
					epoll_set_events(pThis, pSessEtry, sock, EPOLLIN);
				} else {
					epoll_set_events(pThis, pSessEtry, sock, EPOLLIN | EPOLLOUT);
				}
			}
		}

		/* wait for io to become ready */
		if(relpEngineShouldStop(pThis)) break;
		pThis->dbgprint("librelp: doing epoll_wait\n");
		nEvents = epoll_wait(pThis->efd, events, sizeof(events)/sizeof(struct epoll_event), -1);
		pThis->dbgprint("librelp: done epoll_wait, nEvents:%d\n", nEvents);
		if(relpEngineShouldStop(pThis)) break;

		for(i = 0 ; i < nEvents ; ++i) {
			if(relpEngineShouldStop(pThis)) break;
			epd = (epolld_t*) events[i].data.ptr;
			switch(epd->typ) {
			case epolld_lstn:
				handleConnectionRequest(pThis, epd->ptr, epd->sock);
				break;
			case epolld_sess:
				handleSessIO(pThis, epd);
				break;
			default:
				relpEngineCallOnGenericErr(pThis, "librelp", RELP_RET_ERR_INTERNAL,
					"invalid epolld_type_t %d after epoll", epd->typ);
				break;
			}
		}
	}

	LEAVE_RELPFUNC;
}
#else /* no epoll support available */
static inline relpRetVal engineEventLoopInit(relpEngine_t __attribute__((unused)) *pThis) { return RELP_RET_OK; }
static inline relpRetVal engineEventLoopExit(relpEngine_t __attribute__((unused)) *pThis) { return RELP_RET_OK; }
static relpRetVal
engineEventLoopRun(relpEngine_t *pThis)
{
	relpEngSrvLst_t *pSrvEtry;
	relpEngSessLst_t *pSessEtry;
	relpEngSessLst_t *pSessEtryNext;
	relpTcp_t *pTcp;
	relpRetVal localRet;
	int iSocks;
	int sock;
	int maxfds;
	int nfds;
	fd_set readfds;
	fd_set writefds;

	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Engine);

	pThis->bStop = 0;
	while(!relpEngineShouldStop(pThis)) {
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
#			ifdef ENABLE_TLS
			if(relpSessTcpRequiresRtry(pSessEtry->pSess)) {
				pThis->dbgprint("librelp: retry op requested for sock %d\n", sock);
				if(relpTcpGetRtryDirection(pSessEtry->pSess->pTcp) == 0) {
					FD_SET(sock, &readfds);
				} else {
					FD_SET(sock, &writefds);
				}
			} else
#			endif /* #ifdef ENABLE_TLS */
			{
				FD_SET(sock, &readfds);
				/* now check if a send request is outstanding and, if so, add it */
				if(!relpSendqIsEmpty(pSessEtry->pSess->pSendq)) {
					FD_SET(sock, &writefds);
				}
			}
			if(sock > maxfds) maxfds = sock;
		}

		/* done adding all sockets */
		if(pThis->dbgprint != dbgprintDummy) {
			pThis->dbgprint("librelp: calling select, active file descriptors (max %d): ", maxfds);
			for(nfds = 0; nfds <= maxfds; ++nfds)
				if(FD_ISSET(nfds, &readfds))
					pThis->dbgprint("%d ", nfds);
			pThis->dbgprint("\n");
		}

		/* wait for io to become ready */
		if(relpEngineShouldStop(pThis)) break;
		nfds = select(maxfds+1, (fd_set *) &readfds, &writefds, NULL, NULL);
		pThis->dbgprint("relp select returns, nfds %d\n", nfds);
		if(relpEngineShouldStop(pThis)) break;

		if(nfds == -1) {
			if(errno == EINTR) {
				pThis->dbgprint("relp select interrupted\n");
			} else {
				pThis->dbgprint("relp select returned error %d\n", errno);
			}
			continue;
		}
	
		/* and then start again with the servers (new connection request) */
		for(pSrvEtry = pThis->pSrvLstRoot ; nfds && pSrvEtry != NULL ; pSrvEtry = pSrvEtry->pNext) {
			for(iSocks = 1 ; nfds && iSocks <= relpSrvGetNumLstnSocks(pSrvEtry->pSrv) ; ++iSocks) {
				if(relpEngineShouldStop(pThis)) break;
				sock = relpSrvGetLstnSock(pSrvEtry->pSrv, iSocks);
				if(FD_ISSET(sock, &readfds)) {
					handleConnectionRequest(pThis, pSrvEtry->pSrv, sock);
					--nfds; /* indicate we have processed one */
				}
			}
		}

		/* now check if we have some action waiting for sessions */
		for(pSessEtry = pThis->pSessLstRoot ; nfds && pSessEtry != NULL ; pSessEtry = pSessEtryNext) {
			if(relpEngineShouldStop(pThis)) break;
			pSessEtryNext = pSessEtry->pNext; /* we need to cache this as we may delete the entry! */
			sock = relpSessGetSock(pSessEtry->pSess);
			if(relpSessTcpRequiresRtry(pSessEtry->pSess)) {
				pTcp = pSessEtry->pSess->pTcp;
				if(FD_ISSET(sock, &readfds) || FD_ISSET(sock, &writefds)) {
					if(relpTcpRtryOp(pTcp) == relpTCP_RETRY_send) {
						doSend(pThis, pSessEtry, sock);
						--nfds; /* indicate we have processed one */
					} else if(relpTcpRtryOp(pTcp) == relpTCP_RETRY_recv) {
						doRecv(pThis, pSessEtry, sock);
						--nfds; /* indicate we have processed one */
					} else {
#						ifdef ENABLE_TLS
							localRet = relpTcpRtryHandshake(pSessEtry->pSess->pTcp);
							if(localRet != RELP_RET_OK) {
								pThis->dbgprint("relp session %d handshake iRet %d, tearing it down\n",
										sock, localRet);
								relpEngineDelSess(pThis, pSessEtry);
							}
#						else
							pThis->dbgprint("librelp error: handshake retry requested in "
									"non-TLS mode");
				
#						endif /* #ifdef ENABLE_TLS */
					}
				}
			} else {
				if(FD_ISSET(sock, &readfds)) {
					if(doRecv(pThis, pSessEtry, sock) != RELP_RET_OK)
						continue; /* else write may cause invld mem access! */
					--nfds; /* indicate we have processed one */
				}
				if(FD_ISSET(sock, &writefds)) {
					doSend(pThis, pSessEtry, sock);
					--nfds; /* indicate we have processed one */
				}
			}
		}

	}

	LEAVE_RELPFUNC;
}
#endif /* epoll/select support */

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
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Engine);

	CHKRet(engineEventLoopInit(pThis));
	engineEventLoopRun(pThis);
	engineEventLoopExit(pThis);
finalize_it:
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
	 * loaded and, when so, should come from a linked list.
	 * NOTE: the command handler are sorted so that most frequently used are
	 * at the top of the list!
	 */
	if(!strcmp((char*)pFrame->cmd, "syslog")) {
		CHKRet(relpSCSyslog(pFrame, pSess));
	} else if(!strcmp((char*)pFrame->cmd, "rsp")) {
		CHKRet(relpSCRsp(pFrame, pSess));
	} else if(!strcmp((char*)pFrame->cmd, "open")) {
		CHKRet(relpSCInit(pFrame, pSess));
	} else if(!strcmp((char*)pFrame->cmd, "close")) {
		CHKRet(relpSCClose(pFrame, pSess));
	} else if(!strcmp((char*)pFrame->cmd, "serverclose")) {
		CHKRet(relpCCServerclose(pFrame, pSess));
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
	pThis->dbgprint("relp engine created new client %p\n", *ppClt);

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

	pThis->dbgprint("relp engine destructing client %p\n", *ppClt);
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

void
relpEngineSetShutdownImmdtPtr(relpEngine_t *pThis, int *ptr)
{
	if(pThis->bShutdownImmdt != ptr)
		pThis->bShutdownImmdt = ptr;
}

/* Enable or disable a command. Note that a command can not be enabled once
 * it has been set to forbidden! There will be no error return state in this
 * case. -- rgerhards, 2008-03-27
 */
relpRetVal
relpEngineSetEnableCmd(relpEngine_t *pThis, unsigned char *pszCmd, relpCmdEnaState_t stateCmd)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Engine);
	assert(pszCmd != NULL);

	if(!strcmp((char*)pszCmd, "syslog")) {
		if(pThis->stateCmdSyslog != eRelpCmdState_Forbidden)
			pThis->stateCmdSyslog = stateCmd;
	} else {
		pThis->dbgprint("tried to set unknown command '%s' to %d\n", pszCmd, stateCmd);
		ABORT_FINALIZE(RELP_RET_UNKNOWN_CMD);
	}

finalize_it:
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
