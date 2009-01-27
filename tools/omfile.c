/* omfile.c
 * This is the implementation of the build-in file output module.
 *
 * Handles: eTypeCONSOLE, eTypeTTY, eTypeFILE, eTypePIPE
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * File begun on 2007-07-21 by RGerhards (extracted from syslogd.c)
 * This file is under development and has not yet arrived at being fully
 * self-contained and a real object. So far, it is mostly an excerpt
 * of the "old" message code without any modifications. However, it
 * helps to have things at the right place one we go to the meat of it.
 *
 * Copyright 2007, 2008 Rainer Gerhards and Adiscon GmbH.
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

#include "syslogd.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "outchannel.h"
#include "omfile.h"
#include "cfsysline.h"
#include "module-template.h"
#include "errmsg.h"

MODULE_TYPE_OUTPUT

/* internal structures
 */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(errmsg)

/* The following structure is a dynafile name cache entry.
 */
struct s_dynaFileCacheEntry {
	uchar *pName;	/* name currently open, if dynamic name */
	short	fd;		/* name associated with file name in cache */
	time_t	lastUsed;	/* for LRU - last access */
};
typedef struct s_dynaFileCacheEntry dynaFileCacheEntry;


/* globals for default values */
static int iDynaFileCacheSize = 10; /* max cache for dynamic files */
static int fCreateMode = 0644; /* mode to use when creating files */
static int fDirCreateMode = 0644; /* mode to use when creating files */
static int	bFailOnChown;	/* fail if chown fails? */
static uid_t	fileUID;	/* UID to be used for newly created files */
static uid_t	fileGID;	/* GID to be used for newly created files */
static uid_t	dirUID;		/* UID to be used for newly created directories */
static uid_t	dirGID;		/* GID to be used for newly created directories */
static int	bCreateDirs;	/* auto-create directories for dynaFiles: 0 - no, 1 - yes */
static int	bEnableSync = 0;/* enable syncing of files (no dash in front of pathname in conf): 0 - no, 1 - yes */
static uchar	*pszTplName = NULL; /* name of the default template to use */
/* end globals for default values */


typedef struct _instanceData {
	uchar	f_fname[MAXFNAME];/* file or template name (display only) */
	short	fd;		  /* file descriptor for (current) file */
	enum {
		eTypeFILE,
		eTypeTTY,
		eTypeCONSOLE,
		eTypePIPE
	} fileType;	
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
	int	iCurrElt;	/* currently active cache element (-1 = none) */
	int	iCurrCacheSize;	/* currently cache size (1-based) */
	int	iDynaFileCacheSize; /* size of file handle cache */
	/* The cache is implemented as an array. An empty element is indicated
	 * by a NULL pointer. Memory is allocated as needed. The following
	 * pointer points to the overall structure.
	 */
	dynaFileCacheEntry **dynCache;
	off_t	f_sizeLimit;		/* file size limit, 0 = no limit */
	char	*f_sizeLimitCmd;	/* command to carry out when size limit is reached */
} instanceData;


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURERepeatedMsgReduction)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	if(pData->bDynamicName) {
		printf("[dynamic]\n\ttemplate='%s'"
		       "\tfile cache size=%d\n"
		       "\tcreate directories: %s\n"
		       "\tfile owner %d, group %d\n"
		       "\tdirectory owner %d, group %d\n"
		       "\tfail if owner/group can not be set: %s\n",
		        pData->f_fname,
			pData->iDynaFileCacheSize,
			pData->bCreateDirs ? "yes" : "no",
			pData->fileUID, pData->fileGID,
			pData->dirUID, pData->dirGID,
			pData->bFailOnChown ? "yes" : "no"
			);
	} else { /* regular file */
		printf("%s", pData->f_fname);
		if (pData->fd == -1)
			printf(" (unused)");
	}
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
	} else if(iNewVal > 10000) {
		snprintf((char*) errMsg, sizeof(errMsg)/sizeof(uchar),
		         "DynaFileCacheSize maximum is 10,000 (%d given), changed to 10,000.", iNewVal);
		errno = 0;
		errmsg.LogError(0, RS_RET_VAL_OUT_OF_RANGE, "%s", errMsg);
		iRet = RS_RET_VAL_OUT_OF_RANGE;
		iNewVal = 10000;
	}

	iDynaFileCacheSize = iNewVal;
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

	/* this must always be a file, because we can not set a size limit
	 * on a pipe...
	 * rgerhards 2005-06-21: later, this will be a separate type, but let's
	 * emulate things for the time being. When everything runs, we can
	 * extend it...
	 */
	pData->fileType = eTypeFILE;

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
	strncpy((char*) pData->f_fname, (char*) pOch->pszFileTemplate, MAXFNAME);
	pData->f_sizeLimit = pOch->uSizeLimit;
	/* WARNING: It is dangerous "just" to pass the pointer. As we
	 * never rebuild the output channel description, this is acceptable here.
	 */
	pData->f_sizeLimitCmd = (char*) pOch->cmdOnSizeLimit;

	iRet = cflineParseTemplateName(&p, pOMSR, iEntry, iTplOpts,
				       (pszTplName == NULL) ? (uchar*)"RSYSLOG_FileFormat" : pszTplName);

