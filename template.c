/* This is the template processing code of rsyslog.
 * Please see syslogd.c for license information.
 * begun 2004-11-17 rgerhards
 *
 * Copyright 2004, 2007 Rainer Gerhards and Adiscon
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

#include "rsyslog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "stringbuf.h"
#include "syslogd-types.h"
#include "template.h"
#include "msg.h"
#include "dirty.h"
#include "obj.h"
#include "errmsg.h"
#include "strgen.h"
#include "unicode-helper.h"

/* static data */
DEFobjCurrIf(obj)
DEFobjCurrIf(errmsg)
DEFobjCurrIf(strgen)

#ifdef FEATURE_REGEXP
DEFobjCurrIf(regexp)
static int bFirstRegexpErrmsg = 1; /**< did we already do a "can't load regexp" error message? */
#endif

static struct template *tplRoot = NULL;	/* the root of the template list */
static struct template *tplLast = NULL;	/* points to the last element of the template list */
static struct template *tplLastStatic = NULL; /* last static element of the template list */



/* helper to tplToString and strgen's, extends buffer */
#define ALLOC_INC 128
rsRetVal
ExtendBuf(uchar **pBuf, size_t *pLenBuf, size_t iMinSize)
{
	uchar *pNewBuf;
	size_t iNewSize;
	DEFiRet;

	iNewSize = (iMinSize / ALLOC_INC + 1) * ALLOC_INC;
	CHKmalloc(pNewBuf = (uchar*) realloc(*pBuf, iNewSize));
	*pBuf = pNewBuf;
	*pLenBuf = iNewSize;

finalize_it:
	RETiRet;
}


/* This functions converts a template into a string.
 *
 * The function takes a pointer to a template and a pointer to a msg object
 * as well as a pointer to an output buffer and its size. Note that the output
 * buffer pointer may be NULL, size 0, in which case a new one is allocated.
 * The outpub buffer is grown as required. It is the caller's duty to free the
 * buffer when it is done. Note that it is advisable to reuse memory, as this
 * offers big performance improvements.
 * rewritten 2009-06-19 rgerhards
 */
rsRetVal tplToString(struct template *pTpl, msg_t *pMsg, uchar **ppBuf, size_t *pLenBuf)
{
	DEFiRet;
	struct templateEntry *pTpe;
	size_t iBuf;
	unsigned short bMustBeFreed = 0;
	uchar *pVal;
	size_t iLenVal = 0;

	assert(pTpl != NULL);
	assert(pMsg != NULL);
	assert(ppBuf != NULL);
	assert(pLenBuf != NULL);

	if(pTpl->pStrgen != NULL) {
		CHKiRet(pTpl->pStrgen(pMsg, ppBuf, pLenBuf));
		FINALIZE;
	}

	/* loop through the template. We obtain one value
	 * and copy it over to our dynamic string buffer. Then, we
	 * free the obtained value (if requested). We continue this
	 * loop until we got hold of all values.
	 */
	pTpe = pTpl->pEntryRoot;
	iBuf = 0;
	while(pTpe != NULL) {
		if(pTpe->eEntryType == CONSTANT) {
			pVal = (uchar*) pTpe->data.constant.pConstant;
			iLenVal = pTpe->data.constant.iLenConstant;
			bMustBeFreed = 0;
		} else 	if(pTpe->eEntryType == FIELD) {
			pVal = (uchar*) MsgGetProp(pMsg, pTpe, pTpe->data.field.propid,
						   pTpe->data.field.propName,  &iLenVal, &bMustBeFreed);
			/* we now need to check if we should use SQL option. In this case,
			 * we must go over the generated string and escape '\'' characters.
			 * rgerhards, 2005-09-22: the option values below look somewhat misplaced,
			 * but they are handled in this way because of legacy (don't break any
			 * existing thing).
			 */
			if(pTpl->optFormatForSQL == 1)
				doSQLEscape(&pVal, &iLenVal, &bMustBeFreed, 1);
			else if(pTpl->optFormatForSQL == 2)
				doSQLEscape(&pVal, &iLenVal, &bMustBeFreed, 0);
		}
		/* got source, now copy over */
		if(iLenVal > 0) { /* may be zero depending on property */
			/* first, make sure buffer fits */
			if(iBuf + iLenVal >= *pLenBuf) /* we reserve one char for the final \0! */
				CHKiRet(ExtendBuf(ppBuf, pLenBuf, iBuf + iLenVal + 1));

			memcpy(*ppBuf + iBuf, pVal, iLenVal);
			iBuf += iLenVal;
		}

		if(bMustBeFreed)
			free(pVal);

		pTpe = pTpe->pNext;
	}

	if(iBuf == *pLenBuf) {
		/* in the weired case of an *empty* template, this can happen.
		 * it is debatable if we should really fix it here or simply
		 * forbid that case. However, performance toll is minimal, so 
		 * I tend to permit it. -- 201011-05 rgerhards
		 */
		CHKiRet(ExtendBuf(ppBuf, pLenBuf, iBuf + 1));
	}
	(*ppBuf)[iBuf] = '\0';
	
finalize_it:
	RETiRet;
}


/* This functions converts a template into an array of strings.
 * For further general details, see the very similar funtion
 * tpltoString().
 * Instead of a string, an array of string pointers is returned by
 * thus function. The caller is repsonsible for destroying that array as
 * well as all of its elements. The array is of fixed size. It's end
 * is indicated by a NULL pointer.
 * rgerhards, 2009-04-03
 */
