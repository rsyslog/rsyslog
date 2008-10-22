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

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(glbl)

/* forward-definitions */
rsRetVal queueChkPersist(queue_t *pThis);
static rsRetVal queueSetEnqOnly(queue_t *pThis, int bEnqOnly, int bLockMutex);
static rsRetVal queueRateLimiter(queue_t *pThis);
static int queueChkStopWrkrDA(queue_t *pThis);
static int queueIsIdleDA(queue_t *pThis);
static rsRetVal queueConsumerDA(queue_t *pThis, wti_t *pWti, int iCancelStateSave);
static rsRetVal queueConsumerCancelCleanup(void *arg1, void *arg2);
static rsRetVal queueUngetObj(queue_t *pThis, obj_t *pUsr, int bLockMutex);

/* some constants for queuePersist () */
#define QUEUE_CHECKPOINT	1
#define QUEUE_NO_CHECKPOINT	0

/* methods */


/* get the overall queue size, which includes ungotten objects. Must only be called
 * while mutex is locked!
 * rgerhards, 2008-01-29
 */
static inline int
queueGetOverallQueueSize(queue_t *pThis)
{
#if 0 /* leave a bit in for debugging -- rgerhards, 2008-01-30 */
BEGINfunc
dbgoprint((obj_t*) pThis, "queue size: %d (regular %d, ungotten %d)\n",
	   pThis->iQueueSize + pThis->iUngottenObjs, pThis->iQueueSize, pThis->iUngottenObjs);
ENDfunc
#endif
	return pThis->iQueueSize + pThis->iUngottenObjs;
}


/* This function drains the queue in cases where this needs to be done. The most probable
 * reason is a HUP which needs to discard data (because the queue is configured to be lossy).
 * During a shutdown, this is typically not needed, as the OS frees up ressources and does
 * this much quicker than when we clean up ourselvs. -- rgerhards, 2008-10-21
 * This function returns void, as it makes no sense to communicate an error back, even if
 * it happens.
 */
static inline void queueDrain(queue_t *pThis)
{
	void *pUsr;
	
	ASSERT(pThis != NULL);

	/* iQueueSize is not decremented by qDel(), so we need to do it ourselves */
	while(pThis->iQueueSize-- > 0) {
		pThis->qDel(pThis, &pUsr);
		if(pUsr != NULL) {
			objDestruct(pUsr);
		}
	}
}


/* --------------- code for disk-assisted (DA) queue modes -------------------- */


/* returns the number of workers that should be advised at
 * this point in time. The mutex must be locked when
 * ths function is called. -- rgerhards, 2008-01-25
 */
