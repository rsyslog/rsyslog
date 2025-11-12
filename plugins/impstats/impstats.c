/* impstats.c
 * A module to periodically output statistics gathered by rsyslog.
 *
 * Copyright 2010-2025 Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 * -or-
 * see COPYING.ASL20 in the source distribution
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "config.h"
#include "rsyslog.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <sys/resource.h>
#ifdef OS_LINUX
    #include <sys/types.h>
    #include <dirent.h>
#endif
#include "dirty.h"
#include "cfsysline.h"
#include "module-template.h"
#include "errmsg.h"
#include "msg.h"
#include "srUtils.h"
#include "unicode-helper.h"
#include "glbl.h"
#include "statsobj.h"
#include "prop.h"
#include "ruleset.h"
#include "parserif.h"
#include <libfastjson/json.h> /* libfastjson for robust JSON parsing in Zabbix collector */

MODULE_TYPE_INPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("impstats")

/* defines */
#define DEFAULT_STATS_PERIOD (5 * 60)
#define DEFAULT_FACILITY 5 /* syslog */
#define DEFAULT_SEVERITY 6 /* info */

/* Sentinel value for the Zabbix format (distinct from values defined in statsobj.h) */
#define statsFmt_Zabbix ((statsFmtType_t)(1001))

/* Module static data */
DEF_IMOD_STATIC_DATA;
DEFobjCurrIf(glbl) DEFobjCurrIf(prop) DEFobjCurrIf(statsobj) DEFobjCurrIf(ruleset)

    typedef struct configSettings_s {
    int iStatsInterval;
    int iFacility;
    int iSeverity;
    int bJSON;
    int bCEE;
    int bPrometheus;
} configSettings_t;

struct modConfData_s {
    rsconf_t *pConf; /* our overall config object */
    int iStatsInterval;
    int iFacility;
    int iSeverity;
    int logfd; /* fd if logging to file, or -1 if closed */
    ruleset_t *pBindRuleset; /* ruleset to bind listener to (use system default if unspecified) */
    statsFmtType_t statsFmt;
    sbool bLogToSyslog;
    sbool bResetCtrs;
    sbool bBracketing;
    char *logfile;
    sbool configSetViaV2Method;
    uchar *pszBindRuleset; /* name of ruleset to bind to */
};

static modConfData_t *loadModConf = NULL; /* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL; /* modConf ptr to use for the current load process */
static configSettings_t cs;
static int bLegacyCnfModGlobalsPermitted; /* are legacy module-global config parameters permitted? */
static prop_t *pInputName = NULL;

/* module-global parameters */
static struct cnfparamdescr modpdescr[] = {
    {"interval", eCmdHdlrInt, 0},      {"facility", eCmdHdlrInt, 0},      {"severity", eCmdHdlrInt, 0},
    {"bracketing", eCmdHdlrBinary, 0}, {"log.syslog", eCmdHdlrBinary, 0}, {"resetcounters", eCmdHdlrBinary, 0},
    {"log.file", eCmdHdlrGetWord, 0},  {"format", eCmdHdlrGetWord, 0},    {"ruleset", eCmdHdlrString, 0}};

static struct cnfparamblk modpblk = {CNFPARAMBLK_VERSION, sizeof(modpdescr) / sizeof(struct cnfparamdescr), modpdescr};

/* resource use stats counters */
#ifdef OS_LINUX
static int st_openfiles;
#endif
static intctr_t st_ru_utime;
static intctr_t st_ru_stime;
static intctr_t st_ru_maxrss;
static intctr_t st_ru_minflt;
static intctr_t st_ru_majflt;
static intctr_t st_ru_inblock;
static intctr_t st_ru_oublock;
static intctr_t st_ru_nvcsw;
static intctr_t st_ru_nivcsw;
static statsobj_t *statsobj_resources;
static pthread_mutex_t hup_mutex = PTHREAD_MUTEX_INITIALIZER;

/* forward declaration for Zabbix grouped JSON output */
static void generateZabbixStats(void);

BEGINmodExit
    CODESTARTmodExit;
    prop.Destruct(&pInputName);
    /* release objects we used */
    objRelease(glbl, CORE_COMPONENT);
    objRelease(prop, CORE_COMPONENT);
    objRelease(statsobj, CORE_COMPONENT);
    objRelease(ruleset, CORE_COMPONENT);
ENDmodExit
BEGINisCompatibleWithFeature
    CODESTARTisCompatibleWithFeature;
    if (eFeat == sFEATURENonCancelInputTermination) iRet = RS_RET_OK;
