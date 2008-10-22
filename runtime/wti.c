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

#include "rsyslog.h"
#include "stringbuf.h"
#include "srUtils.h"
#include "wtp.h"
#include "wti.h"
#include "obj.h"
#include "glbl.h"

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
 * bActiveOnly specifies if the command should be sent only when the worker is
 * in an active state. -- rgerhards, 2008-01-20
 */
rsRetVal
wtiSetState(wti_t *pThis, qWrkCmd_t tCmd, int bActiveOnly, int bLockMutex)
{
	DEFiRet;
	DEFVARS_mutexProtection;

	ISOBJ_TYPE_assert(pThis, wti);
	assert(tCmd <= eWRKTHRD_SHUTDOWN_IMMEDIATE);

	BEGIN_MTX_PROTECTED_OPERATIONS(&pThis->mut, bLockMutex);

	/* all worker states must be followed sequentially, only termination can be set in any state */
	if(   (bActiveOnly && (pThis->tCurrCmd < eWRKTHRD_RUN_CREATED))
	   || (pThis->tCurrCmd > tCmd && !(tCmd == eWRKTHRD_TERMINATING || tCmd == eWRKTHRD_STOPPED))) {
		dbgprintf("%s: command %d can not be accepted in current %d processing state - ignored\n",
			  wtiGetDbgHdr(pThis), tCmd, pThis->tCurrCmd);
	} else {
		dbgprintf("%s: receiving command %d\n", wtiGetDbgHdr(pThis), tCmd);
		/* we could replace this with a simple if, but we leave the switch in in case we need
		 * to add something at a later stage. -- rgerhards, 2008-09-30
		 */
		switch(tCmd) {
			case eWRKTHRD_TERMINATING:
				/* TODO: re-enable meaningful debug msg! (via function callback?)
				dbgprintf("%s: thread terminating with %d entries left in queue, %d workers running.\n",
					  wtiGetDbgHdr(pThis->pQueue), pThis->pQueue->iQueueSize,
					  pThis->pQueue->iCurNumWrkThrd);
				*/
				pthread_cond_signal(&pThis->condExitDone);
				dbgprintf("%s: worker terminating\n", wtiGetDbgHdr(pThis));
				break;
			/* these cases just to satisfy the compiler, we do (yet) not act an them: */
			case eWRKTHRD_RUNNING:
			case eWRKTHRD_STOPPED:
			case eWRKTHRD_RUN_CREATED:
			case eWRKTHRD_RUN_INIT:
			case eWRKTHRD_SHUTDOWN:
			case eWRKTHRD_SHUTDOWN_IMMEDIATE:
				/* DO NOTHING */
				break;
		}
		pThis->tCurrCmd = tCmd; /* apply the new state */
	}

	END_MTX_PROTECTED_OPERATIONS(&pThis->mut);
	RETiRet;
}


/* Cancel the thread. If the thread is already cancelled or termination,
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

	if(pThis->tCurrCmd >= eWRKTHRD_TERMINATING) {
		dbgoprint((obj_t*) pThis, "canceling worker thread\n");
		pthread_cancel(pThis->thrdID);
		wtiSetState(pThis, eWRKTHRD_TERMINATING, 0, MUTEX_ALREADY_LOCKED);
		pThis->pWtp->bThrdStateChanged = 1; /* indicate change, so harverster will be called */
	}

	d_pthread_mutex_unlock(&pThis->mut);

	RETiRet;
}


/* Destructor */
BEGINobjDestruct(wti) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(wti)
	/* if we reach this point, we must make sure the associated worker has terminated. It is
	 * the callers duty to make sure the worker already knows it shall terminate.
	 * TODO: is it *really* the caller's duty? ...mmmhhhh.... smells bad... rgerhards, 2008-01-25
	 */
	wtiProcessThrdChanges(pThis, LOCK_MUTEX); /* process state change one last time */

	d_pthread_mutex_lock(&pThis->mut);
	if(wtiGetState(pThis, MUTEX_ALREADY_LOCKED) != eWRKTHRD_STOPPED) {
		dbgprintf("%s: WARNING: worker %p shall be destructed but is still running (might be OK) - joining it\n",
			  wtiGetDbgHdr(pThis), pThis);
		/* let's hope the caller actually instructed it to shutdown... */
		pthread_cond_wait(&pThis->condExitDone, &pThis->mut);
		wtiJoinThrd(pThis);
	}
	d_pthread_mutex_unlock(&pThis->mut);

	/* actual destruction */
	pthread_cond_destroy(&pThis->condExitDone);
	pthread_mutex_destroy(&pThis->mut);

	if(pThis->pszDbgHdr != NULL)
		free(pThis->pszDbgHdr);
ENDobjDestruct(wti)


/* Standard-Constructor for the wti object
 */
