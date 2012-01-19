/* omlibdbi.c
 * This is the implementation of the dbi output module.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * This depends on libdbi being present with the proper settings. Older
 * versions do not necessarily have them. Please visit this bug tracker
 * for details: http://bugzilla.adiscon.com/show_bug.cgi?id=31
 *
 * File begun on 2008-02-14 by RGerhards (extracted from syslogd.c)
 *
 * Copyright 2008-2012 Adiscon GmbH.
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
#include <time.h>
#include <dbi/dbi.h>
#include "dirty.h"
#include "syslogd-types.h"
#include "cfsysline.h"
#include "conf.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "debug.h"
#include "errmsg.h"

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP

/* internal structures
 */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(errmsg)
static int bDbiInitialized = 0;	/* dbi_initialize() can only be called one - this keeps track of it */

typedef struct _instanceData {
	dbi_conn conn;		/* handle to database */
	uchar *drvrName;	/* driver to use */
	uchar *host;		/* host to connect to */
	uchar *usrName;		/* user name for connect */
	uchar *pwd;		/* password for connect */
	uchar *dbName;		/* database to use */
	unsigned uLastDBErrno;	/* last errno returned by libdbi or 0 if all is well */
} instanceData;

typedef struct configSettings_s {
	uchar *dbiDrvrDir;	/* global: where do the dbi drivers reside? */
	uchar *drvrName;	/* driver to use */
	uchar *host;		/* host to connect to */
	uchar *usrName;		/* user name for connect */
	uchar *pwd;		/* password for connect */
	uchar *dbName;		/* database to use */
} configSettings_t;

SCOPING_SUPPORT; /* must be set AFTER configSettings_t is defined */

BEGINinitConfVars		/* (re)set config variables to default values */
CODESTARTinitConfVars 
	cs.dbiDrvrDir = NULL;
	cs.drvrName = NULL;
	cs.host = NULL;
	cs.usrName = NULL;	
	cs.pwd = NULL;
	cs.dbName = NULL;
ENDinitConfVars


/* config settings */
#ifdef HAVE_DBI_R
static dbi_inst dbiInst;
#endif


BEGINcreateInstance
CODESTARTcreateInstance
ENDcreateInstance


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	/* we do not like repeated message reduction inside the database */
ENDisCompatibleWithFeature


/* The following function is responsible for closing a
 * database connection.
 */
static void closeConn(instanceData *pData)
{
	ASSERT(pData != NULL);

	if(pData->conn != NULL) {	/* just to be on the safe side... */
		dbi_conn_close(pData->conn);
		pData->conn = NULL;
	}
}

BEGINfreeInstance
CODESTARTfreeInstance
	closeConn(pData);
	free(pData->drvrName);
	free(pData->host);
	free(pData->usrName);
	free(pData->pwd);
	free(pData->dbName);
ENDfreeInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	/* nothing special here */
ENDdbgPrintInstInfo


/* log a database error with descriptive message.
 * We check if we have a valid database handle. If not, we simply
 * report an error, but can not be specific. RGerhards, 2007-01-30
 */
static void
reportDBError(instanceData *pData, int bSilent)
{
	unsigned uDBErrno;
	char errMsg[1024];
	const char *pszDbiErr;

	BEGINfunc
	ASSERT(pData != NULL);

	/* output log message */
	errno = 0;
	if(pData->conn == NULL) {
		errmsg.LogError(0, NO_ERRCODE, "unknown DB error occured - could not obtain connection handle");
	} else { /* we can ask dbi for the error description... */
		uDBErrno = dbi_conn_error(pData->conn, &pszDbiErr);
		snprintf(errMsg, sizeof(errMsg)/sizeof(char), "db error (%d): %s\n", uDBErrno, pszDbiErr);
		if(bSilent || uDBErrno == pData->uLastDBErrno)
			dbgprintf("libdbi, DBError(silent): %s\n", errMsg);
		else {
			pData->uLastDBErrno = uDBErrno;
			errmsg.LogError(0, NO_ERRCODE, "%s", errMsg);
		}
	}
		
	ENDfunc
}


/* The following function is responsible for initializing a connection
 */
static rsRetVal initConn(instanceData *pData, int bSilent)
{
	DEFiRet;
	int iDrvrsLoaded;

	ASSERT(pData != NULL);
	ASSERT(pData->conn == NULL);

	if(bDbiInitialized == 0) {
		/* we need to init libdbi first */
#		ifdef HAVE_DBI_R
		iDrvrsLoaded = dbi_initialize_r((char*) cs.dbiDrvrDir, &dbiInst);
#		else
		iDrvrsLoaded = dbi_initialize((char*) cs.dbiDrvrDir);
#		endif
		if(iDrvrsLoaded == 0) {
			errmsg.LogError(0, RS_RET_SUSPENDED, "libdbi error: libdbi or libdbi drivers not present on this system - suspending.");
			ABORT_FINALIZE(RS_RET_SUSPENDED);
		} else if(iDrvrsLoaded < 0) {
			errmsg.LogError(0, RS_RET_SUSPENDED, "libdbi error: libdbi could not be "
				"initialized (do you have any dbi drivers installed?) - suspending.");
			ABORT_FINALIZE(RS_RET_SUSPENDED);
		}
		bDbiInitialized = 1; /* we are done for the rest of our existence... */
	}

#	ifdef HAVE_DBI_R
	pData->conn = dbi_conn_new_r((char*)pData->drvrName, dbiInst);
#	else
	pData->conn = dbi_conn_new((char*)pData->drvrName);
#	endif
	if(pData->conn == NULL) {
		errmsg.LogError(0, RS_RET_SUSPENDED, "can not initialize libdbi connection");
		iRet = RS_RET_SUSPENDED;
	} else { /* we could get the handle, now on with work... */
		/* Connect to database */
		dbi_conn_set_option(pData->conn, "host",     (char*) pData->host);
		dbi_conn_set_option(pData->conn, "username", (char*) pData->usrName);
		dbi_conn_set_option(pData->conn, "dbname",   (char*) pData->dbName);
		if(pData->pwd != NULL)
			dbi_conn_set_option(pData->conn, "password", (char*) pData->pwd);
		if(dbi_conn_connect(pData->conn) < 0) {
			reportDBError(pData, bSilent);
			closeConn(pData); /* ignore any error we may get */
			iRet = RS_RET_SUSPENDED;
		}
	}

finalize_it:
	RETiRet;
}


