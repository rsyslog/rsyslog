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
#include "stream.h"

/* some information about disk files used by the queue. In the long term, we may
 * export this settings to a separate file module - or not (if they are too
 * queue-specific. I just thought I mention it here so that everyone is aware
 * of this possibility. -- rgerhards, 2008-01-07
 */
typedef struct {
	int fd;		/* the file descriptor, -1 if closed */
	uchar *pszFileName; /* name of current file (if open) */
	int iCurrFileNum;/* current file number (NOT descriptor, but the number in the file name!) */
	size_t iCurrOffs;/* current offset */
	uchar *pIOBuf;	/* io Buffer */
	int iBufPtrMax;	/* current max Ptr in Buffer (if partial read!) */
	int iBufPtr;	/* pointer into current buffer */
	int iUngetC;	/* char set via UngetChar() call or -1 if none set */
	int bDeleteOnClose; /* set to 1 to auto-delete on close -- be careful with that setting! */
} queueFileDescription_t;
#define qFILE_IOBUF_SIZE 4096 /* size of the IO buffer */


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

typedef enum {
	eWRKTHRDCMD_NEVER_RAN = 0,	/* granted, that's more a state than a cmd - thread is dead... */
	eWRKTHRDCMD_TERMINATED = 1,	/* granted, that's more a state than a cmd - thread is dead... */
	/* ALL active states MUST be numerically higher than eWRKTHRDCMD_TERMINATED and NONE must be lower! */
	eWRKTHRDCMD_RUN = 2,
	eWRKTHRDCMD_SHUTDOWN = 3,
	eWRKTHRDCMD_SHUTDOWN_IMMEDIATE = 4
} qWrkCmd_t;	/* commands for queue worker threads */

typedef struct qWrkThrd_s {
	pthread_t thrdID;  /* thread ID */
	qWrkCmd_t tCurrCmd; /* current command to be carried out by worker */
} qWrkThrd_t;	/* type for queue worker threads */

/* the queue object */
typedef struct queue_s {
	BEGINobjInstance;
	queueType_t	qType;
	int	iQueueSize;	/* Current number of elements in the queue */
	int	iMaxQueueSize;	/* how large can the queue grow? */
	int 	iNumWorkerThreads;/* number of worker threads to use */
	int 	iCurNumWrkThrd;/* current number of active worker threads */
	qWrkThrd_t *pWrkThrds;/* array with control structure for the worker thread(s) associated with this queue */
	int	iUpdsSincePersist;/* nbr of queue updates since the last persist call */
	int	iPersistUpdCnt;	/* persits queue info after this nbr of updates - 0 -> persist only on shutdown */
	int	iHighWtrMrk;	/* high water mark for disk-assisted memory queues */
	//int	bWasBelowHighWtr;/* when running in DA mode: queue was below high wtr mrk at least once */
	int	iLowWtrMrk;	/* low water mark for disk-assisted memory queues */
	int	iDiscardMrk;	/* if the queue is above this mark, low-severity messages are discarded */
	int	iDiscardSeverity;/* messages of this severity above are discarded on too-full queue */
	int	bNeedDelQIF;	/* does the QIF file need to be deleted when queue becomes empty? */
	int	toQShutdown;	/* timeout for regular queue shutdown in ms */
	int	toActShutdown;	/* timeout for long-running action shutdown in ms */
	int	toEnq;		/* enqueue timeout */
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
	pthread_cond_t condThrdTrm;/* signalled when threads terminate */
	pthread_cond_t *condSignalOnEmpty;/* caller-provided condition to be signalled when queue is empty (DA mode!) */
	pthread_mutex_t *mutSignalOnEmpty; /* and its associated mutex */
	pthread_cond_t *condSignalOnEmpty2;/* another condition to be signalled on empty */
	int bSignalOnEmpty;		/* signal caller when queue is empty via xxxSignalOnEmpty cond/mut,
					   0  = do not, 1 = signal only condSignalOnEmpty, 2 = signal both condSig..*/
	int bThrdStateChanged;		/* at least one thread state has changed if 1 */
	/* end sync variables */
	/* the following variables are always present, because they
	 * are not only used for the "disk" queueing mode but also for
	 * any other queueing mode if it is set to "disk assisted".
	 * rgerhards, 2008-01-09
	 */
	uchar *pszSpoolDir;
	size_t lenSpoolDir;
	uchar *pszFilePrefix;
	size_t lenFilePrefix;
	int iNumberFiles;	/* how many files make up the queue? */
	size_t iMaxFileSize;	/* max size for a single queue file */
	int bIsDA;		/* is this queue disk assisted? */
	enum {
		QRUNS_REGULAR,
		QRUNS_DA_INIT,
		QRUNS_DA
	} qRunsDA;		/* is this queue actually *running* disk assisted? if so, which mode? */
	pthread_mutex_t mutDA;	/* mutex for low water mark algo */
	pthread_cond_t condDA;	/* and its matching condition */
	struct queue_s *pqDA;	/* queue for disk-assisted modes */
	/* now follow queueing mode specific data elements */
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
			strm_t *pWrite; /* current file to be written */
			strm_t *pRead;  /* current file to be read */
		} disk;
	} tVars;
} queue_t;

/* prototypes */
rsRetVal queueDestruct(queue_t *pThis);
rsRetVal queueEnqObj(queue_t *pThis, void *pUsr);
rsRetVal queueStart(queue_t *pThis);
rsRetVal queueSetMaxFileSize(queue_t *pThis, size_t iMaxFileSize);
rsRetVal queueSetFilePrefix(queue_t *pThis, uchar *pszPrefix, size_t iLenPrefix);
rsRetVal queueConstruct(queue_t **ppThis, queueType_t qType, int iWorkerThreads,
		        int iMaxQueueSize, rsRetVal (*pConsumer)(void*));
PROTOTYPEObjClassInit(queue);
PROTOTYPEpropSetMeth(queue, iPersistUpdCnt, int);
PROTOTYPEpropSetMeth(queue, toQShutdown, long);
PROTOTYPEpropSetMeth(queue, toActShutdown, long);
PROTOTYPEpropSetMeth(queue, toEnq, long);
PROTOTYPEpropSetMeth(queue, iHighWtrMrk, int);
PROTOTYPEpropSetMeth(queue, iLowWtrMrk, int);
PROTOTYPEpropSetMeth(queue, iDiscardMrk, int);
PROTOTYPEpropSetMeth(queue, iDiscardSeverity, int);
#define queueGetID(pThis) ((unsigned long) pThis)

#endif /* #ifndef QUEUE_H_INCLUDED */
