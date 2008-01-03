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

#include "rsyslog.h"
#include "syslogd.h"
#include "queue.h"

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
	CHKiRet(pThis->qDel(pThis, pUsr));
	--pThis->iQueueSize;

	dbgprintf("Queue 0x%lx: entry deleted, size now %d entries\n", (unsigned long) pThis, pThis->iQueueSize);

finalize_it:
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
			queueDel(pThis, &pUsr);
			pthread_mutex_unlock(pThis->mut);
			pthread_cond_signal (pThis->notFull);
			/* do actual processing (the lengthy part, runs in parallel) */
			dbgprintf("Worker for queue 0x%lx is running...\n", (unsigned long) pThis);
			pThis->pConsumer(pUsr);
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

/* Constructor for the queue object */
rsRetVal queueConstruct(queue_t **ppThis, queueType_t qType, int iMaxQueueSize, rsRetVal (*pConsumer)(void*))
{
	DEFiRet;
	queue_t *pThis;
	int i;

	assert(ppThis != NULL);
	assert(pConsumer != NULL);

	if((pThis = (queue_t *)malloc(sizeof(queue_t))) == NULL) {
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
	}

	/* call type-specific constructor */
	CHKiRet(pThis->qConstruct(pThis));

	/* now fire up the worker thread */
	pThis->bDoRun = 1; /* we are NOT done (else worker would immediately terminate) */
	i = pthread_create(&pThis->thrdWorker, NULL, queueWorker, (void*) pThis);
	dbgprintf("Worker thread for queue 0x%lx started with state %d.\n", (unsigned long) pThis, i);
	
finalize_it:
	if(iRet == RS_RET_OK) {
		*ppThis = pThis;
	} else {
		if(pThis != NULL)
			free(pThis);
	}

	return iRet;
}


/* destructor for the queue object */
rsRetVal queueDestruct(queue_t *pThis)
{
	DEFiRet;

	assert(pThis != NULL);

	/* first stop the worker thread */
	dbgprintf("Initiating worker thread shutdown sequence for queue 0x%lx...\n", (unsigned long) pThis);
	pThis->bDoRun = 0;
	/* It's actually not "not empty" below but awaking the worker. The worker
	 * then finds out that it shall terminate and does so.
	 */
	pthread_cond_signal(pThis->notEmpty);
	pthread_join(pThis->thrdWorker, NULL);
	dbgprintf("Worker thread for queue 0x%lx terminated.\n", (unsigned long) pThis);

	/* ... then free resources */
	pthread_mutex_destroy (pThis->mut);
	free (pThis->mut);
	pthread_cond_destroy (pThis->notFull);
	free (pThis->notFull);
	pthread_cond_destroy (pThis->notEmpty);
	free (pThis->notEmpty);
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

	pthread_mutex_lock(pThis->mut);
		
	while(pThis->iQueueSize >= pThis->iMaxQueueSize) {
		dbgprintf("enqueueMsg: queue 0x%lx FULL.\n", (unsigned long) pThis);
		
		clock_gettime (CLOCK_REALTIME, &t);
		t.tv_sec += 2; /* TODO: configurable! */
		
		if(pthread_cond_timedwait (pThis->notFull,
					   pThis->mut, &t) != 0) {
			dbgprintf("Queue 0x%lx: enqueueMsg: cond timeout, dropping message!\n", (unsigned long) pThis);
			ABORT_FINALIZE(RS_RET_QUEUE_FULL);
		}
	}
	CHKiRet(queueAdd(pThis, pUsr));

finalize_it:
	/* now activate the worker thread */
	pthread_mutex_unlock(pThis->mut);
	i = pthread_cond_signal(pThis->notEmpty);
	dbgprintf("Queue 0x%lx: EnqueueMsg signaled condition (%d)\n", (unsigned long) pThis, i);
	return iRet;
}
/*
 * vi:set ai:
 */
