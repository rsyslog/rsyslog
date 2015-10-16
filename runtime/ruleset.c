/* ruleset.c - rsyslog's ruleset object
 *
 * We have a two-way structure of linked lists: one config-specifc linked list
 * (conf->rulesets.llRulesets) hold alls rule sets that we know. Included in each
 * list is a list of rules (which contain a list of actions, but that's
 * a different story).
 *
 * Usually, only a single rule set is executed. However, there exist some
 * situations where all rules must be iterated over, for example on HUP. Thus,
 * we also provide interfaces to do that.
 *
 * Module begun 2009-06-10 by Rainer Gerhards
 *
 * Copyright 2009-2015 Rainer Gerhards and Adiscon GmbH.
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
#include <assert.h>
#include <ctype.h>

#include "rsyslog.h"
#include "obj.h"
#include "cfsysline.h"
#include "msg.h"
#include "ruleset.h"
#include "errmsg.h"
#include "parser.h"
#include "batch.h"
#include "unicode-helper.h"
#include "rsconf.h"
#include "action.h"
#include "rainerscript.h"
#include "srUtils.h"
#include "modules.h"
#include "wti.h"
#include "dirty.h" /* for main ruleset queue creation */

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(errmsg)
DEFobjCurrIf(parser)

/* tables for interfacing with the v6 config system (as far as we need to) */
static struct cnfparamdescr rspdescr[] = {
	{ "name", eCmdHdlrString, CNFPARAM_REQUIRED },
	{ "parser", eCmdHdlrArray, 0 }
};
static struct cnfparamblk rspblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(rspdescr)/sizeof(struct cnfparamdescr),
	  rspdescr
	};

/* forward definitions */
static rsRetVal processBatch(batch_t *pBatch, wti_t *pWti);
static rsRetVal scriptExec(struct cnfstmt *root, msg_t *pMsg, wti_t *pWti);


/* ---------- linked-list key handling functions (ruleset) ---------- */

/* destructor for linked list keys.
 */
rsRetVal
rulesetKeyDestruct(void __attribute__((unused)) *pData)
{
	free(pData);
	return RS_RET_OK;
}
/* ---------- END linked-list key handling functions (ruleset) ---------- */


/* iterate over all actions in a script (stmt subtree) */
static void
scriptIterateAllActions(struct cnfstmt *root, rsRetVal (*pFunc)(void*, void*), void* pParam)
{
	struct cnfstmt *stmt;
	for(stmt = root ; stmt != NULL ; stmt = stmt->next) {
		switch(stmt->nodetype) {
		case S_NOP:
		case S_STOP:
		case S_CALL:/* call does not need to do anything - done in called ruleset! */
			break;
		case S_ACT:
			DBGPRINTF("iterateAllActions calling into action %p\n", stmt->d.act);
			pFunc(stmt->d.act, pParam);
			break;
		case S_IF:
			if(stmt->d.s_if.t_then != NULL)
				scriptIterateAllActions(stmt->d.s_if.t_then,
							pFunc, pParam);
			if(stmt->d.s_if.t_else != NULL)
				scriptIterateAllActions(stmt->d.s_if.t_else,
							pFunc, pParam);
			break;
        case S_FOREACH:
			if(stmt->d.s_foreach.body != NULL)
				scriptIterateAllActions(stmt->d.s_foreach.body,
                                        pFunc, pParam);
			break;
		case S_PRIFILT:
			if(stmt->d.s_prifilt.t_then != NULL)
				scriptIterateAllActions(stmt->d.s_prifilt.t_then,
							pFunc, pParam);
			if(stmt->d.s_prifilt.t_else != NULL)
				scriptIterateAllActions(stmt->d.s_prifilt.t_else,
							pFunc, pParam);
			break;
		case S_PROPFILT:
			scriptIterateAllActions(stmt->d.s_propfilt.t_then,
						pFunc, pParam);
			break;
		default:
			dbgprintf("error: unknown stmt type %u during iterateAll\n",
				(unsigned) stmt->nodetype);
			break;
		}
	}
}

