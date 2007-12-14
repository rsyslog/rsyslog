/* Definitions syncrhonization-related stuff. In theory, that should
 * help porting to something different from pthreads.
 *
 * Copyright 2007 Rainer Gerhards and Adiscon GmbH.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */

#ifndef INCLUDED_SYNC_H
#define INCLUDED_SYNC_H

#ifdef USE_PTHREADS /* Code to compile for threading support */
#include <pthread.h>

/* SYNC_OBJ_TOOL definition must be placed in object to be synced!
 * SYNC_OBJ_TOOL_INIT must be called upon of object construction and
 * SUNC_OBJ_TOOL_EXIT must be called upon object destruction
 */
#define SYNC_OBJ_TOOL pthread_mutex_t *Sync_mut;
#define SYNC_OBJ_TOOL_INIT(x) SyncObjInit(&((x)->Sync_mut))
#define SYNC_OBJ_TOOL_EXIT(x) SyncObjExit(&((x)->Sync_mut))

/* If we run in non-debug (release) mode, we use inline code for the mutex
 * operations. If we run in debug mode, we use functions, because they
 * are better to trace in the stackframe.
 */
#ifdef	NDEBUG
#define LockObj(x) pthread_mutex_lock((x)->Sync_mut)
#define UnlockObj(x) pthread_mutex_unlock((x)->Sync_mut)
#else
#define LockObj(x) lockObj((x)->Sync_mut)
#define UnlockObj(x) unlockObj((x)->Sync_mut)
#endif

void SyncObjInit(pthread_mutex_t **mut);
void SyncObjExit(pthread_mutex_t **mut);
extern void lockObj(pthread_mutex_t *mut);
extern void unlockObj(pthread_mutex_t *mut);

#else /* Code not to compile for threading support */
#define SYNC_OBJ_TOOL
#define SYNC_OBJ_TOOL_INIT(x)
#define SYNC_OBJ_TOOL_EXIT(X)
#define LockObj(x)
#define UnlockObj(x)
#endif

#endif /* #ifndef INCLUDED_SYNC_H */
