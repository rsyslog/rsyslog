/* wti.c
 *
 * This file implements the worker thread instance (wti) class.
 * 
 * File begun on 2008-01-20 by RGerhards based on functions from the 
 * previous queue object class (the wti functions have been extracted)
 *
 * There is some in-depth documentation available in doc/dev_queue.html
 * (and in the web doc set on http://www.rsyslog.com/doc). Be sure to read it
 * if you are getting aquainted to the object.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>

/// TODO: check on solaris if this is any longer needed - I don't think so - rgerhards, 2009-09-20
//#ifdef OS_SOLARIS
//#	include <sched.h>
//#endif

#include "rsyslog.h"
#include "stringbuf.h"
#include "srUtils.h"
#include "wtp.h"
#include "wti.h"
#include "obj.h"
#include "glbl.h"
#include "atomic.h"

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(glbl)

/* forward-definitions */

/* methods */

/* get the header for debug messages
 * The caller must NOT free or otherwise modify the returned string!
 */
static inline uchar *
wtiGetDbgHdr(wti_t *pThis)
{
	ISOBJ_TYPE_assert(pThis, wti);

	if(pThis->pszDbgHdr == NULL)
		return (uchar*) "wti"; /* should not normally happen */
	else
		return pThis->pszDbgHdr;
}


/* return the current worker processing state. For the sake of
 * simplicity, we do not use the iRet interface. -- rgerhards, 2009-07-17
 */
bool
wtiGetState(wti_t *pThis)
{
	return ATOMIC_FETCH_32BIT(pThis->bIsRunning);
}


/* Set this thread to "always running" state (can not be unset)
 * rgerhards, 2009-07-20
 */
rsRetVal
wtiSetAlwaysRunning(wti_t *pThis)
{
	ISOBJ_TYPE_assert(pThis, wti);
	pThis->bAlwaysRunning = TRUE;
	return RS_RET_OK;
}

/* Set status (thread is running or not), actually an property of
 * use for wtp, but we need to have it per thread instance (thus it
 * is inside wti). -- rgerhards, 2009-07-17
 */
rsRetVal
wtiSetState(wti_t *pThis, bool bNewVal)
{
	ISOBJ_TYPE_assert(pThis, wti);
	if(bNewVal)
		ATOMIC_STORE_1_TO_INT(pThis->bIsRunning);
	else
		ATOMIC_STORE_0_TO_INT(pThis->bIsRunning);
	return RS_RET_OK;
}


/* Cancel the thread. If the thread is not running. But it is save and legal to
 * call wtiCancelThrd() in such situations.
 * rgerhards, 2008-02-26
 */
rsRetVal
wtiCancelThrd(wti_t *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, wti);

	if(wtiGetState(pThis)) {
		dbgoprint((obj_t*) pThis, "canceling worker thread\n");
		pthread_cancel(pThis->thrdID);
	}

	RETiRet;
}


/* Destructor */
BEGINobjDestruct(wti) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(wti)
	/* actual destruction */
	free(pThis->batch.pElem);
	free(pThis->pszDbgHdr);
ENDobjDestruct(wti)


/* Standard-Constructor for the wti object
 */
BEGINobjConstruct(wti) /* be sure to specify the object type also in END macro! */
ENDobjConstruct(wti)


/* Construction finalizer
 * rgerhards, 2008-01-17
 */
rsRetVal
wtiConstructFinalize(wti_t *pThis)
{
	DEFiRet;
	int iDeqBatchSize;

	ISOBJ_TYPE_assert(pThis, wti);

	dbgprintf("%s: finalizing construction of worker instance data\n", wtiGetDbgHdr(pThis));

	/* initialize our thread instance descriptor (no concurrency here) */
	pThis->bIsRunning = FALSE; 

	/* we now alloc the array for user pointers. We obtain the max from the queue itself. */
	CHKiRet(pThis->pWtp->pfGetDeqBatchSize(pThis->pWtp->pUsr, &iDeqBatchSize));
	CHKmalloc(pThis->batch.pElem = calloc((size_t)iDeqBatchSize, sizeof(batch_obj_t)));

finalize_it:
	RETiRet;
}


/* cancellation cleanup handler for queueWorker ()
 * Updates admin structure and frees ressources.
 * Keep in mind that cancellation is disabled if we run into
 * the cancel cleanup handler (and have been cancelled).
 * rgerhards, 2008-01-16
 */
static void
wtiWorkerCancelCleanup(void *arg)
{
	wti_t *pThis = (wti_t*) arg;
	wtp_t *pWtp;

	BEGINfunc
	ISOBJ_TYPE_assert(pThis, wti);
	pWtp = pThis->pWtp;
	ISOBJ_TYPE_assert(pWtp, wtp);

	DBGPRINTF("%s: cancelation cleanup handler called.\n", wtiGetDbgHdr(pThis));
	
	/* call user supplied handler */
	pWtp->pfOnWorkerCancel(pThis->pWtp->pUsr, pThis->batch.pElem[0].pUsrp);

	ENDfunc
}


/* wait for queue to become non-empty or timeout
 * helper to wtiWorker. Note the the predicate is
 * re-tested by the caller, so it is OK to NOT do it here.
 * rgerhards, 2009-05-20
 */
