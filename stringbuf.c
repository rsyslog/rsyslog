/* This is the byte-counted string class for rsyslog. It is a replacement
 * for classical \0 terminated string functions. We introduce it in
 * the hope it will make the program more secure, obtain some performance
 * and, most importantly, lay they foundation for syslog-protocol, which
 * requires strings to be able to handle embedded \0 characters.
 * Please see syslogd.c for license information.
 * All functions in this "class" start with rsCStr (rsyslog Counted String).
 * begun 2005-09-07 rgerhards
 *
 * Copyright (C) 2007 by Rainer Gerhards and Adiscon GmbH
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
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include "rsyslog.h"
#include "stringbuf.h"
#include "srUtils.h"
#include "regexp.h"
#include "obj.h"


/* ################################################################# *
 * private members                                                   *
 * ################################################################# */

/* static data */
DEFobjCurrIf(obj)
DEFobjCurrIf(regexp)

/* ################################################################# *
 * public members                                                    *
 * ################################################################# */


rsRetVal rsCStrConstruct(cstr_t **ppThis)
{
	DEFiRet;
	cstr_t *pThis;

	ASSERT(ppThis != NULL);

	if((pThis = (cstr_t*) calloc(1, sizeof(cstr_t))) == NULL)
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

	rsSETOBJTYPE(pThis, OIDrsCStr);
	pThis->pBuf = NULL;
	pThis->pszBuf = NULL;
	pThis->iBufSize = 0;
	pThis->iStrLen = 0;
	pThis->iAllocIncrement = RS_STRINGBUF_ALLOC_INCREMENT;
	*ppThis = pThis;

finalize_it:
	RETiRet;
}


/* construct from sz string
 * rgerhards 2005-09-15
 */
rsRetVal rsCStrConstructFromszStr(cstr_t **ppThis, uchar *sz)
{
	DEFiRet;
	cstr_t *pThis;

	assert(ppThis != NULL);

	CHKiRet(rsCStrConstruct(&pThis));

	pThis->iBufSize = pThis->iStrLen = strlen((char*)(char *) sz);
	if((pThis->pBuf = (uchar*) malloc(sizeof(uchar) * pThis->iStrLen)) == NULL) {
		RSFREEOBJ(pThis);
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}

	/* we do NOT need to copy the \0! */
	memcpy(pThis->pBuf, sz, pThis->iStrLen);

	*ppThis = pThis;

finalize_it:
	RETiRet;
}

/* construct from CStr object. only the counted string is
 * copied, not the szString.
 * rgerhards 2005-10-18
 */
rsRetVal rsCStrConstructFromCStr(cstr_t **ppThis, cstr_t *pFrom)
{
	DEFiRet;
	cstr_t *pThis;

	assert(ppThis != NULL);
	rsCHECKVALIDOBJECT(pFrom, OIDrsCStr);

	CHKiRet(rsCStrConstruct(&pThis));

	pThis->iBufSize = pThis->iStrLen = pFrom->iStrLen;
	if((pThis->pBuf = (uchar*) malloc(sizeof(uchar) * pThis->iStrLen)) == NULL) {
		RSFREEOBJ(pThis);
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}

	/* copy properties */
	memcpy(pThis->pBuf, pFrom->pBuf, pThis->iStrLen);

	*ppThis = pThis;
finalize_it:
	RETiRet;
}


void rsCStrDestruct(cstr_t **ppThis)
{
	cstr_t *pThis = *ppThis;

	/* rgerhards 2005-10-19: The free of pBuf was contained in conditional compilation.
	 * The code was only compiled if STRINGBUF_TRIM_ALLOCSIZE was set to 1. I honestly
	 * do not know why it was so, I think it was an artifact. Anyhow, I have changed this
	 * now. Should there any issue occur, this comment hopefully will shed some light 
	 * on what happened. I re-verified, and this function has never before been called
	 * by anyone. So changing it can have no impact for obvious reasons...
	 *
	 * rgerhards, 2008-02-20: I changed the interface to the new calling conventions, where
	 * the destructor receives a pointer to the object, so that it can set it to NULL.
	 */
	if(pThis->pBuf != NULL) {
		free(pThis->pBuf);
	}

	if(pThis->pszBuf != NULL) {
		free(pThis->pszBuf);
	}

	RSFREEOBJ(pThis);
	*ppThis = NULL;
}


/* extend the string buffer if its size is insufficient.
 * Param iMinNeeded is the minumum free space needed. If it is larger
 * than the default alloc increment, space for at least this amount is
 * allocated. In practice, a bit more is allocated because we envision that
 * some more characters may be added after these.
 * rgerhards, 2008-01-07
 */