/* driver to iterate over all of this ruleset actions */
typedef struct iterateAllActions_s {
	rsRetVal (*pFunc)(void*, void*);
	void *pParam;
} iterateAllActions_t;
/* driver to iterate over all actions */
DEFFUNC_llExecFunc(doIterateAllActions)
{
	DEFiRet;
	ruleset_t* pThis = (ruleset_t*) pData;
	iterateAllActions_t *pMyParam = (iterateAllActions_t*) pParam;
	scriptIterateAllActions(pThis->root, pMyParam->pFunc, pMyParam->pParam);
	RETiRet;
}
/* iterate over ALL actions present in the WHOLE system.
 * this is often needed, for example when HUP processing 
 * must be done or a shutdown is pending.
 */
static rsRetVal
iterateAllActions(rsconf_t *conf, rsRetVal (*pFunc)(void*, void*), void* pParam)
{
	iterateAllActions_t params;
	DEFiRet;
	assert(pFunc != NULL);

	params.pFunc = pFunc;
	params.pParam = pParam;
	CHKiRet(llExecFunc(&(conf->rulesets.llRulesets), doIterateAllActions, &params));

finalize_it:
	RETiRet;
}

/* driver to iterate over all rulesets */
DEFFUNC_llExecFunc(doActivateRulesetQueues)
{
	DEFiRet;
	ruleset_t* pThis = (ruleset_t*) pData;
	dbgprintf("Activating Ruleset Queue[%p] for Ruleset %s\n",
		  pThis->pQueue, pThis->pszName);
	if(pThis->pQueue != NULL)
		startMainQueue(pThis->pQueue);
	RETiRet;
}
/* activate all ruleset queues */
rsRetVal
activateRulesetQueues()
{
	DEFiRet;

	llExecFunc(&(runConf->rulesets.llRulesets), doActivateRulesetQueues, NULL);

	RETiRet;
}


static rsRetVal
execAct(struct cnfstmt *stmt, msg_t *pMsg, wti_t *pWti)
{
	DEFiRet;
	if(stmt->d.act->bDisabled) {
		DBGPRINTF("action %d died, do NOT execute\n", stmt->d.act->iActionNbr);
		FINALIZE;
	}

	DBGPRINTF("executing action %d\n", stmt->d.act->iActionNbr);
	stmt->d.act->submitToActQ(stmt->d.act, pWti, pMsg);
	if(iRet != RS_RET_DISCARDMSG) {
		/* note: we ignore the error code here, as we do NEVER want to
		 * stop script execution due to action return code
		 */
		iRet = RS_RET_OK;
	}
finalize_it:
	RETiRet;
}

static rsRetVal
execSet(struct cnfstmt *stmt, msg_t *pMsg)
{
	struct var result;
	DEFiRet;
	cnfexprEval(stmt->d.s_set.expr, &result, pMsg);
	msgSetJSONFromVar(pMsg, stmt->d.s_set.varname, &result, stmt->d.s_set.force_reset);
	varDelete(&result);
	RETiRet;
}

static rsRetVal
execUnset(struct cnfstmt *stmt, msg_t *pMsg)
{
	DEFiRet;
	msgDelJSON(pMsg, stmt->d.s_unset.varname);
	RETiRet;
}

static rsRetVal
execCall(struct cnfstmt *stmt, msg_t *pMsg, wti_t *pWti)
{
	DEFiRet;
	if(stmt->d.s_call.ruleset == NULL) {
		CHKiRet(scriptExec(stmt->d.s_call.stmt, pMsg, pWti));
	} else {
		CHKmalloc(pMsg = MsgDup((msg_t*) pMsg));
		DBGPRINTF("CALL: forwarding message to async ruleset %p\n",
			  stmt->d.s_call.ruleset->pQueue);
		MsgSetFlowControlType(pMsg, eFLOWCTL_NO_DELAY);
		MsgSetRuleset(pMsg, stmt->d.s_call.ruleset);
		/* Note: we intentionally use submitMsg2() here, as we process messages
		 * that were already run through the rate-limiter.
		 */
		submitMsg2(pMsg);
	}
finalize_it:
	RETiRet;
}

