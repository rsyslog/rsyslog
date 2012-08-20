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

    $OmoracleBatchSize: Number of elements to send to the DB on each
    transaction.

    $OmoracleBatchItemSize: Number of characters each property may
    have. Make it as big as the longest value you expect for *any*
    property in the sentence. For instance, if you expect 5 arguments
    to the statement, 4 have 10 bytes and the 5th may be up to 3KB,
    then specify $OmoracleBatchItemSize 3072. Please, remember to
    leave space to the trailing \0!!

    $OmoracleStatementTemplate: Name of the template containing the
    statement to be prepared and executed in batches. Please note that
    Oracle's prepared statements have their placeholders as
    ':identifier', and this module uses the colon to guess how many
    placeholders there will be.

    All these directives are mandatory. The dbstring can be an Oracle
    easystring or a DB name, as present in the tnsnames.ora file.

    The form of the template is just a list of strings you want
    inserted to the DB, for instance:

    $template TestStmt,"%hostname%%msg%"

    Will provide the arguments to a statement like

    $OmoracleStatement \
        insert into foo(hostname,message)values(:host,:message)

    Also note that identifiers to placeholders are arbitrary. You need
    to define the properties on the template in the correct order you
    want them passed to the statement!

    This file is licensed under the terms of the GPL version 3 or, at
    your choice, any later version. Exceptionally (perhaps), you are
    allowed to link to the Oracle Call Interface in your derived work

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
#include <ctype.h>
#include "dirty.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include "cfsysline.h"
#include "omoracle.h"

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("omoracle")

/**  */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(errmsg)

/** Structure defining a batch of items to be sent to the database in
 * the same statement execution. */
struct oracle_batch
{
	/* Batch size */
	int size;
	/* Last element inserted in the buffer. The batch will be
	 * executed when n == size */
	int n;
	/* Number of arguments the statement takes */
	int arguments;
	/** Maximum size of each parameter */
	int param_size;
	/* Parameters to pass to the statement on this transaction */
	char*** parameters;
	/* Binding parameters */
	OCIBind** bindings;
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
	/* Connection string, kept here for possible retries. */
	char* connection;
	/* Statement to be prepared. */
	char* txt_statement;
	/* Batch */
	struct oracle_batch batch;
} instanceData;

/* To be honest, strlcpy is faster than strncpy and makes very easy to
 * detect if a message has been truncated. */
#ifndef strlcpy
#define strlcpy(dst,src,sz) snprintf((dst), (sz), "%s", (src))
#endif


/** Database name, to be filled by the $OmoracleDB directive */
static char* db_name;
/** Database user name, to be filled by the $OmoracleDBUser
 * directive */
static char* db_user;
/** Database password, to be filled by the $OmoracleDBPassword */
static char* db_password;
/** Batch size. */
static int batch_size;
/** Size of each element in the batch. */
static int batch_item_size;
/** Statement to prepare and execute */
static char* db_statement;

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
		OCIErrorGet(handle, 1, NULL, &errcode, buf, sizeof buf, htype);
		errmsg.LogError(0, NO_ERRCODE, "OCI SUCCESS - With info: %s",
				buf);
                return OCI_SUCCESS_WITH_INFO;
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
		/* In this case we may have to trigger a call to
		 * tryResume(). */
		return RS_RET_SUSPENDED;
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

/** Callback for OCIBindDynamic.
 *
 * OCI doesn't insert an array of char* by itself (although it can
 * handle arrays of int), so we must either run in batches of size one
 * (no way) or bind all parameters with OCI_DATA_AT_EXEC instead of
 * OCI_DEFAULT, and then give this function as an argument to
 * OCIBindDynamic so that it is able to handle all strings in a single
 * server trip.
 *
 * See the documentation of OCIBindDynamic
 * (http://download.oracle.com/docs/cd/B28359_01/appdev.111/b28395/oci16rel003.htm#i444015)
 * for more details.
 */
static int bind_dynamic (char** in, OCIBind __attribute__((unused))* bind,
			 int  iter, int __attribute__((unused))  idx,
			 char** out, int* buflen, unsigned char* piece,
			 void** bd)
{
	*out = in[iter];
	*buflen = strlen(*out) + 1;
	dbgprintf ("omoracle bound line %d, length %d: %s\n", iter, *buflen,
		   *out);
	*piece = OCI_ONE_PIECE;
	*bd = NULL;
	return OCI_CONTINUE;
}


/** Returns the number of bind parameters for the statement given as
 * an argument. It counts the number of appearances of ':', as in
 *
 * insert into foo(bar, baz) values(:bar, :baz)
 *
 * while taking in account that string literals must not be parsed. */
static int count_bind_parameters(char* p)
{
	int n = 0;
	int enable = 1;

	for (; *p; p++)
		if (enable && *p == BIND_MARK )
			n++;
		else if (*p == '\'')
			enable ^= 1;
	dbgprintf ("omoracle statement has %d parameters\n", n);
	return n;
}

