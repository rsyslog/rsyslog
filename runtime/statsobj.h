/* The statsobj object.
 *
 * Copyright 2010 Rainer Gerhards and Adiscon GmbH.
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
	rsRetVal (*GetAllStatsLines)(rsRetVal(*cb)(void*, cstr_t*), void *usrptr);
	rsRetVal (*AddCounter)(statsobj_t *pThis, uchar *ctrName, statsCtrType_t ctrType, void *pCtr);
	rsRetVal (*EnableStats)(void);
ENDinterface(statsobj)
#define statsobjCURR_IF_VERSION 1 /* increment whenever you change the interface structure! */


/* prototypes */
PROTOTYPEObj(statsobj);


/* macros to handle stats counters
 * These are to be used by "counter providers". Note that we MUST
 * specify the mutex name, even though at first it looks like it
 * could be automatically be generated via e.g. "mut##ctr". 
 * Unfortunately, this does not work if counter is e.g. "pThis->ctr".
 * So we decided, for clarity, to always insist on specifying the mutex
 * name (after all, it's just a few more keystrokes...).
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