static rsRetVal
execIf(struct cnfstmt *stmt, msg_t *pMsg, wti_t *pWti)
{
	sbool bRet;
	DEFiRet;
	bRet = cnfexprEvalBool(stmt->d.s_if.expr, pMsg);
	DBGPRINTF("if condition result is %d\n", bRet);
	if(bRet) {
		if(stmt->d.s_if.t_then != NULL)
			CHKiRet(scriptExec(stmt->d.s_if.t_then, pMsg, pWti));
	} else {
		if(stmt->d.s_if.t_else != NULL)
			CHKiRet(scriptExec(stmt->d.s_if.t_else, pMsg, pWti));
	}
finalize_it:
	RETiRet;
}

static rsRetVal
execForeach(struct cnfstmt *stmt, msg_t *pMsg, wti_t *pWti)
{
	json_object *arr = NULL;
	DEFiRet;
	arr = cnfexprEvalCollection(stmt->d.s_foreach.iter->collection, pMsg);
	if (arr == NULL || !json_object_is_type(arr, json_type_array)) {
		DBGPRINTF("foreach loop skipped, as object to iterate upon is either empty or not an array\n");
		FINALIZE;
	}
	int len = json_object_array_length(arr);
	json_object *curr;
	for (int i = 0; i < len; i++) {
		curr = json_object_array_get_idx(arr, i);
		struct var v;
		v.d.json = curr;
		v.datatype = 'J';
		CHKiRet(msgSetJSONFromVar(pMsg, (uchar*)stmt->d.s_foreach.iter->var, &v, 1));
		CHKiRet(scriptExec(stmt->d.s_foreach.body, pMsg, pWti));
	}
	CHKiRet(msgDelJSON(pMsg, (uchar*)stmt->d.s_foreach.iter->var));
finalize_it:
	if (arr != NULL) json_object_put(arr);
	RETiRet;
}

static rsRetVal
execPRIFILT(struct cnfstmt *stmt, msg_t *pMsg, wti_t *pWti)
{
	int bRet;
	DEFiRet;
	if( (stmt->d.s_prifilt.pmask[pMsg->iFacility] == TABLE_NOPRI) ||
	   ((stmt->d.s_prifilt.pmask[pMsg->iFacility]
		    & (1<<pMsg->iSeverity)) == 0) )
		bRet = 0;
	else
		bRet = 1;

	DBGPRINTF("PRIFILT condition result is %d\n", bRet);
	if(bRet) {
		if(stmt->d.s_prifilt.t_then != NULL)
			CHKiRet(scriptExec(stmt->d.s_prifilt.t_then, pMsg, pWti));
	} else {
		if(stmt->d.s_prifilt.t_else != NULL)
			CHKiRet(scriptExec(stmt->d.s_prifilt.t_else, pMsg, pWti));
	}
finalize_it:
	RETiRet;
}


