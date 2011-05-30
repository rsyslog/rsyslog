/* rsconf.c - the rsyslog configuration system.
 *
 * Module begun 2011-04-19 by Rainer Gerhards
 *
 * Copyright 2011 by Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
 *
 * The rsyslog runtime library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The rsyslog runtime library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the rsyslog runtime library.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 * A copy of the LGPL can be found in the file "COPYING.LESSER" in this distribution.
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <grp.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "rsyslog.h"
#include "obj.h"
#include "srUtils.h"
#include "ruleset.h"
#include "modules.h"
#include "conf.h"
#include "queue.h"
#include "rsconf.h"
#include "cfsysline.h"
#include "errmsg.h"
#include "action.h"
#include "glbl.h"
#include "unicode-helper.h"
#include "omshell.h"
#include "omusrmsg.h"
#include "omfwd.h"
#include "omfile.h"
#include "ompipe.h"
#include "omdiscard.h"
#include "pmrfc5424.h"
#include "pmrfc3164.h"
#include "smfile.h"
#include "smtradfile.h"
#include "smfwd.h"
#include "smtradfwd.h"
#include "parser.h"
#include "outchannel.h"
#include "threads.h"
#include "dirty.h"

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(ruleset)
DEFobjCurrIf(module)
DEFobjCurrIf(conf)
DEFobjCurrIf(errmsg)
DEFobjCurrIf(glbl)
DEFobjCurrIf(parser)

/* exported static data */
rsconf_t *runConf = NULL;/* the currently running config */
rsconf_t *loadConf = NULL;/* the config currently being loaded (no concurrent config load supported!) */

/* hardcoded standard templates (used for defaults) */
static uchar template_DebugFormat[] = "\"Debug line with all properties:\nFROMHOST: '%FROMHOST%', fromhost-ip: '%fromhost-ip%', HOSTNAME: '%HOSTNAME%', PRI: %PRI%,\nsyslogtag '%syslogtag%', programname: '%programname%', APP-NAME: '%APP-NAME%', PROCID: '%PROCID%', MSGID: '%MSGID%',\nTIMESTAMP: '%TIMESTAMP%', STRUCTURED-DATA: '%STRUCTURED-DATA%',\nmsg: '%msg%'\nescaped msg: '%msg:::drop-cc%'\ninputname: %inputname% rawmsg: '%rawmsg%'\n\n\"";
static uchar template_SyslogProtocol23Format[] = "\"<%PRI%>1 %TIMESTAMP:::date-rfc3339% %HOSTNAME% %APP-NAME% %PROCID% %MSGID% %STRUCTURED-DATA% %msg%\n\"";
static uchar template_TraditionalFileFormat[] = "=RSYSLOG_TraditionalFileFormat";
static uchar template_FileFormat[] = "=RSYSLOG_FileFormat";
static uchar template_ForwardFormat[] = "=RSYSLOG_ForwardFormat";
static uchar template_TraditionalForwardFormat[] = "=RSYSLOG_TraditionalForwardFormat";
static uchar template_WallFmt[] = "\"\r\n\7Message from syslogd@%HOSTNAME% at %timegenerated% ...\r\n %syslogtag%%msg%\n\r\"";
static uchar template_StdUsrMsgFmt[] = "\" %syslogtag%%msg%\n\r\"";
static uchar template_StdDBFmt[] = "\"insert into SystemEvents (Message, Facility, FromHost, Priority, DeviceReportedTime, ReceivedAt, InfoUnitID, SysLogTag) values ('%msg%', %syslogfacility%, '%HOSTNAME%', %syslogpriority%, '%timereported:::date-mysql%', '%timegenerated:::date-mysql%', %iut%, '%syslogtag%')\",SQL";
static uchar template_StdPgSQLFmt[] = "\"insert into SystemEvents (Message, Facility, FromHost, Priority, DeviceReportedTime, ReceivedAt, InfoUnitID, SysLogTag) values ('%msg%', %syslogfacility%, '%HOSTNAME%', %syslogpriority%, '%timereported:::date-pgsql%', '%timegenerated:::date-pgsql%', %iut%, '%syslogtag%')\",STDSQL";
static uchar template_spoofadr[] = "\"%fromhost-ip%\"";
/* end templates */


/* Standard-Constructor
 */
BEGINobjConstruct(rsconf) /* be sure to specify the object type also in END macro! */
	pThis->globals.bDebugPrintTemplateList = 1;
	pThis->globals.bDebugPrintModuleList = 1;
	pThis->globals.bDebugPrintCfSysLineHandlerList = 1;
	pThis->globals.bLogStatusMsgs = DFLT_bLogStatusMsgs;
	pThis->globals.bErrMsgToStderr = 1;
	pThis->templates.root = NULL;
	pThis->templates.last = NULL;
	pThis->templates.lastStatic = NULL;
	pThis->actions.nbrActions = 0;
	CHKiRet(llInit(&pThis->rulesets.llRulesets, rulesetDestructForLinkedList,
			rulesetKeyDestruct, strcasecmp));
	/* queue params */
	pThis->globals.mainQ.iMainMsgQueueSize = 10000;
	pThis->globals.mainQ.iMainMsgQHighWtrMark = 8000;
	pThis->globals.mainQ.iMainMsgQLowWtrMark = 2000;
	pThis->globals.mainQ.iMainMsgQDiscardMark = 9800;
	pThis->globals.mainQ.iMainMsgQDiscardSeverity = 8;
	pThis->globals.mainQ.iMainMsgQueueNumWorkers = 1;
	pThis->globals.mainQ.MainMsgQueType = QUEUETYPE_FIXED_ARRAY;
	pThis->globals.mainQ.pszMainMsgQFName = NULL;
	pThis->globals.mainQ.iMainMsgQueMaxFileSize = 1024*1024;
	pThis->globals.mainQ.iMainMsgQPersistUpdCnt = 0;
	pThis->globals.mainQ.bMainMsgQSyncQeueFiles = 0;
	pThis->globals.mainQ.iMainMsgQtoQShutdown = 1500;
	pThis->globals.mainQ.iMainMsgQtoActShutdown = 1000;
	pThis->globals.mainQ.iMainMsgQtoEnq = 2000;
	pThis->globals.mainQ.iMainMsgQtoWrkShutdown = 60000;
	pThis->globals.mainQ.iMainMsgQWrkMinMsgs = 100;
	pThis->globals.mainQ.iMainMsgQDeqSlowdown = 0;
	pThis->globals.mainQ.iMainMsgQueMaxDiskSpace = 0;
	pThis->globals.mainQ.iMainMsgQueDeqBatchSize = 32;
	pThis->globals.mainQ.bMainMsgQSaveOnShutdown = 1;
	pThis->globals.mainQ.iMainMsgQueueDeqtWinFromHr = 0;
	pThis->globals.mainQ.iMainMsgQueueDeqtWinToHr = 25;
	/* end queue params */
