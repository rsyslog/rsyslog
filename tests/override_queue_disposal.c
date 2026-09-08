/* SPDX-License-Identifier: Apache-2.0 */
/* Pause the final JSON-root release for one marked message. The main executable
 * binds msgDestruct internally, so interpose its external libfastjson release
 * instead. The test never renders or replaces this tree before destruction.
 * A FIFO handshake gives the test an exact unlocked-disposal/idle interleaving;
 * poll bounds a missing release and preserves a nonzero daemon failure oracle.
 */
#include "config.h"
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <json.h>

static int (*real_put)(struct json_object *);
static pthread_once_t resolve_once = PTHREAD_ONCE_INIT;
static int claimed;

static void resolve_put(void) {
    dlerror();
    real_put = (int (*)(struct json_object *))dlsym(RTLD_NEXT, "fjson_object_put");
    if (dlerror() != NULL || real_put == NULL) {
        fputs("queue disposal hook: cannot resolve fjson_object_put\n", stderr);
        _exit(2);
    }
}

int json_object_put(struct json_object *object) {
    struct json_object *marker;
    const char *ready;
    const char *release;
    int pipefd;
    int readyfd;
    struct pollfd fd;
    struct timespec started, now;
    int remaining;
    int polled;
    char byte;

    pthread_once(&resolve_once, resolve_put);
    if (object != NULL && json_object_get_type(object) == json_type_object &&
        json_object_object_get_ex(object, "queue_disposal_gate", &marker) &&
        __atomic_exchange_n(&claimed, 1, __ATOMIC_RELAXED) == 0) {
        ready = getenv("RSYSLOG_QUEUE_DISPOSAL_READY");
        release = getenv("RSYSLOG_QUEUE_DISPOSAL_RELEASE");
        if (ready == NULL || release == NULL) _exit(2);
        pipefd = open(release, O_RDONLY | O_NONBLOCK);
        readyfd = open(ready, O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (pipefd < 0 || readyfd < 0 || write(readyfd, "ready\n", 6) != 6 || close(readyfd) != 0) _exit(2);
        fd.fd = pipefd;
        fd.events = POLLIN;
        fd.revents = 0;
        if (clock_gettime(CLOCK_MONOTONIC, &started) != 0) _exit(2);
        remaining = 30000;
        while ((polled = poll(&fd, 1, remaining)) < 0 && errno == EINTR) {
            /* Shutdown signals the paused worker with SIGTTIN. Keep the
             * handshake alive without extending its bounded deadline. */
            if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) _exit(2);
            remaining = 30000 - (int)((now.tv_sec - started.tv_sec) * 1000 + (now.tv_nsec - started.tv_nsec) / 1000000);
            if (remaining <= 0) _exit(2);
        }
        if (polled != 1 || read(pipefd, &byte, 1) != 1 || close(pipefd) != 0) _exit(2);
    }
    return real_put(object);
}
