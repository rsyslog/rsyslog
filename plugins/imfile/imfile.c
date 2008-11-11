/* imfile.c
 * 
 * This is the input module for reading text file data. A text file is a
 * non-binary file who's lines are delemited by the \n character.
 *
 * Work originally begun on 2008-02-01 by Rainer Gerhards
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
#include "config.h" /* this is for autotools and always must be the first include */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>		/* do NOT remove: will soon be done by the module generation macros */
#ifdef HAVE_SYS_STAT_H
#	include <sys/stat.h>
#endif
#include "rsyslog.h"		/* error codes etc... */
#include "dirty.h"
#include "cfsysline.h"		/* access to config file objects */
#include "module-template.h"	/* generic module interface code - very important, read it! */
#include "srUtils.h"		/* some utility functions */
#include "msg.h"
#include "stream.h"
#include "errmsg.h"
#include "glbl.h"
#include "datetime.h"

MODULE_TYPE_INPUT	/* must be present for input modules, do not remove */

/* defines */

/* Module static data */
DEF_IMOD_STATIC_DATA	/* must be present, starts static data */
DEFobjCurrIf(errmsg)
DEFobjCurrIf(glbl)
DEFobjCurrIf(datetime)

typedef struct fileInfo_s {
	uchar *pszFileName;
	uchar *pszTag;
	uchar *pszStateFile; /* file in which state between runs is to be stored */
	int iFacility;
	int iSeverity;
	strm_t *pStrm;	/* its stream (NULL if not assigned) */
} fileInfo_t;


/* config variables */
static uchar *pszFileName = NULL;
static uchar *pszFileTag = NULL;
static uchar *pszStateFile = NULL;
static int iPollInterval = 10;	/* number of seconds to sleep when there was no file activity */
static int iFacility = 128; /* local0 */
static int iSeverity = 5;  /* notice, as of rfc 3164 */

static int iFilPtr = 0;		/* number of files to be monitored; pointer to next free spot during config */
#define MAX_INPUT_FILES 100
static fileInfo_t files[MAX_INPUT_FILES];


/* enqueue the read file line as a message. The provided string is
 * not freed - thuis must be done by the caller.
 */
static rsRetVal enqLine(fileInfo_t *pInfo, cstr_t *cstrLine)
{
	DEFiRet;
	msg_t *pMsg;

	if(rsCStrLen(cstrLine) == 0) {
		/* we do not process empty lines */
		FINALIZE;
	}

	CHKiRet(msgConstruct(&pMsg));
	MsgSetFlowControlType(pMsg, eFLOWCTL_FULL_DELAY);
	MsgSetInputName(pMsg, "imfile");
	MsgSetUxTradMsg(pMsg, (char*)rsCStrGetSzStr(cstrLine));
	MsgSetRawMsg(pMsg, (char*)rsCStrGetSzStr(cstrLine));
	MsgSetMSG(pMsg, (char*)rsCStrGetSzStr(cstrLine));
	MsgSetHOSTNAME(pMsg, (char*)glbl.GetLocalHostName());
	MsgSetTAG(pMsg, (char*)pInfo->pszTag);
	pMsg->iFacility = LOG_FAC(pInfo->iFacility);
	pMsg->iSeverity = LOG_PRI(pInfo->iSeverity);
	pMsg->bParseHOSTNAME = 0;
	CHKiRet(submitMsg(pMsg));
finalize_it:
	RETiRet;
}


/* try to open a file. This involves checking if there is a status file and,
 * if so, reading it in. Processing continues from the last know location.
 */
