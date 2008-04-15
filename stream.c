//TODO: O_TRUC mode!
/* The serial stream class.
 *
 * A serial stream provides serial data access. In theory, serial streams
 * can be implemented via a number of methods (e.g. files or in-memory
 * streams). In practice, there currently only exist the file type (aka
 * "driver").
 *
 * File begun on 2008-01-09 by RGerhards
 *
 * Copyright 2008 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
 *
 * The rsyslog runtime library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The rsyslog runtime library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the rsyslog runtime library.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 * A copy of the LGPL can be found in the file "COPYING.LESSER" in this distribution.
 */
#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>	 /* required for HP UX */
#include <errno.h>

#include "rsyslog.h"
#include "syslogd.h"
#include "stringbuf.h"
#include "srUtils.h"
#include "obj.h"
#include "stream.h"

/* static data */
DEFobjStaticHelpers

/* methods */

/* first, we define type-specific handlers. The provide a generic functionality,
 * but for this specific type of strm. The mapping to these handlers happens during
 * strm construction. Later on, handlers are called by pointers present in the
 * strm instance object.
 */

/* open a strm file
 * It is OK to call this function when the stream is already open. In that
 * case, it returns immediately with RS_RET_OK
 */
static rsRetVal strmOpenFile(strm_t *pThis)
{
	DEFiRet;
	int iFlags;

	ASSERT(pThis != NULL);
	ASSERT(pThis->tOperationsMode == STREAMMODE_READ || pThis->tOperationsMode == STREAMMODE_WRITE);

	if(pThis->fd != -1)
		ABORT_FINALIZE(RS_RET_OK);

	if(pThis->pszFName == NULL)
		ABORT_FINALIZE(RS_RET_FILE_PREFIX_MISSING);

	if(pThis->sType == STREAMTYPE_FILE_CIRCULAR) {
		CHKiRet(genFileName(&pThis->pszCurrFName, pThis->pszDir, pThis->lenDir,
				    pThis->pszFName, pThis->lenFName, pThis->iCurrFNum, pThis->iFileNumDigits));
	} else {
		if(pThis->pszDir == NULL) {
			if((pThis->pszCurrFName = (uchar*) strdup((char*) pThis->pszFName)) == NULL)
				ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
		} else {
			CHKiRet(genFileName(&pThis->pszCurrFName, pThis->pszDir, pThis->lenDir,
					    pThis->pszFName, pThis->lenFName, -1, 0));
		}
	}

	/* compute which flags we need to provide to open */
	if(pThis->tOperationsMode == STREAMMODE_READ)
		iFlags = O_RDONLY;
	else
		iFlags = O_WRONLY | O_CREAT;

	iFlags |= pThis->iAddtlOpenFlags;

	pThis->fd = open((char*)pThis->pszCurrFName, iFlags, pThis->tOpenMode);
	if(pThis->fd == -1) {
		int ierrnoSave = errno;
		dbgoprint((obj_t*) pThis, "open error %d, file '%s'\n", errno, pThis->pszCurrFName);
		if(ierrnoSave == ENOENT)
			ABORT_FINALIZE(RS_RET_FILE_NOT_FOUND);
		else
			ABORT_FINALIZE(RS_RET_IO_ERROR);
	}

	pThis->iCurrOffs = 0;

	dbgoprint((obj_t*) pThis, "opened file '%s' for %s (0x%x) as %d\n", pThis->pszCurrFName,
		  (pThis->tOperationsMode == STREAMMODE_READ) ? "READ" : "WRITE", iFlags, pThis->fd);

finalize_it:
	RETiRet;
}


/* close a strm file
 * Note that the bDeleteOnClose flag is honored. If it is set, the file will be
 * deleted after close. This is in support for the qRead thread.
 */
