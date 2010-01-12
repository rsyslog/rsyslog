/* action.c
 *
 * Implementation of the action object.
 *
 * File begun on 2007-08-06 by RGerhards (extracted from syslogd.c)
 *
 * Copyright 2007-2009 Rainer Gerhards and Adiscon GmbH.
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
#include "rsyslog.h"
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <errno.h>

#include "dirty.h"
#include "template.h"
#include "action.h"
#include "modules.h"
#include "sync.h"
#include "cfsysline.h"
#include "srUtils.h"
#include "errmsg.h"
#include "batch.h"
#include "wti.h"
#include "datetime.h"
#include "unicode-helper.h"

#define NO_TIME_PROVIDED 0 /* indicate we do not provide any cached time */

/* forward definitions */
static rsRetVal processBatchMain(action_t *pAction, batch_t *pBatch, int*);

/* object static data (once for all instances) */
/* TODO: make this an object! DEFobjStaticHelpers -- rgerhards, 2008-03-05 */
DEFobjCurrIf(obj)
DEFobjCurrIf(datetime)
DEFobjCurrIf(module)
DEFobjCurrIf(errmsg)

static int iActExecOnceInterval = 0; /* execute action once every nn seconds */
static int iActExecEveryNthOccur = 0; /* execute action every n-th occurence (0,1=always) */
static time_t iActExecEveryNthOccurTO = 0; /* timeout for n-occurence setting (in seconds, 0=never) */
static int glbliActionResumeInterval = 30;
int glbliActionResumeRetryCount = 0;		/* how often should suspended actions be retried? */
static int bActionRepMsgHasMsg = 0;		/* last messsage repeated... has msg fragment in it */

static int bActionWriteAllMarkMsgs = FALSE;			/* should all mark messages be unconditionally written? */
static uchar *pszActionName;					/* short name for the action */
/* action queue and its configuration parameters */
static queueType_t ActionQueType = QUEUETYPE_DIRECT;		/* type of the main message queue above */
static int iActionQueueSize = 1000;				/* size of the main message queue above */
static int iActionQueueDeqBatchSize = 16;			/* batch size for action queues */
static int iActionQHighWtrMark = 800;				/* high water mark for disk-assisted queues */
static int iActionQLowWtrMark = 200;				/* low water mark for disk-assisted queues */
static int iActionQDiscardMark = 9800;				/* begin to discard messages */
static int iActionQDiscardSeverity = 8;				/* by default, discard nothing to prevent unintentional loss */
static int iActionQueueNumWorkers = 1;				/* number of worker threads for the mm queue above */
static uchar *pszActionQFName = NULL;				/* prefix for the main message queue file */
static int64 iActionQueMaxFileSize = 1024*1024;
static int iActionQPersistUpdCnt = 0;				/* persist queue info every n updates */
static int bActionQSyncQeueFiles = 0;				/* sync queue files */
static int iActionQtoQShutdown = 0;				/* queue shutdown */ 
static int iActionQtoActShutdown = 1000;			/* action shutdown (in phase 2) */ 
static int iActionQtoEnq = 2000;				/* timeout for queue enque */ 
static int iActionQtoWrkShutdown = 60000;			/* timeout for worker thread shutdown */
static int iActionQWrkMinMsgs = 100;				/* minimum messages per worker needed to start a new one */
static int bActionQSaveOnShutdown = 1;				/* save queue on shutdown (when DA enabled)? */
static int64 iActionQueMaxDiskSpace = 0;			/* max disk space allocated 0 ==> unlimited */
static int iActionQueueDeqSlowdown = 0;				/* dequeue slowdown (simple rate limiting) */
static int iActionQueueDeqtWinFromHr = 0;			/* hour begin of time frame when queue is to be dequeued */
static int iActionQueueDeqtWinToHr = 25;			/* hour begin of time frame when queue is to be dequeued */

/* the counter below counts actions created. It is used to obtain unique IDs for the action. They
 * should not be relied on for any long-term activity (e.g. disk queue names!), but they are nice
 * to have during one instance of an rsyslogd run. For example, I use them to name actions when there
 * is no better name available. Note that I do NOT recover previous numbers on HUP - we simply keep
 * counting. -- rgerhards, 2008-01-29
 */
static int iActionNbr = 0;

/* ------------------------------ methods ------------------------------ */ 

/* This function returns the "current" time for this action. Current time
 * is not necessarily real-time. In order to enhance performance, current
 * system time is obtained the first time an action needs to know the time
 * and then kept cached inside the action structure. Later requests will
 * always return that very same time. Wile not totally accurate, it is far
 * accurate in most cases and considered "acurate enough" for all cases.
 * When changing the threading model, please keep in mind that this
 * logic needs to be changed should we once allow more than one parallel
 * call into the same action (object). As this is currently not supported,
 * we simply cache the time inside the action object itself, after it
 * is under mutex protection.
 * Side-note: the value -1 is used as tActNow, because it also is the
 * error return value of time(). So we would do a retry with the next
 * invocation if time() failed. Then, of course, we would probably already
 * be in trouble, but for the sake of performance we accept this very,
 * very slight risk.
 * This logic has been added as part of an overall performance improvment
 * effort inspired by David Lang. -- rgerhards, 2008-09-16
 * Note: this function does not use the usual iRet call conventions
 * because that would provide little to no benefit but complicate things
 * a lot. So we simply return the system time.
 */
static inline time_t
getActNow(action_t *pThis)
{
	assert(pThis != NULL);
	if(pThis->tActNow == -1) {
		pThis->tActNow = datetime.GetTime(NULL); /* good time call - the only one done */
		if(pThis->tLastExec > pThis->tActNow) {
			/* if we are traveling back in time, reset tLastExec */
			pThis->tLastExec = (time_t) 0;
		}
	}

	return pThis->tActNow;
}


/* resets action queue parameters to their default values. This happens
 * after each action has been created in order to prevent any wild defaults
 * to be used. It is somewhat against the original spirit of the config file
 * reader, but I think it is a good thing to do.
 * rgerhards, 2008-01-29
 */
static rsRetVal
actionResetQueueParams(void)
{
	DEFiRet;

	ActionQueType = QUEUETYPE_DIRECT;		/* type of the main message queue above */
	iActionQueueSize = 1000;			/* size of the main message queue above */
	iActionQueueDeqBatchSize = 16;			/* default batch size */
	iActionQHighWtrMark = 800;			/* high water mark for disk-assisted queues */
	iActionQLowWtrMark = 200;			/* low water mark for disk-assisted queues */
	iActionQDiscardMark = 9800;			/* begin to discard messages */
	iActionQDiscardSeverity = 8;			/* discard warning and above */
	iActionQueueNumWorkers = 1;			/* number of worker threads for the mm queue above */
	iActionQueMaxFileSize = 1024*1024;
	iActionQPersistUpdCnt = 0;			/* persist queue info every n updates */
	bActionQSyncQeueFiles = 0;
	iActionQtoQShutdown = 0;			/* queue shutdown */ 
	iActionQtoActShutdown = 1000;			/* action shutdown (in phase 2) */ 
	iActionQtoEnq = 2000;				/* timeout for queue enque */ 
	iActionQtoWrkShutdown = 60000;			/* timeout for worker thread shutdown */
	iActionQWrkMinMsgs = 100;			/* minimum messages per worker needed to start a new one */
	bActionQSaveOnShutdown = 1;			/* save queue on shutdown (when DA enabled)? */
	iActionQueMaxDiskSpace = 0;
	iActionQueueDeqSlowdown = 0;
	iActionQueueDeqtWinFromHr = 0;
	iActionQueueDeqtWinToHr = 25;			/* 25 disables time windowed dequeuing */

	glbliActionResumeRetryCount = 0;		/* I guess it is smart to reset this one, too */

	d_free(pszActionQFName);
	pszActionQFName = NULL;				/* prefix for the main message queue file */

	RETiRet;
}


