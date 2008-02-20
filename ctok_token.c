/* ctok_token - implements the token_t class.
 *
 * Module begun 2008-02-20 by Rainer Gerhards
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
#include <ctype.h>
#include <strings.h>
#include <assert.h>

#include "rsyslog.h"
#include "template.h"
#include "ctok_token.h"

/* static data */
DEFobjStaticHelpers


/* Standard-Constructor
 */
BEGINobjConstruct(ctok_token) /* be sure to specify the object type also in END macro! */
ENDobjConstruct(ctok_token)


/* ConstructionFinalizer
 * rgerhards, 2008-01-09
 */
rsRetVal ctok_tokenConstructFinalize(ctok_token_t __attribute__((unused)) *pThis)
{
	DEFiRet;
	RETiRet;
}


/* destructor for the ctok object */
BEGINobjDestruct(ctok_token) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(ctok_token)
	if(pThis->pstrVal != NULL) {
		rsCStrDestruct(&pThis->pstrVal);
	}
ENDobjDestruct(ctok_token)


/* get the rsCStrObj from the token, but do not destruct it. This is meant to
 * be used by a caller who passes on the string to some other function. The
 * caller is responsible for destructing it.
 * rgerhards, 2008-02-20
 */
rsRetVal
ctok_tokenUnlinkCStr(ctok_token_t *pThis, rsCStrObj **ppCStr)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, ctok_token);
	ASSERT(ppCStr != NULL);

	*ppCStr = pThis->pstrVal;
	pThis->pstrVal = NULL;

	RETiRet;
}

BEGINObjClassInit(ctok_token, 1) /* class, version */
	OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, ctok_tokenConstructFinalize);
ENDObjClassInit(ctok_token)

/* vi:set ai:
 */
