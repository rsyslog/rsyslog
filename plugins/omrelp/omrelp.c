/* omrelp.c
 *
 * This is the implementation of the RELP output module.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * File begun on 2008-03-13 by RGerhards
 *
 * Copyright 2008-2013 Adiscon GmbH.
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
#include <errno.h>
#include <ctype.h>
#include <librelp.h>
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "cfsysline.h"
#include "module-template.h"
#include "glbl.h"
#include "errmsg.h"
#include "debug.h"
#include "unicode-helper.h"

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("omrelp")

/* internal structures
 */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(errmsg)
DEFobjCurrIf(glbl)

#define DFLT_ENABLE_TLS 0
#define DFLT_ENABLE_TLSZIP 0

static relpEngine_t *pRelpEngine;	/* our relp engine */

typedef struct _instanceData {
	uchar *target;
	uchar *port;
	int bInitialConnect; /* is this the initial connection request of our module? (0-no, 1-yes) */
	int bIsConnected; /* currently connected to server? 0 - no, 1 - yes */
	unsigned timeout;
	unsigned rebindInterval;
	unsigned nSent;
	relpClt_t *pRelpClt; /* relp client for this instance */
	sbool bEnableTLS;
	sbool bEnableTLSZip;
	uchar *pristring;		/* GnuTLS priority string (NULL if not to be provided) */
	uchar *caCertFile;
	uchar *myCertFile;
	uchar *myPrivKeyFile;
	uchar *tplName;
	struct {
		int nmemb;
		uchar **name;
	} permittedPeers;
} instanceData;

typedef struct configSettings_s {
	EMPTY_STRUCT
} configSettings_t;
static configSettings_t __attribute__((unused)) cs;


/* tables for interfacing with the v6 config system */
/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {
	{ "target", eCmdHdlrGetWord, 1 },
	{ "tls", eCmdHdlrBinary, 0 },
	{ "tls.compression", eCmdHdlrBinary, 0 },
	{ "tls.prioritystring", eCmdHdlrString, 0 },
	{ "tls.cacert", eCmdHdlrString, 0 },
	{ "tls.mycert", eCmdHdlrString, 0 },
	{ "tls.myprivkey", eCmdHdlrString, 0 },
	{ "tls.permittedpeer", eCmdHdlrArray, 0 },
	{ "port", eCmdHdlrGetWord, 0 },
	{ "rebindinterval", eCmdHdlrInt, 0 },
	{ "timeout", eCmdHdlrInt, 0 },
	{ "template", eCmdHdlrGetWord, 1 }
};
static struct cnfparamblk actpblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(actpdescr)/sizeof(struct cnfparamdescr),
	  actpdescr
	};

BEGINinitConfVars		/* (re)set config variables to default values */
CODESTARTinitConfVars 
ENDinitConfVars

/* We may change the implementation to try to lookup the port
 * if it is unspecified. So far, we use 514 as default (what probably
 * is not a really bright idea, but kept for backward compatibility).
 */
static uchar *getRelpPt(instanceData *pData)
{
	assert(pData != NULL);
	if(pData->port == NULL)
		return((uchar*)"514");
	else
		return(pData->port);
}

static void
onAuthErr(void *pUsr, char *authinfo, char* errmesg, __attribute__((unused)) relpRetVal errcode)
{
	instanceData *pData = (instanceData*) pUsr;
	errmsg.LogError(0, RS_RET_RELP_AUTH_FAIL, "omrelp[%s]: authentication error '%s', peer "
			"is '%s'", pData->port, errmesg, authinfo);
}

