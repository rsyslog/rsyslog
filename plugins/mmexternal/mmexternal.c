/* mmexternal.c
 * This core plugin is an interface module to message modification
 * modules written in languages other than C.
 *
 * Copyright 2014-2018 by Rainer Gerhards
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
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include "rsyslog.h"
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "module-template.h"
#include "msg.h"
#include "errmsg.h"
#include "cfsysline.h"
#include "glbl.h"
#include "rsconf.h"


MODULE_TYPE_OUTPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME("mmexternal")

/* internal structures
 */
DEF_OMOD_STATIC_DATA;

typedef struct _instanceData {
    uchar *szBinary; /* name of binary to call */
    char **aParams; /* Optional Parameters for binary command */
    int iParams; /* Holds the count of parameters if set*/
    int bForceSingleInst; /* only a single wrkr instance of program permitted? */
    int inputProp; /* what to provide as input to the external program? */
#define INPUT_MSG 0
#define INPUT_RAWMSG 1
#define INPUT_JSON 2
    int outputMode; /* what to expect from the external program? */
#define OUTPUT_JSON 0
#define OUTPUT_NONE 1
    long responseTimeout; /* milliseconds to wait for JSON response, 0 means forever */
    uchar *outputFileName; /* name of file for std[out/err] or NULL if to discard */
    pthread_mutex_t mut; /* make sure only one instance is active */
} instanceData;

typedef struct wrkrInstanceData {
    instanceData *pData;
    pid_t pid; /* pid of currently running process */
    int fdOutput; /* it's fd (-1 if closed) */
    int fdPipeOut; /* file descriptor to write to */
    int fdPipeIn; /* fd we receive messages from the program (if we want to) */
    int bIsRunning; /* is binary currently running? 0-no, 1-yes */
    char *respBuf; /* buffer to read exernal plugin's response */
    int maxLenRespBuf; /* (current) maximum length of response buffer */
    int lenRespBuf; /* actual nbr of chars in response buffer */
    int idxRespBuf; /* last char read from response buffer */
} wrkrInstanceData_t;

typedef struct configSettings_s {
    uchar *szBinary; /* name of binary to call */
} configSettings_t;
static configSettings_t cs;


/* tables for interfacing with the v6 config system */
/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {
    {"binary", eCmdHdlrString, CNFPARAM_REQUIRED}, {"interface.input", eCmdHdlrString, 0},
    {"interface.output", eCmdHdlrString, 0},       {"output", eCmdHdlrString, 0},
    {"responsetimeout", eCmdHdlrNonNegInt, 0},     {"forcesingleinstance", eCmdHdlrBinary, 0}};
static struct cnfparamblk actpblk = {CNFPARAMBLK_VERSION, sizeof(actpdescr) / sizeof(struct cnfparamdescr), actpdescr};

BEGINinitConfVars /* (re)set config variables to default values */
    CODESTARTinitConfVars;
    cs.szBinary = NULL; /* name of binary to call */
ENDinitConfVars

/* config settings */

BEGINcreateInstance
    CODESTARTcreateInstance;
    pData->inputProp = INPUT_MSG;
    pData->outputMode = OUTPUT_JSON;
    pData->responseTimeout = 0;
    pthread_mutex_init(&pData->mut, NULL);
ENDcreateInstance

BEGINcreateWrkrInstance
    CODESTARTcreateWrkrInstance;
    pWrkrData->fdPipeIn = -1;
    pWrkrData->fdPipeOut = -1;
    pWrkrData->fdOutput = -1;
    pWrkrData->bIsRunning = 0;
    pWrkrData->respBuf = NULL;
    pWrkrData->maxLenRespBuf = 0;
    pWrkrData->lenRespBuf = 0;
    pWrkrData->idxRespBuf = 0;
ENDcreateWrkrInstance


