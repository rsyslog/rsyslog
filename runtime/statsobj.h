/* The statsobj object.
 *
 * Copyright 2010-2012 Rainer Gerhards and Adiscon GmbH.
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
#ifndef INCLUDED_STATSOBJ_H
#define INCLUDED_STATSOBJ_H

#include "atomic.h"

/* The following data item is somewhat dirty, in that it does not follow
 * our usual object calling conventions. However, much like with "Debug", we
 * do this to gain speed. If we finally come to a platform that does not
 * provide resolution of names for dynamically loaded modules, we need to find
 * a work-around, but until then, we use the direct access.
 * If set to 0, statistics are not gathered, otherwise they are.
 */
extern int GatherStats;

/* our basic counter type -- need 32 bit on 32 bit platform. 
 * IMPORTANT: this type *MUST* be supported by atomic instructions!
 */
typedef uint64 intctr_t;

/* counter types */
typedef enum statsCtrType_e {
	ctrType_IntCtr,
	ctrType_Int
} statsCtrType_t;

/* stats line format types */
typedef enum statsFmtType_e {
	statsFmt_Legacy,
	statsFmt_JSON,
	statsFmt_CEE
} statsFmtType_t;


/* helper entity, the counter */
typedef struct ctr_s {
	uchar *name;
	statsCtrType_t ctrType;
	union {
		intctr_t *pIntCtr;
		int *pInt;
	} val;
	struct ctr_s *next, *prev;
} ctr_t;

/* the statsobj object */
struct statsobj_s {
	BEGINobjInstance;		/* Data to implement generic object - MUST be the first data element! */
	uchar *name;
	pthread_mutex_t mutCtr;		/* to guard counter linked-list ops */
	ctr_t *ctrRoot;			/* doubly-linked list of statsobj counters */
	ctr_t *ctrLast;
	/* used to link ourselves together */
	statsobj_t *prev;
	statsobj_t *next;
};


/* interfaces */
BEGINinterface(statsobj) /* name must also be changed in ENDinterface macro! */
	INTERFACEObjDebugPrint(statsobj);
	rsRetVal (*Construct)(statsobj_t **ppThis);
	rsRetVal (*ConstructFinalize)(statsobj_t *pThis);
	rsRetVal (*Destruct)(statsobj_t **ppThis);
	rsRetVal (*SetName)(statsobj_t *pThis, uchar *name);
	rsRetVal (*GetStatsLine)(statsobj_t *pThis, cstr_t **ppcstr);
	rsRetVal (*GetAllStatsLines)(rsRetVal(*cb)(void*, cstr_t*), void *usrptr, statsFmtType_t fmt);
	rsRetVal (*AddCounter)(statsobj_t *pThis, uchar *ctrName, statsCtrType_t ctrType, void *pCtr);
	rsRetVal (*EnableStats)(void);
ENDinterface(statsobj)
#define statsobjCURR_IF_VERSION 10 /* increment whenever you change the interface structure! */
/* Changes
 * v2-v9 rserved for future use in "older" version branches
 * v10, 2012-04-01: GetAllStatsLines got fmt parameter
 */


/* prototypes */
PROTOTYPEObj(statsobj);


/* macros to handle stats counters
 * These are to be used by "counter providers". Note that we MUST
 * specify the mutex name, even though at first it looks like it
 * could be automatically be generated via e.g. "mut##ctr". 
 * Unfortunately, this does not work if counter is e.g. "pThis->ctr".
 * So we decided, for clarity, to always insist on specifying the mutex
 * name (after all, it's just a few more keystrokes...).
 * --------------------------------------------------------------------
 *                               NOTE WELL
 * --------------------------------------------------------------------
 * There are actually two types of stats counters: "regular" counters,
 * which are only used for stats purposes and "dual" counters, which 
 * are primarily used for other purposes but can be included in stats
 * as well. ALL regular counters MUST be initialized with
 * STATSCOUNTER_INIT and only be modified by STATSCOUNTER_* functions.
 * They MUST NOT be used for any other purpose (if this seems to make
 * sense, consider changing it to a dual counter).
 * Dual counters are somewhat dangerous in that a single variable is
 * used for two purposes: the actual application need and stats
 * counting. However, this is supported for performance reasons, as it
 * provides insight into the inner engine workings without need for
 * additional counters (and their maintenance code). Dual counters
 * MUST NOT be modified by STATSCOUNTER_* functions. Most importantly,
 * it is expected that the actua application code provides proper
 * (enough) synchronized access to these counters. Most importantly,
 * this means they have NO stats-system mutex associated to them.
 *
 * The interface function AddCounter() is a read-only function. It 
 * only provides the stats subsystem with a reference to a counter.
 * It is irrelevant if the counter is a regular or dual one. For that
 * reason, AddCounter() must not modify the counter contents, as in
 * the case of a dual counter application code may be broken.
 */
#define STATSCOUNTER_DEF(ctr, mut) \
	intctr_t ctr; \
	DEF_ATOMIC_HELPER_MUT64(mut);

#define STATSCOUNTER_INIT(ctr, mut) \
	INIT_ATOMIC_HELPER_MUT64(mut); \
	ctr = 0;

#define STATSCOUNTER_INC(ctr, mut) \
	if(GatherStats) \
		ATOMIC_INC_uint64(&ctr, &mut);

#define STATSCOUNTER_DEC(ctr, mut) \
	if(GatherStats) \
		ATOMIC_DEC_uint64(&ctr, mut);

/* the next macro works only if the variable is already guarded
 * by mutex (or the users risks a wrong result). It is assumed 
 * that there are not concurrent operations that modify the counter.
 */
#define STATSCOUNTER_SETMAX_NOMUT(ctr, newmax) \
	if(GatherStats && ((newmax) > (ctr))) \
		ctr = newmax;

#endif /* #ifndef INCLUDED_STATSOBJ_H */
