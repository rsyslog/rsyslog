/** 
 * The rfc3195 input module.
 *
 * Please note that this file replaces the rfc3195d daemon that was
 * also present in pre-v3 versions of rsyslog.
 *
 * WARNING: due to no demand at all for RFC3195, we have converted rfc3195d
 * to this input module, but we have NOT conducted any testing. Also,
 * the module does not yet properly handle the recovery case. If someone
 * intends to put this module into production, good testing should be
 * made and it also is a good idea to notify me that you intend to use
 * it in production. In this case, I'll probably give the module another
 * cleanup. I don't do this now because so far it looks just like a big
 * waste of time. -- rgerhards, 2008-04-16
 *
 * \author  Rainer Gerhards <rgerhards@adiscon.com>
 *
 * Copyright 2003-2008 Rainer Gerhards and Adiscon GmbH.
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

#include <stdio.h>
#include <unistd.h>
#include <sys/errno.h>
#include <assert.h>
#include "rsyslog.h"
#include "dirty.h"
#include "liblogging/liblogging.h"
#include "liblogging/srAPI.h"
#include "liblogging/syslogmessage.h"
#include "module-template.h"
#include "cfsysline.h"
#include "errmsg.h"

MODULE_TYPE_INPUT

/* Module static data */
DEF_IMOD_STATIC_DATA
DEFobjCurrIf(errmsg)

/* configuration settings */
static int listenPort = 601;

/* we use a global API object below, because this listener is
 * not very complex. As such, this hack should not harm anything.
 * rgerhards, 2005-10-12
 */
static srAPIObj* pAPI;


/* This method is called when a message has been fully received.
 * It passes the received message to the rsyslog main message
 * queue. Please note that this callback is synchronous, thus
 * liblogging will be on hold until it returns. This is important
 * to note because in an error case we might stay in this code
 * for an extended amount of time. So far, we think this is the
 * best solution, but real-world experience might tell us a
 * different truth ;)
 */
void OnReceive(srAPIObj __attribute__((unused)) *pMyAPI, srSLMGObj* pSLMG)
{
	uchar *pszRawMsg;
	uchar *fromHost = (uchar*) "[unset]"; /* TODO: get hostname */
	uchar *fromHostIP = (uchar*) "[unset]"; /* TODO: get hostname */

	srSLMGGetRawMSG(pSLMG, &pszRawMsg);

	parseAndSubmitMessage(fromHost, fromHostIP, pszRawMsg, strlen((char*)pszRawMsg),
		MSG_PARSE_HOSTNAME, NOFLAG, eFLOWCTL_FULL_DELAY, (uchar*)"im3195");
}


BEGINrunInput
CODESTARTrunInput
	/* this is an endless loop - it is terminated when the thread is
	 * signalled to do so. This, however, is handled by the framework,
	 * right into the sleep below.
	 */
	while(!pThrd->bShallStop) {
		/* now move the listener to running state. Control will only
		 * return after SIGUSR1.
		 */
		if((iRet = srAPIRunListener(pAPI)) != SR_RET_OK) {
			errmsg.LogError(0, NO_ERRCODE, "error %d running liblogging listener - im3195 is defunct", iRet);
			FINALIZE; /* this causes im3195 to become defunct; TODO: recovery handling */
		}
	}
finalize_it:
ENDrunInput


BEGINwillRun
CODESTARTwillRun
	if((pAPI = srAPIInitLib()) == NULL) {
		errmsg.LogError(0, NO_ERRCODE, "error initializing liblogging - im3195 is defunct");
		ABORT_FINALIZE(RS_RET_ERR);
	}

	if((iRet = srAPISetOption(pAPI, srOPTION_BEEP_LISTENPORT, listenPort)) != SR_RET_OK) {
		errmsg.LogError(0, NO_ERRCODE, "error %d setting liblogging listen port - im3195 is defunct", iRet);
		FINALIZE;
	}

	if((iRet = srAPISetupListener(pAPI, OnReceive)) != SR_RET_OK) {
		errmsg.LogError(0, NO_ERRCODE, "error %d setting up liblogging listener - im3195 is defunct", iRet);
		FINALIZE;
	}

finalize_it:
ENDwillRun


BEGINafterRun
CODESTARTafterRun
	dbgprintf("Shutting down rfc3195d. Be patient, this can take up to 30 seconds...\n");
	srAPIShutdownListener(pAPI);
ENDafterRun


BEGINmodExit
CODESTARTmodExit
	srAPIExitLib(pAPI); /* terminate liblogging */
	/* release objects we used */
	objRelease(errmsg, CORE_COMPONENT);
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_IMOD_QUERIES
ENDqueryEtryPt

static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	listenPort = 601;
	return RS_RET_OK;
}


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(errmsg, CORE_COMPONENT));

	CHKiRet(omsdRegCFSLineHdlr((uchar *)"input3195listenport", 0, eCmdHdlrInt, NULL, &listenPort, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit
/* vim:set ai:
 */
