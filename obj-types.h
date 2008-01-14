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

/* property types */
typedef enum {	 /* do NOT start at 0 to detect uninitialized types after calloc() */
	PROPTYPE_PSZ = 1,
	PROPTYPE_SHORT = 2,
	PROPTYPE_INT = 3,
	PROPTYPE_LONG = 4,
	PROPTYPE_CSTR = 5,
	PROPTYPE_SYSLOGTIME = 6
} propertyType_t;

typedef struct {
	rsCStrObj *pcsName;
	propertyType_t propType;
	union {
		short vShort;
		int vInt;
		long vLong;
		rsCStrObj *vpCStr; /* used for both rsCStr and psz */
		syslogTime_t vSyslogTime;

	} val;
} property_t;

/* object Types/IDs */
typedef enum {	/* IDs of known object "types/classes" */
	OBJNull = 0,	/* no valid object (we do not start at zero so we can detect calloc()) */
	OBJMsg = 1,
	OBJstrm = 2,
	OBJqueue = 3	/* remeber to UPDATE OBJ_NUM_IDS (below) if you add one! */
} objID_t;	
#define OBJ_NUM_IDS 4

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
} obj_t;

/* macros which must be gloablly-visible (because they are used during definition of
 * other objects.
 */
#ifndef NDEBUG /* this means if debug... */
#	define BEGINobjInstance \
		objInfo_t *pObjInfo; \
		unsigned int iObjCooCKiE; /* prevent name conflict, thus the strange name */ 
#	define ISOBJ_assert(pObj) \
		{ \
		assert((pObj) != NULL); \
		assert((unsigned) ((obj_t*)(pObj))->iObjCooCKiE == (unsigned) 0xBADEFEE); \
		}
#	define ISOBJ_TYPE_assert(pObj, objType) \
		{ \
		assert(pObj != NULL); \
		assert((unsigned) pObj->iObjCooCKiE == (unsigned) 0xBADEFEE); \
		assert(objGetObjID(pObj) == OBJ##objType); \
		}
#else /* non-debug mode, no checks but much faster */
#	define BEGINobjInstance objInfo_t *pObjInfo; 
#	define ISOBJ_TYPE_assert(pObj, objType)
#	define ISOBJ_assert(pObj, objType)
#endif

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
	return iRet; \
}

/* this defines both the constructor and initializer
 * rgerhards, 2008-01-10
 */
#define BEGINobjConstruct(obj) \
	rsRetVal obj##Initialize(obj##_t *pThis) \
	{ \
		DEFiRet;

#define ENDobjConstruct(obj) \
		/* use finalize_it: before calling the macro (if you need it)! */ \
		return iRet; \
	} \
	rsRetVal obj##Construct(obj##_t **ppThis) \
	{ \
		DEFiRet; \
		obj##_t *pThis; \
	 \
		assert(ppThis != NULL); \
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
		return iRet; \
	} 


#endif /* #ifndef OBJ_TYPES_H_INCLUDED */
