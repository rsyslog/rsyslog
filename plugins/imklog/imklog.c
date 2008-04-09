/* The kernel log module.
 *
 * This is an abstracted module. As Linux and BSD kernel log is conceptually the
 * same, we do not do different input plugins for them but use
 * imklog in both cases, just with different "backend drivers" for
 * the different platforms. This also enables a rsyslog.conf to
 * be used on multiple platforms without the need to take care of
 * what the kernel log is coming from.
 *
 * See platform-specific files (e.g. linux.c, bsd.c) in the plugin's
 * working directory. For other systems with similar kernel logging
 * functionality, no new input plugin shall be written but rather a
 * driver be developed for imklog. Please note that imklog itself is
 * mostly concerned with handling the interface. Any real action happens
 * in the drivers, as things may be pretty different on different
 * platforms.
 *
 * Please note that this file replaces the klogd daemon that was
 * also present in pre-v3 versions of rsyslog.
 *
 * Copyright (C) 2008 by Rainer Gerhards and Adiscon GmbH
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
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include "syslogd.h"
#include "cfsysline.h"
#include "template.h"
#include "msg.h"
#include "module-template.h"
#include "imklog.h"

MODULE_TYPE_INPUT

/* Module static data */
DEF_IMOD_STATIC_DATA

/* configuration settings TODO: move to instance data? */
int dbgPrintSymbols = 0; /* this one is extern so the helpers can access it! */
int symbols_twice = 0;
int use_syscall = 0;
int symbol_lookup = 1;
/* TODO: configuration for the following directives must be implemented. It 
 * was not done yet because we either do not yet have a config handler for
 * that type or I thought it was acceptable to push it to a later stage when
 * I gained more handson experience with the input module interface (and the
 * changes resulting from that). -- rgerhards, 2007-12-20
 */
char *symfile = NULL; 
int console_log_level = -1;


/* Includes. */
#include <unistd.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <sys/stat.h>

#if HAVE_TIME_H
#	include <time.h>
#endif

#include <stdarg.h>
#include <paths.h>

#define __LIBRARY__
#include <unistd.h>



/* Write a message to the message queue.
 * returns -1 if it fails, something else otherwise
 */
static rsRetVal writeSyslogV(int iPRI, const char *szFmt, va_list va)
{
	DEFiRet;
	int iChars;
	int iLen;
	time_t tNow;
	char msgBuf[2048]; /* we use the same size as sysklogd to remain compatible */

	assert(szFmt != NULL);

	/* build the message */
	time(&tNow);
	/* we can use sprintf safely below, because we know the size of the constants.
	 * By doing so, we save some cpu cycles and code complexity (for unnecessary
	 * error checking).
	 */
	iLen = sprintf(msgBuf, "<%d>%.15s kernel: ", iPRI, ctime(&tNow) + 4);

	iChars = vsnprintf(msgBuf + iLen, sizeof(msgBuf) / sizeof(char) - iLen, szFmt, va);

	/* here we must create our message object and supply it to the message queue
	 */
	CHKiRet(parseAndSubmitMessage(LocalHostName, msgBuf, strlen(msgBuf), MSG_DONT_PARSE_HOSTNAME, NOFLAG, eFLOWCTL_LIGHT_DELAY));

finalize_it:
	RETiRet;
}

/* And now the same with variable arguments */
static int writeSyslog(int iPRI, const char *szFmt, ...)
{
	int iRet;
	va_list va;

	assert(szFmt != NULL);
	va_start(va, szFmt);
	iRet = writeSyslogV(iPRI, szFmt, va);
	va_end(va);

	return(iRet);
}

rsRetVal Syslog(int priority, char *fmt, ...) __attribute__((format(printf,2, 3)));
rsRetVal Syslog(int priority, char *fmt, ...)
{
	DEFiRet;
	va_list ap;
	char *argl;

	/* Output using syslog */
	if(!strcmp(fmt, "%s")) {
		va_start(ap, fmt);
		argl = va_arg(ap, char *);
		if(argl[0] == '<' && argl[1] && argl[2] == '>') {
			switch(argl[1]) {
			case '0':
				priority = LOG_EMERG;
				break;
			case '1':
				priority = LOG_ALERT;
				break;
			case '2':
				priority = LOG_CRIT;
				break;
			case '3':
				priority = LOG_ERR;
				break;
			case '4':
				priority = LOG_WARNING;
				break;
			case '5':
				priority = LOG_NOTICE;
				break;
			case '6':
				priority = LOG_INFO;
				break;
			case '7':
			default:
				priority = LOG_DEBUG;
			}
			argl += 3;
		}
		iRet = writeSyslog(priority, fmt, argl);
		va_end(ap);
	} else {
		va_start(ap, fmt);
		iRet = writeSyslogV(priority, fmt, ap);
		va_end(ap);
	}

	RETiRet;
}


BEGINrunInput
CODESTARTrunInput
	/* this is an endless loop - it is terminated when the thread is
	 * signalled to do so. This, however, is handled by the framework,
	 * right into the sleep below.
	 */
	while(!pThrd->bShallStop) {
		/* klogLogKMsg() waits for the next kernel message, obtains it
                 * and then submits it to the rsyslog main queue.
	   	 * rgerhards, 2008-04-09
	   	 */
                CHKiRet(klogLogKMsg());
	}
finalize_it:
ENDrunInput


BEGINwillRun
CODESTARTwillRun
        iRet = klogWillRun();
ENDwillRun


BEGINafterRun
CODESTARTafterRun
        iRet = klogAfterRun();
ENDafterRun


BEGINmodExit
CODESTARTmodExit
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_IMOD_QUERIES
ENDqueryEtryPt

static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	dbgPrintSymbols = 0;
	symbols_twice = 0;
	use_syscall = 0;
	symfile = NULL;
	symbol_lookup = 1;
	return RS_RET_OK;
}

BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"debugprintkernelsymbols", 0, eCmdHdlrBinary, NULL, &dbgPrintSymbols, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"klogsymbollookup", 0, eCmdHdlrBinary, NULL, &symbol_lookup, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"klogsymbolstwice", 0, eCmdHdlrBinary, NULL, &symbols_twice, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"klogusesyscallinterface", 0, eCmdHdlrBinary, NULL, &use_syscall, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit
/* vim:set ai:
 */
