/* tcpsrv live-reload event-loop and worker fence. */
#include "config.h"

#include <assert.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

#include "rsyslog.h"
#include "dirty.h"
#include "tcpsrv.h"
#include "unicode-helper.h"

static rsRetVal ATTR_NONNULL() wakeEventLoop(tcpsrv_t *const server) {
    const char wake = 'F';
    rsRetVal ret = RS_RET_OK;
    pthread_mutex_lock(&server->fenceMut);
    if (server->fenceReady && server->controlPipe[1] >= 0) {
        ssize_t written;
        do written = write(server->controlPipe[1], &wake, sizeof(wake));
        while (written < 0 && errno == EINTR);
        if (written < 0 && errno != EAGAIN && errno != EWOULDBLOCK) ret = RS_RET_IO_ERROR;
    } else {
        ret = RS_RET_NOT_IMPLEMENTED;
    }
    pthread_mutex_unlock(&server->fenceMut);
    return ret;
}

static int ATTR_NONNULL() tokenOwnedByCaller(tcpsrv_t *const server, const uint64_t token) {
    return token != 0 && token == server->fenceGeneration && server->fenceOwnerValid &&
           pthread_equal(server->fenceOwner, pthread_self());
}

void tcpsrvAbortFenceLocked(tcpsrv_t *const server) {
    server->fenceRequested = 0;
    server->fenceRelease = 1;
    if (!server->fenceActive) server->fenceOwnerValid = 0;
    pthread_cond_broadcast(&server->fenceCond);
}

static void ATTR_NONNULL() finishDrainLocked(tcpsrv_t *const server) {
    if (server->fenceRelease && server->fenceOutstanding == 0 && server->fenceParked == 0 &&
        !server->fenceEventLoopParked) {
        server->fenceActive = 0;
        server->fenceOwnerValid = 0;
        pthread_cond_broadcast(&server->fenceCond);
    }
}

static int timespecCmp(const struct timespec *const left, const struct timespec *const right) {
    if (left->tv_sec != right->tv_sec) return left->tv_sec < right->tv_sec ? -1 : 1;
    return (left->tv_nsec > right->tv_nsec) - (left->tv_nsec < right->tv_nsec);
}

static rsRetVal waitSlice(pthread_cond_t *const cond,
                          pthread_mutex_t *const mut,
                          const struct timespec *const finalDeadline) {
    struct timespec slice;
    if (clock_gettime(CLOCK_REALTIME, &slice) != 0) return RS_RET_ERR;
    slice.tv_nsec += 50000000L;
    if (slice.tv_nsec >= 1000000000L) {
        ++slice.tv_sec;
        slice.tv_nsec -= 1000000000L;
    }
    if (finalDeadline != NULL && timespecCmp(finalDeadline, &slice) < 0) slice = *finalDeadline;
    const int waitRet = pthread_cond_timedwait(cond, mut, &slice);
    return waitRet == 0 ? RS_RET_OK : (waitRet == ETIMEDOUT ? RS_RET_TIMEOUT : RS_RET_ERR);
}

rsRetVal tcpsrvRequestFence(tcpsrv_t *const server, uint64_t *const token) {
    if (server == NULL || token == NULL) return RS_RET_PARAM_ERROR;
    *token = 0;
    if (!server->fenceSyncInitialized) return RS_RET_NOT_IMPLEMENTED;
    pthread_mutex_lock(&server->fenceMut);
    if (!server->fenceReady || server->fenceOwnerValid || server->fenceRequested || server->fenceActive) {
        pthread_mutex_unlock(&server->fenceMut);
        return RS_RET_NOT_IMPLEMENTED;
    }
    ++server->fenceGeneration;
    if (server->fenceGeneration == 0) ++server->fenceGeneration;
    server->fenceAcks = 0;
    server->fenceParked = 0;
    server->fenceOutstanding = 0;
    server->fenceEventLoopParked = 0;
    server->fenceRelease = 0;
    server->fenceAcquired = 0;
    server->fenceReleaseCommitted = 0;
    server->fenceRequested = 1;
    server->fenceOwner = pthread_self();
    server->fenceOwnerValid = 1;
    *token = server->fenceGeneration;
    pthread_mutex_unlock(&server->fenceMut);
    const rsRetVal wakeRet = wakeEventLoop(server);
    if (wakeRet != RS_RET_OK) {
        pthread_mutex_lock(&server->fenceMut);
        if (tokenOwnedByCaller(server, *token)) tcpsrvAbortFenceLocked(server);
        pthread_mutex_unlock(&server->fenceMut);
        *token = 0;
        return wakeRet;
    }
    return RS_RET_OK;
}

