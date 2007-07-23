/* omfile.c
 * This is the implementation of the build-in file output module.
 *
 * Handles: F_CONSOLE, F_TTY, F_FILE, F_PIPE
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

#include <unistd.h>
#include <sys/file.h>

#include "rsyslog.h"
#include "syslogd.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "omfile.h"


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
void writeFile(selector_t *f)
{
	off_t actualFileSize;

	assert(f != NULL);

	/* first check if we have a dynamic file name and, if so,
	 * check if it still is ok or a new file needs to be created
	 */
	if(f->f_un.f_file.bDynamicName) {
		if(prepareDynFile(f) != 0)
			return;
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
				f->f_type = F_UNUSED;
				snprintf(errMsg, sizeof(errMsg),
					 "no longer writing to file %s; grown beyond configured file size of %lld bytes, actual size %lld - configured command did not resolve situation",
					 f->f_un.f_file.f_fname, (long long) f->f_un.f_file.f_sizeLimit, (long long) actualFileSize);
				errno = 0;
				logerror(errMsg);
				return;
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
			return;

		/* If the filesystem is filled up, just ignore
		 * it for now and continue writing when possible
		 * based on patch for sysklogd by Martin Schulze on 2007-05-24
		 */
		if (f->f_type == F_FILE && e == ENOSPC)
			return;

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
				f->f_type = F_UNUSED;
				logerror(f->f_un.f_file.f_fname);
			} else {
				untty();
				goto again;
			}
		} else {
			f->f_type = F_UNUSED;
			errno = e;
			logerror(f->f_un.f_file.f_fname);
		}
	} else if (f->f_flags & SYNC_FILE)
		fsync(f->f_file);
}


/* free an instance
 * returns 0 if it succeeds, something else otherwise
 */
int freeInstanceFile(selector_t *f)
{
	assert(f != NULL);
	if(f->f_un.f_file.bDynamicName) {
		dynaFileFreeCache(f);
	} else 
		close(f->f_file);
	return 0;
}


/* call the shell action
 * returns 0 if it succeeds, something else otherwise
 */
int doActionFile(selector_t *f)
{
	assert(f != NULL);

	dprintf(" (%s)\n", f->f_un.f_file.f_fname);
printf("iovUsed address: %x, size %d\n",&f->f_iIovUsed, sizeof(selector_t));
	/* f->f_file == -1 is an indicator that the we couldn't
	 * open the file at startup. For dynaFiles, this is ok,
	 * all others are doomed.
	 */
	if(f->f_un.f_file.bDynamicName || (f->f_file != -1))
		writeFile(f);
	return 0;
}
/*
 * vi:set ai:
 */