ENDisCompatibleWithFeature

#ifdef OS_LINUX
/* count number of open files (linux specific) */
static void countOpenFiles(void) {
    char proc_path[MAXFNAME];
    DIR *dp;
    struct dirent *files;
    st_openfiles = 0;
    snprintf(proc_path, sizeof(proc_path), "/proc/%d/fd", glblGetOurPid());
    if ((dp = opendir(proc_path)) == NULL) {
        LogError(errno, RS_RET_ERR, "impstats: error reading %s\n", proc_path);
        return;
    }
    while ((files = readdir(dp)) != NULL) {
        if (!strcmp(files->d_name, ".") || !strcmp(files->d_name, "..")) continue;
        st_openfiles++;
    }
    closedir(dp);
}
#endif

static void initConfigSettings(void) {
    cs.iStatsInterval = DEFAULT_STATS_PERIOD;
    cs.iFacility = DEFAULT_FACILITY;
    cs.iSeverity = DEFAULT_SEVERITY;
    cs.bJSON = 0;
    cs.bCEE = 0;
}

/* actually submit a message to the rsyslog core */
static void doSubmitMsg(const char *line) {
    smsg_t *pMsg;
    if (msgConstruct(&pMsg) != RS_RET_OK) goto finalize_it;
    MsgSetInputName(pMsg, pInputName);
    /* COPY-IN variant so the core owns the message buffer */
    MsgSetRawMsg(pMsg, line, strlen(line));
    MsgSetHOSTNAME(pMsg, glbl.GetLocalHostName(), ustrlen(glbl.GetLocalHostName()));
    MsgSetRcvFrom(pMsg, glbl.GetLocalHostNameProp());
    MsgSetRcvFromIP(pMsg, glbl.GetLocalHostIP());
    MsgSetMSGoffs(pMsg, 0);
    MsgSetRuleset(pMsg, runModConf->pBindRuleset);
    MsgSetTAG(pMsg, UCHAR_CONSTANT("rsyslogd-pstats:"), sizeof("rsyslogd-pstats:") - 1);
    pMsg->iFacility = runModConf->iFacility;
    pMsg->iSeverity = runModConf->iSeverity;
    pMsg->msgFlags = 0;
    /* we do not use rate-limiting, as the stats message always need to be emitted */
    submitMsg2(pMsg);
    DBGPRINTF("impstats: submit [%d,%d] msg '%s'\n", runModConf->iFacility, runModConf->iSeverity, line);
finalize_it:
    return;
}

/* log stats message to file; limited error handling done */
static void doLogToFile(const char *ln, const size_t lenLn) {
    struct iovec iov[4];
    ssize_t nwritten;
    ssize_t nexpect;
    time_t t;
    char timebuf[32];

    pthread_mutex_lock(&hup_mutex);
    if (lenLn == 0) goto done;

    if (runModConf->logfd == -1) {
        runModConf->logfd = open(runModConf->logfile, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, S_IRUSR | S_IWUSR);
        if (runModConf->logfd == -1) {
            DBGPRINTF("impstats: error opening stats file %s\n", runModConf->logfile);
            goto done;
        } else {
            DBGPRINTF("impstats: opened stats file %s\n", runModConf->logfile);
        }
    }

    time(&t);
    iov[0].iov_base = ctime_r(&t, timebuf);
    iov[0].iov_len = nexpect = strlen((char *)iov[0].iov_base) - 1; /* strip \n */
    iov[1].iov_base = (void *)": ";
    iov[1].iov_len = 2;
    nexpect += 2;
    iov[2].iov_base = (void *)ln;
    iov[2].iov_len = lenLn;
    nexpect += lenLn;
    iov[3].iov_base = (void *)"\n";
    iov[3].iov_len = 1;
    nexpect++;
    nwritten = writev(runModConf->logfd, iov, 4);
    if (nwritten != nexpect) {
        DBGPRINTF("error writing stats file %s, nwritten %lld, expected %lld\n", runModConf->logfile,
                  (long long)nwritten, (long long)nexpect);
    }
_done2:
    pthread_mutex_unlock(&hup_mutex);
    return;
done:
    goto _done2;
}

/* submit a line to our log destinations. Line must be fully formatted as
 * required (but may be a simple verb like "BEGIN" and "END".
 */
