/* netstrmstrm.c
 * 
 * This class implements a generic netstrmwork stream class. It supports
 * sending and receiving data streams over a netstrmwork. The class abstracts
 * the transport, though it is a safe assumption that TCP is being used.
 * The class has a number of properties, among which are also ones to
 * select privacy settings, eg by enabling TLS and/or GSSAPI. In the 
 * long run, this class shall provide all stream-oriented netstrmwork
 * functionality inside rsyslog.
 *
 * It is a high-level class, which uses a number of helper objects
 * to carry out its work (including, and most importantly, transport
 * drivers).
 *
 * Work on this module begun 2008-04-17 by Rainer Gerhards. This code
 * borrows from librelp's tcp.c/.h code. librelp is dual licensed and
 * Rainer Gerhards and Adiscon GmbH have agreed to permit using the code
 * under the terms of the GNU Lesser General Public License.
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

#include "syslogd-types.h"
#include "module-template.h"
#include "parse.h"
#include "srUtils.h"
#include "obj.h"
#include "errmsg.h"
#include "net.h"
#include "nsd.h"
#include "netstrm.h"

MODULE_TYPE_LIB

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(errmsg)
DEFobjCurrIf(glbl)
DEFobjCurrIf(net)


/* load our low-level driver. This must be done before any
 * driver-specific functions (allmost all...) can be carried
 * out. Note that the driver's .ifIsLoaded is correctly
 * initialized by calloc() and we depend on that.
 * rgerhards, 2008-04-18
 */
static rsRetVal
loadDrvr(netstrm_t *pThis)
{
	uchar *pDrvrName;
	DEFiRet;

	if(pThis->Drvr.ifIsLoaded == 0) {
		pDrvrName = pThis->pDrvrName;
		if(pDrvrName == NULL) { /* if no drvr name is set, use system default */
			pDrvrName = glbl.GetDfltNetstrmDrvr();
			pThis->pDrvrName = (uchar*)strdup((char*)pDrvrName); // TODO: use set method once it exists
		}

		pThis->Drvr.ifVersion = nsdCURR_IF_VERSION;
		/* The pDrvrName+2 below is a hack to obtain the object name. It 
		 * safes us to have yet another variable with the name without "lm" in
		 * front of it. If we change the module load interface, we may re-think
		 * about this hack, but for the time being it is efficient and clean
		 * enough. -- rgerhards, 2008-04-18
		 */
		CHKiRet(obj.UseObj(__FILE__, pDrvrName+2, pDrvrName, (void*) &pThis->Drvr));
	}
finalize_it:
	RETiRet;
}


/* Standard-Constructor */
BEGINobjConstruct(netstrm) /* be sure to specify the object type also in END macro! */
ENDobjConstruct(netstrm)


/* destructor for the netstrm object */
BEGINobjDestruct(netstrm) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(netstrm)
	if(pThis->pDrvrData != NULL)
		iRet = pThis->Drvr.Destruct(&pThis->pDrvrData);
	
	/* driver can only be released after all data has been destructed */
	if(pThis->Drvr.ifIsLoaded == 1) {
		obj.ReleaseObj(__FILE__, pThis->pDrvrName+2, pThis->pDrvrName, (void*) &pThis->Drvr);
	}
	if(pThis->pDrvrName != NULL)
		free(pThis->pDrvrName);
ENDobjDestruct(netstrm)


/* ConstructionFinalizer */
static rsRetVal
netstrmConstructFinalize(netstrm_t *pThis)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, netstrm);
	CHKiRet(loadDrvr(pThis));
	CHKiRet(pThis->Drvr.Construct(&pThis->pDrvrData));
finalize_it:
	RETiRet;
}

/* abort a connection. This is much like Destruct(), but tries
 * to discard any unsent data. -- rgerhards, 2008-03-24
 */
static rsRetVal
AbortDestruct(netstrm_t **ppThis)
{
	DEFiRet;
	assert(ppThis != NULL);
	ISOBJ_TYPE_assert((*ppThis), netstrm);

	/* we do NOT exit on error, because that would make things worse */
	(*ppThis)->Drvr.Abort((*ppThis)->pDrvrData);
	iRet = netstrmDestruct(ppThis);

	RETiRet;
}


#if 0
This is not yet working - wait until we arrive at the receiver side (distracts too much at the moment)

/* accept an incoming connection request, pNsdLstn provides the "listen socket" on which we can
 * accept the new session.
 * rgerhards, 2008-03-17
 */
