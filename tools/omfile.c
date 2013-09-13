/* omfile.c
 * This is the implementation of the build-in file output module.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * File begun on 2007-07-21 by RGerhards (extracted from syslogd.c, which
 * at the time of the fork from sysklogd was under BSD license)
 *
 * A large re-write of this file was done in June, 2009. The focus was
 * to introduce many more features (like zipped writing), clean up the code
 * and make it more reliable. In short, that rewrite tries to provide a new
 * solid basis for the next three to five years to come. During it, bugs
 * may have been introduced ;) -- rgerhards, 2009-06-04
 *
 * Note that as of 2010-02-28 this module does no longer handle
 * pipes. These have been moved to ompipe, to reduced the entanglement
 * between the two different functionalities. -- rgerhards
 *
 * Copyright 2007-2013 Adiscon GmbH.
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
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <libgen.h>
#include <unistd.h>
#include <sys/file.h>
#ifdef OS_SOLARIS
#	include <fcntl.h>
#endif
#ifdef HAVE_ATOMIC_BUILTINS
#	include <pthread.h>
#endif


#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "outchannel.h"
#include "omfile.h"
#include "cfsysline.h"
#include "module-template.h"
#include "errmsg.h"
#include "stream.h"
#include "unicode-helper.h"
#include "atomic.h"
#include "statsobj.h"
#include "sigprov.h"
#include "cryprov.h"

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("omfile")

/* forward definitions */
static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal);

/* internal structures
 */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(errmsg)
DEFobjCurrIf(strm)
DEFobjCurrIf(statsobj)

/* for our current LRU mechanism, we need a monotonically increasing counters. We use
 * it much like a "Lamport logical clock": we do not need the actual time, we just need
 * to know the sequence in which files were accessed. So we use a simple counter to
 * create that sequence. We use an unsigned 64 bit value which is extremely unlike to
 * wrap within the lifetime of a process. If we process 1,000,000 file writes per
 * second, the process could still exist over 500,000 years before a wrap to 0 happens.
 * That should be sufficient (and even than, there would no really bad effect ;)).
 * The variable below is the global counter/clock.
 */
#if HAVE_ATOMIC_BUILTINS_64BIT
static uint64 clockFileAccess = 0;
#else
static unsigned clockFileAccess = 0;
#endif
/* and the "tick" function */
#ifndef HAVE_ATOMIC_BUILTINS
static pthread_mutex_t mutClock;
#endif
static inline uint64
getClockFileAccess(void)
{
#if HAVE_ATOMIC_BUILTINS_64BIT
	return ATOMIC_INC_AND_FETCH_uint64(&clockFileAccess, &mutClock);
#else
	return ATOMIC_INC_AND_FETCH_unsigned(&clockFileAccess, &mutClock);
#endif
}


/* The following structure is a dynafile name cache entry.
 */
struct s_dynaFileCacheEntry {
	uchar *pName;		/* name currently open, if dynamic name */
	strm_t	*pStrm;		/* our output stream */
	void	*sigprovFileData;	/* opaque data ptr for provider use */
	uint64	clkTickAccessed;/* for LRU - based on clockFileAccess */
};
typedef struct s_dynaFileCacheEntry dynaFileCacheEntry;


#define IOBUF_DFLT_SIZE 4096	/* default size for io buffers */
#define FLUSH_INTRVL_DFLT 1 	/* default buffer flush interval (in seconds) */
#define USE_ASYNCWRITER_DFLT 0 	/* default buffer use async writer */
#define FLUSHONTX_DFLT 1 	/* default for flush on TX end */


typedef struct _instanceData {
	uchar	*f_fname;	/* file or template name (display only) */
	uchar 	*tplName;	/* name of assigned template */
	strm_t	*pStrm;		/* our output stream */
	char	bDynamicName;	/* 0 - static name, 1 - dynamic name (with properties) */
	int	fCreateMode;	/* file creation mode for open() */
	int	fDirCreateMode;	/* creation mode for mkdir() */
	int	bCreateDirs;	/* auto-create directories? */
	int	bSyncFile;	/* should the file by sync()'ed? 1- yes, 0- no */
	uid_t	fileUID;	/* IDs for creation */
	uid_t	dirUID;
	gid_t	fileGID;
	gid_t	dirGID;
	int	bFailOnChown;	/* fail creation if chown fails? */
	uchar 	*sigprovName;	/* signature provider */
	uchar 	*sigprovNameFull;/* full internal signature provider name */
	sigprov_if_t sigprov;	/* ptr to signature provider interface */
	void	*sigprovData;	/* opaque data ptr for provider use */
	void 	*sigprovFileData;/* opaque data ptr for file instance */
	sbool	useSigprov;	/* quicker than checkig ptr (1 vs 8 bytes!) */
	uchar 	*cryprovName;	/* crypto provider */
	uchar 	*cryprovNameFull;/* full internal crypto provider name */
	void	*cryprovData;	/* opaque data ptr for provider use */
	void 	*cryprovFileData;/* opaque data ptr for file instance */
	cryprov_if_t cryprov;	/* ptr to crypto provider interface */
	sbool	useCryprov;	/* quicker than checkig ptr (1 vs 8 bytes!) */
	int	iCurrElt;	/* currently active cache element (-1 = none) */
	int	iCurrCacheSize;	/* currently cache size (1-based) */
	int	iDynaFileCacheSize; /* size of file handle cache */
	/* The cache is implemented as an array. An empty element is indicated
	 * by a NULL pointer. Memory is allocated as needed. The following
	 * pointer points to the overall structure.
	 */
	dynaFileCacheEntry **dynCache;
	off_t	iSizeLimit;		/* file size limit, 0 = no limit */
	uchar	*pszSizeLimitCmd;	/* command to carry out when size limit is reached */
	int 	iZipLevel;		/* zip mode to use for this selector */
	int	iIOBufSize;		/* size of associated io buffer */
	int	iFlushInterval;		/* how fast flush buffer on inactivity? */
	sbool	bFlushOnTXEnd;		/* flush write buffers when transaction has ended? */
	sbool	bUseAsyncWriter;	/* use async stream writer? */
	sbool	bVeryRobustZip;
	statsobj_t *stats;		/* dynafile, primarily cache stats */
	STATSCOUNTER_DEF(ctrRequests, mutCtrRequests);
	STATSCOUNTER_DEF(ctrLevel0, mutCtrLevel0);
	STATSCOUNTER_DEF(ctrEvict, mutCtrEvict);
	STATSCOUNTER_DEF(ctrMiss, mutCtrMiss);
	STATSCOUNTER_DEF(ctrMax, mutCtrMax);
} instanceData;