static rsRetVal rsCStrExtendBuf(cstr_t *pThis, size_t iMinNeeded)
{
	DEFiRet;
	uchar *pNewBuf;
	size_t iNewSize;

	/* first compute the new size needed */
	if(iMinNeeded > pThis->iAllocIncrement) {
		/* we allocate "n" iAllocIncrements. Usually, that should
		 * leave some room after the absolutely needed one. It also
		 * reduces memory fragmentation. Note that all of this are
		 * integer operations (very important to understand what is
		 * going on)! Parenthesis are for better readibility.
		 */
		iNewSize = ((iMinNeeded / pThis->iAllocIncrement) + 1) * pThis->iAllocIncrement;
	} else {
		iNewSize = pThis->iBufSize + pThis->iAllocIncrement;
	}
	iNewSize += pThis->iBufSize; /* add current size */

	/* and then allocate and copy over */
	/* DEV debugging only: dbgprintf("extending string buffer, old %d, new %d\n", pThis->iBufSize, iNewSize); */
	if((pNewBuf = (uchar*) malloc(iNewSize * sizeof(uchar))) == NULL)
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	memcpy(pNewBuf, pThis->pBuf, pThis->iBufSize);
	pThis->iBufSize = iNewSize;
	if(pThis->pBuf != NULL) {
		free(pThis->pBuf);
	}
	pThis->pBuf = pNewBuf;

finalize_it:
	RETiRet;
}


/* append a string of known length. In this case, we make sure we do at most
 * one additional memory allocation.
 * I optimized this function to use memcpy(), among others. Consider it a
 * rewrite (which may be good to know in case of bugs) -- rgerhards, 2008-01-07
 */
rsRetVal rsCStrAppendStrWithLen(cstr_t *pThis, uchar* psz, size_t iStrLen)
{
	DEFiRet;

	rsCHECKVALIDOBJECT(pThis, OIDrsCStr);
	assert(psz != NULL);

	/* does the string fit? */
	if(pThis->iStrLen + iStrLen > pThis->iBufSize) {  
		CHKiRet(rsCStrExtendBuf(pThis, iStrLen)); /* need more memory! */
	}

	/* ok, now we always have sufficient continues memory to do a memcpy() */
	memcpy(pThis->pBuf + pThis->iStrLen, psz, iStrLen);
	pThis->iStrLen += iStrLen;

finalize_it:
	RETiRet;
}


/* changed to be a wrapper to rsCStrAppendStrWithLen() so that
 * we can save some time when we have the length but do not
 * need to change existing code.
 * rgerhards, 2007-07-03
 */
rsRetVal rsCStrAppendStr(cstr_t *pThis, uchar* psz)
{
	return rsCStrAppendStrWithLen(pThis, psz, strlen((char*) psz));
}


/* append the contents of one cstr_t object to another
 * rgerhards, 2008-02-25
 */
rsRetVal rsCStrAppendCStr(cstr_t *pThis, cstr_t *pstrAppend)
{
	return rsCStrAppendStrWithLen(pThis, pstrAppend->pBuf, pstrAppend->iStrLen);
}


rsRetVal rsCStrAppendInt(cstr_t *pThis, long i)
{
	DEFiRet;
	uchar szBuf[32];

	rsCHECKVALIDOBJECT(pThis, OIDrsCStr);

	CHKiRet(srUtilItoA((char*) szBuf, sizeof(szBuf), i));

	iRet = rsCStrAppendStr(pThis, szBuf);
finalize_it:
	RETiRet;
}


rsRetVal rsCStrAppendChar(cstr_t *pThis, uchar c)
{
	DEFiRet;

	rsCHECKVALIDOBJECT(pThis, OIDrsCStr);

	if(pThis->iStrLen >= pThis->iBufSize) {  
		CHKiRet(rsCStrExtendBuf(pThis, 1)); /* need more memory! */
	}

	/* ok, when we reach this, we have sufficient memory */
	*(pThis->pBuf + pThis->iStrLen++) = c;

	/* check if we need to invalidate an sz representation! */
	if(pThis->pszBuf != NULL) {
		free(pThis->pszBuf);
		pThis->pszBuf = NULL;
	}

finalize_it:
	RETiRet;
}


/* Sets the string object to the classigal sz-string provided.
 * Any previously stored vlaue is discarded. If a NULL pointer
 * the the new value (pszNew) is provided, an empty string is
 * created (this is NOT an error!). Property iAllocIncrement is
 * not modified by this function.
 * rgerhards, 2005-10-18
 */
