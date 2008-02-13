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
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>		/* do NOT remove: will soon be done by the module generation macros */
#include "rsyslog.h"		/* error codes etc... */
#include "syslogd.h"
#include "cfsysline.h"		/* access to config file objects */
#include "module-template.h"	/* generic module interface code - very important, read it! */
#include "srUtils.h"		/* some utility functions */
#include "msg.h"

MODULE_TYPE_INPUT	/* must be present for input modules, do not remove */

/* defines */

/* Module static data */
DEF_IMOD_STATIC_DATA	/* must be present, starts static data */

/* Here, define whatever static data is needed. Is it suggested that static variables only are
 * used (not externally visible). If you need externally visible variables, make sure you use a
 * prefix in order not to conflict with other modules or rsyslogd itself (also see comment
 * at file header).
 */
/* static int imtemplateWhateverVar = 0; */

typedef struct fileInfo_s {
	uchar *pszFileName;
	uchar *pszTag;
	uchar *pszStateFile; /* file in which state between runs is to be stored */
	int64 offsLast; /* offset last read from */
	int iFacility;
	int iSeverity;
	int fd; /*its file descriptor (-1 if closed) */
} fileInfo_t;


/* config variables */
static uchar *pszFileName = NULL;
static uchar *pszFileTag = NULL;
static uchar *pszStateFile = NULL;
static int iFacility;
static int iSeverity;

static int iFilPtr = 0;
#define MAX_INPUT_FILES 100
static fileInfo_t files[MAX_INPUT_FILES];

/* instanceData must be defined to keep the framework happy, but it currently
 * is of no practical use. This may change in later revisions of the plugin
 * interface.
 */
typedef struct _instanceData {
} instanceData;

/* config settings */


/* enqueue the read file line as a message
 */
static rsRetVal enqLine(fileInfo_t *pInfo, uchar *pLine)
{
		DEFiRet;
		msg_t *pMsg;
		int flags = 0;
		int pri;

		CHKiRet(msgConstruct(&pMsg));
		MsgSetUxTradMsg(pMsg, (char*)pLine);
		MsgSetRawMsg(pMsg, (char*)pLine);
		MsgSetHOSTNAME(pMsg, LocalHostName);
		MsgSetTAG(pMsg, (char*)pInfo->pszTag);
		pMsg->iFacility = pInfo->iFacility;
		pMsg->iSeverity = pInfo->iSeverity;
		pMsg->bParseHOSTNAME = 0;
		getCurrTime(&(pMsg->tTIMESTAMP)); /* use the current time! */
		logmsg(pMsg, flags); /* some time, CHKiRet() will work here, too [today NOT!] */
finalize_it:
	RETiRet;
}


/* poll a file, need to check file rollover etc. open file if not open */
static rsRetVal pollFile(fileInfo_t *pThis)
{
	DEFiRet;
	uchar *pszLine;	
	int bAllNewLinesRead; /* set to 1 if all new lines are read */

	if(pThis->fd == -1) {
		/* open file */
		/* move to offset */
	}

	bAllNewLinesRead = 0;
	while(!bAllNewLinesRead) {
		/* do read file, put pointer to file line in pszLine */

		/* do the magic ;) */
		CHKiRet(enqLine(pThis, pszLine));
	}

	/* save the offset back to structure! */

finalize_it:
	RETiRet;
}


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
 * So how is it terminated? When it is time to terminate, rsyslog actually cancels
 * the threads. This may sound scary, but is not. There is a cancel cleanup handler
 * defined (the function directly above). See comments there for specifics.
 *
 * runInput is always called on a single thread. If the module neees multiple threads,
 * it is free to create them. HOWEVER, it must make sure that any threads created
 * are killed and joined in the cancel cleanup handler.
 */
BEGINrunInput
	int i;
