// TODO: peekmsg() on first entry, with new/inprogress/deleted entry, destruction in 
// call consumer state. Facilitates retaining messages in queue until action could
// be called!
/* queue.c
 *
 * This file implements the queue object and its several queueing methods.
 * 
 * File begun on 2008-01-03 by RGerhards
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

/* methods */

/* first, we define type-specific handlers. The provide a generic functionality,
 * but for this specific type of queue. The mapping to these handlers happens during
 * queue construction. Later on, handlers are called by pointers present in the
 * queue instance object.
 */

/* -------------------- fixed array -------------------- */
static rsRetVal qConstructFixedArray(queue_t *pThis)
{
	DEFiRet;

	assert(pThis != NULL);

	if((pThis->tVars.farray.pBuf = malloc(sizeof(void *) * pThis->iMaxQueueSize)) == NULL) {
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}

	pThis->tVars.farray.head = 0;
	pThis->tVars.farray.tail = 0;

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

	assert(pThis != NULL);

	CHKiRet(strmConstruct(&pThis->tVars.disk.pWrite));
	CHKiRet(strmSetDir(pThis->tVars.disk.pWrite, glblGetWorkDir(), strlen((char*)glblGetWorkDir())));
	CHKiRet(strmSetiMaxFiles(pThis->tVars.disk.pWrite, 10000000));
	CHKiRet(strmSettOperationsMode(pThis->tVars.disk.pWrite, STREAMMODE_WRITE));
	CHKiRet(strmConstructFinalize(pThis->tVars.disk.pWrite));

	CHKiRet(strmConstruct(&pThis->tVars.disk.pRead));
	CHKiRet(strmSetbDeleteOnClose(pThis->tVars.disk.pRead, 1));
	CHKiRet(strmSetDir(pThis->tVars.disk.pRead, glblGetWorkDir(), strlen((char*)glblGetWorkDir())));
	CHKiRet(strmSetiMaxFiles(pThis->tVars.disk.pRead, 10000000));
	CHKiRet(strmSettOperationsMode(pThis->tVars.disk.pRead, STREAMMODE_READ));
	CHKiRet(strmConstructFinalize(pThis->tVars.disk.pRead));

finalize_it:
	return iRet;
}


static rsRetVal qDestructDisk(queue_t *pThis)
{
	DEFiRet;
	
	assert(pThis != NULL);
	
	strmDestruct(pThis->tVars.disk.pWrite);
	strmDestruct(pThis->tVars.disk.pRead);

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
	return objDeserialize(ppUsr, OBJMsg, pThis->tVars.disk.pRead);
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

	/* TODO: calling the consumer should go into its own function! -- rgerhards, 2008-01-05*/
	iRetLocal = pThis->pConsumer(pUsr);
	if(iRetLocal != RS_RET_OK)
		dbgprintf("Queue 0x%lx: Consumer returned iRet %d\n",
			  (unsigned long) pThis, iRetLocal);
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

	dbgprintf("Queue 0x%lx: entry added, size now %d entries\n", (unsigned long) pThis, pThis->iQueueSize);

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
		  (unsigned long) pThis, iRet, pThis->iQueueSize);

	return iRet;
}


/* Worker thread management function carried out when the main
 * worker is about to terminate.
 */
static rsRetVal queueShutdownWorkers(queue_t *pThis)
{
	DEFiRet;
	int i;
	qWrkCmd_t tShutdownCmd;

pThis->bImmediateShutdown = 1; /*testing */
	assert(pThis != NULL);

	/* select shutdown mode */
	tShutdownCmd = (pThis->bImmediateShutdown) ? eWRKTHRDCMD_SHUTDOWN_IMMEDIATE : eWRKTHRDCMD_SHUTDOWN;

	dbgprintf("Queue 0x%lx: initiating worker thread shutdown sequence, mode %d...\n",
		  (unsigned long) pThis, (int) tShutdownCmd);

	/* tell all workers to terminate */
	for(i = 0 ; i < pThis->iNumWorkerThreads ; ++i)
		pThis->pWrkThrds[i].tCurrCmd = tShutdownCmd;

	/* awake them... */
	pthread_cond_broadcast(pThis->notEmpty);
	
	/* and wait for their termination */
	for(i = 0 ; i < pThis->iNumWorkerThreads ; ++i) {
		pthread_join(pThis->pWrkThrds[i].thrdID, NULL);
	}

	dbgprintf("Queue 0x%lx: worker threads terminated, remaining queue size %d.\n",
		  (unsigned long) pThis, pThis->iQueueSize);

	return iRet;
}


