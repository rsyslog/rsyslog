/* SPDX-License-Identifier: Apache-2.0
 * Unit coverage for the non-destructive WTP/queue batch barrier. Workers are
 * synchronized with conditions rather than sleeps. The oracle is that idle and
 * active workers acknowledge only at a boundary, zero-worker pools complete
 * immediately, timeouts leave the token releasable, direct queues fail closed,
 * both DA consumer pools pause under their own mutexes, latent segmented-disk
 * work and zero-worker demand are advised on resume, token ownership is
 * enforced, and shutdown state is never overwritten. Opt-in completion tests
 * hold the real production boundary callback, retain exact errors, invalidate
 * a generation during unlocked disposal, balance cancellation/shutdown, and
 * include DA transfer without changing ordinary barrier cleanup semantics.
 */
#include "config.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "rsyslog.h"
#include "atomic.h"
#include "queue.h"
#include "wti.h"
#include "wtp.h"

typedef struct advice_call_s {
    wtp_t *pool;
    int workers;
} advice_call_t;

static advice_call_t adviceCalls[8];
static size_t adviceCallCount;
static sbool segmentedDiskMayHaveData;

/* Keep this synchronization unit independent of the daemon/runtime link
 * closure while retaining an exact oracle for resume-time worker demand. */
rsRetVal wtpAdviseMaxWorkers(wtp_t *const pThis,
                             const int nMaxWrkr,
                             const int __attribute__((unused)) permit_during_shutdown) {
    if (adviceCallCount < sizeof(adviceCalls) / sizeof(adviceCalls[0])) {
        adviceCalls[adviceCallCount].pool = pThis;
        adviceCalls[adviceCallCount].workers = nMaxWrkr;
    }
    ++adviceCallCount;
    return RS_RET_OK;
}

sbool segdiskStoreMayHaveData(const segdisk_store_t __attribute__((unused)) * store) {
    return segmentedDiskMayHaveData;
}

#include "../../runtime/wtp-quiesce.c"
#include "../../runtime/queue-barrier.c"

#define CHECK(condition)                                                                    \
    do {                                                                                    \
        if (!(condition)) {                                                                 \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            return 1;                                                                       \
        }                                                                                   \
    } while (0)

#define THREAD_CHECK(condition)                                                             \
    do {                                                                                    \
        if (!(condition)) {                                                                 \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            return (void *)(uintptr_t)1;                                                    \
        }                                                                                   \
    } while (0)

typedef struct test_pool_s {
    wtp_t pool;
    wti_t worker;
    wti_t *workers[1];
    pthread_mutex_t queueMutex;
} test_pool_t;

typedef struct worker_ctx_s {
    test_pool_t *testPool;
    pthread_mutex_t gateMutex;
    pthread_cond_t gateCond;
    int ready;
    int releaseWork;
    int active;
} worker_ctx_t;

typedef struct foreign_owner_ctx_s {
    qqueue_batch_barrier_t *barrier;
    rsRetVal waitResult;
    rsRetVal releaseResult;
    int retainedToken;
} foreign_owner_ctx_t;

static objInfo_t wtpObjInfo = {.pszID = (uchar *)"wtp"};

static void setObjectIdentity(obj_t *const object, objInfo_t *const info) {
    object->pObjInfo = info;
#ifndef NDEBUG
    object->iObjCooCKiE = 0xBADEFEE;
#endif
}

static void initPool(test_pool_t *const testPool, const int activeWorkers) {
    memset(testPool, 0, sizeof(*testPool));
    setObjectIdentity((obj_t *)&testPool->pool, &wtpObjInfo);
    pthread_mutex_init(&testPool->queueMutex, NULL);
    pthread_mutex_init(&testPool->pool.mutWtp, NULL);
    pthread_cond_init(&testPool->pool.condQuiesced, NULL);
    INIT_ATOMIC_HELPER_MUT(testPool->pool.mutCurNumWrkThrd);
    INIT_ATOMIC_HELPER_MUT(testPool->pool.mutWtpState);
    testPool->pool.pmutUsr = &testPool->queueMutex;
    testPool->pool.wtpState = wtpState_RUNNING;
    testPool->pool.iNumWorkerThreads = 1;
    testPool->pool.iCurNumWrkThrd = activeWorkers;
    testPool->pool.pWrkr = testPool->workers;
    testPool->workers[0] = &testPool->worker;
    pthread_cond_init(&testPool->worker.pcondBusy, NULL);
}

