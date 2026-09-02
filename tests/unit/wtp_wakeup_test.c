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
    /* Optional deterministic gate: the controller reserves this worker before
     * it may enter its timed wait. entered_wait reports that the next condvar
     * wait is the actual wait being tested, not the setup gate. */
    pthread_cond_t *gate;
    int *gate_open;
    pthread_cond_t *entered_wait;
    int *entered_wait_count;
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
    while (waiter->gate != NULL && !*waiter->gate_open) {
        if (pthread_cond_wait(waiter->gate, waiter->mutex) != 0) abort();
    }
    if (waiter->entered_wait != NULL) {
        ++*waiter->entered_wait_count;
        if (pthread_cond_broadcast(waiter->entered_wait) != 0) abort();
    }
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

static void init_budget_worker(wti_t *worker, const int state, const int waiting, const int reserved) {
    memset(worker, 0, sizeof(*worker));
    INIT_ATOMIC_HELPER_MUT(worker->mutIsRunning);
    ATOMIC_STORE_32BIT(&worker->bIsRunning, &worker->mutIsRunning, state);
    worker->bWaitingForWork = waiting;
    worker->bWakeupReserved = reserved;
    worker->bExiting = 0;
}

static void destruct_budget_worker(wti_t *worker) {
    (void)worker;
    DESTROY_ATOMIC_HELPER_MUT(worker->mutIsRunning);
}

/* This is the non-threaded half of wtpAdviseMaxWorkers(): its caller already
 * holds the queue mutex, so each successful reservation is one exact future
 * consumer and duplicate reservations are excluded by wtiReserveWakeup().
 */
static int reserve_wakeup_budget(wti_t *const *workers, const int nworkers, const int target) {
    int i;
    int reserved = 0;
    int budget = wtiGetWakeupBudget(workers, nworkers, target);

    for (i = 0; i < nworkers && budget > 0; ++i) {
        if (wtiReserveWakeup(workers[i])) {
            --budget;
            ++reserved;
        }
    }
    return reserved;
}

static int check_wakeup_budget(void) {
    wti_t workers[4];
    wti_t *worker_ptrs[4] = {&workers[0], &workers[1], &workers[2], &workers[3]};
    int i;

    /* Two busy workers plus two waiters need exactly one reserved wakeup for
     * target parallelism three; the second waiter must remain unreserved.
     */
    init_budget_worker(&workers[0], WRKTHRD_RUNNING, 0, 0);
    init_budget_worker(&workers[1], WRKTHRD_RUNNING, 0, 0);
    init_budget_worker(&workers[2], WRKTHRD_RUNNING, 1, 0);
    init_budget_worker(&workers[3], WRKTHRD_RUNNING, 1, 0);
    CHECK(wtiGetWakeupBudget(worker_ptrs, 4, 3) == 1);
    CHECK(reserve_wakeup_budget(worker_ptrs, 4, 3) == 1);
    CHECK(workers[2].bWakeupReserved && !workers[3].bWakeupReserved);
    CHECK(wtiGetWakeupBudget(worker_ptrs, 4, 3) == 0);
    CHECK(reserve_wakeup_budget(worker_ptrs, 4, 3) == 0);

    /* A previous producer's reservation is already effective parallelism.
     * A stopped or terminating slot must not hide a needed wakeup. This models
     * the last enqueue while a former busy worker has entered its exit window:
     * marking it exiting makes the already-idle worker the one exact wakeup.
     */
    wtiMarkExiting(&workers[0]);
    workers[1].bWaitingForWork = 1;
    workers[1].bWakeupReserved = 0;
    ATOMIC_STORE_32BIT(&workers[2].bIsRunning, &workers[2].mutIsRunning, WRKTHRD_WAIT_JOIN);
    workers[2].bWaitingForWork = 0;
    workers[2].bWakeupReserved = 0;
    ATOMIC_STORE_32BIT(&workers[3].bIsRunning, &workers[3].mutIsRunning, WRKTHRD_STOPPED);
    workers[3].bWaitingForWork = 0;
    workers[3].bWakeupReserved = 0;
    CHECK(wtiGetWakeupBudget(worker_ptrs, 4, 1) == 1);
    CHECK(reserve_wakeup_budget(worker_ptrs, 4, 1) == 1);
    CHECK(workers[1].bWakeupReserved);
    CHECK(wtiGetWakeupBudget(worker_ptrs, 4, 1) == 0);

    for (i = 0; i < 4; ++i) destruct_budget_worker(&workers[i]);
    return 0;
}

