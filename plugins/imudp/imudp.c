/* imudp.c
 * This is the implementation of the UDP input module.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * File begun on 2007-12-21 by RGerhards (extracted from syslogd.c)
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
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#if HAVE_SYS_EPOLL_H
#	include <sys/epoll.h>
#endif
#ifdef HAVE_SCHED_H
#	include <sched.h>
#endif
#include "rsyslog.h"
#include "dirty.h"
#include "net.h"
#include "cfsysline.h"
#include "module-template.h"
#include "srUtils.h"
#include "errmsg.h"
#include "glbl.h"
#include "msg.h"
#include "parser.h"
#include "datetime.h"
#include "prop.h"
#include "ruleset.h"
#include "unicode-helper.h"

MODULE_TYPE_INPUT
MODULE_TYPE_NOKEEP

/* defines */

/* Module static data */
DEF_IMOD_STATIC_DATA
DEFobjCurrIf(errmsg)
DEFobjCurrIf(glbl)
DEFobjCurrIf(net)
DEFobjCurrIf(datetime)
DEFobjCurrIf(prop)
DEFobjCurrIf(ruleset)

static int bDoACLCheck;			/* are ACL checks neeed? Cached once immediately before listener startup */
static int iMaxLine;			/* maximum UDP message size supported */
static time_t ttLastDiscard = 0;	/* timestamp when a message from a non-permitted sender was last discarded
					 * This shall prevent remote DoS when the "discard on disallowed sender"
					 * message is configured to be logged on occurance of such a case.
					 */
static int *udpLstnSocks = NULL;	/* Internet datagram sockets, first element is nbr of elements
					 * read-only after init(), but beware of restart! */
static ruleset_t **udpRulesets = NULL;	/* ruleset to be used with sockets in question (entry 0 is empty) */
static uchar *pszBindAddr = NULL;	/* IP to bind socket to */
static uchar *pRcvBuf = NULL;		/* receive buffer (for a single packet). We use a global and alloc
					 * it so that we can check available memory in willRun() and request
					 * termination if we can not get it. -- rgerhards, 2007-12-27
					 */
static prop_t *pInputName = NULL;	/* our inputName currently is always "imudp", and this will hold it */
static uchar *pszSchedPolicy = NULL;	/* scheduling policy string */
static int iSchedPolicy;		/* scheduling policy as SCHED_xxx */
static int iSchedPrio;			/* scheduling priority */
static int seen_iSchedPrio = 0;		/* have we seen scheduling priority in the config file? */
static ruleset_t *pBindRuleset = NULL;	/* ruleset to bind listener to (use system default if unspecified) */
#define TIME_REQUERY_DFLT 2
static int iTimeRequery = TIME_REQUERY_DFLT;/* how often is time to be queried inside tight recv loop? 0=always */

/* config settings */

static rsRetVal check_scheduling_priority(int report_error)
{
    	DEFiRet;

#ifdef HAVE_SCHED_GET_PRIORITY_MAX
	if (iSchedPrio < sched_get_priority_min(iSchedPolicy) ||
	    iSchedPrio > sched_get_priority_max(iSchedPolicy)) {
	    	if (report_error)
		    	errmsg.LogError(errno, NO_ERRCODE,
				"imudp: scheduling priority %d out of range (%d - %d)"
				" for scheduling policy '%s' - ignoring settings",
				iSchedPrio,
				sched_get_priority_min(iSchedPolicy),
				sched_get_priority_max(iSchedPolicy),
				pszSchedPolicy);
		ABORT_FINALIZE(RS_RET_VALIDATION_RUN);
	}
#endif

finalize_it:
	RETiRet;
}

/* Set scheduling priority in the supplied variable (will be iSchedPrio)
 * and record that we have seen the directive (in seen_iSchedPrio).
 */
static rsRetVal set_scheduling_priority(void *pVal, int value)
{
	DEFiRet;

	if (seen_iSchedPrio) {
		errmsg.LogError(0, NO_ERRCODE, "directive already seen");
		ABORT_FINALIZE(RS_RET_VALIDATION_RUN);
	}
	*(int *)pVal = value;
	seen_iSchedPrio = 1;
	if (pszSchedPolicy != NULL)
	    	CHKiRet(check_scheduling_priority(1));

finalize_it:
	RETiRet;
}

