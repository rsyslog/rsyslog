// TODO: think about mutDA - I think it's no longer needed
// TODO: start up the correct num of workers when switching to non-DA mode
// TODO: "preforked" worker threads
// TODO: do an if(debug) in dbgrintf - performance in release build!
// TODO: peekmsg() on first entry, with new/inprogress/deleted entry, destruction in 
// call consumer state. Facilitates retaining messages in queue until action could
// be called!
/* queue.c
 *
 * This file implements the queue object and its several queueing methods.
 * 
 * File begun on 2008-01-03 by RGerhards
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
#include "queue.h"
#include "stringbuf.h"
#include "srUtils.h"
#include "obj.h"

/* static data */
DEFobjStaticHelpers

/* debug aides */
#if 0
#define d_pthread_mutex_lock(x)   {dbgprintf("mutex %p   lock %s, %s(), line %d\n", x, __FILE__, __func__, __LINE__); \
				   pthread_mutex_lock(x); \
                                   if(1)dbgprintf("mutex %p   lock aquired %s, %s(), line %d\n", x, __FILE__, __func__, __LINE__); \
				  }
#define d_pthread_mutex_unlock(x) {dbgprintf("mutex %p UNlock %s, %s(), line %d\n", x ,__FILE__, __func__, __LINE__);\
				   pthread_mutex_unlock(x); \
                                   if(1)dbgprintf("mutex %p UNlock done %s, %s(), line %d\n", x, __FILE__, __func__, __LINE__); \
				  }
#else
#define d_pthread_mutex_lock(x)     pthread_mutex_lock(x)
#define d_pthread_mutex_unlock(x)   pthread_mutex_unlock(x)
#endif


/* forward-definitions */
rsRetVal queueChkPersist(queue_t *pThis);
static void *queueWorker(void *arg);
static rsRetVal queueChkWrkThrdChanges(queue_t *pThis);
static rsRetVal queueSetEnqOnly(queue_t *pThis, int bEnqOnly);

/* methods */


/* cancellation cleanup handler - frees provided mutex
 * rgerhards, 2008-01-14
 */
static void queueMutexCleanup(void *arg)
{
	assert(arg != NULL);
	d_pthread_mutex_unlock((pthread_mutex_t*) arg);
}


/* get the current worker state. For simplicity and speed, we have
 * NOT used our regular calling interface this time. I hope that won't
 * bite in the long term... -- rgerhards, 2008-01-17
 */
static inline qWrkCmd_t
qWrkrGetState(qWrkThrd_t *pThis)
{
	assert(pThis != NULL);
	return pThis->tCurrCmd;
}


/* indicate worker thread startup
 * (it would be best if we could do this with an atomic operation)
 * rgerhards, 2008-01-19
 */
static void
queueWrkrThrdStartupIndication(queue_t *pThis)
{
	int iCancelStateSave;

	ISOBJ_TYPE_assert(pThis, queue);
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave);
	d_pthread_mutex_lock(&pThis->mutThrdMgmt);
	pThis->iCurNumWrkThrd++;
	d_pthread_mutex_unlock(&pThis->mutThrdMgmt);
	pthread_setcancelstate(iCancelStateSave, NULL);
}


/* indicate worker thread shutdown
 * (it would be best if we could do this with an atomic operation)
 * rgerhards, 2008-01-19
 */
static void
queueWrkrThrdShutdownIndication(queue_t *pThis)
{
	int iCancelStateSave;

	ISOBJ_TYPE_assert(pThis, queue);
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave);
	d_pthread_mutex_lock(&pThis->mutThrdMgmt);
	pThis->iCurNumWrkThrd--;
	pthread_cond_signal(&pThis->condThrdTrm); /* activate anyone waiting on thread shutdown */
	d_pthread_mutex_unlock(&pThis->mutThrdMgmt);
	pthread_setcancelstate(iCancelStateSave, NULL);
}


/* send a command to a specific thread
 */
static rsRetVal
qWrkrSetState(qWrkThrd_t *pThis, qWrkCmd_t tCmd)
{
	DEFiRet;

	assert(pThis != NULL);

dbgprintf("Queue 0x%lx: trying to  send command %d to thread %d\n", queueGetID(pThis->pQueue), tCmd, pThis->iThrd);
	if(pThis->tCurrCmd == eWRKTHRD_SHUTDOWN_IMMEDIATE && tCmd != eWRKTHRD_TERMINATING)
		FINALIZE;

	dbgprintf("Queue 0x%lx: sending command %d to thread %d\n", queueGetID(pThis->pQueue), tCmd, pThis->iThrd);

	/* change some admin structures */
	switch(tCmd) {
		case eWRKTHRD_TERMINATING:
			pthread_cond_destroy(&pThis->condInitDone);
			pthread_mutex_destroy(&pThis->mut);
			dbgprintf("Queue 0x%lx/w%d: thread terminating with %d entries left in queue, %d workers running.\n",
				  queueGetID(pThis->pQueue), pThis->iThrd, pThis->pQueue->iQueueSize,
				  pThis->pQueue->iCurNumWrkThrd);
			break;
		case eWRKTHRD_RUN_CREATED:
			pthread_cond_init(&pThis->condInitDone, NULL);
			pthread_mutex_init(&pThis->mut, NULL);
			break;
		case eWRKTHRD_RUN_INIT:
			break;
		case eWRKTHRD_RUNNING:
			pthread_cond_signal(&pThis->condInitDone);
			break;
		/* these cases just to satisfy the compiler, we do (yet) not act an them: */
		case eWRKTHRD_STOPPED:
		case eWRKTHRD_SHUTDOWN:
		case eWRKTHRD_SHUTDOWN_IMMEDIATE:
			/* DO NOTHING */
			break;
	}

	pThis->tCurrCmd = tCmd;

finalize_it:
	return iRet;
}

/* send a command to a specific active thread. If the thread is not
 * active, the command is not sent.
 */
static inline rsRetVal
queueTellActWrkThrd(queue_t *pThis, int iIdx, qWrkCmd_t tCmd)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, queue);
	assert(iIdx >= 0 && iIdx <= pThis->iNumWorkerThreads);

	if(pThis->pWrkThrds[iIdx].tCurrCmd >= eWRKTHRD_RUN_CREATED) {
		qWrkrSetState(&pThis->pWrkThrds[iIdx], tCmd);
	} else {
		dbgprintf("Queue 0x%lx: command %d NOT sent to inactive thread %d\n", queueGetID(pThis), tCmd, iIdx);
	}

	return iRet;
}


/* Finalize construction of a wWrkrThrd_t "object"
 * rgerhards, 2008-01-17
 */
static inline rsRetVal
qWrkrConstructFinalize(qWrkThrd_t *pThis, queue_t *pQueue, int i)
{
	assert(pThis != NULL);
	ISOBJ_TYPE_assert(pQueue, queue);

	dbgprintf("Queue 0x%lx: finalizing construction of worker %d instance data\n", queueGetID(pQueue), i);

	/* initialize our thread instance descriptor */
	pThis = pQueue->pWrkThrds + i;
	pThis->pQueue = pQueue;
	pThis->iThrd = i;
	pThis->pUsr = NULL;

	qWrkrSetState(pThis, eWRKTHRD_STOPPED);

	return RS_RET_OK;
}


/* Waits until the specified worker thread 
 * changed to full running state (aka have started up). This function
 * MUST NOT be called while the queue mutex is locked as it does
 * this itself. The wait is without timeout.
 * rgerhards, 2008-01-17
 */
static inline rsRetVal
qWrkrWaitStartup(qWrkThrd_t *pThis)
{
	int iCancelStateSave;

	assert(pThis != NULL);

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave);
	d_pthread_mutex_lock(pThis->pQueue->mut);
	if((pThis->tCurrCmd == eWRKTHRD_RUN_CREATED) || (pThis->tCurrCmd == eWRKTHRD_RUN_CREATED)) {
		dbgprintf("Queue 0x%lx: waiting on worker thread %d startup\n", queueGetID(pThis->pQueue),
			  pThis->iThrd);
		pthread_cond_wait(&pThis->condInitDone, pThis->pQueue->mut);
dbgprintf("worker startup done!\n");
	}
	d_pthread_mutex_unlock(pThis->pQueue->mut);
	pthread_setcancelstate(iCancelStateSave, NULL);

	return RS_RET_OK;
}


/* waits until all worker threads that a currently initializing are fully started up
 * rgerhards, 2008-01-18
 */
static rsRetVal
qWrkrWaitAllWrkrStartup(queue_t *pThis)
{
	DEFiRet;
	int i;

	for(i = 0 ; i <= pThis->iNumWorkerThreads ; ++i) {
		qWrkrWaitStartup(pThis->pWrkThrds + i);
	}

	return iRet;
}


/* initialize the qWrkThrd_t structure - this MUST be called right after
 * startup of a worker thread. -- rgerhards, 2008-01-17
 */
static inline rsRetVal
qWrkrInit(qWrkThrd_t **ppThis, queue_t *pQueue)
{
	qWrkThrd_t *pThis;
	int i;

	assert(ppThis != NULL);
	ISOBJ_TYPE_assert(pQueue, queue);

	/* find myself in the queue's thread table */
	for(i = 0 ; i <= pQueue->iNumWorkerThreads ; ++i)
		if(pQueue->pWrkThrds[i].thrdID == pthread_self())
			break;
dbgprintf("Queue %p, worker start, thrd id found %x, idx %d, self %x\n", pQueue,
	(unsigned) pQueue->pWrkThrds[i].thrdID, i, (unsigned) pthread_self());
	assert(pQueue->pWrkThrds[i].thrdID == pthread_self());

	/* initialize our thread instance descriptor */
	pThis = pQueue->pWrkThrds + i;
	pThis->pQueue = pQueue;
	pThis->iThrd = i;
	pThis->pUsr = NULL;

	*ppThis = pThis;
	qWrkrSetState(pThis, eWRKTHRD_RUN_INIT);

	return RS_RET_OK;
}


/* join a specific worker thread
 */
static inline rsRetVal
queueJoinWrkThrd(queue_t *pThis, int iIdx)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, queue);
	assert(iIdx >= 0 && iIdx <= pThis->iNumWorkerThreads);
	assert(pThis->pWrkThrds[iIdx].tCurrCmd != eWRKTHRD_STOPPED);

	dbgprintf("Queue 0x%lx: thread %d state %d, waiting for exit\n", queueGetID(pThis), iIdx,
		  pThis->pWrkThrds[iIdx].tCurrCmd);
	pthread_join(pThis->pWrkThrds[iIdx].thrdID, NULL);
	qWrkrSetState(&pThis->pWrkThrds[iIdx], eWRKTHRD_STOPPED); /* back to virgin... */
	pThis->pWrkThrds[iIdx].thrdID = 0; /* invalidate the thread ID so that we do not accidently find reused ones */
	dbgprintf("Queue 0x%lx: thread %d state %d, has stopped\n", queueGetID(pThis), iIdx,
		  pThis->pWrkThrds[iIdx].tCurrCmd);

	return iRet;
}


/* Starts a worker thread (on a specific index [i]!)
 */
