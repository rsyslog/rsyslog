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
 * This file is part of rsyslog.
 *
 * Rsyslog is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rsyslog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Rsyslog.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
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
#include "syslogd.h"
#include "stringbuf.h"
#include "srUtils.h"
#include "wtp.h"
#include "wti.h"
#include "obj.h"

/* static data */
DEFobjStaticHelpers

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
static inline qWrkCmd_t
wtiGetState(wti_t *pThis, int bLockMutex)
{
	DEFVARS_mutexProtection;
	qWrkCmd_t tCmd;

	ISOBJ_TYPE_assert(pThis, wti);

	BEGIN_MTX_PROTECTED_OPERATIONS(&pThis->mut, bLockMutex);
	tCmd = pThis->tCurrCmd;
	END_MTX_PROTECTED_OPERATIONS(&pThis->mut);

	return tCmd;
}



/* send a command to a specific thread
 * bActiveOnly specifies if the command should be sent only when the worker is
 * in an active state. -- rgerhards, 2008-01-20
 */
rsRetVal
wtiSetState(wti_t *pThis, qWrkCmd_t tCmd, int bActiveOnly)
{
	DEFiRet;
	DEFVARS_mutex_cancelsafeLock;

	ISOBJ_TYPE_assert(pThis, wti);
	assert(tCmd <= eWRKTHRD_SHUTDOWN_IMMEDIATE);

	mutex_cancelsafe_lock(&pThis->mut);

	/* all worker states must be followed sequentially, only termination can be set in any state */
	if(   (bActiveOnly && (pThis->tCurrCmd < eWRKTHRD_RUN_CREATED))
	   || (pThis->tCurrCmd > tCmd && tCmd != eWRKTHRD_TERMINATING)) {
		dbgprintf("%s: command %d can not be accepted in current %d processing state - ignored\n",
			  wtiGetDbgHdr(pThis), tCmd, pThis->tCurrCmd);
	} else {
		dbgprintf("%s: receiving command %d\n", wtiGetDbgHdr(pThis), tCmd);
		switch(tCmd) {
			case eWRKTHRD_RUN_CREATED:
				break;
			case eWRKTHRD_TERMINATING:
				/* TODO: re-enable meaningful debug msg! (via function callback?)
				dbgprintf("%s: thread terminating with %d entries left in queue, %d workers running.\n",
					  wtiGetDbgHdr(pThis->pQueue), pThis->pQueue->iQueueSize,
					  pThis->pQueue->iCurNumWrkThrd);
				*/
				dbgprintf("%s: worker terminating\n", wtiGetDbgHdr(pThis));
				break;
			case eWRKTHRD_RUNNING:
				pthread_cond_signal(&pThis->condInitDone);
				break;
			/* these cases just to satisfy the compiler, we do (yet) not act an them: */
			case eWRKTHRD_STOPPED:
			case eWRKTHRD_RUN_INIT:
			case eWRKTHRD_SHUTDOWN:
			case eWRKTHRD_SHUTDOWN_IMMEDIATE:
				/* DO NOTHING */
				break;
		}
		pThis->tCurrCmd = tCmd; /* apply the new state */
	}

	mutex_cancelsafe_unlock(&pThis->mut);
	RETiRet;
}


/* Destructor */
rsRetVal wtiDestruct(wti_t **ppThis)
{
	DEFiRet;
	wti_t *pThis;
	int iCancelStateSave;

	assert(ppThis != NULL);
	pThis = *ppThis;
	ISOBJ_TYPE_assert(pThis, wti);

	/* we can not be canceled, that would have a myriad of side-effects */
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave);

	/* actual destruction */
	pthread_cond_destroy(&pThis->condInitDone);
	pthread_mutex_destroy(&pThis->mut);

	if(pThis->pszDbgHdr != NULL)
		free(pThis->pszDbgHdr);

	/* and finally delete the queue objet itself */
	free(pThis);
	*ppThis = NULL;

	/* back to normal */
	pthread_setcancelstate(iCancelStateSave, NULL);

	RETiRet;
}


/* Standard-Constructor for the wti object
 */
BEGINobjConstruct(wti) /* be sure to specify the object type also in END macro! */
	pthread_cond_init(&pThis->condInitDone, NULL);
	pthread_mutex_init(&pThis->mut, NULL);
ENDobjConstruct(wti)


/* Construction finalizer
 * rgerhards, 2008-01-17
 */
rsRetVal
wtiConstructFinalize(wti_t *pThis)
{
	ISOBJ_TYPE_assert(pThis, wti);

	dbgprintf("%s: finalizing construction of worker instance data\n", wtiGetDbgHdr(pThis));

	/* initialize our thread instance descriptor */
	pThis->pUsr = NULL;
	pThis->tCurrCmd = eWRKTHRD_STOPPED;

	return RS_RET_OK;
}


/* join a specific worker thread
 * we do not lock the mutex, because join will sync anyways...
 */
