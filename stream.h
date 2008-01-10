/* Definition of serial stream class (strm).
 *
 * A serial stream provides serial data access. In theory, serial streams
 * can be implemented via a number of methods (e.g. files or in-memory
 * streams). In practice, there currently only exist the file type (aka
 * "driver").
 *
 * In practice, many stream features are bound to files. I have not yet made
 * any serious effort, except for the naming of this class, to try to make
 * the interfaces very generic. However, I assume that we could work much
 * like in the strm class, where some properties are simply ignored when
 * the wrong strm mode is selected (which would translate here to the wrong
 * stream mode).
 *
 * Most importantly, this class provides generic input and output functions
 * which can directly be used to work with the strms and file output. It
 * provides such useful things like a circular file buffer and, hopefully
 * at a later stage, a lazy writer. The object is also seriazable and thus
 * can easily be persistet. The bottom line is that it makes much sense to
 * use this class whereever possible as its features may grow in the future.
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

#ifndef STREAM_H_INCLUDED
#define STREAM_H_INCLUDED

#include <pthread.h>
#include "obj-types.h"
#include "glbl.h"
#include "stream.h"

/* stream types */
typedef enum {
	STREAMTYPE_FILE = 0
} strmType_t;

/* The strm_t data structure */
typedef struct strm_s {
	BEGINobjInstance;	/* Data to implement generic object - MUST be the first data element! */
	strmType_t sType;
	int fd;		/* the file descriptor, -1 if closed */
	uchar *pszCurrFName; /* name of current file (if open) */
	int iCurrFNum;/* current file number (NOT descriptor, but the number in the file name!) */
	uchar *pszDir; /* Directory */
	int lenDir;
	uchar *pszFilePrefix; /* prefix for generated filenames */
	int lenFilePrefix;
	size_t iCurrOffs;/* current offset */
	uchar *pIOBuf;	/* io Buffer */
	int iBufPtrMax;	/* current max Ptr in Buffer (if partial read!) */
	int iBufPtr;	/* pointer into current buffer */
	int iUngetC;	/* char set via UngetChar() call or -1 if none set */
	int iFlagsOpenOS;
	int iModeOpenOS;
	size_t iMaxFileSize;/* maximum size a file may grow to */
	int bDeleteOnClose; /* set to 1 to auto-delete on close -- be careful with that setting! */
	int iMaxFiles;	/* maximum number of files if a circular mode is in use */
	int iFileNumDigits;/* min number of digits to use in file number (only in circular mode) */
	size_t	sIOBufSize;/* size of IO buffer */
} strm_t;

/* prototypes */
rsRetVal strmConstruct(strm_t **ppThis);
rsRetVal strmConstructFinalize(strm_t __attribute__((unused)) *pThis);
rsRetVal strmDestruct(strm_t *pThis);
rsRetVal strmSetMaxFileSize(strm_t *pThis, size_t iMaxFileSize);
rsRetVal strmSetFilePrefix(strm_t *pThis, uchar *pszPrefix, size_t iLenPrefix);
rsRetVal strmReadChar(strm_t *pThis, uchar *pC);
rsRetVal strmUnreadChar(strm_t *pThis, uchar c);
rsRetVal strmWrite(strm_t *pThis, uchar *pBuf, size_t lenBuf);
rsRetVal strmNextFile(strm_t *pThis);
rsRetVal strmOpenFile(strm_t *pThis, int flags, mode_t mode);
rsRetVal strmSetFilePrefix(strm_t *pThis, uchar *pszPrefix, size_t iLenPrefix);
rsRetVal strmSetDir(strm_t *pThis, uchar *pszDir, size_t iLenDir);
PROTOTYPEObjClassInit(strm);
PROTOTYPEpropSetMeth(strm, bDeleteOnClose, int);
PROTOTYPEpropSetMeth(strm, iMaxFileSize, int);
PROTOTYPEpropSetMeth(strm, iMaxFiles, int);
PROTOTYPEpropSetMeth(strm, iFileNumDigits, int);

#endif /* #ifndef STREAM_H_INCLUDED */
