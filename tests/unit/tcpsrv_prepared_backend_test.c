/* SPDX-License-Identifier: Apache-2.0 */

/*
 * Copyright 2026 Rainer Gerhards and Adiscon GmbH.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 */

/* Exercise the production RunPrepared backend, loaded through object interface
 * v44, with an ephemeral localhost listener and real queued TCP traffic.
 * A gate/condition handshake is the oracle for readiness and zero pre-commit
 * accepts/dispatches; exact framed receive and session destruction prove commit.
 * Abort and real EMFILE setup failure must return through backend cleanup.
 * Once ready, the gate stays parked until the owning test thread releases it;
 * observer/topology deadlines and the external run timeout are hang guards,
 * never success oracles. All started threads are released and joined on
 * assertion failures.
 * Compile/run in both epoll-enabled and epoll-disabled builds: no backend copy
 * or test-only production configuration switch is involved.
 */
#include "config.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "rsyslog.h"
#include "obj.h"
#include "debug.h"
#include "unicode-helper.h"
#include "dnscache.h"
#include "ratelimit.h"
#include "rsconf.h"
#include "glbl.h"
#include "netstrm.h"
#include "tcps_sess.h"
#include "tcpsrv.h"

DEFobjCurrIf(obj);
DEFobjCurrIf(glbl);
DEFobjCurrIf(netstrm);
DEFobjCurrIf(tcps_sess);
DEFobjCurrIf(tcpsrv);

static const char payload[] = "prepared-backend-real-message";
static int joinFailed;

static void reportRuntimeError(const int severity, const int error, const uchar *message) {
    fprintf(stderr, "prepared backend runtime diagnostic severity=%d error=%d: %s\n", severity, error, message);
}

typedef struct testState_s {
    tcpsrv_t *server;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    int ready;
    int release;
    int finished;
    int accepts;
    int receives;
    int destroyed;
    int badPayload;
    int control[2];
    int backend;
    unsigned requestedWorkers;
    unsigned actualWorkerThreads;
    int workerTopologyReady;
    rsRetVal gateResult;
    rsRetVal result;
} testState_t;

/* CHECK is used only by the owning thread and always takes the cleanup path. */
#define CHECK(condition)                                                                  \
    do {                                                                                  \
        if (!(condition)) {                                                               \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
            failed = 1;                                                                   \
            goto cleanup;                                                                 \
        }                                                                                 \
    } while (0)

static struct timespec deadline(void) {
    struct timespec limit = {0};
    if (clock_gettime(CLOCK_REALTIME, &limit) != 0) {
        perror("prepared backend deadline clock");
        return limit;
    }
    limit.tv_sec += 30;
    return limit;
}

static int waitFor(testState_t *state, int *counter) {
    const struct timespec limit = deadline();
    int error = 0;
    pthread_mutex_lock(&state->mutex);
    while (*counter == 0 && !state->finished && error == 0)
        error = pthread_cond_timedwait(&state->condition, &state->mutex, &limit);
    const int found = *counter != 0;
    pthread_mutex_unlock(&state->mutex);
    return found;
}

static int waitForWorkerTopology(testState_t *state) {
    tcpsrv_t *const server = state->server;
    unsigned expectedWorkers = state->requestedWorkers;
#if !defined(ENABLE_IMTCP_EPOLL)
    /* The production poll backend intentionally uses the event-loop thread. */
    expectedWorkers = 1;
#endif
    if (server->workQueue.numWrkr != expectedWorkers) return 0;
    if (expectedWorkers <= 1) return 1;

    /* RunPrepared starts workers before entering the gate but thread scheduling
     * may lag pthread_create(). Wait for each real worker to claim its slot so
     * the two-worker epoll case proves a pool, not just a request for one. */
    const struct timespec limit = deadline();
    const struct timespec retry = {.tv_sec = 0, .tv_nsec = 1000000};
    for (;;) {
        pthread_mutex_lock(&server->workQueue.mut);
        const unsigned started = (unsigned)server->currWrkrs;
        const int poolAllocated =
            server->workQueue.wrkr_tids != NULL && server->workQueue.wrkr_data != NULL && server->fenceItems != NULL;
        pthread_mutex_unlock(&server->workQueue.mut);
        state->actualWorkerThreads = started;
        if (started == expectedWorkers) return poolAllocated;
        if (started > expectedWorkers) return 0;

        struct timespec now;
        if (clock_gettime(CLOCK_REALTIME, &now) != 0 || now.tv_sec > limit.tv_sec ||
            (now.tv_sec == limit.tv_sec && now.tv_nsec >= limit.tv_nsec))
            return 0;
        struct timespec pause = retry;
        while (nanosleep(&pause, &pause) != 0 && errno == EINTR) {
        }
    }
}

