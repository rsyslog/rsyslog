/* SPDX-License-Identifier: Apache-2.0 */
/* Control one synthetic UDP target's DNS order and an optional dual-stack
 * local name. Numeric source and unrelated lookups use libc. Socket binding
 * and delivery remain real. Own nodes are tagged for the paired free override. */
#include "config.h"
#include <arpa/inet.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static char owned_result[] = "omfwd UDP family fixture";
struct result_node {
    struct addrinfo info;
    union {
        struct sockaddr_in v4;
        struct sockaddr_in6 v6;
    } address;
};

static struct addrinfo *make_result(const int family, const unsigned short port) {
    struct result_node *node = calloc(1, sizeof(*node));
    if (node == NULL) return NULL;
    node->info.ai_family = family;
    node->info.ai_socktype = SOCK_DGRAM;
    node->info.ai_protocol = IPPROTO_UDP;
    node->info.ai_canonname = owned_result;
    if (family == AF_INET) {
        node->address.v4.sin_family = AF_INET;
        node->address.v4.sin_port = htons(port);
        node->address.v4.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        node->info.ai_addr = (struct sockaddr *)&node->address.v4;
        node->info.ai_addrlen = sizeof(node->address.v4);
    } else {
        node->address.v6.sin6_family = AF_INET6;
        node->address.v6.sin6_port = htons(port);
        node->address.v6.sin6_addr = in6addr_loopback;
        node->info.ai_addr = (struct sockaddr *)&node->address.v6;
        node->info.ai_addrlen = sizeof(node->address.v6);
    }
    return &node->info;
}

void freeaddrinfo(struct addrinfo *result) {
    if (result != NULL && result->ai_canonname == owned_result) {
        while (result != NULL) {
            struct addrinfo *next = result->ai_next;
            free(result);
            result = next;
        }
    } else {
        void (*real_free)(struct addrinfo *) = dlsym(RTLD_NEXT, "freeaddrinfo");
        if (real_free == NULL) abort();
        real_free(result);
    }
}

int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **result) {
    const char *target = getenv("RSYSLOG_UDP_FAMILY_TARGET");
    const char *source = getenv("RSYSLOG_UDP_FAMILY_SOURCE");
    const int is_source = node != NULL && source != NULL && strcmp(node, source) == 0;
    if (!is_source && (node == NULL || target == NULL || strcmp(node, target) != 0)) {
        int (*real_get)(const char *, const char *, const struct addrinfo *, struct addrinfo **) =
            dlsym(RTLD_NEXT, "getaddrinfo");
        if (real_get == NULL) abort();
        return real_get(node, service, hints, result);
    }
    char *end = NULL;
    const long port = service == NULL ? 0 : strtol(service, &end, 10);
    if (result == NULL || (service != NULL && (*service == '\0' || *end != '\0' || port < 0 || port > 65535)))
        return EAI_SERVICE;
    *result = NULL;
    const char *order = is_source ? "v6-first" : getenv("RSYSLOG_UDP_FAMILY_ORDER");
    const int first =
        order != NULL && (strcmp(order, "v4-first") == 0 || strcmp(order, "v4-only") == 0) ? AF_INET : AF_INET6;
    const int families[] = {first, first == AF_INET ? AF_INET6 : AF_INET};
    struct addrinfo **tail = result;
    const int count = order != NULL && (strcmp(order, "v6-only") == 0 || strcmp(order, "v4-only") == 0) ? 1 : 2;
    for (int i = 0; i < count; ++i) {
        if (hints != NULL && hints->ai_family != AF_UNSPEC && hints->ai_family != families[i]) continue;
        *tail = make_result(families[i], (unsigned short)port);
        if (*tail == NULL) {
            freeaddrinfo(*result);
            *result = NULL;
            return EAI_MEMORY;
        }
        tail = &(*tail)->ai_next;
    }
    return *result == NULL ? EAI_NONAME : 0;
}

/* Observe only actual successful UDP binds in the dual-source sender. No bind
 * result or socket behavior is changed; both families must really bind. */
int bind(int fd, const struct sockaddr *address, socklen_t length) {
    int (*real_bind)(int, const struct sockaddr *, socklen_t) = dlsym(RTLD_NEXT, "bind");
    if (real_bind == NULL) abort();
    const int result = real_bind(fd, address, length);
    const char *marker = getenv("RSYSLOG_UDP_FAMILY_BIND_MARKER");
    if (result == 0 && marker != NULL) {
        int type;
        socklen_t type_length = sizeof(type);
        if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &type_length) == 0 && type == SOCK_DGRAM) {
            const char *text = address->sa_family == AF_INET6 ? "AF_INET6\n" : "AF_INET\n";
            const size_t size = strlen(text);
            const int output = open(marker, O_WRONLY | O_CREAT | O_APPEND, 0600);
            if (output < 0 || write(output, text, size) != (ssize_t)size || close(output) != 0) _exit(2);
        }
    }
    return result;
}