static rsRetVal submitLine(const char *const ln, const size_t lenLn) {
    DEFiRet;
    if (runModConf->bLogToSyslog) doSubmitMsg(ln);
    if (runModConf->logfile != NULL) doLogToFile(ln, lenLn);
    RETiRet;
}

/* callback for statsobj
 * Note: usrptr exists only to satisfy requirements of statsobj callback interface!
 */
static rsRetVal doStatsLine(void __attribute__((unused)) * usrptr, const char *const str) {
    DEFiRet;
    iRet = submitLine(str, strlen(str));
    RETiRet;
}

/* the function to generate the actual statistics messages
 * rgerhards, 2010-09-09
 */
static void generateStatsMsgs(void) {
    struct rusage ru;
    int r;
    r = getrusage(RUSAGE_SELF, &ru);
    if (r != 0) {
        DBGPRINTF("impstats: getrusage() failed with error %d, zeroing out\n", errno);
        memset(&ru, 0, sizeof(ru));
    }
#ifdef OS_LINUX
    countOpenFiles();
#endif
    st_ru_utime = ru.ru_utime.tv_sec * 1000000 + ru.ru_utime.tv_usec;
    st_ru_stime = ru.ru_stime.tv_sec * 1000000 + ru.ru_stime.tv_usec;
    st_ru_maxrss = ru.ru_maxrss;
    st_ru_minflt = ru.ru_minflt;
    st_ru_majflt = ru.ru_majflt;
    st_ru_inblock = ru.ru_inblock;
    st_ru_oublock = ru.ru_oublock;
    st_ru_nvcsw = ru.ru_nvcsw;
    st_ru_nivcsw = ru.ru_nivcsw;

    if (runModConf->statsFmt == statsFmt_Zabbix) { /* statsFmt_Zabbix sentinel */
#ifdef DEBUG_ZABBIX
        DBGPRINTF("impstats: statsFmt=%d (Zabbix sentinel=1001) -> entering generateZabbixStats()\n",
                  (int)runModConf->statsFmt);
#endif
        generateZabbixStats();
    } else {
        statsobj.GetAllStatsLines(doStatsLine, NULL, runModConf->statsFmt, runModConf->bResetCtrs);
    }
}

BEGINbeginCnfLoad
    CODESTARTbeginCnfLoad;
    loadModConf = pModConf;
    pModConf->pConf = pConf;

    /* init our settings */
    loadModConf->configSetViaV2Method = 0;
    loadModConf->iStatsInterval = DEFAULT_STATS_PERIOD;
    loadModConf->iFacility = DEFAULT_FACILITY;
    loadModConf->iSeverity = DEFAULT_SEVERITY;
    loadModConf->statsFmt = statsFmt_Legacy;
    loadModConf->logfd = -1;
    loadModConf->logfile = NULL;
    loadModConf->pszBindRuleset = NULL;
    loadModConf->bLogToSyslog = 1;
    loadModConf->bBracketing = 0;
    loadModConf->bResetCtrs = 0;
    bLegacyCnfModGlobalsPermitted = 1;
    /* init legacy config vars */
    initConfigSettings();
ENDbeginCnfLoad

