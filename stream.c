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

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
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

	assert(pThis != NULL);
	assert(pThis->tOperationsMode == STREAMMODE_READ || pThis->tOperationsMode == STREAMMODE_WRITE);

	if(pThis->fd != -1)
		ABORT_FINALIZE(RS_RET_OK);

	if(pThis->pszFilePrefix == NULL)
		ABORT_FINALIZE(RS_RET_FILE_PREFIX_MISSING);

	CHKiRet(genFileName(&pThis->pszCurrFName, pThis->pszDir, pThis->lenDir,
 		     	    pThis->pszFilePrefix, pThis->lenFilePrefix, pThis->iCurrFNum, pThis->iFileNumDigits));

	/* compute which flags we need to provide to open */
	if(pThis->tOperationsMode == STREAMMODE_READ)
		iFlags = O_RDONLY;
	else
		iFlags = O_WRONLY | O_TRUNC | O_CREAT | O_APPEND;

	pThis->fd = open((char*)pThis->pszCurrFName, iFlags, pThis->tOpenMode);
	pThis->iCurrOffs = 0;

	dbgprintf("Stream 0x%lx: opened file '%s' for %s (0x%x) as %d\n", (unsigned long) pThis,
		  pThis->pszCurrFName, (pThis->tOperationsMode == STREAMMODE_READ) ? "READ" : "WRITE",
		  iFlags, pThis->fd);

finalize_it:
	return iRet;
}


/* close a strm file
 * Note that the bDeleteOnClose flag is honored. If it is set, the file will be
 * deleted after close. This is in support for the qRead thread.
 */
static rsRetVal strmCloseFile(strm_t *pThis)
{
	DEFiRet;

	assert(pThis != NULL);
	assert(pThis->fd != -1);
	dbgprintf("Stream 0x%lx: closing file %d\n", (unsigned long) pThis, pThis->fd);

	if(pThis->tOperationsMode == STREAMMODE_WRITE)
		strmFlush(pThis);

	close(pThis->fd); // TODO: error check
	pThis->fd = -1;

	if(pThis->bDeleteOnClose) {
		unlink((char*) pThis->pszCurrFName); // TODO: check returncode
	}

	if(pThis->pszCurrFName != NULL) {
		free(pThis->pszCurrFName);	/* no longer needed in any case (just for open) */
		pThis->pszCurrFName = NULL;
	}

dbgprintf("exit strmCloseFile, fd: %d\n", pThis->fd);
	return iRet;
}


/* switch to next strm file
 * This method must only be called if we are in a multi-file mode!
 */
static rsRetVal
strmNextFile(strm_t *pThis)
{
	DEFiRet;

dbgprintf("strmNextFile, old num %d\n", pThis->iCurrFNum);
	assert(pThis != NULL);
	assert(pThis->iMaxFiles != 0);
	assert(pThis->fd != -1);

	CHKiRet(strmCloseFile(pThis));

	/* we do modulo operation to ensure we obej the iMaxFile property. This will always
	 * result in a file number lower than iMaxFile, so it if wraps, the name is back to
	 * 0, which results in the first file being overwritten. Not desired for queues, so
	 * make sure their iMaxFiles is large enough. But it is well-desired for other
	 * use cases, e.g. a circular output log file. -- rgerhards, 2008-01-10
	 */
	pThis->iCurrFNum = (pThis->iCurrFNum + 1) % pThis->iMaxFiles;

finalize_it:
	return iRet;
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
	int bRun;
	long iLenRead;
	
	assert(pThis != NULL);
	assert(pC != NULL);

	/* DEV debug only: dbgprintf("strmRead index %d, max %d\n", pThis->iBufPtr, pThis->iBufPtrMax); */
	if(pThis->iUngetC != -1) {	/* do we have an "unread" char that we need to provide? */
		*pC = pThis->iUngetC;
		pThis->iUngetC = -1;
		ABORT_FINALIZE(RS_RET_OK);
	}
	
	/* do we need to obtain a new buffer */
	if(pThis->iBufPtr >= pThis->iBufPtrMax) {
		/* We need to try read at least twice because we may run into EOF and need to switch files. */
		bRun = 1;
		while(bRun) {
			/* first check if we need to (re)open the file (we may have switched to a new one!) */
			CHKiRet(strmOpenFile(pThis));
			iLenRead = read(pThis->fd, pThis->pIOBuf, pThis->sIOBufSize);
			dbgprintf("Stream 0x%lx: read %ld bytes from file %d\n", (unsigned long) pThis,
				  iLenRead, pThis->fd);
			if(iLenRead == 0) {
				if(pThis->iMaxFiles == 0)
					ABORT_FINALIZE(RS_RET_EOF);
				else {
					/* we have multiple files and need to switch to the next one */
					/* TODO: think about emulating EOF in this case (not yet needed) */
					dbgprintf("Stream 0x%lx: EOF on file %d\n", (unsigned long) pThis, pThis->fd);
					CHKiRet(strmNextFile(pThis));
				}
			} else if(iLenRead < 0)
				ABORT_FINALIZE(RS_RET_IO_ERROR);
			else { /* good read */
				pThis->iBufPtrMax = iLenRead;
				bRun = 0;	/* exit loop */
			}
		}
		/* if we reach this point, we had a good read */
		pThis->iBufPtr = 0;
	}

	*pC = pThis->pIOBuf[pThis->iBufPtr++];

finalize_it:
	return iRet;
}


