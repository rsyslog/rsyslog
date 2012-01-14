/* ompipe.c
 * This is the implementation of the build-in pipe output module.
 * Note that this module stems back to the "old" (4.4.2 and below)
 * omfile. There were some issues with the new omfile code and pipes
 * (namely in regard to xconsole), so we took out the pipe code and moved
 * that to a separate module. That a) immediately solves the issue for a
 * less common use case and probably makes it much easier to enhance
 * file and pipe support (now independently) in the future (we always
 * needed to think about pipes in omfile so far, what we now no longer
 * need to, hopefully resulting in reduction of complexity).
 *
 * NOTE: read comments in module-template.h to understand how this pipe
 *       works!
 *
 * Copyright 2007-2012 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *       http://www.apache.org/licenses/LICENSE-2.0
 *       -or-
 *       see COPYING.ASL20 in the source distribution
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "config.h"
#include "rsyslog.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>

#include "syslogd.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "ompipe.h"
#include "omfile.h" /* for dirty trick: access to $ActionFileDefaultTemplate value */
#include "cfsysline.h"
#include "module-template.h"
#include "conf.h"
#include "errmsg.h"

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP

/* internal structures
 */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(errmsg)


/* globals for default values */
/* end globals for default values */


typedef struct _instanceData {
	uchar	f_fname[MAXFNAME];/* pipe or template name (display only) */
	short	fd;		  /* pipe descriptor for (current) pipe */
} instanceData;

typedef struct configSettings_s {
	EMPTY_STRUCT
} configSettings_t;

SCOPING_SUPPORT; /* must be set AFTER configSettings_t is defined */

BEGINinitConfVars		/* (re)set config variables to default values */
CODESTARTinitConfVars 
ENDinitConfVars


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURERepeatedMsgReduction)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	dbgprintf("pipe %s", pData->f_fname);
	if (pData->fd == -1)
		dbgprintf(" (unused)");
ENDdbgPrintInstInfo


/* This is now shared code for all types of files. It simply prepares
 * pipe access, which, among others, means the the pipe wil be opened
 * and any directories in between will be created (based on config, of
 * course). -- rgerhards, 2008-10-22
 * changed to iRet interface - 2009-03-19
 */
static inline rsRetVal
preparePipe(instanceData *pData)
{
	DEFiRet;
	pData->fd = open((char*) pData->f_fname, O_RDWR|O_NONBLOCK|O_CLOEXEC);
	RETiRet;
}


/* rgerhards 2004-11-11: write to a pipe output. This
 * will be called for all outputs using pipe semantics,
 * for example also for pipes.
 */
static rsRetVal writePipe(uchar **ppString, instanceData *pData)
{
	int iLenWritten;
	DEFiRet;

	ASSERT(pData != NULL);

	if(pData->fd == -1) {
		rsRetVal iRetLocal;
		iRetLocal = preparePipe(pData);
		if((iRetLocal != RS_RET_OK) || (pData->fd == -1))
			ABORT_FINALIZE(RS_RET_SUSPENDED); /* whatever the failure was, we need to retry */
	}

	/* create the message based on format specified */
	iLenWritten = write(pData->fd, ppString[0], strlen((char*)ppString[0]));
	if(iLenWritten < 0) {
		int e = errno;
		char errStr[1024];
		rs_strerror_r(errno, errStr, sizeof(errStr));
		DBGPRINTF("pipe (%d) write error %d: %s\n", pData->fd, e, errStr);

		/* If a named pipe is full, we suspend this action for a while */
		if(e == EAGAIN)
			ABORT_FINALIZE(RS_RET_SUSPENDED);

		close(pData->fd);
		pData->fd = -1; /* tell that fd is no longer open! */
		iRet = RS_RET_SUSPENDED;
		errno = e;
		errmsg.LogError(0, NO_ERRCODE, "%s", pData->f_fname);
	}

finalize_it:
	RETiRet;
}


BEGINcreateInstance
CODESTARTcreateInstance
	pData->fd = -1;
ENDcreateInstance


BEGINfreeInstance
CODESTARTfreeInstance
	if(pData->fd != -1)
		close(pData->fd);
ENDfreeInstance


BEGINtryResume
CODESTARTtryResume
ENDtryResume

BEGINdoAction
CODESTARTdoAction
	DBGPRINTF(" (%s)\n", pData->f_fname);
	iRet = writePipe(ppString, pData);
ENDdoAction


BEGINparseSelectorAct
CODESTARTparseSelectorAct
	/* yes, the if below is redundant, but I need it now. Will go away as
	 * the code further changes.  -- rgerhards, 2007-07-25
	 */
	if(*p == '|') {
		if((iRet = createInstance(&pData)) != RS_RET_OK) {
			ENDfunc
			return iRet; /* this can not use RET_iRet! */
		}
	} else {
		/* this is not clean, but we need it for the time being
		 * TODO: remove when cleaning up modularization 
		 */
		ENDfunc
		return RS_RET_CONFLINE_UNPROCESSED;
	}

	CODE_STD_STRING_REQUESTparseSelectorAct(1)
	++p;
	/* rgerhards 2004-11-17: from now, we need to have different
	 * processing, because after the first comma, the template name
	 * to use is specified. So we need to scan for the first coma first
	 * and then look at the rest of the line.
	 */
	CHKiRet(cflineParseFileName(p, (uchar*) pData->f_fname, *ppOMSR, 0, OMSR_NO_RQD_TPL_OPTS,
				       (pszFileDfltTplName == NULL) ? (uchar*)"RSYSLOG_FileFormat" : pszFileDfltTplName));

	/* at this stage, we ignore the return value of preparePipe, this is taken
	 * care of in later steps. -- rgerhards, 2009-03-19
	 */
	preparePipe(pData);
		
	if(pData->fd < 0 ) {
		pData->fd = -1;
		DBGPRINTF("Error opening log pipe: %s\n", pData->f_fname);
		errmsg.LogError(0, RS_RET_NO_FILE_ACCESS, "Could not open output pipe '%s'", pData->f_fname);
	}
CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINdoHUP
CODESTARTdoHUP
	if(pData->fd != -1) {
		close(pData->fd);
		pData->fd = -1;
	}
ENDdoHUP


BEGINmodExit
CODESTARTmodExit
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
CODEqueryEtryPt_doHUP
ENDqueryEtryPt


BEGINmodInit(Pipe)
CODESTARTmodInit
SCOPINGmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
ENDmodInit
/* vi:set ai:
 */
