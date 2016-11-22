/* mmpstrucdata.c
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
#include <ctype.h>
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("mmpstrucdata")


DEFobjCurrIf(errmsg);
DEF_OMOD_STATIC_DATA

/* config variables */

typedef struct _instanceData {
	uchar *jsonRoot;	/**< container where to store fields */
} instanceData;

typedef struct wrkrInstanceData {
	instanceData *pData;
} wrkrInstanceData_t;

struct modConfData_s {
	rsconf_t *pConf;	/* our overall config object */
};
static modConfData_t *loadModConf = NULL;/* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL;/* modConf ptr to use for the current exec process */


/* tables for interfacing with the v6 config system */
/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {
	{ "jsonroot", eCmdHdlrString, 0 }
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

BEGINcreateWrkrInstance
CODESTARTcreateWrkrInstance
ENDcreateWrkrInstance


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
ENDisCompatibleWithFeature


BEGINfreeInstance
CODESTARTfreeInstance
	free(pData->jsonRoot);
ENDfreeInstance

BEGINfreeWrkrInstance
CODESTARTfreeWrkrInstance
ENDfreeWrkrInstance


static inline void
setInstParamDefaults(instanceData *pData)
{
	pData->jsonRoot = NULL;
}

BEGINnewActInst
	struct cnfparamvals *pvals;
	int i;
CODESTARTnewActInst
	DBGPRINTF("newActInst (mmpstrucdata)\n");
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
		if(!strcmp(actpblk.descr[i].name, "jsonroot")) {
			pData->jsonRoot = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else {
			dbgprintf("mmpstrucdata: program error, non-handled "
			  "param '%s'\n", actpblk.descr[i].name);
		}
	}
	if(pData->jsonRoot == NULL) {
		CHKmalloc(pData->jsonRoot = (uchar*) strdup("!"));
	}

CODE_STD_FINALIZERnewActInst
	cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
ENDdbgPrintInstInfo


BEGINtryResume
CODESTARTtryResume
ENDtryResume


static rsRetVal
parsePARAM_VALUE(uchar *sdbuf, int lenbuf, int *curridx, uchar *fieldbuf)
{
	int i, j;
	DEFiRet;
	i = *curridx;
	j = 0;
	while(i < lenbuf && sdbuf[i] != '"') {
		if(sdbuf[i] == '\\') {
			if(++i == lenbuf) {
				fieldbuf[j++] = '\\';
			} else {
				if(sdbuf[i] == '"') {
					fieldbuf[j++] = '"';
				} else if(sdbuf[i] == '\\') {
					fieldbuf[j++] = '\\';
				} else if(sdbuf[i] == ']') {
					fieldbuf[j++] = '"';
				} else {
					fieldbuf[j++] = '\\';
					fieldbuf[j++] = sdbuf[i];
				}
				++i;
			}
		} else {
			fieldbuf[j++] = sdbuf[i++];
		}
	}
	fieldbuf[j] = '\0';
	*curridx = i;
	RETiRet;
}


static rsRetVal
parseSD_NAME(uchar *sdbuf, int lenbuf, int *curridx, uchar *namebuf)
{
	int i, j;
	DEFiRet;
	i = *curridx;
	for(j = 0 ; i < lenbuf && j < 32; ++j) {
		if(   sdbuf[i] == '=' || sdbuf[i] == '"'
		   || sdbuf[i] == ']' || sdbuf[i] == ' ')
			break;
		namebuf[j] = tolower(sdbuf[i]);
		++i;
	}
	namebuf[j] = '\0';
	*curridx = i;
	RETiRet;
}


static rsRetVal
parseSD_PARAM(uchar *sdbuf, int lenbuf, int *curridx, struct json_object *jroot)
{
	int i;
	uchar pName[33];
	uchar pVal[32*1024];
	struct json_object *jval;
	DEFiRet;
	
	i = *curridx;
	CHKiRet(parseSD_NAME(sdbuf, lenbuf, &i, pName));
	if(sdbuf[i] != '=') {
		ABORT_FINALIZE(RS_RET_STRUC_DATA_INVLD);
	}
	++i;
	if(sdbuf[i] != '"') {
		ABORT_FINALIZE(RS_RET_STRUC_DATA_INVLD);
	}
	++i;
	CHKiRet(parsePARAM_VALUE(sdbuf, lenbuf, &i, pVal));
	if(sdbuf[i] != '"') {
		ABORT_FINALIZE(RS_RET_STRUC_DATA_INVLD);
	}
	++i;

	jval = json_object_new_string((char*)pVal);
	json_object_object_add(jroot, (char*)pName, jval);

	*curridx = i;
finalize_it:
	RETiRet;
}


static rsRetVal
parseSD_ELEMENT(uchar *sdbuf, int lenbuf, int *curridx, struct json_object *jroot)
{
	int i;
	uchar sd_id[33];
	struct json_object *json = NULL;
	DEFiRet;
	
	i = *curridx;
	if(sdbuf[i] != '[') {
		ABORT_FINALIZE(RS_RET_STRUC_DATA_INVLD);
	}
	++i; /* eat '[' */

	CHKiRet(parseSD_NAME(sdbuf, lenbuf, &i, sd_id));
	json =  json_object_new_object();

	while(i < lenbuf) {
		if(sdbuf[i] == ']') {
			break;
		} else if(sdbuf[i] != ' ') {
			ABORT_FINALIZE(RS_RET_STRUC_DATA_INVLD);
		}
		++i;
		while(i < lenbuf && sdbuf[i] == ' ')
			++i;
		CHKiRet(parseSD_PARAM(sdbuf, lenbuf, &i, json));
	}

	if(sdbuf[i] != ']') {
		DBGPRINTF("mmpstrucdata: SD-ELEMENT does not terminate with "
		          "']': '%s'\n", sdbuf+i);
		ABORT_FINALIZE(RS_RET_STRUC_DATA_INVLD);
	}
	++i; /* eat ']' */
	*curridx = i;
	json_object_object_add(jroot, (char*)sd_id, json);
finalize_it:
	if(iRet != RS_RET_OK && json != NULL)
		json_object_put(json);
	RETiRet;
}

static rsRetVal
parse_sd(instanceData *pData, smsg_t *pMsg)
{
	struct json_object *json, *jroot;
	uchar *sdbuf;
	int lenbuf;
	int i = 0;
	DEFiRet;

	json =  json_object_new_object();
	if(json == NULL) {
		ABORT_FINALIZE(RS_RET_ERR);
	}
	MsgGetStructuredData(pMsg, &sdbuf,&lenbuf);
	while(i < lenbuf) {
		CHKiRet(parseSD_ELEMENT(sdbuf, lenbuf, &i, json));
	}

	jroot =  json_object_new_object();
	if(jroot == NULL) {
		ABORT_FINALIZE(RS_RET_ERR);
	}
	json_object_object_add(jroot, "rfc5424-sd", json);
 	msgAddJSON(pMsg, pData->jsonRoot, jroot, 0, 0);
finalize_it:
	if(iRet != RS_RET_OK && json != NULL)
		json_object_put(json);
	RETiRet;
}


BEGINdoAction_NoStrings
	smsg_t **ppMsg = (smsg_t **) pMsgData;
	smsg_t *pMsg = ppMsg[0];
CODESTARTdoAction
	DBGPRINTF("mmpstrucdata: enter\n");
	if(!MsgHasStructuredData(pMsg)) {
		DBGPRINTF("mmpstrucdata: message does not have structured data\n");
		FINALIZE;
	}
	/* don't check return code - we never want rsyslog to retry
	 * or suspend this action!
	 */
	parse_sd(pWrkrData->pData, pMsg);
finalize_it:
ENDdoAction


BEGINparseSelectorAct
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	if(strncmp((char*) p, ":mmpstrucdata:", sizeof(":mmpstrucdata:") - 1)) {
		errmsg.LogError(0, RS_RET_LEGA_ACT_NOT_SUPPORTED,
			"mmpstrucdata supports only v6+ config format, use: "
			"action(type=\"mmpstrucdata\" ...)");
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
CODEqueryEtryPt_STD_OMOD8_QUERIES
CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES
CODEqueryEtryPt_STD_CONF2_QUERIES
ENDqueryEtryPt



BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	DBGPRINTF("mmpstrucdata: module compiled with rsyslog version %s.\n", VERSION);
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
ENDmodInit
