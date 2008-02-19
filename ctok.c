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
 * Copyright 2008 Rainer Gerhards and Adiscon GmbH.
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
#include <ctype.h>
#include <strings.h>
#include <assert.h>

#include "rsyslog.h"
#include "template.h"
#include "ctok.h"

/* static data */
DEFobjStaticHelpers


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

	if(*pThis->pp == '\0') {
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
 * string...). A word is anything between whitespace. If the word is longer
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
	while(!isspace(c) && lenWordBuf > 1) {
		*pWordBuf = c;
		--lenWordBuf;
		CHKiRet(ctokGetCharFromStream(pThis, &c));
	}
	*pWordBuf = '\0'; /* there is always space for this - see while() */

dbgprintf("end ctokGetWorkFromStream, stream now '%s'\n", pThis->pp);
finalize_it:
	RETiRet;
}


#if 0
/* Get the next token from the input stream. This parses the next token and
 * ignores any whitespace in between. End of stream is communicated via iRet.
 * rgerhards, 2008-02-19
 */
rsRetVal
ctokGetNextToken(ctok_t *pThis, ctok_token_t *pToken)
{
	DEFiRet;
	uchar pszWord[128];

	ISOBJ_TYPE_assert(pThis, ctok);
	ASSERT(pToken != NULL);

	CHKiRet(ctokGetWordFromStream(pThis, pszWord, sizeof(pszWord)/sizeof(uchar)));

	/* now recognize words... */
	if(strcasecmp((char*)pszWord, "or")) {
		*pToken = ctok_OR;
	} else if(strcasecmp((char*)pszWord, "and")) {
		*pToken = ctok_AND;
	} else if(strcasecmp((char*)pszWord, "+")) {
		*pToken = ctok_PLUS;
	} else if(strcasecmp((char*)pszWord, "-")) {
		*pToken = ctok_MINUS;
	} else if(strcasecmp((char*)pszWord, "*")) {
		*pToken = ctok_TIMES;
	} else if(strcasecmp((char*)pszWord, "/")) {
		*pToken = ctok_DIV;
	} else if(strcasecmp((char*)pszWord, "%")) {
		*pToken = ctok_MOD;
	} else if(strcasecmp((char*)pszWord, "not")) {
		*pToken = ctok_NOT;
	} else if(strcasecmp((char*)pszWord, "(")) {
		*pToken = ctok_LPAREN;
	} else if(strcasecmp((char*)pszWord, ")")) {
		*pToken = ctok_RPAREN;
	} else if(strcasecmp((char*)pszWord, ",")) {
		*pToken = ctok_COMMA;
	} else if(strcasecmp((char*)pszWord, "$")) {
		*pToken = ctok_DOLLAR;
	} else if(strcasecmp((char*)pszWord, "'")) {
		*pToken = ctok_QUOTE;
	} else if(strcasecmp((char*)pszWord, "\"")) {
		*pToken = ctok_DBL_QUOTE;
	} else if(strcasecmp((char*)pszWord, "==")) {
		*pToken = ctok_CMP_EQ;
	} else if(strcasecmp((char*)pszWord, "!=")) {
		*pToken = ctok_CMP_NEQ;
	} else if(strcasecmp((char*)pszWord, "<>")) { /* an alias for the non-C folks... */
		*pToken = ctok_CMP_NEQ;
	} else if(strcasecmp((char*)pszWord, "<")) {
		*pToken = ctok_CMP_LT;
	} else if(strcasecmp((char*)pszWord, ">")) {
		*pToken = ctok_CMP_GT;
	} else if(strcasecmp((char*)pszWord, "<=")) {
		*pToken = ctok_CMP_LTEQ;
	} else if(strcasecmp((char*)pszWord, ">=")) {
		*pToken = ctok_CMP_GTEQ;
	}

RUNLOG_VAR("%d", *pToken);

finalize_it:
	RETiRet;
}
#endif


/* Get the next token from the input stream. This parses the next token and
 * ignores any whitespace in between. End of stream is communicated via iRet.
 * rgerhards, 2008-02-19
 */
