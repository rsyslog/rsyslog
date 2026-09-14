/* omtesting.c
 *
 * This module is a testing aid. It is not meant to be used in production. I have
 * initially written it to introduce delays of custom length to action processing.
 * This is needed for development of new message queueing methods. However, I think
 * there are other uses for this module. For example, I can envision that it is a good
 * thing to have an output module that requests a retry on every "n"th invocation
 * and such things. I implement only what I need. But should further testing needs
 * arise, it makes much sense to add them here.
 *
 * This module will become part of the CVS and the rsyslog project because I think
 * it is a generally useful debugging, testing and development aid for everyone
 * involved with rsyslog.
 *
 * CURRENT SUPPORTED COMMANDS:
 *
 * :omtesting:sleep <seconds> <microseconds>
 * :omtesting:file_barrier <entered-file> <release-fifo>
 *
 * Must be specified exactly as above. Keep in mind microseconds are a millionth
 * of a second!
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * Copyright 2007-2017 Rainer Gerhards and Adiscon GmbH.
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
#include "rsyslog.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include "dirty.h"
#include "syslogd-types.h"
#include "module-template.h"
#include "conf.h"
#include "cfsysline.h"
#include "srUtils.h"

#ifndef O_CLOEXEC
    #define O_CLOEXEC 0
#endif

MODULE_TYPE_OUTPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME("omtesting")

/* internal structures
 */
DEF_OMOD_STATIC_DATA;

typedef struct _instanceData {
    enum {
        MD_SLEEP,
        MD_FAIL,
        MD_RANDFAIL,
        MD_ALWAYS_SUSPEND,
        MD_BARRIER_ERROR,
        MD_BARRIER_SUSPEND,
        MD_FILE_BARRIER
    } mode;
    int bEchoStdout;
    int iWaitSeconds;
    int iWaitUSeconds; /* micro-seconds (one millionth of a second, just to make sure...) */
    int iCurrCallNbr;
    int iFailFrequency;
    int iResumeAfter;
    int iCurrRetries;
    int bFailed; /* indicates if we are already in failed state - this is necessary
                  * to work properly together with multiple worker instances.
                  */
    int barrier_target;
    int barrier_count;
    int barrier_triggered;
    char *barrier_enter_file;
    char *barrier_release_fifo;
    pthread_mutex_t mut;
    pthread_cond_t barrier_cond;
} instanceData;

typedef struct wrkrInstanceData {
    instanceData *pData;
} wrkrInstanceData_t;

typedef struct configSettings_s {
    int bEchoStdout; /* echo non-failed messages to stdout */
} configSettings_t;
static configSettings_t cs;

BEGINinitConfVars /* (re)set config variables to default values */
    CODESTARTinitConfVars;
    cs.bEchoStdout = 0;
ENDinitConfVars

BEGINcreateInstance
    CODESTARTcreateInstance;
    pData->iWaitSeconds = 1;
    pData->iWaitUSeconds = 0;
    pthread_mutex_init(&pData->mut, NULL);
    pthread_cond_init(&pData->barrier_cond, NULL);
ENDcreateInstance


BEGINcreateWrkrInstance
    CODESTARTcreateWrkrInstance;
ENDcreateWrkrInstance


BEGINdbgPrintInstInfo
    CODESTARTdbgPrintInstInfo;
    dbgprintf("Action delays rule by %d second(s) and %d microsecond(s)\n", pData->iWaitSeconds, pData->iWaitUSeconds);
    /* do nothing */
ENDdbgPrintInstInfo


BEGINisCompatibleWithFeature
    CODESTARTisCompatibleWithFeature;
    /* we are not compatible with repeated msg reduction feature, so do not allow it */
ENDisCompatibleWithFeature


/* implement "fail" command in retry processing */
static rsRetVal doFailOnResume(instanceData *pData) {
    DEFiRet;

    dbgprintf("fail retry curr %d, max %d\n", pData->iCurrRetries, pData->iResumeAfter);
    if (++pData->iCurrRetries == pData->iResumeAfter) {
        iRet = RS_RET_OK;
        pData->bFailed = 0;
    } else {
        iRet = RS_RET_SUSPENDED;
    }

    RETiRet;
}