finalize_it:
ENDobjConstruct(rsconf)


/* ConstructionFinalizer
 */
rsRetVal rsconfConstructFinalize(rsconf_t __attribute__((unused)) *pThis)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, rsconf);
	RETiRet;
}


/* destructor for the rsconf object */
BEGINobjDestruct(rsconf) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(rsconf)
	free(pThis->globals.mainQ.pszMainMsgQFName);
	llDestroy(&(pThis->rulesets.llRulesets));
ENDobjDestruct(rsconf)


/* DebugPrint support for the rsconf object */
BEGINobjDebugPrint(rsconf) /* be sure to specify the object type also in END and CODESTART macros! */
	cfgmodules_etry_t *modNode;

	dbgprintf("configuration object %p\n", pThis);
	dbgprintf("Global Settings:\n");
	dbgprintf("  bDebugPrintTemplateList.............: %d\n",
		  pThis->globals.bDebugPrintTemplateList);
	dbgprintf("  bDebugPrintModuleList               : %d\n",
		  pThis->globals.bDebugPrintModuleList);
	dbgprintf("  bDebugPrintCfSysLineHandlerList.....: %d\n",
		  pThis->globals.bDebugPrintCfSysLineHandlerList);
	dbgprintf("  bLogStatusMsgs                      : %d\n",
		  pThis->globals.bLogStatusMsgs);
	dbgprintf("  bErrMsgToStderr.....................: %d\n",
		  pThis->globals.bErrMsgToStderr);
	dbgprintf("  drop Msgs with malicious PTR Record : %d\n",
		  glbl.GetDropMalPTRMsgs());
	ruleset.DebugPrintAll(pThis);
	dbgprintf("\n");
	if(pThis->globals.bDebugPrintTemplateList)
		tplPrintList(pThis);
	if(pThis->globals.bDebugPrintModuleList)
		module.PrintList();
	if(pThis->globals.bDebugPrintCfSysLineHandlerList)
		dbgPrintCfSysLineHandlers();
	// TODO: The following code needs to be "streamlined", so far just moved over...
	dbgprintf("Main queue size %d messages.\n", pThis->globals.mainQ.iMainMsgQueueSize);
	dbgprintf("Main queue worker threads: %d, wThread shutdown: %d, Perists every %d updates.\n",
		  pThis->globals.mainQ.iMainMsgQueueNumWorkers,
		  pThis->globals.mainQ.iMainMsgQtoWrkShutdown, pThis->globals.mainQ.iMainMsgQPersistUpdCnt);
	dbgprintf("Main queue timeouts: shutdown: %d, action completion shutdown: %d, enq: %d\n",
		   pThis->globals.mainQ.iMainMsgQtoQShutdown,
		   pThis->globals.mainQ.iMainMsgQtoActShutdown, pThis->globals.mainQ.iMainMsgQtoEnq);
	dbgprintf("Main queue watermarks: high: %d, low: %d, discard: %d, discard-severity: %d\n",
		   pThis->globals.mainQ.iMainMsgQHighWtrMark, pThis->globals.mainQ.iMainMsgQLowWtrMark,
		   pThis->globals.mainQ.iMainMsgQDiscardMark, pThis->globals.mainQ.iMainMsgQDiscardSeverity);
	dbgprintf("Main queue save on shutdown %d, max disk space allowed %lld\n",
		   pThis->globals.mainQ.bMainMsgQSaveOnShutdown, pThis->globals.mainQ.iMainMsgQueMaxDiskSpace);
	/* TODO: add
	iActionRetryCount = 0;
	iActionRetryInterval = 30000;
       static int iMainMsgQtoWrkMinMsgs = 100;
	static int iMainMsgQbSaveOnShutdown = 1;
	iMainMsgQueMaxDiskSpace = 0;
	setQPROP(qqueueSetiMinMsgsPerWrkr, "$MainMsgQueueWorkerThreadMinimumMessages", 100);
	setQPROP(qqueueSetbSaveOnShutdown, "$MainMsgQueueSaveOnShutdown", 1);
	 */
	dbgprintf("Work Directory: '%s'.\n", glbl.GetWorkDir());
	ochPrintList();
	dbgprintf("Modules used in this configuration:\n");
	for(modNode = pThis->modules.root ; modNode != NULL ; modNode = modNode->next) {
		dbgprintf("    %s\n", module.GetName(modNode->pMod));
	}
CODESTARTobjDebugPrint(rsconf)
ENDobjDebugPrint(rsconf)


/* drop to specified group
 * if something goes wrong, the function never returns
 * Note that such an abort can cause damage to on-disk structures, so we should
 * re-design the "interface" in the long term. -- rgerhards, 2008-11-26
 */
static void doDropPrivGid(int iGid)
{
	int res;
	uchar szBuf[1024];

	res = setgroups(0, NULL); /* remove all supplementary group IDs */
	if(res) {
		perror("could not remove supplemental group IDs");
		exit(1);
	}
	DBGPRINTF("setgroups(0, NULL): %d\n", res);
	res = setgid(iGid);
	if(res) {
		/* if we can not set the userid, this is fatal, so let's unconditionally abort */
		perror("could not set requested group id");
		exit(1);
	}
	DBGPRINTF("setgid(%d): %d\n", iGid, res);
	snprintf((char*)szBuf, sizeof(szBuf)/sizeof(uchar), "rsyslogd's groupid changed to %d", iGid);
	logmsgInternal(NO_ERRCODE, LOG_SYSLOG|LOG_INFO, szBuf, 0);
}


/* drop to specified user
 * if something goes wrong, the function never returns
 * Note that such an abort can cause damage to on-disk structures, so we should
 * re-design the "interface" in the long term. -- rgerhards, 2008-11-19
 */
