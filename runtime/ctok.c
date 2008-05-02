/* cfgtok.c - helper class to tokenize an input stream - which surprisingly
 * currently does not work with streams but with string. But that will
 * probably change over time ;) This class was originally written to support
 * the expression module but may evolve when (if) the expression module is
 * expanded (or aggregated) by a full-fledged ctoken based config parser.
 * Obviously, this class is used together with config files and not any other
 * parse function.
 *
 * Module begun 2008-02-19 by Rainer Gerhards
 *
 * Copyright (C) 2008 by Rainer Gerhards and Adiscon GmbH.
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

#include "config.h"
#include <stdlib.h>
#include <ctype.h>
#include <strings.h>
#include <assert.h>

#include "rsyslog.h"
#include "template.h"
#include "ctok.h"

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(ctok_token)
DEFobjCurrIf(var)


/* Standard-Constructor
 */
BEGINobjConstruct(ctok) /* be sure to specify the object type also in END macro! */
ENDobjConstruct(ctok)


/* ConstructionFinalizer
 * rgerhards, 2008-01-09
 */
rsRetVal ctokConstructFinalize(ctok_t __attribute__((unused)) *pThis)
{
	DEFiRet;
	RETiRet;
}


/* destructor for the ctok object */
BEGINobjDestruct(ctok) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(ctok)
	/* ... then free resources */
ENDobjDestruct(ctok)


/* unget character from input stream. At most one character can be ungotten.
 * This funtion is only permitted to be called after at least one character
 * has been read from the stream. Right now, we handle the situation simply by
 * moving the string "stream" pointer one position backwards. If we work with
 * real streams (some time), the strm object will handle the functionality
 * itself. -- rgerhards, 2008-02-19
 */
static rsRetVal
ctokUngetCharFromStream(ctok_t *pThis, uchar __attribute__((unused)) c)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, ctok);
	--pThis->pp;

	RETiRet;
}


/* get the next character from the input "stream" (currently just a in-memory
 * string...) -- rgerhards, 2008-02-19
 */
static rsRetVal 
ctokGetCharFromStream(ctok_t *pThis, uchar *pc)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, ctok);
	ASSERT(pc != NULL);

	/* end of string or begin of comment terminates the "stream" */
	if(*pThis->pp == '\0' || *pThis->pp == '#') {
		ABORT_FINALIZE(RS_RET_EOS);
	} else {
		*pc = *pThis->pp;
		++pThis->pp;
	}

finalize_it:
	RETiRet;
}


/* skip whitespace in the input "stream".
 * rgerhards, 2008-02-19
 */
static rsRetVal 
ctokSkipWhitespaceFromStream(ctok_t *pThis)
{
	DEFiRet;
	uchar c;

	ISOBJ_TYPE_assert(pThis, ctok);

	CHKiRet(ctokGetCharFromStream(pThis, &c));
	while(isspace(c)) {
		CHKiRet(ctokGetCharFromStream(pThis, &c));
	}

	/* we must unget the one non-whitespace we found */
	CHKiRet(ctokUngetCharFromStream(pThis, c));

dbgprintf("skipped whitepsace, stream now '%s'\n", pThis->pp);
finalize_it:
	RETiRet;
}


/* get the next word from the input "stream" (currently just a in-memory
 * string...). A word is anything from the current location until the
 * first non-alphanumeric character. If the word is longer
 * than the provided memory buffer, parsing terminates when buffer length
 * has been reached. A buffer of 128 bytes or more should always be by
 * far sufficient. -- rgerhards, 2008-02-19
 */
static rsRetVal 
ctokGetWordFromStream(ctok_t *pThis, uchar *pWordBuf, size_t lenWordBuf)
{
	DEFiRet;
	uchar c;

	ISOBJ_TYPE_assert(pThis, ctok);
	ASSERT(pWordBuf != NULL);
	ASSERT(lenWordBuf > 0);

	CHKiRet(ctokSkipWhitespaceFromStream(pThis));

	CHKiRet(ctokGetCharFromStream(pThis, &c));
	while((isalnum(c) || c == '_' || c == '-') && lenWordBuf > 1) {
		*pWordBuf++ = c;
		--lenWordBuf;
		CHKiRet(ctokGetCharFromStream(pThis, &c));
	}
	*pWordBuf = '\0'; /* there is always space for this - see while() */

	/* push back the char that we have read too much */
	CHKiRet(ctokUngetCharFromStream(pThis, c));

finalize_it:
	RETiRet;
}


