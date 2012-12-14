/**
 * \brief This is the main file of the rsyslogd daemon.
 *
 * Please visit the rsyslog project at
 *
 * http://www.rsyslog.com
 *
 * to learn more about it and discuss any questions you may have.
 *
 * rsyslog had initially been forked from the sysklogd project.
 * I would like to express my thanks to the developers of the sysklogd
 * package - without it, I would have had a much harder start...
 *
 * Please note that while rsyslog started from the sysklogd code base,
 * it nowadays has almost nothing left in common with it. Allmost all
 * parts of the code have been rewritten.
 *
 * This Project was intiated and is maintained by
 * Rainer Gerhards <rgerhards@hq.adiscon.com>.
 *
 * For further information, please see http://www.rsyslog.com
 *
 * rsyslog - An Enhanced syslogd Replacement.
 * Copyright 2003-2012 Rainer Gerhards and Adiscon GmbH.
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

#define DEFUPRI		(LOG_USER|LOG_NOTICE)

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <ctype.h>
#include <limits.h>
#define GNU_SOURCE
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <assert.h>

#ifdef OS_SOLARIS
#	include <errno.h>
#	include <fcntl.h>
#	include <stropts.h>
#	include <sys/termios.h>
#	include <sys/types.h>
#else
#	include <libgen.h>
#	include <sys/errno.h>
#endif

#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/resource.h>
#include <grp.h>

#if HAVE_SYS_TIMESPEC_H
# include <sys/timespec.h>
#endif

#if HAVE_SYS_STAT_H
#	include <sys/stat.h>
#endif

#include <signal.h>

#if HAVE_PATHS_H
#include <paths.h>
#endif

#ifdef USE_NETZIP
#include <zlib.h>
#endif

extern int yydebug; /* interface to flex */

#include <netdb.h>

#include "pidfile.h"
#include "srUtils.h"
#include "stringbuf.h"
#include "syslogd-types.h"
#include "template.h"
#include "outchannel.h"
#include "syslogd.h"

#include "msg.h"
#include "modules.h"
#include "action.h"
#include "iminternal.h"
#include "cfsysline.h"
#include "threads.h"
#include "wti.h"
#include "queue.h"
#include "stream.h"
#include "conf.h"
#include "errmsg.h"
#include "datetime.h"
#include "parser.h"
#include "batch.h"
#include "unicode-helper.h"
#include "ruleset.h"
#include "net.h"
#include "prop.h"
#include "rsconf.h"
#include "dnscache.h"
#include "sd-daemon.h"
#include "rainerscript.h"
#include "ratelimit.h"

/* definitions for objects we access */
DEFobjCurrIf(obj)
DEFobjCurrIf(glbl)
DEFobjCurrIf(datetime) /* TODO: make go away! */
DEFobjCurrIf(conf)
DEFobjCurrIf(module)
DEFobjCurrIf(errmsg)
DEFobjCurrIf(ruleset)
DEFobjCurrIf(prop)
DEFobjCurrIf(parser)
DEFobjCurrIf(rsconf)
DEFobjCurrIf(net) /* TODO: make go away! */


/* forward definitions */
static rsRetVal GlobalClassExit(void);
static rsRetVal queryLocalHostname(void);


#ifndef _PATH_LOGCONF 
#define _PATH_LOGCONF	"/etc/rsyslog.conf"
#endif

#ifndef _PATH_MODDIR
#       if defined(__FreeBSD__)
#               define _PATH_MODDIR     "/usr/local/lib/rsyslog/"
#       else
#               define _PATH_MODDIR     "/lib/rsyslog/"
#       endif
#endif

#if defined(SYSLOGD_PIDNAME)
#	undef _PATH_LOGPID
#	if defined(FSSTND)
#		ifdef OS_BSD
#			define _PATH_VARRUN "/var/run/"
#		endif
#		if defined(__sun) || defined(__hpux)
#			define _PATH_VARRUN "/var/run/"
#		endif
#		define _PATH_LOGPID _PATH_VARRUN SYSLOGD_PIDNAME
#	else
#		define _PATH_LOGPID "/etc/" SYSLOGD_PIDNAME
#	endif
#else
#	ifndef _PATH_LOGPID
#		if defined(__sun) || defined(__hpux)
#			define _PATH_VARRUN "/var/run/"
#		endif
#		if defined(FSSTND)
#			define _PATH_LOGPID _PATH_VARRUN "rsyslogd.pid"
#		else
#			define _PATH_LOGPID "/etc/rsyslogd.pid"
#		endif
#	endif
#endif

#ifndef _PATH_TTY
#	define _PATH_TTY	"/dev/tty"
#endif

rsconf_t *ourConf;				/* our config object */

static prop_t *pInternalInputName = NULL;	/* there is only one global inputName for all internally-generated messages */
static uchar	*ConfFile = (uchar*) _PATH_LOGCONF; /* read-only after startup */
static char	*PidFile = _PATH_LOGPID; /* read-only after startup */

static pid_t myPid;	/* our pid for use in self-generated messages, e.g. on startup */
/* mypid is read-only after the initial fork() */
static int bHadHUP = 0; /* did we have a HUP? */

static int bFinished = 0;	/* used by termination signal handler, read-only except there
				 * is either 0 or the number of the signal that requested the
 				 * termination.
				 */
int iConfigVerify = 0;	/* is this just a config verify run? */

#define LIST_DELIMITER	':'		/* delimiter between two hosts */

static pid_t ppid; /* This is a quick and dirty hack used for spliting main/startup thread */

struct queuefilenames_s {
	struct queuefilenames_s *next;
	uchar *name;
} *queuefilenames = NULL;


static ratelimit_t *dflt_ratelimiter = NULL; /* ratelimiter for submits without explicit one */
static ratelimit_t *internalMsg_ratelimiter = NULL; /* ratelimiter for rsyslog-own messages */
int	MarkInterval = 20 * 60;	/* interval between marks in seconds - read-only after startup */
int      send_to_all = 0;        /* send message to all IPv4/IPv6 addresses */
static int	NoFork = 0; 	/* don't fork - don't run in daemon mode - read-only after startup */
int	bHaveMainQueue = 0;/* set to 1 if the main queue - in queueing mode - is available
				 * If the main queue is either not yet ready or not running in 
				 * queueing mode (mode DIRECT!), then this is set to 0.
				 */

extern	int errno;

/* main message queue and its configuration parameters */
qqueue_t *pMsgQueue = NULL;				/* the main message queue */


/* up to the next comment, prototypes that should be removed by reordering */
/* Function prototypes. */
static char **crunch_list(char *list);
static void reapchild();
static void debug_switch();
static void sighup_handler();


static int usage(void)
{
	fprintf(stderr, "usage: rsyslogd [-46AdnqQvwx] [-l<hostlist>] [-s<domainlist>]\n"
			"                [-f<conffile>] [-i<pidfile>] [-N<level>] [-M<module load path>]\n"
			"                [-u<number>]\n"
			"For further information see http://www.rsyslog.com/doc\n");
	exit(1); /* "good" exit - done to terminate usage() */
}


/* ------------------------------ some support functions for imdiag ------------------------------ *
 * This is a bit dirty, but the only way to do it, at least with reasonable effort.
 * rgerhards, 2009-05-25
 */

/* return back the approximate current number of messages in the main message queue
 * This number includes the messages that reside in an associated DA queue (if
 * it exists) -- rgerhards, 2009-10-14
 */
