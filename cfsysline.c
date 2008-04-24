/* cfsysline.c
 * Implementation of the configuration system line object.
 *
 * File begun on 2007-07-30 by RGerhards
 *
 * Copyright (C) 2007, 2008 by Rainer Gerhards and Adiscon GmbH.
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
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <pwd.h>
#include <grp.h>

#include "syslogd.h" /* TODO: when the module interface & library design is done, this should be able to go away */
#include "cfsysline.h"
#include "obj.h"
#include "errmsg.h"
#include "srUtils.h"


/* static data */
DEFobjCurrIf(obj)
DEFobjCurrIf(errmsg)

linkedList_t llCmdList; /* this is NOT a pointer - no typo here ;) */

/* --------------- START functions for handling canned syntaxes --------------- */


/* parse a character from the config line
 * added 2007-07-17 by rgerhards
 * TODO: enhance this function to handle different classes of characters
 * HINT: check if char is ' and, if so, use 'c' where c may also be things
 * like \t etc.
 */
static rsRetVal doGetChar(uchar **pp, rsRetVal (*pSetHdlr)(void*, uid_t), void *pVal)
{
	DEFiRet;

	assert(pp != NULL);
	assert(*pp != NULL);

	skipWhiteSpace(pp); /* skip over any whitespace */

	/* if we are not at a '\0', we have our new char - no validity checks here... */
	if(**pp == '\0') {
		errmsg.LogError(NO_ERRCODE, "No character available");
		iRet = RS_RET_NOT_FOUND;
	} else {
		if(pSetHdlr == NULL) {
			/* we should set value directly to var */
			*((uchar*)pVal) = **pp;
		} else {
			/* we set value via a set function */
			CHKiRet(pSetHdlr(pVal, **pp));
		}
		++(*pp); /* eat processed char */
	}

finalize_it:
	RETiRet;
}


/* Parse a number from the configuration line. This is more or less
 * a shell to call the custom handler.
 * rgerhards, 2007-07-31
 */
static rsRetVal doCustomHdlr(uchar **pp, rsRetVal (*pSetHdlr)(uchar**, void*), void *pVal)
{
	DEFiRet;

	assert(pp != NULL);
	assert(*pp != NULL);

	CHKiRet(pSetHdlr(pp, pVal));

finalize_it:
	RETiRet;
}


/* Parse a number from the configuration line. This functions just parses
 * the number and does NOT call any handlers or set any values. It is just
 * for INTERNAL USE by other parse functions!
 * rgerhards, 2008-01-08
 */
static rsRetVal parseIntVal(uchar **pp, int64 *pVal)
{
	DEFiRet;
	uchar *p;
	int64 i;	
	int bWasNegative;

	assert(pp != NULL);
	assert(*pp != NULL);
	assert(pVal != NULL);
	
	skipWhiteSpace(pp); /* skip over any whitespace */
	p = *pp;

	if(*p == '-') {
		bWasNegative = 1;
		++p; /* eat it */
	} else {
		bWasNegative = 0;
	}

	if(!isdigit((int) *p)) {
		errno = 0;
		errmsg.LogError(NO_ERRCODE, "invalid number");
		ABORT_FINALIZE(RS_RET_INVALID_INT);
	}

	/* pull value */
	for(i = 0 ; *p && (isdigit((int) *p) || *p == '.' || *p == ',')  ; ++p) {
		if(isdigit((int) *p)) {
			i = i * 10 + *p - '0';
		}
	}

	if(bWasNegative)
		i *= -1;

	*pVal = i;
	*pp = p;

finalize_it:
	RETiRet;
}


/* Parse a number from the configuration line.
 * rgerhards, 2007-07-31
 */
static rsRetVal doGetInt(uchar **pp, rsRetVal (*pSetHdlr)(void*, uid_t), void *pVal)
{
	uchar *p;
	DEFiRet;
	int64 i;	

	assert(pp != NULL);
	assert(*pp != NULL);
	
	CHKiRet(parseIntVal(pp, &i));
	p = *pp;

	if(pSetHdlr == NULL) {
		/* we should set value directly to var */
		*((int*)pVal) = (int) i;
	} else {
		/* we set value via a set function */
		CHKiRet(pSetHdlr(pVal, (int) i));
	}

	*pp = p;

finalize_it:
	RETiRet;
}


