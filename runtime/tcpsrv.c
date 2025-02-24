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
//#undef HAVE_EPOLL_CREATE // TODO: remove, testing aid!
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
#if defined(HAVE_SYS_EPOLL_H)
#	include <sys/epoll.h>
#endif
#if !defined(HAVE_EPOLL_CREATE)
#include <sys/select.h>
#include <sys/poll.h>
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
DEFobjCurrIf(prop)
DEFobjCurrIf(statsobj)

#define NSPOLL_MAX_EVENTS_PER_WAIT 128



/* We check which event notification mechanism we have and use the best available one.
 * We switch back from library-specific drivers, because event notification always works
 * at the socket layer and is conceptually the same. As such, we can reduce code, code
 * complexity, and a bit of runtime overhead by using a compile time selection and
 * abstraction of the event notification mechanism.
 * rgerhards, 2025-01-16
 */
#if defined(HAVE_EPOLL_CREATE)

static rsRetVal
eventNotify_init(tcpsrv_t *const pThis)
{
	DEFiRet;
#if defined(EPOLL_CLOEXEC) && defined(HAVE_EPOLL_CREATE1)
	DBGPRINTF("tcpsrv uses epoll_create1()\n");
	pThis->evtdata.epoll.efd = epoll_create1(EPOLL_CLOEXEC);
	if(pThis->evtdata.epoll.efd < 0 && errno == ENOSYS)
#endif
	{
		DBGPRINTF("tcpsrv uses epoll_create()\n");
		pThis->evtdata.epoll.efd = epoll_create(100); /* size is ignored in newer kernels, but 100 is not bad... */
	}

	if(pThis->evtdata.epoll.efd < 0) {
		DBGPRINTF("epoll_create1() could not create fd\n");
		ABORT_FINALIZE(RS_RET_IO_ERROR);
	}
finalize_it:
	RETiRet;
}


static rsRetVal
eventNotify_exit(tcpsrv_t *const pThis)
{
	DEFiRet;
	close(pThis->evtdata.epoll.efd);
	RETiRet;
}


/* Modify socket set */
static rsRetVal
epoll_Ctl(tcpsrv_t *const pThis, tcpsrv_io_descr_t *const pioDescr, const int isLstn, const int op)
{
	DEFiRet;

	const int id = pioDescr->id;
	const int sock = pioDescr->sock;
	assert(sock != 0);

	if(op == EPOLL_CTL_ADD) {
		dbgprintf("adding epoll entry %d, socket %d\n", id, sock);
		pioDescr->event.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLONESHOT;
		pioDescr->event.data.ptr = (void*) pioDescr;
		if(epoll_ctl(pThis->evtdata.epoll.efd, EPOLL_CTL_ADD,  sock, &pioDescr->event) < 0) {
			LogError(errno, RS_RET_ERR_EPOLL_CTL,
				"epoll_ctl failed on fd %d, isLstn %d\n",
				sock, isLstn);
		}
	} else if(op == EPOLL_CTL_DEL) {
		dbgprintf("removing epoll entry %d, socket %d\n", id, sock);
		if(epoll_ctl(pThis->evtdata.epoll.efd, EPOLL_CTL_DEL, sock, NULL) < 0) {
			LogError(errno, RS_RET_ERR_EPOLL_CTL,
				"epoll_ctl failed on fd %d, isLstn %d\n",
				sock, isLstn);
			ABORT_FINALIZE(RS_RET_ERR_EPOLL_CTL);
		}
	} else {
		dbgprintf("program error: invalid NSDPOLL_mode %d - ignoring request\n", op);
		ABORT_FINALIZE(RS_RET_ERR);
	}

finalize_it:
	DBGPRINTF("Done processing epoll entry %d, iRet %d\n", id, iRet);
	RETiRet;
}


/* Wait for io to become ready. After the successful call, idRdy contains the
 * id set by the caller for that i/o event, ppUsr is a pointer to a location
 * where the user pointer shall be stored.
 * numEntries contains the maximum number of entries on entry and the actual
 * number of entries actually read on exit.
 * rgerhards, 2009-11-18
 */
