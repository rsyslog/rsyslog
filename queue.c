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

#include <stdio.h>
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
DEFobjStaticHelpers

/* forward-definitions */
rsRetVal queueChkPersist(queue_t *pThis);

/* methods */

/* compute an absolute time timeout suitable for calls to pthread_cond_timedwait()
 * rgerhards, 2008-01-14
 */
static rsRetVal
queueTimeoutComp(struct timespec *pt, int iTimeout)
{
	assert(pt != NULL);
	/* compute timeout */
	clock_gettime(CLOCK_REALTIME, pt);
	pt->tv_nsec += (iTimeout % 1000) * 1000000; /* think INTEGER arithmetic! */
	if(pt->tv_nsec > 999999999) { /* overrun? */
		pt->tv_nsec -= 1000000000;
		++pt->tv_sec;
	}
	pt->tv_sec += iTimeout / 1000;
	return RS_RET_OK; /* so far, this is static... */
}


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

	if(pThis->iMaxQueueSize == 0)
		ABORT_FINALIZE(RS_RET_QSIZE_ZERO);

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


/* This method checks if there is any persistent information on the
 * queue.
 */
#if 0
static rsRetVal 
queueTryLoadPersistedInfo(queue_t *pThis)
{
	DEFiRet;
	strm_t *psQIF = NULL;
	uchar pszQIFNam[MAXFNAME];
	size_t lenQIFNam;
	struct stat stat_buf;
}
#endif


static rsRetVal
queueLoadPersStrmInfoFixup(strm_t *pStrm, queue_t *pThis)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pStrm, strm);
	ISOBJ_TYPE_assert(pThis, queue);
	CHKiRet(strmSetDir(pStrm, glblGetWorkDir(), strlen((char*)glblGetWorkDir())));
finalize_it:
	return iRet;
}


/* The method loads the persistent queue information.
 * rgerhards, 2008-01-11
 */
static rsRetVal 
queueTryLoadPersistedInfo(queue_t *pThis)
{
	DEFiRet;
	strm_t *psQIF = NULL;
	uchar pszQIFNam[MAXFNAME];
	size_t lenQIFNam;
	struct stat stat_buf;

	ISOBJ_TYPE_assert(pThis, queue);

	/* Construct file name */
	lenQIFNam = snprintf((char*)pszQIFNam, sizeof(pszQIFNam) / sizeof(uchar), "%s/%s.qi",
			     (char*) glblGetWorkDir(), (char*)pThis->pszFilePrefix);

	/* check if the file exists */
dbgprintf("stat '%s'\n", pszQIFNam);
	if(stat((char*) pszQIFNam, &stat_buf) == -1) {
		if(errno == ENOENT) {
			dbgprintf("Queue 0x%lx: clean startup, no .qi file found\n", queueGetID(pThis));
			ABORT_FINALIZE(RS_RET_FILE_NOT_FOUND);
		} else {
			dbgprintf("Queue 0x%lx: error %d trying to access .qi file\n", queueGetID(pThis), errno);
			ABORT_FINALIZE(RS_RET_IO_ERROR);
		}
	}

	/* If we reach this point, we have a .qi file */

	CHKiRet(strmConstruct(&psQIF));
	CHKiRet(strmSetDir(psQIF, glblGetWorkDir(), strlen((char*)glblGetWorkDir())));
	CHKiRet(strmSettOperationsMode(psQIF, STREAMMODE_READ));
	CHKiRet(strmSetsType(psQIF, STREAMTYPE_FILE_SINGLE));
	CHKiRet(strmSetFName(psQIF, pszQIFNam, lenQIFNam));
	CHKiRet(strmConstructFinalize(psQIF));

	/* first, we try to read the property bag for ourselfs */
	CHKiRet(objDeserializePropBag((obj_t*) pThis, psQIF));
	
	/* and now the stream objects (some order as when persisted!) */
	CHKiRet(objDeserialize(&pThis->tVars.disk.pWrite, OBJstrm, psQIF,
			       (rsRetVal(*)(obj_t*,void*))queueLoadPersStrmInfoFixup, pThis));
	CHKiRet(objDeserialize(&pThis->tVars.disk.pRead, OBJstrm, psQIF,
			       (rsRetVal(*)(obj_t*,void*))queueLoadPersStrmInfoFixup, pThis));

	CHKiRet(strmSeekCurrOffs(pThis->tVars.disk.pWrite));
	CHKiRet(strmSeekCurrOffs(pThis->tVars.disk.pRead));

	/* OK, we could successfully read the file, so we now can request that it be
	 * deleted when we are done with the persisted information.
	 */
	pThis->bNeedDelQIF = 1;

finalize_it:
	if(psQIF != NULL)
		strmDestruct(psQIF);

	if(iRet != RS_RET_OK) {
		dbgprintf("Queue 0x%lx: error %d reading .qi file - can not start queue\n",
			  queueGetID(pThis), iRet);
	}

	return iRet;
}


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
	int bRestarted = 0;

	assert(pThis != NULL);

	/* and now check if there is some persistent information that needs to be read in */
	iRet = queueTryLoadPersistedInfo(pThis);
	if(iRet == RS_RET_OK)
		bRestarted = 1;
	else if(iRet != RS_RET_FILE_NOT_FOUND)
			FINALIZE;

