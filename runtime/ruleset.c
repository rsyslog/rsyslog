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
 * Copyright 2009-2013 Rainer Gerhards and Adiscon GmbH.
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
static rsRetVal processBatch(batch_t *pBatch);
static rsRetVal scriptExec(struct cnfstmt *root, batch_t *pBatch, sbool *active);


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


/* This function is similar to processBatch(), but works on a batch that
 * contains rules from multiple rulesets. In this case, we can not push
 * the whole batch through the ruleset. Instead, we examine it and
 * partition it into sub-rulesets which we then push through the system.
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
	int bHaveUnprocessed;	/* do we (still) have unprocessed entries? (loop term predicate) */
	DEFiRet;

	do {
		bHaveUnprocessed = 0;
		/* search for first unprocessed element */
		for(iStart = 0 ; iStart < pBatch->nElem && pBatch->eltState[iStart] == BATCH_STATE_DISC ; ++iStart)
			/* just search, no action */;
		if(iStart == pBatch->nElem)
			break; /* everything processed */

		/* prepare temporary batch */
		CHKiRet(batchInit(&snglRuleBatch, pBatch->nElem));
		snglRuleBatch.pbShutdownImmediate = pBatch->pbShutdownImmediate;
		currRuleset = batchElemGetRuleset(pBatch, iStart);
		iNew = 0;
		for(i = iStart ; i < pBatch->nElem ; ++i) {
			if(batchElemGetRuleset(pBatch, i) == currRuleset) {
				/* for performance reasons, we copy only those members that we actually need */
				snglRuleBatch.pElem[iNew].pMsg = pBatch->pElem[i].pMsg;
				snglRuleBatch.eltState[iNew] = pBatch->eltState[i];
				++iNew;
				/* We indicate the element also as done, so it will not be processed again */
				pBatch->eltState[i] = BATCH_STATE_DISC;
			} else {
				bHaveUnprocessed = 1;
			}
		}
		snglRuleBatch.nElem = iNew; /* was left just right by the for loop */
		batchSetSingleRuleset(&snglRuleBatch, 1);
		/* process temp batch */
		processBatch(&snglRuleBatch);
		batchFree(&snglRuleBatch);
	} while(bHaveUnprocessed == 1);

finalize_it:
	RETiRet;
}

/* return a new "active" structure for the batch. Free with freeActive(). */
static inline sbool *newActive(batch_t *pBatch)
{
	return malloc(sizeof(sbool) * batchNumMsgs(pBatch));
	
}
static inline void freeActive(sbool *active) { free(active); }


/* for details, see scriptExec() header comment! */
/* call action for all messages with filter on */
static rsRetVal
execAct(struct cnfstmt *stmt, batch_t *pBatch, sbool *active)
{
	DEFiRet;
dbgprintf("RRRR: execAct [%s]: batch of %d elements, active %p\n", modGetName(stmt->d.act->pMod), batchNumMsgs(pBatch), active);
	pBatch->active = active;
	stmt->d.act->submitToActQ(stmt->d.act, pBatch);
	RETiRet;
}

static rsRetVal
execSet(struct cnfstmt *stmt, batch_t *pBatch, sbool *active)
{
	int i;
	struct var result;
	DEFiRet;
	for(i = 0 ; i < batchNumMsgs(pBatch) && !*(pBatch->pbShutdownImmediate) ; ++i) {
		if(   pBatch->eltState[i] != BATCH_STATE_DISC
		   && (active == NULL || active[i])) {
			cnfexprEval(stmt->d.s_set.expr, &result, pBatch->pElem[i].pMsg);
			msgSetJSONFromVar(pBatch->pElem[i].pMsg, stmt->d.s_set.varname,
					  &result);
			varDelete(&result);
		}
	}
	RETiRet;
}

static rsRetVal
execUnset(struct cnfstmt *stmt, batch_t *pBatch, sbool *active)
{
	int i;
	DEFiRet;
	for(i = 0 ; i < batchNumMsgs(pBatch) && !*(pBatch->pbShutdownImmediate) ; ++i) {
		if(   pBatch->eltState[i] != BATCH_STATE_DISC
		   && (active == NULL || active[i])) {
			msgUnsetJSON(pBatch->pElem[i].pMsg, stmt->d.s_unset.varname);
		}
	}
	RETiRet;
}

/* for details, see scriptExec() header comment! */
/* "stop" simply discards the filtered items - it's just a (hopefully more intuitive
 * shortcut for users.
 */
static rsRetVal
execStop(batch_t *pBatch, sbool *active)
{
	int i;
	DEFiRet;
	for(i = 0 ; i < batchNumMsgs(pBatch) && !*(pBatch->pbShutdownImmediate) ; ++i) {
		if(   pBatch->eltState[i] != BATCH_STATE_DISC
		   && (active == NULL || active[i])) {
			pBatch->eltState[i] = BATCH_STATE_DISC;
		}
	}
	RETiRet;
}

