/* debug.c
 *
 * This file proides debug and run time error analysis support. Some of the
 * settings are very performance intense and my be turned off during a release
 * build.
 * 
 * File begun on 2008-01-22 by RGerhards
 *
 * There is some in-depth documentation available in doc/dev_queue.html
 * (and in the web doc set on http://www.rsyslog.com/doc). Be sure to read it
 * if you are getting aquainted to the object.
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


#include "config.h" /* autotools! */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <assert.h>

#include "rsyslog.h"
#include "debug.h"

/* static data (some time to be replaced) */
int	Debug;		/* debug flag  - read-only after startup */
int debugging_on = 0;	 /* read-only, except on sig USR1 */
static int bLogFuncFlow = 0; /* shall the function entry and exit be logged to the debug log? */

static FILE *stddbg;

typedef struct dbgMutLog_s {
	struct dbgMutLog_s *pNext;
	struct dbgMutLog_s *pPrev;
	pthread_mutex_t *mut;
	pthread_t thrd;
	const char *file;
	short ln;	/* more than 64K source lines are forbidden ;) */
	short mutexOp;
} dbgMutLog_t;
static dbgMutLog_t *dbgMutLogListRoot = NULL;
static dbgMutLog_t *dbgMutLogListLast = NULL;
static pthread_mutex_t mutMutLog = PTHREAD_MUTEX_INITIALIZER;

typedef struct dbgCallStack_s {
	pthread_t thrd;
	const char* callStack[5000];
	int stackPtr;
	struct dbgCallStack_s *pNext;
	struct dbgCallStack_s *pPrev;
} dbgCallStack_t;
static dbgCallStack_t *dbgCallStackListRoot = NULL;
static dbgCallStack_t *dbgCallStackListLast = NULL;
static pthread_mutex_t mutCallStack = PTHREAD_MUTEX_INITIALIZER;

static pthread_mutex_t mutdbgprintf = PTHREAD_MUTEX_INITIALIZER;

static pthread_key_t keyCallStack;


/* we do not have templates, so we use some macros to create linked list handlers
 * for the several types
 * DLL means "doubly linked list"
 * rgerhards, 2008-01-23
 */