static rsRetVal strmCloseFile(strm_t *pThis)
{
	DEFiRet;

	ASSERT(pThis != NULL);
	ASSERT(pThis->fd != -1);
	dbgoprint((obj_t*) pThis, "file %d closing\n", pThis->fd);

	if(pThis->tOperationsMode == STREAMMODE_WRITE)
		strmFlush(pThis);

	close(pThis->fd); // TODO: error check
	pThis->fd = -1;

	if(pThis->bDeleteOnClose) {
		unlink((char*) pThis->pszCurrFName); // TODO: check returncode
	}

	pThis->iCurrOffs = 0;	/* we are back at begin of file */
	if(pThis->pszCurrFName != NULL) {
		free(pThis->pszCurrFName);	/* no longer needed in any case (just for open) */
		pThis->pszCurrFName = NULL;
	}

	RETiRet;
}


/* switch to next strm file
 * This method must only be called if we are in a multi-file mode!
 */
static rsRetVal
strmNextFile(strm_t *pThis)
{
	DEFiRet;

	ASSERT(pThis != NULL);
	ASSERT(pThis->iMaxFiles != 0);
	ASSERT(pThis->fd != -1);

	CHKiRet(strmCloseFile(pThis));

	/* we do modulo operation to ensure we obey the iMaxFile property. This will always
	 * result in a file number lower than iMaxFile, so it if wraps, the name is back to
	 * 0, which results in the first file being overwritten. Not desired for queues, so
	 * make sure their iMaxFiles is large enough. But it is well-desired for other
	 * use cases, e.g. a circular output log file. -- rgerhards, 2008-01-10
	 */
	pThis->iCurrFNum = (pThis->iCurrFNum + 1) % pThis->iMaxFiles;

finalize_it:
	RETiRet;
}


/* handle the eof case for monitored files.
 * If we are monitoring a file, someone may have rotated it. In this case, we
 * also need to close it and reopen it under the same name.
 * rgerhards, 2008-02-13
 */
static rsRetVal
strmHandleEOFMonitor(strm_t *pThis)
{
	DEFiRet;
	struct stat statOpen;
	struct stat statName;

	ISOBJ_TYPE_assert(pThis, strm);
	/* find inodes of both current descriptor as well as file now in file
	 * system. If they are different, the file has been rotated (or
	 * otherwise rewritten). We also check the size, because the inode
	 * does not change if the file is truncated (this, BTW, is also a case
	 * where we actually loose log lines, because we can not do anything
	 * against truncation...). We do NOT rely on the time of last
	 * modificaton because that may not be available under all
	 * circumstances. -- rgerhards, 2008-02-13
	 */
	if(fstat(pThis->fd, &statOpen) == -1)
		ABORT_FINALIZE(RS_RET_IO_ERROR);
	if(stat((char*) pThis->pszCurrFName, &statName) == -1)
		ABORT_FINALIZE(RS_RET_IO_ERROR);
	if(statOpen.st_ino == statName.st_ino && pThis->iCurrOffs == statName.st_size) {
		ABORT_FINALIZE(RS_RET_EOF);
	} else {
		/* we had a file change! */
		CHKiRet(strmCloseFile(pThis));
		CHKiRet(strmOpenFile(pThis));
	}

finalize_it:
	RETiRet;
}


/* handle the EOF case of a stream
 * The EOF case is somewhat complicated, as the proper action depends on the
 * mode the stream is in. If there are multiple files (circular logs, most
 * important use case is queue files!), we need to close the current file and
 * try to open the next one.
 * rgerhards, 2008-02-13
 */
static rsRetVal
strmHandleEOF(strm_t *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, strm);
	switch(pThis->sType) {
		case STREAMTYPE_FILE_SINGLE:
			ABORT_FINALIZE(RS_RET_EOF);
			break;
		case STREAMTYPE_FILE_CIRCULAR:
			/* we have multiple files and need to switch to the next one */
			/* TODO: think about emulating EOF in this case (not yet needed) */
#if 0
			if(pThis->iMaxFiles == 0) /* TODO: why do we need this? ;) */
				ABORT_FINALIZE(RS_RET_EOF);
#endif
			dbgoprint((obj_t*) pThis, "file %d EOF\n", pThis->fd);
			CHKiRet(strmNextFile(pThis));
			break;
		case STREAMTYPE_FILE_MONITOR:
			CHKiRet(strmHandleEOFMonitor(pThis));
			break;
	}

