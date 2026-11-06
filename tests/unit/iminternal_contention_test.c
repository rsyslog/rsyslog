/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright 2026 Rainer Gerhards and Adiscon GmbH.
 */
/* Exercise real iminternalAddMsg/RemoveMsg with the macOS fallback selected on
 * every platform. A real trylock observes the held list mutex before the holder
 * releases it: success means the exact message is later removed, without its
 * ownership being destroyed. Keeping the lock held must instead return and
 * destroy the message once. Cancellation while contended must complete ownership
 * transfer and release the mutex before delivery. The alarm bounds a hang; elapsed
 * time and return/ownership checks are the oracles, not a scheduling sleep.
 */
#include "config.h"
#include "rsyslog.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "syslogd.h"
#include "iminternal.h"
#include "linkedlist.h"

#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) {                                                               \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
            return 1;                                                                \
        }                                                                            \
    } while (0)

int Debug;
int bHaveMainQueue;
pid_t glbl_ourpid;
static unsigned destroyed;
static pthread_mutex_t observed_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t observed_cond = PTHREAD_COND_INITIALIZER;
static unsigned busy_attempts;

#ifndef DEBUGLESS
void r_dbgprintf(const char *srcname __attribute__((unused)), const char *fmt __attribute__((unused)), ...) {}
#endif
rsRetVal msgDestruct(smsg_t **msg) {
    ++destroyed;
    *msg = NULL;
    return RS_RET_OK;
}
uchar *getMSG(smsg_t *msg) {
    return msg->pszRawMsg;
}
uchar *glblGetLocalHostName(void) {
    return (uchar *)"unit-test";
}
void MsgSetHOSTNAME(smsg_t *msg, const uchar *name, int len) {
    msg->pszHOSTNAME = (uchar *)name;
    msg->iLenHOSTNAME = len;
}

/* Observe actual contention, without fabricating an EBUSY result. */
static int observed_trylock(pthread_mutex_t *mutex) {
    int result = pthread_mutex_trylock(mutex);
    if (result == EBUSY) {
        pthread_mutex_lock(&observed_mutex);
        ++busy_attempts;
        pthread_cond_signal(&observed_cond);
        pthread_mutex_unlock(&observed_mutex);
    }
    return result;
}
#ifndef __APPLE__
    #define __APPLE__ 1
    #define IMINTERNAL_TEST_UNDEFINE_APPLE 1
#endif
#define pthread_mutex_trylock observed_trylock
#include "../../tools/iminternal.c"
#undef pthread_mutex_trylock
#ifdef IMINTERNAL_TEST_UNDEFINE_APPLE
    #undef __APPLE__
    #undef IMINTERNAL_TEST_UNDEFINE_APPLE
#endif
#include "../../runtime/linkedlist.c"

struct addition {
    smsg_t *msg;
    rsRetVal result;
};
static void *add_message(void *arg) {
    struct addition *addition = arg;
    addition->result = iminternalAddMsg(addition->msg);
    pthread_testcancel();
    return NULL;
}

static int test_contention(int release, int cancel) {
    smsg_t msg;
    smsg_t *received = NULL;
    struct addition addition = {.msg = &msg, .result = RS_RET_ERR};
    pthread_t thread;
    void *thread_result = NULL;
    struct timespec start, end;
    memset(&msg, 0, sizeof(msg));
    msg.pszRawMsg = (uchar *)"retained internal diagnostic";
    msg.pszHOSTNAME = (uchar *)"unit-test";
    destroyed = busy_attempts = 0;
    CHECK(modInitIminternal() == RS_RET_OK);
    CHECK(pthread_mutex_lock(&mutList) == 0);
    CHECK(clock_gettime(CLOCK_MONOTONIC, &start) == 0);
    CHECK(pthread_create(&thread, NULL, add_message, &addition) == 0);
    CHECK(pthread_mutex_lock(&observed_mutex) == 0);
    while (busy_attempts == 0) CHECK(pthread_cond_wait(&observed_cond, &observed_mutex) == 0);
    CHECK(pthread_mutex_unlock(&observed_mutex) == 0);
    if (cancel) CHECK(pthread_cancel(thread) == 0);
    if (release) CHECK(pthread_mutex_unlock(&mutList) == 0);
    CHECK(pthread_join(thread, &thread_result) == 0);
    CHECK(thread_result == (cancel ? PTHREAD_CANCELED : NULL));
    CHECK(clock_gettime(CLOCK_MONOTONIC, &end) == 0);
    if (!release) CHECK(pthread_mutex_unlock(&mutList) == 0);
    if (release) {
        if (!cancel) CHECK(addition.result == RS_RET_OK);
        CHECK(destroyed == 0);
        CHECK(iminternalRemoveMsg(&received) == RS_RET_OK);
        CHECK(received == &msg);
        CHECK(strcmp((char *)received->pszRawMsg, "retained internal diagnostic") == 0);
    } else {
        CHECK(addition.result == RS_RET_ERR);
        CHECK(destroyed == 1);
        CHECK(busy_attempts > 1);
        /* The one-second deadline must actually wait, rather than drop early.
         * 200 ms lower slack tolerates clock granularity; the upper slack
         * allows loaded CI scheduling without accepting an unbounded lock. */
        long long elapsed = (end.tv_sec - start.tv_sec) * 1000000000LL + end.tv_nsec - start.tv_nsec;
        CHECK(elapsed >= 800000000LL);
        CHECK(elapsed < 3000000000LL);
        CHECK(iminternalRemoveMsg(&received) == RS_RET_END_OF_LINKEDLIST);
    }
    CHECK(modExitIminternal() == RS_RET_OK);
    return 0;
}

int main(void) {
    alarm(10);
    /* Cancellation while waiting must never strand the list mutex or lose
     * ownership. Delivery is checked after the canceled adder is joined. */
    if (test_contention(1, 0) || test_contention(0, 0) || test_contention(1, 1)) return 1;
    alarm(0);
    puts("iminternal transient contention retention and bounded failure passed");
    return 0;
}
