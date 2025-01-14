/* nsdpoll_ptcp.c
 *
 * An implementation of the nsd epoll() interface for plain tcp sockets.
 *
 * Copyright 2009-2025 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
 *
 * The rsyslog runtime library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The rsyslog runtime library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the rsyslog runtime library.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 * A copy of the LGPL can be found in the file "COPYING.LESSER" in this distribution.
 */
#include "config.h"

#ifdef HAVE_EPOLL_CREATE /* this module requires epoll! */

#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#ifdef HAVE_SYS_EPOLL_H
#	include <sys/epoll.h>
#endif

#include "rsyslog.h"
#include "module-template.h"
#include "obj.h"
#include "errmsg.h"
#include "srUtils.h"
#include "netstrm.h"
#include "nspoll.h"
#include "nsd_ptcp.h"
#include "nsdpoll_ptcp.h"

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(glbl)



// TODO: update comment
/* add new entry to list. We assume that the fd is not already present and DO NOT check this!
 * Returns newly created entry in pEvtLst.
 * Note that we currently need to use level-triggered mode, because the upper layers do not work
 * in parallel. As such, in edge-triggered mode we may not get notified, because new data comes
 * in after we have read everything that was present. To use ET mode, we need to change the upper
 * peers so that they immediately start a new wait before processing the data read. That obviously
 * requires more elaborate redesign and we postpone this until the current more simplictic mode has
 * been proven OK in practice.
 * rgerhards, 2009-11-18
 */
static rsRetVal
addEvent(struct epoll_event *const event, tcpsrv_io_descr_t *pioDescr, const int mode)
{
	DEFiRet;

	event->events = 0; /* TODO: at some time we should be able to use EPOLLET */
	if(!(mode & NSDPOLL_IN_LSTN))  {
		/* right now, we refactor only regular data sessions in edge triggered mode */
		event->events = EPOLLET; /* TODO: at some time we should be able to use EPOLLET */
	}
	if((mode & NSDPOLL_IN) || (mode & NSDPOLL_IN_LSTN))
		event->events |= EPOLLIN;
	if(mode & NSDPOLL_OUT)
		event->events |= EPOLLOUT;
	event->data.ptr = (void*) pioDescr;

	RETiRet;
}


/* Standard-Constructor
 */
BEGINobjConstruct(nsdpoll_ptcp) /* be sure to specify the object type also in END macro! */
#if defined(EPOLL_CLOEXEC) && defined(HAVE_EPOLL_CREATE1)
	DBGPRINTF("nsdpoll_ptcp uses epoll_create1()\n");
	pThis->efd = epoll_create1(EPOLL_CLOEXEC);
	if(pThis->efd < 0 && errno == ENOSYS)
#endif
	{
		DBGPRINTF("nsdpoll_ptcp uses epoll_create()\n");
		pThis->efd = epoll_create(100); /* size is ignored in newer kernels, but 100 is not bad... */
	}

	if(pThis->efd < 0) {
		DBGPRINTF("epoll_create1() could not create fd\n");
		ABORT_FINALIZE(RS_RET_IO_ERROR);
	}
finalize_it:
ENDobjConstruct(nsdpoll_ptcp)


/* destructor for the nsdpoll_ptcp object */
BEGINobjDestruct(nsdpoll_ptcp) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(nsdpoll_ptcp)
ENDobjDestruct(nsdpoll_ptcp)


