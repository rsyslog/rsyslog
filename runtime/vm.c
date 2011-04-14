/* vm.c - the arithmetic stack of a virtual machine.
 *
 * Module begun 2008-02-22 by Rainer Gerhards
 *
 * Copyright 2008 Rainer Gerhards and Adiscon GmbH.
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
#include <string.h>
#include <assert.h>
#include <ctype.h>

#include "rsyslog.h"
#include "obj.h"
#include "vm.h"
#include "sysvar.h"
#include "stringbuf.h"
#include "unicode-helper.h"

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(vmstk)
DEFobjCurrIf(var)
DEFobjCurrIf(sysvar)

static pthread_mutex_t mutGetenv; /* we need to make this global because otherwise we can not guarantee proper init! */

/* ------------------------------ function registry code and structures  ------------------------------ */

/* we maintain a registry of known functions */
/* currently, this is a singly-linked list, this shall become a binary
 * tree when we add the real call interface. So far, entries are added
 * at the root, only.
 */
typedef struct s_rsf_entry {
	cstr_t *pName;			/* function name */
	prsf_t rsf;	/* pointer to function code */
	struct s_rsf_entry *pNext;	/* Pointer to next element or NULL */
} rsf_entry_t;
rsf_entry_t *funcRegRoot = NULL;


/* add a function to the function registry.
 * The handed-over cstr_t* object must no longer be used by the caller.
 * A duplicate function name is an error.
 * rgerhards, 2009-04-06
 */
static rsRetVal
rsfrAddFunction(uchar *szName, prsf_t rsf)
{
	rsf_entry_t *pEntry;
	size_t lenName;
	DEFiRet;

	assert(szName != NULL);
	assert(rsf != NULL);

	/* first check if we have a duplicate name, with the current approach this means
	 * we need to go through the whole list.
	 */
	lenName = strlen((char*)szName);
	for(pEntry = funcRegRoot ; pEntry != NULL ; pEntry = pEntry->pNext)
		if(!rsCStrSzStrCmp(pEntry->pName, szName, lenName))
			ABORT_FINALIZE(RS_RET_DUP_FUNC_NAME);

	/* unique name, so add to head of list */
	CHKmalloc(pEntry = calloc(1, sizeof(rsf_entry_t)));
	CHKiRet(rsCStrConstructFromszStr(&pEntry->pName, szName));
	CHKiRet(cstrFinalize(pEntry->pName));
	pEntry->rsf = rsf;
	pEntry->pNext = funcRegRoot;
	funcRegRoot = pEntry;

finalize_it:
	if(iRet != RS_RET_OK && iRet != RS_RET_DUP_FUNC_NAME)
		free(pEntry);

	RETiRet;
}


/* find a function inside the function registry
 * The caller provides a cstr_t with the function name and receives
 * a function pointer back. If no function is found, an RS_RET_UNKNW_FUNC
 * error is returned. So if the function returns with RS_RET_OK, the caller
 * can savely assume the function pointer is valid.
 * rgerhards, 2009-04-06
 */
static rsRetVal
findRSFunction(cstr_t *pcsName, prsf_t *prsf)
{
	rsf_entry_t *pEntry;
	rsf_entry_t *pFound;
	DEFiRet;

	assert(prsf != NULL);

	/* find function by list walkthrough.  */
	pFound = NULL;
	for(pEntry = funcRegRoot ; pEntry != NULL && pFound == NULL ; pEntry = pEntry->pNext)
		if(!rsCStrCStrCmp(pEntry->pName, pcsName))
			pFound = pEntry;

	if(pFound == NULL)
		ABORT_FINALIZE(RS_RET_UNKNW_FUNC);

	*prsf = pFound->rsf;

finalize_it:
	RETiRet;
}


/* find the name of a RainerScript function whom's function pointer
 * is known. This function returns the cstr_t object, which MUST NOT
 * be modified by the caller.
 * rgerhards, 2009-04-06
 */
static rsRetVal
findRSFunctionName(prsf_t rsf, cstr_t **ppcsName)
{
	rsf_entry_t *pEntry;
	rsf_entry_t *pFound;
	DEFiRet;

	assert(rsf != NULL);
	assert(ppcsName != NULL);

	/* find function by list walkthrough.  */
	pFound = NULL;
	for(pEntry = funcRegRoot ; pEntry != NULL && pFound == NULL ; pEntry = pEntry->pNext)
		if(pEntry->rsf == rsf)
			pFound = pEntry;

	if(pFound == NULL)
		ABORT_FINALIZE(RS_RET_UNKNW_FUNC);

	*ppcsName = pFound->pName;

finalize_it:
	RETiRet;
}