static rsRetVal
epoll_Wait(tcpsrv_t *const pThis, const int timeout, int *const numEntries, tcpsrv_io_descr_t *pWorkset[])
{
	struct epoll_event event[NSPOLL_MAX_EVENTS_PER_WAIT];
	int nfds;
	int i;
	DEFiRet;

	assert(pWorkset != NULL);

	if(*numEntries > NSPOLL_MAX_EVENTS_PER_WAIT)
		*numEntries = NSPOLL_MAX_EVENTS_PER_WAIT;
	DBGPRINTF("doing epoll_wait for max %d events\n", *numEntries);
	nfds = epoll_wait(pThis->evtdata.epoll.efd, event, *numEntries, timeout);
	if(nfds == -1) {
		if(errno == EINTR) {
			ABORT_FINALIZE(RS_RET_EINTR);
		} else {
			DBGPRINTF("epoll_wait() returned with error code %d\n", errno);
			ABORT_FINALIZE(RS_RET_ERR_EPOLL);
		}
	} else if(nfds == 0) {
		ABORT_FINALIZE(RS_RET_TIMEOUT);
	}

	/* we got valid events, so tell the caller... */
	DBGPRINTF("epoll_wait returned %d entries\n", nfds);
	for(i = 0 ; i < nfds ; ++i) {
		pWorkset[i] = event[i].data.ptr;
		/* default is no error, on error we terminate, so we need only to set in error case! */
		if(event[i].events & EPOLLERR) {
			ATOMIC_STORE_1_TO_INT(&pWorkset[i]->isInError, &pWorkset[i]->mut_isInError);
		}
	}
	*numEntries = nfds;

finalize_it:
	RETiRet;
}


#else /* no epoll, let's use select  ------------------------------------------------------------ */
#define FDSET_INCREMENT 1024 /* increment for struct pollfds array allocation */


static rsRetVal
eventNotify_init(tcpsrv_t *const pThis ATTR_UNUSED)
{
	DEFiRet;
	RETiRet;
}


static rsRetVal
eventNotify_exit(tcpsrv_t *const pThis)
{
	DEFiRet;
	free(pThis->evtdata.poll.fds);
	RETiRet;
}


/* Add a socket to the select set */
static rsRetVal ATTR_NONNULL()
select_Add(tcpsrv_t *const pThis, netstrm_t *const pStrm, const nsdsel_waitOp_t waitOp)
{
	DEFiRet;
	int sock;

	CHKiRet(netstrm.GetSock(pStrm, &sock));

	if(pThis->evtdata.poll.currfds == pThis->evtdata.poll.maxfds) {
		struct pollfd *newfds;
		CHKmalloc(newfds = realloc(pThis->evtdata.poll.fds,
			sizeof(struct pollfd) * (pThis->evtdata.poll.maxfds + FDSET_INCREMENT)));
		pThis->evtdata.poll.maxfds += FDSET_INCREMENT;
		pThis->evtdata.poll.fds = newfds;
	}

	switch(waitOp) {
		case NSDSEL_RD:
			pThis->evtdata.poll.fds[pThis->evtdata.poll.currfds].events = POLLIN;
			break;
		case NSDSEL_WR:
			pThis->evtdata.poll.fds[pThis->evtdata.poll.currfds].events = POLLOUT;
			break;
		case NSDSEL_RDWR:
			pThis->evtdata.poll.fds[pThis->evtdata.poll.currfds].events = POLLIN | POLLOUT;
			break;
	}
	pThis->evtdata.poll.fds[pThis->evtdata.poll.currfds].fd = sock;
	++pThis->evtdata.poll.currfds;

finalize_it:
	RETiRet;
}


/* perform the select()  piNumReady returns how many descriptors are ready for IO
 * TODO: add timeout!
 */
static rsRetVal ATTR_NONNULL()
select_Poll(tcpsrv_t *const pThis, int *const piNumReady)
{
	DEFiRet;

	assert(piNumReady != NULL);

	/* Output debug first*/
	if(Debug) {
		dbgprintf("calling poll, active fds (%d): ", pThis->evtdata.poll.currfds);
		for(uint32_t i = 0; i <= pThis->evtdata.poll.currfds; ++i)
			dbgprintf("%d ", pThis->evtdata.poll.fds[i].fd);
		dbgprintf("\n");
	}
	assert(pThis->evtdata.poll.currfds >= 1);

	/* now do the select */
	*piNumReady = poll(pThis->evtdata.poll.fds, pThis->evtdata.poll.currfds, -1);
	if(*piNumReady < 0) {
		if(errno == EINTR) {
			DBGPRINTF("tcpsrv received EINTR\n");
		} else {
			LogMsg(errno, RS_RET_POLL_ERR, LOG_WARNING,
				"ndssel_ptcp: poll system call failed, may cause further troubles");
		}
		*piNumReady = 0;
	}

	RETiRet;
}


