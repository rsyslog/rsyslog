	// TODO: we need to implement peek(), without it (today!) we lose one message upon
	// worker cancellation! -- rgerhards, 2008-01-14
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
#include "wtp.h"
#include "wti.h"

/* static data */
DEFobjStaticHelpers

/* forward-definitions */
rsRetVal queueChkPersist(queue_t *pThis);
static rsRetVal queueSetEnqOnly(queue_t *pThis, int bEnqOnly, int bLockMutex);
static int queueChkStopWrkrDA(queue_t *pThis);
static int queueIsIdleDA(queue_t *pThis);
static rsRetVal queueConsumerDA(queue_t *pThis, wti_t *pWti, int iCancelStateSave);
static rsRetVal queueConsumerCancelCleanup(void *arg1, void *arg2);

/* methods */



/* --------------- code for disk-assisted (DA) queue modes -------------------- */


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
	assert(pThis->bRunsDA);

	/* at this point, we need a fully initialized DA queue. So if it isn't, we finally need
	 * to wait for its startup... -- rgerhards, 2008-01-25
	 */
	while(pThis->bRunsDA != 2) {
		d_pthread_cond_wait(&pThis->condDAReady, pThis->mut);
	}
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
RUNLOG_VAR("%p", pThis->pqDA);
RUNLOG_VAR("%d", pThis->pqDA->iQueueSize);
	if(pThis->pqDA->iQueueSize == 0) {
dbgprintf("Queue 0x%lx: disk-assistance being been turned off, bEnqOnly %d, bQueInDestr %d, NumWrkd %d\n",
	  queueGetID(pThis), 
	pThis->bEnqOnly,pThis->bQueueInDestruction,pThis->iCurNumWrkThrd);

		pThis->bRunsDA = 0; /* tell the world we are back in non-DA mode */
		/* we destruct the queue object, which will also shutdown the queue worker. As the queue is empty,
		 * this will be quick.
		 */
		queueDestruct(&pThis->pqDA); /* and now we are ready to destruct the DA queue */
		dbgprintf("Queue 0x%lx: disk-assistance has been turned off, disk queue was empty (iRet %d)\n",
			  queueGetID(pThis), iRet);
	}

	RETiRet;
}



/* returns the number of workers that should be advised at
 * this point in time. The mutex must be locked when
 * ths function is called. -- rgerhards, 2008-01-25
 */
