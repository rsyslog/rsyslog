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

#define countof(X) ( (size_t) ( sizeof(X)/sizeof*(X) ) )

#define DEFAULT_SERVER "127.0.0.1"
#define DEFAULT_DATABASE "syslog"
#define DEFAULT_COLLECTION "log"
#define DEFAULT_DB_COLLECTION "syslog.log"

//i just defined some constants, i couldt not find the limit
#define MONGO_DB_NAME_SIZE 128
#define MONGO_COLLECTION_NAME_SIZE 128

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("ommongodb")
/* internal structures
 */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(errmsg)

typedef struct _instanceData {
	mongo_sync_connection *conn;
	// OLD:
#if 0
        mongo_connection_options opts[1];
        mongo_conn_return status;
#endif
        char db[MONGO_DB_NAME_SIZE];
        char collection[MONGO_COLLECTION_NAME_SIZE];
        char dbcollection[MONGO_DB_NAME_SIZE + MONGO_COLLECTION_NAME_SIZE + 1];
        unsigned uLastMongoDBErrno;
	unsigned iSrvPort;	/* sample: server port */
} instanceData;

char db[_DB_MAXDBLEN+2];
static int iSrvPort = 27017;
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
	ASSERT(pData != NULL);

	if(pData->conn != NULL) {
                mongo_sync_disconnect(pData->conn);
		pData->conn = NULL;
	}
}

BEGINfreeInstance
CODESTARTfreeInstance
	closeMongoDB(pData);
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
	DEFiRet;

	ASSERT(pData != NULL);
	ASSERT(pData->conn == NULL);

        //I'm trying to fallback to a default here
#if 0
        if(pData->opts->port == 0)
         pData->opts->port = 27017;

        if(pData->opts->host == 0x00)
            strcpy(pData->opts->host,DEFAULT_SERVER);

#endif
        if(pData->dbcollection == 0x00)
            strcpy(pData->dbcollection,DEFAULT_DB_COLLECTION);
        
	pData->conn = mongo_sync_connect("127.0.0.1", pData->iSrvPort, TRUE);
	if(pData->conn == NULL) {
                errmsg.LogError(0, RS_RET_SUSPENDED, "can not initialize MongoDB handle");
                ABORT_FINALIZE(RS_RET_SUSPENDED);
	}

finalize_it:
	RETiRet;
}

//we must implement it
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
	doc = bson_build(BSON_TYPE_STRING, "msg", szParams[0], -1,
			 BSON_TYPE_STRING, "facility", szParams[1], -1,
			 BSON_TYPE_STRING, "hostname", szParams[2], -1,
			 BSON_TYPE_STRING, "priority", szParams[3], -1,
			 BSON_TYPE_NONE);
	if(doc == NULL) {
		dbgprintf("ommongodb: error creating BSON doc\n");
		ABORT_FINALIZE(RS_RET_ERR);
	}
	bson_finish(doc);
	if(!mongo_sync_cmd_insert(pData->conn, "syslog.doc", doc, NULL)) {
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

BEGINparseSelectorAct
	//int iMongoDBPropErr = 0;
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
       
	if(!strncmp((char*) p, ":ommongodb:", sizeof(":ommongodb:") - 1)) {
		p += sizeof(":ommongodb:") - 1; /* eat indicator sequence  (-1 because of '\0'!) */
	} else {
		ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
	}

        CHKiRet(createInstance(&pData));
        
#if 0
        if(getSubString(&p, pData->opts->host, MAXHOSTNAMELEN+1, ','))
            strcpy(pData->opts->host,DEFAULT_SERVER);
#endif
            
        //we must define the max db name
        if(getSubString(&p,pData->db,255,','))
            strcpy(pData->db,DEFAULT_DATABASE);
        if(getSubString(&p,pData->collection,255,';'))
            strcpy(pData->collection,DEFAULT_COLLECTION);
        if(*(p-1) == ';')
		--p;	

        
       	CHKiRet(cflineParseTemplateName(&p, *ppOMSR, 0, OMSR_TPL_AS_ARRAY, (uchar*) " StdMongoDBFmt"));
        
        
        pData->iSrvPort = iSrvPort;	/* set configured port */
        sprintf(pData->dbcollection,"%s.%s",pData->db,pData->collection);
        CHKiRet(initMongoDB(pData, 0));
	
CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINmodExit
CODESTARTmodExit
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
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