rsRetVal rsCStrSetSzStr(cstr_t *pThis, uchar *pszNew)
{
	rsCHECKVALIDOBJECT(pThis, OIDrsCStr);

	if(pThis->pBuf != NULL)
		free(pThis->pBuf);
	if(pThis->pszBuf != NULL)
		free(pThis->pszBuf);
	if(pszNew == NULL) {
		pThis->iStrLen = 0;
		pThis->iBufSize = 0;
		pThis->pBuf = NULL;
		pThis->pszBuf = NULL;
	} else {
		pThis->iStrLen = strlen((char*)pszNew);
		pThis->iBufSize = pThis->iStrLen;
		pThis->pszBuf = NULL;
		/* iAllocIncrement is NOT modified! */

		/* now save the new value */
		if((pThis->pBuf = (uchar*) malloc(sizeof(uchar) * pThis->iStrLen)) == NULL) {
			RSFREEOBJ(pThis);
			return RS_RET_OUT_OF_MEMORY;
		}

		/* we do NOT need to copy the \0! */
		memcpy(pThis->pBuf, pszNew, pThis->iStrLen);
	}

	return RS_RET_OK;
}

/* Converts the CStr object to a classical sz string and returns that.
 * Same restrictions as in rsCStrGetSzStr() applies (see there!). This
 * function here guarantees that a valid string is returned, even if
 * the CStr object currently holds a NULL pointer string buffer. If so,
 * "" is returned.
 * rgerhards 2005-10-19
 * WARNING: The returned pointer MUST NOT be freed, as it may be
 *          obtained from that constant memory pool (in case of NULL!)
 */
uchar*  rsCStrGetSzStrNoNULL(cstr_t *pThis)
{
	rsCHECKVALIDOBJECT(pThis, OIDrsCStr);
	if(pThis->pBuf == NULL)
		return (uchar*) "";
	else
		return rsCStrGetSzStr(pThis);
}


/* Converts the CStr object to a classical zero-terminated C string
 * and returns that string. The caller must not free it and must not
 * destroy the CStr object as long as the ascii string is used.
 * This function may return NULL, if the string is currently NULL. This
 * is a feature, not a bug. If you need non-NULL in any case, use
 * rsCStrGetSzStrNoNULL() instead.
 * rgerhards, 2005-09-15
 */
uchar*  rsCStrGetSzStr(cstr_t *pThis)
{
	size_t i;

	rsCHECKVALIDOBJECT(pThis, OIDrsCStr);

	if(pThis->pBuf != NULL)
		if(pThis->pszBuf == NULL) {
			/* we do not yet have a usable sz version - so create it... */
			if((pThis->pszBuf = malloc((pThis->iStrLen + 1) * sizeof(uchar))) == NULL) {
				/* TODO: think about what to do - so far, I have no bright
				 *       idea... rgerhards 2005-09-07
				 */
			}
			else { /* we can create the sz String */
				/* now copy it while doing a sanity check. The string might contain a
				 * \0 byte. There is no way how a sz string can handle this. For
				 * the time being, we simply replace it with space - something that
				 * could definitely be improved (TODO).
				 * 2005-09-15 rgerhards
				 */
				for(i = 0 ; i < pThis->iStrLen ; ++i) {
					if(pThis->pBuf[i] == '\0')
						pThis->pszBuf[i] = ' ';
					else
						pThis->pszBuf[i] = pThis->pBuf[i];
				}
				/* write terminator... */
				pThis->pszBuf[i] = '\0';
			}
		}

	return(pThis->pszBuf);
}


/* Converts the CStr object to a classical zero-terminated C string,
 * returns that string and destroys the CStr object. The returned string
 * MUST be freed by the caller. The function might return NULL if
 * no memory can be allocated.
 *
 * TODO:
 * This function should at some time become special. The base idea is to
 * add one extra byte to the end of the regular buffer, so that we can
 * convert it to an szString without the need to copy. The extra memory
 * footprint is not hefty, but the performance gain is potentially large.
 * To get it done now, I am not doing the optimiziation right now.
 * rgerhards, 2005-09-07
 *
 * rgerhards, 2007-09-04: I have changed the interface of this function. It now
 * returns an rsRetVal, so that we can communicate back if we have an error.
 * Using the standard method is much better than returning NULL. Secondly, NULL
 * was not actually an error - it was in indication if the string was empty.
 * This was needed in some parts of the code, in others not. I have now added
 * a second parameter to specify what the caller needs. I hope these changes
 * will make it less likely that the function is called incorrectly, what
 * previously happend quite often and was the cause of a number of program
 * aborts. So the parameters are now:
 * pointer to the object, pointer to string-pointer to receive string and
 * bRetNULL: 0 - must not return NULL on empty string, return "" in that
 * case, 1 - return NULL instead of an empty string.
 * PLEASE NOTE: the caller must free the memory returned in ppSz in any case
 * (except, of course, if it is NULL).
 */
