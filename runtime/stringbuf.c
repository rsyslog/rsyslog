/* This is the byte-counted string class for rsyslog. It is a replacement
 * for classical \0 terminated string functions. We introduce it in
 * the hope it will make the program more secure, obtain some performance
 * and, most importantly, lay they foundation for syslog-protocol, which
 * requires strings to be able to handle embedded \0 characters.
 * Please see syslogd.c for license information.
 * All functions in this "class" start with rsCStr (rsyslog Counted String).
 * begun 2005-09-07 rgerhards
 * did some optimization (read: bugs!) rgerhards, 2009-06-16
 *
 * Copyright (C) 2007-2012 Adiscon GmbH
 *
 * This file is part of the rsyslog runtime library.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *       http://www.apache.org/licenses/LICENSE-2.0
 *       -or-
 *       see COPYING.ASL20 in the source distribution
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "config.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <libestr.h>
#include "rsyslog.h"
#include "stringbuf.h"
#include "srUtils.h"
#include "regexp.h"
#include "obj.h"

uchar*  rsCStrGetSzStr(cstr_t *pThis);

/* ################################################################# *
 * private members                                                   *
 * ################################################################# */

/* static data */
DEFobjCurrIf(obj)
DEFobjCurrIf(regexp)

/* ################################################################# *
 * public members                                                    *
 * ################################################################# */


rsRetVal cstrConstruct(cstr_t **ppThis)
{
	DEFiRet;
	cstr_t *pThis;

	ASSERT(ppThis != NULL);

	CHKmalloc(pThis = (cstr_t*) calloc(1, sizeof(cstr_t)));

	rsSETOBJTYPE(pThis, OIDrsCStr);
	pThis->pBuf = NULL;
	pThis->pszBuf = NULL;
	pThis->iBufSize = 0;
	pThis->iStrLen = 0;
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

	pThis->iBufSize = pThis->iStrLen = strlen((char *) sz);
	if((pThis->pBuf = (uchar*) MALLOC(sizeof(uchar) * pThis->iStrLen)) == NULL) {
		RSFREEOBJ(pThis);
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}

	/* we do NOT need to copy the \0! */
	memcpy(pThis->pBuf, sz, pThis->iStrLen);

	*ppThis = pThis;

finalize_it:
	RETiRet;
}


/* construct from es_str_t string
 * rgerhards 2010-12-03
 */
