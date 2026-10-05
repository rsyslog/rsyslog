#define _GNU_SOURCE
#include "config.h"

#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

typedef ssize_t (*recv_func_t)(int socket, void *buffer, size_t length, int flags);

static recv_func_t real_recv;

static void __attribute__((constructor)) init_real_recv(void) {
    real_recv = (recv_func_t)dlsym(RTLD_NEXT, "recv");
}

ssize_t recv(int socket, void *buffer, size_t length, int flags) {
    static const char payload[] = "X\n";
    static const char sentinel[] = "IMPTCP_RECV_TAIL_SENTINEL";
    const char *const enabled = getenv("RSYSLOG_TEST_IMPTCP_RECV_TAIL");
    ssize_t received;

    if (real_recv == NULL) {
        errno = ENOSYS;
        return -1;
    }

    received = real_recv(socket, buffer, length, flags);
    if (received == (ssize_t)(sizeof(payload) - 1) && length == 128 * 1024 && enabled != NULL && enabled[0] != '\0' &&
        memcmp(buffer, payload, sizeof(payload) - 1) == 0) {
        memcpy(buffer, payload, sizeof(payload) - 1);
        memcpy((char *)buffer + sizeof(payload) - 1, sentinel, sizeof(sentinel));
        return sizeof(payload) - 1;
    }

    return received;
}
