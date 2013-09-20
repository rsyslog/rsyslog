/* mmrfc5424addhmac.c
 * custom module: add hmac to RFC5424 messages
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
MODULE_CNFNAME("mmrfc5424addhmac")


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
	//pData->replChar = 'x';
}

BEGINnewActInst
	struct cnfparamvals *pvals;
	int i;
CODESTARTnewActInst
	DBGPRINTF("newActInst (mmrfc5424addhmac)\n");
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
		if(!strcmp(actpblk.descr[i].name, "replacementchar")) {
	//		pData->replChar = es_getBufAddr(pvals[i].val.d.estr)[0];
		} else if(!strcmp(actpblk.descr[i].name, "ipv4.bits")) {
	//		pData->ipv4.bits = (int8_t) pvals[i].val.d.n;
		} else {
			dbgprintf("mmrfc5424addhmac: program error, non-handled "
			  "param '%s'\n", actpblk.descr[i].name);
		}
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


#if 0
/* write an IP address octet to the output position */
static int
writeOctet(uchar *msg, int idx, int *nxtidx, uint8_t octet)
{
	if(octet > 99) {
		msg[idx++] = '0' + octet / 100;
		octet = octet % 100;
	}
	if(octet > 9) {
		msg[idx++] = '0' + octet / 10;
		octet = octet % 10;
	}
	msg[idx++] =  '0' + octet;

	if(nxtidx != NULL) {
		if(idx + 1 != *nxtidx) {
			/* we got shorter, fix it! */
			msg[idx] = '.';
			*nxtidx = idx + 1;
		}
	}
	return idx;
}

/* currently works for IPv4 only! */
void
anonip(instanceData *pData, uchar *msg, int *pLenMsg, int *idx)
{
	int i = *idx;
	int octet;
	uint32_t ipv4addr;
	int ipstart[4];
	int j;
	int endpos;
	int lenMsg = *pLenMsg;

	while(i < lenMsg && (msg[i] <= '0' || msg[i] >= '9')) {
		++i; /* skip to first number */
	}
	if(i >= lenMsg)
		goto done;
	
	/* got digit, let's see if ip */
	ipstart[0] = i;
	octet = getnum(msg, lenMsg, &i);
	if(octet > 255 || msg[i] != '.') goto done;
	ipv4addr = octet << 24;
	++i;
	ipstart[1] = i;
	octet = getnum(msg, lenMsg, &i);
	if(octet > 255 || msg[i] != '.') goto done;
	ipv4addr |= octet << 16;
	++i;
	ipstart[2] = i;
	octet = getnum(msg, lenMsg, &i);
	if(octet > 255 || msg[i] != '.') goto done;
	ipv4addr |= octet << 8;
	++i;
	ipstart[3] = i;
	octet = getnum(msg, lenMsg, &i);
	if(octet > 255 || !(msg[i] == ' ' || msg[i] == ':')) goto done;
	ipv4addr |= octet;

	/* OK, we now found an ip address */
	if(pData->mode == SIMPLE_MODE) {
		if(pData->ipv4.bits == 8)
			j = ipstart[3];
		else if(pData->ipv4.bits == 16)
			j = ipstart[2];
		else if(pData->ipv4.bits == 24)
			j = ipstart[1];
		else /* due to our checks, this *must* be 32 */
			j = ipstart[0];
		while(j < i) {
			if(msg[j] != '.')
				msg[j] = pData->replChar;
			++j;
		}
	} else { /* REWRITE_MODE */
		ipv4addr &= ipv4masks[pData->ipv4.bits];
		if(pData->ipv4.bits > 24)
			writeOctet(msg, ipstart[0], &(ipstart[1]), ipv4addr >> 24);
		if(pData->ipv4.bits > 16)
			writeOctet(msg, ipstart[1], &(ipstart[2]), (ipv4addr >> 16) & 0xff);
		if(pData->ipv4.bits > 8)
			writeOctet(msg, ipstart[2], &(ipstart[3]), (ipv4addr >> 8) & 0xff);
		endpos = writeOctet(msg, ipstart[3], NULL, ipv4addr & 0xff);
		/* if we had truncation, we need to shrink the msg */
		dbgprintf("existing i %d, endpos %d\n", i, endpos);
		if(i - endpos > 0) {
			*pLenMsg = lenMsg - (i - endpos);
			memmove(msg+endpos, msg+i, lenMsg - i + 1);
		}
	}

done:	*idx = i;
	return;
}
#endif


BEGINdoAction
	msg_t *pMsg;
	uchar *msg;
	int lenMsg;
	int i;
CODESTARTdoAction
	pMsg = (msg_t*) ppString[0];
	lenMsg = getMSGLen(pMsg);
	msg = getMSG(pMsg);
	for(i = 0 ; i < lenMsg ; ++i) {
		anonip(pData, msg, &lenMsg, &i);
	}
	if(lenMsg != getMSGLen(pMsg))
		setMSGLen(pMsg, lenMsg);
ENDdoAction


BEGINparseSelectorAct
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	if(strncmp((char*) p, ":mmrfc5424addhmac:", sizeof(":mmrfc5424addhmac:") - 1)) {
		errmsg.LogError(0, RS_RET_LEGA_ACT_NOT_SUPPORTED,
			"mmrfc5424addhmac supports only v6+ config format, use: "
			"action(type=\"mmrfc5424addhmac\" ...)");
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
	DBGPRINTF("mmrfc5424addhmac: module compiled with rsyslog version %s.\n", VERSION);
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
ENDmodInit