/* helper to execPROPFILT(), as the evaluation itself is quite lengthy */
static int
evalPROPFILT(struct cnfstmt *stmt, msg_t *pMsg)
{
	unsigned short pbMustBeFreed;
	uchar *pszPropVal;
	int bRet = 0;
	rs_size_t propLen;

	if(stmt->d.s_propfilt.prop.id == PROP_INVALID)
		goto done;

	pszPropVal = MsgGetProp(pMsg, NULL, &stmt->d.s_propfilt.prop,
				&propLen, &pbMustBeFreed, NULL);

	/* Now do the compares (short list currently ;)) */
	switch(stmt->d.s_propfilt.operation ) {
	case FIOP_CONTAINS:
		if(rsCStrLocateInSzStr(stmt->d.s_propfilt.pCSCompValue, (uchar*) pszPropVal) != -1)
			bRet = 1;
		break;
	case FIOP_ISEMPTY:
		if(propLen == 0)
			bRet = 1; /* process message! */
		break;
	case FIOP_ISEQUAL:
		if(rsCStrSzStrCmp(stmt->d.s_propfilt.pCSCompValue,
				  pszPropVal, propLen) == 0)
			bRet = 1; /* process message! */
		break;
	case FIOP_STARTSWITH:
		if(rsCStrSzStrStartsWithCStr(stmt->d.s_propfilt.pCSCompValue,
				  pszPropVal, propLen) == 0)
			bRet = 1; /* process message! */
		break;
	case FIOP_REGEX:
		if(rsCStrSzStrMatchRegex(stmt->d.s_propfilt.pCSCompValue,
				(unsigned char*) pszPropVal, 0, &stmt->d.s_propfilt.regex_cache) == RS_RET_OK)
			bRet = 1;
		break;
	case FIOP_EREREGEX:
		if(rsCStrSzStrMatchRegex(stmt->d.s_propfilt.pCSCompValue,
				  (unsigned char*) pszPropVal, 1, &stmt->d.s_propfilt.regex_cache) == RS_RET_OK)
			bRet = 1;
		break;
	default:
		/* here, it handles NOP (for performance reasons) */
		assert(stmt->d.s_propfilt.operation == FIOP_NOP);
		bRet = 1; /* as good as any other default ;) */
		break;
	}

	/* now check if the value must be negated */
	if(stmt->d.s_propfilt.isNegated)
		bRet = (bRet == 1) ?  0 : 1;

	if(Debug) {
		if(stmt->d.s_propfilt.prop.id == PROP_CEE) {
			DBGPRINTF("Filter: check for CEE property '%s' (value '%s') ",
				stmt->d.s_propfilt.prop.name, pszPropVal);
		} else if(stmt->d.s_propfilt.prop.id == PROP_LOCAL_VAR) {
			DBGPRINTF("Filter: check for local var '%s' (value '%s') ",
				stmt->d.s_propfilt.prop.name, pszPropVal);
		} else if(stmt->d.s_propfilt.prop.id == PROP_GLOBAL_VAR) {
			DBGPRINTF("Filter: check for global var '%s' (value '%s') ",
				stmt->d.s_propfilt.prop.name, pszPropVal);
		} else {
			DBGPRINTF("Filter: check for property '%s' (value '%s') ",
				propIDToName(stmt->d.s_propfilt.prop.id), pszPropVal);
		}
		if(stmt->d.s_propfilt.isNegated)
			DBGPRINTF("NOT ");
		if(stmt->d.s_propfilt.operation == FIOP_ISEMPTY) {
			DBGPRINTF("%s : %s\n",
			       getFIOPName(stmt->d.s_propfilt.operation),
			       bRet ? "TRUE" : "FALSE");
		} else {
			DBGPRINTF("%s '%s': %s\n",
			       getFIOPName(stmt->d.s_propfilt.operation),
			       rsCStrGetSzStrNoNULL(stmt->d.s_propfilt.pCSCompValue),
			       bRet ? "TRUE" : "FALSE");
		}
	}

	/* cleanup */
	if(pbMustBeFreed)
		free(pszPropVal);
done:
	return bRet;
}

static rsRetVal
execPROPFILT(struct cnfstmt *stmt, msg_t *pMsg, wti_t *pWti)
{
	sbool bRet;
	DEFiRet;

	bRet = evalPROPFILT(stmt, pMsg);
	DBGPRINTF("PROPFILT condition result is %d\n", bRet);
	if(bRet)
		CHKiRet(scriptExec(stmt->d.s_propfilt.t_then, pMsg, pWti));
finalize_it:
	RETiRet;
}

/* The rainerscript execution engine. It is debatable if that would be better
 * contained in grammer/rainerscript.c, HOWEVER, that file focusses primarily
 * on the parsing and object creation part. So as an actual executor, it is
 * better suited here.
 * rgerhards, 2012-09-04
 */
