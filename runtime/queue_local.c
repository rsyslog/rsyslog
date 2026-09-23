/* Experimental local queues with a shared memory/DA backend.
 * Copyright 2026 Adiscon GmbH.
 * Licensed under the Apache License, Version 2.0.
 *
 * Concurrency & Locking:
 * - A fixed family registry is protected only for first registration/closure.
 * - Each registered actual producer owns one SPSC publication endpoint. Its
 *   consumer owns one wti, one lease, and all output-module worker state.
 * - Seq-cst REDIRECT/publishing handshakes close registration/publication races.
 *   Publication notification uses only that FE's mutex, never the BE mutex.
 * - FE callbacks and destruction run without the FE mutex. Only after join may
 *   shutdown take the consumer endpoint and retained lease. Descriptors and
 *   workers remain allocated until logical destruction and lease reconciliation.
 * - BE admission/transfer and its condition predicates use the ordinary BE
 *   mutex. Snapshot callbacks acquire no mutex and never change registration.
 */
#include "config.h"
#include <assert.h>
#include <errno.h>
#ifdef ENABLE_TESTBENCH
    #include <fcntl.h>
    #include <sys/stat.h>
#endif
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "rsyslog.h"
#include "queue.h"
#include "queue_local.h"
#include "queue_lease.h"
#include "queue_spsc.h"
#include "wti.h"
#include "wtp.h"
#include "errmsg.h"
#include "rsconf.h"
#include "action.h"
#include "srUtils.h"

/* Local scope fails closed when its atomic/monotonic primitives are absent.
 * In particular, do not emit out-of-line 64-bit atomics on unsupported targets
 * and accidentally require a new libatomic dependency for global queues. */
#if defined(HAVE_ATOMIC_BUILTINS) && defined(__GCC_ATOMIC_LLONG_LOCK_FREE) && __GCC_ATOMIC_LLONG_LOCK_FREE == 2 && \
    defined(_POSIX_CLOCK_SELECTION) && _POSIX_CLOCK_SELECTION >= 0

enum { LOCAL_RUNNING, LOCAL_REDIRECT, LOCAL_CLOSED, LOCAL_STOPPED };
enum { FE_UNUSED, FE_REGISTERING, FE_RUNNING, FE_FAILED, FE_JOINED };

struct qqueueLocalFrontend_s {
    qqueue_t *source;
    qqueue_t *owner;
    wtp_t *pool;
    rs_spsc_queue_t ring;
    void **slots;
    void **producerScratch;
    void **consumerScratch;
    pthread_mutex_t mutex;
    pthread_cond_t publisherDone;
    unsigned mutexInitialized, condInitialized, state, publishing, producerExited;
    uint32_t index, capacity, batchSize, helperLimit;
    /* List links require BE mutex; membership transitions additionally hold FE.
     * No FE publisher ever takes BE. */
    struct qqueueLocalFrontend_s *idleNext, *idlePrev;
    unsigned idleListed, helpWakePending;
    uint64_t help_attempts, help_empty, help_batches, help_messages, help_max, help_active, help_retry;
    uint64_t help_terminal, help_waits, help_wakes, help_completed, help_returned, wake_fe, wake_shutdown;
    uint64_t attempts, published, dequeued, terminal, active, retry, discarded;
    uint64_t overflow, nofit, oversized, transferred, shutdownDiscarded, bytes, batches;
    uint64_t publishedBatches, dequeueMax, dequeueMessages;
    uint64_t submittedBatches, submittedMax, overflowBatches, oversizedBatches;
};

struct qqueueLocal_s {
    qqueue_t *owner;
    qqueueLocalFrontend_t *fronts;
    qqueueLocalFrontend_t *idleHelpers; /* BE mutex */
    pthread_mutex_t registry;
    pthread_cond_t space;
    pthread_cond_t admissionDone;
    unsigned registryInitialized, spaceInitialized, admissionInitialized;
    unsigned state, count, nextRegistration, registered, started;
    unsigned beSubmitting; /* owner->mut */
    uint64_t beAttempted, beAdmitted, beRejected, beTerminal, beDiscarded, beShutdownDiscarded;
    uint64_t diskRestored, diskTransferred, diskTerminal, diskDiscarded, diskPersisted;
    uint64_t registrationFailures, allocationBytes;
    uint64_t sampledOut, severityDiscarded, policyRejected, policyBytes, retryFailed;
    unsigned samplingPosition; /* BE mutex, configured admission policy only */
    uint64_t legacyPolicyMessages, legacyPolicyBytes, legacySeverityDiscarded; /* stats-list reader */
    uint64_t beBatches, beNofit, beOversized, beRegistration, beRedirect, beUnclassified;
    uint64_t beInternal, beCapacity, capacityExhaustions;
    uint64_t beDequeueBatches, beDequeueMessages, beDequeueMax;
    uint64_t legacyPublished, legacyBytes; /* stats-list-serialized reader only */
    #ifdef ENABLE_TESTBENCH
    char *testShutdownMarker; /* release publishes all immutable marker paths */
    char *testActionMarker;
    char *testForceTermMarker;
    unsigned testForceTermNoted;
    unsigned testHelpGateStage, testHelpGateUsed;
    int testHelpGateFd;
    char *testHelpGateMarker;
    #endif
};

typedef struct localCache_s {
    struct localCache_s *next;
    qqueue_t *owner;
    qqueueLocalFrontend_t *frontend;
    enum qqueueLocalRouteReason failureReason;
} localCache_t;

typedef struct localProducer_s {
    localCache_t *cache;
} localProducer_t;

static pthread_once_t producerOnce = PTHREAD_ONCE_INIT;
static pthread_key_t producerKey;
static unsigned producerKeyValid;
/* Trust is execution provenance, independent of cache allocation success.
 * This compiler-TLS scalar is available on the same GCC/Clang local backend
 * that supplies the required always-lock-free atomics. */
static __thread unsigned producerTrustedDepth;
/* Startup sets this before inputs run. No reload is supported and the flag
 * never resets while threads can inspect it; global-only runs avoid TLS work. */
static unsigned localEnabled;

/* Counters have one writer per domain (BE writers are mutex serialized).
 * Relaxed accesses provide race-free, possibly non-atomic whole snapshots. */
static uint64_t counterRead(const uint64_t *const value) {
    return __atomic_load_n(value, __ATOMIC_RELAXED);
}
static void counterAdd(uint64_t *const value, const uint64_t n) {
    __atomic_store_n(value, counterRead(value) + n, __ATOMIC_RELAXED);
}
static unsigned stateRead(const unsigned *const value) {
    return __atomic_load_n(value, __ATOMIC_SEQ_CST);
}
static void stateSet(unsigned *const value, const unsigned state) {
    __atomic_store_n(value, state, __ATOMIC_SEQ_CST);
}

    #ifdef ENABLE_TESTBENCH
enum localTestFault {
    TEST_FAULT_NONE,
    TEST_FAULT_STARTUP_FAMILY,
    TEST_FAULT_STARTUP_FRONTEND,
    TEST_FAULT_TLS,
    TEST_FAULT_CACHE,
    TEST_FAULT_WORKER,
    TEST_FAULT_RETIRE,
    TEST_FAULT_BACKEND_NODE,
    TEST_FAULT_RETRY_NODE
};
static enum localTestFault testFault;
static const char *testFaultName;
static pthread_once_t testFaultOnce = PTHREAD_ONCE_INIT;
static unsigned testFaultFired;
static __thread unsigned testProducerRetire;
static pthread_mutex_t testRedirectMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t testRedirectCond;
static pthread_once_t testRedirectOnce = PTHREAD_ONCE_INIT;
static int testRedirectInitResult;
static int initMonotonicCond(pthread_cond_t *condition);
static struct timespec deadlineAfter(int milliseconds);
static void testInitRedirectCond(void) {
    testRedirectInitResult = initMonotonicCond(&testRedirectCond);
}
enum { TEST_REDIRECT_OFF, TEST_REDIRECT_ARMED, TEST_REDIRECT_BLOCKED, TEST_REDIRECT_RELEASED };
static unsigned testRedirectState;

static void readTestFault(void) {
    const char *const value = getenv("RSYSLOG_LOCAL_QUEUE_TEST_FAULT");
    static const char *const names[] = {"",
                                        "startup-family",
                                        "startup-frontend",
                                        "producer-tls",
                                        "producer-cache",
                                        "frontend-worker",
                                        "producer-retire",
                                        "backend-node",
                                        "retry-node"};
    if (value == NULL) return;
    for (unsigned i = 1; i < sizeof(names) / sizeof(names[0]); ++i) {
        if (!strcmp(value, names[i])) {
            testFault = (enum localTestFault)i;
            testFaultName = names[i];
            break;
        }
    }
}

/* Marker comes from the actual injected boundary, never mere configuration.
 * This direct test diagnostic cannot recursively submit to a local queue. */
static int hitTestFault(const enum localTestFault fault) {
    if (testFault != fault) return 0;
    const unsigned alreadyFired = __atomic_exchange_n(&testFaultFired, 1, __ATOMIC_RELAXED);
    if (!alreadyFired) fprintf(stderr, "local queue test fault: %s\n", testFaultName);
    return (fault != TEST_FAULT_RETIRE && fault != TEST_FAULT_BACKEND_NODE && fault != TEST_FAULT_RETRY_NODE) ||
           !alreadyFired;
}

int qqueueLocalTestFailNode(const qqueue_t *owner, const int retry) {
    return owner->bLocalScope && owner->qType == QUEUETYPE_LINKEDLIST &&
           hitTestFault(retry ? TEST_FAULT_RETRY_NODE : TEST_FAULT_BACKEND_NODE);
}

int qqueueLocalTestProducerShouldExit(void) {
    return testProducerRetire != 0;
}

void qqueueLocalTestRedirectArm(void) {
    pthread_once(&testRedirectOnce, testInitRedirectCond);
    if (testRedirectInitResult != 0) return;
    pthread_mutex_lock(&testRedirectMutex);
    assert(stateRead(&testRedirectState) == TEST_REDIRECT_OFF);
    stateSet(&testRedirectState, TEST_REDIRECT_ARMED);
    pthread_mutex_unlock(&testRedirectMutex);
}

int qqueueLocalTestRedirectWaitPublisher(const unsigned timeout_ms) {
    pthread_once(&testRedirectOnce, testInitRedirectCond);
    if (testRedirectInitResult != 0 || timeout_ms > INT_MAX) return 0;
    const struct timespec deadline = deadlineAfter((int)timeout_ms);
    int waitResult = 0;
    pthread_mutex_lock(&testRedirectMutex);
    while (stateRead(&testRedirectState) != TEST_REDIRECT_BLOCKED && waitResult == 0)
        waitResult = pthread_cond_timedwait(&testRedirectCond, &testRedirectMutex, &deadline);
    const int blocked = stateRead(&testRedirectState) == TEST_REDIRECT_BLOCKED;
    pthread_mutex_unlock(&testRedirectMutex);
    return blocked;
}

static void testGatePublisher(void) {
    if (stateRead(&testRedirectState) != TEST_REDIRECT_ARMED) return;
    pthread_mutex_lock(&testRedirectMutex);
    if (stateRead(&testRedirectState) == TEST_REDIRECT_ARMED) {
        stateSet(&testRedirectState, TEST_REDIRECT_BLOCKED);
        pthread_cond_broadcast(&testRedirectCond);
        while (stateRead(&testRedirectState) == TEST_REDIRECT_BLOCKED)
            pthread_cond_wait(&testRedirectCond, &testRedirectMutex);
        stateSet(&testRedirectState, TEST_REDIRECT_OFF);
    }
    pthread_mutex_unlock(&testRedirectMutex);
}

