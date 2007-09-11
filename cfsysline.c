/* cfsysline.c
 * Implementation of the configuration system line object.
 *
 * File begun on 2007-07-30 by RGerhards
 *
 * Copyright 2007 Rainer Gerhards and Adiscon GmbH.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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
#include "srUtils.h"


/* static data */
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
		logerror("No character available");
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
	return iRet;
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
	return iRet;
}


/* Parse a number from the configuration line.
 * rgerhards, 2007-07-31
 */
static rsRetVal doGetInt(uchar **pp, rsRetVal (*pSetHdlr)(void*, uid_t), void *pVal)
{
	uchar *p;
	DEFiRet;
	int i;	

	assert(pp != NULL);
	assert(*pp != NULL);
	
	skipWhiteSpace(pp); /* skip over any whitespace */
	p = *pp;

	if(!isdigit((int) *p)) {
		errno = 0;
		logerror("invalid number");
		ABORT_FINALIZE(RS_RET_INVALID_INT);
	}

	/* pull value */
	for(i = 0 ; *p && isdigit((int) *p) ; ++p)
		i = i * 10 + *p - '0';

	if(pSetHdlr == NULL) {
		/* we should set value directly to var */
		*((int*)pVal) = i;
	} else {
		/* we set value via a set function */
		CHKiRet(pSetHdlr(pVal, i));
	}

	*pp = p;

finalize_it:
	return iRet;
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
		logerror((char*) errMsg);
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
	return iRet;
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
		logerror("Invalid $-configline - could not extract on/off option");
		return -1;
	}
	
	if(!strcmp((char*)szOpt, "on")) {
		return 1;
	} else if(!strcmp((char*)szOpt, "off")) {
		return 0;
	} else {
		logerrorSz("Option value must be on or off, but is '%s'", (char*)pOptStart);
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
		logerror("could not extract group name");
		ABORT_FINALIZE(RS_RET_NOT_FOUND);
	}

	getgrnam_r((char*)szName, &gBuf, stringBuf, sizeof(stringBuf), &pgBuf);

	if(pgBuf == NULL) {
		logerrorSz("ID for group '%s' could not be found or error", (char*)szName);
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
	return iRet;
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
		logerror("could not extract user name");
		ABORT_FINALIZE(RS_RET_NOT_FOUND);
	}

	getpwnam_r((char*)szName, &pwBuf, stringBuf, sizeof(stringBuf), &ppwBuf);

	if(ppwBuf == NULL) {
		logerrorSz("ID for user '%s' could not be found or error", (char*)szName);
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
	return iRet;
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
	return iRet;
}


/* Parse and a word config line option. A word is a consequitive
 * sequence of non-whitespace characters. pVal must be
 * a pointer to a string which is to receive the option
 * value. The returned string must be freed by the caller.
 * rgerhards, 2007-09-07
 */
static rsRetVal doGetWord(uchar **pp, rsRetVal (*pSetHdlr)(void*, uchar*), void *pVal)
{
	DEFiRet;
	rsCStrObj *pStrB;
	uchar *p;
	uchar *pNewVal;

	assert(pp != NULL);
	assert(*pp != NULL);

	if((pStrB = rsCStrConstruct()) == NULL) 
		return RS_RET_OUT_OF_MEMORY;

	/* parse out the word */
	p = *pp;

	while(*p && !isspace((int) *p)) {
		CHKiRet(rsCStrAppendChar(pStrB, *p++));
	}
	CHKiRet(rsCStrFinish(pStrB));

	CHKiRet(rsCStrConvSzStrAndDestruct(pStrB, &pNewVal, 0));

	/* we got the word, now set it */
	if(pSetHdlr == NULL) {
		/* we should set value directly to var */
		*((uchar**)pVal) = pNewVal;
	} else {
		/* we set value via a set function */
		CHKiRet(pSetHdlr(pVal, pNewVal));
	}

	*pp = p;
	skipWhiteSpace(pp); /* skip over any whitespace */

finalize_it:
	if(iRet != RS_RET_OK) {
		if(pStrB != NULL)
			rsCStrDestruct(pStrB);
	}

	return iRet;
}


