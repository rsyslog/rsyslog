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
 * NOTE: as of 2009-04-22, I have begin to remove the qqueue* prefix from static
 * function names - this makes it really hard to read and does not provide much
 * benefit, at least I (now) think so...
 *
 * Copyright 2008, 2009 Rainer Gerhards and Adiscon GmbH.
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
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>	 /* required for HP UX */
#include <time.h>
#include <errno.h>

#include "rsyslog.h"
#include "queue.h"
#include "stringbuf.h"
#include "srUtils.h"
#include "obj.h"
#include "wtp.h"
#include "wti.h"
#include "atomic.h"

#ifdef OS_SOLARIS
#	include <sched.h>
#	define pthread_yield() sched_yield()
#endif

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(glbl)

/* forward-definitions */
static rsRetVal qqueueChkPersist(qqueue_t *pThis, int nUpdates);
static rsRetVal qqueueSetEnqOnly(qqueue_t *pThis, int bEnqOnly, int bLockMutex);
static rsRetVal RateLimiter(qqueue_t *pThis);
static int qqueueChkStopWrkrDA(qqueue_t *pThis);
static rsRetVal GetDeqBatchSize(qqueue_t *pThis, int *pVal);
static int qqueueIsIdleDA(qqueue_t *pThis);
static rsRetVal ConsumerDA(qqueue_t *pThis, wti_t *pWti, int iCancelStateSave);
static rsRetVal ConsumerCancelCleanup(void *arg1, void *arg2);

/* some constants for queuePersist () */
#define QUEUE_CHECKPOINT	1
#define QUEUE_NO_CHECKPOINT	0

/***********************************************************************
 * we need a private data structure, the "to-delete" list. As C does
 * not provide any partly private data structures, we implement this
 * structure right here inside the module.
 * Note that this list must always be kept sorted based on a unique
 * dequeue ID (which is monotonically increasing).
 * rgerhards, 2009-05-18
 ***********************************************************************/

/* generate next uniqueue dequeue ID. Note that uniqueness is only required
 * on a per-queue basis and while this instance runs. So a stricly monotonically
 * increasing counter is sufficient (if enough bits are used).
 */
static inline qDeqID getNextDeqID(qqueue_t *pQueue)
{
	ISOBJ_TYPE_assert(pQueue, qqueue);
	return pQueue->deqIDAdd++;
}


/* return the top element of the to-delete list or NULL, if the
 * list is empty.
 */
static inline toDeleteLst_t *tdlPeek(qqueue_t *pQueue)
{
	ISOBJ_TYPE_assert(pQueue, qqueue);
	return pQueue->toDeleteLst;
}


/* remove the top element of the to-delete list. Nothing but the
 * element itself is destroyed. Must not be called when the list
 * is empty.
 */
static inline rsRetVal tdlPop(qqueue_t *pQueue)
{
	toDeleteLst_t *pRemove;
	DEFiRet;

	ISOBJ_TYPE_assert(pQueue, qqueue);
	assert(pQueue->toDeleteLst != NULL);

	pRemove = pQueue->toDeleteLst;
	pQueue->toDeleteLst = pQueue->toDeleteLst->pNext;
	free(pRemove);

	RETiRet;
}


/* Add a new to-delete list entry. The function allocates the data
 * structure, populates it with the values provided and links the new
 * element into the correct place inside the list.
 */
static inline rsRetVal tdlAdd(qqueue_t *pQueue, qDeqID deqID, int nElem)
{
	toDeleteLst_t *pNew;
	toDeleteLst_t *pPrev;
	DEFiRet;

	ISOBJ_TYPE_assert(pQueue, qqueue);
	assert(pQueue->toDeleteLst != NULL);

	CHKmalloc(pNew = malloc(sizeof(toDeleteLst_t)));
	pNew->deqID = deqID;
	pNew->nElem = nElem;

	/* now find right spot */
	for(  pPrev = pQueue->toDeleteLst
	    ; pPrev != NULL && deqID > pPrev->deqID
	    ; pPrev = pPrev->pNext) {
		/*JUST SEARCH*/;
	}

	if(pPrev == NULL) {
		pNew->pNext = pQueue->toDeleteLst;
		pQueue->toDeleteLst = pNew;
	} else {
		pNew->pNext = pPrev->pNext;
		pPrev->pNext = pNew;
	}

finalize_it:
	RETiRet;
}


/* methods */


/* get the physical queue size. Must only be called
 * while mutex is locked!
 * rgerhards, 2008-01-29
 */
static inline int
getPhysicalQueueSize(qqueue_t *pThis)
{
	return pThis->iQueueSize;
}


/* get the logical queue size (that is store size minus logically dequeued elements).
 * Must only be called while mutex is locked!
 * rgerhards, 2009-05-19
 */
static inline int
getLogicalQueueSize(qqueue_t *pThis)
{
	return pThis->iQueueSize - pThis->nLogDeq;
}



/* This function drains the queue in cases where this needs to be done. The most probable
 * reason is a HUP which needs to discard data (because the queue is configured to be lossy).
 * During a shutdown, this is typically not needed, as the OS frees up ressources and does
 * this much quicker than when we clean up ourselvs. -- rgerhards, 2008-10-21
 * This function returns void, as it makes no sense to communicate an error back, even if
 * it happens.
 * This functions works "around" the regular deque mechanism, because it is only used to
 * clean up (in cases where message loss is acceptable). 
 */
static inline void queueDrain(qqueue_t *pThis)
{
	void *pUsr;
	
	ASSERT(pThis != NULL);

// TODO: ULTRA it may be a good idea to check validitity once again
	/* iQueueSize is not decremented by qDel(), so we need to do it ourselves */
	while(pThis->iQueueSize-- > 0) {
		pThis->qDeq(pThis, &pUsr);
		if(pUsr != NULL) {
			objDestruct(pUsr);
		}
		pThis->qDel(pThis);
	}
}


/* --------------- code for disk-assisted (DA) queue modes -------------------- */


/* returns the number of workers that should be advised at
 * this point in time. The mutex must be locked when
 * ths function is called. -- rgerhards, 2008-01-25
 */
static inline rsRetVal qqueueAdviseMaxWorkers(qqueue_t *pThis)
{
	DEFiRet;
	int iMaxWorkers;

	ISOBJ_TYPE_assert(pThis, qqueue);

	if(!pThis->bEnqOnly) {
		if(pThis->bRunsDA) {
			/* if we have not yet reached the high water mark, there is no need to start a
			 * worker. -- rgerhards, 2008-01-26
			 */
			if(getLogicalQueueSize(pThis) >= pThis->iHighWtrMrk || pThis->bQueueStarted == 0) {
				wtpAdviseMaxWorkers(pThis->pWtpDA, 1); /* disk queues have always one worker */
			}
		} else {
			if(pThis->qType == QUEUETYPE_DISK || pThis->iMinMsgsPerWrkr == 0) {
				iMaxWorkers = 1;
			} else {
				iMaxWorkers = getLogicalQueueSize(pThis) / pThis->iMinMsgsPerWrkr + 1;
			}
			wtpAdviseMaxWorkers(pThis->pWtpReg, iMaxWorkers); /* disk queues have always one worker */
		}
	}

	RETiRet;
}


/* wait until we have a fully initialized DA queue. Sometimes, we need to
 * sync with it, as we expect it for some function.
 * rgerhards, 2008-02-27
 */
static rsRetVal
qqueueWaitDAModeInitialized(qqueue_t *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, qqueue);
	ASSERT(pThis->bRunsDA);

	while(pThis->bRunsDA != 2) {
		d_pthread_cond_wait(&pThis->condDAReady, pThis->mut);
	}

	RETiRet;
}


/* Destruct DA queue. This is the last part of DA-to-normal-mode
 * transistion. This is called asynchronously and some time quite a
 * while after the actual transistion. The key point is that we need to
 * do it at some later time, because we need to destruct the DA queue. That,
 * however, can not be done in a thread that has been signalled 
 * This is to be called when we revert back to our own queue.
 * This function must be called with the queue mutex locked (the wti
 * class ensures this).
 * rgerhards, 2008-01-15
 */
static rsRetVal
qqueueTurnOffDAMode(qqueue_t *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, qqueue);
	ASSERT(pThis->bRunsDA);

	/* at this point, we need a fully initialized DA queue. So if it isn't, we finally need
	 * to wait for its startup... -- rgerhards, 2008-01-25
	 */
	qqueueWaitDAModeInitialized(pThis);

	/* if we need to pull any data that we still need from the (child) disk queue,
	 * now would be the time to do so. At present, we do not need this, but I'd like to
	 * keep that comment if future need arises.
	 */

	/* we need to check if the DA queue is empty because the DA worker may simply have
	 * terminated due to no new messages arriving. That does not, however, mean that the
	 * DA queue is empty. If there is still data in that queue, we do nothing and leave
	 * that for a later incarnation of this function (it will be called multiple times
	 * during the lifetime of DA-mode, depending on how often the DA worker receives an
	 * inactivity timeout. -- rgerhards, 2008-01-25
	 */
	if(getLogicalQueueSize(pThis->pqDA) == 0) {
		pThis->bRunsDA = 0; /* tell the world we are back in non-DA mode */
		/* we destruct the queue object, which will also shutdown the queue worker. As the queue is empty,
		 * this will be quick.
		 */
		qqueueDestruct(&pThis->pqDA); /* and now we are ready to destruct the DA queue */
		dbgoprint((obj_t*) pThis, "disk-assistance has been turned off, disk queue was empty (iRet %d)\n",
			  iRet);
		/* now we need to check if the regular queue has some messages. This may be the case
		 * when it is waiting that the high water mark is reached again. If so, we need to start up
		 * a regular worker. -- rgerhards, 2008-01-26
		 */
		if(getLogicalQueueSize(pThis) > 0) {
			qqueueAdviseMaxWorkers(pThis);
		}
	}

	RETiRet;
}


/* check if we run in disk-assisted mode and record that
 * setting for easy (and quick!) access in the future. This
 * function must only be called from constructors and only
 * from those that support disk-assisted modes (aka memory-
 * based queue drivers).
 * rgerhards, 2008-01-14
 */
static rsRetVal
qqueueChkIsDA(qqueue_t *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, qqueue);
	if(pThis->pszFilePrefix != NULL) {
		pThis->bIsDA = 1;
		dbgoprint((obj_t*) pThis, "is disk-assisted, disk will be used on demand\n");
	} else {
		dbgoprint((obj_t*) pThis, "is NOT disk-assisted\n");
	}

	RETiRet;
}


/* Start disk-assisted queue mode. All internal settings are changed. This is supposed
 * to be called from the DA worker, which must have been started before. The most important
 * chore of this function is to create the DA queue object. If that function fails,
 * the DA worker should return with an appropriate state, which in turn should lead to
 * a re-set to non-DA mode in the Enq process. The queue mutex must be locked when this
 * function is called, else a number of races will happen.
 * Please note that this function may be called *while* we in DA mode. This is due to the
 * fact that the DA worker calls it and the DA worker may be suspended (and restarted) due
 * to inactivity timeouts.
 * rgerhards, 2008-01-15
 */
static rsRetVal
qqueueStartDA(qqueue_t *pThis)
{
	DEFiRet;
	uchar pszDAQName[128];

	ISOBJ_TYPE_assert(pThis, qqueue);

	if(pThis->bRunsDA == 2) /* check if already in (fully initialized) DA mode... */
		FINALIZE;       /* ... then we are already done! */

	/* create message queue */
	CHKiRet(qqueueConstruct(&pThis->pqDA, QUEUETYPE_DISK , 1, 0, pThis->pConsumer));

	/* give it a name */
	snprintf((char*) pszDAQName, sizeof(pszDAQName)/sizeof(uchar), "%s[DA]", obj.GetName((obj_t*) pThis));
	obj.SetName((obj_t*) pThis->pqDA, pszDAQName);

	/* as the created queue is the same object class, we take the
	 * liberty to access its properties directly.
	 */
	pThis->pqDA->pqParent = pThis;

	CHKiRet(qqueueSetpUsr(pThis->pqDA, pThis->pUsr));
	CHKiRet(qqueueSetsizeOnDiskMax(pThis->pqDA, pThis->sizeOnDiskMax));
	CHKiRet(qqueueSetiDeqSlowdown(pThis->pqDA, pThis->iDeqSlowdown));
	CHKiRet(qqueueSetMaxFileSize(pThis->pqDA, pThis->iMaxFileSize));
	CHKiRet(qqueueSetFilePrefix(pThis->pqDA, pThis->pszFilePrefix, pThis->lenFilePrefix));
	CHKiRet(qqueueSetiPersistUpdCnt(pThis->pqDA, pThis->iPersistUpdCnt));
	CHKiRet(qqueueSettoActShutdown(pThis->pqDA, pThis->toActShutdown));
	CHKiRet(qqueueSettoEnq(pThis->pqDA, pThis->toEnq));
	CHKiRet(qqueueSetEnqOnly(pThis->pqDA, pThis->bDAEnqOnly, MUTEX_ALREADY_LOCKED));
	CHKiRet(qqueueSetiDeqtWinFromHr(pThis->pqDA, pThis->iDeqtWinFromHr));
	CHKiRet(qqueueSetiDeqtWinToHr(pThis->pqDA, pThis->iDeqtWinToHr));
	CHKiRet(qqueueSetiHighWtrMrk(pThis->pqDA, 0));
	CHKiRet(qqueueSetiDiscardMrk(pThis->pqDA, 0));
	if(pThis->toQShutdown == 0) {
		CHKiRet(qqueueSettoQShutdown(pThis->pqDA, 0)); /* if the user really wants... */
	} else {
		/* we use the shortest possible shutdown (0 is endless!) because when we run on disk AND
		 * have an obviously large backlog, we can't finish it in any case. So there is no point
		 * in holding shutdown longer than necessary. -- rgerhards, 2008-01-15
		 */
		CHKiRet(qqueueSettoQShutdown(pThis->pqDA, 1));
	}

	iRet = qqueueStart(pThis->pqDA);
	/* file not found is expected, that means it is no previous QIF available */
	if(iRet != RS_RET_OK && iRet != RS_RET_FILE_NOT_FOUND)
		FINALIZE; /* something is wrong */

	/* as we are right now starting DA mode because we are so busy, it is
	 * extremely unlikely that any regular worker is sleeping on empty queue. HOWEVER,
	 * we want to be on the safe side, and so we awake anyone that is waiting
	 * on one. So even if the scheduler plays badly with us, things should be
	 * quite well. -- rgerhards, 2008-01-15
	 */
	wtpWakeupWrkr(pThis->pWtpReg); /* awake all workers, but not ourselves ;) */

	pThis->bRunsDA = 2;	/* we are now in DA mode, but not fully initialized */
	pThis->bChildIsDone = 0;/* set to 1 when child's worker detect queue is finished */
	pthread_cond_broadcast(&pThis->condDAReady); /* signal we are now initialized and ready to go ;) */

	dbgoprint((obj_t*) pThis, "is now running in disk assisted mode, disk queue 0x%lx\n",
		  qqueueGetID(pThis->pqDA));

finalize_it:
	if(iRet != RS_RET_OK) {
		if(pThis->pqDA != NULL) {
			qqueueDestruct(&pThis->pqDA);
		}
		dbgoprint((obj_t*) pThis, "error %d creating disk queue - giving up.\n", iRet);
		pThis->bIsDA = 0;
	}

	RETiRet;
}


