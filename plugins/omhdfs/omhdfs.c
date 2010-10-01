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
#include <pthread.h>
#include <hdfs.h>

#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "conf.h"
#include "cfsysline.h"
#include "module-template.h"
#include "unicode-helper.h"
#include "errmsg.h"

MODULE_TYPE_OUTPUT

/* internal structures
 */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(errmsg)

/* globals for default values */
static uchar *fileName = NULL;	
static uchar *hdfsHost = NULL;	
int hdfsPort = 0;
/* end globals for default values */

typedef struct {
	uchar	*name;
	hdfsFS fs;
	hdfsFile fh;
	const char *hdfsHost;
	tPort hdfsPort;
	int nUsers;
	pthread_mutex_t mut;
} file_t;


typedef struct _instanceData {
	file_t *pFile;
} instanceData;

/* forward definitions (down here, need data types) */
static inline rsRetVal fileClose(file_t *pFile);

BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURERepeatedMsgReduction)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	printf("omhdfs: file:%s", pData->pFile->name);
ENDdbgPrintInstInfo



#if 0
static void prepareFile(instanceData *pData, uchar *newFileName)
{
	if(access((char*)newFileName, F_OK) == 0) {
		/* file already exists */
		pData->fh = open((char*) newFileName, O_WRONLY|O_APPEND|O_CREAT|O_NOCTTY,
				pData->fCreateMode);
	} else {
		pData->fh = -1;
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
		pData->fh = open((char*) newFileName, O_WRONLY|O_APPEND|O_CREAT|O_NOCTTY,
				pData->fCreateMode);
		if(pData->fh != -1) {
			/* check and set uid/gid */
			if(pData->fileUID != (uid_t)-1 || pData->fileGID != (gid_t) -1) {
				/* we need to set owner/group */
				if(fchown(pData->fh, pData->fileUID,
					  pData->fileGID) != 0) {
					if(pData->bFailOnChown) {
						int eSave = errno;
						close(pData->fh);
						pData->fh = -1;
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

/* ---BEGIN FILE OBJECT---------------------------------------------------- */
/* This code handles the "file object". This is split from the actual
 * instance data, because several instances may write into the same file.
 * If so, we need to use a single object, and also synchronize their writes.
 * So we keep the file object separately, and just stick a reference into
 * the instance data.
 */

static inline rsRetVal
fileObjConstruct(file_t **ppFile)
{
	file_t *pFile;
	DEFiRet;

	CHKmalloc(pFile = malloc(sizeof(file_t)));
	pFile->name = NULL;
	pFile->hdfsHost = NULL;
	pFile->fh = NULL;
	pFile->nUsers = 0;

	*ppFile = pFile;
finalize_it:
	RETiRet;
}

static inline void
fileObjAddUser(file_t *pFile)
{
	/* init mutex only when second user is added */
	++pFile->nUsers;
	if(pFile->nUsers == 2)
		pthread_mutex_init(&pFile->mut, NULL);
}

static inline rsRetVal
fileObjDestruct(file_t **ppFile)
{
	file_t *pFile = *ppFile;
	if(pFile->nUsers > 1)
		pthread_mutex_destroy(&pFile->mut);
	fileClose(pFile);
	free(pFile->name);
	free((char*)pFile->hdfsHost);
	free(pFile->fh);

	return RS_RET_OK;
}

static inline rsRetVal
fileOpen(file_t *pFile)
{
	DEFiRet;

	assert(pFile->fh == NULL);
	if(pFile->nUsers > 1)
		d_pthread_mutex_lock(&pFile->mut);
	DBGPRINTF("omhdfs: try to connect to HDFS at host '%s', port %d\n",
		  pFile->hdfsHost, pFile->hdfsPort);
	pFile->fs = hdfsConnect(pFile->hdfsHost, pFile->hdfsPort);
	if(pFile->fs == NULL) {
		DBGPRINTF("omhdfs: error can not connect to hdfs\n");
		ABORT_FINALIZE(RS_RET_SUSPENDED);
	}
	pFile->fh = hdfsOpenFile(pFile->fs, (char*)pFile->name, O_WRONLY|O_APPEND, 0, 0, 0);
	if(pFile->fh == NULL) {
		/* maybe the file does not exist, so we try to create it now.
		 * Note that we can not use hdfsExists() because of a deficit in
		 * it: https://issues.apache.org/jira/browse/HDFS-1154
		 * As of my testing, libhdfs at least seems to return ENOENT if
		 * the file does not exist.
		 */
		if(errno == ENOENT) {
			DBGPRINTF("omhdfs: ENOENT trying to append to '%s', now trying create\n",
				  pFile->name);
		 	pFile->fh = hdfsOpenFile(pFile->fs, (char*)pFile->name, O_WRONLY|O_CREAT, 0, 0, 0);
		}
	}
	if(pFile->fh == NULL) {
		DBGPRINTF("omhdfs: failed to open %s for writing!\n", pFile->name);
		ABORT_FINALIZE(RS_RET_SUSPENDED);
	}

finalize_it:
	if(pFile->nUsers > 1)
		d_pthread_mutex_unlock(&pFile->mut);
	RETiRet;
}


static inline rsRetVal
fileWrite(file_t *pFile, uchar *buf)
{
	size_t lenWrite;
	DEFiRet;

	assert(pFile->fh != NULL);
	if(pFile->nUsers > 1)
		d_pthread_mutex_lock(&pFile->mut);
	lenWrite = strlen((char*) buf);
	tSize num_written_bytes = hdfsWrite(pFile->fs, pFile->fh, buf, lenWrite);
	if((unsigned) num_written_bytes != lenWrite) {
		errmsg.LogError(errno, RS_RET_ERR_HDFS_WRITE, "omhdfs: failed to write %s, expected %lu bytes, "
			        "written %lu\n", pFile->name, lenWrite, (unsigned long) num_written_bytes);
		ABORT_FINALIZE(RS_RET_SUSPENDED);
	}

finalize_it:
	if(pFile->nUsers > 1)
		d_pthread_mutex_unlock(&pFile->mut);
	RETiRet;
}


static inline rsRetVal
fileClose(file_t *pFile)
{
	if(pFile->nUsers > 1)
		d_pthread_mutex_lock(&pFile->mut);
	if(pFile->fh != NULL) {
		hdfsCloseFile(pFile->fs, pFile->fh);
		pFile->fh = NULL;
	}

	if(pFile->nUsers > 1)
		d_pthread_mutex_unlock(&pFile->mut);

	return RS_RET_OK;
}

/* ---END FILE OBJECT---------------------------------------------------- */


BEGINcreateInstance
CODESTARTcreateInstance
	pData->pFile = NULL;
ENDcreateInstance


BEGINfreeInstance
CODESTARTfreeInstance
	fileObjDestruct(&pData->pFile);
ENDfreeInstance


BEGINtryResume
CODESTARTtryResume
ENDtryResume

BEGINdoAction
CODESTARTdoAction
	DBGPRINTF(" (%s)\n", pData->pFile->name);
	iRet = fileWrite(pData->pFile, ppString[0]);
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
	CHKiRet(cflineParseTemplateName(&p, *ppOMSR, 0, 0, (uchar*) "RSYSLOG_FileFormat"));
				       //(pszFileDfltTplName == NULL) ? (uchar*)"RSYSLOG_FileFormat" : pszFileDfltTplName));

	if(fileName == NULL) {
		errmsg.LogError(0, RS_RET_ERR_HDFS_OPEN, "omhdfs: no file name specified, can not continue");
		ABORT_FINALIZE(RS_RET_FILE_NOT_SPECIFIED);
	}

	CHKiRet(fileObjConstruct(&pData->pFile));
	CHKmalloc(pData->pFile->name = (uchar*)strdup((char*)fileName));
	if(hdfsHost == NULL) {
		CHKmalloc(pData->pFile->hdfsHost = strdup("default"));
	} else {
		CHKmalloc(pData->pFile->hdfsHost = strdup((char*)hdfsHost));
	}
	pData->pFile->hdfsPort = hdfsPort;

	fileOpen(pData->pFile);
		
	if(pData->pFile->fh == NULL){
		errmsg.LogError(0, RS_RET_ERR_HDFS_OPEN, "omhdfs: failed to open %s - retrying later", pData->pFile->name);
		iRet = RS_RET_SUSPENDED;
	}
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
	hdfsHost = NULL;
	hdfsPort = 0;
	return RS_RET_OK;
}


BEGINmodExit
CODESTARTmodExit
	objRelease(errmsg, CORE_COMPONENT);
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION;
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(errmsg, CORE_COMPONENT));

	CHKiRet(omsdRegCFSLineHdlr((uchar *)"omhdfsfilename", 0, eCmdHdlrGetWord, NULL, &fileName, NULL));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"omhdfshost", 0, eCmdHdlrGetWord, NULL, &hdfsHost, NULL));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"omhdfsport", 0, eCmdHdlrInt, NULL, &hdfsPort, NULL));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
CODEmodInit_QueryRegCFSLineHdlr
ENDmodInit