/* Parse a size from the configuration line. This is basically an integer
 * syntax, but modifiers may be added after the integer (e.g. 1k to mean
 * 1024). The size must immediately follow the number. Note that the
 * param value must be int64!
 * rgerhards, 2008-01-09
 */
static rsRetVal doGetSize(uchar **pp, rsRetVal (*pSetHdlr)(void*, uid_t), void *pVal)
{
	DEFiRet;
	int64 i;

	assert(pp != NULL);
	assert(*pp != NULL);
	
	CHKiRet(parseIntVal(pp, &i));

	/* we now check if the next character is one of our known modifiers.
	 * If so, we accept it as such. If not, we leave it alone. tera and
	 * above does not make any sense as that is above a 32-bit int value.
	 */
	switch(**pp) {
		/* traditional binary-based definitions */
		case 'k': i *= 1024; ++(*pp); break;
		case 'm': i *= 1024 * 1024; ++(*pp); break;
		case 'g': i *= 1024 * 1024 * 1024; ++(*pp); break;
		case 't': i *= (int64) 1024 * 1024 * 1024 * 1024; ++(*pp); break; /* tera */
		case 'p': i *= (int64) 1024 * 1024 * 1024 * 1024 * 1024; ++(*pp); break; /* peta */
		case 'e': i *= (int64) 1024 * 1024 * 1024 * 1024 * 1024 * 1024; ++(*pp); break; /* exa */
		/* and now the "new" 1000-based definitions */
		case 'K': i *= 1000; ++(*pp); break;
		case 'M': i *= 10000; ++(*pp); break;
		case 'G': i *= 100000; ++(*pp); break;
		case 'T': i *= 1000000; ++(*pp); break; /* tera */
		case 'P': i *= 10000000; ++(*pp); break; /* peta */
		case 'E': i *= 100000000; ++(*pp); break; /* exa */
	}

	/* done */
	if(pSetHdlr == NULL) {
		/* we should set value directly to var */
		*((int64*)pVal) = i;
	} else {
		/* we set value via a set function */
		CHKiRet(pSetHdlr(pVal, i));
	}

finalize_it:
	RETiRet;
}


/* Parse and interpet a $FileCreateMode and $umask line. This function
 * pulls the creation mode and, if successful, stores it
 * into the global variable so that the rest of rsyslogd
 * opens files with that mode. Any previous value will be
 * overwritten.
 * HINT: if we store the creation mode in selector_t, we
 * can even specify multiple modes simply be virtue of
 * being placed in the right section of rsyslog.conf
 * rgerhards, 2007-07-4 (happy independence day to my US friends!)
 * Parameter **pp has a pointer to the current config line.
 * On exit, it will be updated to the processed position.
 */
static rsRetVal doFileCreateMode(uchar **pp, rsRetVal (*pSetHdlr)(void*, uid_t), void *pVal)
{
	uchar *p;
	DEFiRet;
	uchar errMsg[128];	/* for dynamic error messages */
	int iVal;	

	assert(pp != NULL);
	assert(*pp != NULL);
	
	skipWhiteSpace(pp); /* skip over any whitespace */
	p = *pp;

	/* for now, we parse and accept only octal numbers
	 * Sequence of tests is important, we are using boolean shortcuts
	 * to avoid addressing invalid memory!
	 */
	if(!(   (*p == '0')
	     && (*(p+1) && *(p+1) >= '0' && *(p+1) <= '7')
	     && (*(p+2) && *(p+2) >= '0' && *(p+2) <= '7')
	     && (*(p+3) && *(p+3) >= '0' && *(p+3) <= '7')  )  ) {
		snprintf((char*) errMsg, sizeof(errMsg)/sizeof(uchar),
		         "value must be octal (e.g 0644).");
		errno = 0;
		errmsg.LogError(NO_ERRCODE, "%s", errMsg);
		ABORT_FINALIZE(RS_RET_INVALID_VALUE);
	}

	/*  we reach this code only if the octal number is ok - so we can now
	 *  compute the value.
	 */
	iVal  = (*(p+1)-'0') * 64 + (*(p+2)-'0') * 8 + (*(p+3)-'0');

	if(pSetHdlr == NULL) {
		/* we should set value directly to var */
		*((int*)pVal) = iVal;
	} else {
		/* we set value via a set function */
		CHKiRet(pSetHdlr(pVal, iVal));
	}

	p += 4;	/* eat the octal number */
	*pp = p;

finalize_it:
	RETiRet;
}


