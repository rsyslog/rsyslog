/* debug.c
 *
 * This file proides debug support.
 *
 * File begun on 2008-01-22 by RGerhards
 *
 * Some functions are controlled by environment variables:
 *
 * RSYSLOG_DEBUGLOG if set, a debug log file is written to that location
 * RSYSLOG_DEBUG    specific debug options
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
#include "cfsysline.h"
#include "obj.h"


/* static data (some time to be replaced) */
DEFobjCurrIf(obj)
int Debug = DEBUG_OFF;      /* debug flag  - read-only after startup */
int debugging_on = 0;    /* read-only, except on sig USR1 */
int dbgTimeoutToStderr = 0;
static int bPrintTime = 1;  /* print a timestamp together with debug message */
static int bOutputTidToStderr = 0;/* output TID to stderr on thread creation */
char *pszAltDbgFileName = NULL; /* if set, debug output is *also* sent to here */
int altdbg = -1;    /* and the handle for alternate debug output */
int stddbg = 1; /* the handle for regular debug output, set to stdout if not forking, -1 otherwise */
static uint64_t dummy_errcount = 0; /* just to avoid some static analyzer complaints */
static pthread_key_t keyThrdName;


/* output the current thread ID to "relevant" places
 * (what "relevant" means is determinded by various ways)
 */
void
dbgOutputTID(char* name __attribute__((unused)))
{
#   if defined(HAVE_SYSCALL) && defined(HAVE_SYS_gettid)
    if(bOutputTidToStderr)
        fprintf(stderr, "thread tid %u, name '%s'\n",
            (unsigned)syscall(SYS_gettid), name);
    DBGPRINTF("thread created, tid %u, name '%s'\n",
              (unsigned)syscall(SYS_gettid), name);
#   endif
}


/* build a string with the thread name. If none is set, the thread ID is
 * used instead. Caller must provide buffer space.
 */
static void ATTR_NONNULL()
dbgGetThrdName(char *const pszBuf, const size_t lenBuf, const pthread_t thrdID)
{
    assert(pszBuf != NULL);

    const char *const thrdName = pthread_getspecific(keyThrdName);
    if(thrdName == NULL) {
        snprintf(pszBuf, lenBuf, "%lx", (long) thrdID);
    } else {
        snprintf(pszBuf, lenBuf, "%-15s", thrdName);
    }
}


/* set a name for the current thread */
void ATTR_NONNULL()
dbgSetThrdName(const uchar *const pszName)
{
    (void) pthread_setspecific(keyThrdName, strdup((char*) pszName));
}


/* destructor for thread name (called by pthreads!) */
static void dbgThrdNameDestruct(void *arg)
{
    free(arg);
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
    static pthread_t ptLastThrdID = 0;
    static int bWasNL = 0;
    char pszThrdName[64]; /* 64 is to be on the safe side, anything over 20 is bad... */
    char pszWriteBuf[32*1024];
    size_t lenCopy;
    size_t offsWriteBuf = 0;
    size_t lenWriteBuf;
    struct timespec t;
#   if  _POSIX_TIMERS <= 0
    struct timeval tv;
#   endif

    if(pObj != NULL) {
        pszObjName = obj.GetName(pObj);
    }

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

    dbgGetThrdName(pszThrdName, sizeof(pszThrdName), ptLastThrdID);

    if(bWasNL) {
        if(bPrintTime) {
#           if _POSIX_TIMERS > 0
            /* this is the "regular" code */
            clock_gettime(CLOCK_REALTIME, &t);
#           else
            gettimeofday(&tv, NULL);
            t.tv_sec = tv.tv_sec;
            t.tv_nsec = tv.tv_usec * 1000;
#           endif
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

/* support system to set debug options at runtime */


/* parse a param/value pair from the current location of the
 * option string. Returns 1 if an option was found, 0
 * otherwise. 0 means there are NO MORE options to be
 * processed. -- rgerhards, 2008-02-28
 */
static int ATTR_NONNULL()
dbgGetRTOptNamVal(const uchar **const ppszOpt, uchar **const ppOptName)
{
    assert(ppszOpt != NULL);
    assert(*ppszOpt != NULL);
    int bRet = 0;
    const uchar *p = *ppszOpt;
    size_t i;
    static uchar optname[128] = "";

    /* skip whitespace */
    while(*p && isspace(*p))
        ++p;

    /* name: up until whitespace */
    i = 0;
    while(i < (sizeof(optname) - 1) && *p && !isspace(*p)) {
        optname[i++] = *p++;
    }

    if(i > 0) {
        bRet = 1;
        optname[i] = '\0';
    }

    /* done */
    *ppszOpt = p;
    *ppOptName = optname;
    return bRet;
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
    const uchar *pszOpts;
    uchar *optname;

    /* set some defaults */
    if((pszOpts = (uchar*) getenv("RSYSLOG_DEBUG")) != NULL) {
        /* we have options set, so let's process them */
        while(dbgGetRTOptNamVal(&pszOpts, &optname)) {
            if(!strcasecmp((char*)optname, "help")) {
                fprintf(stderr,
                    "rsyslogd " VERSION " runtime debug support - help requested, "
                    "rsyslog terminates\n\nenvironment variables:\n"
                    "additional logfile: export RSYSLOG_DEBUGFILE=\"/path/to/file\"\n"
                    "to set: export RSYSLOG_DEBUG=\"cmd cmd cmd\"\n\n"
                    "Commands are (all case-insensitive):\n"
                    "help (this list, terminates rsyslogd)\n"
                    "NoLogTimestamp\n"
                    "Nostdout\n"
                    "OutputTidToStderr\n"
                    "DebugOnDemand - enables debugging on USR1, but does not turn on output\n"
                    "\nSee debug.html in your doc set or https://www.rsyslog.com for details\n");
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
                dbgprintf("Note: debug on demand turned on via configuration file, "
                      "use USR1 signal to activate.\n");
                debugging_on = 0;
            } else if(!strcasecmp((char*)optname, "nologtimestamp")) {
                bPrintTime = 0;
            } else if(!strcasecmp((char*)optname, "nostdout")) {
                stddbg = -1;
            } else if(!strcasecmp((char*)optname, "outputtidtostderr")) {
                bOutputTidToStderr = 1;
            } else {
                fprintf(stderr, "rsyslogd " VERSION " error: invalid debug option '%s' "
                    "- ignored\n", optname);
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
    rsRetVal iRet;  /* do not use DEFiRet, as this makes calls into the debug system! */


    (void) pthread_key_create(&keyThrdName, dbgThrdNameDestruct);

    /* while we try not to use any of the real rsyslog code (to avoid infinite loops), we
     * need to have the ability to query object names. Thus, we need to obtain a pointer to
     * the object interface. -- rgerhards, 2008-02-29
     */
    CHKiRet(objGetObjInterface(&obj)); /* this provides the root pointer for all other queries */

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
    pthread_key_delete(keyThrdName);
    if(altdbg != -1)
        close(altdbg);
    return RS_RET_OK;
}