rsRetVal
diagGetMainMsgQSize(int *piSize)
{
	DEFiRet;
	assert(piSize != NULL);
	*piSize = (pMsgQueue->pqDA != NULL) ? pMsgQueue->pqDA->iQueueSize : 0;
	*piSize += pMsgQueue->iQueueSize;
	RETiRet;
}


/* ------------------------------ end support functions for imdiag  ------------------------------ */


/* rgerhards, 2005-10-24: crunch_list is called only during option processing. So
 * it is never called once rsyslogd is running. This code
 * contains some exits, but they are considered safe because they only happen
 * during startup. Anyhow, when we review the code here, we might want to
 * reconsider the exit()s.
 */
static char **crunch_list(char *list)
{
	int count, i;
	char *p, *q;
	char **result = NULL;

	p = list;

	/* strip off trailing delimiters */
	while (p[strlen(p)-1] == LIST_DELIMITER) {
		count--;
		p[strlen(p)-1] = '\0';
	}
	/* cut off leading delimiters */
	while (p[0] == LIST_DELIMITER) {
		count--;
               p++;
	}

	/* count delimiters to calculate elements */
	for (count=i=0; p[i]; i++)
		if (p[i] == LIST_DELIMITER) count++;

	if ((result = (char **)MALLOC(sizeof(char *) * (count+2))) == NULL) {
		printf ("Sorry, can't get enough memory, exiting.\n");
		exit(0); /* safe exit, because only called during startup */
	}

	/*
	 * We now can assume that the first and last
	 * characters are different from any delimiters,
	 * so we don't have to care about this.
	 */
	count = 0;
	while ((q=strchr(p, LIST_DELIMITER))) {
		result[count] = (char *) MALLOC((q - p + 1) * sizeof(char));
		if (result[count] == NULL) {
			printf ("Sorry, can't get enough memory, exiting.\n");
			exit(0); /* safe exit, because only called during startup */
		}
		strncpy(result[count], p, q - p);
		result[count][q - p] = '\0';
		p = q; p++;
		count++;
	}
	if ((result[count] = \
	     (char *)MALLOC(sizeof(char) * strlen(p) + 1)) == NULL) {
		printf ("Sorry, can't get enough memory, exiting.\n");
		exit(0); /* safe exit, because only called during startup */
	}
	strcpy(result[count],p);
	result[++count] = NULL;

#if 0
	count=0;
	while (result[count])
		DBGPRINTF("#%d: %s\n", count, StripDomains[count++]);
#endif
	return result;
}


void untty(void)
#ifdef HAVE_SETSID
{
	if(!Debug) {
		setsid();
	}
	return;
}
#else
{
	int i;
	pid_t pid;

	if(!Debug) {
		pid = getpid();
		if (setpgid(pid, pid) < 0) {
			perror("setpgid");
			exit(1);
		}

		i = open(_PATH_TTY, O_RDWR|O_CLOEXEC);
		if (i >= 0) {
#			if !defined(__hpux)
				(void) ioctl(i, (int) TIOCNOTTY, NULL);
#			else
				/* TODO: we need to implement something for HP UX! -- rgerhards, 2008-03-04 */
				/* actually, HP UX should have setsid, so the code directly above should
				 * trigger. So the actual question is why it doesn't do that...
				 */
#			endif
			close(i);
		}
	}
}
#endif


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


/* this is a special function used to submit an error message. This
 * function is also passed to the runtime library as the generic error
 * message handler. -- rgerhards, 2008-04-17
 */
rsRetVal
submitErrMsg(int iErr, uchar *msg)
{
	DEFiRet;
	iRet = logmsgInternal(iErr, LOG_SYSLOG|LOG_ERR, msg, 0);
	RETiRet;
}


static inline rsRetVal
submitMsgWithDfltRatelimiter(msg_t *pMsg)
{
	return ratelimitAddMsg(dflt_ratelimiter, NULL, pMsg);
}

/* rgerhards 2004-11-09: the following is a function that can be used
 * to log a message orginating from the syslogd itself.
 */
rsRetVal
logmsgInternal(int iErr, int pri, uchar *msg, int flags)
{
	uchar pszTag[33];
	msg_t *pMsg;
	DEFiRet;

	CHKiRet(msgConstruct(&pMsg));
	MsgSetInputName(pMsg, pInternalInputName);
	MsgSetRawMsgWOSize(pMsg, (char*)msg);
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

	/* we now check if we should print internal messages out to stderr. This was
	 * suggested by HKS as a way to help people troubleshoot rsyslog configuration
	 * (by running it interactively. This makes an awful lot of sense, so I add
	 * it here. -- rgerhards, 2008-07-28
	 * Note that error messages can not be disable during a config verify. This
	 * permits us to process unmodified config files which otherwise contain a
	 * supressor statement.
	 */
	if(((Debug == DEBUG_FULL || NoFork) && ourConf->globals.bErrMsgToStderr) || iConfigVerify) {
		if(LOG_PRI(pri) == LOG_ERR)
			fprintf(stderr, "rsyslogd: %s\n", msg);
	}

	if(bHaveMainQueue == 0) { /* not yet in queued mode */
		iminternalAddMsg(pMsg);
	} else {
               /* we have the queue, so we can simply provide the
		 * message to the queue engine.
		 */
		ratelimitAddMsg(internalMsg_ratelimiter, NULL, pMsg);
		//submitMsgWithDfltRatelimiter(pMsg);
	}
finalize_it:
	RETiRet;
}


/* preprocess a batch of messages, that is ready them for actual processing. This is done
 * as a first stage and totally in parallel to any other worker active in the system. So
 * it helps us keep up the overall concurrency level.
 * rgerhards, 2010-06-09
 */
