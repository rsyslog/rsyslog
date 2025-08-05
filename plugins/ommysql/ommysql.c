/* ommysql.c
 * This is the implementation of the build-in output module for MySQL.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * File begun on 2007-07-20 by RGerhards (extracted from syslogd.c)
 *
 * Copyright 2007-2021 Adiscon GmbH.
 *
 * This file is part of rsyslog.
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
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <netdb.h>
#include <mysql.h>
#include <mysqld_error.h>
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include "cfsysline.h"
#include "parserif.h"

MODULE_TYPE_OUTPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME("ommysql")

static rsRetVal resetConfigVariables(uchar __attribute__((unused)) * pp, void __attribute__((unused)) * pVal);

/* internal structures
 */
DEF_OMOD_STATIC_DATA;

typedef struct _instanceData {
    char dbsrv[MAXHOSTNAMELEN + 1]; /* IP or hostname of DB server*/
    unsigned int dbsrvPort; /* port of MySQL server */
    char dbname[_DB_MAXDBLEN + 1]; /* DB name */
    char dbuid[_DB_MAXUNAMELEN + 1]; /* DB user */
    char dbpwd[_DB_MAXPWDLEN + 1]; /* DB user's password */
    uchar *configfile; /* MySQL Client Configuration File */
    uchar *configsection; /* MySQL Client Configuration Section */
    uchar *tplName; /* format template to use */
    uchar *socket; /* MySQL socket path */
} instanceData;

typedef struct wrkrInstanceData {
    instanceData *pData;
    MYSQL *hmysql; /* handle to MySQL */
    unsigned uLastMySQLErrno; /* last errno returned by MySQL or 0 if all is well */
} wrkrInstanceData_t;

typedef struct configSettings_s {
    int iSrvPort; /* database server port */
    uchar *pszMySQLConfigFile; /* MySQL Client Configuration File */
    uchar *pszMySQLConfigSection; /* MySQL Client Configuration Section */
} configSettings_t;
static configSettings_t cs;

/* tables for interfacing with the v6 config system */
/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {
    {"server", eCmdHdlrGetWord, 1},
    {"db", eCmdHdlrGetWord, 1},
    {"uid", eCmdHdlrGetWord, 1},
    {"pwd", eCmdHdlrGetWord, 1},
    {"serverport", eCmdHdlrInt, 0},
    {"mysqlconfig.file", eCmdHdlrGetWord, 0},
    {"mysqlconfig.section", eCmdHdlrGetWord, 0},
    {"template", eCmdHdlrGetWord, 0},
    {"socket", eCmdHdlrGetWord, 0},
};
static struct cnfparamblk actpblk = {CNFPARAMBLK_VERSION, sizeof(actpdescr) / sizeof(struct cnfparamdescr), actpdescr};

/**
 * @brief Synchronizes access to the shared MySQL handle.
 *
 * Worker threads read-lock this mutex while issuing queries so multiple
 * threads can use the connection concurrently.  When the handle needs to be
 * closed or reinitialized, the code temporarily upgrades to a write lock to
 * gain exclusive access and prevent use-after-free errors.
 *
 * TODO: we need to validate this assumption - in theory, this shall not be
 * possible, because instance data is worker-local. However, the following 2021
 * commit suggests there were some issue. I tend to say the commit was wrong,
 * but this needs to be checked in more depth, while we need some better doc
 * on the current locking method right now. So we document existing and valid
 * behaviour and will follow up later.
 *
 * related commit: https://github.com/rsyslog/rsyslog/pull/4719/commits/a3b2983342e20d157b76695d162533c3cfa69587
 */
pthread_rwlock_t rwlock_hmysql;

BEGINinitConfVars /* (re)set config variables to default values */
    CODESTARTinitConfVars;
    resetConfigVariables(NULL, NULL);
ENDinitConfVars


BEGINcreateInstance
    CODESTARTcreateInstance;
ENDcreateInstance