static inline rsRetVal
doCreateRelpClient(instanceData *pData)
{
	int i;
	DEFiRet;
	if(relpEngineCltConstruct(pRelpEngine, &pData->pRelpClt) != RELP_RET_OK)
		ABORT_FINALIZE(RS_RET_RELP_ERR);
	if(relpCltSetTimeout(pData->pRelpClt, pData->timeout) != RELP_RET_OK)
		ABORT_FINALIZE(RS_RET_RELP_ERR);
	if(relpCltSetUsrPtr(pData->pRelpClt, pData) != RELP_RET_OK)
		ABORT_FINALIZE(RS_RET_RELP_ERR);
	if(pData->bEnableTLS) {
		if(relpCltEnableTLS(pData->pRelpClt) != RELP_RET_OK)
			ABORT_FINALIZE(RS_RET_RELP_ERR);
		if(pData->bEnableTLSZip) {
			if(relpCltEnableTLSZip(pData->pRelpClt) != RELP_RET_OK)
				ABORT_FINALIZE(RS_RET_RELP_ERR);
		}
		if(relpCltSetGnuTLSPriString(pData->pRelpClt, (char*) pData->pristring) != RELP_RET_OK)
			ABORT_FINALIZE(RS_RET_RELP_ERR);
		if(relpCltSetCACert(pData->pRelpClt, (char*) pData->caCertFile) != RELP_RET_OK)
			ABORT_FINALIZE(RS_RET_RELP_ERR);
		if(relpCltSetOwnCert(pData->pRelpClt, (char*) pData->myCertFile) != RELP_RET_OK)
			ABORT_FINALIZE(RS_RET_RELP_ERR);
		if(relpCltSetPrivKey(pData->pRelpClt, (char*) pData->myPrivKeyFile) != RELP_RET_OK)
			ABORT_FINALIZE(RS_RET_RELP_ERR);
		for(i = 0 ; i <  pData->permittedPeers.nmemb ; ++i) {
dbgprintf("DDDD: omrelp add permitted peer %s\n",(char*)pData->permittedPeers.name[i]);
			relpCltAddPermittedPeer(pData->pRelpClt, (char*)pData->permittedPeers.name[i]);
		}
	}
	if(glbl.GetSourceIPofLocalClient() == NULL) {	/* ar Do we have a client IP set? */
		if(relpCltSetClientIP(pData->pRelpClt, glbl.GetSourceIPofLocalClient()) != RELP_RET_OK)
			ABORT_FINALIZE(RS_RET_RELP_ERR);
	}
	pData->bInitialConnect = 1;
	pData->nSent = 0;
finalize_it:
	RETiRet;
}

BEGINcreateInstance
CODESTARTcreateInstance
	pData->timeout = 90;
	pData->rebindInterval = 0;
	pData->bEnableTLS = DFLT_ENABLE_TLS;
	pData->bEnableTLSZip = DFLT_ENABLE_TLSZIP;
	pData->pristring = NULL;
	pData->caCertFile = NULL;
	pData->myCertFile = NULL;
	pData->myPrivKeyFile = NULL;
	pData->permittedPeers.nmemb = 0;
ENDcreateInstance

BEGINfreeInstance
	int i;
CODESTARTfreeInstance
	if(pData->pRelpClt != NULL)
		relpEngineCltDestruct(pRelpEngine, &pData->pRelpClt);
	free(pData->target);
	free(pData->port);
	free(pData->tplName);
	free(pData->pristring);
	free(pData->caCertFile);
	free(pData->myCertFile);
	free(pData->myPrivKeyFile);
	for(i = 0 ; i <  pData->permittedPeers.nmemb ; ++i) {
		free(pData->permittedPeers.name[i]);
	}
ENDfreeInstance

static inline void
setInstParamDefaults(instanceData *pData)
{
	pData->target = NULL;
	pData->port = NULL;
	pData->tplName = NULL;
	pData->timeout = 90;
	pData->rebindInterval = 0;
	pData->bEnableTLS = DFLT_ENABLE_TLS;
	pData->bEnableTLSZip = DFLT_ENABLE_TLSZIP;
	pData->pristring = NULL;
	pData->caCertFile = NULL;
	pData->myCertFile = NULL;
	pData->myPrivKeyFile = NULL;
	pData->permittedPeers.nmemb = 0;
}


BEGINnewActInst
	struct cnfparamvals *pvals;
	int i,j;
