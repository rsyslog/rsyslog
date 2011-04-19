/* rsconf.c - the rsyslog configuration system.
 *
 * Module begun 2011-04-19 by Rainer Gerhards
 *
 * Copyright 2007, 2008 Rainer Gerhards and Adiscon GmbH.
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
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "rsyslog.h"
#include "obj.h"
#include "srUtils.h"
#include "ruleset.h"
#include "rsconf.h"

/* static data */
DEFobjStaticHelpers


/* Standard-Constructor
 */
BEGINobjConstruct(rsconf) /* be sure to specify the object type also in END macro! */
	pThis->templates.root = NULL;
	pThis->templates.last = NULL;
	pThis->templates.lastStatic = NULL;
	pThis->actions.nbrActions = 0;
	CHKiRet(llInit(&pThis->rulesets.llRulesets, rulesetDestructForLinkedList, rulesetKeyDestruct, strcasecmp));
finalize_it:
ENDobjConstruct(rsconf)


/* ConstructionFinalizer
 * rgerhards, 2008-01-09
 */
rsRetVal rsconfConstructFinalize(rsconf_t __attribute__((unused)) *pThis)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, rsconf);
	RETiRet;
}


/* destructor for the rsconf object */
BEGINobjDestruct(rsconf) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(rsconf)
	llDestroy(&(pThis->rulesets.llRulesets));
ENDobjDestruct(rsconf)


/* DebugPrint support for the rsconf object */
BEGINobjDebugPrint(rsconf) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDebugPrint(rsconf)
ENDobjDebugPrint(rsconf)



/* queryInterface function
 */
BEGINobjQueryInterface(rsconf)
CODESTARTobjQueryInterface(rsconf)
	if(pIf->ifVersion != rsconfCURR_IF_VERSION) { /* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->Construct = rsconfConstruct;
	pIf->ConstructFinalize = rsconfConstructFinalize;
	pIf->Destruct = rsconfDestruct;
	pIf->DebugPrint = rsconfDebugPrint;
finalize_it:
ENDobjQueryInterface(rsconf)


/* Initialize the rsconf class. Must be called as the very first method
 * before anything else is called inside this class.
 */
BEGINObjClassInit(rsconf, 1, OBJ_IS_CORE_MODULE) /* class, version */
	/* request objects we use */

	/* now set our own handlers */
	OBJSetMethodHandler(objMethod_DEBUGPRINT, rsconfDebugPrint);
	OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, rsconfConstructFinalize);
ENDObjClassInit(rsconf)


/* De-initialize the rsconf class.
 */
BEGINObjClassExit(rsconf, OBJ_IS_CORE_MODULE) /* class, version */
ENDObjClassExit(rsconf)

/* vi:set ai:
 */
