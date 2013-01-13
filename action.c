/* action.c
 *
 * Implementation of the action object.
 *
 * File begun on 2007-08-06 by RGerhards (extracted from syslogd.c)
 *
 * Some notes on processing (this hopefully makes it easier to find
 * the right code in question): For performance reasons, this module
 * uses different methods of message submission based on the user-selected
 * configuration. This code is similar, but can not be abstracted because
 * of the performance-affecting differences in it. As such, it is often
 * necessary to triple-check that everything works well in *all* modes.
 * The different modes (and calling sequence) are:
 *
 * if set iExecEveryNthOccur > 1 || iSecsExecOnceInterval
 * - doSubmitToActionQComplexBatch
 * - helperSubmitToActionQComplexBatch
 * - doActionCallAction
 *   handles mark message reduction, but in essence calls
 * - actionWriteToAction
 * - qqueueEnqObj
 *   (now queue engine processing)
 * if(pThis->bWriteAllMarkMsgs == RSFALSE) - this is the DEFAULT
 * - doSubmitToActionQNotAllMarkBatch
 * - doSubmitToActionQBatch (and from here like in the else case below!)
 * else
 * - doSubmitToActionQBatch
 * - doSubmitToActionQ
 * - qqueueEnqObj
 *   (now queue engine processing)
 *
 * Note that bWriteAllMakrMsgs on or off creates almost the same processing.
 * The difference ist that if WriteAllMarkMsgs is not set, we need to
 * preprocess the batch and drop mark messages which are not yet due for
 * writing.
 *
 * After dequeue, processing is as follows:
 * - processBatchMain
 * - processAction
 * - submitBatch
 * - tryDoAction
 * - ...
 *
 * MORE ON PROCESSING, QUEUES and FILTERING
 * All filtering needs to be done BEFORE messages are enqueued to an
 * action. In previous code, part of the filtering was done at the
 * "remote end" of the action queue, which lead to problems in
 * non-direct mode (because then things run asynchronously). In order
 * to solve this problem once and for all, I have changed the code so
 * that all filtering is done before enq, and processing on the
 * dequeue side of action processing now always executes whatever is
 * enqueued. This is the only way to handle things consistently and
 * (as much as possible) in a queue-type agnostic way. However, it is
 * a rather radical change, which I unfortunately needed to make from
 * stable version 5.8.1 to 5.8.2. If new problems pop up, you now know
 * what may be their cause. In any case, the way it is done now is the
 * only correct one.
 * A problem is that, under fortunate conditions, we use the current
 * batch for the output system as well. This is very good from a performance
 * point of view, but makes the distinction between enq and deq side of
 * the queue a bit hard. The current idea is that the filter condition
 * alone is checked at the deq side of the queue (seems to be unavoidable
 * to do it that way), but all other complex conditons (like failover
 * handling) go into the computation of the filter condition. For
 * non-direct queues, we still enqueue only what is acutally necessary.
 * Note that in this case the rest of the code must ensure that the filter
 * is set to "true". While this is not perfect and not as simple as
 * we would like to see it, it looks like the best way to tackle that
 * beast.
 * rgerhards, 2011-06-15
 *
 * Copyright 2007-2011 Rainer Gerhards and Adiscon GmbH.
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
#include <json/json.h>

#include "dirty.h"
#include "template.h"
#include "action.h"
#include "modules.h"
#include "cfsysline.h"
#include "srUtils.h"
#include "errmsg.h"
#include "batch.h"
#include "wti.h"
#include "rsconf.h"
#include "datetime.h"
#include "unicode-helper.h"
#include "atomic.h"
#include "ruleset.h"
#include "statsobj.h"

#define NO_TIME_PROVIDED 0 /* indicate we do not provide any cached time */

/* forward definitions */
static rsRetVal processBatchMain(action_t *pAction, batch_t *pBatch, int*);
static rsRetVal doSubmitToActionQComplexBatch(action_t *pAction, batch_t *pBatch);
static rsRetVal doSubmitToActionQNotAllMarkBatch(action_t *pAction, batch_t *pBatch);
static rsRetVal doSubmitToActionQBatch(action_t *pAction, batch_t *pBatch);

/* object static data (once for all instances) */
/* TODO: make this an object! DEFobjStaticHelpers -- rgerhards, 2008-03-05 */
DEFobjCurrIf(obj)
DEFobjCurrIf(datetime)
DEFobjCurrIf(module)
DEFobjCurrIf(errmsg)
DEFobjCurrIf(statsobj)
DEFobjCurrIf(ruleset)


typedef struct configSettings_s {
	int bActExecWhenPrevSusp;			/* execute action only when previous one was suspended? */
	int bActionWriteAllMarkMsgs;			/* should all mark messages be unconditionally written? */
	int iActExecOnceInterval;			/* execute action once every nn seconds */
	int iActExecEveryNthOccur;			/* execute action every n-th occurence (0,1=always) */
	time_t iActExecEveryNthOccurTO;			/* timeout for n-occurence setting (in seconds, 0=never) */
	int glbliActionResumeInterval;
	int glbliActionResumeRetryCount;		/* how often should suspended actions be retried? */
	int bActionRepMsgHasMsg;			/* last messsage repeated... has msg fragment in it */
	uchar *pszActionName;				/* short name for the action */
	/* action queue and its configuration parameters */
	queueType_t ActionQueType;			/* type of the main message queue above */
	int iActionQueueSize;				/* size of the main message queue above */
	int iActionQueueDeqBatchSize;			/* batch size for action queues */
	int iActionQHighWtrMark;			/* high water mark for disk-assisted queues */
	int iActionQLowWtrMark;				/* low water mark for disk-assisted queues */
	int iActionQDiscardMark;			/* begin to discard messages */
	int iActionQDiscardSeverity;			/* by default, discard nothing to prevent unintentional loss */
	int iActionQueueNumWorkers;			/* number of worker threads for the mm queue above */
	uchar *pszActionQFName;				/* prefix for the main message queue file */
	int64 iActionQueMaxFileSize;
	int iActionQPersistUpdCnt;			/* persist queue info every n updates */
	int bActionQSyncQeueFiles;			/* sync queue files */
	int iActionQtoQShutdown;			/* queue shutdown */ 
	int iActionQtoActShutdown;			/* action shutdown (in phase 2) */ 
	int iActionQtoEnq;				/* timeout for queue enque */ 
	int iActionQtoWrkShutdown;			/* timeout for worker thread shutdown */
	int iActionQWrkMinMsgs;				/* minimum messages per worker needed to start a new one */
	int bActionQSaveOnShutdown;			/* save queue on shutdown (when DA enabled)? */
	int64 iActionQueMaxDiskSpace;			/* max disk space allocated 0 ==> unlimited */
	int iActionQueueDeqSlowdown;			/* dequeue slowdown (simple rate limiting) */
	int iActionQueueDeqtWinFromHr;			/* hour begin of time frame when queue is to be dequeued */
	int iActionQueueDeqtWinToHr;			/* hour begin of time frame when queue is to be dequeued */
} configSettings_t;

configSettings_t cs;					/* our current config settings */
configSettings_t cs_save;				/* our saved (scope!) config settings */

/* the counter below counts actions created. It is used to obtain unique IDs for the action. They
 * should not be relied on for any long-term activity (e.g. disk queue names!), but they are nice
 * to have during one instance of an rsyslogd run. For example, I use them to name actions when there
 * is no better name available. Note that I do NOT recover previous numbers on HUP - we simply keep
 * counting. -- rgerhards, 2008-01-29
 */
static int iActionNbr = 0;

