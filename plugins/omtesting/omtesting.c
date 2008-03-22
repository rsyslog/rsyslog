/* omtesting.c
 *
 * This module is a testing aid. It is not meant to be used in production. I have
 * initially written it to introduce delays of custom length to action processing.
 * This is needed for development of new message queueing methods. However, I think
 * there are other uses for this module. For example, I can envision that it is a good
 * thing to have an output module that requests a retry on every "n"th invocation
 * and such things. I implement only what I need. But should further testing needs
 * arise, it makes much sense to add them here.
 *
 * This module will become part of the CVS and the rsyslog project because I think
 * it is a generally useful debugging, testing and development aid for everyone
 * involved with rsyslog.
 *
 * CURRENT SUPPORTED COMMANDS:
 *
 * :omtesting:sleep <seconds> <milliseconds>
 *
 * Must be specified exactly as above. Keep in mind milliseconds are a millionth
 * of a second!
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
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
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "syslogd.h"
#include "syslogd-types.h"
#include "module-template.h"

MODULE_TYPE_OUTPUT

/* internal structures
 */
DEF_OMOD_STATIC_DATA

typedef struct _instanceData {
	int	iWaitSeconds;
	int	iWaitUSeconds;	/* milli-seconds (one million of a second, just to make sure...) */
} instanceData;

BEGINcreateInstance
CODESTARTcreateInstance
	pData->iWaitSeconds = 1;
	pData->iWaitUSeconds = 0;
ENDcreateInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	dbgprintf("Action delays rule by %d second(s) and %d millisecond(s)\n",
		  pData->iWaitSeconds, pData->iWaitUSeconds);
	/* do nothing */
ENDdbgPrintInstInfo


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	/* we are not compatible with repeated msg reduction feature, so do not allow it */
ENDisCompatibleWithFeature


BEGINtryResume
CODESTARTtryResume
ENDtryResume

BEGINdoAction
CODESTARTdoAction
	struct timeval tvSelectTimeout;

	dbgprintf("sleep(%d, %d)\n", pData->iWaitSeconds, pData->iWaitUSeconds);
	tvSelectTimeout.tv_sec = pData->iWaitSeconds;
	tvSelectTimeout.tv_usec = pData->iWaitUSeconds; /* milli seconds */
	select(0, NULL, NULL, NULL, &tvSelectTimeout);
	//dbgprintf(":omtesting: end doAction(), iRet %d\n", iRet);
ENDdoAction


BEGINfreeInstance
CODESTARTfreeInstance
	/* we do not have instance data, so we do not need to
	 * do anything here. -- rgerhards, 2007-07-25
	 */
ENDfreeInstance


BEGINparseSelectorAct
	int i;
	uchar szBuf[1024];
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(0)
	/* code here is quick and dirty - if you like, clean it up. But keep
	 * in mind it is just a testing aid ;) -- rgerhards, 2007-12-31
	 */
	if(!strncmp((char*) p, ":omtesting:", sizeof(":omtesting:") - 1)) {
		p += sizeof(":omtesting:") - 1; /* eat indicator sequence (-1 because of '\0'!) */
	} else {
		ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
	}

	/* ok, if we reach this point, we have something for us */
	if((iRet = createInstance(&pData)) != RS_RET_OK)
		goto finalize_it;
	
	/* check mode */
	for(i = 0 ; *p && !isspace((char) *p) && ((unsigned) i < sizeof(szBuf) - 1) ; ++i) {
		szBuf[i] = (uchar) *p++;
	}
	szBuf[i] = '\0';
	if(isspace(*p))
		++p;

	if(!strcmp((char*) szBuf, "sleep")) {
		/* parse seconds */
		for(i = 0 ; *p && !isspace(*p) && ((unsigned) i < sizeof(szBuf) - 1) ; ++i) {
			szBuf[i] = *p++;
		}
		szBuf[i] = '\0';
		if(isspace(*p))
			++p;
		pData->iWaitSeconds = atoi((char*) szBuf);
		/* parse milliseconds */
		for(i = 0 ; *p && !isspace(*p) && ((unsigned) i < sizeof(szBuf) - 1) ; ++i) {
			szBuf[i] = *p++;
		}
		szBuf[i] = '\0';
		if(isspace(*p))
			++p;
		pData->iWaitUSeconds = atoi((char*) szBuf);
	}
	/* once there are other modes, here is the spot to add it! */
	else {
		dbgprintf("invalid mode '%s', doing 'sleep 1 0' - fix your config\n", szBuf);
	}

CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINmodExit
CODESTARTmodExit
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
ENDmodInit
/*
 * vi:set ai:
 */
