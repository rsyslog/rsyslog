/* persourceratelimit.h
 *
 * Interface for per-source rate limiting.
 *
 * Copyright 2025 Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
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
#ifndef INCLUDED_PERSOURCERATELIMIT_H
#define INCLUDED_PERSOURCERATELIMIT_H

#include "rsyslog.h"
#include "hashtable.h"
#include "statsobj.h"

/* Structure for override configuration */
struct perSourceOverride_s {
    unsigned int max;
    unsigned int window;
};
typedef struct perSourceOverride_s perSourceOverride_t;

/* Structure for sender state */
struct perSourceSenderState_s {
    unsigned int currentCount;
    time_t windowStart;
    pthread_mutex_t mut;
};
typedef struct perSourceSenderState_s perSourceSenderState_t;

struct perSourceRateLimiter_s {
    uchar *pszPolicyFile;
    struct hashtable *htSenders; /* Key: sender string, Value: perSourceSenderState_t* */
    struct hashtable *htOverrides; /* Key: sender string, Value: perSourceOverride_t* */
    pthread_rwlock_t rwlock; /* Protects hashtables */
    
    /* Default limits */
    unsigned int defaultMax;
    unsigned int defaultWindow;

    statsobj_t *stats;
    STATSCOUNTER_DEF(ctrAllowed, mutCtrAllowed);
    STATSCOUNTER_DEF(ctrDropped, mutCtrDropped);
    uchar *pszOrigin;
};

typedef struct perSourceRateLimiter_s perSourceRateLimiter_t;

/* Interface */
rsRetVal perSourceRateLimiterNew(perSourceRateLimiter_t **ppThis);
rsRetVal perSourceRateLimiterConstructFinalize(perSourceRateLimiter_t *pThis);
rsRetVal perSourceRateLimiterDestruct(perSourceRateLimiter_t **ppThis);
rsRetVal perSourceRateLimiterSetPolicyFile(perSourceRateLimiter_t *pThis, const uchar *pszFileName);
rsRetVal perSourceRateLimiterCheck(perSourceRateLimiter_t *pThis, const uchar *key, time_t ttNow);
rsRetVal perSourceRateLimiterReload(perSourceRateLimiter_t *pThis);
rsRetVal perSourceRateLimiterSetOrigin(perSourceRateLimiter_t *pThis, const uchar *pszOrigin);

#endif /* #ifndef INCLUDED_PERSOURCERATELIMIT_H */
