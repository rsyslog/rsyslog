/* imudp.c
 * This is the implementation of the UDP input module.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * File begun on 2007-12-21 by RGerhards (extracted from syslogd.c)
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
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>
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

MODULE_TYPE_INPUT

/* defines */

/* Module static data */
DEF_IMOD_STATIC_DATA
DEFobjCurrIf(errmsg)
DEFobjCurrIf(glbl)
DEFobjCurrIf(net)
DEFobjCurrIf(datetime)

static int iMaxLine;			/* maximum UDP message size supported */
static time_t ttLastDiscard = 0;	/* timestamp when a message from a non-permitted sender was last discarded
					 * This shall prevent remote DoS when the "discard on disallowed sender"
					 * message is configured to be logged on occurance of such a case.
					 */
static int *udpLstnSocks = NULL;	/* Internet datagram sockets, first element is nbr of elements
					 * read-only after init(), but beware of restart! */
static uchar *pszBindAddr = NULL;	/* IP to bind socket to */
static uchar *pRcvBuf = NULL;		/* receive buffer (for a single packet). We use a global and alloc
					 * it so that we can check available memory in willRun() and request
					 * termination if we can not get it. -- rgerhards, 2007-12-27
					 */
#define TIME_REQUERY_DFLT 2
static int iTimeRequery = TIME_REQUERY_DFLT;/* how often is time to be queried inside tight recv loop? 0=always */

/* config settings */


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

	/* check which address to bind to. We could do this more compact, but have not
	 * done so in order to make the code more readable. -- rgerhards, 2007-12-27
	 */
	if(pszBindAddr == NULL)
		bindAddr = NULL;
	else if(pszBindAddr[0] == '*' && pszBindAddr[1] == '\0')
		bindAddr = NULL;
	else
		bindAddr = pszBindAddr;

	dbgprintf("Trying to open syslog UDP ports at %s:%s.\n",
		  (bindAddr == NULL) ? (uchar*)"*" : bindAddr, pNewVal);

	newSocks = net.create_udp_socket(bindAddr, (pNewVal == NULL || *pNewVal == '\0') ? (uchar*) "514" : pNewVal, 1);
	if(newSocks != NULL) {
		/* we now need to add the new sockets to the existing set */
		if(udpLstnSocks == NULL) {
			/* esay, we can just replace it */
			udpLstnSocks = newSocks;
		} else {
			/* we need to add them */
			if((tmpSocks = malloc(sizeof(int) * 1 + newSocks[0] + udpLstnSocks[0])) == NULL) {
				dbgprintf("out of memory trying to allocate udp listen socket array\n");
				/* in this case, we discard the new sockets but continue with what we
				 * already have
				 */
				free(newSocks);
				ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
			} else {
				/* ready to copy */
				iDst = 1;
				for(iSrc = 1 ; iSrc <= udpLstnSocks[0] ; ++iSrc)
					tmpSocks[iDst++] = udpLstnSocks[iSrc];
				for(iSrc = 1 ; iSrc <= newSocks[0] ; ++iSrc)
					tmpSocks[iDst++] = newSocks[iSrc];
				tmpSocks[0] = udpLstnSocks[0] + newSocks[0];
				free(newSocks);
				free(udpLstnSocks);
				udpLstnSocks = tmpSocks;
			}
		}
	}

