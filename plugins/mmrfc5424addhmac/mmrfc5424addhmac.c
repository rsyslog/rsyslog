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
#include <openssl/hmac.h>
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
	uchar *key;
	int keylen;	/* cached length of key, to avoid recompution */
	const EVP_MD *algo;
} instanceData;

struct modConfData_s {
	rsconf_t *pConf;	/* our overall config object */
};
static modConfData_t *loadModConf = NULL;/* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL;/* modConf ptr to use for the current exec process */


/* tables for interfacing with the v6 config system */
/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {
	{ "key", eCmdHdlrString, 1 },
	{ "hashfunction", eCmdHdlrString, 1 }
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
	pData->key = NULL;
}

BEGINnewActInst
	struct cnfparamvals *pvals;
	char *ciphername;
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
			pData->key = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
			pData->keylen = strlen((char*)pData->key);
		} else if(!strcmp(actpblk.descr[i].name, "hashfunction")) {
			ciphername = es_str2cstr(pvals[i].val.d.estr, NULL);
			pData->algo = EVP_get_digestbyname(ciphername);
			if(pData->algo == NULL) {
				errmsg.LogError(0, RS_RET_CRY_INVLD_ALGO,
					"hashFunction '%s' unknown to openssl - "
					"cannot continue", ciphername);
				free(ciphername);
				ABORT_FINALIZE(RS_RET_CRY_INVLD_ALGO);
			}
			free(ciphername);
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


/* turn the binary data in bin of length len into a
 * printable hex string. "print" must be 2*len+1 (for \0)
 */
static inline void
hexify(uchar *bin, int len, uchar *print)
{
	static const char hexchars[16] =
	   {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
	int iSrc, iDst;

	for(iSrc = iDst = 0 ; iSrc < len ; ++iSrc) {
		print[iDst++] = hexchars[bin[iSrc]>>4];
		print[iDst++] = hexchars[bin[iSrc]&0x0f];
	}
	print[iDst] = '\0';
}

static inline rsRetVal
hashMsg(instanceData *pData, msg_t *pMsg)
{
	uchar *pRawMsg;
	int lenRawMsg;
	unsigned int hashlen;
	uchar hash[EVP_MAX_MD_SIZE];
	uchar hashPrintable[2*EVP_MAX_MD_SIZE+1];
	DEFiRet;

// Next two debug only!
MsgGetStructuredData(pMsg, &pRawMsg, &lenRawMsg);
dbgprintf("DDDD: STRUCTURED-DATA is: '%s'\n", pRawMsg);
	getRawMsg(pMsg, &pRawMsg, &lenRawMsg);
 	HMAC(pData->algo, pData->key, pData->keylen,
	     pRawMsg, lenRawMsg, hash, &hashlen);
	hexify(hash, hashlen, hashPrintable);
dbgprintf("DDDD: rawmsg is: '%s', hash: '%s'\n", pRawMsg, hashPrintable);
	RETiRet;
}


BEGINdoAction
	msg_t *pMsg;
CODESTARTdoAction
	pMsg = (msg_t*) ppString[0];
	if(msgGetProtocolVersion(pMsg) == MSG_RFC5424_PROTOCOL) {
		hashMsg(pData, pMsg);
	} else {
		if(Debug) {
			uchar *pRawMsg;
			int lenRawMsg;
			getRawMsg(pMsg, &pRawMsg, &lenRawMsg);
			dbgprintf("mmrfc5424addhmac: non-rfc5424: %.256s\n", pRawMsg);
		}
	}
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
	EVP_cleanup();
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES
CODEqueryEtryPt_STD_CONF2_QUERIES
ENDqueryEtryPt



BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION;
CODEmodInit_QueryRegCFSLineHdlr
	DBGPRINTF("mmrfc5424addhmac: module compiled with rsyslog version %s.\n", VERSION);
	OpenSSL_add_all_digests();
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
ENDmodInit
