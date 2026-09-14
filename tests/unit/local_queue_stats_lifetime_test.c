/* SPDX-License-Identifier: Apache-2.0 */

/*
 * Copyright 2026 Rainer Gerhards and Adiscon GmbH.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 * This test exercises the real statsobj global object list and the local queue
 * adapter's actual pre-read callback. A minimal snapshot provider is deliberate:
 * queue startup is covered by daemon integration tests, while this fixture
 * isolates the adapter lifetime contract. The pre-read hook holds the real
 * list lock; destruction must not complete until that hook is released, and a
 * later native counter walk must find no adapter object. Conditions, never an
 * elapsed-time delay, order the reader, destructor, and release phases.
 */
#include "config.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rsyslog.h"
#include "unicode-helper.h"
#include "obj.h"
#include "queue.h"
#include "queue_local.h"
#include "queue_local_stats.h"
#include "statsobj.h"

DEFobjCurrIf(obj);
DEFobjCurrIf(statsobj);
#define CHECK(condition)                                                                    \
    do {                                                                                    \
        if (!(condition)) {                                                                 \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            exit(1);                                                                        \
        }                                                                                   \
    } while (0)

/*
 * The adapter queries these lock-free snapshot entry points after the test
 * hook. Their zero snapshot is sufficient here: this is a real adapter/list
 * lifetime test, rather than a duplicate of queue routing accounting tests.
 */
void qqueueLocalGetSnapshot(const qqueue_t *const owner, qqueueLocalSnapshot_t *const snapshot) {
    (void)owner;
    memset(snapshot, 0, sizeof(*snapshot));
}

int qqueueLocalGetFrontendSnapshot(const qqueue_t *const owner,
                                   const uint32_t index,
                                   qqueueLocalFrontendSnapshot_t *const snapshot) {
    (void)owner;
    (void)index;
    memset(snapshot, 0, sizeof(*snapshot));
    return 0;
}

typedef struct phase_s {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int pre_read_entered;
    int release_pre_read;
    int destroy_started;
    int destroy_returned;
    int reader_returned;
} phase_t;

typedef struct fixture_s {
    phase_t phase;
    qqueueLocalStats_t *stats;
    rsRetVal reader_status;
} fixture_t;

static void waitFor(phase_t *const phase, int *const predicate) {
    CHECK(pthread_mutex_lock(&phase->mutex) == 0);
    while (!*predicate) {
        CHECK(pthread_cond_wait(&phase->cond, &phase->mutex) == 0);
    }
    CHECK(pthread_mutex_unlock(&phase->mutex) == 0);
}

static void preReadHook(void *const context) {
    phase_t *const phase = context;

    CHECK(pthread_mutex_lock(&phase->mutex) == 0);
    phase->pre_read_entered = 1;
    CHECK(pthread_cond_broadcast(&phase->cond) == 0);
    while (!phase->release_pre_read) {
        CHECK(pthread_cond_wait(&phase->cond, &phase->mutex) == 0);
    }
    CHECK(pthread_mutex_unlock(&phase->mutex) == 0);
}

static rsRetVal countAdapterCounters(void *const context,
                                     const uchar *const object_name,
                                     const uchar *const object_origin,
                                     const uchar *const counter_name,
                                     const statsCtrType_t counter_type,
                                     const uint64_t value,
                                     const int8_t flags) {
    (void)object_origin;
    (void)counter_name;
    (void)counter_type;
    (void)value;
    (void)flags;
    unsigned *const count = context;
    if (object_name != NULL && !strcmp((const char *)object_name, "adapter-lifetime.local")) ++*count;
    return RS_RET_OK;
}

static void *readCounters(void *const context) {
    fixture_t *const fixture = context;
    fixture->reader_status = statsobj.GetAllCounters(countAdapterCounters, &(unsigned){0});

    CHECK(pthread_mutex_lock(&fixture->phase.mutex) == 0);
    fixture->phase.reader_returned = 1;
    CHECK(pthread_cond_broadcast(&fixture->phase.cond) == 0);
    CHECK(pthread_mutex_unlock(&fixture->phase.mutex) == 0);
    return NULL;
}

