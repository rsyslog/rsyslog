/* Definition of the generic obj class module.
 *
 * This module relies heavily on preprocessor macros in order to
 * provide fast execution time AND ease of use.
 *
 * Each object that uses this base class MUST provide a constructor with
 * the following interface:
 *
 * Destruct(pThis);
 *
 * A constructor is not necessary (except for some features, e.g. de-serialization).
 * If it is provided, it is a three-part constructor (to handle all cases with a
 * generic interface):
 *
 * Construct(&pThis);
 * SetProperty(pThis, property_t *);
 * ConstructFinalize(pThis);
 *
 * SetProperty() and ConstructFinalize() may also be called on an object
 * instance which has been Construct()'ed outside of this module.
 *
 * pThis always references to a pointer of the object.
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

#include "obj-types.h"
#include "stream.h"

/* macros */
/* the following one is a helper that prevents us from writing the
 * ever-same code at the end of Construct()
 */
#define OBJCONSTRUCT_CHECK_SUCCESS_AND_CLEANUP \
	if(iRet == RS_RET_OK) { \
		*ppThis = pThis; \
	} else { \
		if(pThis != NULL) \
			free(pThis); \
	}

#define objSerializeSCALAR(propName, propType) \
	CHKiRet(objSerializeProp(pCStr, (uchar*) #propName, PROPTYPE_##propType, (void*) &pThis->propName));
#define objSerializePTR(propName, propType) \
	CHKiRet(objSerializeProp(pCStr, (uchar*) #propName, PROPTYPE_##propType, (void*) pThis->propName));
#define DEFobjStaticHelpers static objInfo_t *pObjInfoOBJ = NULL;
#define objGetName(pThis) (((obj_t*) (pThis))->pObjInfo->pszName)
#define objGetObjID(pThis) (((obj_t*) (pThis))->pObjInfo->objID)
#define objGetVersion(pThis) (((obj_t*) (pThis))->pObjInfo->iObjVers)
/* must be called in Constructor: */
#define objConstructSetObjInfo(pThis) ((obj_t*) (pThis))->pObjInfo = pObjInfoOBJ;
#define objDestruct(pThis) (((obj_t*) (pThis))->pObjInfo->objMethods[objMethod_DESTRUCT])(pThis)
#define objSerialize(pThis) (((obj_t*) (pThis))->pObjInfo->objMethods[objMethod_SERIALIZE])

#define OBJSetMethodHandler(methodID, pHdlr) \
	CHKiRet(objInfoSetMethod(pObjInfoOBJ, methodID, (rsRetVal (*)(void*)) pHdlr))

/* prototypes */
rsRetVal objInfoConstruct(objInfo_t **ppThis, objID_t objID, uchar *pszName, int iObjVers, rsRetVal (*pConstruct)(void *), rsRetVal (*pDestruct)(void *));
rsRetVal objInfoSetMethod(objInfo_t *pThis, objMethod_t objMethod, rsRetVal (*pHandler)(void*));
rsRetVal objBeginSerialize(rsCStrObj **ppCStr, obj_t *pObj, size_t iExpectedObjSize);
rsRetVal objSerializePsz(rsCStrObj *pCStr, uchar *psz, size_t len);
rsRetVal objEndSerialize(rsCStrObj **ppCStr, obj_t *pObj);
rsRetVal objSerializeProp(rsCStrObj *pCStr, uchar *pszPropName, propertyType_t propType, void *pUsr);
rsRetVal objRegisterObj(objID_t oID, objInfo_t *pInfo);
rsRetVal objDeserialize(void *ppObj, objID_t objTypeExpected, strm_t *pSerStore);
PROTOTYPEObjClassInit(obj);

#endif /* #ifndef OBJ_H_INCLUDED */
