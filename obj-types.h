/* Some type definitions and macros for the obj object.
 * I needed to move them out of the main obj.h, because obj.h's
 * prototypes use other data types. However, their .h's rely
 * on some of the obj.h data types and macros. So I needed to break
 * that loop somehow and I've done that by moving the typedefs
 * into this file here.
 *
 * Copyright 2008 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Rsyslog is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rsyslog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Rsyslog.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */

#ifndef OBJ_TYPES_H_INCLUDED
#define OBJ_TYPES_H_INCLUDED

#include "stringbuf.h"
#include "syslogd-types.h"

/* object Types/IDs */
typedef enum {	/* IDs of known object "types/classes" */
	OBJNull = 0,	/* no valid object (we do not start at zero so we can detect calloc()) */
	OBJmsg = 1,
	OBJstrm = 2,
	OBJwtp = 3,
	OBJwti = 4,
	OBJqueue = 5,
	OBJctok = 6,
	OBJctok_token = 7,
	OBJvar = 8,
	OBJvmop = 9,
	OBJvmprg = 10,
	OBJvm = 11,
	OBJvmstk = 12,
	OBJexpr = 13	/* remeber to UPDATE OBJ_NUM_IDS (below) if you add one! */
} objID_t;	
#define OBJ_NUM_IDS 14

typedef enum {	/* IDs of base methods supported by all objects - used for jump table, so
		 * they must start at zero and be incremented. -- rgerahrds, 2008-01-04
		 */
	objMethod_CONSTRUCT = 0,
	objMethod_DESTRUCT = 1,
	objMethod_SERIALIZE = 2,
	objMethod_DESERIALIZE = 3,
	objMethod_SETPROPERTY = 4,
	objMethod_CONSTRUCTION_FINALIZER = 5,
	objMethod_GETSEVERITY = 6,
	objMethod_DEBUGPRINT = 7
} objMethod_t;
#define OBJ_NUM_METHODS 8	/* must be updated to contain the max number of methods supported */

typedef struct objInfo_s {
	objID_t	objID;	
	int iObjVers;
	uchar *pszName;
	rsRetVal (*objMethods[OBJ_NUM_METHODS])();
} objInfo_t;


typedef struct obj {	/* the dummy struct that each derived class can be casted to */
	objInfo_t *pObjInfo;
#ifndef NDEBUG /* this means if debug... */
	unsigned int iObjCooCKiE; /* must always be 0xBADEFEE for a valid object */
#endif
	uchar *pszName;		/* the name of *this* specific object instance */
} obj_t;


/* macros which must be gloablly-visible (because they are used during definition of
 * other objects.
 */
#ifndef NDEBUG /* this means if debug... */
#	define BEGINobjInstance \
		obj_t objData
#	define ISOBJ_assert(pObj) \
		do { \
		ASSERT((pObj) != NULL); \
		ASSERT((unsigned) ((obj_t*)(pObj))->iObjCooCKiE == (unsigned) 0xBADEFEE); \
		} while(0);