/* check if a socket is ready for IO */
static rsRetVal ATTR_NONNULL()
select_IsReady(tcpsrv_t *const pThis, netstrm_t *const pStrm, const nsdsel_waitOp_t waitOp, int *const pbIsReady)
{
	DEFiRet;
	int sock;

	CHKiRet(netstrm.GetSock(pStrm, &sock));

	// TODO: consider doing a binary search

	uint32_t idx;
	for(idx = 0 ; idx < pThis->evtdata.poll.currfds ; ++idx) {
		if(pThis->evtdata.poll.fds[idx].fd == sock)
			break;
	}
	if(idx >= pThis->evtdata.poll.currfds) {
		LogMsg(0, RS_RET_INTERNAL_ERROR, LOG_ERR,
			"ndssel_ptcp: could not find socket %d which should be present", sock);
		ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
	}

	const short revent = pThis->evtdata.poll.fds[idx].revents;
	if (revent & POLLNVAL) {
		DBGPRINTF("ndssel_ptcp: revent & POLLNVAL is TRUE, we had a race, ignoring, revent = %d", revent);
		*pbIsReady = 0;
	}
	switch(waitOp) {
		case NSDSEL_RD:
			*pbIsReady = revent & POLLIN;
			break;
		case NSDSEL_WR:
			*pbIsReady = revent & POLLOUT;
			break;
		case NSDSEL_RDWR:
			*pbIsReady = revent & (POLLIN | POLLOUT);
			break;
	}

finalize_it:
	RETiRet;
}

#endif /* event notification compile time selection */


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


/* helper to close a session. Takes status of poll vs. select into consideration.
 * rgerhards, 2009-11-25
 */
static rsRetVal
closeSess(tcpsrv_t *const pThis, tcpsrv_io_descr_t *const pioDescr)
{
	DEFiRet;
	assert(pioDescr->ptrType == NSD_PTR_TYPE_SESS);
dbgprintf("RGER: iodescr %p destructed via closeSess\n", pioDescr);
	tcps_sess_t *pSess = pioDescr->ptr.pSess;
	#if defined(HAVE_EPOLL_CREATE)
		CHKiRet(epoll_Ctl(pThis, pioDescr, 0, EPOLL_CTL_DEL));
	#endif
	pThis->pOnRegularClose(pSess);
	pthread_mutex_unlock(&pSess->mut);

	tcps_sess.Destruct(&pSess);
#if defined(HAVE_EPOLL_CREATE)
finalize_it:
#endif
	#if defined(HAVE_EPOLL_CREATE)
		/* in epoll mode, ioDescr is dynamically allocated */
		DESTROY_ATOMIC_HELPER_MUT(pioDescr->mut_isInError);
		free(pioDescr);
	#else
		pThis->pSessions[pioDescr->id] = NULL;
	#endif
	RETiRet;
}



static rsRetVal
notifyReArm(tcpsrv_io_descr_t *const pioDescr)
{
	DEFiRet;

	if(epoll_ctl(pioDescr->pSrv->evtdata.epoll.efd, EPOLL_CTL_MOD, pioDescr->sock, &pioDescr->event) < 0) {
		// TODO: BETTER handling!
		LogError(errno, RS_RET_ERR_EPOLL_CTL, "epoll_ctl failed re-armed socket %d", pioDescr->sock);
		ABORT_FINALIZE(RS_RET_ERR_EPOLL_CTL);
	}
DBGPRINTF("RGER: sock %d re-armed\n", pioDescr->sock);

finalize_it:
	RETiRet;
}

/* process a receive request on one of the streams
 * If in epoll mode, we need to remove any descriptor we close from the epoll set.
 * rgerhards, 2009-07-020
 */
