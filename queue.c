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

/* static data */

/* methods */

/* first, we define type-specific handlers. The provide a generic functionality,
 * but for this specific type of queue. The mapping to these handlers happens during
 * queue construction. Later on, handlers are called by pointers present in the
 * queue instance object.
 */

/* -------------------- fixed array -------------------- */
rsRetVal qConstructFixedArray(queue_t *pThis)
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


rsRetVal qDestructFixedArray(queue_t *pThis)
{
	DEFiRet;
	
	assert(pThis != NULL);

	if(pThis->tVars.farray.pBuf != NULL)
		free(pThis->tVars.farray.pBuf);

	return iRet;
}

rsRetVal qAddFixedArray(queue_t *pThis, void* in)
{
	DEFiRet;

	assert(pThis != NULL);
	pThis->tVars.farray.pBuf[pThis->tVars.farray.tail] = in;
	pThis->tVars.farray.tail++;
	if (pThis->tVars.farray.tail == pThis->iMaxQueueSize)
		pThis->tVars.farray.tail = 0;

	return iRet;
}

rsRetVal qDelFixedArray(queue_t *pThis, void **out)
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
rsRetVal qConstructLinkedList(queue_t *pThis)
{
	DEFiRet;

	assert(pThis != NULL);

	pThis->tVars.linklist.pRoot = 0;
	pThis->tVars.linklist.pLast = 0;

	return iRet;
}


rsRetVal qDestructLinkedList(queue_t *pThis)
{
	DEFiRet;
	
	assert(pThis != NULL);
	
	/* with the linked list type, there is nothing to do here. The
	 * reason is that the Destructor is only called after all entries
	 * have bene taken off the queue. In this case, there is nothing
	 * dynamic left with the linked list.
	 */

	return iRet;
}

rsRetVal qAddLinkedList(queue_t *pThis, void* pUsr)
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

rsRetVal qDelLinkedList(queue_t *pThis, void **ppUsr)
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

rsRetVal qConstructDisk(queue_t *pThis)
{
	DEFiRet;
	uchar *pszFile;

	assert(pThis != NULL);

	if((pThis->tVars.disk.pszSpoolDir = (uchar*) strdup((char*)pszSpoolDirectory)) == NULL)
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

	pThis->tVars.disk.lenSpoolDir = strlen((char*)pThis->tVars.disk.pszSpoolDir);

	/* now open the file */
	CHKiRet(genFileName(&pszFile, pThis->tVars.disk.pszSpoolDir, pThis->tVars.disk.lenSpoolDir,
 		     	    (uchar*) "mainq", 5, 1, (uchar*) "qf", 2));

	dbgprintf("Queue 0x%lx: opening file '%s'\n", (unsigned long) pThis, pszFile);

	pThis->tVars.disk.fd = open((char*)pszFile, O_RDWR|O_CREAT, 0600);
	dbgprintf("opened file %d\n", pThis->tVars.disk.fd);

finalize_it:
	if(pThis->tVars.disk.pszSpoolDir != NULL)
		free(pThis->tVars.disk.pszSpoolDir);

	return iRet;
}


rsRetVal qDestructDisk(queue_t *pThis)
{
	DEFiRet;
	
	assert(pThis != NULL);
	
	close(pThis->tVars.disk.fd);

	return iRet;
}

rsRetVal qAddDisk(queue_t *pThis, void* pUsr)
{
	DEFiRet;
	int i;
	rsCStrObj *pCStr;

	assert(pThis != NULL);
	dbgprintf("writing to file %d\n", pThis->tVars.disk.fd);
	CHKiRet((objSerialize(pUsr))(pUsr, &pCStr)); // TODO: hier weiter machen!
	i = write(pThis->tVars.disk.fd, rsCStrGetBufBeg(pCStr), rsCStrLen(pCStr));
	dbgprintf("write wrote %d bytes, errno: %d, err %s\n", i, errno, strerror(errno));

finalize_it:
	return iRet;
}

rsRetVal qDelDisk(queue_t __attribute__((unused)) *pThis, void __attribute__((unused)) **ppUsr)
{
	DEFiRet;

	iRet = RS_RET_ERR;

	return iRet;
}

/* -------------------- direct (no queueing) -------------------- */
rsRetVal qConstructDirect(queue_t __attribute__((unused)) *pThis)
{
	return RS_RET_OK;
}


rsRetVal qDestructDirect(queue_t __attribute__((unused)) *pThis)
{
	return RS_RET_OK;
}

rsRetVal qAddDirect(queue_t *pThis, void* pUsr)
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

rsRetVal qDelDirect(queue_t __attribute__((unused)) *pThis, __attribute__((unused)) void **out)
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




