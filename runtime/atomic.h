/* This header supplies atomic operations. So far, we rely on GCC's
 * atomic builtins. During configure, we check if atomic operations are
 * available. If they are not, I am making the necessary provisioning to live without them if
 * they are not available. Please note that you should only use the macros
 * here if you think you can actually live WITHOUT an explicit atomic operation,
 * because in the non-presence of them, we simply do it without atomicity.
 * Which, for word-aligned data types, usually (but only usually!) should work.
 *
 * We are using the functions described in
 * http://gcc.gnu.org/onlinedocs/gcc/Atomic-Builtins.html
 *
 * THESE MACROS MUST ONLY BE USED WITH WORD-SIZED DATA TYPES!
 *
 * Usage guidance (important for portability and static analysis tools):
 * - Operations are provided as macros or inline functions. Prefer simple lvalues
 *   for the "data" argument to avoid double-evaluation surprises.
 * - The "phlpmut" argument is a helper mutex used when atomics are unavailable.
 *   If atomics are supported, these helper mutex macros become no-ops.
 *   Callers must still pass a valid pointer where required to keep signatures
 *   uniform across platforms.
 * - Some macros are "best effort" (PREFER_*). They may degrade to non-atomic
 *   operations when no atomic support is available. Use them only for
 *   statistics or counters where occasional lost updates are acceptable.
 * - The remaining ATOMIC_* helpers provide true atomicity when available and
 *   fall back to mutex-based critical sections when not.
 * - Mixing atomic loads/stores with plain reads/writes to the same variable is
 *   undefined on modern compilers. If you use ATOMIC_* on a variable, access it
 *   exclusively via these helpers (or other synchronized paths).
 *
 * Copyright 2008-2012 Rainer Gerhards and Adiscon GmbH.
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
#ifndef INCLUDED_ATOMIC_H
#define INCLUDED_ATOMIC_H
#include <time.h>
#include "typedefs.h"

/* for this release, we disable atomic calls because there seem to be some
 * portability problems and we can not fix that without destabilizing the build.
 * They simply came in too late. -- rgerhards, 2008-04-02
 */
