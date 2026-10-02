/* Resolver helpers with a namespace-compatible interface.
 *
 * Copyright 2026 Cisco Systems, Inc., and/or its affiliates.
 * Copyright 2026 Adiscon GmbH.
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

#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <time.h>

#include "rsyslog.h"
#include "netns_gai.h"

/**
 * @brief Resolve a node and service with the system resolver.
 * @param node Hostname or numeric address to resolve, or NULL.
 * @param service Service name or port string to resolve, or NULL.
 * @param hints Optional getaddrinfo hints.
 * @param res Output result list on success. The caller must free it with
 *        netns_freeaddrinfo().
 * @param ns Reserved for namespace-aware resolver backends; ignored by this
 *        implementation.
 * @return 0 on success, otherwise an EAI_* error code.
 */
int netns_getaddrinfo(
    const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res, const char *ns) {
    (void)ns;
    return getaddrinfo(node, service, hints, res);
}

/**
 * @brief Free an addrinfo list returned by netns_getaddrinfo().
 * @param res Result list to free. NULL is permitted.
 */
void netns_freeaddrinfo(struct addrinfo *res) {
    freeaddrinfo(res);
}

/**
 * @brief Convert an EAI_* resolver error into a human-readable string.
 * @param errcode Resolver error code returned by a netns_* resolver helper.
 * @return Static string describing @p errcode.
 */
const char *netns_gai_strerror(int errcode) {
    return gai_strerror(errcode);
}

/**
 * @brief Reverse-resolve a socket address with the system resolver.
 * @param sa Socket address to inspect.
 * @param salen Length of @p sa.
 * @param host Output buffer for the host name or numeric address.
 * @param hostlen Size of @p host in bytes.
 * @param serv Output buffer for the service name or numeric port.
 * @param servlen Size of @p serv in bytes.
 * @param flags NI_* flags passed through to getnameinfo().
 * @param ns Reserved for namespace-aware resolver backends; ignored by this
 *        implementation.
 * @return 0 on success, otherwise an EAI_* error code.
 */
int netns_getnameinfo(const struct sockaddr *sa,
                      socklen_t salen,
                      char *host,
                      socklen_t hostlen,
                      char *serv,
                      socklen_t servlen,
                      int flags,
                      const char *ns) {
    int savedCancelState;
    (void)ns;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &savedCancelState);
    const int result = getnameinfo(sa, salen, host, hostlen, serv, servlen, flags);
    pthread_setcancelstate(savedCancelState, NULL);
    return result;
}

#ifdef HAVE_GNU_GETADDRINFO_A
/**
 * @brief Submit one or more asynchronous system resolver requests.
 * @param mode GNU getaddrinfo_a mode, typically GAI_WAIT or GAI_NOWAIT.
 * @param list Array of request pointers.
 * @param ent Number of entries in @p list.
 * @param sig Optional asynchronous completion notification.
 * @param ns Reserved for namespace-aware resolver backends; ignored by this
 *        implementation.
 * @return 0 on submission success, otherwise an EAI_* error code.
 */
int netns_getaddrinfo_a(int mode, struct gaicb *list[__restrict_arr], int ent, struct sigevent *sig, const char *ns) {
    (void)ns;
    return getaddrinfo_a(mode, list, ent, sig);
}

/**
 * @brief Wait for one or more asynchronous resolver requests to complete.
 * @param list Array of request pointers to wait on.
 * @param ent Number of entries in @p list.
 * @param timeout Relative timeout, or NULL to wait indefinitely.
 * @return 0 if at least one request completed, otherwise an EAI_* code.
 */
int netns_gai_suspend(const struct gaicb *const list[], int ent, const struct timespec *timeout) {
    return gai_suspend(list, ent, timeout);
}

/**
 * @brief Query the completion state of an asynchronous resolver request.
 * @param req Request object previously passed to netns_getaddrinfo_a().
 * @return EAI_INPROGRESS while pending, 0 on success, or another EAI_* code.
 */
int netns_gai_error(struct gaicb *req) {
    return gai_error(req);
}

/**
 * @brief Attempt to cancel one or more asynchronous resolver requests.
 * @param req Specific request to cancel, or NULL to cancel all outstanding
 *        requests in the active resolver backend.
 * @return GNU getaddrinfo_a cancellation status such as EAI_CANCELED,
 *         EAI_NOTCANCELED, or EAI_ALLDONE.
 */
int netns_gai_cancel(struct gaicb *req) {
    return gai_cancel(req);
}
#endif /* HAVE_GNU_GETADDRINFO_A */
