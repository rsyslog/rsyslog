/* improg.c
 * This input plugin enables rsyslog to execute a program and
 * receive from it the message stream as standard input.
 * One message per line with a maximum size
 *
 * NOTE: read comments in module-template.h for more specifics!
 *
 * File begun on 2009-04-01 by RGerhards
 * File copied and adjust from improg.c on 2019-02-07 by Ph. Duveau
 *
 * Copyright 2009-2018 Adiscon GmbH.
 *
 * This file is contribution of rsyslog.
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
#include <stdio.h>
#include <syslog.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <pthread.h>
#include <poll.h>

/* Very strange error on solaris 11 LOG_CRON already defined here.
 * It is redefined in rsyslog.h
 * The error also appeared with module omprog but the warning is
 * accepted without generated an error
 */
#ifdef LOG_CRON
    #undef LOG_CRON
#endif

#include "rsyslog.h"
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "unicode-helper.h"
#include "errmsg.h"
#include "cfsysline.h"
#include "glbl.h"
#include "prop.h"
#include "ruleset.h"
#include "ratelimit.h"
#include "stringbuf.h"

MODULE_TYPE_INPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME("improg")

struct instanceConf_s {
    uchar *pszBinary; /* name of external program to call */
    char **aParams; /* optional parameters to pass to external program */
    int iParams; /* holds the count of parameters if set */
    uchar *pszTag;
    size_t lenTag;
    int iFacility;
    int iSeverity;
    int bConfirmMessages; /* does the program provide feedback via stdout? */
    int bSignalOnClose; /* should send SIGTERM to program before closing pipe? */
    long lCloseTimeout; /* how long to wait for program to terminate after closing pipe (ms) */
    int bKillUnresponsive; /* should send SIGKILL if closeTimeout is reached? */
    cstr_t *ppCStr;
    int bIsRunning; /* is program currently running? 0-no, 1-yes */
    pid_t pid; /* pid of currently running child process */
    int fdPipeToChild; /* fd for sending data to the program */
    int fdPipeFromChild; /* fd for receiving messages from the program, or -1 */
    uchar *pszBindRuleset;
    ruleset_t *pBindRuleset; /* ruleset to bind listener to (use system default if unspecified) */
    ratelimit_t *ratelimiter;
    struct instanceConf_s *next;
    struct instanceConf_s *prev;
};

/* config variables */
struct modConfData_s {
    rsconf_t *pConf; /* our overall config object */
};

/* internal structures
 */
DEF_OMOD_STATIC_DATA;
DEFobjCurrIf(glbl) DEFobjCurrIf(prop) DEFobjCurrIf(ruleset)

    static prop_t *pInputName = NULL;

#define NO_HUP_FORWARD -1 /* indicates that HUP should NOT be forwarded */
#define DEFAULT_CONFIRM_TIMEOUT_MS 10000
#define DEFAULT_CLOSE_TIMEOUT_MS 5000
#define RESPONSE_LINE_BUFFER_SIZE 4096
#define OUTPUT_CAPTURE_BUFFER_SIZE 4096
#define MAX_FD_TO_CLOSE 65535

static instanceConf_t *confRoot = NULL;
static fd_set rfds;
static int nfds = 0;

#include "im-helper.h" /* must be included AFTER the type definitions! */

static inline void std_checkRuleset_genErrMsg(__attribute__((unused)) modConfData_t *modConf, instanceConf_t *pInst) {
    LogError(0, NO_ERRCODE,
             "improg: ruleset '%s' for binary %s not found - "
             "using default ruleset instead",
             pInst->pszBindRuleset, pInst->pszBinary);
}

/* tables for interfacing with the v6 config system */
/* action (instance) parameters */
static struct cnfparamdescr inppdescr[] = {{"binary", eCmdHdlrString, CNFPARAM_REQUIRED},
                                           {"tag", eCmdHdlrString, CNFPARAM_REQUIRED},
                                           {"severity", eCmdHdlrSeverity, 0},
                                           {"facility", eCmdHdlrFacility, 0},
                                           {"ruleset", eCmdHdlrString, 0},
                                           {"confirmmessages", eCmdHdlrBinary, 0},
                                           {"signalonclose", eCmdHdlrBinary, 0},
                                           {"closetimeout", eCmdHdlrInt, 0},
                                           {"killunresponsive", eCmdHdlrBinary, 0}};
