/* debug.h
 *
 * Definitions for the debug and run-time analysis support module.
 * Contains a lot of macros.
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
#ifndef DEBUG_H_INCLUDED
#define DEBUG_H_INCLUDED

#include <pthread.h>
#include "obj-types.h"

/* some settings for various debug modes */
#define DEBUG_OFF	0
#define DEBUG_ONDEMAND	1
#define DEBUG_FULL	2

/* external static data elements (some time to be replaced) */
extern int Debug;		/* debug flag  - read-only after startup */
extern int debugging_on;	 /* read-only, except on sig USR1 */
extern int stddbg; /* the handle for regular debug output, set to stdout if not forking, -1 otherwise */

/* data types */

/* the function database. It is used as a static var inside each function. That provides
 * us the fast access to it that we need to make the instrumentation work. It's address
 * also serves as a unique function identifier and can be used inside other structures
 * to refer to the function (e.g. for pretty-printing names).
 * rgerhards, 2008-01-24
 */
typedef struct dbgFuncDBmutInfoEntry_s {
	pthread_mutex_t *pmut;
	int lockLn; /* line where it was locked (inside our func): -1 means mutex is not locked */
	pthread_t thrd; /* thrd where the mutex was locked */
	unsigned long lInvocation; /* invocation (unique during program run!) of this function that locked the mutex */
} dbgFuncDBmutInfoEntry_t;
typedef struct dbgFuncDB_s {
	unsigned magic;
	unsigned long nTimesCalled;
	char *func;
	char *file;
	int line;
	dbgFuncDBmutInfoEntry_t mutInfo[5];
	/* remember to update the initializer if you add anything or change the order! */
} dbgFuncDB_t;
#define dbgFUNCDB_MAGIC 0xA1B2C3D4
#define dbgFuncDB_t_INITIALIZER \
	{ \
	.magic = dbgFUNCDB_MAGIC,\
	.nTimesCalled = 0,\
	.func = __func__, \
	.file = __FILE__, \
	.line = __LINE__ \
	}

/* the structure below was originally just the thread's call stack, but it has
 * a bit evolved over time. So we have now ended up with the fact that it 
 * all debug info we know about the thread.
 */
typedef struct dbgCallStack_s {
	pthread_t thrd;
	dbgFuncDB_t *callStack[500];
	int lastLine[500]; /* last line where code execution was seen */
	int stackPtr;
	int stackPtrMax;
	char *pszThrdName;
	struct dbgCallStack_s *pNext;
	struct dbgCallStack_s *pPrev;
} dbgThrdInfo_t;


/* prototypes */
rsRetVal dbgClassInit(void);
rsRetVal dbgClassExit(void);
void dbgSetDebugFile(uchar *fn);
void dbgSetDebugLevel(int level);
void sigsegvHdlr(int signum);
void dbgoprint(obj_t *pObj, char *fmt, ...) __attribute__((format(printf, 2, 3)));
void dbgprintf(char *fmt, ...) __attribute__((format(printf, 1, 2)));
int dbgMutexLock(pthread_mutex_t *pmut, dbgFuncDB_t *pFuncD, int ln, int iStackPtr);
int dbgMutexTryLock(pthread_mutex_t *pmut, dbgFuncDB_t *pFuncD, int ln, int iStackPtr);
int dbgMutexUnlock(pthread_mutex_t *pmut, dbgFuncDB_t *pFuncD, int ln, int iStackPtr);
int dbgCondWait(pthread_cond_t *cond, pthread_mutex_t *pmut, dbgFuncDB_t *pFuncD, int ln, int iStackPtr);
int dbgCondTimedWait(pthread_cond_t *cond, pthread_mutex_t *pmut, const struct timespec *abstime, dbgFuncDB_t *pFuncD, int ln, int iStackPtr);
void dbgFree(void *pMem, dbgFuncDB_t *pFuncDB, int ln, int iStackPtr);
int dbgEntrFunc(dbgFuncDB_t **ppFuncDB, const char *file, const char *func, int line);
void dbgExitFunc(dbgFuncDB_t *pFuncDB, int iStackPtrRestore, int iRet);
void dbgSetExecLocation(int iStackPtr, int line);
void dbgSetThrdName(uchar *pszName);
void dbgPrintAllDebugInfo(void);
void *dbgmalloc(size_t size);
void dbgOutputTID(char* name);
int dbgGetDbglogFd(void);