static void doDropPrivUid(int iUid)
{
	int res;
	uchar szBuf[1024];

	res = setuid(iUid);
	if(res) {
		/* if we can not set the userid, this is fatal, so let's unconditionally abort */
		perror("could not set requested userid");
		exit(1);
	}
	DBGPRINTF("setuid(%d): %d\n", iUid, res);
	snprintf((char*)szBuf, sizeof(szBuf)/sizeof(uchar), "rsyslogd's userid changed to %d", iUid);
	logmsgInternal(NO_ERRCODE, LOG_SYSLOG|LOG_INFO, szBuf, 0);
}



/* drop privileges. This will drop to the configured privileges, if
 * set by the user. After this method has been executed, the previous
 * privileges can no be re-gained.
 */
static inline rsRetVal
dropPrivileges(rsconf_t *cnf)
{
	DEFiRet;

	/* If instructed to do so, we now drop privileges. Note that this is not 100% secure,
	 * because outputs are already running at this time. However, we can implement
	 * dropping of privileges rather quickly and it will work in many cases. While it is not
	 * the ultimate solution, the current one is still much better than not being able to
	 * drop privileges at all. Doing it correctly, requires a change in architecture, which
	 * we should do over time. TODO -- rgerhards, 2008-11-19
	 */
	if(cnf->globals.gidDropPriv != 0) {
		doDropPrivGid(ourConf->globals.gidDropPriv);
		DBGPRINTF("group privileges have been dropped to gid %u\n", (unsigned) 
			  ourConf->globals.gidDropPriv);
	}

	if(cnf->globals.uidDropPriv != 0) {
		doDropPrivUid(ourConf->globals.uidDropPriv);
		DBGPRINTF("user privileges have been dropped to uid %u\n", (unsigned) 
			  ourConf->globals.uidDropPriv);
	}

	RETiRet;
}


/* Tell input modules that the config parsing stage is over.  */
static rsRetVal
tellModulesConfigLoadDone(void)
{
	cfgmodules_etry_t *node;

	BEGINfunc
	DBGPRINTF("telling modules that config load for %p is done\n", loadConf);
	node = module.GetNxtCnfType(loadConf, NULL, eMOD_ANY);
	while(node != NULL) {
		if(node->pMod->beginCnfLoad != NULL)
			node->pMod->endCnfLoad(node->modCnf);
		node = module.GetNxtCnfType(runConf, node, eMOD_IN);
	}

	ENDfunc
	return RS_RET_OK; /* intentional: we do not care about module errors */
}


/* Tell input modules to verify config object */
static rsRetVal
tellModulesCheckConfig(void)
{
	cfgmodules_etry_t *node;
	rsRetVal localRet;

	BEGINfunc
	DBGPRINTF("telling modules to check config %p\n", loadConf);
	node = module.GetNxtCnfType(loadConf, NULL, eMOD_ANY);
	while(node != NULL) {
		if(node->pMod->beginCnfLoad != NULL) {
			localRet = node->pMod->checkCnf(node->modCnf);
			DBGPRINTF("module %s tells us config can %sbe activated\n",
					  node->pMod->pszName, (localRet == RS_RET_OK) ? "" : "NOT ");
			if(localRet == RS_RET_OK) {
				node->canActivate = 1;
			} else {
				node->canActivate = 0;
			}
		}
		node = module.GetNxtCnfType(runConf, node, eMOD_IN);
	}

	ENDfunc
	return RS_RET_OK; /* intentional: we do not care about module errors */
}


/* Tell modules to activate current running config (pre privilege drop) */
static rsRetVal
tellModulesActivateConfigPrePrivDrop(void)
{
	cfgmodules_etry_t *node;
	rsRetVal localRet;

	BEGINfunc
	DBGPRINTF("telling modules to activate config (before dropping privs) %p\n", runConf);
	node = module.GetNxtCnfType(runConf, NULL, eMOD_ANY);
	while(node != NULL) {
		if(   node->pMod->beginCnfLoad != NULL
		   && node->pMod->activateCnfPrePrivDrop != NULL
		   && node->canActivate) {
			DBGPRINTF("pre priv drop activating config %p for module %s\n",
				  runConf, node->pMod->pszName);
			localRet = node->pMod->activateCnfPrePrivDrop(node->modCnf);
			if(localRet != RS_RET_OK) {
				errmsg.LogError(0, localRet, "activation of module %s failed",
						node->pMod->pszName);
			node->canActivate = 0; /* in a sense, could not activate... */
			}
		}
		node = module.GetNxtCnfType(runConf, node, eMOD_IN);
	}

	ENDfunc
	return RS_RET_OK; /* intentional: we do not care about module errors */
}


/* Tell modules to activate current running config */
static rsRetVal
tellModulesActivateConfig(void)
{
	cfgmodules_etry_t *node;
	rsRetVal localRet;

	BEGINfunc
	DBGPRINTF("telling modules to activate config %p\n", runConf);
	node = module.GetNxtCnfType(runConf, NULL, eMOD_ANY);
	while(node != NULL) {
		if(node->pMod->beginCnfLoad != NULL && node->canActivate) {
			DBGPRINTF("activating config %p for module %s\n",
				  runConf, node->pMod->pszName);
			localRet = node->pMod->activateCnf(node->modCnf);
			if(localRet != RS_RET_OK) {
				errmsg.LogError(0, localRet, "activation of module %s failed",
						node->pMod->pszName);
			node->canActivate = 0; /* in a sense, could not activate... */
			}
		}
		node = module.GetNxtCnfType(runConf, node, eMOD_IN);
	}

	ENDfunc
	return RS_RET_OK; /* intentional: we do not care about module errors */
}


/* Actually run the input modules.  This happens after privileges are dropped,
 * if that is requested.
 */
static rsRetVal
runInputModules(void)
{
	cfgmodules_etry_t *node;
	int bNeedsCancel;

	BEGINfunc
	node = module.GetNxtCnfType(runConf, NULL, eMOD_IN);
	while(node != NULL) {
		if(node->canRun) {
			DBGPRINTF("running module %s with config %p\n", node->pMod->pszName, node);
			bNeedsCancel = (node->pMod->isCompatibleWithFeature(sFEATURENonCancelInputTermination) == RS_RET_OK) ?
				       0 : 1;
			thrdCreate(node->pMod->mod.im.runInput, node->pMod->mod.im.afterRun, bNeedsCancel);
		}
		node = module.GetNxtCnfType(runConf, node, eMOD_IN);
	}

	ENDfunc
	return RS_RET_OK; /* intentional: we do not care about module errors */
}


