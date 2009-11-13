/* apc.c - asynchronous procedure call support
 *
 * An asynchronous procedure call (APC) is a procedure call (guess what) that is potentially run
 * asynchronously to its main thread. It can be scheduled to occur at a caller-provided time.
 * As long as the procedure has not been called, the APC entry may be modified by the caller
 * or deleted. It is the caller's purpose to make sure proper synchronization is in place.
 * The APC object only case about APC's own control structures (which *are* properly 
 * guarded by synchronization primitives).
 *
 * Module begun 2009-06-15 by Rainer Gerhards
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
#include <pthread.h>

#include "rsyslog.h"
#include "obj.h"
#include "apc.h"
#include "srUtils.h"
#include "datetime.h"

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(datetime)

/* following is a used to implement a monotonically increasing id for the apcs. That
 * ID can be used to cancel an apc request. Note that the ID is generated with modulo
 * arithmetic, so at some point, it will wrap. Howerver, this happens at 2^32-1 at
 * earliest, so this is not considered a problem.
 */
apc_id_t apcID = 0;

/* private data structures */

/* the apc list and its entries
 * This is a doubly-linked list as we need to be able to do inserts
 * and deletes right in the middle of the list. It is inspired by the
 * Unix callout mechanism.
 * Note that we support two generic caller-provided parameters as
 * experience shows that at most two are often used. This causes very
 * little overhead, but simplifies caller code in cases where exactly
 * two parameters are needed. We hope this is a useful optimizaton.
 * rgerhards, 2009-06-15
 */
typedef struct apc_list_s {
	struct apc_list_s *pNext;
	struct apc_list_s *pPrev;
	apc_id_t id;
	apc_t *pApc;			/* pointer to the APC object to be scheduled */
} apc_list_t;

apc_list_t *apcListRoot = NULL;
apc_list_t *apcListTail = NULL;
pthread_mutex_t listMutex;		/* needs to be locked for all list operations */


/* destructor for the apc object */
BEGINobjDestruct(apc) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(apc)
ENDobjDestruct(apc)


/* ------------------------------ APC list handling functions ------------------------------ */

/* Function that handles changes to the list root. Most importantly, this function
 * needs to schedule a new timer. It is OK to call this function with an empty list.
 */
static rsRetVal
listRootChanged(void)
{
	DEFiRet;

	if(apcListRoot == NULL)
		FINALIZE;

	// TODO: implement!

finalize_it:
	RETiRet;
}


/* insert an apc entry into the APC list. The same entry MUST NOT already be present!
 */
static rsRetVal
insertApc(apc_t *pThis, apc_id_t *pID)
{
	apc_list_t *pCurr;
	apc_list_t *pNew;
	DEFiRet;

	CHKmalloc(pNew = (apc_list_t*) calloc(1, sizeof(apc_list_t)));
	pNew->pApc = pThis;
	pNew->id = *pID = apcID++;
dbgprintf("insert apc %p, id %ld\n", pThis, pNew->id);

	/* find right list location */
	if(apcListRoot == NULL) {
		/* no need to search, list is empty */
		apcListRoot = pNew;
		apcListTail = pNew;
		CHKiRet(listRootChanged());
	} else {
		for(pCurr = apcListRoot ; pCurr != NULL ; pCurr = pCurr->pNext) {
			if(pCurr->pApc->ttExec > pThis->ttExec)
				break;
		}

		if(pCurr == NULL) {
			/* insert at tail */
			pNew->pPrev = apcListTail;
			apcListTail->pNext = pNew;
			apcListTail = pNew;
		} else {
			if(pCurr == apcListRoot) {
				/* new first entry */
				pCurr->pPrev = pNew;
				pNew->pNext = pCurr;
				apcListRoot = pNew;
				CHKiRet(listRootChanged());
			} else {
				/* in the middle of the list */
				pCurr->pPrev = pNew;
				pNew->pNext = pCurr;
			}
		}
	}


finalize_it:
	RETiRet;
}


/* Delete an apc entry from the APC list. It is OK if the entry is not found,
 * in this case we assume it already has been processed.
 */
static rsRetVal
deleteApc(apc_id_t id)
{
	apc_list_t *pCurr;
	DEFiRet;

dbgprintf("trying to delete apc %ld\n", id);
	for(pCurr = apcListRoot ; pCurr != NULL ; pCurr = pCurr->pNext) {
		if(pCurr->id == id) {
RUNLOG_STR("apc id found, now deleting!\n");
			if(pCurr == apcListRoot) {
				apcListRoot = pCurr->pNext;
				CHKiRet(listRootChanged());
			} else {
				pCurr->pPrev->pNext = pCurr->pNext;
			}
			if(pCurr->pNext == NULL) {
				apcListTail = pCurr->pPrev;
			} else {
				pCurr->pNext->pPrev = pCurr->pPrev;
			}
			free(pCurr);
			pCurr = NULL;
			break;
		}
	}

finalize_it:
	RETiRet;
}


/* unlist all elements up to the current timestamp. Return this as a seperate list
 * to the caller. Returns an empty (NULL ptr) list if there are no such elements.
 * The caller must handle that gracefully. The list is returned in the parameter.
 */