static inline rsRetVal
queueStrtWrkThrd(queue_t *pThis, int i)
{
	DEFiRet;
	int iState;

	ISOBJ_TYPE_assert(pThis, queue);
	assert(i >= 0 && i <= pThis->iNumWorkerThreads);
	assert(pThis->pWrkThrds[i].tCurrCmd < eWRKTHRD_RUN_CREATED);

	qWrkrSetState(&pThis->pWrkThrds[i], eWRKTHRD_RUN_CREATED);
	iState = pthread_create(&(pThis->pWrkThrds[i].thrdID), NULL, queueWorker, (void*) pThis);
	dbgprintf("Queue 0x%lx: starting Worker thread %x, index %d with state %d.\n",
		  (unsigned long) pThis, (unsigned) pThis->pWrkThrds[i].thrdID, i, iState);

	return iRet;
}


/* start the DA worker thread (if not already running)
 */
static inline rsRetVal
queueStrtDAWrkr(queue_t *pThis)
{
	DEFiRet;
	int iCancelStateSave;

	ISOBJ_TYPE_assert(pThis, queue);

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave);
	d_pthread_mutex_lock(&pThis->pWrkThrds[0].mut);
	if(pThis->pWrkThrds[0].tCurrCmd == eWRKTHRD_STOPPED) {
		iRet = queueStrtWrkThrd(pThis, 0);
	}
	d_pthread_mutex_unlock(&pThis->pWrkThrds[0].mut);
	pthread_setcancelstate(iCancelStateSave, NULL);

	return iRet;
}

/* Starts a *new* worker thread. Function searches itself for a free index spot. It must only
 * be called when we have less than max workers active. Pending wrkr thread requests MUST have
 * been processed before calling this function. -- rgerhards, 2008-01-16
 */
static inline rsRetVal
queueStrtNewWrkThrd(queue_t *pThis)
{
	DEFiRet;
	int i;
	int iStartingUp;
	int iState;

	ISOBJ_TYPE_assert(pThis, queue);

	/* find free spot in thread table. If we find at least one worker that is in initializiation,
	 * we do NOT start a new one. Let's give the other one a chance, first.
	 */
	iStartingUp = -1;
	for(i = 1 ; i <= pThis->iNumWorkerThreads ; ++i) {
dbgprintf("Queue %p: search thrd tbl slot: i %d, CuccCmd %d\n", pThis, i, pThis->pWrkThrds[i].tCurrCmd);
		if(pThis->pWrkThrds[i].tCurrCmd == eWRKTHRD_STOPPED) {
			break;
		} else if(pThis->pWrkThrds[i].tCurrCmd == eWRKTHRD_RUN_CREATED) {
			iStartingUp = i;
			break;
		}
	}

dbgprintf("Queue %p: after thrd search: i %d, iStartingUp %d\n", pThis, i, iStartingUp);
	if(iStartingUp > -1)
		ABORT_FINALIZE(RS_RET_ALREADY_STARTING);

	assert(i <= pThis->iNumWorkerThreads); /* now there must be a free spot, else something is really wrong! */

	qWrkrSetState(&pThis->pWrkThrds[i], eWRKTHRD_RUN_CREATED);
	iState = pthread_create(&(pThis->pWrkThrds[i].thrdID), NULL, queueWorker, (void*) pThis);
	dbgprintf("Queue 0x%lx: Worker thread %x, index %d started with state %d.\n",
		  (unsigned long) pThis, (unsigned) pThis->pWrkThrds[i].thrdID, i, iState);
	/* we try to give the starting worker a little boost. It won't help much as we still
 	 * hold the queue's mutex, but at least it has a chance to start on a single-CPU system.
 	 */
	pthread_yield();

finalize_it:
	return iRet;
}


/* send a command to all active worker threads. A start index can be
 * given. Usually, this is 0 or 1. Thread 0 is reserved to disk-assisted
 * mode and this start index take care of the special handling it needs to
 * receive. -- rgerhards, 2008-01-16
 */
static inline rsRetVal
queueTellActWrkThrds(queue_t *pThis, int iStartIdx, qWrkCmd_t tCmd)
{
	DEFiRet;
	int i;

	ISOBJ_TYPE_assert(pThis, queue);
	assert(iStartIdx == 0 || iStartIdx == 1);

	/* tell the workers our request */
	for(i = iStartIdx ; i <= pThis->iNumWorkerThreads ; ++i)
		if(pThis->pWrkThrds[i].tCurrCmd >= eWRKTHRD_TERMINATING)
			queueTellActWrkThrd(pThis, i, tCmd);

	return iRet;
}


/* compute an absolute time timeout suitable for calls to pthread_cond_timedwait()
 * rgerhards, 2008-01-14
 */
static rsRetVal
queueTimeoutComp(struct timespec *pt, int iTimeout)
{
	assert(pt != NULL);
	/* compute timeout */
	clock_gettime(CLOCK_REALTIME, pt);
	pt->tv_nsec += (iTimeout % 1000) * 1000000; /* think INTEGER arithmetic! */
	if(pt->tv_nsec > 999999999) { /* overrun? */
		pt->tv_nsec -= 1000000000;
		++pt->tv_sec;
	}
	pt->tv_sec += iTimeout / 1000;
	return RS_RET_OK; /* so far, this is static... */
}


/* wake up all worker threads. Param bWithDAWrk tells if the DA worker
 * is to be awaken, too. It needs special handling because it waits on
 * two different conditions depending on processing state.
 * rgerhards, 2008-01-16
 */
static inline rsRetVal
queueWakeupWrkThrds(queue_t *pThis, int bWithDAWrk)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, queue);

	pthread_cond_broadcast(&pThis->notEmpty);
	if(bWithDAWrk && pThis->qRunsDA != QRUNS_REGULAR) {
		/* if running disk-assisted, workers may wait on that condition, too */
		pthread_cond_broadcast(&pThis->condDA);
	}

	return iRet;
}


/* This function checks if (another) worker threads needs to be started. It
 * must be called while the caller holds a lock on the queue mutex. So it must not
 * do anything that either reaquires the mutex or forces somebody else to aquire
 * it (that would lead to a deadlock).
 * rgerhards, 2008-01-16
 */
static inline rsRetVal
queueChkAndStrtWrk(queue_t *pThis)
{
	DEFiRet;
	int iCancelStateSave;

	ISOBJ_TYPE_assert(pThis, queue);

	/* process any pending thread requests */
	queueChkWrkThrdChanges(pThis);

	if(pThis->bEnqOnly == 1)
		FINALIZE;	/* in enqueue-only mode we have no workers */

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave);
	d_pthread_mutex_lock(&pThis->mutThrdMgmt);
	pthread_cleanup_push(queueMutexCleanup, &pThis->mutThrdMgmt);
	pthread_setcancelstate(iCancelStateSave, NULL);
	/* check if we need to start up another worker */
	if(pThis->qRunsDA == QRUNS_REGULAR) {
		if(pThis->iCurNumWrkThrd < pThis->iNumWorkerThreads) {
dbgprintf("Queue %p: less than max workers are running, qsize %d, workers %d, qRunsDA: %d\n",
	   pThis, pThis->iQueueSize, pThis->iCurNumWrkThrd, pThis->qRunsDA);
			/* check if we satisfy the min nbr of messages per worker to start a new one */
			if(pThis->iCurNumWrkThrd == 0 ||
			   pThis->iQueueSize / pThis->iCurNumWrkThrd > pThis->iMinMsgsPerWrkr) {
				dbgprintf("Queue 0x%lx: high activity - starting additional worker thread.\n",
					  queueGetID(pThis));
				queueStrtNewWrkThrd(pThis);
			}
		}
	} else {
		if(pThis->iCurNumWrkThrd == 0 && pThis->bEnqOnly == 0) {
dbgprintf("Queue %p: DA worker is no longer running, restarting, qsize %d, workers %d, qRunsDA: %d\n",
	   pThis, pThis->iQueueSize, pThis->iCurNumWrkThrd, pThis->qRunsDA);
			/* DA worker has timed out and needs to be restarted */
			iRet = queueStrtDAWrkr(pThis);
		}
	}
	pthread_cleanup_pop(1);

finalize_it:
	return iRet;
}


/* --------------- code for disk-assisted (DA) queue modes -------------------- */


/* Destruct DA queue. This is the last part of DA-to-normal-mode
 * transistion. This is called asynchronously and some time quite a
 * while after the actual transistion. The key point is that we need to
 * do it at some later time, because we need to destruct the DA queue. That,
 * however, can not be done in a thread that has been signalled 
 * This is to be called when we revert back to our own queue.
 * rgerhards, 2008-01-15
 */
static inline rsRetVal
queueTurnOffDAMode(queue_t *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, queue);
	assert(pThis->qRunsDA != QRUNS_REGULAR);

	/* if we need to pull any data that we still need from the (child) disk queue,
	 * now would be the time to do so. At present, we do not need this, but I'd like to
	 * keep that comment if future need arises.
	 */

	/* we start at least one worker thread. If no new messages come in, this will
	 * be the only one for the time being. I am not yet sure if that is acceptable.
	 * To solve that issue, queueWorker () would need to check if it needs to fire
	 * up addtl ones. I am not yet sure if that is justified. After all, if no new
	 * messages come into the queue, we may be well off with a single worker.
	 * rgerhards, 2008-01-16
	 */
dbgprintf("Queue 0x%lx: disk-assistance being been turned off, bEnqOnly %d, bQueInDestr %d, NumWrkd %d\n",
	  queueGetID(pThis), 
	pThis->bEnqOnly,pThis->bQueueInDestruction,pThis->iCurNumWrkThrd);
	// TODO: think about this code - there is a race
	if(pThis->bEnqOnly == 0 && pThis->bQueueInDestruction == 0 && pThis->iCurNumWrkThrd < 2)
		queueStrtNewWrkThrd(pThis);
	pThis->qRunsDA = QRUNS_REGULAR; /* tell the world we are back in non-DA mode */

	/* we destruct the queue object, which will also shutdown the queue worker. As the queue is empty,
	 * this will be quick.
	 */
	queueDestruct(&pThis->pqDA); /* and now we are ready to destruct the DA queue */

	/* now free the remaining resources */
	pthread_mutex_destroy(&pThis->mutDA);
	pthread_cond_destroy(&pThis->condDA);

	queueTellActWrkThrd(pThis, 0, eWRKTHRD_SHUTDOWN_IMMEDIATE);/* finally, tell ourselves to shutdown */
	dbgprintf("Queue 0x%lx: disk-assistance has been turned off, disk queue was empty (iRet %d)\n",
		  queueGetID(pThis), iRet);

	return iRet;
}

/* check if we had any worker thread changes and, if so, act
 * on them. At a minimum, terminated threads are harvested (joined).
 * This function MUST NEVER block on the queue mutex!
 */
