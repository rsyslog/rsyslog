/* nsd_gtls.c
 *
 * An implementation of the nsd interface for GnuTLS.
 * 
 * Copyright 2007, 2008 Rainer Gerhards and Adiscon GmbH.
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

#include "rsyslog.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <netdb.h>
#include <fnmatch.h>
#include <fcntl.h>
#include <unistd.h>

#include "rsyslog.h"
#include "syslogd-types.h"
#include "module-template.h"
#include "parse.h"
#include "srUtils.h"
#include "obj.h"
#include "errmsg.h"
#include "nsd_ptcp.h"
#include "nsd_gtls.h"

MODULE_TYPE_LIB

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(errmsg)
DEFobjCurrIf(glbl)
DEFobjCurrIf(nsd_ptcp)


/* Standard-Constructor */
BEGINobjConstruct(nsd_gtls) /* be sure to specify the object type also in END macro! */
	iRet = nsd_ptcp.Construct(&pThis->pTcp);
ENDobjConstruct(nsd_gtls)


/* destructor for the nsd_gtls object */
BEGINobjDestruct(nsd_gtls) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(nsd_gtls)
	if(pThis->pTcp != NULL)
		nsd_ptcp.Destruct(&pThis->pTcp);
ENDobjDestruct(nsd_gtls)


/* abort a connection. This is meant to be called immediately
 * before the Destruct call. -- rgerhards, 2008-03-24
 */
static rsRetVal
Abort(nsd_t *pNsd)
{
	nsd_gtls_t *pThis = (nsd_gtls_t*) pNsd;
	DEFiRet;

	ISOBJ_TYPE_assert((pThis), nsd_gtls);

	if(pThis->iMode == 0) {
		nsd_ptcp.Abort(pThis->pTcp);
	}

	RETiRet;
}



/* initialize the tcp socket for a listner
 * pLstnPort must point to a port name or number. NULL is NOT permitted
 * (hint: we need to be careful when we use this module together with librelp,
 * there NULL indicates the default port
 * default is used.
 * gerhards, 2008-03-17
 */
static rsRetVal
LstnInit(nsd_t *pNsd, uchar *pLstnPort)
{
	nsd_gtls_t *pThis = (nsd_gtls_t*) pNsd;
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, nsd_gtls);
	assert(pLstnPort != NULL);

	if(pThis->iMode == 0) {
		CHKiRet(nsd_ptcp.LstnInit(pThis->pTcp, pLstnPort));
	}

finalize_it:
	RETiRet;
}


/* receive data from a tcp socket
 * The lenBuf parameter must contain the max buffer size on entry and contains
 * the number of octets read (or -1 in case of error) on exit. This function
 * never blocks, not even when called on a blocking socket. That is important
 * for client sockets, which are set to block during send, but should not
 * block when trying to read data. If *pLenBuf is -1, an error occured and
 * errno holds the exact error cause.
 * rgerhards, 2008-03-17
 */
static rsRetVal
Rcv(nsd_t *pNsd, uchar *pBuf, ssize_t *pLenBuf)
{
	DEFiRet;
	nsd_gtls_t *pThis = (nsd_gtls_t*) pNsd;
	ISOBJ_TYPE_assert(pThis, nsd_gtls);

	if(pThis->iMode == 0) {
		CHKiRet(nsd_ptcp.Rcv(pThis->pTcp, pBuf, pLenBuf));
	}

finalize_it:
	RETiRet;
}


/* send a buffer. On entry, pLenBuf contains the number of octets to
 * write. On exit, it contains the number of octets actually written.
 * If this number is lower than on entry, only a partial buffer has
 * been written.
 * rgerhards, 2008-03-19
 */
static rsRetVal
Send(nsd_t *pNsd, uchar *pBuf, ssize_t *pLenBuf)
{
	nsd_gtls_t *pThis = (nsd_gtls_t*) pNsd;
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, nsd_gtls);

	if(pThis->iMode == 0) {
		CHKiRet(nsd_ptcp.Send(pThis->pTcp, pBuf, pLenBuf));
	}
finalize_it:
	RETiRet;
}


/* open a connection to a remote host (server).
 * rgerhards, 2008-03-19
 */
static rsRetVal
Connect(nsd_t *pNsd, int family, uchar *port, uchar *host)
{
	nsd_gtls_t *pThis = (nsd_gtls_t*) pNsd;
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, nsd_gtls);
	assert(port != NULL);
	assert(host != NULL);
	if(pThis->iMode == 0) {
		CHKiRet(nsd_ptcp.Connect(pThis->pTcp, family, port, host));
	}


finalize_it:
	RETiRet;
}


/* queryInterface function */
BEGINobjQueryInterface(nsd_gtls)
CODESTARTobjQueryInterface(nsd_gtls)
	if(pIf->ifVersion != nsdCURR_IF_VERSION) {/* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->Construct = (rsRetVal(*)(nsd_t**)) nsd_gtlsConstruct;
	pIf->Destruct = (rsRetVal(*)(nsd_t**)) nsd_gtlsDestruct;
	pIf->Abort = Abort;
	pIf->LstnInit = LstnInit;
	//pIf->AcceptConnReq = AcceptConnReq;
	pIf->Rcv = Rcv;
	pIf->Send = Send;
	pIf->Connect = Connect;
finalize_it:
ENDobjQueryInterface(nsd_gtls)


/* exit our class
 */
BEGINObjClassExit(nsd_gtls, OBJ_IS_LOADABLE_MODULE) /* CHANGE class also in END MACRO! */
CODESTARTObjClassExit(nsd_gtls)
	/* release objects we no longer need */
	objRelease(nsd_ptcp, LM_NSD_PTCP_FILENAME);
	objRelease(glbl, CORE_COMPONENT);
	objRelease(errmsg, CORE_COMPONENT);
ENDObjClassExit(nsd_gtls)


/* Initialize the nsd_gtls class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINObjClassInit(nsd_gtls, 1, OBJ_IS_LOADABLE_MODULE) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(nsd_ptcp, LM_NSD_PTCP_FILENAME));

	/* set our own handlers */
ENDObjClassInit(nsd_gtls)


/* --------------- here now comes the plumbing that makes as a library module --------------- */


BEGINmodExit
CODESTARTmodExit
	nsd_gtlsClassExit();
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_LIB_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */

	/* Initialize all classes that are in our module - this includes ourselfs */
	CHKiRet(nsd_gtlsClassInit(pModInfo)); /* must be done after tcps_sess, as we use it */
ENDmodInit
/* vi:set ai:
 */
