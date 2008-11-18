/* sysvar.c - imlements rsyslog system variables
 *
 * At least for now, this class only has static functions and no
 * instances.
 *
 * Module begun 2008-02-25 by Rainer Gerhards
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
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "rsyslog.h"
#include "obj.h"
#include "stringbuf.h"
#include "sysvar.h"
#include "datetime.h"

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(var)
DEFobjCurrIf(datetime)


/* Standard-Constructor
 */
BEGINobjConstruct(sysvar) /* be sure to specify the object type also in END macro! */
ENDobjConstruct(sysvar)


/* ConstructionFinalizer
 * rgerhards, 2008-01-09
 */
static rsRetVal
sysvarConstructFinalize(sysvar_t __attribute__((unused)) *pThis)
{
	DEFiRet;
	RETiRet;
}


/* destructor for the sysvar object */
BEGINobjDestruct(sysvar) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(sysvar)
ENDobjDestruct(sysvar)


/* This function returns the current date in different
 * variants. It is used to construct the $NOW series of
 * system properties. The returned buffer must be freed
 * by the caller when no longer needed. If the function
 * can not allocate memory, it returns a NULL pointer.
 * Added 2007-07-10 rgerhards
 * TODO: this was taken from msg.c and we should consolidate it with the code
 * there. This is especially important when we increase the number of system
 * variables (what we definitely want to do).
 */
typedef enum ENOWType { NOW_NOW, NOW_YEAR, NOW_MONTH, NOW_DAY, NOW_HOUR, NOW_MINUTE } eNOWType;
static rsRetVal
getNOW(eNOWType eNow, cstr_t **ppStr)
{
	DEFiRet;
	uchar szBuf[16];
	struct syslogTime t;

	datetime.getCurrTime(&t, NULL);
	switch(eNow) {
	case NOW_NOW:
		snprintf((char*) szBuf, sizeof(szBuf)/sizeof(uchar), "%4.4d-%2.2d-%2.2d", t.year, t.month, t.day);
		break;
	case NOW_YEAR:
		snprintf((char*) szBuf, sizeof(szBuf)/sizeof(uchar), "%4.4d", t.year);
		break;
	case NOW_MONTH:
		snprintf((char*) szBuf, sizeof(szBuf)/sizeof(uchar), "%2.2d", t.month);
		break;
	case NOW_DAY:
		snprintf((char*) szBuf, sizeof(szBuf)/sizeof(uchar), "%2.2d", t.day);
		break;
	case NOW_HOUR:
		snprintf((char*) szBuf, sizeof(szBuf)/sizeof(uchar), "%2.2d", t.hour);
		break;
	case NOW_MINUTE:
		snprintf((char*) szBuf, sizeof(szBuf)/sizeof(uchar), "%2.2d", t.minute);
		break;
	}

	/* now create a string object out of it and hand that over to the var */
	CHKiRet(rsCStrConstructFromszStr(ppStr, szBuf));

finalize_it:
	RETiRet;
}


/* The function returns a system variable suitable for use with RainerScript. Most importantly, this means
 * that the value is returned in a var_t object. The var_t is constructed inside this function and
 * MUST be freed by the caller.
 * rgerhards, 2008-02-25
 */
static rsRetVal
GetVar(cstr_t *pstrVarName, var_t **ppVar)
{
	DEFiRet;
	var_t *pVar;
	cstr_t *pstrProp;

	ASSERT(pstrVarName != NULL);
	ASSERT(ppVar != NULL);

	/* make sure we have a var_t instance */
	CHKiRet(var.Construct(&pVar));
	CHKiRet(var.ConstructFinalize(pVar));

	/* now begin the actual variable evaluation */
	if(!rsCStrSzStrCmp(pstrVarName, (uchar*)"now", sizeof("now") - 1)) {
		CHKiRet(getNOW(NOW_NOW, &pstrProp));
	} else if(!rsCStrSzStrCmp(pstrVarName, (uchar*)"year", sizeof("year") - 1)) {
		CHKiRet(getNOW(NOW_YEAR, &pstrProp));
	} else if(!rsCStrSzStrCmp(pstrVarName, (uchar*)"month", sizeof("month") - 1)) {
		CHKiRet(getNOW(NOW_MONTH, &pstrProp));
	} else if(!rsCStrSzStrCmp(pstrVarName, (uchar*)"day", sizeof("day") - 1)) {
		CHKiRet(getNOW(NOW_DAY, &pstrProp));
	} else if(!rsCStrSzStrCmp(pstrVarName, (uchar*)"hour", sizeof("hour") - 1)) {
		CHKiRet(getNOW(NOW_HOUR, &pstrProp));
	} else if(!rsCStrSzStrCmp(pstrVarName, (uchar*)"minute", sizeof("minute") - 1)) {
		CHKiRet(getNOW(NOW_MINUTE, &pstrProp));
	} else {
		ABORT_FINALIZE(RS_RET_SYSVAR_NOT_FOUND);
	}

	/* now hand the string over to the var object */
	CHKiRet(var.SetString(pVar, pstrProp));

	/* finally store var */
	*ppVar = pVar;

finalize_it:
	RETiRet;
}


/* queryInterface function
 * rgerhards, 2008-02-21
 */
BEGINobjQueryInterface(sysvar)
CODESTARTobjQueryInterface(sysvar)
	if(pIf->ifVersion != sysvarCURR_IF_VERSION) { /* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	//xxxpIf->oID = "sysvar";//OBJsysvar;

	pIf->Construct = sysvarConstruct;
	pIf->ConstructFinalize = sysvarConstructFinalize;
	pIf->Destruct = sysvarDestruct;
	pIf->GetVar = GetVar;
finalize_it:
ENDobjQueryInterface(sysvar)


/* Initialize the sysvar class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINObjClassInit(sysvar, 1, OBJ_IS_CORE_MODULE) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(var, CORE_COMPONENT));
	CHKiRet(objUse(datetime, CORE_COMPONENT));

	/* set our own handlers */
	OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, sysvarConstructFinalize);
ENDObjClassInit(sysvar)

/* vi:set ai:
 */