finalize_it:
	free(pNewVal); /* in any case, this is no longer needed */

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
processSocket(int fd, struct sockaddr_storage *frominetPrev, int *pbIsPermitted,
	      uchar *fromHost, uchar *fromHostFQDN, uchar *fromHostIP)
{
	DEFiRet;
	int iNbrTimeUsed;
	time_t ttGenTime;
	struct syslogTime stTime;
	socklen_t socklen;
	ssize_t lenRcvBuf;
	struct sockaddr_storage frominet;
	msg_t *pMsg;
	char errStr[1024];

	iNbrTimeUsed = 0;
	while(1) { /* loop is terminated if we have a bad receive, done below in the body */
		socklen = sizeof(struct sockaddr_storage);
		lenRcvBuf = recvfrom(fd, (char*) pRcvBuf, iMaxLine, 0, (struct sockaddr *)&frominet, &socklen);
		if(lenRcvBuf < 0) {
			if(errno != EINTR && errno != EAGAIN) {
				rs_strerror_r(errno, errStr, sizeof(errStr));
				DBGPRINTF("INET socket error: %d = %s.\n", errno, errStr);
				errmsg.LogError(errno, NO_ERRCODE, "recvfrom inet");
			}
			ABORT_FINALIZE(RS_RET_ERR);
		}

		/* if we reach this point, we had a good receive and can process the packet received */
		/* check if we have a different sender than before, if so, we need to query some new values */
		if(memcmp(&frominet, frominetPrev, socklen) != 0) {
			CHKiRet(net.cvthname(&frominet, fromHost, fromHostFQDN, fromHostIP));
DBGPRINTF("returned: fromHost '%s', FQDN: '%s'\n", fromHost, fromHostFQDN);
			memcpy(frominetPrev, &frominet, socklen); /* update cache indicator */
			/* Here we check if a host is permitted to send us
			* syslog messages. If it isn't, we do not further
			* process the message but log a warning (if we are
			* configured to do this).
			* rgerhards, 2005-09-26
			*/
			*pbIsPermitted = net.isAllowedSender((uchar*)"UDP",
			  			            (struct sockaddr *)&frominet, (char*)fromHostFQDN);
	
			if(!*pbIsPermitted) {
				DBGPRINTF("%s is not an allowed sender\n", (char*)fromHostFQDN);
				if(glbl.GetOption_DisallowWarning) {
					time_t tt;

					time(&tt);
					if(tt > ttLastDiscard + 60) {
						ttLastDiscard = tt;
						errmsg.LogError(0, NO_ERRCODE,
						"UDP message from disallowed sender %s discarded",
						(char*)fromHost);
					}
				}
			}
		}

		DBGPRINTF("recv(%d,%d)/%s,acl:%d,msg:%.80s\n", fd, (int) lenRcvBuf, fromHost, *pbIsPermitted, pRcvBuf);

		if(*pbIsPermitted)  {
			if((iTimeRequery == 0) || (iNbrTimeUsed++ % iTimeRequery) == 0) {
				datetime.getCurrTime(&stTime, &ttGenTime);
			}
			/* we now create our own message object and submit it to the queue */
			CHKiRet(msgConstructWithTime(&pMsg, &stTime, ttGenTime));
			/* first trim the buffer to what we have actually received */
			CHKmalloc(pMsg->pszRawMsg = malloc(sizeof(uchar)* lenRcvBuf));
			memcpy(pMsg->pszRawMsg, pRcvBuf, lenRcvBuf);
			pMsg->iLenRawMsg = lenRcvBuf;
			MsgSetInputName(pMsg, "imudp");
			MsgSetFlowControlType(pMsg, eFLOWCTL_NO_DELAY);
			pMsg->msgFlags  = NEEDS_PARSING | PARSE_HOSTNAME;
			MsgSetRcvFrom(pMsg, (char*)fromHost);
			CHKiRet(MsgSetRcvFromIP(pMsg, fromHostIP));
			CHKiRet(submitMsg(pMsg));
		}
	}


finalize_it:
	RETiRet;
}


/* This function is called to gather input.
 * Note that udpLstnSocks must be non-NULL because otherwise we would not have
 * indicated that we want to run (or we have a programming error ;)). -- rgerhards, 2008-10-02
 * rgerhards, 2008-10-07: I have implemented a very simple, yet in most cases probably
 * highly efficient "name caching". Before querying a name, I now check if the name to be
 * queried is the same as the one queried in the last message processed. If that is the
 * case, we can simple re-use the previous value. This algorithm works quite well with
 * few sender, especially if they emit messages in bursts. The more sender and the
 * more intermixed messages arrive, the less this algorithm works, but the overhead
 * is so minimal (a simple memory compare and move) that this does not hurt. Even
 * with a real name lookup cache, this optimization here is useful as it is quicker
 * than even a cache lookup).
 */
