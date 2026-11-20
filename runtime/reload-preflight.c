/* SPDX-License-Identifier: Apache-2.0 */

/* reload-preflight.c - config reload "report" mode (pre-flight child)
 *
 * See reload-preflight.h for the overview. State machine, all on the main
 * thread:
 *
 *   idle --(HUP, files changed)--> running --(exit | timeout)--> idle
 *
 * A HUP while a child runs only sets the "rerun" flag. When the child is
 * done and the files differ from the snapshot taken at spawn time, exactly
 * one more child is started. A burst of HUPs thus costs at most two children.
 *
 * The child is created with fork()+execve(). Everything the child needs
 * (argument vector, environment, descriptors) is prepared before the fork so
 * that the child only calls async-signal-safe functions.
 *
 * Copyright 2026 Rainer Gerhards and Adiscon GmbH.
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
#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#if defined(HAVE_LINUX_CLOSE_RANGE_H)
    #include <linux/close_range.h>
#endif
#if defined(HAVE_SYS_PRCTL_H)
    #include <sys/prctl.h>
#endif

#include "rsyslog.h"
#include "errmsg.h"
#include "obj.h"
#include "statsobj.h"
#include "unicode-helper.h"
#include "rsconf.h"
#include "cfgfiles.h"
#include "reload-preflight.h"

#define PF_OUT_MAX 4096 /* output bytes kept from the child */
#define PF_MAX_LINES 10 /* output lines shown in the report */
#define PF_DEFAULT_TIMEOUT_MS 30000
#define PF_POLL_MS 250 /* main loop cap while a child runs */
#define PF_MAX_OPTS 64
#define PF_MSG_MAX 2048

DEFobjCurrIf(obj) DEFobjCurrIf(statsobj)

    /* environment variables the child must not inherit: it must neither write
     * the debug log nor talk to systemd. */
    static const char *const dropEnv[] = {"RSYSLOG_DEBUG", "RSYSLOG_DEBUGLOG", "NOTIFY_SOCKET", "WATCHDOG_USEC",
                                          "WATCHDOG_PID",  "LISTEN_FDS",       "LISTEN_PID",    NULL};

/* ---- data captured at startup ---- */
static char *exePath = NULL; /* what we execve() */
static char *argv0Copy = NULL;
static char *replayOpts[PF_MAX_OPTS]; /* "-f", "file", "-M", "dir", ... */
static int nReplayOpts = 0;
static char *loadCwd = NULL;

/* ---- state machine ---- */
static int bInit = 0; /* statistics set up */
static int bDisabled = 0; /* e.g. no binary found */
static int timeoutMs = PF_DEFAULT_TIMEOUT_MS;
static pid_t childPid = -1;
static int childFd = -1;
static uint64_t deadlineMs = 0;
static char outBuf[PF_OUT_MAX + 1];
static size_t outLen = 0;
static int bOutTruncated = 0;
static int bRerun = 0;
static cfgfiles_t *spawnSnap = NULL; /* file state when the child was started */
static char lastMsg[PF_MSG_MAX]; /* last reported outcome, to log identical ones once */

static statsobj_t *stats = NULL;
STATSCOUNTER_DEF(ctrAttempted, mutCtrAttempted)
STATSCOUNTER_DEF(ctrUnchanged, mutCtrUnchanged)
STATSCOUNTER_DEF(ctrCoalesced, mutCtrCoalesced)
STATSCOUNTER_DEF(ctrInvalid, mutCtrInvalid)
STATSCOUNTER_DEF(ctrValidChanged, mutCtrValidChanged)
STATSCOUNTER_DEF(ctrTimeout, mutCtrTimeout)
STATSCOUNTER_DEF(ctrFailed, mutCtrFailed)


static uint64_t nowMs(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return ((uint64_t)ts.tv_sec * 1000ULL) + (uint64_t)(ts.tv_nsec / 1000000ULL);
}


/* ---------------------------------------------------------------------- */
/* startup bookkeeping                                                    */
/* ---------------------------------------------------------------------- */

/* search an executable file named @p name in $PATH (only for argv[0]
 * without a slash); returns malloc'ed absolute path or NULL */
