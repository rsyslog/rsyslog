/* parsing routines for the counted string class. for generic
 * informaton see parse.h.
 *
 * begun 2005-09-15 rgerhards
 *
 * Copyright 2005
 *     Rainer Gerhards and Adiscon GmbH. All Rights Reserved.
 *     This code is placed under the GPL.
 */
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include "rsyslog.h"
#include "parse.h"

/* ################################################################# *
 * private members                                                   *
 * ################################################################# */



/* ################################################################# *
 * public members                                                    *
 * ################################################################# */


/**
 * Construct a rsPars object.
 */
rsRetVal rsParsConstruct(rsParsObj **ppThis)
{
	rsParsObj *pThis;

	assert(ppThis != NULL);

	if((pThis = (rsParsObj*) calloc(1, sizeof(rsParsObj))) == NULL)
		return RS_RET_OUT_OF_MEMORY;

	rsSETOBJTYPE(pThis, OIDrsPars);

	*ppThis = pThis;
	return RS_RET_OK;
}

/**
 * Assign the to-be-parsed string.
 */
rsRetVal rsParsAssignString(rsParsObj *pThis, rsCStrObj *pCStr)
{
	rsCHECKVALIDOBJECT(pThis, OIDrsPars);
	rsCHECKVALIDOBJECT(pCStr, OIDrsCStr);
	
	pThis->pCStr = pCStr;
	pThis->iCurrPos = 0;

	return RS_RET_OK;
}

/* parse an integer. The parse pointer is advanced */
rsRetVal parsInt(rsParsObj *pThis, int* pInt)
{
	rsCHECKVALIDOBJECT(pThis, OIDrsPars);
	assert(pInt != NULL);

	return RS_RET_OK;
}

/* Skip whitespace. Often used to trim parsable entries.
 * Returns with ParsePointer set to first non-whitespace
 * character (or at end of string).
 */
rsRetVal parsSkipWhitespace(rsParsObj *pThis)
{
	register char *pC;

	rsCHECKVALIDOBJECT(pThis, OIDrsPars);

	pC = rsCStrGetBufBeg(pThis->pCStr);

	while(pThis->iCurrPos >= rsCStrLen(pThis->pCStr)) {
		if(!isspace(*(pC+pThis->iCurrPos)))
			break;
		++pThis->iCurrPos;
	}

	return RS_RET_OK;
}

/* Parse string up to a delimiter.
 *
 * Input:
 * cDelim - the delimiter
 *   The following two are for whitespace stripping,
 *   0 means "no", 1 "yes"
 *   - bTrimLeading
 *   - bTrimTrailing
 * 
 * Output:
 * ppCStr Pointer to the parsed string - must be freed by caller!
 */
rsRetVal parsDelimCStr(rsParsObj *pThis, rsCStrObj **ppCStr, char cDelim, int bTrimLeading, int bTrimTrailing)
{
	register char *pC;
	rsCStrObj *pCStr;
	rsRetVal iRet;

	rsCHECKVALIDOBJECT(pThis, OIDrsPars);

	if((pCStr = rsCStrConstruct()) == NULL)
		return RS_RET_OUT_OF_MEMORY;

	pC = rsCStrGetBufBeg(pThis->pCStr) + pThis->iCurrPos;

	if(bTrimLeading)
		parsSkipWhitespace(pThis);

	while(pThis->iCurrPos >= rsCStrLen(pThis->pCStr)
	      && *pC != cDelim) {
		if((iRet = rsCStrAppendChar(pCStr, *pC)) != RS_RET_OK) {
			RSFREEOBJ(pCStr);
			return(iRet);
		}
		++pThis->iCurrPos;
	}
	
	/* We got the string, now take it and see if we need to
	 * remove anything at its end.
	 */
	if((iRet = rsCStrFinish(pCStr)) != RS_RET_OK) {
		RSFREEOBJ(pCStr);
		return(iRet);
	}

	if(bTrimTrailing) {
		if((iRet = rsCStrTrimTrailingWhiteSpace(pCStr)) 
		   != RS_RET_OK) {
			RSFREEOBJ(pCStr);
			return iRet;
		}
	}

	/* done! */
	*ppCStr = pCStr;
	return RS_RET_OK;
}

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 * End:
 * vi:set ai:
 */
