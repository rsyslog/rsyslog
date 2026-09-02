/* wtp.c
 *
 * This file implements the worker thread pool (wtp) class.
 *
 * File begun on 2008-01-20 by RGerhards
 *
 * There is some in-depth documentation available in doc/dev_queue.html
 * (and in the web doc set on https://www.rsyslog.com/doc/). Be sure to read it
 * if you are getting acquainted to the object.
 *
 * Copyright 2008-2026 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
 *
 * The rsyslog runtime library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The rsyslog runtime library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the rsyslog runtime library.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 * A copy of the LGPL can be found in the file "COPYING.LESSER" in this distribution.
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <atomic.h>
#ifdef HAVE_SYS_PRCTL_H
    #include <sys/prctl.h>
#endif

#include "rsyslog.h"
#include "stringbuf.h"
#include "srUtils.h"
#include "wtp.h"
#include "wti.h"
#include "obj.h"
#include "unicode-helper.h"
#include "glbl.h"
#include "errmsg.h"

/* static data */
DEFobjStaticHelpers;
DEFobjCurrIf(glbl)

    /* forward-definitions */

    /* methods */

    /* get the header for debug messages
     * The caller must NOT free or otherwise modify the returned string!
     */
    static uchar *wtpGetDbgHdr(wtp_t *pThis) {
    ISOBJ_TYPE_assert(pThis, wtp);

    if (pThis->pszDbgHdr == NULL)
        return (uchar *)"wtp"; /* should not normally happen */
    else
        return pThis->pszDbgHdr;
}


/* Not implemented dummy function for constructor */
static rsRetVal NotImplementedDummy_voidp_int(__attribute__((unused)) void *p1, __attribute__((unused)) int p2) {
    return RS_RET_NOT_IMPLEMENTED;
}
static rsRetVal NotImplementedDummy_voidp_intp(__attribute__((unused)) void *p1, __attribute__((unused)) int *p2) {
    return RS_RET_NOT_IMPLEMENTED;
}
static rsRetVal NotImplementedDummy_voidp_voidp(__attribute__((unused)) void *p1, __attribute__((unused)) void *p2) {
    return RS_RET_NOT_IMPLEMENTED;
}
static rsRetVal NotImplementedDummy_voidp_wti_tp(__attribute__((unused)) void *p1, __attribute__((unused)) wti_t *p2) {
    return RS_RET_NOT_IMPLEMENTED;
}
/* Standard-Constructor for the wtp object
 */
BEGINobjConstruct(wtp) /* be sure to specify the object type also in END macro! */
    pThis->toFirstWrkShutdown = -2;
    pthread_mutex_init(&pThis->mutWtp, NULL);
    pthread_cond_init(&pThis->condThrdInitDone, NULL);
    pthread_cond_init(&pThis->condThrdTrm, NULL);
    pthread_attr_init(&pThis->attrThrd);
    /* Set thread scheduling policy to default */
#ifdef HAVE_PTHREAD_SETSCHEDPARAM
    pthread_attr_setschedpolicy(&pThis->attrThrd, default_thr_sched_policy);
    pthread_attr_setschedparam(&pThis->attrThrd, &default_sched_param);
    pthread_attr_setinheritsched(&pThis->attrThrd, PTHREAD_EXPLICIT_SCHED);
#endif
    /* set all function pointers to "not implemented" dummy so that we can safely call them */
    pThis->pfChkStopWrkr = (rsRetVal(*)(void *, int))NotImplementedDummy_voidp_int;
    pThis->pfGetDeqBatchSize = (rsRetVal(*)(void *, int *))NotImplementedDummy_voidp_intp;
    pThis->pfDoWork = (rsRetVal(*)(void *, void *))NotImplementedDummy_voidp_voidp;
    pThis->pfObjProcessed = (rsRetVal(*)(void *, wti_t *))NotImplementedDummy_voidp_wti_tp;
    INIT_ATOMIC_HELPER_MUT(pThis->mutCurNumWrkThrd);
    INIT_ATOMIC_HELPER_MUT(pThis->mutWtpState);
ENDobjConstruct(wtp)


