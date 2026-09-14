/* Experimental memory-only local queues.
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
    uint32_t index, capacity, batchSize;
    uint64_t attempts, published, dequeued, terminal, active, retry;
    uint64_t overflow, nofit, oversized, transferred, shutdownDiscarded, bytes, batches;
    uint64_t publishedBatches, dequeueMax, dequeueMessages;
    uint64_t submittedBatches, submittedMax, overflowBatches, oversizedBatches;
};

struct qqueueLocal_s {
    qqueue_t *owner;
    qqueueLocalFrontend_t *fronts;
    pthread_mutex_t registry;
    pthread_cond_t space;
    pthread_cond_t admissionDone;
    unsigned registryInitialized, spaceInitialized, admissionInitialized;
    unsigned state, count, nextRegistration, registered, started;
    unsigned beSubmitting; /* owner->mut */
    uint64_t beAttempted, beAdmitted, beRejected, beTerminal, beDiscarded;
    uint64_t registrationFailures, allocationBytes;
    uint64_t beBatches, beNofit, beOversized, beRegistration, beRedirect, beUnclassified;
    uint64_t beInternal, beCapacity, capacityExhaustions;
    uint64_t beDequeueBatches, beDequeueMessages, beDequeueMax;
    uint64_t legacyPublished, legacyBytes; /* stats-list-serialized reader only */
};

typedef struct localCache_s {
    struct localCache_s *next;
    qqueue_t *owner;
    qqueueLocalFrontend_t *frontend;
    enum qqueueLocalRouteReason failureReason;
} localCache_t;

typedef struct localProducer_s {
    localCache_t *cache;
    unsigned trustedDepth;
} localProducer_t;

static pthread_once_t producerOnce = PTHREAD_ONCE_INIT;
static pthread_key_t producerKey;
static unsigned producerKeyValid;
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
    ++producer->trustedDepth;
}

void qqueueLocalProducerLeave(void) {
    localProducer_t *const producer = stateRead(&producerKeyValid) ? pthread_getspecific(producerKey) : NULL;
    if (producer != NULL && producer->trustedDepth != 0) --producer->trustedDepth;
}

void qqueueLocalProducerExit(void *const unused) {
    (void)unused;
    localProducer_t *const producer = stateRead(&producerKeyValid) ? pthread_getspecific(producerKey) : NULL;
    if (producer == NULL) return;
    /* Called by tcpsrv Run/worker cleanup, before input joins and queue teardown.
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

static rsRetVal feComplete(void *const source, wti_t *const worker) {
    qqueue_t *const queue = source;
    qqueueLocalFrontend_t *const fe = queue->localSource;
    batch_t *const batch = &worker->batch;
    if (worker->source_queue == NULL) return RS_RET_OK;
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
    ret = qqueueLeaseBind(&worker->source_queue, &worker->logical_owner, queue, fe->owner, worker->pWtp->pUsr,
                          worker->pWtp->pmutUsr, queue->mut);
    if (ret != RS_RET_OK) return RS_RET_ERR_QUEUE_EMERGENCY;
    const size_t count = rsSpscQueuePop(&fe->ring, fe->consumerScratch, fe->batchSize);
    if (count == 0) {
        qqueueLeaseClear(&worker->source_queue, &worker->logical_owner, queue);
        return RS_RET_IDLE;
    }
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
    /* Lifecycle belongs to the logical family; callback interruption belongs
     * to the physical source. This lets FE callbacks settle while BE remains
     * available for their residual obligations under the same phase deadline. */
    worker->pbShutdownImmediate = &queue->bShutdownImmediate;
    #ifndef HAVE_ATOMIC_BUILTINS
    worker->pmutShutdownImmediate = &queue->mutShutdownImmediate;
    #endif
    pthread_mutex_unlock(queue->mut);
    int oldCancel;
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldCancel);
    ret = fe->owner->pConsumer(fe->owner->pAction, batch, worker);
    pthread_setcancelstate(oldCancel, NULL);
    pthread_mutex_lock(queue->mut);
    /* Completion is source-bound on the next iteration or exit. Callback
     * errors cannot bypass disabled cancellation before reacquiring mutex. */
    (void)ret;
    return RS_RET_OK;
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
    if (pthread_mutex_init(&fe->mutex, NULL) != 0) return RS_RET_ERR;
    fe->mutexInitialized = 1;
    if (initMonotonicCond(&fe->publisherDone) != 0) return RS_RET_ERR;
    fe->condInitialized = 1;
    CHKmalloc(fe->slots = calloc(capacity, sizeof(void *)));
    CHKmalloc(fe->producerScratch = calloc(capacity, sizeof(void *)));
    CHKmalloc(fe->consumerScratch = calloc(batchSize, sizeof(void *)));
    if (!rsSpscQueueInit(&fe->ring, fe->slots, capacity)) ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
    CHKiRet(qqueueConstruct(&fe->source, QUEUETYPE_FIXED_ARRAY, 1, 0, owner->pConsumer));
    fe->source->localSource = fe;
    fe->source->mut = &fe->mutex;
    fe->source->iDeqBatchSize = (int)batchSize;
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
    if (owner->qType != QUEUETYPE_FIXED_ARRAY || owner->bIsDA || owner->localFrontendSize <= 0 ||
        owner->localMaxFrontends <= 0 || owner->iDeqBatchSize <= 0)
        return RS_RET_PARAM_ERROR;
    qqueueLocal_t *family = calloc(1, sizeof(*family));
    if (family == NULL) return RS_RET_OUT_OF_MEMORY;
    owner->local = family; /* unpublished configuration startup */
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
    localCache_t *const cache = calloc(1, sizeof(*cache));
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
        const rsRetVal ret = wtpAdviseMaxWorkers(fe->pool, 1, DENY_WORKER_START_DURING_SHUTDOWN);
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