/* Each queue has one associated worker (consumer) thread. It will pull
 * the message from the queue and pass it to a user-defined function.
 * This function was provided on construction. It MUST be thread-safe.
 *
 * Please NOTE:
 * Having more than one worker requires considerable
 * additional code review in regard to thread-safety.
 */
static void *
queueWorker(void *arg)
{
	DEFiRet;
	queue_t *pThis = (queue_t*) arg;
	void *pUsr;
	sigset_t sigSet;

	assert(pThis != NULL);

	sigfillset(&sigSet);
	pthread_sigmask(SIG_BLOCK, &sigSet, NULL);

	while(pThis->bDoRun || !pThis->iQueueSize == 0) {
		pthread_mutex_lock(pThis->mut);
		while (pThis->iQueueSize == 0 && pThis->bDoRun) {
			dbgprintf("queueWorker: queue 0x%lx EMPTY, waiting for next message.\n", (unsigned long) pThis);
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
				dbgprintf("Worker for queue 0x%lx is running...\n", (unsigned long) pThis);
				iRetLocal = pThis->pConsumer(pUsr);
				if(iRetLocal != RS_RET_OK)
					dbgprintf("Queue 0x%lx: Consumer returned iRet %d\n",
					          (unsigned long) pThis, iRetLocal);
			} else {
				dbgprintf("Queue 0x%lx: error %d dequeueing element - ignoring, but strange things "
				          "may happen\n", (unsigned long) pThis, iRet);
			}
		} else { /* the mutex must be unlocked in any case (important for termination) */
			pthread_mutex_unlock(pThis->mut);
		}
		
		if(Debug && !pThis->bDoRun && pThis->iQueueSize > 0)
			dbgprintf("Worker 0x%lx does not yet terminate because it still has messages to process.\n",
				 (unsigned long) pThis);
	}

	dbgprintf("Worker thread for queue 0x%lx terminates.\n", (unsigned long) pThis);
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
	pThis->qType = qType;

	/* set type-specific handlers */
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
			break;
		case QUEUETYPE_DIRECT:
			pThis->qConstruct = qConstructDirect;
			pThis->qDestruct = qDestructDirect;
			pThis->qAdd = qAddDirect;
			pThis->qDel = qDelDirect;
			break;
	}

	/* call type-specific constructor */
	CHKiRet(pThis->qConstruct(pThis));

finalize_it:
	if(iRet == RS_RET_OK) {
		*ppThis = pThis;
	} else {
		if(pThis != NULL)
			free(pThis);
	}

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

	if(pThis->qType != QUEUETYPE_DIRECT) {
		if((pThis->pWorkerThreads = calloc(pThis->iNumWorkerThreads, sizeof(pthread_t))) == NULL)
			ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

		/* fire up the worker thread */
		pThis->bDoRun = 1; /* we are NOT done (else worker would immediately terminate) */
		for(i = 0 ; i < pThis->iNumWorkerThreads ; ++i) {
			iState = pthread_create(&(pThis->pWorkerThreads[i]), NULL, queueWorker, (void*) pThis);
			dbgprintf("Worker thread %d for queue 0x%lx, type %d started with state %d.\n",
				  i, (unsigned long) pThis, (int) pThis->qType, iState);
		}
	}

finalize_it:
	return iRet;
}

/* destructor for the queue object */
rsRetVal queueDestruct(queue_t *pThis)
{
	DEFiRet;
	int i;

	assert(pThis != NULL);

	if(pThis->pWorkerThreads != NULL) {
		/* first stop the worker thread */
		dbgprintf("Initiating worker thread shutdown sequence for queue 0x%lx...\n", (unsigned long) pThis);
		pThis->bDoRun = 0;
		/* It's actually not "not empty" below but awaking the workers. They
		 * then find out that they shall terminate and do so.
		 */
		pthread_cond_broadcast(pThis->notEmpty);
		/* end then wait for all worker threads to terminate */
		for(i = 0 ; i < pThis->iNumWorkerThreads ; ++i) {
			pthread_join(pThis->pWorkerThreads[i], NULL);
		}
		dbgprintf("Worker threads for queue 0x%lx terminated.\n", (unsigned long) pThis);
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
	int i;
	struct timespec t;

	assert(pThis != NULL);

	if(pThis->qType != QUEUETYPE_DIRECT)
		pthread_mutex_lock(pThis->mut);
		
	while(pThis->iQueueSize >= pThis->iMaxQueueSize) {
		dbgprintf("enqueueMsg: queue 0x%lx FULL.\n", (unsigned long) pThis);
		
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
	if(pThis->pWorkerThreads != NULL) {
		pthread_mutex_unlock(pThis->mut);
		i = pthread_cond_signal(pThis->notEmpty);
		dbgprintf("Queue 0x%lx: EnqueueMsg signaled condition (%d)\n", (unsigned long) pThis, i);
	}

	return iRet;
}
/*
 * vi:set ai:
 */
