/* The relp sendqueue object.
 *
 * Copyright 2008 by Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of librelp.
 *
 * Librelp is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Librelp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Librelp.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 *
 * If the terms of the GPL are unsuitable for your needs, you may obtain
 * a commercial license from Adiscon. Contact sales@adiscon.com for further
 * details.
 *
 * ALL CONTRIBUTORS PLEASE NOTE that by sending contributions, you assign
 * your copyright to Adiscon GmbH, Germany. This is necessary to permit the
 * dual-licensing set forth here. Our apologies for this inconvenience, but
 * we sincerely believe that the dual-licensing model helps us provide great
 * free software while at the same time obtaining some funding for further
 * development.
 */
#include "config.h"
#include <stdlib.h>
#include "relp.h"
#include "sendq.h"
#include "dbllinklist.h"


/* handling for the sendq entries  */

/** Construct a RELP sendq instance
 * This is the first thing that a caller must do before calling any
 * RELP function. The relp sendq must only destructed after all RELP
 * operations have been finished.
 */
static relpRetVal
relpSendqeConstruct(relpSendqe_t **ppThis)
{
	relpSendqe_t *pThis;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	if((pThis = calloc(1, sizeof(relpSendqe_t))) == NULL) {
		ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}

	RELP_CORE_CONSTRUCTOR(pThis, Sendqe);

	*ppThis = pThis;

finalize_it:
	LEAVE_RELPFUNC;
}


/** Destruct a RELP sendq instance
 */
static relpRetVal
relpSendqeDestruct(relpSendqe_t **ppThis)
{
	relpSendqe_t *pThis;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	pThis = *ppThis;
	RELPOBJ_assert(pThis, Sendqe);

	/* done with de-init work, now free sendqe object itself */
	free(pThis);
	*ppThis = NULL;

finalize_it:
	LEAVE_RELPFUNC;
}



/* handling for the sendq itself */


/** Construct a RELP sendq instance
 * This is the first thing that a caller must do before calling any
 * RELP function. The relp sendq must only destructed after all RELP
 * operations have been finished.
 */
relpRetVal
relpSendqConstruct(relpSendq_t **ppThis)
{
	relpSendq_t *pThis;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	if((pThis = calloc(1, sizeof(relpSendq_t))) == NULL) {
		ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}

	RELP_CORE_CONSTRUCTOR(pThis, Sendq);
	pthread_mutex_init(&pThis->mut, NULL);

	*ppThis = pThis;

finalize_it:
	LEAVE_RELPFUNC;
}


/** Destruct a RELP sendq instance
 */
relpRetVal
relpSendqDestruct(relpSendq_t **ppThis)
{
	relpSendq_t *pThis;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	pThis = *ppThis;
	RELPOBJ_assert(pThis, Sendq);

	pthread_mutex_destroy(&pThis->mut);
	/* done with de-init work, now free sendq object itself */
	free(pThis);
	*ppThis = NULL;

finalize_it:
	LEAVE_RELPFUNC;
}


/* add a sendbuffer to the queue
 * the send buffer object is handed over, caller must no longer access it after
 * it has been passed into here.
 * rgerhards, 2008-03-17
 */
relpRetVal
relpSendqAddBuf(relpSendq_t *pThis, relpSendbuf_t *pBuf)
{
	relpSendqe_t *pEntry;

	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sendq);
	RELPOBJ_assert(pBuf, Sendbuf);

	CHKRet(relpSendqeConstruct(&pEntry));
	pEntry->pBuf = pBuf;

	pthread_mutex_lock(&pThis->mut);
	DLL_Add(pEntry, pThis->pRoot, pThis->pLast);
	pthread_mutex_unlock(&pThis->mut);

finalize_it:
	LEAVE_RELPFUNC;
}


/* remove the sendbuffer at the top of the queue. The removed buffer is destructed.
 * This must only be called if there is at least one send buffer inside the queue.
 * rgerhards, 2008-03-17
 */
relpRetVal
relpSendqDelFirstBuf(relpSendq_t *pThis)
{
	relpSendqe_t *pEntry;

	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Sendq);
	RELPOBJ_assert(pThis->pRoot, Sendbuf);

	pthread_mutex_lock(&pThis->mut);
	pEntry = pThis->pRoot;
	DLL_Del(pEntry, pThis->pRoot, pThis->pLast);
	pthread_mutex_unlock(&pThis->mut);

	CHKRet(relpSendqeDestruct(&pEntry));

finalize_it:
	LEAVE_RELPFUNC;
}