CODESTARTrunInput
	/* ------------------------------------------------------------------------------------------ *
	 * DO NOT TOUCH the following code - it will soon be part of the module generation macros!    */
	pthread_cleanup_push(inputModuleCleanup, NULL);
	while(1) { /* endless loop - do NOT break; out of it! */
	/* END no-touch zone                                                                          *
	 * ------------------------------------------------------------------------------------------ */

	for(i = 0 ; i < iFilPtr ; ++i) {
		pollFile(&files[i]);
	}

	srSleep(1,0);

	/* ------------------------------------------------------------------------------------------ *
	 * DO NOT TOUCH the following code - it will soon be part of the module generation macros!    */
	}
	/*NOTREACHED*/
	
	pthread_cleanup_pop(0); /* just for completeness, but never called... */
	RETiRet;	/* use it to make sure the housekeeping is done! */
ENDrunInput
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
		logerror("No files configured to be monitored");
		ABORT_FINALIZE(RS_RET_NO_RUN);
	}

finalize_it:
ENDwillRun


/* This function is called by the framework after runInput() has been terminated. It
 * shall free any resources and prepare the module for unload.
 *
 * So it is important that runInput() keeps track of what needs to be cleaned up.
 * Objects to think about are files (must be closed), network connections, threads (must
 * be stopped and joined) and memory (must be freed). Of course, there are a myriad
 * of other things, so use your own judgement what you need to do.
 *
 * Another important chore of this function is to persist whatever state the module
 * needs to persist. Unfortunately, there is currently no standard way of doing that.
 * Future version of the module interface will probably support it, but that doesn't
 * help you right at the moment. In general, it is suggested that anything that needs
 * to be persisted is saved in a file, whose name and location is passed in by a
 * module-specific config directive.
 */
BEGINafterRun
	/* place any variables needed here */
CODESTARTafterRun
	/* loop through file array and close everything that's open */

	/* somehow persist the file arry information, at least the offset! Must be 
	 * able to get back up an rolling even when the order of files inside the
	 * array changes (think of config changes!).
	 */
ENDafterRun


/* The following entry points are defined in module-template.h.
 * In general, they need to be present, but you do NOT need to provide
 * any code here.
 */
BEGINfreeInstance
CODESTARTfreeInstance
ENDfreeInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
ENDdbgPrintInstInfo


BEGINmodExit
CODESTARTmodExit
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
	iFacility = 12; /* see RFC 3164 for values */
	iSeverity = 4;

	RETiRet;
}


/* add a new monitor */
static rsRetVal addMonitor(void __attribute__((unused)) *pVal, uchar *pNewVal)
{
	DEFiRet;
	fileInfo_t *pThis;

	if(iFilPtr < MAX_INPUT_FILES) {
		pThis = &files[iFilPtr];
		++iFilPtr;
		pThis->pszFileName = (uchar*) strdup((char*) pszFileName);
		pThis->pszTag = (uchar*) strdup((char*) pszFileTag);
		pThis->iSeverity = iSeverity;
		pThis->iFacility = iFacility;
		pThis->offsLast = 0;
	} else {
		logerror("Too many file monitors configured - ignoring this one");
	}
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
	*ipIFVersProvided = 1; /* interface spec version this module is written to (currently always 1) */
CODEmodInit_QueryRegCFSLineHdlr
	 CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputfilename", 0, eCmdHdlrGetWord,
	  	NULL, &pszFileName, STD_LOADABLE_MODULE_ID));
	 CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputfiletag", 0, eCmdHdlrGetWord,
	  	NULL, &pszFileTag, STD_LOADABLE_MODULE_ID));
	 CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputfilestatefile", 0, eCmdHdlrGetWord,
	  	NULL, &pszStateFile, STD_LOADABLE_MODULE_ID));
	 /* use numerical values as of RFC 3164 for the time being... */
	 CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputfileseverity", 0, eCmdHdlrInt,
	  	NULL, &iSeverity, STD_LOADABLE_MODULE_ID));
	 CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputfilesfacility", 0, eCmdHdlrInt,
	  	NULL, &iFacility, STD_LOADABLE_MODULE_ID));
	/* things missing, e.g. polling intervall... */
	/* that command ads a new file! */
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputrunfilemonitor", 0, eCmdHdlrGetWord,
		addMonitor, NULL, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler,
		resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit
/*
 * vim:set ai:
 */
