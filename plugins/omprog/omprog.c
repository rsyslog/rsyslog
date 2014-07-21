/* omprog.c
 * This output plugin enables rsyslog to execute a program and
 * feed it the message stream as standard input.
 *
 * NOTE: read comments in module-template.h for more specifics!
 *
 * File begun on 2009-04-01 by RGerhards
 *
 * Copyright 2009-2014 Adiscon GmbH.
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
#include <fcntl.h>
#include <wait.h>
#include <pthread.h>
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
	char **aParams;		/* Optional Parameters for binary command */
	uchar *tplName;		/* assigned output template */
	int iParams;		/* Holds the count of parameters if set*/
	int bForceSingleInst;	/* only a single wrkr instance of program permitted? */
	uchar *outputFileName;	/* name of file for std[out/err] or NULL if to discard */
	pthread_mutex_t mut;	/* make sure only one instance is active */
} instanceData;

typedef struct wrkrInstanceData {
	instanceData *pData;
	pid_t pid;		/* pid of currently running process */
	int fdOutput;		/* it's fd (-1 if closed) */
	int fdPipeOut;		/* file descriptor to write to */
	int fdPipeIn;		/* fd we receive messages from the program (if we want to) */
	int bIsRunning;		/* is binary currently running? 0-no, 1-yes */
} wrkrInstanceData_t;

typedef struct configSettings_s {
	uchar *szBinary;	/* name of binary to call */
} configSettings_t;
static configSettings_t cs;


/* tables for interfacing with the v6 config system */
/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {
	{ "binary", eCmdHdlrString, CNFPARAM_REQUIRED },
	{ "output", eCmdHdlrString, 0 },
	{ "forcesingleinstance", eCmdHdlrBinary, 0 },
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
	pthread_mutex_init(&pData->mut, NULL);
ENDcreateInstance

BEGINcreateWrkrInstance
CODESTARTcreateWrkrInstance
	pWrkrData->fdPipeIn = -1;
	pWrkrData->fdPipeOut = -1;
	pWrkrData->fdOutput = -1;
	pWrkrData->bIsRunning = 0;
ENDcreateWrkrInstance


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURERepeatedMsgReduction)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINfreeInstance
	int i;
CODESTARTfreeInstance
	pthread_mutex_destroy(&pData->mut);
	free(pData->szBinary);
	free(pData->outputFileName);
	if(pData->aParams != NULL) {
		for (i = 0; i < pData->iParams; i++) {
			free(pData->aParams[i]);
		}
		free(pData->aParams); 
	}
ENDfreeInstance

BEGINfreeWrkrInstance
CODESTARTfreeWrkrInstance
ENDfreeWrkrInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
ENDdbgPrintInstInfo


BEGINtryResume
CODESTARTtryResume
ENDtryResume


/* As this is assume to be a debug function, we only make
 * best effort to write the message but do *not* try very
 * hard to handle errors. -- rgerhards, 2014-01-16
 */
static void
writeProgramOutput(wrkrInstanceData_t *__restrict__ const pWrkrData,
	const char *__restrict__ const buf,
	const ssize_t lenBuf)
{
	char errStr[1024];
	ssize_t r;

dbgprintf("omprog: writeProgramOutput, fd %d\n", pWrkrData->fdOutput);
	if(pWrkrData->fdOutput == -1) {
		pWrkrData->fdOutput = open((char*)pWrkrData->pData->outputFileName,
				       O_WRONLY | O_APPEND | O_CREAT, 0600);
		if(pWrkrData->fdOutput == -1) {
			DBGPRINTF("omprog: error opening output file %s: %s\n",
				   pWrkrData->pData->outputFileName, 
				   rs_strerror_r(errno, errStr, sizeof(errStr)));
			goto done;
		}
	}

	r = write(pWrkrData->fdOutput, buf, (size_t) lenBuf);
	if(r != lenBuf) {
		DBGPRINTF("omprog: problem writing output file %s: bytes "
			  "requested %lld, written %lld, msg: %s\n",
			   pWrkrData->pData->outputFileName, (long long) lenBuf, (long long) r,
			   rs_strerror_r(errno, errStr, sizeof(errStr)));
	}
done:	return;
}


/* check output of the executed program
 * If configured to care about the output, we check if there is some and,
 * if so, properly handle it.
 */
static void
checkProgramOutput(wrkrInstanceData_t *__restrict__ const pWrkrData)
{
	char buf[4096];
	ssize_t r;

dbgprintf("omprog: checking prog output, fd %d\n", pWrkrData->fdPipeIn);
	if(pWrkrData->fdPipeIn == -1)
		goto done;

	do {
memset(buf, 0, sizeof(buf));
		r = read(pWrkrData->fdPipeIn, buf, sizeof(buf));
dbgprintf("omprog: read state %lld, data '%s'\n", (long long) r, buf);
		if(r > 0)
			writeProgramOutput(pWrkrData, buf, r);
	} while(r > 0);

done:	return;
}