/* Modify socket set */
static rsRetVal
Ctl(nsdpoll_t *const pNsdpoll, tcpsrv_io_descr_t *const pioDescr, const int mode, const int op)
{
	nsdpoll_ptcp_t *const pThis = (nsdpoll_ptcp_t*) pNsdpoll;
	struct epoll_event event;
	DEFiRet;

	const int id = pioDescr->id;
	const int sock = pioDescr->sock;
	assert(sock != 0);

	if(op == NSDPOLL_ADD) {
		dbgprintf("adding nsdpoll entry %d, sock %d\n", id, sock);
		CHKiRet(addEvent(&event, pioDescr, mode));
		if(epoll_ctl(pThis->efd, EPOLL_CTL_ADD,  sock, &event) < 0) {
			LogError(errno, RS_RET_ERR_EPOLL_CTL,
				"epoll_ctl failed on fd %d, id %d, op %d\n",
				sock, id, mode);
		}
	} else if(op == NSDPOLL_DEL) {
		dbgprintf("removing nsdpoll entry %d, sock %d\n", id, sock);
		if(epoll_ctl(pThis->efd, EPOLL_CTL_DEL, sock, NULL) < 0) {
			LogError(errno, RS_RET_ERR_EPOLL_CTL,
				"epoll_ctl failed on fd %d, id %d, op %d\n",
				sock, id, mode);
			ABORT_FINALIZE(RS_RET_ERR_EPOLL_CTL);
		}
	} else {
		dbgprintf("program error: invalid NSDPOLL_mode %d - ignoring request\n", op);
		ABORT_FINALIZE(RS_RET_ERR);
	}

finalize_it:
	DBGPRINTF("Done adding nsdpoll entry %d, iRet %d\n", id, iRet);
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
Wait(nsdpoll_t *const pNsdpoll, const int timeout, int *const numEntries, tcpsrv_io_descr_t *pWorkset[])
{
	nsdpoll_ptcp_t *pThis = (nsdpoll_ptcp_t*) pNsdpoll;
	struct epoll_event event[NSPOLL_MAX_EVENTS_PER_WAIT];
	int nfds;
	int i;
	DEFiRet;

	assert(pWorkset != NULL);

	if(*numEntries > NSPOLL_MAX_EVENTS_PER_WAIT)
		*numEntries = NSPOLL_MAX_EVENTS_PER_WAIT;
	DBGPRINTF("doing epoll_wait for max %d events\n", *numEntries);
	nfds = epoll_wait(pThis->efd, event, *numEntries, timeout);
	if(nfds == -1) {
		if(errno == EINTR) {
			ABORT_FINALIZE(RS_RET_EINTR);
		} else {
			DBGPRINTF("epoll() returned with error code %d\n", errno);
			ABORT_FINALIZE(RS_RET_ERR_EPOLL);
		}
	} else if(nfds == 0) {
		ABORT_FINALIZE(RS_RET_TIMEOUT);
	}

	/* we got valid events, so tell the caller... */
	DBGPRINTF("epoll returned %d entries\n", nfds);
	for(i = 0 ; i < nfds ; ++i) {
		pWorkset[i] = event[i].data.ptr;
		pWorkset[i]->isInError = event[i].events & EPOLLERR;
	}
	*numEntries = nfds;

finalize_it:
	RETiRet;
}


/* ------------------------------ end support for the epoll() interface ------------------------------ */


/* queryInterface function */
BEGINobjQueryInterface(nsdpoll_ptcp)
CODESTARTobjQueryInterface(nsdpoll_ptcp)
	if(pIf->ifVersion != nsdCURR_IF_VERSION) {/* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->Construct = (rsRetVal(*)(nsdpoll_t**)) nsdpoll_ptcpConstruct;
	pIf->Destruct = (rsRetVal(*)(nsdpoll_t**)) nsdpoll_ptcpDestruct;
	pIf->Ctl = Ctl;
	pIf->Wait = Wait;
finalize_it:
ENDobjQueryInterface(nsdpoll_ptcp)


/* exit our class
 */
BEGINObjClassExit(nsdpoll_ptcp, OBJ_IS_CORE_MODULE) /* CHANGE class also in END MACRO! */
CODESTARTObjClassExit(nsdpoll_ptcp)
	/* release objects we no longer need */
	objRelease(glbl, CORE_COMPONENT);
ENDObjClassExit(nsdpoll_ptcp)


/* Initialize the nsdpoll_ptcp class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINObjClassInit(nsdpoll_ptcp, 1, OBJ_IS_CORE_MODULE) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(glbl, CORE_COMPONENT));

	/* set our own handlers */
ENDObjClassInit(nsdpoll_ptcp)
#else

#ifdef __xlc__ /* Xlc require some code, even unused, in source file*/
static void dummy(void) {}
#endif

#endif /* #ifdef HAVE_EPOLL_CREATE this module requires epoll! */