BEGINisCompatibleWithFeature
    CODESTARTisCompatibleWithFeature;
    if (eFeat == sFEATURERepeatedMsgReduction) iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINfreeInstance
    int i;
    CODESTARTfreeInstance;
    pthread_mutex_destroy(&pData->mut);
    free(pData->szBinary);
    free(pData->outputFileName);
    if (pData->aParams != NULL) {
        for (i = 0; i < pData->iParams; i++) {
            free(pData->aParams[i]);
        }
        free(pData->aParams);
    }
ENDfreeInstance

BEGINfreeWrkrInstance
    CODESTARTfreeWrkrInstance;
    free(pWrkrData->respBuf);
ENDfreeWrkrInstance


BEGINdbgPrintInstInfo
    CODESTARTdbgPrintInstInfo;
ENDdbgPrintInstInfo


BEGINtryResume
    CODESTARTtryResume;
ENDtryResume


/* As this is just a debug function, we only make
 * best effort to write the message but do *not* try very
 * hard to handle errors. -- rgerhards, 2014-01-16
 */
static void writeOutputDebug(wrkrInstanceData_t *__restrict__ const pWrkrData,
                             const char *__restrict__ const buf,
                             const ssize_t lenBuf) {
    char errStr[1024];
    ssize_t r;

    if (pWrkrData->pData->outputFileName == NULL) goto done;

    if (pWrkrData->fdOutput == -1) {
        pWrkrData->fdOutput = open((char *)pWrkrData->pData->outputFileName, O_WRONLY | O_APPEND | O_CREAT, 0600);
        if (pWrkrData->fdOutput == -1) {
            DBGPRINTF("mmexternal: error opening output file %s: %s\n", pWrkrData->pData->outputFileName,
                      rs_strerror_r(errno, errStr, sizeof(errStr)));
            goto done;
        }
    }

    r = write(pWrkrData->fdOutput, buf, (size_t)lenBuf);
    if (r != lenBuf) {
        DBGPRINTF(
            "mmexternal: problem writing output file %s: bytes "
            "requested %lld, written %lld, msg: %s\n",
            pWrkrData->pData->outputFileName, (long long)lenBuf, (long long)r,
            rs_strerror_r(errno, errStr, sizeof(errStr)));
    }
done:
    return;
}


static void waitForChildExit(wrkrInstanceData_t *__restrict__ const pWrkrData, const long timeoutMs) {
    int status;
    int ret = -1;
    long counter;

    counter = timeoutMs / 10;
    while ((ret = waitpid(pWrkrData->pid, &status, WNOHANG)) == 0 && counter > 0) {
        srSleep(0, 10000);
        --counter;
    }

    if (ret == 0) {
        if (kill(pWrkrData->pid, SIGKILL) == -1) {
            LogError(errno, RS_RET_SYS_ERR, "mmexternal: could not send SIGKILL to child process");
            return;
        }
        ret = waitpid(pWrkrData->pid, &status, 0);
    }

    /* waitpid may fail with ECHILD if rsyslogd's main loop already reaped the child. */
    if (ret == pWrkrData->pid) {
        glblReportChildProcessExit(runConf, pWrkrData->pData->szBinary, pWrkrData->pid, status);
    }
}


static void closeWrkrPipes(wrkrInstanceData_t *__restrict__ const pWrkrData) {
    if (pWrkrData->fdOutput != -1) {
        close(pWrkrData->fdOutput);
        pWrkrData->fdOutput = -1;
    }
    if (pWrkrData->fdPipeIn != -1) {
        close(pWrkrData->fdPipeIn);
        pWrkrData->fdPipeIn = -1;
    }
    if (pWrkrData->fdPipeOut != -1) {
        close(pWrkrData->fdPipeOut);
        pWrkrData->fdPipeOut = -1;
    }
}


