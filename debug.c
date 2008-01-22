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

static FILE *stddbg;

typedef struct dbgCallStack_s {
	pthread_t thrd;
	const char* callStack[100000];
	int stackPtr;
	struct dbgCallStack_s *pNext;
	struct dbgCallStack_s *pPrev;
} dbgCallStack_t;
static dbgCallStack_t *dbgCallStackListRoot = NULL;
static dbgCallStack_t *dbgCallStackListLast = NULL;
static pthread_mutex_t mutCallStack = PTHREAD_MUTEX_INITIALIZER;

static pthread_key_t keyCallStack;

/* destructor for a call stack object */
static void dbgCallStackDestruct(void *arg)
{
	dbgCallStack_t *pStack = (dbgCallStack_t*) arg;

	dbgprintf("destructor for debug call stack %p called\n", pStack);
	if(pStack->pPrev != NULL)
		pStack->pPrev->pNext = pStack->pNext;
	if(pStack->pNext != NULL)
		pStack->pNext->pPrev = pStack->pPrev;
	if(pStack == dbgCallStackListRoot)
		dbgCallStackListRoot = pStack->pNext;
	if(pStack == dbgCallStackListLast)
		dbgCallStackListLast = pStack->pNext;
	free(pStack);
}


/* print a thread's call stack
 */
static void dbgCallStackPrint(dbgCallStack_t *pStack)
{
	int i;

	/* TODO: mutex guard! */
	dbgprintf("\nRecorded Call Order for Thread 0x%lx (%p):\n", (unsigned long) pStack->thrd, pStack);
	for(i = 0 ; i < pStack->stackPtr ; i++) {
		dbgprintf("%s()\n", pStack->callStack[i]);
	}
	dbgprintf("NOTE: not all calls may have been recorded.\n");
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
		if(dbgCallStackListRoot == NULL) {
			dbgCallStackListRoot = pStack;
			dbgCallStackListLast = pStack;
		} else {
			pStack->pPrev = dbgCallStackListLast;
			dbgCallStackListLast->pNext = pStack;
			dbgCallStackListLast = pStack;
		}
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
	dbgCallStack_t *pStack;

	if(signum == SIGSEGV) {
		signame = " (SIGSEGV)";
	} else {
		signame = "";
	}

	dbgprintf("Signal %d%s occured, execution must be terminated %d.\n", signum, signame, SIGSEGV);

	/* stack info */
	for(pStack = dbgCallStackListRoot ; pStack != NULL ; pStack = pStack->pNext) {
		dbgCallStackPrint(pStack);
	}

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

	if ( !(Debug && debugging_on) )
		return;
	
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
	return;
}


/* handler called when a function is entered
 */
void dbgEntrFunc(char* file, int line, const char* func)
{
	dbgCallStack_t *pStack = dbgGetCallStack();

	dbgprintf("%s:%d: %s: enter\n", file, line, func);
	pStack->callStack[pStack->stackPtr++] = func;
	dbgprintf("stack %d\n", pStack->stackPtr);
	assert(pStack->stackPtr < (int) (sizeof(pStack->callStack) / sizeof(const char*)));
	
}


/* handler called when a function is exited
 */
void dbgExitFunc(char* file, int line, const char* func)
{
	dbgCallStack_t *pStack = dbgGetCallStack();

	dbgprintf("%s:%d: %s: exit\n", file, line, func);
	pStack->stackPtr--;
	assert(pStack->stackPtr > 0);
}

rsRetVal dbgClassInit(void)
{
	stddbg = stdout;
	(void) pthread_key_create(&keyCallStack, dbgCallStackDestruct);
	return RS_RET_OK;
}


rsRetVal dbgClassExit(void)
{
	pthread_key_delete(keyCallStack);
	//(void) pthread_key_create(&keyCallStack, dbgCallStackDestructor);
	return RS_RET_OK;
}
/*
 * vi:set ai:
 */
