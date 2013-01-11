/* impstats.c
 * A module to periodically output statistics gathered by rsyslog.
 *
 * Copyright 2010-2012 Adiscon GmbH.
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
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/uio.h>
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

MODULE_TYPE_INPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("impstats")

/* defines */
#define DEFAULT_STATS_PERIOD (5 * 60)
#define DEFAULT_FACILITY 5 /* syslog */
#define DEFAULT_SEVERITY 6 /* info */

/* Module static data */
DEF_IMOD_STATIC_DATA
DEFobjCurrIf(glbl)
DEFobjCurrIf(prop)
DEFobjCurrIf(statsobj)
DEFobjCurrIf(errmsg)

typedef struct configSettings_s {
	int iStatsInterval;
	int iFacility;
	int iSeverity;
	int bJSON;
	int bCEE;
} configSettings_t;

struct modConfData_s {
	rsconf_t *pConf; /* our overall config object */
	int iStatsInterval;
	int iFacility;
	int iSeverity;
	int logfd; /* fd if logging to file, or -1 if closed */
	statsFmtType_t statsFmt;
	sbool bLogToSyslog;
	char *logfile;
	sbool configSetViaV2Method;
};
static modConfData_t *loadModConf = NULL;/* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL;/* modConf ptr to use for the current load process */

static configSettings_t cs;
static int bLegacyCnfModGlobalsPermitted;/* are legacy module-global config parameters permitted? */
static prop_t *pInputName = NULL;

/* module-global parameters */
static struct cnfparamdescr modpdescr[] = {
	{ "interval", eCmdHdlrInt, 0 },
	{ "facility", eCmdHdlrInt, 0 },
	{ "severity", eCmdHdlrInt, 0 },
	{ "log.syslog", eCmdHdlrBinary, 0 },
	{ "log.file", eCmdHdlrGetWord, 0 },
	{ "format", eCmdHdlrGetWord, 0 }
};
static struct cnfparamblk modpblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(modpdescr)/sizeof(struct cnfparamdescr),
	  modpdescr
	};

BEGINmodExit
CODESTARTmodExit
	prop.Destruct(&pInputName);
	/* release objects we used */
	objRelease(glbl, CORE_COMPONENT);
	objRelease(prop, CORE_COMPONENT);
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(statsobj, CORE_COMPONENT);
ENDmodExit


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURENonCancelInputTermination)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature

static inline void
initConfigSettings(void)
{
	cs.iStatsInterval = DEFAULT_STATS_PERIOD;
	cs.iFacility = DEFAULT_FACILITY;
	cs.iSeverity = DEFAULT_SEVERITY;
	cs.bJSON = 0;
	cs.bCEE = 0;
}


/* actually submit a message to the rsyslog core
 */
static inline void
doSubmitMsg(uchar *line)
{
	msg_t *pMsg;
	DEFiRet;

	CHKiRet(msgConstruct(&pMsg));
	MsgSetInputName(pMsg, pInputName);
	MsgSetRawMsgWOSize(pMsg, (char*)line);
	MsgSetHOSTNAME(pMsg, glbl.GetLocalHostName(), ustrlen(glbl.GetLocalHostName()));
	MsgSetRcvFrom(pMsg, glbl.GetLocalHostNameProp());
	MsgSetRcvFromIP(pMsg, glbl.GetLocalHostIP());
	MsgSetMSGoffs(pMsg, 0);
	MsgSetTAG(pMsg, UCHAR_CONSTANT("rsyslogd-pstats:"), sizeof("rsyslogd-pstats:") - 1);
	pMsg->iFacility = runModConf->iFacility;
	pMsg->iSeverity = runModConf->iSeverity;
	pMsg->msgFlags  = 0;

	/* we do not use rate-limiting, as the stats message always need to be emitted */
	submitMsg2(pMsg);
	DBGPRINTF("impstats: submit [%d,%d] msg '%s'\n", runModConf->iFacility,
	          runModConf->iSeverity, line);

finalize_it:
	return;
}