dbgprintf("qConstructDisk: bRestarted %d, iRet %d\n", bRestarted, iRet);
	if(bRestarted == 1) {
		;
	} else {
		CHKiRet(strmConstruct(&pThis->tVars.disk.pWrite));
		CHKiRet(strmSetDir(pThis->tVars.disk.pWrite, glblGetWorkDir(), strlen((char*)glblGetWorkDir())));
		CHKiRet(strmSetiMaxFiles(pThis->tVars.disk.pWrite, 10000000));
		CHKiRet(strmSettOperationsMode(pThis->tVars.disk.pWrite, STREAMMODE_WRITE));
		CHKiRet(strmSetsType(pThis->tVars.disk.pWrite, STREAMTYPE_FILE_CIRCULAR));
		CHKiRet(strmConstructFinalize(pThis->tVars.disk.pWrite));

		CHKiRet(strmConstruct(&pThis->tVars.disk.pRead));
		CHKiRet(strmSetbDeleteOnClose(pThis->tVars.disk.pRead, 1));
		CHKiRet(strmSetDir(pThis->tVars.disk.pRead, glblGetWorkDir(), strlen((char*)glblGetWorkDir())));
		CHKiRet(strmSetiMaxFiles(pThis->tVars.disk.pRead, 10000000));
		CHKiRet(strmSettOperationsMode(pThis->tVars.disk.pRead, STREAMMODE_READ));
		CHKiRet(strmSetsType(pThis->tVars.disk.pRead, STREAMTYPE_FILE_CIRCULAR));
		CHKiRet(strmConstructFinalize(pThis->tVars.disk.pRead));


		CHKiRet(strmSetFName(pThis->tVars.disk.pWrite, pThis->pszFilePrefix, pThis->lenFilePrefix));
		CHKiRet(strmSetFName(pThis->tVars.disk.pRead,  pThis->pszFilePrefix, pThis->lenFilePrefix));
	}

	/* now we set (and overwrite in case of a persisted restart) some parameters which
	 * should always reflect the current configuration variables. Be careful by doing so,
	 * for example file name generation must not be changed as that would break the
	 * ability to read existing queue files. -- rgerhards, 2008-01-12
	 */
CHKiRet(strmSetiMaxFileSize(pThis->tVars.disk.pWrite, pThis->iMaxFileSize));
CHKiRet(strmSetiMaxFileSize(pThis->tVars.disk.pRead, pThis->iMaxFileSize));

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
	return objDeserialize(ppUsr, OBJMsg, pThis->tVars.disk.pRead, NULL, NULL);
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
			  queueGetID(pThis), iRetLocal);
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

	dbgprintf("Queue 0x%lx: entry added, size now %d entries\n", queueGetID(pThis), pThis->iQueueSize);

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
		  queueGetID(pThis), iRet, pThis->iQueueSize);

	return iRet;
}


/* Send a shutdown command to all workers and see if they terminate.
 * A timeout may be specified.
 * rgerhards, 2008-01-14
 */