/* destructs an action descriptor object
 * rgerhards, 2007-08-01
 */
rsRetVal actionDestruct(action_t *pThis)
{
	int i;
	DEFiRet;
	ASSERT(pThis != NULL);

	if(pThis->pQueue != NULL) {
		qqueueDestruct(&pThis->pQueue);
	}

	if(pThis->pMod != NULL)
		pThis->pMod->freeInstance(pThis->pModData);

	if(pThis->f_pMsg != NULL)
		msgDestruct(&pThis->f_pMsg);

	SYNC_OBJ_TOOL_EXIT(pThis);
	pthread_mutex_destroy(&pThis->mutActExec);
	d_free(pThis->pszName);
	d_free(pThis->ppTpl);

	/* message ptr cleanup */
	for(i = 0 ; i < pThis->iNumTpls ; ++i) {
		if(pThis->ppMsgs[i] != NULL) {
			switch(pThis->eParamPassing) {
			case ACT_ARRAY_PASSING:
#if 0 /* later, as an optimization. So far, we do the cleanup after each message */ 
				iArr = 0;
				while(((char **)pThis->ppMsgs[i])[iArr] != NULL) {
					d_free(((char **)pThis->ppMsgs[i])[iArr++]);
					((char **)pThis->ppMsgs[i])[iArr++] = NULL;
				}
				d_free(pThis->ppMsgs[i]);
				pThis->ppMsgs[i] = NULL;
#endif
				break;
			case ACT_STRING_PASSING:
				d_free(pThis->ppMsgs[i]);
				break;
			case ACT_MSG_PASSING:
				/* No cleanup needed in this case */
				break;
			default:
				assert(0);
			}
		}
	}
	d_free(pThis->ppMsgs);
	d_free(pThis->lenMsgs);

	d_free(pThis);
	
	RETiRet;
}


/* create a new action descriptor object
 * rgerhards, 2007-08-01
 */
rsRetVal actionConstruct(action_t **ppThis)
{
	DEFiRet;
	action_t *pThis;

	ASSERT(ppThis != NULL);
	
	CHKmalloc(pThis = (action_t*) calloc(1, sizeof(action_t)));
	pThis->iResumeInterval = glbliActionResumeInterval;
	pThis->iResumeRetryCount = glbliActionResumeRetryCount;
	pThis->tLastOccur = datetime.GetTime(NULL);	/* done once per action on startup only */
	pthread_mutex_init(&pThis->mutActExec, NULL);
	SYNC_OBJ_TOOL_INIT(pThis);

	/* indicate we have a new action */
	++iActionNbr;

finalize_it:
	*ppThis = pThis;
	RETiRet;
}


/* action construction finalizer
 */