/* Each queue has one associated worker (consumer) thread. It will pull
 * the message from the queue and pass it to a user-defined function.
 * This function was provided on construction. It MUST be thread-safe.
 *
 * There are two classes of worker threads, all implemented via the function
 * below. The queue may start multiple workers. The first one carries out normal
 * processing BUT also manages the other workers (the first one and all other
 * ones are the two different classes). This is so that the queue can dynamically
 * start and stop worker threads. So far, this dynamic mode is not yet supported,
 * but we will need it at least for disk-assisted queue types. There, multiple
 * workers are supported as long as the queue is running in memory, but only
 * a single worker is supported if running in disk mode. To start and stop
 * workers, we need to have one thread that is capable to wait. We could start
 * up a specific management thread. However, this means additional overhead. So
 * we have decided to use worker #0, which is always present, to carry out this
 * management as an additional chore. -- rgerhards, 2008-01-10
 */
static void *
queueWorker(void *arg)
{
	DEFiRet;
	queue_t *pThis = (queue_t*) arg;
	void *pUsr;
	sigset_t sigSet;
	int iMyThrdIndx;	/* index for this thread in queue thread table */

	assert(pThis != NULL);

	sigfillset(&sigSet);
	pthread_sigmask(SIG_BLOCK, &sigSet, NULL);

	/* first find myself in the queue's thread table */
	for(iMyThrdIndx = 0 ; iMyThrdIndx < pThis->iNumWorkerThreads ; ++iMyThrdIndx)
		if(pThis->pWrkThrds[iMyThrdIndx].thrdID == pthread_self())
			break;
	assert(pThis->pWrkThrds[iMyThrdIndx].thrdID == pthread_self());

	dbgprintf("Queue 0x%lx/w%d: worker thread startup.\n", (unsigned long) pThis, iMyThrdIndx);

	/* now we have our identity, on to real processing */
	while(pThis->pWrkThrds[iMyThrdIndx].tCurrCmd == eWRKTHRDCMD_RUN
	      || (pThis->pWrkThrds[iMyThrdIndx].tCurrCmd == eWRKTHRDCMD_SHUTDOWN && pThis->iQueueSize > 0)) {
dbgprintf("worker %d runs, cmd %d\n", iMyThrdIndx, pThis->pWrkThrds[iMyThrdIndx].tCurrCmd);
		pthread_mutex_lock(pThis->mut);
		while (pThis->iQueueSize == 0 && pThis->pWrkThrds[iMyThrdIndx].tCurrCmd == eWRKTHRDCMD_RUN) {
			dbgprintf("Queue 0x%lx/w%d: queue EMPTY, waiting for next message.\n",
				  (unsigned long) pThis, iMyThrdIndx);
			pthread_cond_wait (pThis->notEmpty, pThis->mut);
		}
		if(pThis->iQueueSize > 0) {
			/* dequeue element (still protected from mutex) */
			iRet = queueDel(pThis, &pUsr);
			pthread_mutex_unlock(pThis->mut);
			pthread_cond_signal (pThis->notFull);
			/* do actual processing (the lengthy part, runs in parallel)
			 * If we had a problem while dequeing, we do not call the consumer,
			 * but we otherwise ignore it. This is in the hopes that it will be
			 * self-healing. Howerver, this is really not a good thing.
			 * rgerhards, 2008-01-03
			 */
			if(iRet == RS_RET_OK) {
				rsRetVal iRetLocal;
				dbgprintf("Queue 0x%lx/w%d: worker executes consumer...\n",
				           (unsigned long) pThis, iMyThrdIndx);
				iRetLocal = pThis->pConsumer(pUsr);
				dbgprintf("Queue 0x%lx/w%d: worker: consumer returnd %d\n",
				           (unsigned long) pThis, iMyThrdIndx, iRetLocal);
				if(iRetLocal != RS_RET_OK)
					dbgprintf("Queue 0x%lx/w%d: Consumer returned iRet %d\n",
					          (unsigned long) pThis, iMyThrdIndx, iRetLocal);
			} else {
				dbgprintf("Queue 0x%lx/w%d: error %d dequeueing element - ignoring, but strange things "
				          "may happen\n", (unsigned long) pThis, iMyThrdIndx, iRet);
			}
		} else { /* the mutex must be unlocked in any case (important for termination) */
			pthread_mutex_unlock(pThis->mut);
		}

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
		
		if(Debug && (pThis->pWrkThrds[iMyThrdIndx].tCurrCmd == eWRKTHRDCMD_SHUTDOWN) && pThis->iQueueSize > 0)
			dbgprintf("Queue 0x%lx/w%d: worker does not yet terminate because it still has "
				  " %d messages to process.\n", (unsigned long) pThis, iMyThrdIndx, pThis->iQueueSize);
	}

	dbgprintf("Queue 0x%lx/w%d: worker thread terminates with %d entries left in queue.\n",
		  (unsigned long) pThis, iMyThrdIndx, pThis->iQueueSize);
	pThis->pWrkThrds[iMyThrdIndx].tCurrCmd = eWRKTHRDCMD_TERMINATED; /* indicate termination */
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

dbgprintf("queueConstruct 0\n");
	if((pThis = (queue_t *)calloc(1, sizeof(queue_t))) == NULL) {
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}
dbgprintf("queueConstruct 0a\n");

	/* we have an object, so let's fill the properties */
	if((pThis->pszSpoolDir = (uchar*) strdup((char*)glblGetWorkDir())) == NULL)
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
dbgprintf("queueConstruct 1\n");

	pThis->lenSpoolDir = strlen((char*)pThis->pszSpoolDir);
	pThis->iMaxFileSize = 1024 * 1024; /* default is 1 MiB */
	pThis->iQueueSize = 0;
	pThis->iMaxQueueSize = iMaxQueueSize;
	pThis->pConsumer = pConsumer;
	pThis->mut = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
	pthread_mutex_init (pThis->mut, NULL);
	pThis->notFull = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
	pthread_cond_init (pThis->notFull, NULL);
	pThis->notEmpty = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
	pthread_cond_init (pThis->notEmpty, NULL);
	pThis->iNumWorkerThreads = iWorkerThreads;

	pThis->pszFilePrefix = NULL;
	pThis->qType = qType;

dbgprintf("queueConstruct 2\n");
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

dbgprintf("queueConstruct 3\n");
	/* call type-specific constructor */
	CHKiRet(pThis->qConstruct(pThis));

finalize_it:
dbgprintf("queueConstruct 4\n");
	OBJCONSTRUCT_CHECK_SUCCESS_AND_CLEANUP
dbgprintf("queueConstruct 5\n");
	return iRet;
}