typedef struct configSettings_s {
	int iDynaFileCacheSize; /* max cache for dynamic files */
	int fCreateMode; /* mode to use when creating files */
	int fDirCreateMode; /* mode to use when creating files */
	int	bFailOnChown;	/* fail if chown fails? */
	uid_t	fileUID;	/* UID to be used for newly created files */
	uid_t	fileGID;	/* GID to be used for newly created files */
	uid_t	dirUID;		/* UID to be used for newly created directories */
	uid_t	dirGID;		/* GID to be used for newly created directories */
	int	bCreateDirs;/* auto-create directories for dynaFiles: 0 - no, 1 - yes */
	int	bEnableSync;/* enable syncing of files (no dash in front of pathname in conf): 0 - no, 1 - yes */
	int	iZipLevel;	/* zip compression mode (0..9 as usual) */
	sbool	bFlushOnTXEnd;/* flush write buffers when transaction has ended? */
	int64	iIOBufSize;	/* size of an io buffer */
	int	iFlushInterval; 	/* how often flush the output buffer on inactivity? */
	int	bUseAsyncWriter;	/* should we enable asynchronous writing? */
	EMPTY_STRUCT
} configSettings_t;
static configSettings_t cs;
uchar	*pszFileDfltTplName; /* name of the default template to use */

struct modConfData_s {
	rsconf_t *pConf;	/* our overall config object */
	uchar 	*tplName;	/* default template */
};

static modConfData_t *loadModConf = NULL;/* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL;/* modConf ptr to use for the current exec process */

/* tables for interfacing with the v6 config system */
/* module-global parameters */
static struct cnfparamdescr modpdescr[] = {
	{ "template", eCmdHdlrGetWord, 0 },
};
static struct cnfparamblk modpblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(modpdescr)/sizeof(struct cnfparamdescr),
	  modpdescr
	};

/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {
	{ "dynafilecachesize", eCmdHdlrInt, 0 }, /* legacy: dynafilecachesize */
	{ "ziplevel", eCmdHdlrInt, 0 }, /* legacy: omfileziplevel */
	{ "flushinterval", eCmdHdlrInt, 0 }, /* legacy: omfileflushinterval */
	{ "asyncwriting", eCmdHdlrBinary, 0 }, /* legacy: omfileasyncwriting */
	{ "veryrobustzip", eCmdHdlrBinary, 0 },
	{ "flushontxend", eCmdHdlrBinary, 0 }, /* legacy: omfileflushontxend */
	{ "iobuffersize", eCmdHdlrSize, 0 }, /* legacy: omfileiobuffersize */
	{ "dirowner", eCmdHdlrUID, 0 }, /* legacy: dirowner */
	{ "dirgroup", eCmdHdlrGID, 0 }, /* legacy: dirgroup */
	{ "fileowner", eCmdHdlrUID, 0 }, /* legacy: fileowner */
	{ "filegroup", eCmdHdlrGID, 0 }, /* legacy: filegroup */
	{ "dircreatemode", eCmdHdlrFileCreateMode, 0 }, /* legacy: dircreatemode */
	{ "filecreatemode", eCmdHdlrFileCreateMode, 0 }, /* legacy: filecreatemode */
	{ "failonchownfailure", eCmdHdlrBinary, 0 }, /* legacy: failonchownfailure */
	{ "createdirs", eCmdHdlrBinary, 0 }, /* legacy: createdirs */
	{ "sync", eCmdHdlrBinary, 0 }, /* legacy: actionfileenablesync */
	{ "file", eCmdHdlrString, 0 },     /* either "file" or ... */
	{ "dynafile", eCmdHdlrString, 0 }, /* "dynafile" MUST be present */
	{ "sig.provider", eCmdHdlrGetWord, 0 },
	{ "cry.provider", eCmdHdlrGetWord, 0 },
	{ "template", eCmdHdlrGetWord, 0 }
};
static struct cnfparamblk actpblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(actpdescr)/sizeof(struct cnfparamdescr),
	  actpdescr
	};


/* this function gets the default template. It coordinates action between
 * old-style and new-style configuration parts.
 */
static inline uchar*
getDfltTpl(void)
{
	if(loadModConf != NULL && loadModConf->tplName != NULL)
		return loadModConf->tplName;
	else if(pszFileDfltTplName == NULL)
		return (uchar*)"RSYSLOG_FileFormat";
	else
		return pszFileDfltTplName;
}


BEGINinitConfVars		/* (re)set config variables to default values */
CODESTARTinitConfVars 
	pszFileDfltTplName = NULL; /* make sure this can be free'ed! */
	iRet = resetConfigVariables(NULL, NULL); /* params are dummies */
ENDinitConfVars

BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURERepeatedMsgReduction)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	if(pData->bDynamicName) {
		dbgprintf("[dynamic]\n");
	} else { /* regular file */
		dbgprintf("%s%s\n", pData->f_fname,
			  (pData->pStrm == NULL) ? " (closed)" : "");
	}

	dbgprintf("\ttemplate='%s'\n", pData->f_fname);
	dbgprintf("\tuse async writer=%d\n", pData->bUseAsyncWriter);
	dbgprintf("\tflush on TX end=%d\n", pData->bFlushOnTXEnd);
	dbgprintf("\tflush interval=%d\n", pData->iFlushInterval);
	dbgprintf("\tfile cache size=%d\n", pData->iDynaFileCacheSize);
	dbgprintf("\tcreate directories: %s\n", pData->bCreateDirs ? "on" : "off");
	dbgprintf("\tvery robust zip: %s\n", pData->bCreateDirs ? "on" : "off");
	dbgprintf("\tfile owner %d, group %d\n", (int) pData->fileUID, (int) pData->fileGID);
	dbgprintf("\tdirectory owner %d, group %d\n", (int) pData->dirUID, (int) pData->dirGID);
	dbgprintf("\tdir create mode 0%3.3o, file create mode 0%3.3o\n",
		  pData->fDirCreateMode, pData->fCreateMode);
	dbgprintf("\tfail if owner/group can not be set: %s\n", pData->bFailOnChown ? "yes" : "no");
ENDdbgPrintInstInfo



/* set the default template to be used
 * This is a module-global parameter, and as such needs special handling. It needs to
 * be coordinated with values set via the v2 config system (rsyslog v6+). What we do
 * is we do not permit this directive after the v2 config system has been used to set
 * the parameter.
 */
rsRetVal
setLegacyDfltTpl(void __attribute__((unused)) *pVal, uchar* newVal)
{
	DEFiRet;

	if(loadModConf != NULL && loadModConf->tplName != NULL) {
		free(newVal);
		errmsg.LogError(0, RS_RET_ERR, "omfile: default template already set via module "
			"global parameter - can no longer be changed");
		ABORT_FINALIZE(RS_RET_ERR);
	}
	free(pszFileDfltTplName);
	pszFileDfltTplName = newVal;
finalize_it:
	RETiRet;
}


/* set the dynaFile cache size. Does some limit checking.
 * rgerhards, 2007-07-31
 */
