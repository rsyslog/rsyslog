/* wtp.c
 *
 * This file implements the worker thread pool (wtp) class.
 * 
 * File begun on 2008-01-20 by RGerhards
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
#include <fcntl.h>
#include <unistd.h>
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
wtpGetDbgHdr(wtp_t *pThis)
{
	ISOBJ_TYPE_assert(pThis, wtp);

	if(pThis->pszDbgHdr == NULL)
		return (uchar*) "wtp"; /* should not normally happen */
	else
		return pThis->pszDbgHdr;
}



/* Not implemented dummy function for constructor */
static rsRetVal NotImplementedDummy() { return RS_RET_NOT_IMPLEMENTED; }
/* Standard-Constructor for the wtp object
 */
BEGINobjConstruct(wtp) /* be sure to specify the object type also in END macro! */
	int i;
	uchar pszBuf[64];
	size_t lenBuf;
	wti_t *pWti;

	pthread_mutex_init(&pThis->mut, NULL);
	pthread_cond_init(&pThis->condThrdTrm, NULL);
	/* set all function pointers to "not implemented" dummy so that we can safely call them */
	pThis->pfChkStopWrkr = NotImplementedDummy;
	pThis->pfIsIdle = NotImplementedDummy;
	pThis->pfDoWork = NotImplementedDummy;
	pThis->pfOnShutdownAdvise = NotImplementedDummy;
	pThis->pfOnIdle = NotImplementedDummy;
	pThis->pfOnWorkerCancel = NotImplementedDummy;

	/* alloc and construct workers */
	if((pThis->pWrkr = malloc(sizeof(wti_t*) * pThis->iNumWorkerThreads)) == NULL)
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

	for(i = 0 ; i < pThis->iNumWorkerThreads ; ++i) {
		CHKiRet(wtiConstruct(&pThis->pWrkr[i]));
		pWti = pThis->pWrkr[i];
		lenBuf = snprintf((char*)pszBuf, sizeof(pszBuf), "%s/w%d", wtpGetDbgHdr(pThis), i);
		CHKiRet(wtiSetDbgHdr(pWti, pszBuf, lenBuf));
		CHKiRet(wtiConstructFinalize(pWti));
	}
		
finalize_it:
ENDobjConstruct(wtp)


/* Construction finalizer
 * rgerhards, 2008-01-17
 */
rsRetVal
wtpConstructFinalize(wtp_t __attribute__((unused)) *pThis)
{
	ISOBJ_TYPE_assert(pThis, wtp);

	dbgprintf("%s: finalizing construction of worker thread pool\n", wtpGetDbgHdr(pThis));

	return RS_RET_OK;
}


/* Destructor */
rsRetVal
wtpDestruct(wtp_t **ppThis)
{
	DEFiRet;
	wtp_t *pThis;
	int iCancelStateSave;
	int i;

	assert(ppThis != NULL);
	pThis = *ppThis;
	ISOBJ_TYPE_assert(pThis, wtp);

	/* we can not be canceled, that would have a myriad of side-effects */
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave);

	/* destruct workers */
	for(i = 0 ; i < pThis->iNumWorkerThreads ; ++i)
		wtiDestruct(&pThis->pWrkr[i]);

	free(pThis->pWrkr);
	pThis->pWrkr = NULL;

	/* actual destruction */
	pthread_cond_destroy(&pThis->condThrdTrm);
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


/* wake up all worker threads. Param bWithDAWrk tells if the DA worker
 * is to be awaken, too. It needs special handling because it waits on
 * two different conditions depending on processing state.
 * rgerhards, 2008-01-16
 */
static inline rsRetVal
wtpWakeupWrks(wtp_t *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, wtp);
	pthread_cond_broadcast(pThis->pcondBusy);
	RETiRet;
}


/* check if we had any worker thread changes and, if so, act
 * on them. At a minimum, terminated threads are harvested (joined).
 * This function MUST NEVER block on the queue mutex!
 */
rsRetVal
wtpProcessThrdChanges(wtp_t *pThis)
{
	DEFiRet;
	int i;

	ISOBJ_TYPE_assert(pThis, wtp);

	if(pThis->bThrdStateChanged == 0)
		FINALIZE;

	/* go through all threads */
	for(i = 0 ; i < pThis->iNumWorkerThreads ; ++i) {
		wtiProcessThrdChanges(pThis->pWrkr[i], LOCK_MUTEX);
	}

finalize_it:
	RETiRet;
}


/* Sent a specific state for the worker thread pool.
 * rgerhards, 2008-01-21
 */
rsRetVal
wtpSetState(wtp_t *pThis, wtpState_t iNewState)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, wtp);
	pThis->wtpState = iNewState;
	// TODO: must wakeup workers?

	RETiRet;
}


