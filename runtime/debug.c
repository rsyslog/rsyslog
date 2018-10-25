/* debug.c
 *
 * This file proides debug and run time error analysis support. Some of the
 * settings are very performance intense and my be turned off during a release
 * build.
 *
 * File begun on 2008-01-22 by RGerhards
 *
 * Some functions are controlled by environment variables:
 *
 * RSYSLOG_DEBUGLOG	if set, a debug log file is written to that location
 * RSYSLOG_DEBUG	specific debug options
 *
 * For details, visit doc/debug.html
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
#include "config.h" /* autotools! */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef HAVE_SYS_SYSCALL_H
#  include <sys/syscall.h>
#endif
#if _POSIX_TIMERS <= 0
#include <sys/time.h>
#endif

#include "rsyslog.h"
#include "debug.h"
#include "atomic.h"
#include "cfsysline.h"
#include "obj.h"


/* static data (some time to be replaced) */
DEFobjCurrIf(obj)
int Debug = DEBUG_OFF;		/* debug flag  - read-only after startup */
int debugging_on = 0;	 /* read-only, except on sig USR1 */
int dbgTimeoutToStderr = 0;
static int bLogFuncFlow = 0; /* shall the function entry and exit be logged to the debug log? */
static int bLogAllocFree = 0; /* shall calls to (m/c)alloc and free be logged to the debug log? */
static int bPrintFuncDBOnExit = 0; /* shall the function entry and exit be logged to the debug log? */
static int bPrintMutexAction = 0; /* shall mutex calls be printed to the debug log? */
static int bPrintTime = 1;	/* print a timestamp together with debug message */
static int bPrintAllDebugOnExit = 0;
static int bAbortTrace = 1;	/* print a trace after SIGABRT or SIGSEGV */
static int bOutputTidToStderr = 0;/* output TID to stderr on thread creation */
char *pszAltDbgFileName = NULL; /* if set, debug output is *also* sent to here */
int altdbg = -1;	/* and the handle for alternate debug output */
int stddbg = 1; /* the handle for regular debug output, set to stdout if not forking, -1 otherwise */
static uint64_t dummy_errcount = 0; /* just to avoid some static analyzer complaints */

/* list of files/objects that should be printed */
typedef struct dbgPrintName_s {
	uchar *pName;
	struct dbgPrintName_s *pNext;
} dbgPrintName_t;


/* forward definitions */
static void dbgGetThrdName(char *pszBuf, size_t lenBuf, pthread_t thrd, int bIncludeNumID);
static dbgThrdInfo_t *dbgGetThrdInfo(void);


/* This lists are single-linked and members are added at the top */
static dbgPrintName_t *printNameFileRoot = NULL;


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

static pthread_mutex_t mutFuncDBList;

typedef struct dbgMutLog_s {
	struct dbgMutLog_s *pNext;
	struct dbgMutLog_s *pPrev;
	pthread_mutex_t *mut;
	pthread_t thrd;
	dbgFuncDB_t *pFuncDB;
	int lockLn;	/* the actual line where the mutex was locked */
	short mutexOp;
} dbgMutLog_t;


static dbgThrdInfo_t *dbgCallStackListRoot = NULL;
static dbgThrdInfo_t *dbgCallStackListLast = NULL;
static pthread_mutex_t mutCallStack;

static pthread_mutex_t mutdbgprint;

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
	dbgprintf("%10.10ld times called: %s:%d:%s\n", pFuncDB->nTimesCalled, pFuncDB->file, pFuncDB->line,
		pFuncDB->func);
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


/* ------------------------- END FuncDB utility functions ------------------------- */

/* output the current thread ID to "relevant" places
 * (what "relevant" means is determinded by various ways)
 */
void
dbgOutputTID(char* name __attribute__((unused)))
{
#	if defined(HAVE_SYSCALL) && defined(HAVE_SYS_gettid)
	if(bOutputTidToStderr)
		fprintf(stderr, "thread tid %u, name '%s'\n",
			(unsigned)syscall(SYS_gettid), name);
	DBGPRINTF("thread created, tid %u, name '%s'\n",
	          (unsigned)syscall(SYS_gettid), name);
#	endif
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
		if((pThrd = calloc(1, sizeof(dbgThrdInfo_t))) != NULL) {
			pThrd->thrd = pthread_self();
			(void) pthread_setspecific(keyCallStack, pThrd);
			DLL_Add(CallStack, pThrd);
		}
	}
	pthread_mutex_unlock(&mutCallStack);
	return pThrd;
}



