/* omfwd.c
 * This is the implementation of the build-in forwarding output module.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * File begun on 2007-07-20 by RGerhards (extracted from syslogd.c)
 * This file is under development and has not yet arrived at being fully
 * self-contained and a real object. So far, it is mostly an excerpt
 * of the "old" message code without any modifications. However, it
 * helps to have things at the right place one we go to the meat of it.
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
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "net.h"
#include "netstrms.h"
#include "netstrm.h"
#include "omfwd.h"
#include "template.h"
#include "msg.h"
#include "tcpclt.h"
#include "cfsysline.h"
#include "module-template.h"
#include "glbl.h"
#include "errmsg.h"

MODULE_TYPE_OUTPUT

/* internal structures
 */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(errmsg)
DEFobjCurrIf(glbl)
DEFobjCurrIf(net)
DEFobjCurrIf(netstrms)
DEFobjCurrIf(netstrm)
DEFobjCurrIf(tcpclt)

typedef struct _instanceData {
	netstrms_t *pNS; /* netstream subsystem */
	netstrm_t *pNetstrm; /* our output netstream */
	uchar *pszStrmDrvr;
	uchar *pszStrmDrvrAuthMode;
	permittedPeers_t *pPermPeers;
	int iStrmDrvrMode;
	char	*f_hname;
	int *pSockArray;	/* sockets to use for UDP */
	int bIsConnected;  /* are we connected to remote host? 0 - no, 1 - yes, UDP means addr resolved */
	struct addrinfo *f_addr;
	int compressionLevel;	/* 0 - no compression, else level for zlib */
	char *port;
	int protocol;
	int iUDPRebindInterval;	/* rebind interval */
	int iTCPRebindInterval;	/* rebind interval */
	int nXmit;		/* number of transmissions since last (re-)bind */
#	define	FORW_UDP 0
#	define	FORW_TCP 1
	/* following fields for TCP-based delivery */
	tcpclt_t *pTCPClt;	/* our tcpclt object */
} instanceData;

/* config data */
static uchar *pszTplName = NULL; /* name of the default template to use */
static uchar *pszStrmDrvr = NULL; /* name of the stream driver to use */
static short iStrmDrvrMode = 0; /* mode for stream driver, driver-dependent (0 mostly means plain tcp) */
static short bResendLastOnRecon = 0; /* should the last message be re-sent on a successful reconnect? */
static uchar *pszStrmDrvrAuthMode = NULL; /* authentication mode to use */
static int iUDPRebindInterval = 0;	/* support for automatic re-binding (load balancers!). 0 - no rebind */
static int iTCPRebindInterval = 0;	/* support for automatic re-binding (load balancers!). 0 - no rebind */

static permittedPeers_t *pPermPeers = NULL;

static rsRetVal doTryResume(instanceData *pData);

/* Close the UDP sockets.
 * rgerhards, 2009-05-29
 */
static rsRetVal
closeUDPSockets(instanceData *pData)
{
	DEFiRet;
	assert(pData != NULL);
	if(pData->pSockArray != NULL) {
		net.closeUDPListenSockets(pData->pSockArray);
		pData->pSockArray = NULL;
		freeaddrinfo(pData->f_addr);
		pData->f_addr = NULL;
	}
pData->bIsConnected = 0; // TODO: remove this variable altogether
	RETiRet;
}


/* get the syslog forward port from selector_t. The passed in
 * struct must be one that is setup for forwarding.
 * rgerhards, 2007-06-28
 * We may change the implementation to try to lookup the port
 * if it is unspecified. So far, we use the IANA default auf 514.
 */
static char *getFwdPt(instanceData *pData)
{
	assert(pData != NULL);
	if(pData->port == NULL)
		return("514");
	else
		return(pData->port);
}


/* destruct the TCP helper objects
 * This, for example, is needed after something went wrong.
 * This function is void because it "can not" fail.
 * rgerhards, 2008-06-04
 */