static void cleanupPool(test_pool_t *const testPool) {
    pthread_cond_destroy(&testPool->worker.pcondBusy);
    DESTROY_ATOMIC_HELPER_MUT(testPool->pool.mutWtpState);
    DESTROY_ATOMIC_HELPER_MUT(testPool->pool.mutCurNumWrkThrd);
    pthread_cond_destroy(&testPool->pool.condQuiesced);
    pthread_mutex_destroy(&testPool->pool.mutWtp);
    pthread_mutex_destroy(&testPool->queueMutex);
}

static int deadlineMs(const long milliseconds, struct timespec *const deadline) {
    CHECK(clock_gettime(CLOCK_REALTIME, deadline) == 0);
    deadline->tv_nsec += (milliseconds % 1000) * 1000000L;
    deadline->tv_sec += milliseconds / 1000 + deadline->tv_nsec / 1000000000L;
    deadline->tv_nsec %= 1000000000L;
    return 0;
}

static void *foreignOwnerMain(void *const arg) {
    foreign_owner_ctx_t *const ctx = arg;
    qqueue_batch_barrier_t *alias = ctx->barrier;
    struct timespec deadline;

    THREAD_CHECK(deadlineMs(100, &deadline) == 0);
    ctx->waitResult = qqueueBatchBarrierWait(alias, &deadline);
    ctx->releaseResult = qqueueBatchBarrierRelease(&alias);
    ctx->retainedToken = alias == ctx->barrier;
    return NULL;
}

static wtpState_t poolState(test_pool_t *const testPool) {
    return (wtpState_t)ATOMIC_LOAD_32BIT((int *)&testPool->pool.wtpState, &testPool->pool.mutWtpState);
}

static void setPoolState(test_pool_t *const testPool, const wtpState_t state) {
    ATOMIC_STORE_32BIT((int *)&testPool->pool.wtpState, &testPool->pool.mutWtpState, state);
}

static int advised(const wtp_t *const pool, const int workers) {
    for (size_t i = 0; i < adviceCallCount && i < sizeof(adviceCalls) / sizeof(adviceCalls[0]); ++i)
        if (adviceCalls[i].pool == pool && adviceCalls[i].workers == workers) return 1;
    return 0;
}

static void *workerMain(void *const arg) {
    worker_ctx_t *const ctx = arg;
    test_pool_t *const testPool = ctx->testPool;

    if (ctx->active) {
        pthread_mutex_lock(&ctx->gateMutex);
        ctx->ready = 1;
        pthread_cond_signal(&ctx->gateCond);
        while (!ctx->releaseWork) pthread_cond_wait(&ctx->gateCond, &ctx->gateMutex);
        pthread_mutex_unlock(&ctx->gateMutex);
        pthread_mutex_lock(&testPool->queueMutex);
    } else {
        pthread_mutex_lock(&testPool->queueMutex);
        pthread_mutex_lock(&ctx->gateMutex);
        ctx->ready = 1;
        pthread_cond_signal(&ctx->gateCond);
        pthread_mutex_unlock(&ctx->gateMutex);
        while (poolState(testPool) == wtpState_RUNNING)
            pthread_cond_wait(&testPool->worker.pcondBusy, &testPool->queueMutex);
    }

    THREAD_CHECK(poolState(testPool) == wtpState_QUIESCE);
    wtpWorkerQuiesceAtBoundary(&testPool->pool, &testPool->worker);
    pthread_mutex_unlock(&testPool->queueMutex);
    return NULL;
}

static void waitWorkerReady(worker_ctx_t *const ctx) {
    pthread_mutex_lock(&ctx->gateMutex);
    while (!ctx->ready) pthread_cond_wait(&ctx->gateCond, &ctx->gateMutex);
    pthread_mutex_unlock(&ctx->gateMutex);
}

static void initWorkerContext(worker_ctx_t *const ctx, test_pool_t *const testPool, const int active) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->testPool = testPool;
    ctx->active = active;
    pthread_mutex_init(&ctx->gateMutex, NULL);
    pthread_cond_init(&ctx->gateCond, NULL);
}

static void cleanupWorkerContext(worker_ctx_t *const ctx) {
    pthread_cond_destroy(&ctx->gateCond);
    pthread_mutex_destroy(&ctx->gateMutex);
}

