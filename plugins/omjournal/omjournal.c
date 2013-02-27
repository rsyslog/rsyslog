/* omjournal.c
 * send messages to the Linux Journal. This is meant to be used
 * in cases where journal serves as the whole system log database.
 * Note that we may get into a loop if journald re-injects messages
 * into the syslog stream and we read that via imuxsock. Thus there
 * is an option in imuxsock to ignore messages from ourselves 
 * (actually from our pid). So there are some module-interdependencies.
 *
 * Copyright 2013 Adiscon GmbH.
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
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include <systemd/sd-journal.h>

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("omjournal")


DEFobjCurrIf(errmsg);
DEF_OMOD_STATIC_DATA

/* config variables */


typedef struct _instanceData {
} instanceData;

struct modConfData_s {
	rsconf_t *pConf;	/* our overall config object */
};
static modConfData_t *loadModConf = NULL;/* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL;/* modConf ptr to use for the current exec process */

BEGINbeginCnfLoad
CODESTARTbeginCnfLoad
	loadModConf = pModConf;
	pModConf->pConf = pConf;
ENDbeginCnfLoad

BEGINendCnfLoad
CODESTARTendCnfLoad
ENDendCnfLoad

BEGINcheckCnf
CODESTARTcheckCnf
ENDcheckCnf

BEGINactivateCnf
CODESTARTactivateCnf
	runModConf = pModConf;
ENDactivateCnf

BEGINfreeCnf
CODESTARTfreeCnf
ENDfreeCnf


BEGINcreateInstance
CODESTARTcreateInstance
ENDcreateInstance


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
ENDisCompatibleWithFeature


BEGINfreeInstance
CODESTARTfreeInstance
ENDfreeInstance


BEGINnewActInst
CODESTARTnewActInst
	/* Note: we currently do not have any parameters, so we do not need
	 * the lst ptr. However, we will most probably need params in the 
	 * future.
	 */
	DBGPRINTF("newActInst (mmjournal)\n");
	CODE_STD_STRING_REQUESTnewActInst(1)
	CHKiRet(OMSRsetEntry(*ppOMSR, 0, NULL, OMSR_TPL_AS_MSG));
	CHKiRet(createInstance(&pData));
	/*setInstParamDefaults(pData);*/
CODE_STD_FINALIZERnewActInst
/*	cnfparamvalsDestruct(pvals, &actpblk);*/
ENDnewActInst


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
ENDdbgPrintInstInfo


BEGINtryResume
CODESTARTtryResume
ENDtryResume

BEGINdoAction
	msg_t *pMsg;
	uchar *tag;
	int lenTag;
	int sev;
	int r;
CODESTARTdoAction
	pMsg = (msg_t*) ppString[0];
	MsgGetSeverity(pMsg, &sev);
	getTAG(pMsg, &tag, &lenTag);
	/* we can use more properties here, but let's see if there
	 * is some real user interest. We can always add later...
	 */
	r = sd_journal_send("MESSAGE=%s", getMSG(pMsg),
                "PRIORITY=%d", sev,
		"SYSLOG_FACILITY=%d", pMsg->iFacility,
		"SYSLOG_IDENTIFIER=%s", tag,
                NULL);
	/* FIXME: think about what to do with errors ;) */
ENDdoAction


BEGINparseSelectorAct
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	if(strncmp((char*) p, ":omjournal:", sizeof(":omjournal:") - 1)) {
		errmsg.LogError(0, RS_RET_LEGA_ACT_NOT_SUPPORTED,
			"omjournal supports only v6+ config format, use: "
			"action(type=\"omjournal\" ...)");
	}
	ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINmodExit
CODESTARTmodExit
	objRelease(errmsg, CORE_COMPONENT);
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES
CODEqueryEtryPt_STD_CONF2_QUERIES
ENDqueryEtryPt



BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	DBGPRINTF("omjournal: module compiled with rsyslog version %s.\n", VERSION);
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
ENDmodInit