static rsRetVal
unlistCurrent(apc_list_t **ppList)
{
	apc_list_t *pCurr;
	time_t tCurr;
	DEFiRet;
	assert(ppList != NULL);

	datetime.GetTime(&tCurr);

	if(apcListRoot == NULL || apcListRoot->pApc->ttExec >  tCurr) {
		*ppList = NULL;
		FINALIZE;
	}

	*ppList = apcListRoot;
	/* now search up to which entry we need to execute */
	for(pCurr = apcListRoot ; pCurr != NULL && pCurr->pApc->ttExec <= tCurr ; pCurr = pCurr->pNext)  {
		/*JUST SKIP TO LAST ELEMENT*/;
	}

	if(pCurr == NULL) {
		/* all elements can be unlisted */
		apcListRoot = NULL;
		apcListTail = NULL;
	} else {
		/* need to set a new root */
		pCurr->pPrev->pNext = NULL; /* terminate newly unlisted list */
		pCurr->pPrev = NULL; /* we are the new root */
		apcListRoot = pCurr;
	}

finalize_it:
	RETiRet;
}


/* ------------------------------ END APC list handling functions ------------------------------ */


/* execute all list elements that are currently scheduled for execution. We do this in two phases.
 * In the first phase, we look the list mutex and move everything from the head of the queue to
 * the current timestamp to a new to-be-executed list. Then we unlock the mutex and do the actual
 * exec (which may take some time).
 * Note that the caller is responsible for proper
 * caller-level synchronization. The caller may schedule another Apc, this module must
 * ensure that (and it does so by not locking the list mutex while we call the Apc).
 * Note: this function "consumes" the apc_t, so it is no longer existing after this
 * function returns.
 */
// TODO make static and associated with our own pthread-based timer
rsRetVal
execScheduled(void)
{
	apc_list_t *pExecList;
	apc_list_t *pCurr;
	apc_list_t *pNext;
	DEFiRet;

	d_pthread_mutex_lock(&listMutex);
	iRet = unlistCurrent(&pExecList);
	d_pthread_mutex_unlock(&listMutex);
	CHKiRet(iRet);

	if(pExecList != NULL) {
		DBGPRINTF("running apc scheduler -  we have %s to execute\n",
			  pExecList == NULL ? "nothing" : "something");
	}

	for(pCurr = pExecList ; pCurr != NULL ; pCurr = pNext) {
dbgprintf("executing apc list entry %p, apc %p\n", pCurr, pCurr->pApc);
		pNext = pCurr->pNext;
		pCurr->pApc->pProc(pCurr->pApc->param1, pCurr->pApc->param2);
		apcDestruct(&pCurr->pApc);
		free(pCurr);
	}

finalize_it:
	RETiRet;
}


/* Standard-Constructor
 */
BEGINobjConstruct(apc) /* be sure to specify the object type also in END macro! */
ENDobjConstruct(apc)


/* ConstructionFinalizer
 * Note that we use a non-standard calling interface: pID returns the current APC
 * id. This is the only way to handle the situation without the need for extra
 * locking.
 * rgerhards, 2008-01-09
 */
static rsRetVal
apcConstructFinalize(apc_t *pThis, apc_id_t *pID)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, apc);
	assert(pID != NULL);
	d_pthread_mutex_lock(&listMutex);
	insertApc(pThis, pID);
	d_pthread_mutex_unlock(&listMutex);
	RETiRet;
}


/* some set methods */
static rsRetVal
SetProcedure(apc_t *pThis, void (*pProc)(void*, void*))
{
	ISOBJ_TYPE_assert(pThis, apc);
	pThis->pProc = pProc;
	return RS_RET_OK;
}
static rsRetVal
SetParam1(apc_t *pThis, void *param1)
{
	ISOBJ_TYPE_assert(pThis, apc);
	pThis->param1 = param1;
	return RS_RET_OK;
}
static rsRetVal
SetParam2(apc_t *pThis, void *param2)
{
	ISOBJ_TYPE_assert(pThis, apc);
	pThis->param1 = param2;
	return RS_RET_OK;
}


/* cancel an Apc request, ID is provided. It is OK if the ID can not be found, this may
 * happen if the Apc was executed in the mean time. So it is safe to call CancelApc() at
 * any time.
 */
static rsRetVal
CancelApc(apc_id_t id)
{
	BEGINfunc
	d_pthread_mutex_lock(&listMutex);
	deleteApc(id);
	d_pthread_mutex_unlock(&listMutex);
	ENDfunc
	return RS_RET_OK;
}


/* debugprint for the apc object */
BEGINobjDebugPrint(apc) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDebugPrint(apc)
	dbgoprint((obj_t*) pThis, "APC module, currently no state info available\n");
ENDobjDebugPrint(apc)


/* queryInterface function
 */
BEGINobjQueryInterface(apc)
CODESTARTobjQueryInterface(apc)
	if(pIf->ifVersion != apcCURR_IF_VERSION) { /* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->Construct = apcConstruct;
	pIf->ConstructFinalize = apcConstructFinalize;
	pIf->Destruct = apcDestruct;
	pIf->DebugPrint = apcDebugPrint;
	pIf->CancelApc = CancelApc;
	pIf->SetProcedure = SetProcedure;
	pIf->SetParam1 = SetParam1;
	pIf->SetParam2 = SetParam2;
finalize_it:
ENDobjQueryInterface(apc)


/* Exit the apc class.
 * rgerhards, 2009-04-06
 */
BEGINObjClassExit(apc, OBJ_IS_CORE_MODULE) /* class, version */
	objRelease(datetime, CORE_COMPONENT);
	pthread_mutex_destroy(&listMutex);
ENDObjClassExit(apc)


/* Initialize the apc class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINObjClassInit(apc, 1, OBJ_IS_CORE_MODULE) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(datetime, CORE_COMPONENT));

	/* set our own handlers */
	OBJSetMethodHandler(objMethod_DEBUGPRINT, apcDebugPrint);
	OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, apcConstructFinalize);

	/* do other initializations */
	pthread_mutex_init(&listMutex, NULL);
ENDObjClassInit(apc)

/* vi:set ai:
 */
