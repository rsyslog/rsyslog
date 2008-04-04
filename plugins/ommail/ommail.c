/* ommail.c
 *
 * This is an implementation of a mail sending output module.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * File begun on 2008-04-04 by RGerhards
 *
 * Copyright 2008 Rainer Gerhards and Adiscon GmbH.
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
#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <librelp.h>
#include "syslogd.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "cfsysline.h"
#include "module-template.h"
#include "errmsg.h"

MODULE_TYPE_OUTPUT

/* internal structures
 */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(errmsg)

static uchar *pSrv;
static uchar *pszFrom;
static uchar *pszTo;

typedef struct _instanceData {
	int iMode;	/* 0 - smtp, 1 - sendmail */
	union {
		struct {
			uchar *pszSrv;
			uchar *pszSrvPort;
			uchar *pszFrom;
			uchar *pszTo;
			uchar *pszSubject;
			int sock;	/* socket to this server (most important when we do multiple msgs per mail) */
			} smtp;
	} md;	/* mode-specific data */
} instanceData;


BEGINcreateInstance
CODESTARTcreateInstance
ENDcreateInstance


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURERepeatedMsgReduction)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINfreeInstance
CODESTARTfreeInstance
	if(pData->iMode == 0) {
		if(pData->md.smtp.pszSrv != NULL)
			free(pData->md.smtp.pszSrv);
		if(pData->md.smtp.pszSrvPort != NULL)
			free(pData->md.smtp.pszSrvPort);
		if(pData->md.smtp.pszFrom != NULL)
			free(pData->md.smtp.pszFrom);
		if(pData->md.smtp.pszTo != NULL)
			free(pData->md.smtp.pszTo);
		if(pData->md.smtp.pszSubject != NULL)
			free(pData->md.smtp.pszSubject);
	}
ENDfreeInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	printf("mail"); /* TODO: extend! */
ENDdbgPrintInstInfo


/* connect to server
 * rgerhards, 2008-04-04
 */
static rsRetVal doConnect(instanceData *pData)
{
	DEFiRet;

	RETiRet;
}


BEGINtryResume
CODESTARTtryResume
	iRet = doConnect(pData);
ENDtryResume


BEGINdoAction
CODESTARTdoAction
	dbgprintf(" Mail\n");

//	if(!pData->bIsConnected) {
//		CHKiRet(doConnect(pData));
//	}

	/* forward */
	iRet = sendSMTP(pData, ppString[0]);
	if(iRet != RELP_RET_OK) {
		/* error! */
		dbgprintf("error sending mail, suspending\n");
		iRet = RS_RET_SUSPENDED;
	}

finalize_it:
ENDdoAction


BEGINparseSelectorAct
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	if(!strncmp((char*) p, ":ommail:", sizeof(":ommail:") - 1)) {
		p += sizeof(":ommail:") - 1; /* eat indicator sequence (-1 because of '\0'!) */
	} else {
		ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
	}

	/* ok, if we reach this point, we have something for us */
	if((iRet = createInstance(&pData)) != RS_RET_OK)
		FINALIZE;

	/* TODO: do we need to call freeInstance if we failed - this is a general question for
	 * all output modules. I'll address it later as the interface evolves. rgerhards, 2007-07-25
	 */
CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINmodExit
CODESTARTmodExit
	/* release what we no longer need */
	objRelease(errmsg, CORE_COMPONENT);
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	/* tell which objects we need */
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
ENDmodInit

/* vim:set ai:
 */
