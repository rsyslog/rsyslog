/* rule.c - rsyslog's rule object
 *
 * See file comment in rule.c for the overall structure of rule processing.
 *
 * Module begun 2009-06-10 by Rainer Gerhards
 *
 * Copyright 2009-2012 Adiscon GmbH.
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
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#include "rsyslog.h"
#include "obj.h"
#include "action.h"
#include "rule.h"
#include "errmsg.h"
#include "vm.h"
#include "var.h"
#include "srUtils.h"
#include "batch.h"
#include "unicode-helper.h"

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(errmsg)
DEFobjCurrIf(expr)
DEFobjCurrIf(var)
DEFobjCurrIf(vm)


/* support for simple textual representation of FIOP names
 * rgerhards, 2005-09-27
 */
static char*
getFIOPName(unsigned iFIOP)
{
	char *pRet;
	switch(iFIOP) {
		case FIOP_CONTAINS:
			pRet = "contains";
			break;
		case FIOP_ISEQUAL:
			pRet = "isequal";
			break;
		case FIOP_STARTSWITH:
			pRet = "startswith";
			break;
		case FIOP_REGEX:
			pRet = "regex";
			break;
		case FIOP_EREREGEX:
			pRet = "ereregex";
			break;
		case FIOP_ISEMPTY:
			pRet = "isempty";
			break;
		default:
			pRet = "NOP";
			break;
	}
	return pRet;
}


/* iterate over all actions, this is often needed, for example when HUP processing 
 * must be done or a shutdown is pending.
 */
static rsRetVal
iterateAllActions(rule_t *pThis, rsRetVal (*pFunc)(void*, void*), void* pParam)
{
	return llExecFunc(&pThis->llActList, pFunc, pParam);
}


/* helper to processMsg(), used to call the configured actions. It is
 * executed from within llExecFunc() of the action list.
 * rgerhards, 2007-08-02
 */
DEFFUNC_llExecFunc(processBatchDoActions)
{
	DEFiRet;
	rsRetVal iRetMod;	/* return value of module - we do not always pass that back */
	action_t *pAction = (action_t*) pData;
	batch_t *pBatch = (batch_t*) pParam;

	DBGPRINTF("Processing next action\n");
	iRetMod = pAction->submitToActQ(pAction, pBatch);

	RETiRet;
}


/* This functions looks at the given message and checks if it matches the
 * provided filter condition.
 */