rsRetVal tplToArray(struct template *pTpl, msg_t *pMsg, uchar*** ppArr)
{
	DEFiRet;
	struct templateEntry *pTpe;
	uchar **pArr;
	int iArr;
	size_t propLen;
	unsigned short bMustBeFreed;
	uchar *pVal;

	assert(pTpl != NULL);
	assert(pMsg != NULL);
	assert(ppArr != NULL);

	/* loop through the template. We obtain one value, create a
	 * private copy (if necessary), add it to the string array
	 * and then on to the next until we have processed everything.
	 */

	CHKmalloc(pArr = calloc(pTpl->tpenElements + 1, sizeof(uchar*)));
	iArr = 0;

	pTpe = pTpl->pEntryRoot;
	while(pTpe != NULL) {
		if(pTpe->eEntryType == CONSTANT) {
			CHKmalloc(pArr[iArr] = (uchar*)strdup((char*) pTpe->data.constant.pConstant));
		} else 	if(pTpe->eEntryType == FIELD) {
			pVal = (uchar*) MsgGetProp(pMsg, pTpe, pTpe->data.field.propid,
						   pTpe->data.field.propName,  &propLen, &bMustBeFreed);
			if(bMustBeFreed) { /* if it must be freed, it is our own private copy... */
				pArr[iArr] = pVal; /* ... so we can use it! */
			} else {
				CHKmalloc(pArr[iArr] = (uchar*)strdup((char*) pVal));
			}
		}
		iArr++;
		pTpe = pTpe->pNext;
	}

finalize_it:
	*ppArr = (iRet == RS_RET_OK) ? pArr : NULL;

	RETiRet;
}


/* Helper to doSQLEscape. This is called if doSQLEscape
 * runs out of memory allocating the escaped string.
 * Then we are in trouble. We can
 * NOT simply return the unmodified string because this
 * may cause SQL injection. But we also can not simply
 * abort the run, this would be a DoS. I think an appropriate
 * measure is to remove the dangerous \' characters. We
 * replace them by \", which will break the message and
 * signatures eventually present - but this is the
 * best thing we can do now (or does anybody 
 * have a better idea?). rgerhards 2004-11-23
 * added support for "escapeMode" (so doSQLEscape for details).
 * if mode = 1, then backslashes are changed to slashes.
 * rgerhards 2005-09-22
 */
static void doSQLEmergencyEscape(register uchar *p, int escapeMode)
{
	while(*p) {
		if(*p == '\'')
			*p = '"';
		else if((escapeMode == 1) && (*p == '\\'))
			*p = '/';
		++p;
	}
}


/* SQL-Escape a string. Single quotes are found and
 * replaced by two of them. A new buffer is allocated
 * for the provided string and the provided buffer is
 * freed. The length is updated. Parameter pbMustBeFreed
 * is set to 1 if a new buffer is allocated. Otherwise,
 * it is left untouched.
 * --
 * We just discovered a security issue. MySQL is so
 * "smart" to not only support the standard SQL mechanism
 * for escaping quotes, but to also provide its own (using
 * c-type syntax with backslashes). As such, it is actually
 * possible to do sql injection via rsyslogd. The cure is now
 * to escape backslashes, too. As we have found on the web, some
 * other databases seem to be similar "smart" (why do we have standards
 * at all if they are violated without any need???). Even better, MySQL's
 * smartness depends on config settings. So we add a new option to this
 * function that allows the caller to select if they want to standard or
 * "smart" encoding ;)
 * new parameter escapeMode is 0 - standard sql, 1 - "smart" engines
 * 2005-09-22 rgerhards
 */
rsRetVal
doSQLEscape(uchar **pp, size_t *pLen, unsigned short *pbMustBeFreed, int escapeMode)
{
	DEFiRet;
	uchar *p;
	int iLen;
	cstr_t *pStrB = NULL;
	uchar *pszGenerated;

	assert(pp != NULL);
	assert(*pp != NULL);
	assert(pLen != NULL);
	assert(pbMustBeFreed != NULL);

	/* first check if we need to do anything at all... */
	if(escapeMode == 0)
		for(p = *pp ; *p && *p != '\'' ; ++p)
			;
	else
		for(p = *pp ; *p && *p != '\'' && *p != '\\' ; ++p)
			;
	/* when we get out of the loop, we are either at the
	 * string terminator or the first \'. */
	if(*p == '\0')
		FINALIZE; /* nothing to do in this case! */

	p = *pp;
	iLen = *pLen;
	CHKiRet(cstrConstruct(&pStrB));
	
	while(*p) {
		if(*p == '\'') {
			CHKiRet(cstrAppendChar(pStrB, (escapeMode == 0) ? '\'' : '\\'));
			iLen++;	/* reflect the extra character */
		} else if((escapeMode == 1) && (*p == '\\')) {
			CHKiRet(cstrAppendChar(pStrB, '\\'));
			iLen++;	/* reflect the extra character */
		}
		CHKiRet(cstrAppendChar(pStrB, *p));
		++p;
	}
	CHKiRet(cstrFinalize(pStrB));
	CHKiRet(cstrConvSzStrAndDestruct(pStrB, &pszGenerated, 0));

	if(*pbMustBeFreed)
		free(*pp); /* discard previous value */

	*pp = pszGenerated;
	*pLen = iLen;
	*pbMustBeFreed = 1;

finalize_it:
	if(iRet != RS_RET_OK) {
		doSQLEmergencyEscape(*pp, escapeMode);
		if(pStrB != NULL)
			cstrDestruct(&pStrB);
	}

	RETiRet;
}


/* Constructs a template entry object. Returns pointer to it
 * or NULL (if it fails). Pointer to associated template list entry 
 * must be provided.
 */
struct templateEntry* tpeConstruct(struct template *pTpl)
{
	struct templateEntry *pTpe;

	assert(pTpl != NULL);

	if((pTpe = calloc(1, sizeof(struct templateEntry))) == NULL)
		return NULL;
	
	/* basic initialization is done via calloc() - need to
	 * initialize only values != 0. */