#ifdef HAVE_ATOMIC_BUILTINS
    /*
     * Return-value helpers mirror their names and the mutex fallback below:
     * *_AND_FETCH returns the value after the operation.
     */
    #define ATOMIC_SUB(data, val, phlpmut) __atomic_fetch_sub((data), (val), __ATOMIC_SEQ_CST)
    #define ATOMIC_SUB_unsigned(data, val, phlpmut) __atomic_fetch_sub((data), (val), __ATOMIC_SEQ_CST)
    #define ATOMIC_ADD(data, val) __atomic_fetch_add(&(data), (val), __ATOMIC_SEQ_CST)
    #define ATOMIC_INC(data, phlpmut) ((void)__atomic_fetch_add((data), 1, __ATOMIC_SEQ_CST))
    #define ATOMIC_INC_AND_FETCH_int(data, phlpmut) __atomic_add_fetch((data), 1, __ATOMIC_SEQ_CST)
    #define ATOMIC_INC_AND_FETCH_unsigned(data, phlpmut) __atomic_add_fetch((data), 1, __ATOMIC_SEQ_CST)
    #define ATOMIC_DEC(data, phlpmut) ((void)__atomic_sub_fetch((data), 1, __ATOMIC_SEQ_CST))
    #define ATOMIC_DEC_AND_FETCH(data, phlpmut) __atomic_sub_fetch((data), 1, __ATOMIC_SEQ_CST)
    #define ATOMIC_LOAD_32BIT(data, phlpmut) ((int)__atomic_load_n((data), __ATOMIC_ACQUIRE))
    #define ATOMIC_LOAD_32BIT_unsigned(data, phlpmut) ((unsigned)__atomic_load_n((data), __ATOMIC_ACQUIRE))
    #define ATOMIC_LOAD_32BIT_RELAXED(data, phlpmut) ((int)__atomic_load_n((data), __ATOMIC_RELAXED))
    #define ATOMIC_LOAD_32BIT_RELAXED_unsigned(data, phlpmut) ((unsigned)__atomic_load_n((data), __ATOMIC_RELAXED))
    #define ATOMIC_STORE_32BIT(data, phlpmut, val) ((void)__atomic_store_n((data), (val), __ATOMIC_RELEASE))
    #define ATOMIC_STORE_32BIT_unsigned(data, phlpmut, val) ((void)__atomic_store_n((data), (val), __ATOMIC_RELEASE))
    #define ATOMIC_STORE_32BIT_RELAXED(data, phlpmut, val) ((void)__atomic_store_n((data), (val), __ATOMIC_RELAXED))
    #define ATOMIC_STORE_32BIT_RELAXED_unsigned(data, phlpmut, val) \
        ((void)__atomic_store_n((data), (val), __ATOMIC_RELAXED))
    #define ATOMIC_STORE_1_TO_32BIT(data) ((void)__atomic_store_n(&(data), 1, __ATOMIC_RELEASE))
    #define ATOMIC_OR_INT_TO_INT(data, phlpmut, val) __atomic_fetch_or((data), (val), __ATOMIC_SEQ_CST)
    #define ATOMIC_CAS(data, oldVal, newVal, phlpmut)                                                                  \
        ({                                                                                                             \
            __typeof__(*(data)) rs_atomic_expected = (oldVal);                                                         \
            __atomic_compare_exchange_n((data), &rs_atomic_expected, (newVal), 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); \
        })
    #define ATOMIC_CAS_time_t(data, oldVal, newVal, phlpmut)                                                           \
        ({                                                                                                             \
            __typeof__(*(data)) rs_atomic_expected = (oldVal);                                                         \
            __atomic_compare_exchange_n((data), &rs_atomic_expected, (newVal), 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); \
        })
    #define ATOMIC_CAS_VAL(data, oldVal, newVal, phlpmut)                                                 \
        ({                                                                                                \
            __typeof__(*(data)) rs_atomic_expected = (oldVal);                                            \
            (void)__atomic_compare_exchange_n((data), &rs_atomic_expected, (newVal), 0, __ATOMIC_SEQ_CST, \
                                              __ATOMIC_SEQ_CST);                                          \
            rs_atomic_expected;                                                                           \
        })
    /* Atomic operations for pointers (word-sized on all supported platforms) */
    #define ATOMIC_LOAD_PTR(data, phlpmut) ((void *)__atomic_load_n((data), __ATOMIC_ACQUIRE))
    #define ATOMIC_STORE_PTR(data, phlpmut, val) ((void)__atomic_store_n((data), (val), __ATOMIC_RELEASE))
    #define ATOMIC_CAS_PTR(data, oldVal, newVal, phlpmut)                                                              \
        ({                                                                                                             \
            __typeof__(*(data)) rs_atomic_expected = (oldVal);                                                         \
            __atomic_compare_exchange_n((data), &rs_atomic_expected, (newVal), 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); \
        })

    /* functions below are not needed if we have atomics */
    #define DEF_ATOMIC_HELPER_MUT(x)
    #define INIT_ATOMIC_HELPER_MUT(x)
    #define DESTROY_ATOMIC_HELPER_MUT(x)

    /* the following operations should preferably be done atomically, but it is
     * not fatal if not -- that means we can live with some missed updates. So be
     * sure to use these macros only if that really does not matter!
     */
    /*
     * PREFER_* keeps its weak contract: the no-atomics build below still uses
     * plain accesses. PREFER_FETCH_32BIT callers pass a scalar lvalue, not a
     * pointer; the atomic branch takes its address locally.
     */
    #define PREFER_ATOMIC_INC(data) ((void)__atomic_fetch_add(&(data), 1, __ATOMIC_RELAXED))
    #define PREFER_LOAD_INT(data) ((int)__atomic_load_n((data), __ATOMIC_RELAXED))
    #define PREFER_FETCH_32BIT(data) ((unsigned)__atomic_load_n(&(data), __ATOMIC_RELAXED))
    #define PREFER_STORE_INT(data, val) ((void)__atomic_store_n((data), (val), __ATOMIC_RELAXED))
    #define PREFER_STORE_0_TO_INT(data) ((void)__atomic_store_n((data), 0, __ATOMIC_RELAXED))
    #define PREFER_STORE_1_TO_INT(data) ((void)__atomic_store_n((data), 1, __ATOMIC_RELAXED))
