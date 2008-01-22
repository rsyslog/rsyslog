/* omsnmp.c
 *
 * This module sends an snmp trap. More text will come here soon ^^
 *
 * This module will become part of the CVS and the rsyslog project because I think
 * it is a generally useful debugging, testing and development aid for everyone
 * involved with rsyslog.
 *
 * CURRENT SUPPORTED COMMANDS:
 *
 * :omsnmp:sleep <seconds> <milliseconds>
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
#include "cfsysline.h"
#include "module-template.h"

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

MODULE_TYPE_OUTPUT

/* internal structures
 */
DEF_OMOD_STATIC_DATA

static uchar* pszTarget = NULL;

typedef struct _instanceData {
	uchar	szTarget[MAXHOSTNAMELEN+1];	/* IP or hostname of Snmp Target*/ 

} instanceData;

BEGINcreateInstance
CODESTARTcreateInstance
//	pData->szTarget
ENDcreateInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	dbgprintf("%s", pData->szTarget);
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
	dbgprintf("Sending SNMP Trap to '%s'\n", pData->szTarget);
ENDdoAction


BEGINfreeInstance
CODESTARTfreeInstance
	/* we do not have instance data, so we do not need to
	 * do anything here. -- rgerhards, 2007-07-25
	 */
ENDfreeInstance


BEGINparseSelectorAct
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(0)

	/* code here is quick and dirty - if you like, clean it up. But keep
	 * in mind it is just a testing aid ;) -- rgerhards, 2007-12-31
	 */
	if(!strncmp((char*) p, ":omsnmp:", sizeof(":omsnmp:") - 1)) {
		p += sizeof(":omsnmp:") - 1; /* eat indicator sequence (-1 because of '\0'!) */
	} else {
		ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
	}
dbgprintf("0\n");	

	/* ok, if we reach this point, we have something for us */
	if((iRet = createInstance(&pData)) != RS_RET_OK)
		goto finalize_it;

dbgprintf("1\n");	

		ABORT_FINALIZE( RS_RET_CONFLINE_UNPROCESSED );// RS_RET_PARAM_ERROR );
dbgprintf("2\n");	

	/* Failsave */
	if (pszTarget == NULL)
		ABORT_FINALIZE( RS_RET_CONFLINE_UNPROCESSED );// RS_RET_PARAM_ERROR );

	/* Copy Target */
	strncpy( (char*) pData->szTarget, (char*) pszTarget, sizeof( pData->szTarget -1 ) );

CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINneedUDPSocket
CODESTARTneedUDPSocket
ENDneedUDPSocket

/* Reset config variables for this module to default values.
 */
static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	if (pszTarget != NULL)
		free(pszTarget);
	pszTarget = NULL;

	return RS_RET_OK;
}


BEGINmodExit
CODESTARTmodExit
if (pszTarget != NULL)
	free(pszTarget);	
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = 1; /* so far, we only support the initial definition */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"actionsnmptarget", 0, eCmdHdlrGetWord, NULL, &pszTarget, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit
/*
 * vi:set ai:
 */