static inline rsRetVal
preprocessBatch(batch_t *pBatch) {
	uchar fromHost[NI_MAXHOST];
	uchar fromHostIP[NI_MAXHOST];
	uchar fromHostFQDN[NI_MAXHOST];
	prop_t *propFromHost = NULL;
	prop_t *propFromHostIP = NULL;
	int bSingleRuleset;
	ruleset_t *batchRuleset; /* the ruleset used for all message inside the batch, if there is a single one */
	int bIsPermitted;
	msg_t *pMsg;
	int i;
	rsRetVal localRet;
	DEFiRet;

	bSingleRuleset = 1;
	batchRuleset = (pBatch->nElem > 0) ? pBatch->pElem[0].pMsg->pRuleset : NULL;
	
	for(i = 0 ; i < pBatch->nElem  && !*(pBatch->pbShutdownImmediate) ; i++) {
		pMsg = pBatch->pElem[i].pMsg;
		if((pMsg->msgFlags & NEEDS_ACLCHK_U) != 0) {
			DBGPRINTF("msgConsumer: UDP ACL must be checked for message (hostname-based)\n");
			if(net.cvthname(pMsg->rcvFrom.pfrominet, fromHost, fromHostFQDN, fromHostIP) != RS_RET_OK)
				continue;
			bIsPermitted = net.isAllowedSender2((uchar*)"UDP",
			    (struct sockaddr *)pMsg->rcvFrom.pfrominet, (char*)fromHostFQDN, 1);
			if(!bIsPermitted) {
				DBGPRINTF("Message from '%s' discarded, not a permitted sender host\n",
					  fromHostFQDN);
				pBatch->pElem[i].state = BATCH_STATE_DISC;
			} else {
				/* save some of the info we obtained */
				MsgSetRcvFromStr(pMsg, fromHost, ustrlen(fromHost), &propFromHost);
				CHKiRet(MsgSetRcvFromIPStr(pMsg, fromHostIP, ustrlen(fromHostIP), &propFromHostIP));
				pMsg->msgFlags &= ~NEEDS_ACLCHK_U;
			}
		}
		if((pMsg->msgFlags & NEEDS_PARSING) != 0) {
			if((localRet = parser.ParseMsg(pMsg)) != RS_RET_OK)  {
				DBGPRINTF("Message discarded, parsing error %d\n", localRet);
				pBatch->pElem[i].state = BATCH_STATE_DISC;
			}
		}
		if(pMsg->pRuleset != batchRuleset)
			bSingleRuleset = 0;
	}

	batchSetSingleRuleset(pBatch, bSingleRuleset);

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
msgConsumer(void __attribute__((unused)) *notNeeded, batch_t *pBatch, int *pbShutdownImmediate)
{
	DEFiRet;
	assert(pBatch != NULL);
	pBatch->pbShutdownImmediate = pbShutdownImmediate; /* TODO: move this to batch creation! */
	preprocessBatch(pBatch);
	ruleset.ProcessBatch(pBatch);
//TODO: the BATCH_STATE_COMM must be set somewhere down the road, but we 
//do not have this yet and so we emulate -- 2010-06-10
int i;
	for(i = 0 ; i < pBatch->nElem  && !*pbShutdownImmediate ; i++) {
		pBatch->pElem[i].state = BATCH_STATE_COMM;
	}
	RETiRet;
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

rsRetVal
submitMsg(msg_t *pMsg)
{
	return submitMsgWithDfltRatelimiter(pMsg);
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


static void
reapchild()
{
	int saved_errno = errno;
	struct sigaction sigAct;

	memset(&sigAct, 0, sizeof (sigAct));
	sigemptyset(&sigAct.sa_mask);
	sigAct.sa_handler = reapchild;
	sigaction(SIGCHLD, &sigAct, NULL);  /* reset signal handler -ASP */

	while(waitpid(-1, NULL, WNOHANG) > 0);
	errno = saved_errno;
}


static void debug_switch()
{
	time_t tTime;
	struct tm tp;
	struct sigaction sigAct;

	datetime.GetTime(&tTime);
	localtime_r(&tTime, &tp);
	if(debugging_on == 0) {
		debugging_on = 1;
		dbgprintf("\n");
		dbgprintf("\n");
		dbgprintf("********************************************************************************\n");
		dbgprintf("Switching debugging_on to true at %2.2d:%2.2d:%2.2d\n",
			  tp.tm_hour, tp.tm_min, tp.tm_sec);
		dbgprintf("********************************************************************************\n");
	} else {
		dbgprintf("********************************************************************************\n");
		dbgprintf("Switching debugging_on to false at %2.2d:%2.2d:%2.2d\n",
			  tp.tm_hour, tp.tm_min, tp.tm_sec);
		dbgprintf("********************************************************************************\n");
		dbgprintf("\n");
		dbgprintf("\n");
		debugging_on = 0;
	}

	memset(&sigAct, 0, sizeof (sigAct));
	sigemptyset(&sigAct.sa_mask);
	sigAct.sa_handler = debug_switch;
	sigaction(SIGUSR1, &sigAct, NULL);
}


/* doDie() is a signal handler. If called, it sets the bFinished variable
 * to indicate the program should terminate. However, it does not terminate
 * it itself, because that causes issues with multi-threading. The actual
 * termination is then done on the main thread. This solution might introduce
 * a minimal delay, but it is much cleaner than the approach of doing everything
 * inside the signal handler.
 * rgerhards, 2005-10-26
 * Note:
 * - we do not call DBGPRINTF() as this may cause us to block in case something
 *   with the threading is wrong.
 * - we do not really care about the return state of write(), but we need this
 *   strange check we do to silence compiler warnings (thanks, Ubuntu!)
 */
static void doDie(int sig)
{
#	define MSG1 "DoDie called.\n"
#	define MSG2 "DoDie called 5 times - unconditional exit\n"
	static int iRetries = 0; /* debug aid */
	dbgprintf(MSG1);
	if(Debug == DEBUG_FULL) {
		if(write(1, MSG1, sizeof(MSG1) - 1)) {}
	}
	if(iRetries++ == 4) {
		if(Debug == DEBUG_FULL) {
			if(write(1, MSG2, sizeof(MSG2) - 1)) {}
		}
		abort();
	}
	bFinished = sig;
#	undef MSG1
#	undef MSG2
}


/* Finalize and destruct all actions.
 */
static inline void
destructAllActions(void)
{
	ruleset.DestructAllActions(runConf);
	bHaveMainQueue = 0; // flag that internal messages need to be temporarily stored
}


/* die() is called when the program shall end. This typically only occurs
 * during sigterm or during the initialization.
 * As die() is intended to shutdown rsyslogd, it is
 * safe to call exit() here. Just make sure that die() itself is not called
 * at inapropriate places. As a general rule of thumb, it is a bad idea to add
 * any calls to die() in new code!
 * rgerhards, 2005-10-24
 */
static void
die(int sig)
{
	char buf[256];

	DBGPRINTF("exiting on signal %d\n", sig);

	/* IMPORTANT: we should close the inputs first, and THEN send our termination
	 * message. If we do it the other way around, logmsgInternal() may block on
	 * a full queue and the inputs still fill up that queue. Depending on the
	 * scheduling order, we may end up with logmsgInternal being held for a quite
	 * long time. When the inputs are terminated first, that should not happen
	 * because the queue is drained in parallel. The situation could only become
	 * an issue with extremely long running actions in a queue full environment.
	 * However, such actions are at least considered poorly written, if not
	 * outright wrong. So we do not care about this very remote problem.
	 * rgerhards, 2008-01-11
	 */

	/* close the inputs */
	DBGPRINTF("Terminating input threads...\n");
	glbl.SetGlobalInputTermination();
	thrdTerminateAll();

	/* and THEN send the termination log message (see long comment above) */
	if(sig && runConf->globals.bLogStatusMsgs) {
		(void) snprintf(buf, sizeof(buf) / sizeof(char),
		 " [origin software=\"rsyslogd\" " "swVersion=\"" VERSION \
		 "\" x-pid=\"%d\" x-info=\"http://www.rsyslog.com\"]" " exiting on signal %d.",
		 (int) myPid, sig);
		errno = 0;
		logmsgInternal(NO_ERRCODE, LOG_SYSLOG|LOG_INFO, (uchar*)buf, 0);
	}
	/* we sleep for 50ms to give the queue a chance to pick up the exit message;
	 * otherwise we have seen cases where the message did not make it to log
	 * files, even on idle systems.
	 */
	srSleep(0, 50);

	/* drain queue (if configured so) and stop main queue worker thread pool */
	DBGPRINTF("Terminating main queue...\n");
	qqueueDestruct(&pMsgQueue);
	pMsgQueue = NULL;

	/* Free ressources and close connections. This includes flushing any remaining
	 * repeated msgs.
	 */
	DBGPRINTF("Terminating outputs...\n");
	destructAllActions();

	DBGPRINTF("all primary multi-thread sources have been terminated - now doing aux cleanup...\n");

	DBGPRINTF("destructing current config...\n");
	rsconf.Destruct(&runConf);

	/* rger 2005-02-22
	 * now clean up the in-memory structures. OK, the OS
	 * would also take care of that, but if we do it
	 * ourselfs, this makes finding memory leaks a lot
	 * easier.
	 */
	/* de-init some modules */
	modExitIminternal();

	/*dbgPrintAllDebugInfo(); / * this is the last spot where this can be done - below output modules are unloaded! */

	/* the following line cleans up CfSysLineHandlers that were not based on loadable
	 * modules. As such, they are not yet cleared.
	 */
	unregCfSysLineHdlrs();

	/* destruct our global properties */
	if(pInternalInputName != NULL)
		prop.Destruct(&pInternalInputName);

	/* terminate the remaining classes */
	GlobalClassExit();

	module.UnloadAndDestructAll(eMOD_LINK_ALL);

	DBGPRINTF("Clean shutdown completed, bye\n");
	/* dbgClassExit MUST be the last one, because it de-inits the debug system */
	dbgClassExit();

	/* NO CODE HERE - dbgClassExit() must be the last thing before exit()! */
	remove_pid(PidFile);
	exit(0); /* "good" exit, this is the terminator function for rsyslog [die()] */
}

/*
 * Signal handler to terminate the parent process.
 * rgerhards, 2005-10-24: this is only called during forking of the
 * detached syslogd. I consider this method to be safe.
 */
static void doexit()
{
	exit(0); /* "good" exit, only during child-creation */
}

#if 0 /* TODO: re-enable, currently not used */
/* helper to generateConfigDAG, to print out all actions via
 * the llExecFunc() facility.
 * rgerhards, 2007-08-02
 */
struct dag_info {
	FILE *fp;	/* output file */
	int iActUnit;	/* current action unit number */
	int iAct;	/* current action in unit */
	int bDiscarded;	/* message discarded (config error) */
	};
DEFFUNC_llExecFunc(generateConfigDAGAction)
{
	action_t *pAction;
	uchar *pszModName;
	uchar *pszVertexName;
	struct dag_info *pDagInfo;
	DEFiRet;

	pDagInfo = (struct dag_info*) pParam;
	pAction = (action_t*) pData;

	pszModName = module.GetStateName(pAction->pMod);

	/* vertex */
	if(pAction->pszName == NULL) {
		if(!strcmp((char*)pszModName, "builtin-discard"))
			pszVertexName = (uchar*)"discard";
		else
			pszVertexName = pszModName;
	} else {
		pszVertexName = pAction->pszName;
	}

	fprintf(pDagInfo->fp, "\tact%d_%d\t\t[label=\"%s\"%s%s]\n",
		pDagInfo->iActUnit, pDagInfo->iAct, pszVertexName,
		pDagInfo->bDiscarded ? " style=dotted color=red" : "",
		(pAction->pQueue->qType == QUEUETYPE_DIRECT) ? "" : " shape=hexagon"
		);

	/* edge */
	if(pDagInfo->iAct == 0) {
	} else {
		fprintf(pDagInfo->fp, "\tact%d_%d -> act%d_%d[%s%s]\n",
			pDagInfo->iActUnit, pDagInfo->iAct - 1,
			pDagInfo->iActUnit, pDagInfo->iAct,
			pDagInfo->bDiscarded ? " style=dotted color=red" : "",
			pAction->bExecWhenPrevSusp ? " label=\"only if\\nsuspended\"" : "" );
	}

	/* check for discard */
	if(!strcmp((char*) pszModName, "builtin-discard")) {
		fprintf(pDagInfo->fp, "\tact%d_%d\t\t[shape=box]\n",
			pDagInfo->iActUnit, pDagInfo->iAct);
		pDagInfo->bDiscarded = 1;
	}


	++pDagInfo->iAct;

	RETiRet;
}


/* create config DAG
 * This functions takes a rsyslog config and produces a .dot file for use
 * with graphviz (http://www.graphviz.org). This is done in an effort to
 * document, and also potentially troubleshoot, configurations. Plus, I
 * consider it a nice feature to explain some concepts. Note that the
 * current version only produces a graph with relatively little information.
 * This is a foundation that may be later expanded (if it turns out to be
 * useful enough).
 * rgerhards, 2009-05-11
 */
static rsRetVal
generateConfigDAG(uchar *pszDAGFile)
{
	//rule_t *f;
	FILE *fp;
	int iActUnit = 1;
	//int bHasFilter = 0;	/* filter associated with this action unit? */
	//int bHadFilter;
	//int i;
	struct dag_info dagInfo;
	//char *pszFilterName;
	char szConnectingNode[64];
	DEFiRet;

	assert(pszDAGFile != NULL);
	
	logmsgInternal(NO_ERRCODE, LOG_SYSLOG|LOG_INFO, (uchar*)
		"Configuration graph generation is unfortunately disabled "
		"in the current code base.", 0);
	ABORT_FINALIZE(RS_RET_FILENAME_INVALID);

	if((fp = fopen((char*) pszDAGFile, "w")) == NULL) {
		logmsgInternal(NO_ERRCODE, LOG_SYSLOG|LOG_INFO, (uchar*)
			"configuraton graph output file could not be opened, none generated", 0);
		ABORT_FINALIZE(RS_RET_FILENAME_INVALID);
	}

	dagInfo.fp = fp;

	/* from here on, we assume writes go well. This here is a really
	 * unimportant utility function and if something goes wrong, it has
	 * almost no effect. So let's not overdo this...
	 */
	fprintf(fp, "# graph created by rsyslog " VERSION "\n\n"
	 	    "# use the dot tool from http://www.graphviz.org to visualize!\n"
		    "digraph rsyslogConfig {\n"
		    "\tinputs [shape=tripleoctagon]\n"
		    "\tinputs -> act0_0\n"
		    "\tact0_0 [label=\"main\\nqueue\" shape=hexagon]\n"
		    /*"\tmainq -> act1_0\n"*/
		    );
	strcpy(szConnectingNode, "act0_0");
	dagInfo.bDiscarded = 0;

/* TODO: re-enable! */
#if 0
	for(f = Files; f != NULL ; f = f->f_next) {
		/* BSD-Style filters are currently ignored */
		bHadFilter = bHasFilter;
		if(f->f_filter_type == FILTER_PRI) {
			bHasFilter = 0;
			for (i = 0; i <= LOG_NFACILITIES; i++)
				if (f->f_filterData.f_pmask[i] != 0xff) {
					bHasFilter = 1;
					break;
				}
		} else {
			bHasFilter = 1;
		}

		/* we know we have a filter, so it can be false */
		switch(f->f_filter_type) {
			case FILTER_PRI:
				pszFilterName = "pri filter";
				break;
			case FILTER_PROP:
				pszFilterName = "property filter";
				break;
			case FILTER_EXPR:
				pszFilterName = "script filter";
				break;
		}

		/* write action unit node */
		if(bHasFilter) {
			fprintf(fp, "\t%s -> act%d_end\t[label=\"%s:\\nfalse\"]\n",
				szConnectingNode, iActUnit, pszFilterName);
			fprintf(fp, "\t%s -> act%d_0\t[label=\"%s:\\ntrue\"]\n",
				szConnectingNode, iActUnit, pszFilterName);
			fprintf(fp, "\tact%d_end\t\t\t\t[shape=point]\n", iActUnit);
			snprintf(szConnectingNode, sizeof(szConnectingNode), "act%d_end", iActUnit);
		} else {
			fprintf(fp, "\t%s -> act%d_0\t[label=\"no filter\"]\n",
				szConnectingNode, iActUnit);
			snprintf(szConnectingNode, sizeof(szConnectingNode), "act%d_0", iActUnit);
		}

		/* draw individual nodes */
		dagInfo.iActUnit = iActUnit;
		dagInfo.iAct = 0;
		dagInfo.bDiscarded = 0;
		llExecFunc(&f->llActList, generateConfigDAGAction, &dagInfo); /* actions */

		/* finish up */
		if(bHasFilter && !dagInfo.bDiscarded) {
			fprintf(fp, "\tact%d_%d -> %s\n",
				iActUnit, dagInfo.iAct - 1, szConnectingNode);
		}

		++iActUnit;
	}
#endif

	fprintf(fp, "\t%s -> act%d_0\n", szConnectingNode, iActUnit);
	fprintf(fp, "\tact%d_0\t\t[label=discard shape=box]\n"
		    "}\n", iActUnit);
	fclose(fp);

finalize_it:
	RETiRet;
}
#endif


/* create a main message queue, now also used for ruleset queues. This function
 * needs to be moved to some other module, but it is considered acceptable for
 * the time being (remember that we want to restructure config processing at large!).
 * rgerhards, 2009-10-27
 */
rsRetVal createMainQueue(qqueue_t **ppQueue, uchar *pszQueueName, struct cnfparamvals *queueParams)
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

	if(queueParams == NULL) { /* use legacy parameters? */
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
		qqueueApplyCnfParam(*ppQueue, queueParams);
	}

	/* ... and finally start the queue! */
	CHKiRet_Hdlr(qqueueStart(*ppQueue)) {
		/* no queue is fatal, we need to give up in that case... */
		errmsg.LogError(0, iRet, "could not start (ruleset) main message queue"); \
	}
	RETiRet;
}


/* INIT -- Initialize syslogd
 * Note that if iConfigVerify is set, only the config file is verified but nothing
 * else happens. -- rgerhards, 2008-07-28
 */
static rsRetVal
init(void)
{
	char bufStartUpMsg[512];
	struct sigaction sigAct;
	DEFiRet;

	memset(&sigAct, 0, sizeof (sigAct));
	sigemptyset(&sigAct.sa_mask);
	sigAct.sa_handler = sighup_handler;
	sigaction(SIGHUP, &sigAct, NULL);

	CHKiRet(rsconf.Activate(ourConf));
	DBGPRINTF(" started.\n");

	/* we now generate the startup message. It now includes everything to
	 * identify this instance. -- rgerhards, 2005-08-17
	 */
	if(ourConf->globals.bLogStatusMsgs) {
               snprintf(bufStartUpMsg, sizeof(bufStartUpMsg)/sizeof(char),
			 " [origin software=\"rsyslogd\" " "swVersion=\"" VERSION \
			 "\" x-pid=\"%d\" x-info=\"http://www.rsyslog.com\"] start",
			 (int) myPid);
		logmsgInternal(NO_ERRCODE, LOG_SYSLOG|LOG_INFO, (uchar*)bufStartUpMsg, 0);
	}

finalize_it:
	RETiRet;
}


/*
 * The following function is resposible for handling a SIGHUP signal.  Since
 * we are now doing mallocs/free as part of init we had better not being
 * doing this during a signal handler.  Instead this function simply sets
 * a flag variable which will tells the main loop to do "the right thing".
 */
void sighup_handler()
{
	struct sigaction sigAct;

	bHadHUP = 1;

	memset(&sigAct, 0, sizeof (sigAct));
	sigemptyset(&sigAct.sa_mask);
	sigAct.sa_handler = sighup_handler;
	sigaction(SIGHUP, &sigAct, NULL);
}

void sigttin_handler()
{
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
		submitMsgWithDfltRatelimiter(pMsg);
	}
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
			 (int) myPid);
			errno = 0;
		logmsgInternal(NO_ERRCODE, LOG_SYSLOG|LOG_INFO, (uchar*)buf, 0);
	}

	queryLocalHostname(); /* re-read our name */
	ruleset.IterateAllActions(ourConf, doHUPActions, NULL);
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