finalize_it:
	RETiRet;
}


/* rgerhards 2005-06-21: Try to resolve a size limit
 * situation. This first runs the command, and then
 * checks if we are still above the treshold.
 * returns 0 if ok, 1 otherwise
 * TODO: consider moving the initial check in here, too
 */
int resolveFileSizeLimit(instanceData *pData)
{
	uchar *pParams;
	uchar *pCmd;
	uchar *p;
	off_t actualFileSize;
	ASSERT(pData != NULL);

	if(pData->f_sizeLimitCmd == NULL)
		return 1; /* nothing we can do in this case... */
	
	/* the execProg() below is probably not great, but at least is is
	 * fairly secure now. Once we change the way file size limits are
	 * handled, we should also revisit how this command is run (and
	 * with which parameters).   rgerhards, 2007-07-20
	 */
	/* we first check if we have command line parameters. We assume this, 
	 * when we have a space in the program name. If we find it, everything after
	 * the space is treated as a single argument.
	 */
	if((pCmd = (uchar*)strdup((char*)pData->f_sizeLimitCmd)) == NULL) {
		/* there is not much we can do - we make syslogd close the file in this case */
		return 1;
		}

	for(p = pCmd ; *p && *p != ' ' ; ++p) {
		/* JUST SKIP */
	}

	if(*p == ' ') {
		*p = '\0'; /* pretend string-end */
		pParams = p+1;
	} else
		pParams = NULL;

	execProg(pCmd, 1, pParams);

	free(pCmd);

	pData->fd = open((char*) pData->f_fname, O_WRONLY|O_APPEND|O_CREAT|O_NOCTTY,
			pData->fCreateMode);

	actualFileSize = lseek(pData->fd, 0, SEEK_END);
	if(actualFileSize >= pData->f_sizeLimit) {
		/* OK, it didn't work out... */
		return 1;
		}

	return 0;
}


/* This function deletes an entry from the dynamic file name
 * cache. A pointer to the cache must be passed in as well
 * as the index of the to-be-deleted entry. This index may
 * point to an unallocated entry, in whcih case the
 * function immediately returns. Parameter bFreeEntry is 1
 * if the entry should be d_free()ed and 0 if not.
 */
static void dynaFileDelCacheEntry(dynaFileCacheEntry **pCache, int iEntry, int bFreeEntry)
{
	ASSERT(pCache != NULL);

	BEGINfunc;

	if(pCache[iEntry] == NULL)
		FINALIZE;

	DBGPRINTF("Removed entry %d for file '%s' from dynaCache.\n", iEntry,
		pCache[iEntry]->pName == NULL ? "[OPEN FAILED]" : (char*)pCache[iEntry]->pName);
	/* if the name is NULL, this is an improperly initilized entry which
	 * needs to be discarded. In this case, neither the file is to be closed
	 * not the name to be freed.
	 */
	if(pCache[iEntry]->pName != NULL) {
		close(pCache[iEntry]->fd);
		d_free(pCache[iEntry]->pName);
		pCache[iEntry]->pName = NULL;
	}

	if(bFreeEntry) {
		d_free(pCache[iEntry]);
		pCache[iEntry] = NULL;
	}

finalize_it:
	ENDfunc;
}


/* This function frees all dynamic file name cache entries and closes the
 * relevant files. Part of Shutdown and HUP processing.
 * rgerhards, 2008-10-23
 */
