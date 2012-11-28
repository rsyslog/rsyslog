/* omprog.c
 * This output plugin enables rsyslog to execute a program and
 * feed it the message stream as standard input.
 *
 * NOTE: read comments in module-template.h for more specifics!
 *
 * File begun on 2009-04-01 by RGerhards
 *
 * Copyright 2009-2012 Adiscon GmbH.
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
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <wait.h>
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include "cfsysline.h"

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("omprog")

/* internal structures
 */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(errmsg)

typedef struct _instanceData {
	uchar *szBinary;	/* name of binary to call */
	uchar *tplName;		/* assigned output template */
	pid_t pid;		/* pid of currently running process */
	int fdPipe;		/* file descriptor to write to */
	int bIsRunning;		/* is binary currently running? 0-no, 1-yes */
} instanceData;

typedef struct configSettings_s {
	uchar *szBinary;	/* name of binary to call */
} configSettings_t;
static configSettings_t cs;


/* tables for interfacing with the v6 config system */
/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {
	{ "binary", eCmdHdlrString, CNFPARAM_REQUIRED },
	{ "template", eCmdHdlrGetWord, 0 }
};
static struct cnfparamblk actpblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(actpdescr)/sizeof(struct cnfparamdescr),
	  actpdescr
	};

BEGINinitConfVars		/* (re)set config variables to default values */
CODESTARTinitConfVars 
	cs.szBinary = NULL;	/* name of binary to call */
ENDinitConfVars

/* config settings */

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
	if(pData->szBinary != NULL)
		free(pData->szBinary);
ENDfreeInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
ENDdbgPrintInstInfo


BEGINtryResume
CODESTARTtryResume
ENDtryResume


/* execute the child process (must be called in child context
 * after fork).
 */

static void execBinary(instanceData *pData, int fdStdin)
{
	int i;
	struct sigaction sigAct;
	char *newargv[] = { NULL };
	char *newenviron[] = { NULL };

	assert(pData != NULL);

	fclose(stdin);
	if(dup(fdStdin) == -1) {
		DBGPRINTF("omprog: dup() failed\n");
		/* do some more error handling here? Maybe if the module
		 * gets some more widespread use...
		 */
	}
	//fclose(stdout);

	/* we close all file handles as we fork soon
	 * Is there a better way to do this? - mail me! rgerhards@adiscon.com
	 */
#	ifndef VALGRIND /* we can not use this with valgrind - too many errors... */
	for(i = 3 ; i <= 65535 ; ++i)
		close(i);
#	endif

	/* reset signal handlers to default */
	memset(&sigAct, 0, sizeof(sigAct));
	sigfillset(&sigAct.sa_mask);
	sigAct.sa_handler = SIG_DFL;
	for(i = 1 ; i < NSIG ; ++i)
		sigaction(i, &sigAct, NULL);

	alarm(0);

	/* finally exec child */
	execve((char*)pData->szBinary, newargv, newenviron);
	/* switch to?
	execlp((char*)program, (char*) program, (char*)arg, NULL);
	*/

	/* we should never reach this point, but if we do, we terminate */
	exit(1);
}


/* creates a pipe and starts program, uses pipe as stdin for program.
 * rgerhards, 2009-04-01
 */
static rsRetVal
openPipe(instanceData *pData)
{
	int pipefd[2];
	pid_t cpid;
	DEFiRet;

	assert(pData != NULL);

	if(pipe(pipefd) == -1) {
		ABORT_FINALIZE(RS_RET_ERR_CREAT_PIPE);
	}

	DBGPRINTF("executing program '%s'\n", pData->szBinary);

	/* NO OUTPUT AFTER FORK! */

	cpid = fork();
	if(cpid == -1) {
		ABORT_FINALIZE(RS_RET_ERR_FORK);
	}

	if(cpid == 0) {    
		/* we are now the child, just set the right selectors and
		 * exec the binary. If that fails, there is not much we can do.
		 */
		close(pipefd[1]);
		execBinary(pData, pipefd[0]);
		/*NO CODE HERE - WILL NEVER BE REACHED!*/
	}

	DBGPRINTF("child has pid %d\n", (int) cpid);
	pData->fdPipe = pipefd[1];
	pData->pid = cpid;
	close(pipefd[0]);
	pData->bIsRunning = 1;
finalize_it:
	RETiRet;
}


/* clean up after a terminated child
 */
static inline rsRetVal
cleanup(instanceData *pData)
{
	int status;
	int ret;
	char errStr[1024];
	DEFiRet;

	assert(pData != NULL);
	assert(pData->bIsRunning == 1);
	ret = waitpid(pData->pid, &status, 0);
	if(ret != pData->pid) {
		/* if waitpid() fails, we can not do much - try to ignore it... */
		DBGPRINTF("waitpid() returned state %d[%s], future malfunction may happen\n", ret,
			   rs_strerror_r(errno, errStr, sizeof(errStr)));
	} else {
		/* check if we should print out some diagnostic information */
		DBGPRINTF("waitpid status return for program '%s': %2.2x\n",
			  pData->szBinary, status);
		if(WIFEXITED(status)) {
			errmsg.LogError(0, NO_ERRCODE, "program '%s' exited normally, state %d",
					pData->szBinary, WEXITSTATUS(status));
		} else if(WIFSIGNALED(status)) {
			errmsg.LogError(0, NO_ERRCODE, "program '%s' terminated by signal %d.",
					pData->szBinary, WTERMSIG(status));
		}
	}

	pData->bIsRunning = 0;
	RETiRet;
}


