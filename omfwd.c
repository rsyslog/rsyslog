/* omfwd.c
 * This is the implementation of the build-in forwarding output module.
 *
 * File begun on 2007-07-20 by RGerhards (extracted from syslogd.c)
 * This file is under development and has not yet arrived at being fully
 * self-contained and a real object. So far, it is mostly an excerpt
 * of the "old" message code without any modifications. However, it
 * helps to have things at the right place one we go to the meat of it.
 *
 * Copyright 2007 Rainer Gerhards and Adiscon GmbH.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */
#include "config.h"
#ifdef SYSLOG_INET
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
#ifdef USE_PTHREADS
#include <pthread.h>
#endif
#include "rsyslog.h"
#include "syslogd.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "net.h"
#include "omfwd.h"
#include "template.h"
#include "msg.h"
#include "tcpsyslog.h"

/*
 * This table contains plain text for h_errno errors used by the
 * net subsystem.
 */
static const char *sys_h_errlist[] = {
    "No problem",						/* NETDB_SUCCESS */
    "Authoritative answer: host not found",			/* HOST_NOT_FOUND */
    "Non-authoritative answer: host not found, or serverfail",	/* TRY_AGAIN */
    "Non recoverable errors",					/* NO_RECOVERY */
    "Valid name, no data record of requested type",		/* NO_DATA */
    "no address, look for MX record"				/* NO_ADDRESS */
 };

/* query feature compatibility
 */
static rsRetVal isCompatibleWithFeature(syslogFeature eFeat)
{
	if(eFeat == sFEATURERepeatedMsgReduction)
		return RS_RET_OK;

	return RS_RET_INCOMPATIBLE;
}


/* call the shell action
 */
