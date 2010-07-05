/* ruleset.c - rsyslog's ruleset object
 *
 * We have a two-way structure of linked lists: one global linked list
 * (llAllRulesets) hold alls rule sets that we know. Included in each
 * list is a list of rules (which contain a list of actions, but that's
 * a different story).
 *
 * Usually, only a single rule set is executed. However, there exist some
 * situations where all rules must be iterated over, for example on HUP. Thus,
 * we also provide interfaces to do that.
 *
 * Module begun 2009-06-10 by Rainer Gerhards
 *
 * Copyright 2009 Rainer Gerhards and Adiscon GmbH.
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
#include "msg.h"
#include "ruleset.h"
#include "rule.h"
#include "errmsg.h"
#include "unicode-helper.h"

static rsRetVal debugPrintAll(void); // TODO: remove!

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(errmsg)
DEFobjCurrIf(rule)

linkedList_t llRulesets; /* this is NOT a pointer - no typo here ;) */
ruleset_t *pCurrRuleset = NULL; /* currently "active" ruleset */
ruleset_t *pDfltRuleset = NULL; /* currentl default ruleset, e.g. for binding to actions which have no other */

/* ---------- linked-list key handling functions ---------- */

/* destructor for linked list keys.
 */
static rsRetVal keyDestruct(void __attribute__((unused)) *pData)
{
	free(pData);
	return RS_RET_OK;
}


/* ---------- END linked-list key handling functions ---------- */


/* driver to iterate over all of this ruleset actions */
typedef struct iterateAllActions_s {
	rsRetVal (*pFunc)(void*, void*);
	void *pParam;
} iterateAllActions_t;
DEFFUNC_llExecFunc(doIterateRulesetActions)
{
	DEFiRet;
	rule_t* pRule = (rule_t*) pData;
	iterateAllActions_t *pMyParam = (iterateAllActions_t*) pParam;
	iRet = rule.IterateAllActions(pRule, pMyParam->pFunc, pMyParam->pParam);
	RETiRet;
}
/* iterate over all actions of THIS rule set.
 */
static rsRetVal
iterateRulesetAllActions(ruleset_t *pThis, rsRetVal (*pFunc)(void*, void*), void* pParam)
{
	iterateAllActions_t params;
	DEFiRet;
	assert(pFunc != NULL);

	params.pFunc = pFunc;
	params.pParam = pParam;
	CHKiRet(llExecFunc(&(pThis->llRules), doIterateRulesetActions, &params));

finalize_it:
	RETiRet;
}


/* driver to iterate over all actions */
DEFFUNC_llExecFunc(doIterateAllActions)
{
	DEFiRet;
	ruleset_t* pThis = (ruleset_t*) pData;
	iterateAllActions_t *pMyParam = (iterateAllActions_t*) pParam;
	iRet = iterateRulesetAllActions(pThis, pMyParam->pFunc, pMyParam->pParam);
	RETiRet;
}
/* iterate over ALL actions present in the WHOLE system.
 * this is often needed, for example when HUP processing 
 * must be done or a shutdown is pending.
 */
static rsRetVal
iterateAllActions(rsRetVal (*pFunc)(void*, void*), void* pParam)
{
	iterateAllActions_t params;
	DEFiRet;
	assert(pFunc != NULL);

	params.pFunc = pFunc;
	params.pParam = pParam;
	CHKiRet(llExecFunc(&llRulesets, doIterateAllActions, &params));

finalize_it:
	RETiRet;
}



/* helper to processMsg(), used to call the configured actions. It is
 * executed from within llExecFunc() of the action list.
 * rgerhards, 2007-08-02
 */
DEFFUNC_llExecFunc(processMsgDoRules)
{
	ISOBJ_TYPE_assert(pData, rule);
	return rule.ProcessMsg((rule_t*) pData, (msg_t*) pParam);
}


/* Process (consume) a received message. Calls the actions configured.
 * rgerhards, 2005-10-13
 */
static rsRetVal
processMsg(msg_t *pMsg)
{
	ruleset_t *pThis;
	DEFiRet;
	assert(pMsg != NULL);

	pThis = (pMsg->pRuleset == NULL) ? pDfltRuleset : pMsg->pRuleset;
	ISOBJ_TYPE_assert(pThis, ruleset);

	CHKiRet(llExecFunc(&pThis->llRules, processMsgDoRules, pMsg));

finalize_it:
	if(iRet == RS_RET_DISCARDMSG)
		iRet = RS_RET_OK;

	RETiRet;
}

