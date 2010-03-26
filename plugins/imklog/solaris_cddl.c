#define MAXLINE 4096
/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/* Portions Copyright 2010 by Rainer Gerhards and Adiscon
 */
/*
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 *	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
 *	All Rights Reserved
 */

/*
 * University Copyright- Copyright (c) 1982, 1986, 1988
 * The Regents of the University of California
 * All Rights Reserved
 *
 * University Acknowledgment- Portions of this document are derived from
 * software developed by the University of California, Berkeley, and its
 * contributors.
 */

#include "config.h"
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/poll.h>

//---------
#include <note.h>
#include <errno.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <ctype.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <libscf.h>
#include <netconfig.h>
#include <netdir.h>
#include <pwd.h>
#include <utmpx.h>
#include <limits.h>
#include <pthread.h>
#include <fcntl.h>
#include <stropts.h>
#include <assert.h>
#include <sys/statvfs.h>

#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/syslog.h>
#include <sys/strlog.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <sys/poll.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <sys/note.h>
#include <door.h>
//--------


#include "rsyslog.h"

static struct pollfd Pfd;		/* Pollfd for local the log device */




/*
 * findnl_bkwd:
 *	Scans each character in buf until it finds the last newline in buf,
 *	or the scanned character becomes the last COMPLETE character in buf.
 *	Returns the number of scanned bytes.
 *
 *	buf - pointer to a buffer containing the message string
 *	len - the length of the buffer
 */
size_t
findnl_bkwd(const char *buf, const size_t len)
{
	const char *p;
	size_t	mb_cur_max;
	pthread_t mythreadno;

	if (Debug) {
		mythreadno = pthread_self();
	}

	if (len == 0) {
		return (0);
	}

	mb_cur_max = MB_CUR_MAX;

	if (mb_cur_max == 1) {
		/* single-byte locale */
		for (p = buf + len - 1; p != buf; p--) {
			if (*p == '\n') {
				return ((size_t)(p - buf));
			}
		}
		return ((size_t)len);
	} else {
		/* multi-byte locale */
		int mlen;
		const char *nl;
		size_t	rem;

		p = buf;
		nl = NULL;
		for (rem = len; rem >= mb_cur_max; ) {
			mlen = mblen(p, mb_cur_max);
			if (mlen == -1) {
				/*
				 * Invalid character found.
				 */
				dbgprintf("findnl_bkwd(%u): Invalid MB "
				    "sequence\n", mythreadno);
				/*
				 * handle as a single byte character.
				 */
				p++;
				rem--;
			} else {
				/*
				 * It's guaranteed that *p points to
				 * the 1st byte of a multibyte character.
				 */
				if (*p == '\n') {
					nl = p;
				}
				p += mlen;
				rem -= mlen;
			}
		}
		if (nl) {
			return ((size_t)(nl - buf));
		}
		/*
		 * no newline nor null byte found.
		 * Also it's guaranteed that *p points to
		 * the 1st byte of a (multibyte) character
		 * at this point.
		 */
		return (len - rem);
	}
}
//___ end

/*
 * Attempts to open the local log device
 * and return a file descriptor.
 */
int
sun_openklog(char *name, int mode)
{
	int fd;
	struct strioctl str;

	solaris_create_unix_socket(name);
	if ((fd = open(name, mode)) < 0) {
		//logerror("cannot open %s", name);
		dbgprintf("openklog: cannot open %s (%d)\n",
		    name, errno);
		return (-1);
	}
	str.ic_cmd = I_CONSLOG;
	str.ic_timout = 0;
	str.ic_len = 0;
	str.ic_dp = NULL;
	if (ioctl(fd, I_STR, &str) < 0) {
		//logerror("cannot register to log console messages");
		dbgprintf("openklog: cannot register to log "
		    "console messages (%d)\n", errno);
		return (-1);
	}
	Pfd.fd = fd;
	return (fd);
}


/*
 * Pull up one message from log driver.
 */
