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
 * File begun on 2007-12-21 by RGerhards (extracted from syslogd.c)
 *
 * Copyright 2007-2010 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Rsyslog is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rsyslog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Rsyslog.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */

#include "config.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <ctype.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
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
#include "unicode-helper.h"

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
DEFobjCurrIf(errmsg)
DEFobjCurrIf(net)
DEFobjCurrIf(netstrms)
DEFobjCurrIf(netstrm)
DEFobjCurrIf(nssel)
DEFobjCurrIf(nspoll)
DEFobjCurrIf(prop)


/* add new listener port to listener port list
 * rgerhards, 2009-05-21
 */
static inline rsRetVal
addNewLstnPort(tcpsrv_t *pThis, uchar *pszPort)
{
	tcpLstnPortList_t *pEntry;
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, tcpsrv);

	/* create entry */
	CHKmalloc(pEntry = MALLOC(sizeof(tcpLstnPortList_t)));
	pEntry->pszPort = pszPort;
	pEntry->pSrv = pThis;
	pEntry->pRuleset = pThis->pRuleset;

	/* we need to create a property */ 
	CHKiRet(prop.Construct(&pEntry->pInputName));
	CHKiRet(prop.SetString(pEntry->pInputName, pThis->pszInputName, ustrlen(pThis->pszInputName)));
	CHKiRet(prop.ConstructFinalize(pEntry->pInputName));

	/* and add to list */
	pEntry->pNext = pThis->pLstnPorts;
	pThis->pLstnPorts = pEntry;

finalize_it:
	RETiRet;
}


/* configure TCP listener settings.
 * Note: pszPort is handed over to us - the caller MUST NOT free it!
 * rgerhards, 2008-03-20
 */
static rsRetVal
configureTCPListen(tcpsrv_t *pThis, uchar *pszPort)
{
	int i;
	uchar *pPort = pszPort;
	DEFiRet;

	assert(pszPort != NULL);
	ISOBJ_TYPE_assert(pThis, tcpsrv);

	/* extract port */
	i = 0;
	while(isdigit((int) *pPort)) {
		i = i * 10 + *pPort++ - '0';
	}

	if(i >= 0 && i <= 65535) {
		CHKiRet(addNewLstnPort(pThis, pszPort));
	} else {
		errmsg.LogError(0, NO_ERRCODE, "Invalid TCP listen port %s - ignored.\n", pszPort);
	}

finalize_it:
	RETiRet;
}


/* Initialize the session table
 * returns 0 if OK, somewhat else otherwise
 */
static rsRetVal
TCPSessTblInit(tcpsrv_t *pThis)
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
TCPSessTblFindFreeSpot(tcpsrv_t *pThis)
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

	BEGINfunc
	ISOBJ_TYPE_assert(pThis, tcpsrv);
	assert(pThis->pSessions != NULL);
	for(i = iCurr + 1 ; i < pThis->iSessMax ; ++i) {
		if(pThis->pSessions[i] != NULL)
			break;
	}

	ENDfunc
	return((i < pThis->iSessMax) ? i : -1);
}


/* De-Initialize TCP listner sockets.
 * This function deinitializes everything, including freeing the
 * session table. No TCP listen receive operations are permitted
 * unless the subsystem is reinitialized.
 * rgerhards, 2007-06-21
 */
static void deinit_tcp_listener(tcpsrv_t *pThis)
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
		free(pEntry->pszPort);
		prop.Destruct(&pEntry->pInputName);
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
addTcpLstn(void *pUsr, netstrm_t *pLstn)
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
 * rgerhards, 2009-05-21
 */
