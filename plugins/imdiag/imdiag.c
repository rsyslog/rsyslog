/* imdiag.c
 * This is a testbench tool. It started out with a broader scope,
 * but we dropped this idea. To learn about rsyslog runtime statistics
 * have a look at impstats.
 *
 * File begun on 2008-07-25 by RGerhards
 *
 * Copyright 2008-2020 Adiscon GmbH.
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
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <ctype.h>
#include <signal.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <semaphore.h>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include "rsyslog.h"
#include "dirty.h"
#include "cfsysline.h"
#include "module-template.h"
#include "unicode-helper.h"
#include "net.h"
#include "netstrm.h"
#include "errmsg.h"
#include "tcpsrv.h"
#include "srUtils.h"
#include "msg.h"
#include "datetime.h"
#include "ratelimit.h"
#include "queue.h"
#include "lookup.h"
#include "net.h" /* for permittedPeers, may be removed when this is removed */
#include "statsobj.h"


MODULE_TYPE_INPUT
MODULE_TYPE_NOKEEP

/* static data */
DEF_IMOD_STATIC_DATA
DEFobjCurrIf(tcpsrv)
DEFobjCurrIf(tcps_sess)
DEFobjCurrIf(net)
DEFobjCurrIf(netstrm)
DEFobjCurrIf(datetime)
DEFobjCurrIf(prop)
DEFobjCurrIf(statsobj)

/* Module static data */
static tcpsrv_t *pOurTcpsrv = NULL;  /* our TCP server(listener) TODO: change for multiple instances */
static permittedPeers_t *pPermPeersRoot = NULL;
static prop_t *pInputName = NULL;
/* there is only one global inputName for all messages generated by this input */
static prop_t *pRcvDummy = NULL;
static prop_t *pRcvIPDummy = NULL;

static int max_empty_checks = 3; /* how often check for queue empty during shutdown? */

statsobj_t *diagStats;
STATSCOUNTER_DEF(potentialArtificialDelayMs, mutPotentialArtificialDelayMs)
STATSCOUNTER_DEF(actualArtificialDelayMs, mutActualArtificialDelayMs)
STATSCOUNTER_DEF(delayInvocationCount, mutDelayInvocationCount)

static sem_t statsReportingBlocker;
static long long statsReportingBlockStartTimeMs = 0;
static int allowOnlyOnce = 0;
DEF_ATOMIC_HELPER_MUT(mutAllowOnlyOnce);
pthread_mutex_t mutStatsReporterWatch;
pthread_cond_t statsReporterWatch;
int statsReported = 0;
static int abortTimeout = -1;		/* for timeoutGuard - if set, abort rsyslogd after that many seconds */
static pthread_t timeoutGuard_thrd;	/* thread ID for timeoutGuard thread (if active) */

/* config settings */
struct modConfData_s {
	EMPTY_STRUCT;
};

static flowControl_t injectmsgDelayMode = eFLOWCTL_NO_DELAY;
static int iTCPSessMax = 20; /* max number of sessions */
static int iStrmDrvrMode = 0; /* mode for stream driver, driver-dependent (0 mostly means plain tcp) */
static uchar *pszLstnPortFileName = NULL;
static uchar *pszStrmDrvrAuthMode = NULL; /* authentication mode to use */
static uchar *pszInputName = NULL; /* value for inputname property, NULL is OK and handled by core engine */


/* callbacks */
/* this shall go into a specific ACL module! */
static int
isPermittedHost(struct sockaddr __attribute__((unused)) *addr, char __attribute__((unused)) *fromHostFQDN,
		void __attribute__((unused)) *pUsrSrv, void __attribute__((unused)) *pUsrSess)
{
	return 1;	/* TODO: implement ACLs ... or via some other way? */
}


static rsRetVal
doOpenLstnSocks(tcpsrv_t *pSrv)
{
	ISOBJ_TYPE_assert(pSrv, tcpsrv);
	dbgprintf("in imdiag doOpenLstnSocks\n");
	return tcpsrv.create_tcp_socket(pSrv);
}


static rsRetVal
doRcvData(tcps_sess_t *pSess, char *buf, size_t lenBuf, ssize_t *piLenRcvd, int *oserr)
{
	assert(pSess != NULL);
	assert(piLenRcvd != NULL);

	*piLenRcvd = lenBuf;
	return netstrm.Rcv(pSess->pStrm, (uchar*) buf, piLenRcvd, oserr);
}

