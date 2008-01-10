/* imtcp.c
 * This is the implementation of the TCP input module.
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
#include "rsyslog.h"
#include "syslogd.h"
#include "cfsysline.h"
#include "module-template.h"
#include "tcpsyslog.h"

MODULE_TYPE_INPUT

/* defines */

/* Module static data */
DEF_IMOD_STATIC_DATA

typedef struct _instanceData {
} instanceData;

/* config settings */


/* This function is called to gather input.
 */
BEGINrunInput
	int maxfds;
	int nfds;
	int i;
	int iTCPSess;
	fd_set readfds;
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

		/* Add the TCP listen sockets to the list of read descriptors.
	    	 */
		if(sockTCPLstn != NULL && *sockTCPLstn) {
			for (i = 0; i < *sockTCPLstn; i++) {
				/* The if() below is theoretically not needed, but I leave it in
				 * so that a socket may become unsuable during execution. That
				 * feature is not yet supported by the current code base.
				 */
				if (sockTCPLstn[i+1] != -1) {
					if(Debug)
						debugListenInfo(sockTCPLstn[i+1], "TCP");
					FD_SET(sockTCPLstn[i+1], &readfds);
					if(sockTCPLstn[i+1]>maxfds) maxfds=sockTCPLstn[i+1];
				}
			}
			/* do the sessions */
			iTCPSess = TCPSessGetNxtSess(-1);
			while(iTCPSess != -1) {
				int fdSess;
				fdSess = pTCPSessions[iTCPSess].sock;
				dbgprintf("Adding TCP Session %d\n", fdSess);
				FD_SET(fdSess, &readfds);
				if (fdSess>maxfds) maxfds=fdSess;
				/* now get next... */
				iTCPSess = TCPSessGetNxtSess(iTCPSess);
			}
		}

		if(Debug) {
			dbgprintf("--------imTCP calling select, active file descriptors (max %d): ", maxfds);
			for (nfds = 0; nfds <= maxfds; ++nfds)
				if ( FD_ISSET(nfds, &readfds) )
					dbgprintf("%d ", nfds);
			dbgprintf("\n");
		}

		/* wait for io to become ready */
		nfds = select(maxfds+1, (fd_set *) &readfds, NULL, NULL, NULL);

		for (i = 0; i < *sockTCPLstn; i++) {
			if (FD_ISSET(sockTCPLstn[i+1], &readfds)) {
				dbgprintf("New connect on TCP inetd socket: #%d\n", sockTCPLstn[i+1]);
#				ifdef USE_GSSAPI
				if(bEnableTCP & ALLOWEDMETHOD_GSS)
					TCPSessGSSAccept(sockTCPLstn[i+1]);
				else
#				endif
					TCPSessAccept(sockTCPLstn[i+1]);
				--nfds; /* indicate we have processed one */
			}
		}

		/* now check the sessions */
		iTCPSess = TCPSessGetNxtSess(-1);
		while(nfds && iTCPSess != -1) {
			int fdSess;
			int state;
			fdSess = pTCPSessions[iTCPSess].sock;
			if(FD_ISSET(fdSess, &readfds)) {
				char buf[MAXLINE];
				dbgprintf("tcp session socket with new data: #%d\n", fdSess);

				/* Receive message */
#				ifdef USE_GSSAPI
				int allowedMethods = pTCPSessions[iTCPSess].allowedMethods;
				if(allowedMethods & ALLOWEDMETHOD_GSS)
					state = TCPSessGSSRecv(iTCPSess, buf, sizeof(buf));
				else
#				endif
					state = recv(fdSess, buf, sizeof(buf), 0);
				if(state == 0) {
#					ifdef USE_GSSAPI
					if(allowedMethods & ALLOWEDMETHOD_GSS)
						TCPSessGSSClose(iTCPSess);
					else {
#					endif
						/* process any incomplete frames left over */
						TCPSessPrepareClose(iTCPSess);
						/* Session closed */
						TCPSessClose(iTCPSess);
#					ifdef USE_GSSAPI
					}
#					endif
				} else if(state == -1) {
					logerrorInt("TCP session %d will be closed, error ignored\n",
						    fdSess);
#					ifdef USE_GSSAPI
					if(allowedMethods & ALLOWEDMETHOD_GSS)
						TCPSessGSSClose(iTCPSess);
					else
#					endif
						TCPSessClose(iTCPSess);
				} else {
					/* valid data received, process it! */
					if(TCPSessDataRcvd(iTCPSess, buf, state) == 0) {
						/* in this case, something went awfully wrong.
						 * We are instructed to terminate the session.
						 */
						logerrorInt("Tearing down TCP Session %d - see "
							    "previous messages for reason(s)\n",
							    iTCPSess);
#						ifdef USE_GSSAPI
						if(allowedMethods & ALLOWEDMETHOD_GSS)
							TCPSessGSSClose(iTCPSess);
						else
#						endif
							TCPSessClose(iTCPSess);
					}
				}
				--nfds; /* indicate we have processed one */
			}
			iTCPSess = TCPSessGetNxtSess(iTCPSess);
		}
	}

	return iRet;
