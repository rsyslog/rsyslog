/* SPDX-License-Identifier: Apache-2.0
 * Exercise the tcpsrv live-reload fence without sockets or a daemon. The
 * oracle drives the production Request/Wait/Release and event-loop/worker
 * safepoints directly: single-worker acquisition, multi-worker timeout with
 * automatic abort/drain followed by a fresh generation, and TERM while all
 * participants are parked. Bounded absolute deadlines prevent a broken fence
 * from hanging the unit test.
 * A receive-worker handoff is held until markers are queued; the production
 * append/pop helpers must process normal descriptors in FIFO order before
 * all worker acknowledgments and WaitFence success. Fault-injected sync-init
 * failures prove partial-init unwind without touching an unstarted work queue.
 * Repeated ready handoffs keep markers pending until the supplied deadline;
 * timeout must clear ordering, drain residual markers and permit a fresh fence
 * after the producer yields. No elapsed-time performance threshold is used.
 * Poll capacity checks call the production reservation helper at exact and
 * growth boundaries, writing the control descriptor and trailing sentinel.
 * Nonzero unnamed limiter budget/loss state survives unrelated profile
 * updates; only changed interval/burst resets it, never named policy state.
 * Listener table tests drive production allocation/stage/swap/destruction,
 * injecting each allocation failure and proving retained object identity,
 * descriptor rebinding, safe shrink and rejection of every occupied tail kind.
 */
#include "config.h"

#include <stdatomic.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "rsyslog.h"
#include "ruleset.h"
#include "ratelimit.h"
#include "tcpsrv.h"
#include "netstrm.h"
#include "../../runtime/tcpsrv-poll.h"

/* Fault injection stays in this translation unit; production calls pthread
 * directly. Observe unwind without ever touching an uninitialized primitive. */
static int failMutexInit, failCondInit, mutexDestroys;
static int fenceTestMutexInit(pthread_mutex_t *mut, const pthread_mutexattr_t *attr) {
    return failMutexInit ? EAGAIN : pthread_mutex_init(mut, attr);
}
static int fenceTestCondInit(pthread_cond_t *cond, const pthread_condattr_t *attr) {
    return failCondInit ? EAGAIN : pthread_cond_init(cond, attr);
}
static int fenceTestMutexDestroy(pthread_mutex_t *mut) {
    ++mutexDestroys;
    return pthread_mutex_destroy(mut);
}
#define pthread_mutex_init fenceTestMutexInit
#define pthread_cond_init fenceTestCondInit
#define pthread_mutex_destroy fenceTestMutexDestroy
static ratelimit_t *rateLimitCalls[4];
static unsigned rateLimitIntervals[4];
static unsigned rateLimitBursts[4];
static size_t rateLimitCallCount;

void ratelimitSetLinuxLike(ratelimit_t *const limiter, const unsigned interval, const unsigned burst) {
    /* Model the setter's reset effect so an unnecessary production-helper
     * call destroys visible nonzero state, rather than only recording a call. */
    limiter->done = limiter->missed = 0;
    limiter->begin = 0;
    if (rateLimitCallCount < sizeof(rateLimitCalls) / sizeof(rateLimitCalls[0])) {
        rateLimitCalls[rateLimitCallCount] = limiter;
        rateLimitIntervals[rateLimitCallCount] = interval;
        rateLimitBursts[rateLimitCallCount] = burst;
    }
    ++rateLimitCallCount;
}

/* Include the small production state machine so the unit can drive its
 * event-loop and worker safepoints without constructing network objects. */
static int listenerAllocCall;
static int listenerAllocFailAt;
static int listenerArraysOwned;
static void *listenerTestCalloc(size_t count, size_t size) {
    if (++listenerAllocCall == listenerAllocFailAt) return NULL;
    void *result = calloc(count, size);
    if (result != NULL) ++listenerArraysOwned;
    return result;
}
static void listenerTestFree(void *pointer) {
    if (pointer != NULL) --listenerArraysOwned;
    free(pointer);
}
#define calloc listenerTestCalloc
#define free listenerTestFree
#include "../../runtime/tcpsrv-fence.c"
#undef pthread_mutex_init
#undef pthread_cond_init
#undef pthread_mutex_destroy
#undef free
#undef calloc

#define CHECK(condition)                                                                    \
    do {                                                                                    \
        if (!(condition)) {                                                                 \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            return 1;                                                                       \
        }                                                                                   \
    } while (0)

static atomic_int terminated;
static atomic_int threadFailure;
static atomic_int terminateOnOwnerCheck;
static pthread_t ownerThread;

#define CHECK_THREAD(condition)                                                                    \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "thread CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            atomic_store_explicit(&threadFailure, 1, memory_order_relaxed);                        \
            return NULL;                                                                           \
        }                                                                                          \
    } while (0)

static int getTermState(void) {
    if (atomic_load_explicit(&terminateOnOwnerCheck, memory_order_acquire) &&
        pthread_equal(pthread_self(), ownerThread)) {
        atomic_store_explicit(&terminated, 1, memory_order_relaxed);
    }
    return atomic_load_explicit(&terminated, memory_order_relaxed);
}

int tcpsrvFenceTerminated(void) {
    return getTermState();
}

