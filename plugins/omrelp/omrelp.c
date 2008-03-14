/* omrelp.c
 *
 * This is the implementation of the RELP output module.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * File begun on 2008-03-13 by RGerhards
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
#include <time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fnmatch.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#ifdef USE_NETZIP
#include <zlib.h>
#endif
#include <pthread.h>
#include "syslogd.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "net.h"
#include "omfwd.h"
#include "template.h"
#include "msg.h"
#include "tcpsyslog.h"
#include "tcpclt.h"
#include "cfsysline.h"
#include "module-template.h"
#include "errmsg.h"

MODULE_TYPE_OUTPUT

/* internal structures
 */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(errmsg)
DEFobjCurrIf(net)
DEFobjCurrIf(tcpclt)

typedef struct _instanceData {
	char	f_hname[MAXHOSTNAMELEN+1];
	short	sock;			/* file descriptor */
	int *pSockArray;		/* sockets to use for UDP */
	enum { /* TODO: we shoud revisit these definitions */
		eDestFORW,
		eDestFORW_SUSP,
		eDestFORW_UNKN
	} eDestState;
	struct addrinfo *f_addr;
	int compressionLevel; /* 0 - no compression, else level for zlib */
	char *port;
#	define	FORW_UDP 0
#	define	FORW_TCP 1
	/* following fields for TCP-based delivery */
	tcpclt_t *pTCPClt;		/* our tcpclt object */
} instanceData;

/* get the syslog forward port from selector_t. The passed in
 * struct must be one that is setup for forwarding.
 * rgerhards, 2007-06-28
 * We may change the implementation to try to lookup the port
 * if it is unspecified. So far, we use the IANA default auf 514.
 */
char *getFwdSyslogPt(instanceData *pData)
{
	assert(pData != NULL);
	if(pData->port == NULL)
		return("514");
	else
		return(pData->port);
}

BEGINcreateInstance
CODESTARTcreateInstance
	pData->sock = -1;
ENDcreateInstance


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURERepeatedMsgReduction)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINfreeInstance
CODESTARTfreeInstance
	switch (pData->eDestState) {
		case eDestFORW:
		case eDestFORW_SUSP:
			freeaddrinfo(pData->f_addr);
			/* fall through */
		case eDestFORW_UNKN:
			if(pData->port != NULL)
				free(pData->port);
			break;
	}

	/* final cleanup */
	if(pData->sock >= 0)
		close(pData->sock);
	if(pData->pSockArray != NULL)
		net.closeUDPListenSockets(pData->pSockArray);

	tcpclt.Destruct(&pData->pTCPClt);

ENDfreeInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	printf("%s", pData->f_hname);
ENDdbgPrintInstInfo


/* Send a frame
 */
static rsRetVal TCPSendFrame(void *pvData, char *msg, size_t len)
{
	DEFiRet;
	ssize_t lenSend;
	instanceData *pData = (instanceData *) pvData;

	lenSend = send(pData->sock, msg, len, 0);
	dbgprintf("TCP sent %ld bytes, requested %ld\n", (long) lenSend, (long) len);

	if(lenSend == -1) {
		/* we have an error case - check what we can live with */
		switch(errno) {
		case EMSGSIZE:
			dbgprintf("message not (tcp)send, too large\n");
			/* This is not a real error, so it is not flagged as one */
			break;
		default:
			dbgprintf("message not (tcp)send");
			iRet = RS_RET_TCP_SEND_ERROR;
			break;
		}
	} else if(lenSend != (ssize_t) len) {
		/* no real error, could "just" not send everything... 
		 * For the time being, we ignore this...
		 * rgerhards, 2005-10-25
		 */
		dbgprintf("message not completely (tcp)send, ignoring %ld\n", (long) lenSend);
		usleep(1000); /* experimental - might be benefitial in this situation */
		/* TODO: we need to revisit this code -- rgerhards, 2007-12-28 */
	}

	RETiRet;
}


/* This function is called immediately before a send retry is attempted.
 * It shall clean up whatever makes sense.
 * rgerhards, 2007-12-28
 */
