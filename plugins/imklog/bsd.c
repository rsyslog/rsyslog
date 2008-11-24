/* klog for BSD, based on the FreeBSD syslogd implementation.
 *
 * This contains OS-specific functionality to read the BSD
 * kernel log. For a general overview, see head comment in
 * imklog.c.
 *
 * Copyright (C) 2008 by Rainer Gerhards for the modifications of
 * the original FreeBSD sources.
 *
 * I would like to express my gratitude to those folks which
 * layed an important foundation for rsyslog to build on.
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
 *
 * This file is based on earlier work included in the FreeBSD sources. We
 * integrated it into the rsyslog project. The copyright below applies, and
 * I also reproduce the original license under which we aquired the code:
 *
 * Copyright (c) 1983, 1988, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * If you would like to use the code under the BSD license, you should
 * aquire your own copy of BSD's syslogd, from which we have taken it. The
 * code in this file is modified and may only be used under the terms of
 * the GPLv3+ as specified above.
 */

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "rsyslog.h"
#include "imklog.h"

/* globals */
static int	fklog = -1;	/* /dev/klog */

#ifndef _PATH_KLOG
#	define _PATH_KLOG "/dev/klog"
#endif

/* open the kernel log - will be called inside the willRun() imklog
 * entry point. -- rgerhards, 2008-04-09
 */
rsRetVal
klogWillRun(void)
{
	DEFiRet;

	fklog = open(_PATH_KLOG, O_RDONLY, 0);
	if (fklog < 0) {
		dbgprintf("can't open %s (%d)\n", _PATH_KLOG, errno);
		iRet = RS_RET_ERR; // TODO: better error code
	}

	RETiRet;
}


/* Read /dev/klog while data are available, split into lines.
 * Contrary to standard BSD syslogd, we do a blocking read. We can
 * afford this as imklog is running on its own threads. So if we have
 * a single file, it really doesn't matter if we wait inside a 1-file
 * select or the read() directly.
 */
static void
readklog(void)
{
	char *p, *q;
	int len, i;
	int iMaxLine;
	uchar bufRcv[4096+1];
	uchar *pRcv = NULL; /* receive buffer */

	iMaxLine = klog_getMaxLine();

	/* we optimize performance: if iMaxLine is below 4K (which it is in almost all
	 * cases, we use a fixed buffer on the stack. Only if it is higher, heap memory
	 * is used. We could use alloca() to achive a similar aspect, but there are so
	 * many issues with alloca() that I do not want to take that route.
	 * rgerhards, 2008-09-02
	 */
	if((size_t) iMaxLine < sizeof(bufRcv) - 1) {
		pRcv = bufRcv;
	} else {
		if((pRcv = (uchar*) malloc(sizeof(uchar) * (iMaxLine + 1))) == NULL)
			iMaxLine = sizeof(bufRcv) - 1; /* better this than noting */
	}

	len = 0;
	for (;;) {
		dbgprintf("----------imklog(BSD) waiting for kernel log line\n");
		i = read(fklog, pRcv + len, iMaxLine - len);
		if (i > 0) {
			pRcv[i + len] = '\0';
		} else {
			if (i < 0 && errno != EINTR && errno != EAGAIN) {
				imklogLogIntMsg(LOG_ERR,
				       "imklog error %d reading kernel log - shutting down imklog",
				       errno);
				fklog = -1;
			}
			break;
		}

		for (p = pRcv; (q = strchr(p, '\n')) != NULL; p = q + 1) {
			*q = '\0';
			Syslog(LOG_INFO, (uchar*) p);
		}
		len = strlen(p);
		if (len >= iMaxLine - 1) {
			Syslog(LOG_INFO, (uchar*)p);
			len = 0;
		}
		if (len > 0)
			memmove(pRcv, p, len + 1);
	}
	if (len > 0)
		Syslog(LOG_INFO, pRcv);

	if(pRcv != NULL && (size_t) iMaxLine >= sizeof(bufRcv) - 1)
		free(pRcv);
}


/* to be called in the module's AfterRun entry point
 * rgerhards, 2008-04-09
 */
rsRetVal klogAfterRun(void)
{
        DEFiRet;
	if(fklog != -1)
		close(fklog);
        RETiRet;
}



/* to be called in the module's WillRun entry point, this is the main
 * "message pull" mechanism.
 * rgerhards, 2008-04-09
 */
rsRetVal klogLogKMsg(void)
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