static rsRetVal
queueWrkThrdTrm(queue_t *pThis, qWrkCmd_t tShutdownCmd, long iTimeout)
{
	DEFiRet;
	int i;
	int bTimedOut;
	struct timespec t;

	/* first tell the workers our request */
	for(i = 0 ; i < pThis->iNumWorkerThreads ; ++i)
		if(pThis->pWrkThrds[i].tCurrCmd != eWRKTHRDCMD_TERMINATED)
			pThis->pWrkThrds[i].tCurrCmd = tShutdownCmd;

	/* awake them... */
	pthread_cond_broadcast(pThis->notEmpty);

	/* get timeout */
	queueTimeoutComp(&t, iTimeout);
		
	/* and wait for their termination */
	pthread_mutex_lock(pThis->mut);
	bTimedOut = 0;
	while(pThis->iCurNumWrkThrd > 0 && !bTimedOut) {
		dbgprintf("Queue 0x%lx: waiting %ld ms on worker thread termination, %d still running\n",
			   queueGetID(pThis), iTimeout, pThis->iCurNumWrkThrd);

		if(pthread_cond_timedwait(&pThis->condThrdTrm, pThis->mut, &t) != 0) {
			dbgprintf("Queue 0x%lx: timeout waiting on worker thread termination\n", queueGetID(pThis));
			bTimedOut = 1;	/* we exit the loop on timeout */
		}
	}
	pthread_mutex_unlock(pThis->mut);

	if(bTimedOut)
		iRet = RS_RET_TIMED_OUT;

	return iRet;
}


/* Unconditionally cancel all running worker threads.
 * rgerhards, 2008-01-14
 */
static rsRetVal
queueWrkThrdCancel(queue_t *pThis)
{
	DEFiRet;
	int i;
	// TODO: we need to implement peek(), without it (today!) we lose one message upon
	// worker cancellation! -- rgerhards, 2008-01-14

	/* awake the workers one more time, just to be sure */
	pthread_cond_broadcast(pThis->notEmpty);

	/* first tell the workers our request */
	for(i = 0 ; i < pThis->iNumWorkerThreads ; ++i)
		if(pThis->pWrkThrds[i].tCurrCmd != eWRKTHRDCMD_TERMINATED) {
			dbgprintf("Queue 0x%lx: canceling worker thread %d\n", queueGetID(pThis), i);
			pthread_cancel(pThis->pWrkThrds[i].thrdID);
		}

	return iRet;
}


/* Worker thread management function carried out when the main
 * worker is about to terminate.
 */
static rsRetVal queueShutdownWorkers(queue_t *pThis)
{
	DEFiRet;
	int i;

	assert(pThis != NULL);

	dbgprintf("Queue 0x%lx: initiating worker thread shutdown sequence\n", (unsigned long) pThis);

	/* even if the timeout count is set to 0 (run endless), we still call the queueWrkThrdTrm(). This
	 * is necessary so that all threads get sent the termination command. With a timeout of 0, however,
	 * the function returns immediate with RS_RET_TIMED_OUT. We catch that state and accept it as
	 * good.
	 */
	iRet = queueWrkThrdTrm(pThis, eWRKTHRDCMD_SHUTDOWN, pThis->toQShutdown);
	if(iRet == RS_RET_TIMED_OUT) {
		if(pThis->toQShutdown == 0) {
			iRet = RS_RET_OK;
		} else {
			/* OK, we now need to try force the shutdown */
			dbgprintf("Queue 0x%lx: regular worker shutdown timed out, now trying immediate\n",
				  queueGetID(pThis));
			iRet = queueWrkThrdTrm(pThis, eWRKTHRDCMD_SHUTDOWN_IMMEDIATE, pThis->toActShutdown);
		}
	}

	if(iRet != RS_RET_OK) { /* this is true on actual error on first try or timeout and error on second */
		/* still didn't work out - so we now need to cancel the workers */
		dbgprintf("Queue 0x%lx: worker threads could not be shutdown, now canceling them\n", (unsigned long) pThis);
		iRet = queueWrkThrdCancel(pThis);
	}
	
	/* finally join the threads
	 * In case of a cancellation, this may actually take some time. This is also
	 * needed to clean up the thread descriptors, even with a regular termination.
	 * And, most importantly, this is needed if we have an indifitite termination
	 * time set (timeout == 0)! -- rgerhards, 2008-01-14
	 */
	for(i = 0 ; i < pThis->iNumWorkerThreads ; ++i) {
dbgprintf("join thred %d\n", i);
		pthread_join(pThis->pWrkThrds[i].thrdID, NULL);
	}

	/* as we may have cancelled a thread, clean up our internal structure. All are
	 * terminated now. For simplicity, we simply overwrite the states.
	 */
	for(i = 0 ; i < pThis->iNumWorkerThreads ; ++i) {
		pThis->pWrkThrds[i].tCurrCmd = eWRKTHRDCMD_TERMINATED;
	}

	dbgprintf("Queue 0x%lx: worker threads terminated, remaining queue size %d.\n",
		  queueGetID(pThis), pThis->iQueueSize);

	return iRet;
}