rsRetVal
ctokGetNextToken(ctok_t *pThis, ctok_token_t *pToken)
{
	DEFiRet;
	uchar c;

	ISOBJ_TYPE_assert(pThis, ctok);
	ASSERT(pToken != NULL);

	CHKiRet(ctokSkipWhitespaceFromStream(pThis));

	CHKiRet(ctokGetCharFromStream(pThis, &c)); /* read a charater */
	switch(c) {
		case 'o':/* or */
			CHKiRet(ctokGetCharFromStream(pThis, &c)); /* read a charater */
			*pToken = (c == 'r')? ctok_OR : ctok_INVALID;
			break;
		case 'a': /* and */
			CHKiRet(ctokGetCharFromStream(pThis, &c)); /* read a charater */
			if(c == 'n') {
				CHKiRet(ctokGetCharFromStream(pThis, &c)); /* read a charater */
				*pToken = (c == 'd')? ctok_AND : ctok_INVALID;
			} else {
				*pToken = ctok_INVALID;
			}
			break;
		case 'n': /* not */
			CHKiRet(ctokGetCharFromStream(pThis, &c)); /* read a charater */
			if(c == 'o') {
				CHKiRet(ctokGetCharFromStream(pThis, &c)); /* read a charater */
				*pToken = (c == 't')? ctok_NOT : ctok_INVALID;
			} else {
				*pToken = ctok_INVALID;
			}
			break;
		case '=': /* == */
			CHKiRet(ctokGetCharFromStream(pThis, &c)); /* read a charater */
			*pToken = (c == '=')? ctok_CMP_EQ : ctok_INVALID;
			break;
		case '!': /* != */
			CHKiRet(ctokGetCharFromStream(pThis, &c)); /* read a charater */
			*pToken = (c == '=')? ctok_CMP_NEQ : ctok_INVALID;
			break;
		case '<': /* <, <=, <> */
			CHKiRet(ctokGetCharFromStream(pThis, &c)); /* read a charater */
			if(c == '=') {
				*pToken = ctok_CMP_LTEQ;
			} else if(c == '>') {
				*pToken = ctok_CMP_NEQ;
			} else {
				*pToken = ctok_CMP_LT;
			}
			break;
		case '>': /* >, >= */
			CHKiRet(ctokGetCharFromStream(pThis, &c)); /* read a charater */
			if(c == '=') {
				*pToken = ctok_CMP_GTEQ;
			} else {
				*pToken = ctok_CMP_GT;
			}
			break;
		case '+':
			*pToken = ctok_PLUS;
			break;
		case '-':
			*pToken = ctok_MINUS;
			break;
		case '*':
			*pToken = ctok_TIMES;
			break;
		case '/':
			*pToken = ctok_DIV;
			break;
		case '%':
			*pToken = ctok_MOD;
			break;
		case '(':
			*pToken = ctok_LPAREN;
			break;
		case ')':
			*pToken = ctok_RPAREN;
			break;
		case ',':
			*pToken = ctok_COMMA;
			break;
		case '$':
			*pToken = ctok_DOLLAR;
			break;
		case '\'':
			*pToken = ctok_QUOTE;
			break;
		case '"':
			*pToken = ctok_DBL_QUOTE;
			break;
		default:
			*pToken = ctok_INVALID;
			break;
	}

RUNLOG_VAR("%d", *pToken);

finalize_it:
	RETiRet;
}


/* property set methods */
/* simple ones first */
DEFpropSetMeth(ctok, pp, uchar*)

/* return the current position of pp - most important as currently we do only
 * partial parsing, so the rest must know where to start from...
 * rgerhards, 2008-02-19
 */
rsRetVal
ctokGetpp(ctok_t *pThis, uchar **pp)
{
	DEFiRet;
	ASSERT(pp != NULL);
	*pp = pThis->pp;
	RETiRet;
}

BEGINObjClassInit(ctok, 1) /* class, version */
	OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, ctokConstructFinalize);
ENDObjClassInit(ctok)

/* vi:set ai:
 */