/* print version and compile-time setting information.
 */
static void printVersion(void)
{
	printf("rsyslogd %s, ", VERSION);
	printf("compiled with:\n");
#ifdef FEATURE_REGEXP
	printf("\tFEATURE_REGEXP:\t\t\t\tYes\n");
#else
	printf("\tFEATURE_REGEXP:\t\t\t\tNo\n");
#endif
#if defined(_LARGE_FILES) || (defined (_FILE_OFFSET_BITS) && _FILE_OFFSET_BITS >= 64)
	printf("\tFEATURE_LARGEFILE:\t\t\tYes\n");
#else
	printf("\tFEATURE_LARGEFILE:\t\t\tNo\n");
#endif
#if defined(SYSLOG_INET) && defined(USE_GSSAPI)
	printf("\tGSSAPI Kerberos 5 support:\t\tYes\n");
#else
	printf("\tGSSAPI Kerberos 5 support:\t\tNo\n");
#endif
#ifndef	NDEBUG
	printf("\tFEATURE_DEBUG (debug build, slow code):\tYes\n");
#else
	printf("\tFEATURE_DEBUG (debug build, slow code):\tNo\n");
#endif
#ifdef	HAVE_ATOMIC_BUILTINS
	printf("\t32bit Atomic operations supported:\tYes\n");
#else
	printf("\t32bit Atomic operations supported:\tNo\n");
#endif
#ifdef	HAVE_ATOMIC_BUILTINS_64BIT
	printf("\t64bit Atomic operations supported:\tYes\n");
#else
	printf("\t64bit Atomic operations supported:\tNo\n");
#endif
#ifdef	RTINST
	printf("\tRuntime Instrumentation (slow code):\tYes\n");
#else
	printf("\tRuntime Instrumentation (slow code):\tNo\n");
#endif
#ifdef	USE_LIBUUID
	printf("\tuuid support:\t\t\t\tYes\n");
#else
	printf("\tuuid support:\t\t\t\tNo\n");
#endif
	printf("\nSee http://www.rsyslog.com for more information.\n");
}


