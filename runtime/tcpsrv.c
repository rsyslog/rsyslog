/* tcpsrv.c
 *
 * Common code for plain TCP syslog based servers. This is currently being
 * utilized by imtcp and imgssapi.
 *
 * NOTE: this is *not* a generic TCP server, but one for syslog servers. For
 *       generic stream servers, please use ./runtime/strmsrv.c!
 *
 * There are actually two classes within the tcpserver code: one is
 * the tcpsrv itself, the other one is its sessions. This is a helper
 * class to tcpsrv.
 *
 * The common code here calls upon specific functionality by using
 * callbacks. The specialised input modules need to set the proper
 * callbacks before the code is run. The tcpsrv then calls back
 * into the specific input modules at the appropriate time.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * File begun on 2007-12-21 by RGerhards (extracted from syslogd.c[which was
 * licensed under BSD at the time of the rsyslog fork])
 *
 * Copyright 2007-2025 Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *       -or-
 *       see COPYING.ASL20 in the source distribution
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <ctype.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/socket.h>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include "rsyslog.h"
#include "dirty.h"
#include "cfsysline.h"
#include "module-template.h"
#include "net.h"
#include "srUtils.h"
#include "conf.h"
#include "tcpsrv.h"
#include "obj.h"
#include "glbl.h"
#include "netstrms.h"
#include "netstrm.h"
#include "nssel.h"
#include "nspoll.h"
#include "errmsg.h"
#include "ruleset.h"
#include "ratelimit.h"
#include "unicode-helper.h"
#include "nsd_ptcp.h"

PRAGMA_INGORE_Wswitch_enum
MODULE_TYPE_LIB
MODULE_TYPE_NOKEEP

/* defines */
#define TCPSESS_MAX_DEFAULT 200 /* default for nbr of tcp sessions if no number is given */
#define TCPLSTN_MAX_DEFAULT 20 /* default for nbr of listeners */

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(conf)
DEFobjCurrIf(glbl)
DEFobjCurrIf(ruleset)
DEFobjCurrIf(tcps_sess)
DEFobjCurrIf(net)
DEFobjCurrIf(netstrms)
DEFobjCurrIf(netstrm)
DEFobjCurrIf(nssel)
DEFobjCurrIf(nspoll)
DEFobjCurrIf(prop)
DEFobjCurrIf(statsobj)

static void startWorkerPool(tcpsrv_t *const pThis);
static void stopWorkerPool(tcpsrv_t *const pThis);


/* add new listener port to listener port list
 * rgerhards, 2009-05-21
 */
static rsRetVal ATTR_NONNULL(1, 2)
addNewLstnPort(tcpsrv_t *const pThis, tcpLstnParams_t *const cnf_params)
{
	tcpLstnPortList_t *pEntry;
	uchar statname[64];
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, tcpsrv);

	/* create entry */
	CHKmalloc(pEntry = (tcpLstnPortList_t*)calloc(1, sizeof(tcpLstnPortList_t)));
	pEntry->cnf_params = cnf_params;

	strcpy((char*)pEntry->cnf_params->dfltTZ, (char*)pThis->dfltTZ);
	pEntry->cnf_params->bSPFramingFix = pThis->bSPFramingFix;
	pEntry->cnf_params->bPreserveCase = pThis->bPreserveCase;
	pEntry->pSrv = pThis;


	/* support statistics gathering */
	CHKiRet(ratelimitNew(&pEntry->ratelimiter, "tcperver", NULL));
	ratelimitSetLinuxLike(pEntry->ratelimiter, pThis->ratelimitInterval, pThis->ratelimitBurst);
	ratelimitSetThreadSafe(pEntry->ratelimiter);

	CHKiRet(statsobj.Construct(&(pEntry->stats)));
	snprintf((char*)statname, sizeof(statname), "%s(%s)", cnf_params->pszInputName, cnf_params->pszPort);
	statname[sizeof(statname)-1] = '\0'; /* just to be on the save side... */
	CHKiRet(statsobj.SetName(pEntry->stats, statname));
	CHKiRet(statsobj.SetOrigin(pEntry->stats, pThis->pszOrigin));
	STATSCOUNTER_INIT(pEntry->ctrSubmit, pEntry->mutCtrSubmit);
	CHKiRet(statsobj.AddCounter(pEntry->stats, UCHAR_CONSTANT("submitted"),
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &(pEntry->ctrSubmit)));
	CHKiRet(statsobj.ConstructFinalize(pEntry->stats));

	/* all OK - add to list */
	pEntry->pNext = pThis->pLstnPorts;
	pThis->pLstnPorts = pEntry;

finalize_it:
	if(iRet != RS_RET_OK) {
		if(pEntry != NULL) {
			if(pEntry->cnf_params->pInputName != NULL) {
				prop.Destruct(&pEntry->cnf_params->pInputName);
			}
			if(pEntry->ratelimiter != NULL) {
				ratelimitDestruct(pEntry->ratelimiter);
			}
			if(pEntry->stats != NULL) {
				statsobj.Destruct(&pEntry->stats);
			}
			free(pEntry);
		}
	}

	RETiRet;
}


/* configure TCP listener settings.
 * Note: pszPort is handed over to us - the caller MUST NOT free it!
 * rgerhards, 2008-03-20
 */
static rsRetVal ATTR_NONNULL(1,2)
configureTCPListen(tcpsrv_t *const pThis, tcpLstnParams_t *const cnf_params)
{
	assert(cnf_params->pszPort != NULL);
	int i;
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, tcpsrv);

	/* extract port */
	const uchar *pPort = cnf_params->pszPort;
	i = 0;
	while(isdigit((int) *pPort)) {
		i = i * 10 + *pPort++ - '0';
	}

	if(i >= 0 && i <= 65535) {
		CHKiRet(addNewLstnPort(pThis, cnf_params));
	} else {
		LogError(0, NO_ERRCODE, "Invalid TCP listen port %s - ignored.\n", cnf_params->pszPort);
	}

finalize_it:
	RETiRet;
}


/* Initialize the session table
 * returns 0 if OK, somewhat else otherwise
 */
static rsRetVal
TCPSessTblInit(tcpsrv_t *const pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, tcpsrv);
	assert(pThis->pSessions == NULL);

	DBGPRINTF("Allocating buffer for %d TCP sessions.\n", pThis->iSessMax);
	if((pThis->pSessions = (tcps_sess_t **) calloc(pThis->iSessMax, sizeof(tcps_sess_t *))) == NULL) {
		DBGPRINTF("Error: TCPSessInit() could not alloc memory for TCP session table.\n");
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}

finalize_it:
	RETiRet;
}


/* find a free spot in the session table. If the table
 * is full, -1 is returned, else the index of the free
 * entry (0 or higher).
 */
static int
TCPSessTblFindFreeSpot(tcpsrv_t *const pThis)
{
	register int i;

	ISOBJ_TYPE_assert(pThis, tcpsrv);

	for(i = 0 ; i < pThis->iSessMax ; ++i) {
		if(pThis->pSessions[i] == NULL)
			break;
	}

	return((i < pThis->iSessMax) ? i : -1);
}


/* Get the next session index. Free session tables entries are
 * skipped. This function is provided the index of the last
 * session entry, or -1 if no previous entry was obtained. It
 * returns the index of the next session or -1, if there is no
 * further entry in the table. Please note that the initial call
 * might as well return -1, if there is no session at all in the
 * session table.
 */
static int
TCPSessGetNxtSess(tcpsrv_t *pThis, int iCurr)
{
	register int i;

	ISOBJ_TYPE_assert(pThis, tcpsrv);
	assert(pThis->pSessions != NULL);
	for(i = iCurr + 1 ; i < pThis->iSessMax ; ++i) {
		if(pThis->pSessions[i] != NULL)
			break;
	}

	return((i < pThis->iSessMax) ? i : -1);
}


/* De-Initialize TCP listner sockets.
 * This function deinitializes everything, including freeing the
 * session table. No TCP listen receive operations are permitted
 * unless the subsystem is reinitialized.
 * rgerhards, 2007-06-21
 */