static rsRetVal
onRegularClose(tcps_sess_t *pSess)
{
	DEFiRet;
	assert(pSess != NULL);

	/* process any incomplete frames left over */
	tcps_sess.PrepareClose(pSess);
	/* Session closed */
	tcps_sess.Close(pSess);
	RETiRet;
}


static rsRetVal
onErrClose(tcps_sess_t *pSess)
{
	DEFiRet;
	assert(pSess != NULL);

	tcps_sess.Close(pSess);
	RETiRet;
}

/* ------------------------------ end callbacks ------------------------------ */


/* get the first word delimited by space from a given string. The pointer is
 * advanced to after the word. Any leading spaces are discarded. If the
 * output buffer is too small, parsing ends on buffer full condition.
 * An empty buffer is returned if there is no more data inside the string.
 * rgerhards, 2009-05-27
 */
#define TO_LOWERCASE	1
#define NO_MODIFY	0
static void
getFirstWord(uchar **ppszSrc, uchar *pszBuf, size_t lenBuf, int options)
{
	uchar c;
	uchar *pszSrc = *ppszSrc;

	while(*pszSrc && *pszSrc == ' ')
		++pszSrc; /* skip to first non-space */

	while(*pszSrc && *pszSrc != ' ' && lenBuf > 1) {
		c = *pszSrc++;
		if(options & TO_LOWERCASE)
			c = tolower(c);
		*pszBuf++ = c;
		lenBuf--;
	}

	*pszBuf = '\0';
	*ppszSrc = pszSrc;
}


/* send a response back to the originator
 * rgerhards, 2009-05-27
 */
static rsRetVal __attribute__((format(printf, 2, 3)))
sendResponse(tcps_sess_t *pSess, const char *const __restrict__ fmt, ...)
{
	va_list ap;
	ssize_t len;
	uchar buf[1024];
	DEFiRet;

	va_start(ap, fmt);
	len = vsnprintf((char*)buf, sizeof(buf), fmt, ap);
	va_end(ap);
	CHKiRet(netstrm.Send(pSess->pStrm, buf, &len));

finalize_it:
	RETiRet;
}

/* submit a generated numeric-suffix message to the rsyslog core
 */
static rsRetVal
doInjectMsg(uchar *szMsg, ratelimit_t *ratelimiter)
{
	smsg_t *pMsg;
	struct syslogTime stTime;
	time_t ttGenTime;
	DEFiRet;

	datetime.getCurrTime(&stTime, &ttGenTime, TIME_IN_LOCALTIME);
	/* we now create our own message object and submit it to the queue */
	CHKiRet(msgConstructWithTime(&pMsg, &stTime, ttGenTime));
	MsgSetRawMsg(pMsg, (char*) szMsg, ustrlen(szMsg));
	MsgSetInputName(pMsg, pInputName);
	MsgSetFlowControlType(pMsg, injectmsgDelayMode);
	pMsg->msgFlags  = NEEDS_PARSING | PARSE_HOSTNAME;
	MsgSetRcvFrom(pMsg, pRcvDummy);
	CHKiRet(MsgSetRcvFromIP(pMsg, pRcvIPDummy));
	CHKiRet(ratelimitAddMsg(ratelimiter, NULL, pMsg));

finalize_it:
	RETiRet;
}

/* submit a generated numeric-suffix message to the rsyslog core
 */
static rsRetVal
doInjectNumericSuffixMsg(int64_t iNum, ratelimit_t *ratelimiter)
{
	uchar szMsg[1024];
	DEFiRet;
	snprintf((char*)szMsg, sizeof(szMsg)/sizeof(uchar),
		"<167>Mar  1 01:00:00 172.20.245.8 tag msgnum:%8.8lld:", (long long) iNum);
	iRet = doInjectMsg(szMsg, ratelimiter);
	RETiRet;
}

/* This function injects messages. Command format:
 * injectmsg <fromnbr> <number-of-messages>
 * rgerhards, 2009-05-27
 */
