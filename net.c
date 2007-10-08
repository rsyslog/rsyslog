/* net.c
 * Implementation of network-related stuff.
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

#include "rsyslog.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <netdb.h>

#include "syslogd.h"
#include "syslogd-types.h"
#include "net.h"

/* The following #ifdef sequence is a small compatibility 
 * layer. It tries to work around the different availality
 * levels of SO_BSDCOMPAT on linuxes...
 * I borrowed this code from
 *    http://www.erlang.org/ml-archive/erlang-questions/200307/msg00037.html
 * It still needs to be a bit better adapted to rsyslog.
 * rgerhards 2005-09-19
 */
#ifndef BSD
#include <sys/utsname.h>
int should_use_so_bsdcompat(void)
{
    static int init_done;
    static int so_bsdcompat_is_obsolete;

    if (!init_done) {
	struct utsname utsname;
	unsigned int version, patchlevel;

	init_done = 1;
	if (uname(&utsname) < 0) {
		char errStr[1024];
		dbgprintf("uname: %s\r\n", strerror_r(errno, errStr, sizeof(errStr)));
		return 1;
	}
	/* Format is <version>.<patchlevel>.<sublevel><extraversion>
	   where the first three are unsigned integers and the last
	   is an arbitrary string. We only care about the first two. */
	if (sscanf(utsname.release, "%u.%u", &version, &patchlevel) != 2) {
	    dbgprintf("uname: unexpected release '%s'\r\n",
		    utsname.release);
	    return 1;
	}
	/* SO_BSCOMPAT is deprecated and triggers warnings in 2.5
	   kernels. It is a no-op in 2.4 but not in 2.2 kernels. */
	if (version > 2 || (version == 2 && patchlevel >= 5))
	    so_bsdcompat_is_obsolete = 1;
    }
    return !so_bsdcompat_is_obsolete;
}
#else	/* #ifndef BSD */
#define should_use_so_bsdcompat() 1
#endif	/* #ifndef BSD */
#ifndef SO_BSDCOMPAT
/* this shall prevent compiler errors due to undfined name */
#define SO_BSDCOMPAT 0
#endif


/* get the hostname of the message source. This was originally in cvthname()
 * but has been moved out of it because of clarity and fuctional separation.
 * It must be provided by the socket we received the message on as well as
 * a NI_MAXHOST size large character buffer for the FQDN.
 *
 * Please see http://www.hmug.org/man/3/getnameinfo.php (under Caveats)
 * for some explanation of the code found below. We do by default not
 * discard message where we detected malicouos DNS PTR records. However,
 * there is a user-configurabel option that will tell us if
 * we should abort. For this, the return value tells the caller if the
 * message should be processed (1) or discarded (0).
 */
/* TODO: after the bughunt, make this function static - rgerhards, 2007-09-18 */
rsRetVal gethname(struct sockaddr_storage *f, uchar *pszHostFQDN)
{
	DEFiRet;
	int error;
	sigset_t omask, nmask;
	char ip[NI_MAXHOST];
	struct addrinfo hints, *res;
	
	assert(f != NULL);
	assert(pszHostFQDN != NULL);

        error = getnameinfo((struct sockaddr *)f, SALEN((struct sockaddr *)f),
			    ip, sizeof ip, NULL, 0, NI_NUMERICHOST);

        if (error) {
                dbgprintf("Malformed from address %s\n", gai_strerror(error));
		strcpy((char*) pszHostFQDN, "???");
		ABORT_FINALIZE(RS_RET_INVALID_SOURCE);
	}

	if (!DisableDNS) {
		sigemptyset(&nmask);
		sigaddset(&nmask, SIGHUP);
		pthread_sigmask(SIG_BLOCK, &nmask, &omask);

		error = getnameinfo((struct sockaddr *)f, SALEN((struct sockaddr *) f),
				    (char*)pszHostFQDN, NI_MAXHOST, NULL, 0, NI_NAMEREQD);
		
		if (error == 0) {
			memset (&hints, 0, sizeof (struct addrinfo));
			hints.ai_flags = AI_NUMERICHOST;
			hints.ai_socktype = SOCK_DGRAM;

			/* we now do a lookup once again. This one should fail,
			 * because we should not have obtained a non-numeric address. If
			 * we got a numeric one, someone messed with DNS!
			 */
			if (getaddrinfo ((char*)pszHostFQDN, NULL, &hints, &res) == 0) {
				uchar szErrMsg[1024];
				freeaddrinfo (res);
				/* OK, we know we have evil. The question now is what to do about
				 * it. One the one hand, the message might probably be intended
				 * to harm us. On the other hand, losing the message may also harm us.
				 * Thus, the behaviour is controlled by the $DropMsgsWithMaliciousDnsPTRRecords
				 * option. If it tells us we should discard, we do so, else we proceed,
				 * but log an error message together with it.
				 * time being, we simply drop the name we obtained and use the IP - that one
				 * is OK in any way. We do also log the error message. rgerhards, 2007-07-16
		 		 */
		 		if(bDropMalPTRMsgs == 1) {
					snprintf((char*)szErrMsg, sizeof(szErrMsg) / sizeof(uchar),
						 "Malicious PTR record, message dropped "
						 "IP = \"%s\" HOST = \"%s\"",
						 ip, pszHostFQDN);
					logerror((char*)szErrMsg);
					pthread_sigmask(SIG_SETMASK, &omask, NULL);
					ABORT_FINALIZE(RS_RET_MALICIOUS_ENTITY);
				}

				/* Please note: we deal with a malicous entry. Thus, we have crafted
				 * the snprintf() below so that all text is in front of the entry - maybe
				 * it contains characters that make the message unreadable
				 * (OK, I admit this is more or less impossible, but I am paranoid...)
				 * rgerhards, 2007-07-16
				 */
				snprintf((char*)szErrMsg, sizeof(szErrMsg) / sizeof(uchar),
					 "Malicious PTR record (message accepted, but used IP "
					 "instead of PTR name: IP = \"%s\" HOST = \"%s\"",
					 ip, pszHostFQDN);
				logerror((char*)szErrMsg);

				error = 1; /* that will trigger using IP address below. */
			}
		}		
		pthread_sigmask(SIG_SETMASK, &omask, NULL);
	}

        if (error || DisableDNS) {
                dbgprintf("Host name for your address (%s) unknown\n", ip);
		strcpy((char*) pszHostFQDN, ip);
		ABORT_FINALIZE(RS_RET_ADDRESS_UNKNOWN);
        }

finalize_it:
	return iRet;
}