/* free the whole function registry
 */
static void
rsfrRemoveAll(void)
{
	rsf_entry_t *pEntry;
	rsf_entry_t *pEntryDel;
	
	BEGINfunc
	pEntry = funcRegRoot;
	while(pEntry != NULL) {
		pEntryDel = pEntry;
		pEntry = pEntry->pNext;
		cstrDestruct(&pEntryDel->pName);
		free(pEntryDel);
	}
	funcRegRoot = NULL;
	ENDfunc
}


/* ------------------------------ end function registry code and structures  ------------------------------ */


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
#define NUMOP(name, OPERATION) \
BEGINop(name) /* remember to set the instruction also in the ENDop macro! */ \
	var_t *operand1; \
	var_t *operand2; \
CODESTARTop(name) \
	vmstk.PopNumber(pThis->pStk, &operand1); \
	vmstk.PopNumber(pThis->pStk, &operand2); \
	operand1->val.num = operand1->val.num OPERATION operand2->val.num; \
	vmstk.Push(pThis->pStk, operand1); /* result */ \
	var.Destruct(&operand2); /* no longer needed */ \
ENDop(name)
NUMOP(PLUS,   +)
NUMOP(MINUS,  -)
NUMOP(TIMES,  *)
NUMOP(DIV,    /)
NUMOP(MOD,    %)
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
ENDCMPOP(CMP_NEQ)

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
	number_t bRes;
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


BEGINop(CMP_CONTAINSI) /* remember to set the instruction also in the ENDop macro! */
	var_t *operand1;
	var_t *operand2;
	number_t bRes;
CODESTARTop(CMP_CONTAINSI)
	/* operand2 is on top of stack, so needs to be popped first */
	vmstk.PopString(pThis->pStk, &operand2);
	vmstk.PopString(pThis->pStk, &operand1);
var.DebugPrint(operand1); \
var.DebugPrint(operand2); \
	/* TODO: extend cstr class so that it supports location of cstr inside cstr */
	bRes = (rsCStrCaseInsensitiveLocateInSzStr(operand2->val.pStr, rsCStrGetSzStr(operand1->val.pStr)) == -1) ? 0 : 1;

	/* we have a result, so let's push it */
	PUSHRESULTop(operand1, bRes);
	var.Destruct(&operand2); /* no longer needed */
ENDop(CMP_CONTAINSI)


BEGINop(CMP_STARTSWITH) /* remember to set the instruction also in the ENDop macro! */
	var_t *operand1;
	var_t *operand2;
	number_t bRes;
CODESTARTop(CMP_STARTSWITH)
	/* operand2 is on top of stack, so needs to be popped first */
	vmstk.PopString(pThis->pStk, &operand2);
	vmstk.PopString(pThis->pStk, &operand1);
	/* TODO: extend cstr class so that it supports location of cstr inside cstr */
	bRes = (rsCStrStartsWithSzStr(operand1->val.pStr, rsCStrGetSzStr(operand2->val.pStr),
		rsCStrLen(operand2->val.pStr)) == 0) ? 1 : 0;

	/* we have a result, so let's push it */
	PUSHRESULTop(operand1, bRes);
	var.Destruct(&operand2); /* no longer needed */
ENDop(CMP_STARTSWITH)


BEGINop(CMP_STARTSWITHI) /* remember to set the instruction also in the ENDop macro! */
	var_t *operand1;
	var_t *operand2;
	number_t bRes;
CODESTARTop(CMP_STARTSWITHI)
	/* operand2 is on top of stack, so needs to be popped first */
	vmstk.PopString(pThis->pStk, &operand2);
	vmstk.PopString(pThis->pStk, &operand1);
	/* TODO: extend cstr class so that it supports location of cstr inside cstr */
	bRes = (rsCStrCaseInsensitveStartsWithSzStr(operand1->val.pStr, rsCStrGetSzStr(operand2->val.pStr),
		rsCStrLen(operand2->val.pStr)) == 0) ? 1 : 0;

	/* we have a result, so let's push it */
	PUSHRESULTop(operand1, bRes);
	var.Destruct(&operand2); /* no longer needed */