/* unget a single character just like ungetc(). As with that call, there is only a single
 * character buffering capability.
 * rgerhards, 2008-01-07
 */
rsRetVal strmUnreadChar(strm_t *pThis, uchar c)
{
	assert(pThis != NULL);
	assert(pThis->iUngetC == -1);
	pThis->iUngetC = c;

	return RS_RET_OK;
}

#if 0
/* we have commented out the code below because we would like to preserve it. It 
 * is currently not needed, but may be useful if we implemented a bufferred file
 * class. NOTE: YOU MUST REVIEW THIS CODE BEFORE ACTIVATION. It may be pretty 
 * outdated! -- rgerhards, 2008-01-10
 */
/* read a line from a strm file. A line is terminated by LF. The LF is read, but it
 * is not returned in the buffer (it is discared). The caller is responsible for
 * destruction of the returned CStr object!
 * rgerhards, 2008-01-07
 */
static rsRetVal strmReadLine(strm_t *pThis, rsCStrObj **ppCStr)
{
	DEFiRet;
	uchar c;
	rsCStrObj *pCStr = NULL;

	assert(pThis != NULL);
	assert(ppCStr != NULL);

	if((pCStr = rsCStrConstruct()) == NULL)
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

	/* now read the line */
	CHKiRet(strmReadChar(pThis, &c));
	while(c != '\n') {
		CHKiRet(rsCStrAppendChar(pCStr, c));
		CHKiRet(strmReadChar(pThis, &c));
	}
	CHKiRet(rsCStrFinish(pCStr));
	*ppCStr = pCStr;

finalize_it:
	if(iRet != RS_RET_OK && pCStr != NULL)
		rsCStrDestruct(pCStr);

	return iRet;
}

#endif /* #if 0 - saved code */


/* Standard-Constructor for the strm object
 */
BEGINobjConstruct(strm)
	pThis->iCurrFNum = 1;
	pThis->fd = -1;
	pThis->iUngetC = -1;
	pThis->sType = STREAMTYPE_FILE;
	pThis->sIOBufSize = glblGetIOBufSize();
	pThis->tOpenMode = 0600;
ENDobjConstruct(strm)


/* ConstructionFinalizer
 * rgerhards, 2008-01-09
 */
rsRetVal strmConstructFinalize(strm_t *pThis)
{
	DEFiRet;

	assert(pThis != NULL);

	if(pThis->pIOBuf == NULL) { /* allocate our io buffer in case we have not yet */
		if((pThis->pIOBuf = (uchar*) malloc(sizeof(uchar) * pThis->sIOBufSize)) == NULL)
			ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
		pThis->iBufPtrMax = 0; /* results in immediate read request */
	}

finalize_it:
	return iRet;
}


/* destructor for the strm object */
rsRetVal strmDestruct(strm_t *pThis)
{
	DEFiRet;

	assert(pThis != NULL);

	/* ... then free resources */
	if(pThis->fd != -1)
		strmCloseFile(pThis);

	if(pThis->pszDir != NULL)
		free(pThis->pszDir);

	/* and finally delete the strm objet itself */
	free(pThis);

	return iRet;
}


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
		dbgprintf("Stream 0x%lx: max file size %ld reached for %d, now %ld - starting new file\n",
			  (unsigned long) pThis, (long) pThis->iMaxFileSize, pThis->fd, (long) pThis->iCurrOffs);
		CHKiRet(strmNextFile(pThis));
	}

finalize_it:
	return iRet;
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

	assert(pThis != NULL);
	assert(pBuf == pThis->pIOBuf || pThis->iBufPtr == 0);

	if(pThis->fd == -1)
		CHKiRet(strmOpenFile(pThis));

	iWritten = write(pThis->fd, pBuf, lenBuf);
	dbgprintf("Stream 0x%lx: write wrote %d bytes, errno: %d, err %s\n", (unsigned long) pThis,
	          iWritten, errno, strerror(errno));
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
	CHKiRet(strmCheckNextOutputFile(pThis));

finalize_it:
	pThis->iBufPtr = 0; /* see comment above */

	return iRet;
}

/* flush stream output buffer to persistent storage. This can be called at any time
 * and is automatically called when the output buffer is full.
 * rgerhards, 2008-01-10
 */