static void terminateChild(wrkrInstanceData_t *__restrict__ const pWrkrData, const long timeoutMs) {
    if (pWrkrData->bIsRunning == 0) return;

    if (kill(pWrkrData->pid, SIGTERM) == -1) {
        LogError(errno, RS_RET_SYS_ERR, "mmexternal: could not send SIGTERM to child process");
    }
    closeWrkrPipes(pWrkrData);
    waitForChildExit(pWrkrData, timeoutMs);
    pWrkrData->bIsRunning = 0;
}


/* Get reply from external program. Note that we *must* receive one
 * reply for each message sent (half-duplex protocol). As such, the last
 * char we read MUST be \n ... we cannot have multiple LF as this is
 * forbidden by the plugin interface. We cannot have multiple responses
 * for multiple messages, as we are in half-duplex mode! This makes
 * things quite a bit simpler. So don't think the simple code does
 * not handle those border-cases that are describe to cannot exist!
 */
static rsRetVal processProgramReply(wrkrInstanceData_t *__restrict__ const pWrkrData, smsg_t *const pMsg) {
    DEFiRet;
    char errStr[1024];
    ssize_t r;
    int numCharsRead;
    char *newptr;
    struct pollfd fdToPoll[1];
    struct timespec responseDeadline;
    long pollTimeout;

    if (pWrkrData->pData->outputMode == OUTPUT_NONE) {
        FINALIZE;
    }

    numCharsRead = 0;
    fdToPoll[0].fd = pWrkrData->fdPipeIn;
    fdToPoll[0].events = POLLIN;
    if (pWrkrData->pData->responseTimeout > 0) {
        timeoutComp(&responseDeadline, pWrkrData->pData->responseTimeout);
    }
    do {
        if (pWrkrData->maxLenRespBuf < numCharsRead + 256) { /* 256 to permit at least a decent read */
            pWrkrData->maxLenRespBuf += 4096;
            if ((newptr = realloc(pWrkrData->respBuf, pWrkrData->maxLenRespBuf)) == NULL) {
                LogError(errno, RS_RET_OUT_OF_MEMORY, "mmexternal: error reallocating response buffer");
                ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
            }
            pWrkrData->respBuf = newptr;
        }
        if (pWrkrData->pData->responseTimeout > 0) {
            pollTimeout = timeoutVal(&responseDeadline);
            if (pollTimeout == 0) {
                LogMsg(0, RS_RET_TIMED_OUT, LOG_WARNING,
                       "mmexternal: program '%s' (pid %ld) did not respond within timeout (%ld ms); "
                       "will be restarted and current message skipped",
                       pWrkrData->pData->szBinary, (long)pWrkrData->pid, pWrkrData->pData->responseTimeout);
                terminateChild(pWrkrData, 1000);
                FINALIZE;
            }
            if (pollTimeout > INT_MAX) {
                pollTimeout = INT_MAX;
            }
            const int pollRet = poll(fdToPoll, 1, (int)pollTimeout);
            if (pollRet == -1) {
                if (errno == EINTR) continue;
                LogError(errno, RS_RET_SYS_ERR, "mmexternal: error polling for response from program");
                ABORT_FINALIZE(RS_RET_SYS_ERR);
            } else if (pollRet == 0) {
                LogMsg(0, RS_RET_TIMED_OUT, LOG_WARNING,
                       "mmexternal: program '%s' (pid %ld) did not respond within timeout (%ld ms); "
                       "will be restarted and current message skipped",
                       pWrkrData->pData->szBinary, (long)pWrkrData->pid, pWrkrData->pData->responseTimeout);
                terminateChild(pWrkrData, 1000);
                FINALIZE;
            }
        }
        r = read(pWrkrData->fdPipeIn, pWrkrData->respBuf + numCharsRead, pWrkrData->maxLenRespBuf - numCharsRead - 1);
        if (r > 0) {
            numCharsRead += r;
            pWrkrData->respBuf[numCharsRead] = '\0'; /* space reserved in read! */
        } else {
            /* emergency - fake no update */
            memcpy(pWrkrData->respBuf, "{}\n", sizeof("{}\n"));
            numCharsRead = 3;
        }
        if (Debug && r == -1) {
            DBGPRINTF("mmexternal: error reading from external program: %s\n",
                      rs_strerror_r(errno, errStr, sizeof(errStr)));
        }
    } while (pWrkrData->respBuf[numCharsRead - 1] != '\n');

    writeOutputDebug(pWrkrData, pWrkrData->respBuf, numCharsRead);
    /* strip LF, which is not part of the JSON message but framing */
    pWrkrData->respBuf[numCharsRead - 1] = '\0';
    iRet = MsgSetPropsViaJSON(pMsg, (uchar *)pWrkrData->respBuf);
    if (iRet != RS_RET_OK) {
        LogError(0, iRet, "mmexternal: invalid reply '%s' from program '%s'", pWrkrData->respBuf,
                 pWrkrData->pData->szBinary);
        iRet = RS_RET_OK;
    }

finalize_it:
    RETiRet;
}


