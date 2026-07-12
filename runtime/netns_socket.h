/* Definitions for netns_socket API
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
#ifndef INCLUDED_NETNS_SOCKET_H
#define INCLUDED_NETNS_SOCKET_H

#include "rsyslog.h"

/**
 * @brief Create a socket in an optional network namespace.
 * @param fdp Output location for the socket descriptor; set to -1 on failure.
 * @param domain Socket domain passed to socket().
 * @param type Socket type passed to socket().
 * @param protocol Socket protocol passed to socket().
 * @param ns Namespace name under /var/run/netns, or NULL/empty for the
 *        calling thread's current namespace.
 * @return RS_RET_OK on success, or an rsRetVal error code on failure.
 * @details The calling thread is restored to its original namespace before
 *        return. The socket remains associated with the namespace in which it
 *        was created. The original socket errno is preserved across cleanup.
 */
rsRetVal netns_socket(int *fdp, int domain, int type, int protocol, const char *ns);

/**
 * @brief Switch the calling thread to an optional network namespace.
 * @param ns Namespace name under /var/run/netns, or NULL/empty for no change.
 * @return RS_RET_OK on success, or an rsRetVal error code on failure.
 * @details The caller must save and restore the original namespace when a
 *        temporary switch is required.
 */
rsRetVal netns_switch(const char *ns);

/**
 * @brief Open a descriptor for the calling thread's current network namespace.
 * @param fd Output descriptor location, which must contain -1 on entry.
 * @return RS_RET_OK on success, or an rsRetVal error code on failure.
 * @details On platforms without setns support this is a successful no-op and
 *        leaves @p fd equal to -1. The caller owns any descriptor returned.
 */
rsRetVal ATTR_NONNULL() netns_save(int *fd);

/**
 * @brief Restore a network namespace previously saved by netns_save().
 * @param fd Descriptor location returned by netns_save(), or -1 for no change.
 * @return RS_RET_OK on success, or an rsRetVal error code on failure.
 * @details Any nonnegative descriptor is closed and @p fd is reset to -1,
 *        including when restoration fails.
 */
rsRetVal ATTR_NONNULL() netns_restore(int *fd);

#endif /* #ifndef INCLUDED_NETNS_SOCKET_H */
