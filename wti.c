/* wti.c
 *
 * This file implements the worker thread instance (wti) class.
 * 
 * File begun on 2008-01-20 by RGerhards based on functions from the 
 * previous queue object class (the wti functions have been extracted)
 *
 * There is some in-depth documentation available in doc/dev_queue.html
 * (and in the web doc set on http://www.rsyslog.com/doc). Be sure to read it
 * if you are getting aquainted to the object.
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
#include <errno.h>

#include "rsyslog.h"
#include "syslogd.h"
#include "stringbuf.h"
#include "srUtils.h"
#include "wtp.h"
#include "wti.h"
#include "obj.h"

/* static data */
DEFobjStaticHelpers

/* forward-definitions */
static void *wtiWorker(void *arg);

/* methods */

/* get the header for debug messages
 * The caller must NOT free or otherwise modify the returned string!
 */
static inline uchar *
wtiGetDbgHdr(wti_t *pThis)
{
	ISOBJ_TYPE_assert(pThis, wti);

	if(pThis->pszDbgHdr == NULL)
		return (uchar*) "wti"; /* should not normally happen */
	else
		return pThis->pszDbgHdr;
}


/* get the current worker state. For simplicity and speed, we have
 * NOT used our regular calling interface this time. I hope that won't
 * bite in the long term... -- rgerhards, 2008-01-17
 */
static inline qWrkCmd_t
wtiGetState(wti_t *pThis)
{
	ISOBJ_TYPE_assert(pThis, wti);
	// TODO: lock mutex?
	return pThis->tCurrCmd;
}



/* send a command to a specific thread
 * bActiveOnly specifies if the command should be sent only when the worker is
 * in an active state. -- rgerhards, 2008-01-20
 */
rsRetVal
wtiSetState(wti_t *pThis, qWrkCmd_t tCmd, bActiveOnly)
{
	DEFiRet;
	DEFVARS_mutex_cancelsafeLock;
	int iState;

	ISOBJ_TYPE_assert(pThis, wti);
	assert(tCmd <= eWRKTHRD_SHUTDOWN_IMMEDIATE);

	mutex_cancelsafe_lock(&pThis->mut);

	/* all worker states must be followed sequentially, only termination can be set in any state */
	if(   (bActiveOnly && (pThis->tCurrCmd < eWRKTHRD_RUN_CREATED))
	   || (pThis->tCurrCmd > tCmd && tCmd != eWRKTHRD_TERMINATING)) {
		dbgprintf("%s: command %d can not be accepted in current %d processing state - ignored\n",
			  wtiGetDbgHdr(pThis), tCmd, pThis->tCurrCmd);
	} else {
		dbgprintf("%s: receiving command %d\n", wtiGetDbgHdr(pThis), tCmd);
		switch(tCmd) {
			case eWRKTHRD_RUN_CREATED:
				assert(pThis->tCurrCmd < eWRKTHRD_RUN_CREATED);
				iState = pthread_create(&(pThis->thrdID), NULL, wtiWorker, (void*) pThis);
				dbgprintf("wti: Worker thread %s, started with state %d.\n", wtiGetDbgHdr(pThis), iState);
				break;
			case eWRKTHRD_TERMINATING:
				/* TODO: re-enable meaningful debug msg! (via function callback?)
				dbgprintf("%s: thread terminating with %d entries left in queue, %d workers running.\n",
					  wtiGetDbgHdr(pThis->pQueue), pThis->pQueue->iQueueSize,
					  pThis->pQueue->iCurNumWrkThrd);
				*/
				dbgprintf("%s: worker terminating\n", wtiGetDbgHdr(pThis));
				break;
			case eWRKTHRD_RUNNING:
				pthread_cond_signal(&pThis->condInitDone);
				break;
			/* these cases just to satisfy the compiler, we do (yet) not act an them: */
			case eWRKTHRD_STOPPED:
			case eWRKTHRD_RUN_INIT:
			case eWRKTHRD_SHUTDOWN:
			case eWRKTHRD_SHUTDOWN_IMMEDIATE:
				/* DO NOTHING */
				break;
		}
		pThis->tCurrCmd = tCmd; /* apply the new state */
	}

	mutex_cancelsafe_unlock(&pThis->mut);
	return iRet;
}


