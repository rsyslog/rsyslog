/* The statsobj object.
 *
 * This object provides a statistics-gathering facility inside rsyslog. This
 * functionality will be pragmatically implemented and extended.
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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <pthread.h>
#include <errno.h>
#include <assert.h>

#include "rsyslog.h"
#include "unicode-helper.h"
#include "obj.h"
#include "statsobj.h"
#include "sysvar.h"
#include "srUtils.h"
#include "stringbuf.h"


/* externally-visiable data (see statsobj.h for explanation) */
int GatherStats = 0;

/* static data */
DEFobjStaticHelpers

/* doubly linked list of stats objects. Object is automatically linked to it
 * upon construction. Enqueue always happens at the front (simplifies logic).
 */
static statsobj_t *objRoot = NULL;
static statsobj_t *objLast = NULL;

static pthread_mutex_t mutStats;

/* ------------------------------ statsobj linked list maintenance  ------------------------------ */

static inline void
addToObjList(statsobj_t *pThis)
{
	pthread_mutex_lock(&mutStats);
	pThis->prev = objLast;
	if(objLast != NULL)
		objLast->next = pThis;
	objLast = pThis;
	if(objRoot == NULL)
		objRoot = pThis;
	pthread_mutex_unlock(&mutStats);
}


static inline void
removeFromObjList(statsobj_t *pThis)
{
	pthread_mutex_lock(&mutStats);
	if(pThis->prev != NULL)
		pThis->prev->next = pThis->next;
	if(pThis->next != NULL)
		pThis->next->prev = pThis->prev;
	if(objLast == pThis)
		objLast = pThis->prev;
	if(objRoot == pThis)
		objRoot = pThis->next;
	pthread_mutex_unlock(&mutStats);
}


static inline void
addCtrToList(statsobj_t *pThis, ctr_t *pCtr)
{
	pthread_mutex_lock(&pThis->mutCtr);
	pCtr->prev = pThis->ctrLast;
	if(pThis->ctrLast != NULL)
		pThis->ctrLast->next = pCtr;
	pThis->ctrLast = pCtr;
	if(pThis->ctrRoot == NULL)
		pThis->ctrRoot = pCtr;
	pthread_mutex_unlock(&pThis->mutCtr);
}

/* ------------------------------ methods ------------------------------ */


/* Standard-Constructor
 */
BEGINobjConstruct(statsobj) /* be sure to specify the object type also in END macro! */
	pthread_mutex_init(&pThis->mutCtr, NULL);
	pThis->ctrLast = NULL;
	pThis->ctrRoot = NULL;
ENDobjConstruct(statsobj)


/* ConstructionFinalizer
 */
static rsRetVal
statsobjConstructFinalize(statsobj_t *pThis)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, statsobj);
	addToObjList(pThis);
	RETiRet;
}


/* set name. Note that we make our own copy of the memory, caller is
 * responsible to free up name it passes in (if required).
 */
static rsRetVal
setName(statsobj_t *pThis, uchar *name)
{
	DEFiRet;
	CHKmalloc(pThis->name = ustrdup(name));
finalize_it:
	RETiRet;
}


/* add a counter to an object
 * ctrName is duplicated, caller must free it if requried
 */
static rsRetVal
addCounter(statsobj_t *pThis, uchar *ctrName, statsCtrType_t ctrType, void *pCtr)
{
	ctr_t *ctr;
	DEFiRet;

	CHKmalloc(ctr = malloc(sizeof(ctr_t)));
	ctr->next = NULL;
	ctr->prev = NULL;
	CHKmalloc(ctr->name = ustrdup(ctrName));
	ctr->ctrType = ctrType;
	switch(ctrType) {
	case ctrType_IntCtr:
		ctr->val.pIntCtr = (intctr_t*) pCtr;
		break;
	case ctrType_Int:
		ctr->val.pInt = (int*) pCtr;
		break;
	}
	addCtrToList(pThis, ctr);

finalize_it:
	RETiRet;
}


/* get all the object's countes together with object name as one line.
 */