	if(pTpl->pEntryLast == NULL){
		/* we are the first element! */
		pTpl->pEntryRoot = pTpl->pEntryLast  = pTpe;
	} else {
		pTpl->pEntryLast->pNext = pTpe;
		pTpl->pEntryLast  = pTpe;
	}
	pTpl->tpenElements++;

	return(pTpe);
}


/* Constructs a template list object. Returns pointer to it
 * or NULL (if it fails).
 */
struct template* tplConstruct(void)
{
	struct template *pTpl;
	if((pTpl = calloc(1, sizeof(struct template))) == NULL)
		return NULL;
	
	/* basic initialisation is done via calloc() - need to
	 * initialize only values != 0. */

	if(tplLast == NULL)	{
		/* we are the first element! */
		tplRoot = tplLast = pTpl;
	} else {
		tplLast->pNext = pTpl;
		tplLast = pTpl;
	}

	return(pTpl);
}


/* helper to tplAddLine. Parses a constant and generates
 * the necessary structure.
 * returns: 0 - ok, 1 - failure
 */
static int do_Constant(unsigned char **pp, struct template *pTpl)
{
	register unsigned char *p;
	cstr_t *pStrB;
	struct templateEntry *pTpe;
	int i;

	assert(pp != NULL);
	assert(*pp != NULL);
	assert(pTpl != NULL);

	p = *pp;

	if(cstrConstruct(&pStrB) != RS_RET_OK)
		 return 1;
	/* process the message and expand escapes
	 * (additional escapes can be added here if needed)
	 */
	while(*p && *p != '%' && *p != '\"') {
		if(*p == '\\') {
			switch(*++p) {
				case '\0':	
					/* the best we can do - it's invalid anyhow... */
					cstrAppendChar(pStrB, *p);
					break;
				case 'n':
					cstrAppendChar(pStrB, '\n');
					++p;
					break;
				case 'r':
					cstrAppendChar(pStrB, '\r');
					++p;
					break;
				case '\\':
					cstrAppendChar(pStrB, '\\');
					++p;
					break;
				case '%':
					cstrAppendChar(pStrB, '%');
					++p;
					break;
				case '0': /* numerical escape sequence */
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					i = 0;
					while(*p && isdigit((int)*p)) {
						i = i * 10 + *p++ - '0';
					}
					cstrAppendChar(pStrB, i);
					break;
				default:
					cstrAppendChar(pStrB, *p++);
					break;
			}
		}
		else
			cstrAppendChar(pStrB, *p++);
	}

	if((pTpe = tpeConstruct(pTpl)) == NULL) {
		rsCStrDestruct(&pStrB);
		return 1;
	}
	pTpe->eEntryType = CONSTANT;
	cstrFinalize(pStrB);
	/* We obtain the length from the counted string object
	 * (before we delete it). Later we might take additional
	 * benefit from the counted string object.
	 * 2005-09-09 rgerhards
	 */
	pTpe->data.constant.iLenConstant = rsCStrLen(pStrB);
	if(cstrConvSzStrAndDestruct(pStrB, &pTpe->data.constant.pConstant, 0) != RS_RET_OK)
		return 1;

	*pp = p;

	return 0;
}


/* Helper to do_Parameter(). This parses the formatting options
 * specified in a template variable. It returns the passed-in pointer
 * updated to the next processed character.
 */
static void doOptions(unsigned char **pp, struct templateEntry *pTpe)
{
	register unsigned char *p;
	unsigned char Buf[64];
	size_t i;

	assert(pp != NULL);
	assert(*pp != NULL);
	assert(pTpe != NULL);

	p = *pp;

	while(*p && *p != '%') {
		/* outer loop - until end of options */
		i = 0;
		while((i < sizeof(Buf) / sizeof(char)) &&
		      *p && *p != '%' && *p != ',') {
			/* inner loop - until end of ONE option */
			Buf[i++] = tolower((int)*p);
			++p;
		}
		Buf[i] = '\0'; /* terminate */
		/* check if we need to skip oversize option */
		while(*p && *p != '%' && *p != ',')
			++p;	/* just skip */
		if(*p == ',')
			++p; /* eat ',' */
		/* OK, we got the option, so now lets look what
		 * it tells us...
		 */
		 if(!strcmp((char*)Buf, "date-mysql")) {
			pTpe->data.field.eDateFormat = tplFmtMySQLDate;
                 } else if(!strcmp((char*)Buf, "date-pgsql")) {
                        pTpe->data.field.eDateFormat = tplFmtPgSQLDate;
		 } else if(!strcmp((char*)Buf, "date-rfc3164")) {
			pTpe->data.field.eDateFormat = tplFmtRFC3164Date;
		 } else if(!strcmp((char*)Buf, "date-rfc3164-buggyday")) {
			pTpe->data.field.eDateFormat = tplFmtRFC3164BuggyDate;
		 } else if(!strcmp((char*)Buf, "date-rfc3339")) {
			pTpe->data.field.eDateFormat = tplFmtRFC3339Date;
		 } else if(!strcmp((char*)Buf, "date-subseconds")) {
			pTpe->data.field.eDateFormat = tplFmtSecFrac;
		 } else if(!strcmp((char*)Buf, "lowercase")) {
			pTpe->data.field.eCaseConv = tplCaseConvLower;
		 } else if(!strcmp((char*)Buf, "uppercase")) {
			pTpe->data.field.eCaseConv = tplCaseConvUpper;
		 } else if(!strcmp((char*)Buf, "sp-if-no-1st-sp")) {
			pTpe->data.field.options.bSPIffNo1stSP = 1;
		 } else if(!strcmp((char*)Buf, "escape-cc")) {
			pTpe->data.field.options.bEscapeCC = 1;
		 } else if(!strcmp((char*)Buf, "drop-cc")) {
			pTpe->data.field.options.bDropCC = 1;
		 } else if(!strcmp((char*)Buf, "space-cc")) {
			pTpe->data.field.options.bSpaceCC = 1;
		 } else if(!strcmp((char*)Buf, "drop-last-lf")) {
			pTpe->data.field.options.bDropLastLF = 1;
		 } else if(!strcmp((char*)Buf, "secpath-drop")) {
			pTpe->data.field.options.bSecPathDrop = 1;
		 } else if(!strcmp((char*)Buf, "secpath-replace")) {
			pTpe->data.field.options.bSecPathReplace = 1;
		 } else if(!strcmp((char*)Buf, "csv")) {
			pTpe->data.field.options.bCSV = 1;
		 } else {
			dbgprintf("Invalid field option '%s' specified - ignored.\n", Buf);
		 }
	}

	*pp = p;
}


