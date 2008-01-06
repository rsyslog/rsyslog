/*! \file stringbuf.h
 *  \brief The counted string object
 *
 * This is the byte-counted string class for rsyslog. It is a replacement
 * for classical \0 terminated string functions. We introduce it in
 * the hope it will make the program more secure, obtain some performance
 * and, most importantly, lay they foundation for syslog-protocol, which
 * requires strings to be able to handle embedded \0 characters.
 *
 * \author  Rainer Gerhards <rgerhards@adiscon.com>
 * \date    2005-09-07
 *          Initial version  begun.
 *
 * All functions in this "class" start with rsCStr (rsyslog Counted String).
 * Copyright 2005
 *     Rainer Gerhards and Adiscon GmbH. All Rights Reserved.
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
#ifndef _STRINGBUF_H_INCLUDED__
#define _STRINGBUF_H_INCLUDED__ 1

/** 
 * The dynamic string buffer object.
 */
struct rsCStrObject
{	
#ifndef	NDEBUG
	rsObjID OID;		/**< object ID */
#endif
	uchar *pBuf;		/**< pointer to the string buffer, may be NULL if string is empty */
	uchar *pszBuf;		/**< pointer to the sz version of the string (after it has been created )*/
	size_t iBufSize;	/**< current maximum size of the string buffer */
	size_t iStrLen;		/**< length of the string in characters. */
	size_t iAllocIncrement;	/**< the amount of bytes the string should be expanded if it needs to */
};
typedef struct rsCStrObject rsCStrObj;


/**
 * Construct a rsCStr object.
 */
rsCStrObj *rsCStrConstruct(void);
rsRetVal rsCStrConstructFromszStr(rsCStrObj **ppThis, uchar *sz);
rsRetVal rsCStrConstructFromCStr(rsCStrObj **ppThis, rsCStrObj *pFrom);

/**
 * Destruct the string buffer object.
 */
void rsCStrDestruct(rsCStrObj *pThis);

/**
 * Append a character to an existing string. If necessary, the
 * method expands the string buffer.
 *
 * \param c Character to append to string.
 */
rsRetVal rsCStrAppendChar(rsCStrObj *pThis, uchar c);

/**
 * Finish the string buffer dynamic allocation.
 */
rsRetVal rsCStrFinish(rsCStrObj *pThis);

/**
 * Truncate "n" number of characters from the end of the
 * string. The buffer remains unchanged, just the
 * string length is manipulated. This is for performance
 * reasons.
 */
rsRetVal rsCStrTruncate(rsCStrObj *pThis, size_t nTrunc);

rsRetVal rsCStrTrimTrailingWhiteSpace(rsCStrObj *pThis);

/**
 * Append a string to the buffer. For performance reasons,
 * use rsCStrAppenStrWithLen() if you know the length.
 *
 * \param psz pointer to string to be appended. Must not be NULL.
 */
rsRetVal rsCStrAppendStr(rsCStrObj *pThis, uchar* psz);

/**
 * Append a string to the buffer.
 *
 * \param psz pointer to string to be appended. Must not be NULL.
 * \param iStrLen the length of the string pointed to by psz
 */
rsRetVal rsCStrAppendStrWithLen(rsCStrObj *pThis, uchar* psz, size_t iStrLen);

/**
 * Set a new allocation incremet. This will influence
 * the allocation the next time the string will be expanded.
 * It can be set and changed at any time. If done immediately
 * after custructing the StrB object, this will also be
 * the inital allocation.
 *
 * \param iNewIncrement The new increment size
 *
 * \note It is possible to use a very low increment, e.g. 1 byte.
 *       This can generate a considerable overhead. We highly 
 *       advise not to use an increment below 32 bytes, except
 *       if you are very well aware why you are doing it ;)
 */
void rsCStrSetAllocIncrement(rsCStrObj *pThis, int iNewIncrement);

/**
 * Append an integer to the string. No special formatting is
 * done.
 */
rsRetVal rsCStrAppendInt(rsCStrObj *pThis, long i);


uchar*  rsCStrGetSzStr(rsCStrObj *pThis);
uchar*  rsCStrGetSzStrNoNULL(rsCStrObj *pThis);
rsRetVal rsCStrSetSzStr(rsCStrObj *pThis, uchar *pszNew);
rsRetVal rsCStrConvSzStrAndDestruct(rsCStrObj *pThis, uchar **ppSz, int bRetNULL);
int rsCStrCStrCmp(rsCStrObj *pCS1, rsCStrObj *pCS2);
int rsCStrSzStrCmp(rsCStrObj *pCS1, uchar *psz, size_t iLenSz);
int rsCStrOffsetSzStrCmp(rsCStrObj *pCS1, size_t iOffset, uchar *psz, size_t iLenSz);
int rsCStrLocateSzStr(rsCStrObj *pCStr, uchar *sz);
int rsCStrLocateInSzStr(rsCStrObj *pThis, uchar *sz);
int rsCStrStartsWithSzStr(rsCStrObj *pCS1, uchar *psz, size_t iLenSz);
int rsCStrSzStrStartsWithCStr(rsCStrObj *pCS1, uchar *psz, size_t iLenSz);
int rsCStrSzStrMatchRegex(rsCStrObj *pCS1, uchar *psz);

/* now come inline-like functions */
#ifdef NDEBUG
#	define rsCStrLen(x) ((int)((x)->iStrLen))
#else
	int rsCStrLen(rsCStrObj *pThis);
#endif

#define rsCStrGetBufBeg(x) ((x)->pBuf)

#endif /* single include */