/* find a specific thread ID. It must be present, else something is wrong
 */
static dbgThrdInfo_t *
dbgFindThrd(pthread_t thrd)
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
		snprintf(pszBuf, lenBuf, "%lx", (long) thrd);
	} else {
		if(bIncludeNumID) {
			snprintf(pszBuf, lenBuf, "%-15s (%lx)", pThrd->pszThrdName, (long) thrd);
		} else {
			snprintf(pszBuf, lenBuf, "%-15s", pThrd->pszThrdName);
		}
	}
}


/* set a name for the current thread. The caller provided string is duplicated.
 * Note: we must lock the "dbgprint" mutex, because dbgprint() uses the thread
 * name and we could get a race (and abort) in cases where both are executed in
 * parallel and we free or incompletely-copy the string.
 */
void dbgSetThrdName(uchar *pszName)
{
	pthread_mutex_lock(&mutdbgprint);
	dbgThrdInfo_t *pThrd = dbgGetThrdInfo();
	if(pThrd->pszThrdName != NULL)
		free(pThrd->pszThrdName);
	pThrd->pszThrdName = strdup((char*)pszName);
	pthread_mutex_unlock(&mutdbgprint);
}


/* destructor for a call stack object */
static void dbgCallStackDestruct(void *arg)
{
	dbgThrdInfo_t *pThrd = (dbgThrdInfo_t*) arg;

	dbgprintf("destructor for debug call stack %p called\n", pThrd);
	if(pThrd->pszThrdName != NULL) {
		free(pThrd->pszThrdName);
	}

	pthread_mutex_lock(&mutCallStack);
	DLL_Del(CallStack, pThrd);
	pthread_mutex_unlock(&mutCallStack);
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
		dbgprintf("%d: %s:%d:%s:\n", i, pThrd->callStack[i]->file, pThrd->lastLine[i],
			pThrd->callStack[i]->func);
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

static
void dbgPrintAllDebugInfo(void)
{
	dbgCallStackPrintAll();
	if(bPrintFuncDBOnExit)
		dbgFuncDBPrintAll();
}


/* actually write the debug message. This is a separate fuction because the cleanup_push/_pop
 * interface otherwise is unsafe to use (generates compiler warnings at least).
 * 2009-05-20 rgerhards
 */
static void
do_dbgprint(uchar *pszObjName, char *pszMsg, const char *pszFileName, size_t lenMsg)
{
	static pthread_t ptLastThrdID = 0;
	static int bWasNL = 0;
	char pszThrdName[64]; /* 64 is to be on the safe side, anything over 20 is bad... */
	char pszWriteBuf[32*1024];
	size_t lenCopy;
	size_t offsWriteBuf = 0;
	size_t lenWriteBuf;
	struct timespec t;
#	if  _POSIX_TIMERS <= 0
	struct timeval tv;
#	endif

#if 1
	/* The bWasNL handler does not really work. It works if no thread
	 * switching occurs during non-NL messages. Else, things are messed
	 * up. Anyhow, it works well enough to provide useful help during
	 * getting this up and running. It is questionable if the extra effort
	 * is worth fixing it, giving the limited appliability. -- rgerhards, 2005-10-25
	 * I have decided that it is not worth fixing it - especially as it works
	 * pretty well. -- rgerhards, 2007-06-15
	 */
	if(ptLastThrdID != pthread_self()) {
		if(!bWasNL) {
			pszWriteBuf[0] = '\n';
			offsWriteBuf = 1;
			bWasNL = 1;
		}
		ptLastThrdID = pthread_self();
	}

	/* do not cache the thread name, as the caller might have changed it
	 * TODO: optimized, invalidate cache when new name is set
	 */
	dbgGetThrdName(pszThrdName, sizeof(pszThrdName), ptLastThrdID, 0);

	if(bWasNL) {
		if(bPrintTime) {
#			if _POSIX_TIMERS > 0
			/* this is the "regular" code */
			clock_gettime(CLOCK_REALTIME, &t);
#			else
			gettimeofday(&tv, NULL);
			t.tv_sec = tv.tv_sec;
			t.tv_nsec = tv.tv_usec * 1000;
#			endif
			lenWriteBuf = snprintf(pszWriteBuf+offsWriteBuf, sizeof(pszWriteBuf) - offsWriteBuf,
				 	"%4.4ld.%9.9ld:", (long) (t.tv_sec % 10000), t.tv_nsec);
			offsWriteBuf += lenWriteBuf;
		}

		lenWriteBuf = snprintf(pszWriteBuf + offsWriteBuf, sizeof(pszWriteBuf) - offsWriteBuf,
				"%s: ", pszThrdName);
		offsWriteBuf += lenWriteBuf;
		/* print object name header if we have an object */
		if(pszObjName != NULL) {
			lenWriteBuf = snprintf(pszWriteBuf + offsWriteBuf, sizeof(pszWriteBuf) - offsWriteBuf,
					"%s: ", pszObjName);
			offsWriteBuf += lenWriteBuf;
		}
		lenWriteBuf = snprintf(pszWriteBuf + offsWriteBuf, sizeof(pszWriteBuf) - offsWriteBuf,
			"%s: ", pszFileName);
		offsWriteBuf += lenWriteBuf;
	}
#endif
	if(lenMsg > sizeof(pszWriteBuf) - offsWriteBuf)
		lenCopy = sizeof(pszWriteBuf) - offsWriteBuf;
	else
		lenCopy = lenMsg;
	memcpy(pszWriteBuf + offsWriteBuf, pszMsg, lenCopy);
	offsWriteBuf += lenCopy;
	/* the write is included in an "if" just to silence compiler
	 * warnings. Here, we really don't care if the write fails, we
	 * have no good response to that in any case... -- rgerhards, 2012-11-28
	 */
	if(stddbg != -1) {
		if(write(stddbg, pszWriteBuf, offsWriteBuf)) {
			++dummy_errcount;
		}
	}
	if(altdbg != -1) {
		if(write(altdbg, pszWriteBuf, offsWriteBuf)) {
			++dummy_errcount;
		}
	}

	bWasNL = (pszMsg[lenMsg - 1] == '\n') ? 1 : 0;
}

static void
dbgprintfWithCancelHdlr(uchar *const pszObjName, char *pszMsg,
	const char *pszFileName, const size_t lenMsg)
{
	pthread_mutex_lock(&mutdbgprint);
	pthread_cleanup_push(dbgMutexCancelCleanupHdlr, &mutdbgprint);
	do_dbgprint(pszObjName, pszMsg, pszFileName, lenMsg);
	pthread_cleanup_pop(1);
}
/* write the debug message. This is a helper to dbgprintf and dbgoprint which
 * contains common code. added 2008-09-26 rgerhards
 * Note: We need to split the function due to the bad nature of POSIX
 * cancel cleanup handlers.
 */
static void DBGL_UNUSED
dbgprint(obj_t *pObj, char *pszMsg, const char *pszFileName, const size_t lenMsg)
{
	uchar *pszObjName = NULL;

	/* we must get the object name before we lock the mutex, because the object
	 * potentially calls back into us. If we locked the mutex, we would deadlock
	 * ourselfs. On the other hand, the GetName call needs not to be protected, as
	 * this thread has a valid reference. If such an object is deleted by another
	 * thread, we are in much more trouble than just for dbgprint(). -- rgerhards, 2008-09-26
	 */
	if(pObj != NULL) {
		pszObjName = obj.GetName(pObj);
	}

	dbgprintfWithCancelHdlr(pszObjName, pszMsg, pszFileName, lenMsg);
}

static int DBGL_UNUSED
checkDbgFile(const char *srcname)
{

	if(glblDbgFilesNum == 0) {
		return 1;
	}
	if(glblDbgWhitelist) {
		if(bsearch(srcname, glblDbgFiles, glblDbgFilesNum, sizeof(char*), bs_arrcmp_glblDbgFiles) == NULL) {
			return 0;
		} else {
			return 1;
		}
	} else {
		if(bsearch(srcname, glblDbgFiles, glblDbgFilesNum, sizeof(char*), bs_arrcmp_glblDbgFiles) != NULL) {
			return 0;
		} else {
			return 1;
		}
	}
}
/* print some debug output when an object is given
 * This is mostly a copy of dbgprintf, but I do not know how to combine it
 * into a single function as we have variable arguments and I don't know how to call
 * from one vararg function into another. I don't dig in this, it is OK for the
 * time being. -- rgerhards, 2008-01-29
 */
#ifndef DEBUGLESS
void
r_dbgoprint( const char *srcname, obj_t *pObj, const char *fmt, ...)
{
	va_list ap;
	char pszWriteBuf[32*1024];
	size_t lenWriteBuf;

	if(!(Debug && debugging_on))
		return;
	
	if(!checkDbgFile(srcname)) {
		return;
	}

	/* a quick and very dirty hack to enable us to display just from those objects
	 * that we are interested in. So far, this must be changed at compile time (and
	 * chances are great it is commented out while you read it. Later, this shall
	 * be selectable via the environment. -- rgerhards, 2008-02-20
	 */
#if 0
	if(objGetObjID(pObj) != OBJexpr)
		return;
#endif

	va_start(ap, fmt);
	lenWriteBuf = vsnprintf(pszWriteBuf, sizeof(pszWriteBuf), fmt, ap);
	va_end(ap);
	if(lenWriteBuf >= sizeof(pszWriteBuf)) {
		/* prevent buffer overrruns and garbagge display */
		pszWriteBuf[sizeof(pszWriteBuf) - 5] = '.';
		pszWriteBuf[sizeof(pszWriteBuf) - 4] = '.';
		pszWriteBuf[sizeof(pszWriteBuf) - 3] = '.';
		pszWriteBuf[sizeof(pszWriteBuf) - 2] = '\n';
		pszWriteBuf[sizeof(pszWriteBuf) - 1] = '\0';
		lenWriteBuf = sizeof(pszWriteBuf);
	}
	dbgprint(pObj, pszWriteBuf, srcname, lenWriteBuf);
}
#endif

/* print some debug output when no object is given
 * WARNING: duplicate code, see dbgoprint above!
 */
#ifndef DEBUGLESS
void
r_dbgprintf(const char *srcname, const char *fmt, ...)
{
	va_list ap;
	char pszWriteBuf[32*1024];
	size_t lenWriteBuf;

	if(!(Debug && debugging_on)) {
		return;
	}

	if(!checkDbgFile(srcname)) {
		return;
	}

	va_start(ap, fmt);
	lenWriteBuf = vsnprintf(pszWriteBuf, sizeof(pszWriteBuf), fmt, ap);
	va_end(ap);
	if(lenWriteBuf >= sizeof(pszWriteBuf)) {
		/* prevent buffer overrruns and garbagge display */
		pszWriteBuf[sizeof(pszWriteBuf) - 5] = '.';
		pszWriteBuf[sizeof(pszWriteBuf) - 4] = '.';
		pszWriteBuf[sizeof(pszWriteBuf) - 3] = '.';
		pszWriteBuf[sizeof(pszWriteBuf) - 2] = '\n';
		pszWriteBuf[sizeof(pszWriteBuf) - 1] = '\0';
		lenWriteBuf = sizeof(pszWriteBuf);
	}
	dbgprint(NULL, pszWriteBuf, srcname, lenWriteBuf);
}
#endif


/* Handler for SIGUSR2. Dumps all available debug output
 */
static void sigusr2Hdlr(int __attribute__((unused)) signum)
{
	dbgprintf("SIGUSR2 received, dumping debug information\n");
	dbgPrintAllDebugInfo();
}

/* support system to set debug options at runtime */


/* parse a param/value pair from the current location of the
 * option string. Returns 1 if an option was found, 0
 * otherwise. 0 means there are NO MORE options to be
 * processed. -- rgerhards, 2008-02-28
 */
static int
dbgGetRTOptNamVal(uchar **ppszOpt, uchar **ppOptName, uchar **ppOptVal)
{
	int bRet = 0;
	uchar *p;
	size_t i;
	static uchar optname[128]; /* not thread- or reentrant-safe, but that      */
	static uchar optval[1024]; /* doesn't matter (called only once at startup) */

	assert(ppszOpt != NULL);
	assert(*ppszOpt != NULL);

	/* make sure we have some initial values */
	optname[0] = '\0';
	optval[0] = '\0';

	p = *ppszOpt;
	/* skip whitespace */
	while(*p && isspace(*p))
		++p;

	/* name - up until '=' or whitespace */
	i = 0;
	while(i < (sizeof(optname) - 1) && *p && *p != '=' && !isspace(*p)) {
		optname[i++] = *p++;
	}

	if(i > 0) {
		bRet = 1;
		optname[i] = '\0';
		if(*p == '=') {
			/* we have a value, get it */
			++p;
			i = 0;
			while(i < (sizeof(optval) - 1) && *p && !isspace(*p)) {
				optval[i++] = *p++;
			}
			optval[i] = '\0';
		}
	}

	/* done */
	*ppszOpt = p;
	*ppOptName = optname;
	*ppOptVal = optval;
	return bRet;
}


/* create new PrintName list entry and add it to list (they will never
 * be removed. -- rgerhards, 2008-02-28
 */
static void
dbgPrintNameAdd(uchar *pName, dbgPrintName_t **ppRoot)
{
	dbgPrintName_t *pEntry;

	if((pEntry = calloc(1, sizeof(dbgPrintName_t))) == NULL) {
		fprintf(stderr, "ERROR: out of memory during debug setup\n");
		exit(1);
	}

	if((pEntry->pName = (uchar*) strdup((char*) pName)) == NULL) {
		fprintf(stderr, "ERROR: out of memory during debug setup\n");
		exit(1);
	}

	if(*ppRoot != NULL) {
		pEntry->pNext = *ppRoot; /* we enqueue at the front */
	}
	*ppRoot = pEntry;
}


/* report fd used for debug log. This is needed in case of
 * auto-backgrounding, where the debug log shall not be closed.
 */
int
dbgGetDbglogFd(void)
{
	return altdbg;
}

/* read in the runtime options
 * rgerhards, 2008-02-28
 */
static void
dbgGetRuntimeOptions(void)
{
	uchar *pszOpts;
	uchar *optval;
	uchar *optname;

	/* set some defaults */
	if((pszOpts = (uchar*) getenv("RSYSLOG_DEBUG")) != NULL) {
		/* we have options set, so let's process them */
		while(dbgGetRTOptNamVal(&pszOpts, &optname, &optval)) {
			if(!strcasecmp((char*)optname, "help")) {
				fprintf(stderr,
					"rsyslogd " VERSION " runtime debug support - help requested, "
					"rsyslog terminates\n\nenvironment variables:\n"
					"addional logfile: export RSYSLOG_DEBUGFILE=\"/path/to/file\"\n"
					"to set: export RSYSLOG_DEBUG=\"cmd cmd cmd\"\n\n"
					"Commands are (all case-insensitive):\n"
					"help (this list, terminates rsyslogd\n"
					"LogFuncFlow\n"
					"LogAllocFree (very partly implemented)\n"
					"PrintFuncDB\n"
					"PrintMutexAction\n"
					"PrintAllDebugInfoOnExit (not yet implemented)\n"
					"NoLogTimestamp\n"
					"Nostdoout\n"
					"OutputTidToStderr\n"
					"filetrace=file (may be provided multiple times)\n"
					"DebugOnDemand - enables debugging on USR1, but does not turn on output\n"
					"\nSee debug.html in your doc set or http://www.rsyslog.com for details\n");
				exit(1);
			} else if(!strcasecmp((char*)optname, "debug")) {
				/* this is earlier in the process than the -d option, as such it
				 * allows us to spit out debug messages from the very beginning.
				 */
				Debug = DEBUG_FULL;
				debugging_on = 1;
			} else if(!strcasecmp((char*)optname, "debugondemand")) {
				/* Enables debugging, but turns off debug output */
				Debug = DEBUG_ONDEMAND;
				debugging_on = 1;
				dbgprintf("Note: debug on demand turned on via configuraton file, "
					  "use USR1 signal to activate.\n");
				debugging_on = 0;
			} else if(!strcasecmp((char*)optname, "logfuncflow")) {
				bLogFuncFlow = 1;
			} else if(!strcasecmp((char*)optname, "logallocfree")) {
				bLogAllocFree = 1;
			} else if(!strcasecmp((char*)optname, "printfuncdb")) {
				bPrintFuncDBOnExit = 1;
			} else if(!strcasecmp((char*)optname, "printmutexaction")) {
				bPrintMutexAction = 1;
			} else if(!strcasecmp((char*)optname, "printalldebuginfoonexit")) {
				bPrintAllDebugOnExit = 1;
			} else if(!strcasecmp((char*)optname, "nologtimestamp")) {
				bPrintTime = 0;
			} else if(!strcasecmp((char*)optname, "nostdout")) {
				stddbg = -1;
			} else if(!strcasecmp((char*)optname, "noaborttrace")) {
				bAbortTrace = 0;
			} else if(!strcasecmp((char*)optname, "outputtidtostderr")) {
				bOutputTidToStderr = 1;
			} else if(!strcasecmp((char*)optname, "filetrace")) {
				if(*optval == '\0') {
					fprintf(stderr, "rsyslogd " VERSION " error: logfile debug option requires "
					"filename, e.g. \"logfile=debug.c\"\n");
					exit(1);
				} else {
					/* create new entry and add it to list */
					dbgPrintNameAdd(optval, &printNameFileRoot);
				}
			} else {
				fprintf(stderr, "rsyslogd " VERSION " error: invalid debug option '%s', "
					"value '%s' - ignored\n", optval, optname);
			}
		}
	}
}


void
dbgSetDebugLevel(int level)
{
	Debug = level;
	debugging_on = (level == DEBUG_FULL) ? 1 : 0;
}

void
dbgSetDebugFile(uchar *fn)
{
	if(altdbg != -1) {
		dbgprintf("switching to debug file %s\n", fn);
		close(altdbg);
	}
	if((altdbg = open((char*)fn, O_WRONLY|O_CREAT|O_TRUNC|O_NOCTTY|O_CLOEXEC, S_IRUSR|S_IWUSR)) == -1) {
		fprintf(stderr, "alternate debug file could not be opened, ignoring. Error: %s\n", strerror(errno));
	}
}

/* end support system to set debug options at runtime */

rsRetVal dbgClassInit(void)
{
	pthread_mutexattr_t mutAttr;
	rsRetVal iRet;	/* do not use DEFiRet, as this makes calls into the debug system! */

	struct sigaction sigAct;
	sigset_t sigSet;
	
	(void) pthread_key_create(&keyCallStack, dbgCallStackDestruct); /* MUST be the first action done! */

	/* the mutexes must be recursive, because it may be called from within
	 * signal handlers, which can lead to a hang if the signal interrupted dbgprintf
	 * (yes, we have really seen that situation in practice!). -- rgerhards, 2013-05-17
	 */
	pthread_mutexattr_init(&mutAttr);
	pthread_mutexattr_settype(&mutAttr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&mutFuncDBList, &mutAttr);
	pthread_mutex_init(&mutCallStack, &mutAttr);
	pthread_mutex_init(&mutdbgprint, &mutAttr);

	/* while we try not to use any of the real rsyslog code (to avoid infinite loops), we
	 * need to have the ability to query object names. Thus, we need to obtain a pointer to
	 * the object interface. -- rgerhards, 2008-02-29
	 */
	CHKiRet(objGetObjInterface(&obj)); /* this provides the root pointer for all other queries */

	memset(&sigAct, 0, sizeof (sigAct));
	sigemptyset(&sigAct.sa_mask);
	sigAct.sa_handler = sigusr2Hdlr;
	sigaction(SIGUSR2, &sigAct, NULL);

	sigemptyset(&sigSet);
	sigaddset(&sigSet, SIGUSR2);
	pthread_sigmask(SIG_UNBLOCK, &sigSet, NULL);

	const char *dbgto2stderr;
	dbgto2stderr = getenv("RSYSLOG_DEBUG_TIMEOUTS_TO_STDERR");
	dbgTimeoutToStderr = (dbgto2stderr != NULL && !strcmp(dbgto2stderr, "on")) ? 1 : 0;
	if(dbgTimeoutToStderr) {
		fprintf(stderr, "rsyslogd: NOTE: RSYSLOG_DEBUG_TIMEOUTS_TO_STDERR activated\n");
	}
	dbgGetRuntimeOptions(); /* init debug system from environment */
	pszAltDbgFileName = getenv("RSYSLOG_DEBUGLOG");

	if(pszAltDbgFileName != NULL) {
		/* we have a secondary file, so let's open it) */
		if((altdbg = open(pszAltDbgFileName, O_WRONLY|O_CREAT|O_TRUNC|O_NOCTTY|O_CLOEXEC, S_IRUSR|S_IWUSR))
		== -1) {
			fprintf(stderr, "alternate debug file could not be opened, ignoring. Error: %s\n",
				strerror(errno));
		}
	}

	dbgSetThrdName((uchar*)"main thread");

finalize_it:
	return(iRet);
}


rsRetVal dbgClassExit(void)
{
	dbgFuncDBListEntry_t *pFuncDBListEtry, *pToDel;
	pthread_key_delete(keyCallStack);

	if(bPrintAllDebugOnExit)
		dbgPrintAllDebugInfo();

	if(altdbg != -1)
		close(altdbg);

	/* now free all of our memory to make the memory debugger happy... */
	pFuncDBListEtry = pFuncDBListRoot;
	while(pFuncDBListEtry != NULL) {
		pToDel = pFuncDBListEtry;
		pFuncDBListEtry = pFuncDBListEtry->pNext;
		free(pToDel->pFuncDB->file);
		free(pToDel->pFuncDB->func);
		free(pToDel->pFuncDB);
		free(pToDel);
	}

	return RS_RET_OK;
}
/* vi:set ai:
 */