static struct timespec deadlineAfterMs(const long milliseconds) {
    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_nsec += (milliseconds % 1000) * 1000000L;
    deadline.tv_sec += milliseconds / 1000 + deadline.tv_nsec / 1000000000L;
    deadline.tv_nsec %= 1000000000L;
    return deadline;
}

static int initServer(tcpsrv_t *const server, const unsigned workers) {
    memset(server, 0, sizeof(*server));
    if (tcpsrvInitFenceSync(server) != RS_RET_OK) return 0;
    pthread_mutex_init(&server->workQueue.mut, NULL);
    pthread_cond_init(&server->workQueue.workRdy, NULL);
    server->fenceSyncInitialized = 1;
    server->fenceReady = 1;
    server->workQueue.numWrkr = workers;
    if (pipe(server->controlPipe) != 0) return 0;
    if (fcntl(server->controlPipe[0], F_SETFL, O_NONBLOCK) != 0 ||
        fcntl(server->controlPipe[1], F_SETFL, O_NONBLOCK) != 0)
        return 0;
    if (workers > 1) {
        server->fenceItems = calloc(workers, sizeof(*server->fenceItems));
        if (server->fenceItems == NULL) return 0;
        for (unsigned i = 0; i < workers; ++i) {
            server->fenceItems[i].pSrv = server;
            server->fenceItems[i].ptrType = NSD_PTR_TYPE_FENCE;
        }
    }
    return 1;
}

static void destroyServer(tcpsrv_t *const server) {
    close(server->controlPipe[0]);
    close(server->controlPipe[1]);
    free(server->fenceItems);
    pthread_cond_destroy(&server->workQueue.workRdy);
    pthread_mutex_destroy(&server->workQueue.mut);
    pthread_cond_destroy(&server->fenceCond);
    pthread_mutex_destroy(&server->fenceMut);
}

static void *eventLoopOnce(void *const arg) {
    tcpsrv_t *const server = arg;
    struct pollfd ready = {.fd = server->controlPipe[0], .events = POLLIN};
    char byte;
    CHECK_THREAD(poll(&ready, 1, 1000) == 1 && (ready.revents & POLLIN) != 0);
    CHECK_THREAD(read(server->controlPipe[0], &byte, 1) == 1);
    tcpsrvActivateFence(server);
    return NULL;
}

static void *workerOnce(void *const arg) {
    tcpsrv_t *const server = arg;
    tcpsrv_io_descr_t *item;
    pthread_mutex_lock(&server->workQueue.mut);
    const struct timespec deadline = deadlineAfterMs(1000);
    while (server->workQueue.head == NULL) {
        const int ret = pthread_cond_timedwait(&server->workQueue.workRdy, &server->workQueue.mut, &deadline);
        if (ret != 0) {
            pthread_mutex_unlock(&server->workQueue.mut);
            CHECK_THREAD(ret == 0);
        }
    }
    item = tcpsrvPopWorkLocked(server);
    pthread_mutex_unlock(&server->workQueue.mut);
    CHECK_THREAD(item->ptrType == NSD_PTR_TYPE_FENCE);
    tcpsrvParkAtFence(server);
    return NULL;
}

static int waitUntilIdle(tcpsrv_t *const server) {
    const struct timespec deadline = deadlineAfterMs(1000);
    pthread_mutex_lock(&server->fenceMut);
    while (server->fenceActive || server->fenceOwnerValid) {
        const int ret = pthread_cond_timedwait(&server->fenceCond, &server->fenceMut, &deadline);
        if (ret == ETIMEDOUT) {
            pthread_mutex_unlock(&server->fenceMut);
            return 0;
        }
    }
    pthread_mutex_unlock(&server->fenceMut);
    return 1;
}

static int waitUntilActivated(tcpsrv_t *const server, const unsigned outstanding) {
    const struct timespec deadline = deadlineAfterMs(1000);
    pthread_mutex_lock(&server->fenceMut);
    while (!server->fenceActive || !server->fenceEventLoopParked || server->fenceOutstanding != outstanding) {
        const int ret = pthread_cond_timedwait(&server->fenceCond, &server->fenceMut, &deadline);
        if (ret == ETIMEDOUT) {
            pthread_mutex_unlock(&server->fenceMut);
            return 0;
        }
    }
    pthread_mutex_unlock(&server->fenceMut);
    return 1;
}

static void drainWakeups(tcpsrv_t *const server) {
    char byte;
    while (read(server->controlPipe[0], &byte, 1) == 1) {
    }
}

struct handoffContext {
    tcpsrv_t *server;
    tcpsrv_io_descr_t *first, *second, *handoff;
    atomic_int processed;
};

static void *returningWorker(void *const arg) {
    struct handoffContext *const ctx = arg;
    tcpsrv_t *const server = ctx->server;
    /* Hold the receive worker's handoff until activation orders markers. */
    CHECK_THREAD(waitUntilActivated(server, 2));
    pthread_mutex_lock(&server->workQueue.mut);
    tcpsrvAppendWorkLocked(server, ctx->handoff);
    int fifo = tcpsrvPopWorkLocked(server) == ctx->first;
    fifo = fifo && tcpsrvPopWorkLocked(server) == ctx->second;
    fifo = fifo && tcpsrvPopWorkLocked(server) == ctx->handoff;
    /* The production FIFO order is the descriptor-processing oracle. No
     * marker may acknowledge before all three ready descriptors are handled. */
    atomic_store_explicit(&ctx->processed, fifo, memory_order_release);
    tcpsrv_io_descr_t *const marker = tcpsrvPopWorkLocked(server);
    pthread_mutex_unlock(&server->workQueue.mut);
    CHECK_THREAD(fifo && marker == &server->fenceItems[0]);
    tcpsrvParkAtFence(server);
    return NULL;
}