static inline rsRetVal queueAdviseMaxWorkers(queue_t *pThis)
{
	DEFiRet;
	int iMaxWorkers;

	ISOBJ_TYPE_assert(pThis, queue);

RUNLOG_VAR("%d", pThis->bEnqOnly);
	if(!pThis->bEnqOnly) {
		if(pThis->bRunsDA) {
			wtpAdviseMaxWorkers(pThis->pWtpDA, 1); /* disk queues have always one worker */
		} else {
			if(pThis->qType == QUEUETYPE_DISK || pThis->iMinMsgsPerWrkr == 0) {
				iMaxWorkers = 1;
			} else {
				iMaxWorkers = pThis->iQueueSize / pThis->iMinMsgsPerWrkr + 1;
			}
			wtpAdviseMaxWorkers(pThis->pWtpReg, iMaxWorkers); /* disk queues have always one worker */
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
RUNLOG_VAR("%s", pThis->pszFilePrefix);
	if(pThis->pszFilePrefix != NULL) {
		pThis->bIsDA = 1;
		dbgprintf("Queue 0x%lx: is disk-assisted, disk will be used on demand\n", queueGetID(pThis));
	} else {
		dbgprintf("Queue 0x%lx: is NOT disk-assisted\n", queueGetID(pThis));
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

	ISOBJ_TYPE_assert(pThis, queue);

	if(pThis->bRunsDA == 2) /* check if already in (fully initialized) DA mode... */
		FINALIZE;       /* ... then we are already done! */

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
	pThis->pqDA->pqParent = pThis;

	CHKiRet(queueSetMaxFileSize(pThis->pqDA, pThis->iMaxFileSize));
	CHKiRet(queueSetFilePrefix(pThis->pqDA, pThis->pszFilePrefix, pThis->lenFilePrefix));
	CHKiRet(queueSetiPersistUpdCnt(pThis->pqDA, pThis->iPersistUpdCnt));
	CHKiRet(queueSettoActShutdown(pThis->pqDA, pThis->toActShutdown));
	CHKiRet(queueSettoEnq(pThis->pqDA, pThis->toEnq));
	CHKiRet(queueSetEnqOnly(pThis->pqDA, pThis->bDAEnqOnly, MUTEX_ALREADY_LOCKED));
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

dbgprintf("Queue %p: queueStartDA pre start\n", pThis);
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
	pthread_cond_signal(&pThis->condDAReady); /* signal we are now initialized and ready to go ;) */

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
		lenBuf = snprintf((char*)pszBuf, sizeof(pszBuf), "Queue 0x%lx:DA", (unsigned long) pThis);
		CHKiRet(wtpConstruct		(&pThis->pWtpDA));
		CHKiRet(wtpSetDbgHdr		(pThis->pWtpDA, pszBuf, lenBuf));
		CHKiRet(wtpSetpfChkStopWrkr	(pThis->pWtpDA, queueChkStopWrkrDA));
		CHKiRet(wtpSetpfIsIdle		(pThis->pWtpDA, queueIsIdleDA));
		CHKiRet(wtpSetpfDoWork		(pThis->pWtpDA, queueConsumerDA));
		CHKiRet(wtpSetpfOnWorkerCancel	(pThis->pWtpDA, queueConsumerCancelCleanup));
		CHKiRet(wtpSetpfOnWorkerStartup	(pThis->pWtpDA, queueStartDA));
		CHKiRet(wtpSetpfOnWorkerShutdown(pThis->pWtpDA, queueTurnOffDAMode));
		CHKiRet(wtpSetpmutUsr		(pThis->pWtpDA, pThis->mut));
		CHKiRet(wtpSetpcondBusy		(pThis->pWtpDA, &pThis->notEmpty));
		CHKiRet(wtpSetiNumWorkerThreads	(pThis->pWtpDA, 1));
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
	queueAdviseMaxWorkers(pThis);

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
	if(pThis->iQueueSize != pThis->iHighWtrMrk)
		ABORT_FINALIZE(RS_RET_OK);

dbgprintf("Queue %p: chkStartDA\n", pThis);
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
		dbgprintf("Queue 0x%lx: %d entries - passed high water mark in DA mode, send notify\n",
			  queueGetID(pThis), pThis->iQueueSize);
		queueAdviseMaxWorkers(pThis);
	} else {
		/* this is the case when we are currently not running in DA mode. So it is time
		 * to turn it back on.
		 */
		dbgprintf("Queue 0x%lx: %d entries - passed high water mark for disk-assisted mode, initiating...\n",
			  queueGetID(pThis), pThis->iQueueSize);
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
	RETiRet;
}


static rsRetVal qDestructFixedArray(queue_t *pThis)
{
	DEFiRet;
	
	assert(pThis != NULL);

	if(pThis->tVars.farray.pBuf != NULL)
		free(pThis->tVars.farray.pBuf);

	RETiRet;
}

static rsRetVal qAddFixedArray(queue_t *pThis, void* in)
{
	DEFiRet;

	assert(pThis != NULL);
	pThis->tVars.farray.pBuf[pThis->tVars.farray.tail] = in;
	pThis->tVars.farray.tail++;
	if (pThis->tVars.farray.tail == pThis->iMaxQueueSize)
		pThis->tVars.farray.tail = 0;

	RETiRet;
}

static rsRetVal qDelFixedArray(queue_t *pThis, void **out)
{
	DEFiRet;

	assert(pThis != NULL);
	*out = (void*) pThis->tVars.farray.pBuf[pThis->tVars.farray.head];

	pThis->tVars.farray.head++;
	if (pThis->tVars.farray.head == pThis->iMaxQueueSize)
		pThis->tVars.farray.head = 0;

	RETiRet;
}


/* -------------------- linked list  -------------------- */
static rsRetVal qConstructLinkedList(queue_t *pThis)
{
	DEFiRet;

	assert(pThis != NULL);

	pThis->tVars.linklist.pRoot = 0;
	pThis->tVars.linklist.pLast = 0;

	queueChkIsDA(pThis);

	RETiRet;
}


static rsRetVal qDestructLinkedList(queue_t __attribute__((unused)) *pThis)
{
	DEFiRet;
	
	/* with the linked list type, there is nothing to do here. The
	 * reason is that the Destructor is only called after all entries
	 * have bene taken off the queue. In this case, there is nothing
	 * dynamic left with the linked list.
	 */

	RETiRet;
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
	RETiRet;
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

	RETiRet;
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
	RETiRet;
}


static rsRetVal qDestructDisk(queue_t *pThis)
{
	DEFiRet;
	
	assert(pThis != NULL);
	
	strmDestruct(&pThis->tVars.disk.pWrite);
	strmDestruct(&pThis->tVars.disk.pRead);

	if(pThis->pszSpoolDir != NULL)
		free(pThis->pszSpoolDir);

	RETiRet;
}

static rsRetVal qAddDisk(queue_t *pThis, void* pUsr)
{
	DEFiRet;

	assert(pThis != NULL);

	CHKiRet((objSerialize(pUsr))(pUsr, pThis->tVars.disk.pWrite));
	CHKiRet(strmFlush(pThis->tVars.disk.pWrite));

finalize_it:
	RETiRet;
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

	RETiRet;
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
	RETiRet;
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

	RETiRet;
}


/* This function shuts down all worker threads and waits until they
 * have terminated. If they timeout, they are cancelled. Parameters have been set
 * before this function is called so that DA queues will be fully persisted to
 * disk (if configured to do so).
 * rgerhards, 2008-01-24
 */
static rsRetVal queueShutdownWorkers(queue_t *pThis)
{
	DEFiRet;
	DEFVARS_mutexProtection;
	struct timespec tTimeout;
	rsRetVal iRetLocal;

	ISOBJ_TYPE_assert(pThis, queue);

	dbgprintf("Queue 0x%lx: initiating worker thread shutdown sequence\n", queueGetID(pThis));

	// TODO: reminder, delte after testing: do we need to modify the high wtr mark? I dont' think so 2008-01-25
	/* we reduce the low water mark in any case. This is not absolutely necessary, but
	 * it is useful because we enable DA mode at several spots below and so we do not need
	 * to think about the low water mark each time. 
	 */
	pThis->iLowWtrMrk = 0;

	/* first try to shutdown the queue within the regular shutdown period */
	BEGIN_MTX_PROTECTED_OPERATIONS(pThis->mut, LOCK_MUTEX);	/* some workers may be running in parallel! */
	if(pThis->iQueueSize > 0) {
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
	iRetLocal = wtpShutdownAll(pThis->pWtpReg, wtpState_SHUTDOWN, &tTimeout);
	if(iRetLocal == RS_RET_TIMED_OUT) {
		dbgprintf("Queue 0x%lx: regular shutdown timed out on primary queue (this is OK)\n", queueGetID(pThis));
	} else {
		/* OK, the regular queue is now shut down. So we can now wait for the DA queue (if running DA) */
		dbgprintf("Queue 0x%lx: regular queue workers shut down.\n", queueGetID(pThis));
		BEGIN_MTX_PROTECTED_OPERATIONS(pThis->mut, LOCK_MUTEX);	/* some workers may be running in parallel! */
		if(pThis->bRunsDA) {
			END_MTX_PROTECTED_OPERATIONS(pThis->mut);
			dbgprintf("Queue 0x%lx: we have a DA queue (0x%lx), requesting its shutdown.\n",
				 queueGetID(pThis), queueGetID(pThis->pqDA));
			/* we use the same absolute timeout as above, so we do not use more than the configured
			 * timeout interval!
			 */
			iRetLocal = wtpShutdownAll(pThis->pWtpDA, wtpState_SHUTDOWN, &tTimeout);
			if(iRetLocal == RS_RET_TIMED_OUT) {
				dbgprintf("Queue 0x%lx: shutdown timed out on DA queue (this is OK)\n",
					  queueGetID(pThis));
			}
		} else {
			END_MTX_PROTECTED_OPERATIONS(pThis->mut);
		}
	}

	/* when we reach this point, both queues are either empty or the regular queue shutdown timeout
	 * has expired. Now we need to check if we are configured to not loose messages. If so, we need
	 * to persist the queue to disk (this is only possible if the queue is DA-enabled).
	 */
// TODO: what about pure disk queues and bSaveOnShutdown?
	BEGIN_MTX_PROTECTED_OPERATIONS(pThis->mut, LOCK_MUTEX);	/* some workers may be running in parallel! */
	/* optimize parameters for shutdown of DA-enabled queues */
RUNLOG_VAR("%d", pThis->bSaveOnShutdown);
RUNLOG_VAR("%d", pThis->bIsDA);
RUNLOG_VAR("%d", pThis->iQueueSize);
	if(pThis->bIsDA && pThis->iQueueSize > 0 && pThis->bSaveOnShutdown) {
RUNLOG;
		/* switch to enqueue-only mode so that no more actions happen */
		if(pThis->bRunsDA == 0) {
			queueInitDA(pThis, QUEUE_MODE_ENQONLY, MUTEX_ALREADY_LOCKED); /* switch to DA mode */
		} else {
			queueSetEnqOnly(pThis->pqDA, QUEUE_MODE_ENQONLY, MUTEX_ALREADY_LOCKED); /* switch to enqueue-only mode */
		}
		END_MTX_PROTECTED_OPERATIONS(pThis->mut);
		/* make sure we do not timeout before we are done */
		dbgprintf("Queue 0x%lx: bSaveOnShutdown configured, eternal timeout set\n", queueGetID(pThis));
		timeoutComp(&tTimeout, QUEUE_TIMEOUT_ETERNAL);
		/* and run the primary's queue worker to drain the queue */
		iRetLocal = wtpShutdownAll(pThis->pWtpReg, wtpState_SHUTDOWN, &tTimeout);
		if(iRetLocal != RS_RET_OK) {
			dbgprintf("Queue 0x%lx: unexpected iRet state %d after trying to shut down primary queue in disk save mode, "
				  "continuing, but results are unpredictable\n",
				  queueGetID(pThis), iRetLocal);
		}
	} else {
		END_MTX_PROTECTED_OPERATIONS(pThis->mut);
	}

RUNLOG;
	/* now the primary queue is either empty, persisted to disk - or set to loose messages. So we
	 * can now request immediate shutdown of any remaining workers.
	 */
	BEGIN_MTX_PROTECTED_OPERATIONS(pThis->mut, LOCK_MUTEX);	/* some workers may be running in parallel! */
RUNLOG_VAR("%d", pThis->iQueueSize);
	if(pThis->iQueueSize > 0) {
		END_MTX_PROTECTED_OPERATIONS(pThis->mut);
		timeoutComp(&tTimeout, QUEUE_TIMEOUT_ETERNAL);
		iRetLocal = wtpShutdownAll(pThis->pWtpReg, wtpState_SHUTDOWN_IMMEDIATE, &tTimeout);
		if(iRetLocal != RS_RET_OK) {
			dbgprintf("Queue 0x%lx: unexpected iRet state %d after trying immediate shutdown of the primary queue "
				  "in disk save mode. Continuing, but results are unpredictable\n",
				  queueGetID(pThis), iRetLocal);
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
	BEGIN_MTX_PROTECTED_OPERATIONS(pThis->mut, LOCK_MUTEX);	/* some workers may be running in parallel! */
RUNLOG_VAR("%d", pThis->iQueueSize);
	if(pThis->iQueueSize > 0) {
		END_MTX_PROTECTED_OPERATIONS(pThis->mut);
		dbgprintf("Queue 0x%lx: checking to see if we need to cancel any worker threads of the primary queue\n",
			  queueGetID(pThis));
		iRetLocal = wtpCancelAll(pThis->pWtpReg);
		if(iRetLocal != RS_RET_OK) {
			dbgprintf("Queue 0x%lx: unexpected iRet state %d trying to cancel primary queue worker "
				  "threads, continuing, but results are unpredictable\n",
				  queueGetID(pThis), iRetLocal);
		}
	} else {
		END_MTX_PROTECTED_OPERATIONS(pThis->mut);
	}

	/* ... and now the DA queue, if it exists (should always be after the primary one) */
	BEGIN_MTX_PROTECTED_OPERATIONS(pThis->mut, LOCK_MUTEX);	/* some workers may be running in parallel! */
	if(pThis->pqDA != NULL && pThis->pqDA->iQueueSize > 0) {
		END_MTX_PROTECTED_OPERATIONS(pThis->mut);
		dbgprintf("Queue 0x%lx: checking to see if we need to cancel any worker threads of the DA queue\n",
			  queueGetID(pThis));
		iRetLocal = wtpCancelAll(pThis->pWtpReg);
		if(iRetLocal != RS_RET_OK) {
			dbgprintf("Queue 0x%lx: unexpected iRet state %d trying to cancel DA queue worker "
				  "threads, continuing, but results are unpredictable\n",
				  queueGetID(pThis), iRetLocal);
		}
	} else {
		END_MTX_PROTECTED_OPERATIONS(pThis->mut);
	}

	/* ... finally ... all worker threads have terminated :-)
	 * Well, more precisely, they *are in termination*. Some cancel cleanup handlers
	 * may still be running. 
	 */
	dbgprintf("Queue 0x%lx: worker threads terminated, remaining queue size %d.\n",
		  queueGetID(pThis), pThis->iQueueSize);

	RETiRet;
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
	RETiRet;
}


/* cancellation cleanup handler for queueWorker ()
 * Updates admin structure and frees ressources.
 * rgerhards, 2008-01-16
 */
static rsRetVal
queueConsumerCancelCleanup(void *arg1, void *arg2)
{
	queue_t *pThis = (queue_t*) arg1;
	wti_t *pWti = (wti_t*) arg2;

	ISOBJ_TYPE_assert(pThis, queue);
	ISOBJ_TYPE_assert(pWti, wti);

	dbgprintf("Queue 0x%lx: cancelation cleanup handler consumer called (NOT FULLY IMPLEMENTED, one msg lost!)\n",
		  queueGetID(pThis));
	
	/* TODO: re-enqueue the data element! */

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
			dbgprintf("Queue 0x%lx: queue nearly full (%d entries), discarded severity %d message\n",
				  queueGetID(pThis), iQueueSize, iSeverity);
			objDestruct(pUsr);
			ABORT_FINALIZE(RS_RET_QUEUE_FULL);
		} else {
			dbgprintf("Queue 0x%lx: queue nearly full (%d entries), but could not drop msg "
				  "(iRet: %d, severity %d)\n", queueGetID(pThis), iQueueSize,
				  iRetLocal, iSeverity);
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
	queueChkPersist(pThis); // when we support peek(), we must do this down after the del!
	iQueueSize = pThis->iQueueSize; /* cache this for after mutex release */
	bRunsDA = pThis->bRunsDA; /* cache this for after mutex release */
	pWti->pUsrp = pUsr; /* save it for the cancel cleanup handler */
	d_pthread_mutex_unlock(pThis->mut);
	pthread_cond_signal(&pThis->notFull);
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
		dbgprintf("Queue 0x%lx/w?: error %d dequeueing element - ignoring, but strange things "
			  "may happen\n", queueGetID(pThis), iRet);
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
	CHKiRet(pThis->pConsumer(pWti->pUsrp));

finalize_it:
dbgprintf("Queue %p: regular consumer returns %d\n", pThis, iRet);
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

dbgprintf("Queue %p/w?: queueDAConsumer, queue size %d\n", pThis, pThis->iQueueSize);/* dirty iQueueSize! */
	CHKiRet(queueDequeueConsumable(pThis, pWti, iCancelStateSave));
	CHKiRet(queueEnqObj(pThis->pqDA, pWti->pUsrp));

finalize_it:
dbgprintf("DAConsumer returns with iRet %d\n", iRet);
	RETiRet;
}


/* must only be called when the queue mutex is locked, else results
 * are not stable!
 * Version when running in DA mode.
 */
static int
queueChkStopWrkrDA(queue_t *pThis)
{
	return pThis->bEnqOnly || !pThis->bRunsDA;
}

/* must only be called when the queue mutex is locked, else results
 * are not stable!
 * Version when running in non-DA mode.
 */
static int
queueChkStopWrkrReg(queue_t *pThis)
{
	return pThis->bEnqOnly || pThis->bRunsDA;
}


/* must only be called when the queue mutex is locked, else results
 * are not stable! DA queue version
 */
static int
queueIsIdleDA(queue_t *pThis)
{
	/* remember: iQueueSize is the DA queue size, not the main queue! */
RUNLOG_VAR("%d", pThis->iLowWtrMrk);
dbgprintf("queueIsIdleDA(%p) returns %d, qsize %d\n", pThis, pThis->iQueueSize == 0 || (pThis->bRunsDA && pThis->iQueueSize <= pThis->iLowWtrMrk), pThis->iQueueSize);
	//// TODO: I think we need just a single function...
	//return (pThis->iQueueSize == 0);
	return (pThis->iQueueSize == 0 || (pThis->bRunsDA && pThis->iQueueSize <= pThis->iLowWtrMrk));
}
/* must only be called when the queue mutex is locked, else results
 * are not stable! Regular queue version
 */
static int
queueIsIdleReg(queue_t *pThis)
{
	return (pThis->iQueueSize == 0 || (pThis->bRunsDA && pThis->iQueueSize <= pThis->iLowWtrMrk));
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

	assert(pThis != NULL);

	/* finalize some initializations that could not yet be done because it is
	 * influenced by properties which might have been set after queueConstruct ()
	 */
	if(pThis->pqParent == NULL) {
dbgprintf("Queue %p: no parent, alloc mutex\n", pThis);
		pThis->mut = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
		pthread_mutex_init(pThis->mut, NULL);
	} else {
		/* child queue, we need to use parent's mutex */
		pThis->mut = pThis->pqParent->mut;
dbgprintf("Queue %p: I am child, use mutex %p\n", pThis, pThis->pqParent->mut);
	}

	pthread_mutex_init(&pThis->mutThrdMgmt, NULL);
	pthread_cond_init (&pThis->condDAReady, NULL);
	pthread_cond_init (&pThis->notFull, NULL);
	pthread_cond_init (&pThis->notEmpty, NULL);
dbgprintf("Queue %p: post mutexes, mut %p\n", pThis, pThis->mut);

	/* call type-specific constructor */
	CHKiRet(pThis->qConstruct(pThis)); /* this also sets bIsDA */

	dbgprintf("Queue 0x%lx: type %d, enq-only %d, disk assisted %d, maxFileSz %ld, qsize %d starting\n", queueGetID(pThis),
		  pThis->qType, pThis->bEnqOnly, pThis->bIsDA, pThis->iMaxFileSize, pThis->iQueueSize);

	if(pThis->qType == QUEUETYPE_DIRECT)
		FINALIZE;	/* with direct queues, we are already finished... */

	/* create worker thread pools for regular operation. The DA pool is created on an as-needed
	 * basis, which potentially means never under most circumstances.
	 */
	lenBuf = snprintf((char*)pszBuf, sizeof(pszBuf), "Queue 0x%lx:Reg", (unsigned long) pThis);
	CHKiRet(wtpConstruct		(&pThis->pWtpReg));
	CHKiRet(wtpSetDbgHdr		(pThis->pWtpReg, pszBuf, lenBuf));
	CHKiRet(wtpSetpfChkStopWrkr	(pThis->pWtpReg, queueChkStopWrkrReg));
	CHKiRet(wtpSetpfIsIdle		(pThis->pWtpReg, queueIsIdleReg));
	CHKiRet(wtpSetpfDoWork		(pThis->pWtpReg, queueConsumerReg));
	CHKiRet(wtpSetpfOnWorkerCancel	(pThis->pWtpReg, queueConsumerCancelCleanup));
	CHKiRet(wtpSetpmutUsr		(pThis->pWtpReg, pThis->mut));
	CHKiRet(wtpSetpcondBusy		(pThis->pWtpReg, &pThis->notEmpty));
	CHKiRet(wtpSetiNumWorkerThreads	(pThis->pWtpReg, pThis->iNumWorkerThreads));
	CHKiRet(wtpSetpUsr		(pThis->pWtpReg, pThis));
	CHKiRet(wtpConstructFinalize	(pThis->pWtpReg));

	/* initialize worker thread instances */
RUNLOG_VAR("%d", pThis->bIsDA);
	if(pThis->bIsDA) {
		/* If we are disk-assisted, we need to check if there is a QIF file
		 * which we need to load. -- rgerhards, 2008-01-15
		 */
		iRetLocal = queueHaveQIF(pThis);
RUNLOG_VAR("%d", iRetLocal);
		if(iRetLocal == RS_RET_OK) {
			dbgprintf("Queue 0x%lx: on-disk queue present, needs to be reloaded\n",
				  queueGetID(pThis));
RUNLOG;
			queueInitDA(pThis, QUEUE_MODE_ENQDEQ, LOCK_MUTEX); /* initiate DA mode */
			bInitialized = 1; /* we are done */
		} else {
			// TODO: use logerror? -- rgerhards, 2008-01-16
			dbgprintf("Queue 0x%lx: error %d trying to access on-disk queue files, starting without them. "
			          "Some data may be lost\n", queueGetID(pThis), iRetLocal);
		}
	}

RUNLOG_VAR("%d", bInitialized);
	if(!bInitialized) {
		dbgprintf("Queue 0x%lx: queue starts up without (loading) any DA disk state (this is normal for the DA "
			  "queue itself!)\n", queueGetID(pThis));
	}

	/* if the queue already contains data, we need to start the correct number of worker threads. This can be
	 * the case when a disk queue has been loaded. If we did not start it here, it would never start.
	 */
	queueAdviseMaxWorkers(pThis);

	pThis->bQueueStarted = 1;

finalize_it:
dbgprintf("queueStart() exit, iret %d\n", iRet);
	RETiRet;
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
		queuePersist(pThis);
		pThis->iUpdsSincePersist = 0;
	}

	RETiRet;
}


/* destructor for the queue object */
rsRetVal queueDestruct(queue_t **ppThis)
{
	DEFiRet;
	queue_t *pThis;

	assert(ppThis != NULL);
	pThis = *ppThis;
	ISOBJ_TYPE_assert(pThis, queue);

pThis->bSaveOnShutdown = 1; // TODO: Test remove

	pThis->bQueueInDestruction = 1; /* indicate we are in destruction (modifies some behaviour) */

	/* shut down all workers (handles *all* of the persistence logic) */
	if(!pThis->bEnqOnly) /* in enque-only mode, we have no worker pool! */
		queueShutdownWorkers(pThis);
RUNLOG;

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
RUNLOG;

	/* Now check if we actually have a DA queue and, if so, destruct it.
	 * Note that the wtp must be destructed first, it may be in cancel cleanup handler
	 * *right now* and actually *need* to access the queue object to persist some final
	 * data (re-queueing case). So we need to destruct the wtp first, which will make 
	 * sure all workers have terminated.
	 */
RUNLOG_VAR("%p", pThis->pWtpDA);
	if(pThis->pWtpDA != NULL) {
RUNLOG;
		wtpDestruct(&pThis->pWtpDA);
RUNLOG_VAR("%p", pThis->pqDA);
	}
	if(pThis->pqDA != NULL) {
		queueDestruct(&pThis->pqDA);
	}
RUNLOG;

	/* persist the queue (we always do that - queuePersits() does cleanup if the queue is empty)
	 * This handler is most important for disk queues, it will finally persist the necessary
	 * on-disk structures. In theory, other queueing modes may implement their other (non-DA)
	 * methods of persisting a queue between runs, but in practice all of this is done via
	 * disk queues and DA mode. Anyhow, it doesn't hurt to know that we could extend it here
	 * if need arises (what I doubt...) -- rgerhards, 2008-01-25
	 */
	CHKiRet_Hdlr(queuePersist(pThis)) {
		dbgprintf("Queue 0x%lx: error %d persisting queue - data lost!\n", (unsigned long) pThis, iRet);
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

	/* type-specific destructor */
	iRet = pThis->qDestruct(pThis);

	if(pThis->pszFilePrefix != NULL)
		free(pThis->pszFilePrefix);

	/* and finally delete the queue objet itself */
	free(pThis);
	*ppThis = NULL;

	RETiRet;
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

	ISOBJ_TYPE_assert(pThis, queue);

// TODO: check if queue is terminating and if so either discard message or enqeue it to the DA queue *directly*
dbgprintf("Queue %p: EnqObj() 1\n", pThis);
RUNLOG_VAR("%d", pThis->bRunsDA);
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

	/* first check if we need to discard this message (which will cause CHKiRet() to exit) */
	CHKiRet(queueChkDiscardMsg(pThis, pThis->iQueueSize, pThis->bRunsDA, pUsr));

	/* then check if we need to add an assistance disk queue */
	if(pThis->bIsDA)
		CHKiRet(queueChkStrtDA(pThis));
	

	/* wait for the queue to be ready... */
	while(pThis->iMaxQueueSize > 0 && pThis->iQueueSize >= pThis->iMaxQueueSize) {
		dbgprintf("Queue 0x%lx: enqueueMsg: queue FULL - waiting to drain.\n", queueGetID(pThis));
		timeoutComp(&t, pThis->toEnq);
		if(pthread_cond_timedwait(&pThis->notFull, pThis->mut, &t) != 0) {
			dbgprintf("Queue 0x%lx: enqueueMsg: cond timeout, dropping message!\n", queueGetID(pThis));
			objDestruct(pUsr);
			ABORT_FINALIZE(RS_RET_QUEUE_FULL);
		}
	}

	/* and finally enqueue the message */
RUNLOG_VAR("%p", pThis);
RUNLOG_VAR("%d", pThis->bRunsDA);
	CHKiRet(queueAdd(pThis, pUsr));
	queueChkPersist(pThis);

finalize_it:
	if(pThis->qType != QUEUETYPE_DIRECT) {
		d_pthread_mutex_unlock(pThis->mut);
		i = pthread_cond_signal(&pThis->notEmpty);
		dbgprintf("Queue 0x%lx: EnqueueMsg signaled condition (%d)\n", (unsigned long) pThis, i);
		pthread_setcancelstate(iCancelStateSave, NULL);
	}

	/* make sure at least one worker is running. */
	queueAdviseMaxWorkers(pThis);

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
			dbgprintf("Queue 0x%lx: switching to enqueue-only mode, terminating all worker threads\n",
				   queueGetID(pThis));
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
	RETiRet;
}
#undef	isProp
/* Initialize the stream class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-01-09
 */
BEGINObjClassInit(queue, 1)
	OBJSetMethodHandler(objMethod_SETPROPERTY, queueSetProperty);
	//OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, strmConstructFinalize);
//fprintf(stdout, "queueChkStopWrkrReg: %p\n", queueChkStopWrkrReg);
//fprintf(stdout, "queueChkStopWrkrDA:  %p\n", queueChkStopWrkrDA);
ENDObjClassInit(queue)

/*
 * vi:set ai:
 */
