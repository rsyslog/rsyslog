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

typedef enum {	/* IDs of known object "types/classes" */
	objNull = 0,	/* no valid object (we do not start at zero so we can detect calloc()) */
	objMsg = 1
} objID_t;	

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
	uchar *pszName;
	rsRetVal (*objMethods[OBJ_NUM_METHODS])(void *pThis);
} objInfo_t;

typedef struct obj {	/* the dummy struct that each derived class can be casted to */
	objInfo_t *pObjInfo;
} obj_t;


/* macros */
#define DEFobjStaticHelpers static objInfo_t *pObjInfoOBJ = NULL;
#define BEGINobjInstance objInfo_t *pObjInfo
/* must be called in Constructor: */
#define objConstructSetObjInfo(pThis) ((obj_t*) (pThis))->pObjInfo = pObjInfoOBJ;
#define objDestruct(pThis) ((objInfo_t*) (pThis)->objMethods[objMethod_DESTRUCT])
/* class initializer */
#define PROTOTYPEObjClassInit(objName) rsRetVal objName##ClassInit(void)
#define BEGINObjClassInit(objName) \
rsRetVal objName##ClassInit(void) \
{ \
	DEFiRet; \
	CHKiRet(objInfoConstruct(&pObjInfoOBJ, obj##objName, (uchar*) #objName, (rsRetVal (*)(void*))objName##Destruct)); 

#define ENDObjClassInit \
finalize_it: \
	return iRet; \
}

#define OBJSetMethodHandler(methodID, pHdlr) \
	CHKiRet(objInfoSetMethod(pObjInfoOBJ, methodID, (rsRetVal (*)(void*)) pHdlr))


/* prototypes */
rsRetVal objInfoConstruct(objInfo_t **ppThis, objID_t objID, uchar *pszName, rsRetVal (*pDestruct)(void *));
rsRetVal objInfoSetMethod(objInfo_t *pThis, objMethod_t objMethod, rsRetVal (*pHandler)(void*));

#endif /* #ifndef OBJ_H_INCLUDED */