rsRetVal setDynaFileCacheSize(void __attribute__((unused)) *pVal, int iNewVal)
{
	DEFiRet;
	uchar errMsg[128];	/* for dynamic error messages */

	if(iNewVal < 1) {
		snprintf((char*) errMsg, sizeof(errMsg)/sizeof(uchar),
		         "DynaFileCacheSize must be greater 0 (%d given), changed to 1.", iNewVal);
		errno = 0;
		errmsg.LogError(0, RS_RET_VAL_OUT_OF_RANGE, "%s", errMsg);
		iRet = RS_RET_VAL_OUT_OF_RANGE;
		iNewVal = 1;
	} else if(iNewVal > 1000) {
		snprintf((char*) errMsg, sizeof(errMsg)/sizeof(uchar),
		         "DynaFileCacheSize maximum is 1,000 (%d given), changed to 1,000.", iNewVal);
		errno = 0;
		errmsg.LogError(0, RS_RET_VAL_OUT_OF_RANGE, "%s", errMsg);
		iRet = RS_RET_VAL_OUT_OF_RANGE;
		iNewVal = 1000;
	}

	cs.iDynaFileCacheSize = iNewVal;
	DBGPRINTF("DynaFileCacheSize changed to %d.\n", iNewVal);

	RETiRet;
}


/* Helper to cfline(). Parses a output channel name up until the first
 * comma and then looks for the template specifier. Tries
 * to find that template. Maps the output channel to the 
 * proper filed structure settings. Everything is stored in the
 * filed struct. Over time, the dependency on filed might be
 * removed.
 * rgerhards 2005-06-21
 */
static rsRetVal cflineParseOutchannel(instanceData *pData, uchar* p, omodStringRequest_t *pOMSR, int iEntry, int iTplOpts)
{
	DEFiRet;
	size_t i;
	struct outchannel *pOch;
	char szBuf[128];	/* should be more than sufficient */

	++p; /* skip '$' */
	i = 0;
	/* get outchannel name */
	while(*p && *p != ';' && *p != ' ' &&
	      i < sizeof(szBuf) / sizeof(char)) {
	      szBuf[i++] = *p++;
	}
	szBuf[i] = '\0';

	/* got the name, now look up the channel... */
	pOch = ochFind(szBuf, i);

	if(pOch == NULL) {
		char errMsg[128];
		errno = 0;
		snprintf(errMsg, sizeof(errMsg)/sizeof(char),
			 "outchannel '%s' not found - ignoring action line",
			 szBuf);
		errmsg.LogError(0, RS_RET_NOT_FOUND, "%s", errMsg);
		ABORT_FINALIZE(RS_RET_NOT_FOUND);
	}

	/* check if there is a file name in the outchannel... */
	if(pOch->pszFileTemplate == NULL) {
		char errMsg[128];
		errno = 0;
		snprintf(errMsg, sizeof(errMsg)/sizeof(char),
			 "outchannel '%s' has no file name template - ignoring action line",
			 szBuf);
		errmsg.LogError(0, RS_RET_ERR, "%s", errMsg);
		ABORT_FINALIZE(RS_RET_ERR);
	}

	/* OK, we finally got a correct template. So let's use it... */
	pData->f_fname = ustrdup(pOch->pszFileTemplate);
	pData->iSizeLimit = pOch->uSizeLimit;
	/* WARNING: It is dangerous "just" to pass the pointer. As we
	 * never rebuild the output channel description, this is acceptable here.
	 */
	pData->pszSizeLimitCmd = pOch->cmdOnSizeLimit;

	iRet = cflineParseTemplateName(&p, pOMSR, iEntry, iTplOpts, getDfltTpl());

finalize_it:
	RETiRet;
}


/* This function deletes an entry from the dynamic file name
 * cache. A pointer to the cache must be passed in as well
 * as the index of the to-be-deleted entry. This index may
 * point to an unallocated entry, in whcih case the
 * function immediately returns. Parameter bFreeEntry is 1
 * if the entry should be d_free()ed and 0 if not.
 */
static rsRetVal
dynaFileDelCacheEntry(instanceData *pData, int iEntry, int bFreeEntry)
{
	dynaFileCacheEntry **pCache = pData->dynCache;
	DEFiRet;
	ASSERT(pCache != NULL);

	if(pCache[iEntry] == NULL)
		FINALIZE;

	DBGPRINTF("Removing entry %d for file '%s' from dynaCache.\n", iEntry,
		pCache[iEntry]->pName == NULL ? UCHAR_CONSTANT("[OPEN FAILED]") : pCache[iEntry]->pName);

	if(pCache[iEntry]->pName != NULL) {
		d_free(pCache[iEntry]->pName);
		pCache[iEntry]->pName = NULL;
	}

	if(pCache[iEntry]->pStrm != NULL) {
		strm.Destruct(&pCache[iEntry]->pStrm);
		if(pData->useSigprov) {
			pData->sigprov.OnFileClose(pCache[iEntry]->sigprovFileData);
			pCache[iEntry]->sigprovFileData = NULL;
		}
	}

	if(bFreeEntry) {
		d_free(pCache[iEntry]);
		pCache[iEntry] = NULL;
	}

finalize_it:
	RETiRet;
}


/* This function frees all dynamic file name cache entries and closes the
 * relevant files. Part of Shutdown and HUP processing.
 * rgerhards, 2008-10-23
 */
static inline void
dynaFileFreeCacheEntries(instanceData *pData)
{
	register int i;
	ASSERT(pData != NULL);

	BEGINfunc;
	for(i = 0 ; i < pData->iCurrCacheSize ; ++i) {
		dynaFileDelCacheEntry(pData, i, 1);
	}
	pData->iCurrElt = -1; /* invalidate current element */
	ENDfunc;
}


/* This function frees the dynamic file name cache.
 */
static void dynaFileFreeCache(instanceData *pData)
{
	ASSERT(pData != NULL);

	BEGINfunc;
	dynaFileFreeCacheEntries(pData);
	if(pData->dynCache != NULL)
		d_free(pData->dynCache);
	ENDfunc;
}


/* close current file */
static rsRetVal
closeFile(instanceData *pData)
{
	DEFiRet;
	if(pData->useSigprov) {
		pData->sigprov.OnFileClose(pData->sigprovFileData);
		pData->sigprovFileData = NULL;
	}
	strm.Destruct(&pData->pStrm);
	RETiRet;
}


/* This prepares the signature provider to process a file */
static rsRetVal
sigprovPrepare(instanceData *pData, uchar *fn)
{
	DEFiRet;
	pData->sigprov.OnFileOpen(pData->sigprovData, fn, &pData->sigprovFileData);
	RETiRet;
}

/* This is now shared code for all types of files. It simply prepares
 * file access, which, among others, means the the file wil be opened
 * and any directories in between will be created (based on config, of
 * course). -- rgerhards, 2008-10-22
 * changed to iRet interface - 2009-03-19
 */
