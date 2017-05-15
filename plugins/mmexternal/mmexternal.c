/* mmexternal.c
 * This core plugin is an interface module to message modification
 * modules written in languages other than C.
 *
 * Copyright 2014-2016 by Rainer Gerhards
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
#if defined(_AIX) || defined(__FreeBSD__) || defined(__APPLE__) 
#include <sys/wait.h>
#else
#include <wait.h>
#endif
#ifdef _AIX /* AIXPORT */
#include <errno.h>
#endif /* AIXPORT */
#include <sys/uio.h>
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
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
	int iParams;		/* Holds the count of parameters if set*/
	int bForceSingleInst;	/* only a single wrkr instance of program permitted? */
	int inputProp;		/* what to provide as input to the external program? */
#define	INPUT_MSG 0
#define	INPUT_RAWMSG 1
#define INPUT_JSON 2
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
	char *respBuf;		/* buffer to read exernal plugin's response */
	int maxLenRespBuf;	/* (current) maximum length of response buffer */
	int lenRespBuf;		/* actual nbr of chars in response buffer */
	int idxRespBuf;		/* last char read from response buffer */
} wrkrInstanceData_t;

typedef struct configSettings_s {
	uchar *szBinary;	/* name of binary to call */
} configSettings_t;
static configSettings_t cs;