/* Construction finalizer
 * rgerhards, 2008-01-17
 */
rsRetVal wtpConstructFinalize(wtp_t *pThis) {
    DEFiRet;
    int i;
    uchar pszBuf[64];
    size_t lenBuf;
    wti_t *pWti;

    ISOBJ_TYPE_assert(pThis, wtp);

    DBGPRINTF("%s: finalizing construction of worker thread pool (numworkerThreads %d)\n", wtpGetDbgHdr(pThis),
              pThis->iNumWorkerThreads);
    /* alloc and construct workers - this can only be done in finalizer as we previously do
     * not know the max number of workers
     */
    CHKmalloc(pThis->pWrkr = malloc(sizeof(wti_t *) * pThis->iNumWorkerThreads));

    for (i = 0; i < pThis->iNumWorkerThreads; ++i) {
        CHKiRet(wtiConstruct(&pThis->pWrkr[i]));
        pWti = pThis->pWrkr[i];
        lenBuf = snprintf((char *)pszBuf, sizeof(pszBuf), "%.*s/w%d",
                          (int)(sizeof(pszBuf) - 6), /* leave 6 chars for \0, "/w" and number: */
                          wtpGetDbgHdr(pThis), i);
        if (lenBuf >= sizeof(pszBuf)) {
            LogError(0, RS_RET_INTERNAL_ERROR,
                     "%s:%d debug header too long: %zd - in "
                     "thory this cannot happen - truncating",
                     __FILE__, __LINE__, lenBuf);
            lenBuf = sizeof(pszBuf) - 1;
            pszBuf[lenBuf] = '\0';
        }
        CHKiRet(wtiSetDbgHdr(pWti, pszBuf, lenBuf));
        CHKiRet(wtiSetpWtp(pWti, pThis));
        pWti->workerIndex = i;
        CHKiRet(wtiConstructFinalize(pWti));
    }


finalize_it:
    RETiRet;
}


/* Destructor */
BEGINobjDestruct(wtp) /* be sure to specify the object type also in END and CODESTART macros! */
    int i;
    CODESTARTobjDestruct(wtp);
    d_pthread_mutex_lock(&pThis->mutWtp); /* make sure nobody is still using the mutex */
    assert(pThis->iCurNumWrkThrd == 0);

    /* destruct workers */
    for (i = 0; i < pThis->iNumWorkerThreads; ++i) wtiDestruct(&pThis->pWrkr[i]);

    free(pThis->pWrkr);
    pThis->pWrkr = NULL;

    /* actual destruction */
    d_pthread_mutex_unlock(&pThis->mutWtp);
    pthread_cond_destroy(&pThis->condThrdTrm);
    pthread_cond_destroy(&pThis->condThrdInitDone);
    pthread_mutex_destroy(&pThis->mutWtp);
    pthread_attr_destroy(&pThis->attrThrd);
    DESTROY_ATOMIC_HELPER_MUT(pThis->mutCurNumWrkThrd);
    DESTROY_ATOMIC_HELPER_MUT(pThis->mutWtpState);

    free(pThis->pszDbgHdr);
ENDobjDestruct(wtp)


/* Set worker thread pool state. -- rgerhards, 2008-01-21
 *
 * This function is called from multiple contexts:
 *   1. During shutdown (wtpShutdownAll, queue destructor)
 *   2. During persistence mode changes (queuePersist)
 *
 * IMPORTANT: Uses atomic store because worker threads read wtpState atomically
 * in wtpChkStopWrkr(). Mixing plain stores with atomic loads violates the C11
 * memory model and creates undefined behavior.
 *
 * The atomic store ensures:
 *   - Immediate visibility to all worker threads
 *   - SHUTDOWN_IMMEDIATE is seen without delay
 *   - Proper memory barriers on all architectures (including ARM)
 *
 * rgerhards, 2008-01-21
 */
rsRetVal wtpSetState(wtp_t *pThis, wtpState_t iNewState) {
    ISOBJ_TYPE_assert(pThis, wtp);
    ATOMIC_STORE_32BIT((int *)&pThis->wtpState, &pThis->mutWtpState, iNewState);
    return RS_RET_OK;
}