static rsRetVal TCPSendPrepRetry(void *pvData)
{
	DEFiRet;
	instanceData *pData = (instanceData *) pvData;

	assert(pData != NULL);
	close(pData->sock);
	pData->sock = -1;
	RETiRet;
}


/* initialies everything so that TCPSend can work.
 * rgerhards, 2007-12-28
 */
static rsRetVal TCPSendInit(void *pvData)
{
	DEFiRet;
	instanceData *pData = (instanceData *) pvData;

	assert(pData != NULL);
	if(pData->sock < 0) {
		if((pData->sock = tcpclt.CreateSocket(pData->f_addr)) < 0)
			iRet = RS_RET_TCP_SOCKCREATE_ERR;
	}

	RETiRet;
}


/* try to resume connection if it is not ready
 * rgerhards, 2007-08-02
 */
static rsRetVal doTryResume(instanceData *pData)
{
	DEFiRet;
	struct addrinfo *res;
	struct addrinfo hints;
	unsigned e;

	switch (pData->eDestState) {
	case eDestFORW_SUSP:
		iRet = RS_RET_OK; /* the actual check happens during doAction() only */
		pData->eDestState = eDestFORW;
		break;
		
	case eDestFORW_UNKN:
		/* The remote address is not yet known and needs to be obtained */
		dbgprintf(" %s\n", pData->f_hname);
		memset(&hints, 0, sizeof(hints));
		/* port must be numeric, because config file syntax requests this */
		/* TODO: this code is a duplicate from cfline() - we should later create
		 * a common function.
		 */
		hints.ai_flags = AI_NUMERICSERV;
		hints.ai_family = family;
		hints.ai_socktype = SOCK_STREAM;
		if((e = getaddrinfo(pData->f_hname,
				    getFwdSyslogPt(pData), &hints, &res)) == 0) {
			dbgprintf("%s found, resuming.\n", pData->f_hname);
			pData->f_addr = res;
			pData->eDestState = eDestFORW;
		} else {
			iRet = RS_RET_SUSPENDED;
		}
		break;
	case eDestFORW:
		/* rgerhards, 2007-09-11: this can not happen, but I've included it to
		 * a) make the compiler happy, b) detect any logic errors */
		assert(0);
		break;
	}

	RETiRet;
}


BEGINtryResume
CODESTARTtryResume
	iRet = doTryResume(pData);
ENDtryResume

BEGINdoAction
	char *psz; /* temporary buffering */
	register unsigned l;
CODESTARTdoAction
	switch (pData->eDestState) {
	case eDestFORW_SUSP:
		dbgprintf("internal error in omrelp.c, eDestFORW_SUSP in doAction()!\n");
		iRet = RS_RET_SUSPENDED;
		break;
		
	case eDestFORW_UNKN:
		dbgprintf("doAction eDestFORW_UNKN\n");
		iRet = doTryResume(pData);
		break;

	case eDestFORW:
		dbgprintf(" %s:%s/%s\n", pData->f_hname, getFwdSyslogPt(pData), "tcp");
		psz = (char*) ppString[0];
		l = strlen((char*) psz);
		if(l > MAXLINE)
			l = MAXLINE;

#		ifdef	USE_NETZIP

	/* TODO: move all this ZLIB code into a library module! */

		/* Check if we should compress and, if so, do it. We also
		 * check if the message is large enough to justify compression.
		 * The smaller the message, the less likely is a gain in compression.
		 * To save CPU cycles, we do not try to compress very small messages.
		 * What "very small" means needs to be configured. Currently, it is
		 * hard-coded but this may be changed to a config parameter.
		 * rgerhards, 2006-11-30
		 */
		if(pData->compressionLevel && (l > MIN_SIZE_FOR_COMPRESS)) {
			Bytef out[MAXLINE+MAXLINE/100+12] = "z";
			uLongf destLen = sizeof(out) / sizeof(Bytef);
			uLong srcLen = l;
			int ret;
			ret = compress2((Bytef*) out+1, &destLen, (Bytef*) psz,
					srcLen, pData->compressionLevel);
			dbgprintf("Compressing message, length was %d now %d, return state  %d.\n",
				l, (int) destLen, ret);
			if(ret != Z_OK) {
				/* if we fail, we complain, but only in debug mode
				 * Otherwise, we are silent. In any case, we ignore the
				 * failed compression and just sent the uncompressed
				 * data, which is still valid. So this is probably the
				 * best course of action.
				 * rgerhards, 2006-11-30
				 */
				dbgprintf("Compression failed, sending uncompressed message\n");
			} else if(destLen+1 < l) {
				/* only use compression if there is a gain in using it! */
				dbgprintf("there is gain in compression, so we do it\n");
				psz = (char*) out;
				l = destLen + 1; /* take care for the "z" at message start! */
			}
			++destLen;
		}
#			endif

		/* forward */
		rsRetVal ret;
		ret = tcpclt.Send(pData->pTCPClt, pData, psz, l);
		if(ret != RS_RET_OK) {
			/* error! */
			dbgprintf("error forwarding via tcp, suspending\n");
			pData->eDestState = eDestFORW_SUSP;
			iRet = RS_RET_SUSPENDED;
		}
		break;
	}
