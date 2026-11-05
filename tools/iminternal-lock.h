/* SPDX-License-Identifier: Apache-2.0 */
#ifndef INCLUDED_IMINTERNAL_LOCK_H
#define INCLUDED_IMINTERNAL_LOCK_H

#include <errno.h>
#include <pthread.h>
#include <time.h>

/* macOS has no pthread_mutex_timedlock. Retry transient contention for a
 * bounded monotonic interval instead of discarding an internal diagnostic on
 * the first busy mutex. The caller must suppress cancellation until it has
 * released the acquired mutex and completed ownership transfer. */
static inline int iminternalMutexTimedLock(pthread_mutex_t *mutex, const unsigned timeout_ms) {
    struct timespec deadline;
    struct timespec now;
    const struct timespec pause = {0, 1000000};
    int ret = pthread_mutex_trylock(mutex);
    if (ret != EBUSY) return ret;
    if (clock_gettime(CLOCK_MONOTONIC, &deadline) != 0) return errno;
    deadline.tv_sec += timeout_ms / 1000;
    deadline.tv_nsec += (long)(timeout_ms % 1000) * 1000000;
    if (deadline.tv_nsec >= 1000000000) {
        ++deadline.tv_sec;
        deadline.tv_nsec -= 1000000000;
    }
    for (;;) {
        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return errno;
        if (now.tv_sec > deadline.tv_sec || (now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec))
            return ETIMEDOUT;
        if (nanosleep(&pause, NULL) != 0 && errno != EINTR) return errno;
        ret = pthread_mutex_trylock(mutex);
        if (ret != EBUSY) return ret;
    }
}

#endif