/* helper to tplAddLine. Parses a parameter and generates
 * the necessary structure.
 * returns: 0 - ok, 1 - failure
 */
static int do_Parameter(unsigned char **pp, struct template *pTpl)
{
	unsigned char *p;
	cstr_t *pStrB;
	struct templateEntry *pTpe;
	int iNum;	/* to compute numbers */
#ifdef FEATURE_REGEXP
	/* APR: variables for regex */
	rsRetVal iRetLocal;
	int longitud;
	unsigned char *regex_char;
	unsigned char *regex_end;
#endif

	assert(pp != NULL);
	assert(*pp != NULL);
	assert(pTpl != NULL);

	p = (unsigned char*) *pp;

	if(cstrConstruct(&pStrB) != RS_RET_OK)
		 return 1;

	if((pTpe = tpeConstruct(pTpl)) == NULL) {
		/* TODO: add handler */
		dbgprintf("Could not allocate memory for template parameter!\n");
		return 1;
	}
	pTpe->eEntryType = FIELD;

	while(*p && *p != '%' && *p != ':') {
		cstrAppendChar(pStrB, tolower(*p));
		++p; /* do NOT do this in tolower()! */
	}

	/* got the name */
	cstrFinalize(pStrB);

	if(propNameToID(pStrB, &pTpe->data.field.propid) != RS_RET_OK) {
		cstrDestruct(&pStrB);
		return 1;
	}
	if(pTpe->data.field.propid == PROP_CEE) {
		/* in CEE case, we need to preserve the actual property name */
		if((pTpe->data.field.propName = es_newStrFromCStr((char*)cstrGetSzStrNoNULL(pStrB)+2, cstrLen(pStrB)-2)) == NULL) {
			cstrDestruct(&pStrB);
			return 1;
		}
	}
	cstrDestruct(&pStrB);

	/* Check frompos, if it has an R, then topos should be a regex */
	if(*p == ':') {
		++p; /* eat ':' */
#ifdef FEATURE_REGEXP
		if(*p == 'R') {
			/* APR: R found! regex alarm ! :) */
			++p;	/* eat ':' */

			/* first come the regex type */
			if(*p == ',') {
				++p; /* eat ',' */
				if(p[0] == 'B' && p[1] == 'R' && p[2] == 'E' && (p[3] == ',' || p[3] == ':')) {
					pTpe->data.field.typeRegex = TPL_REGEX_BRE;
					p += 3; /* eat indicator sequence */
				} else if(p[0] == 'E' && p[1] == 'R' && p[2] == 'E' && (p[3] == ',' || p[3] == ':')) {
					pTpe->data.field.typeRegex = TPL_REGEX_ERE;
					p += 3; /* eat indicator sequence */
				} else {
					errmsg.LogError(0, NO_ERRCODE, "error: invalid regular expression type, rest of line %s",
				               (char*) p);
				}
			}

			/* now check for submatch ID */
			pTpe->data.field.iSubMatchToUse = 0;
			if(*p == ',') {
				/* in this case a number follows, which indicates which match
				 * shall be used. This must be a single digit.
				 */
				++p; /* eat ',' */
				if(isdigit((int) *p)) {
					pTpe->data.field.iSubMatchToUse = *p - '0';
					++p; /* eat digit */
				}
			}

			/* now pull what to do if we do not find a match */
			if(*p == ',') {
				++p; /* eat ',' */
				if(p[0] == 'D' && p[1] == 'F' && p[2] == 'L' && p[3] == 'T'
				   && (p[4] == ',' || p[4] == ':')) {
					pTpe->data.field.nomatchAction = TPL_REGEX_NOMATCH_USE_DFLTSTR;
					p += 4; /* eat indicator sequence */
				} else if(p[0] == 'B' && p[1] == 'L' && p[2] == 'A' && p[3] == 'N' && p[4] == 'K'
				          && (p[5] == ',' || p[5] == ':')) {
					pTpe->data.field.nomatchAction = TPL_REGEX_NOMATCH_USE_BLANK;
					p += 5; /* eat indicator sequence */
				} else if(p[0] == 'F' && p[1] == 'I' && p[2] == 'E' && p[3] == 'L' && p[4] == 'D'
				          && (p[5] == ',' || p[5] == ':')) {
					pTpe->data.field.nomatchAction = TPL_REGEX_NOMATCH_USE_WHOLE_FIELD;
					p += 5; /* eat indicator sequence */
				} else if(p[0] == 'Z' && p[1] == 'E' && p[2] == 'R' && p[3] == 'O'
				          && (p[4] == ',' || p[4] == ':')) {
					pTpe->data.field.nomatchAction = TPL_REGEX_NOMATCH_USE_ZERO;
					p += 4; /* eat indicator sequence */
				} else if(p[0] == ',') { /* empty, use default */
					pTpe->data.field.nomatchAction = TPL_REGEX_NOMATCH_USE_DFLTSTR;
					 /* do NOT eat indicator sequence, as this was already eaten - the 
					  * comma itself is already part of the next field.
					  */
				} else {
					errmsg.LogError(0, NO_ERRCODE, "error: invalid regular expression type, rest of line %s",
				               (char*) p);
				}
			}

			/* now check for match ID */
			pTpe->data.field.iMatchToUse = 0;
			if(*p == ',') {
				/* in this case a number follows, which indicates which match
				 * shall be used. This must be a single digit.
				 */
				++p; /* eat ',' */
				if(isdigit((int) *p)) {
					pTpe->data.field.iMatchToUse = *p - '0';
					++p; /* eat digit */
				}
			}

			if(*p != ':') {
				/* There is something more than an R , this is invalid ! */
				/* Complain on extra characters */
				errmsg.LogError(0, NO_ERRCODE, "error: invalid character in frompos after \"R\", property: '%%%s'",
				    (char*) *pp);
			} else {
				pTpe->data.field.has_regex = 1;
				dbgprintf("we have a regexp and use match #%d, submatch #%d\n",
					  pTpe->data.field.iMatchToUse, pTpe->data.field.iSubMatchToUse);
			}
		} else {
			/* now we fall through the "regular" FromPos code */
#endif /* #ifdef FEATURE_REGEXP */
			if(*p == 'F') {
				pTpe->data.field.field_expand = 0;
				/* we have a field counter, so indicate it in the template */
				++p; /* eat 'F' */
				if (*p == ':') {
					/* no delimiter specified, so use the default (HT) */
					pTpe->data.field.has_fields = 1;
					pTpe->data.field.field_delim = 9;
				} else if (*p == ',') {
					++p; /* eat ',' */
					/* configured delimiter follows, so we need to obtain
					 * it. Important: the following number must be the
					 * **DECIMAL** ASCII value of the delimiter character.
					 */
					pTpe->data.field.has_fields = 1;
					if(!isdigit((int)*p)) {
						/* complain and use default */
						errmsg.LogError(0, NO_ERRCODE, "error: invalid character in frompos after \"F,\", property: '%%%s' - using 9 (HT) as field delimiter",
						    (char*) *pp);
						pTpe->data.field.field_delim = 9;
					} else {
						iNum = 0;
						while(isdigit((int)*p))
							iNum = iNum * 10 + *p++ - '0';
						if(iNum < 0 || iNum > 255) {
							errmsg.LogError(0, NO_ERRCODE, "error: non-USASCII delimiter character value %d in template - using 9 (HT) as substitute", iNum);
							pTpe->data.field.field_delim = 9;
						  } else {
							pTpe->data.field.field_delim = iNum;
							if (*p == '+') {
								pTpe->data.field.field_expand = 1;
								p ++;
							}
						  }
					}
				} else {
					/* invalid character after F, so we need to reject
					 * this.
					 */
					errmsg.LogError(0, NO_ERRCODE, "error: invalid character in frompos after \"F\", property: '%%%s'",
					    (char*) *pp);
				}
			} else {
				/* we now have a simple offset in frompos (the previously "normal" case) */
				iNum = 0;
				while(isdigit((int)*p))
					iNum = iNum * 10 + *p++ - '0';
				pTpe->data.field.iFromPos = iNum;
				/* skip to next known good */
				while(*p && *p != '%' && *p != ':') {
					/* TODO: complain on extra characters */
					dbgprintf("error: extra character in frompos: '%s'\n", p);
					++p;
				}
			}
#ifdef FEATURE_REGEXP
		}
#endif /* #ifdef FEATURE_REGEXP */
	}
	/* check topos  (holds an regex if FromPos is "R"*/
	if(*p == ':') {
		++p; /* eat ':' */

#ifdef FEATURE_REGEXP
		if (pTpe->data.field.has_regex) {

			dbgprintf("debug: has regex \n");

			/* APR 2005-09 I need the string that represent the regex */
			/* The regex end is: "--end" */
			/* TODO : this is hardcoded and cant be escaped, please change */
			regex_end = (unsigned char*) strstr((char*)p, "--end");
			if (regex_end == NULL) {
				dbgprintf("error: can not find regex end in: '%s'\n", p);
				pTpe->data.field.has_regex = 0;
			} else {
				/* We get here ONLY if the regex end was found */
				longitud = regex_end - p;
				/* Malloc for the regex string */
				regex_char = (unsigned char *) MALLOC(longitud + 1);
				if(regex_char == NULL) {
					dbgprintf("Could not allocate memory for template parameter!\n");
					pTpe->data.field.has_regex = 0;
					return 1;
					/* TODO: RGer: check if we can recover better... (probably not) */
				}

				/* Get the regex string for compiling later */
				memcpy(regex_char, p, longitud);
				regex_char[longitud] = '\0';

				dbgprintf("debug: regex detected: '%s'\n", regex_char);

				/* Now i compile the regex */
				/* Remember that the re is an attribute of the Template entry */
				if((iRetLocal = objUse(regexp, LM_REGEXP_FILENAME)) == RS_RET_OK) {
					int iOptions;
					iOptions = (pTpe->data.field.typeRegex == TPL_REGEX_ERE) ? REG_EXTENDED : 0;
					if(regexp.regcomp(&(pTpe->data.field.re), (char*) regex_char, iOptions) != 0) {
						dbgprintf("error: can not compile regex: '%s'\n", regex_char);
						pTpe->data.field.has_regex = 2;
					}
				} else {
					/* regexp object could not be loaded */
					dbgprintf("error %d trying to load regexp library - this may be desired and thus OK",
						  iRetLocal);
					if(bFirstRegexpErrmsg) { /* prevent flood of messages, maybe even an endless loop! */
						bFirstRegexpErrmsg = 0;
						errmsg.LogError(0, NO_ERRCODE, "regexp library could not be loaded (error %d), "
								"regexp ignored", iRetLocal);
					}
					pTpe->data.field.has_regex = 2;
				}

				/* Finally we move the pointer to the end of the regex
				 * so it aint parsed twice or something weird */
				p = regex_end + 5/*strlen("--end")*/;
				free(regex_char);
			}
		} else if(*p == '$') {
			/* shortcut for "end of message */
			p++; /* eat '$' */
			/* in this case, we do a quick, somewhat dirty but totally
			 * legitimate trick: we simply use a topos that is higher than
			 * potentially ever can happen. The code below checks that no copy
			 * will occur after the end of string, so this is perfectly legal.
			 * rgerhards, 2006-10-17
			 */
			pTpe->data.field.iToPos = 9999999;
		} else {
			/* fallthrough to "regular" ToPos code */
#endif /* #ifdef FEATURE_REGEXP */

			iNum = 0;
			while(isdigit((int)*p))
				iNum = iNum * 10 + *p++ - '0';
			pTpe->data.field.iToPos = iNum;
			/* skip to next known good */
			while(*p && *p != '%' && *p != ':') {
				/* TODO: complain on extra characters */
				dbgprintf("error: extra character in frompos: '%s'\n", p);
				++p;
			}
#ifdef FEATURE_REGEXP
		}
#endif /* #ifdef FEATURE_REGEXP */
	}

	if((pTpe->data.field.has_fields == 0) && (pTpe->data.field.iToPos < pTpe->data.field.iFromPos)) {
		iNum = pTpe->data.field.iToPos;
		pTpe->data.field.iToPos = pTpe->data.field.iFromPos;
		pTpe->data.field.iFromPos = iNum;
	}

	/* check options */
	if(*p == ':') {
		++p; /* eat ':' */
		doOptions(&p, pTpe);
	}

	if(*p) ++p; /* eat '%' */

	*pp = p;
	return 0;
}