static inline void
doIdleProcessing(wti_t *pThis, wtp_t *pWtp, int *pbInactivityTOOccured)
{
	struct timespec t;

	BEGINfunc
	DBGPRINTF("%s: worker IDLE, waiting for work.\n", wtiGetDbgHdr(pThis));

	pWtp->pfOnIdle(pWtp->pUsr, MUTEX_ALREADY_LOCKED);

	d_pthread_mutex_lock(pWtp->pmutUsr);
	if(pThis->bAlwaysRunning) {
		/* never shut down any started worker */
		d_pthread_cond_wait(pWtp->pcondBusy, pWtp->pmutUsr);
	} else {
		timeoutComp(&t, pWtp->toWrkShutdown);/* get absolute timeout */
		if(d_pthread_cond_timedwait(pWtp->pcondBusy, pWtp->pmutUsr, &t) != 0) {
			DBGPRINTF("%s: inactivity timeout, worker terminating...\n", wtiGetDbgHdr(pThis));
			*pbInactivityTOOccured = 1; /* indicate we had a timeout */
		}
	}
	d_pthread_mutex_unlock(pWtp->pmutUsr);
	ENDfunc
}


/* generic worker thread framework
 */
#pragma GCC diagnostic ignored "-Wempty-body"
rsRetVal
wtiWorker(wti_t *pThis)
{
	wtp_t *pWtp;		/* our worker thread pool */
	int bInactivityTOOccured = 0;
	rsRetVal localRet;
	rsRetVal terminateRet;
	int iCancelStateSave;
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, wti);
	pWtp = pThis->pWtp; /* shortcut */
	ISOBJ_TYPE_assert(pWtp, wtp);

	dbgSetThrdName(pThis->pszDbgHdr);
	pthread_cleanup_push(wtiWorkerCancelCleanup, pThis);

	pWtp->pfOnWorkerStartup(pWtp->pUsr);

	/* now we have our identity, on to real processing */
	while(1) { /* loop will be broken below - need to do mutex locks */
		if(pWtp->pfRateLimiter != NULL) { /* call rate-limiter, if defined */
			pWtp->pfRateLimiter(pWtp->pUsr);
		}
		
		d_pthread_mutex_lock(pWtp->pmutUsr);

		/* first check if we are in shutdown process (but evaluate a bit later) */
		terminateRet = wtpChkStopWrkr(pWtp, MUTEX_ALREADY_LOCKED);
		if(terminateRet == RS_RET_TERMINATE_NOW) {
			/* we now need to free the old batch */
			localRet = pWtp->pfObjProcessed(pWtp->pUsr, pThis);
			dbgoprint((obj_t*) pThis, "terminating worker because of TERMINATE_NOW mode, del iRet %d\n",
				 localRet);
			d_pthread_mutex_unlock(pWtp->pmutUsr);
			break;
		}

		/* try to execute and process whatever we have */
		/* This function must and does RELEASE the MUTEX! */
		localRet = pWtp->pfDoWork(pWtp->pUsr, pThis);

		if(localRet == RS_RET_IDLE) {
			if(terminateRet == RS_RET_TERMINATE_WHEN_IDLE || bInactivityTOOccured) {
				break;	/* end of loop */
			}
			doIdleProcessing(pThis, pWtp, &bInactivityTOOccured);
			continue; /* request next iteration */
		}

		bInactivityTOOccured = 0; /* reset for next run */
	}

	/* indicate termination */
	d_pthread_mutex_lock(pWtp->pmutUsr);
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave);
	pthread_cleanup_pop(0); /* remove cleanup handler */
	pWtp->pfOnWorkerShutdown(pWtp->pUsr);
	pthread_setcancelstate(iCancelStateSave, NULL);
	d_pthread_mutex_unlock(pWtp->pmutUsr);

	RETiRet;
}
#pragma GCC diagnostic warning "-Wempty-body"


/* some simple object access methods */
DEFpropSetMeth(wti, pWtp, wtp_t*)

/* set the debug header message
 * The passed-in string is duplicated. So if the caller does not need
 * it any longer, it must free it. Must be called only before object is finalized.
 * rgerhards, 2008-01-09
 */
rsRetVal
wtiSetDbgHdr(wti_t *pThis, uchar *pszMsg, size_t lenMsg)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, wti);
	assert(pszMsg != NULL);
	
	if(lenMsg < 1)
		ABORT_FINALIZE(RS_RET_PARAM_ERROR);

	if(pThis->pszDbgHdr != NULL) {
		free(pThis->pszDbgHdr);
	}

	if((pThis->pszDbgHdr = malloc(sizeof(uchar) * lenMsg + 1)) == NULL)
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

	memcpy(pThis->pszDbgHdr, pszMsg, lenMsg + 1); /* always think about the \0! */

finalize_it:
	RETiRet;
}


/* dummy */
rsRetVal wtiQueryInterface(void) { return RS_RET_NOT_IMPLEMENTED; }

/* exit our class
 */
BEGINObjClassExit(wti, OBJ_IS_CORE_MODULE) /* CHANGE class also in END MACRO! */
CODESTARTObjClassExit(nsdsel_gtls)
	/* release objects we no longer need */
	objRelease(glbl, CORE_COMPONENT);
ENDObjClassExit(wti)


/* Initialize the wti class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-01-09
 */
BEGINObjClassInit(wti, 1, OBJ_IS_CORE_MODULE) /* one is the object version (most important for persisting) */
	/* request objects we use */
	CHKiRet(objUse(glbl, CORE_COMPONENT));
ENDObjClassInit(wti)

/* vi:set ai:
 */
