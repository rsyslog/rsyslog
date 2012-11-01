/* omhiredis.c
 * Copyright 2012 Talksum, Inc
*
* This program is free software: you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public License
* as published by the Free Software Foundation, either version 3 of
* the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this program. If not, see
* <http://www.gnu.org/licenses/>.
*
* Author: Brian Knox
* <briank@talksum.com>
*/



#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>
#include <time.h>
#include <hiredis/hiredis.h>

#include "rsyslog.h"
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include "cfsysline.h"

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("omhiredis")
/* internal structures
 */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(errmsg)

typedef struct _instanceData {
	redisContext *conn;
	uchar *server;
	int port;
	uchar *tplName;
} instanceData;


static struct cnfparamdescr actpdescr[] = {
	{ "server", eCmdHdlrGetWord, 0 },
	{ "serverport", eCmdHdlrInt, 0 },
	{ "template", eCmdHdlrGetWord, 1 }
};
static struct cnfparamblk actpblk = {
	CNFPARAMBLK_VERSION,
	sizeof(actpdescr)/sizeof(struct cnfparamdescr),
	actpdescr
};

BEGINcreateInstance
CODESTARTcreateInstance
ENDcreateInstance

BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURERepeatedMsgReduction)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature

static void closeHiredis(instanceData *pData)
{
	if(pData->conn != NULL) {
		redisFree(pData->conn);
		pData->conn = NULL;
	}
}


BEGINfreeInstance
CODESTARTfreeInstance
	closeHiredis(pData);
	free(pData->server);
	free(pData->tplName);
ENDfreeInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	/* nothing special here */
ENDdbgPrintInstInfo


static rsRetVal initHiredis(instanceData *pData, int bSilent)
{
	char *server;
	DEFiRet;

	server = (pData->server == NULL) ? "127.0.0.1" : (char*) pData->server;
	DBGPRINTF("omhiredis: trying connect to '%s' at port %d\n", server, pData->port);
    
	struct timeval timeout = { 1, 500000 }; /* 1.5 seconds */
	pData->conn = redisConnectWithTimeout(server, pData->port, timeout);
	if (pData->conn->err) {
		if(!bSilent)
			errmsg.LogError(0, RS_RET_SUSPENDED,
				"can not initialize redis handle");
				ABORT_FINALIZE(RS_RET_SUSPENDED);
	}

finalize_it:
	RETiRet;
}

rsRetVal writeHiredis(uchar *message, instanceData *pData)
{
	redisReply *reply;
	DEFiRet;

	if(pData->conn == NULL)
		CHKiRet(initHiredis(pData, 0));

	reply = redisCommand(pData->conn, (char*)message);
	if (reply->type == REDIS_REPLY_ERROR) {
		errmsg.LogError(0, NO_ERRCODE, "omhiredis: %s", reply->str);
		dbgprintf("omhiredis: %s\n", reply->str);
		freeReplyObject(reply);
		ABORT_FINALIZE(RS_RET_ERR);
	} else {
		freeReplyObject(reply); 
	}

finalize_it:
	RETiRet;
}

BEGINtryResume
CODESTARTtryResume
	if(pData->conn == NULL)
		iRet = initHiredis(pData, 0);
ENDtryResume

BEGINdoAction
CODESTARTdoAction
	iRet = writeHiredis(ppString[0], pData);
ENDdoAction


static inline void
setInstParamDefaults(instanceData *pData)
{
	pData->server = NULL;
	pData->port = 6379;
	pData->tplName = NULL;
}

BEGINnewActInst
	struct cnfparamvals *pvals;
	int i;
CODESTARTnewActInst
	if((pvals = nvlstGetParams(lst, &actpblk, NULL)) == NULL)
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);

	CHKiRet(createInstance(&pData));
	setInstParamDefaults(pData);

	CODE_STD_STRING_REQUESTnewActInst(1)
	for(i = 0 ; i < actpblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
	
		if(!strcmp(actpblk.descr[i].name, "server")) {
			pData->server = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "serverport")) {
			pData->port = (int) pvals[i].val.d.n, NULL;
		} else if(!strcmp(actpblk.descr[i].name, "template")) {
			pData->tplName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else {
			dbgprintf("omhiredis: program error, non-handled "
				"param '%s'\n", actpblk.descr[i].name);
		}
	}

	if(pData->tplName == NULL) {
		dbgprintf("omhiredis: action requires a template name");
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	/* template string 0 is just a regular string */
	OMSRsetEntry(*ppOMSR, 0,(uchar*)pData->tplName, OMSR_NO_RQD_TPL_OPTS);

CODE_STD_FINALIZERnewActInst
	cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst


BEGINparseSelectorAct
CODESTARTparseSelectorAct

/* tell the engine we only want one template string */
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	if(!strncmp((char*) p, ":omhiredis:", sizeof(":omhiredis:") - 1)) 
		errmsg.LogError(0, RS_RET_LEGA_ACT_NOT_SUPPORTED,
			"omhiredis supports only v6 config format, use: "
			"action(type=\"omhiredis\" server=...)");
	ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINmodExit
CODESTARTmodExit
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES
ENDqueryEtryPt

BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* only supports rsyslog 6 configs */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	INITChkCoreFeature(bCoreSupportsBatching, CORE_FEATURE_BATCHING);
	DBGPRINTF("omhiredis: module compiled with rsyslog version %s.\n", VERSION);
ENDmodInit
