/* impstats.c
 * A module to periodically output statistics gathered by rsyslog.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * File begun on 2007-07-20 by RGerhards (extracted from syslogd.c)
 * This file is under development and has not yet arrived at being fully
 * self-contained and a real object. So far, it is mostly an excerpt
 * of the "old" message code without any modifications. However, it
 * helps to have things at the right place one we go to the meat of it.
 *
 * Copyright 2010 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Rsyslog is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rsyslog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Rsyslog.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */
#include "config.h"
#include "rsyslog.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
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
} configSettings_t;

static configSettings_t cs;

static prop_t *pInputName = NULL;
static prop_t *pLocalHostIP = NULL;

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
}


/* actually submit a message to the rsyslog core
 */
static inline rsRetVal
doSubmitMsg(uchar *line)
{
	msg_t *pMsg;
	DEFiRet;

	CHKiRet(msgConstruct(&pMsg));
	MsgSetInputName(pMsg, pInputName);
	MsgSetRawMsgWOSize(pMsg, (char*)line);
	MsgSetHOSTNAME(pMsg, glbl.GetLocalHostName(), ustrlen(glbl.GetLocalHostName()));
	MsgSetRcvFrom(pMsg, glbl.GetLocalHostNameProp());
	MsgSetRcvFromIP(pMsg, pLocalHostIP);
	MsgSetMSGoffs(pMsg, 0);
	MsgSetTAG(pMsg, UCHAR_CONSTANT("rsyslogd-pstats:"), sizeof("rsyslogd-pstats:") - 1);
	pMsg->iFacility = cs.iFacility;
	pMsg->iSeverity = cs.iSeverity;
	pMsg->msgFlags  = 0;

	submitMsg(pMsg);

finalize_it:
	RETiRet;

}


/* callback for statsobj
 * Note: usrptr exists only to satisfy requirements of statsobj callback interface!
 */
static rsRetVal
doStatsLine(void __attribute__((unused)) *usrptr, cstr_t *cstr)
{
	DEFiRet;
	doSubmitMsg(rsCStrGetSzStrNoNULL(cstr));
	RETiRet;
}


/* the function to generate the actual statistics messages
 * rgerhards, 2010-09-09
 */
static inline void
generateStatsMsgs(void)
{
	statsobj.GetAllStatsLines(doStatsLine, NULL);
}


BEGINrunInput
CODESTARTrunInput
	/* this is an endless loop - it is terminated when the thread is
	 * signalled to do so. This, however, is handled by the framework,
	 * right into the sleep below.
	 */
	while(1) {
		srSleep(cs.iStatsInterval, 0); /* seconds, micro seconds */

		if(glbl.GetGlobalInputTermState() == 1)
			break; /* terminate input! */

		generateStatsMsgs();
	}
ENDrunInput


BEGINwillRun
	rsRetVal localRet;
CODESTARTwillRun
	DBGPRINTF("impstats: stats interval %d seconds\n", cs.iStatsInterval);
	if(cs.iStatsInterval == 0)
		ABORT_FINALIZE(RS_RET_NO_RUN);
	localRet = statsobj.EnableStats();
	if(localRet != RS_RET_OK) {
		errmsg.LogError(0, localRet, "impstat: error enabling statistics gathering");
		ABORT_FINALIZE(RS_RET_NO_RUN);
	}
finalize_it:
ENDwillRun


BEGINafterRun
CODESTARTafterRun
ENDafterRun


BEGINmodExit
CODESTARTmodExit
	prop.Destruct(&pInputName);
	prop.Destruct(&pLocalHostIP);
	/* release objects we used */
	objRelease(glbl, CORE_COMPONENT);
	objRelease(prop, CORE_COMPONENT);
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(statsobj, CORE_COMPONENT);
ENDmodExit



BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_IMOD_QUERIES
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
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"pstatsinterval", 0, eCmdHdlrInt, NULL, &cs.iStatsInterval, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"pstatinterval", 0, eCmdHdlrInt, NULL, &cs.iStatsInterval, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"pstatfacility", 0, eCmdHdlrInt, NULL, &cs.iFacility, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"pstatseverity", 0, eCmdHdlrInt, NULL, &cs.iSeverity, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));

	CHKiRet(prop.Construct(&pInputName));
	CHKiRet(prop.SetString(pInputName, UCHAR_CONSTANT("impstats"), sizeof("impstats") - 1));
	CHKiRet(prop.ConstructFinalize(pInputName));

	CHKiRet(prop.Construct(&pLocalHostIP));
	CHKiRet(prop.SetString(pLocalHostIP, UCHAR_CONSTANT("127.0.0.1"), sizeof("127.0.0.1") - 1));
	CHKiRet(prop.ConstructFinalize(pLocalHostIP));
ENDmodInit
/* vi:set ai:
 */