BEGINcreateWrkrInstance
    CODESTARTcreateWrkrInstance;
    pthread_rwlock_wrlock(&rwlock_hmysql);
    pWrkrData->hmysql = NULL;
    pthread_rwlock_unlock(&rwlock_hmysql);
ENDcreateWrkrInstance


BEGINisCompatibleWithFeature
    CODESTARTisCompatibleWithFeature;
    if (eFeat == sFEATURERepeatedMsgReduction) iRet = RS_RET_OK;
ENDisCompatibleWithFeature


/**
 * @brief Close the current MySQL connection.
 *
 * The caller holds @ref rwlock_hmysql in read mode and expects to keep a read
 * lock on return. This function temporarily releases the read lock, obtains a 
 * write lock to safely close the connection, then reacquires a read lock.
 * 
 * Note: During the brief window between releasing the read lock and acquiring
 * the write lock, other threads may also enter this function. The NULL check
 * after acquiring the write lock ensures that only one thread actually closes
 * the connection.
 */
static void closeMySQL(wrkrInstanceData_t *pWrkrData) {
    pthread_rwlock_unlock(&rwlock_hmysql);
    pthread_rwlock_wrlock(&rwlock_hmysql);
    if (pWrkrData->hmysql != NULL) { /* re-check to see if another instance already closed it */
        mysql_close(pWrkrData->hmysql);
        pWrkrData->hmysql = NULL;
    }
    pthread_rwlock_unlock(&rwlock_hmysql);
    pthread_rwlock_rdlock(&rwlock_hmysql);
}

BEGINfreeInstance
    CODESTARTfreeInstance;
    free(pData->configfile);
    free(pData->configsection);
    free(pData->tplName);
    free(pData->socket);
ENDfreeInstance


BEGINfreeWrkrInstance
    CODESTARTfreeWrkrInstance;
    pthread_rwlock_rdlock(&rwlock_hmysql);
    closeMySQL(pWrkrData);
    mysql_thread_end();
    pthread_rwlock_unlock(&rwlock_hmysql);
ENDfreeWrkrInstance


BEGINdbgPrintInstInfo
    CODESTARTdbgPrintInstInfo;
    /* nothing special here */
ENDdbgPrintInstInfo


/* log a database error with descriptive message.
 * We check if we have a valid MySQL handle. If not, we simply
 * report an error, but can not be specific. RGerhards, 2007-01-30
 */
static void reportDBError(wrkrInstanceData_t *pWrkrData, int bSilent) {
    char errMsg[512];
    unsigned uMySQLErrno;

    /* output log message */
    errno = 0;
    if (pWrkrData->hmysql == NULL) {
        LogError(0, NO_ERRCODE, "ommysql: unknown DB error occurred - could not obtain MySQL handle");
    } else { /* we can ask mysql for the error description... */
        uMySQLErrno = mysql_errno(pWrkrData->hmysql);
        snprintf(errMsg, sizeof(errMsg), "db error (%u): %s\n", uMySQLErrno, mysql_error(pWrkrData->hmysql));
        if (bSilent || uMySQLErrno == pWrkrData->uLastMySQLErrno)
            dbgprintf("mysql, DBError(silent): %s\n", errMsg);
        else {
            pWrkrData->uLastMySQLErrno = uMySQLErrno;
            LogError(0, NO_ERRCODE, "ommysql: %s", errMsg);
        }
    }

    return;
}


/* The following function is responsible for initializing a
 * MySQL connection.
 * Initially added 2004-10-28 mmeckelein
 */
