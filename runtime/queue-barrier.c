/* Queue-level composition of non-destructive worker-pool barriers. */
#include "config.h"

#include "rsyslog.h"
#include "queue.h"

struct qqueue_batch_barrier_s {
    qqueue_t *queue;
    wtp_t *pools[2];
    unsigned nPools;
    unsigned nRequested;
};

static void adviseAccumulatedWorkLocked(qqueue_t *const queue, wtp_t *const pool) {
    const int size = queue->iQueueSize - queue->nLogDeq;
    int workers;

    if (size <= 0 || queue->bEnqOnly) return;
    workers = queue->iMinMsgsPerWrkr == 0 ? 1 : size / queue->iMinMsgsPerWrkr + 1;
    wtpAdviseMaxWorkers(pool, workers, DENY_WORKER_START_DURING_SHUTDOWN);
}

rsRetVal qqueueBatchBarrierBegin(qqueue_t *const pThis, qqueue_batch_barrier_t **const ppBarrier) {
    qqueue_batch_barrier_t *barrier = NULL;
    rsRetVal ret = RS_RET_OK;

    if (pThis == NULL || ppBarrier == NULL || *ppBarrier != NULL) return RS_RET_PARAM_ERROR;
    if (pThis->qType == QUEUETYPE_DIRECT) return RS_RET_NOT_IMPLEMENTED;
    if (pThis->mut == NULL || pThis->pWtpReg == NULL) return RS_RET_NO_RUN;
    if ((barrier = calloc(1, sizeof(*barrier))) == NULL) return RS_RET_OUT_OF_MEMORY;
    barrier->queue = pThis;
    barrier->pools[barrier->nPools++] = pThis->pWtpReg;
    if (pThis->bIsDA && pThis->pqDA != NULL && pThis->pqDA->pWtpReg != NULL)
        barrier->pools[barrier->nPools++] = pThis->pqDA->pWtpReg;

    d_pthread_mutex_lock(pThis->mut);
    for (unsigned i = 0; i < barrier->nPools; ++i) {
        ret = wtpRequestQuiesceLocked(barrier->pools[i]);
        if (ret != RS_RET_OK) break;
        ++barrier->nRequested;
    }
    if (ret != RS_RET_OK)
        while (barrier->nRequested != 0) wtpResumeLocked(barrier->pools[--barrier->nRequested]);
    d_pthread_mutex_unlock(pThis->mut);

    if (ret == RS_RET_OK)
        *ppBarrier = barrier;
    else
        free(barrier);
    return ret;
}

rsRetVal qqueueBatchBarrierWait(qqueue_batch_barrier_t *const pBarrier, const struct timespec *const ptTimeout) {
    if (pBarrier == NULL || ptTimeout == NULL) return RS_RET_PARAM_ERROR;
    for (unsigned i = 0; i < pBarrier->nRequested; ++i) {
        const rsRetVal ret = wtpWaitQuiesced(pBarrier->pools[i], ptTimeout);
        if (ret != RS_RET_OK) return ret;
    }
    return RS_RET_OK;
}

void qqueueBatchBarrierRelease(qqueue_batch_barrier_t **const ppBarrier) {
    qqueue_batch_barrier_t *barrier;

    if (ppBarrier == NULL || *ppBarrier == NULL) return;
    barrier = *ppBarrier;
    d_pthread_mutex_lock(barrier->queue->mut);
    while (barrier->nRequested != 0) wtpResumeLocked(barrier->pools[--barrier->nRequested]);
    adviseAccumulatedWorkLocked(barrier->queue, barrier->queue->pWtpReg);
    if (barrier->queue->bIsDA && barrier->queue->pqDA != NULL && barrier->queue->pqDA->pWtpReg != NULL)
        adviseAccumulatedWorkLocked(barrier->queue->pqDA, barrier->queue->pqDA->pWtpReg);
    d_pthread_mutex_unlock(barrier->queue->mut);
    free(barrier);
    *ppBarrier = NULL;
}