/* join terminated worker threads */
static void ATTR_NONNULL() wtpJoinTerminatedWrkr(wtp_t *const pThis) {
    int i;
    for (i = 0; i < pThis->iNumWorkerThreads; ++i) {
        wtiJoinThrd(pThis->pWrkr[i]);
    }
}


/* check if the worker shall shutdown (1 = yes, 0 = no)
 * Note: there may be two mutexes locked, the bLockUsrMutex is the one in our "user"
 * (e.g. the queue clas)
 * rgerhards, 2008-01-21
 */
rsRetVal wtpChkStopWrkr(wtp_t *pThis, int bLockUsrMutex) {
    DEFiRet;
    wtpState_t wtpState;

    ISOBJ_TYPE_assert(pThis, wtp);
    /* we need a consistent value, but it doesn't really matter if it is changed
     * right after the load - then we simply do one more iteration in the worker
     */
    wtpState = (wtpState_t)ATOMIC_LOAD_32BIT((int *)&pThis->wtpState, &pThis->mutWtpState);

    if (wtpState == wtpState_SHUTDOWN_IMMEDIATE) {
        ABORT_FINALIZE(RS_RET_TERMINATE_NOW);
    } else if (wtpState == wtpState_SHUTDOWN) {
        ABORT_FINALIZE(RS_RET_TERMINATE_WHEN_IDLE);
    }

    /* try customer handler if one was set and we do not yet have a definite result */
    if (pThis->pfChkStopWrkr != NULL) {
        iRet = pThis->pfChkStopWrkr(pThis->pUsr, bLockUsrMutex);
    }

finalize_it:
    RETiRet;
}


PRAGMA_DIAGNOSTIC_PUSH
PRAGMA_IGNORE_Wempty_body
    /* Send a shutdown command to all workers and see if they terminate.
     * A timeout may be specified. This function may also be called with
     * the current number of workers being 0, in which case it does not
     * shut down any worker.
     * rgerhards, 2008-01-14
     */
    rsRetVal
    ATTR_NONNULL() wtpShutdownAll(wtp_t *pThis, wtpState_t tShutdownCmd, struct timespec *ptTimeout) {
    DEFiRet;
    int bTimedOut;
    int i;

    ISOBJ_TYPE_assert(pThis, wtp);

    /* lock mutex to prevent races (may otherwise happen during idle processing and such...) */
    d_pthread_mutex_lock(pThis->pmutUsr);
    wtpSetState(pThis, tShutdownCmd);
    /* awake workers in retry loop */
    for (i = 0; i < pThis->iNumWorkerThreads; ++i) {
        wtpJoinTerminatedWrkr(pThis);
        pthread_cond_signal(&pThis->pWrkr[i]->pcondBusy);
        wtiWakeupThrd(pThis->pWrkr[i]);
    }
    d_pthread_mutex_unlock(pThis->pmutUsr);

    /* wait for worker thread termination */
    d_pthread_mutex_lock(&pThis->mutWtp);
    pthread_cleanup_push(mutexCancelCleanup, &pThis->mutWtp);
    bTimedOut = 0;
    /* Use atomic read for consistency - iCurNumWrkThrd is updated atomically by
     * worker threads. Even though we hold mutWtp, workers update this counter
     * atomically, so we must read it atomically to ensure we see the latest value.
     */
    while (ATOMIC_LOAD_32BIT(&pThis->iCurNumWrkThrd, &pThis->mutCurNumWrkThrd) > 0 && !bTimedOut) {
        wtpJoinTerminatedWrkr(pThis);
        DBGPRINTF("%s: waiting %ldms on worker thread termination, %d still running\n", wtpGetDbgHdr(pThis),
                  timeoutVal(ptTimeout), ATOMIC_LOAD_32BIT_RELAXED(&pThis->iCurNumWrkThrd, &pThis->mutCurNumWrkThrd));

        if (d_pthread_cond_timedwait(&pThis->condThrdTrm, &pThis->mutWtp, ptTimeout) != 0) {
            DBGPRINTF("%s: timeout waiting on worker thread termination\n", wtpGetDbgHdr(pThis));
            bTimedOut = 1; /* we exit the loop on timeout */
        }

        /* awake workers in retry loop */
        for (i = 0; i < pThis->iNumWorkerThreads; ++i) {
            wtiWakeupThrd(pThis->pWrkr[i]);
        }
    }
    /* The final worker can transition to WAIT_JOIN and decrement the active
     * count to zero just before the loop exits. Join it now so destructors see
     * the stable STOPPED state instead of a joinable terminated worker. */
    wtpJoinTerminatedWrkr(pThis);
    pthread_cleanup_pop(1);

    if (bTimedOut) iRet = RS_RET_TIMED_OUT;

    RETiRet;
}
PRAGMA_DIAGNOSTIC_POP