/* Return a printable representation of a host address.
 * Now (2007-07-16) also returns the full host name (if it could be obtained)
 * in the second param [thanks to mildew@gmail.com for the patch].
 * The caller must provide buffer space for pszHost and pszHostFQDN. These
 * buffers must be of size NI_MAXHOST. This is not checked here, because
 * there is no way to check it. We use this way of doing things because it
 * frees us from using dynamic memory allocation where it really does not
 * pay.
 */
rsRetVal cvthname(struct sockaddr_storage *f, uchar *pszHost, uchar *pszHostFQDN)
{
	DEFiRet;
	register uchar *p;
	int count;
	
	assert(f != NULL);
	assert(pszHost != NULL);
	assert(pszHostFQDN != NULL);

	iRet = gethname(f, pszHostFQDN);

	if(iRet == RS_RET_INVALID_SOURCE || iRet == RS_RET_ADDRESS_UNKNOWN) {
		strcpy((char*) pszHost, (char*) pszHostFQDN); /* we use whatever was provided as replacement */
		ABORT_FINALIZE(RS_RET_OK); /* this is handled, we are happy with it */
	} else if(iRet != RS_RET_OK) {
		FINALIZE; /* we return whatever error state we have - can not handle it */
	}

	/* if we reach this point, we obtained a non-numeric hostname and can now process it */

	/* Convert to lower case, just like LocalDomain above
	 */
	for (p = pszHostFQDN ; *p ; p++)
		if (isupper((int) *p))
			*p = tolower(*p);
	
	/* OK, the fqdn is now known. Now it is time to extract only the hostname
	 * part if we were instructed to do so.
	 */
	/* TODO: quick and dirty right now: we need to optimize that. We simply
	 * copy over the buffer and then use the old code. In the long term, that should
	 * be placed in its own function and probably outside of the net module (at least
	 * if should no longer reley on syslogd.c's global config-setting variables).
	 * Note that the old code always removes the local domain. We may want to
	 * make this in option in the long term. (rgerhards, 2007-09-11)
	 */
	strcpy((char*)pszHost, (char*)pszHostFQDN);
	if ((p = (uchar*) strchr((char*)pszHost, '.'))) { /* find start of domain name "machine.example.com" */
		if(strcmp((char*) (p + 1), LocalDomain) == 0) {
			*p = '\0'; /* simply terminate the string */
		} else {
			/* now check if we belong to any of the domain names that were specified
			 * in the -s command line option. If so, remove and we are done.
			 */
			if (StripDomains) {
				count=0;
				while (StripDomains[count]) {
					if (strcmp((char*)(p + 1), StripDomains[count]) == 0) {
						*p = '\0';
						FINALIZE; /* we are done */
					}
					count++;
				}
			}
			/* if we reach this point, we have not found any domain we should strip. Now
			 * we try and see if the host itself is listed in the -l command line option
			 * and so should be stripped also. If so, we do it and return. Please note that
			 * -l list FQDNs, not just the hostname part. If it did just list the hostname, the
			 * door would be wide-open for all kinds of mixing up of hosts. Because of this,
			 * you'll see comparison against the full string (pszHost) below. The termination
			 * still occurs at *p, which points at the first dot after the hostname.
			 */
			if (LocalHosts) {
				count=0;
				while (LocalHosts[count]) {
					if (!strcmp((char*)pszHost, LocalHosts[count])) {
						*p = '\0';
						break; /* we are done */
					}
					count++;
				}
			}
		}
	}

finalize_it:
	return iRet;
}

#endif /* #ifdef SYSLOG_INET */
/*
 * vi:set ai:
 */
