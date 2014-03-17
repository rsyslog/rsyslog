/* mmexternal.c
 * This core plugin is an interface module to message modification
 * modules written in languages other than C.
 *
 * Copyright 2014 by Rainer Gerhards
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
#include "msg.h"
#include "errmsg.h"
#include "cfsysline.h"

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("mmexternal")

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


/* As this is just a debug function, we only make
 * best effort to write the message but do *not* try very
 * hard to handle errors. -- rgerhards, 2014-01-16
 */
static void
writeOutputDebug(wrkrInstanceData_t *__restrict__ const pWrkrData,
	const char *__restrict__ const buf,
	const ssize_t lenBuf)
{
	char errStr[1024];
	ssize_t r;

	if(pWrkrData->pData->outputFileName == NULL)
		goto done;

	if(pWrkrData->fdOutput == -1) {
		pWrkrData->fdOutput = open((char*)pWrkrData->pData->outputFileName,
				       O_WRONLY | O_APPEND | O_CREAT, 0600);
		if(pWrkrData->fdOutput == -1) {
			DBGPRINTF("mmexternal: error opening output file %s: %s\n",
				   pWrkrData->pData->outputFileName, 
				   rs_strerror_r(errno, errStr, sizeof(errStr)));
			goto done;
		}
	}

	r = write(pWrkrData->fdOutput, buf, (size_t) lenBuf);
	if(r != lenBuf) {
		DBGPRINTF("mmexternal: problem writing output file %s: bytes "
			  "requested %lld, written %lld, msg: %s\n",
			   pWrkrData->pData->outputFileName, (long long) lenBuf, (long long) r,
			   rs_strerror_r(errno, errStr, sizeof(errStr)));
	}
done:	return;
}



/* Get reply from external program. Note that we *must* receive one
 * reply for each message sent (half-duplex protocol).
 */