static rsRetVal doActionFwd(selector_t *f)
{
	char *psz; /* temporary buffering */
	register unsigned l;
	int i;
	unsigned e, lsent = 0;
	int bSendSuccess;
	time_t fwd_suspend;
	struct addrinfo *res, *r;
	struct addrinfo hints;

	assert(f != NULL);

	switch (f->f_type) {
	case F_FORW_SUSP:
		fwd_suspend = time(NULL) - f->f_un.f_forw.ttSuspend;
		if ( fwd_suspend >= INET_SUSPEND_TIME ) {
			dprintf("\nForwarding suspension over, retrying FORW ");
			f->f_type = F_FORW;
			goto f_forw;
		}
		else {
			dprintf(" %s\n", f->f_un.f_forw.f_hname);
			dprintf("Forwarding suspension not over, time left: %d.\n",
			        INET_SUSPEND_TIME - fwd_suspend);
		}
		break;
		
	/* The trick is to wait some time, then retry to get the
	 * address. If that fails retry x times and then give up.
	 *
	 * You'll run into this problem mostly if the name server you
	 * need for resolving the address is on the same machine, but
	 * is started after syslogd. 
	 */
	case F_FORW_UNKN:
	/* The remote address is not yet known and needs to be obtained */
		dprintf(" %s\n", f->f_un.f_forw.f_hname);
		fwd_suspend = time(NULL) - f->f_un.f_forw.ttSuspend;
		if(fwd_suspend >= INET_SUSPEND_TIME) {
			dprintf("Forwarding suspension to unknown over, retrying\n");
			memset(&hints, 0, sizeof(hints));
			/* port must be numeric, because config file syntax requests this */
			/* TODO: this code is a duplicate from cfline() - we should later create
			 * a common function.
			 */
			hints.ai_flags = AI_NUMERICSERV;
			hints.ai_family = family;
			hints.ai_socktype = f->f_un.f_forw.protocol == FORW_UDP ? SOCK_DGRAM : SOCK_STREAM;
			if((e = getaddrinfo(f->f_un.f_forw.f_hname,
					    getFwdSyslogPt(f), &hints, &res)) != 0) {
				dprintf("Failure: %s\n", sys_h_errlist[h_errno]);
				dprintf("Retries: %d\n", f->f_prevcount);
				if ( --f->f_prevcount < 0 ) {
					dprintf("Giving up.\n");
					f->f_type = F_UNUSED;
				}
				else
					dprintf("Left retries: %d\n", f->f_prevcount);
			}
			else {
			        dprintf("%s found, resuming.\n", f->f_un.f_forw.f_hname);
				f->f_un.f_forw.f_addr = res;
				f->f_prevcount = 0;
				f->f_type = F_FORW;
				goto f_forw;
			}
		}
		else
			dprintf("Forwarding suspension not over, time " \
				"left: %d\n", INET_SUSPEND_TIME - fwd_suspend);
		break;

	case F_FORW:
	f_forw:
		dprintf(" %s:%s/%s\n", f->f_un.f_forw.f_hname, getFwdSyslogPt(f),
			 f->f_un.f_forw.protocol == FORW_UDP ? "udp" : "tcp");
		iovCreate(f);
		if ( strcmp(getHOSTNAME(f->f_pMsg), LocalHostName) && NoHops )
			dprintf("Not sending message to remote.\n");
		else {
			f->f_un.f_forw.ttSuspend = time(NULL);
			psz = iovAsString(f);
			l = f->f_iLenpsziov;
			if (l > MAXLINE)
				l = MAXLINE;

#			ifdef	USE_NETZIP
			/* Check if we should compress and, if so, do it. We also
			 * check if the message is large enough to justify compression.
			 * The smaller the message, the less likely is a gain in compression.
			 * To save CPU cycles, we do not try to compress very small messages.
			 * What "very small" means needs to be configured. Currently, it is
			 * hard-coded but this may be changed to a config parameter.
			 * rgerhards, 2006-11-30
			 */
			if(f->f_un.f_forw.compressionLevel && (l > MIN_SIZE_FOR_COMPRESS)) {
				Bytef out[MAXLINE+MAXLINE/100+12] = "z";
				uLongf destLen = sizeof(out) / sizeof(Bytef);
				uLong srcLen = l;
				int ret;
				ret = compress2((Bytef*) out+1, &destLen, (Bytef*) psz,
						srcLen, f->f_un.f_forw.compressionLevel);
				dprintf("Compressing message, length was %d now %d, return state  %d.\n",
					l, (int) destLen, ret);
				if(ret != Z_OK) {
					/* if we fail, we complain, but only in debug mode
					 * Otherwise, we are silent. In any case, we ignore the
					 * failed compression and just sent the uncompressed
					 * data, which is still valid. So this is probably the
					 * best course of action.
					 * rgerhards, 2006-11-30
					 */
					dprintf("Compression failed, sending uncompressed message\n");
				} else if(destLen+1 < l) {
					/* only use compression if there is a gain in using it! */
					dprintf("there is gain in compression, so we do it\n");
					psz = (char*) out;
					l = destLen + 1; /* take care for the "z" at message start! */
				}
				++destLen;
			}
#			endif

			if(f->f_un.f_forw.protocol == FORW_UDP) {
				/* forward via UDP */
	                        if(finet != NULL) {
					/* we need to track if we have success sending to the remote
					 * peer. Success is indicated by at least one sendto() call
					 * succeeding. We track this be bSendSuccess. We can not simply
					 * rely on lsent, as a call might initially work, but a later
					 * call fails. Then, lsent has the error status, even though
					 * the sendto() succeeded.
					 * rgerhards, 2007-06-22
					 */
					bSendSuccess = FALSE;
					for (r = f->f_un.f_forw.f_addr; r; r = r->ai_next) {
		                       		for (i = 0; i < *finet; i++) {
		                                       lsent = sendto(finet[i+1], psz, l, 0,
		                                                      r->ai_addr, r->ai_addrlen);
							if (lsent == l) {
						       		bSendSuccess = TRUE;
								break;
							} else {
								int eno = errno;
								dprintf("sendto() error: %d = %s.\n",
									eno, strerror(eno));
							}
		                                }
						if (lsent == l && !send_to_all)
	                         	               break;
					}
					/* finished looping */
	                                if (bSendSuccess == FALSE) {
		                                f->f_type = F_FORW_SUSP;
		                                errno = 0;
		                                logerror("error forwarding via udp, suspending");
					}
				}
			} else {
				/* forward via TCP */
				if(TCPSend(f, psz, l) != 0) {
					/* error! */
					f->f_type = F_FORW_SUSP;
					errno = 0;
					logerror("error forwarding via tcp, suspending...");
				}
			}
		}
		break;
	}
	return RS_RET_OK;
}


