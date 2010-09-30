/* omhdfs.c
 * This is the implementation of the build-in file output module.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * Copyright 2010 Rainer Gerhards and Adiscon GmbH.
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

/* this is kind of a hack, if defined, it instructs omhdfs to use
 * the regular (non-hdfs) file system calls. This eases development 
 * (and hopefully troubleshooting) especially in cases when no
 * hdfs environment is available.
 */
#define USE_REGULAR_FS 1

#include "config.h"
#include "rsyslog.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/file.h>

#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "conf.h"
#include "cfsysline.h"
#include "module-template.h"
#include "unicode-helper.h"
#ifndef USE_REGULAR_FS
#  include "hdfs.h"
#endif

MODULE_TYPE_OUTPUT

/* internal structures
 */
DEF_OMOD_STATIC_DATA

/* globals for default values */
static uchar *fileName = NULL;	
/* end globals for default values */

typedef struct _instanceData {
	uchar	*fileName;
#	ifdef USE_REGULAR_FS
	short	fd;		  /* file descriptor for (current) file */
#	else
	hdfsFS fs;
	hdfsFile fd;
#	endif
} instanceData;


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURERepeatedMsgReduction)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	printf("omhdfs: file:%s", pData->fileName);
	if (pData->fd == -1)
		printf(" (unused)");
ENDdbgPrintInstInfo



#if 0
static void prepareFile(instanceData *pData, uchar *newFileName)
{
	if(access((char*)newFileName, F_OK) == 0) {
		/* file already exists */
		pData->fd = open((char*) newFileName, O_WRONLY|O_APPEND|O_CREAT|O_NOCTTY,
				pData->fCreateMode);
	} else {
		pData->fd = -1;
		/* file does not exist, create it (and eventually parent directories */
		if(pData->bCreateDirs) {
			/* we fist need to create parent dirs if they are missing
			 * We do not report any errors here ourselfs but let the code
			 * fall through to error handler below.
			 */
			if(makeFileParentDirs(newFileName, strlen((char*)newFileName),
			     pData->fDirCreateMode, pData->dirUID,
			     pData->dirGID, pData->bFailOnChown) != 0) {
			     	return; /* we give up */
			}
		}
		/* no matter if we needed to create directories or not, we now try to create
		 * the file. -- rgerhards, 2008-12-18 (based on patch from William Tisater)
		 */
		pData->fd = open((char*) newFileName, O_WRONLY|O_APPEND|O_CREAT|O_NOCTTY,
				pData->fCreateMode);
		if(pData->fd != -1) {
			/* check and set uid/gid */
			if(pData->fileUID != (uid_t)-1 || pData->fileGID != (gid_t) -1) {
				/* we need to set owner/group */
				if(fchown(pData->fd, pData->fileUID,
					  pData->fileGID) != 0) {
					if(pData->bFailOnChown) {
						int eSave = errno;
						close(pData->fd);
						pData->fd = -1;
						errno = eSave;
					}
					/* we will silently ignore the chown() failure
					 * if configured to do so.
					 */
				}
			}
		}
	}
}
#endif

static void prepareFile(instanceData *pData, uchar *newFileName)
{
#	if USE_REGULAR_FS
	pData->fd = open((char*) newFileName, O_WRONLY|O_APPEND|O_CREAT|O_NOCTTY, 0666);
#	else
	pData->fs = hdfsConnect("default", 0);
	pData->fd = hdfsOpenFile(fs, newFileName, O_WRONLY|O_CREAT, 0, 0, 0);
	if(!pData->fd) {
		dbgprintf(stderr, "Failed to open %s for writing!\n", newFileName);
	// TODO: suspend/error report
	}

#	endif
}

static rsRetVal writeFile(uchar **ppString, unsigned iMsgOpts, instanceData *pData)
{
	size_t lenWrite;
	DEFiRet;

#	if USE_REGULAR_FS
	if (write(pData->fd, ppString[0], strlen((char*)ppString[0])) < 0) {
		int e = errno;
		dbgprintf("omhdfs write error!\n");

		/* If the filesystem is filled up, just ignore
		 * it for now and continue writing when possible
		 */
		//if(pData->fileType == eTypeFILE && e == ENOSPC)
			//return RS_RET_OK;

		//(void) close(pData->fd);
		iRet = RS_RET_DISABLE_ACTION;
		errno = e;
		//logerror((char*) pData->f_fname);
	}
#	else
	lenWrite = strlen(char*ppString[0]);
	tSize num_written_bytes = hdfsWrite(pData->fs, pData->fd, ppString[0], lenWrite);
	if(num_written_bytes != lenWrite) {
		dbgprintf("Failed to write %s, expected %lu bytes, written %lu\n", pData->fileName,
			  lenWrite, num_written_bytes);
	// TODO: suspend/error report
	}
#	endif
else { dbgprintf("omhdfs has successfully written to file\n"); }

	RETiRet;
}


BEGINcreateInstance
CODESTARTcreateInstance
	pData->fd = -1;
ENDcreateInstance


BEGINfreeInstance
CODESTARTfreeInstance
#	ifdef USE_REGULAR_FS
	if(pData->fd != -1)
		close(pData->fd);
#	else
	hdfsCloseFile(pData->fs, pData->fd);
#	endif
ENDfreeInstance


BEGINtryResume
CODESTARTtryResume
ENDtryResume

BEGINdoAction
CODESTARTdoAction
	dbgprintf(" (%s)\n", pData->fileName);
	iRet = writeFile(ppString, iMsgOpts, pData);
ENDdoAction


BEGINparseSelectorAct
CODESTARTparseSelectorAct

	/* first check if this config line is actually for us */
	if(strncmp((char*) p, ":omhdfs:", sizeof(":omhdfs:") - 1)) {
		ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
	}

	/* ok, if we reach this point, we have something for us */
	p += sizeof(":omhdfs:") - 1; /* eat indicator sequence  (-1 because of '\0'!) */
	CHKiRet(createInstance(&pData));

	CODE_STD_STRING_REQUESTparseSelectorAct(1)
	/* rgerhards 2004-11-17: from now, we need to have different
	 * processing, because after the first comma, the template name
	 * to use is specified. So we need to scan for the first coma first
	 * and then look at the rest of the line.
	 */
	CHKiRet(cflineParseTemplateName(&p, *ppOMSR, 0, 0, (uchar*) "RSYSLOG_FileFormat"));
				       //(pszFileDfltTplName == NULL) ? (uchar*)"RSYSLOG_FileFormat" : pszFileDfltTplName));

	// TODO: check for NULL filename
	CHKmalloc(pData->fileName = ustrdup(fileName));

	prepareFile(pData, pData->fileName);
		
#if 0
	if ( pData->fd < 0 ){
		pData->fd = -1;
		dbgprintf("Error opening log file: %s\n", pData->f_fname);
		logerror((char*) pData->f_fname);
	}
#endif
CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


/* Reset config variables for this module to default values.
 * rgerhards, 2007-07-17
 */
static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
/*
	fileUID = -1;
	fileGID = -1;
	dirUID = -1;
	dirGID = -1;
	bFailOnChown = 1;
	iDynaFileCacheSize = 10;
	fCreateMode = 0644;
	fDirCreateMode = 0700;
	bCreateDirs = 1;
*/
	return RS_RET_OK;
}


BEGINmodExit
CODESTARTmodExit
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION;
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"omhdfsfilename", 0, eCmdHdlrGetWord, NULL, &fileName, NULL));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
CODEmodInit_QueryRegCFSLineHdlr
ENDmodInit