rsRetVal tcpsrvWaitFence(tcpsrv_t *const server, const uint64_t token, const struct timespec *const deadline) {
    if (server == NULL || deadline == NULL || !server->fenceSyncInitialized) return RS_RET_PARAM_ERROR;
    pthread_mutex_lock(&server->fenceMut);
    if (!tokenOwnedByCaller(server, token) || server->fenceRelease) {
        pthread_mutex_unlock(&server->fenceMut);
        return RS_RET_PARAM_ERROR;
    }
    const unsigned requiredAcks = server->workQueue.numWrkr > 1 ? server->workQueue.numWrkr : 1;
    while ((server->fenceAcks != requiredAcks || !server->fenceEventLoopParked) && !tcpsrvFenceTerminated()) {
        const rsRetVal waitRet = waitSlice(&server->fenceCond, &server->fenceMut, deadline);
        struct timespec now;
        if (waitRet != RS_RET_OK && waitRet != RS_RET_TIMEOUT) {
            tcpsrvAbortFenceLocked(server);
            pthread_mutex_unlock(&server->fenceMut);
            (void)wakeEventLoop(server);
            return waitRet;
        }
        if (waitRet == RS_RET_TIMEOUT && clock_gettime(CLOCK_REALTIME, &now) == 0 && timespecCmp(&now, deadline) >= 0) {
            tcpsrvAbortFenceLocked(server);
            pthread_mutex_unlock(&server->fenceMut);
            (void)wakeEventLoop(server);
            return RS_RET_TIMEOUT;
        }
    }
    if (tcpsrvFenceTerminated()) {
        tcpsrvAbortFenceLocked(server);
        pthread_mutex_unlock(&server->fenceMut);
        (void)wakeEventLoop(server);
        return RS_RET_FORCE_TERM;
    }
    if (server->fenceRelease) {
        pthread_mutex_unlock(&server->fenceMut);
        return RS_RET_PARAM_ERROR;
    }
    server->fenceAcquired = 1;
    pthread_mutex_unlock(&server->fenceMut);
    return RS_RET_OK;
}

rsRetVal tcpsrvReleaseFence(tcpsrv_t *const server, const uint64_t token) {
    int notify = 0;
    if (server == NULL || !server->fenceSyncInitialized) return RS_RET_PARAM_ERROR;
    pthread_mutex_lock(&server->fenceMut);
    const unsigned requiredAcks = server->workQueue.numWrkr > 1 ? server->workQueue.numWrkr : 1;
    if (token == server->fenceGeneration && !server->fenceActive && !server->fenceOwnerValid &&
        server->fenceReleaseCommitted) {
        pthread_mutex_unlock(&server->fenceMut);
        return RS_RET_OK;
    }
    if (!tokenOwnedByCaller(server, token) || !server->fenceActive ||
        (!server->fenceRelease && (!server->fenceAcquired || server->fenceAcks != requiredAcks)) ||
        (server->fenceRelease && !server->fenceReleaseCommitted)) {
        pthread_mutex_unlock(&server->fenceMut);
        return RS_RET_PARAM_ERROR;
    }
    if (!server->fenceRelease) {
        server->fenceRelease = 1;
        server->fenceReleaseCommitted = 1;
        server->fenceRequested = 0;
        pthread_cond_broadcast(&server->fenceCond);
        notify = 1;
    }
    pthread_mutex_unlock(&server->fenceMut);
    if (notify) (void)wakeEventLoop(server);

    /* HUP completion must not race the asynchronous unpark/drain. A retry of
     * Release after a rare condvar error rejoins the same generation. */
    pthread_mutex_lock(&server->fenceMut);
    while (server->fenceActive) {
        const rsRetVal waitRet = waitSlice(&server->fenceCond, &server->fenceMut, NULL);
        if (waitRet != RS_RET_OK && waitRet != RS_RET_TIMEOUT) {
            pthread_mutex_unlock(&server->fenceMut);
            return waitRet;
        }
    }
    pthread_mutex_unlock(&server->fenceMut);
    return RS_RET_OK;
}