/* start up the queue - it must have been constructed and parameters defined
 * before.
 */
rsRetVal queueStart(queue_t *pThis)
{
	DEFiRet;
	int iState;
	int i;

	assert(pThis != NULL);

	dbgprintf("Queue 0x%lx: type %d, maxFileSz %ld starting\n", (unsigned long) pThis, pThis->qType,
		   pThis->iMaxFileSize);

	if(pThis->qType != QUEUETYPE_DIRECT) {
		if((pThis->pWrkThrds = calloc(pThis->iNumWorkerThreads, sizeof(qWrkThrd_t))) == NULL)
			ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

		/* fire up the worker threads */
		for(i = 0 ; i < pThis->iNumWorkerThreads ; ++i) {
			pThis->pWrkThrds[i].tCurrCmd = eWRKTHRDCMD_RUN;
			iState = pthread_create(&(pThis->pWrkThrds[i].thrdID), NULL, queueWorker, (void*) pThis);
			dbgprintf("Queue 0x%lx: Worker thread %x, index %d started with state %d.\n",
				  (unsigned long) pThis, (unsigned) pThis->pWrkThrds[i].thrdID, i, iState);
		}
	}

finalize_it:
	return iRet;
}

/* destructor for the queue object */
rsRetVal queueDestruct(queue_t *pThis)
{
	DEFiRet;

	assert(pThis != NULL);

	/* first, terminate worker threads */
	if(pThis->pWrkThrds != NULL) {
		queueShutdownWorkers(pThis);
		free(pThis->pWrkThrds);
		pThis->pWrkThrds = NULL;
	}

	/* ... then free resources */
	pthread_mutex_destroy(pThis->mut);
	free(pThis->mut);
	pthread_cond_destroy(pThis->notFull);
	free(pThis->notFull);
	pthread_cond_destroy(pThis->notEmpty);
	free(pThis->notEmpty);
	/* type-specific destructor */
	iRet = pThis->qDestruct(pThis);

	/* and finally delete the queue objet itself */
	free(pThis);

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
	if(pThis->qType == QUEUETYPE_DISK) {
		CHKiRet(strmSetFilePrefix(pThis->tVars.disk.pWrite, pszPrefix, iLenPrefix));
		CHKiRet(strmSetFilePrefix(pThis->tVars.disk.pRead,  pszPrefix, iLenPrefix));
	}
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

	assert(pThis != NULL);
	
	if(iMaxFileSize < 1024) {
		ABORT_FINALIZE(RS_RET_VALUE_TOO_LOW);
	}

	pThis->iMaxFileSize = iMaxFileSize;
	if(pThis->qType == QUEUETYPE_DISK)
		CHKiRet(strmSetiMaxFileSize(pThis->tVars.disk.pWrite, iMaxFileSize));

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

	assert(pThis != NULL);

	/* Please note that this function is not cancel-safe and consequently
	 * sets the calling thread's cancelibility state to PTHREAD_CANCEL_DISABLE
	 * during its execution. If that is not done, race conditions occur if the
	 * thread is canceled (most important use case is input module termination).
	 * rgerhards, 2008-01-08
	 */
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave);
	if(pThis->pWrkThrds != NULL)
		pthread_mutex_lock(pThis->mut);
		
	while(pThis->iQueueSize >= pThis->iMaxQueueSize) {
		dbgprintf("Queue 0x%lx: enqueueMsg: queue FULL - waiting to drain.\n", (unsigned long) pThis);
		
		clock_gettime (CLOCK_REALTIME, &t);
		t.tv_sec += 2; /* TODO: configurable! */
		
		if(pthread_cond_timedwait (pThis->notFull,
					   pThis->mut, &t) != 0) {
			dbgprintf("Queue 0x%lx: enqueueMsg: cond timeout, dropping message!\n", (unsigned long) pThis);
			objDestruct(pUsr);
			ABORT_FINALIZE(RS_RET_QUEUE_FULL);
		}
	}
	CHKiRet(queueAdd(pThis, pUsr));

finalize_it:
	/* now activate the worker thread */
	if(pThis->pWrkThrds != NULL) {
		pthread_mutex_unlock(pThis->mut);
		i = pthread_cond_signal(pThis->notEmpty);
		dbgprintf("Queue 0x%lx: EnqueueMsg signaled condition (%d)\n", (unsigned long) pThis, i);
	}

	pthread_setcancelstate(iCancelStateSave, NULL);

	return iRet;
}
/*
 * vi:set ai:
 */
