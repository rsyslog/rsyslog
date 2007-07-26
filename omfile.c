/* omfile.c
 * This is the implementation of the build-in file output module.
 *
 * Handles: F_CONSOLE, F_TTY, F_FILE, F_PIPE
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
 * Copyright 2007 Rainer Gerhards and Adiscon GmbH.
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

#include "rsyslog.h"
#include "syslogd.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "outchannel.h"
#include "omfile.h"
#include "module-template.h"

/* internal structures
 */

typedef struct _instanceData {
} instanceData;

BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURERepeatedMsgReduction)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	if(f->f_un.f_file.bDynamicName) {
		printf("[dynamic]\n\ttemplate='%s'\n"
		       "\tfile cache size=%d\n"
		       "\tcreate directories: %s\n"
		       "\tfile owner %d, group %d\n"
		       "\tdirectory owner %d, group %d\n"
		       "\tfail if owner/group can not be set: %s\n",
			f->f_un.f_file.f_fname,
			f->f_un.f_file.iDynaFileCacheSize,
			f->f_un.f_file.bCreateDirs ? "yes" : "no",
			f->f_un.f_file.fileUID, f->f_un.f_file.fileGID,
			f->f_un.f_file.dirUID, f->f_un.f_file.dirGID,
			f->f_un.f_file.bFailOnChown ? "yes" : "no"
			);
	} else { /* regular file */
		printf("%s", f->f_un.f_file.f_fname);
		if (f->f_file == -1)
			printf(" (unused)");
	}
ENDdbgPrintInstInfo


/* Helper to cfline(). Parses a output channel name up until the first
 * comma and then looks for the template specifier. Tries
 * to find that template. Maps the output channel to the 
 * proper filed structure settings. Everything is stored in the
 * filed struct. Over time, the dependency on filed might be
 * removed.
 * rgerhards 2005-06-21
 */
static rsRetVal cflineParseOutchannel(selector_t *f, uchar* p)
{
	rsRetVal iRet = RS_RET_OK;
	size_t i;
	struct outchannel *pOch;
	char szBuf[128];	/* should be more than sufficient */

	/* this must always be a file, because we can not set a size limit
	 * on a pipe...
	 * rgerhards 2005-06-21: later, this will be a separate type, but let's
	 * emulate things for the time being. When everything runs, we can
	 * extend it...
	 */
	f->f_type = F_FILE;

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
		logerror(errMsg);
		return RS_RET_NOT_FOUND;
	}

	/* check if there is a file name in the outchannel... */
	if(pOch->pszFileTemplate == NULL) {
		char errMsg[128];
		errno = 0;
		snprintf(errMsg, sizeof(errMsg)/sizeof(char),
			 "outchannel '%s' has no file name template - ignoring action line",
			 szBuf);
		logerror(errMsg);
		return RS_RET_ERR;
	}

	/* OK, we finally got a correct template. So let's use it... */
	strncpy(f->f_un.f_file.f_fname, pOch->pszFileTemplate, MAXFNAME);
	f->f_un.f_file.f_sizeLimit = pOch->uSizeLimit;
	/* WARNING: It is dangerous "just" to pass the pointer. As we
	 * never rebuild the output channel description, this is acceptable here.
	 */
	f->f_un.f_file.f_sizeLimitCmd = pOch->cmdOnSizeLimit;

	/* back to the input string - now let's look for the template to use
	 * Just as a general precaution, we skip whitespace.
	 */
	while(*p && isspace((int) *p))
		++p;
	if(*p == ';')
		++p; /* eat it */

	if((iRet = cflineParseTemplateName(&p, szBuf,
	                        sizeof(szBuf) / sizeof(char))) == RS_RET_OK) {
		if(szBuf[0] == '\0')	/* no template? */
			strcpy(szBuf, " TradFmt"); /* use default! */

		iRet = cflineSetTemplateAndIOV(f, szBuf);
		
		dprintf("[outchannel]filename: '%s', template: '%s', size: %lu\n", f->f_un.f_file.f_fname, szBuf,
			f->f_un.f_file.f_sizeLimit);
	}

	return(iRet);
}


/* rgerhards 2005-06-21: Try to resolve a size limit
 * situation. This first runs the command, and then
 * checks if we are still above the treshold.
 * returns 0 if ok, 1 otherwise
 * TODO: consider moving the initial check in here, too
 */