static struct cnfparamblk inppblk = {CNFPARAMBLK_VERSION, sizeof(inppdescr) / sizeof(struct cnfparamdescr), inppdescr};

/* execute the external program (must be called in child context after fork).
 */
static __attribute__((noreturn)) void execBinary(const instanceConf_t *pInst, int pipeToParent, int pipeFromParent) {
    int maxFd, fd, sigNum;
    struct sigaction sigAct;
    sigset_t sigSet;
    char errStr[1024];

    if (dup2(pipeToParent, STDOUT_FILENO) == -1) {
        goto failed;
    }

    if (pipeFromParent != -1) {
        if (dup2(pipeFromParent, STDIN_FILENO) == -1) {
            goto failed;
        }
    }

    /* close the file handles the child process doesn't need (all above STDERR).
     * The following way is simple and portable, though not perfect.
     * See https://stackoverflow.com/a/918469 for alternatives.
     */
    maxFd = sysconf(_SC_OPEN_MAX);
    if (maxFd < 0 || maxFd > MAX_FD_TO_CLOSE) {
        maxFd = MAX_FD_TO_CLOSE;
    }
#ifdef VALGRIND
    else {
        maxFd -= 10;
    }
#endif
    for (fd = STDERR_FILENO + 1; fd <= maxFd; ++fd) {
        close(fd);
    }

    /* reset signal handlers to default */
    memset(&sigAct, 0, sizeof(sigAct));
    sigemptyset(&sigAct.sa_mask);
    sigAct.sa_handler = SIG_DFL;
    for (sigNum = 1; sigNum < NSIG; ++sigNum) {
        sigaction(sigNum, &sigAct, NULL);
    }

    /* we need to block SIGINT, otherwise our program is cancelled when we are
     * stopped in debug mode.
     */
    sigAct.sa_handler = SIG_IGN;
    sigaction(SIGINT, &sigAct, NULL);
    sigemptyset(&sigSet);
    sigprocmask(SIG_SETMASK, &sigSet, NULL);

    alarm(0);

    /* finally exec program */
    execv((char *)pInst->pszBinary, pInst->aParams);

failed:
    /* an error occurred: log it and exit the child process. We use the
     * 'syslog' system call to log the error (we cannot use LogMsg/LogError,
     * since these functions add directly to the rsyslog input queue).
     */
    rs_strerror_r(errno, errStr, sizeof(errStr));
    DBGPRINTF("improg: failed to execute program '%s': %s\n", pInst->pszBinary, errStr);
    openlog("rsyslogd", 0, LOG_SYSLOG);
    syslog(LOG_ERR, "improg: failed to execute program '%s': %s\n", pInst->pszBinary, errStr);
    /* let's print the error to stderr for test bench purposes */
    fprintf(stderr, "improg: failed to execute program '%s': %s\n", pInst->pszBinary, errStr);
    exit(1);
}

/* creates a pipe and starts program, uses pipe as stdin for program.
 * rgerhards, 2009-04-01
 */
