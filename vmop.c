/* vmop.c - abstracts an operation (instructed) supported by the
 * rsyslog virtual machine
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
#include "vmop.h"

/* static data */
DEFobjStaticHelpers


/* Standard-Constructor
 */
BEGINobjConstruct(vmop) /* be sure to specify the object type also in END macro! */
ENDobjConstruct(vmop)


/* ConstructionFinalizer
 * rgerhards, 2008-01-09
 */
rsRetVal vmopConstructFinalize(vmop_t *pThis)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, vmop);
	RETiRet;
}


/* destructor for the vmop object */
BEGINobjDestruct(vmop) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(vmop)
ENDobjDestruct(vmop)


/* set operand (variant case)
 * rgerhards, 2008-02-20
 */
rsRetVal
vmopSetVar(vmop_t *pThis, var_t *pVar)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, vmop);
	ISOBJ_TYPE_assert(pVar, var);
	pThis->operand.pVar = pVar;
	RETiRet;
}


/* set operation
 * rgerhards, 2008-02-20
 */
rsRetVal
vmopSetOpcode(vmop_t *pThis, opcode_t opcode)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, vmop);
	pThis->opcode = opcode;
	RETiRet;
}


/* Initialize the vmop class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINObjClassInit(vmop, 1) /* class, version */
	OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, vmopConstructFinalize);
ENDObjClassInit(vmop)

/* vi:set ai:
 */
