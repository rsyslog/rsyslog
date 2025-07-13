/* debug.h
 *
 * Definitions for the debug module.
 *
 * Copyright 2008-2018 Rainer Gerhards and Adiscon GmbH.
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
#ifndef DEBUG_H_INCLUDED
#define DEBUG_H_INCLUDED

#include <pthread.h>
#include "obj-types.h"

/* some settings for various debug modes */
#define DEBUG_OFF   0
#define DEBUG_ONDEMAND  1
#define DEBUG_FULL  2

/* external static data elements (some time to be replaced) */
extern int Debug;       /* debug flag  - read-only after startup */
extern int debugging_on;     /* read-only, except on sig USR1 */
extern int stddbg; /* the handle for regular debug output, set to stdout if not forking, -1 otherwise */
extern int dbgTimeoutToStderr;


/* prototypes */
rsRetVal dbgClassInit(void);
rsRetVal dbgClassExit(void);
void dbgSetDebugFile(uchar *fn);
void dbgSetDebugLevel(int level);
void dbgSetThrdName(const uchar *pszName);
void dbgOutputTID(char* name);
int dbgGetDbglogFd(void);

/* external data */
extern char *pszAltDbgFileName; /* if set, debug output is *also* sent to here */
extern int altdbg;  /* and the handle for alternate debug output */

/* macros */
#ifdef DEBUGLESS
#   define DBGL_UNUSED __attribute__((__unused__))
    static inline void r_dbgoprint(const char DBGL_UNUSED *srcname, obj_t DBGL_UNUSED *pObj,
                             const char DBGL_UNUSED *fmt, ...) {}
    static inline void r_dbgprintf(const char DBGL_UNUSED *srcname, const char DBGL_UNUSED *fmt, ...) {}
#else
#   define DBGL_UNUSED
    void r_dbgoprint(const char *srcname, obj_t *pObj, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
    void r_dbgprintf(const char *srcname, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
#endif

#define DBGPRINTF(...) if(Debug) { r_dbgprintf(__FILE__, __VA_ARGS__); }
#define DBGOPRINT(...) if(Debug) { r_dbgoprint(__FILE__, __VA_ARGS__); }
#define dbgprintf(...) r_dbgprintf(__FILE__, __VA_ARGS__)
#define dbgoprint(...) r_dbgoprint(__FILE__, __VA_ARGS__)

/* things originally introduced for now removed rtinst */
#define d_pthread_mutex_lock(x)     pthread_mutex_lock(x)
#define d_pthread_mutex_trylock(x)  pthread_mutex_trylock(x)
#define d_pthread_mutex_unlock(x)   pthread_mutex_unlock(x)
#define d_pthread_cond_wait(cond, mut)   pthread_cond_wait(cond, mut)
#define d_pthread_cond_timedwait(cond, mut, to)   pthread_cond_timedwait(cond, mut, to)

#endif /* #ifndef DEBUG_H_INCLUDED */