static int check_start_missing_budget(void) {
    wti_t workers[4];
    wti_t *worker_ptrs[4] = {&workers[0], &workers[1], &workers[2], &workers[3]};
    int i;

    /* Two live waiters and target parallelism three require exactly one start.
     * Once that started worker is RUNNING, the re-evaluated budget is two, so
     * both established waiters are reserved rather than left idle.
     */
    init_budget_worker(&workers[0], WRKTHRD_RUNNING, 1, 0);
    init_budget_worker(&workers[1], WRKTHRD_RUNNING, 1, 0);
    init_budget_worker(&workers[2], WRKTHRD_STOPPED, 0, 0);
    init_budget_worker(&workers[3], WRKTHRD_STOPPED, 0, 0);
    CHECK(wtiGetWorkerStartBudget(worker_ptrs, 4, 3) == 1);
    ATOMIC_STORE_32BIT(&workers[2].bIsRunning, &workers[2].mutIsRunning, WRKTHRD_RUNNING);
    CHECK(wtiGetWakeupBudget(worker_ptrs, 4, 3) == 2);
    CHECK(reserve_wakeup_budget(worker_ptrs, 4, 3) == 2);
    CHECK(workers[0].bWakeupReserved && workers[1].bWakeupReserved);
    CHECK(wtiGetWakeupBudget(worker_ptrs, 4, 3) == 0);

    for (i = 0; i < 4; ++i) destruct_budget_worker(&workers[i]);
    return 0;
}

static int check_persistent_first_worker_budget(void) {
    wti_t workers[2];
    wti_t *worker_ptrs[2] = {&workers[0], &workers[1]};

    /* In regular classic queues, wtpStartWrkr marks w0 always-running. When
     * an additional worker is in normal exit cleanup, the final enqueue still
     * has w0 as a live waiter and must reserve exactly that one wakeup.
     */
    init_budget_worker(&workers[0], WRKTHRD_RUNNING, 1, 0);
    workers[0].bAlwaysRunning = 1;
    init_budget_worker(&workers[1], WRKTHRD_RUNNING, 0, 0);
    wtiMarkExiting(&workers[1]);
    CHECK(wtiGetWorkerStartBudget(worker_ptrs, 2, 1) == 0);
    CHECK(wtiGetWakeupBudget(worker_ptrs, 2, 1) == 1);
    CHECK(reserve_wakeup_budget(worker_ptrs, 2, 1) == 1);
    CHECK(workers[0].bWakeupReserved);
    CHECK(wtiGetWakeupBudget(worker_ptrs, 2, 1) == 0);

    destruct_budget_worker(&workers[0]);
    destruct_budget_worker(&workers[1]);
    return 0;
}

static int check_cancel_exit_budget(void) {
    wti_t workers[2];
    wti_t *worker_ptrs[2] = {&workers[0], &workers[1]};

    /* wtiWorkerCancelCleanup() uses wtiMarkExiting() before it restores the
     * cancelled batch. A concurrently waiting peer must therefore become the
     * one exact consumer for target one, rather than the cancelled slot being
     * mistakenly counted as capacity during that cleanup window.
     */
    init_budget_worker(&workers[0], WRKTHRD_RUNNING, 1, 0);
    workers[0].bAlwaysRunning = 1;
    init_budget_worker(&workers[1], WRKTHRD_RUNNING, 0, 0);
    wtiMarkExiting(&workers[1]);
    CHECK(wtiGetWorkerStartBudget(worker_ptrs, 2, 1) == 0);
    CHECK(wtiGetWakeupBudget(worker_ptrs, 2, 1) == 1);
    CHECK(reserve_wakeup_budget(worker_ptrs, 2, 1) == 1);
    CHECK(workers[0].bWakeupReserved);

    destruct_budget_worker(&workers[0]);
    destruct_budget_worker(&workers[1]);
    return 0;
}

