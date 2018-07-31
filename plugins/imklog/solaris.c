/* klog driver for solaris
 *
 * This contains OS-specific functionality to read the
 * kernel log. For a general overview, see head comment in
 * imklog.c.
 *
 * This file relies on Sun code in solaris_cddl.c. We have split
 * it from Sun's code to keep the copyright issue as simple as possible.
 *
 * This file is part of rsyslog.
 *
 * Rsyslog is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * If that may be required, an exception is granted to permit linking
 * this code to the code in solaris_cddl.c that is under the cddl license.
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

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>



#include "rsyslog.h"
#include "imklog.h"
#include "srUtils.h"
#include "unicode-helper.h"
#include "solaris_cddl.h"

/* globals */
static int fklog; // TODO: remove
#ifndef _PATH_KLOG
#	define _PATH_KLOG "/dev/log"
#endif


static uchar *GetPath(void)
{
	return pszPath ? pszPath : UCHAR_CONSTANT(_PATH_KLOG);
}

/* open the kernel log - will be called inside the willRun() imklog
 * entry point. -- rgerhards, 2008-04-09
 */
rsRetVal
klogWillRun(void)
{
	DEFiRet;

	fklog = sun_openklog((char*) GetPath(), O_RDONLY);
	if (fklog < 0) {
		char errStr[1024];
		int err = errno;
		rs_strerror_r(err, errStr, sizeof(errStr));
		DBGPRINTF("error %s opening log socket: %s\n",
				   errStr, GetPath());
		iRet = RS_RET_ERR; // TODO: better error code
	}

	RETiRet;
}


/* to be called in the module's AfterRun entry point
 * rgerhards, 2008-04-09
 */
rsRetVal klogAfterRun(void)
{
	DEFiRet;
	if(fklog != -1)
		close(fklog);
	RETiRet;
}



/* to be called in the module's WillRun entry point, this is the main
 * "message pull" mechanism.
 * rgerhards, 2008-04-09
 */
rsRetVal klogLogKMsg(void)
{
	DEFiRet;
	sun_sys_poll();
	RETiRet;
}


/* provide the (system-specific) default facility for internal messages
 * rgerhards, 2008-04-14
 */
int
klogFacilIntMsg(void)
{
	return LOG_SYSLOG;
}