static inline rsRetVal
initTCPListener(tcpsrv_t *pThis, tcpLstnPortList_t *pPortEntry)
{
	DEFiRet;
	uchar *TCPLstnPort;

	ISOBJ_TYPE_assert(pThis, tcpsrv);
	assert(pPortEntry != NULL);

	if(!ustrcmp(pPortEntry->pszPort, UCHAR_CONSTANT("0")))
		TCPLstnPort = UCHAR_CONSTANT("514");
		/* use default - we can not do service db update, because there is
		 * no IANA-assignment for syslog/tcp. In the long term, we might
		 * re-use RFC 3195 port of 601, but that would probably break to
		 * many existing configurations.
		 * rgerhards, 2007-06-28
		 */
	else
		TCPLstnPort = pPortEntry->pszPort;

	/* TODO: add capability to specify local listen address! */
	CHKiRet(netstrm.LstnInit(pThis->pNS, (void*)pPortEntry, addTcpLstn, TCPLstnPort, NULL, pThis->iSessMax));

finalize_it:
	RETiRet;
}


/* Initialize TCP sockets (for listener) and listens on them */
static rsRetVal
create_tcp_socket(tcpsrv_t *pThis)
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
			errmsg.LogError(0, localRet, "Could not create tcp listener, ignoring port %s.", pEntry->pszPort);
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
		errmsg.LogError(0, RS_RET_ERR, "Could not initialize TCP session table, suspending TCP message reception.");
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
SessAccept(tcpsrv_t *pThis, tcpLstnPortList_t *pLstnInfo, tcps_sess_t **ppSess, netstrm_t *pStrm)
{
	DEFiRet;
	tcps_sess_t *pSess = NULL;
	netstrm_t *pNewStrm = NULL;
	int iSess = -1;
	struct sockaddr_storage *addr;
	uchar *fromHostFQDN = NULL;
	uchar *fromHostIP = NULL;

	ISOBJ_TYPE_assert(pThis, tcpsrv);
	assert(pLstnInfo != NULL);

	CHKiRet(netstrm.AcceptConnReq(pStrm, &pNewStrm));

	/* Add to session list */
	iSess = TCPSessTblFindFreeSpot(pThis);
	if(iSess == -1) {
		errno = 0;
		errmsg.LogError(0, RS_RET_MAX_SESS_REACHED, "too many tcp sessions - dropping incoming request");
		ABORT_FINALIZE(RS_RET_MAX_SESS_REACHED);
	}

	/* we found a free spot and can construct our session object */
	CHKiRet(tcps_sess.Construct(&pSess));
	CHKiRet(tcps_sess.SetTcpsrv(pSess, pThis));
	CHKiRet(tcps_sess.SetLstnInfo(pSess, pLstnInfo));
	if(pThis->OnMsgReceive != NULL)
		CHKiRet(tcps_sess.SetOnMsgReceive(pSess, pThis->OnMsgReceive));

	/* get the host name */
	CHKiRet(netstrm.GetRemoteHName(pNewStrm, &fromHostFQDN));
	CHKiRet(netstrm.GetRemoteIP(pNewStrm, &fromHostIP));
	CHKiRet(netstrm.GetRemAddr(pNewStrm, &addr));
	/* TODO: check if we need to strip the domain name here -- rgerhards, 2008-04-24 */

	/* Here we check if a host is permitted to send us messages. If it isn't, we do not further
	 * process the message but log a warning (if we are configured to do this).
	 * rgerhards, 2005-09-26
	 */
	if(!pThis->pIsPermittedHost((struct sockaddr*) addr, (char*) fromHostFQDN, pThis->pUsr, pSess->pUsr)) {
		DBGPRINTF("%s is not an allowed sender\n", fromHostFQDN);
		if(glbl.GetOption_DisallowWarning()) {
			errno = 0;
			errmsg.LogError(0, RS_RET_HOST_NOT_PERMITTED, "TCP message from disallowed sender %s discarded", fromHostFQDN);
		}
		ABORT_FINALIZE(RS_RET_HOST_NOT_PERMITTED);
	}

	/* OK, we have an allowed sender, so let's continue, what
	 * means we can finally fill in the session object.
	 */
	CHKiRet(tcps_sess.SetHost(pSess, fromHostFQDN));
	fromHostFQDN = NULL; /* we handed this string over */
	CHKiRet(tcps_sess.SetHostIP(pSess, fromHostIP));
	fromHostIP = NULL; /* we handed this string over */
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

finalize_it:
	if(iRet != RS_RET_OK) {
		if(pSess != NULL)
			tcps_sess.Destruct(&pSess);
		if(pNewStrm != NULL)
			netstrm.Destruct(&pNewStrm);
		free(fromHostFQDN);
		free(fromHostIP);
	}

	RETiRet;
}