static void ATTR_NONNULL()
deinit_tcp_listener(tcpsrv_t *const pThis)
{
	int i;
	tcpLstnPortList_t *pEntry;
	tcpLstnPortList_t *pDel;

	ISOBJ_TYPE_assert(pThis, tcpsrv);

	if(pThis->pSessions != NULL) {
		/* close all TCP connections! */
		if(!pThis->bUsingEPoll) {
			i = TCPSessGetNxtSess(pThis, -1);
			while(i != -1) {
				tcps_sess.Destruct(&pThis->pSessions[i]);
				/* now get next... */
				i = TCPSessGetNxtSess(pThis, i);
			}
		}

		/* we are done with the session table - so get rid of it...  */
		free(pThis->pSessions);
		pThis->pSessions = NULL; /* just to make sure... */
	}

	/* free list of tcp listen ports */
	pEntry = pThis->pLstnPorts;
	while(pEntry != NULL) {
		prop.Destruct(&pEntry->cnf_params->pInputName);
		free((void*)pEntry->cnf_params->pszInputName);
		free((void*)pEntry->cnf_params->pszPort);
		free((void*)pEntry->cnf_params->pszAddr);
		free((void*)pEntry->cnf_params->pszLstnPortFileName);
		free((void*)pEntry->cnf_params);
		ratelimitDestruct(pEntry->ratelimiter);
		statsobj.Destruct(&(pEntry->stats));
		pDel = pEntry;
		pEntry = pEntry->pNext;
		free(pDel);
	}

	/* finally close our listen streams */
	for(i = 0 ; i < pThis->iLstnCurr ; ++i) {
		netstrm.Destruct(pThis->ppLstn + i);
	}
}


/* add a listen socket to our listen socket array. This is a callback
 * invoked from the netstrm class. -- rgerhards, 2008-04-23
 */
static rsRetVal
addTcpLstn(void *pUsr, netstrm_t *const pLstn)
{
	tcpLstnPortList_t *pPortList = (tcpLstnPortList_t *) pUsr;
	tcpsrv_t *pThis = pPortList->pSrv;
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, tcpsrv);
	ISOBJ_TYPE_assert(pLstn, netstrm);

	if(pThis->iLstnCurr >= pThis->iLstnMax)
		ABORT_FINALIZE(RS_RET_MAX_LSTN_REACHED);

	pThis->ppLstn[pThis->iLstnCurr] = pLstn;
	pThis->ppLstnPort[pThis->iLstnCurr] = pPortList;
	++pThis->iLstnCurr;

finalize_it:
	RETiRet;
}


/* Initialize TCP listener socket for a single port
 * Note: at this point, TLS vs. non-TLS does not matter; TLS params are
 * set on connect!
 * rgerhards, 2009-05-21
 */
static rsRetVal
initTCPListener(tcpsrv_t *pThis, tcpLstnPortList_t *pPortEntry)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, tcpsrv);
	assert(pPortEntry != NULL);

	// pPortEntry->pszAddr = NULL ==> bind to all interfaces
	CHKiRet(netstrm.LstnInit(pThis->pNS, (void*)pPortEntry, addTcpLstn,
		pThis->iSessMax, pPortEntry->cnf_params));

finalize_it:
	RETiRet;
}


/* Initialize TCP sockets (for listener) and listens on them */
static rsRetVal
create_tcp_socket(tcpsrv_t *const pThis)
{
	DEFiRet;
	rsRetVal localRet;
	tcpLstnPortList_t *pEntry;

	ISOBJ_TYPE_assert(pThis, tcpsrv);

	/* init all configured ports */
	pEntry = pThis->pLstnPorts;
	while(pEntry != NULL) {
		localRet = initTCPListener(pThis, pEntry);
		if(localRet != RS_RET_OK) {
			LogError(0, localRet, "Could not create tcp listener, ignoring port "
			"%s bind-address %s.",
			(pEntry->cnf_params->pszPort == NULL) ? "**UNSPECIFIED**"
				: (const char*) pEntry->cnf_params->pszPort,
			(pEntry->cnf_params->pszAddr == NULL) ? "**UNSPECIFIED**"
				: (const char*)pEntry->cnf_params->pszAddr);
		}
		pEntry = pEntry->pNext;
	}

	/* OK, we had success. Now it is also time to
	 * initialize our connections
	 */
	if(TCPSessTblInit(pThis) != 0) {
		/* OK, we are in some trouble - we could not initialize the
		 * session table, so we can not continue. We need to free all
		 * we have assigned so far, because we can not really use it...
		 */
		LogError(0, RS_RET_ERR, "Could not initialize TCP session table, suspending TCP "
				"message reception.");
		ABORT_FINALIZE(RS_RET_ERR);
	}

finalize_it:
	RETiRet;
}


/* Accept new TCP connection; make entry in session table. If there
 * is no more space left in the connection table, the new TCP
 * connection is immediately dropped.
 * ppSess has a pointer to the newly created session, if it succeeds.
 * If it does not succeed, no session is created and ppSess is
 * undefined. If the user has provided an OnSessAccept Callback,
 * this one is executed immediately after creation of the
 * session object, so that it can do its own initialization.
 * rgerhards, 2008-03-02
 */
static rsRetVal
SessAccept(tcpsrv_t *const pThis, tcpLstnPortList_t *const pLstnInfo, tcps_sess_t **ppSess, netstrm_t *pStrm)
{
	DEFiRet;
	tcps_sess_t *pSess = NULL;
	netstrm_t *pNewStrm = NULL;
	const tcpLstnParams_t *const cnf_params = pLstnInfo->cnf_params;
	int iSess = -1;
	struct sockaddr_storage *addr;
	uchar *fromHostFQDN = NULL;
	prop_t *fromHostIP = NULL;

	ISOBJ_TYPE_assert(pThis, tcpsrv);
	assert(pLstnInfo != NULL);

	CHKiRet(netstrm.AcceptConnReq(pStrm, &pNewStrm));

	/* Add to session list */
	iSess = TCPSessTblFindFreeSpot(pThis);
	if(iSess == -1) {
		errno = 0;
		LogError(0, RS_RET_MAX_SESS_REACHED, "too many tcp sessions - dropping incoming request");
		ABORT_FINALIZE(RS_RET_MAX_SESS_REACHED);
	}

	if(pThis->bUseKeepAlive) {
	        CHKiRet(netstrm.SetKeepAliveProbes(pNewStrm, pThis->iKeepAliveProbes));
	        CHKiRet(netstrm.SetKeepAliveTime(pNewStrm, pThis->iKeepAliveTime));
	        CHKiRet(netstrm.SetKeepAliveIntvl(pNewStrm, pThis->iKeepAliveIntvl));
		CHKiRet(netstrm.EnableKeepAlive(pNewStrm));
	}

	/* we found a free spot and can construct our session object */
	if(pThis->gnutlsPriorityString != NULL) {
		CHKiRet(netstrm.SetGnutlsPriorityString(pNewStrm, pThis->gnutlsPriorityString));
	}
	CHKiRet(tcps_sess.Construct(&pSess));
	CHKiRet(tcps_sess.SetTcpsrv(pSess, pThis));
	CHKiRet(tcps_sess.SetLstnInfo(pSess, pLstnInfo));
	if(pThis->OnMsgReceive != NULL)
		CHKiRet(tcps_sess.SetOnMsgReceive(pSess, pThis->OnMsgReceive));

	/* get the host name */
	CHKiRet(netstrm.GetRemoteHName(pNewStrm, &fromHostFQDN));
	if (!cnf_params->bPreserveCase) {
		/* preserve_case = off */
		uchar *p;
		for(p = fromHostFQDN; *p; p++) {
			if (isupper((int) *p)) {
				*p = tolower((int) *p);
			}
		}
	}
	CHKiRet(netstrm.GetRemoteIP(pNewStrm, &fromHostIP));
	CHKiRet(netstrm.GetRemAddr(pNewStrm, &addr));
	/* TODO: check if we need to strip the domain name here -- rgerhards, 2008-04-24 */

	/* Here we check if a host is permitted to send us messages. If it isn't, we do not further
	 * process the message but log a warning (if we are configured to do this).
	 * rgerhards, 2005-09-26
	 */
	if(!pThis->pIsPermittedHost((struct sockaddr*) addr, (char*) fromHostFQDN, pThis->pUsr, pSess->pUsr)) {
		DBGPRINTF("%s is not an allowed sender\n", fromHostFQDN);
		if(glbl.GetOptionDisallowWarning(runConf)) {
			errno = 0;
			LogError(0, RS_RET_HOST_NOT_PERMITTED, "connection request from disallowed "
					"sender %s discarded", fromHostFQDN);
		}
		ABORT_FINALIZE(RS_RET_HOST_NOT_PERMITTED);
	}

	/* OK, we have an allowed sender, so let's continue, what
	 * means we can finally fill in the session object.
	 */
	CHKiRet(tcps_sess.SetHost(pSess, fromHostFQDN));
	fromHostFQDN = NULL; /* we handed this string over */
	CHKiRet(tcps_sess.SetHostIP(pSess, fromHostIP));
	CHKiRet(tcps_sess.SetStrm(pSess, pNewStrm));
	pNewStrm = NULL; /* prevent it from being freed in error handler, now done in tcps_sess! */
	CHKiRet(tcps_sess.SetMsgIdx(pSess, 0));
	CHKiRet(tcps_sess.ConstructFinalize(pSess));

	/* check if we need to call our callback */
	if(pThis->pOnSessAccept != NULL) {
		CHKiRet(pThis->pOnSessAccept(pThis, pSess));
	}

	*ppSess = pSess;
	if(!pThis->bUsingEPoll)
		pThis->pSessions[iSess] = pSess;
	pSess = NULL; /* this is now also handed over */

	if(pThis->bEmitMsgOnOpen) {
		LogMsg(0, RS_RET_NO_ERRCODE, LOG_INFO,
			"imtcp: connection established with host: %s",
			propGetSzStr(fromHostIP));
	}

finalize_it:
	if(iRet != RS_RET_OK) {
		if(iRet != RS_RET_HOST_NOT_PERMITTED && pThis->bEmitMsgOnOpen) {
			LogError(0, NO_ERRCODE, "imtcp: connection could not be "
				"established with host: %s",
				fromHostIP == NULL ? "(IP unknown)"
					: (const char*)propGetSzStr(fromHostIP));
		}
		if(pSess != NULL)
			tcps_sess.Destruct(&pSess);
		if(pNewStrm != NULL)
			netstrm.Destruct(&pNewStrm);
		free(fromHostFQDN);
	}

	RETiRet;
}