/* Set scheduling policy in iSchedPolicy */
static rsRetVal set_scheduling_policy(void *pVal, uchar *pNewVal)
{
    	int have_sched_policy = 0;
	DEFiRet;

	if (pszSchedPolicy != NULL) {
	    	errmsg.LogError(0, NO_ERRCODE, "directive already seen");
		ABORT_FINALIZE(RS_RET_VALIDATION_RUN);
	}
	*((uchar**)pVal) = pNewVal;	/* pVal is pszSchedPolicy */
	if (0) { /* trick to use conditional compilation */
#ifdef SCHED_FIFO
	} else if (!strcasecmp((char*)pszSchedPolicy, "fifo")) {
		iSchedPolicy = SCHED_FIFO;
		have_sched_policy = 1;
#endif
#ifdef SCHED_RR
	} else if (!strcasecmp((char*)pszSchedPolicy, "rr")) {
		iSchedPolicy = SCHED_RR;
		have_sched_policy = 1;
#endif
#ifdef SCHED_OTHER
	} else if (!strcasecmp((char*)pszSchedPolicy, "other")) {
		iSchedPolicy = SCHED_OTHER;
		have_sched_policy = 1;
#endif
	} else {
		errmsg.LogError(errno, NO_ERRCODE,
			    "imudp: invalid scheduling policy '%s' "
			    "- ignoring setting", pszSchedPolicy);
	}
	if (have_sched_policy == 0) {
	    	free(pszSchedPolicy);
		pszSchedPolicy = NULL;
		ABORT_FINALIZE(RS_RET_VALIDATION_RUN);
	}
	if (seen_iSchedPrio)
	    	CHKiRet(check_scheduling_priority(1));

finalize_it:
	RETiRet;
}


/* This function is called when a new listener shall be added. It takes
 * the configured parameters, tries to bind the socket and, if that
 * succeeds, adds it to the list of existing listen sockets.
 * rgerhards, 2007-12-27
 */
static rsRetVal addListner(void __attribute__((unused)) *pVal, uchar *pNewVal)
{
	DEFiRet;
	uchar *bindAddr;
	int *newSocks;
	int *tmpSocks;
	int iSrc, iDst;
	ruleset_t **tmpRulesets;

	/* check which address to bind to. We could do this more compact, but have not
	 * done so in order to make the code more readable. -- rgerhards, 2007-12-27
	 */
	if(pszBindAddr == NULL)
		bindAddr = NULL;
	else if(pszBindAddr[0] == '*' && pszBindAddr[1] == '\0')
		bindAddr = NULL;
	else
		bindAddr = pszBindAddr;

	DBGPRINTF("Trying to open syslog UDP ports at %s:%s.\n",
		  (bindAddr == NULL) ? (uchar*)"*" : bindAddr, pNewVal);

	newSocks = net.create_udp_socket(bindAddr, (pNewVal == NULL || *pNewVal == '\0') ? (uchar*) "514" : pNewVal, 1);
	if(newSocks != NULL) {
		/* we now need to add the new sockets to the existing set */
		if(udpLstnSocks == NULL) {
			/* esay, we can just replace it */
			udpLstnSocks = newSocks;
			CHKmalloc(udpRulesets = (ruleset_t**) MALLOC(sizeof(ruleset_t*) * (newSocks[0] + 1)));
			for(iDst = 1 ; iDst <= newSocks[0] ; ++iDst)
				udpRulesets[iDst] = pBindRuleset;
		} else {
			/* we need to add them */
			tmpSocks = (int*) MALLOC(sizeof(int) * (1 + newSocks[0] + udpLstnSocks[0]));
			tmpRulesets = (ruleset_t**) MALLOC(sizeof(ruleset_t*) * (1 + newSocks[0] + udpLstnSocks[0]));
			if(tmpSocks == NULL || tmpRulesets == NULL) {
				DBGPRINTF("out of memory trying to allocate udp listen socket array\n");
				/* in this case, we discard the new sockets but continue with what we
				 * already have
				 */
				free(newSocks);
				free(tmpSocks);
				free(tmpRulesets);
				ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
			} else {
				/* ready to copy */
				iDst = 1;
				for(iSrc = 1 ; iSrc <= udpLstnSocks[0] ; ++iSrc, ++iDst) {
					tmpSocks[iDst] = udpLstnSocks[iSrc];
					tmpRulesets[iDst] = udpRulesets[iSrc];
				}
				for(iSrc = 1 ; iSrc <= newSocks[0] ; ++iSrc, ++iDst) {
					tmpSocks[iDst] = newSocks[iSrc];
					tmpRulesets[iDst] = pBindRuleset;
				}
				tmpSocks[0] = udpLstnSocks[0] + newSocks[0];
				free(newSocks);
				free(udpLstnSocks);
				udpLstnSocks = tmpSocks;
				free(udpRulesets);
				udpRulesets = tmpRulesets;
			}
		}
	}

finalize_it:
	free(pNewVal); /* in any case, this is no longer needed */

	RETiRet;
}


