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
 * Copyright 2007-2012 Adiscon GmbH.
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

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP

/* forward definitions */
static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal);

/* internal structures
 */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(errmsg)
DEFobjCurrIf(strm)

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
	uint64	clkTickAccessed;/* for LRU - based on clockFileAccess */
};
typedef struct s_dynaFileCacheEntry dynaFileCacheEntry;


#define IOBUF_DFLT_SIZE 1024	/* default size for io buffers */
#define FLUSH_INTRVL_DFLT 1 	/* default buffer flush interval (in seconds) */
#define USE_ASYNCWRITER_DFLT 0 	/* default buffer use async writer */
#define FLUSHONTX_DFLT 1 	/* default for flush on TX end */

#define DFLT_bForceChown 0

typedef struct _instanceData {
	uchar	f_fname[MAXFNAME];/* file or template name (display only) */
	strm_t	*pStrm;		/* our output stream */
	strmType_t strmType;	/* stream type, used for named pipes */
	char	bDynamicName;	/* 0 - static name, 1 - dynamic name (with properties) */
	int	fCreateMode;	/* file creation mode for open() */
	int	fDirCreateMode;	/* creation mode for mkdir() */
	int	bCreateDirs;	/* auto-create directories? */
	int	bSyncFile;	/* should the file by sync()'ed? 1- yes, 0- no */
	sbool	bForceChown;	/* force chown() on existing files? */
	uid_t	fileUID;	/* IDs for creation */
	uid_t	dirUID;
	gid_t	fileGID;
	gid_t	dirGID;
	int	bFailOnChown;	/* fail creation if chown fails? */
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
} instanceData;


typedef struct configSettings_s {
	int iDynaFileCacheSize; /* max cache for dynamic files */
	int fCreateMode; /* mode to use when creating files */
	int fDirCreateMode; /* mode to use when creating files */
	int	bFailOnChown;	/* fail if chown fails? */
	int	bForceChown;	/* Force chown() on existing files? */
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
uchar	*pszFileDfltTplName; /* name of the default template to use */

SCOPING_SUPPORT; /* must be set AFTER configSettings_t is defined */

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
			  (pData->pStrm == NULL) ? " (unused)" : "");
	}

	dbgprintf("\ttemplate='%s'\n", pData->f_fname);
	dbgprintf("\tuse async writer=%d\n", pData->bUseAsyncWriter);
	dbgprintf("\tflush on TX end=%d\n", pData->bFlushOnTXEnd);
	dbgprintf("\tflush interval=%d\n", pData->iFlushInterval);
	dbgprintf("\tfile cache size=%d\n", pData->iDynaFileCacheSize);
	dbgprintf("\tcreate directories: %s\n", pData->bCreateDirs ? "yes" : "no");
	dbgprintf("\tfile owner %d, group %d\n", (int) pData->fileUID, (int) pData->fileGID);
	dbgprintf("\tforce chown() for all files: %s\n", pData->bForceChown ? "yes" : "no"); 
	dbgprintf("\tdirectory owner %d, group %d\n", (int) pData->dirUID, (int) pData->dirGID);
	dbgprintf("\tdir create mode 0%3.3o, file create mode 0%3.3o\n",
		  pData->fDirCreateMode, pData->fCreateMode);
	dbgprintf("\tfail if owner/group can not be set: %s\n", pData->bFailOnChown ? "yes" : "no");
