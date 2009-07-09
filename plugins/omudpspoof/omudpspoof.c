/* omudpspoof.c
 *
 * This is a udp-based output module that support spoofing.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * --------------------------------------------------------------------------------
 *
 * USAGE NOTES:
 * To use it create a template that puts the hostname-ip ahead of what you want to 
 * send, similar to
 * 
 * $template TraditionalFwdFormat,"%fromhost-ip% <%pri%>%timegenerated% %HOSTNAME% 
 * %syslogtag%%msg%\n"
 * 
 * *.*     @10.0.0.100;TraditionalFwdFormat
 * 
 * The one problem right now is that any logs sent from the local box will go out 
 * with a source IP of 127.0.0.1
 *
 * --------------------------------------------------------------------------------
 *
 * Note: this file builds on UDP spoofing code contributed by 
 * David Lang <david@lang.hm>. I then created a "real" rsyslog module
 * out of that code and omfwd. I decided to make it a separate module because
 * omfwd already mixes up too many things (TCP & UDP & a differnt modes,
 * this has historic reasons), it would not be a good idea to also add
 * spoofing to it. And, looking at the requirements, there is little in 
 * common between omfwd and this module.
 *
 * Copyright 2009 David Lang (spoofing code)
 * Copyright 2009 Rainer Gerhards and Adiscon GmbH.
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
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "net.h"
#include "template.h"
#include "msg.h"
#include "cfsysline.h"
#include "module-template.h"
#include "glbl.h"
#include "errmsg.h"
#include "dirty.h"
#include "unicode-helper.h"


#include <libnet.h>
#define _BSD_SOURCE 1
#define __BSD_SOURCE 1
#define __FAVOR_BSD 1


MODULE_TYPE_OUTPUT

/* internal structures
 */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(errmsg)
DEFobjCurrIf(glbl)
DEFobjCurrIf(net)

typedef struct _instanceData {
	char	*host;
	char	*port;
	int	*pSockArray;		/* sockets to use for UDP */
	int	bIsAddrResolved;  	/* is hostname address resolved? 0 - no, 1 - yes */
	int	compressionLevel;	/* 0 - no compression, else level for zlib */
	struct addrinfo *f_addr;
} instanceData;

/* config data */
static uchar *pszTplName = NULL; /* name of the default template to use */
static uchar *pszSourceNameTemplate = NULL; /* name of the template containing the spoofing address */


/* add some variables needed for libnet */
libnet_t *libnet_handle;
libnet_ptag_t ip, ipo;
libnet_ptag_t udp;
char errbuf[LIBNET_ERRBUF_SIZE];
u_short source_port=32000;

/* forward definitions */
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
pData->bIsAddrResolved = 0; // TODO: remove this variable altogether
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

BEGINcreateInstance
CODESTARTcreateInstance
    /* Initialize the libnet library.  Root priviledges are required.
     * this initializes a IPv4 socket to use for forging UDP packets.
     */
    libnet_handle = libnet_init(
            LIBNET_RAW4,                            /* injection type */
            NULL,                                   /* network interface */
            errbuf);                                /* errbuf */

    if (libnet_handle == NULL) {
        fprintf(stderr, "libnet_init() failed: %s\n", errbuf);
        exit(EXIT_FAILURE);
    }

ENDcreateInstance


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURERepeatedMsgReduction)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINfreeInstance
CODESTARTfreeInstance
	/* final cleanup */
	closeUDPSockets(pData);
	free(pData->port);
	free(pData->host);
	/* destroy the libnet state needed for forged UDP sources */
	libnet_destroy(libnet_handle);
ENDfreeInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	dbgprintf("%s", pData->host);
ENDdbgPrintInstInfo


/* Send a message via UDP
 * rgehards, 2007-12-20
 */
