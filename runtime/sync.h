/* Definitions syncrhonization-related stuff. In theory, that should
 * help porting to something different from pthreads.
 *
 * Copyright 2007-2012 Adiscon GmbH.
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
