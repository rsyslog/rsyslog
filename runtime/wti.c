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


/* get the current worker state. For simplicity and speed, we have
 * NOT used our regular calling interface this time. I hope that won't
 * bite in the long term... -- rgerhards, 2008-01-17
 * TODO: may be performance optimized by atomic operations
 */
qWrkCmd_t
wtiGetState(wti_t *pThis, int bLockMutex)
{
	DEFVARS_mutexProtection;
	qWrkCmd_t tCmd;

	BEGINfunc
	ISOBJ_TYPE_assert(pThis, wti);

	BEGIN_MTX_PROTECTED_OPERATIONS(&pThis->mut, bLockMutex);
	tCmd = pThis->tCurrCmd;
	END_MTX_PROTECTED_OPERATIONS(&pThis->mut);

	ENDfunc
	return tCmd;
}


/* send a command to a specific thread
 * rgerhards, 2008-01-20
 */
rsRetVal
wtiSetState(wti_t *pThis, qWrkCmd_t tCmd, int bLockMutex)
{
	DEFiRet;
	qWrkCmd_t tCurrCmd;
	DEFVARS_mutexProtection;

	ISOBJ_TYPE_assert(pThis, wti);
	assert(tCmd <= eWRKTHRD_SHUTDOWN_IMMEDIATE);

	BEGIN_MTX_PROTECTED_OPERATIONS(&pThis->mut, bLockMutex);

	tCurrCmd = pThis->tCurrCmd;
	/* all worker states must be followed sequentially, only termination can be set in any state */
	if(tCurrCmd > tCmd && !(tCmd == eWRKTHRD_STOPPED)) {
		DBGPRINTF("%s: command %d can not be accepted in current %d processing state - ignored\n",
			  wtiGetDbgHdr(pThis), tCmd, tCurrCmd);
	} else {
		DBGPRINTF("%s: receiving command %d\n", wtiGetDbgHdr(pThis), tCmd);
		/* we could replace this with a simple if, but we leave the switch in in case we need
		 * to add something at a later stage. -- rgerhards, 2008-09-30
		 */
		if(tCmd == eWRKTHRD_STOPPED) {
			dbgprintf("%s: worker almost stopped, assuming it has\n", wtiGetDbgHdr(pThis));
			pThis->thrdID = 0; /* invalidate the thread ID so that we do not accidently find reused ones */
		}
		/* apply the new state */
dbgprintf("worker terminator will write stateval %d\n", tCmd);
		unsigned val = ATOMIC_CAS_VAL(pThis->tCurrCmd, tCurrCmd, tCmd);
		if(val != tCurrCmd) {
			DBGPRINTF("wtiSetState PROBLEM, tCurrCmd %d overwritten with %d, wanted to set %d\n", tCurrCmd, val, tCmd);
		}
	}

	END_MTX_PROTECTED_OPERATIONS(&pThis->mut);
	RETiRet;
}


/* Cancel the thread. If the thread is already cancelled or terminated,
 * we do not again cancel it. But it is save and legal to call wtiCancelThrd() in
 * such situations.
 * rgerhards, 2008-02-26
 */
rsRetVal
wtiCancelThrd(wti_t *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, wti);

	d_pthread_mutex_lock(&pThis->mut);

	if(pThis->tCurrCmd != eWRKTHRD_STOPPED) {
		dbgoprint((obj_t*) pThis, "canceling worker thread, curr stat %d\n", pThis->tCurrCmd);
		pthread_cancel(pThis->thrdID);
		/* TODO: check: the following check should automatically be done by cancel cleanup handler! 2009-07-08 rgerhards */
		wtiSetState(pThis, eWRKTHRD_STOPPED, MUTEX_ALREADY_LOCKED);
	}

	d_pthread_mutex_unlock(&pThis->mut);

	RETiRet;
}


/* Destructor */
BEGINobjDestruct(wti) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(wti)
	if(Debug && wtiGetState(pThis, MUTEX_ALREADY_LOCKED) != eWRKTHRD_STOPPED) {
		dbgprintf("%s: WARNING: worker %p shall be destructed but is still running (might be OK) - ignoring\n",
			  wtiGetDbgHdr(pThis), pThis);
	}

	/* actual destruction */
	pthread_mutex_destroy(&pThis->mut);

	free(pThis->batch.pElem);
	free(pThis->pszDbgHdr);
ENDobjDestruct(wti)


/* Standard-Constructor for the wti object
 */
BEGINobjConstruct(wti) /* be sure to specify the object type also in END macro! */
	pthread_mutex_init(&pThis->mut, NULL);
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

	/* initialize our thread instance descriptor */
	pThis->tCurrCmd = eWRKTHRD_STOPPED;

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
	
	/* call user supplied handler (that one e.g. requeues the element) */
	pWtp->pfOnWorkerCancel(pThis->pWtp->pUsr, pThis->batch.pElem[0].pUsrp);

	d_pthread_mutex_lock(&pWtp->mut);
	wtiSetState(pThis, eWRKTHRD_STOPPED, MUTEX_ALREADY_LOCKED);
	/* TODO: sync access? I currently think it is NOT needed -- rgerhards, 2008-01-28 */
	d_pthread_mutex_unlock(&pWtp->mut);
	ENDfunc
}


