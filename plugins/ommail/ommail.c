/* ommail.c
 *
 * This is an implementation of a mail sending output module.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * File begun on 2008-04-04 by RGerhards
 *
 * Copyright 2008 Rainer Gerhards and Adiscon GmbH.
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
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include "syslogd.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "cfsysline.h"
#include "module-template.h"
#include "errmsg.h"

MODULE_TYPE_OUTPUT

/* internal structures
 */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(errmsg)

static uchar *pSrv;
static uchar *pszFrom;
static uchar *pszTo;

typedef struct _instanceData {
	int iMode;	/* 0 - smtp, 1 - sendmail */
	union {
		struct {
			uchar *pszSrv;
			uchar *pszSrvPort;
			uchar *pszFrom;
			uchar *pszTo;
			uchar *pszSubject;
			char RcvBuf[1024]; /* buffer for receiving server responses */
			size_t lenRcvBuf;
			size_t iRcvBuf;	/* current index into the rcvBuf (buf empty if iRcvBuf == lenRcvBuf) */
			int sock;	/* socket to this server (most important when we do multiple msgs per mail) */
			} smtp;
	} md;	/* mode-specific data */
} instanceData;


BEGINcreateInstance
CODESTARTcreateInstance
ENDcreateInstance


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURERepeatedMsgReduction)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINfreeInstance
CODESTARTfreeInstance
	if(pData->iMode == 0) {
		if(pData->md.smtp.pszSrv != NULL)
			free(pData->md.smtp.pszSrv);
		if(pData->md.smtp.pszSrvPort != NULL)
			free(pData->md.smtp.pszSrvPort);
		if(pData->md.smtp.pszFrom != NULL)
			free(pData->md.smtp.pszFrom);
		if(pData->md.smtp.pszTo != NULL)
			free(pData->md.smtp.pszTo);
		if(pData->md.smtp.pszSubject != NULL)
			free(pData->md.smtp.pszSubject);
	}
ENDfreeInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	printf("mail"); /* TODO: extend! */
ENDdbgPrintInstInfo


/* TCP support code, should probably be moved to net.c or some place else... -- rgerhards, 2008-04-04 */

/* "receive" a character from the remote server. A single character
 * is returned. Returns RS_RET_NO_MORE_DATA if the server has closed
 * the connection and RS_RET_IO_ERROR if something goes wrong. This
 * is a blocking read.
 * rgerhards, 2008-04-04
 */
static rsRetVal
getRcvChar(instanceData *pData, char *pC)
{
	DEFiRet;
	ssize_t lenBuf;
	assert(pData != NULL);

	if(pData->md.smtp.iRcvBuf == pData->md.smtp.lenRcvBuf) { /* buffer empty? */
		/* yes, we need to read the next server response */
		do {
			lenBuf = recv(pData->md.smtp.sock, pData->md.smtp.RcvBuf, sizeof(pData->md.smtp.RcvBuf), 0);
			if(lenBuf == 0) {
				ABORT_FINALIZE(RS_RET_NO_MORE_DATA);
			} else if(lenBuf < 0) {
				if(errno != EAGAIN) {
					ABORT_FINALIZE(RS_RET_IO_ERROR);
				}
			} else {
				/* good read */
				pData->md.smtp.iRcvBuf = 0;
				pData->md.smtp.lenRcvBuf = lenBuf;
			}
				
		} while(lenBuf < 1);
	}

	/* when we reach this point, we have a non-empty buffer */
	*pC = pData->md.smtp.RcvBuf[pData->md.smtp.iRcvBuf++];

finalize_it:
	RETiRet;
}


#if 0
/* Initialize TCP socket (for sender), new socket is returned in 
 * pSock if all goes well.
 */
static rsRetVal
CreateSocket(struct addrinfo *addrDest, int *pSock)
{
	DEFiRet;
	int fd;
	struct addrinfo *r; 
	char errStr[1024];
	
	r = addrDest;

	while(r != NULL) {
		fd = socket(r->ai_family, r->ai_socktype, r->ai_protocol);
		if(fd != -1) {
			if(connect(fd, r->ai_addr, r->ai_addrlen) != 0) {
				dbgprintf("create tcp connection failed, reason %s", rs_strerror_r(errno, errStr, sizeof(errStr)));
			} else {
				*pSock = fd;
				FINALIZE;
			}
			close(fd);
		} else {
			dbgprintf("couldn't create send socket, reason %s", rs_strerror_r(errno, errStr, sizeof(errStr)));
		}		
		r = r->ai_next;
	}

	dbgprintf("no working socket could be obtained");
	iRet = RS_RET_NO_SOCKET;

finalize_it:
	RETiRet;
}
#endif


/* open a connection to the mail server
 * rgerhards, 2008-04-04
 */
