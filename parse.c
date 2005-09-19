/* parsing routines for the counted string class. for generic
 * informaton see parse.h.
 *
 * begun 2005-09-15 rgerhards
 *
 * Copyright 2005
 *     Rainer Gerhards and Adiscon GmbH. All Rights Reserved.
 *     This code is placed under the GPL.
 */
#include <stdio.h>
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

/* Skip everything up to a specified character.
 * Returns with ParsePointer set BEHIND this character.
 * Returns RS_RET_OK if found, RS_RET_NOT_FOUND if not
 * found. In that case, the ParsePointer is moved to the
 * last character of the string.
 * 2005-09-19 rgerhards
 */
rsRetVal parsSkipAfterChar(rsParsObj *pThis, char c)
{
	register char *pC;
	rsRetVal iRet;

	rsCHECKVALIDOBJECT(pThis, OIDrsPars);

	pC = rsCStrGetBufBeg(pThis->pCStr);

	while(pThis->iCurrPos < rsCStrLen(pThis->pCStr)) {
		if(pC[pThis->iCurrPos] == c)
			break;
		++pThis->iCurrPos;
	}

	/* delimiter found? */
	if(pC[pThis->iCurrPos] == c) {
		if(pThis->iCurrPos+1 < rsCStrLen(pThis->pCStr)) {
			iRet = RS_RET_OK;
			pThis->iCurrPos++; /* 'eat' delimiter */
		} else {
			iRet = RS_RET_FOUND_AT_STRING_END;
		}
	} else {
		iRet = RS_RET_NOT_FOUND;
	}

	return iRet;
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

	while(pThis->iCurrPos < rsCStrLen(pThis->pCStr)) {
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

	if(bTrimLeading)
		parsSkipWhitespace(pThis);

	pC = rsCStrGetBufBeg(pThis->pCStr) + pThis->iCurrPos;

	while(pThis->iCurrPos < rsCStrLen(pThis->pCStr)
	      && *pC != cDelim) {
		if((iRet = rsCStrAppendChar(pCStr, *pC)) != RS_RET_OK) {
			RSFREEOBJ(pCStr);
			return(iRet);
		}
		++pThis->iCurrPos;
		++pC;
	}
	
	if(*pC == cDelim) {
		++pThis->iCurrPos; /* eat delimiter */
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

/* Parse a quoted string ("-some-data") from the given position.
 * Leading whitespace before the first quote is skipped. During
 * parsing, escape sequences are detected and converted:
 * \\ - backslash character
 * \" - quote character
 * any other value \<somechar> is reserved for future use.
 *
 * After return, the parse pointer is paced after the trailing
 * quote.
 *
 * Output:
 * ppCStr Pointer to the parsed string - must be freed by caller and
 *        does NOT include the quotes.
 * rgerhards, 2005-09-19
 */
rsRetVal parsQuotedCStr(rsParsObj *pThis, rsCStrObj **ppCStr)
{
	register char *pC;
	rsCStrObj *pCStr;
	rsRetVal iRet;

	rsCHECKVALIDOBJECT(pThis, OIDrsPars);

	if((iRet = parsSkipAfterChar(pThis, '"')) != RS_RET_OK)
		return iRet;
	pC = rsCStrGetBufBeg(pThis->pCStr) + pThis->iCurrPos;

	/* OK, we most probably can obtain a value... */
	if((pCStr = rsCStrConstruct()) == NULL)
		return RS_RET_OUT_OF_MEMORY;

	while(pThis->iCurrPos < rsCStrLen(pThis->pCStr)) {
		if(*pC == '"') {
			break;	/* we are done! */
		} else if(*pC == '\\') {
			++pThis->iCurrPos;
			++pC;
			if(pThis->iCurrPos < rsCStrLen(pThis->pCStr)) {
				/* in this case, we copy the escaped character
				 * to the output buffer (but do not rely on this,
				 * we might later introduce other things, like \007!
				 */
				if((iRet = rsCStrAppendChar(pCStr, *pC)) != RS_RET_OK) {
					RSFREEOBJ(pCStr);
					return(iRet);
				}
			}
		} else { /* regular character */
			if((iRet = rsCStrAppendChar(pCStr, *pC)) != RS_RET_OK) {
				RSFREEOBJ(pCStr);
				return(iRet);
			}
		}
		++pThis->iCurrPos;
		++pC;
	}
	
	if(*pC == '"') {
		++pThis->iCurrPos; /* 'eat' trailing quote */
	} else {
		/* error - improperly quoted string! */
		RSFREEOBJ(pCStr);
		return RS_RET_MISSING_TRAIL_QUOTE;
	}

	/* We got the string, let's finish it...  */
	if((iRet = rsCStrFinish(pCStr)) != RS_RET_OK) {
		RSFREEOBJ(pCStr);
		return(iRet);
	}

	/* done! */
	*ppCStr = pCStr;
	return RS_RET_OK;
}

/* return the position of the parse pointer
 */
int rsParsGetParsePointer(rsParsObj *pThis)
{
	rsCHECKVALIDOBJECT(pThis, OIDrsPars);

	if(pThis->iCurrPos < rsCStrLen(pThis->pCStr))
		return pThis->iCurrPos;
	else
		return rsCStrLen(pThis->pCStr) - 1;
}


/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 * End:
 * vi:set ai:
 */
