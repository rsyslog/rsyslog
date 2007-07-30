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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <pwd.h>

#include "rsyslog.h"
#include "syslogd.h" /* TODO: when the module interface & library design is done, this should be able to go away */
#include "cfsysline.h"
#include "srUtils.h"


/* static data */
cslCmd_t *pCmdListRoot = NULL; /* The list of known configuration commands. */
cslCmd_t *pCmdListLast = NULL;

/* --------------- START functions for handling canned syntaxes --------------- */

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


/* extract a username and return its uid.
 * rgerhards, 2007-07-17
 */
rsRetVal doGetUID(uchar **pp, rsRetVal (*pSetHdlr)(void*, uid_t), void *pVal)
{
	struct passwd *ppwBuf;
	struct passwd pwBuf;
	rsRetVal iRet = RS_RET_OK;
	uchar szName[256];
	char stringBuf[2048];	/* I hope this is large enough... */

	assert(pp != NULL);
	assert(*pp != NULL);
	assert(pVal != NULL);

	if(getSubString(pp, (char*) szName, sizeof(szName) / sizeof(uchar), ' ')  != 0) {
		logerror("could not extract user name");
		iRet = RS_RET_NOT_FOUND;
		goto finalize_it;
	}

	getpwnam_r((char*)szName, &pwBuf, stringBuf, sizeof(stringBuf), &ppwBuf);

	if(ppwBuf == NULL) {
		logerrorSz("ID for user '%s' could not be found or error", (char*)szName);
	} else {
		if(pSetHdlr == NULL) {
			/* we should set value directly to var */
			*((uid_t*)pVal) = ppwBuf->pw_uid;
		} else {
			/* we set value via a set function */
			pSetHdlr(pVal, ppwBuf->pw_uid);
		}
		dprintf("uid %d obtained for user '%s'\n", ppwBuf->pw_uid, szName);
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
rsRetVal doBinaryOptionLine(uchar **pp, rsRetVal (*pSetHdlr)(void*, int), void *pVal)
{
	int iOption;
	rsRetVal iRet = RS_RET_OK;

	assert(pp != NULL);
	assert(*pp != NULL);
	assert(pVal != NULL);

	if((iOption = doParseOnOffOption(pp)) == -1)
		return RS_RET_ERR;	/* nothing left to do */
	
	if(pSetHdlr == NULL) {
		/* we should set value directly to var */
		*((int*)pVal) = iOption;
	} else {
		/* we set value via a set function */
		pSetHdlr(pVal, iOption);
	}

	skipWhiteSpace(pp); /* skip over any whitespace */
	return iRet;
}


/* --------------- END functions for handling canned syntaxes --------------- */

/* destructor for cslCmdHdlr
 */
rsRetVal cslchDestruct(cslCmdHdlr_t *pThis)
{
	assert(pThis != NULL);
	free(pThis);
	
	return RS_RET_OK;
}


/* constructor for cslCmdHdlr
 */
rsRetVal cslchConstruct(cslCmdHdlr_t **ppThis)
{
	cslCmdHdlr_t *pThis;
	rsRetVal iRet = RS_RET_OK;

	assert(ppThis != NULL);
	if((pThis = calloc(1, sizeof(cslCmdHdlr_t))) == NULL) {
		iRet = RS_RET_OUT_OF_MEMORY;
		goto abort_it;
	}

abort_it:
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
rsRetVal cslchCallHdlr(cslCmdHdlr_t *pThis, uchar **ppConfLine)
{
	assert(pThis != NULL);
	assert(ppConfLine != NULL);


	return RS_RET_OK;
}

/* ---------------------------------------------------------------------- *
 * now come the handlers for cslCmd_t
 * ---------------------------------------------------------------------- */

/* destructor for cslCmd
 */
rsRetVal cslcDestruct(cslCmd_t *pThis)
{
	cslCmdHdlr_t *pHdlr;
	cslCmdHdlr_t *pHdlrPrev;
	assert(pThis != NULL);

	for(pHdlr = pThis->pHdlrRoot ; pHdlr != NULL ; pHdlr = pHdlrPrev->pNext) {
		pHdlrPrev = pHdlr; /* else we get a segfault */
		cslchDestruct(pHdlr);
	}

	free(pThis->pszCmdString);
	free(pThis);
	
	return RS_RET_OK;
}


/* constructor for cslCmdHdlr
 */
rsRetVal cslcConstruct(cslCmd_t **ppThis)
{
	cslCmd_t *pThis;
	rsRetVal iRet = RS_RET_OK;

	assert(ppThis != NULL);
	if((pThis = calloc(1, sizeof(cslCmd_t))) == NULL) {
		iRet = RS_RET_OUT_OF_MEMORY;
		goto abort_it;
	}

abort_it:
	*ppThis = pThis;
	return iRet;
}
/*
 * vi:set ai:
 */
