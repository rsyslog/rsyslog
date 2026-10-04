/* Exercise the production prepared-listener handshake, not a duplicate model.
 * The gate must acknowledge readiness without authorizing dispatch; commit
 * releases it, while cancellation and initialization failure reach the owner.
 * Expired deadlines deterministically exercise timeout without sleeps, and a
 * gated setup thread proves a late-ready report cannot override cancellation.
 * All server threads are joined before state teardown. This tests the shared
 * state machine only, not actual poll/epoll socket registration. */
#include "config.h"
#include <stdio.h>
#include "rsyslog.h"
#include "../../runtime/tcpsrv-prepared.h"

typedef struct worker_s {
    tcpsrvPrepared_t activation;
    pthread_mutex_t setupMut;
    pthread_cond_t setupCond;
    int setupReleased;
    int afterGate;
    rsRetVal setupResult;
} worker_t;

static void *serverThread(void *context) {
    worker_t *worker = context;
    pthread_mutex_lock(&worker->setupMut);
    while (!worker->setupReleased) pthread_cond_wait(&worker->setupCond, &worker->setupMut);
    pthread_mutex_unlock(&worker->setupMut);
    rsRetVal ret = worker->setupResult;
    if (ret == RS_RET_OK) ret = tcpsrvPreparedGate(&worker->activation);
    /* Models dispatch only after the production gate authorizes the backend. */
    if (ret == RS_RET_OK) worker->afterGate = 1;
    tcpsrvPreparedFinish(&worker->activation, ret);
    return NULL;
}

static int runCase(int cancel, int setupFailure, int timeout) {
    worker_t worker = {0};
    pthread_t thread;
    struct timespec deadline;
    int failed = 0;
    if (tcpsrvPreparedInit(&worker.activation) != RS_RET_OK) return 1;
    if (pthread_mutex_init(&worker.setupMut, NULL) != 0) {
        tcpsrvPreparedDestroy(&worker.activation);
        return 1;
    }
    if (pthread_cond_init(&worker.setupCond, NULL) != 0) {
        pthread_mutex_destroy(&worker.setupMut);
        tcpsrvPreparedDestroy(&worker.activation);
        return 1;
    }
    worker.setupReleased = !timeout;
    worker.setupResult = setupFailure ? RS_RET_IO_ERROR : RS_RET_OK;
    if (clock_gettime(CLOCK_REALTIME, &deadline) != 0) {
        failed = 1;
        goto cleanup;
    }
    /* The future deadline is solely a hang guard. An already-expired deadline
     * tests timeout while setup is held by an explicit condition predicate. */
    deadline.tv_sec += timeout ? -1 : 5;
    if (pthread_create(&thread, NULL, serverThread, &worker) != 0) {
        failed = 1;
        goto cleanup;
    }
    const rsRetVal waitResult = tcpsrvPreparedWait(&worker.activation, &deadline);
    if (timeout) {
        failed |= waitResult != RS_RET_TIMEOUT;
        pthread_mutex_lock(&worker.setupMut);
        worker.setupReleased = 1;
        pthread_cond_broadcast(&worker.setupCond);
        pthread_mutex_unlock(&worker.setupMut);
    } else if (setupFailure) {
        failed |= waitResult != RS_RET_IO_ERROR;
    } else {
        failed |= waitResult != RS_RET_OK;
        pthread_mutex_lock(&worker.activation.mut);
        failed |= !worker.activation.ready || worker.activation.authorized || worker.activation.finished;
        pthread_mutex_unlock(&worker.activation.mut);
        if (cancel || failed)
            tcpsrvPreparedCancel(&worker.activation);
        else
            tcpsrvPreparedAuthorize(&worker.activation);
    }
    failed |= pthread_join(thread, NULL) != 0;
    failed |= !worker.activation.finished;
    const rsRetVal expected = setupFailure ? RS_RET_IO_ERROR : (cancel || timeout ? RS_RET_FORCE_TERM : RS_RET_OK);
    failed |= worker.activation.result != expected;
    failed |= worker.afterGate != (!cancel && !timeout && !setupFailure);
    if (timeout || setupFailure) failed |= worker.activation.ready; /* no late/failed readiness */
cleanup:
    pthread_cond_destroy(&worker.setupCond);
    pthread_mutex_destroy(&worker.setupMut);
    tcpsrvPreparedDestroy(&worker.activation);
    if (failed)
        fprintf(stderr, "prepared handshake failed: cancel=%d init-error=%d timeout=%d\n", cancel, setupFailure,
                timeout);
    return failed;
}

int main(void) {
    int failed = 0;
    failed |= runCase(0, 0, 0); /* activation-ready -> committed/dispatch */
    failed |= runCase(1, 0, 0); /* activation-ready -> abort/join */
    failed |= runCase(0, 1, 0); /* setup error -> exact result, no ready */
    failed |= runCase(0, 0, 1); /* timeout -> abort -> late setup -> join */
    return failed;
}