static rsRetVal gate(void *context) {
    testState_t *state = context;
    const int workerTopologyReady = waitForWorkerTopology(state);
    int error = 0;
    pthread_mutex_lock(&state->mutex);
    state->control[0] = state->server->controlPipe[0];
    state->control[1] = state->server->controlPipe[1];
#ifdef ENABLE_IMTCP_EPOLL
    state->backend = state->server->evtdata.epoll.efd;
#endif
    state->workerTopologyReady = workerTopologyReady;
    state->ready = 1;
    pthread_cond_broadcast(&state->condition);
    while (!state->release && error == 0) error = pthread_cond_wait(&state->condition, &state->mutex);
    const rsRetVal ret = error == 0 ? state->gateResult : RS_RET_ERR;
    pthread_mutex_unlock(&state->mutex);
    return ret;
}

static void *runPrepared(void *context) {
    testState_t *state = context;
    const rsRetVal ret = tcpsrv.RunPrepared(state->server, gate, state);
    pthread_mutex_lock(&state->mutex);
    state->result = ret;
    state->finished = 1;
    pthread_cond_broadcast(&state->condition);
    pthread_mutex_unlock(&state->mutex);
    return NULL;
}

static int permitted(struct sockaddr *addr, char *host, void *server, void *session) {
    (void)addr;
    (void)host;
    (void)server;
    (void)session;
    return 1;
}

static rsRetVal receiveData(
    tcps_sess_t *session, char *buffer, size_t size, ssize_t *received, int *error, unsigned *direction) {
    *received = size;
    return netstrm.Rcv(session->pStrm, (uchar *)buffer, received, error, direction);
}

static rsRetVal accepted(tcpsrv_t *server, tcps_sess_t *session, char *info) {
    (void)info;
    testState_t *state = server->pUsr;
    session->pUsr = state;
    pthread_mutex_lock(&state->mutex);
    ++state->accepts;
    pthread_cond_broadcast(&state->condition);
    pthread_mutex_unlock(&state->mutex);
    return RS_RET_OK;
}

static rsRetVal received(tcps_sess_t *session, uchar *message, int length) {
    testState_t *state = session->pSrv->pUsr;
    pthread_mutex_lock(&state->mutex);
    state->badPayload |= length != (int)strlen(payload) || memcmp(message, payload, strlen(payload)) != 0;
    ++state->receives;
    pthread_cond_broadcast(&state->condition);
    pthread_mutex_unlock(&state->mutex);
    return RS_RET_OK;
}

static rsRetVal destroyed(void *context) {
    testState_t *state = *(testState_t **)context;
    if (state != NULL) {
        pthread_mutex_lock(&state->mutex);
        ++state->destroyed;
        pthread_cond_broadcast(&state->condition);
        pthread_mutex_unlock(&state->mutex);
    }
    return RS_RET_OK;
}

static rsRetVal regularClose(tcps_sess_t *session) {
    const rsRetVal ret = tcps_sess.PrepareClose(session);
    const rsRetVal closeRet = tcps_sess.Close(session);
    return ret == RS_RET_OK ? closeRet : ret;
}

