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


/* the structure below was originally just the thread's call stack, but it has
 * a bit evolved over time. So we have now ended up with the fact that it 
 * all debug info we know about the thread.
 */
typedef struct dbgCallStack_s {
	pthread_t thrd;
	const char* callStack[500];
	const char* callStackFile[500];
	int stackPtr;
	int stackPtrMax;
	char *pszThrdName;
	struct dbgCallStack_s *pNext;
	struct dbgCallStack_s *pPrev;
} dbgThrdInfo_t;
static dbgThrdInfo_t *dbgCallStackListRoot = NULL;
static dbgThrdInfo_t *dbgCallStackListLast = NULL;
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
	free(pThis);

#define DLL_Add(type, pThis) \
		if(dbg##type##ListRoot == NULL) { \
			dbg##type##ListRoot = pThis; \
			dbg##type##ListLast = pThis; \
		} else { \
			pThis->pPrev = dbg##type##ListLast; \
			dbg##type##ListLast->pNext = pThis; \
			dbg##type##ListLast = pThis; \
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

	/* fill data members */
	pLog->mut = pmut;
	pLog->thrd = pthread_self();
	pLog->mutexOp = mutexOp;
	pLog->file = file;
	pLog->ln = line;

	DLL_Add(MutLog, pLog);

	return pLog;
}


/* destruct log entry
 */
void dbgMutLogDelEntry(dbgMutLog_t *pLog)
{
	assert(pLog != NULL);
	DLL_Del(MutLog, pLog);
}


/* print a single mutex log entry */
static void dbgMutLogPrintOne(dbgMutLog_t *pLog)
{
	char *strmutop;
	char buf[64];

	assert(pLog != NULL);
	switch(pLog->mutexOp) {
		case MUTOP_LOCKWAIT:
			strmutop = "waited on";
			break;
		case MUTOP_LOCK:
			strmutop = "owned";
			break;
		default:
			snprintf(buf, sizeof(buf)/sizeof(char), "unknown state %d - should not happen!", pLog->mutexOp);
			strmutop = buf;
			break;
	}
}

/* print the complete mutex log */
static void dbgMutLogPrintAll(void)
{
	dbgMutLog_t *pLog;

	dbgprintf("Mutex log for all known mutex operations:\n");
	for(pLog = dbgMutLogListRoot ; pLog != NULL ; pLog = pLog->pNext)
		dbgMutLogPrintOne(pLog);
	
}


/* find the last log entry for that specific mutex object. Is used to delete
 * a thread's own requests. Searches occur from the back.
 * file and line are optional. If they are NULL and -1, they are ignored. This is
 * important for the unlock case.
 */