/* Add a new rule to the end of the current rule set. We do a number
 * of checks and ignore the rule if it does not pass them.
 */
static rsRetVal
addRule(ruleset_t *pThis, rule_t **ppRule)
{
	int iActionCnt;
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, ruleset);
	ISOBJ_TYPE_assert(*ppRule, rule);

	CHKiRet(llGetNumElts(&(*ppRule)->llActList, &iActionCnt));
	if(iActionCnt == 0) {
		errmsg.LogError(0, NO_ERRCODE, "warning: selector line without actions will be discarded");
		rule.Destruct(ppRule);
	} else {
		CHKiRet(llAppend(&pThis->llRules, NULL, *ppRule));
		dbgprintf("selector line successfully processed\n");
	}

finalize_it:
	RETiRet;
}


/* set name for ruleset */
static rsRetVal setName(ruleset_t *pThis, uchar *pszName)
{
	DEFiRet;
	free(pThis->pszName);
	CHKmalloc(pThis->pszName = ustrdup(pszName));

finalize_it:
	RETiRet;
}


/* get current ruleset
 * We use a non-standard calling interface, as nothing can go wrong and it
 * is really much more natural to return the pointer directly.
 */
static ruleset_t*
GetCurrent(void)
{
	return pCurrRuleset;
}


/* Find the ruleset with the given name and return a pointer to its object.
 */
static rsRetVal
GetRuleset(ruleset_t **ppRuleset, uchar *pszName)
{
	DEFiRet;
	assert(ppRuleset != NULL);
	assert(pszName != NULL);

	CHKiRet(llFind(&llRulesets, pszName, (void*) ppRuleset));

finalize_it:
	RETiRet;
}


/* Set a new default rule set. If the default can not be found, no change happens.
 */
static rsRetVal
SetDefaultRuleset(uchar *pszName)
{
	ruleset_t *pRuleset;
	DEFiRet;
	assert(pszName != NULL);

	CHKiRet(GetRuleset(&pRuleset, pszName));
	pDfltRuleset = pRuleset;
	dbgprintf("default rule set changed to %p: '%s'\n", pRuleset, pszName);

finalize_it:
	RETiRet;
}


/* Set a new current rule set. If the ruleset can not be found, no change happens.
 */
static rsRetVal
SetCurrRuleset(uchar *pszName)
{
	ruleset_t *pRuleset;
	DEFiRet;
	assert(pszName != NULL);

	CHKiRet(GetRuleset(&pRuleset, pszName));
	pCurrRuleset = pRuleset;
	dbgprintf("current rule set changed to %p: '%s'\n", pRuleset, pszName);

finalize_it:
	RETiRet;
}


/* destructor we need to destruct rules inside our linked list contents.
 */
static rsRetVal
doRuleDestruct(void *pData)
{
	rule_t *pRule = (rule_t *) pData;
	DEFiRet;
	rule.Destruct(&pRule);
	RETiRet;
}


/* Standard-Constructor
 */
BEGINobjConstruct(ruleset) /* be sure to specify the object type also in END macro! */
	CHKiRet(llInit(&pThis->llRules, doRuleDestruct, NULL, NULL));
finalize_it:
ENDobjConstruct(ruleset)


/* ConstructionFinalizer
 * This also adds the rule set to the list of all known rulesets.
 */
static rsRetVal
rulesetConstructFinalize(ruleset_t *pThis)
{
	uchar *keyName;
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, ruleset);

	/* we must duplicate our name, as the key destructer would also
	 * free it, resulting in a double-free. It's also cleaner to have
	 * two separate copies.
	 */
	CHKmalloc(keyName = ustrdup(pThis->pszName));
	CHKiRet(llAppend(&llRulesets, keyName, pThis));

	/* this now also is the new current ruleset */
	pCurrRuleset = pThis;

	/* and also the default, if so far none has been set */
	if(pDfltRuleset == NULL)
		pDfltRuleset = pThis;

finalize_it:
	RETiRet;
}