/* initiate DA mode
 * param bEnqOnly tells if the disk queue is to be run in enqueue-only mode. This may
 * be needed during shutdown of memory queues which need to be persisted to disk.
 * If this function fails (should not happen), DA mode is not turned on.
 * rgerhards, 2008-01-16
 */
static inline rsRetVal
qqueueInitDA(qqueue_t *pThis, int bEnqOnly, int bLockMutex)
{
	DEFiRet;
	DEFVARS_mutexProtection;
	uchar pszBuf[64];
	size_t lenBuf;

	BEGIN_MTX_PROTECTED_OPERATIONS(pThis->mut, bLockMutex);
	/* check if we already have a DA worker pool. If not, initiate one. Please note that the
	 * pool is created on first need but never again destructed (until the queue is). This
	 * is intentional. We assume that when we need it once, we may also need it on another
	 * occasion. Ressources used are quite minimal when no worker is running.
	 * rgerhards, 2008-01-24
	 */
	if(pThis->pWtpDA == NULL) {
		lenBuf = snprintf((char*)pszBuf, sizeof(pszBuf), "%s:DA", obj.GetName((obj_t*) pThis));
		CHKiRet(wtpConstruct		(&pThis->pWtpDA));
		CHKiRet(wtpSetDbgHdr		(pThis->pWtpDA, pszBuf, lenBuf));
		CHKiRet(wtpSetpfChkStopWrkr	(pThis->pWtpDA, (rsRetVal (*)(void *pUsr, int)) qqueueChkStopWrkrDA));
		CHKiRet(wtpSetpfGetDeqBatchSize	(pThis->pWtpDA, (rsRetVal (*)(void *pUsr, int*)) GetDeqBatchSize));
		CHKiRet(wtpSetpfIsIdle		(pThis->pWtpDA, (rsRetVal (*)(void *pUsr, int)) qqueueIsIdleDA));
		CHKiRet(wtpSetpfDoWork		(pThis->pWtpDA, (rsRetVal (*)(void *pUsr, void *pWti, int)) ConsumerDA));
		CHKiRet(wtpSetpfOnWorkerCancel	(pThis->pWtpDA, (rsRetVal (*)(void *pUsr, void*pWti)) ConsumerCancelCleanup));
		CHKiRet(wtpSetpfOnWorkerStartup	(pThis->pWtpDA, (rsRetVal (*)(void *pUsr)) qqueueStartDA));
		CHKiRet(wtpSetpfOnWorkerShutdown(pThis->pWtpDA, (rsRetVal (*)(void *pUsr)) qqueueTurnOffDAMode));
		CHKiRet(wtpSetpmutUsr		(pThis->pWtpDA, pThis->mut));
		CHKiRet(wtpSetpcondBusy		(pThis->pWtpDA, &pThis->notEmpty));
		CHKiRet(wtpSetiNumWorkerThreads	(pThis->pWtpDA, 1));
		CHKiRet(wtpSettoWrkShutdown	(pThis->pWtpDA, pThis->toWrkShutdown));
		CHKiRet(wtpSetpUsr		(pThis->pWtpDA, pThis));
		CHKiRet(wtpConstructFinalize	(pThis->pWtpDA));
	}
	/* if we reach this point, we have a "good" DA worker pool */

	/* indicate we now run in DA mode - this is reset by the DA worker if it fails */
	pThis->bRunsDA = 1;
	pThis->bDAEnqOnly = bEnqOnly;

	/* now we must now adivse the wtp that we need one worker. If none is yet active,
	 * that will also start one up. If we forgot that step, everything would be stalled
	 * until the next enqueue request.
	 */
	wtpAdviseMaxWorkers(pThis->pWtpDA, 1); /* DA queues alsways have just one worker max */

finalize_it:
	END_MTX_PROTECTED_OPERATIONS(pThis->mut);
	RETiRet;
}


/* check if we need to start disk assisted mode and send some signals to
 * keep it running if we are already in it. It also checks if DA mode is
 * partially initialized, in which case it waits for initialization to
 * complete.
 * rgerhards, 2008-01-14
 */
static inline rsRetVal
qqueueChkStrtDA(qqueue_t *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, qqueue);

	/* if we do not hit the high water mark, we have nothing to do */
	if(getPhysicalQueueSize(pThis) != pThis->iHighWtrMrk)
		ABORT_FINALIZE(RS_RET_OK);

	if(pThis->bRunsDA) {
		/* then we need to signal that we are at the high water mark again. If that happens
		 * on our way down the queue, that doesn't matter, because then nobody is waiting
		 * on the condition variable.
		 * (Remember that a DA queue stops draining the queue once it has reached the low
		 * water mark and restarts it when the high water mark is reached again - this is
		 * what this code here is responsible for. Please note that all workers may have been
		 * terminated due to the inactivity timeout, thus we need to advise the pool that
		 * we need at least one).
		 */
		dbgoprint((obj_t*) pThis, "%d entries - passed high water mark in DA mode, send notify\n",
			  getPhysicalQueueSize(pThis));
		qqueueAdviseMaxWorkers(pThis);
	} else {
		/* this is the case when we are currently not running in DA mode. So it is time
		 * to turn it back on.
		 */
		dbgoprint((obj_t*) pThis, "%d entries - passed high water mark for disk-assisted mode, initiating...\n",
			  getPhysicalQueueSize(pThis));
		qqueueInitDA(pThis, QUEUE_MODE_ENQDEQ, MUTEX_ALREADY_LOCKED); /* initiate DA mode */
	}

finalize_it:
	RETiRet;
}


/* --------------- end code for disk-assisted queue modes -------------------- */


/* Now, we define type-specific handlers. The provide a generic functionality,
 * but for this specific type of queue. The mapping to these handlers happens during
 * queue construction. Later on, handlers are called by pointers present in the
 * queue instance object.
 */

/* -------------------- fixed array -------------------- */
static rsRetVal qConstructFixedArray(qqueue_t *pThis)
{
	DEFiRet;

	ASSERT(pThis != NULL);

	if(pThis->iMaxQueueSize == 0)
		ABORT_FINALIZE(RS_RET_QSIZE_ZERO);

	if((pThis->tVars.farray.pBuf = malloc(sizeof(void *) * pThis->iMaxQueueSize)) == NULL) {
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}

	pThis->tVars.farray.deqhead = 0;
	pThis->tVars.farray.head = 0;
	pThis->tVars.farray.tail = 0;

	qqueueChkIsDA(pThis);

finalize_it:
	RETiRet;
}


static rsRetVal qDestructFixedArray(qqueue_t *pThis)
{
	DEFiRet;
	
	ASSERT(pThis != NULL);

	queueDrain(pThis); /* discard any remaining queue entries */
	free(pThis->tVars.farray.pBuf);

	RETiRet;
}


static rsRetVal qAddFixedArray(qqueue_t *pThis, void* in)
{
	DEFiRet;

	ASSERT(pThis != NULL);
	pThis->tVars.farray.pBuf[pThis->tVars.farray.tail] = in;
	pThis->tVars.farray.tail++;
	if (pThis->tVars.farray.tail == pThis->iMaxQueueSize)
		pThis->tVars.farray.tail = 0;

	RETiRet;
}


static rsRetVal qDeqFixedArray(qqueue_t *pThis, void **out)
{
	DEFiRet;

	ASSERT(pThis != NULL);
	*out = (void*) pThis->tVars.farray.pBuf[pThis->tVars.farray.deqhead];

//MULTIdbgprintf("ULTRA qDeqFA, deqhead=%d head=%d, tail=%d\n", pThis->tVars.farray.deqhead, pThis->tVars.farray.head, pThis->tVars.farray.tail);
	pThis->tVars.farray.deqhead++;
	if (pThis->tVars.farray.deqhead == pThis->iMaxQueueSize)
		pThis->tVars.farray.deqhead = 0;

	RETiRet;
}

static rsRetVal qDelFixedArray(qqueue_t *pThis)
{
	DEFiRet;

	ASSERT(pThis != NULL);

//MULTIdbgprintf("ULTRA qDelFA, deqhead=%d head=%d, tail=%d\n", pThis->tVars.farray.deqhead, pThis->tVars.farray.head, pThis->tVars.farray.tail);
	pThis->tVars.farray.head++;
	if (pThis->tVars.farray.head == pThis->iMaxQueueSize)
		pThis->tVars.farray.head = 0;

	RETiRet;
}


/* -------------------- linked list  -------------------- */

/* first some generic functions which are also used for the unget linked list */

static inline rsRetVal qqueueAddLinkedList(qLinkedList_t **ppRoot, qLinkedList_t **ppLast, void* pUsr)
{
	qLinkedList_t *pEntry;
	DEFiRet;

	ASSERT(ppRoot != NULL);
	ASSERT(ppLast != NULL);

	CHKmalloc((pEntry = (qLinkedList_t*) malloc(sizeof(qLinkedList_t))));

	pEntry->pNext = NULL;
	pEntry->pUsr = pUsr;

	if(*ppRoot == NULL) {
		*ppRoot = *ppLast = pEntry;
	} else {
		(*ppLast)->pNext = pEntry;
		*ppLast = pEntry;
	}

finalize_it:
	RETiRet;
}

static inline rsRetVal qqueueDelLinkedList(qLinkedList_t **ppRoot, qLinkedList_t **ppLast, obj_t **ppUsr)
{
	DEFiRet;
	qLinkedList_t *pEntry;

	ASSERT(ppRoot != NULL);
	ASSERT(ppLast != NULL);
	ASSERT(ppUsr != NULL);
	ASSERT(*ppRoot != NULL);
	
	pEntry = *ppRoot;
	*ppUsr = pEntry->pUsr;

	if(*ppRoot == *ppLast) {
		*ppRoot = NULL;
		*ppLast = NULL;
	} else {
		*ppRoot = pEntry->pNext;
	}
	free(pEntry);

	RETiRet;
}

/* end generic functions which are also used for the unget linked list */


static rsRetVal qConstructLinkedList(qqueue_t *pThis)
{
	DEFiRet;

	ASSERT(pThis != NULL);

	pThis->tVars.linklist.pDeqRoot = NULL;
	pThis->tVars.linklist.pDelRoot = NULL;
	pThis->tVars.linklist.pLast = NULL;

	qqueueChkIsDA(pThis);

	RETiRet;
}


static rsRetVal qDestructLinkedList(qqueue_t __attribute__((unused)) *pThis)
{
	DEFiRet;

	queueDrain(pThis); /* discard any remaining queue entries */

	/* with the linked list type, there is nothing left to do here. The
	 * reason is that there are no dynamic elements for the list itself.
	 */

	RETiRet;
}

static rsRetVal qAddLinkedList(qqueue_t *pThis, void* pUsr)
{
	qLinkedList_t *pEntry;
	DEFiRet;

	CHKmalloc((pEntry = (qLinkedList_t*) malloc(sizeof(qLinkedList_t))));

	pEntry->pNext = NULL;
	pEntry->pUsr = pUsr;

	if(pThis->tVars.linklist.pDelRoot == NULL) {
		pThis->tVars.linklist.pDelRoot = pThis->tVars.linklist.pDeqRoot = pThis->tVars.linklist.pLast = pEntry;
	} else {
		pThis->tVars.linklist.pLast->pNext = pEntry;
		pThis->tVars.linklist.pLast = pEntry;
	}

	if(pThis->tVars.linklist.pDeqRoot == NULL) {
		pThis->tVars.linklist.pDeqRoot = pEntry;
	}
RUNLOG_VAR("%p", pThis->tVars.linklist.pDeqRoot);

finalize_it:
	RETiRet;
}


