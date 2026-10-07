/* SPDX-License-Identifier: Apache-2.0 */
#ifndef IMTCP_LIFECYCLE_H
#define IMTCP_LIFECYCLE_H

#include <pthread.h>
#include "rsyslog.h"

/* Private registry ownership gate. Server workers never acquire this mutex;
 * it orders their creator's publication and traversal against control reclaim. */
typedef struct imtcpLifecycle_s {
    pthread_mutex_t mut;
    int startupComplete;
} imtcpLifecycle_t;

#define IMTCP_LIFECYCLE_INITIALIZER \
    { PTHREAD_MUTEX_INITIALIZER, 0 }

static inline void imtcpLifecycleLock(imtcpLifecycle_t *const lifecycle) {
    pthread_mutex_lock(&lifecycle->mut);
}

/* Also usable as the input thread's cancellation cleanup handler. */
static inline void imtcpLifecycleUnlock(void *const context) {
    imtcpLifecycle_t *const lifecycle = context;
    pthread_mutex_unlock(&lifecycle->mut);
}

/* Caller holds the gate across the entire initial registry traversal, not just
 * pthread_create: the creator must finish publishing every thread handle. */
static inline void imtcpLifecycleStartupComplete(imtcpLifecycle_t *const lifecycle) {
    lifecycle->startupComplete = 1;
}

static inline rsRetVal imtcpLifecycleLockReload(imtcpLifecycle_t *const lifecycle) {
    imtcpLifecycleLock(lifecycle);
    if (!lifecycle->startupComplete) {
        /* The outer input thread might never have been created. Do not wait
         * indefinitely or start a reload addition during initial traversal. */
        imtcpLifecycleUnlock(lifecycle);
        return RS_RET_RETRY;
    }
    return RS_RET_OK;
}

#endif