static rsRetVal
queueChkWrkThrdChanges(queue_t *pThis)
{
	DEFiRet;
	int i;

	ISOBJ_TYPE_assert(pThis, queue);

	if(pThis->bThrdStateChanged == 0)
		FINALIZE;

	/* go through all threads (including DA thread) */
	for(i = 0 ; i <= pThis->iNumWorkerThreads ; ++i) {
		switch(pThis->pWrkThrds[i].tCurrCmd) {
			case eWRKTHRD_TERMINATING:
				queueJoinWrkThrd(pThis, i);
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
	}

finalize_it:
	return iRet;
}


/* check if we run in disk-assisted mode and record that
 * setting for easy (and quick!) access in the future. This
 * function must only be called from constructors and only
 * from those that support disk-assisted modes (aka memory-
 * based queue drivers).
 * rgerhards, 2008-01-14
 */
static rsRetVal
queueChkIsDA(queue_t *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, queue);
	if(pThis->pszFilePrefix != NULL) {
		pThis->bIsDA = 1;
		dbgprintf("Queue 0x%lx: is disk-assisted, disk will be used on demand\n", queueGetID(pThis));
	} else {
		dbgprintf("Queue 0x%lx: is NOT disk-assisted\n", queueGetID(pThis));
	}

	return iRet;
}


/* Start disk-assisted queue mode. All internal settings are changed. This is supposed
 * to be called from the DA worker, which must have been started before. The most important
 * chore of this function is to create the DA queue object. If that function fails,
 * the DA worker should return with an appropriate state, which in turn should lead to
 * a re-set to non-DA mode in the Enq process. The queue mutex must be locked when this
 * function is called, else a race on pThis->qRunsDA may happen.
 * rgerhards, 2008-01-15
 */
static rsRetVal
queueStrtDA(queue_t *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, queue);

	/* set up sync objects */
	pthread_mutex_init(&pThis->mutDA, NULL);
	pthread_cond_init(&pThis->condDA, NULL);

	/* create message queue */
dbgprintf("Queue %p: queueSTrtDA pre child queue construct,\n", pThis);
	CHKiRet(queueConstruct(&pThis->pqDA, QUEUETYPE_DISK , 1, 0, pThis->pConsumer));

dbgprintf("Queue %p: queueSTrtDA after child queue construct, q %p\n", pThis, pThis->pqDA);
	/* as the created queue is the same object class, we take the
	 * liberty to access its properties directly.
	 */
	pThis->pqDA->condSignalOnEmpty = &pThis->condDA;
	pThis->pqDA->mutSignalOnEmpty = &pThis->mutDA;
	pThis->pqDA->condSignalOnEmpty2 = &pThis->notEmpty;
	pThis->pqDA->bSignalOnEmpty = 2;
	pThis->pqDA->pqParent = pThis;
dbgprintf("Queue %p: queueSTrtDA after assign\n", pThis);

	CHKiRet(queueSetMaxFileSize(pThis->pqDA, pThis->iMaxFileSize));
	CHKiRet(queueSetFilePrefix(pThis->pqDA, pThis->pszFilePrefix, pThis->lenFilePrefix));
	CHKiRet(queueSetiPersistUpdCnt(pThis->pqDA, pThis->iPersistUpdCnt));
	CHKiRet(queueSettoActShutdown(pThis->pqDA, pThis->toActShutdown));
	CHKiRet(queueSettoEnq(pThis->pqDA, pThis->toEnq));
dbgprintf("Queue %p: queueSTrtDA 10\n", pThis);
	CHKiRet(queueSetEnqOnly(pThis->pqDA, pThis->bDAEnqOnly));
dbgprintf("Queue %p: queueSTrtDA 15\n", pThis);
	CHKiRet(queueSetiHighWtrMrk(pThis->pqDA, 0));
dbgprintf("Queue %p: queueSTrtDA 20\n", pThis);
	CHKiRet(queueSetiDiscardMrk(pThis->pqDA, 0));
dbgprintf("Queue %p: queueSTrtDA 25\n", pThis);
	if(pThis->toQShutdown == 0) {
dbgprintf("Queue %p: queueSTrtDA 30a\n", pThis);
		CHKiRet(queueSettoQShutdown(pThis->pqDA, 0)); /* if the user really wants... */
	} else {
dbgprintf("Queue %p: queueSTrtDA 30b\n", pThis);
		/* we use the shortest possible shutdown (0 is endless!) because when we run on disk AND
		 * have an obviously large backlog, we can't finish it in any case. So there is no point
		 * in holding shutdown longer than necessary. -- rgerhards, 2008-01-15
		 */
		CHKiRet(queueSettoQShutdown(pThis->pqDA, 1));
	}

dbgprintf("Queue %p: queueSTrtDA pre start\n", pThis);
	iRet = queueStart(pThis->pqDA);
	/* file not found is expected, that means it is no previous QIF available */
	if(iRet != RS_RET_OK && iRet != RS_RET_FILE_NOT_FOUND)
		FINALIZE; /* something is wrong */

	/* tell our fellow workers to shut down
	 * NOTE: we do NOT join them by intension! If we did, we would hold draining
	 * the queue until some potentially long-running actions are finished. Having
	 * the ability to immediatly drain the queue was the primary intension of
	 * reserving worker thread 0 for DA queues. So if we would join the other
	 * workers, we would screw up and do against our design goal.
	 */
	CHKiRet(queueTellActWrkThrds(pThis, 1, eWRKTHRD_SHUTDOWN_IMMEDIATE));

	/* as we are right now starting DA mode because we are so busy, it is
	 * extremely unlikely that any worker is sleeping on empty queue. HOWEVER,
	 * we want to be on the safe side, and so we awake anyone that is waiting
	 * on one. So even if the scheduler plays badly with us, things should be
	 * quite well. -- rgerhards, 2008-01-15
	 */
	queueWakeupWrkThrds(pThis, 0); /* awake all workers, but not ourselves ;) */

	pThis->qRunsDA = QRUNS_DA;	/* we are now in DA mode! */

	dbgprintf("Queue 0x%lx: is now running in disk assisted mode, disk queue 0x%lx\n",
		  queueGetID(pThis), queueGetID(pThis->pqDA));

finalize_it:
	if(iRet != RS_RET_OK) {
		if(pThis->pqDA != NULL) {
			queueDestruct(&pThis->pqDA);
		}
		dbgprintf("Queue 0x%lx: error %d creating disk queue - giving up.\n",
		  	  queueGetID(pThis), iRet);
		pThis->bIsDA = 0;
	}

	return iRet;
}


/* initiate DA mode
 * param bEnqOnly tells if the disk queue is to be run in enqueue-only mode. This may
 * be needed during shutdown of memory queues which need to be persisted to disk.
 * rgerhards, 2008-01-16
 */
static inline rsRetVal
queueInitDA(queue_t *pThis, int bEnqOnly)
{
	DEFiRet;

	/* indicate we now run in DA mode - this is reset by the DA worker if it fails */
	pThis->qRunsDA = QRUNS_DA_INIT;
	pThis->bDAEnqOnly = bEnqOnly;

	/* now we must start our DA worker thread - it does the rest of the initialization
	 * In enqueue-only mode, we do not start any workers.
	 */
	if(pThis->bEnqOnly == 0)
		iRet = queueStrtDAWrkr(pThis);

	return iRet;
}


/* check if we need to start disk assisted mode and send some signals to
 * keep it running if we are already in it.
 * rgerhards, 2008-01-14
 */
static rsRetVal
queueChkStrtDA(queue_t *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, queue);

	/* if we do not hit the high water mark, we have nothing to do */
	if(pThis->iQueueSize != pThis->iHighWtrMrk)
		ABORT_FINALIZE(RS_RET_OK);

	if(pThis->qRunsDA != QRUNS_REGULAR) {
		/* then we need to signal that we are at the high water mark again. If that happens
		 * on our way down the queue, that doesn't matter, because then nobody is waiting
		 * on the condition variable.
		 */
		dbgprintf("Queue 0x%lx: %d entries - passed high water mark in DA mode, send notify\n",
			  queueGetID(pThis), pThis->iQueueSize);
		//pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave);
		// TODO: mutex call order check! this must aquire the queue mutex
		//d_pthread_mutex_lock(&pThis->mutDA);
		pthread_cond_signal(&pThis->condDA);
		//d_pthread_mutex_unlock(&pThis->mutDA);
		//pthread_setcancelstate(iCancelStateSave, NULL);
		queueChkWrkThrdChanges(pThis); /* the queue mode may have changed while we waited, so check! */
	}

	/* we need to re-check if we run disk-assisted, because that status may have changed
	 * in our high water mark processing.
	 */
	if(pThis->qRunsDA == QRUNS_REGULAR) {
		/* if we reach this point, we are NOT currently running in DA mode. */
		dbgprintf("Queue 0x%lx: %d entries - passed high water mark for disk-assisted mode, initiating...\n",
			  queueGetID(pThis), pThis->iQueueSize);

		queueInitDA(pThis, QUEUE_MODE_ENQDEQ); /* initiate DA mode */
	}

finalize_it:
	return iRet;
}


/* --------------- end code for disk-assisted queue modes -------------------- */


/* Now, we define type-specific handlers. The provide a generic functionality,
 * but for this specific type of queue. The mapping to these handlers happens during
 * queue construction. Later on, handlers are called by pointers present in the
 * queue instance object.
 */

/* -------------------- fixed array -------------------- */
static rsRetVal qConstructFixedArray(queue_t *pThis)
{
	DEFiRet;

	assert(pThis != NULL);

	if(pThis->iMaxQueueSize == 0)
		ABORT_FINALIZE(RS_RET_QSIZE_ZERO);

	if((pThis->tVars.farray.pBuf = malloc(sizeof(void *) * pThis->iMaxQueueSize)) == NULL) {
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}

	pThis->tVars.farray.head = 0;
	pThis->tVars.farray.tail = 0;

	queueChkIsDA(pThis);

finalize_it:
	return iRet;
}


static rsRetVal qDestructFixedArray(queue_t *pThis)
{
	DEFiRet;
	
	assert(pThis != NULL);

	if(pThis->tVars.farray.pBuf != NULL)
		free(pThis->tVars.farray.pBuf);

	return iRet;
}

static rsRetVal qAddFixedArray(queue_t *pThis, void* in)
{
	DEFiRet;

	assert(pThis != NULL);
	pThis->tVars.farray.pBuf[pThis->tVars.farray.tail] = in;
	pThis->tVars.farray.tail++;
	if (pThis->tVars.farray.tail == pThis->iMaxQueueSize)
		pThis->tVars.farray.tail = 0;

	return iRet;
}

static rsRetVal qDelFixedArray(queue_t *pThis, void **out)
{
	DEFiRet;

	assert(pThis != NULL);
	*out = (void*) pThis->tVars.farray.pBuf[pThis->tVars.farray.head];

	pThis->tVars.farray.head++;
	if (pThis->tVars.farray.head == pThis->iMaxQueueSize)
		pThis->tVars.farray.head = 0;

	return iRet;
}


/* -------------------- linked list  -------------------- */
static rsRetVal qConstructLinkedList(queue_t *pThis)
{
	DEFiRet;

	assert(pThis != NULL);

	pThis->tVars.linklist.pRoot = 0;
	pThis->tVars.linklist.pLast = 0;

	queueChkIsDA(pThis);

	return iRet;
}


static rsRetVal qDestructLinkedList(queue_t __attribute__((unused)) *pThis)
{
	DEFiRet;
	
	/* with the linked list type, there is nothing to do here. The
	 * reason is that the Destructor is only called after all entries
	 * have bene taken off the queue. In this case, there is nothing
	 * dynamic left with the linked list.
	 */

	return iRet;
}

