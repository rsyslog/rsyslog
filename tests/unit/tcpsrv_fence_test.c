/*
 * Exercise the tcpsrv live-reload fence without sockets or a daemon. The
 * oracle drives the production Request/Wait/Release and event-loop/worker
 * safepoints directly: single-worker acquisition, multi-worker timeout with
 * automatic abort/drain followed by a fresh generation, and TERM while all
 * participants are parked. Bounded absolute deadlines prevent a broken fence
 * from hanging the unit test.
 */
#include "config.h"

#include <stdatomic.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "rsyslog.h"
#include "ruleset.h"
#include "tcpsrv.h"

/* Include the small production state machine so the unit can drive its
 * event-loop and worker safepoints without constructing network objects. */
#include "../../runtime/tcpsrv-fence.c"

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
    if (atomic_load_explicit(&terminateOnOwnerCheck, memory_order_relaxed) &&
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
    pthread_mutex_init(&server->fenceMut, NULL);
    pthread_cond_init(&server->fenceCond, NULL);
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
    while (server->workQueue.head == NULL) pthread_cond_wait(&server->workQueue.workRdy, &server->workQueue.mut);
    item = server->workQueue.head;
    server->workQueue.head = item->next;
    if (server->workQueue.head == NULL) server->workQueue.tail = NULL;
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
    deadline = deadlineAfterMs(20);
    CHECK(tcpsrvWaitFence(&server, token, &deadline) == RS_RET_TIMEOUT);
    CHECK(tcpsrvReleaseFence(&server, token) == RS_RET_PARAM_ERROR);
    CHECK(tcpsrvRequestFence(&server, &token) == RS_RET_NOT_IMPLEMENTED);
    for (size_t i = 0; i < 2; ++i) CHECK(pthread_create(&workers[i], NULL, workerOnce, &server) == 0);
    for (size_t i = 0; i < 2; ++i) CHECK(pthread_join(workers[i], NULL) == 0);
    CHECK(pthread_join(eventLoop, NULL) == 0);
    CHECK(!atomic_load_explicit(&threadFailure, memory_order_relaxed));
    CHECK(waitUntilIdle(&server));
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
    atomic_store_explicit(&terminateOnOwnerCheck, 1, memory_order_relaxed);
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

int main(void) {
    if (singleWorkerRoundTrip() != 0) return 1;
    if (timeoutDrainAndRetry() != 0) return 1;
    if (termWhileParked() != 0) return 1;
    if (flowControlSnapshot() != 0) return 1;
    if (starvationMaxReadsSnapshot() != 0) return 1;
    if (notificationSnapshot() != 0) return 1;
    if (preserveCaseNewSessions() != 0) return 1;
    if (keepAliveNewSessions() != 0) return 1;
    if (defaultTZSnapshot() != 0) return 1;
    if (rulesetSnapshot() != 0) return 1;
    puts("tcpsrv reload fence tests passed");
    return 0;
}