/** Prepares the statement, binding all its positional parameters */
static int prepare_statement(instanceData* pData)
{
	int i;
	DEFiRet;

	CHECKERR(pData->error,
		 OCIStmtPrepare(pData->statement,
				pData->error,
				pData->txt_statement,
				strlen(pData->txt_statement),
				OCI_NTV_SYNTAX, OCI_DEFAULT));
	for (i = 0; i < pData->batch.arguments; i++) {
		CHECKERR(pData->error,
			 OCIBindByPos(pData->statement,
				      pData->batch.bindings+i,
				      pData->error, i+1, NULL,
				      pData->batch.param_size,
				      SQLT_STR, NULL, NULL, NULL,
				      0, 0,  OCI_DATA_AT_EXEC));
		CHECKERR(pData->error,
			 OCIBindDynamic(pData->batch.bindings[i],
					pData->error,
					pData->batch.parameters[i],
					bind_dynamic, NULL, NULL));
	}

finalize_it:
	RETiRet;
}


/* Resource allocation */
BEGINcreateInstance
	int i, j;
	struct template* tpl;
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
	tpl = tplFind(db_statement, strlen(db_statement));
	pData->txt_statement = strdup(tpl->pEntryRoot->data.constant.pConstant);
	CHKmalloc(pData->txt_statement);
	dbgprintf("omoracle will run stored statement: %s\n",
		  pData->txt_statement);

	pData->batch.n = 0;
	pData->batch.size = batch_size;
	pData->batch.param_size = batch_item_size *
		sizeof ***pData->batch.parameters;
	pData->batch.arguments = count_bind_parameters(pData->txt_statement);

	/* I know, this can be done with a single malloc() call but this is
	 * easier to read. :) */
	pData->batch.parameters = calloc(pData->batch.arguments,
					 sizeof *pData->batch.parameters);
	CHKmalloc(pData->batch.parameters);
	for (i = 0; i < pData->batch.arguments; i++) {
		pData->batch.parameters[i] = calloc(pData->batch.size,
						    sizeof **pData->batch.parameters);
		CHKmalloc(pData->batch.parameters[i]);
		for (j = 0; j < pData->batch.size; j++) {
			/* Each entry has at most
			 * pData->batch.param_size bytes because OCI
			 * doesn't like null-terminated strings when
			 * operating with batches, and the maximum
			 * size of each entry must be provided when
			 * binding parameters. pData->batch.param_size
			 * is long enough for usual entries. */
			pData->batch.parameters[i][j] = malloc(pData->batch.param_size);
			CHKmalloc(pData->batch.parameters[i][j]);
		}
	}

	pData->batch.bindings = calloc(pData->batch.arguments,
				       sizeof *pData->batch.bindings);
	CHKmalloc(pData->batch.bindings);

finalize_it:
ENDcreateInstance

/* Analyses the errors during a batch statement execution, and logs
 * all the corresponding ORA-MESSAGES, together with some useful
 * information. */
static void log_detailed_err(instanceData* pData)
{
       DEFiRet;
       int errs, i, row, code, j;
       OCIError *er = NULL, *er2 = NULL;
       unsigned char buf[MAX_BUFSIZE];

       OCIAttrGet(pData->statement, OCI_HTYPE_STMT, &errs, 0,
                  OCI_ATTR_NUM_DML_ERRORS, pData->error);
       errmsg.LogError(0, NO_ERRCODE, "OCI: %d errors in execution  of "
                       "statement: %s", errs, pData->txt_statement);

       CHECKENV(pData->environment,
                OCIHandleAlloc(pData->environment, &er, OCI_HTYPE_ERROR,
                               0, NULL));
       CHECKENV(pData->environment,
                OCIHandleAlloc(pData->environment, &er2, OCI_HTYPE_ERROR,
                               0, NULL));

       for (i = 0; i < errs; i++) {
               OCIParamGet(pData->error, OCI_HTYPE_ERROR,
                           er2, &er, i);
               OCIAttrGet(er, OCI_HTYPE_ERROR, &row, 0,
                          OCI_ATTR_DML_ROW_OFFSET, er2);
               errmsg.LogError(0, NO_ERRCODE, "OCI failure in row %d:", row);
               for (j = 0; j < pData->batch.arguments; j++)
                       errmsg.LogError(0, NO_ERRCODE, "%s",
                                       pData->batch.parameters[j][row]);
               OCIErrorGet(er, 1, NULL, &code, buf, sizeof buf,
                           OCI_HTYPE_ERROR);
               errmsg.LogError(0, NO_ERRCODE, "FAILURE DETAILS: %s", buf);
       }

finalize_it:
       OCIHandleFree(er, OCI_HTYPE_ERROR);
       OCIHandleFree(er2, OCI_HTYPE_ERROR);
}