finalize_it:
	RETiRet;
}

/* read the next buffer from disk
 * rgerhards, 2008-02-13
 */
static rsRetVal
strmReadBuf(strm_t *pThis)
{
	DEFiRet;
	int bRun;
	long iLenRead;

	ISOBJ_TYPE_assert(pThis, strm);
	/* We need to try read at least twice because we may run into EOF and need to switch files. */
	bRun = 1;
	while(bRun) {
		/* first check if we need to (re)open the file. We may have switched to a new one in
		 * circular mode or it may have been rewritten (rotated) if we monitor a file
		 * rgerhards, 2008-02-13
		 */
		CHKiRet(strmOpenFile(pThis));
		iLenRead = read(pThis->fd, pThis->pIOBuf, pThis->sIOBufSize);
		dbgoprint((obj_t*) pThis, "file %d read %ld bytes\n", pThis->fd, iLenRead);
		if(iLenRead == 0) {
			CHKiRet(strmHandleEOF(pThis));
		} else if(iLenRead < 0)
			ABORT_FINALIZE(RS_RET_IO_ERROR);
		else { /* good read */
			pThis->iBufPtrMax = iLenRead;
			bRun = 0;	/* exit loop */
		}
	}
	/* if we reach this point, we had a good read */
	pThis->iBufPtr = 0;

finalize_it:
	RETiRet;
}


/* logically "read" a character from a file. What actually happens is that
 * data is taken from the buffer. Only if the buffer is full, data is read 
 * directly from file. In that case, a read is performed blockwise.
 * rgerhards, 2008-01-07
 * NOTE: needs to be enhanced to support sticking with a strm entry (if not
 * deleted).
 */
rsRetVal strmReadChar(strm_t *pThis, uchar *pC)
{
	DEFiRet;
	
	ASSERT(pThis != NULL);
	ASSERT(pC != NULL);

	/* DEV debug only: dbgoprint((obj_t*) pThis, "strmRead index %d, max %d\n", pThis->iBufPtr, pThis->iBufPtrMax); */
	if(pThis->iUngetC != -1) {	/* do we have an "unread" char that we need to provide? */
		*pC = pThis->iUngetC;
		++pThis->iCurrOffs; /* one more octet read */
		pThis->iUngetC = -1;
		ABORT_FINALIZE(RS_RET_OK);
	}
	
	/* do we need to obtain a new buffer? */
	if(pThis->iBufPtr >= pThis->iBufPtrMax) {
		CHKiRet(strmReadBuf(pThis));
	}

	/* if we reach this point, we have data available in the buffer */

	*pC = pThis->pIOBuf[pThis->iBufPtr++];
	++pThis->iCurrOffs; /* one more octet read */

finalize_it:
	RETiRet;
}


/* unget a single character just like ungetc(). As with that call, there is only a single
 * character buffering capability.
 * rgerhards, 2008-01-07
 */
rsRetVal strmUnreadChar(strm_t *pThis, uchar c)
{
	ASSERT(pThis != NULL);
	ASSERT(pThis->iUngetC == -1);
	pThis->iUngetC = c;
	--pThis->iCurrOffs; /* one less octet read - NOTE: this can cause problems if we got a file change
	and immediately do an unread and the file is on a buffer boundary and the stream is then persisted.
	With the queue, this can not happen as an Unread is only done on record begin, which is never split
	accross files. For other cases we accept the very remote risk. -- rgerhards, 2008-01-12 */

	return RS_RET_OK;
}