static rsRetVal
shouldProcessThisMessage(rule_t *pRule, msg_t *pMsg, sbool *bProcessMsg)
{
	DEFiRet;
	unsigned short pbMustBeFreed;
	uchar *pszPropVal;
	int bRet = 0;
	size_t propLen;
	vm_t *pVM = NULL;
	var_t *pResult = NULL;

	ISOBJ_TYPE_assert(pRule, rule);
	assert(pMsg != NULL);

	/* we first have a look at the global, BSD-style block filters (for tag
	 * and host). Only if they match, we evaluate the actual filter.
	 * rgerhards, 2005-10-18
	 */
	if(pRule->eHostnameCmpMode == HN_NO_COMP) {
		/* EMPTY BY INTENSION - we check this value first, because
		 * it is the one most often used, so this saves us time!
		 */
	} else if(pRule->eHostnameCmpMode == HN_COMP_MATCH) {
		if(rsCStrSzStrCmp(pRule->pCSHostnameComp, (uchar*) getHOSTNAME(pMsg), getHOSTNAMELen(pMsg))) {
			/* not equal, so we are already done... */
			dbgprintf("hostname filter '+%s' does not match '%s'\n", 
				rsCStrGetSzStrNoNULL(pRule->pCSHostnameComp), getHOSTNAME(pMsg));
			FINALIZE;
		}
	} else { /* must be -hostname */
		if(!rsCStrSzStrCmp(pRule->pCSHostnameComp, (uchar*) getHOSTNAME(pMsg), getHOSTNAMELen(pMsg))) {
			/* not equal, so we are already done... */
			dbgprintf("hostname filter '-%s' does not match '%s'\n", 
				rsCStrGetSzStrNoNULL(pRule->pCSHostnameComp), getHOSTNAME(pMsg));
			FINALIZE;
		}
	}
	
	if(pRule->pCSProgNameComp != NULL) {
		int bInv = 0, bEqv = 0, offset = 0;
		if(*(rsCStrGetSzStrNoNULL(pRule->pCSProgNameComp)) == '-') {
			if(*(rsCStrGetSzStrNoNULL(pRule->pCSProgNameComp) + 1) == '-')
				offset = 1;
			else {
				bInv = 1;
				offset = 1;
			}
		}
		if(!rsCStrOffsetSzStrCmp(pRule->pCSProgNameComp, offset,
			(uchar*) getProgramName(pMsg, LOCK_MUTEX), getProgramNameLen(pMsg, LOCK_MUTEX)))
			bEqv = 1;

		if((!bEqv && !bInv) || (bEqv && bInv)) {
			/* not equal or inverted selection, so we are already done... */
			DBGPRINTF("programname filter '%s' does not match '%s'\n", 
				rsCStrGetSzStrNoNULL(pRule->pCSProgNameComp), getProgramName(pMsg, LOCK_MUTEX));
			FINALIZE;
		}
	}
	
	/* done with the BSD-style block filters */

	if(pRule->f_filter_type == FILTER_PRI) {
		/* skip messages that are incorrect priority */
		dbgprintf("testing filter, f_pmask %d\n", pRule->f_filterData.f_pmask[pMsg->iFacility]);
		if ( (pRule->f_filterData.f_pmask[pMsg->iFacility] == TABLE_NOPRI) || \
		    ((pRule->f_filterData.f_pmask[pMsg->iFacility] & (1<<pMsg->iSeverity)) == 0) )
			bRet = 0;
		else
			bRet = 1;
	} else if(pRule->f_filter_type == FILTER_EXPR) {
		CHKiRet(vm.Construct(&pVM));
		CHKiRet(vm.ConstructFinalize(pVM));
		CHKiRet(vm.SetMsg(pVM, pMsg));
		CHKiRet(vm.ExecProg(pVM, pRule->f_filterData.f_expr->pVmprg));
		CHKiRet(vm.PopBoolFromStack(pVM, &pResult));
		dbgprintf("result of rainerscript filter evaluation: %lld\n", pResult->val.num);
		/* VM is destructed on function exit */
		bRet = (pResult->val.num) ? 1 : 0;
	} else {
		assert(pRule->f_filter_type == FILTER_PROP); /* assert() just in case... */
		pszPropVal = MsgGetProp(pMsg, NULL, pRule->f_filterData.prop.propID,
					pRule->f_filterData.prop.propName, &propLen, &pbMustBeFreed);

		/* Now do the compares (short list currently ;)) */
		switch(pRule->f_filterData.prop.operation ) {
		case FIOP_CONTAINS:
			if(rsCStrLocateInSzStr(pRule->f_filterData.prop.pCSCompValue, (uchar*) pszPropVal) != -1)
				bRet = 1;
			break;
		case FIOP_ISEMPTY:
			if(propLen == 0)
				bRet = 1; /* process message! */
			break;
		case FIOP_ISEQUAL:
			if(rsCStrSzStrCmp(pRule->f_filterData.prop.pCSCompValue,
					  pszPropVal, ustrlen(pszPropVal)) == 0)
				bRet = 1; /* process message! */
			break;
		case FIOP_STARTSWITH:
			if(rsCStrSzStrStartsWithCStr(pRule->f_filterData.prop.pCSCompValue,
					  pszPropVal, ustrlen(pszPropVal)) == 0)
				bRet = 1; /* process message! */
			break;
		case FIOP_REGEX:
			if(rsCStrSzStrMatchRegex(pRule->f_filterData.prop.pCSCompValue,
					(unsigned char*) pszPropVal, 0, &pRule->f_filterData.prop.regex_cache) == RS_RET_OK)
				bRet = 1;
			break;
		case FIOP_EREREGEX:
			if(rsCStrSzStrMatchRegex(pRule->f_filterData.prop.pCSCompValue,
					  (unsigned char*) pszPropVal, 1, &pRule->f_filterData.prop.regex_cache) == RS_RET_OK)
				bRet = 1;
			break;
		default:
			/* here, it handles NOP (for performance reasons) */
			assert(pRule->f_filterData.prop.operation == FIOP_NOP);
			bRet = 1; /* as good as any other default ;) */
			break;
		}

		/* now check if the value must be negated */
		if(pRule->f_filterData.prop.isNegated)
			bRet = (bRet == 1) ?  0 : 1;

		if(Debug) {
			char *cstr;
			if(pRule->f_filterData.prop.propID == PROP_CEE) {
				cstr = es_str2cstr(pRule->f_filterData.prop.propName, NULL);
				dbgprintf("Filter: check for CEE property '%s' (value '%s') ",
					cstr, pszPropVal);
				free(cstr);
			} else {
				dbgprintf("Filter: check for property '%s' (value '%s') ",
					propIDToName(pRule->f_filterData.prop.propID), pszPropVal);
			}
			if(pRule->f_filterData.prop.isNegated)
				dbgprintf("NOT ");
			if(pRule->f_filterData.prop.operation == FIOP_ISEMPTY) {
				dbgprintf("%s : %s\n",
				       getFIOPName(pRule->f_filterData.prop.operation),
				       bRet ? "TRUE" : "FALSE");
			} else {
				dbgprintf("%s '%s': %s\n",
				       getFIOPName(pRule->f_filterData.prop.operation),
				       rsCStrGetSzStrNoNULL(pRule->f_filterData.prop.pCSCompValue),
				       bRet ? "TRUE" : "FALSE");
			}
		}

		/* cleanup */
		if(pbMustBeFreed)
			free(pszPropVal);
	}

finalize_it:
	/* destruct in any case, not just on error, but it makes error handling much easier */
	if(pVM != NULL)
		vm.Destruct(&pVM);

	if(pResult != NULL)
		var.Destruct(&pResult);

	*bProcessMsg = bRet;
	RETiRet;
}



