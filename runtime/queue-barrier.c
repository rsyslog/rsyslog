/* Queue-level composition of non-destructive worker-pool barriers. */
#include "config.h"

#include "rsyslog.h"
#include "queue.h"
#include "segdisk_store.h"

struct qqueue_batch_barrier_s {
    qqueue_t *queue;
    wtp_t *pools[2];
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

rsRetVal qqueueBatchBarrierBegin(qqueue_t *const pThis, qqueue_batch_barrier_t **const ppBarrier) {
    qqueue_batch_barrier_t *barrier = NULL;
    rsRetVal ret = RS_RET_OK;

    if (pThis == NULL || ppBarrier == NULL || *ppBarrier != NULL) return RS_RET_PARAM_ERROR;
    if (pThis->qType == QUEUETYPE_DIRECT) return RS_RET_NOT_IMPLEMENTED;
    if (pThis->mut == NULL || pThis->pWtpReg == NULL) return RS_RET_NO_RUN;
    if ((barrier = calloc(1, sizeof(*barrier))) == NULL) return RS_RET_OUT_OF_MEMORY;
    barrier->queue = pThis;
    barrier->owner = pthread_self();
    barrier->pools[barrier->nPools++] = pThis->pWtpReg;
    if (pThis->bIsDA && pThis->pqDA != NULL && pThis->pqDA->pWtpReg != NULL)
        barrier->pools[barrier->nPools++] = pThis->pqDA->pWtpReg;

    for (unsigned i = 0; i < barrier->nPools; ++i) {
        ret = wtpRequestQuiesce(barrier->pools[i]);
        if (ret != RS_RET_OK) break;
        ++barrier->nRequested;
    }
    if (ret != RS_RET_OK) {
        while (barrier->nRequested != 0) wtpResume(barrier->pools[--barrier->nRequested]);
        adviseAccumulatedWork(barrier->queue);
        if (barrier->queue->bIsDA) adviseAccumulatedWork(barrier->queue->pqDA);
    }

    if (ret == RS_RET_OK)
        *ppBarrier = barrier;
    else
        free(barrier);
    return ret;
}

rsRetVal qqueueBatchBarrierWait(qqueue_batch_barrier_t *const pBarrier, const struct timespec *const ptTimeout) {
    if (pBarrier == NULL || ptTimeout == NULL) return RS_RET_PARAM_ERROR;
    if (!pthread_equal(pBarrier->owner, pthread_self())) return RS_RET_PARAM_ERROR;
    for (unsigned i = 0; i < pBarrier->nRequested; ++i) {
        const rsRetVal ret = wtpWaitQuiesced(pBarrier->pools[i], ptTimeout);
        if (ret != RS_RET_OK) return ret;
    }
    return RS_RET_OK;
}

rsRetVal qqueueBatchBarrierRelease(qqueue_batch_barrier_t **const ppBarrier) {
    qqueue_batch_barrier_t *barrier;

    if (ppBarrier == NULL || *ppBarrier == NULL) return RS_RET_PARAM_ERROR;
    barrier = *ppBarrier;
    if (!pthread_equal(barrier->owner, pthread_self())) return RS_RET_PARAM_ERROR;
    while (barrier->nRequested != 0) wtpResume(barrier->pools[--barrier->nRequested]);
    adviseAccumulatedWork(barrier->queue);
    if (barrier->queue->bIsDA) adviseAccumulatedWork(barrier->queue->pqDA);
    free(barrier);
    *ppBarrier = NULL;
    return RS_RET_OK;
}