static int singleWorkerRoundTrip(void) {
    tcpsrv_t server;
    pthread_t eventLoop;
    uint64_t token;
    struct timespec deadline;
    int releaseDrained;
    CHECK(initServer(&server, 1));
    CHECK(pthread_create(&eventLoop, NULL, eventLoopOnce, &server) == 0);
    CHECK(tcpsrvRequestFence(&server, &token) == RS_RET_OK);
    deadline = deadlineAfterMs(1000);
    CHECK(tcpsrvWaitFence(&server, token, &deadline) == RS_RET_OK);
    CHECK(tcpsrvReleaseFence(&server, token) == RS_RET_OK);
    pthread_mutex_lock(&server.fenceMut);
    releaseDrained = !server.fenceActive && !server.fenceOwnerValid;
    pthread_mutex_unlock(&server.fenceMut);
    CHECK(releaseDrained);
    CHECK(pthread_join(eventLoop, NULL) == 0);
    CHECK(!atomic_load_explicit(&threadFailure, memory_order_relaxed));
    CHECK(waitUntilIdle(&server));
    drainWakeups(&server);
    destroyServer(&server);
    return 0;
}

static int syncInitFailures(void) {
    tcpsrv_t server = {0};
    const int destroysBefore = mutexDestroys;
    failMutexInit = 1;
    CHECK(tcpsrvInitFenceSync(&server) == RS_RET_ERR);
    CHECK(!server.fenceSyncInitialized && mutexDestroys == destroysBefore);
    failMutexInit = 0;
    failCondInit = 1;
    CHECK(tcpsrvInitFenceSync(&server) == RS_RET_ERR);
    CHECK(!server.fenceSyncInitialized && mutexDestroys == destroysBefore + 1);
    failCondInit = 0;
    CHECK(tcpsrvInitFenceSync(&server) == RS_RET_OK);
    /* Even a configured multiworker server can be aborted before its worker
     * queue mutex is initialized: only an active fence may acquire it. */
    server.workQueue.numWrkr = 2;
    pthread_mutex_lock(&server.fenceMut);
    tcpsrvAbortFenceLocked(&server);
    pthread_mutex_unlock(&server.fenceMut);
    pthread_cond_destroy(&server.fenceCond);
    pthread_mutex_destroy(&server.fenceMut);
    return 0;
}

/* The caller holds a ready descriptor on behalf of a receive worker until
 * activation has queued both markers. This is precisely the starvation
 * handoff ordering that previously left normal work behind parked workers.
 * Drive the real append/pop primitives, not a model of the linked list. */
static int handoffBeforeFence(void) {
    tcpsrv_t server;
    pthread_t eventLoop;
    pthread_t workers[2];
    tcpsrv_io_descr_t first = {0}, second = {0}, handoff = {0};
    struct handoffContext ctx = {.server = &server, .first = &first, .second = &second, .handoff = &handoff};
    atomic_init(&ctx.processed, 0);
    uint64_t token;
    CHECK(initServer(&server, 2));
    pthread_mutex_lock(&server.workQueue.mut);
    tcpsrvAppendWorkLocked(&server, &first);
    tcpsrvAppendWorkLocked(&server, &second);
    int fifo = tcpsrvPopWorkLocked(&server) == &first && tcpsrvPopWorkLocked(&server) == &second;
    pthread_mutex_unlock(&server.workQueue.mut);
    CHECK(fifo);
    pthread_mutex_lock(&server.workQueue.mut);
    tcpsrvAppendWorkLocked(&server, &first);
    tcpsrvAppendWorkLocked(&server, &second);
    pthread_mutex_unlock(&server.workQueue.mut);
    CHECK(pthread_create(&eventLoop, NULL, eventLoopOnce, &server) == 0);
    CHECK(pthread_create(&workers[0], NULL, returningWorker, &ctx) == 0);
    CHECK(tcpsrvRequestFence(&server, &token) == RS_RET_OK);
    CHECK(waitUntilActivated(&server, 1));
    CHECK(atomic_load_explicit(&ctx.processed, memory_order_acquire));
    CHECK(pthread_create(&workers[1], NULL, workerOnce, &server) == 0);
    const struct timespec deadline = deadlineAfterMs(1000);
    CHECK(tcpsrvWaitFence(&server, token, &deadline) == RS_RET_OK);
    CHECK(atomic_load_explicit(&ctx.processed, memory_order_acquire));
    CHECK(tcpsrvReleaseFence(&server, token) == RS_RET_OK);
    for (size_t i = 0; i < 2; ++i) CHECK(pthread_join(workers[i], NULL) == 0);
    CHECK(pthread_join(eventLoop, NULL) == 0);
    CHECK(!atomic_load_explicit(&threadFailure, memory_order_relaxed));
    destroyServer(&server);
    return 0;
}

