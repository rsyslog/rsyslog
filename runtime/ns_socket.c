/* Implementation for ns_socket API
 *
 * This file is part of rsyslog.
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
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "rsyslog.h"
#include "debug.h"
#include "errmsg.h"
#include "ns_socket.h"

#ifdef HAVE_SETNS

/* Change to network namespace pData->pszNetworkNamespace.
 * This function based on tools/omfwd.c function changeToNs.
 */
static rsRetVal ns_switch(const uchar *ns) {
    DEFiRet;
    int ns_fd = -1;
    char *nsPath = NULL;

    /* Build network namespace path */
    if (asprintf(&nsPath, "/var/run/netns/%s", ns) == -1) {
        LogError(0, RS_RET_OUT_OF_MEMORY, "%s: asprintf failed", __func__);
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    /* Open file descriptor of destination network namespace */
    ns_fd = open(nsPath, 0);
    if (ns_fd < 0) {
        LogError(0, RS_RET_IO_ERROR, "%s: could not change to namespace '%s': %s", __func__, ns, strerror(errno));
        ABORT_FINALIZE(RS_RET_IO_ERROR);
    }
    /* Change to the destination network namespace */
    if (setns(ns_fd, CLONE_NEWNET) != 0) {
        LogError(0, RS_RET_IO_ERROR, "%s: could not change to namespace '%s': %s", __func__, ns, strerror(errno));
        ABORT_FINALIZE(RS_RET_IO_ERROR);
    }
    dbgprintf("%s: changed to network namespace '%s'\n", __func__, ns);
finalize_it:
    free(nsPath);
    if ((ns_fd >= 0) && (close(ns_fd) != 0)) {
        LogError(0, RS_RET_IO_ERROR, "%s: failed to close namespace '%s': %s", __func__, ns, strerror(errno));
    }
    RETiRet;
}


/* Return to the startup network namespace.
 * This function based on code in tools/omfwd.c
 */
static rsRetVal ATTR_NONNULL() ns_restore(int *fd) {
    DEFiRet;

    if (*fd >= 0) {
        if (setns(*fd, CLONE_NEWNET) != 0) {
            LogError(0, RS_RET_IO_ERROR, "%s: could not return to startup namespace: %s", __func__, strerror(errno));
            ABORT_FINALIZE(RS_RET_IO_ERROR);
        }
        dbgprintf("%s: returned to startup network namespace\n", __func__);
    }

finalize_it:
    if (*fd >= 0 && close(*fd) != 0) {
        LogError(0, RS_RET_IO_ERROR, "%s: could not close startup namespace fd: %s", __func__, strerror(errno));
        *fd = -1;
    }
    RETiRet;
}

/* Save the current network namespace fd
 */
static rsRetVal ATTR_NONNULL() ns_save(int *fd) {
    DEFiRet;

    /*
     * Avoid a descriptor leak if called incorrectly.
     */
    if (*fd >= 0 && close(*fd) != 0)
        LogError(0, RS_RET_IO_ERROR, "%s: could not close previous namespace: %s", __func__, strerror(errno));
    *fd = open("/proc/self/ns/net", O_RDONLY);
    if (*fd == -1) {
        LogError(0, RS_RET_IO_ERROR, "%s: could not access startup namespace: %s", __func__, strerror(errno));
        ABORT_FINALIZE(RS_RET_IO_ERROR);
    }
    dbgprintf("%s: pushed startup network namespace\n", __func__);
finalize_it:
    RETiRet;
}
#endif /* def HAVE_SETNS */

rsRetVal ns_socket(int *fdp, int domain, int type, int protocol, const uchar *ns) {
    DEFiRet;
    int fd = -1;

#ifndef HAVE_SETNS
    if (ns && *ns) {
        LogError(0, RS_RET_IO_ERROR, "Network namespaces are not supported");
        ABORT_FINALIZE(RS_RET_VALUE_NOT_SUPPORTED);
    }
#else /* def HAVE_SETNS */
    rsRetVal iRet_pop;
    int ns_fd = -1;

    if (ns && *ns) {
        CHKiRet(ns_save(&ns_fd));
        CHKiRet(ns_switch(ns));
    }
#endif /* def HAVE_SETNS */
    *fdp = fd = socket(domain, type, protocol);
    if (fd == -1) {
        LogError(0, RS_RET_NO_SOCKET, "%s: socket(%d, %d, %d) failed: %s", __func__, domain, type, protocol,
                 strerror(errno));
        ABORT_FINALIZE(RS_RET_NO_SOCKET);
    }
finalize_it:
#ifdef HAVE_SETNS
    iRet_pop = ns_restore(&ns_fd);
    if (iRet == RS_RET_OK) iRet = iRet_pop;
#endif /* def HAVE_SETNS */
    if (iRet != RS_RET_OK && fd != -1) {
        (void)close(fd);
        *fdp = -1;
    }
    RETiRet;
}