ENDdbgPrintInstInfo


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
	ustrncpy(pData->f_fname, pOch->pszFileTemplate, MAXFNAME);
	pData->iSizeLimit = pOch->uSizeLimit;
	/* WARNING: It is dangerous "just" to pass the pointer. As we
	 * never rebuild the output channel description, this is acceptable here.
	 */
	pData->pszSizeLimitCmd = pOch->cmdOnSizeLimit;

	iRet = cflineParseTemplateName(&p, pOMSR, iEntry, iTplOpts,
				       (pszFileDfltTplName == NULL) ? (uchar*)"RSYSLOG_FileFormat" : pszFileDfltTplName);

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
dynaFileDelCacheEntry(dynaFileCacheEntry **pCache, int iEntry, int bFreeEntry)
{
	DEFiRet;
	ASSERT(pCache != NULL);

	if(pCache[iEntry] == NULL)
		FINALIZE;

	DBGPRINTF("Removed entry %d for file '%s' from dynaCache.\n", iEntry,
		pCache[iEntry]->pName == NULL ? UCHAR_CONSTANT("[OPEN FAILED]") : pCache[iEntry]->pName);

	if(pCache[iEntry]->pName != NULL) {
		d_free(pCache[iEntry]->pName);
		pCache[iEntry]->pName = NULL;
	}

	if(pCache[iEntry]->pStrm != NULL) {
		strm.Destruct(&pCache[iEntry]->pStrm);
		if(pCache[iEntry]->pStrm != NULL) /* safety check -- TODO: remove if no longer necessary */
			abort();
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
		dynaFileDelCacheEntry(pData->dynCache, i, 1);
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
	if(access((char*)newFileName, F_OK) == 0) {
		if(pData->bForceChown) {
			/* Try to fix wrong ownership set by someone else. Note that this code
			 * will no longer work once we have made the $PrivDrop code fully secure.
			 * This change is based on an idea of Michael Terry, provided as part of
			 * the effort to make rsyslogd the Ubuntu default syslogd.
			 * rgerhards, 2009-09-11
			 */
			if(chown((char*)newFileName, pData->fileUID, pData->fileGID) != 0) {
				if(pData->bFailOnChown) {
					int eSave = errno;
					errno = eSave;
				}
			}
		}
	} else {
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
			if(pData->bForceChown || pData->fileUID != (uid_t)-1 || pData->fileGID != (gid_t) -1) {
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
	CHKiRet(strm.SetsIOBufSize(pData->pStrm, (size_t) pData->iIOBufSize));
	CHKiRet(strm.SettOperationsMode(pData->pStrm, STREAMMODE_WRITE_APPEND));
	CHKiRet(strm.SettOpenMode(pData->pStrm, cs.fCreateMode));
	CHKiRet(strm.SetbSync(pData->pStrm, pData->bSyncFile));
	CHKiRet(strm.SetsType(pData->pStrm, pData->strmType));
	CHKiRet(strm.SetiSizeLimit(pData->pStrm, pData->iSizeLimit));
	/* set the flush interval only if we actually use it - otherwise it will activate
	 * async processing, which is a real performance waste if we do not do buffered
	 * writes! -- rgerhards, 2009-07-06
	 */
	if(pData->bUseAsyncWriter)
		CHKiRet(strm.SetiFlushInterval(pData->pStrm, pData->iFlushInterval));
	if(pData->pszSizeLimitCmd != NULL)
		CHKiRet(strm.SetpszSizeLimitCmd(pData->pStrm, ustrdup(pData->pszSizeLimitCmd)));
	CHKiRet(strm.ConstructFinalize(pData->pStrm));
	
finalize_it:
	if(iRet != RS_RET_OK) {
		if(pData->pStrm != NULL) {
			strm.Destruct(&pData->pStrm);
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

	/* first check, if we still have the current file
	 * I *hope* this will be a performance enhancement.
	 */
	if(   (pData->iCurrElt != -1)
	   && !ustrcmp(newFileName, pCache[pData->iCurrElt]->pName)) {
	   	/* great, we are all set */
		pCache[pData->iCurrElt]->clkTickAccessed = getClockFileAccess();
		// LRU needs only a strictly monotonically increasing counter, so such a one could do
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
			if(!ustrcmp(newFileName, pCache[i]->pName)) {  // RG:  name == NULL?
				/* we found our element! */
				pData->pStrm = pCache[i]->pStrm;
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
	pData->pStrm = NULL;

	if(iFirstFree == -1 && (pData->iCurrCacheSize < pData->iDynaFileCacheSize)) {
		/* there is space left, so set it to that index */
		iFirstFree = pData->iCurrCacheSize++;
	}

	/* Note that the following code sequence does not work with the cache entry itself,
	 * but rather with pData->pStrm, the (sole) stream pointer in the non-dynafile case.
	 * The cache array is only updated after the open was successful. -- rgerhards, 2010-03-21
	 */
	if(iFirstFree == -1) {
		dynaFileDelCacheEntry(pCache, iOldest, 0);
		iFirstFree = iOldest; /* this one *is* now free ;) */
	} else {
		/* we need to allocate memory for the cache structure */
		/* TODO: performance note: we could alloc all entries on startup, thus saving malloc
		 *       overhead -- this may be something to consider in v5...
		 */
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
		strm.Destruct(&pData->pStrm); /* need to free failed entry! */
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}
	pCache[iFirstFree]->pStrm = pData->pStrm;
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

dbgprintf("write to stream, pData->pStrm %p, lenBuf %d\n", pData->pStrm, lenBuf);
	if(pData->pStrm != NULL){
		CHKiRet(strm.Write(pData->pStrm, pszBuf, lenBuf));
		FINALIZE;
	}

finalize_it:
	RETiRet;
}


/* rgerhards 2004-11-11: write to a file output. This
 * will be called for all outputs using file semantics,
 * for example also for pipes.
 */
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
		}
	}

	CHKiRet(doWrite(pData, ppString[0], strlen(CHAR_CONVERT(ppString[0]))));

finalize_it:
	if(iRet != RS_RET_OK) {
		/* in v5, we shall return different states for message-caused failure (but only there!) */
		if(pData->strmType == STREAMTYPE_NAMED_PIPE)
			iRet = RS_RET_DISABLE_ACTION; /* this is the traditional semantic -- rgerhards, 2010-01-15 */
	}
	RETiRet;
}


BEGINcreateInstance
CODESTARTcreateInstance
	pData->pStrm = NULL;
ENDcreateInstance


BEGINfreeInstance
CODESTARTfreeInstance
	if(pData->bDynamicName) {
		dynaFileFreeCache(pData);
	} else if(pData->pStrm != NULL)
		strm.Destruct(&pData->pStrm);
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
		CHKiRet(strm.Flush(pData->pStrm));
	}
finalize_it:
ENDendTransaction


BEGINdoAction
CODESTARTdoAction
	DBGPRINTF("file to log to: %s\n", pData->f_fname);
	CHKiRet(writeFile(ppString, iMsgOpts, pData));
	if(!bCoreSupportsBatching && pData->bFlushOnTXEnd) {
		CHKiRet(strm.Flush(pData->pStrm));
	}
finalize_it:
	if(iRet == RS_RET_OK)
		iRet = RS_RET_DEFER_COMMIT;
ENDdoAction


BEGINparseSelectorAct
CODESTARTparseSelectorAct
	/* Note: the indicator sequence permits us to use '$' to signify
	 * outchannel, what otherwise is not possible due to truely 
	 * unresolvable grammar conflicts (*this time no way around*).
	 * rgerhards, 2011-07-09
	 */
	if(!strncmp((char*) p, ":omfile:", sizeof(":omfile:") - 1)) {
		p += sizeof(":omfile:") - 1;
	} else {
		if(*p == '$') {
			errmsg.LogError(0, RS_RET_OUTDATED_STMT,
				"action '%s' treated as ':omfile:%s' - please "
				"change syntax, '%s' will not be supported in "
				"rsyslog v6 and above.", p, p, p);
		}
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
		CHKiRet(cflineParseFileName(p, (uchar*) pData->f_fname, *ppOMSR, 0, OMSR_NO_RQD_TPL_OPTS,
				               (pszFileDfltTplName == NULL) ? (uchar*)"RSYSLOG_FileFormat" : pszFileDfltTplName));
		/* "filename" is actually a template name, we need this as string 1. So let's add it
		 * to the pOMSR. -- rgerhards, 2007-07-27
		 */
		CHKiRet(OMSRsetEntry(*ppOMSR, 1, ustrdup(pData->f_fname), OMSR_NO_RQD_TPL_OPTS));

		pData->bDynamicName = 1;
		pData->iCurrElt = -1;		  /* no current element */
		/* we now allocate the cache table */
		CHKmalloc(pData->dynCache = (dynaFileCacheEntry**)
				calloc(cs.iDynaFileCacheSize, sizeof(dynaFileCacheEntry*)));
		break;

        /* case '|': while pipe support has been removed, I leave the code in in case we 
	 *           need high-performance pipes at a later stage (unlikely). -- rgerhards, 2010-02-28
	 */
	case '/':
	case '.':
		CODE_STD_STRING_REQUESTparseSelectorAct(1)
		/* we now have *almost* the same semantics for files and pipes, but we still need
		 * to know we deal with a pipe, because we must do non-blocking opens in that case
		 * (to keep consistent with traditional semantics and prevent rsyslog from hanging).
		 */
		if(*p == '|') {
			++p;
			pData->strmType = STREAMTYPE_NAMED_PIPE;
		} else {
			pData->strmType = STREAMTYPE_FILE_SINGLE;
		}

		CHKiRet(cflineParseFileName(p, (uchar*) pData->f_fname, *ppOMSR, 0, OMSR_NO_RQD_TPL_OPTS,
				               (pszFileDfltTplName == NULL) ? (uchar*)"RSYSLOG_FileFormat" : pszFileDfltTplName));
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
	pData->bForceChown = cs.bForceChown;
	pData->fileUID = cs.fileUID;
	pData->fileGID = cs.fileGID;
	pData->dirUID = cs.dirUID;
	pData->dirGID = cs.dirGID;
	pData->iZipLevel = cs.iZipLevel;
	pData->bFlushOnTXEnd = cs.bFlushOnTXEnd;
	pData->iIOBufSize = (int) cs.iIOBufSize;
	pData->iFlushInterval = cs.iFlushInterval;
	pData->bUseAsyncWriter = cs.bUseAsyncWriter;

	if(pData->bDynamicName == 0) {
		/* try open and emit error message if not possible. At this stage, we ignore the
		 * return value of prepareFile, this is taken care of in later steps.
		 */
		prepareFile(pData, pData->f_fname);
		        
	  	if(pData->pStrm == NULL) {
			DBGPRINTF("Error opening log file: %s\n", pData->f_fname);
			errmsg.LogError(0, RS_RET_NO_FILE_ACCESS, "Could not open output file '%s'", pData->f_fname);
		}
	}
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
	cs.bForceChown = DFLT_bForceChown;
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
			strm.Destruct(&pData->pStrm);
			pData->pStrm = NULL;
		}
	}
ENDdoHUP


BEGINmodExit
CODESTARTmodExit
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(strm, CORE_COMPONENT);
	free(pszFileDfltTplName);
	DESTROY_ATOMIC_HELPER_MUT(mutClock);
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
CODEqueryEtryPt_TXIF_OMOD_QUERIES /* we support the transactional interface! */
CODEqueryEtryPt_doHUP
ENDqueryEtryPt


BEGINmodInit(File)
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
SCOPINGmodInit
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(strm, CORE_COMPONENT));

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
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"omfileForceChown", 0, eCmdHdlrBinary, NULL, &cs.bForceChown, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"actionfileenablesync", 0, eCmdHdlrBinary, NULL, &cs.bEnableSync, STD_LOADABLE_MODULE_ID));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionfiledefaulttemplate", 0, eCmdHdlrGetWord, NULL, &pszFileDfltTplName, NULL));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit
/* vi:set ai:
 */