static rsRetVal
prepareFile(instanceData *pData, uchar *newFileName)
{
	int fd;
	DEFiRet;

	pData->pStrm = NULL;
	if(access((char*)newFileName, F_OK) != 0) {
		/* file does not exist, create it (and eventually parent directories */
		if(pData->bCreateDirs) {
			/* We first need to create parent dirs if they are missing.
			 * We do not report any errors here ourselfs but let the code
			 * fall through to error handler below.
			 */
			if(makeFileParentDirs(newFileName, ustrlen(newFileName),
			     pData->fDirCreateMode, pData->dirUID,
			     pData->dirGID, pData->bFailOnChown) != 0) {
			     	ABORT_FINALIZE(RS_RET_ERR); /* we give up */
			}
		}
		/* no matter if we needed to create directories or not, we now try to create
		 * the file. -- rgerhards, 2008-12-18 (based on patch from William Tisater)
		 */
		fd = open((char*) newFileName, O_WRONLY|O_APPEND|O_CREAT|O_NOCTTY|O_CLOEXEC,
				pData->fCreateMode);
		if(fd != -1) {
			/* check and set uid/gid */
			if(pData->fileUID != (uid_t)-1 || pData->fileGID != (gid_t) -1) {
				/* we need to set owner/group */
				if(fchown(fd, pData->fileUID, pData->fileGID) != 0) {
					if(pData->bFailOnChown) {
						int eSave = errno;
						close(fd);
						fd = -1;
						errno = eSave;
					}
					/* we will silently ignore the chown() failure
					 * if configured to do so.
					 */
				}
			}
			close(fd); /* close again, as we need a stream further on */
		}
	}

	/* the copies below are clumpsy, but there is no way around given the
	 * anomalies in dirname() and basename() [they MODIFY the provided buffer...]
	 */
	uchar szNameBuf[MAXFNAME];
	uchar szDirName[MAXFNAME];
	uchar szBaseName[MAXFNAME];
	ustrncpy(szNameBuf, newFileName, MAXFNAME);
	ustrncpy(szDirName, (uchar*)dirname((char*)szNameBuf), MAXFNAME);
	ustrncpy(szNameBuf, newFileName, MAXFNAME);
	ustrncpy(szBaseName, (uchar*)basename((char*)szNameBuf), MAXFNAME);

	CHKiRet(strm.Construct(&pData->pStrm));
	CHKiRet(strm.SetFName(pData->pStrm, szBaseName, ustrlen(szBaseName)));
	CHKiRet(strm.SetDir(pData->pStrm, szDirName, ustrlen(szDirName)));
	CHKiRet(strm.SetiZipLevel(pData->pStrm, pData->iZipLevel));
	CHKiRet(strm.SetbVeryReliableZip(pData->pStrm, pData->bVeryRobustZip));
	CHKiRet(strm.SetsIOBufSize(pData->pStrm, (size_t) pData->iIOBufSize));
	CHKiRet(strm.SettOperationsMode(pData->pStrm, STREAMMODE_WRITE_APPEND));
	CHKiRet(strm.SettOpenMode(pData->pStrm, cs.fCreateMode));
	CHKiRet(strm.SetbSync(pData->pStrm, pData->bSyncFile));
	CHKiRet(strm.SetsType(pData->pStrm, STREAMTYPE_FILE_SINGLE));
	CHKiRet(strm.SetiSizeLimit(pData->pStrm, pData->iSizeLimit));
	if(pData->useCryprov) {
		CHKiRet(strm.Setcryprov(pData->pStrm, &pData->cryprov));
		CHKiRet(strm.SetcryprovData(pData->pStrm, pData->cryprovData));
	}
	/* set the flush interval only if we actually use it - otherwise it will activate
	 * async processing, which is a real performance waste if we do not do buffered
	 * writes! -- rgerhards, 2009-07-06
	 */
	if(pData->bUseAsyncWriter)
		CHKiRet(strm.SetiFlushInterval(pData->pStrm, pData->iFlushInterval));
	if(pData->pszSizeLimitCmd != NULL)
		CHKiRet(strm.SetpszSizeLimitCmd(pData->pStrm, ustrdup(pData->pszSizeLimitCmd)));
	CHKiRet(strm.ConstructFinalize(pData->pStrm));

	if(pData->useSigprov)
		sigprovPrepare(pData, szNameBuf);
	
finalize_it:
	if(iRet != RS_RET_OK) {
		if(pData->pStrm != NULL) {
			closeFile(pData);
		}
	}
	RETiRet;
}


/* This function handles dynamic file names. It checks if the
 * requested file name is already open and, if not, does everything
 * needed to switch to the it.
 * Function returns 0 if all went well and non-zero otherwise.
 * On successful return pData->fd must point to the correct file to
 * be written.
 * This is a helper to writeFile(). rgerhards, 2007-07-03
 */
