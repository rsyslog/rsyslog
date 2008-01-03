/* Definition of the queue support module.
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

#ifndef QUEUE_H_INCLUDED
#define QUEUE_H_INCLUDED


/* queue types */
typedef enum {
	QUEUETYPE_FIXED_ARRAY,/* a simple queue made out of a fixed (initially malloced) array fast but memoryhog */
	QUEUETYPE_LINKEDLIST,/* linked list used as buffer, lower fixed memory overhead but slower */
} queueType_t;

/* the queue object */
typedef struct queue_s {
	queueType_t	qType;
	int	iMaxQueueSize;	/* how large can the queue grow? */
	/* type-specific handlers (set during construction) */
	rsRetVal (*qConstruct)(struct queue_s *pThis);
	rsRetVal (*qDestruct)(struct queue_s *pThis);
	rsRetVal (*qAdd)(struct queue_s *pThis, void *pUsr);
	rsRetVal (*qDel)(struct queue_s *pThis, void **ppUsr);
	/* end type-specific handler */
	/* synchronization variables */
	pthread_mutex_t *mut;
	pthread_cond_t *notFull, *notEmpty;
	int full, empty;
	/* end sync variables */
	union {			/* different data elements based on queue type (qType) */
		struct {
			long head, tail;
			void** pBuf;		/* the queued user data structure */
		} farray;
	} tVars;
} queue_t;


/* prototypes */
rsRetVal queueConstruct(queue_t **ppThis, queueType_t qType, int iMaxQueueSize);
rsRetVal queueDestruct(queue_t *pThis);
rsRetVal queueAdd(queue_t *pThis, void* in);
rsRetVal queueDel(queue_t *pThis, void **out);

#endif /* #ifndef QUEUE_H_INCLUDED */