#else
    /* note that we gained practical proof that theoretical problems DO occur
     * if we do not properly address them. See this blog post for details:
     * http://blog.gerhards.net/2009/01/rsyslog-data-race-analysis.html
     * The bottom line is that if there are no atomics available, we should NOT
     * simply go ahead and do without them - use mutexes or other things. The
     * code needs to be checked against all those cases. -- rgerhards, 2009-01-30
     */
    #include <pthread.h>
    #define ATOMIC_INC(data, phlpmut)      \
        {                                  \
            pthread_mutex_lock(phlpmut);   \
            ++(*(data));                   \
            pthread_mutex_unlock(phlpmut); \
        }

    #define ATOMIC_OR_INT_TO_INT(data, hlpmut, val) \
        {                                           \
            pthread_mutex_lock(hlpmut);             \
            *(data) |= (val);                       \
            pthread_mutex_unlock(hlpmut);           \
        }

static inline int ATOMIC_CAS(int *data, int oldVal, int newVal, pthread_mutex_t *phlpmut) {
    int bSuccess;
    pthread_mutex_lock(phlpmut);
    if (*data == oldVal) {
        *data = newVal;
        bSuccess = 1;
    } else {
        bSuccess = 0;
    }
    pthread_mutex_unlock(phlpmut);
    return (bSuccess);
}

static inline int ATOMIC_CAS_time_t(time_t *data, time_t oldVal, time_t newVal, pthread_mutex_t *phlpmut) {
    int bSuccess;
    pthread_mutex_lock(phlpmut);
    if (*data == oldVal) {
        *data = newVal;
        bSuccess = 1;
    } else {
        bSuccess = 0;
    }
    pthread_mutex_unlock(phlpmut);
    return (bSuccess);
}


static inline int ATOMIC_CAS_VAL(int *data, int oldVal, int newVal, pthread_mutex_t *phlpmut) {
    int val;
    pthread_mutex_lock(phlpmut);
    val = *data;
    if (*data == oldVal) {
        *data = newVal;
    }
    pthread_mutex_unlock(phlpmut);
    return (val);
}

    #define ATOMIC_DEC(data, phlpmut)      \
        {                                  \
            pthread_mutex_lock(phlpmut);   \
            --(*(data));                   \
            pthread_mutex_unlock(phlpmut); \
        }

static inline int ATOMIC_INC_AND_FETCH_int(int *data, pthread_mutex_t *phlpmut) {
    int val;
    pthread_mutex_lock(phlpmut);
    val = ++(*data);
    pthread_mutex_unlock(phlpmut);
    return (val);
}

static inline unsigned ATOMIC_INC_AND_FETCH_unsigned(unsigned *data, pthread_mutex_t *phlpmut) {
    unsigned val;
    pthread_mutex_lock(phlpmut);
    val = ++(*data);
    pthread_mutex_unlock(phlpmut);
    return (val);
}

static inline int ATOMIC_DEC_AND_FETCH(int *data, pthread_mutex_t *phlpmut) {
    int val;
    pthread_mutex_lock(phlpmut);
    val = --(*data);
    pthread_mutex_unlock(phlpmut);
    return (val);
}

static inline int ATOMIC_LOAD_32BIT(int *data, pthread_mutex_t *phlpmut) {
    int val;
    pthread_mutex_lock(phlpmut);
    val = (*data);
    pthread_mutex_unlock(phlpmut);
    return val;
}

static inline int ATOMIC_LOAD_32BIT_RELAXED(int *data, pthread_mutex_t *phlpmut) {
    int val;
    pthread_mutex_lock(phlpmut);
    val = (*data);
    pthread_mutex_unlock(phlpmut);
    return val;
}

static inline void ATOMIC_STORE_32BIT(int *data, pthread_mutex_t *phlpmut, int val) {
    pthread_mutex_lock(phlpmut);
    *data = val;
    pthread_mutex_unlock(phlpmut);
}

static inline void ATOMIC_STORE_32BIT_RELAXED(int *data, pthread_mutex_t *phlpmut, int val) {
    pthread_mutex_lock(phlpmut);
    *data = val;
    pthread_mutex_unlock(phlpmut);
}

static inline unsigned ATOMIC_LOAD_32BIT_unsigned(unsigned *data, pthread_mutex_t *phlpmut) {
    unsigned val;
    pthread_mutex_lock(phlpmut);
    val = (*data);
    pthread_mutex_unlock(phlpmut);
    return val;
}

static inline unsigned ATOMIC_LOAD_32BIT_RELAXED_unsigned(unsigned *data, pthread_mutex_t *phlpmut) {
    unsigned val;
    pthread_mutex_lock(phlpmut);
    val = (*data);
    pthread_mutex_unlock(phlpmut);
    return val;
}