/* Add a new entry for a template module.
 * returns pointer to new object if it succeeds, NULL otherwise.
 * rgerhards, 2010-05-31
 */
static rsRetVal
tplAddTplMod(struct template *pTpl, uchar** ppRestOfConfLine)
{
	uchar *pSrc, *pDst;
	uchar szMod[2048];
	unsigned lenMod;
	strgen_t *pStrgen;
	DEFiRet;

	pSrc = *ppRestOfConfLine;
	pDst = szMod;
	lenMod = 0;
	while(*pSrc && !isspace(*pSrc) && lenMod < sizeof(szMod) - 1) {
		szMod[lenMod] = *pSrc++;
		lenMod++;
		
	}
	szMod[lenMod] = '\0';
	*ppRestOfConfLine = pSrc;
	CHKiRet(strgen.FindStrgen(&pStrgen, szMod));
	pTpl->pStrgen = pStrgen->pModule->mod.sm.strgen;
	DBGPRINTF("template bound to strgen '%s'\n", szMod);
	/* check if the name potentially contains some well-known options
	 * Note: we have opted to let the name contain all options. This sounds 
	 * useful, because the strgen MUST actually implement a specific set
	 * of options. Doing this via the name looks to the enduser as if the
	 * regular syntax were used, and it make sure the strgen postively
	 * acknowledged implementing the option. -- rgerhards, 2011-03-21
	 */
	if(lenMod > 6 && !strcasecmp((char*) szMod + lenMod - 7, ",stdsql")) {
		pTpl->optFormatForSQL = 2;
		DBGPRINTF("strgen suports the stdsql option\n");
	} else if(lenMod > 3 && !strcasecmp((char*) szMod+ lenMod - 4, ",sql")) {
		pTpl->optFormatForSQL = 1;
		DBGPRINTF("strgen suports the sql option\n");
	}

finalize_it:
	RETiRet;
}