void qqueueLocalTestRedirectRelease(void) {
    pthread_mutex_lock(&testRedirectMutex);
    if (stateRead(&testRedirectState) == TEST_REDIRECT_BLOCKED) {
        stateSet(&testRedirectState, TEST_REDIRECT_RELEASED);
        pthread_cond_broadcast(&testRedirectCond);
    } else if (stateRead(&testRedirectState) == TEST_REDIRECT_ARMED) {
        stateSet(&testRedirectState, TEST_REDIRECT_OFF);
        pthread_cond_broadcast(&testRedirectCond);
    }
    pthread_mutex_unlock(&testRedirectMutex);
}

static char *testMarkerPath(const char *const base, const char *const suffix) {
    const size_t length = strlen(base), extra = strlen(suffix);
    if (length > SIZE_MAX - extra - 1) return NULL;
    char *const path = malloc(length + extra + 1);
    if (path != NULL) {
        memcpy(path, base, length);
        memcpy(path + length, suffix, extra + 1);
    }
    return path;
}

rsRetVal qqueueLocalTestArmShutdownCheck(qqueue_t *const owner, const char *const markerPath) {
    if (owner == NULL || owner->local == NULL || markerPath == NULL || markerPath[0] != '/') return RS_RET_PARAM_ERROR;
    char *const marker = testMarkerPath(markerPath, "");
    char *const actionMarker = testMarkerPath(markerPath, ".action-phase");
    char *const forceTermMarker = testMarkerPath(markerPath, ".force-term");
    if (marker == NULL || actionMarker == NULL || forceTermMarker == NULL) {
        free(marker);
        free(actionMarker);
        free(forceTermMarker);
        return RS_RET_OUT_OF_MEMORY;
    }
    qqueueLocal_t *const family = owner->local;
    pthread_mutex_lock(&family->registry);
    const int allowed = stateRead(&family->state) == LOCAL_RUNNING &&
                        __atomic_load_n(&family->testShutdownMarker, __ATOMIC_ACQUIRE) == NULL;
    if (allowed) {
        family->testActionMarker = actionMarker;
        family->testForceTermMarker = forceTermMarker;
        __atomic_store_n(&family->testShutdownMarker, marker, __ATOMIC_RELEASE);
    }
    pthread_mutex_unlock(&family->registry);
    if (!allowed) {
        free(marker);
        free(actionMarker);
        free(forceTermMarker);
    }
    return allowed ? RS_RET_OK : RS_RET_PARAM_ERROR;
}

static void testWritePhaseMarker(const char *const path, const char *const text) {
    FILE *const output = fopen(path, "w");
    if (output != NULL) {
        fputs(text, output);
        fclose(output);
    }
}

static void testNoteActionPhase(qqueueLocal_t *const family) {
    if (__atomic_load_n(&family->testShutdownMarker, __ATOMIC_ACQUIRE) != NULL)
        testWritePhaseMarker(family->testActionMarker, "immediate=1\n");
}

void qqueueLocalTestNoteForceTerm(wti_t *const worker, const int currentIParams) {
    if (currentIParams != 0 || !qqueueLocalWorker(worker) || worker->source_queue == NULL) return;
    qqueue_t *const owner = worker->logical_owner;
    if (owner == NULL || owner->local == NULL) return;
    qqueueLocal_t *const family = owner->local;
    if (__atomic_load_n(&family->testShutdownMarker, __ATOMIC_ACQUIRE) == NULL ||
        __atomic_exchange_n(&family->testForceTermNoted, 1, __ATOMIC_RELAXED))
        return;
    /* The caller has already reset the real transaction's parameter count.
     * Source ownership remains live; this observation changes no batch state. */
    testWritePhaseMarker(family->testForceTermMarker, "force_term=1 currIParam=0\n");
}

static int testWorkerSettled(wti_t *const worker) {
    return wtiGetState(worker) == WRKTHRD_STOPPED && worker->source_queue == NULL && worker->logical_owner == NULL &&
           !qqueueLeaseHasResponsibility(worker->batch.nElem, worker->batch.nElemDeq, worker->batch.storeData) &&
           worker->n_deferred_msgs == 0;
}

static rsRetVal testCheckShutdown(qqueue_t *const owner) {
    qqueueLocal_t *const family = owner->local;
    const char *const marker = __atomic_load_n(&family->testShutdownMarker, __ATOMIC_ACQUIRE);
    if (marker == NULL) return RS_RET_OK;
    qqueueLocalSnapshot_t snapshot;
    qqueueLocalGetSnapshot(owner, &snapshot);
    unsigned joined = 0;
    for (unsigned i = 0; i < family->count; ++i) {
        qqueueLocalFrontend_t *const fe = &family->fronts[i];
        const unsigned state = stateRead(&fe->state);
        if (state != FE_UNUSED && state != FE_FAILED && state != FE_JOINED) return RS_RET_INTERNAL_ERROR;
        if (!testWorkerSettled(fe->pool->pWrkr[0])) return RS_RET_INTERNAL_ERROR;
        if (state == FE_JOINED) {
            ++joined;
            if (rsSpscQueueConsumerAvailable(&fe->ring) != 0) return RS_RET_INTERNAL_ERROR;
        }
    }
    for (int i = 0; i < owner->pWtpReg->iNumWorkerThreads; ++i)
        if (!testWorkerSettled(owner->pWtpReg->pWrkr[i])) return RS_RET_INTERNAL_ERROR;
    if (family->idleHelpers != NULL || snapshot.help_active != 0 || snapshot.help_retry != 0 ||
        snapshot.help_messages != snapshot.help_completed ||
        snapshot.help_completed != snapshot.help_terminal + snapshot.help_returned || snapshot.outstanding != 0 ||
        snapshot.fe_queued != 0 || snapshot.fe_active != 0 || snapshot.fe_retry != 0 || snapshot.be_physical != 0 ||
        snapshot.be_active != 0 || snapshot.admitted + snapshot.restored != snapshot.terminal ||
        snapshot.attempts + snapshot.restored !=
            snapshot.terminal + snapshot.preadmission_rejected + snapshot.sampled_out + snapshot.severity_discarded ||
        snapshot.disk_active != 0 || snapshot.disk_physical != snapshot.persisted)
        return RS_RET_INTERNAL_ERROR;
    FILE *const output = fopen(marker, "w");
    if (output == NULL) return RS_RET_IO_ERROR;
    const int written =
        fprintf(output,
                "OK fe.joined=%u fe.registered=%llu shutdown.discarded=%llu outstanding=0 "
                "admitted=%llu terminal=%llu rejected=%llu transferred=%llu help.completed=%llu help.returned=%llu "
                "restored=%llu persisted=%llu executed=%llu discarded=%llu sampled_out=%llu severity_discarded=%llu "
                "retry_failed=%llu\n",
                joined, (unsigned long long)snapshot.fe_registered, (unsigned long long)snapshot.shutdown_discarded,
                (unsigned long long)snapshot.admitted, (unsigned long long)snapshot.terminal,
                (unsigned long long)snapshot.preadmission_rejected, (unsigned long long)snapshot.transferred,
                (unsigned long long)snapshot.help_completed, (unsigned long long)snapshot.help_returned,
                (unsigned long long)snapshot.restored, (unsigned long long)snapshot.persisted,
                (unsigned long long)snapshot.executed, (unsigned long long)snapshot.discarded,
                (unsigned long long)snapshot.sampled_out, (unsigned long long)snapshot.severity_discarded,
                (unsigned long long)snapshot.retry_failed);
    const int closed = fclose(output);
    return written < 0 || closed != 0 ? RS_RET_IO_ERROR : RS_RET_OK;
}
    #endif

    #ifdef ENABLE_TESTBENCH
/* The cold-selected one-shot fixture holds the worker immediately before BE
 * registration, or after registration with FE retained but BE released. The
 * latter exposes the exact signal/cond-wait handoff without changing normal
 * build lock traffic. Arm only after the initial local callback retires:
 * otherwise a newly started empty worker can hold the gate before its first
 * producer publishes, while the shell waits for that first message's output.
 * Shell FIFO release is independent of queue admission. */
static void testHelpGate(qqueueLocalFrontend_t *const fe, const unsigned stage) {
    qqueueLocal_t *const family = fe->owner->local;
    if ((stage == 3 && fe->index != 1) || family->testHelpGateStage != stage || counterRead(&fe->terminal) == 0 ||
        __atomic_exchange_n(&family->testHelpGateUsed, 1, __ATOMIC_RELAXED))
        return;
    FILE *const marker = fopen(family->testHelpGateMarker, "w");
    if (marker != NULL) {
        fputs("READY\n", marker);
        fclose(marker);
    }
    char token;
    while (read(family->testHelpGateFd, &token, 1) < 0 && errno == EINTR) {
    }
}
    #endif

static void freeProducer(void *const arg) {
    localProducer_t *const producer = arg;
    if (producer == NULL) return;
    while (producer->cache != NULL) {
        localCache_t *const old = producer->cache;
        producer->cache = old->next;
        free(old); /* Never dereference a queue from the TLS fallback destructor. */
    }
    free(producer);
}

static void makeProducerKey(void) {
    stateSet(&producerKeyValid, pthread_key_create(&producerKey, freeProducer) == 0);
}

int qqueueLocalEnabled(void) {
    return stateRead(&localEnabled) != 0;
}

void qqueueLocalProducerEnter(void) {
    localProducer_t *producer;
    if (!qqueueLocalEnabled()) return;
    ++producerTrustedDepth;
    #ifdef ENABLE_TESTBENCH
    if (hitTestFault(TEST_FAULT_TLS)) return;
    #endif
    pthread_once(&producerOnce, makeProducerKey);
    if (!stateRead(&producerKeyValid)) return;
    producer = pthread_getspecific(producerKey);
    if (producer == NULL) {
        producer = calloc(1, sizeof(*producer));
        if (producer == NULL) return;
        if (pthread_setspecific(producerKey, producer) != 0) {
            free(producer);
            return;
        }
    }
}

void qqueueLocalProducerLeave(void) {
    if (producerTrustedDepth != 0) --producerTrustedDepth;
}

void qqueueLocalProducerExit(void *const unused) {
    (void)unused;
    producerTrustedDepth = 0;
    #ifdef ENABLE_TESTBENCH
    testProducerRetire = 0;
    #endif
    localProducer_t *const producer = stateRead(&producerKeyValid) ? pthread_getspecific(producerKey) : NULL;
    if (producer == NULL) return;
    /* Called by input or graph-worker cleanup, before downstream queue teardown.
     * The explicit departure marks are not delegated to arbitrary TLS teardown. */
    for (localCache_t *cache = producer->cache; cache != NULL; cache = cache->next) {
        if (cache->frontend != NULL) stateSet(&cache->frontend->producerExited, 1);
    }
    pthread_setspecific(producerKey, NULL);
    freeProducer(producer);
}

static int initMonotonicCond(pthread_cond_t *const condition) {
    pthread_condattr_t attr;
    int ret = pthread_condattr_init(&attr);
    if (ret != 0) return ret;
    ret = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    if (ret == 0) ret = pthread_cond_init(condition, &attr);
    pthread_condattr_destroy(&attr);
    return ret;
}

