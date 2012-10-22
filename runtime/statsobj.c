/* The statsobj object.
 *
 * This object provides a statistics-gathering facility inside rsyslog. This
 * functionality will be pragmatically implemented and extended.
 *
 * Copyright 2010-2012 Adiscon GmbH.
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
 * NOTE: The counter is READ-ONLY and MUST NOT be modified (most
 * importantly, it must not be initialized, so the caller must
 * ensure the counter is properly initialized before AddCounter()
 * is called.
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

/* get all the object's countes together as CEE. */
static rsRetVal
getStatsLineCEE(statsobj_t *pThis, cstr_t **ppcstr, int cee_cookie)
{
	cstr_t *pcstr;
	ctr_t *pCtr;
	DEFiRet;

	CHKiRet(cstrConstruct(&pcstr));

	if (cee_cookie == 1)
		rsCStrAppendStrWithLen(pcstr, UCHAR_CONSTANT("@cee: "), 6);
	
	rsCStrAppendStrWithLen(pcstr, UCHAR_CONSTANT("{"), 1);
	rsCStrAppendStrWithLen(pcstr, UCHAR_CONSTANT("\""), 1);
	rsCStrAppendStrWithLen(pcstr, UCHAR_CONSTANT("name"), 4);
	rsCStrAppendStrWithLen(pcstr, UCHAR_CONSTANT("\""), 1);
	rsCStrAppendStrWithLen(pcstr, UCHAR_CONSTANT(":"), 1);
	rsCStrAppendStrWithLen(pcstr, UCHAR_CONSTANT("\""), 1);
	rsCStrAppendStr(pcstr, pThis->name);
	rsCStrAppendStrWithLen(pcstr, UCHAR_CONSTANT("\""), 1);
	rsCStrAppendStrWithLen(pcstr, UCHAR_CONSTANT(","), 1);

	/* now add all counters to this line */
	pthread_mutex_lock(&pThis->mutCtr);
	for(pCtr = pThis->ctrRoot ; pCtr != NULL ; pCtr = pCtr->next) {
		rsCStrAppendStrWithLen(pcstr, UCHAR_CONSTANT("\""), 1);
		rsCStrAppendStr(pcstr, pCtr->name);
		rsCStrAppendStrWithLen(pcstr, UCHAR_CONSTANT("\""), 1);
		cstrAppendChar(pcstr, ':');
		switch(pCtr->ctrType) {
		case ctrType_IntCtr:
			rsCStrAppendInt(pcstr, *(pCtr->val.pIntCtr)); // TODO: OK?????
			break;
		case ctrType_Int:
			rsCStrAppendInt(pcstr, *(pCtr->val.pInt));
			break;
		}
		if (pCtr->next != NULL) {
			cstrAppendChar(pcstr, ',');
		} else {
			cstrAppendChar(pcstr, '}');
		}

	}
	pthread_mutex_unlock(&pThis->mutCtr);

	CHKiRet(cstrFinalize(pcstr));
	*ppcstr = pcstr;

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
getAllStatsLines(rsRetVal(*cb)(void*, cstr_t*), void *usrptr, statsFmtType_t fmt)
{
	statsobj_t *o;
	cstr_t *cstr;
	DEFiRet;

	for(o = objRoot ; o != NULL ; o = o->next) {
		switch(fmt) {
		case statsFmt_Legacy:
			CHKiRet(getStatsLine(o, &cstr));
			break;
		case statsFmt_CEE:
			CHKiRet(getStatsLineCEE(o, &cstr, 1));
			break;
		case statsFmt_JSON:
			CHKiRet(getStatsLineCEE(o, &cstr, 0));
			break;
		}
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
