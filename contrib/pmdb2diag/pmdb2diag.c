/* pmdb2diag.c
 *
 * This is a parser module specifically for DB2diag log file.
 * It extracted program, pid and severity from the log.
 *
 * Copyright 2015 Philippe Duveau @ Pari Mutuel Urbain.
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
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#ifdef HAVE_SYS_TIME_H
    #include <sys/time.h>
#endif
#include "rsyslog.h"
#include "conf.h"
#include "syslogd-types.h"
#include "template.h"
#include "msg.h"
#include "module-template.h"
#include "glbl.h"
#include "errmsg.h"
#include "parser.h"
#include "datetime.h"
#include "unicode-helper.h"

MODULE_TYPE_PARSER
MODULE_TYPE_NOKEEP;
PARSER_NAME("db2.diag")
MODULE_CNFNAME("pmdb2diag")

/* internal structures
 */
DEF_PMOD_STATIC_DATA;
DEFobjCurrIf(glbl) DEFobjCurrIf(datetime)

    /* input instance parameters */
    static struct cnfparamdescr parserpdescr[] = {
        {"levelpos", eCmdHdlrInt, 0},
        {"timepos", eCmdHdlrInt, 0},
        {"timeformat", eCmdHdlrString, 0},
        {"pidstarttoprogstartshift", eCmdHdlrInt, 0},
};
static struct cnfparamblk parserpblk = {CNFPARAMBLK_VERSION, sizeof(parserpdescr) / sizeof(struct cnfparamdescr),
                                        parserpdescr};

struct instanceConf_s {
    int levelpos; /* expected severity position in read message */
    int timepos; /* expected time position in read message */
    int pidstarttoprogstartshift; /* position of prog related to pid */
    char *timeformat; /* format of timestamp in read message */
    char sepSec; /* decimal separator between second and milliseconds */
};

BEGINisCompatibleWithFeature
    CODESTARTisCompatibleWithFeature;
    if (eFeat == sFEATUREAutomaticPRIParsing) iRet = RS_RET_OK;
ENDisCompatibleWithFeature

BEGINparse2
    struct tm tm;
    char *ms, *timepos, *pid, *prog, *eprog, *backslash, *end, *lvl;
    int lprog, lpid, lvl_len;
    char buffer[128];
    CODESTARTparse2;
    assert(pMsg != NULL);
    assert(pMsg->pszRawMsg != NULL);

    DBGPRINTF("Message will now be parsed by \"db2diag\" parser.\n");
    if (pMsg->iLenRawMsg - (int)pMsg->offAfterPRI < pInst->levelpos + 4) ABORT_FINALIZE(RS_RET_COULD_NOT_PARSE);

    /* Instead of comparing strings which a waste of cpu cycles we take interpret the 4 first chars of
     * level read it as int32 and compare it to same interpretation of our constant "levels"
     * So this test is not sensitive to ENDIANESS. This is not a clean way but very efficient.
     */
    lvl = (char *)(pMsg->pszRawMsg + pMsg->offAfterPRI + pInst->levelpos);

    switch (*lvl) {
        case 'C': /* Critical */
            pMsg->iSeverity = LOG_EMERG;
            lvl_len = 8;
            break;
        case 'A': /* Alert */
            pMsg->iSeverity = LOG_ALERT;
            lvl_len = 5;
            break;
        case 'S': /* Severe */
            pMsg->iSeverity = LOG_CRIT;
            lvl_len = 6;
            break;
        case 'E': /* Error / Event */
            pMsg->iSeverity = (lvl[1] == 'r') ? LOG_ERR : LOG_NOTICE;
            lvl_len = 5;
            break;
        case 'W': /* Warning */
            pMsg->iSeverity = LOG_WARNING;
            lvl_len = 7;
            break;
        case 'I': /* Info */
            pMsg->iSeverity = LOG_INFO;
            lvl_len = 4;
            break;
        case 'D': /* Debug */
            pMsg->iSeverity = LOG_DEBUG;
            lvl_len = 5;
            break;
        default:
            /* perhaps the message does not contain a proper level if so don't parse the log */
            ABORT_FINALIZE(0);
    }

    /* let recheck with the real level len */
    if (pMsg->iLenRawMsg - (int)pMsg->offAfterPRI < pInst->levelpos + lvl_len) ABORT_FINALIZE(RS_RET_COULD_NOT_PARSE);

    DBGPRINTF("db2parse Level %d\n", pMsg->iSeverity);

    end = (char *)pMsg->pszRawMsg + pMsg->iLenRawMsg;

    timepos = (char *)pMsg->pszRawMsg + pMsg->offAfterPRI + pInst->timepos;

    DBGPRINTF("db2parse Time %.30s\n", timepos);
    ms = strptime(timepos, pInst->timeformat, &tm);

    if (ms > timepos && *(ms - 1) == pInst->sepSec) {
        /* the timestamp could be properly interpreted by strptime & our format then set proper timestamp  */
        int secfrac = 0, tzoff = 0;

        char *tzpos = strchr(ms, '+');
        if (!tzpos) tzpos = strchr(ms, '-');
        if (!tzpos) tzpos = (char *)"+";

        sscanf(ms, (*tzpos == '+') ? "%d+%d " : "%d-%d ", &secfrac, &tzoff);

        pMsg->tTIMESTAMP.year = tm.tm_year + 1900;
        pMsg->tTIMESTAMP.month = tm.tm_mon + 1;
        pMsg->tTIMESTAMP.day = tm.tm_mday;
        pMsg->tTIMESTAMP.hour = tm.tm_hour;
        pMsg->tTIMESTAMP.minute = tm.tm_min;
        pMsg->tTIMESTAMP.second = tm.tm_sec;
        pMsg->tTIMESTAMP.secfrac = secfrac;
        pMsg->tTIMESTAMP.secfracPrecision = tzpos - ms;
        pMsg->tTIMESTAMP.OffsetMode = *tzpos;
        pMsg->tTIMESTAMP.OffsetHour = tzoff / 60;
        pMsg->tTIMESTAMP.OffsetMinute = tzoff % 60;
    }

    pid = strchr((char *)pMsg->pszRawMsg + pInst->levelpos + lvl_len, ':');
    if (!pid || pid >= end) ABORT_FINALIZE(0);
    pid += 2;
    lpid = strchr(pid, ' ') - pid;

    DBGPRINTF("db2parse pid %.*s\n", lpid, pid);

    /* set the pid */
    snprintf(buffer, 128, "%.*s", lpid, pid);
    MsgSetPROCID(pMsg, buffer);

    prog = pid + pInst->pidstarttoprogstartshift; /* this offset between start of pid to start of prog */
    if (prog >= end) ABORT_FINALIZE(0);

    eprog = strchr(prog, ' '); /* let find the end of the program */
    if (eprog && eprog >= end) ABORT_FINALIZE(0);

    backslash = strchr(prog, '\\'); /* perhaps program contain an backslash */
    if (!backslash || backslash >= end) backslash = end;

    /* Determine the final length of prog */
    lprog = (eprog && eprog < backslash) ? eprog - prog : backslash - prog;

    DBGPRINTF("db2parse prog %.*s     lprog %d\n", lprog, prog, lprog);

    /* set the appname */
    snprintf(buffer, 128, "%.*s", lprog, prog);
    MsgSetAPPNAME(pMsg, buffer);

    /* the original raw msg if not altered by the parser */