/* read a line from a strm file. A line is terminated by LF. The LF is read, but it
 * is not returned in the buffer (it is discared). The caller is responsible for
 * destruction of the returned CStr object! -- rgerhards, 2008-01-07
 * rgerhards, 2008-03-27: I now use the ppCStr directly, without any interim
 * string pointer. The reason is that this function my be called by inputs, which
 * are pthread_killed() upon termination. So if we use their native pointer, they
 * can cleanup (but only then).
 */
rsRetVal
strmReadLine(strm_t *pThis, cstr_t **ppCStr)
{
	DEFiRet;
	uchar c;

	ASSERT(pThis != NULL);
	ASSERT(ppCStr != NULL);

	CHKiRet(rsCStrConstruct(ppCStr));

	/* now read the line */
	CHKiRet(strmReadChar(pThis, &c));
	while(c != '\n') {
		CHKiRet(rsCStrAppendChar(*ppCStr, c));
		CHKiRet(strmReadChar(pThis, &c));
	}
	CHKiRet(rsCStrFinish(*ppCStr));

finalize_it:
	if(iRet != RS_RET_OK && *ppCStr != NULL)
		rsCStrDestruct(ppCStr);

	RETiRet;
}


/* Standard-Constructor for the strm object
 */
BEGINobjConstruct(strm) /* be sure to specify the object type also in END macro! */
	pThis->iCurrFNum = 1;
	pThis->fd = -1;
	pThis->iUngetC = -1;
	pThis->sType = STREAMTYPE_FILE_SINGLE;
	pThis->sIOBufSize = glblGetIOBufSize();
	pThis->tOpenMode = 0600; /* TODO: make configurable */
ENDobjConstruct(strm)


/* ConstructionFinalizer
 * rgerhards, 2008-01-09
 */
rsRetVal strmConstructFinalize(strm_t *pThis)
{
	DEFiRet;

	ASSERT(pThis != NULL);

	if(pThis->pIOBuf == NULL) { /* allocate our io buffer in case we have not yet */
		if((pThis->pIOBuf = (uchar*) malloc(sizeof(uchar) * pThis->sIOBufSize)) == NULL)
			ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
		pThis->iBufPtrMax = 0; /* results in immediate read request */
	}

finalize_it:
	RETiRet;
}


/* destructor for the strm object */
BEGINobjDestruct(strm) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(strm)
	if(pThis->tOperationsMode == STREAMMODE_WRITE)
		strmFlush(pThis);

	/* ... then free resources */
	if(pThis->fd != -1)
		strmCloseFile(pThis);

	if(pThis->pszDir != NULL)
		free(pThis->pszDir);
	if(pThis->pIOBuf != NULL)
		free(pThis->pIOBuf);
	if(pThis->pszCurrFName != NULL)
		free(pThis->pszCurrFName);
	if(pThis->pszFName != NULL)
		free(pThis->pszFName);
ENDobjDestruct(strm)


/* check if we need to open a new file (in output mode only).
 * The decision is based on file size AND record delimition state.
 * This method may also be called on a closed file, in which case
 * it immediately returns.
 */
static rsRetVal strmCheckNextOutputFile(strm_t *pThis)
{
	DEFiRet;

	if(pThis->fd == -1)
		FINALIZE;

	if(pThis->iCurrOffs >= pThis->iMaxFileSize) {
		dbgoprint((obj_t*) pThis, "max file size %ld reached for %d, now %ld - starting new file\n",
			  (long) pThis->iMaxFileSize, pThis->fd, (long) pThis->iCurrOffs);
		CHKiRet(strmNextFile(pThis));
	}

finalize_it:
	RETiRet;
}

/* write memory buffer to a stream object.
 * To support direct writes of large objects, this method may be called
 * with a buffer pointing to some region other than the stream buffer itself.
 * However, in that case the stream buffer must be empty (strmFlush() has to
 * be called before), because we would otherwise mess up with the sequence
 * inside the stream. -- rgerhards, 2008-01-10
 */
