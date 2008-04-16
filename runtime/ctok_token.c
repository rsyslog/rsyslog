/* ctok_token - implements the token_t class.
 *
 * Module begun 2008-02-20 by Rainer Gerhards
 *
 * Copyright 2008 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
 *
 * The rsyslog runtime library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The rsyslog runtime library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the rsyslog runtime library.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 * A copy of the LGPL can be found in the file "COPYING.LESSER" in this distribution.
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
DEFobjCurrIf(var)


/* Standard-Constructor
 */
BEGINobjConstruct(ctok_token) /* be sure to specify the object type also in END macro! */
	/* TODO: we may optimize the code below and alloc var only if actually
	 * needed (but we need it quite often)
	 */
	CHKiRet(var.Construct(&pThis->pVar));
	CHKiRet(var.ConstructFinalize(pThis->pVar));
finalize_it:
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
	if(pThis->pVar != NULL) {
		var.Destruct(&pThis->pVar);
	}
ENDobjDestruct(ctok_token)


/* get the cstr_t from the token, but do not destruct it. This is meant to
 * be used by a caller who passes on the string to some other function. The
 * caller is responsible for destructing it.
 * rgerhards, 2008-02-20
 */
static rsRetVal
ctok_tokenUnlinkVar(ctok_token_t *pThis, var_t **ppVar)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, ctok_token);
	ASSERT(ppVar != NULL);

	*ppVar = pThis->pVar;
	pThis->pVar = NULL;

	RETiRet;
}


/* tell the caller if the supplied token is a compare operation */
static int ctok_tokenIsCmpOp(ctok_token_t *pThis)
{
	return(pThis->tok >= ctok_CMP_EQ && pThis->tok <= ctok_CMP_GTEQ);
}

/* queryInterface function
 * rgerhards, 2008-02-21
 */
BEGINobjQueryInterface(ctok_token)
CODESTARTobjQueryInterface(ctok_token)
	if(pIf->ifVersion != ctok_tokenCURR_IF_VERSION) { /* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	//xxxpIf->oID = OBJctok_token;

	pIf->Construct = ctok_tokenConstruct;
	pIf->ConstructFinalize = ctok_tokenConstructFinalize;
	pIf->Destruct = ctok_tokenDestruct;
	pIf->UnlinkVar = ctok_tokenUnlinkVar;
	pIf->IsCmpOp = ctok_tokenIsCmpOp;
finalize_it:
ENDobjQueryInterface(ctok_token)


BEGINObjClassInit(ctok_token, 1, OBJ_IS_CORE_MODULE) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(var, CORE_COMPONENT));

	OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, ctok_tokenConstructFinalize);
ENDObjClassInit(ctok_token)

/* vi:set ai:
 */