/* Add a new template line
 * returns pointer to new object if it succeeds, NULL otherwise.
 */
struct template *tplAddLine(char* pName, uchar** ppRestOfConfLine)
{
	struct template *pTpl;
 	unsigned char *p;
	int bDone;
	char optBuf[128]; /* buffer for options - should be more than enough... */
	size_t i;
	rsRetVal localRet;

	assert(pName != NULL);
	assert(ppRestOfConfLine != NULL);

	if((pTpl = tplConstruct()) == NULL)
		return NULL;
	
	pTpl->iLenName = strlen(pName);
	pTpl->pszName = (char*) MALLOC(sizeof(char) * (pTpl->iLenName + 1));
	if(pTpl->pszName == NULL) {
		dbgprintf("tplAddLine could not alloc memory for template name!");
		pTpl->iLenName = 0;
		return NULL;
		/* I know - we create a memory leak here - but I deem
		 * it acceptable as it is a) a very small leak b) very
		 * unlikely to happen. rgerhards 2004-11-17
		 */
	}
	memcpy(pTpl->pszName, pName, pTpl->iLenName + 1);

	/* now actually parse the line */
	p = *ppRestOfConfLine;
	assert(p != NULL);

	while(isspace((int)*p))/* skip whitespace */
		++p;
	
	switch(*p) {
	case '"': /* just continue */
		break;
	case '=':
		*ppRestOfConfLine = p + 1;
		localRet = tplAddTplMod(pTpl, ppRestOfConfLine);
		if(localRet != RS_RET_OK) {
			errmsg.LogError(0, localRet, "Template '%s': error %d defining template via strgen module",
					pTpl->pszName, localRet);
			/* we simply make the template defunct in this case by setting
			 * its name to a zero-string. We do not free it, as this would
			 * require additional code and causes only a very small memory
			 * consumption. Memory is freed, however, in normal operation
			 * and most importantly by HUPing syslogd.
			 */
			*pTpl->pszName = '\0';
		}
		return NULL;
	default:
		dbgprintf("Template '%s' invalid, does not start with '\"'!\n", pTpl->pszName);
		/* we simply make the template defunct in this case by setting
		 * its name to a zero-string. We do not free it, as this would
		 * require additional code and causes only a very small memory
		 * consumption.
		 */
		*pTpl->pszName = '\0';
		return NULL;
	}
	++p;

