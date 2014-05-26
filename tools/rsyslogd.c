/* This is the main rsyslogd file.
 * It contains code * that is known to be validly under ASL 2.0,
 * because it was either written from scratch by me (rgerhards) or
 * contributors who agreed to ASL 2.0.
 *
 * Copyright 2004-2014 Rainer Gerhards and Adiscon
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
#include "rsyslog.h"

#include <liblogging/stdlog.h>
#ifdef OS_SOLARIS
#	include <errno.h>
#else
#	include <sys/errno.h>
#endif

#include "wti.h"
#include "ratelimit.h"
#include "parser.h"
#include "linkedlist.h"
#include "ruleset.h"
#include "action.h"
#include "iminternal.h"
#include "errmsg.h"
#include "threads.h"
#include "dnscache.h"
#include "prop.h"
#include "unicode-helper.h"
#include "net.h"
#include "errmsg.h"
#include "glbl.h"
#include "debug.h"
#include "dirty.h"

DEFobjCurrIf(obj)
DEFobjCurrIf(prop)
DEFobjCurrIf(parser)
DEFobjCurrIf(ruleset)
DEFobjCurrIf(net)
DEFobjCurrIf(errmsg)
DEFobjCurrIf(glbl)

/* imports from syslogd.c, these should go away over time (as we
 * migrate/replace more and more code to ASL 2.0).
 */
extern int bHadHUP;
extern int bFinished;
extern int doFork;

extern int realMain(int argc, char **argv);
extern rsRetVal queryLocalHostname(void);
void syslogdInit(int argc, char **argv);
void syslogd_die(void);
/* end syslogd.c imports */

/* forward definitions */
void rsyslogd_submitErrMsg(const int severity, const int iErr, const uchar *msg);

int MarkInterval = 20 * 60;	/* interval between marks in seconds - read-only after startup */
ratelimit_t *dflt_ratelimiter = NULL; /* ratelimiter for submits without explicit one */
uchar *ConfFile = (uchar*) "/etc/rsyslog.conf";

static struct queuefilenames_s {
	struct queuefilenames_s *next;
	uchar *name;
} *queuefilenames = NULL;

prop_t *pInternalInputName = NULL;	/* there is only one global inputName for all internally-generated messages */
ratelimit_t *internalMsg_ratelimiter = NULL; /* ratelimiter for rsyslog-own messages */


void
rsyslogd_usage(void)
{
	fprintf(stderr, "usage: rsyslogd [options]\n"
			"use \"man rsyslogd\" for details. To run rsyslog "
			"interactively, use \"rsyslogd -n\""
			"to run it in debug mode use \"rsyslogd -dn\"\n"
			"For further information see http://www.rsyslog.com/doc\n");
	exit(1); /* "good" exit - done to terminate usage() */
}


void
rsyslogd_sigttin_handler()
{
	/* this is just a dummy to care for our sigttin input
	 * module cancel interface. The important point is that
	 * it actually does *nothing*.
	 */
}

rsRetVal
rsyslogd_InitStdRatelimiters(void)
{
	DEFiRet;
	CHKiRet(ratelimitNew(&dflt_ratelimiter, "rsyslogd", "dflt"));
	/* TODO: add linux-type limiting capability */
	CHKiRet(ratelimitNew(&internalMsg_ratelimiter, "rsyslogd", "internal_messages"));
	ratelimitSetLinuxLike(internalMsg_ratelimiter, 5, 500);
	/* TODO: make internalMsg ratelimit settings configurable */
finalize_it:
	RETiRet;
}
#if 0
/* Use all objects we need. This is called by the GPLv3 part.
 */