/* execute the child process (must be called in child context
 * after fork).
 */
static void
execBinary(wrkrInstanceData_t *pWrkrData, int fdStdin, int fdStdOutErr)
{
	int i, iRet;
	struct sigaction sigAct;
	sigset_t set;
	char errStr[1024];
	char *newenviron[] = { NULL };

	fclose(stdin);
	if(dup(fdStdin) == -1) {
		DBGPRINTF("omprog: dup() stdin failed\n");
		/* do some more error handling here? Maybe if the module
		 * gets some more widespread use...
		 */
	}
	if(pWrkrData->pData->outputFileName == NULL) {
		close(fdStdOutErr);
	} else {
		close(1);
		if(dup(fdStdOutErr) == -1) {
			DBGPRINTF("omprog: dup() stdout failed\n");
		}
		close(2);
		if(dup(fdStdOutErr) == -1) {
			DBGPRINTF("omprog: dup() stderr failed\n");
		}
	}

	/* we close all file handles as we fork soon
	 * Is there a better way to do this? - mail me! rgerhards@adiscon.com
	 */
#	ifndef VALGRIND /* we can not use this with valgrind - too many errors... */
	for(i = 3 ; i <= 65535 ; ++i)
		close(i);
#	endif

	/* reset signal handlers to default */
	memset(&sigAct, 0, sizeof(sigAct));
	sigemptyset(&sigAct.sa_mask);
	sigAct.sa_handler = SIG_DFL;
	for(i = 1 ; i < NSIG ; ++i)
		sigaction(i, &sigAct, NULL);
	/* we need to block SIGINT, otherwise our program is cancelled when we are
	 * stopped in debug mode.
	 */
	sigAct.sa_handler = SIG_IGN;
	sigaction(SIGINT, &sigAct, NULL);
	sigemptyset(&set);
        sigprocmask(SIG_SETMASK, &set, NULL);

	alarm(0);

	/* finally exec child */
	iRet = execve((char*)pWrkrData->pData->szBinary, pWrkrData->pData->aParams, newenviron);
	if(iRet == -1) {
		/* Note: this will go to stdout of the **child**, so rsyslog will never
		 * see it except when stdout is captured. If we use the plugin interface,
		 * we can use this to convey a proper status back!
		 */
		rs_strerror_r(errno, errStr, sizeof(errStr));
		DBGPRINTF("omprog: failed to execute binary '%s': %s\n",
			  pWrkrData->pData->szBinary, errStr); 
		openlog("rsyslogd", 0, LOG_SYSLOG);
		syslog(LOG_ERR, "omprog: failed to execute binary '%s': %s\n",
			  pWrkrData->pData->szBinary, errStr); 
	}
	
	/* we should never reach this point, but if we do, we terminate */
	exit(1);
}


/* creates a pipe and starts program, uses pipe as stdin for program.
 * rgerhards, 2009-04-01
 */
static rsRetVal
openPipe(wrkrInstanceData_t *pWrkrData)
{
	int pipestdin[2];
	int pipestdout[2];
	pid_t cpid;
	int flags;
	DEFiRet;

	if(pipe(pipestdin) == -1) {
		ABORT_FINALIZE(RS_RET_ERR_CREAT_PIPE);
	}
	if(pipe(pipestdout) == -1) {
		ABORT_FINALIZE(RS_RET_ERR_CREAT_PIPE);
	}

	DBGPRINTF("omprog: executing program '%s' with '%d' parameters\n",
		  pWrkrData->pData->szBinary, pWrkrData->pData->iParams);

	/* NO OUTPUT AFTER FORK! */

	cpid = fork();
	if(cpid == -1) {
		ABORT_FINALIZE(RS_RET_ERR_FORK);
	}
	pWrkrData->pid = cpid;

	if(cpid == 0) {    
		/* we are now the child, just exec the binary. */
		close(pipestdin[1]); /* close those pipe "ports" that */
		close(pipestdout[0]); /* we don't need */
		execBinary(pWrkrData, pipestdin[0], pipestdout[1]);
		/*NO CODE HERE - WILL NEVER BE REACHED!*/
	}

	DBGPRINTF("omprog: child has pid %d\n", (int) cpid);
	if(pWrkrData->pData->outputFileName != NULL) {
		pWrkrData->fdPipeIn = dup(pipestdout[0]);
		/* we need to set our fd to be non-blocking! */
		flags = fcntl(pWrkrData->fdPipeIn, F_GETFL);
		flags |= O_NONBLOCK;
		fcntl(pWrkrData->fdPipeIn, F_SETFL, flags);
	} else {
		pWrkrData->fdPipeIn = -1;
	}
	close(pipestdin[0]);
	close(pipestdout[1]);
	pWrkrData->pid = cpid;
	pWrkrData->fdPipeOut = pipestdin[1];
	pWrkrData->bIsRunning = 1;
finalize_it:
	RETiRet;
}


