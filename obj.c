/* obj.c
 *
 * This file implements a generic object "class". All other classes can
 * use the service of this base class here to include auto-destruction and
 * other capabilities in a generic manner.
 * 
 * File begun on 2008-01-04 by RGerhards
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
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "rsyslog.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "obj.h"

/* static data */
static objInfo_t *arrObjInfo[OBJ_NUM_IDS]; /* array with object information pointers */

/* some defines */

/* cookies for serialized lines */
#define COOKIE_OBJLINE  '<'
#define COOKIE_PROPLINE '+'
#define COOKIE_ENDLINE  '>'

/* methods */

/* This is a dummy method to be used when a standard method has not been
 * implemented by an object. Having it allows us to simply call via the 
 * jump table without any NULL pointer checks - which gains quite
 * some performance. -- rgerhards, 2008-01-04
 */
static rsRetVal objInfoNotImplementedDummy(void __attribute__((unused)) *pThis)
{
	return RS_RET_NOT_IMPLEMENTED;
}


/* construct an object Info object. Each class shall do this on init. The
 * resulting object shall be cached during the lifetime of the class and each
 * object shall receive a reference. A constructor and destructor MUST be provided for all
 * objects, thus they are in the parameter list.
 * pszName must point to constant pool memory. It is never freed.
 */
rsRetVal objInfoConstruct(objInfo_t **ppThis, objID_t objID, uchar *pszName, int iObjVers,
                          rsRetVal (*pConstruct)(void *), rsRetVal (*pDestruct)(void *))
{
	DEFiRet;
	int i;
	objInfo_t *pThis;

	assert(ppThis != NULL);
	assert(pDestruct != NULL);

	if((pThis = calloc(1, sizeof(objInfo_t))) == NULL)
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

	pThis->pszName = pszName;
	pThis->iObjVers = iObjVers;
	pThis->objID = objID;

	pThis->objMethods[0] = pConstruct;
	pThis->objMethods[1] = pDestruct;
	for(i = 2 ; i < OBJ_NUM_METHODS ; ++i) {
		pThis->objMethods[i] = objInfoNotImplementedDummy;
	}

	*ppThis = pThis;

finalize_it:
	return iRet;
}


/* set a method handler */
rsRetVal objInfoSetMethod(objInfo_t *pThis, objMethod_t objMethod, rsRetVal (*pHandler)(void*))
{
	assert(pThis != NULL);
	assert(objMethod > 0 && objMethod < OBJ_NUM_METHODS);

	pThis->objMethods[objMethod] = pHandler;

	return RS_RET_OK;
}


/* --------------- object serializiation / deserialization support --------------- */

/* begin serialization of an object - this is a very simple hook. It once wrote the wrapper,
 * now it only constructs the string object. We still leave it in here so that we may utilize
 * it in the future (it is a nice abstraction). iExpcectedObjSize is an optimization setting.
 * It must contain the size (in characters) that the calling object expects the string 
 * representation to grow to. Specifying a bit too large size doesn't hurt. A too-small size
 * does not cause any malfunction, but results in more often memory copies than necessary. So
 * the caller is advised to be conservative in guessing. Binary multiples are recommended.
 * rgerhards, 2008-01-06
 */
rsRetVal objBeginSerialize(rsCStrObj **ppCStr, obj_t *pObj, size_t iExpectedObjSize)
{
	DEFiRet;

	assert(ppCStr != NULL);
	assert(pObj != NULL);

	if((*ppCStr = rsCStrConstruct()) == NULL)
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

	rsCStrSetAllocIncrement(*ppCStr, iExpectedObjSize);

finalize_it:
	return iRet;
}


/* append a property
 */