struct runCancelCleanupData_s {
	tcpsrv_t *pThis;
	nspoll_t **ppPoll;
};
static void
RunCancelCleanup(void *arg)
{
	struct runCancelCleanupData_s *const mydata = (struct runCancelCleanupData_s *) arg;
	nspoll_t **ppPoll = mydata->ppPoll;
	tcpsrv_t *const pThis = mydata->pThis;

	if (*ppPoll != NULL)
		nspoll.Destruct(ppPoll);

	/* Wait for any running workers to finish */
	pthread_mutex_lock(&pThis->wrkrMut);
	DBGPRINTF("tcpsrv terminating, waiting for %d workers\n", pThis->wrkrRunning);
	while(pThis->wrkrRunning > 0) {
		pthread_cond_wait(&pThis->wrkrIdle, &pThis->wrkrMut);
	}
	pthread_mutex_unlock(&pThis->wrkrMut);
}

static void
RunSelectCancelCleanup(void *arg)
{
	nssel_t **ppSel = (nssel_t**) arg;

	if(*ppSel != NULL)
		nssel.Destruct(ppSel);
}


/* helper to close a session. Takes status of poll vs. select into consideration.
 * rgerhards, 2009-11-25
 */
static rsRetVal
closeSess(tcpsrv_t *const pThis, tcpsrv_io_descr_t *const pioDescr, nspoll_t *const pPoll) {
	DEFiRet;
	tcps_sess_t *pSess = pioDescr->ptr.pSess;
	if(pPoll != NULL) {
		CHKiRet(nspoll.Ctl(pPoll, pioDescr, NSDPOLL_IN, NSDPOLL_DEL));
	}
	pThis->pOnRegularClose(pSess);
	tcps_sess.Destruct(&pSess);
finalize_it:
	if(pPoll == NULL) {
		pThis->pSessions[pioDescr->id] = NULL;
	} else {
		/* in epoll mode, ioDescr is dynamically allocated */
		free(pioDescr);
	}
	RETiRet;
}


/* process a receive request on one of the streams
 * If pPoll is non-NULL, we have a netstream in epoll mode, which means we need
 * to remove any descriptor we close from the epoll set.
 * rgerhards, 2009-07-020
 */
static rsRetVal
doReceive(tcpsrv_t *const pThis, nspoll_t *const pPoll, tcpsrv_io_descr_t *const pioDescr)
{
	char buf[128*1024]; /* reception buffer - may hold a partial or multiple messages */
	ssize_t iRcvd;
	rsRetVal localRet;
	DEFiRet;
	uchar *pszPeer;
	int lenPeer;
	int oserr = 0;
	int do_run = 1;
	int loop_ctr = 0;
	tcps_sess_t *pSess = pioDescr->ptr.pSess;

	ISOBJ_TYPE_assert(pThis, tcpsrv);
	prop.GetString((pSess)->fromHostIP, &pszPeer, &lenPeer);
	DBGPRINTF("netstream %p with new data from remote peer %s\n", (pSess)->pStrm, pszPeer);

	/* if we had EPOLLERR, give information. The other processing continues. This
	 * seems to be best practice and may cause better error information.
	 */
	if(pioDescr->isInError) {
		int error = 0;
		socklen_t len = sizeof(error);
		const int sock = ((nsd_ptcp_t *) pSess->pStrm->pDrvrData)->sock;
		if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len) == 0) {
			LogError(error, RS_RET_IO_ERROR, "epoll subsystem signalled state EPOLLERR "
				"for stream %p, peer %s  ", (pSess)->pStrm, pszPeer);
		} /* no else - if this fails, we have nothing to report... */
	}

	while(do_run && loop_ctr < 500) {	/*  break happens in switch below! */
		dbgprintf("RGER: doReceive loop iteration %d\n", loop_ctr++);

		// TODO: STARVATION needs URGENTLY be considered!!! loop_ctr is one step into
		// this direction, but we need to consider that in regard to edge triggered epoll

		/* Receive message */
		iRet = pThis->pRcvData(pSess, buf, sizeof(buf), &iRcvd, &oserr);
		switch(iRet) {
		case RS_RET_CLOSED:
			if(pThis->bEmitMsgOnClose) {
				errno = 0;
				LogError(0, RS_RET_PEER_CLOSED_CONN, "Netstream session %p closed by remote "
					"peer %s.\n", (pSess)->pStrm, pszPeer);
			}
			CHKiRet(closeSess(pThis, pioDescr, pPoll));
			do_run = 0;
			break;
		case RS_RET_RETRY:
			/* we simply ignore retry - this is not an error, but we also have not received anything */
			do_run = 0;
			break;
		case RS_RET_OK:
			/* valid data received, process it! */
			localRet = tcps_sess.DataRcvd(pSess, buf, iRcvd);
			if(localRet != RS_RET_OK && localRet != RS_RET_QUEUE_FULL) {
				/* in this case, something went awfully wrong.
				 * We are instructed to terminate the session.
				 */
				LogError(oserr, localRet, "Tearing down TCP Session from %s", pszPeer);
				CHKiRet(closeSess(pThis, pioDescr, pPoll));
			}
			break;
		default:
			LogError(oserr, iRet, "netstream session %p from %s will be closed due to error",
					pSess->pStrm, pszPeer);
			CHKiRet(closeSess(pThis, pioDescr, pPoll));
			do_run = 0;
			break;
		}
	}

finalize_it:
	dbgprintf("RGER: doReceive exit, iRet %d\n", iRet);
	RETiRet;
}


static rsRetVal ATTR_NONNULL(1)
doAccept(tcpsrv_t *const pThis, nspoll_t *const pPoll, const int idx)
{
	tcpLstnParams_t *cnf_params;
	tcps_sess_t *pNewSess = NULL;
	tcpsrv_io_descr_t *pDescr = NULL;
	DEFiRet;

	DBGPRINTF("New connect on NSD %p.\n", pThis->ppLstn[idx]);
	iRet = SessAccept(pThis, pThis->ppLstnPort[idx], &pNewSess, pThis->ppLstn[idx]);
	cnf_params = pThis->ppLstnPort[idx]->cnf_params;
	if(iRet == RS_RET_OK) {
		if(pPoll != NULL) {
			/* pDescr is only dyn allocated in epoll mode! */
			CHKmalloc(pDescr = (tcpsrv_io_descr_t*) calloc(1, sizeof(tcpsrv_io_descr_t)));
			pDescr->id = idx; // TODO: remove if session handling is refactored to dyn max sessions
			pDescr->isInError = 0;
			pDescr->ptrType = NSD_PTR_TYPE_SESS;
			pDescr->ptr.pSess = pNewSess;
			CHKiRet(nspoll.Ctl(pPoll, pDescr, NSDPOLL_IN, NSDPOLL_ADD));
		}
		DBGPRINTF("New session created with NSD %p.\n", pNewSess);
	} else {
		DBGPRINTF("tcpsrv: error %d during accept\n", iRet);
	}

finalize_it:
	if(iRet != RS_RET_OK) {
		LogError(0, iRet, "tcpsrv listener (inputname: '%s') failed "
			"to process incoming connection with error %d",
			(cnf_params->pszInputName == NULL) ? (uchar*)"*UNSET*" : cnf_params->pszInputName, iRet);
		free(pDescr);
		srSleep(0,20000); /* Sleep 20ms */
	}
	RETiRet;
}

/* process a single workset item
 */
static rsRetVal ATTR_NONNULL(1)
processWorksetItem(tcpsrv_t *const pThis, nspoll_t *pPoll, tcpsrv_io_descr_t *const pioDescr)
{
	DEFiRet;

	DBGPRINTF("tcpsrv: processing item %d\n", pioDescr->id);
	if(pioDescr->ptrType == NSD_PTR_TYPE_LSTN) {
		doAccept(pThis, pPoll, pioDescr->id);
	} else {
		doReceive(pThis, pPoll, pioDescr);
	}
	RETiRet;
}


