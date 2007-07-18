/* logger.c
 * Helper routines for klogd. It replaces the regular glibc syslog()
 * call. The reason is that the glibc version does not support
 * logging with the kernel facility. This file is a re-implementation
 * of the functions with only the functionality required for klogd.
 * So it can NOT be used as a general-purpose replacment.
 * Thanks to being special, this version is deemed to be considerable
 * faster than its glibc counterpart.
 * To avoid confusion, the syslog() replacement is named writeSyslog().
 * all other functions are just helpers to it.
 * RGerhards, 2007-06-14
 *
 * Copyright (C) 2007 Rainer Gerhards
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

#include <stdio.h>
#include <assert.h>
#include <sys/socket.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <unistd.h>

#define PATH_LOGNAME "/dev/log"		/* be sure you know what you do if you change this! */

static struct sockaddr LoggerAddr;	/* AF_UNIX address of local logger */
static int connected;			/* have done connect */
static int fdLog = -1;			/* fd for log - -1 if closed */

/* klogOpenLog
 * opens the system log for writing. The log is opened immediately.
 * If it is already open, no action is taken)
 */
static void klogOpenlog(void)
{
	if (fdLog == -1) {
		LoggerAddr.sa_family = AF_UNIX;
		strncpy(LoggerAddr.sa_data, PATH_LOGNAME, sizeof(LoggerAddr.sa_data));
		fdLog = socket(AF_UNIX, SOCK_DGRAM, 0);
	}
	if (fdLog != -1 && !connected)
		if(connect(fdLog, &LoggerAddr, sizeof(LoggerAddr.sa_family) +
		    sizeof(PATH_LOGNAME) - 1 /* for \0 byte!*/ ) != -1)
		connected = 1;
}

/* Close the log file if it is currently open.
 */
static void klogCloselog(void)
{
	if(fdLog != -1)
		close(fdLog);
	fdLog = -1;
	connected = 0;
}

/* Write a message to the syslogd.
 * returns -1 if it fails, something else otherwise
 */
int writeSyslogV(int iPRI, const char *szFmt, va_list va)
{
	int iChars;
	int iLen;
	time_t tNow;
	int iWritten;	/* number of bytes written */
	char msgBuf[2048]; /* we use the same size as sysklogd to remain compatible */

	assert(szFmt != NULL);

	if (fdLog < 0 || !connected)
		klogOpenlog();

	/* build the message */
	time(&tNow);
	/* we can use sprintf safely below, because we know the size of the constants.
	 * By doing so, we save some cpu cycles and code complexity (for unnecessary
	 * error checking).
	 */
	iLen = sprintf(msgBuf, "<%d>%.15s kernel: ", iPRI, ctime(&tNow) + 4);

	iChars = vsnprintf(msgBuf + iLen, sizeof(msgBuf) / sizeof(char) - iLen, szFmt, va);
	if(iChars > (int)(sizeof(msgBuf) / sizeof(char) - iLen))
		iLen = sizeof(msgBuf) / sizeof(char) - 1; /* full buffer siz minus \0 byte */
	else
		iLen += iChars;

	/* output the message to the local logger */
	iWritten = write(fdLog, msgBuf, iLen);
	/* Debug aid below - uncomment to use
	printf("wrote to log(%d): '%s'\n", iWritten, msgBuf); */

	if(iWritten == -1) {
		/* retry */
		klogCloselog();
		klogOpenlog();
		iWritten = write(fdLog, msgBuf, iLen);
	}

	return(iWritten);
}

/* And now the same with variable arguments */
int writeSyslog(int iPRI, const char *szFmt, ...)
{
	int iRet;
	va_list va;

	assert(szFmt != NULL);
	va_start(va, szFmt);
	iRet = writeSyslogV(iPRI, szFmt, va);
	va_end(va);

	return(iRet);
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 * vi:set ai:
 */