/* read in a constant number
 * This is the "number" ABNF element
 * rgerhards, 2008-02-19
 */
static rsRetVal
ctokGetNumber(ctok_t *pThis, ctok_token_t *pToken)
{
	DEFiRet;
	number_t n; /* the parsed number */
	uchar c;
	int valC;
	int iBase;

	ISOBJ_TYPE_assert(pThis, ctok);
	ASSERT(pToken != NULL);

	pToken->tok = ctok_NUMBER;

	CHKiRet(ctokGetCharFromStream(pThis, &c));
	if(c == '0') { /* octal? */
		CHKiRet(ctokGetCharFromStream(pThis, &c));
		if(c == 'x') { /* nope, hex! */
			CHKiRet(ctokGetCharFromStream(pThis, &c));
			c = tolower(c);
			iBase = 16;
		} else {
			iBase = 8;
		}
	} else {
		iBase = 10;
	}
		
	n = 0;
	/* this loop is quite simple, a variable name is terminated by whitespace. */
	while(isdigit(c) || (c >= 'a' && c <= 'f')) {
		if(isdigit(c)) {
			valC = c - '0';
		} else {
			valC = c - 'a' + 10;
		}
		
		if(valC >= iBase) {
			if(iBase == 8) {
				ABORT_FINALIZE(RS_RET_INVALID_OCTAL_DIGIT);
			} else {
				ABORT_FINALIZE(RS_RET_INVALID_HEX_DIGIT);
			}
		}
		/* we now have the next value and know it is right */
		n = n * iBase + valC;
		CHKiRet(ctokGetCharFromStream(pThis, &c));
		c = tolower(c);
	}

	/* we need to unget the character that made the loop terminate */
	CHKiRet(ctokUngetCharFromStream(pThis, c));

	CHKiRet(var.SetNumber(pToken->pVar, n));

finalize_it:
	RETiRet;
}


/* read in a variable
 * This covers both msgvar and sysvar from the ABNF.
 * rgerhards, 2008-02-19
 */
static rsRetVal
ctokGetVar(ctok_t *pThis, ctok_token_t *pToken)
{
	DEFiRet;
	uchar c;
	cstr_t *pstrVal;

	ISOBJ_TYPE_assert(pThis, ctok);
	ASSERT(pToken != NULL);

	CHKiRet(ctokGetCharFromStream(pThis, &c));

	if(c == '$') { /* second dollar, we have a system variable */
		pToken->tok = ctok_SYSVAR;
		CHKiRet(ctokGetCharFromStream(pThis, &c)); /* "eat" it... */
	} else {
		pToken->tok = ctok_MSGVAR;
	}

	CHKiRet(rsCStrConstruct(&pstrVal));
	/* this loop is quite simple, a variable name is terminated by whitespace. */
	while(!isspace(c)) {
		CHKiRet(rsCStrAppendChar(pstrVal, tolower(c)));
		CHKiRet(ctokGetCharFromStream(pThis, &c));
	}
	CHKiRet(rsCStrFinish(pStrB));

	CHKiRet(var.SetString(pToken->pVar, pstrVal));
	pstrVal = NULL;

finalize_it:
	if(iRet != RS_RET_OK) {
		if(pstrVal != NULL) {
			rsCStrDestruct(&pstrVal);
		}
	}

	RETiRet;
}


/* read in a simple string (simpstr in ABNF)
 * rgerhards, 2008-02-19
 */
static rsRetVal
ctokGetSimpStr(ctok_t *pThis, ctok_token_t *pToken)
{
	DEFiRet;
	uchar c;
	int bInEsc = 0;
	cstr_t *pstrVal;

	ISOBJ_TYPE_assert(pThis, ctok);
	ASSERT(pToken != NULL);

	pToken->tok = ctok_SIMPSTR;

	CHKiRet(rsCStrConstruct(&pstrVal));
	CHKiRet(ctokGetCharFromStream(pThis, &c));
	/* while we are in escape mode (had a backslash), no sequence
	 * terminates the loop. If outside, it is terminated by a single quote.
	 */
	while(bInEsc || c != '\'') {
		if(bInEsc) {
			CHKiRet(rsCStrAppendChar(pstrVal, c));
			bInEsc = 0;
		} else {
			if(c == '\\') {
				bInEsc = 1;
			} else {
				CHKiRet(rsCStrAppendChar(pstrVal, c));
			}
		}
		CHKiRet(ctokGetCharFromStream(pThis, &c));
	}
	CHKiRet(rsCStrFinish(pStrB));

	CHKiRet(var.SetString(pToken->pVar, pstrVal));
	pstrVal = NULL;

finalize_it:
	if(iRet != RS_RET_OK) {
		if(pstrVal != NULL) {
			rsCStrDestruct(&pstrVal);
		}
	}

	RETiRet;
}