rsRetVal rsCStrConvSzStrAndDestruct(cstr_t *pThis, uchar **ppSz, int bRetNULL)
{
	DEFiRet;
	uchar* pRetBuf;

	rsCHECKVALIDOBJECT(pThis, OIDrsCStr);
	assert(ppSz != NULL);
	assert(bRetNULL == 0 || bRetNULL == 1);

	if(pThis->pBuf == NULL) {
		if(bRetNULL == 0) {
			if((pRetBuf = malloc(sizeof(uchar))) == NULL)
				ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
			*pRetBuf = '\0';
		} else {
			pRetBuf = NULL;
		}
	} else
		pRetBuf = rsCStrGetSzStr(pThis);
	
	*ppSz = pRetBuf;

finalize_it:
	/* We got it, now free the object ourselfs. Please note
	 * that we can NOT use the rsCStrDestruct function as it would
	 * also free the sz String buffer, which we pass on to the user.
	 */
	if(pThis->pBuf != NULL)
		free(pThis->pBuf);
	RSFREEOBJ(pThis);
	
	RETiRet;
}


#if STRINGBUF_TRIM_ALLOCSIZE == 1
	/* Only in this mode, we need to trim the string. To do
	 * so, we must allocate a new buffer of the exact 
	 * string size, and then copy the old one over. 
	 */
	/* WARNING
	 * STRINGBUF_TRIM_ALLOCSIZE can, in theory, be used to trim
	 * memory buffers. This part of the code was inherited from
	 * liblogging (where it is used in a different context) but
	 * never put to use in rsyslog. The reason is that it is hardly
	 * imaginable where the extra performance cost is worth the save
	 * in memory alloc. Then Anders Blomdel rightfully pointed out that
	 * the code does not work at all - and nobody even know that it
	 * probably shouldn't. Rather than removing, I deciced to somewhat
	 * fix the code, so that this feature may be enabled if somebody
	 * really has a need for it. Be warned, however, that I NEVER
	 * tested the fix. So if you intend to use this feature, you must
	 * do full testing before you rely on it. -- rgerhards, 2008-02-12
	 */
rsRetVal  rsCStrFinish(cstr_t __attribute__((unused)) *pThis)
{
	DEFiRet;
	uchar* pBuf;
	rsCHECKVALIDOBJECT(pThis, OIDrsCStr);

	if((pBuf = malloc((pThis->iStrLen) * sizeof(uchar))) == NULL)
	{	/* OK, in this case we use the previous buffer. At least
		 * we have it ;)
		 */
	}
	else
	{	/* got the new buffer, so let's use it */
		memcpy(pBuf, pThis->pBuf, pThis->iStrLen);
		pThis->pBuf = pBuf;
	}

	RETiRet;
}
#endif 	/* #if STRINGBUF_TRIM_ALLOCSIZE == 1 */


void rsCStrSetAllocIncrement(cstr_t *pThis, int iNewIncrement)
{
	rsCHECKVALIDOBJECT(pThis, OIDrsCStr);
	assert(iNewIncrement > 0);

	pThis->iAllocIncrement = iNewIncrement;
}


/* return the length of the current string
 * 2005-09-09 rgerhards
 * Please note: this is only a function in a debug build.
 * For release builds, it is a macro defined in stringbuf.h.
 * This is due to performance reasons.
 */
#ifndef NDEBUG
int rsCStrLen(cstr_t *pThis)
{
	rsCHECKVALIDOBJECT(pThis, OIDrsCStr);
	return(pThis->iStrLen);
}
#endif

/* Truncate characters from the end of the string.
 * rgerhards 2005-09-15
 */
rsRetVal rsCStrTruncate(cstr_t *pThis, size_t nTrunc)
{
	rsCHECKVALIDOBJECT(pThis, OIDrsCStr);

	if(pThis->iStrLen < nTrunc)
		return RS_TRUNCAT_TOO_LARGE;
	
	pThis->iStrLen -= nTrunc;

	if(pThis->pszBuf != NULL) {
		/* in this case, we adjust the psz representation
		 * by writing a new \0 terminator - this is by far
		 * the fastest way and outweights the additional memory
		 * required. 2005-9-19 rgerhards.
		 */
		 pThis->pszBuf[pThis->iStrLen] = '\0';
	}

	return RS_RET_OK;
}