/* Make the modules check if they are ready to start.
 */
static rsRetVal
startInputModules(void)
{
	DEFiRet;
	cfgmodules_etry_t *node;

	node = module.GetNxtCnfType(runConf, NULL, eMOD_IN);
	while(node != NULL) {
		if(node->canActivate) {
			iRet = node->pMod->mod.im.willRun();
			node->canRun = (iRet == RS_RET_OK);
			if(!node->canRun) {
				DBGPRINTF("module %s will not run, iRet %d\n", node->pMod->pszName, iRet);
			}
		} else {
			node->canRun = 0;
		}
		node = module.GetNxtCnfType(runConf, node, eMOD_IN);
	}

	ENDfunc
	return RS_RET_OK; /* intentional: we do not care about module errors */
}


/* activate the main queue */
static inline rsRetVal
activateMainQueue()
{
	DEFiRet;
	/* create message queue */
	CHKiRet_Hdlr(createMainQueue(&pMsgQueue, UCHAR_CONSTANT("main Q"))) {
		/* no queue is fatal, we need to give up in that case... */
		fprintf(stderr, "fatal error %d: could not create message queue - rsyslogd can not run!\n", iRet);
		FINALIZE;
	}

	bHaveMainQueue = (ourConf->globals.mainQ.MainMsgQueType == QUEUETYPE_DIRECT) ? 0 : 1;
	DBGPRINTF("Main processing queue is initialized and running\n");
finalize_it:
	RETiRet;
}


/* Activate an already-loaded configuration. The configuration will become
 * the new running conf (if successful). Note that in theory this method may
 * be called when there already is a running conf. In practice, the current
 * version of rsyslog does not support this. Future versions probably will.
 * Begun 2011-04-20, rgerhards
 */
rsRetVal
activate(rsconf_t *cnf)
{
	DEFiRet;

	/* at this point, we "switch" over to the running conf */
	runConf = cnf;
#	if	0 /* currently the DAG is not supported -- code missing! */
	/* TODO: re-enable this functionality some time later! */
	/* check if we need to generate a config DAG and, if so, do that */
	if(ourConf->globals.pszConfDAGFile != NULL)
		generateConfigDAG(ourConf->globals.pszConfDAGFile);
#	endif
	tellModulesConfigLoadDone();
	tellModulesCheckConfig();

	/* the output part and the queue is now ready to run. So it is a good time
	 * to initialize the inputs. Please note that the net code above should be
	 * shuffled to down here once we have everything in input modules.
	 * rgerhards, 2007-12-14
	 * NOTE: as of 2009-06-29, the input modules are initialized, but not yet run.
	 * Keep in mind. though, that the outputs already run if the queue was
	 * persisted to disk. -- rgerhards
	 */
	tellModulesActivateConfigPrePrivDrop();

	CHKiRet(dropPrivileges(cnf));

	tellModulesActivateConfig();
	startInputModules();
	CHKiRet(activateActions());
	CHKiRet(activateMainQueue());
	/* finally let the inputs run... */
	runInputModules();

	dbgprintf("configuration %p activated\n", cnf);

finalize_it:
	RETiRet;
}


/* -------------------- some legacy config handlers --------------------
 * TODO: move to conf.c?
 */

/* legacy config system: set the action resume interval */
static rsRetVal setActionResumeInterval(void __attribute__((unused)) *pVal, int iNewVal)
{
	return actionSetGlobalResumeInterval(iNewVal);
}


/* this method is needed to shuffle the current conf object down to the
 * IncludeConfig handler.
 */
static rsRetVal
doIncludeLine(void *pVal, uchar *pNewVal)
{
	DEFiRet;
	iRet = conf.doIncludeLine(ourConf, pVal, pNewVal);
	free(pNewVal);
	RETiRet;
}


/* set the processes umask (upon configuration request) */
static rsRetVal
setUmask(void __attribute__((unused)) *pVal, int iUmask)
{
#warning this *really* needs to be done differently!
	umask(iUmask);
	DBGPRINTF("umask set to 0%3.3o.\n", iUmask);

	return RS_RET_OK;
}


/* set the maximum message size */
static rsRetVal setMaxMsgSize(void __attribute__((unused)) *pVal, long iNewVal)
{
	return glbl.SetMaxLine(iNewVal);
}


/* Switch the default ruleset (that, what servcies bind to if nothing specific
 * is specified).
 * rgerhards, 2009-06-12
 */
static rsRetVal
setDefaultRuleset(void __attribute__((unused)) *pVal, uchar *pszName)
{
	DEFiRet;

	CHKiRet(ruleset.SetDefaultRuleset(ourConf, pszName));

finalize_it:
	free(pszName); /* no longer needed */
	RETiRet;
}


/* Switch to either an already existing rule set or start a new one. The
 * named rule set becomes the new "current" rule set (what means that new
 * actions are added to it).
 * rgerhards, 2009-06-12
 */
static rsRetVal
setCurrRuleset(void __attribute__((unused)) *pVal, uchar *pszName)
{
	ruleset_t *pRuleset;
	rsRetVal localRet;
	DEFiRet;

	localRet = ruleset.SetCurrRuleset(ourConf, pszName);

	if(localRet == RS_RET_NOT_FOUND) {
		DBGPRINTF("begin new current rule set '%s'\n", pszName);
		CHKiRet(ruleset.Construct(&pRuleset));
		CHKiRet(ruleset.SetName(ourConf, pRuleset, pszName));
		CHKiRet(ruleset.ConstructFinalize(ourConf, pRuleset));
	} else {
		ABORT_FINALIZE(localRet);
	}

finalize_it:
	free(pszName); /* no longer needed */
	RETiRet;
}


/* set the main message queue mode
 * rgerhards, 2008-01-03
 */