BEGINobjConstruct(wti) /* be sure to specify the object type also in END macro! */
	pThis->bOptimizeUniProc = glbl.GetOptimizeUniProc();
	pthread_cond_init(&pThis->condExitDone, NULL);
	pthread_mutex_init(&pThis->mut, NULL);
ENDobjConstruct(wti)


/* Construction finalizer
 * rgerhards, 2008-01-17
 */
rsRetVal
wtiConstructFinalize(wti_t *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, wti);

	dbgprintf("%s: finalizing construction of worker instance data\n", wtiGetDbgHdr(pThis));

	/* initialize our thread instance descriptor */
	pThis->pUsrp = NULL;
	pThis->tCurrCmd = eWRKTHRD_STOPPED;

	RETiRet;
}


/* join a specific worker thread
 * we do not lock the mutex, because join will sync anyways...
 */
rsRetVal
wtiJoinThrd(wti_t *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, wti);
	dbgprintf("waiting for worker %s termination, current state %d\n", wtiGetDbgHdr(pThis), pThis->tCurrCmd);
	pthread_join(pThis->thrdID, NULL);
	wtiSetState(pThis, eWRKTHRD_STOPPED, 0, MUTEX_ALREADY_LOCKED); /* back to virgin... */
	pThis->thrdID = 0; /* invalidate the thread ID so that we do not accidently find reused ones */
	dbgprintf("worker %s has stopped\n", wtiGetDbgHdr(pThis));

	RETiRet;
}

/* check if we had a worker thread changes and, if so, act
 * on it. At a minimum, terminated threads are harvested (joined).
 */
rsRetVal
wtiProcessThrdChanges(wti_t *pThis, int bLockMutex)
{
	DEFiRet;
	DEFVARS_mutexProtection;

	ISOBJ_TYPE_assert(pThis, wti);

	BEGIN_MTX_PROTECTED_OPERATIONS(&pThis->mut, bLockMutex);
	switch(pThis->tCurrCmd) {
		case eWRKTHRD_TERMINATING:
			/* we need to at least temporarily release the mutex, because otherwise
			 * we may deadlock with the thread we intend to join (it aquires the mutex
			 * during termination processing). -- rgerhards, 2008-02-26
			 */
			END_MTX_PROTECTED_OPERATIONS(&pThis->mut);
			iRet = wtiJoinThrd(pThis);
			BEGIN_MTX_PROTECTED_OPERATIONS(&pThis->mut, bLockMutex);
			break;
		/* these cases just to satisfy the compiler, we do not act an them: */
		case eWRKTHRD_STOPPED:
		case eWRKTHRD_RUN_CREATED:
		case eWRKTHRD_RUN_INIT:
		case eWRKTHRD_RUNNING:
		case eWRKTHRD_SHUTDOWN:
		case eWRKTHRD_SHUTDOWN_IMMEDIATE:
			/* DO NOTHING */
			break;
	}
	END_MTX_PROTECTED_OPERATIONS(&pThis->mut);

	RETiRet;
}


/* cancellation cleanup handler for queueWorker ()
 * Updates admin structure and frees ressources.
 * rgerhards, 2008-01-16
 */
static void
wtiWorkerCancelCleanup(void *arg)
{
	wti_t *pThis = (wti_t*) arg;
	wtp_t *pWtp;
	int iCancelStateSave;

	BEGINfunc
	ISOBJ_TYPE_assert(pThis, wti);
	pWtp = pThis->pWtp;
	ISOBJ_TYPE_assert(pWtp, wtp);

	DBGPRINTF("%s: cancelation cleanup handler called.\n", wtiGetDbgHdr(pThis));
	
	/* call user supplied handler (that one e.g. requeues the element) */
	pWtp->pfOnWorkerCancel(pThis->pWtp->pUsr, pThis->pUsrp);

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave);
	d_pthread_mutex_lock(&pWtp->mut);
	wtiSetState(pThis, eWRKTHRD_TERMINATING, 0, MUTEX_ALREADY_LOCKED);
	/* TODO: sync access? I currently think it is NOT needed -- rgerhards, 2008-01-28 */
	pWtp->bThrdStateChanged = 1; /* indicate change, so harverster will be called */

	d_pthread_mutex_unlock(&pWtp->mut);
	pthread_setcancelstate(iCancelStateSave, NULL);
	ENDfunc
}