/* worker to process incoming requests
 */
static void * ATTR_NONNULL(1)
wrkr(void *const myself)
{
	struct tcpsrv_wrkrInfo_s *const me = (struct tcpsrv_wrkrInfo_s*) myself;
	tcpsrv_t *const pThis = me->mySrv;

	pthread_mutex_lock(&pThis->wrkrMut);
	while(1) {
		// wait for work, in which case pSrv will be populated
		while(me->pSrv == NULL && glbl.GetGlobalInputTermState() == 0) {
			pthread_cond_wait(&me->run, &pThis->wrkrMut);
		}
		if(me->pSrv == NULL) {
			// only possible if glbl.GetGlobalInputTermState() == 1
			// we need to query me->opSrv to avoid clang static
			// analyzer false positive! -- rgerhards, 2017-10-23
			assert(glbl.GetGlobalInputTermState() == 1);
			break;
		}
		pthread_mutex_unlock(&pThis->wrkrMut);

		++me->numCalled;
		processWorksetItem(me->pSrv, me->pPoll, me->pioDescr);

		pthread_mutex_lock(&pThis->wrkrMut);
		me->pSrv = NULL;	/* indicate we are free again */
		--pThis->wrkrRunning;
		pthread_cond_broadcast(&pThis->wrkrIdle);
	}
	me->enabled = 0; /* indicate we are no longer available */
	pthread_mutex_unlock(&pThis->wrkrMut);

	return NULL;
}

/* This has been factored out from processWorkset() because
 * pthread_cleanup_push() invokes setjmp() and this triggers the -Wclobbered
 * warning for the iRet variable.
 */
static void
waitForWorkers(tcpsrv_t *const pThis)
{
	pthread_mutex_lock(&pThis->wrkrMut);
	pthread_cleanup_push(mutexCancelCleanup, &pThis->wrkrMut);
	while(pThis->wrkrRunning > 0) {
		pthread_cond_wait(&pThis->wrkrIdle, &pThis->wrkrMut);
	}
	pthread_cleanup_pop(1);
}

/* Process a workset, that is handle io. We become activated
 * from either select or epoll handler. We split the workload
 * out to a pool of threads, but try to avoid context switches
 * as much as possible.
 */
static rsRetVal
processWorkset(tcpsrv_t *const pThis, nspoll_t *const pPoll, int numEntries, tcpsrv_io_descr_t *const pioDescr[])
{
	int i;
	int origEntries = numEntries;
	DEFiRet;

	DBGPRINTF("tcpsrv: ready to process %d event entries\n", numEntries);

	while(numEntries > 0) {
		if(glbl.GetGlobalInputTermState() == 1)
			ABORT_FINALIZE(RS_RET_FORCE_TERM);
		if(numEntries == 1) {
			/* process self, save context switch */
			iRet = processWorksetItem(pThis, pPoll, pioDescr[0]);
		} else {
			/* No cancel handler needed here, since no cancellation
			 * points are executed while pThis->wrkrMut is locked.
			 *
			 * Re-evaluate this if you add a DBGPRINTF or something!
			 */
			pthread_mutex_lock(&pThis->wrkrMut);
			/* check if there is a free worker */
			for(i = 0 ; (i < pThis->wrkrMax) && ((pThis->wrkrInfo[i].pSrv != NULL) || (pThis->wrkrInfo[i].enabled == 0)) ; ++i)
				/*do search*/;
			//if(i < pThis->wrkrMax) {
			if(i < 0) {
				/* worker free -> use it! */
				pThis->wrkrInfo[i].pSrv = pThis;
				pThis->wrkrInfo[i].pPoll = pPoll;
				pThis->wrkrInfo[i].pioDescr = pioDescr[numEntries - 1];
				/* Note: we must increment pThis->wrkrRunning HERE and not inside the worker's
				 * code. This is because a worker may actually never start, and thus
				 * increment pThis->wrkrRunning, before we finish and check the running worker
				 * count. We can only avoid this by incrementing it here.
				 */
				++pThis->wrkrRunning;
				pthread_cond_signal(&pThis->wrkrInfo[i].run);
				pthread_mutex_unlock(&pThis->wrkrMut);
			} else {
				pthread_mutex_unlock(&pThis->wrkrMut);
				/* no free worker, so we process this one ourselfs */
				iRet = processWorksetItem(pThis, pPoll, pioDescr[numEntries-1]);
			}
		}
		--numEntries;
	}

	if(origEntries > 1) {
		/* we now need to wait until all workers finish. This is because the
		 * rest of this module can not handle the concurrency introduced
		 * by workers running during the epoll call.
		 */
		waitForWorkers(pThis);
	}

finalize_it:
	RETiRet;
}


/* This function is called to gather input.
 * This variant here is only used if we need to work with a netstream driver
 * that does not support epoll().
 */
PRAGMA_DIAGNOSTIC_PUSH
PRAGMA_IGNORE_Wempty_body
static rsRetVal
RunSelect(tcpsrv_t *const pThis)
{
	DEFiRet;
	int nfds;
	int i;
	int iWorkset;
	int iTCPSess;
	int bIsReady;
	tcpsrv_io_descr_t *pWorkset[NSPOLL_MAX_EVENTS_PER_WAIT];
	tcpsrv_io_descr_t workset[NSPOLL_MAX_EVENTS_PER_WAIT];
	const int sizeWorkset = sizeof(workset)/sizeof(tcpsrv_io_descr_t);
	nssel_t *pSel = NULL;
	rsRetVal localRet;

	ISOBJ_TYPE_assert(pThis, tcpsrv);

	/* init the workset pointers, they will remain fixed */
	for(i = 0 ; i < sizeWorkset ; ++i) {
		pWorkset[i] = workset + i;
	}

	pthread_cleanup_push(RunSelectCancelCleanup, (void*) &pSel);
	while(1) {
		CHKiRet(nssel.Construct(&pSel));
		if(pThis->pszDrvrName != NULL)
			CHKiRet(nssel.SetDrvrName(pSel, pThis->pszDrvrName));
		CHKiRet(nssel.ConstructFinalize(pSel));

		/* Add the TCP listen sockets to the list of read descriptors. */
		for(i = 0 ; i < pThis->iLstnCurr ; ++i) {
			CHKiRet(nssel.Add(pSel, pThis->ppLstn[i], NSDSEL_RD));
		}

		/* do the sessions */
		iTCPSess = TCPSessGetNxtSess(pThis, -1);
		while(iTCPSess != -1) {
			/* TODO: access to pNsd is NOT really CLEAN, use method... */
			CHKiRet(nssel.Add(pSel, pThis->pSessions[iTCPSess]->pStrm, NSDSEL_RD));
			DBGPRINTF("tcpsrv process session %d:\n", iTCPSess);

			/* now get next... */
			iTCPSess = TCPSessGetNxtSess(pThis, iTCPSess);
		}

		/* wait for io to become ready */
		CHKiRet(nssel.Wait(pSel, &nfds));
		if(glbl.GetGlobalInputTermState() == 1)
			break; /* terminate input! */

		iWorkset = 0;
		for(i = 0 ; i < pThis->iLstnCurr ; ++i) {
			if(glbl.GetGlobalInputTermState() == 1)
				ABORT_FINALIZE(RS_RET_FORCE_TERM);
			CHKiRet(nssel.IsReady(pSel, pThis->ppLstn[i], NSDSEL_RD, &bIsReady, &nfds));
			if(bIsReady) {
				workset[iWorkset].id = i;

				workset[iWorkset].ptrType = NSD_PTR_TYPE_LSTN;
				workset[iWorkset].id = i;
				workset[iWorkset].isInError = 0;
				workset[iWorkset].ptr.ppLstn = pThis->ppLstn;
				/* this is a flag to indicate listen sock */
				++iWorkset;
				if(iWorkset >= (int) sizeWorkset) {
					processWorkset(pThis, NULL, iWorkset, pWorkset);
					iWorkset = 0;
				}
				--nfds; /* indicate we have processed one */
			}
		}

		/* now check the sessions */
		iTCPSess = TCPSessGetNxtSess(pThis, -1);
		while(nfds && iTCPSess != -1) {
			if(glbl.GetGlobalInputTermState() == 1)
				ABORT_FINALIZE(RS_RET_FORCE_TERM);
			localRet = nssel.IsReady(pSel, pThis->pSessions[iTCPSess]->pStrm, NSDSEL_RD,
				&bIsReady, &nfds);
			if(bIsReady || localRet != RS_RET_OK) {
				workset[iWorkset].ptrType = NSD_PTR_TYPE_SESS;
				workset[iWorkset].id = iTCPSess;
				workset[iWorkset].isInError = 0;
				workset[iWorkset].ptr.pSess = pThis->pSessions[iTCPSess];
				++iWorkset;
				if(iWorkset >= (int) sizeWorkset) {
					processWorkset(pThis, NULL, iWorkset, pWorkset);
					iWorkset = 0;
				}
				if(bIsReady)
					--nfds; /* indicate we have processed one */
			}
			iTCPSess = TCPSessGetNxtSess(pThis, iTCPSess);
		}

		if(iWorkset > 0)
			processWorkset(pThis, NULL, iWorkset, pWorkset);

		/* we need to copy back close descriptors */
		nssel.Destruct(&pSel); /* no iRet check as it is overriden at start of loop! */
finalize_it: /* this is a very special case - this time only we do not exit the function,
	      * because that would not help us either. So we simply retry it. Let's see
	      * if that actually is a better idea. Exiting the loop wasn't we always
	      * crashed, which made sense (the rest of the engine was not prepared for
	      * that) -- rgerhards, 2008-05-19
	      */
		if(pSel != NULL) { /* cleanup missing? happens during err exit! */
			nssel.Destruct(&pSel);
		}
	}

	pthread_cleanup_pop(1); /* execute and remove cleanup handler */

	RETiRet;
}
PRAGMA_DIAGNOSTIC_POP