/* Parse and interpret an on/off inside a config file line. This is most
 * often used for boolean options, but of course it may also be used
 * for other things. The passed-in pointer is updated to point to
 * the first unparsed character on exit. Function emits error messages
 * if the value is neither on or off. It returns 0 if the option is off,
 * 1 if it is on and another value if there was an error.
 * rgerhards, 2007-07-15
 */
static int doParseOnOffOption(uchar **pp)
{
	uchar *pOptStart;
	uchar szOpt[32];

	assert(pp != NULL);
	assert(*pp != NULL);

	pOptStart = *pp;
	skipWhiteSpace(pp); /* skip over any whitespace */

	if(getSubString(pp, (char*) szOpt, sizeof(szOpt) / sizeof(uchar), ' ')  != 0) {
		errmsg.LogError(NO_ERRCODE, "Invalid $-configline - could not extract on/off option");
		return -1;
	}
	
	if(!strcmp((char*)szOpt, "on")) {
		return 1;
	} else if(!strcmp((char*)szOpt, "off")) {
		return 0;
	} else {
		errmsg.LogError(NO_ERRCODE, "Option value must be on or off, but is '%s'", (char*)pOptStart);
		return -1;
	}
}


/* extract a groupname and return its gid.
 * rgerhards, 2007-07-17
 */
static rsRetVal doGetGID(uchar **pp, rsRetVal (*pSetHdlr)(void*, uid_t), void *pVal)
{
	struct group *pgBuf;
	struct group gBuf;
	DEFiRet;
	uchar szName[256];
	char stringBuf[2048];	/* I hope this is large enough... */

	assert(pp != NULL);
	assert(*pp != NULL);

	if(getSubString(pp, (char*) szName, sizeof(szName) / sizeof(uchar), ' ')  != 0) {
		errmsg.LogError(NO_ERRCODE, "could not extract group name");
		ABORT_FINALIZE(RS_RET_NOT_FOUND);
	}

	getgrnam_r((char*)szName, &gBuf, stringBuf, sizeof(stringBuf), &pgBuf);

	if(pgBuf == NULL) {
		errmsg.LogError(NO_ERRCODE, "ID for group '%s' could not be found or error", (char*)szName);
		iRet = RS_RET_NOT_FOUND;
	} else {
		if(pSetHdlr == NULL) {
			/* we should set value directly to var */
			*((gid_t*)pVal) = pgBuf->gr_gid;
		} else {
			/* we set value via a set function */
			CHKiRet(pSetHdlr(pVal, pgBuf->gr_gid));
		}
		dbgprintf("gid %d obtained for group '%s'\n", pgBuf->gr_gid, szName);
	}

	skipWhiteSpace(pp); /* skip over any whitespace */

finalize_it:
	RETiRet;
}


/* extract a username and return its uid.
 * rgerhards, 2007-07-17
 */
