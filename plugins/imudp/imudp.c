/* imudp.c
 * This is the implementation of the UDP input module.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * File begun on 2007-12-21 by RGerhards (extracted from syslogd.c)
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
#include <stdio.h>
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
#include "statsobj.h"
#include "unicode-helper.h"

MODULE_TYPE_INPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("imudp")

/* defines */

/* Module static data */
DEF_IMOD_STATIC_DATA
DEFobjCurrIf(errmsg)
DEFobjCurrIf(glbl)
DEFobjCurrIf(net)
DEFobjCurrIf(datetime)
DEFobjCurrIf(prop)
DEFobjCurrIf(ruleset)
DEFobjCurrIf(statsobj)


static struct lstn_s {
	struct lstn_s *next;
	int sock;		/* socket */
	ruleset_t *pRuleset;	/* bound ruleset */
	statsobj_t *stats;	/* listener stats */
	STATSCOUNTER_DEF(ctrSubmit, mutCtrSubmit)
} *lcnfRoot = NULL, *lcnfLast = NULL;

static int bDoACLCheck;			/* are ACL checks neeed? Cached once immediately before listener startup */
static int iMaxLine;			/* maximum UDP message size supported */
static time_t ttLastDiscard = 0;	/* timestamp when a message from a non-permitted sender was last discarded
					 * This shall prevent remote DoS when the "discard on disallowed sender"
					 * message is configured to be logged on occurance of such a case.
					 */
static uchar *pRcvBuf = NULL;		/* receive buffer (for a single packet). We use a global and alloc
					 * it so that we can check available memory in willRun() and request
					 * termination if we can not get it. -- rgerhards, 2007-12-27
					 */
static prop_t *pInputName = NULL;	/* our inputName currently is always "imudp", and this will hold it */

#define TIME_REQUERY_DFLT 2
#define SCHED_PRIO_UNSET -12345678	/* a value that indicates that the scheduling priority has not been set */
/* config vars for legacy config system */
static struct configSettings_s {
	uchar *pszBindAddr;		/* IP to bind socket to */
	uchar *pszSchedPolicy;		/* scheduling policy string */
	uchar *pszBindRuleset;		/* name of Ruleset to bind to */
	int iSchedPrio;			/* scheduling priority */
	int iTimeRequery;		/* how often is time to be queried inside tight recv loop? 0=always */
} cs;

struct instanceConf_s {
	uchar *pszBindAddr;		/* IP to bind socket to */
	uchar *pszBindPort;		/* Port to bind socket to */
	uchar *pszBindRuleset;		/* name of ruleset to bind to */
	ruleset_t *pBindRuleset;	/* ruleset to bind listener to (use system default if unspecified) */
	struct instanceConf_s *next;
};

struct modConfData_s {
	rsconf_t *pConf;		/* our overall config object */
	instanceConf_t *root, *tail;
	uchar *pszSchedPolicy;		/* scheduling policy string */
	int iSchedPolicy;		/* scheduling policy as SCHED_xxx */
	int iSchedPrio;			/* scheduling priority */
	int iTimeRequery;		/* how often is time to be queried inside tight recv loop? 0=always */
};
static modConfData_t *loadModConf = NULL;/* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL;/* modConf ptr to use for the current load process */

#include "im-helper.h" /* must be included AFTER the type definitions! */



/* This function is called when a new listener instace shall be added to 
 * the current config object via the legacy config system. It just shuffles
 * all parameters to the listener in-memory instance.
 * rgerhards, 2011-05-04
 */
static rsRetVal addInstance(void __attribute__((unused)) *pVal, uchar *pNewVal)
{
	instanceConf_t *inst;
	DEFiRet;

	CHKmalloc(inst = MALLOC(sizeof(instanceConf_t)));
	CHKmalloc(inst->pszBindPort = ustrdup((pNewVal == NULL || *pNewVal == '\0')
				 	       ? (uchar*) "514" : pNewVal));
	if((cs.pszBindAddr == NULL) || (cs.pszBindAddr[0] == '\0')) {
		inst->pszBindAddr = NULL;
	} else {
		CHKmalloc(inst->pszBindAddr = ustrdup(cs.pszBindAddr));
	}
	if((cs.pszBindRuleset == NULL) || (cs.pszBindRuleset[0] == '\0')) {
		inst->pszBindRuleset = NULL;
	} else {
		CHKmalloc(inst->pszBindRuleset = ustrdup(cs.pszBindRuleset));
	}
	inst->pBindRuleset = NULL;
	inst->next = NULL;

	/* node created, let's add to config */
	if(loadModConf->tail == NULL) {
		loadModConf->tail = loadModConf->root = inst;
	} else {
		loadModConf->tail->next = inst;
		loadModConf->tail = inst;
	}

finalize_it:
	free(pNewVal);
	RETiRet;
}


