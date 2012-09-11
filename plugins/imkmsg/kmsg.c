/* imkmsg driver for Linux /dev/kmsg structured logging
 *
 * This contains OS-specific functionality to read the BSD
 * or Linux kernel log. For a general overview, see head comment in
 * imklog.c. This started out as the BSD-specific drivers, but it
 * turned out that on modern Linux the implementation details
 * are very small, and so we use a single driver for both OS's with
 * a little help of conditional compilation.
 *
 * Copyright 2008-2012 Adiscon GmbH
 *
 * This file is part of rsyslog.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *       http://www.apache.org/licenses/LICENSE-2.0
 *       -or-
 *       see COPYING.ASL20 in the source distribution
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <sys/klog.h>

#include "rsyslog.h"
#include "srUtils.h"
#include "debug.h"
#include "imkmsg.h"

/* globals */
static int	fklog = -1;	/* kernel log fd */

#ifndef _PATH_KLOG
#	define _PATH_KLOG "/dev/kmsg"
#endif

/* submit a message to imklog Syslog() API. In this function, we parse
 * necessary information from kernel log line, and make json string
 * from the rest.
 */
static void
submitSyslog(uchar *buf)
{
	struct timeval tv;
	long offs = 0;
	long int timestamp = 0;
	struct timespec monotonic;
	struct timespec realtime;
	char outMsg[8096];
	int priority = 0;

	offs = snprintf(outMsg, 8, "%s", "@cee: {");

	/* get priority */
	for (; isdigit(*buf); buf++) {
		priority += (priority * 10) + (*buf - '0');
	}
	buf++;

	/* messages sequence number */
	offs += snprintf(outMsg+offs, 12, "%s", "\"sequnum\":\"");
	for (; isdigit(*buf); buf++, offs++) {
		outMsg[offs] = *buf;
	}
	buf++; /* skip , */

	/* get timestamp */
	for (; isdigit(*buf); buf++) {
		timestamp += (timestamp * 10) + (*buf - '0');
	}
	buf++; /* skip ; */

	offs += snprintf(outMsg+offs, 10, "%s", "\",\"msg\":\"");

	for (; *buf != '\n' && *buf != '\0'; buf++, offs++) {
		outMsg[offs] = *buf;
	}

	if (*buf != '\0') /* message has appended properties, skip \n */
		buf++;

	while (strlen((char *)buf)) {
		offs += snprintf(outMsg+offs, 4, "%s", "\",\"");
		buf++; /* skip ' ' */
		for (; *buf != '=' && *buf != ' '; buf++, offs++) { /* separator is = or ' ' */
			outMsg[offs] = *buf;
		}
		buf++; /* skip = */

		offs += snprintf(outMsg+offs, 4, "%s", "\":\"");
		for (; *buf != '\n' && *buf != '\0'; buf++, offs++) {
			outMsg[offs] = *buf;
		}

		if (*buf != '\0')
			buf++; /* another property, skip \n */
	}
	offs += snprintf(outMsg+offs, 3, "%s", "\"}");

	outMsg[offs] = '\0';

	/* calculate timestamp */
	clock_gettime(CLOCK_MONOTONIC, &monotonic);
	clock_gettime(CLOCK_REALTIME, &realtime);
	tv.tv_sec = realtime.tv_sec + ((timestamp / 1000000l) - monotonic.tv_sec);
	tv.tv_usec = (realtime.tv_nsec + ((timestamp / 1000000000l) - monotonic.tv_nsec)) / 1000;

	Syslog(priority, (uchar *)outMsg, &tv);
}


static uchar *GetPath(modConfData_t *pModConf)
{
	return pModConf->pszPath ? pModConf->pszPath : (uchar*) _PATH_KLOG;
}

/* open the kernel log - will be called inside the willRun() imklog
 * entry point. -- rgerhards, 2008-04-09
 */
