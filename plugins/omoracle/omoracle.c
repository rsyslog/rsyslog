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
	OCIEnv* environment;
	OCISession* session;
	OCIError* error;
	OCIStmt* statement;
	OCISvcCtx* service;
	OCIAuthInfo* authinfo;
	OCIBind* binding;
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
		dbgprintf ("OCI SUCCESS - With info\n");
		break;
	case OCI_NEED_DATA:
		dbgprintf ("OCI NEEDS MORE DATA\n");
		break;
	case OCI_ERROR:
		dbgprintf ("OCI GENERAL ERROR\n");
		if (handle) {
			OCIErrorGet(handle, 1, NULL, &errcode, buf,
				    sizeof buf, htype);
			dbgprintf ("Error message: %s", buf);
		} else
			dbgprintf ("NULL handle\n"
				"Unable to extract further information");
		break;
	case OCI_INVALID_HANDLE:
		dbgprintf ("OCI INVALID HANDLE\n");
		break;
	case OCI_STILL_EXECUTING:
		dbgprintf ("Still executing...\n");
		break;
	case OCI_CONTINUE:
		dbgprintf ("OCI CONTINUE\n");
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

/** Free any resources allocated by createInstance. */
BEGINfreeInstance
CODESTARTfreeInstance

OCISessionRelease(pData->service, pData->error, NULL, 0, OCI_DEFAULT);
OCIHandleFree(pData->environment, OCI_HTYPE_ENV);
OCIHandleFree(pData->error, OCI_HTYPE_ERROR);
OCIHandleFree(pData->service, OCI_HTYPE_SVCCTX);
OCIHandleFree(pData->authinfo, OCI_HTYPE_AUTHINFO);
OCIHandleFree(pData->statement, OCI_HTYPE_STMT);
dbgprintf ("omoracle freed all its resources\n");

RETiRet;

ENDfreeInstance


BEGINtryResume
CODESTARTtryResume

dbgprintf ("***** OMORACLE ***** At tryResume\n");
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
	RETiRet;
}


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
/* Right now, this module is compatible with nothing. */
dbgprintf ("***** OMORACLE ***** At isCompatibleWithFeature\n");
iRet = RS_RET_INCOMPATIBLE;
ENDisCompatibleWithFeature

BEGINparseSelectorAct

char connection_string[MAXHOSTNAMELEN];
char user[_DB_MAXUNAMELEN];
char pwd[_DB_MAXPWDLEN];


CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1);

if (strncmp((char*) p, ":omoracle:", sizeof ":omoracle:" - 1)) {
	ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
}


p += sizeof ":omoracle:" - 1;

if (*p == '\0' || *p == ',') {
	dbgprintf ("Wrong char processing module arguments: %c\n", *p);
	ABORT_FINALIZE(RS_RET_INVALID_PARAMS);
}

CHKiRet(getSubString(&p, connection_string, MAXHOSTNAMELEN, ','));
CHKiRet(getSubString(&p, user, _DB_MAXUNAMELEN, ','));
CHKiRet(getSubString(&p, pwd, _DB_MAXPWDLEN, ';'));
p--;
CHKiRet(cflineParseTemplateName(&p, *ppOMSR, 0,
				OMSR_RQD_TPL_OPT_SQL, " StdFmt"));
CHKiRet(createInstance(&pData));
CHKiRet(startSession(pData, connection_string, user, pwd));

dbgprintf ("omoracle module got all its resources allocated "
	   "and connected to the DB\n");

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
ENDdoAction

BEGINmodExit
CODESTARTmodExit
ENDmodExit

BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
dbgprintf ("***** OMORACLE ***** At bdgPrintInstInfo\n");

ENDdbgPrintInstInfo


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
dbgprintf ("***** OMORACLE ***** At queryEtryPt\n");

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
char dbname[MAX_BUFSIZE];
CODEmodInit_QueryRegCFSLineHdlr
dbgprintf ("***** OMORACLE ***** At modInit\n");
CHKiRet(objUse(errmsg, CORE_COMPONENT));
/* CHKiRet(omsdRegCFSLineHdlr((uchar*)"actionomoracle",  */
CHKiRet(omsdRegCFSLineHdlr((uchar*) "resetconfigvariables", 1,
			   eCmdHdlrCustomHandler, resetConfigVariables,
			   NULL, STD_LOADABLE_MODULE_ID));
dbgprintf ("***** OMORACLE ***** dbname before = %s\n", dbname);
CHKiRet(omsdRegCFSLineHdlr((uchar*) "actionoracledb", 0, eCmdHdlrInt,
			   NULL, dbname, STD_LOADABLE_MODULE_ID));
dbgprintf ("***** OMORACLE ***** dbname = %s\n", dbname);
ENDmodInit