static rsRetVal
DoRun(tcpsrv_t *const pThis, nspoll_t **ppPoll)
{
	DEFiRet;
	int i;
	tcpsrv_io_descr_t *workset[NSPOLL_MAX_EVENTS_PER_WAIT];
	int numEntries;
	nspoll_t *pPoll = NULL;
	rsRetVal localRet;

	if((localRet = nspoll.Construct(ppPoll)) == RS_RET_OK) {
		pPoll = *ppPoll;
		if(pThis->pszDrvrName != NULL)
			CHKiRet(nspoll.SetDrvrName(pPoll, pThis->pszDrvrName));
		localRet = nspoll.ConstructFinalize(pPoll);
	}
	if(localRet != RS_RET_OK) {
		/* fall back to select */
fprintf(stderr, "WARNING_ Falling back to select(), iRet was %d, driver '%s'\n", localRet, pThis->pszDrvrName);
		DBGPRINTF("tcpsrv could not use epoll() interface, iRet=%d, using select()\n", localRet);
		iRet = RunSelect(pThis);
		FINALIZE;
	}

	DBGPRINTF("tcpsrv uses epoll() interface, nsdpoll driver found\n");

	/* flag that we are in epoll mode */
	pThis->bUsingEPoll = RSTRUE;

	/* Add the TCP listen sockets to the list of sockets to monitor */
	for(i = 0 ; i < pThis->iLstnCurr ; ++i) {
		DBGPRINTF("Trying to add listener %d, pUsr=%p\n", i, pThis->ppLstn);
		CHKmalloc(pThis->ppioDescrPtr[i] = (tcpsrv_io_descr_t*) calloc(1, sizeof(tcpsrv_io_descr_t)));
		pThis->ppioDescrPtr[i]->id = i; // TODO: remove if session handling is refactored to dyn max sessions
		pThis->ppioDescrPtr[i]->isInError = 0;
		pThis->ppioDescrPtr[i]->ptrType = NSD_PTR_TYPE_LSTN;
		pThis->ppioDescrPtr[i]->ptr.ppLstn = pThis->ppLstn;
		CHKiRet(nspoll.Ctl(pPoll, pThis->ppioDescrPtr[i], NSDPOLL_IN_LSTN, NSDPOLL_ADD));
		DBGPRINTF("Added listener %d\n", i);
	}

	while(glbl.GetGlobalInputTermState() == 0) {
		numEntries = sizeof(workset)/sizeof(tcpsrv_io_descr_t *);
		localRet = nspoll.Wait(pPoll, -1, &numEntries, workset);
		if(glbl.GetGlobalInputTermState() == 1)
			break; /* terminate input! */

		/* check if we need to ignore the i/o ready state. We do this if we got an invalid
		 * return state. Validly, this can happen for RS_RET_EINTR, for other cases it may
		 * not be the right thing, but what is the right thing is really hard at this point...
		 */
		if(localRet != RS_RET_OK)
			continue;

		processWorkset(pThis, pPoll, numEntries, workset);
	}

	/* remove the tcp listen sockets from the epoll set */
	for(i = 0 ; i < pThis->iLstnCurr ; ++i) {
		CHKiRet(nspoll.Ctl(pPoll, pThis->ppioDescrPtr[i], NSDPOLL_IN_LSTN, NSDPOLL_DEL));
		free(pThis->ppioDescrPtr[i]);
	}

finalize_it:
	RETiRet;
}


/* This function is called to gather input. It tries doing that via the epoll()
 * interface. If the driver does not support that, it falls back to calling its
 * select() equivalent.
 * rgerhards, 2009-11-18
 */
static rsRetVal
Run(tcpsrv_t *const pThis)
{
	DEFiRet;
	nspoll_t *pPoll = NULL;

	ISOBJ_TYPE_assert(pThis, tcpsrv);

	if(pThis->iLstnCurr == 0) {
		dbgprintf("tcpsrv: no listeneres at all (probably init error), terminating\n");
		RETiRet; /* somewhat "dirty" exit to avoid issue with cancel handler */
	}

	/* check if we need to start the worker pool. Once it is running, all is
	 * well. Shutdown is done on modExit.
	 */
	d_pthread_mutex_lock(&pThis->wrkrMut);
	if(!pThis->bWrkrRunning) {
		pThis->bWrkrRunning = 1;
		startWorkerPool(pThis);
	}
	d_pthread_mutex_unlock(&pThis->wrkrMut);

	/* We try to terminate cleanly, but install a cancellation clean-up
	 * handler in case we are cancelled.
	 */
	struct runCancelCleanupData_s cleanup_data;
	cleanup_data.ppPoll = &pPoll;
	cleanup_data.pThis = pThis;
	pthread_cleanup_push(RunCancelCleanup, (void*) &cleanup_data);
	iRet = DoRun(pThis, &pPoll);
	pthread_cleanup_pop(1);

	RETiRet;
}


/* Standard-Constructor */
BEGINobjConstruct(tcpsrv) /* be sure to specify the object type also in END macro! */
	pThis->iSessMax = TCPSESS_MAX_DEFAULT;
	pThis->iLstnMax = TCPLSTN_MAX_DEFAULT;
	pThis->addtlFrameDelim = TCPSRV_NO_ADDTL_DELIMITER;
	pThis->maxFrameSize = 200000;
	pThis->bDisableLFDelim = 0;
	pThis->discardTruncatedMsg = 0;
	pThis->OnMsgReceive = NULL;
	pThis->dfltTZ[0] = '\0';
	pThis->bSPFramingFix = 0;
	pThis->ratelimitInterval = 0;
	pThis->ratelimitBurst = 10000;
	pThis->bUseFlowControl = 1;
	pThis->pszDrvrName = NULL;
	pThis->bPreserveCase = 1; /* preserve case in fromhost; default to true. */
	pThis->iSynBacklog = 0; /* default: unset */
	pThis->DrvrTlsVerifyDepth = 0;

	pThis->wrkrMax = 4; // TODO: do not hardcode!
	for(int i = 0 ; i < 4 ; ++i) {
		pThis->wrkrInfo[i].mySrv = pThis;
	}
	pthread_mutex_init(&pThis->wrkrMut, NULL);
	pThis->bWrkrRunning = 0;
ENDobjConstruct(tcpsrv)


