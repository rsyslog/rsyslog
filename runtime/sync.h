/* Definitions syncrhonization-related stuff. In theory, that should
 * help porting to something different from pthreads.
 *
 * Copyright 2007 Rainer Gerhards and Adiscon GmbH.
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

#ifndef INCLUDED_SYNC_H
#define INCLUDED_SYNC_H

#include <pthread.h>

/* SYNC_OBJ_TOOL definition must be placed in object to be synced!
 * SYNC_OBJ_TOOL_INIT must be called upon of object construction and
 * SUNC_OBJ_TOOL_EXIT must be called upon object destruction
 */
#define SYNC_OBJ_TOOL pthread_mutex_t *Sync_mut
#define SYNC_OBJ_TOOL_INIT(x) SyncObjInit(&((x)->Sync_mut))
#define SYNC_OBJ_TOOL_EXIT(x) SyncObjExit(&((x)->Sync_mut))

/* If we run in non-debug (release) mode, we use inline code for the mutex
 * operations. If we run in debug mode, we use functions, because they
 * are better to trace in the stackframe.
 */
#define LockObj(x) d_pthread_mutex_lock((x)->Sync_mut)
#define UnlockObj(x) d_pthread_mutex_unlock((x)->Sync_mut)

void SyncObjInit(pthread_mutex_t **mut);
void SyncObjExit(pthread_mutex_t **mut);
extern void lockObj(pthread_mutex_t *mut);
extern void unlockObj(pthread_mutex_t *mut);

#endif /* #ifndef INCLUDED_SYNC_H */
