/* SPDX-License-Identifier: Apache-2.0
 */
/** @file queue-barrier.c
 * @brief Queue-level composition of non-destructive worker-pool barriers.
 *
 * Concurrency & Locking: begin/wait/release belong to one control thread.
 * Pool operations acquire their own mutexes; queue advice holds queue->mut.
 * Completion mode invokes the normal preceding-batch callback before a live
 * worker parks, including DA transfer owners. It does not close producers or
 * prove retirement emptiness: departed-worker leases, RAM/disk physical and
 * logical inventory, to-delete entries, storeData and deferred messages must
 * be accounted for first. In particular segdiskStoreCanDematerialize proves
 * store-local absence of pending/recovery/delete work, not absence of WTI
 * owners. Exiting/cancel cleanup currently discards completion errors; making
 * those obligations observable is the next prerequisite, not restart policy.
 * No queue or pool mutex may be held by the control caller on entry.
 */
#include "config.h"

#include "rsyslog.h"
#include "queue.h"
#include "segdisk_store.h"

struct qqueue_batch_barrier_s {
    qqueue_t *queue;
    wtp_t *pools[3];
    uint64_t generations[3];
    int complete;
    unsigned nPools;
    unsigned nRequested;
    pthread_t owner;
};

static int logicalSizeLocked(const qqueue_t *const queue) {
    int size = queue->iQueueSize - queue->nLogDeq;

    if (size == 0 && queue->qType == QUEUETYPE_SEGMENTED_DISK && queue->tVars.segdisk != NULL &&
        segdiskStoreMayHaveData(queue->tVars.segdisk))
        size = 1;
    return size;
}

static void adviseAccumulatedWorkLocked(qqueue_t *const queue) {
    const int size = logicalSizeLocked(queue);
    int workers;

    if (size <= 0 || queue->bEnqOnly) return;
    if (queue->bIsDA && queue->pWtpDA != NULL && size >= queue->iHighWtrMrk)
        wtpAdviseMaxWorkers(queue->pWtpDA, 1, DENY_WORKER_START_DURING_SHUTDOWN);
    workers = queue->iMinMsgsPerWrkr <= 0 ? 1 : size / queue->iMinMsgsPerWrkr + 1;
    wtpAdviseMaxWorkers(queue->pWtpReg, workers, DENY_WORKER_START_DURING_SHUTDOWN);
}

static void adviseAccumulatedWork(qqueue_t *const queue) {
    if (queue == NULL || queue->mut == NULL || queue->pWtpReg == NULL) return;
    d_pthread_mutex_lock(queue->mut);
    adviseAccumulatedWorkLocked(queue);
    d_pthread_mutex_unlock(queue->mut);
}

static rsRetVal beginBarrier(qqueue_t *const pThis, qqueue_batch_barrier_t **const ppBarrier, const int complete) {
    qqueue_batch_barrier_t *barrier = NULL;
    rsRetVal ret = RS_RET_OK;

    if (pThis == NULL || ppBarrier == NULL || *ppBarrier != NULL) return RS_RET_PARAM_ERROR;
    if (pThis->local_graph_conf != NULL) return RS_RET_NOT_IMPLEMENTED;
    if (pThis->qType == QUEUETYPE_DIRECT) return RS_RET_NOT_IMPLEMENTED;
    if (pThis->mut == NULL || pThis->pWtpReg == NULL) return RS_RET_NO_RUN;
    if ((barrier = calloc(1, sizeof(*barrier))) == NULL) return RS_RET_OUT_OF_MEMORY;
    barrier->queue = pThis;
    barrier->complete = complete;
    barrier->owner = pthread_self();
    barrier->pools[barrier->nPools++] = pThis->pWtpReg;
    /* Transfer workers own parent batches independently of disk consumers. */
    if (complete && pThis->bIsDA && pThis->pWtpDA != NULL) barrier->pools[barrier->nPools++] = pThis->pWtpDA;
    if (pThis->bIsDA && pThis->pqDA != NULL && pThis->pqDA->pWtpReg != NULL)
        barrier->pools[barrier->nPools++] = pThis->pqDA->pWtpReg;

    for (unsigned i = 0; i < barrier->nPools; ++i) {
        ret = complete ? wtpRequestCompletionQuiesce(barrier->pools[i], &barrier->generations[i])
                       : wtpRequestQuiesce(barrier->pools[i]);
        if (ret != RS_RET_OK) break;
        ++barrier->nRequested;
    }
    if (ret != RS_RET_OK) {
        while (barrier->nRequested != 0) {
            const unsigned i = --barrier->nRequested;
            if (complete)
                wtpResumeCompletion(barrier->pools[i], barrier->generations[i]);
            else
                wtpResume(barrier->pools[i]);
        }
        adviseAccumulatedWork(barrier->queue);
        if (barrier->queue->bIsDA) adviseAccumulatedWork(barrier->queue->pqDA);
    }

    if (ret == RS_RET_OK)
        *ppBarrier = barrier;
    else
        free(barrier);
    return ret;
}

rsRetVal qqueueBatchBarrierBegin(qqueue_t *const queue, qqueue_batch_barrier_t **const barrier) {
    return beginBarrier(queue, barrier, 0);
}

rsRetVal qqueueCompletionBarrierBegin(qqueue_t *const queue, qqueue_batch_barrier_t **const barrier) {
    return beginBarrier(queue, barrier, 1);
}

rsRetVal qqueueBatchBarrierWait(qqueue_batch_barrier_t *const pBarrier, const struct timespec *const ptTimeout) {
    if (pBarrier == NULL || ptTimeout == NULL) return RS_RET_PARAM_ERROR;
    if (!pthread_equal(pBarrier->owner, pthread_self())) return RS_RET_PARAM_ERROR;
    for (unsigned i = 0; i < pBarrier->nRequested; ++i) {
        const rsRetVal ret = pBarrier->complete
                                 ? wtpWaitCompletionQuiesced(pBarrier->pools[i], pBarrier->generations[i], ptTimeout)
                                 : wtpWaitQuiesced(pBarrier->pools[i], ptTimeout);
        if (ret != RS_RET_OK) return ret;
    }
    return RS_RET_OK;
}

rsRetVal qqueueBatchBarrierRelease(qqueue_batch_barrier_t **const ppBarrier) {
    qqueue_batch_barrier_t *barrier;

    if (ppBarrier == NULL || *ppBarrier == NULL) return RS_RET_PARAM_ERROR;
    barrier = *ppBarrier;
    if (!pthread_equal(barrier->owner, pthread_self())) return RS_RET_PARAM_ERROR;
    while (barrier->nRequested != 0) {
        const unsigned i = --barrier->nRequested;
        if (barrier->complete)
            wtpResumeCompletion(barrier->pools[i], barrier->generations[i]);
        else
            wtpResume(barrier->pools[i]);
    }
    adviseAccumulatedWork(barrier->queue);
    if (barrier->queue->bIsDA) adviseAccumulatedWork(barrier->queue->pqDA);
    free(barrier);
    *ppBarrier = NULL;
    return RS_RET_OK;
}
