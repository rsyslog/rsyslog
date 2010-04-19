/* imsolaris.c
 * This input module is used to gather local log data under Solaris. This
 * includes messages from local applications AS WELL AS the kernel log.
 * I first considered to make all of this available via imklog, but that
 * did not lock appropriately on second thought. So I created this module
 * that does anything for local message recption.
 *
 * This module is not meant to be used on plaforms other than Solaris. As
 * such, trying to compile it elswhere will probably fail with all sorts
 * of errors.
 * 
 * Some notes on the Solaris syslog mechanism:
 * Both system (kernel) and application log messages are provided via
 * a single message stream.
 *
 * Solaris checks if the syslogd is running. If so, syslog() emits messages
 * to the log socket, only. Otherwise, it emits messages to the console.
 * It is possible to gather these console messages as well. However, then
 * we clutter the console.
 * Solaris does this "syslogd alive check" in a somewhat unexpected way
 * (at least unexpected for me): it uses the so-called "door" mechanism, a
 * fast RPC facility. I first thought that the door API was used to submit
 * the actual syslog messages. But this is not the case. Instead, a door
 * call is done, and the server process inside rsyslog simply does NOTHING
 * but return. All that Solaris sylsogd() is interested in is if the door
 * server (we) responds and thus can be considered alive. The actual message
 * is then submitted via the usual stream. I have to admit I do not 
 * understand why the message itself is not passed via this high-performance
 * API. But anyhow, that's nothing I can change, so the most important thing
 * is to note how Solaris does this thing ;)
 * The syslog() library call checks syslogd state for *each* call (what a 
 * waste of time...) and decides each time if the message should go to the
 * console or not.  According to OpenSolaris sources, it looks like there is
 * message loss potential when the door file is created before all data has
 * been pulled from the stream. While I have to admit that I do not fully
 * understand that problem, I will follow the original code advise and do
 * one complete pull cycle on the log socket (until it has no further data
 * available) and only thereafter create the door file and start the "regular"
 * pull cycle. As of my understanding, there is a minimal race between the
 * point where the intial pull cycle has ended and the door file is created,
 * but that race is also present in OpenSolaris syslogd code, so it should
 * not matter that much (plus, I do not know how to avoid it...)
 *
 * File begun on 2010-04-15 by RGerhards
 *
 * Copyright 2010 Rainer Gerhards and Adiscon GmbH.
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
#include <string.h>
#include <stropts.h>
#include <sys/strlog.h>
#include <errno.h>
#include "dirty.h"
#include "cfsysline.h"
#include "unicode-helper.h"
#include "module-template.h"
#include "srUtils.h"
#include "errmsg.h"
#include "net.h"
#include "glbl.h"
#include "msg.h"
#include "prop.h"
#include "sun_cddl.h"

MODULE_TYPE_INPUT

/* defines */
#define PATH_LOG	"/dev/log"


/* Module static data */
DEF_IMOD_STATIC_DATA
DEFobjCurrIf(errmsg)
DEFobjCurrIf(glbl)
DEFobjCurrIf(prop)


static int logfd = -1; /* file descriptor to access the system log */


/* config settings */
static prop_t *pInputName = NULL;	/* our inputName currently is always "imuxsock", and this will hold it */
static char *LogName = NULL;	/* the log socket name TODO: make configurable! */



/* This function receives data from a socket indicated to be ready
 * to receive and submits the message received for processing.
 * rgerhards, 2007-12-20
 * Interface changed so that this function is passed the array index
 * of the socket which is to be processed. This eases access to the
 * growing number of properties. -- rgerhards, 2008-08-01
 */