/* accept a new ruleset to bind. Checks if it exists and complains, if not */
static rsRetVal
setRuleset(void __attribute__((unused)) *pVal, uchar *pszName)
{
	ruleset_t *pRuleset;
	rsRetVal localRet;
	DEFiRet;

	localRet = ruleset.GetRuleset(&pRuleset, pszName);
	if(localRet == RS_RET_NOT_FOUND) {
		errmsg.LogError(0, NO_ERRCODE, "error: ruleset '%s' not found - ignored", pszName);
	}
	CHKiRet(localRet);
	pBindRuleset = pRuleset;
	DBGPRINTF("imudp current bind ruleset %p: '%s'\n", pRuleset, pszName);

finalize_it:
	free(pszName); /* no longer needed */
	RETiRet;
}


/* This function is a helper to runInput. I have extracted it
 * from the main loop just so that we do not have that large amount of code
 * in a single place. This function takes a socket and pulls messages from
 * it until the socket does not have any more waiting.
 * rgerhards, 2008-01-08
 * We try to read from the file descriptor until there
 * is no more data. This is done in the hope to get better performance
 * out of the system. However, this also means that a descriptor
 * monopolizes processing while it contains data. This can lead to
 * data loss in other descriptors. However, if the system is incapable of
 * handling the workload, we will loss data in any case. So it doesn't really
 * matter where the actual loss occurs - it is always random, because we depend
 * on scheduling order. -- rgerhards, 2008-10-02
 */
