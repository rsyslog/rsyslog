/* Unit coverage for omazuredce request-size and timer-stop helpers.
 *
 * The timer test freezes a waiter after it observed a false stop predicate.
 * The stop helper must block on the wait mutex until pthread_cond_wait() has
 * registered the waiter and released that mutex. A two-second timed condition
 * wait first proves the stopper cannot complete while the waiter holds the
 * mutex, then a second bounded wait proves the paired wakeup completes.
 *
 * The size test verifies that a request which grows beyond its configured cap
 * is classified for retry rather than successful destructive completion.
 */
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>

#include "../../plugins/omazuredce/omazuredce_utils.h"

typedef struct timer_test_state_s {
    pthread_mutex_t batchMutex;
    pthread_cond_t timerCond;
    signed char stopRequested;
    pthread_mutex_t gateMutex;
    pthread_cond_t gateCond;
    int waiterObservedPredicate;
    int stopperStarted;
    int stopperFinished;
    int releaseWaiter;
    int waiterExited;
} timer_test_state_t;

static void *timerWaiter(void *arg) {
    timer_test_state_t *const state = arg;
    int observedStop;

    (void)pthread_mutex_lock(&state->batchMutex);
    observedStop = state->stopRequested;

    (void)pthread_mutex_lock(&state->gateMutex);
    state->waiterObservedPredicate = 1;
    (void)pthread_cond_broadcast(&state->gateCond);
    while (!state->releaseWaiter) {
        (void)pthread_cond_wait(&state->gateCond, &state->gateMutex);
    }
    (void)pthread_mutex_unlock(&state->gateMutex);

    if (!observedStop) {
        (void)pthread_cond_wait(&state->timerCond, &state->batchMutex);
    }
    (void)pthread_mutex_unlock(&state->batchMutex);

    (void)pthread_mutex_lock(&state->gateMutex);
    state->waiterExited = 1;
    (void)pthread_cond_broadcast(&state->gateCond);
    (void)pthread_mutex_unlock(&state->gateMutex);
    return NULL;
}

static void *timerStopper(void *arg) {
    timer_test_state_t *const state = arg;

    (void)pthread_mutex_lock(&state->gateMutex);
    state->stopperStarted = 1;
    (void)pthread_cond_broadcast(&state->gateCond);
    (void)pthread_mutex_unlock(&state->gateMutex);

    omazuredceRequestTimerStop(&state->batchMutex, &state->timerCond, &state->stopRequested);

    (void)pthread_mutex_lock(&state->gateMutex);
    state->stopperFinished = 1;
    (void)pthread_cond_broadcast(&state->gateCond);
    (void)pthread_mutex_unlock(&state->gateMutex);
    return NULL;
}

static int testTimerStopCannotLoseWakeup(void) {
    timer_test_state_t state = {
        PTHREAD_MUTEX_INITIALIZER,
        PTHREAD_COND_INITIALIZER,
        0,
        PTHREAD_MUTEX_INITIALIZER,
        PTHREAD_COND_INITIALIZER,
        0,
        0,
        0,
        0,
        0,
    };
    pthread_t waiter;
    pthread_t stopper;
    struct timespec deadline;
    int waitRet = 0;

    if (pthread_create(&waiter, NULL, timerWaiter, &state) != 0) return 1;

    (void)pthread_mutex_lock(&state.gateMutex);
    while (!state.waiterObservedPredicate) {
        (void)pthread_cond_wait(&state.gateCond, &state.gateMutex);
    }
    (void)pthread_mutex_unlock(&state.gateMutex);

    if (pthread_create(&stopper, NULL, timerStopper, &state) != 0) return 1;

    (void)pthread_mutex_lock(&state.gateMutex);
    while (!state.stopperStarted) {
        (void)pthread_cond_wait(&state.gateCond, &state.gateMutex);
    }

    (void)clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += 2;
    while (!state.stopperFinished && waitRet == 0) {
        waitRet = pthread_cond_timedwait(&state.gateCond, &state.gateMutex, &deadline);
    }
    if (state.stopperFinished) {
        state.releaseWaiter = 1;
        (void)pthread_cond_broadcast(&state.gateCond);
        (void)pthread_mutex_unlock(&state.gateMutex);
        (void)pthread_mutex_lock(&state.batchMutex);
        (void)pthread_cond_signal(&state.timerCond);
        (void)pthread_mutex_unlock(&state.batchMutex);
        (void)pthread_join(waiter, NULL);
        (void)pthread_join(stopper, NULL);
        fputs("timer stop helper completed without acquiring wait mutex\n", stderr);
        return 1;
    }

    state.releaseWaiter = 1;
    (void)pthread_cond_broadcast(&state.gateCond);
    (void)pthread_mutex_unlock(&state.gateMutex);

    waitRet = 0;
    (void)clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += 2;
    (void)pthread_mutex_lock(&state.gateMutex);
    while (!state.waiterExited && waitRet == 0) {
        waitRet = pthread_cond_timedwait(&state.gateCond, &state.gateMutex, &deadline);
    }
    (void)pthread_mutex_unlock(&state.gateMutex);
    if (!state.waiterExited) {
        fprintf(stderr, "timer waiter did not receive stop wakeup: %d\n", waitRet);
        (void)pthread_mutex_lock(&state.batchMutex);
        (void)pthread_cond_signal(&state.timerCond);
        (void)pthread_mutex_unlock(&state.batchMutex);
        (void)pthread_join(waiter, NULL);
        (void)pthread_join(stopper, NULL);
        return 1;
    }

    (void)pthread_join(waiter, NULL);
    (void)pthread_join(stopper, NULL);
    if (!state.stopRequested) {
        fputs("timer stop predicate was not set\n", stderr);
        return 1;
    }
    return 0;
}

static int testOversizeRequestRetries(void) {
    if (omazuredceClassifyRequestSize(4096, 4096) != OMAZUREDCE_REQUEST_FITS) return 1;
    if (omazuredceClassifyRequestSize(4097, 4096) != OMAZUREDCE_REQUEST_RETRY) return 1;
    return 0;
}

int main(void) {
    if (testTimerStopCannotLoseWakeup() != 0) return 1;
    if (testOversizeRequestRetries() != 0) return 1;
    return 0;
}