	/* we finally go to the actual template string - so let's have some fun... */
	bDone = *p ? 0 : 1;
	while(!bDone) {
		switch(*p) {
			case '\0':
				bDone = 1;
				break;
			case '%': /* parameter */
				++p; /* eat '%' */
				do_Parameter(&p, pTpl);
				break;
			default: /* constant */
				do_Constant(&p, pTpl);
				break;
		}
		if(*p == '"') {/* end of template string? */
			++p;	/* eat it! */
			bDone = 1;
		}
	}
	
	/* we now have the template - let's look at the options (if any)
	 * we process options until we reach the end of the string or 
	 * an error occurs - whichever is first.
	 */
	while(*p) {
		while(isspace((int)*p))/* skip whitespace */
			++p;
		
		if(*p != ',')
			break;
		++p; /* eat ',' */

		while(isspace((int)*p))/* skip whitespace */
			++p;
		
		/* read option word */
		i = 0;
		while(i < sizeof(optBuf) / sizeof(char) - 1
		      && *p && *p != '=' && *p !=',' && *p != '\n') {
			optBuf[i++] = tolower((int)*p);
			++p;
		}
		optBuf[i] = '\0';

		if(*p == '\n')
			++p;

		/* as of now, the no form is nonsense... but I do include
		 * it anyhow... ;) rgerhards 2004-11-22
		 */
		if(!strcmp(optBuf, "stdsql")) {
			pTpl->optFormatForSQL = 2;
		} else if(!strcmp(optBuf, "sql")) {
			pTpl->optFormatForSQL = 1;
		} else if(!strcmp(optBuf, "nosql")) {
			pTpl->optFormatForSQL = 0;
		} else {
			dbgprintf("Invalid option '%s' ignored.\n", optBuf);
		}
	}

	*ppRestOfConfLine = p;

	return(pTpl);
}


/* Find a template object based on name. Search
 * currently is case-senstive (should we change?).
 * returns pointer to template object if found and
 * NULL otherwise.
 * rgerhards 2004-11-17
 */
struct template *tplFind(char *pName, int iLenName)
{
	struct template *pTpl;

	assert(pName != NULL);

	pTpl = tplRoot;
	while(pTpl != NULL &&
	      !(pTpl->iLenName == iLenName &&
	        !strcmp(pTpl->pszName, pName)
	        ))
		{
			pTpl = pTpl->pNext;
		}
	return(pTpl);
}

/* Destroy the template structure. This is for de-initialization
 * at program end. Everything is deleted.
 * rgerhards 2005-02-22
 * I have commented out dbgprintfs, because they are not needed for
 * "normal" debugging. Uncomment them, if they are needed.
 * rgerhards, 2007-07-05
 */
void tplDeleteAll(void)
{
	struct template *pTpl, *pTplDel;
	struct templateEntry *pTpe, *pTpeDel;
	BEGINfunc

	pTpl = tplRoot;
	while(pTpl != NULL) {
		/* dbgprintf("Delete Template: Name='%s'\n ", pTpl->pszName == NULL? "NULL" : pTpl->pszName);*/
		pTpe = pTpl->pEntryRoot;
		while(pTpe != NULL) {
			pTpeDel = pTpe;
			pTpe = pTpe->pNext;
			/*dbgprintf("\tDelete Entry(%x): type %d, ", (unsigned) pTpeDel, pTpeDel->eEntryType);*/
			switch(pTpeDel->eEntryType) {
			case UNDEFINED:
				/*dbgprintf("(UNDEFINED)");*/
				break;
			case CONSTANT:
				/*dbgprintf("(CONSTANT), value: '%s'",
					pTpeDel->data.constant.pConstant);*/
				free(pTpeDel->data.constant.pConstant);
				break;
			case FIELD:
				/* check if we have a regexp and, if so, delete it */
#ifdef FEATURE_REGEXP
				if(pTpeDel->data.field.has_regex != 0) {
					if(objUse(regexp, LM_REGEXP_FILENAME) == RS_RET_OK) {
						regexp.regfree(&(pTpeDel->data.field.re));
					}
				}
				if(pTpeDel->data.field.propName != NULL)
					es_deleteStr(pTpeDel->data.field.propName);
#endif
				break;
			}
			/*dbgprintf("\n");*/
			free(pTpeDel);
		}
		pTplDel = pTpl;
		pTpl = pTpl->pNext;
		if(pTplDel->pszName != NULL)
			free(pTplDel->pszName);
		free(pTplDel);
	}
	ENDfunc
}


/* Destroy all templates obtained from conf file
 * preserving hardcoded ones. This is called from init().
 */