static int timeoutDrainAndRetry(void) {
    tcpsrv_t server;
    pthread_t eventLoop;
    pthread_t workers[2];
    uint64_t token;
    struct timespec deadline;
    CHECK(initServer(&server, 2));
    CHECK(pthread_create(&eventLoop, NULL, eventLoopOnce, &server) == 0);
    CHECK(tcpsrvRequestFence(&server, &token) == RS_RET_OK);
    CHECK(waitUntilActivated(&server, 2));
    /* Model one continuously ready receive descriptor returning repeatedly.
     * Both markers remain pending on every turn, including the empty-normal
     * boundary. This deliberately exhausts availability, not a time budget. */
    tcpsrv_io_descr_t repeated = {0};
    unsigned handoffs = 0;
    pthread_mutex_lock(&server.workQueue.mut);
    while (handoffs < 8) {
        tcpsrvAppendWorkLocked(&server, &repeated);
        if (tcpsrvPopWorkLocked(&server) != &repeated || server.workQueue.head != &server.fenceItems[0]) break;
        ++handoffs;
    }
    pthread_mutex_unlock(&server.workQueue.mut);
    CHECK(handoffs == 8);
    deadline = deadlineAfterMs(20);
    CHECK(tcpsrvWaitFence(&server, token, &deadline) == RS_RET_TIMEOUT);
    /* Abort turns off priority so a continuous sender cannot starve the
     * remaining markers and prevent a subsequent generation forever. */
    tcpsrv_io_descr_t afterAbort = {0};
    pthread_mutex_lock(&server.workQueue.mut);
    tcpsrvAppendWorkLocked(&server, &afterAbort);
    const int abortOrdering =
        !server.fenceOrdering && server.workQueue.tail == &afterAbort && server.workQueue.head == &server.fenceItems[0];
    pthread_mutex_unlock(&server.workQueue.mut);
    CHECK(abortOrdering);
    CHECK(tcpsrvReleaseFence(&server, token) == RS_RET_PARAM_ERROR);
    CHECK(tcpsrvRequestFence(&server, &token) == RS_RET_NOT_IMPLEMENTED);
    for (size_t i = 0; i < 2; ++i) CHECK(pthread_create(&workers[i], NULL, workerOnce, &server) == 0);
    for (size_t i = 0; i < 2; ++i) CHECK(pthread_join(workers[i], NULL) == 0);
    CHECK(pthread_join(eventLoop, NULL) == 0);
    CHECK(!atomic_load_explicit(&threadFailure, memory_order_relaxed));
    CHECK(waitUntilIdle(&server));
    pthread_mutex_lock(&server.workQueue.mut);
    const int drained = tcpsrvPopWorkLocked(&server) == &afterAbort;
    pthread_mutex_unlock(&server.workQueue.mut);
    CHECK(drained);
    drainWakeups(&server);

    CHECK(pthread_create(&eventLoop, NULL, eventLoopOnce, &server) == 0);
    for (size_t i = 0; i < 2; ++i) CHECK(pthread_create(&workers[i], NULL, workerOnce, &server) == 0);
    CHECK(tcpsrvRequestFence(&server, &token) == RS_RET_OK);
    deadline = deadlineAfterMs(1000);
    CHECK(tcpsrvWaitFence(&server, token, &deadline) == RS_RET_OK);
    CHECK(tcpsrvReleaseFence(&server, token) == RS_RET_OK);
    for (size_t i = 0; i < 2; ++i) CHECK(pthread_join(workers[i], NULL) == 0);
    CHECK(pthread_join(eventLoop, NULL) == 0);
    CHECK(!atomic_load_explicit(&threadFailure, memory_order_relaxed));
    CHECK(waitUntilIdle(&server));
    destroyServer(&server);
    return 0;
}

static int termWhileParked(void) {
    tcpsrv_t server;
    pthread_t eventLoop;
    uint64_t token;
    struct timespec deadline;
    CHECK(initServer(&server, 1));
    CHECK(pthread_create(&eventLoop, NULL, eventLoopOnce, &server) == 0);
    CHECK(tcpsrvRequestFence(&server, &token) == RS_RET_OK);
    CHECK(waitUntilActivated(&server, 0));
    ownerThread = pthread_self();
    atomic_store_explicit(&terminateOnOwnerCheck, 1, memory_order_release);
    deadline = deadlineAfterMs(1000);
    CHECK(tcpsrvWaitFence(&server, token, &deadline) == RS_RET_FORCE_TERM);
    CHECK(pthread_join(eventLoop, NULL) == 0);
    CHECK(!atomic_load_explicit(&threadFailure, memory_order_relaxed));
    CHECK(waitUntilIdle(&server));
    atomic_store_explicit(&terminateOnOwnerCheck, 0, memory_order_relaxed);
    atomic_store_explicit(&terminated, 0, memory_order_relaxed);
    destroyServer(&server);
    return 0;
}

static int flowControlSnapshot(void) {
    tcpsrv_t server = {0};
    tcps_sess_t first = {0};
    tcps_sess_t second = {0};
    tcps_sess_t *sessions[] = {&first, NULL, &second};
    server.iSessMax = 3;
    server.pSessions = sessions;
    server.bUseFlowControl = 1;
    first.bUseFlowControl = 1;
    second.bUseFlowControl = 1;
    tcpsrvApplyFlowControlLive(&server, 0);
    CHECK(server.bUseFlowControl == 0);
    CHECK(first.bUseFlowControl == 0);
    CHECK(second.bUseFlowControl == 0);
    return 0;
}