static rsRetVal
getStatsLine(statsobj_t *pThis, cstr_t **ppcstr)
{
	cstr_t *pcstr;
	ctr_t *pCtr;
	DEFiRet;

	CHKiRet(cstrConstruct(&pcstr));
	rsCStrAppendStr(pcstr, pThis->name);
	rsCStrAppendStrWithLen(pcstr, UCHAR_CONSTANT(": "), 2);

	/* now add all counters to this line */
	pthread_mutex_lock(&pThis->mutCtr);
	for(pCtr = pThis->ctrRoot ; pCtr != NULL ; pCtr = pCtr->next) {
		rsCStrAppendStr(pcstr, pCtr->name);
		cstrAppendChar(pcstr, '=');
		switch(pCtr->ctrType) {
		case ctrType_IntCtr:
			rsCStrAppendInt(pcstr, *(pCtr->val.pIntCtr)); // TODO: OK?????
			break;
		case ctrType_Int:
			rsCStrAppendInt(pcstr, *(pCtr->val.pInt));
			break;
		}
		cstrAppendChar(pcstr, ' ');
	}
	pthread_mutex_unlock(&pThis->mutCtr);

	CHKiRet(cstrFinalize(pcstr));
	*ppcstr = pcstr;

finalize_it:
	RETiRet;
}


/* this function can be used to obtain all stats lines. In this case,
 * a callback must be provided. This module than iterates over all objects and
 * submits each stats line to the callback. The callback has two parameters:
 * the first one is a caller-provided void*, the second one the cstr_t with the
 * line. If the callback reports an error, processing is stopped.
 */
static rsRetVal
getAllStatsLines(rsRetVal(*cb)(void*, cstr_t*), void *usrptr)
{
	statsobj_t *o;
	cstr_t *cstr;
	DEFiRet;

	for(o = objRoot ; o != NULL ; o = o->next) {
		CHKiRet(getStatsLine(o, &cstr));
		CHKiRet(cb(usrptr, cstr));
		rsCStrDestruct(&cstr);
	}

finalize_it:
	RETiRet;
}


/* Enable statistics gathering. currently there is no function to disable it
 * again, as this is right now not needed.
 */
static rsRetVal
enableStats()
{
	GatherStats = 1;
	return RS_RET_OK;
}


/* destructor for the statsobj object */
BEGINobjDestruct(statsobj) /* be sure to specify the object type also in END and CODESTART macros! */
	ctr_t *ctr, *ctrToDel;
CODESTARTobjDestruct(statsobj)
	removeFromObjList(pThis);

	/* destruct counters */
	ctr = pThis->ctrRoot;
	while(ctr != NULL) {
		ctrToDel = ctr;
		ctr = ctr->next;
		free(ctrToDel->name);
		free(ctrToDel);
	}

	pthread_mutex_destroy(&pThis->mutCtr);
	free(pThis->name);
ENDobjDestruct(statsobj)


/* debugprint for the statsobj object */
BEGINobjDebugPrint(statsobj) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDebugPrint(statsobj)
	dbgoprint((obj_t*) pThis, "statsobj object, currently no state info available\n");
ENDobjDebugPrint(statsobj)


/* queryInterface function
 */
BEGINobjQueryInterface(statsobj)
CODESTARTobjQueryInterface(statsobj)
	if(pIf->ifVersion != statsobjCURR_IF_VERSION) { /* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->Construct = statsobjConstruct;
	pIf->ConstructFinalize = statsobjConstructFinalize;
	pIf->Destruct = statsobjDestruct;
	pIf->DebugPrint = statsobjDebugPrint;
	pIf->SetName = setName;
	pIf->GetStatsLine = getStatsLine;
	pIf->GetAllStatsLines = getAllStatsLines;
	pIf->AddCounter = addCounter;
	pIf->EnableStats = enableStats;
finalize_it:
ENDobjQueryInterface(statsobj)


/* Initialize the statsobj class. Must be called as the very first method
 * before anything else is called inside this class.
 */
BEGINAbstractObjClassInit(statsobj, 1, OBJ_IS_CORE_MODULE) /* class, version */
	/* request objects we use */

	/* set our own handlers */
	OBJSetMethodHandler(objMethod_DEBUGPRINT, statsobjDebugPrint);
	OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, statsobjConstructFinalize);

	/* init other data items */
	pthread_mutex_init(&mutStats, NULL);

ENDObjClassInit(statsobj)

/* Exit the class.
 */
BEGINObjClassExit(statsobj, OBJ_IS_CORE_MODULE) /* class, version */
	/* release objects we no longer need */
	pthread_mutex_destroy(&mutStats);
ENDObjClassExit(statsobj)