static rsRetVal initMySQL(wrkrInstanceData_t *pWrkrData, int bSilent) {
    instanceData *pData;
    DEFiRet;

    assert(pWrkrData->hmysql == NULL);
    pData = pWrkrData->pData;

    pthread_rwlock_unlock(&rwlock_hmysql);
    pthread_rwlock_wrlock(&rwlock_hmysql);

    pWrkrData->hmysql = mysql_init(NULL);
    if (pWrkrData->hmysql == NULL) {
        LogError(0, RS_RET_SUSPENDED, "can not initialize MySQL handle");
        iRet = RS_RET_SUSPENDED;
    } else { /* we could get the handle, now on with work... */
        mysql_options(pWrkrData->hmysql, MYSQL_READ_DEFAULT_GROUP,
                      ((pData->configsection != NULL) ? (char *)pData->configsection : "client"));
        if (pData->configfile != NULL) {
            FILE *fp;
            fp = fopen((char *)pData->configfile, "r");
            int err = errno;
            if (fp == NULL) {
                char msg[512];
                snprintf(msg, sizeof(msg), "Could not open '%s' for reading", pData->configfile);
                if (bSilent) {
                    char errStr[512];
                    rs_strerror_r(err, errStr, sizeof(errStr));
                    dbgprintf("mysql configuration error(%d): %s - %s\n", err, msg, errStr);
                } else
                    LogError(err, NO_ERRCODE, "mysql configuration error: %s\n", msg);
            } else {
                fclose(fp);
                mysql_options(pWrkrData->hmysql, MYSQL_READ_DEFAULT_FILE, pData->configfile);
            }
        }
        /* Connect to database */
        if (mysql_real_connect(pWrkrData->hmysql, pData->dbsrv, pData->dbuid, pData->dbpwd, pData->dbname,
                               pData->dbsrvPort, (const char *)pData->socket, 0) == NULL) {
            reportDBError(pWrkrData, bSilent);
            closeMySQL(pWrkrData); /* ignore any error we may get */
            ABORT_FINALIZE(RS_RET_SUSPENDED);
        }
        if (mysql_autocommit(pWrkrData->hmysql, 0)) {
            LogMsg(0, NO_ERRCODE, LOG_WARNING,
                   "ommysql: activating autocommit failed, "
                   "some data may be duplicated\n");
            reportDBError(pWrkrData, 0);
        }
    }

finalize_it:
    pthread_rwlock_unlock(&rwlock_hmysql);
    pthread_rwlock_rdlock(&rwlock_hmysql);
    RETiRet;
}


/* The following function writes the current log entry
 * to an established MySQL session.
 * Initially added 2004-10-28 mmeckelein
 */
static rsRetVal writeMySQL(wrkrInstanceData_t *pWrkrData, const uchar *const psz) {
    DEFiRet;

    /* see if we are ready to proceed */
    if (pWrkrData->hmysql == NULL) {
        CHKiRet(initMySQL(pWrkrData, 0));
    }

    /* try insert */
    if (mysql_query(pWrkrData->hmysql, (char *)psz)) {
        const int mysql_err = mysql_errno(pWrkrData->hmysql);
        /* We assume server error codes are non-recoverable, mainly data errors.
         * This also means we need to differentiate between client and server error
         * codes. Unfortunately, the API does not provide a specified function for
         * this. Howerver, error codes 2000..2999 are currently client error codes.
         * So we use this as guideline.
         */
        if (mysql_err < 2000 || mysql_err > 2999) {
            reportDBError(pWrkrData, 0);
            LogError(0, RS_RET_DATAFAIL, "The error statement was: %s", psz);
            ABORT_FINALIZE(RS_RET_DATAFAIL);
        }
        /* potentially recoverable error occurred, try to re-init connection and retry */
        closeMySQL(pWrkrData); /* close the current handle */
        CHKiRet(initMySQL(pWrkrData, 0)); /* try to re-open */
        if (mysql_query(pWrkrData->hmysql, (char *)psz)) { /* re-try insert */
            /* we failed, giving up for now */
            DBGPRINTF("ommysql: suspending due to failed write of '%s'\n", psz);
            reportDBError(pWrkrData, 0);
            closeMySQL(pWrkrData); /* free ressources */
            ABORT_FINALIZE(RS_RET_SUSPENDED);
        }
    }

finalize_it:
    if (iRet == RS_RET_OK) {
        pWrkrData->uLastMySQLErrno = 0; /* reset error for error supression */
    }

    RETiRet;
}