/* Method to initialize all global classes and use the objects that we need.
 * rgerhards, 2008-01-04
 * rgerhards, 2008-04-16: the actual initialization is now carried out by the runtime
 */
static rsRetVal
InitGlobalClasses(void)
{
	DEFiRet;
	char *pErrObj; /* tells us which object failed if that happens (useful for troubleshooting!) */

	/* Intialize the runtime system */
	pErrObj = "rsyslog runtime"; /* set in case the runtime errors before setting an object */
	CHKiRet(rsrtInit(&pErrObj, &obj));
	CHKiRet(rsrtSetErrLogger(submitErrMsg)); /* set out error handler */

	/* Now tell the system which classes we need ourselfs */
	pErrObj = "glbl";
	CHKiRet(objUse(glbl,     CORE_COMPONENT));
	pErrObj = "errmsg";
	CHKiRet(objUse(errmsg,   CORE_COMPONENT));
	pErrObj = "module";
	CHKiRet(objUse(module,   CORE_COMPONENT));
	pErrObj = "datetime";
	CHKiRet(objUse(datetime, CORE_COMPONENT));
	pErrObj = "ruleset";
	CHKiRet(objUse(ruleset,  CORE_COMPONENT));
	pErrObj = "conf";
	CHKiRet(objUse(conf,     CORE_COMPONENT));
	pErrObj = "prop";
	CHKiRet(objUse(prop,     CORE_COMPONENT));
	pErrObj = "parser";
	CHKiRet(objUse(parser,     CORE_COMPONENT));
	pErrObj = "rsconf";
	CHKiRet(objUse(rsconf,     CORE_COMPONENT));

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

finalize_it:
	if(iRet != RS_RET_OK) {
		/* we know we are inside the init sequence, so we can safely emit
		 * messages to stderr. -- rgerhards, 2008-04-02
		 */
		fprintf(stderr, "Error during class init for object '%s' - failing...\n", pErrObj);
	}

	RETiRet;
}


