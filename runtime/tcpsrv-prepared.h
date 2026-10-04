/* Private prepared-listener activation handshake; not a configuration API.
 * Concurrency & Locking: all state is guarded by mut. The owner must join the
 * server thread before destroying this state or its backend/context. */
#ifndef INCLUDED_TCPSRV_PREPARED_H
#define INCLUDED_TCPSRV_PREPARED_H

#include <errno.h>
#include <pthread.h>
#include <time.h>

typedef struct tcpsrvPrepared_s {
    pthread_mutex_t mut;
    pthread_cond_t cond;
    int ready;
    int authorized;
    int cancelled;
    int finished;
    rsRetVal result;
} tcpsrvPrepared_t;

static inline rsRetVal tcpsrvPreparedInit(tcpsrvPrepared_t *state) {
    if (pthread_mutex_init(&state->mut, NULL) != 0) return RS_RET_ERR;
    if (pthread_cond_init(&state->cond, NULL) != 0) {
        pthread_mutex_destroy(&state->mut);
        return RS_RET_ERR;
    }
    state->ready = state->authorized = state->cancelled = state->finished = 0;
    state->result = RS_RET_OK;
    return RS_RET_OK;
}

static inline void tcpsrvPreparedDestroy(tcpsrvPrepared_t *state) {
    pthread_cond_destroy(&state->cond);
    pthread_mutex_destroy(&state->mut);
}

/* Called only before starting a new server thread, never while it runs. */
static inline void tcpsrvPreparedReset(tcpsrvPrepared_t *state, int authorized) {
    pthread_mutex_lock(&state->mut);
    state->ready = state->cancelled = state->finished = 0;
    state->authorized = authorized;
    state->result = RS_RET_OK;
    pthread_mutex_unlock(&state->mut);
}

/* Backend setup calls this before its first I/O wait/dispatch. Cancellation
 * wins over both a late ready report and authorization. */
static inline rsRetVal tcpsrvPreparedGate(void *context) {
    tcpsrvPrepared_t *state = context;
    rsRetVal ret = RS_RET_OK;
    pthread_mutex_lock(&state->mut);
    if (!state->cancelled) {
        state->ready = 1;
        pthread_cond_broadcast(&state->cond);
    }
    while (!state->authorized && !state->cancelled) {
        if (pthread_cond_wait(&state->cond, &state->mut) != 0) {
            state->cancelled = 1;
            ret = RS_RET_ERR;
            break;
        }
    }
    if (state->cancelled && ret == RS_RET_OK) ret = RS_RET_FORCE_TERM;
    pthread_mutex_unlock(&state->mut);
    return ret;
}

static inline void tcpsrvPreparedFinish(tcpsrvPrepared_t *state, rsRetVal result) {
    pthread_mutex_lock(&state->mut);
    state->result = result;
    state->finished = 1;
    pthread_cond_broadcast(&state->cond);
    pthread_mutex_unlock(&state->mut);
}

/* deadline uses CLOCK_REALTIME, matching the portable default cond clock.
 * Timeout is an error/cleanup guard, never an oracle that setup completed. */
static inline rsRetVal tcpsrvPreparedWait(tcpsrvPrepared_t *state, const struct timespec *deadline) {
    rsRetVal ret = RS_RET_OK;
    pthread_mutex_lock(&state->mut);
    while (!state->ready && !state->finished && !state->cancelled) {
        const int r = pthread_cond_timedwait(&state->cond, &state->mut, deadline);
        if (r != 0) {
            ret = r == ETIMEDOUT ? RS_RET_TIMEOUT : RS_RET_ERR;
            break;
        }
    }
    if (ret == RS_RET_OK && state->finished) ret = state->result == RS_RET_OK ? RS_RET_ERR : state->result;
    if (ret == RS_RET_OK && state->cancelled) ret = RS_RET_FORCE_TERM;
    if (ret != RS_RET_OK) {
        state->cancelled = 1;
        pthread_cond_broadcast(&state->cond);
    }
    pthread_mutex_unlock(&state->mut);
    return ret;
}

static inline void tcpsrvPreparedAuthorize(tcpsrvPrepared_t *state) {
    pthread_mutex_lock(&state->mut);
    state->authorized = 1;
    pthread_cond_broadcast(&state->cond);
    pthread_mutex_unlock(&state->mut);
}

static inline void tcpsrvPreparedCancel(tcpsrvPrepared_t *state) {
    pthread_mutex_lock(&state->mut);
    state->cancelled = 1;
    pthread_cond_broadcast(&state->cond);
    pthread_mutex_unlock(&state->mut);
}

#endif
