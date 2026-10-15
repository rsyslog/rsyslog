/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright 2026 Rainer Gerhards and Adiscon GmbH.
 * Exercise the production accepted-session publication helper with real session
 * construction/destruction and real epoll syscalls. A synchronous reclamation
 * hook after successful ADD checks that session metadata is already complete,
 * then destroys both objects before ADD returns. This deterministically exposes
 * post-publication accesses under ASan. A closed epoll fd checks failed ADD
 * returns an error and destroys the unpublished session/table entry exactly once.
 * This seam covers ownership, not network accept or backend worker scheduling.
 */
#include "config.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#ifdef ENABLE_IMTCP_EPOLL
    #include <sys/epoll.h>
#endif
#include "rsyslog.h"
#include "obj.h"
#include "debug.h"
#include "rsconf.h"
#include "netstrm.h"
#include "tcps_sess.h"
#include "tcpsrv.h"

#ifdef ENABLE_IMTCP_EPOLL
DEFobjCurrIf(obj);
DEFobjCurrIf(tcps_sess);
DEFobjCurrIf(tcpsrv);
static int destructions;
static int closes;
static rsRetVal destroyed(void *unused) {
    (void)unused;
    ++destructions;
    return RS_RET_OK;
}
static rsRetVal closed(tcps_sess_t *session) {
    ++closes;
    return tcps_sess.Close(session);
}
static void diagnostic(int severity, int error, const uchar *message) {
    (void)severity;
    fprintf(stderr, "publication test diagnostic %d: %s\n", error, message);
}

#endif /* ENABLE_IMTCP_EPOLL */

int main(void) {
#ifndef ENABLE_IMTCP_EPOLL
    return 77;
#else
    typedef rsRetVal (*publish_t)(tcpsrv_t *, tcps_sess_t *, int, int, int);
    rsconf_t config = {0};
    const char *errorObject = NULL;
    int initialized = 0, sessionUsed = 0, serverUsed = 0;
    int result = 1;
    void *module = NULL;
    tcpsrv_t *server = NULL;
    tcps_sess_t *session = NULL;
    int sockets[2] = {-1, -1};
    int epollFd = -1;
    setenv("RSYSLOG_MODDIR", "../runtime/.libs/", 1);
    dbgClassInit();
    rsrtSetErrLogger(diagnostic);
    if (rsrtInit(&errorObject, &obj) != RS_RET_OK) goto cleanup;
    initialized = 1;
    if (objUse(tcps_sess, LM_TCPSRV_FILENAME) != RS_RET_OK) goto cleanup;
    sessionUsed = 1;
    if (objUse(tcpsrv, LM_TCPSRV_FILENAME) != RS_RET_OK) goto cleanup;
    serverUsed = 1;
    module = dlopen("../runtime/.libs/lmtcpsrv.so", RTLD_NOW);
    if (module == NULL) goto cleanup;
    publish_t publish = (publish_t)dlsym(module, "tcpsrvTestPublishAccepted");
    if (publish == NULL) goto cleanup;
    config.globals.iMaxLine = 8192;
    runConf = &config;
    loadConf = &config;
    if (tcpsrv.Construct(&server) != RS_RET_OK) goto cleanup;
    server->iSessMax = 1;
    server->pSessions = calloc(1, sizeof(*server->pSessions));
    if (server->pSessions == NULL) goto cleanup;
    server->pOnRegularClose = closed;
    server->pOnErrClose = closed;
    server->pOnSessDestruct = destroyed;
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) goto cleanup;
    epollFd = epoll_create1(EPOLL_CLOEXEC);
    if (epollFd < 0) goto cleanup;
    for (int failure = 0; failure < 2; ++failure) {
        if (tcps_sess.Construct(&session) != RS_RET_OK) goto cleanup;
        if (tcps_sess.SetTcpsrv(session, server) != RS_RET_OK) goto cleanup;
        server->pSessions[0] = session;
        server->evtdata.epoll.efd = failure ? -1 : epollFd;
        const rsRetVal ret = publish(server, session, 0, sockets[0], !failure);
        session = NULL; /* publication helper always consumes its argument */
        if (ret != (failure ? RS_RET_ERR_EPOLL_CTL : RS_RET_OK) || server->pSessions[0] != NULL ||
            destructions != failure + 1 || closes != failure + 1)
            goto cleanup;
    }
    result = 0;
cleanup:
    if (session != NULL) tcps_sess.Destruct(&session);
    if (server != NULL) {
        server->evtdata.epoll.efd = -1;
        tcpsrv.Destruct(&server);
    }
    if (epollFd >= 0) close(epollFd);
    for (int i = 0; i < 2; ++i)
        if (sockets[i] >= 0) close(sockets[i]);
    if (module != NULL) dlclose(module);
    runConf = NULL;
    loadConf = NULL;
    if (serverUsed) objRelease(tcpsrv, LM_TCPSRV_FILENAME);
    if (sessionUsed) objRelease(tcps_sess, LM_TCPSRV_FILENAME);
    if (initialized) rsrtExit();
    if (result) fprintf(stderr, "accepted-session publication regression failed\n");
    return result;
#endif
}