int main(void) {
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t registered = PTHREAD_COND_INITIALIZER;
    pthread_cond_t timeout_gate = PTHREAD_COND_INITIALIZER;
    pthread_cond_t cancel_gate = PTHREAD_COND_INITIALIZER;
    pthread_cond_t cancel_entered_wait = PTHREAD_COND_INITIALIZER;
    waiter_t waiters[3];
    pthread_t threads[3];
    int registered_count = 0;
    int i;

    CHECK(check_wakeup_budget() == 0);
    CHECK(check_start_missing_budget() == 0);
    CHECK(check_persistent_first_worker_budget() == 0);
    CHECK(check_cancel_exit_budget() == 0);

    memset(waiters, 0, sizeof(waiters));
    for (i = 0; i < 3; ++i) {
        INIT_ATOMIC_HELPER_MUT(waiters[i].wti.mutIsRunning);
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
        DESTROY_ATOMIC_HELPER_MUT(waiters[i].wti.mutIsRunning);
    }

    int timeout_gate_open = 0;
    waiter_t timeout_waiter = {.mutex = &mutex,
                               .registered = &registered,
                               .registered_count = &registered_count,
                               .gate = &timeout_gate,
                               .gate_open = &timeout_gate_open,
                               .timeout_ms = 0};
    pthread_t timeout_thread;
    INIT_ATOMIC_HELPER_MUT(timeout_waiter.wti.mutIsRunning);
    CHECK(pthread_cond_init(&timeout_waiter.cond, NULL) == 0);
    CHECK(pthread_create(&timeout_thread, NULL, waiter_main, &timeout_waiter) == 0);
    CHECK(wait_until_registered(&mutex, &registered, &registered_count, 4) == 0);
    CHECK(pthread_mutex_lock(&mutex) == 0);
    CHECK(wtiReserveWakeup(&timeout_waiter.wti));
    CHECK(timeout_waiter.wti.bWakeupReserved);
    timeout_gate_open = 1;
    CHECK(pthread_cond_signal(&timeout_gate) == 0);
    CHECK(pthread_mutex_unlock(&mutex) == 0);
    CHECK(pthread_join(timeout_thread, NULL) == 0);
    CHECK(timeout_waiter.result == ETIMEDOUT);
    CHECK(!timeout_waiter.wti.bWaitingForWork && !timeout_waiter.wti.bWakeupReserved);
    CHECK(pthread_cond_destroy(&timeout_waiter.cond) == 0);
    DESTROY_ATOMIC_HELPER_MUT(timeout_waiter.wti.mutIsRunning);

    int cancel_gate_open = 0;
    int cancel_entered_wait_count = 0;
    waiter_t cancel_waiter = {.mutex = &mutex,
                              .registered = &registered,
                              .registered_count = &registered_count,
                              .gate = &cancel_gate,
                              .gate_open = &cancel_gate_open,
                              .entered_wait = &cancel_entered_wait,
                              .entered_wait_count = &cancel_entered_wait_count,
                              .timeout_ms = -1};
    pthread_t cancel_thread;
    INIT_ATOMIC_HELPER_MUT(cancel_waiter.wti.mutIsRunning);
    CHECK(pthread_cond_init(&cancel_waiter.cond, NULL) == 0);
    CHECK(pthread_create(&cancel_thread, NULL, waiter_main, &cancel_waiter) == 0);
    CHECK(wait_until_registered(&mutex, &registered, &registered_count, 5) == 0);
    CHECK(pthread_mutex_lock(&mutex) == 0);
    CHECK(wtiReserveWakeup(&cancel_waiter.wti));
    CHECK(cancel_waiter.wti.bWakeupReserved);
    cancel_gate_open = 1;
    CHECK(pthread_cond_signal(&cancel_gate) == 0);
    while (cancel_entered_wait_count == 0) CHECK(pthread_cond_wait(&cancel_entered_wait, &mutex) == 0);
    CHECK(pthread_mutex_unlock(&mutex) == 0);
    CHECK(pthread_cancel(cancel_thread) == 0);
    CHECK(pthread_join(cancel_thread, NULL) == 0);
    CHECK(!cancel_waiter.wti.bWaitingForWork && !cancel_waiter.wti.bWakeupReserved);
    CHECK(pthread_cond_destroy(&cancel_waiter.cond) == 0);
    DESTROY_ATOMIC_HELPER_MUT(cancel_waiter.wti.mutIsRunning);

    CHECK(pthread_cond_destroy(&registered) == 0);
    CHECK(pthread_cond_destroy(&timeout_gate) == 0);
    CHECK(pthread_cond_destroy(&cancel_gate) == 0);
    CHECK(pthread_cond_destroy(&cancel_entered_wait) == 0);
    CHECK(pthread_mutex_destroy(&mutex) == 0);
    puts("WTP concurrent waiter reservation tests passed");
    return 0;
}