static rsRetVal setMainMsgQueType(void __attribute__((unused)) *pVal, uchar *pszType)
{
	DEFiRet;

	if (!strcasecmp((char *) pszType, "fixedarray")) {
		loadConf->globals.mainQ.MainMsgQueType = QUEUETYPE_FIXED_ARRAY;
		DBGPRINTF("main message queue type set to FIXED_ARRAY\n");
	} else if (!strcasecmp((char *) pszType, "linkedlist")) {
		loadConf->globals.mainQ.MainMsgQueType = QUEUETYPE_LINKEDLIST;
		DBGPRINTF("main message queue type set to LINKEDLIST\n");
	} else if (!strcasecmp((char *) pszType, "disk")) {
		loadConf->globals.mainQ.MainMsgQueType = QUEUETYPE_DISK;
		DBGPRINTF("main message queue type set to DISK\n");
	} else if (!strcasecmp((char *) pszType, "direct")) {
		loadConf->globals.mainQ.MainMsgQueType = QUEUETYPE_DIRECT;
		DBGPRINTF("main message queue type set to DIRECT (no queueing at all)\n");
	} else {
		errmsg.LogError(0, RS_RET_INVALID_PARAMS, "unknown mainmessagequeuetype parameter: %s", (char *) pszType);
		iRet = RS_RET_INVALID_PARAMS;
	}
	free(pszType); /* no longer needed */

	RETiRet;
}


/* -------------------- end legacy config handlers -------------------- */


/* set the processes max number ob files (upon configuration request)
 * 2009-04-14 rgerhards
 */
static rsRetVal setMaxFiles(void __attribute__((unused)) *pVal, int iFiles)
{
// TODO this must use a local var, then carry out action during activate!
	struct rlimit maxFiles;
	char errStr[1024];
	DEFiRet;

	maxFiles.rlim_cur = iFiles;
	maxFiles.rlim_max = iFiles;

	if(setrlimit(RLIMIT_NOFILE, &maxFiles) < 0) {
		/* NOTE: under valgrind, we seem to be unable to extend the size! */
		rs_strerror_r(errno, errStr, sizeof(errStr));
		errmsg.LogError(0, RS_RET_ERR_RLIM_NOFILE, "could not set process file limit to %d: %s [kernel max %ld]",
				iFiles, errStr, (long) maxFiles.rlim_max);
		ABORT_FINALIZE(RS_RET_ERR_RLIM_NOFILE);
	}
#ifdef USE_UNLIMITED_SELECT
	glbl.SetFdSetSize(howmany(iFiles, __NFDBITS) * sizeof (fd_mask));
#endif
	DBGPRINTF("Max number of files set to %d [kernel max %ld].\n", iFiles, (long) maxFiles.rlim_max);

finalize_it:
	RETiRet;
}


/* legac config system: reset config variables to default values.  */
static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	loadConf->globals.bLogStatusMsgs = DFLT_bLogStatusMsgs;
	loadConf->globals.bDebugPrintTemplateList = 1;
	loadConf->globals.bDebugPrintCfSysLineHandlerList = 1;
	loadConf->globals.bDebugPrintModuleList = 1;
	loadConf->globals.bAbortOnUncleanConfig = 0;
	loadConf->globals.bReduceRepeatMsgs = 0;
	free(loadConf->globals.mainQ.pszMainMsgQFName);
	loadConf->globals.mainQ.pszMainMsgQFName = NULL;
	loadConf->globals.mainQ.iMainMsgQueueSize = 10000;
	loadConf->globals.mainQ.iMainMsgQHighWtrMark = 8000;
	loadConf->globals.mainQ.iMainMsgQLowWtrMark = 2000;
	loadConf->globals.mainQ.iMainMsgQDiscardMark = 9800;
	loadConf->globals.mainQ.iMainMsgQDiscardSeverity = 8;
	loadConf->globals.mainQ.iMainMsgQueMaxFileSize = 1024 * 1024;
	loadConf->globals.mainQ.iMainMsgQueueNumWorkers = 1;
	loadConf->globals.mainQ.iMainMsgQPersistUpdCnt = 0;
	loadConf->globals.mainQ.bMainMsgQSyncQeueFiles = 0;
	loadConf->globals.mainQ.iMainMsgQtoQShutdown = 1500;
	loadConf->globals.mainQ.iMainMsgQtoActShutdown = 1000;
	loadConf->globals.mainQ.iMainMsgQtoEnq = 2000;
	loadConf->globals.mainQ.iMainMsgQtoWrkShutdown = 60000;
	loadConf->globals.mainQ.iMainMsgQWrkMinMsgs = 100;
	loadConf->globals.mainQ.iMainMsgQDeqSlowdown = 0;
	loadConf->globals.mainQ.bMainMsgQSaveOnShutdown = 1;
	loadConf->globals.mainQ.MainMsgQueType = QUEUETYPE_FIXED_ARRAY;
	loadConf->globals.mainQ.iMainMsgQueMaxDiskSpace = 0;
	loadConf->globals.mainQ.iMainMsgQueDeqBatchSize = 32;

	return RS_RET_OK;
}


/* legacy config system: set the action resume interval */
static rsRetVal
setModDir(void __attribute__((unused)) *pVal, uchar* pszNewVal)
{
	DEFiRet;
	iRet = module.SetModDir(pszNewVal);
	free(pszNewVal);
	RETiRet;
}


/* "load" a build in module and register it for the current load config */
static rsRetVal
regBuildInModule(rsRetVal (*modInit)(), uchar *name, void *pModHdlr)
{
	modInfo_t *pMod;
	DEFiRet;
	CHKiRet(module.doModInit(modInit, name, pModHdlr, &pMod));
	addModToCnfList(pMod);
finalize_it:
	RETiRet;
}


/* load build-in modules
 * very first version begun on 2007-07-23 by rgerhards
 */