static struct timespec deadlineAfter(const int milliseconds) {
    struct timespec deadline;
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    const int bounded = milliseconds < 0 ? QUEUE_TIMEOUT_ETERNAL : milliseconds;
    deadline.tv_sec += bounded / 1000;
    deadline.tv_nsec += (long)(bounded % 1000) * 1000000;
    if (deadline.tv_nsec >= 1000000000) {
        ++deadline.tv_sec;
        deadline.tv_nsec -= 1000000000;
    }
    return deadline;
}

static rsRetVal feBatchSize(void *const source, int *const size) {
    *size = (int)((qqueue_t *)source)->localSource->batchSize;
    return RS_RET_OK;
}

static rsRetVal feCheckStop(void *const source, const int lock) {
    (void)lock;
    qqueueLocalFrontend_t *const fe = ((qqueue_t *)source)->localSource;
    return stateRead(&fe->owner->local->state) == LOCAL_RUNNING ? RS_RET_OK : RS_RET_TERMINATE_NOW;
}

/* All references in this deferred array are already terminal and independent
 * of module state. Never hold the FE mutex across a final message destructor. */
static void feDrainDeferred(wti_t *const worker) {
    for (int i = 0; i < worker->n_deferred_msgs; ++i) msgDestruct(&worker->p_deferred_msgs[i]);
    worker->n_deferred_msgs = 0;
}

/* Lock order is BE then FE. The FE worker enters/exits its adapter holding
 * FE, dropping it before any BE acquisition. List membership covers the gap
 * before the existing WTI condition wait: registration retains FE until that
 * wait releases it, so a BE notification cannot be lost in that gap. */
static void unlinkHelper(qqueueLocalFrontend_t *const fe) {
    if (!fe->idleListed) return;
    if (fe->idlePrev != NULL)
        fe->idlePrev->idleNext = fe->idleNext;
    else
        fe->owner->local->idleHelpers = fe->idleNext;
    if (fe->idleNext != NULL) fe->idleNext->idlePrev = fe->idlePrev;
    fe->idleNext = fe->idlePrev = NULL;
    fe->idleListed = 0;
}

void qqueueLocalWakeBackendHelpers(qqueue_t *const owner) {
    qqueueLocal_t *const family = owner->local;
    if (family == NULL || stateRead(&family->state) != LOCAL_RUNNING || owner->localDAActive ||
        owner->iQueueSize <= owner->nLogDeq || family->idleHelpers == NULL)
        return;
    qqueueLocalFrontend_t *const fe = family->idleHelpers;
    pthread_mutex_lock(&fe->mutex);
    unlinkHelper(fe);
    fe->helpWakePending = 1;
    counterAdd(&fe->help_wakes, 1);
    pthread_cond_signal(&fe->pool->pWrkr[0]->pcondBusy);
    pthread_mutex_unlock(&fe->mutex);
}

int qqueueLocalBorrowWorker(const qqueue_t *const owner, const wti_t *const worker) {
    const qqueue_t *const home = worker->pWtp == NULL ? NULL : worker->pWtp->pUsr;
    return owner->local != NULL && home != NULL && home->localSource != NULL && home->localSource->owner == owner &&
           home->localSource->source == home && home->localSource->pool == worker->pWtp &&
           worker->pWtp->pmutUsr == home->mut;
}

static int helperReusable(const wti_t *const worker) {
    if (worker->source_queue != NULL || worker->batch.nElem != 0 || worker->batch.nElemDeq != 0 ||
        worker->batch.storeData != NULL || worker->n_deferred_msgs != 0)
        return 0;
    for (int i = 0; i < runConf->actions.iActionNbr; ++i) {
        const actWrkrInfo_t *const info = &worker->actWrkrInfo[i];
        /* This is a reuse check, never evidence that an interrupted commit
         * delivered its message. The source lease controls that decision. */
        if (info->pAction != NULL && info->pAction->isTransactional && info->p.tx.currIParam != 0) return 0;
    }
    return 1;
}

/* Called before local acquisition when a previous wait was registered, and
 * after a fresh empty-ring observation otherwise. RETRY means FE work won. */
static rsRetVal tryHelp(qqueueLocalFrontend_t *const fe, wti_t *const worker) {
    qqueue_t *const owner = fe->owner;
    pthread_mutex_unlock(&fe->mutex);
    #ifdef ENABLE_TESTBENCH
    testHelpGate(fe, 1);
    #endif
    pthread_mutex_lock(owner->mut);
    pthread_mutex_lock(&fe->mutex);
    unlinkHelper(fe);
    fe->helpWakePending = 0;
    rsRetVal ret = RS_RET_IDLE;
    if (stateRead(&owner->local->state) == LOCAL_RUNNING && helperReusable(worker)) {
        if (rsSpscQueueConsumerAvailable(&fe->ring) != 0) {
            ret = RS_RET_RETRY;
        } else if (fe->helperLimit != 0) {
            counterAdd(&fe->help_attempts, 1);
            ret = qqueueLocalTryBorrowBackend(owner, worker, fe->helperLimit);
            if (ret == RS_RET_OK) {
                const uint64_t n = (uint64_t)worker->batch.nElem;
                counterAdd(&fe->help_batches, 1);
                counterAdd(&fe->help_messages, n);
                if (n > counterRead(&fe->help_max)) __atomic_store_n(&fe->help_max, n, __ATOMIC_RELAXED);
                __atomic_store_n(&fe->help_active, n, __ATOMIC_RELAXED);
            } else if (ret == RS_RET_IDLE) {
                counterAdd(&fe->help_empty, 1);
                counterAdd(&fe->help_waits, 1);
                fe->idleNext = owner->local->idleHelpers;
                if (fe->idleNext != NULL) fe->idleNext->idlePrev = fe;
                owner->local->idleHelpers = fe;
                fe->idleListed = 1;
            }
        }
    }
    if (ret == RS_RET_IDLE) {
        /* Keep FE locked through WTI's predicate check and condition wait. */
        pthread_mutex_unlock(owner->mut);
    #ifdef ENABLE_TESTBENCH
        if (fe->idleListed) testHelpGate(fe, 2);
    #endif
    } else {
        pthread_mutex_unlock(&fe->mutex);
        /* Cascade at most one wake for remaining backlog, including when a
         * selected helper instead chose its newly published FE batch. */
        qqueueLocalWakeBackendHelpers(owner);
        pthread_mutex_unlock(owner->mut);
        pthread_mutex_lock(&fe->mutex);
    }
    return ret;
}

static rsRetVal completeBorrow(qqueueLocalFrontend_t *const fe, wti_t *const worker, const int joined) {
    unsigned retry = 0;
    for (int i = 0; i < worker->batch.nElem; ++i)
        if (worker->batch.eltState[i] == BATCH_STATE_RDY || worker->batch.eltState[i] == BATCH_STATE_SUB) ++retry;
    if (retry != 0 && !joined) {
        /* Never switch sources with unresolved action state. Joined cleanup
         * has disposed module parameters before reinserting these BE refs. */
        __atomic_store_n(&fe->help_retry, retry, __ATOMIC_RELAXED);
        return RS_RET_RETRY;
    }
    const uint64_t terminal = (uint64_t)worker->batch.nElem - retry;
    pthread_mutex_unlock(&fe->mutex);
    pthread_mutex_lock(fe->owner->mut);
    const rsRetVal ret = qqueueLocalCompleteBorrowedBackend(fe->owner, worker);
    pthread_mutex_unlock(fe->owner->mut);
    if (ret == RS_RET_OK) {
        unsigned returned = 0;
        for (unsigned i = 0; i < terminal + retry; ++i)
            if (worker->batch.eltState[i] == BATCH_STATE_RDY || worker->batch.eltState[i] == BATCH_STATE_SUB)
                ++returned;
        feDrainDeferred(worker);
        counterAdd(&fe->help_terminal, terminal + retry - returned);
        counterAdd(&fe->help_completed, terminal + retry);
        counterAdd(&fe->help_returned, returned);
        __atomic_store_n(&fe->help_active, 0, __ATOMIC_RELAXED);
        __atomic_store_n(&fe->help_retry, 0, __ATOMIC_RELAXED);
    }
    pthread_mutex_lock(&fe->mutex);
    return ret;
}

static rsRetVal feComplete(void *const source, wti_t *const worker) {
    qqueue_t *const queue = source;
    qqueueLocalFrontend_t *const fe = queue->localSource;
    batch_t *const batch = &worker->batch;
    if (worker->source_queue == NULL) return RS_RET_OK;
    if (worker->source_queue == fe->owner) return completeBorrow(fe, worker, 0);
    if (worker->source_queue != queue || worker->logical_owner != fe->owner || worker->pWtp->pUsr != queue ||
        worker->pWtp->pmutUsr != queue->mut)
        return RS_RET_INTERNAL_ERROR;

    /* Keep positions unchanged until all callback state is reusable or the
     * worker has joined. ruleset.processBatch does not skip COMM elements. */
    unsigned retained = 0;
    unsigned terminal = 0;
    for (int i = 0; i < batch->nElem; ++i) {
        if (batch->pElem[i].pMsg == NULL) continue;
        if (batch->eltState[i] == BATCH_STATE_RDY || batch->eltState[i] == BATCH_STATE_SUB) {
            ++retained;
        } else {
            assert(worker->n_deferred_msgs < batch->maxElem);
            worker->p_deferred_msgs[worker->n_deferred_msgs++] = batch->pElem[i].pMsg;
            batch->pElem[i].pMsg = NULL;
            ++terminal;
            if (batch->eltState[i] == BATCH_STATE_DISC) counterAdd(&fe->discarded, 1);
        }
    }
    counterAdd(&fe->terminal, terminal);
    __atomic_store_n(&fe->active, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&fe->retry, retained, __ATOMIC_RELAXED);
    if (retained == 0) {
        batch->nElem = batch->nElemDeq = 0;
        qqueueLeaseClear(&worker->source_queue, &worker->logical_owner, queue);
    }
    if (worker->n_deferred_msgs != 0) {
        pthread_mutex_unlock(queue->mut);
        feDrainDeferred(worker);
        pthread_mutex_lock(queue->mut);
    }
    return retained == 0 ? RS_RET_OK : RS_RET_RETRY;
}