static rsRetVal qDeqLinkedList(qqueue_t *pThis, obj_t **ppUsr)
{
	qLinkedList_t *pEntry;
	DEFiRet;

RUNLOG_VAR("%p", pThis->tVars.linklist.pDeqRoot);
	pEntry = pThis->tVars.linklist.pDeqRoot;
	*ppUsr = pEntry->pUsr;
	pThis->tVars.linklist.pDeqRoot = pEntry->pNext;

	RETiRet;
}


static rsRetVal qDelLinkedList(qqueue_t *pThis)
{
	qLinkedList_t *pEntry;
	DEFiRet;

	pEntry = pThis->tVars.linklist.pDelRoot;

	if(pThis->tVars.linklist.pDelRoot == pThis->tVars.linklist.pLast) {
		pThis->tVars.linklist.pDelRoot = pThis->tVars.linklist.pDeqRoot = pThis->tVars.linklist.pLast = NULL;
	} else {
		pThis->tVars.linklist.pDelRoot = pEntry->pNext;
	}

	free(pEntry);

	RETiRet;
}


/* -------------------- disk  -------------------- */


static rsRetVal
qqueueLoadPersStrmInfoFixup(strm_t *pStrm, qqueue_t __attribute__((unused)) *pThis)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pStrm, strm);
	ISOBJ_TYPE_assert(pThis, qqueue);
	CHKiRet(strmSetDir(pStrm, glbl.GetWorkDir(), strlen((char*)glbl.GetWorkDir())));
finalize_it:
	RETiRet;
}


/* This method checks if we have a QIF file for the current queue (no matter of
 * queue mode). Returns RS_RET_OK if we have a QIF file or an error status otherwise.
 * rgerhards, 2008-01-15
 */
static rsRetVal 
qqueueHaveQIF(qqueue_t *pThis)
{
	DEFiRet;
	uchar pszQIFNam[MAXFNAME];
	size_t lenQIFNam;
	struct stat stat_buf;

	ISOBJ_TYPE_assert(pThis, qqueue);

	if(pThis->pszFilePrefix == NULL)
		ABORT_FINALIZE(RS_RET_NO_FILEPREFIX);

	/* Construct file name */
	lenQIFNam = snprintf((char*)pszQIFNam, sizeof(pszQIFNam) / sizeof(uchar), "%s/%s.qi",
			     (char*) glbl.GetWorkDir(), (char*)pThis->pszFilePrefix);

	/* check if the file exists */
	if(stat((char*) pszQIFNam, &stat_buf) == -1) {
		if(errno == ENOENT) {
			dbgoprint((obj_t*) pThis, "no .qi file found\n");
			ABORT_FINALIZE(RS_RET_FILE_NOT_FOUND);
		} else {
			dbgoprint((obj_t*) pThis, "error %d trying to access .qi file\n", errno);
			ABORT_FINALIZE(RS_RET_IO_ERROR);
		}
	}
	/* If we reach this point, we have a .qi file */

finalize_it:
	RETiRet;
}


/* The method loads the persistent queue information.
 * rgerhards, 2008-01-11
 */
static rsRetVal 
qqueueTryLoadPersistedInfo(qqueue_t *pThis)
{
	DEFiRet;
	strm_t *psQIF = NULL;
	uchar pszQIFNam[MAXFNAME];
	size_t lenQIFNam;
	struct stat stat_buf;

	ISOBJ_TYPE_assert(pThis, qqueue);

	/* Construct file name */
	lenQIFNam = snprintf((char*)pszQIFNam, sizeof(pszQIFNam) / sizeof(uchar), "%s/%s.qi",
			     (char*) glbl.GetWorkDir(), (char*)pThis->pszFilePrefix);

	/* check if the file exists */
	if(stat((char*) pszQIFNam, &stat_buf) == -1) {
		if(errno == ENOENT) {
			dbgoprint((obj_t*) pThis, "clean startup, no .qi file found\n");
			ABORT_FINALIZE(RS_RET_FILE_NOT_FOUND);
		} else {
			dbgoprint((obj_t*) pThis, "error %d trying to access .qi file\n", errno);
			ABORT_FINALIZE(RS_RET_IO_ERROR);
		}
	}

	/* If we reach this point, we have a .qi file */

	CHKiRet(strmConstruct(&psQIF));
	CHKiRet(strmSettOperationsMode(psQIF, STREAMMODE_READ));
	CHKiRet(strmSetsType(psQIF, STREAMTYPE_FILE_SINGLE));
	CHKiRet(strmSetFName(psQIF, pszQIFNam, lenQIFNam));
	CHKiRet(strmConstructFinalize(psQIF));

	/* first, we try to read the property bag for ourselfs */
	CHKiRet(obj.DeserializePropBag((obj_t*) pThis, psQIF));
	
	/* then the stream objects (same order as when persisted!) */
	CHKiRet(obj.Deserialize(&pThis->tVars.disk.pWrite, (uchar*) "strm", psQIF,
			       (rsRetVal(*)(obj_t*,void*))qqueueLoadPersStrmInfoFixup, pThis));
	CHKiRet(obj.Deserialize(&pThis->tVars.disk.pReadDel, (uchar*) "strm", psQIF,
			       (rsRetVal(*)(obj_t*,void*))qqueueLoadPersStrmInfoFixup, pThis));

	CHKiRet(strmSeekCurrOffs(pThis->tVars.disk.pWrite));
	CHKiRet(strmSeekCurrOffs(pThis->tVars.disk.pReadDel));

	/* we now need to take care of the Deq handle. It is not persisted, so we can create
	 * a virgin copy based on pReadDel. // TODO duplicat code, same as blow - single function!
	 */

	CHKiRet(strmConstruct(&pThis->tVars.disk.pReadDeq));
	CHKiRet(strmSetbDeleteOnClose(pThis->tVars.disk.pReadDeq, 0));
	CHKiRet(strmSetDir(pThis->tVars.disk.pReadDeq, glbl.GetWorkDir(), strlen((char*)glbl.GetWorkDir())));
	CHKiRet(strmSetiMaxFiles(pThis->tVars.disk.pReadDeq, 10000000));
	CHKiRet(strmSettOperationsMode(pThis->tVars.disk.pReadDeq, STREAMMODE_READ));
	CHKiRet(strmSetsType(pThis->tVars.disk.pReadDeq, STREAMTYPE_FILE_CIRCULAR));
	CHKiRet(strmConstructFinalize(pThis->tVars.disk.pReadDeq));

	/* TODO: dirty, need stream methods --> */
	pThis->tVars.disk.pReadDeq->iCurrFNum = pThis->tVars.disk.pReadDel->iCurrFNum;
	pThis->tVars.disk.pReadDeq->iCurrOffs = pThis->tVars.disk.pReadDel->iCurrOffs;
	/* <-- dirty, need stream methods :TODO */
	CHKiRet(strmSeekCurrOffs(pThis->tVars.disk.pReadDeq));

	/* OK, we could successfully read the file, so we now can request that it be
	 * deleted when we are done with the persisted information.
	 */
	pThis->bNeedDelQIF = 1;

finalize_it:
	if(psQIF != NULL)
		strmDestruct(&psQIF);

	if(iRet != RS_RET_OK) {
		dbgoprint((obj_t*) pThis, "error %d reading .qi file - can not read persisted info (if any)\n",
			  iRet);
	}

	RETiRet;
}


/* disk queue constructor.
 * Note that we use a file limit of 10,000,000 files. That number should never pose a
 * problem. If so, I guess the user has a design issue... But of course, the code can
 * always be changed (though it would probably be more appropriate to increase the
 * allowed file size at this point - that should be a config setting...
 * rgerhards, 2008-01-10
 */
static rsRetVal qConstructDisk(qqueue_t *pThis)
{
	DEFiRet;
	int bRestarted = 0;

	ASSERT(pThis != NULL);

	/* and now check if there is some persistent information that needs to be read in */
	iRet = qqueueTryLoadPersistedInfo(pThis);
	if(iRet == RS_RET_OK)
		bRestarted = 1;
	else if(iRet != RS_RET_FILE_NOT_FOUND)
			FINALIZE;

	if(bRestarted == 1) {
		;
	} else {
		CHKiRet(strmConstruct(&pThis->tVars.disk.pWrite));
		CHKiRet(strmSetDir(pThis->tVars.disk.pWrite, glbl.GetWorkDir(), strlen((char*)glbl.GetWorkDir())));
		CHKiRet(strmSetiMaxFiles(pThis->tVars.disk.pWrite, 10000000));
		CHKiRet(strmSettOperationsMode(pThis->tVars.disk.pWrite, STREAMMODE_WRITE));
		CHKiRet(strmSetsType(pThis->tVars.disk.pWrite, STREAMTYPE_FILE_CIRCULAR));
		CHKiRet(strmConstructFinalize(pThis->tVars.disk.pWrite));

		CHKiRet(strmConstruct(&pThis->tVars.disk.pReadDeq));
		CHKiRet(strmSetbDeleteOnClose(pThis->tVars.disk.pReadDeq, 0));
		CHKiRet(strmSetDir(pThis->tVars.disk.pReadDeq, glbl.GetWorkDir(), strlen((char*)glbl.GetWorkDir())));
		CHKiRet(strmSetiMaxFiles(pThis->tVars.disk.pReadDeq, 10000000));
		CHKiRet(strmSettOperationsMode(pThis->tVars.disk.pReadDeq, STREAMMODE_READ));
		CHKiRet(strmSetsType(pThis->tVars.disk.pReadDeq, STREAMTYPE_FILE_CIRCULAR));
		CHKiRet(strmConstructFinalize(pThis->tVars.disk.pReadDeq));

		CHKiRet(strmConstruct(&pThis->tVars.disk.pReadDel));
		CHKiRet(strmSetbDeleteOnClose(pThis->tVars.disk.pReadDel, 1));
		CHKiRet(strmSetDir(pThis->tVars.disk.pReadDel, glbl.GetWorkDir(), strlen((char*)glbl.GetWorkDir())));
		CHKiRet(strmSetiMaxFiles(pThis->tVars.disk.pReadDel, 10000000));
		CHKiRet(strmSettOperationsMode(pThis->tVars.disk.pReadDel, STREAMMODE_READ));
		CHKiRet(strmSetsType(pThis->tVars.disk.pReadDel, STREAMTYPE_FILE_CIRCULAR));
		CHKiRet(strmConstructFinalize(pThis->tVars.disk.pReadDel));

		CHKiRet(strmSetFName(pThis->tVars.disk.pWrite,   pThis->pszFilePrefix, pThis->lenFilePrefix));
		CHKiRet(strmSetFName(pThis->tVars.disk.pReadDeq, pThis->pszFilePrefix, pThis->lenFilePrefix));
		CHKiRet(strmSetFName(pThis->tVars.disk.pReadDel, pThis->pszFilePrefix, pThis->lenFilePrefix));
	}

	/* now we set (and overwrite in case of a persisted restart) some parameters which
	 * should always reflect the current configuration variables. Be careful by doing so,
	 * for example file name generation must not be changed as that would break the
	 * ability to read existing queue files. -- rgerhards, 2008-01-12
	 */
	CHKiRet(strmSetiMaxFileSize(pThis->tVars.disk.pWrite, pThis->iMaxFileSize));
	CHKiRet(strmSetiMaxFileSize(pThis->tVars.disk.pReadDeq, pThis->iMaxFileSize));
	CHKiRet(strmSetiMaxFileSize(pThis->tVars.disk.pReadDel, pThis->iMaxFileSize));

finalize_it:
	RETiRet;
}


static rsRetVal qDestructDisk(qqueue_t *pThis)
{
	DEFiRet;
	
	ASSERT(pThis != NULL);
	
	strmDestruct(&pThis->tVars.disk.pWrite);
	strmDestruct(&pThis->tVars.disk.pReadDeq);
	strmDestruct(&pThis->tVars.disk.pReadDel);

	RETiRet;
}

static rsRetVal qAddDisk(qqueue_t *pThis, void* pUsr)
{
	DEFiRet;
	number_t nWriteCount;

	ASSERT(pThis != NULL);

	CHKiRet(strmSetWCntr(pThis->tVars.disk.pWrite, &nWriteCount));
	CHKiRet((objSerialize(pUsr))(pUsr, pThis->tVars.disk.pWrite));
	CHKiRet(strmFlush(pThis->tVars.disk.pWrite));
	CHKiRet(strmSetWCntr(pThis->tVars.disk.pWrite, NULL)); /* no more counting for now... */

	pThis->tVars.disk.sizeOnDisk += nWriteCount;

	/* we have enqueued the user element to disk. So we now need to destruct
	 * the in-memory representation. The instance will be re-created upon
	 * dequeue. -- rgerhards, 2008-07-09
	 */
	objDestruct(pUsr);

	dbgoprint((obj_t*) pThis, "write wrote %lld octets to disk, queue disk size now %lld octets\n",
		   nWriteCount, pThis->tVars.disk.sizeOnDisk);

finalize_it:
	RETiRet;
}