static inline rsRetVal queueAdviseMaxWorkers(queue_t *pThis)
{
	DEFiRet;
	int iMaxWorkers;

	ISOBJ_TYPE_assert(pThis, queue);

	if(!pThis->bEnqOnly) {
		if(pThis->bRunsDA) {
			/* if we have not yet reached the high water mark, there is no need to start a
			 * worker. -- rgerhards, 2008-01-26
			 */
			if(queueGetOverallQueueSize(pThis) >= pThis->iHighWtrMrk || pThis->bQueueStarted == 0) {
				wtpAdviseMaxWorkers(pThis->pWtpDA, 1); /* disk queues have always one worker */
			}
		} else {
			if(pThis->qType == QUEUETYPE_DISK || pThis->iMinMsgsPerWrkr == 0) {
				iMaxWorkers = 1;
			} else {
				iMaxWorkers = queueGetOverallQueueSize(pThis) / pThis->iMinMsgsPerWrkr + 1;
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
queueWaitDAModeInitialized(queue_t *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, queue);
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
queueTurnOffDAMode(queue_t *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, queue);
	ASSERT(pThis->bRunsDA);

	/* at this point, we need a fully initialized DA queue. So if it isn't, we finally need
	 * to wait for its startup... -- rgerhards, 2008-01-25
	 */
	queueWaitDAModeInitialized(pThis);

	/* if we need to pull any data that we still need from the (child) disk queue,
	 * now would be the time to do so. At present, we do not need this, but I'd like to
	 * keep that comment if future need arises.
	 */

	/* we need to check if the DA queue is empty because the DA worker may simply have
	 * terminated do to no new messages arriving. That does not, however, mean that the
	 * DA queue is empty. If there is still data in that queue, we do nothing and leave
	 * that for a later incarnation of this function (it will be called multiple times
	 * during the lifetime of DA-mode, depending on how often the DA worker receives an
	 * inactivity timeout. -- rgerhards, 2008-01-25
	 */
	if(pThis->pqDA->iQueueSize == 0) {
		pThis->bRunsDA = 0; /* tell the world we are back in non-DA mode */
		/* we destruct the queue object, which will also shutdown the queue worker. As the queue is empty,
		 * this will be quick.
		 */
		queueDestruct(&pThis->pqDA); /* and now we are ready to destruct the DA queue */
		dbgoprint((obj_t*) pThis, "disk-assistance has been turned off, disk queue was empty (iRet %d)\n",
			  iRet);
		/* now we need to check if the regular queue has some messages. This may be the case
		 * when it is waiting that the high water mark is reached again. If so, we need to start up
		 * a regular worker. -- rgerhards, 2008-01-26
		 */
		if(queueGetOverallQueueSize(pThis) > 0) {
			queueAdviseMaxWorkers(pThis);
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
queueChkIsDA(queue_t *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, queue);
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
queueStartDA(queue_t *pThis)
{
	DEFiRet;
	uchar pszDAQName[128];

	ISOBJ_TYPE_assert(pThis, queue);

	if(pThis->bRunsDA == 2) /* check if already in (fully initialized) DA mode... */
		FINALIZE;       /* ... then we are already done! */

	/* create message queue */
	CHKiRet(queueConstruct(&pThis->pqDA, QUEUETYPE_DISK , 1, 0, pThis->pConsumer));

	/* give it a name */
	snprintf((char*) pszDAQName, sizeof(pszDAQName)/sizeof(uchar), "%s[DA]", obj.GetName((obj_t*) pThis));
	obj.SetName((obj_t*) pThis->pqDA, pszDAQName);

	/* as the created queue is the same object class, we take the
	 * liberty to access its properties directly.
	 */
	pThis->pqDA->pqParent = pThis;

	CHKiRet(queueSetpUsr(pThis->pqDA, pThis->pUsr));
	CHKiRet(queueSetsizeOnDiskMax(pThis->pqDA, pThis->sizeOnDiskMax));
	CHKiRet(queueSetiDeqSlowdown(pThis->pqDA, pThis->iDeqSlowdown));
	CHKiRet(queueSetMaxFileSize(pThis->pqDA, pThis->iMaxFileSize));
	CHKiRet(queueSetFilePrefix(pThis->pqDA, pThis->pszFilePrefix, pThis->lenFilePrefix));
	CHKiRet(queueSetiPersistUpdCnt(pThis->pqDA, pThis->iPersistUpdCnt));
	CHKiRet(queueSettoActShutdown(pThis->pqDA, pThis->toActShutdown));
	CHKiRet(queueSettoEnq(pThis->pqDA, pThis->toEnq));
	CHKiRet(queueSetEnqOnly(pThis->pqDA, pThis->bDAEnqOnly, MUTEX_ALREADY_LOCKED));
	CHKiRet(queueSetiDeqtWinFromHr(pThis->pqDA, pThis->iDeqtWinFromHr));
	CHKiRet(queueSetiDeqtWinToHr(pThis->pqDA, pThis->iDeqtWinToHr));
	CHKiRet(queueSetiHighWtrMrk(pThis->pqDA, 0));
	CHKiRet(queueSetiDiscardMrk(pThis->pqDA, 0));
	if(pThis->toQShutdown == 0) {
		CHKiRet(queueSettoQShutdown(pThis->pqDA, 0)); /* if the user really wants... */
	} else {
		/* we use the shortest possible shutdown (0 is endless!) because when we run on disk AND
		 * have an obviously large backlog, we can't finish it in any case. So there is no point
		 * in holding shutdown longer than necessary. -- rgerhards, 2008-01-15
		 */
		CHKiRet(queueSettoQShutdown(pThis->pqDA, 1));
	}

	iRet = queueStart(pThis->pqDA);
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
		  queueGetID(pThis->pqDA));

finalize_it:
	if(iRet != RS_RET_OK) {
		if(pThis->pqDA != NULL) {
			queueDestruct(&pThis->pqDA);
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
queueInitDA(queue_t *pThis, int bEnqOnly, int bLockMutex)
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
		CHKiRet(wtpSetpfChkStopWrkr	(pThis->pWtpDA, (rsRetVal (*)(void *pUsr, int)) queueChkStopWrkrDA));
		CHKiRet(wtpSetpfIsIdle		(pThis->pWtpDA, (rsRetVal (*)(void *pUsr, int)) queueIsIdleDA));
		CHKiRet(wtpSetpfDoWork		(pThis->pWtpDA, (rsRetVal (*)(void *pUsr, void *pWti, int)) queueConsumerDA));
		CHKiRet(wtpSetpfOnWorkerCancel	(pThis->pWtpDA, (rsRetVal (*)(void *pUsr, void*pWti)) queueConsumerCancelCleanup));
		CHKiRet(wtpSetpfOnWorkerStartup	(pThis->pWtpDA, (rsRetVal (*)(void *pUsr)) queueStartDA));
		CHKiRet(wtpSetpfOnWorkerShutdown(pThis->pWtpDA, (rsRetVal (*)(void *pUsr)) queueTurnOffDAMode));
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
queueChkStrtDA(queue_t *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, queue);

	/* if we do not hit the high water mark, we have nothing to do */
	if(queueGetOverallQueueSize(pThis) != pThis->iHighWtrMrk)
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
			  queueGetOverallQueueSize(pThis));
		queueAdviseMaxWorkers(pThis);
	} else {
		/* this is the case when we are currently not running in DA mode. So it is time
		 * to turn it back on.
		 */
		dbgoprint((obj_t*) pThis, "%d entries - passed high water mark for disk-assisted mode, initiating...\n",
			  queueGetOverallQueueSize(pThis));
		queueInitDA(pThis, QUEUE_MODE_ENQDEQ, MUTEX_ALREADY_LOCKED); /* initiate DA mode */
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
static rsRetVal qConstructFixedArray(queue_t *pThis)
{
	DEFiRet;

	ASSERT(pThis != NULL);

	if(pThis->iMaxQueueSize == 0)
		ABORT_FINALIZE(RS_RET_QSIZE_ZERO);

	if((pThis->tVars.farray.pBuf = malloc(sizeof(void *) * pThis->iMaxQueueSize)) == NULL) {
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}

	pThis->tVars.farray.head = 0;
	pThis->tVars.farray.tail = 0;

	queueChkIsDA(pThis);

finalize_it:
	RETiRet;
}


static rsRetVal qDestructFixedArray(queue_t *pThis)
{
	DEFiRet;
	
	ASSERT(pThis != NULL);

	queueDrain(pThis); /* discard any remaining queue entries */

	if(pThis->tVars.farray.pBuf != NULL)
		free(pThis->tVars.farray.pBuf);

	RETiRet;
}


static rsRetVal qAddFixedArray(queue_t *pThis, void* in)
{
	DEFiRet;

	ASSERT(pThis != NULL);
	pThis->tVars.farray.pBuf[pThis->tVars.farray.tail] = in;
	pThis->tVars.farray.tail++;
	if (pThis->tVars.farray.tail == pThis->iMaxQueueSize)
		pThis->tVars.farray.tail = 0;

	RETiRet;
}

static rsRetVal qDelFixedArray(queue_t *pThis, void **out)
{
	DEFiRet;

	ASSERT(pThis != NULL);
	*out = (void*) pThis->tVars.farray.pBuf[pThis->tVars.farray.head];

	pThis->tVars.farray.head++;
	if (pThis->tVars.farray.head == pThis->iMaxQueueSize)
		pThis->tVars.farray.head = 0;

	RETiRet;
}


/* -------------------- linked list  -------------------- */

/* first some generic functions which are also used for the unget linked list */

static inline rsRetVal queueAddLinkedList(qLinkedList_t **ppRoot, qLinkedList_t **ppLast, void* pUsr)
{
	DEFiRet;
	qLinkedList_t *pEntry;

	ASSERT(ppRoot != NULL);
	ASSERT(ppLast != NULL);

	if((pEntry = (qLinkedList_t*) malloc(sizeof(qLinkedList_t))) == NULL) {
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}

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

static inline rsRetVal queueDelLinkedList(qLinkedList_t **ppRoot, qLinkedList_t **ppLast, obj_t **ppUsr)
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


static rsRetVal qConstructLinkedList(queue_t *pThis)
{
	DEFiRet;

	ASSERT(pThis != NULL);

	pThis->tVars.linklist.pRoot = 0;
	pThis->tVars.linklist.pLast = 0;

	queueChkIsDA(pThis);

	RETiRet;
}


static rsRetVal qDestructLinkedList(queue_t __attribute__((unused)) *pThis)
{
	DEFiRet;

	queueDrain(pThis); /* discard any remaining queue entries */

	/* with the linked list type, there is nothing left to do here. The
	 * reason is that there are no dynamic elements for the list itself.
	 */

	RETiRet;
}

static rsRetVal qAddLinkedList(queue_t *pThis, void* pUsr)
{
	DEFiRet;

	iRet = queueAddLinkedList(&pThis->tVars.linklist.pRoot, &pThis->tVars.linklist.pLast, pUsr);
#if 0
	qLinkedList_t *pEntry;

	ASSERT(pThis != NULL);
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
#endif
	RETiRet;
}

static rsRetVal qDelLinkedList(queue_t *pThis, obj_t **ppUsr)
{
	DEFiRet;
	iRet = queueDelLinkedList(&pThis->tVars.linklist.pRoot, &pThis->tVars.linklist.pLast, ppUsr);
#if 0
	qLinkedList_t *pEntry;

	ASSERT(pThis != NULL);
	ASSERT(pThis->tVars.linklist.pRoot != NULL);
	
	pEntry = pThis->tVars.linklist.pRoot;
	*ppUsr = pEntry->pUsr;

	if(pThis->tVars.linklist.pRoot == pThis->tVars.linklist.pLast) {
		pThis->tVars.linklist.pRoot = NULL;
		pThis->tVars.linklist.pLast = NULL;
	} else {
		pThis->tVars.linklist.pRoot = pEntry->pNext;
	}
	free(pEntry);

#endif
	RETiRet;
}


/* -------------------- disk  -------------------- */


static rsRetVal
queueLoadPersStrmInfoFixup(strm_t *pStrm, queue_t __attribute__((unused)) *pThis)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pStrm, strm);
	ISOBJ_TYPE_assert(pThis, queue);
	CHKiRet(strmSetDir(pStrm, glbl.GetWorkDir(), strlen((char*)glbl.GetWorkDir())));
finalize_it:
	RETiRet;
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
queueTryLoadPersistedInfo(queue_t *pThis)
{
	DEFiRet;
	strm_t *psQIF = NULL;
	uchar pszQIFNam[MAXFNAME];
	size_t lenQIFNam;
	struct stat stat_buf;
	int iUngottenObjs;
	obj_t *pUsr;

	ISOBJ_TYPE_assert(pThis, queue);

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
	
	/* then the ungotten object queue */
	iUngottenObjs = pThis->iUngottenObjs;
	pThis->iUngottenObjs = 0; /* will be incremented when we add objects! */

	while(iUngottenObjs > 0) {
		/* fill the queue from disk */
		CHKiRet(obj.Deserialize((void*) &pUsr, (uchar*)"msg", psQIF, NULL, NULL));
		queueUngetObj(pThis, pUsr, MUTEX_ALREADY_LOCKED);
		--iUngottenObjs; /* one less */
	}

	/* and now the stream objects (some order as when persisted!) */
	CHKiRet(obj.Deserialize(&pThis->tVars.disk.pWrite, (uchar*) "strm", psQIF,
			       (rsRetVal(*)(obj_t*,void*))queueLoadPersStrmInfoFixup, pThis));
	CHKiRet(obj.Deserialize(&pThis->tVars.disk.pRead, (uchar*) "strm", psQIF,
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
static rsRetVal qConstructDisk(queue_t *pThis)
{
	DEFiRet;
	int bRestarted = 0;

	ASSERT(pThis != NULL);

	/* and now check if there is some persistent information that needs to be read in */
	iRet = queueTryLoadPersistedInfo(pThis);
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

		CHKiRet(strmConstruct(&pThis->tVars.disk.pRead));
		CHKiRet(strmSetbDeleteOnClose(pThis->tVars.disk.pRead, 1));
		CHKiRet(strmSetDir(pThis->tVars.disk.pRead, glbl.GetWorkDir(), strlen((char*)glbl.GetWorkDir())));
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
	RETiRet;
}


static rsRetVal qDestructDisk(queue_t *pThis)
{
	DEFiRet;
	
	ASSERT(pThis != NULL);
	
	strmDestruct(&pThis->tVars.disk.pWrite);
	strmDestruct(&pThis->tVars.disk.pRead);

	RETiRet;
}

static rsRetVal qAddDisk(queue_t *pThis, void* pUsr)
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

static rsRetVal qDelDisk(queue_t *pThis, void **ppUsr)
{
	DEFiRet;

	int64 offsIn;
	int64 offsOut;

	CHKiRet(strmGetCurrOffset(pThis->tVars.disk.pRead, &offsIn));
	CHKiRet(obj.Deserialize(ppUsr, (uchar*) "msg", pThis->tVars.disk.pRead, NULL, NULL));
	CHKiRet(strmGetCurrOffset(pThis->tVars.disk.pRead, &offsOut));

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

	ASSERT(pThis != NULL);

	/* calling the consumer is quite different here than it is from a worker thread */
	/* we need to provide the consumer's return value back to the caller because in direct
	 * mode the consumer probably has a lot to convey (which get's lost in the other modes
	 * because they are asynchronous. But direct mode is deliberately synchronous.
	 * rgerhards, 2008-02-12
	 */
	iRet = pThis->pConsumer(pThis->pUsr, pUsr);

	RETiRet;
}

static rsRetVal qDelDirect(queue_t __attribute__((unused)) *pThis, __attribute__((unused)) void **out)
{
	return RS_RET_OK;
}


/* --------------- end type-specific handlers -------------------- */


/* unget a user pointer that has been dequeued. This functionality is especially important
 * for consumer cancel cleanup handlers. To support it, a short list of ungotten user pointers
 * is maintened in memory.
 * rgerhards, 2008-01-20
 */
static rsRetVal
queueUngetObj(queue_t *pThis, obj_t *pUsr, int bLockMutex)
{
	DEFiRet;
	DEFVARS_mutexProtection;

	ISOBJ_TYPE_assert(pThis, queue);
	ISOBJ_assert(pUsr); /* TODO: we aborted right at this place at least 3 times -- race? 2008-02-28, -03-10, -03-15
			       The second time I noticed it the queue was in destruction with NO worker threads
			       running. The pUsr ptr was totally off and provided no clue what it may be pointing
			       at (except that it looked like the static data pool). Both times, the abort happend
			       inside an action queue */

	dbgoprint((obj_t*) pThis, "ungetting user object %s\n", obj.GetName(pUsr));
	BEGIN_MTX_PROTECTED_OPERATIONS(pThis->mut, bLockMutex);
	iRet = queueAddLinkedList(&pThis->pUngetRoot, &pThis->pUngetLast, pUsr);
	++pThis->iUngottenObjs;	/* indicate one more */
	END_MTX_PROTECTED_OPERATIONS(pThis->mut);

	RETiRet;
}


/* dequeues a user pointer from the ungotten queue. Pointers from there should always be
 * dequeued first.
 *
 * This function must only be called when the mutex is locked!
 *
 * rgerhards, 2008-01-29
 */
static rsRetVal
queueGetUngottenObj(queue_t *pThis, obj_t **ppUsr)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, queue);
	ASSERT(ppUsr != NULL);

	iRet = queueDelLinkedList(&pThis->pUngetRoot, &pThis->pUngetLast, ppUsr);
	--pThis->iUngottenObjs;	/* indicate one less */
	dbgoprint((obj_t*) pThis, "dequeued ungotten user object %s\n", obj.GetName(*ppUsr));

	RETiRet;
}


/* generic code to add a queue entry
 * We use some specific code to most efficiently support direct mode
 * queues. This is justified in spite of the gain and the need to do some
 * things truely different. -- rgerhards, 2008-02-12
 */
static rsRetVal
queueAdd(queue_t *pThis, void *pUsr)
{
	DEFiRet;

	ASSERT(pThis != NULL);

	CHKiRet(pThis->qAdd(pThis, pUsr));

	if(pThis->qType != QUEUETYPE_DIRECT) {
		ATOMIC_INC(pThis->iQueueSize);
		dbgoprint((obj_t*) pThis, "entry added, size now %d entries\n", pThis->iQueueSize);
	}

finalize_it:
	RETiRet;
}


/* generic code to remove a queue entry
 * rgerhards, 2008-01-29: we must first see if there is any object in the
 * ungotten list and, if so, dequeue it first.
 */
static rsRetVal
queueDel(queue_t *pThis, void *pUsr)
{
	DEFiRet;

	ASSERT(pThis != NULL);

	/* we do NOT abort if we encounter an error, because otherwise the queue
	 * will not be decremented, what will most probably result in an endless loop.
	 * If we decrement, however, we may lose a message. But that is better than
	 * losing the whole process because it loops... -- rgerhards, 2008-01-03
	 */
	if(pThis->iUngottenObjs > 0) {
		iRet = queueGetUngottenObj(pThis, (obj_t**) pUsr);
	} else {
		iRet = pThis->qDel(pThis, pUsr);
		ATOMIC_DEC(pThis->iQueueSize);
	}

	dbgoprint((obj_t*) pThis, "entry deleted, state %d, size now %d entries\n",
		  iRet, pThis->iQueueSize);

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
static rsRetVal queueShutdownWorkers(queue_t *pThis)
{
	DEFiRet;
	DEFVARS_mutexProtection;
	struct timespec tTimeout;
	rsRetVal iRetLocal;

	ISOBJ_TYPE_assert(pThis, queue);
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
	if(queueGetOverallQueueSize(pThis) > 0) {
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
				 queueGetID(pThis->pqDA));
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
		queueWaitDAModeInitialized(pThis);

	BEGIN_MTX_PROTECTED_OPERATIONS(pThis->mut, LOCK_MUTEX);	/* some workers may be running in parallel! */
	/* optimize parameters for shutdown of DA-enabled queues */
	if(pThis->bIsDA && queueGetOverallQueueSize(pThis) > 0 && pThis->bSaveOnShutdown) {
		/* switch to enqueue-only mode so that no more actions happen */
		if(pThis->bRunsDA == 0) {
			queueInitDA(pThis, QUEUE_MODE_ENQONLY, MUTEX_ALREADY_LOCKED); /* switch to DA mode */
		} else {
			/* TODO: RACE: we may reach this point when the DA worker has been initialized (state 1)
			 * but is not yet running (state 2). In this case, pThis->pqDA is NULL! rgerhards, 2008-02-27
			 */
			queueSetEnqOnly(pThis->pqDA, QUEUE_MODE_ENQONLY, MUTEX_ALREADY_LOCKED); /* switch to enqueue-only mode */
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
	if(queueGetOverallQueueSize(pThis) > 0) {
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
	dbgoprint((obj_t*) pThis, "worker threads terminated, remaining queue size %d.\n", queueGetOverallQueueSize(pThis));

	RETiRet;
}



/* Constructor for the queue object
 * This constructs the data structure, but does not yet start the queue. That
 * is done by queueStart(). The reason is that we want to give the caller a chance
 * to modify some parameters before the queue is actually started.
 */
rsRetVal queueConstruct(queue_t **ppThis, queueType_t qType, int iWorkerThreads,
		        int iMaxQueueSize, rsRetVal (*pConsumer)(void*,void*))
{
	DEFiRet;
	queue_t *pThis;

	ASSERT(ppThis != NULL);
	ASSERT(pConsumer != NULL);
	ASSERT(iWorkerThreads >= 0);

	if((pThis = (queue_t *)calloc(1, sizeof(queue_t))) == NULL) {
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}

	/* we have an object, so let's fill the properties */
	objConstructSetObjInfo(pThis);
	pThis->bOptimizeUniProc = glbl.GetOptimizeUniProc();
	if((pThis->pszSpoolDir = (uchar*) strdup((char*)glbl.GetWorkDir())) == NULL)
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

	/* set some water marks so that we have useful defaults if none are set specifically */
	pThis->iFullDlyMrk = (iMaxQueueSize < 100) ? iMaxQueueSize : 100; /* 100 should be far sufficient */
	pThis->iLightDlyMrk = iMaxQueueSize - (iMaxQueueSize / 100) * 70; /* default 70% */

	pThis->lenSpoolDir = strlen((char*)pThis->pszSpoolDir);
	pThis->iMaxFileSize = 1024 * 1024; /* default is 1 MiB */
	pThis->iQueueSize = 0;
	pThis->iMaxQueueSize = iMaxQueueSize;
	pThis->pConsumer = pConsumer;
	pThis->iNumWorkerThreads = iWorkerThreads;
	pThis->iDeqtWinToHr = 25; /* disable time-windowed dequeuing by default */

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
			pThis->qDel = (rsRetVal (*)(queue_t*,void**)) qDelLinkedList;
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
	RETiRet;
}


/* cancellation cleanup handler for queueWorker ()
 * Updates admin structure and frees ressources.
 * Params:
 * arg1 - user pointer (in this case a queue_t)
 * arg2 - user data pointer (in this case a queue data element, any object [queue's pUsr ptr!])
 * Note that arg2 may be NULL, in which case no dequeued but unprocessed pUsr exists!
 * rgerhards, 2008-01-16
 */
static rsRetVal
queueConsumerCancelCleanup(void *arg1, void *arg2)
{
	DEFiRet;

	queue_t *pThis = (queue_t*) arg1;
	obj_t *pUsr = (obj_t*) arg2;

	ISOBJ_TYPE_assert(pThis, queue);

	if(pUsr != NULL) {
		/* make sure the data element is not lost */
		dbgoprint((obj_t*) pThis, "cancelation cleanup handler consumer called, we need to unget one user data element\n");
		CHKiRet(queueUngetObj(pThis, pUsr, LOCK_MUTEX));
	}
	
finalize_it:
	RETiRet;
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
static int queueChkDiscardMsg(queue_t *pThis, int iQueueSize, int bRunsDA, void *pUsr)
{
	DEFiRet;
	rsRetVal iRetLocal;
	int iSeverity;

	ISOBJ_TYPE_assert(pThis, queue);
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


/* dequeue the queued object for the queue consumers.
 * rgerhards, 2008-10-21
 */
static rsRetVal
queueDequeueConsumable(queue_t *pThis, wti_t *pWti, int iCancelStateSave)
{
	DEFiRet;
	void *pUsr;
	int iQueueSize;
	int bRunsDA;	 /* cache for early mutex release */

	/* dequeue element (still protected from mutex) */
	iRet = queueDel(pThis, &pUsr);
	queueChkPersist(pThis);
	iQueueSize = queueGetOverallQueueSize(pThis); /* cache this for after mutex release */
	bRunsDA = pThis->bRunsDA; /* cache this for after mutex release */

	/* We now need to save the user pointer for the cancel cleanup handler, BUT ONLY
	 * if we could successfully obtain a user pointer. Otherwise, we would bring the
	 * cancel cleanup handler into big troubles (and we did ;)). Note that we can
	 * NOT set the variable further below, as this may lead to an object leak. We 
	 * may get cancelled before we reach that part of the code, so the only 
	 * solution is to do it here. -- rgerhards, 2008-02-27
	 */
	if(iRet == RS_RET_OK) {
		pWti->pUsrp = pUsr;
	}

	/* awake some flow-controlled sources if we can do this right now */
	/* TODO: this could be done better from a performance point of view -- do it only if
	 * we have someone waiting for the condition (or only when we hit the watermark right
	 * on the nail [exact value]) -- rgerhards, 2008-03-14
	 */
	if(iQueueSize < pThis->iFullDlyMrk) {
		pthread_cond_broadcast(&pThis->belowFullDlyWtrMrk);
	}

	if(iQueueSize < pThis->iLightDlyMrk) {
		pthread_cond_broadcast(&pThis->belowLightDlyWtrMrk);
	}

	/* rgerhards, 2008-09-30: I reversed the order of cond_signal und mutex_unlock
	 * as of the pthreads recommendation on predictable scheduling behaviour. I don't see
	 * any problems caused by this, but I add this comment in case some will be seen
	 * in the next time.
	 */
	pthread_cond_signal(&pThis->notFull);
	d_pthread_mutex_unlock(pThis->mut);
	pthread_setcancelstate(iCancelStateSave, NULL);
	/* WE ARE NO LONGER PROTECTED BY THE MUTEX */

	/* do actual processing (the lengthy part, runs in parallel)
	 * If we had a problem while dequeing, we do not call the consumer,
	 * but we otherwise ignore it. This is in the hopes that it will be
	 * self-healing. However, this is really not a good thing.
	 * rgerhards, 2008-01-03
	 */
	if(iRet != RS_RET_OK)
		FINALIZE;

	/* we are running in normal, non-disk-assisted mode do a quick check if we need to drain the queue.
	 * In DA mode, we do not discard any messages as we assume the disk subsystem is fast enough to
	 * provide real-time creation of spool files.
	 * Note: It is OK to use the cached iQueueSize here, because it does not hurt if it is slightly wrong.
	 */
	CHKiRet(queueChkDiscardMsg(pThis, iQueueSize, bRunsDA, pUsr));

finalize_it:
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
queueRateLimiter(queue_t *pThis)
{
	DEFiRet;
	int iDelay;
	int iHrCurr;
	time_t tCurr;
	struct tm m;

	ISOBJ_TYPE_assert(pThis, queue);

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
queueConsumerReg(queue_t *pThis, wti_t *pWti, int iCancelStateSave)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, queue);
	ISOBJ_TYPE_assert(pWti, wti);

	CHKiRet(queueDequeueConsumable(pThis, pWti, iCancelStateSave));
	CHKiRet(pThis->pConsumer(pThis->pUsr, pWti->pUsrp));

	/* we now need to check if we should deliberately delay processing a bit
	 * and, if so, do that. -- rgerhards, 2008-01-30
	 */
	if(pThis->iDeqSlowdown) {
		dbgoprint((obj_t*) pThis, "sleeping %d microseconds as requested by config params\n",
			  pThis->iDeqSlowdown);
		srSleep(pThis->iDeqSlowdown / 1000000, pThis->iDeqSlowdown % 1000000);
	}

finalize_it:
	RETiRet;
}


/* This is a special consumer to feed the disk-queue in disk-assited mode.
 * When active, our own queue more or less acts as a memory buffer to the disk.
 * So this consumer just needs to drain the memory queue and submit entries
 * to the disk queue. The disk queue will then call the actual consumer from
 * the app point of view (we chain two queues here).
 * When this method is entered, the mutex is always locked and needs to be unlocked
 * as part of the processing.
 * rgerhards, 2008-01-14
 */
static rsRetVal
queueConsumerDA(queue_t *pThis, wti_t *pWti, int iCancelStateSave)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, queue);
	ISOBJ_TYPE_assert(pWti, wti);

	CHKiRet(queueDequeueConsumable(pThis, pWti, iCancelStateSave));
	CHKiRet(queueEnqObj(pThis->pqDA, eFLOWCTL_NO_DELAY, pWti->pUsrp));

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
queueChkStopWrkrDA(queue_t *pThis)
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
			} else if(queueGetOverallQueueSize(pThis) < pThis->iHighWtrMrk && pThis->bQueueStarted == 1) {
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
queueChkStopWrkrReg(queue_t *pThis)
{
	return pThis->bEnqOnly || pThis->bRunsDA || (pThis->pqParent != NULL && queueGetOverallQueueSize(pThis) == 0);
}


/* must only be called when the queue mutex is locked, else results
 * are not stable! DA queue version
 */
static int
queueIsIdleDA(queue_t *pThis)
{
	/* remember: iQueueSize is the DA queue size, not the main queue! */
	/* TODO: I think we need just a single function for DA and non-DA mode - but I leave it for now as is */
	return(queueGetOverallQueueSize(pThis) == 0 || (pThis->bRunsDA && queueGetOverallQueueSize(pThis) <= pThis->iLowWtrMrk));
}
/* must only be called when the queue mutex is locked, else results
 * are not stable! Regular queue version
 */
static int
queueIsIdleReg(queue_t *pThis)
{
#if 0 /* enable for performance testing */
	int ret;
	ret = queueGetOverallQueueSize(pThis) == 0 || (pThis->bRunsDA && queueGetOverallQueueSize(pThis) <= pThis->iLowWtrMrk);
	if(ret) fprintf(stderr, "queue is idle\n");
	return ret;
#else 
	/* regular code! */
	return(queueGetOverallQueueSize(pThis) == 0 || (pThis->bRunsDA && queueGetOverallQueueSize(pThis) <= pThis->iLowWtrMrk));
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
queueRegOnWrkrShutdown(queue_t *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, queue);

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
queueRegOnWrkrStartup(queue_t *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, queue);

	if(pThis->pqParent != NULL) {
		pThis->pqParent->bChildIsDone = 0;
	}

	RETiRet;
}


/* start up the queue - it must have been constructed and parameters defined
 * before.
 */
rsRetVal queueStart(queue_t *pThis) /* this is the ConstructionFinalizer */
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

	dbgoprint((obj_t*) pThis, "type %d, enq-only %d, disk assisted %d, maxFileSz %lld, qsize %d, child %d starting\n",
		  pThis->qType, pThis->bEnqOnly, pThis->bIsDA, pThis->iMaxFileSize,
		  queueGetOverallQueueSize(pThis), pThis->pqParent == NULL ? 0 : 1);

	if(pThis->qType == QUEUETYPE_DIRECT)
		FINALIZE;	/* with direct queues, we are already finished... */

	/* create worker thread pools for regular operation. The DA pool is created on an as-needed
	 * basis, which potentially means never under most circumstances.
	 */
	lenBuf = snprintf((char*)pszBuf, sizeof(pszBuf), "%s:Reg", obj.GetName((obj_t*) pThis));
	CHKiRet(wtpConstruct		(&pThis->pWtpReg));
	CHKiRet(wtpSetDbgHdr		(pThis->pWtpReg, pszBuf, lenBuf));
	CHKiRet(wtpSetpfRateLimiter	(pThis->pWtpReg, (rsRetVal (*)(void *pUsr)) queueRateLimiter));
	CHKiRet(wtpSetpfChkStopWrkr	(pThis->pWtpReg, (rsRetVal (*)(void *pUsr, int)) queueChkStopWrkrReg));
	CHKiRet(wtpSetpfIsIdle		(pThis->pWtpReg, (rsRetVal (*)(void *pUsr, int)) queueIsIdleReg));
	CHKiRet(wtpSetpfDoWork		(pThis->pWtpReg, (rsRetVal (*)(void *pUsr, void *pWti, int)) queueConsumerReg));
	CHKiRet(wtpSetpfOnWorkerCancel	(pThis->pWtpReg, (rsRetVal (*)(void *pUsr, void*pWti))queueConsumerCancelCleanup));
	CHKiRet(wtpSetpfOnWorkerStartup	(pThis->pWtpReg, (rsRetVal (*)(void *pUsr)) queueRegOnWrkrStartup));
	CHKiRet(wtpSetpfOnWorkerShutdown(pThis->pWtpReg, (rsRetVal (*)(void *pUsr)) queueRegOnWrkrShutdown));
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
		iRetLocal = queueHaveQIF(pThis);
		if(iRetLocal == RS_RET_OK) {
			dbgoprint((obj_t*) pThis, "on-disk queue present, needs to be reloaded\n");
			queueInitDA(pThis, QUEUE_MODE_ENQDEQ, LOCK_MUTEX); /* initiate DA mode */
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
	queueAdviseMaxWorkers(pThis);
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
static rsRetVal queuePersist(queue_t *pThis, int bIsCheckpoint)
{
	DEFiRet;
	strm_t *psQIF = NULL; /* Queue Info File */
	uchar pszQIFNam[MAXFNAME];
	size_t lenQIFNam;
	obj_t *pUsr;

	ASSERT(pThis != NULL);

	if(pThis->qType != QUEUETYPE_DISK) {
		if(queueGetOverallQueueSize(pThis) > 0) {
			/* This error code is OK, but we will probably not implement this any time
 			 * The reason is that persistence happens via DA queues. But I would like to
			 * leave the code as is, as we so have a hook in case we need one.
			 * -- rgerhards, 2008-01-28
			 */
			ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
		} else
			FINALIZE; /* if the queue is empty, we are happy and done... */
	}

	dbgoprint((obj_t*) pThis, "persisting queue to disk, %d entries...\n", queueGetOverallQueueSize(pThis));

	/* Construct file name */
	lenQIFNam = snprintf((char*)pszQIFNam, sizeof(pszQIFNam) / sizeof(uchar), "%s/%s.qi",
			     (char*) glbl.GetWorkDir(), (char*)pThis->pszFilePrefix);

	if((bIsCheckpoint != QUEUE_CHECKPOINT) && (queueGetOverallQueueSize(pThis) == 0)) {
		if(pThis->bNeedDelQIF) {
			unlink((char*)pszQIFNam);
			pThis->bNeedDelQIF = 0;
		}
		/* indicate spool file needs to be deleted */
		CHKiRet(strmSetbDeleteOnClose(pThis->tVars.disk.pRead, 1));
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
	objSerializeSCALAR(psQIF, iUngottenObjs, INT);
	objSerializeSCALAR(psQIF, tVars.disk.sizeOnDisk, INT64);
	objSerializeSCALAR(psQIF, tVars.disk.bytesRead, INT64);
	CHKiRet(obj.EndSerialize(psQIF));

	/* now we must persist all objects on the ungotten queue - they can not go to
	 * to the regular files. -- rgerhards, 2008-01-29
	 */
	while(pThis->iUngottenObjs > 0) {
		CHKiRet(queueGetUngottenObj(pThis, &pUsr));
		CHKiRet((objSerialize(pUsr))(pUsr, psQIF));
		objDestruct(pUsr);
	}

	/* now persist the stream info */
	CHKiRet(strmSerialize(pThis->tVars.disk.pWrite, psQIF));
	CHKiRet(strmSerialize(pThis->tVars.disk.pRead, psQIF));
	
	/* tell the input file object that it must not delete the file on close if the queue
	 * is non-empty - but only if we are not during a simple checkpoint
	 */
	if(bIsCheckpoint != QUEUE_CHECKPOINT) {
		CHKiRet(strmSetbDeleteOnClose(pThis->tVars.disk.pRead, 0));
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
 * error occurs, thus should be ignored by caller (but we still
 * abide to our regular call interface)...
 * rgerhards, 2008-01-13
 */
rsRetVal queueChkPersist(queue_t *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, queue);

	if(pThis->iPersistUpdCnt && ++pThis->iUpdsSincePersist >= pThis->iPersistUpdCnt) {
		queuePersist(pThis, QUEUE_CHECKPOINT);
		pThis->iUpdsSincePersist = 0;
	}

	RETiRet;
}


/* destructor for the queue object */
BEGINobjDestruct(queue) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(queue)
	pThis->bQueueInDestruction = 1; /* indicate we are in destruction (modifies some behaviour) */

	/* shut down all workers (handles *all* of the persistence logic)
	 * See function head comment of queueShutdownWorkers () on why we don't call it
	 * We also do not need to shutdown workers when we are in enqueue-only mode or we are a
	 * direct queue - because in both cases we have none... ;)
	 * with a child! -- rgerhards, 2008-01-28
	 */
	if(pThis->qType != QUEUETYPE_DIRECT && !pThis->bEnqOnly && pThis->pqParent == NULL)
		queueShutdownWorkers(pThis);

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
		queueDestruct(&pThis->pqDA);
	}

	/* persist the queue (we always do that - queuePersits() does cleanup if the queue is empty)
	 * This handler is most important for disk queues, it will finally persist the necessary
	 * on-disk structures. In theory, other queueing modes may implement their other (non-DA)
	 * methods of persisting a queue between runs, but in practice all of this is done via
	 * disk queues and DA mode. Anyhow, it doesn't hurt to know that we could extend it here
	 * if need arises (what I doubt...) -- rgerhards, 2008-01-25
	 */
	CHKiRet_Hdlr(queuePersist(pThis, QUEUE_NO_CHECKPOINT)) {
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

	if(pThis->pszFilePrefix != NULL)
		free(pThis->pszFilePrefix);

	if(pThis->pszSpoolDir != NULL)
		free(pThis->pszSpoolDir);
ENDobjDestruct(queue)


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
	RETiRet;
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
	RETiRet;
}


/* enqueue a new user data element
 * Enqueues the new element and awakes worker thread.
 */
rsRetVal
queueEnqObj(queue_t *pThis, flowControl_t flowCtlType, void *pUsr)
{
	DEFiRet;
	int iCancelStateSave;
	struct timespec t;

	ISOBJ_TYPE_assert(pThis, queue);

	/* first check if we need to discard this message (which will cause CHKiRet() to exit)
	 * rgerhards, 2008-10-07: It is OK to do this outside of mutex protection. The iQueueSize
	 * and bRunsDA parameters may not reflect the correct settings here, but they are
	 * "good enough" in the sense that they can be used to drive the decision. Valgrind's
	 * threading tools may point this access to be an error, but this is done
	 * intentional. I do not see this causes problems to us.
	 */
	CHKiRet(queueChkDiscardMsg(pThis, pThis->iQueueSize, pThis->bRunsDA, pUsr));

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
		CHKiRet(queueChkStrtDA(pThis));
	
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
		while(pThis->iQueueSize >= pThis->iFullDlyMrk) {
			dbgoprint((obj_t*) pThis, "enqueueMsg: FullDelay mark reached for full delayble message - blocking.\n");
			pthread_cond_wait(&pThis->belowFullDlyWtrMrk, pThis->mut); /* TODO error check? But what do then? */
		}
	} else if(flowCtlType == eFLOWCTL_LIGHT_DELAY) {
		if(pThis->iQueueSize >= pThis->iLightDlyMrk) {
			dbgoprint((obj_t*) pThis, "enqueueMsg: LightDelay mark reached for light delayble message - blocking a bit.\n");
			timeoutComp(&t, 1000); /* 1000 millisconds = 1 second TODO: make configurable */
			pthread_cond_timedwait(&pThis->belowLightDlyWtrMrk, pThis->mut, &t); /* TODO error check? But what do then? */
		}
	}

	/* from our regular flow control settings, we are now ready to enqueue the object.
	 * However, we now need to do a check if the queue permits to add more data. If that
	 * is not the case, basic flow control enters the field, which means we wait for
	 * the queue to become ready or drop the new message. -- rgerhards, 2008-03-14
	 */
	while(   (pThis->iMaxQueueSize > 0 && pThis->iQueueSize >= pThis->iMaxQueueSize)
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
	CHKiRet(queueAdd(pThis, pUsr));
	queueChkPersist(pThis);

finalize_it:
	if(pThis->qType != QUEUETYPE_DIRECT) {
		/* make sure at least one worker is running. */
		queueAdviseMaxWorkers(pThis);
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
queueSetEnqOnly(queue_t *pThis, int bEnqOnly, int bLockMutex)
{
	DEFiRet;
	DEFVARS_mutexProtection;

	ISOBJ_TYPE_assert(pThis, queue);

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
DEFpropSetMeth(queue, iPersistUpdCnt, int)
DEFpropSetMeth(queue, iDeqtWinFromHr, int)
DEFpropSetMeth(queue, iDeqtWinToHr, int)
DEFpropSetMeth(queue, toQShutdown, long)
DEFpropSetMeth(queue, toActShutdown, long)
DEFpropSetMeth(queue, toWrkShutdown, long)
DEFpropSetMeth(queue, toEnq, long)
DEFpropSetMeth(queue, iHighWtrMrk, int)
DEFpropSetMeth(queue, iLowWtrMrk, int)
DEFpropSetMeth(queue, iDiscardMrk, int)
DEFpropSetMeth(queue, iFullDlyMrk, int)
DEFpropSetMeth(queue, iDiscardSeverity, int)
DEFpropSetMeth(queue, bIsDA, int)
DEFpropSetMeth(queue, iMinMsgsPerWrkr, int)
DEFpropSetMeth(queue, bSaveOnShutdown, int)
DEFpropSetMeth(queue, pUsr, void*)
DEFpropSetMeth(queue, iDeqSlowdown, int)
DEFpropSetMeth(queue, sizeOnDiskMax, int64)


/* This function can be used as a generic way to set properties. Only the subset
 * of properties required to read persisted property bags is supported. This
 * functions shall only be called by the property bag reader, thus it is static.
 * rgerhards, 2008-01-11
 */
#define isProp(name) !rsCStrSzStrCmp(pProp->pcsName, (uchar*) name, sizeof(name) - 1)
static rsRetVal queueSetProperty(queue_t *pThis, var_t *pProp)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, queue);
	ASSERT(pProp != NULL);

 	if(isProp("iQueueSize")) {
		pThis->iQueueSize = pProp->val.num;
 	} else if(isProp("iUngottenObjs")) {
		pThis->iUngottenObjs = pProp->val.num;
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
rsRetVal queueQueryInterface(void) { return RS_RET_NOT_IMPLEMENTED; }

/* Initialize the stream class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-01-09
 */
BEGINObjClassInit(queue, 1, OBJ_IS_CORE_MODULE)
	/* request objects we use */
	CHKiRet(objUse(glbl, CORE_COMPONENT));

	/* now set our own handlers */
	OBJSetMethodHandler(objMethod_SETPROPERTY, queueSetProperty);
ENDObjClassInit(queue)

/* vi:set ai:
 */
