/* omuxsock.c
 * This is the implementation of datgram unix domain socket forwarding.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * Copyright 2010-2012 Adiscon GmbH.
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
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include "conf.h"
#include "srUtils.h"
#include "template.h"
#include "msg.h"
#include "cfsysline.h"
#include "module-template.h"
#include "glbl.h"
#include "errmsg.h"
#include "unicode-helper.h"

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("omuxsock")

/* internal structures
 */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(errmsg)
DEFobjCurrIf(glbl)

#define INVLD_SOCK -1

typedef struct _instanceData {
	permittedPeers_t *pPermPeers;
	uchar *sockName;
	int sock;
	int bIsConnected;  /* are we connected to remote host? 0 - no, 1 - yes, UDP means addr resolved */
	struct sockaddr_un addr;
} instanceData;

/* config data */
typedef struct configSettings_s {
	uchar *tplName; /* name of the default template to use */
	uchar *sockName; /* name of the default template to use */
} configSettings_t;
static configSettings_t cs;

/* module-global parameters */
static struct cnfparamdescr modpdescr[] = {
	{ "template", eCmdHdlrGetWord, 0 },
};
static struct cnfparamblk modpblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(modpdescr)/sizeof(struct cnfparamdescr),
	  modpdescr
	};

struct modConfData_s {
	rsconf_t *pConf;	/* our overall config object */
	uchar 	*tplName;	/* default template */
};

static modConfData_t *loadModConf = NULL;/* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL;/* modConf ptr to use for the current exec process */



BEGINinitConfVars		/* (re)set config variables to default values */
CODESTARTinitConfVars 
	cs.tplName = NULL;
	cs.sockName = NULL;
ENDinitConfVars


static rsRetVal doTryResume(instanceData *pData);


/* this function gets the default template. It coordinates action between
 * old-style and new-style configuration parts.
 */
static inline uchar*
getDfltTpl(void)
{
	if(loadModConf != NULL && loadModConf->tplName != NULL)
		return loadModConf->tplName;
	else if(cs.tplName == NULL)
		return (uchar*)"RSYSLOG_TraditionalForwardFormat";
	else
		return cs.tplName;
}

/* set the default template to be used
 * This is a module-global parameter, and as such needs special handling. It needs to
 * be coordinated with values set via the v2 config system (rsyslog v6+). What we do
 * is we do not permit this directive after the v2 config system has been used to set
 * the parameter.
 */
rsRetVal
setLegacyDfltTpl(void __attribute__((unused)) *pVal, uchar* newVal)
{
	DEFiRet;

	if(loadModConf != NULL && loadModConf->tplName != NULL) {
		free(newVal);
		errmsg.LogError(0, RS_RET_ERR, "omuxsock default template already set via module "
			"global parameter - can no longer be changed");
		ABORT_FINALIZE(RS_RET_ERR);
	}
	free(cs.tplName);
	cs.tplName = newVal;
finalize_it:
	RETiRet;
}


static inline rsRetVal
closeSocket(instanceData *pData)
{
	DEFiRet;
	if(pData->sock != INVLD_SOCK) {
		close(pData->sock);
		pData->sock = INVLD_SOCK;
	}
pData->bIsConnected = 0; // TODO: remove this variable altogether
	RETiRet;
}




BEGINbeginCnfLoad
CODESTARTbeginCnfLoad
	loadModConf = pModConf;
	pModConf->pConf = pConf;
	pModConf->tplName = NULL;
ENDbeginCnfLoad

BEGINsetModCnf
	struct cnfparamvals *pvals = NULL;
	int i;
CODESTARTsetModCnf
	pvals = nvlstGetParams(lst, &modpblk, NULL);
	if(pvals == NULL) {
		errmsg.LogError(0, RS_RET_MISSING_CNFPARAMS, "error processing module "
				"config parameters [module(...)]");
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	if(Debug) {
		dbgprintf("module (global) param blk for omuxsock:\n");
		cnfparamsPrint(&modpblk, pvals);
	}

	for(i = 0 ; i < modpblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(modpblk.descr[i].name, "template")) {
			loadModConf->tplName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
			if(cs.tplName != NULL) {
				errmsg.LogError(0, RS_RET_DUP_PARAM, "omuxsock: warning: default template "
						"was already set via legacy directive - may lead to inconsistent "
						"results.");
			}
		} else {
			dbgprintf("omuxsock: program error, non-handled "
			  "param '%s' in beginCnfLoad\n", modpblk.descr[i].name);
		}
	}
finalize_it:
	if(pvals != NULL)
		cnfparamvalsDestruct(pvals, &modpblk);
ENDsetModCnf

BEGINendCnfLoad
CODESTARTendCnfLoad
	loadModConf = NULL; /* done loading */
	/* free legacy config vars */
	free(cs.tplName);
	cs.tplName = NULL;
ENDendCnfLoad

BEGINcheckCnf
CODESTARTcheckCnf
ENDcheckCnf

BEGINactivateCnf
CODESTARTactivateCnf
	runModConf = pModConf;
ENDactivateCnf

BEGINfreeCnf
CODESTARTfreeCnf
	free(pModConf->tplName);
ENDfreeCnf

BEGINcreateInstance
CODESTARTcreateInstance
	pData->sock = INVLD_SOCK;
ENDcreateInstance


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURERepeatedMsgReduction)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINfreeInstance
CODESTARTfreeInstance
	/* final cleanup */
	closeSocket(pData);
	free(pData->sockName);
ENDfreeInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	DBGPRINTF("%s", pData->sockName);
ENDdbgPrintInstInfo


/* Send a message via UDP
 * rgehards, 2007-12-20
 */
static rsRetVal sendMsg(instanceData *pData, char *msg, size_t len)
{
	DEFiRet;
	unsigned lenSent = 0;

	if(pData->sock == INVLD_SOCK) {
		CHKiRet(doTryResume(pData));
	}

	if(pData->sock != INVLD_SOCK) {
		/* we need to track if we have success sending to the remote
		 * peer. Success is indicated by at least one sendto() call
		 * succeeding. We track this be bSendSuccess. We can not simply
		 * rely on lsent, as a call might initially work, but a later
		 * call fails. Then, lsent has the error status, even though
		 * the sendto() succeeded. -- rgerhards, 2007-06-22
		 */
		lenSent = sendto(pData->sock, msg, len, 0, &pData->addr, sizeof(pData->addr));
		if(lenSent == len) {
			int eno = errno;
			char errStr[1024];
			DBGPRINTF("omuxsock suspending: sendto(), socket %d, error: %d = %s.\n",
				pData->sock, eno, rs_strerror_r(eno, errStr, sizeof(errStr)));
		}
	}

finalize_it:
	RETiRet;
}


/* open socket to remote system
 */
static inline rsRetVal
openSocket(instanceData *pData)
{
	DEFiRet;
	assert(pData->sock == INVLD_SOCK);

	if((pData->sock = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
		char errStr[1024];
		int eno = errno;
		DBGPRINTF("error %d creating AF_UNIX/SOCK_DGRAM: %s.\n",
			eno, rs_strerror_r(eno, errStr, sizeof(errStr)));
		pData->sock = INVLD_SOCK;
		ABORT_FINALIZE(RS_RET_NO_SOCKET);
		
	}

	/* set up server address structure */
	memset(&pData->addr, 0, sizeof(pData->addr));
        pData->addr.sun_family = AF_UNIX;
        strcpy(pData->addr.sun_path, (char*)pData->sockName);

finalize_it:
	if(iRet != RS_RET_OK) {
		closeSocket(pData);
	}
	RETiRet;
}



/* try to resume connection if it is not ready
 */
static rsRetVal doTryResume(instanceData *pData)
{
	DEFiRet;

	DBGPRINTF("omuxsock trying to resume\n");
	closeSocket(pData);
	iRet = openSocket(pData);

	if(iRet != RS_RET_OK) {
		iRet = RS_RET_SUSPENDED;
	}

	RETiRet;
}


BEGINtryResume
CODESTARTtryResume
	iRet = doTryResume(pData);
ENDtryResume

BEGINdoAction
	char *psz = NULL; /* temporary buffering */
	register unsigned l;
	int iMaxLine;
CODESTARTdoAction
	CHKiRet(doTryResume(pData));

	iMaxLine = glbl.GetMaxLine();

	DBGPRINTF(" omuxsock:%s\n", pData->sockName);

	psz = (char*) ppString[0];
	l = strlen((char*) psz);
	if((int) l > iMaxLine)
		l = iMaxLine;

	CHKiRet(sendMsg(pData, psz, l));

finalize_it:
ENDdoAction


BEGINparseSelectorAct
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)

	/* first check if this config line is actually for us */
	if(strncmp((char*) p, ":omuxsock:", sizeof(":omuxsock:") - 1)) {
		ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
	}

	/* ok, if we reach this point, we have something for us */
	p += sizeof(":omuxsock:") - 1; /* eat indicator sequence  (-1 because of '\0'!) */
	CHKiRet(createInstance(&pData));

	/* check if a non-standard template is to be applied */
	if(*(p-1) == ';')
		--p;
	CHKiRet(cflineParseTemplateName(&p, *ppOMSR, 0, 0, getDfltTpl()));
	
	if(cs.sockName == NULL) {
		errmsg.LogError(0, RS_RET_NO_SOCK_CONFIGURED, "No output socket configured for omuxsock\n");
		ABORT_FINALIZE(RS_RET_NO_SOCK_CONFIGURED);
	}

	pData->sockName = cs.sockName;
	cs.sockName = NULL; /* pData is now owner and will fee it */

CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


/* a common function to free our configuration variables - used both on exit
 * and on $ResetConfig processing. -- rgerhards, 2008-05-16
 */
static inline void
freeConfigVars(void)
{
	free(cs.tplName);
	cs.tplName = NULL;
	free(cs.sockName);
	cs.sockName = NULL;
}


BEGINmodExit
CODESTARTmodExit
	/* release what we no longer need */
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(glbl, CORE_COMPONENT);

	freeConfigVars();
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
CODEqueryEtryPt_STD_CONF2_QUERIES
CODEqueryEtryPt_STD_CONF2_setModCnf_QUERIES
ENDqueryEtryPt


/* Reset config variables for this module to default values.
 * rgerhards, 2008-03-28
 */
static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	freeConfigVars();
	return RS_RET_OK;
}


BEGINmodInit()
CODESTARTmodInit
INITLegCnfVars
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(errmsg, CORE_COMPONENT));

	CHKiRet(regCfSysLineHdlr((uchar *)"omuxsockdefaulttemplate", 0, eCmdHdlrGetWord, setLegacyDfltTpl, NULL, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"omuxsocksocket", 0, eCmdHdlrGetWord, NULL, &cs.sockName, NULL));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit

/* vim:set ai:
 */