/* try to process a selector action line. Checks if the action
 * applies to this module and, if so, processed it. If not, it
 * is left untouched. The driver will then call another module
 */
static rsRetVal parseSelectorAct(uchar **pp, selector_t *f)
{
	uchar *p, *q;
	int i;
        int error;
	int bErr;
	rsRetVal iRet = RS_RET_CONFLINE_PROCESSED;
        struct addrinfo hints, *res;
	char szTemplateName[128];

	assert(pp != NULL);
	assert(f != NULL);

	p = *pp;

	switch (*p)
	{
	case '@':
		++p; /* eat '@' */
		if(*p == '@') { /* indicator for TCP! */
			f->f_un.f_forw.protocol = FORW_TCP;
			++p; /* eat this '@', too */
			/* in this case, we also need a mutex... */
#			ifdef USE_PTHREADS
			pthread_mutex_init(&f->f_un.f_forw.mtxTCPSend, 0);
#			endif
		} else {
			f->f_un.f_forw.protocol = FORW_UDP;
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
						f->f_un.f_forw.compressionLevel = iLevel;
					} else {
						logerrorInt("Invalid compression level '%c' specified in "
						         "forwardig action - NOT turning on compression.",
							 *p);
					}
#					else
					logerror("Compression requested, but rsyslogd is not compiled "
					         "with compression support - request ignored.");
#					endif /* #ifdef USE_NETZIP */
				} else if(*p == 'o') { /* octet-couting based TCP framing? */
					++p; /* eat */
					/* no further options settable */
					f->f_un.f_forw.tcp_framing = TCP_FRAMING_OCTET_COUNTING;
				} else { /* invalid option! Just skip it... */
					logerrorInt("Invalid option %c in forwarding action - ignoring.", *p);
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
				logerror("Option block not terminated in forwarding action.");
		}
		/* extract the host first (we do a trick - we replace the ';' or ':' with a '\0')
		 * now skip to port and then template name. rgerhards 2005-07-06
		 */
		for(q = p ; *p && *p != ';' && *p != ':' ; ++p)
		 	/* JUST SKIP */;

		f->f_un.f_forw.port = NULL;
		if(*p == ':') { /* process port */
			uchar * tmp;

			*p = '\0'; /* trick to obtain hostname (later)! */
			tmp = ++p;
			for(i=0 ; *p && isdigit((int) *p) ; ++p, ++i)
				/* SKIP AND COUNT */;
			f->f_un.f_forw.port = malloc(i + 1);
			if(f->f_un.f_forw.port == NULL) {
				logerror("Could not get memory to store syslog forwarding port, "
					 "using default port, results may not be what you intend\n");
				/* we leave f_forw.port set to NULL, this is then handled by
				 * getFwdSyslogPt().
				 */
			} else {
				memcpy(f->f_un.f_forw.port, tmp, i);
				*(f->f_un.f_forw.port + i) = '\0';
			}
		}
		
		/* now skip to template */
		bErr = 0;
		while(*p && *p != ';') {
			if(*p && *p != ';' && !isspace((int) *p)) {
				if(bErr == 0) { /* only 1 error msg! */
					bErr = 1;
					errno = 0;
					logerror("invalid selector line (port), probably not doing "
					         "what was intended");
				}
			}
			++p;
		}
	
		if(*p == ';') {
			*p = '\0'; /* trick to obtain hostname (later)! */
			++p;
			 /* Now look for the template! */
			cflineParseTemplateName(&p, szTemplateName,
						sizeof(szTemplateName) / sizeof(char));
		} else
			szTemplateName[0] = '\0';
		if(szTemplateName[0] == '\0') {
			/* we do not have a template, so let's use the default */
			strcpy(szTemplateName, " StdFwdFmt");
		}

		/* first set the f->f_type */
		strcpy(f->f_un.f_forw.f_hname, (char*) q);
		memset(&hints, 0, sizeof(hints));
		/* port must be numeric, because config file syntax requests this */
		hints.ai_flags = AI_NUMERICSERV;
		hints.ai_family = family;
		hints.ai_socktype = f->f_un.f_forw.protocol == FORW_UDP ? SOCK_DGRAM : SOCK_STREAM;
		if( (error = getaddrinfo(f->f_un.f_forw.f_hname, getFwdSyslogPt(f), &hints, &res)) != 0) {
			f->f_type = F_FORW_UNKN;
			f->f_prevcount = INET_RETRY_MAX;
			f->f_un.f_forw.ttSuspend = time(NULL);
		} else {
			f->f_type = F_FORW;
			f->f_un.f_forw.f_addr = res;
		}

		/* then try to find the template and re-set f_type to UNUSED
		 * if it can not be found. */
		cflineSetTemplateAndIOV(f, szTemplateName);
		if(f->f_type == F_UNUSED)
			/* safety measure to make sure we have a valid
			 * selector line before we continue down below.
			 * rgerhards 2005-07-29
			 */
			break;

		dprintf("forwarding host: '%s:%s/%s' template '%s'\n", q, getFwdSyslogPt(f),
			 f->f_un.f_forw.protocol == FORW_UDP ? "udp" : "tcp",
			 szTemplateName);
		/*
		 * Otherwise the host might be unknown due to an
		 * inaccessible nameserver (perhaps on the same
		 * host). We try to get the ip number later, like
		 * FORW_SUSP.
		 */
		break;
	default:
		iRet = RS_RET_CONFLINE_UNPROCESSED;
		break;
	}

	if(iRet == RS_RET_CONFLINE_PROCESSED)
		*pp = p;
	return iRet;
}