/* Each queue has one associated worker (consumer) thread. It will pull
 * the message from the queue and pass it to a user-defined function.
 * This function was provided on construction. It MUST be thread-safe.
 */
static void *
queueWorker(void *arg)
{
	DEFiRet;
	queue_t *pThis = (queue_t*) arg;
	void *pUsr;
	sigset_t sigSet;
	int iMyThrdIndx;	/* index for this thread in queue thread table */
	int iCancelStateSave;
	int iSeverity;
	rsRetVal iRetLocal;

	assert(pThis != NULL);

	sigfillset(&sigSet);
	pthread_sigmask(SIG_BLOCK, &sigSet, NULL);

	/* first find myself in the queue's thread table */
	for(iMyThrdIndx = 0 ; iMyThrdIndx < pThis->iNumWorkerThreads ; ++iMyThrdIndx)
		if(pThis->pWrkThrds[iMyThrdIndx].thrdID == pthread_self())
			break;
	assert(pThis->pWrkThrds[iMyThrdIndx].thrdID == pthread_self());

	dbgprintf("Queue 0x%lx/w%d: worker thread startup.\n", queueGetID(pThis), iMyThrdIndx);

	/* tell the world there is one more worker */
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave);
	pthread_mutex_lock(pThis->mut);
	pThis->iCurNumWrkThrd++;
	pthread_mutex_unlock(pThis->mut);
	pthread_setcancelstate(iCancelStateSave, NULL);

	/* now we have our identity, on to real processing */
	while(pThis->pWrkThrds[iMyThrdIndx].tCurrCmd == eWRKTHRDCMD_RUN
	      || (pThis->pWrkThrds[iMyThrdIndx].tCurrCmd == eWRKTHRDCMD_SHUTDOWN && pThis->iQueueSize > 0)) {
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave);
		pthread_mutex_lock(pThis->mut);
		while (pThis->iQueueSize == 0 && pThis->pWrkThrds[iMyThrdIndx].tCurrCmd == eWRKTHRDCMD_RUN) {
			dbgprintf("Queue 0x%lx/w%d: queue EMPTY, waiting for next message.\n",
				  queueGetID(pThis), iMyThrdIndx);
			pthread_cond_wait(pThis->notEmpty, pThis->mut);
		}
		if(pThis->iQueueSize > 0) {
			/* dequeue element (still protected from mutex) */
			iRet = queueDel(pThis, &pUsr);
			queueChkPersist(pThis); // when we support peek(), we must do this down after the del!
			pthread_mutex_unlock(pThis->mut);
			pthread_cond_signal(pThis->notFull);
			pthread_setcancelstate(iCancelStateSave, NULL);
			/* Now make sure we can get canceled - it is not specified if pthread_setcancelstate() is
			 * a cancellation point in itself. As we run most of the time without cancel enabled, I fear
			 * we may never get cancelled if we do not create a cancellation point ourselfs.
			 */
			pthread_testcancel();
			/* do actual processing (the lengthy part, runs in parallel)
			 * If we had a problem while dequeing, we do not call the consumer,
			 * but we otherwise ignore it. This is in the hopes that it will be
			 * self-healing. Howerver, this is really not a good thing.
			 * rgerhards, 2008-01-03
			 */
			if(iRet == RS_RET_OK) {
				/* do a quick check if we need to drain the queue */
				if(pThis->iDiscardMrk > 0 && pThis->iQueueSize >= pThis->iDiscardMrk) {
					iRetLocal = objGetSeverity(pUsr, &iSeverity);
					if(iRetLocal == RS_RET_OK && iSeverity >= pThis->iDiscardSeverity) {
						dbgprintf("Queue 0x%lx/w%d: dequeue/queue nearly full (%d entries), "
							  "discarded severity %d message\n",
							  queueGetID(pThis), iMyThrdIndx, pThis->iQueueSize, iSeverity);
						objDestruct(pUsr);
					}
				} else {
					dbgprintf("Queue 0x%lx/w%d: worker executes consumer...\n",
						   queueGetID(pThis), iMyThrdIndx);
					iRetLocal = pThis->pConsumer(pUsr);
					if(iRetLocal != RS_RET_OK)
						dbgprintf("Queue 0x%lx/w%d: Consumer returned iRet %d\n",
							  queueGetID(pThis), iMyThrdIndx, iRetLocal);
				}
			} else {
				dbgprintf("Queue 0x%lx/w%d: error %d dequeueing element - ignoring, but strange things "
				          "may happen\n", queueGetID(pThis), iMyThrdIndx, iRet);
			}
		} else { /* the mutex must be unlocked in any case (important for termination) */
			pthread_mutex_unlock(pThis->mut);
			pthread_setcancelstate(iCancelStateSave, NULL);
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
				  " %d messages to process.\n", queueGetID(pThis), iMyThrdIndx, pThis->iQueueSize);
	}

	/* indicate termination */
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave);
	pthread_mutex_lock(pThis->mut);
	pThis->iCurNumWrkThrd--;
	pThis->pWrkThrds[iMyThrdIndx].tCurrCmd = eWRKTHRDCMD_TERMINATED;
	pthread_cond_signal(&pThis->condThrdTrm); /* important for shutdown situation */
	dbgprintf("Queue 0x%lx/w%d: thread terminates with %d entries left in queue, %d workers running.\n",
		  queueGetID(pThis), iMyThrdIndx, pThis->iQueueSize, pThis->iCurNumWrkThrd);
	pthread_mutex_unlock(pThis->mut);
	pthread_setcancelstate(iCancelStateSave, NULL);

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
	objConstructSetObjInfo(pThis);
	if((pThis->pszSpoolDir = (uchar*) strdup((char*)glblGetWorkDir())) == NULL)
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

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