rsRetVal
rsyslogdInit(void)
{
	DEFiRet;
	char *pErrObj; /* tells us which object failed if that happens (useful for troubleshooting!) */

	CHKiRet(objGetObjInterface(&obj)); /* this provides the root pointer for all other queries */

	/* Now tell the system which classes we need ourselfs */
	pErrObj = "ruleset";
	CHKiRet(objUse(ruleset,  CORE_COMPONENT));
	pErrObj = "prop";
	CHKiRet(objUse(prop,     CORE_COMPONENT));


finalize_it:
	if(iRet != RS_RET_OK) {
		fprintf(stderr, "Error during initialization for object '%s' - failing...\n", pErrObj);
	}

	RETiRet;
}
#endif

/* Method to initialize all global classes and use the objects that we need.
 * rgerhards, 2008-01-04
 * rgerhards, 2008-04-16: the actual initialization is now carried out by the runtime
 */
rsRetVal
rsyslogd_InitGlobalClasses(void)
{
	DEFiRet;
	char *pErrObj; /* tells us which object failed if that happens (useful for troubleshooting!) */

	/* Intialize the runtime system */
	pErrObj = "rsyslog runtime"; /* set in case the runtime errors before setting an object */
	CHKiRet(rsrtInit(&pErrObj, &obj));
	rsrtSetErrLogger(rsyslogd_submitErrMsg);

	/* Now tell the system which classes we need ourselfs */
	pErrObj = "glbl";
	CHKiRet(objUse(glbl,     CORE_COMPONENT));
	pErrObj = "errmsg";
	CHKiRet(objUse(errmsg,   CORE_COMPONENT));
	/*pErrObj = "module";
	CHKiRet(objUse(module,   CORE_COMPONENT));
	pErrObj = "datetime";
	CHKiRet(objUse(datetime, CORE_COMPONENT));*/
	pErrObj = "ruleset";
	CHKiRet(objUse(ruleset,  CORE_COMPONENT));
	/*pErrObj = "conf";
	CHKiRet(objUse(conf,     CORE_COMPONENT));*/
	pErrObj = "prop";
	CHKiRet(objUse(prop,     CORE_COMPONENT));
	pErrObj = "parser";
	CHKiRet(objUse(parser,     CORE_COMPONENT));
	/*pErrObj = "rsconf";
	CHKiRet(objUse(rsconf,     CORE_COMPONENT));*/

	/* intialize some dummy classes that are not part of the runtime */
	pErrObj = "action";
	CHKiRet(actionClassInit());
	pErrObj = "template";
	CHKiRet(templateInit());

	/* TODO: the dependency on net shall go away! -- rgerhards, 2008-03-07 */
	pErrObj = "net";
	CHKiRet(objUse(net, LM_NET_FILENAME));

	dnscacheInit();
	initRainerscript();
	ratelimitModInit();

	/* we need to create the inputName property (only once during our lifetime) */
	CHKiRet(prop.Construct(&pInternalInputName));
	CHKiRet(prop.SetString(pInternalInputName, UCHAR_CONSTANT("rsyslogd"), sizeof("rsyslogd") - 1));
	CHKiRet(prop.ConstructFinalize(pInternalInputName));

finalize_it:
	if(iRet != RS_RET_OK) {
		/* we know we are inside the init sequence, so we can safely emit
		 * messages to stderr. -- rgerhards, 2008-04-02
		 */
		fprintf(stderr, "Error during class init for object '%s' - failing...\n", pErrObj);
		fprintf(stderr, "rsyslogd initializiation failed - global classes could not be initialized.\n"
				"Did you do a \"make install\"?\n"
				"Suggested action: run rsyslogd with -d -n options to see what exactly "
				"fails.\n");
	}

	RETiRet;
}

/* preprocess a batch of messages, that is ready them for actual processing. This is done
 * as a first stage and totally in parallel to any other worker active in the system. So
 * it helps us keep up the overall concurrency level.
 * rgerhards, 2010-06-09
 */
