/* mmfields.c
 * Parse all fields of the message into structured data inside the
 * JSON tree.
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
#include <stdint.h>
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("mmfields")


DEFobjCurrIf(errmsg);
DEF_OMOD_STATIC_DATA

/* config variables */

/* define operation modes we have */
#define SIMPLE_MODE 0	 /* just overwrite */
#define REWRITE_MODE 1	 /* rewrite IP address, canoninized */
typedef struct _instanceData {
	char separator;
} instanceData;

struct modConfData_s {
	rsconf_t *pConf;	/* our overall config object */
};
static modConfData_t *loadModConf = NULL;/* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL;/* modConf ptr to use for the current exec process */


/* tables for interfacing with the v6 config system */
/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {
	{ "separator", eCmdHdlrGetChar, 0 }
};
static struct cnfparamblk actpblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(actpdescr)/sizeof(struct cnfparamdescr),
	  actpdescr
	};

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


static inline void
setInstParamDefaults(instanceData *pData)
{
	pData->separator = ',';
}

BEGINnewActInst
	struct cnfparamvals *pvals;
	int i;
CODESTARTnewActInst
	DBGPRINTF("newActInst (mmfields)\n");
	if((pvals = nvlstGetParams(lst, &actpblk, NULL)) == NULL) {
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	CODE_STD_STRING_REQUESTnewActInst(1)
	CHKiRet(OMSRsetEntry(*ppOMSR, 0, NULL, OMSR_TPL_AS_MSG));
	CHKiRet(createInstance(&pData));
	setInstParamDefaults(pData);

	for(i = 0 ; i < actpblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(actpblk.descr[i].name, "separator")) {
			pData->separator = es_getBufAddr(pvals[i].val.d.estr)[0];
dbgprintf("DDDD: separator set to %d [%c]\n", pData->separator, pData->separator);
		} else {
			dbgprintf("mmfields: program error, non-handled "
			  "param '%s'\n", actpblk.descr[i].name);
		}
	}

dbgprintf("DDDD: separator IS %d [%c]\n", pData->separator, pData->separator);
CODE_STD_FINALIZERnewActInst
	cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
ENDdbgPrintInstInfo


BEGINtryResume
CODESTARTtryResume
ENDtryResume


static inline rsRetVal
extractField(instanceData *pData, uchar *msgtext, int lenMsg, int *curridx, uchar *fieldbuf)
{
	int i, j;
	DEFiRet;
	i = *curridx;
	j = 0;
	while(i < lenMsg && msgtext[i] != pData->separator) {
		fieldbuf[j++] = msgtext[i++];
	}
	fieldbuf[j] = '\0';
	if(i < lenMsg)
		++i;
	*curridx = i;

	RETiRet;
}

static inline rsRetVal
parse_fields(instanceData *pData, msg_t *pMsg, uchar *msgtext, int lenMsg)
{
	uchar fieldbuf[32*1024];
	int field;
	uchar *buf;
	int currIdx = 0;
	DEFiRet;

	if(lenMsg < (int) sizeof(fieldbuf)) {
		buf = fieldbuf;
	} else {
		CHKmalloc(buf = malloc(lenMsg+1));
	}

	field = 1;
	while(currIdx < lenMsg) {
		CHKiRet(extractField(pData, msgtext, lenMsg, &currIdx, buf));
		DBGPRINTF("mmfields: field %d: '%s'\n", field, buf);
		field++;
	}
finalize_it:
	RETiRet;
}


BEGINdoAction
	msg_t *pMsg;
	uchar *msg;
	int lenMsg;
CODESTARTdoAction
	pMsg = (msg_t*) ppString[0];
	lenMsg = getMSGLen(pMsg);
	msg = getMSG(pMsg);
	CHKiRet(parse_fields(pData, pMsg, msg, lenMsg));
finalize_it:
ENDdoAction


BEGINparseSelectorAct
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	if(strncmp((char*) p, ":mmfields:", sizeof(":mmfields:") - 1)) {
		errmsg.LogError(0, RS_RET_LEGA_ACT_NOT_SUPPORTED,
			"mmfields supports only v6+ config format, use: "
			"action(type=\"mmfields\" ...)");
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
	DBGPRINTF("mmfields: module compiled with rsyslog version %s.\n", VERSION);
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
ENDmodInit