static int testIdleWorker(void) {
    test_pool_t testPool;
    worker_ctx_t ctx;
    pthread_t thread;
    initPool(&testPool, 1);
    initWorkerContext(&ctx, &testPool, 0);
    CHECK(pthread_create(&thread, NULL, workerMain, &ctx) == 0);
    waitWorkerReady(&ctx);

    CHECK(wtpRequestQuiesce(&testPool.pool) == RS_RET_OK);
    struct timespec deadline;
    CHECK(deadlineMs(1000, &deadline) == 0);
    CHECK(wtpWaitQuiesced(&testPool.pool, &deadline) == RS_RET_OK);
    CHECK(wtpResume(&testPool.pool) == RS_RET_OK);
    void *threadResult = NULL;
    CHECK(pthread_join(thread, &threadResult) == 0);
    CHECK(threadResult == NULL);
    CHECK(testPool.pool.iNumQuiesced == 0);
    cleanupWorkerContext(&ctx);
    cleanupPool(&testPool);
    return 0;
}

static int testActiveWorkerAndTimeout(void) {
    test_pool_t testPool;
    worker_ctx_t ctx;
    pthread_t thread;
    initPool(&testPool, 1);
    initWorkerContext(&ctx, &testPool, 1);
    CHECK(pthread_create(&thread, NULL, workerMain, &ctx) == 0);
    waitWorkerReady(&ctx);

    CHECK(wtpRequestQuiesce(&testPool.pool) == RS_RET_OK);
    struct timespec deadline;
    CHECK(deadlineMs(10, &deadline) == 0);
    CHECK(wtpWaitQuiesced(&testPool.pool, &deadline) == RS_RET_TIMED_OUT);

    pthread_mutex_lock(&ctx.gateMutex);
    ctx.releaseWork = 1;
    pthread_cond_signal(&ctx.gateCond);
    pthread_mutex_unlock(&ctx.gateMutex);
    CHECK(deadlineMs(1000, &deadline) == 0);
    CHECK(wtpWaitQuiesced(&testPool.pool, &deadline) == RS_RET_OK);
    CHECK(wtpResume(&testPool.pool) == RS_RET_OK);
    void *threadResult = NULL;
    CHECK(pthread_join(thread, &threadResult) == 0);
    CHECK(threadResult == NULL);
    cleanupWorkerContext(&ctx);
    cleanupPool(&testPool);
    return 0;
}

static int testZeroWorkerAndShutdownPriority(void) {
    test_pool_t testPool;
    initPool(&testPool, 0);
    CHECK(wtpRequestQuiesce(&testPool.pool) == RS_RET_OK);
    struct timespec deadline;
    CHECK(deadlineMs(100, &deadline) == 0);
    CHECK(wtpWaitQuiesced(&testPool.pool, &deadline) == RS_RET_OK);
    setPoolState(&testPool, wtpState_SHUTDOWN_IMMEDIATE);
    CHECK(wtpResume(&testPool.pool) == RS_RET_OK);
    CHECK(poolState(&testPool) == wtpState_SHUTDOWN_IMMEDIATE);
    cleanupPool(&testPool);
    return 0;
}

static int testParkedWorkerShutdownPriority(void) {
    test_pool_t testPool;
    worker_ctx_t ctx;
    pthread_t thread;
    void *threadResult = NULL;
    struct timespec deadline;

    initPool(&testPool, 1);
    initWorkerContext(&ctx, &testPool, 0);
    CHECK(pthread_create(&thread, NULL, workerMain, &ctx) == 0);
    waitWorkerReady(&ctx);
    CHECK(wtpRequestQuiesce(&testPool.pool) == RS_RET_OK);
    CHECK(deadlineMs(1000, &deadline) == 0);
    CHECK(wtpWaitQuiesced(&testPool.pool, &deadline) == RS_RET_OK);

    /* This models TERM winning after the barrier acknowledgement but before
     * release. Resume must wake the parked worker without restoring RUNNING. */
    setPoolState(&testPool, wtpState_SHUTDOWN_IMMEDIATE);
    CHECK(wtpResume(&testPool.pool) == RS_RET_OK);
    CHECK(pthread_join(thread, &threadResult) == 0);
    CHECK(threadResult == NULL);
    CHECK(poolState(&testPool) == wtpState_SHUTDOWN_IMMEDIATE);
    CHECK(testPool.pool.iNumQuiesced == 0);
    cleanupWorkerContext(&ctx);
    cleanupPool(&testPool);
    return 0;
}

