/* SPDX-License-Identifier: Apache-2.0 */

/*
 * Copyright 2026 Rainer Gerhards and Adiscon GmbH.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 */

/* Exercise the production imtcp lifecycle guard used around the initial
 * endpoint-registry traversal and reload mutations. Control before startup
 * must return RS_RET_RETRY. A condition-gated creator then holds the same
 * guard after thread creation but before publishing its started state; the
 * oracle is pthread_mutex_trylock returning EBUSY, proving that reload cannot
 * traverse or retire the endpoint before publication. After publication,
 * reload can add an endpoint and shutdown can take the guard. The condition
 * gate is deterministic; the 30-second condition deadline is only a hang
 * guard, not a timing or success oracle.
 */
#include "config.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../../plugins/imtcp/imtcp-lifecycle.h"

typedef struct testState_s {
    imtcpLifecycle_t lifecycle;
    pthread_mutex_t gateMut;
    pthread_cond_t gateCond;
    int creatorPaused;
    int allowPublication;
    int threadStarted;
    int activeEndpoints;
} testState_t;

#define CHECK(condition)                                                                  \
    do {                                                                                  \
        if (!(condition)) {                                                               \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
            failed = 1;                                                                   \
            goto cleanup;                                                                 \
        }                                                                                 \
    } while (0)

static struct timespec hangDeadline(void) {
    struct timespec limit = {0};
    if (clock_gettime(CLOCK_REALTIME, &limit) != 0) return limit;
    limit.tv_sec += 30;
    return limit;
}

static void *publishInitialWorker(void *const context) {
    testState_t *const state = context;
    imtcpLifecycleLock(&state->lifecycle);

    /* This condition gate represents the post-pthread_create / pre-
     * thread_started publication window inside the initial runInput walk. */
    pthread_mutex_lock(&state->gateMut);
    state->creatorPaused = 1;
    pthread_cond_broadcast(&state->gateCond);
    while (!state->allowPublication) pthread_cond_wait(&state->gateCond, &state->gateMut);
    pthread_mutex_unlock(&state->gateMut);

    state->threadStarted = 1;
    imtcpLifecycleStartupComplete(&state->lifecycle);
    imtcpLifecycleUnlock(&state->lifecycle);
    return NULL;
}

int main(void) {
    testState_t state = {.lifecycle = IMTCP_LIFECYCLE_INITIALIZER};
    pthread_t creator;
    int failed = 0;
    int gateMutexReady = 0;
    int gateCondReady = 0;
    int creatorStarted = 0;
    int error;

    error = pthread_mutex_init(&state.gateMut, NULL);
    CHECK(error == 0);
    gateMutexReady = 1;
    error = pthread_cond_init(&state.gateCond, NULL);
    CHECK(error == 0);
    gateCondReady = 1;

    /* An outer input-thread creation failure must not leave HUP waiting for a
     * startup event that can never happen. The guard reports retry instead. */
    rsRetVal earlyControl = imtcpLifecycleLockReload(&state.lifecycle);
    if (earlyControl == RS_RET_OK) imtcpLifecycleUnlock(&state.lifecycle);
    CHECK(earlyControl == RS_RET_RETRY);

    error = pthread_create(&creator, NULL, publishInitialWorker, &state);
    CHECK(error == 0);
    creatorStarted = 1;

    const struct timespec limit = hangDeadline();
    pthread_mutex_lock(&state.gateMut);
    while (!state.creatorPaused) {
        error = pthread_cond_timedwait(&state.gateCond, &state.gateMut, &limit);
        if (error != 0) break;
    }
    pthread_mutex_unlock(&state.gateMut);
    CHECK(error == 0);

    error = pthread_mutex_trylock(&state.lifecycle.mut);
    if (error == 0) pthread_mutex_unlock(&state.lifecycle.mut);
    CHECK(error == EBUSY);

    pthread_mutex_lock(&state.gateMut);
    state.allowPublication = 1;
    pthread_cond_broadcast(&state.gateCond);
    pthread_mutex_unlock(&state.gateMut);

    error = pthread_join(creator, NULL);
    if (error == 0) creatorStarted = 0;
    CHECK(error == 0);
    CHECK(state.threadStarted == 1);
    CHECK(state.lifecycle.startupComplete == 1);

    /* Once initial traversal publishes completion, private additions can use
     * the reload side of the same registry guard. The caller owns the guard
     * through the synthetic linked-list update, just as commit does. */
    CHECK(imtcpLifecycleLockReload(&state.lifecycle) == RS_RET_OK);
    ++state.activeEndpoints;
    imtcpLifecycleUnlock(&state.lifecycle);
    CHECK(state.activeEndpoints == 1);

    /* Shutdown uses the unconditional lock for the final registry walk. */
    imtcpLifecycleLock(&state.lifecycle);
    state.activeEndpoints = 0;
    imtcpLifecycleUnlock(&state.lifecycle);
    CHECK(state.activeEndpoints == 0);

cleanup:
    if (creatorStarted) {
        pthread_mutex_lock(&state.gateMut);
        state.allowPublication = 1;
        pthread_cond_broadcast(&state.gateCond);
        pthread_mutex_unlock(&state.gateMut);
        error = pthread_join(creator, NULL);
        if (error != 0) {
            fprintf(stderr, "pthread_join during cleanup failed: %s\n", strerror(error));
            /* The thread may still reference the stack fixture and both
             * mutexes, so leave them intact until main returns. */
            return 1;
        }
    }
    if (gateCondReady) pthread_cond_destroy(&state.gateCond);
    if (gateMutexReady) pthread_mutex_destroy(&state.gateMut);
    pthread_mutex_destroy(&state.lifecycle.mut);
    if (failed) return 1;
    puts("imtcp lifecycle guard tests passed");
    return 0;
}