static int starvationMaxReadsSnapshot(void) {
    tcpsrv_t server = {.starvationMaxReads = 500};
    tcpsrvApplyStarvationMaxReadsLive(&server, 1);
    CHECK(server.starvationMaxReads == 1);
    tcpsrvApplyStarvationMaxReadsLive(&server, 0);
    CHECK(server.starvationMaxReads == 0);
    return 0;
}

/* The production helper must retain spent budget and pending loss reports
 * when an unrelated profile update reapplies the same effective limits.
 * Changed interval and changed burst each reset unnamed buckets; named
 * policy objects remain untouched because their ownership is separate. */
static int rateLimitSnapshot(void) {
    tcpsrv_t server = {.ratelimitInterval = 60, .ratelimitBurst = 12000};
    tcpLstnParams_t firstParams = {0};
    tcpLstnParams_t namedParams = {.pszRatelimitName = UCHAR_CONSTANT("shared-policy")};
    tcpLstnParams_t thirdParams = {0};
    ratelimit_t firstLimiter = {.done = 11, .missed = 7, .begin = 123};
    ratelimit_t namedLimiter = {.done = 9, .missed = 2, .begin = 789};
    ratelimit_t thirdLimiter = {.done = 17, .missed = 8, .begin = 456};
    tcpLstnPortList_t thirdListener = {.cnf_params = &thirdParams, .ratelimiter = &thirdLimiter};
    tcpLstnPortList_t namedListener = {
        .cnf_params = &namedParams, .ratelimiter = &namedLimiter, .pNext = &thirdListener};
    tcpLstnPortList_t firstListener = {
        .cnf_params = &firstParams, .ratelimiter = &firstLimiter, .pNext = &namedListener};
    server.pLstnPorts = &firstListener;
    rateLimitCallCount = 0;
    tcpsrvApplyStarvationMaxReadsLive(&server, 1);
    tcpsrvApplyRateLimitLive(&server, 60, 12000);
    CHECK(server.ratelimitInterval == 60);
    CHECK(server.ratelimitBurst == 12000);
    CHECK(rateLimitCallCount == 0);
    CHECK(firstLimiter.done == 11 && firstLimiter.missed == 7 && firstLimiter.begin == 123);
    CHECK(thirdLimiter.done == 17 && thirdLimiter.missed == 8 && thirdLimiter.begin == 456);
    CHECK(namedLimiter.done == 9 && namedLimiter.missed == 2 && namedLimiter.begin == 789);

    tcpsrvApplyRateLimitLive(&server, 90, 12000);
    CHECK(server.ratelimitInterval == 90 && server.ratelimitBurst == 12000);
    CHECK(rateLimitCallCount == 2);
    CHECK(rateLimitCalls[0] == &firstLimiter);
    CHECK(rateLimitCalls[1] == &thirdLimiter);
    CHECK(rateLimitIntervals[0] == 90 && rateLimitIntervals[1] == 90);
    CHECK(rateLimitBursts[0] == 12000 && rateLimitBursts[1] == 12000);
    CHECK(firstLimiter.done == 0 && firstLimiter.missed == 0 && firstLimiter.begin == 0);
    CHECK(thirdLimiter.done == 0 && thirdLimiter.missed == 0 && thirdLimiter.begin == 0);
    firstLimiter.done = thirdLimiter.done = 5;
    firstLimiter.missed = thirdLimiter.missed = 3;
    firstLimiter.begin = thirdLimiter.begin = 456;
    tcpsrvApplyRateLimitLive(&server, 90, 11000);
    CHECK(server.ratelimitInterval == 90 && server.ratelimitBurst == 11000);
    CHECK(rateLimitCallCount == 4);
    CHECK(rateLimitIntervals[2] == 90 && rateLimitIntervals[3] == 90);
    CHECK(rateLimitBursts[2] == 11000 && rateLimitBursts[3] == 11000);
    CHECK(firstLimiter.done == 0 && firstLimiter.missed == 0 && firstLimiter.begin == 0);
    CHECK(thirdLimiter.done == 0 && thirdLimiter.missed == 0 && thirdLimiter.begin == 0);
    CHECK(namedLimiter.done == 9 && namedLimiter.missed == 2 && namedLimiter.begin == 789);
    return 0;
}

static int notificationSnapshot(void) {
    tcpsrv_t server = {.bEmitMsgOnOpen = 0, .bEmitMsgOnClose = 1};
    tcpsrvApplyNotificationsLive(&server, 1, 0);
    CHECK(server.bEmitMsgOnOpen == 1);
    CHECK(server.bEmitMsgOnClose == 0);
    return 0;
}

static int preserveCaseNewSessions(void) {
    tcpsrv_t server = {.bPreserveCase = 1};
    tcpLstnParams_t firstParams = {.bPreserveCase = 1};
    tcpLstnParams_t secondParams = {.bPreserveCase = 1};
    tcpLstnPortList_t secondListener = {.cnf_params = &secondParams};
    tcpLstnPortList_t firstListener = {.cnf_params = &firstParams, .pNext = &secondListener};
    server.pLstnPorts = &firstListener;
    tcpsrvApplyPreserveCaseForNewSessions(&server, 0);
    CHECK(server.bPreserveCase == 0);
    CHECK(firstParams.bPreserveCase == 0);
    CHECK(secondParams.bPreserveCase == 0);
    return 0;
}