static rsRetVal ATTR_NONNULL(1)
doReceive(tcpsrv_io_descr_t *const pioDescr, int *const pNeedReArm)
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
	int needReArm = 1;
	tcps_sess_t *const pSess = pioDescr->ptr.pSess;
	tcpsrv_t *const pThis = pioDescr->pSrv;
	int freeMutex = 0;

	ISOBJ_TYPE_assert(pThis, tcpsrv);
	prop.GetString((pSess)->fromHostIP, &pszPeer, &lenPeer);
	DBGPRINTF("netstream %p with new data from remote peer %s\n", (pSess)->pStrm, pszPeer);

	pthread_mutex_lock(&pSess->mut);
	freeMutex = 1;

	/* if we had EPOLLERR, give information. The other processing continues. This
	 * seems to be best practice and may cause better error information.
	 */
	if(ATOMIC_FETCH_32BIT(&pioDescr->isInError, &pioDescr->mut_isInError)) {
		int error = 0;
		socklen_t len = sizeof(error);
		const int sock = ((nsd_ptcp_t *) pSess->pStrm->pDrvrData)->sock;
		if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len) == 0) {
			LogError(error, RS_RET_IO_ERROR, "epoll subsystem signalled state EPOLLERR "
				"for stream %p, peer %s  ", (pSess)->pStrm, pszPeer);
		} /* no else - if this fails, we have nothing to report... */
	}

	while(do_run && loop_ctr < 500) {	/*  break happens in switch below! */
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
			needReArm = 0;
			freeMutex = 0;
			closeSess(pThis, pioDescr);
			do_run = 0;
			break;
		case RS_RET_RETRY:
			/* we simply ignore retry - this is not an error, but we also have not received anything */
			needReArm = 1;
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
				needReArm = 0;
				freeMutex = 0;
				CHKiRet(closeSess(pThis, pioDescr));
			}
			break;
		default:
			LogError(oserr, iRet, "netstream session %p from %s will be closed due to error",
					pSess->pStrm, pszPeer);
			needReArm = 0;
				freeMutex = 0;
			closeSess(pThis, pioDescr);
			do_run = 0;
			break;
		}
	}

finalize_it:
	if(freeMutex) {
		pthread_mutex_unlock(&pSess->mut);
	}

	*pNeedReArm = needReArm;
	RETiRet;
}


/* This function processes a single incoming connection */
static rsRetVal ATTR_NONNULL(1)
doSingleAccept(tcpsrv_io_descr_t *const pioDescr, int *const pNeedReArm)
{
	tcps_sess_t *pNewSess = NULL;
	tcpsrv_io_descr_t *pDescrNew = NULL;
	const int idx = pioDescr->id;
	tcpsrv_t *const pThis = pioDescr->pSrv;
	DEFiRet;

	DBGPRINTF("New connect on NSD %p.\n", pThis->ppLstn[idx]);
	iRet = SessAccept(pThis, pThis->ppLstnPort[idx], &pNewSess, pThis->ppLstn[idx]);
	if(iRet == RS_RET_NO_MORE_DATA) {
		goto no_more_data;
	}

	if(iRet == RS_RET_OK) {
		#if defined(HAVE_EPOLL_CREATE)
			/* pDescrNew is only dyn allocated in epoll mode! */
			CHKmalloc(pDescrNew = (tcpsrv_io_descr_t*) calloc(1, sizeof(tcpsrv_io_descr_t)));
dbgprintf("RGER: iodescr %p created\n", pDescrNew);
			pDescrNew->pSrv = pThis;
			pDescrNew->id = idx; // TODO: remove if session handling is refactored to dyn max sessions
			pDescrNew->isInError = 0;
			INIT_ATOMIC_HELPER_MUT(pDescrNew->mut_isInError);
			pDescrNew->ptrType = NSD_PTR_TYPE_SESS;
			CHKiRet(netstrm.GetSock(pNewSess->pStrm, &pDescrNew->sock));
			pDescrNew->ptr.pSess = pNewSess;
			CHKiRet(epoll_Ctl(pThis, pDescrNew, 0, EPOLL_CTL_ADD));
		#endif

		DBGPRINTF("New session created with NSD %p.\n", pNewSess);
	} else {
		DBGPRINTF("tcpsrv: error %d during accept\n", iRet);
	}

#if defined(HAVE_EPOLL_CREATE)
finalize_it:
#endif
	if(iRet != RS_RET_OK) {
		const tcpLstnParams_t *cnf_params = pThis->ppLstnPort[idx]->cnf_params;
		LogError(0, iRet, "tcpsrv listener (inputname: '%s') failed "
			"to process incoming connection with error %d",
			(cnf_params->pszInputName == NULL) ? (uchar*)"*UNSET*" : cnf_params->pszInputName, iRet);
		if(pDescrNew != NULL) {
			DESTROY_ATOMIC_HELPER_MUT(pioDescrNew->mut_isInError);
			free(pDescrNew);
		}
		srSleep(0,20000); /* Sleep 20ms */
	}
no_more_data:
	*pNeedReArm = 1; /* listeners must always be re-aremd */
	RETiRet;
}


