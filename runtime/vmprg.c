/* vmprg.c - abstracts a program (bytecode) for the rsyslog virtual machine
 *
 * Module begun 2008-02-20 by Rainer Gerhards
 *
 * Copyright 2007, 2008 Rainer Gerhards and Adiscon GmbH.
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

#include "rsyslog.h"
#include "obj.h"
#include "vmprg.h"

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(vmop)


/* Standard-Constructor
 */
BEGINobjConstruct(vmprg) /* be sure to specify the object type also in END macro! */
ENDobjConstruct(vmprg)


/* ConstructionFinalizer
 * rgerhards, 2008-01-09
 */
static rsRetVal
vmprgConstructFinalize(vmprg_t __attribute__((unused)) *pThis)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, vmprg);
	RETiRet;
}


/* destructor for the vmprg object */
BEGINobjDestruct(vmprg) /* be sure to specify the object type also in END and CODESTART macros! */
	vmop_t *pOp;
	vmop_t *pTmp;
CODESTARTobjDestruct(vmprg)
	/* we need to destruct the program elements! */
	for(pOp = pThis->vmopRoot ; pOp != NULL ; ) {
		pTmp = pOp;
		pOp = pOp->pNext;
		vmop.Destruct(&pTmp);
	}
ENDobjDestruct(vmprg)


/* destructor for the vmop object */
BEGINobjDebugPrint(vmprg) /* be sure to specify the object type also in END and CODESTART macros! */
	vmop_t *pOp;
CODESTARTobjDebugPrint(vmprg)
	dbgoprint((obj_t*) pThis, "program contents:\n");
	for(pOp = pThis->vmopRoot ; pOp != NULL ; pOp = pOp->pNext) {
		vmop.DebugPrint(pOp);
	}
ENDobjDebugPrint(vmprg)


/* add an operation (instruction) to the end of the current program. This
 * function is expected to be called while creating the program, but never
 * again after this is done and it is being executed. Results are undefined if
 * it is called after execution.
 */
static rsRetVal
vmprgAddOperation(vmprg_t *pThis, vmop_t *pOp)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, vmprg);
	ISOBJ_TYPE_assert(pOp, vmop);

	if(pThis->vmopRoot == NULL) {
		pThis->vmopRoot = pOp;
	} else {
		pThis->vmopLast->pNext = pOp;
	}
	pThis->vmopLast = pOp;

	RETiRet;
}


/* this is a shortcut for high-level callers. It creates a new vmop, sets its
 * parameters and adds it to the program - all in one big step. If there is no
 * var associated with this operation, the caller can simply supply NULL as
 * pVar.
 */
static rsRetVal
vmprgAddVarOperation(vmprg_t *pThis, opcode_t opcode, var_t *pVar)
{
	DEFiRet;
	vmop_t *pOp;

	ISOBJ_TYPE_assert(pThis, vmprg);

	/* construct and fill vmop */
	CHKiRet(vmop.Construct(&pOp));
	CHKiRet(vmop.ConstructFinalize(pOp));
	CHKiRet(vmop.ConstructFinalize(pOp));
	CHKiRet(vmop.SetOpcode(pOp, opcode));
	if(pVar != NULL)
		CHKiRet(vmop.SetVar(pOp, pVar));

	/* and add it to the program */
	CHKiRet(vmprgAddOperation(pThis, pOp));

finalize_it:
	RETiRet;
}


/* queryInterface function
 * rgerhards, 2008-02-21
 */
BEGINobjQueryInterface(vmprg)
CODESTARTobjQueryInterface(vmprg)
	if(pIf->ifVersion != vmprgCURR_IF_VERSION) { /* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	//xxxpIf->oID = OBJvmprg;

	pIf->Construct = vmprgConstruct;
	pIf->ConstructFinalize = vmprgConstructFinalize;
	pIf->Destruct = vmprgDestruct;
	pIf->DebugPrint = vmprgDebugPrint;
	pIf->AddOperation = vmprgAddOperation;
	pIf->AddVarOperation = vmprgAddVarOperation;
finalize_it:
ENDobjQueryInterface(vmprg)


/* Initialize the vmprg class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINObjClassInit(vmprg, 1, OBJ_IS_CORE_MODULE) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(vmop, CORE_COMPONENT));

	/* set our own handlers */
	OBJSetMethodHandler(objMethod_DEBUGPRINT, vmprgDebugPrint);
	OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, vmprgConstructFinalize);
ENDObjClassInit(vmprg)

/* vi:set ai:
 */