dbgMutLog_t *dbgMutLogFindSpecific(pthread_mutex_t *pmut, short mutop, const char *file, int line)
{
	dbgMutLog_t *pLog;
	pthread_t mythrd = pthread_self();
	
	pLog = dbgMutLogListLast;
	while(pLog != NULL) {

		if(   pLog->mut == pmut && pLog->thrd == mythrd && pLog->mutexOp == mutop
		   && ((file == NULL) || !strcmp(pLog->file, file))  && ((line == -1) || (pLog->ln == line)))
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

	dbgprintf("%s:%d:%s: mutex %p waiting on lock, held by %s\n", file, line, func, (void*)pmut, pszHolder);
	pthread_mutex_unlock(&mutMutLog);
}


/* report aquired mutex */
static void dbgMutexLockLog(pthread_mutex_t *pmut, const char *file, const char* func, int line)
{
	dbgMutLog_t *pLog;

	pthread_mutex_lock(&mutMutLog);
	pLog = dbgMutLogFindSpecific(pmut, MUTOP_LOCKWAIT, file, line);
	assert(pLog != NULL);
	dbgMutLogDelEntry(pLog);
	pLog = dbgMutLogAddEntry(pmut, MUTOP_LOCK, file, line);
	pthread_mutex_unlock(&mutMutLog);
	dbgprintf("%s:%d:%s: mutex %p aquired\n", file, line, func, (void*)pmut); 
}

/* if we unlock, we just remove the lock aquired entry from the log list */
static void dbgMutexUnlockLog(pthread_mutex_t *pmut, const char *file, const char* func, int line)
{
	dbgMutLog_t *pLog;

	pthread_mutex_lock(&mutMutLog);
	pLog = dbgMutLogFindSpecific(pmut, MUTOP_LOCK, NULL, -1);
	assert(pLog != NULL);
	dbgMutLogDelEntry(pLog);
	pthread_mutex_unlock(&mutMutLog);
	dbgprintf("%s:%d:%s: mutex %p UNlocked\n", file, line, func, (void*)pmut);
}


int dbgMutexLock(pthread_mutex_t *pmut, const char *file, const char* func, int line)
{
	int ret;
	dbgMutexPreLockLog(pmut, file, func, line);
	ret = pthread_mutex_lock(pmut);

	dbgMutexLockLog(pmut, file, func, line);
	return ret;
}


int dbgMutexUnlock(pthread_mutex_t *pmut, const char *file, const char* func, int line)
{
	int ret;
	dbgMutexUnlockLog(pmut, file, func, line);
	ret = pthread_mutex_unlock(pmut);
	if(1)dbgprintf("mutex %p UNlock done %s, %s(), line %d\n",(void*)pmut, file, func, line);
	return ret;
}


int dbgCondWait(pthread_cond_t *cond, pthread_mutex_t *pmut, const char *file, const char* func, int line)
{
	int ret;
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
	dbgMutexUnlockLog(pmut, file, func, line);
	dbgMutexPreLockLog(pmut, file, func, line);
	ret = pthread_cond_timedwait(cond, pmut, abstime);
	dbgMutexLockLog(pmut, file, func, line);
	return ret;
}


/* ------------------------- end mutex tracking code ------------------------- */ 

/* ------------------------- thread tracking code ------------------------- */ 

/* get ptr to call stack - if none exists, create a new stack
 */
static dbgThrdInfo_t *dbgGetThrdInfo(void)
{
	dbgThrdInfo_t *pThrd;

	pthread_mutex_lock(&mutCallStack);
	if((pThrd = pthread_getspecific(keyCallStack)) == NULL) {
		/* construct object */
		pThrd = calloc(1, sizeof(dbgThrdInfo_t));
		pThrd->thrd = pthread_self();
		(void) pthread_setspecific(keyCallStack, pThrd);
fprintf(stdout, "dbgGetThrdInfo Create thrd %lx, pstack %p, thrd %lx\n", (unsigned long) pthread_self(), pThrd, pThrd->thrd);
		DLL_Add(CallStack, pThrd);
	}
	pthread_mutex_unlock(&mutCallStack);
	return pThrd;
}



/* find a specific thread ID. It must be present, else something is wrong
 */
static inline dbgThrdInfo_t *dbgFindThrd(pthread_t thrd)
{
	dbgThrdInfo_t *pThrd;

	for(pThrd = dbgCallStackListRoot ; pThrd != NULL ; pThrd = pThrd->pNext) {
		if(pThrd->thrd == thrd)
			break;
	}
	return pThrd;
}


/* build a string with the thread name. If none is set, the thread ID is
 * used instead. Caller must provide buffer space. If bIncludeNumID is set
 * to 1, the numerical ID is always included.
 * rgerhards 2008-01-23
 */
static void dbgGetThrdName(char *pszBuf, size_t lenBuf, pthread_t thrd, int bIncludeNumID)
{
	dbgThrdInfo_t *pThrd;

	assert(pszBuf != NULL);

	pThrd = dbgFindThrd(thrd);

	if(pThrd == 0 || pThrd->pszThrdName == NULL) {
		/* no thread name, use numeric value  */
		snprintf(pszBuf, lenBuf, "%lx", thrd);
	} else {
		if(bIncludeNumID) {
			snprintf(pszBuf, lenBuf, "%s (%lx)", pThrd->pszThrdName, thrd);
		} else {
			snprintf(pszBuf, lenBuf, "%s", pThrd->pszThrdName);
		}
	}

}


/* set a name for the current thread. The caller provided string is duplicated.
 */
void dbgSetThrdName(uchar *pszName)
{
	dbgThrdInfo_t *pThrd = dbgGetThrdInfo();
	if(pThrd->pszThrdName != NULL)
		free(pThrd->pszThrdName);
	pThrd->pszThrdName = strdup((char*)pszName);
}


/* destructor for a call stack object */
static void dbgCallStackDestruct(void *arg)
{
	dbgThrdInfo_t *pThrd = (dbgThrdInfo_t*) arg;

	dbgprintf("destructor for debug call stack %p called\n", pThrd);
	DLL_Del(CallStack, pThrd);
}


/* print a thread's call stack
 */
static void dbgCallStackPrint(dbgThrdInfo_t *pThrd)
{
	int i;
	char pszThrdName[64];

	pthread_mutex_lock(&mutCallStack);
	dbgGetThrdName(pszThrdName, sizeof(pszThrdName), pThrd->thrd, 1);
	dbgprintf("\n");
	dbgprintf("Recorded Call Order for Thread '%s':\n", pszThrdName);
	for(i = 0 ; i < pThrd->stackPtr ; i++) {
		dbgprintf("%d, %s:%s\n", i, pThrd->callStackFile[i], pThrd->callStack[i]);
	}
	dbgprintf("NOTE: not all calls may have been recorded, highest number of nested calls %d.\n", pThrd->stackPtrMax);
	pthread_mutex_unlock(&mutCallStack);
}

/* print all threads call stacks
 */
static void dbgCallStackPrintAll(void)
{
	dbgThrdInfo_t *pThrd;
	/* stack info */
	for(pThrd = dbgCallStackListRoot ; pThrd != NULL ; pThrd = pThrd->pNext) {
		dbgCallStackPrint(pThrd);
	}
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
	static char pszThrdName[64]; /* 64 is to be on the safe side, anything over 20 is bad... */

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
		dbgGetThrdName(pszThrdName, sizeof(pszThrdName), ptLastThrdID, 0);
	}

	if(bWasNL) {
		fprintf(stddbg, "%s: ", pszThrdName);
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
int dbgEntrFunc(char* file, int line, const char* func)
{
	int iStackPtr;
	dbgThrdInfo_t *pThrd = dbgGetThrdInfo();

	if(bLogFuncFlow)
		dbgprintf("%s:%d: %s: enter\n", file, line, func);
	if(pThrd->stackPtr >= (int) (sizeof(pThrd->callStack) / sizeof(const char*))) {
		dbgprintf("%s:%d: %s: debug module: call stack for this thread full, suspending call tracking\n", file, line, func);
		iStackPtr = pThrd->stackPtr;
	} else {
		iStackPtr = pThrd->stackPtr++;
		if(pThrd->stackPtr > pThrd->stackPtrMax)
			pThrd->stackPtrMax = pThrd->stackPtr;
		pThrd->callStackFile[iStackPtr] = file;
		pThrd->callStack[iStackPtr] = func;
	}
	
	return iStackPtr;
}


/* handler called when a function is exited
 */
void dbgExitFunc(int iStackPtrRestore, char* file, int line, const char* func)
{
	dbgThrdInfo_t *pThrd = dbgGetThrdInfo();

	assert(iStackPtrRestore >= 0);

	if(bLogFuncFlow)
		dbgprintf("%s:%d: %s: exit\n", file, line, func);
	pThrd->stackPtr = iStackPtrRestore;
	if(pThrd->stackPtr < 0) {
		dbgprintf("Stack pointer for thread %lx below 0 - resetting (some RETiRet still wrong!)\n", pthread_self());
		pThrd->stackPtr = 0;
	}
}


/* Handler for SIGUSR2. Dumps all available debug output
 */
static void sigusr2Hdlr(int __attribute__((unused)) signum)
{
	dbgprintf("SIGUSR2 received, dumping debug information\n");
	dbgCallStackPrintAll();
	dbgMutLogPrintAll();
}


rsRetVal dbgClassInit(void)
{
	struct sigaction sigAct;
	
	(void) pthread_key_create(&keyCallStack, dbgCallStackDestruct); /* MUST be the first action done! */
	memset(&sigAct, 0, sizeof (sigAct));
	sigemptyset(&sigAct.sa_mask);
	sigAct.sa_handler = sigusr2Hdlr;
	//sigaction(SIGUSR2, &sigAct, NULL);
	sigaction(SIGUSR1, &sigAct, NULL);

	stddbg = stdout;
	dbgSetThrdName((uchar*)"main thread");
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
