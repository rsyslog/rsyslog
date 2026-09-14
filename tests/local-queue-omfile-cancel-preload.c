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
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static ssize_t (*real_write)(int, const void *, size_t);
static int (*real_lock)(pthread_mutex_t *);
static _Thread_local pthread_mutex_t *last_lock;
static _Atomic(pthread_mutex_t *) output_lock;
static atomic_int claimed;
static atomic_int waiting;
static atomic_int cancelled;
static const char *target;
static const char *events;

static void __attribute__((constructor)) initialize(void) {
    real_write = (ssize_t(*)(int, const void *, size_t))dlsym(RTLD_NEXT, "write");
    real_lock = (int (*)(pthread_mutex_t *))dlsym(RTLD_NEXT, "pthread_mutex_lock");
    target = getenv("RSYSLOG_OMFILE_CANCEL_TARGET");
    events = getenv("RSYSLOG_OMFILE_CANCEL_EVENTS");
    if (real_write == NULL || real_lock == NULL || target == NULL || events == NULL) _exit(2);
}

static void mark(const char *const text) {
    const size_t len = strlen(text);
    const int fd = open(events, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0600);
    if (fd == -1 || real_write(fd, text, len) != (ssize_t)len || close(fd) != 0) _exit(2);
}

int pthread_mutex_lock(pthread_mutex_t *const mutex) {
    if (atomic_load_explicit(&output_lock, memory_order_acquire) == mutex &&
        atomic_exchange_explicit(&waiting, 1, memory_order_relaxed) == 0)
        mark("waiting\n");
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
    atomic_store_explicit(&cancelled, 1, memory_order_release);
    mark("cancelled\n");
}

ssize_t write(int fd, const void *buf, size_t len) {
    struct stat actual, expected;
    if (fstat(fd, &actual) != 0 || stat(target, &expected) != 0 || actual.st_dev != expected.st_dev ||
        actual.st_ino != expected.st_ino)
        return real_write(fd, buf, len);
    if (atomic_exchange_explicit(&claimed, 1, memory_order_relaxed) == 0) {
        if (last_lock == NULL || len < 3 || real_write(fd, buf, 3) != 3) _exit(2);
        atomic_store_explicit(&output_lock, last_lock, memory_order_release);
        mark("entered\n");
        pthread_cleanup_push(cancelled_writer, NULL);
        struct timespec remaining = {.tv_sec = 60, .tv_nsec = 0};
        while (nanosleep(&remaining, &remaining) != 0 && errno == EINTR) {
        }
        /* Normal return from the barrier means the expected cancellation failed. */
        _exit(2);
        pthread_cleanup_pop(0);
    }
    if (!atomic_load_explicit(&cancelled, memory_order_acquire) || len != sizeof(" sibling-after-cancel\n") - 1 ||
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
