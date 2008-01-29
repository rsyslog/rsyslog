/* action.c
 *
 * Implementation of the action object.
 *
 * File begun on 2007-08-06 by RGerhards (extracted from syslogd.c)
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
#include "rsyslog.h"
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "syslogd.h"
#include "template.h"
#include "action.h"
#include "modules.h"
#include "sync.h"
#include "cfsysline.h"
#include "srUtils.h"


/* forward definitions */
rsRetVal actionCallDoAction(action_t *pAction, msg_t *pMsg);

/* object static data (once for all instances) */
static int glbliActionResumeInterval = 30;
int glbliActionResumeRetryCount = 0;		/* how often should suspended actions be retried? */

/* main message queue and its configuration parameters */
static queueType_t ActionQueType = QUEUETYPE_DIRECT;		/* type of the main message queue above */
static int iActionQueueSize = 10000;				/* size of the main message queue above */
static int iActionQHighWtrMark = 8000;				/* high water mark for disk-assisted queues */
static int iActionQLowWtrMark = 2000;				/* low water mark for disk-assisted queues */
static int iActionQDiscardMark = 9800;				/* begin to discard messages */
static int iActionQDiscardSeverity = 4;				/* discard warning and above */
static int iActionQueueNumWorkers = 1;				/* number of worker threads for the mm queue above */
static uchar *pszActionQFName = NULL;				/* prefix for the main message queue file */
static size_t iActionQueMaxFileSize = 1024*1024;
static int iActionQPersistUpdCnt = 0;				/* persist queue info every n updates */
static int iActionQtoQShutdown = 0;				/* queue shutdown */ 
static int iActionQtoActShutdown = 1000;			/* action shutdown (in phase 2) */ 
static int iActionQtoEnq = 2000;				/* timeout for queue enque */ 
static int iActionQtoWrkShutdown = 60000;			/* timeout for worker thread shutdown */
static int iActionQWrkMinMsgs = 100;				/* minimum messages per worker needed to start a new one */
static int bActionQSaveOnShutdown = 1;				/* save queue on shutdown (when DA enabled)? */

/* destructs an action descriptor object
 * rgerhards, 2007-08-01
 */
rsRetVal actionDestruct(action_t *pThis)
{
	ASSERT(pThis != NULL);

	if(pThis->pMod != NULL)
		pThis->pMod->freeInstance(pThis->pModData);

	if(pThis->f_pMsg != NULL)
		MsgDestruct(&pThis->f_pMsg);

	SYNC_OBJ_TOOL_EXIT(pThis);
	if(pThis->ppTpl != NULL)
		free(pThis->ppTpl);
	if(pThis->ppMsgs != NULL)
		free(pThis->ppMsgs);
	free(pThis);
	
	return RS_RET_OK;
}


/* create a new action descriptor object
 * rgerhards, 2007-08-01
 */
