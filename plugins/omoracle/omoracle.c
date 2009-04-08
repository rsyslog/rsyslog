/** omoracle.c

    This is an output module feeding directly to an Oracle
    database. It uses Oracle Call Interface, a propietary module
    provided by Oracle.

    Selector lines to be used are of this form:

    :omoracle:;TemplateName

    The module gets its configuration via rsyslog $... directives,
    namely:

    $OmoracleDBUser: user name to log in on the database.
    $OmoracleDBPassword: password to log in on the database.
    $OmoracleDB: connection string (an Oracle easy connect or a db
    name as specified by tnsnames.ora)
    $OmoracleBatchSize: Number of elements to send to the DB.

    All fields are mandatory. The dbstring can be an Oracle easystring
    or a DB name, as present in the tnsnames.ora file.

    Author: Luis Fernando Muñoz Mejías
    <Luis.Fernando.Munoz.Mejias@cern.ch>

    This file is part of rsyslog.
*/
#include "config.h"
#include "rsyslog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <oci.h>
#include <errno.h>
#include <stdarg.h>
#include <signal.h>
#include <time.h>
#include <assert.h>
#include "dirty.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include "cfsysline.h"
#include "omoracle.h"

MODULE_TYPE_OUTPUT

/**  */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(errmsg)

/**  */
struct oracle_batch
{
	/* Batch size */
	int size;
	/* Last element inserted in the buffer. The batch will be
	 * executed when n == size */
	int n;
	/* Statements to run on this transaction */
	char** statements;
};

typedef struct _instanceData {
	/* Environment handler, the base for any OCI work. */
	OCIEnv* environment;
	/* Session handler, the actual DB connection object. */
	OCISession* session;
	/* Error handler for OCI calls. */
	OCIError* error;
	/* Prepared statement. */
	OCIStmt* statement;
	/* Service handler. */
	OCISvcCtx* service;
	/* Credentials object for the connection. */
	OCIAuthInfo* authinfo;
	/* Binding parameters, currently unused */
	OCIBind* binding;
	/* Connection string, kept here for possible retries. */
	char* connection;
	/* Batch */
	struct oracle_batch batch;
} instanceData;

/** Database name, to be filled by the $OmoracleDB directive */
static char* db_name;
/** Database user name, to be filled by the $OmoracleDBUser
 * directive */
static char* db_user;
/** Database password, to be filled by the $OmoracleDBPassword */
static char* db_password;
/** Batch size. */
static int batch_size;

/** Generic function for handling errors from OCI.

    It will be called only inside CHECKERR and CHECKENV macros.

    Arguments: handle The error or environment handle to check.
    htype: OCI_HTYPE_* constant, usually OCI_HTYPE_ERROR or
    OCI_HTYPE_ENV
    status: status code to check, usually the return value of an OCI
    function.

    Returns OCI_SUCCESS on success, OCI_ERROR otherwise.
*/
static int oci_errors(void* handle, ub4 htype, sword status)
{
	sb4 errcode;
	unsigned char buf[MAX_BUFSIZE];
	
	switch (status) {
	case OCI_SUCCESS:
		return OCI_SUCCESS;
		break;
	case OCI_SUCCESS_WITH_INFO:
		errmsg.LogError(0, NO_ERRCODE, "OCI SUCCESS - With info\n");
		break;
	case OCI_NEED_DATA:
		errmsg.LogError(0, NO_ERRCODE, "OCI NEEDS MORE DATA\n");
		break;
	case OCI_ERROR:
		dbgprintf ("OCI GENERAL ERROR\n");
		if (handle) {
			OCIErrorGet(handle, 1, NULL, &errcode, buf,
				    sizeof buf, htype);
			errmsg.LogError(0, NO_ERRCODE, "Error message: %s", buf);
		} else
			errmsg.LogError(0, NO_ERRCODE, "NULL handle\n"
					 "Unable to extract further "
					 "information");
		break;
	case OCI_INVALID_HANDLE:
		errmsg.LogError(0, NO_ERRCODE, "OCI INVALID HANDLE\n");
		break;
	case OCI_STILL_EXECUTING:
		errmsg.LogError(0, NO_ERRCODE, "Still executing...\n");
		break;
	case OCI_CONTINUE:
		errmsg.LogError(0, NO_ERRCODE, "OCI CONTINUE\n");
		break;
	}
	return OCI_ERROR;
}