#if 0
/* Send a shutdown command to all workers and see if they terminate.
 * A timeout may be specified.
 * rgerhards, 2008-01-14
 */
static rsRetVal
wtpWrkrShutdown(wtp_t *pThis)
{
	DEFiRet;
	int bTimedOut;
	struct timespec t;
	int iCancelStateSave;

	// TODO: implement
	ISOBJ_TYPE_assert(pThis);
	queueTellActWrkThrds(pThis, 0, tShutdownCmd);/* first tell the workers our request */
	queueWakeupWrkThrds(pThis, 1); /* awake all workers including DA-worker */
	/* race: must make sure all are running! */
	queueTimeoutComp(&t, iTimeout);/* get timeout */
		
	/* and wait for their termination */
dbgprintf("Queue %p: waiting for mutex %p\n", pThis, &pThis->mutThrdMgmt);
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave);
	d_pthread_mutex_lock(&pThis->mutThrdMgmt);
	pthread_cleanup_push(queueMutexCleanup, &pThis->mutThrdMgmt);
	pthread_setcancelstate(iCancelStateSave, NULL);
	bTimedOut = 0;
	while(pThis->iCurNumWrkThrd > 0 && !bTimedOut) {
		dbgprintf("Queue 0x%lx: waiting %ldms on worker thread termination, %d still running\n",
			   queueGetID(pThis), iTimeout, pThis->iCurNumWrkThrd);

		if(pthread_cond_timedwait(&pThis->condThrdTrm, &pThis->mutThrdMgmt, &t) != 0) {
			dbgprintf("Queue 0x%lx: timeout waiting on worker thread termination\n", queueGetID(pThis));
			bTimedOut = 1;	/* we exit the loop on timeout */
		}
	}
	pthread_cleanup_pop(1);

	if(bTimedOut)
		iRet = RS_RET_TIMED_OUT;

	RETiRet;
}


/* Unconditionally cancel all running worker threads.
 * rgerhards, 2008-01-14
 */
static rsRetVal
wtpWrkrCancel(wtp_t *pThis)
{
	DEFiRet;
	int i;
	// TODO: we need to implement peek(), without it (today!) we lose one message upon
	// worker cancellation! -- rgerhards, 2008-01-14

	ISOB_TYPE_assert(pThis);
	/* process any pending thread requests so that we know who actually is still running */
	queueChkWrkThrdChanges(pThis);

	/* awake the workers one more time, just to be sure */
	queueWakeupWrkThrds(pThis, 1); /* awake all workers including DA-worker */

	/* first tell the workers our request */
	for(i = 0 ; i <= pThis->iNumWorkerThreads ; ++i) {
		if(pThis->pWrkThrds[i].tCurrCmd >= eWRKTHRD_TERMINATING) {
			dbgprintf("Queue 0x%lx: canceling worker thread %d\n", queueGetID(pThis), i);
			pthread_cancel(pThis->pWrkThrds[i].thrdID);
		}
	}

	RETiRet;
}


/* Worker thread management function carried out when the main
 * worker is about to terminate.
 */
static rsRetVal
wtpShutdownWorkers(wtp_t *pThis)
{
	DEFiRet;
	int i;

	assert(pThis != NULL);

	dbgprintf("Queue 0x%lx: initiating worker thread shutdown sequence\n", (unsigned long) pThis);

	ISOB_TYPE_assert(pThis);
	/* even if the timeout count is set to 0 (run endless), we still call the queueWrkThrdTrm(). This
	 * is necessary so that all threads get sent the termination command. With a timeout of 0, however,
	 * the function returns immediate with RS_RET_TIMED_OUT. We catch that state and accept it as
	 * good.
	 */
	iRet = queueWrkThrdTrm(pThis, eWRKTHRD_SHUTDOWN, pThis->toQShutdown);
	if(iRet == RS_RET_TIMED_OUT) {
		if(pThis->toQShutdown == 0) {
			iRet = RS_RET_OK;
		} else {
			/* OK, we now need to try force the shutdown */
			dbgprintf("Queue 0x%lx: regular worker shutdown timed out, now trying immediate\n",
				  queueGetID(pThis));
			iRet = queueWrkThrdTrm(pThis, eWRKTHRD_SHUTDOWN_IMMEDIATE, pThis->toActShutdown);
		}
	}

	if(iRet != RS_RET_OK) { /* this is true on actual error on first try or timeout and error on second */
		/* still didn't work out - so we now need to cancel the workers */
		dbgprintf("Queue 0x%lx: worker threads could not be shutdown, now canceling them\n", (unsigned long) pThis);
		iRet = queueWrkThrdCancel(pThis);
	}
	
	/* finally join the threads
	 * In case of a cancellation, this may actually take some time. This is also
	 * needed to clean up the thread descriptors, even with a regular termination.
	 * And, most importantly, this is needed if we have an indifitite termination
	 * time set (timeout == 0)! -- rgerhards, 2008-01-14
	 */
	for(i = 0 ; i <= pThis->iNumWorkerThreads ; ++i) {
		if(pThis->pWrkThrds[i].tCurrCmd != eWRKTHRD_STOPPED) {
			queueJoinWrkThrd(pThis, i);
		}
	}

	dbgprintf("Queue 0x%lx: worker threads terminated, remaining queue size %d.\n",
		  queueGetID(pThis), pThis->iQueueSize);

	RETiRet;
}
#endif