static rsRetVal
openFile(fileInfo_t *pThis)
{
	DEFiRet;
	strm_t *psSF = NULL;
	uchar pszSFNam[MAXFNAME];
	size_t lenSFNam;
	struct stat stat_buf;

	/* Construct file name */
	lenSFNam = snprintf((char*)pszSFNam, sizeof(pszSFNam) / sizeof(uchar), "%s/%s",
			     (char*) glbl.GetWorkDir(), (char*)pThis->pszStateFile);

	/* check if the file exists */
	if(stat((char*) pszSFNam, &stat_buf) == -1) {
		if(errno == ENOENT) {
			/* currently no object! dbgoprint((obj_t*) pThis, "clean startup, no .si file found\n"); */
			ABORT_FINALIZE(RS_RET_FILE_NOT_FOUND);
		} else {
			/* currently no object! dbgoprint((obj_t*) pThis, "error %d trying to access .si file\n", errno); */
			ABORT_FINALIZE(RS_RET_IO_ERROR);
		}
	}

	/* If we reach this point, we have a .si file */

	CHKiRet(strmConstruct(&psSF));
	CHKiRet(strmSettOperationsMode(psSF, STREAMMODE_READ));
	CHKiRet(strmSetsType(psSF, STREAMTYPE_FILE_SINGLE));
	CHKiRet(strmSetFName(psSF, pszSFNam, lenSFNam));
	CHKiRet(strmConstructFinalize(psSF));

	/* read back in the object */
	CHKiRet(obj.Deserialize(&pThis->pStrm, (uchar*) "strm", psSF, NULL, pThis));

	CHKiRet(strmSeekCurrOffs(pThis->pStrm));

	/* OK, we could successfully read the file, so we now can request that it be deleted.
	 * If we need it again, it will be written on the next shutdown.
	 */
	psSF->bDeleteOnClose = 1;

finalize_it:
	if(psSF != NULL)
		strmDestruct(&psSF);

	if(iRet != RS_RET_OK) {
		CHKiRet(strmConstruct(&pThis->pStrm));
		CHKiRet(strmSettOperationsMode(pThis->pStrm, STREAMMODE_READ));
		CHKiRet(strmSetsType(pThis->pStrm, STREAMTYPE_FILE_MONITOR));
		CHKiRet(strmSetFName(pThis->pStrm, pThis->pszFileName, strlen((char*) pThis->pszFileName)));
		CHKiRet(strmConstructFinalize(pThis->pStrm));
	}

	RETiRet;
}


/* The following is a cancel cleanup handler for strmReadLine(). It is necessary in case
 * strmReadLine() is cancelled while processing the stream. -- rgerhards, 2008-03-27
 */
static void pollFileCancelCleanup(void *pArg)
{
	BEGINfunc;
	cstr_t **ppCStr = (cstr_t**) pArg;
	if(*ppCStr != NULL)
		rsCStrDestruct(ppCStr);
	ENDfunc;
}


/* poll a file, need to check file rollover etc. open file if not open */
#pragma GCC diagnostic ignored "-Wempty-body"
static rsRetVal pollFile(fileInfo_t *pThis, int *pbHadFileData)
{
	cstr_t *pCStr = NULL;
	DEFiRet;

	ASSERT(pbHadFileData != NULL);

	/* Note: we must do pthread_cleanup_push() immediately, because the POXIS macros
	 * otherwise do not work if I include the _cleanup_pop() inside an if... -- rgerhards, 2008-08-14
	 */
	pthread_cleanup_push(pollFileCancelCleanup, &pCStr);
	if(pThis->pStrm == NULL) {
		CHKiRet(openFile(pThis)); /* open file */
	}

	/* loop below will be exited when strmReadLine() returns EOF */
	while(1) {
		CHKiRet(strmReadLine(pThis->pStrm, &pCStr));
		*pbHadFileData = 1; /* this is just a flag, so set it and forget it */
		CHKiRet(enqLine(pThis, pCStr)); /* process line */
		rsCStrDestruct(&pCStr); /* discard string (must be done by us!) */
	}

finalize_it:
	/*EMPTY - just to keep the compiler happy, do NOT remove*/;
	/* Note: the problem above is that pthread:cleanup_pop() is a macro which
	 * evaluates to something like "} while(0);". So the code would become
	 * "finalize_it: }", that is a label without a statement. The C standard does
	 * not permit this. So we add an empty statement "finalize_it: ; }" and
	 * everybody is happy. Note that without the ;, an error is reported only
	 * on some platforms/compiler versions. -- rgerhards, 2008-08-15
	 */
	pthread_cleanup_pop(0);

	if(pCStr != NULL) {
		rsCStrDestruct(&pCStr);
	}

	RETiRet;
}
#pragma GCC diagnostic warning "-Wempty-body"