/* Destructor */
rsRetVal wtiDestruct(wti_t **ppThis)
{
	DEFiRet;
	wti_t *pThis;
	int iCancelStateSave;

	assert(ppThis != NULL);
	pThis = *ppThis;
	ISOBJ_TYPE_assert(pThis, wti);

	/* we can not be canceled, that would have a myriad of side-effects */
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave);

	/* actual destruction */
	pthread_cond_destroy(&pThis->condInitDone);
	pthread_mutex_destroy(&pThis->mut);

	if(pThis->pszDbgHdr != NULL)
		free(pThis->pszDbgHdr);

	/* and finally delete the queue objet itself */
	free(pThis);
	*ppThis = NULL;

	/* back to normal */
	pthread_setcancelstate(iCancelStateSave, NULL);

	return iRet;
}


/* Standard-Constructor for the wti object
 */
BEGINobjConstruct(wti) /* be sure to specify the object type also in END macro! */
	pthread_cond_init(&pThis->condInitDone, NULL);
	pthread_mutex_init(&pThis->mut, NULL);
ENDobjConstruct(wti)


/* Construction finalizer
 * rgerhards, 2008-01-17
 */
rsRetVal
wtiConstructFinalize(wti_t *pThis)
{
	ISOBJ_TYPE_assert(pThis, wti);

	dbgprintf("%s: finalizing construction of worker instance data\n", wtiGetDbgHdr(pThis));

	/* initialize our thread instance descriptor */
	pThis->pUsr = NULL;
	pThis->tCurrCmd = eWRKTHRD_STOPPED;

	return RS_RET_OK;
}


/* Waits until the specified worker thread 
 * changed to full running state (aka has started up).
 * rgerhards, 2008-01-17
 */
static inline rsRetVal
wtiWaitStartup(wti_t *pThis)
{
	DEFVARS_mutex_cancelsafeLock;
	ISOBJ_TYPE_assert(pThis, wti);

	mutex_cancelsafe_lock(&pThis->mut);
	if((pThis->tCurrCmd == eWRKTHRD_RUN_CREATED) || (pThis->tCurrCmd == eWRKTHRD_RUN_CREATED)) {
		dbgprintf("wti: waiting on worker thread %s startup\n", wtiGetDbgHdr(pThis));
		pthread_cond_wait(&pThis->condInitDone, &pThis->mut);
dbgprintf("worker startup done!\n");
	}
	mutex_cancelsafe_unlock(&pThis->mut);

	return RS_RET_OK;
}


/* join a specific worker thread
 * we do not lock the mutex, because join will sync anyways...
 */
rsRetVal
wtiJoinThrd(wti_t *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, wti);
	dbgprintf("wti: waiting for worker %s termination, current state %d\n", wtiGetDbgHdr(pThis), pThis->tCurrCmd);
	pthread_join(pThis->thrdID, NULL);
	wtiSetState(pThis, eWRKTHRD_STOPPED); /* back to virgin... */
	pThis->thrdID = 0; /* invalidate the thread ID so that we do not accidently find reused ones */
	dbgprintf("wti: worker %s has stopped\n", wtiGetDbgHdr(pThis));

	return iRet;
}


static void *
wtiWorker(void *arg)
{
	wti_t *pThis = (wti_t*) arg;

	ISOBJ_TYPE_assert(pThis, wti);

	// TODO: add logic!
	//
	pthread_exit(0);
}

/* Starts a worker thread (on a specific index [i]!)
 */
rsRetVal
wtiStart(wti_t *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, wti);
	wtiSetState(pThis, eWRKTHRD_RUN_CREATED);

	return iRet;
}


/* some simple object access methods */
DEFpropSetMeth(wti, toShutdown, int);

/* set the debug header message
 * The passed-in string is duplicated. So if the caller does not need
 * it any longer, it must free it. Must be called only before object is finalized.
 * rgerhards, 2008-01-09
 */
rsRetVal
wtiSetDbgHdr(wti_t *pThis, uchar *pszMsg, size_t lenMsg)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, wti);
	assert(pszMsg != NULL);
	
	if(lenMsg < 1)
		ABORT_FINALIZE(RS_RET_PARAM_ERROR);

	if(pThis->pszDbgHdr != NULL) {
		free(pThis->pszDbgHdr);
		pThis->pszDbgHdr = NULL;
	}

	if((pThis->pszDbgHdr = malloc(sizeof(uchar) * lenMsg + 1)) == NULL)
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

	memcpy(pThis->pszDbgHdr, pszMsg, lenMsg + 1); /* always think about the \0! */

finalize_it:
	return iRet;
}


/* Initialize the wti class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-01-09
 */
BEGINObjClassInit(wti, 1) /* one is the object version (most important for persisting) */
ENDObjClassInit(queue)

/*
 * vi:set ai:
 */