rsRetVal actionConstruct(action_t **ppThis)
{
	DEFiRet;
	action_t *pThis;

	ASSERT(ppThis != NULL);
	
	if((pThis = (action_t*) calloc(1, sizeof(action_t))) == NULL) {
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}

	pThis->iResumeInterval = glbliActionResumeInterval;
	pThis->iResumeRetryCount = glbliActionResumeRetryCount;
	SYNC_OBJ_TOOL_INIT(pThis);

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

	ASSERT(pThis != NULL);

	/* create queue */
RUNLOG_VAR("%d", ActionQueType);
	CHKiRet(queueConstruct(&pThis->pQueue, ActionQueType, 1, 10, (rsRetVal (*)(void*,void*))actionCallDoAction));


	/* ... set some properties ... */
#	define setQPROP(func, directive, data) \
	CHKiRet_Hdlr(func(pThis->pQueue, data)) { \
		logerrorInt("Invalid " #directive ", error %d. Ignored, running with default setting", iRet); \
	}
#	define setQPROPstr(func, directive, data) \
	CHKiRet_Hdlr(func(pThis->pQueue, data, (data == NULL)? 0 : strlen((char*) data))) { \
		logerrorInt("Invalid " #directive ", error %d. Ignored, running with default setting", iRet); \
	}

	queueSetpUsr(pThis->pQueue, pThis);
	setQPROP(queueSetMaxFileSize, "$ActionQueueFileSize", iActionQueMaxFileSize);
	setQPROPstr(queueSetFilePrefix, "$ActionQueueFileName", pszActionQFName);
	setQPROP(queueSetiPersistUpdCnt, "$ActionQueueCheckpointInterval", iActionQPersistUpdCnt);
	setQPROP(queueSettoQShutdown, "$ActionQueueTimeoutShutdown", iActionQtoQShutdown );
	setQPROP(queueSettoActShutdown, "$ActionQueueTimeoutActionCompletion", iActionQtoActShutdown);
	setQPROP(queueSettoWrkShutdown, "$ActionQueueTimeoutWorkerThreadShutdown", iActionQtoWrkShutdown);
	setQPROP(queueSettoEnq, "$ActionQueueTimeoutEnqueue", iActionQtoEnq);
	setQPROP(queueSetiHighWtrMrk, "$ActionQueueHighWaterMark", iActionQHighWtrMark);
	setQPROP(queueSetiLowWtrMrk, "$ActionQueueLowWaterMark", iActionQLowWtrMark);
	setQPROP(queueSetiDiscardMrk, "$ActionQueueDiscardMark", iActionQDiscardMark);
	setQPROP(queueSetiDiscardSeverity, "$ActionQueueDiscardSeverity", iActionQDiscardSeverity);
	setQPROP(queueSetiMinMsgsPerWrkr, "$ActionQueueWorkerThreadMinimumMessages", iActionQWrkMinMsgs);
	setQPROP(queueSetbSaveOnShutdown, "$ActionQueueSaveOnShutdown", bActionQSaveOnShutdown);

#	undef setQPROP
#	undef setQPROPstr

	CHKiRet(queueStart(pThis->pQueue));
	dbgprintf("Action %p: queue %p created\n", pThis, pThis->pQueue);

finalize_it:
	RETiRet;
}


/* set an action back to active state -- rgerhards, 2007-08-02
 */
static rsRetVal actionResume(action_t *pThis)
{
	DEFiRet;

	ASSERT(pThis != NULL);
	pThis->bSuspended = 0;

	RETiRet;
}


/* set the global resume interval
 */
rsRetVal actionSetGlobalResumeInterval(int iNewVal)
{
	glbliActionResumeInterval = iNewVal;
	return RS_RET_OK;
}


/* suspend an action -- rgerhards, 2007-08-02
 */
rsRetVal actionSuspend(action_t *pThis)
{
	DEFiRet;

	ASSERT(pThis != NULL);
	pThis->bSuspended = 1;
	pThis->ttResumeRtry = time(NULL) + pThis->iResumeInterval;
	pThis->iNbrResRtry = 0; /* tell that we did not yet retry to resume */

	RETiRet;
}

/* try to resume an action -- rgerhards, 2007-08-02
 * returns RS_RET_OK if resumption worked, RS_RET_SUSPEND if the
 * action is still suspended.
 */
rsRetVal actionTryResume(action_t *pThis)
{
	DEFiRet;
	time_t ttNow;

	ASSERT(pThis != NULL);

	ttNow = time(NULL); /* do the system call just once */

	/* first check if it is time for a re-try */
	if(ttNow > pThis->ttResumeRtry) {
		iRet = pThis->pMod->tryResume(pThis->pModData);
		if(iRet == RS_RET_SUSPENDED) {
			/* set new tryResume time */
			++pThis->iNbrResRtry;
			/* if we have more than 10 retries, we prolong the
			 * retry interval. If something is really stalled, it will
			 * get re-tried only very, very seldom - but that saves
			 * CPU time. TODO: maybe a config option for that?
			 * rgerhards, 2007-08-02
			 */
			pThis->ttResumeRtry = ttNow + pThis->iResumeInterval * (pThis->iNbrResRtry / 10 + 1);
		}
	} else {
		/* it's too early, we are still suspended --> indicate this */
		iRet = RS_RET_SUSPENDED;
	}

	if(iRet == RS_RET_OK)
		actionResume(pThis);

	dbgprintf("actionTryResume: iRet: %d, next retry (if applicable): %u [now %u]\n",
		iRet, (unsigned) pThis->ttResumeRtry, (unsigned) ttNow);

	RETiRet;
}


/* debug-print the contents of an action object
 * rgerhards, 2007-08-02
 */
rsRetVal actionDbgPrint(action_t *pThis)
{
	DEFiRet;

	printf("%s: ", modGetStateName(pThis->pMod));
	pThis->pMod->dbgPrintInstInfo(pThis->pModData);
	printf("\n\tInstance data: 0x%lx\n", (unsigned long) pThis->pModData);
	printf("\tRepeatedMsgReduction: %d\n", pThis->f_ReduceRepeated);
	printf("\tResume Interval: %d\n", pThis->iResumeInterval);
	printf("\tSuspended: %d", pThis->bSuspended);
	if(pThis->bSuspended) {
		printf(" next retry: %u, number retries: %d", (unsigned) pThis->ttResumeRtry, pThis->iNbrResRtry);
	}
	printf("\n");
	printf("\tDisabled: %d\n", !pThis->bEnabled);
	printf("\tExec only when previous is suspended: %d\n", pThis->bExecWhenPrevSusp);
	printf("\n");

	RETiRet;
}


/* call the DoAction output plugin entry point
 * rgerhards, 2008-01-28
 */
rsRetVal
actionCallDoAction(action_t *pAction, msg_t *pMsg)
{
	DEFiRet;
	int iRetries;
	int i;
	int iSleepPeriod;

	ASSERT(pAction != NULL);

	/* here we must loop to process all requested strings */
	for(i = 0 ; i < pAction->iNumTpls ; ++i) {
		CHKiRet(tplToString(pAction->ppTpl[i], pMsg, &pAction->ppMsgs[i]));
	}

	iRetries = 0;
	do {
RUNLOG_STR("going into do_action call loop");
RUNLOG_VAR("%d", iRetries);
		/* first check if we are suspended and, if so, retry */
		if(actionIsSuspended(pAction)) {
dbgprintf("action %p is suspended\n", pAction);
			iRet = actionTryResume(pAction);
		}

		if(iRet == RS_RET_OK) {
			/* call configured action */
			iRet = pAction->pMod->mod.om.doAction(pAction->ppMsgs, pAction->f_pMsg->msgFlags, pAction->pModData);
		}

RUNLOG_VAR("%d", iRet);
		if(iRet == RS_RET_SUSPENDED) {
			/* ok, this calls for our retry logic... */
			++iRetries;
			iSleepPeriod = pAction->iResumeInterval;
RUNLOG_VAR("%d", iSleepPeriod);
			srSleep(iSleepPeriod, 0);
		} else {
			break; /* we are done in any case */
		}
	} while(pAction->iResumeRetryCount == -1 || iRetries < pAction->iResumeRetryCount); /* do...while! */
RUNLOG_STR("out of retry loop");

	if(iRet == RS_RET_DISABLE_ACTION) {
		dbgprintf("Action requested to be disabled, done that.\n");
		pAction->bEnabled = 0; /* that's it... */
	}

	if(iRet == RS_RET_SUSPENDED) {
		dbgprintf("Action requested to be suspended, done that.\n");
		actionSuspend(pAction);
	}

	if(iRet == RS_RET_OK)
		pAction->f_prevcount = 0; /* message processed, so we start a new cycle */

finalize_it:
	/* cleanup */
	for(i = 0 ; i < pAction->iNumTpls ; ++i) {
		if(pAction->ppMsgs[i] != NULL) {
			free(pAction->ppMsgs[i]);
			pAction->ppMsgs[i] = NULL;
		}
	}
	MsgDestruct(&pMsg); /* we are now finished with the message */

	RETiRet;
}

/* set the action message queue mode
 * TODO: probably move this into queue object, merge with MainMsgQueue!
 * rgerhards, 2008-01-28
 */
static rsRetVal setActionQueType(void __attribute__((unused)) *pVal, uchar *pszType)
{
	DEFiRet;

	if (!strcasecmp((char *) pszType, "fixedarray")) {
		ActionQueType = QUEUETYPE_FIXED_ARRAY;
		dbgprintf("action queue type set to FIXED_ARRAY\n");
	} else if (!strcasecmp((char *) pszType, "linkedlist")) {
		ActionQueType = QUEUETYPE_LINKEDLIST;
		dbgprintf("action queue type set to LINKEDLIST\n");
	} else if (!strcasecmp((char *) pszType, "disk")) {
		ActionQueType = QUEUETYPE_DISK;
		dbgprintf("action queue type set to DISK\n");
	} else if (!strcasecmp((char *) pszType, "direct")) {
		ActionQueType = QUEUETYPE_DIRECT;
		dbgprintf("action queue type set to DIRECT (no queueing at all)\n");
	} else {
		logerrorSz("unknown actionqueue parameter: %s", (char *) pszType);
		iRet = RS_RET_INVALID_PARAMS;
	}
	free(pszType); /* no longer needed */

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
	/* first check if this is a regular message or the repeation of
	 * a previous message. If so, we need to change the message text
	 * to "last message repeated n times" and then go ahead and write
	 * it. Please note that we can not modify the message object, because
	 * that would update it in other selectors as well. As such, we first
	 * need to create a local copy of the message, which we than can update.
	 * rgerhards, 2007-07-10
	 */
	if(pAction->f_prevcount > 1) {
		msg_t *pMsg;
		uchar szRepMsg[64];
		snprintf((char*)szRepMsg, sizeof(szRepMsg), "last message repeated %d times",
		    pAction->f_prevcount);

		if((pMsg = MsgDup(pAction->f_pMsg)) == NULL) {
			/* it failed - nothing we can do against it... */
			dbgprintf("Message duplication failed, dropping repeat message.\n");
			ABORT_FINALIZE(RS_RET_ERR);
		}

		/* We now need to update the other message properties.
		 * ... RAWMSG is a problem ... Please note that digital
		 * signatures inside the message are also invalidated.
		 */
		getCurrTime(&(pMsg->tRcvdAt));
		getCurrTime(&(pMsg->tTIMESTAMP));
		MsgSetMSG(pMsg, (char*)szRepMsg);
		MsgSetRawMsg(pMsg, (char*)szRepMsg);

		pMsgSave = pAction->f_pMsg;	/* save message pointer for later restoration */
		pAction->f_pMsg = pMsg;	/* use the new msg (pointer will be restored below) */
	}

	dbgprintf("Called action, logging to %s", modGetStateName(pAction->pMod));

	time(&pAction->f_time); /* we need this for message repeation processing */

	/* When we reach this point, we have a valid, non-disabled action.
	 * So let's enqueue our message for execution. -- rgerhards, 2007-07-24
	 */
	iRet = queueEnqObj(pAction->pQueue, (void*) MsgAddRef(pAction->f_pMsg));

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
		MsgDestruct(&pAction->f_pMsg);
		pAction->f_pMsg = pMsgSave;	/* restore it */
	}

	RETiRet;
}


/* call the configured action. Does all necessary housekeeping.
 * rgerhards, 2007-08-01
 */
rsRetVal
actionCallAction(action_t *pAction, msg_t *pMsg)
{
	DEFiRet;
	int iCancelStateSave;

	ISOBJ_TYPE_assert(pMsg, Msg);
	ASSERT(pAction != NULL);

	/* Make sure nodbody else modifies/uses this action object. Right now, this
         * is important because of "message repeated n times" processing and potentially
	 * multiple worker threads. -- rgerhards, 2007-12-11
 	 */
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave);
	LockObj(pAction);
	pthread_cleanup_push(mutexCancelCleanup, pAction->Sync_mut);
	pthread_setcancelstate(iCancelStateSave, NULL);

	/* first, we need to check if this is a disabled
	 * entry. If so, we must not further process it.
	 * rgerhards 2005-09-26
	 * In the future, disabled modules may be re-probed from time
	 * to time. They are in a perfectly legal state, except that the
	 * doAction method indicated that it wanted to be disabled - but
	 * we do not consider this is a solution for eternity... So we
	 * should check from time to time if affairs have improved.
	 * rgerhards, 2007-07-24
	 */
	if(pAction->bEnabled == 0) {
		ABORT_FINALIZE(RS_RET_OK);
	}

	/* don't output marks to recently written files */
	if ((pMsg->msgFlags & MARK) && (time(NULL) - pAction->f_time) < MarkInterval / 2) {
		ABORT_FINALIZE(RS_RET_OK);
	}

	/* suppress duplicate messages
	 */
	if ((pAction->f_ReduceRepeated == 1) && pAction->f_pMsg != NULL &&
	    (pMsg->msgFlags & MARK) == 0 && getMSGLen(pMsg) == getMSGLen(pAction->f_pMsg) &&
	    !strcmp(getMSG(pMsg), getMSG(pAction->f_pMsg)) &&
	    !strcmp(getHOSTNAME(pMsg), getHOSTNAME(pAction->f_pMsg)) &&
	    !strcmp(getPROCID(pMsg), getPROCID(pAction->f_pMsg)) &&
	    !strcmp(getAPPNAME(pMsg), getAPPNAME(pAction->f_pMsg))) {
		pAction->f_prevcount++;
		dbgprintf("msg repeated %d times, %ld sec of %d.\n",
		    pAction->f_prevcount, time(NULL) - pAction->f_time,
		    repeatinterval[pAction->f_repeatcount]);
		/* use current message, so we have the new timestamp (means we need to discard previous one) */
		MsgDestruct(&pAction->f_pMsg);
		pAction->f_pMsg = MsgAddRef(pMsg);
		/* If domark would have logged this by now, flush it now (so we don't hold
		 * isolated messages), but back off so we'll flush less often in the future.
		 */
		if(time(NULL) > REPEATTIME(pAction)) {
			iRet = actionWriteToAction(pAction);
			BACKOFF(pAction);
		}
	} else {
		/* new message, save it */
		/* first check if we have a previous message stored
		 * if so, emit and then discard it first
		 */
		if(pAction->f_pMsg != NULL) {
			if(pAction->f_prevcount > 0)
				actionWriteToAction(pAction);
				/* we do not care about iRet above - I think it's right but if we have
				 * some troubles, you know where to look at ;) -- rgerhards, 2007-08-01
				 */
			MsgDestruct(&pAction->f_pMsg);
		}
		pAction->f_pMsg = MsgAddRef(pMsg);
		/* call the output driver */
		iRet = actionWriteToAction(pAction);
	}

finalize_it:
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave);
	UnlockObj(pAction);
	pthread_cleanup_pop(0); /* remove mutex cleanup handler */
	pthread_setcancelstate(iCancelStateSave, NULL);
	RETiRet;
}