/* log stats message to file; limited error handling done */
static inline void
doLogToFile(cstr_t *cstr)
{
	struct iovec iov[4];
	ssize_t nwritten;
	ssize_t nexpect;
	time_t t;
	char timebuf[32];

	if(cstrLen(cstr) == 0)
		goto done;
	if(runModConf->logfd == -1) {
		runModConf->logfd = open(runModConf->logfile, O_WRONLY|O_CREAT|O_APPEND|O_CLOEXEC, S_IRUSR|S_IWUSR);
		if(runModConf->logfd == -1) {
			dbgprintf("error opening stats file %s\n", runModConf->logfile);
			goto done;
		}
	}

	time(&t);
	iov[0].iov_base = ctime_r(&t, timebuf);
	iov[0].iov_len = nexpect = strlen(iov[0].iov_base) - 1; /* -1: strip \n */
	iov[1].iov_base = ": ";
	iov[1].iov_len = 2;
	nexpect += 2;
	iov[2].iov_base = rsCStrGetSzStrNoNULL(cstr);
	iov[2].iov_len = (size_t) cstrLen(cstr);
	nexpect += cstrLen(cstr);
	iov[3].iov_base = "\n";
	iov[3].iov_len = 1;
	nexpect++;
	nwritten = writev(runModConf->logfd, iov, 4);

	if(nwritten != nexpect) {
			dbgprintf("error writing stats file %s, nwritten %lld, expected %lld\n",
				  runModConf->logfile, (long long) nwritten, (long long) nexpect);
	}
done:	return;
}


/* callback for statsobj
 * Note: usrptr exists only to satisfy requirements of statsobj callback interface!
 */
static rsRetVal
doStatsLine(void __attribute__((unused)) *usrptr, cstr_t *cstr)
{
	DEFiRet;
	if(runModConf->bLogToSyslog)
		doSubmitMsg(rsCStrGetSzStrNoNULL(cstr));
	if(runModConf->logfile != NULL)
		doLogToFile(cstr);
	RETiRet;
}


/* the function to generate the actual statistics messages
 * rgerhards, 2010-09-09
 */
static inline void
generateStatsMsgs(void)
{
	statsobj.GetAllStatsLines(doStatsLine, NULL, runModConf->statsFmt);
}


BEGINbeginCnfLoad
CODESTARTbeginCnfLoad
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
	loadModConf->bLogToSyslog = 1;
	bLegacyCnfModGlobalsPermitted = 1;
	/* init legacy config vars */
	initConfigSettings();
ENDbeginCnfLoad


BEGINsetModCnf
	struct cnfparamvals *pvals = NULL;
	char *mode;
	int i;
CODESTARTsetModCnf
	pvals = nvlstGetParams(lst, &modpblk, NULL);
	if(pvals == NULL) {
		errmsg.LogError(0, RS_RET_MISSING_CNFPARAMS, "error processing module "
				"config parameters [module(...)]");
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	if(Debug) {
		dbgprintf("module (global) param blk for impstats:\n");
		cnfparamsPrint(&modpblk, pvals);
	}

	for(i = 0 ; i < modpblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(modpblk.descr[i].name, "interval")) {
			loadModConf->iStatsInterval = (int) pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "facility")) {
			loadModConf->iFacility = (int) pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "severity")) {
			loadModConf->iSeverity = (int) pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "log.syslog")) {
			loadModConf->bLogToSyslog = (sbool) pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "log.file")) {
			loadModConf->logfile = es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(modpblk.descr[i].name, "format")) {
			mode = es_str2cstr(pvals[i].val.d.estr, NULL);
			if(!strcasecmp(mode, "json")) {
				loadModConf->statsFmt = statsFmt_JSON;
			} else if(!strcasecmp(mode, "cee")) {
				loadModConf->statsFmt = statsFmt_CEE;
			} else if(!strcasecmp(mode, "legacy")) {
				loadModConf->statsFmt = statsFmt_Legacy;
			} else {
				errmsg.LogError(0, RS_RET_ERR, "impstats: invalid format %s",
						mode);
			}
			free(mode);
		} else {
			dbgprintf("impstats: program error, non-handled "
			  "param '%s' in beginCnfLoad\n", modpblk.descr[i].name);
		}
	}

	loadModConf->configSetViaV2Method = 1;
	bLegacyCnfModGlobalsPermitted = 0;