static inline rsRetVal
preprocessBatch(batch_t *pBatch, int *pbShutdownImmediate) {
	prop_t *ip;
	prop_t *fqdn;
	prop_t *localName;
	prop_t *propFromHost = NULL;
	prop_t *propFromHostIP = NULL;
	int bIsPermitted;
	msg_t *pMsg;
	int i;
	rsRetVal localRet;
	DEFiRet;

	for(i = 0 ; i < pBatch->nElem  && !*pbShutdownImmediate ; i++) {
		pMsg = pBatch->pElem[i].pMsg;
		if((pMsg->msgFlags & NEEDS_ACLCHK_U) != 0) {
			DBGPRINTF("msgConsumer: UDP ACL must be checked for message (hostname-based)\n");
			if(net.cvthname(pMsg->rcvFrom.pfrominet, &localName, &fqdn, &ip) != RS_RET_OK)
				continue;
			bIsPermitted = net.isAllowedSender2((uchar*)"UDP",
			    (struct sockaddr *)pMsg->rcvFrom.pfrominet, (char*)propGetSzStr(fqdn), 1);
			if(!bIsPermitted) {
				DBGPRINTF("Message from '%s' discarded, not a permitted sender host\n",
					  propGetSzStr(fqdn));
				pBatch->eltState[i] = BATCH_STATE_DISC;
			} else {
				/* save some of the info we obtained */
				MsgSetRcvFrom(pMsg, localName);
				CHKiRet(MsgSetRcvFromIP(pMsg, ip));
				pMsg->msgFlags &= ~NEEDS_ACLCHK_U;
			}
		}
		if((pMsg->msgFlags & NEEDS_PARSING) != 0) {
			if((localRet = parser.ParseMsg(pMsg)) != RS_RET_OK)  {
				DBGPRINTF("Message discarded, parsing error %d\n", localRet);
				pBatch->eltState[i] = BATCH_STATE_DISC;
			}
		}
	}

finalize_it:
	if(propFromHost != NULL)
		prop.Destruct(&propFromHost);
	if(propFromHostIP != NULL)
		prop.Destruct(&propFromHostIP);
	RETiRet;
}


/* The consumer of dequeued messages. This function is called by the
 * queue engine on dequeueing of a message. It runs on a SEPARATE
 * THREAD. It receives an array of pointers, which it must iterate
 * over. We do not do any further batching, as this is of no benefit
 * for the main queue.
 */
static rsRetVal
msgConsumer(void __attribute__((unused)) *notNeeded, batch_t *pBatch, wti_t *pWti)
{
	DEFiRet;
	assert(pBatch != NULL);
	preprocessBatch(pBatch, pWti->pbShutdownImmediate);
	ruleset.ProcessBatch(pBatch, pWti);
//TODO: the BATCH_STATE_COMM must be set somewhere down the road, but we 
//do not have this yet and so we emulate -- 2010-06-10
int i;
	for(i = 0 ; i < pBatch->nElem  && !*pWti->pbShutdownImmediate ; i++) {
		pBatch->eltState[i] = BATCH_STATE_COMM;
	}
	RETiRet;
}


/* create a main message queue, now also used for ruleset queues. This function
 * needs to be moved to some other module, but it is considered acceptable for
 * the time being (remember that we want to restructure config processing at large!).
 * rgerhards, 2009-10-27
 */
