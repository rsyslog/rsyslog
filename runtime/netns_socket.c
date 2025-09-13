/* Implementation for netns_socket API
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
#include "netns_socket.h"


/* Change to the given network namespace.
 * This function based on previous implementation
 * of tools/omfwd.c function changeToNs.
 */
rsRetVal netns_switch(const char *ns) {
    DEFiRet;
#ifdef HAVE_SETNS
    int ns_fd = -1;
    char *nsPath = NULL;

    if (ns && *ns) {
        /* Build network namespace path */
        if (asprintf(&nsPath, "/var/run/netns/%s", ns) == -1) {
            // Some implementations say nsPath would be undefined on failure
            nsPath = NULL;
            LogError(0, RS_RET_OUT_OF_MEMORY, "%s: asprintf failed", __func__);
            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        }

        /* Open file descriptor of destination network namespace */
        ns_fd = open(nsPath, O_RDONLY);
        if (ns_fd < 0) {
            LogError(errno, RS_RET_IO_ERROR, "%s: could not open namespace '%s'", __func__, ns);
            ABORT_FINALIZE(RS_RET_IO_ERROR);
        }
        /* Change to the destination network namespace */
        if (setns(ns_fd, CLONE_NEWNET) != 0) {
            LogError(errno, RS_RET_IO_ERROR, "%s: could not change to namespace '%s'", __func__, ns);
            ABORT_FINALIZE(RS_RET_IO_ERROR);
        }
        dbgprintf("%s: changed to network namespace '%s'\n", __func__, ns);
    }
finalize_it:
    free(nsPath);
    if ((ns_fd >= 0) && (close(ns_fd) != 0)) {
        LogError(errno, RS_RET_IO_ERROR, "%s: failed to close namespace '%s'", __func__, ns);
    }
#else  // ndef HAVE_SETNS
    if (ns && *ns) {
        LogError(ENOSYS, RS_RET_VALUE_NOT_SUPPORTED, "%s: could not change to namespace '%s'", __func__, ns);
        ABORT_FINALIZE(RS_RET_VALUE_NOT_SUPPORTED);
    }
finalize_it:
#endif  // ndef HAVE_SETNS
    RETiRet;
}


/* Return to the startup network namespace.
 * This function based on code in tools/omfwd.c
 */
rsRetVal ATTR_NONNULL() netns_restore(int *fd) {
    DEFiRet;

#ifdef HAVE_SETNS
    if (*fd >= 0) {
        if (setns(*fd, CLONE_NEWNET) != 0) {
            LogError(errno, RS_RET_IO_ERROR, "%s: could not return to startup namespace", __func__);
            ABORT_FINALIZE(RS_RET_IO_ERROR);
        }
        dbgprintf("%s: returned to startup network namespace\n", __func__);
    }

finalize_it:
#endif  // def HAVE_SETNS
    if (*fd >= 0 && close(*fd) != 0) {
        LogError(errno, RS_RET_IO_ERROR, "%s: could not close startup namespace fd", __func__);
    }
    *fd = -1;
    RETiRet;
}

/* Save the current network namespace fd
 */
rsRetVal ATTR_NONNULL() netns_save(int *fd) {
    DEFiRet;

    /*
     * The fd must always point to either a valid fd
     * or to -1.  We expect it to be -1 on entry here.
     * To avoid bugs, or possible descriptor leaks,
     * check that it is always -1 on entry.
     */
#ifdef HAVE_SETNS
    if (*fd != -1) {
        LogError(0, RS_RET_CODE_ERR, "%s: called with uninitialized descriptor", __func__);
        ABORT_FINALIZE(RS_RET_CODE_ERR);
    }
    *fd = open("/proc/self/ns/net", O_RDONLY);
    if (*fd == -1) {
        LogError(errno, RS_RET_IO_ERROR, "%s: could not access startup namespace", __func__);
        ABORT_FINALIZE(RS_RET_IO_ERROR);
    }
    dbgprintf("%s: saved startup network namespace\n", __func__);
finalize_it:
#endif  // def HAVE_SETNS
    RETiRet;
}

rsRetVal netns_socket(int *fdp, int domain, int type, int protocol, const char *ns) {
    DEFiRet;
    int fd = -1;

#ifndef HAVE_SETNS
    if (ns && *ns) {
        LogError(0, RS_RET_VALUE_NOT_SUPPORTED, "Network namespaces are not supported");
        ABORT_FINALIZE(RS_RET_VALUE_NOT_SUPPORTED);
    }
#else /* def HAVE_SETNS */
    rsRetVal iRet_restore;
    int ns_fd = -1;

    if (ns && *ns) {
        CHKiRet(netns_save(&ns_fd));
        CHKiRet(netns_switch(ns));
    }
#endif /* def HAVE_SETNS */
    *fdp = fd = socket(domain, type, protocol);
    if (fd == -1) {
        LogError(errno, RS_RET_NO_SOCKET, "%s: socket(%d, %d, %d) failed", __func__, domain, type, protocol);
        ABORT_FINALIZE(RS_RET_NO_SOCKET);
    }
finalize_it:
#ifdef HAVE_SETNS
    iRet_restore = netns_restore(&ns_fd);
    if (iRet == RS_RET_OK) iRet = iRet_restore;
#endif /* def HAVE_SETNS */
    if (iRet != RS_RET_OK && fd != -1) {
        (void)close(fd);
        *fdp = -1;
    }
    RETiRet;
}