finalize_it:
	OBJCONSTRUCT_CHECK_SUCCESS_AND_CLEANUP
	return iRet;
}


/* start up the queue - it must have been constructed and parameters defined
 * before.
 */
rsRetVal queueStart(queue_t *pThis) /* this is the ConstructionFinalizer */
{
	DEFiRet;
	int iState;
	int i;

	assert(pThis != NULL);

	/* call type-specific constructor */
	CHKiRet(pThis->qConstruct(pThis));

	dbgprintf("Queue 0x%lx: type %d, maxFileSz %ld starting\n", queueGetID(pThis), pThis->qType,
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


#if 0
/* persist disk status on disk. This is necessary if we run either
 * a disk queue or in a disk assisted mode.
 */
static rsRetVal queuePersistDskFilInfo(queue_t *pThis)
{
	DEFiRet;

	assert(pThis != NULL);


finalize_it:
	return iRet;
}
#endif



/* persist the queue to disk. If we have something to persist, we first
 * save the information on the queue properties itself and then we call
 * the queue-type specific drivers.
 * rgerhards, 2008-01-10
 */
static rsRetVal queuePersist(queue_t *pThis)
{
	DEFiRet;
	strm_t *psQIF = NULL; /* Queue Info File */
	uchar pszQIFNam[MAXFNAME];
	size_t lenQIFNam;

	assert(pThis != NULL);

	if(pThis->qType != QUEUETYPE_DISK) {
		if(pThis->iQueueSize > 0)
			ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED); /* TODO: later... */
		else
			FINALIZE; /* if the queue is empty, we are happy and done... */
	}

	dbgprintf("Queue 0x%lx: persisting queue to disk, %d entries...\n", queueGetID(pThis), pThis->iQueueSize);
	/* Construct file name */
	lenQIFNam = snprintf((char*)pszQIFNam, sizeof(pszQIFNam) / sizeof(uchar), "%s/%s.qi",
		             (char*) glblGetWorkDir(), (char*)pThis->pszFilePrefix);

	if(pThis->iQueueSize == 0) {
		if(pThis->bNeedDelQIF) {
			unlink((char*)pszQIFNam);
			pThis->bNeedDelQIF = 0;
		}
		/* indicate spool file needs to be deleted */
		CHKiRet(strmSetbDeleteOnClose(pThis->tVars.disk.pRead, 1));
		FINALIZE; /* nothing left to do, so be happy */
	}

	CHKiRet(strmConstruct(&psQIF));
	CHKiRet(strmSetDir(psQIF, glblGetWorkDir(), strlen((char*)glblGetWorkDir())));
	CHKiRet(strmSettOperationsMode(psQIF, STREAMMODE_WRITE));
	CHKiRet(strmSetiAddtlOpenFlags(psQIF, O_TRUNC));
	CHKiRet(strmSetsType(psQIF, STREAMTYPE_FILE_SINGLE));
	CHKiRet(strmSetFName(psQIF, pszQIFNam, lenQIFNam));
	CHKiRet(strmConstructFinalize(psQIF));

	/* first, write the property bag for ourselfs
	 * And, surprisingly enough, we currently need to persist only the size of the
	 * queue. All the rest is re-created with then-current config parameters when the
	 * queue is re-created. Well, we'll also save the current queue type, just so that
	 * we know when somebody has changed the queue type... -- rgerhards, 2008-01-11
	 */
	CHKiRet(objBeginSerializePropBag(psQIF, (obj_t*) pThis));
	objSerializeSCALAR(psQIF, iQueueSize, INT);
	CHKiRet(objEndSerialize(psQIF));

	/* this is disk specific and must be moved to a function */
	CHKiRet(strmSerialize(pThis->tVars.disk.pWrite, psQIF));
	CHKiRet(strmSerialize(pThis->tVars.disk.pRead, psQIF));
	
	/* persist queue object itself */

	/* tell the input file object that it must not delete the file on close if the queue is non-empty */
	CHKiRet(strmSetbDeleteOnClose(pThis->tVars.disk.pRead, 0));

	/* we have persisted the queue object. So whenever it comes to an empty queue,
	 * we need to delete the QIF. Thus, we indicte that need.
	 */
	pThis->bNeedDelQIF = 1;

finalize_it:
	if(psQIF != NULL)
		strmDestruct(psQIF);

	return iRet;
}


/* check if we need to persist the current queue info. If an
 * error occurs, thus should be ignored by caller (but we still
 * abide to our regular call interface)...
 * rgerhards, 2008-01-13
 */
rsRetVal queueChkPersist(queue_t *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, queue);

	if(pThis->iPersistUpdCnt && ++pThis->iUpdsSincePersist >= pThis->iPersistUpdCnt) {
		queuePersist(pThis);
		pThis->iUpdsSincePersist = 0;
	}

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

	/* persist the queue (we always do that - queuePersits() does cleanup it the queue is empty) */
	CHKiRet_Hdlr(queuePersist(pThis)) {
		dbgprintf("Queue 0x%lx: error %d persisting queue - data lost!\n", (unsigned long) pThis, iRet);
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

	if(pThis->pszFilePrefix != NULL)
		free(pThis->pszFilePrefix);

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

	if(pThis->pszFilePrefix != NULL)
		free(pThis->pszFilePrefix);

	if((pThis->pszFilePrefix = malloc(sizeof(uchar) * iLenPrefix + 1)) == NULL)
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	memcpy(pThis->pszFilePrefix, pszPrefix, iLenPrefix + 1);
	pThis->lenFilePrefix = iLenPrefix;

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
	int iSeverity = 8;
	rsRetVal iRetLocal;

	assert(pThis != NULL);

	/* Please note that this function is not cancel-safe and consequently
	 * sets the calling thread's cancelibility state to PTHREAD_CANCEL_DISABLE
	 * during its execution. If that is not done, race conditions occur if the
	 * thread is canceled (most important use case is input module termination).
	 * rgerhards, 2008-01-08
	 */
	if(pThis->pWrkThrds != NULL) {
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave);
		pthread_mutex_lock(pThis->mut);
	}

	if(pThis->iDiscardMrk > 0 && pThis->iQueueSize >= pThis->iDiscardMrk) {
		iRetLocal = objGetSeverity(pUsr, &iSeverity);
		if(iRetLocal == RS_RET_OK && iSeverity >= pThis->iDiscardSeverity) {
			dbgprintf("Queue 0x%lx: queue nearly full (%d entries), discarded severity %d message\n",
				  queueGetID(pThis), pThis->iQueueSize, iSeverity);
			objDestruct(pUsr);
			ABORT_FINALIZE(RS_RET_QUEUE_FULL);
		} else {
			dbgprintf("Queue 0x%lx: queue nearly full (%d entries), but could not drop msg "
				  "(iRet: %d, severity %d)\n", queueGetID(pThis), pThis->iQueueSize,
				  iRetLocal, iSeverity);
		}
	}

		
	while(pThis->iMaxQueueSize > 0 && pThis->iQueueSize >= pThis->iMaxQueueSize) {
		dbgprintf("Queue 0x%lx: enqueueMsg: queue FULL - waiting to drain.\n", queueGetID(pThis));
		queueTimeoutComp(&t, pThis->toEnq);
		if(pthread_cond_timedwait (pThis->notFull,
					   pThis->mut, &t) != 0) {
			dbgprintf("Queue 0x%lx: enqueueMsg: cond timeout, dropping message!\n", queueGetID(pThis));
			objDestruct(pUsr);
			ABORT_FINALIZE(RS_RET_QUEUE_FULL);
		}
	}
	CHKiRet(queueAdd(pThis, pUsr));
	queueChkPersist(pThis);

finalize_it:
	/* now activate the worker thread */
	if(pThis->pWrkThrds != NULL) {
		pthread_mutex_unlock(pThis->mut);
		i = pthread_cond_signal(pThis->notEmpty);
		dbgprintf("Queue 0x%lx: EnqueueMsg signaled condition (%d)\n", (unsigned long) pThis, i);
		pthread_setcancelstate(iCancelStateSave, NULL);
	}


	return iRet;
}