BEGINsetModCnf
    struct cnfparamvals *pvals = NULL;
    char *mode;
    int i;

    CODESTARTsetModCnf;
    pvals = nvlstGetParams(lst, &modpblk, NULL);
    if (pvals == NULL) {
        LogError(0, RS_RET_MISSING_CNFPARAMS, "error processing module config parameters [module(...)]");
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }
    if (Debug) {
        DBGPRINTF("module (global) param blk for impstats:\n");
        cnfparamsPrint(&modpblk, pvals);
    }
    for (i = 0; i < modpblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;
        if (!strcmp(modpblk.descr[i].name, "interval")) {
            loadModConf->iStatsInterval = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "facility")) {
            loadModConf->iFacility = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "severity")) {
            loadModConf->iSeverity = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "bracketing")) {
            loadModConf->bBracketing = (sbool)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "log.syslog")) {
            loadModConf->bLogToSyslog = (sbool)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "resetcounters")) {
            loadModConf->bResetCtrs = (sbool)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "log.file")) {
            loadModConf->logfile = es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(modpblk.descr[i].name, "format")) {
            mode = es_str2cstr(pvals[i].val.d.estr, NULL);
            if (!strcasecmp(mode, "json")) {
                loadModConf->statsFmt = statsFmt_JSON;
            } else if (!strcasecmp(mode, "json-elasticsearch")) {
                loadModConf->statsFmt = statsFmt_JSON_ES;
            } else if (!strcasecmp(mode, "cee")) {
                loadModConf->statsFmt = statsFmt_CEE;
            } else if (!strcasecmp(mode, "legacy")) {
                loadModConf->statsFmt = statsFmt_Legacy;
            } else if (!strcasecmp(mode, "prometheus")) {
                loadModConf->statsFmt = statsFmt_Prometheus;
            } else if (!strcasecmp(mode, "zabbix")) {
                loadModConf->statsFmt = statsFmt_Zabbix; /* RFC 8259 Compliant */
            } else {
                LogError(0, RS_RET_ERR, "impstats: invalid format %s", mode);
            }
            free(mode);
        } else if (!strcmp(modpblk.descr[i].name, "ruleset")) {
            loadModConf->pszBindRuleset = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else {
            DBGPRINTF("impstats: program error, non-handled param '%s' in beginCnfLoad\n", modpblk.descr[i].name);
        }
    }
    if (loadModConf->pszBindRuleset != NULL && loadModConf->bLogToSyslog == 0) {
        parser_warnmsg(
            "impstats: log.syslog set to \"off\" but ruleset specified - with "
            "these settings, the ruleset will never be used, because regular syslog "
            "processing is turned off - ruleset parameter is ignored");
        free(loadModConf->pszBindRuleset);
        loadModConf->pszBindRuleset = NULL;
    }

    /* Add warning about log.syslog and format=zabbix without log.file */
    if (loadModConf->bLogToSyslog != 0 && loadModConf->statsFmt == statsFmt_Zabbix && loadModConf->logfile == NULL) {
        parser_warnmsg(
            "impstats: log.syslog set to \"on\" and format set to \"zabbix\" without "
            "log.file set. This is not recommended due to potential for pstats msg "
            "truncation leading to malformed JSON. Ensure \"$MaxMessageSize\" at the "
            "top of rsyslog.conf to a sufficient size or ensure log.file is specified. "
            "Increasing $MaxMessageSize may have other adverse side effect.");
    }
    loadModConf->configSetViaV2Method = 1;
    bLegacyCnfModGlobalsPermitted = 0;

finalize_it:
    if (pvals != NULL) cnfparamvalsDestruct(pvals, &modpblk);
ENDsetModCnf

BEGINendCnfLoad
    CODESTARTendCnfLoad;
    if (!loadModConf->configSetViaV2Method) {
        /* persist module-specific settings from legacy config system */
        loadModConf->iStatsInterval = cs.iStatsInterval;
        loadModConf->iFacility = cs.iFacility;
        loadModConf->iSeverity = cs.iSeverity;
        if (cs.bCEE == 1) {
            loadModConf->statsFmt = statsFmt_CEE;
        } else if (cs.bJSON == 1) {
            loadModConf->statsFmt = statsFmt_JSON;
        } else {
            loadModConf->statsFmt = statsFmt_Legacy;
        }
    }
ENDendCnfLoad

/* we need our special version of checkRuleset(), as we do not have any instances */
static rsRetVal checkRuleset(modConfData_t *modConf) {
    ruleset_t *pRuleset;
    rsRetVal localRet;
    DEFiRet;
    modConf->pBindRuleset = NULL; /* assume default ruleset */
    if (modConf->pszBindRuleset == NULL) FINALIZE;
    localRet = ruleset.GetRuleset(modConf->pConf, &pRuleset, modConf->pszBindRuleset);
    if (localRet == RS_RET_NOT_FOUND) {
        LogError(0, NO_ERRCODE, "impstats: ruleset '%s' not found - using default ruleset instead",
                 modConf->pszBindRuleset);
    }
    CHKiRet(localRet);
    modConf->pBindRuleset = pRuleset;
finalize_it:
    RETiRet;
}

/* to use HUP, we need to have an instanceData type, as this was
 * originally designed for actions. However, we do not, and cannot,
 * use the content. The core will always provide a NULL pointer.
 */
typedef struct _instanceData {
    int dummy;
} instanceData;

BEGINdoHUP
    CODESTARTdoHUP;
    DBGPRINTF("impstats: received HUP\n");
    pthread_mutex_lock(&hup_mutex);
    if (runModConf->logfd != -1) {
        DBGPRINTF("impstats: closing log file due to HUP\n");
        close(runModConf->logfd);
        runModConf->logfd = -1;
    }
    pthread_mutex_unlock(&hup_mutex);
ENDdoHUP