/* destructor for the ruleset object */
BEGINobjDestruct(ruleset) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(ruleset)
	dbgprintf("destructing ruleset %p, name %p\n", pThis, pThis->pszName);
	llDestroy(&pThis->llRules);
	free(pThis->pszName);
ENDobjDestruct(ruleset)

/* this is a special destructor for the linkedList class. LinkedList does NOT
 * provide a pointer to the pointer, but rather the raw pointer itself. So we 
 * must map this, otherwise the destructor will abort.
 */
static rsRetVal
rulesetDestructForLinkedList(void *pData)
{
	ruleset_t *pThis = (ruleset_t*) pData;
	return rulesetDestruct(&pThis);
}


/* destruct ALL rule sets that reside in the system. This must
 * be callable before unloading this module as the module may
 * not be unloaded before unload of the actions is required. This is
 * kind of a left-over from previous logic and may be optimized one
 * everything runs stable again. -- rgerhards, 2009-06-10
 */
static rsRetVal
destructAllActions(void)
{
	DEFiRet;

	CHKiRet(llDestroy(&llRulesets));
	CHKiRet(llInit(&llRulesets, rulesetDestructForLinkedList, keyDestruct, strcasecmp));
	pDfltRuleset = NULL;

finalize_it:
	RETiRet;
}

/* helper for debugPrint(), initiates rule printing */
DEFFUNC_llExecFunc(doDebugPrintRule)
{
	return rule.DebugPrint((rule_t*) pData);
}
/* debugprint for the ruleset object */
BEGINobjDebugPrint(ruleset) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDebugPrint(ruleset)
	dbgoprint((obj_t*) pThis, "rsyslog ruleset %s:\n", pThis->pszName);
	llExecFunc(&pThis->llRules, doDebugPrintRule, NULL);
ENDobjDebugPrint(ruleset)


/* helper for debugPrintAll(), prints a single ruleset */
DEFFUNC_llExecFunc(doDebugPrintAll)
{
	return rulesetDebugPrint((ruleset_t*) pData);
}
/* debug print all rulesets
 */
static rsRetVal
debugPrintAll(void)
{
	DEFiRet;
	dbgprintf("All Rulesets:\n");
	llExecFunc(&llRulesets, doDebugPrintAll, NULL);
	dbgprintf("End of Rulesets.\n");
	RETiRet;
}


/* queryInterface function
 * rgerhards, 2008-02-21
 */
BEGINobjQueryInterface(ruleset)
CODESTARTobjQueryInterface(ruleset)
	if(pIf->ifVersion != rulesetCURR_IF_VERSION) { /* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->Construct = rulesetConstruct;
	pIf->ConstructFinalize = rulesetConstructFinalize;
	pIf->Destruct = rulesetDestruct;
	pIf->DebugPrint = rulesetDebugPrint;

	pIf->IterateAllActions = iterateAllActions;
	pIf->DestructAllActions = destructAllActions;
	pIf->AddRule = addRule;
	pIf->ProcessMsg = processMsg;
	pIf->SetName = setName;
	pIf->DebugPrintAll = debugPrintAll;
	pIf->GetCurrent = GetCurrent;
	pIf->GetRuleset = GetRuleset;
	pIf->SetDefaultRuleset = SetDefaultRuleset;
	pIf->SetCurrRuleset = SetCurrRuleset;
finalize_it:
ENDobjQueryInterface(ruleset)


/* Exit the ruleset class.
 * rgerhards, 2009-04-06
 */
BEGINObjClassExit(ruleset, OBJ_IS_CORE_MODULE) /* class, version */
	llDestroy(&llRulesets);
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(rule, CORE_COMPONENT);
ENDObjClassExit(ruleset)


/* Initialize the ruleset class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINObjClassInit(ruleset, 1, OBJ_IS_CORE_MODULE) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(rule, CORE_COMPONENT));

	/* set our own handlers */
	OBJSetMethodHandler(objMethod_DEBUGPRINT, rulesetDebugPrint);
	OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, rulesetConstructFinalize);

	/* prepare global data */
	CHKiRet(llInit(&llRulesets, rulesetDestructForLinkedList, keyDestruct, strcasecmp));
ENDObjClassInit(ruleset)

/* vi:set ai:
 */
