/* stringbuf.h
 * The counted string object
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
 * Copyright 2005-2009
 *     Rainer Gerhards and Adiscon GmbH. All Rights Reserved.
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
#ifndef _STRINGBUF_H_INCLUDED__
#define _STRINGBUF_H_INCLUDED__ 1


/** 
 * The dynamic string buffer object.
 */
typedef struct cstr_s
{	
#ifndef	NDEBUG
	rsObjID OID;		/**< object ID */
#endif
	uchar *pBuf;		/**< pointer to the string buffer, may be NULL if string is empty */
	uchar *pszBuf;		/**< pointer to the sz version of the string (after it has been created )*/
	size_t iBufSize;	/**< current maximum size of the string buffer */
	size_t iStrLen;		/**< length of the string in characters. */
	bool bIsForeignBuf;	/**< is pBuf a buffer provided by someone else? */
} cstr_t;


/**
 * Construct a rsCStr object.
 */
rsRetVal cstrConstruct(cstr_t **ppThis);
#define rsCStrConstruct(x) cstrConstruct((x))
rsRetVal rsCStrConstructFromszStr(cstr_t **ppThis, uchar *sz);
rsRetVal rsCStrConstructFromCStr(cstr_t **ppThis, cstr_t *pFrom);

/**
 * Destruct the string buffer object.
 */
void rsCStrDestruct(cstr_t **ppThis);
#define cstrDestruct(x) rsCStrDestruct((x))


/* Append a character to the current string object. This may only be done until
 * cstrFinalize() is called.
 * rgerhards, 2009-06-16
 */
rsRetVal rsCStrExtendBuf(cstr_t *pThis, size_t iMinNeeded); /* our helper, NOT a public interface! */
static inline rsRetVal cstrAppendChar(cstr_t *pThis, uchar c)
{
	rsRetVal iRet;

	rsCHECKVALIDOBJECT(pThis, OIDrsCStr);

	if(pThis->iStrLen >= pThis->iBufSize) {  
		CHKiRet(rsCStrExtendBuf(pThis, 1)); /* need more memory! */
	}

	/* ok, when we reach this, we have sufficient memory */
	*(pThis->pBuf + pThis->iStrLen++) = c;

finalize_it:
	return iRet;
}

/**
 * Truncate "n" number of characters from the end of the
 * string. The buffer remains unchanged, just the
 * string length is manipulated. This is for performance
 * reasons.
 */
rsRetVal rsCStrTruncate(cstr_t *pThis, size_t nTrunc);

rsRetVal rsCStrTrimTrailingWhiteSpace(cstr_t *pThis);
rsRetVal cstrTrimTrailingWhiteSpace(cstr_t *pThis);

/**
 * Append a string to the buffer. For performance reasons,
 * use rsCStrAppenStrWithLen() if you know the length.
 *
 * \param psz pointer to string to be appended. Must not be NULL.
 */
rsRetVal rsCStrAppendStr(cstr_t *pThis, uchar* psz);

/**
 * Append a string to the buffer.
 *
 * \param psz pointer to string to be appended. Must not be NULL.
 * \param iStrLen the length of the string pointed to by psz
 */
rsRetVal rsCStrAppendStrWithLen(cstr_t *pThis, uchar* psz, size_t iStrLen);


/**
 * Append an integer to the string. No special formatting is
 * done.
 */
rsRetVal rsCStrAppendInt(cstr_t *pThis, long i);


rsRetVal strExit(void); /* TODO: remove once we have a real object interface! */
uchar* __attribute__((deprecated)) rsCStrGetSzStr(cstr_t *pThis);
uchar*  rsCStrGetSzStrNoNULL(cstr_t *pThis);
rsRetVal rsCStrSetSzStr(cstr_t *pThis, uchar *pszNew);
int rsCStrCStrCmp(cstr_t *pCS1, cstr_t *pCS2);
int rsCStrSzStrCmp(cstr_t *pCS1, uchar *psz, size_t iLenSz);
int rsCStrOffsetSzStrCmp(cstr_t *pCS1, size_t iOffset, uchar *psz, size_t iLenSz);
int rsCStrLocateSzStr(cstr_t *pCStr, uchar *sz);
int rsCStrLocateInSzStr(cstr_t *pThis, uchar *sz);
int rsCStrCaseInsensitiveLocateInSzStr(cstr_t *pThis, uchar *sz);
int rsCStrStartsWithSzStr(cstr_t *pCS1, uchar *psz, size_t iLenSz);
int rsCStrCaseInsensitveStartsWithSzStr(cstr_t *pCS1, uchar *psz, size_t iLenSz);
int rsCStrSzStrStartsWithCStr(cstr_t *pCS1, uchar *psz, size_t iLenSz);
rsRetVal rsCStrSzStrMatchRegex(cstr_t *pCS1, uchar *psz, int iType, void *cache);
void rsCStrRegexDestruct(void *rc);
rsRetVal rsCStrConvertToNumber(cstr_t *pStr, number_t *pNumber);
rsRetVal rsCStrConvertToBool(cstr_t *pStr, number_t *pBool);

/* in migration */
#define rsCStrAppendCStr(pThis, pstrAppend) cstrAppendCStr(pThis, pstrAppend)

/* new calling interface */
rsRetVal cstrFinalize(cstr_t *pThis);
rsRetVal cstrConvSzStrAndDestruct(cstr_t *pThis, uchar **ppSz, int bRetNULL);
uchar*  cstrGetSzStr(cstr_t *pThis);
uchar*  cstrGetSzStrNoNULL(cstr_t *pThis);
rsRetVal cstrAppendCStr(cstr_t *pThis, cstr_t *pstrAppend);

/* now come inline-like functions */
#ifdef NDEBUG
#	define cstrLen(x) ((int)((x)->iStrLen))
#else
	int cstrLen(cstr_t *pThis);
#endif
#define rsCStrLen(s) cstrLen((s))

#define rsCStrGetBufBeg(x) ((x)->pBuf)

rsRetVal strInit();
rsRetVal strExit();

#endif /* single include */
