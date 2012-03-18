/* ommongodb.c
 * Output module for mongodb.
 * Note: this module uses the libmongo-client library. The original 10gen
 * mongodb C interface is crap. Obtain the library here:
 * https://github.com/algernon/libmongo-client
 *
 * Copyright 2007-2012 Rainer Gerhards and Adiscon GmbH.
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>
#include <time.h>
#include <mongo.h>

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
MODULE_CNFNAME("ommongodb")
/* internal structures
 */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(errmsg)

typedef struct _instanceData {
	mongo_sync_connection *conn;
	uchar *server;
	int port;
        uchar *db;
	uchar *collection;
	uchar *uid;
	uchar *pwd;
	uchar *dbNcoll;
        unsigned uLastMongoDBErrno;
	uchar *tplName;
} instanceData;


/* tables for interfacing with the v6 config system */
/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {
	{ "server", eCmdHdlrGetWord, 0 },
	{ "serverport", eCmdHdlrInt, 0 },
	{ "db", eCmdHdlrGetWord, 0 },
	{ "collection", eCmdHdlrGetWord, 0 },
	{ "uid", eCmdHdlrGetWord, 0 },
	{ "pwd", eCmdHdlrGetWord, 0 },
	{ "template", eCmdHdlrGetWord, 1 }
};
static struct cnfparamblk actpblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(actpdescr)/sizeof(struct cnfparamdescr),
	  actpdescr
	};

BEGINcreateInstance
CODESTARTcreateInstance
ENDcreateInstance

BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	/* use this to specify if select features are supported by this
	 * plugin. If not, the framework will handle that. Currently, only
	 * RepeatedMsgReduction ("last message repeated n times") is optional.
	 */
	if(eFeat == sFEATURERepeatedMsgReduction)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature

static void closeMongoDB(instanceData *pData)
{
	if(pData->conn != NULL) {
                mongo_sync_disconnect(pData->conn);
		pData->conn = NULL;
	}
}


BEGINfreeInstance
CODESTARTfreeInstance
	closeMongoDB(pData);
	free(pData->server);
	free(pData->db);
	free(pData->collection);
	free(pData->uid);
	free(pData->pwd);
	free(pData->tplName);
ENDfreeInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	/* nothing special here */
ENDdbgPrintInstInfo


/* The following function is responsible for initializing a
 * MySQL connection.
 * Initially added 2004-10-28 mmeckelein
 */
static rsRetVal initMongoDB(instanceData *pData, int bSilent)
{
	char *server;
	DEFiRet;

	server = (pData->server == NULL) ? "127.0.0.1" : (char*) pData->server;
	DBGPRINTF("ommongodb: trying connect to '%s' at port %d\n", server, pData->port);
        
	pData->conn = mongo_sync_connect(server, pData->port, TRUE);
	if(pData->conn == NULL) {
		if(!bSilent)
			errmsg.LogError(0, RS_RET_SUSPENDED,
					"can not initialize MongoDB handle");
                ABORT_FINALIZE(RS_RET_SUSPENDED);
	}

finalize_it:
	RETiRet;
}

rsRetVal writeMongoDB(uchar *psz, instanceData *pData)
{
	bson *doc;
	char **szParams;
	DEFiRet;

	/* see if we are ready to proceed */
	if(pData->conn == NULL) {
		CHKiRet(initMongoDB(pData, 0));
	}

	szParams = (char**)(void*) psz;
	doc = bson_build(BSON_TYPE_STRING, "p_proc", szParams[0], -1,
			 BSON_TYPE_STRING, "p_sys", szParams[1], -1,
			 BSON_TYPE_STRING, "time", szParams[2], -1,
			 BSON_TYPE_STRING, "crit", szParams[3], -1,
			 BSON_TYPE_STRING, "rawmsg", szParams[4], -1,
			 BSON_TYPE_NONE);
	if(doc == NULL) {
		dbgprintf("ommongodb: error creating BSON doc\n");
		ABORT_FINALIZE(RS_RET_ERR);
	}
	bson_finish(doc);
	if(!mongo_sync_cmd_insert(pData->conn, (char*)pData->dbNcoll, doc, NULL)) {
		perror ("mongo_sync_cmd_insert()");
		dbgprintf("ommongodb: insert error\n");
		ABORT_FINALIZE(RS_RET_ERR);
	}
	bson_free(doc);

finalize_it:
	if(iRet == RS_RET_OK) {
		pData->uLastMongoDBErrno = 0; /* reset error for error supression */
	}

        
	RETiRet;
}