static rsRetVal doGetUID(uchar **pp, rsRetVal (*pSetHdlr)(void*, uid_t), void *pVal)
{
	struct passwd *ppwBuf;
	struct passwd pwBuf;
	DEFiRet;
	uchar szName[256];
	char stringBuf[2048];	/* I hope this is large enough... */

	assert(pp != NULL);
	assert(*pp != NULL);

	if(getSubString(pp, (char*) szName, sizeof(szName) / sizeof(uchar), ' ')  != 0) {
		errmsg.LogError(NO_ERRCODE, "could not extract user name");
		ABORT_FINALIZE(RS_RET_NOT_FOUND);
	}

	getpwnam_r((char*)szName, &pwBuf, stringBuf, sizeof(stringBuf), &ppwBuf);

	if(ppwBuf == NULL) {
		errmsg.LogError(NO_ERRCODE, "ID for user '%s' could not be found or error", (char*)szName);
		iRet = RS_RET_NOT_FOUND;
	} else {
		if(pSetHdlr == NULL) {
			/* we should set value directly to var */
			*((uid_t*)pVal) = ppwBuf->pw_uid;
		} else {
			/* we set value via a set function */
			CHKiRet(pSetHdlr(pVal, ppwBuf->pw_uid));
		}
		dbgprintf("uid %d obtained for user '%s'\n", ppwBuf->pw_uid, szName);
	}

	skipWhiteSpace(pp); /* skip over any whitespace */

finalize_it:
	RETiRet;
}


/* Parse and process an binary cofig option. pVal must be
 * a pointer to an integer which is to receive the option
 * value.
 * rgerhards, 2007-07-15
 */
static rsRetVal doBinaryOptionLine(uchar **pp, rsRetVal (*pSetHdlr)(void*, int), void *pVal)
{
	int iOption;
	DEFiRet;

	assert(pp != NULL);
	assert(*pp != NULL);

	if((iOption = doParseOnOffOption(pp)) == -1)
		return RS_RET_ERR;	/* nothing left to do */
	
	if(pSetHdlr == NULL) {
		/* we should set value directly to var */
		*((int*)pVal) = iOption;
	} else {
		/* we set value via a set function */
		CHKiRet(pSetHdlr(pVal, iOption));
	}

	skipWhiteSpace(pp); /* skip over any whitespace */

finalize_it:
	RETiRet;
}


/* parse a whitespace-delimited word from the provided string. This is a
 * helper function for a number of syntaxes. The parsed value is returned
 * in ppStrB (which must be provided by caller).
 * rgerhards, 2008-02-14
 */
static rsRetVal
getWord(uchar **pp, cstr_t **ppStrB)
{
	DEFiRet;
	uchar *p;

	ASSERT(pp != NULL);
	ASSERT(*pp != NULL);
	ASSERT(ppStrB != NULL);

	CHKiRet(rsCStrConstruct(ppStrB));

	/* parse out the word */
	p = *pp;

	while(*p && !isspace((int) *p)) {
		CHKiRet(rsCStrAppendChar(*ppStrB, *p++));
	}
	CHKiRet(rsCStrFinish(*ppStrB));

	*pp = p;

finalize_it:
	RETiRet;
}


/* Parse and a word config line option. A word is a consequtive
 * sequence of non-whitespace characters. pVal must be
 * a pointer to a string which is to receive the option
 * value. The returned string must be freed by the caller.
 * rgerhards, 2007-09-07
 * To facilitate multiple instances of the same command line
 * directive, doGetWord() now checks if pVal is already a
 * non-NULL pointer. If so, we assume it was created by a previous
 * incarnation and is automatically freed. This happens only when
 * no custom handler is defined. If it is, the customer handler
 * must do the cleanup. I have checked and this was al also memory
 * leak with some code. Obviously, not a large one. -- rgerhards, 2007-12-20
 * Just to clarify: if pVal is parsed to a custom handler, this handler
 * is responsible for freeing pVal. -- rgerhards, 2008-03-20
 */
