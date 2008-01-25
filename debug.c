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

/* forward definitions */
static void dbgGetThrdName(char *pszBuf, size_t lenBuf, pthread_t thrd, int bIncludeNumID);

/* static data (some time to be replaced) */
int	Debug;		/* debug flag  - read-only after startup */
int debugging_on = 0;	 /* read-only, except on sig USR1 */
static int bLogFuncFlow = 0; /* shall the function entry and exit be logged to the debug log? */
static int bPrintFuncDBOnExit = 0; /* shall the function entry and exit be logged to the debug log? */
static char *pszAltDbgFileName = "/home/rger/proj/rsyslog/log"; /* if set, debug output is *also* sent to here */
static FILE *altdbg;	/* and the handle for alternate debug output */
static FILE *stddbg;
static dbgFuncDB_t pCurrFunc;


/* list of all known FuncDBs. We use a special list, because it must only be single-linked. As
 * functions never disappear, we only need to add elements when we see a new one and never need
 * to remove anything. For this, we simply add at the top, which saves us a Last pointer. The goal
 * is to use as few memory as possible.
 */
typedef struct dbgFuncDBListEntry_s {
	dbgFuncDB_t *pFuncDB;
	struct dbgFuncDBListEntry_s *pNext;
} dbgFuncDBListEntry_t;
dbgFuncDBListEntry_t *pFuncDBListRoot;

static pthread_mutex_t mutFuncDBList = PTHREAD_MUTEX_INITIALIZER;