ENDop(CMP_STARTSWITHI)

/* end comare operations that work on strings, only */

BEGINop(STRADD) /* remember to set the instruction also in the ENDop macro! */
	var_t *operand1;
	var_t *operand2;
CODESTARTop(STRADD)
	vmstk.PopString(pThis->pStk, &operand2);
	vmstk.PopString(pThis->pStk, &operand1);

	CHKiRet(rsCStrAppendCStr(operand1->val.pStr, operand2->val.pStr));
	CHKiRet(cstrFinalize(operand1->val.pStr));

	/* we have a result, so let's push it */
	vmstk.Push(pThis->pStk, operand1);
	var.Destruct(&operand2); /* no longer needed */
finalize_it:
ENDop(STRADD)

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
	var_t *pVarDup;   /* we need to duplicate the var, as we need to hand it over */
CODESTARTop(PUSHCONSTANT)
	CHKiRet(var.Duplicate(pOp->operand.pVar, &pVarDup));
	vmstk.Push(pThis->pStk, pVarDup);
finalize_it:
ENDop(PUSHCONSTANT)


BEGINop(PUSHMSGVAR) /* remember to set the instruction also in the ENDop macro! */
	var_t *pVal; /* the value to push */
	cstr_t *pstrVal;
CODESTARTop(PUSHMSGVAR)
	if(pThis->pMsg == NULL) {
		/* TODO: flag an error message! As a work-around, we permit
		 * execution to continue here with an empty string
		 */
		/* TODO: create a method in var to create a string var? */
		CHKiRet(var.Construct(&pVal));
		CHKiRet(var.ConstructFinalize(pVal));
		CHKiRet(rsCStrConstructFromszStr(&pstrVal, (uchar*)""));
		CHKiRet(var.SetString(pVal, pstrVal));
	} else {
		/* we have a message, so pull value from there */
		CHKiRet(msgGetMsgVar(pThis->pMsg, pOp->operand.pVar->val.pStr, &pVal));
	}

	/* if we reach this point, we have a valid pVal and can push it */
	vmstk.Push(pThis->pStk, pVal);
finalize_it:
ENDop(PUSHMSGVAR)


BEGINop(PUSHSYSVAR) /* remember to set the instruction also in the ENDop macro! */
	var_t *pVal; /* the value to push */
CODESTARTop(PUSHSYSVAR)
	CHKiRet(sysvar.GetVar(pOp->operand.pVar->val.pStr, &pVal));
	vmstk.Push(pThis->pStk, pVal);
finalize_it:
	if(Debug && iRet != RS_RET_OK) {
		if(iRet == RS_RET_SYSVAR_NOT_FOUND) {
			DBGPRINTF("rainerscript: sysvar '%s' not found\n",
				  rsCStrGetSzStrNoNULL(pOp->operand.pVar->val.pStr));
		} else {
			DBGPRINTF("rainerscript: error %d trying to obtain sysvar '%s'\n",
				  iRet, rsCStrGetSzStrNoNULL(pOp->operand.pVar->val.pStr));
		}
	}
ENDop(PUSHSYSVAR)

/* The function call operation is only very roughly implemented. While the plumbing
 * to reach this instruction is fine, the instruction itself currently supports only
 * functions with a single argument AND with a name that we know.
 * TODO: later, we can add here the real logic, that involves looking up function
 * names, loading them dynamically ... and all that...
 * implementation begun 2009-03-10 by rgerhards
 */
BEGINop(FUNC_CALL) /* remember to set the instruction also in the ENDop macro! */
	var_t *numOperands;
CODESTARTop(FUNC_CALL)
	vmstk.PopNumber(pThis->pStk, &numOperands);
	CHKiRet((*pOp->operand.rsf)(pThis->pStk, numOperands->val.num));
	var.Destruct(&numOperands); /* no longer needed */
finalize_it:
ENDop(FUNC_CALL)


/* ------------------------------ end instruction set implementation ------------------------------ */


