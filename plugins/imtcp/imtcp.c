/* imtcp.c
 * This is the implementation of the TCP input module.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * File begun on 2007-12-21 by RGerhards (extracted from syslogd.c)
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
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/un.h>
#include "syslogd.h"
#include "cfsysline.h"
#include "module-template.h"

MODULE_TYPE_INPUT
TERM_SYNC_TYPE(eTermSync_NONE)

/* defines */

/* Module static data */
DEF_IMOD_STATIC_DATA

typedef struct _instanceData {
} instanceData;

/* config settings */


/* This function is called to gather input.
 */
BEGINrunInput
	int maxfds;
	int nfds;
	int i;
	int fd;
	fd_set readfds;
CODESTARTrunInput
	/* this is an endless loop - it is terminated when the thread is
	 * signalled to do so. This, however, is handled by the framework,
	 * right into the sleep below.
	 */
	while(1) {
		/* Add the Unix Domain Sockets to the list of read
		 * descriptors.
		 * rgerhards 2005-08-01: we must now check if there are
		 * any local sockets to listen to at all. If the -o option
		 * is given without -a, we do not need to listen at all..
		 */
	        maxfds = 0;
	        FD_ZERO (&readfds);

		if(Debug) {
			dbgprintf("--------imTCP calling select, active file descriptors (max %d): ", maxfds);
			for (nfds= 0; nfds <= maxfds; ++nfds)
				if ( FD_ISSET(nfds, &readfds) )
					dbgprintf("%d ", nfds);
			dbgprintf("\n");
		}

		/* wait for io to become ready */
		/* select here */

	}

	return iRet;
ENDrunInput


/* initialize and return if will run or not */
BEGINwillRun
CODESTARTwillRun
	/* first apply some config settings */
ENDwillRun


BEGINafterRun
CODESTARTafterRun
	/* do cleanup here */
ENDafterRun


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

static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	//bOmitLocalLogging = 0;
	return RS_RET_OK;
}


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = 1; /* so far, we only support the initial definition */
CODEmodInit_QueryRegCFSLineHdlr
	/* register config file handlers */
	//CHKiRet(omsdRegCFSLineHdlr((uchar *)"omitlocallogging", 0, eCmdHdlrBinary,
	//	NULL, &bOmitLocalLogging, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler,
		resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit
/*
 * vi:set ai:
 */
