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
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include "dirty.h"
#include "cfsysline.h"
#include "obj.h"
#include "msg.h"
#include "module-template.h"
#include "datetime.h"
#include "imklog.h"
#include "glbl.h"

MODULE_TYPE_INPUT

/* Module static data */
DEF_IMOD_STATIC_DATA
DEFobjCurrIf(datetime)
DEFobjCurrIf(glbl)

/* configuration settings */
int dbgPrintSymbols = 0; /* this one is extern so the helpers can access it! */
int symbols_twice = 0;
int use_syscall = 0;
int symbol_lookup = 0; /* on recent kernels > 2.6, the kernel does this */
int bPermitNonKernel = 0; /* permit logging of messages not having LOG_KERN facility */
int iFacilIntMsg; /* the facility to use for internal messages (set by driver) */
/* TODO: configuration for the following directives must be implemented. It 
 * was not done yet because we either do not yet have a config handler for
 * that type or I thought it was acceptable to push it to a later stage when
 * I gained more handson experience with the input module interface (and the
 * changes resulting from that). -- rgerhards, 2007-12-20
 */
char *symfile = NULL; 
int console_log_level = -1;


/* enqueue the the kernel message into the message queue.
 * The provided msg string is not freed - thus must be done
 * by the caller.
 * rgerhards, 2008-04-12
 */
static rsRetVal
enqMsg(uchar *msg, uchar* pszTag, int iFacility, int iSeverity)
{
	DEFiRet;
	msg_t *pMsg;

	assert(msg != NULL);
	assert(pszTag != NULL);

	CHKiRet(msgConstruct(&pMsg));
	MsgSetFlowControlType(pMsg, eFLOWCTL_LIGHT_DELAY);
	MsgSetInputName(pMsg, "imklog");
	MsgSetRawMsg(pMsg, (char*)msg);
	MsgSetUxTradMsg(pMsg, (char*)msg);
	MsgSetRawMsg(pMsg, (char*)msg);
	MsgSetMSG(pMsg, (char*)msg);
	MsgSetRcvFrom(pMsg, (char*)glbl.GetLocalHostName());
	MsgSetRcvFromIP(pMsg, (uchar*)"127.0.0.1");
	MsgSetHOSTNAME(pMsg, (char*)glbl.GetLocalHostName());
	MsgSetTAG(pMsg, (char*)pszTag);
	pMsg->iFacility = LOG_FAC(iFacility);
	pMsg->iSeverity = LOG_PRI(iSeverity);
	pMsg->bParseHOSTNAME = 0;
	CHKiRet(submitMsg(pMsg));

finalize_it:
	RETiRet;
}

/* parse the PRI from a kernel message. At least BSD seems to have
 * non-kernel messages inside the kernel log...
 * Expected format: "<pri>". piPri is only valid if the function 
 * successfully returns. If there was a proper pri ppSz is advanced to the
 * position right after ">".
 * rgerhards, 2008-04-14
 */
static rsRetVal
parsePRI(uchar **ppSz, int *piPri)
{
	DEFiRet;
	int i;
	uchar *pSz;

	assert(ppSz != NULL);
	pSz = *ppSz;
	assert(pSz != NULL);
	assert(piPri != NULL);

	if(*pSz != '<' || !isdigit(*(pSz+1)))
		ABORT_FINALIZE(RS_RET_INVALID_PRI);

	++pSz;
	i = 0;
	while(isdigit(*pSz)) {
		i = i * 10 + *pSz++ - '0';
	}

	if(*pSz != '>')
		ABORT_FINALIZE(RS_RET_INVALID_PRI);

	/* OK, we have a valid PRI */
	*piPri = i;
	*ppSz = pSz + 1; /* update msg ptr to position after PRI */

finalize_it:
	RETiRet;
}


/* log an imklog-internal message
 * rgerhards, 2008-04-14
 */
rsRetVal imklogLogIntMsg(int priority, char *fmt, ...)
{
	DEFiRet;
	va_list ap;
	uchar msgBuf[2048]; /* we use the same size as sysklogd to remain compatible */
	uchar *pLogMsg;

	va_start(ap, fmt);
	vsnprintf((char*)msgBuf, sizeof(msgBuf) / sizeof(char), fmt, ap);
	pLogMsg = msgBuf;
	va_end(ap);

	iRet = enqMsg((uchar*)pLogMsg, (uchar*) ((iFacilIntMsg == LOG_KERN) ? "kernel:" : "imklog:"),
		      iFacilIntMsg, LOG_PRI(priority));

	RETiRet;
}


/* log a kernel message 
 * rgerhards, 2008-04-14
 */
rsRetVal Syslog(int priority, uchar *pMsg)
{
	DEFiRet;
	rsRetVal localRet;

	/* Output using syslog */
	localRet = parsePRI(&pMsg, &priority);
	if(localRet != RS_RET_INVALID_PRI && localRet != RS_RET_OK)
		FINALIZE;
	/* if we don't get the pri, we use whatever we were supplied */

	/* ignore non-kernel messages if not permitted */
	if(bPermitNonKernel == 0 && LOG_FAC(priority) != LOG_KERN)
		FINALIZE; /* silently ignore */

	iRet = enqMsg((uchar*)pMsg, (uchar*) "kernel:", LOG_FAC(priority), LOG_PRI(priority));

finalize_it:
	RETiRet;
}


/* helper for some klog drivers which need to know the MaxLine global setting. They can
 * not obtain it themselfs, because they are no modules and can not query the object hander.
 * It would probably be a good idea to extend the interface to support it, but so far
 * we create a (sufficiently valid) work-around. -- rgerhards, 2008-11-24
 */
int klog_getMaxLine(void)
{
	return glbl.GetMaxLine();
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
	/* release objects we used */
	objRelease(glbl, CORE_COMPONENT);
	objRelease(datetime, CORE_COMPONENT);
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
	symbol_lookup = 0;
	bPermitNonKernel = 0;
	iFacilIntMsg = klogFacilIntMsg();
	return RS_RET_OK;
}

BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(datetime, CORE_COMPONENT));
	CHKiRet(objUse(glbl, CORE_COMPONENT));

	iFacilIntMsg = klogFacilIntMsg();

	CHKiRet(omsdRegCFSLineHdlr((uchar *)"debugprintkernelsymbols", 0, eCmdHdlrBinary, NULL, &dbgPrintSymbols, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"klogsymbollookup", 0, eCmdHdlrBinary, NULL, &symbol_lookup, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"klogsymbolstwice", 0, eCmdHdlrBinary, NULL, &symbols_twice, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"klogusesyscallinterface", 0, eCmdHdlrBinary, NULL, &use_syscall, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"klogpermitnonkernelfacility", 0, eCmdHdlrBinary, NULL, &bPermitNonKernel, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"kloginternalmsgfacility", 0, eCmdHdlrFacility, NULL, &iFacilIntMsg, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit
/* vim:set ai:
 */