/* Method to exit all global classes. We do not do any error checking here,
 * because that wouldn't help us at all. So better try to deinit blindly
 * as much as succeeds (which usually means everything will). We just must
 * be careful to do the de-init in the opposite order of the init, because
 * of the dependencies. However, its not as important this time, because
 * we have reference counting.
 * rgerhards, 2008-03-10
 */
static rsRetVal
GlobalClassExit(void)
{
	DEFiRet;

	/* first, release everything we used ourself */
	objRelease(net,      LM_NET_FILENAME);/* TODO: the dependency on net shall go away! -- rgerhards, 2008-03-07 */
	objRelease(prop,     CORE_COMPONENT);
	objRelease(conf,     CORE_COMPONENT);
	objRelease(ruleset,  CORE_COMPONENT);
	parserClassExit();					/* this is hack, currently core_modules do not get this automatically called */
	rsconfClassExit();					/* this is hack, currently core_modules do not get this automatically called */
	objRelease(datetime, CORE_COMPONENT);

	/* TODO: implement the rest of the deinit */
	/* dummy "classes */
	strExit();
	ratelimitModExit();

#if 0
	CHKiRet(objGetObjInterface(&obj)); /* this provides the root pointer for all other queries */
	/* the following classes were intialized by objClassInit() */
	CHKiRet(objUse(errmsg,   CORE_COMPONENT));
	CHKiRet(objUse(module,   CORE_COMPONENT));
#endif
	dnscacheDeinit();
	rsrtExit(); /* *THIS* *MUST/SHOULD?* always be the first class initilizer being called (except debug)! */

	RETiRet;
}


/* query our host and domain names - we need to do this early as we may emit
 * rgerhards, 2012-04-11
 */
static rsRetVal
queryLocalHostname(void)
{
	uchar *LocalHostName;
	uchar *LocalDomain;
	uchar *LocalFQDNName;
	uchar *p;
	struct hostent *hent;
	DEFiRet;

	net.getLocalHostname(&LocalFQDNName);
	CHKmalloc(LocalHostName = (uchar*) strdup((char*)LocalFQDNName));
	glbl.SetLocalFQDNName(LocalFQDNName); /* set the FQDN before we modify it */
	if((p = (uchar*)strchr((char*)LocalHostName, '.'))) {
		*p++ = '\0';
		LocalDomain = p;
	} else {
		LocalDomain = (uchar*)"";

		/* It's not clearly defined whether gethostname()
		 * should return the simple hostname or the fqdn. A
		 * good piece of software should be aware of both and
		 * we want to distribute good software.  Joey
		 *
		 * Good software also always checks its return values...
		 * If syslogd starts up before DNS is up & /etc/hosts
		 * doesn't have LocalHostName listed, gethostbyname will
                * return NULL.
		 */
		/* TODO: gethostbyname() is not thread-safe, but replacing it is
		 * not urgent as we do not run on multiple threads here. rgerhards, 2007-09-25
		 */
		hent = gethostbyname((char*)LocalHostName);
		if(hent) {
			int i = 0;

			if(hent->h_aliases) {
				size_t hnlen;

				hnlen = strlen((char *) LocalHostName);

				for (i = 0; hent->h_aliases[i]; i++) {
					if (!strncmp(hent->h_aliases[i], (char *) LocalHostName, hnlen)
					    && hent->h_aliases[i][hnlen] == '.') {
						/* found a matching hostname */
						break;
					}
				}
			}

			free(LocalHostName);
			if(hent->h_aliases && hent->h_aliases[i]) {
				CHKmalloc(LocalHostName = (uchar*)strdup(hent->h_aliases[i]));
			} else {
				CHKmalloc(LocalHostName = (uchar*)strdup(hent->h_name));
			}

			if((p = (uchar*)strchr((char*)LocalHostName, '.')))
			{
				*p++ = '\0';
				LocalDomain = p;
			}
		}
	}

	/* LocalDomain is "" or part of LocalHostName, allocate a new string */
	CHKmalloc(LocalDomain = (uchar*)strdup((char*)LocalDomain));

	/* Convert to lower case to recognize the correct domain laterly */
	for(p = LocalDomain ; *p ; p++)
		*p = (char)tolower((int)*p);

	/* we now have our hostname and can set it inside the global vars.
	 * TODO: think if all of this would better be a runtime function
	 * rgerhards, 2008-04-17
	 */
	glbl.SetLocalHostName(LocalHostName);
	glbl.SetLocalDomain(LocalDomain);
	glbl.GenerateLocalHostNameProperty(); /* must be redone after conf processing, FQDN setting may have changed */
finalize_it:
	RETiRet;
}


/* some support for command line option parsing. Any non-trivial options must be
 * buffered until the complete command line has been parsed. This is necessary to
 * prevent dependencies between the options. That, in turn, means we need to have
 * something that is capable of buffering options and there values. The follwing
 * functions handle that.
 * rgerhards, 2008-04-04
 */
typedef struct bufOpt {
	struct bufOpt *pNext;
	char optchar;
	char *arg;
} bufOpt_t;
static bufOpt_t *bufOptRoot = NULL;
static bufOpt_t *bufOptLast = NULL;

/* add option buffer */
static rsRetVal
bufOptAdd(char opt, char *arg)
{
	DEFiRet;
	bufOpt_t *pBuf;

	if((pBuf = MALLOC(sizeof(bufOpt_t))) == NULL)
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

	pBuf->optchar = opt;
	pBuf->arg = arg;
	pBuf->pNext = NULL;

	if(bufOptLast == NULL) {
		bufOptRoot = pBuf; /* then there is also no root! */
	} else {
		bufOptLast->pNext = pBuf;
	}
	bufOptLast = pBuf;

finalize_it:
	RETiRet;
}



/* remove option buffer from top of list, return values and destruct buffer itself.
 * returns RS_RET_END_OF_LINKEDLIST when no more options are present.
 * (we use int *opt instead of char *opt to keep consistent with getopt())
 */
static rsRetVal
bufOptRemove(int *opt, char **arg)
{
	DEFiRet;
	bufOpt_t *pBuf;

	if(bufOptRoot == NULL)
		ABORT_FINALIZE(RS_RET_END_OF_LINKEDLIST);
	pBuf = bufOptRoot;

	*opt = pBuf->optchar;
	*arg = pBuf->arg;

	bufOptRoot = pBuf->pNext;
	free(pBuf);

finalize_it:
	RETiRet;
}


/* global initialization, to be done only once and before the mainloop is started.
 * rgerhards, 2008-07-28 (extracted from realMain())
 */