/* --------------- END functions for handling canned syntaxes --------------- */

/* destructor for cslCmdHdlr
 * pThis is actually a cslCmdHdlr_t, but we do not cast it as all we currently
 * need to do is free it.
 */
static rsRetVal cslchDestruct(void *pThis)
{
	assert(pThis != NULL);
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
	return iRet;
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
	case eCmdHdlrGetChar:
		pHdlr = doGetChar;
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
	return iRet;
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

	CHKiRet(llInit(&pThis->llCmdHdlrs, cslchDestruct, NULL, NULL));

finalize_it:
	*ppThis = pThis;
	return iRet;
}


/* add a handler entry to a known command
 */
static rsRetVal cslcAddHdlr(cslCmd_t *pThis, ecslCmdHdrlType eType, rsRetVal (*pHdlr)(), void *pData)
{
	DEFiRet;
	cslCmdHdlr_t *pCmdHdlr = NULL;

	assert(pThis != NULL);

	CHKiRet(cslchConstruct(&pCmdHdlr));
	CHKiRet(cslchSetEntry(pCmdHdlr, eType, pHdlr, pData));
	CHKiRet(llAppend(&pThis->llCmdHdlrs, NULL, pCmdHdlr));

finalize_it:
	if(iRet != RS_RET_OK) {
		if(pHdlr != NULL)
			cslchDestruct(pCmdHdlr);
	}

	return iRet;
}


/* function that initializes this module here. This is primarily a hook
 * for syslogd.
 */
rsRetVal cfsyslineInit(void)
{
	DEFiRet;

	CHKiRet(llInit(&llCmdList, cslcDestruct, cslcKeyDestruct, strcasecmp));

finalize_it:
	return iRet;
}


/* function that registers cfsysline handlers.
 * The supplied pCmdName is copied and a new buffer is allocated. This
 * buffer is automatically destroyed when the element is freed, the
 * caller does not need to take care of that. The caller must, however,
 * free pCmdName if he allocated it dynamically! -- rgerhards, 2007-08-09
 */
rsRetVal regCfSysLineHdlr(uchar *pCmdName, int bChainingPermitted, ecslCmdHdrlType eType, rsRetVal (*pHdlr)(), void *pData)
{
	cslCmd_t *pThis;
	uchar *pMyCmdName;
	DEFiRet;

	iRet = llFind(&llCmdList, (void *) pCmdName, (void*) &pThis);
	if(iRet == RS_RET_NOT_FOUND) {
		/* new command */
		CHKiRet(cslcConstruct(&pThis, bChainingPermitted));
		CHKiRet_Hdlr(cslcAddHdlr(pThis, eType, pHdlr, pData)) {
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
		CHKiRet_Hdlr(cslcAddHdlr(pThis, eType, pHdlr, pData)) {
			cslcDestruct(pThis);
			goto finalize_it;
		}
	}

finalize_it:
	return iRet;
}


rsRetVal unregCfSysLineHdlrs(void)
{
	return llDestroy(&llCmdList);
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
		logerror("invalid or yet-unknown config file command - have you forgotten to load a module?");
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
	return iRet;
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

	printf("\nSytem Line Configuration Commands:\n");
	llCookieCmd = NULL;
	while((iRet = llGetNextElt(&llCmdList, &llCookieCmd, (void*)&pCmd)) == RS_RET_OK) {
		llGetKey(llCookieCmd, (void*) &pKey); /* TODO: using the cookie is NOT clean! */
		printf("\tCommand '%s':\n", pKey);
		llCookieCmdHdlr = NULL;
		while((iRet = llGetNextElt(&pCmd->llCmdHdlrs, &llCookieCmdHdlr, (void*)&pCmdHdlr)) == RS_RET_OK) {
			printf("\t\ttype : %d\n", pCmdHdlr->eType);
			printf("\t\tpData: 0x%x\n", (unsigned) pCmdHdlr->pData);
			printf("\t\tHdlr : 0x%x\n", (unsigned) pCmdHdlr->cslCmdHdlr);
			printf("\n");
		}
	}
	printf("\n");

}

/*
 * vi:set ai:
 */
