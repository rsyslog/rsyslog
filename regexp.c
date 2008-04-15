/* The regexp object.
 *
 * Module begun 2008-03-05 by Rainer Gerhards, based on some code
 * from syslogd.c
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
#include <regex.h>
#include <string.h>
#include <assert.h>

#include "rsyslog.h"
#include "module-template.h"
#include "obj.h"
#include "regexp.h"

MODULE_TYPE_LIB

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
