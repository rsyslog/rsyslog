/* omstdout.c
 * send all output to stdout - this is primarily a test driver (but may
 * be used for weired use cases). Not tested for robustness!
 *
 * NOTE: read comments in module-template.h for more specifics!
 *
 * File begun on 2009-03-19 by RGerhards
 *
 * Copyright 2009 Rainer Gerhards and Adiscon GmbH.
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
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include "cfsysline.h"

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP

/* internal structures
 */
DEF_OMOD_STATIC_DATA

/* config variables */
static int bUseArrayInterface = 0;	/* shall action use array instead of string template interface? */
static int bEnsureLFEnding = 1;		/* shall action use array instead of string template interface? */


typedef struct _instanceData {
	int bUseArrayInterface;		/* uses action use array instead of string template interface? */
	int bEnsureLFEnding;		/* ensure that a linefeed is written at the end of EACH record (test aid for nettester) */
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
ENDdbgPrintInstInfo


BEGINtryResume
CODESTARTtryResume
ENDtryResume

BEGINdoAction
	char **szParams;
	char *toWrite;
	int iParamVal;
	int iParam;
	int iBuf;
	char szBuf[65564];
	size_t len;
CODESTARTdoAction
	if(pData->bUseArrayInterface) {
		/* if we use array passing, we need to put together a string
		 * ourselves. At this point, please keep in mind that omstdout is
		 * primarily a testing aid. Other modules may do different processing
		 * if they would like to support downlevel versions which do not support
		 * array-passing, but also use that interface on cores who do...
		 * So this code here is also more or less an example of how to do that.
		 * rgerhards, 2009-04-03
		 */
		szParams = (char**)(void*) (ppString[0]);
		/* In array-passing mode, ppString[] contains a NULL-terminated array
		 * of char *pointers.
		 */
		iParam = 0;
		iBuf = 0;
		while(szParams[iParam] != NULL) {
			if(iParam > 0)
				szBuf[iBuf++] = ','; /* all but first need a delimiter */
			iParamVal = 0;
			while(szParams[iParam][iParamVal] != '\0' && iBuf < (int) sizeof(szBuf)) {
				szBuf[iBuf++] = szParams[iParam][iParamVal++];
			}
			++iParam;
		}
		szBuf[iBuf] = '\0';
		toWrite = szBuf;
	} else {
		toWrite = (char*) ppString[0];
	}
	len = strlen(toWrite);
	write(1, toWrite, len); /* 1 is stdout! */
	if(pData->bEnsureLFEnding && toWrite[len-1] != '\n') {
		write(1, "\n", 1); /* write missing LF */
	}
ENDdoAction


BEGINparseSelectorAct
	int iTplOpts;
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	/* first check if this config line is actually for us */
	if(strncmp((char*) p, ":omstdout:", sizeof(":omstdout:") - 1)) {
		ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
	}

	/* ok, if we reach this point, we have something for us */
	p += sizeof(":omstdout:") - 1; /* eat indicator sequence  (-1 because of '\0'!) */
	CHKiRet(createInstance(&pData));

	/* check if a non-standard template is to be applied */
	if(*(p-1) == ';')
		--p;
	iTplOpts = (bUseArrayInterface == 0) ? 0 : OMSR_TPL_AS_ARRAY;
	CHKiRet(cflineParseTemplateName(&p, *ppOMSR, 0, iTplOpts, (uchar*) "RSYSLOG_FileFormat"));
	pData->bUseArrayInterface = bUseArrayInterface;
	pData->bEnsureLFEnding = bEnsureLFEnding;
CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINmodExit
CODESTARTmodExit
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
ENDqueryEtryPt



/* Reset config variables for this module to default values.
 */
static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	DEFiRet;
	bUseArrayInterface = 0;
	bEnsureLFEnding = 1;
	RETiRet;
}


BEGINmodInit()
	rsRetVal localRet;
	rsRetVal (*pomsrGetSupportedTplOpts)(unsigned long *pOpts);
	unsigned long opts;
	int bArrayPassingSupported;		/* does core support template passing as an array? */
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	/* check if the rsyslog core supports parameter passing code */
	bArrayPassingSupported = 0;
	localRet = pHostQueryEtryPt((uchar*)"OMSRgetSupportedTplOpts", &pomsrGetSupportedTplOpts);
	if(localRet == RS_RET_OK) {
		/* found entry point, so let's see if core supports array passing */
		CHKiRet((*pomsrGetSupportedTplOpts)(&opts));
		if(opts & OMSR_TPL_AS_ARRAY)
			bArrayPassingSupported = 1;
	} else if(localRet != RS_RET_ENTRY_POINT_NOT_FOUND) {
		ABORT_FINALIZE(localRet); /* Something else went wrong, what is not acceptable */
	}
	DBGPRINTF("omstdout: array-passing is %ssupported by rsyslog core.\n", bArrayPassingSupported ? "" : "not ");

	if(bArrayPassingSupported) {
		/* enable config comand only if core supports it */
		CHKiRet(omsdRegCFSLineHdlr((uchar *)"actionomstdoutarrayinterface", 0, eCmdHdlrBinary, NULL,
			                   &bUseArrayInterface, STD_LOADABLE_MODULE_ID));
	}
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"actionomstdoutensurelfending", 0, eCmdHdlrBinary, NULL,
				   &bEnsureLFEnding, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler,
				    resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit

/* vi:set ai:
 */