/* implement "fail" command */
static rsRetVal doFail(instanceData *pData) {
    DEFiRet;

    dbgprintf("fail curr %d, frequency %d, bFailed %d\n", pData->iCurrCallNbr, pData->iFailFrequency, pData->bFailed);
    if (pData->bFailed) {
        ABORT_FINALIZE(RS_RET_SUSPENDED);
    } else {
        if (pData->iCurrCallNbr++ % pData->iFailFrequency == 0) {
            pData->iCurrRetries = 0;
            pData->bFailed = 1;
            iRet = RS_RET_SUSPENDED;
        }
    }
finalize_it:
    RETiRet;
}


/* implement "sleep" command */
static rsRetVal doSleep(instanceData *pData) {
    DEFiRet;
    struct timeval tvSelectTimeout;

    dbgprintf("sleep(%d, %d)\n", pData->iWaitSeconds, pData->iWaitUSeconds);
    tvSelectTimeout.tv_sec = pData->iWaitSeconds;
    tvSelectTimeout.tv_usec = pData->iWaitUSeconds; /* microseconds */
    select(0, NULL, NULL, NULL, &tvSelectTimeout);
    RETiRet;
}


/* implement "randomfail" command */
static rsRetVal doRandFail(void) {
    DEFiRet;
    if ((randomNumber() >> 4) < (RAND_MAX >> 5)) { /* rougly same probability */
        iRet = RS_RET_OK;
        dbgprintf("omtesting randfail: succeeded this time\n");
    } else {
        iRet = RS_RET_SUSPENDED;
        dbgprintf("omtesting randfail: failed this time\n");
    }
    RETiRet;
}


/* The queue-shutdown path may cancel an action worker while it is waiting for
 * a test release.  Make the descriptor cancellation-safe so a test cannot
 * leave a FIFO reader behind after that expected cancellation.
 */
static void fileBarrierCloseFd(void *const arg) {
    int *const fd = arg;
    if (*fd >= 0) {
        close(*fd);
        *fd = -1;
    }
}


static int writeFully(const int fd, const char *buf, size_t len) {
    while (len != 0) {
        const ssize_t written = write(fd, buf, len);
        if (written < 0) {
            if (errno == EINTR) continue;
            return 0;
        }
        if (written == 0) return 0;
        buf += written;
        len -= (size_t)written;
    }
    return 1;
}


/* Publish entry before blocking on the test-owned FIFO.  The test opens its
 * writer only after it has observed that file, so the FIFO release is an
 * explicit phase transition rather than a timing delay.
 */