rsRetVal qqueueLocalSubmit(qqueue_t *const owner,
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
    const int trusted = !internal && producer != NULL && producer->trustedDepth != 0 && owner->local != NULL;
    if (internal) reason = QLOCAL_INTERNAL;
    if (trusted) fe = registerFrontend(owner, producer, &reason);
    if (fe != NULL) {
        counterAdd(&fe->attempts, count);
        counterAdd(&fe->submittedBatches, 1);
        if (count > counterRead(&fe->submittedMax)) __atomic_store_n(&fe->submittedMax, count, __ATOMIC_RELAXED);
        /* Seq-cst two-sided handshake: either shutdown observes publishing,
         * or a publisher beginning later observes REDIRECT and never writes. */
        stateSet(&fe->publishing, 1);
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
                counterAdd(&fe->published, count);
                counterAdd(&fe->publishedBatches, 1);
                counterAdd(&fe->bytes, bytes);
                pthread_mutex_lock(&fe->mutex);
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

int qqueueLocalIsClosed(const qqueue_t *const owner) {
    return owner->local != NULL && stateRead(&owner->local->state) >= LOCAL_CLOSED;
}

int qqueueLocalWorker(const wti_t *const worker) {
    const qqueue_t *const source = worker->pWtp == NULL ? NULL : worker->pWtp->pUsr;
    return source != NULL && (source->local != NULL || source->localSource != NULL);
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
    qqueueLocal_t *const family = owner->local;
    if (family == NULL || stateRead(&family->state) == LOCAL_STOPPED) return RS_RET_OK;
    int oldCancel;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldCancel);
    const struct timespec graceful = deadlineAfter(owner->toQShutdown);
    struct timespec action = graceful;
    int actionPhase = 0;
    pthread_mutex_lock(&family->registry);
    stateSet(&family->state, LOCAL_REDIRECT);
    pthread_mutex_unlock(&family->registry);
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
        action = deadlineAfter(owner->toActShutdown);
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
        for (unsigned i = 0; i < family->count; ++i) {
            qqueueLocalFrontend_t *const fe = &family->fronts[i];
            if (stateRead(&fe->state) == FE_RUNNING && wtpWaitShutdownUntil(fe->pool, &action) == RS_RET_OK)
                stateSet(&fe->state, FE_JOINED);
        }
        /* Cancel all unfinished FEs before joining one. Join cannot acquire a
         * fresh timeout: it is required for safe lifetime even after expiry. */
        for (unsigned i = 0; i < family->count; ++i) {
            qqueueLocalFrontend_t *const fe = &family->fronts[i];
            if (stateRead(&fe->state) == FE_RUNNING) wtpCancelAll(fe->pool, (const uchar *)"local FE shutdown");
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
    wtpRequestShutdown(owner->pWtpReg, wtpState_SHUTDOWN);
    if (wtpWaitShutdownUntil(owner->pWtpReg, deadline) != RS_RET_OK) {
        if (!actionPhase) {
            action = deadlineAfter(owner->toActShutdown);
            actionPhase = 1;
        }
        ATOMIC_STORE_32BIT(&owner->bShutdownImmediate, &owner->mutShutdownImmediate, 1);
        wtpRequestShutdown(owner->pWtpReg, wtpState_SHUTDOWN_IMMEDIATE);
        (void)wtpWaitShutdownUntil(owner->pWtpReg, &action);
    }
    wtpCancelAll(owner->pWtpReg, (const uchar *)"local BE shutdown");
    wtpWaitShutdownUntil(owner->pWtpReg, NULL);
    /* All module state is disposed and threads joined. Every failed transfer
     * still belongs to its FE; classify the residual memory loss exactly once. */
    for (unsigned i = 0; i < family->count; ++i) {
        qqueueLocalFrontend_t *const fe = &family->fronts[i];
        if (stateRead(&fe->state) == FE_JOINED) drainFrontend(fe, NULL, 1);
    }
    qqueueLocalDiscardBackend(owner);
    stateSet(&family->state, LOCAL_STOPPED);
    pthread_setcancelstate(oldCancel, NULL);
    return RS_RET_OK;
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
    #define SNAP(field) snapshot->field = counterRead(&fe->field)
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
    family->legacyPublished = published;
    family->legacyBytes = bytes;
}

void qqueueLocalGetSnapshot(const qqueue_t *const owner, qqueueLocalSnapshot_t *const snapshot) {
    memset(snapshot, 0, sizeof(*snapshot));
    const qqueueLocal_t *const family = owner->local;
    if (family == NULL) return;
    snapshot->be_dequeue_batches = counterRead(&family->beDequeueBatches);
    snapshot->be_dequeue_messages = counterRead(&family->beDequeueMessages);
    snapshot->be_dequeue_max = counterRead(&family->beDequeueMax);
    snapshot->be_attempted = counterRead(&family->beAttempted);
    snapshot->be_admitted = counterRead(&family->beAdmitted);
    snapshot->be_terminal = counterRead(&family->beTerminal);
    snapshot->preadmission_rejected = counterRead(&family->beRejected);
    snapshot->shutdown_discarded = counterRead(&family->beDiscarded);
    snapshot->attempts = snapshot->be_attempted;
    snapshot->admitted = snapshot->be_admitted;
    snapshot->terminal = snapshot->be_terminal;
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
        snapshot->terminal += fe.terminal;
        snapshot->shutdown_discarded += fe.shutdown_discarded;
        snapshot->fe_queued += fe.queued;
        snapshot->fe_active += fe.active;
        snapshot->fe_retry += fe.retry;
        snapshot->transferred += fe.transferred;
        snapshot->fe_producerless += fe.producer_exited;
    }
    snapshot->outstanding = snapshot->admitted >= snapshot->terminal ? snapshot->admitted - snapshot->terminal : 0;
}

#else
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
rsRetVal qqueueLocalShutdown(qqueue_t *owner) {
    (void)owner;
    return RS_RET_NOT_IMPLEMENTED;
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
void qqueueLocalBackendRoute(qqueue_t *owner, size_t count, enum qqueueLocalRouteReason reason) {
    (void)owner;
    (void)count;
    (void)reason;
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
