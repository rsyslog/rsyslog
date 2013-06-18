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

/*  our instance data.
 *  this will be accessable 
 *  via pData */
typedef struct _instanceData {
	redisContext *conn; /*  redis connection */
	uchar *server; /*  redis server address */
	int port; /*  redis port */
	uchar *tplName; /*  template name */
    redisReply **replies; /* array to hold replies from redis */
    int count; /*  count of command sent for current batch */
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

/*  called when closing */
static void closeHiredis(instanceData *pData)
{
	if(pData->conn != NULL) {
		redisFree(pData->conn);
		pData->conn = NULL;
	}
}

/*  Free our instance data.
 *  TODO: free **replies */
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

/*  establish our connection to redis */
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
	DEFiRet;

    /*  if we do not have a redis connection, call
     *  initHiredis and try to establish one */
	if(pData->conn == NULL)
		CHKiRet(initHiredis(pData, 0));

    /*  try to append the command to the pipeline. 
     *  REDIS_ERR reply indicates something bad
     *  happened, in which case abort. otherwise
     *  increase our current pipeline count
     *  by 1 and continue. */
	int rc;
	rc = redisAppendCommand(pData->conn, (char*)message);
	if (rc == REDIS_ERR) {
		errmsg.LogError(0, NO_ERRCODE, "omhiredis: %s", pData->conn->errstr);
		dbgprintf("omhiredis: %s\n", pData->conn->errstr);
		ABORT_FINALIZE(RS_RET_ERR);
	} else {
        pData->count++;
    }

finalize_it:
	RETiRet;
}

/*  called when resuming from suspended state.
 *  try to restablish our connection to redis */
BEGINtryResume
CODESTARTtryResume
	if(pData->conn == NULL)
		iRet = initHiredis(pData, 0);
ENDtryResume

/*  begin a transaction. for now does nothing.
 *  if I decide to use MULTI ... EXEC in the
 *  fture, this block should send the
 *  MULTI command to redis. */
BEGINbeginTransaction
CODESTARTbeginTransaction
    dbgprintf("omhiredis: beginTransaction called\n");
ENDbeginTransaction

/*  call writeHiredis for this log line,
 *  which appends it as a command to the
 *  current pipeline */
BEGINdoAction
CODESTARTdoAction
	CHKiRet(writeHiredis(ppString[0], pData));
    iRet = RS_RET_DEFER_COMMIT;
finalize_it:
ENDdoAction

/*  called when we have reached the end of a
 *  batch (queue.dequeuebatchsize).  this
 *  iterates over the replies, putting them
 *  into the pData->replies buffer. we currently
 *  don't really bother to check for errors
 *  which should be fixed */
BEGINendTransaction
CODESTARTendTransaction
    dbgprintf("omhiredis: endTransaction called\n");
    int i;
    pData->replies = malloc ( sizeof ( redisReply* ) * pData->count );
    for ( i = 0; i < pData->count; i++ ) {
        redisGetReply ( pData->conn, (void *)&pData->replies[i] );
        /*  TODO: add error checking here! */
        free ( pData->replies[i] );
    }
    free ( pData->replies );
    pData->count = 0;
ENDendTransaction

/*  set defaults. note server is set to NULL 
 *  and is set to a default in initHiredis if 
 *  it is still null when it's called - I should
 *  probable just set the default here instead */
static inline void
setInstParamDefaults(instanceData *pData)
{
	pData->server = NULL;
	pData->port = 6379;
	pData->tplName = NULL;
    pData->count = 0;
}

/*  here is where the work to set up a new instance
 *  is done.  this reads the config options from 
 *  the rsyslog conf and takes appropriate setup
 *  actions. */
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

/*  register our plugin entry points
 *  with the rsyslog core engine */
BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES
CODEqueryEtryPt_TXIF_OMOD_QUERIES /*  supports transaction interface */ 
ENDqueryEtryPt

/*  note we do not support rsyslog v5 syntax */
BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* only supports rsyslog 6 configs */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	INITChkCoreFeature(bCoreSupportsBatching, CORE_FEATURE_BATCHING);
    if (!bCoreSupportsBatching) {
        errmsg.LogError(0, NO_ERRCODE, "omhiredis: rsyslog core does not support batching - abort");
        ABORT_FINALIZE(RS_RET_ERR);
    }
	DBGPRINTF("omhiredis: module compiled with rsyslog version %s.\n", VERSION);
ENDmodInit
