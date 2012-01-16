/* ctok_token - implements the token_t class.
 *
 * Module begun 2008-02-20 by Rainer Gerhards
 *
 * Copyright 2008-2012 Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *       http://www.apache.org/licenses/LICENSE-2.0
 *       -or-
 *       see COPYING.ASL20 in the source distribution
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