#define DLL_Del(type, pThis) \
	if(pThis->pPrev != NULL) \
		pThis->pPrev->pNext = pThis->pNext; \
	if(pThis->pNext != NULL) \
		pThis->pNext->pPrev = pThis->pPrev; \
	if(pThis == dbg##type##ListRoot) \
		dbg##type##ListRoot = pThis->pNext; \
	if(pThis == dbg##type##ListLast) \
		dbg##type##ListLast = pThis->pPrev; \
dbgprintf("DLL del %s entry %p plast %p\n", #type, pThis, dbg##type##ListLast); \
	free(pThis);

#define DLL_Add(type, pThis) \
dbgprintf("DLL add %s entry %p\n", #type, pThis); \
		if(dbg##type##ListRoot == NULL) { \
dbgprintf("DLL_add %s Root NULL\n", #type); \
			dbg##type##ListRoot = pThis; \
			dbg##type##ListLast = pThis; \
		} else { \
			pThis->pPrev = dbg##type##ListLast; \
			dbg##type##ListLast->pNext = pThis; \
			dbg##type##ListLast = pThis; \
dbgprintf("DLL_add %s Root NON-NULL, pThis %p, prev %p\n", #type, pThis, pThis->pPrev); \
		}

/* we need to do our own mutex cancel cleanup handler as it shall not
 * be subject to the debugging instrumentation (that would probably run us
 * into an infinite loop
 */
static void dbgMutexCancelCleanupHdlr(void *pmut)
{
	pthread_mutex_unlock((pthread_mutex_t*) pmut);
}


/* ------------------------- mutex tracking code ------------------------- */ 

/* ###########################################################################
 * 				IMPORTANT NOTE
 * Mutex instrumentation reduces the code's concurrency and thus affects its
 * order of execution. It is vital to test the code also with mutex 
 * instrumentation turned off! Some bugs may not show up while it on...
 * ###########################################################################
 */

/* constructor & add new entry to list
 */
dbgMutLog_t *dbgMutLogAddEntry(pthread_mutex_t *pmut, short mutexOp, const char *file, int line)
{
	dbgMutLog_t *pLog;

	pLog = calloc(1, sizeof(dbgMutLog_t));
	assert(pLog != NULL);

dbgprintf("mutexlockaddEntry 1\n");
	/* fill data members */
	pLog->mut = pmut;
	pLog->thrd = pthread_self();
	pLog->mutexOp = mutexOp;
	pLog->file = file;
	pLog->ln = line;
dbgprintf("mutexlockaddEntry 2\n");

dbgprintf("mutex %p: op %d\n", pLog, pLog->mutexOp);
	DLL_Add(MutLog, pLog);
dbgprintf("mutexlockaddEntry 3, entry %p added\n", pLog);

	return pLog;
}


/* destruct log entry
 */
void dbgMutLogDelEntry(dbgMutLog_t *pLog)
{
	assert(pLog != NULL);
	DLL_Del(MutLog, pLog);
}


/* find the last log entry for that specific mutex object. Is used to delete
 * a thread's own requests. Searches occur from the back.
 */
dbgMutLog_t *dbgMutLogFindSpecific(pthread_mutex_t *pmut, short mutop, const char *file, int line)
{
	dbgMutLog_t *pLog;
	pthread_t mythrd = pthread_self();
	
dbgprintf("reqeusted to search for op %d\n", mutop);
	pLog = dbgMutLogListLast;
	while(pLog != NULL) {
dbgprintf("FindSpecific %p: mut %p == %p, thrd %lx == %lx, op %d == %d, ln %d == %d, file '%s' = '%s'\n", pLog,
		 pLog->mut, pmut, pLog->thrd, mythrd, pLog->mutexOp, mutop, pLog->ln, line, pLog->file, file);

		if(   pLog->mut == pmut && pLog->thrd == mythrd && pLog->mutexOp == mutop
		   && !strcmp(pLog->file, file) && pLog->ln == line)
			break;
		pLog = pLog->pPrev;
	}

	return pLog;
}


/* find mutex object from the back of the list */
dbgMutLog_t *dbgMutLogFindFromBack(pthread_mutex_t *pmut, dbgMutLog_t *pLast)
{
	dbgMutLog_t *pLog;
	
	if(pLast == NULL)
		pLog = dbgMutLogListLast;
	else
		pLog = pLast;

	while(pLog != NULL) {
		if(pLog->mut == pmut)
			break;
		pLog = pLog->pPrev;
	}

	return pLog;
}


/* find lock aquire for mutex from back of list */
dbgMutLog_t *dbgMutLogFindHolder(pthread_mutex_t *pmut)
{
	dbgMutLog_t *pLog;

	pLog = dbgMutLogFindFromBack(pmut, NULL);
	while(pLog != NULL) {
		if(pLog->mutexOp == MUTOP_LOCK)
			break;
		pLog = dbgMutLogFindFromBack(pmut, pLog);
	}

	return pLog;
}


/* report wait on a mutex and add it to the mutex log */
static void dbgMutexPreLockLog(pthread_mutex_t *pmut, const char *file, const char* func, int line)
{
	dbgMutLog_t *pHolder;
	dbgMutLog_t *pLog;
	char pszBuf[128];
	char *pszHolder;

	pthread_mutex_lock(&mutMutLog);
	pHolder = dbgMutLogFindHolder(pmut);
	pLog = dbgMutLogAddEntry(pmut, MUTOP_LOCKWAIT, file, line);

	if(pHolder == NULL)
		pszHolder = "[NONE(available)]";
	else {
		snprintf(pszBuf, sizeof(pszBuf)/sizeof(char), "%s:%d", pLog->file, pLog->ln);
		pszHolder = pszBuf;
	}

	dbgprintf("mutex %p pre lock %s, %s(), line %d, holder: %s\n", (void*)pmut, file, func, line, pszHolder);
	pthread_mutex_unlock(&mutMutLog);
}


/* report aquired mutex */
static void dbgMutexLockLog(pthread_mutex_t *pmut, const char *file, const char* func, int line)
{
	dbgMutLog_t *pLog;

	pthread_mutex_lock(&mutMutLog);
dbgprintf("mutexlocklog 1, serach for op %d\n", MUTOP_LOCKWAIT);
	pLog = dbgMutLogFindSpecific(pmut, MUTOP_LOCKWAIT, file, line);
dbgprintf("mutexlocklog 2, found %p\n", pLog);
	assert(pLog != NULL);
	dbgMutLogDelEntry(pLog);
dbgprintf("mutexlocklog 3\n");
	pLog = dbgMutLogAddEntry(pmut, MUTOP_LOCK, file, line);
	pthread_mutex_unlock(&mutMutLog);
	dbgprintf("mutex %p aquired %s, %s(), line %d\n", (void*)pmut, file, func, line);
}

/* if we unlock, we just remove the lock aquired entry from the log list */
static void dbgMutexUnlockLog(pthread_mutex_t *pmut, const char *file, const char* func, int line)
{
	dbgMutLog_t *pLog;

	pthread_mutex_lock(&mutMutLog);
	pLog = dbgMutLogFindSpecific(pmut, MUTOP_LOCK, file, line);
	assert(pLog != NULL);
	dbgMutLogDelEntry(pLog);
	pthread_mutex_unlock(&mutMutLog);
	dbgprintf("mutex %p UNlock %s, %s(), line %d\n", (void*)pmut, file, func, line);
}


int dbgMutexLock(pthread_mutex_t *pmut, const char *file, const char* func, int line)
{
dbgprintf("---> dbgMutexlock\n");
	int ret;
	dbgMutexPreLockLog(pmut, file, func, line);
	ret = pthread_mutex_lock(pmut);

dbgprintf("--->   lock ret %d: %s\n", ret, strerror(errno));
	dbgMutexLockLog(pmut, file, func, line);
	return ret;
}


int dbgMutexUnlock(pthread_mutex_t *pmut, const char *file, const char* func, int line)
{
dbgprintf("---> dbgMutexUnlock\n");
	int ret;
	dbgMutexUnlockLog(pmut, file, func, line);
	ret = pthread_mutex_unlock(pmut);
	if(1)dbgprintf("mutex %p UNlock done %s, %s(), line %d\n",(void*)pmut, file, func, line);
	return ret;
}


int dbgCondWait(pthread_cond_t *cond, pthread_mutex_t *pmut, const char *file, const char* func, int line)
{
	int ret;
dbgprintf("---> dbCondWait\n");
	dbgMutexUnlockLog(pmut, file, func, line);
	dbgMutexPreLockLog(pmut, file, func, line);
	ret = pthread_cond_wait(cond, pmut);
	dbgMutexLockLog(pmut, file, func, line);
	return ret;
}


int dbgCondTimedWait(pthread_cond_t *cond, pthread_mutex_t *pmut, const struct timespec *abstime,
		     const char *file, const char* func, int line)
{
	int ret;
dbgprintf("---> dbCondTimedWait\n");
	dbgMutexUnlockLog(pmut, file, func, line);
	dbgMutexPreLockLog(pmut, file, func, line);
	ret = pthread_cond_timedwait(cond, pmut, abstime);
	dbgMutexLockLog(pmut, file, func, line);
	return ret;
}


/* ------------------------- end mutex tracking code ------------------------- */ 

/* ------------------------- call stack tracking code ------------------------- */ 

/* destructor for a call stack object */
static void dbgCallStackDestruct(void *arg)
{
	dbgCallStack_t *pStack = (dbgCallStack_t*) arg;

	dbgprintf("destructor for debug call stack %p called\n", pStack);
	DLL_Del(CallStack, pStack);
}


/* print a thread's call stack
 */
static void dbgCallStackPrint(dbgCallStack_t *pStack)
{
	int i;

	pthread_mutex_lock(&mutCallStack);
	dbgprintf("\nRecorded Call Order for Thread 0x%lx (%p):\n", (unsigned long) pStack->thrd, pStack);
	for(i = 0 ; i < pStack->stackPtr ; i++) {
		dbgprintf("%s()\n", pStack->callStack[i]);
	}
	dbgprintf("NOTE: not all calls may have been recorded.\n");
	pthread_mutex_unlock(&mutCallStack);
}

/* print all threads call stacks
 */
static void dbgCallStackPrintAll(void)
{
	dbgCallStack_t *pStack;
	/* stack info */
	for(pStack = dbgCallStackListRoot ; pStack != NULL ; pStack = pStack->pNext) {
		dbgCallStackPrint(pStack);
	}
}


/* get ptr to call stack - if none exists, create a new stack
 */
static dbgCallStack_t *dbgGetCallStack(void)
{
	dbgCallStack_t *pStack;

	pthread_mutex_lock(&mutCallStack);
	if((pStack = pthread_getspecific(keyCallStack)) == NULL) {
		/* construct object */
		pStack = calloc(1, sizeof(dbgCallStack_t));
		pStack->thrd = pthread_self();
		(void) pthread_setspecific(keyCallStack, pStack);
fprintf(stdout, "dbgGetCallStack Create thrd %lx, pstack %p, thrd %lx\n", (unsigned long) pthread_self(), pStack, pStack->thrd);
		DLL_Add(CallStack, pStack);
	}
	pthread_mutex_unlock(&mutCallStack);
	return pStack;
}



/* handler for SIGSEGV - MUST terminiate the app, but does so in a somewhat 
 * more meaningful way.
 * rgerhards, 2008-01-22
 */
void
sigsegvHdlr(int signum)
{
	struct sigaction sigAct;
	char *signame;

	if(signum == SIGSEGV) {
		signame = " (SIGSEGV)";
	} else {
		signame = "";
	}

	dbgprintf("Signal %d%s occured, execution must be terminated %d.\n", signum, signame, SIGSEGV);

	dbgCallStackPrintAll();

	fflush(stddbg);
	/* re-instantiate original handler ... */
	memset(&sigAct, 0, sizeof (sigAct));
	sigemptyset(&sigAct.sa_mask);
	sigAct.sa_handler = SIG_DFL;
	sigaction(SIGSEGV, &sigAct, NULL);

	/* and call it */
	int ir = raise(signum);
	printf("raise returns %d, errno %d: %s\n", ir, errno, strerror(errno));

	/* we should never arrive here - but we provide some code just in case... */
	dbgprintf("sigsegvHdlr: oops, returned from raise(), doing exit(), something really wrong...\n");
	exit(1);
}


/* print some debug output */
void
dbgprintf(char *fmt, ...)
{
	static pthread_t ptLastThrdID = 0;
	static int bWasNL = 0;
	va_list ap;

	if(!(Debug && debugging_on))
		return;
	
	pthread_mutex_lock(&mutdbgprintf);
	pthread_cleanup_push(dbgMutexCancelCleanupHdlr, &mutdbgprintf);

	/* The bWasNL handler does not really work. It works if no thread
	 * switching occurs during non-NL messages. Else, things are messed
	 * up. Anyhow, it works well enough to provide useful help during
	 * getting this up and running. It is questionable if the extra effort
	 * is worth fixing it, giving the limited appliability.
	 * rgerhards, 2005-10-25
	 * I have decided that it is not worth fixing it - especially as it works
	 * pretty well.
	 * rgerhards, 2007-06-15
	 */
	if(ptLastThrdID != pthread_self()) {
		if(!bWasNL) {
			fprintf(stddbg, "\n");
			bWasNL = 1;
		}
		ptLastThrdID = pthread_self();
	}

	if(bWasNL) {
		fprintf(stddbg, "%8.8x: ", (unsigned int) pthread_self());
	}
	bWasNL = (*(fmt + strlen(fmt) - 1) == '\n') ? 1 : 0;
	va_start(ap, fmt);
	vfprintf(stddbg, fmt, ap);
	va_end(ap);

	fflush(stddbg);
	pthread_cleanup_pop(1);
}


/* handler called when a function is entered
 */
void dbgEntrFunc(char* file, int line, const char* func)
{
	dbgCallStack_t *pStack = dbgGetCallStack();

	if(bLogFuncFlow)
		dbgprintf("%s:%d: %s: enter\n", file, line, func);
	pStack->callStack[pStack->stackPtr++] = func;
	assert(pStack->stackPtr < (int) (sizeof(pStack->callStack) / sizeof(const char*)));
	
}


/* handler called when a function is exited
 */
void dbgExitFunc(char* file, int line, const char* func)
{
	dbgCallStack_t *pStack = dbgGetCallStack();

	if(bLogFuncFlow)
		dbgprintf("%s:%d: %s: exit\n", file, line, func);
	pStack->stackPtr--;
	assert(pStack->stackPtr > 0);
}


/* Handler for SIGUSR2. Dumps all available debug output
 */
static void sigusr2Hdlr(int __attribute__((unused)) signum)
{
	dbgprintf("SIGUSR2 received, dumping debug information\n");
	dbgCallStackPrintAll();
}


rsRetVal dbgClassInit(void)
{
	struct sigaction sigAct;
	
	memset(&sigAct, 0, sizeof (sigAct));
	sigemptyset(&sigAct.sa_mask);
	sigAct.sa_handler = sigusr2Hdlr;
	//sigaction(SIGUSR2, &sigAct, NULL);
	sigaction(SIGUSR1, &sigAct, NULL);

	stddbg = stdout;
	(void) pthread_key_create(&keyCallStack, dbgCallStackDestruct);
	return RS_RET_OK;
}


rsRetVal dbgClassExit(void)
{
	pthread_key_delete(keyCallStack);
	return RS_RET_OK;
}
/*
 * vi:set ai:
 */