/* check if the worker shall shutdown
 * TODO: check if we can use atomic operations to enhance performance
 * Note: there may be two mutexes locked, the bLockUsrMutex is the one in our "user"
 * (e.g. the queue clas)
 * rgerhards, 2008-01-21
 */
rsRetVal
wtpChkStopWrkr(wtp_t *pThis, int bLockMutex, int bLockUsrMutex)
{
	DEFiRet;
	DEFVARS_mutexProtection;

	BEGIN_MTX_PROTECTED_OPERATIONS(&pThis->mut, bLockMutex);
	if(   (pThis->wtpState == wtpState_SHUTDOWN_IMMEDIATE)
	   || ((pThis->wtpState == wtpState_SHUTDOWN) && pThis->pfIsIdle(pThis->pUsr, bLockUsrMutex)))
		iRet = RS_RET_TERMINATE_NOW;
	END_MTX_PROTECTED_OPERATIONS(&pThis->mut);

	/* try customer handler if one was set and we do not yet have a definite result */
	if(iRet == RS_RET_OK && pThis->pfChkStopWrkr != NULL)
		iRet = pThis->pfChkStopWrkr(pThis->pUsr, bLockUsrMutex);

	RETiRet;
}

/* Set the Inactivity Guard
 * rgerhards, 2008-01-21
 */
rsRetVal
wtpSetInactivityGuard(wtp_t *pThis, int bNewState, int bLockMutex)
{
	DEFiRet;
	DEFVARS_mutexProtection;

	BEGIN_MTX_PROTECTED_OPERATIONS(&pThis->mut, bLockMutex);
	pThis->bInactivityGuard = bNewState;
	END_MTX_PROTECTED_OPERATIONS(&pThis->mut);

	RETiRet;
}


/* cancellation cleanup handler for executing worker
 * decrements the worker counter
 * rgerhards, 2008-01-20
 */
void
wtpWrkrExecCancelCleanup(void *arg)
{
	wtp_t *pThis = (wtp_t*) arg;

	ISOBJ_TYPE_assert(pThis, wtp);
	pThis->iCurNumWrkThrd--;

	dbgprintf("%s: thread CANCELED with %d workers running.\n", wtpGetDbgHdr(pThis), pThis->iCurNumWrkThrd);

}


/* wtp worker shell. This is started and calls into the actual
 * wti worker.
 * rgerhards, 2008-01-21
 */
static void *
wtpWorker(void *arg)
{
	DEFiRet;
	DEFVARS_mutexProtection;
	wti_t *pWti = (wti_t*) arg;
	wtp_t *pThis;
	sigset_t sigSet;

	ISOBJ_TYPE_assert(pWti, wti);
	pThis = pWti->pWtp;
	ISOBJ_TYPE_assert(pThis, wtp);

	sigfillset(&sigSet);
	pthread_sigmask(SIG_BLOCK, &sigSet, NULL);

	BEGIN_MTX_PROTECTED_OPERATIONS(&pThis->mut, LOCK_MUTEX);

	/* do some late initialization */

	pthread_cleanup_push(wtpWrkrExecCancelCleanup, pThis);

	// TODO: review code below - if still needed (setState yes!)? 
	/* finally change to RUNNING state. We need to check if we actually should still run,
	 * because someone may have requested us to shut down even before we got a chance to do
	 * our init. That would be a bad race... -- rgerhards, 2008-01-16
	 */
	//if(qWrkrGetState(pWrkrInst) == eWRKTHRD_RUN_INIT)
		wtiSetState(pWti, eWRKTHRD_RUNNING, MUTEX_ALREADY_LOCKED); /* we are running now! */

	do {
		END_MTX_PROTECTED_OPERATIONS(&pThis->mut);

		iRet = wtiWorker(pWti); /* just to make sure: this is NOT protected by the mutex! */

		BEGIN_MTX_PROTECTED_OPERATIONS(&pThis->mut, LOCK_MUTEX);
	} while(pThis->iCurNumWrkThrd == 1 && pThis->bInactivityGuard == 1);
	/* inactivity guard prevents shutdown of all workers while one should be running due to race
	 * condition. It can lead to one more worker running than desired, but that is acceptable. After
	 * all, that worker will shutdown itself due to inactivity timeout. If, however, none were running
	 * when one was required, processing could come to a halt. -- rgerhards, 2008-01-21
	 */

	pthread_cleanup_pop(0);
	pThis->iCurNumWrkThrd--;

	dbgprintf("%s: Worker thread %lx, terminated, num workers now %d\n",
		  wtpGetDbgHdr(pThis), (unsigned long) pWti, pThis->iCurNumWrkThrd);

	END_MTX_PROTECTED_OPERATIONS(&pThis->mut);

	pthread_exit(0);
}


