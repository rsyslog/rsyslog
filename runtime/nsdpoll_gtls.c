/* nsdpoll_gtls.c
 *
 * An implementation of the nsd epoll() interface for plain tcp sockets.
 *
 * Copyright 2025 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
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
#include "nsdpoll_gtls.h"

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(glbl)



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
BEGINobjConstruct(nsdpoll_gtls) /* be sure to specify the object type also in END macro! */
#if defined(EPOLL_CLOEXEC) && defined(HAVE_EPOLL_CREATE1)
	DBGPRINTF("nsdpoll_gtls uses epoll_create1()\n");
	pThis->efd = epoll_create1(EPOLL_CLOEXEC);
	if(pThis->efd < 0 && errno == ENOSYS)
#endif
	{
		DBGPRINTF("nsdpoll_gtls uses epoll_create()\n");
		pThis->efd = epoll_create(100); /* size is ignored in newer kernels, but 100 is not bad... */
	}

	if(pThis->efd < 0) {
		DBGPRINTF("epoll_create1() could not create fd\n");
		ABORT_FINALIZE(RS_RET_IO_ERROR);
	}
finalize_it:
ENDobjConstruct(nsdpoll_gtls)


/* destructor for the nsdpoll_gtls object */
BEGINobjDestruct(nsdpoll_gtls) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(nsdpoll_gtls)
ENDobjDestruct(nsdpoll_gtls)


/* Modify socket set */
static rsRetVal
Ctl(nsdpoll_t *const pNsdpoll, tcpsrv_io_descr_t *const pioDescr, const int mode, const int op)
{
	nsdpoll_gtls_t *const pThis = (nsdpoll_gtls_t*) pNsdpoll;
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
	nsdpoll_gtls_t *pThis = (nsdpoll_gtls_t*) pNsdpoll;
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
BEGINobjQueryInterface(nsdpoll_gtls)
CODESTARTobjQueryInterface(nsdpoll_gtls)
	if(pIf->ifVersion != nsdCURR_IF_VERSION) {/* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->Construct = (rsRetVal(*)(nsdpoll_t**)) nsdpoll_gtlsConstruct;
	pIf->Destruct = (rsRetVal(*)(nsdpoll_t**)) nsdpoll_gtlsDestruct;
	pIf->Ctl = Ctl;
	pIf->Wait = Wait;
finalize_it:
ENDobjQueryInterface(nsdpoll_gtls)


BEGINObjClassExit(nsdpoll_gtls, OBJ_IS_CORE_MODULE) /* CHANGE class also in END MACRO! */
CODESTARTObjClassExit(nsdpoll_gtls)
	/* release objects we no longer need */
	objRelease(glbl, CORE_COMPONENT);
ENDObjClassExit(nsdpoll_gtls)


BEGINObjClassInit(nsdpoll_gtls, 1, OBJ_IS_CORE_MODULE) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(glbl, CORE_COMPONENT));
ENDObjClassInit(nsdpoll_gtls)
#else

#endif /* #ifdef HAVE_EPOLL_CREATE this module requires epoll! */
