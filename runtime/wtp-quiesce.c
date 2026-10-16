/* SPDX-License-Identifier: Apache-2.0
 */
/** @file wtp-quiesce.c
 * @brief Non-destructive worker-pool batch-boundary barrier.
 *
 * Concurrency & Locking: control requests/resume lock pmutUsr before mutWtp.
 * Workers enter with pmutUsr held and use mutWtp for acknowledgement counters;
 * condition waits release only their associated mutex. State uses atomics.
 * Completion mode is generation-bound and records callback errors before
 * acknowledgment; callbacks may drop pmutUsr. Ordinary mode never invokes the
 * completion callback and retains its consumer-return boundary semantics.
 */
#include "config.h"

#include "rsyslog.h"
#include "atomic.h"
#include "wti.h"
#include "wtp.h"

static rsRetVal requestQuiesce(wtp_t *const pThis, const int complete, uint64_t *const generation) {
    rsRetVal ret = RS_RET_OK;

    if (pThis == NULL || pThis->pmutUsr == NULL) return RS_RET_PARAM_ERROR;
    d_pthread_mutex_lock(pThis->pmutUsr);
    d_pthread_mutex_lock(&pThis->mutWtp);
    const wtpState_t state = (wtpState_t)ATOMIC_LOAD_32BIT((int *)&pThis->wtpState, &pThis->mutWtpState);
    if (state != wtpState_RUNNING) {
        ret = RS_RET_NO_RUN;
    } else if (complete && pThis->iNumCompletionQuiesced != 0) {
        ret = RS_RET_RETRY; /* an older generation is still leaving its park */
    } else {
        if (++pThis->quiesceGeneration == 0) ++pThis->quiesceGeneration;
        pThis->completionQuiesce = complete;
        if (complete) pThis->completionQuiesceResult = RS_RET_OK;
        if (generation != NULL) *generation = pThis->quiesceGeneration;
        ATOMIC_STORE_32BIT((int *)&pThis->wtpState, &pThis->mutWtpState, wtpState_QUIESCE);
        for (int i = 0; i < pThis->iNumWorkerThreads; ++i) pthread_cond_signal(&pThis->pWrkr[i]->pcondBusy);
    }
    d_pthread_mutex_unlock(&pThis->mutWtp);
    d_pthread_mutex_unlock(pThis->pmutUsr);
    return ret;
}

rsRetVal wtpRequestQuiesce(wtp_t *const pool) {
    return requestQuiesce(pool, 0, NULL);
}

rsRetVal wtpRequestCompletionQuiesce(wtp_t *const pool, uint64_t *const generation) {
    if (generation == NULL) return RS_RET_PARAM_ERROR;
    *generation = 0;
    return requestQuiesce(pool, 1, generation);
}

rsRetVal wtpWaitCompletionQuiesced(wtp_t *const pool,
                                   const uint64_t generation,
                                   const struct timespec *const deadline) {
    rsRetVal result = RS_RET_OK;
    int expired = 0;
    if (pool == NULL || generation == 0 || deadline == NULL) return RS_RET_PARAM_ERROR;
    d_pthread_mutex_lock(&pool->mutWtp);
    for (;;) {
        const wtpState_t state = (wtpState_t)ATOMIC_LOAD_32BIT((int *)&pool->wtpState, &pool->mutWtpState);
        if (pool->quiesceGeneration != generation || !pool->completionQuiesce) {
            result = RS_RET_NO_RUN;
            break;
        }
        if (pool->completionQuiesceResult != RS_RET_OK) {
            result = pool->completionQuiesceResult;
            break;
        }
        if (state != wtpState_QUIESCE) {
            result = RS_RET_NO_RUN;
            break;
        }
        if (pool->iNumCompletionQuiesced >= ATOMIC_LOAD_32BIT(&pool->iCurNumWrkThrd, &pool->mutCurNumWrkThrd)) break;
        if (expired) {
            result = RS_RET_TIMED_OUT;
            break;
        }
        expired = d_pthread_cond_timedwait(&pool->condQuiesced, &pool->mutWtp, deadline) != 0;
    }
    d_pthread_mutex_unlock(&pool->mutWtp);
    return result;
}

rsRetVal wtpWaitQuiesced(wtp_t *const pThis, const struct timespec *const ptTimeout) {
    int timedOut = 0;

    if (pThis == NULL || ptTimeout == NULL) return RS_RET_PARAM_ERROR;
    d_pthread_mutex_lock(&pThis->mutWtp);
    while (pThis->iNumQuiesced < ATOMIC_LOAD_32BIT(&pThis->iCurNumWrkThrd, &pThis->mutCurNumWrkThrd)) {
        if (d_pthread_cond_timedwait(&pThis->condQuiesced, &pThis->mutWtp, ptTimeout) != 0) {
            timedOut = 1;
            break;
        }
    }
    d_pthread_mutex_unlock(&pThis->mutWtp);
    return timedOut ? RS_RET_TIMED_OUT : RS_RET_OK;
}