/* execute the child process (must be called in child context
 * after fork).
 * Note: all output will go to std[err/out] of the **child**, so
 * rsyslog will never see it except as script output. Do NOT
 * use dbgprintf() or LogError() and friends.
 */
static void __attribute__((noreturn)) execBinary(wrkrInstanceData_t *pWrkrData,
                                                 const int fdStdin,
                                                 const int fdStdOutErr) {
    int i;
    int fdDevNull = -1;
    struct sigaction sigAct;
    sigset_t set;
    char *newenviron[] = {NULL};

    if (dup2(fdStdin, STDIN_FILENO) == -1) {
        perror("mmexternal: dup() stdin failed\n");
    }
    if (fdStdOutErr == -1) {
        fdDevNull = open("/dev/null", O_WRONLY);
        if (fdDevNull == -1) {
            perror("mmexternal: open(/dev/null) failed\n");
        }
    }
    const int targetFd = (fdStdOutErr == -1) ? fdDevNull : fdStdOutErr;
    if (targetFd != -1) {
        if (dup2(targetFd, STDOUT_FILENO) == -1) {
            perror("mmexternal: dup() stdout failed\n");
        }
        if (dup2(targetFd, STDERR_FILENO) == -1) {
            perror("mmexternal: dup() stderr failed\n");
        }
    }

    /* we close all file handles as we fork soon
     * Is there a better way to do this? - mail me! rgerhards@adiscon.com
     */
#ifndef VALGRIND /* we can not use this with valgrind - too many errors... */
    for (i = 3; i <= 65535; ++i) close(i);
#endif

    /* reset signal handlers to default */
    memset(&sigAct, 0, sizeof(sigAct));
    sigemptyset(&sigAct.sa_mask);
    sigAct.sa_handler = SIG_DFL;
    for (i = 1; i < NSIG; ++i) sigaction(i, &sigAct, NULL);
    /* we need to block SIGINT, otherwise the external program is cancelled when we are
     * stopped in debug mode.
     */
    sigAct.sa_handler = SIG_IGN;
    sigaction(SIGINT, &sigAct, NULL);
    sigemptyset(&set);
    sigprocmask(SIG_SETMASK, &set, NULL);

    alarm(0);

    /* finally exec child */
    execve((char *)pWrkrData->pData->szBinary, pWrkrData->pData->aParams, newenviron);

    /* we should never reach this point, but if we do, we complain and terminate */
    char errstr[1024];
    char errbuf[2048];
    rs_strerror_r(errno, errstr, sizeof(errstr));
    errstr[sizeof(errstr) - 1] = '\0';
    const size_t lenbuf = snprintf(errbuf, sizeof(errbuf), "mmexternal: failed to execute binary '%s': %s\n",
                                   pWrkrData->pData->szBinary, errstr);
    errbuf[sizeof(errbuf) - 1] = '\0';
    if (write(2, errbuf, lenbuf) != (ssize_t)lenbuf) {
        /* just keep static analyzers happy... */
        exit(2);
    }
    exit(1);
}