static void
RunCancelCleanup(void *arg)
{
	nssel_t **ppSel = (nssel_t**) arg;

	if(*ppSel != NULL)
		nssel.Destruct(ppSel);
}


/* helper to close a session. Takes status of poll vs. select into consideration.
 * rgerhards, 2009-11-25
 */
static inline rsRetVal
closeSess(tcpsrv_t *pThis, tcps_sess_t **ppSess, nspoll_t *pPoll) {
	DEFiRet;
	if(pPoll != NULL) {
		CHKiRet(nspoll.Ctl(pPoll, (*ppSess)->pStrm, 0, *ppSess, NSDPOLL_IN, NSDPOLL_DEL));
	}
	pThis->pOnRegularClose(*ppSess);
	tcps_sess.Destruct(ppSess);
finalize_it:
	RETiRet;
}


/* process a receive request on one of the streams
 * If pPoll is non-NULL, we have a netstream in epoll mode, which means we need
 * to remove any descriptor we close from the epoll set.
 * rgerhards, 2009-07-020
 */
static rsRetVal
doReceive(tcpsrv_t *pThis, tcps_sess_t **ppSess, nspoll_t *pPoll)
{
	char buf[128*1024]; /* reception buffer - may hold a partial or multiple messages */
	ssize_t iRcvd;
	rsRetVal localRet;
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, tcpsrv);
	DBGPRINTF("netstream %p with new data\n", (*ppSess)->pStrm);
	/* Receive message */
	iRet = pThis->pRcvData(*ppSess, buf, sizeof(buf), &iRcvd);
	switch(iRet) {
	case RS_RET_CLOSED:
		if(pThis->bEmitMsgOnClose) {
			uchar *pszPeer;
			int lenPeer;
			errno = 0;
			prop.GetString((*ppSess)->fromHostIP, &pszPeer, &lenPeer);
			errmsg.LogError(0, RS_RET_PEER_CLOSED_CONN, "Netstream session %p closed by remote peer %s.\n",
					(*ppSess)->pStrm, pszPeer);
		}
		CHKiRet(closeSess(pThis, ppSess, pPoll));
		break;
	case RS_RET_RETRY:
		/* we simply ignore retry - this is not an error, but we also have not received anything */
		break;
	case RS_RET_OK:
		/* valid data received, process it! */
		localRet = tcps_sess.DataRcvd(*ppSess, buf, iRcvd);
		if(localRet != RS_RET_OK && localRet != RS_RET_QUEUE_FULL) {
			/* in this case, something went awfully wrong.
			 * We are instructed to terminate the session.
			 */
			errmsg.LogError(0, localRet, "Tearing down TCP Session - see "
					    "previous messages for reason(s)\n");
			CHKiRet(closeSess(pThis, ppSess, pPoll));
		}
		break;
	default:
		errno = 0;
		errmsg.LogError(0, iRet, "netstream session %p will be closed due to error\n",
				(*ppSess)->pStrm);
		CHKiRet(closeSess(pThis, ppSess, pPoll));
		break;
	}

finalize_it:
	RETiRet;
}


/* This function is called to gather input.
 * This variant here is only used if we need to work with a netstream driver
 * that does not support epoll().
 */
