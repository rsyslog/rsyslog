/* The errmsg object.
 *
 * Module begun 2008-03-05 by Rainer Gerhards, based on some code
 * from syslogd.c
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
#include <stdarg.h>
#include <errno.h>
#include <assert.h>

#include "rsyslog.h"
#include "syslogd.h"
#include "obj.h"
#include "errmsg.h"
#include "sysvar.h"
#include "srUtils.h"
#include "stringbuf.h"

/* static data */
DEFobjStaticHelpers


/* ------------------------------ methods ------------------------------ */


/* TODO: restructure this code some time. Especially look if we need
 * to check errno and, if so, how to do that in a clean way.
 */
static void __attribute__((format(printf, 2, 3)))
LogError(int __attribute__((unused)) iErrCode, char *fmt, ... )
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

	if (errno == 0) {
		snprintf(msg, sizeof(msg), "%s", buf);
	} else {
		rs_strerror_r(errno, errStr, sizeof(errStr));
		snprintf(msg, sizeof(msg), "%s: %s", buf, errStr);
	}
	msg[sizeof(msg)/sizeof(char) - 1] = '\0'; /* just to be on the safe side... */
	errno = 0;
	logmsgInternal(LOG_SYSLOG|LOG_ERR, msg, ADDDATE);

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

/* vi:set ai:
 */