/* creates a pipe and starts program, uses pipe as stdin for program.
 * rgerhards, 2009-04-01
 */
static rsRetVal openPipe(wrkrInstanceData_t *pWrkrData) {
    int pipestdin[2] = {-1, -1};
    int pipestdout[2] = {-1, -1};
    const int useOutputPipe = pWrkrData->pData->outputMode != OUTPUT_NONE;
    pid_t cpid = -1;
    DEFiRet;

    if (pipe(pipestdin) == -1) {
        ABORT_FINALIZE(RS_RET_ERR_CREAT_PIPE);
    }
    if (useOutputPipe && pipe(pipestdout) == -1) {
        ABORT_FINALIZE(RS_RET_ERR_CREAT_PIPE);
    }

    DBGPRINTF("mmexternal: executing program '%s' with '%d' parameters\n", pWrkrData->pData->szBinary,
              pWrkrData->pData->iParams);

    /* final sanity check */
    assert(pWrkrData->pData->szBinary != NULL);
    assert(pWrkrData->pData->aParams != NULL);

    /* NO OUTPUT AFTER FORK! */
    cpid = fork();
    if (cpid == -1) {
        ABORT_FINALIZE(RS_RET_ERR_FORK);
    }
    pWrkrData->pid = cpid;

    if (cpid == 0) {
        /* we are now the child, just exec the binary. */
        close(pipestdin[1]); /* close those pipe "ports" that */
        if (useOutputPipe) {
            close(pipestdout[0]); /* we don't need */
            execBinary(pWrkrData, pipestdin[0], pipestdout[1]);
        } else {
            execBinary(pWrkrData, pipestdin[0], -1);
        }
        /*NO CODE HERE - WILL NEVER BE REACHED!*/
    }

    DBGPRINTF("mmexternal: child has pid %d\n", (int)cpid);
    if (useOutputPipe) {
        pWrkrData->fdPipeIn = pipestdout[0];
        pipestdout[0] = -1;
        close(pipestdout[1]);
        pipestdout[1] = -1;
    }
    close(pipestdin[0]);
    pipestdin[0] = -1;
    pWrkrData->pid = cpid;
    pWrkrData->fdPipeOut = pipestdin[1];
    pipestdin[1] = -1;
    pWrkrData->bIsRunning = 1;
finalize_it:
    if (pipestdin[0] != -1) close(pipestdin[0]);
    if (pipestdin[1] != -1) close(pipestdin[1]);
    if (pipestdout[0] != -1) close(pipestdout[0]);
    if (pipestdout[1] != -1) close(pipestdout[1]);
    RETiRet;
}


/* clean up after a terminated child
 */
static rsRetVal cleanup(wrkrInstanceData_t *pWrkrData) {
    int status;
    int ret;
    DEFiRet;

    assert(pWrkrData->bIsRunning == 1);
    ret = waitpid(pWrkrData->pid, &status, 0);

    /* waitpid will fail with errno == ECHILD if the child process has already
       been reaped by the rsyslogd main loop (see rsyslogd.c) */
    if (ret == pWrkrData->pid) {
        glblReportChildProcessExit(runConf, pWrkrData->pData->szBinary, pWrkrData->pid, status);
    }

    closeWrkrPipes(pWrkrData);
    pWrkrData->bIsRunning = 0;
    RETiRet;
}


/* try to restart the binary when it has stopped.
 */
static rsRetVal tryRestart(wrkrInstanceData_t *pWrkrData) {
    DEFiRet;
    assert(pWrkrData->bIsRunning == 0);

    iRet = openPipe(pWrkrData);
    RETiRet;
}

/* write to pipe
 * note that we do not try to run block-free. If the users fears something
 * may block (and this not be acceptable), the action should be run on its
 * own action queue.
 */
