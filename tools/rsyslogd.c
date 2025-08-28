/* This is the main rsyslogd file.
 * It contains code * that is known to be validly under ASL 2.0,
 * because it was either written from scratch by me (rgerhards) or
 * contributors who agreed to ASL 2.0.
 *
 * Copyright 2004-2024 Rainer Gerhards and Adiscon
 *
 * This file is part of rsyslog.
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
#include "config.h"

#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#ifdef ENABLE_LIBLOGGING_STDLOG
    #include <liblogging/stdlog.h>
#else
    #include <syslog.h>
#endif
#ifdef HAVE_LIBSYSTEMD
    #include <systemd/sd-daemon.h>
#endif
#ifdef ENABLE_LIBCAPNG
    #include <cap-ng.h>
#endif
#if defined(HAVE_LINUX_CLOSE_RANGE_H)
    #include <linux/close_range.h>
#endif

#include "rsyslog.h"
#include "wti.h"
#include "ratelimit.h"
#include "parser.h"
#include "linkedlist.h"
#include "ruleset.h"
#include "action.h"
#include "iminternal.h"
#include "errmsg.h"
#include "threads.h"
#include "dnscache.h"
#include "prop.h"
#include "unicode-helper.h"
#include "net.h"
#include "glbl.h"
#include "debug.h"
#include "srUtils.h"
#include "rsconf.h"
#include "cfsysline.h"
#include "datetime.h"
#include "operatingstate.h"
#include "dirty.h"
#include "janitor.h"
#include "parserif.h"

/* some global vars we need to differentiate between environments,
 * for TZ-related things see
 * https://github.com/rsyslog/rsyslog/issues/2994
 */
static int runningInContainer = 0;
#ifdef OS_LINUX
static int emitTZWarning = 0;
#else
static int emitTZWarning = 1;
#endif
static pthread_t mainthread = 0;

#if defined(_AIX)
    /* AIXPORT : start
     * The following includes and declarations are for support of the System
     * Resource Controller (SRC) .
     */
    #include <sys/select.h>
    /* AIXPORT : start*/
    #define SRC_FD 13
    #define SRCMSG (sizeof(srcpacket))

static void deinitAll(void);
    #include <spc.h>
static struct srcreq srcpacket;
int cont;
struct srchdr *srchdr;
char progname[128];


/* Normally defined as locals in main
 * But here since the functionality is split
 * across multiple functions, we make it global
 */
static int rc;
static socklen_t addrsz;
static struct sockaddr srcaddr;
int src_exists = TRUE;
    /* src end */

    /*
     * SRC packet processing - .
     */
    #define SRCMIN(a, b) (a < b) ? a : b
void dosrcpacket(msgno, txt, len) int msgno;
char *txt;
int len;
{
    struct srcrep reply;

    reply.svrreply.rtncode = msgno;
    /* AIXPORT :  srv was corrected to syslogd */
    strcpy(reply.svrreply.objname, "syslogd");
    snprintf(reply.svrreply.rtnmsg, SRCMIN(sizeof(reply.svrreply.rtnmsg) - 1, strlen(txt)), "%s", txt);
    srchdr = srcrrqs((char *)&srcpacket);
    srcsrpy(srchdr, (char *)&reply, len, cont);
}

    #define AIX_SRC_EXISTS_IF if (!src_exists) {
    #define AIX_SRC_EXISTS_FI }

static void aix_close_it(int i) {
    if (src_exists) {
        if (i != SRC_FD) (void)close(i);
    } else
        close(i);
}


#else

    #define AIX_SRC_EXISTS_IF
    #define AIX_SRC_EXISTS_FI
    #define aix_close_it(x) close(x)
#endif

/* AIXPORT : end  */


DEFobjCurrIf(obj) DEFobjCurrIf(prop) DEFobjCurrIf(parser) DEFobjCurrIf(ruleset) DEFobjCurrIf(net) DEFobjCurrIf(rsconf)
    DEFobjCurrIf(module) DEFobjCurrIf(datetime) DEFobjCurrIf(glbl)

        extern int yydebug; /* interface to flex */


/* forward definitions */
void rsyslogd_submitErrMsg(const int severity, const int iErr, const uchar *msg);
void rsyslogdDoDie(int sig);


#ifndef PATH_PIDFILE
    #if defined(_AIX) /* AIXPORT : Add _AIX */
        #define PATH_PIDFILE "/etc/rsyslogd.pid"
    #else
        #define PATH_PIDFILE "/var/run/rsyslogd.pid"
    #endif /*_AIX*/
#endif

#ifndef PATH_CONFFILE
    #define PATH_CONFFILE "/etc/rsyslog.conf"
#endif

/* global data items */
static pthread_mutex_t mutChildDied;
static int bChildDied = 0;
static pthread_mutex_t mutHadHUP;
static int bHadHUP;
static int doFork = 1; /* fork - run in daemon mode - read-only after startup */
int bFinished = 0; /* used by termination signal handler, read-only except there
                    * is either 0 or the number of the signal that requested the
                    * termination.
                    */
const char *PidFile = NULL;
#define NO_PIDFILE "NONE"
int iConfigVerify = 0; /* is this just a config verify run? */
rsconf_t *ourConf = NULL; /* our config object */
int MarkInterval = 20 * 60; /* interval between marks in seconds - read-only after startup */
ratelimit_t *dflt_ratelimiter = NULL; /* ratelimiter for submits without explicit one */
uchar *ConfFile = (uchar *)PATH_CONFFILE;
int bHaveMainQueue = 0; /* set to 1 if the main queue - in queueing mode - is available
                         * If the main queue is either not yet ready or not running in
                         * queueing mode (mode DIRECT!), then this is set to 0.
                         */
prop_t *pInternalInputName = NULL; /* there is only one global inputName for all internally-generated messages */
ratelimit_t *internalMsg_ratelimiter = NULL; /* ratelimiter for rsyslog-own messages */
int send_to_all = 0; /* send message to all IPv4/IPv6 addresses */

static struct queuefilenames_s {
    struct queuefilenames_s *next;
    uchar *name;
} *queuefilenames = NULL;


static __attribute__((noreturn)) void rsyslogd_usage(void) {
    fprintf(stderr,
            "usage: rsyslogd [options]\n"
            "use \"man rsyslogd\" for details. To run rsyslog "
            "interactively, use \"rsyslogd -n\"\n"
            "to run it in debug mode use \"rsyslogd -dn\"\n"
            "For further information see https://www.rsyslog.com/doc/\n");
    exit(1); /* "good" exit - done to terminate usage() */
}

#ifndef HAVE_SETSID
extern void untty(void); /* in syslogd.c, GPLv3 */
static int setsid(void) {
    untty();
    return 0;
}
#endif

/* helper for imdiag. Returns if HUP processing has been requested or
 * is not yet finished. We know this is racy, but imdiag handles this
 * part by repeating operations. The mutex look is primarily to force
 * a memory barrier, so that we have a change to see changes already
 * written, but not present in the core's cache.
 * 2023-07-26 Rainer Gerhards
 */
int get_bHadHUP(void) {
    pthread_mutex_lock(&mutHadHUP);
    const int ret = bHadHUP;
    pthread_mutex_unlock(&mutHadHUP);
    /* note: at this point ret can already be invalid */
    return ret;
}

/* we need a pointer to the conf, because in early startup stage we
 * need to use loadConf, later on runConf.
 */
rsRetVal queryLocalHostname(rsconf_t *const pConf) {
    uchar *LocalHostName = NULL;
    uchar *LocalDomain = NULL;
    uchar *LocalFQDNName;
    DEFiRet;

    assert(net.getLocalHostname != NULL); /* keep clang static analyzer silent - this IS the case */
    CHKiRet(net.getLocalHostname(pConf, &LocalFQDNName));
    uchar *dot = (uchar *)strstr((char *)LocalFQDNName, ".");
    if (dot == NULL) {
        CHKmalloc(LocalHostName = (uchar *)strdup((char *)LocalFQDNName));
        CHKmalloc(LocalDomain = (uchar *)strdup(""));
    } else {
        const size_t lenhn = dot - LocalFQDNName;
        CHKmalloc(LocalHostName = (uchar *)strndup((char *)LocalFQDNName, lenhn));
        CHKmalloc(LocalDomain = (uchar *)strdup((char *)dot + 1));
    }

    glbl.SetLocalFQDNName(LocalFQDNName);
    glbl.SetLocalHostName(LocalHostName);
    glbl.SetLocalDomain(LocalDomain);
    glbl.GenerateLocalHostNameProperty();
    LocalHostName = NULL; /* handed over */
    LocalDomain = NULL; /* handed over */

finalize_it:
    free(LocalHostName);
    free(LocalDomain);
    RETiRet;
}

static rsRetVal writePidFile(void) {
    FILE *fp;
    DEFiRet;

    const char *tmpPidFile = NULL;

    if (!strcmp(PidFile, NO_PIDFILE)) {
        FINALIZE;
    }
    if (asprintf((char **)&tmpPidFile, "%s.tmp", PidFile) == -1) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }
    if (tmpPidFile == NULL) tmpPidFile = PidFile;

    DBGPRINTF("rsyslogd: writing pidfile '%s'.\n", tmpPidFile);
    if ((fp = fopen((char *)tmpPidFile, "w")) == NULL) {
        perror("rsyslogd: error writing pid file (creation stage)\n");
        ABORT_FINALIZE(RS_RET_ERR);
    }
    if (fprintf(fp, "%d", (int)glblGetOurPid()) < 0) {
        LogError(errno, iRet, "rsyslog: error writing pid file");
    }
    fclose(fp);
    if (tmpPidFile != PidFile) {
        if (rename(tmpPidFile, PidFile) != 0) {
            perror("rsyslogd: error writing pid file (rename stage)");
        }
    }

finalize_it:
    if (tmpPidFile != PidFile) {
        free((void *)tmpPidFile);
    }
    RETiRet;
}

static void clearPidFile(void) {
    if (PidFile != NULL) {
        if (strcmp(PidFile, NO_PIDFILE)) {
            unlink(PidFile);
        }
    }
}

/* duplicate startup protection: check, based on pid file, if our instance
 * is already running. This MUST be called before we write our own pid file.
 */
static rsRetVal checkStartupOK(void) {
    FILE *fp = NULL;
    DEFiRet;

    DBGPRINTF("rsyslogd: checking if startup is ok, pidfile '%s'.\n", PidFile);

    if (!strcmp(PidFile, NO_PIDFILE)) {
        dbgprintf("no pid file shall be written, skipping check\n");
        FINALIZE;
    }

    if ((fp = fopen((char *)PidFile, "r")) == NULL) FINALIZE; /* all well, no pid file yet */

    int pf_pid;
    if (fscanf(fp, "%d", &pf_pid) != 1) {
        fprintf(stderr, "rsyslogd: error reading pid file, cannot start up\n");
        ABORT_FINALIZE(RS_RET_ERR);
    }

    /* ok, we got a pid, let's check if the process is running */
    const pid_t pid = (pid_t)pf_pid;
    if (kill(pid, 0) == 0 || errno != ESRCH) {
        fprintf(stderr,
                "rsyslogd: pidfile '%s' and pid %d already exist.\n"
                "If you want to run multiple instances of rsyslog, you need "
                "to specify\n"
                "different pid files for them (-i option).\n",
                PidFile, (int)getpid());
        ABORT_FINALIZE(RS_RET_ERR);
    }

finalize_it:
    if (fp != NULL) fclose(fp);
    RETiRet;
}