static rsRetVal
doGlblProcessInit(void)
{
	struct sigaction sigAct;
	int num_fds;
	int i;
	DEFiRet;

	thrdInit();

	if( !(Debug == DEBUG_FULL || NoFork) )
	{
		DBGPRINTF("Checking pidfile '%s'.\n", PidFile);
		if (!check_pid(PidFile))
		{
			memset(&sigAct, 0, sizeof (sigAct));
			sigemptyset(&sigAct.sa_mask);
			sigAct.sa_handler = doexit;
			sigaction(SIGTERM, &sigAct, NULL);

			/* stop writing debug messages to stdout (if debugging is on) */
			stddbg = -1;

			if (fork()) {
				/* Parent process
				 */
				sleep(300);
				/* Not reached unless something major went wrong.  5
				 * minutes should be a fair amount of time to wait.
				 * Please note that this procedure is important since
				 * the father must not exit before syslogd isn't
				 * initialized or the klogd won't be able to flush its
				 * logs.  -Joey
				 */
				exit(1); /* "good" exit - after forking, not diasabling anything */
			}

			num_fds = getdtablesize();
			close(0);
			/* we keep stdout and stderr open in case we have to emit something */
			i = 3;

			/* if (sd_booted()) */ {
				const char *e;
				char buf[24] = { '\0' };
				char *p = NULL;
				unsigned long l;
				int sd_fds;

				/* fork & systemd socket activation:
				 * fetch listen pid and update to ours,
				 * when it is set to pid of our parent.
				 */
				if ( (e = getenv("LISTEN_PID"))) {
					errno = 0;
					l = strtoul(e, &p, 10);
					if (errno ==  0 && l > 0 && (!p || !*p)) {
						if (getppid() == (pid_t)l) {
							snprintf(buf, sizeof(buf), "%d",
								 getpid());
							setenv("LISTEN_PID", buf, 1);
						}
					}
				}

				/*
				 * close only all further fds, except
				 * of the fds provided by systemd.
				 */
				sd_fds = sd_listen_fds(0);
				if (sd_fds > 0)
					i = SD_LISTEN_FDS_START + sd_fds;
			}
			for ( ; i < num_fds; i++)
				(void) close(i);

			untty();
		} else {
			fputs(" Already running. If you want to run multiple instances, you need "
			      "to specify different pid files (use -i option)\n", stderr);
			exit(1); /* "good" exit, done if syslogd is already running */
		}
	}

	/* tuck my process id away */
	DBGPRINTF("Writing pidfile '%s'.\n", PidFile);
	if (!check_pid(PidFile))
	{
		if (!write_pid(PidFile))
		{
			fputs("Can't write pid.\n", stderr);
			exit(1); /* exit during startup - questionable */
		}
	}
	else
	{
		fputs("Pidfile (and pid) already exist.\n", stderr);
		exit(1); /* exit during startup - questionable */
	}
	myPid = getpid(); 	/* save our pid for further testing (also used for messages) */

	memset(&sigAct, 0, sizeof (sigAct));
	sigemptyset(&sigAct.sa_mask);

	sigAct.sa_handler = sigsegvHdlr;
	sigaction(SIGSEGV, &sigAct, NULL);
	sigAct.sa_handler = sigsegvHdlr;
	sigaction(SIGABRT, &sigAct, NULL);
	sigAct.sa_handler = doDie;
	sigaction(SIGTERM, &sigAct, NULL);
	sigAct.sa_handler = Debug ? doDie : SIG_IGN;
	sigaction(SIGINT, &sigAct, NULL);
	sigaction(SIGQUIT, &sigAct, NULL);
	sigAct.sa_handler = reapchild;
	sigaction(SIGCHLD, &sigAct, NULL);
	sigAct.sa_handler = Debug ? debug_switch : SIG_IGN;
	sigaction(SIGUSR1, &sigAct, NULL);
	sigAct.sa_handler = sigttin_handler;
	sigaction(SIGTTIN, &sigAct, NULL); /* (ab)used to interrupt input threads */
	sigAct.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sigAct, NULL);
	sigaction(SIGXFSZ, &sigAct, NULL); /* do not abort if 2gig file limit is hit */

	RETiRet;
}


/* This is the main entry point into rsyslogd. Over time, we should try to
 * modularize it a bit more...
 */