static int testQueueDirectAndDA(void) {
    qqueue_batch_barrier_t *barrier = NULL;
    qqueue_t directQueue;
    memset(&directQueue, 0, sizeof(directQueue));
    directQueue.qType = QUEUETYPE_DIRECT;
    CHECK(qqueueBatchBarrierBegin(&directQueue, &barrier) == RS_RET_NOT_IMPLEMENTED);
    CHECK(barrier == NULL);

    test_pool_t parentPool;
    test_pool_t childPool;
    test_pool_t transferPool;
    qqueue_t parentQueue;
    qqueue_t childQueue;
    initPool(&parentPool, 0);
    initPool(&childPool, 0);
    initPool(&transferPool, 0);
    memset(&parentQueue, 0, sizeof(parentQueue));
    memset(&childQueue, 0, sizeof(childQueue));
    parentQueue.qType = QUEUETYPE_LINKEDLIST;
    parentQueue.mut = &parentPool.queueMutex;
    parentQueue.pWtpReg = &parentPool.pool;
    parentQueue.pWtpDA = &transferPool.pool;
    parentQueue.bIsDA = 1;
    parentQueue.pqDA = &childQueue;
    parentQueue.iQueueSize = 3;
    parentQueue.iMinMsgsPerWrkr = 2;
    parentQueue.iHighWtrMrk = 2;
    childQueue.qType = QUEUETYPE_SEGMENTED_DISK;
    childQueue.mut = &childPool.queueMutex;
    childQueue.pWtpReg = &childPool.pool;
    childQueue.iMinMsgsPerWrkr = 1;
    childQueue.tVars.segdisk = (segdisk_store_t *)(uintptr_t)1;
    segmentedDiskMayHaveData = 1;
    adviceCallCount = 0;

    CHECK(qqueueBatchBarrierBegin(&parentQueue, &barrier) == RS_RET_OK);
    CHECK(poolState(&parentPool) == wtpState_QUIESCE);
    CHECK(poolState(&childPool) == wtpState_QUIESCE);
    struct timespec deadline;
    CHECK(deadlineMs(100, &deadline) == 0);
    CHECK(qqueueBatchBarrierWait(barrier, &deadline) == RS_RET_OK);
    CHECK(qqueueBatchBarrierRelease(&barrier) == RS_RET_OK);
    CHECK(barrier == NULL);
    CHECK(poolState(&parentPool) == wtpState_RUNNING);
    CHECK(poolState(&childPool) == wtpState_RUNNING);
    CHECK(adviceCallCount == 3);
    CHECK(advised(&parentPool.pool, 2));
    CHECK(advised(&transferPool.pool, 1));
    CHECK(advised(&childPool.pool, 2));

    segmentedDiskMayHaveData = 0;
    cleanupPool(&transferPool);
    cleanupPool(&childPool);
    cleanupPool(&parentPool);
    return 0;
}

static int testQueueBarrierOwner(void) {
    test_pool_t testPool;
    qqueue_t queue;
    qqueue_batch_barrier_t *barrier = NULL;
    foreign_owner_ctx_t ctx;
    pthread_t thread;
    void *threadResult = NULL;

    initPool(&testPool, 0);
    memset(&queue, 0, sizeof(queue));
    memset(&ctx, 0, sizeof(ctx));
    queue.qType = QUEUETYPE_LINKEDLIST;
    queue.mut = &testPool.queueMutex;
    queue.pWtpReg = &testPool.pool;
    CHECK(qqueueBatchBarrierBegin(&queue, &barrier) == RS_RET_OK);
    ctx.barrier = barrier;
    CHECK(pthread_create(&thread, NULL, foreignOwnerMain, &ctx) == 0);
    CHECK(pthread_join(thread, &threadResult) == 0);
    CHECK(threadResult == NULL);
    CHECK(ctx.waitResult == RS_RET_PARAM_ERROR);
    CHECK(ctx.releaseResult == RS_RET_PARAM_ERROR);
    CHECK(ctx.retainedToken);
    CHECK(barrier != NULL);
    CHECK(qqueueBatchBarrierRelease(&barrier) == RS_RET_OK);
    CHECK(barrier == NULL);
    cleanupPool(&testPool);
    return 0;
}