static inline rsRetVal
prepareDynFile(instanceData *pData, uchar *newFileName, unsigned iMsgOpts)
{
	uint64 ctOldest; /* "timestamp" of oldest element */
	int iOldest;
	int i;
	int iFirstFree;
	rsRetVal localRet;
	dynaFileCacheEntry **pCache;
	DEFiRet;

	ASSERT(pData != NULL);
	ASSERT(newFileName != NULL);

	pCache = pData->dynCache;

	/* first check, if we still have the current file */
	if(   (pData->iCurrElt != -1)
	   && !ustrcmp(newFileName, pCache[pData->iCurrElt]->pName)) {
	   	/* great, we are all set */
		pCache[pData->iCurrElt]->clkTickAccessed = getClockFileAccess();
		STATSCOUNTER_INC(pData->ctrLevel0, pData->mutCtrLevel0);
		/* LRU needs only a strictly monotonically increasing counter, so such a one could do */
		FINALIZE;
	}

	/* ok, no luck. Now let's search the table if we find a matching spot.
	 * While doing so, we also prepare for creation of a new one.
	 */
	pData->iCurrElt = -1;	/* invalid current element pointer */
	iFirstFree = -1; /* not yet found */
	iOldest = 0; /* we assume the first element to be the oldest - that will change as we loop */
	ctOldest = getClockFileAccess(); /* there must always be an older one */
	for(i = 0 ; i < pData->iCurrCacheSize ; ++i) {
		if(pCache[i] == NULL || pCache[i]->pName == NULL) {
			if(iFirstFree == -1)
				iFirstFree = i;
		} else { /* got an element, let's see if it matches */
			if(!ustrcmp(newFileName, pCache[i]->pName)) {
				/* we found our element! */
				pData->pStrm = pCache[i]->pStrm;
				if(pData->useSigprov)
					pData->sigprovFileData = pCache[i]->sigprovFileData;
				pData->iCurrElt = i;
				pCache[i]->clkTickAccessed = getClockFileAccess(); /* update "timestamp" for LRU */
				FINALIZE;
			}
			/* did not find it - so lets keep track of the counters for LRU */
			if(pCache[i]->clkTickAccessed < ctOldest) {
				ctOldest = pCache[i]->clkTickAccessed;
				iOldest = i;
				}
		}
	}

	/* we have not found an entry */
	STATSCOUNTER_INC(pData->ctrMiss, pData->mutCtrMiss);

	/* invalidate iCurrElt as we may error-exit out of this function when the currrent
	 * iCurrElt has been freed or otherwise become unusable. This is a precaution, and
	 * performance-wise it may be better to do that in each of the exits. However, that
	 * is error-prone, so I prefer to do it here. -- rgerhards, 2010-03-02
	 */
	pData->iCurrElt = -1;
	/* similarly, we need to set the current pStrm to NULL, because otherwise, if prepareFile() fails,
	 * we may end up using an old stream. This bug depends on how exactly prepareFile fails,
	 * but it could be triggered in the common case of a failed open() system call.
	 * rgerhards, 2010-03-22
	 */
	pData->pStrm = NULL, pData->sigprovFileData = NULL;

	if(iFirstFree == -1 && (pData->iCurrCacheSize < pData->iDynaFileCacheSize)) {
		/* there is space left, so set it to that index */
		iFirstFree = pData->iCurrCacheSize++;
		STATSCOUNTER_SETMAX_NOMUT(pData->ctrMax, (unsigned) pData->iCurrCacheSize);
	}

	/* Note that the following code sequence does not work with the cache entry itself,
	 * but rather with pData->pStrm, the (sole) stream pointer in the non-dynafile case.
	 * The cache array is only updated after the open was successful. -- rgerhards, 2010-03-21
	 */
	if(iFirstFree == -1) {
		dynaFileDelCacheEntry(pData, iOldest, 0);
		STATSCOUNTER_INC(pData->ctrEvict, pData->mutCtrEvict);
		iFirstFree = iOldest; /* this one *is* now free ;) */
	} else {
		/* we need to allocate memory for the cache structure */
		CHKmalloc(pCache[iFirstFree] = (dynaFileCacheEntry*) calloc(1, sizeof(dynaFileCacheEntry)));
	}

	/* Ok, we finally can open the file */
	localRet = prepareFile(pData, newFileName); /* ignore exact error, we check fd below */

	/* check if we had an error */
	if(localRet != RS_RET_OK) {
		/* do not report anything if the message is an internally-generated
		 * message. Otherwise, we could run into a never-ending loop. The bad
		 * news is that we also lose errors on startup messages, but so it is.
		 */
		if(iMsgOpts & INTERNAL_MSG) {
			DBGPRINTF("Could not open dynaFile '%s', state %d, discarding message\n",
				  newFileName, localRet);
		} else {
			errmsg.LogError(0, localRet, "Could not open dynamic file '%s' [state %d] - discarding message", newFileName, localRet);
		}
		ABORT_FINALIZE(localRet);
	}

	if((pCache[iFirstFree]->pName = ustrdup(newFileName)) == NULL) {
		closeFile(pData); /* need to free failed entry! */
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}
	pCache[iFirstFree]->pStrm = pData->pStrm;
	if(pData->useSigprov)
		pCache[iFirstFree]->sigprovFileData = pData->sigprovFileData;
	pCache[iFirstFree]->clkTickAccessed = getClockFileAccess();
	pData->iCurrElt = iFirstFree;
	DBGPRINTF("Added new entry %d for file cache, file '%s'.\n", iFirstFree, newFileName);

finalize_it:
	RETiRet;
}


/* do the actual write process. This function is to be called once we are ready for writing.
 * It will do buffered writes and persist data only when the buffer is full. Note that we must
 * be careful to detect when the file handle changed.
 * rgerhards, 2009-06-03
 */
static  rsRetVal
doWrite(instanceData *pData, uchar *pszBuf, int lenBuf)
{
	DEFiRet;
	ASSERT(pData != NULL);
	ASSERT(pszBuf != NULL);

	DBGPRINTF("write to stream, pData->pStrm %p, lenBuf %d\n", pData->pStrm, lenBuf);
	if(pData->pStrm != NULL){
		CHKiRet(strm.Write(pData->pStrm, pszBuf, lenBuf));
		if(pData->useSigprov) {
			CHKiRet(pData->sigprov.OnRecordWrite(pData->sigprovFileData, pszBuf, lenBuf));
		}
	}

finalize_it:
	RETiRet;
}


/* rgerhards 2004-11-11: write to a file output.  */
static rsRetVal
writeFile(uchar **ppString, unsigned iMsgOpts, instanceData *pData)
{
	DEFiRet;

	ASSERT(pData != NULL);

	/* first check if we have a dynamic file name and, if so,
	 * check if it still is ok or a new file needs to be created
	 */
	if(pData->bDynamicName) {
		CHKiRet(prepareDynFile(pData, ppString[1], iMsgOpts));
	} else { /* "regular", non-dynafile */
		if(pData->pStrm == NULL) {
			CHKiRet(prepareFile(pData, pData->f_fname));
			if(pData->pStrm == NULL) {
				errmsg.LogError(0, RS_RET_NO_FILE_ACCESS, "Could no open output file '%s'", pData->f_fname);
			}
		}
	}

	CHKiRet(doWrite(pData, ppString[0], strlen(CHAR_CONVERT(ppString[0]))));

finalize_it:
	RETiRet;
}


BEGINbeginCnfLoad
CODESTARTbeginCnfLoad
	loadModConf = pModConf;
	pModConf->pConf = pConf;
	pModConf->tplName = NULL;
ENDbeginCnfLoad

BEGINsetModCnf
	struct cnfparamvals *pvals = NULL;
	int i;
CODESTARTsetModCnf
	pvals = nvlstGetParams(lst, &modpblk, NULL);
	if(pvals == NULL) {
		errmsg.LogError(0, RS_RET_MISSING_CNFPARAMS, "error processing module "
				"config parameters [module(...)]");
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	if(Debug) {
		dbgprintf("module (global) param blk for omfile:\n");
		cnfparamsPrint(&modpblk, pvals);
	}

	for(i = 0 ; i < modpblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(modpblk.descr[i].name, "template")) {
			loadModConf->tplName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
			if(pszFileDfltTplName != NULL) {
				errmsg.LogError(0, RS_RET_DUP_PARAM, "omfile: warning: default template "
						"was already set via legacy directive - may lead to inconsistent "
						"results.");
			}
		} else {
			dbgprintf("omfile: program error, non-handled "
			  "param '%s' in beginCnfLoad\n", modpblk.descr[i].name);
		}
	}
finalize_it:
	if(pvals != NULL)
		cnfparamvalsDestruct(pvals, &modpblk);
ENDsetModCnf

BEGINendCnfLoad
CODESTARTendCnfLoad
	loadModConf = NULL; /* done loading */
	/* free legacy config vars */
	free(pszFileDfltTplName);
	pszFileDfltTplName = NULL;
ENDendCnfLoad

BEGINcheckCnf
CODESTARTcheckCnf
ENDcheckCnf

BEGINactivateCnf
CODESTARTactivateCnf
	runModConf = pModConf;
ENDactivateCnf

BEGINfreeCnf
CODESTARTfreeCnf
	free(pModConf->tplName);
ENDfreeCnf


BEGINcreateInstance
CODESTARTcreateInstance
	pData->pStrm = NULL;
ENDcreateInstance