finalize_it:
ENDparse2


BEGINfreeParserInst
    CODESTARTfreeParserInst;
    free(pInst->timeformat);
ENDfreeParserInst

BEGINcheckParserInst
    CODESTARTcheckParserInst;
ENDcheckParserInst

static rsRetVal createInstance(instanceConf_t **ppInst) {
    instanceConf_t *pInst;
    DEFiRet;
    CHKmalloc(pInst = (instanceConf_t *)malloc(sizeof(instanceConf_t)));
    pInst->timeformat = NULL;
    pInst->levelpos = 59;
    pInst->timepos = 0;
    pInst->pidstarttoprogstartshift = 49;

    *ppInst = pInst;
finalize_it:
    RETiRet;
}

BEGINnewParserInst
    struct cnfparamvals *pvals = NULL;
    int i;
    CODESTARTnewParserInst;
    inst = NULL;

    DBGPRINTF("newParserInst (pmdb2diag)\n");
    CHKiRet(createInstance(&inst));

    if (lst) {
        if ((pvals = nvlstGetParams(lst, &parserpblk, NULL)) == NULL) {
            ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
        }

        if (Debug) {
            DBGPRINTF("parser param blk in pmdb2diag:\n");
            cnfparamsPrint(&parserpblk, pvals);
        }

        for (i = 0; i < parserpblk.nParams; ++i) {
            if (!pvals[i].bUsed) continue;
            if (!strcmp(parserpblk.descr[i].name, "timeformat")) {
                inst->timeformat = (char *)es_str2cstr(pvals[i].val.d.estr, NULL);
            } else if (!strcmp(parserpblk.descr[i].name, "timepos")) {
                inst->timepos = (int)pvals[i].val.d.n;
            } else if (!strcmp(parserpblk.descr[i].name, "levelpos")) {
                inst->levelpos = (int)pvals[i].val.d.n;
            } else if (!strcmp(parserpblk.descr[i].name, "pidstarttoprogstartshift")) {
                inst->pidstarttoprogstartshift = (int)pvals[i].val.d.n;
            } else {
                DBGPRINTF(
                    "pmdb2diag: program error, non-handled "
                    "param '%s'\n",
                    parserpblk.descr[i].name);
            }
        }
    }

    if (inst->timeformat == NULL) {
        inst->timeformat = strdup("%Y-%m-%d-%H.%M.%S.");
        inst->sepSec = '.';
    } else
        inst->sepSec = inst->timeformat[strlen(inst->timeformat) - 1];

    DBGPRINTF("pmdb2diag: parsing date/time with '%s' at position %d and level at position %d.\n", inst->timeformat,
              inst->timepos, inst->levelpos);

finalize_it:
    CODE_STD_FINALIZERnewParserInst if (lst != NULL) cnfparamvalsDestruct(pvals, &parserpblk);
ENDnewParserInst

BEGINmodExit
    CODESTARTmodExit;
    /* release what we no longer need */
    objRelease(glbl, CORE_COMPONENT);
    objRelease(datetime, CORE_COMPONENT);
ENDmodExit

BEGINqueryEtryPt
    CODESTARTqueryEtryPt;
    CODEqueryEtryPt_STD_PMOD2_QUERIES;
    CODEqueryEtryPt_IsCompatibleWithFeature_IF_OMOD_QUERIES;
ENDqueryEtryPt


BEGINmodInit()
    CODESTARTmodInit;
    *ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
    CODEmodInit_QueryRegCFSLineHdlr CHKiRet(objUse(glbl, CORE_COMPONENT));
    CHKiRet(objUse(datetime, CORE_COMPONENT));
    DBGPRINTF("pmdb2diag parser init called, compiled with version %s\n", VERSION);
ENDmodInit