/* This function is called when a new listener shall be added. It takes
 * the instance config description, tries to bind the socket and, if that
 * succeeds, adds it to the list of existing listen sockets.
 */
static inline rsRetVal
addListner(instanceConf_t *inst)
{
	DEFiRet;
	uchar *bindAddr;
	int *newSocks;
	int iSrc;
	struct lstn_s *newlcnfinfo;
	uchar *bindName;
	uchar *port;
	uchar statname[64];

	/* check which address to bind to. We could do this more compact, but have not
	 * done so in order to make the code more readable. -- rgerhards, 2007-12-27
	 */
#if 0 //<<<<<<< HEAD

	DBGPRINTF("imudp: trying to open port at %s:%s.\n",
		  (inst->pszBindAddr == NULL) ? (uchar*)"*" : inst->pszBindAddr, inst->pszBindPort);

	newSocks = net.create_udp_socket(inst->pszBindAddr, inst->pszBindPort, 1);
	if(newSocks != NULL) {
		/* we now need to add the new sockets to the existing set */
		if(udpLstnSocks == NULL) {
			/* esay, we can just replace it */
			udpLstnSocks = newSocks;
			CHKmalloc(udpRulesets = (ruleset_t**) MALLOC(sizeof(ruleset_t*) * (newSocks[0] + 1)));
			for(iDst = 1 ; iDst <= newSocks[0] ; ++iDst)
				udpRulesets[iDst] = inst->pBindRuleset;
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
					tmpRulesets[iDst] = inst->pBindRuleset;
				}
				tmpSocks[0] = udpLstnSocks[0] + newSocks[0];
				free(newSocks);
				free(udpLstnSocks);
				udpLstnSocks = tmpSocks;
				free(udpRulesets);
				udpRulesets = tmpRulesets;
			}
#else //=======
	if(inst->pszBindAddr == NULL)
		bindAddr = NULL;
	else if(inst->pszBindAddr[0] == '*' && inst->pszBindAddr[1] == '\0')
		bindAddr = NULL;
	else
		bindAddr = inst->pszBindAddr;
	bindName = (bindAddr == NULL) ? (uchar*)"*" : bindAddr;
	port = (inst->pszBindPort == NULL || *inst->pszBindPort == '\0') ? (uchar*) "514" : inst->pszBindPort;

	DBGPRINTF("Trying to open syslog UDP ports at %s:%s.\n", bindName, inst->pszBindPort);

	newSocks = net.create_udp_socket(bindAddr, port, 1);
	if(newSocks != NULL) {
		/* we now need to add the new sockets to the existing set */
		/* ready to copy */
		for(iSrc = 1 ; iSrc <= newSocks[0] ; ++iSrc) {
			CHKmalloc(newlcnfinfo = (struct lstn_s*) MALLOC(sizeof(struct lstn_s)));
			newlcnfinfo->next = NULL;
			newlcnfinfo->sock = newSocks[iSrc];
			newlcnfinfo->pRuleset = inst->pBindRuleset;
			/* support statistics gathering */
			CHKiRet(statsobj.Construct(&(newlcnfinfo->stats)));
			snprintf((char*)statname, sizeof(statname), "imudp(%s:%s)", bindName, port);
			statname[sizeof(statname)-1] = '\0'; /* just to be on the save side... */
			CHKiRet(statsobj.SetName(newlcnfinfo->stats, statname));
			STATSCOUNTER_INIT(newlcnfinfo->ctrSubmit, newlcnfinfo->mutCtrSubmit);
			CHKiRet(statsobj.AddCounter(newlcnfinfo->stats, UCHAR_CONSTANT("submitted"),
				ctrType_IntCtr, &(newlcnfinfo->ctrSubmit)));
			CHKiRet(statsobj.ConstructFinalize(newlcnfinfo->stats));
			/* link to list. Order must be preserved to take care for 
			 * conflicting matches.
			 */
			if(lcnfRoot == NULL)
				lcnfRoot = newlcnfinfo;
			if(lcnfLast == NULL)
				lcnfLast = newlcnfinfo;
			else {
				lcnfLast->next = newlcnfinfo;
				lcnfLast = newlcnfinfo;
#endif //>>>>>>> ef34821a2737799f48c3032b9616418e4f7fa34f
			}
		}
	}

finalize_it:
	free(newSocks);
	RETiRet;
}


