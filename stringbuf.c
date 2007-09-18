/* This is the byte-counted string class for rsyslog. It is a replacement
 * for classical \0 terminated string functions. We introduce it in
 * the hope it will make the program more secure, obtain some performance
 * and, most importantly, lay they foundation for syslog-protocol, which
 * requires strings to be able to handle embedded \0 characters.
 * Please see syslogd.c for license information.
 * All functions in this "class" start with rsCStr (rsyslog Counted String).
 * This code is placed under the GPL.
 * begun 2005-09-07 rgerhards
 */
#include "config.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <regex.h>
#include "rsyslog.h"
#include "stringbuf.h"
#include "srUtils.h"


/* ################################################################# *
 * private members                                                   *
 * ################################################################# */



/* ################################################################# *
 * public members                                                    *
 * ################################################################# */


rsCStrObj *rsCStrConstruct(void)
{
	rsCStrObj *pThis;

	if((pThis = (rsCStrObj*) calloc(1, sizeof(rsCStrObj))) == NULL)
		return NULL;

	rsSETOBJTYPE(pThis, OIDrsCStr);
	pThis->pBuf = NULL;
	pThis->pszBuf = NULL;
	pThis->iBufSize = 0;
	pThis->iStrLen = 0;
	pThis->iAllocIncrement = RS_STRINGBUF_ALLOC_INCREMENT;

	return pThis;
}

/* construct from sz string
 * rgerhards 2005-09-15
 */
rsRetVal rsCStrConstructFromszStr(rsCStrObj **ppThis, uchar *sz)
{
	rsCStrObj *pThis;

	assert(ppThis != NULL);

	if((pThis = rsCStrConstruct()) == NULL)
		return RS_RET_OUT_OF_MEMORY;

	pThis->iBufSize = pThis->iStrLen = strlen((char*)(char *) sz);
	if((pThis->pBuf = (uchar*) malloc(sizeof(uchar) * pThis->iStrLen)) == NULL) {
		RSFREEOBJ(pThis);
		return RS_RET_OUT_OF_MEMORY;
	}

	/* we do NOT need to copy the \0! */
	memcpy(pThis->pBuf, sz, pThis->iStrLen);

	*ppThis = pThis;
	return RS_RET_OK;
}

/* construct from CStr object. only the counted string is
 * copied, not the szString.
 * rgerhards 2005-10-18
 */
rsRetVal rsCStrConstructFromCStr(rsCStrObj **ppThis, rsCStrObj *pFrom)
{
	rsCStrObj *pThis;

	assert(ppThis != NULL);
	rsCHECKVALIDOBJECT(pFrom, OIDrsCStr);

	if((pThis = rsCStrConstruct()) == NULL)
		return RS_RET_OUT_OF_MEMORY;

	pThis->iBufSize = pThis->iStrLen = pFrom->iStrLen;
	if((pThis->pBuf = (uchar*) malloc(sizeof(uchar) * pThis->iStrLen)) == NULL) {
		RSFREEOBJ(pThis);
		return RS_RET_OUT_OF_MEMORY;
	}

	/* copy properties */
	memcpy(pThis->pBuf, pFrom->pBuf, pThis->iStrLen);

	*ppThis = pThis;
	return RS_RET_OK;
}


void rsCStrDestruct(rsCStrObj *pThis)
{
	/* rgerhards 2005-10-19: The free of pBuf was contained in conditional compilation.
	 * The code was only compiled if STRINGBUF_TRIM_ALLOCSIZE was set to 1. I honestly
	 * do not know why it was so, I think it was an artifact. Anyhow, I have changed this
	 * now. Should there any issue occur, this comment hopefully will shed some light 
	 * on what happened. I re-verified, and this function has never before been called
	 * by anyone. So changing it can have no impact for obvious reasons...
	 */
	if(pThis->pBuf != NULL) {
		free(pThis->pBuf);
	}

	if(pThis->pszBuf != NULL) {
		free(pThis->pszBuf);
	}

	RSFREEOBJ(pThis);
}