static rsRetVal doFileBarrier(instanceData *const pData) {
    /* pthread_cleanup_push() may be a setjmp-based macro. Keep the return
     * state volatile across the cancellation cleanup regions. */
    volatile rsRetVal iRet = RS_RET_OK;
    int fd;
    ssize_t nread;
    char release[32];

    fd = open(pData->barrier_enter_file, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (fd < 0) {
        LogError(errno, RS_RET_ERR, "omtesting file_barrier cannot publish entry to '%s'", pData->barrier_enter_file);
        ABORT_FINALIZE(RS_RET_ERR);
    }
    pthread_cleanup_push(fileBarrierCloseFd, &fd);
    if (!writeFully(fd, "entered\n", sizeof("entered\n") - 1)) {
        LogError(errno, RS_RET_ERR, "omtesting file_barrier cannot write entry to '%s'", pData->barrier_enter_file);
        iRet = RS_RET_ERR;
    }
    pthread_cleanup_pop(1);
    if (iRet != RS_RET_OK) goto finalize_it;

    fd = open(pData->barrier_release_fifo, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        LogError(errno, RS_RET_ERR, "omtesting file_barrier cannot open release FIFO '%s'",
                 pData->barrier_release_fifo);
        ABORT_FINALIZE(RS_RET_ERR);
    }
    pthread_cleanup_push(fileBarrierCloseFd, &fd);
    do {
        nread = read(fd, release, sizeof(release));
    } while (nread < 0 && errno == EINTR);
    if (nread <= 0) {
        LogError(nread < 0 ? errno : 0, RS_RET_ERR, "omtesting file_barrier received no release from '%s'",
                 pData->barrier_release_fifo);
        iRet = RS_RET_ERR;
    }
    pthread_cleanup_pop(1);

finalize_it:
    RETiRet;
}


/* Synchronize action workers so TSan tests can exercise concurrent core paths.
 * Returns one for every worker in the first complete barrier generation and
 * zero for later calls.
 */
static int barrier_trigger(instanceData *const pData) {
    volatile int triggered = 0;

    pthread_mutex_lock(&pData->mut);
    pthread_cleanup_push(mutexCancelCleanup, &pData->mut);
    if (!pData->barrier_triggered) {
        ++pData->barrier_count;
        if (pData->barrier_count == pData->barrier_target) {
            pData->barrier_triggered = 1;
            pthread_cond_broadcast(&pData->barrier_cond);
        } else {
            while (!pData->barrier_triggered) {
                pthread_cond_wait(&pData->barrier_cond, &pData->mut);
            }
        }
        triggered = 1;
    }
    pthread_cleanup_pop(1);

    return triggered;
}


BEGINtryResume
    CODESTARTtryResume;
    dbgprintf("omtesting tryResume() called\n");
    pthread_mutex_lock(&pWrkrData->pData->mut);
    switch (pWrkrData->pData->mode) {
        case MD_SLEEP:
            break;
        case MD_FAIL:
            iRet = doFailOnResume(pWrkrData->pData);
            break;
        case MD_RANDFAIL:
            iRet = doRandFail();
            break;
        case MD_ALWAYS_SUSPEND:
            iRet = RS_RET_SUSPENDED;
            break;
        case MD_BARRIER_ERROR:
        case MD_BARRIER_SUSPEND:
        case MD_FILE_BARRIER:
            iRet = RS_RET_OK;
            break;
        default:
            // No action needed for other cases
            break;
    }
    pthread_mutex_unlock(&pWrkrData->pData->mut);
    dbgprintf("omtesting tryResume() returns iRet %d\n", iRet);
ENDtryResume


BEGINdoAction
    instanceData *pData;
    CODESTARTdoAction;
    dbgprintf("omtesting received msg '%s'\n", ppString[0]);
    pData = pWrkrData->pData;
    if (pData->mode == MD_FILE_BARRIER) {
        iRet = doFileBarrier(pData);
    } else if (pData->mode == MD_BARRIER_ERROR || pData->mode == MD_BARRIER_SUSPEND) {
        const int triggered = barrier_trigger(pData);
        if (triggered && pData->mode == MD_BARRIER_ERROR) {
            LogError(0, RS_RET_ERR, "omtesting synchronized error");
        } else if (triggered) {
            iRet = RS_RET_SUSPENDED;
        }
    } else {
        pthread_mutex_lock(&pData->mut);
        switch (pData->mode) {
            case MD_SLEEP:
                iRet = doSleep(pData);
                break;
            case MD_FAIL:
                iRet = doFail(pData);
                break;
            case MD_RANDFAIL:
                iRet = doRandFail();
                break;
            case MD_ALWAYS_SUSPEND:
                iRet = RS_RET_SUSPENDED;
                break;
            case MD_BARRIER_ERROR:
            case MD_BARRIER_SUSPEND:
            case MD_FILE_BARRIER:
                break;
            default:
                // No action needed for other cases
                break;
        }

        if (iRet == RS_RET_OK && pData->bEchoStdout) {
            fprintf(stdout, "%s", ppString[0]);
            fflush(stdout);
        }
        pthread_mutex_unlock(&pData->mut);
    }
    dbgprintf(":omtesting: end doAction(), iRet %d\n", iRet);
ENDdoAction


BEGINfreeInstance
    CODESTARTfreeInstance;
    free(pData->barrier_enter_file);
    free(pData->barrier_release_fifo);
    pthread_cond_destroy(&pData->barrier_cond);
    pthread_mutex_destroy(&pData->mut);
ENDfreeInstance


BEGINfreeWrkrInstance
    CODESTARTfreeWrkrInstance;
ENDfreeWrkrInstance


/* The legacy action parser owns the remainder of the selector line.  Consume
 * exactly one unquoted nonempty argument, stopping before the template
 * separator, so file_barrier cannot silently accept a malformed command.
 */
static rsRetVal parseFileBarrierArgument(uchar **const pp, char **const out) {
    DEFiRet;
    uchar *start;
    size_t len;

    while (isspace((int)**pp)) ++*pp;
    if (**pp == '\0' || **pp == ';') ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    start = *pp;
    while (**pp != '\0' && **pp != ';' && !isspace((int)**pp)) ++*pp;
    len = (size_t)(*pp - start);
    if (len == 0) ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    CHKmalloc(*out = malloc(len + 1));
    memcpy(*out, start, len);
    (*out)[len] = '\0';

finalize_it:
    RETiRet;
}


BEGINparseSelectorAct
    int i;
    uchar szBuf[1024];
    CODESTARTparseSelectorAct;
    CODE_STD_STRING_REQUESTparseSelectorAct(1)
        /* code here is quick and dirty - if you like, clean it up. But keep
         * in mind it is just a testing aid ;) -- rgerhards, 2007-12-31
         */
        if (!strncmp((char *)p, ":omtesting:", sizeof(":omtesting:") - 1)) {
        p += sizeof(":omtesting:") - 1; /* eat indicator sequence (-1 because of '\0'!) */
    }
    else {
        ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
    }

    /* ok, if we reach this point, we have something for us */
    if ((iRet = createInstance(&pData)) != RS_RET_OK) goto finalize_it;

    /* check mode */
    for (i = 0; *p && !isspace((char)*p) && ((unsigned)i < sizeof(szBuf) - 1); ++i) {
        szBuf[i] = (uchar)*p++;
    }
    szBuf[i] = '\0';
    if (isspace(*p)) ++p;

    dbgprintf("omtesting command: '%s'\n", szBuf);
    if (!strcmp((char *)szBuf, "sleep")) {
        /* parse seconds */
        for (i = 0; *p && !isspace(*p) && ((unsigned)i < sizeof(szBuf) - 1); ++i) {
            szBuf[i] = *p++;
        }
        szBuf[i] = '\0';
        if (isspace(*p)) ++p;
        pData->iWaitSeconds = atoi((char *)szBuf);
        /* parse microseconds */
        for (i = 0; *p && !isspace(*p) && ((unsigned)i < sizeof(szBuf) - 1); ++i) {
            szBuf[i] = *p++;
        }
        szBuf[i] = '\0';
        if (isspace(*p)) ++p;
        pData->iWaitUSeconds = atoi((char *)szBuf);
        pData->mode = MD_SLEEP;
    } else if (!strcmp((char *)szBuf, "fail")) {
        /* "fail fail-freqency resume-after"
         * fail-frequency specifies how often doAction() fails
         * resume-after speicifes how fast tryResume() should come back with success
         * all numbers being "times called"
         */
        /* parse fail-frequence */
        for (i = 0; *p && !isspace(*p) && ((unsigned)i < sizeof(szBuf) - 1); ++i) {
            szBuf[i] = *p++;
        }
        szBuf[i] = '\0';
        if (isspace(*p)) ++p;
        pData->iFailFrequency = atoi((char *)szBuf);
        /* parse resume-after */
        for (i = 0; *p && !isspace(*p) && ((unsigned)i < sizeof(szBuf) - 1); ++i) {
            szBuf[i] = *p++;
        }
        szBuf[i] = '\0';
        if (isspace(*p)) ++p;
        pData->iResumeAfter = atoi((char *)szBuf);
        pData->iCurrCallNbr = 1;
        pData->mode = MD_FAIL;
    } else if (!strcmp((char *)szBuf, "randfail")) {
        pData->mode = MD_RANDFAIL;
    } else if (!strcmp((char *)szBuf, "always_suspend")) {
        pData->mode = MD_ALWAYS_SUSPEND;
    } else if (!strcmp((char *)szBuf, "barrier_error") || !strcmp((char *)szBuf, "barrier_suspend")) {
        pData->mode = !strcmp((char *)szBuf, "barrier_error") ? MD_BARRIER_ERROR : MD_BARRIER_SUSPEND;
        for (i = 0; *p && !isspace(*p) && ((unsigned)i < sizeof(szBuf) - 1); ++i) {
            szBuf[i] = *p++;
        }
        szBuf[i] = '\0';
        pData->barrier_target = atoi((char *)szBuf);
        if (pData->barrier_target < 2) {
            pData->barrier_target = 2;
        }
    } else if (!strcmp((char *)szBuf, "file_barrier")) {
        CHKiRet(parseFileBarrierArgument(&p, &pData->barrier_enter_file));
        CHKiRet(parseFileBarrierArgument(&p, &pData->barrier_release_fifo));
        pData->mode = MD_FILE_BARRIER;
    } else {
        dbgprintf("invalid mode '%s', doing 'sleep 1 0' - fix your config\n", szBuf);
    }

    pData->bEchoStdout = cs.bEchoStdout;
    CHKiRet(cflineParseTemplateName(&p, *ppOMSR, 0, OMSR_NO_RQD_TPL_OPTS, (uchar *)"RSYSLOG_TraditionalForwardFormat"));

    CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINmodExit
    CODESTARTmodExit;
ENDmodExit


/* Only a testbench build may qualify controlled blocking/error injection. */
static rsRetVal localQueueCheckAction(void *const instance) {
#ifdef ENABLE_IMDIAG
    const instanceData *const pData = instance;
    if (pData != NULL && pData->iWaitSeconds >= 0 && pData->iWaitUSeconds >= 0) {
        switch (pData->mode) {
            case MD_SLEEP:
            case MD_FAIL:
            case MD_ALWAYS_SUSPEND:
            case MD_BARRIER_ERROR:
            case MD_BARRIER_SUSPEND:
                return RS_RET_OK;
            case MD_FILE_BARRIER:
                if (pData->barrier_enter_file == NULL || pData->barrier_enter_file[0] == '\0' ||
                    pData->barrier_release_fifo == NULL || pData->barrier_release_fifo[0] == '\0') {
                    break;
                }
                return RS_RET_OK;
            case MD_RANDFAIL:
            default:
                break;
        }
    }
#else
    (void)instance;
#endif
    return RS_RET_LOCAL_QUEUE_CONFIG;
}

BEGINqueryEtryPt
    CODESTARTqueryEtryPt;
    if (!strcmp((char *)name, "localQueueCheckAction")) *pEtryPoint = (rsRetVal(*)())localQueueCheckAction;
    CODEqueryEtryPt_STD_OMOD_QUERIES;
    CODEqueryEtryPt_STD_OMOD8_QUERIES;
    CODEqueryEtryPt_STD_CONF2_CNFNAME_QUERIES;
ENDqueryEtryPt


BEGINmodInit()
    CODESTARTmodInit;
    INITLegCnfVars;
    *ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
    CODEmodInit_QueryRegCFSLineHdlr CHKiRet(omsdRegCFSLineHdlr((uchar *)"actionomtestingechostdout", 0, eCmdHdlrBinary,
                                                               NULL, &cs.bEchoStdout, STD_LOADABLE_MODULE_ID));
    /* we seed the random-number generator in any case... */
    srand(time(NULL));
ENDmodInit
/*
 * vi:set ai:
 */