static rsRetVal feDoWork(void *const source, void *const workerArg) {
    qqueue_t *const queue = source;
    qqueueLocalFrontend_t *const fe = queue->localSource;
    wti_t *const worker = workerArg;
    batch_t *const batch = &worker->batch;
    #ifdef ENABLE_TESTBENCH
    if (fe->helpWakePending && fe->owner->local->testHelpGateStage == 3) {
        pthread_mutex_unlock(&fe->mutex);
        testHelpGate(fe, 3);
        pthread_mutex_lock(&fe->mutex);
    }
    #endif
    rsRetVal ret = feComplete(queue, worker);
    if (ret == RS_RET_RETRY) {
        /* Qualified action retry waits live inside their callback. An
         * unexpected returned unresolved lease has no generic safe replay
         * operation. Retain it until shutdown; publication only wakes a
         * predicate recheck and must not replay already completed scripts. */
        return RS_RET_IDLE;
    }
    if (ret != RS_RET_OK) return RS_RET_ERR_QUEUE_EMERGENCY;
    if (stateRead(&fe->owner->local->state) != LOCAL_RUNNING) return RS_RET_IDLE;
    if (fe->idleListed || fe->helpWakePending ||
        (fe->helperLimit != 0 && rsSpscQueueConsumerAvailable(&fe->ring) == 0)) {
        ret = tryHelp(fe, worker);
        if (ret == RS_RET_IDLE) return ret;
        if (ret != RS_RET_OK && ret != RS_RET_RETRY) return RS_RET_ERR_QUEUE_EMERGENCY;
    }
    if (worker->source_queue == NULL) {
        const unsigned minimum = (unsigned)queue->iMinDeqBatchSize;
        size_t available = rsSpscQueueConsumerAvailable(&fe->ring);
        if (minimum > 0 && available > 0 && available < minimum) {
            struct timespec deadline;
            timeoutComp(&deadline, queue->toMinDeqBatchSize);
            while (available < minimum && stateRead(&fe->owner->local->state) == LOCAL_RUNNING) {
                if (pthread_cond_timedwait(&worker->pcondBusy, queue->mut, &deadline) == ETIMEDOUT) break;
                available = rsSpscQueueConsumerAvailable(&fe->ring);
            }
            if (stateRead(&fe->owner->local->state) != LOCAL_RUNNING) return RS_RET_IDLE;
        }
        ret = qqueueLeaseBind(&worker->source_queue, &worker->logical_owner, queue, fe->owner, worker->pWtp->pUsr,
                              worker->pWtp->pmutUsr, queue->mut);
        if (ret != RS_RET_OK) return RS_RET_ERR_QUEUE_EMERGENCY;
        const size_t count = rsSpscQueuePop(&fe->ring, fe->consumerScratch, fe->batchSize);
        if (count == 0) {
            qqueueLeaseClear(&worker->source_queue, &worker->logical_owner, queue);
            /* A publisher can race the prior empty check. Register only after
             * a second BE+FE-serialized check, never idle on FE alone. */
            if (fe->helperLimit != 0) {
                ret = tryHelp(fe, worker);
                if (ret == RS_RET_RETRY) return RS_RET_OK;
                if (ret != RS_RET_OK) return ret;
            } else
                return RS_RET_IDLE;
        } else {
            for (size_t i = 0; i < count; ++i) {
                batch->pElem[i].pMsg = fe->consumerScratch[i];
                batch->eltState[i] = BATCH_STATE_RDY;
                fe->consumerScratch[i] = NULL;
            }
            batch->nElem = batch->nElemDeq = (int)count;
            counterAdd(&fe->dequeued, count);
            counterAdd(&fe->batches, 1);
            counterAdd(&fe->dequeueMessages, count);
            if (count > counterRead(&fe->dequeueMax)) __atomic_store_n(&fe->dequeueMax, count, __ATOMIC_RELAXED);
            __atomic_store_n(&fe->active, count, __ATOMIC_RELAXED);
        }
    }
    /* Lifecycle belongs to the logical family; callback interruption belongs
     * to the executing FE even while it borrows BE. This lets callbacks settle while BE remains
     * available for their residual obligations under the same phase deadline. */
    worker->pbShutdownImmediate = &queue->bShutdownImmediate;
    #ifndef HAVE_ATOMIC_BUILTINS
    worker->pmutShutdownImmediate = &queue->mutShutdownImmediate;
    #endif
    pthread_mutex_unlock(queue->mut);
    int oldCancel;
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldCancel);
    ret = fe->owner->pConsumer(fe->owner->pAction, batch, worker);
    if (fe->owner->iDeqSlowdown != 0) srSleep(fe->owner->iDeqSlowdown / 1000000, fe->owner->iDeqSlowdown % 1000000);
    pthread_setcancelstate(oldCancel, NULL);
    pthread_mutex_lock(queue->mut);
    if (wtiIsShutdownImmediate(worker)) qqueueLocalRetainAmbiguous(worker);
    /* Completion is source-bound on the next iteration or exit. Callback
     * errors cannot bypass disabled cancellation before reacquiring mutex. */
    return ret;
}

static rsRetVal constructFrontend(qqueueLocalFrontend_t *const fe,
                                  qqueue_t *const owner,
                                  const uint32_t index,
                                  const uint32_t capacity,
                                  const uint32_t batchSize) {
    DEFiRet;
    fe->owner = owner;
    fe->index = index;
    fe->capacity = capacity;
    fe->batchSize = batchSize;
    fe->helperLimit = owner->localHelperBatchSizeSet ? (unsigned)owner->localHelperBatchSize : batchSize;
    if (pthread_mutex_init(&fe->mutex, NULL) != 0) return RS_RET_ERR;
    fe->mutexInitialized = 1;
    if (initMonotonicCond(&fe->publisherDone) != 0) return RS_RET_ERR;
    fe->condInitialized = 1;
    CHKmalloc(fe->slots = calloc(capacity, sizeof(void *)));
    #ifdef ENABLE_TESTBENCH
    /* With N>=2 this fails inside the second partially constructed FE. */
    if (index == 1 && hitTestFault(TEST_FAULT_STARTUP_FRONTEND)) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    #endif
    CHKmalloc(fe->producerScratch = calloc(capacity, sizeof(void *)));
    CHKmalloc(fe->consumerScratch = calloc(batchSize, sizeof(void *)));
    if (!rsSpscQueueInit(&fe->ring, fe->slots, capacity)) ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
    CHKiRet(qqueueConstruct(&fe->source, QUEUETYPE_FIXED_ARRAY, 1, 0, owner->pConsumer));
    fe->source->localSource = fe;
    fe->source->localGraphConf = owner->localGraphConf;
    fe->source->mut = &fe->mutex;
    fe->source->iDeqBatchSize = (int)batchSize;
    fe->source->iMinDeqBatchSize = owner->iMinDeqBatchSize < (int)batchSize ? owner->iMinDeqBatchSize : (int)batchSize;
    fe->source->toMinDeqBatchSize = owner->toMinDeqBatchSize;
    fe->source->iDeqtWinFromHr = owner->iDeqtWinFromHr;
    fe->source->iDeqtWinToHr = owner->iDeqtWinToHr;
    CHKiRet(wtpConstruct(&fe->pool));
    CHKiRet(wtpUseMonotonicTermination(fe->pool));
    CHKiRet(wtpSetpUsr(fe->pool, fe->source));
    CHKiRet(wtpSetpmutUsr(fe->pool, &fe->mutex));
    CHKiRet(wtpSetiNumWorkerThreads(fe->pool, 1));
    CHKiRet(wtpSettoWrkShutdown(fe->pool, -1));
    CHKiRet(wtpSetbAllowFirstWorkerToTimeout(fe->pool, 0));
    CHKiRet(wtpSetpfChkStopWrkr(fe->pool, feCheckStop));
    CHKiRet(wtpSetpfGetDeqBatchSize(fe->pool, feBatchSize));
    CHKiRet(wtpSetpfDoWork(fe->pool, feDoWork));
    if (owner->iDeqtWinToHr != 25) CHKiRet(wtpSetpfRateLimiter(fe->pool, qqueueLocalRateLimiter));
    CHKiRet(wtpSetpfObjProcessed(fe->pool, feComplete));
    char name[64];
    const int length = snprintf(name, sizeof(name), "local-fe-%u", index + 1);
    CHKiRet(wtpSetDbgHdr(fe->pool, (uchar *)name, (size_t)length));
    CHKiRet(wtpConstructFinalize(fe->pool));
    fe->source->pWtpReg = fe->pool;
finalize_it:
    RETiRet;
}

rsRetVal qqueueLocalStart(qqueue_t *const owner) {
    DEFiRet;
    if (owner->local != NULL) return RS_RET_OK;
    if ((owner->qType != QUEUETYPE_FIXED_ARRAY && owner->qType != QUEUETYPE_LINKEDLIST) ||
        owner->localFrontendSize <= 0 || owner->localMaxFrontends <= 0 || owner->iDeqBatchSize <= 0)
        return RS_RET_PARAM_ERROR;
    #ifdef ENABLE_TESTBENCH
    pthread_once(&testFaultOnce, readTestFault);
    if (hitTestFault(TEST_FAULT_STARTUP_FAMILY)) return RS_RET_OUT_OF_MEMORY;
    #endif
    qqueueLocal_t *family = calloc(1, sizeof(*family));
    if (family == NULL) return RS_RET_OUT_OF_MEMORY;
    owner->local = family; /* unpublished configuration startup */
    #ifdef ENABLE_TESTBENCH
    family->testHelpGateFd = -1;
    const char *const helpGate = getenv("RSYSLOG_LOCAL_QUEUE_TEST_HELP_GATE");
    if (helpGate != NULL) {
        const char *const marker = getenv("RSYSLOG_LOCAL_QUEUE_TEST_HELP_ENTRY");
        const char *const release = getenv("RSYSLOG_LOCAL_QUEUE_TEST_HELP_RELEASE");
        struct stat st;
        family->testHelpGateStage = !strcmp(helpGate, "before")     ? 1
                                    : !strcmp(helpGate, "after")    ? 2
                                    : !strcmp(helpGate, "selected") ? 3
                                                                    : 0;
        if (!family->testHelpGateStage || marker == NULL || marker[0] != '/' || release == NULL || release[0] != '/')
            ABORT_FINALIZE(RS_RET_PARAM_ERROR);
        CHKmalloc(family->testHelpGateMarker = strdup(marker));
        family->testHelpGateFd = open(release, O_RDWR | O_CLOEXEC);
        if (family->testHelpGateFd < 0 || fstat(family->testHelpGateFd, &st) != 0 || !S_ISFIFO(st.st_mode))
            ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }
    #endif
    family->owner = owner;
    family->count = (unsigned)owner->localMaxFrontends;
    const uint32_t capacity = (uint32_t)owner->localFrontendSize;
    const uint32_t batchSize = capacity < (uint32_t)owner->iDeqBatchSize ? capacity : (uint32_t)owner->iDeqBatchSize;
    if (capacity > UINT32_MAX / 2 || sizeof(void *) > SIZE_MAX / capacity ||
        sizeof(*family->fronts) > SIZE_MAX / family->count)
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    if (pthread_mutex_init(&family->registry, NULL) != 0) ABORT_FINALIZE(RS_RET_ERR);
    family->registryInitialized = 1;
    if (initMonotonicCond(&family->space) != 0) ABORT_FINALIZE(RS_RET_ERR);
    family->spaceInitialized = 1;
    if (initMonotonicCond(&family->admissionDone) != 0) ABORT_FINALIZE(RS_RET_ERR);
    family->admissionInitialized = 1;
    CHKmalloc(family->fronts = calloc(family->count, sizeof(*family->fronts)));
    family->allocationBytes = sizeof(*family) + (uint64_t)family->count * sizeof(*family->fronts);
    for (unsigned i = 0; i < family->count; ++i) {
        CHKiRet(constructFrontend(&family->fronts[i], owner, i, capacity, batchSize));
        const qqueueLocalFrontend_t *const fe = &family->fronts[i];
        family->allocationBytes +=
            ((uint64_t)capacity * 2 + batchSize) * sizeof(void *) + sizeof(qqueue_t) + sizeof(wtp_t) + sizeof(wti_t) +
            sizeof(wti_t *) + (uint64_t)batchSize * (sizeof(batch_obj_t) + sizeof(batch_state_t) + sizeof(smsg_t *)) +
            (uint64_t)runConf->actions.iActionNbr * sizeof(actWrkrInfo_t) + strlen((const char *)fe->pool->pszDbgHdr) +
            strlen((const char *)fe->pool->pWrkr[0]->pszDbgHdr) + 2;
        if (fe->source->pszSpoolDir != NULL)
            family->allocationBytes += strlen((const char *)fe->source->pszSpoolDir) + 1;
    }
finalize_it:
    if (iRet != RS_RET_OK)
        qqueueLocalDestruct(owner);
    else
        stateSet(&localEnabled, 1);
    RETiRet;
}