static rsRetVal doGetWord(uchar **pp, rsRetVal (*pSetHdlr)(void*, uchar*), void *pVal)
{
	DEFiRet;
	cstr_t *pStrB;
	uchar *pNewVal;

	ASSERT(pp != NULL);
	ASSERT(*pp != NULL);

	CHKiRet(getWord(pp, &pStrB));
	CHKiRet(rsCStrConvSzStrAndDestruct(pStrB, &pNewVal, 0));
	pStrB = NULL;

	/* we got the word, now set it */
	if(pSetHdlr == NULL) {
		/* we should set value directly to var */
		if(*((uchar**)pVal) != NULL)
			free(*((uchar**)pVal)); /* free previous entry */
		*((uchar**)pVal) = pNewVal; /* set new one */
	} else {
		/* we set value via a set function */
		CHKiRet(pSetHdlr(pVal, pNewVal));
	}

	skipWhiteSpace(pp); /* skip over any whitespace */

finalize_it:
	if(iRet != RS_RET_OK) {
		if(pStrB != NULL)
			rsCStrDestruct(&pStrB);
	}

	RETiRet;
}


/* parse a syslog name from the string. This is the generic code that is
 * called by the facility/severity functions. Note that we do not check the
 * validity of numerical values, something that should probably change over
 * time (TODO). -- rgerhards, 2008-02-14
 */
static rsRetVal
doSyslogName(uchar **pp, rsRetVal (*pSetHdlr)(void*, int), void *pVal, syslogName_t *pNameTable)
{
	DEFiRet;
	cstr_t *pStrB;
	int iNewVal;

	ASSERT(pp != NULL);
	ASSERT(*pp != NULL);

	CHKiRet(getWord(pp, &pStrB)); /* get word */
	iNewVal = decodeSyslogName(rsCStrGetSzStr(pStrB), pNameTable);

	if(pSetHdlr == NULL) {
		/* we should set value directly to var */
		*((int*)pVal) = iNewVal; /* set new one */
	} else {
		/* we set value via a set function */
		CHKiRet(pSetHdlr(pVal, iNewVal));
	}

	skipWhiteSpace(pp); /* skip over any whitespace */

finalize_it:
	if(pStrB != NULL)
		rsCStrDestruct(&pStrB);

	RETiRet;
}


/* Implements the facility syntax.
 * rgerhards, 2008-02-14
 */
static rsRetVal
doFacility(uchar **pp, rsRetVal (*pSetHdlr)(void*, int), void *pVal)
{
	DEFiRet;
	iRet = doSyslogName(pp, pSetHdlr, pVal, syslogFacNames);
	RETiRet;
}


/* Implements the severity syntax.
 * rgerhards, 2008-02-14
 */
static rsRetVal
doSeverity(uchar **pp, rsRetVal (*pSetHdlr)(void*, int), void *pVal)
{
	DEFiRet;
	iRet = doSyslogName(pp, pSetHdlr, pVal, syslogPriNames);
	RETiRet;
}


/* --------------- END functions for handling canned syntaxes --------------- */

/* destructor for cslCmdHdlr
 * pThis is actually a cslCmdHdlr_t, but we do not cast it as all we currently
 * need to do is free it.
 */
static rsRetVal cslchDestruct(void *pThis)
{
	ASSERT(pThis != NULL);
	free(pThis);
	
	return RS_RET_OK;
}


/* constructor for cslCmdHdlr
 */
static rsRetVal cslchConstruct(cslCmdHdlr_t **ppThis)
{
	cslCmdHdlr_t *pThis;
	DEFiRet;

	assert(ppThis != NULL);
	if((pThis = calloc(1, sizeof(cslCmdHdlr_t))) == NULL) {
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}

finalize_it:
	*ppThis = pThis;
	RETiRet;
}

/* destructor for linked list keys. As we do not use any dynamic memory,
 * we simply return. However, this entry point must be defined for the 
 * linkedList class to make sure we have not forgotten a destructor.
 * rgerhards, 2007-11-21
 */
static rsRetVal cslchKeyDestruct(void __attribute__((unused)) *pData)
{
	return RS_RET_OK;
}


/* Key compare operation for linked list class. This compares two
 * owner cookies (void *).
 * rgerhards, 2007-11-21
 */
static int cslchKeyCompare(void *pKey1, void *pKey2)
{
	if(pKey1 == pKey2)
		return 0;
	else
		if(pKey1 < pKey2)
			return -1;
		else
			return 1;
}


