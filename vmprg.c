/* vmprg.c - abstracts a program (bytecode) for the rsyslog virtual machine
 *
 * Module begun 2008-02-20 by Rainer Gerhards
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
#include "vmprg.h"

/* static data */
DEFobjStaticHelpers


/* Standard-Constructor
 */
BEGINobjConstruct(vmprg) /* be sure to specify the object type also in END macro! */
ENDobjConstruct(vmprg)


/* ConstructionFinalizer
 * rgerhards, 2008-01-09
 */
rsRetVal vmprgConstructFinalize(vmprg_t *pThis)
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
		vmopDestruct(&pTmp);
	}
ENDobjDestruct(vmprg)


/* add an operation (instruction) to the end of the current program. This
 * function is expected to be called while creating the program, but never
 * again after this is done and it is being executed. Results are undefined if
 * it is called after execution.
 */
rsRetVal
addOperation(vmprg_t *pThis, vmop_t *pOp)
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


/* Initialize the vmprg class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINObjClassInit(vmprg, 1) /* class, version */
	OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, vmprgConstructFinalize);
ENDObjClassInit(vmprg)

/* vi:set ai:
 */
