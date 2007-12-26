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
#include "cfsysline.h"
#include "module-template.h"

MODULE_TYPE_INPUT
TERM_SYNC_TYPE(eTermSync_NONE)

/* defines */

/* Module static data */
DEF_IMOD_STATIC_DATA
static int *udpLstnSocks = NULL;	/* Internet datagram sockets, first element is nbr of elements
					 * read-only after init(), but beware of restart! */

typedef struct _instanceData {
} instanceData;

/* config settings */


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
	char line[MAXLINE +1];
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
		if(udpLstnSocks != NULL && AcceptRemote) {
                        for (i = 0; i < *udpLstnSocks; i++) {
                                if (udpLstnSocks[i+1] != -1) {
					if(Debug)
						debugListenInfo(udpLstnSocks[i+1], "UDP");
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

		if (udpLstnSocks != NULL && AcceptRemote) {
		       for (i = 0; nfds && i < *udpLstnSocks; i++) {
			       if (FD_ISSET(udpLstnSocks[i+1], &readfds)) {
				       socklen = sizeof(frominet);
				       l = recvfrom(udpLstnSocks[i+1], line, MAXLINE - 1, 0,
						    (struct sockaddr *)&frominet, &socklen);
				       if (l > 0) {
					       if(cvthname(&frominet, fromHost, fromHostFQDN) == RS_RET_OK) {
						       dbgprintf("Message from inetd socket: #%d, host: %s\n",
							       udpLstnSocks[i+1], fromHost);
						       /* Here we check if a host is permitted to send us
							* syslog messages. If it isn't, we do not further
							* process the message but log a warning (if we are
							* configured to do this).
							* rgerhards, 2005-09-26
							*/
						       if(isAllowedSender(pAllowedSenders_UDP,
							  (struct sockaddr *)&frominet, (char*)fromHostFQDN)) {
							       printchopped((char*)fromHost, line, l,  udpLstnSocks[i+1], 1);
						       } else {
							       dbgprintf("%s is not an allowed sender\n", (char*)fromHostFQDN);
							       if(option_DisallowWarning) {
								       logerrorSz("UDP message from disallowed sender %s discarded",
										  (char*)fromHost);
							       }	
						       }
					       }
				       } else if (l < 0 && errno != EINTR && errno != EAGAIN) {
						char errStr[1024];
						strerror_r(errno, errStr, sizeof(errStr));
						dbgprintf("INET socket error: %d = %s.\n", errno, errStr);
						       logerror("recvfrom inet");
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
	PrintAllowedSenders(1); /* UDP */
	if((udpLstnSocks = create_udp_socket(NULL, (uchar*)LogPort, 1)) != NULL)
		dbgprintf("Opened %d syslog UDP port(s).\n", *udpLstnSocks);

ENDwillRun


BEGINafterRun
CODESTARTafterRun
	/* do cleanup here */
dbgprintf("call clearAllowedSenders(0x%lx)\n", (unsigned long) pAllowedSenders_UDP);
	if (pAllowedSenders_UDP != NULL) {
		clearAllowedSenders (pAllowedSenders_UDP);
		pAllowedSenders_UDP = NULL;
	}
	closeUDPListenSockets(udpLstnSocks);
ENDafterRun


BEGINfreeInstance
CODESTARTfreeInstance
ENDfreeInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
ENDdbgPrintInstInfo


BEGINmodExit
CODESTARTmodExit
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_IMOD_QUERIES
ENDqueryEtryPt

static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	return RS_RET_OK;
}


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = 1; /* so far, we only support the initial definition */
CODEmodInit_QueryRegCFSLineHdlr
	/* register config file handlers */
	//CHKiRet(omsdRegCFSLineHdlr((uchar *)"omitlocallogging", 0, eCmdHdlrBinary,
	//	NULL, &bOmitLocalLogging, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler,
		resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit
/*
 * vi:set ai:
 */