#	define ISOBJ_TYPE_assert(pObj, objType) \
		do { \
		ASSERT(pObj != NULL); \
		ASSERT((unsigned) ((obj_t*) (pObj))->iObjCooCKiE == (unsigned) 0xBADEFEE); \
		ASSERT(objGetObjID(pObj) == OBJ##objType); \
		} while(0);
#else /* non-debug mode, no checks but much faster */
#	define BEGINobjInstance obj_t objData
#	define ISOBJ_TYPE_assert(pObj, objType)
#	define ISOBJ_assert(pObj)
#endif

#define DEFpropSetMethPTR(obj, prop, dataType)\
	rsRetVal obj##Set##prop(obj##_t *pThis, dataType *pVal)\
	{ \
		/* DEV debug: dbgprintf("%sSet%s()\n", #obj, #prop); */\
		pThis->prop = pVal; \
		return RS_RET_OK; \
	}
#define PROTOTYPEpropSetMethPTR(obj, prop, dataType)\
	rsRetVal obj##Set##prop(obj##_t *pThis, dataType*)
#define DEFpropSetMeth(obj, prop, dataType)\
	rsRetVal obj##Set##prop(obj##_t *pThis, dataType pVal)\
	{ \
		/* DEV debug: dbgprintf("%sSet%s()\n", #obj, #prop); */\
		pThis->prop = pVal; \
		return RS_RET_OK; \
	}
#define DEFpropSetMethFP(obj, prop, dataType)\
	rsRetVal obj##Set##prop(obj##_t *pThis, dataType)\
	{ \
		/* DEV debug: dbgprintf("%sSet%s()\n", #obj, #prop); */\
		pThis->prop = pVal; \
		return RS_RET_OK; \
	}
#define PROTOTYPEpropSetMethFP(obj, prop, dataType)\
	rsRetVal obj##Set##prop(obj##_t *pThis, dataType)
#define DEFpropSetMeth(obj, prop, dataType)\
	rsRetVal obj##Set##prop(obj##_t *pThis, dataType pVal)\
	{ \
		/* DEV debug: dbgprintf("%sSet%s()\n", #obj, #prop); */\
		pThis->prop = pVal; \
		return RS_RET_OK; \
	}
#define PROTOTYPEpropSetMeth(obj, prop, dataType)\
	rsRetVal obj##Set##prop(obj##_t *pThis, dataType pVal)
/* class initializer */
#define PROTOTYPEObjClassInit(objName) rsRetVal objName##ClassInit(void)
#define BEGINObjClassInit(objName, objVers) \
rsRetVal objName##ClassInit(void) \
{ \
	DEFiRet; \
	CHKiRet(objInfoConstruct(&pObjInfoOBJ, OBJ##objName, (uchar*) #objName, objVers, \
	                         (rsRetVal (*)(void*))objName##Construct,  (rsRetVal (*)(void*))objName##Destruct)); 

#define ENDObjClassInit(objName) \
	objRegisterObj(OBJ##objName, pObjInfoOBJ); \
finalize_it: \
	RETiRet; \
}


/* this defines both the constructor and initializer
 * rgerhards, 2008-01-10
 */
#define BEGINobjConstruct(obj) \
	rsRetVal obj##Initialize(obj##_t __attribute__((unused)) *pThis) \
	{ \
		DEFiRet;

#define ENDobjConstruct(obj) \
		/* use finalize_it: before calling the macro (if you need it)! */ \
		RETiRet; \
	} \
	rsRetVal obj##Construct(obj##_t **ppThis) \
	{ \
		DEFiRet; \
		obj##_t *pThis; \
	 \
		ASSERT(ppThis != NULL); \
	 \
		if((pThis = (obj##_t *)calloc(1, sizeof(obj##_t))) == NULL) { \
			ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY); \
		} \
		objConstructSetObjInfo(pThis); \
	 \
		obj##Initialize(pThis); \
	\
	finalize_it: \
		OBJCONSTRUCT_CHECK_SUCCESS_AND_CLEANUP \
		RETiRet; \
	} 


/* this defines the destructor. The important point is that the base object
 * destructor is called. The upper-level class shall destruct all of its
 * properties, but not the instance itself. This is freed here by the 
 * framework (we need an intact pointer because we need to free the
 * obj_t structures inside it). A pointer to the object pointer must be
 * parse, because it is re-set to NULL (this, for example, is important in
 * cancellation handlers). The object pointer is always named pThis.
 * The object is always freed, even if there is some error while
 * Cancellation is blocked during destructors, as this could have fatal
 * side-effects. However, this also means the upper-level object should
 * not perform any lenghty processing.
 * IMPORTANT: if the upper level object requires some situations where the
 * object shall not be destructed (e.g. via reference counting), then
 * it shall set pThis to NULL, which prevents destruction of the
 * object.
 * processing.
 * rgerhards, 2008-01-30
 */
#define BEGINobjDestruct(obj) \
	rsRetVal obj##Destruct(obj##_t **ppThis) \
	{ \
		DEFiRet; \
		int iCancelStateSave; \
		obj##_t *pThis; 

#define CODESTARTobjDestruct(obj) \
		ASSERT(ppThis != NULL); \
		pThis = *ppThis; \
		ISOBJ_TYPE_assert(pThis, obj); \
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave);

#define ENDobjDestruct(obj) \
	 	goto finalize_it; /* prevent compiler warning ;) */ \
	 	/* no more code here! */ \
	finalize_it: \
		if(pThis != NULL) { \
			objDestructObjSelf((obj_t*) pThis); \
			free(pThis); \
			*ppThis = NULL; \
		} \
		pthread_setcancelstate(iCancelStateSave, NULL); \
		RETiRet; \
	} 


/* this defines the debug print entry point. DebugPrint is optional. If
 * it is provided, the object should output some meaningful information
 * via the debug system.
 * rgerhards, 2008-02-20
 */
#define PROTOTYPEObjDebugPrint(obj) rsRetVal obj##DebugPrint(obj##_t *pThis)
#define INTERFACEObjDebugPrint(obj) rsRetVal (*DebugPrint)(obj##_t *pThis)
#define BEGINobjDebugPrint(obj) \
	rsRetVal obj##DebugPrint(obj##_t *pThis) \
	{ \
		DEFiRet; \

#define CODESTARTobjDebugPrint(obj) \
		ASSERT(pThis != NULL); \
		ISOBJ_TYPE_assert(pThis, obj); \

#define ENDobjDebugPrint(obj) \
		RETiRet; \
	} 

/* ------------------------------ object loader system ------------------------------ *
 * The following code is the early beginning of a dynamic object loader system. The 
 * root idea is that all objects will become dynamically loadable libraries over time,
 * which is necessary to get a clean plug-in interface where every plugin can access
 * rsyslog's rich object model via simple and quite portable methods.
 *
 * To do so, each object defines one or more interfaces. They are essentially structures
 * with function (method) pointers. Anyone interested in calling an object must first
 * obtain the interface and can then call through it.
 *
 * The interface data type must always be called <obj>_if_t, as this is expected
 * by the macros. Having consitent naming is also easier for the programmer. By default,
 * macros create a static variable named like the object in each calling objects
 * static data block.
 *
 * To facilitate moving to this system, I begin to implement some hooks, which
 * allows to use interfaces today (when the rest of the infrastructure is not yet
 * there). This is in the hope that it will ease migration to the full-fledged system
 * once we are ready to work on that.
 * rgerhards, 2008-02-21
 */

/* this defines the QueryInterface print entry point. Over time, it should be
 * present in all objects.
 */
#define PROTOTYPEObjQueryInterface(obj) rsRetVal obj##QueryInterface(obj##_if_t *pThis)
#define BEGINobjQueryInterface(obj) \
	rsRetVal obj##QueryInterface(obj##_if_t *pIf) \
	{ \
		DEFiRet; \

#define CODESTARTobjQueryInterface(obj) \
		ASSERT(pIf != NULL);

#define ENDobjQueryInterface(obj) \
		RETiRet; \
	} 
/* the base data type for interfaces
 * This MUST be in sync with the ifBEGIN macro
 */
typedef struct interface_s {
	int ifVersion;	/* must be set to version requested */ 
	objID_t oID;	/* our object ID (later dynamically assigned) */
} interface_t;


/* the following macro is used to get access to an object (not an instance,
 * just the class itself!). It must be called before any of the object's
 * methods can be accessed.
 */
#define objUse(obj) \
	obj##QueryInterface(&obj)

/* defines data that must always be present at the very begin of the interface structure */
#define ifBEGIN \
	int ifVersion;	/* must be set to version requested */ \
	objID_t oID;	/* our object ID (later dynamically assigned) */


/* use the following define some place in your static data (suggested right at
 * the beginning
 */
#define DEFobjCurrIf(obj) \
	static obj##_if_t obj = { .ifVersion = obj##CURR_IF_VERSION };
 
/* ------------------------------ end object loader system ------------------------------ */


#endif /* #ifndef OBJ_TYPES_H_INCLUDED */
