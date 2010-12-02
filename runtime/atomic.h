/* This header supplies atomic operations. So far, we rely on GCC's
 * atomic builtins. During configure, we check if atomic operatons are
 * available. If they are not, I am making the necessary provisioning to live without them if
 * they are not available. Please note that you should only use the macros
 * here if you think you can actually live WITHOUT an explicit atomic operation,
 * because in the non-presence of them, we simply do it without atomicitiy.
 * Which, for word-aligned data types, usually (but only usually!) should work.
 *
 * We are using the functions described in
 * http:/gcc.gnu.org/onlinedocs/gcc/Atomic-Builtins.html
 *
 * THESE MACROS MUST ONLY BE USED WITH WORD-SIZED DATA TYPES!
 *
 * Copyright 2008 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
 *
 * The rsyslog runtime library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The rsyslog runtime library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the rsyslog runtime library.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 * A copy of the LGPL can be found in the file "COPYING.LESSER" in this distribution.
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
#	define ATOMIC_SUB(data, val, phlpmut) __sync_fetch_and_sub(data, val)
#	define ATOMIC_ADD(data, val) __sync_fetch_and_add(&(data), val)
#	define ATOMIC_INC(data, phlpmut) ((void) __sync_fetch_and_add(data, 1))
#	define ATOMIC_INC_AND_FETCH_int(data, phlpmut) __sync_fetch_and_add(data, 1)
#	define ATOMIC_INC_AND_FETCH_unsigned(data, phlpmut) __sync_fetch_and_add(data, 1)
#	define ATOMIC_DEC(data, phlpmut) ((void) __sync_sub_and_fetch(data, 1))
#	define ATOMIC_DEC_AND_FETCH(data, phlpmut) __sync_sub_and_fetch(data, 1)
#	define ATOMIC_FETCH_32BIT(data, phlpmut) ((unsigned) __sync_fetch_and_and(data, 0xffffffff))
#	define ATOMIC_STORE_1_TO_32BIT(data) __sync_lock_test_and_set(&(data), 1)
#	define ATOMIC_STORE_0_TO_INT(data, phlpmut) __sync_fetch_and_and(data, 0)
#	define ATOMIC_STORE_1_TO_INT(data, phlpmut) __sync_fetch_and_or(data, 1)
#	define ATOMIC_STORE_INT_TO_INT(data, val) __sync_fetch_and_or(&(data), (val))
#	define ATOMIC_CAS(data, oldVal, newVal, phlpmut) __sync_bool_compare_and_swap(data, (oldVal), (newVal))
#	define ATOMIC_CAS_time_t(data, oldVal, newVal, phlpmut) __sync_bool_compare_and_swap(data, (oldVal), (newVal))
#	define ATOMIC_CAS_VAL(data, oldVal, newVal, phlpmut) __sync_val_compare_and_swap(data, (oldVal), (newVal));

	/* functions below are not needed if we have atomics */
#	define DEF_ATOMIC_HELPER_MUT(x)
#	define INIT_ATOMIC_HELPER_MUT(x)
#	define DESTROY_ATOMIC_HELPER_MUT(x) 

	/* the following operations should preferrably be done atomic, but it is
	 * not fatal if not -- that means we can live with some missed updates. So be
	 * sure to use these macros only if that really does not matter!
	 */
#	define PREFER_ATOMIC_INC(data) ((void) __sync_fetch_and_add(&(data), 1))
#else
	/* note that we gained parctical proof that theoretical problems DO occur
	 * if we do not properly address them. See this blog post for details:
	 * http://blog.gerhards.net/2009/01/rsyslog-data-race-analysis.html
	 * The bottom line is that if there are no atomics available, we should NOT
	 * simply go ahead and do without them - use mutexes or other things. The
	 * code needs to be checked against all those cases. -- rgerhards, 2009-01-30
	 */
	#include <pthread.h>
#	define ATOMIC_INC(data, phlpmut)  { \
		pthread_mutex_lock(phlpmut); \
		++(*(data)); \
		pthread_mutex_unlock(phlpmut); \
	}

#	define ATOMIC_STORE_0_TO_INT(data, hlpmut)  { \
		pthread_mutex_lock(hlpmut); \
		*(data) = 0; \
		pthread_mutex_unlock(hlpmut); \
	}