static rsRetVal qDeqDisk(qqueue_t *pThis, void **ppUsr)
{
	DEFiRet;

	CHKiRet(obj.Deserialize(ppUsr, (uchar*) "msg", pThis->tVars.disk.pReadDeq, NULL, NULL));

finalize_it:
	RETiRet;
}


static rsRetVal qDelDisk(qqueue_t *pThis)
{
	obj_t *pDummyObj;	/* we need to deserialize it... */
	DEFiRet;

	int64 offsIn;
	int64 offsOut;

	CHKiRet(strmGetCurrOffset(pThis->tVars.disk.pReadDel, &offsIn));
	CHKiRet(obj.Deserialize(&pDummyObj, (uchar*) "msg", pThis->tVars.disk.pReadDel, NULL, NULL));
	objDestruct(pDummyObj);
	CHKiRet(strmGetCurrOffset(pThis->tVars.disk.pReadDel, &offsOut));

	/* This time it is a bit tricky: we free disk space only upon file deletion. So we need
	 * to keep track of what we have read until we get an out-offset that is lower than the
	 * in-offset (which indicates file change). Then, we can subtract the whole thing from
	 * the on-disk size. -- rgerhards, 2008-01-30
	 */
	if(offsIn < offsOut) {
		pThis->tVars.disk.bytesRead += offsOut - offsIn;
	} else {
		pThis->tVars.disk.sizeOnDisk -= pThis->tVars.disk.bytesRead;
		pThis->tVars.disk.bytesRead = offsOut;
		dbgoprint((obj_t*) pThis, "a file has been deleted, now %lld octets disk space used\n", pThis->tVars.disk.sizeOnDisk);
		/* awake possibly waiting enq process */
		pthread_cond_signal(&pThis->notFull); /* we hold the mutex while we are in here! */
	}

finalize_it:
	RETiRet;
}


/* -------------------- direct (no queueing) -------------------- */
static rsRetVal qConstructDirect(qqueue_t __attribute__((unused)) *pThis)
{
	return RS_RET_OK;
}


static rsRetVal qDestructDirect(qqueue_t __attribute__((unused)) *pThis)
{
	return RS_RET_OK;
}

static rsRetVal qAddDirect(qqueue_t *pThis, void* pUsr)
{
	batch_t singleBatch;
	batch_obj_t batchObj;
	DEFiRet;

	ASSERT(pThis != NULL);

	/* calling the consumer is quite different here than it is from a worker thread */
	/* we need to provide the consumer's return value back to the caller because in direct
	 * mode the consumer probably has a lot to convey (which get's lost in the other modes
	 * because they are asynchronous. But direct mode is deliberately synchronous.
	 * rgerhards, 2008-02-12
	 * We use our knowledge about the batch_t structure below, but without that, we
	 * pay a too-large performance toll... -- rgerhards, 2009-04-22
	 */
	batchObj.state = BATCH_STATE_RDY;
	batchObj.pUsrp = (obj_t*) pUsr;
	singleBatch.nElem = 1; /* there always is only one in direct mode */
	singleBatch.pElem = &batchObj;
	iRet = pThis->pConsumer(pThis->pUsr, &singleBatch);

	RETiRet;
}


static rsRetVal qDelDirect(qqueue_t __attribute__((unused)) *pThis)
{
	return RS_RET_OK;
}


/* --------------- end type-specific handlers -------------------- */


/* generic code to add a queue entry
 * We use some specific code to most efficiently support direct mode
 * queues. This is justified in spite of the gain and the need to do some
 * things truely different. -- rgerhards, 2008-02-12
 */
static rsRetVal
qqueueAdd(qqueue_t *pThis, void *pUsr)
{
	DEFiRet;

	ASSERT(pThis != NULL);

	CHKiRet(pThis->qAdd(pThis, pUsr));

	if(pThis->qType != QUEUETYPE_DIRECT) {
		ATOMIC_INC(pThis->iQueueSize);
		dbgoprint((obj_t*) pThis, "entry added, size now log %d, phys %d entries\n",
			  getLogicalQueueSize(pThis), getPhysicalQueueSize(pThis));
	}

finalize_it:
	RETiRet;
}


/* generic code to dequeue a queue entry
 */
static rsRetVal
qqueueDeq(qqueue_t *pThis, void *pUsr)
{
	DEFiRet;

	ASSERT(pThis != NULL);

	/* we do NOT abort if we encounter an error, because otherwise the queue
	 * will not be decremented, what will most probably result in an endless loop.
	 * If we decrement, however, we may lose a message. But that is better than
	 * losing the whole process because it loops... -- rgerhards, 2008-01-03
	 */
	iRet = pThis->qDeq(pThis, pUsr);
	ATOMIC_INC(pThis->nLogDeq);

	dbgoprint((obj_t*) pThis, "entry deleted, size now log %d, phys %d entries\n",
		  getLogicalQueueSize(pThis), getPhysicalQueueSize(pThis));

	RETiRet;
}


/* This function shuts down all worker threads and waits until they
 * have terminated. If they timeout, they are cancelled. Parameters have been set
 * before this function is called so that DA queues will be fully persisted to
 * disk (if configured to do so).
 * rgerhards, 2008-01-24
 * Please note that this function shuts down BOTH the parent AND the child queue
 * in DA case. This is necessary because their timeouts are tightly coupled. Most
 * importantly, the timeouts would be applied twice (or logic be extremely
 * complex) if each would have its own shutdown. The function does not self check
 * this condition - the caller must make sure it is not called with a parent.
 */
static rsRetVal qqueueShutdownWorkers(qqueue_t *pThis)
{
	DEFiRet;
	DEFVARS_mutexProtection;
	struct timespec tTimeout;
	rsRetVal iRetLocal;

	ISOBJ_TYPE_assert(pThis, qqueue);
	ASSERT(pThis->pqParent == NULL); /* detect invalid calling sequence */

	dbgoprint((obj_t*) pThis, "initiating worker thread shutdown sequence\n");

	/* we reduce the low water mark in any case. This is not absolutely necessary, but
	 * it is useful because we enable DA mode at several spots below and so we do not need
	 * to think about the low water mark each time. 
	 */
	pThis->iHighWtrMrk = 1; /* if we do not do this, the DA queue will not stop! */
	pThis->iLowWtrMrk = 0;

	/* first try to shutdown the queue within the regular shutdown period */
	BEGIN_MTX_PROTECTED_OPERATIONS(pThis->mut, LOCK_MUTEX);	/* some workers may be running in parallel! */
	if(getPhysicalQueueSize(pThis) > 0) {
		if(pThis->bRunsDA) {
			/* We may have waited on the low water mark. As it may have changed, we
			 * see if we reactivate the worker.
			 */
			wtpAdviseMaxWorkers(pThis->pWtpDA, 1);
		}
	}
	END_MTX_PROTECTED_OPERATIONS(pThis->mut);

	/* Now wait for the queue's workers to shut down. Note that we run into the code even if we just found
	 * out there are no active workers - that doesn't matter: the wtp knows about that and so will
	 * return immediately.
	 * We do not yet care about the DA worker - that will be handled down later in the process.
	 * Note that we must not request shutdown right now - that may introduce a race: if the regular queue
	 * still runs DA assisted and the DA worker gets scheduled first, it will terminate itself (if the DA
	 * queue happens to be empty at that instant). Then the regular worker enqueues messages, what will lead
	 * to a restart of the worker. Of course, everything will continue to run, but in a bit sub-optimal way
	 * (from a performance point of view). So we don't do anything right now. The DA queue will continue to
	 * process messages and shutdown itself in any case if there is nothing to do. So we don't loose anything
	 * by not requesting shutdown now.
	 * rgerhards, 2008-01-25
	 */
	/* first calculate absolute timeout - we need the absolute value here, because we need to coordinate
	 * shutdown of both the regular and DA queue on *the same* timeout.
	 */
	timeoutComp(&tTimeout, pThis->toQShutdown);
	dbgoprint((obj_t*) pThis, "trying shutdown of regular workers\n");
	iRetLocal = wtpShutdownAll(pThis->pWtpReg, wtpState_SHUTDOWN, &tTimeout);
	if(iRetLocal == RS_RET_TIMED_OUT) {
		dbgoprint((obj_t*) pThis, "regular shutdown timed out on primary queue (this is OK)\n");
	} else {
		/* OK, the regular queue is now shut down. So we can now wait for the DA queue (if running DA) */
		dbgoprint((obj_t*) pThis, "regular queue workers shut down.\n");
		BEGIN_MTX_PROTECTED_OPERATIONS(pThis->mut, LOCK_MUTEX);	/* some workers may be running in parallel! */
		if(pThis->bRunsDA) {
			END_MTX_PROTECTED_OPERATIONS(pThis->mut);
			dbgoprint((obj_t*) pThis, "we have a DA queue (0x%lx), requesting its shutdown.\n",
				 qqueueGetID(pThis->pqDA));
			/* we use the same absolute timeout as above, so we do not use more than the configured
			 * timeout interval!
			 */
			dbgoprint((obj_t*) pThis, "trying shutdown of DA workers\n");
			iRetLocal = wtpShutdownAll(pThis->pWtpDA, wtpState_SHUTDOWN, &tTimeout);
			if(iRetLocal == RS_RET_TIMED_OUT) {
				dbgoprint((obj_t*) pThis, "shutdown timed out on DA queue (this is OK)\n");
			}
		} else {
			END_MTX_PROTECTED_OPERATIONS(pThis->mut);
		}
	}

	/* when we reach this point, both queues are either empty or the regular queue shutdown timeout
	 * has expired. Now we need to check if we are configured to not loose messages. If so, we need
	 * to persist the queue to disk (this is only possible if the queue is DA-enabled). We must also
	 * set the primary queue to SHUTDOWN_IMMEDIATE, as it shall now terminate as soon as its consumer
	 * is done. This is especially important as we otherwise may interfere with queue order while the
	 * DA consumer is running. -- rgerhards, 2008-01-27
	 * Note: there was a note that we should not wait eternally on the DA worker if we run in
	 * enqueue-only note. I have reviewed the code and think there is no need for this check. Howerver,
	 * I'd like to keep this note in here should we happen to run into some related trouble.
	 * rgerhards, 2008-01-28
	 */
	wtpSetState(pThis->pWtpReg, wtpState_SHUTDOWN_IMMEDIATE); /* set primary queue to shutdown only */

	/* at this stage, we need to have the DA worker properly initialized and running (if there is one) */
	if(pThis->bRunsDA)
		qqueueWaitDAModeInitialized(pThis);

	BEGIN_MTX_PROTECTED_OPERATIONS(pThis->mut, LOCK_MUTEX);	/* some workers may be running in parallel! */
	/* optimize parameters for shutdown of DA-enabled queues */
	if(pThis->bIsDA && getPhysicalQueueSize(pThis) > 0 && pThis->bSaveOnShutdown) {
		/* switch to enqueue-only mode so that no more actions happen */
		if(pThis->bRunsDA == 0) {
			qqueueInitDA(pThis, QUEUE_MODE_ENQONLY, MUTEX_ALREADY_LOCKED); /* switch to DA mode */
		} else {
			/* TODO: RACE: we may reach this point when the DA worker has been initialized (state 1)
			 * but is not yet running (state 2). In this case, pThis->pqDA is NULL! rgerhards, 2008-02-27
			 */
			qqueueSetEnqOnly(pThis->pqDA, QUEUE_MODE_ENQONLY, MUTEX_ALREADY_LOCKED); /* switch to enqueue-only mode */
		}
		END_MTX_PROTECTED_OPERATIONS(pThis->mut);
		/* make sure we do not timeout before we are done */
		dbgoprint((obj_t*) pThis, "bSaveOnShutdown configured, eternal timeout set\n");
		timeoutComp(&tTimeout, QUEUE_TIMEOUT_ETERNAL);
		/* and run the primary queue's DA worker to drain the queue */
		iRetLocal = wtpShutdownAll(pThis->pWtpDA, wtpState_SHUTDOWN, &tTimeout);
		if(iRetLocal != RS_RET_OK) {
			dbgoprint((obj_t*) pThis, "unexpected iRet state %d after trying to shut down primary queue in disk save mode, "
				  "continuing, but results are unpredictable\n", iRetLocal);
		}
	} else {
		END_MTX_PROTECTED_OPERATIONS(pThis->mut);
	}

	/* now the primary queue is either empty, persisted to disk - or set to loose messages. So we
	 * can now request immediate shutdown of any remaining workers. Note that if bSaveOnShutdown was set,
	 * the queue is now empty. If regular workers are still running, and try to pull the next message,
	 * they will automatically terminate as there no longer is any message left to process.
	 */
	BEGIN_MTX_PROTECTED_OPERATIONS(pThis->mut, LOCK_MUTEX);	/* some workers may be running in parallel! */
	if(getPhysicalQueueSize(pThis) > 0) {
		timeoutComp(&tTimeout, pThis->toActShutdown);
		if(wtpGetCurNumWrkr(pThis->pWtpReg, LOCK_MUTEX) > 0) {
			END_MTX_PROTECTED_OPERATIONS(pThis->mut);
			dbgoprint((obj_t*) pThis, "trying immediate shutdown of regular workers\n");
			iRetLocal = wtpShutdownAll(pThis->pWtpReg, wtpState_SHUTDOWN_IMMEDIATE, &tTimeout);
			if(iRetLocal == RS_RET_TIMED_OUT) {
				dbgoprint((obj_t*) pThis, "immediate shutdown timed out on primary queue (this is acceptable and "
					  "triggers cancellation)\n");
			} else if(iRetLocal != RS_RET_OK) {
				dbgoprint((obj_t*) pThis, "unexpected iRet state %d after trying immediate shutdown of the primary queue "
					  "in disk save mode. Continuing, but results are unpredictable\n", iRetLocal);
			}
			/* we need to re-aquire the mutex for the next check in this case! */
			BEGIN_MTX_PROTECTED_OPERATIONS(pThis->mut, LOCK_MUTEX);	/* some workers may be running in parallel! */
		}
		if(pThis->bIsDA && wtpGetCurNumWrkr(pThis->pWtpDA, LOCK_MUTEX) > 0) {
			/* and now the same for the DA queue */
			END_MTX_PROTECTED_OPERATIONS(pThis->mut);
			dbgoprint((obj_t*) pThis, "trying immediate shutdown of DA workers\n");
			iRetLocal = wtpShutdownAll(pThis->pWtpDA, wtpState_SHUTDOWN_IMMEDIATE, &tTimeout);
			if(iRetLocal == RS_RET_TIMED_OUT) {
				dbgoprint((obj_t*) pThis, "immediate shutdown timed out on DA queue (this is acceptable and "
					  "triggers cancellation)\n");
			} else if(iRetLocal != RS_RET_OK) {
				dbgoprint((obj_t*) pThis, "unexpected iRet state %d after trying immediate shutdown of the DA queue "
					  "in disk save mode. Continuing, but results are unpredictable\n", iRetLocal);
			}
		} else {
			END_MTX_PROTECTED_OPERATIONS(pThis->mut);
		}
	} else {
		END_MTX_PROTECTED_OPERATIONS(pThis->mut);
	}

	/* Now queue workers should have terminated. If not, we need to cancel them as we have applied
	 * all timeout setting. If any worker in any queue still executes, its consumer is possibly
	 * long-running and cancelling is the only way to get rid of it. Note that the
	 * cancellation handler will probably re-queue a user pointer, so the queue's enqueue
	 * function is still needed (what is no problem as we do not yet destroy the queue - but I
	 * thought it's a good idea to mention that fact). -- rgerhards, 2008-01-25
	 */
	dbgoprint((obj_t*) pThis, "checking to see if we need to cancel any worker threads of the primary queue\n");
	iRetLocal = wtpCancelAll(pThis->pWtpReg); /* returns immediately if all threads already have terminated */
	if(iRetLocal != RS_RET_OK) {
		dbgoprint((obj_t*) pThis, "unexpected iRet state %d trying to cancel primary queue worker "
			  "threads, continuing, but results are unpredictable\n", iRetLocal);
	}


	/* TODO: think: do we really need to do this here? Can't it happen on DA queue destruction? If we 
	 * disable it, we get an assertion... I think this is OK, as we need to have a certain order and
	 * canceling the DA workers here ensures that order. But in any instant, we may have a look at this
	 * code after we have reaced the milestone. -- rgerhards, 2008-01-27
	 */
	/* ... and now the DA queue, if it exists (should always be after the primary one) */
	if(pThis->pqDA != NULL) {
		dbgoprint((obj_t*) pThis, "checking to see if we need to cancel any worker threads of the DA queue\n");
		iRetLocal = wtpCancelAll(pThis->pqDA->pWtpReg); /* returns immediately if all threads already have terminated */
		if(iRetLocal != RS_RET_OK) {
			dbgoprint((obj_t*) pThis, "unexpected iRet state %d trying to cancel DA queue worker "
				  "threads, continuing, but results are unpredictable\n", iRetLocal);
		}
	}

	/* ... finally ... all worker threads have terminated :-)
	 * Well, more precisely, they *are in termination*. Some cancel cleanup handlers
	 * may still be running. 
	 */
	dbgoprint((obj_t*) pThis, "worker threads terminated, remaining queue size %d.\n", getPhysicalQueueSize(pThis));

	RETiRet;
}