static rsRetVal
scriptExec(struct cnfstmt *root, msg_t *pMsg, wti_t *pWti)
{
	struct cnfstmt *stmt;
	DEFiRet;

	for(stmt = root ; stmt != NULL ; stmt = stmt->next) {
		if(*pWti->pbShutdownImmediate) {
			DBGPRINTF("scriptExec: ShutdownImmediate set, "
				  "force terminating\n");	
			ABORT_FINALIZE(RS_RET_FORCE_TERM);
		}
		if(Debug) {
			cnfstmtPrintOnly(stmt, 2, 0);
		}
		switch(stmt->nodetype) {
		case S_NOP:
			break;
		case S_STOP:
			ABORT_FINALIZE(RS_RET_DISCARDMSG);
			break;
		case S_ACT:
			CHKiRet(execAct(stmt, pMsg, pWti));
			break;
		case S_SET:
			CHKiRet(execSet(stmt, pMsg));
			break;
		case S_UNSET:
			CHKiRet(execUnset(stmt, pMsg));
			break;
		case S_CALL:
			CHKiRet(execCall(stmt, pMsg, pWti));
			break;
		case S_IF:
			CHKiRet(execIf(stmt, pMsg, pWti));
			break;
		case S_FOREACH:
			CHKiRet(execForeach(stmt, pMsg, pWti));
			break;
		case S_PRIFILT:
			CHKiRet(execPRIFILT(stmt, pMsg, pWti));
			break;
		case S_PROPFILT:
			CHKiRet(execPROPFILT(stmt, pMsg, pWti));
			break;
		default:
			dbgprintf("error: unknown stmt type %u during exec\n",
				(unsigned) stmt->nodetype);
			break;
		}
	}
finalize_it:
	RETiRet;
}


/* Process (consume) a batch of messages. Calls the actions configured.
 * This is called by MAIN queues.
 */
static rsRetVal
processBatch(batch_t *pBatch, wti_t *pWti)
{
	int i;
	msg_t *pMsg;
	ruleset_t *pRuleset;
	DEFiRet;

	DBGPRINTF("processBATCH: batch of %d elements must be processed\n", pBatch->nElem);

	wtiResetExecState(pWti, pBatch);

	/* execution phase */
	for(i = 0 ; i < batchNumMsgs(pBatch) && !*(pWti->pbShutdownImmediate) ; ++i) {
		pMsg = pBatch->pElem[i].pMsg;
		DBGPRINTF("processBATCH: next msg %d: %.128s\n", i, pMsg->pszRawMsg);
		pRuleset = (pMsg->pRuleset == NULL) ? ourConf->rulesets.pDflt : pMsg->pRuleset;
		scriptExec(pRuleset->root, pMsg, pWti);
		// TODO: think if we need a return state of scriptExec - most probably
		// the answer is "no", as we need to process the batch in any case!
		// TODO: we must refactor this!  flag messages as committed
		batchSetElemState(pBatch, i, BATCH_STATE_COMM);
	}

	/* commit phase */
	dbgprintf("END batch execution phase, entering to commit phase\n");
	actionCommitAllDirect(pWti);

	DBGPRINTF("processBATCH: batch of %d elements has been processed\n", pBatch->nElem);
	RETiRet;
}


/* return the ruleset-assigned parser list. NULL means use the default
 * parser list.
 * rgerhards, 2009-11-04
 */
static parserList_t*
GetParserList(rsconf_t *conf, msg_t *pMsg)
{
	return (pMsg->pRuleset == NULL) ? conf->rulesets.pDflt->pParserLst : pMsg->pRuleset->pParserLst;
}


/* Add a script block to the current ruleset */
static void
addScript(ruleset_t *pThis, struct cnfstmt *script)
{
	if(pThis->last == NULL)
		pThis->root = pThis->last = script;
	else {
		pThis->last->next = script;
		pThis->last = script;
	}
}


