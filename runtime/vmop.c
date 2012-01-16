/* vmop.c - abstracts an operation (instructed) supported by the
 * rsyslog virtual machine
 *
 * Module begun 2008-02-20 by Rainer Gerhards
 *
 * Copyright 2007-2012 Adiscon GmbH.
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
#include "vmop.h"
#include "vm.h"

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(var)
DEFobjCurrIf(vm)


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
	if(pThis->opcode != opcode_FUNC_CALL) {
		if(pThis->operand.pVar != NULL)
			var.Destruct(&pThis->operand.pVar);
	}
ENDobjDestruct(vmop)


/* DebugPrint support for the vmop object */
BEGINobjDebugPrint(vmop) /* be sure to specify the object type also in END and CODESTART macros! */
	uchar *pOpcodeName;
	cstr_t *pStrVar;
CODESTARTobjDebugPrint(vmop)
	vmopOpcode2Str(pThis, &pOpcodeName);
	if(pThis->opcode == opcode_FUNC_CALL) {
		CHKiRet(vm.FindRSFunctionName(pThis->operand.rsf, &pStrVar));
		assert(pStrVar != NULL);
	} else {
		CHKiRet(rsCStrConstruct(&pStrVar));
		if(pThis->operand.pVar != NULL) {
			CHKiRet(var.Obj2Str(pThis->operand.pVar, pStrVar));
		}
	}
	CHKiRet(cstrFinalize(pStrVar));
	dbgoprint((obj_t*) pThis, "%.12s\t%s\n", pOpcodeName, rsCStrGetSzStrNoNULL(pStrVar));
	if(pThis->opcode != opcode_FUNC_CALL)
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
	cstr_t *pcsFuncName;
	uchar szBuf[2048];
	size_t lenBuf;
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, vmop);
	assert(pstrPrg != NULL);
	vmopOpcode2Str(pThis, &pOpcodeName);
	lenBuf = snprintf((char*) szBuf, sizeof(szBuf), "%s\t", pOpcodeName);
	CHKiRet(rsCStrAppendStrWithLen(pstrPrg, szBuf, lenBuf));
	if(pThis->opcode == opcode_FUNC_CALL) {
		CHKiRet(vm.FindRSFunctionName(pThis->operand.rsf, &pcsFuncName));
		CHKiRet(rsCStrAppendCStr(pstrPrg, pcsFuncName));
	} else {
		if(pThis->operand.pVar != NULL)
			CHKiRet(var.Obj2Str(pThis->operand.pVar, pstrPrg));
	}
	CHKiRet(cstrAppendChar(pstrPrg, '\n'));

finalize_it:
	RETiRet;
}


/* set function
 * rgerhards, 2009-04-06
 */
static rsRetVal
vmopSetFunc(vmop_t *pThis, cstr_t *pcsFuncName)
{
	prsf_t rsf;	/* pointer to function */
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, vmop);
	CHKiRet(vm.FindRSFunction(pcsFuncName, &rsf)); /* check if function exists and obtain pointer to it */
	assert(rsf != NULL);	/* just double-check, would be very hard to find! */
	pThis->operand.rsf = rsf;
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
	pIf->SetFunc = vmopSetFunc;
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
	CHKiRet(objUse(vm, CORE_COMPONENT));

	OBJSetMethodHandler(objMethod_DEBUGPRINT, vmopDebugPrint);
	OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, vmopConstructFinalize);
ENDObjClassInit(vmop)

/* vi:set ai:
 */
