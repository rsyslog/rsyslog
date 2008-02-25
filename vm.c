/* vm.c - the arithmetic stack of a virtual machine.
 *
 * Module begun 2008-02-22 by Rainer Gerhards
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
#include "vm.h"
#include "stringbuf.h"

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(vmstk)
DEFobjCurrIf(var)


/* ------------------------------ instruction set implementation ------------------------------ *
 * The following functions implement the VM's instruction set.
 */
#define BEGINop(instruction) \
	static rsRetVal op##instruction(vm_t *pThis, __attribute__((unused)) vmop_t *pOp) \
	{ \
		DEFiRet;

#define CODESTARTop(instruction) \
		ISOBJ_TYPE_assert(pThis, vm);

#define PUSHRESULTop(operand, res) \
	/* we have a result, so let's push it */ \
	var.SetNumber(operand, res); \
	vmstk.Push(pThis->pStk, operand); /* result */

#define ENDop(instruction) \
		RETiRet; \
	}

/* code generator for boolean operations */
#define BOOLOP(name, OPERATION) \
BEGINop(name) /* remember to set the instruction also in the ENDop macro! */ \
	var_t *operand1; \
	var_t *operand2; \
CODESTARTop(name) \
	vmstk.PopBool(pThis->pStk, &operand1); \
	vmstk.PopBool(pThis->pStk, &operand2); \
	if(operand1->val.num OPERATION operand2->val.num) { \
		CHKiRet(var.SetNumber(operand1, 1)); \
	} else { \
		CHKiRet(var.SetNumber(operand1, 0)); \
	} \
	vmstk.Push(pThis->pStk, operand1); /* result */ \
	var.Destruct(&operand2); /* no longer needed */ \
finalize_it: \
ENDop(name)
BOOLOP(OR,  ||)
BOOLOP(AND, &&)
#undef BOOLOP


/* code generator for numerical operations */
#define BOOLOP(name, OPERATION) \
BEGINop(name) /* remember to set the instruction also in the ENDop macro! */ \
	var_t *operand1; \
	var_t *operand2; \
CODESTARTop(name) \
	vmstk.PopNumber(pThis->pStk, &operand1); \
	vmstk.PopNumber(pThis->pStk, &operand2); \
	operand1->val.num = operand1->val.num OPERATION operand2->val.num; \
RUNLOG_VAR("%lld", operand1->val.num); \
	vmstk.Push(pThis->pStk, operand1); /* result */ \
	var.Destruct(&operand2); /* no longer needed */ \
ENDop(name)
BOOLOP(PLUS,   +)
BOOLOP(MINUS,  -)
BOOLOP(TIMES,  *)
BOOLOP(DIV,    /)
BOOLOP(MOD,    %)
#undef BOOLOP


/* code generator for compare operations */
#define BEGINCMPOP(name) \
BEGINop(name) \
	var_t *operand1; \
	var_t *operand2; \
	number_t bRes; \
CODESTARTop(name) \
	CHKiRet(vmstk.Pop2CommOp(pThis->pStk, &operand1, &operand2)); \
	/* data types are equal (so we look only at operand1), but we must \
	 * check which type we have to deal with... \
	 */ \
	switch(operand1->varType) {
#define ENDCMPOP(name) \
		default: \
			bRes = 0; /* we do not abort just so that we have a value. TODO: reconsider */ \
			break; \
	} \
 \
	/* we have a result, so let's push it */ \
RUNLOG_VAR("%lld", bRes); \
	var.SetNumber(operand1, bRes); \
	vmstk.Push(pThis->pStk, operand1); /* result */ \
	var.Destruct(&operand2); /* no longer needed */ \
finalize_it: \
ENDop(name)

BEGINCMPOP(CMP_EQ) /* remember to change the name also in the END macro! */
	case VARTYPE_NUMBER:
		bRes = operand1->val.num == operand2->val.num;
		break;
	case VARTYPE_STR:
		bRes = !rsCStrCStrCmp(operand1->val.pStr, operand2->val.pStr);
		break;
ENDCMPOP(CMP_EQ)