BEGINtryResume
    CODESTARTtryResume;
    pthread_rwlock_rdlock(&rwlock_hmysql);
    if (pWrkrData->hmysql == NULL) {
        iRet = initMySQL(pWrkrData, 1);
    }
    pthread_rwlock_unlock(&rwlock_hmysql);
ENDtryResume

BEGINbeginTransaction
    CODESTARTbeginTransaction;
    // NOTHING TO DO IN HERE
ENDbeginTransaction

BEGINcommitTransaction
    CODESTARTcommitTransaction;
    DBGPRINTF("ommysql: commitTransaction\n");
    pthread_rwlock_rdlock(&rwlock_hmysql);
    CHKiRet(writeMySQL(pWrkrData, (uchar *)"START TRANSACTION"));

    for (unsigned i = 0; i < nParams; ++i) {
        iRet = writeMySQL(pWrkrData, actParam(pParams, 1, i, 0).param);
        if (iRet != RS_RET_OK && iRet != RS_RET_DEFER_COMMIT && iRet != RS_RET_PREVIOUS_COMMITTED) {
            if (pWrkrData->hmysql == NULL) {
                DBGPRINTF(
                    "ommysql: server error: hmysql is closed, transaction rollback "
                    "will not be tried (it probably already happened)\n");
            } else {
                if (mysql_rollback(pWrkrData->hmysql) != 0) {
                    DBGPRINTF("ommysql: server error: transaction could not be rolled back\n");
                }
                closeMySQL(pWrkrData);
            }
            FINALIZE;
        }
    }

    if (mysql_commit(pWrkrData->hmysql) != 0) {
        DBGPRINTF("ommysql: server error: transaction not committed\n");
        reportDBError(pWrkrData, 0);
        ABORT_FINALIZE(RS_RET_SUSPENDED);
    }
    DBGPRINTF("ommysql: transaction committed\n");
finalize_it:
    pthread_rwlock_unlock(&rwlock_hmysql);
ENDcommitTransaction

static inline void setInstParamDefaults(instanceData *pData) {
    pData->dbsrvPort = 0;
    pData->configfile = NULL;
    pData->configsection = NULL;
    pData->tplName = NULL;
    pData->socket = NULL;
}


/* note: we use the fixed-size buffers inside the config object to avoid
 * changing too much of the previous plumbing. rgerhards, 2012-02-02
 */