static inline void dynaFileFreeCacheEntries(instanceData *pData)
{
	register int i;
	ASSERT(pData != NULL);

	BEGINfunc;
	for(i = 0 ; i < pData->iCurrCacheSize ; ++i) {
		dynaFileDelCacheEntry(pData->dynCache, i, 1);
	}
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
 */
static void prepareFile(instanceData *pData, uchar *newFileName)
{
	if(pData->fileType == eTypePIPE) {
		pData->fd = open((char*) pData->f_fname, O_RDWR|O_NONBLOCK);
		FINALIZE; /* we are done in this case */
	}

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
finalize_it:
	if((pData->fd) != 0 && isatty(pData->fd)) {
		DBGPRINTF("file %d is a tty file\n", pData->fd);
		pData->fileType = eTypeTTY;
		untty();
	}
}


/* This function handles dynamic file names. It checks if the
 * requested file name is already open and, if not, does everything
 * needed to switch to the it.
 * Function returns 0 if all went well and non-zero otherwise.
 * On successful return pData->fd must point to the correct file to
 * be written.
 * This is a helper to writeFile(). rgerhards, 2007-07-03
 */
static int prepareDynFile(instanceData *pData, uchar *newFileName, unsigned iMsgOpts)
{
	time_t ttOldest; /* timestamp of oldest element */
	int iOldest;
	int i;
	int iFirstFree;
	dynaFileCacheEntry **pCache;

	ASSERT(pData != NULL);
	ASSERT(newFileName != NULL);

	pCache = pData->dynCache;

	/* first check, if we still have the current file
	 * I *hope* this will be a performance enhancement.
	 */
	if(   (pData->iCurrElt != -1)
	   && !strcmp((char*) newFileName, (char*) pCache[pData->iCurrElt]->pName)) {
	   	/* great, we are all set */
		pCache[pData->iCurrElt]->lastUsed = time(NULL); /* update timestamp for LRU */
		return 0;
	}

	/* ok, no luck. Now let's search the table if we find a matching spot.
	 * While doing so, we also prepare for creation of a new one.
	 */
	iFirstFree = -1; /* not yet found */
	iOldest = 0; /* we assume the first element to be the oldest - that will change as we loop */
	ttOldest = time(NULL) + 1; /* there must always be an older one */
	for(i = 0 ; i < pData->iCurrCacheSize ; ++i) {
		if(pCache[i] == NULL) {
			if(iFirstFree == -1)
				iFirstFree = i;
		} else { /* got an element, let's see if it matches */
			if(!strcmp((char*) newFileName, (char*) pCache[i]->pName)) {
				/* we found our element! */
				pData->fd = pCache[i]->fd;
				pData->iCurrElt = i;
				pCache[i]->lastUsed = time(NULL); /* update timestamp for LRU */
				return 0;
			}
			/* did not find it - so lets keep track of the counters for LRU */
			if(pCache[i]->lastUsed < ttOldest) {
				ttOldest = pCache[i]->lastUsed;
				iOldest = i;
				}
		}
	}

	/* we have not found an entry */
	if(iFirstFree == -1 && (pData->iCurrCacheSize < pData->iDynaFileCacheSize)) {
		/* there is space left, so set it to that index */
		iFirstFree = pData->iCurrCacheSize++;
	}

	if(iFirstFree == -1) {
		dynaFileDelCacheEntry(pCache, iOldest, 0);
		iFirstFree = iOldest; /* this one *is* now free ;) */
	} else {
		/* we need to allocate memory for the cache structure */
		pCache[iFirstFree] = (dynaFileCacheEntry*) calloc(1, sizeof(dynaFileCacheEntry));
		if(pCache[iFirstFree] == NULL) {
			DBGPRINTF("prepareDynfile(): could not alloc mem, discarding this request\n");
			return -1;
		}
	}

	/* Ok, we finally can open the file */
	prepareFile(pData, newFileName);

	/* file is either open now or an error state set */
	if(pData->fd == -1) {
		/* do not report anything if the message is an internally-generated
		 * message. Otherwise, we could run into a never-ending loop. The bad
		 * news is that we also lose errors on startup messages, but so it is.
		 */
		if(iMsgOpts & INTERNAL_MSG) {
			DBGPRINTF("Could not open dynaFile, discarding message\n");
		} else {
			errmsg.LogError(0, NO_ERRCODE, "Could not open dynamic file '%s' - discarding message", (char*)newFileName);
		}
		dynaFileDelCacheEntry(pCache, iFirstFree, 1);
		pData->iCurrElt = -1;
		return -1;
	}

	pCache[iFirstFree]->fd = pData->fd;
	pCache[iFirstFree]->pName = (uchar*)strdup((char*)newFileName); /* TODO: check for NULL (very unlikely) */
	pCache[iFirstFree]->lastUsed = time(NULL);
	pData->iCurrElt = iFirstFree;
	DBGPRINTF("Added new entry %d for file cache, file '%s'.\n", iFirstFree, newFileName);

	return 0;
}


/* rgerhards 2004-11-11: write to a file output. This
 * will be called for all outputs using file semantics,
 * for example also for pipes.
 */
static rsRetVal writeFile(uchar **ppString, unsigned iMsgOpts, instanceData *pData)
{
	off_t actualFileSize;
	DEFiRet;

	ASSERT(pData != NULL);

	/* first check if we have a dynamic file name and, if so,
	 * check if it still is ok or a new file needs to be created
	 */
	if(pData->bDynamicName) {
		if(prepareDynFile(pData, ppString[1], iMsgOpts) != 0)
			ABORT_FINALIZE(RS_RET_ERR);
	} else if(pData->fd == -1) {
		prepareFile(pData, pData->f_fname);
	}

	/* create the message based on format specified */
again:
	/* check if we have a file size limit and, if so,
	 * obey to it.
	 */
	if(pData->f_sizeLimit != 0) {
		actualFileSize = lseek(pData->fd, 0, SEEK_END);
		if(actualFileSize >= pData->f_sizeLimit) {
			char errMsg[256];
			/* for now, we simply disable a file once it is
			 * beyond the maximum size. This is better than having
			 * us aborted by the OS... rgerhards 2005-06-21
			 */
			(void) close(pData->fd);
			/* try to resolve the situation */
			if(resolveFileSizeLimit(pData) != 0) {
				/* didn't work out, so disable... */
				snprintf(errMsg, sizeof(errMsg),
					 "no longer writing to file %s; grown beyond configured file size of %lld bytes, actual size %lld - configured command did not resolve situation",
					 pData->f_fname, (long long) pData->f_sizeLimit, (long long) actualFileSize);
				errno = 0;
				errmsg.LogError(0, RS_RET_DISABLE_ACTION, "%s", errMsg);
				ABORT_FINALIZE(RS_RET_DISABLE_ACTION);
			} else {
				snprintf(errMsg, sizeof(errMsg),
					 "file %s had grown beyond configured file size of %lld bytes, actual size was %lld - configured command resolved situation",
					 pData->f_fname, (long long) pData->f_sizeLimit, (long long) actualFileSize);
				errno = 0;
				errmsg.LogError(0, NO_ERRCODE, "%s", errMsg);
			}
		}
	}

	if (write(pData->fd, ppString[0], strlen((char*)ppString[0])) < 0) {
		int e = errno;

		/* If a named pipe is full, just ignore it for now
		   - mrn 24 May 96 */
		if (pData->fileType == eTypePIPE && e == EAGAIN)
			ABORT_FINALIZE(RS_RET_OK);

		/* If the filesystem is filled up, just ignore
		 * it for now and continue writing when possible
		 * based on patch for sysklogd by Martin Schulze on 2007-05-24
		 */
		if (pData->fileType == eTypeFILE && e == ENOSPC)
			ABORT_FINALIZE(RS_RET_OK);

		(void) close(pData->fd);
		/* Check for EBADF on TTY's due to vhangup()
		 * Linux uses EIO instead (mrn 12 May 96)
		 */
		if((pData->fileType == eTypeTTY || pData->fileType == eTypeCONSOLE)
#ifdef linux
			&& e == EIO) {
#else
			&& e == EBADF) {
#endif
			pData->fd = open((char*) pData->f_fname, O_WRONLY|O_APPEND|O_NOCTTY);
			if (pData->fd < 0) {
				iRet = RS_RET_DISABLE_ACTION;
				errmsg.LogError(0, NO_ERRCODE, "%s", pData->f_fname);
			} else {
				untty();
				goto again;
			}
		} else {
			iRet = RS_RET_DISABLE_ACTION;
			errno = e;
			errmsg.LogError(0, NO_ERRCODE, "%s", pData->f_fname);
		}
	} else if (pData->bSyncFile) {
		fsync(pData->fd);
	}

finalize_it:
	RETiRet;
}


BEGINcreateInstance
CODESTARTcreateInstance
	pData->fd = -1;
ENDcreateInstance


BEGINfreeInstance
CODESTARTfreeInstance
	if(pData->bDynamicName) {
		dynaFileFreeCache(pData);
	} else if(pData->fd != -1)
		close(pData->fd);
ENDfreeInstance


BEGINtryResume
CODESTARTtryResume
ENDtryResume

BEGINdoAction
CODESTARTdoAction
	DBGPRINTF(" (%s)\n", pData->f_fname);
	iRet = writeFile(ppString, iMsgOpts, pData);
ENDdoAction


BEGINparseSelectorAct
CODESTARTparseSelectorAct
	/* yes, the if below is redundant, but I need it now. Will go away as
	 * the code further changes.  -- rgerhards, 2007-07-25
	 */
	if(*p == '$' || *p == '?' || *p == '|' || *p == '/' || *p == '-') {
		if((iRet = createInstance(&pData)) != RS_RET_OK) {
			ENDfunc
			return iRet; /* this can not use RET_iRet! */
		}
	} else {
		/* this is not clean, but we need it for the time being
		 * TODO: remove when cleaning up modularization 
		 */
		ENDfunc
		return RS_RET_CONFLINE_UNPROCESSED;
	}

	if(*p == '-') {
		pData->bSyncFile = 0;
		p++;
	} else {
		pData->bSyncFile = bEnableSync ? 1 : 0;
	}

	pData->f_sizeLimit = 0; /* default value, use outchannels to configure! */

	switch (*p)
	{
        case '$':
		CODE_STD_STRING_REQUESTparseSelectorAct(1)
		/* rgerhards 2005-06-21: this is a special setting for output-channel
		 * definitions. In the long term, this setting will probably replace
		 * anything else, but for the time being we must co-exist with the
		 * traditional mode lines.
		 * rgerhards, 2007-07-24: output-channels will go away. We keep them
		 * for compatibility reasons, but seems to have been a bad idea.
		 */
		if((iRet = cflineParseOutchannel(pData, p, *ppOMSR, 0, OMSR_NO_RQD_TPL_OPTS)) == RS_RET_OK) {
			pData->bDynamicName = 0;
			pData->fCreateMode = fCreateMode; /* preserve current setting */
			pData->fDirCreateMode = fDirCreateMode; /* preserve current setting */
			pData->fd = open((char*) pData->f_fname, O_WRONLY|O_APPEND|O_CREAT|O_NOCTTY,
					 pData->fCreateMode);
		}
		break;

	case '?': /* This is much like a regular file handle, but we need to obtain
		   * a template name. rgerhards, 2007-07-03
		   */
		CODE_STD_STRING_REQUESTparseSelectorAct(2)
		++p; /* eat '?' */
		if((iRet = cflineParseFileName(p, (uchar*) pData->f_fname, *ppOMSR, 0, OMSR_NO_RQD_TPL_OPTS,
				               (pszTplName == NULL) ? (uchar*)"RSYSLOG_FileFormat" : pszTplName))
		   != RS_RET_OK)
			break;
		/* "filename" is actually a template name, we need this as string 1. So let's add it
		 * to the pOMSR. -- rgerhards, 2007-07-27
		 */
		if((iRet = OMSRsetEntry(*ppOMSR, 1, (uchar*)strdup((char*) pData->f_fname), OMSR_NO_RQD_TPL_OPTS)) != RS_RET_OK)
			break;

		pData->bDynamicName = 1;
		pData->iCurrElt = -1;		  /* no current element */
		pData->fCreateMode = fCreateMode; /* freeze current setting */
		pData->fDirCreateMode = fDirCreateMode; /* preserve current setting */
		pData->bCreateDirs = bCreateDirs;
		pData->bFailOnChown = bFailOnChown;
		pData->fileUID = fileUID;
		pData->fileGID = fileGID;
		pData->dirUID = dirUID;
		pData->dirGID = dirGID;
		pData->iDynaFileCacheSize = iDynaFileCacheSize; /* freeze current setting */
		/* we now allocate the cache table. We use calloc() intentionally, as we 
		 * need all pointers to be initialized to NULL pointers.
		 */
		if((pData->dynCache = (dynaFileCacheEntry**)
		    calloc(iDynaFileCacheSize, sizeof(dynaFileCacheEntry*))) == NULL) {
			iRet = RS_RET_OUT_OF_MEMORY;
			DBGPRINTF("Could not allocate memory for dynaFileCache - selector disabled.\n");
		}
		break;

        case '|':
	case '/':
		CODE_STD_STRING_REQUESTparseSelectorAct(1)
		/* rgerhards, 2007-0726: first check if file or pipe */
		if(*p == '|') {
			pData->fileType = eTypePIPE;
			++p;
		} else {
			pData->fileType = eTypeFILE;
		}
		/* rgerhards 2004-11-17: from now, we need to have different
		 * processing, because after the first comma, the template name
		 * to use is specified. So we need to scan for the first coma first
		 * and then look at the rest of the line.
		 */
		if((iRet = cflineParseFileName(p, (uchar*) pData->f_fname, *ppOMSR, 0, OMSR_NO_RQD_TPL_OPTS,
				               (pszTplName == NULL) ? (uchar*)"RSYSLOG_FileFormat" : pszTplName))
		   != RS_RET_OK)
			break;

		pData->bDynamicName = 0;
		pData->fCreateMode = fCreateMode; /* preserve current setting */
		pData->fDirCreateMode = fDirCreateMode;
		pData->bCreateDirs = bCreateDirs;
		pData->bFailOnChown = bFailOnChown;
		pData->fileUID = fileUID;
		pData->fileGID = fileGID;
		pData->dirUID = dirUID;
		pData->dirGID = dirGID;

		prepareFile(pData, pData->f_fname);
		        
	  	if(pData->fd < 0 ) {
			pData->fd = -1;
			DBGPRINTF("Error opening log file: %s\n", pData->f_fname);
			errmsg.LogError(0, NO_ERRCODE, "%s", pData->f_fname);
			break;
		}
		if(strcmp((char*) p, _PATH_CONSOLE) == 0)
			pData->fileType = eTypeCONSOLE;
		break;
	default:
		iRet = RS_RET_CONFLINE_UNPROCESSED;
		break;
	}
CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


/* Reset config variables for this module to default values.
 * rgerhards, 2007-07-17
 */
static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	fileUID = -1;
	fileGID = -1;
	dirUID = -1;
	dirGID = -1;
	bFailOnChown = 1;
	iDynaFileCacheSize = 10;
	fCreateMode = 0644;
	fDirCreateMode = 0644;
	bCreateDirs = 1;
	bEnableSync = 0;
	if(pszTplName != NULL) {
		free(pszTplName);
		pszTplName = NULL;
	}

	return RS_RET_OK;
}