BEGINCMPOP(CMP_NEQ) /* remember to change the name also in the END macro! */
	case VARTYPE_NUMBER:
		bRes = operand1->val.num != operand2->val.num;
		break;
	case VARTYPE_STR:
		bRes = rsCStrCStrCmp(operand1->val.pStr, operand2->val.pStr);
		break;
ENDCMPOP(CMP_EQ)

BEGINCMPOP(CMP_LT) /* remember to change the name also in the END macro! */
	case VARTYPE_NUMBER:
		bRes = operand1->val.num < operand2->val.num;
		break;
	case VARTYPE_STR:
		bRes = rsCStrCStrCmp(operand1->val.pStr, operand2->val.pStr) < 0;
		break;
ENDCMPOP(CMP_LT)

BEGINCMPOP(CMP_GT) /* remember to change the name also in the END macro! */
	case VARTYPE_NUMBER:
		bRes = operand1->val.num > operand2->val.num;
		break;
	case VARTYPE_STR:
		bRes = rsCStrCStrCmp(operand1->val.pStr, operand2->val.pStr) > 0;
		break;
ENDCMPOP(CMP_GT)

BEGINCMPOP(CMP_LTEQ) /* remember to change the name also in the END macro! */
	case VARTYPE_NUMBER:
		bRes = operand1->val.num <= operand2->val.num;
		break;
	case VARTYPE_STR:
		bRes = rsCStrCStrCmp(operand1->val.pStr, operand2->val.pStr) <= 0;
		break;
ENDCMPOP(CMP_LTEQ)

BEGINCMPOP(CMP_GTEQ) /* remember to change the name also in the END macro! */
	case VARTYPE_NUMBER:
		bRes = operand1->val.num >= operand2->val.num;
		break;
	case VARTYPE_STR:
		bRes = rsCStrCStrCmp(operand1->val.pStr, operand2->val.pStr) >= 0;
		break;
ENDCMPOP(CMP_GTEQ)

#undef BEGINCMPOP
#undef ENDCMPOP
/* end regular compare operations */

/* comare operations that work on strings, only */
BEGINop(CMP_CONTAINS) /* remember to set the instruction also in the ENDop macro! */
	var_t *operand1;
	var_t *operand2;
	int bRes;
CODESTARTop(CMP_CONTAINS)
	/* operand2 is on top of stack, so needs to be popped first */
	vmstk.PopString(pThis->pStk, &operand2);
	vmstk.PopString(pThis->pStk, &operand1);
	/* TODO: extend cstr class so that it supports location of cstr inside cstr */
	bRes = (rsCStrLocateInSzStr(operand2->val.pStr, rsCStrGetSzStr(operand1->val.pStr)) == -1) ? 0 : 1;

	/* we have a result, so let's push it */
	PUSHRESULTop(operand1, bRes);
	var.Destruct(&operand2); /* no longer needed */
ENDop(CMP_CONTAINS)

/* end comare operations that work on strings, only */

BEGINop(NOT) /* remember to set the instruction also in the ENDop macro! */
	var_t *operand;
CODESTARTop(NOT)
	vmstk.PopBool(pThis->pStk, &operand);
	PUSHRESULTop(operand, !operand->val.num);
ENDop(NOT)

BEGINop(UNARY_MINUS) /* remember to set the instruction also in the ENDop macro! */
	var_t *operand;
CODESTARTop(UNARY_MINUS)
	vmstk.PopNumber(pThis->pStk, &operand);
	PUSHRESULTop(operand, -operand->val.num);
ENDop(UNARY_MINUS)


BEGINop(PUSHCONSTANT) /* remember to set the instruction also in the ENDop macro! */
CODESTARTop(PUSHCONSTANT)
	vmstk.Push(pThis->pStk, pOp->operand.pVar);
ENDop(PUSHCONSTANT)


/* ------------------------------ end instruction set implementation ------------------------------ */


/* Standard-Constructor
 */
BEGINobjConstruct(vm) /* be sure to specify the object type also in END macro! */
ENDobjConstruct(vm)


