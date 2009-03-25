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
		printf ("OCI SUCCESS - With info\n");
		break;
	case OCI_NEED_DATA:
		printf ("OCI NEEDS MORE DATA\n");
		break;
	case OCI_ERROR:
		printf ("OCI GENERAL ERROR\n");
		if (handle) {
			OCIErrorGet(handle, 1, NULL, &errcode, buf,
				    sizeof buf, htype);
			printf ("Error message: %s", buf);
		} else
			printf ("NULL handle\n"
				"Unable to extract further information");
		break;
	case OCI_INVALID_HANDLE:
		printf ("OCI INVALID HANDLE\n");
		break;
	case OCI_STILL_EXECUTING:
		printf ("Still executing...\n");
		break;
	case OCI_CONTINUE:
		printf ("OCI CONTINUE\n");
		break;
	}
	return OCI_ERROR;
}


/* Resource allocation */
BEGINcreateInstance
CODESTARTcreateInstance
CHECKENV(pData->environment,
	 OCIEnvCreate((void*) &(pData->environment), OCI_DEFAULT,
		      NULL, NULL, NULL, NULL, 0, NULL));
CHECKENV(pData->environment,
	 OCIHandleAlloc(pData->environment, (void*) &(pData->error),
			OCI_HTYPE_ERROR, 0, NULL));
CHECKENV(pData->environment,
	 OCIHandleAlloc(pData->environment, (void*) &(pData->server),
			OCI_HTYPE_SERVER, 0, NULL));
CHECKENV(pData->environment,
	 OCIHandleAlloc(pData->environment, (void*) &(pData->service),
			OCI_HTYPE_SVCCTX, 0, NULL));
CHECKENV(pData->environment,
	 OCIHandleAlloc(pData->environment, (void*) &(pData->authinfo),
			OCI_HTYPE_AUTHINFO, 0, NULL));
finalize_it:
ENDcreateInstance

/** Free any resources allocated by createInstance. */
BEGINfreeInstance
CODESTARTfreeInstance

OCIHandleFree(pData->environment, OCI_HTYPE_ENV);
OCIHandleFree(pData->error, OCI_HTYPE_ERROR);
OCIHandleFree(pData->server, OCI_HTYPE_SERVER);
OCIHandleFree(pData->service, OCI_HTYPE_SVCCTX);
OCIHandleFree(pData->authinfo, OCI_HTYPE_AUTHINFO);

RETiRet;

ENDfreeInstance


BEGINtryResume
CODESTARTtryResume
ENDtryResume

BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
/* Right now, this module is compatible with nothing. */
ENDisCompatibleWithFeature

BEGINparseSelectorAct
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1);
CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct

BEGINdoAction
CODESTARTdoAction
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

BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION;
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
ENDmodInit