rsRetVal objSerializeProp(rsCStrObj *pCStr, uchar *pszPropName, propertyType_t propType, void *pUsr)
{
	DEFiRet;
	uchar *pszBuf = NULL;
	size_t lenBuf = 0;
	uchar szBuf[64];

	assert(pCStr != NULL);
	assert(pszPropName != NULL);

	/* if we have no user pointer, there is no need to write this property.
	 * TODO: think if that's the righ point of view
	 * rgerhards, 2008-01-06
	 */
	if(pUsr == NULL) {
		ABORT_FINALIZE(RS_RET_OK);
	}

	switch(propType) {
		case PROPTYPE_PSZ:
			pszBuf = (uchar*) pUsr;
			lenBuf = strlen((char*) pszBuf);
			break;
		case PROPTYPE_SHORT:
			CHKiRet(srUtilItoA((char*) szBuf, sizeof(szBuf), (long) *((short*) pUsr)));
			pszBuf = szBuf;
			lenBuf = strlen((char*) szBuf);
			break;
		case PROPTYPE_INT:
			CHKiRet(srUtilItoA((char*) szBuf, sizeof(szBuf), (long) *((int*) pUsr)));
			pszBuf = szBuf;
			lenBuf = strlen((char*) szBuf);
			break;
		case PROPTYPE_LONG:
			CHKiRet(srUtilItoA((char*) szBuf, sizeof(szBuf), *((long*) pUsr)));
			pszBuf = szBuf;
			lenBuf = strlen((char*) szBuf);
			break;
		case PROPTYPE_CSTR:
			pszBuf = rsCStrGetSzStrNoNULL((rsCStrObj *) pUsr);
			lenBuf = rsCStrLen((rsCStrObj*) pUsr);
			break;
		case PROPTYPE_SYSLOGTIME:
			lenBuf = snprintf((char*) szBuf, sizeof(szBuf), "%d %d %d %d %d %d %d %d %d %c %d %d",
					  ((struct syslogTime*)pUsr)->timeType,
					  ((struct syslogTime*)pUsr)->year,
					  ((struct syslogTime*)pUsr)->month,
					  ((struct syslogTime*)pUsr)->day,
					  ((struct syslogTime*)pUsr)->hour,
					  ((struct syslogTime*)pUsr)->minute,
					  ((struct syslogTime*)pUsr)->second,
					  ((struct syslogTime*)pUsr)->secfrac,
					  ((struct syslogTime*)pUsr)->secfracPrecision,
					  ((struct syslogTime*)pUsr)->OffsetMode,
					  ((struct syslogTime*)pUsr)->OffsetHour,
					  ((struct syslogTime*)pUsr)->OffsetMinute);
			if(lenBuf > sizeof(szBuf) - 1)
				ABORT_FINALIZE(RS_RET_PROVIDED_BUFFER_TOO_SMALL);
			pszBuf = szBuf;
			break;
	}

	/* cookie */
	CHKiRet(rsCStrAppendChar(pCStr, COOKIE_PROPLINE));
	/* name */
	CHKiRet(rsCStrAppendStr(pCStr, pszPropName));
	CHKiRet(rsCStrAppendChar(pCStr, ':'));
	/* type */
	CHKiRet(rsCStrAppendInt(pCStr, (int) propType));
	CHKiRet(rsCStrAppendChar(pCStr, ':'));
	/* length */
	CHKiRet(rsCStrAppendInt(pCStr, lenBuf));
	CHKiRet(rsCStrAppendChar(pCStr, ':'));

	/* data */
	CHKiRet(rsCStrAppendStrWithLen(pCStr, (uchar*) pszBuf, lenBuf));

	/* trailer */
	CHKiRet(rsCStrAppendChar(pCStr, ':'));
	CHKiRet(rsCStrAppendChar(pCStr, '\n'));

finalize_it:
	return iRet;
}


