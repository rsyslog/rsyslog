/* SPDX-License-Identifier: Apache-2.0 */
/* Exercise the macOS internal-message mutex fallback on every platform.
 * Observe a real EBUSY before releasing the mutex to a waiting thread; that
 * thread must then acquire it. A mutex held through the deadline must return
 * ETIMEDOUT. The alarm only bounds a broken implementation's test runtime. */
#include "config.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
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

/* Observe contention without changing the real pthread operation or result. */
static int observed_trylock(pthread_mutex_t *target) {
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
#include "tools/iminternal-lock.h"
#undef pthread_mutex_trylock

static void *wait_for_lock(void *unused) {
    (void)unused;
    CHECK(iminternalMutexTimedLock(&mutex, 1000) == 0);
    CHECK(pthread_mutex_unlock(&mutex) == 0);
    return NULL;
}

int main(void) {
    pthread_t waiter;
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
    CHECK(pthread_mutex_lock(&mutex) == 0);
    CHECK(iminternalMutexTimedLock(&mutex, 30) == ETIMEDOUT);
    CHECK(pthread_mutex_unlock(&mutex) == 0);
    alarm(0);
    puts("internal-message lock contention and timeout passed");
    return 0;
}
