/* vmop.c - abstracts an operation (instructed) supported by the
 * rsyslog virtual machine
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
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "rsyslog.h"
#include "obj.h"
#include "vmop.h"

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(var)


/* forward definitions */
static rsRetVal vmopOpcode2Str(vmop_t *pThis, uchar **ppName);

/* Standard-Constructor
 */
BEGINobjConstruct(vmop) /* be sure to specify the object type also in END macro! */
ENDobjConstruct(vmop)


/* ConstructionFinalizer
 * rgerhards, 2008-01-09
 */
rsRetVal vmopConstructFinalize(vmop_t __attribute__((unused)) *pThis)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, vmop);
	RETiRet;
}


/* destructor for the vmop object */
BEGINobjDestruct(vmop) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(vmop)
	if(pThis->operand.pVar != NULL)
		var.Destruct(&pThis->operand.pVar);
ENDobjDestruct(vmop)


/* DebugPrint support for the vmop object */
BEGINobjDebugPrint(vmop) /* be sure to specify the object type also in END and CODESTART macros! */
	uchar *pOpcodeName;
	cstr_t *pStrVar;
CODESTARTobjDebugPrint(vmop)
	vmopOpcode2Str(pThis, &pOpcodeName);
	CHKiRet(rsCStrConstruct(&pStrVar));
	CHKiRet(rsCStrFinish(&pStrVar));
	if(pThis->operand.pVar != NULL) {
		CHKiRet(var.Obj2Str(pThis->operand.pVar, pStrVar));
	}
	dbgoprint((obj_t*) pThis, "%.12s\t%s\n", pOpcodeName, rsCStrGetSzStrNoNULL(pStrVar));
	rsCStrDestruct(&pStrVar);
finalize_it:
ENDobjDebugPrint(vmop)


/* This function is similar to DebugPrint, but does not send its output to
 * the debug log but instead to a caller-provided string. The idea here is that
 * we can use this string to get a textual representation of an operation.
 * Among others, this is useful for creating testbenches, our first use case for
 * it. Here, it enables simple comparison of the resulting program to a
 * reference program by simple string compare.
 * Note that the caller must initialize the string object. We always add
 * data to it. So, it can be easily combined into a chain of methods
 * to generate the final string.
 * rgerhards, 2008-07-04
 */
static rsRetVal
Obj2Str(vmop_t *pThis, cstr_t *pstrPrg)
{
	uchar *pOpcodeName;
	uchar szBuf[2048];
	size_t lenBuf;
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, vmop);
	assert(pstrPrg != NULL);
	vmopOpcode2Str(pThis, &pOpcodeName);
	lenBuf = snprintf((char*) szBuf, sizeof(szBuf), "%s\t", pOpcodeName);
	CHKiRet(rsCStrAppendStrWithLen(pstrPrg, szBuf, lenBuf));
	if(pThis->operand.pVar != NULL)
		CHKiRet(var.Obj2Str(pThis->operand.pVar, pstrPrg));
	CHKiRet(rsCStrAppendChar(pstrPrg, '\n'));

finalize_it:
	RETiRet;
}


/* set operand (variant case)
 * rgerhards, 2008-02-20
 */
static rsRetVal
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
static rsRetVal
vmopSetOpcode(vmop_t *pThis, opcode_t opcode)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, vmop);
	pThis->opcode = opcode;
	RETiRet;
}


/* a way to turn an opcode into a readable string
 */
static rsRetVal
vmopOpcode2Str(vmop_t *pThis, uchar **ppName)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, vmop);

	switch(pThis->opcode) {
		case opcode_OR:
			*ppName = (uchar*) "or";
			break;
		case opcode_AND:
			*ppName = (uchar*) "and";
			break;
		case opcode_PLUS:
			*ppName = (uchar*) "add";
			break;
		case opcode_MINUS:
			*ppName = (uchar*) "sub";
			break;
		case opcode_TIMES:
			*ppName = (uchar*) "mul";
			break;
		case opcode_DIV:
			*ppName = (uchar*) "div";
			break;
		case opcode_MOD:
			*ppName = (uchar*) "mod";
			break;
		case opcode_NOT:
			*ppName = (uchar*) "not";
			break;
		case opcode_CMP_EQ:
			*ppName = (uchar*) "cmp_==";
			break;
		case opcode_CMP_NEQ:
			*ppName = (uchar*) "cmp_!=";
			break;
		case opcode_CMP_LT:
			*ppName = (uchar*) "cmp_<";
			break;
		case opcode_CMP_GT:
			*ppName = (uchar*) "cmp_>";
			break;
		case opcode_CMP_LTEQ:
			*ppName = (uchar*) "cmp_<=";
			break;
		case opcode_CMP_CONTAINS:
			*ppName = (uchar*) "contains";
			break;
		case opcode_CMP_STARTSWITH:
			*ppName = (uchar*) "startswith";
			break;
		case opcode_CMP_GTEQ:
			*ppName = (uchar*) "cmp_>=";
			break;
		case opcode_PUSHSYSVAR:
			*ppName = (uchar*) "push_sysvar";
			break;
		case opcode_PUSHMSGVAR:
			*ppName = (uchar*) "push_msgvar";
			break;
		case opcode_PUSHCONSTANT:
			*ppName = (uchar*) "push_const";
			break;
		case opcode_POP:
			*ppName = (uchar*) "pop";
			break;
		case opcode_UNARY_MINUS:
			*ppName = (uchar*) "unary_minus";
			break;
		case opcode_STRADD:
			*ppName = (uchar*) "strconcat";
			break;
		case opcode_FUNC_CALL:
			*ppName = (uchar*) "func_call";
			break;
		default:
			*ppName = (uchar*) "!invalid_opcode!";
			break;
	}

	RETiRet;
}


/* queryInterface function
 * rgerhards, 2008-02-21
 */
BEGINobjQueryInterface(vmop)
CODESTARTobjQueryInterface(vmop)
	if(pIf->ifVersion != vmopCURR_IF_VERSION) { /* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->Construct = vmopConstruct;
	pIf->ConstructFinalize = vmopConstructFinalize;
	pIf->Destruct = vmopDestruct;
	pIf->DebugPrint = vmopDebugPrint;
	pIf->SetOpcode = vmopSetOpcode;
	pIf->SetVar = vmopSetVar;
	pIf->Opcode2Str = vmopOpcode2Str;
	pIf->Obj2Str = Obj2Str;
finalize_it:
ENDobjQueryInterface(vmop)


/* Initialize the vmop class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINObjClassInit(vmop, 1, OBJ_IS_CORE_MODULE) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(var, CORE_COMPONENT));

	OBJSetMethodHandler(objMethod_DEBUGPRINT, vmopDebugPrint);
	OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, vmopConstructFinalize);
ENDObjClassInit(vmop)

/* vi:set ai:
 */