#pragma GCC diagnostic ignored "-Wempty-body"
static inline rsRetVal
RunSelect(tcpsrv_t *pThis)
{
	DEFiRet;
	int nfds;
	int i;
	int iTCPSess;
	int bIsReady;
	tcps_sess_t *pNewSess;
	nssel_t *pSel = NULL;

	ISOBJ_TYPE_assert(pThis, tcpsrv);

	/* this is an endless loop - it is terminated by the framework canelling
	 * this thread. Thus, we also need to instantiate a cancel cleanup handler
	 * to prevent us from leaking anything. -- rgerhards, 20080-04-24
	 */
	pthread_cleanup_push(RunCancelCleanup, (void*) &pSel);
	while(1) {
		CHKiRet(nssel.Construct(&pSel));
		// TODO: set driver
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
			/* now get next... */
			iTCPSess = TCPSessGetNxtSess(pThis, iTCPSess);
		}

		/* wait for io to become ready */
		CHKiRet(nssel.Wait(pSel, &nfds));
		if(glbl.GetGlobalInputTermState() == 1)
			break; /* terminate input! */

		for(i = 0 ; i < pThis->iLstnCurr ; ++i) {
			if(glbl.GetGlobalInputTermState() == 1)
				ABORT_FINALIZE(RS_RET_FORCE_TERM);
			CHKiRet(nssel.IsReady(pSel, pThis->ppLstn[i], NSDSEL_RD, &bIsReady, &nfds));
			if(bIsReady) {
				DBGPRINTF("New connect on NSD %p.\n", pThis->ppLstn[i]);
				SessAccept(pThis, pThis->ppLstnPort[i], &pNewSess, pThis->ppLstn[i]);
				--nfds; /* indicate we have processed one */
			}
		}

		/* now check the sessions */
		iTCPSess = TCPSessGetNxtSess(pThis, -1);
		while(nfds && iTCPSess != -1) {
			if(glbl.GetGlobalInputTermState() == 1)
				ABORT_FINALIZE(RS_RET_FORCE_TERM);
			CHKiRet(nssel.IsReady(pSel, pThis->pSessions[iTCPSess]->pStrm, NSDSEL_RD, &bIsReady, &nfds));
			if(bIsReady) {
				doReceive(pThis, &pThis->pSessions[iTCPSess], NULL);
				--nfds; /* indicate we have processed one */
			}
			iTCPSess = TCPSessGetNxtSess(pThis, iTCPSess);
		}
		CHKiRet(nssel.Destruct(&pSel));
finalize_it: /* this is a very special case - this time only we do not exit the function,
	      * because that would not help us either. So we simply retry it. Let's see
	      * if that actually is a better idea. Exiting the loop wasn't we always
	      * crashed, which made sense (the rest of the engine was not prepared for
	      * that) -- rgerhards, 2008-05-19
	      */
		/*EMPTY*/;
	}

	/* note that this point is usually not reached */
	pthread_cleanup_pop(1); /* remove cleanup handler */

	RETiRet;
}
#pragma GCC diagnostic warning "-Wempty-body"


/* This function is called to gather input. It tries doing that via the epoll()
 * interface. If the driver does not support that, it falls back to calling its
 * select() equivalent.
 * rgerhards, 2009-11-18
 */