/* generic worker thread framework
 *
 * Some special comments below, so that they do not clutter the main function code:
 *
 * On the use of pthread_testcancel():
 * Now make sure we can get canceled - it is not specified if pthread_setcancelstate() is
 * a cancellation point in itself. As we run most of the time without cancel enabled, I fear
 * we may never get cancelled if we do not create a cancellation point ourselfs.
 *
 * On the use of pthread_yield():
 * We yield to give the other threads a chance to obtain the mutex. If we do not
 * do that, this thread may very well aquire the mutex again before another thread
 * has even a chance to run. The reason is that mutex operations are free to be
 * implemented in the quickest possible way (and they typically are!). That is, the
 * mutex lock/unlock most probably just does an atomic memory swap and does not necessarily
 * schedule other threads waiting on the same mutex. That can lead to the same thread
 * aquiring the mutex ever and ever again while all others are starving for it. We
 * have exactly seen this behaviour when we deliberately introduced a long-running
 * test action which basically did a sleep. I understand that with real actions the
 * likelihood of this starvation condition is very low - but it could still happen
 * and would be very hard to debug. The yield() is a sure fix, its performance overhead
 * should be well accepted given the above facts. -- rgerhards, 2008-01-10
 */
#pragma GCC diagnostic ignored "-Wempty-body"
rsRetVal
wtiWorker(wti_t *pThis)
{
	DEFiRet;
	DEFVARS_mutexProtection;
	struct timespec t;
	wtp_t *pWtp;		/* our worker thread pool */
	int bInactivityTOOccured = 0;

	ISOBJ_TYPE_assert(pThis, wti);
	pWtp = pThis->pWtp; /* shortcut */
	ISOBJ_TYPE_assert(pWtp, wtp);

	dbgSetThrdName(pThis->pszDbgHdr);
	pThis->pUsrp = NULL;
	pthread_cleanup_push(wtiWorkerCancelCleanup, pThis);

	BEGIN_MTX_PROTECTED_OPERATIONS(pWtp->pmutUsr, LOCK_MUTEX);
	pWtp->pfOnWorkerStartup(pWtp->pUsr);
	END_MTX_PROTECTED_OPERATIONS(pWtp->pmutUsr);

	/* now we have our identity, on to real processing */
	while(1) { /* loop will be broken below - need to do mutex locks */
		/* process any pending thread requests */
		wtpProcessThrdChanges(pWtp);
		pthread_testcancel(); /* see big comment in function header */
#		if !defined(__hpux) /* pthread_yield is missing there! */
		if(pThis->bOptimizeUniProc)
			pthread_yield(); /* see big comment in function header */
#		endif

		/* if we have a rate-limiter set for this worker pool, let's call it. Please
		 * keep in mind that the rate-limiter may hold us for an extended period
		 * of time. -- rgerhards, 2008-04-02
		 */
		if(pWtp->pfRateLimiter != NULL) {
			pWtp->pfRateLimiter(pWtp->pUsr);
		}
		
		wtpSetInactivityGuard(pThis->pWtp, 0, LOCK_MUTEX); /* must be set before usr mutex is locked! */
		BEGIN_MTX_PROTECTED_OPERATIONS(pWtp->pmutUsr, LOCK_MUTEX);

		if(  (bInactivityTOOccured && pWtp->pfIsIdle(pWtp->pUsr, MUTEX_ALREADY_LOCKED))
		   || wtpChkStopWrkr(pWtp, LOCK_MUTEX, MUTEX_ALREADY_LOCKED)) {
			END_MTX_PROTECTED_OPERATIONS(pWtp->pmutUsr);
			break; /* end worker thread run */
		}
		bInactivityTOOccured = 0; /* reset for next run */

		/* if we reach this point, we are still protected by the mutex */

		if(pWtp->pfIsIdle(pWtp->pUsr, MUTEX_ALREADY_LOCKED)) {
			DBGPRINTF("%s: worker IDLE, waiting for work.\n", wtiGetDbgHdr(pThis));
			pWtp->pfOnIdle(pWtp->pUsr, MUTEX_ALREADY_LOCKED);

			if(pWtp->toWrkShutdown == -1) {
				/* never shut down any started worker */
				d_pthread_cond_wait(pWtp->pcondBusy, pWtp->pmutUsr);
			} else {
				timeoutComp(&t, pWtp->toWrkShutdown);/* get absolute timeout */
				if(d_pthread_cond_timedwait(pWtp->pcondBusy, pWtp->pmutUsr, &t) != 0) {
					DBGPRINTF("%s: inactivity timeout, worker terminating...\n", wtiGetDbgHdr(pThis));
					bInactivityTOOccured = 1; /* indicate we had a timeout */
				}
			}
			END_MTX_PROTECTED_OPERATIONS(pWtp->pmutUsr);
			continue; /* request next iteration */
		}

		/* if we reach this point, we have a non-empty queue (and are still protected by mutex) */
		pWtp->pfDoWork(pWtp->pUsr, pThis, iCancelStateSave);
	}

	/* indicate termination */
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave);
	d_pthread_mutex_lock(&pThis->mut);
	pthread_cleanup_pop(0); /* remove cleanup handler */

	pWtp->pfOnWorkerShutdown(pWtp->pUsr);

	wtiSetState(pThis, eWRKTHRD_TERMINATING, 0, MUTEX_ALREADY_LOCKED);
	pWtp->bThrdStateChanged = 1; /* indicate change, so harverster will be called */
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