BEGINnewActInst
    struct cnfparamvals *pvals;
    int i;
    char *cstr;
    size_t len;
    CODESTARTnewActInst;
    if ((pvals = nvlstGetParams(lst, &actpblk, NULL)) == NULL) {
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }

    CHKiRet(createInstance(&pData));
    setInstParamDefaults(pData);

    CODE_STD_STRING_REQUESTparseSelectorAct(1) for (i = 0; i < actpblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;
        if (!strcmp(actpblk.descr[i].name, "server")) {
            cstr = es_str2cstr(pvals[i].val.d.estr, NULL);
            len = es_strlen(pvals[i].val.d.estr);
            if (len >= sizeof(pData->dbsrv) - 1) {
                parser_errmsg(
                    "ommysql: dbname parameter longer than supported "
                    "maximum of %d characters",
                    (int)sizeof(pData->dbsrv) - 1);
                ABORT_FINALIZE(RS_RET_PARAM_ERROR);
            }
            memcpy(pData->dbsrv, cstr, len + 1);
            free(cstr);
        } else if (!strcmp(actpblk.descr[i].name, "serverport")) {
            pData->dbsrvPort = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "db")) {
            cstr = es_str2cstr(pvals[i].val.d.estr, NULL);
            len = es_strlen(pvals[i].val.d.estr);
            if (len >= sizeof(pData->dbname) - 1) {
                parser_errmsg(
                    "ommysql: dbname parameter longer than supported "
                    "maximum of %d characters",
                    (int)sizeof(pData->dbname) - 1);
                ABORT_FINALIZE(RS_RET_PARAM_ERROR);
            }
            memcpy(pData->dbname, cstr, len + 1);
            free(cstr);
        } else if (!strcmp(actpblk.descr[i].name, "uid")) {
            cstr = es_str2cstr(pvals[i].val.d.estr, NULL);
            len = es_strlen(pvals[i].val.d.estr);
            if (len >= sizeof(pData->dbuid) - 1) {
                parser_errmsg(
                    "ommysql: uid parameter longer than supported "
                    "maximum of %d characters",
                    (int)sizeof(pData->dbuid) - 1);
                ABORT_FINALIZE(RS_RET_PARAM_ERROR);
            }
            memcpy(pData->dbuid, cstr, len + 1);
            free(cstr);
        } else if (!strcmp(actpblk.descr[i].name, "pwd")) {
            cstr = es_str2cstr(pvals[i].val.d.estr, NULL);
            len = es_strlen(pvals[i].val.d.estr);
            if (len >= sizeof(pData->dbpwd) - 1) {
                parser_errmsg(
                    "ommysql: pwd parameter longer than supported "
                    "maximum of %d characters",
                    (int)sizeof(pData->dbpwd) - 1);
                ABORT_FINALIZE(RS_RET_PARAM_ERROR);
            }
            memcpy(pData->dbpwd, cstr, len + 1);
            free(cstr);
        } else if (!strcmp(actpblk.descr[i].name, "mysqlconfig.file")) {
            pData->configfile = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "mysqlconfig.section")) {
            pData->configsection = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "template")) {
            pData->tplName = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "socket")) {
            pData->socket = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else {
            dbgprintf(
                "ommysql: program error, non-handled "
                "param '%s'\n",
                actpblk.descr[i].name);
        }
    }

    if (pData->tplName == NULL) {
        CHKiRet(OMSRsetEntry(*ppOMSR, 0, (uchar *)strdup(" StdDBFmt"), OMSR_RQD_TPL_OPT_SQL));
    } else {
        CHKiRet(OMSRsetEntry(*ppOMSR, 0, (uchar *)strdup((char *)pData->tplName), OMSR_RQD_TPL_OPT_SQL));
    }
    CODE_STD_FINALIZERnewActInst;
    cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst


BEGINparseSelectorAct
    int iMySQLPropErr = 0;
    CODESTARTparseSelectorAct;
    CODE_STD_STRING_REQUESTparseSelectorAct(1)
        /* first check if this config line is actually for us
         * The first test [*p == '>'] can be skipped if a module shall only
         * support the newer slection syntax [:modname:]. This is in fact
         * recommended for new modules. Please note that over time this part
         * will be handled by rsyslogd itself, but for the time being it is
         * a good compromise to do it at the module level.
         * rgerhards, 2007-10-15
         */
        if (*p == '>') {
        p++; /* eat '>' '*/
    }
    else if (!strncmp((char *)p, ":ommysql:", sizeof(":ommysql:") - 1)) {
        p += sizeof(":ommysql:") - 1; /* eat indicator sequence  (-1 because of '\0'!) */
    }
    else {
        ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
    }

    /* ok, if we reach this point, we have something for us */
    CHKiRet(createInstance(&pData));

    /* rger 2004-10-28: added support for MySQL
     * >server,dbname,userid,password
     * Now we read the MySQL connection properties
     * and verify that the properties are valid.
     */
    if (getSubString(&p, pData->dbsrv, MAXHOSTNAMELEN + 1, ',')) iMySQLPropErr++;
    if (*pData->dbsrv == '\0') iMySQLPropErr++;
    if (getSubString(&p, pData->dbname, _DB_MAXDBLEN + 1, ',')) iMySQLPropErr++;
    if (*pData->dbname == '\0') iMySQLPropErr++;
    if (getSubString(&p, pData->dbuid, _DB_MAXUNAMELEN + 1, ',')) iMySQLPropErr++;
    if (*pData->dbuid == '\0') iMySQLPropErr++;
    if (getSubString(&p, pData->dbpwd, _DB_MAXPWDLEN + 1, ';')) iMySQLPropErr++;
    /* now check for template
     * We specify that the SQL option must be present in the template.
     * This is for your own protection (prevent sql injection).
     */
    if (*(p - 1) == ';')
        --p; /* TODO: the whole parsing of the MySQL module needs to be re-thought - but this here
              *       is clean enough for the time being -- rgerhards, 2007-07-30
              */
    CHKiRet(cflineParseTemplateName(&p, *ppOMSR, 0, OMSR_RQD_TPL_OPT_SQL, (uchar *)" StdDBFmt"));

    /* If we detect invalid properties, we disable logging,
     * because right properties are vital at this place.
     * Retries make no sense.
     */
    if (iMySQLPropErr) {
        LogError(0, RS_RET_INVALID_PARAMS,
                 "Trouble with MySQL connection properties. "
                 "-MySQL logging disabled");
        ABORT_FINALIZE(RS_RET_INVALID_PARAMS);
    } else {
        pData->dbsrvPort = (unsigned)cs.iSrvPort; /* set configured port */
        pData->configfile = cs.pszMySQLConfigFile;
        pData->configsection = cs.pszMySQLConfigSection;
        pData->socket = NULL;
    }

    CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINmodExit
    CODESTARTmodExit;
    pthread_rwlock_destroy(&rwlock_hmysql);