static rsRetVal openPipe(instanceConf_t *pInst) {
    int pipeFromChild[2] = {-1, -1};
    int pipeToChild[2] = {-1, -1};
    pid_t cpid;
    DEFiRet;

    /* if the 'confirmMessages' setting is enabled, open a pipe to send
         message confirmations to the program */
    if (pInst->bConfirmMessages && pipe(pipeToChild) == -1) {
        ABORT_FINALIZE(RS_RET_ERR_CREAT_PIPE);
    }

    /* open a pipe to receive messages to the program */
    if (pipe(pipeFromChild) == -1) {
        ABORT_FINALIZE(RS_RET_ERR_CREAT_PIPE);
    }

    DBGPRINTF("improg: executing program '%s' with '%d' parameters\n", pInst->pszBinary, pInst->iParams);

    cpid = fork();
    if (cpid == -1) {
        ABORT_FINALIZE(RS_RET_ERR_FORK);
    }

    if (cpid == 0) { /* we are now the child process: execute the program */
        /* close the pipe ends that the child doesn't need */
        close(pipeFromChild[0]);
        if (pipeToChild[1] != -1) {
            close(pipeToChild[1]);
        }

        execBinary(pInst, pipeFromChild[1], pipeToChild[0]);
        /* NO CODE HERE - WILL NEVER BE REACHED! */
    }

    DBGPRINTF("improg: child has pid %ld\n", (long int)cpid);

    /* close the pipe ends that the parent doesn't need */
    close(pipeFromChild[1]);
    if (pipeToChild[0] != -1) {
        close(pipeToChild[0]);
    }

    pInst->fdPipeToChild = pipeToChild[1]; /* we'll send messages confirmations to the program via this fd */
    pInst->fdPipeFromChild = pipeFromChild[0]; /* we'll receive message via this fd */

    FD_SET(pInst->fdPipeFromChild, &rfds); /* manage select read fd set */
    nfds = (nfds > pInst->fdPipeFromChild) ? nfds : pInst->fdPipeFromChild + 1;

    pInst->pid = cpid;
    pInst->bIsRunning = 1;

finalize_it:
    if (iRet != RS_RET_OK) {
        if (pipeFromChild[0] != -1) {
            close(pipeFromChild[0]);
            close(pipeFromChild[1]);
        }
        if (pipeToChild[0] != -1) {
            close(pipeToChild[0]);
            close(pipeToChild[1]);
        }
    }
    RETiRet;
}

static void waitForChild(instanceConf_t *pInst) {
    int status;
    int ret;
    long counter;

    counter = pInst->lCloseTimeout / 10;
    while ((ret = waitpid(pInst->pid, &status, WNOHANG)) == 0 && counter > 0) {
        srSleep(0, 10000); /* 0 seconds, 10 milliseconds */
        --counter;
    }

    if (ret == 0) { /* timeout reached */
        if (!pInst->bKillUnresponsive) {
            LogMsg(0, NO_ERRCODE, LOG_WARNING,
                   "improg: program '%s' (pid %ld) did not terminate "
                   "within timeout (%ld ms); ignoring it",
                   pInst->pszBinary, (long int)pInst->pid, pInst->lCloseTimeout);
            return;
        }

        LogMsg(0, NO_ERRCODE, LOG_WARNING,
               "improg: program '%s' (pid %ld) did not terminate "
               "within timeout (%ld ms); killing it",
               pInst->pszBinary, (long int)pInst->pid, pInst->lCloseTimeout);
        if (kill(pInst->pid, SIGKILL) == -1) {
            LogError(errno, RS_RET_SYS_ERR, "improg: could not send SIGKILL to child process");
            return;
        }

        ret = waitpid(pInst->pid, &status, 0);
    }

    /* waitpid will fail with errno == ECHILD if the child process has already
         been reaped by the rsyslogd main loop (see rsyslogd.c) */
    if (ret == pInst->pid) {
        glblReportChildProcessExit(runConf, pInst->pszBinary, pInst->pid, status);
    }
}

/* Send SIGTERM to child process if configured to do so, close pipe
 * and wait for child to terminate.
 */
static void terminateChild(instanceConf_t *pInst) {
    if (pInst->bIsRunning) {
        if (pInst->fdPipeFromChild != -1) {
            close(pInst->fdPipeFromChild);
            FD_CLR(pInst->fdPipeFromChild, &rfds);
            pInst->fdPipeFromChild = -1;
        }

        if (pInst->fdPipeToChild != -1) {
            close(pInst->fdPipeToChild);
            pInst->fdPipeToChild = -1;
        }

        /* wait for the child AFTER closing the pipe, so it receives EOF */
        waitForChild(pInst);

        pInst->bIsRunning = 0;
    }
}

static rsRetVal startChild(instanceConf_t *pInst) {
    DEFiRet;

    if (!pInst->bIsRunning) CHKiRet(openPipe(pInst));

finalize_it:
    if (iRet != RS_RET_OK && pInst->bIsRunning) {
        /* if initialization has failed, terminate program */
        terminateChild(pInst);
    }
    RETiRet;
}