static qqueueLocalFrontend_t *registerFrontend(qqueue_t *const owner,
                                               localProducer_t *const producer,
                                               enum qqueueLocalRouteReason *const reason) {
    for (localCache_t *cache = producer->cache; cache != NULL; cache = cache->next) {
        if (cache->owner == owner) {
            *reason = cache->failureReason;
            return cache->frontend;
        }
    }
    *reason = QLOCAL_REGISTRATION;
    localCache_t *const cache =
    #ifdef ENABLE_TESTBENCH
        hitTestFault(TEST_FAULT_CACHE) ? NULL :
    #endif
                                       calloc(1, sizeof(*cache));
    qqueueLocal_t *const family = owner->local;
    if (cache == NULL) {
        __atomic_fetch_add(&family->registrationFailures, 1, __ATOMIC_RELAXED);
        return NULL;
    }
    cache->owner = owner;
    cache->failureReason = QLOCAL_REGISTRATION;
    cache->next = producer->cache;
    producer->cache = cache;
    pthread_mutex_lock(&family->registry);
    if (stateRead(&family->state) != LOCAL_RUNNING) {
        cache->failureReason = QLOCAL_REDIRECT;
    } else if (family->nextRegistration == family->count) {
        cache->failureReason = QLOCAL_CAPACITY;
        __atomic_fetch_add(&family->capacityExhaustions, 1, __ATOMIC_RELAXED);
    } else {
        qqueueLocalFrontend_t *const fe = &family->fronts[family->nextRegistration++];
        stateSet(&fe->state, FE_REGISTERING);
        __atomic_fetch_add(&family->registered, 1, __ATOMIC_RELAXED);
        pthread_mutex_lock(&fe->mutex);
        const rsRetVal ret =
    #ifdef ENABLE_TESTBENCH
            hitTestFault(TEST_FAULT_WORKER) ? RS_RET_ERR :
    #endif
                                            wtpAdviseMaxWorkers(fe->pool, 1, DENY_WORKER_START_DURING_SHUTDOWN);
        if (ret == RS_RET_OK && wtiGetState(fe->pool->pWrkr[0]) == WRKTHRD_RUNNING) {
            cache->frontend = fe;
            stateSet(&fe->state, FE_RUNNING);
            __atomic_fetch_add(&family->started, 1, __ATOMIC_RELAXED);
        } else {
            stateSet(&fe->state, FE_FAILED);
            __atomic_fetch_add(&family->registrationFailures, 1, __ATOMIC_RELAXED);
        }
        pthread_mutex_unlock(&fe->mutex);
    }
    *reason = cache->failureReason;
    pthread_mutex_unlock(&family->registry);
    return cache->frontend;
}

static rsRetVal submitSurvivors(qqueue_t *const owner,
                                smsg_t *const *const messages,
                                const size_t count,
                                const int singleFlowControl) {
    if (count == 0) return RS_RET_OK;
    int oldCancel;
    rsRetVal ret;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldCancel);
    localProducer_t *const producer = stateRead(&producerKeyValid) ? pthread_getspecific(producerKey) : NULL;
    qqueueLocalFrontend_t *fe = NULL;
    enum qqueueLocalRouteReason reason = QLOCAL_UNCLASSIFIED;
    int internal = 0;
    for (size_t i = 0; i < count && !internal; ++i) internal = (messages[i]->msgFlags & INTERNAL_MSG) != 0;
    const int trusted = !internal && producerTrustedDepth != 0 && owner->local != NULL;
    if (internal) reason = QLOCAL_INTERNAL;
    if (trusted) {
        if (producer != NULL) {
            fe = registerFrontend(owner, producer, &reason);
        } else {
            reason = QLOCAL_REGISTRATION;
            __atomic_fetch_add(&owner->local->registrationFailures, 1, __ATOMIC_RELAXED);
        }
    }
    if (fe != NULL) {
        counterAdd(&fe->attempts, count);
        counterAdd(&fe->submittedBatches, 1);
        if (count > counterRead(&fe->submittedMax)) __atomic_store_n(&fe->submittedMax, count, __ATOMIC_RELAXED);
        /* Seq-cst two-sided handshake: either shutdown observes publishing,
         * or a publisher beginning later observes REDIRECT and never writes. */
        stateSet(&fe->publishing, 1);
    #ifdef ENABLE_TESTBENCH
        testGatePublisher();
    #endif
        if (stateRead(&owner->local->state) == LOCAL_RUNNING && stateRead(&fe->state) == FE_RUNNING) {
            int published = 0;
            uint64_t bytes = 0;
            if (count <= fe->capacity) {
                for (size_t i = 0; i < count; ++i) {
                    fe->producerScratch[i] = messages[i];
                    bytes += (uint64_t)messages[i]->iLenRawMsg;
                }
                published = rsSpscQueueTryPush(&fe->ring, fe->producerScratch, count);
            }
            if (published) {
                /* Published references may already be destroyed by the
                 * consumer. Do not inspect messages after publication. */
    #ifdef ENABLE_TESTBENCH
                if (hitTestFault(TEST_FAULT_RETIRE)) testProducerRetire = 1;
    #endif
                counterAdd(&fe->published, count);
                counterAdd(&fe->publishedBatches, 1);
                counterAdd(&fe->bytes, bytes);
                pthread_mutex_lock(&fe->mutex);
                counterAdd(&fe->wake_fe, 1);
                pthread_cond_signal(&fe->pool->pWrkr[0]->pcondBusy);
                stateSet(&fe->publishing, 0);
                pthread_cond_broadcast(&fe->publisherDone);
                pthread_mutex_unlock(&fe->mutex);
                ret = RS_RET_OK;
                goto done;
            }
            reason = count > fe->capacity ? QLOCAL_OVERSIZED : QLOCAL_NOFIT;
            counterAdd(&fe->overflow, count);
            counterAdd(&fe->overflowBatches, 1);
            if (count > fe->capacity) counterAdd(&fe->oversizedBatches, 1);
            counterAdd(count > fe->capacity ? &fe->oversized : &fe->nofit, count);
        }
        pthread_mutex_lock(&fe->mutex);
        stateSet(&fe->publishing, 0);
        pthread_cond_broadcast(&fe->publisherDone);
        pthread_mutex_unlock(&fe->mutex);
    }
    if (owner->local != NULL && stateRead(&owner->local->state) != LOCAL_RUNNING) reason = QLOCAL_REDIRECT;
    ret = qqueueLocalSubmitBackend(owner, messages, count, singleFlowControl, reason);
done:
    pthread_setcancelstate(oldCancel, NULL);
    return ret;
}

/* Sampling and severity are logical admission decisions. Only configured
 * policies serialize here; the default FE path retains its lock-free ring.
 * The inventory is a sampled aggregate, as severity shedding has always been:
 * BE physical includes borrowed/dedicated active leases; FE active and retry
 * are disjoint holdings. Disk inventory has its separate disk capacity policy.
 * This is not an atomic quota. A compact vector preserves whole-survivor-batch fit.
 */
rsRetVal qqueueLocalSubmit(qqueue_t *const owner,
                           smsg_t *const *const messages,
                           const size_t count,
                           const int singleFlowControl) {
    if (count == 0) return RS_RET_OK;
    if (owner->iSmpInterval == 0 && (owner->iDiscardSeverity == 8 || owner->iDiscardMrk == 0))
        return submitSurvivors(owner, messages, count, singleFlowControl);
    int oldCancel;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldCancel);
    smsg_t **survivors = count <= SIZE_MAX / sizeof(*survivors) ? malloc(count * sizeof(*survivors)) : NULL;
    qqueueLocal_t *const family = owner->local;
    pthread_mutex_lock(owner->mut);
    qqueueLocalBackendBegin(owner);
    size_t kept = 0;
    int closed = 0;
    qqueueLocalSnapshot_t inventory;
    qqueueLocalGetSnapshot(owner, &inventory);
    uint64_t occupied = inventory.be_physical + inventory.fe_queued + inventory.fe_active + inventory.fe_retry;
    const uint64_t mark = owner->iDiscardMrk == -1
                              ? inventory.reserved_messages / 100 * 98 + inventory.reserved_messages % 100 * 98 / 100
                              : (uint64_t)owner->iDiscardMrk;
    for (size_t i = 0; i < count; ++i) {
        smsg_t *message = messages[i];
        int drop = 0;
        if (survivors == NULL || qqueueLocalIsClosed(owner) || owner->localGraphClosed) {
            closed = qqueueLocalIsClosed(owner) || owner->localGraphClosed;
            counterAdd(&family->policyRejected, 1);
            drop = 1;
        } else if (owner->iSmpInterval > 0 &&
                   (family->samplingPosition = (family->samplingPosition + 1) % (unsigned)owner->iSmpInterval) != 0) {
            counterAdd(&family->sampledOut, 1);
            drop = 1;
        } else if (owner->iDiscardSeverity < 8 && mark > 0 && occupied >= mark) {
            int severity;
            if (MsgGetSeverity(message, &severity) == RS_RET_OK && severity >= owner->iDiscardSeverity) {
                counterAdd(&family->severityDiscarded, 1);
                drop = 1;
            }
        }
        if (drop) {
            counterAdd(&family->policyBytes, (uint64_t)message->iLenRawMsg);
            /* Reference destruction can emit diagnostics; no queue mutex may
             * be held. Sequence selection remains serialized per element. */
            pthread_mutex_unlock(owner->mut);
            msgDestruct(&message);
            pthread_mutex_lock(owner->mut);
        } else {
            survivors[kept++] = message;
            ++occupied;
        }
    }
    pthread_mutex_unlock(owner->mut);
    const rsRetVal routed = submitSurvivors(owner, survivors, kept, singleFlowControl);
    const rsRetVal ret = closed ? RS_RET_FORCE_TERM : survivors == NULL ? RS_RET_OUT_OF_MEMORY : routed;
    pthread_mutex_lock(owner->mut);
    qqueueLocalBackendEnd(owner);
    pthread_mutex_unlock(owner->mut);
    free(survivors);
    pthread_setcancelstate(oldCancel, NULL);
    return ret;
}

void qqueueLocalRetryFailed(qqueue_t *owner) {
    counterAdd(&owner->local->retryFailed, 1);
}
void qqueueLocalBackendShutdownDiscarded(qqueue_t *owner) {
    counterAdd(&owner->local->beShutdownDiscarded, 1);
}

int qqueueLocalIsClosed(const qqueue_t *const owner) {
    return owner->local != NULL && stateRead(&owner->local->state) >= LOCAL_CLOSED;
}

int qqueueLocalWorker(const wti_t *const worker) {
    const qqueue_t *const source = worker->pWtp == NULL ? NULL : worker->pWtp->pUsr;
    return source != NULL && (source->local != NULL || source->localSource != NULL || source->localGraphConf != NULL);
}

