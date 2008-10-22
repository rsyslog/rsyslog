/* Definition of the queue support module.
 *
 * Copyright 2008 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
 *
 * The rsyslog runtime library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The rsyslog runtime library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the rsyslog runtime library.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 * A copy of the LGPL can be found in the file "COPYING.LESSER" in this distribution.
 */

#ifndef QUEUE_H_INCLUDED
#define QUEUE_H_INCLUDED

#include <pthread.h>
#include "obj.h"
#include "wtp.h"
#include "stream.h"

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


typedef struct qWrkThrd_s {
	pthread_t thrdID;  /* thread ID */
	qWrkCmd_t tCurrCmd; /* current command to be carried out by worker */
	obj_t *pUsr;        /* current user object being processed (or NULL if none) */
	struct queue_s *pQueue; /* my queue (important if only the work thread instance is passed! */
	int iThrd;	/* my worker thread array index */
	pthread_cond_t condInitDone; /* signaled when the thread startup is done (once per thread existance) */
	pthread_mutex_t mut;
} qWrkThrd_t;	/* type for queue worker threads */

/* the queue object */
typedef struct queue_s {
	BEGINobjInstance;
	queueType_t	qType;
	int	bOptimizeUniProc; /* cache for the equally-named global setting, pulled at time of queue creation */
	int	bEnqOnly;	/* does queue run in enqueue-only mode (1) or not (0)? */
	int	bSaveOnShutdown;/* persists everthing on shutdown (if DA!)? 1-yes, 0-no */
	int	bQueueStarted;	/* has queueStart() been called on this queue? 1-yes, 0-no */
	int	bQueueInDestruction;/* 1 if queue is in destruction process, 0 otherwise */
	int	iQueueSize;	/* Current number of elements in the queue */
	int	iMaxQueueSize;	/* how large can the queue grow? */
	int 	iNumWorkerThreads;/* number of worker threads to use */
	int 	iCurNumWrkThrd;/* current number of active worker threads */
	int	iMinMsgsPerWrkr;/* minimum nbr of msgs per worker thread, if more, a new worker is started until max wrkrs */
	wtp_t	*pWtpDA;
	wtp_t	*pWtpReg;
	void	*pUsr;		/* a global, user-supplied pointer. Is passed back to consumer. */
	int	iUpdsSincePersist;/* nbr of queue updates since the last persist call */
	int	iPersistUpdCnt;	/* persits queue info after this nbr of updates - 0 -> persist only on shutdown */
	int	iHighWtrMrk;	/* high water mark for disk-assisted memory queues */
	int	iLowWtrMrk;	/* low water mark for disk-assisted memory queues */
	int	iDiscardMrk;	/* if the queue is above this mark, low-severity messages are discarded */
	int	iFullDlyMrk;	/* if the queue is above this mark, FULL_DELAYable message are put on hold */
	int	iLightDlyMrk;	/* if the queue is above this mark, LIGHT_DELAYable message are put on hold */
	int	iDiscardSeverity;/* messages of this severity above are discarded on too-full queue */
	int	bNeedDelQIF;	/* does the QIF file need to be deleted when queue becomes empty? */
	int	toQShutdown;	/* timeout for regular queue shutdown in ms */
	int	toActShutdown;	/* timeout for long-running action shutdown in ms */
	int	toWrkShutdown;	/* timeout for idle workers in ms, -1 means indefinite (0 is immediate) */
	int	toEnq;		/* enqueue timeout */
	/* rate limiting settings (will be expanded) */
	int	iDeqSlowdown; /* slow down dequeue by specified nbr of microseconds */
	/* end rate limiting */
	/* dequeue time window settings (may also be expanded) */
	int iDeqtWinFromHr;	/* begin of dequeue time window (hour only) */
	int iDeqtWinToHr;	/* end of dequeue time window (hour only), set to 25 to disable deq window! */
	/* note that begin and end have specific semantics. It is a big difference if we have
	 * begin 4, end 22 or begin 22, end 4. In the later case, dequeuing will run from 10p,
	 * throughout the night and stop at 4 in the morning. In the first case, it will start
	 * at 4am, run throughout the day, and stop at 10 in the evening! So far, not logic is
	 * applied to detect user configuration errors (and tell me how should we detect what
	 * the user really wanted...). -- rgerhards, 2008-04-02
	 */
	/* ane dequeue time window */
	rsRetVal (*pConsumer)(void *,void*); /* user-supplied consumer function for dequeued messages */
	/* calling interface for pConsumer: arg1 is the global user pointer from this structure, arg2 is the
	 * user pointer that was dequeued (actual sample: for actions, arg1 is the pAction and arg2 is pointer
	 * to message)
	 * rgerhards, 2008-01-28
	 */
	/* type-specific handlers (set during construction) */
	rsRetVal (*qConstruct)(struct queue_s *pThis);
	rsRetVal (*qDestruct)(struct queue_s *pThis);
	rsRetVal (*qAdd)(struct queue_s *pThis, void *pUsr);
	rsRetVal (*qDel)(struct queue_s *pThis, void **ppUsr);
	/* end type-specific handler */
	/* synchronization variables */
	pthread_mutex_t mutThrdMgmt; /* mutex for the queue's thread management */
	pthread_mutex_t *mut; /* mutex for enqueing and dequeueing messages */
	pthread_cond_t notFull, notEmpty;
	pthread_cond_t belowFullDlyWtrMrk; /* below eFLOWCTL_FULL_DELAY watermark */
	pthread_cond_t belowLightDlyWtrMrk; /* below eFLOWCTL_FULL_DELAY watermark */
	pthread_cond_t condDAReady;/* signalled when the DA queue is fully initialized and ready for processing */
	int bChildIsDone;		/* set to 1 when the child DA queue has finished processing, 0 otherwise */
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
	int64 iMaxFileSize;	/* max size for a single queue file */
	int64 sizeOnDiskMax;    /* maximum size on disk allowed */
	int bIsDA;		/* is this queue disk assisted? */
	int bRunsDA;		/* is this queue actually *running* disk assisted? */
	struct queue_s *pqDA;	/* queue for disk-assisted modes */
	struct queue_s *pqParent;/* pointer to the parent (if this is a child queue) */
	int	bDAEnqOnly;	/* EnqOnly setting for DA queue */
	/* some data elements for the queueUngetObj() functionality. This list should always be short
	 * and is always kept in memory
	 */
	qLinkedList_t *pUngetRoot;
	qLinkedList_t *pUngetLast;
	int iUngottenObjs;	/* number of objects currently in the "ungotten" list */
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
			int64 sizeOnDisk; /* current amount of disk space used */
			int64 bytesRead;  /* number of bytes read from current (undeleted!) file */
			strm_t *pWrite; /* current file to be written */
			strm_t *pRead;  /* current file to be read */
		} disk;
	} tVars;
} queue_t;