/* note: this function is specific to OS'es which provide
 * the ability to read open file descriptors via /proc.
 * returns 0 - success, something else otherwise
 */
static int close_unneeded_open_files(const char *const procdir, const int beginClose, const int parentPipeFD) {
    DIR *dir;
    struct dirent *entry;

    dir = opendir(procdir);
    if (dir == NULL) {
        dbgprintf("closes unneeded files: opendir failed for %s\n", procdir);
        return 1;
    }

    while ((entry = readdir(dir)) != NULL) {
        const int fd = atoi(entry->d_name);
        if (fd >= beginClose && (((fd != dbgGetDbglogFd()) && (fd != parentPipeFD)))) {
            close(fd);
        }
    }

    closedir(dir);
    return 0;
}

/* prepares the background processes (if auto-backbrounding) for
 * operation.
 */
static void prepareBackground(const int parentPipeFD) {
    DBGPRINTF("rsyslogd: in child, finalizing initialization\n");

    dbgTimeoutToStderr = 0; /* we loose stderr when backgrounding! */
    int r = setsid();
    if (r == -1) {
        char err[1024];
        char em[2048];
        rs_strerror_r(errno, err, sizeof(err));
        snprintf(em, sizeof(em) - 1,
                 "rsyslog: error "
                 "auto-backgrounding: %s\n",
                 err);
        dbgprintf("%s\n", em);
        fprintf(stderr, "%s", em);
    }

    int beginClose = 3;

#ifdef HAVE_LIBSYSTEMD
    /* running under systemd? Then we must make sure we "forward" any
     * fds passed by it (adjust the pid).
     */
    if (sd_booted()) {
        const char *lstnPid = getenv("LISTEN_PID");
        if (lstnPid != NULL) {
            char szBuf[64];
            const int lstnPidI = atoi(lstnPid);
            snprintf(szBuf, sizeof(szBuf), "%d", lstnPidI);
            if (!strcmp(szBuf, lstnPid) && lstnPidI == getppid()) {
                snprintf(szBuf, sizeof(szBuf), "%d", (int)getpid());
                setenv("LISTEN_PID", szBuf, 1);
                /* ensure we do not close what systemd provided */
                const int nFds = sd_listen_fds(0);
                if (nFds > 0) {
                    beginClose = SD_LISTEN_FDS_START + nFds;
                }
            }
        }
    }
#endif

    /* close unnecessary open files - first try to use /proc file system,
     * if that is not possible iterate through all potentially open file
     * descriptors. This can be lengthy, but in practice /proc should work
     * for almost all current systems, and the fallback is primarily for
     * Solaris and AIX, where we do expect a decent max numbers of fds.
     */
    close(0); /* always close stdin, we do not need it */

    /* try Linux, Cygwin, NetBSD */
    if (close_unneeded_open_files("/proc/self/fd", beginClose, parentPipeFD) != 0) {
        /* try MacOS, FreeBSD */
        if (close_unneeded_open_files("/proc/fd", beginClose, parentPipeFD) != 0) {
            /* did not work out, so let's close everything... */
            int endClose = (parentPipeFD > dbgGetDbglogFd()) ? parentPipeFD : dbgGetDbglogFd();
            for (int i = beginClose; i <= endClose; ++i) {
                if ((i != dbgGetDbglogFd()) && (i != parentPipeFD)) {
                    aix_close_it(i); /* AIXPORT */
                }
            }
            beginClose = endClose + 1;
            endClose = getdtablesize();
#if defined(HAVE_CLOSE_RANGE)
            if (close_range(beginClose, endClose, 0) != 0) {
                dbgprintf("errno %d after close_range(), fallback to loop\n", errno);
#endif
                for (int i = beginClose; i <= endClose; ++i) {
                    aix_close_it(i); /* AIXPORT */
                }
#if defined(HAVE_CLOSE_RANGE)
            }
#endif
        }
    }
    seedRandomNumberForChild();
}

/* This is called when rsyslog is set to auto-background itself. If so, a child
 * is forked and the parent waits until it is initialized.
 * The parent never returns from this function, only this happens for the child.
 * So if it returns, you know you are in the child.
 * return: file descriptor to which the child needs to write an "OK" or error
 * message.
 */
static int forkRsyslog(void) {
    int pipefd[2];
    pid_t cpid;
    char err[1024];
    char msgBuf[4096];

    dbgprintf("rsyslogd: parent ready for forking\n");
    if (pipe(pipefd) == -1) {
        perror("error creating rsyslog \"fork pipe\" - terminating");
        exit(1);
    }
    AIX_SRC_EXISTS_IF /* AIXPORT */
        cpid = fork();
    if (cpid == -1) {
        perror("error forking rsyslogd process - terminating");
        exit(1);
    }
    AIX_SRC_EXISTS_FI /* AIXPORT */

        if (cpid == 0) {
        prepareBackground(pipefd[1]);
        close(pipefd[0]);
        return pipefd[1];
    }

    /* we are now in the parent. All we need to do here is wait for the
     * startup message, emit it (if necessary) and then terminate.
     */
    close(pipefd[1]);
    dbgprintf("rsyslogd: parent waiting up to 60 seconds to read startup message\n");

    fd_set rfds;
    struct timeval tv;
    int retval;

    FD_ZERO(&rfds);
    FD_SET(pipefd[0], &rfds);
    tv.tv_sec = 60;
    tv.tv_usec = 0;

    retval = select(pipefd[0] + 1, &rfds, NULL, NULL, &tv);
    if (retval == -1)
        rs_strerror_r(errno, err, sizeof(err));
    else
        strcpy(err, "OK");
    dbgprintf("rsyslogd: select() returns %d: %s\n", retval, err);
    if (retval == -1) {
        fprintf(stderr, "rsyslog startup failure, select() failed: %s\n", err);
        exit(1);
    } else if (retval == 0) {
        fprintf(stderr,
                "rsyslog startup failure, child did not "
                "respond within startup timeout (60 seconds)\n");
        exit(1);
    }

    int nRead = read(pipefd[0], msgBuf, sizeof(msgBuf) - 1);
    if (nRead > 0) {
        msgBuf[nRead] = '\0';
    } else {
        rs_strerror_r(errno, err, sizeof(err));
        snprintf(msgBuf, sizeof(msgBuf) - 1, "error reading \"fork pipe\": %s", err);
    }
    if (strcmp(msgBuf, "OK")) {
        dbgprintf("rsyslog parent startup failure: %s\n", msgBuf);
        fprintf(stderr, "rsyslog startup failure: %s\n", msgBuf);
        exit(1);
    }
    close(pipefd[0]);
    dbgprintf("rsyslogd: parent terminates after successful child startup\n");
    exit(0);
}

/* startup processing: this signals the waiting parent that the child is ready
 * and the parent may terminate.
 */
static void tellChildReady(const int pipefd, const char *const msg) {
    dbgprintf("rsyslogd: child signaling OK\n");
    const int nWritten = write(pipefd, msg, strlen(msg));
    dbgprintf("rsyslogd: child signalled OK, nWritten %d\n", (int)nWritten);
    close(pipefd);
    sleep(1);
}

/* print version and compile-time setting information */
static void printVersion(void) {
    printf("rsyslogd  " VERSION " (aka %4d.%2.2d) compiled with:\n", 2000 + VERSION_YEAR, VERSION_MONTH);
    printf("\tPLATFORM:\t\t\t\t%s\n", PLATFORM_ID);
    printf("\tPLATFORM (lsb_release -d):\t\t%s\n", PLATFORM_ID_LSB);
#ifdef FEATURE_REGEXP
    printf("\tFEATURE_REGEXP:\t\t\t\tYes\n");
#else
    printf("\tFEATURE_REGEXP:\t\t\t\tNo\n");
#endif
#if defined(SYSLOG_INET) && defined(USE_GSSAPI)
    printf("\tGSSAPI Kerberos 5 support:\t\tYes\n");
#else
    printf("\tGSSAPI Kerberos 5 support:\t\tNo\n");
#endif
#ifndef NDEBUG
    printf("\tFEATURE_DEBUG (debug build, slow code):\tYes\n");
#else
    printf("\tFEATURE_DEBUG (debug build, slow code):\tNo\n");
#endif
#ifdef HAVE_ATOMIC_BUILTINS
    printf("\t32bit Atomic operations supported:\tYes\n");
#else
    printf("\t32bit Atomic operations supported:\tNo\n");
#endif
#ifdef HAVE_ATOMIC_BUILTINS64
    printf("\t64bit Atomic operations supported:\tYes\n");
#else
    printf("\t64bit Atomic operations supported:\tNo\n");
#endif
#ifdef HAVE_JEMALLOC
    printf("\tmemory allocator:\t\t\tjemalloc\n");
#else
    printf("\tmemory allocator:\t\t\tsystem default\n");
#endif
#ifdef RTINST
    printf("\tRuntime Instrumentation (slow code):\tYes\n");
#else
    printf("\tRuntime Instrumentation (slow code):\tNo\n");
#endif
#ifdef USE_LIBUUID
    printf("\tuuid support:\t\t\t\tYes\n");
#else
    printf("\tuuid support:\t\t\t\tNo\n");
#endif
#ifdef HAVE_LIBSYSTEMD
    printf("\tsystemd support:\t\t\tYes\n");
#else
    printf("\tsystemd support:\t\t\tNo\n");
#endif
    /* we keep the following message to so that users don't need
     * to wonder.
     */
    printf("\tConfig file:\t\t\t\t" PATH_CONFFILE "\n");
    printf("\tPID file:\t\t\t\t" PATH_PIDFILE "%s\n",
           PATH_PIDFILE[0] != '/' ? "(relative to global workingdirectory)" : "");
    printf("\tNumber of Bits in RainerScript integers: 64\n");
    printf("\nSee https://www.rsyslog.com for more information.\n");
}

static rsRetVal rsyslogd_InitStdRatelimiters(void) {
    DEFiRet;
    CHKiRet(ratelimitNew(&dflt_ratelimiter, "rsyslogd", "dflt"));
    CHKiRet(ratelimitNew(&internalMsg_ratelimiter, "rsyslogd", "internal_messages"));
    ratelimitSetThreadSafe(internalMsg_ratelimiter);
    ratelimitSetLinuxLike(internalMsg_ratelimiter, loadConf->globals.intMsgRateLimitItv,
                          loadConf->globals.intMsgRateLimitBurst);
    /* TODO: make internalMsg ratelimit settings configurable */
finalize_it:
    RETiRet;
}


/* Method to initialize all global classes and use the objects that we need.
 * rgerhards, 2008-01-04
 * rgerhards, 2008-04-16: the actual initialization is now carried out by the runtime
 */