rsRetVal createMainQueue(qqueue_t **ppQueue, uchar *pszQueueName, struct nvlst *lst)
{
	struct queuefilenames_s *qfn;
	uchar *qfname = NULL;
	static int qfn_renamenum = 0;
	uchar qfrenamebuf[1024];
	DEFiRet;

	/* create message queue */
	CHKiRet_Hdlr(qqueueConstruct(ppQueue, ourConf->globals.mainQ.MainMsgQueType, ourConf->globals.mainQ.iMainMsgQueueNumWorkers, ourConf->globals.mainQ.iMainMsgQueueSize, msgConsumer)) {
		/* no queue is fatal, we need to give up in that case... */
		errmsg.LogError(0, iRet, "could not create (ruleset) main message queue"); \
	}
	/* name our main queue object (it's not fatal if it fails...) */
	obj.SetName((obj_t*) (*ppQueue), pszQueueName);

	if(lst == NULL) { /* use legacy parameters? */
		/* ... set some properties ... */
	#	define setQPROP(func, directive, data) \
		CHKiRet_Hdlr(func(*ppQueue, data)) { \
			errmsg.LogError(0, NO_ERRCODE, "Invalid " #directive ", error %d. Ignored, running with default setting", iRet); \
		}
	#	define setQPROPstr(func, directive, data) \
		CHKiRet_Hdlr(func(*ppQueue, data, (data == NULL)? 0 : strlen((char*) data))) { \
			errmsg.LogError(0, NO_ERRCODE, "Invalid " #directive ", error %d. Ignored, running with default setting", iRet); \
		}

		if(ourConf->globals.mainQ.pszMainMsgQFName != NULL) {
			/* check if the queue file name is unique, else emit an error */
			for(qfn = queuefilenames ; qfn != NULL ; qfn = qfn->next) {
				dbgprintf("check queue file name '%s' vs '%s'\n", qfn->name, ourConf->globals.mainQ.pszMainMsgQFName );
				if(!ustrcmp(qfn->name, ourConf->globals.mainQ.pszMainMsgQFName)) {
					snprintf((char*)qfrenamebuf, sizeof(qfrenamebuf), "%d-%s-%s",
						 ++qfn_renamenum, ourConf->globals.mainQ.pszMainMsgQFName,  
						 (pszQueueName == NULL) ? "NONAME" : (char*)pszQueueName);
					qfname = ustrdup(qfrenamebuf);
					errmsg.LogError(0, NO_ERRCODE, "Error: queue file name '%s' already in use "
						" - using '%s' instead", ourConf->globals.mainQ.pszMainMsgQFName, qfname);
					break;
				}
			}
			if(qfname == NULL)
				qfname = ustrdup(ourConf->globals.mainQ.pszMainMsgQFName);
			qfn = malloc(sizeof(struct queuefilenames_s));
			qfn->name = qfname;
			qfn->next = queuefilenames;
			queuefilenames = qfn;
		}

		setQPROP(qqueueSetMaxFileSize, "$MainMsgQueueFileSize", ourConf->globals.mainQ.iMainMsgQueMaxFileSize);
		setQPROP(qqueueSetsizeOnDiskMax, "$MainMsgQueueMaxDiskSpace", ourConf->globals.mainQ.iMainMsgQueMaxDiskSpace);
		setQPROP(qqueueSetiDeqBatchSize, "$MainMsgQueueDequeueBatchSize", ourConf->globals.mainQ.iMainMsgQueDeqBatchSize);
		setQPROPstr(qqueueSetFilePrefix, "$MainMsgQueueFileName", qfname);
		setQPROP(qqueueSetiPersistUpdCnt, "$MainMsgQueueCheckpointInterval", ourConf->globals.mainQ.iMainMsgQPersistUpdCnt);
		setQPROP(qqueueSetbSyncQueueFiles, "$MainMsgQueueSyncQueueFiles", ourConf->globals.mainQ.bMainMsgQSyncQeueFiles);
		setQPROP(qqueueSettoQShutdown, "$MainMsgQueueTimeoutShutdown", ourConf->globals.mainQ.iMainMsgQtoQShutdown );
		setQPROP(qqueueSettoActShutdown, "$MainMsgQueueTimeoutActionCompletion", ourConf->globals.mainQ.iMainMsgQtoActShutdown);
		setQPROP(qqueueSettoWrkShutdown, "$MainMsgQueueWorkerTimeoutThreadShutdown", ourConf->globals.mainQ.iMainMsgQtoWrkShutdown);
		setQPROP(qqueueSettoEnq, "$MainMsgQueueTimeoutEnqueue", ourConf->globals.mainQ.iMainMsgQtoEnq);
		setQPROP(qqueueSetiHighWtrMrk, "$MainMsgQueueHighWaterMark", ourConf->globals.mainQ.iMainMsgQHighWtrMark);
		setQPROP(qqueueSetiLowWtrMrk, "$MainMsgQueueLowWaterMark", ourConf->globals.mainQ.iMainMsgQLowWtrMark);
		setQPROP(qqueueSetiDiscardMrk, "$MainMsgQueueDiscardMark", ourConf->globals.mainQ.iMainMsgQDiscardMark);
		setQPROP(qqueueSetiDiscardSeverity, "$MainMsgQueueDiscardSeverity", ourConf->globals.mainQ.iMainMsgQDiscardSeverity);
		setQPROP(qqueueSetiMinMsgsPerWrkr, "$MainMsgQueueWorkerThreadMinimumMessages", ourConf->globals.mainQ.iMainMsgQWrkMinMsgs);
		setQPROP(qqueueSetbSaveOnShutdown, "$MainMsgQueueSaveOnShutdown", ourConf->globals.mainQ.bMainMsgQSaveOnShutdown);
		setQPROP(qqueueSetiDeqSlowdown, "$MainMsgQueueDequeueSlowdown", ourConf->globals.mainQ.iMainMsgQDeqSlowdown);
		setQPROP(qqueueSetiDeqtWinFromHr,  "$MainMsgQueueDequeueTimeBegin", ourConf->globals.mainQ.iMainMsgQueueDeqtWinFromHr);
		setQPROP(qqueueSetiDeqtWinToHr,    "$MainMsgQueueDequeueTimeEnd", ourConf->globals.mainQ.iMainMsgQueueDeqtWinToHr);

	#	undef setQPROP
	#	undef setQPROPstr
	} else { /* use new style config! */
		qqueueSetDefaultsRulesetQueue(*ppQueue);
		qqueueApplyCnfParam(*ppQueue, lst);
	}
	RETiRet;
}

rsRetVal
startMainQueue(qqueue_t *pQueue)
{
	DEFiRet;
	CHKiRet_Hdlr(qqueueStart(pQueue)) {
		/* no queue is fatal, we need to give up in that case... */
		errmsg.LogError(0, iRet, "could not start (ruleset) main message queue"); \
	}
	RETiRet;
}


/* this is a special function used to submit an error message. This
 * function is also passed to the runtime library as the generic error
 * message handler. -- rgerhards, 2008-04-17
 */
void
rsyslogd_submitErrMsg(const int severity, const int iErr, const uchar *msg)
{
	logmsgInternal(iErr, LOG_SYSLOG|(severity & 0x07), msg, 0);
}

static inline rsRetVal
submitMsgWithDfltRatelimiter(msg_t *pMsg)
{
	return ratelimitAddMsg(dflt_ratelimiter, NULL, pMsg);
}


/* This function logs a message to rsyslog itself, using its own
 * internal structures. This means external programs (like the
 * system journal) will never see this message.
 */
static rsRetVal
logmsgInternalSelf(const int iErr, const int pri, const size_t lenMsg,
	const char *__restrict__ const msg, int flags)
{
	uchar pszTag[33];
	msg_t *pMsg;
	DEFiRet;

	CHKiRet(msgConstruct(&pMsg));
	MsgSetInputName(pMsg, pInternalInputName);
	MsgSetRawMsg(pMsg, (char*)msg, lenMsg);
	MsgSetHOSTNAME(pMsg, glbl.GetLocalHostName(), ustrlen(glbl.GetLocalHostName()));
	MsgSetRcvFrom(pMsg, glbl.GetLocalHostNameProp());
	MsgSetRcvFromIP(pMsg, glbl.GetLocalHostIP());
	MsgSetMSGoffs(pMsg, 0);
	/* check if we have an error code associated and, if so,
	 * adjust the tag. -- rgerhards, 2008-06-27
	 */
	if(iErr == NO_ERRCODE) {
		MsgSetTAG(pMsg, UCHAR_CONSTANT("rsyslogd:"), sizeof("rsyslogd:") - 1);
	} else {
		size_t len = snprintf((char*)pszTag, sizeof(pszTag), "rsyslogd%d:", iErr);
		pszTag[32] = '\0'; /* just to make sure... */
		MsgSetTAG(pMsg, pszTag, len);
	}
	pMsg->iFacility = LOG_FAC(pri);
	pMsg->iSeverity = LOG_PRI(pri);
	flags |= INTERNAL_MSG;
	pMsg->msgFlags  = flags;

	if(bHaveMainQueue == 0) { /* not yet in queued mode */
		iminternalAddMsg(pMsg);
	} else {
               /* we have the queue, so we can simply provide the
		 * message to the queue engine.
		 */
		ratelimitAddMsg(internalMsg_ratelimiter, NULL, pMsg);
	}
finalize_it:
	RETiRet;
}



/* rgerhards 2004-11-09: the following is a function that can be used
 * to log a message orginating from the syslogd itself.
 */
rsRetVal
logmsgInternal(int iErr, int pri, const uchar *const msg, int flags)
{
	size_t lenMsg;
	unsigned i;
	char *bufModMsg = NULL; /* buffer for modified message, should we need to modify */
	DEFiRet;

	/* we first do a path the remove control characters that may have accidently
	 * introduced (program error!). This costs performance, but we do not expect
	 * to be called very frequently in any case ;) -- rgerhards, 2013-12-19.
	 */
	lenMsg = ustrlen(msg);
	for(i = 0 ; i < lenMsg ; ++i) {
		if(msg[i] < 0x20 || msg[i] == 0x7f) {
			if(bufModMsg == NULL) {
				CHKmalloc(bufModMsg = strdup((char*) msg));
			}
			bufModMsg[i] = ' ';
		}
	}

	if(bProcessInternalMessages) {
		CHKiRet(logmsgInternalSelf(iErr, pri, lenMsg,
					   (bufModMsg == NULL) ? (char*)msg : bufModMsg,
					   flags));
	} else {
		stdlog_log(stdlog_hdl, LOG_PRI(pri), "%s",
			   (bufModMsg == NULL) ? (char*)msg : bufModMsg);
	}

	/* we now check if we should print internal messages out to stderr. This was
	 * suggested by HKS as a way to help people troubleshoot rsyslog configuration
	 * (by running it interactively. This makes an awful lot of sense, so I add
	 * it here. -- rgerhards, 2008-07-28
	 * Note that error messages can not be disabled during a config verify. This
	 * permits us to process unmodified config files which otherwise contain a
	 * supressor statement.
	 */
	if(((Debug == DEBUG_FULL || !doFork) && ourConf->globals.bErrMsgToStderr) || iConfigVerify) {
		if(LOG_PRI(pri) == LOG_ERR)
			fprintf(stderr, "rsyslogd: %s\n", (bufModMsg == NULL) ? (char*)msg : bufModMsg);
	}

finalize_it:
	free(bufModMsg);
	RETiRet;
}

rsRetVal
submitMsg(msg_t *pMsg)
{
	return submitMsgWithDfltRatelimiter(pMsg);
}

/* submit a message to the main message queue.   This is primarily
 * a hook to prevent the need for callers to know about the main message queue
 * rgerhards, 2008-02-13
 */
rsRetVal
submitMsg2(msg_t *pMsg)
{
	qqueue_t *pQueue;
	ruleset_t *pRuleset;
	DEFiRet;

	ISOBJ_TYPE_assert(pMsg, msg);

	pRuleset = MsgGetRuleset(pMsg);
	pQueue = (pRuleset == NULL) ? pMsgQueue : ruleset.GetRulesetQueue(pRuleset);

	/* if a plugin logs a message during shutdown, the queue may no longer exist */
	if(pQueue == NULL) {
		DBGPRINTF("submitMsg2() could not submit message - "
			  "queue does (no longer?) exist - ignored\n");
		FINALIZE;
	}

	qqueueEnqMsg(pQueue, pMsg->flowCtlType, pMsg);

finalize_it:
	RETiRet;
}

/* submit multiple messages at once, very similar to submitMsg, just
 * for multi_submit_t. All messages need to go into the SAME queue!
 * rgerhards, 2009-06-16
 */
rsRetVal
multiSubmitMsg2(multi_submit_t *pMultiSub)
{
	qqueue_t *pQueue;
	ruleset_t *pRuleset;
	DEFiRet;
	assert(pMultiSub != NULL);

	if(pMultiSub->nElem == 0)
		FINALIZE;

	pRuleset = MsgGetRuleset(pMultiSub->ppMsgs[0]);
	pQueue = (pRuleset == NULL) ? pMsgQueue : ruleset.GetRulesetQueue(pRuleset);

	/* if a plugin logs a message during shutdown, the queue may no longer exist */
	if(pQueue == NULL) {
		DBGPRINTF("multiSubmitMsg() could not submit message - "
			  "queue does (no longer?) exist - ignored\n");
		FINALIZE;
	}

	iRet = pQueue->MultiEnq(pQueue, pMultiSub);
	pMultiSub->nElem = 0;

finalize_it:
	RETiRet;
}
rsRetVal
multiSubmitMsg(multi_submit_t *pMultiSub) /* backward compat. level */
{
	return multiSubmitMsg2(pMultiSub);
}


/* flush multiSubmit, e.g. at end of read records */
rsRetVal
multiSubmitFlush(multi_submit_t *pMultiSub)
{
	DEFiRet;
	if(pMultiSub->nElem > 0) {
		iRet = multiSubmitMsg2(pMultiSub);
	}
	RETiRet;
}


/* this function pulls all internal messages from the buffer
 * and puts them into the processing engine.
 * We can only do limited error handling, as this would not
 * really help us. TODO: add error messages?
 * rgerhards, 2007-08-03
 */
static inline void processImInternal(void)
{
	msg_t *pMsg;

	while(iminternalRemoveMsg(&pMsg) == RS_RET_OK) {
		ratelimitAddMsg(dflt_ratelimiter, NULL, pMsg);
	}
}


/* This takes a received message that must be decoded and submits it to
 * the main message queue. This is a legacy function which is being provided
 * to aid older input plugins that do not support message creation via
 * the new interfaces themselves. It is not recommended to use this
 * function for new plugins. -- rgerhards, 2009-10-12
 */
rsRetVal
parseAndSubmitMessage(uchar *hname, uchar *hnameIP, uchar *msg, int len, int flags, flowControl_t flowCtlType,
	prop_t *pInputName, struct syslogTime *stTime, time_t ttGenTime, ruleset_t *pRuleset)
{
	prop_t *pProp = NULL;
	msg_t *pMsg;
	DEFiRet;

	/* we now create our own message object and submit it to the queue */
	if(stTime == NULL) {
		CHKiRet(msgConstruct(&pMsg));
	} else {
		CHKiRet(msgConstructWithTime(&pMsg, stTime, ttGenTime));
	}
	if(pInputName != NULL)
		MsgSetInputName(pMsg, pInputName);
	MsgSetRawMsg(pMsg, (char*)msg, len);
	MsgSetFlowControlType(pMsg, flowCtlType);
	MsgSetRuleset(pMsg, pRuleset);
	pMsg->msgFlags  = flags | NEEDS_PARSING;

	MsgSetRcvFromStr(pMsg, hname, ustrlen(hname), &pProp);
	CHKiRet(prop.Destruct(&pProp));
	CHKiRet(MsgSetRcvFromIPStr(pMsg, hnameIP, ustrlen(hnameIP), &pProp));
	CHKiRet(prop.Destruct(&pProp));
	CHKiRet(submitMsg2(pMsg));

finalize_it:
	RETiRet;
}


/* helper to doHUP(), this "HUPs" each action. The necessary locking
 * is done inside the action class and nothing we need to take care of.
 * rgerhards, 2008-10-22
 */
DEFFUNC_llExecFunc(doHUPActions)
{
	BEGINfunc
	actionCallHUPHdlr((action_t*) pData);
	ENDfunc
	return RS_RET_OK; /* we ignore errors, we can not do anything either way */
}


/* This function processes a HUP after one has been detected. Note that this
 * is *NOT* the sighup handler. The signal is recorded by the handler, that record
 * detected inside the mainloop and then this function is called to do the
 * real work. -- rgerhards, 2008-10-22
 * Note: there is a VERY slim chance of a data race when the hostname is reset.
 * We prefer to take this risk rather than sync all accesses, because to the best
 * of my analysis it can not really hurt (the actual property is reference-counted)
 * but the sync would require some extra CPU for *each* message processed.
 * rgerhards, 2012-04-11
 */
static inline void
doHUP(void)
{
	char buf[512];

	if(ourConf->globals.bLogStatusMsgs) {
		snprintf(buf, sizeof(buf) / sizeof(char),
			 " [origin software=\"rsyslogd\" " "swVersion=\"" VERSION
			 "\" x-pid=\"%d\" x-info=\"http://www.rsyslog.com\"] rsyslogd was HUPed",
			 (int) glblGetOurPid());
			errno = 0;
		logmsgInternal(NO_ERRCODE, LOG_SYSLOG|LOG_INFO, (uchar*)buf, 0);
	}

	queryLocalHostname(); /* re-read our name */
	ruleset.IterateAllActions(ourConf, doHUPActions, NULL);
	lookupDoHUP();
}


/* This is the main processing loop. It is called after successful initialization.
 * When it returns, the syslogd terminates.
 * Its sole function is to provide some housekeeping things. The real work is done
 * by the other threads spawned.
 */
static void
mainloop(void)
{
	struct timeval tvSelectTimeout;

	BEGINfunc
	/* first check if we have any internal messages queued and spit them out. We used
	 * to do that on any loop iteration, but that is no longer necessry. The reason
	 * is that once we reach this point here, we always run on multiple threads and
	 * thus the main queue is properly initialized. -- rgerhards, 2008-06-09
	 */
	processImInternal();

	while(!bFinished){
		/* this is now just a wait - please note that we do use a near-"eternal"
		 * timeout of 1 day. This enables us to help safe the environment
		 * by not unnecessarily awaking rsyslog on a regular tick (just think
		 * powertop, for example). In that case, we primarily wait for a signal,
		 * but a once-a-day wakeup should be quite acceptable. -- rgerhards, 2008-06-09
		 */
		tvSelectTimeout.tv_sec = 86400 /*1 day*/;
		tvSelectTimeout.tv_usec = 0;
		select(1, NULL, NULL, NULL, &tvSelectTimeout);
		if(bFinished)
			break;	/* exit as quickly as possible */

		if(bHadHUP) {
			doHUP();
			bHadHUP = 0;
			continue;
		}
	}
	ENDfunc
}


/* Finalize and destruct all actions.
 */
void
rsyslogd_destructAllActions(void)
{
	ruleset.DestructAllActions(runConf);
	bHaveMainQueue = 0; // flag that internal messages need to be temporarily stored
}


/* This is the main entry point into rsyslogd. This must be a function in its own
 * right in order to intialize the debug system in a portable way (otherwise we would
 * need to have a statement before variable definitions.
 * rgerhards, 20080-01-28
 */
int main(int argc, char **argv)
{
	dbgClassInit();
	syslogdInit(argc, argv);

	mainloop();

	/* do any de-init's that need to be done AFTER this comment */
	syslogd_die();
	thrdExit();
	if(pInternalInputName != NULL)
		prop.Destruct(&pInternalInputName);

	exit(0);
}