static rsRetVal
loadBuildInModules()
{
	DEFiRet;

	CHKiRet(regBuildInModule(modInitFile, UCHAR_CONSTANT("builtin-file"), NULL));
	CHKiRet(regBuildInModule(modInitPipe, UCHAR_CONSTANT("builtin-pipe"), NULL));
	CHKiRet(regBuildInModule(modInitShell, UCHAR_CONSTANT("builtin-shell"), NULL));
	CHKiRet(regBuildInModule(modInitDiscard, UCHAR_CONSTANT("builtin-discard"), NULL));
#	ifdef SYSLOG_INET
	CHKiRet(regBuildInModule(modInitFwd, UCHAR_CONSTANT("builtin-fwd"), NULL));
#	endif

	/* dirty, but this must be for the time being: the usrmsg module must always be
	 * loaded as last module. This is because it processes any type of action selector.
	 * If we load it before other modules, these others will never have a chance of
	 * working with the config file. We may change that implementation so that a user name
	 * must start with an alnum, that would definitely help (but would it break backwards
	 * compatibility?). * rgerhards, 2007-07-23
	 * User names now must begin with:
	 *   [a-zA-Z0-9_.]
	 */
	CHKiRet(regBuildInModule(modInitUsrMsg, (uchar*) "builtin-usrmsg", NULL));

	/* load build-in parser modules */
	CHKiRet(regBuildInModule(modInitpmrfc5424, UCHAR_CONSTANT("builtin-pmrfc5424"), NULL));
	CHKiRet(regBuildInModule(modInitpmrfc3164, UCHAR_CONSTANT("builtin-pmrfc3164"), NULL));

	/* and set default parser modules. Order is *very* important, legacy
	 * (3164) parser needs to go last! */
	CHKiRet(parser.AddDfltParser(UCHAR_CONSTANT("rsyslog.rfc5424")));
	CHKiRet(parser.AddDfltParser(UCHAR_CONSTANT("rsyslog.rfc3164")));

	/* load build-in strgen modules */
	CHKiRet(regBuildInModule(modInitsmfile, UCHAR_CONSTANT("builtin-smfile"), NULL));
	CHKiRet(regBuildInModule(modInitsmtradfile, UCHAR_CONSTANT("builtin-smtradfile"), NULL));
	CHKiRet(regBuildInModule(modInitsmfwd, UCHAR_CONSTANT("builtin-smfwd"), NULL));
	CHKiRet(regBuildInModule(modInitsmtradfwd, UCHAR_CONSTANT("builtin-smtradfwd"), NULL));

finalize_it:
	if(iRet != RS_RET_OK) {
		/* we need to do fprintf, as we do not yet have an error reporting system
		 * in place.
		 */
		fprintf(stderr, "fatal error: could not activate built-in modules. Error code %d.\n",
			iRet);
	}
	RETiRet;
}