int resolveFileSizeLimit(selector_t *f)
{
	uchar *pParams;
	uchar *pCmd;
	uchar *p;
	off_t actualFileSize;
	assert(f != NULL);

	if(f->f_un.f_file.f_sizeLimitCmd == NULL)
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
	if((pCmd = (uchar*)strdup((char*)f->f_un.f_file.f_sizeLimitCmd)) == NULL) {
		/* there is not much we can do - we make syslogd close the file in this case */
		glblHadMemShortage = 1;
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

	f->f_file = open(f->f_un.f_file.f_fname, O_WRONLY|O_APPEND|O_CREAT|O_NOCTTY,
			f->f_un.f_file.fCreateMode);

	actualFileSize = lseek(f->f_file, 0, SEEK_END);
	if(actualFileSize >= f->f_un.f_file.f_sizeLimit) {
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
 * if the entry should be free()ed and 0 if not.
 */
static void dynaFileDelCacheEntry(dynaFileCacheEntry **pCache, int iEntry, int bFreeEntry)
{
	assert(pCache != NULL);

	if(pCache[iEntry] == NULL)
		return;

	dprintf("Removed entry %d for file '%s' from dynaCache.\n", iEntry,
		pCache[iEntry]->pName == NULL ? "[OPEN FAILED]" : (char*)pCache[iEntry]->pName);
	/* if the name is NULL, this is an improperly initilized entry which
	 * needs to be discarded. In this case, neither the file is to be closed
	 * not the name to be freed.
	 */
	if(pCache[iEntry]->pName != NULL) {
		close(pCache[iEntry]->fd);
		free(pCache[iEntry]->pName);
		pCache[iEntry]->pName = NULL;
	}

	if(bFreeEntry) {
		free(pCache[iEntry]);
		pCache[iEntry] = NULL;
	}
}


/* This function frees the dynamic file name cache.
 */
static void dynaFileFreeCache(selector_t *f)
{
	register int i;
	assert(f != NULL);

	for(i = 0 ; i < f->f_un.f_file.iCurrCacheSize ; ++i) {
		dynaFileDelCacheEntry(f->f_un.f_file.dynCache, i, 1);
	}

	free(f->f_un.f_file.dynCache);
}


/* This function handles dynamic file names. It generates a new one
 * based on the current message, checks if that file is already open
 * and, if not, does everything needed to switch to the new one.
 * Function returns 0 if all went well and non-zero otherwise.
 * On successful return f->f_file must point to the correct file to
 * be written.
 * This is a helper to writeFile(). rgerhards, 2007-07-03
 */
static int prepareDynFile(selector_t *f)
{
	uchar *newFileName;
	time_t ttOldest; /* timestamp of oldest element */
	int iOldest;
	int i;
	int iFirstFree;
	dynaFileCacheEntry **pCache;

	assert(f != NULL);
	if((newFileName = tplToString(f->f_un.f_file.pTpl, f->f_pMsg)) == NULL) {
		/* memory shortage - there is nothing we can do to resolve it.
		 * We silently ignore it, this is probably the best we can do.
		 */
		glblHadMemShortage = TRUE;
		dprintf("prepareDynfile(): could not create file name, discarding this request\n");
		return -1;
	}

	pCache = f->f_un.f_file.dynCache;

	/* first check, if we still have the current file
	 * I *hope* this will be a performance enhancement.
	 */
	if(   (f->f_un.f_file.iCurrElt != -1)
	   && !strcmp((char*) newFileName,
	              (char*) pCache[f->f_un.f_file.iCurrElt])) {
	   	/* great, we are all set */
		free(newFileName);
		pCache[f->f_un.f_file.iCurrElt]->lastUsed = time(NULL); /* update timestamp for LRU */
		return 0;
	}

	/* ok, no luck. Now let's search the table if we find a matching spot.
	 * While doing so, we also prepare for creation of a new one.
	 */
	iFirstFree = -1; /* not yet found */
	iOldest = 0; /* we assume the first element to be the oldest - that will change as we loop */
	ttOldest = time(NULL) + 1; /* there must always be an older one */
	for(i = 0 ; i < f->f_un.f_file.iCurrCacheSize ; ++i) {
		if(pCache[i] == NULL) {
			if(iFirstFree == -1)
				iFirstFree = i;
		} else { /* got an element, let's see if it matches */
			if(!strcmp((char*) newFileName, (char*) pCache[i]->pName)) {
				/* we found our element! */
				f->f_file = pCache[i]->fd;
				f->f_un.f_file.iCurrElt = i;
				free(newFileName);
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
	if(iFirstFree == -1 && (f->f_un.f_file.iCurrCacheSize < f->f_un.f_file.iDynaFileCacheSize)) {
		/* there is space left, so set it to that index */
		iFirstFree = f->f_un.f_file.iCurrCacheSize++;
	}

	if(iFirstFree == -1) {
		dynaFileDelCacheEntry(pCache, iOldest, 0);
		iFirstFree = iOldest; /* this one *is* now free ;) */
	} else {
		/* we need to allocate memory for the cache structure */
		pCache[iFirstFree] = (dynaFileCacheEntry*) calloc(1, sizeof(dynaFileCacheEntry));
		if(pCache[iFirstFree] == NULL) {
			glblHadMemShortage = TRUE;
			dprintf("prepareDynfile(): could not alloc mem, discarding this request\n");
			free(newFileName);
			return -1;
		}
	}

	/* Ok, we finally can open the file */
	if(access((char*)newFileName, F_OK) == 0) {
		/* file already exists */
		f->f_file = open((char*) newFileName, O_WRONLY|O_APPEND|O_CREAT|O_NOCTTY,
				f->f_un.f_file.fCreateMode);
	} else {
		/* file does not exist, create it (and eventually parent directories */
		if(f->f_un.f_file.bCreateDirs) {
			/* we fist need to create parent dirs if they are missing
			 * We do not report any errors here ourselfs but let the code
			 * fall through to error handler below.
			 */
			if(makeFileParentDirs(newFileName, strlen((char*)newFileName),
			     f->f_un.f_file.fDirCreateMode, f->f_un.f_file.dirUID,
			     f->f_un.f_file.dirGID, f->f_un.f_file.bFailOnChown) == 0) {
				f->f_file = open((char*) newFileName, O_WRONLY|O_APPEND|O_CREAT|O_NOCTTY,
						f->f_un.f_file.fCreateMode);
				if(f->f_file != -1) {
					/* check and set uid/gid */
					if(f->f_un.f_file.fileUID != (uid_t)-1 || f->f_un.f_file.fileGID != (gid_t) -1) {
						/* we need to set owner/group */
						if(fchown(f->f_file, f->f_un.f_file.fileUID,
						          f->f_un.f_file.fileGID) != 0) {
							if(f->f_un.f_file.bFailOnChown) {
								int eSave = errno;
								close(f->f_file);
								f->f_file = -1;
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
	}

	/* file is either open now or an error state set */
	if(f->f_file == -1) {
		/* do not report anything if the message is an internally-generated
		 * message. Otherwise, we could run into a never-ending loop. The bad
		 * news is that we also lose errors on startup messages, but so it is.
		 */
		if(f->f_pMsg->msgFlags & INTERNAL_MSG)
			dprintf("Could not open dynaFile, discarding message\n");
		else
			logerrorSz("Could not open dynamic file '%s' - discarding message", (char*)newFileName);
		free(newFileName);
		dynaFileDelCacheEntry(pCache, iFirstFree, 1);
		return -1;
	}

	pCache[iFirstFree]->fd = f->f_file;
	pCache[iFirstFree]->pName = newFileName;
	pCache[iFirstFree]->lastUsed = time(NULL);
	f->f_un.f_file.iCurrElt = iFirstFree;
	dprintf("Added new entry %d for file cache, file '%s'.\n",
		iFirstFree, newFileName);

	return 0;
}


/* rgerhards 2004-11-11: write to a file output. This
 * will be called for all outputs using file semantics,
 * for example also for pipes.
 */
static rsRetVal writeFile(selector_t *f)
{
	off_t actualFileSize;
	rsRetVal iRet = RS_RET_OK;

	assert(f != NULL);

	/* first check if we have a dynamic file name and, if so,
	 * check if it still is ok or a new file needs to be created
	 */
	if(f->f_un.f_file.bDynamicName) {
		if(prepareDynFile(f) != 0)
			return RS_RET_ERR;
	}

	/* create the message based on format specified */
	iovCreate(f);
again:
	/* check if we have a file size limit and, if so,
	 * obey to it.
	 */
	if(f->f_un.f_file.f_sizeLimit != 0) {
		actualFileSize = lseek(f->f_file, 0, SEEK_END);
		if(actualFileSize >= f->f_un.f_file.f_sizeLimit) {
			char errMsg[256];
			/* for now, we simply disable a file once it is
			 * beyond the maximum size. This is better than having
			 * us aborted by the OS... rgerhards 2005-06-21
			 */
			(void) close(f->f_file);
			/* try to resolve the situation */
			if(resolveFileSizeLimit(f) != 0) {
				/* didn't work out, so disable... */
				snprintf(errMsg, sizeof(errMsg),
					 "no longer writing to file %s; grown beyond configured file size of %lld bytes, actual size %lld - configured command did not resolve situation",
					 f->f_un.f_file.f_fname, (long long) f->f_un.f_file.f_sizeLimit, (long long) actualFileSize);
				errno = 0;
				logerror(errMsg);
				return RS_RET_DISABLE_ACTION;
			} else {
				snprintf(errMsg, sizeof(errMsg),
					 "file %s had grown beyond configured file size of %lld bytes, actual size was %lld - configured command resolved situation",
					 f->f_un.f_file.f_fname, (long long) f->f_un.f_file.f_sizeLimit, (long long) actualFileSize);
				errno = 0;
				logerror(errMsg);
			}
		}
	}

	if (writev(f->f_file, f->f_iov, f->f_iIovUsed) < 0) {
		int e = errno;

		/* If a named pipe is full, just ignore it for now
		   - mrn 24 May 96 */
		if (f->f_type == F_PIPE && e == EAGAIN)
			return RS_RET_OK;

		/* If the filesystem is filled up, just ignore
		 * it for now and continue writing when possible
		 * based on patch for sysklogd by Martin Schulze on 2007-05-24
		 */
		if (f->f_type == F_FILE && e == ENOSPC)
			return RS_RET_OK;

		(void) close(f->f_file);
		/*
		 * Check for EBADF on TTY's due to vhangup()
		 * Linux uses EIO instead (mrn 12 May 96)
		 */
		if ((f->f_type == F_TTY || f->f_type == F_CONSOLE)
#ifdef linux
			&& e == EIO) {
#else
			&& e == EBADF) {
#endif
			f->f_file = open(f->f_un.f_file.f_fname, O_WRONLY|O_APPEND|O_NOCTTY);
			if (f->f_file < 0) {
				iRet = RS_RET_DISABLE_ACTION;
				logerror(f->f_un.f_file.f_fname);
			} else {
				untty();
				goto again;
			}
		} else {
			iRet = RS_RET_DISABLE_ACTION;
			errno = e;
			logerror(f->f_un.f_file.f_fname);
		}
	} else if (f->f_flags & SYNC_FILE)
		fsync(f->f_file);
	return(iRet);
}


BEGINcreateInstance
CODESTARTcreateInstance
ENDcreateInstance


BEGINfreeInstance
CODESTARTfreeInstance
	if(f->f_un.f_file.bDynamicName) {
		dynaFileFreeCache(f);
	} else 
		close(f->f_file);
ENDfreeInstance


BEGINonSelectReadyWrite
CODESTARTonSelectReadyWrite
ENDonSelectReadyWrite


BEGINgetWriteFDForSelect
CODESTARTgetWriteFDForSelect
ENDgetWriteFDForSelect


BEGINdoAction
CODESTARTdoAction
	dprintf(" (%s)\n", f->f_un.f_file.f_fname);
	/* f->f_file == -1 is an indicator that the we couldn't
	 * open the file at startup. For dynaFiles, this is ok,
	 * all others are doomed.
	 */
	if(f->f_un.f_file.bDynamicName || (f->f_file != -1))
		iRet = writeFile(f);
ENDdoAction


BEGINparseSelectorAct
	int syncfile;
CODESTARTparseSelectorAct
	p = *pp;

	if (*p == '-') {
		syncfile = 0;
		p++;
	} else
		syncfile = 1;

	f->f_un.f_file.f_sizeLimit = 0; /* default value, use outchannels to configure! */

	/* yes, the if below is redundant, but I need it now. Will go away as
	 * the code further changes.  -- rgerhards, 2007-07-25
	 */
	if(*p == '$' || *p == '?' || *p == '|' || *p == '/') {
		if((iRet = createInstance(&pData)) != RS_RET_OK)
			return iRet;
	}

	switch (*p)
	{
        case '$':
		/* rgerhards 2005-06-21: this is a special setting for output-channel
		 * definitions. In the long term, this setting will probably replace
		 * anything else, but for the time being we must co-exist with the
		 * traditional mode lines.
		 * rgerhards, 2007-07-24: output-channels will go away. We keep them
		 * for compatibility reasons, but seems to have been a bad idea.
		 */
		if((iRet = cflineParseOutchannel(f, p)) == RS_RET_OK) {
			f->f_un.f_file.bDynamicName = 0;
			f->f_un.f_file.fCreateMode = fCreateMode; /* preserve current setting */
			f->f_un.f_file.fDirCreateMode = fDirCreateMode; /* preserve current setting */
			f->f_file = open(f->f_un.f_file.f_fname, O_WRONLY|O_APPEND|O_CREAT|O_NOCTTY,
					 f->f_un.f_file.fCreateMode);
		}
		break;

	case '?': /* This is much like a regular file handle, but we need to obtain
		   * a template name. rgerhards, 2007-07-03
		   */
		++p; /* eat '?' */
		if((iRet = cflineParseFileName(f, p, (uchar*) f->f_un.f_file.f_fname)) != RS_RET_OK)
			break;
		f->f_un.f_file.pTpl = tplFind((char*)f->f_un.f_file.f_fname,
					       strlen((char*) f->f_un.f_file.f_fname));
		if(f->f_un.f_file.pTpl == NULL) {
			logerrorSz("Template '%s' not found - dynaFile deactivated.", f->f_un.f_file.f_fname);
			iRet = RS_RET_NOT_FOUND; /* that's it... :( */
			break;
		}

		if(syncfile)
			f->f_flags |= SYNC_FILE;
		f->f_un.f_file.bDynamicName = 1;
		f->f_un.f_file.iCurrElt = -1;		  /* no current element */
		f->f_un.f_file.fCreateMode = fCreateMode; /* freeze current setting */
		f->f_un.f_file.fDirCreateMode = fDirCreateMode; /* preserve current setting */
		f->f_un.f_file.bCreateDirs = bCreateDirs;
		f->f_un.f_file.bFailOnChown = bFailOnChown;
		f->f_un.f_file.fileUID = fileUID;
		f->f_un.f_file.fileGID = fileGID;
		f->f_un.f_file.dirUID = dirUID;
		f->f_un.f_file.dirGID = dirGID;
		f->f_un.f_file.iDynaFileCacheSize = iDynaFileCacheSize; /* freeze current setting */
		/* we now allocate the cache table. We use calloc() intentionally, as we 
		 * need all pointers to be initialized to NULL pointers.
		 */
		if((f->f_un.f_file.dynCache = (dynaFileCacheEntry**)
		    calloc(iDynaFileCacheSize, sizeof(dynaFileCacheEntry*))) == NULL) {
			iRet = RS_RET_OUT_OF_MEMORY;
			dprintf("Could not allocate memory for dynaFileCache - selector disabled.\n");
		}
		break;

        case '|':
	case '/':
		/* rgerhards 2004-11-17: from now, we need to have different
		 * processing, because after the first comma, the template name
		 * to use is specified. So we need to scan for the first coma first
		 * and then look at the rest of the line.
		 */
		if((iRet = cflineParseFileName(f, p, (uchar*) f->f_un.f_file.f_fname)) != RS_RET_OK)
			break;

		if(syncfile)
			f->f_flags |= SYNC_FILE;
		f->f_un.f_file.bDynamicName = 0;
		f->f_un.f_file.fCreateMode = fCreateMode; /* preserve current setting */
		if(f->f_type == F_PIPE) {
			f->f_file = open(f->f_un.f_file.f_fname, O_RDWR|O_NONBLOCK);
	        } else {
			f->f_file = open(f->f_un.f_file.f_fname, O_WRONLY|O_APPEND|O_CREAT|O_NOCTTY,
					 f->f_un.f_file.fCreateMode);
		}
		        
	  	if ( f->f_file < 0 ){
			f->f_file = -1;
			dprintf("Error opening log file: %s\n", f->f_un.f_file.f_fname);
			logerror(f->f_un.f_file.f_fname);
			break;
		}
		if (isatty(f->f_file)) {
			f->f_type = F_TTY;
			untty();
		}
		if (strcmp((char*) p, ctty) == 0)
			f->f_type = F_CONSOLE;
		break;
	default:
		iRet = RS_RET_CONFLINE_UNPROCESSED;
		break;
	}
ENDparseSelectorAct


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
ENDqueryEtryPt


BEGINmodInit(File)
CODESTARTmodInit
	*ipIFVersProvided = 1; /* so far, we only support the initial definition */
ENDmodInit


/*
 * vi:set ai:
 */