void tplDeleteNew(void)
{
	struct template *pTpl, *pTplDel;
	struct templateEntry *pTpe, *pTpeDel;

	BEGINfunc

	if(tplRoot == NULL || tplLastStatic == NULL)
		return;

	pTpl = tplLastStatic->pNext;
	tplLastStatic->pNext = NULL;
	tplLast = tplLastStatic;
	while(pTpl != NULL) {
		/* dbgprintf("Delete Template: Name='%s'\n ", pTpl->pszName == NULL? "NULL" : pTpl->pszName);*/
		pTpe = pTpl->pEntryRoot;
		while(pTpe != NULL) {
			pTpeDel = pTpe;
			pTpe = pTpe->pNext;
			/*dbgprintf("\tDelete Entry(%x): type %d, ", (unsigned) pTpeDel, pTpeDel->eEntryType);*/
			switch(pTpeDel->eEntryType) {
			case UNDEFINED:
				/*dbgprintf("(UNDEFINED)");*/
				break;
			case CONSTANT:
				/*dbgprintf("(CONSTANT), value: '%s'",
					pTpeDel->data.constant.pConstant);*/
				free(pTpeDel->data.constant.pConstant);
				break;
			case FIELD:
#ifdef FEATURE_REGEXP
				/* check if we have a regexp and, if so, delete it */
				if(pTpeDel->data.field.has_regex != 0) {
					if(objUse(regexp, LM_REGEXP_FILENAME) == RS_RET_OK) {
						regexp.regfree(&(pTpeDel->data.field.re));
					}
				}
				if(pTpeDel->data.field.propName != NULL)
					es_deleteStr(pTpeDel->data.field.propName);
#endif
				break;
			}
			/*dbgprintf("\n");*/
			free(pTpeDel);
		}
		pTplDel = pTpl;
		pTpl = pTpl->pNext;
		if(pTplDel->pszName != NULL)
			free(pTplDel->pszName);
		free(pTplDel);
	}
	ENDfunc
}

/* Store the pointer to the last hardcoded teplate */
void tplLastStaticInit(struct template *tpl)
{
	tplLastStatic = tpl;
}

/* Print the template structure. This is more or less a 
 * debug or test aid, but anyhow I think it's worth it...
 */
void tplPrintList(void)
{
	struct template *pTpl;
	struct templateEntry *pTpe;

	pTpl = tplRoot;
	while(pTpl != NULL) {
		dbgprintf("Template: Name='%s' ", pTpl->pszName == NULL? "NULL" : pTpl->pszName);
		if(pTpl->optFormatForSQL == 1)
			dbgprintf("[SQL-Format (MySQL)] ");
		else if(pTpl->optFormatForSQL == 2)
			dbgprintf("[SQL-Format (standard SQL)] ");
		dbgprintf("\n");
		pTpe = pTpl->pEntryRoot;
		while(pTpe != NULL) {
			dbgprintf("\tEntry(%lx): type %d, ", (unsigned long) pTpe, pTpe->eEntryType);
			switch(pTpe->eEntryType) {
			case UNDEFINED:
				dbgprintf("(UNDEFINED)");
				break;
			case CONSTANT:
				dbgprintf("(CONSTANT), value: '%s'",
					pTpe->data.constant.pConstant);
				break;
			case FIELD:
				dbgprintf("(FIELD), value: '%d' ", pTpe->data.field.propid);
				if(pTpe->data.field.propid == PROP_CEE) {
					char *cstr = es_str2cstr(pTpe->data.field.propName, NULL);
					dbgprintf("[EE-Property: '%s'] ", cstr);
					free(cstr);
				}
				switch(pTpe->data.field.eDateFormat) {
				case tplFmtDefault:
					break;
				case tplFmtMySQLDate:
					dbgprintf("[Format as MySQL-Date] ");
					break;
                                case tplFmtPgSQLDate:
                                        dbgprintf("[Format as PgSQL-Date] ");
                                        break;
				case tplFmtRFC3164Date:
					dbgprintf("[Format as RFC3164-Date] ");
					break;
				case tplFmtRFC3339Date:
					dbgprintf("[Format as RFC3339-Date] ");
					break;
				default:
					dbgprintf("[INVALID eDateFormat %d] ", pTpe->data.field.eDateFormat);
				}
				switch(pTpe->data.field.eCaseConv) {
				case tplCaseConvNo:
					break;
				case tplCaseConvLower:
					dbgprintf("[Converted to Lower Case] ");
					break;
				case tplCaseConvUpper:
					dbgprintf("[Converted to Upper Case] ");
					break;
				}
				if(pTpe->data.field.options.bEscapeCC) {
				  	dbgprintf("[escape control-characters] ");
				}
				if(pTpe->data.field.options.bDropCC) {
				  	dbgprintf("[drop control-characters] ");
				}
				if(pTpe->data.field.options.bSpaceCC) {
				  	dbgprintf("[replace control-characters with space] ");
				}
				if(pTpe->data.field.options.bSecPathDrop) {
				  	dbgprintf("[slashes are dropped] ");
				}
				if(pTpe->data.field.options.bSecPathReplace) {
				  	dbgprintf("[slashes are replaced by '_'] ");
				}
				if(pTpe->data.field.options.bSPIffNo1stSP) {
				  	dbgprintf("[SP iff no first SP] ");
				}
				if(pTpe->data.field.options.bCSV) {
				  	dbgprintf("[format as CSV (RFC4180)]");
				}
				if(pTpe->data.field.options.bDropLastLF) {
				  	dbgprintf("[drop last LF in msg] ");
				}
				if(pTpe->data.field.has_fields == 1) {
				  	dbgprintf("[substring, field #%d only (delemiter %d)] ",
						pTpe->data.field.iToPos, pTpe->data.field.field_delim);
				} else if(pTpe->data.field.iFromPos != 0 ||
				          pTpe->data.field.iToPos != 0) {
				  	dbgprintf("[substring, from character %d to %d] ",
						pTpe->data.field.iFromPos,
						pTpe->data.field.iToPos);
				}
				break;
			}
			dbgprintf("\n");
			pTpe = pTpe->pNext;
		}
		pTpl = pTpl->pNext; /* done, go next */
	}
}

int tplGetEntryCount(struct template *pTpl)
{
	assert(pTpl != NULL);
	return(pTpl->tpenElements);
}

/* our init function. TODO: remove once converted to a class
 */
rsRetVal templateInit()
{
	DEFiRet;
	CHKiRet(objGetObjInterface(&obj));
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(strgen, CORE_COMPONENT));

finalize_it:
	RETiRet;
}