static rsRetVal
injectMsg(uchar *pszCmd, tcps_sess_t *pSess)
{
	uchar wordBuf[1024];
	int64_t iFrom, nMsgs;
	uchar *literalMsg;
	int64_t i;
	ratelimit_t *ratelimit = NULL;
	DEFiRet;

	literalMsg = NULL;

	memset(wordBuf, 0, sizeof(wordBuf));
	CHKiRet(ratelimitNew(&ratelimit, "imdiag", "injectmsg"));
	/* we do not check errors here! */
	getFirstWord(&pszCmd, wordBuf, sizeof(wordBuf), TO_LOWERCASE);
	if (ustrcmp(UCHAR_CONSTANT("literal"), wordBuf) == 0) {
		/* user has provided content for a message */
		++pszCmd; /* ignore following space */
		CHKiRet(doInjectMsg(pszCmd, ratelimit));
		nMsgs = 1;
	} else { /* assume 2 args, (from_idx, count) */
		iFrom = atol((char*)wordBuf);
		getFirstWord(&pszCmd, wordBuf, sizeof(wordBuf), TO_LOWERCASE);
		nMsgs = atol((char*)wordBuf);
		for(i = 0 ; i < nMsgs ; ++i) {
			CHKiRet(doInjectNumericSuffixMsg(i + iFrom, ratelimit));
		}
	}
	CHKiRet(sendResponse(pSess, "%lld messages injected\n", nMsgs));

	DBGPRINTF("imdiag: %lld messages injected\n", nMsgs);

finalize_it:
	if(ratelimit != NULL)
		ratelimitDestruct(ratelimit);
	free(literalMsg);
	RETiRet;
}


/* This function waits until all queues are drained (size = 0)
 * To make sure it really is drained, we check multiple times. Otherwise we
 * may just see races. Note: it is important to ensure that the size
 * is zero multiple times in succession. Otherwise, we may just accidently
 * hit a situation where the queue isn't filled for a while (we have seen
 * this in practice, see https://github.com/rsyslog/rsyslog/issues/688).
 * Note: until 2014--07-13, this checked just the main queue. However,
 * the testbench was the sole user and checking all queues makes much more
 * sense. So we change function semantics instead of carrying the old
 * semantics over and crafting a new function. -- rgerhards
 */
static rsRetVal
waitMainQEmpty(tcps_sess_t *pSess)
{
	int iPrint = 0;
	int iPrintVerbosity = 500; // 500 default
	int nempty = 0;
	static unsigned lastOverallQueueSize = 1;
	DEFiRet;

	while(1) {
		processImInternal();
		const unsigned OverallQueueSize = PREFER_FETCH_32BIT(iOverallQueueSize);
		if(OverallQueueSize == 0) {
			++nempty;
		} else {
			if(OverallQueueSize > 500) {
				/* do a bit of extra sleep to not poll too frequently */
				srSleep(0, (OverallQueueSize > 2000) ? 900000 : 100000);
			}
			nempty = 0;
		}
		if(dbgTimeoutToStderr) { /* we abuse this setting a bit ;-) */
			if(OverallQueueSize != lastOverallQueueSize) {
				fprintf(stderr, "imdiag: wait q_empty: qsize %d nempty %d\n",
					OverallQueueSize, nempty);
				lastOverallQueueSize = OverallQueueSize;
			}
		}
		if(nempty > max_empty_checks)
			break;
		if(iPrint++ % iPrintVerbosity == 0)
			DBGPRINTF("imdiag sleeping, wait queues drain, "
				"curr size %d, nempty %d\n",
				OverallQueueSize, nempty);
		srSleep(0,100000);/* wait a little bit */
	}

	CHKiRet(sendResponse(pSess, "mainqueue empty\n"));
	DBGPRINTF("imdiag: mainqueue empty\n");

finalize_it:
	RETiRet;
}

static rsRetVal
awaitLookupTableReload(tcps_sess_t *pSess)
{
	DEFiRet;

	while(1) {
		if(lookupPendingReloadCount() == 0) {
			break;
		}
		srSleep(0,500000);
	}

	CHKiRet(sendResponse(pSess, "no pending lookup-table reloads found\n"));
	DBGPRINTF("imdiag: no pending lookup-table reloads found\n");

finalize_it:
	RETiRet;
}

static rsRetVal
enableDebug(tcps_sess_t *pSess)
{
	DEFiRet;

	Debug = DEBUG_FULL;
	debugging_on = 1;
	dbgprintf("Note: debug turned on via imdiag\n");

	CHKiRet(sendResponse(pSess, "debug enabled\n"));

finalize_it:
	RETiRet;
}

static void
imdiag_statsReadCallback(statsobj_t __attribute__((unused)) *const ignore_stats,
	void __attribute__((unused)) *const ignore_ctx)
{
	long long waitStartTimeMs = currentTimeMills();
	sem_wait(&statsReportingBlocker);
	long delta = currentTimeMills() - waitStartTimeMs;
	if ((int)ATOMIC_DEC_AND_FETCH(&allowOnlyOnce, &mutAllowOnlyOnce) < 0) {
		sem_post(&statsReportingBlocker);
	} else {
		LogError(0, RS_RET_OK, "imdiag(stats-read-callback): current stats-reporting "
						"cycle will proceed now, next reporting cycle will again be blocked");
	}

	if (pthread_mutex_lock(&mutStatsReporterWatch) == 0) {
		statsReported = 1;
		pthread_cond_signal(&statsReporterWatch);
		pthread_mutex_unlock(&mutStatsReporterWatch);
	}

	if (delta > 0) {
		STATSCOUNTER_ADD(actualArtificialDelayMs, mutActualArtificialDelayMs, delta);
	}
}