static void ATTR_NONNULL() enqueueFenceItems(tcpsrv_t *const server) {
    workQueue_t *const queue = &server->workQueue;
    for (unsigned i = 0; i < queue->numWrkr; ++i) {
        tcpsrv_io_descr_t *const item = &server->fenceItems[i];
        item->next = NULL;
        if (queue->tail == NULL)
            queue->head = item;
        else
            queue->tail->next = item;
        queue->tail = item;
    }
    pthread_cond_broadcast(&queue->workRdy);
}

void tcpsrvActivateFence(tcpsrv_t *const server) {
    workQueue_t *const queue = &server->workQueue;
    pthread_mutex_lock(&server->fenceMut);
    if (server->fenceRequested && !server->fenceActive) {
        server->fenceActive = 1;
        if (queue->numWrkr > 1) {
            server->fenceOutstanding = queue->numWrkr;
            pthread_mutex_lock(&queue->mut);
            enqueueFenceItems(server);
            pthread_mutex_unlock(&queue->mut);
        } else {
            server->fenceAcks = 1;
        }
        server->fenceEventLoopParked = 1;
        pthread_cond_broadcast(&server->fenceCond);
        while (server->fenceActive && !server->fenceRelease && !tcpsrvFenceTerminated()) {
            const rsRetVal waitRet = waitSlice(&server->fenceCond, &server->fenceMut, NULL);
            if (waitRet != RS_RET_OK && waitRet != RS_RET_TIMEOUT) tcpsrvAbortFenceLocked(server);
        }
        if (tcpsrvFenceTerminated()) tcpsrvAbortFenceLocked(server);
        server->fenceEventLoopParked = 0;
        finishDrainLocked(server);
    }
    pthread_mutex_unlock(&server->fenceMut);
}

void tcpsrvParkAtFence(tcpsrv_t *const server) {
    pthread_mutex_lock(&server->fenceMut);
    if (server->fenceActive) {
        assert(server->fenceOutstanding > 0);
        --server->fenceOutstanding;
        ++server->fenceAcks;
        ++server->fenceParked;
        pthread_cond_broadcast(&server->fenceCond);
        while (server->fenceActive && !server->fenceRelease && !tcpsrvFenceTerminated()) {
            const rsRetVal waitRet = waitSlice(&server->fenceCond, &server->fenceMut, NULL);
            if (waitRet != RS_RET_OK && waitRet != RS_RET_TIMEOUT) tcpsrvAbortFenceLocked(server);
        }
        --server->fenceParked;
        finishDrainLocked(server);
    }
    pthread_mutex_unlock(&server->fenceMut);
}

void tcpsrvApplyFlowControlLive(tcpsrv_t *const server, const int useFlowControl) {
    server->bUseFlowControl = useFlowControl;
    if (server->pSessions == NULL) return;
    for (int i = 0; i < server->iSessMax; ++i) {
        tcps_sess_t *const session = server->pSessions[i];
        if (session != NULL) session->bUseFlowControl = useFlowControl;
    }
}