rsRetVal
actionConstructFinalize(action_t *pThis)
{
	DEFiRet;
	uchar pszQName[64]; /* friendly name of our queue */

	ASSERT(pThis != NULL);

	/* find a name for our queue */
	snprintf((char*) pszQName, sizeof(pszQName)/sizeof(uchar), "action %d queue", iActionNbr);

	/* we need to make a safety check: if the queue is NOT in direct mode, a single 
	 * message object may be accessed by multiple threads. As such, we need to enable
	 * msg object thread safety in this case (this costs a bit performance and thus
	 * is not enabled by default. -- rgerhards, 2008-02-20
	 */
	if(ActionQueType != QUEUETYPE_DIRECT)
		MsgEnableThreadSafety();

	/* create queue */
	/* action queues always (for now) have just one worker. This may change when
	 * we begin to implement an interface the enable output modules to request
	 * to be run on multiple threads. So far, this is forbidden by the interface
	 * spec. -- rgerhards, 2008-01-30
	 */
	CHKiRet(qqueueConstruct(&pThis->pQueue, ActionQueType, 1, iActionQueueSize,
					(rsRetVal (*)(void*, batch_t*, int*))processBatchMain));
	obj.SetName((obj_t*) pThis->pQueue, pszQName);

	/* ... set some properties ... */
#	define setQPROP(func, directive, data) \
	CHKiRet_Hdlr(func(pThis->pQueue, data)) { \
		errmsg.LogError(0, NO_ERRCODE, "Invalid " #directive ", error %d. Ignored, running with default setting", iRet); \
	}
#	define setQPROPstr(func, directive, data) \
	CHKiRet_Hdlr(func(pThis->pQueue, data, (data == NULL)? 0 : strlen((char*) data))) { \
		errmsg.LogError(0, NO_ERRCODE, "Invalid " #directive ", error %d. Ignored, running with default setting", iRet); \
	}

	qqueueSetpUsr(pThis->pQueue, pThis);
	setQPROP(qqueueSetsizeOnDiskMax, "$ActionQueueMaxDiskSpace", iActionQueMaxDiskSpace);
	setQPROP(qqueueSetiDeqBatchSize, "$ActionQueueDequeueBatchSize", iActionQueueDeqBatchSize);
	setQPROP(qqueueSetMaxFileSize, "$ActionQueueFileSize", iActionQueMaxFileSize);
	setQPROPstr(qqueueSetFilePrefix, "$ActionQueueFileName", pszActionQFName);
	setQPROP(qqueueSetiPersistUpdCnt, "$ActionQueueCheckpointInterval", iActionQPersistUpdCnt);
	setQPROP(qqueueSetbSyncQueueFiles, "$ActionQueueSyncQueueFiles", bActionQSyncQeueFiles);
	setQPROP(qqueueSettoQShutdown, "$ActionQueueTimeoutShutdown", iActionQtoQShutdown );
	setQPROP(qqueueSettoActShutdown, "$ActionQueueTimeoutActionCompletion", iActionQtoActShutdown);
	setQPROP(qqueueSettoWrkShutdown, "$ActionQueueWorkerTimeoutThreadShutdown", iActionQtoWrkShutdown);
	setQPROP(qqueueSettoEnq, "$ActionQueueTimeoutEnqueue", iActionQtoEnq);
	setQPROP(qqueueSetiHighWtrMrk, "$ActionQueueHighWaterMark", iActionQHighWtrMark);
	setQPROP(qqueueSetiLowWtrMrk, "$ActionQueueLowWaterMark", iActionQLowWtrMark);
	setQPROP(qqueueSetiDiscardMrk, "$ActionQueueDiscardMark", iActionQDiscardMark);
	setQPROP(qqueueSetiDiscardSeverity, "$ActionQueueDiscardSeverity", iActionQDiscardSeverity);
	setQPROP(qqueueSetiMinMsgsPerWrkr, "$ActionQueueWorkerThreadMinimumMessages", iActionQWrkMinMsgs);
	setQPROP(qqueueSetbSaveOnShutdown, "$ActionQueueSaveOnShutdown", bActionQSaveOnShutdown);
	setQPROP(qqueueSetiDeqSlowdown,    "$ActionQueueDequeueSlowdown", iActionQueueDeqSlowdown);
	setQPROP(qqueueSetiDeqtWinFromHr,  "$ActionQueueDequeueTimeBegin", iActionQueueDeqtWinFromHr);
	setQPROP(qqueueSetiDeqtWinToHr,    "$ActionQueueDequeueTimeEnd", iActionQueueDeqtWinToHr);

#	undef setQPROP
#	undef setQPROPstr

	dbgoprint((obj_t*) pThis->pQueue, "save on shutdown %d, max disk space allowed %lld\n",
		   bActionQSaveOnShutdown, iActionQueMaxDiskSpace);
 	

	CHKiRet(qqueueStart(pThis->pQueue));
	DBGPRINTF("Action %p: queue %p created\n", pThis, pThis->pQueue);
	
	/* and now reset the queue params (see comment in its function header!) */
	actionResetQueueParams();

finalize_it:
	RETiRet;
}



/* set the global resume interval
 */
rsRetVal actionSetGlobalResumeInterval(int iNewVal)
{
	glbliActionResumeInterval = iNewVal;
	return RS_RET_OK;
}


/* returns the action state name in human-readable form
 * returned string must not be modified.
 * rgerhards, 2009-05-07
 */
static uchar *getActStateName(action_t *pThis)
{
	switch(pThis->eState) {
		case ACT_STATE_RDY:
			return (uchar*) "rdy";
		case ACT_STATE_ITX:
			return (uchar*) "itx";
		case ACT_STATE_RTRY:
			return (uchar*) "rtry";
		case ACT_STATE_SUSP:
			return (uchar*) "susp";
		case ACT_STATE_DIED:
			return (uchar*) "died";
		case ACT_STATE_COMM:
			return (uchar*) "comm";
		default:
			return (uchar*) "ERROR/UNKNWON";
	}
}


/* returns a suitable return code based on action state
 * rgerhards, 2009-05-07
 */
static rsRetVal getReturnCode(action_t *pThis)
{
	DEFiRet;

	ASSERT(pThis != NULL);
	switch(pThis->eState) {
		case ACT_STATE_RDY:
			iRet = RS_RET_OK;
			break;
		case ACT_STATE_ITX:
			if(pThis->bHadAutoCommit) {
				pThis->bHadAutoCommit = 0; /* auto-reset */
				iRet = RS_RET_PREVIOUS_COMMITTED;
			} else {
				iRet = RS_RET_DEFER_COMMIT;
			}
			break;
		case ACT_STATE_RTRY:
			iRet = RS_RET_SUSPENDED;
			break;
		case ACT_STATE_SUSP:
		case ACT_STATE_DIED:
			iRet = RS_RET_ACTION_FAILED;
			break;
		default:
			DBGPRINTF("Invalid action engine state %d, program error\n",
					(int) pThis->eState);
			iRet = RS_RET_ERR;
			break;
	}

	RETiRet;
}


/* set the action to a new state
 * rgerhards, 2007-08-02
 */
static inline void actionSetState(action_t *pThis, action_state_t newState)
{
	pThis->eState = newState;
	DBGPRINTF("Action %p transitioned to state: %s\n", pThis, getActStateName(pThis));
}

/* Handles the transient commit state. So far, this is
 * mostly a dummy...
 * rgerhards, 2007-08-02
 */
static void actionCommitted(action_t *pThis)
{
	actionSetState(pThis, ACT_STATE_RDY);
}


/* set action to "rtry" state.
 * rgerhards, 2007-08-02
 */
static void actionRetry(action_t *pThis)
{
	actionSetState(pThis, ACT_STATE_RTRY);
}


/* Disable action, this means it will never again be usable
 * until rsyslog is reloaded. Use only as a last resort, but
 * depends on output module.
 * rgerhards, 2007-08-02
 */
static void actionDisable(action_t *pThis)
{
	actionSetState(pThis, ACT_STATE_DIED);
}


/* Suspend action, this involves changing the acton state as well
 * as setting the next retry time.
 * if we have more than 10 retries, we prolong the
 * retry interval. If something is really stalled, it will
 * get re-tried only very, very seldom - but that saves
 * CPU time. TODO: maybe a config option for that?
 * rgerhards, 2007-08-02
 */
static inline void actionSuspend(action_t *pThis, time_t ttNow)
{
	if(ttNow == NO_TIME_PROVIDED)
		datetime.GetTime(&ttNow);
	pThis->ttResumeRtry = ttNow + pThis->iResumeInterval * (pThis->iNbrResRtry / 10 + 1);
	actionSetState(pThis, ACT_STATE_SUSP);
	DBGPRINTF("earliest retry=%d\n", (int) pThis->ttResumeRtry);
}


/* actually do retry processing. Note that the function receives a timestamp so
 * that we do not need to call the (expensive) time() API.
 * Note that we do the full retry processing here, doing the configured number of
 * iterations.
 * rgerhards, 2009-05-07
 */
static rsRetVal actionDoRetry(action_t *pThis, time_t ttNow)
{
	int iRetries;
	int iSleepPeriod;
	DEFiRet;

	ASSERT(pThis != NULL);

	iRetries = 0;
	while(pThis->eState == ACT_STATE_RTRY) {
		iRet = pThis->pMod->tryResume(pThis->pModData);
		if(iRet == RS_RET_OK) {
			actionSetState(pThis, ACT_STATE_RDY);
		} else if(iRet == RS_RET_SUSPENDED) {
			/* max retries reached? */
			if((pThis->iResumeRetryCount != -1 && iRetries >= pThis->iResumeRetryCount)) {
				actionSuspend(pThis, ttNow);
			} else {
				++pThis->iNbrResRtry;
				++iRetries;
				iSleepPeriod = pThis->iResumeInterval;
				ttNow += iSleepPeriod; /* not truly exact, but sufficiently... */
				srSleep(iSleepPeriod, 0);
			}
		} else if(iRet == RS_RET_DISABLE_ACTION) {
			actionDisable(pThis);
		}
	}

	if(pThis->eState == ACT_STATE_RDY) {
		pThis->iNbrResRtry = 0;
	}

	RETiRet;
}


/* try to resume an action -- rgerhards, 2007-08-02
 * changed to new action state engine -- rgerhards, 2009-05-07
 */
static rsRetVal actionTryResume(action_t *pThis)
{
	DEFiRet;
	time_t ttNow = NO_TIME_PROVIDED;

	ASSERT(pThis != NULL);

	if(pThis->eState == ACT_STATE_SUSP) {
		/* if we are suspended, we need to check if the timeout expired.
		 * for this handling, we must always obtain a fresh timestamp. We used
		 * to use the action timestamp, but in this case we will never reach a
		 * point where a resumption is actually tried, because the action timestamp
		 * is always in the past. So we can not avoid doing a fresh time() call
		 * here. -- rgerhards, 2009-03-18
		 */
		datetime.GetTime(&ttNow); /* cache "now" */
		if(ttNow > pThis->ttResumeRtry) {
			actionSetState(pThis, ACT_STATE_RTRY); /* back to retries */
		}
	}

	if(pThis->eState == ACT_STATE_RTRY) {
		if(ttNow == NO_TIME_PROVIDED) /* use cached result if we have it */
			datetime.GetTime(&ttNow);
		CHKiRet(actionDoRetry(pThis, ttNow));
	}

	if(Debug && (pThis->eState == ACT_STATE_RTRY ||pThis->eState == ACT_STATE_SUSP)) {
		DBGPRINTF("actionTryResume: action state: %s, next retry (if applicable): %u [now %u]\n",
			getActStateName(pThis), (unsigned) pThis->ttResumeRtry, (unsigned) ttNow);
	}

finalize_it:
	RETiRet;
}


/* prepare an action for performing work. This involves trying to recover it,
 * depending on its current state.
 * rgerhards, 2009-05-07
 */
static rsRetVal actionPrepare(action_t *pThis)
{
	DEFiRet;

	assert(pThis != NULL);
	CHKiRet(actionTryResume(pThis));

	/* if we are now ready, we initialize the transaction and advance
	 * action state accordingly
	 */
	if(pThis->eState == ACT_STATE_RDY) {
		iRet = pThis->pMod->mod.om.beginTransaction(pThis->pModData);
		switch(iRet) {
			case RS_RET_OK:
				actionSetState(pThis, ACT_STATE_ITX);
				break;
			case RS_RET_SUSPENDED:
				actionRetry(pThis);
				break;
			case RS_RET_DISABLE_ACTION:
				actionDisable(pThis);
				break;
			default:FINALIZE;
		}
	}

finalize_it:
	RETiRet;
}


/* debug-print the contents of an action object
 * rgerhards, 2007-08-02
 */
rsRetVal actionDbgPrint(action_t *pThis)
{
	DEFiRet;

	dbgprintf("%s: ", module.GetStateName(pThis->pMod));
	pThis->pMod->dbgPrintInstInfo(pThis->pModData);
	dbgprintf("\n\tInstance data: 0x%lx\n", (unsigned long) pThis->pModData);
	dbgprintf("\tRepeatedMsgReduction: %d\n", pThis->f_ReduceRepeated);
	dbgprintf("\tResume Interval: %d\n", pThis->iResumeInterval);
	if(pThis->eState == ACT_STATE_SUSP) {
		dbgprintf("\tresume next retry: %u, number retries: %d",
			  (unsigned) pThis->ttResumeRtry, pThis->iNbrResRtry);
	}
	dbgprintf("\tState: %s\n", getActStateName(pThis));
	dbgprintf("\tExec only when previous is suspended: %d\n", pThis->bExecWhenPrevSusp);
	dbgprintf("\n");

	RETiRet;
}


/* prepare the calling parameters for doAction()
 * rgerhards, 2009-05-07
 */
static rsRetVal prepareDoActionParams(action_t *pAction, msg_t *pMsg)
{
	int i;
	DEFiRet;

	ASSERT(pAction != NULL);

	/* here we must loop to process all requested strings */
	for(i = 0 ; i < pAction->iNumTpls ; ++i) {
		switch(pAction->eParamPassing) {
			case ACT_STRING_PASSING:
				CHKiRet(tplToString(pAction->ppTpl[i], pMsg, &(pAction->ppMsgs[i]), &(pAction->lenMsgs[i])));
				break;
			case ACT_ARRAY_PASSING:
				CHKiRet(tplToArray(pAction->ppTpl[i], pMsg, (uchar***) &(pAction->ppMsgs[i])));
				break;
			case ACT_MSG_PASSING:
				/* we abuse the uchar* ptr, it now actually is a void*, but we can not
				 * change that other than by chaning the interface, what we don't like...
				 */
				pAction->ppMsgs[i] = (uchar*) pMsg;
				break;
			default:assert(0); /* software bug if this happens! */
		}
	}

finalize_it:
	RETiRet;
}


/* cleanup doAction calling parameters
 * rgerhards, 2009-05-07
 */
static rsRetVal cleanupDoActionParams(action_t *pAction)
{
	int i;
	int iArr;
	DEFiRet;

	ASSERT(pAction != NULL);
	for(i = 0 ; i < pAction->iNumTpls ; ++i) {
		if(pAction->ppMsgs[i] != NULL) {
			switch(pAction->eParamPassing) {
			case ACT_ARRAY_PASSING:
				iArr = 0;
				while(((char **)pAction->ppMsgs[i])[iArr] != NULL) {
					d_free(((char **)pAction->ppMsgs[i])[iArr++]);
					((char **)pAction->ppMsgs[i])[iArr++] = NULL;
				}
				d_free(pAction->ppMsgs[i]);
				pAction->ppMsgs[i] = NULL;
				break;
			case ACT_MSG_PASSING:
			case ACT_STRING_PASSING:
				break;
			default:
				assert(0);
			}
		}
	}

	RETiRet;
}


/* call the DoAction output plugin entry point
 * Performance note: we build the action parameters here in this function. That
 * means we do it while we hold the action look, potentially reducing concurrency
 * (especially if the action queue is run in DIRECT mode). As an alternative, we
 * may generate all params for the batch as whole before aquiring the action. However,
 * that requires more memory, for large batches potentially a lot of memory. So for the
 * time being, I am doing it here - the performance hit should be very minor and may even
 * not be a hit because we may gain CPU cache locality gains with the "fewer memory"
 * approach (I'd say that is rater likely).
 * rgerhards, 2008-01-28
 */
rsRetVal
actionCallDoAction(action_t *pThis, msg_t *pMsg)
{
	DEFiRet;

	ASSERT(pThis != NULL);
	ISOBJ_TYPE_assert(pMsg, msg);

	DBGPRINTF("entering actionCalldoAction(), state: %s\n", getActStateName(pThis));
	CHKiRet(prepareDoActionParams(pThis, pMsg));

	pThis->bHadAutoCommit = 0;
	iRet = pThis->pMod->mod.om.doAction(pThis->ppMsgs, pMsg->msgFlags, pThis->pModData);
	switch(iRet) {
		case RS_RET_OK:
			actionCommitted(pThis);
			break;
		case RS_RET_DEFER_COMMIT:
			/* we are done, action state remains the same */
			break;
		case RS_RET_PREVIOUS_COMMITTED:
			/* action state remains the same, but we had a commit. */
			pThis->bHadAutoCommit = 1;
			break;
		case RS_RET_SUSPENDED:
			actionRetry(pThis);
			break;
		case RS_RET_DISABLE_ACTION:
			actionDisable(pThis);
			break;
		default:/* permanent failure of this message - no sense in retrying. This is
			 * not yet handled (but easy TODO)
			 */
			FINALIZE;
	}
	iRet = getReturnCode(pThis);

finalize_it:
	cleanupDoActionParams(pThis); /* iRet ignored! */

	RETiRet;
}


/* process a message
 * this readies the action and then calls doAction()
 * rgerhards, 2008-01-28
 */
rsRetVal
actionProcessMessage(action_t *pThis, msg_t *pMsg)
{
	DEFiRet;

	ASSERT(pThis != NULL);
	ISOBJ_TYPE_assert(pMsg, msg);

RUNLOG_STR("inside actionProcessMsg()");
	CHKiRet(actionPrepare(pThis));
	if(pThis->eState == ACT_STATE_ITX)
		CHKiRet(actionCallDoAction(pThis, pMsg));

	iRet = getReturnCode(pThis);
finalize_it:
	RETiRet;
}


/* finish processing a batch. Most importantly, that means we commit if we 
 * need to do so.
 * rgerhards, 2008-01-28
 */
static rsRetVal
finishBatch(action_t *pThis, batch_t *pBatch)
{
	int i;
	DEFiRet;

	ASSERT(pThis != NULL);

	if(pThis->eState == ACT_STATE_RDY)
		FINALIZE; /* nothing to do */

	CHKiRet(actionPrepare(pThis));
	if(pThis->eState == ACT_STATE_ITX) {
		iRet = pThis->pMod->mod.om.endTransaction(pThis->pModData);
		switch(iRet) {
			case RS_RET_OK:
				actionCommitted(pThis);
				/* flag messages as committed */
				for(i = 0 ; i < pBatch->nElem ; ++i) {
					pBatch->pElem[i].state = BATCH_STATE_COMM;
				}
				break;
			case RS_RET_SUSPENDED:
				actionRetry(pThis);
				break;
			case RS_RET_DISABLE_ACTION:
				actionDisable(pThis);
				break;
			case RS_RET_DEFER_COMMIT:
				DBGPRINTF("output plugin error: endTransaction() returns RS_RET_DEFER_COMMIT "
					  "- ignored\n");
				actionCommitted(pThis);
				break;
			case RS_RET_PREVIOUS_COMMITTED:
				DBGPRINTF("output plugin error: endTransaction() returns RS_RET_PREVIOUS_COMMITTED "
					  "- ignored\n");
				actionCommitted(pThis);
				break;
			default:/* permanent failure of this message - no sense in retrying. This is
				 * not yet handled (but easy TODO)
				 */
				FINALIZE;
		}
	}
	iRet = getReturnCode(pThis);

finalize_it:
	RETiRet;
}


/* try to submit a partial batch of elements.
 * rgerhards, 2009-05-12
 */
static rsRetVal
tryDoAction(action_t *pAction, batch_t *pBatch, int *pnElem, int *pbShutdownImmediate)
{
	int i;
	int iElemProcessed;
	int iCommittedUpTo;
	msg_t *pMsg;
	rsRetVal localRet;
	DEFiRet;

	assert(pBatch != NULL);
	assert(pnElem != NULL);

	i = pBatch->iDoneUpTo;	/* all messages below that index are processed */
	iElemProcessed = 0;
	iCommittedUpTo = i;
	while(iElemProcessed <= *pnElem && i < pBatch->nElem) {
		if(*pbShutdownImmediate)
			ABORT_FINALIZE(RS_RET_FORCE_TERM);
		pMsg = (msg_t*) pBatch->pElem[i].pUsrp;
		if(pBatch->pElem[i].state != BATCH_STATE_DISC) {
			localRet = actionProcessMessage(pAction, pMsg);
			DBGPRINTF("action call returned %d\n", localRet);
			if(localRet == RS_RET_OK) {
				/* mark messages as committed */
				while(iCommittedUpTo <= i) {
					pBatch->pElem[iCommittedUpTo++].state = BATCH_STATE_COMM;
				}
			} else if(localRet == RS_RET_PREVIOUS_COMMITTED) {
				/* mark messages as committed */
				while(iCommittedUpTo < i) {
					pBatch->pElem[iCommittedUpTo++].state = BATCH_STATE_COMM;
				}
				pBatch->pElem[i].state = BATCH_STATE_SUB;
			} else if(localRet == RS_RET_DEFER_COMMIT) {
				pBatch->pElem[i].state = BATCH_STATE_SUB;
			} else if(localRet == RS_RET_DISCARDMSG) {
				pBatch->pElem[i].state = BATCH_STATE_DISC;
			} else {
				dbgprintf("tryDoAction: unexpected error code %d, finalizing\n", localRet);
				iRet = localRet;
				FINALIZE;
			}
		}
		++i;
		++iElemProcessed;
	}

finalize_it:
	if(pBatch->nElem == 1 && pBatch->pElem[0].state == BATCH_STATE_DISC) {
		iRet = RS_RET_DISCARDMSG;
	} else if(pBatch->iDoneUpTo != iCommittedUpTo) {
		*pnElem += iCommittedUpTo - pBatch->iDoneUpTo;
		pBatch->iDoneUpTo = iCommittedUpTo;
	}
	RETiRet;
}


/* submit a batch for actual action processing.
 * The first nElem elements are processed. This function calls itself
 * recursively if it needs to handle errors.
 * rgerhards, 2009-05-12
 */
static rsRetVal
submitBatch(action_t *pAction, batch_t *pBatch, int nElem, int *pbShutdownImmediate)
{
	int i;
	int bDone;
	rsRetVal localRet;
	DEFiRet;

	assert(pBatch != NULL);

	bDone = 0;
	do {
		localRet = tryDoAction(pAction, pBatch, &nElem, pbShutdownImmediate);
dbgprintf("submitBatch: state of tryDoAction %d\n", localRet);
		if(localRet == RS_RET_FORCE_TERM)
			FINALIZE;
		if(   localRet == RS_RET_OK
		   || localRet == RS_RET_PREVIOUS_COMMITTED
		   || localRet == RS_RET_DEFER_COMMIT) {
			/* try commit transaction, once done, we can simply do so as if
			 * that return state was returned from tryDoAction().
			 */
			localRet = finishBatch(pAction, pBatch); // TODO: careful, do we need the elem counter?
		}

		if(   localRet == RS_RET_OK
		   || localRet == RS_RET_PREVIOUS_COMMITTED
		   || localRet == RS_RET_DEFER_COMMIT) {
			bDone = 1;
		} else if(localRet == RS_RET_DISCARDMSG) {
			iRet = RS_RET_DISCARDMSG; /* TODO: verify this sequence -- rgerhards, 2009-07-30 */
			bDone = 1;
		} else if(localRet == RS_RET_SUSPENDED) {
			; /* do nothing, this will retry the full batch */
		} else if(localRet == RS_RET_ACTION_FAILED) {
			/* in this case, the whole batch can not be processed */
			for(i = 0 ; i < nElem ; ++i) {
				pBatch->pElem[++pBatch->iDoneUpTo].state = BATCH_STATE_BAD;
			}
			bDone = 1;
		} else {
			if(nElem == 1) {
				pBatch->pElem[++pBatch->iDoneUpTo].state = BATCH_STATE_BAD;
				bDone = 1;
			} else {
				/* retry with half as much. Depth is log_2 batchsize, so recursion is not too deep */
				submitBatch(pAction, pBatch, nElem / 2, pbShutdownImmediate);
				submitBatch(pAction, pBatch, nElem - (nElem / 2), pbShutdownImmediate);
				bDone = 1;
			}
		}
	} while(!bDone && !*pbShutdownImmediate); /* do .. while()! */

	if(*pbShutdownImmediate)
		ABORT_FINALIZE(RS_RET_FORCE_TERM);

finalize_it:
	RETiRet;
}


/* receive a batch and process it. This includes retry handling.
 * rgerhards, 2009-05-12
 */
static rsRetVal
processAction(action_t *pAction, batch_t *pBatch, int *pbShutdownImmediate)
{
	DEFiRet;

	assert(pBatch != NULL);
	pBatch->iDoneUpTo = 0;
	CHKiRet(submitBatch(pAction, pBatch, pBatch->nElem, pbShutdownImmediate));
	iRet = finishBatch(pAction, pBatch);

finalize_it:
	RETiRet;
}


#pragma GCC diagnostic ignored "-Wempty-body"
/* receive an array of to-process user pointers and submit them
 * for processing.
 * rgerhards, 2009-04-22
 */
static rsRetVal
processBatchMain(action_t *pAction, batch_t *pBatch, int *pbShutdownImmediate)
{
	DEFiRet;

	assert(pBatch != NULL);

	/* We now must guard the output module against execution by multiple threads. The
	 * plugin interface specifies that output modules must not be thread-safe (except
	 * if they notify us they are - functionality not yet implemented...).
	 * rgerhards, 2008-01-30
	 */
	d_pthread_mutex_lock(&pAction->mutActExec);
	pthread_cleanup_push(mutexCancelCleanup, &pAction->mutActExec);

	iRet = processAction(pAction, pBatch, pbShutdownImmediate);

	pthread_cleanup_pop(1); /* unlock mutex */

	RETiRet;
}
#pragma GCC diagnostic warning "-Wempty-body"


/* call the HUP handler for a given action, if such a handler is defined. The
 * action mutex is locked, because the HUP handler most probably needs to modify
 * some internal state information.
 * rgerhards, 2008-10-22
 */
#pragma GCC diagnostic ignored "-Wempty-body"
rsRetVal
actionCallHUPHdlr(action_t *pAction)
{
	DEFiRet;

	ASSERT(pAction != NULL);
	DBGPRINTF("Action %p checks HUP hdlr: %p\n", pAction, pAction->pMod->doHUP);

	if(pAction->pMod->doHUP == NULL) {
		FINALIZE;	/* no HUP handler, so we are done ;) */
	}

	d_pthread_mutex_lock(&pAction->mutActExec);
	pthread_cleanup_push(mutexCancelCleanup, &pAction->mutActExec);
	CHKiRet(pAction->pMod->doHUP(pAction->pModData));
	pthread_cleanup_pop(1); /* unlock mutex */

finalize_it:
	RETiRet;
}
#pragma GCC diagnostic warning "-Wempty-body"


/* set the action message queue mode
 * TODO: probably move this into queue object, merge with MainMsgQueue!
 * rgerhards, 2008-01-28
 */
static rsRetVal setActionQueType(void __attribute__((unused)) *pVal, uchar *pszType)
{
	DEFiRet;

	if (!strcasecmp((char *) pszType, "fixedarray")) {
		ActionQueType = QUEUETYPE_FIXED_ARRAY;
		DBGPRINTF("action queue type set to FIXED_ARRAY\n");
	} else if (!strcasecmp((char *) pszType, "linkedlist")) {
		ActionQueType = QUEUETYPE_LINKEDLIST;
		DBGPRINTF("action queue type set to LINKEDLIST\n");
	} else if (!strcasecmp((char *) pszType, "disk")) {
		ActionQueType = QUEUETYPE_DISK;
		DBGPRINTF("action queue type set to DISK\n");
	} else if (!strcasecmp((char *) pszType, "direct")) {
		ActionQueType = QUEUETYPE_DIRECT;
		DBGPRINTF("action queue type set to DIRECT (no queueing at all)\n");
	} else {
		errmsg.LogError(0, RS_RET_INVALID_PARAMS, "unknown actionqueue parameter: %s", (char *) pszType);
		iRet = RS_RET_INVALID_PARAMS;
	}
	d_free(pszType); /* no longer needed */

	RETiRet;
}


/* rgerhards 2004-11-09: fprintlog() is the actual driver for
 * the output channel. It receives the channel description (f) as
 * well as the message and outputs them according to the channel
 * semantics. The message is typically already contained in the
 * channel save buffer (f->f_prevline). This is not only the case
 * when a message was already repeated but also when a new message
 * arrived.
 * rgerhards 2007-08-01: interface changed to use action_t
 * rgerhards, 2007-12-11: please note: THIS METHOD MUST ONLY BE
 * CALLED AFTER THE CALLER HAS LOCKED THE pAction OBJECT! We do
 * not do this here. Failing to do so results in all kinds of
 * "interesting" problems!
 * RGERHARDS, 2008-01-29:
 * This is now the action caller and has been renamed.
 */
rsRetVal
actionWriteToAction(action_t *pAction)
{
	msg_t *pMsgSave;	/* to save current message pointer, necessary to restore
				   it in case it needs to be updated (e.g. repeated msgs) */
	DEFiRet;

	pMsgSave = NULL;	/* indicate message poiner not saved */

	/* first, we check if the action should actually be called. The action-specific
	 * $ActionExecOnlyEveryNthTime permits us to execute an action only every Nth
	 * time. So we need to check if we need to drop the (otherwise perfectly executable)
	 * action for this reason. Note that in case we need to drop it, we return RS_RET_OK
	 * as the action was properly "passed to execution" from the upper layer's point
	 * of view. -- rgerhards, 2008-08-07.
	 */
	if(pAction->iExecEveryNthOccur > 1) {
		/* we need to care about multiple occurences */
		if(   pAction->iExecEveryNthOccurTO > 0
		   && (getActNow(pAction) - pAction->tLastOccur) > pAction->iExecEveryNthOccurTO) {
		  	DBGPRINTF("n-th occurence handling timed out (%d sec), restarting from 0\n",
				  (int) (getActNow(pAction) - pAction->tLastOccur));
			pAction->iNbrNoExec = 0;
			pAction->tLastOccur = getActNow(pAction);
		   }
		if(pAction->iNbrNoExec < pAction->iExecEveryNthOccur - 1) {
			++pAction->iNbrNoExec;
			DBGPRINTF("action %p passed %d times to execution - less than neded - discarding\n",
			  pAction, pAction->iNbrNoExec);
			FINALIZE;
		} else {
			pAction->iNbrNoExec = 0; /* we execute the action now, so the number of no execs is down to */
		}
	}

	/* then check if this is a regular message or the repeation of
	 * a previous message. If so, we need to change the message text
	 * to "last message repeated n times" and then go ahead and write
	 * it. Please note that we can not modify the message object, because
	 * that would update it in other selectors as well. As such, we first
	 * need to create a local copy of the message, which we than can update.
	 * rgerhards, 2007-07-10
	 */
	if(pAction->f_prevcount > 1) {
		msg_t *pMsg;
		size_t lenRepMsg;
		uchar szRepMsg[1024];

		if((pMsg = MsgDup(pAction->f_pMsg)) == NULL) {
			/* it failed - nothing we can do against it... */
			DBGPRINTF("Message duplication failed, dropping repeat message.\n");
			ABORT_FINALIZE(RS_RET_ERR);
		}

		if(pAction->bRepMsgHasMsg == 0) { /* old format repeat message? */
			lenRepMsg = snprintf((char*)szRepMsg, sizeof(szRepMsg), " last message repeated %d times",
			    pAction->f_prevcount);
		} else {
			lenRepMsg = snprintf((char*)szRepMsg, sizeof(szRepMsg), " message repeated %d times: [%.800s]",
			    pAction->f_prevcount, getMSG(pAction->f_pMsg));
		}

		/* We now need to update the other message properties. Please note that digital
		 * signatures inside the message are also invalidated.
		 */
		datetime.getCurrTime(&(pMsg->tRcvdAt), &(pMsg->ttGenTime));
		memcpy(&pMsg->tTIMESTAMP, &pMsg->tRcvdAt, sizeof(struct syslogTime));
		MsgReplaceMSG(pMsg, szRepMsg, lenRepMsg);
		pMsgSave = pAction->f_pMsg;	/* save message pointer for later restoration */
		pAction->f_pMsg = pMsg;	/* use the new msg (pointer will be restored below) */
	}

	DBGPRINTF("Called action, logging to %s\n", module.GetStateName(pAction->pMod));

	/* now check if we need to drop the message because otherwise the action would be too
	 * frequently called. -- rgerhards, 2008-04-08
	 * Note that the check for "pAction->iSecsExecOnceInterval > 0" is not necessary from
	 * a purely logical point of view. However, if safes us to check the system time in
	 * (those common) cases where ExecOnceInterval is not used. -- rgerhards, 2008-09-16
	 */
	if(pAction->iSecsExecOnceInterval > 0 &&
	   pAction->iSecsExecOnceInterval + pAction->tLastExec > getActNow(pAction)) {
		/* in this case we need to discard the message - its not yet time to exec the action */
		DBGPRINTF("action not yet ready again to be executed, onceInterval %d, tCurr %d, tNext %d\n",
			  (int) pAction->iSecsExecOnceInterval, (int) getActNow(pAction),
			  (int) (pAction->iSecsExecOnceInterval + pAction->tLastExec));
		pAction->tLastExec = getActNow(pAction); /* re-init time flags */
		FINALIZE;
	}

	/* we use reception time, not dequeue time - this is considered more appropriate and also faster ;) -- rgerhards, 2008-09-17 */
	pAction->tLastExec = getActNow(pAction); /* re-init time flags */
	pAction->f_time = pAction->f_pMsg->ttGenTime;

	/* When we reach this point, we have a valid, non-disabled action.
	 * So let's enqueue our message for execution. -- rgerhards, 2007-07-24
	 */
	iRet = qqueueEnqObj(pAction->pQueue, pAction->f_pMsg->flowCtlType, (void*) MsgAddRef(pAction->f_pMsg));

	if(iRet == RS_RET_OK)
		pAction->f_prevcount = 0; /* message processed, so we start a new cycle */

finalize_it:
	if(pMsgSave != NULL) {
		/* we had saved the original message pointer. That was
		 * done because we needed to create a temporary one
		 * (most often for "message repeated n time" handling). If so,
		 * we need to restore the original one now, so that procesing
		 * can continue as normal. We also need to discard the temporary
		 * one, as we do not like memory leaks ;) Please note that the original
		 * message object will be discarded by our callers, so this is nothing
		 * of our business. rgerhards, 2007-07-10
		 */
		msgDestruct(&pAction->f_pMsg);
		pAction->f_pMsg = pMsgSave;	/* restore it */
	}

	RETiRet;
}


/* helper to actonCallAction, mostly needed because of this damn
 * pthread_cleanup_push() POSIX macro...
 */
static rsRetVal
doActionCallAction(action_t *pAction, msg_t *pMsg)
{
	DEFiRet;

	pAction->tActNow = -1; /* we do not yet know our current time (clear prev. value) */

	/* don't output marks to recently written outputs */
	if(pAction->bWriteAllMarkMsgs == FALSE
	   && (pMsg->msgFlags & MARK) && (getActNow(pAction) - pAction->f_time) < MarkInterval / 2) {
		ABORT_FINALIZE(RS_RET_OK);
	}

	/* suppress duplicate messages */
	if ((pAction->f_ReduceRepeated == 1) && pAction->f_pMsg != NULL &&
	    (pMsg->msgFlags & MARK) == 0 && getMSGLen(pMsg) == getMSGLen(pAction->f_pMsg) &&
	    !ustrcmp(getMSG(pMsg), getMSG(pAction->f_pMsg)) &&
	    !strcmp(getHOSTNAME(pMsg), getHOSTNAME(pAction->f_pMsg)) &&
	    !strcmp(getPROCID(pMsg, LOCK_MUTEX), getPROCID(pAction->f_pMsg, LOCK_MUTEX)) &&
	    !strcmp(getAPPNAME(pMsg, LOCK_MUTEX), getAPPNAME(pAction->f_pMsg, LOCK_MUTEX))) {
		pAction->f_prevcount++;
		DBGPRINTF("msg repeated %d times, %ld sec of %d.\n",
		    pAction->f_prevcount, (long) getActNow(pAction) - pAction->f_time,
		    repeatinterval[pAction->f_repeatcount]);
		/* use current message, so we have the new timestamp (means we need to discard previous one) */
		msgDestruct(&pAction->f_pMsg);
		pAction->f_pMsg = MsgAddRef(pMsg);
		/* If domark would have logged this by now, flush it now (so we don't hold
		 * isolated messages), but back off so we'll flush less often in the future.
		 */
		if(getActNow(pAction) > REPEATTIME(pAction)) {
			iRet = actionWriteToAction(pAction);
			BACKOFF(pAction);
		}
	} else {/* new message, save it */
		/* first check if we have a previous message stored
		 * if so, emit and then discard it first
		 */
		if(pAction->f_pMsg != NULL) {
			if(pAction->f_prevcount > 0)
				actionWriteToAction(pAction);
				/* we do not care about iRet above - I think it's right but if we have
				 * some troubles, you know where to look at ;) -- rgerhards, 2007-08-01
				 */
			msgDestruct(&pAction->f_pMsg);
		}
		pAction->f_pMsg = MsgAddRef(pMsg);
		/* call the output driver */
		iRet = actionWriteToAction(pAction);
	}

finalize_it:
	RETiRet;
}


/* call the configured action. Does all necessary housekeeping.
 * rgerhards, 2007-08-01
 * FYI: currently, this function is only called from the queue
 * consumer. So we (conceptually) run detached from the input
 * threads (which also means we may run much later than when the
 * message was generated).
 */
#pragma GCC diagnostic ignored "-Wempty-body"
rsRetVal
actionCallAction(action_t *pAction, msg_t *pMsg)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pMsg, msg);
	ASSERT(pAction != NULL);

	/* We need to lock the mutex only for repeated line processing. 
	 * rgerhards, 2009-06-19
	 */
	//if(pAction->f_ReduceRepeated == 1) {
		LockObj(pAction);
		pthread_cleanup_push(mutexCancelCleanup, pAction->Sync_mut);
		iRet = doActionCallAction(pAction, pMsg);
		UnlockObj(pAction);
		pthread_cleanup_pop(0); /* remove mutex cleanup handler */
	//} else {
		//iRet = doActionCallAction(pAction, pMsg);
	//}

	RETiRet;
}
#pragma GCC diagnostic warning "-Wempty-body"