int realMain(int argc, char **argv)
{
	rsRetVal localRet;
	int ch;
	extern int optind;
	extern char *optarg;
	int bEOptionWasGiven = 0;
	int iHelperUOpt;
	int bChDirRoot = 1; /* change the current working directory to "/"? */
	char *arg;	/* for command line option processing */
	char cwdbuf[128]; /* buffer to obtain/display current working directory */
	DEFiRet;

	/* first, parse the command line options. We do not carry out any actual work, just
	 * see what we should do. This relieves us from certain anomalies and we can process
	 * the parameters down below in the correct order. For example, we must know the
	 * value of -M before we can do the init, but at the same time we need to have
	 * the base classes init before we can process most of the options. Now, with the
	 * split of functionality, this is no longer a problem. Thanks to varmofekoj for
	 * suggesting this algo.
	 * Note: where we just need to set some flags and can do so without knowledge
	 * of other options, we do this during the inital option processing.
	 * rgerhards, 2008-04-04
	 */
	while((ch = getopt(argc, argv, "46a:Ac:dDef:g:hi:l:m:M:nN:op:qQr::s:t:T:u:vwx")) != EOF) {
		switch((char)ch) {
                case '4':
                case '6':
                case 'A':
                case 'a':
		case 'f': /* configuration file */
		case 'h':
		case 'i': /* pid file name */
		case 'l':
		case 'm': /* mark interval */
		case 'n': /* don't fork */
		case 'N': /* enable config verify mode */
                case 'o':
                case 'p':
		case 'q': /* add hostname if DNS resolving has failed */
		case 'Q': /* dont resolve hostnames in ACL to IPs */
		case 's':
		case 'T': /* chroot on startup (primarily for testing) */
		case 'u': /* misc user settings */
		case 'w': /* disable disallowed host warnings */
		case 'x': /* disable dns for remote messages */
		case 'g': /* enable tcp gssapi logging */
		case 'r': /* accept remote messages */
		case 't': /* enable tcp logging */
			CHKiRet(bufOptAdd(ch, optarg));
			break;
		case 'c':		/* compatibility mode */
			fprintf(stderr, "rsyslogd: error: option -c is no longer supported - ignored\n");
			break;
		case 'd': /* debug - must be handled now, so that debug is active during init! */
			debugging_on = 1;
			Debug = 1;
			yydebug = 1;
			break;
		case 'D': /* BISON debug */
			yydebug = 1;
			break;
		case 'e':		/* log every message (no repeat message supression) */
			bEOptionWasGiven = 1;
			break;
		case 'M': /* default module load path -- this MUST be carried out immediately! */
			glblModPath = (uchar*) optarg;
			break;
		case 'v': /* MUST be carried out immediately! */
			printVersion();
			exit(0); /* exit for -v option - so this is a "good one" */
		case '?':
		default:
			usage();
		}
	}

	if(argc - optind)
		usage();

	DBGPRINTF("rsyslogd %s startup, module path '%s', cwd:%s\n",
		  VERSION, glblModPath == NULL ? "" : (char*)glblModPath,
		  getcwd(cwdbuf, sizeof(cwdbuf)));

	/* we are done with the initial option parsing and processing. Now we init the system. */

	ppid = getpid();

	CHKiRet_Hdlr(InitGlobalClasses()) {
		fprintf(stderr, "rsyslogd initializiation failed - global classes could not be initialized.\n"
				"Did you do a \"make install\"?\n"
				"Suggested action: run rsyslogd with -d -n options to see what exactly "
				"fails.\n");
		FINALIZE;
	}

	/* doing some core initializations */

	/* we need to create the inputName property (only once during our lifetime) */
	CHKiRet(prop.Construct(&pInternalInputName));
	CHKiRet(prop.SetString(pInternalInputName, UCHAR_CONSTANT("rsyslogd"), sizeof("rsyslogd") - 1));
	CHKiRet(prop.ConstructFinalize(pInternalInputName));

	/* get our host and domain names - we need to do this early as we may emit
	 * error log messages, which need the correct hostname. -- rgerhards, 2008-04-04
	 */
	queryLocalHostname();

	/* initialize the objects */
	if((iRet = modInitIminternal()) != RS_RET_OK) {
		fprintf(stderr, "fatal error: could not initialize errbuf object (error code %d).\n",
			iRet);
		exit(1); /* "good" exit, leaving at init for fatal error */
	}


	/* END core initializations - we now come back to carrying out command line options*/

	while((iRet = bufOptRemove(&ch, &arg)) == RS_RET_OK) {
		DBGPRINTF("deque option %c, optarg '%s'\n", ch, (arg == NULL) ? "" : arg);
		switch((char)ch) {
                case '4':
	                glbl.SetDefPFFamily(PF_INET);
                        break;
                case '6':
                        glbl.SetDefPFFamily(PF_INET6);
                        break;
                case 'A':
                        send_to_all++;
                        break;
                case 'a':
			fprintf(stderr, "rsyslogd: error -a is no longer supported, use module imuxsock instead");
                        break;
		case 'f':		/* configuration file */
			ConfFile = (uchar*) arg;
			break;
		case 'g':		/* enable tcp gssapi logging */
			fprintf(stderr,	"rsyslogd: -g option no longer supported - ignored\n");
		case 'h':
			fprintf(stderr, "rsyslogd: error -h is no longer supported - ignored");
			break;
		case 'i':		/* pid file name */
			PidFile = arg;
			break;
		case 'l':
			if(glbl.GetLocalHosts() != NULL) {
				fprintf (stderr, "rsyslogd: Only one -l argument allowed, the first one is taken.\n");
			} else {
				glbl.SetLocalHosts(crunch_list(arg));
			}
			break;
		case 'm':		/* mark interval */
			fprintf(stderr, "rsyslogd: error -m is no longer supported - use immark instead");
			break;
		case 'n':		/* don't fork */
			NoFork = 1;
			break;
		case 'N':		/* enable config verify mode */
			iConfigVerify = atoi(arg);
			break;
                case 'o':
			fprintf(stderr, "error -o is no longer supported, use module imuxsock instead");
                        break;
                case 'p':
			fprintf(stderr, "error -p is no longer supported, use module imuxsock instead");
			break;
		case 'q':               /* add hostname if DNS resolving has failed */
		        *(net.pACLAddHostnameOnFail) = 1;
		        break;
		case 'Q':               /* dont resolve hostnames in ACL to IPs */
		        *(net.pACLDontResolve) = 1;
		        break;
		case 'r':		/* accept remote messages */
			fprintf(stderr, "rsyslogd: error option -r is no longer supported - ignored");
			break;
		case 's':
			if(glbl.GetStripDomains() != NULL) {
				fprintf (stderr, "rsyslogd: Only one -s argument allowed, the first one is taken.\n");
			} else {
				glbl.SetStripDomains(crunch_list(arg));
			}
			break;
		case 't':		/* enable tcp logging */
			fprintf(stderr, "rsyslogd: error option -t is no longer supported - ignored");
			break;
		case 'T':/* chroot() immediately at program startup, but only for testing, NOT security yet */
			if(chroot(arg) != 0) {
				perror("chroot");
				exit(1);
			}
			break;
		case 'u':		/* misc user settings */
			iHelperUOpt = atoi(arg);
			if(iHelperUOpt & 0x01)
				glbl.SetParseHOSTNAMEandTAG(0);
			if(iHelperUOpt & 0x02)
				bChDirRoot = 0;
			break;
		case 'w':		/* disable disallowed host warnigs */
			glbl.SetOption_DisallowWarning(0);
			break;
		case 'x':		/* disable dns for remote messages */
			glbl.SetDisableDNS(1);
			break;
               case '?':
		default:
			usage();
		}
	}

	if(iRet != RS_RET_END_OF_LINKEDLIST)
		FINALIZE;

	if(iConfigVerify) {
		fprintf(stderr, "rsyslogd: version %s, config validation run (level %d), master config %s\n",
			VERSION, iConfigVerify, ConfFile);
	}

	localRet = rsconf.Load(&ourConf, ConfFile);
	queryLocalHostname();	/* need to re-query to pick up a changed hostname due to config */

	if(localRet == RS_RET_NONFATAL_CONFIG_ERR) {
		if(loadConf->globals.bAbortOnUncleanConfig) {
			fprintf(stderr, "rsyslogd: $AbortOnUncleanConfig is set, and config is not clean.\n"
					"Check error log for details, fix errors and restart. As a last\n"
					"resort, you may want to remove $AbortOnUncleanConfig to permit a\n"
					"startup with a dirty config.\n");
			exit(2);
		}
		if(iConfigVerify) {
			/* a bit dirty, but useful... */
			exit(1);
		}
		localRet = RS_RET_OK;
	}
	CHKiRet(localRet);

	CHKiRet(ratelimitNew(&dflt_ratelimiter, "rsyslogd", "dflt"));
	/* TODO: add linux-type limiting capability */
	CHKiRet(ratelimitNew(&internalMsg_ratelimiter, "rsyslogd", "internal_messages"));
	ratelimitSetLinuxLike(internalMsg_ratelimiter, 5, 500);
	/* TODO: make internalMsg ratelimit settings configurable */

	if(bChDirRoot) {
		if(chdir("/") != 0)
			fprintf(stderr, "Can not do 'cd /' - still trying to run\n");
	}

	/* process compatibility mode settings */
	if(bEOptionWasGiven) {
		errmsg.LogError(0, NO_ERRCODE, "WARNING: \"message repeated n times\" feature MUST be turned on in "
					    "rsyslog.conf - CURRENTLY EVERY MESSAGE WILL BE LOGGED. Visit "
					    "http://www.rsyslog.com/rptdmsgreduction to learn "
					    "more and cast your vote if you want us to keep this feature.");
	}

	if(!iConfigVerify)
		CHKiRet(doGlblProcessInit());

	CHKiRet(init());

	if(Debug && debugging_on) {
		dbgprintf("Debugging enabled, SIGUSR1 to turn off debugging.\n");
	}

	/* Send a signal to the parent so it can terminate.  */
	if(myPid != ppid)
		kill(ppid, SIGTERM);


	/* END OF INTIALIZATION */
	DBGPRINTF("initialization completed, transitioning to regular run mode\n");

	/* close stderr and stdout if they are kept open during a fork. Note that this
	 * may introduce subtle security issues: if we are in a jail, one may break out of
	 * it via these descriptors. But if I close them earlier, error messages will (once
	 * again) not be emitted to the user that starts the daemon. As root jail support
	 * is still in its infancy (and not really done), we currently accept this issue.
	 * rgerhards, 2009-06-29
	 */
	if(!(Debug == DEBUG_FULL || NoFork)) {
		close(1);
		close(2);
		ourConf->globals.bErrMsgToStderr = 0;
	}

	mainloop();

	/* do any de-init's that need to be done AFTER this comment */

	die(bFinished);

	thrdExit();

finalize_it:
	if(iRet == RS_RET_VALIDATION_RUN) {
		fprintf(stderr, "rsyslogd: End of config validation run. Bye.\n");
	} else if(iRet != RS_RET_OK) {
		fprintf(stderr, "rsyslogd: run failed with error %d (see rsyslog.h "
				"or try http://www.rsyslog.com/e/%d to learn what that number means)\n", iRet, iRet*-1);
		exit(1);
	}

	ENDfunc
	return 0;
}


/* This is the main entry point into rsyslogd. This must be a function in its own
 * right in order to intialize the debug system in a portable way (otherwise we would
 * need to have a statement before variable definitions.
 * rgerhards, 20080-01-28
 */
int main(int argc, char **argv)
{
	dbgClassInit();
	return realMain(argc, argv);
}
/* vim:set ai:
 */