static rsRetVal
blockStatsReporting(tcps_sess_t *pSess) {
	DEFiRet;

	sem_wait(&statsReportingBlocker);
	CHKiConcCtrl(pthread_mutex_lock(&mutStatsReporterWatch));
	statsReported = 0;
	CHKiConcCtrl(pthread_mutex_unlock(&mutStatsReporterWatch));
	ATOMIC_STORE_0_TO_INT(&allowOnlyOnce, &mutAllowOnlyOnce);
	statsReportingBlockStartTimeMs = currentTimeMills();
	LogError(0, RS_RET_OK, "imdiag: blocked stats reporting");
	CHKiRet(sendResponse(pSess, "next stats reporting call will be blocked\n"));

finalize_it:
	if (iRet != RS_RET_OK) {
		LogError(0, iRet, "imdiag: block-stats-reporting wasn't successful");
		CHKiRet(sendResponse(pSess, "imdiag::error something went wrong\n"));
	}
	RETiRet;
}

static rsRetVal
awaitStatsReport(uchar *pszCmd, tcps_sess_t *pSess) {
	uchar subCmd[1024];
	int blockAgain = 0;
	DEFiRet;

	memset(subCmd, 0, sizeof(subCmd));
	getFirstWord(&pszCmd, subCmd, sizeof(subCmd), TO_LOWERCASE);
	blockAgain = (ustrcmp(UCHAR_CONSTANT("block_again"), subCmd) == 0);
	if (statsReportingBlockStartTimeMs > 0) {
		long delta = currentTimeMills() - statsReportingBlockStartTimeMs;
		if (blockAgain) {
			ATOMIC_STORE_1_TO_INT(&allowOnlyOnce, &mutAllowOnlyOnce);
			LogError(0, RS_RET_OK, "imdiag: un-blocking ONLY the next cycle of stats reporting");
		} else {
			statsReportingBlockStartTimeMs = 0;
			LogError(0, RS_RET_OK, "imdiag: un-blocking stats reporting");
		}
		sem_post(&statsReportingBlocker);
		LogError(0, RS_RET_OK, "imdiag: stats reporting unblocked");
		STATSCOUNTER_ADD(potentialArtificialDelayMs, mutPotentialArtificialDelayMs, delta);
		STATSCOUNTER_INC(delayInvocationCount, mutDelayInvocationCount);
		LogError(0, RS_RET_OK, "imdiag: will now await next reporting cycle");
		CHKiConcCtrl(pthread_mutex_lock(&mutStatsReporterWatch));
		while (! statsReported) {
			CHKiConcCtrl(pthread_cond_wait(&statsReporterWatch, &mutStatsReporterWatch));
		}
		statsReported = 0;
		CHKiConcCtrl(pthread_mutex_unlock(&mutStatsReporterWatch));
		if (blockAgain) {
			statsReportingBlockStartTimeMs = currentTimeMills();
		}
		LogError(0, RS_RET_OK, "imdiag: stats were reported, wait complete, returning");
		CHKiRet(sendResponse(pSess, "stats reporting was unblocked\n"));
	} else {
		CHKiRet(sendResponse(pSess, "imdiag::error : stats reporting was not blocked, bug?\n"));
	}

finalize_it:
	if (iRet != RS_RET_OK) {
		LogError(0, iRet, "imdiag: stats-reporting unblock + await-run wasn't successfully completed");
		CHKiRet(sendResponse(pSess, "imdiag::error something went wrong\n"));
	}
	RETiRet;
}

/* Function to handle received messages. This is our core function!
 * rgerhards, 2009-05-24
 */