/* some symbolic constants for easier reference */
#define QUEUE_MODE_ENQDEQ 0
#define QUEUE_MODE_ENQONLY 1

#define QUEUE_IDX_DA_WORKER 0 /* index for the DA worker (fixed) */
#define QUEUE_PTR_DA_WORKER(x) (&((pThis)->pWrkThrds[0]))

/* the define below is an "eternal" timeout for the timeout settings which require a value.
 * It is one day, which is not really eternal, but comes close to it if we think about
 * rsyslog (e.g.: do you want to wait on shutdown for more than a day? ;))
 * rgerhards, 2008-01-17
 */
#define QUEUE_TIMEOUT_ETERNAL 24 * 60 * 60 * 1000

/* prototypes */
rsRetVal queueDestruct(queue_t **ppThis);
rsRetVal queueEnqObj(queue_t *pThis, flowControl_t flwCtlType, void *pUsr);
rsRetVal queueStart(queue_t *pThis);
rsRetVal queueSetMaxFileSize(queue_t *pThis, size_t iMaxFileSize);
rsRetVal queueSetFilePrefix(queue_t *pThis, uchar *pszPrefix, size_t iLenPrefix);
rsRetVal queueConstruct(queue_t **ppThis, queueType_t qType, int iWorkerThreads,
		        int iMaxQueueSize, rsRetVal (*pConsumer)(void*,void*));
PROTOTYPEObjClassInit(queue);
PROTOTYPEpropSetMeth(queue, iPersistUpdCnt, int);
PROTOTYPEpropSetMeth(queue, iDeqtWinFromHr, int);
PROTOTYPEpropSetMeth(queue, iDeqtWinToHr, int);
PROTOTYPEpropSetMeth(queue, toQShutdown, long);
PROTOTYPEpropSetMeth(queue, toActShutdown, long);
PROTOTYPEpropSetMeth(queue, toWrkShutdown, long);
PROTOTYPEpropSetMeth(queue, toEnq, long);
PROTOTYPEpropSetMeth(queue, iHighWtrMrk, int);
PROTOTYPEpropSetMeth(queue, iLowWtrMrk, int);
PROTOTYPEpropSetMeth(queue, iDiscardMrk, int);
PROTOTYPEpropSetMeth(queue, iDiscardSeverity, int);
PROTOTYPEpropSetMeth(queue, iMinMsgsPerWrkr, int);
PROTOTYPEpropSetMeth(queue, bSaveOnShutdown, int);
PROTOTYPEpropSetMeth(queue, pUsr, void*);
PROTOTYPEpropSetMeth(queue, iDeqSlowdown, int);
PROTOTYPEpropSetMeth(queue, sizeOnDiskMax, int64);
#define queueGetID(pThis) ((unsigned long) pThis)

#endif /* #ifndef QUEUE_H_INCLUDED */
