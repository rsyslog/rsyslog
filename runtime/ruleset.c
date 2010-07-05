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
#include "cfsysline.h"
#include "msg.h"
#include "ruleset.h"
#include "rule.h"
#include "errmsg.h"
#include "parser.h"
#include "batch.h"
#include "unicode-helper.h"
#include "dirty.h" /* for main ruleset queue creation */

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(errmsg)
DEFobjCurrIf(rule)
DEFobjCurrIf(parser)

linkedList_t llRulesets; /* this is NOT a pointer - no typo here ;) */
ruleset_t *pCurrRuleset = NULL; /* currently "active" ruleset */
ruleset_t *pDfltRuleset = NULL; /* current default ruleset, e.g. for binding to actions which have no other */

/* forward definitions */
static rsRetVal processBatch(batch_t *pBatch);

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



/* helper to processBatch(), used to call the configured actions. It is
 * executed from within llExecFunc() of the action list.
 * rgerhards, 2007-08-02
 */
DEFFUNC_llExecFunc(processBatchDoRules)
{
	rsRetVal iRet;
	ISOBJ_TYPE_assert(pData, rule);
	dbgprintf("Processing next rule\n");
	iRet = rule.ProcessBatch((rule_t*) pData, (batch_t*) pParam);
dbgprintf("ruleset: get iRet %d from rule.ProcessMsg()\n", iRet);
	return iRet;
}



/* This function is similar to processBatch(), but works on a batch that
 * contains rules from multiple rulesets. In this case, we can not push
 * the whole batch through the ruleset. Instead, we examine it and
 * partition it into sub-rulesets which we then push through the system.
 * Note that when we evaluate which message must be processed, we do NOT need
 * to look at bFilterOK, because this value is only set in a later processing
 * stage. Doing so caused a bug during development ;)
 * rgerhards, 2010-06-15
 */
static inline rsRetVal
processBatchMultiRuleset(batch_t *pBatch)
{
	ruleset_t *currRuleset;
	batch_t snglRuleBatch;
	int i;
	int iStart;	/* start index of partial batch */
	int iNew;	/* index for new (temporary) batch */
	DEFiRet;

	CHKiRet(batchInit(&snglRuleBatch, pBatch->nElem));
	snglRuleBatch.pbShutdownImmediate = pBatch->pbShutdownImmediate;

	while(1) { /* loop broken inside */
		/* search for first unprocessed element */
		for(iStart = 0 ; iStart < pBatch->nElem && pBatch->pElem[iStart].state == BATCH_STATE_DISC ; ++iStart)
			/* just search, no action */;

		if(iStart == pBatch->nElem)
			FINALIZE; /* everything processed */

		/* prepare temporary batch */
		currRuleset = batchElemGetRuleset(pBatch, iStart);
		iNew = 0;
		for(i = iStart ; i < pBatch->nElem ; ++i) {
			if(batchElemGetRuleset(pBatch, i) == currRuleset) {
				batchCopyElem(&(snglRuleBatch.pElem[iNew++]), &(pBatch->pElem[i]));
				/* We indicate the element also as done, so it will not be processed again */
				pBatch->pElem[i].state = BATCH_STATE_DISC;
			}
		}
		snglRuleBatch.nElem = iNew; /* was left just right by the for loop */
		batchSetSingleRuleset(&snglRuleBatch, 1);
		/* process temp batch */
		processBatch(&snglRuleBatch);
	}
	batchFree(&snglRuleBatch);

finalize_it:
	RETiRet;
}

/* Process (consume) a batch of messages. Calls the actions configured.
 * If the whole batch uses a singel ruleset, we can process the batch as 
 * a whole. Otherwise, we need to process it slower, on a message-by-message
 * basis (what can be optimized to a per-ruleset basis)
 * rgerhards, 2005-10-13
 */
static rsRetVal
processBatch(batch_t *pBatch)
{
	ruleset_t *pThis;
	DEFiRet;
	assert(pBatch != NULL);

dbgprintf("ZZZ: processBatch: batch of %d elements must be processed\n", pBatch->nElem);
	if(pBatch->bSingleRuleset) {
		pThis = batchGetRuleset(pBatch);
		if(pThis == NULL)
			pThis = pDfltRuleset;
		ISOBJ_TYPE_assert(pThis, ruleset);
		CHKiRet(llExecFunc(&pThis->llRules, processBatchDoRules, pBatch));
	} else {
		CHKiRet(processBatchMultiRuleset(pBatch));
	}

finalize_it:
dbgprintf("ruleset.ProcessMsg() returns %d\n", iRet);
	RETiRet;
}


/* return the ruleset-assigned parser list. NULL means use the default
 * parser list.
 * rgerhards, 2009-11-04
 */