static inline void
std_checkRuleset_genErrMsg(__attribute__((unused)) modConfData_t *modConf, instanceConf_t *inst)
{
	errmsg.LogError(0, NO_ERRCODE, "imudp: ruleset '%s' for %s:%s not found - "
			"using default ruleset instead", inst->pszBindRuleset,
			inst->pszBindAddr == NULL ? "*" : (char*) inst->pszBindAddr,
			inst->pszBindPort);
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
processSocket(thrdInfo_t *pThrd, struct lstn_s *lstn, struct sockaddr_storage *frominetPrev, int *pbIsPermitted)
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
		lenRcvBuf = recvfrom(lstn->sock, (char*) pRcvBuf, iMaxLine, 0, (struct sockaddr *)&frominet, &socklen);
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

		DBGPRINTF("recv(%d,%d),acl:%d,msg:%s\n", lstn->sock, (int) lenRcvBuf, *pbIsPermitted, pRcvBuf);

		if(*pbIsPermitted != 0)  {
			if((runModConf->iTimeRequery == 0) || (iNbrTimeUsed++ % runModConf->iTimeRequery) == 0) {
				datetime.getCurrTime(&stTime, &ttGenTime);
			}
			/* we now create our own message object and submit it to the queue */
			CHKiRet(msgConstructWithTime(&pMsg, &stTime, ttGenTime));
			MsgSetRawMsg(pMsg, (char*)pRcvBuf, lenRcvBuf);
			MsgSetInputName(pMsg, pInputName);
			MsgSetRuleset(pMsg, lstn->pRuleset);
			MsgSetFlowControlType(pMsg, eFLOWCTL_NO_DELAY);
			pMsg->msgFlags  = NEEDS_PARSING | PARSE_HOSTNAME | NEEDS_DNSRESOL;
			if(*pbIsPermitted == 2)
				pMsg->msgFlags  |= NEEDS_ACLCHK_U; /* request ACL check after resolution */
			CHKiRet(msgSetFromSockinfo(pMsg, &frominet));
			CHKiRet(submitMsg(pMsg));
			STATSCOUNTER_INC(lstn->ctrSubmit, lstn->mutCtrSubmit);
		}
	}

finalize_it:
	if(propFromHost != NULL)
		prop.Destruct(&propFromHost);
	if(propFromHostIP != NULL)
		prop.Destruct(&propFromHostIP);

	RETiRet;
}


/* check configured scheduling priority.
 * Precondition: iSchedPolicy must have been set
 */
static inline rsRetVal
checkSchedulingPriority(modConfData_t *modConf)
{
    	DEFiRet;

#ifdef HAVE_SCHED_GET_PRIORITY_MAX
	if(   modConf->iSchedPrio < sched_get_priority_min(modConf->iSchedPolicy)
	   || modConf->iSchedPrio > sched_get_priority_max(modConf->iSchedPolicy)) {
		errmsg.LogError(0, NO_ERRCODE,
			"imudp: scheduling priority %d out of range (%d - %d)"
			" for scheduling policy '%s' - ignoring settings",
			modConf->iSchedPrio,
			sched_get_priority_min(modConf->iSchedPolicy),
			sched_get_priority_max(modConf->iSchedPolicy),
			modConf->pszSchedPolicy);
		ABORT_FINALIZE(RS_RET_VALIDATION_RUN);
	}
#endif

finalize_it:
	RETiRet;
}


/* check scheduling policy string and, if valid, set its 
 * numeric equivalent in current load config
 */
static rsRetVal
checkSchedulingPolicy(modConfData_t *modConf)
{
	DEFiRet;

	if (0) { /* trick to use conditional compilation */
#ifdef SCHED_FIFO
	} else if (!strcasecmp((char*)modConf->pszSchedPolicy, "fifo")) {
		modConf->iSchedPolicy = SCHED_FIFO;
#endif
#ifdef SCHED_RR
	} else if (!strcasecmp((char*)modConf->pszSchedPolicy, "rr")) {
		modConf->iSchedPolicy = SCHED_RR;
#endif
#ifdef SCHED_OTHER
	} else if (!strcasecmp((char*)modConf->pszSchedPolicy, "other")) {
		modConf->iSchedPolicy = SCHED_OTHER;
#endif
	} else {
		errmsg.LogError(errno, NO_ERRCODE,
			    "imudp: invalid scheduling policy '%s' "
			    "- ignoring setting", modConf->pszSchedPolicy);
		ABORT_FINALIZE(RS_RET_ERR_SCHED_PARAMS);
	}
finalize_it:
	RETiRet;
}