/* Unconditionally cancel all running worker threads.
 * rgerhards, 2008-01-14
 */
rsRetVal ATTR_NONNULL() wtpCancelAll(wtp_t *pThis, const uchar *const cancelobj) {
    DEFiRet;
    int i;

    ISOBJ_TYPE_assert(pThis, wtp);

    /* go through all workers and cancel those that are active */
    for (i = 0; i < pThis->iNumWorkerThreads; ++i) {
        wtiCancelThrd(pThis->pWrkr[i], cancelobj);
    }

    RETiRet;
}


/* this function contains shared code for both regular worker shutdown as
 * well as shutdown via cancellation. We can not simply use pthread_cleanup_pop(1)
 * as this introduces a race in the debug system (RETiRet system).
 * rgerhards, 2009-10-26
 */
static void wtpWrkrExecCleanup(wti_t *pWti) {
    wtp_t *pThis;

    ISOBJ_TYPE_assert(pWti, wti);
    pThis = pWti->pWtp;
    ISOBJ_TYPE_assert(pThis, wtp);

    // TESTBENCH bughunt - remove when done! 2018-11-05 rgerhards
    if (dbgTimeoutToStderr) {
        fprintf(stderr, "rsyslog debug: %s: enter WrkrExecCleanup\n", wtiGetDbgHdr(pWti));
    }
    /* the order of the next two statements is important! */
    wtiSetState(pWti, WRKTHRD_WAIT_JOIN);
    ATOMIC_DEC(&pThis->iCurNumWrkThrd, &pThis->mutCurNumWrkThrd);

    /* note: numWorkersNow is only for message generation, so we do not try
     * hard to get it 100% accurate (as curently done, it is not).
     */
    const int numWorkersNow = ATOMIC_LOAD_32BIT_RELAXED(&pThis->iCurNumWrkThrd, &pThis->mutCurNumWrkThrd);
    DBGPRINTF("%s: Worker thread %lx, terminated, num workers now %d\n", wtpGetDbgHdr(pThis), (unsigned long)pWti,
              numWorkersNow);
    if (numWorkersNow > 0) {
        // TODO: did the thread ID experiment (pthread_self) work out? rgerhards, 2024-07-25
        LogMsg(0, RS_RET_OPERATION_STATUS, LOG_INFO,
               "%s: worker thread %lx (%" PRIuPTR ") terminated, now %d active worker threads", wtpGetDbgHdr(pThis),
               (unsigned long)pWti, (uintptr_t)pthread_self(), numWorkersNow);
    }
}


/* cancellation cleanup handler for executing worker decrements the worker counter.
 * rgerhards, 2009-07-20
 */
static void wtpWrkrExecCancelCleanup(void *arg) {
    wti_t *pWti = (wti_t *)arg;
    wtp_t *pThis;

    ISOBJ_TYPE_assert(pWti, wti);
    pThis = pWti->pWtp;
    ISOBJ_TYPE_assert(pThis, wtp);
    DBGPRINTF("%s: Worker thread %lx requested to be cancelled.\n", wtpGetDbgHdr(pThis), (unsigned long)pWti);

    wtpWrkrExecCleanup(pWti);

    pthread_cond_broadcast(&pThis->condThrdTrm); /* activate anyone waiting on thread shutdown */
}


/* wtp worker shell. This is started and calls into the actual
 * wti worker.
 * rgerhards, 2008-01-21
 */