/* This function is the cancel cleanup handler. It is called when rsyslog decides the
 * module must be stopped, what most probably happens during shutdown of rsyslogd. When
 * this function is called, the runInput() function (below) is already terminated - somewhere
 * in the middle of what it was doing. The cancel cleanup handler below should take
 * care of any locked mutexes and such, things that really need to be cleaned up
 * before processing continues. In general, many plugins do not need to provide
 * any code at all here.
 *
 * IMPORTANT: the calling interface of this function can NOT be modified. It actually is
 * called by pthreads. The provided argument is currently not being used.
 */
/* ------------------------------------------------------------------------------------------ *
 * DO NOT TOUCH the following code - it will soon be part of the module generation macros!    */
static void
inputModuleCleanup(void __attribute__((unused)) *arg)
{
	BEGINfunc
/* END no-touch zone                                                                          *
 * ------------------------------------------------------------------------------------------ */



	/* so far not needed */



/* ------------------------------------------------------------------------------------------ *
 * DO NOT TOUCH the following code - it will soon be part of the module generation macros!    */
	ENDfunc
}
/* END no-touch zone                                                                          *
 * ------------------------------------------------------------------------------------------ */


/* This function is called by the framework to gather the input. The module stays
 * most of its lifetime inside this function. It MUST NEVER exit this function. Doing
 * so would end module processing and rsyslog would NOT reschedule the module. If
 * you exit from this function, you violate the interface specification!
 *
 * We go through all files and remember if at least one had data. If so, we do
 * another run (until no data was present in any file). Then we sleep for
 * PollInterval seconds and restart the whole process. This ensures that as
 * long as there is some data present, it will be processed at the fastest
 * possible pace - probably important for busy systmes. If we monitor just a
 * single file, the algorithm is slightly modified. In that case, the sleep
 * hapens immediately. The idea here is that if we have just one file, we
 * returned from the file processer because that file had no additional data.
 * So even if we found some lines, it is highly unlikely to find a new one
 * just now. Trying it would result in a performance-costly additional try
 * which in the very, very vast majority of cases will never find any new
 * lines. 
 * On spamming the main queue: keep in mind that it will automatically rate-limit
 * ourselfes if we begin to overrun it. So we really do not need to care here.
 */
#pragma GCC diagnostic ignored "-Wempty-body"
BEGINrunInput
	int i;
	int bHadFileData; /* were there at least one file with data during this run? */
CODESTARTrunInput
	/* ------------------------------------------------------------------------------------------ *
	 * DO NOT TOUCH the following code - it will soon be part of the module generation macros!    */
	pthread_cleanup_push(inputModuleCleanup, NULL);
	while(1) { /* endless loop - do NOT break; out of it! */
	/* END no-touch zone                                                                          *
	 * ------------------------------------------------------------------------------------------ */

	do {
		bHadFileData = 0;
		for(i = 0 ; i < iFilPtr ; ++i) {
			pollFile(&files[i], &bHadFileData);
		}
	} while(iFilPtr > 1 && bHadFileData == 1); /* waring: do...while()! */

	/* Note: the additional 10ns wait is vitally important. It guards rsyslog against totally
	 * hogging the CPU if the users selects a polling interval of 0 seconds. It doesn't hurt any
	 * other valid scenario. So do not remove. -- rgerhards, 2008-02-14
	 */
	srSleep(iPollInterval, 10);

	/* ------------------------------------------------------------------------------------------ *
	 * DO NOT TOUCH the following code - it will soon be part of the module generation macros!    */
	}
	/*NOTREACHED*/
	
	pthread_cleanup_pop(0); /* just for completeness, but never called... */
	RETiRet;	/* use it to make sure the housekeeping is done! */