/* set name for ruleset */
static rsRetVal rulesetSetName(ruleset_t *pThis, uchar *pszName)
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
GetCurrent(rsconf_t *conf)
{
	return conf->rulesets.pCurr;
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
rsRetVal
rulesetGetRuleset(rsconf_t *conf, ruleset_t **ppRuleset, uchar *pszName)
{
	DEFiRet;
	assert(ppRuleset != NULL);
	assert(pszName != NULL);

	CHKiRet(llFind(&(conf->rulesets.llRulesets), pszName, (void*) ppRuleset));

finalize_it:
	RETiRet;
}


/* Set a new default rule set. If the default can not be found, no change happens.
 */
static rsRetVal
SetDefaultRuleset(rsconf_t *conf, uchar *pszName)
{
	ruleset_t *pRuleset;
	DEFiRet;
	assert(pszName != NULL);

	CHKiRet(rulesetGetRuleset(conf, &pRuleset, pszName));
	conf->rulesets.pDflt = pRuleset;
	DBGPRINTF("default rule set changed to %p: '%s'\n", pRuleset, pszName);

finalize_it:
	RETiRet;
}


/* Set a new current rule set. If the ruleset can not be found, no change happens */
static rsRetVal
SetCurrRuleset(rsconf_t *conf, uchar *pszName)
{
	ruleset_t *pRuleset;
	DEFiRet;
	assert(pszName != NULL);

	CHKiRet(rulesetGetRuleset(conf, &pRuleset, pszName));
	conf->rulesets.pCurr = pRuleset;
	DBGPRINTF("current rule set changed to %p: '%s'\n", pRuleset, pszName);

finalize_it:
	RETiRet;
}


/* Standard-Constructor
 */
BEGINobjConstruct(ruleset) /* be sure to specify the object type also in END macro! */
	pThis->root = NULL;
	pThis->last = NULL;
ENDobjConstruct(ruleset)


/* ConstructionFinalizer
 * This also adds the rule set to the list of all known rulesets.
 */
static rsRetVal
rulesetConstructFinalize(rsconf_t *conf, ruleset_t *pThis)
{
	uchar *keyName;
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, ruleset);

	/* we must duplicate our name, as the key destructer would also
	 * free it, resulting in a double-free. It's also cleaner to have
	 * two separate copies.
	 */
	CHKmalloc(keyName = ustrdup(pThis->pszName));
	CHKiRet(llAppend(&(conf->rulesets.llRulesets), keyName, pThis));

	/* and also the default, if so far none has been set */
	if(conf->rulesets.pDflt == NULL)
		conf->rulesets.pDflt = pThis;

finalize_it:
	RETiRet;
}


/* destructor for the ruleset object */
BEGINobjDestruct(ruleset) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(ruleset)
	DBGPRINTF("destructing ruleset %p, name %p\n", pThis, pThis->pszName);
	if(pThis->pQueue != NULL) {
		qqueueDestruct(&pThis->pQueue);
	}
	if(pThis->pParserLst != NULL) {
		parser.DestructParserList(&pThis->pParserLst);
	}
	free(pThis->pszName);
	cnfstmtDestructLst(pThis->root);
ENDobjDestruct(ruleset)


/* destruct ALL rule sets that reside in the system. This must
 * be callable before unloading this module as the module may
 * not be unloaded before unload of the actions is required. This is
 * kind of a left-over from previous logic and may be optimized one
 * everything runs stable again. -- rgerhards, 2009-06-10
 */
static rsRetVal
destructAllActions(rsconf_t *conf)
{
	DEFiRet;

	CHKiRet(llDestroy(&(conf->rulesets.llRulesets)));
	CHKiRet(llInit(&(conf->rulesets.llRulesets), rulesetDestructForLinkedList, rulesetKeyDestruct, strcasecmp));
	conf->rulesets.pDflt = NULL;

finalize_it:
	RETiRet;
}

/* this is a special destructor for the linkedList class. LinkedList does NOT
 * provide a pointer to the pointer, but rather the raw pointer itself. So we 
 * must map this, otherwise the destructor will abort.
 */
rsRetVal
rulesetDestructForLinkedList(void *pData)
{
	ruleset_t *pThis = (ruleset_t*) pData;
	return rulesetDestruct(&pThis);
}

/* debugprint for the ruleset object */
BEGINobjDebugPrint(ruleset) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDebugPrint(ruleset)
	dbgoprint((obj_t*) pThis, "rsyslog ruleset %s:\n", pThis->pszName);
	cnfstmtPrint(pThis->root, 0);
	dbgoprint((obj_t*) pThis, "ruleset %s assigned parser list:\n", pThis->pszName);
	printParserList(pThis->pParserLst);
ENDobjDebugPrint(ruleset)