static rsRetVal rsyslogd_InitGlobalClasses(void) {
    DEFiRet;
    const char *pErrObj; /* tells us which object failed if that happens (useful for troubleshooting!) */

    /* Initialize the runtime system */
    pErrObj = "rsyslog runtime"; /* set in case the runtime errors before setting an object */
    CHKiRet(rsrtInit(&pErrObj, &obj));
    rsrtSetErrLogger(rsyslogd_submitErrMsg);

    /* Now tell the system which classes we need ourselves */
    pErrObj = "glbl";
    CHKiRet(objUse(glbl, CORE_COMPONENT));
    pErrObj = "module";
    CHKiRet(objUse(module, CORE_COMPONENT));
    pErrObj = "datetime";
    CHKiRet(objUse(datetime, CORE_COMPONENT));
    pErrObj = "ruleset";
    CHKiRet(objUse(ruleset, CORE_COMPONENT));
    pErrObj = "prop";
    CHKiRet(objUse(prop, CORE_COMPONENT));
    pErrObj = "parser";
    CHKiRet(objUse(parser, CORE_COMPONENT));
    pErrObj = "rsconf";
    CHKiRet(objUse(rsconf, CORE_COMPONENT));

    /* initialize some dummy classes that are not part of the runtime */
    pErrObj = "action";
    CHKiRet(actionClassInit());
    pErrObj = "template";
    CHKiRet(templateInit());

    /* TODO: the dependency on net shall go away! -- rgerhards, 2008-03-07 */
    pErrObj = "net";
    CHKiRet(objUse(net, LM_NET_FILENAME));

    dnscacheInit();
    initRainerscript();
    ratelimitModInit();

    /* we need to create the inputName property (only once during our lifetime) */
    CHKiRet(prop.Construct(&pInternalInputName));
    CHKiRet(prop.SetString(pInternalInputName, UCHAR_CONSTANT("rsyslogd"), sizeof("rsyslogd") - 1));
    CHKiRet(prop.ConstructFinalize(pInternalInputName));

finalize_it:
    if (iRet != RS_RET_OK) {
        /* we know we are inside the init sequence, so we can safely emit
         * messages to stderr. -- rgerhards, 2008-04-02
         */
        fprintf(stderr, "Error during class init for object '%s' - failing...\n", pErrObj);
        fprintf(stderr,
                "rsyslogd initialization failed - global classes could not be initialized.\n"
                "Did you do a \"make install\"?\n"
                "Suggested action: run rsyslogd with -d -n options to see what exactly "
                "fails.\n");
    }

    RETiRet;
}

/* preprocess a batch of messages, that is ready them for actual processing. This is done
 * as a first stage and totally in parallel to any other worker active in the system. So
 * it helps us keep up the overall concurrency level.
 * rgerhards, 2010-06-09
 */
static rsRetVal preprocessBatch(batch_t *pBatch, int *pbShutdownImmediate) {
    prop_t *ip;
    prop_t *fqdn;
    prop_t *localName;
    int bIsPermitted;
    smsg_t *pMsg;
    int i;
    rsRetVal localRet;
    DEFiRet;

    for (i = 0; i < pBatch->nElem && !*pbShutdownImmediate; i++) {
        pMsg = pBatch->pElem[i].pMsg;
        if ((pMsg->msgFlags & NEEDS_ACLCHK_U) != 0) {
            DBGPRINTF("msgConsumer: UDP ACL must be checked for message (hostname-based)\n");
            if (net.cvthname(pMsg->rcvFrom.pfrominet, &localName, &fqdn, &ip) != RS_RET_OK) continue;
            bIsPermitted = net.isAllowedSender2((uchar *)"UDP", (struct sockaddr *)pMsg->rcvFrom.pfrominet,
                                                (char *)propGetSzStr(fqdn), 1);
            if (!bIsPermitted) {
                DBGPRINTF("Message from '%s' discarded, not a permitted sender host\n", propGetSzStr(fqdn));
                pBatch->eltState[i] = BATCH_STATE_DISC;
            } else {
                /* save some of the info we obtained */
                MsgSetRcvFrom(pMsg, localName);
                CHKiRet(MsgSetRcvFromIP(pMsg, ip));
                pMsg->msgFlags &= ~NEEDS_ACLCHK_U;
            }
        }
        if ((pMsg->msgFlags & NEEDS_PARSING) != 0) {
            if ((localRet = parser.ParseMsg(pMsg)) != RS_RET_OK) {
                DBGPRINTF("Message discarded, parsing error %d\n", localRet);
                pBatch->eltState[i] = BATCH_STATE_DISC;
            }
        }
    }

finalize_it:
    RETiRet;
}


/* The consumer of dequeued messages. This function is called by the
 * queue engine on dequeueing of a message. It runs on a SEPARATE
 * THREAD. It receives an array of pointers, which it must iterate
 * over. We do not do any further batching, as this is of no benefit
 * for the main queue.
 */
static rsRetVal msgConsumer(void __attribute__((unused)) * notNeeded, batch_t *pBatch, wti_t *pWti) {
    DEFiRet;
    assert(pBatch != NULL);
    preprocessBatch(pBatch, pWti->pbShutdownImmediate);
    ruleset.ProcessBatch(pBatch, pWti);
    // TODO: the BATCH_STATE_COMM must be set somewhere down the road, but we
    // do not have this yet and so we emulate -- 2010-06-10
    int i;
    for (i = 0; i < pBatch->nElem && !*pWti->pbShutdownImmediate; i++) {
        pBatch->eltState[i] = BATCH_STATE_COMM;
    }
    RETiRet;
}


/* create a main message queue, now also used for ruleset queues. This function
 * needs to be moved to some other module, but it is considered acceptable for
 * the time being (remember that we want to restructure config processing at large!).
 * rgerhards, 2009-10-27
 */
rsRetVal createMainQueue(qqueue_t **ppQueue, uchar *pszQueueName, struct nvlst *lst) {
    struct queuefilenames_s *qfn;
    uchar *qfname = NULL;
    static int qfn_renamenum = 0;
    uchar qfrenamebuf[1024];
    DEFiRet;

    /* create message queue */
    CHKiRet_Hdlr(qqueueConstruct(ppQueue, ourConf->globals.mainQ.MainMsgQueType,
                                 ourConf->globals.mainQ.iMainMsgQueueNumWorkers,
                                 ourConf->globals.mainQ.iMainMsgQueueSize, msgConsumer)) {
        /* no queue is fatal, we need to give up in that case... */
        LogError(0, iRet, "could not create (ruleset) main message queue");
    }
    /* name our main queue object (it's not fatal if it fails...) */
    obj.SetName((obj_t *)(*ppQueue), pszQueueName);

    if (lst == NULL) { /* use legacy parameters? */
        /* ... set some properties ... */
#define setQPROP(func, directive, data)          \
    CHKiRet_Hdlr(func(*ppQueue, data)) {         \
        LogError(0, NO_ERRCODE,                  \
                 "Invalid " #directive           \
                 ", error %d. Ignored, "         \
                 "running with default setting", \
                 iRet);                          \
    }
#define setQPROPstr(func, directive, data)                                          \
    CHKiRet_Hdlr(func(*ppQueue, data, (data == NULL) ? 0 : strlen((char *)data))) { \
        LogError(0, NO_ERRCODE,                                                     \
                 "Invalid " #directive                                              \
                 ", error %d. Ignored, "                                            \
                 "running with default setting",                                    \
                 iRet);                                                             \
    }

        if (ourConf->globals.mainQ.pszMainMsgQFName != NULL) {
            /* check if the queue file name is unique, else emit an error */
            for (qfn = queuefilenames; qfn != NULL; qfn = qfn->next) {
                dbgprintf("check queue file name '%s' vs '%s'\n", qfn->name, ourConf->globals.mainQ.pszMainMsgQFName);
                if (!ustrcmp(qfn->name, ourConf->globals.mainQ.pszMainMsgQFName)) {
                    snprintf((char *)qfrenamebuf, sizeof(qfrenamebuf), "%d-%s-%s", ++qfn_renamenum,
                             ourConf->globals.mainQ.pszMainMsgQFName,
                             (pszQueueName == NULL) ? "NONAME" : (char *)pszQueueName);
                    qfname = ustrdup(qfrenamebuf);
                    LogError(0, NO_ERRCODE,
                             "Error: queue file name '%s' already in use "
                             " - using '%s' instead",
                             ourConf->globals.mainQ.pszMainMsgQFName, qfname);
                    break;
                }
            }
            if (qfname == NULL) qfname = ustrdup(ourConf->globals.mainQ.pszMainMsgQFName);
            qfn = malloc(sizeof(struct queuefilenames_s));
            qfn->name = qfname;
            qfn->next = queuefilenames;
            queuefilenames = qfn;
        }

        setQPROP(qqueueSetMaxFileSize, "$MainMsgQueueFileSize", ourConf->globals.mainQ.iMainMsgQueMaxFileSize);
        setQPROP(qqueueSetsizeOnDiskMax, "$MainMsgQueueMaxDiskSpace", ourConf->globals.mainQ.iMainMsgQueMaxDiskSpace);
        setQPROP(qqueueSetiDeqBatchSize, "$MainMsgQueueDequeueBatchSize",
                 ourConf->globals.mainQ.iMainMsgQueDeqBatchSize);
        setQPROPstr(qqueueSetFilePrefix, "$MainMsgQueueFileName", qfname);
        setQPROP(qqueueSetiPersistUpdCnt, "$MainMsgQueueCheckpointInterval",
                 ourConf->globals.mainQ.iMainMsgQPersistUpdCnt);
        setQPROP(qqueueSetbSyncQueueFiles, "$MainMsgQueueSyncQueueFiles",
                 ourConf->globals.mainQ.bMainMsgQSyncQeueFiles);
        setQPROP(qqueueSettoQShutdown, "$MainMsgQueueTimeoutShutdown", ourConf->globals.mainQ.iMainMsgQtoQShutdown);
        setQPROP(qqueueSettoActShutdown, "$MainMsgQueueTimeoutActionCompletion",
                 ourConf->globals.mainQ.iMainMsgQtoActShutdown);
        setQPROP(qqueueSettoWrkShutdown, "$MainMsgQueueWorkerTimeoutThreadShutdown",
                 ourConf->globals.mainQ.iMainMsgQtoWrkShutdown);
        setQPROP(qqueueSettoEnq, "$MainMsgQueueTimeoutEnqueue", ourConf->globals.mainQ.iMainMsgQtoEnq);
        setQPROP(qqueueSetiHighWtrMrk, "$MainMsgQueueHighWaterMark", ourConf->globals.mainQ.iMainMsgQHighWtrMark);
        setQPROP(qqueueSetiLowWtrMrk, "$MainMsgQueueLowWaterMark", ourConf->globals.mainQ.iMainMsgQLowWtrMark);
        setQPROP(qqueueSetiDiscardMrk, "$MainMsgQueueDiscardMark", ourConf->globals.mainQ.iMainMsgQDiscardMark);
        setQPROP(qqueueSetiDiscardSeverity, "$MainMsgQueueDiscardSeverity",
                 ourConf->globals.mainQ.iMainMsgQDiscardSeverity);
        setQPROP(qqueueSetiMinMsgsPerWrkr, "$MainMsgQueueWorkerThreadMinimumMessages",
                 ourConf->globals.mainQ.iMainMsgQWrkMinMsgs);
        setQPROP(qqueueSetbSaveOnShutdown, "$MainMsgQueueSaveOnShutdown",
                 ourConf->globals.mainQ.bMainMsgQSaveOnShutdown);
        setQPROP(qqueueSetiDeqSlowdown, "$MainMsgQueueDequeueSlowdown", ourConf->globals.mainQ.iMainMsgQDeqSlowdown);
        setQPROP(qqueueSetiDeqtWinFromHr, "$MainMsgQueueDequeueTimeBegin",
                 ourConf->globals.mainQ.iMainMsgQueueDeqtWinFromHr);
        setQPROP(qqueueSetiDeqtWinToHr, "$MainMsgQueueDequeueTimeEnd", ourConf->globals.mainQ.iMainMsgQueueDeqtWinToHr);

#undef setQPROP
#undef setQPROPstr
    } else { /* use new style config! */
        qqueueSetDefaultsRulesetQueue(*ppQueue);
        qqueueApplyCnfParam(*ppQueue, lst);
    }
    qqueueCorrectParams(*ppQueue);

    RETiRet;
}