static rsRetVal callExtProg(wrkrInstanceData_t *__restrict__ const pWrkrData, smsg_t *__restrict__ const pMsg) {
    int lenWritten;
    int lenWrite;
    int writeOffset;
    int i_iov;
    struct iovec iov[2];
    int bFreeInputstr = 1; /* we must only free if it does not point to msg-obj mem! */
    const uchar *inputstr = NULL; /* string to be processed by external program */
    DEFiRet;

    if (pWrkrData->pData->inputProp == INPUT_MSG) {
        inputstr = getMSG(pMsg);
        lenWrite = getMSGLen(pMsg);
        bFreeInputstr = 0;
    } else if (pWrkrData->pData->inputProp == INPUT_RAWMSG) {
        getRawMsg(pMsg, (uchar **)&inputstr, &lenWrite);
        bFreeInputstr = 0;
    } else {
        inputstr = msgGetJSONMESG(pMsg);
        lenWrite = strlen((const char *)inputstr);
    }

    writeOffset = 0;
    do {
        DBGPRINTF("mmexternal: writing to prog (fd %d, offset %d): %s\n", pWrkrData->fdPipeOut, (int)writeOffset,
                  inputstr);
        i_iov = 0;
        if (writeOffset < lenWrite) {
            iov[0].iov_base = (char *)inputstr + writeOffset;
            iov[0].iov_len = lenWrite - writeOffset;
            ++i_iov;
        }
        iov[i_iov].iov_base = (void *)"\n";
        iov[i_iov].iov_len = 1;
        lenWritten = writev(pWrkrData->fdPipeOut, iov, i_iov + 1);
        if (lenWritten == -1) {
            switch (errno) {
                case EPIPE:
                    LogMsg(0, RS_RET_ERR_WRITE_PIPE, LOG_WARNING,
                           "mmexternal: program '%s' (pid %ld) terminated; will be restarted",
                           pWrkrData->pData->szBinary, (long)pWrkrData->pid);
                    CHKiRet(cleanup(pWrkrData));
                    CHKiRet(tryRestart(pWrkrData));
                    writeOffset = 0;
                    break;
                default:
                    LogError(errno, RS_RET_ERR_WRITE_PIPE, "mmexternal: error sending message to program");
                    ABORT_FINALIZE(RS_RET_ERR_WRITE_PIPE);
                    break;
            }
        } else {
            writeOffset += lenWritten;
        }
    } while (lenWritten != lenWrite + 1);

    CHKiRet(processProgramReply(pWrkrData, pMsg));

finalize_it:
    /* we need to free json input strings, only. All others point to memory
     * inside the msg object, which is destroyed when the msg is destroyed.
     */
    if (bFreeInputstr) {
        free((void *)inputstr);
    }
    RETiRet;
}


BEGINdoAction_NoStrings
    smsg_t **ppMsg = (smsg_t **)pMsgData;
    smsg_t *pMsg = ppMsg[0];
    instanceData *pData;
    CODESTARTdoAction;
    pData = pWrkrData->pData;
    if (pData->bForceSingleInst) pthread_mutex_lock(&pData->mut);
    if (pWrkrData->bIsRunning == 0) {
        openPipe(pWrkrData);
    }

    iRet = callExtProg(pWrkrData, pMsg);

    if (iRet != RS_RET_OK) iRet = RS_RET_SUSPENDED;
    if (pData->bForceSingleInst) pthread_mutex_unlock(&pData->mut);
ENDdoAction


static void setInstParamDefaults(instanceData *pData) {
    pData->szBinary = NULL;
    pData->aParams = NULL;
    pData->outputFileName = NULL;
    pData->iParams = 0;
    pData->bForceSingleInst = 0;
    pData->inputProp = INPUT_MSG;
    pData->outputMode = OUTPUT_JSON;
    pData->responseTimeout = 0;
}


