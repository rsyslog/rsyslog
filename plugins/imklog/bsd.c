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


/* open the kernel log  -- rger */
static void openKLog(void)
{
	if ((fklog = open(_PATH_KLOG, O_RDONLY, 0)) >= 0)
		if (fcntl(fklog, F_SETFL, O_NONBLOCK) < 0)
			fklog = -1;
	if (fklog < 0)
		dprintf("can't open %s (%d)\n", _PATH_KLOG, errno);
}


static int	fklog = -1;	/* /dev/klog */
/*
 * Read /dev/klog while data are available, split into lines.
 */
static void
readklog(void)
{
	char *p, *q, line[MAXLINE + 1];
	int len, i;

	len = 0;
	for (;;) {
		i = read(fklog, line + len, MAXLINE - 1 - len);
		if (i > 0) {
			line[i + len] = '\0';
		} else {
			if (i < 0 && errno != EINTR && errno != EAGAIN) {
				logerror("klog");
				fklog = -1;
			}
			break;
		}

		for (p = line; (q = strchr(p, '\n')) != NULL; p = q + 1) {
			*q = '\0';
			printsys(p);
		}
		len = strlen(p);
		if (len >= MAXLINE - 1) {
			printsys(p);
			len = 0;
		}
		if (len > 0)
			memmove(line, p, len + 1);
	}
	if (len > 0)
		printsys(line);
}

/*
 * Take a raw input line from /dev/klog, format similar to syslog().
 */
static void
printsys(char *msg)
{
	char *p, *q;
	long n;
	int flags, isprintf, pri;

	flags = ISKERNEL | SYNC_FILE | ADDDATE;	/* fsync after write */
	p = msg;
	pri = DEFSPRI;
	isprintf = 1;
	if (*p == '<') {
		errno = 0;
		n = strtol(p + 1, &q, 10);
		if (*q == '>' && n >= 0 && n < INT_MAX && errno == 0) {
			p = q + 1;
			pri = n;
			isprintf = 0;
		}
	}
	/*
	 * Kernel printf's and LOG_CONSOLE messages have been displayed
	 * on the console already.
	 */
	if (isprintf || (pri & LOG_FACMASK) == LOG_CONSOLE)
		flags |= IGN_CONS;
	if (pri &~ (LOG_FACMASK|LOG_PRIMASK))
		pri = DEFSPRI;
	logmsg(pri, p, LocalHostName, flags);
}