/* tables for interfacing with the v6 config system */
static struct cnfparamdescr cnfparamdescr[] = {
	{ "name", eCmdHdlrGetWord, 0 }, /* legacy: actionname */
	{ "type", eCmdHdlrString, CNFPARAM_REQUIRED }, /* legacy: actionname */
	{ "action.writeallmarkmessages", eCmdHdlrBinary, 0 }, /* legacy: actionwriteallmarkmessages */
	{ "action.execonlyeverynthtime", eCmdHdlrInt, 0 }, /* legacy: actionexeconlyeverynthtime */
	{ "action.execonlyeverynthtimetimeout", eCmdHdlrInt, 0 }, /* legacy: actionexeconlyeverynthtimetimeout */
	{ "action.execonlyonceeveryinterval", eCmdHdlrInt, 0 }, /* legacy: actionexeconlyonceeveryinterval */
	{ "action.execonlywhenpreviousissuspended", eCmdHdlrInt, 0 }, /* legacy: actionexeconlywhenpreviousissuspended */
	{ "action.repeatedmsgcontainsoriginalmsg", eCmdHdlrBinary, 0 }, /* legacy: repeatedmsgcontainsoriginalmsg */
	{ "action.resumeretrycount", eCmdHdlrInt, 0 }, /* legacy: actionresumeretrycount */
	{ "action.resumeinterval", eCmdHdlrInt, 0 }
};
static struct cnfparamblk pblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(cnfparamdescr)/sizeof(struct cnfparamdescr),
	  cnfparamdescr
	};

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

	cs.ActionQueType = QUEUETYPE_DIRECT;		/* type of the main message queue above */
	cs.iActionQueueSize = 1000;			/* size of the main message queue above */
	cs.iActionQueueDeqBatchSize = 16;		/* default batch size */
	cs.iActionQHighWtrMark = 800;			/* high water mark for disk-assisted queues */
	cs.iActionQLowWtrMark = 200;			/* low water mark for disk-assisted queues */
	cs.iActionQDiscardMark = 9800;			/* begin to discard messages */
	cs.iActionQDiscardSeverity = 8;			/* discard warning and above */
	cs.iActionQueueNumWorkers = 1;			/* number of worker threads for the mm queue above */
	cs.iActionQueMaxFileSize = 1024*1024;
	cs.iActionQPersistUpdCnt = 0;			/* persist queue info every n updates */
	cs.bActionQSyncQeueFiles = 0;
	cs.iActionQtoQShutdown = 0;			/* queue shutdown */ 
	cs.iActionQtoActShutdown = 1000;		/* action shutdown (in phase 2) */ 
	cs.iActionQtoEnq = 50;				/* timeout for queue enque */ 
	cs.iActionQtoWrkShutdown = 60000;		/* timeout for worker thread shutdown */
	cs.iActionQWrkMinMsgs = 100;			/* minimum messages per worker needed to start a new one */
	cs.bActionQSaveOnShutdown = 1;			/* save queue on shutdown (when DA enabled)? */
	cs.iActionQueMaxDiskSpace = 0;
	cs.iActionQueueDeqSlowdown = 0;
	cs.iActionQueueDeqtWinFromHr = 0;
	cs.iActionQueueDeqtWinToHr = 25;		/* 25 disables time windowed dequeuing */

	cs.glbliActionResumeRetryCount = 0;		/* I guess it is smart to reset this one, too */

	d_free(cs.pszActionQFName);
	cs.pszActionQFName = NULL;			/* prefix for the main message queue file */

	RETiRet;
}


/* destructs an action descriptor object
 * rgerhards, 2007-08-01
 */
rsRetVal actionDestruct(action_t *pThis)
{
	DEFiRet;
	ASSERT(pThis != NULL);

	if(!strcmp((char*)modGetName(pThis->pMod), "builtin:omdiscard")) {
		/* discard actions will be optimized out */
		FINALIZE;
	}

	if(pThis->pQueue != NULL) {
		qqueueDestruct(&pThis->pQueue);
	}

	/* destroy stats object, if we have one (may not always be
	 * be the case, e.g. if turned off)
	 */
	if(pThis->statsobj != NULL)
		statsobj.Destruct(&pThis->statsobj);

	if(pThis->pMod != NULL)
		pThis->pMod->freeInstance(pThis->pModData);

	pthread_mutex_destroy(&pThis->mutAction);
	pthread_mutex_destroy(&pThis->mutActExec);
	d_free(pThis->pszName);
	d_free(pThis->ppTpl);

finalize_it:
	d_free(pThis);
	RETiRet;
}


/* create a new action descriptor object
 * rgerhards, 2007-08-01
 * Note that it is vital to set proper initial values as the v6 config
 * system depends on these!
 */
rsRetVal actionConstruct(action_t **ppThis)
{
	DEFiRet;
	action_t *pThis;

	ASSERT(ppThis != NULL);
	
	CHKmalloc(pThis = (action_t*) calloc(1, sizeof(action_t)));
	pThis->iResumeInterval = 30;
	pThis->iResumeRetryCount = 0;
	pThis->pszName = NULL;
	pThis->bWriteAllMarkMsgs = RSFALSE;
	pThis->iExecEveryNthOccur = 0;
	pThis->iExecEveryNthOccurTO = 0;
	pThis->iSecsExecOnceInterval = 0;
	pThis->bExecWhenPrevSusp = 0;
	pThis->bRepMsgHasMsg = 0;
	pThis->tLastOccur = datetime.GetTime(NULL);	/* done once per action on startup only */
	pthread_mutex_init(&pThis->mutActExec, NULL);
	pthread_mutex_init(&pThis->mutAction, NULL);
	INIT_ATOMIC_HELPER_MUT(pThis->mutCAS);

	/* indicate we have a new action */
	++iActionNbr;

finalize_it:
	*ppThis = pThis;
	RETiRet;
}


/* action construction finalizer
 */