/* ------------------------------ begin built-in function implementation ------------------------------ */
/* note: this shall probably be moved to a separate module, but for the time being we do it directly
 * in here. This is on our way to get from a dirty to a clean solution via baby steps that are
 * a bit less dirty each time... 
 *
 * The advantage of doing it here is that we do not yet need to think about how to handle the
 * exit case, where we must not unload function modules which functions are still referenced.
 *
 * CALLING INTERFACE:
 * The function must pop its parameters off the stack and pop its result onto
 * the stack when it is finished. The number of parameters the function was
 * called with is provided to it. If the argument count is less then what the function
 * expected, it may handle the situation with defaults (or return an error). If the
 * argument count is greater than expected, returnung an error is highly 
 * recommended (use RS_RET_INVLD_NBR_ARGUMENTS for these cases).
 *
 * All function names are prefixed with "rsf_" (RainerScript Function) to have
 * a separate "name space".
 *
 * rgerhards, 2009-04-06
 */


/* The strlen function, also probably a prototype of how all functions should be
 * implemented.
 * rgerhards, 2009-04-06
 */
static rsRetVal
rsf_strlen(vmstk_t *pStk, int numOperands)
{
	DEFiRet;
	var_t *operand1;
	int iStrlen;

	if(numOperands != 1)
		ABORT_FINALIZE(RS_RET_INVLD_NBR_ARGUMENTS);

	/* pop args and do operaton (trivial case here...) */
	vmstk.PopString(pStk, &operand1);
	iStrlen = strlen((char*) rsCStrGetSzStr(operand1->val.pStr));

	/* Store result and cleanup */
	var.SetNumber(operand1, iStrlen);
	vmstk.Push(pStk, operand1);
finalize_it:
	RETiRet;
}


/* The getenv function. Note that we guard the OS call by a mutex, as that
 * function is not guaranteed to be thread-safe. This implementation here is far from
 * being optimal, at least we should cache the result. This is left TODO for
 * a later revision.
 * rgerhards, 2009-11-03
 */
static rsRetVal
rsf_getenv(vmstk_t *pStk, int numOperands)
{
	DEFiRet;
	var_t *operand1;
	char *envResult;
	cstr_t *pCstr;

	if(numOperands != 1)
		ABORT_FINALIZE(RS_RET_INVLD_NBR_ARGUMENTS);

	/* pop args and do operaton (trivial case here...) */
	vmstk.PopString(pStk, &operand1);
	d_pthread_mutex_lock(&mutGetenv);
	envResult = getenv((char*) rsCStrGetSzStr(operand1->val.pStr));
	DBGPRINTF("rsf_getenv(): envvar '%s', return '%s'\n", rsCStrGetSzStr(operand1->val.pStr),
		   envResult == NULL ? "(NULL)" : envResult);
	iRet = rsCStrConstructFromszStr(&pCstr, (envResult == NULL) ? UCHAR_CONSTANT("") : (uchar*)envResult);
	d_pthread_mutex_unlock(&mutGetenv);
	if(iRet != RS_RET_OK)
		FINALIZE; /* need to do this after mutex is unlocked! */

	/* Store result and cleanup */
	var.SetString(operand1, pCstr);
	vmstk.Push(pStk, operand1);
finalize_it:
	RETiRet;
}


/* The "tolower" function, which converts its sole argument to lower case.
 * Quite honestly, currently this is primarily a test driver for me...
 * rgerhards, 2009-04-06
 */
static rsRetVal
rsf_tolower(vmstk_t *pStk, int numOperands)
{
	DEFiRet;
	var_t *operand1;
	uchar *pSrc;
	cstr_t *pcstr;
	int iStrlen;

	if(numOperands != 1)
		ABORT_FINALIZE(RS_RET_INVLD_NBR_ARGUMENTS);

	/* pop args and do operaton */
	CHKiRet(cstrConstruct(&pcstr));
	vmstk.PopString(pStk, &operand1);
	pSrc = cstrGetSzStr(operand1->val.pStr);
	iStrlen = strlen((char*)pSrc); // TODO: use count from string!
	while(iStrlen--) {
		CHKiRet(cstrAppendChar(pcstr, tolower(*pSrc++)));
	}

	/* Store result and cleanup */
	CHKiRet(cstrFinalize(pcstr));
	var.SetString(operand1, pcstr);
	vmstk.Push(pStk, operand1);
finalize_it:
	RETiRet;
}


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
	if(pThis->pMsg != NULL)
		msgDestruct(&pThis->pMsg);
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

