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
rsRetVal strmOpenFile(strm_t *pThis, int flags, mode_t mode)
{
	DEFiRet;

	assert(pThis != NULL);

	if(pThis->fd != -1)
		ABORT_FINALIZE(RS_RET_OK);

	if(pThis->pszFilePrefix == NULL)
		ABORT_FINALIZE(RS_RET_FILE_PREFIX_MISSING);

	CHKiRet(genFileName(&pThis->pszCurrFName, pThis->pszDir, pThis->lenDir,
 		     	    pThis->pszFilePrefix, pThis->lenFilePrefix, pThis->iCurrFNum, (uchar*) "qf", 2));

	pThis->fd = open((char*)pThis->pszCurrFName, flags, mode); // TODO: open modes!
	pThis->iCurrOffs = 0;

	dbgprintf("Stream 0x%lx: opened file '%s' for %d as %d\n", (unsigned long) pThis, pThis->pszCurrFName, flags, pThis->fd);

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
	dbgprintf("Stream 0x%lx: closing file %d\n", (unsigned long) pThis, pThis->fd);

	close(pThis->fd); // TODO: error check
	pThis->fd = -1;

	if(pThis->bDeleteOnClose) {
		unlink((char*) pThis->pszCurrFName); // TODO: check returncode
	}

	if(pThis->pszCurrFName != NULL) {
		free(pThis->pszCurrFName);	/* no longer needed in any case (just for open) */
		pThis->pszCurrFName = NULL;
	}

	return iRet;
}


/* switch to next strm file */
rsRetVal strmNextFile(strm_t *pThis)
{
	DEFiRet;

	assert(pThis != NULL);
	CHKiRet(strmCloseFile(pThis));

	/* we do modulo 1,000,000 so that the file number is always at most 6 digits. If we have a million
	 * or more strm files, something is awfully wrong and it is OK if we run into problems in that
	 * situation ;) -- rgerhards, 2008-01-09
	 */
	pThis->iCurrFNum = (pThis->iCurrFNum + 1) % 1000000;

finalize_it:
	return iRet;
}


/*** buffered read functions for strm files ***/

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
	
	assert(pThis != NULL);
	assert(pC != NULL);

	/* DEV debug only: dbgprintf("strmRead index %d, max %d\n", pThis->iBufPtr, pThis->iBufPtrMax); */
	if(pThis->pIOBuf == NULL) { /* TODO: maybe we should move that to file open... */
		if((pThis->pIOBuf = (uchar*) malloc(sizeof(uchar) * STRM_IOBUF_SIZE )) == NULL)
			ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
		pThis->iBufPtrMax = 0; /* results in immediate read request */
	}

	if(pThis->iUngetC != -1) {	/* do we have an "unread" char that we need to provide? */
		*pC = pThis->iUngetC;
		pThis->iUngetC = -1;
		ABORT_FINALIZE(RS_RET_OK);
	}
	
	/* do we need to obtain a new buffer */
	if(pThis->iBufPtr >= pThis->iBufPtrMax) {
		/* read */
		pThis->iBufPtrMax = read(pThis->fd, pThis->pIOBuf, STRM_IOBUF_SIZE);
		dbgprintf("strmReadChar read %d bytes from file %d\n", pThis->iBufPtrMax, pThis->fd);
		if(pThis->iBufPtrMax == 0)
			ABORT_FINALIZE(RS_RET_EOF);
		else if(pThis->iBufPtrMax < 0)
			ABORT_FINALIZE(RS_RET_IO_ERROR);
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
 * class.
 * rgerhards, 2008-01-07
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

/*** end buffered read functions for strm files ***/


/* --------------- end type-specific handlers -------------------- */


/* Constructor for the strm object
 */
rsRetVal strmConstruct(strm_t **ppThis)
{
	DEFiRet;
	strm_t *pThis;
dbgprintf("strmConstruct\n");

	assert(ppThis != NULL);

	if((pThis = (strm_t *)calloc(1, sizeof(strm_t))) == NULL) {
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}

	/* we have an object, so let's fill all properties that must not be 0 (we did calloc()!) */
	pThis->iCurrFNum = 1;
	pThis->fd = -1;
	pThis->iUngetC = -1;
	pThis->sType = STREAMTYPE_FILE;

finalize_it:
	OBJCONSTRUCT_CHECK_SUCCESS_AND_CLEANUP
	return iRet;
}


/* ConstructionFinalizer - currently provided just to comply to the interface
 * definiton. -- rgerhards, 2008-01-09
 */
rsRetVal strmConstructFinalize(strm_t __attribute__((unused)) *pThis)
{
	return RS_RET_OK;
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


/* write memory buffer to a stream object
 */
rsRetVal strmWrite(strm_t *pThis, uchar *pBuf, size_t lenBuf)
{
	DEFiRet;
	int iWritten;

dbgprintf("strmWrite()\n");
	assert(pThis != NULL);
	assert(pBuf != NULL);

	if(pThis->fd == -1)
		CHKiRet(strmOpenFile(pThis, O_RDWR|O_CREAT|O_TRUNC, 0600)); // TODO: open modes!

	iWritten = write(pThis->fd, pBuf, lenBuf);
	dbgprintf("Stream 0x%lx: write wrote %d bytes, errno: %d, err %s\n", (unsigned long) pThis,
	          iWritten, errno, strerror(errno));
	/* TODO: handle error case -- rgerhards, 2008-01-07 */

	pThis->iCurrOffs += iWritten;
	if(pThis->iCurrOffs >= pThis->iMaxFileSize)
		CHKiRet(strmNextFile(pThis));

finalize_it:
	return iRet;
}


/* property set methods */
/* simple ones first */
DEFpropSetMeth(strm, bDeleteOnClose, int)

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