/* Trim trailing whitespace from a given string
 */
rsRetVal rsCStrTrimTrailingWhiteSpace(cstr_t *pThis)
{
	register int i;
	register uchar *pC;
	rsCHECKVALIDOBJECT(pThis, OIDrsCStr);

	i = pThis->iStrLen;
	pC = pThis->pBuf + i - 1;
	while(i > 0 && isspace((int)*pC)) {
		--pC;
		--i;
	}
	/* i now is the new string length! */
	pThis->iStrLen = i;

	return RS_RET_OK;
}

/* compare two string objects - works like strcmp(), but operates
 * on CStr objects. Please note that this version here is
 * faster in the majority of cases, simply because it can
 * rely on StrLen.
 * rgerhards 2005-09-19
 * fixed bug, in which only the last byte was actually compared
 * in equal-size strings.
 * rgerhards, 2005-09-26
 */
int rsCStrCStrCmp(cstr_t *pCS1, cstr_t *pCS2)
{
	rsCHECKVALIDOBJECT(pCS1, OIDrsCStr);
	rsCHECKVALIDOBJECT(pCS2, OIDrsCStr);
	if(pCS1->iStrLen == pCS2->iStrLen)
		if(pCS1->iStrLen == 0)
			return 0; /* zero-sized string are equal ;) */
		else {  /* we now have two non-empty strings of equal
			 * length, so we need to actually check if they
			 * are equal.
			 */
			register size_t i;
			for(i = 0 ; i < pCS1->iStrLen ; ++i) {
				if(pCS1->pBuf[i] != pCS2->pBuf[i])
					return pCS1->pBuf[i] - pCS2->pBuf[i];
			}
			/* if we arrive here, the strings are equal */
			return 0;
		}
	else
		return pCS1->iStrLen - pCS2->iStrLen;
}


/* check if a sz-type string starts with a CStr object. This function
 * is initially written to support the "startswith" property-filter
 * comparison operation. Maybe it also has other needs.
 * This functions is modelled after the strcmp() series, thus a
 * return value of 0 indicates that the string starts with the
 * sequence while -1 indicates it does not!
 * rgerhards 2005-10-19
 */
int rsCStrSzStrStartsWithCStr(cstr_t *pCS1, uchar *psz, size_t iLenSz)
{
	register int i;
	int iMax;

	rsCHECKVALIDOBJECT(pCS1, OIDrsCStr);
	assert(psz != NULL);
	assert(iLenSz == strlen((char*)psz)); /* just make sure during debugging! */
	if(iLenSz >= pCS1->iStrLen) {
		/* we need to checkusing pCS1->iStrLen charactes at maximum, thus
		 * we move it to iMax.
		 */
		iMax = pCS1->iStrLen;
		if(iMax == 0)
			return 0; /* yes, it starts with a zero-sized string ;) */
		else {  /* we now have something to compare, so let's do it... */
			for(i = 0 ; i < iMax ; ++i) {
				if(psz[i] != pCS1->pBuf[i])
					return psz[i] - pCS1->pBuf[i];
			}
			/* if we arrive here, the string actually starts with pCS1 */
			return 0;
		}
	}
	else
		return -1; /* pCS1 is less then psz */
}


/* check if a CStr object starts with a sz-type string.
 * This functions is modelled after the strcmp() series, thus a
 * return value of 0 indicates that the string starts with the
 * sequence while -1 indicates it does not!
 * rgerhards 2005-09-26
 */
int rsCStrStartsWithSzStr(cstr_t *pCS1, uchar *psz, size_t iLenSz)
{
	register size_t i;

	rsCHECKVALIDOBJECT(pCS1, OIDrsCStr);
	assert(psz != NULL);
	assert(iLenSz == strlen((char*)psz)); /* just make sure during debugging! */
	if(pCS1->iStrLen >= iLenSz) {
		/* we are using iLenSz below, because we need to check
		 * iLenSz characters at maximum (start with!)
		 */
		if(iLenSz == 0)
			return 0; /* yes, it starts with a zero-sized string ;) */
		else {  /* we now have something to compare, so let's do it... */
			for(i = 0 ; i < iLenSz ; ++i) {
				if(pCS1->pBuf[i] != psz[i])
					return pCS1->pBuf[i] - psz[i];
			}
			/* if we arrive here, the string actually starts with psz */
			return 0;
		}
	}
	else
		return -1; /* pCS1 is less then psz */
}


/* The same as rsCStrStartsWithSzStr(), but does a case-insensitive
 * comparison. TODO: consolidate the two.
 * rgerhards 2008-02-28
 */