void qqueueLocalRetainAmbiguous(wti_t *const worker) {
    if (!qqueueLocalWorker(worker)) return;
    qqueue_t *const home = worker->pWtp->pUsr;
    /* DA COMM acknowledges disk acceptance, not interrupted action execution. */
    if (worker->pWtp == home->pWtpDA) return;
    qqueue_t *const borrowed =
        home->localSource != NULL && worker->source_queue == home->localSource->owner ? worker->source_queue : NULL;
    if (borrowed != NULL) {
        pthread_mutex_unlock(home->mut);
        pthread_mutex_lock(borrowed->mut);
    }
    /* ruleset completion marks COMM before committing Direct transactions.
     * Neither cancellation nor cooperative immediate-stop proves delivery.
     * Retrying this whole message may duplicate earlier successful actions
     * and repeat script mutations; preserve that uncertainty, never silently
     * retire it as a successful terminal result. */
    for (int i = 0; i < worker->batch.nElem; ++i) {
        if (worker->batch.eltState[i] == BATCH_STATE_COMM) worker->batch.eltState[i] = BATCH_STATE_RDY;
    }
    if (borrowed != NULL) {
        pthread_mutex_unlock(borrowed->mut);
        pthread_mutex_lock(home->mut);
    }
}

void qqueueLocalBackendRoute(qqueue_t *const owner, const size_t n, const enum qqueueLocalRouteReason reason) {
    qqueueLocal_t *const family = owner->local;
    counterAdd(&family->beBatches, 1);
    switch (reason) {
        case QLOCAL_NOFIT:
            counterAdd(&family->beNofit, n);
            break;
        case QLOCAL_OVERSIZED:
            counterAdd(&family->beOversized, n);
            break;
        case QLOCAL_REGISTRATION:
            counterAdd(&family->beRegistration, n);
            break;
        case QLOCAL_REDIRECT:
            counterAdd(&family->beRedirect, n);
            break;
        default:
        case QLOCAL_UNCLASSIFIED:
            counterAdd(&family->beUnclassified, n);
            break;
        case QLOCAL_INTERNAL:
            counterAdd(&family->beInternal, n);
            break;
        case QLOCAL_CAPACITY:
            counterAdd(&family->beCapacity, n);
            break;
    }
}

void qqueueLocalNoteActionPhase(qqueue_t *const owner) {
    #ifdef ENABLE_TESTBENCH
    if (owner->local != NULL) testNoteActionPhase(owner->local);
    #else
    (void)owner;
    #endif
}
void qqueueLocalDiskRestored(qqueue_t *const owner, const uint64_t count) {
    if (owner->local != NULL) counterAdd(&owner->local->diskRestored, count);
}
void qqueueLocalDiskTransferred(qqueue_t *const owner, const uint64_t count) {
    if (owner->local != NULL) counterAdd(&owner->local->diskTransferred, count);
}
void qqueueLocalDiskTerminal(qqueue_t *const owner, const uint64_t count, const uint64_t discarded) {
    if (owner->local == NULL) return;
    counterAdd(&owner->local->diskTerminal, count);
    counterAdd(&owner->local->diskDiscarded, discarded);
}
void qqueueLocalDiskPersisted(qqueue_t *const owner, const uint64_t count) {
    if (owner->local != NULL) __atomic_store_n(&owner->local->diskPersisted, count, __ATOMIC_RELAXED);
}
void qqueueLocalBackendAcquired(qqueue_t *const owner, const uint64_t n) {
    if (owner->local == NULL || n == 0) return;
    counterAdd(&owner->local->beDequeueBatches, 1);
    counterAdd(&owner->local->beDequeueMessages, n);
    if (n > counterRead(&owner->local->beDequeueMax))
        __atomic_store_n(&owner->local->beDequeueMax, n, __ATOMIC_RELAXED);
}
void qqueueLocalBackendAttempt(qqueue_t *const owner, const uint64_t n) {
    counterAdd(&owner->local->beAttempted, n);
}
void qqueueLocalBackendAdmitted(qqueue_t *const owner, const uint64_t n) {
    counterAdd(&owner->local->beAdmitted, n);
}
void qqueueLocalBackendRejected(qqueue_t *const owner, const uint64_t n) {
    counterAdd(&owner->local->beRejected, n);
}
void qqueueLocalBackendTerminal(qqueue_t *const owner, const uint64_t n, const uint64_t discarded) {
    counterAdd(&owner->local->beTerminal, n);
    counterAdd(&owner->local->beDiscarded, discarded);
}
void qqueueLocalBackendBegin(qqueue_t *const owner) {
    ++owner->local->beSubmitting;
}
void qqueueLocalBackendEnd(qqueue_t *const owner) {
    assert(owner->local->beSubmitting != 0);
    if (--owner->local->beSubmitting == 0) pthread_cond_broadcast(&owner->local->admissionDone);
}
int qqueueLocalBackendWaitSpace(qqueue_t *const owner, const struct timespec *const deadline) {
    return pthread_cond_timedwait(&owner->local->space, owner->mut, deadline);
}
void qqueueLocalBackendWakeSpace(qqueue_t *const owner) {
    if (owner->local != NULL) pthread_cond_broadcast(&owner->local->space);
}

/* A joined FE's worker batch is the only maintenance holding. Transfer that
 * lease before taking another batch from its ring. Retained suffixes stay put. */
static void drainFrontend(qqueueLocalFrontend_t *const fe, const struct timespec *const deadline, const int discard) {
    wti_t *const worker = fe->pool->pWrkr[0];
    batch_t *const batch = &worker->batch;
    assert(wtiGetState(worker) == WRKTHRD_STOPPED);
    if (worker->source_queue == fe->owner) {
        pthread_mutex_lock(&fe->mutex);
        const rsRetVal ret = completeBorrow(fe, worker, 1);
        pthread_mutex_unlock(&fe->mutex);
        if (ret != RS_RET_OK) return;
    }
    for (;;) {
        for (int i = 0; i < batch->nElem; ++i) {
            smsg_t *const message = batch->pElem[i].pMsg;
            if (message == NULL) continue;
            if (!discard && qqueueLocalTransferBackend(fe->owner, message, deadline) != RS_RET_OK) return;
            if (discard) {
                msgDestruct(&batch->pElem[i].pMsg);
                counterAdd(&fe->terminal, 1);
                counterAdd(&fe->shutdownDiscarded, 1);
            } else {
                batch->pElem[i].pMsg = NULL;
                counterAdd(&fe->transferred, 1);
            }
            const uint64_t remaining = counterRead(&fe->retry);
            assert(remaining != 0);
            __atomic_store_n(&fe->retry, remaining - 1, __ATOMIC_RELAXED);
        }
        batch->nElem = batch->nElemDeq = 0;
        __atomic_store_n(&fe->active, 0, __ATOMIC_RELAXED);
        __atomic_store_n(&fe->retry, 0, __ATOMIC_RELAXED);
        /* Preserve the lease helper's source-mutex contract even though
         * join and publisher quiescence already grant exclusive maintenance.
         * No BE admission or message destructor executes under this mutex. */
        pthread_mutex_lock(&fe->mutex);
        if (worker->source_queue != NULL) qqueueLeaseClear(&worker->source_queue, &worker->logical_owner, fe->source);
        const rsRetVal bindRet = qqueueLeaseBind(&worker->source_queue, &worker->logical_owner, fe->source, fe->owner,
                                                 worker->pWtp->pUsr, worker->pWtp->pmutUsr, &fe->mutex);
        pthread_mutex_unlock(&fe->mutex);
        if (bindRet != RS_RET_OK) {
            LogError(0, bindRet, "local FE maintenance source identity mismatch");
            return;
        }
        const size_t n = rsSpscQueuePop(&fe->ring, fe->consumerScratch, fe->batchSize);
        if (n == 0) {
            pthread_mutex_lock(&fe->mutex);
            qqueueLeaseClear(&worker->source_queue, &worker->logical_owner, fe->source);
            pthread_mutex_unlock(&fe->mutex);
            return;
        }
        for (size_t i = 0; i < n; ++i) {
            batch->pElem[i].pMsg = fe->consumerScratch[i];
            fe->consumerScratch[i] = NULL;
            batch->eltState[i] = BATCH_STATE_RDY;
        }
        batch->nElem = batch->nElemDeq = (int)n;
        counterAdd(&fe->dequeued, n);
        __atomic_store_n(&fe->retry, n, __ATOMIC_RELAXED);
    }
}

rsRetVal qqueueLocalShutdown(qqueue_t *const owner) {
    return qqueueLocalShutdownUntil(owner, NULL, NULL);
}

rsRetVal qqueueLocalShutdownUntil(qqueue_t *const owner,
                                  const struct timespec *const graphGraceful,
                                  const struct timespec *const graphAction) {
    qqueueLocal_t *const family = owner->local;
    if (family == NULL || stateRead(&family->state) == LOCAL_STOPPED) return RS_RET_OK;
    int oldCancel;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldCancel);
    const struct timespec graceful = graphGraceful != NULL ? *graphGraceful : deadlineAfter(owner->toQShutdown);
    struct timespec action = graceful;
    int actionPhase = 0;
    pthread_mutex_lock(&family->registry);
    pthread_mutex_lock(owner->mut);
    stateSet(&family->state, LOCAL_REDIRECT);
    while (family->idleHelpers != NULL) {
        qqueueLocalFrontend_t *const fe = family->idleHelpers;
        pthread_mutex_lock(&fe->mutex);
        unlinkHelper(fe);
        fe->helpWakePending = 0;
        counterAdd(&fe->wake_shutdown, 1);
        pthread_cond_signal(&fe->pool->pWrkr[0]->pcondBusy);
        pthread_mutex_unlock(&fe->mutex);
    }
    pthread_mutex_unlock(owner->mut);
    pthread_mutex_unlock(&family->registry);
    #ifdef ENABLE_TESTBENCH
    qqueueLocalTestRedirectRelease();
    #endif
    /* Registration is frozen. A publisher with a stale RUNNING observation
     * finishes its notification before maintenance takes its endpoint. */
    for (unsigned i = 0; i < family->count; ++i) {
        qqueueLocalFrontend_t *const fe = &family->fronts[i];
        if (stateRead(&fe->state) != FE_RUNNING) continue;
        pthread_mutex_lock(&fe->mutex);
        while (stateRead(&fe->publishing)) pthread_cond_wait(&fe->publisherDone, &fe->mutex);
        pthread_mutex_unlock(&fe->mutex);
        wtpRequestShutdown(fe->pool, wtpState_SHUTDOWN_IMMEDIATE);
    }
    int pending = 0;
    for (unsigned i = 0; i < family->count; ++i) {
        qqueueLocalFrontend_t *const fe = &family->fronts[i];
        if (stateRead(&fe->state) != FE_RUNNING) continue;
        if (wtpWaitShutdownUntil(fe->pool, &graceful) == RS_RET_OK) {
            stateSet(&fe->state, FE_JOINED);
            drainFrontend(fe, &graceful, 0);
            if (fe->pool->pWrkr[0]->batch.nElem != 0 || rsSpscQueueConsumerAvailable(&fe->ring) != 0) pending = 1;
        } else {
            pending = 1;
        }
    }
    if (pending) {
        action = graphAction != NULL ? *graphAction : deadlineAfter(owner->toActShutdown);
        actionPhase = 1;
        /* One action deadline for the family, never renewed per FE. Its
         * callbacks consult FE source flags. BE remains a live consumer until
         * every FE producer/worker has relinquished its endpoint and lease. */
        for (unsigned i = 0; i < family->count; ++i) {
            qqueueLocalFrontend_t *const fe = &family->fronts[i];
            if (stateRead(&fe->state) == FE_RUNNING) {
                ATOMIC_STORE_32BIT(&fe->source->bShutdownImmediate, &fe->source->mutShutdownImmediate, 1);
                wtpRequestShutdown(fe->pool, wtpState_SHUTDOWN_IMMEDIATE);
            }
        }
    #ifdef ENABLE_TESTBENCH
        testNoteActionPhase(family);
    #endif
        for (unsigned i = 0; i < family->count; ++i) {
            qqueueLocalFrontend_t *const fe = &family->fronts[i];
            if (stateRead(&fe->state) == FE_RUNNING && wtpWaitShutdownUntil(fe->pool, &action) == RS_RET_OK)
                stateSet(&fe->state, FE_JOINED);
        }
        /* Cancel all unfinished FEs before joining one. Join cannot acquire a
         * fresh timeout: it is required for safe lifetime even after expiry. */
        for (unsigned i = 0; i < family->count; ++i) {
            qqueueLocalFrontend_t *const fe = &family->fronts[i];
            if (stateRead(&fe->state) == FE_RUNNING) wtpRequestCancelAll(fe->pool);
        }
        for (unsigned i = 0; i < family->count; ++i) {
            qqueueLocalFrontend_t *const fe = &family->fronts[i];
            if (stateRead(&fe->state) == FE_RUNNING) {
                wtpWaitShutdownUntil(fe->pool, NULL);
                stateSet(&fe->state, FE_JOINED);
            }
        }
    }
    const struct timespec *deadline = actionPhase ? &action : &graceful;
    for (unsigned i = 0; i < family->count; ++i) {
        qqueueLocalFrontend_t *const fe = &family->fronts[i];
        if (stateRead(&fe->state) == FE_JOINED) drainFrontend(fe, deadline, 0);
    }
    /* Internal transfers are finished or explicitly retained. Close late BE
     * admission under its predicate mutex and wait for existing wrapper calls
     * to consume their exact remaining references. Their waits recheck CLOSED. */
    pthread_mutex_lock(owner->mut);
    stateSet(&family->state, LOCAL_CLOSED);
    pthread_cond_broadcast(&owner->notFull);
    pthread_cond_broadcast(&owner->belowFullDlyWtrMrk);
    pthread_cond_broadcast(&owner->belowLightDlyWtrMrk);
    pthread_cond_broadcast(&family->space);
    while (family->beSubmitting != 0) pthread_cond_wait(&family->admissionDone, owner->mut);
    pthread_mutex_unlock(owner->mut);
    if (!actionPhase) action = graphAction != NULL ? *graphAction : deadlineAfter(owner->toActShutdown);
    (void)qqueueShutdownBackendUntil(owner, deadline, &action);
    pthread_setcancelstate(oldCancel, NULL);
    /* A graph first joins every callback pool. Its second pass persists
     * residuals so slow disk I/O cannot defer a downstream callback deadline. */
    return graphGraceful != NULL ? RS_RET_OK : qqueueLocalFinishShutdown(owner);
}

