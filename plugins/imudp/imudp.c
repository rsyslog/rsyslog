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
#include "syslogd.h"
#include "net.h"
#include "cfsysline.h"
#include "module-template.h"
#include "srUtils.h"
#include "errmsg.h"

MODULE_TYPE_INPUT

/* defines */

/* Module static data */
DEF_IMOD_STATIC_DATA
DEFobjCurrIf(errmsg)
DEFobjCurrIf(net)

static int *udpLstnSocks = NULL;	/* Internet datagram sockets, first element is nbr of elements
					 * read-only after init(), but beware of restart! */
static uchar *pszBindAddr = NULL;	/* IP to bind socket to */
static uchar *pRcvBuf = NULL;		/* receive buffer (for a single packet). We use a global and alloc
					 * it so that we can check available memory in willRun() and request
					 * termination if we can not get it. -- rgerhards, 2007-12-27
					 */

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


/* This function is called to gather input.
 */
BEGINrunInput
	int maxfds;
	int nfds;
	int i;
	fd_set readfds;
	struct sockaddr_storage frominet;
	socklen_t socklen;
	uchar fromHost[NI_MAXHOST];
	uchar fromHostFQDN[NI_MAXHOST];
	ssize_t l;
CODESTARTrunInput
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

		/* Add the UDP listen sockets to the list of read descriptors.
		 */
		if(udpLstnSocks != NULL) {
                        for (i = 0; i < *udpLstnSocks; i++) {
                                if (udpLstnSocks[i+1] != -1) {
					if(Debug)
						net.debugListenInfo(udpLstnSocks[i+1], "UDP");
                                        FD_SET(udpLstnSocks[i+1], &readfds);
					if(udpLstnSocks[i+1]>maxfds) maxfds=udpLstnSocks[i+1];
				}
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

		if(udpLstnSocks != NULL) {
		       for (i = 0; nfds && i < *udpLstnSocks; i++) {
			       if (FD_ISSET(udpLstnSocks[i+1], &readfds)) {
				       socklen = sizeof(frominet);
				       l = recvfrom(udpLstnSocks[i+1], (char*) pRcvBuf, MAXLINE - 1, 0,
						    (struct sockaddr *)&frominet, &socklen);
				       if (l > 0) {
					       if(net.cvthname(&frominet, fromHost, fromHostFQDN) == RS_RET_OK) {
						       dbgprintf("Message from inetd socket: #%d, host: %s\n",
							       udpLstnSocks[i+1], fromHost);
						       /* Here we check if a host is permitted to send us
							* syslog messages. If it isn't, we do not further
							* process the message but log a warning (if we are
							* configured to do this).
							* rgerhards, 2005-09-26
							*/
						       if(net.isAllowedSender(net.pAllowedSenders_UDP,
							  (struct sockaddr *)&frominet, (char*)fromHostFQDN)) {
							       parseAndSubmitMessage((char*)fromHost, (char*) pRcvBuf, l,
							       MSG_PARSE_HOSTNAME, NOFLAG, eFLOWCTL_NO_DELAY);
						       } else {
							       dbgprintf("%s is not an allowed sender\n", (char*)fromHostFQDN);
							       if(option_DisallowWarning) {
								       errmsg.LogError(NO_ERRCODE, "UDP message from disallowed sender %s discarded",
										  (char*)fromHost);
							       }	
						       }
					       }
				       } else if (l < 0 && errno != EINTR && errno != EAGAIN) {
						char errStr[1024];
						rs_strerror_r(errno, errStr, sizeof(errStr));
						dbgprintf("INET socket error: %d = %s.\n", errno, errStr);
						       errmsg.LogError(NO_ERRCODE, "recvfrom inet");
						       /* should be harmless */
						       sleep(1);
					       }
					--nfds; /* indicate we have processed one */
				}
		       }
		}
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

	if((pRcvBuf = malloc(MAXLINE * sizeof(char))) == NULL) {
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}
finalize_it:
ENDwillRun


BEGINafterRun
CODESTARTafterRun
	/* do cleanup here */
	if (net.pAllowedSenders_UDP != NULL) {
		net.clearAllowedSenders (net.pAllowedSenders_UDP);
		net.pAllowedSenders_UDP = NULL;
	}
	if(udpLstnSocks != NULL)
		net.closeUDPListenSockets(udpLstnSocks);
	if(pRcvBuf != NULL)
		free(pRcvBuf);
ENDafterRun


BEGINmodExit
CODESTARTmodExit
	/* release what we no longer need */
	objRelease(errmsg, CORE_COMPONENT);
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
	return RS_RET_OK;
}


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(net, LM_NET_FILENAME));

	/* register config file handlers */
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"udpserverrun", 0, eCmdHdlrGetWord,
		addListner, NULL, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"udpserveraddress", 0, eCmdHdlrGetWord,
		NULL, &pszBindAddr, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler,
		resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit
/*
 * vi:set ai:
 */
