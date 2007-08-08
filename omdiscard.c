/* omdiscard.c
 * This is the implementation of the built-in discard output module.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * File begun on 2007-07-24 by RGerhards
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
#include "omdiscard.h"
#include "module-template.h"

/* internal structures
 */
DEF_OMOD_STATIC_DATA

typedef struct _instanceData {
} instanceData;

/* we do not need a createInstance()!
BEGINcreateInstance
CODESTARTcreateInstance
ENDcreateInstance
*/


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
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
	iRet = RS_RET_DISCARDMSG;
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
	pData = NULL; /* this action does not have any instance data */
	p = *pp;

	if(*p == '~') {
		/* TODO: check the rest of the selector line - error reporting */
		dbgprintf("discard\n");
	} else {
		iRet = RS_RET_CONFLINE_UNPROCESSED;
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


BEGINmodInit(Discard)
CODESTARTmodInit
	*ipIFVersProvided = 1; /* so far, we only support the initial definition */
CODEmodInit_QueryRegCFSLineHdlr
ENDmodInit
/*
 * vi:set ai:
 */
