/* Definition of the generic obj class module.
 *
 * This module relies heavily on preprocessor macros in order to
 * provide fast execution time AND ease of use.
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

#ifndef OBJ_H_INCLUDED
#define OBJ_H_INCLUDED

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

/* object Types/IDs */
typedef enum {	/* IDs of known object "types/classes" */
	objNull = 0,	/* no valid object (we do not start at zero so we can detect calloc()) */
	objMsg = 1
} objID_t;	
#define OBJ_NUM_IDS 2

typedef enum {	/* IDs of base methods supported by all objects - used for jump table, so
		 * they must start at zero and be incremented. -- rgerahrds, 2008-01-04
		 */
	objMethod_DESTRUCT = 0,
	objMethod_SERIALIZE = 1,
	objMethod_DESERIALIZE = 2,
	objMethod_DEBUGPRINT = 3
} objMethod_t;
#define OBJ_NUM_METHODS 4	/* must be updated to contain the max number of methods supported */

typedef struct objInfo_s {
	objID_t	objID;	
	int iObjVers;
	uchar *pszName;
	rsRetVal (*objMethods[OBJ_NUM_METHODS])();
} objInfo_t;

typedef struct obj {	/* the dummy struct that each derived class can be casted to */
	objInfo_t *pObjInfo;
} obj_t;


/* macros */
#define objSerializeSCALAR(propName, propType) \
	CHKiRet(objSerializeProp(pCStr, (uchar*) #propName, PROPTYPE_##propType, (void*) &pThis->propName));
#define objSerializePTR(propName, propType) \
	CHKiRet(objSerializeProp(pCStr, (uchar*) #propName, PROPTYPE_##propType, (void*) pThis->propName));
#define DEFobjStaticHelpers static objInfo_t *pObjInfoOBJ = NULL;
#define BEGINobjInstance objInfo_t *pObjInfo
#define objGetName(pThis) (((obj_t*) (pThis))->pObjInfo->pszName)
#define objGetObjID(pThis) (((obj_t*) (pThis))->pObjInfo->objID)
#define objGetVersion(pThis) (((obj_t*) (pThis))->pObjInfo->iObjVers)
/* must be called in Constructor: */
#define objConstructSetObjInfo(pThis) ((obj_t*) (pThis))->pObjInfo = pObjInfoOBJ;
#define objDestruct(pThis) (((obj_t*) (pThis))->pObjInfo->objMethods[objMethod_DESTRUCT])(pThis)
#define objSerialize(pThis) (((obj_t*) (pThis))->pObjInfo->objMethods[objMethod_SERIALIZE])
/* class initializer */
#define PROTOTYPEObjClassInit(objName) rsRetVal objName##ClassInit(void)
#define BEGINObjClassInit(objName, objVers) \
rsRetVal objName##ClassInit(void) \
{ \
	DEFiRet; \
	CHKiRet(objInfoConstruct(&pObjInfoOBJ, obj##objName, (uchar*) #objName, objVers, (rsRetVal (*)(void*))objName##Destruct)); 

#define ENDObjClassInit(objName) \
	objRegisterObj(obj##objName, pObjInfoOBJ); \
finalize_it: \
	return iRet; \
}

#define OBJSetMethodHandler(methodID, pHdlr) \
	CHKiRet(objInfoSetMethod(pObjInfoOBJ, methodID, (rsRetVal (*)(void*)) pHdlr))


/* prototypes */
rsRetVal objInfoConstruct(objInfo_t **ppThis, objID_t objID, uchar *pszName, int iObjVers, rsRetVal (*pDestruct)(void *));
rsRetVal objInfoSetMethod(objInfo_t *pThis, objMethod_t objMethod, rsRetVal (*pHandler)(void*));
rsRetVal objBeginSerialize(rsCStrObj **ppCStr, obj_t *pObj, size_t iExpectedObjSize);
rsRetVal objSerializePsz(rsCStrObj *pCStr, uchar *psz, size_t len);
rsRetVal objEndSerialize(rsCStrObj **ppCStr, obj_t *pObj);
rsRetVal objSerializeProp(rsCStrObj *pCStr, uchar *pszPropName, propertyType_t propType, void *pUsr);
rsRetVal objRegisterObj(objID_t oID, objInfo_t *pInfo);
PROTOTYPEObjClassInit(obj);

#endif /* #ifndef OBJ_H_INCLUDED */