ENDrunInput
#pragma GCC diagnostic warning "-Wempty-body"
	/* END no-touch zone                                                                          *
	 * ------------------------------------------------------------------------------------------ */


/* The function is called by rsyslog before runInput() is called. It is a last chance
 * to set up anything specific. Most importantly, it can be used to tell rsyslog if the
 * input shall run or not. The idea is that if some config settings (or similiar things)
 * are not OK, the input can tell rsyslog it will not execute. To do so, return
 * RS_RET_NO_RUN or a specific error code. If RS_RET_OK is returned, rsyslog will
 * proceed and call the runInput() entry point.
 */
BEGINwillRun
CODESTARTwillRun
	if(iFilPtr == 0) {
		errmsg.LogError(0, RS_RET_NO_RUN, "No files configured to be monitored");
		ABORT_FINALIZE(RS_RET_NO_RUN);
	}

finalize_it:
ENDwillRun



/* This function persists information for a specific file being monitored.
 * To do so, it simply persists the stream object. We do NOT abort on error
 * iRet as that makes matters worse (at least we can try persisting the others...).
 * rgerhards, 2008-02-13
 */
static rsRetVal
persistStrmState(fileInfo_t *pInfo)
{
	DEFiRet;
	strm_t *psSF = NULL; /* state file (stream) */

	ASSERT(pInfo != NULL);

	/* TODO: create a function persistObj in obj.c? */
	CHKiRet(strmConstruct(&psSF));
	CHKiRet(strmSetDir(psSF, glbl.GetWorkDir(), strlen((char*)glbl.GetWorkDir())));
	CHKiRet(strmSettOperationsMode(psSF, STREAMMODE_WRITE));
	CHKiRet(strmSetiAddtlOpenFlags(psSF, O_TRUNC));
	CHKiRet(strmSetsType(psSF, STREAMTYPE_FILE_SINGLE));
	CHKiRet(strmSetFName(psSF, pInfo->pszStateFile, strlen((char*) pInfo->pszStateFile)));
	CHKiRet(strmConstructFinalize(psSF));

	CHKiRet(strmSerialize(pInfo->pStrm, psSF));

	CHKiRet(strmDestruct(&psSF));

finalize_it:
	if(psSF != NULL)
		strmDestruct(&psSF);

	RETiRet;
}


/* This function is called by the framework after runInput() has been terminated. It
 * shall free any resources and prepare the module for unload.
 */
BEGINafterRun
	int i;
CODESTARTafterRun
	/* Close files and persist file state information. We do NOT abort on error iRet as that makes
	 * matters worse (at least we can try persisting the others...). Please note that, under stress
	 * conditions, it may happen that we are terminated before we actuall could open all streams. So
	 * before we change anything, we need to make sure the stream was open.
	 */
	for(i = 0 ; i < iFilPtr ; ++i) {
		if(files[i].pStrm != NULL) { /* stream open? */
			persistStrmState(&files[i]);
			strmDestruct(&(files[i].pStrm));
		}
	}
ENDafterRun


/* The following entry points are defined in module-template.h.
 * In general, they need to be present, but you do NOT need to provide
 * any code here.
 */
BEGINmodExit
CODESTARTmodExit
	/* release objects we used */
	objRelease(datetime, CORE_COMPONENT);
	objRelease(glbl, CORE_COMPONENT);
	objRelease(errmsg, CORE_COMPONENT);
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_IMOD_QUERIES
ENDqueryEtryPt