BEGINtryResume
CODESTARTtryResume
	if(pData->conn == NULL) {
		iRet = initMongoDB(pData, 1);
	}
ENDtryResume

BEGINdoAction
CODESTARTdoAction
	iRet = writeMongoDB(ppString[0], pData);
ENDdoAction


static inline void
setInstParamDefaults(instanceData *pData)
{
	pData->server = NULL;
	pData->port = 27017;
	pData->db = NULL;
	pData->collection= NULL;
	pData->uid = NULL;
	pData->pwd = NULL;
	pData->tplName = NULL;
}

BEGINnewActInst
	struct cnfparamvals *pvals;
	int i;
	unsigned lendb, lencoll;
CODESTARTnewActInst
	if((pvals = nvlstGetParams(lst, &actpblk, NULL)) == NULL) {
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	CHKiRet(createInstance(&pData));
	setInstParamDefaults(pData);

	CODE_STD_STRING_REQUESTparseSelectorAct(1)
	for(i = 0 ; i < actpblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(actpblk.descr[i].name, "server")) {
			pData->server = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "serverport")) {
			pData->port = (int) pvals[i].val.d.n, NULL;
		} else if(!strcmp(actpblk.descr[i].name, "db")) {
			pData->db = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "collection")) {
			pData->collection = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "uid")) {
			pData->uid = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "pwd")) {
			pData->pwd = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "template")) {
			pData->tplName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else {
			dbgprintf("ommongodb: program error, non-handled "
			  "param '%s'\n", actpblk.descr[i].name);
		}
	}

	if(pData->tplName == NULL) {
		CHKiRet(OMSRsetEntry(*ppOMSR, 0, (uchar*) strdup(" StdDBFmt"),
			OMSR_TPL_AS_ARRAY));
	} else {
		CHKiRet(OMSRsetEntry(*ppOMSR, 0,
			(uchar*) strdup((char*) pData->tplName),
			OMSR_TPL_AS_ARRAY));
	}

	if(pData->db == NULL)
		pData->db = (uchar*)strdup("syslog");
	if(pData->collection == NULL)
		pData->collection = (uchar*)strdup("log");

	/* we now create a db+collection string as we need to pass this
	 * into the API and we do not want to generate it each time ;)
	 * +2 ==> dot as delimiter and \0
	 */
	lendb = strlen((char*)pData->db);
	lencoll = strlen((char*)pData->collection);
	CHKmalloc(pData->dbNcoll = malloc(lendb+lencoll+2));
	memcpy(pData->dbNcoll, pData->db, lendb);
	pData->dbNcoll[lendb] = '.';
	/* lencoll+1 => copy \0! */
	memcpy(pData->dbNcoll+lendb+1, pData->collection, lencoll+1);

CODE_STD_FINALIZERnewActInst
	cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst


BEGINparseSelectorAct
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	if(!strncmp((char*) p, ":ommongodb:", sizeof(":ommongodb:") - 1)) {
		errmsg.LogError(0, RS_RET_LEGA_ACT_NOT_SUPPORTED,
			"ommongodb supports only v6 config format, use: "
			"action(type=\"ommongodb\" server=...)");
	}
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
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	INITChkCoreFeature(bCoreSupportsBatching, CORE_FEATURE_BATCHING);
	DBGPRINTF("ommongodb: module compiled with rsyslog version %s.\n", VERSION);
	//DBGPRINTF("ommongodb: %susing transactional output interface.\n", bCoreSupportsBatching ? "" : "not ");
ENDmodInit
