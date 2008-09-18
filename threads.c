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
#include "dirty.h"
#include "linkedlist.h"
#include "threads.h"

/* linked list of currently-known threads */
static linkedList_t llThrds;

/* methods */

/* Construct a new thread object
 */
static rsRetVal thrdConstruct(thrdInfo_t **ppThis)
{
	DEFiRet;
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
	RETiRet;
}


/* Destructs a thread object. The object must not be linked to the
 * linked list of threads. Please note that the thread should have been
 * stopped before. If not, we try to do it.
 */
static rsRetVal thrdDestruct(thrdInfo_t *pThis)
{
	DEFiRet;
	assert(pThis != NULL);

	if(pThis->bIsActive == 1) {
		thrdTerminate(pThis);
	}
	free(pThis->mutTermOK);
	free(pThis);

	RETiRet;
}


/* terminate a thread gracefully.
 */
rsRetVal thrdTerminate(thrdInfo_t *pThis)
{
	DEFiRet;
	assert(pThis != NULL);
	
	pthread_cancel(pThis->thrdID);
	pthread_join(pThis->thrdID, NULL); /* wait for cancel to complete */
	pThis->bIsActive = 0;

	/* call cleanup function, if any */
	if(pThis->pAfterRun != NULL)
		pThis->pAfterRun(pThis);
	
	RETiRet;
}


/* terminate all known threads gracefully.
 */
rsRetVal thrdTerminateAll(void)
{
	DEFiRet;
	llDestroy(&llThrds);
	RETiRet;
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

	/* block all signals */
	sigset_t sigSet;
	sigfillset(&sigSet);
	pthread_sigmask(SIG_BLOCK, &sigSet, NULL);

	/* setup complete, we are now ready to execute the user code. We will not
	 * regain control until the user code is finished, in which case we terminate
	 * the thread.
	 */
	iRet = pThis->pUsrThrdMain(pThis);

	dbgprintf("thrdStarter: usrThrdMain 0x%lx returned with iRet %d, exiting now.\n", (unsigned long) pThis->thrdID, iRet);
	ENDfunc
	pthread_exit(0);
}

/* Start a new thread and add it to the list of currently
 * executing threads. It is added at the end of the list.
 * rgerhards, 2007-12-14
 */
rsRetVal thrdCreate(rsRetVal (*thrdMain)(thrdInfo_t*), rsRetVal(*afterRun)(thrdInfo_t *))
{
	DEFiRet;
	thrdInfo_t *pThis;
	int i;

	assert(thrdMain != NULL);

	CHKiRet(thrdConstruct(&pThis));
	pThis->bIsActive = 1;
	pThis->pUsrThrdMain = thrdMain;
	pThis->pAfterRun = afterRun;
	i = pthread_create(&pThis->thrdID, NULL, thrdStarter, pThis);
	CHKiRet(llAppend(&llThrds, NULL, pThis));

finalize_it:
	RETiRet;
}


/* initialize the thread-support subsystem
 * must be called once at the start of the program
 */
rsRetVal thrdInit(void)
{
	DEFiRet;
	iRet = llInit(&llThrds, thrdDestruct, NULL, NULL);
	RETiRet;
}


/* de-initialize the thread subsystem
 * must be called once at the end of the program
 */
rsRetVal thrdExit(void)
{
	DEFiRet;

	iRet = llDestroy(&llThrds);

	RETiRet;
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
	select(1, NULL, NULL, NULL, &tvSelectTimeout);
	if(pThis->bShallStop)
		iRet = RS_RET_TERMINATE_NOW;
	RETiRet;
}


/*
 * vi:set ai:
 */