CODESTARTnewActInst
	if((pvals = nvlstGetParams(lst, &actpblk, NULL)) == NULL) {
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	CHKiRet(createInstance(&pData));
	setInstParamDefaults(pData);

	for(i = 0 ; i < actpblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(actpblk.descr[i].name, "target")) {
			pData->target = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "port")) {
			pData->port = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "template")) {
			pData->tplName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "timeout")) {
			pData->timeout = (unsigned) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "rebindinterval")) {
			pData->rebindInterval = (unsigned) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "tls")) {
			pData->bEnableTLS = (unsigned) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "tls.compression")) {
			pData->bEnableTLSZip = (unsigned) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "tls.prioritystring")) {
			pData->pristring = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "tls.cacert")) {
			pData->caCertFile = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "tls.mycert")) {
			pData->myCertFile = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "tls.myprivkey")) {
			pData->myPrivKeyFile = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "tls.permittedpeer")) {
			pData->permittedPeers.nmemb = pvals[i].val.d.ar->nmemb;
			CHKmalloc(pData->permittedPeers.name =
				malloc(sizeof(uchar*) * pData->permittedPeers.nmemb));
			for(j = 0 ; j <  pvals[i].val.d.ar->nmemb ; ++j) {
				pData->permittedPeers.name[j] = (uchar*)es_str2cstr(pvals[i].val.d.ar->arr[j], NULL);
			}
		} else {
			dbgprintf("omrelp: program error, non-handled "
			  "param '%s'\n", actpblk.descr[i].name);
		}
	}
	
	CODE_STD_STRING_REQUESTnewActInst(1)

	CHKiRet(OMSRsetEntry(*ppOMSR, 0, (uchar*)strdup((pData->tplName == NULL) ?
			    "RSYSLOG_ForwardFormat" : (char*)pData->tplName),
	   		    OMSR_NO_RQD_TPL_OPTS));

	CHKiRet(doCreateRelpClient(pData));

CODE_STD_FINALIZERnewActInst
	cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst

BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURERepeatedMsgReduction)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature

BEGINSetShutdownImmdtPtr
CODESTARTSetShutdownImmdtPtr
	relpEngineSetShutdownImmdtPtr(pRelpEngine, pPtr);
	DBGPRINTF("omrelp: shutdownImmediate ptr now is %p\n", pPtr);
ENDSetShutdownImmdtPtr


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	dbgprintf("RELP/%s", pData->target);
ENDdbgPrintInstInfo


/* try to connect to server
 * rgerhards, 2008-03-21
 */
static rsRetVal doConnect(instanceData *pData)
{
	DEFiRet;

	if(pData->bInitialConnect) {
		iRet = relpCltConnect(pData->pRelpClt, glbl.GetDefPFFamily(), pData->port, pData->target);
		if(iRet == RELP_RET_OK)
			pData->bInitialConnect = 0;
	} else {
		iRet = relpCltReconnect(pData->pRelpClt);
	}

	if(iRet == RELP_RET_OK) {
		pData->bIsConnected = 1;
	} else {
		pData->bIsConnected = 0;
		iRet = RS_RET_SUSPENDED;
	}

	RETiRet;
}


BEGINtryResume
CODESTARTtryResume
	iRet = doConnect(pData);
ENDtryResume

static inline rsRetVal
doRebind(instanceData *pData)
{
	DEFiRet;
	DBGPRINTF("omrelp: destructing relp client due to rebindInterval\n");
	CHKiRet(relpEngineCltDestruct(pRelpEngine, &pData->pRelpClt));
	pData->bIsConnected = 0;
	CHKiRet(doCreateRelpClient(pData));
finalize_it:
	RETiRet;
}

BEGINdoAction
	uchar *pMsg; /* temporary buffering */
	size_t lenMsg;
	relpRetVal ret;
CODESTARTdoAction
	dbgprintf(" %s:%s/RELP\n", pData->target, getRelpPt(pData));

	if(!pData->bIsConnected) {
		CHKiRet(doConnect(pData));
	}

	pMsg = ppString[0];
	lenMsg = strlen((char*) pMsg); /* TODO: don't we get this? */

	/* we need to truncate oversize msgs - no way around that... */
	if((int) lenMsg > glbl.GetMaxLine())
		lenMsg = glbl.GetMaxLine();

	/* forward */
	ret = relpCltSendSyslog(pData->pRelpClt, (uchar*) pMsg, lenMsg);
	if(ret != RELP_RET_OK) {
		/* error! */
		dbgprintf("error forwarding via relp, suspending\n");
		ABORT_FINALIZE(RS_RET_SUSPENDED);
	}

	if(pData->rebindInterval != 0 &&
	   (++pData->nSent >= pData->rebindInterval)) {
	   	doRebind(pData);
	}