/* macros */
#ifdef DEBUGLESS
#	define DBGPRINTF(...) {}
#	define DBGOPRINT(...) {}
#else
#	define DBGPRINTF(...) if(Debug) { dbgprintf(__VA_ARGS__); }
#	define DBGOPRINT(...) if(Debug) { dbgoprint(__VA_ARGS__); }
#endif
#ifdef RTINST
#	define BEGINfunc static dbgFuncDB_t *pdbgFuncDB; int dbgCALLStaCK_POP_POINT = dbgEntrFunc(&pdbgFuncDB, __FILE__, __func__, __LINE__);
#	define ENDfunc dbgExitFunc(pdbgFuncDB, dbgCALLStaCK_POP_POINT, RS_RET_NO_IRET);
#	define ENDfuncIRet dbgExitFunc(pdbgFuncDB, dbgCALLStaCK_POP_POINT, iRet);
#	define ASSERT(x) assert(x)
#else
#	define BEGINfunc
#	define ENDfunc
#	define ENDfuncIRet
#	define ASSERT(x)
#endif
#ifdef RTINST
#	define RUNLOG dbgSetExecLocation(dbgCALLStaCK_POP_POINT, __LINE__); dbgprintf("%s:%d: %s: log point\n", __FILE__, __LINE__, __func__)
#	define RUNLOG_VAR(fmt, x) dbgSetExecLocation(dbgCALLStaCK_POP_POINT, __LINE__);\
	 		          dbgprintf("%s:%d: %s: var '%s'[%s]: " fmt "\n", __FILE__, __LINE__, __func__, #x, fmt, x)
#	define RUNLOG_STR(str)    dbgSetExecLocation(dbgCALLStaCK_POP_POINT, __LINE__);\
				  dbgprintf("%s:%d: %s: %s\n", __FILE__, __LINE__, __func__, str)
#else
#	define RUNLOG
#	define RUNLOG_VAR(fmt, x)
#	define RUNLOG_STR(str)
#endif

#ifdef MEMCHECK
#	define MALLOC(x) dbgmalloc(x)
#else
#	define MALLOC(x) malloc(x)
#endif

/* mutex operations */
#define MUTOP_LOCKWAIT		1
#define MUTOP_LOCK		2
#define MUTOP_UNLOCK		3
#define MUTOP_TRYLOCK		4


/* debug aides */
#ifdef RTINST
#define d_pthread_mutex_lock(x)      dbgMutexLock(x, pdbgFuncDB, __LINE__, dbgCALLStaCK_POP_POINT )
#define d_pthread_mutex_trylock(x)   dbgMutexTryLock(x, pdbgFuncDB, __LINE__, dbgCALLStaCK_POP_POINT )
#define d_pthread_mutex_unlock(x)    dbgMutexUnlock(x, pdbgFuncDB, __LINE__, dbgCALLStaCK_POP_POINT )
#define d_pthread_cond_wait(cond, mut)   dbgCondWait(cond, mut, pdbgFuncDB, __LINE__, dbgCALLStaCK_POP_POINT )
#define d_pthread_cond_timedwait(cond, mut, to)   dbgCondTimedWait(cond, mut, to, pdbgFuncDB, __LINE__, dbgCALLStaCK_POP_POINT )
#define d_free(x)      dbgFree(x, pdbgFuncDB, __LINE__, dbgCALLStaCK_POP_POINT )
#else
#define d_pthread_mutex_lock(x)     pthread_mutex_lock(x)
#define d_pthread_mutex_trylock(x)  pthread_mutex_trylock(x)
#define d_pthread_mutex_unlock(x)   pthread_mutex_unlock(x)
#define d_pthread_cond_wait(cond, mut)   pthread_cond_wait(cond, mut)
#define d_pthread_cond_timedwait(cond, mut, to)   pthread_cond_timedwait(cond, mut, to)
#define d_free(x)      free(x)
#endif
#endif /* #ifndef DEBUG_H_INCLUDED */