int rsCStrCaseInsensitveStartsWithSzStr(cstr_t *pCS1, uchar *psz, size_t iLenSz)
{
	register size_t i;

	rsCHECKVALIDOBJECT(pCS1, OIDrsCStr);
	assert(psz != NULL);
	assert(iLenSz == strlen((char*)psz)); /* just make sure during debugging! */
	if(pCS1->iStrLen >= iLenSz) {
		/* we are using iLenSz below, because we need to check
		 * iLenSz characters at maximum (start with!)
		 */
		if(iLenSz == 0)
			return 0; /* yes, it starts with a zero-sized string ;) */
		else {  /* we now have something to compare, so let's do it... */
			for(i = 0 ; i < iLenSz ; ++i) {
				if(tolower(pCS1->pBuf[i]) != tolower(psz[i]))
					return tolower(pCS1->pBuf[i]) - tolower(psz[i]);
			}
			/* if we arrive here, the string actually starts with psz */
			return 0;
		}
	}
	else
		return -1; /* pCS1 is less then psz */
}

/* check if a CStr object matches a regex.
 * msamia@redhat.com 2007-07-12
 * @return returns 0 if matched
 * bug: doesn't work for CStr containing \0
 * rgerhards, 2007-07-16: bug is no real bug, because rsyslogd ensures there
 * never is a \0 *inside* a property string.
 * Note that the function returns -1 if regexp functionality is not available.
 * TODO: change calling interface! -- rgerhards, 2008-03-07
 */
int rsCStrSzStrMatchRegex(cstr_t *pCS1, uchar *psz)
{
	regex_t preq;
	int ret;

	BEGINfunc

	if(objUse(regexp, LM_REGEXP_FILENAME) == RS_RET_OK) {
		regexp.regcomp(&preq, (char*) rsCStrGetSzStr(pCS1), 0);
		ret = regexp.regexec(&preq, (char*) psz, 0, NULL, 0);
		regexp.regfree(&preq);
	} else {
		ret = 1; /* simulate "not found" */
	}

	ENDfunc
	return ret;
}


/* compare a rsCStr object with a classical sz string.  This function
 * is almost identical to rsCStrZsStrCmp(), but it also takes an offset
 * to the CStr object from where the comparison is to start.
 * I have thought quite a while if it really makes sense to more or
 * less duplicate the code. After all, if you call it with an offset of
 * zero, the functionality is exactly the same. So it looks natural to
 * just have a single function. However, supporting the offset requires
 * some (few) additional integer operations. While they are few, they
 * happen at places in the code that is run very frequently. All in all,
 * I have opted for performance and thus duplicated the code. I hope
 * this is a good, or at least acceptable, compromise.
 * rgerhards, 2005-09-26
 * This function also has an offset-pointer which allows to
 * specify *where* the compare operation should begin in
 * the CStr. If everything is to be compared, it must be set
 * to 0. If some leading bytes are to be skipped, it must be set
 * to the first index that is to be compared. It must not be
 * set higher than the string length (this is considered a
 * program bug and will lead to unpredictable results and program aborts).
 * rgerhards 2005-09-26
 */
int rsCStrOffsetSzStrCmp(cstr_t *pCS1, size_t iOffset, uchar *psz, size_t iLenSz)
{
	BEGINfunc
	rsCHECKVALIDOBJECT(pCS1, OIDrsCStr);
	assert(iOffset < pCS1->iStrLen);
	assert(psz != NULL);
	assert(iLenSz == strlen((char*)psz)); /* just make sure during debugging! */
	if((pCS1->iStrLen - iOffset) == iLenSz) {
		/* we are using iLenSz below, because the lengths
		 * are equal and iLenSz is faster to access
		 */
		if(iLenSz == 0) {
			return 0; /* zero-sized strings are equal ;) */
			ENDfunc
		} else {  /* we now have two non-empty strings of equal
			 * length, so we need to actually check if they
			 * are equal.
			 */
			register size_t i;
			for(i = 0 ; i < iLenSz ; ++i) {
				if(pCS1->pBuf[i+iOffset] != psz[i])
					return pCS1->pBuf[i+iOffset] - psz[i];
			}
			/* if we arrive here, the strings are equal */
			return 0;
			ENDfunc
		}
	}
	else {
		return pCS1->iStrLen - iOffset - iLenSz;
		ENDfunc
	}
}


/* Converts a string to a number. If the string dos not contain a number, 
 * RS_RET_NOT_A_NUMBER is returned and the contents of pNumber is undefined.
 * If all goes well, pNumber contains the number that the string was converted
 * to.
 */