/* Constructor for the queue object
 * This constructs the data structure, but does not yet start the queue. That
 * is done by queueStart(). The reason is that we want to give the caller a chance
 * to modify some parameters before the queue is actually started.
 */
rsRetVal qqueueConstruct(qqueue_t **ppThis, queueType_t qType, int iWorkerThreads,
		        int iMaxQueueSize, rsRetVal (*pConsumer)(void*, batch_t*))
{
	DEFiRet;
	qqueue_t *pThis;

	ASSERT(ppThis != NULL);
	ASSERT(pConsumer != NULL);
	ASSERT(iWorkerThreads >= 0);

	if((pThis = (qqueue_t *)calloc(1, sizeof(qqueue_t))) == NULL) {
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}

	/* we have an object, so let's fill the properties */
	objConstructSetObjInfo(pThis);
	pThis->bOptimizeUniProc = glbl.GetOptimizeUniProc();
	if((pThis->pszSpoolDir = (uchar*) strdup((char*)glbl.GetWorkDir())) == NULL)
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

	/* set some water marks so that we have useful defaults if none are set specifically */
	pThis->iFullDlyMrk  = iMaxQueueSize - (iMaxQueueSize / 100) *  3; /* default 97% */
	pThis->iLightDlyMrk = iMaxQueueSize - (iMaxQueueSize / 100) * 30; /* default 70% */

	pThis->lenSpoolDir = strlen((char*)pThis->pszSpoolDir);
	pThis->iMaxFileSize = 1024 * 1024; /* default is 1 MiB */
	pThis->iQueueSize = 0;
	pThis->nLogDeq = 0;
	pThis->iMaxQueueSize = iMaxQueueSize;
	pThis->pConsumer = pConsumer;
	pThis->iNumWorkerThreads = iWorkerThreads;
	pThis->iDeqtWinToHr = 25; /* disable time-windowed dequeuing by default */
	pThis->iDeqBatchSize = 8; /* conservative default, should still provide good performance */

	pThis->pszFilePrefix = NULL;
	pThis->qType = qType;

	/* set type-specific handlers and other very type-specific things (we can not totally hide it...) */
	switch(qType) {
		case QUEUETYPE_FIXED_ARRAY:
			pThis->qConstruct = qConstructFixedArray;
			pThis->qDestruct = qDestructFixedArray;
			pThis->qAdd = qAddFixedArray;
			pThis->qDeq = qDeqFixedArray;
			pThis->qDel = qDelFixedArray;
			break;
		case QUEUETYPE_LINKEDLIST:
			pThis->qConstruct = qConstructLinkedList;
			pThis->qDestruct = qDestructLinkedList;
			pThis->qAdd = qAddLinkedList;
			pThis->qDeq = (rsRetVal (*)(qqueue_t*,void**)) qDeqLinkedList;
			pThis->qDel = (rsRetVal (*)(qqueue_t*)) qDelLinkedList;
			break;
		case QUEUETYPE_DISK:
			pThis->qConstruct = qConstructDisk;
			pThis->qDestruct = qDestructDisk;
			pThis->qAdd = qAddDisk;
			pThis->qDeq = qDeqDisk;
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
	RETiRet;
}


/* cancellation cleanup handler for queueWorker ()
 * Updates admin structure and frees ressources.
 * Params:
 * arg1 - user pointer (in this case a qqueue_t)
 * arg2 - user data pointer (in this case a queue data element, any object [queue's pUsr ptr!])
 * Note that arg2 may be NULL, in which case no dequeued but unprocessed pUsr exists!
 * rgerhards, 2008-01-16
 */
static rsRetVal
ConsumerCancelCleanup(void *arg1, void *arg2)
{
	//TODO: looks like we no longer need it!
	/*
	DEFiRet;

	qqueue_t *pThis = (qqueue_t*) arg1;
	obj_t *pUsr = (obj_t*) arg2;

	ISOBJ_TYPE_assert(pThis, qqueue);

	RETiRet;
	*/
	return RS_RET_OK;
}



/* This function checks if the provided message shall be discarded and does so, if needed.
 * In DA mode, we do not discard any messages as we assume the disk subsystem is fast enough to
 * provide real-time creation of spool files.
 * Note: cached copies of iQueueSize and bRunsDA are provided so that no mutex locks are required.
 * The caller must have obtained them while the mutex was locked. Of course, these values may no
 * longer be current, but that is OK for the discard check. At worst, the message is either processed
 * or discarded when it should not have been. As discarding is in itself somewhat racy and erratic,
 * that is no problems for us. This function MUST NOT lock the queue mutex, it could result in
 * deadlocks!
 * If the message is discarded, it can no longer be processed by the caller. So be sure to check
 * the return state!
 * rgerhards, 2008-01-24
 */
static int qqueueChkDiscardMsg(qqueue_t *pThis, int iQueueSize, int bRunsDA, void *pUsr)
{
	DEFiRet;
	rsRetVal iRetLocal;
	int iSeverity;

	ISOBJ_TYPE_assert(pThis, qqueue);
	ISOBJ_assert(pUsr);

	if(pThis->iDiscardMrk > 0 && iQueueSize >= pThis->iDiscardMrk && bRunsDA == 0) {
		iRetLocal = objGetSeverity(pUsr, &iSeverity);
		if(iRetLocal == RS_RET_OK && iSeverity >= pThis->iDiscardSeverity) {
			dbgoprint((obj_t*) pThis, "queue nearly full (%d entries), discarded severity %d message\n",
				  iQueueSize, iSeverity);
			objDestruct(pUsr);
			ABORT_FINALIZE(RS_RET_QUEUE_FULL);
		} else {
			dbgoprint((obj_t*) pThis, "queue nearly full (%d entries), but could not drop msg "
				  "(iRet: %d, severity %d)\n", iQueueSize, iRetLocal, iSeverity);
		}
	}

finalize_it:
	RETiRet;
}


/* Finally remove n elements from the queue store.
 */
static inline rsRetVal
DoDeleteBatchFromQStore(qqueue_t *pThis, int nElem)
{
	int i;
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, qqueue);

dbgprintf("pre delete batch from store, new sizes: log %d, phys %d, nElem %d\n", getLogicalQueueSize(pThis), getPhysicalQueueSize(pThis), nElem);
	/* now send delete request to storage driver */
	for(i = 0 ; i < nElem ; ++i) {
		pThis->qDel(pThis);
	}

	/* iQueueSize is not decremented by qDel(), so we need to do it ourselves */
	pThis->iQueueSize -= nElem;
	pThis->nLogDeq -= nElem;
dbgprintf("delete batch from store, new sizes: log %d, phys %d\n", getLogicalQueueSize(pThis), getPhysicalQueueSize(pThis));
	++pThis->deqIDDel; /* one more batch dequeued */

	RETiRet;
}


/* remove messages from the physical queue store that are fully processed. This is
 * controlled via the to-delete list. We can only delete those elements, that are
 * at the current physical tail of the queue. If the batch is from another position,
 * we schedule it for deletion, but actual deletion will happen at a later call
 * of this function here. We always delete as much as possible, which includes
 * picking up things from the to-delete list.
 */
static inline rsRetVal
DeleteBatchFromQStore(qqueue_t *pThis, batch_t *pBatch)
{
	toDeleteLst_t *pTdl;
	qDeqID	deqIDDel;
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, qqueue);
	assert(pBatch != NULL);

	pTdl = tdlPeek(pThis);
	if(pTdl == NULL) {
		DoDeleteBatchFromQStore(pThis, pBatch->nElem);
	} else if(pBatch->deqID == pThis->deqIDDel) {
		deqIDDel = pThis->deqIDDel;
		pTdl = tdlPeek(pThis);
		while(pTdl != NULL && deqIDDel == pTdl->deqID) {
			DoDeleteBatchFromQStore(pThis, pTdl->nElem);
			tdlPop(pThis);
			++deqIDDel;
			pTdl = tdlPeek(pThis);
		}
	} else {
		/* can not delete, insert into to-delete list */
		CHKiRet(tdlAdd(pThis, pBatch->deqID, pBatch->nElem));
	}

finalize_it:
	RETiRet;
}


/* Delete a batch of processed user objects from the queue, which includes
 * destructing the objects themself.
 * rgerhards, 2009-05-13
 */
static inline rsRetVal
DeleteProcessedBatch(qqueue_t *pThis, batch_t *pBatch)
{
	int i;
	void *pUsr;
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, qqueue);
	assert(pBatch != NULL);
// TODO: ULTRA: lock qaueue mutex if instructed to do so

	/* if the queue runs in DA mode, the DA worker already deleted the in-memory representation
	 * of the message. But in regular mode, we need to do it ourselfs. We differentiate between
	 * the two cases, because it is actually the easiest way to handle the destruct-Problem in
	 * a simple and pUsrp-Type agnostic way (else we would need an objAddRef() generic function).
	 */
	if(!pThis->bRunsDA) {
		for(i = 0 ; i < pBatch->nElem ; ++i) {
			pUsr = pBatch->pElem[i].pUsrp;
			objDestruct(pUsr);
		}
	}

	iRet = DeleteBatchFromQStore(pThis, pBatch);

	pBatch->nElem = 0; /* reset batch */

	RETiRet;
}