/* ConstructionFinalizer */
static rsRetVal
tcpsrvConstructFinalize(tcpsrv_t *pThis)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, tcpsrv);

	/* prepare network stream subsystem */
	CHKiRet(netstrms.Construct(&pThis->pNS));
	CHKiRet(netstrms.SetSynBacklog(pThis->pNS, pThis->iSynBacklog));
	if(pThis->pszDrvrName != NULL)
		CHKiRet(netstrms.SetDrvrName(pThis->pNS, pThis->pszDrvrName));
	CHKiRet(netstrms.SetDrvrMode(pThis->pNS, pThis->iDrvrMode));
	CHKiRet(netstrms.SetDrvrCheckExtendedKeyUsage(pThis->pNS, pThis->DrvrChkExtendedKeyUsage));
	CHKiRet(netstrms.SetDrvrPrioritizeSAN(pThis->pNS, pThis->DrvrPrioritizeSan));
	CHKiRet(netstrms.SetDrvrTlsVerifyDepth(pThis->pNS, pThis->DrvrTlsVerifyDepth));
	if(pThis->pszDrvrAuthMode != NULL)
		CHKiRet(netstrms.SetDrvrAuthMode(pThis->pNS, pThis->pszDrvrAuthMode));
	/* Call SetDrvrPermitExpiredCerts required
	 * when param is NULL default handling for ExpiredCerts is set! */
	CHKiRet(netstrms.SetDrvrPermitExpiredCerts(pThis->pNS, pThis->pszDrvrPermitExpiredCerts));
	CHKiRet(netstrms.SetDrvrTlsCAFile(pThis->pNS, pThis->pszDrvrCAFile));
	CHKiRet(netstrms.SetDrvrTlsCRLFile(pThis->pNS, pThis->pszDrvrCRLFile));
	CHKiRet(netstrms.SetDrvrTlsKeyFile(pThis->pNS, pThis->pszDrvrKeyFile));
	CHKiRet(netstrms.SetDrvrTlsCertFile(pThis->pNS, pThis->pszDrvrCertFile));
	if(pThis->pPermPeers != NULL)
		CHKiRet(netstrms.SetDrvrPermPeers(pThis->pNS, pThis->pPermPeers));
	if(pThis->gnutlsPriorityString != NULL)
		CHKiRet(netstrms.SetDrvrGnutlsPriorityString(pThis->pNS, pThis->gnutlsPriorityString));
	CHKiRet(netstrms.ConstructFinalize(pThis->pNS));

	/* set up listeners */
	CHKmalloc(pThis->ppLstn = calloc(pThis->iLstnMax, sizeof(netstrm_t*)));
	CHKmalloc(pThis->ppLstnPort = calloc(pThis->iLstnMax, sizeof(tcpLstnPortList_t*)));
	CHKmalloc(pThis->ppioDescrPtr = calloc(pThis->iLstnMax, sizeof(tcpsrv_io_descr_t*)));
	iRet = pThis->OpenLstnSocks(pThis);

finalize_it:
	if(iRet != RS_RET_OK) {
		if(pThis->pNS != NULL)
			netstrms.Destruct(&pThis->pNS);
		LogError(0, iRet, "tcpsrv could not create listener (inputname: '%s')",
				(pThis->pszInputName == NULL) ? (uchar*)"*UNSET*" : pThis->pszInputName);
	}
	RETiRet;
}


/* destructor for the tcpsrv object */
BEGINobjDestruct(tcpsrv) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(tcpsrv)
	if(pThis->OnDestruct != NULL)
		pThis->OnDestruct(pThis->pUsr);

	deinit_tcp_listener(pThis);

	if(pThis->bWrkrRunning) {
		stopWorkerPool(pThis);
		pThis->bWrkrRunning = 0;
	}
	pthread_mutex_destroy(&pThis->wrkrMut);

	if(pThis->pNS != NULL)
		netstrms.Destruct(&pThis->pNS);
	free(pThis->pszDrvrName);
	free(pThis->pszDrvrAuthMode);
	free(pThis->pszDrvrPermitExpiredCerts);
	free(pThis->pszDrvrCAFile);
	free(pThis->pszDrvrCRLFile);
	free(pThis->pszDrvrKeyFile);
	free(pThis->pszDrvrCertFile);
	free(pThis->ppLstn);
	free(pThis->ppLstnPort);
	free(pThis->ppioDescrPtr);
	free(pThis->pszInputName);
	free(pThis->pszOrigin);
ENDobjDestruct(tcpsrv)


/* debugprint for the tcpsrv object */
BEGINobjDebugPrint(tcpsrv) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDebugPrint(tcpsrv)
ENDobjDebugPrint(tcpsrv)

/* set functions */
static rsRetVal
SetCBIsPermittedHost(tcpsrv_t *pThis, int (*pCB)(struct sockaddr *addr, char *fromHostFQDN, void*, void*))
{
	DEFiRet;
	pThis->pIsPermittedHost = pCB;
	RETiRet;
}

static rsRetVal
SetCBRcvData(tcpsrv_t *pThis, rsRetVal (*pRcvData)(tcps_sess_t*, char*, size_t, ssize_t*, int*))
{
	DEFiRet;
	pThis->pRcvData = pRcvData;
	RETiRet;
}

static rsRetVal
SetCBOnListenDeinit(tcpsrv_t *pThis, rsRetVal (*pCB)(void*))
{
	DEFiRet;
	pThis->pOnListenDeinit = pCB;
	RETiRet;
}

static rsRetVal
SetCBOnSessAccept(tcpsrv_t *pThis, rsRetVal (*pCB)(tcpsrv_t*, tcps_sess_t*))
{
	DEFiRet;
	pThis->pOnSessAccept = pCB;
	RETiRet;
}

static rsRetVal
SetCBOnDestruct(tcpsrv_t *pThis, rsRetVal (*pCB)(void*))
{
	DEFiRet;
	pThis->OnDestruct = pCB;
	RETiRet;
}

static rsRetVal
SetCBOnSessConstructFinalize(tcpsrv_t *pThis, rsRetVal (*pCB)(void*))
{
	DEFiRet;
	pThis->OnSessConstructFinalize = pCB;
	RETiRet;
}

static rsRetVal
SetCBOnSessDestruct(tcpsrv_t *pThis, rsRetVal (*pCB)(void*))
{
	DEFiRet;
	pThis->pOnSessDestruct = pCB;
	RETiRet;
}

static rsRetVal
SetCBOnRegularClose(tcpsrv_t *pThis, rsRetVal (*pCB)(tcps_sess_t*))
{
	DEFiRet;
	pThis->pOnRegularClose = pCB;
	RETiRet;
}

static rsRetVal
SetCBOnErrClose(tcpsrv_t *pThis, rsRetVal (*pCB)(tcps_sess_t*))
{
	DEFiRet;
	pThis->pOnErrClose = pCB;
	RETiRet;
}

static rsRetVal
SetCBOpenLstnSocks(tcpsrv_t *pThis, rsRetVal (*pCB)(tcpsrv_t*))
{
	DEFiRet;
	pThis->OpenLstnSocks = pCB;
	RETiRet;
}

static rsRetVal
SetUsrP(tcpsrv_t *pThis, void *pUsr)
{
	DEFiRet;
	pThis->pUsr = pUsr;
	RETiRet;
}

static rsRetVal
SetKeepAlive(tcpsrv_t *pThis, int iVal)
{
	DEFiRet;
	DBGPRINTF("tcpsrv: keep-alive set to %d\n", iVal);
	pThis->bUseKeepAlive = iVal;
	RETiRet;
}

static rsRetVal
SetKeepAliveIntvl(tcpsrv_t *pThis, int iVal)
{
	DEFiRet;
	DBGPRINTF("tcpsrv: keep-alive interval set to %d\n", iVal);
	pThis->iKeepAliveIntvl = iVal;
	RETiRet;
}

static rsRetVal
SetKeepAliveProbes(tcpsrv_t *pThis, int iVal)
{
	DEFiRet;
	DBGPRINTF("tcpsrv: keep-alive probes set to %d\n", iVal);
	pThis->iKeepAliveProbes = iVal;
	RETiRet;
}

static rsRetVal
SetKeepAliveTime(tcpsrv_t *pThis, int iVal)
{
	DEFiRet;
	DBGPRINTF("tcpsrv: keep-alive timeout set to %d\n", iVal);
	pThis->iKeepAliveTime = iVal;
	RETiRet;
}

static rsRetVal
SetGnutlsPriorityString(tcpsrv_t *pThis, uchar *iVal)
{
	DEFiRet;
	DBGPRINTF("tcpsrv: gnutlsPriorityString set to %s\n",
		(iVal == NULL) ? "(null)" : (const char*) iVal);
	pThis->gnutlsPriorityString = iVal;
	RETiRet;
}


static rsRetVal
SetOnMsgReceive(tcpsrv_t *pThis, rsRetVal (*OnMsgReceive)(tcps_sess_t*, uchar*, int))
{
	DEFiRet;
	assert(OnMsgReceive != NULL);
	pThis->OnMsgReceive = OnMsgReceive;
	RETiRet;
}


/* set enable/disable standard LF frame delimiter (use with care!)
 * -- rgerhards, 2010-01-03
 */
static rsRetVal
SetbDisableLFDelim(tcpsrv_t *pThis, int bVal)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, tcpsrv);
	pThis->bDisableLFDelim = bVal;
	RETiRet;
}


/* discard the truncated msg part
 * -- PascalWithopf, 2017-04-20
 */
static rsRetVal
SetDiscardTruncatedMsg(tcpsrv_t *pThis, int discard)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, tcpsrv);
	pThis->discardTruncatedMsg = discard;
	RETiRet;
}


/* Set additional framing to use (if any) -- rgerhards, 2008-12-10 */
static rsRetVal
SetAddtlFrameDelim(tcpsrv_t *pThis, int iDelim)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, tcpsrv);
	pThis->addtlFrameDelim = iDelim;
	RETiRet;
}


/* Set max frame size for octet counted -- PascalWithopf, 2017-04-20*/
static rsRetVal
SetMaxFrameSize(tcpsrv_t *pThis, int maxFrameSize)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, tcpsrv);
	pThis->maxFrameSize = maxFrameSize;
	RETiRet;
}