BEGINcheckCnf
    CODESTARTcheckCnf;
    if (pModConf->iStatsInterval == 0) {
        LogError(0, NO_ERRCODE, "impstats: stats interval zero not permitted, using default of %d seconds",
                 DEFAULT_STATS_PERIOD);
        pModConf->iStatsInterval = DEFAULT_STATS_PERIOD;
    }
    checkRuleset(pModConf);
ENDcheckCnf

BEGINactivateCnf
    rsRetVal localRet;
    CODESTARTactivateCnf;
    runModConf = pModConf;
    DBGPRINTF("impstats: stats interval %d seconds, reset %d, logToSyslog %d, logFile %s\n", runModConf->iStatsInterval,
              runModConf->bResetCtrs, runModConf->bLogToSyslog,
              runModConf->logfile == NULL ? "deactivated" : (char *)runModConf->logfile);

    localRet = statsobj.EnableStats();
    if (localRet != RS_RET_OK) {
        LogError(0, localRet, "impstats: error enabling statistics gathering");
        ABORT_FINALIZE(RS_RET_NO_RUN);
    }

    /* initialize our own counters */
    CHKiRet(statsobj.Construct(&statsobj_resources));
    CHKiRet(statsobj.SetName(statsobj_resources, (uchar *)"resource-usage"));
    CHKiRet(statsobj.SetOrigin(statsobj_resources, (uchar *)"impstats"));
    CHKiRet(
        statsobj.AddCounter(statsobj_resources, UCHAR_CONSTANT("utime"), ctrType_IntCtr, CTR_FLAG_NONE, &st_ru_utime));
    CHKiRet(
        statsobj.AddCounter(statsobj_resources, UCHAR_CONSTANT("stime"), ctrType_IntCtr, CTR_FLAG_NONE, &st_ru_stime));
    CHKiRet(statsobj.AddCounter(statsobj_resources, UCHAR_CONSTANT("maxrss"), ctrType_IntCtr, CTR_FLAG_NONE,
                                &st_ru_maxrss));
    CHKiRet(statsobj.AddCounter(statsobj_resources, UCHAR_CONSTANT("minflt"), ctrType_IntCtr, CTR_FLAG_NONE,
                                &st_ru_minflt));
    CHKiRet(statsobj.AddCounter(statsobj_resources, UCHAR_CONSTANT("majflt"), ctrType_IntCtr, CTR_FLAG_NONE,
                                &st_ru_majflt));
    CHKiRet(statsobj.AddCounter(statsobj_resources, UCHAR_CONSTANT("inblock"), ctrType_IntCtr, CTR_FLAG_NONE,
                                &st_ru_inblock));
    CHKiRet(statsobj.AddCounter(statsobj_resources, UCHAR_CONSTANT("oublock"), ctrType_IntCtr, CTR_FLAG_NONE,
                                &st_ru_oublock));
    CHKiRet(
        statsobj.AddCounter(statsobj_resources, UCHAR_CONSTANT("nvcsw"), ctrType_IntCtr, CTR_FLAG_NONE, &st_ru_nvcsw));
    CHKiRet(statsobj.AddCounter(statsobj_resources, UCHAR_CONSTANT("nivcsw"), ctrType_IntCtr, CTR_FLAG_NONE,
                                &st_ru_nivcsw));
#ifdef OS_LINUX
    CHKiRet(statsobj.AddCounter(statsobj_resources, UCHAR_CONSTANT("openfiles"), ctrType_Int, CTR_FLAG_NONE,
                                &st_openfiles));
#endif
    CHKiRet(statsobj.ConstructFinalize(statsobj_resources));

finalize_it:
    if (iRet != RS_RET_OK) {
        LogError(0, iRet, "impstats: error activating module");
        iRet = RS_RET_NO_RUN;
    }
ENDactivateCnf

BEGINfreeCnf
    CODESTARTfreeCnf;
    if (runModConf->logfd != -1) close(runModConf->logfd);
    free(runModConf->logfile);
    free(runModConf->pszBindRuleset);
ENDfreeCnf

BEGINrunInput
    CODESTARTrunInput;
    /* this is an endless loop - it is terminated when the thread is
     * signalled to do so. This, however, is handled by the framework,
     * right into the sleep below. Note that we DELIBERATLY output
     * final set of stats counters on termination request. Depending
     * on configuration, they may not make it to the final destination...
     */
    while (glbl.GetGlobalInputTermState() == 0) {
        srSleep(runModConf->iStatsInterval, 0); /* seconds, micro seconds */
        DBGPRINTF("impstats: woke up, generating messages\n");
        if (runModConf->bBracketing) submitLine("BEGIN", sizeof("BEGIN") - 1);
        generateStatsMsgs();
        if (runModConf->bBracketing) submitLine("END", sizeof("END") - 1);
    }
