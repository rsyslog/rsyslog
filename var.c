/* var.c - a typeless variable class
 *
 * This class is used to represent variable values, which may have any type.
 * Among others, it will be used inside rsyslog's expression system, but
 * also internally at any place where a typeless variable is needed.
 *
 * Module begun 2008-02-20 by Rainer Gerhards, with some code taken
 * from the obj.c/.h files.
 *
 * Copyright 2007, 2008 Rainer Gerhards and Adiscon GmbH.
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
#include <assert.h>

#include "rsyslog.h"
#include "obj.h"
#include "var.h"

/* static data */
DEFobjStaticHelpers


/* Standard-Constructor
 */
BEGINobjConstruct(var) /* be sure to specify the object type also in END macro! */
ENDobjConstruct(var)


/* ConstructionFinalizer
 * rgerhards, 2008-01-09
 */
rsRetVal varConstructFinalize(var_t __attribute__((unused)) *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, var);

	RETiRet;
}


/* destructor for the var object */
BEGINobjDestruct(var) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(var)
	if(pThis->pcsName != NULL)
		d_free(pThis->pcsName);
	if(pThis->varType == VARTYPE_CSTR) {
		if(pThis->val.vpCStr != NULL)
			d_free(pThis->val.vpCStr);
	}

ENDobjDestruct(var)


/* DebugPrint support for the var object */
BEGINobjDebugPrint(var) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDebugPrint(var)
	switch(pThis->varType) {
		case VARTYPE_CSTR:
			dbgoprint((obj_t*) pThis, "type: cstr, val '%s'\n", rsCStrGetSzStr(pThis->val.vpCStr));
			break;
		default:
			dbgoprint((obj_t*) pThis, "type %d currently not suppored in debug output\n", pThis->varType);
			break;
	}
ENDobjDebugPrint(var)


/* free the current values (destructs objects if necessary)
 */
static rsRetVal
varUnsetValues(var_t *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, var);
	if(pThis->varType == VARTYPE_CSTR)
		rsCStrDestruct(&pThis->val.vpCStr);

	pThis->varType = VARTYPE_NONE;

	RETiRet;
}


/* set a string value */
static rsRetVal
varSetString(var_t *pThis, cstr_t *pCStr)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, var);

	CHKiRet(varUnsetValues(pThis));
	pThis->varType = VARTYPE_CSTR;
	pThis->val.vpCStr = pCStr;

finalize_it:
	RETiRet;
}


/* set an int64 value */
static rsRetVal
varSetInt64(var_t *pThis, int64 iVal)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, var);

	CHKiRet(varUnsetValues(pThis));
	pThis->varType = VARTYPE_INT64;
	pThis->val.vInt64 = iVal;

finalize_it:
	RETiRet;
}


/* queryInterface function
 * rgerhards, 2008-02-21
 */
BEGINobjQueryInterface(var)
CODESTARTobjQueryInterface(var)
	if(pIf->ifVersion != varCURR_IF_VERSION) { /* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->oID = OBJvar;

	pIf->Construct = varConstruct;
	pIf->ConstructFinalize = varConstructFinalize;
	pIf->Destruct = varDestruct;
	pIf->DebugPrint = varDebugPrint;
	pIf->SetInt64 = varSetInt64;
	pIf->SetString = varSetString;
finalize_it:
ENDobjQueryInterface(var)


/* Initialize the var class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINObjClassInit(var, 1) /* class, version */
	OBJSetMethodHandler(objMethod_DEBUGPRINT, varDebugPrint);
	OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, varConstructFinalize);
ENDObjClassInit(var)

/* vi:set ai:
 */