static rsRetVal makeServer(testState_t *state, unsigned workers, int *listener) {
    tcpLstnParams_t *params = NULL;
    DEFiRet;
    state->requestedWorkers = workers;
    CHKiRet(tcpsrv.Construct(&state->server));
    CHKiRet(tcpsrv.SetOrigin(state->server, UCHAR_CONSTANT("prepared-backend-test")));
    CHKiRet(tcpsrv.SetNumWrkr(state->server, workers));
    CHKiRet(tcpsrv.SetUsrP(state->server, state));
    CHKiRet(tcpsrv.SetCBOpenLstnSocks(state->server, tcpsrv.create_tcp_socket));
    CHKiRet(tcpsrv.SetCBIsPermittedHost(state->server, permitted));
    CHKiRet(tcpsrv.SetCBRcvData(state->server, receiveData));
    CHKiRet(tcpsrv.SetCBOnSessAccept(state->server, accepted));
    CHKiRet(tcpsrv.SetCBOnSessDestruct(state->server, destroyed));
    CHKiRet(tcpsrv.SetCBOnRegularClose(state->server, regularClose));
    CHKiRet(tcpsrv.SetCBOnErrClose(state->server, tcps_sess.Close));
    CHKiRet(tcpsrv.SetOnMsgReceive(state->server, received));
    CHKmalloc(params = calloc(1, sizeof(*params)));
    params->pszPort = (uchar *)strdup("0");
    params->pszAddr = (uchar *)strdup("127.0.0.1");
    if (params->pszPort == NULL || params->pszAddr == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    CHKiRet(tcpsrv.SetInputName(state->server, params, UCHAR_CONSTANT("prepared-backend-test")));
    /* configureTCPListen owns params on both success and failure. */
    tcpLstnParams_t *transferred = params;
    params = NULL;
    CHKiRet(tcpsrv.configureTCPListen(state->server, transferred));
    CHKiRet(tcpsrv.ConstructFinalizePrepared(state->server));
    CHKiRet(tcpsrv.ActivatePreparedListeners(state->server));
    if (state->server->iLstnCurr != 1) ABORT_FINALIZE(RS_RET_ERR);
    CHKiRet(netstrm.GetSock(state->server->ppLstn[0], listener));
finalize_it:
    if (params != NULL) {
        if (params->pInputName != NULL) propDestruct(&params->pInputName);
        free(params->pszInputName);
        free((void *)params->pszPort);
        free((void *)params->pszAddr);
        free(params);
    }
    RETiRet;
}

static int closedDescriptor(int descriptor) {
    errno = 0;
    return descriptor >= 0 && fcntl(descriptor, F_GETFD) == -1 && errno == EBADF;
}

static int backendRegistered(testState_t *state) {
    tcpsrv_t *server = state->server;
    if (!server->fenceReady) return 0;
#ifdef ENABLE_IMTCP_EPOLL
    /* Duplicate ADD must fail with EEXIST, proving actual kernel registration
     * without changing events or consuming the pending listener readiness. */
    errno = 0;
    if (epoll_ctl(state->backend, EPOLL_CTL_ADD, state->control[0], &server->controlDescr.event) != -1 ||
        errno != EEXIST)
        return 0;
    for (int i = 0; i < server->iLstnCurr; ++i) {
        tcpsrv_io_descr_t *descriptor = server->ppioDescrPtr[i];
        if (descriptor == NULL) return 0;
        errno = 0;
        if (epoll_ctl(state->backend, EPOLL_CTL_ADD, descriptor->sock, &descriptor->event) != -1 || errno != EEXIST)
            return 0;
    }
#else
    for (int i = 0; i < server->iLstnCurr; ++i) {
        int listener;
        if (netstrm.GetSock(server->ppLstn[i], &listener) != RS_RET_OK || server->evtdata.poll.fds[i].fd != listener ||
            !(server->evtdata.poll.fds[i].events & POLLIN))
            return 0;
    }
    const unsigned controlIndex = server->iLstnCurr;
    if (server->evtdata.poll.currfds != controlIndex + 1 ||
        server->evtdata.poll.fds[controlIndex].fd != state->control[0] ||
        !(server->evtdata.poll.fds[controlIndex].events & POLLIN))
        return 0;
#endif
    return 1;
}

static int backendClean(testState_t *state) {
    tcpsrv_t *server = state->server;
    if (server->fenceReady || server->controlPipe[0] != -1 || server->controlPipe[1] != -1 ||
        server->workQueue.wrkr_tids != NULL || server->workQueue.wrkr_data != NULL || server->fenceItems != NULL)
        return 0;
    for (int i = 0; i < server->iLstnMax; ++i)
        if (server->ppioDescrPtr[i] != NULL) return 0;
    for (int i = 0; i < server->iSessMax; ++i)
        if (server->pSessions[i] != NULL) return 0;
#ifdef ENABLE_IMTCP_EPOLL
    if (state->ready && !closedDescriptor(state->backend)) return 0;
#else
    if (server->evtdata.poll.fds != NULL || server->evtdata.poll.maxfds != 0 || server->evtdata.poll.currfds != 0)
        return 0;
#endif
    return !state->ready || (closedDescriptor(state->control[0]) && closedDescriptor(state->control[1]));
}

static int sendAll(int descriptor, const char *buffer, size_t length) {
    size_t sent = 0;
    while (sent < length) {
        const ssize_t result = send(descriptor, buffer + sent, length - sent, 0);
        if (result < 0 && errno == EINTR) continue;
        if (result <= 0) return 0;
        sent += (size_t)result;
    }
    return 1;
}

static int trafficCase(int commit, unsigned workers) {
    testState_t state = {.gateResult = RS_RET_FORCE_TERM};
    pthread_t thread;
    int started = 0;
    int client = -1;
    int listener = -1;
    int failed = 0;
    const int mutexError = pthread_mutex_init(&state.mutex, NULL);
    if (mutexError != 0) {
        fprintf(stderr, "prepared backend mutex initialization failed: %s\n", strerror(mutexError));
        return 1;
    }
    const int conditionError = pthread_cond_init(&state.condition, NULL);
    if (conditionError != 0) {
        fprintf(stderr, "prepared backend condition initialization failed: %s\n", strerror(conditionError));
        pthread_mutex_destroy(&state.mutex);
        return 1;
    }
    CHECK(makeServer(&state, workers, &listener) == RS_RET_OK);
    CHECK(pthread_create(&thread, NULL, runPrepared, &state) == 0);
    started = 1;
    CHECK(waitFor(&state, &state.ready));
    CHECK(state.workerTopologyReady);
    CHECK(backendRegistered(&state));
    struct sockaddr_in address = {0};
    socklen_t addressLength = sizeof(address);
    CHECK(getsockname(listener, (struct sockaddr *)&address, &addressLength) == 0);
    CHECK(addressLength == (socklen_t)sizeof(address));
    CHECK(address.sin_family == AF_INET);
    CHECK(address.sin_port != 0);
    client = socket(AF_INET, SOCK_STREAM, 0);
    CHECK(client >= 0);
    CHECK(connect(client, (struct sockaddr *)&address, addressLength) == 0);
    const char wire[] = "prepared-backend-real-message\n";
    CHECK(sendAll(client, wire, sizeof(wire) - 1));
    /* The event loop is inside gate, not merely presumed idle after a delay.
     * Socket readiness proves traffic is queued before checking zero dispatch. */
    struct pollfd pending = {.fd = listener, .events = POLLIN};
    CHECK(poll(&pending, 1, 30000) == 1 && (pending.revents & POLLIN));
    pthread_mutex_lock(&state.mutex);
    const int gated = state.ready && !state.release && !state.finished && state.accepts == 0 && state.receives == 0;
    pthread_mutex_unlock(&state.mutex);
    CHECK(gated);
    pthread_mutex_lock(&state.mutex);
    state.gateResult = commit ? RS_RET_OK : RS_RET_FORCE_TERM;
    state.release = 1;
    pthread_cond_broadcast(&state.condition);
    pthread_mutex_unlock(&state.mutex);
    if (commit) {
        CHECK(waitFor(&state, &state.receives));
        close(client);
        client = -1;
        CHECK(waitFor(&state, &state.destroyed));
    } else {
        CHECK(waitFor(&state, &state.finished));
    }
cleanup:
    if (started) {
        pthread_mutex_lock(&state.mutex);
        if (!state.release) {
            state.gateResult = RS_RET_FORCE_TERM;
            state.release = 1;
            pthread_cond_broadcast(&state.condition);
        }
        const int committed = commit && state.gateResult == RS_RET_OK;
        pthread_mutex_unlock(&state.mutex);
        if (client >= 0) {
            close(client);
            client = -1;
        }
        /* Close and drain the real session before terminating the input. This
         * cleanup also runs on failed assertions, not only the success path. */
        if (committed && !waitFor(&state, &state.destroyed)) failed = 1;
        pthread_mutex_lock(&state.mutex);
        const int needsTermination = committed && !state.finished;
        pthread_mutex_unlock(&state.mutex);
        if (needsTermination) {
            glbl.SetGlobalInputTermination();
            /* Match controlNotifyExit's lock so an early backend error cannot
             * close/reuse the pipe descriptor while this cleanup wakes it. */
            pthread_mutex_lock(&state.server->fenceMut);
            const char wake = 'x';
            if (state.server->controlPipe[1] >= 0) (void)write(state.server->controlPipe[1], &wake, 1);
            pthread_mutex_unlock(&state.server->fenceMut);
        }
        const int joinError = pthread_join(thread, NULL);
        if (joinError != 0) {
            fprintf(stderr, "prepared backend pthread_join failed: %s\n", strerror(joinError));
            /* No thread completion/ownership is proven. Do not inspect or
             * destruct its resources, or unload its module. The isolated child
             * returns failure and process teardown reclaims this exceptional
             * harness-error path. Ordinary failed assertions still join above. */
            joinFailed = 1;
            return 1;
        }
        if (state.result != (commit ? RS_RET_OK : RS_RET_FORCE_TERM) || state.badPayload || state.accepts != commit ||
            state.receives != commit || state.destroyed != commit || !backendClean(&state))
            failed = 1;
    }
    if (client >= 0) close(client);
    if (state.server != NULL) tcpsrv.Destruct(&state.server);
    if (listener >= 0 && !closedDescriptor(listener)) failed = 1;
    pthread_cond_destroy(&state.condition);
    pthread_mutex_destroy(&state.mutex);
    if (failed)
        fprintf(stderr,
                "traffic case commit=%d requested-workers=%u actual-worker-threads=%u topology-ready=%d failed "
                "(ret=%d accept=%d receive=%d destroyed=%d)\n",
                commit, workers, state.actualWorkerThreads, state.workerTopologyReady, state.result, state.accepts,
                state.receives, state.destroyed);
    return failed;
}

static int setupFailureCase(void) {
    /* A bounded descriptor limit drives an actual pipe(2) failure in
     * controlNotifyInit. No fake backend or allocation-failure hook is used.
     * Restore resources before checking/destructing the server. */
    testState_t state = {.mutex = PTHREAD_MUTEX_INITIALIZER, .condition = PTHREAD_COND_INITIALIZER};
    state.release = 1;
    state.gateResult = RS_RET_FORCE_TERM;
    struct rlimit original, limited;
    int descriptors[128];
    int count = 0, limitChanged = 0, listener = -1, failed = 0;
    CHECK(makeServer(&state, 2, &listener) == RS_RET_OK);
    CHECK(getrlimit(RLIMIT_NOFILE, &original) == 0);
    limited = original;
    if (limited.rlim_cur > 128) limited.rlim_cur = 128;
    CHECK(setrlimit(RLIMIT_NOFILE, &limited) == 0);
    limitChanged = 1;
    while (count < 128) {
        const int descriptor = open("/dev/null", O_RDONLY);
        if (descriptor < 0) break;
        descriptors[count++] = descriptor;
    }
    CHECK(count < 128 && errno == EMFILE);
    state.result = tcpsrv.RunPrepared(state.server, gate, &state);
    CHECK(state.result == RS_RET_IO_ERROR && !state.ready);
    CHECK(backendClean(&state));
cleanup:
    for (int i = 0; i < count; ++i) close(descriptors[i]);
    if (limitChanged && setrlimit(RLIMIT_NOFILE, &original) != 0) failed = 1;
    if (state.server != NULL) tcpsrv.Destruct(&state.server);
    if (listener >= 0 && !closedDescriptor(listener)) failed = 1;
    pthread_cond_destroy(&state.condition);
    pthread_mutex_destroy(&state.mutex);
    return failed;
}

static int runSuite(unsigned commitWorkers) {
    const char *errorObject = "runtime";
    rsconf_t config = {0};
    int initialized = 0;
    int glblUsed = 0, streamUsed = 0, sessionUsed = 0, serverUsed = 0;
    int dnsInitialized = 0, ratelimitInitialized = 0;
    int failed = 1;
    const char *stage = "rsrtInit";
    rsRetVal initResult;
    /* Same runtime/object-loader initialization as tests/testbench.h, with an
     * explicit final cleanup even when the test returns failure. */
    setenv("RSYSLOG_MODDIR", "../runtime/.libs/", 1);
    dbgClassInit();
    rsrtSetErrLogger(reportRuntimeError);
    initResult = rsrtInit(&errorObject, &obj);
    if (initResult != RS_RET_OK) goto cleanup;
    initialized = 1;
    if (obj.UseObj == NULL || obj.ReleaseObj == NULL) {
        stage = "rsrtInit object-loader callbacks";
        initResult = RS_RET_ERR;
        goto cleanup;
    }
    stage = "objUse(glbl)";
    initResult = objUse(glbl, CORE_COMPONENT);
    if (initResult != RS_RET_OK) goto cleanup;
    glblUsed = 1;
    stage = "objUse(netstrm)";
    initResult = objUse(netstrm, LM_NETSTRMS_FILENAME);
    if (initResult != RS_RET_OK) goto cleanup;
    streamUsed = 1;
    stage = "objUse(tcps_sess)";
    initResult = objUse(tcps_sess, LM_TCPSRV_FILENAME);
    if (initResult != RS_RET_OK) goto cleanup;
    sessionUsed = 1;
    stage = "objUse(tcpsrv)";
    initResult = objUse(tcpsrv, LM_TCPSRV_FILENAME);
    if (initResult != RS_RET_OK) goto cleanup;
    serverUsed = 1;
    config.globals.iMaxLine = 8192;
    config.globals.bDisableDNS = 1;
    runConf = &config;
    loadConf = &config;
    /* Plain netstream accept still uses the DNS cache when DNS resolution is
     * disabled. These are normal daemon prerequisites outside rsrtInit. */
    stage = "dnscacheInit";
    initResult = dnscacheInit();
    if (initResult != RS_RET_OK) goto cleanup;
    dnsInitialized = 1;
    stage = "ratelimitModInit";
    initResult = ratelimitModInit();
    if (initResult != RS_RET_OK) goto cleanup;
    ratelimitInitialized = 1;
    failed = setupFailureCase() || trafficCase(0, 1) || trafficCase(0, 2) || trafficCase(1, commitWorkers);
cleanup:
    if (initResult != RS_RET_OK)
        fprintf(stderr, "prepared backend initialization failed at %s: ret=%d object=%s\n", stage, initResult,
                errorObject);
    if (joinFailed) return 1;
    if (ratelimitInitialized) ratelimitModExit();
    if (dnsInitialized) dnscacheDeinit();
    runConf = NULL;
    loadConf = NULL;
    if (serverUsed) objRelease(tcpsrv, LM_TCPSRV_FILENAME);
    if (sessionUsed) objRelease(tcps_sess, LM_TCPSRV_FILENAME);
    if (streamUsed) objRelease(netstrm, LM_NETSTRMS_FILENAME);
    if (glblUsed) objRelease(glbl, CORE_COMPONENT);
    if (initialized) rsrtExit();
    if (!failed) puts("production prepared tcpsrv backend tests passed");
    return failed;
}

int main(void) {
    /* Global input termination is deliberately one-way. Separate processes,
     * forked before runtime/thread initialization, cover committed traffic with
     * one and two configured workers without resetting production global state.
     * Poll builds legitimately coerce both cases to one backend worker. */
    int failed = 0;
    for (unsigned workers = 1; workers <= 2; ++workers) {
        const pid_t child = fork();
        if (child < 0) {
            perror("prepared backend fork");
            return 1;
        }
        if (child == 0) return runSuite(workers);
        int status;
        pid_t waited;
        do {
            waited = waitpid(child, &status, 0);
        } while (waited < 0 && errno == EINTR);
        if (waited != child) {
            perror("prepared backend waitpid");
            failed = 1;
        } else if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            fprintf(stderr, "prepared backend child workers=%u failed: wait status=%d%s\n", workers, status,
                    WIFSIGNALED(status) ? " (terminated by signal)" : "");
            failed = 1;
        }
    }
    return failed;
}