static parserList_t*
GetParserList(msg_t *pMsg)
{
	return (pMsg->pRuleset == NULL) ? pDfltRuleset->pParserLst : pMsg->pRuleset->pParserLst;
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


/* get main queue associated with ruleset. If no ruleset-specifc main queue
 * is set, the primary main message queue is returned.
 * We use a non-standard calling interface, as nothing can go wrong and it
 * is really much more natural to return the pointer directly.
 */
static qqueue_t*
GetRulesetQueue(ruleset_t *pThis)
{
	ISOBJ_TYPE_assert(pThis, ruleset);
	return (pThis->pQueue == NULL) ? pMsgQueue : pThis->pQueue;
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
	if(pThis->pQueue != NULL) {
		qqueueDestruct(&pThis->pQueue);
	}
	if(pThis->pParserLst != NULL) {
		parser.DestructParserList(&pThis->pParserLst);
	}
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


/* Create a ruleset-specific "main" queue for this ruleset. If one is already
 * defined, an error message is emitted but nothing else is done.
 * Note: we use the main message queue parameters for queue creation and access
 * syslogd.c directly to obtain these. This is far from being perfect, but
 * considered acceptable for the time being.
 * rgerhards, 2009-10-27
 */
static rsRetVal
rulesetCreateQueue(void __attribute__((unused)) *pVal, int *pNewVal)
{
	DEFiRet;

	if(pCurrRuleset == NULL) {
		errmsg.LogError(0, RS_RET_NO_CURR_RULESET, "error: currently no specific ruleset specified, thus a "
				"queue can not be added to it");
		ABORT_FINALIZE(RS_RET_NO_CURR_RULESET);
	}

	if(pCurrRuleset->pQueue != NULL) {
		errmsg.LogError(0, RS_RET_RULES_QUEUE_EXISTS, "error: ruleset already has a main queue, can not "
				"add another one");
		ABORT_FINALIZE(RS_RET_RULES_QUEUE_EXISTS);
	}

	if(pNewVal == 0)
		FINALIZE; /* if it is turned off, we do not need to change anything ;) */

	dbgprintf("adding a ruleset-specific \"main\" queue");
	CHKiRet(createMainQueue(&pCurrRuleset->pQueue, UCHAR_CONSTANT("ruleset")));

finalize_it:
	RETiRet;
}


/* Add a ruleset specific parser to the ruleset. Note that adding the first
 * parser automatically disables the default parsers. If they are needed as well,
 * the must be added via explicit config directives.
 * Note: this is the only spot in the code that requires the parser object. In order
 * to solve some class init bootstrap sequence problems, we get the object handle here
 * instead of during module initialization. Note that objUse() is capable of being 
 * called multiple times.
 * rgerhards, 2009-11-04
 */
static rsRetVal
rulesetAddParser(void __attribute__((unused)) *pVal, uchar *pName)
{
	parser_t *pParser;
	DEFiRet;

	assert(pCurrRuleset != NULL); 

	CHKiRet(objUse(parser, CORE_COMPONENT));
	iRet = parser.FindParser(&pParser, pName);
	if(iRet == RS_RET_PARSER_NOT_FOUND) {
		errmsg.LogError(0, RS_RET_PARSER_NOT_FOUND, "error: parser '%s' unknown at this time "
			  	"(maybe defined too late in rsyslog.conf?)", pName);
		ABORT_FINALIZE(RS_RET_NO_CURR_RULESET);
	} else if(iRet != RS_RET_OK) {
		errmsg.LogError(0, iRet, "error trying to find parser '%s'\n", pName);
		FINALIZE;
	}

	CHKiRet(parser.AddParserToList(&pCurrRuleset->pParserLst, pParser));

	dbgprintf("added parser '%s' to ruleset '%s'\n", pName, pCurrRuleset->pszName);
RUNLOG_VAR("%p", pCurrRuleset->pParserLst);

finalize_it:
	d_free(pName); /* no longer needed */

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
	pIf->ProcessBatch = processBatch;
	pIf->SetName = setName;
	pIf->DebugPrintAll = debugPrintAll;
	pIf->GetCurrent = GetCurrent;
	pIf->GetRuleset = GetRuleset;
	pIf->SetDefaultRuleset = SetDefaultRuleset;
	pIf->SetCurrRuleset = SetCurrRuleset;
	pIf->GetRulesetQueue = GetRulesetQueue;
	pIf->GetParserList = GetParserList;
finalize_it:
ENDobjQueryInterface(ruleset)


/* Exit the ruleset class.
 * rgerhards, 2009-04-06
 */
BEGINObjClassExit(ruleset, OBJ_IS_CORE_MODULE) /* class, version */
	llDestroy(&llRulesets);
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(rule, CORE_COMPONENT);
	objRelease(parser, CORE_COMPONENT);
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

	/* config file handlers */
	CHKiRet(regCfSysLineHdlr((uchar *)"rulesetparser", 0, eCmdHdlrGetWord, rulesetAddParser, NULL, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"rulesetcreatemainqueue", 0, eCmdHdlrBinary, rulesetCreateQueue, NULL, NULL));
ENDObjClassInit(ruleset)

/* vi:set ai:
 */