/* dequeue as many user pointers as are available, until we hit the configured
 * upper limit of pointers.
 * This must only be called when the queue mutex is LOOKED, otherwise serious
 * malfunction will happen.
 */
static inline rsRetVal
DequeueConsumableElements(qqueue_t *pThis, wti_t *pWti, int *piRemainingQueueSize)
{
	int nDequeued;
	int iQueueSize;
	void *pUsr;
	rsRetVal localRet;
	DEFiRet;

	/* this is the place to destruct the old messages and pull them off the queue - MULTI-DEQUEUE */
	DeleteProcessedBatch(pThis, &pWti->batch);

	nDequeued = 0;
	do {
dbgprintf("DequeueConsumableElements, index %d\n", nDequeued);
		CHKiRet(qqueueDeq(pThis, &pUsr));
		iQueueSize = getLogicalQueueSize(pThis);

		/* check if we should discard this element */
		localRet = qqueueChkDiscardMsg(pThis, pThis->iQueueSize, pThis->bRunsDA, pUsr);
		//MULTI-DEQUEUE / ULTRA-RELIABLE: we need to handle this case, we need to advance the
		// DEQ pointer (or so...) TODO!!! Idea: get a second nElem int in pBatch, nDequeued. Use that when deleting!
		if(localRet == RS_RET_QUEUE_FULL)
			continue;
		else if(localRet != RS_RET_OK)
			ABORT_FINALIZE(localRet);

		/* all well, use this element */
		pWti->batch.pElem[nDequeued].pUsrp = pUsr;
		pWti->batch.pElem[nDequeued].state = BATCH_STATE_RDY;
		++nDequeued;
	} while(iQueueSize > 0 && nDequeued < pThis->iDeqBatchSize);

	qqueueChkPersist(pThis, nDequeued); /* it is sufficient to persist only when the bulk of work is done */

	pWti->batch.nElem = nDequeued;
	pWti->batch.deqID = getNextDeqID(pThis);
	*piRemainingQueueSize = iQueueSize;

finalize_it:
	RETiRet;
}


/* dequeue the queued object for the queue consumers.
 * rgerhards, 2008-10-21
 * I made a radical change - we now dequeue multiple elements, and store these objects in
 * an array of user pointers. We expect that this increases performance.
 * rgerhards, 2009-04-22
 */
static rsRetVal
DequeueConsumable(qqueue_t *pThis, wti_t *pWti, int iCancelStateSave)
{
	DEFiRet;
	int iQueueSize = 0; /* keep the compiler happy... */

	/* dequeue element batch (still protected from mutex) */
	iRet = DequeueConsumableElements(pThis, pWti, &iQueueSize);

	/* awake some flow-controlled sources if we can do this right now */
	/* TODO: this could be done better from a performance point of view -- do it only if
	 * we have someone waiting for the condition (or only when we hit the watermark right
	 * on the nail [exact value]) -- rgerhards, 2008-03-14
	 * now that we dequeue batches of pointers, this is much less an issue...
	 * rgerhards, 2009-04-22
	 */
	if(iQueueSize < pThis->iFullDlyMrk) {
		pthread_cond_broadcast(&pThis->belowFullDlyWtrMrk);
	}

	if(iQueueSize < pThis->iLightDlyMrk) {
		pthread_cond_broadcast(&pThis->belowLightDlyWtrMrk);
	}

	pthread_cond_signal(&pThis->notFull);
	d_pthread_mutex_unlock(pThis->mut);
	pthread_setcancelstate(iCancelStateSave, NULL);
	/* WE ARE NO LONGER PROTECTED BY THE MUTEX */

	if(iRet != RS_RET_OK && iRet != RS_RET_DISCARDMSG) {
		dbgoprint((obj_t*) pThis, "error %d dequeueing element - ignoring, but strange things "
			  "may happen\n", iRet);
	}

	RETiRet;
}


/* The rate limiter
 *
 * Here we may wait if a dequeue time window is defined or if we are
 * rate-limited. TODO: If we do so, we should also look into the
 * way new worker threads are spawned. Obviously, it doesn't make much
 * sense to spawn additional worker threads when none of them can do any
 * processing. However, it is deemed acceptable to allow this for an initial
 * implementation of the timeframe/rate limiting feature.
 * Please also note that these feature could also be implemented at the action
 * level. However, that would limit them to be used together with actions. We have
 * taken the broader approach, moving it right into the queue. This is even
 * necessary if we want to prevent spawning of multiple unnecessary worker
 * threads as described above. -- rgerhards, 2008-04-02
 *
 *
 * time window: tCurr is current time; tFrom is start time, tTo is end time (in mil 24h format).
 * We may have tFrom = 4, tTo = 10 --> run from 4 to 10 hrs. nice and happy
 * we may also have tFrom= 22, tTo = 4 -> run from 10pm to 4am, which is actually two
 *     windows: 0-4; 22-23:59
 * so when to run? Let's assume we have 3am
 *
 * if(tTo < tFrom) {
 * 	if(tCurr < tTo [3 < 4] || tCurr > tFrom [3 > 22])
 * 		do work
 * 	else
 * 		sleep for tFrom - tCurr "hours" [22 - 5 --> 17]
 * } else {
 * 	if(tCurr >= tFrom [3 >= 4] && tCurr < tTo [3 < 10])
 * 		do work
 * 	else
 * 		sleep for tTo - tCurr "hours" [4 - 3 --> 1]
 * }
 *
 * Bottom line: we need to check which type of window we have and need to adjust our
 * logic accordingly. Of course, sleep calculations need to be done up to the minute, 
 * but you get the idea from the code above.
 */
static rsRetVal
RateLimiter(qqueue_t *pThis)
{
	DEFiRet;
	int iDelay;
	int iHrCurr;
	time_t tCurr;
	struct tm m;

	ISOBJ_TYPE_assert(pThis, qqueue);

	iDelay = 0;
	if(pThis->iDeqtWinToHr != 25) { /* 25 means disabled */
		/* time calls are expensive, so only do them when needed */
		time(&tCurr);
		localtime_r(&tCurr, &m);
		iHrCurr = m.tm_hour;

		if(pThis->iDeqtWinToHr < pThis->iDeqtWinFromHr) {
			if(iHrCurr < pThis->iDeqtWinToHr || iHrCurr > pThis->iDeqtWinFromHr) {
				; /* do not delay */
			} else {
				iDelay = (pThis->iDeqtWinFromHr - iHrCurr) * 3600;
				/* this time, we are already into the next hour, so we need
				 * to subtract our current minute and seconds.
				 */
				iDelay -= m.tm_min * 60;
				iDelay -= m.tm_sec;
			}
		} else {
			if(iHrCurr >= pThis->iDeqtWinFromHr && iHrCurr < pThis->iDeqtWinToHr) {
				; /* do not delay */
			} else {
				if(iHrCurr < pThis->iDeqtWinFromHr) {
					iDelay = (pThis->iDeqtWinFromHr - iHrCurr - 1) * 3600; /* -1 as we are already in the hour */
					iDelay += (60 - m.tm_min) * 60;
					iDelay += 60 - m.tm_sec;
				} else {
					iDelay = (24 - iHrCurr + pThis->iDeqtWinFromHr) * 3600;
					/* this time, we are already into the next hour, so we need
					 * to subtract our current minute and seconds.
					 */
					iDelay -= m.tm_min * 60;
					iDelay -= m.tm_sec;
				}
			}
		}
	}

	if(iDelay > 0) {
		dbgoprint((obj_t*) pThis, "outside dequeue time window, delaying %d seconds\n", iDelay);
		srSleep(iDelay, 0);
	}

	RETiRet;
}



/* This is the queue consumer in the regular (non-DA) case. It is 
 * protected by the queue mutex, but MUST release it as soon as possible.
 * rgerhards, 2008-01-21
 */
static rsRetVal
ConsumerReg(qqueue_t *pThis, wti_t *pWti, int iCancelStateSave)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, qqueue);
	ISOBJ_TYPE_assert(pWti, wti);

	CHKiRet(DequeueConsumable(pThis, pWti, iCancelStateSave));
	CHKiRet(pThis->pConsumer(pThis->pUsr, &pWti->batch));

	/* we now need to check if we should deliberately delay processing a bit
	 * and, if so, do that. -- rgerhards, 2008-01-30
	 */
//TODO: MULTIQUEUE: the following setting is no longer correct - need to think about how to do that...
	if(pThis->iDeqSlowdown) {
		dbgoprint((obj_t*) pThis, "sleeping %d microseconds as requested by config params\n",
			  pThis->iDeqSlowdown);
		srSleep(pThis->iDeqSlowdown / 1000000, pThis->iDeqSlowdown % 1000000);
	}

finalize_it:
	RETiRet;
}


/* This is a special consumer to feed the disk-queue in disk-assisted mode.
 * When active, our own queue more or less acts as a memory buffer to the disk.
 * So this consumer just needs to drain the memory queue and submit entries
 * to the disk queue. The disk queue will then call the actual consumer from
 * the app point of view (we chain two queues here).
 * When this method is entered, the mutex is always locked and needs to be unlocked
 * as part of the processing.
 * rgerhards, 2008-01-14
 */
static rsRetVal
ConsumerDA(qqueue_t *pThis, wti_t *pWti, int iCancelStateSave)
{
	int i;
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, qqueue);
	ISOBJ_TYPE_assert(pWti, wti);

	CHKiRet(DequeueConsumable(pThis, pWti, iCancelStateSave));
	/* iterate over returned results and enqueue them in DA queue */
	for(i = 0 ; i < pWti->batch.nElem ; i++)
		CHKiRet(qqueueEnqObj(pThis->pqDA, eFLOWCTL_NO_DELAY, pWti->batch.pElem[i].pUsrp));

finalize_it:
	dbgoprint((obj_t*) pThis, "DAConsumer returns with iRet %d\n", iRet);
	RETiRet;
}


/* must only be called when the queue mutex is locked, else results
 * are not stable!
 * If we are a child, we have done our duty when the queue is empty. In that case,
 * we can terminate.
 * Version for the DA worker thread. NOTE: the pThis->bRunsDA is different from
 * the DA queue
 */
static int
qqueueChkStopWrkrDA(qqueue_t *pThis)
{
	/* if our queue is in destruction, we drain to the DA queue and so we shall not terminate
	 * until we have done so.
	 */
	int bStopWrkr;

	BEGINfunc

	if(pThis->bEnqOnly) {
		bStopWrkr = 1;
	} else {
		if(pThis->bRunsDA) {
			ASSERT(pThis->pqDA != NULL);
			if(   pThis->pqDA->bEnqOnly
			   && pThis->pqDA->sizeOnDiskMax > 0
			   && pThis->pqDA->tVars.disk.sizeOnDisk > pThis->pqDA->sizeOnDiskMax) {
				/* this queue can never grow, so we can give up... */
				bStopWrkr = 1;
			} else if(getPhysicalQueueSize(pThis) < pThis->iHighWtrMrk && pThis->bQueueStarted == 1) {
				bStopWrkr = 1;
			} else {
				bStopWrkr = 0;
			}
		} else {
			bStopWrkr = 1;
		}
	}

	ENDfunc
	return  bStopWrkr;
}


/* must only be called when the queue mutex is locked, else results
 * are not stable!
 * If we are a child, we have done our duty when the queue is empty. In that case,
 * we can terminate.
 * Version for the regular worker thread. NOTE: the pThis->bRunsDA is different from
 * the DA queue
 */
static int
ChkStopWrkrReg(qqueue_t *pThis)
{
	return pThis->bEnqOnly || pThis->bRunsDA || (pThis->pqParent != NULL && getPhysicalQueueSize(pThis) == 0);
}


/* return the configured "deq max at once" interval
 * rgerhards, 2009-04-22
 */
static rsRetVal
GetDeqBatchSize(qqueue_t *pThis, int *pVal)
{
	DEFiRet;
	assert(pVal != NULL);
	*pVal = pThis->iDeqBatchSize;
	RETiRet;
}

/* must only be called when the queue mutex is locked, else results
 * are not stable! DA queue version
 */
static int
qqueueIsIdleDA(qqueue_t *pThis)
{
	/* remember: iQueueSize is the DA queue size, not the main queue! */
	/* TODO: I think we need just a single function for DA and non-DA mode - but I leave it for now as is */
	return(getLogicalQueueSize(pThis) == 0 || (pThis->bRunsDA && getLogicalQueueSize(pThis) <= pThis->iLowWtrMrk));
}
/* must only be called when the queue mutex is locked, else results
 * are not stable! Regular queue version
 */
static int
IsIdleReg(qqueue_t *pThis)
{
#if 0 /* enable for performance testing */
	int ret;
	ret = getLogicalQueueSize(pThis) == 0 || (pThis->bRunsDA && getLogicalQueueSize(pThis) <= pThis->iLowWtrMrk);
	if(ret) fprintf(stderr, "queue is idle\n");
	return ret;
#else 
	/* regular code! */
	return(getLogicalQueueSize(pThis) == 0 || (pThis->bRunsDA && getLogicalQueueSize(pThis) <= pThis->iLowWtrMrk));
#endif
}