/* for details, see scriptExec() header comment! */
// save current filter, evaluate new one
// perform then (if any message)
// if ELSE given:
//    set new filter, inverted
//    perform else (if any messages)
static rsRetVal
execIf(struct cnfstmt *stmt, batch_t *pBatch, sbool *active)
{
	sbool *newAct;
	int i;
	sbool bRet;
	sbool allInactive = 1;
	DEFiRet;
	newAct = newActive(pBatch);
	for(i = 0 ; i < batchNumMsgs(pBatch) ; ++i) {
		if(*(pBatch->pbShutdownImmediate))
			FINALIZE;
		if(pBatch->eltState[i] == BATCH_STATE_DISC)
			continue; /* will be ignored in any case */
		if(active == NULL || active[i]) {
			bRet = cnfexprEvalBool(stmt->d.s_if.expr, pBatch->pElem[i].pMsg);
			allInactive = 0;
		} else 
			bRet = 0;
		newAct[i] = bRet;
		DBGPRINTF("batch: item %d: expr eval: %d\n", i, bRet);
	}

	if(allInactive) {
		DBGPRINTF("execIf: all batch elements are inactive, holding execution\n");
		freeActive(newAct);
		FINALIZE;
	}

	if(stmt->d.s_if.t_then != NULL) {
		scriptExec(stmt->d.s_if.t_then, pBatch, newAct);
	}
	if(stmt->d.s_if.t_else != NULL) {
		for(i = 0 ; i < batchNumMsgs(pBatch) ; ++i) {
			if(*(pBatch->pbShutdownImmediate))
				FINALIZE;
			if(pBatch->eltState[i] != BATCH_STATE_DISC
			   && (active == NULL || active[i]))
				newAct[i] = !newAct[i];
			}
		scriptExec(stmt->d.s_if.t_else, pBatch, newAct);
	}
	freeActive(newAct);
finalize_it:
	RETiRet;
}

/* for details, see scriptExec() header comment! */
static void
execPRIFILT(struct cnfstmt *stmt, batch_t *pBatch, sbool *active)
{
	sbool *newAct;
	msg_t *pMsg;
	int bRet;
	int i;
	newAct = newActive(pBatch);
	for(i = 0 ; i < batchNumMsgs(pBatch) ; ++i) {
		if(*(pBatch->pbShutdownImmediate))
			return;
		if(pBatch->eltState[i] == BATCH_STATE_DISC)
			continue; /* will be ignored in any case */
		pMsg = pBatch->pElem[i].pMsg;
		if(active == NULL || active[i]) {
			if( (stmt->d.s_prifilt.pmask[pMsg->iFacility] == TABLE_NOPRI) ||
			   ((stmt->d.s_prifilt.pmask[pMsg->iFacility]
			            & (1<<pMsg->iSeverity)) == 0) )
				bRet = 0;
			else
				bRet = 1;
		} else 
			bRet = 0;
		newAct[i] = bRet;
		DBGPRINTF("batch: item %d PRIFILT %d\n", i, newAct[i]);
	}

	if(stmt->d.s_prifilt.t_then != NULL) {
		scriptExec(stmt->d.s_prifilt.t_then, pBatch, newAct);
	}
	if(stmt->d.s_prifilt.t_else != NULL) {
		for(i = 0 ; i < batchNumMsgs(pBatch) ; ++i) {
			if(*(pBatch->pbShutdownImmediate))
				return;
			if(pBatch->eltState[i] != BATCH_STATE_DISC
			   && (active == NULL || active[i]))
				newAct[i] = !newAct[i];
			}
		scriptExec(stmt->d.s_prifilt.t_else, pBatch, newAct);
	}
	freeActive(newAct);
}