rsRetVal startMainQueue(rsconf_t *cnf, qqueue_t *const pQueue) {
    DEFiRet;
    CHKiRet_Hdlr(qqueueStart(cnf, pQueue)) {
        /* no queue is fatal, we need to give up in that case... */
        LogError(0, iRet, "could not start (ruleset) main message queue");
        if (runConf->globals.bAbortOnFailedQueueStartup) {
            fprintf(stderr,
                    "rsyslogd: could not start (ruleset) main message queue, "
                    "abortOnFailedQueueStartup is set, so we abort rsyslog now.\n");
            fflush(stderr);
            clearPidFile();
            exit(1); /* "good" exit, this is intended here */
        }
        pQueue->qType = QUEUETYPE_DIRECT;
        CHKiRet_Hdlr(qqueueStart(cnf, pQueue)) {
            /* no queue is fatal, we need to give up in that case... */
            LogError(0, iRet, "fatal error: could not even start queue in direct mode");
        }
    }
    RETiRet;
}


/* this is a special function used to submit an error message. This
 * function is also passed to the runtime library as the generic error
 * message handler. -- rgerhards, 2008-04-17
 */
void rsyslogd_submitErrMsg(const int severity, const int iErr, const uchar *msg) {
    if (glbl.GetGlobalInputTermState() == 1) {
        /* After fork the stderr is unusable (dfltErrLogger uses is internally) */
        if (!doFork) dfltErrLogger(severity, iErr, msg);
    } else {
        logmsgInternal(iErr, LOG_SYSLOG | (severity & 0x07), msg, 0);
    }
}

static inline rsRetVal submitMsgWithDfltRatelimiter(smsg_t *pMsg) {
    return ratelimitAddMsg(dflt_ratelimiter, NULL, pMsg);
}


static void logmsgInternal_doWrite(smsg_t *pMsg) {
    const int pri = getPRIi(pMsg);
    if (pri % 8 <= runConf->globals.intMsgsSeverityFilter) {
        if (runConf->globals.bProcessInternalMessages) {
            submitMsg2(pMsg);
            pMsg = NULL; /* msg obj handed over; do not destruct */
        } else {
            uchar *const msg = getMSG(pMsg);
#ifdef ENABLE_LIBLOGGING_STDLOG
            /* the "emit only once" rate limiter is quick and dirty and not
             * thread safe. However, that's no problem for the current intend
             * and it is not justified to create more robust code for the
             * functionality. -- rgerhards, 2018-05-14
             */
            static warnmsg_emitted = 0;
            if (warnmsg_emitted == 0) {
                stdlog_log(runConf->globals.stdlog_hdl, LOG_WARNING, "%s",
                           "RSYSLOG WARNING: liblogging-stdlog "
                           "functionality will go away soon. For details see "
                           "https://github.com/rsyslog/rsyslog/issues/2706");
                warnmsg_emitted = 1;
            }
            stdlog_log(runConf->globals.stdlog_hdl, pri2sev(pri), "%s", (char *)msg);
#else
            syslog(pri, "%s", msg);
#endif
        }
    }
    if (pMsg != NULL) {
        msgDestruct(&pMsg);
    }
}

/* This function creates a log message object out of the provided
 * message text and forwards it for logging.
 */
static rsRetVal logmsgInternalSubmit(
    const int iErr, const syslog_pri_t pri, const size_t lenMsg, const char *__restrict__ const msg, int flags) {
    uchar pszTag[33];
    smsg_t *pMsg;
    DEFiRet;

    if (glblAbortOnProgramError && iErr == RS_RET_PROGRAM_ERROR) {
        fprintf(stderr,
                "\n\n\n========================================\n"
                "rsyslog reports program error: %s\n"
                "rsyslog is configured to abort in this case, "
                "this will be done now\n",
                msg);
        fflush(stdout);
        abort();
    }

    CHKiRet(msgConstruct(&pMsg));
    MsgSetInputName(pMsg, pInternalInputName);
    MsgSetRawMsg(pMsg, (char *)msg, lenMsg);
    MsgSetHOSTNAME(pMsg, glbl.GetLocalHostName(), ustrlen(glbl.GetLocalHostName()));
    MsgSetRcvFrom(pMsg, glbl.GetLocalHostNameProp());
    MsgSetRcvFromIP(pMsg, glbl.GetLocalHostIP());
    MsgSetMSGoffs(pMsg, 0);
    /* check if we have an error code associated and, if so,
     * adjust the tag. -- rgerhards, 2008-06-27
     */
    if (iErr == NO_ERRCODE) {
        MsgSetTAG(pMsg, UCHAR_CONSTANT("rsyslogd:"), sizeof("rsyslogd:") - 1);
    } else {
        size_t len = snprintf((char *)pszTag, sizeof(pszTag), "rsyslogd%d:", iErr);
        pszTag[32] = '\0'; /* just to make sure... */
        MsgSetTAG(pMsg, pszTag, len);
    }

    flags |= INTERNAL_MSG;
    pMsg->msgFlags = flags;
    msgSetPRI(pMsg, pri);

    iminternalAddMsg(pMsg);
finalize_it:
    RETiRet;
}


/* rgerhards 2004-11-09: the following is a function that can be used
 * to log a message originating from the syslogd itself.
 */
rsRetVal logmsgInternal(int iErr, const syslog_pri_t pri, const uchar *const msg, int flags) {
    size_t lenMsg;
    unsigned i;
    char *bufModMsg = NULL; /* buffer for modified message, should we need to modify */
    DEFiRet;

    /* we first do a path the remove control characters that may have accidentally
     * introduced (program error!). This costs performance, but we do not expect
     * to be called very frequently in any case ;) -- rgerhards, 2013-12-19.
     */
    lenMsg = ustrlen(msg);
    for (i = 0; i < lenMsg; ++i) {
        if (msg[i] < 0x20 || msg[i] == 0x7f) {
            if (bufModMsg == NULL) {
                CHKmalloc(bufModMsg = strdup((char *)msg));
            }
            bufModMsg[i] = ' ';
        }
    }

    CHKiRet(logmsgInternalSubmit(iErr, pri, lenMsg, (bufModMsg == NULL) ? (char *)msg : bufModMsg, flags));

    /* we now check if we should print internal messages out to stderr. This was
     * suggested by HKS as a way to help people troubleshoot rsyslog configuration
     * (by running it interactively. This makes an awful lot of sense, so I add
     * it here. -- rgerhards, 2008-07-28
     * Note that error messages can not be disabled during a config verify. This
     * permits us to process unmodified config files which otherwise contain a
     * suppressor statement.
     */
    int emit_to_stderr = (ourConf == NULL) ? 1 : (ourConf->globals.bErrMsgToStderr || ourConf->globals.bAllMsgToStderr);
    int emit_supress_msg = 0;
    if (Debug == DEBUG_FULL || !doFork) {
        emit_to_stderr = 1;
    }
    if (ourConf != NULL && ourConf->globals.maxErrMsgToStderr != -1) {
        if (emit_to_stderr && ourConf->globals.maxErrMsgToStderr != -1 && ourConf->globals.maxErrMsgToStderr) {
            --ourConf->globals.maxErrMsgToStderr;
            if (ourConf->globals.maxErrMsgToStderr == 0) emit_supress_msg = 1;
        } else {
            emit_to_stderr = 0;
        }
    }
    if (emit_to_stderr || iConfigVerify) {
        if ((ourConf != NULL && ourConf->globals.bAllMsgToStderr) || pri2sev(pri) == LOG_ERR)
            fprintf(stderr, "rsyslogd: %s\n", (bufModMsg == NULL) ? (char *)msg : bufModMsg);
    }
    if (emit_supress_msg) {
        fprintf(stderr,
                "rsyslogd: configured max number of error messages "
                "to stderr reached, further messages will not be output\n"
                "Consider adjusting\n"
                "    global(errorMessagesToStderr.maxNumber=\"xx\")\n"
                "if you want more.\n");
    }

finalize_it:
    free(bufModMsg);
    RETiRet;
}

rsRetVal submitMsg(smsg_t *pMsg) {
    return submitMsgWithDfltRatelimiter(pMsg);
}


static rsRetVal ATTR_NONNULL() splitOversizeMessage(smsg_t *const pMsg) {
    DEFiRet;
    const char *rawmsg;
    int nsegments;
    int len_rawmsg;
    const int maxlen = glblGetMaxLine(runConf);
    ISOBJ_TYPE_assert(pMsg, msg);

    getRawMsg(pMsg, (uchar **)&rawmsg, &len_rawmsg);
    nsegments = len_rawmsg / maxlen;
    const int len_last_segment = len_rawmsg % maxlen;
    DBGPRINTF(
        "splitting oversize message, size %d, segment size %d, "
        "nsegments %d, bytes in last fragment %d\n",
        len_rawmsg, maxlen, nsegments, len_last_segment);

    smsg_t *pMsg_seg;

    /* process full segments */
    for (int i = 0; i < nsegments; ++i) {
        CHKmalloc(pMsg_seg = MsgDup(pMsg));
        MsgSetRawMsg(pMsg_seg, rawmsg + (i * maxlen), maxlen);
        submitMsg2(pMsg_seg);
    }

    /* if necessary, write partial last segment */
    if (len_last_segment != 0) {
        CHKmalloc(pMsg_seg = MsgDup(pMsg));
        MsgSetRawMsg(pMsg_seg, rawmsg + (nsegments * maxlen), len_last_segment);
        submitMsg2(pMsg_seg);
    }

finalize_it:
    RETiRet;
}


/* submit a message to the main message queue.   This is primarily
 * a hook to prevent the need for callers to know about the main message queue
 * rgerhards, 2008-02-13
 */