PRAGMA_DIAGNOSTIC_PUSH
PRAGMA_IGNORE_Wempty_body static void *wtpWorker(
    void *arg) /* the arg is actually a wti object, even though we are in wtp! */
{
    wti_t *pWti = (wti_t *)arg;
    wtp_t *pThis;
    sigset_t sigSet;
#if defined(HAVE_PRCTL) && defined(PR_SET_NAME)
    uchar *pszDbgHdr;
    uchar thrdName[32] = "rs:";
#endif

    ISOBJ_TYPE_assert(pWti, wti);
    pThis = pWti->pWtp;
    ISOBJ_TYPE_assert(pThis, wtp);

    /* block all signals except SIGTTIN and SIGSEGV */
    sigfillset(&sigSet);
    sigdelset(&sigSet, SIGTTIN);
    sigdelset(&sigSet, SIGSEGV);
    pthread_sigmask(SIG_BLOCK, &sigSet, NULL);

    /* Avoid a startup race: cancellation can be requested very early during
     * shutdown. If that happens before our cleanup handler is registered,
     * the thread can exit without transitioning state to WAIT_JOIN.
     * Keep cancellation disabled until cleanup registration is complete.
     */
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_cleanup_push(wtpWrkrExecCancelCleanup, pWti);

#if defined(HAVE_PRCTL) && defined(PR_SET_NAME)
    /* set thread name - we ignore if the call fails, has no harsh consequences... */
    pszDbgHdr = wtpGetDbgHdr(pThis);
    ustrncpy(thrdName + 3, pszDbgHdr, 20);
    if (prctl(PR_SET_NAME, thrdName, 0, 0, 0) != 0) {
        DBGPRINTF("prctl failed, not setting thread name for '%s'\n", wtpGetDbgHdr(pThis));
    }
    dbgOutputTID((char *)thrdName);
#endif

    // TESTBENCH bughunt - remove when done! 2018-11-05 rgerhards
    if (dbgTimeoutToStderr) {
        fprintf(stderr, "rsyslog debug: %s: worker %p started\n", wtpGetDbgHdr(pThis), pThis);
    }
    /* let the parent know we're done with initialization */
    d_pthread_mutex_lock(&pThis->mutWtp);
    /* Startup invariant: once a worker is visible as RUNNING, cancellation
     * cleanup is already installed and will transition it to WAIT_JOIN.
     */
    wtiSetState(pWti, WRKTHRD_RUNNING);
    pthread_cond_broadcast(&pThis->condThrdInitDone);
    d_pthread_mutex_unlock(&pThis->mutWtp);

    wtiWorker(pWti);
    pthread_cleanup_pop(0);
    d_pthread_mutex_lock(&pThis->mutWtp);
    pthread_cleanup_push(mutexCancelCleanup, &pThis->mutWtp);
    wtpWrkrExecCleanup(pWti);

    pthread_cond_broadcast(&pThis->condThrdTrm); /* activate anyone waiting on thread shutdown */
    pthread_cleanup_pop(1); /* unlock mutex */
    if (dbgTimeoutToStderr) {
        fprintf(stderr, "rsyslog debug: %p: worker exiting\n", pWti);
    }
    pthread_exit(0);
    return NULL; /* To suppress warning */
}
PRAGMA_DIAGNOSTIC_POP

/* start a new worker */
/* Caller holds mutWtp.  Keeping slot allocation and the INITIALIZING count
 * under this one lock makes concurrent ensure requests respect the target. */