/* helper to execPROPFILT(), as the evaluation itself is quite lengthy */
static int
evalPROPFILT(struct cnfstmt *stmt, msg_t *pMsg)
{
	unsigned short pbMustBeFreed;
	uchar *pszPropVal;
	int bRet = 0;
	rs_size_t propLen;

	if(stmt->d.s_propfilt.propID == PROP_INVALID)
		goto done;

	pszPropVal = MsgGetProp(pMsg, NULL, stmt->d.s_propfilt.propID,
				stmt->d.s_propfilt.propName, &propLen,
				&pbMustBeFreed, NULL);

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
		char *cstr;
		if(stmt->d.s_propfilt.propID == PROP_CEE) {
			cstr = es_str2cstr(stmt->d.s_propfilt.propName, NULL);
			DBGPRINTF("Filter: check for CEE property '%s' (value '%s') ",
				cstr, pszPropVal);
			free(cstr);
		} else {
			DBGPRINTF("Filter: check for property '%s' (value '%s') ",
				propIDToName(stmt->d.s_propfilt.propID), pszPropVal);
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

/* for details, see scriptExec() header comment! */
static void
execPROPFILT(struct cnfstmt *stmt, batch_t *pBatch, sbool *active)
{
	sbool *thenAct;
	sbool bRet;
	int i;
	thenAct = newActive(pBatch);
	for(i = 0 ; i < batchNumMsgs(pBatch) ; ++i) {
		if(*(pBatch->pbShutdownImmediate))
			return;
		if(pBatch->eltState[i] == BATCH_STATE_DISC)
			continue; /* will be ignored in any case */
		if(active == NULL || active[i]) {
			bRet = evalPROPFILT(stmt, pBatch->pElem[i].pMsg);
		} else 
			bRet = 0;
		thenAct[i] = bRet;
		DBGPRINTF("batch: item %d PROPFILT %d\n", i, thenAct[i]);
	}

	scriptExec(stmt->d.s_propfilt.t_then, pBatch, thenAct);
	freeActive(thenAct);
}

/* The rainerscript execution engine. It is debatable if that would be better
 * contained in grammer/rainerscript.c, HOWEVER, that file focusses primarily
 * on the parsing and object creation part. So as an actual executor, it is
 * better suited here.
 * param active: if NULL, all messages are active (to be processed), if non-null
 *               this is an array of the same size as the batch. If 1, the message
 *               is to be processed, otherwise not.
 * NOTE: this function must receive batches which contain a single ruleset ONLY!
 * rgerhards, 2012-09-04
 */
static rsRetVal
scriptExec(struct cnfstmt *root, batch_t *pBatch, sbool *active)
{
	DEFiRet;
	struct cnfstmt *stmt;

	for(stmt = root ; stmt != NULL ; stmt = stmt->next) {
		if(Debug) {
			dbgprintf("scriptExec: batch of %d elements, active %p, active[0]:%d\n",
				  batchNumMsgs(pBatch), active, (active == NULL ? 1 : active[0]));
			cnfstmtPrintOnly(stmt, 2, 0);
		}
		switch(stmt->nodetype) {
		case S_NOP:
			break;
		case S_STOP:
			execStop(pBatch, active);
			break;
		case S_ACT:
			execAct(stmt, pBatch, active);
			break;
		case S_SET:
			execSet(stmt, pBatch, active);
			break;
		case S_UNSET:
			execUnset(stmt, pBatch, active);
			break;
		case S_CALL:
			scriptExec(stmt->d.s_call.stmt, pBatch, active);
			break;
		case S_IF:
			execIf(stmt, pBatch, active);
			break;
		case S_PRIFILT:
			execPRIFILT(stmt, pBatch, active);
			break;
		case S_PROPFILT:
			execPROPFILT(stmt, pBatch, active);
			break;
		default:
			dbgprintf("error: unknown stmt type %u during exec\n",
				(unsigned) stmt->nodetype);
			break;
		}
	}
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

	DBGPRINTF("processBatch: batch of %d elements must be processed\n", pBatch->nElem);
	if(pBatch->bSingleRuleset) {
		pThis = batchGetRuleset(pBatch);
		if(pThis == NULL)
			pThis = ourConf->rulesets.pDflt;
		ISOBJ_TYPE_assert(pThis, ruleset);
		CHKiRet(scriptExec(pThis->root, pBatch, NULL));
	} else {
		CHKiRet(processBatchMultiRuleset(pBatch));
	}

finalize_it:
	DBGPRINTF("ruleset.ProcessMsg() returns %d\n", iRet);
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
	cnfstmtDestruct(pThis->root);
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
	struct cnfparamvals *queueParams;
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
		cnfstmtDestruct(o->script);
		ABORT_FINALIZE(RS_RET_RULESET_EXISTS);
	} else if(localRet != RS_RET_NOT_FOUND) {
		ABORT_FINALIZE(localRet);
	}
	CHKiRet(rulesetConstruct(&pRuleset));
	CHKiRet(rulesetSetName(pRuleset, rsName));
	CHKiRet(rulesetConstructFinalize(loadConf, pRuleset));
	addScript(pRuleset, o->script);

	/* we have only two params, so we do NOT do the usual param loop */
	parserIdx = cnfparamGetIdx(&rspblk, "parser");
	if(parserIdx != -1  && pvals[parserIdx].bUsed) {
		ar = pvals[parserIdx].val.d.ar;
		for(i = 0 ; i <  ar->nmemb ; ++i) {
			parserName = (uchar*)es_str2cstr(ar->arr[i], NULL);
			doRulesetAddParser(pRuleset, parserName);
			free(parserName);
		}
	}

	/* pick up ruleset queue parameters */
	qqueueDoCnfParams(o->nvlst, &queueParams);
	if(queueCnfParamsSet(queueParams)) {
		rsname = (pRuleset->pszName == NULL) ? (uchar*) "[ruleset]" : pRuleset->pszName;
		DBGPRINTF("adding a ruleset-specific \"main\" queue for ruleset '%s'\n", rsname);
		CHKiRet(createMainQueue(&pRuleset->pQueue, rsname, queueParams));
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
