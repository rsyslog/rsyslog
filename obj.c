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

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "rsyslog.h"
#include "obj.h"

/* static data */

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
 * object shall receive a reference. A constructor MUST be provided for all
 * objects, thus it is in the parameter list.
 * pszName must point to constant pool memory. It is never freed.
 */
rsRetVal objInfoConstruct(objInfo_t **ppThis, objID_t objID, uchar *pszName, rsRetVal (*pDestruct)(void *))
{
	DEFiRet;
	int i;
	objInfo_t *pThis;

	assert(ppThis != NULL);
	assert(pDestruct != NULL);

	if((pThis = calloc(1, sizeof(objInfo_t))) == NULL)
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

	pThis->pszName = pszName;
	pThis->objID = objID;

	pThis->objMethods[0] = pDestruct;
	for(i = 1 ; i < OBJ_NUM_METHODS ; ++i) {
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

/*
 * vi:set ai:
 */
