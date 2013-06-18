/* The regexp object.
 *
 * Module begun 2008-03-05 by Rainer Gerhards, based on some code
 * from syslogd.c
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
#include <regex.h>
#include <string.h>
#include <assert.h>

#include "rsyslog.h"
#include "module-template.h"
#include "obj.h"
#include "regexp.h"

MODULE_TYPE_LIB
MODULE_TYPE_NOKEEP

/* static data */
DEFobjStaticHelpers


/* ------------------------------ methods ------------------------------ */



/* queryInterface function
 * rgerhards, 2008-03-05
 */
BEGINobjQueryInterface(regexp)
CODESTARTobjQueryInterface(regexp)
	if(pIf->ifVersion != regexpCURR_IF_VERSION) { /* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->regcomp = regcomp;
	pIf->regexec = regexec;
	pIf->regerror = regerror;
	pIf->regfree = regfree;
finalize_it:
ENDobjQueryInterface(regexp)


/* Initialize the regexp class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINAbstractObjClassInit(regexp, 1, OBJ_IS_LOADABLE_MODULE) /* class, version */
	/* request objects we use */

	/* set our own handlers */
ENDObjClassInit(regexp)


/* --------------- here now comes the plumbing that makes as a library module --------------- */


BEGINmodExit
CODESTARTmodExit
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_LIB_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */

	CHKiRet(regexpClassInit(pModInfo)); /* must be done after tcps_sess, as we use it */
	/* Initialize all classes that are in our module - this includes ourselfs */
ENDmodInit
/* vi:set ai:
 */