/* add an Action to the current selector
 * The pOMSR is freed, as it is not needed after this function.
 * Note: this function pulls global data that specifies action config state.
 * rgerhards, 2007-07-27
 */
rsRetVal
addAction(action_t **ppAction, modInfo_t *pMod, void *pModData, omodStringRequest_t *pOMSR, int bSuspended)
{
	DEFiRet;
	int i;
	int iTplOpts;
	uchar *pTplName;
	action_t *pAction;
	char errMsg[512];

	assert(ppAction != NULL);
	assert(pMod != NULL);
	assert(pOMSR != NULL);
	DBGPRINTF("Module %s processed this config line.\n", module.GetName(pMod));

	CHKiRet(actionConstruct(&pAction)); /* create action object first */
	pAction->pMod = pMod;
	pAction->pModData = pModData;
	pAction->pszName = pszActionName;
	pszActionName = NULL;	/* free again! */
	pAction->bWriteAllMarkMsgs = bActionWriteAllMarkMsgs;
	bActionWriteAllMarkMsgs = FALSE; /* reset */
	pAction->bExecWhenPrevSusp = bActExecWhenPrevSusp;
	pAction->iSecsExecOnceInterval = iActExecOnceInterval;
	pAction->iExecEveryNthOccur = iActExecEveryNthOccur;
	pAction->iExecEveryNthOccurTO = iActExecEveryNthOccurTO;
	pAction->bRepMsgHasMsg = bActionRepMsgHasMsg;
	iActExecEveryNthOccur = 0; /* auto-reset */
	iActExecEveryNthOccurTO = 0; /* auto-reset */

	/* check if we can obtain the template pointers - TODO: move to separate function? */
	pAction->iNumTpls = OMSRgetEntryCount(pOMSR);
	assert(pAction->iNumTpls >= 0); /* only debug check because this "can not happen" */
	/* please note: iNumTpls may validly be zero. This is the case if the module
	 * does not request any templates. This sounds unlikely, but an actual example is
	 * the discard action, which does not require a string. -- rgerhards, 2007-07-30
	 */
	if(pAction->iNumTpls > 0) {
		/* we first need to create the template pointer array */
		CHKmalloc(pAction->ppTpl = (struct template **)calloc(pAction->iNumTpls, sizeof(struct template *)));
		CHKmalloc(pAction->ppMsgs = (uchar**) calloc(pAction->iNumTpls, sizeof(uchar *)));
		CHKmalloc(pAction->lenMsgs = (size_t*) calloc(pAction->iNumTpls, sizeof(size_t)));
	}
	
	for(i = 0 ; i < pAction->iNumTpls ; ++i) {
		CHKiRet(OMSRgetEntry(pOMSR, i, &pTplName, &iTplOpts));
		/* Ok, we got everything, so it now is time to look up the template
		 * (Hint: templates MUST be defined before they are used!)
		 */
		if((pAction->ppTpl[i] = tplFind((char*)pTplName, strlen((char*)pTplName))) == NULL) {
			snprintf(errMsg, sizeof(errMsg) / sizeof(char),
				 " Could not find template '%s' - action disabled\n",
				 pTplName);
			errno = 0;
			errmsg.LogError(0, RS_RET_NOT_FOUND, "%s", errMsg);
			ABORT_FINALIZE(RS_RET_NOT_FOUND);
		}
		/* check required template options */
		if(   (iTplOpts & OMSR_RQD_TPL_OPT_SQL)
		   && (pAction->ppTpl[i]->optFormatForSQL == 0)) {
			errno = 0;
			errmsg.LogError(0, RS_RET_RQD_TPLOPT_MISSING, "Action disabled. To use this action, you have to specify "
				"the SQL or stdSQL option in your template!\n");
			ABORT_FINALIZE(RS_RET_RQD_TPLOPT_MISSING);
		}

		/* set parameter-passing mode */
		if(iTplOpts & OMSR_TPL_AS_ARRAY) {
			pAction->eParamPassing = ACT_ARRAY_PASSING;
		} else if(iTplOpts & OMSR_TPL_AS_MSG) {
			pAction->eParamPassing = ACT_MSG_PASSING;
		} else {
			pAction->eParamPassing = ACT_STRING_PASSING;
		}

		DBGPRINTF("template: '%s' assigned\n", pTplName);
	}

	pAction->pMod = pMod;
	pAction->pModData = pModData;
	/* now check if the module is compatible with select features */
	if(pMod->isCompatibleWithFeature(sFEATURERepeatedMsgReduction) == RS_RET_OK)
		pAction->f_ReduceRepeated = bReduceRepeatMsgs;
	else {
		DBGPRINTF("module is incompatible with RepeatedMsgReduction - turned off\n");
		pAction->f_ReduceRepeated = 0;
	}
	pAction->eState = ACT_STATE_RDY; /* action is enabled */

	if(bSuspended)
		actionSuspend(pAction, datetime.GetTime(NULL)); /* "good" time call, only during init and unavoidable */

	CHKiRet(actionConstructFinalize(pAction));
	
	/* TODO: if we exit here, we have a memory leak... */

	*ppAction = pAction; /* finally store the action pointer */

finalize_it:
	if(iRet == RS_RET_OK)
		iRet = OMSRdestruct(pOMSR);
	else {
		/* do not overwrite error state! */
		OMSRdestruct(pOMSR);
		if(pAction != NULL)
			actionDestruct(pAction);
	}

	RETiRet;
}