/* This function processes all pending accepts on this fd */
static rsRetVal ATTR_NONNULL(1)
doAccept(tcpsrv_io_descr_t *const pioDescr, int *const pNeedReArm)
{
	DEFiRet;
	int bRun = 1;

	while(bRun) {
		iRet = doSingleAccept(pioDescr, pNeedReArm);
		if(iRet != RS_RET_OK) {
			bRun = 0;
		}
	}
	RETiRet;
}


/* process a single workset item
 */
static rsRetVal ATTR_NONNULL(1)
processWorksetItem(tcpsrv_io_descr_t *const pioDescr)
{
	DEFiRet;
	int needReArm;

	DBGPRINTF("tcpsrv: processing item %d, socket %d\n", pioDescr->id, pioDescr->sock);
	if(pioDescr->ptrType == NSD_PTR_TYPE_LSTN) {
		iRet = doAccept(pioDescr, &needReArm);
	} else {
		iRet = doReceive(pioDescr, &needReArm);
	}

	if(needReArm) {
		notifyReArm(pioDescr);
	}
DBGPRINTF("RGER: processWorksetItem returns %d\n", iRet);
	RETiRet;
}


/* work queue functions */

static void * wrkr(void *arg); /* forward-def of wrkr */

static rsRetVal
startWrkrPool(tcpsrv_t *const pThis)
{

	// TODO: make sure caller checks return value
	DEFiRet;
	workQueue_t *const queue = &pThis->workQueue;


	CHKmalloc(pThis->workQueue.wrkr_tids = calloc(queue->numWrkr, sizeof(pthread_t)));
	pThis->currWrkrs = 0;
	pthread_mutex_init(&queue->mut, NULL);
	pthread_cond_init(&queue->workRdy, NULL);
	for(unsigned i = 0; i < queue->numWrkr; i++) {
	//for(unsigned i = 0; i < 1;  i++) {// DEBUGGING ONLY - TODO: remove
		pthread_create(&queue->wrkr_tids[i], NULL, wrkr, pThis);
dbgprintf("RGER: wrkr %u created\n", i);
	}
finalize_it:
	RETiRet;
}

static void
stopWrkrPool(tcpsrv_t *const pThis)
{
	workQueue_t *const queue = &pThis->workQueue;

DBGPRINTF("RGER: stopWrkrPool\n");
	pthread_mutex_lock(&queue->mut);
	//queue->stop = true;
	pthread_cond_broadcast(&queue->workRdy);
DBGPRINTF("RGER: stopWrkrPool broadcasted\n");
	pthread_mutex_unlock(&queue->mut);

	for(unsigned i = 0; i < queue->numWrkr; i++) {
	//for(unsigned i = 0; i < 1;  i++) {// DEBUGGING ONLY - TODO: remove
		pthread_join(queue->wrkr_tids[i], NULL);
	}
	free(pThis->workQueue.wrkr_tids);
DBGPRINTF("RGER: done stopWrkrPool\n");
}

static tcpsrv_io_descr_t *
dequeueWork(tcpsrv_t *pSrv)
{	
	workQueue_t *const queue = &pSrv->workQueue;
	tcpsrv_io_descr_t *pioDescr;

	pthread_mutex_lock(&queue->mut);
	while((queue->head == NULL) && !glbl.GetGlobalInputTermState()) {
dbgprintf("RGER: waiting on condition\n");
		pthread_cond_wait(&queue->workRdy, &queue->mut);
	}

	if(queue->head == NULL) {
		pioDescr = NULL;
		goto finalize_it;
	}

	pioDescr = queue->head;
	queue->head = pioDescr->next;
	if(queue->head == NULL) {
		queue->tail = NULL;
	}
DBGPRINTF("RGER: DEqueuWork done, sock %d\n", pioDescr->sock);

finalize_it:
	pthread_mutex_unlock(&queue->mut);
	return pioDescr;
}