static inline void
DestructTCPInstanceData(instanceData *pData)
{
	assert(pData != NULL);
	if(pData->pNetstrm != NULL)
		netstrm.Destruct(&pData->pNetstrm);
	if(pData->pNS != NULL)
		netstrms.Destruct(&pData->pNS);
}

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
	/* final cleanup */
	DestructTCPInstanceData(pData);
	closeUDPSockets(pData);

	if(pData->protocol == FORW_TCP) {
		tcpclt.Destruct(&pData->pTCPClt);
	}

	free(pData->port);
	free(pData->f_hname);
	free(pData->pszStrmDrvr);
	free(pData->pszStrmDrvrAuthMode);
	net.DestructPermittedPeers(&pData->pPermPeers);
ENDfreeInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	dbgprintf("%s", pData->f_hname);
ENDdbgPrintInstInfo


/* Send a message via UDP
 * rgehards, 2007-12-20
 */
static rsRetVal UDPSend(instanceData *pData, char *msg, size_t len)
{
	DEFiRet;
	struct addrinfo *r;
	int i;
	unsigned lsent = 0;
	int bSendSuccess;

	if(pData->iUDPRebindInterval && (pData->nXmit++ % pData->iUDPRebindInterval == 0)) {
		dbgprintf("omfwd dropping UDP 'connection' (as configured)\n");
		pData->nXmit = 1;	/* else we have an addtl wrap at 2^31-1 */
		CHKiRet(closeUDPSockets(pData));
	}

	if(pData->pSockArray == NULL) {
		CHKiRet(doTryResume(pData));
	}

	if(pData->pSockArray != NULL) {
		/* we need to track if we have success sending to the remote
		 * peer. Success is indicated by at least one sendto() call
		 * succeeding. We track this be bSendSuccess. We can not simply
		 * rely on lsent, as a call might initially work, but a later
		 * call fails. Then, lsent has the error status, even though
		 * the sendto() succeeded. -- rgerhards, 2007-06-22
		 */
		bSendSuccess = FALSE;
		for (r = pData->f_addr; r; r = r->ai_next) {
			for (i = 0; i < *pData->pSockArray; i++) {
			       lsent = sendto(pData->pSockArray[i+1], msg, len, 0, r->ai_addr, r->ai_addrlen);
				if (lsent == len) {
					bSendSuccess = TRUE;
					break;
				} else {
					int eno = errno;
					char errStr[1024];
					dbgprintf("sendto() error: %d = %s.\n",
						eno, rs_strerror_r(eno, errStr, sizeof(errStr)));
				}
			}
			if (lsent == len && !send_to_all)
			       break;
		}
		/* finished looping */
		if (bSendSuccess == FALSE) {
			dbgprintf("error forwarding via udp, suspending\n");
			iRet = RS_RET_SUSPENDED;
		}
	}

finalize_it:
	RETiRet;
}


/* set the permitted peers -- rgerhards, 2008-05-19
 */
static rsRetVal
setPermittedPeer(void __attribute__((unused)) *pVal, uchar *pszID)
{
	DEFiRet;
	CHKiRet(net.AddPermittedPeer(&pPermPeers, pszID));
	free(pszID); /* no longer needed, but we must free it as of interface def */
finalize_it:
	RETiRet;
}



/* CODE FOR SENDING TCP MESSAGES */


/* Send a frame via plain TCP protocol
 * rgerhards, 2007-12-28
 */
static rsRetVal TCPSendFrame(void *pvData, char *msg, size_t len)
{
	DEFiRet;
	ssize_t lenSend;
	instanceData *pData = (instanceData *) pvData;

	lenSend = len;
	netstrm.CheckConnection(pData->pNetstrm); /* hack for plain tcp syslog - see ptcp driver for details */
	CHKiRet(netstrm.Send(pData->pNetstrm, (uchar*)msg, &lenSend));
	dbgprintf("TCP sent %ld bytes, requested %ld\n", (long) lenSend, (long) len);

	if(lenSend != (ssize_t) len) {
		/* no real error, could "just" not send everything... 
		 * For the time being, we ignore this...
		 * rgerhards, 2005-10-25
		 */
		dbgprintf("message not completely (tcp)send, ignoring %ld\n", (long) lenSend);
		usleep(1000); /* experimental - might be benefitial in this situation */
		/* TODO: we need to revisit this code -- rgerhards, 2007-12-28 */
	}

finalize_it:
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
	DestructTCPInstanceData(pData);
	RETiRet;
}


