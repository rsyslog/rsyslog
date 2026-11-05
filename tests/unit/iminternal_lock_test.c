/* SPDX-License-Identifier: Apache-2.0 */
/* Exercise the macOS internal-message mutex fallback on every platform.
 * Observe a real EBUSY before releasing the mutex to a waiting thread; that
 * thread must then acquire it. A mutex held through the deadline must return
 * ETIMEDOUT. A scripted clock/sleep boundary also proves that a sleep which
 * passes the deadline does not permit another lock attempt. The alarm only
 * bounds a broken implementation's test runtime. */
#include "config.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define CHECK(expr)                                       \
    do {                                                  \
        if (!(expr)) {                                    \
            fprintf(stderr, "CHECK failed: %s\n", #expr); \
            exit(1);                                      \
        }                                                 \
    } while (0)

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t gate = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t ready = PTHREAD_COND_INITIALIZER;
static int busy_seen;
static int observe_busy;
static int timeout_held;
static int release_timeout;
static int scripted_clock;
static time_t scripted_seconds;
static int late_attempts;

static int observed_clock_gettime(clockid_t clock, struct timespec *now) {
    if (!scripted_clock) return clock_gettime(clock, now);
    CHECK(clock == CLOCK_MONOTONIC);
    now->tv_sec = scripted_seconds;
    now->tv_nsec = 0;
    return 0;
}

static int observed_nanosleep(const struct timespec *delay, struct timespec *remaining) {
    if (!scripted_clock) return nanosleep(delay, remaining);
    /* Model scheduler delay across expiry. The mutex would be available if
     * incorrectly tried after this sleep, without a real-time test race. */
    scripted_seconds = 2;
    return 0;
}

/* Observe contention without changing the real pthread operation or result. */
static int observed_trylock(pthread_mutex_t *target) {
    if (scripted_clock) {
        if (scripted_seconds >= 2) {
            ++late_attempts;
            return 0;
        }
        return EBUSY;
    }
    const int ret = pthread_mutex_trylock(target);
    if (ret == EBUSY && observe_busy) {
        CHECK(pthread_mutex_lock(&gate) == 0);
        busy_seen = 1;
        CHECK(pthread_cond_signal(&ready) == 0);
        CHECK(pthread_mutex_unlock(&gate) == 0);
    }
    return ret;
}
#define pthread_mutex_trylock observed_trylock
#define clock_gettime observed_clock_gettime
#define nanosleep observed_nanosleep
#include "tools/iminternal-lock.h"
#undef pthread_mutex_trylock
#undef clock_gettime
#undef nanosleep

static void *wait_for_lock(void *unused) {
    (void)unused;
    CHECK(iminternalMutexTimedLock(&mutex, 1000) == 0);
    CHECK(pthread_mutex_unlock(&mutex) == 0);
    return NULL;
}

static void *hold_through_timeout(void *unused) {
    (void)unused;
    CHECK(pthread_mutex_lock(&mutex) == 0);
    CHECK(pthread_mutex_lock(&gate) == 0);
    timeout_held = 1;
    CHECK(pthread_cond_signal(&ready) == 0);
    while (!release_timeout) CHECK(pthread_cond_wait(&ready, &gate) == 0);
    CHECK(pthread_mutex_unlock(&gate) == 0);
    CHECK(pthread_mutex_unlock(&mutex) == 0);
    return NULL;
}

int main(void) {
    pthread_t waiter;
    pthread_t holder;
    alarm(10);
    CHECK(iminternalMutexTimedLock(&mutex, 1000) == 0);
    CHECK(pthread_mutex_unlock(&mutex) == 0);
    CHECK(pthread_mutex_lock(&mutex) == 0);
    CHECK(pthread_mutex_lock(&gate) == 0);
    observe_busy = 1;
    CHECK(pthread_create(&waiter, NULL, wait_for_lock, NULL) == 0);
    while (!busy_seen) CHECK(pthread_cond_wait(&ready, &gate) == 0);
    CHECK(pthread_mutex_unlock(&gate) == 0);
    CHECK(pthread_mutex_unlock(&mutex) == 0);
    CHECK(pthread_join(waiter, NULL) == 0);
    observe_busy = 0;
    CHECK(pthread_mutex_lock(&gate) == 0);
    CHECK(pthread_create(&holder, NULL, hold_through_timeout, NULL) == 0);
    while (!timeout_held) CHECK(pthread_cond_wait(&ready, &gate) == 0);
    CHECK(pthread_mutex_unlock(&gate) == 0);
    CHECK(iminternalMutexTimedLock(&mutex, 30) == ETIMEDOUT);
    CHECK(pthread_mutex_lock(&gate) == 0);
    release_timeout = 1;
    CHECK(pthread_cond_signal(&ready) == 0);
    CHECK(pthread_mutex_unlock(&gate) == 0);
    CHECK(pthread_join(holder, NULL) == 0);
    scripted_clock = 1;
    CHECK(iminternalMutexTimedLock(&mutex, 1000) == ETIMEDOUT);
    CHECK(late_attempts == 0);
    scripted_clock = 0;
    alarm(0);
    puts("internal-message lock contention and timeout passed");
    return 0;
}