static rsRetVal objSerializeHeader(rsCStrObj **ppCStr, obj_t *pObj, rsCStrObj *pCSObjString, size_t iAllocIncrement)
{
	DEFiRet;
	rsCStrObj *pCStr;

	assert(ppCStr != NULL);
	assert(pObj != NULL);

	if((pCStr = rsCStrConstruct()) == NULL)
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	rsCStrSetAllocIncrement(pCStr, iAllocIncrement);

	/* object cookie and serializer version (so far always 1) */
	CHKiRet(rsCStrAppendChar(pCStr, COOKIE_OBJLINE));
	CHKiRet(rsCStrAppendStr(pCStr, (uchar*) "Obj1"));

	/* object type, version and string length */
	CHKiRet(rsCStrAppendChar(pCStr, ':'));
	CHKiRet(rsCStrAppendInt(pCStr, objGetObjID(pObj)));
	CHKiRet(rsCStrAppendChar(pCStr, ':'));
	CHKiRet(rsCStrAppendInt(pCStr, objGetVersion(pObj)));
	CHKiRet(rsCStrAppendChar(pCStr, ':'));
	CHKiRet(rsCStrAppendInt(pCStr, rsCStrLen(pCSObjString)));

	/* and finally we write the object name - this is primarily meant for
	 * human readers. The idea is that it can be easily skipped when reading
	 * the object back in
	 */
	CHKiRet(rsCStrAppendChar(pCStr, ':'));
	CHKiRet(rsCStrAppendStr(pCStr, objGetName(pObj)));
	/* record trailer */
	CHKiRet(rsCStrAppendChar(pCStr, ':'));
	CHKiRet(rsCStrAppendChar(pCStr, '\n'));

	*ppCStr = pCStr;

finalize_it:
	return iRet;
}


/* end serialization of an object. The caller receives a
 * standard C string, which he must free when no longer needed.
 */
rsRetVal objEndSerialize(rsCStrObj **ppCStr, obj_t *pObj)
{
	DEFiRet;
	rsCStrObj *pCStr = NULL;

	assert(ppCStr != NULL);
	CHKiRet(objSerializeHeader(&pCStr, pObj, *ppCStr, rsCStrGetAllocIncrement(*ppCStr)));

	CHKiRet(rsCStrAppendStrWithLen(pCStr, rsCStrGetBufBeg(*ppCStr), rsCStrLen(*ppCStr)));
	CHKiRet(rsCStrAppendChar(pCStr, COOKIE_ENDLINE));
	CHKiRet(rsCStrAppendStr(pCStr, (uchar*) "EndObj\n\n"));
	CHKiRet(rsCStrFinish(pCStr));

	rsCStrDestruct(*ppCStr);
	*ppCStr = pCStr;

finalize_it:
	if(iRet != RS_RET_OK && pCStr != NULL)
		rsCStrDestruct(pCStr);
	return iRet;
}


/* define a helper to make code below a bit cleaner (and quicker to write) */
#define NEXTC CHKiRet(serialStoreGetChar(pSerStore, &c))//;dbgprintf("c: %c\n", c);

/* de-serialize an (long) integer */
static rsRetVal objDeserializeLong(long *pInt, serialStore_t *pSerStore)
{
	DEFiRet;
	int i;
	uchar c;

	assert(pInt != NULL);

	NEXTC;
	i = 0;
	while(isdigit(c)) {
		i = i * 10 + c - '0';
		NEXTC;
	}

	if(c != ':') ABORT_FINALIZE(RS_RET_INVALID_DELIMITER);

	*pInt = i;
finalize_it:
	return iRet;
}


/* de-serialize a string, length must be provided */
static rsRetVal objDeserializeStr(rsCStrObj **ppCStr, int iLen, serialStore_t *pSerStore)
{
	DEFiRet;
	int i;
	uchar c;
	rsCStrObj *pCStr = NULL;

	assert(ppCStr != NULL);
	assert(iLen > 0);

	if((pCStr = rsCStrConstruct()) == NULL)
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

	NEXTC;
dbgprintf("deserializestring, c: %c (%x)\n", c, c);
	for(i = 0 ; i < iLen ; ++i) {
		CHKiRet(rsCStrAppendChar(pCStr, c));
		NEXTC;
	}
	CHKiRet(rsCStrFinish(pCStr));

	/* check terminator */
	if(c != ':') ABORT_FINALIZE(RS_RET_INVALID_DELIMITER);

	*ppCStr = pCStr;

finalize_it:
	if(iRet != RS_RET_OK && pCStr != NULL)
		rsCStrDestruct(pCStr);

	return iRet;
}


/* de-serialize an object header
 * rgerhards, 2008-01-07
 */
