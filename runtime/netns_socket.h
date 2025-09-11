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

/*
 * Open a socket in the named network namespace
 */
rsRetVal netns_socket(int *fdp, int domain, int type, int protocol, const char *ns);

/*
 * Switch to the named networknamespace
 */
rsRetVal netns_switch(const char *ns);

/*
 * Save and restore our current network namespace
 */
rsRetVal ATTR_NONNULL() netns_save(int *fd);
rsRetVal ATTR_NONNULL() netns_restore(int *fd);

#endif /* #ifndef INCLUDED_NETNS_SOCKET_H */