static rsRetVal
Run(tcpsrv_t *pThis)
{
	DEFiRet;
	int i;
	tcps_sess_t *pNewSess;
	nspoll_t *pPoll = NULL;
	void *pUsr;
	rsRetVal localRet;

	ISOBJ_TYPE_assert(pThis, tcpsrv);

	/* this is an endless loop - it is terminated by the framework canelling
	 * this thread. Thus, we also need to instantiate a cancel cleanup handler
	 * to prevent us from leaking anything. -- rgerhards, 20080-04-24
	 */
	if((localRet = nspoll.Construct(&pPoll)) == RS_RET_OK) {
		// TODO: set driver
		localRet = nspoll.ConstructFinalize(pPoll);
	}
	if(localRet != RS_RET_OK) {
		/* fall back to select */
		dbgprintf("tcpsrv could not use epoll() interface, iRet=%d, using select()\n", localRet);
		iRet = RunSelect(pThis);
		FINALIZE;
	}

	dbgprintf("tcpsrv uses epoll() interface, nsdpol driver found\n");

	/* flag that we are in epoll mode */
	pThis->bUsingEPoll = TRUE;

	/* Add the TCP listen sockets to the list of sockets to monitor */
	for(i = 0 ; i < pThis->iLstnCurr ; ++i) {
		dbgprintf("Trying to add listener %d, pUsr=%p\n", i, pThis->ppLstn);
		CHKiRet(nspoll.Ctl(pPoll, pThis->ppLstn[i], i, pThis->ppLstn, NSDPOLL_IN, NSDPOLL_ADD));
		dbgprintf("Added listener %d\n", i);
	}

	while(1) {
		localRet = nspoll.Wait(pPoll, -1, &i, &pUsr);
		if(glbl.GetGlobalInputTermState() == 1)
			break; /* terminate input! */

		/* check if we need to ignore the i/o ready state. We do this if we got an invalid
		 * return state. Validly, this can happen for RS_RET_EINTR, for other cases it may
		 * not be the right thing, but what is the right thing is really hard at this point...
		 */
		if(localRet != RS_RET_OK)
			continue;

		dbgprintf("poll returned with i %d, pUsr %p\n", i, pUsr);

		if(pUsr == pThis->ppLstn) {
			DBGPRINTF("New connect on NSD %p.\n", pThis->ppLstn[i]);
			SessAccept(pThis, pThis->ppLstnPort[i], &pNewSess, pThis->ppLstn[i]);
			CHKiRet(nspoll.Ctl(pPoll, pNewSess->pStrm, 0, pNewSess, NSDPOLL_IN, NSDPOLL_ADD));
			DBGPRINTF("New session created with NSD %p.\n", pNewSess);
		} else {
			pNewSess = (tcps_sess_t*) pUsr;
			doReceive(pThis, &pNewSess, pPoll);
		}
	}

	/* remove the tcp listen sockets from the epoll set */
	for(i = 0 ; i < pThis->iLstnCurr ; ++i) {
		CHKiRet(nspoll.Ctl(pPoll, pThis->ppLstn[i], i, pThis->ppLstn, NSDPOLL_IN, NSDPOLL_DEL));
	}

finalize_it:
	if(pPoll != NULL)
		nspoll.Destruct(&pPoll);
	RETiRet;
}


/* Standard-Constructor */
BEGINobjConstruct(tcpsrv) /* be sure to specify the object type also in END macro! */
	pThis->iSessMax = TCPSESS_MAX_DEFAULT;
	pThis->iLstnMax = TCPLSTN_MAX_DEFAULT;
	pThis->addtlFrameDelim = TCPSRV_NO_ADDTL_DELIMITER;
	pThis->bDisableLFDelim = 0;
	pThis->OnMsgReceive = NULL;
ENDobjConstruct(tcpsrv)


/* ConstructionFinalizer */
static rsRetVal
tcpsrvConstructFinalize(tcpsrv_t *pThis)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, tcpsrv);

	/* prepare network stream subsystem */
	CHKiRet(netstrms.Construct(&pThis->pNS));
	CHKiRet(netstrms.SetDrvrMode(pThis->pNS, pThis->iDrvrMode));
	if(pThis->pszDrvrAuthMode != NULL)
		CHKiRet(netstrms.SetDrvrAuthMode(pThis->pNS, pThis->pszDrvrAuthMode));
	if(pThis->pPermPeers != NULL)
		CHKiRet(netstrms.SetDrvrPermPeers(pThis->pNS, pThis->pPermPeers));
	// TODO: set driver!
	CHKiRet(netstrms.ConstructFinalize(pThis->pNS));

	/* set up listeners */
	CHKmalloc(pThis->ppLstn = calloc(pThis->iLstnMax, sizeof(netstrm_t*)));
	CHKmalloc(pThis->ppLstnPort = calloc(pThis->iLstnMax, sizeof(tcpLstnPortList_t*)));
	iRet = pThis->OpenLstnSocks(pThis);

finalize_it:
	if(iRet != RS_RET_OK) {
		if(pThis->pNS != NULL)
			netstrms.Destruct(&pThis->pNS);
	}
	RETiRet;
}