static rsRetVal objDeserializeHeader(objID_t *poID, int* poVers, serialStore_t *pSerStore)
{
	DEFiRet;
	long ioID;
	long oVers;
	uchar c;

	assert(poID != NULL);
	assert(poVers != NULL);

	/* check header cookie */
	NEXTC; if(c != COOKIE_OBJLINE) ABORT_FINALIZE(RS_RET_INVALID_HEADER);
	NEXTC; if(c != 'O') ABORT_FINALIZE(RS_RET_INVALID_HEADER);
	NEXTC; if(c != 'b') ABORT_FINALIZE(RS_RET_INVALID_HEADER);
	NEXTC; if(c != 'j') ABORT_FINALIZE(RS_RET_INVALID_HEADER);
	NEXTC; if(c != '1') ABORT_FINALIZE(RS_RET_INVALID_HEADER_VERS);
	NEXTC; if(c != ':') ABORT_FINALIZE(RS_RET_INVALID_HEADER_VERS);

	/* object type and version and string length */
	CHKiRet(objDeserializeLong(&ioID, pSerStore));
	CHKiRet(objDeserializeLong(&oVers, pSerStore));

	if(ioID < 1 || ioID >= OBJ_NUM_IDS)
		ABORT_FINALIZE(RS_RET_INVALID_OID);

	/* and now we skip over the rest until the delemiting \n */
	NEXTC;
	while(c != '\n')
		NEXTC;

	*poID = (objID_t) ioID;
	*poVers = oVers;

finalize_it:
dbgprintf("DeserializeHeader oid: %ld, vers: %ld, iRet: %d\n", ioID, oVers, iRet);
	return iRet;
}


/* Deserialize a single property. Pointer must be positioned at begin of line. Whole line
 * up until the \n is read.
 */
static rsRetVal objDeserializeProperty(property_t *pProp, serialStore_t *pSerStore)
{
	DEFiRet;
	long i;
	long iLen;
	uchar c;

	assert(pProp != NULL);

	/* check cookie */
	NEXTC;
	if(c != COOKIE_PROPLINE) {
		/* oops, we've read one char that does not belong to use - unget it first */
		CHKiRet(serialStoreUngetChar(pSerStore, c));
		ABORT_FINALIZE(RS_RET_NO_PROPLINE);
	}

	/* get the property name first */
	if((pProp->pcsName = rsCStrConstruct()) == NULL)
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

	NEXTC;
	while(c != ':') {
		CHKiRet(rsCStrAppendChar(pProp->pcsName, c));
		NEXTC;
	}
	CHKiRet(rsCStrFinish(pProp->pcsName));

	/* property type */
	CHKiRet(objDeserializeLong(&i, pSerStore));
	pProp->propType = i;

	/* size (needed for strings) */
	CHKiRet(objDeserializeLong(&iLen, pSerStore));

	/* we now need to deserialize the value */
dbgprintf("deserialized property name '%s', type %d, size %ld, c: %c\n", rsCStrGetSzStrNoNULL(pProp->pcsName), pProp->propType, iLen, c);
	switch(pProp->propType) {
		case PROPTYPE_PSZ:
			CHKiRet(objDeserializeStr(&pProp->val.vpCStr, iLen, pSerStore));
			break;
		case PROPTYPE_SHORT:
			CHKiRet(objDeserializeLong(&i, pSerStore));
			pProp->val.vShort = i;
			break;
		case PROPTYPE_INT:
			CHKiRet(objDeserializeLong(&i, pSerStore));
			pProp->val.vInt = i;
			break;
		case PROPTYPE_LONG:
			CHKiRet(objDeserializeLong(&pProp->val.vLong, pSerStore));
			break;
		case PROPTYPE_CSTR:
			CHKiRet(objDeserializeStr(&pProp->val.vpCStr, iLen, pSerStore));
			break;
		case PROPTYPE_SYSLOGTIME:
			/* dummy */ NEXTC; while(c != ':') NEXTC;
			break;
	}

	/* we should now be at the end of the line. So the next char must be \n */
	NEXTC;
	if(c != '\n') ABORT_FINALIZE(RS_RET_INVALID_PROPFRAME);

finalize_it:
	return iRet;
}


/* de-serialize an object trailer. This does not get any data but checks if the
 * format is ok.
 * rgerhards, 2008-01-07
 */