rsRetVal rsCStrAppendStrWithLen(rsCStrObj *pThis, uchar* psz, size_t iStrLen)
{
	rsRetVal iRet;
	int iOldAllocInc;

	rsCHECKVALIDOBJECT(pThis, OIDrsCStr);
	assert(psz != NULL);

	/* we first check if the to-be-added string is larger than the
	 * alloc increment. If so, we temporarily increase the alloc
	 * increment to the length of the string. This will ensure that
	 * one string copy will be needed at most. As this is a very
	 * costly operation, it outweights the cost of the strlen((char*)) and
	 * related stuff - at least I think so.
	 * rgerhards 2005-09-22
	 */
	/* We save the current alloc increment in any case, so we can just
	 * overwrite it below, this is faster than any if-construct.
	 */
	iOldAllocInc = pThis->iAllocIncrement;
	if(iStrLen > pThis->iAllocIncrement) {
		pThis->iAllocIncrement = iStrLen;
	}

	while(*psz)
		if((iRet = rsCStrAppendChar(pThis, *psz++)) != RS_RET_OK)
			return iRet;

	pThis->iAllocIncrement = iOldAllocInc; /* restore */
	return RS_RET_OK;
}


/* changed to be a wrapper to rsCStrAppendStrWithLen() so that
 * we can save some time when we have the length but do not
 * need to change existing code.
 * rgerhards, 2007-07-03
 */
rsRetVal rsCStrAppendStr(rsCStrObj *pThis, uchar* psz)
{
	return rsCStrAppendStrWithLen(pThis, psz, strlen((char*)(char*) psz));
}


rsRetVal rsCStrAppendInt(rsCStrObj *pThis, int i)
{
	rsRetVal iRet;
	uchar szBuf[32];

	rsCHECKVALIDOBJECT(pThis, OIDrsCStr);

	if((iRet = srUtilItoA((char*) szBuf, sizeof(szBuf), i)) != RS_RET_OK)
		return iRet;

	return rsCStrAppendStr(pThis, szBuf);
}


rsRetVal rsCStrAppendChar(rsCStrObj *pThis, uchar c)
{
	uchar* pNewBuf;

	rsCHECKVALIDOBJECT(pThis, OIDrsCStr);

	if(pThis->iStrLen >= pThis->iBufSize)
	{  /* need more memory! */
		if((pNewBuf = (uchar*) malloc((pThis->iBufSize + pThis->iAllocIncrement) * sizeof(uchar))) == NULL)
			return RS_RET_OUT_OF_MEMORY;
		memcpy(pNewBuf, pThis->pBuf, pThis->iBufSize);
		pThis->iBufSize += pThis->iAllocIncrement;
		if(pThis->pBuf != NULL) {
			free(pThis->pBuf);
		}
		pThis->pBuf = pNewBuf;
	}

	/* ok, when we reach this, we have sufficient memory */
	*(pThis->pBuf + pThis->iStrLen++) = c;

	/* check if we need to invalidate an sz representation! */
	if(pThis->pszBuf != NULL) {
		free(pThis->pszBuf);
		pThis->pszBuf = NULL;
	}

	return RS_RET_OK;
}


/* Sets the string object to the classigal sz-string provided.
 * Any previously stored vlaue is discarded. If a NULL pointer
 * the the new value (pszNew) is provided, an empty string is
 * created (this is NOT an error!). Property iAllocIncrement is
 * not modified by this function.
 * rgerhards, 2005-10-18
 */
rsRetVal rsCStrSetSzStr(rsCStrObj *pThis, uchar *pszNew)
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
uchar*  rsCStrGetSzStrNoNULL(rsCStrObj *pThis)
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
uchar*  rsCStrGetSzStr(rsCStrObj *pThis)
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
rsRetVal rsCStrConvSzStrAndDestruct(rsCStrObj *pThis, uchar **ppSz, int bRetNULL)
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
	
	return(iRet);
}