static rsRetVal qAddLinkedList(queue_t *pThis, void* pUsr)
{
	DEFiRet;
	qLinkedList_t *pEntry;

	assert(pThis != NULL);
	if((pEntry = (qLinkedList_t*) malloc(sizeof(qLinkedList_t))) == NULL) {
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}

	pEntry->pNext = NULL;
	pEntry->pUsr = pUsr;

	if(pThis->tVars.linklist.pRoot == NULL) {
		pThis->tVars.linklist.pRoot = pThis->tVars.linklist.pLast = pEntry;
	} else {
		pThis->tVars.linklist.pLast->pNext = pEntry;
		pThis->tVars.linklist.pLast = pEntry;
	}

finalize_it:
	return iRet;
}

static rsRetVal qDelLinkedList(queue_t *pThis, void **ppUsr)
{
	DEFiRet;
	qLinkedList_t *pEntry;

	assert(pThis != NULL);
	assert(pThis->tVars.linklist.pRoot != NULL);
	
	pEntry = pThis->tVars.linklist.pRoot;
	*ppUsr = pEntry->pUsr;

	if(pThis->tVars.linklist.pRoot == pThis->tVars.linklist.pLast) {
		pThis->tVars.linklist.pRoot = NULL;
		pThis->tVars.linklist.pLast = NULL;
	} else {
		pThis->tVars.linklist.pRoot = pEntry->pNext;
	}
	free(pEntry);

	return iRet;
}


/* -------------------- disk  -------------------- */


static rsRetVal
queueLoadPersStrmInfoFixup(strm_t *pStrm, queue_t *pThis)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pStrm, strm);
	ISOBJ_TYPE_assert(pThis, queue);
	CHKiRet(strmSetDir(pStrm, glblGetWorkDir(), strlen((char*)glblGetWorkDir())));
finalize_it:
	return iRet;
}


/* This method checks if we have a QIF file for the current queue (no matter of
 * queue mode). Returns RS_RET_OK if we have a QIF file or an error status otherwise.
 * rgerhards, 2008-01-15
 */
static rsRetVal 
queueHaveQIF(queue_t *pThis)
{
	DEFiRet;
	uchar pszQIFNam[MAXFNAME];
	size_t lenQIFNam;
	struct stat stat_buf;

	ISOBJ_TYPE_assert(pThis, queue);

	if(pThis->pszFilePrefix == NULL)
		ABORT_FINALIZE(RS_RET_ERR); // TODO: change code!

	/* Construct file name */
	lenQIFNam = snprintf((char*)pszQIFNam, sizeof(pszQIFNam) / sizeof(uchar), "%s/%s.qi",
			     (char*) glblGetWorkDir(), (char*)pThis->pszFilePrefix);

	/* check if the file exists */
	if(stat((char*) pszQIFNam, &stat_buf) == -1) {
		if(errno == ENOENT) {
			dbgprintf("Queue 0x%lx: no .qi file found\n", queueGetID(pThis));
			ABORT_FINALIZE(RS_RET_FILE_NOT_FOUND);
		} else {
			dbgprintf("Queue 0x%lx: error %d trying to access .qi file\n", queueGetID(pThis), errno);
			ABORT_FINALIZE(RS_RET_IO_ERROR);
		}
	}
	/* If we reach this point, we have a .qi file */

finalize_it:
	return iRet;
}


/* The method loads the persistent queue information.
 * rgerhards, 2008-01-11
 */
static rsRetVal 
queueTryLoadPersistedInfo(queue_t *pThis)
{
	DEFiRet;
	strm_t *psQIF = NULL;
	uchar pszQIFNam[MAXFNAME];
	size_t lenQIFNam;
	struct stat stat_buf;

	ISOBJ_TYPE_assert(pThis, queue);

	/* Construct file name */
	lenQIFNam = snprintf((char*)pszQIFNam, sizeof(pszQIFNam) / sizeof(uchar), "%s/%s.qi",
			     (char*) glblGetWorkDir(), (char*)pThis->pszFilePrefix);

	/* check if the file exists */
	if(stat((char*) pszQIFNam, &stat_buf) == -1) {
		if(errno == ENOENT) {
			dbgprintf("Queue 0x%lx: clean startup, no .qi file found\n", queueGetID(pThis));
			ABORT_FINALIZE(RS_RET_FILE_NOT_FOUND);
		} else {
			dbgprintf("Queue 0x%lx: error %d trying to access .qi file\n", queueGetID(pThis), errno);
			ABORT_FINALIZE(RS_RET_IO_ERROR);
		}
	}

	/* If we reach this point, we have a .qi file */

	CHKiRet(strmConstruct(&psQIF));
	CHKiRet(strmSetDir(psQIF, glblGetWorkDir(), strlen((char*)glblGetWorkDir())));
	CHKiRet(strmSettOperationsMode(psQIF, STREAMMODE_READ));
	CHKiRet(strmSetsType(psQIF, STREAMTYPE_FILE_SINGLE));
	CHKiRet(strmSetFName(psQIF, pszQIFNam, lenQIFNam));
	CHKiRet(strmConstructFinalize(psQIF));

	/* first, we try to read the property bag for ourselfs */
	CHKiRet(objDeserializePropBag((obj_t*) pThis, psQIF));
	
	/* and now the stream objects (some order as when persisted!) */
	CHKiRet(objDeserialize(&pThis->tVars.disk.pWrite, OBJstrm, psQIF,
			       (rsRetVal(*)(obj_t*,void*))queueLoadPersStrmInfoFixup, pThis));
	CHKiRet(objDeserialize(&pThis->tVars.disk.pRead, OBJstrm, psQIF,
			       (rsRetVal(*)(obj_t*,void*))queueLoadPersStrmInfoFixup, pThis));

	CHKiRet(strmSeekCurrOffs(pThis->tVars.disk.pWrite));
	CHKiRet(strmSeekCurrOffs(pThis->tVars.disk.pRead));

	/* OK, we could successfully read the file, so we now can request that it be
	 * deleted when we are done with the persisted information.
	 */
	pThis->bNeedDelQIF = 1;

finalize_it:
	if(psQIF != NULL)
		strmDestruct(&psQIF);

	if(iRet != RS_RET_OK) {
		dbgprintf("Queue 0x%lx: error %d reading .qi file - can not read persisted info (if any)\n",
			  queueGetID(pThis), iRet);
	}

	return iRet;
}


/* disk queue constructor.
 * Note that we use a file limit of 10,000,000 files. That number should never pose a
 * problem. If so, I guess the user has a design issue... But of course, the code can
 * always be changed (though it would probably be more appropriate to increase the
 * allowed file size at this point - that should be a config setting...
 * rgerhards, 2008-01-10
 */
static rsRetVal qConstructDisk(queue_t *pThis)
{
	DEFiRet;
	int bRestarted = 0;

	assert(pThis != NULL);

	/* and now check if there is some persistent information that needs to be read in */
	iRet = queueTryLoadPersistedInfo(pThis);
	if(iRet == RS_RET_OK)
		bRestarted = 1;
	else if(iRet != RS_RET_FILE_NOT_FOUND)
			FINALIZE;

dbgprintf("qConstructDisk: bRestarted %d, iRet %d\n", bRestarted, iRet);
	if(bRestarted == 1) {
		;
	} else {
		CHKiRet(strmConstruct(&pThis->tVars.disk.pWrite));
		CHKiRet(strmSetDir(pThis->tVars.disk.pWrite, glblGetWorkDir(), strlen((char*)glblGetWorkDir())));
		CHKiRet(strmSetiMaxFiles(pThis->tVars.disk.pWrite, 10000000));
		CHKiRet(strmSettOperationsMode(pThis->tVars.disk.pWrite, STREAMMODE_WRITE));
		CHKiRet(strmSetsType(pThis->tVars.disk.pWrite, STREAMTYPE_FILE_CIRCULAR));
		CHKiRet(strmConstructFinalize(pThis->tVars.disk.pWrite));

		CHKiRet(strmConstruct(&pThis->tVars.disk.pRead));
		CHKiRet(strmSetbDeleteOnClose(pThis->tVars.disk.pRead, 1));
		CHKiRet(strmSetDir(pThis->tVars.disk.pRead, glblGetWorkDir(), strlen((char*)glblGetWorkDir())));
		CHKiRet(strmSetiMaxFiles(pThis->tVars.disk.pRead, 10000000));
		CHKiRet(strmSettOperationsMode(pThis->tVars.disk.pRead, STREAMMODE_READ));
		CHKiRet(strmSetsType(pThis->tVars.disk.pRead, STREAMTYPE_FILE_CIRCULAR));
		CHKiRet(strmConstructFinalize(pThis->tVars.disk.pRead));


		CHKiRet(strmSetFName(pThis->tVars.disk.pWrite, pThis->pszFilePrefix, pThis->lenFilePrefix));
		CHKiRet(strmSetFName(pThis->tVars.disk.pRead,  pThis->pszFilePrefix, pThis->lenFilePrefix));
	}

	/* now we set (and overwrite in case of a persisted restart) some parameters which
	 * should always reflect the current configuration variables. Be careful by doing so,
	 * for example file name generation must not be changed as that would break the
	 * ability to read existing queue files. -- rgerhards, 2008-01-12
	 */
CHKiRet(strmSetiMaxFileSize(pThis->tVars.disk.pWrite, pThis->iMaxFileSize));
CHKiRet(strmSetiMaxFileSize(pThis->tVars.disk.pRead, pThis->iMaxFileSize));

finalize_it:
	return iRet;
}


static rsRetVal qDestructDisk(queue_t *pThis)
{
	DEFiRet;
	
	assert(pThis != NULL);
	
	strmDestruct(&pThis->tVars.disk.pWrite);
	strmDestruct(&pThis->tVars.disk.pRead);

	if(pThis->pszSpoolDir != NULL)
		free(pThis->pszSpoolDir);

	return iRet;
}

static rsRetVal qAddDisk(queue_t *pThis, void* pUsr)
{
	DEFiRet;

	assert(pThis != NULL);

	CHKiRet((objSerialize(pUsr))(pUsr, pThis->tVars.disk.pWrite));
	CHKiRet(strmFlush(pThis->tVars.disk.pWrite));

finalize_it:
	return iRet;
}

static rsRetVal qDelDisk(queue_t *pThis, void **ppUsr)
{
	return objDeserialize(ppUsr, OBJMsg, pThis->tVars.disk.pRead, NULL, NULL);
}

/* -------------------- direct (no queueing) -------------------- */
static rsRetVal qConstructDirect(queue_t __attribute__((unused)) *pThis)
{
	return RS_RET_OK;
}


static rsRetVal qDestructDirect(queue_t __attribute__((unused)) *pThis)
{
	return RS_RET_OK;
}

static rsRetVal qAddDirect(queue_t *pThis, void* pUsr)
{
	DEFiRet;
	rsRetVal iRetLocal;

	assert(pThis != NULL);

	/* calling the consumer is quite different here than it is from a worker thread */
	iRetLocal = pThis->pConsumer(pUsr);
	if(iRetLocal != RS_RET_OK)
		dbgprintf("Queue 0x%lx: Consumer returned iRet %d\n",
			  queueGetID(pThis), iRetLocal);
	--pThis->iQueueSize; /* this is kind of a hack, but its the smartest thing we can do given
			      * the somewhat astonishing fact that this queue type does not actually
			      * queue anything ;)
			      */

	return iRet;
}