/* checks scheduling parameters during config check phase */
static rsRetVal
checkSchedParam(modConfData_t *modConf)
{
	DEFiRet;

	if(modConf->pszSchedPolicy != NULL && modConf->iSchedPrio == SCHED_PRIO_UNSET) {
		errmsg.LogError(0, RS_RET_ERR_SCHED_PARAMS,
			"imudp: scheduling policy set, but without priority - ignoring settings");
		ABORT_FINALIZE(RS_RET_ERR_SCHED_PARAMS);
	} else if(modConf->pszSchedPolicy == NULL && modConf->iSchedPrio != SCHED_PRIO_UNSET) {
		errmsg.LogError(0, RS_RET_ERR_SCHED_PARAMS,
			"imudp: scheduling priority set, but without policy - ignoring settings");
		ABORT_FINALIZE(RS_RET_ERR_SCHED_PARAMS);
	} else if(modConf->pszSchedPolicy != NULL && modConf->iSchedPrio != SCHED_PRIO_UNSET) {
		/* we have parameters set, so check them */
		CHKiRet(checkSchedulingPolicy(modConf));
		CHKiRet(checkSchedulingPriority(modConf));
	} else { /* nothing set */
		modConf->iSchedPrio = SCHED_PRIO_UNSET; /* prevents doing the activation call */
	}
#ifndef HAVE_PTHREAD_SETSCHEDPARAM
	errmsg.LogError(0, NO_ERRCODE,
		"imudp: cannot set thread scheduling policy, "
		"pthread_setschedparam() not available");
	ABORT_FINALIZE(RS_RET_ERR_SCHED_PARAMS);
#endif

finalize_it:
	if(iRet != RS_RET_OK)
		modConf->iSchedPrio = SCHED_PRIO_UNSET; /* prevents doing the activation call */

	RETiRet;
}