/* Process (consume) a batch of messages. Calls the actions configured.
 * rgerhards, 2005-10-13
 */
static rsRetVal
processBatch(rule_t *pThis, batch_t *pBatch)
{
	int i;
	rsRetVal localRet;
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, rule);
	assert(pBatch != NULL);

	/* first check the filters and reset status variables */
	for(i = 0 ; i < batchNumMsgs(pBatch) && !*(pBatch->pbShutdownImmediate) ; ++i) {
		localRet = shouldProcessThisMessage(pThis, (msg_t*)(pBatch->pElem[i].pUsrp),
						    &(pBatch->pElem[i].bFilterOK));
		if(localRet != RS_RET_OK) {
			DBGPRINTF("processBatch: iRet %d returned from shouldProcessThisMessage, "
			          "ignoring message\n", localRet);
			pBatch->pElem[i].bFilterOK = 0;
		}
		if(pBatch->pElem[i].bFilterOK) {
			/* re-init only when actually needed (cache write cost!) */
			pBatch->pElem[i].bPrevWasSuspended = 0;
		}
	}
	CHKiRet(llExecFunc(&pThis->llActList, processBatchDoActions, pBatch));

finalize_it:
	RETiRet;
}


/* Standard-Constructor
 */
BEGINobjConstruct(rule) /* be sure to specify the object type also in END macro! */
ENDobjConstruct(rule)


/* ConstructionFinalizer
 * rgerhards, 2008-01-09
 */
static rsRetVal
ruleConstructFinalize(rule_t *pThis)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, rule);

	/* note: actionDestruct is from action.c API! */
	CHKiRet(llInit(&pThis->llActList, actionDestruct, NULL, NULL));
	
finalize_it:
	RETiRet;
}


/* destructor for the rule object */
BEGINobjDestruct(rule) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(rule)
	if(pThis->pCSHostnameComp != NULL)
		rsCStrDestruct(&pThis->pCSHostnameComp);
	if(pThis->pCSProgNameComp != NULL)
		rsCStrDestruct(&pThis->pCSProgNameComp);

	if(pThis->f_filter_type == FILTER_PROP) {
		if(pThis->f_filterData.prop.pCSCompValue != NULL)
			rsCStrDestruct(&pThis->f_filterData.prop.pCSCompValue);
		if(pThis->f_filterData.prop.regex_cache != NULL)
			rsCStrRegexDestruct(&pThis->f_filterData.prop.regex_cache);
		if(pThis->f_filterData.prop.propName != NULL)
			es_deleteStr(pThis->f_filterData.prop.propName);
	} else if(pThis->f_filter_type == FILTER_EXPR) {
		if(pThis->f_filterData.f_expr != NULL)
			expr.Destruct(&pThis->f_filterData.f_expr);
	}

	llDestroy(&pThis->llActList);
ENDobjDestruct(rule)


/* set the associated ruleset */
static rsRetVal
setAssRuleset(rule_t *pThis, ruleset_t *pRuleset)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, rule);
	ISOBJ_TYPE_assert(pRuleset, ruleset);
	pThis->pRuleset = pRuleset;
	RETiRet;
}

/* get the associated ruleset (may be NULL if not set!) */
static ruleset_t*
getAssRuleset(rule_t *pThis)
{
	ISOBJ_TYPE_assert(pThis, rule);
	return pThis->pRuleset;
}


