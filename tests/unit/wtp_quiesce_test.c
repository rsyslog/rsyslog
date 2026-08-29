/*
 * Unit coverage for the non-destructive WTP/queue batch barrier. Workers are
 * synchronized with conditions rather than sleeps. The oracle is that idle and
 * active workers acknowledge only at a boundary, zero-worker pools complete
 * immediately, timeouts leave the token releasable, direct queues fail closed,
 * both DA consumer pools pause, and shutdown state is never overwritten.
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

/* Keep this synchronization unit independent of the daemon/runtime link
 * closure. The queue barrier calls this only when messages accumulated while
 * a zero-worker pool was paused; the dedicated test queues remain empty. */
rsRetVal wtpAdviseMaxWorkers(wtp_t __attribute__((unused)) * pThis,
                             int __attribute__((unused)) nMaxWrkr,
                             const int __attribute__((unused)) permit_during_shutdown) {
    return RS_RET_OK;
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

static wtpState_t poolState(test_pool_t *const testPool) {
    return (wtpState_t)ATOMIC_LOAD_32BIT((int *)&testPool->pool.wtpState, &testPool->pool.mutWtpState);
}

static void setPoolState(test_pool_t *const testPool, const wtpState_t state) {
    ATOMIC_STORE_32BIT((int *)&testPool->pool.wtpState, &testPool->pool.mutWtpState, state);
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
    wtpWorkerQuiesce(&testPool->pool, &testPool->worker.pcondBusy);
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

    pthread_mutex_lock(&testPool.queueMutex);
    CHECK(wtpRequestQuiesceLocked(&testPool.pool) == RS_RET_OK);
    pthread_mutex_unlock(&testPool.queueMutex);
    struct timespec deadline;
    CHECK(deadlineMs(1000, &deadline) == 0);
    CHECK(wtpWaitQuiesced(&testPool.pool, &deadline) == RS_RET_OK);
    pthread_mutex_lock(&testPool.queueMutex);
    CHECK(wtpResumeLocked(&testPool.pool) == RS_RET_OK);
    pthread_mutex_unlock(&testPool.queueMutex);
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

    pthread_mutex_lock(&testPool.queueMutex);
    CHECK(wtpRequestQuiesceLocked(&testPool.pool) == RS_RET_OK);
    pthread_mutex_unlock(&testPool.queueMutex);
    struct timespec deadline;
    CHECK(deadlineMs(10, &deadline) == 0);
    CHECK(wtpWaitQuiesced(&testPool.pool, &deadline) == RS_RET_TIMED_OUT);

    pthread_mutex_lock(&ctx.gateMutex);
    ctx.releaseWork = 1;
    pthread_cond_signal(&ctx.gateCond);
    pthread_mutex_unlock(&ctx.gateMutex);
    CHECK(deadlineMs(1000, &deadline) == 0);
    CHECK(wtpWaitQuiesced(&testPool.pool, &deadline) == RS_RET_OK);
    pthread_mutex_lock(&testPool.queueMutex);
    CHECK(wtpResumeLocked(&testPool.pool) == RS_RET_OK);
    pthread_mutex_unlock(&testPool.queueMutex);
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
    pthread_mutex_lock(&testPool.queueMutex);
    CHECK(wtpRequestQuiesceLocked(&testPool.pool) == RS_RET_OK);
    pthread_mutex_unlock(&testPool.queueMutex);
    struct timespec deadline;
    CHECK(deadlineMs(100, &deadline) == 0);
    CHECK(wtpWaitQuiesced(&testPool.pool, &deadline) == RS_RET_OK);
    setPoolState(&testPool, wtpState_SHUTDOWN_IMMEDIATE);
    pthread_mutex_lock(&testPool.queueMutex);
    CHECK(wtpResumeLocked(&testPool.pool) == RS_RET_OK);
    pthread_mutex_unlock(&testPool.queueMutex);
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
    pthread_mutex_lock(&testPool.queueMutex);
    CHECK(wtpRequestQuiesceLocked(&testPool.pool) == RS_RET_OK);
    pthread_mutex_unlock(&testPool.queueMutex);
    CHECK(deadlineMs(1000, &deadline) == 0);
    CHECK(wtpWaitQuiesced(&testPool.pool, &deadline) == RS_RET_OK);

    /* This models TERM winning after the barrier acknowledgement but before
     * release. Resume must wake the parked worker without restoring RUNNING. */
    pthread_mutex_lock(&testPool.queueMutex);
    setPoolState(&testPool, wtpState_SHUTDOWN_IMMEDIATE);
    CHECK(wtpResumeLocked(&testPool.pool) == RS_RET_OK);
    pthread_mutex_unlock(&testPool.queueMutex);
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
    qqueue_t parentQueue;
    qqueue_t childQueue;
    initPool(&parentPool, 0);
    initPool(&childPool, 0);
    /* A DA parent and child intentionally share their queue mutex. */
    childPool.pool.pmutUsr = &parentPool.queueMutex;
    memset(&parentQueue, 0, sizeof(parentQueue));
    memset(&childQueue, 0, sizeof(childQueue));
    parentQueue.qType = QUEUETYPE_LINKEDLIST;
    parentQueue.mut = &parentPool.queueMutex;
    parentQueue.pWtpReg = &parentPool.pool;
    parentQueue.bIsDA = 1;
    parentQueue.pqDA = &childQueue;
    childQueue.qType = QUEUETYPE_DISK;
    childQueue.mut = &parentPool.queueMutex;
    childQueue.pWtpReg = &childPool.pool;

    CHECK(qqueueBatchBarrierBegin(&parentQueue, &barrier) == RS_RET_OK);
    CHECK(poolState(&parentPool) == wtpState_QUIESCE);
    CHECK(poolState(&childPool) == wtpState_QUIESCE);
    struct timespec deadline;
    CHECK(deadlineMs(100, &deadline) == 0);
    CHECK(qqueueBatchBarrierWait(barrier, &deadline) == RS_RET_OK);
    qqueueBatchBarrierRelease(&barrier);
    CHECK(barrier == NULL);
    CHECK(poolState(&parentPool) == wtpState_RUNNING);
    CHECK(poolState(&childPool) == wtpState_RUNNING);

    /* childPool owns a separate test mutex which was replaced only as its
     * runtime predicate pointer; cleanup remains responsible for that mutex. */
    cleanupPool(&childPool);
    cleanupPool(&parentPool);
    return 0;
}

int main(void) {
    if (testIdleWorker() != 0 || testActiveWorkerAndTimeout() != 0 || testZeroWorkerAndShutdownPriority() != 0 ||
        testParkedWorkerShutdownPriority() != 0 || testQueueDirectAndDA() != 0)
        return 1;
    puts("WTP and queue batch barrier tests passed");
    return 0;
}