static char *findInPath(const char *name) {
    const char *path = getenv("PATH");
    char *copy, *dir, *save = NULL, *res = NULL;
    char buf[PATH_MAX];

    if (path == NULL || (copy = strdup(path)) == NULL) return NULL;
    for (dir = strtok_r(copy, ":", &save); dir != NULL; dir = strtok_r(NULL, ":", &save)) {
        snprintf(buf, sizeof(buf), "%s/%s", dir, name);
        if (access(buf, X_OK) == 0) {
            res = realpath(buf, NULL);
            if (res != NULL) break;
        }
    }
    free(copy);
    return res;
}

void reloadPreflightInitExe(const char *argv0) {
    if (argv0 != NULL) argv0Copy = strdup(argv0);
#if defined(__linux__)
    /* Refers to the running image even if a package upgrade replaced the
     * file on disk, which is the binary we want to ask. */
    if (access("/proc/self/exe", X_OK) == 0) {
        exePath = strdup("/proc/self/exe");
        return;
    }
#endif
#if defined(__sun)
    {
        const char *const execname = getexecname();
        if (execname != NULL && (exePath = realpath(execname, NULL)) != NULL) return;
    }
#endif
    /* Fallback: argv[0], resolved before any chdir() happens. */
    if (argv0 != NULL) {
        if (strchr(argv0, '/') != NULL)
            exePath = realpath(argv0, NULL);
        else
            exePath = findInPath(argv0);
    }
}

void reloadPreflightNoteOpt(int opt, const char *arg) {
    switch (opt) {
        /* replayed; see the M1 pre-flight design in doc/source/development/config_reload.rst.
         * Never replayed: -T (we already are inside the chroot), -i, -n, -d, -D,
         * -F, -o, -N (we add -N1 ourselves). */
        case '4':
        case '6':
        case 'A':
        case 'q':
        case 'Q':
        case 'x':
        case 'w':
        case 'C':
            if (nReplayOpts + 1 <= PF_MAX_OPTS) {
                char flag[3] = {'-', (char)opt, '\0'};
                if ((replayOpts[nReplayOpts] = strdup(flag)) != NULL) ++nReplayOpts;
            }
            break;
        case 'f':
        case 'M':
        case 'S':
        case 'u':
            if (arg != NULL && nReplayOpts + 2 <= PF_MAX_OPTS) {
                char flag[3] = {'-', (char)opt, '\0'};
                char *a = strdup(flag);
                char *b = strdup(arg);
                if (a == NULL || b == NULL) {
                    free(a);
                    free(b);
                } else {
                    replayOpts[nReplayOpts++] = a;
                    replayOpts[nReplayOpts++] = b;
                }
            }
            break;
        default:
            break;
    }
}

void reloadPreflightNoteCwd(void) {
    char cwd[PATH_MAX];
    free(loadCwd);
    loadCwd = (getcwd(cwd, sizeof(cwd)) != NULL) ? strdup(cwd) : NULL;
}

int reloadPreflightIsChild(void) {
    return getenv(RELOAD_PREFLIGHT_CHILD_ENV) != NULL;
}

void reloadPreflightInit(void) {
    const char *env;
    DEFiRet;

    if (runConf == NULL || runConf->globals.reloadOnHUP == 0 || bInit) FINALIZE;
    if (exePath == NULL) {
        LogError(0, RS_RET_CONF_RELOAD_CHECK_FAILED,
                 "config.reloadOnHUP=\"report\": cannot determine the path of the rsyslogd "
                 "binary - config reload checks are disabled");
        bDisabled = 1;
    }
    /* testbench/diagnostic knob: shorten the validation deadline */
    if ((env = getenv("RSYSLOG_RELOAD_CHECK_TIMEOUT_MS")) != NULL && atoi(env) > 0) timeoutMs = atoi(env);

    STATSCOUNTER_INIT(ctrAttempted, mutCtrAttempted);
    STATSCOUNTER_INIT(ctrUnchanged, mutCtrUnchanged);
    STATSCOUNTER_INIT(ctrCoalesced, mutCtrCoalesced);
    STATSCOUNTER_INIT(ctrInvalid, mutCtrInvalid);
    STATSCOUNTER_INIT(ctrValidChanged, mutCtrValidChanged);
    STATSCOUNTER_INIT(ctrTimeout, mutCtrTimeout);
    STATSCOUNTER_INIT(ctrFailed, mutCtrFailed);
    CHKiRet(objGetObjInterface(&obj));
    CHKiRet(objUse(statsobj, CORE_COMPONENT));
    CHKiRet(statsobj.Construct(&stats));
    CHKiRet(statsobj.SetName(stats, UCHAR_CONSTANT("config.reload.check")));
    CHKiRet(statsobj.SetOrigin(stats, UCHAR_CONSTANT("core")));
    CHKiRet(
        statsobj.AddCounter(stats, UCHAR_CONSTANT("attempted"), ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrAttempted));
    CHKiRet(
        statsobj.AddCounter(stats, UCHAR_CONSTANT("unchanged"), ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrUnchanged));
    CHKiRet(
        statsobj.AddCounter(stats, UCHAR_CONSTANT("coalesced"), ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrCoalesced));
    CHKiRet(statsobj.AddCounter(stats, UCHAR_CONSTANT("invalid"), ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrInvalid));
    CHKiRet(statsobj.AddCounter(stats, UCHAR_CONSTANT("valid_changed"), ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &ctrValidChanged));
    CHKiRet(statsobj.AddCounter(stats, UCHAR_CONSTANT("timeout"), ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrTimeout));
    CHKiRet(statsobj.AddCounter(stats, UCHAR_CONSTANT("failed"), ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrFailed));
    CHKiRet(statsobj.ConstructFinalize(stats));
    bInit = 1;
finalize_it:
    if (iRet != RS_RET_OK) {
        LogError(0, iRet, "config.reloadOnHUP=\"report\": could not set up statistics - counters are unavailable");
        if (stats != NULL) statsobj.Destruct(&stats);
        bInit = 1; /* the check itself works without counters */
    }
}