/* initializes everything so that TCPSend can work.
 * rgerhards, 2007-12-28
 */
static rsRetVal TCPSendInit(void *pvData)
{
	DEFiRet;
	instanceData *pData = (instanceData *) pvData;

	assert(pData != NULL);
	if(pData->pNetstrm == NULL) {
		CHKiRet(netstrms.Construct(&pData->pNS));
		/* the stream driver must be set before the object is finalized! */
		CHKiRet(netstrms.SetDrvrName(pData->pNS, pszStrmDrvr));
		CHKiRet(netstrms.ConstructFinalize(pData->pNS));

		/* now create the actual stream and connect to the server */
		CHKiRet(netstrms.CreateStrm(pData->pNS, &pData->pNetstrm));
		CHKiRet(netstrm.ConstructFinalize(pData->pNetstrm));
		CHKiRet(netstrm.SetDrvrMode(pData->pNetstrm, pData->iStrmDrvrMode));
		/* now set optional params, but only if they were actually configured */
		if(pData->pszStrmDrvrAuthMode != NULL) {
			CHKiRet(netstrm.SetDrvrAuthMode(pData->pNetstrm, pData->pszStrmDrvrAuthMode));
		}
		if(pData->pPermPeers != NULL) {
			CHKiRet(netstrm.SetDrvrPermPeers(pData->pNetstrm, pData->pPermPeers));
		}
		/* params set, now connect */
		CHKiRet(netstrm.Connect(pData->pNetstrm, glbl.GetDefPFFamily(),
			(uchar*)getFwdPt(pData), (uchar*)pData->f_hname));
	}

finalize_it:
	if(iRet != RS_RET_OK) {
		DestructTCPInstanceData(pData);
	}

	RETiRet;
}


/* try to resume connection if it is not ready
 * rgerhards, 2007-08-02
 */
static rsRetVal doTryResume(instanceData *pData)
{
	int iErr;
	struct addrinfo *res;
	struct addrinfo hints;
	DEFiRet;

	if(pData->bIsConnected)
		FINALIZE;

	/* The remote address is not yet known and needs to be obtained */
	dbgprintf(" %s\n", pData->f_hname);
	if(pData->protocol == FORW_UDP) {
		memset(&hints, 0, sizeof(hints));
		/* port must be numeric, because config file syntax requires this */
		hints.ai_flags = AI_NUMERICSERV;
		hints.ai_family = glbl.GetDefPFFamily();
		hints.ai_socktype = SOCK_DGRAM;
		if((iErr = (getaddrinfo(pData->f_hname, getFwdPt(pData), &hints, &res))) != 0) {
			dbgprintf("could not get addrinfo for hostname '%s':'%s': %d%s\n",
				  pData->f_hname, getFwdPt(pData), iErr, gai_strerror(iErr));
			ABORT_FINALIZE(RS_RET_SUSPENDED);
		}
		dbgprintf("%s found, resuming.\n", pData->f_hname);
		pData->f_addr = res;
		pData->bIsConnected = 1;
		if(pData->pSockArray == NULL) {
			pData->pSockArray = net.create_udp_socket((uchar*)pData->f_hname, NULL, 0);
		}
	} else {
		CHKiRet(TCPSendInit((void*)pData));
	}

finalize_it:
	if(iRet != RS_RET_OK) {
		if(pData->f_addr != NULL) {
			freeaddrinfo(pData->f_addr);
			pData->f_addr = NULL;
		}
		iRet = RS_RET_SUSPENDED;
	}

	RETiRet;
}


BEGINtryResume
CODESTARTtryResume
	iRet = doTryResume(pData);
ENDtryResume

BEGINdoAction
	char *psz = NULL; /* temporary buffering */
	register unsigned l;
	int iMaxLine;
