/* linkedlist.c
 * This file set implements a generic linked list object. It can be used
 * wherever a linke list is required.
 * 
 * NOTE: we do not currently provide a constructor and destructor for the
 * object itself as we assume it will always be part of another strucuture.
 * Having a pointer to it, I think, does not really make sense but costs
 * performance. Consequently, there is is llInit() and llDestroy() and they
 * do what a constructor and destructur do, except for creating the
 * linkedList_t structure itself.
 *
 * File begun on 2007-07-31 by RGerhards
 *
 * Copyright 2007 Rainer Gerhards and Adiscon GmbH.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "rsyslog.h"
#include "linkedlist.h"


/* Initialize an existing linkedList_t structure
 * pKey destructor may be zero to take care of non-keyed lists.
 */
rsRetVal llInit(linkedList_t *pThis, rsRetVal (*pEltDestructor)(), rsRetVal (*pKeyDestructor)(void*), int (*pCmpOp)())
{
	assert(pThis != NULL);
	assert(pEltDestructor != NULL);

	pThis->pEltDestruct = pEltDestructor;
	pThis->pKeyDestruct = pKeyDestructor;
	pThis->cmpOp = pCmpOp;
	pThis->pKey = NULL;
	pThis->iNumElts = 0;
	pThis->pRoot = NULL;
	pThis->pLast = NULL;

	return RS_RET_OK;
};
	

/* llDestroyEltData - destroys a list element 
 * It is a separate function as the
 * functionality is needed in multiple code-pathes.
 */
static rsRetVal llDestroyElt(linkedList_t *pList, llElt_t *pElt)
{
	DEFiRet;

	assert(pList != NULL);
	assert(pElt != NULL);

	/* we ignore errors during destruction, as we need to try
	 * free the element in any case.
	 */
	if(pElt->pData != NULL)
		pList->pEltDestruct(pElt->pData);
	if(pElt->pKey != NULL)
		pList->pKeyDestruct(pElt->pKey);
	free(pElt);
	pList->iNumElts--; /* one less */

	return iRet;
}


/* llDestroy - destroys a COMPLETE linkedList
 */
rsRetVal llDestroy(linkedList_t *pThis)
{
	DEFiRet;
	llElt_t *pElt;
	llElt_t *pEltPrev;

	assert(pThis != NULL);

	pElt = pThis->pRoot;
	while(pElt != NULL) {
		pEltPrev = pElt;
		pElt = pElt->pNext;
		/* we ignore errors during destruction, as we need to try
		 * finish the linked list in any case.
		 */
		llDestroyElt(pThis, pEltPrev);
	}

	return iRet;
}

/* llDestroyRootElt - destroy the root element but otherwise
 * keeps this list intact.  -- rgerhards, 2007-08-03
 */
rsRetVal llDestroyRootElt(linkedList_t *pThis)
{
	DEFiRet;
	llElt_t *pPrev;
	
	if(pThis->pRoot == NULL) {
		ABORT_FINALIZE(RS_RET_EMPTY_LIST);
	}

	pPrev = pThis->pRoot;
	if(pPrev->pNext == NULL) {
		/* it was the only list element */
		pThis->pLast = NULL;
		pThis->pRoot = NULL;
	} else {
		/* there are other list elements */
		pThis->pRoot = pPrev->pNext;
	}

	CHKiRet(llDestroyElt(pThis, pPrev));

finalize_it:
	return iRet;
}


/* get next user data element of a linked list. The caller must also
 * provide a "cookie" to the function. On initial call, it must be
 * NULL. Other than that, the caller is not allowed to to modify the
 * cookie. In the current implementation, the cookie is an actual
 * pointer to the current list element, but this is nothing that the
 * caller should rely on.
 */
rsRetVal llGetNextElt(linkedList_t *pThis, linkedListCookie_t *ppElt, void **ppUsr)
{
	llElt_t *pElt;
	DEFiRet;

	assert(pThis != NULL);
	assert(ppElt != NULL);
	assert(ppUsr != NULL);

	pElt = *ppElt;

	pElt = (pElt == NULL) ? pThis->pRoot : pElt->pNext;

	if(pElt == NULL) {
		iRet = RS_RET_END_OF_LINKEDLIST;
	} else {
		*ppUsr = pElt->pData;
	}

	*ppElt = pElt;

	return iRet;
}


/* return the key of an Elt
 */
rsRetVal llGetKey(llElt_t *pThis, void **ppData)
{
	assert(pThis != NULL);
	assert(ppData != NULL);

	*ppData = pThis->pKey;

	return RS_RET_OK;
}


/* construct a new llElt_t
 */
static rsRetVal llEltConstruct(llElt_t **ppThis, void *pKey, void *pData)
{
	DEFiRet;
	llElt_t *pThis;

	assert(ppThis != NULL);

	if((pThis = (llElt_t*) calloc(1, sizeof(llElt_t))) == NULL) {
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}

	pThis->pKey = pKey;
	pThis->pData = pData;

finalize_it:
	*ppThis = pThis;
	return iRet;
}


/* append a user element to the end of the linked list. This includes setting a key. If no
 * key is desired, simply pass in a NULL pointer for it.
 */
rsRetVal llAppend(linkedList_t *pThis, void *pKey, void *pData)
{
	llElt_t *pElt;
	DEFiRet;
	
	CHKiRet(llEltConstruct(&pElt, pKey, pData));

	pThis->iNumElts++; /* one more */
	if(pThis->pLast == NULL) {
		pThis->pRoot = pElt;
	} else {
		pThis->pLast->pNext = pElt;
	}
	pThis->pLast = pElt;

finalize_it:
	return iRet;
}


/* find a user element based on the provided key
 */
rsRetVal llFind(linkedList_t *pThis, void *pKey, void **ppData)
{
	DEFiRet;
	llElt_t *pElt;
	int bFound = 0;

	assert(pThis != NULL);
	assert(pKey != NULL);
	assert(ppData != NULL);

	pElt = pThis->pRoot;
	while(pElt != NULL && bFound == 0) {
		if(pThis->cmpOp(pKey, pElt->pKey) == 0)
			bFound = 1;
		else
			pElt = pElt->pNext;
	}

	if(bFound == 1) {
		*ppData = pElt->pData;
	} else {
		iRet = RS_RET_NOT_FOUND;
	}

	return iRet;
}


/* provide the count of linked list elements
 */
rsRetVal llGetNumElts(linkedList_t *pThis, int *piCnt)
{
	DEFiRet;

	assert(pThis != NULL);
	assert(piCnt != NULL);

	*piCnt = pThis->iNumElts;

	return iRet;
}


/* execute a function on all list members. The functions receives a
 * user-supplied parameter, which may be either a simple value
 * or a pointer to a structure with more data. If the user-supplied
 * function does not return RS_RET_OK, this function here terminates.
 * rgerhards, 2007-08-02
 */
rsRetVal llExecFunc(linkedList_t *pThis, rsRetVal (*pFunc)(void*, void*), void* pParam)
{
	DEFiRet;
	rsRetVal iRetLL;
	void *pData;
	linkedListCookie_t llCookie = NULL;

	assert(pThis != NULL);
	assert(pFunc != NULL);

	while((iRetLL = llGetNextElt(pThis, &llCookie, (void**)&pData)) == RS_RET_OK) {
		CHKiRet(pFunc(pData, pParam));
	}

	if(iRetLL != RS_RET_END_OF_LINKEDLIST)
		iRet = iRetLL;

finalize_it:
	return iRet;
}


/*
 * vi:set ai:
 */