/* ---------------------------------------------------------------------- */
/* reporting                                                              */
/* ---------------------------------------------------------------------- */

static int containsNoCase(const char *hay, const char *needle) {
    const size_t nl = strlen(needle);
    for (; *hay != '\0'; ++hay)
        if (strncasecmp(hay, needle, nl) == 0) return 1;
    return 0;
}

static int isBannerLine(const char *l) {
    return (!strncmp(l, "rsyslogd: version ", 18) && strstr(l, "config validation run") != NULL) ||
           !strncmp(l, "rsyslogd: End of config validation run", 38);
}

/* A -N1 run prints only warnings and errors. Errors are what matters for the verdict. */
static int isErrorLine(const char *l) {
    return strncmp(l, "NOTE:", 5) != 0 && !containsNoCase(l, "warning");
}

/* Condense the child output into at most PF_MAX_LINES lines, joined by " | ".
 * Control characters are replaced. Lines that look like errors are preferred
 * over warnings and notes; if there are none, the first lines are used. */
static void condenseOutput(char *dst, size_t dstlen) {
    char *lines[512];
    size_t nlines = 0, i;
    size_t used = 0;
    int shown = 0, pass, bMore = 0;
    char *p, *q, *save;

    dst[0] = '\0';
    /* sanitize in place: only '\n' survives as line separator */
    for (p = outBuf; *p != '\0'; ++p) {
        if (*p != '\n' && ((unsigned char)*p < 0x20 || *p == 0x7f)) *p = ' ';
    }
    for (p = strtok_r(outBuf, "\n", &save); p != NULL && nlines < sizeof(lines) / sizeof(lines[0]);
         p = strtok_r(NULL, "\n", &save)) {
        while (*p == ' ') ++p;
        if (*p == '\0' || isBannerLine(p)) continue;
        /* cut the " [v8.x try https://www.rsyslog.com/e/NNNN ]" trailer, it only makes the line long */
        if ((q = strstr(p, " [v")) != NULL && strstr(q, " try https://") != NULL) *q = '\0';
        lines[nlines++] = p;
    }
    for (pass = 0; pass < 2 && shown == 0; ++pass) {
        size_t candidates = 0;
        for (i = 0; i < nlines; ++i) {
            const char *l = lines[i];
            if (pass == 0 && !isErrorLine(l)) continue;
            ++candidates;
            if (shown < PF_MAX_LINES && used + 1 < dstlen) {
                int n;
                if (!strncmp(l, "rsyslogd: ", 10)) l += 10; /* the prefix says nothing new */
                n = snprintf(dst + used, dstlen - used, "%s%s", shown ? " | " : "", l);
                if (n < 0 || (size_t)n >= dstlen - used) {
                    used = dstlen - 1; /* cut off */
                    bMore = 1;
                    break;
                }
                used += (size_t)n;
                ++shown;
            }
        }
        if (candidates > (size_t)shown) bMore = 1;
    }
    if ((bOutTruncated || bMore) && used + 24 < dstlen) {
        snprintf(dst + used, dstlen - used, " | [output truncated]");
    }
}

