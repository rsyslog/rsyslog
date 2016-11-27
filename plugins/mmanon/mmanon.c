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
#include <stdint.h>
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

/* precomputed table of IPv4 anonymization masks */
static const uint32_t ipv4masks[33] = {
	0xffffffff, 0xfffffffe, 0xfffffffc, 0xfffffff8,
	0xfffffff0, 0xffffffe0, 0xffffffc0, 0xffffff80,
	0xffffff00, 0xfffffe00, 0xfffffc00, 0xfffff800,
	0xfffff000, 0xffffe000, 0xffffc000, 0xffff8000,
	0xffff0000, 0xfffe0000, 0xfffc0000, 0xfff80000,
	0xfff00000, 0xffe00000, 0xffc00000, 0xff800000,
	0xff000000, 0xfe000000, 0xfc000000, 0xf8000000,
	0xf0000000, 0xe0000000, 0xc0000000, 0x80000000,
	0x00000000
	};

/* define operation modes we have */
#define SIMPLE_MODE 0	 /* just overwrite */
#define REWRITE_MODE 1	 /* rewrite IP address, canoninized */
typedef struct _instanceData {
	char replChar;
	int8_t mode;
	struct {
		int8_t bits;
	} ipv4;
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

BEGINcreateWrkrInstance
CODESTARTcreateWrkrInstance
ENDcreateWrkrInstance


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
ENDisCompatibleWithFeature


BEGINfreeInstance
CODESTARTfreeInstance
ENDfreeInstance


BEGINfreeWrkrInstance
CODESTARTfreeWrkrInstance
ENDfreeWrkrInstance


static inline void
setInstParamDefaults(instanceData *pData)
{
	pData->mode = REWRITE_MODE;
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
			} else if(!es_strbufcmp(pvals[i].val.d.estr, (uchar*)"rewrite",
					 sizeof("rewrite")-1)) {
				pData->mode = REWRITE_MODE;
			} else {
				char *cstr = es_str2cstr(pvals[i].val.d.estr, NULL);
				errmsg.LogError(0, RS_RET_INVLD_MODE,
					"mmanon: invalid anonymization mode '%s' - ignored",
					cstr);
				free(cstr);
			}
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
	} else { /* REWRITE_MODE */
		if(pData->ipv4.bits < 1 || pData->ipv4.bits > 32) {
			pData->ipv4.bits = 32;
			errmsg.LogError(0, RS_RET_INVLD_ANON_BITS,
				"mmanon: invalid number of ipv4 bits "
				"in rewrite mode, corrected to %d",
				pData->ipv4.bits);
		}
		if(pData->replChar != 'x') {
			errmsg.LogError(0, RS_RET_REPLCHAR_IGNORED,
				"mmanon: replacementChar parameter is ignored "
				"in rewrite mode");
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


static int
getnum(uchar *msg, int lenMsg, int *idx)
{
	int num = 0;
	int i = *idx;

	while(i < lenMsg && msg[i] >= '0' && msg[i] <= '9') {
		num = num * 10 + msg[i] - '0';
		++i;
	}

	*idx = i;
	return num;
}


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
static void
anonip(instanceData *pData, uchar *msg, int *pLenMsg, int *idx)
{
	int i = *idx;
	int octet;
	uint32_t ipv4addr;
	int ipstart[4];
	int j;
	int endpos;
	int lenMsg = *pLenMsg;

	while(i < lenMsg && (msg[i] <= '0' || msg[i] > '9')) {
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
	if(octet > 255) goto done;
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
			/* correct index for next search! */
			i -= (i - endpos);
		}
	}

done:	*idx = i;
	return;
}


BEGINdoAction_NoStrings
	smsg_t **ppMsg = (smsg_t **) pMsgData;
	smsg_t *pMsg = ppMsg[0];
	uchar *msg;
	int lenMsg;
	int i;
CODESTARTdoAction
	lenMsg = getMSGLen(pMsg);
	msg = getMSG(pMsg);
	for(i = 0 ; i < lenMsg ; ++i) {
		anonip(pWrkrData->pData, msg, &lenMsg, &i);
	}
	if(lenMsg != getMSGLen(pMsg))
		setMSGLen(pMsg, lenMsg);
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
CODEqueryEtryPt_STD_OMOD8_QUERIES
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