rsRetVal submitMsg2(smsg_t *pMsg) {
    qqueue_t *pQueue;
    ruleset_t *pRuleset;
    DEFiRet;

    ISOBJ_TYPE_assert(pMsg, msg);

    if (getRawMsgLen(pMsg) > glblGetMaxLine(runConf)) {
        uchar *rawmsg;
        int dummy;
        getRawMsg(pMsg, &rawmsg, &dummy);
        if (glblReportOversizeMessage(runConf)) {
            LogMsg(0, RS_RET_OVERSIZE_MSG, LOG_WARNING,
                   "message too long (%d) with configured size %d, begin of "
                   "message is: %.80s",
                   getRawMsgLen(pMsg), glblGetMaxLine(runConf), rawmsg);
        }
        writeOversizeMessageLog(pMsg);
        if (glblGetOversizeMsgInputMode(runConf) == glblOversizeMsgInputMode_Split) {
            splitOversizeMessage(pMsg);
            /* we have submitted the message segments recursively, so we
             * can just deleted the original msg object and terminate.
             */
            msgDestruct(&pMsg);
            FINALIZE;
        } else if (glblGetOversizeMsgInputMode(runConf) == glblOversizeMsgInputMode_Truncate) {
            MsgTruncateToMaxSize(pMsg);
        } else {
            /* in "accept" mode, we do nothing, simply because "accept" means
             * to use as-is.
             */
            assert(glblGetOversizeMsgInputMode(runConf) == glblOversizeMsgInputMode_Accept);
        }
    }

    pRuleset = MsgGetRuleset(pMsg);
    assert(ruleset.GetRulesetQueue != NULL); /* This is only to keep clang static analyzer happy */
    pQueue = (pRuleset == NULL) ? runConf->pMsgQueue : ruleset.GetRulesetQueue(pRuleset);

    /* if a plugin logs a message during shutdown, the queue may no longer exist */
    if (pQueue == NULL) {
        DBGPRINTF(
            "submitMsg2() could not submit message - "
            "queue does (no longer?) exist - ignored\n");
        FINALIZE;
    }

    qqueueEnqMsg(pQueue, pMsg->flowCtlType, pMsg);

finalize_it:
    RETiRet;
}

/* submit multiple messages at once, very similar to submitMsg, just
 * for multi_submit_t. All messages need to go into the SAME queue!
 * rgerhards, 2009-06-16
 */
rsRetVal ATTR_NONNULL() multiSubmitMsg2(multi_submit_t *const pMultiSub) {
    qqueue_t *pQueue;
    ruleset_t *pRuleset;
    DEFiRet;

    if (pMultiSub->nElem == 0) FINALIZE;

    pRuleset = MsgGetRuleset(pMultiSub->ppMsgs[0]);
    pQueue = (pRuleset == NULL) ? runConf->pMsgQueue : ruleset.GetRulesetQueue(pRuleset);

    /* if a plugin logs a message during shutdown, the queue may no longer exist */
    if (pQueue == NULL) {
        DBGPRINTF(
            "multiSubmitMsg() could not submit message - "
            "queue does (no longer?) exist - ignored\n");
        FINALIZE;
    }

    iRet = pQueue->MultiEnq(pQueue, pMultiSub);
    pMultiSub->nElem = 0;

finalize_it:
    RETiRet;
}
rsRetVal multiSubmitMsg(multi_submit_t *pMultiSub) /* backward compat. level */
{
    return multiSubmitMsg2(pMultiSub);
}


/* flush multiSubmit, e.g. at end of read records */
rsRetVal multiSubmitFlush(multi_submit_t *pMultiSub) {
    DEFiRet;
    if (pMultiSub->nElem > 0) {
        iRet = multiSubmitMsg2(pMultiSub);
    }
    RETiRet;
}


/* some support for command line option parsing. Any non-trivial options must be
 * buffered until the complete command line has been parsed. This is necessary to
 * prevent dependencies between the options. That, in turn, means we need to have
 * something that is capable of buffering options and there values. The following
 * functions handle that.
 * rgerhards, 2008-04-04
 */
typedef struct bufOpt {
    struct bufOpt *pNext;
    char optchar;
    char *arg;
} bufOpt_t;
static bufOpt_t *bufOptRoot = NULL;
static bufOpt_t *bufOptLast = NULL;

