/* lookup.c
 * Support for lookup tables in RainerScript.
 *
 * Copyright 2013 Adiscon GmbH.
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
#include <string.h>
#include <assert.h>

#include "rsyslog.h"
#include "errmsg.h"
#include "lookup.h"
#include "msg.h"
#include "rsconf.h"
#include "dirty.h"

/* definitions for objects we access */
DEFobjStaticHelpers
DEFobjCurrIf(errmsg)
DEFobjCurrIf(glbl)

/* static data */

rsRetVal
lookupNew(lookup_t **ppThis, char *modname, char *dynname)
{
	lookup_t *pThis;
	DEFiRet;

	CHKmalloc(pThis = calloc(1, sizeof(lookup_t)));

	*ppThis = pThis;
finalize_it:
	RETiRet;
}
void
lookupDestruct(lookup_t *lookup)
{
	free(lookup);
}

void
lookupModExit(void)
{
	objRelease(glbl, CORE_COMPONENT);
	objRelease(errmsg, CORE_COMPONENT);
}

rsRetVal
lookupModInit(void)
{
	DEFiRet;
	CHKiRet(objGetObjInterface(&obj));
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
finalize_it:
	RETiRet;
}