BEGINfreeInstance
CODESTARTfreeInstance
	free(pData->tplName);
	free(pData->f_fname);
	if(pData->bDynamicName) {
		dynaFileFreeCache(pData);
	} else if(pData->pStrm != NULL)
		closeFile(pData);
	if(pData->useSigprov) {
		pData->sigprov.Destruct(&pData->sigprovData);
		obj.ReleaseObj(__FILE__, pData->sigprovNameFull+2, pData->sigprovNameFull,
			       (void*) &pData->sigprov);
		free(pData->sigprovName);
		free(pData->sigprovNameFull);
	}
	if(pData->useCryprov) {
		pData->cryprov.Destruct(&pData->cryprovData);
		obj.ReleaseObj(__FILE__, pData->cryprovNameFull+2, pData->cryprovNameFull,
			       (void*) &pData->cryprov);
		free(pData->cryprovName);
		free(pData->cryprovNameFull);
	}
ENDfreeInstance


BEGINtryResume
CODESTARTtryResume
ENDtryResume

BEGINbeginTransaction
CODESTARTbeginTransaction
	/* we have nothing to do to begin a transaction */
ENDbeginTransaction


BEGINendTransaction
CODESTARTendTransaction
	/* Note: pStrm may be NULL if there was an error opening the stream */
	if(pData->bFlushOnTXEnd && pData->pStrm != NULL) {
		/* if we have an async writer, it controls the flush via
		 * a timeout. However, without it, we actually need to flush,
		 * else incomplete records are written.
		 */
		if(!pData->bUseAsyncWriter)
			CHKiRet(strm.Flush(pData->pStrm));
	}
finalize_it:
ENDendTransaction


BEGINdoAction
CODESTARTdoAction
	DBGPRINTF("file to log to: %s\n",
		  (pData->bDynamicName) ? ppString[1] : pData->f_fname);
	DBGPRINTF("omfile: start of data: '%.128s'\n", ppString[0]);
	STATSCOUNTER_INC(pData->ctrRequests, pData->mutCtrRequests);
	CHKiRet(writeFile(ppString, iMsgOpts, pData));
	if(!bCoreSupportsBatching && pData->bFlushOnTXEnd) {
		CHKiRet(strm.Flush(pData->pStrm));
	}
finalize_it:
	if(iRet == RS_RET_OK)
		iRet = RS_RET_DEFER_COMMIT;
ENDdoAction


static inline void
setInstParamDefaults(instanceData *pData)
{
	pData->f_fname = NULL;
	pData->tplName = NULL;
	pData->fileUID = -1;
	pData->fileGID = -1;
	pData->dirUID = -1;
	pData->dirGID = -1;
	pData->bFailOnChown = 1;
	pData->iDynaFileCacheSize = 10;
	pData->fCreateMode = 0644;
	pData->fDirCreateMode = 0700;
	pData->bCreateDirs = 1;
	pData->bSyncFile = 0;
	pData->iZipLevel = 0;
	pData->bVeryRobustZip = 0;
	pData->bFlushOnTXEnd = FLUSHONTX_DFLT;
	pData->iIOBufSize = IOBUF_DFLT_SIZE;
	pData->iFlushInterval = FLUSH_INTRVL_DFLT;
	pData->bUseAsyncWriter = USE_ASYNCWRITER_DFLT;
	pData->sigprovName = NULL;
	pData->cryprovName = NULL;
	pData->useSigprov = 0;
	pData->useCryprov = 0;
}


static rsRetVal
setupInstStatsCtrs(instanceData *pData)
{
	uchar ctrName[512];
	DEFiRet;

	if(!pData->bDynamicName) {
		FINALIZE;
	}

	/* support statistics gathering */
	snprintf((char*)ctrName, sizeof(ctrName), "dynafile cache %s", pData->f_fname);
	ctrName[sizeof(ctrName)-1] = '\0'; /* be on the save side */
	CHKiRet(statsobj.Construct(&(pData->stats)));
	CHKiRet(statsobj.SetName(pData->stats, ctrName));
	STATSCOUNTER_INIT(pData->ctrRequests, pData->mutCtrRequests);
	CHKiRet(statsobj.AddCounter(pData->stats, UCHAR_CONSTANT("requests"),
		ctrType_IntCtr, &(pData->ctrRequests)));
	STATSCOUNTER_INIT(pData->ctrLevel0, pData->mutCtrLevel0);
	CHKiRet(statsobj.AddCounter(pData->stats, UCHAR_CONSTANT("level0"),
		ctrType_IntCtr, &(pData->ctrLevel0)));
	STATSCOUNTER_INIT(pData->ctrMiss, pData->mutCtrMiss);
	CHKiRet(statsobj.AddCounter(pData->stats, UCHAR_CONSTANT("missed"),
		ctrType_IntCtr, &(pData->ctrMiss)));
	STATSCOUNTER_INIT(pData->ctrEvict, pData->mutCtrEvict);
	CHKiRet(statsobj.AddCounter(pData->stats, UCHAR_CONSTANT("evicted"),
		ctrType_IntCtr, &(pData->ctrEvict)));
	STATSCOUNTER_INIT(pData->ctrMax, pData->mutCtrMax);
	CHKiRet(statsobj.AddCounter(pData->stats, UCHAR_CONSTANT("maxused"),
		ctrType_IntCtr, &(pData->ctrMax)));
	CHKiRet(statsobj.ConstructFinalize(pData->stats));

finalize_it:
	RETiRet;
}

static inline void
initSigprov(instanceData *pData, struct nvlst *lst)
{
	uchar szDrvrName[1024];

	if(snprintf((char*)szDrvrName, sizeof(szDrvrName), "lmsig_%s", pData->sigprovName)
		== sizeof(szDrvrName)) {
		errmsg.LogError(0, RS_RET_ERR, "omfile: signature provider "
				"name is too long: '%s' - signatures disabled",
				pData->sigprovName);
		goto done;
	}
	pData->sigprovNameFull = ustrdup(szDrvrName);

	pData->sigprov.ifVersion = sigprovCURR_IF_VERSION;
	/* The pDrvrName+2 below is a hack to obtain the object name. It 
	 * safes us to have yet another variable with the name without "lm" in
	 * front of it. If we change the module load interface, we may re-think
	 * about this hack, but for the time being it is efficient and clean enough.
	 */
	if(obj.UseObj(__FILE__, szDrvrName, szDrvrName, (void*) &pData->sigprov)
		!= RS_RET_OK) {
		errmsg.LogError(0, RS_RET_LOAD_ERROR, "omfile: could not load "
				"signature provider '%s' - signatures disabled",
				szDrvrName);
		goto done;
	}

	if(pData->sigprov.Construct(&pData->sigprovData) != RS_RET_OK) {
		errmsg.LogError(0, RS_RET_SIGPROV_ERR, "omfile: error constructing "
				"signature provider %s dataset - signatures disabled",
				szDrvrName);
		goto done;
	}
	pData->sigprov.SetCnfParam(pData->sigprovData, lst);

	dbgprintf("loaded signature provider %s, data instance at %p\n",
		  szDrvrName, pData->sigprovData);
	pData->useSigprov = 1;
done:	return;
}