static rsRetVal ATTR_NONNULL()
OnMsgReceived(tcps_sess_t *const pSess, uchar *const pRcv, const int iLenMsg)
{
	uchar *pszMsg;
	uchar *pToFree = NULL;
	uchar cmdBuf[1024];
	DEFiRet;

	assert(pSess != NULL);
	assert(pRcv != NULL);

	/* NOTE: pRcv is NOT a C-String but rather an array of characters
	 * WITHOUT a termination \0 char. So we need to convert it to one
	 * before proceeding.
	 */
	CHKmalloc(pszMsg = calloc(1, iLenMsg + 1));
	pToFree = pszMsg;
	memcpy(pszMsg, pRcv, iLenMsg);
	pszMsg[iLenMsg] = '\0';

	memset(cmdBuf, 0, sizeof(cmdBuf)); /* keep valgrind happy */
	getFirstWord(&pszMsg, cmdBuf, sizeof(cmdBuf), TO_LOWERCASE);

	dbgprintf("imdiag received command '%s'\n", cmdBuf);
	if(!ustrcmp(cmdBuf, UCHAR_CONSTANT("getmainmsgqueuesize"))) {
		CHKiRet(sendResponse(pSess, "%d\n", iOverallQueueSize));
		DBGPRINTF("imdiag: %d messages in main queue\n", iOverallQueueSize);
	} else if(!ustrcmp(cmdBuf, UCHAR_CONSTANT("waitmainqueueempty"))) {
		CHKiRet(waitMainQEmpty(pSess));
	} else if(!ustrcmp(cmdBuf, UCHAR_CONSTANT("awaitlookuptablereload"))) {
		CHKiRet(awaitLookupTableReload(pSess));
	} else if(!ustrcmp(cmdBuf, UCHAR_CONSTANT("injectmsg"))) {
		CHKiRet(injectMsg(pszMsg, pSess));
	} else if(!ustrcmp(cmdBuf, UCHAR_CONSTANT("blockstatsreporting"))) {
		CHKiRet(blockStatsReporting(pSess));
	} else if(!ustrcmp(cmdBuf, UCHAR_CONSTANT("awaitstatsreport"))) {
		CHKiRet(awaitStatsReport(pszMsg, pSess));
	} else if(!ustrcmp(cmdBuf, UCHAR_CONSTANT("enabledebug"))) {
		CHKiRet(enableDebug(pSess));
	} else {
		dbgprintf("imdiag unkown command '%s'\n", cmdBuf);
		CHKiRet(sendResponse(pSess, "unkown command '%s'\n", cmdBuf));
	}

finalize_it:
	free(pToFree);
	RETiRet;
}


/* set permitted peer -- rgerhards, 2008-05-19
 */
static rsRetVal
setPermittedPeer(void __attribute__((unused)) *pVal, uchar *pszID)
{
	DEFiRet;
	CHKiRet(net.AddPermittedPeer(&pPermPeersRoot, pszID));
	free(pszID); /* no longer needed, but we need to free as of interface def */
finalize_it:
	RETiRet;
}


static rsRetVal
setInjectDelayMode(void __attribute__((unused)) *pVal, uchar *const pszMode)
{
	DEFiRet;

	if(!strcasecmp((char*)pszMode, "no")) {
		injectmsgDelayMode = eFLOWCTL_NO_DELAY;
	} else if(!strcasecmp((char*)pszMode, "light")) {
		injectmsgDelayMode = eFLOWCTL_LIGHT_DELAY;
	} else if(!strcasecmp((char*)pszMode, "full")) {
		injectmsgDelayMode = eFLOWCTL_FULL_DELAY;
	} else {
		LogError(0, RS_RET_PARAM_ERROR,
			"imdiag: invalid imdiagInjectDelayMode '%s' - ignored", pszMode);
	}
	free(pszMode);
	RETiRet;
}