static int keepAliveNewSessions(void) {
    tcpsrv_t server = {0};
    tcpsrvApplyKeepAliveForNewSessions(&server, 1, 2, 3, 30);
    CHECK(server.bUseKeepAlive == 1);
    CHECK(server.iKeepAliveIntvl == 2);
    CHECK(server.iKeepAliveProbes == 3);
    CHECK(server.iKeepAliveTime == 30);
    return 0;
}

/* Framing fields are copied when a session is accepted. The control-path
 * helper must update the server profile and the listener-owned Cisco framing
 * flag without mutating already established sessions. */
static int framingNewSessions(void) {
    tcpsrv_t server = {.bSPFramingFix = 0,
                       .addtlFrameDelim = -1,
                       .maxFrameSize = 200000,
                       .bDisableLFDelim = 0,
                       .discardTruncatedMsg = 0};
    tcpLstnParams_t firstParams = {.bSPFramingFix = 0};
    tcpLstnParams_t secondParams = {.bSPFramingFix = 0};
    tcpLstnPortList_t secondListener = {.cnf_params = &secondParams};
    tcpLstnPortList_t firstListener = {.cnf_params = &firstParams, .pNext = &secondListener};
    server.pLstnPorts = &firstListener;
    tcpsrvApplyFramingForNewSessions(&server, 1, 10, 210000, 1, 1);
    CHECK(server.bSPFramingFix == 1);
    CHECK(server.addtlFrameDelim == 10);
    CHECK(server.maxFrameSize == 210000);
    CHECK(server.bDisableLFDelim == 1);
    CHECK(server.discardTruncatedMsg == 1);
    CHECK(firstParams.bSPFramingFix == 1);
    CHECK(secondParams.bSPFramingFix == 1);
    tcpsrvApplyOctetCountedFramingForNewSessions(&server, 0);
    CHECK(firstParams.bSuppOctetFram == 0);
    CHECK(secondParams.bSuppOctetFram == 0);
    tcpsrvApplyMultiLineForNewSessions(&server, 1);
    CHECK(firstParams.bMultiLine == 1);
    CHECK(secondParams.bMultiLine == 1);
    return 0;
}

/* Compression state is copied from the listener into a new session. The
 * helper updates the complete server/listener profile while existing sessions
 * remain untouched. */
static int compressionNewSessions(void) {
    tcpsrv_t server = {0};
    tcpLstnPortList_t secondListener = {0};
    tcpLstnPortList_t firstListener = {.pNext = &secondListener};
    server.pLstnPorts = &firstListener;
    tcpsrvApplyCompressionForNewSessions(&server, TCPSRV_COMPRESS_STREAM_ALWAYS, TCPSRV_COMPRESS_DRIVER_ZLIB, 2048,
                                         1024 * 1024, 2 * 1024 * 1024);
    CHECK(server.compressionMode == TCPSRV_COMPRESS_STREAM_ALWAYS);
    CHECK(server.compressionDriver == TCPSRV_COMPRESS_DRIVER_ZLIB);
    CHECK(server.compressionMaxExpansionRatio == 2048);
    CHECK(server.compressionMaxDecompressedBytesPerReceive == 1024 * 1024);
    CHECK(server.compressionMaxTotalZstdWindowBytes == 2 * 1024 * 1024);
    CHECK(firstListener.compressionMode == server.compressionMode);
    CHECK(firstListener.compressionDriver == server.compressionDriver);
    CHECK(firstListener.compressionMaxExpansionRatio == server.compressionMaxExpansionRatio);
    CHECK(firstListener.compressionMaxDecompressedBytesPerReceive == server.compressionMaxDecompressedBytesPerReceive);
    CHECK(firstListener.compressionMaxTotalZstdWindowBytes == server.compressionMaxTotalZstdWindowBytes);
    CHECK(secondListener.compressionMode == server.compressionMode);
    CHECK(secondListener.compressionDriver == server.compressionDriver);
    CHECK(secondListener.compressionMaxExpansionRatio == server.compressionMaxExpansionRatio);
    CHECK(secondListener.compressionMaxDecompressedBytesPerReceive == server.compressionMaxDecompressedBytesPerReceive);
    CHECK(secondListener.compressionMaxTotalZstdWindowBytes == server.compressionMaxTotalZstdWindowBytes);
    return 0;
}

static int defaultTZSnapshot(void) {
    tcpsrv_t server = {0};
    tcps_sess_t first = {0};
    tcps_sess_t second = {0};
    tcps_sess_t *sessions[] = {&first, NULL, &second};
    tcpLstnParams_t firstParams = {0};
    tcpLstnParams_t secondParams = {0};
    tcpLstnPortList_t secondListener = {.cnf_params = &secondParams};
    tcpLstnPortList_t firstListener = {.cnf_params = &firstParams, .pNext = &secondListener};
    server.iSessMax = 3;
    server.pSessions = sessions;
    server.pLstnPorts = &firstListener;
    tcpsrvApplyDefaultTZLive(&server, UCHAR_CONSTANT("+02:00"));
    CHECK(!strcmp((const char *)server.dfltTZ, "+02:00"));
    CHECK(!strcmp((const char *)firstParams.dfltTZ, "+02:00"));
    CHECK(!strcmp((const char *)secondParams.dfltTZ, "+02:00"));
    CHECK(!strcmp((const char *)first.dfltTZ, "+02:00"));
    CHECK(!strcmp((const char *)second.dfltTZ, "+02:00"));
    tcpsrvApplyDefaultTZLive(&server, NULL);
    CHECK(server.dfltTZ[0] == '\0');
    CHECK(firstParams.dfltTZ[0] == '\0');
    CHECK(secondParams.dfltTZ[0] == '\0');
    CHECK(first.dfltTZ[0] == '\0');
    CHECK(second.dfltTZ[0] == '\0');
    return 0;
}