static rsRetVal ATTR_NONNULL() wtpStartWrkrLocked(wtp_t *const pThis, const int permit_during_shutdown) {
    wti_t *pWti;
    int i;
    int iState;
    DEFiRet;

    ISOBJ_TYPE_assert(pThis, wtp);

    // TESTBENCH bughunt - remove when done! 2018-11-05 rgerhards
    if (dbgTimeoutToStderr) {
        fprintf(stderr, "%s: worker start requested, num workers currently %d\n", wtpGetDbgHdr(pThis),
                ATOMIC_LOAD_32BIT_RELAXED(&pThis->iCurNumWrkThrd, &pThis->mutCurNumWrkThrd));
    }

    /* Check state under mutex to prevent TOCTOU race where worker could be
     * created during shutdown. Must hold mutWtp to ensure state doesn't change
     * between check and worker creation.
     *
     * Note: An atomic read is required as wtpState is protected by its own
     * lock (mutWtpState). This lock on mutWtp ensures the check and
     * subsequent worker creation are an atomic unit, fixing the TOCTOU race.
     */
    const wtpState_t wtpState = (wtpState_t)ATOMIC_LOAD_32BIT((int *)&pThis->wtpState, &pThis->mutWtpState);
    if (wtpState != wtpState_RUNNING && !permit_during_shutdown) {
        DBGPRINTF("%s: worker start requested during shutdown - ignored\n", wtpGetDbgHdr(pThis));
        if (dbgTimeoutToStderr) {
            fprintf(stderr, "rsyslog debug: %s: worker start requested during shutdown - ignored\n",
                    wtpGetDbgHdr(pThis));
        }
        return RS_RET_ERR; /* exceptional case, but really makes sense here! */
    }

    wtpJoinTerminatedWrkr(pThis);
    /* find free spot in thread table. */
    for (i = 0; i < pThis->iNumWorkerThreads; ++i) {
        if (wtiCASState(pThis->pWrkr[i], WRKTHRD_STOPPED, WRKTHRD_INITIALIZING) == RS_RET_OK) {
            break;
        }
    }

    if (i == pThis->iNumWorkerThreads) ABORT_FINALIZE(RS_RET_NO_MORE_THREADS);

    const sbool first_worker_runs_forever =
        i == 0 && (!pThis->bAllowFirstWorkerToTimeout || pThis->toFirstWrkShutdown == -1 ||
                   (pThis->toFirstWrkShutdown == -2 && pThis->toWrkShutdown == -1));
    if (first_worker_runs_forever || (i != 0 && pThis->toWrkShutdown == -1)) {
        wtiSetAlwaysRunning(pThis->pWrkr[i]);
    }

    pWti = pThis->pWrkr[i];
    iState = pthread_create(&(pWti->thrdID), &pThis->attrThrd, wtpWorker, (void *)pWti);
    if (iState != 0) {
        wtiSetState(pWti, WRKTHRD_STOPPED);
        LogError(iState, RS_RET_ERR, "%s: pthread_create failed while starting a worker", wtpGetDbgHdr(pThis));
        ABORT_FINALIZE(RS_RET_ERR);
    }
    ATOMIC_INC(&pThis->iCurNumWrkThrd, &pThis->mutCurNumWrkThrd); /* we got one more! */

    // TESTBENCH bughunt - remove when done! 2018-11-05 rgerhards
    if (dbgTimeoutToStderr) {
        fprintf(stderr, "%s: wrkr start initiated with state %d, num workers now %d\n", wtpGetDbgHdr(pThis), iState,
                ATOMIC_LOAD_32BIT_RELAXED(&pThis->iCurNumWrkThrd, &pThis->mutCurNumWrkThrd));
    }
    DBGPRINTF("%s: started with state %d, num workers now %d\n", wtpGetDbgHdr(pThis), iState,
              ATOMIC_LOAD_32BIT_RELAXED(&pThis->iCurNumWrkThrd, &pThis->mutCurNumWrkThrd));

    /* wait for the new thread to initialize its signal mask and
     * cancellation cleanup handler before proceeding
     */
    do {
        d_pthread_cond_wait(&pThis->condThrdInitDone, &pThis->mutWtp);
    } while ((iState = wtiGetState(pWti)) == WRKTHRD_INITIALIZING);
    DBGPRINTF("%s: new worker finished initialization with state %d, num workers now %d\n", wtpGetDbgHdr(pThis), iState,
              ATOMIC_LOAD_32BIT_RELAXED(&pThis->iCurNumWrkThrd, &pThis->mutCurNumWrkThrd));
    // TESTBENCH bughunt - remove when done! 2018-11-05 rgerhards
    if (dbgTimeoutToStderr) {
        fprintf(stderr, "rsyslog debug: %s: started with state %d, num workers now %d\n", wtpGetDbgHdr(pThis), iState,
                ATOMIC_LOAD_32BIT_RELAXED(&pThis->iCurNumWrkThrd, &pThis->mutCurNumWrkThrd));
    }

finalize_it:
    RETiRet;
}