rsRetVal
rsCStrConvertToNumber(cstr_t *pStr, number_t *pNumber)
{
	DEFiRet;
	number_t n;
	int bIsNegative;
	size_t i;

	ASSERT(pStr != NULL);
	ASSERT(pNumber != NULL);

	if(pStr->iStrLen == 0) {
		/* can be converted to 0! (by convention) */
		pNumber = 0;
		FINALIZE;
	}

	/* first skip whitespace (if present) */
	for(i = 0 ; i < pStr->iStrLen && isspace(pStr->pBuf[i]) ; ++i) {
		/*DO NOTHING*/
	}

	/* we have a string, so let's check its syntax */
	if(pStr->pBuf[i] == '+') {
		++i; /* skip that char */
		bIsNegative = 0;
	} else if(pStr->pBuf[0] == '-') {
		++i; /* skip that char */
		bIsNegative = 1;
	} else {
		bIsNegative = 0;
	}

	/* TODO: octal? hex? */
	n = 0;
	while(i < pStr->iStrLen && isdigit(pStr->pBuf[i])) {
		n = n * 10 + pStr->pBuf[i] * 10;
		++i;
	}
	
	if(i < pStr->iStrLen) /* non-digits before end of string? */
		ABORT_FINALIZE(RS_RET_NOT_A_NUMBER);

	if(bIsNegative)
		n *= -1;

	/* we got it, so return the number */
	*pNumber = n;

finalize_it:
	RETiRet;
}


/* Converts a string to a boolen. First tries to convert to a number. If
 * that succeeds, we are done (number is then used as boolean value). If
 * that fails, we look if the string is "yes" or "true". If so, a value
 * of 1 is returned. In all other cases, a value of 0 is returned. Please
 * note that we do not have a specific boolean type, so we return a number.
 * so, these are 
 * RS_RET_NOT_A_NUMBER is returned and the contents of pNumber is undefined.
 * If all goes well, pNumber contains the number that the string was converted
 * to.
 */
rsRetVal
rsCStrConvertToBool(cstr_t *pStr, number_t *pBool)
{
	DEFiRet;

	ASSERT(pStr != NULL);
	ASSERT(pBool != NULL);

	iRet = rsCStrConvertToNumber(pStr, pBool);

	if(iRet != RS_RET_NOT_A_NUMBER) {
		FINALIZE; /* in any case, we have nothing left to do */
	}

	/* TODO: maybe we can do better than strcasecmp ;) -- overhead! */
	if(!strcasecmp((char*)rsCStrGetSzStr(pStr), "true")) {
		*pBool = 1;
	} else if(!strcasecmp((char*)rsCStrGetSzStr(pStr), "yes")) {
		*pBool = 1;
	} else {
		*pBool = 0;
	}

finalize_it:
	RETiRet;
}


/* compare a rsCStr object with a classical sz string.
 * Just like rsCStrCStrCmp, just for a different data type.
 * There must not only the sz string but also its length be
 * provided. If the caller does not know the length he can
 * call with
 * rsCstrSzStrCmp(pCS, psz, strlen((char*)psz));
 * we are not doing the strlen((char*)) ourselfs as the caller might
 * already know the length and in such cases we can save the
 * overhead of doing it one more time (strelen() is costly!).
 * The bottom line is that the provided length MUST be correct!
 * The to sz string pointer must not be NULL!
 * rgerhards 2005-09-26
 */
int rsCStrSzStrCmp(cstr_t *pCS1, uchar *psz, size_t iLenSz)
{
	rsCHECKVALIDOBJECT(pCS1, OIDrsCStr);
	assert(psz != NULL);
	assert(iLenSz == strlen((char*)psz)); /* just make sure during debugging! */
	if(pCS1->iStrLen == iLenSz)
		/* we are using iLenSz below, because the lengths
		 * are equal and iLenSz is faster to access
		 */
		if(iLenSz == 0)
			return 0; /* zero-sized strings are equal ;) */
		else {  /* we now have two non-empty strings of equal
			 * length, so we need to actually check if they
			 * are equal.
			 */
			register size_t i;
			for(i = 0 ; i < iLenSz ; ++i) {
				if(pCS1->pBuf[i] != psz[i])
					return pCS1->pBuf[i] - psz[i];
			}
			/* if we arrive here, the strings are equal */
			return 0;
		}
	else
		return pCS1->iStrLen - iLenSz;
}


/* Locate the first occurence of this rsCStr object inside a standard sz string.
 * Returns the offset (0-bound) of this first occurrence. If not found, -1 is
 * returned. Both parameters MUST be given (NULL is not allowed).
 * rgerhards 2005-09-19
 */
