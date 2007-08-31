/* omshell.c
 * This is the implementation of the build-in shell output module.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * shell support was initially written by bkalkbrenner 2005-09-20
 *
 * File begun on 2007-07-20 by RGerhards (extracted from syslogd.c)
 * This file is under development and has not yet arrived at being fully
 * self-contained and a real object. So far, it is mostly an excerpt
 * of the "old" message code without any modifications. However, it
 * helps to have things at the right place one we go to the meat of it.
 *
 * Copyright 2007 Rainer Gerhards and Adiscon GmbH.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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
#include "syslogd.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "omshell.h"
#include "module-template.h"

/* internal structures
 */
DEF_OMOD_STATIC_DATA

typedef struct _instanceData {
	uchar	progName[MAXFNAME]; /* program  to execute */
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
ENDfreeInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	printf("%s", pData->progName);
ENDdbgPrintInstInfo


BEGINtryResume
CODESTARTtryResume
ENDtryResume

BEGINdoAction
CODESTARTdoAction
	/* TODO: using pData->progName is not clean from the point of
	 * modularization. We'll change that as we go ahead with modularization.
	 * rgerhards, 2007-07-20
	 */
	dbgprintf("\n");
	if(execProg((uchar*) pData->progName, 1, ppString[0]) == 0)
	 	logerrorSz("Executing program '%s' failed", (char*)pData->progName);
ENDdoAction


BEGINparseSelectorAct
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	/* yes, the if below is redundant, but I need it now. Will go away as
	 * the code further changes.  -- rgerhards, 2007-07-25
	 */
	if(*p == '^') {
		if((iRet = createInstance(&pData)) != RS_RET_OK)
			goto finalize_it;
	}


	switch (*p)
	{
	case '^': /* bkalkbrenner 2005-09-20: execute shell command */
		dbgprintf("exec\n");
		++p;
		iRet = cflineParseFileName(p, (uchar*) pData->progName, *ppOMSR, 0, OMSR_NO_RQD_TPL_OPTS);
		break;
	default:
		iRet = RS_RET_CONFLINE_UNPROCESSED;
		break;
	}

CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINneedUDPSocket
CODESTARTneedUDPSocket
ENDneedUDPSocket


BEGINonSelectReadyWrite
CODESTARTonSelectReadyWrite
ENDonSelectReadyWrite


BEGINgetWriteFDForSelect
CODESTARTgetWriteFDForSelect
ENDgetWriteFDForSelect


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
ENDqueryEtryPt


BEGINmodInit(Shell)
CODESTARTmodInit
	*ipIFVersProvided = 1; /* so far, we only support the initial definition */
CODEmodInit_QueryRegCFSLineHdlr
ENDmodInit

/*
 * vi:set ai:
 */