BEGINnewActInst
    struct cnfparamvals *pvals;
    int i;
    const char *inputCStr = NULL;
    const char *outputCStr = NULL;
    CODESTARTnewActInst;
    if ((pvals = nvlstGetParams(lst, &actpblk, NULL)) == NULL) {
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }

    CHKiRet(createInstance(&pData));
    setInstParamDefaults(pData);

    CODE_STD_STRING_REQUESTnewActInst(1);
    for (i = 0; i < actpblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;
        if (!strcmp(actpblk.descr[i].name, "binary")) {
            CHKiRet(split_binary_parameters(&pData->szBinary, &pData->aParams, &pData->iParams, pvals[i].val.d.estr));
        } else if (!strcmp(actpblk.descr[i].name, "output")) {
            CHKmalloc(pData->outputFileName = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(actpblk.descr[i].name, "responsetimeout")) {
            pData->responseTimeout = pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "forcesingleinstance")) {
            pData->bForceSingleInst = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "interface.input")) {
            inputCStr = es_str2cstr(pvals[i].val.d.estr, NULL);
            CHKmalloc(inputCStr);
            if (!strcmp(inputCStr, "msg"))
                pData->inputProp = INPUT_MSG;
            else if (!strcmp(inputCStr, "rawmsg"))
                pData->inputProp = INPUT_RAWMSG;
            else if (!strcmp(inputCStr, "fulljson"))
                pData->inputProp = INPUT_JSON;
            else {
                LogError(0, RS_RET_INVLD_INTERFACE_INPUT, "mmexternal: invalid interface.input parameter '%s'",
                         inputCStr);
                ABORT_FINALIZE(RS_RET_INVLD_INTERFACE_INPUT);
            }
        } else if (!strcmp(actpblk.descr[i].name, "interface.output")) {
            outputCStr = es_str2cstr(pvals[i].val.d.estr, NULL);
            CHKmalloc(outputCStr);
            if (!strcmp(outputCStr, "json"))
                pData->outputMode = OUTPUT_JSON;
            else if (!strcmp(outputCStr, "none"))
                pData->outputMode = OUTPUT_NONE;
            else {
                LogError(0, RS_RET_INVLD_INTERFACE_INPUT, "mmexternal: invalid interface.output parameter '%s'",
                         outputCStr);
                ABORT_FINALIZE(RS_RET_INVLD_INTERFACE_INPUT);
            }
        } else {
            DBGPRINTF("mmexternal: program error, non-handled param '%s'\n", actpblk.descr[i].name);
        }
    }

    CHKiRet(OMSRsetEntry(*ppOMSR, 0, NULL, OMSR_TPL_AS_MSG));
    DBGPRINTF("mmexternal: bForceSingleInst %d\n", pData->bForceSingleInst);
    DBGPRINTF("mmexternal: interface.input '%s', mode %d\n", inputCStr == NULL ? "msg" : inputCStr, pData->inputProp);
    DBGPRINTF("mmexternal: interface.output '%s', mode %d, responseTimeout %ld\n",
              outputCStr == NULL ? "json" : outputCStr, pData->outputMode, pData->responseTimeout);
    CODE_STD_FINALIZERnewActInst;
    free((void *)inputCStr);
    free((void *)outputCStr);
    cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst

NO_LEGACY_CONF_parseSelectorAct


    BEGINmodExit CODESTARTmodExit;
free(cs.szBinary);
cs.szBinary = NULL;
ENDmodExit


BEGINqueryEtryPt
    CODESTARTqueryEtryPt;
    CODEqueryEtryPt_STD_OMOD_QUERIES;
    CODEqueryEtryPt_STD_OMOD8_QUERIES;
    CODEqueryEtryPt_STD_CONF2_CNFNAME_QUERIES;
    CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES;
ENDqueryEtryPt

BEGINmodInit()
    CODESTARTmodInit;
    INITLegCnfVars;
    *ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
    CODEmodInit_QueryRegCFSLineHdlr CODEmodInit_QueryRegCFSLineHdlr
ENDmodInit