static inline void ATOMIC_STORE_32BIT_unsigned(unsigned *data, pthread_mutex_t *phlpmut, unsigned val) {
    pthread_mutex_lock(phlpmut);
    *data = val;
    pthread_mutex_unlock(phlpmut);
}

static inline void ATOMIC_STORE_32BIT_RELAXED_unsigned(unsigned *data, pthread_mutex_t *phlpmut, unsigned val) {
    pthread_mutex_lock(phlpmut);
    *data = val;
    pthread_mutex_unlock(phlpmut);
}

static inline void ATOMIC_SUB(int *data, int val, pthread_mutex_t *phlpmut) {
    pthread_mutex_lock(phlpmut);
    (*data) -= val;
    pthread_mutex_unlock(phlpmut);
}

static inline void ATOMIC_SUB_unsigned(unsigned *data, int val, pthread_mutex_t *phlpmut) {
    pthread_mutex_lock(phlpmut);
    (*data) -= val;
    pthread_mutex_unlock(phlpmut);
}

/* Atomic operations for pointers - fallback to mutex-protected operations */
static inline void *ATOMIC_LOAD_PTR(void **data, pthread_mutex_t *phlpmut) {
    void *val;
    pthread_mutex_lock(phlpmut);
    val = *data;
    pthread_mutex_unlock(phlpmut);
    return val;
}

static inline void ATOMIC_STORE_PTR(void **data, pthread_mutex_t *phlpmut, void *val) {
    pthread_mutex_lock(phlpmut);
    *data = val;
    pthread_mutex_unlock(phlpmut);
}

static inline int ATOMIC_CAS_PTR(void **data, void *oldVal, void *newVal, pthread_mutex_t *phlpmut) {
    int bSuccess;
    pthread_mutex_lock(phlpmut);
    if (*data == oldVal) {
        *data = newVal;
        bSuccess = 1;
    } else {
        bSuccess = 0;
    }
    pthread_mutex_unlock(phlpmut);
    return bSuccess;
}

    #define DEF_ATOMIC_HELPER_MUT(x) pthread_mutex_t x
    #define INIT_ATOMIC_HELPER_MUT(x) pthread_mutex_init(&(x), NULL)
    #define DESTROY_ATOMIC_HELPER_MUT(x) pthread_mutex_destroy(&(x))

    #define PREFER_ATOMIC_INC(data) ((void)++data)
    #define PREFER_LOAD_INT(data) (*(data))
    /* Caller passes the scalar object, not its address. No-atomic builds deliberately
     * use a plain read for these weak flags/counters where a racy value is acceptable.
     */
    #define PREFER_FETCH_32BIT(data) ((unsigned)(data))
    #define PREFER_STORE_INT(data, val) (*(data) = (val))
    #define PREFER_STORE_0_TO_INT(data) (*(data) = 0)
    #define PREFER_STORE_1_TO_INT(data) (*(data) = 1)

#endif

/* we need to handle 64bit atomics seperately as some platforms have
 * 32 bit atomics, but not 64 bit ones... -- rgerhards, 2010-12-01
 */
#ifdef HAVE_ATOMIC_BUILTINS64
    #define ATOMIC_INC_uint64(data, phlpmut) ((void)__atomic_fetch_add((data), 1, __ATOMIC_SEQ_CST))
    #define ATOMIC_ADD_uint64(data, phlpmut, value) ((void)__atomic_fetch_add((data), (value), __ATOMIC_SEQ_CST))
    #define ATOMIC_DEC_uint64(data, phlpmut) ((void)__atomic_sub_fetch((data), 1, __ATOMIC_SEQ_CST))
    #define ATOMIC_INC_AND_FETCH_uint64(data, phlpmut) __atomic_add_fetch((data), 1, __ATOMIC_SEQ_CST)
    #define ATOMIC_STORE_uint64(data, phlpmut, value) ((void)__atomic_store_n((data), (value), __ATOMIC_RELEASE))
    #define ATOMIC_INC_uint64_RELAXED(data, phlpmut) ((void)__atomic_fetch_add((data), 1, __ATOMIC_RELAXED))
    #define ATOMIC_ADD_uint64_RELAXED(data, phlpmut, value) \
        ((void)__atomic_fetch_add((data), (value), __ATOMIC_RELAXED))
    #define ATOMIC_DEC_uint64_RELAXED(data, phlpmut) ((void)__atomic_sub_fetch((data), 1, __ATOMIC_RELAXED))
    #define ATOMIC_LOAD_time_t_ACQUIRE(data, phlpmut) ((time_t)__atomic_load_n((data), __ATOMIC_ACQUIRE))
    #define ATOMIC_LOAD_time_t_RELAXED(data, phlpmut) ((time_t)__atomic_load_n((data), __ATOMIC_RELAXED))
    #define ATOMIC_STORE_time_t_RELAXED(data, phlpmut, value) \
        ((void)__atomic_store_n((data), (value), __ATOMIC_RELAXED))
    #define PREFER_LOAD_time_t(data) ((time_t)__atomic_load_n((data), __ATOMIC_RELAXED))
    #define PREFER_STORE_time_t(data, value) ((void)__atomic_store_n((data), (value), __ATOMIC_RELAXED))
    #define PREFER_LOAD_uint64(data) ((uint64)__atomic_load_n((data), __ATOMIC_RELAXED))
    #define PREFER_STORE_uint64(data, value) ((void)__atomic_store_n((data), (value), __ATOMIC_RELAXED))

    #define DEF_ATOMIC_HELPER_MUT64(x)
    #define INIT_ATOMIC_HELPER_MUT64(x)
    #define DESTROY_ATOMIC_HELPER_MUT64(x)