static rsRetVal
enqueueWork(tcpsrv_io_descr_t *const pioDescr)
{	
	workQueue_t *const queue = &pioDescr->pSrv->workQueue;
	//tcpsrv_io_descr_t *pioDescr_copy;
	DEFiRet;

dbgprintf("RGER: iodescr %p used in enqueuWork\n", pioDescr);

	pthread_mutex_lock(&queue->mut);
	pioDescr->next = NULL;
	if(queue->tail == NULL) {
		assert(queue->head == NULL);
		queue->head = pioDescr;
	} else {
		queue->tail->next = pioDescr;
	}
	queue->tail = pioDescr;

	pthread_cond_signal(&queue->workRdy);
	pthread_mutex_unlock(&queue->mut);

DBGPRINTF("RGER: enqueuWork done, sock %d\n", pioDescr->sock);

//finalize_it:
	RETiRet;
}

/* Worker thread function */ // TODO replace!
static void *
wrkr(void *arg)
{
	//rsRetVal localRet;
	tcpsrv_t *const pThis = (tcpsrv_t *) arg;
	workQueue_t *const queue = &pThis->workQueue;
	tcpsrv_io_descr_t *pioDescr;

	pthread_mutex_lock(&queue->mut);
	const int wrkrIdx = pThis->currWrkrs++;
	pthread_mutex_unlock(&queue->mut);
	uchar thrdName[32];
	snprintf((char*)thrdName, sizeof(thrdName), "tcpsrv/w%d", wrkrIdx);
	dbgSetThrdName(thrdName);
#	if defined(HAVE_PRCTL) && defined(PR_SET_NAME)
	/* set thread name - we ignore if the call fails, has no harsh consequences... */
	if(prctl(PR_SET_NAME, thrdName, 0, 0, 0) != 0) {
		DBGPRINTF("prctl failed, not setting thread name for '%s'\n", thrdName);
	}
#	else
	int r = pthread_setname_np(pthread_self(), (char*) thrdName);
	dbgprintf("queueWork: thread name %s, return %d: %s\n", thrdName,r, strerror(r));
#	endif
	
	while(1) {
		pioDescr = dequeueWork(pThis);
		if(pioDescr == NULL) {
			break;
		}
		#if 1
		processWorksetItem(pioDescr); // TODO check result?
		#else
		int isSess_ioDescr = (pioDescr->ptrType == NSD_PTR_TYPE_SESS);

		// TODO: more granular check (at least think about it, esp. in regard to free() */
		if(localRet == RS_RET_RETRY || localRet == RS_RET_NO_MORE_DATA || localRet == RS_RET_OK) {
			if(isSess_ioDescr) {
			//	free((void*) pioDescr);
			}
		}
		else { dbgprintf("RGER: iodescr no free because iRet %d\n", localRet); }
		#endif

	}

	return NULL;
}


/* Process a workset, that is handle io. We become activated
 * from either select or epoll handler. We split the workload
 * out to a pool of threads, but try to avoid context switches
 * as much as possible.
 */
static rsRetVal
processWorkset(const int numEntries, tcpsrv_io_descr_t *const pioDescr[])
{
	int i;
	const unsigned numWrkr = pioDescr[0]->pSrv->workQueue.numWrkr; /* pSrv is always the same! */
	DEFiRet;

	DBGPRINTF("tcpsrv: ready to process %d event entries\n", numEntries);

	for(i = 0 ; i < numEntries ; ++i) {
		if(numWrkr == 1) {
			/* we process all on this thread, no need for context switch */
			processWorksetItem(pioDescr[i]);
		} else {
			iRet = enqueueWork(pioDescr[i]);
		}
	}
	RETiRet;
}