rsRetVal
wtiJoinThrd(wti_t *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, wti);
	dbgprintf("wti: waiting for worker %s termination, current state %d\n", wtiGetDbgHdr(pThis), pThis->tCurrCmd);
	pthread_join(pThis->thrdID, NULL);
	wtiSetState(pThis, eWRKTHRD_STOPPED, 0); /* back to virgin... */
	pThis->thrdID = 0; /* invalidate the thread ID so that we do not accidently find reused ones */
	dbgprintf("wti: worker %s has stopped\n", wtiGetDbgHdr(pThis));

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
			iRet = wtiJoinThrd(pThis);
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

	ISOBJ_TYPE_assert(pThis, wti);
	pWtp = pThis->pWtp;
	ISOBJ_TYPE_assert(pWtp, wtp);

	dbgprintf("%s: cancelation cleanup handler called.\n", wtiGetDbgHdr(pThis));
	
	/* call user supplied handler (that one e.g. requeues the element) */
	pWtp->pfOnWorkerCancel(pThis->pWtp->pUsr);

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave);
	d_pthread_mutex_lock(&pWtp->mut);
	wtiSetState(pThis, eWRKTHRD_TERMINATING, 0);
	// TODO: sync access!
	pWtp->bThrdStateChanged = 1; /* indicate change, so harverster will be called */

	pthread_cond_signal(&pWtp->condThrdTrm); /* activate anyone waiting on thread shutdown */
	d_pthread_mutex_unlock(&pWtp->mut);
	pthread_setcancelstate(iCancelStateSave, NULL);
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

	pthread_cleanup_push(wtiWorkerCancelCleanup, pThis);

	/* now we have our identity, on to real processing */
	while(1) { /* loop will be broken below - need to do mutex locks */
dbgprintf("%s: start worker run, queue cmd currently %d\n", wtiGetDbgHdr(pThis), pThis->tCurrCmd);
		/* process any pending thread requests */
		wtpProcessThrdChanges(pWtp);
		pthread_testcancel(); /* see big comment in function header */
		pthread_yield(); /* see big comment in function header */

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
			dbgprintf("%s: worker IDLE, waiting for work.\n", wtiGetDbgHdr(pThis));
			pWtp->pfOnIdle(pWtp->pUsr, MUTEX_ALREADY_LOCKED);

			dbgprintf("%s: pre condwait ->notEmpty, worker shutdown %d\n",
				  wtiGetDbgHdr(pThis), pThis->pWtp->toWrkShutdown); // DEL
			if(pWtp->toWrkShutdown == -1) {
				dbgprintf("worker never times out!\n"); // DEL
				/* never shut down any started worker */
				pthread_cond_wait(pWtp->pcondBusy, pWtp->pmutUsr);
			} else {
				timeoutComp(&t, pWtp->toWrkShutdown);/* get absolute timeout */
				if(pthread_cond_timedwait(pWtp->pcondBusy, pWtp->pmutUsr, &t) != 0) {
					dbgprintf("%s: inactivity timeout, worker terminating...\n", wtiGetDbgHdr(pThis));
					bInactivityTOOccured = 1; /* indicate we had a timeout */
				}
			}
			dbgprintf("%s: post condwait ->notEmpty\n", wtiGetDbgHdr(pThis)); // DEL
			END_MTX_PROTECTED_OPERATIONS(pWtp->pmutUsr);
			continue; /* request next iteration */
		}

		/* if we reach this point, we have a non-empty queue (and are still protected by mutex) */
		pWtp->pfDoWork(pThis, iCancelStateSave);

		/* TODO: move this above into one of the chck Term functions */
		//if(Debug && (qWrkrGetState(pWrkrInst) == eWRKTHRD_SHUTDOWN) && pThis->iQueueSize > 0)
		//	dbgprintf("%s: worker does not yet terminate because it still has "
		//		  " %d messages to process.\n", wtiGetDbgHdr(pThis), pThis->iQueueSize);
	}

	/* indicate termination */
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave);
dbgprintf("%s: worker waiting for mutex\n", wtiGetDbgHdr(pThis));
	d_pthread_mutex_lock(&pThis->mut);
	pthread_cleanup_pop(0); /* remove cleanup handler */

	// TODO: I think we no longer need that - but check!
#if 0
	/* if we ever need finalize_it, here would be the place for it! */
	if(qWrkrGetState(pWrkrInst) == eWRKTHRD_SHUTDOWN ||
	   qWrkrGetState(pWrkrInst) == eWRKTHRD_SHUTDOWN_IMMEDIATE ||
	   qWrkrGetState(pWrkrInst) == eWRKTHRD_RUN_INIT ||
	   qWrkrGetState(pWrkrInst) == eWRKTHRD_RUN_CREATED) {
	   	/* in shutdown case, we need to flag termination. All other commands
		 * have a meaning to the thread harvester, so we can not overwrite them
		 */
dbgprintf("%s: setting termination state\n", wtiGetDbgHdr(pThis));
		wtiSetState(pWrkrInst, eWRKTHRD_TERMINATING, 0);
	}
#else
	wtiSetState(pThis, eWRKTHRD_TERMINATING, 0);
#endif
	// TODO: call, mutex:
	pWtp->bThrdStateChanged = 1; /* indicate change, so harverster will be called */
	pthread_cond_signal(&pWtp->condThrdTrm); /* activate anyone waiting on thread shutdown */
	d_pthread_mutex_unlock(&pThis->mut);
	pthread_setcancelstate(iCancelStateSave, NULL);

	RETiRet;
}


/* some simple object access methods */

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


/* Initialize the wti class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-01-09
 */
BEGINObjClassInit(wti, 1) /* one is the object version (most important for persisting) */
ENDObjClassInit(queue)

/*
 * vi:set ai:
 */