static rsRetVal
SetDfltTZ(tcpsrv_t *const pThis, uchar *const tz)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, tcpsrv);
	strncpy((char*)pThis->dfltTZ, (char*)tz, sizeof(pThis->dfltTZ));
	pThis->dfltTZ[sizeof(pThis->dfltTZ)-1] = '\0';
	RETiRet;
}


static rsRetVal
SetbSPFramingFix(tcpsrv_t *pThis, const sbool val)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, tcpsrv);
	pThis->bSPFramingFix = val;
	RETiRet;
}

static rsRetVal
SetOrigin(tcpsrv_t *pThis, uchar *origin)
{
	DEFiRet;
	free(pThis->pszOrigin);
	pThis->pszOrigin = (origin == NULL) ? NULL : ustrdup(origin);
	RETiRet;
}

/* Set the input name to use -- rgerhards, 2008-12-10 */
static rsRetVal
SetInputName(tcpsrv_t *const pThis,tcpLstnParams_t *const cnf_params, const uchar *const name)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, tcpsrv);
	if(name == NULL)
		cnf_params->pszInputName = NULL;
	else
		CHKmalloc(cnf_params->pszInputName = ustrdup(name));
	free(pThis->pszInputName); // TODO: REMOVE ME
	pThis->pszInputName = ustrdup("imtcp"); // TODO: REMOVE ME

	/* we need to create a property */
	CHKiRet(prop.Construct(&cnf_params->pInputName));
	CHKiRet(prop.SetString(cnf_params->pInputName, cnf_params->pszInputName, ustrlen(cnf_params->pszInputName)));
	CHKiRet(prop.ConstructFinalize(cnf_params->pInputName));
finalize_it:
	RETiRet;
}


/* Set the linux-like ratelimiter settings */
static rsRetVal
SetLinuxLikeRatelimiters(tcpsrv_t *pThis, unsigned int ratelimitInterval, unsigned int ratelimitBurst)
{
	DEFiRet;
	pThis->ratelimitInterval = ratelimitInterval;
	pThis->ratelimitBurst = ratelimitBurst;
	RETiRet;
}


/* Set connection open notification */
static rsRetVal
SetNotificationOnRemoteOpen(tcpsrv_t *pThis, const int bNewVal)
{
	pThis->bEmitMsgOnOpen = bNewVal;
	return RS_RET_OK;
}
/* Set connection close notification */
static rsRetVal
SetNotificationOnRemoteClose(tcpsrv_t *pThis, const int bNewVal)
{
	DEFiRet;
	pThis->bEmitMsgOnClose = bNewVal;
	RETiRet;
}


/* here follows a number of methods that shuffle authentication settings down
 * to the drivers. Drivers not supporting these settings may return an error
 * state.
 * -------------------------------------------------------------------------- */

/* set the driver mode -- rgerhards, 2008-04-30 */
static rsRetVal
SetDrvrMode(tcpsrv_t *pThis, const int iMode)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, tcpsrv);
	pThis->iDrvrMode = iMode;
	RETiRet;
}

static rsRetVal
SetDrvrName(tcpsrv_t *pThis, uchar *const name)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, tcpsrv);
	free(pThis->pszDrvrName);
	CHKmalloc(pThis->pszDrvrName = ustrdup(name));
finalize_it:
	RETiRet;
}

/* set the driver authentication mode -- rgerhards, 2008-05-19 */
static rsRetVal
SetDrvrAuthMode(tcpsrv_t *pThis, uchar *const mode)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, tcpsrv);
	CHKmalloc(pThis->pszDrvrAuthMode = ustrdup(mode));
finalize_it:
	RETiRet;
}

/* set the driver permitexpiredcerts mode -- alorbach, 2018-12-20
 */
static rsRetVal
SetDrvrPermitExpiredCerts(tcpsrv_t *pThis, uchar *mode)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, tcpsrv);
	if (mode != NULL) {
		CHKmalloc(pThis->pszDrvrPermitExpiredCerts = ustrdup(mode));
	}
finalize_it:
	RETiRet;
}

static rsRetVal
SetDrvrCAFile(tcpsrv_t *const pThis, uchar *const mode)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, tcpsrv);
	if (mode != NULL) {
		CHKmalloc(pThis->pszDrvrCAFile = ustrdup(mode));
	}
finalize_it:
	RETiRet;
}

static rsRetVal
SetDrvrCRLFile(tcpsrv_t *const pThis, uchar *const mode)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, tcpsrv);
	if (mode != NULL) {
		CHKmalloc(pThis->pszDrvrCRLFile = ustrdup(mode));
	}
finalize_it:
	RETiRet;
}

static rsRetVal
SetDrvrKeyFile(tcpsrv_t *pThis, uchar *mode)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, tcpsrv);
	if (mode != NULL) {
		CHKmalloc(pThis->pszDrvrKeyFile = ustrdup(mode));
	}
finalize_it:
	RETiRet;
}

static rsRetVal
SetDrvrCertFile(tcpsrv_t *pThis, uchar *mode)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, tcpsrv);
	if (mode != NULL) {
		CHKmalloc(pThis->pszDrvrCertFile = ustrdup(mode));
	}
finalize_it:
	RETiRet;
}


/* set the driver's permitted peers -- rgerhards, 2008-05-19 */
static rsRetVal
SetDrvrPermPeers(tcpsrv_t *pThis, permittedPeers_t *pPermPeers)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, tcpsrv);
	pThis->pPermPeers = pPermPeers;
	RETiRet;
}

/* set the driver cert extended key usage check setting -- jvymazal, 2019-08-16 */
static rsRetVal
SetDrvrCheckExtendedKeyUsage(tcpsrv_t *pThis, int ChkExtendedKeyUsage)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, tcpsrv);
	pThis->DrvrChkExtendedKeyUsage = ChkExtendedKeyUsage;
	RETiRet;
}

/* set the driver name checking policy -- jvymazal, 2019-08-16 */
static rsRetVal
SetDrvrPrioritizeSAN(tcpsrv_t *pThis, int prioritizeSan)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, tcpsrv);
	pThis->DrvrPrioritizeSan = prioritizeSan;
	RETiRet;
}

/* set the driver Set the driver tls  verifyDepth -- alorbach, 2019-12-20 */
static rsRetVal
SetDrvrTlsVerifyDepth(tcpsrv_t *pThis, int verifyDepth)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, tcpsrv);
	pThis->DrvrTlsVerifyDepth = verifyDepth;
	RETiRet;
}


/* End of methods to shuffle autentication settings to the driver.;

 * -------------------------------------------------------------------------- */


/* set max number of listeners
 * this must be called before ConstructFinalize, or it will have no effect!
 * rgerhards, 2009-08-17
 */
static rsRetVal
SetLstnMax(tcpsrv_t *pThis, int iMax)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, tcpsrv);
	pThis->iLstnMax = iMax;
	RETiRet;
}


/* set if flow control shall be supported
 */
static rsRetVal
SetUseFlowControl(tcpsrv_t *pThis, int bUseFlowControl)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, tcpsrv);
	pThis->bUseFlowControl = bUseFlowControl;
	RETiRet;
}


/* set max number of sessions
 * this must be called before ConstructFinalize, or it will have no effect!
 * rgerhards, 2009-04-09
 */
static rsRetVal
SetSessMax(tcpsrv_t *pThis, int iMax)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, tcpsrv);
	pThis->iSessMax = iMax;
	RETiRet;
}


static rsRetVal
SetPreserveCase(tcpsrv_t *pThis, int bPreserveCase)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, tcpsrv);
	pThis-> bPreserveCase = bPreserveCase;
	RETiRet;
}


static rsRetVal
SetSynBacklog(tcpsrv_t *pThis, const int iSynBacklog)
{
	pThis->iSynBacklog = iSynBacklog;
	return RS_RET_OK;
}


/* queryInterface function
 * rgerhards, 2008-02-29
 */