#	define ATOMIC_STORE_1_TO_INT(data, hlpmut) { \
		pthread_mutex_lock(hlpmut); \
		*(data) = 1; \
		pthread_mutex_unlock(hlpmut); \
	}

	static inline int
	ATOMIC_CAS(int *data, int oldVal, int newVal, pthread_mutex_t *phlpmut) {
		int bSuccess;
		pthread_mutex_lock(phlpmut);
		if(*data == oldVal) {
			*data = newVal;
			bSuccess = 1;
		} else {
			bSuccess = 0;
		}
		pthread_mutex_unlock(phlpmut);
		return(bSuccess);
	}

	static inline int
	ATOMIC_CAS_time_t(time_t *data, time_t oldVal, time_t newVal, pthread_mutex_t *phlpmut) {
		int bSuccess;
		pthread_mutex_lock(phlpmut);
		if(*data == oldVal) {
			*data = newVal;
			bSuccess = 1;
		} else {
			bSuccess = 0;
		}
		pthread_mutex_unlock(phlpmut);
		return(bSuccess);
	}


	static inline int
	ATOMIC_CAS_VAL(int *data, int oldVal, int newVal, pthread_mutex_t *phlpmut) {
		int val;
		pthread_mutex_lock(phlpmut);
		if(*data == oldVal) {
			*data = newVal;
		}
		val = *data;
		pthread_mutex_unlock(phlpmut);
		return(val);
	}

#	define ATOMIC_DEC(data, phlpmut)  { \
		pthread_mutex_lock(phlpmut); \
		--(*(data)); \
		pthread_mutex_unlock(phlpmut); \
	}

	static inline int
	ATOMIC_INC_AND_FETCH_int(int *data, pthread_mutex_t *phlpmut) {
		int val;
		pthread_mutex_lock(phlpmut);
		val = ++(*data);
		pthread_mutex_unlock(phlpmut);
		return(val);
	}

	static inline unsigned
	ATOMIC_INC_AND_FETCH_unsigned(unsigned *data, pthread_mutex_t *phlpmut) {
		unsigned val;
		pthread_mutex_lock(phlpmut);
		val = ++(*data);
		pthread_mutex_unlock(phlpmut);
		return(val);
	}

	static inline int
	ATOMIC_DEC_AND_FETCH(int *data, pthread_mutex_t *phlpmut) {
		int val;
		pthread_mutex_lock(phlpmut);
		val = --(*data);
		pthread_mutex_unlock(phlpmut);
		return(val);
	}

	static inline int
	ATOMIC_FETCH_32BIT(int *data, pthread_mutex_t *phlpmut) {
		int val;
		pthread_mutex_lock(phlpmut);
		val = (*data);
		pthread_mutex_unlock(phlpmut);
		return(val);
	}

	static inline void
	ATOMIC_SUB(int *data, int val, pthread_mutex_t *phlpmut) {
		pthread_mutex_lock(phlpmut);
		(*data) -= val;
		pthread_mutex_unlock(phlpmut);
	}
#	define DEF_ATOMIC_HELPER_MUT(x)  pthread_mutex_t x
#	define INIT_ATOMIC_HELPER_MUT(x) pthread_mutex_init(&(x), NULL)
#	define DESTROY_ATOMIC_HELPER_MUT(x) pthread_mutex_destroy(&(x))

#	define PREFER_ATOMIC_INC(data) ((void) ++data)

#endif

/* we need to handle 64bit atomics seperately as some platforms have 
 * 32 bit atomics, but not 64 biot ones... -- rgerhards, 2010-12-01
 */
#ifdef HAVE_ATOMIC_BUILTINS_64BIT
#	define ATOMIC_INC_uint64(data, phlpmut) ((void) __sync_fetch_and_add(data, 1))
#	define ATOMIC_DEC_unit64(data, phlpmut) ((void) __sync_sub_and_fetch(data, 1))
#	define ATOMIC_INC_AND_FETCH_uint64(data, phlpmut) __sync_fetch_and_add(data, 1)

#	define DEF_ATOMIC_HELPER_MUT64(x)
#	define INIT_ATOMIC_HELPER_MUT64(x)
#	define DESTROY_ATOMIC_HELPER_MUT64(x) 
#else
#	define ATOMIC_INC_uint64(data, phlpmut)  { \
		pthread_mutex_lock(phlpmut); \
		++(*(data)); \
		pthread_mutex_unlock(phlpmut); \
	}
#	define ATOMIC_DEC_uint64(data, phlpmut)  { \
		pthread_mutex_lock(phlpmut); \
		--(*(data)); \
		pthread_mutex_unlock(phlpmut); \
	}

	static inline unsigned
	ATOMIC_INC_AND_FETCH_uint64(uint64 *data, pthread_mutex_t *phlpmut) {
		uint64 val;
		pthread_mutex_lock(phlpmut);
		val = ++(*data);
		pthread_mutex_unlock(phlpmut);
		return(val);
	}

#	define DEF_ATOMIC_HELPER_MUT64(x)  pthread_mutex_t x
#	define INIT_ATOMIC_HELPER_MUT64(x) pthread_mutex_init(&(x), NULL)
#	define DESTROY_ATOMIC_HELPER_MUT64(x) pthread_mutex_destroy(&(x))
#endif /* #ifdef HAVE_ATOMIC_BUILTINS_64BIT */

#endif /* #ifndef INCLUDED_ATOMIC_H */