static int rulesetSnapshot(void) {
    tcpsrv_t server = {0};
    tcps_sess_t first = {0};
    tcps_sess_t second = {0};
    tcps_sess_t *sessions[] = {&first, NULL, &second};
    tcpLstnParams_t firstParams = {0};
    tcpLstnParams_t secondParams = {0};
    tcpLstnPortList_t secondListener = {.cnf_params = &secondParams};
    tcpLstnPortList_t firstListener = {.cnf_params = &firstParams, .pNext = &secondListener};
    ruleset_t target = {0};
    server.iSessMax = 3;
    server.pSessions = sessions;
    server.pLstnPorts = &firstListener;
    tcpsrvApplyRulesetLive(&server, &target);
    CHECK(firstParams.pRuleset == &target);
    CHECK(secondParams.pRuleset == &target);
    CHECK(first.pRuleset == &target);
    CHECK(second.pRuleset == &target);
    return 0;
}

static int pollCapacity(void) {
    struct pollfd *fds = NULL;
    uint32_t capacity = 0;
    CHECK(tcpsrvPollReserve(&fds, &capacity, 0, 1) == RS_RET_OK);
    CHECK(capacity == 1024);
    fds[0].fd = 42;
    /* 1,023 data descriptors plus control exactly fill the logical capacity;
     * the sentinel remains writable without requiring a spare logical slot. */
    CHECK(tcpsrvPollReserve(&fds, &capacity, 1023, 1) == RS_RET_OK);
    CHECK(capacity == 1024);
    fds[1023].fd = 43;
    /* Volatile accesses preserve the sanitizer boundary oracle even when
     * ordinary sentinel stores could be eliminated as dead writes. */
    ((volatile struct pollfd *)fds)[1024].fd = 0;
    CHECK(((volatile struct pollfd *)fds)[1024].fd == 0);
    CHECK(tcpsrvPollReserve(&fds, &capacity, 1024, 1) == RS_RET_OK);
    CHECK(capacity == 2048 && fds[0].fd == 42 && fds[1023].fd == 43);
    CHECK(tcpsrvPollReserve(&fds, &capacity, 2047, 1) == RS_RET_OK);
    CHECK(capacity == 2048);
    fds[2047].fd = 44;
    ((volatile struct pollfd *)fds)[2048].fd = 0;
    CHECK(((volatile struct pollfd *)fds)[2048].fd == 0);
    CHECK(tcpsrvPollReserve(&fds, &capacity, 2048, 1) == RS_RET_OK);
    CHECK(capacity == 3072 && fds[2047].fd == 44);
    ((volatile struct pollfd *)fds)[3072].fd = 0;
    CHECK(((volatile struct pollfd *)fds)[3072].fd == 0);
    struct pollfd *const retained = fds;
    CHECK(tcpsrvPollReserve(&fds, &capacity, UINT32_MAX, 1) == RS_RET_OUT_OF_MEMORY);
    CHECK(fds == retained && capacity == 3072 && fds[0].fd == 42);
    free(fds);
    return 0;
}