ENDrunInput

BEGINwillRun
    CODESTARTwillRun;
ENDwillRun

BEGINafterRun
    CODESTARTafterRun;
ENDafterRun

BEGINqueryEtryPt
    CODESTARTqueryEtryPt;
    CODEqueryEtryPt_STD_IMOD_QUERIES;
    CODEqueryEtryPt_STD_CONF2_QUERIES;
    CODEqueryEtryPt_STD_CONF2_setModCnf_QUERIES;
    CODEqueryEtryPt_IsCompatibleWithFeature_IF_OMOD_QUERIES;
    CODEqueryEtryPt_doHUP;
ENDqueryEtryPt

static rsRetVal resetConfigVariables(uchar __attribute__((unused)) * pp, void __attribute__((unused)) * pVal) {
    initConfigSettings();
    return RS_RET_OK;
}

BEGINmodInit()
    CODESTARTmodInit;
    *ipIFVersProvided = CURR_MOD_IF_VERSION; /* support the current interface spec */
    CODEmodInit_QueryRegCFSLineHdlr DBGPRINTF("impstats version %s loading\n", VERSION);
    initConfigSettings();
    CHKiRet(objUse(glbl, CORE_COMPONENT));
    CHKiRet(objUse(prop, CORE_COMPONENT));
    CHKiRet(objUse(statsobj, CORE_COMPONENT));
    CHKiRet(objUse(ruleset, CORE_COMPONENT));
    /* legacy aliases */
    CHKiRet(regCfSysLineHdlr2((uchar *)"pstatsinterval", 0, eCmdHdlrInt, NULL, &cs.iStatsInterval,
                              STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
    CHKiRet(regCfSysLineHdlr2((uchar *)"pstatinterval", 0, eCmdHdlrInt, NULL, &cs.iStatsInterval,
                              STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
    CHKiRet(regCfSysLineHdlr2((uchar *)"pstatfacility", 0, eCmdHdlrInt, NULL, &cs.iFacility, STD_LOADABLE_MODULE_ID,
                              &bLegacyCnfModGlobalsPermitted));
    CHKiRet(regCfSysLineHdlr2((uchar *)"pstatseverity", 0, eCmdHdlrInt, NULL, &cs.iSeverity, STD_LOADABLE_MODULE_ID,
                              &bLegacyCnfModGlobalsPermitted));
    CHKiRet(regCfSysLineHdlr2((uchar *)"pstatjson", 0, eCmdHdlrBinary, NULL, &cs.bJSON, STD_LOADABLE_MODULE_ID,
                              &bLegacyCnfModGlobalsPermitted));
    CHKiRet(regCfSysLineHdlr2((uchar *)"pstatcee", 0, eCmdHdlrBinary, NULL, &cs.bCEE, STD_LOADABLE_MODULE_ID,
                              &bLegacyCnfModGlobalsPermitted));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL,
                               STD_LOADABLE_MODULE_ID));
    CHKiRet(prop.Construct(&pInputName));
    CHKiRet(prop.SetString(pInputName, UCHAR_CONSTANT("impstats"), sizeof("impstats") - 1));
    CHKiRet(prop.ConstructFinalize(pInputName));
ENDmodInit

/* ---------- Zabbix (grouped JSON object) builder ----------
 Produces RFC 8259 Compliant Arrayed-JSON.
 Produces one JSON object per emission:
 {
   "timedate": "<ctime_r>",
   "stats_<origin>": [ ... ],
   "stats_<origin>_global": [ ... ], "stats_<origin>_local": [ ... ] // for dual-origins
 }
 Dual-origin rule:
 - For origins that have both global and local statistics (imkafka, omkafka, imtcp, imudp):
   if (name == origin) -> "stats_<origin>_global"
   else -> "stats_<origin>_local"
 Remap rule (this patch):
 - If origin == "core.action" AND name contains "omkafka":
   group into "stats_omkafka_local" (without changing the JSON line).
*/

/* NOTE: statsFmt_Zabbix is defined at the top with other defines (no duplicates here). */

/* whether this origin has both global & local lines we must separate */
static int is_dual_origin(const char *origin) {
    return (!strcmp(origin, "imkafka") || !strcmp(origin, "omkafka") || !strcmp(origin, "imtcp") ||
            !strcmp(origin, "imudp"));
}

/* sanitize origin into identifier-friendly token: replace '.' with '_' */
static void sanitize_origin(const char *origin, char *out, size_t outlen) {
    size_t i = 0;
    for (; origin[i] && i + 1 < outlen; ++i) {
        char c = origin[i];
        out[i] = (c == '.') ? '_' : c;
    }
    if (outlen) out[i < outlen ? i : outlen - 1] = '\0';
}

typedef struct zbx_group_s {
    es_str_t *arr; /* array buffer (open with '['; we'll close later) */
    int first; /* first element flag */
    char *key; /* group key string (e.g., "stats_core_action") */
    size_t count; /* # of items pushed */
} zbx_group_t;

typedef struct zbx_ctx_s {
    zbx_group_t *groups;
    size_t len, cap;
} zbx_ctx_t;

static zbx_group_t *zbx_find_or_create_group(zbx_ctx_t *ctx, const char *key) {
    for (size_t i = 0; i < ctx->len; ++i) {
        if (!strcmp(ctx->groups[i].key, key)) return &ctx->groups[i];
    }
    /* grow */
    if (ctx->len == ctx->cap) {
        size_t ncap = ctx->cap ? ctx->cap * 2 : 8;
        zbx_group_t *ng = (zbx_group_t *)realloc(ctx->groups, ncap * sizeof(*ng));
        if (!ng) return NULL;
        ctx->groups = ng;
        ctx->cap = ncap;
    }
    /* init new group */
    zbx_group_t *g = &ctx->groups[ctx->len++];
    g->arr = es_newStrFromCStr("[", 1);
    g->first = 1;
    g->key = strdup(key);
    g->count = 0;
    if (g->arr == NULL || g->key == NULL) {
        if (g->arr != NULL) es_deleteStr(g->arr);
        if (g->key != NULL) free(g->key);
        ctx->len--;
        return NULL;
    }
    return g;
}

/* Simple detector: core.action entry that belongs to omkafka */
static int is_core_action_omkafka(const char *origin, const char *name) {
    return (strcmp(origin, "core.action") == 0) && (strstr(name, "omkafka") != NULL);
}

/* Parse once and read "origin" and "name" via libfastjson (robust), minimizing overhead. */
static void parse_origin_name(const char *json, char *origin, size_t olen, char *name, size_t nlen) {
    if (olen) origin[0] = '\0';
    if (nlen) name[0] = '\0';

    struct fjson_object *root = fjson_tokener_parse(json);
    if (!root) {
        if (olen) {
            strncpy(origin, "unknown", olen - 1);
            origin[olen - 1] = '\0';
        }
        return;
    }

    struct fjson_object *j = NULL;

    if (fjson_object_object_get_ex(root, "origin", &j) && fjson_object_is_type(j, fjson_type_string)) {
        const char *s = fjson_object_get_string(j);
        if (s && olen) {
            strncpy(origin, s, olen - 1);
            origin[olen - 1] = '\0';
        }
    } else if (olen) {
        strncpy(origin, "unknown", olen - 1);
        origin[olen - 1] = '\0';
    }

    j = NULL;
    if (fjson_object_object_get_ex(root, "name", &j) && fjson_object_is_type(j, fjson_type_string)) {
        const char *s = fjson_object_get_string(j);
        if (s && nlen) {
            strncpy(name, s, nlen - 1);
            name[nlen - 1] = '\0';
        }
    }

    fjson_object_put(root);
}

/* Collector: called for each JSON stats line from statsobj (rendered as JSON). */
static rsRetVal collectStats_zbx(void *usrptr, const char *const str) {
    zbx_ctx_t *ctx = (zbx_ctx_t *)usrptr;

    /* Parse minimal fields from the JSON line (once) */
    char origin[128] = {0}, name[256] = {0};
    parse_origin_name(str, origin, sizeof(origin), name, sizeof(name));

    /* Remap core.action/omkafka lines into stats_omkafka_local (no JSON mutation) */
    if (is_core_action_omkafka(origin, name)) {
        zbx_group_t *g = zbx_find_or_create_group(ctx, "stats_omkafka_local");
        if (!g) return RS_RET_ERR;
        if (!g->first) es_addChar(&g->arr, ',');
        es_addBuf(&g->arr, str, strlen(str));
        g->first = 0;
        g->count++;
        return RS_RET_OK;
    }

    /* Standard grouping */
    char or_sanit[128];
    sanitize_origin(origin, or_sanit, sizeof(or_sanit));
    char keybuf[256];

    if (is_dual_origin(origin)) {
        /* strict equality check for global vs local */
        int is_global = (name[0] != '\0' && !strcmp(origin, name));
        snprintf(keybuf, sizeof(keybuf), "stats_%s_%s", or_sanit, is_global ? "global" : "local");
    } else {
        snprintf(keybuf, sizeof(keybuf), "stats_%s", or_sanit);
    }

    zbx_group_t *g = zbx_find_or_create_group(ctx, keybuf);
    if (!g) return RS_RET_ERR;
    if (!g->first) es_addChar(&g->arr, ',');
    es_addBuf(&g->arr, str, strlen(str));
    g->first = 0;
    g->count++;
    return RS_RET_OK;
}

/* Build one grouped object and emit it as a single line */
static void generateZabbixStats(void) {
    /* Update resource counters (safe/optional) */
    struct rusage ru;
    if (getrusage(RUSAGE_SELF, &ru) == 0) {
        st_ru_utime = ru.ru_utime.tv_sec * 1000000 + ru.ru_utime.tv_usec;
        st_ru_stime = ru.ru_stime.tv_sec * 1000000 + ru.ru_stime.tv_usec;
        st_ru_maxrss = ru.ru_maxrss;
        st_ru_minflt = ru.ru_minflt;
        st_ru_majflt = ru.ru_majflt;
        st_ru_inblock = ru.ru_inblock;
        st_ru_oublock = ru.ru_oublock;
        st_ru_nvcsw = ru.ru_nvcsw;
        st_ru_nivcsw = ru.ru_nivcsw;
    }
#ifdef OS_LINUX
    countOpenFiles();
#endif

    /* Collect and group */
    zbx_ctx_t ctx = {.groups = NULL, .len = 0, .cap = 0};
#ifdef DEBUG_ZABBIX
    DBGPRINTF("impstats: generateZabbixStats() start\n");
#endif
    statsobj.GetAllStatsLines(collectStats_zbx, &ctx, statsFmt_JSON, runModConf->bResetCtrs);

    /* Timestamp */
    time_t t = time(NULL);
    char timebuf[32];
#if defined(OS_LINUX) || defined(_POSIX_VERSION)
    struct tm tm_local;
    localtime_r(&t, &tm_local);
    strftime(timebuf, sizeof(timebuf), "%a %b %d %H:%M:%S %Y", &tm_local);
#else
    strftime(timebuf, sizeof(timebuf), "%a %b %d %H:%M:%S %Y", localtime(&t));
#endif

    /* Build final grouped JSON */
    es_str_t *finalJson = es_newStrFromCStr("{ \"timedate\": \"", strlen("{ \"timedate\": \""));
    if (finalJson == NULL) {
        LogError(0, RS_RET_OUT_OF_MEMORY, "impstats: fn() generateZabbixStats: could not create finalJson string");
        /* cleanup of ctx is handled at the end of the function, so we can just return */
        return;
    }
    es_addBuf(&finalJson, timebuf, strlen(timebuf));
    es_addBuf(&finalJson, "\"", 1);

    /* Append each group in insertion order */
    for (size_t i = 0; i < ctx.len; ++i) {
        es_addBuf(&finalJson, ", \"", 3);
        es_addBuf(&finalJson, ctx.groups[i].key, strlen(ctx.groups[i].key));
        es_addBuf(&finalJson, "\": ", 3);
        /* close group's array */
        es_addBuf(&ctx.groups[i].arr, "]", 1);
        es_addBuf(&finalJson, (const char *)es_getBufAddr(ctx.groups[i].arr), es_strlen(ctx.groups[i].arr));
    }
    es_addChar(&finalJson, '}');

#ifdef DEBUG_ZABBIX
    DBGPRINTF("impstats: Zabbix: %zu groups emitted\n", ctx.len);
#endif

    /* Emit */
    char *out = es_str2cstr(finalJson, NULL);
    submitLine(out, strlen(out));
    free(out);

    /* Cleanup */
    for (size_t i = 0; i < ctx.len; ++i) {
        es_deleteStr(ctx.groups[i].arr);
        free(ctx.groups[i].key);
    }
    free(ctx.groups);
    es_deleteStr(finalJson);
}