ENDrunInput


/* initialize and return if will run or not */
BEGINwillRun
CODESTARTwillRun
	/* first apply some config settings */
dbgprintf("imtcp: bEnableTCP %d\n", bEnableTCP);
	PrintAllowedSenders(2); /* TCP */
#ifdef USE_GSSAPI
	PrintAllowedSenders(3); /* GSS */
#endif
	if (bEnableTCP) {
		if(sockTCPLstn == NULL) {
#			ifdef USE_GSSAPI
			if(bEnableTCP & ALLOWEDMETHOD_GSS) {
				if(TCPSessGSSInit()) {
					logerror("GSS-API initialization failed\n");
					bEnableTCP &= ~(ALLOWEDMETHOD_GSS);
				}
			}
			if(bEnableTCP)
#			endif
				if((sockTCPLstn = create_tcp_socket()) != NULL) {
					dbgprintf("Opened %d syslog TCP port(s).\n", *sockTCPLstn);
				}
		}
	}
ENDwillRun


BEGINafterRun
CODESTARTafterRun
	/* do cleanup here */
	if (pAllowedSenders_TCP != NULL) {
		clearAllowedSenders (pAllowedSenders_TCP);
		pAllowedSenders_TCP = NULL;
	}
#ifdef USE_GSSAPI
	if (pAllowedSenders_GSS != NULL) {
		clearAllowedSenders (pAllowedSenders_GSS);
		pAllowedSenders_GSS = NULL;
	}
#endif
ENDafterRun


BEGINfreeInstance
CODESTARTfreeInstance
ENDfreeInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
ENDdbgPrintInstInfo


BEGINmodExit
CODESTARTmodExit
	/* Close the TCP inet socket. */
	if(sockTCPLstn != NULL && *sockTCPLstn) {
		deinit_tcp_listener();
	}
#ifdef USE_GSSAPI
	if(bEnableTCP & ALLOWEDMETHOD_GSS)
		TCPSessGSSDeinit();
#endif
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_IMOD_QUERIES
ENDqueryEtryPt

static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
#if defined(USE_GSSAPI)
	if (gss_listen_service_name != NULL) {
		free(gss_listen_service_name);
		gss_listen_service_name = NULL;
	}
#endif
	return RS_RET_OK;
}


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = 1; /* so far, we only support the initial definition */
CODEmodInit_QueryRegCFSLineHdlr
	/* register config file handlers */
#if defined(USE_GSSAPI)
	CHKiRet(regCfSysLineHdlr((uchar *)"gsslistenservicename", 0, eCmdHdlrGetWord, NULL, &gss_listen_service_name, NULL));
#endif
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler,
		resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit
/*
 * vi:set ai:
 */
