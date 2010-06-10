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

#include "msg.h"

/* enum for batch states. Actually, we violate a layer here, in that we assume that a batch is used
 * for action processing. So far, this seems acceptable, the status is simply ignored inside the
 * main message queue. But over time, it could potentially be useful to split the two.
 * rgerhad, 2009-05-12
 */
typedef enum {
	BATCH_STATE_RDY  = 0,	/* object ready for processing */
	BATCH_STATE_BAD  = 1,	/* unrecoverable failure while processing, do NOT resubmit to same action */
	BATCH_STATE_SUB  = 2,	/* message submitted for processing, outcome yet unknown */
	BATCH_STATE_COMM = 3,	/* message successfully commited */
	BATCH_STATE_DISC = 4, 	/* discarded - processed OK, but do not submit to any other action */
} batch_state_t;


/* an object inside a batch, including any information (state!) needed for it to "life".
 */
struct batch_obj_s {
	obj_t *pUsrp;		/* pointer to user object (most often message) */
	batch_state_t state;	/* associated state */
	/* work variables for action processing; these are reused for each action (or block of
	 * actions)
	 */
	sbool bFilterOK;	/* work area for filter processing (per action, reused!) */
	sbool bPrevWasSuspended;
	void *pActParams;	/* parameters to be passed to action */
	size_t *pLenParams;	/* length of the parameter in question */
	void *staticActParams[CONF_OMOD_NUMSTRINGS_BUFSIZE];
				/* a cache to save malloc(), if not absolutely necessary */
	size_t staticLenParams[CONF_OMOD_NUMSTRINGS_BUFSIZE];
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
	int nElem;		/* actual number of element in this entry */
	int nElemDeq;		/* actual number of elements dequeued (and thus to be deleted) - see comment above! */
	int iDoneUpTo;		/* all messages below this index have state other than RDY */
	qDeqID	deqID;		/* ID of dequeue operation that generated this batch */
	int *pbShutdownImmediate;/* end processing of this batch immediately if set to 1 */
	sbool bSingleRuleset;	/* do all msgs of this batch use a single ruleset? */
	batch_obj_t *pElem;	/* batch elements */
};


/* some inline functions (we may move this off to an object .. or not) */
static inline void
batchSetSingleRuleset(batch_t *pBatch, sbool val) {
	pBatch->bSingleRuleset = val;
}

/* get the batches ruleset */
static inline ruleset_t*
batchGetRuleset(batch_t *pBatch) {
	return (pBatch->nElem > 0) ? ((msg_t*) pBatch->pElem[0].pUsrp)->pRuleset : NULL;
}

/* get number of msgs for this batch */
static inline int
batchNumMsgs(batch_t *pBatch) {
	return pBatch->nElem;
}
#endif /* #ifndef BATCH_H_INCLUDED */