/* helper to DebugPrint, to print out all actions via
 * the llExecFunc() facility.
 */
DEFFUNC_llExecFunc(dbgPrintInitInfoAction)
{
	DEFiRet;
	iRet = actionDbgPrint((action_t*) pData);
	dbgprintf("\n");
	RETiRet;
}


/* debugprint for the rule object */
BEGINobjDebugPrint(rule) /* be sure to specify the object type also in END and CODESTART macros! */
	int i;
	char *cstr;
CODESTARTobjDebugPrint(rule)
	dbgoprint((obj_t*) pThis, "rsyslog rule:\n");
	if(pThis->pCSProgNameComp != NULL)
		dbgprintf("tag: '%s'\n", rsCStrGetSzStrNoNULL(pThis->pCSProgNameComp));
	if(pThis->eHostnameCmpMode != HN_NO_COMP)
		dbgprintf("hostname: %s '%s'\n",
			pThis->eHostnameCmpMode == HN_COMP_MATCH ?
				"only" : "allbut",
			rsCStrGetSzStrNoNULL(pThis->pCSHostnameComp));
	if(pThis->f_filter_type == FILTER_PRI) {
		for (i = 0; i <= LOG_NFACILITIES; i++)
			if (pThis->f_filterData.f_pmask[i] == TABLE_NOPRI)
				dbgprintf(" X ");
			else
				dbgprintf("%2X ", pThis->f_filterData.f_pmask[i]);
	} else if(pThis->f_filter_type == FILTER_EXPR) {
		dbgprintf("EXPRESSION-BASED Filter: can currently not be displayed");
	} else {
		dbgprintf("PROPERTY-BASED Filter:\n");
		dbgprintf("\tProperty.: '%s'\n", propIDToName(pThis->f_filterData.prop.propID));
		if(pThis->f_filterData.prop.propName != NULL) {
			cstr = es_str2cstr(pThis->f_filterData.prop.propName, NULL);
			dbgprintf("\tCEE-Prop.: '%s'\n", cstr);
			free(cstr);
		}
		dbgprintf("\tOperation: ");
		if(pThis->f_filterData.prop.isNegated)
			dbgprintf("NOT ");
		dbgprintf("'%s'\n", getFIOPName(pThis->f_filterData.prop.operation));
		if(pThis->f_filterData.prop.pCSCompValue != NULL) {
			dbgprintf("\tValue....: '%s'\n",
			       rsCStrGetSzStrNoNULL(pThis->f_filterData.prop.pCSCompValue));
		}
		dbgprintf("\tAction...: ");
	}

	dbgprintf("\nActions:\n");
	llExecFunc(&pThis->llActList, dbgPrintInitInfoAction, NULL); /* actions */

	dbgprintf("\n");
ENDobjDebugPrint(rule)


/* queryInterface function
 * rgerhards, 2008-02-21
 */
BEGINobjQueryInterface(rule)
CODESTARTobjQueryInterface(rule)
	if(pIf->ifVersion != ruleCURR_IF_VERSION) { /* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->Construct = ruleConstruct;
	pIf->ConstructFinalize = ruleConstructFinalize;
	pIf->Destruct = ruleDestruct;
	pIf->DebugPrint = ruleDebugPrint;

	pIf->IterateAllActions = iterateAllActions;
	pIf->ProcessBatch = processBatch;
	pIf->SetAssRuleset = setAssRuleset;
	pIf->GetAssRuleset = getAssRuleset;
finalize_it:
ENDobjQueryInterface(rule)


/* Exit the rule class.
 * rgerhards, 2009-04-06
 */
BEGINObjClassExit(rule, OBJ_IS_CORE_MODULE) /* class, version */
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(expr, CORE_COMPONENT);
	objRelease(var, CORE_COMPONENT);
	objRelease(vm, CORE_COMPONENT);
ENDObjClassExit(rule)


/* Initialize the rule class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINObjClassInit(rule, 1, OBJ_IS_CORE_MODULE) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(expr, CORE_COMPONENT));
	CHKiRet(objUse(var, CORE_COMPONENT));
	CHKiRet(objUse(vm, CORE_COMPONENT));

	/* set our own handlers */
	OBJSetMethodHandler(objMethod_DEBUGPRINT, ruleDebugPrint);
	OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, ruleConstructFinalize);
ENDObjClassInit(rule)

/* vi:set ai:
 */