BEGINobjQueryInterface(tcpsrv)
CODESTARTobjQueryInterface(tcpsrv)
	if(pIf->ifVersion != tcpsrvCURR_IF_VERSION) { /* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->DebugPrint = tcpsrvDebugPrint;
	pIf->Construct = tcpsrvConstruct;
	pIf->ConstructFinalize = tcpsrvConstructFinalize;
	pIf->Destruct = tcpsrvDestruct;

	pIf->configureTCPListen = configureTCPListen;
	pIf->create_tcp_socket = create_tcp_socket;
	pIf->Run = Run;

	pIf->SetKeepAlive = SetKeepAlive;
	pIf->SetKeepAliveIntvl = SetKeepAliveIntvl;
	pIf->SetKeepAliveProbes = SetKeepAliveProbes;
	pIf->SetKeepAliveTime = SetKeepAliveTime;
	pIf->SetGnutlsPriorityString = SetGnutlsPriorityString;
	pIf->SetUsrP = SetUsrP;
	pIf->SetInputName = SetInputName;
	pIf->SetOrigin = SetOrigin;
	pIf->SetDfltTZ = SetDfltTZ;
	pIf->SetbSPFramingFix = SetbSPFramingFix;
	pIf->SetAddtlFrameDelim = SetAddtlFrameDelim;
	pIf->SetMaxFrameSize = SetMaxFrameSize;
	pIf->SetbDisableLFDelim = SetbDisableLFDelim;
	pIf->SetDiscardTruncatedMsg = SetDiscardTruncatedMsg;
	pIf->SetSessMax = SetSessMax;
	pIf->SetUseFlowControl = SetUseFlowControl;
	pIf->SetLstnMax = SetLstnMax;
	pIf->SetDrvrMode = SetDrvrMode;
	pIf->SetDrvrAuthMode = SetDrvrAuthMode;
	pIf->SetDrvrPermitExpiredCerts = SetDrvrPermitExpiredCerts;
	pIf->SetDrvrCAFile = SetDrvrCAFile;
	pIf->SetDrvrCRLFile = SetDrvrCRLFile;
	pIf->SetDrvrKeyFile = SetDrvrKeyFile;
	pIf->SetDrvrCertFile = SetDrvrCertFile;
	pIf->SetDrvrName = SetDrvrName;
	pIf->SetDrvrPermPeers = SetDrvrPermPeers;
	pIf->SetCBIsPermittedHost = SetCBIsPermittedHost;
	pIf->SetCBOpenLstnSocks = SetCBOpenLstnSocks;
	pIf->SetCBRcvData = SetCBRcvData;
	pIf->SetCBOnListenDeinit = SetCBOnListenDeinit;
	pIf->SetCBOnSessAccept = SetCBOnSessAccept;
	pIf->SetCBOnSessConstructFinalize = SetCBOnSessConstructFinalize;
	pIf->SetCBOnSessDestruct = SetCBOnSessDestruct;
	pIf->SetCBOnDestruct = SetCBOnDestruct;
	pIf->SetCBOnRegularClose = SetCBOnRegularClose;
	pIf->SetCBOnErrClose = SetCBOnErrClose;
	pIf->SetOnMsgReceive = SetOnMsgReceive;
	pIf->SetLinuxLikeRatelimiters = SetLinuxLikeRatelimiters;
	pIf->SetNotificationOnRemoteClose = SetNotificationOnRemoteClose;
	pIf->SetNotificationOnRemoteOpen = SetNotificationOnRemoteOpen;
	pIf->SetPreserveCase = SetPreserveCase;
	pIf->SetDrvrCheckExtendedKeyUsage = SetDrvrCheckExtendedKeyUsage;
	pIf->SetDrvrPrioritizeSAN = SetDrvrPrioritizeSAN;
	pIf->SetDrvrTlsVerifyDepth = SetDrvrTlsVerifyDepth;
	pIf->SetSynBacklog = SetSynBacklog;

finalize_it:
ENDobjQueryInterface(tcpsrv)


/* exit our class
 * rgerhards, 2008-03-10
 */
BEGINObjClassExit(tcpsrv, OBJ_IS_LOADABLE_MODULE) /* CHANGE class also in END MACRO! */
CODESTARTObjClassExit(tcpsrv)
	/* release objects we no longer need */
	objRelease(tcps_sess, DONT_LOAD_LIB);
	objRelease(conf, CORE_COMPONENT);
	objRelease(prop, CORE_COMPONENT);
	objRelease(statsobj, CORE_COMPONENT);
	objRelease(ruleset, CORE_COMPONENT);
	objRelease(glbl, CORE_COMPONENT);
	objRelease(netstrms, DONT_LOAD_LIB);
	objRelease(nssel, DONT_LOAD_LIB);
	objRelease(netstrm, LM_NETSTRMS_FILENAME);
	objRelease(net, LM_NET_FILENAME);
ENDObjClassExit(tcpsrv)


/* Initialize our class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-29
 */
BEGINObjClassInit(tcpsrv, 1, OBJ_IS_LOADABLE_MODULE) /* class, version - CHANGE class also in END MACRO! */
	/* request objects we use */
	CHKiRet(objUse(net, LM_NET_FILENAME));
	CHKiRet(objUse(netstrms, LM_NETSTRMS_FILENAME));
	CHKiRet(objUse(netstrm, DONT_LOAD_LIB));
	CHKiRet(objUse(nssel, DONT_LOAD_LIB));
	CHKiRet(objUse(nspoll, DONT_LOAD_LIB));
	CHKiRet(objUse(tcps_sess, DONT_LOAD_LIB));
	CHKiRet(objUse(conf, CORE_COMPONENT));
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(ruleset, CORE_COMPONENT));
	CHKiRet(objUse(statsobj, CORE_COMPONENT));
	CHKiRet(objUse(prop, CORE_COMPONENT));

	/* set our own handlers */
	OBJSetMethodHandler(objMethod_DEBUGPRINT, tcpsrvDebugPrint);
	OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, tcpsrvConstructFinalize);
ENDObjClassInit(tcpsrv)


/* start worker threads
 * Important: if we fork, this MUST be done AFTER forking
 */
static void
startWorkerPool(tcpsrv_t *const pThis)
{
	int i;
	int r;
	pthread_attr_t sessThrdAttr;

	/* We need to temporarily block all signals because the new thread
	 * inherits our signal mask. There is a race if we do not block them
	 * now, and we have seen in practice that this race causes grief.
	 * So we 1. save the current set, 2. block evertyhing, 3. start
	 * threads, and 4 reset the current set to saved state.
	 * rgerhards, 2019-08-16
	 */
	sigset_t sigSet, sigSetSave;
	sigfillset(&sigSet);
	pthread_sigmask(SIG_SETMASK, &sigSet, &sigSetSave);

	pThis->wrkrRunning = 0;
	pthread_cond_init(&pThis->wrkrIdle, NULL);
	pthread_attr_init(&sessThrdAttr);
	pthread_attr_setstacksize(&sessThrdAttr, 4096*1024);
	for(i = 0 ; i < pThis->wrkrMax ; ++i) {
		/* init worker info structure! */
		pthread_cond_init(&pThis->wrkrInfo[i].run, NULL);
		pThis->wrkrInfo[i].pSrv = NULL;
		pThis->wrkrInfo[i].numCalled = 0;
		r = pthread_create(&pThis->wrkrInfo[i].tid, &sessThrdAttr, wrkr, &(pThis->wrkrInfo[i]));
		if(r == 0) {
			pThis->wrkrInfo[i].enabled = 1;
		} else {
			LogError(r, NO_ERRCODE, "tcpsrv error creating thread");
		}
	}
	pthread_attr_destroy(&sessThrdAttr);
	pthread_sigmask(SIG_SETMASK, &sigSetSave, NULL);
}


/* destroy worker pool structures and wait for workers to terminate
 */
static void
stopWorkerPool(tcpsrv_t *const pThis)
{
	int i;

	for(i = 0 ; i < pThis->wrkrMax ; ++i) {
		pthread_mutex_lock(&pThis->wrkrMut);
		pthread_cond_signal(&pThis->wrkrInfo[i].run); /* awake wrkr if not running */
		pthread_mutex_unlock(&pThis->wrkrMut);
		pthread_join(pThis->wrkrInfo[i].tid, NULL);
		DBGPRINTF("tcpsrv: info: worker %d was called %llu times\n", i, pThis->wrkrInfo[i].numCalled);
		pthread_cond_destroy(&pThis->wrkrInfo[i].run);
	}
	pthread_cond_destroy(&pThis->wrkrIdle);
}


/* --------------- here now comes the plumbing that makes as a library module --------------- */

BEGINmodExit
CODESTARTmodExit
	/* de-init in reverse order! */
	tcpsrvClassExit();
	tcps_sessClassExit();
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_LIB_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
	/* we just init the worker mutex, but do not start the workers themselves. This is deferred
	 * to the first call of Run(). Reasons for this:
	 * 1. depending on load order, tcpsrv gets loaded during rsyslog startup BEFORE
	 *    it forks, in which case the workers would be running in the then-killed parent,
	 *    leading to a defuncnt child (we actually had this bug).
	 * 2. depending on circumstances, Run() would possibly never be called, in which case
	 *    the worker threads would be totally useless.
	 * Note that in order to guarantee a non-racy worker start, we need to guard the
	 * startup sequence by a mutex, which is why we init it here (no problem with fork()
	 * in this case as the mutex is a pure-memory structure).
	 * rgerhards, 2012-05-18
	 */

	/* Initialize all classes that are in our module - this includes ourselfs */
	CHKiRet(tcps_sessClassInit(pModInfo));
	CHKiRet(tcpsrvClassInit(pModInfo)); /* must be done after tcps_sess, as we use it */
ENDmodInit