static rsRetVal qDelDirect(queue_t __attribute__((unused)) *pThis, __attribute__((unused)) void **out)
{
	return RS_RET_OK;
}


/* --------------- end type-specific handlers -------------------- */


/* generic code to add a queue entry */
static rsRetVal
queueAdd(queue_t *pThis, void *pUsr)
{
	DEFiRet;

	assert(pThis != NULL);
	CHKiRet(pThis->qAdd(pThis, pUsr));

	++pThis->iQueueSize;

	dbgprintf("Queue 0x%lx: entry added, size now %d entries\n", queueGetID(pThis), pThis->iQueueSize);

finalize_it:
	return iRet;
}


/* generic code to remove a queue entry */
static rsRetVal
queueDel(queue_t *pThis, void *pUsr)
{
	DEFiRet;

	assert(pThis != NULL);

	/* we do NOT abort if we encounter an error, because otherwise the queue
	 * will not be decremented, what will most probably result in an endless loop.
	 * If we decrement, however, we may lose a message. But that is better than
	 * losing the whole process because it loops... -- rgerhards, 2008-01-03
	 */
	iRet = pThis->qDel(pThis, pUsr);
	--pThis->iQueueSize;

	dbgprintf("Queue 0x%lx: entry deleted, state %d, size now %d entries\n",
		  queueGetID(pThis), iRet, pThis->iQueueSize);

	return iRet;
}


/* Send a shutdown command to all workers and awake them. This function
 * does NOT wait for them to terminate. Set bIncludeDAWRk to send the
 * termination command to the DA worker, too (else this does not happen).
 * rgerhards, 2008-01-16
 */
static inline rsRetVal
queueWrkThrdReqTrm(queue_t *pThis, qWrkCmd_t tShutdownCmd, int bIncludeDAWrk)
{
	DEFiRet;

	if(bIncludeDAWrk) {
		queueTellActWrkThrds(pThis, 0, tShutdownCmd);/* first tell the workers our request */
		queueWakeupWrkThrds(pThis, 1); /* awake all workers including DA-worker */
	} else {
		queueTellActWrkThrds(pThis, 1, tShutdownCmd);/* first tell the workers our request */
		queueWakeupWrkThrds(pThis, 0); /* awake all workers but not DA-worker */
	}

	return iRet;
}


/* Send a shutdown command to all workers and see if they terminate.
 * A timeout may be specified.
 * rgerhards, 2008-01-14
 */
static rsRetVal
queueWrkThrdTrm(queue_t *pThis, qWrkCmd_t tShutdownCmd, long iTimeout)
{
	DEFiRet;
	int bTimedOut;
	struct timespec t;
	int iCancelStateSave;

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

	return iRet;
}


/* Unconditionally cancel all running worker threads.
 * rgerhards, 2008-01-14
 */
static rsRetVal
queueWrkThrdCancel(queue_t *pThis)
{
	DEFiRet;
	int i;
	// TODO: we need to implement peek(), without it (today!) we lose one message upon
	// worker cancellation! -- rgerhards, 2008-01-14

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

	return iRet;
}


/* Worker thread management function carried out when the main
 * worker is about to terminate.
 */
static rsRetVal queueShutdownWorkers(queue_t *pThis)
{
	DEFiRet;
	int i;

	assert(pThis != NULL);

	dbgprintf("Queue 0x%lx: initiating worker thread shutdown sequence\n", (unsigned long) pThis);

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

	return iRet;
}

/* This is a special consumer to feed the disk-queue in disk-assited mode.
 * When active, our own queue more or less acts as a memory buffer to the disk.
 * So this consumer just needs to drain the memory queue and submit entries
 * to the disk queue. The disk queue will then call the actual consumer from
 * the app point of view (we chain two queues here).
 * rgerhards, 2008-01-14
 */
static inline rsRetVal
queueDAConsumer(queue_t *pThis, qWrkThrd_t *pWrkrInst)
{
	DEFiRet;
	int iCancelStateSave;

	ISOBJ_TYPE_assert(pThis, queue);
	assert(pThis->qRunsDA != QRUNS_REGULAR);
	ISOBJ_assert(pWrkrInst->pUsr);

dbgprintf("Queue %p/w%d: queueDAConsumer, queue size %d\n", pThis, pWrkrInst->iThrd, pThis->iQueueSize);/* dirty iQueueSize! */
	CHKiRet(queueEnqObj(pThis->pqDA, pWrkrInst->pUsr));

	/* We check if we reached the low water mark (but only if we are not in shutdown mode)
	 * Note that the child queue now in almost all cases is non-empty, because we just enqueued
	 * a message. Note that we need a quick check below to see if we are still in running state.
	 * If not, we do not go into the wait, because that's not a good thing to do. We do not
	 * do a full termination check, as this is done when we go back to the main worker loop.
	 * We need to re-aquire the queue mutex here, because we need to have a consistent
	 * access to the queue's admin data.
	 */
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave);
dbgprintf("pre mutex lock (think about CLEANUP!)\n");
	d_pthread_mutex_lock(pThis->mut);
	pthread_cleanup_push(queueMutexCleanup, pThis->mut);
	pthread_setcancelstate(iCancelStateSave, NULL);
dbgprintf("mutex locked (think about CLEANUP!)\n");
	if(pThis->iQueueSize <= pThis->iLowWtrMrk && pWrkrInst->tCurrCmd == eWRKTHRD_RUNNING) {
		dbgprintf("Queue 0x%lx/w%d: %d entries - passed low water mark in DA mode, sleeping\n",
			  queueGetID(pThis), pWrkrInst->iThrd, pThis->iQueueSize);
		/* wait for either passing the high water mark or the child disk queue drain */
		pthread_cond_wait(&pThis->condDA, pThis->mut);
	}
	pthread_cleanup_pop(1); /* release mutex in an atomic way via cleanup handler */

finalize_it:
dbgprintf("DAConsumer returns with iRet %d\n", iRet);
	return iRet;
}


/* This is a helper for queueWorker () it either calls the configured
 * consumer or the DA-consumer (if in disk-assisted mode). It is 
 * protected by the queue mutex, but MUST release it as soon as possible.
 * Most importantly, it must release it before the consumer is called.
 * rgerhards, 2008-01-14
 */
static inline rsRetVal
queueWorkerChkAndCallConsumer(queue_t *pThis, qWrkThrd_t *pWrkrInst, int iCancelStateSave)
{
	DEFiRet;
	rsRetVal iRetLocal;
	int iSeverity;
	int iQueueSize;
	void *pUsr;
	int qRunsDA;
	int iMyThrdIndx;

	ISOBJ_TYPE_assert(pThis, queue);
	assert(pWrkrInst != NULL);

	iMyThrdIndx = pWrkrInst->iThrd;

	/* dequeue element (still protected from mutex) */
	iRet = queueDel(pThis, &pUsr);
	queueChkPersist(pThis); // when we support peek(), we must do this down after the del!
	iQueueSize = pThis->iQueueSize; /* cache this for after mutex release */
	pWrkrInst->pUsr = pUsr; /* save it for the cancel cleanup handler */
	qRunsDA = pThis->qRunsDA;
	d_pthread_mutex_unlock(pThis->mut);
	pthread_cond_signal(&pThis->notFull);
	pthread_setcancelstate(iCancelStateSave, NULL);
	/* WE ARE NO LONGER PROTECTED FROM THE MUTEX */

	/* do actual processing (the lengthy part, runs in parallel)
	 * If we had a problem while dequeing, we do not call the consumer,
	 * but we otherwise ignore it. This is in the hopes that it will be
	 * self-healing. However, this is really not a good thing.
	 * rgerhards, 2008-01-03
	 */
	if(iRet != RS_RET_OK)
		FINALIZE;

	/* call consumer depending on queue mode (in DA mode, we have just one thread, so it can not change) */
	if(qRunsDA == QRUNS_DA) {
		queueDAConsumer(pThis, pWrkrInst);
	} else {
		/* we are running in normal, non-disk-assisted mode 
		 * do a quick check if we need to drain the queue. It is OK to use the cached
		 * iQueueSize here, because it does not hurt if it is slightly wrong.
		 */
		if(pThis->iDiscardMrk > 0 && iQueueSize >= pThis->iDiscardMrk) {
			iRetLocal = objGetSeverity(pUsr, &iSeverity);
			if(iRetLocal == RS_RET_OK && iSeverity >= pThis->iDiscardSeverity) {
				dbgprintf("Queue 0x%lx/w%d: dequeue/queue nearly full (%d entries), "
					  "discarded severity %d message\n",
					  queueGetID(pThis), iMyThrdIndx, iQueueSize, iSeverity);
				objDestruct(pUsr);
			}
		} else {
			dbgprintf("Queue 0x%lx/w%d: worker executes consumer...\n",
				   queueGetID(pThis), iMyThrdIndx);
			iRetLocal = pThis->pConsumer(pUsr);
			if(iRetLocal != RS_RET_OK) {
				dbgprintf("Queue 0x%lx/w%d: Consumer returned iRet %d\n",
					  queueGetID(pThis), iMyThrdIndx, iRetLocal);
			}
		}
	}

finalize_it:
	if(iRet != RS_RET_OK) {
		dbgprintf("Queue 0x%lx/w%d: error %d dequeueing element - ignoring, but strange things "
			  "may happen\n", queueGetID(pThis), iMyThrdIndx, iRet);
	}
dbgprintf("CallConsumer returns %d\n", iRet);
	return iRet;
}



/* cancellation cleanup handler for queueWorker ()
 * Updates admin structure and frees ressources.
 * rgerhards, 2008-01-16
 */
static void queueWorkerCancelCleanup(void *arg)
{
	qWrkThrd_t *pWrkrInst = (qWrkThrd_t*) arg;
	queue_t *pThis;
	int iCancelStateSave;

	assert(pWrkrInst != NULL);
	ISOBJ_TYPE_assert(pWrkrInst->pQueue, queue);
	pThis = pWrkrInst->pQueue;

	dbgprintf("Queue 0x%lx/w%d: cancelation cleanup handler called (NOT FULLY IMPLEMENTED, one msg lost!)\n",
		  queueGetID(pThis), pWrkrInst->iThrd);
	
	/* TODO: re-enqueue the data element! */

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave);
	d_pthread_mutex_lock(&pThis->mutThrdMgmt);
	qWrkrSetState(&pThis->pWrkThrds[pWrkrInst->iThrd], eWRKTHRD_TERMINATING);
	pThis->bThrdStateChanged = 1; /* indicate change, so harverster will be called */

	dbgprintf("Queue 0x%lx/w%d: thread CANCELED with %d entries left in queue, %d workers running.\n",
		  queueGetID(pThis), pWrkrInst->iThrd, pThis->iQueueSize, pThis->iCurNumWrkThrd - 1);

	pThis->iCurNumWrkThrd--;
	pthread_cond_signal(&pThis->condThrdTrm); /* activate anyone waiting on thread shutdown */
	d_pthread_mutex_unlock(&pThis->mutThrdMgmt);
	pthread_setcancelstate(iCancelStateSave, NULL);
}