rsRetVal
actionConstructFinalize(action_t *pThis, struct cnfparamvals *queueParams)
{
	DEFiRet;
	uchar pszAName[64]; /* friendly name of our action */

	ASSERT(pThis != NULL);

	if(!strcmp((char*)modGetName(pThis->pMod), "builtin:omdiscard")) {
		/* discard actions will be optimized out */
		FINALIZE;
	}
	/* generate a friendly name for us action stats */
	if(pThis->pszName == NULL) {
		snprintf((char*) pszAName, sizeof(pszAName)/sizeof(uchar), "action %d", iActionNbr);
	} else {
		ustrncpy(pszAName, pThis->pszName, sizeof(pszAName));
		pszAName[sizeof(pszAName)-1] = '\0'; /* to be on the save side */
	}

	/* support statistics gathering */
	CHKiRet(statsobj.Construct(&pThis->statsobj));
	CHKiRet(statsobj.SetName(pThis->statsobj, pszAName));

	STATSCOUNTER_INIT(pThis->ctrProcessed, pThis->mutCtrProcessed);
	CHKiRet(statsobj.AddCounter(pThis->statsobj, UCHAR_CONSTANT("processed"),
		ctrType_IntCtr, &pThis->ctrProcessed));

	STATSCOUNTER_INIT(pThis->ctrFail, pThis->mutCtrFail);
	CHKiRet(statsobj.AddCounter(pThis->statsobj, UCHAR_CONSTANT("failed"),
		ctrType_IntCtr, &pThis->ctrFail));

	CHKiRet(statsobj.ConstructFinalize(pThis->statsobj));

	/* create our queue */

	/* generate a friendly name for the queue */
	if(pThis->pszName == NULL) {
		snprintf((char*) pszAName, sizeof(pszAName)/sizeof(uchar), "action %d queue",
			 iActionNbr);
	} else {
		ustrncpy(pszAName, pThis->pszName, sizeof(pszAName));
		pszAName[63] = '\0'; /* to be on the save side */
	}
	/* now check if we can run the action in "firehose mode" during stage one of 
	 * its processing (that is before messages are enqueued into the action q).
	 * This is only possible if some features, which require strict sequence, are
	 * not used. Thankfully, that is usually the case. The benefit of firehose
	 * mode is much faster processing (and simpler code) -- rgerhards, 2010-06-08
	 */
	if(   pThis->iExecEveryNthOccur > 1
	   || pThis->iSecsExecOnceInterval
	  ) {
		DBGPRINTF("info: firehose mode disabled for action because "
		          "iExecEveryNthOccur=%d, iSecsExecOnceInterval=%d\n",
			  pThis->iExecEveryNthOccur, pThis->iSecsExecOnceInterval);
		pThis->submitToActQ = doSubmitToActionQComplexBatch;
	} else if(pThis->bWriteAllMarkMsgs == RSFALSE) {
		/* nearly full-speed submission mode, default case */
		pThis->submitToActQ = doSubmitToActionQNotAllMarkBatch;
	} else {
		/* full firehose submission mode */
		pThis->submitToActQ = doSubmitToActionQBatch;
	}

	/* create queue */
	/* action queues always (for now) have just one worker. This may change when
	 * we begin to implement an interface the enable output modules to request
	 * to be run on multiple threads. So far, this is forbidden by the interface
	 * spec. -- rgerhards, 2008-01-30
	 */
	CHKiRet(qqueueConstruct(&pThis->pQueue, cs.ActionQueType, 1, cs.iActionQueueSize,
					(rsRetVal (*)(void*, batch_t*, int*))processBatchMain));
	obj.SetName((obj_t*) pThis->pQueue, pszAName);
	qqueueSetpAction(pThis->pQueue, pThis);

	if(queueParams == NULL) { /* use legacy params? */
		/* ... set some properties ... */
#		define setQPROP(func, directive, data) \
		CHKiRet_Hdlr(func(pThis->pQueue, data)) { \
			errmsg.LogError(0, NO_ERRCODE, "Invalid " #directive ", \
				error %d. Ignored, running with default setting", iRet); \
		}
#		define setQPROPstr(func, directive, data) \
		CHKiRet_Hdlr(func(pThis->pQueue, data, (data == NULL)? 0 : strlen((char*) data))) { \
			errmsg.LogError(0, NO_ERRCODE, "Invalid " #directive ", \
				error %d. Ignored, running with default setting", iRet); \
		}
		setQPROP(qqueueSetsizeOnDiskMax, "$ActionQueueMaxDiskSpace", cs.iActionQueMaxDiskSpace);
		setQPROP(qqueueSetiDeqBatchSize, "$ActionQueueDequeueBatchSize", cs.iActionQueueDeqBatchSize);
		setQPROP(qqueueSetMaxFileSize, "$ActionQueueFileSize", cs.iActionQueMaxFileSize);
		setQPROPstr(qqueueSetFilePrefix, "$ActionQueueFileName", cs.pszActionQFName);
		setQPROP(qqueueSetiPersistUpdCnt, "$ActionQueueCheckpointInterval", cs.iActionQPersistUpdCnt);
		setQPROP(qqueueSetbSyncQueueFiles, "$ActionQueueSyncQueueFiles", cs.bActionQSyncQeueFiles);
		setQPROP(qqueueSettoQShutdown, "$ActionQueueTimeoutShutdown", cs.iActionQtoQShutdown );
		setQPROP(qqueueSettoActShutdown, "$ActionQueueTimeoutActionCompletion", cs.iActionQtoActShutdown);
		setQPROP(qqueueSettoWrkShutdown, "$ActionQueueWorkerTimeoutThreadShutdown", cs.iActionQtoWrkShutdown);
		setQPROP(qqueueSettoEnq, "$ActionQueueTimeoutEnqueue", cs.iActionQtoEnq);
		setQPROP(qqueueSetiHighWtrMrk, "$ActionQueueHighWaterMark", cs.iActionQHighWtrMark);
		setQPROP(qqueueSetiLowWtrMrk, "$ActionQueueLowWaterMark", cs.iActionQLowWtrMark);
		setQPROP(qqueueSetiDiscardMrk, "$ActionQueueDiscardMark", cs.iActionQDiscardMark);
		setQPROP(qqueueSetiDiscardSeverity, "$ActionQueueDiscardSeverity", cs.iActionQDiscardSeverity);
		setQPROP(qqueueSetiMinMsgsPerWrkr, "$ActionQueueWorkerThreadMinimumMessages", cs.iActionQWrkMinMsgs);
		setQPROP(qqueueSetbSaveOnShutdown, "$ActionQueueSaveOnShutdown", cs.bActionQSaveOnShutdown);
		setQPROP(qqueueSetiDeqSlowdown,    "$ActionQueueDequeueSlowdown", cs.iActionQueueDeqSlowdown);
		setQPROP(qqueueSetiDeqtWinFromHr,  "$ActionQueueDequeueTimeBegin", cs.iActionQueueDeqtWinFromHr);
		setQPROP(qqueueSetiDeqtWinToHr,    "$ActionQueueDequeueTimeEnd", cs.iActionQueueDeqtWinToHr);
	} else {
		/* we have v6-style config params */
		qqueueSetDefaultsActionQueue(pThis->pQueue);
		qqueueApplyCnfParam(pThis->pQueue, queueParams);
	}

#	undef setQPROP
#	undef setQPROPstr

	qqueueDbgPrint(pThis->pQueue);

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
	cs.glbliActionResumeInterval = iNewVal;
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
	pThis->iResumeOKinRow++;
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
 * iterations. -- rgerhards, 2009-05-07
 * We need to guard against module which always return RS_RET_OK from their tryResume()
 * entry point. This is invalid, but has harsh consequences: it will cause the rsyslog
 * engine to go into a tight loop. That obviously is not acceptable. As such, we track the
 * count of iterations that a tryResume returning RS_RET_OK is immediately followed by
 * an unsuccessful call to doAction(). If that happens more than 1,000 times, we assume 
 * the return acutally is a RS_RET_SUSPENDED. In order to go through the various 
 * resumption stages, we do this for every 1000 requests. This magic number 1000 may
 * not be the most appropriate, but it should be thought of a "if nothing else helps"
 * kind of facility: in the first place, the module should return a proper indication
 * of its inability to recover. -- rgerhards, 2010-04-26.
 */
static inline rsRetVal
actionDoRetry(action_t *pThis, time_t ttNow, int *pbShutdownImmediate)
{
	int iRetries;
	int iSleepPeriod;
	int bTreatOKasSusp;
	DEFiRet;

	ASSERT(pThis != NULL);

	iRetries = 0;
	while((*pbShutdownImmediate == 0) && pThis->eState == ACT_STATE_RTRY) {
		iRet = pThis->pMod->tryResume(pThis->pModData);
		if((pThis->iResumeOKinRow > 9) && (pThis->iResumeOKinRow % 10 == 0)) {
			bTreatOKasSusp = 1;
			pThis->iResumeOKinRow = 0;
		} else {
			bTreatOKasSusp = 0;
		}
		if((iRet == RS_RET_OK) && (!bTreatOKasSusp)) {
			actionSetState(pThis, ACT_STATE_RDY);
		} else if(iRet == RS_RET_SUSPENDED || bTreatOKasSusp) {
			/* max retries reached? */
			if((pThis->iResumeRetryCount != -1 && iRetries >= pThis->iResumeRetryCount)) {
				actionSuspend(pThis, ttNow);
			} else {
				++pThis->iNbrResRtry;
				++iRetries;
				iSleepPeriod = pThis->iResumeInterval;
				ttNow += iSleepPeriod; /* not truly exact, but sufficiently... */
				srSleep(iSleepPeriod, 0);
				if(*pbShutdownImmediate) {
					ABORT_FINALIZE(RS_RET_FORCE_TERM);
				}
			}
		} else if(iRet == RS_RET_DISABLE_ACTION) {
			actionDisable(pThis);
		}
	}

	if(pThis->eState == ACT_STATE_RDY) {
		pThis->iNbrResRtry = 0;
	}

finalize_it:
	RETiRet;
}


/* try to resume an action -- rgerhards, 2007-08-02
 * changed to new action state engine -- rgerhards, 2009-05-07
 */
static rsRetVal actionTryResume(action_t *pThis, int *pbShutdownImmediate)
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
		if(ttNow >= pThis->ttResumeRtry) {
			actionSetState(pThis, ACT_STATE_RTRY); /* back to retries */
		}
	}

	if(pThis->eState == ACT_STATE_RTRY) {
		if(ttNow == NO_TIME_PROVIDED) /* use cached result if we have it */
			datetime.GetTime(&ttNow);
		CHKiRet(actionDoRetry(pThis, ttNow, pbShutdownImmediate));
	}

	if(Debug && (pThis->eState == ACT_STATE_RTRY ||pThis->eState == ACT_STATE_SUSP)) {
		DBGPRINTF("actionTryResume: action %p state: %s, next retry (if applicable): %u [now %u]\n",
			pThis, getActStateName(pThis), (unsigned) pThis->ttResumeRtry, (unsigned) ttNow);
	}

finalize_it:
	RETiRet;
}


/* prepare an action for performing work. This involves trying to recover it,
 * depending on its current state.
 * rgerhards, 2009-05-07
 */
static inline rsRetVal actionPrepare(action_t *pThis, int *pbShutdownImmediate)
{
	DEFiRet;

	assert(pThis != NULL);
	CHKiRet(actionTryResume(pThis, pbShutdownImmediate));

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
	char *sz;

	dbgprintf("%s: ", module.GetStateName(pThis->pMod));
	pThis->pMod->dbgPrintInstInfo(pThis->pModData);
	dbgprintf("\n");
	dbgprintf("\tInstance data: 0x%lx\n", (unsigned long) pThis->pModData);
	dbgprintf("\tResume Interval: %d\n", pThis->iResumeInterval);
	if(pThis->eState == ACT_STATE_SUSP) {
		dbgprintf("\tresume next retry: %u, number retries: %d",
			  (unsigned) pThis->ttResumeRtry, pThis->iNbrResRtry);
	}
	dbgprintf("\tState: %s\n", getActStateName(pThis));
	dbgprintf("\tExec only when previous is suspended: %d\n", pThis->bExecWhenPrevSusp);
	if(pThis->submitToActQ == doSubmitToActionQComplexBatch) {
			sz = "slow, but feature-rich";
	} else if(pThis->submitToActQ == doSubmitToActionQNotAllMarkBatch) {
			sz = "fast, but supports partial mark messages";
	} else if(pThis->submitToActQ == doSubmitToActionQBatch) {
			sz = "firehose (fastest)";
	} else {
			sz = "unknown (need to update debug display?)";
	}
	dbgprintf("\tsubmission mode: %s\n", sz);
	dbgprintf("\n");

	RETiRet;
}


/* prepare the calling parameters for doAction()
 * rgerhards, 2009-05-07
 */
static rsRetVal
prepareDoActionParams(action_t *pAction, batch_obj_t *pElem, struct syslogTime *ttNow)
{
	int i;
	msg_t *pMsg;
	struct json_object *json;
	DEFiRet;

	ASSERT(pAction != NULL);
	ASSERT(pElem != NULL);

	pMsg = pElem->pMsg;
	/* here we must loop to process all requested strings */
	for(i = 0 ; i < pAction->iNumTpls ; ++i) {
		switch(pAction->eParamPassing) {
			case ACT_STRING_PASSING:
				CHKiRet(tplToString(pAction->ppTpl[i], pMsg, &(pElem->staticActStrings[i]),
					&pElem->staticLenStrings[i], ttNow));
				pElem->staticActParams[i] = pElem->staticActStrings[i];
				break;
			case ACT_ARRAY_PASSING:
				CHKiRet(tplToArray(pAction->ppTpl[i], pMsg, (uchar***) &(pElem->staticActParams[i]), ttNow));
				break;
			case ACT_MSG_PASSING:
				pElem->staticActParams[i] = (void*) pMsg;
				break;
			case ACT_JSON_PASSING:
				CHKiRet(tplToJSON(pAction->ppTpl[i], pMsg, &json, ttNow));
				pElem->staticActParams[i] = (void*) json;
				break;
			default:dbgprintf("software bug/error: unknown pAction->eParamPassing %d in prepareDoActionParams\n",
					   (int) pAction->eParamPassing);
				assert(0); /* software bug if this happens! */
				break;
		}
	}

finalize_it:
	RETiRet;
}


/* free a batches ressources, but not string buffers (because they will
 * most probably be reused). String buffers are only deleted upon final
 * destruction of the batch.
 * This function here must be called only when the batch is actually no
 * longer used, also not for retrying actions or such. It invalidates
 * buffers.
 * rgerhards, 2010-12-17
 */
static rsRetVal releaseBatch(action_t *pAction, batch_t *pBatch)
{
	int jArr;
	int i, j;
	batch_obj_t *pElem;
	uchar ***ppMsgs;
	DEFiRet;

	ASSERT(pAction != NULL);

	for(i = 0 ; i < batchNumMsgs(pBatch) && !*(pBatch->pbShutdownImmediate) ; ++i) {
		pElem = &(pBatch->pElem[i]);
		if(batchIsValidElem(pBatch, i)) {
			switch(pAction->eParamPassing) {
			case ACT_ARRAY_PASSING:
				ppMsgs = (uchar***) pElem->staticActParams;
				for(j = 0 ; j < pAction->iNumTpls ; ++j) {
					if(((uchar**)ppMsgs)[j] != NULL) {
						jArr = 0;
						while(ppMsgs[j][jArr] != NULL) {
							d_free(ppMsgs[j][jArr]);
							ppMsgs[j][jArr] = NULL;
							++jArr;
						}
						d_free(((uchar**)ppMsgs)[j]);
						((uchar**)ppMsgs)[j] = NULL;
					}
				}
				break;
			case ACT_STRING_PASSING:
			case ACT_MSG_PASSING:
				/* nothing to do in that case */
				/* TODO ... and yet we do something ;) This is considered not
				 * really needed, but I was not bold enough to remove that while
				 * fixing the stable. It should be removed in a devel version
				 * soon (I really don't see a reason why we would need it).
				 * rgerhards, 2010-12-16
				 */
				for(j = 0 ; j < pAction->iNumTpls ; ++j) {
					((uchar**)pElem->staticActParams)[j] = NULL;
				}
				break;
			case ACT_JSON_PASSING:
				for(j = 0 ; j < pAction->iNumTpls ; ++j) {
					json_object_put((struct json_object*)
							pElem->staticActParams[j]);
					pElem->staticActParams[j] = NULL;
				}
				break;
			}
		}
	}

	RETiRet;
}


/* call the DoAction output plugin entry point
 * rgerhards, 2008-01-28
 */
rsRetVal
actionCallDoAction(action_t *pThis, msg_t *pMsg, void *actParams)
{
	DEFiRet;

	ASSERT(pThis != NULL);
	ISOBJ_TYPE_assert(pMsg, msg);

	DBGPRINTF("entering actionCalldoAction(), state: %s\n", getActStateName(pThis));

	pThis->bHadAutoCommit = 0;
	iRet = pThis->pMod->mod.om.doAction(actParams, pMsg->msgFlags, pThis->pModData);
	switch(iRet) {
		case RS_RET_OK:
			actionCommitted(pThis);
			pThis->iResumeOKinRow = 0; /* we had a successful call! */
			break;
		case RS_RET_DEFER_COMMIT:
			pThis->iResumeOKinRow = 0; /* we had a successful call! */
			/* we are done, action state remains the same */
			break;
		case RS_RET_PREVIOUS_COMMITTED:
			/* action state remains the same, but we had a commit. */
			pThis->bHadAutoCommit = 1;
			pThis->iResumeOKinRow = 0; /* we had a successful call! */
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
	RETiRet;
}


/* process a message
 * this readies the action and then calls doAction()
 * rgerhards, 2008-01-28
 */
static inline rsRetVal
actionProcessMessage(action_t *pThis, msg_t *pMsg, void *actParams, int *pbShutdownImmediate)
{
	DEFiRet;

	ASSERT(pThis != NULL);
	ISOBJ_TYPE_assert(pMsg, msg);

	CHKiRet(actionPrepare(pThis, pbShutdownImmediate));
	if(pThis->eState == ACT_STATE_ITX)
		CHKiRet(actionCallDoAction(pThis, pMsg, actParams));

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

	if(pThis->eState == ACT_STATE_RDY) {
		/* we just need to flag the batch as commited */
		FINALIZE; /* nothing to do */
	}

	CHKiRet(actionPrepare(pThis, pBatch->pbShutdownImmediate));
	if(pThis->eState == ACT_STATE_ITX) {
		iRet = pThis->pMod->mod.om.endTransaction(pThis->pModData);
		switch(iRet) {
			case RS_RET_OK:
				actionCommitted(pThis);
				/* flag messages as committed */
				for(i = 0 ; i < pBatch->nElem ; ++i) {
					batchSetElemState(pBatch, i, BATCH_STATE_COMM);
					pBatch->pElem[i].bPrevWasSuspended = 0; /* we had success! */
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
static inline rsRetVal
tryDoAction(action_t *pAction, batch_t *pBatch, int *pnElem)
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
	DBGPRINTF("tryDoAction %p, pnElem %d, nElem %d\n", pAction, *pnElem, pBatch->nElem);
	while(iElemProcessed <= *pnElem && i < pBatch->nElem) {
		if(*(pBatch->pbShutdownImmediate))
			ABORT_FINALIZE(RS_RET_FORCE_TERM);
		/* NOTE: do NOT extend the filter below! Anything else must be done on the
		 * enq side of the queue (see file header comment)! -- rgerhards, 2011-06-15
		 */
		if(batchIsValidElem(pBatch, i)) {
			pMsg = pBatch->pElem[i].pMsg;
			localRet = actionProcessMessage(pAction, pMsg, pBatch->pElem[i].staticActParams,
							pBatch->pbShutdownImmediate);
			DBGPRINTF("action %p call returned %d\n", pAction, localRet);
			/* Note: we directly modify the batch object state, because we know that
			 * wo do not overwrite BATCH_STATE_DISC indicators!
			 */
			if(localRet == RS_RET_OK) {
				/* mark messages as committed */
				while(iCommittedUpTo <= i) {
					pBatch->pElem[iCommittedUpTo].bPrevWasSuspended = 0; /* we had success! */
					batchSetElemState(pBatch, iCommittedUpTo, BATCH_STATE_COMM);
					++iCommittedUpTo;
					//pBatch->pElem[iCommittedUpTo++].state = BATCH_STATE_COMM;
				}
			} else if(localRet == RS_RET_PREVIOUS_COMMITTED) {
				/* mark messages as committed */
				while(iCommittedUpTo < i) {
					pBatch->pElem[iCommittedUpTo].bPrevWasSuspended = 0; /* we had success! */
					batchSetElemState(pBatch, iCommittedUpTo, BATCH_STATE_COMM);
					++iCommittedUpTo;
					//pBatch->pElem[iCommittedUpTo++].state = BATCH_STATE_COMM;
				}
				pBatch->pElem[i].state = BATCH_STATE_SUB;
			} else if(localRet == RS_RET_DEFER_COMMIT) {
				pBatch->pElem[i].state = BATCH_STATE_SUB;
			} else if(localRet == RS_RET_DISCARDMSG) {
				pBatch->pElem[i].state = BATCH_STATE_DISC;
			} else {
				dbgprintf("tryDoAction: unexpected error code %d[nElem %d, Commited UpTo %d], finalizing\n",
					  localRet, *pnElem, iCommittedUpTo);
				iRet = localRet;
				FINALIZE;
			}
		}
		++i;
		++iElemProcessed;
	}

finalize_it:
	if(pBatch->iDoneUpTo != iCommittedUpTo) {
		pBatch->iDoneUpTo = iCommittedUpTo;
	}
	RETiRet;
}

/* submit a batch for actual action processing.
 * The first nElem elements are processed. This function calls itself
 * recursively if it needs to handle errors.
 * Note: we don't need the number of the first message to be processed as a parameter,
 * because this is kept track of inside the batch itself (iDoneUpTo).
 * rgerhards, 2009-05-12
 */
static rsRetVal
submitBatch(action_t *pAction, batch_t *pBatch, int nElem)
{
	int i;
	int bDone;
	rsRetVal localRet;
	int wasDoneTo;
	DEFiRet;

	assert(pBatch != NULL);

	wasDoneTo = pBatch->iDoneUpTo;
	bDone = 0;
	do {
		localRet = tryDoAction(pAction, pBatch, &nElem);
		if(localRet == RS_RET_FORCE_TERM) {
			ABORT_FINALIZE(RS_RET_FORCE_TERM);
		}
		if(   localRet == RS_RET_OK
		   || localRet == RS_RET_PREVIOUS_COMMITTED
		   || localRet == RS_RET_DEFER_COMMIT) {
			/* try commit transaction, once done, we can simply do so as if
			 * that return state was returned from tryDoAction().
			 */
			localRet = finishBatch(pAction, pBatch);
		}

		if(   localRet == RS_RET_OK
		   || localRet == RS_RET_PREVIOUS_COMMITTED
		   || localRet == RS_RET_DEFER_COMMIT) {
			bDone = 1;
		} else if(localRet == RS_RET_SUSPENDED) {
			; /* do nothing, this will retry the full batch */
		} else if(localRet == RS_RET_ACTION_FAILED) {
			/* in this case, everything not yet committed is BAD */
			for(i = pBatch->iDoneUpTo ; i < wasDoneTo + nElem ; ++i) {
				if(   pBatch->pElem[i].state != BATCH_STATE_DISC
				   && pBatch->pElem[i].state != BATCH_STATE_COMM ) {
					pBatch->pElem[i].state = BATCH_STATE_BAD;
					pBatch->pElem[i].bPrevWasSuspended = 1;
					STATSCOUNTER_INC(pAction->ctrFail, pAction->mutCtrFail);
				}
			}
			bDone = 1;
		} else {
			if(nElem == 1) {
				batchSetElemState(pBatch, pBatch->iDoneUpTo, BATCH_STATE_BAD);
				bDone = 1;
			} else {
				/* retry with half as much. Depth is log_2 batchsize, so recursion is not too deep */
				DBGPRINTF("submitBatch recursing trying to find and exclude the culprit "
				          "for iRet %d\n", localRet);
				submitBatch(pAction, pBatch, nElem / 2);
				submitBatch(pAction, pBatch, nElem - (nElem / 2));
				bDone = 1;
			}
		}
	} while(!bDone && !*(pBatch->pbShutdownImmediate)); /* do .. while()! */

	if(*(pBatch->pbShutdownImmediate))
		ABORT_FINALIZE(RS_RET_FORCE_TERM);

finalize_it:
	RETiRet;
}


/* copy "active" array of batch, as we need to modify it. The caller
 * must make sure the new array is freed and the orginal batch
 * pointer is restored (thus the caller must save it). If active
 * is currently NULL, this is properly handled.
 * Note: the batches active pointer is modified, so it must be
 * saved BEFORE calling this function!
 * rgerhards, 2012-09-12
 */
static rsRetVal
copyActive(batch_t *pBatch)
{
	sbool *active;
	DEFiRet;

	CHKmalloc(active = malloc(batchNumMsgs(pBatch) * sizeof(sbool)));
	if(pBatch->active == NULL)
		memset(active, 1, batchNumMsgs(pBatch));
	else
		memcpy(active, pBatch->active, batchNumMsgs(pBatch));
	pBatch->active = active;
finalize_it:
	RETiRet;
}

/* The following function prepares a batch for processing, that it is
 * reinitializes batch states, generates strings and does everything else
 * that needs to be done in order to make the batch ready for submission to
 * the actual output module. Note that we look at the precomputed
 * filter OK condition and process only those messages, that actually matched
 * the filter.
 * rgerhards, 2010-06-14
 */
static inline rsRetVal
prepareBatch(action_t *pAction, batch_t *pBatch, sbool **activeSave, int *bMustRestoreActivePtr)
{
	int i;
	batch_obj_t *pElem;
	struct syslogTime ttNow;
	DEFiRet;

	/* indicate we have not yet read the date */
	ttNow.year = 0;

	pBatch->iDoneUpTo = 0;
	for(i = 0 ; i < batchNumMsgs(pBatch) && !*(pBatch->pbShutdownImmediate) ; ++i) {
		pElem = &(pBatch->pElem[i]);
		if(batchIsValidElem(pBatch, i)) {
			pElem->state = BATCH_STATE_RDY;
			if(prepareDoActionParams(pAction, pElem, &ttNow) != RS_RET_OK) {
				/* make sure we have our copy of "active" array */
				if(!*bMustRestoreActivePtr) {
					*activeSave = pBatch->active;
					copyActive(pBatch);
				}
				pBatch->active[i] = RSFALSE;
			}
		}
	}
	RETiRet;
}


/* receive a batch and process it. This includes retry handling.
 * rgerhards, 2009-05-12
 */
static inline rsRetVal
processAction(action_t *pAction, batch_t *pBatch)
{
	DEFiRet;

	assert(pBatch != NULL);
	CHKiRet(submitBatch(pAction, pBatch, pBatch->nElem));
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
	int *pbShutdownImmdtSave;
	sbool *activeSave;
	int bMustRestoreActivePtr = 0;
	rsRetVal localRet;
	DEFiRet;

	assert(pBatch != NULL);

	pbShutdownImmdtSave = pBatch->pbShutdownImmediate;
	pBatch->pbShutdownImmediate = pbShutdownImmediate;
	CHKiRet(prepareBatch(pAction, pBatch, &activeSave, &bMustRestoreActivePtr));

	/* We now must guard the output module against execution by multiple threads. The
	 * plugin interface specifies that output modules must not be thread-safe (except
	 * if they notify us they are - functionality not yet implemented...).
	 * rgerhards, 2008-01-30
	 */
	d_pthread_mutex_lock(&pAction->mutActExec);
	pthread_cleanup_push(mutexCancelCleanup, &pAction->mutActExec);

	iRet = processAction(pAction, pBatch);

	pthread_cleanup_pop(1); /* unlock mutex */

	/* even if processAction failed, we need to release the batch (else we
	 * have a memory leak). So we do this first, and then check if we need to
	 * return an error code. If so, the code from processAction has priority.
	 * rgerhards, 2010-12-17
	 */
	localRet = releaseBatch(pAction, pBatch);

	if(iRet == RS_RET_OK)
		iRet = localRet;
	
	if(bMustRestoreActivePtr) {
		free(pBatch->active);
		pBatch->active = activeSave;
	}

finalize_it:
	pBatch->pbShutdownImmediate = pbShutdownImmdtSave;
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
		cs.ActionQueType = QUEUETYPE_FIXED_ARRAY;
		DBGPRINTF("action queue type set to FIXED_ARRAY\n");
	} else if (!strcasecmp((char *) pszType, "linkedlist")) {
		cs.ActionQueType = QUEUETYPE_LINKEDLIST;
		DBGPRINTF("action queue type set to LINKEDLIST\n");
	} else if (!strcasecmp((char *) pszType, "disk")) {
		cs.ActionQueType = QUEUETYPE_DISK;
		DBGPRINTF("action queue type set to DISK\n");
	} else if (!strcasecmp((char *) pszType, "direct")) {
		cs.ActionQueType = QUEUETYPE_DIRECT;
		DBGPRINTF("action queue type set to DIRECT (no queueing at all)\n");
	} else {
		errmsg.LogError(0, RS_RET_INVALID_PARAMS, "unknown actionqueue parameter: %s", (char *) pszType);
		iRet = RS_RET_INVALID_PARAMS;
	}
	d_free(pszType); /* no longer needed */

	RETiRet;
}


/* This submits the message to the action queue in case we do NOT need to handle repeat
 * message processing. That case permits us to gain lots of freedom during processing
 * and thus speed. This is also utilized to submit messages in complex case once
 * the complex logic has been applied ;)
 * rgerhards, 2010-06-08
 */
static inline rsRetVal
doSubmitToActionQ(action_t *pAction, msg_t *pMsg)
{
	DEFiRet;

	if(pAction->eState == ACT_STATE_DIED) {
		DBGPRINTF("action %p died, do NOT execute\n", pAction);
		FINALIZE;
	}

	STATSCOUNTER_INC(pAction->ctrProcessed, pAction->mutCtrProcessed);
	if(pAction->pQueue->qType == QUEUETYPE_DIRECT)
		iRet = qqueueEnqMsgDirect(pAction->pQueue, MsgAddRef(pMsg));
	else
		iRet = qqueueEnqMsg(pAction->pQueue, eFLOWCTL_NO_DELAY, MsgAddRef(pMsg));

finalize_it:
	RETiRet;
}


/* This function builds up a batch of messages to be (later)
 * submitted to the action queue.
 * Important: this function MUST not be called with messages that are to
 * be discarded due to their "prevWasSuspended" state. It will not check for
 * this and submit all messages to the queue for execution. So these must
 * be filtered out before calling us (what is done currently!).
 */
rsRetVal
actionWriteToAction(action_t *pAction, msg_t *pMsg)
{
	DEFiRet;

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

	DBGPRINTF("Called action(complex case), logging to %s\n", module.GetStateName(pAction->pMod));

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
		FINALIZE;
	}

	/* we use reception time, not dequeue time - this is considered more appropriate and also faster ;)
	 * rgerhards, 2008-09-17 */
	pAction->tLastExec = getActNow(pAction); /* re-init time flags */
	pAction->f_time = pMsg->ttGenTime;

	/* When we reach this point, we have a valid, non-disabled action.
	 * So let's enqueue our message for execution. -- rgerhards, 2007-07-24
	 */
	iRet = doSubmitToActionQ(pAction, pMsg);

finalize_it:
	RETiRet;
}


/* helper to actonCallAction, mostly needed because of this damn
 * pthread_cleanup_push() POSIX macro...
 */
static inline rsRetVal
doActionCallAction(action_t *pAction, batch_t *pBatch, int idxBtch)
{
	msg_t *pMsg;
	DEFiRet;

	pMsg = pBatch->pElem[idxBtch].pMsg;
	pAction->tActNow = -1; /* we do not yet know our current time (clear prev. value) */

	/* don't output marks to recently written outputs */
	if(pAction->bWriteAllMarkMsgs == RSFALSE
	   && (pMsg->msgFlags & MARK) && (getActNow(pAction) - pAction->f_time) < MarkInterval / 2) {
		ABORT_FINALIZE(RS_RET_OK);
	}

	/* call the output driver */
	iRet = actionWriteToAction(pAction, pMsg);

finalize_it:
	/* we need to update the batch to handle failover processing correctly */
	if(iRet == RS_RET_OK) {
		pBatch->pElem[idxBtch].bPrevWasSuspended = 0;
	} else if(iRet == RS_RET_ACTION_FAILED) {
		pBatch->pElem[idxBtch].bPrevWasSuspended = 1;
	}

	RETiRet;
}


/* helper to activateActions, it activates a specific action.
 */
DEFFUNC_llExecFunc(doActivateActions)
{
	rsRetVal localRet;
	action_t *pThis = (action_t*) pData;
	BEGINfunc
	localRet = qqueueStart(pThis->pQueue);
	if(localRet != RS_RET_OK) {
		errmsg.LogError(0, localRet, "error starting up action queue");
		if(localRet == RS_RET_FILE_PREFIX_MISSING) {
			errmsg.LogError(0, localRet, "file prefix (work directory?) "
					"is missing");
		}
		actionDisable(pThis);
	}
	DBGPRINTF("Action %s[%p]: queue %p started\n", modGetName(pThis->pMod),
		  pThis, pThis->pQueue);
	ENDfunc
	return RS_RET_OK; /* we ignore errors, we can not do anything either way */
}


/* This function "activates" the action after privileges have been dropped. Currently,
 * this means that the queues are started.
 * rgerhards, 2011-05-02
 */
rsRetVal
activateActions(void)
{
	DEFiRet;
	iRet = ruleset.IterateAllActions(ourConf, doActivateActions, NULL);
	RETiRet;
}



/* This submits the message to the action queue in case where we need to handle
 * bWriteAllMarkMessage == RSFALSE only. Note that we use a non-blocking CAS loop
 * for the synchronization. Here, we just modify the filter condition to be false when
 * a mark message must not be written. However, in this case we must save the previous
 * filter as we may need it in the next action (potential future optimization: check if this is
 * the last action TODO).
 * rgerhards, 2010-06-08
 */
static rsRetVal
doSubmitToActionQNotAllMarkBatch(action_t *pAction, batch_t *pBatch)
{
	time_t now = 0;
	time_t lastAct;
	int i;
	sbool *activeSave;
	DEFiRet;

	activeSave = pBatch->active;
	copyActive(pBatch);

	for(i = 0 ; i < batchNumMsgs(pBatch) ; ++i) {
		if((pBatch->pElem[i].state == BATCH_STATE_DISC) || !pBatch->active[i])
			continue;
		if(now == 0) {
			now = datetime.GetTime(NULL); /* good time call - the only one done */
		}
		/* CAS loop, we write back a bit early, but that's OK... */
		/* we use reception time, not dequeue time - this is considered more appropriate and
		 * also faster ;) -- rgerhards, 2008-09-17 */
		do {
			lastAct = pAction->f_time;
			if(pBatch->pElem[i].pMsg->msgFlags & MARK) {
				if((now - lastAct) < MarkInterval / 2) {
					pBatch->active[i] = 0;
					DBGPRINTF("batch item %d: action was recently called, ignoring "
						  "mark message\n", i);
					break; /* do not update timestamp for non-written mark messages */
				}
			}
		} while(ATOMIC_CAS_time_t(&pAction->f_time, lastAct,
			pBatch->pElem[i].pMsg->ttGenTime, &pAction->mutCAS) == 0);
		if(pBatch->active[i]) {
			DBGPRINTF("Called action(NotAllMark), processing batch[%d] via '%s'\n",
				  i, module.GetStateName(pAction->pMod));
		}
	}

	iRet = doSubmitToActionQBatch(pAction, pBatch);

	free(pBatch->active);
	pBatch->active = activeSave;

	RETiRet;
}

static inline void
countStatsBatchEnq(action_t *pAction, batch_t *pBatch)
{
	int i;
	for(i = 0 ; i < batchNumMsgs(pBatch) && !*(pBatch->pbShutdownImmediate) ; ++i) {
		if( batchIsValidElem(pBatch, i)) {
			STATSCOUNTER_INC(pAction->ctrProcessed, pAction->mutCtrProcessed);
		}
	}
}


/* enqueue a batch in direct mode. We have put this into its own function just to avoid
 * cluttering the actual submit function.
 * rgerhards, 2011-06-16
 */
static inline rsRetVal
doQueueEnqObjDirectBatch(action_t *pAction, batch_t *pBatch)
{
	sbool bNeedSubmit;
	sbool *activeSave;
	int i;
	DEFiRet;

	activeSave = pBatch->active;
	copyActive(pBatch);

	/* note: for direct mode, we need to adjust the filter property. For non-direct
	 * this is not necessary, because in that case we enqueue only what actually needs
	 * to be processed.
	 */
	if(pAction->bExecWhenPrevSusp) {
		bNeedSubmit = 0;
		for(i = 0 ; i < batchNumMsgs(pBatch) && !*(pBatch->pbShutdownImmediate) ; ++i) {
			if(!pBatch->pElem[i].bPrevWasSuspended) {
				DBGPRINTF("action enq stage: change active to 0 due to "
					  "failover case in elem %d\n", i);
				pBatch->active[i] = 0;
			}
			if(batchIsValidElem(pBatch, i)) {
				STATSCOUNTER_INC(pAction->ctrProcessed, pAction->mutCtrProcessed);
				bNeedSubmit = 1;
			}
			DBGPRINTF("action %p[%d]: valid:%d state:%d execWhenPrev:%d prevWasSusp:%d\n",
				   pAction, i, batchIsValidElem(pBatch, i),  pBatch->pElem[i].state,
				   pAction->bExecWhenPrevSusp, pBatch->pElem[i].bPrevWasSuspended);
		}
		if(bNeedSubmit) {
			/* note: stats were already computed above */
			iRet = qqueueEnqObjDirectBatch(pAction->pQueue, pBatch);
		} else {
			DBGPRINTF("no need to submit batch, all invalid\n");
		}
	} else {
		if(GatherStats)
			countStatsBatchEnq(pAction, pBatch);
		iRet = qqueueEnqObjDirectBatch(pAction->pQueue, pBatch);
	}

	free(pBatch->active);
	pBatch->active = activeSave;
	RETiRet;
}

/* This submits the message to the action queue in case we do NOT need to handle repeat
 * message processing. That case permits us to gain lots of freedom during processing
 * and thus speed.
 * rgerhards, 2010-06-08
 */
static rsRetVal
doSubmitToActionQBatch(action_t *pAction, batch_t *pBatch)
{
	int i;
	DEFiRet;

	DBGPRINTF("Called action(Batch), logging to %s\n", module.GetStateName(pAction->pMod));

	if(pAction->pQueue->qType == QUEUETYPE_DIRECT) {
		iRet = doQueueEnqObjDirectBatch(pAction, pBatch);
	} else {/* in this case, we do single submits to the queue. 
		 * TODO: optimize this, we may do at least a multi-submit!
		 */
		for(i = 0 ; i < batchNumMsgs(pBatch) && !*(pBatch->pbShutdownImmediate) ; ++i) {
			DBGPRINTF("action %p: valid:%d state:%d execWhenPrev:%d prevWasSusp:%d\n",
		           pAction, batchIsValidElem(pBatch, i),  pBatch->pElem[i].state,
			   pAction->bExecWhenPrevSusp, pBatch->pElem[i].bPrevWasSuspended);
			if(   batchIsValidElem(pBatch, i) 
			   && (pAction->bExecWhenPrevSusp == 0 || pBatch->pElem[i].bPrevWasSuspended == 1)) {
				doSubmitToActionQ(pAction, pBatch->pElem[i].pMsg);
			}
		}
	}

	RETiRet;
}



/* Helper to submit a batch of actions to the engine. Note that we have rather
 * complicated processing here, so we need to do this one message after another.
 * rgerhards, 2010-06-23
 */
static inline rsRetVal
helperSubmitToActionQComplexBatch(action_t *pAction, batch_t *pBatch)
{
	int i;
	DEFiRet;

	DBGPRINTF("Called action %p (complex case), logging to %s\n",
		  pAction, module.GetStateName(pAction->pMod));
	for(i = 0 ; i < batchNumMsgs(pBatch) && !*(pBatch->pbShutdownImmediate) ; ++i) {
		DBGPRINTF("action %p: valid:%d state:%d execWhenPrev:%d prevWasSusp:%d\n",
		           pAction, batchIsValidElem(pBatch, i),  pBatch->pElem[i].state,
			   pAction->bExecWhenPrevSusp, pBatch->pElem[i].bPrevWasSuspended);
		if(   batchIsValidElem(pBatch, i)
		   && ((pAction->bExecWhenPrevSusp  == 0) || pBatch->pElem[i].bPrevWasSuspended) ) {
			doActionCallAction(pAction, pBatch, i);
		}
	}

	RETiRet;
}

/* Call configured action, most complex case with all features supported (and thus slow).
 * rgerhards, 2010-06-08
 */
#pragma GCC diagnostic ignored "-Wempty-body"
static rsRetVal
doSubmitToActionQComplexBatch(action_t *pAction, batch_t *pBatch)
{
	DEFiRet;

	d_pthread_mutex_lock(&pAction->mutAction);
	pthread_cleanup_push(mutexCancelCleanup, &pAction->mutAction);
	iRet = helperSubmitToActionQComplexBatch(pAction, pBatch);
	d_pthread_mutex_unlock(&pAction->mutAction);
	pthread_cleanup_pop(0); /* remove mutex cleanup handler */

	RETiRet;
}
#pragma GCC diagnostic warning "-Wempty-body"


/* apply all params from param block to action. This supports the v6 config system.
 * Defaults must have been set appropriately during action construct!
 * rgerhards, 2011-08-01
 */
static rsRetVal
actionApplyCnfParam(action_t *pAction, struct cnfparamvals *pvals)
{
	int i;
	
	for(i = 0 ; i < pblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(pblk.descr[i].name, "name")) {
			pAction->pszName = (uchar*) es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(pblk.descr[i].name, "type")) {
			continue; /* this is handled seperately during module select! */
		} else if(!strcmp(pblk.descr[i].name, "action.writeallmarkmessages")) {
			pAction->bWriteAllMarkMsgs = pvals[i].val.d.n;
		} else if(!strcmp(pblk.descr[i].name, "action.execonlyeverynthtime")) {
			pAction->iExecEveryNthOccur = pvals[i].val.d.n;
		} else if(!strcmp(pblk.descr[i].name, "action.execonlyeverynthtimetimeout")) {
			pAction->iExecEveryNthOccurTO = pvals[i].val.d.n;
		} else if(!strcmp(pblk.descr[i].name, "action.execonlyonceeveryinterval")) {
			pAction->iSecsExecOnceInterval = pvals[i].val.d.n;
		} else if(!strcmp(pblk.descr[i].name, "action.execonlywhenpreviousissuspended")) {
			pAction->bExecWhenPrevSusp = pvals[i].val.d.n;
		} else if(!strcmp(pblk.descr[i].name, "action.repeatedmsgcontainsoriginalmsg")) {
			pAction->bRepMsgHasMsg = pvals[i].val.d.n;
		} else if(!strcmp(pblk.descr[i].name, "action.resumeretrycount")) {
			pAction->iResumeRetryCount = pvals[i].val.d.n;
		} else if(!strcmp(pblk.descr[i].name, "action.resumeinterval")) {
			pAction->iResumeInterval = pvals[i].val.d.n;
		} else {
			dbgprintf("action: program error, non-handled "
			  "param '%s'\n", pblk.descr[i].name);
		}
	}
	return RS_RET_OK;
}


/* add an Action to the current selector
 * The pOMSR is freed, as it is not needed after this function.
 * Note: this function pulls global data that specifies action config state.
 * rgerhards, 2007-07-27
 */
rsRetVal
addAction(action_t **ppAction, modInfo_t *pMod, void *pModData,
	  omodStringRequest_t *pOMSR, struct cnfparamvals *actParams,
	  struct cnfparamvals *queueParams, int bSuspended)
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
	DBGPRINTF("Module %s processes this action.\n", module.GetName(pMod));

	CHKiRet(actionConstruct(&pAction)); /* create action object first */
	pAction->pMod = pMod;
	pAction->pModData = pModData;
	if(actParams == NULL) { /* use legacy systemn */
		pAction->pszName = cs.pszActionName;
		pAction->iResumeInterval = cs.glbliActionResumeInterval;
		pAction->iResumeRetryCount = cs.glbliActionResumeRetryCount;
		pAction->bWriteAllMarkMsgs = cs.bActionWriteAllMarkMsgs;
		pAction->bExecWhenPrevSusp = cs.bActExecWhenPrevSusp;
		pAction->iSecsExecOnceInterval = cs.iActExecOnceInterval;
		pAction->iExecEveryNthOccur = cs.iActExecEveryNthOccur;
		pAction->iExecEveryNthOccurTO = cs.iActExecEveryNthOccurTO;
		pAction->bRepMsgHasMsg = cs.bActionRepMsgHasMsg;
		cs.iActExecEveryNthOccur = 0; /* auto-reset */
		cs.iActExecEveryNthOccurTO = 0; /* auto-reset */
		cs.bActionWriteAllMarkMsgs = RSFALSE; /* auto-reset */
		cs.pszActionName = NULL;	/* free again! */
	} else {
		actionApplyCnfParam(pAction, actParams);
	}

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
	}
	
	for(i = 0 ; i < pAction->iNumTpls ; ++i) {
		CHKiRet(OMSRgetEntry(pOMSR, i, &pTplName, &iTplOpts));
		/* Ok, we got everything, so it now is time to look up the template
		 * (Hint: templates MUST be defined before they are used!)
		 */
		if(   !(iTplOpts & OMSR_TPL_AS_MSG)
		   && (pAction->ppTpl[i] =
		   	tplFind(ourConf, (char*)pTplName, strlen((char*)pTplName))) == NULL) {
			snprintf(errMsg, sizeof(errMsg) / sizeof(char),
				 " Could not find template '%s' - action disabled",
				 pTplName);
			errno = 0;
			errmsg.LogError(0, RS_RET_NOT_FOUND, "%s", errMsg);
			ABORT_FINALIZE(RS_RET_NOT_FOUND);
		}
		/* check required template options */
		if(   (iTplOpts & OMSR_RQD_TPL_OPT_SQL)
		   && (pAction->ppTpl[i]->optFormatEscape == 0)) {
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
		} else if(iTplOpts & OMSR_TPL_AS_JSON) {
			pAction->eParamPassing = ACT_JSON_PASSING;
		} else {
			pAction->eParamPassing = ACT_STRING_PASSING;
		}

		DBGPRINTF("template: '%s' assigned\n", pTplName);
	}

	pAction->pMod = pMod;
	pAction->pModData = pModData;
	/* check if the module is compatible with select features (currently no such features exist) */
	pAction->eState = ACT_STATE_RDY; /* action is enabled */

	if(bSuspended)
		actionSuspend(pAction, datetime.GetTime(NULL)); /* "good" time call, only during init and unavoidable */

	CHKiRet(actionConstructFinalize(pAction, queueParams));
	
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
	cs.iActExecOnceInterval = 0;
	cs.bActExecWhenPrevSusp = 0;
	return RS_RET_OK;
}


/* initialize (current) config variables.
 * Used at program start and when a new scope is created.
 */
static inline void
initConfigVariables(void)
{
	cs.bActionWriteAllMarkMsgs = RSFALSE;
	cs.glbliActionResumeRetryCount = 0;
	cs.bActExecWhenPrevSusp = 0;
	cs.iActExecOnceInterval = 0;
	cs.iActExecEveryNthOccur = 0;
	cs.iActExecEveryNthOccurTO = 0;
	cs.glbliActionResumeInterval = 30;
	cs.glbliActionResumeRetryCount = 0;
	cs.bActionRepMsgHasMsg = 0;
	if(cs.pszActionName != NULL) {
		free(cs.pszActionName);
		cs.pszActionName = NULL;
	}
	actionResetQueueParams();
}


rsRetVal
actionNewInst(struct nvlst *lst, action_t **ppAction)
{
	struct cnfparamvals *paramvals;
	struct cnfparamvals *queueParams;
	modInfo_t *pMod;
	uchar *cnfModName = NULL;
	omodStringRequest_t *pOMSR;
	void *pModData;
	action_t *pAction;
	int typeIdx;
	DEFiRet;

	paramvals = nvlstGetParams(lst, &pblk, NULL);
	if(paramvals == NULL) {
		ABORT_FINALIZE(RS_RET_ERR);
	}
	dbgprintf("action param blk after actionNewInst:\n");
	cnfparamsPrint(&pblk, paramvals);
	typeIdx = cnfparamGetIdx(&pblk, "type");
	if(paramvals[typeIdx].bUsed == 0) {
		errmsg.LogError(0, RS_RET_CONF_RQRD_PARAM_MISSING, "action type missing");
		ABORT_FINALIZE(RS_RET_CONF_RQRD_PARAM_MISSING); // TODO: move this into rainerscript handlers
	}
	cnfModName = (uchar*)es_str2cstr(paramvals[cnfparamGetIdx(&pblk, ("type"))].val.d.estr, NULL);
	if((pMod = module.FindWithCnfName(loadConf, cnfModName, eMOD_OUT)) == NULL) {
		errmsg.LogError(0, RS_RET_MOD_UNKNOWN, "module name '%s' is unknown", cnfModName);
		ABORT_FINALIZE(RS_RET_MOD_UNKNOWN);
	}
	iRet = pMod->mod.om.newActInst(cnfModName, lst, &pModData, &pOMSR);
	// TODO: check if RS_RET_SUSPENDED is still valid in v6!
	if(iRet != RS_RET_OK && iRet != RS_RET_SUSPENDED) {
		FINALIZE; /* iRet is already set to error state */
	}

	qqueueDoCnfParams(lst, &queueParams);

	if((iRet = addAction(&pAction, pMod, pModData, pOMSR, paramvals, queueParams,
	                    (iRet == RS_RET_SUSPENDED)? 1 : 0)) == RS_RET_OK) {
		/* check if the module is compatible with select features
		 * (currently no such features exist) */
		pAction->eState = ACT_STATE_RDY; /* action is enabled */
		loadConf->actions.nbrActions++;	/* one more active action! */
	}
	*ppAction = pAction;

finalize_it:
	free(cnfModName);
	cnfparamvalsDestruct(paramvals, &pblk);
	RETiRet;
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
	CHKiRet(objUse(statsobj, CORE_COMPONENT));
	CHKiRet(objUse(ruleset, CORE_COMPONENT));

	CHKiRet(regCfSysLineHdlr((uchar *)"actionname", 0, eCmdHdlrGetWord, NULL, &cs.pszActionName, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuefilename", 0, eCmdHdlrGetWord, NULL, &cs.pszActionQFName, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuesize", 0, eCmdHdlrInt, NULL, &cs.iActionQueueSize, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionwriteallmarkmessages", 0, eCmdHdlrBinary, NULL, &cs.bActionWriteAllMarkMsgs, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuedequeuebatchsize", 0, eCmdHdlrInt, NULL, &cs.iActionQueueDeqBatchSize, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuemaxdiskspace", 0, eCmdHdlrSize, NULL, &cs.iActionQueMaxDiskSpace, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuehighwatermark", 0, eCmdHdlrInt, NULL, &cs.iActionQHighWtrMark, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuelowwatermark", 0, eCmdHdlrInt, NULL, &cs.iActionQLowWtrMark, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuediscardmark", 0, eCmdHdlrInt, NULL, &cs.iActionQDiscardMark, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuediscardseverity", 0, eCmdHdlrInt, NULL, &cs.iActionQDiscardSeverity, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuecheckpointinterval", 0, eCmdHdlrInt, NULL, &cs.iActionQPersistUpdCnt, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuesyncqueuefiles", 0, eCmdHdlrBinary, NULL, &cs.bActionQSyncQeueFiles, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuetype", 0, eCmdHdlrGetWord, setActionQueType, NULL, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueueworkerthreads", 0, eCmdHdlrInt, NULL, &cs.iActionQueueNumWorkers, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuetimeoutshutdown", 0, eCmdHdlrInt, NULL, &cs.iActionQtoQShutdown, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuetimeoutactioncompletion", 0, eCmdHdlrInt, NULL, &cs.iActionQtoActShutdown, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuetimeoutenqueue", 0, eCmdHdlrInt, NULL, &cs.iActionQtoEnq, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueueworkertimeoutthreadshutdown", 0, eCmdHdlrInt, NULL, &cs.iActionQtoWrkShutdown, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueueworkerthreadminimummessages", 0, eCmdHdlrInt, NULL, &cs.iActionQWrkMinMsgs, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuemaxfilesize", 0, eCmdHdlrSize, NULL, &cs.iActionQueMaxFileSize, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuesaveonshutdown", 0, eCmdHdlrBinary, NULL, &cs.bActionQSaveOnShutdown, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuedequeueslowdown", 0, eCmdHdlrInt, NULL, &cs.iActionQueueDeqSlowdown, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuedequeuetimebegin", 0, eCmdHdlrInt, NULL, &cs.iActionQueueDeqtWinFromHr, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuedequeuetimeend", 0, eCmdHdlrInt, NULL, &cs.iActionQueueDeqtWinToHr, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionexeconlyeverynthtime", 0, eCmdHdlrInt, NULL, &cs.iActExecEveryNthOccur, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionexeconlyeverynthtimetimeout", 0, eCmdHdlrInt, NULL, &cs.iActExecEveryNthOccurTO, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionexeconlyonceeveryinterval", 0, eCmdHdlrInt, NULL, &cs.iActExecOnceInterval, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"repeatedmsgcontainsoriginalmsg", 0, eCmdHdlrBinary, NULL, &cs.bActionRepMsgHasMsg, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionexeconlywhenpreviousissuspended", 0, eCmdHdlrBinary, NULL, &cs.bActExecWhenPrevSusp, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionresumeretrycount", 0, eCmdHdlrInt, NULL, &cs.glbliActionResumeRetryCount, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL, NULL));

	initConfigVariables(); /* first-time init of config setings */

finalize_it:
	RETiRet;
}

/* vi:set ai:
 */