CODESTARTdoAction
	CHKiRet(doTryResume(pData));

	iMaxLine = glbl.GetMaxLine();

	dbgprintf(" %s:%s/%s\n", pData->f_hname, getFwdPt(pData),
		 pData->protocol == FORW_UDP ? "udp" : "tcp");

	psz = (char*) ppString[0];
	l = strlen((char*) psz);
	if((int) l > iMaxLine)
		l = iMaxLine;

#	ifdef	USE_NETZIP
	/* Check if we should compress and, if so, do it. We also
	 * check if the message is large enough to justify compression.
	 * The smaller the message, the less likely is a gain in compression.
	 * To save CPU cycles, we do not try to compress very small messages.
	 * What "very small" means needs to be configured. Currently, it is
	 * hard-coded but this may be changed to a config parameter.
	 * rgerhards, 2006-11-30
	 */
	if(pData->compressionLevel && (l > MIN_SIZE_FOR_COMPRESS)) {
		Bytef *out;
		uLongf destLen = iMaxLine + iMaxLine/100 +12; /* recommended value from zlib doc */
		uLong srcLen = l;
		int ret;
		/* TODO: optimize malloc sequence? -- rgerhards, 2008-09-02 */
		CHKmalloc(out = (Bytef*) malloc(destLen));
		out[0] = 'z';
		out[1] = '\0';
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
			free(out);
		} else if(destLen+1 < l) {
			/* only use compression if there is a gain in using it! */
			dbgprintf("there is gain in compression, so we do it\n");
			psz = (char*) out;
			l = destLen + 1; /* take care for the "z" at message start! */
		} else {
			free(out);
		}
		++destLen;
	}
#	endif

	if(pData->protocol == FORW_UDP) {
		/* forward via UDP */
		CHKiRet(UDPSend(pData, psz, l));
	} else {
		/* forward via TCP */
		rsRetVal ret;
		ret = tcpclt.Send(pData->pTCPClt, pData, psz, l);
		if(ret != RS_RET_OK) {
			/* error! */
			dbgprintf("error forwarding via tcp, suspending\n");
			DestructTCPInstanceData(pData);
			iRet = RS_RET_SUSPENDED;
		}
	}
finalize_it:
#	ifdef USE_NETZIP
	if((psz != NULL) && (psz != (char*) ppString[0]))  {
		/* we need to free temporary buffer, alloced above - Naoya Nakazawa, 2010-01-11 */
		free(psz);
	}
#	endif
ENDdoAction


/* This function loads TCP support, if not already loaded. It will be called
 * during config processing. To server ressources, TCP support will only
 * be loaded if it actually is used. -- rgerhard, 2008-04-17
 */
static rsRetVal
loadTCPSupport(void)
{
	DEFiRet;
	CHKiRet(objUse(netstrms, LM_NETSTRMS_FILENAME));
	CHKiRet(objUse(netstrm, LM_NETSTRMS_FILENAME));
	CHKiRet(objUse(tcpclt, LM_TCPCLT_FILENAME));

finalize_it:
	RETiRet;
}