/* intialize the legacy config system */
static inline rsRetVal 
initLegacyConf(void)
{
	DEFiRet;
	uchar *pTmp;
	ruleset_t *pRuleset;

	DBGPRINTF("doing legacy config system init\n");
	/* construct the default ruleset */
	ruleset.Construct(&pRuleset);
	ruleset.SetName(loadConf, pRuleset, UCHAR_CONSTANT("RSYSLOG_DefaultRuleset"));
	ruleset.ConstructFinalize(loadConf, pRuleset);

	/* now register config handlers */
	CHKiRet(regCfSysLineHdlr((uchar *)"sleep", 0, eCmdHdlrGoneAway,
		NULL, NULL, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"logrsyslogstatusmessages", 0, eCmdHdlrBinary,
		NULL, &loadConf->globals.bLogStatusMsgs, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"errormessagestostderr", 0, eCmdHdlrBinary,
		NULL, &loadConf->globals.bErrMsgToStderr, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"abortonuncleanconfig", 0, eCmdHdlrBinary,
		NULL, &loadConf->globals.bAbortOnUncleanConfig, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"repeatedmsgreduction", 0, eCmdHdlrBinary,
		NULL, &loadConf->globals.bReduceRepeatMsgs, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"debugprinttemplatelist", 0, eCmdHdlrBinary,
		NULL, &(loadConf->globals.bDebugPrintTemplateList), NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"debugprintmodulelist", 0, eCmdHdlrBinary,
		NULL, &(loadConf->globals.bDebugPrintModuleList), NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"debugprintcfsyslinehandlerlist", 0, eCmdHdlrBinary,
		 NULL, &(loadConf->globals.bDebugPrintCfSysLineHandlerList), NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"privdroptouser", 0, eCmdHdlrUID,
		NULL, &loadConf->globals.uidDropPriv, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"privdroptouserid", 0, eCmdHdlrInt,
		NULL, &loadConf->globals.uidDropPriv, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"privdroptogroup", 0, eCmdHdlrGID,
		NULL, &loadConf->globals.gidDropPriv, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"privdroptogroupid", 0, eCmdHdlrGID,
		NULL, &loadConf->globals.gidDropPriv, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"generateconfiggraph", 0, eCmdHdlrGetWord,
		NULL, &loadConf->globals.pszConfDAGFile, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"maxopenfiles", 0, eCmdHdlrInt,
		setMaxFiles, NULL, NULL, eConfObjGlobal));

	CHKiRet(regCfSysLineHdlr((uchar *)"actionresumeinterval", 0, eCmdHdlrInt,
		setActionResumeInterval, NULL, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"modload", 0, eCmdHdlrCustomHandler,
		conf.doModLoad, NULL, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"includeconfig", 0, eCmdHdlrCustomHandler,
		doIncludeLine, NULL, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"umask", 0, eCmdHdlrFileCreateMode,
		setUmask, NULL, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"maxmessagesize", 0, eCmdHdlrSize,
		setMaxMsgSize, NULL, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"defaultruleset", 0, eCmdHdlrGetWord,
		setDefaultRuleset, NULL, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"ruleset", 0, eCmdHdlrGetWord,
		setCurrRuleset, NULL, NULL, eConfObjGlobal));

	/* handler for "larger" config statements (tie into legacy conf system) */
	CHKiRet(regCfSysLineHdlr((uchar *)"template", 0, eCmdHdlrCustomHandler,
		conf.doNameLine, (void*)DIR_TEMPLATE, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"outchannel", 0, eCmdHdlrCustomHandler,
		conf.doNameLine, (void*)DIR_OUTCHANNEL, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"allowedsender", 0, eCmdHdlrCustomHandler,
		conf.doNameLine, (void*)DIR_ALLOWEDSENDER, NULL, eConfObjGlobal));

	/* the following are parameters for the main message queue. I have the
	 * strong feeling that this needs to go to a different space, but that
	 * feeling may be wrong - we'll see how things evolve.
	 * rgerhards, 2011-04-21
	 */
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuefilename", 0, eCmdHdlrGetWord,
		NULL, &loadConf->globals.mainQ.pszMainMsgQFName, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuesize", 0, eCmdHdlrInt,
		NULL, &loadConf->globals.mainQ.iMainMsgQueueSize, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuehighwatermark", 0, eCmdHdlrInt,
		NULL, &loadConf->globals.mainQ.iMainMsgQHighWtrMark, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuelowwatermark", 0, eCmdHdlrInt,
		NULL, &loadConf->globals.mainQ.iMainMsgQLowWtrMark, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuediscardmark", 0, eCmdHdlrInt,
		NULL, &loadConf->globals.mainQ.iMainMsgQDiscardMark, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuediscardseverity", 0, eCmdHdlrSeverity,
		NULL, &loadConf->globals.mainQ.iMainMsgQDiscardSeverity, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuecheckpointinterval", 0, eCmdHdlrInt,
		NULL, &loadConf->globals.mainQ.iMainMsgQPersistUpdCnt, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuesyncqueuefiles", 0, eCmdHdlrBinary,
		NULL, &loadConf->globals.mainQ.bMainMsgQSyncQeueFiles, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuetype", 0, eCmdHdlrGetWord,
		setMainMsgQueType, NULL, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueueworkerthreads", 0, eCmdHdlrInt,
		NULL, &loadConf->globals.mainQ.iMainMsgQueueNumWorkers, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuetimeoutshutdown", 0, eCmdHdlrInt,
		NULL, &loadConf->globals.mainQ.iMainMsgQtoQShutdown, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuetimeoutactioncompletion", 0, eCmdHdlrInt,
		NULL, &loadConf->globals.mainQ.iMainMsgQtoActShutdown, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuetimeoutenqueue", 0, eCmdHdlrInt,
		NULL, &loadConf->globals.mainQ.iMainMsgQtoEnq, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueueworkertimeoutthreadshutdown", 0, eCmdHdlrInt,
		NULL, &loadConf->globals.mainQ.iMainMsgQtoWrkShutdown, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuedequeueslowdown", 0, eCmdHdlrInt,
		NULL, &loadConf->globals.mainQ.iMainMsgQDeqSlowdown, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueueworkerthreadminimummessages", 0, eCmdHdlrInt,
		NULL, &loadConf->globals.mainQ.iMainMsgQWrkMinMsgs, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuemaxfilesize", 0, eCmdHdlrSize,
		NULL, &loadConf->globals.mainQ.iMainMsgQueMaxFileSize, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuedequeuebatchsize", 0, eCmdHdlrSize,
		NULL, &loadConf->globals.mainQ.iMainMsgQueDeqBatchSize, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuemaxdiskspace", 0, eCmdHdlrSize,
		NULL, &loadConf->globals.mainQ.iMainMsgQueMaxDiskSpace, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuesaveonshutdown", 0, eCmdHdlrBinary,
		NULL, &loadConf->globals.mainQ.bMainMsgQSaveOnShutdown, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuedequeuetimebegin", 0, eCmdHdlrInt,
		NULL, &loadConf->globals.mainQ.iMainMsgQueueDeqtWinFromHr, NULL, eConfObjGlobal));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuedequeuetimeend", 0, eCmdHdlrInt,
		NULL, &loadConf->globals.mainQ.iMainMsgQueueDeqtWinToHr, NULL, eConfObjGlobal));
	/* moddir is a bit hard problem -- because it actually needs to
	 * modify a setting that is specific to module.c. The important point
	 * is that this action MUST actually be carried out during config load,
	 * because we must load modules in order to get their config extensions
	 * (no way around).
	 * TODO: think about a clean solution
	 */
	CHKiRet(regCfSysLineHdlr((uchar *)"moddir", 0, eCmdHdlrGetWord,
		setModDir, NULL, NULL, eConfObjGlobal));

	/* finally, the reset handler */
	CHKiRet(regCfSysLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler,
		resetConfigVariables, NULL, NULL, eConfObjGlobal));

	/* initialize the build-in templates */
	pTmp = template_DebugFormat;
	tplAddLine(ourConf, "RSYSLOG_DebugFormat", &pTmp);
	pTmp = template_SyslogProtocol23Format;
	tplAddLine(ourConf, "RSYSLOG_SyslogProtocol23Format", &pTmp);
	pTmp = template_FileFormat; /* new format for files with high-precision stamp */
	tplAddLine(ourConf, "RSYSLOG_FileFormat", &pTmp);
	pTmp = template_TraditionalFileFormat;
	tplAddLine(ourConf, "RSYSLOG_TraditionalFileFormat", &pTmp);
	pTmp = template_WallFmt;
	tplAddLine(ourConf, " WallFmt", &pTmp);
	pTmp = template_ForwardFormat;
	tplAddLine(ourConf, "RSYSLOG_ForwardFormat", &pTmp);
	pTmp = template_TraditionalForwardFormat;
	tplAddLine(ourConf, "RSYSLOG_TraditionalForwardFormat", &pTmp);
	pTmp = template_StdUsrMsgFmt;
	tplAddLine(ourConf, " StdUsrMsgFmt", &pTmp);
	pTmp = template_StdDBFmt;
	tplAddLine(ourConf, " StdDBFmt", &pTmp);
        pTmp = template_StdPgSQLFmt;
        tplAddLine(ourConf, " StdPgSQLFmt", &pTmp);
        pTmp = template_spoofadr;
        tplLastStaticInit(ourConf, tplAddLine(ourConf, "RSYSLOG_omudpspoofDfltSourceTpl", &pTmp));

finalize_it:
	RETiRet;
}


/* validate the current configuration, generate error messages, do 
 * optimizations, etc, etc,...
 */
static inline rsRetVal
validateConf(void)
{
	DEFiRet;

	/* some checks */
	if(ourConf->globals.mainQ.iMainMsgQueueNumWorkers < 1) {
		errmsg.LogError(0, NO_ERRCODE, "$MainMsgQueueNumWorkers must be at least 1! Set to 1.\n");
		ourConf->globals.mainQ.iMainMsgQueueNumWorkers = 1;
	}

	if(ourConf->globals.mainQ.MainMsgQueType == QUEUETYPE_DISK) {
		errno = 0;	/* for logerror! */
		if(glbl.GetWorkDir() == NULL) {
			errmsg.LogError(0, NO_ERRCODE, "No $WorkDirectory specified - can not run main message queue in 'disk' mode. "
				 "Using 'FixedArray' instead.\n");
			ourConf->globals.mainQ.MainMsgQueType = QUEUETYPE_FIXED_ARRAY;
		}
		if(ourConf->globals.mainQ.pszMainMsgQFName == NULL) {
			errmsg.LogError(0, NO_ERRCODE, "No $MainMsgQueueFileName specified - can not run main message queue in "
				 "'disk' mode. Using 'FixedArray' instead.\n");
			ourConf->globals.mainQ.MainMsgQueType = QUEUETYPE_FIXED_ARRAY;
		}
	}
	RETiRet;
}


