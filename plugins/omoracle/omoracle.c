/** omoracle.c

    This is an output module feeding directly to an Oracle
    database. It uses Oracle Call Interface, a propietary module
    provided by Oracle.

    Config lines to be used are of this form:

    :omoracle:dbstring,user,password;StatementTemplate

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
} instanceData;

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
	dbgprintf ("omoracle freed all its resources\n");
	RETiRet;

ENDfreeInstance

BEGINtryResume
CODESTARTtryResume
	dbgprintf("Attempting to restart the last action\n");
	OCISessionRelease(pData->service, pData->error, NULL, 0, OCI_DEFAULT);
	OCIHandleFree(pData->service, OCI_HTYPE_SVCCTX);
	CHECKERR(pData->error, OCISessionGet(pData->environment, pData->error,
					     &pData->service, pData->authinfo,
					     pData->connection,
					     strlen(pData->connection), NULL, 0,
					     NULL, NULL, NULL, OCI_DEFAULT));
	CHECKERR(pData->error, OCIStmtExecute(pData->service, pData->statement,
					      pData->error, 1, 0, NULL, NULL,
					      OCI_DEFAULT));

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
		errmsg.LogError(0, NO_ERRCODE, "Unable to start Oracle session\n");
	RETiRet;
}


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	/* Right now, this module is compatible with nothing. */
	dbgprintf ("***** OMORACLE ***** At isCompatibleWithFeature\n");
	iRet = RS_RET_INCOMPATIBLE;
ENDisCompatibleWithFeature

BEGINparseSelectorAct

	char user[_DB_MAXUNAMELEN];
	char pwd[_DB_MAXPWDLEN];
	char connection_string[MAXHOSTNAMELEN];

CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1);

	if (strncmp((char*) p, ":omoracle:", sizeof ":omoracle:" - 1)) {
		ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
	}

	p += sizeof ":omoracle:" - 1;

	if (*p == '\0' || *p == ',') {
		errmsg.LogError(0, NO_ERRCODE, "Wrong char processing module arguments: %c\n", *p);
		ABORT_FINALIZE(RS_RET_INVALID_PARAMS);
	}

	CHKiRet(getSubString(&p, connection_string, MAXHOSTNAMELEN, ','));
	CHKiRet(getSubString(&p, user, _DB_MAXUNAMELEN, ','));
	CHKiRet(getSubString(&p, pwd, _DB_MAXPWDLEN, ';'));
	p--;
	CHKiRet(cflineParseTemplateName(&p, *ppOMSR, 0,
					OMSR_RQD_TPL_OPT_SQL, " StdFmt"));
	CHKiRet(createInstance(&pData));
	pData->connection = strdup(connection_string);
	if (pData->connection == NULL) {
		iRet = RS_RET_OUT_OF_MEMORY;
		goto finalize_it;
	}
	CHKiRet(startSession(pData, connection_string, user, pwd));
	
	dbgprintf ("omoracle module got all its resources allocated "
		   "and connected to the DB\n");
	
	memset(user, 0, sizeof user);
	memset(pwd, 0, sizeof pwd);
CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct

BEGINdoAction
CODESTARTdoAction
	dbgprintf("omoracle attempting to execute statement %s\n", *ppString);
	CHECKERR(pData->error,
		 OCIStmtPrepare(pData->statement, pData->error, *ppString,
				strlen(*ppString), OCI_NTV_SYNTAX,
				OCI_DEFAULT));
	CHECKERR(pData->error,
		 OCIStmtExecute(pData->service, pData->statement, pData->error,
				1, 0, NULL, NULL, OCI_DEFAULT));
	CHECKERR(pData->error,
		 OCITransCommit(pData->service, pData->error, 0));
finalize_it:
	dbgprintf ("omoracle %s at executing statement %s\n",
		   iRet?"did not succeed":"succeeded", *ppString);
/* Clean credentials to avoid leakage in case of core dump. */
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
	DEFiRet;
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
ENDmodInit