/* clean up after a terminated child
 */
static inline rsRetVal
cleanup(wrkrInstanceData_t *pWrkrData)
{
	int status;
	int ret;
	char errStr[1024];
	DEFiRet;

	assert(pWrkrData->bIsRunning == 1);
	ret = waitpid(pWrkrData->pid, &status, 0);
	if(ret != pWrkrData->pid) {
		/* if waitpid() fails, we can not do much - try to ignore it... */
		DBGPRINTF("omprog: waitpid() returned state %d[%s], future malfunction may happen\n", ret,
			   rs_strerror_r(errno, errStr, sizeof(errStr)));
	} else {
		/* check if we should print out some diagnostic information */
		DBGPRINTF("omprog: waitpid status return for program '%s': %2.2x\n",
			  pWrkrData->pData->szBinary, status);
		if(WIFEXITED(status)) {
			errmsg.LogError(0, NO_ERRCODE, "program '%s' exited normally, state %d",
					pWrkrData->pData->szBinary, WEXITSTATUS(status));
		} else if(WIFSIGNALED(status)) {
			errmsg.LogError(0, NO_ERRCODE, "program '%s' terminated by signal %d.",
					pWrkrData->pData->szBinary, WTERMSIG(status));
		}
	}

	checkProgramOutput(pWrkrData); /* try to catch any late messages */

	if(pWrkrData->fdOutput != -1) {
		close(pWrkrData->fdOutput);
		pWrkrData->fdOutput = -1;
	}
	if(pWrkrData->fdPipeIn != -1) {
		close(pWrkrData->fdPipeIn);
		pWrkrData->fdPipeIn = -1;
	}
	if(pWrkrData->fdPipeOut != -1) {
		close(pWrkrData->fdPipeOut);
		pWrkrData->fdPipeOut = -1;
	}
	pWrkrData->bIsRunning = 0;
	pWrkrData->bIsRunning = 0;
	RETiRet;
}


/* try to restart the binary when it has stopped.
 */
static inline rsRetVal
tryRestart(wrkrInstanceData_t *pWrkrData)
{
	DEFiRet;
	assert(pWrkrData->bIsRunning == 0);

	iRet = openPipe(pWrkrData);
	RETiRet;
}

/* write to pipe
 * note that we do not try to run block-free. If the users fears something
 * may block (and this not be acceptable), the action should be run on its
 * own action queue.
 */
static rsRetVal
writePipe(wrkrInstanceData_t *pWrkrData, uchar *szMsg)
{
	int lenWritten;
	int lenWrite;
	int writeOffset;
	char errStr[1024];
	DEFiRet;
	
	lenWrite = strlen((char*)szMsg);
	writeOffset = 0;

	do {
		checkProgramOutput(pWrkrData);
dbgprintf("omprog: writing to prog (fd %d): %s\n", pWrkrData->fdPipeOut, szMsg);
		lenWritten = write(pWrkrData->fdPipeOut, ((char*)szMsg)+writeOffset, lenWrite);
		if(lenWritten == -1) {
			switch(errno) {
			case EPIPE:
				DBGPRINTF("omprog: program '%s' terminated, trying to restart\n",
					  pWrkrData->pData->szBinary);
				CHKiRet(cleanup(pWrkrData));
				CHKiRet(tryRestart(pWrkrData));
				break;
			default:
				DBGPRINTF("omprog: error %d writing to pipe: %s\n", errno,
					   rs_strerror_r(errno, errStr, sizeof(errStr)));
				ABORT_FINALIZE(RS_RET_ERR_WRITE_PIPE);
				break;
			}
		} else {
			writeOffset += lenWritten;
		}
	} while(lenWritten != lenWrite);

	checkProgramOutput(pWrkrData);

finalize_it:
	RETiRet;
}


BEGINdoAction
	instanceData *pData;
CODESTARTdoAction
dbgprintf("DDDD:omprog processing message\n");
	pData = pWrkrData->pData;
	if(pData->bForceSingleInst)
		pthread_mutex_lock(&pData->mut);
	if(pWrkrData->bIsRunning == 0) {
		openPipe(pWrkrData);
	}
	
	iRet = writePipe(pWrkrData, ppString[0]);

	if(iRet != RS_RET_OK)
		iRet = RS_RET_SUSPENDED;
	if(pData->bForceSingleInst)
		pthread_mutex_unlock(&pData->mut);
