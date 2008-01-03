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
int iMainMsgQueueSize;
msgQueue *pMsgQueue = NULL;

/* methods */

/* queue functions (may be migrated to some other file...)
 */


msgQueue *queueInit (void)
{
	msgQueue *q;

	q = (msgQueue *)malloc(sizeof(msgQueue));
	if (q == NULL) return (NULL);
	if((q->pbuf = malloc(sizeof(void *) * iMainMsgQueueSize)) == NULL) {
		free(q);
		return NULL;
	}

	q->empty = 1;
	q->full = 0;
	q->head = 0;
	q->tail = 0;
	q->mut = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
	pthread_mutex_init (q->mut, NULL);
	q->notFull = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
	pthread_cond_init (q->notFull, NULL);
	q->notEmpty = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
	pthread_cond_init (q->notEmpty, NULL);
	
	return (q);
}

void queueDelete (msgQueue *q)
{
	pthread_mutex_destroy (q->mut);
	free (q->mut);
	pthread_cond_destroy (q->notFull);
	free (q->notFull);
	pthread_cond_destroy (q->notEmpty);
	free (q->notEmpty);
	free(q->pbuf);
	free (q);
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
void queueAdd (msgQueue *q, void* in)
{
	q->pbuf[q->tail] = in;
	q->tail++;
	if (q->tail == iMainMsgQueueSize)
		q->tail = 0;
	if (q->tail == q->head)
		q->full = 1;
	q->empty = 0;

	return;
}

void queueDel(msgQueue *q, void **out)
{
	*out = (void*) q->pbuf[q->head];

	q->head++;
	if (q->head == iMainMsgQueueSize)
		q->head = 0;
	if (q->head == q->tail)
		q->empty = 1;
	q->full = 0;

	return;
}

/*
 * vi:set ai:
 */