/* start a new worker */
static rsRetVal
wtpStartWrkr(wtp_t *pThis, int bLockMutex)
{
	DEFiRet;
	DEFVARS_mutexProtection;
	wti_t *pWti;
	int i;
	int iState;

	ISOBJ_TYPE_assert(pThis, wtp);

	wtpProcessThrdChanges(pThis);

	BEGIN_MTX_PROTECTED_OPERATIONS(&pThis->mut, bLockMutex);

	pThis->iCurNumWrkThrd++;

	/* find free spot in thread table. If we find at least one worker that is in initialization,
	 * we do NOT start a new one. Let's give the other one a chance, first.
	 */
	for(i = 0 ; i < pThis->iNumWorkerThreads ; ++i) {
		// TODO: sync!
		if(pThis->pWrkr[i]->tCurrCmd == eWRKTHRD_STOPPED) {
			break;
		}
	}

dbgprintf("%s: after thrd search: i %d, max %d\n", wtpGetDbgHdr(pThis), i, pThis->iNumWorkerThreads);
	if(i == pThis->iNumWorkerThreads)
		ABORT_FINALIZE(RS_RET_NO_MORE_THREADS);

	pWti = pThis->pWrkr[i];
	wtiSetState(pWti, eWRKTHRD_RUN_CREATED, LOCK_MUTEX); // TODO: thuink about mutex lock
	iState = pthread_create(&(pWti->thrdID), NULL, wtpWorker, (void*) pWti);
	dbgprintf("%s: started with state %d, num workers now %d\n",
		  wtpGetDbgHdr(pThis), iState, pThis->iCurNumWrkThrd);

	/* we try to give the starting worker a little boost. It won't help much as we still
 	 * hold the queue's mutex, but at least it has a chance to start on a single-CPU system.
 	 */
	pthread_yield();

	/* indicate we just started a worker and would like to see it running */
	wtpSetInactivityGuard(pThis, 1, MUTEX_ALREADY_LOCKED);

finalize_it:
	END_MTX_PROTECTED_OPERATIONS(&pThis->mut);
	RETiRet;
}


/* set the number of worker threads that should be running. If less than currently running,
 * a new worker may be started. Please note that there is no guarantee the number of workers
 * said will be running after we exit this function. It is just a hint. If the number is
 * higher than one, the "busy" condition is also signaled to awake a worker.
 * rgerhards, 2008-01-21
 */
rsRetVal
wtpAdviseMaxWorkers(wtp_t *pThis, int nMaxWrkr)
{
	DEFiRet;
	DEFVARS_mutexProtection;
	int nMissing; /* number workers missing to run */
	int i;

	ISOBJ_TYPE_assert(pThis, wtp);

	if(nMaxWrkr == 0)
		FINALIZE;

	BEGIN_MTX_PROTECTED_OPERATIONS(&pThis->mut, LOCK_MUTEX);

	nMissing = nMaxWrkr - pThis->iCurNumWrkThrd;
	if(nMissing > pThis->iNumWorkerThreads)
		nMissing = pThis->iNumWorkerThreads;
	else if(nMissing < 0)
		nMissing = 0;

	dbgprintf("%s: high activity - starting %d additional worker thread(s).\n", wtpGetDbgHdr(pThis), nMissing);
	/* start the rqtd nbr of workers */
	for(i = 0 ; i < nMissing ; ++i) {
		CHKiRet(wtpStartWrkr(pThis, MUTEX_ALREADY_LOCKED));
	}
	
	END_MTX_PROTECTED_OPERATIONS(&pThis->mut);

finalize_it:
	RETiRet;
}


/* some simple object access methods */
DEFpropSetMeth(wtp, toWrkShutdown, long);
DEFpropSetMeth(wtp, wtpState, wtpState_t);


/* set the debug header message
 * The passed-in string is duplicated. So if the caller does not need
 * it any longer, it must free it. Must be called only before object is finalized.
 * rgerhards, 2008-01-09
 */
rsRetVal
wtpSetDbgHdr(wti_t *pThis, uchar *pszMsg, size_t lenMsg)
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

/* Initialize the stream class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-01-09
 */
BEGINObjClassInit(wtp, 1)
ENDObjClassInit(wtp)

/*
 * vi:set ai:
 */
