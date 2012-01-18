/* sysvar.c - imlements rsyslog system variables
 *
 * At least for now, this class only has static functions and no
 * instances.
 *
 * Module begun 2008-02-25 by Rainer Gerhards
 *
 * Copyright (C) 2008-2012 Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
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
DEFobjCurrIf(glbl)


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
	} else if(!rsCStrSzStrCmp(pstrVarName, (uchar*)"myhostname", sizeof("myhostname") - 1)) {
		CHKiRet(rsCStrConstructFromszStr(&pstrProp, glbl.GetLocalHostName()));
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
	CHKiRet(objUse(glbl, CORE_COMPONENT));

	/* set our own handlers */
	OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, sysvarConstructFinalize);
ENDObjClassInit(sysvar)

/* vi:set ai:
 */