int rsCStrLocateInSzStr(cstr_t *pThis, uchar *sz)
{
	int i;
	int iMax;
	int bFound;
	rsCHECKVALIDOBJECT(pThis, OIDrsCStr);
	assert(sz != NULL);
	
	if(pThis->iStrLen == 0)
		return 0;
	
	/* compute the largest index where a match could occur - after all,
	 * the to-be-located string must be able to be present in the 
	 * searched string (it needs its size ;)).
	 */
	iMax = strlen((char*)sz) - pThis->iStrLen;

	bFound = 0;
	i = 0;
	while(i  <= iMax && !bFound) {
		size_t iCheck;
		uchar *pComp = sz + i;
		for(iCheck = 0 ; iCheck < pThis->iStrLen ; ++iCheck)
			if(*(pComp + iCheck) != *(pThis->pBuf + iCheck))
				break;
		if(iCheck == pThis->iStrLen)
			bFound = 1; /* found! - else it wouldn't be equal */
		else
			++i; /* on to the next try */
	}

	return(bFound ? i : -1);
}


/* This is the same as rsCStrLocateInSzStr(), but does a case-insensitve
 * comparison.
 * TODO: over time, consolidate the two.
 * rgerhards, 2008-02-28
 */
int rsCStrCaseInsensitiveLocateInSzStr(cstr_t *pThis, uchar *sz)
{
	int i;
	int iMax;
	int bFound;
	rsCHECKVALIDOBJECT(pThis, OIDrsCStr);
	assert(sz != NULL);
	
	if(pThis->iStrLen == 0)
		return 0;
	
	/* compute the largest index where a match could occur - after all,
	 * the to-be-located string must be able to be present in the 
	 * searched string (it needs its size ;)).
	 */
	iMax = strlen((char*)sz) - pThis->iStrLen;

	bFound = 0;
	i = 0;
	while(i  <= iMax && !bFound) {
		size_t iCheck;
		uchar *pComp = sz + i;
		for(iCheck = 0 ; iCheck < pThis->iStrLen ; ++iCheck)
			if(tolower(*(pComp + iCheck)) != tolower(*(pThis->pBuf + iCheck)))
				break;
		if(iCheck == pThis->iStrLen)
			bFound = 1; /* found! - else it wouldn't be equal */
		else
			++i; /* on to the next try */
	}

	return(bFound ? i : -1);
}


#if 0	 /* read comment below why this is commented out. In short: for future use! */
/* locate the first occurence of a standard sz string inside a rsCStr object.
 * Returns the offset (0-bound) of this first occurrence. If not found, -1 is
 * returned.
 * rgerhards 2005-09-19
 * WARNING: I accidently created this function (I later noticed I didn't relly
 *          need it... I will not remove the function, as it probably is useful
 *          some time later. However, it is not fully tested, so start with testing
 *          it before you put it to first use).
 */
int rsCStrLocateSzStr(cstr_t *pThis, uchar *sz)
{
	int iLenSz;
	int i;
	int iMax;
	int bFound;
	rsCHECKVALIDOBJECT(pThis, OIDrsCStr);
	
	if(sz == NULL)
		return 0;

	iLenSz = strlen((char*)sz);
	if(iLenSz == 0)
		return 0;
	
	/* compute the largest index where a match could occur - after all,
	 * the to-be-located string must be able to be present in the 
	 * searched string (it needs its size ;)).
	 */
	iMax = pThis->iStrLen - iLenSz;

	bFound = 0;
	i = 0;
	while(i  < iMax && !bFound) {
		int iCheck;
		uchar *pComp = pThis->pBuf + i;
		for(iCheck = 0 ; iCheck < iLenSz ; ++iCheck)
			if(*(pComp + iCheck) != *(sz + iCheck))
				break;
		if(iCheck == iLenSz)
			bFound = 1; /* found! - else it wouldn't be equal */
		else
			++i; /* on to the next try */
	}

	return(bFound ? i : -1);
}
#endif /* end comment out */


/* our exit function. TODO: remove once converted to a class
 * rgerhards, 2008-03-11
 */
rsRetVal strExit()
{
	DEFiRet;
	objRelease(regexp, LM_REGEXP_FILENAME);
	RETiRet;
}


/* our init function. TODO: remove once converted to a class
 */
rsRetVal strInit()
{
	DEFiRet;
	CHKiRet(objGetObjInterface(&obj));

finalize_it:
	RETiRet;
}


/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 * End:
 * vi:set ai:
 */