/* This function is called when a worker thread for the regular queue is shut down.
 * If we are the primary queue, this is not really interesting to us. If, however,
 * we are the DA (child) queue, that means the DA queue is empty. In that case, we
 * need to signal the parent queue's DA worker, so that it can terminate DA mode.
 * rgerhards, 2008-01-26
 * rgerhards, 2008-02-27: HOWEVER, in a shutdown condition, it may be that the parent's worker thread pool
 * has already been terminated and destructed. This *is* a legal condition and happens
 * from time to time in practice. So we need to signal only if there still is a
 * parent DA worker queue. Please keep in mind that the the parent's DA worker
 * pool is DIFFERENT from our (DA queue) regular worker pool. So when the parent's
 * pWtpDA is destructed, there can still be some of our (DAq/wtp) threads be running.
 * I am telling this, because I, too, always get confused by those...
 */
static rsRetVal
RegOnWrkrShutdown(qqueue_t *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, qqueue);

	if(pThis->pqParent != NULL) {
		pThis->pqParent->bChildIsDone = 1; /* indicate we are done */
		if(pThis->pqParent->pWtpDA != NULL) { /* see comment in function header from 2008-02-27 */
			wtpAdviseMaxWorkers(pThis->pqParent->pWtpDA, 1); /* reactivate DA worker (always 1) */
		}
	}

	RETiRet;
}


/* The following function is called when a regular queue worker starts up. We need this
 * hook to indicate in the parent queue (if we are a child) that we are not done yet.
 */
static rsRetVal
RegOnWrkrStartup(qqueue_t *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, qqueue);

	if(pThis->pqParent != NULL) {
		pThis->pqParent->bChildIsDone = 0;
	}

	RETiRet;
}


/* start up the queue - it must have been constructed and parameters defined
 * before.
 */
rsRetVal qqueueStart(qqueue_t *pThis) /* this is the ConstructionFinalizer */
{
	DEFiRet;
	rsRetVal iRetLocal;
	int bInitialized = 0; /* is queue already initialized? */
	uchar pszBuf[64];
	size_t lenBuf;

	ASSERT(pThis != NULL);

	/* we need to do a quick check if our water marks are set plausible. If not,
	 * we correct the most important shortcomings. TODO: do that!!!! -- rgerhards, 2008-03-14
	 */

	/* finalize some initializations that could not yet be done because it is
	 * influenced by properties which might have been set after queueConstruct ()
	 */
	if(pThis->pqParent == NULL) {
		pThis->mut = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
		pthread_mutex_init(pThis->mut, NULL);
	} else {
		/* child queue, we need to use parent's mutex */
		dbgoprint((obj_t*) pThis, "I am a child\n");
		pThis->mut = pThis->pqParent->mut;
	}

	pthread_mutex_init(&pThis->mutThrdMgmt, NULL);
	pthread_cond_init (&pThis->condDAReady, NULL);
	pthread_cond_init (&pThis->notFull, NULL);
	pthread_cond_init (&pThis->notEmpty, NULL);
	pthread_cond_init (&pThis->belowFullDlyWtrMrk, NULL);
	pthread_cond_init (&pThis->belowLightDlyWtrMrk, NULL);

	/* call type-specific constructor */
	CHKiRet(pThis->qConstruct(pThis)); /* this also sets bIsDA */

	dbgoprint((obj_t*) pThis, "type %d, enq-only %d, disk assisted %d, maxFileSz %lld, lqsize %d, pqsize %d, child %d, "
				  "full delay %d, light delay %d, deq batch size %d starting\n",
		  pThis->qType, pThis->bEnqOnly, pThis->bIsDA, pThis->iMaxFileSize,
		  getLogicalQueueSize(pThis), getPhysicalQueueSize(pThis),
		  pThis->pqParent == NULL ? 0 : 1, pThis->iFullDlyMrk, pThis->iLightDlyMrk,
		  pThis->iDeqBatchSize);

	if(pThis->qType == QUEUETYPE_DIRECT)
		FINALIZE;	/* with direct queues, we are already finished... */

	/* create worker thread pools for regular operation. The DA pool is created on an as-needed
	 * basis, which potentially means never under most circumstances.
	 */
	lenBuf = snprintf((char*)pszBuf, sizeof(pszBuf), "%s:Reg", obj.GetName((obj_t*) pThis));
	CHKiRet(wtpConstruct		(&pThis->pWtpReg));
	CHKiRet(wtpSetDbgHdr		(pThis->pWtpReg, pszBuf, lenBuf));
	CHKiRet(wtpSetpfRateLimiter	(pThis->pWtpReg, (rsRetVal (*)(void *pUsr)) RateLimiter));
	CHKiRet(wtpSetpfChkStopWrkr	(pThis->pWtpReg, (rsRetVal (*)(void *pUsr, int)) ChkStopWrkrReg));
	CHKiRet(wtpSetpfGetDeqBatchSize	(pThis->pWtpReg, (rsRetVal (*)(void *pUsr, int*)) GetDeqBatchSize));
	CHKiRet(wtpSetpfIsIdle		(pThis->pWtpReg, (rsRetVal (*)(void *pUsr, int)) IsIdleReg));
	CHKiRet(wtpSetpfDoWork		(pThis->pWtpReg, (rsRetVal (*)(void *pUsr, void *pWti, int)) ConsumerReg));
	CHKiRet(wtpSetpfOnWorkerCancel	(pThis->pWtpReg, (rsRetVal (*)(void *pUsr, void*pWti))ConsumerCancelCleanup));
	CHKiRet(wtpSetpfOnWorkerStartup	(pThis->pWtpReg, (rsRetVal (*)(void *pUsr)) RegOnWrkrStartup));
	CHKiRet(wtpSetpfOnWorkerShutdown(pThis->pWtpReg, (rsRetVal (*)(void *pUsr)) RegOnWrkrShutdown));
	CHKiRet(wtpSetpmutUsr		(pThis->pWtpReg, pThis->mut));
	CHKiRet(wtpSetpcondBusy		(pThis->pWtpReg, &pThis->notEmpty));
	CHKiRet(wtpSetiNumWorkerThreads	(pThis->pWtpReg, pThis->iNumWorkerThreads));
	CHKiRet(wtpSettoWrkShutdown	(pThis->pWtpReg, pThis->toWrkShutdown));
	CHKiRet(wtpSetpUsr		(pThis->pWtpReg, pThis));
	CHKiRet(wtpConstructFinalize	(pThis->pWtpReg));

	/* initialize worker thread instances */
	if(pThis->bIsDA) {
		/* If we are disk-assisted, we need to check if there is a QIF file
		 * which we need to load. -- rgerhards, 2008-01-15
		 */
		iRetLocal = qqueueHaveQIF(pThis);
		if(iRetLocal == RS_RET_OK) {
			dbgoprint((obj_t*) pThis, "on-disk queue present, needs to be reloaded\n");
			qqueueInitDA(pThis, QUEUE_MODE_ENQDEQ, LOCK_MUTEX); /* initiate DA mode */
			bInitialized = 1; /* we are done */
		} else {
			/* TODO: use logerror? -- rgerhards, 2008-01-16 */
			dbgoprint((obj_t*) pThis, "error %d trying to access on-disk queue files, starting without them. "
			          "Some data may be lost\n", iRetLocal);
		}
	}

	if(!bInitialized) {
		dbgoprint((obj_t*) pThis, "queue starts up without (loading) any DA disk state (this is normal for the DA "
			  "queue itself!)\n");
	}

	/* if the queue already contains data, we need to start the correct number of worker threads. This can be
	 * the case when a disk queue has been loaded. If we did not start it here, it would never start.
	 */
	qqueueAdviseMaxWorkers(pThis);
	pThis->bQueueStarted = 1;

finalize_it:
	RETiRet;
}


/* persist the queue to disk. If we have something to persist, we first
 * save the information on the queue properties itself and then we call
 * the queue-type specific drivers.
 * Variable bIsCheckpoint is set to 1 if the persist is for a checkpoint,
 * and 0 otherwise.
 * rgerhards, 2008-01-10
 */
static rsRetVal qqueuePersist(qqueue_t *pThis, int bIsCheckpoint)
{
	DEFiRet;
	strm_t *psQIF = NULL; /* Queue Info File */
	uchar pszQIFNam[MAXFNAME];
	size_t lenQIFNam;

	ASSERT(pThis != NULL);

	if(pThis->qType != QUEUETYPE_DISK) {
		if(getPhysicalQueueSize(pThis) > 0) {
			/* This error code is OK, but we will probably not implement this any time
 			 * The reason is that persistence happens via DA queues. But I would like to
			 * leave the code as is, as we so have a hook in case we need one.
			 * -- rgerhards, 2008-01-28
			 */
			ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
		} else
			FINALIZE; /* if the queue is empty, we are happy and done... */
	}

	dbgoprint((obj_t*) pThis, "persisting queue to disk, %d entries...\n", getPhysicalQueueSize(pThis));

	/* Construct file name */
	lenQIFNam = snprintf((char*)pszQIFNam, sizeof(pszQIFNam) / sizeof(uchar), "%s/%s.qi",
			     (char*) glbl.GetWorkDir(), (char*)pThis->pszFilePrefix);

	if((bIsCheckpoint != QUEUE_CHECKPOINT) && (getPhysicalQueueSize(pThis) == 0)) {
		if(pThis->bNeedDelQIF) {
			unlink((char*)pszQIFNam);
			pThis->bNeedDelQIF = 0;
		}
		/* indicate spool file needs to be deleted */
		CHKiRet(strmSetbDeleteOnClose(pThis->tVars.disk.pReadDel, 1));
		FINALIZE; /* nothing left to do, so be happy */
	}

	CHKiRet(strmConstruct(&psQIF));
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
	CHKiRet(obj.BeginSerializePropBag(psQIF, (obj_t*) pThis));
	objSerializeSCALAR(psQIF, iQueueSize, INT);
	objSerializeSCALAR(psQIF, tVars.disk.sizeOnDisk, INT64);
	objSerializeSCALAR(psQIF, tVars.disk.bytesRead, INT64);
	CHKiRet(obj.EndSerialize(psQIF));

	/* now persist the stream info */
	CHKiRet(strmSerialize(pThis->tVars.disk.pWrite, psQIF));
	CHKiRet(strmSerialize(pThis->tVars.disk.pReadDel, psQIF));
	
	/* tell the input file object that it must not delete the file on close if the queue
	 * is non-empty - but only if we are not during a simple checkpoint
	 */
	if(bIsCheckpoint != QUEUE_CHECKPOINT) {
		CHKiRet(strmSetbDeleteOnClose(pThis->tVars.disk.pReadDel, 0));
	}

	/* we have persisted the queue object. So whenever it comes to an empty queue,
	 * we need to delete the QIF. Thus, we indicte that need.
	 */
	pThis->bNeedDelQIF = 1;

finalize_it:
	if(psQIF != NULL)
		strmDestruct(&psQIF);

	RETiRet;
}


/* check if we need to persist the current queue info. If an
 * error occurs, this should be ignored by caller (but we still
 * abide to our regular call interface)...
 * rgerhards, 2008-01-13
 * nUpdates is the number of updates since the last call to this function.
 * It may be > 1 due to batches. -- rgerhards, 2009-05-12
 */
static rsRetVal qqueueChkPersist(qqueue_t *pThis, int nUpdates)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, qqueue);
	assert(nUpdates > 0);

	pThis->iUpdsSincePersist += nUpdates;
	if(pThis->iPersistUpdCnt && pThis->iUpdsSincePersist >= pThis->iPersistUpdCnt) {
		qqueuePersist(pThis, QUEUE_CHECKPOINT);
		pThis->iUpdsSincePersist = 0;
	}

	RETiRet;
}


/* destructor for the queue object */
BEGINobjDestruct(qqueue) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(qqueue)
	pThis->bQueueInDestruction = 1; /* indicate we are in destruction (modifies some behaviour) */

	/* shut down all workers (handles *all* of the persistence logic)
	 * See function head comment of queueShutdownWorkers () on why we don't call it
	 * We also do not need to shutdown workers when we are in enqueue-only mode or we are a
	 * direct queue - because in both cases we have none... ;)
	 * with a child! -- rgerhards, 2008-01-28
	 */
	if(pThis->qType != QUEUETYPE_DIRECT && !pThis->bEnqOnly && pThis->pqParent == NULL)
		qqueueShutdownWorkers(pThis);

	/* finally destruct our (regular) worker thread pool
	 * Note: currently pWtpReg is never NULL, but if we optimize our logic, this may happen,
	 * e.g. when they are not created in enqueue-only mode. We already check the condition
	 * as this may otherwise be very hard to find once we optimize (and have long forgotten
	 * about this condition here ;)
	 * rgerhards, 2008-01-25
	 */
	if(pThis->qType != QUEUETYPE_DIRECT && pThis->pWtpReg != NULL) {
		wtpDestruct(&pThis->pWtpReg);
	}

	/* Now check if we actually have a DA queue and, if so, destruct it.
	 * Note that the wtp must be destructed first, it may be in cancel cleanup handler
	 * *right now* and actually *need* to access the queue object to persist some final
	 * data (re-queueing case). So we need to destruct the wtp first, which will make 
	 * sure all workers have terminated. Please note that this also generates a situation
	 * where it is possible that the DA queue has a parent pointer but the parent has
	 * no WtpDA associated with it - which is perfectly legal thanks to this code here.
	 */
	if(pThis->pWtpDA != NULL) {
		wtpDestruct(&pThis->pWtpDA);
	}
	if(pThis->pqDA != NULL) {
		qqueueDestruct(&pThis->pqDA);
	}

	/* persist the queue (we always do that - queuePersits() does cleanup if the queue is empty)
	 * This handler is most important for disk queues, it will finally persist the necessary
	 * on-disk structures. In theory, other queueing modes may implement their other (non-DA)
	 * methods of persisting a queue between runs, but in practice all of this is done via
	 * disk queues and DA mode. Anyhow, it doesn't hurt to know that we could extend it here
	 * if need arises (what I doubt...) -- rgerhards, 2008-01-25
	 */
	CHKiRet_Hdlr(qqueuePersist(pThis, QUEUE_NO_CHECKPOINT)) {
		dbgoprint((obj_t*) pThis, "error %d persisting queue - data lost!\n", iRet);
	}

	/* finally, clean up some simple things... */
	if(pThis->pqParent == NULL) {
		/* if we are not a child, we allocated our own mutex, which we now need to destroy */
		pthread_mutex_destroy(pThis->mut);
		free(pThis->mut);
	}
	pthread_mutex_destroy(&pThis->mutThrdMgmt);
	pthread_cond_destroy(&pThis->condDAReady);
	pthread_cond_destroy(&pThis->notFull);
	pthread_cond_destroy(&pThis->notEmpty);
	pthread_cond_destroy(&pThis->belowFullDlyWtrMrk);
	pthread_cond_destroy(&pThis->belowLightDlyWtrMrk);

	/* type-specific destructor */
	iRet = pThis->qDestruct(pThis);

	free(pThis->pszFilePrefix);
	free(pThis->pszSpoolDir);