#ifdef HAVE_MYSQL_LIBRARY_INIT
    mysql_library_end();
#else
    mysql_server_end();
#endif
ENDmodExit


BEGINqueryEtryPt
    CODESTARTqueryEtryPt;
    CODEqueryEtryPt_STD_OMODTX_QUERIES;
    CODEqueryEtryPt_STD_OMOD8_QUERIES;
    CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES;
ENDqueryEtryPt


/* Reset config variables for this module to default values.
 */
static rsRetVal resetConfigVariables(uchar __attribute__((unused)) * pp, void __attribute__((unused)) * pVal) {
    DEFiRet;
    cs.iSrvPort = 0; /* zero is the default port */
    free(cs.pszMySQLConfigFile);
    cs.pszMySQLConfigFile = NULL;
    free(cs.pszMySQLConfigSection);
    cs.pszMySQLConfigSection = NULL;
    RETiRet;
}

BEGINmodInit()
    CODESTARTmodInit;
    INITLegCnfVars;
    *ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
    CODEmodInit_QueryRegCFSLineHdlr INITChkCoreFeature(bCoreSupportsBatching, CORE_FEATURE_BATCHING);
    if (!bCoreSupportsBatching) {
        LogError(0, NO_ERRCODE, "ommysql: rsyslog core too old");
        ABORT_FINALIZE(RS_RET_ERR);
    }

    /* we need to init the MySQL library. If that fails, we cannot run */
    if (
#ifdef HAVE_MYSQL_LIBRARY_INIT
        mysql_library_init(0, NULL, NULL)
#else
        mysql_server_init(0, NULL, NULL)
#endif
    ) {
        LogError(0, NO_ERRCODE,
                 "ommysql: initializing mysql client failed, plugin "
                 "can not run");
        ABORT_FINALIZE(RS_RET_ERR);
    }

    pthread_rwlock_init(&rwlock_hmysql, NULL);

    /* register our config handlers */
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"actionommysqlserverport", 0, eCmdHdlrInt, NULL, &cs.iSrvPort,
                               STD_LOADABLE_MODULE_ID));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"ommysqlconfigfile", 0, eCmdHdlrGetWord, NULL, &cs.pszMySQLConfigFile,
                               STD_LOADABLE_MODULE_ID));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"ommysqlconfigsection", 0, eCmdHdlrGetWord, NULL, &cs.pszMySQLConfigSection,
                               STD_LOADABLE_MODULE_ID));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL,
                               STD_LOADABLE_MODULE_ID));
ENDmodInit