static int testQueueDADistinctWorkerMutexes(void) {
    test_pool_t parentPool;
    test_pool_t childPool;
    worker_ctx_t parentCtx;
    worker_ctx_t childCtx;
    qqueue_t parentQueue;
    qqueue_t childQueue;
    qqueue_batch_barrier_t *barrier = NULL;
    pthread_t parentThread;
    pthread_t childThread;
    void *threadResult = NULL;
    struct timespec deadline;

    initPool(&parentPool, 1);
    initPool(&childPool, 1);
    initWorkerContext(&parentCtx, &parentPool, 0);
    initWorkerContext(&childCtx, &childPool, 0);
    memset(&parentQueue, 0, sizeof(parentQueue));
    memset(&childQueue, 0, sizeof(childQueue));
    parentQueue.qType = QUEUETYPE_LINKEDLIST;
    parentQueue.mut = &parentPool.queueMutex;
    parentQueue.pWtpReg = &parentPool.pool;
    parentQueue.bIsDA = 1;
    parentQueue.pqDA = &childQueue;
    childQueue.qType = QUEUETYPE_DISK;
    childQueue.mut = &childPool.queueMutex;
    childQueue.pWtpReg = &childPool.pool;
    CHECK(pthread_create(&parentThread, NULL, workerMain, &parentCtx) == 0);
    CHECK(pthread_create(&childThread, NULL, workerMain, &childCtx) == 0);
    waitWorkerReady(&parentCtx);
    waitWorkerReady(&childCtx);

    CHECK(qqueueBatchBarrierBegin(&parentQueue, &barrier) == RS_RET_OK);
    CHECK(deadlineMs(1000, &deadline) == 0);
    CHECK(qqueueBatchBarrierWait(barrier, &deadline) == RS_RET_OK);
    CHECK(qqueueBatchBarrierRelease(&barrier) == RS_RET_OK);
    CHECK(pthread_join(parentThread, &threadResult) == 0);
    CHECK(threadResult == NULL);
    threadResult = NULL;
    CHECK(pthread_join(childThread, &threadResult) == 0);
    CHECK(threadResult == NULL);
    CHECK(parentPool.pool.iNumQuiesced == 0);
    CHECK(childPool.pool.iNumQuiesced == 0);

    cleanupWorkerContext(&childCtx);
    cleanupWorkerContext(&parentCtx);
    cleanupPool(&childPool);
    cleanupPool(&parentPool);
    return 0;
}

static int testQueueBarrierPartialBeginRollback(void) {
    test_pool_t parentPool;
    test_pool_t childPool;
    qqueue_t parentQueue;
    qqueue_t childQueue;
    qqueue_batch_barrier_t *barrier = NULL;

    initPool(&parentPool, 0);
    initPool(&childPool, 0);
    memset(&parentQueue, 0, sizeof(parentQueue));
    memset(&childQueue, 0, sizeof(childQueue));
    parentQueue.qType = QUEUETYPE_LINKEDLIST;
    parentQueue.mut = &parentPool.queueMutex;
    parentQueue.pWtpReg = &parentPool.pool;
    parentQueue.bIsDA = 1;
    parentQueue.pqDA = &childQueue;
    parentQueue.iQueueSize = 1;
    parentQueue.iMinMsgsPerWrkr = 1;
    childQueue.qType = QUEUETYPE_DISK;
    childQueue.mut = &childPool.queueMutex;
    childQueue.pWtpReg = &childPool.pool;
    setPoolState(&childPool, wtpState_SHUTDOWN_IMMEDIATE);
    adviceCallCount = 0;

    CHECK(qqueueBatchBarrierBegin(&parentQueue, &barrier) == RS_RET_NO_RUN);
    CHECK(barrier == NULL);
    CHECK(poolState(&parentPool) == wtpState_RUNNING);
    CHECK(poolState(&childPool) == wtpState_SHUTDOWN_IMMEDIATE);
    CHECK(advised(&parentPool.pool, 2));

    cleanupPool(&childPool);
    cleanupPool(&parentPool);
    return 0;
}


/* The actual production boundary invokes this callback with the queue mutex
 * held. Release it while held on a condition, exactly as deferred disposal is
 * allowed to do. This exercises request-generation revalidation, not a copied
 * queue/store algorithm. Actual source-lease completion is covered by the
 * queue-layer initialized-daemon fixture. */
typedef struct completion_ctx_s {
    test_pool_t *pool;
    pthread_mutex_t gate;
    pthread_cond_t changed;
    int entered;
    int release;
    rsRetVal result;
} completion_ctx_t;

static rsRetVal completionCallback(void *arg, wti_t *worker) {
    completion_ctx_t *ctx = arg;
    (void)worker;
    pthread_mutex_unlock(ctx->pool->pool.pmutUsr);
    pthread_mutex_lock(&ctx->gate);
    ++ctx->entered;
    pthread_cond_broadcast(&ctx->changed);
    while (!ctx->release) pthread_cond_wait(&ctx->changed, &ctx->gate);
    pthread_mutex_unlock(&ctx->gate);
    pthread_mutex_lock(ctx->pool->pool.pmutUsr);
    return ctx->result;
}

static void *completionWorker(void *arg) {
    completion_ctx_t *ctx = arg;
    int saved;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &saved);
    pthread_mutex_lock(ctx->pool->pool.pmutUsr);
    wtpWorkerQuiesceAtBoundary(&ctx->pool->pool, &ctx->pool->worker);
    pthread_mutex_unlock(ctx->pool->pool.pmutUsr);
    pthread_setcancelstate(saved, NULL);
    pthread_testcancel();
    return NULL;
}