static rsRetVal enqLine(instanceConf_t *const __restrict__ pInst) {
    DEFiRet;
    smsg_t *pMsg;

    if (cstrLen(pInst->ppCStr) == 0) {
        /* we do not process empty lines */
        FINALIZE;
    }

    CHKiRet(msgConstruct(&pMsg));

    MsgSetMSGoffs(pMsg, 0);
    MsgSetHOSTNAME(pMsg, glbl.GetLocalHostName(), ustrlen(glbl.GetLocalHostName()));
    MsgSetFlowControlType(pMsg, eFLOWCTL_FULL_DELAY);
    MsgSetInputName(pMsg, pInputName);
    MsgSetAPPNAME(pMsg, (const char *)pInst->pszTag);
    MsgSetTAG(pMsg, pInst->pszTag, pInst->lenTag);
    msgSetPRI(pMsg, pInst->iFacility | pInst->iSeverity);
    MsgSetRawMsg(pMsg, (const char *)rsCStrGetBufBeg(pInst->ppCStr), cstrLen(pInst->ppCStr));
    MsgSetRuleset(pMsg, pInst->pBindRuleset);

    ratelimitAddMsg(pInst->ratelimiter, NULL, pMsg);
finalize_it:
    RETiRet;
}

/* read line(s) from the external program and sent them when they are complete */
static rsRetVal readChild(instanceConf_t *const pInst) {
    char c;
    int retval;

    while ((retval = read(pInst->fdPipeFromChild, &c, 1)) == 1) {
        if (c == '\n') {
            enqLine(pInst);
            /* if confirm required then send an ACK to the program */
            if (pInst->bConfirmMessages) {
                if (write(pInst->fdPipeToChild, "ACK\n", sizeof("ACK\n") - 1) <= 0)
                    LogMsg(0, NO_ERRCODE, LOG_WARNING, "improg: pipe to child seems to be closed.");
            }
            rsCStrTruncate(pInst->ppCStr, rsCStrLen(pInst->ppCStr));
        } else {
            cstrAppendChar(pInst->ppCStr, c);
        }
    }
    return (retval == 0) ? RS_RET_OK : RS_RET_IO_ERROR;
}

/* create input instance, set default parameters, and
 * add it to the list of instances.
 */
static rsRetVal ATTR_NONNULL(1) createInstance(instanceConf_t **const ppInst) {
    instanceConf_t *pInst;
    DEFiRet;
    CHKmalloc(pInst = malloc(sizeof(instanceConf_t)));
    pInst->next = NULL;
    pInst->pszBindRuleset = NULL;
    pInst->pBindRuleset = NULL;
    pInst->ratelimiter = NULL;
    pInst->iSeverity = 5;
    pInst->iFacility = 128;

    pInst->pszTag = NULL;
    pInst->lenTag = 0;

    pInst->bIsRunning = 0;
    pInst->pid = -1;
    pInst->fdPipeToChild = -1;
    pInst->fdPipeFromChild = -1;

    pInst->pszBinary = NULL;
    pInst->aParams = NULL;
    pInst->iParams = 0;

    pInst->bConfirmMessages = 1;
    pInst->bSignalOnClose = 0;
    pInst->lCloseTimeout = 200;
    pInst->bKillUnresponsive = 1;

    *ppInst = pInst;
finalize_it:
    RETiRet;
}

/* This adds a new listener object to the bottom of the list, but
 * it does NOT initialize any data members except for the list
 * pointers themselves.
 */
static rsRetVal ATTR_NONNULL() lstnAdd(instanceConf_t *pInst) {
    DEFiRet;
    CHKiRet(ratelimitNew(&pInst->ratelimiter, "improg", (char *)pInst->pszBinary));

    /* insert it at the begin of the list */
    pInst->prev = NULL;
    pInst->next = confRoot;

    if (confRoot != NULL) confRoot->prev = pInst;

    confRoot = pInst;

finalize_it:
    RETiRet;
}