/* This function is created to keep the code in queueWorker () short. Thus it
 * also does not abide to the usual calling conventions used in rsyslog. It is more
 * like a macro. Its sole purpose is to have a handy shortcut for the queue
 * termination condition. For the same reason, the calling parameters are a bit
 * more verbose than the need to be in theory. The reasoning is the Worker has
 * everything handy and so we do not need to access it from memory (OK, the
 * optimized would probably have created the same code, but why not do it
 * optimal right away...). The function returns 0 if the worker should terminate
 * and something else if it should continue to run.
 * rgerhards, 2008-01-18
 */
static inline int
queueWorkerRemainActive(queue_t *pThis, qWrkThrd_t *pWrkrInst)
{
	register int b; /* this is a boolean! */

	/* first check the usual termination condition that applies to all workers */
	b = (   (qWrkrGetState(pWrkrInst) == eWRKTHRD_RUNNING)
	     || ((qWrkrGetState(pWrkrInst) == eWRKTHRD_SHUTDOWN) && (pThis->iQueueSize > 0)));
dbgprintf("Queue %p/w%d: chk 1 pre empty queue, qsize %d (high wtr %d), cont run: %d, cmd %d, DA qsize %d\n", pThis,
	  pWrkrInst->iThrd,
	  pThis->iQueueSize, pThis->iHighWtrMrk, b, qWrkrGetState(pWrkrInst),
	  (pThis->pqDA == NULL) ? -1 : pThis->pqDA->iQueueSize);
	if(b && pWrkrInst->iThrd == 0 && pThis->qRunsDA == QRUNS_DA) {
		b = pThis->iQueueSize >= pThis->iHighWtrMrk || pThis->pqDA->iQueueSize != 0;
	}

dbgprintf("Queue %p/w%d: pre empty queue, qsize %d, cont run: %d\n", pThis, pWrkrInst->iThrd, pThis->iQueueSize, b);
	return b;
}


/* Each queue has at least one associated worker (consumer) thread. It will pull
 * the message from the queue and pass it to a user-defined function.
 * This function was provided on construction. It MUST be thread-safe.
 * Worker thread 0 is always reserved for disk-assisted mode (if the queue
 * is not DA, this worker will be dormant). All other workers are for
 * regular operations mode. Workers are started and stopped as need arises.
 * rgerhards, 2008-01-15
 */
static void *
queueWorker(void *arg)
{
	queue_t *pThis = (queue_t*) arg;
	sigset_t sigSet;
	struct timespec t;
	int iMyThrdIndx;	/* index for this thread in queue thread table */
	int iCancelStateSave;
	qWrkThrd_t *pWrkrInst; /* for cleanup handler */
	int bContinueRun;

	ISOBJ_TYPE_assert(pThis, queue);

	sigfillset(&sigSet);
	pthread_sigmask(SIG_BLOCK, &sigSet, NULL);

	/* do some one-time thread initialization */
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave);
	d_pthread_mutex_lock(&pThis->mutThrdMgmt);

	/* initialize our thread instance descriptor */
	qWrkrInit(&pWrkrInst, pThis);

	iMyThrdIndx = pWrkrInst->iThrd;

	dbgprintf("Queue 0x%lx/w%d: worker thread startup.\n", queueGetID(pThis), iMyThrdIndx);

dbgprintf("qRunsDA %d, check against %d\n", pThis->qRunsDA, QRUNS_DA);
	if((iMyThrdIndx == 0) && (pThis->qRunsDA != QRUNS_DA)) { /* are we the DA worker? */
		if(queueStrtDA(pThis) != RS_RET_OK) { /* then fully initialize the DA queue! */
			/* if we could not init the DA queue, we have nothing to do, so shut down. */
			queueTellActWrkThrd(pThis, 0, eWRKTHRD_SHUTDOWN_IMMEDIATE);
		}
	}
			
	/* finally change to RUNNING state. We need to check if we actually should still run,
	 * because someone may have requested us to shut down even before we got a chance to do
	 * our init. That would be a bad race... -- rgerhards, 2008-01-16
	 */
	pThis->iCurNumWrkThrd++;
	if(qWrkrGetState(pWrkrInst) == eWRKTHRD_RUN_INIT)
		qWrkrSetState(pWrkrInst, eWRKTHRD_RUNNING); /* we are running now! */

	pthread_cleanup_push(queueWorkerCancelCleanup, pWrkrInst);

	d_pthread_mutex_unlock(&pThis->mutThrdMgmt);
	pthread_setcancelstate(iCancelStateSave, NULL);
	/* end one-time stuff */

	/* now we have our identity, on to real processing */
	bContinueRun = 1; /* we need this variable, because we need to check the actual termination condition 
			   * while protected by mutex */
	while(bContinueRun) {
dbgprintf("Queue 0x%lx/w%d: start worker run, queue cmd currently %d\n", 
	  queueGetID(pThis), iMyThrdIndx, pThis->pWrkThrds[iMyThrdIndx].tCurrCmd);
		/* process any pending thread requests */
		queueChkWrkThrdChanges(pThis);

		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave);
		d_pthread_mutex_lock(pThis->mut);
dbgprintf("pthis 2: %p\n", pThis);

		if((bContinueRun = queueWorkerRemainActive(pThis, pWrkrInst)) == 0) {
dbgprintf("pthis 2a: %p\n", pThis);
			d_pthread_mutex_unlock(pThis->mut);
			pthread_setcancelstate(iCancelStateSave, NULL);
dbgprintf("pthis 2b: %p\n", pThis);
			continue; /* and break loop */
		}

		/* if we reach this point, we are still protected by the mutex */
dbgprintf("pthis 3: %p\n", pThis);

		if(pThis->iQueueSize == 0) {
			dbgprintf("Queue 0x%lx/w%d: queue EMPTY, waiting for next message.\n",
				  queueGetID(pThis), iMyThrdIndx);
			/* check if the parent DA worker is running and, if not, initiate it. Thanks
			 * to queueStrtDAWrkr (), we do not actually need to check (that routines does
			 * that for us, but we need to aquire the parent queue's mutex to call it.
			 */
			if(pThis->pqParent != NULL) {
			dbgprintf("Queue %p: pre start parent %p worker\n", pThis, pThis->pqParent);
				queueStrtDAWrkr(pThis->pqParent);
			}

			if(pThis->bSignalOnEmpty > 0) {
				/* we need to signal our parent queue that we are empty */
	dbgprintf("Queue %p/w%d: signal parent we are empty\n", pThis, iMyThrdIndx);
				pthread_cond_signal(pThis->condSignalOnEmpty);
	dbgprintf("Queue %p/w%d: signaling parent empty done\n", pThis, iMyThrdIndx);
			}
			if(pThis->bSignalOnEmpty > 1) {
				/* no mutex associated with this condition, it's just a try (but needed
				 * to wakeup a parent worker if e.g. the queue was restarted from disk) */
				pthread_cond_signal(pThis->condSignalOnEmpty2);
			}
			/* If we arrive here, we have the regular case, where we can safely assume that
			 * iQueueSize and tCmd have not changed since the while().
			 */
	dbgprintf("Queue %p/w%d: pre condwait ->notEmpty, worker shutdown %d\n", pThis, iMyThrdIndx, pThis->toWrkShutdown);
			/* DA worker and first worker never have an inactivity timeout */
			if(pWrkrInst->iThrd < 2 || pThis->toWrkShutdown == -1) {
		//xxx	if(pThis->toWrkShutdown == -1) {
	dbgprintf("worker never times out!\n");
				/* never shut down any started worker */
				pthread_cond_wait(&pThis->notEmpty, pThis->mut);
			} else {
				queueTimeoutComp(&t, pThis->toWrkShutdown);/* get absolute timeout */
				if(pthread_cond_timedwait(&pThis->notEmpty, pThis->mut, &t) != 0) {
					dbgprintf("Queue 0x%lx/w%d: inactivity timeout, worker terminating...\n",
						  queueGetID(pThis), iMyThrdIndx);
					/* we use SHUTDOWN (and not SHUTDOWN_IMMEDIATE) so that the worker
					 * does not terminate if in the mean time a new message arrived.
					 */
					qWrkrSetState(pWrkrInst, eWRKTHRD_SHUTDOWN);
				}
			}
	dbgprintf("Queue %p/w%d: post condwait ->notEmpty\n", pThis, iMyThrdIndx);
			d_pthread_mutex_unlock(pThis->mut);
			pthread_setcancelstate(iCancelStateSave, NULL);
			pthread_testcancel(); /* see big comment below */
			pthread_yield(); /* see big comment below */
			continue; /* request next iteration */
		}

		/* if we reach this point, we have a non-empty queue (and are still protected by mutex) */
		queueWorkerChkAndCallConsumer(pThis, pWrkrInst, iCancelStateSave);

		/* Now make sure we can get canceled - it is not specified if pthread_setcancelstate() is
		 * a cancellation point in itself. As we run most of the time without cancel enabled, I fear
		 * we may never get cancelled if we do not create a cancellation point ourselfs.
		 */
		pthread_testcancel();
		/* We now yield to give the other threads a chance to obtain the mutex. If we do not
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
		pthread_yield();
		if(Debug && (qWrkrGetState(pWrkrInst) == eWRKTHRD_SHUTDOWN) && pThis->iQueueSize > 0)
			dbgprintf("Queue 0x%lx/w%d: worker does not yet terminate because it still has "
				  " %d messages to process.\n", queueGetID(pThis), iMyThrdIndx, pThis->iQueueSize);
	}

	/* indicate termination */
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave);
dbgprintf("Queue %p: worker waiting for mutex\n", pThis);
	d_pthread_mutex_lock(&pThis->mutThrdMgmt);
	/* check if we are the DA worker and, if so, switch back to regular mode */
	if(pWrkrInst->iThrd == 0) {
		queueTurnOffDAMode(pThis);
	}
	pthread_cleanup_pop(0); /* remove cleanup handler */

	pThis->iCurNumWrkThrd--; /* one less ;) */
	/* if we ever need finalize_it, here would be the place for it! */
	if(qWrkrGetState(pWrkrInst) == eWRKTHRD_SHUTDOWN ||
	   qWrkrGetState(pWrkrInst) == eWRKTHRD_SHUTDOWN_IMMEDIATE ||
	   qWrkrGetState(pWrkrInst) == eWRKTHRD_RUN_INIT ||
	   qWrkrGetState(pWrkrInst) == eWRKTHRD_RUN_CREATED) {
	   	/* in shutdown case, we need to flag termination. All other commands
		 * have a meaning to the thread harvester, so we can not overwrite them
		 */
dbgprintf("Queue 0x%lx/w%d: setting termination state\n", queueGetID(pThis), iMyThrdIndx);
		qWrkrSetState(pWrkrInst, eWRKTHRD_TERMINATING);
	}
	pThis->bThrdStateChanged = 1; /* indicate change, so harverster will be called */
	pthread_cond_signal(&pThis->condThrdTrm); /* activate anyone waiting on thread shutdown */
	d_pthread_mutex_unlock(&pThis->mutThrdMgmt);
	pthread_setcancelstate(iCancelStateSave, NULL);

	pthread_exit(0);
}


