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
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "dirty.h"
#include "syslogd-types.h"
#include "module-template.h"
#include "conf.h"
#include "cfsysline.h"

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("omtesting")

/* internal structures
 */
DEF_OMOD_STATIC_DATA


typedef struct _instanceData {
	enum { MD_SLEEP, MD_FAIL, MD_RANDFAIL, MD_ALWAYS_SUSPEND }
		mode;
	int	bEchoStdout;
	int	iWaitSeconds;
	int	iWaitUSeconds;	/* milli-seconds (one million of a second, just to make sure...) */
	int 	iCurrCallNbr;
	int	iFailFrequency;
	int	iResumeAfter;
	int	iCurrRetries;
} instanceData;

typedef struct configSettings_s {
	int bEchoStdout;	/* echo non-failed messages to stdout */
} configSettings_t;
static configSettings_t cs;

BEGINinitConfVars		/* (re)set config variables to default values */
CODESTARTinitConfVars 
	cs.bEchoStdout = 0;
ENDinitConfVars

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


/* implement "fail" command in retry processing */
static rsRetVal doFailOnResume(instanceData *pData)
{
	DEFiRet;

	dbgprintf("fail retry curr %d, max %d\n", pData->iCurrRetries, pData->iResumeAfter);
	if(++pData->iCurrRetries == pData->iResumeAfter) {
		iRet = RS_RET_OK;
	} else {
		iRet = RS_RET_SUSPENDED;
	}

	RETiRet;
}


/* implement "fail" command */
static rsRetVal doFail(instanceData *pData)
{
	DEFiRet;

	dbgprintf("fail curr %d, frquency %d\n", pData->iCurrCallNbr, pData->iFailFrequency);
	if(pData->iCurrCallNbr++ % pData->iFailFrequency == 0) {
		pData->iCurrRetries = 0;
		iRet = RS_RET_SUSPENDED;
	}

	RETiRet;
}


/* implement "sleep" command */
static rsRetVal doSleep(instanceData *pData)
{
	DEFiRet;
	struct timeval tvSelectTimeout;

	dbgprintf("sleep(%d, %d)\n", pData->iWaitSeconds, pData->iWaitUSeconds);
	tvSelectTimeout.tv_sec = pData->iWaitSeconds;
	tvSelectTimeout.tv_usec = pData->iWaitUSeconds; /* milli seconds */
	select(0, NULL, NULL, NULL, &tvSelectTimeout);
	RETiRet;
}


/* implement "randomfail" command */
static rsRetVal doRandFail(void)
{
	DEFiRet;
	if((rand() >> 4) < (RAND_MAX >> 5)) { /* rougly same probability */
		iRet = RS_RET_OK;
		dbgprintf("omtesting randfail: succeeded this time\n");
	} else {
		iRet = RS_RET_SUSPENDED;
		dbgprintf("omtesting randfail: failed this time\n");
	}
	RETiRet;
}


BEGINtryResume
CODESTARTtryResume
	dbgprintf("omtesting tryResume() called\n");
	switch(pData->mode) {
		case MD_SLEEP:
			break;
		case MD_FAIL:
			iRet = doFailOnResume(pData);
			break;
		case MD_RANDFAIL:
			iRet = doRandFail();
			break;
		case MD_ALWAYS_SUSPEND:
			iRet = RS_RET_SUSPENDED;
	}
	dbgprintf("omtesting tryResume() returns iRet %d\n", iRet);
ENDtryResume


BEGINdoAction
CODESTARTdoAction
	dbgprintf("omtesting received msg '%s'\n", ppString[0]);
	switch(pData->mode) {
		case MD_SLEEP:
			iRet = doSleep(pData);
			break;
		case MD_FAIL:
			iRet = doFail(pData);
			break;
		case MD_RANDFAIL:
			iRet = doRandFail();
			break;
		case MD_ALWAYS_SUSPEND:
			iRet = RS_RET_SUSPENDED;
			break;
	}

	if(iRet == RS_RET_OK && pData->bEchoStdout) {
		fprintf(stdout, "%s", ppString[0]);
		fflush(stdout);
	}
	dbgprintf(":omtesting: end doAction(), iRet %d\n", iRet);
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
CODE_STD_STRING_REQUESTparseSelectorAct(1)
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

	dbgprintf("omtesting command: '%s'\n", szBuf);
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
		pData->mode = MD_SLEEP;
	} else if(!strcmp((char*) szBuf, "fail")) {
		/* "fail fail-freqency resume-after"
		 * fail-frequency specifies how often doAction() fails
		 * resume-after speicifes how fast tryResume() should come back with success
		 * all numbers being "times called"
		 */
		/* parse fail-frequence */
		for(i = 0 ; *p && !isspace(*p) && ((unsigned) i < sizeof(szBuf) - 1) ; ++i) {
			szBuf[i] = *p++;
		}
		szBuf[i] = '\0';
		if(isspace(*p))
			++p;
		pData->iFailFrequency = atoi((char*) szBuf);
		/* parse resume-after */
		for(i = 0 ; *p && !isspace(*p) && ((unsigned) i < sizeof(szBuf) - 1) ; ++i) {
			szBuf[i] = *p++;
		}
		szBuf[i] = '\0';
		if(isspace(*p))
			++p;
		pData->iResumeAfter = atoi((char*) szBuf);
		pData->iCurrCallNbr = 1;
		pData->mode = MD_FAIL;
	} else if(!strcmp((char*) szBuf, "randfail")) {
		pData->mode = MD_RANDFAIL;
	} else if(!strcmp((char*) szBuf, "always_suspend")) {
		pData->mode = MD_ALWAYS_SUSPEND;
	} else {
		dbgprintf("invalid mode '%s', doing 'sleep 1 0' - fix your config\n", szBuf);
	}

	pData->bEchoStdout = cs.bEchoStdout;
	CHKiRet(cflineParseTemplateName(&p, *ppOMSR, 0, OMSR_NO_RQD_TPL_OPTS,
				         (uchar*)"RSYSLOG_TraditionalForwardFormat"));

CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINmodExit
CODESTARTmodExit
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
CODEqueryEtryPt_STD_CONF2_CNFNAME_QUERIES 
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
INITLegCnfVars
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"actionomtestingechostdout", 0, eCmdHdlrBinary, NULL,
				   &cs.bEchoStdout, STD_LOADABLE_MODULE_ID));
	/* we seed the random-number generator in any case... */
	srand(time(NULL));
ENDmodInit
/*
 * vi:set ai:
 */
