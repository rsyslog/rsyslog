/* Non-destructive worker-pool batch-boundary barrier. */
#include "config.h"

#include "rsyslog.h"
#include "atomic.h"
#include "wti.h"
#include "wtp.h"

rsRetVal wtpRequestQuiesceLocked(wtp_t *const pThis) {
    rsRetVal ret = RS_RET_OK;

    if (pThis == NULL || pThis->pmutUsr == NULL) return RS_RET_PARAM_ERROR;
    d_pthread_mutex_lock(&pThis->mutWtp);
    const wtpState_t state = (wtpState_t)ATOMIC_LOAD_32BIT((int *)&pThis->wtpState, &pThis->mutWtpState);
    if (state != wtpState_RUNNING) {
        ret = RS_RET_NO_RUN;
    } else {
        ATOMIC_STORE_32BIT((int *)&pThis->wtpState, &pThis->mutWtpState, wtpState_QUIESCE);
        for (int i = 0; i < pThis->iNumWorkerThreads; ++i) pthread_cond_signal(&pThis->pWrkr[i]->pcondBusy);
    }
    d_pthread_mutex_unlock(&pThis->mutWtp);
    return ret;
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

rsRetVal wtpResumeLocked(wtp_t *const pThis) {
    if (pThis == NULL || pThis->pmutUsr == NULL) return RS_RET_PARAM_ERROR;
    d_pthread_mutex_lock(&pThis->mutWtp);
    const wtpState_t state = (wtpState_t)ATOMIC_LOAD_32BIT((int *)&pThis->wtpState, &pThis->mutWtpState);
    if (state == wtpState_QUIESCE) ATOMIC_STORE_32BIT((int *)&pThis->wtpState, &pThis->mutWtpState, wtpState_RUNNING);
    for (int i = 0; i < pThis->iNumWorkerThreads; ++i) pthread_cond_signal(&pThis->pWrkr[i]->pcondBusy);
    d_pthread_mutex_unlock(&pThis->mutWtp);
    return RS_RET_OK;
}

void wtpWorkerQuiesce(wtp_t *const pThis, pthread_cond_t *const pcondBusy) {
    int previousCancelState;

    /* The regular worker contract already has cancellation disabled here. Do
     * it explicitly so a future caller cannot strand iNumQuiesced at this
     * pthread cancellation point. */
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &previousCancelState);
    d_pthread_mutex_lock(&pThis->mutWtp);
    ++pThis->iNumQuiesced;
    pthread_cond_broadcast(&pThis->condQuiesced);
    d_pthread_mutex_unlock(&pThis->mutWtp);

    while ((wtpState_t)ATOMIC_LOAD_32BIT((int *)&pThis->wtpState, &pThis->mutWtpState) == wtpState_QUIESCE)
        d_pthread_cond_wait(pcondBusy, pThis->pmutUsr);

    d_pthread_mutex_lock(&pThis->mutWtp);
    --pThis->iNumQuiesced;
    pthread_cond_broadcast(&pThis->condQuiesced);
    d_pthread_mutex_unlock(&pThis->mutWtp);
    pthread_setcancelstate(previousCancelState, NULL);
}
