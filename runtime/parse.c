/* parsing routines for the counted string class. for generic
 * informaton see parse.h.
 *
 * begun 2005-09-15 rgerhards
 *
 * Copyright 2005-2017 Adiscon GmbH.
 *
 * This file is part of rsyslog.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include "rsyslog.h"
#include "parse.h"
#include "debug.h"

/* ################################################################# *
 * private members                                                   *
 * ################################################################# */


/* ################################################################# *
 * public members                                                    *
 * ################################################################# */

/**
 * Destruct a rsPars object and its associated string.
 * rgerhards, 2005-09-26
 */
rsRetVal rsParsDestruct(rsParsObj *pThis) {
    rsCHECKVALIDOBJECT(pThis, OIDrsPars);

    if (pThis->pCStr != NULL) rsCStrDestruct(&pThis->pCStr);
    RSFREEOBJ(pThis);
    return RS_RET_OK;
}


/**
 * Construct a rsPars object.
 */
rsRetVal rsParsConstruct(rsParsObj **ppThis) {
    rsParsObj *pThis;

    assert(ppThis != NULL);

    if ((pThis = (rsParsObj *)calloc(1, sizeof(rsParsObj))) == NULL) return RS_RET_OUT_OF_MEMORY;

    rsSETOBJTYPE(pThis, OIDrsPars);

    *ppThis = pThis;
    return RS_RET_OK;
}

/**
 * Construct a rsPars object and populate it with a
 * classical zero-terinated C-String.
 * rgerhards, 2005-09-27
 */
rsRetVal rsParsConstructFromSz(rsParsObj **ppThis, unsigned char *psz) {
    DEFiRet;
    rsParsObj *pThis;
    cstr_t *pCS;

    assert(ppThis != NULL);
    assert(psz != NULL);

    /* create string for parser */
    CHKiRet(rsCStrConstructFromszStr(&pCS, psz));

    /* create parser */
    if ((iRet = rsParsConstruct(&pThis)) != RS_RET_OK) {
        rsCStrDestruct(&pCS);
        FINALIZE;
    }

    /* assign string to parser */
    if ((iRet = rsParsAssignString(pThis, pCS)) != RS_RET_OK) {
        rsParsDestruct(pThis);
        FINALIZE;
    }
    *ppThis = pThis;

finalize_it:
    RETiRet;
}

/**
 * Assign the to-be-parsed string.
 */
rsRetVal rsParsAssignString(rsParsObj *pThis, cstr_t *pCStr) {
    rsCHECKVALIDOBJECT(pThis, OIDrsPars);
    rsCHECKVALIDOBJECT(pCStr, OIDrsCStr);

    pThis->pCStr = pCStr;
    pThis->iCurrPos = 0;

    return RS_RET_OK;
}

/* parse an integer. The parse pointer is advanced to the
 * position directly after the last digit. If no digit is
 * found at all, an error is returned and the parse pointer
 * is NOT advanced.
 * PORTABILITY WARNING: this function depends on the
 * continues representation of digits inside the character
 * set (as in ASCII).
 * rgerhards 2005-09-27
 */
rsRetVal parsInt(rsParsObj *pThis, int *pInt) {
    unsigned char *pC;
    int iVal;

    rsCHECKVALIDOBJECT(pThis, OIDrsPars);
    assert(pInt != NULL);

    iVal = 0;
    pC = rsCStrGetBufBeg(pThis->pCStr) + pThis->iCurrPos;

    /* order of checks is important, else we might do
     * mis-addressing! (off by one)
     */
    if (pThis->iCurrPos >= rsCStrLen(pThis->pCStr)) return RS_RET_NO_MORE_DATA;
    if (!isdigit((int)*pC)) return RS_RET_NO_DIGIT;

    while (pThis->iCurrPos < rsCStrLen(pThis->pCStr) && isdigit((int)*pC)) {
        iVal = iVal * 10 + *pC - '0';
        ++pThis->iCurrPos;
        ++pC;
    }

    *pInt = iVal;

    return RS_RET_OK;
}

/* Skip everything up to a specified character.
 * Returns with ParsePointer set BEHIND this character.
 * Returns RS_RET_OK if found, RS_RET_NOT_FOUND if not
 * found. In that case, the ParsePointer is moved to the
 * last character of the string.
 * 2005-09-19 rgerhards
 */
rsRetVal parsSkipAfterChar(rsParsObj *pThis, char c) {
    register unsigned char *pC;
    DEFiRet;

    rsCHECKVALIDOBJECT(pThis, OIDrsPars);

    pC = rsCStrGetBufBeg(pThis->pCStr);

    while (pThis->iCurrPos < rsCStrLen(pThis->pCStr)) {
        if (pC[pThis->iCurrPos] == c) break;
        ++pThis->iCurrPos;
    }

    /* delimiter found? */
    if (pC[pThis->iCurrPos] == c) {
        if (pThis->iCurrPos + 1 < rsCStrLen(pThis->pCStr)) {
            iRet = RS_RET_OK;
            pThis->iCurrPos++; /* 'eat' delimiter */
        } else {
            iRet = RS_RET_FOUND_AT_STRING_END;
        }
    } else {
        iRet = RS_RET_NOT_FOUND;
    }

    RETiRet;
}

/* Skip whitespace. Often used to trim parsable entries.
 * Returns with ParsePointer set to first non-whitespace
 * character (or at end of string).
 */
