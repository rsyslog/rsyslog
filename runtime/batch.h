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

/* enum for batch states. Actually, we violate a layer here, in that we assume that a batch is used
 * for action processing. So far, this seems acceptable, the status is simply ignored inside the
 * main message queue. But over time, it could potentially be useful to split the two.
 * rgerhad, 2009-05-12
 */
typedef enum {
	BATCH_STATE_RDY  = 0,	/* object ready for processing */
	BATCH_STATE_BAD  = 1,	/* unrecoverable failure while processing, do NOT resubmit to same action */
	BATCH_STATE_SUB  = 2,	/* message submitted for processing, outcome yet unkonwn */
	BATCH_STATE_COMM = 3,	/* message successfully commited */
	BATCH_STATE_DISC = 4, 	/* discarded - processed OK, but do not submit to any other action */
} batch_state_t;


/* an object inside a batch, including any information (state!) needed for it to "life".
 */
struct batch_obj_s {
	obj_t *pUsrp;		/* pointer to user object (most often message) */
	batch_state_t state;	/* associated state */
};

/* the batch
 * This object is used to dequeue multiple user pointers which are than handed over
 * to processing. The size of elements is fixed after queue creation, but may be 
 * modified by config variables (better said: queue properties).
 * Note that a "user pointer" in rsyslog context so far always is a message 
 * object. We stick to the more generic term because queues may potentially hold
 * other types of objects, too.
 * rgerhards, 2009-05-12
 */
struct batch_s {
	int nElem;		/* actual number of element in this entry */
	int iDoneUpTo;		/* all messages below this index have state other than RDY */
	batch_obj_t *pElem;	/* batch elements */
};

#endif /* #ifndef BATCH_H_INCLUDED */