void
sun_getkmsg(int timeout)
{
	int flags = 0, i;
	char *lastline;
	struct strbuf ctl, dat;
	struct log_ctl hdr;
	char buf[MAXLINE+1];
	size_t buflen;
	size_t len;
	char tmpbuf[MAXLINE+1];

	dat.maxlen = MAXLINE;
	dat.buf = buf;
	ctl.maxlen = sizeof (struct log_ctl);
	ctl.buf = (caddr_t)&hdr;

	while ((i = getmsg(Pfd.fd, &ctl, &dat, &flags)) == MOREDATA) {
		lastline = &dat.buf[dat.len];
		*lastline = '\0';

		dbgprintf("sys_poll: getmsg: dat.len = %d\n", dat.len);
		buflen = strlen(buf);
		len = findnl_bkwd(buf, buflen);

		(void) memcpy(tmpbuf, buf, len);
		tmpbuf[len] = '\0';

		/* Format sys will enqueue the log message.
		 * Set the sync flag if timeout != 0, which
		 * means that we're done handling all the
		 * initial messages ready during startup.
		 */
		Syslog(LOG_INFO, buf);
		/*if (timeout == 0) {
			formatsys(&hdr, tmpbuf, 0);
			//sys_init_msg_count++;
		} else {
			formatsys(&hdr, tmpbuf, 1);
		}*/

		if (len != buflen) {
			/* If anything remains in buf */
			size_t remlen;

			if (buf[len] == '\n') {
				/* skip newline */
				len++;
			}

			/* Move the remaining bytes to
			 * the beginnning of buf.
			 */

			remlen = buflen - len;
			(void) memcpy(buf, &buf[len], remlen);
			dat.maxlen = MAXLINE - remlen;
			dat.buf = &buf[remlen];
		} else {
			dat.maxlen = MAXLINE;
			dat.buf = buf;
		}
	}

	if (i == 0 && dat.len > 0) {
		dat.buf[dat.len] = '\0';
		/*
		 * Format sys will enqueue the log message.
		 * Set the sync flag if timeout != 0, which
		 * means that we're done handling all the
		 * initial messages ready during startup.
		 */
		dbgprintf("getkmsg: getmsg: dat.maxlen = %d\n", dat.maxlen);
		dbgprintf("getkmsg: getmsg: dat.len = %d\n", dat.len);
		dbgprintf("getkmsg: getmsg: strlen(dat.buf) = %d\n", strlen(dat.buf));
		dbgprintf("getkmsg: getmsg: dat.buf = \"%s\"\n", dat.buf);
		dbgprintf("getkmsg: buf len = %d\n", strlen(buf));
		//if (timeout == 0) {
			//formatsys(&hdr, buf, 0);
			//--sys_init_msg_count++;
		//} else {
			//formatsys(&hdr, buf, 1);
		//}
		Syslog(LOG_INFO, buf);
	} else if (i < 0 && errno != EINTR) {
		if(1){ // (!shutting_down) {
			dbgprintf("kernel log driver read error");
		}
		// TODO trigger retry logic
		//(void) close(Pfd.fd);
		//Pfd.fd = -1;
	}
}


/*
 * Open the log device, and pull up all pending messages.
 */
void
sun_prepare_sys_poll()
{
	int nfds, funix;

/*
	if ((funix = sun_openklog(LogName, O_RDONLY)) < 0) {
		logerror("can't open kernel log device - fatal");
		exit(1);
	}
*/

	Pfd.fd = funix;
	Pfd.events = POLLIN;

	for (;;) {
		nfds = poll(&Pfd, 1, 0);
		/*
		if (nfds <= 0) {
			if (sys_init_msg_count > 0)
				flushmsg(SYNC_FILE);
			break;
		}*/

		if (Pfd.revents & POLLIN) {
			sun_getkmsg(0);
		} else if (Pfd.revents & (POLLNVAL|POLLHUP|POLLERR)) {
			//logerror("kernel log driver poll error");
			dbgprintf("kernel log driver poll error");
			break;
		}
	}

}

/*
 * this thread listens to the local stream log driver for log messages
 * generated by this host, formats them, and queues them to the logger
 * thread.
 */
/*ARGSUSED*/
void *
sun_sys_poll(void *ap)
{
	int nfds;

	dbgprintf("sys_poll: sys_thread started\n");

	/*
	 * Try to process as many messages as we can without blocking on poll.
	 * We count such "initial" messages with sys_init_msg_count and
	 * enqueue them without the SYNC_FILE flag.  When no more data is
	 * waiting on the local log device, we set timeout to INFTIM,
	 * clear sys_init_msg_count, and generate a flush message to sync
	 * the previously counted initial messages out to disk.
	 */

	for (;;) {
		errno = 0;

		nfds = poll(&Pfd, 1, INFTIM);

		if (nfds == 0)
			continue;

		if (nfds < 0) {
			if (errno != EINTR)
				dbgprintf("poll error");// logerror("poll");
			continue;
		}
		if (Pfd.revents & POLLIN) {
			sun_getkmsg(INFTIM);
		} else {
			// TODO: shutdown, the rsyslog way
			//if (shutting_down) {
				//pthread_exit(0);
			//}
			if (Pfd.revents & (POLLNVAL|POLLHUP|POLLERR)) {
			// TODO: trigger retry logic	
/*				logerror("kernel log driver poll error");
				(void) close(Pfd.fd);
				Pfd.fd = -1;
				*/
			}
		}

/*		while (Pfd.fd == -1 && klogerrs++ < 10) {
			Pfd.fd = sun_openklog(LogName, O_RDONLY);
		}
		if (klogerrs >= 10) {
			logerror("can't reopen kernel log device - fatal");
			exit(1);
		}*/
	}
	/*NOTREACHED*/
	return (NULL);
}