/* Log one outcome line unless it is identical to the previous one. */
static void reportOutcome(const rsRetVal code, const int severity, const char *text) {
    if (!strcmp(lastMsg, text)) {
        DBGPRINTF("reload check: identical to previous result, not logged again\n");
        return;
    }
    snprintf(lastMsg, sizeof(lastMsg), "%s", text);
    LogMsg(0, code, severity, "%s", text);
}


/* ---------------------------------------------------------------------- */
/* child handling                                                         */
/* ---------------------------------------------------------------------- */

static void closeChildFd(void) {
    if (childFd != -1) {
        close(childFd);
        childFd = -1;
    }
}

/* read what is available, never blocks (the fd is O_NONBLOCK) */
static void drainPipe(void) {
    char buf[1024];
    ssize_t n;

    if (childFd == -1) return;
    while ((n = read(childFd, buf, sizeof(buf))) > 0) {
        if (outLen < PF_OUT_MAX) {
            size_t take = (size_t)n;
            if (take > PF_OUT_MAX - outLen) take = PF_OUT_MAX - outLen;
            memcpy(outBuf + outLen, buf, take);
            outLen += take;
            if (take < (size_t)n) bOutTruncated = 1;
        } else {
            bOutTruncated = 1; /* keep draining, discard */
        }
    }
}

static int spawnChild(void);

/* the child is gone: decide on the verdict and report */
static void childDone(const int bTimedOut, const int status) {
    char detail[PF_MSG_MAX];
    char msg[PF_MSG_MAX];
    int bRunAgain = 0;

    if (!bTimedOut) drainPipe();
    closeChildFd();
    childPid = -1;
    outBuf[outLen] = '\0';
    condenseOutput(detail, sizeof(detail) - 256);

    if (bTimedOut) {
        STATSCOUNTER_INC(ctrTimeout, mutCtrTimeout);
        snprintf(msg, sizeof(msg),
                 "config reload check: validation of the changed configuration timed out after %d seconds, "
                 "running configuration kept",
                 (timeoutMs + 999) / 1000);
        reportOutcome(RS_RET_CONF_RELOAD_TIMEOUT, LOG_ERR, msg);
    } else if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        STATSCOUNTER_INC(ctrValidChanged, mutCtrValidChanged);
        reportOutcome(RS_RET_OK, LOG_INFO,
                      "config reload check: configuration changed and is valid; restart rsyslog to apply it");
    } else if (WIFEXITED(status) && (WEXITSTATUS(status) == 126 || WEXITSTATUS(status) == 127)) {
        STATSCOUNTER_INC(ctrFailed, mutCtrFailed);
        snprintf(msg, sizeof(msg), "config reload check: could not run the validation process (exit code %d)%s%s",
                 WEXITSTATUS(status), detail[0] ? ": " : "", detail);
        reportOutcome(RS_RET_CONF_RELOAD_CHECK_FAILED, LOG_ERR, msg);
    } else if (WIFSIGNALED(status)) {
        STATSCOUNTER_INC(ctrFailed, mutCtrFailed);
        snprintf(msg, sizeof(msg), "config reload check: the validation process died from signal %d%s%s",
                 WTERMSIG(status), detail[0] ? ": " : "", detail);
        reportOutcome(RS_RET_CONF_RELOAD_CHECK_FAILED, LOG_ERR, msg);
    } else {
        STATSCOUNTER_INC(ctrInvalid, mutCtrInvalid);
        snprintf(msg, sizeof(msg),
                 "config reload check: changed configuration is invalid, running configuration kept%s%s",
                 detail[0] ? ": " : "", detail);
        reportOutcome(RS_RET_CONF_RELOAD_INVALID, LOG_ERR, msg);
    }

    /* coalesced HUPs: run once more if the files changed again meanwhile */
    if (bRerun) {
        bRerun = 0;
        cfgfiles_t *cur = cfgfilesCapture(cfgfilesGetRecord());
        if (cur != NULL && spawnSnap != NULL && !cfgfilesEqual(cur, spawnSnap)) {
            bRunAgain = 1;
            cfgfilesDestruct(spawnSnap);
            spawnSnap = cur;
        } else {
            cfgfilesDestruct(cur);
        }
    }
    if (!bRunAgain) {
        cfgfilesDestruct(spawnSnap);
        spawnSnap = NULL;
    } else if (spawnChild() != 0) {
        cfgfilesDestruct(spawnSnap);
        spawnSnap = NULL;
    }
}