static void releaseCompletion(completion_ctx_t *ctx) {
    pthread_mutex_lock(&ctx->gate);
    ctx->release = 1;
    pthread_cond_broadcast(&ctx->changed);
    pthread_mutex_unlock(&ctx->gate);
}

static int completionCase(const int failure, const int replaceGeneration, const int cancel, const int shutdown) {
    test_pool_t pool;
    initPool(&pool, 1);
    completion_ctx_t ctx = {.pool = &pool,
                            .gate = PTHREAD_MUTEX_INITIALIZER,
                            .changed = PTHREAD_COND_INITIALIZER,
                            .result = failure ? RS_RET_IO_ERROR : RS_RET_OK};
    pool.pool.pUsr = &ctx;
    pool.pool.pfObjProcessed = completionCallback;
    pool.worker.batch.nElemDeq = 1; /* preceding owned batch, never erased by the barrier */
    uint64_t first = 0, current = 0;
    struct timespec deadline;
    pthread_t thread;
    CHECK(wtpRequestCompletionQuiesce(&pool.pool, &first) == RS_RET_OK);
    current = first;
    CHECK(pthread_create(&thread, NULL, completionWorker, &ctx) == 0);
    pthread_mutex_lock(&ctx.gate);
    while (!ctx.entered) pthread_cond_wait(&ctx.changed, &ctx.gate);
    pthread_mutex_unlock(&ctx.gate);
    CHECK(deadlineMs(-1000, &deadline) == 0);
    CHECK(wtpWaitCompletionQuiesced(&pool.pool, first, &deadline) == RS_RET_TIMED_OUT);
    CHECK(pool.pool.iNumCompletionQuiesced == 0);
    if (shutdown) {
        /* Shutdown wins while deferred disposal has released the queue mutex.
         * Its return cannot acknowledge the retired request or restore RUNNING. */
        pthread_mutex_lock(&pool.queueMutex);
        setPoolState(&pool, wtpState_SHUTDOWN_IMMEDIATE);
        pthread_mutex_unlock(&pool.queueMutex);
        CHECK(wtpWaitCompletionQuiesced(&pool.pool, current, &deadline) == RS_RET_NO_RUN);
        releaseCompletion(&ctx);
        CHECK(wtpResumeCompletion(&pool.pool, current) == RS_RET_OK);
        CHECK(pthread_join(thread, NULL) == 0);
        CHECK(pool.pool.iNumCompletionQuiesced == 0 && pool.pool.iNumQuiesced == 0);
        CHECK(poolState(&pool) == wtpState_SHUTDOWN_IMMEDIATE);
        pthread_cond_destroy(&ctx.changed);
        pthread_mutex_destroy(&ctx.gate);
        cleanupPool(&pool);
        return 0;
    }
    if (replaceGeneration) {
        CHECK(wtpResumeCompletion(&pool.pool, first) == RS_RET_OK);
        CHECK(wtpRequestCompletionQuiesce(&pool.pool, &current) == RS_RET_OK);
        CHECK(current != first);
        CHECK(wtpWaitCompletionQuiesced(&pool.pool, first, &deadline) == RS_RET_NO_RUN);
    }
    releaseCompletion(&ctx);
    if (replaceGeneration) {
        CHECK(pthread_join(thread, NULL) == 0);
        CHECK(deadlineMs(-1000, &deadline) == 0);
        CHECK(wtpWaitCompletionQuiesced(&pool.pool, current, &deadline) == RS_RET_TIMED_OUT);
        CHECK(pool.pool.iNumCompletionQuiesced == 0); /* no stale acknowledgment */
        CHECK(pthread_create(&thread, NULL, completionWorker, &ctx) == 0);
    }
    CHECK(deadlineMs(5000, &deadline) == 0); /* hang guard, not a completion oracle */
    CHECK(wtpWaitCompletionQuiesced(&pool.pool, current, &deadline) == (failure ? RS_RET_IO_ERROR : RS_RET_OK));
    if (failure) CHECK(wtpWaitCompletionQuiesced(&pool.pool, current, &deadline) == RS_RET_IO_ERROR);
    if (cancel) CHECK(pthread_cancel(thread) == 0);
    if (shutdown) {
        pthread_mutex_lock(&pool.queueMutex);
        setPoolState(&pool, wtpState_SHUTDOWN_IMMEDIATE);
        pthread_cond_signal(&pool.worker.pcondBusy);
        pthread_mutex_unlock(&pool.queueMutex);
        CHECK(wtpWaitCompletionQuiesced(&pool.pool, current, &deadline) == RS_RET_NO_RUN);
    }
    CHECK(wtpResumeCompletion(&pool.pool, current) == RS_RET_OK);
    void *joined = NULL;
    CHECK(pthread_join(thread, &joined) == 0);
    CHECK(joined == (cancel ? PTHREAD_CANCELED : NULL));
    CHECK(pool.pool.iNumCompletionQuiesced == 0 && pool.pool.iNumQuiesced == 0);
    CHECK(poolState(&pool) == (shutdown ? wtpState_SHUTDOWN_IMMEDIATE : wtpState_RUNNING));
    pthread_cond_destroy(&ctx.changed);
    pthread_mutex_destroy(&ctx.gate);
    cleanupPool(&pool);
    return 0;
}