static inline rsRetVal
processSocket(thrdInfo_t *pThrd, int fd, struct sockaddr_storage *frominetPrev, int *pbIsPermitted,
	      ruleset_t *pRuleset)
{
	DEFiRet;
	int iNbrTimeUsed;
	time_t ttGenTime;
	struct syslogTime stTime;
	socklen_t socklen;
	ssize_t lenRcvBuf;
	struct sockaddr_storage frominet;
	msg_t *pMsg;
	prop_t *propFromHost = NULL;
	prop_t *propFromHostIP = NULL;
	char errStr[1024];

	assert(pThrd != NULL);
	iNbrTimeUsed = 0;
	while(1) { /* loop is terminated if we have a bad receive, done below in the body */
		if(pThrd->bShallStop == TRUE)
			ABORT_FINALIZE(RS_RET_FORCE_TERM);
		socklen = sizeof(struct sockaddr_storage);
		lenRcvBuf = recvfrom(fd, (char*) pRcvBuf, iMaxLine, 0, (struct sockaddr *)&frominet, &socklen);
		if(lenRcvBuf < 0) {
			if(errno != EINTR && errno != EAGAIN) {
				rs_strerror_r(errno, errStr, sizeof(errStr));
				DBGPRINTF("INET socket error: %d = %s.\n", errno, errStr);
				errmsg.LogError(errno, NO_ERRCODE, "recvfrom inet");
			}
			ABORT_FINALIZE(RS_RET_ERR); // this most often is NOT an error, state is not checked by caller!
		}

		if(lenRcvBuf == 0)
			continue; /* this looks a bit strange, but practice shows it happens... */

		/* if we reach this point, we had a good receive and can process the packet received */
		/* check if we have a different sender than before, if so, we need to query some new values */
		if(bDoACLCheck) {
			if(net.CmpHost(&frominet, frominetPrev, socklen) != 0) {
				memcpy(frominetPrev, &frominet, socklen); /* update cache indicator */
				/* Here we check if a host is permitted to send us syslog messages. If it isn't,
				 * we do not further process the message but log a warning (if we are
				 * configured to do this). However, if the check would require name resolution,
				 * it is postponed to the main queue. See also my blog post at
				 * http://blog.gerhards.net/2009/11/acls-imudp-and-accepting-messages.html
				 * rgerhards, 2009-11-16
				 */
				*pbIsPermitted = net.isAllowedSender2((uchar*)"UDP",
						    (struct sockaddr *)&frominet, "", 0);
		
				if(*pbIsPermitted == 0) {
					DBGPRINTF("msg is not from an allowed sender\n");
					if(glbl.GetOption_DisallowWarning) {
						time_t tt;
						datetime.GetTime(&tt);
						if(tt > ttLastDiscard + 60) {
							ttLastDiscard = tt;
							errmsg.LogError(0, NO_ERRCODE,
							"UDP message from disallowed sender discarded");
						}
					}
				}
			}
		} else {
			*pbIsPermitted = 1; /* no check -> everything permitted */
		}

		DBGPRINTF("recv(%d,%d),acl:%d,msg:%s\n", fd, (int) lenRcvBuf, *pbIsPermitted, pRcvBuf);

		if(*pbIsPermitted != 0)  {
			if((iTimeRequery == 0) || (iNbrTimeUsed++ % iTimeRequery) == 0) {
				datetime.getCurrTime(&stTime, &ttGenTime);
			}
			/* we now create our own message object and submit it to the queue */
			CHKiRet(msgConstructWithTime(&pMsg, &stTime, ttGenTime));
			MsgSetRawMsg(pMsg, (char*)pRcvBuf, lenRcvBuf);
			MsgSetInputName(pMsg, pInputName);
			MsgSetRuleset(pMsg, pRuleset);
			MsgSetFlowControlType(pMsg, eFLOWCTL_NO_DELAY);
			pMsg->msgFlags  = NEEDS_PARSING | PARSE_HOSTNAME | NEEDS_DNSRESOL;
			if(*pbIsPermitted == 2)
				pMsg->msgFlags  |= NEEDS_ACLCHK_U; /* request ACL check after resolution */
			CHKiRet(msgSetFromSockinfo(pMsg, &frominet));
			CHKiRet(submitMsg(pMsg));
		}
	}

finalize_it:
	if(propFromHost != NULL)
		prop.Destruct(&propFromHost);
	if(propFromHostIP != NULL)
		prop.Destruct(&propFromHostIP);

	RETiRet;
}

static void set_thread_schedparam(void)
{
	struct sched_param sparam;

	if (pszSchedPolicy != NULL && seen_iSchedPrio == 0) {
		errmsg.LogError(0, NO_ERRCODE,
			"imudp: scheduling policy set, but without priority - ignoring settings");
	} else if (pszSchedPolicy == NULL && seen_iSchedPrio != 0) {
		errmsg.LogError(0, NO_ERRCODE,
			"imudp: scheduling priority set, but without policy - ignoring settings");
	} else if (pszSchedPolicy != NULL && seen_iSchedPrio != 0 &&
		   check_scheduling_priority(0) == 0) {
#ifndef HAVE_PTHREAD_SETSCHEDPARAM
	    	errmsg.LogError(0, NO_ERRCODE,
			"imudp: cannot set thread scheduling policy, "
			"pthread_setschedparam() not available");
#else
		int err;

		memset(&sparam, 0, sizeof sparam);
		sparam.sched_priority = iSchedPrio;
		dbgprintf("imudp trying to set sched policy to '%s', prio %d\n",
			  pszSchedPolicy, iSchedPrio);
		err = pthread_setschedparam(pthread_self(), iSchedPolicy, &sparam);
		if (err != 0) {
			errmsg.LogError(err, NO_ERRCODE, "imudp: pthread_setschedparam() failed");
		}
#endif
	}

	if (pszSchedPolicy != NULL) {
	    	free(pszSchedPolicy);
		pszSchedPolicy = NULL;
	}
}

/* This function implements the main reception loop. Depending on the environment,
 * we either use the traditional (but slower) select() or the Linux-specific epoll()
 * interface. ./configure settings control which one is used.
 * rgerhards, 2009-09-09
 */