finalize_it:
ENDdoAction


BEGINparseSelectorAct
	uchar *q;
	int i;
	int bErr;
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	if(!strncmp((char*) p, ":omrelp:", sizeof(":omrelp:") - 1)) {
		p += sizeof(":omrelp:") - 1; /* eat indicator sequence (-1 because of '\0'!) */
	} else {
		ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
	}

	/* ok, if we reach this point, we have something for us */
	if((iRet = createInstance(&pData)) != RS_RET_OK)
		FINALIZE;

	/* extract the host first (we do a trick - we replace the ';' or ':' with a '\0')
	 * now skip to port and then template name. rgerhards 2005-07-06
	 */
	if(*p == '[') { /* everything is hostname upto ']' */
		++p; /* skip '[' */
		for(q = p ; *p && *p != ']' ; ++p)
			/* JUST SKIP */;
		if(*p == ']') {
			*p = '\0'; /* trick to obtain hostname (later)! */
			++p; /* eat it */
		}
	} else { /* traditional view of hostname */
		for(q = p ; *p && *p != ';' && *p != ':' && *p != '#' ; ++p)
			/* JUST SKIP */;
	}

	pData->port = NULL;
	if(*p == ':') { /* process port */
		uchar * tmp;

		*p = '\0'; /* trick to obtain hostname (later)! */
		tmp = ++p;
		for(i=0 ; *p && isdigit((int) *p) ; ++p, ++i)
			/* SKIP AND COUNT */;
		pData->port = MALLOC(i + 1);
		if(pData->port == NULL) {
			errmsg.LogError(0, NO_ERRCODE, "Could not get memory to store relp port, "
				 "using default port, results may not be what you intend\n");
			/* we leave f_forw.port set to NULL, this is then handled by getRelpPt() */
		} else {
			memcpy(pData->port, tmp, i);
			*(pData->port + i) = '\0';
		}
	}
	
	/* now skip to template */
	bErr = 0;
	while(*p && *p != ';') {
		if(*p && *p != ';' && !isspace((int) *p)) {
			if(bErr == 0) { /* only 1 error msg! */
				bErr = 1;
				errno = 0;
				errmsg.LogError(0, NO_ERRCODE, "invalid selector line (port), probably not doing "
					 "what was intended");
			}
		}
		++p;
	}

	if(*p == ';') {
		*p = '\0'; /* trick to obtain hostname (later)! */
		CHKmalloc(pData->target = ustrdup(q));
		*p = ';';
	} else {
		CHKmalloc(pData->target = ustrdup(q));
	}

	/* process template */
	CHKiRet(cflineParseTemplateName(&p, *ppOMSR, 0, OMSR_NO_RQD_TPL_OPTS, (uchar*) "RSYSLOG_ForwardFormat"));

	CHKiRet(doCreateRelpClient(pData));

CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINmodExit
CODESTARTmodExit
	relpEngineDestruct(&pRelpEngine);

	/* release what we no longer need */
	objRelease(glbl, CORE_COMPONENT);
	objRelease(errmsg, CORE_COMPONENT);
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
CODEqueryEtryPt_STD_CONF2_CNFNAME_QUERIES 
CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES
CODEqueryEtryPt_SetShutdownImmdtPtr
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
INITLegCnfVars
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	/* create our relp engine */
	CHKiRet(relpEngineConstruct(&pRelpEngine));
	CHKiRet(relpEngineSetDbgprint(pRelpEngine, dbgprintf));
	CHKiRet(relpEngineSetOnAuthErr(pRelpEngine, onAuthErr));
	CHKiRet(relpEngineSetEnableCmd(pRelpEngine, (uchar*) "syslog", eRelpCmdState_Required));

	/* tell which objects we need */
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(glbl, CORE_COMPONENT));
ENDmodInit

/* vim:set ai:
 */