/* Resource allocation */
BEGINcreateInstance
CODESTARTcreateInstance

	ASSERT(pData != NULL);
	
	CHECKENV(pData->environment,
		 OCIEnvCreate((void*) &(pData->environment), OCI_DEFAULT,
			      NULL, NULL, NULL, NULL, 0, NULL));
	CHECKENV(pData->environment,
		 OCIHandleAlloc(pData->environment, (void*) &(pData->error),
				OCI_HTYPE_ERROR, 0, NULL));
	CHECKENV(pData->environment,
		 OCIHandleAlloc(pData->environment, (void*) &(pData->authinfo),
				OCI_HTYPE_AUTHINFO, 0, NULL));
	CHECKENV(pData->environment,
		 OCIHandleAlloc(pData->environment, (void*) &(pData->statement),
				OCI_HTYPE_STMT, 0, NULL));

	pData->batch.n = 0;
	pData->batch.size = batch_size;
	pData->batch.statements = calloc(pData->batch.size,
					 sizeof *pData->batch.statements);
	CHKmalloc(pData->batch.statements);

finalize_it:
ENDcreateInstance

/** Close the session and free anything allocated by
    createInstance. */
BEGINfreeInstance
CODESTARTfreeInstance

	OCISessionRelease(pData->service, pData->error, NULL, 0, OCI_DEFAULT);
	OCIHandleFree(pData->environment, OCI_HTYPE_ENV);
	OCIHandleFree(pData->error, OCI_HTYPE_ERROR);
	OCIHandleFree(pData->service, OCI_HTYPE_SVCCTX);
	OCIHandleFree(pData->authinfo, OCI_HTYPE_AUTHINFO);
	OCIHandleFree(pData->statement, OCI_HTYPE_STMT);
	free(pData->connection);
	while (pData->batch.size--) 
		free(pData->batch.statements[pData->batch.size]);
	free(pData->batch.statements);
	dbgprintf ("omoracle freed all its resources\n");

ENDfreeInstance

BEGINtryResume
CODESTARTtryResume
	/* Here usually only a reconnect is done. The rsyslog core will call
	 * this entry point from time to time when the action suspended itself.
	 * Note that the rsyslog core expects that if the plugin suspended itself
	 * the action was not carried out during that invocation. Thus, rsyslog
	 * will call the action with *the same* data item again AFTER a resume
	 * was successful. As such, tryResume should NOT write the failed data
	 * item. If it needs to for some reason, it must delete the item again,
	 * otherwise, it will get duplicated.
	 * This handling inside the rsyslog core is important to be able to
	 * preserve data over rsyslog restarts. With it, the core can ensure that
	 * it queues all not-yet-processed messages without the plugin needing
	 * to take care about that.
	 * So in essence, it is recommended that just a reconnet is tried, but
	 * the last action not restarted. Note that it is not a real problem
	 * (but causes a slight performance degradation) if tryResume returns
	 * successfully but the next call to doAction() immediately returns
	 * RS_RET_SUSPENDED. So it is OK to do the actual restart inside doAction().
	 * ... of course I don't know why Oracle might need a full restart...
	 * rgerhards, 2009-03-26
	 */
	dbgprintf("Attempting to reconnect to DB server\n");
	OCISessionRelease(pData->service, pData->error, NULL, 0, OCI_DEFAULT);
	OCIHandleFree(pData->service, OCI_HTYPE_SVCCTX);
	CHECKERR(pData->error, OCISessionGet(pData->environment, pData->error,
					     &pData->service, pData->authinfo,
					     pData->connection,
					     strlen(pData->connection), NULL, 0,
					     NULL, NULL, NULL, OCI_DEFAULT));

finalize_it:
ENDtryResume

static rsRetVal startSession(instanceData* pData, char* connection, char* user,
			     char * password)
{
	DEFiRet;
	CHECKERR(pData->error,
		 OCIAttrSet(pData->authinfo, OCI_HTYPE_AUTHINFO, user,
			    strlen(user), OCI_ATTR_USERNAME, pData->error));
	CHECKERR(pData->error,
		 OCIAttrSet(pData->authinfo, OCI_HTYPE_AUTHINFO, password,
			    strlen(password), OCI_ATTR_PASSWORD, pData->error));
	CHECKERR(pData->error,
		 OCISessionGet(pData->environment, pData->error,
			       &pData->service, pData->authinfo, connection,
			       strlen(connection), NULL, 0, NULL, NULL, NULL,
			       OCI_DEFAULT));
finalize_it:
	if (iRet != RS_RET_OK)
		errmsg.LogError(0, NO_ERRCODE,
				"Unable to start Oracle session\n");
	RETiRet;
}


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	/* Right now, this module is compatible with nothing. */
	dbgprintf ("***** OMORACLE ***** At isCompatibleWithFeature\n");
	iRet = RS_RET_INCOMPATIBLE;