/* Reset config variables to default values.
 * rgerhards, 2009-11-12
 */
static rsRetVal
resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	iActExecOnceInterval = 0;
	return RS_RET_OK;
}


/* TODO: we are not yet a real object, the ClassInit here just looks like it is..
 */
rsRetVal actionClassInit(void)
{
	DEFiRet;
	/* request objects we use */
	CHKiRet(objGetObjInterface(&obj)); /* this provides the root pointer for all other queries */
	CHKiRet(objUse(datetime, CORE_COMPONENT));
	CHKiRet(objUse(module, CORE_COMPONENT));
	CHKiRet(objUse(errmsg, CORE_COMPONENT));

	CHKiRet(regCfSysLineHdlr((uchar *)"actionname", 0, eCmdHdlrGetWord, NULL, &pszActionName, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuefilename", 0, eCmdHdlrGetWord, NULL, &pszActionQFName, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuesize", 0, eCmdHdlrInt, NULL, &iActionQueueSize, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionwriteallmarkmessages", 0, eCmdHdlrBinary, NULL, &bActionWriteAllMarkMsgs, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuedequeuebatchsize", 0, eCmdHdlrInt, NULL, &iActionQueueDeqBatchSize, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuemaxdiskspace", 0, eCmdHdlrSize, NULL, &iActionQueMaxDiskSpace, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuehighwatermark", 0, eCmdHdlrInt, NULL, &iActionQHighWtrMark, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuelowwatermark", 0, eCmdHdlrInt, NULL, &iActionQLowWtrMark, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuediscardmark", 0, eCmdHdlrInt, NULL, &iActionQDiscardMark, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuediscardseverity", 0, eCmdHdlrInt, NULL, &iActionQDiscardSeverity, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuecheckpointinterval", 0, eCmdHdlrInt, NULL, &iActionQPersistUpdCnt, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuesyncqueuefiles", 0, eCmdHdlrBinary, NULL, &bActionQSyncQeueFiles, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuetype", 0, eCmdHdlrGetWord, setActionQueType, NULL, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueueworkerthreads", 0, eCmdHdlrInt, NULL, &iActionQueueNumWorkers, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuetimeoutshutdown", 0, eCmdHdlrInt, NULL, &iActionQtoQShutdown, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuetimeoutactioncompletion", 0, eCmdHdlrInt, NULL, &iActionQtoActShutdown, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuetimeoutenqueue", 0, eCmdHdlrInt, NULL, &iActionQtoEnq, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueueworkertimeoutthreadshutdown", 0, eCmdHdlrInt, NULL, &iActionQtoWrkShutdown, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueueworkerthreadminimummessages", 0, eCmdHdlrInt, NULL, &iActionQWrkMinMsgs, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuemaxfilesize", 0, eCmdHdlrSize, NULL, &iActionQueMaxFileSize, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuesaveonshutdown", 0, eCmdHdlrBinary, NULL, &bActionQSaveOnShutdown, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuedequeueslowdown", 0, eCmdHdlrInt, NULL, &iActionQueueDeqSlowdown, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuedequeuetimebegin", 0, eCmdHdlrInt, NULL, &iActionQueueDeqtWinFromHr, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuedequeuetimeend", 0, eCmdHdlrInt, NULL, &iActionQueueDeqtWinToHr, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionexeconlyeverynthtime", 0, eCmdHdlrInt, NULL, &iActExecEveryNthOccur, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionexeconlyeverynthtimetimeout", 0, eCmdHdlrInt, NULL, &iActExecEveryNthOccurTO, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionexeconlyonceeveryinterval", 0, eCmdHdlrInt, NULL, &iActExecOnceInterval, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"repeatedmsgcontainsoriginalmsg", 0, eCmdHdlrBinary, NULL, &bActionRepMsgHasMsg, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL, NULL));

finalize_it:
	RETiRet;
}

/* vi:set ai:
 */