rsRetVal
solaris_readLog(int fd)
{
	DEFiRet;
	int iRcvd;
	int iMaxLine;
	struct strbuf data;
	struct strbuf ctl;
	struct log_ctl hdr;
	int flags;
	msg_t *pMsg;
	int ret;
	uchar bufRcv[4096+1];
	uchar *pRcv = NULL; /* receive buffer */
	char errStr[1024];
	uchar fmtBuf[10240]; // TODO: use better solution

	assert(logfd >= 0);

	iMaxLine = glbl.GetMaxLine();

	/* we optimize performance: if iMaxLine is below 4K (which it is in almost all
	 * cases, we use a fixed buffer on the stack. Only if it is higher, heap memory
	 * is used. We could use alloca() to achive a similar aspect, but there are so
	 * many issues with alloca() that I do not want to take that route.
	 * rgerhards, 2008-09-02
	 */
	if((size_t) iMaxLine < sizeof(bufRcv) - 1) {
		pRcv = bufRcv;
	} else {
		CHKmalloc(pRcv = (uchar*) malloc(sizeof(uchar) * (iMaxLine + 1)));
	}

	data.buf = (char*)pRcv;
	data.maxlen = iMaxLine;
	ctl.maxlen = sizeof (struct log_ctl);
	ctl.buf = (caddr_t)&hdr;
	flags = 0;
	ret = getmsg(fd, &ctl, &data, &flags);
	if(ret < 0) {
		rs_strerror_r(errno, errStr, sizeof(errStr));
		dbgprintf("imsolaris: getmsg() error on fd %d: %s.\n", fd, errStr);
	}
	dbgprintf("imsolaris: getmsg() returns %d\n", ret);
	dbgprintf("imsolaris: message from log socket: #%d: %s\n", fd, pRcv);
	if (1) {//iRcvd > 0) {
		// TODO: use hdr info! This whole section is a work-around to get
		// it going.
#if 0		
		CHKiRet(msgConstruct(&pMsg));
		//MsgSetFlowControlType(pMsg, eFLOWCTL_FULL_DELAY);
		MsgSetInputName(pMsg, pInputName);
		MsgSetRawMsg(pMsg, (char*)pRcv, strlen((char*)pRcv));
		MsgSetMSGoffs(pMsg, 0); /* we do not have a header... */
		MsgSetHOSTNAME(pMsg, glbl.GetLocalHostName(), ustrlen(glbl.GetLocalHostName()));
		//MsgSetTAG(pMsg, pInfo->pszTag, pInfo->lenTag);
		//pMsg->iFacility = LOG_FAC(pInfo->iFacility);
		//pMsg->iSeverity = LOG_PRI(pInfo->iSeverity);
		pMsg->bParseHOSTNAME = 0;
		CHKiRet(submitMsg(pMsg));
#else
		iRcvd = snprintf((char*)fmtBuf, sizeof(fmtBuf), "<%d>%s", hdr.pri, (char*) pRcv);
		parseAndSubmitMessage(glbl.GetLocalHostName(),
				      (uchar*)"127.0.0.1", fmtBuf,
			 	      iRcvd, 0,
				      0, pInputName, NULL, 0);
#endif				      
	} else if (iRcvd < 0 && errno != EINTR) {
		int en = errno;
		rs_strerror_r(en, errStr, sizeof(errStr));
		dbgprintf("imsolaris: stream error: %d = %s.\n", errno, errStr);
		//errmsg.LogError(en, NO_ERRCODE, "imsolaris: socket error UNIX");
	}

finalize_it:
	if(pRcv != NULL && (size_t) iMaxLine >= sizeof(bufRcv) - 1)
		free(pRcv);

	RETiRet;
}


/* This function is called to gather input. */
BEGINrunInput
CODESTARTrunInput
	/* this is an endless loop - it is terminated when the thread is
	 * signalled to do so. This, however, is handled by the framework,
	 * right into the sleep below.
	 */

	dbgprintf("imsolaris: prepare_sys_poll()\n");
	prepare_sys_poll();

	/* note: sun's syslogd code claims that the door should only
	 * be opened when the log socket has been polled. So file header
	 * comment of this file for more details.
	 */
	sun_open_door();
	dbgprintf("imsolaris: starting regular poll loop\n");
	while(1) {
		sun_sys_poll();
	}

	RETiRet;
ENDrunInput


BEGINwillRun
CODESTARTwillRun
	/* we need to create the inputName property (only once during our lifetime) */
	CHKiRet(prop.Construct(&pInputName));
	CHKiRet(prop.SetString(pInputName, UCHAR_CONSTANT("imsolaris"), sizeof("imsolaris") - 1));
	CHKiRet(prop.ConstructFinalize(pInputName));

	iRet = sun_openklog((LogName == NULL) ? PATH_LOG : LogName, &logfd);
	dbgprintf("imsolaris opened system log socket as fd %d\n", logfd);
	if(iRet != RS_RET_OK) {
		errmsg.LogError(0, iRet, "error opening system log socket");
	}
finalize_it:
ENDwillRun


BEGINafterRun
CODESTARTafterRun
	/* do cleanup here */
	if(pInputName != NULL)
		prop.Destruct(&pInputName);
ENDafterRun


BEGINmodExit
CODESTARTmodExit
	sun_delete_doorfiles();
	objRelease(glbl, CORE_COMPONENT);
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(prop, CORE_COMPONENT);
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_IMOD_QUERIES
ENDqueryEtryPt

static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	return RS_RET_OK;
}


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(prop, CORE_COMPONENT));

	dbgprintf("imsolaris version %s initializing\n", PACKAGE_VERSION);

	/* register config file handlers */
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler,
		resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit
/* vim:set ai:
 */