rsRetVal qqueueLocalFinishShutdown(qqueue_t *const owner) {
    qqueueLocal_t *const family = owner->local;
    if (family == NULL || stateRead(&family->state) == LOCAL_STOPPED) return RS_RET_OK;
    int oldCancel;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldCancel);
    /* Persistence retains the existing save-on-shutdown contract after the
     * shared callback deadlines. Consolidation uses that same DA worker, not
     * an expired action deadline, for the remaining accepted FE obligations. */
    rsRetVal saveResult = RS_RET_OK;
    if (owner->bIsDA && owner->bSaveOnShutdown) {
        owner->localDASaving = 1;
        for (unsigned i = 0; i < family->count; ++i) {
            qqueueLocalFrontend_t *const fe = &family->fronts[i];
            if (stateRead(&fe->state) == FE_JOINED) drainFrontend(fe, NULL, 0);
        }
        saveResult = qqueueSaveLocalBackend(owner);
        if (saveResult != RS_RET_OK) LogError(0, saveResult, "local queue: disk save did not drain the backend");
        owner->localDASaving = 0;
    }
    const rsRetVal diskResult = qqueueFinishLocalDisk(owner);
    /* All module state is disposed and threads joined. Every failed transfer
     * still belongs to its FE; classify the residual memory loss exactly once. */
    for (unsigned i = 0; i < family->count; ++i) {
        qqueueLocalFrontend_t *const fe = &family->fronts[i];
        if (stateRead(&fe->state) == FE_JOINED) drainFrontend(fe, NULL, 1);
    }
    qqueueLocalDiscardBackend(owner);
    stateSet(&family->state, LOCAL_STOPPED);
    rsRetVal result = saveResult != RS_RET_OK ? saveResult : diskResult;
    #ifdef ENABLE_TESTBENCH
    if (result == RS_RET_OK) result = testCheckShutdown(owner);
    #endif
    pthread_setcancelstate(oldCancel, NULL);
    return result;
}

void qqueueLocalDestruct(qqueue_t *const owner) {
    qqueueLocal_t *const family = owner->local;
    if (family == NULL) return;
    /* Startup failure has no published registrations. Otherwise shutdown has
     * joined all workers and cleared their leases before this destruction. */
    if (family->fronts != NULL) {
        for (unsigned i = 0; i < family->count; ++i) {
            qqueueLocalFrontend_t *const fe = &family->fronts[i];
            if (fe->pool != NULL) wtpDestruct(&fe->pool);
            if (fe->source != NULL) {
                fe->source->pWtpReg = NULL;
                qqueueDestruct(&fe->source);
            }
            free(fe->slots);
            free(fe->producerScratch);
            free(fe->consumerScratch);
            if (fe->condInitialized) pthread_cond_destroy(&fe->publisherDone);
            if (fe->mutexInitialized) pthread_mutex_destroy(&fe->mutex);
        }
        free(family->fronts);
    }
    #ifdef ENABLE_TESTBENCH
    free(__atomic_load_n(&family->testShutdownMarker, __ATOMIC_ACQUIRE));
    free(family->testActionMarker);
    free(family->testForceTermMarker);
    free(family->testHelpGateMarker);
    if (family->testHelpGateFd >= 0) close(family->testHelpGateFd);
    #endif
    if (family->admissionInitialized) pthread_cond_destroy(&family->admissionDone);
    if (family->spaceInitialized) pthread_cond_destroy(&family->space);
    if (family->registryInitialized) pthread_mutex_destroy(&family->registry);
    free(family);
    owner->local = NULL;
}

int qqueueLocalGetFrontendSnapshot(const qqueue_t *const owner,
                                   const uint32_t index,
                                   qqueueLocalFrontendSnapshot_t *const snapshot) {
    memset(snapshot, 0, sizeof(*snapshot));
    if (owner->local == NULL || index >= owner->local->count) return 0;
    const qqueueLocalFrontend_t *const fe = &owner->local->fronts[index];
    snapshot->state = stateRead(&fe->state);
    if (snapshot->state == FE_UNUSED) return 0;
    snapshot->identity = (uint64_t)index + 1;
    snapshot->generation = 1;
    snapshot->submitted_batches = counterRead(&fe->submittedBatches);
    snapshot->submitted_max = counterRead(&fe->submittedMax);
    snapshot->overflow_batches = counterRead(&fe->overflowBatches);
    snapshot->oversized_batches = counterRead(&fe->oversizedBatches);
    snapshot->published_batches = counterRead(&fe->publishedBatches);
    snapshot->dequeue_max = counterRead(&fe->dequeueMax);
    snapshot->dequeue_messages = counterRead(&fe->dequeueMessages);
    snapshot->capacity = fe->capacity;
    snapshot->help_limit = fe->helperLimit;
    #define SNAP(field) snapshot->field = counterRead(&fe->field)
    SNAP(help_attempts);
    SNAP(help_empty);
    SNAP(help_batches);
    SNAP(help_messages);
    SNAP(help_max);
    SNAP(help_active);
    SNAP(help_retry);
    SNAP(help_completed);
    SNAP(help_returned);
    SNAP(wake_fe);
    SNAP(wake_shutdown);
    SNAP(help_terminal);
    SNAP(help_waits);
    SNAP(help_wakes);
    SNAP(attempts);
    SNAP(published);
    SNAP(dequeued);
    SNAP(terminal);
    SNAP(active);
    SNAP(retry);
    SNAP(overflow);
    SNAP(nofit);
    SNAP(oversized);
    SNAP(transferred);
    SNAP(bytes);
    SNAP(batches);
    #undef SNAP
    snapshot->shutdown_discarded = counterRead(&fe->shutdownDiscarded);
    snapshot->producer_exited = stateRead(&fe->producerExited);
    snapshot->queued = snapshot->published >= snapshot->dequeued ? snapshot->published - snapshot->dequeued : 0;
    return 1;
}

/* Add only newly observed FE publications to legacy resettable counters.
 * BE already updates those counters at admission. The pre-read callback is
 * serialized by the stats-list lock; delta bridging therefore preserves reset
 * behavior without contended FE hot-path writes to the logical queue counters. */
void qqueueLocalRefreshLegacy(qqueue_t *const owner) {
    qqueueLocal_t *const family = owner->local;
    if (family == NULL) return;
    uint64_t published = 0, bytes = 0;
    for (unsigned i = 0; i < family->count; ++i) {
        published += counterRead(&family->fronts[i].published);
        bytes += counterRead(&family->fronts[i].bytes);
    }
    __atomic_fetch_add(&owner->ctrEnqueued, published - family->legacyPublished, __ATOMIC_RELAXED);
    __atomic_fetch_add(&owner->ctrSizeEnqueued, bytes - family->legacyBytes, __ATOMIC_RELAXED);
    const uint64_t policyMessages = counterRead(&family->sampledOut) + counterRead(&family->severityDiscarded) +
                                    counterRead(&family->policyRejected);
    const uint64_t policyBytes = counterRead(&family->policyBytes);
    __atomic_fetch_add(&owner->ctrEnqueued, policyMessages - family->legacyPolicyMessages, __ATOMIC_RELAXED);
    __atomic_fetch_add(&owner->ctrSizeEnqueued, policyBytes - family->legacyPolicyBytes, __ATOMIC_RELAXED);
    const uint64_t severity = counterRead(&family->severityDiscarded);
    __atomic_fetch_add(&owner->ctrNFDscrd, severity - family->legacySeverityDiscarded, __ATOMIC_RELAXED);
    family->legacySeverityDiscarded = severity;
    family->legacyPolicyMessages = policyMessages;
    family->legacyPolicyBytes = policyBytes;
    family->legacyPublished = published;
    family->legacyBytes = bytes;
}