BEGINparseSelectorAct
	uchar *q;
	int i;
	int bErr;
	rsRetVal localRet;
        struct addrinfo;
	TCPFRAMINGMODE tcp_framing = TCP_FRAMING_OCTET_STUFFING;
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	if(*p != '@')
		ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);

	CHKiRet(createInstance(&pData));

	++p; /* eat '@' */
	if(*p == '@') { /* indicator for TCP! */
		localRet = loadTCPSupport();
		if(localRet != RS_RET_OK) {
			errmsg.LogError(0, localRet, "could not activate network stream modules for TCP "
					"(internal error %d) - are modules missing?", localRet);
			ABORT_FINALIZE(localRet);
		}
		pData->protocol = FORW_TCP;
		++p; /* eat this '@', too */
	} else {
		pData->protocol = FORW_UDP;
	}
	/* we are now after the protocol indicator. Now check if we should
	 * use compression. We begin to use a new option format for this:
	 * @(option,option)host:port
	 * The first option defined is "z[0..9]" where the digit indicates
	 * the compression level. If it is not given, 9 (best compression) is
	 * assumed. An example action statement might be:
	 * @@(z5,o)127.0.0.1:1400  
	 * Which means send via TCP with medium (5) compresion (z) to the local
	 * host on port 1400. The '0' option means that octet-couting (as in
	 * IETF I-D syslog-transport-tls) is to be used for framing (this option
	 * applies to TCP-based syslog only and is ignored when specified with UDP).
	 * That is not yet implemented.
	 * rgerhards, 2006-12-07
	 * In order to support IPv6 addresses, we must introduce an extension to
	 * the hostname. If it is in square brackets, whatever is in them is treated as
	 * the hostname - without any exceptions ;) -- rgerhards, 2008-08-05
	 */
	if(*p == '(') {
		/* at this position, it *must* be an option indicator */
		do {
			++p; /* eat '(' or ',' (depending on when called) */
			/* check options */
			if(*p == 'z') { /* compression */
#				ifdef USE_NETZIP
				++p; /* eat */
				if(isdigit((int) *p)) {
					int iLevel;
					iLevel = *p - '0';
					++p; /* eat */
					pData->compressionLevel = iLevel;
				} else {
					errmsg.LogError(0, NO_ERRCODE, "Invalid compression level '%c' specified in "
						 "forwardig action - NOT turning on compression.",
						 *p);
				}
#				else
				errmsg.LogError(0, NO_ERRCODE, "Compression requested, but rsyslogd is not compiled "
					 "with compression support - request ignored.");
#				endif /* #ifdef USE_NETZIP */
			} else if(*p == 'o') { /* octet-couting based TCP framing? */
				++p; /* eat */
				/* no further options settable */
				tcp_framing = TCP_FRAMING_OCTET_COUNTING;
			} else { /* invalid option! Just skip it... */
				errmsg.LogError(0, NO_ERRCODE, "Invalid option %c in forwarding action - ignoring.", *p);
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
			errmsg.LogError(0, NO_ERRCODE, "Option block not terminated in forwarding action.");
	}

	/* extract the host first (we do a trick - we replace the ';' or ':' with a '\0')
	 * now skip to port and then template name. rgerhards 2005-07-06
	 */
	if(*p == '[') { /* everything is hostname upto ']' */
		++p; /* skip '[' */
		for(q = p ; *p && *p != ']' ; ++p)
			/* JUST SKIP */;
		if(*p == ']') {
			*p = '\0'; /* trick to obtain hostname (later)! */
			++p; /* eat it */
		}
	} else { /* traditional view of hostname */
		for(q = p ; *p && *p != ';' && *p != ':' && *p != '#' ; ++p)
			/* JUST SKIP */;
	}

	pData->port = NULL;
	if(*p == ':') { /* process port */
		uchar * tmp;

		*p = '\0'; /* trick to obtain hostname (later)! */
		tmp = ++p;
		for(i=0 ; *p && isdigit((int) *p) ; ++p, ++i)
			/* SKIP AND COUNT */;
		pData->port = malloc(i + 1);
		if(pData->port == NULL) {
			errmsg.LogError(0, NO_ERRCODE, "Could not get memory to store syslog forwarding port, "
				 "using default port, results may not be what you intend\n");
			/* we leave f_forw.port set to NULL, this is then handled by getFwdPt(). */
		} else {
			memcpy(pData->port, tmp, i);
			*(pData->port + i) = '\0';
		}
	}
	
	/* now skip to template */
	bErr = 0;
	while(*p && *p != ';'  && *p != '#' && !isspace((int) *p))
		++p; /*JUST SKIP*/

	/* TODO: make this if go away! */
	if(*p == ';' || *p == '#' || isspace(*p)) {
		uchar cTmp = *p;
		*p = '\0'; /* trick to obtain hostname (later)! */
		CHKmalloc(pData->f_hname = strdup((char*) q));
		*p = cTmp;
	} else {
		CHKmalloc(pData->f_hname = strdup((char*) q));
	}

	/* copy over config data as needed */
	pData->iUDPRebindInterval = iUDPRebindInterval;
	pData->iTCPRebindInterval = iTCPRebindInterval;

	/* process template */
	CHKiRet(cflineParseTemplateName(&p, *ppOMSR, 0, OMSR_NO_RQD_TPL_OPTS,
		(pszTplName == NULL) ? (uchar*)"RSYSLOG_TraditionalForwardFormat" : pszTplName));

	if(pData->protocol == FORW_TCP) {
		/* create our tcpclt */
		CHKiRet(tcpclt.Construct(&pData->pTCPClt));
		CHKiRet(tcpclt.SetResendLastOnRecon(pData->pTCPClt, bResendLastOnRecon));
		/* and set callbacks */
		CHKiRet(tcpclt.SetSendInit(pData->pTCPClt, TCPSendInit));
		CHKiRet(tcpclt.SetSendFrame(pData->pTCPClt, TCPSendFrame));
		CHKiRet(tcpclt.SetSendPrepRetry(pData->pTCPClt, TCPSendPrepRetry));
		CHKiRet(tcpclt.SetFraming(pData->pTCPClt, tcp_framing));
		CHKiRet(tcpclt.SetRebindInterval(pData->pTCPClt, pData->iTCPRebindInterval));
		pData->iStrmDrvrMode = iStrmDrvrMode;
		if(pszStrmDrvr != NULL)
			CHKmalloc(pData->pszStrmDrvr = (uchar*)strdup((char*)pszStrmDrvr));
		if(pszStrmDrvrAuthMode != NULL)
			CHKmalloc(pData->pszStrmDrvrAuthMode =
				     (uchar*)strdup((char*)pszStrmDrvrAuthMode));
		if(pPermPeers != NULL) {
			pData->pPermPeers = pPermPeers;
			pPermPeers = NULL;
		}
	}

CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


/* a common function to free our configuration variables - used both on exit
 * and on $ResetConfig processing. -- rgerhards, 2008-05-16
 */
static void
freeConfigVars(void)
{
	if(pszTplName != NULL) {
		free(pszTplName);
		pszTplName = NULL;
	}
	if(pszStrmDrvr != NULL) {
		free(pszStrmDrvr);
		pszStrmDrvr = NULL;
	}
	if(pszStrmDrvrAuthMode != NULL) {
		free(pszStrmDrvrAuthMode);
		pszStrmDrvrAuthMode = NULL;
	}
	if(pPermPeers != NULL) {
		free(pPermPeers);
	}
}


BEGINmodExit
CODESTARTmodExit
	/* release what we no longer need */
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(glbl, CORE_COMPONENT);
	objRelease(net, LM_NET_FILENAME);
	objRelease(netstrm, LM_NETSTRMS_FILENAME);
	objRelease(netstrms, LM_NETSTRMS_FILENAME);
	objRelease(tcpclt, LM_TCPCLT_FILENAME);

	freeConfigVars();
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
ENDqueryEtryPt


/* Reset config variables for this module to default values.
 * rgerhards, 2008-03-28
 */
static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	freeConfigVars();

	/* we now must reset all non-string values */
	iStrmDrvrMode = 0;
	bResendLastOnRecon = 0;
	iUDPRebindInterval = 0;
	iTCPRebindInterval = 0;

	return RS_RET_OK;
}


BEGINmodInit(Fwd)
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(net,LM_NET_FILENAME));

	CHKiRet(regCfSysLineHdlr((uchar *)"actionforwarddefaulttemplate", 0, eCmdHdlrGetWord, NULL, &pszTplName, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionsendtcprebindinterval", 0, eCmdHdlrInt, NULL, &iTCPRebindInterval, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionsendudprebindinterval", 0, eCmdHdlrInt, NULL, &iUDPRebindInterval, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionsendstreamdriver", 0, eCmdHdlrGetWord, NULL, &pszStrmDrvr, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionsendstreamdrivermode", 0, eCmdHdlrInt, NULL, &iStrmDrvrMode, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionsendstreamdriverauthmode", 0, eCmdHdlrGetWord, NULL, &pszStrmDrvrAuthMode, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionsendstreamdriverpermittedpeer", 0, eCmdHdlrGetWord, setPermittedPeer, NULL, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionsendresendlastmsgonreconnect", 0, eCmdHdlrBinary, NULL, &bResendLastOnRecon, NULL));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit

/* vim:set ai:
 */