/* set data members for this object
 */
rsRetVal cslchSetEntry(cslCmdHdlr_t *pThis, ecslCmdHdrlType eType, rsRetVal (*pHdlr)(), void *pData)
{
	assert(pThis != NULL);
	assert(eType != eCmdHdlrInvalid);

	pThis->eType = eType;
	pThis->cslCmdHdlr = pHdlr;
	pThis->pData = pData;

	return RS_RET_OK;
}


/* call the specified handler
 */
static rsRetVal cslchCallHdlr(cslCmdHdlr_t *pThis, uchar **ppConfLine)
{
	DEFiRet;
	rsRetVal (*pHdlr)() = NULL;
	assert(pThis != NULL);
	assert(ppConfLine != NULL);

	switch(pThis->eType) {
	case eCmdHdlrCustomHandler:
		pHdlr = doCustomHdlr;
		break;
	case eCmdHdlrUID:
		pHdlr = doGetUID;
		break;
	case eCmdHdlrGID:
		pHdlr = doGetGID;
		break;
	case eCmdHdlrBinary:
		pHdlr = doBinaryOptionLine;
		break;
	case eCmdHdlrFileCreateMode:
		pHdlr = doFileCreateMode;
		break;
	case eCmdHdlrInt:
		pHdlr = doGetInt;
		break;
	case eCmdHdlrSize:
		pHdlr = doGetSize;
		break;
	case eCmdHdlrGetChar:
		pHdlr = doGetChar;
		break;
	case eCmdHdlrFacility:
		pHdlr = doFacility;
		break;
	case eCmdHdlrSeverity:
		pHdlr = doSeverity;
		break;
	case eCmdHdlrGetWord:
		pHdlr = doGetWord;
		break;
	default:
		iRet = RS_RET_NOT_IMPLEMENTED;
		goto finalize_it;
	}

	/* we got a pointer to the handler, so let's call it */
	assert(pHdlr != NULL);
	CHKiRet(pHdlr(ppConfLine, pThis->cslCmdHdlr, pThis->pData));

finalize_it:
	RETiRet;
}


/* ---------------------------------------------------------------------- *
 * now come the handlers for cslCmd_t
 * ---------------------------------------------------------------------- */

/* destructor for a cslCmd list key (a string as of now)
 */
static rsRetVal cslcKeyDestruct(void *pData)
{
	free(pData); /* we do not need to cast as all we do is free it anyway... */
	return RS_RET_OK;
}

/* destructor for cslCmd
 */
static rsRetVal cslcDestruct(void *pData)
{
	cslCmd_t *pThis = (cslCmd_t*) pData;

	assert(pThis != NULL);

	llDestroy(&pThis->llCmdHdlrs);
	free(pThis);
	
	return RS_RET_OK;
}


/* constructor for cslCmd
 */
static rsRetVal cslcConstruct(cslCmd_t **ppThis, int bChainingPermitted)
{
	cslCmd_t *pThis;
	DEFiRet;

	assert(ppThis != NULL);
	if((pThis = calloc(1, sizeof(cslCmd_t))) == NULL) {
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}

	pThis->bChainingPermitted = bChainingPermitted;

	CHKiRet(llInit(&pThis->llCmdHdlrs, cslchDestruct, cslchKeyDestruct, cslchKeyCompare));

finalize_it:
	*ppThis = pThis;
	RETiRet;
}


/* add a handler entry to a known command
 */
static rsRetVal cslcAddHdlr(cslCmd_t *pThis, ecslCmdHdrlType eType, rsRetVal (*pHdlr)(), void *pData, void *pOwnerCookie)
{
	DEFiRet;
	cslCmdHdlr_t *pCmdHdlr = NULL;

	assert(pThis != NULL);

	CHKiRet(cslchConstruct(&pCmdHdlr));
	CHKiRet(cslchSetEntry(pCmdHdlr, eType, pHdlr, pData));
	CHKiRet(llAppend(&pThis->llCmdHdlrs, pOwnerCookie, pCmdHdlr));

finalize_it:
	if(iRet != RS_RET_OK) {
		if(pHdlr != NULL)
			cslchDestruct(pCmdHdlr);
	}

	RETiRet;
}