/* add option buffer */
static rsRetVal bufOptAdd(char opt, char *arg) {
    DEFiRet;
    bufOpt_t *pBuf;

    if ((pBuf = malloc(sizeof(bufOpt_t))) == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

    pBuf->optchar = opt;
    pBuf->arg = arg;
    pBuf->pNext = NULL;

    if (bufOptLast == NULL) {
        bufOptRoot = pBuf; /* then there is also no root! */
    } else {
        bufOptLast->pNext = pBuf;
    }
    bufOptLast = pBuf;

finalize_it:
    RETiRet;
}


/* remove option buffer from top of list, return values and destruct buffer itself.
 * returns RS_RET_END_OF_LINKEDLIST when no more options are present.
 * (we use int *opt instead of char *opt to keep consistent with getopt())
 */
static rsRetVal bufOptRemove(int *opt, char **arg) {
    DEFiRet;
    bufOpt_t *pBuf;

    if (bufOptRoot == NULL) ABORT_FINALIZE(RS_RET_END_OF_LINKEDLIST);
    pBuf = bufOptRoot;

    *opt = pBuf->optchar;
    *arg = pBuf->arg;

    bufOptRoot = pBuf->pNext;
    free(pBuf);

finalize_it:
    RETiRet;
}


static void hdlr_sigttin_ou(void) {
    /* this is just a dummy to care for our sigttin input
     * module cancel interface and sigttou internal message
     * notification/mainloop wakeup mechanism. The important
     * point is that it actually does *NOTHING*.
     */
}

static void hdlr_enable(int sig, void (*hdlr)()) {
    struct sigaction sigAct;
    memset(&sigAct, 0, sizeof(sigAct));
    sigemptyset(&sigAct.sa_mask);
    sigAct.sa_handler = hdlr;
    sigaction(sig, &sigAct, NULL);
}

static void hdlr_sighup(void) {
    pthread_mutex_lock(&mutHadHUP);
    bHadHUP = 1;
    pthread_mutex_unlock(&mutHadHUP);
    /* at least on FreeBSD we seem not to necessarily awake the main thread.
     * So let's do it explicitly.
     */
    dbgprintf("awaking mainthread on HUP\n");
    pthread_kill(mainthread, SIGTTIN);
}

static void hdlr_sigchld(void) {
    pthread_mutex_lock(&mutChildDied);
    bChildDied = 1;
    pthread_mutex_unlock(&mutChildDied);
}

static void rsyslogdDebugSwitch(void) {
    time_t tTime;
    struct tm tp;

    datetime.GetTime(&tTime);
    localtime_r(&tTime, &tp);
    if (debugging_on == 0) {
        debugging_on = 1;
        dbgprintf("\n");
        dbgprintf("\n");
        dbgprintf("********************************************************************************\n");
        dbgprintf("Switching debugging_on to true at %2.2d:%2.2d:%2.2d\n", tp.tm_hour, tp.tm_min, tp.tm_sec);
        dbgprintf("********************************************************************************\n");
    } else {
        dbgprintf("********************************************************************************\n");
        dbgprintf("Switching debugging_on to false at %2.2d:%2.2d:%2.2d\n", tp.tm_hour, tp.tm_min, tp.tm_sec);
        dbgprintf("********************************************************************************\n");
        dbgprintf("\n");
        dbgprintf("\n");
        debugging_on = 0;
    }
}


/* This is the main entry point into rsyslogd. Over time, we should try to
 * modularize it a bit more...
 *
 * NOTE on stderr and stdout: they are kept open during a fork. Note that this
 * may introduce subtle security issues: if we are in a jail, one may break out of
 * it via these descriptors. But if I close them earlier, error messages will (once
 * again) not be emitted to the user that starts the daemon. Given that the risk
 * of a break-in is very low in the startup phase, we decide it is more important
 * to emit error messages.
 */
static void initAll(int argc, char **argv) {
    rsRetVal localRet;
    int ch;
    int iHelperUOpt;
    int bChDirRoot = 1; /* change the current working directory to "/"? */
    char *arg; /* for command line option processing */
    char cwdbuf[128]; /* buffer to obtain/display current working directory */
    int parentPipeFD = 0; /* fd of pipe to parent, if auto-backgrounding */
    DEFiRet;

    /* prepare internal signaling */
    hdlr_enable(SIGTTIN, hdlr_sigttin_ou);
    hdlr_enable(SIGTTOU, hdlr_sigttin_ou);

    /* first, parse the command line options. We do not carry out any actual work, just
     * see what we should do. This relieves us from certain anomalies and we can process
     * the parameters down below in the correct order. For example, we must know the
     * value of -M before we can do the init, but at the same time we need to have
     * the base classes init before we can process most of the options. Now, with the
     * split of functionality, this is no longer a problem. Thanks to varmofekoj for
     * suggesting this algo.
     * Note: where we just need to set some flags and can do so without knowledge
     * of other options, we do this during the initial option processing.
     * rgerhards, 2008-04-04
     */
#if defined(_AIX)
    while ((ch = getopt(argc, argv, "46ACDdf:hi:M:nN:o:qQS:T:u:vwxR")) != EOF) {
#else
    while ((ch = getopt(argc, argv, "46ACDdf:hi:M:nN:o:qQS:T:u:vwx")) != EOF) {
#endif
        switch ((char)ch) {
            case '4':
            case '6':
            case 'A':
            case 'f': /* configuration file */
            case 'i': /* pid file name */
            case 'n': /* don't fork */
            case 'N': /* enable config verify mode */
            case 'q': /* add hostname if DNS resolving has failed */
            case 'Q': /* dont resolve hostnames in ACL to IPs */
            case 'S': /* Source IP for local client to be used on multihomed host */
            case 'T': /* chroot on startup (primarily for testing) */
            case 'u': /* misc user settings */
            case 'w': /* disable disallowed host warnings */
            case 'C':
            case 'o': /* write output config file */
            case 'x': /* disable dns for remote messages */
                CHKiRet(bufOptAdd(ch, optarg));
                break;
#if defined(_AIX)
            case 'R': /* This option is a no-op for AIX */
                break;
#endif
            case 'd': /* debug - must be handled now, so that debug is active during init! */
                debugging_on = 1;
                Debug = 1;
                yydebug = 1;
                break;
            case 'D': /* BISON debug */
                yydebug = 1;
                break;
            case 'M': /* default module load path -- this MUST be carried out immediately! */
                glblModPath = (uchar *)optarg;
                break;
            case 'v': /* MUST be carried out immediately! */
                printVersion();
                exit(0); /* exit for -v option - so this is a "good one" */
            case 'h':
            case '?':
            default:
                rsyslogd_usage();
        }
    }

    if (argc - optind) rsyslogd_usage();

    DBGPRINTF("rsyslogd %s startup, module path '%s', cwd:%s\n", VERSION,
              glblModPath == NULL ? "" : (char *)glblModPath, getcwd(cwdbuf, sizeof(cwdbuf)));

    /* we are done with the initial option parsing and processing. Now we init the system. */

    CHKiRet(rsyslogd_InitGlobalClasses());

    /* doing some core initializations */

    if ((iRet = modInitIminternal()) != RS_RET_OK) {
        fprintf(stderr, "fatal error: could not initialize errbuf object (error code %d).\n", iRet);
        exit(1); /* "good" exit, leaving at init for fatal error */
    }

    /* we now can emit error messages "the regular way" */

    if (getenv("TZ") == NULL) {
        const char *const tz = (access("/etc/localtime", R_OK) == 0) ? "TZ=/etc/localtime" : "TZ=UTC";
        putenv((char *)tz);
        if (emitTZWarning) {
            LogMsg(0, RS_RET_NO_TZ_SET, LOG_WARNING,
                   "environment variable TZ is not "
                   "set, auto correcting this to %s",
                   tz);
        } else {
            dbgprintf("environment variable TZ is not set, auto correcting this to %s\n", tz);
        }
    }

    /* END core initializations - we now come back to carrying out command line options*/

    while ((iRet = bufOptRemove(&ch, &arg)) == RS_RET_OK) {
        DBGPRINTF("deque option %c, optarg '%s'\n", ch, (arg == NULL) ? "" : arg);
        switch ((char)ch) {
            case '4':
                fprintf(stderr,
                        "rsyslogd: the -4 command line option has gone away.\n"
                        "Please use the global(net.ipprotocol=\"ipv4-only\") "
                        "configuration parameter instead.\n");
                break;
            case '6':
                fprintf(stderr,
                        "rsyslogd: the -6 command line option will has gone away.\n"
                        "Please use the global(net.ipprotocol=\"ipv6-only\") "
                        "configuration parameter instead.\n");
                break;
            case 'A':
                fprintf(stderr,
                        "rsyslogd: the -A command line option will go away "
                        "soon.\n"
                        "Please use the omfwd parameter \"upd.sendToAll\" instead.\n");
                send_to_all++;
                break;
            case 'S': /* Source IP for local client to be used on multihomed host */
                fprintf(stderr,
                        "rsyslogd: the -S command line option will go away "
                        "soon.\n"
                        "Please use the omrelp parameter \"localClientIP\" instead.\n");
                if (glbl.GetSourceIPofLocalClient() != NULL) {
                    fprintf(stderr, "rsyslogd: Only one -S argument allowed, the first one is taken.\n");
                } else {
                    glbl.SetSourceIPofLocalClient((uchar *)arg);
                }
                break;
            case 'f': /* configuration file */
                ConfFile = (uchar *)arg;
                break;
            case 'i': /* pid file name */
                free((void *)PidFile);
                PidFile = arg;
                break;
            case 'n': /* don't fork */
                doFork = 0;
                break;
            case 'N': /* enable config verify mode */
                iConfigVerify = (arg == NULL) ? 0 : atoi(arg);
                break;
            case 'o':
                if (fp_rs_full_conf_output != NULL) {
                    fprintf(stderr,
                            "warning: -o option given multiple times. Now "
                            "using value %s\n",
                            (arg == NULL) ? "-" : arg);
                    fclose(fp_rs_full_conf_output);
                    fp_rs_full_conf_output = NULL;
                }
                if (arg == NULL || !strcmp(arg, "-")) {
                    fp_rs_full_conf_output = stdout;
                } else {
                    fp_rs_full_conf_output = fopen(arg, "w");
                }
                if (fp_rs_full_conf_output == NULL) {
                    perror(arg);
                    fprintf(stderr,
                            "rsyslogd: cannot open config output file %s - "
                            "-o option will be ignored\n",
                            arg);
                } else {
                    time_t tTime;
                    struct tm tp;
                    datetime.GetTime(&tTime);
                    localtime_r(&tTime, &tp);
                    fprintf(fp_rs_full_conf_output,
                            "## full conf created by rsyslog version %s at "
                            "%4.4d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d ##\n",
                            VERSION, tp.tm_year + 1900, tp.tm_mon + 1, tp.tm_mday, tp.tm_hour, tp.tm_min, tp.tm_sec);
                }
                break;
            case 'q': /* add hostname if DNS resolving has failed */
                fprintf(stderr,
                        "rsyslogd: the -q command line option has gone away.\n"
                        "Please use the global(net.aclAddHostnameOnFail=\"on\") "
                        "configuration parameter instead.\n");
                break;
            case 'Q': /* dont resolve hostnames in ACL to IPs */
                fprintf(stderr,
                        "rsyslogd: the -Q command line option has gone away.\n"
                        "Please use the global(net.aclResolveHostname=\"off\") "
                        "configuration parameter instead.\n");
                break;
            case 'T': /* chroot() immediately at program startup, but only for testing, NOT security yet */
                if (arg == NULL) {
                    /* note this case should already be handled by getopt,
                     * but we want to keep the static analyzer happy.
                     */
                    fprintf(stderr, "-T options needs a parameter\n");
                    exit(1);
                }
                if (chroot(arg) != 0) {
                    perror("chroot");
                    exit(1);
                }
                if (chdir("/") != 0) {
                    perror("chdir");
                    exit(1);
                }
                break;
            case 'u': /* misc user settings */
                iHelperUOpt = (arg == NULL) ? 0 : atoi(arg);
                if (iHelperUOpt & 0x01) {
                    fprintf(stderr,
                            "rsyslogd: the -u command line option has gone away.\n"
                            "For the 0x01 bit, please use the "
                            "global(parser.parseHostnameAndTag=\"off\") "
                            "configuration parameter instead.\n");
                }
                if (iHelperUOpt & 0x02) {
                    fprintf(stderr,
                            "rsyslogd: the -u command line option will go away "
                            "soon.\n"
                            "For the 0x02 bit, please use the -C option instead.");
                    bChDirRoot = 0;
                }
                break;
            case 'C':
                bChDirRoot = 0;
                break;
            case 'w': /* disable disallowed host warnings */
                fprintf(stderr,
                        "rsyslogd: the -w command line option has gone away.\n"
                        "Please use the global(net.permitWarning=\"off\") "
                        "configuration parameter instead.\n");
                break;
            case 'x': /* disable dns for remote messages */
                fprintf(stderr,
                        "rsyslogd: the -x command line option has gone away.\n"
                        "Please use the global(net.enableDNS=\"off\") "
                        "configuration parameter instead.\n");
                break;
            case 'h':
            case '?':
            default:
                rsyslogd_usage();
        }
    }

    if (iRet != RS_RET_END_OF_LINKEDLIST) FINALIZE;

    if (iConfigVerify) {
        doFork = 0;
        fprintf(stderr, "rsyslogd: version %s, config validation run (level %d), master config %s\n", VERSION,
                iConfigVerify, ConfFile);
    }

    resetErrMsgsFlag();
    localRet = rsconf.Load(&ourConf, ConfFile);

#ifdef ENABLE_LIBCAPNG
    if (loadConf->globals.bCapabilityDropEnabled) {
        /*
         * Drop capabilities to the necessary set
         */
        int capng_rc, capng_failed = 0;
        typedef struct capabilities_s {
            int capability; /* capability code */
            const char *name; /* name of the capability to be displayed */
            /* is the capability present that is needed by rsyslog? if so we do not drop it */
            sbool present;
            capng_type_t type;
        } capabilities_t;

        // clang-format off
        capabilities_t capabilities[] = {
            { CAP_BLOCK_SUSPEND,    "CAP_BLOCK_SUSPEND",    0, CAPNG_EFFECTIVE | CAPNG_PERMITTED },
            { CAP_NET_RAW,          "CAP_NET_RAW",          0, CAPNG_EFFECTIVE | CAPNG_PERMITTED },
            { CAP_CHOWN,            "CAP_CHOWN",            0, CAPNG_EFFECTIVE | CAPNG_PERMITTED },
            { CAP_LEASE,            "CAP_LEASE",            0, CAPNG_EFFECTIVE | CAPNG_PERMITTED },
            { CAP_NET_ADMIN,        "CAP_NET_ADMIN",        0, CAPNG_EFFECTIVE | CAPNG_PERMITTED },
            { CAP_NET_BIND_SERVICE, "CAP_NET_BIND_SERVICE", 0, CAPNG_EFFECTIVE | CAPNG_PERMITTED },
            { CAP_DAC_OVERRIDE,     "CAP_DAC_OVERRIDE",     0, CAPNG_EFFECTIVE | CAPNG_PERMITTED | CAPNG_BOUNDING_SET },
            { CAP_SETGID,           "CAP_SETGID",           0, CAPNG_EFFECTIVE | CAPNG_PERMITTED },
            { CAP_SETUID,           "CAP_SETUID",           0, CAPNG_EFFECTIVE | CAPNG_PERMITTED },
            { CAP_SYS_ADMIN,        "CAP_SYS_ADMIN",        0, CAPNG_EFFECTIVE | CAPNG_PERMITTED },
            { CAP_SYS_CHROOT,       "CAP_SYS_CHROOT",       0, CAPNG_EFFECTIVE | CAPNG_PERMITTED },
            { CAP_SYS_RESOURCE,     "CAP_SYS_RESOURCE",     0, CAPNG_EFFECTIVE | CAPNG_PERMITTED },
            { CAP_SYSLOG,           "CAP_SYSLOG",           0, CAPNG_EFFECTIVE | CAPNG_PERMITTED }
        };
        // clang-format on

        if (capng_have_capabilities(CAPNG_SELECT_CAPS) > CAPNG_NONE) {
            /* Examine which capabilities are available to us, so we do not try to
                drop something that is not present. We need to do this in two steps,
                because capng_clear clears the capability set. In the second step,
                we add back those caps, which were present before clearing the selected
                posix capabilities set.
            */
            unsigned long caps_len = sizeof(capabilities) / sizeof(capabilities_t);
            for (unsigned long i = 0; i < caps_len; i++) {
                if (capng_have_capability(CAPNG_EFFECTIVE, capabilities[i].capability)) {
                    capabilities[i].present = 1;
                }
            }

            capng_clear(CAPNG_SELECT_BOTH);

            for (unsigned long i = 0; i < caps_len; i++) {
                if (capabilities[i].present) {
                    DBGPRINTF(
                        "The %s capability is present, "
                        "will try to preserve it.\n",
                        capabilities[i].name);
                    if ((capng_rc = capng_update(CAPNG_ADD, capabilities[i].type, capabilities[i].capability)) != 0) {
                        LogError(0, RS_RET_LIBCAPNG_ERR,
                                 "could not update the internal posix capabilities"
                                 " settings based on the options passed to it,"
                                 " capng_update=%d",
                                 capng_rc);
                        capng_failed = 1;
                    }
                } else {
                    DBGPRINTF(
                        "The %s capability is not present, "
                        "will not try to preserve it.\n",
                        capabilities[i].name);
                }
            }

            if ((capng_rc = capng_apply(CAPNG_SELECT_BOTH)) != 0) {
                LogError(0, RS_RET_LIBCAPNG_ERR,
                         "could not transfer the specified internal posix capabilities "
                         "settings to the kernel, capng_apply=%d",
                         capng_rc);
                capng_failed = 1;
            }

            if (capng_failed) {
                DBGPRINTF("Capabilities were not dropped successfully.\n");
                if (loadConf->globals.bAbortOnFailedLibcapngSetup) {
                    ABORT_FINALIZE(RS_RET_LIBCAPNG_ERR);
                }
            } else {
                DBGPRINTF("Capabilities were dropped successfully\n");
            }
        } else {
            DBGPRINTF("No capabilities to drop\n");
        }
    }
#endif

    if (fp_rs_full_conf_output != NULL) {
        if (fp_rs_full_conf_output != stdout) {
            fclose(fp_rs_full_conf_output);
        }
        fp_rs_full_conf_output = NULL;
    }

    /* check for "hard" errors that needs us to abort in any case */
    if ((localRet == RS_RET_CONF_FILE_NOT_FOUND) || (localRet == RS_RET_NO_ACTIONS)) {
        /* for extreme testing, we keep the ability to let rsyslog continue
         * even on hard config errors. Note that this may lead to segfaults
         * or other malfunction further down the road.
         */
        if ((loadConf->globals.glblDevOptions & DEV_OPTION_KEEP_RUNNING_ON_HARD_CONF_ERROR) == 1) {
            fprintf(stderr,
                    "rsyslogd: NOTE: developer-only option set to keep rsyslog "
                    "running where it should abort - this can lead to "
                    "more problems later in the run.\n");
        } else {
            ABORT_FINALIZE(localRet);
        }
    }

    glbl.GenerateLocalHostNameProperty();

    if (hadErrMsgs()) {
        if (loadConf->globals.bAbortOnUncleanConfig) {
            fprintf(stderr,
                    "rsyslogd: global(AbortOnUncleanConfig=\"on\") is set, and "
                    "config is not clean.\n"
                    "Check error log for details, fix errors and restart. As a last\n"
                    "resort, you may want to use global(AbortOnUncleanConfig=\"off\") \n"
                    "to permit a startup with a dirty config.\n");
            exit(2);
        }
        if (iConfigVerify) {
            /* a bit dirty, but useful... */
            exit(1);
        }
        localRet = RS_RET_OK;
    }
    CHKiRet(localRet);

    CHKiRet(rsyslogd_InitStdRatelimiters());

    if (bChDirRoot) {
        if (chdir("/") != 0) fprintf(stderr, "Can not do 'cd /' - still trying to run\n");
    }

    if (iConfigVerify) FINALIZE;
    /* after this point, we are in a "real" startup */

    thrdInit();
    CHKiRet(checkStartupOK());
    if (doFork) {
        parentPipeFD = forkRsyslog();
    }
    glblSetOurPid(getpid());

    hdlr_enable(SIGPIPE, SIG_IGN);
    hdlr_enable(SIGXFSZ, SIG_IGN);
    if (Debug || loadConf->globals.permitCtlC) {
        hdlr_enable(SIGUSR1, rsyslogdDebugSwitch);
        hdlr_enable(SIGINT, rsyslogdDoDie);
        hdlr_enable(SIGQUIT, rsyslogdDoDie);
    } else {
        hdlr_enable(SIGUSR1, SIG_IGN);
        hdlr_enable(SIGINT, SIG_IGN);
        hdlr_enable(SIGQUIT, SIG_IGN);
    }
    hdlr_enable(SIGTERM, rsyslogdDoDie);
    hdlr_enable(SIGCHLD, hdlr_sigchld);
    hdlr_enable(SIGHUP, hdlr_sighup);

    if (rsconfNeedDropPriv(loadConf)) {
        /* need to write pid file early as we may loose permissions */
        CHKiRet(writePidFile());
    }

    CHKiRet(rsconf.Activate(ourConf));

    if (runConf->globals.bLogStatusMsgs) {
        char bufStartUpMsg[512];
        snprintf(bufStartUpMsg, sizeof(bufStartUpMsg),
                 "[origin software=\"rsyslogd\" "
                 "swVersion=\"" VERSION "\" x-pid=\"%d\" x-info=\"https://www.rsyslog.com\"] start",
                 (int)glblGetOurPid());
        logmsgInternal(NO_ERRCODE, LOG_SYSLOG | LOG_INFO, (uchar *)bufStartUpMsg, 0);
    }

    if (!rsconfNeedDropPriv(runConf)) {
        CHKiRet(writePidFile());
    }

    /* END OF INITIALIZATION */
    DBGPRINTF("rsyslogd: initialization completed, transitioning to regular run mode\n");

    if (doFork) {
        tellChildReady(parentPipeFD, "OK");
        stddbg = -1; /* turn off writing to fd 1 */
        close(1);
        close(2);
        runConf->globals.bErrMsgToStderr = 0;
    }

finalize_it:
    if (iRet == RS_RET_VALIDATION_RUN) {
        fprintf(stderr, "rsyslogd: End of config validation run. Bye.\n");
        exit(0);
    } else if (iRet != RS_RET_OK) {
        fprintf(stderr,
                "rsyslogd: run failed with error %d (see rsyslog.h "
                "or try https://www.rsyslog.com/e/%d to learn what that number means)\n",
                iRet, iRet * -1);
        exit(1);
    }
}


/* this function pulls all internal messages from the buffer
 * and puts them into the processing engine.
 * We can only do limited error handling, as this would not
 * really help us. TODO: add error messages?
 * rgerhards, 2007-08-03
 */
void processImInternal(void) {
    smsg_t *pMsg;
    smsg_t *repMsg;

    while (iminternalRemoveMsg(&pMsg) == RS_RET_OK) {
        rsRetVal localRet = ratelimitMsg(internalMsg_ratelimiter, pMsg, &repMsg);
        if (repMsg != NULL) {
            logmsgInternal_doWrite(repMsg);
        }
        if (localRet == RS_RET_OK) {
            logmsgInternal_doWrite(pMsg);
        }
    }
}


/* This takes a received message that must be decoded and submits it to
 * the main message queue. This is a legacy function which is being provided
 * to aid older input plugins that do not support message creation via
 * the new interfaces themselves. It is not recommended to use this
 * function for new plugins. -- rgerhards, 2009-10-12
 */
rsRetVal parseAndSubmitMessage(const uchar *const hname,
                               const uchar *const hnameIP,
                               const uchar *const msg,
                               const int len,
                               const int flags,
                               const flowControl_t flowCtlType,
                               prop_t *const pInputName,
                               const struct syslogTime *const stTime,
                               const time_t ttGenTime,
                               ruleset_t *const pRuleset) {
    prop_t *pProp = NULL;
    smsg_t *pMsg = NULL;
    DEFiRet;

    /* we now create our own message object and submit it to the queue */
    if (stTime == NULL) {
        CHKiRet(msgConstruct(&pMsg));
    } else {
        CHKiRet(msgConstructWithTime(&pMsg, stTime, ttGenTime));
    }
    if (pInputName != NULL) MsgSetInputName(pMsg, pInputName);
    MsgSetRawMsg(pMsg, (char *)msg, len);
    MsgSetFlowControlType(pMsg, flowCtlType);
    MsgSetRuleset(pMsg, pRuleset);
    pMsg->msgFlags = flags | NEEDS_PARSING;

    MsgSetRcvFromStr(pMsg, hname, ustrlen(hname), &pProp);
    CHKiRet(prop.Destruct(&pProp));
    CHKiRet(MsgSetRcvFromIPStr(pMsg, hnameIP, ustrlen(hnameIP), &pProp));
    CHKiRet(prop.Destruct(&pProp));
    CHKiRet(submitMsg2(pMsg));

finalize_it:
    if (iRet != RS_RET_OK) {
        DBGPRINTF("parseAndSubmitMessage() error, discarding msg: %s\n", msg);
        if (pMsg != NULL) {
            msgDestruct(&pMsg);
        }
    }
    RETiRet;
}


/* helper to doHUP(), this "HUPs" each action. The necessary locking
 * is done inside the action class and nothing we need to take care of.
 * rgerhards, 2008-10-22
 */
DEFFUNC_llExecFunc(doHUPActions) {
    actionCallHUPHdlr((action_t *)pData);
    return RS_RET_OK; /* we ignore errors, we can not do anything either way */
}


/* This function processes a HUP after one has been detected. Note that this
 * is *NOT* the sighup handler. The signal is recorded by the handler, that record
 * detected inside the mainloop and then this function is called to do the
 * real work. -- rgerhards, 2008-10-22
 * Note: there is a VERY slim chance of a data race when the hostname is reset.
 * We prefer to take this risk rather than sync all accesses, because to the best
 * of my analysis it can not really hurt (the actual property is reference-counted)
 * but the sync would require some extra CPU for *each* message processed.
 * rgerhards, 2012-04-11
 */
static void doHUP(void) {
    char buf[512];

    DBGPRINTF("doHUP: doing modules\n");
    if (ourConf != NULL && ourConf->globals.bLogStatusMsgs) {
        snprintf(buf, sizeof(buf),
                 "[origin software=\"rsyslogd\" "
                 "swVersion=\"" VERSION "\" x-pid=\"%d\" x-info=\"https://www.rsyslog.com\"] rsyslogd was HUPed",
                 (int)glblGetOurPid());
        errno = 0;
        logmsgInternal(NO_ERRCODE, LOG_SYSLOG | LOG_INFO, (uchar *)buf, 0);
    }

    queryLocalHostname(runConf); /* re-read our name */
    ruleset.IterateAllActions(ourConf, doHUPActions, NULL);
    DBGPRINTF("doHUP: doing modules\n");
    modDoHUP();
    DBGPRINTF("doHUP: doing lookup tables\n");
    lookupDoHUP();
    DBGPRINTF("doHUP: doing errmsgs\n");
    errmsgDoHUP();
}

/* rsyslogdDoDie() is a signal handler. If called, it sets the bFinished variable
 * to indicate the program should terminate. However, it does not terminate
 * it itself, because that causes issues with multi-threading. The actual
 * termination is then done on the main thread. This solution might introduce
 * a minimal delay, but it is much cleaner than the approach of doing everything
 * inside the signal handler.
 * rgerhards, 2005-10-26
 * Note:
 * - we do not call DBGPRINTF() as this may cause us to block in case something
 *   with the threading is wrong.
 * - we do not really care about the return state of write(), but we need this
 *   strange check we do to silence compiler warnings (thanks, Ubuntu!)
 */
void rsyslogdDoDie(int sig) {
#define MSG1 "DoDie called.\n"
#define MSG2 "DoDie called 5 times - unconditional exit\n"
    static int iRetries = 0; /* debug aid */
    dbgprintf(MSG1);
    if (Debug == DEBUG_FULL) {
        if (write(1, MSG1, sizeof(MSG1) - 1) == -1) {
            dbgprintf("%s:%d: write failed\n", __FILE__, __LINE__);
        }
    }
    if (iRetries++ == 4) {
        if (Debug == DEBUG_FULL) {
            if (write(1, MSG2, sizeof(MSG2) - 1) == -1) {
                dbgprintf("%s:%d: write failed\n", __FILE__, __LINE__);
            }
        }
        abort();
    }
    bFinished = sig;
    if (runConf && runConf->globals.debugOnShutdown) {
        /* kind of hackish - set to 0, so that debug_swith will enable
         * and AND emit the "start debug log" message.
         */
        debugging_on = 0;
        rsyslogdDebugSwitch();
    }
#undef MSG1
#undef MSG2
    /* at least on FreeBSD we seem not to necessarily awake the main thread.
     * So let's do it explicitly.
     */
    dbgprintf("awaking mainthread\n");
    pthread_kill(mainthread, SIGTTIN);
}


static void wait_timeout(const sigset_t *sigmask) {
    struct timespec tvSelectTimeout;

    tvSelectTimeout.tv_sec = runConf->globals.janitorInterval * 60; /* interval is in minutes! */
    tvSelectTimeout.tv_nsec = 0;

#ifdef _AIX
    if (!src_exists) {
        /* it looks like select() is NOT interrupted by HUP, even though
         * SA_RESTART is not given in the signal setup. As this code is
         * not expected to be used in production (when running as a
         * service under src control), we simply make a kind of
         * "somewhat-busy-wait" algorithm. We compute our own
         * timeout value, which we count down to zero. We do this
         * in useful subsecond steps.
         */
        const long wait_period = 500000000; /* wait period in nanoseconds */
        int timeout = runConf->globals.janitorInterval * 60 * (1000000000 / wait_period);

        tvSelectTimeout.tv_sec = 0;
        tvSelectTimeout.tv_nsec = wait_period;
        do {
            pthread_mutex_lock(&mutHadHUP);
            if (bFinished || bHadHUP) {
                pthread_mutex_unlock(&mutHadHUP);
                break;
            }
            pthread_mutex_unlock(&mutHadHUP);
            pselect(1, NULL, NULL, NULL, &tvSelectTimeout, sigmask);
        } while (--timeout > 0);
    } else {
        char buf[256];
        fd_set rfds;

        FD_ZERO(&rfds);
        FD_SET(SRC_FD, &rfds);
        if (pselect(SRC_FD + 1, (fd_set *)&rfds, NULL, NULL, &tvSelectTimeout, sigmask)) {
            if (FD_ISSET(SRC_FD, &rfds)) {
                rc = recvfrom(SRC_FD, &srcpacket, SRCMSG, 0, &srcaddr, &addrsz);
                if (rc < 0) {
                    if (errno != EINTR) {
                        fprintf(stderr, "%s: ERROR: '%d' recvfrom\n", progname, errno);
                        exit(1);  // TODO: this needs to be handled gracefully
                    } else { /* punt on short read */
                        return;
                    }

                    switch (srcpacket.subreq.action) {
                        case START:
                            dosrcpacket(SRC_SUBMSG,
                                        "ERROR: rsyslogd does not support this "
                                        "option.\n",
                                        sizeof(struct srcrep));
                            break;
                        case STOP:
                            if (srcpacket.subreq.object == SUBSYSTEM) {
                                dosrcpacket(SRC_OK, NULL, sizeof(struct srcrep));
                                (void)snprintf(buf, sizeof(buf) / sizeof(char),
                                               " [origin "
                                               "software=\"rsyslogd\" "
                                               "swVersion=\"" VERSION
                                               "\" x-pid=\"%d\" x-info=\"https://www.rsyslog.com\"]"
                                               " exiting due to stopsrc.",
                                               (int)glblGetOurPid());
                                errno = 0;
                                logmsgInternal(NO_ERRCODE, LOG_SYSLOG | LOG_INFO, (uchar *)buf, 0);
                                return;
                            } else
                                dosrcpacket(SRC_SUBMSG,
                                            "ERROR: rsyslogd does not support "
                                            "this option.\n",
                                            sizeof(struct srcrep));
                            break;
                        case REFRESH:
                            dosrcpacket(SRC_SUBMSG,
                                        "ERROR: rsyslogd does not support this "
                                        "option.\n",
                                        sizeof(struct srcrep));
                            break;
                        default:
                            dosrcpacket(SRC_SUBICMD, NULL, sizeof(struct srcrep));
                            break;
                    }
                }
            }
        }
    }
#else
    pselect(0, NULL, NULL, NULL, &tvSelectTimeout, sigmask);
#endif /* AIXPORT : SRC end */
}


static void reapChild(void) {
    pid_t child;
    do {
        int status;
        child = waitpid(-1, &status, WNOHANG);
        if (child != -1 && child != 0) {
            glblReportChildProcessExit(runConf, NULL, child, status);
        }
    } while (child > 0);
}


/* This is the main processing loop. It is called after successful initialization.
 * When it returns, the syslogd terminates.
 * Its sole function is to provide some housekeeping things. The real work is done
 * by the other threads spawned.
 */
static void mainloop(void) {
    time_t tTime;
    sigset_t origmask;
    sigset_t sigblockset;
    int need_free_mutex;

    sigemptyset(&sigblockset);
    sigaddset(&sigblockset, SIGTERM);
    sigaddset(&sigblockset, SIGCHLD);
    sigaddset(&sigblockset, SIGHUP);

    do {
        sigemptyset(&origmask);
        pthread_sigmask(SIG_BLOCK, &sigblockset, &origmask);
        pthread_mutex_lock(&mutChildDied);
        need_free_mutex = 1;
        if (bChildDied) {
            bChildDied = 0;
            pthread_mutex_unlock(&mutChildDied);
            need_free_mutex = 0;
            reapChild();
        }
        if (need_free_mutex) {
            pthread_mutex_unlock(&mutChildDied);
        }

        pthread_mutex_lock(&mutHadHUP);
        need_free_mutex = 1;
        if (bHadHUP) {
            need_free_mutex = 0;
            pthread_mutex_unlock(&mutHadHUP);
            doHUP();
            pthread_mutex_lock(&mutHadHUP);
            bHadHUP = 0;
            pthread_mutex_unlock(&mutHadHUP);
        }
        if (need_free_mutex) {
            pthread_mutex_unlock(&mutHadHUP);
        }

        processImInternal();

        if (bFinished) break; /* exit as quickly as possible */

        wait_timeout(&origmask);
        pthread_sigmask(SIG_UNBLOCK, &sigblockset, NULL);

        janitorRun();

        assert(datetime.GetTime != NULL); /* This is only to keep clang static analyzer happy */
        datetime.GetTime(&tTime);
        checkGoneAwaySenders(tTime);

    } while (!bFinished); /* end do ... while() */
}

/* Finalize and destruct all actions.
 */
static void rsyslogd_destructAllActions(void) {
    ruleset.DestructAllActions(runConf);
    PREFER_STORE_0_TO_INT(&bHaveMainQueue); /* flag that internal messages need to be temporarily stored */
}


/* de-initialize everything, make ready for termination */
static void deinitAll(void) {
    char buf[256];

    DBGPRINTF("exiting on signal %d\n", bFinished);

    /* IMPORTANT: we should close the inputs first, and THEN send our termination
     * message. If we do it the other way around, logmsgInternal() may block on
     * a full queue and the inputs still fill up that queue. Depending on the
     * scheduling order, we may end up with logmsgInternal being held for a quite
     * long time. When the inputs are terminated first, that should not happen
     * because the queue is drained in parallel. The situation could only become
     * an issue with extremely long running actions in a queue full environment.
     * However, such actions are at least considered poorly written, if not
     * outright wrong. So we do not care about this very remote problem.
     * rgerhards, 2008-01-11
     */

    /* close the inputs */
    DBGPRINTF("Terminating input threads...\n");
    glbl.SetGlobalInputTermination();

    thrdTerminateAll();

    /* and THEN send the termination log message (see long comment above) */
    if (bFinished && runConf->globals.bLogStatusMsgs) {
        (void)snprintf(buf, sizeof(buf),
                       "[origin software=\"rsyslogd\" "
                       "swVersion=\"" VERSION
                       "\" x-pid=\"%d\" x-info=\"https://www.rsyslog.com\"]"
                       " exiting on signal %d.",
                       (int)glblGetOurPid(), bFinished);
        errno = 0;
        logmsgInternal(NO_ERRCODE, LOG_SYSLOG | LOG_INFO, (uchar *)buf, 0);
    }
    processImInternal(); /* make sure not-yet written internal messages are processed */
    /* we sleep a couple of ms to give the queue a chance to pick up the late messages
     * (including exit message); otherwise we have seen cases where the message did
     * not make it to log files, even on idle systems.
     */
    srSleep(0, 50);

    /* drain queue (if configured so) and stop main queue worker thread pool */
    DBGPRINTF("Terminating main queue...\n");
    qqueueDestruct(&runConf->pMsgQueue);
    runConf->pMsgQueue = NULL;

    /* Free resources and close connections. This includes flushing any remaining
     * repeated msgs.
     */
    DBGPRINTF("Terminating outputs...\n");
    rsyslogd_destructAllActions();

    DBGPRINTF("all primary multi-thread sources have been terminated - now doing aux cleanup...\n");

    DBGPRINTF("destructing current config...\n");
    rsconf.Destruct(&runConf);

    modExitIminternal();

    if (pInternalInputName != NULL) prop.Destruct(&pInternalInputName);

    /* the following line cleans up CfSysLineHandlers that were not based on loadable
     * modules. As such, they are not yet cleared.  */
    unregCfSysLineHdlrs();

    /* this is the last spot where this can be done - below output modules are unloaded! */

    parserClassExit();
    rsconfClassExit();
    strExit();
    ratelimitModExit();
    dnscacheDeinit();
    thrdExit();
    objRelease(net, LM_NET_FILENAME);

    module.UnloadAndDestructAll(eMOD_LINK_ALL);

    rsrtExit(); /* runtime MUST always be deinitialized LAST (except for debug system) */
    DBGPRINTF("Clean shutdown completed, bye\n");

    errmsgExit();
    /* dbgClassExit MUST be the last one, because it de-inits the debug system */
    dbgClassExit();

    /* NO CODE HERE - dbgClassExit() must be the last thing before exit()! */
    clearPidFile();
}

/* This is the main entry point into rsyslogd. This must be a function in its own
 * right in order to initialize the debug system in a portable way (otherwise we would
 * need to have a statement before variable definitions.
 * rgerhards, 20080-01-28
 */
int main(int argc, char **argv) {
#if defined(_AIX)
    /* SRC support : fd 0 (stdin) must be the SRC socket
     * startup.  fd 0 is duped to a new descriptor so that stdin can be used
     * internally by rsyslogd.
     */

    pthread_mutex_init(&mutHadHUP, NULL);
    pthread_mutex_init(&mutChildDied, NULL);
    strncpy(progname, argv[0], sizeof(progname) - 1);
    addrsz = sizeof(srcaddr);
    if ((rc = getsockname(0, &srcaddr, &addrsz)) < 0) {
        fprintf(stderr, "%s: continuing without SRC support\n", progname);
        src_exists = FALSE;
    }
    if (src_exists)
        if (dup2(0, SRC_FD) == -1) {
            fprintf(stderr, "%s: dup2 failed exiting now...\n", progname);
            /* In the unlikely event of dup2 failing we exit */
            exit(-1);
        }
#endif

    mainthread = pthread_self();
    if ((int)getpid() == 1) {
        fprintf(stderr,
                "rsyslogd %s: running as pid 1, enabling "
                "container-specific defaults, press ctl-c to "
                "terminate rsyslog\n",
                VERSION);
        PidFile = strdup("NONE"); /* disables pid file writing */
        glblPermitCtlC = 1;
        runningInContainer = 1;
        emitTZWarning = 1;
    } else {
        /* "dynamic defaults" - non-container case */
        PidFile = strdup(PATH_PIDFILE);
    }
    if (PidFile == NULL) {
        fprintf(stderr,
                "rsyslogd: could not alloc memory for pid file "
                "default name - aborting\n");
        exit(1);
    }

    /* disable case-sensitive comparisons in variable subsystem: */
    fjson_global_do_case_sensitive_comparison(0);

    dbgClassInit();

    initAll(argc, argv);
#ifdef HAVE_LIBSYSTEMD
    sd_notify(0, "READY=1");
    dbgprintf("done signaling to systemd that we are ready!\n");
#endif
    DBGPRINTF("max message size: %d\n", glblGetMaxLine(runConf));
    DBGPRINTF("----RSYSLOGD INITIALIZED\n");
    LogMsg(0, RS_RET_OK, LOG_DEBUG,
           "rsyslogd fully started up and initialized "
           "- begin actual processing");

    mainloop();
    LogMsg(0, RS_RET_OK, LOG_DEBUG, "rsyslogd shutting down");
    deinitAll();
    osf_close();
    pthread_mutex_destroy(&mutChildDied);
    pthread_mutex_destroy(&mutHadHUP);
    return 0;
}