/* The following function writes the current log entry
 * to an established database connection.
 */
rsRetVal writeDB(uchar *psz, instanceData *pData)
{
	DEFiRet;
	dbi_result dbiRes = NULL;

	ASSERT(psz != NULL);
	ASSERT(pData != NULL);

	/* see if we are ready to proceed */
	if(pData->conn == NULL) {
		CHKiRet(initConn(pData, 0));
	}

	/* try insert */
	if((dbiRes = dbi_conn_query(pData->conn, (const char*)psz)) == NULL) {
		/* error occured, try to re-init connection and retry */
		closeConn(pData); /* close the current handle */
		CHKiRet(initConn(pData, 0)); /* try to re-open */
		if((dbiRes = dbi_conn_query(pData->conn, (const char*)psz)) == NULL) { /* re-try insert */
			/* we failed, giving up for now */
			reportDBError(pData, 0);
			closeConn(pData); /* free ressources */
			ABORT_FINALIZE(RS_RET_SUSPENDED);
		}
	}

finalize_it:
	if(iRet == RS_RET_OK) {
		pData->uLastDBErrno = 0; /* reset error for error supression */
	}

	if(dbiRes != NULL)
		dbi_result_free(dbiRes);

	RETiRet;
}


BEGINtryResume
CODESTARTtryResume
	if(pData->conn == NULL) {
		iRet = initConn(pData, 1);
	}
ENDtryResume

BEGINdoAction
CODESTARTdoAction
	dbgprintf("\n");
	iRet = writeDB(ppString[0], pData);
ENDdoAction


BEGINparseSelectorAct
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	if(!strncmp((char*) p, ":omlibdbi:", sizeof(":omlibdbi:") - 1)) {
		p += sizeof(":omlibdbi:") - 1; /* eat indicator sequence (-1 because of '\0'!) */
	} else {
		ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
	}

	/* ok, if we reach this point, we have something for us */
	CHKiRet(createInstance(&pData));

	/* no create the instance based on what we currently have */
	if(cs.drvrName == NULL) {
		errmsg.LogError(0, RS_RET_NO_DRIVERNAME, "omlibdbi: no db driver name given - action can not be created");
		ABORT_FINALIZE(RS_RET_NO_DRIVERNAME);
	}

	if((pData->drvrName = (uchar*) strdup((char*)cs.drvrName)) == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	/* NULL values are supported because drivers have different needs.
	 * They will err out on connect. -- rgerhards, 2008-02-15
	 */
	if(cs.host    != NULL)
		if((pData->host     = (uchar*) strdup((char*)cs.host))     == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	if(cs.usrName != NULL)
		if((pData->usrName  = (uchar*) strdup((char*)cs.usrName))  == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	if(cs.dbName  != NULL)
		if((pData->dbName   = (uchar*) strdup((char*)cs.dbName))   == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	if(cs.pwd     != NULL)
		if((pData->pwd      = (uchar*) strdup((char*)cs.pwd))       == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

	CHKiRet(cflineParseTemplateName(&p, *ppOMSR, 0, OMSR_RQD_TPL_OPT_SQL, (uchar*) " StdDBFmt"));

CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINmodExit
CODESTARTmodExit
	/* if we initialized libdbi, we now need to cleanup */
	if(bDbiInitialized) {
#		ifdef HAVE_DBI_R
		dbi_shutdown_r(dbiInst);
#		else
		dbi_shutdown();
#		endif
	}
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
ENDqueryEtryPt


/* Reset config variables for this module to default values.
 */
static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	DEFiRet;
	free(cs.dbiDrvrDir);
	cs.dbiDrvrDir = NULL;
	free(cs.drvrName);
	cs.drvrName = NULL;
	free(cs.host);
	cs.host = NULL;
	free(cs.usrName);
	cs.usrName = NULL;
	free(cs.pwd);
	cs.pwd = NULL;
	free(cs.dbName);
	cs.dbName = NULL;
	RETiRet;
}


BEGINmodInit()
CODESTARTmodInit
SCOPINGmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"actionlibdbidriverdirectory", 0, eCmdHdlrGetWord, NULL, &cs.dbiDrvrDir, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"actionlibdbidriver", 0, eCmdHdlrGetWord, NULL, &cs.drvrName, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"actionlibdbihost", 0, eCmdHdlrGetWord, NULL, &cs.host, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"actionlibdbiusername", 0, eCmdHdlrGetWord, NULL, &cs.usrName, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"actionlibdbipassword", 0, eCmdHdlrGetWord, NULL, &cs.pwd, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"actionlibdbidbname", 0, eCmdHdlrGetWord, NULL, &cs.dbName, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
	DBGPRINTF("omlibdbi compiled with version %s loaded, libdbi version %s\n", VERSION, dbi_version());
ENDmodInit

/* vim:set ai:
 */