/* Constructor for the queue object
 * This constructs the data structure, but does not yet start the queue. That
 * is done by queueStart(). The reason is that we want to give the caller a chance
 * to modify some parameters before the queue is actually started.
 */
rsRetVal queueConstruct(queue_t **ppThis, queueType_t qType, int iWorkerThreads,
		        int iMaxQueueSize, rsRetVal (*pConsumer)(void*))
{
	DEFiRet;
	queue_t *pThis;

	assert(ppThis != NULL);
	assert(pConsumer != NULL);
	assert(iWorkerThreads >= 0);

	if((pThis = (queue_t *)calloc(1, sizeof(queue_t))) == NULL) {
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}

	/* we have an object, so let's fill the properties */
	objConstructSetObjInfo(pThis);
	if((pThis->pszSpoolDir = (uchar*) strdup((char*)glblGetWorkDir())) == NULL)
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

	pThis->lenSpoolDir = strlen((char*)pThis->pszSpoolDir);
	pThis->iMaxFileSize = 1024 * 1024; /* default is 1 MiB */
	pThis->iQueueSize = 0;
	pThis->iMaxQueueSize = iMaxQueueSize;
	pThis->pConsumer = pConsumer;
	pThis->iNumWorkerThreads = iWorkerThreads;

	pThis->pszFilePrefix = NULL;
	pThis->qType = qType;

	/* set type-specific handlers and other very type-specific things (we can not totally hide it...) */
	switch(qType) {
		case QUEUETYPE_FIXED_ARRAY:
			pThis->qConstruct = qConstructFixedArray;
			pThis->qDestruct = qDestructFixedArray;
			pThis->qAdd = qAddFixedArray;
			pThis->qDel = qDelFixedArray;
			break;
		case QUEUETYPE_LINKEDLIST:
			pThis->qConstruct = qConstructLinkedList;
			pThis->qDestruct = qDestructLinkedList;
			pThis->qAdd = qAddLinkedList;
			pThis->qDel = qDelLinkedList;
			break;
		case QUEUETYPE_DISK:
			pThis->qConstruct = qConstructDisk;
			pThis->qDestruct = qDestructDisk;
			pThis->qAdd = qAddDisk;
			pThis->qDel = qDelDisk;
			/* special handling */
			pThis->iNumWorkerThreads = 1; /* we need exactly one worker */
			break;
		case QUEUETYPE_DIRECT:
			pThis->qConstruct = qConstructDirect;
			pThis->qDestruct = qDestructDirect;
			pThis->qAdd = qAddDirect;
			pThis->qDel = qDelDirect;
			break;
	}

finalize_it:
	OBJCONSTRUCT_CHECK_SUCCESS_AND_CLEANUP
	return iRet;
}


/* start up the queue - it must have been constructed and parameters defined
 * before.
 */
rsRetVal queueStart(queue_t *pThis) /* this is the ConstructionFinalizer */
{
	DEFiRet;
	rsRetVal iRetLocal;
	int bInitialized = 0; /* is queue already initialized? */
	int i;

	assert(pThis != NULL);

	/* finalize some initializations that could not yet be done because it is
	 * influenced by properties which might have been set after queueConstruct ()
	 */
	if(pThis->pqParent == NULL) {
dbgprintf("Queue %p: no parent, alloc mutex\n", pThis);
		pThis->mut = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
		pthread_mutex_init (pThis->mut, NULL);
	} else {
		/* child queue, we need to use parent's mutex */
		pThis->mut = pThis->pqParent->mut;
dbgprintf("Queue %p: I am child, use mutex %p\n", pThis, pThis->pqParent->mut);
	}

	pthread_mutex_init(&pThis->mutThrdMgmt, NULL);
	pthread_cond_init (&pThis->notFull, NULL);
	pthread_cond_init (&pThis->notEmpty, NULL);
dbgprintf("Queue %p: post mutexes, mut %p\n", pThis, pThis->mut);

	/* call type-specific constructor */
	CHKiRet(pThis->qConstruct(pThis)); /* this also sets bIsDA */

	dbgprintf("Queue 0x%lx: type %d, enq-only %d, disk assisted %d, maxFileSz %ld starting\n", queueGetID(pThis),
		  pThis->qType, pThis->bEnqOnly, pThis->bIsDA, pThis->iMaxFileSize);

	if(pThis->qType == QUEUETYPE_DIRECT)
		FINALIZE;	/* with direct queues, we are already finished... */

	/* initialize worker thread instances 
	 * TODO: move to separate function
	 */
	if((pThis->pWrkThrds = calloc(pThis->iNumWorkerThreads + 1, sizeof(qWrkThrd_t))) == NULL)
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	for(i = 0 ; i < pThis->iNumWorkerThreads + 1 ; ++i) {
		qWrkrConstructFinalize(&pThis->pWrkThrds[i], pThis, i);
	}

	if(pThis->bIsDA) {
		/* If we are disk-assisted, we need to check if there is a QIF file
		 * which we need to load. -- rgerhards, 2008-01-15
		 */
		iRetLocal = queueHaveQIF(pThis);
		if(iRetLocal == RS_RET_OK) {
			dbgprintf("Queue 0x%lx: on-disk queue present, needs to be reloaded\n",
				  queueGetID(pThis));

			queueInitDA(pThis, QUEUE_MODE_ENQDEQ); /* initiate DA mode */
			bInitialized = 1; /* we are done */
		} else {
			// TODO: use logerror? -- rgerhards, 2008-01-16
			dbgprintf("Queue 0x%lx: error %d trying to access on-disk queue files, starting without them. "
			          "Some data may be lost\n", queueGetID(pThis), iRetLocal);
		}
	}

	if(!bInitialized) {
		dbgprintf("Queue 0x%lx: queue starts up without (loading) any disk state\n", queueGetID(pThis));
		/* fire up the worker threads */
		if(pThis->bEnqOnly == 0)
			queueStrtNewWrkThrd(pThis);
		// TODO: preforked workers! queueStrtAllWrkThrds(pThis);
	}
	pThis->bQueueStarted = 1;

finalize_it:
dbgprintf("queueStart() exit, iret %d\n", iRet);
	return iRet;
}


/* persist the queue to disk. If we have something to persist, we first
 * save the information on the queue properties itself and then we call
 * the queue-type specific drivers.
 * rgerhards, 2008-01-10
 */
static rsRetVal queuePersist(queue_t *pThis)
{
	DEFiRet;
	strm_t *psQIF = NULL; /* Queue Info File */
	uchar pszQIFNam[MAXFNAME];
	size_t lenQIFNam;

	assert(pThis != NULL);

	if(pThis->qType != QUEUETYPE_DISK) {
		if(pThis->iQueueSize > 0)
			ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED); /* TODO: later... */
		else
			FINALIZE; /* if the queue is empty, we are happy and done... */
	}

	dbgprintf("Queue 0x%lx: persisting queue to disk, %d entries...\n", queueGetID(pThis), pThis->iQueueSize);
	/* Construct file name */
	lenQIFNam = snprintf((char*)pszQIFNam, sizeof(pszQIFNam) / sizeof(uchar), "%s/%s.qi",
		             (char*) glblGetWorkDir(), (char*)pThis->pszFilePrefix);

	if(pThis->iQueueSize == 0) {
		if(pThis->bNeedDelQIF) {
			unlink((char*)pszQIFNam);
			pThis->bNeedDelQIF = 0;
		}
		/* indicate spool file needs to be deleted */
		CHKiRet(strmSetbDeleteOnClose(pThis->tVars.disk.pRead, 1));
		FINALIZE; /* nothing left to do, so be happy */
	}

	CHKiRet(strmConstruct(&psQIF));
	CHKiRet(strmSetDir(psQIF, glblGetWorkDir(), strlen((char*)glblGetWorkDir())));
	CHKiRet(strmSettOperationsMode(psQIF, STREAMMODE_WRITE));
	CHKiRet(strmSetiAddtlOpenFlags(psQIF, O_TRUNC));
	CHKiRet(strmSetsType(psQIF, STREAMTYPE_FILE_SINGLE));
	CHKiRet(strmSetFName(psQIF, pszQIFNam, lenQIFNam));
	CHKiRet(strmConstructFinalize(psQIF));

	/* first, write the property bag for ourselfs
	 * And, surprisingly enough, we currently need to persist only the size of the
	 * queue. All the rest is re-created with then-current config parameters when the
	 * queue is re-created. Well, we'll also save the current queue type, just so that
	 * we know when somebody has changed the queue type... -- rgerhards, 2008-01-11
	 */
	CHKiRet(objBeginSerializePropBag(psQIF, (obj_t*) pThis));
	objSerializeSCALAR(psQIF, iQueueSize, INT);
	CHKiRet(objEndSerialize(psQIF));

	/* this is disk specific and must be moved to a function */
	CHKiRet(strmSerialize(pThis->tVars.disk.pWrite, psQIF));
	CHKiRet(strmSerialize(pThis->tVars.disk.pRead, psQIF));
	
	/* persist queue object itself */

	/* tell the input file object that it must not delete the file on close if the queue is non-empty */
	CHKiRet(strmSetbDeleteOnClose(pThis->tVars.disk.pRead, 0));

	/* we have persisted the queue object. So whenever it comes to an empty queue,
	 * we need to delete the QIF. Thus, we indicte that need.
	 */
	pThis->bNeedDelQIF = 1;

finalize_it:
	if(psQIF != NULL)
		strmDestruct(&psQIF);

	return iRet;
}


/* check if we need to persist the current queue info. If an
 * error occurs, thus should be ignored by caller (but we still
 * abide to our regular call interface)...
 * rgerhards, 2008-01-13
 */
rsRetVal queueChkPersist(queue_t *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, queue);

	if(pThis->iPersistUpdCnt && ++pThis->iUpdsSincePersist >= pThis->iPersistUpdCnt) {
		queuePersist(pThis);
		pThis->iUpdsSincePersist = 0;
	}

	return iRet;
}