typedef struct dbgMutLog_s {
	struct dbgMutLog_s *pNext;
	struct dbgMutLog_s *pPrev;
	pthread_mutex_t *mut;
	pthread_t thrd;
	dbgFuncDB_t *pFuncDB;
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
	dbgFuncDB_t *callStack[500];
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

/* ------------------------- FuncDB utility functions ------------------------- */ 

#define SIZE_FUNCDB_MUTEX_TABLE(pFuncDB) ((int) (sizeof(pFuncDB->mutInfo) / sizeof(dbgFuncDBmutInfoEntry_t)))

/* print a FuncDB
 */
static void dbgFuncDBPrint(dbgFuncDB_t *pFuncDB)
{
	assert(pFuncDB != NULL);
	assert(pFuncDB->magic == dbgFUNCDB_MAGIC);
	/* make output suitable for sorting on invocation count */
	dbgprintf("%10.10ld times called: %s:%d:%s\n", pFuncDB->nTimesCalled, pFuncDB->file, pFuncDB->line, pFuncDB->func);
}


/* print all funcdb entries
 */
static void dbgFuncDBPrintAll(void)
{
	dbgFuncDBListEntry_t *pFuncDBList;
	int nFuncs = 0;

	for(pFuncDBList = pFuncDBListRoot ; pFuncDBList != NULL ; pFuncDBList = pFuncDBList->pNext) {
		dbgFuncDBPrint(pFuncDBList->pFuncDB);
		nFuncs++;
	}

	dbgprintf("%d unique functions called\n", nFuncs);
}


/* find a mutex inside the FuncDB mutex table. Returns NULL if not found. Only mutexes from the same thread
 * are found.
 */
static inline dbgFuncDBmutInfoEntry_t *dbgFuncDBGetMutexInfo(dbgFuncDB_t *pFuncDB, pthread_mutex_t *pmut)
{
	int i;
	int iFound = -1;
	pthread_t ourThrd = pthread_self();

	for(i = 0 ; i < SIZE_FUNCDB_MUTEX_TABLE(pFuncDB) ; ++i) {
		if(pFuncDB->mutInfo[i].pmut == pmut && pFuncDB->mutInfo[i].lockLn != -1 && pFuncDB->mutInfo[i].thrd == ourThrd) {
			iFound = i;
			break;
		}
	}
	
	return (iFound == -1) ?  NULL : &pFuncDB->mutInfo[i];
}


/* print any mutex that can be found in the FuncDB. Custom header is provided.
 * "thrd" is the thread that is searched. If it is 0, mutexes for all threads
 * shall be printed.
 */
static inline void
dbgFuncDBPrintActiveMutexes(dbgFuncDB_t *pFuncDB, char *pszHdrText, pthread_t thrd)
{
	int i;
	char pszThrdName[64];

	for(i = 0 ; i < SIZE_FUNCDB_MUTEX_TABLE(pFuncDB) ; ++i) {
		if(pFuncDB->mutInfo[i].lockLn != -1 && (thrd == 0 || thrd == pFuncDB->mutInfo[i].thrd)) {
			dbgGetThrdName(pszThrdName, sizeof(pszThrdName), pFuncDB->mutInfo[i].thrd, 1);
			dbgprintf("%s:%d:%s:invocation %ld: %s %p[%d/%s]\n", pFuncDB->file, pFuncDB->line, pFuncDB->func,
				  pFuncDB->mutInfo[i].lInvocation, pszHdrText, (void*)pFuncDB->mutInfo[i].pmut, i,
				  pszThrdName);
		}
	}
}

/* find a free mutex info spot in FuncDB. NULL is returned if table is full.
 */
static inline dbgFuncDBmutInfoEntry_t *dbgFuncDBFindFreeMutexInfo(dbgFuncDB_t *pFuncDB)
{
	int i;
	int iFound = -1;

	for(i = 0 ; i < SIZE_FUNCDB_MUTEX_TABLE(pFuncDB) ; ++i) {
		if(pFuncDB->mutInfo[i].lockLn == -1) {
			iFound = i;
			break;
		}
	}

	if(iFound == -1) {
		dbgprintf("%s:%d:%s: INFO: out of space in FuncDB for mutex info (max %d entries) - ignoring\n",
			  pFuncDB->file, pFuncDB->line, pFuncDB->func, SIZE_FUNCDB_MUTEX_TABLE(pFuncDB));
	}
	
	return (iFound == -1) ?  NULL : &pFuncDB->mutInfo[i];
}

/* add a mutex lock to the FuncDB. If the size is exhausted, info is discarded.
 */
static inline void dbgFuncDBAddMutexLock(dbgFuncDB_t *pFuncDB, pthread_mutex_t *pmut, int lockLn)
{
	dbgFuncDBmutInfoEntry_t *pMutInfo;

	if((pMutInfo = dbgFuncDBFindFreeMutexInfo(pFuncDB)) != NULL) {
		pMutInfo->pmut = pmut;
		pMutInfo->lockLn = lockLn;
		pMutInfo->lInvocation = pFuncDB->nTimesCalled;
		pMutInfo->thrd = pthread_self();
	}
}

/* remove a locked mutex from the FuncDB (unlock case!).
 */
static inline void dbgFuncDBRemoveMutexLock(dbgFuncDB_t *pFuncDB, pthread_mutex_t *pmut)
{
	dbgFuncDBmutInfoEntry_t *pMutInfo;

	if((pMutInfo = dbgFuncDBGetMutexInfo(pFuncDB, pmut)) != NULL) {
		pMutInfo->lockLn = -1;
	}
//dbgprintf("dbgFuncDB mutex to be removed %p, ptr found %p, lockLn %d\n", pmut, pMutInfo, (pMutInfo == 0) ? 123456789 : pMutInfo->lockLn);
}


/* ------------------------- END FuncDB utility functions ------------------------- */ 

/* ###########################################################################
 * 				IMPORTANT NOTE
 * Mutex instrumentation reduces the code's concurrency and thus affects its
 * order of execution. It is vital to test the code also with mutex 
 * instrumentation turned off! Some bugs may not show up while it on...
 * ###########################################################################
 */

/* constructor & add new entry to list
 */
dbgMutLog_t *dbgMutLogAddEntry(pthread_mutex_t *pmut, short mutexOp, dbgFuncDB_t *pFuncDB)
{
	dbgMutLog_t *pLog;

	pLog = calloc(1, sizeof(dbgMutLog_t));
	assert(pLog != NULL);

	/* fill data members */
	pLog->mut = pmut;
	pLog->thrd = pthread_self();
	pLog->mutexOp = mutexOp;
	pLog->pFuncDB = pFuncDB;

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
	char pszThrdName[64];

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

	dbgGetThrdName(pszThrdName, sizeof(pszThrdName), pLog->thrd, 1);
	dbgprintf("mutex 0x%lx is being %s by code at %s:%d, thread %s\n", (unsigned long) pLog->mut,
		  strmutop, pLog->pFuncDB->file, pLog->pFuncDB->line, pszThrdName);
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
 * The pFuncDB is optional and may be NULL to indicate no specific funciont is
 * reqested (aka "it is ignored" ;)). This is important for the unlock case.
 */
dbgMutLog_t *dbgMutLogFindSpecific(pthread_mutex_t *pmut, short mutop, dbgFuncDB_t *pFuncDB)
{
	dbgMutLog_t *pLog;
	pthread_t mythrd = pthread_self();
	
	pLog = dbgMutLogListLast;
	while(pLog != NULL) {
		if(   pLog->mut == pmut && pLog->thrd == mythrd && pLog->mutexOp == mutop
		   && (pFuncDB == NULL || pLog->pFuncDB == pFuncDB))
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
		pLog = pLast->pPrev; /* if we get the last processed one, we need to go one before it, else its an endless loop */

	while(pLog != NULL) {
		if(pLog->mut == pmut) {
			break;
		}
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
static inline void dbgMutexPreLockLog(pthread_mutex_t *pmut, dbgFuncDB_t *pFuncDB, int ln)
{
	dbgMutLog_t *pHolder;
	dbgMutLog_t *pLog;
	char pszBuf[128];
	char pszHolderThrdName[64];
	char *pszHolder;

	pthread_mutex_lock(&mutMutLog);
	pHolder = dbgMutLogFindHolder(pmut);
	pLog = dbgMutLogAddEntry(pmut, MUTOP_LOCKWAIT, pFuncDB);

	if(pHolder == NULL)
		pszHolder = "[NONE]";
	else {
		dbgGetThrdName(pszHolderThrdName, sizeof(pszHolderThrdName), pHolder->thrd, 1);
		snprintf(pszBuf, sizeof(pszBuf)/sizeof(char), "%s:%d [%s]", pHolder->pFuncDB->file, pHolder->pFuncDB->line, pszHolderThrdName);
		pszHolder = pszBuf;
	}

	dbgprintf("%s:%d:%s: mutex %p waiting on lock, held by %s\n", pFuncDB->file, ln, pFuncDB->func, (void*)pmut, pszHolder);
	pthread_mutex_unlock(&mutMutLog);
}


/* report aquired mutex */
static inline void dbgMutexLockLog(pthread_mutex_t *pmut, dbgFuncDB_t *pFuncDB, int lockLn)
{
	dbgMutLog_t *pLog;

	pthread_mutex_lock(&mutMutLog);

	/* find and delete "waiting" entry */
	pLog = dbgMutLogFindSpecific(pmut, MUTOP_LOCKWAIT, pFuncDB);
	assert(pLog != NULL);
	dbgMutLogDelEntry(pLog);

	/* add "lock" entry */
	pLog = dbgMutLogAddEntry(pmut, MUTOP_LOCK, pFuncDB);
	dbgFuncDBAddMutexLock(pFuncDB, pmut, lockLn);
	pthread_mutex_unlock(&mutMutLog);
	dbgprintf("%s:%d:%s: mutex %p aquired\n", pFuncDB->file, lockLn, pFuncDB->func, (void*)pmut); 
}

/* if we unlock, we just remove the lock aquired entry from the log list */
static inline void dbgMutexUnlockLog(pthread_mutex_t *pmut, dbgFuncDB_t *pFuncDB, int unlockLn)
{
	dbgMutLog_t *pLog;

	pthread_mutex_lock(&mutMutLog);
	pLog = dbgMutLogFindSpecific(pmut, MUTOP_LOCK, NULL);
	assert(pLog != NULL);

	/* we found the last lock entry. We now need to see from which FuncDB we need to
	 * remove it. This is recorded inside the mutex log entry.
	 */
	dbgFuncDBRemoveMutexLock(pLog->pFuncDB, pmut);

	/* donw with the log entry, get rid of it... */
	dbgMutLogDelEntry(pLog);

	pthread_mutex_unlock(&mutMutLog);
	dbgprintf("%s:%d:%s: mutex %p UNlocked\n", pFuncDB->file, unlockLn, pFuncDB->func, (void*)pmut);
}


/* wrapper for pthread_mutex_lock() */
int dbgMutexLock(pthread_mutex_t *pmut, dbgFuncDB_t *pFuncDB, int ln)
{
	int ret;

	dbgMutexPreLockLog(pmut, pFuncDB, ln);
	ret = pthread_mutex_lock(pmut);
	if(ret == 0) {
		dbgMutexLockLog(pmut, pFuncDB, ln);
	} else {
		dbgprintf("%s:%d:%s: ERROR: pthread_mutex_lock() for mutex %p failed with error %d (%s)\n",
			  pFuncDB->file, ln, pFuncDB->func, (void*)pmut, ret, strerror(errno)); // TODO: remove strerror!"!!
	}

	return ret;
}


/* wrapper for pthread_mutex_unlock() */
int dbgMutexUnlock(pthread_mutex_t *pmut, dbgFuncDB_t *pFuncDB, int ln)
{
	int ret;
	dbgMutexUnlockLog(pmut, pFuncDB, ln);
	ret = pthread_mutex_unlock(pmut);
	return ret;
}


/* wrapper for pthread_cond_wait() */
int dbgCondWait(pthread_cond_t *cond, pthread_mutex_t *pmut, dbgFuncDB_t *pFuncDB, int ln)
{
	int ret;
	dbgMutexUnlockLog(pmut, pFuncDB, ln);
	dbgprintf("%s:%d:%s: mutex %p waiting on condition %p\n", pFuncDB->file, pFuncDB->line, pFuncDB->func, (void*)pmut, (void*)cond);
	dbgMutexPreLockLog(pmut, pFuncDB, ln);
	ret = pthread_cond_wait(cond, pmut);
	return ret;
}


/* wrapper for pthread_cond_timedwait() */
int dbgCondTimedWait(pthread_cond_t *cond, pthread_mutex_t *pmut, const struct timespec *abstime, dbgFuncDB_t *pFuncDB, int ln)
{
	int ret;
	dbgMutexUnlockLog(pmut, pFuncDB, ln);
	dbgMutexPreLockLog(pmut, pFuncDB, ln);
	dbgprintf("%s:%d:%s: mutex %p waiting on condition %p (with timeout)\n", pFuncDB->file, pFuncDB->line, pFuncDB->func,
		  (void*)pmut, (void*)cond);
	ret = pthread_cond_timedwait(cond, pmut, abstime);
	dbgMutexLockLog(pmut, pFuncDB, ln);
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
		dbgprintf("%d: %s:%s\n", i, pThrd->callStack[i]->func, pThrd->callStack[i]->file);
	}
	dbgprintf("maximum number of nested calls for this thread: %d.\n", pThrd->stackPtrMax);
	dbgprintf("NOTE: not all calls may have been recorded, code does not currently guarantee that!\n");
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

	dbgprintf("\n\n\n\nSignal %d%s occured, execution must be terminated %d.\n\n\n\n", signum, signame, SIGSEGV);

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
	static char pszWriteBuf[1024];
	size_t lenWriteBuf;

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
			if(altdbg != NULL) fprintf(altdbg, "\n");
			bWasNL = 1;
		}
		ptLastThrdID = pthread_self();
	}

	/* do not cache the thread name, as the caller might have changed it
	 * TODO: optimized, invalidate cache when new name is set
	 */
	dbgGetThrdName(pszThrdName, sizeof(pszThrdName), ptLastThrdID, 0);

	if(bWasNL) {
		fprintf(stddbg, "%s: ", pszThrdName);
		if(altdbg != NULL) fprintf(altdbg, "%s: ", pszThrdName);
	}
	bWasNL = (*(fmt + strlen(fmt) - 1) == '\n') ? 1 : 0;
	va_start(ap, fmt);
	lenWriteBuf = vsnprintf(pszWriteBuf, sizeof(pszWriteBuf), fmt, ap);
	if(lenWriteBuf >= sizeof(pszWriteBuf)) {
		/* if our buffer was too small, we simply truncate. TODO: maybe something better? */
		lenWriteBuf--;
	}
	va_end(ap);
	/*
	fprintf(stddbg, "%s", pszWriteBuf);
	if(altdbg != NULL) fprintf(altdbg, "%s", pszWriteBuf);
	*/
	fwrite(pszWriteBuf, lenWriteBuf, 1, stddbg);
	fwrite(pszWriteBuf, lenWriteBuf, 1, altdbg);

	fflush(stddbg);
	fflush(altdbg);
	pthread_cleanup_pop(1);
}

void tester(void)
{
BEGINfunc
ENDfunc
}

/* handler called when a function is entered
 */
int dbgEntrFunc(dbgFuncDB_t *pFuncDB)
{
	int iStackPtr;
	dbgThrdInfo_t *pThrd = dbgGetThrdInfo();
	dbgFuncDBListEntry_t *pFuncDBListEntry;
	unsigned int i;

	assert(pFuncDB != NULL);
	assert(pFuncDB->magic == dbgFUNCDB_MAGIC);

	if(pFuncDB->nTimesCalled++ == 0) {
		/* dbgprintf("%s:%d:%s: called first time, initializing FuncDB\n", pFuncDB->file, pFuncDB->line, pFuncDB->func); */
		/* this is the first time we see this function, so let's fully initialize the FuncDB. Note
		 * that partial initialization already occured via the initializer, as far as this could be
		 * done (all static values have been set).
		 */
		for(i = 0 ; i < sizeof(pFuncDB->mutInfo)/sizeof(dbgFuncDBmutInfoEntry_t) ; ++i) {
			pFuncDB->mutInfo[i].lockLn = -1; /* set to not Locked */
		}
		/* finally add element to list of FuncDBs */
		pthread_mutex_lock(&mutFuncDBList);
		if((pFuncDBListEntry = calloc(1, sizeof(dbgFuncDBListEntry_t))) == NULL) {
			dbgprintf("Error %d allocating memory for FuncDB List entry, not adding\n", errno);
			pFuncDB->nTimesCalled = 0; /* retry the next time (OK, quick, but acceptable...) */
		} else {
			pFuncDBListEntry->pFuncDB = pFuncDB;
			pFuncDBListEntry->pNext = pFuncDBListRoot;
			pFuncDBListRoot = pFuncDBListEntry;
		}
		pthread_mutex_unlock(&mutFuncDBList);
	}

	/* when we reach this point, we have a fully-initialized FuncDB! */

	if(bLogFuncFlow)
		dbgprintf("%s:%d: %s: enter\n", pFuncDB->file, pFuncDB->line, pFuncDB->func);
	if(pThrd->stackPtr >= (int) (sizeof(pThrd->callStack) / sizeof(dbgFuncDB_t*))) {
		dbgprintf("%s:%d: %s: debug module: call stack for this thread full, suspending call tracking\n",
			  pFuncDB->file, pFuncDB->line, pFuncDB->func);
		iStackPtr = pThrd->stackPtr;
	} else {
		iStackPtr = pThrd->stackPtr++;
		if(pThrd->stackPtr > pThrd->stackPtrMax)
			pThrd->stackPtrMax = pThrd->stackPtr;
		pThrd->callStack[iStackPtr] = pFuncDB;
	}
	
	return iStackPtr;
}


/* handler called when a function is exited
 */
void dbgExitFunc(dbgFuncDB_t *pFuncDB, int iStackPtrRestore)
{
	dbgThrdInfo_t *pThrd = dbgGetThrdInfo();

	assert(iStackPtrRestore >= 0);
	assert(pFuncDB != NULL);
	assert(pFuncDB->magic == dbgFUNCDB_MAGIC);

	dbgFuncDBPrintActiveMutexes(pFuncDB, "WARNING: mutex still owned by us as we exit function, mutex: ", pthread_self());
	if(bLogFuncFlow)
		dbgprintf("%s:%d: %s: exit\n", pFuncDB->file, pFuncDB->line, pFuncDB->func);
	pThrd->stackPtr = iStackPtrRestore;
	if(pThrd->stackPtr < 0) {
		dbgprintf("Stack pointer for thread %lx below 0 - resetting (some RETiRet still wrong!)\n", pthread_self());
		pThrd->stackPtr = 0;
	}
}


void dbgPrintAllDebugInfo(void)
{
	dbgCallStackPrintAll();
	dbgMutLogPrintAll();
	if(bPrintFuncDBOnExit)
		dbgFuncDBPrintAll();
}


/* Handler for SIGUSR2. Dumps all available debug output
 */
static void sigusr2Hdlr(int __attribute__((unused)) signum)
{
	dbgprintf("SIGUSR2 received, dumping debug information\n");
	dbgPrintAllDebugInfo();
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
	if(pszAltDbgFileName != NULL) {
		/* we have a secondary file, so let's open it) */
		if((altdbg = fopen(pszAltDbgFileName, "w")) == NULL) {
			fprintf(stderr, "alternate debug file could not be opened, ignoring. Error: %s\n", strerror(errno));
		}
	}

	dbgSetThrdName((uchar*)"main thread");
	return RS_RET_OK;
}


rsRetVal dbgClassExit(void)
{
	pthread_key_delete(keyCallStack);
	if(altdbg != NULL)
		fclose(altdbg);
	return RS_RET_OK;
}
/*
 * vi:set ai:
 */