#define doOP(OP) case opcode_##OP: DBGPRINTF("rainerscript: opcode %s\n", #OP); \
				   CHKiRet(op##OP(pThis, pCurrOp)); break
	pCurrOp = pProg->vmopRoot; /* TODO: do this via a method! */
	while(pCurrOp != NULL && pCurrOp->opcode != opcode_END_PROG) {
		DBGPRINTF("rainerscript: executing step, opcode %d...\n", pCurrOp->opcode);
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
			doOP(CMP_CONTAINSI);
			doOP(CMP_STARTSWITH);
			doOP(CMP_STARTSWITHI);
			doOP(NOT);
			doOP(PUSHCONSTANT);
			doOP(PUSHMSGVAR);
			doOP(PUSHSYSVAR);
			doOP(STRADD);
			doOP(PLUS);
			doOP(MINUS);
			doOP(TIMES);
			doOP(DIV);
			doOP(MOD);
			doOP(UNARY_MINUS);
			doOP(FUNC_CALL);
			default:
				dbgoprint((obj_t*) pThis, "invalid instruction %d in vmprg\n", pCurrOp->opcode);
				ABORT_FINALIZE(RS_RET_INVALID_VMOP);
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
	DBGPRINTF("rainerscript: script execution terminated with state %d\n", iRet);
	RETiRet;
}


/* Set the current message object for the VM. It *is* valid to set a
 * NULL message object, what simply means there is none. Message
 * objects are properly reference counted.
 */
static rsRetVal
SetMsg(vm_t *pThis, msg_t *pMsg)
{
	DEFiRet;
	if(pThis->pMsg != NULL) {
		msgDestruct(&pThis->pMsg);
	}

	if(pMsg != NULL) {
		pThis->pMsg = MsgAddRef(pMsg);
	}

	RETiRet;
}


/* Pop a var from the stack and return it to caller. The variable type is not
 * changed, it is taken from the stack as is. This functionality is
 * partly needed. We may (or may not ;)) be able to remove it once we have
 * full RainerScript support. -- rgerhards, 2008-02-25
 */
static rsRetVal
PopVarFromStack(vm_t *pThis, var_t **ppVar)
{
	DEFiRet;
	CHKiRet(vmstk.Pop(pThis->pStk, ppVar));
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
	pIf->Construct = vmConstruct;
	pIf->ConstructFinalize = vmConstructFinalize;
	pIf->Destruct = vmDestruct;
	pIf->DebugPrint = vmDebugPrint;
	pIf->ExecProg = execProg;
	pIf->PopBoolFromStack = PopBoolFromStack;
	pIf->PopVarFromStack = PopVarFromStack;
	pIf->SetMsg = SetMsg;
	pIf->FindRSFunction = findRSFunction;
	pIf->FindRSFunctionName = findRSFunctionName;
finalize_it:
ENDobjQueryInterface(vm)


/* Exit the vm class.
 * rgerhards, 2009-04-06
 */
BEGINObjClassExit(vm, OBJ_IS_CORE_MODULE) /* class, version */
	rsfrRemoveAll();
	objRelease(sysvar, CORE_COMPONENT);
	objRelease(var, CORE_COMPONENT);
	objRelease(vmstk, CORE_COMPONENT);

	pthread_mutex_destroy(&mutGetenv);
ENDObjClassExit(vm)


/* Initialize the vm class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINObjClassInit(vm, 1, OBJ_IS_CORE_MODULE) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(vmstk, CORE_COMPONENT));
	CHKiRet(objUse(var, CORE_COMPONENT));
	CHKiRet(objUse(sysvar, CORE_COMPONENT));

	/* set our own handlers */
	OBJSetMethodHandler(objMethod_DEBUGPRINT, vmDebugPrint);
	OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, vmConstructFinalize);

	/* register built-in functions // TODO: move to its own module */
	CHKiRet(rsfrAddFunction((uchar*)"strlen",  rsf_strlen));
	CHKiRet(rsfrAddFunction((uchar*)"tolower", rsf_tolower));
	CHKiRet(rsfrAddFunction((uchar*)"getenv",  rsf_getenv));

	pthread_mutex_init(&mutGetenv, NULL);

ENDObjClassInit(vm)

/* vi:set ai:
 */