/* destructor for the queue object */
rsRetVal queueDestruct(queue_t **ppThis)
{
	queue_t *pThis;
	DEFiRet;

	assert(ppThis != NULL);
	pThis = *ppThis;
	ISOBJ_TYPE_assert(pThis, queue);

pThis->bSaveOnShutdown = 1; // TODO: Test remove

	pThis->bQueueInDestruction = 1; /* indicate we are in destruction (modifies some behaviour) */

	/* we do not need to take care of any messages left in queue if we are in enqueue only mode */
	if(!pThis->bEnqOnly) {
		/* in regular mode, need look at termination */
		/* optimize parameters for shutdown of DA-enabled queues */
		if(pThis->bIsDA && pThis->iQueueSize > 0) { // TODO: atomic iQueueSize!
	dbgprintf("IsDA queue, modifying params for draining\n");
			pThis->iHighWtrMrk = 1; /* make sure we drain */
			pThis->iLowWtrMrk = 0; /* disable low water mark algo */
			if(pThis->qRunsDA == QRUNS_REGULAR) {
				if(pThis->iQueueSize > 0) {
					queueInitDA(pThis, QUEUE_MODE_ENQONLY); /* initiate DA mode */
				}
			} else {
				queueSetEnqOnly(pThis->pqDA, QUEUE_MODE_ENQONLY); /* turn on enqueue-only mode */
			}
			if(pThis->bSaveOnShutdown) {
	dbgprintf("bSaveOnShutdown set, eternal timeout set\n");
				pThis->toQShutdown = QUEUE_TIMEOUT_ETERNAL;
			}
			/* now we need to activate workers (read doc/dev_queue.html) */
		}

		/* wait until all pending workers are started up */
		qWrkrWaitAllWrkrStartup(pThis);

		// We need to startup a worker if we are in non-DA mode and the queue is not empty and not in enque-only mode */
	dbgprintf("Queue %p: queueDestruct probing if any regular workers need to be started, CurWrkr %d, qsize %d, qRunsDA %d\n",
		  pThis, pThis->iCurNumWrkThrd, pThis->iQueueSize, pThis->qRunsDA);
		d_pthread_mutex_lock(pThis->mut);
	dbgprintf("queueDestruct mutex locked\n");
		if(pThis->iCurNumWrkThrd == 0 && pThis->iQueueSize > 0 && !pThis->bEnqOnly) {
	dbgprintf("Queue %p: queueDestruct must start regular workers!\n", pThis);
	// TODO check mutex call order - doies function aquire mutex?
			queueStrtNewWrkThrd(pThis);
		}
		d_pthread_mutex_unlock(pThis->mut);
	dbgprintf("queueDestruct mutex unlocked\n");

		/* wait again in case a new worker was started */
		qWrkrWaitAllWrkrStartup(pThis);
	}

	/* terminate our own worker threads */
	if(pThis->pWrkThrds != NULL) {
		queueShutdownWorkers(pThis);
	}

	/* if still running DA, terminate disk queue */
	if(pThis->qRunsDA != QRUNS_REGULAR)
		queueDestruct(&pThis->pqDA);

	/* persist the queue (we always do that - queuePersits() does cleanup if the queue is empty) */
	CHKiRet_Hdlr(queuePersist(pThis)) {
		dbgprintf("Queue 0x%lx: error %d persisting queue - data lost!\n", (unsigned long) pThis, iRet);
	}

	/* ... then free resources */
	if(pThis->pWrkThrds != NULL) {
		free(pThis->pWrkThrds);
		pThis->pWrkThrds = NULL;
	}

	if(pThis->pqParent == NULL) {
		/* if we are not a child, we allocated our own mutex, which we now need to destroy */
		pthread_mutex_destroy(pThis->mut);
		free(pThis->mut);
	}
	pthread_mutex_destroy(&pThis->mutThrdMgmt);
	pthread_cond_destroy(&pThis->notFull);
	pthread_cond_destroy(&pThis->notEmpty);
	/* type-specific destructor */
	iRet = pThis->qDestruct(pThis);

	if(pThis->pszFilePrefix != NULL)
		free(pThis->pszFilePrefix);

	/* and finally delete the queue objet itself */
	free(pThis);
	*ppThis = NULL;

	return iRet;
}


/* set the queue's file prefix
 * The passed-in string is duplicated. So if the caller does not need
 * it any longer, it must free it.
 * rgerhards, 2008-01-09
 */
rsRetVal
queueSetFilePrefix(queue_t *pThis, uchar *pszPrefix, size_t iLenPrefix)
{
	DEFiRet;

	if(pThis->pszFilePrefix != NULL)
		free(pThis->pszFilePrefix);

	if(pszPrefix == NULL) /* just unset the prefix! */
		ABORT_FINALIZE(RS_RET_OK);

	if((pThis->pszFilePrefix = malloc(sizeof(uchar) * iLenPrefix + 1)) == NULL)
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	memcpy(pThis->pszFilePrefix, pszPrefix, iLenPrefix + 1);
	pThis->lenFilePrefix = iLenPrefix;

finalize_it:
	return iRet;
}

/* set the queue's maximum file size
 * rgerhards, 2008-01-09
 */
rsRetVal
queueSetMaxFileSize(queue_t *pThis, size_t iMaxFileSize)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, queue);
	
	if(iMaxFileSize < 1024) {
		ABORT_FINALIZE(RS_RET_VALUE_TOO_LOW);
	}

	pThis->iMaxFileSize = iMaxFileSize;

finalize_it:
	return iRet;
}


/* enqueue a new user data element
 * Enqueues the new element and awakes worker thread.
 * TODO: this code still uses the "discard if queue full" approach from
 * the main queue. This needs to be reconsidered or, better, done via a
 * caller-selectable parameter mode. For the time being, I leave it in.
 * rgerhards, 2008-01-03
 */
rsRetVal
queueEnqObj(queue_t *pThis, void *pUsr)
{
	DEFiRet;
	int iCancelStateSave;
	int i;
	struct timespec t;
	int iSeverity = 8;
	rsRetVal iRetLocal;

	ISOBJ_TYPE_assert(pThis, queue);

	/* process any pending thread requests */
	queueChkWrkThrdChanges(pThis);

	/* Please note that this function is not cancel-safe and consequently
	 * sets the calling thread's cancelibility state to PTHREAD_CANCEL_DISABLE
	 * during its execution. If that is not done, race conditions occur if the
	 * thread is canceled (most important use case is input module termination).
	 * rgerhards, 2008-01-08
	 */
	if(pThis->pWrkThrds != NULL) {
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave);
		d_pthread_mutex_lock(pThis->mut);
	}

	/* first check if we can discard anything */
	if(pThis->iDiscardMrk > 0 && pThis->iQueueSize >= pThis->iDiscardMrk) {
		iRetLocal = objGetSeverity(pUsr, &iSeverity);
		if(iRetLocal == RS_RET_OK && iSeverity >= pThis->iDiscardSeverity) {
			dbgprintf("Queue 0x%lx: queue nearly full (%d entries), discarded severity %d message\n",
				  queueGetID(pThis), pThis->iQueueSize, iSeverity);
			objDestruct(pUsr);
			ABORT_FINALIZE(RS_RET_QUEUE_FULL);
		} else {
			dbgprintf("Queue 0x%lx: queue nearly full (%d entries), but could not drop msg "
				  "(iRet: %d, severity %d)\n", queueGetID(pThis), pThis->iQueueSize,
				  iRetLocal, iSeverity);
		}
	}

	/* then check if we need to add an assistance disk queue */
	if(pThis->bIsDA)
		CHKiRet(queueChkStrtDA(pThis));
	
	/* re-process any new pending thread requests and see if we need to start workers */
	queueChkAndStrtWrk(pThis);

	/* and finally (try to) enqueue what is left over */
	while(pThis->iMaxQueueSize > 0 && pThis->iQueueSize >= pThis->iMaxQueueSize) {
		dbgprintf("Queue 0x%lx: enqueueMsg: queue FULL - waiting to drain.\n", queueGetID(pThis));
		queueTimeoutComp(&t, pThis->toEnq);
		if(pthread_cond_timedwait(&pThis->notFull, pThis->mut, &t) != 0) {
			dbgprintf("Queue 0x%lx: enqueueMsg: cond timeout, dropping message!\n", queueGetID(pThis));
			objDestruct(pUsr);
			ABORT_FINALIZE(RS_RET_QUEUE_FULL);
		}
	}
	CHKiRet(queueAdd(pThis, pUsr));
	queueChkPersist(pThis);

finalize_it:
	/* now awake sleeping worker threads */
	if(pThis->pWrkThrds != NULL) {
		d_pthread_mutex_unlock(pThis->mut);
		i = pthread_cond_signal(&pThis->notEmpty);
		dbgprintf("Queue 0x%lx: EnqueueMsg signaled condition (%d)\n", (unsigned long) pThis, i);
		pthread_setcancelstate(iCancelStateSave, NULL);
	}


	return iRet;
}


/* set queue mode to enqueue only or not
 * There is one subtle issue: this method may be called during queue
 * construction or while it is running. In the former case, the queue 
 * mutex does not yet exist (it is NULL), while in the later case it
 * must be locked. The function detects the state and operates as 
 * required.
 * rgerhards, 2008-01-16
 */
static rsRetVal
queueSetEnqOnly(queue_t *pThis, int bEnqOnly)
{
	DEFiRet;
	int iCancelStateSave;

	ISOBJ_TYPE_assert(pThis, queue);

	/* for simplicity, we do one big mutex lock. This method is extremely seldom
	 * called, so that doesn't matter... -- rgerhards, 2008-01-16
	 */
	if(pThis->mut != NULL) {
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave);
		d_pthread_mutex_lock(pThis->mut);
	}

	if(bEnqOnly == pThis->bEnqOnly)
		FINALIZE; /* no change, nothing to do */

	if(pThis->bQueueStarted) {
		/* we need to adjust queue operation only if we are not during initial param setup */
		if(bEnqOnly == 1) {
			/* switch to enqueue-only mode */
			/* this means we need to terminate all workers - that's it... */
			dbgprintf("Queue 0x%lx: switching to enqueue-only mode, terminating all worker threads\n",
				   queueGetID(pThis));
			queueWrkThrdReqTrm(pThis, eWRKTHRD_SHUTDOWN_IMMEDIATE, 0);
		} else {
			/* switch back to regular mode */
			ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED); /* we don't need this so far... */
		}
	}

	pThis->bEnqOnly = bEnqOnly;

finalize_it:
	if(pThis->mut != NULL) {
		d_pthread_mutex_unlock(pThis->mut);
		pthread_setcancelstate(iCancelStateSave, NULL);
	}
	return iRet;
}


/* some simple object access methods */
DEFpropSetMeth(queue, iPersistUpdCnt, int);
DEFpropSetMeth(queue, toQShutdown, long);
DEFpropSetMeth(queue, toActShutdown, long);
DEFpropSetMeth(queue, toWrkShutdown, long);
DEFpropSetMeth(queue, toEnq, long);
DEFpropSetMeth(queue, iHighWtrMrk, int);
DEFpropSetMeth(queue, iLowWtrMrk, int);
DEFpropSetMeth(queue, iDiscardMrk, int);
DEFpropSetMeth(queue, iDiscardSeverity, int);
DEFpropSetMeth(queue, bIsDA, int);
DEFpropSetMeth(queue, iMinMsgsPerWrkr, int);


/* This function can be used as a generic way to set properties. Only the subset
 * of properties required to read persisted property bags is supported. This
 * functions shall only be called by the property bag reader, thus it is static.
 * rgerhards, 2008-01-11
 */
#define isProp(name) !rsCStrSzStrCmp(pProp->pcsName, (uchar*) name, sizeof(name) - 1)
static rsRetVal queueSetProperty(queue_t *pThis, property_t *pProp)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, queue);
	assert(pProp != NULL);

 	if(isProp("iQueueSize")) {
		pThis->iQueueSize = pProp->val.vInt;
 	} else if(isProp("qType")) {
		if(pThis->qType != pProp->val.vLong)
			ABORT_FINALIZE(RS_RET_QTYPE_MISMATCH);
	}

finalize_it:
	return iRet;
}
#undef	isProp


/* Initialize the stream class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-01-09
 */
BEGINObjClassInit(queue, 1)
	//OBJSetMethodHandler(objMethod_SERIALIZE, strmSerialize);
	OBJSetMethodHandler(objMethod_SETPROPERTY, queueSetProperty);
	//OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, strmConstructFinalize);
ENDObjClassInit(queue)

/*
 * vi:set ai:
 */