static inline rsRetVal
initCryprov(instanceData *pData, struct nvlst *lst)
{
	uchar szDrvrName[1024];
	DEFiRet;

	if(snprintf((char*)szDrvrName, sizeof(szDrvrName), "lmcry_%s", pData->cryprovName)
		== sizeof(szDrvrName)) {
		errmsg.LogError(0, RS_RET_ERR, "omfile: crypto provider "
				"name is too long: '%s' - encryption disabled",
				pData->cryprovName);
		ABORT_FINALIZE(RS_RET_ERR);
	}
	pData->cryprovNameFull = ustrdup(szDrvrName);

	pData->cryprov.ifVersion = cryprovCURR_IF_VERSION;
	/* The pDrvrName+2 below is a hack to obtain the object name. It 
	 * safes us to have yet another variable with the name without "lm" in
	 * front of it. If we change the module load interface, we may re-think
	 * about this hack, but for the time being it is efficient and clean enough.
	 */
	if(obj.UseObj(__FILE__, szDrvrName, szDrvrName, (void*) &pData->cryprov)
		!= RS_RET_OK) {
		errmsg.LogError(0, RS_RET_LOAD_ERROR, "omfile: could not load "
				"crypto provider '%s' - encryption disabled",
				szDrvrName);
		ABORT_FINALIZE(RS_RET_CRYPROV_ERR);
	}

	if(pData->cryprov.Construct(&pData->cryprovData) != RS_RET_OK) {
		errmsg.LogError(0, RS_RET_CRYPROV_ERR, "omfile: error constructing "
				"crypto provider %s dataset - encryption disabled",
				szDrvrName);
		ABORT_FINALIZE(RS_RET_CRYPROV_ERR);
	}
	CHKiRet(pData->cryprov.SetCnfParam(pData->cryprovData, lst));

	dbgprintf("loaded crypto provider %s, data instance at %p\n",
		  szDrvrName, pData->cryprovData);
	pData->useCryprov = 1;
finalize_it:
	RETiRet;
}

BEGINnewActInst
	struct cnfparamvals *pvals;
	uchar *tplToUse;
	int i;
CODESTARTnewActInst
	DBGPRINTF("newActInst (omfile)\n");

	pvals = nvlstGetParams(lst, &actpblk, NULL);
	if(pvals == NULL) {
		errmsg.LogError(0, RS_RET_MISSING_CNFPARAMS, "omfile: either the \"file\" or "
				"\"dynfile\" parameter must be given");
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	if(Debug) {
		dbgprintf("action param blk in omfile:\n");
		cnfparamsPrint(&actpblk, pvals);
	}

	CHKiRet(createInstance(&pData));
	setInstParamDefaults(pData);

	for(i = 0 ; i < actpblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(actpblk.descr[i].name, "dynafilecachesize")) {
			pData->iDynaFileCacheSize = (int) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "ziplevel")) {
			pData->iZipLevel = (int) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "flushinterval")) {
			pData->iFlushInterval = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "veryrobustzip")) {
			pData->bVeryRobustZip = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "asyncwriting")) {
			pData->bUseAsyncWriter = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "flushontxend")) {
			pData->bFlushOnTXEnd = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "iobuffersize")) {
			pData->iIOBufSize = (int) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "dirowner")) {
			pData->dirUID = (int) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "dirgroup")) {
			pData->dirGID = (int) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "fileowner")) {
			pData->fileUID = (int) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "filegroup")) {
			pData->fileGID = (int) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "dircreatemode")) {
			pData->fDirCreateMode = (int) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "filecreatemode")) {
			pData->fCreateMode = (int) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "failonchownfailure")) {
			pData->bFailOnChown = (int) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "sync")) {
			pData->bSyncFile = (int) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "createdirs")) {
			pData->bCreateDirs = (int) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "file")) {
			pData->f_fname = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
			CODE_STD_STRING_REQUESTnewActInst(1)
			pData->bDynamicName = 0;
		} else if(!strcmp(actpblk.descr[i].name, "dynafile")) {
			pData->f_fname = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
			CODE_STD_STRING_REQUESTnewActInst(2)
			pData->bDynamicName = 1;
		} else if(!strcmp(actpblk.descr[i].name, "template")) {
			pData->tplName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "sig.provider")) {
			pData->sigprovName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "cry.provider")) {
			pData->cryprovName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else {
			dbgprintf("omfile: program error, non-handled "
			  "param '%s'\n", actpblk.descr[i].name);
		}
	}

	if(pData->f_fname == NULL) {
		errmsg.LogError(0, RS_RET_MISSING_CNFPARAMS, "omfile: either the \"file\" or "
				"\"dynfile\" parameter must be given");
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	if(pData->sigprovName != NULL) {
		initSigprov(pData, lst);
	}

	if(pData->cryprovName != NULL) {
		CHKiRet(initCryprov(pData, lst));
	}

	tplToUse = ustrdup((pData->tplName == NULL) ? getDfltTpl() : pData->tplName);
	CHKiRet(OMSRsetEntry(*ppOMSR, 0, tplToUse, OMSR_NO_RQD_TPL_OPTS));

	if(pData->bDynamicName) {
		/* "filename" is actually a template name, we need this as string 1. So let's add it
		 * to the pOMSR. -- rgerhards, 2007-07-27
		 */
		CHKiRet(OMSRsetEntry(*ppOMSR, 1, ustrdup(pData->f_fname), OMSR_NO_RQD_TPL_OPTS));
		// TODO: create unified code for this (legacy+v6 system)
		/* we now allocate the cache table */
		CHKmalloc(pData->dynCache = (dynaFileCacheEntry**)
				calloc(pData->iDynaFileCacheSize, sizeof(dynaFileCacheEntry*)));
		pData->iCurrElt = -1;		  /* no current element */
	}
// TODO: add	pData->iSizeLimit = 0; /* default value, use outchannels to configure! */
	setupInstStatsCtrs(pData);

CODE_STD_FINALIZERnewActInst
	cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst


BEGINparseSelectorAct
	uchar fname[MAXFNAME];
CODESTARTparseSelectorAct
	/* Note: the indicator sequence permits us to use '$' to signify
	 * outchannel, what otherwise is not possible due to truely 
	 * unresolvable grammar conflicts (*this time no way around*).
	 * rgerhards, 2011-07-09
	 */
	if(!strncmp((char*) p, ":omfile:", sizeof(":omfile:") - 1)) {
		p += sizeof(":omfile:") - 1;
	} 
	if(!(*p == '$' || *p == '?' || *p == '/' || *p == '.' || *p == '-'))
		ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);

	CHKiRet(createInstance(&pData));

	if(*p == '-') {
		pData->bSyncFile = 0;
		p++;
	} else {
		pData->bSyncFile = cs.bEnableSync;
	}
	pData->iSizeLimit = 0; /* default value, use outchannels to configure! */

	switch(*p) {
        case '$':
		CODE_STD_STRING_REQUESTparseSelectorAct(1)
		/* rgerhards 2005-06-21: this is a special setting for output-channel
		 * definitions. In the long term, this setting will probably replace
		 * anything else, but for the time being we must co-exist with the
		 * traditional mode lines.
		 * rgerhards, 2007-07-24: output-channels will go away. We keep them
		 * for compatibility reasons, but seems to have been a bad idea.
		 */
		CHKiRet(cflineParseOutchannel(pData, p, *ppOMSR, 0, OMSR_NO_RQD_TPL_OPTS));
		pData->bDynamicName = 0;
		break;

	case '?': /* This is much like a regular file handle, but we need to obtain
		   * a template name. rgerhards, 2007-07-03
		   */
		CODE_STD_STRING_REQUESTparseSelectorAct(2)
		++p; /* eat '?' */
		CHKiRet(cflineParseFileName(p, fname, *ppOMSR, 0, OMSR_NO_RQD_TPL_OPTS, getDfltTpl()));
		pData->f_fname = ustrdup(fname);
		pData->bDynamicName = 1;
		pData->iCurrElt = -1;		  /* no current element */
		/* "filename" is actually a template name, we need this as string 1. So let's add it
		 * to the pOMSR. -- rgerhards, 2007-07-27
		 */
		CHKiRet(OMSRsetEntry(*ppOMSR, 1, ustrdup(pData->f_fname), OMSR_NO_RQD_TPL_OPTS));
		/* we now allocate the cache table */
		CHKmalloc(pData->dynCache = (dynaFileCacheEntry**)
				calloc(cs.iDynaFileCacheSize, sizeof(dynaFileCacheEntry*)));
		break;

	case '/':
	case '.':
		CODE_STD_STRING_REQUESTparseSelectorAct(1)
		CHKiRet(cflineParseFileName(p, fname, *ppOMSR, 0, OMSR_NO_RQD_TPL_OPTS, getDfltTpl()));
		pData->f_fname = ustrdup(fname);
		pData->bDynamicName = 0;
		break;
	default:
		ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
	}

	/* freeze current paremeters for this action */
	pData->iDynaFileCacheSize = cs.iDynaFileCacheSize;
	pData->fCreateMode = cs.fCreateMode;
	pData->fDirCreateMode = cs.fDirCreateMode;
	pData->bCreateDirs = cs.bCreateDirs;
	pData->bFailOnChown = cs.bFailOnChown;
	pData->fileUID = cs.fileUID;
	pData->fileGID = cs.fileGID;
	pData->dirUID = cs.dirUID;
	pData->dirGID = cs.dirGID;
	pData->iZipLevel = cs.iZipLevel;
	pData->bFlushOnTXEnd = cs.bFlushOnTXEnd;
	pData->iIOBufSize = (int) cs.iIOBufSize;
	pData->iFlushInterval = cs.iFlushInterval;
	pData->bUseAsyncWriter = cs.bUseAsyncWriter;
	pData->bVeryRobustZip = 0;	/* cannot be specified via legacy conf */
	setupInstStatsCtrs(pData);
CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


/* Reset config variables for this module to default values.
 * rgerhards, 2007-07-17
 */
static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	cs.fileUID = -1;
	cs.fileGID = -1;
	cs.dirUID = -1;
	cs.dirGID = -1;
	cs.bFailOnChown = 1;
	cs.iDynaFileCacheSize = 10;
	cs.fCreateMode = 0644;
	cs.fDirCreateMode = 0700;
	cs.bCreateDirs = 1;
	cs.bEnableSync = 0;
	cs.iZipLevel = 0;
	cs.bFlushOnTXEnd = FLUSHONTX_DFLT;
	cs.iIOBufSize = IOBUF_DFLT_SIZE;
	cs.iFlushInterval = FLUSH_INTRVL_DFLT;
	cs.bUseAsyncWriter = USE_ASYNCWRITER_DFLT;
	free(pszFileDfltTplName);
	pszFileDfltTplName = NULL;
	return RS_RET_OK;
}


BEGINdoHUP
CODESTARTdoHUP
	if(pData->bDynamicName) {
		dynaFileFreeCacheEntries(pData);
	} else {
		if(pData->pStrm != NULL) {
			closeFile(pData);
		}
	}
ENDdoHUP


BEGINmodExit
CODESTARTmodExit
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(strm, CORE_COMPONENT);
	objRelease(statsobj, CORE_COMPONENT);
	DESTROY_ATOMIC_HELPER_MUT(mutClock);
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
CODEqueryEtryPt_STD_CONF2_QUERIES
CODEqueryEtryPt_STD_CONF2_setModCnf_QUERIES
CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES
CODEqueryEtryPt_TXIF_OMOD_QUERIES /* we support the transactional interface! */
CODEqueryEtryPt_doHUP
ENDqueryEtryPt


BEGINmodInit(File)
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
INITLegCnfVars
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(strm, CORE_COMPONENT));
	CHKiRet(objUse(statsobj, CORE_COMPONENT));

	INIT_ATOMIC_HELPER_MUT(mutClock);

	INITChkCoreFeature(bCoreSupportsBatching, CORE_FEATURE_BATCHING);
	DBGPRINTF("omfile: %susing transactional output interface.\n", bCoreSupportsBatching ? "" : "not ");
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"dynafilecachesize", 0, eCmdHdlrInt, (void*) setDynaFileCacheSize, NULL, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"omfileziplevel", 0, eCmdHdlrInt, NULL, &cs.iZipLevel, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"omfileflushinterval", 0, eCmdHdlrInt, NULL, &cs.iFlushInterval, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"omfileasyncwriting", 0, eCmdHdlrBinary, NULL, &cs.bUseAsyncWriter, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"omfileflushontxend", 0, eCmdHdlrBinary, NULL, &cs.bFlushOnTXEnd, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"omfileiobuffersize", 0, eCmdHdlrSize, NULL, &cs.iIOBufSize, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"dirowner", 0, eCmdHdlrUID, NULL, &cs.dirUID, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"dirgroup", 0, eCmdHdlrGID, NULL, &cs.dirGID, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"fileowner", 0, eCmdHdlrUID, NULL, &cs.fileUID, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"filegroup", 0, eCmdHdlrGID, NULL, &cs.fileGID, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"dircreatemode", 0, eCmdHdlrFileCreateMode, NULL, &cs.fDirCreateMode, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"filecreatemode", 0, eCmdHdlrFileCreateMode, NULL, &cs.fCreateMode, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"createdirs", 0, eCmdHdlrBinary, NULL, &cs.bCreateDirs, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"failonchownfailure", 0, eCmdHdlrBinary, NULL, &cs.bFailOnChown, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"omfileforcechown", 0, eCmdHdlrGoneAway, NULL, NULL, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"actionfileenablesync", 0, eCmdHdlrBinary, NULL, &cs.bEnableSync, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"actionfiledefaulttemplate", 0, eCmdHdlrGetWord, setLegacyDfltTpl, NULL, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit
/* vi:set ai:
 */