rsRetVal
klogWillRun(modConfData_t *pModConf)
{
	char errmsg[2048];
	int r;
	DEFiRet;

	fklog = open((char*)GetPath(pModConf), O_RDONLY, 0);
	if (fklog < 0) {
		imklogLogIntMsg(RS_RET_ERR_OPEN_KLOG, "imkmsg: cannot open kernel log(%s): %s.",
			GetPath(pModConf), rs_strerror_r(errno, errmsg, sizeof(errmsg)));
		ABORT_FINALIZE(RS_RET_ERR_OPEN_KLOG);
	}

	/* Set level of kernel console messaging.. */
	if(pModConf->console_log_level != -1) {
		r = klogctl(8, NULL, pModConf->console_log_level);
		if(r != 0) {
			imklogLogIntMsg(LOG_WARNING, "imkmsg: cannot set console log level: %s",
				rs_strerror_r(errno, errmsg, sizeof(errmsg)));
			/* make sure we do not try to re-set! */
			pModConf->console_log_level = -1;
		}
	}

finalize_it:
	RETiRet;
}

/* Read kernel log while data are available, split into lines.
 */
static void
readklog(void)
{
	int i;
	uchar pRcv[1024+1]; /* LOG_LINE_MAX is 1024 */
	char errmsg[2048];

#if 0
XXX not sure if LOG_LINE_MAX is 1024
-       int iMaxLine;
-       uchar bufRcv[128*1024+1];
+       uchar pRcv[1024+1]; /* LOG_LINE_MAX is 1024 */
        char errmsg[2048];
-       uchar *pRcv = NULL; /* receive buffer */
-
-       iMaxLine = klog_getMaxLine();
-
-       /* we optimize performance: if iMaxLine is below our fixed size buffer (which
-        * usually is sufficiently large), we use this buffer. if it is higher, heap memory
-        * is used. We could use alloca() to achive a similar aspect, but there are so
-        * many issues with alloca() that I do not want to take that route.
-        * rgerhards, 2008-09-02
-        */
-       if((size_t) iMaxLine < sizeof(bufRcv) - 1) {
-               pRcv = bufRcv;
-       } else {
-               if((pRcv = (uchar*) MALLOC(sizeof(uchar) * (iMaxLine + 1))) == NULL)
-                       iMaxLine = sizeof(bufRcv) - 1; /* better this than noting */
-       }
#endif

	for (;;) {
		dbgprintf("imklog(BSD/Linux) waiting for kernel log line\n");

		/* every read() from the opened device node receives one record of the printk buffer */
		i = read(fklog, pRcv, 1024);

		if (i > 0) {
			/* successful read of message of nonzero length */
			pRcv[i] = '\0';
		} else {
			/* something went wrong - error or zero length message */
			if (i < 0 && errno != EINTR && errno != EAGAIN) {
				/* error occured */
				imklogLogIntMsg(LOG_ERR,
				       "imkmsg: error reading kernel log - shutting down: %s",
					rs_strerror_r(errno, errmsg, sizeof(errmsg)));
				fklog = -1;
			}
			break;
		}

		submitSyslog(pRcv);
	}
}


/* to be called in the module's AfterRun entry point
 * rgerhards, 2008-04-09
 */
rsRetVal klogAfterRun(modConfData_t *pModConf)
{
	DEFiRet;
	if(fklog != -1)
		close(fklog);
	/* Turn on logging of messages to console, but only if a log level was speficied */
	if(pModConf->console_log_level != -1)
		klogctl(7, NULL, 0);
	RETiRet;
}


/* to be called in the module's WillRun entry point, this is the main
 * "message pull" mechanism.
 * rgerhards, 2008-04-09
 */
rsRetVal klogLogKMsg(modConfData_t __attribute__((unused)) *pModConf)
{
	DEFiRet;
	readklog();
	RETiRet;
}


/* provide the (system-specific) default facility for internal messages
 * rgerhards, 2008-04-14
 */
int
klogFacilIntMsg(void)
{
	return LOG_SYSLOG;
}
