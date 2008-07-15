/* The errmsg object.
 *
 * Module begun 2008-03-05 by Rainer Gerhards, based on some code
 * from syslogd.c. I converted this module to lgpl and have checked that
 * all contributors agreed to that step.
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
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <assert.h>

#include "rsyslog.h"
#include "obj.h"
#include "errmsg.h"
#include "sysvar.h"
#include "srUtils.h"
#include "stringbuf.h"

/* static data */
DEFobjStaticHelpers


/* ------------------------------ methods ------------------------------ */


/* We now receive three parameters: one is the internal error code
 * which will also become the error message number, the second is
 * errno - if it is non-zero, the corresponding error message is included
 * in the text and finally the message text itself. Note that it is not
 * 100% clean to use the internal errcode, as it may be reached from
 * multiple actual error causes. However, it is much better than having
 * no error code at all (and in most cases, a single internal error code
 * maps to a specific error event).
 * rgerhards, 2008-06-27
 */
static void __attribute__((format(printf, 3, 4)))
LogError(int iErrno, int iErrCode, char *fmt, ... )
{
	va_list ap;
	char buf[1024];
	char msg[1024];
	char errStr[1024];
	size_t lenBuf;
	
	BEGINfunc
	assert(fmt != NULL);
	/* Format parameters */
	va_start(ap, fmt);
	lenBuf = vsnprintf(buf, sizeof(buf), fmt, ap);
	if(lenBuf >= sizeof(buf)) {
		/* if our buffer was too small, we simply truncate. */
		lenBuf--;
	}
	va_end(ap);
	
	/* Log the error now */
	buf[sizeof(buf)/sizeof(char) - 1] = '\0'; /* just to be on the safe side... */

	dbgprintf("Called LogError, msg: %s\n", buf);

	if(iErrno != 0) {
		rs_strerror_r(iErrno, errStr, sizeof(errStr));
		if(iErrCode == NO_ERRCODE) {
			snprintf(msg, sizeof(msg), "%s: %s", buf, errStr);
		} else {
			snprintf(msg, sizeof(msg), "%s: %s [try http://www.rsyslog.com/e/%d ]", buf, errStr, iErrCode * -1);
		}
	} else {
		if(iErrCode == NO_ERRCODE) {
			snprintf(msg, sizeof(msg), "%s", buf);
		} else {
			snprintf(msg, sizeof(msg), "%s [try http://www.rsyslog.com/e/%d ]", buf, iErrCode * -1);
		}
	}
	msg[sizeof(msg)/sizeof(char) - 1] = '\0'; /* just to be on the safe side... */
	errno = 0;
	
	glblErrLogger(iErrCode, (uchar*)msg);

	ENDfunc
}


/* queryInterface function
 * rgerhards, 2008-03-05
 */
BEGINobjQueryInterface(errmsg)
CODESTARTobjQueryInterface(errmsg)
	if(pIf->ifVersion != errmsgCURR_IF_VERSION) { /* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->LogError = LogError;
finalize_it:
ENDobjQueryInterface(errmsg)


/* Initialize the errmsg class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINAbstractObjClassInit(errmsg, 1, OBJ_IS_CORE_MODULE) /* class, version */
	/* request objects we use */

	/* set our own handlers */
ENDObjClassInit(errmsg)

/* Exit the class.
 * rgerhards, 2008-04-17
 */
BEGINObjClassExit(errmsg, OBJ_IS_CORE_MODULE) /* class, version */
	/* release objects we no longer need */
ENDObjClassExit(errmsg)

/* vi:set ai:
 */
