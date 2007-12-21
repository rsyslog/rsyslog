/* TODO: we should guard the individual thread actions with a mutex. Else, we may
 * run into race conditions on thread termination.
 */

/* threads.c
 *
 * This file implements threading support helpers (and maybe the thread object)
 * for rsyslog.
 * 
 * File begun on 2007-12-14 by RGerhards
 *
 * Copyright 2007 Rainer Gerhards and Adiscon GmbH.
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
#include <signal.h>
#include <pthread.h>
#include <assert.h>

#include "rsyslog.h"
#include "syslogd.h"
#include "linkedlist.h"
#include "threads.h"

/* static data */
int iMainMsgQueueSize;
msgQueue *pMsgQueue = NULL;

/* linked list of currently-known threads */
static linkedList_t llThrds;

/* methods */

/* Construct a new thread object
 */
static rsRetVal thrdConstruct(thrdInfo_t **ppThis)
{
	thrdInfo_t *pThis;

	assert(ppThis != NULL);

	if((pThis = calloc(1, sizeof(thrdInfo_t))) == NULL)
		return RS_RET_OUT_OF_MEMORY;

	/* OK, we got the element, now initialize members that should
	 * not be zero-filled.
	 */
	pThis->mutTermOK = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
	pthread_mutex_init (pThis->mutTermOK, NULL);

	*ppThis = pThis;
	return RS_RET_OK;
}


/* Destructs a thread object. The object must not be linked to the
 * linked list of threads. Please note that the thread should have been
 * stopped before. If not, we try to do it.
 */
static rsRetVal thrdDestruct(thrdInfo_t *pThis)
{
	assert(pThis != NULL);
dbgprintf("thrdDestruct, pThis: %lx\n", pThis);

	if(pThis->bIsActive == 1) {
		thrdTerminate(pThis);
	}
	free(pThis->mutTermOK);
	free(pThis);

	return RS_RET_OK;
}


/* terminate a thread gracefully. It's termination sync state is taken into
 * account.
 */
rsRetVal thrdTerminate(thrdInfo_t *pThis)
{
	assert(pThis != NULL);
	
dbgprintf("Terminate thread %lx via method %d\n", pThis->thrdID, pThis->eTermTool);
	if(pThis->eTermTool == eTermSync_SIGNAL) {
		/* we first wait for the thread to reach a point in execution where it
		 * is safe to terminate it.
		 * TODO: TIMEOUT!
		 */
		pthread_mutex_lock(pThis->mutTermOK);
		pThis->bShallStop = 1;	/* request termination */
		pthread_kill(pThis->thrdID, SIGUSR2); /* get thread out ouf blocking calls */
		pthread_join(pThis->thrdID, NULL);
		pthread_mutex_unlock(pThis->mutTermOK);	 /* cleanup... */
		/* TODO: TIMEOUT! */
	} else if(pThis->eTermTool == eTermSync_NONE) {
		pthread_cancel(pThis->thrdID);
		pthread_join(pThis->thrdID, NULL); /* wait for cancel to complete */
	}
	pThis->bIsActive = 0;

	/* call cleanup function, if any */
	if(pThis->pAfterRun != NULL)
		pThis->pAfterRun(pThis);
	
	return RS_RET_OK;
}


/* terminate all known threads gracefully.
 */
rsRetVal thrdTerminateAll(void)
{
	llDestroy(&llThrds);
	return RS_RET_OK;
}


/* This is an internal wrapper around the user thread function. Its
 * purpose is to handle all the necessary housekeeping stuff so that the
 * user function needs not to be aware of the threading calls. The user
 * function call has just "normal", non-threading semantics.
 * rgerhards, 2007-12-17
 */