static rsRetVal strmWriteInternal(strm_t *pThis, uchar *pBuf, size_t lenBuf)
{
	DEFiRet;
	int iWritten;

	ASSERT(pThis != NULL);
	ASSERT(pBuf == pThis->pIOBuf || pThis->iBufPtr == 0);

	if(pThis->fd == -1)
		CHKiRet(strmOpenFile(pThis));

	iWritten = write(pThis->fd, pBuf, lenBuf);
	dbgoprint((obj_t*) pThis, "file %d write wrote %d bytes\n", pThis->fd, iWritten);
	/* TODO: handle error case -- rgerhards, 2008-01-07 */

	/* Now indicate buffer empty again. We do this in any case, because there
	 * is no way we could react more intelligently to an error during write.
	 * This MUST be done BEFORE strCheckNextOutputFile(), otherwise we have an
	 * endless loop. We reset the buffer pointer also in finalize_it - this is
	 * necessary if we run into problems. Not resetting it would again cause an
	 * endless loop. So it is better to loose some data (which also justifies
	 * duplicating that code, too...) -- rgerhards, 2008-01-10
	 */
	pThis->iBufPtr = 0;
	pThis->iCurrOffs += iWritten;
	/* update user counter, if provided */
	if(pThis->pUsrWCntr != NULL)
		*pThis->pUsrWCntr += iWritten;

	if(pThis->sType == STREAMTYPE_FILE_CIRCULAR)
		CHKiRet(strmCheckNextOutputFile(pThis));

finalize_it:
	pThis->iBufPtr = 0; /* see comment above */

	RETiRet;
}


/* flush stream output buffer to persistent storage. This can be called at any time
 * and is automatically called when the output buffer is full.
 * rgerhards, 2008-01-10
 */
rsRetVal strmFlush(strm_t *pThis)
{
	DEFiRet;

	ASSERT(pThis != NULL);
	dbgoprint((obj_t*) pThis, "file %d flush, buflen %ld\n", pThis->fd, (long) pThis->iBufPtr);

	if(pThis->tOperationsMode == STREAMMODE_WRITE && pThis->iBufPtr > 0) {
		iRet = strmWriteInternal(pThis, pThis->pIOBuf, pThis->iBufPtr);
	}

	RETiRet;
}


/* seek a stream to a specific location. Pending writes are flushed, read data
 * is invalidated.
 * rgerhards, 2008-01-12
 */
static rsRetVal strmSeek(strm_t *pThis, off_t offs)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, strm);

	if(pThis->fd == -1)
		strmOpenFile(pThis);
	else
		strmFlush(pThis);
	int i;
	dbgoprint((obj_t*) pThis, "file %d seek, pos %ld\n", pThis->fd, (long) offs);
	i = lseek(pThis->fd, offs, SEEK_SET); // TODO: check error!
	pThis->iCurrOffs = offs; /* we are now at *this* offset */
	pThis->iBufPtr = 0; /* buffer invalidated */

	RETiRet;
}


/* seek to current offset. This is primarily a helper to readjust the OS file
 * pointer after a strm object has been deserialized.
 */
rsRetVal strmSeekCurrOffs(strm_t *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, strm);

	iRet = strmSeek(pThis, pThis->iCurrOffs);
	RETiRet;
}


/* write a *single* character to a stream object -- rgerhards, 2008-01-10
 */
rsRetVal strmWriteChar(strm_t *pThis, uchar c)
{
	DEFiRet;

	ASSERT(pThis != NULL);

	/* if the buffer is full, we need to flush before we can write */
	if(pThis->iBufPtr == pThis->sIOBufSize) {
		CHKiRet(strmFlush(pThis));
	}
	/* we now always have space for one character, so we simply copy it */
	*(pThis->pIOBuf + pThis->iBufPtr) = c;
	pThis->iBufPtr++;

finalize_it:
	RETiRet;
}


