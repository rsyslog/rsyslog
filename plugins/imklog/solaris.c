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
#include "unicode-helper.h"
#include "solaris_cddl.h"

/* globals */
static int fklog; // TODO: remove
#ifndef _PATH_KLOG
#	define _PATH_KLOG "/dev/log"
#endif

// HELPER
/* handle some defines missing on more than one platform */
#ifndef SUN_LEN
#define SUN_LEN(su) \
   (sizeof(*(su)) - sizeof((su)->sun_path) + strlen((su)->sun_path))
#endif

int solaris_create_unix_socket(const char *path)
{
	struct sockaddr_un sunx;
	int fd;

return;
	if (path[0] == '\0')
		return -1;

	unlink(path);

	memset(&sunx, 0, sizeof(sunx));
	sunx.sun_family = AF_UNIX;
	(void) strncpy(sunx.sun_path, path, sizeof(sunx.sun_path));
	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd < 0 || bind(fd, (struct sockaddr *) &sunx, SUN_LEN(&sunx)) < 0 ||
	    chmod(path, 0666) < 0) {
		//errmsg.LogError(errno, NO_ERRCODE, "connot create '%s'", path);
		dbgprintf("cannot create %s (%d).\n", path, errno);
		close(fd);
		return -1;
	}
	return fd;
}
// END HELPER


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
perror("XXX");
		rs_strerror_r(err, errStr, sizeof(errStr));
		DBGPRINTF("error %d opening log socket %s: %s\n",
				   GetPath(), errStr);
		iRet = RS_RET_ERR; // TODO: better error code
	}

	RETiRet;
}


/* Read /dev/klog while data are available, split into lines.
 * Contrary to standard BSD syslogd, we do a blocking read. We can
 * afford this as imklog is running on its own threads. So if we have
 * a single file, it really doesn't matter if we wait inside a 1-file
 * select or the read() directly.
 */
static void
readklog(void)
{
	char *p, *q;
	int len, i;
	int iMaxLine;
	uchar bufRcv[4096+1];
	uchar *pRcv = NULL; /* receive buffer */

	iMaxLine = klog_getMaxLine();

	/* we optimize performance: if iMaxLine is below 4K (which it is in almost all
	 * cases, we use a fixed buffer on the stack. Only if it is higher, heap memory
	 * is used. We could use alloca() to achive a similar aspect, but there are so
	 * many issues with alloca() that I do not want to take that route.
	 * rgerhards, 2008-09-02
	 */
	if((size_t) iMaxLine < sizeof(bufRcv) - 1) {
		pRcv = bufRcv;
	} else {
		if((pRcv = (uchar*) malloc(sizeof(uchar) * (iMaxLine + 1))) == NULL)
			iMaxLine = sizeof(bufRcv) - 1; /* better this than noting */
	}

	len = 0;
	for (;;) {
		dbgprintf("----------imklog(BSD) waiting for kernel log line\n");
		i = read(fklog, pRcv + len, iMaxLine - len);
		if (i > 0) {
			pRcv[i + len] = '\0';
		} else {
			if (i < 0 && errno != EINTR && errno != EAGAIN) {
				imklogLogIntMsg(LOG_ERR,
				       "imklog error %d reading kernel log - shutting down imklog",
				       errno);
				fklog = -1;
			}
			break;
		}

		for (p = pRcv; (q = strchr(p, '\n')) != NULL; p = q + 1) {
			*q = '\0';
			Syslog(LOG_INFO, (uchar*) p);
		}
		len = strlen(p);
		if (len >= iMaxLine - 1) {
			Syslog(LOG_INFO, (uchar*)p);
			len = 0;
		}
		if (len > 0)
			memmove(pRcv, p, len + 1);
	}
	if (len > 0)
		Syslog(LOG_INFO, pRcv);

	if(pRcv != NULL && (size_t) iMaxLine >= sizeof(bufRcv) - 1)
		free(pRcv);
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
	//readklog();
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