static rsRetVal resumeQuiesce(wtp_t *const pThis, const uint64_t generation) {
    if (pThis == NULL || pThis->pmutUsr == NULL) return RS_RET_PARAM_ERROR;
    d_pthread_mutex_lock(pThis->pmutUsr);
    d_pthread_mutex_lock(&pThis->mutWtp);
    if (generation != 0 && (pThis->quiesceGeneration != generation || !pThis->completionQuiesce)) {
        d_pthread_mutex_unlock(&pThis->mutWtp);
        d_pthread_mutex_unlock(pThis->pmutUsr);
        return RS_RET_NO_RUN;
    }
    const wtpState_t state = (wtpState_t)ATOMIC_LOAD_32BIT((int *)&pThis->wtpState, &pThis->mutWtpState);
    if (state == wtpState_QUIESCE) ATOMIC_STORE_32BIT((int *)&pThis->wtpState, &pThis->mutWtpState, wtpState_RUNNING);
    for (int i = 0; i < pThis->iNumWorkerThreads; ++i) pthread_cond_signal(&pThis->pWrkr[i]->pcondBusy);
    pthread_cond_broadcast(&pThis->condQuiesced);
    d_pthread_mutex_unlock(&pThis->mutWtp);
    d_pthread_mutex_unlock(pThis->pmutUsr);
    return RS_RET_OK;
}

rsRetVal wtpResume(wtp_t *const pool) {
    return resumeQuiesce(pool, 0);
}
rsRetVal wtpResumeCompletion(wtp_t *const pool, const uint64_t generation) {
    if (generation == 0) return RS_RET_PARAM_ERROR;
    return resumeQuiesce(pool, generation);
}

void wtpWorkerQuiesce(wtp_t *const pThis, pthread_cond_t *const pcondBusy) {
    int previousCancelState;
    const uint64_t generation = pThis->quiesceGeneration;

    /* The regular worker contract already has cancellation disabled here. Do
     * it explicitly so a future caller cannot strand iNumQuiesced at this
     * pthread cancellation point. */
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &previousCancelState);
    d_pthread_mutex_lock(&pThis->mutWtp);
    ++pThis->iNumQuiesced;
    pthread_cond_broadcast(&pThis->condQuiesced);
    d_pthread_mutex_unlock(&pThis->mutWtp);

    while ((wtpState_t)ATOMIC_LOAD_32BIT((int *)&pThis->wtpState, &pThis->mutWtpState) == wtpState_QUIESCE &&
           pThis->quiesceGeneration == generation)
        d_pthread_cond_wait(pcondBusy, pThis->pmutUsr);

    d_pthread_mutex_lock(&pThis->mutWtp);
    --pThis->iNumQuiesced;
    pthread_cond_broadcast(&pThis->condQuiesced);
    d_pthread_mutex_unlock(&pThis->mutWtp);
    pthread_setcancelstate(previousCancelState, NULL);
}

/* The queue mutex serializes the request snapshot. pfObjProcessed owns the
 * source lease and may release that mutex for deferred msgDestruct. Never
 * acknowledge the saved request unless it is still current on return. */
void wtpWorkerQuiesceAtBoundary(wtp_t *const pool, wti_t *const worker) {
    int savedCancelState;
    if (!pool->completionQuiesce) {
        wtpWorkerQuiesce(pool, &worker->pcondBusy);
        return;
    }
    const uint64_t generation = pool->quiesceGeneration;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &savedCancelState);
    const int ownsBatch = worker->source_queue != NULL || worker->logical_owner != NULL || worker->batch.nElem != 0 ||
                          worker->batch.nElemDeq != 0 || worker->batch.storeData != NULL ||
                          worker->n_deferred_msgs != 0;
    const rsRetVal result = ownsBatch ? pool->pfObjProcessed(pool->pUsr, worker) : RS_RET_OK;
    d_pthread_mutex_lock(&pool->mutWtp);
    const int current = pool->completionQuiesce && pool->quiesceGeneration == generation &&
                        (wtpState_t)ATOMIC_LOAD_32BIT((int *)&pool->wtpState, &pool->mutWtpState) == wtpState_QUIESCE;
    if (!current) {
        d_pthread_mutex_unlock(&pool->mutWtp);
        pthread_setcancelstate(savedCancelState, NULL);
        return;
    }
    if (result != RS_RET_OK && pool->completionQuiesceResult == RS_RET_OK) pool->completionQuiesceResult = result;
    ++pool->iNumCompletionQuiesced;
    ++pool->iNumQuiesced;
    pthread_cond_broadcast(&pool->condQuiesced);
    d_pthread_mutex_unlock(&pool->mutWtp);
    while ((wtpState_t)ATOMIC_LOAD_32BIT((int *)&pool->wtpState, &pool->mutWtpState) == wtpState_QUIESCE &&
           pool->quiesceGeneration == generation && pool->completionQuiesce)
        d_pthread_cond_wait(&worker->pcondBusy, pool->pmutUsr);
    d_pthread_mutex_lock(&pool->mutWtp);
    --pool->iNumCompletionQuiesced;
    --pool->iNumQuiesced;
    pthread_cond_broadcast(&pool->condQuiesced);
    d_pthread_mutex_unlock(&pool->mutWtp);
    pthread_setcancelstate(savedCancelState, NULL);
}