/* Idle-list operations are called with pmutUsr locked. This is deliberately
 * independent of mutWtp: producer wakeups must not contend with creates,
 * joins, or pool lifecycle operations. */
static void ATTR_NONNULL() wtpIdleAddLocked(wtp_t *const pThis, wti_t *const pWti) {
    assert(pWti->bIdleListed == 0);
    pWti->pIdlePrev = NULL;
    pWti->pIdleNext = pThis->pIdleWorkers;
    if (pThis->pIdleWorkers != NULL) pThis->pIdleWorkers->pIdlePrev = pWti;
    pThis->pIdleWorkers = pWti;
    pWti->bIdleListed = 1;
    ++pThis->nIdleWorkers;
}

static void ATTR_NONNULL() wtpIdleRemoveLocked(wtp_t *const pThis, wti_t *const pWti) {
    if (!pWti->bIdleListed) return; /* already reserved by a producer */
    if (pWti->pIdlePrev != NULL)
        pWti->pIdlePrev->pIdleNext = pWti->pIdleNext;
    else
        pThis->pIdleWorkers = pWti->pIdleNext;
    if (pWti->pIdleNext != NULL) pWti->pIdleNext->pIdlePrev = pWti->pIdlePrev;
    pWti->pIdlePrev = NULL;
    pWti->pIdleNext = NULL;
    pWti->bIdleListed = 0;
    --pThis->nIdleWorkers;
}

static void wtpIdleWaitCancelCleanup(void *arg) {
    wti_t *const pWti = (wti_t *)arg;
    wtpIdleRemoveLocked(pWti->pWtp, pWti);
}

int wtpWaitForWork(wti_t *const pWti, const struct timespec *const timeout) {
    wtp_t *const pWtp = pWti->pWtp;
    int r;

    wtpIdleAddLocked(pWtp, pWti);
    pthread_cleanup_push(wtpIdleWaitCancelCleanup, pWti);
    if (timeout == NULL)
        r = d_pthread_cond_wait(&pWti->pcondBusy, pWtp->pmutUsr);
    else
        r = d_pthread_cond_timedwait(&pWti->pcondBusy, pWtp->pmutUsr, timeout);
    pthread_cleanup_pop(0);
    wtpIdleRemoveLocked(pWtp, pWti);
    return r;
}

rsRetVal ATTR_NONNULL() wtpWakeIdleWorkers(wtp_t *const pThis, int nMaxWrkr) {
    ISOBJ_TYPE_assert(pThis, wtp);
    while (nMaxWrkr-- > 0 && pThis->pIdleWorkers != NULL) {
        wti_t *const pWti = pThis->pIdleWorkers;
        wtpIdleRemoveLocked(pThis, pWti);
        pthread_cond_signal(&pWti->pcondBusy);
    }
    return RS_RET_OK;
}

/* Start missing slots after the producer released pmutUsr. INITIALIZING slots
 * increment iCurNumWrkThrd before the creator waits for initialization, so
 * concurrent ensure requests cannot exceed the configured maximum. */
rsRetVal ATTR_NONNULL() wtpEnsureWorkers(wtp_t *const pThis, int nMaxWrkr, const int permit_during_shutdown) {
    DEFiRet;
    int curNumWrkThrd;

    ISOBJ_TYPE_assert(pThis, wtp);
    if (nMaxWrkr <= 0) return RS_RET_OK;
    if (nMaxWrkr > pThis->iNumWorkerThreads) nMaxWrkr = pThis->iNumWorkerThreads;
    d_pthread_mutex_lock(&pThis->mutWtp);
    while ((curNumWrkThrd = ATOMIC_LOAD_32BIT(&pThis->iCurNumWrkThrd, &pThis->mutCurNumWrkThrd)) < nMaxWrkr) {
        if (curNumWrkThrd > 0) {
            LogMsg(0, RS_RET_OPERATION_STATUS, LOG_INFO,
                   "%s: high activity - starting an additional worker thread, currently %d active worker threads.",
                   wtpGetDbgHdr(pThis), curNumWrkThrd);
        }
        CHKiRet(wtpStartWrkrLocked(pThis, permit_during_shutdown));
    }

finalize_it:
    d_pthread_mutex_unlock(&pThis->mutWtp);
    RETiRet;
}