static rsRetVal
serverConnect(instanceData *pData)
{
	struct addrinfo *res = NULL;
	struct addrinfo hints;
	char errStr[1024];

	DEFiRet;
	assert(pData != NULL);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC; /* TODO: make configurable! */
	hints.ai_socktype = SOCK_STREAM;
	if(getaddrinfo((char*)pData->md.smtp.pszSrv, (char*)pData->md.smtp.pszSrvPort, &hints, &res) != 0) {
		dbgprintf("error %d in getaddrinfo\n", errno);
		ABORT_FINALIZE(RS_RET_IO_ERROR);
	}
	
	if((pData->md.smtp.sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
		dbgprintf("couldn't create send socket, reason %s", rs_strerror_r(errno, errStr, sizeof(errStr)));
		ABORT_FINALIZE(RS_RET_IO_ERROR);
	}

	if(connect(pData->md.smtp.sock, res->ai_addr, res->ai_addrlen) != 0) {
		dbgprintf("create tcp connection failed, reason %s", rs_strerror_r(errno, errStr, sizeof(errStr)));
		ABORT_FINALIZE(RS_RET_IO_ERROR);
	}

finalize_it:
	if(res != NULL)
               freeaddrinfo(res);
		
	if(iRet != RS_RET_OK) {
		if(pData->md.smtp.sock != -1) {
			close(pData->md.smtp.sock);
			pData->md.smtp.sock = -1;
		}
	}

	RETiRet;
}



/* send text to the server, blocking send */
static rsRetVal
Send(int sock, char *msg, size_t len)
{
	DEFiRet;
	size_t offsBuf = 0;
	ssize_t lenSend;

	assert(msg != NULL);

	if(len == 0) /* it's valid, but does not make much sense ;) */
		FINALIZE;

	do {
		lenSend = send(sock, msg + offsBuf, len - offsBuf, 0);
		dbgprintf("TCP sent %ld bytes, requested %ld\n", (long) lenSend, (long) len);

		if(lenSend == -1) {
			if(errno != EAGAIN) {
				dbgprintf("message not (tcp)send, errno %d", errno);
				ABORT_FINALIZE(RS_RET_TCP_SEND_ERROR);
			}
		} else if(lenSend != (ssize_t) len) {
			offsBuf += len; /* on to next round... */
		} else {
			FINALIZE;
		}
	} while(1);

finalize_it:
	RETiRet;
}


/* send a message via SMTP
 * TODO: care about the result codes, we can't do it that blindly ;)
 * rgerhards, 2008-04-04
 */
static rsRetVal
sendSMTP(instanceData *pData, uchar *body)
{
	DEFiRet;
	
	assert(pData != NULL);

	CHKiRet(serverConnect(pData));
	CHKiRet(Send(pData->md.smtp.sock, "HELO\r\n", 6));
	CHKiRet(Send(pData->md.smtp.sock, "MAIL FROM: <rgerhards@adiscon.com>\r\n", sizeof("MAIL FROM: <rgerhards@adiscon.com>\r\n") - 1));
	CHKiRet(Send(pData->md.smtp.sock, "RCPT TO: <rgerhards@adiscon.com>\r\n",   sizeof("RCPT TO: <rgerhards@adiscon.com>\r\n") - 1));
	CHKiRet(Send(pData->md.smtp.sock, "DATA\r\n",   sizeof("DATA\r\n") - 1));
	CHKiRet(Send(pData->md.smtp.sock, body,   strlen((char*) body)));
	CHKiRet(Send(pData->md.smtp.sock, "\r\n.\r\n",   sizeof("\r\n.\r\n") - 1));
	CHKiRet(Send(pData->md.smtp.sock, "QUIT\r\n",   sizeof("QUIT\r\n") - 1));

	close(pData->md.smtp.sock);
	pData->md.smtp.sock = -1;
	
finalize_it:
	RETiRet;
}



/* connect to server
 * rgerhards, 2008-04-04
 */
static rsRetVal doConnect(instanceData *pData)
{
	DEFiRet;

	iRet = serverConnect(pData);
	if(iRet == RS_RET_IO_ERROR)
		iRet = RS_RET_SUSPENDED;

	RETiRet;
}


BEGINtryResume
CODESTARTtryResume
	iRet = doConnect(pData);
ENDtryResume


BEGINdoAction
CODESTARTdoAction
	dbgprintf(" Mail\n");

//	if(!pData->bIsConnected) {
//		CHKiRet(doConnect(pData));
//	}

	/* forward */
	iRet = sendSMTP(pData, ppString[0]);
	if(iRet != RS_RET_OK) {
		/* error! */
		dbgprintf("error sending mail, suspending\n");
		iRet = RS_RET_SUSPENDED;
	}

finalize_it:
ENDdoAction


BEGINparseSelectorAct
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	if(!strncmp((char*) p, ":ommail:", sizeof(":ommail:") - 1)) {
		p += sizeof(":ommail:") - 1; /* eat indicator sequence (-1 because of '\0'!) */
	} else {
		ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
	}

	/* ok, if we reach this point, we have something for us */
	if((iRet = createInstance(&pData)) != RS_RET_OK)
		FINALIZE;

	//pData->md.smtp.pszSrv = "172.19.2.10";
	pData->md.smtp.pszSrv = "172.19.0.6";
	pData->md.smtp.pszSrvPort = "25";
	pData->md.smtp.pszFrom = "rgerhards@adiscon.com";
	pData->md.smtp.pszTo = "rgerhards@adiscon.com";
	pData->md.smtp.pszSubject = "rsyslog test message";

	/* process template */
	CHKiRet(cflineParseTemplateName(&p, *ppOMSR, 0, OMSR_NO_RQD_TPL_OPTS, (uchar*) "RSYSLOG_TraditionalForwardFormat"));

	/* TODO: do we need to call freeInstance if we failed - this is a general question for
	 * all output modules. I'll address it later as the interface evolves. rgerhards, 2007-07-25
	 */
CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINmodExit
CODESTARTmodExit
	/* release what we no longer need */
	objRelease(errmsg, CORE_COMPONENT);
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	/* tell which objects we need */
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
ENDmodInit

/* vim:set ai:
 */