/* function that registers cfsysline handlers.
 * The supplied pCmdName is copied and a new buffer is allocated. This
 * buffer is automatically destroyed when the element is freed, the
 * caller does not need to take care of that. The caller must, however,
 * free pCmdName if he allocated it dynamically! -- rgerhards, 2007-08-09
 */
rsRetVal regCfSysLineHdlr(uchar *pCmdName, int bChainingPermitted, ecslCmdHdrlType eType, rsRetVal (*pHdlr)(), void *pData,
			  void *pOwnerCookie)
{
	DEFiRet;
	cslCmd_t *pThis;
	uchar *pMyCmdName;

	iRet = llFind(&llCmdList, (void *) pCmdName, (void*) &pThis);
	if(iRet == RS_RET_NOT_FOUND) {
		/* new command */
		CHKiRet(cslcConstruct(&pThis, bChainingPermitted));
		CHKiRet_Hdlr(cslcAddHdlr(pThis, eType, pHdlr, pData, pOwnerCookie)) {
			cslcDestruct(pThis);
			goto finalize_it;
		}
		/* important: add to list, AFTER everything else is OK. Else
		 * we mess up things in the error case.
		 */
		if((pMyCmdName = (uchar*) strdup((char*)pCmdName)) == NULL) {
			cslcDestruct(pThis);
			ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
		}
		CHKiRet_Hdlr(llAppend(&llCmdList, pMyCmdName, (void*) pThis)) {
			cslcDestruct(pThis);
			goto finalize_it;
		}
	} else {
		/* command already exists, are we allowed to chain? */
		if(pThis->bChainingPermitted == 0 || bChainingPermitted == 0) {
			ABORT_FINALIZE(RS_RET_CHAIN_NOT_PERMITTED);
		}
		CHKiRet_Hdlr(cslcAddHdlr(pThis, eType, pHdlr, pData, pOwnerCookie)) {
			cslcDestruct(pThis);
			goto finalize_it;
		}
	}

finalize_it:
	RETiRet;
}


rsRetVal unregCfSysLineHdlrs(void)
{
	return llDestroy(&llCmdList);
}


/* helper function for unregCfSysLineHdlrs4Owner(). This is used to see if there is
 * a handler of this owner inside the element and, if so, remove it. Please note that
 * it keeps track of a pointer to the last linked list entry, as this is needed to
 * remove an entry from the list.
 * rgerhards, 2007-11-21
 */
DEFFUNC_llExecFunc(unregHdlrsHeadExec)
{
	DEFiRet;
	cslCmd_t *pListHdr = (cslCmd_t*) pData;
	int iNumElts;

	/* first find element */
	iRet = llFindAndDelete(&(pListHdr->llCmdHdlrs), pParam);

	/* now go back and check how many elements are left */
	CHKiRet(llGetNumElts(&(pListHdr->llCmdHdlrs), &iNumElts));

	if(iNumElts == 0) {
		/* nothing left in header, so request to delete it */
		iRet = RS_RET_OK_DELETE_LISTENTRY;
	}

finalize_it:
	RETiRet;
}
/* unregister and destroy cfSysLineHandlers for a specific owner. This method is
 * most importantly used before unloading a loadable module providing some handlers.
 * The full list of handlers is searched. If the to-be removed handler was the only
 * handler for a directive name, the directive header, too, is deleted.
 * rgerhards, 2007-11-21
 */
rsRetVal unregCfSysLineHdlrs4Owner(void *pOwnerCookie)
{
	DEFiRet;
	/* we need to walk through all directive names, as the linked list
	 * class does not provide a way to just search the lower-level handlers.
	 */
	iRet = llExecFunc(&llCmdList, unregHdlrsHeadExec, pOwnerCookie);

	RETiRet;
}


/* process a cfsysline command (based on handler structure)
 * param "p" is a pointer to the command line after the command. Should be
 * updated.
 */