/* An idle WTI has no preceding responsibility. Do not manufacture a store
 * completion or mutate legacy deletion frontiers by calling its callback. */
static int completionEmptyWorker(void) {
    test_pool_t pool;
    initPool(&pool, 1);
    completion_ctx_t ctx = {.pool = &pool,
                            .gate = PTHREAD_MUTEX_INITIALIZER,
                            .changed = PTHREAD_COND_INITIALIZER,
                            .release = 1,
                            .result = RS_RET_IO_ERROR};
    pool.pool.pUsr = &ctx;
    pool.pool.pfObjProcessed = completionCallback;
    uint64_t generation;
    pthread_t thread;
    struct timespec deadline;
    CHECK(wtpRequestCompletionQuiesce(&pool.pool, &generation) == RS_RET_OK);
    CHECK(pthread_create(&thread, NULL, completionWorker, &ctx) == 0);
    CHECK(deadlineMs(5000, &deadline) == 0);
    CHECK(wtpWaitCompletionQuiesced(&pool.pool, generation, &deadline) == RS_RET_OK);
    CHECK(wtpResumeCompletion(&pool.pool, generation) == RS_RET_OK);
    CHECK(pthread_join(thread, NULL) == 0);
    CHECK(ctx.entered == 0);
    pthread_cond_destroy(&ctx.changed);
    pthread_mutex_destroy(&ctx.gate);
    cleanupPool(&pool);
    return 0;
}

/* The opt-in family includes the real queue-composition transfer-pool slot.
 * Ordinary barriers must continue to omit it. Nonzero queued inventory is
 * deliberately left intact: completion quiescence is not an empty proof. */
static int completionDAFamily(void) {
    test_pool_t regular, transfer, child;
    initPool(&regular, 0);
    initPool(&transfer, 0);
    initPool(&child, 0);
    qqueue_t parentQueue = {0}, childQueue = {0};
    parentQueue.qType = QUEUETYPE_LINKEDLIST;
    parentQueue.mut = &regular.queueMutex;
    parentQueue.pWtpReg = &regular.pool;
    parentQueue.pWtpDA = &transfer.pool;
    parentQueue.bIsDA = 1;
    parentQueue.pqDA = &childQueue;
    parentQueue.iQueueSize = 5;
    childQueue.qType = QUEUETYPE_DISK;
    childQueue.mut = &child.queueMutex;
    childQueue.pWtpReg = &child.pool;
    qqueue_batch_barrier_t *barrier = NULL;
    CHECK(qqueueBatchBarrierBegin(&parentQueue, &barrier) == RS_RET_OK);
    CHECK(poolState(&transfer) == wtpState_RUNNING);
    CHECK(qqueueBatchBarrierRelease(&barrier) == RS_RET_OK);
    CHECK(qqueueCompletionBarrierBegin(&parentQueue, &barrier) == RS_RET_OK);
    CHECK(poolState(&regular) == wtpState_QUIESCE && poolState(&transfer) == wtpState_QUIESCE &&
          poolState(&child) == wtpState_QUIESCE);
    struct timespec deadline;
    CHECK(deadlineMs(5000, &deadline) == 0);
    CHECK(qqueueBatchBarrierWait(barrier, &deadline) == RS_RET_OK);
    CHECK(parentQueue.iQueueSize == 5);
    CHECK(qqueueBatchBarrierRelease(&barrier) == RS_RET_OK && barrier == NULL);
    CHECK(poolState(&regular) == wtpState_RUNNING && poolState(&transfer) == wtpState_RUNNING &&
          poolState(&child) == wtpState_RUNNING);
    cleanupPool(&child);
    cleanupPool(&transfer);
    cleanupPool(&regular);
    return 0;
}