/* query an entry point
 */
static rsRetVal queryEtryPt(uchar *name, rsRetVal (**pEtryPoint)())
{
	if((name == NULL) || (pEtryPoint == NULL))
		return RS_RET_PARAM_ERROR;

	*pEtryPoint = NULL;
	if(!strcmp((char*) name, "doAction")) {
		*pEtryPoint = doActionFwd;
	} else if(!strcmp((char*) name, "parseSelectorAct")) {
		*pEtryPoint = parseSelectorAct;
	} else if(!strcmp((char*) name, "isCompatibleWithFeature")) {
		*pEtryPoint = isCompatibleWithFeature;
	} /*else if(!strcmp((char*) name, "freeInstance")) {
		*pEtryPoint = freeInstanceFile;
	}*/

	return(*pEtryPoint == NULL) ? RS_RET_NOT_FOUND : RS_RET_OK;
}

/* initialize the module
 *
 * Later, much more must be done. So far, we only return a pointer
 * to the queryEtryPt() function
 * TODO: do interface version checking & handshaking
 * iIfVersRequeted is the version of the interface specification that the
 * caller would like to see being used. ipIFVersProvided is what we
 * decide to provide.
 */
rsRetVal modInitFwd(int iIFVersRequested __attribute__((unused)), int *ipIFVersProvided, rsRetVal (**pQueryEtryPt)())
{
	if((pQueryEtryPt == NULL) || (ipIFVersProvided == NULL))
		return RS_RET_PARAM_ERROR;

	*ipIFVersProvided = 1; /* so far, we only support the initial definition */

	*pQueryEtryPt = queryEtryPt;
	return RS_RET_OK;
}

#endif /* #ifdef SYSLOG_INET */
/*
 * vi:set ai:
 */