#if defined(HAVE_EPOLL_CREATE1) || defined(HAVE_EPOLL_CREATE)
#define NUM_EPOLL_EVENTS 10
rsRetVal rcvMainLoop(thrdInfo_t *pThrd)
{
	DEFiRet;
	int nfds;
	int efd;
	int i;
	struct sockaddr_storage frominetPrev;
	int bIsPermitted;
	struct epoll_event *udpEPollEvt = NULL;
	struct epoll_event currEvt[NUM_EPOLL_EVENTS];
	char errStr[1024];

	/* start "name caching" algo by making sure the previous system indicator
	 * is invalidated.
	 */
	set_thread_schedparam();
	bIsPermitted = 0;
	memset(&frominetPrev, 0, sizeof(frominetPrev));

	CHKmalloc(udpEPollEvt = calloc(udpLstnSocks[0], sizeof(struct epoll_event)));

#if defined(EPOLL_CLOEXEC) && defined(HAVE_EPOLL_CREATE1)
	DBGPRINTF("imudp uses epoll_create1()\n");
	efd = epoll_create1(EPOLL_CLOEXEC);
	if(efd < 0 && errno == ENOSYS)
#endif
	{
		DBGPRINTF("imudp uses epoll_create()\n");
		efd = epoll_create(NUM_EPOLL_EVENTS);
	}

	if(efd < 0) {
		DBGPRINTF("epoll_create1() could not create fd\n");
		ABORT_FINALIZE(RS_RET_IO_ERROR);
	}

	/* fill the epoll set - we need to do this only once, as the set
	 * can not change dyamically.
	 */
	for (i = 0; i < *udpLstnSocks; i++) {
		if (udpLstnSocks[i+1] != -1) {
			udpEPollEvt[i].events = EPOLLIN | EPOLLET;
			udpEPollEvt[i].data.u64 = i+1;
			if(epoll_ctl(efd, EPOLL_CTL_ADD,  udpLstnSocks[i+1], &(udpEPollEvt[i])) < 0) {
				rs_strerror_r(errno, errStr, sizeof(errStr));
				errmsg.LogError(errno, NO_ERRCODE, "epoll_ctrl failed on fd %d with %s\n",
					udpLstnSocks[i+1], errStr);
			}
		}
	}

	while(1) {
		/* wait for io to become ready */
		nfds = epoll_wait(efd, currEvt, NUM_EPOLL_EVENTS, -1);
		DBGPRINTF("imudp: epoll_wait() returned with %d fds\n", nfds);

		if(pThrd->bShallStop == TRUE)
			break; /* terminate input! */

		for(i = 0 ; i < nfds ; ++i) {
			processSocket(pThrd, udpLstnSocks[currEvt[i].data.u64], &frominetPrev, &bIsPermitted,
				      udpRulesets[currEvt[i].data.u64]);
		}
	}

finalize_it:
	if(udpEPollEvt != NULL)
		free(udpEPollEvt);

	RETiRet;
}
#else /* #if HAVE_EPOLL_CREATE1 */
/* this is the code for the select() interface */
rsRetVal rcvMainLoop(thrdInfo_t *pThrd)
{
	DEFiRet;
	int maxfds;
	int nfds;
	int i;
	fd_set readfds;
	struct sockaddr_storage frominetPrev;
	int bIsPermitted;

	/* start "name caching" algo by making sure the previous system indicator
	 * is invalidated.
	 */
	set_thread_schedparam();
	bIsPermitted = 0;
	memset(&frominetPrev, 0, sizeof(frominetPrev));
	DBGPRINTF("imudp uses select()\n");

	while(1) {
		/* Add the Unix Domain Sockets to the list of read
		 * descriptors.
		 * rgerhards 2005-08-01: we must now check if there are
		 * any local sockets to listen to at all. If the -o option
		 * is given without -a, we do not need to listen at all..
		 */
	        maxfds = 0;
	        FD_ZERO(&readfds);

		/* Add the UDP listen sockets to the list of read descriptors. */
		for (i = 0; i < *udpLstnSocks; i++) {
			if (udpLstnSocks[i+1] != -1) {
				if(Debug)
					net.debugListenInfo(udpLstnSocks[i+1], "UDP");
				FD_SET(udpLstnSocks[i+1], &readfds);
				if(udpLstnSocks[i+1]>maxfds) maxfds=udpLstnSocks[i+1];
			}
		}
		if(Debug) {
			dbgprintf("--------imUDP calling select, active file descriptors (max %d): ", maxfds);
			for (nfds = 0; nfds <= maxfds; ++nfds)
				if(FD_ISSET(nfds, &readfds))
					dbgprintf("%d ", nfds);
			dbgprintf("\n");
		}

		/* wait for io to become ready */
		nfds = select(maxfds+1, (fd_set *) &readfds, NULL, NULL, NULL);
		if(glbl.GetGlobalInputTermState() == 1)
			break; /* terminate input! */

	       for(i = 0; nfds && i < *udpLstnSocks; i++) {
			if(FD_ISSET(udpLstnSocks[i+1], &readfds)) {
		       		processSocket(pThrd, udpLstnSocks[i+1], &frominetPrev, &bIsPermitted,
					      udpRulesets[i+1]);
			--nfds; /* indicate we have processed one descriptor */
			}
	       }
	       /* end of a run, back to loop for next recv() */
	}

	RETiRet;
}
#endif /* #if HAVE_EPOLL_CREATE1 */

