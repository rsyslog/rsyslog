/* nssel.c
 *
 * The io waiter is a helper object enabling us to wait on a set of streams to become
 * ready for IO - this is modelled after select(). We need this, because
 * stream drivers may have different concepts. Consequently,
 * the structure must contain nsd_t's from the same stream driver type
 * only. This is implemented as a singly-linked list where every
 * new element is added at the top of the list.
 * 
 * Work on this module begun 2008-04-22 by Rainer Gerhards.
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

#include "rsyslog.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#include "rsyslog.h"
#include "obj.h"
#include "module-template.h"
#include "netstrm.h"
#include "nssel.h"

MODULE_TYPE_LIB

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(glbl)


/* load our low-level driver. This must be done before any
 * driver-specific functions (allmost all...) can be carried
 * out. Note that the driver's .ifIsLoaded is correctly
 * initialized by calloc() and we depend on that.
 * rgerhards, 2008-04-18
 */
static rsRetVal
loadDrvr(nssel_t *pThis)
{
	uchar *pDrvrName;
	DEFiRet;

	pDrvrName = pThis->pDrvrName;
	if(pDrvrName == NULL) /* if no drvr name is set, use system default */
		pDrvrName = glbl.GetDfltNetstrmDrvr();

	pThis->Drvr.ifVersion = nsdCURR_IF_VERSION;
	/* The pDrvrName+2 below is a hack to obtain the object name. It 
	 * safes us to have yet another variable with the name without "lm" in
	 * front of it. If we change the module load interface, we may re-think
	 * about this hack, but for the time being it is efficient and clean
	 * enough. -- rgerhards, 2008-04-18
	 */
	//CHKiRet(obj.UseObj(__FILE__, pDrvrName+2, pDrvrName, (void*) &pThis->Drvr));
	CHKiRet(obj.UseObj(__FILE__, "nsdsel_ptcp", "lmnsdsel_ptcp", (void*) &pThis->Drvr));
finalize_it:
	RETiRet;
}


/* Standard-Constructor */
BEGINobjConstruct(nssel) /* be sure to specify the object type also in END macro! */
ENDobjConstruct(nssel)


/* destructor for the nssel object */
BEGINobjDestruct(nssel) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(nssel)
	if(pThis->pDrvrData != NULL)
		pThis->Drvr.Destruct(&pThis->pDrvrData);
ENDobjDestruct(nssel)


/* ConstructionFinalizer */
static rsRetVal
ConstructFinalize(nssel_t *pThis)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, nssel);
	CHKiRet(loadDrvr(pThis));
	CHKiRet(pThis->Drvr.Construct(&pThis->pDrvrData));
finalize_it:
	RETiRet;
}


/* Add a stream object to the current select() set.
 * Note that a single stream may have multiple "sockets" if
 * it is a listener. If so, all of them are begin added.
 */
static rsRetVal
Add(nssel_t *pThis, netstrm_t *pStrm, nsdsel_waitOp_t waitOp)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, nssel);
	ISOBJ_TYPE_assert(pStrm, netstrm);
	
	CHKiRet(pThis->Drvr.Add(pThis->pDrvrData, pStrm->pDrvrData, waitOp));

finalize_it:
	RETiRet;
}


/* wait for IO to happen on one of our netstreams. iNumReady has
 * the number of ready "sockets" after the call. This function blocks
 * until some are ready. EAGAIN is retried.
 */
static rsRetVal
Wait(nssel_t *pThis, int *piNumReady)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, nssel);
	assert(piNumReady != NULL);
	iRet = pThis->Drvr.Select(pThis->pDrvrData, piNumReady);
	RETiRet;
}


/* Check if a stream is ready for IO. *piNumReady contains the remaining number
 * of ready streams. Note that this function may say the stream is not ready
 * but still decrement *piNumReady. This can happen when (e.g. with TLS) the low
 * level driver requires some IO which is hidden from the upper layer point of view.
 * rgerhards, 2008-04-23
 */
static rsRetVal
IsReady(nssel_t *pThis, netstrm_t *pStrm, nsdsel_waitOp_t waitOp, int *pbIsReady, int *piNumReady)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, nssel);
	ISOBJ_TYPE_assert(pStrm, netstrm);
	assert(pbIsReady != NULL);
	assert(piNumReady != NULL);
	iRet = pThis->Drvr.IsReady(pThis->pDrvrData, pStrm->pDrvrData, waitOp, pbIsReady);
	RETiRet;
}


/* queryInterface function */
BEGINobjQueryInterface(nssel)
CODESTARTobjQueryInterface(nssel)
	if(pIf->ifVersion != nsselCURR_IF_VERSION) {/* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->Construct = nsselConstruct;
	pIf->ConstructFinalize = ConstructFinalize;
	pIf->Destruct = nsselDestruct;
	pIf->Add = Add;
	pIf->Wait = Wait;
	pIf->IsReady = IsReady;
finalize_it:
ENDobjQueryInterface(nssel)


/* exit our class
 */
BEGINObjClassExit(nssel, OBJ_IS_LOADABLE_MODULE) /* CHANGE class also in END MACRO! */
CODESTARTObjClassExit(nssel)
	/* release objects we no longer need */
	objRelease(glbl, CORE_COMPONENT);
ENDObjClassExit(nssel)


/* Initialize the nssel class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINObjClassInit(nssel, 1, OBJ_IS_CORE_MODULE) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(glbl, CORE_COMPONENT));

	/* set our own handlers */
ENDObjClassInit(nssel)


/* --------------- here now comes the plumbing that makes us a library module --------------- */


BEGINmodExit
CODESTARTmodExit
	nsselClassExit();
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_LIB_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */

	/* Initialize all classes that are in our module - this includes ourselfs */
	CHKiRet(nsselClassInit(pModInfo)); /* must be done after tcps_sess, as we use it */
ENDmodInit
/* vi:set ai:
 */
