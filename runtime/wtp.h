/* Definition of the worker thread pool (wtp) object.
 *
 * Copyright 2008-2012 Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
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

#ifndef WTP_H_INCLUDED
#define WTP_H_INCLUDED

#include <pthread.h>
#include "obj.h"
#include "atomic.h"

/* commands and states for worker threads. */
typedef enum {
	eWRKTHRD_STOPPED = 0,	/* worker thread is not running (either actually never ran or was shut down) */
	eWRKTHRD_TERMINATING = 1,/* worker thread has shut down, but some finalzing is still needed */
	/* ALL active states MUST be numerically higher than eWRKTHRD_TERMINATED and NONE must be lower! */
	eWRKTHRD_RUN_CREATED = 2,/* worker thread has been created, but not yet begun initialization (prob. not yet scheduled) */
	eWRKTHRD_RUN_INIT = 3,	/* worker thread is initializing, but not yet fully running */
	eWRKTHRD_RUNNING = 4,	/* worker thread is up and running and shall continue to do so */
	eWRKTHRD_SHUTDOWN = 5,	/* worker thread is running but shall terminate when wtp is empty */
	eWRKTHRD_SHUTDOWN_IMMEDIATE = 6/* worker thread is running but shall terminate even if wtp is full */
	/* SHUTDOWN_IMMEDIATE MUST alsways be the numerically highest state! */
} qWrkCmd_t;


/* possible states of a worker thread pool */
typedef enum {
	wtpState_RUNNING = 0,		/* runs in regular mode */
	wtpState_SHUTDOWN = 1,		/* worker threads shall shutdown when idle */
	wtpState_SHUTDOWN_IMMEDIATE = 2	/* worker threads shall shutdown ASAP, even if not idle */
} wtpState_t;


/* the worker thread pool (wtp) object */
typedef struct wtp_s {
	BEGINobjInstance;
	wtpState_t wtpState;
	int 	iNumWorkerThreads;/* number of worker threads to use */
	int 	iCurNumWrkThrd;/* current number of active worker threads */
	struct wti_s **pWrkr;/* array with control structure for the worker thread(s) associated with this wtp */
	int	toWrkShutdown;	/* timeout for idle workers in ms, -1 means indefinite (0 is immediate) */
	bool	bInactivityGuard;/* prevents inactivity due to race condition */
	rsRetVal (*pConsumer)(void *); /* user-supplied consumer function for dewtpd messages */
	/* synchronization variables */
	pthread_mutex_t mutThrdShutdwn; /* mutex to guard thread shutdown processing */
	pthread_mutex_t mut; /* mutex for the wtp's thread management */
	pthread_cond_t condThrdTrm;/* signalled when threads terminate */
	int bThrdStateChanged;	/* at least one thread state has changed if 1 */
	/* end sync variables */
	/* user objects */
	void *pUsr;		/* pointer to user object */
	pthread_mutex_t *pmutUsr;
	pthread_cond_t *pcondBusy; /* condition the user will signal "busy again, keep runing" on (awakes worker) */
	rsRetVal (*pfChkStopWrkr)(void *pUsr, int);
	rsRetVal (*pfRateLimiter)(void *pUsr);
	rsRetVal (*pfIsIdle)(void *pUsr, int);
	rsRetVal (*pfDoWork)(void *pUsr, void *pWti, int);
	rsRetVal (*pfOnIdle)(void *pUsr, int);
	rsRetVal (*pfOnWorkerCancel)(void *pUsr, void*pWti);
	rsRetVal (*pfOnWorkerStartup)(void *pUsr);
	rsRetVal (*pfOnWorkerShutdown)(void *pUsr);
	/* end user objects */
	uchar *pszDbgHdr;	/* header string for debug messages */
	DEF_ATOMIC_HELPER_MUT(mutThrdStateChanged);
} wtp_t;

/* some symbolic constants for easier reference */


/* prototypes */
rsRetVal wtpConstruct(wtp_t **ppThis);
rsRetVal wtpConstructFinalize(wtp_t *pThis);
rsRetVal wtpDestruct(wtp_t **ppThis);
rsRetVal wtpAdviseMaxWorkers(wtp_t *pThis, int nMaxWrkr);
rsRetVal wtpProcessThrdChanges(wtp_t *pThis);
rsRetVal wtpSetInactivityGuard(wtp_t *pThis, int bNewState, int bLockMutex);
rsRetVal wtpChkStopWrkr(wtp_t *pThis, int bLockMutex, int bLockUsrMutex);
rsRetVal wtpSetState(wtp_t *pThis, wtpState_t iNewState);
rsRetVal wtpWakeupWrkr(wtp_t *pThis);
rsRetVal wtpWakeupAllWrkr(wtp_t *pThis);
rsRetVal wtpCancelAll(wtp_t *pThis);
rsRetVal wtpSetDbgHdr(wtp_t *pThis, uchar *pszMsg, size_t lenMsg);
rsRetVal wtpSignalWrkrTermination(wtp_t *pWtp);
rsRetVal wtpShutdownAll(wtp_t *pThis, wtpState_t tShutdownCmd, struct timespec *ptTimeout);
int wtpGetCurNumWrkr(wtp_t *pThis, int bLockMutex);
void wtpSetThrdStateChanged(wtp_t *pThis, int val);
PROTOTYPEObjClassInit(wtp);
PROTOTYPEpropSetMethFP(wtp, pfChkStopWrkr, rsRetVal(*pVal)(void*, int));
PROTOTYPEpropSetMethFP(wtp, pfRateLimiter, rsRetVal(*pVal)(void*));
PROTOTYPEpropSetMethFP(wtp, pfIsIdle, rsRetVal(*pVal)(void*, int));
PROTOTYPEpropSetMethFP(wtp, pfDoWork, rsRetVal(*pVal)(void*, void*, int));
PROTOTYPEpropSetMethFP(wtp, pfOnIdle, rsRetVal(*pVal)(void*, int));
PROTOTYPEpropSetMethFP(wtp, pfOnWorkerCancel, rsRetVal(*pVal)(void*,void*));
PROTOTYPEpropSetMethFP(wtp, pfOnWorkerStartup, rsRetVal(*pVal)(void*));
PROTOTYPEpropSetMethFP(wtp, pfOnWorkerShutdown, rsRetVal(*pVal)(void*));
PROTOTYPEpropSetMeth(wtp, toWrkShutdown, long);
PROTOTYPEpropSetMeth(wtp, wtpState, wtpState_t);
PROTOTYPEpropSetMeth(wtp, iMaxWorkerThreads, int);
PROTOTYPEpropSetMeth(wtp, pUsr, void*);
PROTOTYPEpropSetMeth(wtp, iNumWorkerThreads, int);
PROTOTYPEpropSetMethPTR(wtp, pmutUsr, pthread_mutex_t);
PROTOTYPEpropSetMethPTR(wtp, pcondBusy, pthread_cond_t);

#endif /* #ifndef WTP_H_INCLUDED */