/* add our cfsysline handlers
 * rgerhards, 2008-01-28
 */
rsRetVal
actionAddCfSysLineHdrl(void)
{
	DEFiRet;

	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuefilename", 0, eCmdHdlrGetWord, NULL, &pszActionQFName, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuesize", 0, eCmdHdlrInt, NULL, &iActionQueueSize, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuehighwatermark", 0, eCmdHdlrInt, NULL, &iActionQHighWtrMark, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuelowwatermark", 0, eCmdHdlrInt, NULL, &iActionQLowWtrMark, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuediscardmark", 0, eCmdHdlrInt, NULL, &iActionQDiscardMark, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuediscardseverity", 0, eCmdHdlrInt, NULL, &iActionQDiscardSeverity, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuecheckpointinterval", 0, eCmdHdlrInt, NULL, &iActionQPersistUpdCnt, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuetype", 0, eCmdHdlrGetWord, setActionQueType, NULL, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueueworkerthreads", 0, eCmdHdlrInt, NULL, &iActionQueueNumWorkers, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuetimeoutshutdown", 0, eCmdHdlrInt, NULL, &iActionQtoQShutdown, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuetimeoutactioncompletion", 0, eCmdHdlrInt, NULL, &iActionQtoActShutdown, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuetimeoutenqueue", 0, eCmdHdlrInt, NULL, &iActionQtoEnq, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuetimeoutworkerthreadshutdown", 0, eCmdHdlrInt, NULL, &iActionQtoWrkShutdown, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueueworkerthreadminimummessages", 0, eCmdHdlrInt, NULL, &iActionQWrkMinMsgs, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuemaxfilesize", 0, eCmdHdlrSize, NULL, &iActionQueMaxFileSize, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionqueuesaveonshutdown", 0, eCmdHdlrBinary, NULL, &bActionQSaveOnShutdown, NULL));
	
finalize_it:
	RETiRet;
}


/*
 * vi:set ai:
 */