finalize_it:
ENDdoAction


BEGINparseSelectorAct
	uchar *q;
	int i;
        int error;
	int bErr;
        struct addrinfo hints, *res;
	TCPFRAMINGMODE tcp_framing;
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	if(!strncmp((char*) p, ":omrelp:", sizeof(":omrelp:") - 1)) {
		p += sizeof(":omrelp:") - 1; /* eat indicator sequence (-1 because of '\0'!) */
	} else {
		ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
	}

	/* ok, if we reach this point, we have something for us */
	if((iRet = createInstance(&pData)) != RS_RET_OK)
		FINALIZE;

	/* we are now after the protocol indicator. Now check if we should
	 * use compression. We begin to use a new option format for this:
	 * @(option,option)host:port
	 * The first option defined is "z[0..9]" where the digit indicates
	 * the compression level. If it is not given, 9 (best compression) is
	 * assumed. An example action statement might be:
	 * :omrelp:(z5,o)127.0.0.1:1400  
	 * Which means send via TCP with medium (5) compresion (z) to the local
	 * host on port 1400. The '0' option means that octet-couting (as in
	 * IETF I-D syslog-transport-tls) is to be used for framing (this option
	 * applies to TCP-based syslog only and is ignored when specified with UDP).
	 * That is not yet implemented.
	 * rgerhards, 2006-12-07
	 * TODO: think of all this in spite of RELP -- rgerhards, 2008-03-13
	 */
	if(*p == '(') {
		/* at this position, it *must* be an option indicator */
		do {
			++p; /* eat '(' or ',' (depending on when called) */
			/* check options */
			if(*p == 'z') { /* compression */
#					ifdef USE_NETZIP
				++p; /* eat */
				if(isdigit((int) *p)) {
					int iLevel;
					iLevel = *p - '0';
					++p; /* eat */
					pData->compressionLevel = iLevel;
				} else {
					errmsg.LogError(NO_ERRCODE, "Invalid compression level '%c' specified in "
						 "forwardig action - NOT turning on compression.",
						 *p);
				}
#					else
				errmsg.LogError(NO_ERRCODE, "Compression requested, but rsyslogd is not compiled "
					 "with compression support - request ignored.");
#					endif /* #ifdef USE_NETZIP */
			} else if(*p == 'o') { /* octet-couting based TCP framing? */
				++p; /* eat */
				/* no further options settable */
				tcp_framing = TCP_FRAMING_OCTET_COUNTING;
			} else { /* invalid option! Just skip it... */
				errmsg.LogError(NO_ERRCODE, "Invalid option %c in forwarding action - ignoring.", *p);
				++p; /* eat invalid option */
			}
			/* the option processing is done. We now do a generic skip
			 * to either the next option or the end of the option
			 * block.
			 */
			while(*p && *p != ')' && *p != ',')
				++p;	/* just skip it */
		} while(*p && *p == ','); /* Attention: do.. while() */
		if(*p == ')')
			++p; /* eat terminator, on to next */
		else
			/* we probably have end of string - leave it for the rest
			 * of the code to handle it (but warn the user)
			 */
			errmsg.LogError(NO_ERRCODE, "Option block not terminated in forwarding action.");
	}
	/* extract the host first (we do a trick - we replace the ';' or ':' with a '\0')
	 * now skip to port and then template name. rgerhards 2005-07-06
	 */
	for(q = p ; *p && *p != ';' && *p != ':' ; ++p)
		/* JUST SKIP */;

	pData->port = NULL;
	if(*p == ':') { /* process port */
		uchar * tmp;

		*p = '\0'; /* trick to obtain hostname (later)! */
		tmp = ++p;
		for(i=0 ; *p && isdigit((int) *p) ; ++p, ++i)
			/* SKIP AND COUNT */;
		pData->port = malloc(i + 1);
		if(pData->port == NULL) {
			errmsg.LogError(NO_ERRCODE, "Could not get memory to store syslog forwarding port, "
				 "using default port, results may not be what you intend\n");
			/* we leave f_forw.port set to NULL, this is then handled by
			 * getFwdSyslogPt().
			 */
		} else {
			memcpy(pData->port, tmp, i);
			*(pData->port + i) = '\0';
		}
	}
	
	/* now skip to template */
	bErr = 0;
	while(*p && *p != ';') {
		if(*p && *p != ';' && !isspace((int) *p)) {
			if(bErr == 0) { /* only 1 error msg! */
				bErr = 1;
				errno = 0;
				errmsg.LogError(NO_ERRCODE, "invalid selector line (port), probably not doing "
					 "what was intended");
			}
		}
		++p;
	}

	/* TODO: make this if go away! */
	if(*p == ';') {
		*p = '\0'; /* trick to obtain hostname (later)! */
		strcpy(pData->f_hname, (char*) q);
		*p = ';';
	} else
		strcpy(pData->f_hname, (char*) q);

	/* process template */
	if((iRet = cflineParseTemplateName(&p, *ppOMSR, 0, OMSR_NO_RQD_TPL_OPTS, (uchar*) " StdFwdFmt"))
	   != RS_RET_OK)
		goto finalize_it;

	/* first set the pData->eDestState */
	memset(&hints, 0, sizeof(hints));
	/* port must be numeric, because config file syntax requests this */
	hints.ai_flags = AI_NUMERICSERV;
	hints.ai_family = family;
	hints.ai_socktype = SOCK_STREAM;
	if( (error = getaddrinfo(pData->f_hname, getFwdSyslogPt(pData), &hints, &res)) != 0) {
		pData->eDestState = eDestFORW_UNKN;
	} else {
		pData->eDestState = eDestFORW;
		pData->f_addr = res;
	}
	/*
	 * Otherwise the host might be unknown due to an
	 * inaccessible nameserver (perhaps on the same
	 * host). We try to get the ip number later, like
	 * FORW_SUSP.
	 */

	/* create our tcpclt */
	CHKiRet(tcpclt.Construct(&pData->pTCPClt));
	/* and set callbacks */
	CHKiRet(tcpclt.SetSendInit(pData->pTCPClt, TCPSendInit));
	CHKiRet(tcpclt.SetSendFrame(pData->pTCPClt, TCPSendFrame));
	CHKiRet(tcpclt.SetSendPrepRetry(pData->pTCPClt, TCPSendPrepRetry));
	CHKiRet(tcpclt.SetFraming(pData->pTCPClt, tcp_framing));

	/* TODO: do we need to call freeInstance if we failed - this is a general question for
	 * all output modules. I'll address it lates as the interface evolves. rgerhards, 2007-07-25
	 */
CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINneedUDPSocket
CODESTARTneedUDPSocket
	iRet = RS_RET_TRUE;
ENDneedUDPSocket


BEGINmodExit
CODESTARTmodExit
	/* release what we no longer need */
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(net, LM_NET_FILENAME);
	objRelease(tcpclt, LM_TCPCLT_FILENAME);
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(net, LM_NET_FILENAME));
	CHKiRet(objUse(tcpclt, LM_TCPCLT_FILENAME));
ENDmodInit

/* vim:set ai:
 */