void qqueueLocalGetSnapshot(const qqueue_t *const owner, qqueueLocalSnapshot_t *const snapshot) {
    memset(snapshot, 0, sizeof(*snapshot));
    const qqueueLocal_t *const family = owner->local;
    if (family == NULL) return;
    snapshot->sampled_out = counterRead(&family->sampledOut);
    snapshot->severity_discarded = counterRead(&family->severityDiscarded);
    snapshot->retry_failed = counterRead(&family->retryFailed);
    snapshot->be_capacity = (uint64_t)owner->iMaxQueueSize;
    snapshot->fe_capacity = (uint64_t)family->count * family->fronts[0].capacity;
    snapshot->fe_active_capacity = (uint64_t)family->count * family->fronts[0].batchSize;
    snapshot->reserved_messages = snapshot->be_capacity + snapshot->fe_capacity + snapshot->fe_active_capacity;
    snapshot->be_dequeue_batches = counterRead(&family->beDequeueBatches);
    snapshot->be_dequeue_messages = counterRead(&family->beDequeueMessages);
    snapshot->be_dequeue_max = counterRead(&family->beDequeueMax);
    snapshot->be_attempted = counterRead(&family->beAttempted);
    snapshot->be_admitted = counterRead(&family->beAdmitted);
    snapshot->be_terminal = counterRead(&family->beTerminal);
    snapshot->preadmission_rejected = counterRead(&family->beRejected) + counterRead(&family->policyRejected);
    snapshot->shutdown_discarded = counterRead(&family->beShutdownDiscarded);
    snapshot->attempts = snapshot->be_attempted + counterRead(&family->policyRejected) + snapshot->sampled_out +
                         snapshot->severity_discarded;
    snapshot->admitted = snapshot->be_admitted;
    snapshot->restored = counterRead(&family->diskRestored);
    snapshot->persisted = counterRead(&family->diskPersisted);
    snapshot->disk_terminal = counterRead(&family->diskTerminal);
    snapshot->disk_transferred = counterRead(&family->diskTransferred);
    snapshot->discarded = counterRead(&family->beDiscarded) + counterRead(&family->diskDiscarded);
    snapshot->terminal = snapshot->be_terminal + snapshot->disk_terminal + snapshot->persisted;
    const qqueue_t *const disk = __atomic_load_n(&owner->pqDA, __ATOMIC_ACQUIRE);
    if (disk != NULL) {
        snapshot->disk_physical = (uint64_t)__atomic_load_n(&disk->iQueueSize, __ATOMIC_ACQUIRE);
        snapshot->disk_active = (uint64_t)__atomic_load_n(&disk->nLogDeq, __ATOMIC_ACQUIRE);
    }
    snapshot->be_physical = (uint64_t)__atomic_load_n(&owner->iQueueSize, __ATOMIC_ACQUIRE);
    snapshot->be_active = (uint64_t)__atomic_load_n(&owner->nLogDeq, __ATOMIC_ACQUIRE);
    snapshot->configured_frontends = family->count;
    snapshot->route_be_messages = snapshot->be_attempted;
    snapshot->route_be_batches = counterRead(&family->beBatches);
    snapshot->be_nofit = counterRead(&family->beNofit);
    snapshot->be_oversized = counterRead(&family->beOversized);
    snapshot->be_registration_fallback = counterRead(&family->beRegistration);
    snapshot->be_shutdown_redirect = counterRead(&family->beRedirect);
    snapshot->be_unclassified = counterRead(&family->beUnclassified);
    snapshot->be_internal = counterRead(&family->beInternal);
    snapshot->be_capacity_exhausted = counterRead(&family->beCapacity);
    snapshot->capacity_exhaustions = counterRead(&family->capacityExhaustions);
    snapshot->be_consumers = (uint64_t)__atomic_load_n(&owner->pWtpReg->iCurNumWrkThrd, __ATOMIC_RELAXED);
    snapshot->fe_registered = __atomic_load_n(&family->registered, __ATOMIC_RELAXED);
    snapshot->fe_started = __atomic_load_n(&family->started, __ATOMIC_RELAXED);
    snapshot->registration_failures = counterRead(&family->registrationFailures);
    snapshot->allocation_bytes = family->allocationBytes;
    for (unsigned i = 0; i < family->count; ++i) {
        qqueueLocalFrontendSnapshot_t fe;
        if (!qqueueLocalGetFrontendSnapshot(owner, i, &fe)) continue;
        /* FE overflow/redirect attempts are already in beAttempted. */
        snapshot->attempts += fe.published;
        snapshot->admitted += fe.published;
        snapshot->route_fe_messages += fe.published;
        snapshot->route_fe_batches += fe.published_batches;
        if (fe.state == FE_RUNNING) {
            const wti_t *const worker = family->fronts[i].pool->pWrkr[0];
            if (__atomic_load_n(&worker->bIsRunning, __ATOMIC_ACQUIRE) == WRKTHRD_RUNNING &&
                !__atomic_load_n(&worker->bExiting, __ATOMIC_ACQUIRE))
                ++snapshot->fe_consumers;
        }
        snapshot->help_attempts += fe.help_attempts;
        snapshot->help_empty += fe.help_empty;
        snapshot->help_batches += fe.help_batches;
        snapshot->help_messages += fe.help_messages;
        snapshot->help_active += fe.help_active;
        snapshot->help_retry += fe.help_retry;
        snapshot->help_completed += fe.help_completed;
        snapshot->help_returned += fe.help_returned;
        snapshot->wake_fe += fe.wake_fe;
        snapshot->wake_shutdown += fe.wake_shutdown;
        snapshot->help_terminal += fe.help_terminal;
        snapshot->help_waits += fe.help_waits;
        snapshot->help_wakes += fe.help_wakes;
        if (fe.help_max > snapshot->help_max) snapshot->help_max = fe.help_max;
        snapshot->help_limit = fe.help_limit;
        snapshot->terminal += fe.terminal;
        snapshot->shutdown_discarded += fe.shutdown_discarded;
        snapshot->discarded += fe.shutdown_discarded + counterRead(&family->fronts[i].discarded);
        snapshot->fe_queued += fe.queued;
        snapshot->fe_active += fe.active;
        snapshot->fe_retry += fe.retry;
        snapshot->transferred += fe.transferred;
        snapshot->fe_producerless += fe.producer_exited;
    }
    const uint64_t obligations = snapshot->admitted + snapshot->restored;
    snapshot->outstanding = obligations >= snapshot->terminal ? obligations - snapshot->terminal : 0;
    snapshot->executed = snapshot->terminal >= snapshot->persisted + snapshot->discarded
                             ? snapshot->terminal - snapshot->persisted - snapshot->discarded
                             : 0;
}

#else
void qqueueLocalRetryFailed(qqueue_t *owner) {
    (void)owner;
}
void qqueueLocalBackendShutdownDiscarded(qqueue_t *owner) {
    (void)owner;
}
    #ifdef ENABLE_TESTBENCH
int qqueueLocalTestFailNode(const qqueue_t *owner, const int retry) {
    (void)owner;
    (void)retry;
    return 0;
}
int qqueueLocalTestProducerShouldExit(void) {
    return 0;
}
rsRetVal qqueueLocalTestArmShutdownCheck(qqueue_t *owner, const char *markerPath) {
    (void)owner;
    (void)markerPath;
    return RS_RET_NOT_IMPLEMENTED;
}
void qqueueLocalTestRedirectArm(void) {}
int qqueueLocalTestRedirectWaitPublisher(unsigned timeout_ms) {
    (void)timeout_ms;
    return 0;
}
void qqueueLocalTestRedirectRelease(void) {}
void qqueueLocalTestNoteForceTerm(wti_t *worker, int currentIParams) {
    (void)worker;
    (void)currentIParams;
}
    #endif
/* Unsupported local targets preserve the global build and fail activation. */
int qqueueLocalEnabled(void) {
    return 0;
}
void qqueueLocalProducerEnter(void) {}
void qqueueLocalProducerLeave(void) {}
void qqueueLocalProducerExit(void *unused) {
    (void)unused;
}
rsRetVal qqueueLocalStart(qqueue_t *owner) {
    (void)owner;
    return RS_RET_NOT_IMPLEMENTED;
}
rsRetVal qqueueLocalSubmit(qqueue_t *owner, smsg_t *const *messages, size_t count, int flow) {
    return qqueueLocalSubmitBackend(owner, messages, count, flow, QLOCAL_UNCLASSIFIED);
}
rsRetVal qqueueLocalFinishShutdown(qqueue_t *owner) {
    (void)owner;
    return RS_RET_NOT_IMPLEMENTED;
}
rsRetVal qqueueLocalShutdown(qqueue_t *owner) {
    (void)owner;
    return RS_RET_NOT_IMPLEMENTED;
}
rsRetVal qqueueLocalShutdownUntil(qqueue_t *owner, const struct timespec *graceful, const struct timespec *action) {
    (void)graceful;
    (void)action;
    return qqueueLocalShutdown(owner);
}

void qqueueLocalDestruct(qqueue_t *owner) {
    (void)owner;
}
int qqueueLocalIsClosed(const qqueue_t *owner) {
    (void)owner;
    return 0;
}
int qqueueLocalWorker(const wti_t *worker) {
    (void)worker;
    return 0;
}
void qqueueLocalRetainAmbiguous(wti_t *worker) {
    (void)worker;
}
void qqueueLocalBackendRoute(qqueue_t *owner, size_t count, enum qqueueLocalRouteReason reason) {
    (void)owner;
    (void)count;
    (void)reason;
}
void qqueueLocalNoteActionPhase(qqueue_t *owner) {
    (void)owner;
}
void qqueueLocalDiskRestored(qqueue_t *owner, uint64_t count) {
    (void)owner;
    (void)count;
}
void qqueueLocalDiskTransferred(qqueue_t *owner, uint64_t count) {
    (void)owner;
    (void)count;
}
void qqueueLocalDiskTerminal(qqueue_t *owner, uint64_t count, uint64_t discarded) {
    (void)owner;
    (void)count;
    (void)discarded;
}
void qqueueLocalDiskPersisted(qqueue_t *owner, uint64_t count) {
    (void)owner;
    (void)count;
}
    #define LOCAL_COUNTER_STUB(name)                 \
        void name(qqueue_t *owner, uint64_t count) { \
            (void)owner;                             \
            (void)count;                             \
        }
LOCAL_COUNTER_STUB(qqueueLocalBackendAcquired)
LOCAL_COUNTER_STUB(qqueueLocalBackendAttempt)
LOCAL_COUNTER_STUB(qqueueLocalBackendAdmitted)
LOCAL_COUNTER_STUB(qqueueLocalBackendRejected)
void qqueueLocalBackendTerminal(qqueue_t *owner, uint64_t count, uint64_t discarded) {
    (void)owner;
    (void)count;
    (void)discarded;
}
void qqueueLocalBackendBegin(qqueue_t *owner) {
    (void)owner;
}
void qqueueLocalBackendEnd(qqueue_t *owner) {
    (void)owner;
}
int qqueueLocalBackendWaitSpace(qqueue_t *owner, const struct timespec *deadline) {
    (void)owner;
    (void)deadline;
    return ENOTSUP;
}
int qqueueLocalBorrowWorker(const qqueue_t *owner, const wti_t *worker) {
    (void)owner;
    (void)worker;
    return 0;
}
void qqueueLocalWakeBackendHelpers(qqueue_t *owner) {
    (void)owner;
}
void qqueueLocalBackendWakeSpace(qqueue_t *owner) {
    (void)owner;
}
void qqueueLocalRefreshLegacy(qqueue_t *owner) {
    (void)owner;
}
void qqueueLocalGetSnapshot(const qqueue_t *owner, qqueueLocalSnapshot_t *snapshot) {
    (void)owner;
    memset(snapshot, 0, sizeof(*snapshot));
}
int qqueueLocalGetFrontendSnapshot(const qqueue_t *owner, uint32_t index, qqueueLocalFrontendSnapshot_t *snapshot) {
    (void)owner;
    (void)index;
    memset(snapshot, 0, sizeof(*snapshot));
    return 0;
}
#endif