/* helper for debugPrintAll(), prints a single ruleset */
DEFFUNC_llExecFunc(doDebugPrintAll)
{
	return rulesetDebugPrint((ruleset_t*) pData);
}
/* debug print all rulesets
 */
static rsRetVal
debugPrintAll(rsconf_t *conf)
{
	DEFiRet;
	dbgprintf("All Rulesets:\n");
	llExecFunc(&(conf->rulesets.llRulesets), doDebugPrintAll, NULL);
	dbgprintf("End of Rulesets.\n");
	RETiRet;
}

static inline void
rulesetOptimize(ruleset_t *pRuleset)
{
	if(Debug) {
		dbgprintf("ruleset '%s' before optimization:\n",
			  pRuleset->pszName);
		rulesetDebugPrint((ruleset_t*) pRuleset);
	}
	cnfstmtOptimize(pRuleset->root);
	if(Debug) {
		dbgprintf("ruleset '%s' after optimization:\n",
			  pRuleset->pszName);
		rulesetDebugPrint((ruleset_t*) pRuleset);
	}
}

/* helper for rulsetOptimizeAll(), optimizes a single ruleset */
DEFFUNC_llExecFunc(doRulesetOptimizeAll)
{
	rulesetOptimize((ruleset_t*) pData);
	return RS_RET_OK;
}
/* optimize all rulesets
 */
rsRetVal
rulesetOptimizeAll(rsconf_t *conf)
{
	DEFiRet;
	dbgprintf("begin ruleset optimization phase\n");
	llExecFunc(&(conf->rulesets.llRulesets), doRulesetOptimizeAll, NULL);
	dbgprintf("ruleset optimization phase finished.\n");
	RETiRet;
}


/* Create a ruleset-specific "main" queue for this ruleset. If one is already
 * defined, an error message is emitted but nothing else is done.
 * Note: we use the main message queue parameters for queue creation and access
 * syslogd.c directly to obtain these. This is far from being perfect, but
 * considered acceptable for the time being.
 * rgerhards, 2009-10-27
 */
static inline rsRetVal
doRulesetCreateQueue(rsconf_t *conf, int *pNewVal)
{
	uchar *rsname;
	DEFiRet;

	if(conf->rulesets.pCurr == NULL) {
		errmsg.LogError(0, RS_RET_NO_CURR_RULESET, "error: currently no specific ruleset specified, thus a "
				"queue can not be added to it");
		ABORT_FINALIZE(RS_RET_NO_CURR_RULESET);
	}

	if(conf->rulesets.pCurr->pQueue != NULL) {
		errmsg.LogError(0, RS_RET_RULES_QUEUE_EXISTS, "error: ruleset already has a main queue, can not "
				"add another one");
		ABORT_FINALIZE(RS_RET_RULES_QUEUE_EXISTS);
	}

	if(pNewVal == 0)
		FINALIZE; /* if it is turned off, we do not need to change anything ;) */

	rsname = (conf->rulesets.pCurr->pszName == NULL) ? (uchar*) "[ruleset]" : conf->rulesets.pCurr->pszName;
	DBGPRINTF("adding a ruleset-specific \"main\" queue for ruleset '%s'\n", rsname);
	CHKiRet(createMainQueue(&conf->rulesets.pCurr->pQueue, rsname, NULL));

finalize_it:
	RETiRet;
}

