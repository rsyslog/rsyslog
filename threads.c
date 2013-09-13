/* threads.c
 *
 * This file implements threading support helpers (and maybe the thread object)
 * for rsyslog.
 * 
 * File begun on 2007-12-14 by RGerhards
 *
 * Copyright 2007-2012 Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *       http://www.apache.org/licenses/LICENSE-2.0
 *       -or-
 *       see COPYING.ASL20 in the source distribution
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <assert.h>
#if HAVE_SYS_PRCTL_H
#  include <sys/prctl.h>
#endif

#include "rsyslog.h"
#include "dirty.h"
#include "linkedlist.h"
#include "threads.h"
#include "srUtils.h"
#include "unicode-helper.h"

/* linked list of currently-known threads */
static linkedList_t llThrds;

/* methods */

/* Construct a new thread object
 */
static rsRetVal
thrdConstruct(thrdInfo_t **ppThis)
{
	DEFiRet;
	thrdInfo_t *pThis;

	assert(ppThis != NULL);

	CHKmalloc(pThis = calloc(1, sizeof(thrdInfo_t)));
	pthread_mutex_init(&pThis->mutThrd, NULL);
	pthread_cond_init(&pThis->condThrdTerm, NULL);
	*ppThis = pThis;

finalize_it:
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
	pthread_mutex_destroy(&pThis->mutThrd);
	pthread_cond_destroy(&pThis->condThrdTerm);
	free(pThis->name);
	free(pThis);

	RETiRet;
}


/* terminate a thread via the non-cancel interface
 * This is a separate function as it involves a bit more of code.
 * rgerhads, 2009-10-15
 */
static inline rsRetVal
thrdTerminateNonCancel(thrdInfo_t *pThis)
{
	struct timespec tTimeout;
	int ret;
	DEFiRet;
	assert(pThis != NULL);

	DBGPRINTF("request term via SIGTTIN for input thread '%s' 0x%x\n",
		  pThis->name, (unsigned) pThis->thrdID);
	pThis->bShallStop = RSTRUE;
	do {
		d_pthread_mutex_lock(&pThis->mutThrd);
		pthread_kill(pThis->thrdID, SIGTTIN);
		timeoutComp(&tTimeout, 1000); /* a fixed 1sec timeout */
		ret = d_pthread_cond_timedwait(&pThis->condThrdTerm, &pThis->mutThrd, &tTimeout);
		d_pthread_mutex_unlock(&pThis->mutThrd);
		if(Debug) {
			if(ret == ETIMEDOUT) {
				dbgprintf("input thread term: timeout expired waiting on thread %s termination - canceling\n", pThis->name);
				pthread_cancel(pThis->thrdID);
				pThis->bIsActive = 0;
			} else if(ret == 0) {
				dbgprintf("input thread term: thread %s returned normally and is terminated\n", pThis->name);
			} else {
				char errStr[1024];
				int err = errno;
				rs_strerror_r(err, errStr, sizeof(errStr));
				dbgprintf("input thread term: cond_wait returned with error %d: %s\n",
					  err, errStr);
			}
		}
	} while(pThis->bIsActive);
	DBGPRINTF("non-cancel input thread termination succeeded for thread %s 0x%x\n",
		  pThis->name, (unsigned) pThis->thrdID);

	RETiRet;
}


/* terminate a thread gracefully.
 */
rsRetVal thrdTerminate(thrdInfo_t *pThis)
{
	DEFiRet;
	assert(pThis != NULL);
	
	if(pThis->bNeedsCancel) {
		DBGPRINTF("request term via canceling for input thread 0x%x\n", (unsigned) pThis->thrdID);
		pthread_cancel(pThis->thrdID);
		pThis->bIsActive = 0;
	} else {
		thrdTerminateNonCancel(pThis);
	}
	pthread_join(pThis->thrdID, NULL); /* wait for input thread to complete */

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
#	if HAVE_PRCTL && defined PR_SET_NAME
	uchar thrdName[32] = "in:";
#	endif

	assert(pThis != NULL);
	assert(pThis->pUsrThrdMain != NULL);

#	if HAVE_PRCTL && defined PR_SET_NAME
	ustrncpy(thrdName+3, pThis->name, 20);
	dbgOutputTID((char*)thrdName);

	/* set thread name - we ignore if the call fails, has no harsh consequences... */
	if(prctl(PR_SET_NAME, thrdName, 0, 0, 0) != 0) {
		DBGPRINTF("prctl failed, not setting thread name for '%s'\n", pThis->name);
	} else {
		DBGPRINTF("set thread name to '%s'\n", thrdName);
	}
#	endif

	/* block all signals */
	sigset_t sigSet;
	sigfillset(&sigSet);
	pthread_sigmask(SIG_BLOCK, &sigSet, NULL);

	/* but ignore SIGTTN, which we (ab)use to signal the thread to shutdown -- rgerhards, 2009-07-20 */
	sigemptyset(&sigSet);
	sigaddset(&sigSet, SIGTTIN);
	pthread_sigmask(SIG_UNBLOCK, &sigSet, NULL);

	/* setup complete, we are now ready to execute the user code. We will not
	 * regain control until the user code is finished, in which case we terminate
	 * the thread.
	 */
	iRet = pThis->pUsrThrdMain(pThis);

	dbgprintf("thrdStarter: usrThrdMain %s - 0x%lx returned with iRet %d, exiting now.\n",
		  pThis->name, (unsigned long) pThis->thrdID, iRet);

	/* signal master control that we exit (we do the mutex lock mostly to 
	 * keep the thread debugger happer, it would not really be necessary with
	 * the logic we employ...)
	 */
	d_pthread_mutex_lock(&pThis->mutThrd);
	pThis->bIsActive = 0;
	pthread_cond_signal(&pThis->condThrdTerm);
	d_pthread_mutex_unlock(&pThis->mutThrd);

	ENDfunc
	pthread_exit(0);
}

/* Start a new thread and add it to the list of currently
 * executing threads. It is added at the end of the list.
 * rgerhards, 2007-12-14
 */
rsRetVal thrdCreate(rsRetVal (*thrdMain)(thrdInfo_t*), rsRetVal(*afterRun)(thrdInfo_t *), sbool bNeedsCancel, uchar *name)
{
	DEFiRet;
	thrdInfo_t *pThis;

	assert(thrdMain != NULL);

	CHKiRet(thrdConstruct(&pThis));
	pThis->bIsActive = 1;
	pThis->pUsrThrdMain = thrdMain;
	pThis->pAfterRun = afterRun;
	pThis->bNeedsCancel = bNeedsCancel;
	pThis->name = ustrdup(name);
	pthread_create(&pThis->thrdID,
#ifdef HAVE_PTHREAD_SETSCHEDPARAM
			   &default_thread_attr,
#else
			   NULL,
#endif
			   thrdStarter, pThis);
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


/* vi:set ai:
 */
