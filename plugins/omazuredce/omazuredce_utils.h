/* omazuredce_utils.h
 * Internal helpers shared by the omazuredce module and its unit test.
 *
 * Copyright 2026 Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *       -or-
 *       see COPYING.ASL20 in the source distribution
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef INCLUDED_OMAZUREDCE_UTILS_H
#define INCLUDED_OMAZUREDCE_UTILS_H

#include <pthread.h>
#include <stddef.h>

typedef enum { OMAZUREDCE_REQUEST_FITS = 0, OMAZUREDCE_REQUEST_RETRY } omazuredce_request_size_result_t;

/** Classify a conservative request-size estimate against its configured cap. */
static inline omazuredce_request_size_result_t omazuredceClassifyRequestSize(const size_t requestBytes,
                                                                             const size_t maxRequestBytes) {
    return requestBytes <= maxRequestBytes ? OMAZUREDCE_REQUEST_FITS : OMAZUREDCE_REQUEST_RETRY;
}

/** Set the timer stop predicate and signal its condition while holding the wait mutex.
 *
 * The function acquires and releases mutex. Callers must not already hold it.
 */
static inline void omazuredceRequestTimerStop(pthread_mutex_t *const mutex,
                                              pthread_cond_t *const cond,
                                              signed char *const stopRequested) {
    (void)pthread_mutex_lock(mutex);
    *stopRequested = 1;
    (void)pthread_cond_signal(cond);
    (void)pthread_mutex_unlock(mutex);
}

#endif