static rsRetVal
AcceptConnReq(netstrm_t **ppThis, nsd_t *pNsdLstn)
{
	netstrm_t *pThis = NULL;
	nsd_t *pNsd;
	DEFiRet;

	assert(ppThis != NULL);

	/* construct our object so that we can use it... */
	CHKiRet(netstrmConstruct(&pThis));

	/* TODO: obtain hostname, normalize (callback?), save it */
	CHKiRet(FillRemHost(pThis, (struct sockaddr*) &addr));

	/* set the new socket to non-blocking IO */
	if((sockflags = fcntl(iNewSock, F_GETFL)) != -1) {
		sockflags |= O_NONBLOCK;
		/* SETFL could fail too, so get it caught by the subsequent
		 * error check.
		 */
		sockflags = fcntl(iNewSock, F_SETFL, sockflags);
	}
	if(sockflags == -1) {
		dbgprintf("error %d setting fcntl(O_NONBLOCK) on tcp socket %d", errno, iNewSock);
		ABORT_FINALIZE(RS_RET_IO_ERROR);
	}

	pThis->sock = iNewSock;

	*ppThis = pThis;

finalize_it:
	if(iRet != RS_RET_OK) {
		if(pThis != NULL)
			netstrmDestruct(&pThis);
		/* the close may be redundant, but that doesn't hurt... */
		if(iNewSock >= 0)
			close(iNewSock);
	}

	RETiRet;
}
#endif


/* initialize the tcp socket for a listner
 * pLstnPort must point to a port name or number. NULL is NOT permitted
 * (hint: we need to be careful when we use this module together with librelp,
 * there NULL indicates the default port
 * default is used.
 * gerhards, 2008-03-17
 */
static rsRetVal
LstnInit(netstrm_t *pThis, uchar *pLstnPort)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, netstrm);
	assert(pLstnPort != NULL);
	CHKiRet(pThis->Drvr.LstnInit(pThis->pDrvrData, pLstnPort));

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
Rcv(netstrm_t *pThis, uchar *pBuf, ssize_t *pLenBuf)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, netstrm);
	iRet = pThis->Drvr.Rcv(pThis->pDrvrData, pBuf, pLenBuf);
	RETiRet;
}


/* send a buffer. On entry, pLenBuf contains the number of octets to
 * write. On exit, it contains the number of octets actually written.
 * If this number is lower than on entry, only a partial buffer has
 * been written.
 * rgerhards, 2008-03-19
 */
static rsRetVal
Send(netstrm_t *pThis, uchar *pBuf, ssize_t *pLenBuf)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, netstrm);
	iRet = pThis->Drvr.Send(pThis->pDrvrData, pBuf, pLenBuf);
	RETiRet;
}


/* open a connection to a remote host (server).
 * rgerhards, 2008-03-19
 */
static rsRetVal
Connect(netstrm_t *pThis, int family, uchar *port, uchar *host)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, netstrm);
	assert(port != NULL);
	assert(host != NULL);
	iRet = pThis->Drvr.Connect(pThis->pDrvrData, family, port, host);
	RETiRet;
}


/* queryInterface function
 */
BEGINobjQueryInterface(netstrm)
CODESTARTobjQueryInterface(netstrm)
	if(pIf->ifVersion != netstrmCURR_IF_VERSION) {/* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->Construct = netstrmConstruct;
	pIf->ConstructFinalize = netstrmConstructFinalize;
	pIf->Destruct = netstrmDestruct;
	pIf->AbortDestruct = AbortDestruct;
	pIf->LstnInit = LstnInit;
	// TODO: add later: pIf->AcceptConnReq = AcceptConnReq;
	pIf->Rcv = Rcv;
	pIf->Send = Send;
	pIf->Connect = Connect;
finalize_it:
ENDobjQueryInterface(netstrm)


/* exit our class
 */
BEGINObjClassExit(netstrm, OBJ_IS_LOADABLE_MODULE) /* CHANGE class also in END MACRO! */
CODESTARTObjClassExit(netstrm)
	/* release objects we no longer need */
	objRelease(net, CORE_COMPONENT);
	objRelease(glbl, CORE_COMPONENT);
	objRelease(errmsg, CORE_COMPONENT);
ENDObjClassExit(netstrm)


/* Initialize the netstrm class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINAbstractObjClassInit(netstrm, 1, OBJ_IS_CORE_MODULE) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(net, CORE_COMPONENT));

	/* set our own handlers */
ENDObjClassInit(netstrm)


/* --------------- here now comes the plumbing that makes as a library module --------------- */


BEGINmodExit
CODESTARTmodExit
	netstrmClassExit();
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_LIB_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */

	/* Initialize all classes that are in our module - this includes ourselfs */
	CHKiRet(netstrmClassInit(pModInfo)); /* must be done after tcps_sess, as we use it */
ENDmodInit
/* vi:set ai:
 */
