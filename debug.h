/* debug.h
 *
 * Definitions for the debug and run-time analysis support module.
 * Contains a lot of macros.
 *
 * Copyright 2008 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Rsyslog is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rsyslog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Rsyslog.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */
#ifndef DEBUG_H_INCLUDED
#define DEBUG_H_INCLUDED

#include <pthread.h>

/* external static data elements (some time to be replaced) */
extern int Debug;		/* debug flag  - read-only after startup */
extern int debugging_on;	 /* read-only, except on sig USR1 */

/* prototypes */
rsRetVal dbgClassInit(void);
rsRetVal dbgClassExit(void);
void sigsegvHdlr(int signum);
int dbgCondTimedWait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime,
		     const char *file, const char* func, int line);
int dbgCondWait(pthread_cond_t *cond, pthread_mutex_t *mutex, const char *file, const char* func, int line);
int dbgMutexUnlock(pthread_mutex_t *pmut, const char *file, const char* func, int line);
int dbgMutexLock(pthread_mutex_t *pmut, const char *file, const char* func, int line);
void dbgprintf(char *fmt, ...) __attribute__((format(printf,1, 2)));
int dbgEntrFunc(char* file, int line, const char* func);
void dbgExitFunc(int iStackPtrRestore, char* file, int line, const char* func);

/* macros */
#if 1 /* DEV debug: set to 1 to get a rough call trace -- rgerhards, 2008-01-13 */
#	define BEGINfunc int dbgCALLStaCK_POP_POINT = dbgEntrFunc(__FILE__, __LINE__, __func__);
#	define ENDfunc dbgExitFunc(dbgCALLStaCK_POP_POINT, __FILE__, __LINE__, __func__);
#else
#	define BEGINfunc
#	define ENDfunc
#endif
#if 1 /* DEV debug: set to 1 to enable -- rgerhards, 2008-01-13 */
#	define RUNLOG dbgprintf("%s:%d: %s: log point\n", __FILE__, __LINE__, __func__)
#	define RUNLOG_VAR(fmt, x) dbgprintf("%s:%d: %s: var '%s'[%s]: " fmt "\n", __FILE__, __LINE__, __func__, #x, fmt, x)
#else
#	define RUNLOG
#	define RUNLOG_VAR(x)
#endif

/* mutex operations */
#define MUTOP_LOCKWAIT		1
#define MUTOP_LOCK		2
#define MUTOP_UNLOCK		3


/* debug aides */
#if 1
#define d_pthread_mutex_lock(x)     dbgMutexLock(x, __FILE__, __func__, __LINE__)
#define d_pthread_mutex_unlock(x)   dbgMutexUnlock(x, __FILE__, __func__, __LINE__)
#define d_pthread_cond_wait(cond, mut)   dbgCondWait(cond, mut, __FILE__, __func__, __LINE__)
#define d_pthread_cond_timedwait(cond, mut, to)   dbgCondTimedWait(cond, mut, to, __FILE__, __func__, __LINE__)
#else
#define d_pthread_mutex_lock(x)     pthread_mutex_lock(x)
#define d_pthread_mutex_unlock(x)   pthread_mutex_unlock(x)
#define d_pthread_cond_wait(cond, mut)   pthread_cond_wait(cond, mut)
#define d_pthread_cond_timedwait(cond, mut, to)   pthread_cond_timedwait(cond, mut, to)
#endif
#endif /* #ifndef DEBUG_H_INCLUDED */