static int listenerTableCapacity(void) {
    tcpsrv_t server = {.iLstnCurr = 2};
    tcpsrv_listener_tables_t prepared = {0};
    tcpsrv_listener_tables_t retired = {0};
    tcpsrv_listener_tables_t initial = {0};
    netstrm_t streams[2] = {0};
    netstrm_t *first = &streams[0];
    netstrm_t *second = &streams[1];
    tcpLstnPortList_t ports[2] = {0};
    tcpsrv_io_descr_t descriptors[2] = {0};
    CHECK(tcpsrvPrepareListenerTables(NULL, 4, &prepared) == RS_RET_PARAM_ERROR);
    CHECK(tcpsrvPrepareListenerTables(&server, 0, &prepared) == RS_RET_PARAM_ERROR);
    CHECK(tcpsrvPrepareListenerTables(&server, 1, &prepared) == RS_RET_NOT_IMPLEMENTED);
    for (int failure = 1; failure <= 3; ++failure) {
        listenerAllocCall = 0;
        listenerAllocFailAt = failure;
        CHECK(tcpsrvPrepareListenerTables(&server, 4, &prepared) == RS_RET_OUT_OF_MEMORY);
        CHECK(listenerAllocCall == failure && listenerArraysOwned == 0);
        CHECK(prepared.streams == NULL && prepared.ports == NULL && prepared.descriptors == NULL &&
              prepared.capacity == 0);
    }
    listenerAllocFailAt = 0;
    CHECK(tcpsrvPrepareListenerTables(&server, 4, &initial) == RS_RET_OK);
    server.ppLstn = initial.streams;
    server.ppLstnPort = initial.ports;
    server.ppioDescrPtr = initial.descriptors;
    server.iLstnMax = initial.capacity;
    memset(&initial, 0, sizeof(initial));
    server.ppLstn[0] = first;
    server.ppLstn[1] = second;
    for (int i = 0; i < 2; ++i) {
        server.ppLstnPort[i] = &ports[i];
        server.ppioDescrPtr[i] = &descriptors[i];
        descriptors[i].ptr.ppLstn = server.ppLstn;
    }
    netstrm_t **oldStreams = server.ppLstn;
    CHECK(tcpsrvPrepareListenerTables(&server, 6, &prepared) == RS_RET_OK);
    CHECK(tcpsrvPrepareListenerTables(&server, 6, &prepared) == RS_RET_PARAM_ERROR);
    CHECK(tcpsrvStageListenerTablesWhileFenced(&server, &prepared) == RS_RET_PARAM_ERROR);
    server.fenceAcquired = server.fenceOwnerValid = 1;
    server.fenceOwner = pthread_self();
    CHECK(tcpsrvStageListenerTablesWhileFenced(&server, &prepared) == RS_RET_OK);
    CHECK(prepared.streams[0] == first && prepared.streams[1] == second && prepared.streams[5] == NULL);
    CHECK(server.ppLstn == oldStreams && descriptors[0].ptr.ppLstn == oldStreams);
    /* Abort destroys copied arrays only; runtime objects and bindings survive. */
    tcpsrvDestructListenerTables(&prepared);
    CHECK(listenerArraysOwned == 3 && server.ppLstn[1] == second);
    CHECK(tcpsrvPrepareListenerTables(&server, 6, &prepared) == RS_RET_OK);
    CHECK(tcpsrvStageListenerTablesWhileFenced(&server, &prepared) == RS_RET_OK);
    tcpsrvSwapListenerTablesLive(&server, &prepared, &retired);
    CHECK(server.iLstnMax == 6 && server.iLstnCurr == 2 && server.ppLstn != oldStreams);
    CHECK(server.ppLstn[0] == first && server.ppLstn[1] == second);
    CHECK(server.ppLstnPort[1] == &ports[1] && server.ppioDescrPtr[1] == &descriptors[1]);
    CHECK(descriptors[0].ptr.ppLstn == server.ppLstn && descriptors[1].ptr.ppLstn == server.ppLstn);
    CHECK(retired.streams == oldStreams && retired.capacity == 4 && prepared.streams == NULL);
    tcpsrvDestructListenerTables(&retired);
    CHECK(listenerArraysOwned == 3);

    CHECK(tcpsrvPrepareListenerTables(&server, 2, &prepared) == RS_RET_OK);
    server.ppLstn[5] = first;
    CHECK(tcpsrvStageListenerTablesWhileFenced(&server, &prepared) == RS_RET_NOT_IMPLEMENTED);
    server.ppLstn[5] = NULL;
    server.ppLstnPort[5] = &ports[0];
    CHECK(tcpsrvStageListenerTablesWhileFenced(&server, &prepared) == RS_RET_NOT_IMPLEMENTED);
    server.ppLstnPort[5] = NULL;
    server.ppioDescrPtr[5] = &descriptors[0];
    CHECK(tcpsrvStageListenerTablesWhileFenced(&server, &prepared) == RS_RET_NOT_IMPLEMENTED);
    server.ppioDescrPtr[5] = NULL;
    CHECK(prepared.streams[0] == NULL && server.iLstnMax == 6);
    CHECK(tcpsrvStageListenerTablesWhileFenced(&server, &prepared) == RS_RET_OK);
    tcpsrvSwapListenerTablesLive(&server, &prepared, &retired);
    CHECK(server.iLstnMax == 2 && server.ppLstn[0] == first && server.ppLstn[1] == second);
    CHECK(descriptors[0].ptr.ppLstn == server.ppLstn && descriptors[1].ptr.ppLstn == server.ppLstn);
    tcpsrvDestructListenerTables(&retired);
    initial = (tcpsrv_listener_tables_t){.streams = server.ppLstn,
                                         .ports = server.ppLstnPort,
                                         .descriptors = server.ppioDescrPtr,
                                         .capacity = server.iLstnMax};
    tcpsrvDestructListenerTables(&initial);
    tcpsrvDestructListenerTables(&initial);
    tcpsrvDestructListenerTables(NULL);
    CHECK(listenerArraysOwned == 0);
    return 0;
}

int main(void) {
    if (syncInitFailures() != 0) return 1;
    if (listenerTableCapacity() != 0) return 1;
    if (pollCapacity() != 0) return 1;
    if (singleWorkerRoundTrip() != 0) return 1;
    if (handoffBeforeFence() != 0) return 1;
    if (timeoutDrainAndRetry() != 0) return 1;
    if (termWhileParked() != 0) return 1;
    if (flowControlSnapshot() != 0) return 1;
    if (starvationMaxReadsSnapshot() != 0) return 1;
    if (rateLimitSnapshot() != 0) return 1;
    if (notificationSnapshot() != 0) return 1;
    if (preserveCaseNewSessions() != 0) return 1;
    if (keepAliveNewSessions() != 0) return 1;
    if (framingNewSessions() != 0) return 1;
    if (compressionNewSessions() != 0) return 1;
    if (defaultTZSnapshot() != 0) return 1;
    if (rulesetSnapshot() != 0) return 1;
    puts("tcpsrv reload fence tests passed");
    return 0;
}
