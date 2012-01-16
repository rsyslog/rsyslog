/* vmprg.c - abstracts a program (bytecode) for the rsyslog virtual machine
 *
 * Module begun 2008-02-20 by Rainer Gerhards
 *
 * Copyright 2007-2012 Rainer Gerhards and Adiscon GmbH.
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
#include "vmprg.h"
#include "stringbuf.h"

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
	dbgoprint((obj_t*) pThis, "VM Program:\n");
	for(pOp = pThis->vmopRoot ; pOp != NULL ; pOp = pOp->pNext) {
		vmop.DebugPrint(pOp);
	}
ENDobjDebugPrint(vmprg)


/* This function is similar to DebugPrint, but does not send its output to
 * the debug log but instead to a caller-provided string. The idea here is that
 * we can use this string to get a textual representation of a bytecode program. 
 * Among others, this is useful for creating testbenches, our first use case for
 * it. Here, it enables simple comparison of the resulting program to a
 * reference program by simple string compare.
 * Note that the caller must initialize the string object. We always add
 * data to it. So, it can be easily combined into a chain of methods
 * to generate the final string.
 * rgerhards, 2008-07-04
 */
static rsRetVal
Obj2Str(vmprg_t *pThis, cstr_t *pstrPrg)
{
	uchar szAddr[12];
	vmop_t *pOp;
	int i;
	int lenAddr;
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, vmprg);
	assert(pstrPrg != NULL);
	i = 0;	/* "program counter" */
	for(pOp = pThis->vmopRoot ; pOp != NULL ; pOp = pOp->pNext) {
		lenAddr = snprintf((char*)szAddr, sizeof(szAddr), "%8.8d: ", i++);
		CHKiRet(rsCStrAppendStrWithLen(pstrPrg, szAddr, lenAddr));
		vmop.Obj2Str(pOp, pstrPrg);
	}

finalize_it:
	RETiRet;
}


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
	CHKiRet(vmop.SetOpcode(pOp, opcode));
	if(pVar != NULL)
		CHKiRet(vmop.SetVar(pOp, pVar));

	/* and add it to the program */
	CHKiRet(vmprgAddOperation(pThis, pOp));

finalize_it:
	RETiRet;
}


/* this is another shortcut for high-level callers. It is similar to vmprgAddVarOperation
 * but adds a call operation. Among others, this include a check if the function
 * is known.
 */
static rsRetVal
vmprgAddCallOperation(vmprg_t *pThis, cstr_t *pcsName)
{
	DEFiRet;
	vmop_t *pOp;

	ISOBJ_TYPE_assert(pThis, vmprg);

	/* construct and fill vmop */
	CHKiRet(vmop.Construct(&pOp));
	CHKiRet(vmop.ConstructFinalize(pOp));
	CHKiRet(vmop.SetFunc(pOp, pcsName));
	CHKiRet(vmop.SetOpcode(pOp, opcode_FUNC_CALL));

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
	pIf->Construct = vmprgConstruct;
	pIf->ConstructFinalize = vmprgConstructFinalize;
	pIf->Destruct = vmprgDestruct;
	pIf->DebugPrint = vmprgDebugPrint;
	pIf->Obj2Str = Obj2Str;
	pIf->AddOperation = vmprgAddOperation;
	pIf->AddVarOperation = vmprgAddVarOperation;
	pIf->AddCallOperation = vmprgAddCallOperation;
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