static rsRetVal
addTCPListener(void __attribute__((unused)) *pVal, uchar *pNewVal)
{
	tcpLstnParams_t *cnf_params = NULL;
	DEFiRet;

	if(pOurTcpsrv != NULL) {
		LogError(0, NO_ERRCODE, "imdiag: only a single listener is supported, "
			"trying to add a second");
		ABORT_FINALIZE(RS_RET_ERR);
	}
	CHKmalloc(cnf_params = (tcpLstnParams_t*) calloc(1, sizeof(tcpLstnParams_t)));
	CHKiRet(tcpsrv.Construct(&pOurTcpsrv));
	CHKiRet(tcpsrv.SetSessMax(pOurTcpsrv, iTCPSessMax));
	CHKiRet(tcpsrv.SetCBIsPermittedHost(pOurTcpsrv, isPermittedHost));
	CHKiRet(tcpsrv.SetCBRcvData(pOurTcpsrv, doRcvData));
	CHKiRet(tcpsrv.SetCBOpenLstnSocks(pOurTcpsrv, doOpenLstnSocks));
	CHKiRet(tcpsrv.SetCBOnRegularClose(pOurTcpsrv, onRegularClose));
	CHKiRet(tcpsrv.SetCBOnErrClose(pOurTcpsrv, onErrClose));
	CHKiRet(tcpsrv.SetDrvrMode(pOurTcpsrv, iStrmDrvrMode));
	CHKiRet(tcpsrv.SetOnMsgReceive(pOurTcpsrv, OnMsgReceived));
	/* now set optional params, but only if they were actually configured */
	if(pszStrmDrvrAuthMode != NULL) {
		CHKiRet(tcpsrv.SetDrvrAuthMode(pOurTcpsrv, pszStrmDrvrAuthMode));
	}
	if(pPermPeersRoot != NULL) {
		CHKiRet(tcpsrv.SetDrvrPermPeers(pOurTcpsrv, pPermPeersRoot));
	}

	/* initialized, now add socket */
	CHKiRet(tcpsrv.SetInputName(pOurTcpsrv, cnf_params, pszInputName == NULL ?
						UCHAR_CONSTANT("imdiag") : pszInputName));
	CHKiRet(tcpsrv.SetOrigin(pOurTcpsrv, (uchar*)"imdiag"));
	/* we support octect-counted frame (constant 1 below) */
	cnf_params->pszPort = pNewVal;
	cnf_params->bSuppOctetFram = 1;
	CHKmalloc(cnf_params->pszLstnPortFileName = (const uchar*) strdup((const char*)pszLstnPortFileName));
	tcpsrv.configureTCPListen(pOurTcpsrv, cnf_params);
	cnf_params = NULL;

finalize_it:
	if(iRet != RS_RET_OK) {
		LogError(0, NO_ERRCODE, "error %d trying to add listener", iRet);
		if(pOurTcpsrv != NULL)
			tcpsrv.Destruct(&pOurTcpsrv);
	}
	free(cnf_params);
	RETiRet;
}


static void *
timeoutGuard(ATTR_UNUSED void *arg)
{
	assert(abortTimeout != -1);
	sigset_t sigSet;
	time_t strtTO;
	time_t endTO;

	/* block all signals except SIGTTIN and SIGSEGV */
	sigfillset(&sigSet);
	sigdelset(&sigSet, SIGSEGV);
	pthread_sigmask(SIG_BLOCK, &sigSet, NULL);

	dbgprintf("timeoutGuard: timeout %d seconds, time %lld\n", abortTimeout, (long long) time(NULL));

	time(&strtTO);
	endTO = strtTO + abortTimeout;

	while(1) {
		int to = endTO - time(NULL);
		dbgprintf("timeoutGuard: sleep timeout %d seconds\n", to);
		if(to > 0) {
			srSleep(to, 0);
		}
		if(time(NULL) < endTO) {
			dbgprintf("timeoutGuard: spurios wakeup, going back to sleep, time: %lld\n",
				(long long) time(NULL));
		} else {
			break;
		}
	}
	dbgprintf("timeoutGuard: sleep expired, aborting\n");
	/* note: we use fprintf to stderr intentionally! */

	fprintf(stderr, "timeoutGuard: rsyslog still active after expiry of guard "
		"period (strtTO %lld, endTO %lld, time now %lld, diff %lld), pid %d - initiating abort()\n",
	(long long) strtTO, (long long) endTO, (long long) time(NULL), (long long) (time(NULL) - strtTO),
	(int) glblGetOurPid());
	fflush(stderr);
	abort();
}


static rsRetVal
setAbortTimeout(void __attribute__((unused)) *pVal, int timeout)
{
	DEFiRet;

	if(abortTimeout != -1) {
		LogError(0, NO_ERRCODE, "imdiag: abort timeout already set -"
			"ignoring 2nd+ request");
		ABORT_FINALIZE(RS_RET_ERR);
	}
	if(timeout <= 0) {
		LogError(0, NO_ERRCODE, "imdiag: $IMDiagAbortTimeout must be greater "
			"than 0 - ignored");
		ABORT_FINALIZE(RS_RET_ERR);
	}
	abortTimeout = timeout;
	const int iState = pthread_create(&timeoutGuard_thrd, NULL, timeoutGuard, NULL);
	if(iState != 0) {
		LogError(iState, NO_ERRCODE, "imdiag: error enabling timeoutGuard thread -"
			"not guarding against system hang");
		ABORT_FINALIZE(RS_RET_ERR);
	}

finalize_it:
	RETiRet;
}