static void
processProgramReply(wrkrInstanceData_t *__restrict__ const pWrkrData, msg_t *const pMsg)
{
	char buf[4096];
	char errStr[1024];
	ssize_t r;

dbgprintf("mmexternal: checking prog output, fd %d\n", pWrkrData->fdPipeIn);
	do {
		r = read(pWrkrData->fdPipeIn, buf, sizeof(buf)-1);
		if(r > 0)
			buf[r] = '\0'; /* space reserved in read! */
		else
			buf[0] = '\0';
dbgprintf("mmexternal: read state %lld, data '%s'\n", (long long) r, buf);
		if(Debug && r == -1) {
			DBGPRINTF("mmexternal: error reading from external program: %s\n",
				   rs_strerror_r(errno, errStr, sizeof(errStr)));
		}
		if(r > 0) {
			writeOutputDebug(pWrkrData, buf, r);
			MsgSetPropsViaJSON(pMsg, (uchar*)buf);
		}
	} while(0); // TODO: change this so that we actually read one line!

	return;
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
		DBGPRINTF("mmexternal: dup() stdin failed\n");
	}
	close(1);
	if(dup(fdStdOutErr) == -1) {
		DBGPRINTF("mmexternal: dup() stdout failed\n");
	}
	/* todo: different pipe for stderr? */
	close(2);
	if(dup(fdStdOutErr) == -1) {
		DBGPRINTF("mmexternal: dup() stderr failed\n");
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
		DBGPRINTF("mmexternal: failed to execute binary '%s': %s\n",
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
	DEFiRet;

	if(pipe(pipestdin) == -1) {
		ABORT_FINALIZE(RS_RET_ERR_CREAT_PIPE);
	}
	if(pipe(pipestdout) == -1) {
		ABORT_FINALIZE(RS_RET_ERR_CREAT_PIPE);
	}

	DBGPRINTF("mmexternal: executing program '%s' with '%d' parameters\n",
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

	DBGPRINTF("mmexternal: child has pid %d\n", (int) cpid);
	pWrkrData->fdPipeIn = dup(pipestdout[0]);
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
		DBGPRINTF("mmexternal: waitpid() returned state %d[%s], future malfunction may happen\n", ret,
			   rs_strerror_r(errno, errStr, sizeof(errStr)));
	} else {
		/* check if we should print out some diagnostic information */
		DBGPRINTF("mmexternal: waitpid status return for program '%s': %2.2x\n",
			  pWrkrData->pData->szBinary, status);
		if(WIFEXITED(status)) {
			errmsg.LogError(0, NO_ERRCODE, "program '%s' exited normally, state %d",
					pWrkrData->pData->szBinary, WEXITSTATUS(status));
		} else if(WIFSIGNALED(status)) {
			errmsg.LogError(0, NO_ERRCODE, "program '%s' terminated by signal %d.",
					pWrkrData->pData->szBinary, WTERMSIG(status));
		}
	}

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
callExtProg(wrkrInstanceData_t *__restrict__ const pWrkrData, msg_t *__restrict__ const pMsg)
{
	int lenWritten;
	int lenWrite;
	int writeOffset;
	char errStr[1024];
	uchar msgStr[4096];
	uchar *rawmsg; /* string to be processed by external program */
	DEFiRet;
	
	getRawMsg(pMsg, &rawmsg, &lenWrite);
	lenWrite = snprintf((char*)msgStr, sizeof(msgStr), "%s\n", rawmsg); // TODO: make this more solid!
	writeOffset = 0;

	do {
dbgprintf("mmexternal: writing to prog (fd %d): %s\n", pWrkrData->fdPipeOut, msgStr);
		lenWritten = write(pWrkrData->fdPipeOut, ((char*)msgStr)+writeOffset, lenWrite);
		if(lenWritten == -1) {
			switch(errno) {
			case EPIPE:
				DBGPRINTF("mmexternal: program '%s' terminated, trying to restart\n",
					  pWrkrData->pData->szBinary);
				CHKiRet(cleanup(pWrkrData));
				CHKiRet(tryRestart(pWrkrData));
				break;
			default:
				DBGPRINTF("mmexternal: error %d writing to pipe: %s\n", errno,
					   rs_strerror_r(errno, errStr, sizeof(errStr)));
				ABORT_FINALIZE(RS_RET_ERR_WRITE_PIPE);
				break;
			}
		} else {
			writeOffset += lenWritten;
		}
	} while(lenWritten != lenWrite);

	processProgramReply(pWrkrData, pMsg);

finalize_it:
	RETiRet;
}


BEGINdoAction
	instanceData *pData;
CODESTARTdoAction
dbgprintf("DDDD:mmexternal processing message\n");
	pData = pWrkrData->pData;
	if(pData->bForceSingleInst)
		pthread_mutex_lock(&pData->mut);
	if(pWrkrData->bIsRunning == 0) {
		openPipe(pWrkrData);
	}
	
	iRet = callExtProg(pWrkrData, (msg_t*)ppString[0]);

	if(iRet != RS_RET_OK)
		iRet = RS_RET_SUSPENDED;
	if(pData->bForceSingleInst)
		pthread_mutex_unlock(&pData->mut);
dbgprintf("DDDD:mmexternal DONE processing message\n");
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
			dbgprintf("mmexternal: szBinary = '%s'\n", pData->szBinary); 
			/* Check for Params! */
			if (estrParams != NULL) {
				dbgprintf("mmexternal: szParams = '%s'\n", es_str2cstr(estrParams, NULL) ); 
				
				/* Count parameters if set */
				c = es_getBufAddr(estrParams); /* Reset to beginning */
				pData->iParams = 2; /* Set default to 2, first parameter for binary and second parameter at least from config*/
				iCnt = 0;
				while(iCnt < es_strlen(estrParams) ) {
					if (c[iCnt] == ' ' && c[iCnt-1] != '\\')
						 pData->iParams++; 
					iCnt++;
				}
				dbgprintf("mmexternal: iParams = '%d'\n", pData->iParams); 

				/* Create argv Array */
				CHKmalloc(pData->aParams = malloc( (pData->iParams+1) * sizeof(char*))); /* One more for first param */ 

				/* Second Loop, create parameter array*/
				c = es_getBufAddr(estrParams); /* Reset to beginning */
				iCnt = iStr = iPrm = 0;
				estrTmp = NULL; 
				bInQuotes = FALSE; 
				/* Set first parameter to binary */
				pData->aParams[iPrm] = strdup((char*)pData->szBinary); 
				dbgprintf("mmexternal: Param (%d): '%s'\n", iPrm, pData->aParams[iPrm]);
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
						dbgprintf("mmexternal: Param (%d): '%s'\n", iPrm, pData->aParams[iPrm]);
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
			dbgprintf("mmexternal: program error, non-handled param '%s'\n", actpblk.descr[i].name);
		}
	}

	CHKiRet(OMSRsetEntry(*ppOMSR, 0, NULL, OMSR_TPL_AS_MSG));
	DBGPRINTF("mmexternal: bForceSingleInst %d\n", pData->bForceSingleInst);
CODE_STD_FINALIZERnewActInst
	cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst

BEGINparseSelectorAct
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	if(!strncmp((char*) p, ":mmexternal:", sizeof(":mmexternal:") - 1)) {
		errmsg.LogError(0, RS_RET_LEGA_ACT_NOT_SUPPORTED,
			"mmexternal supports only v6+ config format, use: "
			"action(type=\"mmexternal\" binary=...)");
	}
	ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
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

BEGINmodInit()
CODESTARTmodInit
INITLegCnfVars
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
CODEmodInit_QueryRegCFSLineHdlr
ENDmodInit
