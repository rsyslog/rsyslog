/* omtemplate.c
 * This is a template for an output module. It implements a very
 * simple single-threaded output, just as thought of by the output
 * plugin interface.
 *
 * NOTE: read comments in module-template.h for more specifics!
 *
 * File begun on 2009-03-16 by RGerhards
 *
 * Copyright 2009 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Rsyslog is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rsyslog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Rsyslog.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
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
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include "cfsysline.h"

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP

static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal);

/* internal structures
 */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(errmsg)

typedef struct _instanceData {
	/* here you need to define all action-specific data. A record of type 
	 * instanceData will be handed over to each instance of the action. Keep
	 * in mind that there may be several invocations of the same type of action
	 * inside rsyslog.conf, and this is what keeps them apart. Do NOT use
	 * static data for this!
	 */
	unsigned iSrvPort;	/* sample: server port */
} instanceData;

/* config variables
 * For the configuration interface, we need to keep track of some settings. This
 * is done in global variables, inside a struct. It works as follows: when configuration statements
 * are entered, the config file handler (or custom function) sets the global
 * variable here. When the action then actually is instantiated, this handler
 * copies over to instanceData whatever configuration settings (from the global
 * variables) apply. The global variables are NEVER used inside an action
 * instance (at least this is how it is supposed to work ;)
 */
typedef struct configSettings_s {
	int iSrvPort;	/* sample: server port */
} configSettings_t;

SCOPING_SUPPORT; /* must be set AFTER configSettings_t is defined */

BEGINinitConfVars		/* (re)set config variables to default values */
CODESTARTinitConfVars 
	resetConfigVariables(NULL, NULL);
ENDinitConfVars


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


BEGINfreeInstance
CODESTARTfreeInstance
	/* this is a cleanup callback. All dynamically-allocated resources
	 * in instance data must be cleaned up here. Prime examples are
	 * malloc()ed memory, file & database handles and the like.
	 */
ENDfreeInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	/* permits to spit out some debug info */
ENDdbgPrintInstInfo


BEGINtryResume
CODESTARTtryResume
	/* this is called when an action has been suspended and the
	 * rsyslog core tries to resume it. The action must then
	 * retry (if possible) and report RS_RET_OK if it succeeded
	 * or RS_RET_SUSPENDED otherwise.
	 * Note that no data can be written in this callback, as it is
	 * not present. Prime examples of what can be retried are
	 * reconnects to remote hosts, reconnects to database,
	 * opening of files and the like.
	 * If there is no retry-type of operation, the action may
	 * return RS_RET_OK, so that it will get called on its doAction
	 * entry point (where it receives data), retries there, and
	 * immediately returns RS_RET_SUSPENDED if that does not work
	 * out. This disables some optimizations in the core's retry logic,
	 * but is a valid and expected behaviour. Note that it is also OK
	 * for the retry entry point to return OK but the immediately following
	 * doAction call to fail. In real life, for example, a buggy com line
	 * may cause such behaviour.
	 * Note that there is no guarantee that the core will very quickly
	 * call doAction after the retry succeeded. Today, it does, but that may
	 * not always be the case.
	 */
ENDtryResume

BEGINdoAction
CODESTARTdoAction
	/* this is where you receive the message and need to carry out the
	 * action. Data is provided in ppString[i] where 0 <= i <= num of strings
	 * requested.
	 * Return RS_RET_OK if all goes well, RS_RET_SUSPENDED if the action can
	 * currently not complete, or an error code or RS_RET_DISABLED. The later
	 * two should only be returned if there is no hope that the action can be
	 * restored unless an rsyslog restart (prime example is an invalid config).
	 * Error code or RS_RET_DISABLED permanently disables the action, up to
	 * the next restart.
	 */
ENDdoAction


BEGINparseSelectorAct
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	/* first check if this config line is actually for us
	 * This is a clumpsy interface. We receive the action-part of the selector line
	 * and need to look at the first characters. If they match our signature
	 * ":omtemplate:", then we need to instantiate an action. It is recommended that
	 * newer actions just watch for the template and all other parameters are passed in
	 * via $-config-lines, this will hopefully be compatbile with future config syntaxes.
	 * If we do not detect our signature, we must return with RS_RET_CONFLINE_UNPROCESSED
	 * and NOT do anything else.
	 */
	if(strncmp((char*) p, ":omtemplate:", sizeof(":omtemplate:") - 1)) {
		ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
	}

	/* ok, if we reach this point, we have something for us */
	p += sizeof(":omtemplate:") - 1; /* eat indicator sequence  (-1 because of '\0'!) */
	CHKiRet(createInstance(&pData));

	/* check if a non-standard template is to be applied */
	if(*(p-1) == ';')
		--p;
	/* if we have, call rsyslog runtime to get us template. Note that StdFmt below is
	 * the standard name. Currently, we may need to patch tools/syslogd.c if we need
	 * to add a new standard template.
	 */
	CHKiRet(cflineParseTemplateName(&p, *ppOMSR, 0, OMSR_RQD_TPL_OPT_SQL, (uchar*) " StdFmt"));
	
	/* if we reach this point, all went well, and we can copy over to instanceData
	 * those configuration elements that we need.
	 */
	pData->iSrvPort = (unsigned) cs.iSrvPort;	/* set configured port */

CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINmodExit
CODESTARTmodExit
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
ENDqueryEtryPt


/* Reset config variables for this module to default values.
 */
static rsRetVal
resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	DEFiRet;
	cs.iSrvPort = 0; /* zero is the default port */
	RETiRet;
}


BEGINmodInit()
CODESTARTmodInit
SCOPINGmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	/* register our config handlers */
	/* confguration parameters MUST always be specified in lower case! */
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"actionomtemplteserverport", 0, eCmdHdlrInt, NULL, &cs.iSrvPort, STD_LOADABLE_MODULE_ID));
	/* "resetconfigvariables" should be provided. Notat that it is a chained directive */
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit

/* vi:set ai:
 */
