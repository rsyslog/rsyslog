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
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "liblogging-stub.h"
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

	pThis->OID = OIDrsCStr;
	pThis->pBuf = NULL;
	pThis->pszBuf = NULL;
	pThis->iBufSize = 0;
	pThis->iBufPtr = 0;
	pThis->iStrLen = 0;
	pThis->iAllocIncrement = STRINGBUF_ALLOC_INCREMENT;

	return pThis;
}


void rsCStrDestruct(rsCStrObj *pThis)
{
#	if STRINGBUF_TRIM_ALLOCSIZE == 1
	/* in this mode, a new buffer already was allocated,
	 * so we need to free the old one.
	 */
		if(pThis->pBuf != NULL) {
			free(pThis->pBuf);
		}
#	endif

	if(pThis->pszBuf != NULL) {
		free(pThis->pszBuf);
	}

	SRFREEOBJ(pThis);
}


srRetVal rsCStrAppendStr(rsCStrObj *pThis, char* psz)
{
	srRetVal iRet;

	sbSTRBCHECKVALIDOBJECT(pThis);
	assert(psz != NULL);

	while(*psz)
		if((iRet = rsCStrAppendChar(pThis, *psz++)) != SR_RET_OK)
			return iRet;

	return SR_RET_OK;
}


srRetVal rsCStrAppendInt(rsCStrObj *pThis, int i)
{
	srRetVal iRet;
	char szBuf[32];

	sbSTRBCHECKVALIDOBJECT(pThis);

	if((iRet = srUtilItoA(szBuf, sizeof(szBuf), i)) != SR_RET_OK)
		return iRet;

	return rsCStrAppendStr(pThis, szBuf);
}


srRetVal rsCStrAppendChar(rsCStrObj *pThis, char c)
{
	char* pNewBuf;

	sbSTRBCHECKVALIDOBJECT(pThis);

	if(pThis->iBufPtr >= pThis->iBufSize)
	{  /* need more memory! */
		if((pNewBuf = (char*) malloc((pThis->iBufSize + pThis->iAllocIncrement) * sizeof(char))) == NULL)
			return SR_RET_OUT_OF_MEMORY;
		memcpy(pNewBuf, pThis->pBuf, pThis->iBufSize);
		pThis->iBufSize += pThis->iAllocIncrement;
		if(pThis->pBuf != NULL) {
			free(pThis->pBuf);
		}
		pThis->pBuf = pNewBuf;
	}

	/* ok, when we reach this, we have sufficient memory */
	*(pThis->pBuf + pThis->iBufPtr++) = c;
	pThis->iStrLen++;

	return SR_RET_OK;
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
 *
 * rgerhards, 2005-09-07
 */
char*  rsCStrConvSzStrAndDestruct(rsCStrObj *pThis)
{
	char* pRetBuf;
	int i;

	sbSTRBCHECKVALIDOBJECT(pThis);

	if(pThis->pszBuf == NULL) {
		/* we do not yet have a usable sz version - so create it... */
		if((pThis->pszBuf = malloc(pThis->iStrLen + 1 * sizeof(char))) == NULL) {
			/* TODO: think about what to do - so far, I have no bright
			 *       idea... rgerhards 2005-09-07
			 */
		}
		else {
			/* we can create the sz String */
			if(pThis->pBuf != NULL)
				memcpy(pThis->pszBuf, pThis->pBuf, pThis->iStrLen);
			*(pThis->pszBuf + pThis->iStrLen) = '\0';
		}
	}
	/* we now need to do a sanity check. The string mgiht contain a
	 * \0 byte. There is no way how a sz string can handle this. For
	 * the time being, we simply replace it with space - something that
	 * could definitely be improved (TODO).
	 * 2005-09-09 rgerhards
	 */
	for(i = 0 ; i < pThis->iStrLen ; ++i) {
		if(*(pThis->pszBuf + i) == '\0')
			*(pThis->pszBuf + i) = ' ';
	}

	/* We got it, now free the object ourselfs. Please note
	 * that we can NOT use the rsCStrDestruct function as it would
	 * also free the sz String buffer, which we pass on to the user.
	 */
	pRetBuf = pThis->pszBuf;
	if(pThis->pBuf != NULL)
		free(pThis->pBuf);
	SRFREEOBJ(pThis);
	
	return(pRetBuf);
}


void  rsCStrFinish(rsCStrObj *pThis)
{
	sbSTRBCHECKVALIDOBJECT(pThis);

#	if STRINGBUF_TRIM_ALLOCSIZE == 1
	/* in this mode, we need to trim the string. To do
	 * so, we must allocate a new buffer of the exact 
	 * string size, and then copy the old one over. 
	 * This new buffer is then to be returned.
	 */
	if((pRetBuf = malloc((pThis->iBufSize) * sizeof(char))) == NULL)
	{	/* OK, in this case we use the previous buffer. At least
		 * we have it ;)
		 */
	}
	else
	{	/* got the new buffer, so let's use it */
		char* pBuf;
		memcpy(pBuf, pThis->pBuf, pThis->iBufPtr + 1);
		pThis->pBuf = pBuf;
	}
#	else
	/* here, we need to do ... nothing ;)
	 */
#	endif
}

void rsCStrSetAllocIncrement(rsCStrObj *pThis, int iNewIncrement)
{
	sbSTRBCHECKVALIDOBJECT(pThis);
	assert(iNewIncrement > 0);

	pThis->iAllocIncrement = iNewIncrement;
}

/* return the length of the current string
 * 2005-09-09 rgerhards
 */
int rsCStrLen(rsCStrObj *pThis)
{
	sbSTRBCHECKVALIDOBJECT(pThis);
	return(pThis->iStrLen);
}

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 * End:
 * vi:set ai:
 */