rsRetVal strmFlush(strm_t *pThis)
{
	DEFiRet;

	assert(pThis != NULL);
	dbgprintf("Stream 0x%lx: flush file %d, buflen %ld\n", (unsigned long) pThis, pThis->fd, pThis->iBufPtr);

	if(pThis->iBufPtr > 0) {
		iRet = strmWriteInternal(pThis, pThis->pIOBuf, pThis->iBufPtr);
	}

	return iRet;
}


/* write a *single* character to a stream object -- rgerhards, 2008-01-10
 */
rsRetVal strmWriteChar(strm_t *pThis, uchar c)
{
	DEFiRet;

	assert(pThis != NULL);

	/* if the buffer is full, we need to flush before we can write */
	if(pThis->iBufPtr == pThis->sIOBufSize) {
		CHKiRet(strmFlush(pThis));
	}
	/* we now always have space for one character, so we simply copy it */
	*(pThis->pIOBuf + pThis->iBufPtr) = c;
	pThis->iBufPtr++;

finalize_it:
	return iRet;
}


/* write an integer value (actually a long) to a stream object */
rsRetVal strmWriteLong(strm_t *pThis, long i)
{
	DEFiRet;
	uchar szBuf[32];

	assert(pThis != NULL);

	CHKiRet(srUtilItoA((char*)szBuf, sizeof(szBuf), i));
	CHKiRet(strmWrite(pThis, szBuf, strlen((char*)szBuf)));

finalize_it:
	return iRet;
}


/* write memory buffer to a stream object
 */
rsRetVal strmWrite(strm_t *pThis, uchar *pBuf, size_t lenBuf)
{
	DEFiRet;
	size_t iPartial;

	assert(pThis != NULL);
	assert(pBuf != NULL);

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
	return iRet;
}


/* property set methods */
/* simple ones first */
DEFpropSetMeth(strm, bDeleteOnClose, int)
DEFpropSetMeth(strm, iMaxFileSize, int)
DEFpropSetMeth(strm, iFileNumDigits, int)
DEFpropSetMeth(strm, tOperationsMode, int);
DEFpropSetMeth(strm, tOpenMode, mode_t);

rsRetVal strmSetiMaxFiles(strm_t *pThis, int iNewVal)
{
	pThis->iMaxFiles = iNewVal;
	pThis->iFileNumDigits = getNumberDigits(iNewVal);
	return RS_RET_OK;
}


/* set the stream's file prefix
 * The passed-in string is duplicated. So if the caller does not need
 * it any longer, it must free it.
 * rgerhards, 2008-01-09
 */
rsRetVal
strmSetFilePrefix(strm_t *pThis, uchar *pszPrefix, size_t iLenPrefix)
{
	DEFiRet;

	assert(pThis != NULL);
	assert(pszPrefix != NULL);
	
	if(iLenPrefix < 1)
		ABORT_FINALIZE(RS_RET_FILE_PREFIX_MISSING);

	if((pThis->pszFilePrefix = malloc(sizeof(uchar) * iLenPrefix + 1)) == NULL)
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

	memcpy(pThis->pszFilePrefix, pszPrefix, iLenPrefix + 1); /* always think about the \0! */
	pThis->lenFilePrefix = iLenPrefix;

finalize_it:
	return iRet;
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

	assert(pThis != NULL);
	assert(pszDir != NULL);
	
	if(iLenDir < 1)
		ABORT_FINALIZE(RS_RET_FILE_PREFIX_MISSING);

	if((pThis->pszDir = malloc(sizeof(uchar) * iLenDir + 1)) == NULL)
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

	memcpy(pThis->pszDir, pszDir, iLenDir + 1); /* always think about the \0! */
	pThis->lenDir = iLenDir;

finalize_it:
	return iRet;
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
	assert(pThis != NULL);
	assert(pThis->bInRecord == 0);
	pThis->bInRecord = 1;
dbgprintf("strmRecordBegin set \n");
	return RS_RET_OK;
}

rsRetVal strmRecordEnd(strm_t *pThis)
{
	DEFiRet;
	assert(pThis != NULL);
	assert(pThis->bInRecord == 1);

dbgprintf("strmRecordEnd in %d\n", iRet);
	pThis->bInRecord = 0;
	iRet = strmCheckNextOutputFile(pThis); /* check if we need to switch files */
dbgprintf("strmRecordEnd out %d\n", iRet);

	return iRet;
}
/* end stream record support functions */



/* Initialize the stream class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-01-09
 */
BEGINObjClassInit(strm, 1)
	//OBJSetMethodHandler(objMethod_SERIALIZE, strmSerialize);
	//OBJSetMethodHandler(objMethod_SETPROPERTY, strmSetProperty);
	OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, strmConstructFinalize);
ENDObjClassInit(strm)
/*
 * vi:set ai:
 */