/* Unget a token. The token ungotten will be returned the next time
 * ctokGetToken() is called. Only one token can be ungotten at a time.
 * If a second token is ungotten, the first is lost. This is considered
 * a programming error.
 * rgerhards, 2008-02-20
 */
static rsRetVal
ctokUngetToken(ctok_t *pThis, ctok_token_t *pToken)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, ctok);
	ASSERT(pToken != NULL);
	ASSERT(pThis->pUngotToken == NULL);

	pThis->pUngotToken = pToken;

	RETiRet;
}


/* skip an inine comment (just like a C-comment) 
 * rgerhards, 2008-02-20
 */
static rsRetVal
ctokSkipInlineComment(ctok_t *pThis)
{
	DEFiRet;
	uchar c;
	int bHadAsterisk = 0;

	ISOBJ_TYPE_assert(pThis, ctok);

	CHKiRet(ctokGetCharFromStream(pThis, &c)); /* read a charater */
	while(!(bHadAsterisk && c == '/')) {
		bHadAsterisk = (c == '*') ? 1 : 0;
		CHKiRet(ctokGetCharFromStream(pThis, &c)); /* read next */
	}

finalize_it:
	RETiRet;
}



/* Get the *next* token from the input stream. This parses the next token and
 * ignores any whitespace in between. End of stream is communicated via iRet.
 * The returned token must either be destructed by the caller OR being passed
 * back to ctokUngetToken().
 * rgerhards, 2008-02-19
 */