/* kill a running child and wait for it */
static void killChild(void) {
    if (childPid > 0) {
        int status;
        kill(childPid, SIGKILL);
        while (waitpid(childPid, &status, 0) == -1 && errno == EINTR) /* retry */
            ;
        childPid = -1;
    }
    closeChildFd();
}

/* Build the environment for the child (caller frees the array and entries). */
static char **buildEnv(void) {
    size_t n = 0, i, o = 0;
    char **env;

    while (environ != NULL && environ[n] != NULL) ++n;
    env = calloc(n + 2, sizeof(char *));
    if (env == NULL) return NULL;
    for (i = 0; i < n; ++i) {
        int j, drop = 0;
        for (j = 0; dropEnv[j] != NULL; ++j) {
            const size_t l = strlen(dropEnv[j]);
            if (!strncmp(environ[i], dropEnv[j], l) && environ[i][l] == '=') drop = 1;
        }
        if (drop || !strncmp(environ[i], RELOAD_PREFLIGHT_CHILD_ENV "=", sizeof(RELOAD_PREFLIGHT_CHILD_ENV))) continue;
        if ((env[o] = strdup(environ[i])) == NULL) goto fail;
        ++o;
    }
    if ((env[o] = strdup(RELOAD_PREFLIGHT_CHILD_ENV "=1")) == NULL) goto fail;
    return env;
fail:
    for (i = 0; env[i] != NULL; ++i) free(env[i]);
    free(env);
    return NULL;
}

static void freeVec(char **v) {
    size_t i;
    if (v == NULL) return;
    for (i = 0; v[i] != NULL; ++i) free(v[i]);
    free(v);
}

/* move *fd to a descriptor >= 3 so that the child's dup2() calls cannot clobber it */
static int fdAbove2(int *fd) {
    if (*fd < 3) {
        const int nfd = fcntl(*fd, F_DUPFD_CLOEXEC, 3);
        if (nfd < 0) return -1;
        close(*fd);
        *fd = nfd;
    }
    return 0;
}

/* Start the validation child. Returns 0 on success; on failure the outcome
 * was already reported. Takes the file snapshot from spawnSnap. */