/* set the configured scheduling policy (if possible) */
static rsRetVal
setSchedParams(modConfData_t *modConf)
{
	DEFiRet;

#	ifdef HAVE_PTHREAD_SETSCHEDPARAM
	int err;
	struct sched_param sparam;

	if(modConf->iSchedPrio == SCHED_PRIO_UNSET)
		FINALIZE;

	memset(&sparam, 0, sizeof sparam);
	sparam.sched_priority = modConf->iSchedPrio;
	dbgprintf("imudp trying to set sched policy to '%s', prio %d\n",
		  modConf->pszSchedPolicy, modConf->iSchedPrio);
	err = pthread_setschedparam(pthread_self(), modConf->iSchedPolicy, &sparam);
	if(err != 0) {
		errmsg.LogError(err, NO_ERRCODE, "imudp: pthread_setschedparam() failed - ignoring");
	}
#	endif

finalize_it:
	RETiRet;
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
	struct lstn_s *lstn;
	int nLstn;

	/* start "name caching" algo by making sure the previous system indicator
	 * is invalidated.
	 */
	bIsPermitted = 0;
	memset(&frominetPrev, 0, sizeof(frominetPrev));

	/* count num listeners -- do it here in order to avoid inconsistency */
	nLstn = 0;
	for(lstn = lcnfRoot ; lstn != NULL ; lstn = lstn->next)
		++nLstn;
	CHKmalloc(udpEPollEvt = calloc(nLstn, sizeof(struct epoll_event)));

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
	i = 0;
	for(lstn = lcnfRoot ; lstn != NULL ; lstn = lstn->next) {
		if(lstn->sock != -1) {
			udpEPollEvt[i].events = EPOLLIN | EPOLLET;
			udpEPollEvt[i].data.u64 = (long long unsigned) lstn;
			if(epoll_ctl(efd, EPOLL_CTL_ADD,  lstn->sock, &(udpEPollEvt[i])) < 0) {
				rs_strerror_r(errno, errStr, sizeof(errStr));
				errmsg.LogError(errno, NO_ERRCODE, "epoll_ctrl failed on fd %d with %s\n",
					lstn->sock, errStr);
			}
		}
		i++;
	}

	while(1) {
		/* wait for io to become ready */
		nfds = epoll_wait(efd, currEvt, NUM_EPOLL_EVENTS, -1);
		DBGPRINTF("imudp: epoll_wait() returned with %d fds\n", nfds);

		if(pThrd->bShallStop == TRUE)
			break; /* terminate input! */

		for(i = 0 ; i < nfds ; ++i) {
			processSocket(pThrd, (struct lstn_s*)currEvt[i].data.u64, &frominetPrev, &bIsPermitted);
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
	fd_set readfds;
	struct sockaddr_storage frominetPrev;
	int bIsPermitted;
	struct lstn_s *lstn;

	/* start "name caching" algo by making sure the previous system indicator
	 * is invalidated.
	 */
	bIsPermitted = 0;
	memset(&frominetPrev, 0, sizeof(frominetPrev));
	DBGPRINTF("imudp uses select()\n");

	while(1) {
		/* Add the Unix Domain Sockets to the list of read descriptors.
		 */
	        maxfds = 0;
	        FD_ZERO(&readfds);

		/* Add the UDP listen sockets to the list of read descriptors. */
		for(lstn = lcnfRoot ; lstn != NULL ; lstn = lstn->next) {
			if (lstn->sock != -1) {
				if(Debug)
					net.debugListenInfo(lstn->sock, "UDP");
				FD_SET(lstn->sock, &readfds);
				if(lstn->sock>maxfds) maxfds=lstn->sock;
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

		for(lstn = lcnfRoot ; nfds && lstn != NULL ; lstn = lstn->next) {
			if(FD_ISSET(lstn->sock, &readfds)) {
		       		processSocket(pThrd, lstn, &frominetPrev, &bIsPermitted);
			--nfds; /* indicate we have processed one descriptor */
			}
	       }
	       /* end of a run, back to loop for next recv() */
	}

	RETiRet;
}
#endif /* #if HAVE_EPOLL_CREATE1 */


BEGINbeginCnfLoad
CODESTARTbeginCnfLoad
	loadModConf = pModConf;
	pModConf->pConf = pConf;
	/* init legacy config vars */
	cs.pszBindRuleset = NULL;
	cs.pszSchedPolicy = NULL;
	cs.pszBindAddr = NULL;
	cs.iSchedPrio = SCHED_PRIO_UNSET;
	cs.iTimeRequery = TIME_REQUERY_DFLT;
ENDbeginCnfLoad


BEGINendCnfLoad
CODESTARTendCnfLoad
	/* persist module-specific settings from legacy config system
	 * TODO: when we add the new config system, we must decide on priority
	 * already-set module options should not be overwritable by the legacy
	 * system (though this is debatable and should at least trigger an error
	 * message if the equivalent legacy option is selected as well)
	 * rgerhards, 2011-05-04
	 */
	loadModConf->iSchedPrio = cs.iSchedPrio;
	loadModConf->iTimeRequery = cs.iTimeRequery;
	if((cs.pszSchedPolicy == NULL) || (cs.pszSchedPolicy[0] == '\0')) {
		loadModConf->pszSchedPolicy = NULL;
	} else {
		CHKmalloc(loadModConf->pszSchedPolicy = ustrdup(cs.pszSchedPolicy));
	}

finalize_it:
	loadModConf = NULL; /* done loading */
	/* free legacy config vars */
	free(cs.pszBindRuleset);
	free(cs.pszSchedPolicy);
	free(cs.pszBindAddr);
ENDendCnfLoad


BEGINcheckCnf
	instanceConf_t *inst;
CODESTARTcheckCnf
	checkSchedParam(pModConf); /* this can not cause fatal errors */
	for(inst = pModConf->root ; inst != NULL ; inst = inst->next) {
		std_checkRuleset(pModConf, inst);
	}
	if(pModConf->root == NULL) {
		errmsg.LogError(0, RS_RET_NO_LISTNERS , "imudp: module loaded, but "
				"no listeners defined - no input will be gathered");
		iRet = RS_RET_NO_LISTNERS;
	}
ENDcheckCnf


BEGINactivateCnfPrePrivDrop
	instanceConf_t *inst;
CODESTARTactivateCnfPrePrivDrop
	runModConf = pModConf;
	for(inst = runModConf->root ; inst != NULL ; inst = inst->next) {
		addListner(inst);
	}
	/* if we could not set up any listners, there is no point in running... */
	if(lcnfRoot == NULL) {
		errmsg.LogError(0, NO_ERRCODE, "imudp: no listeners could be started, "
				"input not activated.\n");
		ABORT_FINALIZE(RS_RET_NO_RUN);
	}

	setSchedParams(pModConf);
finalize_it:
ENDactivateCnfPrePrivDrop


BEGINactivateCnf
CODESTARTactivateCnf
	/* caching various settings */
	iMaxLine = glbl.GetMaxLine();
	CHKmalloc(pRcvBuf = MALLOC((iMaxLine + 1) * sizeof(char)));
finalize_it:
ENDactivateCnf


BEGINfreeCnf
	instanceConf_t *inst, *del;
CODESTARTfreeCnf
	for(inst = pModConf->root ; inst != NULL ; ) {
		free(inst->pszBindPort);
		free(inst->pszBindAddr);
		free(inst->pBindRuleset);
		del = inst;
		inst = inst->next;
		free(del);
	}
ENDfreeCnf

/* This function is called to gather input.
 * Note that sock must be non-NULL because otherwise we would not have
 * indicated that we want to run (or we have a programming error ;)). -- rgerhards, 2008-10-02
 */
BEGINrunInput
CODESTARTrunInput
	iRet = rcvMainLoop(pThrd);
ENDrunInput


/* initialize and return if will run or not */
BEGINwillRun
CODESTARTwillRun
	net.PrintAllowedSenders(1); /* UDP */
	net.HasRestrictions(UCHAR_CONSTANT("UDP"), &bDoACLCheck); /* UDP */
ENDwillRun


BEGINafterRun
	struct lstn_s *lstn, *lstnDel;
CODESTARTafterRun
	/* do cleanup here */
	net.clearAllowedSenders((uchar*)"UDP");
	for(lstn = lcnfRoot ; lstn != NULL ; ) {
		statsobj.Destruct(&(lstn->stats));
		close(lstn->sock);
		lstnDel = lstn;
		lstn = lstn->next;
		free(lstnDel);
	}
	lcnfRoot = lcnfLast = NULL;
	if(pRcvBuf != NULL) {
		free(pRcvBuf);
		pRcvBuf = NULL;
	}
ENDafterRun


BEGINmodExit
CODESTARTmodExit
	if(pInputName != NULL)
		prop.Destruct(&pInputName);

	/* release what we no longer need */
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(glbl, CORE_COMPONENT);
	objRelease(statsobj, CORE_COMPONENT);
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
CODEqueryEtryPt_STD_CONF2_QUERIES
CODEqueryEtryPt_STD_CONF2_PREPRIVDROP_QUERIES
CODEqueryEtryPt_IsCompatibleWithFeature_IF_OMOD_QUERIES
ENDqueryEtryPt

static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	if(cs.pszBindAddr != NULL) {
		free(cs.pszBindAddr);
		cs.pszBindAddr = NULL;
	}
	if(cs.pszSchedPolicy != NULL) {
		free(cs.pszSchedPolicy);
		cs.pszSchedPolicy = NULL;
	}
	if(cs.pszBindRuleset != NULL) {
		free(cs.pszBindRuleset);
		cs.pszBindRuleset = NULL;
	}
	cs.iSchedPrio = SCHED_PRIO_UNSET;
	cs.iTimeRequery = TIME_REQUERY_DFLT;/* the default is to query only every second time */
	return RS_RET_OK;
}


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(statsobj, CORE_COMPONENT));
	CHKiRet(objUse(datetime, CORE_COMPONENT));
	CHKiRet(objUse(prop, CORE_COMPONENT));
	CHKiRet(objUse(ruleset, CORE_COMPONENT));
	CHKiRet(objUse(net, LM_NET_FILENAME));

	/* we need to create the inputName property (only once during our lifetime) */
	CHKiRet(prop.Construct(&pInputName));
	CHKiRet(prop.SetString(pInputName, UCHAR_CONSTANT("imudp"), sizeof("imudp") - 1));
	CHKiRet(prop.ConstructFinalize(pInputName));

	/* register config file handlers */
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputudpserverbindruleset", 0, eCmdHdlrGetWord,
		NULL, &cs.pszBindRuleset, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"udpserverrun", 0, eCmdHdlrGetWord,
		addInstance, NULL, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"udpserveraddress", 0, eCmdHdlrGetWord,
		NULL, &cs.pszBindAddr, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"imudpschedulingpolicy", 0, eCmdHdlrGetWord,
		NULL, &cs.pszSchedPolicy, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"imudpschedulingpriority", 0, eCmdHdlrInt,
		NULL, &cs.iSchedPrio, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"udpservertimerequery", 0, eCmdHdlrInt,
		NULL, &cs.iTimeRequery, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler,
		resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit
/* vim:set ai:
 */