/* ConstructionFinalizer
 * rgerhards, 2008-01-09
 */
static rsRetVal
vmConstructFinalize(vm_t __attribute__((unused)) *pThis)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, vm);
	
	CHKiRet(vmstk.Construct(&pThis->pStk));
	CHKiRet(vmstk.ConstructFinalize(pThis->pStk));

finalize_it:
	RETiRet;
}


/* destructor for the vm object */
BEGINobjDestruct(vm) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(vm)
	if(pThis->pStk != NULL)
		vmstk.Destruct(&pThis->pStk);
ENDobjDestruct(vm)


/* debugprint for the vm object */
BEGINobjDebugPrint(vm) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDebugPrint(vm)
	dbgoprint((obj_t*) pThis, "rsyslog virtual machine, currently no state info available\n");
ENDobjDebugPrint(vm)


/* execute a program
 */
static rsRetVal
execProg(vm_t *pThis, vmprg_t *pProg)
{
	DEFiRet;
	vmop_t *pCurrOp; /* virtual instruction pointer */

	ISOBJ_TYPE_assert(pThis, vm);
	ISOBJ_TYPE_assert(pProg, vmprg);

#define doOP(OP) case opcode_##OP: CHKiRet(op##OP(pThis, pCurrOp)); break
	pCurrOp = pProg->vmopRoot; /* TODO: do this via a method! */
	while(pCurrOp != NULL && pCurrOp->opcode != opcode_END_PROG) {
		switch(pCurrOp->opcode) {
			doOP(OR);
			doOP(AND);
			doOP(CMP_EQ);
			doOP(CMP_NEQ);
			doOP(CMP_LT);
			doOP(CMP_GT);
			doOP(CMP_LTEQ);
			doOP(CMP_GTEQ);
			doOP(CMP_CONTAINS);
			// TODO: implement: doOP(CMP_STARTSWITH);
			doOP(NOT);
			doOP(PUSHCONSTANT);
			doOP(PLUS);
			doOP(MINUS);
			doOP(TIMES);
			doOP(DIV);
			doOP(MOD);
			doOP(UNARY_MINUS);
			default:
				ABORT_FINALIZE(RS_RET_INVALID_VMOP);
				dbgoprint((obj_t*) pThis, "invalid instruction %d in vmprg\n", pCurrOp->opcode);
				break;
		}
		/* so far, we have plain sequential execution, so on to next... */
		pCurrOp = pCurrOp->pNext;
	}
#undef doOP

	/* if we reach this point, our program has intintionally terminated
	 * (no error state).
	 */

finalize_it:
	RETiRet;
}


/* Pop a boolean from the stack and return it to caller. This functionality is
 * partly needed. We may (or may not ;)) be able to remove it once we have
 * full RainerScript support. -- rgerhards, 2008-02-25
 */
static rsRetVal
PopBoolFromStack(vm_t *pThis, var_t **ppVar)
{
	DEFiRet;

	CHKiRet(vmstk.PopBool(pThis->pStk, ppVar));

finalize_it:
	RETiRet;
}

/* queryInterface function
 * rgerhards, 2008-02-21
 */
BEGINobjQueryInterface(vm)
CODESTARTobjQueryInterface(vm)
	if(pIf->ifVersion != vmCURR_IF_VERSION) { /* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->oID = OBJvm;

	pIf->Construct = vmConstruct;
	pIf->ConstructFinalize = vmConstructFinalize;
	pIf->Destruct = vmDestruct;
	pIf->DebugPrint = vmDebugPrint;
	pIf->ExecProg = execProg;
	pIf->PopBoolFromStack = PopBoolFromStack;
finalize_it:
ENDobjQueryInterface(vm)


/* Initialize the vm class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINObjClassInit(vm, 1) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(vmstk));
	CHKiRet(objUse(var));

	/* set our own handlers */
	OBJSetMethodHandler(objMethod_DEBUGPRINT, vmDebugPrint);
	OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, vmConstructFinalize);
ENDObjClassInit(vm)

/* vi:set ai:
 */