/* wait for queue to become non-empty or timeout
 * helper to wtiWorker
 * IMPORTANT: mutex must be locked when this code is called!
 * rgerhards, 2009-05-20
 */
static inline void
doIdleProcessing(wti_t *pThis, wtp_t *pWtp, int *pbInactivityTOOccured)
{
	struct timespec t;

	BEGINfunc
	DBGPRINTF("%s: worker IDLE, waiting for work.\n", wtiGetDbgHdr(pThis));
	pWtp->pfOnIdle(pWtp->pUsr, MUTEX_ALREADY_LOCKED);

	if(pWtp->toWrkShutdown == -1) {
		/* never shut down any started worker */
		d_pthread_cond_wait(pWtp->pcondBusy, pWtp->pmutUsr);
	} else {
		timeoutComp(&t, pWtp->toWrkShutdown);/* get absolute timeout */
		if(d_pthread_cond_timedwait(pWtp->pcondBusy, pWtp->pmutUsr, &t) != 0) {
			DBGPRINTF("%s: inactivity timeout, worker terminating...\n", wtiGetDbgHdr(pThis));
			*pbInactivityTOOccured = 1; /* indicate we had a timeout */
		}
	}
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
	bool bMutexIsLocked;
	int iCancelStateSave;
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, wti);
	pWtp = pThis->pWtp; /* shortcut */
	ISOBJ_TYPE_assert(pWtp, wtp);

	dbgSetThrdName(pThis->pszDbgHdr);
	pthread_cleanup_push(wtiWorkerCancelCleanup, pThis);

	d_pthread_mutex_lock(pWtp->pmutUsr);
	pWtp->pfOnWorkerStartup(pWtp->pUsr);
	d_pthread_mutex_unlock(pWtp->pmutUsr);

	/* now we have our identity, on to real processing */
	while(1) { /* loop will be broken below - need to do mutex locks */
		if(pWtp->pfRateLimiter != NULL) { /* call rate-limiter, if defined */
			pWtp->pfRateLimiter(pWtp->pUsr);
		}
		
		wtpSetInactivityGuard(pThis->pWtp, 0, LOCK_MUTEX); /* must be set before usr mutex is locked! */
		d_pthread_mutex_lock(pWtp->pmutUsr);
		bMutexIsLocked = TRUE;

		/* first check if we are in shutdown process (but evaluate a bit later) */
		terminateRet = wtpChkStopWrkr(pWtp, LOCK_MUTEX, MUTEX_ALREADY_LOCKED);
		if(terminateRet == RS_RET_TERMINATE_NOW) {
			/* we now need to free the old batch */
			localRet = pWtp->pfObjProcessed(pWtp->pUsr, pThis);
			dbgoprint((obj_t*) pThis, "terminating worker because of TERMINATE_NOW mode, del iRet %d\n",
				 localRet);
			break;
		}

		/* try to execute and process whatever we have */
		/* This function must and does RELEASE the MUTEX! */
		localRet = pWtp->pfDoWork(pWtp->pUsr, pThis);
		bMutexIsLocked = FALSE;

		if(localRet == RS_RET_IDLE) {
			if(terminateRet == RS_RET_TERMINATE_WHEN_IDLE) {
				break;	/* end of loop */
			}

			if(bInactivityTOOccured) {
				/* we had an inactivity timeout in the last run and are still idle, so it is time to exit... */
				break; /* end worker thread run */
			}
			d_pthread_mutex_lock(pWtp->pmutUsr);
			doIdleProcessing(pThis, pWtp, &bInactivityTOOccured);
			d_pthread_mutex_unlock(pWtp->pmutUsr);
			continue; /* request next iteration */
		}

		bInactivityTOOccured = 0; /* reset for next run */
	}

	/* if we exit the loop, the mutex may be locked and, if so, must be unlocked */
	if(bMutexIsLocked) {
		d_pthread_mutex_unlock(pWtp->pmutUsr);
	}

	/* indicate termination */
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave);
	d_pthread_mutex_lock(&pThis->mut);
	pthread_cleanup_pop(0); /* remove cleanup handler */

	pWtp->pfOnWorkerShutdown(pWtp->pUsr);

	wtiSetState(pThis, eWRKTHRD_STOPPED, MUTEX_ALREADY_LOCKED);
	d_pthread_mutex_unlock(&pThis->mut);
	pthread_setcancelstate(iCancelStateSave, NULL);

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
		pThis->pszDbgHdr = NULL;
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

/*
 * vi:set ai:
 */