rsRetVal wtpWakeupAllWrkr(wtp_t *pThis) {
    if (pThis == NULL) return RS_RET_PARAM_ERROR;
    for (int i = 0; i < pThis->iNumWorkerThreads; ++i) {
        if (wtiGetState(pThis->pWrkr[i]) != WRKTHRD_STOPPED) pthread_cond_signal(&pThis->pWrkr[i]->pcondBusy);
    }
    return RS_RET_OK;
}


/* some simple object access methods
 * Note: the semicolons behind the macros are actually empty declarations. This is
 * a work-around for clang-format's missing understanding of generative macros.
 * Some compilers may flag this empty declarations by a warning. If so, we need
 * to disable this warning. Alternatively, we could exclude this code from being
 * reformatted by clang-format;
 */
DEFpropSetMeth(wtp, toWrkShutdown, long);
DEFpropSetMeth(wtp, toFirstWrkShutdown, long);
DEFpropSetMeth(wtp, wtpState, wtpState_t);
DEFpropSetMeth(wtp, iNumWorkerThreads, int);
DEFpropSetMeth(wtp, pUsr, void *);
DEFpropSetMethPTR(wtp, pmutUsr, pthread_mutex_t);
DEFpropSetMethFP(wtp, pfChkStopWrkr, rsRetVal (*pVal)(void *, int));
DEFpropSetMethFP(wtp, pfRateLimiter, rsRetVal (*pVal)(void *));
DEFpropSetMethFP(wtp, pfGetDeqBatchSize, rsRetVal (*pVal)(void *, int *));
DEFpropSetMethFP(wtp, pfDoWork, rsRetVal (*pVal)(void *, void *));
DEFpropSetMethFP(wtp, pfObjProcessed, rsRetVal (*pVal)(void *, wti_t *));
DEFpropSetMethFP(wtp, pfIdleTimeout, rsRetVal (*pVal)(void *));
DEFpropSetMeth(wtp, bAllowFirstWorkerToTimeout, sbool);


/* set the debug header message
 * The passed-in string is duplicated. So if the caller does not need
 * it any longer, it must free it. Must be called only before object is finalized.
 * rgerhards, 2008-01-09
 */
rsRetVal wtpSetDbgHdr(wtp_t *pThis, uchar *pszMsg, size_t lenMsg) {
    DEFiRet;

    ISOBJ_TYPE_assert(pThis, wtp);
    assert(pszMsg != NULL);

    if (lenMsg < 1) ABORT_FINALIZE(RS_RET_PARAM_ERROR);

    if (pThis->pszDbgHdr != NULL) {
        free(pThis->pszDbgHdr);
        pThis->pszDbgHdr = NULL;
    }

    if ((pThis->pszDbgHdr = malloc(lenMsg + 1)) == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

    memcpy(pThis->pszDbgHdr, pszMsg, lenMsg + 1); /* always think about the \0! */

finalize_it:
    RETiRet;
}

/* dummy */
static rsRetVal wtpQueryInterface(interface_t __attribute__((unused)) * i) {
    return RS_RET_NOT_IMPLEMENTED;
}

/* exit our class
 */
BEGINObjClassExit(wtp, OBJ_IS_CORE_MODULE) /* CHANGE class also in END MACRO! */
    CODESTARTObjClassExit(wtp);
    /* release objects we no longer need */
    objRelease(glbl, CORE_COMPONENT);
ENDObjClassExit(wtp)


/* Initialize the stream class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-01-09
 */
BEGINObjClassInit(wtp, 1, OBJ_IS_CORE_MODULE)
    /* request objects we use */
    CHKiRet(objUse(glbl, CORE_COMPONENT));
ENDObjClassInit(wtp)