finalize_it:
	if(pvals != NULL)
		cnfparamvalsDestruct(pvals, &modpblk);
ENDsetModCnf


BEGINendCnfLoad
CODESTARTendCnfLoad
	if(!loadModConf->configSetViaV2Method) {
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


BEGINcheckCnf
CODESTARTcheckCnf
	if(pModConf->iStatsInterval == 0) {
		errmsg.LogError(0, NO_ERRCODE, "impstats: stats interval zero not permitted, using "
				"default of %d seconds", DEFAULT_STATS_PERIOD);
		pModConf->iStatsInterval = DEFAULT_STATS_PERIOD;
	}
ENDcheckCnf


BEGINactivateCnf
	rsRetVal localRet;
CODESTARTactivateCnf
	runModConf = pModConf;
	DBGPRINTF("impstats: stats interval %d seconds, logToSyslog %d, logFile %s\n",
		  runModConf->iStatsInterval, runModConf->bLogToSyslog,
		  runModConf->logfile == NULL ? "deactivated" : (char*)runModConf->logfile);
	localRet = statsobj.EnableStats();
	if(localRet != RS_RET_OK) {
		errmsg.LogError(0, localRet, "impstats: error enabling statistics gathering");
		ABORT_FINALIZE(RS_RET_NO_RUN);
	}
finalize_it:
ENDactivateCnf


BEGINfreeCnf
CODESTARTfreeCnf
	if(runModConf->logfd != -1)
		close(runModConf->logfd);
	free(runModConf->logfile);
ENDfreeCnf


BEGINrunInput
CODESTARTrunInput
	/* this is an endless loop - it is terminated when the thread is
	 * signalled to do so. This, however, is handled by the framework,
	 * right into the sleep below.
	 */
	while(1) {
		srSleep(runModConf->iStatsInterval, 0); /* seconds, micro seconds */

		if(glbl.GetGlobalInputTermState() == 1)
			break; /* terminate input! */

		DBGPRINTF("impstats: woke up, generating messages\n");
		generateStatsMsgs();
	}
ENDrunInput


BEGINwillRun
CODESTARTwillRun
ENDwillRun


BEGINafterRun
CODESTARTafterRun
ENDafterRun


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_IMOD_QUERIES
CODEqueryEtryPt_STD_CONF2_QUERIES
CODEqueryEtryPt_STD_CONF2_setModCnf_QUERIES
CODEqueryEtryPt_IsCompatibleWithFeature_IF_OMOD_QUERIES
ENDqueryEtryPt

static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	initConfigSettings();
	return RS_RET_OK;
}


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	DBGPRINTF("impstats version %s loading\n", VERSION);
	initConfigSettings();
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(prop, CORE_COMPONENT));
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(statsobj, CORE_COMPONENT));
	/* the pstatsinverval is an alias to support a previous screwed-up syntax... */
	CHKiRet(regCfSysLineHdlr2((uchar *)"pstatsinterval", 0, eCmdHdlrInt, NULL, &cs.iStatsInterval, STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
	CHKiRet(regCfSysLineHdlr2((uchar *)"pstatinterval", 0, eCmdHdlrInt, NULL, &cs.iStatsInterval, STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
	CHKiRet(regCfSysLineHdlr2((uchar *)"pstatfacility", 0, eCmdHdlrInt, NULL, &cs.iFacility, STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
	CHKiRet(regCfSysLineHdlr2((uchar *)"pstatseverity", 0, eCmdHdlrInt, NULL, &cs.iSeverity, STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
	CHKiRet(regCfSysLineHdlr2((uchar *)"pstatjson", 0, eCmdHdlrBinary, NULL, &cs.bJSON, STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
	CHKiRet(regCfSysLineHdlr2((uchar *)"pstatcee", 0, eCmdHdlrBinary, NULL, &cs.bCEE, STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));

	CHKiRet(prop.Construct(&pInputName));
	CHKiRet(prop.SetString(pInputName, UCHAR_CONSTANT("impstats"), sizeof("impstats") - 1));
	CHKiRet(prop.ConstructFinalize(pInputName));
ENDmodInit
/* vi:set ai:
 */