/* write an integer value (actually a long) to a stream object */
rsRetVal strmWriteLong(strm_t *pThis, long i)
{
	DEFiRet;
	uchar szBuf[32];

	ASSERT(pThis != NULL);

	CHKiRet(srUtilItoA((char*)szBuf, sizeof(szBuf), i));
	CHKiRet(strmWrite(pThis, szBuf, strlen((char*)szBuf)));

finalize_it:
	RETiRet;
}


/* write memory buffer to a stream object
 */
rsRetVal strmWrite(strm_t *pThis, uchar *pBuf, size_t lenBuf)
{
	DEFiRet;
	size_t iPartial;

	ASSERT(pThis != NULL);
	ASSERT(pBuf != NULL);

	/* check if the to-be-written data is larger than our buffer size */
	if(lenBuf >= pThis->sIOBufSize) {
		/* it is - so we do a direct write, that is most efficient.
		 * TODO: is it really? think about disk block sizes!
		 */
		CHKiRet(strmFlush(pThis)); /* we need to flush first!!! */
		CHKiRet(strmWriteInternal(pThis, pBuf, lenBuf));
	} else {
		/* data fits into a buffer - we just need to see if it
		 * fits into the current buffer...
		 */
		if(pThis->iBufPtr + lenBuf > pThis->sIOBufSize) {
			/* nope, so we must split it */
			iPartial = pThis->sIOBufSize - pThis->iBufPtr; /* this fits in current buf */
			if(iPartial > 0) { /* the buffer was exactly full, can not write anything! */
				memcpy(pThis->pIOBuf + pThis->iBufPtr, pBuf, iPartial);
				pThis->iBufPtr += iPartial;
			}
			CHKiRet(strmFlush(pThis)); /* get a new buffer for rest of data */
			memcpy(pThis->pIOBuf, pBuf + iPartial, lenBuf - iPartial);
			pThis->iBufPtr = lenBuf - iPartial;
		} else {
			/* we have space, so we simply copy over the string */
			memcpy(pThis->pIOBuf + pThis->iBufPtr, pBuf, lenBuf);
			pThis->iBufPtr += lenBuf;
		}
	}

finalize_it:
	RETiRet;
}


/* property set methods */
/* simple ones first */
DEFpropSetMeth(strm, bDeleteOnClose, int)
DEFpropSetMeth(strm, iMaxFileSize, int)
DEFpropSetMeth(strm, iFileNumDigits, int)
DEFpropSetMeth(strm, tOperationsMode, int)
DEFpropSetMeth(strm, tOpenMode, mode_t)
DEFpropSetMeth(strm, sType, strmType_t);

rsRetVal strmSetiMaxFiles(strm_t *pThis, int iNewVal)
{
	pThis->iMaxFiles = iNewVal;
	pThis->iFileNumDigits = getNumberDigits(iNewVal);
	return RS_RET_OK;
}

rsRetVal strmSetiAddtlOpenFlags(strm_t *pThis, int iNewVal)
{
	DEFiRet;

	if(iNewVal & O_APPEND)
		ABORT_FINALIZE(RS_RET_PARAM_ERROR);

	pThis->iAddtlOpenFlags = iNewVal;

finalize_it:
	RETiRet;
}


/* set the stream's file prefix
 * The passed-in string is duplicated. So if the caller does not need
 * it any longer, it must free it.
 * rgerhards, 2008-01-09
 */
rsRetVal
strmSetFName(strm_t *pThis, uchar *pszName, size_t iLenName)
{
	DEFiRet;

	ASSERT(pThis != NULL);
	ASSERT(pszName != NULL);
	
	if(iLenName < 1)
		ABORT_FINALIZE(RS_RET_FILE_PREFIX_MISSING);

	if(pThis->pszFName != NULL)
		free(pThis->pszFName);

	if((pThis->pszFName = malloc(sizeof(uchar) * iLenName + 1)) == NULL)
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

	memcpy(pThis->pszFName, pszName, iLenName + 1); /* always think about the \0! */
	pThis->lenFName = iLenName;

finalize_it:
	RETiRet;
}