#if 0 /* can be used to integrate into new config system */
BEGINbeginCnfLoad
CODESTARTbeginCnfLoad
ENDbeginCnfLoad


BEGINendCnfLoad
CODESTARTendCnfLoad
ENDendCnfLoad


BEGINcheckCnf
CODESTARTcheckCnf
ENDcheckCnf


BEGINactivateCnf
CODESTARTactivateCnf
ENDactivateCnf


BEGINfreeCnf
CODESTARTfreeCnf
ENDfreeCnf
#endif

/* This function is called to gather input.
 */
BEGINrunInput
CODESTARTrunInput
	CHKiRet(tcpsrv.ConstructFinalize(pOurTcpsrv));
	iRet = tcpsrv.Run(pOurTcpsrv);
finalize_it:
ENDrunInput


/* initialize and return if will run or not */
BEGINwillRun
CODESTARTwillRun
	/* first apply some config settings */
	if(pOurTcpsrv == NULL)
		ABORT_FINALIZE(RS_RET_NO_RUN);
	/* we need to create the inputName property (only once during our lifetime) */
	CHKiRet(prop.Construct(&pInputName));
	CHKiRet(prop.SetString(pInputName, UCHAR_CONSTANT("imdiag"), sizeof("imdiag") - 1));
	CHKiRet(prop.ConstructFinalize(pInputName));

	CHKiRet(prop.Construct(&pRcvDummy));
	CHKiRet(prop.SetString(pRcvDummy, UCHAR_CONSTANT("127.0.0.1"), sizeof("127.0.0.1") - 1));
	CHKiRet(prop.ConstructFinalize(pRcvDummy));

	CHKiRet(prop.Construct(&pRcvIPDummy));
	CHKiRet(prop.SetString(pRcvIPDummy, UCHAR_CONSTANT("127.0.0.1"), sizeof("127.0.0.1") - 1));
	CHKiRet(prop.ConstructFinalize(pRcvIPDummy));

finalize_it:
ENDwillRun


BEGINafterRun
CODESTARTafterRun
	if(pInputName != NULL)
		prop.Destruct(&pInputName);
	if(pRcvDummy != NULL)
		prop.Destruct(&pRcvDummy);
	if(pRcvIPDummy != NULL)
		prop.Destruct(&pRcvIPDummy);
ENDafterRun


BEGINmodExit
CODESTARTmodExit
	if(pOurTcpsrv != NULL)
		iRet = tcpsrv.Destruct(&pOurTcpsrv);

	if(pPermPeersRoot != NULL) {
		net.DestructPermittedPeers(&pPermPeersRoot);
	}

	/* free some globals to keep valgrind happy */
	free(pszInputName);
	free(pszLstnPortFileName);
	free(pszStrmDrvrAuthMode);

	statsobj.Destruct(&diagStats);
	sem_destroy(&statsReportingBlocker);
	DESTROY_ATOMIC_HELPER_MUT(mutAllowOnlyOnce);
	pthread_cond_destroy(&statsReporterWatch);
	pthread_mutex_destroy(&mutStatsReporterWatch);

	/* release objects we used */
	objRelease(net, LM_NET_FILENAME);
	objRelease(netstrm, LM_NETSTRMS_FILENAME);
	objRelease(tcps_sess, LM_TCPSRV_FILENAME);
	objRelease(tcpsrv, LM_TCPSRV_FILENAME);
	objRelease(datetime, CORE_COMPONENT);
	objRelease(prop, CORE_COMPONENT);
	objRelease(statsobj, CORE_COMPONENT);

	/* clean up timeoutGuard if active */
	if(abortTimeout != -1) {
		int r = pthread_cancel(timeoutGuard_thrd);
		if(r == 0) {
			void *dummy;
			pthread_join(timeoutGuard_thrd, &dummy);
		}
	}
ENDmodExit


