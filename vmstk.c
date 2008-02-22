/* vmstk.c - the arithmetic stack of a virtual machine.
 *
 * Module begun 2008-02-21 by Rainer Gerhards
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
#include <assert.h>

#include "rsyslog.h"
#include "obj.h"
#include "vmstk.h"

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(var)


/* Standard-Constructor
 */
BEGINobjConstruct(vmstk) /* be sure to specify the object type also in END macro! */
ENDobjConstruct(vmstk)


/* ConstructionFinalizer
 * rgerhards, 2008-01-09
 */
static rsRetVal
vmstkConstructFinalize(vmstk_t __attribute__((unused)) *pThis)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, vmstk);
	RETiRet;
}


/* destructor for the vmstk object */
BEGINobjDestruct(vmstk) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(vmstk)
ENDobjDestruct(vmstk)


/* debugprint for the vmstk object */
BEGINobjDebugPrint(vmstk) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDebugPrint(vmstk)
	dbgoprint((obj_t*) pThis, "stack contents:\n");
ENDobjDebugPrint(vmstk)


/* push a value on the stack. The provided pVar is now owned
 * by the stack. If the user intends to continue use it, it
 * must be duplicated.
 */
static rsRetVal
push(vmstk_t *pThis, var_t *pVar)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, vmstk);
	ISOBJ_TYPE_assert(pVar, var);

	if(pThis->iStkPtr >= VMSTK_SIZE)
		ABORT_FINALIZE(RS_RET_OUT_OF_STACKSPACE);

	pThis->vStk[pThis->iStkPtr++] = pVar;

finalize_it:
	RETiRet;
}


/* pop a value from the stack
 * The user is responsible for destructing the ppVar returned.
 */
static rsRetVal
pop(vmstk_t *pThis, var_t **ppVar)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, vmstk);
	assert(ppVar != NULL);

	if(pThis->iStkPtr == 0)
		ABORT_FINALIZE(RS_RET_STACK_EMPTY);

	*ppVar = pThis->vStk[pThis->iStkPtr--];

finalize_it:
	RETiRet;
}


/* queryInterface function
 * rgerhards, 2008-02-21
 */
BEGINobjQueryInterface(vmstk)
CODESTARTobjQueryInterface(vmstk)
	if(pIf->ifVersion != vmstkCURR_IF_VERSION) { /* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->oID = OBJvmstk;

	pIf->Construct = vmstkConstruct;
	pIf->ConstructFinalize = vmstkConstructFinalize;
	pIf->Destruct = vmstkDestruct;
	pIf->DebugPrint = vmstkDebugPrint;
	pIf->Push = push;
	pIf->Pop = pop;
finalize_it:
ENDobjQueryInterface(vmstk)


/* Initialize the vmstk class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINObjClassInit(vmstk, 1) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(var));

	/* set our own handlers */
	OBJSetMethodHandler(objMethod_DEBUGPRINT, vmstkDebugPrint);
	OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, vmstkConstructFinalize);
ENDObjClassInit(vmstk)

/* vi:set ai:
 */