static rsRetVal
ctokGetToken(ctok_t *pThis, ctok_token_t **ppToken)
{
	DEFiRet;
	ctok_token_t *pToken;
	uchar c;
	uchar szWord[128];
	int bRetry = 0; /* retry parse? Only needed for inline comments... */

	ISOBJ_TYPE_assert(pThis, ctok);
	ASSERT(ppToken != NULL);

	/* first check if we have an ungotten token and, if so, provide that
	 * one back (without any parsing). -- rgerhards, 2008-02-20
	 */
	if(pThis->pUngotToken != NULL) {
		*ppToken = pThis->pUngotToken;
		pThis->pUngotToken = NULL;
		FINALIZE;
	}

	/* setup the stage - create our token */
	CHKiRet(ctok_token.Construct(&pToken));
	CHKiRet(ctok_token.ConstructFinalize(pToken));

	/* find the next token. We may loop when we have inline comments */
	do {
		bRetry = 0;
		CHKiRet(ctokSkipWhitespaceFromStream(pThis));
		CHKiRet(ctokGetCharFromStream(pThis, &c)); /* read a charater */
		switch(c) {
			case '=': /* == */
				CHKiRet(ctokGetCharFromStream(pThis, &c)); /* read a charater */
				pToken->tok = (c == '=')? ctok_CMP_EQ : ctok_INVALID;
				break;
			case '!': /* != */
				CHKiRet(ctokGetCharFromStream(pThis, &c)); /* read a charater */
				pToken->tok = (c == '=')? ctok_CMP_NEQ : ctok_INVALID;
				break;
			case '<': /* <, <=, <> */
				CHKiRet(ctokGetCharFromStream(pThis, &c)); /* read a charater */
				if(c == '=') {
					pToken->tok = ctok_CMP_LTEQ;
				} else if(c == '>') {
					pToken->tok = ctok_CMP_NEQ;
				} else {
					pToken->tok = ctok_CMP_LT;
				}
				break;
			case '>': /* >, >= */
				CHKiRet(ctokGetCharFromStream(pThis, &c)); /* read a charater */
				if(c == '=') {
					pToken->tok = ctok_CMP_GTEQ;
				} else {
					pToken->tok = ctok_CMP_GT;
				}
				break;
			case '+':
				pToken->tok = ctok_PLUS;
				break;
			case '-':
				pToken->tok = ctok_MINUS;
				break;
			case '*':
				pToken->tok = ctok_TIMES;
				break;
			case '/': /* /, /.* ... *./ (comments, mungled here for obvious reasons...) */
				CHKiRet(ctokGetCharFromStream(pThis, &c)); /* read a charater */
				if(c == '*') {
					/* we have a comment and need to skip it */
					ctokSkipInlineComment(pThis);
					bRetry = 1;
				} else {
					CHKiRet(ctokUngetCharFromStream(pThis, c)); /* put back, not processed */
				}
				pToken->tok = ctok_DIV;
				break;
			case '%':
				pToken->tok = ctok_MOD;
				break;
			case '(':
				pToken->tok = ctok_LPAREN;
				break;
			case ')':
				pToken->tok = ctok_RPAREN;
				break;
			case ',':
				pToken->tok = ctok_COMMA;
				break;
			case '&':
				pToken->tok = ctok_STRADD;
				break;
			case '$':
				CHKiRet(ctokGetVar(pThis, pToken));
				break;
			case '\'': /* simple string, this is somewhat more elaborate */
				CHKiRet(ctokGetSimpStr(pThis, pToken));
				break;
			case '"':
				/* TODO: template string parser */
				ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
				break;
			default:
				CHKiRet(ctokUngetCharFromStream(pThis, c)); /* push back, we need it in any case */
				if(isdigit(c)) {
					CHKiRet(ctokGetNumber(pThis, pToken));
				} else { /* now we check if we have a multi-char sequence */
					CHKiRet(ctokGetWordFromStream(pThis, szWord, sizeof(szWord)/sizeof(uchar)));
					if(!strcasecmp((char*)szWord, "and")) {
						pToken->tok = ctok_AND;
					} else if(!strcasecmp((char*)szWord, "or")) {
						pToken->tok = ctok_OR;
					} else if(!strcasecmp((char*)szWord, "not")) {
						pToken->tok = ctok_NOT;
					} else if(!strcasecmp((char*)szWord, "contains")) {
						pToken->tok = ctok_CMP_CONTAINS;
					} else if(!strcasecmp((char*)szWord, "contains_i")) {
						pToken->tok = ctok_CMP_CONTAINSI;
					} else if(!strcasecmp((char*)szWord, "startswith")) {
						pToken->tok = ctok_CMP_STARTSWITH;
					} else if(!strcasecmp((char*)szWord, "startswith_i")) {
						pToken->tok = ctok_CMP_STARTSWITHI;
					} else if(!strcasecmp((char*)szWord, "then")) {
						pToken->tok = ctok_THEN;
					} else {
						/* finally, we check if it is a function */
						CHKiRet(ctokGetCharFromStream(pThis, &c)); /* read a charater */
						if(c == '(') {
							/* push c back, higher level parser needs it */
							CHKiRet(ctokUngetCharFromStream(pThis, c));
							pToken->tok = ctok_FUNCTION;
							// TODO: fill function name
						} else { /* give up... */
							pToken->tok = ctok_INVALID;
						}
					}
				}
				break;
		}
	} while(bRetry); /* warning: do ... while()! */

	*ppToken = pToken;
	dbgoprint((obj_t*) pToken, "token: %d\n", pToken->tok);

finalize_it:
	if(iRet != RS_RET_OK) {
		if(pToken != NULL)
			ctok_token.Destruct(&pToken);
	}

	RETiRet;
}


/* property set methods */
/* simple ones first */
DEFpropSetMeth(ctok, pp, uchar*)

/* return the current position of pp - most important as currently we do only
 * partial parsing, so the rest must know where to start from...
 * rgerhards, 2008-02-19
 */
static rsRetVal
ctokGetpp(ctok_t *pThis, uchar **pp)
{
	DEFiRet;
	ASSERT(pp != NULL);
	*pp = pThis->pp;
	RETiRet;
}


/* queryInterface function
 * rgerhards, 2008-02-21
 */
BEGINobjQueryInterface(ctok)
CODESTARTobjQueryInterface(ctok)
	if(pIf->ifVersion != ctokCURR_IF_VERSION) { /* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	//xxxpIf->oID = OBJctok;

	pIf->Construct = ctokConstruct;
	pIf->ConstructFinalize = ctokConstructFinalize;
	pIf->Destruct = ctokDestruct;
	pIf->Getpp = ctokGetpp;
	pIf->GetToken = ctokGetToken;
	pIf->UngetToken = ctokUngetToken;
	pIf->Setpp = ctokSetpp;
finalize_it:
ENDobjQueryInterface(ctok)



BEGINObjClassInit(ctok, 1, OBJ_IS_CORE_MODULE) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(ctok_token, CORE_COMPONENT));
	CHKiRet(objUse(var, CORE_COMPONENT));

	OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, ctokConstructFinalize);
ENDObjClassInit(ctok)

/* vi:set ai:
 */