static rsRetVal objDeserializeTrailer(serialStore_t *pSerStore)
{
	DEFiRet;
	uchar c;

	/* check header cookie */
	NEXTC; if(c != COOKIE_ENDLINE) ABORT_FINALIZE(RS_RET_INVALID_TRAILER);
	NEXTC; if(c != 'E')  ABORT_FINALIZE(RS_RET_INVALID_TRAILER);
	NEXTC; if(c != 'n')  ABORT_FINALIZE(RS_RET_INVALID_TRAILER);
	NEXTC; if(c != 'd')  ABORT_FINALIZE(RS_RET_INVALID_TRAILER);
	NEXTC; if(c != 'O')  ABORT_FINALIZE(RS_RET_INVALID_TRAILER);
	NEXTC; if(c != 'b')  ABORT_FINALIZE(RS_RET_INVALID_TRAILER);
	NEXTC; if(c != 'j')  ABORT_FINALIZE(RS_RET_INVALID_TRAILER);
	NEXTC; if(c != '\n') ABORT_FINALIZE(RS_RET_INVALID_TRAILER);
	NEXTC; if(c != '\n') ABORT_FINALIZE(RS_RET_INVALID_TRAILER);

dbgprintf("obj trailer OK\n");
finalize_it:
	return iRet;
}


/* De-Serialize an object.
 * Params: Pointer to object Pointer (pObj) (like a obj_t**, but can not do that due to compiler warning)
 * expected object ID (to check against)
 * Function that returns the next character from the serialized object (from file, memory, whatever)
 * Pointer to be passed to the function
 * The caller must destruct the created object.
 * rgerhards, 2008-01-07
 */
rsRetVal objDeserialize(void *ppObj, objID_t objTypeExpected, serialStore_t *pSerStore)
{
	DEFiRet;
	obj_t *pObj = NULL;
	property_t propBuf;
	objID_t oID = 0; /* this assignment is just to supress a compiler warning - this saddens me */
	int oVers = 0;   /* after all, it is totally useless but takes up some execution time...    */

	assert(ppObj != NULL);
	assert(objTypeExpected > 0 && objTypeExpected < OBJ_NUM_IDS);
	assert(pSerStore != NULL);

	CHKiRet(objDeserializeHeader(&oID, &oVers, pSerStore));

	if(oID != objTypeExpected)
		ABORT_FINALIZE(RS_RET_INVALID_OID);
	CHKiRet(arrObjInfo[oID]->objMethods[objMethod_CONSTRUCT](&pObj));

	/* we got the object, now we need to fill the properties */
	iRet = objDeserializeProperty(&propBuf, pSerStore);
	while(iRet == RS_RET_OK) {
		CHKiRet(arrObjInfo[oID]->objMethods[objMethod_SETPROPERTY](pObj, &propBuf));
		iRet = objDeserializeProperty(&propBuf, pSerStore);
	}
	rsCStrDestruct(propBuf.pcsName); /* todo: a destructor would be nice here... -- rger, 2008-01-07 */

	if(iRet != RS_RET_NO_PROPLINE)
		FINALIZE;
dbgprintf("good propline loop exit\n");

	CHKiRet(objDeserializeTrailer(pSerStore)); /* do trailer checks */

	*((obj_t**) ppObj) = pObj;

finalize_it:
	return iRet;
}

#undef NEXTC /* undef helper macro */


/* --------------- end object serializiation / deserialization support --------------- */

/* register a classe's info pointer, so that we can reference it later, if needed to
 * (e.g. for de-serialization support).
 * rgerhards, 2008-01-07
 */
rsRetVal objRegisterObj(objID_t oID, objInfo_t *pInfo)
{
	DEFiRet;

	assert(pInfo != NULL);
	if(oID < 1 || oID > OBJ_NUM_IDS)
		ABORT_FINALIZE(RS_RET_INVALID_OID);
	
	arrObjInfo[oID] = pInfo;

finalize_it:
	return iRet;
}


/* initialize our own class */
rsRetVal objClassInit(void)
{
	int i;

	for(i = 0 ; i < OBJ_NUM_IDS ; ++i) {
		arrObjInfo[i] = NULL;
	}

	return RS_RET_OK;
}

/*
 * vi:set ai:
 */
