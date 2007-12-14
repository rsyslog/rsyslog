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
#if 1 /* IMMARK */
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include "rsyslog.h"
#include "syslogd.h"
#include "module-template.h"

MODULE_TYPE_INPUT

typedef struct _instanceData {
} instanceData;

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
rsRetVal
immark_runInput(void)
{
	struct timeval tvSelectTimeout;
	sigset_t sigSet;
	sigfillset(&sigSet);
	pthread_sigmask(SIG_BLOCK, &sigSet, NULL);
	sigemptyset(&sigSet);
	sigaddset(&sigSet, SIGUSR2);
	pthread_sigmask(SIG_UNBLOCK, &sigSet, NULL);
	while(!bFinished) {
dbgprintf("immark pre select\n");
		tvSelectTimeout.tv_sec = 5;
		tvSelectTimeout.tv_usec = 0;
		select(0, NULL, NULL, NULL, &tvSelectTimeout);
		if(bFinished)
			break;
dbgprintf("immark post select, doing mark, bFinished: %d\n", bFinished);
		logmsgInternal(LOG_INFO, "-- MARK --", ADDDATE);
		//logmsgInternal(LOG_INFO, "-- MARK --", ADDDATE|MARK);
	}
fprintf(stderr, "immark: finished!\n");
	return RS_RET_OK;
}

BEGINfreeInstance
CODESTARTfreeInstance
ENDfreeInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
ENDdbgPrintInstInfo


BEGINmodExit
CODESTARTmodExit
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_IMOD_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = 1; /* so far, we only support the initial definition */
ENDmodInit
#endif /* #if 0 */
/*
 * vi:set ai:
 */