#if !defined(HAVE_EPOLL_CREATE)
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
	rsRetVal localRet;

	ISOBJ_TYPE_assert(pThis, tcpsrv);
	DBGPRINTF("tcpsrv uses select() interface\n");

	/* init the workset pointers, they will remain fixed */
	for(i = 0 ; i < sizeWorkset ; ++i) {
		pWorkset[i] = workset + i;
	}

	while(1) {
		// TODO: think about more efficient use of malloc/free
		pThis->evtdata.poll.currfds = 0;
		pThis->evtdata.poll.maxfds = FDSET_INCREMENT;
		/* we need to alloc one pollfd more, because the list must be 0-terminated! */
		CHKmalloc(pThis->evtdata.poll.fds = calloc(FDSET_INCREMENT + 1, sizeof(struct pollfd)));

		/* Add the TCP listen sockets to the list of read descriptors. */
		for(i = 0 ; i < pThis->iLstnCurr ; ++i) {
			CHKiRet(select_Add(pThis, pThis->ppLstn[i], NSDSEL_RD));
		}

		/* do the sessions */
		iTCPSess = TCPSessGetNxtSess(pThis, -1);
		while(iTCPSess != -1) {
			/* TODO: access to pNsd is NOT really CLEAN, use method... */
			CHKiRet(select_Add(pThis, pThis->pSessions[iTCPSess]->pStrm, NSDSEL_RD));
			DBGPRINTF("tcpsrv process session %d:\n", iTCPSess);

			/* now get next... */
			iTCPSess = TCPSessGetNxtSess(pThis, iTCPSess);
		}

		/* wait for io to become ready */
		CHKiRet(select_Poll(pThis, &nfds));
		if(glbl.GetGlobalInputTermState() == 1)
			break; /* terminate input! */

		iWorkset = 0;
		for(i = 0 ; i < pThis->iLstnCurr ; ++i) {
			if(glbl.GetGlobalInputTermState() == 1)
				ABORT_FINALIZE(RS_RET_FORCE_TERM);
			CHKiRet(select_IsReady(pThis, pThis->ppLstn[i], NSDSEL_RD, &bIsReady));
			if(bIsReady) {
				workset[iWorkset].pSrv = pThis;
				workset[iWorkset].id = i;
				workset[iWorkset].ptrType = NSD_PTR_TYPE_LSTN;
				workset[iWorkset].id = i;
				workset[iWorkset].isInError = 0;
				CHKiRet(netstrm.GetSock(pThis->ppLstn[i], &(workset[iWorkset].sock)));
				workset[iWorkset].ptr.ppLstn = pThis->ppLstn;
				/* this is a flag to indicate listen sock */
				++iWorkset;
				if(iWorkset >= (int) sizeWorkset) {
					processWorkset(iWorkset, pWorkset);
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
			localRet = select_IsReady(pThis, pThis->pSessions[iTCPSess]->pStrm, NSDSEL_RD, &bIsReady);
			if(bIsReady || localRet != RS_RET_OK) {
				workset[iWorkset].pSrv = pThis;
				workset[iWorkset].ptrType = NSD_PTR_TYPE_SESS;
				workset[iWorkset].id = iTCPSess;
				workset[iWorkset].isInError = 0;
				CHKiRet(netstrm.GetSock(pThis->pSessions[iTCPSess]->pStrm, &(workset[iWorkset].sock)));
				workset[iWorkset].ptr.pSess = pThis->pSessions[iTCPSess];
				++iWorkset;
				if(iWorkset >= (int) sizeWorkset) {
					processWorkset(iWorkset, pWorkset);
					iWorkset = 0;
				}
				if(bIsReady)
					--nfds; /* indicate we have processed one */
			}
			iTCPSess = TCPSessGetNxtSess(pThis, iTCPSess);
		}

		if(iWorkset > 0)
			processWorkset(iWorkset, pWorkset);

		/* we need to copy back close descriptors */
finalize_it: /* this is a very special case - this time only we do not exit the function,
	      * because that would not help us either. So we simply retry it. Let's see
	      * if that actually is a better idea. Exiting the loop wasn't we always
	      * crashed, which made sense (the rest of the engine was not prepared for
	      * that) -- rgerhards, 2008-05-19
	      */
		free(pThis->evtdata.poll.fds);
		continue; /* keep compiler happy, block end after label is non-standard */
	}


	RETiRet;
}
PRAGMA_DIAGNOSTIC_POP
#endif /* conditional exclude when epoll is available */


#if defined(HAVE_EPOLL_CREATE)
static rsRetVal
RunEpoll(tcpsrv_t *const pThis)
{
	DEFiRet;
	int i;
	tcpsrv_io_descr_t *workset[NSPOLL_MAX_EVENTS_PER_WAIT];
	int numEntries;
	rsRetVal localRet;

	DBGPRINTF("tcpsrv uses epoll() interface\n");

	/* flag that we are in epoll mode */ // TODO: check if we still need this (and why!)
	pThis->bUsingEPoll = RSTRUE;

	/* Add the TCP listen sockets to the list of sockets to monitor */
	for(i = 0 ; i < pThis->iLstnCurr ; ++i) {
		DBGPRINTF("Trying to add listener %d, pUsr=%p\n", i, pThis->ppLstn);
		CHKmalloc(pThis->ppioDescrPtr[i] = (tcpsrv_io_descr_t*) calloc(1, sizeof(tcpsrv_io_descr_t)));
		pThis->ppioDescrPtr[i]->pSrv = pThis; // TODO: really needed?
		pThis->ppioDescrPtr[i]->id = i; // TODO: remove if session handling is refactored to dyn max sessions
		pThis->ppioDescrPtr[i]->isInError = 0;
		INIT_ATOMIC_HELPER_MUT(pThis->ppioDescrPtr[i]->isInError);
		CHKiRet(netstrm.GetSock(pThis->ppLstn[i], &(pThis->ppioDescrPtr[i]->sock)));
		pThis->ppioDescrPtr[i]->ptrType = NSD_PTR_TYPE_LSTN;
		pThis->ppioDescrPtr[i]->ptr.ppLstn = pThis->ppLstn;
		CHKiRet(epoll_Ctl(pThis, pThis->ppioDescrPtr[i], 1, EPOLL_CTL_ADD));
		DBGPRINTF("Added listener %d\n", i);
	}

	while(glbl.GetGlobalInputTermState() == 0) {
		numEntries = sizeof(workset)/sizeof(tcpsrv_io_descr_t *);
		localRet = epoll_Wait(pThis, -1, &numEntries, workset);
		if(glbl.GetGlobalInputTermState() == 1)
			break; /* terminate input! */

		/* check if we need to ignore the i/o ready state. We do this if we got an invalid
		 * return state. Validly, this can happen for RS_RET_EINTR, for other cases it may
		 * not be the right thing, but what is the right thing is really hard at this point...
		 */
		if(localRet != RS_RET_OK)
			continue;

		processWorkset(numEntries, workset);
	}

	/* remove the tcp listen sockets from the epoll set */
	for(i = 0 ; i < pThis->iLstnCurr ; ++i) {
		CHKiRet(epoll_Ctl(pThis, pThis->ppioDescrPtr[i], 1, EPOLL_CTL_DEL));
		DESTROY_ATOMIC_HELPER_MUT(pThis->ppioDescrPtr[i]->mut_isInError);
		free(pThis->ppioDescrPtr[i]);
	}

finalize_it:
	RETiRet;
}
#endif


/* This function is called to gather input. It tries doing that via the epoll()
 * interface. If the driver does not support that, it falls back to calling its
 * select() equivalent.
 * rgerhards, 2009-11-18
 */
static rsRetVal
Run(tcpsrv_t *const pThis)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, tcpsrv);

	if(pThis->iLstnCurr == 0) {
		dbgprintf("tcpsrv: no listeneres at all (probably init error), terminating\n");
		RETiRet; /* somewhat "dirty" exit to avoid issue with cancel handler */
	}

	eventNotify_init(pThis);
	if(pThis->workQueue.numWrkr > 1) {
		startWrkrPool(pThis);
	}
	#if defined(HAVE_EPOLL_CREATE)
		iRet = RunEpoll(pThis);
	#else
		/* fall back to select */
		iRet = RunSelect(pThis);
	#endif

	if(pThis->workQueue.numWrkr > 1) {
		stopWrkrPool(pThis);
	}
	eventNotify_exit(pThis);

//finalize_it:
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


static rsRetVal
SetNumWrkr(tcpsrv_t *pThis, const int numWrkr)
{
	pThis->workQueue.numWrkr = numWrkr;
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
	pIf->SetNumWrkr = SetNumWrkr;

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
