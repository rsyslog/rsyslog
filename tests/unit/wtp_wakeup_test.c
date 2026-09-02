/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Verify the queue-mutex-protected WTP waiter protocol. Real pthread waiters
 * register under one mutex, are reserved before individual signals, and clear
 * their reservation after signal, timeout, or deferred cancellation.
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>

#include "rsyslog.h"
#include "wti.h"

#define CHECK(condition)                                                                    \
    do {                                                                                    \
        if (!(condition)) {                                                                 \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            return 1;                                                                       \
        }                                                                                   \
    } while (0)

typedef struct waiter_s {
    wti_t wti;
    pthread_cond_t cond;
    pthread_mutex_t *mutex;
    pthread_cond_t *registered;
    int *registered_count;
    int result;
    int timeout_ms;
} waiter_t;

static void clear_reservation(void *arg) {
    wtiClearWaitReservation(&((waiter_t *)arg)->wti);
}

static void unlock_mutex(void *arg) {
    pthread_mutex_unlock((pthread_mutex_t *)arg);
}

static void *waiter_main(void *arg) {
    waiter_t *const waiter = arg;
    struct timespec deadline;

    if (pthread_mutex_lock(waiter->mutex) != 0) abort();
    waiter->wti.bWaitingForWork = 1;
    waiter->wti.bWakeupReserved = 0;
    ++*waiter->registered_count;
    if (pthread_cond_broadcast(waiter->registered) != 0) abort();
    pthread_cleanup_push(unlock_mutex, waiter->mutex);
    pthread_cleanup_push(clear_reservation, waiter);
    if (waiter->timeout_ms < 0) {
        waiter->result = pthread_cond_wait(&waiter->cond, waiter->mutex);
    } else {
        if (clock_gettime(CLOCK_REALTIME, &deadline) != 0) abort();
        deadline.tv_nsec += (long)waiter->timeout_ms * 1000000L;
        if (deadline.tv_nsec >= 1000000000L) {
            ++deadline.tv_sec;
            deadline.tv_nsec -= 1000000000L;
        }
        waiter->result = pthread_cond_timedwait(&waiter->cond, waiter->mutex, &deadline);
    }
    pthread_cleanup_pop(0);
    wtiClearWaitReservation(&waiter->wti);
    pthread_cleanup_pop(1);
    return NULL;
}

static int wait_until_registered(pthread_mutex_t *mutex, pthread_cond_t *registered, int *count, int expected) {
    CHECK(pthread_mutex_lock(mutex) == 0);
    while (*count < expected) CHECK(pthread_cond_wait(registered, mutex) == 0);
    return pthread_mutex_unlock(mutex);
}

int main(void) {
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t registered = PTHREAD_COND_INITIALIZER;
    waiter_t waiters[3];
    pthread_t threads[3];
    int registered_count = 0;
    int i;

    memset(waiters, 0, sizeof(waiters));
    for (i = 0; i < 3; ++i) {
        CHECK(pthread_cond_init(&waiters[i].cond, NULL) == 0);
        waiters[i].mutex = &mutex;
        waiters[i].registered = &registered;
        waiters[i].registered_count = &registered_count;
        waiters[i].timeout_ms = -1;
        CHECK(pthread_create(&threads[i], NULL, waiter_main, &waiters[i]) == 0);
    }
    CHECK(wait_until_registered(&mutex, &registered, &registered_count, 3) == 0);

    CHECK(pthread_mutex_lock(&mutex) == 0);
    for (i = 0; i < 3; ++i) {
        CHECK(wtiReserveWakeup(&waiters[i].wti));
        CHECK(!wtiReserveWakeup(&waiters[i].wti));
        CHECK(pthread_cond_signal(&waiters[i].cond) == 0);
    }
    CHECK(pthread_mutex_unlock(&mutex) == 0);
    for (i = 0; i < 3; ++i) {
        CHECK(pthread_join(threads[i], NULL) == 0);
        CHECK(waiters[i].result == 0);
        CHECK(!waiters[i].wti.bWaitingForWork && !waiters[i].wti.bWakeupReserved);
        CHECK(pthread_cond_destroy(&waiters[i].cond) == 0);
    }

    waiter_t timeout_waiter = {
        .mutex = &mutex, .registered = &registered, .registered_count = &registered_count, .timeout_ms = 20};
    pthread_t timeout_thread;
    CHECK(pthread_cond_init(&timeout_waiter.cond, NULL) == 0);
    CHECK(pthread_create(&timeout_thread, NULL, waiter_main, &timeout_waiter) == 0);
    CHECK(wait_until_registered(&mutex, &registered, &registered_count, 4) == 0);
    CHECK(pthread_join(timeout_thread, NULL) == 0);
    CHECK(timeout_waiter.result == ETIMEDOUT);
    CHECK(!timeout_waiter.wti.bWaitingForWork && !timeout_waiter.wti.bWakeupReserved);
    CHECK(pthread_cond_destroy(&timeout_waiter.cond) == 0);

    waiter_t cancel_waiter = {
        .mutex = &mutex, .registered = &registered, .registered_count = &registered_count, .timeout_ms = -1};
    pthread_t cancel_thread;
    CHECK(pthread_cond_init(&cancel_waiter.cond, NULL) == 0);
    CHECK(pthread_create(&cancel_thread, NULL, waiter_main, &cancel_waiter) == 0);
    CHECK(wait_until_registered(&mutex, &registered, &registered_count, 5) == 0);
    CHECK(pthread_cancel(cancel_thread) == 0);
    CHECK(pthread_join(cancel_thread, NULL) == 0);
    CHECK(!cancel_waiter.wti.bWaitingForWork && !cancel_waiter.wti.bWakeupReserved);
    CHECK(pthread_cond_destroy(&cancel_waiter.cond) == 0);

    CHECK(pthread_cond_destroy(&registered) == 0);
    CHECK(pthread_mutex_destroy(&mutex) == 0);
    puts("WTP concurrent waiter reservation tests passed");
    return 0;
}
