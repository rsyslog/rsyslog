/* nsdpoll_ptcp.c
 *
 * An implementation of the nsd epoll() interface for plain tcp sockets.
 * 
 * Copyright 2009 Rainer Gerhards and Adiscon GmbH.
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

#include "rsyslog.h"
#include "module-template.h"
#include "obj.h"
#include "errmsg.h"
#include "nsd_ptcp.h"
#include "nsdpoll_ptcp.h"
#include "unlimited_select.h"

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(errmsg)
DEFobjCurrIf(glbl)


/* Standard-Constructor
 */
BEGINobjConstruct(nsdpoll_ptcp) /* be sure to specify the object type also in END macro! */
ENDobjConstruct(nsdpoll_ptcp)


/* destructor for the nsdpoll_ptcp object */
BEGINobjDestruct(nsdpoll_ptcp) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(nsdpoll_ptcp)
ENDobjDestruct(nsdpoll_ptcp)


/* Add a socket to the select set */
static rsRetVal
Ctl(nsdpoll_t *pThis, int fd, int op, epoll_event_t *event) {
	DEFiRet;
	RETiRet;
}


/* Wait for io to become ready.
 */
static rsRetVal
Wait(nsdpoll_t *pThis, epoll_event_t *events, int maxevents, int timeout) {
	DEFiRet;
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
	objRelease(errmsg, CORE_COMPONENT);
ENDObjClassExit(nsdpoll_ptcp)


/* Initialize the nsdpoll_ptcp class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINObjClassInit(nsdpoll_ptcp, 1, OBJ_IS_CORE_MODULE) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(glbl, CORE_COMPONENT));

	/* set our own handlers */
ENDObjClassInit(nsdpoll_ptcp)
/* vi:set ai:
 */