/* set the stream's directory
 * The passed-in string is duplicated. So if the caller does not need
 * it any longer, it must free it.
 * rgerhards, 2008-01-09
 */
rsRetVal
strmSetDir(strm_t *pThis, uchar *pszDir, size_t iLenDir)
{
	DEFiRet;

	ASSERT(pThis != NULL);
	ASSERT(pszDir != NULL);
	
	if(iLenDir < 1)
		ABORT_FINALIZE(RS_RET_FILE_PREFIX_MISSING);

	if((pThis->pszDir = malloc(sizeof(uchar) * iLenDir + 1)) == NULL)
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

	memcpy(pThis->pszDir, pszDir, iLenDir + 1); /* always think about the \0! */
	pThis->lenDir = iLenDir;

finalize_it:
	RETiRet;
}


/* support for data records
 * The stream class is able to write to multiple files. However, there are
 * situation (actually quite common), where a single data record should not
 * be split across files. This may be problematic if multiple stream write
 * calls are used to create the record. To support that, we provide the
 * bInRecord status variable. If it is set, no file spliting occurs. Once
 * it is set to 0, a check is done if a split is necessary and it then
 * happens. For a record-oriented caller, the proper sequence is:
 *
 * strmRecordBegin()
 * strmWrite...()
 * strmRecordEnd()
 *
 * Please note that records do not affect the writing of output buffers. They
 * are always written when full. The only thing affected is circular files
 * creation. So it is safe to write large records.
 *
 * IMPORTANT: RecordBegin() can not be nested! It is a programming error
 * if RecordBegin() is called while already in a record!
 *
 * rgerhards, 2008-01-10
 */
rsRetVal strmRecordBegin(strm_t *pThis)
{
	ASSERT(pThis != NULL);
	ASSERT(pThis->bInRecord == 0);
	pThis->bInRecord = 1;
	return RS_RET_OK;
}

rsRetVal strmRecordEnd(strm_t *pThis)
{
	DEFiRet;
	ASSERT(pThis != NULL);
	ASSERT(pThis->bInRecord == 1);

	pThis->bInRecord = 0;
	iRet = strmCheckNextOutputFile(pThis); /* check if we need to switch files */

	RETiRet;
}
/* end stream record support functions */


/* This method serializes a stream object. That means the whole
 * object is modified into text form. That text form is suitable for
 * later reconstruction of the object.
 * The most common use case for this method is the creation of an
 * on-disk representation of the message object.
 * We do not serialize the dynamic properties. 
 * rgerhards, 2008-01-10
 */
rsRetVal strmSerialize(strm_t *pThis, strm_t *pStrm)
{
	DEFiRet;
	int i;
	long l;

	ISOBJ_TYPE_assert(pThis, strm);
	ISOBJ_TYPE_assert(pStrm, strm);

	strmFlush(pThis);
	CHKiRet(obj.BeginSerialize(pStrm, (obj_t*) pThis));

	objSerializeSCALAR(pStrm, iCurrFNum, INT);
	objSerializePTR(pStrm, pszFName, PSZ);
	objSerializeSCALAR(pStrm, iMaxFiles, INT);
	objSerializeSCALAR(pStrm, bDeleteOnClose, INT);

	i = pThis->sType;
	objSerializeSCALAR_VAR(pStrm, sType, INT, i);

	i = pThis->tOperationsMode;
	objSerializeSCALAR_VAR(pStrm, tOperationsMode, INT, i);

	i = pThis->tOpenMode;
	objSerializeSCALAR_VAR(pStrm, tOpenMode, INT, i);

	l = (long) pThis->iCurrOffs;
	objSerializeSCALAR_VAR(pStrm, iCurrOffs, LONG, l);

	CHKiRet(obj.EndSerialize(pStrm));

finalize_it:
	RETiRet;
}



