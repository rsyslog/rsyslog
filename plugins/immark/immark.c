/* immark.c
 * This is the implementation of the build-in mark message input module.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * File begun on 2007-07-20 by RGerhards (extracted from syslogd.c)
 * This file is under development and has not yet arrived at being fully
 * self-contained and a real object. So far, it is mostly an excerpt
 * of the "old" message code without any modifications. However, it
 * helps to have things at the right place one we go to the meat of it.
 *
 * Copyright 2007 Rainer Gerhards and Adiscon GmbH.
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
#include "dirty.h"
#include "cfsysline.h"
#include "module-template.h"
#include "errmsg.h"
#include "msg.h"

MODULE_TYPE_INPUT

/* defines */
#define DEFAULT_MARK_PERIOD (20 * 60)

/* Module static data */
DEF_IMOD_STATIC_DATA
static int iMarkMessagePeriod = DEFAULT_MARK_PERIOD;

/* This function is called to gather input. It must terminate only
 * a) on failure (iRet set accordingly)
 * b) on termination of the input module (as part of the unload process)
 * Code begun 2007-12-12 rgerhards
 *  
 * This code must simply spawn emit a mark message at each mark interval.
 * We are running on our own thread, so this is extremely easy: we just
 * sleep MarkInterval seconds and each time we awake, we inject the message.
 * Please note that we do not do the other fancy things that sysklogd
 * (and pre 1.20.2 releases of rsyslog) did in mark procesing. They simply
 * do not belong here.
 */
BEGINrunInput
CODESTARTrunInput
	/* this is an endless loop - it is terminated when the thread is
	 * signalled to do so. This, however, is handled by the framework,
	 * right into the sleep below.
	 */
	while(1) {
		/* we do not need to handle the RS_RET_TERMINATE_NOW case any
	   	 * special because we just need to terminate. This may be different
	   	 * if a cleanup is needed. But for now, we can just use CHKiRet().
	   	 * rgerhards, 2007-12-17
	   	 */
		CHKiRet(thrdSleep(pThrd, iMarkMessagePeriod, 0)); /* seconds, micro seconds */
		logmsgInternal(NO_ERRCODE, LOG_INFO, (uchar*)"-- MARK --", MARK);
	}
finalize_it:
	return iRet;
ENDrunInput


BEGINwillRun
CODESTARTwillRun
	/* We set the global MarkInterval to what is configured here -- rgerhards, 2008-07-15 */
	MarkInterval = iMarkMessagePeriod;
	if(iMarkMessagePeriod == 0)
		iRet = RS_RET_NO_RUN;
ENDwillRun


BEGINafterRun
CODESTARTafterRun
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
	iMarkMessagePeriod = DEFAULT_MARK_PERIOD;

	return RS_RET_OK;
}

BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"markmessageperiod", 0, eCmdHdlrInt, NULL, &iMarkMessagePeriod, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit
/*
 * vi:set ai:
 */
