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
#include "var.h"
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

#define objSerializeSCALAR_VAR(strm, propName, propType, var) \
	CHKiRet(obj.SerializeProp(strm, (uchar*) #propName, PROPTYPE_##propType, (void*) &var));
#define objSerializeSCALAR(strm, propName, propType) \
	CHKiRet(obj.SerializeProp(strm, (uchar*) #propName, PROPTYPE_##propType, (void*) &pThis->propName));
#define objSerializePTR(strm, propName, propType) \
	CHKiRet(obj.SerializeProp(strm, (uchar*) #propName, PROPTYPE_##propType, (void*) pThis->propName));
#define DEFobjStaticHelpers \
	static objInfo_t *pObjInfoOBJ = NULL; \
	DEFobjCurrIf(obj)


#define objGetClassName(pThis) (((obj_t*) (pThis))->pObjInfo->pszID)
#define objGetVersion(pThis) (((obj_t*) (pThis))->pObjInfo->iObjVers)
/* the next macro MUST be called in Constructors: */
#ifndef NDEBUG /* this means if debug... */
#	define objConstructSetObjInfo(pThis) \
		ASSERT(((obj_t*) (pThis))->pObjInfo == NULL); \
		((obj_t*) (pThis))->pObjInfo = pObjInfoOBJ; \
		((obj_t*) (pThis))->iObjCooCKiE = 0xBADEFEE
#else
#	define objConstructSetObjInfo(pThis) ((obj_t*) (pThis))->pObjInfo = pObjInfoOBJ
#endif
#define objDestruct(pThis) (((obj_t*) (pThis))->pObjInfo->objMethods[objMethod_DESTRUCT])(&pThis)
#define objSerialize(pThis) (((obj_t*) (pThis))->pObjInfo->objMethods[objMethod_SERIALIZE])
#define objGetSeverity(pThis, piSever) (((obj_t*) (pThis))->pObjInfo->objMethods[objMethod_GETSEVERITY])(pThis, piSever)
#define objDebugPrint(pThis) (((obj_t*) (pThis))->pObjInfo->objMethods[objMethod_DEBUGPRINT])(pThis)

#define OBJSetMethodHandler(methodID, pHdlr) \
	CHKiRet(obj.InfoSetMethod(pObjInfoOBJ, methodID, (rsRetVal (*)(void*)) pHdlr))

/* interfaces */
BEGINinterface(obj) /* name must also be changed in ENDinterface macro! */
	rsRetVal (*UseObj)(char *srcFile, uchar *pObjName, uchar *pObjFile, interface_t *pIf);
	rsRetVal (*ReleaseObj)(char *srcFile, uchar *pObjName, uchar *pObjFile, interface_t *pIf);
	rsRetVal (*InfoConstruct)(objInfo_t **ppThis, uchar *pszID, int iObjVers,
		                  rsRetVal (*pConstruct)(void *), rsRetVal (*pDestruct)(void *),
	      			  rsRetVal (*pQueryIF)(interface_t*), modInfo_t*);
	rsRetVal (*DestructObjSelf)(obj_t *pThis);
	rsRetVal (*BeginSerializePropBag)(strm_t *pStrm, obj_t *pObj);
	rsRetVal (*InfoSetMethod)(objInfo_t *pThis, objMethod_t objMethod, rsRetVal (*pHandler)(void*));
	rsRetVal (*BeginSerialize)(strm_t *pStrm, obj_t *pObj);
	rsRetVal (*SerializeProp)(strm_t *pStrm, uchar *pszPropName, propType_t propType, void *pUsr);
	rsRetVal (*EndSerialize)(strm_t *pStrm);
	rsRetVal (*RegisterObj)(uchar *pszObjName, objInfo_t *pInfo);
	rsRetVal (*UnregisterObj)(uchar *pszObjName, objInfo_t *pInfo);
	rsRetVal (*Deserialize)(void *ppObj, uchar *pszTypeExpected, strm_t *pStrm, rsRetVal (*fFixup)(obj_t*,void*), void *pUsr);
	rsRetVal (*DeserializePropBag)(obj_t *pObj, strm_t *pStrm);
	rsRetVal (*SetName)(obj_t *pThis, uchar *pszName);
	uchar *  (*GetName)(obj_t *pThis);
ENDinterface(obj)
#define objCURR_IF_VERSION 1 /* increment whenever you change the interface structure! */


/* prototypes */
/* the following define *is* necessary, because it provides the root way of obtaining
 * interfaces (at some place we need to start our query...
 */
rsRetVal objGetObjInterface(obj_if_t *pIf);
PROTOTYPEObjClassInit(obj);
PROTOTYPEObjClassExit(obj);

#endif /* #ifndef OBJ_H_INCLUDED */