/* some simple object access methods */
DEFpropSetMeth(queue, iPersistUpdCnt, int);
DEFpropSetMeth(queue, toQShutdown, long);
DEFpropSetMeth(queue, toActShutdown, long);
DEFpropSetMeth(queue, toEnq, long);
DEFpropSetMeth(queue, iHighWtrMrk, int);
DEFpropSetMeth(queue, iLowWtrMrk, int);
DEFpropSetMeth(queue, iDiscardMrk, int);
DEFpropSetMeth(queue, iDiscardSeverity, int);


/* This function can be used as a generic way to set properties. Only the subset
 * of properties required to read persisted property bags is supported. This
 * functions shall only be called by the property bag reader, thus it is static.
 * rgerhards, 2008-01-11
 */
#define isProp(name) !rsCStrSzStrCmp(pProp->pcsName, (uchar*) name, sizeof(name) - 1)
static rsRetVal queueSetProperty(queue_t *pThis, property_t *pProp)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, queue);
	assert(pProp != NULL);

 	if(isProp("iQueueSize")) {
		pThis->iQueueSize = pProp->val.vInt;
 	} else if(isProp("qType")) {
		if(pThis->qType != pProp->val.vLong)
			ABORT_FINALIZE(RS_RET_QTYPE_MISMATCH);
	}

finalize_it:
	return iRet;
}
#undef	isProp


/* Initialize the stream class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-01-09
 */
BEGINObjClassInit(queue, 1)
	//OBJSetMethodHandler(objMethod_SERIALIZE, strmSerialize);
	OBJSetMethodHandler(objMethod_SETPROPERTY, queueSetProperty);
	//OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, strmConstructFinalize);
ENDObjClassInit(queue)

/*
 * vi:set ai:
 */
