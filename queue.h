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

#include <pthread.h>
#include "obj.h"

/* some information about disk files used by the queue. In the long term, we may
 * export this settings to a separate file module - or not (if they are too
 * queue-specific. I just thought I mention it here so that everyone is aware
 * of this possibility. -- rgerhards, 2008-01-07
 */
typedef struct {
	int fd;		/* the file descriptor, -1 if closed */
	int iCurrFileNum;/* current file number (NOT descriptor, but the number in the file name!) */
	int iCurrOffs;	/* current offset */
} queueFileDescription_t;


/* queue types */
typedef enum {
	QUEUETYPE_FIXED_ARRAY = 0,/* a simple queue made out of a fixed (initially malloced) array fast but memoryhog */
	QUEUETYPE_LINKEDLIST = 1, /* linked list used as buffer, lower fixed memory overhead but slower */
	QUEUETYPE_DISK = 2, 	  /* disk files used as buffer */
	QUEUETYPE_DIRECT = 3 	  /* no queuing happens, consumer is directly called */
} queueType_t;

/* list member definition for linked list types of queues: */
typedef struct qLinkedList_S {
	struct qLinkedList_S *pNext;
	void *pUsr;
} qLinkedList_t;

/* the queue object */
typedef struct queue_s {
	queueType_t	qType;
	int	iQueueSize;	/* Current number of elements in the queue */
	int	iMaxQueueSize;	/* how large can the queue grow? */
	int 	iNumWorkerThreads;/* number of worker threads to use */
	pthread_t *pWorkerThreads;/* array with ID of the worker thread(s) associated with this queue */
	int	bDoRun;		/* 1 - run queue, 0 - shutdown of queue requested */
	rsRetVal (*pConsumer)(void *); /* user-supplied consumer function for dequeued messages */
	/* type-specific handlers (set during construction) */
	rsRetVal (*qConstruct)(struct queue_s *pThis);
	rsRetVal (*qDestruct)(struct queue_s *pThis);
	rsRetVal (*qAdd)(struct queue_s *pThis, void *pUsr);
	rsRetVal (*qDel)(struct queue_s *pThis, void **ppUsr);
	/* end type-specific handler */
	/* synchronization variables */
	pthread_mutex_t *mut;
	pthread_cond_t *notFull, *notEmpty;
	/* end sync variables */
	union {			/* different data elements based on queue type (qType) */
		struct {
			long head, tail;
			void** pBuf;		/* the queued user data structure */
		} farray;
		struct {
			qLinkedList_t *pRoot;
			qLinkedList_t *pLast;
		} linklist;
		struct {
			uchar *pszSpoolDir;
			size_t lenSpoolDir;
			uchar *pszFilePrefix;
			size_t lenFilePrefix;
			int iNumberFiles;	/* how many files make up the queue? */
			int iMaxFileSize;	/* max size for a single queue file */
			queueFileDescription_t fWrite; /* current file to be written */
			queueFileDescription_t fRead;  /* current file to be read */
		} disk;
	} tVars;
} queue_t;


/* prototypes */
rsRetVal queueDestruct(queue_t *pThis);
rsRetVal queueEnqObj(queue_t *pThis, void *pUsr);
rsRetVal queueStart(queue_t *pThis);
rsRetVal queueConstruct(queue_t **ppThis, queueType_t qType, int iWorkerThreads,
		        int iMaxQueueSize, rsRetVal (*pConsumer)(void*));

#endif /* #ifndef QUEUE_H_INCLUDED */