rsRetVal parsSkipWhitespace(rsParsObj *pThis) {
    register unsigned char *pC;
    DEFiRet;


    rsCHECKVALIDOBJECT(pThis, OIDrsPars);

    pC = rsCStrGetBufBeg(pThis->pCStr);

    while (pThis->iCurrPos < rsCStrLen(pThis->pCStr)) {
        if (!isspace((int)*(pC + pThis->iCurrPos))) break;
        ++pThis->iCurrPos;
    }

    RETiRet;
}

/* Parse string up to a delimiter.
 *
 * Input:
 * cDelim - the delimiter. Note that SP within a value always is a delimiter,
 * so cDelim is actually an *additional* delimiter.
 *   The following two are for whitespace stripping,
 *   0 means "no", 1 "yes"
 *   - bTrimLeading
 *   - bTrimTrailing
 *   - bConvLower - convert string to lower case?
 *
 * Output:
 * ppCStr Pointer to the parsed string - must be freed by caller!
 */
rsRetVal parsDelimCStr(
    rsParsObj *pThis, cstr_t **ppCStr, char cDelim, int bTrimLeading, int bTrimTrailing, int bConvLower) {
    DEFiRet;
    register unsigned char *pC;
    cstr_t *pCStr = NULL;

    rsCHECKVALIDOBJECT(pThis, OIDrsPars);

    CHKiRet(rsCStrConstruct(&pCStr));

    if (bTrimLeading) parsSkipWhitespace(pThis);

    pC = rsCStrGetBufBeg(pThis->pCStr) + pThis->iCurrPos;

    while (pThis->iCurrPos < rsCStrLen(pThis->pCStr) && *pC != cDelim) {
        CHKiRet(cstrAppendChar(pCStr, bConvLower ? tolower(*pC) : *pC));
        ++pThis->iCurrPos;
        ++pC;
    }

    if (pThis->iCurrPos < cstrLen(pThis->pCStr)) {  // BUGFIX!!
        ++pThis->iCurrPos; /* eat delimiter */
    }

    /* We got the string, now take it and see if we need to
     * remove anything at its end.
     */
    cstrFinalize(pCStr);

    if (bTrimTrailing) {
        cstrTrimTrailingWhiteSpace(pCStr);
    }

    /* done! */
    *ppCStr = pCStr;

finalize_it:
    if (iRet != RS_RET_OK) {
        if (pCStr != NULL) rsCStrDestruct(&pCStr);
    }

    RETiRet;
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
rsRetVal parsQuotedCStr(rsParsObj *pThis, cstr_t **ppCStr) {
    register unsigned char *pC;
    cstr_t *pCStr = NULL;
    DEFiRet;

    rsCHECKVALIDOBJECT(pThis, OIDrsPars);

    CHKiRet(parsSkipAfterChar(pThis, '"'));
    pC = rsCStrGetBufBeg(pThis->pCStr) + pThis->iCurrPos;

    /* OK, we most probably can obtain a value... */
    CHKiRet(cstrConstruct(&pCStr));

    while (pThis->iCurrPos < cstrLen(pThis->pCStr)) {
        if (*pC == '"') {
            break; /* we are done! */
        } else if (*pC == '\\') {
            ++pThis->iCurrPos;
            ++pC;
            if (pThis->iCurrPos < cstrLen(pThis->pCStr)) {
                /* in this case, we copy the escaped character
                 * to the output buffer (but do not rely on this,
                 * we might later introduce other things, like \007!
                 */
                CHKiRet(cstrAppendChar(pCStr, *pC));
            }
        } else { /* regular character */
            CHKiRet(cstrAppendChar(pCStr, *pC));
        }
        ++pThis->iCurrPos;
        ++pC;
    }

    if (*pC == '"') {
        ++pThis->iCurrPos; /* 'eat' trailing quote */
    } else {
        /* error - improperly quoted string! */
        cstrDestruct(&pCStr);
        ABORT_FINALIZE(RS_RET_MISSING_TRAIL_QUOTE);
    }

    cstrFinalize(pCStr);

    /* done! */
    *ppCStr = pCStr;

finalize_it:
    if (iRet != RS_RET_OK) {
        if (pCStr != NULL) cstrDestruct(&pCStr);
    }

    RETiRet;
}

/* tell if the parsepointer is at the end of the
 * to-be-parsed string. Returns 1, if so, 0
 * otherwise. rgerhards, 2005-09-27
 */
int parsIsAtEndOfParseString(rsParsObj *pThis) {
    rsCHECKVALIDOBJECT(pThis, OIDrsPars);

    return (pThis->iCurrPos < rsCStrLen(pThis->pCStr)) ? 0 : 1;
}


/* return the position of the parse pointer
 */
int rsParsGetParsePointer(rsParsObj *pThis) {
    rsCHECKVALIDOBJECT(pThis, OIDrsPars);

    if (pThis->iCurrPos < rsCStrLen(pThis->pCStr))
        return pThis->iCurrPos;
    else
        return rsCStrLen(pThis->pCStr) - 1;
}

/* peek at the character at the parse pointer
 * the caller must ensure that the parse pointer is not
 * at the end of the parse buffer (e.g. by first calling
 * parsIsAtEndOfParseString).
 * rgerhards, 2005-09-27
 */
char parsPeekAtCharAtParsPtr(rsParsObj *pThis) {
    rsCHECKVALIDOBJECT(pThis, OIDrsPars);
    assert(pThis->iCurrPos < rsCStrLen(pThis->pCStr));

    return (*(pThis->pCStr->pBuf + pThis->iCurrPos));
}

/* return the current position inside the parse object.
 * rgerhards, 2007-07-04
 */
int parsGetCurrentPosition(rsParsObj *pThis) {
    return pThis->iCurrPos;
}


/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 * End:
 * vi:set ai:
 */
