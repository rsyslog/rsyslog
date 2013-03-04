/* mmanon.c
 * anonnymize IP addresses inside the syslog message part
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

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("mmanon")


DEFobjCurrIf(errmsg);
DEF_OMOD_STATIC_DATA

/* config variables */


typedef struct _instanceData {
	char replChar;
	int8_t mode;
#	define SIMPLE_MODE 0 /* just overwrite */
	struct {
		int8_t bits;
	} ipv4;
} instanceData;

struct modConfData_s {
	rsconf_t *pConf;	/* our overall config object */
};
static modConfData_t *loadModConf = NULL;/* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL;/* modConf ptr to use for the current exec process */


/* tables for interfacing with the v6 config system */
/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {
	{ "mode", eCmdHdlrGetWord, 0 },
	{ "replacementchar", eCmdHdlrGetChar, 0 },
	{ "ipv4.bits", eCmdHdlrInt, 0 },
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
	pData->mode = SIMPLE_MODE;
	pData->replChar = 'x';
	pData->ipv4.bits = 16;
}

BEGINnewActInst
	struct cnfparamvals *pvals;
	int i;
	sbool bHadBitsErr;
CODESTARTnewActInst
	DBGPRINTF("newActInst (mmanon)\n");
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
		if(!strcmp(actpblk.descr[i].name, "mode")) {
			if(!es_strbufcmp(pvals[i].val.d.estr, (uchar*)"simple",
					 sizeof("simple")-1)) {
				pData->mode = SIMPLE_MODE;
			} else {
				char *cstr = es_str2cstr(pvals[i].val.d.estr, NULL);
				errmsg.LogError(0, RS_RET_INVLD_MODE,
					"mmanon: invalid anonymization mode '%s' - ignored",
					cstr);
				free(cstr);
			}
			pData->replChar = es_getBufAddr(pvals[i].val.d.estr)[0];
		} else if(!strcmp(actpblk.descr[i].name, "replacementchar")) {
			pData->replChar = es_getBufAddr(pvals[i].val.d.estr)[0];
		} else if(!strcmp(actpblk.descr[i].name, "ipv4.bits")) {
			pData->ipv4.bits = (int8_t) pvals[i].val.d.n;
		} else {
			dbgprintf("mmanon: program error, non-handled "
			  "param '%s'\n", actpblk.descr[i].name);
		}
	}

	if(pData->mode == SIMPLE_MODE) {
		bHadBitsErr = 0;
		if(pData->ipv4.bits < 8) {
			pData->ipv4.bits = 8;
			bHadBitsErr = 1;
		} else if(pData->ipv4.bits < 16) {
			pData->ipv4.bits = 16;
			bHadBitsErr = 1;
		} else if(pData->ipv4.bits < 24) {
			pData->ipv4.bits = 24;
			bHadBitsErr = 1;
		} else if(pData->ipv4.bits != 32) {
			pData->ipv4.bits = 32;
			bHadBitsErr = 1;
		}
		if(bHadBitsErr)
			errmsg.LogError(0, RS_RET_INVLD_ANON_BITS,
				"mmanon: invalid number of ipv4 bits "
				"in simple mode, corrected to %d",
				pData->ipv4.bits);
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


static int
getnum(uchar *msg, int lenMsg, int *idx)
{
	int num = 0;
	int i = *idx;

dbgprintf("DDDD: in getnum: %s\n", msg+(*idx));
	while(i < lenMsg && msg[i] >= '0' && msg[i] <= '9') {
		num = num * 10 + msg[i] - '0';
		++i;
	}

	*idx = i;
dbgprintf("DDDD: got octet %d\n", num);
	return num;
}


/* currently works for IPv4 only! */
void
anonip(instanceData *pData, uchar *msg, int lenMsg, int *idx)
{
	int i = *idx;
	int octet[4];
	int ipstart[4];
	int j;

dbgprintf("DDDD: in anonip: %s\n", msg+(*idx));
	while(i < lenMsg && (msg[i] <= '0' || msg[i] >= '9')) {
		++i; /* skip to first number */
	}
	if(i >= lenMsg)
		goto done;
	
	/* got digit, let's see if ip */
	ipstart[0] = i;
	octet[0] = getnum(msg, lenMsg, &i);
	if(octet[0] > 255 || msg[i] != '.') goto done;
	++i;
	ipstart[1] = i;
	octet[1] = getnum(msg, lenMsg, &i);
	if(octet[1] > 255 || msg[i] != '.') goto done;
	++i;
	ipstart[2] = i;
	octet[2] = getnum(msg, lenMsg, &i);
	if(octet[2] > 255 || msg[i] != '.') goto done;
	++i;
	ipstart[3] = i;
	octet[3] = getnum(msg, lenMsg, &i);
	if(octet[3] > 255 || !(msg[i] == ' ' || msg[i] == ':')) goto done;

	/* OK, we now found an ip address */
	if(pData->ipv4.bits == 8)
		j = ipstart[3];
	else if(pData->ipv4.bits == 16)
		j = ipstart[2];
	else if(pData->ipv4.bits == 24)
		j = ipstart[1];
	else /* due to our checks, this *must* be 32 */
		j = ipstart[0];
dbgprintf("DDDD: ipstart is %d: %s\n", j, msg+j);
	while(j < i) {
		if(msg[j] != '.')
			msg[j] = pData->replChar;
		++j;
	}

done:	*idx = i;
	return;
}


BEGINdoAction
	msg_t *pMsg;
	uchar *msg;
	int lenMsg;
	int i;
CODESTARTdoAction
	pMsg = (msg_t*) ppString[0];
	lenMsg = getMSGLen(pMsg);
	msg = getMSG(pMsg);
	DBGPRINTF("DDDD: calling mmanon with msg '%s'\n", msg);
	for(i = 0 ; i < lenMsg ; ++i) {
		anonip(pData, msg, lenMsg, &i);
	}
ENDdoAction


BEGINparseSelectorAct
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	if(strncmp((char*) p, ":mmanon:", sizeof(":mmanon:") - 1)) {
		errmsg.LogError(0, RS_RET_LEGA_ACT_NOT_SUPPORTED,
			"mmanon supports only v6+ config format, use: "
			"action(type=\"mmanon\" ...)");
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
	DBGPRINTF("mmanon: module compiled with rsyslog version %s.\n", VERSION);
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
ENDmodInit