static rsRetVal
resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	iTCPSessMax = 200;
	iStrmDrvrMode = 0;
	free(pszInputName);
	free(pszLstnPortFileName);
	pszLstnPortFileName = NULL;
	if(pszStrmDrvrAuthMode != NULL) {
		free(pszStrmDrvrAuthMode);
		pszStrmDrvrAuthMode = NULL;
	}
	return RS_RET_OK;
}


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURENonCancelInputTermination)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_IMOD_QUERIES
CODEqueryEtryPt_IsCompatibleWithFeature_IF_OMOD_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	pOurTcpsrv = NULL;
	/* request objects we use */
	CHKiRet(objUse(net, LM_NET_FILENAME));
	CHKiRet(objUse(netstrm, LM_NETSTRMS_FILENAME));
	CHKiRet(objUse(tcps_sess, LM_TCPSRV_FILENAME));
	CHKiRet(objUse(tcpsrv, LM_TCPSRV_FILENAME));
	CHKiRet(objUse(datetime, CORE_COMPONENT));
	CHKiRet(objUse(prop, CORE_COMPONENT));
	CHKiRet(objUse(statsobj, CORE_COMPONENT));

	const char *ci_max_empty_checks = getenv("CI_SHUTDOWN_QUEUE_EMPTY_CHECKS");
	if(ci_max_empty_checks != NULL) {
		int n = atoi(ci_max_empty_checks);
		if(n > 200) {
			LogError(0, RS_RET_PARAM_ERROR, "env var CI_SHUTDOWN_QUEUE_EMPTY_CHECKS has "
				"value over 200, which is the maximum - capped to 200");
			n = 200;
		}
		if(n > 0) {
			max_empty_checks = n;
		} else {
			LogError(0, RS_RET_PARAM_ERROR, "env var CI_SHUTDOWN_QUEUE_EMPTY_CHECKS has "
				"value below 1, ignored; using default instead");
		}
		fprintf(stderr, "rsyslogd: info: imdiag does %d empty checks due to "
			"CI_SHUTDOWN_QUEUE_EMPTY_CHECKS\n", max_empty_checks);
	}

	/* register config file handlers */
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("imdiagaborttimeout"), 0, eCmdHdlrInt,
				   setAbortTimeout, NULL, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("imdiagserverrun"), 0, eCmdHdlrGetWord,
				   addTCPListener, NULL, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("imdiaginjectdelaymode"), 0, eCmdHdlrGetWord,
				   setInjectDelayMode, NULL, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("imdiagmaxsessions"), 0, eCmdHdlrInt,
				   NULL, &iTCPSessMax, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("imdiagserverstreamdrivermode"), 0,
				   eCmdHdlrInt, NULL, &iStrmDrvrMode, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("imdiaglistenportfilename"), 0,
				   eCmdHdlrGetWord, NULL, &pszLstnPortFileName, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("imdiagserverstreamdriverauthmode"), 0,
				   eCmdHdlrGetWord, NULL, &pszStrmDrvrAuthMode, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("imdiagserverstreamdriverpermittedpeer"), 0,
				   eCmdHdlrGetWord, setPermittedPeer, NULL, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("imdiagserverinputname"), 0,
				   eCmdHdlrGetWord, NULL, &pszInputName, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("resetconfigvariables"), 1, eCmdHdlrCustomHandler,
							   resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));

	sem_init(&statsReportingBlocker, 0, 1);
	INIT_ATOMIC_HELPER_MUT(mutAllowOnlyOnce);
	CHKiConcCtrl(pthread_mutex_init(&mutStatsReporterWatch, NULL));
	CHKiConcCtrl(pthread_cond_init(&statsReporterWatch, NULL));

	CHKiRet(statsobj.Construct(&diagStats));
	CHKiRet(statsobj.SetName(diagStats, UCHAR_CONSTANT("imdiag-stats-reporting-controller")));
	CHKiRet(statsobj.SetOrigin(diagStats, UCHAR_CONSTANT("imdiag")));
	statsobj.SetStatsObjFlags(diagStats, STATSOBJ_FLAG_DO_PREPEND);
	STATSCOUNTER_INIT(potentialArtificialDelayMs, mutPotentialArtificialDelayMs);
	CHKiRet(statsobj.AddCounter(diagStats, UCHAR_CONSTANT("potentialTotalArtificialDelayInMs"),
						ctrType_IntCtr, CTR_FLAG_NONE, &potentialArtificialDelayMs));
	STATSCOUNTER_INIT(actualArtificialDelayMs, mutActualArtificialDelayMs);
	CHKiRet(statsobj.AddCounter(diagStats, UCHAR_CONSTANT("actualTotalArtificialDelayInMs"),
							ctrType_IntCtr, CTR_FLAG_NONE, &actualArtificialDelayMs));
	STATSCOUNTER_INIT(delayInvocationCount, mutDelayInvocationCount);
	CHKiRet(statsobj.AddCounter(diagStats, UCHAR_CONSTANT("delayInvocationCount"),
			ctrType_IntCtr, CTR_FLAG_NONE, &delayInvocationCount));
	CHKiRet(statsobj.SetReadNotifier(diagStats, imdiag_statsReadCallback, NULL));
	CHKiRet(statsobj.ConstructFinalize(diagStats));
ENDmodInit
