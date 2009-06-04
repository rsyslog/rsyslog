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

#ifndef STREAM_H_INCLUDED
#define STREAM_H_INCLUDED

#include <pthread.h>
#include "obj-types.h"
#include "glbl.h"
#include "stream.h"
#include "zlibw.h"

/* stream types */
typedef enum {
	STREAMTYPE_FILE_SINGLE = 0,	/**< read a single file */
	STREAMTYPE_FILE_CIRCULAR = 1,	/**< circular files */
	STREAMTYPE_FILE_MONITOR = 2	/**< monitor a (third-party) file */
} strmType_t;

typedef enum {				/* when extending, do NOT change existing modes! */
	STREAMMMODE_INVALID = 0,
	STREAMMODE_READ = 1,
	STREAMMODE_WRITE = 2,
	STREAMMODE_WRITE_TRUNC = 3,
	STREAMMODE_WRITE_APPEND = 4
} strmMode_t;

/* The strm_t data structure */
typedef struct strm_s {
	BEGINobjInstance;	/* Data to implement generic object - MUST be the first data element! */
	strmType_t sType;
	/* descriptive properties */
	int iCurrFNum;/* current file number (NOT descriptor, but the number in the file name!) */
	uchar *pszFName; /* prefix for generated filenames */
	int lenFName;
	strmMode_t tOperationsMode;
	mode_t tOpenMode;
	int64 iMaxFileSize;/* maximum size a file may grow to */
	int iMaxFiles;	/* maximum number of files if a circular mode is in use */
	int iFileNumDigits;/* min number of digits to use in file number (only in circular mode) */
	int bDeleteOnClose; /* set to 1 to auto-delete on close -- be careful with that setting! */
	int64 iCurrOffs;/* current offset */
	int64 *pUsrWCntr; /* NULL or a user-provided counter that receives the nbr of bytes written since the last CntrSet() */
	/* dynamic properties, valid only during file open, not to be persistet */
	size_t	sIOBufSize;/* size of IO buffer */
	uchar *pszDir; /* Directory */
	int lenDir;
	int fd;		/* the file descriptor, -1 if closed */
	uchar *pszCurrFName; /* name of current file (if open) */
	uchar *pIOBuf;	/* io Buffer */
	size_t iBufPtrMax;	/* current max Ptr in Buffer (if partial read!) */
	size_t iBufPtr;	/* pointer into current buffer */
	int iUngetC;	/* char set via UngetChar() call or -1 if none set */
	int bInRecord;	/* if 1, indicates that we are currently writing a not-yet complete record */
	int iZipLevel;	/* zip level (0..9). If 0, zip is completely disabled */
	Bytef *pZipBuf;

} strm_t;

/* interfaces */
BEGINinterface(strm) /* name must also be changed in ENDinterface macro! */
	rsRetVal (*Construct)(strm_t **ppThis);
	rsRetVal (*ConstructFinalize)(strm_t __attribute__((unused)) *pThis);
	rsRetVal (*Destruct)(strm_t **ppThis);
	rsRetVal (*SetMaxFileSize)(strm_t *pThis, int64 iMaxFileSize);
	rsRetVal (*SetFileName)(strm_t *pThis, uchar *pszName, size_t iLenName);
	rsRetVal (*ReadChar)(strm_t *pThis, uchar *pC);
	rsRetVal (*UnreadChar)(strm_t *pThis, uchar c);
	rsRetVal (*ReadLine)(strm_t *pThis, cstr_t **ppCStr);
	rsRetVal (*SeekCurrOffs)(strm_t *pThis);
	rsRetVal (*Write)(strm_t *pThis, uchar *pBuf, size_t lenBuf);
	rsRetVal (*WriteChar)(strm_t *pThis, uchar c);
	rsRetVal (*WriteLong)(strm_t *pThis, long i);
	rsRetVal (*SetFName)(strm_t *pThis, uchar *pszPrefix, size_t iLenPrefix);
	rsRetVal (*SetDir)(strm_t *pThis, uchar *pszDir, size_t iLenDir);
	rsRetVal (*Flush)(strm_t *pThis);
	rsRetVal (*RecordBegin)(strm_t *pThis);
	rsRetVal (*RecordEnd)(strm_t *pThis);
	rsRetVal (*Serialize)(strm_t *pThis, strm_t *pStrm);
	rsRetVal (*GetCurrOffset)(strm_t *pThis, int64 *pOffs);
	rsRetVal (*SetWCntr)(strm_t *pThis, number_t *pWCnt);
	INTERFACEpropSetMeth(strm, bDeleteOnClose, int);
	INTERFACEpropSetMeth(strm, iMaxFileSize, int);
	INTERFACEpropSetMeth(strm, iMaxFiles, int);
	INTERFACEpropSetMeth(strm, iFileNumDigits, int);
	INTERFACEpropSetMeth(strm, tOperationsMode, int);
	INTERFACEpropSetMeth(strm, tOpenMode, mode_t);
	INTERFACEpropSetMeth(strm, sType, strmType_t);
	INTERFACEpropSetMeth(strm, iZipLevel, int);
	INTERFACEpropSetMeth(strm, sIOBufSize, size_t);
ENDinterface(strm)
#define strmCURR_IF_VERSION 1 /* increment whenever you change the interface structure! */


/* prototypes */
PROTOTYPEObjClassInit(strm);

#endif /* #ifndef STREAM_H_INCLUDED */