/* delete a listener object */
static void ATTR_NONNULL(1) lstnFree(instanceConf_t *pInst) {
    DBGPRINTF("lstnFree called for %s\n", pInst->pszBinary);
    if (pInst->ratelimiter != NULL) ratelimitDestruct(pInst->ratelimiter);
    if (pInst->pszBinary != NULL) free(pInst->pszBinary);
    if (pInst->pszTag) free(pInst->pszTag);
    if (pInst->pszBindRuleset != NULL) free(pInst->pszBindRuleset);
    if (pInst->aParams) {
        int i;
        for (i = 0; pInst->aParams[i]; i++) free(pInst->aParams[i]);
        free(pInst->aParams);
    }
    if (pInst->ppCStr) rsCStrDestruct(&pInst->ppCStr);
    free(pInst);
}

/* read  */
BEGINnewInpInst
    struct cnfparamvals *pvals;
    instanceConf_t *pInst = NULL;
    int i;
    CODESTARTnewInpInst;
    DBGPRINTF("newInpInst (improg)\n");

    pvals = nvlstGetParams(lst, &inppblk, NULL);
    if (pvals == NULL) {
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }

    if (Debug) {
        DBGPRINTF("input param blk in improg:\n");
        cnfparamsPrint(&inppblk, pvals);
    }

    CHKiRet(createInstance(&pInst));

    for (i = 0; i < inppblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;
        if (!strcmp(inppblk.descr[i].name, "binary")) {
            CHKiRet(split_binary_parameters(&pInst->pszBinary, &pInst->aParams, &pInst->iParams, pvals[i].val.d.estr));
        } else if (!strcmp(inppblk.descr[i].name, "tag")) {
            pInst->pszTag = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
            pInst->lenTag = es_strlen(pvals[i].val.d.estr);
        } else if (!strcmp(inppblk.descr[i].name, "ruleset")) {
            pInst->pszBindRuleset = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(inppblk.descr[i].name, "severity")) {
            pInst->iSeverity = pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "facility")) {
            pInst->iFacility = pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "confirmmessages")) {
            pInst->bConfirmMessages = pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "signalonclose")) {
            pInst->bSignalOnClose = pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "closetimeout")) {
            pInst->lCloseTimeout = pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "killunresponsive")) {
            pInst->bKillUnresponsive = pvals[i].val.d.n;
        } else {
            DBGPRINTF(
                "program error, non-handled "
                "param '%s'\n",
                inppblk.descr[i].name);
        }
    }
    if (pInst->pszBinary == NULL) {
        LogError(0, RS_RET_FILE_NOT_SPECIFIED, "ulogbase is not configured - no input will be gathered");
        ABORT_FINALIZE(RS_RET_FILE_NOT_SPECIFIED);
    }

    CHKiRet(cstrConstruct(&pInst->ppCStr));

    if ((iRet = lstnAdd(pInst)) != RS_RET_OK) {
        ABORT_FINALIZE(iRet);
    }

finalize_it:
    CODE_STD_FINALIZERnewInpInst if (pInst && iRet != RS_RET_OK) lstnFree(pInst);
    cnfparamvalsDestruct(pvals, &inppblk);
ENDnewInpInst

BEGINwillRun
    CODESTARTwillRun;
    /* we need to create the inputName property (only once during our lifetime) */
    CHKiRet(prop.Construct(&pInputName));
    CHKiRet(prop.SetString(pInputName, UCHAR_CONSTANT("improg"), sizeof("improg") - 1));
    CHKiRet(prop.ConstructFinalize(pInputName));
finalize_it:
ENDwillRun