#else
    #define ATOMIC_INC_uint64(data, phlpmut) \
        {                                    \
            pthread_mutex_lock(phlpmut);     \
            ++(*(data));                     \
            pthread_mutex_unlock(phlpmut);   \
        }
    #define ATOMIC_ADD_uint64(data, phlpmut, value) \
        {                                           \
            pthread_mutex_lock(phlpmut);            \
            *data += value;                         \
            pthread_mutex_unlock(phlpmut);          \
        }
    #define ATOMIC_DEC_uint64(data, phlpmut) \
        {                                    \
            pthread_mutex_lock(phlpmut);     \
            --(*(data));                     \
            pthread_mutex_unlock(phlpmut);   \
        }
    #define ATOMIC_INC_uint64_RELAXED(data, phlpmut) ATOMIC_INC_uint64(data, phlpmut)
    #define ATOMIC_ADD_uint64_RELAXED(data, phlpmut, value) ATOMIC_ADD_uint64(data, phlpmut, value)
    #define ATOMIC_DEC_uint64_RELAXED(data, phlpmut) ATOMIC_DEC_uint64(data, phlpmut)
    #define PREFER_LOAD_time_t(data) (*(data))
    #define PREFER_STORE_time_t(data, value) (*(data) = (value))
    #define PREFER_LOAD_uint64(data) (*(data))
    #define PREFER_STORE_uint64(data, value) (*(data) = (value))
    #define ATOMIC_STORE_uint64(data, phlpmut, value) \
        {                                             \
            pthread_mutex_lock(phlpmut);              \
            *(data) = (value);                        \
            pthread_mutex_unlock(phlpmut);            \
        }

static inline time_t ATOMIC_LOAD_time_t_ACQUIRE(time_t *data, pthread_mutex_t *phlpmut) {
    time_t val;
    pthread_mutex_lock(phlpmut);
    val = *data;
    pthread_mutex_unlock(phlpmut);
    return val;
}

static inline time_t ATOMIC_LOAD_time_t_RELAXED(time_t *data, pthread_mutex_t *phlpmut) {
    time_t val;
    pthread_mutex_lock(phlpmut);
    val = *data;
    pthread_mutex_unlock(phlpmut);
    return val;
}

static inline void ATOMIC_STORE_time_t_RELAXED(time_t *data, pthread_mutex_t *phlpmut, time_t value) {
    pthread_mutex_lock(phlpmut);
    *data = value;
    pthread_mutex_unlock(phlpmut);
}

static inline uint64 ATOMIC_INC_AND_FETCH_uint64(uint64 *data, pthread_mutex_t *phlpmut) {
    uint64 val;
    pthread_mutex_lock(phlpmut);
    val = ++(*data);
    pthread_mutex_unlock(phlpmut);
    return (val);
}

    #define DEF_ATOMIC_HELPER_MUT64(x) pthread_mutex_t x
    #define INIT_ATOMIC_HELPER_MUT64(x) pthread_mutex_init(&(x), NULL)
    #define DESTROY_ATOMIC_HELPER_MUT64(x) pthread_mutex_destroy(&(x))
#endif /* #ifdef HAVE_ATOMIC_BUILTINS64 */

#endif /* #ifndef INCLUDED_ATOMIC_H */
