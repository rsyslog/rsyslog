/* Definition of the batch_t data structure.
 * I am not sure yet if this will become a full-blown object. For now, this header just
 * includes the object definition and is not accompanied by code.
 *
 * Copyright 2009 by Rainer Gerhards and Adiscon GmbH.
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

#ifndef BATCH_H_INCLUDED
#define BATCH_H_INCLUDED

#include <string.h>
#include "msg.h"

/* enum for batch states. Actually, we violate a layer here, in that we assume that a batch is used
 * for action processing. So far, this seems acceptable, the status is simply ignored inside the
 * main message queue. But over time, it could potentially be useful to split the two.
 * rgerhad, 2009-05-12
 */
#define BATCH_STATE_RDY  0	/* object ready for processing */
#define BATCH_STATE_BAD  1	/* unrecoverable failure while processing, do NOT resubmit to same action */
#define BATCH_STATE_SUB  2	/* message submitted for processing, outcome yet unknown */
#define BATCH_STATE_COMM 3	/* message successfully commited */
#define BATCH_STATE_DISC 4 	/* discarded - processed OK, but do not submit to any other action */
typedef unsigned char batch_state_t;


/* an object inside a batch, including any information (state!) needed for it to "life".
 */
struct batch_obj_s {
	msg_t *pMsg;
	/* work variables for action processing; these are reused for each action (or block of
	 * actions)
	 */
	sbool bPrevWasSuspended;
	/* following are caches to save allocs if not absolutely necessary */
	uchar *staticActStrings[CONF_OMOD_NUMSTRINGS_MAXSIZE]; /**< for strings */
				/* a cache to save malloc(), if not absolutely necessary */
	void *staticActParams[CONF_OMOD_NUMSTRINGS_MAXSIZE]; /**< for anything else */
	size_t staticLenStrings[CONF_OMOD_NUMSTRINGS_MAXSIZE];
				/* and the same for the message length (if used) */
	/* end action work variables */
};

/* the batch
 * This object is used to dequeue multiple user pointers which are than handed over
 * to processing. The size of elements is fixed after queue creation, but may be 
 * modified by config variables (better said: queue properties).
 * Note that a "user pointer" in rsyslog context so far always is a message 
 * object. We stick to the more generic term because queues may potentially hold
 * other types of objects, too.
 * rgerhards, 2009-05-12
 * Note that nElem is not necessarily equal to nElemDeq. This is the case when we
 * discard some elements (because of configuration) during dequeue processing. As
 * all Elements are only deleted when the batch is processed, we can not immediately
 * delete them. So we need to keep their number that we can delete them when the batch
 * is completed (else, the whole process does not work correctly).
 */
struct batch_s {
	int maxElem;		/* maximum number of elements that this batch supports */
	int nElem;		/* actual number of element in this entry */
	int nElemDeq;		/* actual number of elements dequeued (and thus to be deleted) - see comment above! */
	int iDoneUpTo;		/* all messages below this index have state other than RDY */
	qDeqID	deqID;		/* ID of dequeue operation that generated this batch */
	int *pbShutdownImmediate;/* end processing of this batch immediately if set to 1 */
	sbool *active;		/* which messages are active for processing, NULL=all */
	sbool bSingleRuleset;	/* do all msgs of this batch use a single ruleset? */
	batch_obj_t *pElem;	/* batch elements */
	batch_state_t *eltState;/* state (array!) for individual objects.
	   			   NOTE: we have moved this out of batch_obj_t because we
				         get a *much* better cache hit ratio this way. So do not
					 move it back into this structure! Note that this is really
					 a HUGE saving, even if it doesn't look so (both profiler
					 data as well as practical tests indicate that!).
				*/
};


/* some inline functions (we may move this off to an object .. or not) */
static inline void
batchSetSingleRuleset(batch_t *pBatch, sbool val) {
	pBatch->bSingleRuleset = val;
}

/* get the batches ruleset (if we have a single ruleset) */
static inline ruleset_t*
batchGetRuleset(batch_t *pBatch) {
	return (pBatch->nElem > 0) ? pBatch->pElem[0].pMsg->pRuleset : NULL;
}

/* get the ruleset of a specifc element of the batch (index not verified!) */
static inline ruleset_t*
batchElemGetRuleset(batch_t *pBatch, int i) {
	return pBatch->pElem[i].pMsg->pRuleset;
}

/* get number of msgs for this batch */
static inline int
batchNumMsgs(batch_t *pBatch) {
	return pBatch->nElem;
}


/* set the status of the i-th batch element. Note that once the status is
 * DISC, it will never be reset. So this function can NOT be used to initialize
 * the state table. -- rgerhards, 2010-06-10
 */
static inline void
batchSetElemState(batch_t *pBatch, int i, batch_state_t newState) {
	if(pBatch->eltState[i] != BATCH_STATE_DISC)
		pBatch->eltState[i] = newState;
}


/* check if an element is a valid entry. We do NOT verify if the
 * element index is valid. -- rgerhards, 2010-06-10
 */
static inline int
batchIsValidElem(batch_t *pBatch, int i) {
	return(   (pBatch->eltState[i] != BATCH_STATE_DISC)
	       && (pBatch->active == NULL || pBatch->active[i]));
}


/* free members of a batch "object". Note that we can not do the usual
 * destruction as the object typically is allocated on the stack and so the
 * object itself cannot be freed! -- rgerhards, 2010-06-15
 */
static inline void
batchFree(batch_t *pBatch) {
	int i;
	int j;
	for(i = 0 ; i < pBatch->maxElem ; ++i) {
		for(j = 0 ; j < CONF_OMOD_NUMSTRINGS_MAXSIZE ; ++j) {
			/* staticActParams MUST be freed immediately (if required),
			 * so we do not need to do that!
			 */
			free(pBatch->pElem[i].staticActStrings[j]);
		}
	}
	free(pBatch->pElem);
	free(pBatch->eltState);
}


/* initialiaze a batch "object". The record must already exist,
 * we "just" initialize it. The max number of elements must be
 * provided. -- rgerhards, 2010-06-15
 */
static inline rsRetVal
batchInit(batch_t *pBatch, int maxElem) {
	DEFiRet;
	pBatch->iDoneUpTo = 0;
	pBatch->maxElem = maxElem;
	CHKmalloc(pBatch->pElem = calloc((size_t)maxElem, sizeof(batch_obj_t)));
	CHKmalloc(pBatch->eltState = calloc((size_t)maxElem, sizeof(batch_state_t)));
	// TODO: replace calloc by inidividual writes?
finalize_it:
	RETiRet;
}


/* primarily a helper for debug purposes, get human-readble name of state */
static inline char *
batchState2String(batch_state_t state) {
	switch(state) {
	case BATCH_STATE_RDY:
		return "BATCH_STATE_RDY";
	case BATCH_STATE_BAD:
		return "BATCH_STATE_BAD";
	case BATCH_STATE_SUB:
		return "BATCH_STATE_SUB";
	case BATCH_STATE_COMM:
		return "BATCH_STATE_COMM";
	case BATCH_STATE_DISC:
		return "BATCH_STATE_DISC";
	}
	return "ERROR, batch state not known!";
}
#endif /* #ifndef BATCH_H_INCLUDED */