BEGINrunInput
    struct timeval tv;
    int retval;
    instanceConf_t *pInst;
    CODESTARTrunInput;
    FD_ZERO(&rfds);

    for (pInst = confRoot; pInst != NULL; pInst = pInst->next) {
        startChild(pInst);
    }

    for (pInst = confRoot; pInst != NULL; pInst = pInst->next) {
        if (pInst->bIsRunning && pInst->fdPipeToChild > 0) {
            if (write(pInst->fdPipeToChild, "START\n", sizeof("START\n") - 1) <= 0)
                LogMsg(0, NO_ERRCODE, LOG_WARNING, "improg: pipe to child seems to be closed.");
            DBGPRINTF("Sending START to %s\n", pInst->pszBinary);
        }
    }

    /* main module loop */
    tv.tv_usec = 1000;
    while (glbl.GetGlobalInputTermState() == 0) {
        fd_set temp;
        memcpy(&temp, &rfds, sizeof(fd_set));
        tv.tv_sec = 0;

        /* wait for external data or 0.1 second */
        retval = select(nfds, &temp, NULL, NULL, &tv);

        /* retval is the number of fd with data to read */
        while (retval > 0) {
            for (pInst = confRoot; pInst != NULL; pInst = pInst->next) {
                if (FD_ISSET(pInst->fdPipeFromChild, &temp)) {
                    DBGPRINTF("read child %s\n", pInst->pszBinary);
                    readChild(pInst);
                    retval--;
                }
            }
        }
        tv.tv_usec = 100000;
    }
    DBGPRINTF("terminating upon request of rsyslog core\n");
ENDrunInput

/* This function is called by the framework after runInput() has been terminated. It
 * shall free any resources and prepare the module for unload.
 * CODEqueryEtryPt_STD_IMOD_QUERIES
 */
BEGINafterRun
    CODESTARTafterRun;
    instanceConf_t *pInst = confRoot, *nextInst;
    confRoot = NULL;

    DBGPRINTF("afterRun\n");

    while (pInst != NULL) {
        nextInst = pInst->next;

        if (pInst->bIsRunning) {
            if (pInst->bSignalOnClose) {
                kill(pInst->pid, SIGTERM);
                LogMsg(0, NO_ERRCODE, LOG_INFO, "%s SIGTERM signaled.", pInst->aParams[0]);
            }
            if (pInst->fdPipeToChild > 0) {
                if (write(pInst->fdPipeToChild, "STOP\n", strlen("STOP\n")) <= 0 && !pInst->bSignalOnClose)
                    LogMsg(0, NO_ERRCODE, LOG_WARNING, "improg: pipe to child seems to be closed.");
            }
            terminateChild(pInst);
        }

        lstnFree(pInst);

        pInst = nextInst;
    }

    if (pInputName != NULL) prop.Destruct(&pInputName);
ENDafterRun

BEGINisCompatibleWithFeature
    CODESTARTisCompatibleWithFeature;
    if (eFeat == sFEATURERepeatedMsgReduction) {
        iRet = RS_RET_OK;
    }
ENDisCompatibleWithFeature

BEGINbeginCnfLoad
    CODESTARTbeginCnfLoad;
    pModConf->pConf = pConf;
ENDbeginCnfLoad

BEGINendCnfLoad
    CODESTARTendCnfLoad;
ENDendCnfLoad

BEGINcheckCnf
    instanceConf_t *pInst;
    CODESTARTcheckCnf;
    for (pInst = confRoot; pInst != NULL; pInst = pInst->next) {
        std_checkRuleset(pModConf, pInst);
    }
ENDcheckCnf

BEGINactivateCnf
    CODESTARTactivateCnf;
ENDactivateCnf

BEGINfreeCnf
    CODESTARTfreeCnf;
ENDfreeCnf


BEGINmodExit
    CODESTARTmodExit;
    objRelease(ruleset, CORE_COMPONENT);
    objRelease(glbl, CORE_COMPONENT);
    objRelease(prop, CORE_COMPONENT);
ENDmodExit

BEGINqueryEtryPt
    CODESTARTqueryEtryPt;
    CODEqueryEtryPt_STD_IMOD_QUERIES;
    CODEqueryEtryPt_STD_CONF2_QUERIES;
    CODEqueryEtryPt_STD_CONF2_IMOD_QUERIES;
    CODEqueryEtryPt_IsCompatibleWithFeature_IF_OMOD_QUERIES;
ENDqueryEtryPt

BEGINmodInit()
    CODESTARTmodInit;
    *ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
    CODEmodInit_QueryRegCFSLineHdlr CHKiRet(objUse(ruleset, CORE_COMPONENT));
    CHKiRet(objUse(glbl, CORE_COMPONENT));
    CHKiRet(objUse(prop, CORE_COMPONENT));
ENDmodInit