static rsRetVal
rulesetCreateQueue(void __attribute__((unused)) *pVal, int *pNewVal)
{
	return doRulesetCreateQueue(ourConf, pNewVal);
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
doRulesetAddParser(ruleset_t *pRuleset, uchar *pName)
{
	parser_t *pParser;
	DEFiRet;

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

	CHKiRet(parser.AddParserToList(&pRuleset->pParserLst, pParser));

	DBGPRINTF("added parser '%s' to ruleset '%s'\n", pName, pRuleset->pszName);

finalize_it:
	d_free(pName); /* no longer needed */

	RETiRet;
}

static rsRetVal
rulesetAddParser(void __attribute__((unused)) *pVal, uchar *pName)
{
	return doRulesetAddParser(ourConf->rulesets.pCurr, pName);
}


/* Process ruleset() objects */
rsRetVal
rulesetProcessCnf(struct cnfobj *o)
{
	struct cnfparamvals *pvals;
	rsRetVal localRet;
	uchar *rsName = NULL;
	uchar *parserName;
	int nameIdx, parserIdx;
	ruleset_t *pRuleset;
	struct cnfarray *ar;
	int i;
	uchar *rsname;
	DEFiRet;

	pvals = nvlstGetParams(o->nvlst, &rspblk, NULL);
	if(pvals == NULL) {
		ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
	}
	DBGPRINTF("ruleset param blk after rulesetProcessCnf:\n");
	cnfparamsPrint(&rspblk, pvals);
	nameIdx = cnfparamGetIdx(&rspblk, "name");
	rsName = (uchar*)es_str2cstr(pvals[nameIdx].val.d.estr, NULL);
	
	localRet = rulesetGetRuleset(loadConf, &pRuleset, rsName);
	if(localRet == RS_RET_OK) {
		errmsg.LogError(0, RS_RET_RULESET_EXISTS,
			"error: ruleset '%s' specified more than once",
			rsName);
		cnfstmtDestructLst(o->script);
		ABORT_FINALIZE(RS_RET_RULESET_EXISTS);
	} else if(localRet != RS_RET_NOT_FOUND) {
		ABORT_FINALIZE(localRet);
	}

	CHKiRet(rulesetConstruct(&pRuleset));
	if((localRet = rulesetSetName(pRuleset, rsName)) != RS_RET_OK) {
		rulesetDestruct(&pRuleset);
		ABORT_FINALIZE(localRet);
	}
	if((localRet = rulesetConstructFinalize(loadConf, pRuleset)) != RS_RET_OK) {
		rulesetDestruct(&pRuleset);
		ABORT_FINALIZE(localRet);
	}
	addScript(pRuleset, o->script);

	/* we have only two params, so we do NOT do the usual param loop */
	parserIdx = cnfparamGetIdx(&rspblk, "parser");
	if(parserIdx != -1  && pvals[parserIdx].bUsed) {
		ar = pvals[parserIdx].val.d.ar;
		for(i = 0 ; i <  ar->nmemb ; ++i) {
			parserName = (uchar*)es_str2cstr(ar->arr[i], NULL);
			doRulesetAddParser(pRuleset, parserName);
			/* note parserName is freed in doRulesetAddParser()! */
		}
	}

	/* pick up ruleset queue parameters */
	if(queueCnfParamsSet(o->nvlst)) {
		rsname = (pRuleset->pszName == NULL) ? (uchar*) "[ruleset]" : pRuleset->pszName;
		DBGPRINTF("adding a ruleset-specific \"main\" queue for ruleset '%s'\n", rsname);
		CHKiRet(createMainQueue(&pRuleset->pQueue, rsname, o->nvlst));
	}

finalize_it:
	free(rsName);
	cnfparamvalsDestruct(pvals, &rspblk);
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
	pIf->AddScript = addScript;
	pIf->ProcessBatch = processBatch;
	pIf->SetName = rulesetSetName;
	pIf->DebugPrintAll = debugPrintAll;
	pIf->GetCurrent = GetCurrent;
	pIf->GetRuleset = rulesetGetRuleset;
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
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(parser, CORE_COMPONENT);
ENDObjClassExit(ruleset)


/* Initialize the ruleset class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINObjClassInit(ruleset, 1, OBJ_IS_CORE_MODULE) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(errmsg, CORE_COMPONENT));

	/* set our own handlers */
	OBJSetMethodHandler(objMethod_DEBUGPRINT, rulesetDebugPrint);
	OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, rulesetConstructFinalize);

	/* config file handlers */
	CHKiRet(regCfSysLineHdlr((uchar *)"rulesetparser", 0, eCmdHdlrGetWord, rulesetAddParser, NULL, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"rulesetcreatemainqueue", 0, eCmdHdlrBinary, rulesetCreateQueue, NULL, NULL));
ENDObjClassInit(ruleset)

/* vi:set ai:
 */