BEGINrunInput
	int maxfds;
	int nfds;
	int i;
	fd_set readfds;
	struct sockaddr_storage frominetPrev;
	int bIsPermitted;
	uchar fromHost[NI_MAXHOST];
	uchar fromHostIP[NI_MAXHOST];
	uchar fromHostFQDN[NI_MAXHOST];
CODESTARTrunInput
	/* start "name caching" algo by making sure the previous system indicator
	 * is invalidated.
	 */
	bIsPermitted = 0;
	memset(&frominetPrev, 0, sizeof(frominetPrev));
	/* this is an endless loop - it is terminated when the thread is
	 * signalled to do so. This, however, is handled by the framework,
	 * right into the sleep below.
	 */
	while(1) {
		/* Add the Unix Domain Sockets to the list of read
		 * descriptors.
		 * rgerhards 2005-08-01: we must now check if there are
		 * any local sockets to listen to at all. If the -o option
		 * is given without -a, we do not need to listen at all..
		 */
	        maxfds = 0;
	        FD_ZERO (&readfds);

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
				if ( FD_ISSET(nfds, &readfds) )
					dbgprintf("%d ", nfds);
			dbgprintf("\n");
		}

		/* wait for io to become ready */
		nfds = select(maxfds+1, (fd_set *) &readfds, NULL, NULL, NULL);

	       for(i = 0; nfds && i < *udpLstnSocks; i++) {
			if(FD_ISSET(udpLstnSocks[i+1], &readfds)) {
		       		processSocket(udpLstnSocks[i+1], &frominetPrev, &bIsPermitted,
					      fromHost, fromHostFQDN, fromHostIP);
			--nfds; /* indicate we have processed one descriptor */
			}
	       }
	       /* end of a run, back to loop for next recv() */
	}

	return iRet;
ENDrunInput


/* initialize and return if will run or not */
BEGINwillRun
CODESTARTwillRun
	net.PrintAllowedSenders(1); /* UDP */

	/* if we could not set up any listners, there is no point in running... */
	if(udpLstnSocks == NULL)
		ABORT_FINALIZE(RS_RET_NO_RUN);

	iMaxLine = glbl.GetMaxLine();

	if((pRcvBuf = malloc((iMaxLine + 1) * sizeof(char))) == NULL) {
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}
finalize_it:
ENDwillRun


BEGINafterRun
CODESTARTafterRun
	/* do cleanup here */
	net.clearAllowedSenders((uchar*)"UDP");
	if(udpLstnSocks != NULL) {
		net.closeUDPListenSockets(udpLstnSocks);
		udpLstnSocks = NULL;
	}
	if(pRcvBuf != NULL) {
		free(pRcvBuf);
		pRcvBuf = NULL;
	}
ENDafterRun


BEGINmodExit
CODESTARTmodExit
	/* release what we no longer need */
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(glbl, CORE_COMPONENT);
	objRelease(datetime, CORE_COMPONENT);
	objRelease(net, LM_NET_FILENAME);
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_IMOD_QUERIES
ENDqueryEtryPt

static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	if(pszBindAddr != NULL) {
		free(pszBindAddr);
		pszBindAddr = NULL;
	}
	if(udpLstnSocks != NULL) {
		net.closeUDPListenSockets(udpLstnSocks);
		udpLstnSocks = NULL;
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
	CHKiRet(objUse(net, LM_NET_FILENAME));

	/* register config file handlers */
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"udpserverrun", 0, eCmdHdlrGetWord,
		addListner, NULL, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"udpserveraddress", 0, eCmdHdlrGetWord,
		NULL, &pszBindAddr, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"udpservertimerequery", 0, eCmdHdlrInt,
		NULL, &iTimeRequery, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler,
		resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit
/* vim:set ai:
 */
