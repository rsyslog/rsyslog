/* prop.c - rsyslog's prop object
 *
 * This object is meant to support message properties that are stored
 * seperately from the message. The main intent is to support properties
 * that are "constant" during a period of time, so that many messages may
 * contain a reference to the same property. It is important, though, that
 * properties are destroyed when they are no longer needed.
 *
 * Please note that this is a performance-critical part of the software and
 * as such we may use some methods in here which do not look elegant, but
 * which are fast...
 *
 * Module begun 2009-06-17 by Rainer Gerhards
 *
 * Copyright 2009 Rainer Gerhards and Adiscon GmbH.
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
#include <assert.h>
#include <string.h>

#include "rsyslog.h"
#include "obj.h"
#include "obj-types.h"
#include "atomic.h"
#include "prop.h"

/* static data */
DEFobjStaticHelpers


/* Standard-Constructor
 */
BEGINobjConstruct(prop) /* be sure to specify the object type also in END macro! */
	pThis->iRefCount = 1;
ENDobjConstruct(prop)

/* set string, we make our own private copy! This MUST only be called BEFORE
 * ConstructFinalize()!
 */
static rsRetVal SetString(prop_t *pThis, uchar *psz, int len)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, prop);
	if(pThis->len >= CONF_PROP_BUFSIZE)
		free(pThis->szVal.psz);
	pThis->len = len;
	if(len < CONF_PROP_BUFSIZE) {
		memcpy(pThis->szVal.sz, psz, len + 1);
	} else {
		CHKmalloc(pThis->szVal.psz = malloc(len + 1));
		memcpy(pThis->szVal.psz, psz, len + 1);
	}

finalize_it:
	RETiRet;
}


/* get string length */
static int GetStringLen(prop_t *pThis)
{
	return pThis->len;
}


/* get string */
static rsRetVal GetString(prop_t *pThis, uchar **ppsz, int *plen)
{
	BEGINfunc
	ISOBJ_TYPE_assert(pThis, prop);
	if(pThis->len < CONF_PROP_BUFSIZE) {
		*ppsz = pThis->szVal.sz;
RUNLOG;
	} else {
		*ppsz = pThis->szVal.psz;
RUNLOG;
	}
	*plen = pThis->len;
	ENDfunc
	return RS_RET_OK;
}


/* ConstructionFinalizer
 * rgerhards, 2008-01-09
 */
static rsRetVal
propConstructFinalize(prop_t __attribute__((unused)) *pThis)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, prop);
	RETiRet;
}


/* add a new reference. It is VERY IMPORTANT to call this function whenever
 * the property is handed over to some entitiy that later call Destruct() on it.
 */
static rsRetVal AddRef(prop_t *pThis)
{
	ATOMIC_INC(pThis->iRefCount);
	return RS_RET_OK;
}


/* destructor for the prop object */
BEGINobjDestruct(prop) /* be sure to specify the object type also in END and CODESTART macros! */
	int currRefCount;
CODESTARTobjDestruct(prop)
	currRefCount = ATOMIC_DEC_AND_FETCH(pThis->iRefCount);
	if(currRefCount == 0) {
		/* (only) in this case we need to actually destruct the object */
		if(pThis->len >= CONF_PROP_BUFSIZE)
			free(pThis->szVal.psz);
	} else {
		pThis = NULL; /* tell framework NOT to destructing the object! */
	}
ENDobjDestruct(prop)


/* debugprint for the prop object */
BEGINobjDebugPrint(prop) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDebugPrint(prop)
	dbgprintf("prop object %p - no further debug info implemented\n", pThis);
ENDobjDebugPrint(prop)


/* queryInterface function
 * rgerhards, 2008-02-21
 */
BEGINobjQueryInterface(prop)
CODESTARTobjQueryInterface(prop)
	if(pIf->ifVersion != propCURR_IF_VERSION) { /* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->Construct = propConstruct;
	pIf->ConstructFinalize = propConstructFinalize;
	pIf->Destruct = propDestruct;
	pIf->DebugPrint = propDebugPrint;
	pIf->SetString = SetString;
	pIf->GetString = GetString;
	pIf->GetStringLen = GetStringLen;
	pIf->AddRef = AddRef;

finalize_it:
ENDobjQueryInterface(prop)


/* Exit the prop class.
 * rgerhards, 2009-04-06
 */
BEGINObjClassExit(prop, OBJ_IS_CORE_MODULE) /* class, version */
//	objRelease(errmsg, CORE_COMPONENT);
ENDObjClassExit(prop)


/* Initialize the prop class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINObjClassInit(prop, 1, OBJ_IS_CORE_MODULE) /* class, version */
	/* request objects we use */
//	CHKiRet(objUse(errmsg, CORE_COMPONENT));

	/* set our own handlers */
	OBJSetMethodHandler(objMethod_DEBUGPRINT, propDebugPrint);
	OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, propConstructFinalize);
ENDObjClassInit(prop)

/* vi:set ai:
 */