static void* thrdStarter(void *arg)
{
	DEFiRet;
	thrdInfo_t *pThis = (thrdInfo_t*) arg;

	assert(pThis != NULL);
	assert(pThis->pUsrThrdMain != NULL);

	/* block all signals except the one we need for graceful termination */
	sigset_t sigSet;
	sigfillset(&sigSet);
	pthread_sigmask(SIG_BLOCK, &sigSet, NULL);
	sigemptyset(&sigSet);
	sigaddset(&sigSet, SIGUSR2);
	pthread_sigmask(SIG_UNBLOCK, &sigSet, NULL);

	/* setup complete, we are now ready to execute the user code. We will not
	 * regain control until the user code is finished, in which case we terminate
	 * the thread.
	 */
	iRet = pThis->pUsrThrdMain(pThis);

	dbgprintf("thrdStarter: usrThrdMain 0x%lx returned with iRet %d, exiting now.\n", (unsigned long) pThis->thrdID, iRet);
	pthread_exit(0);
}

/* Start a new thread and add it to the list of currently
 * executing threads. It is added at the end of the list.
 * rgerhards, 2007-12-14
 */
rsRetVal thrdCreate(rsRetVal (*thrdMain)(thrdInfo_t*), eTermSyncType_t eTermSyncType, rsRetVal(*afterRun)(thrdInfo_t *))
{
	DEFiRet;
	thrdInfo_t *pThis;
	int i;

	assert(thrdMain != NULL);

	CHKiRet(thrdConstruct(&pThis));
	pThis->eTermTool = eTermSync_NONE; // eTermSyncType; TODO: review
	pThis->bIsActive = 1;
	pThis->pUsrThrdMain = thrdMain;
	pThis->pAfterRun = afterRun;
	i = pthread_create(&pThis->thrdID, NULL, thrdStarter, pThis);
	CHKiRet(llAppend(&llThrds, NULL, pThis));

finalize_it:
	return iRet;
}


/* This is a dummy handler. We user SIGUSR2 to interrupt blocking system calls
 * if we are in termination mode 1.
 */
static void sigusr2Dummy(int __attribute__((unused)) sig)
{
	dbgprintf("sigusr2Dummy called!\n");
}


/* initialize the thread-support subsystem
 * must be called once at the start of the program
 */
rsRetVal thrdInit(void)
{
	DEFiRet;
	struct sigaction sigAct;

	iRet = llInit(&llThrds, thrdDestruct, NULL, NULL);

	/* set up our termination subsystem */
	memset(&sigAct, 0, sizeof (sigAct));
	sigemptyset(&sigAct.sa_mask);
	sigAct.sa_handler = sigusr2Dummy;
	sigaction(SIGUSR2, &sigAct, NULL);

	return iRet;
}


/* de-initialize the thread subsystem
 * must be called once at the end of the program
 */
rsRetVal thrdExit(void)
{
	DEFiRet;

	iRet = llDestroy(&llThrds);

	return iRet;
}


/* thrdSleep() - a fairly portable way to put a thread to sleep. It 
 * will wake up when
 * a) the wake-time is over
 * b) the thread shall be terminated
 * Returns RS_RET_OK if all went well, RS_RET_TERMINATE_NOW if the calling
 * thread shall be terminated and any other state if an error happened.
 * rgerhards, 2007-12-17
 */
rsRetVal
thrdSleep(thrdInfo_t *pThis, int iSeconds, int iuSeconds)
{
	DEFiRet;
	struct timeval tvSelectTimeout;

	assert(pThis != NULL);
	tvSelectTimeout.tv_sec = iSeconds;
	tvSelectTimeout.tv_usec = iuSeconds; /* micro seconds */
	thrdUnblockTermination(pThis);
	/* there may be a race condition if pthread_kill() is called after unblock but
	 * before the select() is setup. TODO: check and re-eval -- rgerhards, 2007-12-20
	 */
	select(1, NULL, NULL, NULL, &tvSelectTimeout);
	if(pThis->bShallStop)
		iRet = RS_RET_TERMINATE_NOW;
	else
		thrdBlockTermination(pThis);
	return iRet;
}


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