static int spawnChild(void) {
    int pfd[2] = {-1, -1};
    int nullfd = -1;
    char **argv = NULL;
    char **envp = NULL;
    int i, n = 0, ret = -1, maxfd = 0;
    sigset_t allsigs, oldsigs, emptyset;
    pid_t pid;

    STATSCOUNTER_INC(ctrAttempted, mutCtrAttempted);
    outLen = 0;
    bOutTruncated = 0;

    argv = calloc((size_t)nReplayOpts + 4, sizeof(char *));
    envp = buildEnv();
    if (argv == NULL || envp == NULL) goto fail;
    argv[n++] = strdup(argv0Copy != NULL ? argv0Copy : "rsyslogd");
    for (i = 0; i < nReplayOpts; ++i) argv[n++] = strdup(replayOpts[i]);
    argv[n++] = strdup("-N1");
    for (i = 0; i < n; ++i)
        if (argv[i] == NULL) goto fail;

    if (pipe(pfd) != 0) goto fail;
    if (fdAbove2(&pfd[0]) != 0 || fdAbove2(&pfd[1]) != 0) goto fail;
    (void)fcntl(pfd[0], F_SETFD, FD_CLOEXEC);
    (void)fcntl(pfd[0], F_SETFL, fcntl(pfd[0], F_GETFL) | O_NONBLOCK);
    nullfd = open("/dev/null", O_RDWR | O_CLOEXEC);
    if (nullfd < 0 || fdAbove2(&nullfd) != 0) goto fail;
#if !defined(HAVE_CLOSE_RANGE)
    maxfd = (int)sysconf(_SC_OPEN_MAX);
    if (maxfd < 0 || maxfd > 65536) maxfd = 65536;
#endif

    /* No signal handler may run in the child between fork() and execve() */
    sigfillset(&allsigs);
    sigemptyset(&emptyset);
    pthread_sigmask(SIG_BLOCK, &allsigs, &oldsigs);
    pid = fork();
    if (pid == 0) {
        /* child: async-signal-safe functions only */
        int sig, fd;
#if defined(HAVE_PRCTL) && defined(PR_SET_PDEATHSIG)
        /* never outlive the daemon, e.g. if it dies without running its shutdown code */
        (void)prctl(PR_SET_PDEATHSIG, SIGKILL);
#endif
        if (dup2(nullfd, 0) < 0 || dup2(pfd[1], 1) < 0 || dup2(pfd[1], 2) < 0) _exit(126);
#if defined(HAVE_CLOSE_RANGE)
        (void)maxfd;
        if (close_range(3, ~0U, 0) != 0)
            for (fd = 3; fd < 1024; ++fd) close(fd);
#else
        for (fd = 3; fd < maxfd; ++fd) close(fd);
#endif
        for (sig = 1; sig < 65; ++sig) {
            struct sigaction sa;
            if (sig == SIGKILL || sig == SIGSTOP) continue;
            memset(&sa, 0, sizeof(sa));
            sa.sa_handler = SIG_DFL;
            (void)sigaction(sig, &sa, NULL);
        }
        (void)sigprocmask(SIG_SETMASK, &emptyset, NULL);
        if (loadCwd != NULL && chdir(loadCwd) != 0) _exit(126);
        execve(exePath, argv, envp);
        _exit(127);
    }
    pthread_sigmask(SIG_SETMASK, &oldsigs, NULL);
    if (pid < 0) goto fail;

    close(pfd[1]);
    pfd[1] = -1;
    childPid = pid;
    childFd = pfd[0];
    pfd[0] = -1;
    deadlineMs = nowMs() + (uint64_t)timeoutMs;
    DBGPRINTF("reload check: started validation child pid %d\n", (int)pid);
    ret = 0;
    goto done;

fail:
    STATSCOUNTER_INC(ctrFailed, mutCtrFailed);
    {
        char msg[PF_MSG_MAX];
        snprintf(msg, sizeof(msg), "config reload check: could not start the validation process: %s", strerror(errno));
        reportOutcome(RS_RET_CONF_RELOAD_CHECK_FAILED, LOG_ERR, msg);
    }
done:
    if (pfd[0] != -1) close(pfd[0]);
    if (pfd[1] != -1) close(pfd[1]);
    if (nullfd != -1) close(nullfd);
    freeVec(argv);
    freeVec(envp);
    return ret;
}


/* ---------------------------------------------------------------------- */
/* interface for the main loop                                            */
/* ---------------------------------------------------------------------- */

void reloadPreflightOnHUP(void) {
    cfgfiles_t *cur;

    if (runConf == NULL || runConf->globals.reloadOnHUP == 0 || bDisabled) return;

    if (childPid > 0) {
        /* a check is running: remember that another one may be needed */
        bRerun = 1;
        STATSCOUNTER_INC(ctrCoalesced, mutCtrCoalesced);
        return;
    }
    cur = cfgfilesCapture(cfgfilesGetRecord());
    if (cur == NULL) return; /* out of memory: skip this check */
    if (cfgfilesEqual(cfgfilesGetRecord(), cur)) {
        STATSCOUNTER_INC(ctrUnchanged, mutCtrUnchanged);
        lastMsg[0] = '\0'; /* a later change is a new episode and is reported again */
        cfgfilesDestruct(cur);
        return;
    }
    cfgfilesDestruct(spawnSnap);
    spawnSnap = cur;
    if (spawnChild() != 0) {
        cfgfilesDestruct(spawnSnap);
        spawnSnap = NULL;
    }
}

void reloadPreflightPoll(uint64_t now_ms) {
    if (childPid <= 0) return;
    drainPipe();
    if (now_ms >= deadlineMs) {
        killChild();
        childDone(1, 0);
    }
}

int reloadPreflightCapTimeoutMs(int timeout_ms) {
    if (childPid > 0 && timeout_ms > PF_POLL_MS) return PF_POLL_MS;
    return timeout_ms;
}

int reloadPreflightReapChild(pid_t pid, int status) {
    if (childPid <= 0 || pid != childPid) return 0;
    childDone(0, status);
    return 1;
}

void reloadPreflightShutdown(void) {
    killChild();
    cfgfilesDestruct(spawnSnap);
    spawnSnap = NULL;
    if (stats != NULL) statsobj.Destruct(&stats);
}