ENDobjDestruct(qqueue)


/* set the queue's file prefix
 * The passed-in string is duplicated. So if the caller does not need
 * it any longer, it must free it.
 * rgerhards, 2008-01-09
 */
rsRetVal
qqueueSetFilePrefix(qqueue_t *pThis, uchar *pszPrefix, size_t iLenPrefix)
{
	DEFiRet;

	free(pThis->pszFilePrefix);
	pThis->pszFilePrefix = NULL;

	if(pszPrefix == NULL) /* just unset the prefix! */
		ABORT_FINALIZE(RS_RET_OK);

	if((pThis->pszFilePrefix = malloc(sizeof(uchar) * iLenPrefix + 1)) == NULL)
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	memcpy(pThis->pszFilePrefix, pszPrefix, iLenPrefix + 1);
	pThis->lenFilePrefix = iLenPrefix;

finalize_it:
	RETiRet;
}

/* set the queue's maximum file size
 * rgerhards, 2008-01-09
 */
rsRetVal
qqueueSetMaxFileSize(qqueue_t *pThis, size_t iMaxFileSize)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, qqueue);
	
	if(iMaxFileSize < 1024) {
		ABORT_FINALIZE(RS_RET_VALUE_TOO_LOW);
	}

	pThis->iMaxFileSize = iMaxFileSize;

finalize_it:
	RETiRet;
}


/* enqueue a new user data element
 * Enqueues the new element and awakes worker thread.
 */
rsRetVal
qqueueEnqObj(qqueue_t *pThis, flowControl_t flowCtlType, void *pUsr)
{
	DEFiRet;
	int iCancelStateSave;
	struct timespec t;

	ISOBJ_TYPE_assert(pThis, qqueue);

	/* first check if we need to discard this message (which will cause CHKiRet() to exit)
	 * rgerhards, 2008-10-07: It is OK to do this outside of mutex protection. The queue size
	 * and bRunsDA parameters may not reflect the correct settings here, but they are
	 * "good enough" in the sense that they can be used to drive the decision. Valgrind's
	 * threading tools may point this access to be an error, but this is done
	 * intentional. I do not see this causes problems to us.
	 */
	CHKiRet(qqueueChkDiscardMsg(pThis, getPhysicalQueueSize(pThis), pThis->bRunsDA, pUsr));

	/* Please note that this function is not cancel-safe and consequently
	 * sets the calling thread's cancelibility state to PTHREAD_CANCEL_DISABLE
	 * during its execution. If that is not done, race conditions occur if the
	 * thread is canceled (most important use case is input module termination).
	 * rgerhards, 2008-01-08
	 */
	if(pThis->qType != QUEUETYPE_DIRECT) {
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave);
		d_pthread_mutex_lock(pThis->mut);
	}

	/* then check if we need to add an assistance disk queue */
	if(pThis->bIsDA)
		CHKiRet(qqueueChkStrtDA(pThis));
	
	/* handle flow control
	 * There are two different flow control mechanisms: basic and advanced flow control.
	 * Basic flow control has always been implemented and protects the queue structures
	 * in that it makes sure no more data is enqueued than the queue is configured to
	 * support. Enhanced flow control is being added today. There are some sources which
	 * can easily be stopped, e.g. a file reader. This is the case because it is unlikely
	 * that blocking those sources will have negative effects (after all, the file is
	 * continued to be written). Other sources can somewhat be blocked (e.g. the kernel
	 * log reader or the local log stream reader): in general, nothing is lost if messages
	 * from these sources are not picked up immediately. HOWEVER, they can not block for
	 * an extended period of time, as this either causes message loss or - even worse - some
	 * other bad effects (e.g. unresponsive system in respect to the main system log socket).
	 * Finally, there are some (few) sources which can not be blocked at all. UDP syslog is
	 * a prime example. If a UDP message is not received, it is simply lost. So we can't
	 * do anything against UDP sockets that come in too fast. The core idea of advanced
	 * flow control is that we take into account the different natures of the sources and
	 * select flow control mechanisms that fit these needs. This also means, in the end
	 * result, that non-blockable sources like UDP syslog receive priority in the system.
	 * It's a side effect, but a good one ;) -- rgerhards, 2008-03-14
	 */
	if(flowCtlType == eFLOWCTL_FULL_DELAY) {
		while(getPhysicalQueueSize(pThis) >= pThis->iFullDlyMrk) {
			dbgoprint((obj_t*) pThis, "enqueueMsg: FullDelay mark reached for full delayable message - blocking.\n");
			pthread_cond_wait(&pThis->belowFullDlyWtrMrk, pThis->mut); /* TODO error check? But what do then? */
		}
	} else if(flowCtlType == eFLOWCTL_LIGHT_DELAY) {
		if(getPhysicalQueueSize(pThis) >= pThis->iLightDlyMrk) {
			dbgoprint((obj_t*) pThis, "enqueueMsg: LightDelay mark reached for light delayable message - blocking a bit.\n");
			timeoutComp(&t, 1000); /* 1000 millisconds = 1 second TODO: make configurable */
			pthread_cond_timedwait(&pThis->belowLightDlyWtrMrk, pThis->mut, &t); /* TODO error check? But what do then? */
		}
	}

	/* from our regular flow control settings, we are now ready to enqueue the object.
	 * However, we now need to do a check if the queue permits to add more data. If that
	 * is not the case, basic flow control enters the field, which means we wait for
	 * the queue to become ready or drop the new message. -- rgerhards, 2008-03-14
	 */
	while(   (pThis->iMaxQueueSize > 0 && getPhysicalQueueSize(pThis) >= pThis->iMaxQueueSize)
	      || (pThis->qType == QUEUETYPE_DISK && pThis->sizeOnDiskMax != 0
	      	  && pThis->tVars.disk.sizeOnDisk > pThis->sizeOnDiskMax)) {
		dbgoprint((obj_t*) pThis, "enqueueMsg: queue FULL - waiting to drain.\n");
		timeoutComp(&t, pThis->toEnq);
		if(pthread_cond_timedwait(&pThis->notFull, pThis->mut, &t) != 0) {
			dbgoprint((obj_t*) pThis, "enqueueMsg: cond timeout, dropping message!\n");
			objDestruct(pUsr);
			ABORT_FINALIZE(RS_RET_QUEUE_FULL);
		}
	}

	/* and finally enqueue the message */
	CHKiRet(qqueueAdd(pThis, pUsr));
	qqueueChkPersist(pThis, 1);

finalize_it:
	if(pThis->qType != QUEUETYPE_DIRECT) {
		/* make sure at least one worker is running. */
		qqueueAdviseMaxWorkers(pThis);
		/* and release the mutex */
		d_pthread_mutex_unlock(pThis->mut);
		pthread_setcancelstate(iCancelStateSave, NULL);
		dbgoprint((obj_t*) pThis, "EnqueueMsg advised worker start\n");
		/* the following pthread_yield is experimental, but brought us performance
		 * benefit. For details, please see http://kb.monitorware.com/post14216.html#p14216
		 * rgerhards, 2008-10-09
		 * but this is only true for uniprocessors, so we guard it with an optimize flag -- rgerhards, 2008-10-22
		 */
		if(pThis->bOptimizeUniProc)
			pthread_yield();
	}

	RETiRet;
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
qqueueSetEnqOnly(qqueue_t *pThis, int bEnqOnly, int bLockMutex)
{
	DEFiRet;
	DEFVARS_mutexProtection;

	ISOBJ_TYPE_assert(pThis, qqueue);

	/* for simplicity, we do one big mutex lock. This method is extremely seldom
	 * called, so that doesn't matter... -- rgerhards, 2008-01-16
	 */
	if(pThis->mut != NULL) {
		BEGIN_MTX_PROTECTED_OPERATIONS(pThis->mut, bLockMutex);
	}

	if(bEnqOnly == pThis->bEnqOnly)
		FINALIZE; /* no change, nothing to do */

	if(pThis->bQueueStarted) {
		/* we need to adjust queue operation only if we are not during initial param setup */
		if(bEnqOnly == 1) {
			/* switch to enqueue-only mode */
			/* this means we need to terminate all workers - that's it... */
			dbgoprint((obj_t*) pThis, "switching to enqueue-only mode, terminating all worker threads\n");
			if(pThis->pWtpReg != NULL)
				wtpWakeupAllWrkr(pThis->pWtpReg);
			if(pThis->pWtpDA != NULL)
				wtpWakeupAllWrkr(pThis->pWtpDA);
		} else {
			/* switch back to regular mode */
			ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED); /* we don't need this so far... */
		}
	}

	pThis->bEnqOnly = bEnqOnly;

finalize_it:
	if(pThis->mut != NULL) {
		END_MTX_PROTECTED_OPERATIONS(pThis->mut);
	}
	RETiRet;
}


/* some simple object access methods */
DEFpropSetMeth(qqueue, iPersistUpdCnt, int)
DEFpropSetMeth(qqueue, iDeqtWinFromHr, int)
DEFpropSetMeth(qqueue, iDeqtWinToHr, int)
DEFpropSetMeth(qqueue, toQShutdown, long)
DEFpropSetMeth(qqueue, toActShutdown, long)
DEFpropSetMeth(qqueue, toWrkShutdown, long)
DEFpropSetMeth(qqueue, toEnq, long)
DEFpropSetMeth(qqueue, iHighWtrMrk, int)
DEFpropSetMeth(qqueue, iLowWtrMrk, int)
DEFpropSetMeth(qqueue, iDiscardMrk, int)
DEFpropSetMeth(qqueue, iFullDlyMrk, int)
DEFpropSetMeth(qqueue, iDiscardSeverity, int)
DEFpropSetMeth(qqueue, bIsDA, int)
DEFpropSetMeth(qqueue, iMinMsgsPerWrkr, int)
DEFpropSetMeth(qqueue, bSaveOnShutdown, int)
DEFpropSetMeth(qqueue, pUsr, void*)
DEFpropSetMeth(qqueue, iDeqSlowdown, int)
DEFpropSetMeth(qqueue, iDeqBatchSize, int)
DEFpropSetMeth(qqueue, sizeOnDiskMax, int64)


/* This function can be used as a generic way to set properties. Only the subset
 * of properties required to read persisted property bags is supported. This
 * functions shall only be called by the property bag reader, thus it is static.
 * rgerhards, 2008-01-11
 */
#define isProp(name) !rsCStrSzStrCmp(pProp->pcsName, (uchar*) name, sizeof(name) - 1)
static rsRetVal qqueueSetProperty(qqueue_t *pThis, var_t *pProp)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, qqueue);
	ASSERT(pProp != NULL);

 	if(isProp("iQueueSize")) {
		pThis->iQueueSize = pProp->val.num;
 	} else if(isProp("tVars.disk.sizeOnDisk")) {
		pThis->tVars.disk.sizeOnDisk = pProp->val.num;
 	} else if(isProp("tVars.disk.bytesRead")) {
		pThis->tVars.disk.bytesRead = pProp->val.num;
 	} else if(isProp("qType")) {
		if(pThis->qType != pProp->val.num)
			ABORT_FINALIZE(RS_RET_QTYPE_MISMATCH);
	}

finalize_it:
	RETiRet;
}
#undef	isProp

/* dummy */
rsRetVal qqueueQueryInterface(void) { return RS_RET_NOT_IMPLEMENTED; }

/* Initialize the stream class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-01-09
 */
BEGINObjClassInit(qqueue, 1, OBJ_IS_CORE_MODULE)
	/* request objects we use */
	CHKiRet(objUse(glbl, CORE_COMPONENT));

	/* now set our own handlers */
	OBJSetMethodHandler(objMethod_SETPROPERTY, qqueueSetProperty);
ENDObjClassInit(qqueue)

/* vi:set ai:
 */
