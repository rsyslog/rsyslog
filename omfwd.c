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

/* call the shell action
 * returns 0 if it succeeds, something else otherwise
 */
int doActionFwd(selector_t *f)
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
	return 0;
}

#endif /* #ifdef SYSLOG_INET */
/*
 * vi:set ai:
 */
