/* SPDX-License-Identifier: Apache-2.0 */
/* Deterministically interrupt a real plain-file callback after a partial write.
 * The first writer parks with omfile's mutex held. A distinct writer announces
 * its attempt to take that exact mutex before shutdown begins. After the first
 * is cancelled, the second must reach write with its own intact payload. The
 * reused marker proves shared stream access; it is not a delivery receipt.
 * No cancellation request is intercepted or suppressed. The test's 60-second
 * alarm is only hang protection for a missing shutdown request. */
#include "config.h"
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static ssize_t (*real_write)(int, const void *, size_t);
static int (*real_lock)(pthread_mutex_t *);
static __thread pthread_mutex_t *last_lock;
static pthread_mutex_t *output_lock;
static int claimed;
static int waiting;
static int stopping;
static pthread_mutex_t monitor_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t monitor_cond = PTHREAD_COND_INITIALIZER;
static pthread_t monitor;
static int cancelled;
static const char *target;
static const char *events;

static void mark(const char *const text) {
    const size_t len = strlen(text);
    const int fd = open(events, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0600);
    if (fd == -1 || real_write(fd, text, len) != (ssize_t)len || close(fd) != 0) _exit(2);
}

/* Publish readiness from a separate observer. The actual waiter must have no
 * cancellation point between declaring its lock attempt and acquiring mutWrite;
 * writing the marker from that waiter would introduce precisely such a gap. */
static void *observe_waiter(void *unused) {
    (void)unused;
    pthread_mutex_lock(&monitor_mutex);
    while (!waiting && !stopping) pthread_cond_wait(&monitor_cond, &monitor_mutex);
    const int observed = waiting;
    pthread_mutex_unlock(&monitor_mutex);
    if (observed) mark("waiting\n");
    return NULL;
}

static void __attribute__((constructor)) initialize(void) {
    real_write = (ssize_t(*)(int, const void *, size_t))dlsym(RTLD_NEXT, "write");
    real_lock = (int (*)(pthread_mutex_t *))dlsym(RTLD_NEXT, "pthread_mutex_lock");
    target = getenv("RSYSLOG_OMFILE_CANCEL_TARGET");
    events = getenv("RSYSLOG_OMFILE_CANCEL_EVENTS");
    if (real_write == NULL || real_lock == NULL || target == NULL || events == NULL) _exit(2);
    if (pthread_create(&monitor, NULL, observe_waiter, NULL) != 0) _exit(2);
}

static void __attribute__((destructor)) finalize(void) {
    pthread_mutex_lock(&monitor_mutex);
    stopping = 1;
    pthread_cond_signal(&monitor_cond);
    pthread_mutex_unlock(&monitor_mutex);
    if (pthread_join(monitor, NULL) != 0) _exit(2);
}

int pthread_mutex_lock(pthread_mutex_t *const mutex) {
    if (__atomic_load_n(&output_lock, __ATOMIC_ACQUIRE) == mutex) {
        pthread_mutex_lock(&monitor_mutex);
        waiting = 1;
        pthread_cond_signal(&monitor_cond);
        pthread_mutex_unlock(&monitor_mutex);
    }
    const int result = real_lock(mutex);
    if (result == 0) last_lock = mutex;
    return result;
}

static void cancelled_writer(void *unused) {
    (void)unused;
    /* Test-only marker I/O is completed with cancellation disabled so observing
     * cancelled means the inner write barrier has unwound. Production omfile
     * cleanup subsequently releases the output mutex without I/O. */
    int oldstate;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
    __atomic_store_n(&cancelled, 1, __ATOMIC_RELEASE);
    mark("cancelled\n");
}

ssize_t write(int fd, const void *buf, size_t len) {
    struct stat actual, expected;
    if (fstat(fd, &actual) != 0 || stat(target, &expected) != 0 || actual.st_dev != expected.st_dev ||
        actual.st_ino != expected.st_ino)
        return real_write(fd, buf, len);
    if (__atomic_exchange_n(&claimed, 1, __ATOMIC_RELAXED) == 0) {
        pthread_cleanup_push(cancelled_writer, NULL);
        if (last_lock == NULL || len < 3 || real_write(fd, buf, 3) != 3) _exit(2);
        __atomic_store_n(&output_lock, last_lock, __ATOMIC_RELEASE);
        mark("entered\n");
        struct timespec remaining = {.tv_sec = 60, .tv_nsec = 0};
        while (nanosleep(&remaining, &remaining) != 0 && errno == EINTR) {
        }
        /* Normal return from the barrier means the expected cancellation failed. */
        _exit(2);
        pthread_cleanup_pop(0);
    }
    if (!__atomic_load_n(&cancelled, __ATOMIC_ACQUIRE) || len != sizeof(" sibling-after-cancel\n") - 1 ||
        memcmp(buf, " sibling-after-cancel\n", len) != 0)
        _exit(2);
    /* A pending cancellation may fire at this marker's open/write. The mutex
     * waiter cannot be cancelled at pthread_mutex_lock; temporarily postpone
     * cancellation for marker-only local I/O, then restore before real write. */
    int oldstate;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
    mark("reused\n");
    pthread_setcancelstate(oldstate, NULL);
    return real_write(fd, buf, len);
}