ENDisCompatibleWithFeature

BEGINparseSelectorAct

CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1);

	if (strncmp((char*) p, ":omoracle:", sizeof ":omoracle:" - 1)) {
		ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
	}

	p += sizeof ":omoracle:" - 1;

	if (*p == '\0' || *p == ',') {
		errmsg.LogError(0, NO_ERRCODE,
				"Wrong char processing module arguments: %c\n",
				*p);
		ABORT_FINALIZE(RS_RET_INVALID_PARAMS);
	}

	CHKiRet(cflineParseTemplateName(&p, *ppOMSR, 0,
					OMSR_RQD_TPL_OPT_SQL, " StdFmt"));
	CHKiRet(createInstance(&pData));
	CHKmalloc(pData->connection = strdup(db_name));
	CHKiRet(startSession(pData, db_name, db_user, db_password));
	
	dbgprintf ("omoracle module got all its resources allocated "
		   "and connected to the DB\n");
	
CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct

BEGINdoAction
	int i;
	int n;
CODESTARTdoAction
	dbgprintf("omoracle attempting to execute statement %s\n", *ppString);

	if (pData->batch.n == pData->batch.size) {
		dbgprintf("omoracle batch size limit hit, sending into DB\n");
		for (i = 0; i < pData->batch.n; i++) {
			if (pData->batch.statements[i] == NULL)
				continue;
			n = strlen(pData->batch.statements[i]);
			CHECKERR(pData->error,
				 OCIStmtPrepare(pData->statement,
						pData->error,
						pData->batch.statements[i], n,
						OCI_NTV_SYNTAX, OCI_DEFAULT));
			CHECKERR(pData->error,
				 OCIStmtExecute(pData->service,
						pData->statement,
						pData->error,
						1, 0, NULL, NULL, OCI_DEFAULT));
			free(pData->batch.statements[i]);
			pData->batch.statements[i] = NULL;
		}
		CHECKERR(pData->error,
			 OCITransCommit(pData->service, pData->error, 0));
		pData->batch.n = 0;
	}
	pData->batch.statements[pData->batch.n] = strdup(*ppString);
	CHKmalloc(pData->batch.statements[pData->batch.n]);
	pData->batch.n++;

finalize_it:
	dbgprintf ("omoracle %s at executing statement %s\n",
		   iRet?"did not succeed":"succeeded", *ppString);
ENDdoAction

BEGINmodExit
CODESTARTmodExit
ENDmodExit

BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
ENDdbgPrintInstInfo


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
ENDqueryEtryPt

static rsRetVal
resetConfigVariables(uchar __attribute__((unused)) *pp,
		     void __attribute__((unused)) *pVal)
{
	int n;
	DEFiRet;
	if(db_user != NULL)
		free(db_user);
	if(db_name != NULL)
		free(db_name);
	if (db_password != NULL) {
		n = strlen(db_password);
		memset(db_password, 0, n);
		free(db_password);
	}
	db_name = db_user = db_password = NULL;
	RETiRet;
}

BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION;
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	/* CHKiRet(omsdRegCFSLineHdlr((uchar*)"actionomoracle",  */
	CHKiRet(omsdRegCFSLineHdlr((uchar*) "resetconfigvariables", 1,
				   eCmdHdlrCustomHandler, resetConfigVariables,
				   NULL, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar*) "omoracledbuser", 0,
				   eCmdHdlrGetWord, NULL, &db_user,
				   STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar*) "omoracledbpassword", 0,
				   eCmdHdlrGetWord, NULL, &db_password,
				   STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar*) "omoracledb", 0,
				   eCmdHdlrGetWord, NULL, &db_name,
				   STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar*) "omoraclebatchsize", 0,
				   eCmdHdlrInt, NULL, &batch_size,
				   STD_LOADABLE_MODULE_ID));
ENDmodInit