void tcpsrvApplyStarvationMaxReadsLive(tcpsrv_t *const server, const unsigned maxReads) {
    server->starvationMaxReads = maxReads;
}

void tcpsrvApplyNotificationsLive(tcpsrv_t *const server, const int onOpen, const int onClose) {
    server->bEmitMsgOnOpen = onOpen;
    server->bEmitMsgOnClose = onClose;
}

void tcpsrvApplyPreserveCaseForNewSessions(tcpsrv_t *const server, const int preserveCase) {
    tcpLstnPortList_t *listener;

    server->bPreserveCase = preserveCase;
    for (listener = server->pLstnPorts; listener != NULL; listener = listener->pNext) {
        if (listener->cnf_params != NULL) listener->cnf_params->bPreserveCase = preserveCase;
    }
}

void tcpsrvApplyKeepAliveForNewSessions(
    tcpsrv_t *const server, const int enabled, const int interval, const int probes, const int time) {
    server->bUseKeepAlive = enabled;
    server->iKeepAliveIntvl = interval;
    server->iKeepAliveProbes = probes;
    server->iKeepAliveTime = time;
}

void tcpsrvApplyFramingForNewSessions(tcpsrv_t *const server,
                                      const int spFramingFix,
                                      const int additionalDelimiter,
                                      const int maxFrameSize,
                                      const int disableLFDelimiter,
                                      const int discardTruncatedMessage) {
    tcpLstnPortList_t *listener;

    server->bSPFramingFix = spFramingFix;
    server->addtlFrameDelim = additionalDelimiter;
    server->maxFrameSize = maxFrameSize;
    server->bDisableLFDelim = disableLFDelimiter;
    server->discardTruncatedMsg = discardTruncatedMessage;
    for (listener = server->pLstnPorts; listener != NULL; listener = listener->pNext) {
        if (listener->cnf_params != NULL) listener->cnf_params->bSPFramingFix = spFramingFix;
    }
}

void tcpsrvApplyOctetCountedFramingForNewSessions(tcpsrv_t *const server, const int enabled) {
    tcpLstnPortList_t *listener;

    for (listener = server->pLstnPorts; listener != NULL; listener = listener->pNext) {
        if (listener->cnf_params != NULL) listener->cnf_params->bSuppOctetFram = enabled;
    }
}

void tcpsrvApplyDefaultTZLive(tcpsrv_t *const server, const uchar *const defaultTZ) {
    tcpLstnPortList_t *listener;
    const uchar *const value = defaultTZ == NULL ? UCHAR_CONSTANT("") : defaultTZ;

    u_cstr_copy(server->dfltTZ, value, sizeof(server->dfltTZ));
    for (listener = server->pLstnPorts; listener != NULL; listener = listener->pNext) {
        if (listener->cnf_params != NULL)
            u_cstr_copy(listener->cnf_params->dfltTZ, value, sizeof(listener->cnf_params->dfltTZ));
    }
    if (server->pSessions == NULL) return;
    for (int i = 0; i < server->iSessMax; ++i) {
        tcps_sess_t *const session = server->pSessions[i];
        if (session != NULL) u_cstr_copy(session->dfltTZ, value, sizeof(session->dfltTZ));
    }
}

void tcpsrvApplyRulesetLive(tcpsrv_t *const server, ruleset_t *const ruleset) {
    tcpLstnPortList_t *listener;

    for (listener = server->pLstnPorts; listener != NULL; listener = listener->pNext) {
        if (listener->cnf_params != NULL) listener->cnf_params->pRuleset = ruleset;
    }
    if (server->pSessions == NULL) return;
    for (int i = 0; i < server->iSessMax; ++i) {
        tcps_sess_t *const session = server->pSessions[i];
        if (session != NULL) session->pRuleset = ruleset;
    }
}