/* Load a configuration. This will do all necessary steps to create
 * the in-memory representation of the configuration, including support
 * for multiple configuration languages.
 * Note that to support the legacy language we must provide some global
 * object that holds the currently-being-loaded config ptr.
 * Begun 2011-04-20, rgerhards
 */
rsRetVal
load(rsconf_t **cnf, uchar *confFile)
{
	rsRetVal localRet;
	int iNbrActions;
	int bHadConfigErr = 0;
	char cbuf[BUFSIZ];
	DEFiRet;

	CHKiRet(rsconfConstruct(&loadConf));
ourConf = loadConf; // TODO: remove, once ourConf is gone!

	CHKiRet(loadBuildInModules());
	CHKiRet(initLegacyConf());

	/* open the configuration file */
	localRet = conf.processConfFile(loadConf, confFile);
	CHKiRet(conf.GetNbrActActions(loadConf, &iNbrActions));

	if(localRet != RS_RET_OK && localRet != RS_RET_NONFATAL_CONFIG_ERR) {
		errmsg.LogError(0, localRet, "CONFIG ERROR: could not interpret master config file '%s'.", confFile);
		bHadConfigErr = 1;
	} else if(iNbrActions == 0) {
		errmsg.LogError(0, RS_RET_NO_ACTIONS, "CONFIG ERROR: there are no active actions configured. Inputs will "
			 "run, but no output whatsoever is created.");
		bHadConfigErr = 1;
	}

	if((localRet != RS_RET_OK && localRet != RS_RET_NONFATAL_CONFIG_ERR) || iNbrActions == 0) {

		/* rgerhards: this code is executed to set defaults when the
		 * config file could not be opened. We might think about
		 * abandoning the run in this case - but this, too, is not
		 * very clever... So we stick with what we have.
		 * We ignore any errors while doing this - we would be lost anyhow...
		 */
		errmsg.LogError(0, NO_ERRCODE, "EMERGENCY CONFIGURATION ACTIVATED - fix rsyslog config file!");

		/* note: we previously used _POSIY_TTY_NAME_MAX+1, but this turned out to be
		 * too low on linux... :-S   -- rgerhards, 2008-07-28
		 */
		char szTTYNameBuf[128];
		rule_t *pRule = NULL; /* initialization to NULL is *vitally* important! */
		conf.cfline(loadConf, UCHAR_CONSTANT("*.ERR\t" _PATH_CONSOLE), &pRule);
		conf.cfline(loadConf, UCHAR_CONSTANT("syslog.*\t" _PATH_CONSOLE), &pRule);
		conf.cfline(loadConf, UCHAR_CONSTANT("*.PANIC\t*"), &pRule);
		conf.cfline(loadConf, UCHAR_CONSTANT("syslog.*\troot"), &pRule);
		if(ttyname_r(0, szTTYNameBuf, sizeof(szTTYNameBuf)) == 0) {
			snprintf(cbuf,sizeof(cbuf), "*.*\t%s", szTTYNameBuf);
			conf.cfline(loadConf, (uchar*)cbuf, &pRule);
		} else {
			DBGPRINTF("error %d obtaining controlling terminal, not using that emergency rule\n", errno);
		}
		ruleset.AddRule(loadConf, ruleset.GetCurrent(loadConf), &pRule);
	}

	CHKiRet(validateConf());


	/* return warning state if we had some acceptable problems */
	if(bHadConfigErr) {
		iRet = RS_RET_NONFATAL_CONFIG_ERR;
	}

	/* we are done checking the config - now validate if we should actually run or not.
	 * If not, terminate. -- rgerhards, 2008-07-25
	 * TODO: iConfigVerify -- should it be pulled from the config, or leave as is (option)?
	 */
	if(iConfigVerify) {
		if(iRet == RS_RET_OK)
			iRet = RS_RET_VALIDATION_RUN;
		FINALIZE;
	}

	/* all OK, pass loaded conf to caller */
	*cnf = loadConf;
// TODO: enable this once all config code is moved to here!	loadConf = NULL;

	dbgprintf("rsyslog finished loading initial config %p\n", loadConf);
	rsconfDebugPrint(loadConf);

finalize_it:
	RETiRet;
}


/* queryInterface function
 */
BEGINobjQueryInterface(rsconf)
CODESTARTobjQueryInterface(rsconf)
	if(pIf->ifVersion != rsconfCURR_IF_VERSION) { /* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->Construct = rsconfConstruct;
	pIf->ConstructFinalize = rsconfConstructFinalize;
	pIf->Destruct = rsconfDestruct;
	pIf->DebugPrint = rsconfDebugPrint;
	pIf->Load = load;
	pIf->Activate = activate;
finalize_it:
ENDobjQueryInterface(rsconf)


/* Initialize the rsconf class. Must be called as the very first method
 * before anything else is called inside this class.
 */
BEGINObjClassInit(rsconf, 1, OBJ_IS_CORE_MODULE) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(ruleset, CORE_COMPONENT));
	CHKiRet(objUse(module, CORE_COMPONENT));
	CHKiRet(objUse(conf, CORE_COMPONENT));
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(parser, CORE_COMPONENT));

	/* now set our own handlers */
	OBJSetMethodHandler(objMethod_DEBUGPRINT, rsconfDebugPrint);
	OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, rsconfConstructFinalize);
ENDObjClassInit(rsconf)


/* De-initialize the rsconf class.
 */
BEGINObjClassExit(rsconf, OBJ_IS_CORE_MODULE) /* class, version */
	objRelease(ruleset, CORE_COMPONENT);
	objRelease(module, CORE_COMPONENT);
	objRelease(conf, CORE_COMPONENT);
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(glbl, CORE_COMPONENT);
	objRelease(parser, CORE_COMPONENT);
ENDObjClassExit(rsconf)

/* vi:set ai:
 */