/* Inserts all stored statements into the database, releasing any
 * allocated memory. */
static int insert_to_db(instanceData* pData)
{
	DEFiRet;

	CHECKERR(pData->error,
		 OCIStmtExecute(pData->service,
				pData->statement,
				pData->error,
				pData->batch.n, 0, NULL, NULL,
				OCI_BATCH_ERRORS));

finalize_it:
        if (iRet == OCI_SUCCESS_WITH_INFO) {
                log_detailed_err(pData);
                iRet = RS_RET_OK;
        }
	pData->batch.n = 0;
	OCITransCommit(pData->service, pData->error, 0);
	dbgprintf ("omoracle insertion to DB %s\n", iRet == RS_RET_OK ?
		   "succeeded" : "did not succeed");
	RETiRet;
}

/** Close the session and free anything allocated by
    createInstance. */
BEGINfreeInstance
	int i, j;
CODESTARTfreeInstance

/* Before actually releasing our resources, let's try to commit
 * anything pending so that we don't lose any messages. */
	insert_to_db(pData);
	OCISessionRelease(pData->service, pData->error, NULL, 0, OCI_DEFAULT);
	OCIHandleFree(pData->environment, OCI_HTYPE_ENV);
	OCIHandleFree(pData->error, OCI_HTYPE_ERROR);
	OCIHandleFree(pData->service, OCI_HTYPE_SVCCTX);
	OCIHandleFree(pData->authinfo, OCI_HTYPE_AUTHINFO);
	OCIHandleFree(pData->statement, OCI_HTYPE_STMT);
	free(pData->connection);
	free(pData->txt_statement);
	for (i = 0; i < pData->batch.arguments; i++) {
		for (j = 0; j < pData->batch.size; j++)
			free(pData->batch.parameters[i][j]);
		free(pData->batch.parameters[i]);
	}
	free(pData->batch.parameters);
	free(pData->batch.bindings);
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
	dbgprintf("omoracle attempting to reconnect to DB server\n");
	OCISessionRelease(pData->service, pData->error, NULL, 0, OCI_DEFAULT);
	OCIHandleFree(pData->service, OCI_HTYPE_SVCCTX);
	CHECKERR(pData->error, OCISessionGet(pData->environment, pData->error,
					     &pData->service, pData->authinfo,
					     pData->connection,
					     strlen(pData->connection), NULL, 0,
					     NULL, NULL, NULL, OCI_DEFAULT));
	CHKiRet(prepare_statement(pData));

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
					OMSR_TPL_AS_ARRAY, " StdFmt"));
	CHKiRet(createInstance(&pData));
	CHKmalloc(pData->connection = strdup(db_name));
	CHKiRet(startSession(pData, db_name, db_user, db_password));

	CHKiRet(prepare_statement(pData));

	dbgprintf ("omoracle module got all its resources allocated "
		   "and connected to the DB\n");
	
CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct

BEGINdoAction
	int i, sz;
	char **params = (char**) ppString[0];
CODESTARTdoAction

	if (pData->batch.n == pData->batch.size) {
		dbgprintf("omoracle batch size limit hit, sending into DB\n");
		CHKiRet(insert_to_db(pData));
	}

	for (i = 0; i < pData->batch.arguments && params[i]; i++) {
		dbgprintf("batch[%d][%d]=%s\n", i, pData->batch.n, params[i]);
		sz = strlcpy(pData->batch.parameters[i][pData->batch.n],
			     params[i], pData->batch.param_size);
		if (sz >= pData->batch.param_size)
			errmsg.LogError(0, NO_ERRCODE,
					"Possibly truncated %d column of '%s' "
					"statement: %s", i,
					pData->txt_statement, params[i]);
	}
	pData->batch.n++;

finalize_it:
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
	if (db_statement != NULL)
		free(db_statement);
	db_name = db_user = db_password = db_statement = NULL;
	batch_size = batch_item_size = 0;
	RETiRet;
}

BEGINmodInit()
	rsRetVal (*supported_options)(unsigned long *pOpts);
	unsigned long opts;
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION;
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
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
	CHKiRet(pHostQueryEtryPt((uchar*)"OMSRgetSupportedTplOpts", &supported_options));
	CHKiRet((*supported_options)(&opts));
	if (!(opts & OMSR_TPL_AS_ARRAY))
		ABORT_FINALIZE(RS_RET_RSCORE_TOO_OLD);

	CHKiRet(omsdRegCFSLineHdlr((uchar*) "omoraclestatementtemplate", 0,
				   eCmdHdlrGetWord, NULL,
				   &db_statement, STD_LOADABLE_MODULE_ID));

	CHKiRet(omsdRegCFSLineHdlr((uchar*) "omoraclebatchitemsize", 0,
				   eCmdHdlrInt, NULL,
				   &batch_item_size, STD_LOADABLE_MODULE_ID));

ENDmodInit