/* Three real production pool-boundary callbacks share the DA queue mutex.
 * Releasing regular, transfer and child completion separately must leave the
 * family incomplete until the last acknowledgment. A child error is retained
 * exactly; none of these control callbacks may erase a batch responsibility. */
static int completionDAActiveFamily(const int childFailure) {
    test_pool_t pools[3];
    completion_ctx_t contexts[3];
    pthread_t threads[3];
    for (int i = 0; i < 3; ++i) initPool(&pools[i], 1);
    for (int i = 0; i < 3; ++i) {
        memset(&contexts[i], 0, sizeof(contexts[i]));
        contexts[i].pool = &pools[i];
        contexts[i].result = i == 2 && childFailure ? RS_RET_IO_ERROR : RS_RET_OK;
        pthread_mutex_init(&contexts[i].gate, NULL);
        pthread_cond_init(&contexts[i].changed, NULL);
        pools[i].pool.pmutUsr = &pools[0].queueMutex;
        pools[i].pool.pUsr = &contexts[i];
        pools[i].pool.pfObjProcessed = completionCallback;
        pools[i].worker.batch.nElemDeq = 1;
    }
    qqueue_t parent = {0}, child = {0};
    parent.qType = QUEUETYPE_LINKEDLIST;
    parent.mut = &pools[0].queueMutex;
    parent.pWtpReg = &pools[0].pool;
    parent.pWtpDA = &pools[1].pool;
    parent.bIsDA = 1;
    parent.pqDA = &child;
    child.qType = QUEUETYPE_DISK;
    child.mut = parent.mut;
    child.pWtpReg = &pools[2].pool;
    qqueue_batch_barrier_t *barrier = NULL;
    CHECK(qqueueCompletionBarrierBegin(&parent, &barrier) == RS_RET_OK);
    for (int i = 0; i < 3; ++i) CHECK(pthread_create(&threads[i], NULL, completionWorker, &contexts[i]) == 0);
    for (int i = 0; i < 3; ++i) {
        pthread_mutex_lock(&contexts[i].gate);
        while (!contexts[i].entered) pthread_cond_wait(&contexts[i].changed, &contexts[i].gate);
        pthread_mutex_unlock(&contexts[i].gate);
    }
    struct timespec expired, deadline;
    CHECK(deadlineMs(-1000, &expired) == 0);
    CHECK(qqueueBatchBarrierWait(barrier, &expired) == RS_RET_TIMED_OUT);
    for (int i = 0; i < 3; ++i) {
        releaseCompletion(&contexts[i]);
        CHECK(deadlineMs(5000, &deadline) == 0);
        CHECK(wtpWaitCompletionQuiesced(&pools[i].pool, pools[i].pool.quiesceGeneration, &deadline) ==
              (i == 2 && childFailure ? RS_RET_IO_ERROR : RS_RET_OK));
        if (i != 2) CHECK(qqueueBatchBarrierWait(barrier, &expired) == RS_RET_TIMED_OUT);
    }
    CHECK(qqueueBatchBarrierWait(barrier, &deadline) == (childFailure ? RS_RET_IO_ERROR : RS_RET_OK));
    CHECK(qqueueBatchBarrierRelease(&barrier) == RS_RET_OK);
    for (int i = 0; i < 3; ++i) CHECK(pthread_join(threads[i], NULL) == 0);
    for (int i = 0; i < 3; ++i) {
        CHECK(pools[i].worker.batch.nElemDeq == 1);
        CHECK(pools[i].pool.iNumQuiesced == 0 && pools[i].pool.iNumCompletionQuiesced == 0);
        pthread_cond_destroy(&contexts[i].changed);
        pthread_mutex_destroy(&contexts[i].gate);
        cleanupPool(&pools[i]);
    }
    return 0;
}

int main(void) {
    if (completionCase(0, 0, 0, 0) || completionCase(1, 0, 0, 0) || completionCase(0, 1, 0, 0) ||
        completionCase(0, 0, 1, 0) || completionCase(0, 0, 0, 1) || completionEmptyWorker() || completionDAFamily() ||
        completionDAActiveFamily(0) || completionDAActiveFamily(1))
        return 1;
    if (testIdleWorker() != 0 || testActiveWorkerAndTimeout() != 0 || testZeroWorkerAndShutdownPriority() != 0 ||
        testParkedWorkerShutdownPriority() != 0 || testQueueDirectAndDA() != 0 || testQueueBarrierOwner() != 0 ||
        testQueueDADistinctWorkerMutexes() != 0 || testQueueBarrierPartialBeginRollback() != 0)
        return 1;
    puts("WTP and queue batch barrier tests passed");
    return 0;
}