/* The following function shall reset all configuration variables to their
 * default values. The code provided in modInit() below registers it to be
 * called on "$ResetConfigVariables". You may also call it from other places,
 * but in general this is not necessary. Once runInput() has been called, this
 * function here is never again called.
 */
static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	DEFiRet;

	if(pszFileName != NULL) {
		free(pszFileName);
		pszFileName = NULL;
	}

	if(pszFileTag != NULL) {
		free(pszFileTag);
		pszFileTag = NULL;
	}

	if(pszStateFile != NULL) {
		free(pszFileTag);
		pszFileTag = NULL;
	}


	/* set defaults... */
	iPollInterval = 10;
	iFacility = 128; /* local0 */
	iSeverity = 5;  /* notice, as of rfc 3164 */

	RETiRet;
}


/* add a new monitor */
static rsRetVal addMonitor(void __attribute__((unused)) *pVal, uchar *pNewVal)
{
	DEFiRet;
	fileInfo_t *pThis;

	free(pNewVal); /* we do not need it, but we must free it! */

	if(iFilPtr < MAX_INPUT_FILES) {
		pThis = &files[iFilPtr];
		/* TODO: check for strdup() NULL return */
		if(pszFileName == NULL) {
			errmsg.LogError(0, RS_RET_CONFIG_ERROR, "imfile error: no file name given, file monitor can not be created");
			ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
		} else {
			pThis->pszFileName = (uchar*) strdup((char*) pszFileName);
		}

		if(pszFileTag == NULL) {
			errmsg.LogError(0, RS_RET_CONFIG_ERROR, "imfile error: no tag value given , file monitor can not be created");
			ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
		} else {
			pThis->pszTag = (uchar*) strdup((char*) pszFileTag);
		}

		if(pszStateFile == NULL) {
			errmsg.LogError(0, RS_RET_CONFIG_ERROR, "imfile error: not state file name given, file monitor can not be created");
			ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
		} else {
			pThis->pszStateFile = (uchar*) strdup((char*) pszStateFile);
		}

		pThis->iSeverity = iSeverity;
		pThis->iFacility = iFacility;
	} else {
		errmsg.LogError(0, RS_RET_OUT_OF_DESRIPTORS, "Too many file monitors configured - ignoring this one");
		ABORT_FINALIZE(RS_RET_OUT_OF_DESRIPTORS);
	}

	CHKiRet(resetConfigVariables((uchar*) "dummy", (void*) pThis)); /* values are both dummies */

finalize_it:
	if(iRet == RS_RET_OK)
		++iFilPtr;	/* we got a new file to monitor */

	RETiRet;
}

/* modInit() is called once the module is loaded. It must perform all module-wide
 * initialization tasks. There are also a number of housekeeping tasks that the
 * framework requires. These are handled by the macros. Please note that the
 * complexity of processing is depending on the actual module. However, only
 * thing absolutely necessary should be done here. Actual app-level processing
 * is to be performed in runInput(). A good sample of what to do here may be to
 * set some variable defaults. The most important thing probably is registration
 * of config command handlers.
 */
BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(datetime, CORE_COMPONENT));

	CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputfilename", 0, eCmdHdlrGetWord,
	  	NULL, &pszFileName, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputfiletag", 0, eCmdHdlrGetWord,
	  	NULL, &pszFileTag, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputfilestatefile", 0, eCmdHdlrGetWord,
	  	NULL, &pszStateFile, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputfileseverity", 0, eCmdHdlrSeverity,
	  	NULL, &iSeverity, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputfilefacility", 0, eCmdHdlrFacility,
	  	NULL, &iFacility, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputfilepollinterval", 0, eCmdHdlrInt,
	  	NULL, &iPollInterval, STD_LOADABLE_MODULE_ID));
	/* that command ads a new file! */
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputrunfilemonitor", 0, eCmdHdlrGetWord,
		addMonitor, NULL, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler,
		resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit
/* vim:set ai:
 */
