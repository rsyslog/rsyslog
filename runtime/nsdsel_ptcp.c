/* nsdsel_ptcp.c
 *
 * An implementation of the nsd select() interface for plain tcp sockets.
 * 
 * Copyright 2008 Rainer Gerhards and Adiscon GmbH.
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

#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/select.h>

#include "rsyslog.h"
#include "module-template.h"
#include "obj.h"
#include "errmsg.h"
#include "nsd_ptcp.h"
#include "nsdsel_ptcp.h"
#include "unlimited_select.h"

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(errmsg)
DEFobjCurrIf(glbl)


/* Standard-Constructor
 */
BEGINobjConstruct(nsdsel_ptcp) /* be sure to specify the object type also in END macro! */
	pThis->maxfds = 0;
#ifdef USE_UNLIMITED_SELECT
        pThis->pReadfds = calloc(1, glbl.GetFdSetSize());
        pThis->pWritefds = calloc(1, glbl.GetFdSetSize());
#else
	FD_ZERO(&pThis->readfds);
	FD_ZERO(&pThis->writefds);
#endif
ENDobjConstruct(nsdsel_ptcp)


/* destructor for the nsdsel_ptcp object */
BEGINobjDestruct(nsdsel_ptcp) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(nsdsel_ptcp)
#ifdef USE_UNLIMITED_SELECT
	freeFdSet(pThis->pReadfds);
	freeFdSet(pThis->pWritefds);
#endif
ENDobjDestruct(nsdsel_ptcp)


/* Add a socket to the select set */
static rsRetVal
Add(nsdsel_t *pNsdsel, nsd_t *pNsd, nsdsel_waitOp_t waitOp)
{
	DEFiRet;
	nsdsel_ptcp_t *pThis = (nsdsel_ptcp_t*) pNsdsel;
	nsd_ptcp_t *pSock = (nsd_ptcp_t*) pNsd;
#ifdef USE_UNLIMITED_SELECT
        fd_set *pReadfds = pThis->pReadfds;
        fd_set *pWritefds = pThis->pWritefds;
#else
        fd_set *pReadfds = &pThis->readfds;
        fd_set *pWritefds = &pThis->writefds;
#endif

	ISOBJ_TYPE_assert(pSock, nsd_ptcp);
	ISOBJ_TYPE_assert(pThis, nsdsel_ptcp);

	switch(waitOp) {
		case NSDSEL_RD:
			FD_SET(pSock->sock, pReadfds);
			break;
		case NSDSEL_WR:
			FD_SET(pSock->sock, pWritefds);
			break;
		case NSDSEL_RDWR:
			FD_SET(pSock->sock, pReadfds);
			FD_SET(pSock->sock, pWritefds);
			break;
	}

	if(pSock->sock > pThis->maxfds)
		pThis->maxfds = pSock->sock;

	RETiRet;
}


/* perform the select()  piNumReady returns how many descriptors are ready for IO 
 * TODO: add timeout!
 */
static rsRetVal
Select(nsdsel_t *pNsdsel, int *piNumReady)
{
	DEFiRet;
	int i;
	nsdsel_ptcp_t *pThis = (nsdsel_ptcp_t*) pNsdsel;
#ifdef USE_UNLIMITED_SELECT
        fd_set *pReadfds = pThis->pReadfds;
        fd_set *pWritefds = pThis->pWritefds;
#else
        fd_set *pReadfds = &pThis->readfds;
        fd_set *pWritefds = &pThis->writefds;
#endif

	ISOBJ_TYPE_assert(pThis, nsdsel_ptcp);
	assert(piNumReady != NULL);

	if(Debug) { // TODO: debug setting!
		// TODO: name in dbgprintf!
		dbgprintf("--------<NSDSEL_PTCP> calling select, active fds (max %d): ", pThis->maxfds);
		for(i = 0; i <= pThis->maxfds; ++i)
			if(FD_ISSET(i, pReadfds) || FD_ISSET(i, pWritefds))
				dbgprintf("%d ", i);
		dbgprintf("\n");
	}

	/* now do the select */
	*piNumReady = select(pThis->maxfds+1, pReadfds, pWritefds, NULL, NULL);

	RETiRet;
}


/* check if a socket is ready for IO */
static rsRetVal
IsReady(nsdsel_t *pNsdsel, nsd_t *pNsd, nsdsel_waitOp_t waitOp, int *pbIsReady)
{
	DEFiRet;
	nsdsel_ptcp_t *pThis = (nsdsel_ptcp_t*) pNsdsel;
	nsd_ptcp_t *pSock = (nsd_ptcp_t*) pNsd;
#ifdef USE_UNLIMITED_SELECT
        fd_set *pReadfds = pThis->pReadfds;
        fd_set *pWritefds = pThis->pWritefds;
#else
        fd_set *pReadfds = &pThis->readfds;
        fd_set *pWritefds = &pThis->writefds;
#endif

	ISOBJ_TYPE_assert(pThis, nsdsel_ptcp);
	ISOBJ_TYPE_assert(pSock, nsd_ptcp);
	assert(pbIsReady != NULL);

	switch(waitOp) {
		case NSDSEL_RD:
			*pbIsReady = FD_ISSET(pSock->sock, pReadfds);
			break;
		case NSDSEL_WR:
			*pbIsReady = FD_ISSET(pSock->sock, pWritefds);
			break;
		case NSDSEL_RDWR:
			*pbIsReady =   FD_ISSET(pSock->sock, pReadfds)
				     | FD_ISSET(pSock->sock, pWritefds);
			break;
	}

	RETiRet;
}


/* ------------------------------ end support for the select() interface ------------------------------ */


/* queryInterface function */
BEGINobjQueryInterface(nsdsel_ptcp)
CODESTARTobjQueryInterface(nsdsel_ptcp)
	if(pIf->ifVersion != nsdCURR_IF_VERSION) {/* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->Construct = (rsRetVal(*)(nsdsel_t**)) nsdsel_ptcpConstruct;
	pIf->Destruct = (rsRetVal(*)(nsdsel_t**)) nsdsel_ptcpDestruct;
	pIf->Add = Add;
	pIf->Select = Select;
	pIf->IsReady = IsReady;
finalize_it:
ENDobjQueryInterface(nsdsel_ptcp)


/* exit our class
 */
BEGINObjClassExit(nsdsel_ptcp, OBJ_IS_CORE_MODULE) /* CHANGE class also in END MACRO! */
CODESTARTObjClassExit(nsdsel_ptcp)
	/* release objects we no longer need */
	objRelease(glbl, CORE_COMPONENT);
	objRelease(errmsg, CORE_COMPONENT);
ENDObjClassExit(nsdsel_ptcp)


/* Initialize the nsdsel_ptcp class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINObjClassInit(nsdsel_ptcp, 1, OBJ_IS_CORE_MODULE) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(glbl, CORE_COMPONENT));

	/* set our own handlers */
ENDObjClassInit(nsdsel_ptcp)
/* vi:set ai:
 */