/* tables for interfacing with the v6 config system */
/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {
	{ "binary", eCmdHdlrString, CNFPARAM_REQUIRED },
	{ "interface.input", eCmdHdlrString, 0 },
	{ "output", eCmdHdlrString, 0 },
	{ "forcesingleinstance", eCmdHdlrBinary, 0 }
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
	pData->inputProp = INPUT_MSG;
	pthread_mutex_init(&pData->mut, NULL);
ENDcreateInstance

BEGINcreateWrkrInstance
CODESTARTcreateWrkrInstance
	pWrkrData->fdPipeIn = -1;
	pWrkrData->fdPipeOut = -1;
	pWrkrData->fdOutput = -1;
	pWrkrData->bIsRunning = 0;
	pWrkrData->respBuf = NULL;
	pWrkrData->maxLenRespBuf = 0;
	pWrkrData->lenRespBuf = 0;
	pWrkrData->idxRespBuf = 0;
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
	free(pWrkrData->respBuf);
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
 * reply for each message sent (half-duplex protocol). As such, the last
 * char we read MUST be \n ... we cannot have multiple LF as this is
 * forbidden by the plugin interface. We cannot have multiple responses
 * for multiple messages, as we are in half-duplex mode! This makes
 * things quite a bit simpler. So don't think the simple code does
 * not handle those border-cases that are describe to cannot exist!
 */
static void
processProgramReply(wrkrInstanceData_t *__restrict__ const pWrkrData, smsg_t *const pMsg)
{
	rsRetVal iRet;
	char errStr[1024];
	ssize_t r;
	int numCharsRead;
	char *newptr;

	numCharsRead = 0;
	do {
		if(pWrkrData->maxLenRespBuf < numCharsRead + 256) { /* 256 to permit at least a decent read */
			pWrkrData->maxLenRespBuf += 4096;
			if((newptr = realloc(pWrkrData->respBuf, pWrkrData->maxLenRespBuf)) == NULL) {
				DBGPRINTF("mmexternal: error realloc responseBuf: %s\n",
					   rs_strerror_r(errno, errStr, sizeof(errStr)));
				/* emergency - fake no update */
				strcpy(pWrkrData->respBuf, "{}\n");
				numCharsRead = 3;
				break;
			}
			pWrkrData->respBuf = newptr;
		}
		r = read(pWrkrData->fdPipeIn, pWrkrData->respBuf+numCharsRead,
			 pWrkrData->maxLenRespBuf-numCharsRead-1);
		if(r > 0) {
			numCharsRead += r;
			pWrkrData->respBuf[numCharsRead] = '\0'; /* space reserved in read! */
		} else {
			/* emergency - fake no update */
			strcpy(pWrkrData->respBuf, "{}\n");
			numCharsRead = 3;
		}
		if(Debug && r == -1) {
			DBGPRINTF("mmexternal: error reading from external program: %s\n",
				   rs_strerror_r(errno, errStr, sizeof(errStr)));
		}
	} while(pWrkrData->respBuf[numCharsRead-1] != '\n');

	writeOutputDebug(pWrkrData, pWrkrData->respBuf, numCharsRead);
	/* strip LF, which is not part of the JSON message but framing */
	pWrkrData->respBuf[numCharsRead-1] = '\0';
	iRet = MsgSetPropsViaJSON(pMsg, (uchar*)pWrkrData->respBuf);
	if(iRet != RS_RET_OK) {
		errmsg.LogError(0, iRet, "mmexternal: invalid reply '%s' from program '%s'",
				pWrkrData->respBuf, pWrkrData->pData->szBinary);
	}

	return;
}



/* execute the child process (must be called in child context
 * after fork).
 */
static void __attribute__((noreturn))
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
	/* we need to block SIGINT, otherwise the external program is cancelled when we are
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
static rsRetVal
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
static rsRetVal
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
callExtProg(wrkrInstanceData_t *__restrict__ const pWrkrData, smsg_t *__restrict__ const pMsg)
{
	int lenWritten;
	int lenWrite;
	int writeOffset;
	int i_iov;
	char errStr[1024];
	struct iovec iov[2];
	const uchar *inputstr = NULL; /* string to be processed by external program */
	DEFiRet;
	
	if(pWrkrData->pData->inputProp == INPUT_MSG) {
		inputstr = getMSG(pMsg);
		lenWrite = getMSGLen(pMsg);
	} else if(pWrkrData->pData->inputProp == INPUT_RAWMSG) {
		getRawMsg(pMsg, (uchar**)&inputstr, &lenWrite);
	} else  {
		inputstr = msgGetJSONMESG(pMsg);
		lenWrite = strlen((const char*)inputstr);
	}

	writeOffset = 0;
	do {
		DBGPRINTF("mmexternal: writing to prog (fd %d, offset %d): %s\n",
			  pWrkrData->fdPipeOut, (int) writeOffset, inputstr);
		i_iov = 0;
		if(writeOffset < lenWrite) {
			iov[0].iov_base = (char*)inputstr+writeOffset;
			iov[0].iov_len = lenWrite - writeOffset;
			++i_iov;
		}
		iov[i_iov].iov_base = (void*)"\n";
		iov[i_iov].iov_len = 1;
		lenWritten = writev(pWrkrData->fdPipeOut, iov, i_iov+1);
		if(lenWritten == -1) {
			switch(errno) {
			case EPIPE:
				DBGPRINTF("mmexternal: program '%s' terminated, trying to restart\n",
					  pWrkrData->pData->szBinary);
				CHKiRet(cleanup(pWrkrData));
				CHKiRet(tryRestart(pWrkrData));
				writeOffset = 0;
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
	} while(lenWritten != lenWrite+1);

	processProgramReply(pWrkrData, pMsg);

finalize_it:
	/* we need to free json input strings, only. All others point to memory
	 * inside the msg object, which is destroyed when the msg is destroyed.
	 */
	if(pWrkrData->pData->inputProp == INPUT_JSON)
		free((void*)inputstr);
	RETiRet;
}


BEGINdoAction_NoStrings
	smsg_t **ppMsg = (smsg_t **) pMsgData;
	smsg_t *pMsg = ppMsg[0];
	instanceData *pData;
CODESTARTdoAction
	pData = pWrkrData->pData;
	if(pData->bForceSingleInst)
		pthread_mutex_lock(&pData->mut);
	if(pWrkrData->bIsRunning == 0) {
		openPipe(pWrkrData);
	}
	
	iRet = callExtProg(pWrkrData, pMsg);

	if(iRet != RS_RET_OK)
		iRet = RS_RET_SUSPENDED;
	if(pData->bForceSingleInst)
		pthread_mutex_unlock(&pData->mut);
ENDdoAction


static void
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
	const char *cstr = NULL;
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
			DBGPRINTF("mmexternal: szBinary = '%s'\n", pData->szBinary);
			/* Check for Params! */
			if (estrParams != NULL) {
				if(Debug) {
					char *params = es_str2cstr(estrParams, NULL);
					DBGPRINTF("mmexternal: szParams = '%s'\n", params);
					free(params);
				}
				
				/* Count parameters if set */
				c = es_getBufAddr(estrParams); /* Reset to beginning */
				pData->iParams = 2; /* Set default to 2, first parameter for binary and second parameter at least from config*/
				iCnt = 0;
				while(iCnt < es_strlen(estrParams) ) {
					if (c[iCnt] == ' ' && c[iCnt-1] != '\\')
						 pData->iParams++;
					iCnt++;
				}
				DBGPRINTF("mmexternal: iParams = '%d'\n", pData->iParams);

				/* Create argv Array */
				CHKmalloc(pData->aParams = malloc( (pData->iParams+1) * sizeof(char*))); /* One more for first param */ 

				/* Second Loop, create parameter array*/
				c = es_getBufAddr(estrParams); /* Reset to beginning */
				iCnt = iStr = iPrm = 0;
				estrTmp = NULL;
				bInQuotes = FALSE;
				/* Set first parameter to binary */
				pData->aParams[iPrm] = strdup((char*)pData->szBinary);
				DBGPRINTF("mmexternal: Param (%d): '%s'\n", iPrm, pData->aParams[iPrm]);
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
						DBGPRINTF("mmexternal: Param (%d): '%s'\n", iPrm, pData->aParams[iPrm]);
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
		} else if(!strcmp(actpblk.descr[i].name, "interface.input")) {
			cstr = es_str2cstr(pvals[i].val.d.estr, NULL);
			if(!strcmp(cstr, "msg"))
				pData->inputProp = INPUT_MSG;
			else if(!strcmp(cstr, "rawmsg"))
				pData->inputProp = INPUT_RAWMSG;
			else if(!strcmp(cstr, "fulljson"))
				pData->inputProp = INPUT_JSON;
			else {
				errmsg.LogError(0, RS_RET_INVLD_INTERFACE_INPUT,
					"mmexternal: invalid interface.input parameter '%s'",
					cstr);
				ABORT_FINALIZE(RS_RET_INVLD_INTERFACE_INPUT);
			}
		} else {
			DBGPRINTF("mmexternal: program error, non-handled param '%s'\n", actpblk.descr[i].name);
		}
	}

	CHKiRet(OMSRsetEntry(*ppOMSR, 0, NULL, OMSR_TPL_AS_MSG));
	DBGPRINTF("mmexternal: bForceSingleInst %d\n", pData->bForceSingleInst);
	DBGPRINTF("mmexternal: interface.input '%s', mode %d\n", cstr, pData->inputProp);
CODE_STD_FINALIZERnewActInst
	free((void*)cstr);
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