/* destructor for the tcpsrv object */
BEGINobjDestruct(tcpsrv) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(tcpsrv)
	if(pThis->OnDestruct != NULL)
		pThis->OnDestruct(pThis->pUsr);

	deinit_tcp_listener(pThis);

	if(pThis->pNS != NULL)
		netstrms.Destruct(&pThis->pNS);
	free(pThis->pszDrvrAuthMode);
	free(pThis->ppLstn);
	free(pThis->ppLstnPort);
	free(pThis->pszInputName);
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
SetCBRcvData(tcpsrv_t *pThis, rsRetVal (*pRcvData)(tcps_sess_t*, char*, size_t, ssize_t*))
{
	DEFiRet;
	pThis->pRcvData = pRcvData;
	RETiRet;
}

static rsRetVal
SetCBOnListenDeinit(tcpsrv_t *pThis, int (*pCB)(void*))
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


/* Set additional framing to use (if any) -- rgerhards, 2008-12-10 */
static rsRetVal
SetAddtlFrameDelim(tcpsrv_t *pThis, int iDelim)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, tcpsrv);
	pThis->addtlFrameDelim = iDelim;
	RETiRet;
}


/* Set the input name to use -- rgerhards, 2008-12-10 */
static rsRetVal
SetInputName(tcpsrv_t *pThis, uchar *name)
{
	uchar *pszName;
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, tcpsrv);
	if(name == NULL)
		pszName = NULL;
	else
		CHKmalloc(pszName = ustrdup(name));
	free(pThis->pszInputName);
	pThis->pszInputName = pszName;
finalize_it:
	RETiRet;
}


/* Set the ruleset (ptr) to use */
static rsRetVal
SetRuleset(tcpsrv_t *pThis, ruleset_t *pRuleset)
{
	DEFiRet;
	pThis->pRuleset = pRuleset;
	RETiRet;
}


/* Set connection close notification */
static rsRetVal
SetNotificationOnRemoteClose(tcpsrv_t *pThis, int bNewVal)
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
SetDrvrMode(tcpsrv_t *pThis, int iMode)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, tcpsrv);
	pThis->iDrvrMode = iMode;
	RETiRet;
}


/* set the driver authentication mode -- rgerhards, 2008-05-19 */
static rsRetVal
SetDrvrAuthMode(tcpsrv_t *pThis, uchar *mode)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, tcpsrv);
	CHKmalloc(pThis->pszDrvrAuthMode = ustrdup(mode));
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

	pIf->SetUsrP = SetUsrP;
	pIf->SetInputName = SetInputName;
	pIf->SetAddtlFrameDelim = SetAddtlFrameDelim;
	pIf->SetbDisableLFDelim = SetbDisableLFDelim;
	pIf->SetSessMax = SetSessMax;
	pIf->SetLstnMax = SetLstnMax;
	pIf->SetDrvrMode = SetDrvrMode;
	pIf->SetDrvrAuthMode = SetDrvrAuthMode;
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
	pIf->SetRuleset = SetRuleset;
	pIf->SetNotificationOnRemoteClose = SetNotificationOnRemoteClose;

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
	objRelease(ruleset, CORE_COMPONENT);
	objRelease(glbl, CORE_COMPONENT);
	objRelease(errmsg, CORE_COMPONENT);
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
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(net, LM_NET_FILENAME));
	CHKiRet(objUse(netstrms, LM_NETSTRMS_FILENAME));
	CHKiRet(objUse(netstrm, DONT_LOAD_LIB));
	CHKiRet(objUse(nssel, DONT_LOAD_LIB));
	CHKiRet(objUse(nspoll, DONT_LOAD_LIB));
	CHKiRet(objUse(tcps_sess, DONT_LOAD_LIB));
	CHKiRet(objUse(conf, CORE_COMPONENT));
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(ruleset, CORE_COMPONENT));
	CHKiRet(objUse(prop, CORE_COMPONENT));

	/* set our own handlers */
	OBJSetMethodHandler(objMethod_DEBUGPRINT, tcpsrvDebugPrint);
	OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, tcpsrvConstructFinalize);
ENDObjClassInit(tcpsrv)


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

	/* Initialize all classes that are in our module - this includes ourselfs */
	CHKiRet(tcps_sessClassInit(pModInfo));
	CHKiRet(tcpsrvClassInit(pModInfo)); /* must be done after tcps_sess, as we use it */
ENDmodInit

/* vim:set ai:
 */