/* try to restart the binary when it has stopped.
 */
static inline rsRetVal
tryRestart(instanceData *pData)
{
	DEFiRet;
	assert(pData != NULL);
	assert(pData->bIsRunning == 0);

	iRet = openPipe(pData);
	RETiRet;
}


/* write to pipe
 * note that we do not try to run block-free. If the users fears something
 * may block (and this not be acceptable), the action should be run on its
 * own action queue.
 */
static rsRetVal
writePipe(instanceData *pData, uchar *szMsg)
{
	int lenWritten;
	int lenWrite;
	int writeOffset;
	char errStr[1024];
	DEFiRet;
	
	assert(pData != NULL);

	lenWrite = strlen((char*)szMsg);
	writeOffset = 0;

	do
	{
		lenWritten = write(pData->fdPipe, ((char*)szMsg)+writeOffset, lenWrite);
		if(lenWritten == -1) {
			switch(errno) {
				case EPIPE:
					DBGPRINTF("Program '%s' terminated, trying to restart\n",
						  pData->szBinary);
					CHKiRet(cleanup(pData));
					CHKiRet(tryRestart(pData));
					break;
				default:
					DBGPRINTF("error %d writing to pipe: %s\n", errno,
						   rs_strerror_r(errno, errStr, sizeof(errStr)));
					ABORT_FINALIZE(RS_RET_ERR_WRITE_PIPE);
					break;
			}
		} else {
			writeOffset += lenWritten;
		}
	} while(lenWritten != lenWrite);


finalize_it:
	RETiRet;
}


BEGINdoAction
CODESTARTdoAction
	if(pData->bIsRunning == 0) {
		openPipe(pData);
	}
	
	iRet = writePipe(pData, ppString[0]);

	if(iRet != RS_RET_OK)
		iRet = RS_RET_SUSPENDED;
ENDdoAction


static inline void
setInstParamDefaults(instanceData *pData)
{
	pData->szBinary = NULL;
	pData->fdPipe = -1;
	pData->bIsRunning = 0;
}

BEGINnewActInst
	struct cnfparamvals *pvals;
	int i;
CODESTARTnewActInst
	if((pvals = nvlstGetParams(lst, &actpblk, NULL)) == NULL) {
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	CHKiRet(createInstance(&pData));
	setInstParamDefaults(pData);

	CODE_STD_STRING_REQUESTnewActInst(1)
	for(i = 0 ; i < actpblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(actpblk.descr[i].name, "binary")) {
			pData->szBinary = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "template")) {
			pData->tplName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else {
			dbgprintf("omprog: program error, non-handled "
			  "param '%s'\n", actpblk.descr[i].name);
		}
	}

	if(pData->tplName == NULL) {
		CHKiRet(OMSRsetEntry(*ppOMSR, 0, (uchar*) "RSYSLOG_FileFormat",
			OMSR_NO_RQD_TPL_OPTS));
	} else {
		CHKiRet(OMSRsetEntry(*ppOMSR, 0,
			(uchar*) strdup((char*) pData->tplName),
			OMSR_NO_RQD_TPL_OPTS));
	}
CODE_STD_FINALIZERnewActInst
	cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst

BEGINparseSelectorAct
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	/* first check if this config line is actually for us */
	if(strncmp((char*) p, ":omprog:", sizeof(":omprog:") - 1)) {
		ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
	}

	/* ok, if we reach this point, we have something for us */
	p += sizeof(":omprog:") - 1; /* eat indicator sequence  (-1 because of '\0'!) */
	if(cs.szBinary == NULL) {
		errmsg.LogError(0, RS_RET_CONF_RQRD_PARAM_MISSING,
			"no binary to execute specified");
		ABORT_FINALIZE(RS_RET_CONF_RQRD_PARAM_MISSING);
	}

	CHKiRet(createInstance(&pData));

	if(cs.szBinary == NULL) {
		errmsg.LogError(0, RS_RET_CONF_RQRD_PARAM_MISSING,
			"no binary to execute specified");
		ABORT_FINALIZE(RS_RET_CONF_RQRD_PARAM_MISSING);
	}

	CHKmalloc(pData->szBinary = (uchar*) strdup((char*)cs.szBinary));
	/* check if a non-standard template is to be applied */
	if(*(p-1) == ';')
		--p;
	CHKiRet(cflineParseTemplateName(&p, *ppOMSR, 0, 0, (uchar*) "RSYSLOG_FileFormat"));
CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINmodExit
CODESTARTmodExit
	free(cs.szBinary);
	cs.szBinary = NULL;
	CHKiRet(objRelease(errmsg, CORE_COMPONENT));
finalize_it:
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
CODEqueryEtryPt_STD_CONF2_CNFNAME_QUERIES 
CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES
ENDqueryEtryPt



/* Reset config variables for this module to default values.
 */
static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	DEFiRet;
	free(cs.szBinary);
	cs.szBinary = NULL;
	RETiRet;
}


BEGINmodInit()
CODESTARTmodInit
INITLegCnfVars
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"actionomprogbinary", 0, eCmdHdlrGetWord, NULL, &cs.szBinary, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
CODEmodInit_QueryRegCFSLineHdlr
ENDmodInit

/* vi:set ai:
 */