BEGINdoHUP
CODESTARTdoHUP
	if(pData->bDynamicName) {
		dynaFileFreeCacheEntries(pData);
		pData->iCurrElt = -1; /* invalidate current element */
	} else {
		if(pData->fd != -1) {
			close(pData->fd);
			pData->fd = -1;
		}
	}
ENDdoHUP


BEGINmodExit
CODESTARTmodExit
	if(pszTplName != NULL)
		free(pszTplName);
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
CODEqueryEtryPt_doHUP
ENDqueryEtryPt


BEGINmodInit(File)
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"dynafilecachesize", 0, eCmdHdlrInt, (void*) setDynaFileCacheSize, NULL, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"dirowner", 0, eCmdHdlrUID, NULL, &dirUID, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"dirgroup", 0, eCmdHdlrGID, NULL, &dirGID, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"fileowner", 0, eCmdHdlrUID, NULL, &fileUID, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"filegroup", 0, eCmdHdlrGID, NULL, &fileGID, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"dircreatemode", 0, eCmdHdlrFileCreateMode, NULL, &fDirCreateMode, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"filecreatemode", 0, eCmdHdlrFileCreateMode, NULL, &fCreateMode, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"createdirs", 0, eCmdHdlrBinary, NULL, &bCreateDirs, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"failonchownfailure", 0, eCmdHdlrBinary, NULL, &bFailOnChown, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"actionfileenablesync", 0, eCmdHdlrBinary, NULL, &bEnableSync, STD_LOADABLE_MODULE_ID));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionfiledefaulttemplate", 0, eCmdHdlrGetWord, NULL, &pszTplName, NULL));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit
/* vi:set ai:
 */
