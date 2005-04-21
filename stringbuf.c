/*! \file stringbuf.c
 *  \brief Implemetation of the dynamic string buffer helper object.
 *
 * \author  Rainer Gerhards <rgerhards@adiscon.com>
 * \date    2003-08-08
 *          Initial version  begun.
 *
 * Copyright 2002-2003 
 *     Rainer Gerhards and Adiscon GmbH. All Rights Reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 * 
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 * 
 *     * Neither the name of Adiscon GmbH or Rainer Gerhards
 *       nor the names of its contributors may be used to
 *       endorse or promote products derived from this software without
 *       specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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


sbStrBObj *sbStrBConstruct(void)
{
	sbStrBObj *pThis;

	if((pThis = (sbStrBObj*) calloc(1, sizeof(sbStrBObj))) == NULL)
		return NULL;

	pThis->OID = OIDsbStrB;
	pThis->pBuf = NULL;
	pThis->iBufSize = 0;
	pThis->iBufPtr = 0;
	pThis->iAllocIncrement = STRINGBUF_ALLOC_INCREMENT;

	return pThis;
}


void sbStrBDestruct(sbStrBObj *pThis)
{
#	if STRINGBUF_TRIM_ALLOCSIZE == 1
	/* in this mode, a new buffer already was allocated,
	 * so we need to free the old one.
	 */
		if(pThis->pBuf != NULL) {
			free(pThis->pBuf);
		}
#	endif

	SRFREEOBJ(pThis);
}


srRetVal sbStrBAppendStr(sbStrBObj *pThis, char* psz)
{
	srRetVal iRet;

	sbSTRBCHECKVALIDOBJECT(pThis);
	assert(psz != NULL);

	while(*psz)
		if((iRet = sbStrBAppendChar(pThis, *psz++)) != SR_RET_OK)
			return iRet;

	return SR_RET_OK;
}


srRetVal sbStrBAppendInt(sbStrBObj *pThis, int i)
{
	srRetVal iRet;
	char szBuf[32];

	sbSTRBCHECKVALIDOBJECT(pThis);

	if((iRet = srUtilItoA(szBuf, sizeof(szBuf), i)) != SR_RET_OK)
		return iRet;

	return sbStrBAppendStr(pThis, szBuf);
}


srRetVal sbStrBAppendChar(sbStrBObj *pThis, char c)
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

	return SR_RET_OK;
}


char* sbStrBFinish(sbStrBObj *pThis)
{
	char* pRetBuf;

	sbSTRBCHECKVALIDOBJECT(pThis);

	sbStrBAppendChar(pThis, '\0');

#	if STRINGBUF_TRIM_ALLOCSIZE == 1
	/* in this mode, we need to trim the string. To do
	 * so, we must allocate a new buffer of the exact 
	 * string size, and then copy the old one over. 
	 * This new buffer is then to be returned.
	 */
	if((pRetBuf = malloc((pThis->iBufSize + 1) * sizeof(char))) == NULL)
	{	/* OK, in this case we use the previous buffer. At least
		 * we have it ;)
		 */
		pRetBuf = pThis->pBuf;
	}
	else
	{	/* got the new buffer, so let's use it */
		memcpy(pRetBuf, pThis->pBuf, pThis->iBufPtr + 1);
	}
#	else
	/* here, we can simply return a pointer to the
	 * currently existing buffer. We don't care about
	 * the extra memory, as we achieve a big performance
	 * gain.
	 */
	pRetBuf = pThis->pBuf;
#	endif
	
	sbStrBDestruct(pThis);

	return(pRetBuf);
}

void sbStrBSetAllocIncrement(sbStrBObj *pThis, int iNewIncrement)
{
	sbSTRBCHECKVALIDOBJECT(pThis);
	assert(iNewIncrement > 0);

	pThis->iAllocIncrement = iNewIncrement;
}