/* set a user write-counter. This counter is initialized to zero and
 * receives the number of bytes written. It is accurate only after a
 * flush(). This hook is provided as a means to control disk size usage.
 * The pointer must be valid at all times (so if it is on the stack, be sure
 * to remove it when you exit the function). Pointers are removed by
 * calling strmSetWCntr() with a NULL param. Only one pointer is settable,
 * any new set overwrites the previous one.
 * rgerhards, 2008-02-27
 */
rsRetVal
strmSetWCntr(strm_t *pThis, number_t *pWCnt)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, strm);

	if(pWCnt != NULL)
		*pWCnt = 0;
	pThis->pUsrWCntr = pWCnt;

	RETiRet;
}


#include "stringbuf.h"

/* This function can be used as a generic way to set properties.
 * rgerhards, 2008-01-11
 */
#define isProp(name) !rsCStrSzStrCmp(pProp->pcsName, (uchar*) name, sizeof(name) - 1)
rsRetVal strmSetProperty(strm_t *pThis, var_t *pProp)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, strm);
	ASSERT(pProp != NULL);

 	if(isProp("sType")) {
		CHKiRet(strmSetsType(pThis, (strmType_t) pProp->val.num));
 	} else if(isProp("iCurrFNum")) {
		pThis->iCurrFNum = pProp->val.num;
 	} else if(isProp("pszFName")) {
		CHKiRet(strmSetFName(pThis, rsCStrGetSzStrNoNULL(pProp->val.pStr), rsCStrLen(pProp->val.pStr)));
 	} else if(isProp("tOperationsMode")) {
		CHKiRet(strmSettOperationsMode(pThis, pProp->val.num));
 	} else if(isProp("tOpenMode")) {
		CHKiRet(strmSettOpenMode(pThis, pProp->val.num));
 	} else if(isProp("iCurrOffs")) {
		pThis->iCurrOffs = pProp->val.num;
 	} else if(isProp("iMaxFileSize")) {
		CHKiRet(strmSetiMaxFileSize(pThis, pProp->val.num));
 	} else if(isProp("iMaxFiles")) {
		CHKiRet(strmSetiMaxFiles(pThis, pProp->val.num));
 	} else if(isProp("iFileNumDigits")) {
		CHKiRet(strmSetiFileNumDigits(pThis, pProp->val.num));
 	} else if(isProp("bDeleteOnClose")) {
		CHKiRet(strmSetbDeleteOnClose(pThis, pProp->val.num));
	}

finalize_it:
	RETiRet;
}
#undef	isProp


/* return the current offset inside the stream. Note that on two consequtive calls, the offset
 * reported on the second call may actually be lower than on the first call. This is due to
 * file circulation. A caller must deal with that. -- rgerhards, 2008-01-30
 */
rsRetVal
strmGetCurrOffset(strm_t *pThis, int64 *pOffs)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, strm);
	ASSERT(pOffs != NULL);

	*pOffs = pThis->iCurrOffs;

	RETiRet;
}


/* queryInterface function
 * rgerhards, 2008-02-29
 */
BEGINobjQueryInterface(strm)
CODESTARTobjQueryInterface(strm)
	if(pIf->ifVersion != strmCURR_IF_VERSION) { /* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	//xxxpIf->oID = OBJvm;

finalize_it:
ENDobjQueryInterface(strm)


/* Initialize the stream class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-01-09
 */
BEGINObjClassInit(strm, 1, OBJ_IS_CORE_MODULE)
	/* request objects we use */

	OBJSetMethodHandler(objMethod_SERIALIZE, strmSerialize);
	OBJSetMethodHandler(objMethod_SETPROPERTY, strmSetProperty);
	OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, strmConstructFinalize);
ENDObjClassInit(strm)


/*
 * vi:set ai:
 */
