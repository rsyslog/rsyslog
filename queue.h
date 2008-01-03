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
	QUEUETYPE_FIXED_ARRAY,	/* a simple queue made out of a fixed (initially malloced) array fast but memoryhog */
	QUEUETYPE_LINKEDLIST,	/* linked list used as buffer, lower fixed memory overhead but slower */
} queueTypes_t;

/* the queue object */
typedef struct {
	queueTypes_t	qType;
	int	iMaxQueSize;	/* how large can the queue grow? */
	void** pUsr;		/* the queued user data structure */
	/* synchronization variables */
	pthread_mutex_t *mut;
	pthread_cond_t *notFull, *notEmpty;
	int full, empty;
	/* end sync variables */
	union {			/* different data elements based on queue type (qType) */
		struct {
			long head, tail;
		} farray;
	} tVars;
} queue_t;

/* this is the first approach to a queue, this time with static
 * memory.
 */
typedef struct {
	void** pbuf;
	long head, tail;
	int full, empty;
	pthread_mutex_t *mut;
	pthread_cond_t *notFull, *notEmpty;
} msgQueue;

/* prototypes */
msgQueue *queueInit (void);
void queueDelete (msgQueue *q);
void queueAdd (msgQueue *q, void* in);
void queueDel (msgQueue *q, void **out);

/* go-away's */
extern int iMainMsgQueueSize;
extern msgQueue *pMsgQueue;

#endif /* #ifndef QUEUE_H_INCLUDED */
