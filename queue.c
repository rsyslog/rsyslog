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
#include <pthread.h>

#include "rsyslog.h"
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
	free (pThis);

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
	if (pThis->tVars.farray.tail == pThis->tVars.farray.head)
		pThis->full = 1;
	pThis->empty = 0;

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
	if (pThis->tVars.farray.head == pThis->tVars.farray.tail)
		pThis->empty = 1;
	pThis->full = 0;

	return iRet;
}
/* --------------- end type-specific handlers -------------------- */


/* Constructor for the queue object */
rsRetVal queueConstruct(queue_t **ppThis, queueType_t qType, int iMaxQueueSize)
{
	DEFiRet;
	queue_t *pThis;

	assert(ppThis != NULL);
dbgprintf("queueConstruct in \n");

	if((pThis = (queue_t *)malloc(sizeof(queue_t))) == NULL) {
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}

	/* we have an object, so let's fill the properties */
	pThis->iMaxQueueSize = iMaxQueueSize;
	pThis->empty = 1;
	pThis->full = 0;
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


/* destructor for the queue object */
rsRetVal queueDestruct(queue_t *pThis)
{
	DEFiRet;

	assert(pThis != NULL);
	pthread_mutex_destroy (pThis->mut);
	free (pThis->mut);
	pthread_cond_destroy (pThis->notFull);
	free (pThis->notFull);
	pthread_cond_destroy (pThis->notEmpty);
	free (pThis->notEmpty);
	/* type-specific destructor */
	iRet = pThis->qDestruct(pThis);

	return iRet;
}


/* In queueAdd() and queueDel() we have a potential race condition. If a message
 * is dequeued and at the same time a message is enqueued and the queue is either
 * full or empty, the full (or empty) indicator may be invalidly updated. HOWEVER,
 * this does not cause any real problems. No queue pointers can be wrong. And even
 * if one of the flags is set invalidly, that does not pose a real problem. If
 * "full" is invalidly set, at mose one message might be lost, if we are already in
 * a timeout situation (this is quite acceptable). And if "empty" is accidently set,
 * the receiver will not continue the inner loop, but break out of the outer. So no
 * harm is done at all. For this reason, I do not yet use a mutex to guard the two
 * flags - there would be a notable performance hit with, IMHO, no gain in stability
 * or functionality. But anyhow, now it's documented...
 * rgerhards, 2007-09-20
 * NOTE: this comment does not really apply - the callers handle the mutex, so it
 * *is* guarded.
 */
rsRetVal queueAdd(queue_t *pThis, void* in)
{
	DEFiRet;

	assert(pThis != NULL);
	CHKiRet(pThis->qAdd(pThis, in));
finalize_it:
	return iRet;
}

rsRetVal queueDel(queue_t *pThis, void **out)
{
	DEFiRet;

	assert(pThis != NULL);
	CHKiRet(pThis->qDel(pThis, out));
finalize_it:
	return iRet;
}

/*
 * vi:set ai:
 */