ENDdoAction


static inline void
setInstParamDefaults(instanceData *pData)
{
	pData->szBinary = NULL;
	pData->aParams = NULL;
	pData->outputFileName = NULL;
	pData->iParams = 0;
	pData->bForceSingleInst = 0;
}

BEGINnewActInst
	struct cnfparamvals *pvals;
	sbool bInQuotes;
	int i;
	int iPrm;
	unsigned char *c;
	es_size_t iCnt;
	es_size_t iStr;
	es_str_t *estrBinary;
	es_str_t *estrParams;
	es_str_t *estrTmp;
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
			estrBinary = pvals[i].val.d.estr; 
			estrParams = NULL; 

			/* Search for space */
			c = es_getBufAddr(pvals[i].val.d.estr);
			iCnt = 0;
			while(iCnt < es_strlen(pvals[i].val.d.estr) ) {
				if (c[iCnt] == ' ') {
					/* Split binary name from parameters */
					estrBinary = es_newStrFromSubStr ( pvals[i].val.d.estr, 0, iCnt ); 
					estrParams = es_newStrFromSubStr ( pvals[i].val.d.estr, iCnt+1, es_strlen(pvals[i].val.d.estr)); 
					break;
				}
				iCnt++;
			}	
			/* Assign binary and params */
			pData->szBinary = (uchar*)es_str2cstr(estrBinary, NULL);
			dbgprintf("omprog: szBinary = '%s'\n", pData->szBinary); 
			/* Check for Params! */
			if (estrParams != NULL) {
				dbgprintf("omprog: szParams = '%s'\n", es_str2cstr(estrParams, NULL) ); 
				
				/* Count parameters if set */
				c = es_getBufAddr(estrParams); /* Reset to beginning */
				pData->iParams = 2; /* Set default to 2, first parameter for binary and second parameter at least from config*/
				iCnt = 0;
				while(iCnt < es_strlen(estrParams) ) {
					if (c[iCnt] == ' ' && c[iCnt-1] != '\\')
						 pData->iParams++; 
					iCnt++;
				}
				dbgprintf("omprog: iParams = '%d'\n", pData->iParams); 

				/* Create argv Array */
				CHKmalloc(pData->aParams = malloc( (pData->iParams+1) * sizeof(char*))); /* One more for first param */ 

				/* Second Loop, create parameter array*/
				c = es_getBufAddr(estrParams); /* Reset to beginning */
				iCnt = iStr = iPrm = 0;
				estrTmp = NULL; 
				bInQuotes = FALSE; 
				/* Set first parameter to binary */
				pData->aParams[iPrm] = strdup((char*)pData->szBinary); 
				dbgprintf("omprog: Param (%d): '%s'\n", iPrm, pData->aParams[iPrm]);
				iPrm++; 
				while(iCnt < es_strlen(estrParams) ) {
					if ( c[iCnt] == ' ' && !bInQuotes ) {
						/* Copy into Param Array! */
						estrTmp = es_newStrFromSubStr( estrParams, iStr, iCnt-iStr); 
					}
					else if ( iCnt+1 >= es_strlen(estrParams) ) {
						/* Copy rest of string into Param Array! */
						estrTmp = es_newStrFromSubStr( estrParams, iStr, iCnt-iStr+1); 
					}
					else if (c[iCnt] == '"') {
						/* switch inQuotes Mode */
						bInQuotes = !bInQuotes; 
					}

					if ( estrTmp != NULL ) {
						pData->aParams[iPrm] = es_str2cstr(estrTmp, NULL); 
						iStr = iCnt+1; /* Set new start */
						dbgprintf("omprog: Param (%d): '%s'\n", iPrm, pData->aParams[iPrm]);
						es_deleteStr( estrTmp );
						estrTmp = NULL; 
						iPrm++;
					}

					/*Next char*/
					iCnt++;
				}
				/* NULL last parameter! */
				pData->aParams[iPrm] = NULL; 

			}
		} else if(!strcmp(actpblk.descr[i].name, "output")) {
			pData->outputFileName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "forcesingleinstance")) {
			pData->bForceSingleInst = (int) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "template")) {
			pData->tplName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else {
			dbgprintf("omprog: program error, non-handled param '%s'\n", actpblk.descr[i].name);
		}
	}

	CHKiRet(OMSRsetEntry(*ppOMSR, 0, (uchar*)strdup((pData->tplName == NULL) ? 
						"RSYSLOG_FileFormat" : (char*)pData->tplName),
						OMSR_NO_RQD_TPL_OPTS));
	DBGPRINTF("omprog: bForceSingleInst %d\n", pData->bForceSingleInst);
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
CODEqueryEtryPt_STD_OMOD8_QUERIES
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