/* This function is called to gather input.
 * Note that udpLstnSocks must be non-NULL because otherwise we would not have
 * indicated that we want to run (or we have a programming error ;)). -- rgerhards, 2008-10-02
 */
BEGINrunInput
CODESTARTrunInput
	iRet = rcvMainLoop(pThrd);
ENDrunInput


/* initialize and return if will run or not */
BEGINwillRun
CODESTARTwillRun
	/* we need to create the inputName property (only once during our lifetime) */
	CHKiRet(prop.Construct(&pInputName));
	CHKiRet(prop.SetString(pInputName, UCHAR_CONSTANT("imudp"), sizeof("imudp") - 1));
	CHKiRet(prop.ConstructFinalize(pInputName));

	net.PrintAllowedSenders(1); /* UDP */
	net.HasRestrictions(UCHAR_CONSTANT("UDP"), &bDoACLCheck); /* UDP */

	/* if we could not set up any listners, there is no point in running... */
	if(udpLstnSocks == NULL)
		ABORT_FINALIZE(RS_RET_NO_RUN);

	iMaxLine = glbl.GetMaxLine();

	CHKmalloc(pRcvBuf = MALLOC((iMaxLine + 1) * sizeof(char)));
finalize_it:
ENDwillRun


BEGINafterRun
CODESTARTafterRun
	/* do cleanup here */
	net.clearAllowedSenders((uchar*)"UDP");
	if(udpLstnSocks != NULL) {
		net.closeUDPListenSockets(udpLstnSocks);
		udpLstnSocks = NULL;
		free(udpRulesets);
		udpRulesets = NULL;
	}
	if(pRcvBuf != NULL) {
		free(pRcvBuf);
		pRcvBuf = NULL;
	}
	if(pInputName != NULL)
		prop.Destruct(&pInputName);
ENDafterRun


BEGINmodExit
CODESTARTmodExit
	/* release what we no longer need */
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(glbl, CORE_COMPONENT);
	objRelease(datetime, CORE_COMPONENT);
	objRelease(prop, CORE_COMPONENT);
	objRelease(ruleset, CORE_COMPONENT);
	objRelease(net, LM_NET_FILENAME);
ENDmodExit


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

static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	if(pszBindAddr != NULL) {
		free(pszBindAddr);
		pszBindAddr = NULL;
	}
	iTimeRequery = TIME_REQUERY_DFLT;/* the default is to query only every second time */
	return RS_RET_OK;
}


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(datetime, CORE_COMPONENT));
	CHKiRet(objUse(prop, CORE_COMPONENT));
	CHKiRet(objUse(ruleset, CORE_COMPONENT));
	CHKiRet(objUse(net, LM_NET_FILENAME));

	/* register config file handlers */
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputudpserverbindruleset", 0, eCmdHdlrGetWord,
		setRuleset, NULL, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"udpserverrun", 0, eCmdHdlrGetWord,
		addListner, NULL, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"udpserveraddress", 0, eCmdHdlrGetWord,
		NULL, &pszBindAddr, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"imudpschedulingpolicy", 0, eCmdHdlrGetWord,
		&set_scheduling_policy, NULL, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"imudpschedulingpriority", 0, eCmdHdlrInt,
		&set_scheduling_priority, NULL, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"udpservertimerequery", 0, eCmdHdlrInt,
		NULL, &iTimeRequery, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler,
		resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit
/* vim:set ai:
 */