rsRetVal processCfSysLineCommand(uchar *pCmdName, uchar **p)
{
	DEFiRet;
	rsRetVal iRetLL; /* for linked list handling */
	cslCmd_t *pCmd;
	cslCmdHdlr_t *pCmdHdlr;
	linkedListCookie_t llCookieCmdHdlr;
	uchar *pHdlrP; /* the handler's private p (else we could only call one handler) */
	int bWasOnceOK; /* was the result of an handler at least once RS_RET_OK? */
	uchar *pOKp = NULL; /* returned conf line pointer when it was OK */

	iRet = llFind(&llCmdList, (void *) pCmdName, (void*) &pCmd);

	if(iRet == RS_RET_NOT_FOUND) {
		errmsg.LogError(NO_ERRCODE, "invalid or yet-unknown config file command - have you forgotten to load a module?");
	}

	if(iRet != RS_RET_OK)
		goto finalize_it;

	llCookieCmdHdlr = NULL;
	bWasOnceOK = 0;
	while((iRetLL = llGetNextElt(&pCmd->llCmdHdlrs, &llCookieCmdHdlr, (void*)&pCmdHdlr)) == RS_RET_OK) {
		/* for the time being, we ignore errors during handlers. The
		 * reason is that handlers are independent. An error in one
		 * handler does not necessarily mean that another one will
		 * fail, too. Later, we might add a config variable to control
		 * this behaviour (but I am not sure if that is rally
		 * necessary). -- rgerhards, 2007-07-31
		 */
		pHdlrP = *p;
		if((iRet = cslchCallHdlr(pCmdHdlr, &pHdlrP)) == RS_RET_OK) {
			bWasOnceOK = 1;
			pOKp = pHdlrP;
		}
	}

	if(bWasOnceOK == 1) {
		*p = pOKp;
		iRet = RS_RET_OK;
	}

	if(iRetLL != RS_RET_END_OF_LINKEDLIST)
		iRet = iRetLL;

finalize_it:
	RETiRet;
}


/* debug print the command handler structure
 */
void dbgPrintCfSysLineHandlers(void)
{
	DEFiRet;

	cslCmd_t *pCmd;
	cslCmdHdlr_t *pCmdHdlr;
	linkedListCookie_t llCookieCmd;
	linkedListCookie_t llCookieCmdHdlr;
	uchar *pKey;

	dbgprintf("Sytem Line Configuration Commands:\n");
	llCookieCmd = NULL;
	while((iRet = llGetNextElt(&llCmdList, &llCookieCmd, (void*)&pCmd)) == RS_RET_OK) {
		llGetKey(llCookieCmd, (void*) &pKey); /* TODO: using the cookie is NOT clean! */
		dbgprintf("\tCommand '%s':\n", pKey);
		llCookieCmdHdlr = NULL;
		while((iRet = llGetNextElt(&pCmd->llCmdHdlrs, &llCookieCmdHdlr, (void*)&pCmdHdlr)) == RS_RET_OK) {
			dbgprintf("\t\ttype : %d\n", pCmdHdlr->eType);
			dbgprintf("\t\tpData: 0x%lx\n", (unsigned long) pCmdHdlr->pData);
			dbgprintf("\t\tHdlr : 0x%lx\n", (unsigned long) pCmdHdlr->cslCmdHdlr);
			dbgprintf("\t\tOwner: 0x%lx\n", (unsigned long) llCookieCmdHdlr->pKey);
			dbgprintf("\n");
		}
	}
	dbgprintf("\n");
	ENDfunc
}


/* our init function. TODO: remove once converted to a class
 */
rsRetVal cfsyslineInit()
{
	DEFiRet;
	CHKiRet(objGetObjInterface(&obj));
	CHKiRet(objUse(errmsg, CORE_COMPONENT));

	CHKiRet(llInit(&llCmdList, cslcDestruct, cslcKeyDestruct, strcasecmp));

finalize_it:
	RETiRet;
}

/* vim:set ai:
 */