rsRetVal  rsCStrFinish(rsCStrObj __attribute__((unused)) *pThis)
{
	rsCHECKVALIDOBJECT(pThis, OIDrsCStr);

#	if STRINGBUF_TRIM_ALLOCSIZE == 1
	/* in this mode, we need to trim the string. To do
	 * so, we must allocate a new buffer of the exact 
	 * string size, and then copy the old one over. 
	 * This new buffer is then to be returned.
	 */
	if((pRetBuf = malloc((pThis->iBufSize) * sizeof(uchar))) == NULL)
	{	/* OK, in this case we use the previous buffer. At least
		 * we have it ;)
		 */
	}
	else
	{	/* got the new buffer, so let's use it */
		uchar* pBuf;
		memcpy(pBuf, pThis->pBuf, pThis->iBufPtr + 1);
		pThis->pBuf = pBuf;
	}
#	else
	/* here, we need to do ... nothing ;)
	 */
#	endif
	return RS_RET_OK;
}

void rsCStrSetAllocIncrement(rsCStrObj *pThis, int iNewIncrement)
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
int rsCStrLen(rsCStrObj *pThis)
{
	rsCHECKVALIDOBJECT(pThis, OIDrsCStr);
	return(pThis->iStrLen);
}
#endif

/* Truncate characters from the end of the string.
 * rgerhards 2005-09-15
 */
rsRetVal rsCStrTruncate(rsCStrObj *pThis, size_t nTrunc)
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
rsRetVal rsCStrTrimTrailingWhiteSpace(rsCStrObj *pThis)
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
int rsCStrCStrCmp(rsCStrObj *pCS1, rsCStrObj *pCS2)
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


/* check if a sz-type string start with a CStr object. This function
 * is initially written to support the "startswith" property-filter
 * comparison operation. Maybe it also has other needs.
 * rgerhards 2005-10-19
 */
int rsCStrSzStrStartsWithCStr(rsCStrObj *pCS1, uchar *psz, size_t iLenSz)
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
 * rgerhards 2005-09-26
 */
int rsCStrStartsWithSzStr(rsCStrObj *pCS1, uchar *psz, size_t iLenSz)
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

/* check if a CStr object matches a regex.
 * msamia@redhat.com 2007-07-12
 * @return returns 0 if matched
 * bug: doesn't work for CStr containing \0
 * rgerhards, 2007-07-16: bug is no real bug, because rsyslogd ensures there
 * never is a \0 *inside* a property string.
 */
int rsCStrSzStrMatchRegex(rsCStrObj *pCS1, uchar *psz)
{
    regex_t preq;
    regcomp(&preq, (char*) rsCStrGetSzStr(pCS1), 0);
    int iRet = regexec(&preq, (char*) psz, 0, NULL, 0);
    regfree(&preq);
    return iRet;
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
int rsCStrOffsetSzStrCmp(rsCStrObj *pCS1, size_t iOffset, uchar *psz, size_t iLenSz)
{
	rsCHECKVALIDOBJECT(pCS1, OIDrsCStr);
	assert(iOffset < pCS1->iStrLen);
	assert(psz != NULL);
	assert(iLenSz == strlen((char*)psz)); /* just make sure during debugging! */
	if((pCS1->iStrLen - iOffset) == iLenSz) {
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
				if(pCS1->pBuf[i+iOffset] != psz[i])
					return pCS1->pBuf[i+iOffset] - psz[i];
			}
			/* if we arrive here, the strings are equal */
			return 0;
		}
	}
	else
		return pCS1->iStrLen - iOffset - iLenSz;
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
int rsCStrSzStrCmp(rsCStrObj *pCS1, uchar *psz, size_t iLenSz)
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
int rsCStrLocateInSzStr(rsCStrObj *pThis, uchar *sz)
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
int rsCStrLocateSzStr(rsCStrObj *pThis, uchar *sz)
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


/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 * End:
 * vi:set ai:
 */