static void *destroyAdapter(void *const context) {
    fixture_t *const fixture = context;

    CHECK(pthread_mutex_lock(&fixture->phase.mutex) == 0);
    fixture->phase.destroy_started = 1;
    CHECK(pthread_cond_broadcast(&fixture->phase.cond) == 0);
    CHECK(pthread_mutex_unlock(&fixture->phase.mutex) == 0);

    qqueueLocalStatsDestruct(&fixture->stats);

    CHECK(pthread_mutex_lock(&fixture->phase.mutex) == 0);
    CHECK(fixture->phase.release_pre_read);
    fixture->phase.destroy_returned = 1;
    CHECK(pthread_cond_broadcast(&fixture->phase.cond) == 0);
    CHECK(pthread_mutex_unlock(&fixture->phase.mutex) == 0);
    return NULL;
}

int main(void) {
    qqueue_t owner;
    fixture_t fixture;
    pthread_t reader;
    pthread_t destructor;
    unsigned remaining = 0;

    memset(&owner, 0, sizeof(owner));
    memset(&fixture, 0, sizeof(fixture));
    CHECK(pthread_mutex_init(&fixture.phase.mutex, NULL) == 0);
    CHECK(pthread_cond_init(&fixture.phase.cond, NULL) == 0);

    /* This is the production stats-object bootstrap, deliberately narrower
     * than daemon or qqueue startup. The snapshot provider above keeps the
     * fixture focused on adapter/list lifetime. */
    CHECK(objClassInit(NULL) == RS_RET_OK);
    CHECK(objGetObjInterface(&obj) == RS_RET_OK);
    CHECK(statsobjClassInit(NULL) == RS_RET_OK);
    CHECK(objUse(statsobj, CORE_COMPONENT) == RS_RET_OK);
    CHECK(qqueueLocalStatsClassInit() == RS_RET_OK);
    CHECK(qqueueLocalStatsConstruct(&owner, UCHAR_CONSTANT("adapter-lifetime"), 0, 0, &fixture.stats) == RS_RET_OK);
    qqueueLocalStatsSetTestPreReadHook(fixture.stats, preReadHook, &fixture.phase);

    CHECK(pthread_create(&reader, NULL, readCounters, &fixture) == 0);
    waitFor(&fixture.phase, &fixture.phase.pre_read_entered);

    CHECK(pthread_create(&destructor, NULL, destroyAdapter, &fixture) == 0);
    waitFor(&fixture.phase, &fixture.phase.destroy_started);

    /*
     * The reader hook owns statsobj's global list lock. The destructor has
     * begun and has no path to return while the hook remains blocked. Its
     * in-thread assertion below also catches any return before this release.
     */
    CHECK(pthread_mutex_lock(&fixture.phase.mutex) == 0);
    CHECK(!fixture.phase.destroy_returned);
    fixture.phase.release_pre_read = 1;
    CHECK(pthread_cond_broadcast(&fixture.phase.cond) == 0);
    CHECK(pthread_mutex_unlock(&fixture.phase.mutex) == 0);

    CHECK(pthread_join(reader, NULL) == 0);
    CHECK(pthread_join(destructor, NULL) == 0);
    CHECK(fixture.reader_status == RS_RET_OK);
    CHECK(fixture.stats == NULL);

    CHECK(statsobj.GetAllCounters(countAdapterCounters, &remaining) == RS_RET_OK);
    CHECK(remaining == 0);

    CHECK(pthread_cond_destroy(&fixture.phase.cond) == 0);
    CHECK(pthread_mutex_destroy(&fixture.phase.mutex) == 0);
    objRelease(statsobj, CORE_COMPONENT);
    CHECK(statsobjClassExit() == RS_RET_OK);
    CHECK(objClassExit() == RS_RET_OK);
    return 0;
}
