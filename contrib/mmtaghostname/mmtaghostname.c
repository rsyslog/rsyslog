/* mmtaghostname.c
 * This is a message modification module.
 *
 * The name of the module is inspired by the parser module pmnull
 * Its objectives are closed to this parser but as a message modification
 * it can be used in a different step of the message processing without
 * interfering in the parser chain process and can be applied before or
 * after parsing process.
 *
 * Its purposes are :
 * - to add a tag on message produce by input module which does not provide
 *   a tag like imudp or imtcp. Useful when the tag is used for routing the
 *   message.
 * - to force message hostname to the rsyslog valeur. The use case is
 *   application in auto-scaling systems (AWS) providing logs through udp/tcp
 *   were the name of the host is based on an ephemeral IPs with a short term
 *   meaning. In this situation rsyslog local host name is generally the
 *   auto-scaling name then logs produced by the application is affected to
 *   the application instead of the ephemeral VM.
 *
 *
 * This file is a contribution of rsyslog.
 *
 * Author : Ph. Duveau <philippe.duveau@free.fr>
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
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>

#include "rsyslog.h"
#include "conf.h"
#include "syslogd-types.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include "cfsysline.h"
#include "dirty.h"
#include "unicode-helper.h"

MODULE_TYPE_OUTPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME("mmtaghostname")

/* internal structures */
DEF_OMOD_STATIC_DATA;
DEFobjCurrIf(glbl)

/* parser instance parameters */
static struct cnfparamdescr parserpdescr[] = {
	{ "tag", eCmdHdlrString, 0 },
	{ "forcelocalhostname", eCmdHdlrBinary, 0 },
};
static struct cnfparamblk parserpblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(parserpdescr)/sizeof(struct cnfparamdescr),
	  parserpdescr
	};

typedef struct _instanceData {
	char *pszTag;
	size_t lenTag;
	int bForceLocalHostname;
} instanceData;

typedef struct wrkrInstanceData {
	instanceData *pData;
} wrkrInstanceData_t;

static const uchar *pszHostname = NULL;
static size_t lenHostname = 0;

BEGINcreateWrkrInstance
CODESTARTcreateWrkrInstance;
ENDcreateWrkrInstance

BEGINfreeWrkrInstance
CODESTARTfreeWrkrInstance;
ENDfreeWrkrInstance

BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo;
	dbgprintf("mmtaghostname:\n");
	dbgprintf("\ttag='%s'\n", pData->pszTag);
	dbgprintf("\tforce local hostname='%d'\n", pData->bForceLocalHostname);
ENDdbgPrintInstInfo

BEGINcreateInstance
CODESTARTcreateInstance;
	pData->pszTag = NULL;
	pData->lenTag = 0;
	pData->bForceLocalHostname = 0;
ENDcreateInstance

BEGINfreeInstance
CODESTARTfreeInstance;
	free(pData->pszTag);
ENDfreeInstance

BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature;
ENDisCompatibleWithFeature

BEGINnewActInst
	struct cnfparamvals *pvals = NULL;
	int i;
CODESTARTnewActInst;
	DBGPRINTF("newParserInst (mmtaghostname)\n");

	CHKiRet(createInstance(&pData));

	if(lst == NULL)
		FINALIZE;  /* just set defaults, no param block! */

	if((pvals = nvlstGetParams(lst, &parserpblk, NULL)) == NULL) {
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	if(Debug) {
		dbgprintf("parser param blk in mmtaghostname:\n");
		cnfparamsPrint(&parserpblk, pvals);
	}

	for(i = 0 ; i < parserpblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(parserpblk.descr[i].name, "tag")) {
			pData->pszTag = (char *) es_str2cstr(pvals[i].val.d.estr, NULL);
			pData->lenTag = strlen(pData->pszTag);
		} else if(!strcmp(parserpblk.descr[i].name, "forcelocalhostname")) {
			pData->bForceLocalHostname = pvals[i].val.d.n;
		} else {
			dbgprintf("program error, non-handled param '%s'\n",
				parserpblk.descr[i].name);
		}
	}
	CODE_STD_STRING_REQUESTnewActInst(1);
	CHKiRet(OMSRsetEntry(*ppOMSR, 0, NULL, OMSR_TPL_AS_MSG));
CODE_STD_FINALIZERnewActInst;
	if(lst != NULL)
		cnfparamvalsDestruct(pvals, &parserpblk);
ENDnewActInst

BEGINdoAction_NoStrings
	smsg_t **ppMsg = (smsg_t **) pMsgData;
	smsg_t *pMsg = ppMsg[0];
	instanceData *pData = pWrkrData->pData;
CODESTARTdoAction;
	DBGPRINTF("Message will now be managed by mmtaghostname\n");
	if(pData->pszTag != NULL) {
		MsgSetTAG(pMsg, (uchar *)pData->pszTag, pData->lenTag);
	}
	if (pData->bForceLocalHostname) {
		if (pszHostname == NULL) {
			pszHostname = glbl.GetLocalHostName();
			lenHostname = ustrlen(glbl.GetLocalHostName());
		}
		MsgSetHOSTNAME(pMsg, pszHostname, lenHostname);
		DBGPRINTF("Message hostname forced to local\n");
	}
ENDdoAction

BEGINtryResume
CODESTARTtryResume;
ENDtryResume

BEGINparseSelectorAct
CODESTARTparseSelectorAct;
CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct

BEGINmodExit
CODESTARTmodExit;
	objRelease(glbl, CORE_COMPONENT);
ENDmodExit

BEGINqueryEtryPt
CODESTARTqueryEtryPt;
CODEqueryEtryPt_STD_OMOD_QUERIES;
CODEqueryEtryPt_STD_OMOD8_QUERIES;
CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES;
ENDqueryEtryPt

BEGINmodInit()
CODESTARTmodInit;
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(glbl, CORE_COMPONENT));
ENDmodInit
