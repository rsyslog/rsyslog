/** omoracle.c

    This is an output module feeding directly to an Oracle
    database. It uses Oracle Call Interface, a propietary module
    provided by Oracle.

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
	OCIServer* server;
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

dbgprintf ("***** OMORACLE ***** Creating instance\n");
CHECKENV(pData->environment,
	 OCIEnvCreate((void*) &(pData->environment), OCI_DEFAULT,
		      NULL, NULL, NULL, NULL, 0, NULL));
dbgprintf ("***** OMORACLE ***** Created environment\n");
CHECKENV(pData->environment,
	 OCIHandleAlloc(pData->environment, (void*) &(pData->error),
			OCI_HTYPE_ERROR, 0, NULL));
dbgprintf ("***** OMORACLE ***** Created error\n");
CHECKENV(pData->environment,
	 OCIHandleAlloc(pData->environment, (void*) &(pData->server),
			OCI_HTYPE_SERVER, 0, NULL));
dbgprintf ("***** OMORACLE ***** Created server\n");
CHECKENV(pData->environment,
	 OCIHandleAlloc(pData->environment, (void*) &(pData->service),
			OCI_HTYPE_SVCCTX, 0, NULL));
dbgprintf ("***** OMORACLE ***** Created service\n");
CHECKENV(pData->environment,
	 OCIHandleAlloc(pData->environment, (void*) &(pData->authinfo),
			OCI_HTYPE_AUTHINFO, 0, NULL));
dbgprintf ("***** OMORACLE ***** Created authinfo\n");
finalize_it:
ENDcreateInstance

/** Free any resources allocated by createInstance. */
BEGINfreeInstance
CODESTARTfreeInstance

dbgprintf ("***** OMORACLE ***** Destroying instance\n");

OCIHandleFree(pData->environment, OCI_HTYPE_ENV);
dbgprintf ("***** OMORACLE ***** Destroyed environment\n");
OCIHandleFree(pData->error, OCI_HTYPE_ERROR);
dbgprintf ("***** OMORACLE ***** Destroyed error\n");
OCIHandleFree(pData->server, OCI_HTYPE_SERVER);
dbgprintf ("***** OMORACLE ***** Destroyed server\n");
OCIHandleFree(pData->service, OCI_HTYPE_SVCCTX);
dbgprintf ("***** OMORACLE ***** Destroyed service\n");
OCIHandleFree(pData->authinfo, OCI_HTYPE_AUTHINFO);
dbgprintf ("***** OMORACLE ***** Destroyed authinfo\n");

RETiRet;

ENDfreeInstance


BEGINtryResume
CODESTARTtryResume

dbgprintf ("***** OMORACLE ***** At tryResume\n");
ENDtryResume

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

CHKiRet(createInstance(&pData));

p += sizeof ":omoracle:" - 1;
if (*p != ';') {
	dbgprintf ("***** OMORACLE ***** Wrong char: %c\n", *p);
	ABORT_FINALIZE(RS_RET_INVALID_PARAMS);
}
p++;

CHKiRet(cflineParseTemplateName(&p, *ppOMSR, 0,
				OMSR_RQD_TPL_OPT_SQL, " StdFmt"));

dbgprintf ("***** OMORACLE ***** Salido\n");


CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct

BEGINdoAction
CODESTARTdoAction
dbgprintf ("***** OMORACLE ***** At doAction\n");
ENDdoAction

BEGINmodExit
CODESTARTmodExit
dbgprintf ("***** OMORACLE ***** At modExit\n");
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
CODEmodInit_QueryRegCFSLineHdlr
CHKiRet(objUse(errmsg, CORE_COMPONENT));
/* CHKiRet(omsdRegCFSLineHdlr((uchar*)"actionomoracle",  */
CHKiRet(omsdRegCFSLineHdlr((uchar*) "resetconfigvariables", 1,
			   eCmdHdlrCustomHandler, resetConfigVariables,
			   NULL, STD_LOADABLE_MODULE_ID));
			   
dbgprintf ("***** OMORACLE ***** At modInit\n");
ENDmodInit