static rsRetVal UDPSend(instanceData *pData, uchar *pszSourcename, char *msg, size_t len)
{
	struct addrinfo *r;
	int lsent = 0;
	int bSendSuccess;
	int j, build_ip;
	u_char opt[20];
	struct sockaddr_in *tempaddr,source_ip;
	DEFiRet;

RUNLOG_VAR("%s", pszSourcename);
	if(pData->pSockArray == NULL) {
		CHKiRet(doTryResume(pData));
	}

	ip = ipo = udp = 0;
	if(source_port++ >= (u_short)42000){
		source_port = 32000;
	}

int iii;
	//iii = inet_pton(AF_INET, (char*)pszSourcename, &(source_ip.sin_addr));
	iii = inet_pton(AF_INET, "172.19.12.3", &(source_ip.sin_addr));
RUNLOG_VAR("%d", iii);

	bSendSuccess = FALSE;
	for (r = pData->f_addr; r; r = r->ai_next) {
		tempaddr = (struct sockaddr_in *)r->ai_addr;
		libnet_clear_packet(libnet_handle);
		udp = libnet_build_udp(
			source_port,		/* source port */
			tempaddr->sin_port,	/* destination port */
			LIBNET_UDP_H + len,	/* packet length */
			0,			/* checksum */
			(u_char*)msg,		/* payload */
			len,			/* payload size */
			libnet_handle,		/* libnet handle */
			udp);			/* libnet id */
		if (udp == -1) {
			dbgprintf("Can't build UDP header: %s\n", libnet_geterror(libnet_handle));
		}

		build_ip = 0;
		/* this is not a legal options string */
		for (j = 0; j < 20; j++) {
			opt[j] = libnet_get_prand(LIBNET_PR2);
		}
		ipo = libnet_build_ipv4_options(opt, 20, libnet_handle, ipo);
		if (ipo == -1) {
			dbgprintf("Can't build IP options: %s\n", libnet_geterror(libnet_handle));
		}
		ip = libnet_build_ipv4(
			LIBNET_IPV4_H + 20 + len + LIBNET_UDP_H, /* length */
			0,				/* TOS */
			242,				/* IP ID */
			0,				/* IP Frag */
			64,				/* TTL */
			IPPROTO_UDP,			/* protocol */
			0,				/* checksum */
			source_ip.sin_addr.s_addr,
			tempaddr->sin_addr.s_addr,
			NULL,				/* payload */
			0,				/* payload size */
			libnet_handle,			/* libnet handle */
			ip);				/* libnet id */
		if (ip == -1) {
			dbgprintf("Can't build IP header: %s\n", libnet_geterror(libnet_handle));
		}

		/* Write it to the wire. */
		lsent = libnet_write(libnet_handle);
		if (lsent == -1) {
			dbgprintf("Write error: %s\n", libnet_geterror(libnet_handle));
		} else {
			bSendSuccess = TRUE;
			break;
		}
	}
	/* finished looping */
	if (bSendSuccess == FALSE) {
		dbgprintf("error forwarding via udp, suspending\n");
		iRet = RS_RET_SUSPENDED;
	}

finalize_it:
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

	if(pData->pSockArray != NULL)
		FINALIZE;

	/* The remote address is not yet known and needs to be obtained */
	dbgprintf(" %s\n", pData->host);
	memset(&hints, 0, sizeof(hints));
	/* port must be numeric, because config file syntax requires this */
	hints.ai_flags = AI_NUMERICSERV;
	hints.ai_family = glbl.GetDefPFFamily();
	hints.ai_socktype = SOCK_DGRAM;
	if((iErr = (getaddrinfo(pData->host, getFwdPt(pData), &hints, &res))) != 0) {
		dbgprintf("could not get addrinfo for hostname '%s':'%s': %d%s\n",
			  pData->host, getFwdPt(pData), iErr, gai_strerror(iErr));
		ABORT_FINALIZE(RS_RET_SUSPENDED);
	}
	dbgprintf("%s found, resuming.\n", pData->host);
	pData->f_addr = res;
	pData->bIsAddrResolved = 1;
	pData->pSockArray = net.create_udp_socket((uchar*)pData->host, NULL, 0);

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
	char *psz; /* temporary buffering */
	register unsigned l;
	int iMaxLine;
CODESTARTdoAction
	CHKiRet(doTryResume(pData));

	iMaxLine = glbl.GetMaxLine();

	dbgprintf(" %s:%s/udpspoofs\n", pData->host, getFwdPt(pData));

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
		uLongf destLen = sizeof(out) / sizeof(Bytef);
		uLong srcLen = l;
		int ret;
		/* TODO: optimize malloc sequence? -- rgerhards, 2008-09-02 */
		CHKmalloc(out = (Bytef*) malloc(iMaxLine + iMaxLine/100 + 12));
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
		} else if(destLen+1 < l) {
			/* only use compression if there is a gain in using it! */
			dbgprintf("there is gain in compression, so we do it\n");
			psz = (char*) out;
			l = destLen + 1; /* take care for the "z" at message start! */
		}
		++destLen;
	}
#	endif

	CHKiRet(UDPSend(pData, ppString[1], psz, l));

finalize_it:
ENDdoAction


BEGINparseSelectorAct
	uchar *q;
	int i;
	int bErr;
        struct addrinfo;
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(2)
	/* first check if this config line is actually for us */
	if(strncmp((char*) p, ":omudpspoof:", sizeof(":omudpspoof:") - 1)) {
		ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
	}

	/* ok, if we reach this point, we have something for us */
	p += sizeof(":omudpspoof:") - 1; /* eat indicator sequence  (-1 because of '\0'!) */
	CHKiRet(createInstance(&pData));

	if(pszSourceNameTemplate == NULL) {
		errmsg.LogError(0, NO_ERRCODE, "No $ActionOMUDPSpoofSourceNameTemplate given, can not continue");
		ABORT_FINALIZE(RS_RET_NO_SRCNAME_TPL);
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
		CHKmalloc(pData->host = strdup((char*) q));
		*p = cTmp;
	} else {
		CHKmalloc(pData->host = strdup((char*) q));
	}

	/* process template */
	CHKiRet(cflineParseTemplateName(&p, *ppOMSR, 0, OMSR_NO_RQD_TPL_OPTS,
		(pszTplName == NULL) ? (uchar*)"RSYSLOG_TraditionalForwardFormat" : pszTplName));

	CHKiRet(OMSRsetEntry(*ppOMSR, 1, ustrdup(pszSourceNameTemplate), OMSR_NO_RQD_TPL_OPTS));
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
}


BEGINmodExit
CODESTARTmodExit
	/* release what we no longer need */
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(glbl, CORE_COMPONENT);
	objRelease(net, LM_NET_FILENAME);

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
	return RS_RET_OK;
}


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(net,LM_NET_FILENAME));

	CHKiRet(regCfSysLineHdlr((uchar *)"actionomudpspoofdefaulttemplate", 0, eCmdHdlrGetWord, NULL, &pszTplName, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionomudpspoofsourcenametemplate", 0, eCmdHdlrGetWord, NULL, &pszSourceNameTemplate, NULL));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit

/* vim:set ai:
 */