rsRetVal cstrConstructFromESStr(cstr_t **ppThis, es_str_t *str)
{
	DEFiRet;
	cstr_t *pThis;

	assert(ppThis != NULL);

	CHKiRet(rsCStrConstruct(&pThis));

	pThis->iBufSize = pThis->iStrLen = es_strlen(str);
	if((pThis->pBuf = (uchar*) MALLOC(sizeof(uchar) * pThis->iStrLen)) == NULL) {
		RSFREEOBJ(pThis);
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}

	/* we do NOT need to copy the \0! */
	memcpy(pThis->pBuf, es_getBufAddr(str), pThis->iStrLen);

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
	if((pThis->pBuf = (uchar*) MALLOC(sizeof(uchar) * pThis->iStrLen)) == NULL) {
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

	free(pThis->pBuf);
	free(pThis->pszBuf);
	RSFREEOBJ(pThis);
	*ppThis = NULL;
}


/* extend the string buffer if its size is insufficient.
 * Param iMinNeeded is the minumum free space needed. If it is larger
 * than the default alloc increment, space for at least this amount is
 * allocated. In practice, a bit more is allocated because we envision that
 * some more characters may be added after these.
 * rgerhards, 2008-01-07
 * changed to utilized realloc() -- rgerhards, 2009-06-16
 */
rsRetVal
rsCStrExtendBuf(cstr_t *pThis, size_t iMinNeeded)
{
	uchar *pNewBuf;
	size_t iNewSize;
	DEFiRet;

	/* first compute the new size needed */
	if(iMinNeeded > RS_STRINGBUF_ALLOC_INCREMENT) {
		/* we allocate "n" ALLOC_INCREMENTs. Usually, that should
		 * leave some room after the absolutely needed one. It also
		 * reduces memory fragmentation. Note that all of this are
		 * integer operations (very important to understand what is
		 * going on)! Parenthesis are for better readibility.
		 */
		iNewSize = (iMinNeeded / RS_STRINGBUF_ALLOC_INCREMENT + 1) * RS_STRINGBUF_ALLOC_INCREMENT;
	} else {
		iNewSize = pThis->iBufSize + RS_STRINGBUF_ALLOC_INCREMENT;
	}
	iNewSize += pThis->iBufSize; /* add current size */

	/* DEV debugging only: dbgprintf("extending string buffer, old %d, new %d\n", pThis->iBufSize, iNewSize); */
	CHKmalloc(pNewBuf = (uchar*) realloc(pThis->pBuf, iNewSize * sizeof(uchar)));
	pThis->iBufSize = iNewSize;
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
rsRetVal cstrAppendCStr(cstr_t *pThis, cstr_t *pstrAppend)
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


/* Sets the string object to the classigal sz-string provided.
 * Any previously stored vlaue is discarded. If a NULL pointer
 * the the new value (pszNew) is provided, an empty string is
 * created (this is NOT an error!).
 * rgerhards, 2005-10-18
 */
rsRetVal rsCStrSetSzStr(cstr_t *pThis, uchar *pszNew)
{
	rsCHECKVALIDOBJECT(pThis, OIDrsCStr);

	free(pThis->pBuf);
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

		/* now save the new value */
		if((pThis->pBuf = (uchar*) MALLOC(sizeof(uchar) * pThis->iStrLen)) == NULL) {
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
			if((pThis->pszBuf = MALLOC((pThis->iStrLen + 1) * sizeof(uchar))) == NULL) {
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
 * This is the NEW replacement for rsCStrConvSzStrAndDestruct which does
 * no longer utilize a special buffer but soley works on pBuf (and also
 * assumes that cstrFinalize had been called).
 *
 * Parameters are as follows:
 * pointer to the object, pointer to string-pointer to receive string and
 * bRetNULL: 0 - must not return NULL on empty string, return "" in that
 * case, 1 - return NULL instead of an empty string.
 * PLEASE NOTE: the caller must free the memory returned in ppSz in any case
 * (except, of course, if it is NULL).
 */
rsRetVal cstrConvSzStrAndDestruct(cstr_t *pThis, uchar **ppSz, int bRetNULL)
{
	DEFiRet;
	uchar* pRetBuf;

	rsCHECKVALIDOBJECT(pThis, OIDrsCStr);
	assert(ppSz != NULL);
	assert(bRetNULL == 0 || bRetNULL == 1);

	if(pThis->pBuf == NULL) {
		if(bRetNULL == 0) {
			CHKmalloc(pRetBuf = MALLOC(sizeof(uchar)));
			*pRetBuf = '\0';
		} else {
			pRetBuf = NULL;
		}
	} else
		pRetBuf = pThis->pBuf;
	
	*ppSz = pRetBuf;

finalize_it:
	/* We got it, now free the object ourselfs. Please note
	 * that we can NOT use the rsCStrDestruct function as it would
	 * also free the sz String buffer, which we pass on to the user.
	 */
	RSFREEOBJ(pThis);
	RETiRet;
}


/* return the length of the current string
 * 2005-09-09 rgerhards
 * Please note: this is only a function in a debug build.
 * For release builds, it is a macro defined in stringbuf.h.
 * This is due to performance reasons.
 */
#ifndef NDEBUG
int cstrLen(cstr_t *pThis)
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

/* Trim trailing whitespace from a given string
 */
rsRetVal cstrTrimTrailingWhiteSpace(cstr_t *pThis)
{
	register int i;
	register uchar *pC;
	rsCHECKVALIDOBJECT(pThis, OIDrsCStr);

	if(pThis->iStrLen == 0)
		goto done; /* empty string -> nothing to trim ;) */
	i = pThis->iStrLen;
	pC = pThis->pBuf + i - 1;
	while(i > 0 && isspace((int)*pC)) {
		--pC;
		--i;
	}
	/* i now is the new string length! */
	pThis->iStrLen = i;
	pThis->pBuf[pThis->iStrLen] = '0'; /* we always have this space */

done:	return RS_RET_OK;
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
 * rgerhards: 2009-03-04: ERE support added, via parameter iType: 0 - BRE, 1 - ERE
 * Arnaud Cornet/rgerhards: 2009-04-02: performance improvement by caching compiled regex
 * If a caller does not need the cached version, it must still provide memory for it
 * and must call rsCStrRegexDestruct() afterwards.
 */
rsRetVal rsCStrSzStrMatchRegex(cstr_t *pCS1, uchar *psz, int iType, void *rc)
{
	regex_t **cache = (regex_t**) rc;
	int ret;
	DEFiRet;

	assert(pCS1 != NULL);
	assert(psz != NULL);
	assert(cache != NULL);

	if(objUse(regexp, LM_REGEXP_FILENAME) == RS_RET_OK) {
		if (*cache == NULL) {
			*cache = calloc(sizeof(regex_t), 1);
			regexp.regcomp(*cache, (char*) rsCStrGetSzStr(pCS1), (iType == 1 ? REG_EXTENDED : 0) | REG_NOSUB);
		}
		ret = regexp.regexec(*cache, (char*) psz, 0, NULL, 0);
		if(ret != 0)
			ABORT_FINALIZE(RS_RET_NOT_FOUND);
	} else {
		ABORT_FINALIZE(RS_RET_NOT_FOUND);
	}

finalize_it:
	RETiRet;
}


/* free a cached compiled regex
 * Caller must provide a pointer to a buffer that was created by
 * rsCStrSzStrMatchRegexCache()
 */
void rsCStrRegexDestruct(void *rc)
{
	regex_t **cache = rc;
	
	assert(cache != NULL);
	assert(*cache != NULL);

	if(objUse(regexp, LM_REGEXP_FILENAME) == RS_RET_OK) {
		regexp.regfree(*cache);
		free(*cache);
		*cache = NULL;
	}
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
		n = n * 10 + pStr->pBuf[i] - '0';
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


/* vi:set ai:
 */
