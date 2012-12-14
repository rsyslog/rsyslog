/* imtcp.c
 * This is the implementation of the TCP input module.
 *
 * File begun on 2007-12-21 by RGerhards (extracted from syslogd.c,
 * which at the time of the rsyslog fork was BSD-licensed)
 *
 * Copyright 2007-2012 Adiscon GmbH.
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

/* This note shall explain the calling sequence while we do not have
 * have full RainerScript support for (TLS) sender authentication:
 *
 * imtcp --> tcpsrv --> netstrms (this sequence stored pPermPeers in netstrms class)
 * then a callback (doOpenLstnSocks) into imtcp happens, which in turn calls
 * into tcpsrv.create_tcp_socket(),
 * which calls into netstrm.LstnInit(), which receives a pointer to netstrms obj
 * which calls into the driver function LstnInit (again, netstrms obj passed)
 * which finally calls back into netstrms obj's get functions to obtain the auth
 * parameters and then applies them to the driver object instance
 *
 * rgerhards, 2008-05-19
 */
#include "config.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <ctype.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include "rsyslog.h"
#include "dirty.h"
#include "cfsysline.h"
#include "module-template.h"
#include "unicode-helper.h"
#include "net.h"
#include "netstrm.h"
#include "errmsg.h"
#include "tcpsrv.h"
#include "ruleset.h"
#include "rainerscript.h"
#include "net.h" /* for permittedPeers, may be removed when this is removed */

MODULE_TYPE_INPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("imtcp")

/* static data */
DEF_IMOD_STATIC_DATA
DEFobjCurrIf(tcpsrv)
DEFobjCurrIf(tcps_sess)
DEFobjCurrIf(net)
DEFobjCurrIf(netstrm)
DEFobjCurrIf(errmsg)
DEFobjCurrIf(ruleset)

static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal);

/* Module static data */
static tcpsrv_t *pOurTcpsrv = NULL;  /* our TCP server(listener) TODO: change for multiple instances */
static permittedPeers_t *pPermPeersRoot = NULL;


/* config settings */
static struct configSettings_s {
	int iTCPSessMax;
	int iTCPLstnMax;
	int bSuppOctetFram;
	int iStrmDrvrMode;
	int bKeepAlive;
	int bEmitMsgOnClose;
	int iAddtlFrameDelim;
	int bDisableLFDelim;
	int bUseFlowControl;
	uchar *pszStrmDrvrAuthMode;
	uchar *pszInputName;
	uchar *pszBindRuleset;
} cs;

struct instanceConf_s {
	uchar *pszBindPort;		/* port to bind to */
	uchar *pszBindRuleset;		/* name of ruleset to bind to */
	ruleset_t *pBindRuleset;	/* ruleset to bind listener to (use system default if unspecified) */
	uchar *pszInputName;		/* value for inputname property, NULL is OK and handled by core engine */
	int ratelimitInterval;
	int ratelimitBurst;
	int bSuppOctetFram;
	struct instanceConf_s *next;
};


struct modConfData_s {
	rsconf_t *pConf;		/* our overall config object */
	instanceConf_t *root, *tail;
	int iTCPSessMax; /* max number of sessions */
	int iTCPLstnMax; /* max number of sessions */
	int iStrmDrvrMode; /* mode for stream driver, driver-dependent (0 mostly means plain tcp) */
	int iAddtlFrameDelim; /* addtl frame delimiter, e.g. for netscreen, default none */
	int bSuppOctetFram;
	sbool bDisableLFDelim; /* disable standard LF delimiter */
	sbool bUseFlowControl; /* use flow control, what means indicate ourselfs a "light delayable" */
	sbool bKeepAlive;
	sbool bEmitMsgOnClose; /* emit an informational message on close by remote peer */
	uchar *pszStrmDrvrAuthMode; /* authentication mode to use */
	struct cnfarray *permittedPeers;
	sbool configSetViaV2Method;
};

static modConfData_t *loadModConf = NULL;/* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL;/* modConf ptr to use for the current load process */

/* module-global parameters */
static struct cnfparamdescr modpdescr[] = {
	{ "flowcontrol", eCmdHdlrBinary, 0 },
	{ "disablelfdelimiter", eCmdHdlrBinary, 0 },
	{ "octetcountedframing", eCmdHdlrBinary, 0 },
	{ "notifyonconnectionclose", eCmdHdlrBinary, 0 },
	{ "addtlframedelimiter", eCmdHdlrPositiveInt, 0 },
	{ "maxsessions", eCmdHdlrPositiveInt, 0 },
	{ "maxlistners", eCmdHdlrPositiveInt, 0 },
	{ "streamdriver.mode", eCmdHdlrPositiveInt, 0 },
	{ "streamdriver.authmode", eCmdHdlrString, 0 },
	{ "permittedpeer", eCmdHdlrArray, 0 },
	{ "keepalive", eCmdHdlrBinary, 0 }
};
static struct cnfparamblk modpblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(modpdescr)/sizeof(struct cnfparamdescr),
	  modpdescr
	};

/* input instance parameters */
static struct cnfparamdescr inppdescr[] = {
	{ "port", eCmdHdlrString, CNFPARAM_REQUIRED }, /* legacy: InputTCPServerRun */
	{ "name", eCmdHdlrString, 0 },
	{ "ruleset", eCmdHdlrString, 0 },
	{ "supportOctetCountedFraming", eCmdHdlrBinary, 0 },
	{ "ratelimit.interval", eCmdHdlrInt, 0 },
	{ "ratelimit.burst", eCmdHdlrInt, 0 }
};
static struct cnfparamblk inppblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(inppdescr)/sizeof(struct cnfparamdescr),
	  inppdescr
	};

#include "im-helper.h" /* must be included AFTER the type definitions! */

static int bLegacyCnfModGlobalsPermitted;/* are legacy module-global config parameters permitted? */

/* callbacks */
/* this shall go into a specific ACL module! */
static int
isPermittedHost(struct sockaddr *addr, char *fromHostFQDN, void __attribute__((unused)) *pUsrSrv,
	        void __attribute__((unused)) *pUsrSess)
{
	return net.isAllowedSender2(UCHAR_CONSTANT("TCP"), addr, fromHostFQDN, 1);
}


static rsRetVal
doOpenLstnSocks(tcpsrv_t *pSrv)
{
	ISOBJ_TYPE_assert(pSrv, tcpsrv);
	return tcpsrv.create_tcp_socket(pSrv);
}


static rsRetVal
doRcvData(tcps_sess_t *pSess, char *buf, size_t lenBuf, ssize_t *piLenRcvd)
{
	DEFiRet;
	assert(pSess != NULL);
	assert(piLenRcvd != NULL);

	*piLenRcvd = lenBuf;
	CHKiRet(netstrm.Rcv(pSess->pStrm, (uchar*) buf, piLenRcvd));
finalize_it:
	RETiRet;
}

static rsRetVal
onRegularClose(tcps_sess_t *pSess)
{
	DEFiRet;
	assert(pSess != NULL);

	/* process any incomplete frames left over */
	tcps_sess.PrepareClose(pSess);
	/* Session closed */
	tcps_sess.Close(pSess);
	RETiRet;
}


static rsRetVal
onErrClose(tcps_sess_t *pSess)
{
	DEFiRet;
	assert(pSess != NULL);

	tcps_sess.Close(pSess);
	RETiRet;
}

/* ------------------------------ end callbacks ------------------------------ */


/* set permitted peer -- rgerhards, 2008-05-19
 */
static rsRetVal
setPermittedPeer(void __attribute__((unused)) *pVal, uchar *pszID)
{
	DEFiRet;
	CHKiRet(net.AddPermittedPeer(&pPermPeersRoot, pszID));
	free(pszID); /* no longer needed, but we need to free as of interface def */
finalize_it:
	RETiRet;
}


/* create input instance, set default paramters, and
 * add it to the list of instances.
 */
static rsRetVal
createInstance(instanceConf_t **pinst)
{
	instanceConf_t *inst;
	DEFiRet;
	CHKmalloc(inst = MALLOC(sizeof(instanceConf_t)));
	inst->next = NULL;
	inst->pszBindRuleset = NULL;
	inst->pszInputName = NULL;
	inst->bSuppOctetFram = 1;
	inst->ratelimitInterval = 0;
	inst->ratelimitBurst = 10000;

	/* node created, let's add to config */
	if(loadModConf->tail == NULL) {
		loadModConf->tail = loadModConf->root = inst;
	} else {
		loadModConf->tail->next = inst;
		loadModConf->tail = inst;
	}

	*pinst = inst;
finalize_it:
	RETiRet;
}


/* This function is called when a new listener instace shall be added to 
 * the current config object via the legacy config system. It just shuffles
 * all parameters to the listener in-memory instance.
 * rgerhards, 2011-05-04
 */
static rsRetVal addInstance(void __attribute__((unused)) *pVal, uchar *pNewVal)
{
	instanceConf_t *inst;
	DEFiRet;

	CHKiRet(createInstance(&inst));

	CHKmalloc(inst->pszBindPort = ustrdup((pNewVal == NULL || *pNewVal == '\0')
				 	       ? (uchar*) "10514" : pNewVal));
	if((cs.pszBindRuleset == NULL) || (cs.pszBindRuleset[0] == '\0')) {
		inst->pszBindRuleset = NULL;
	} else {
		CHKmalloc(inst->pszBindRuleset = ustrdup(cs.pszBindRuleset));
	}
	if((cs.pszInputName == NULL) || (cs.pszInputName[0] == '\0')) {
		inst->pszInputName = NULL;
	} else {
		CHKmalloc(inst->pszInputName = ustrdup(cs.pszInputName));
	}
	inst->bSuppOctetFram = cs.bSuppOctetFram;

finalize_it:
	free(pNewVal);
	RETiRet;
}


static rsRetVal
addListner(modConfData_t *modConf, instanceConf_t *inst)
{
	DEFiRet;

	if(pOurTcpsrv == NULL) {
		CHKiRet(tcpsrv.Construct(&pOurTcpsrv));
		/* callbacks */
		CHKiRet(tcpsrv.SetCBIsPermittedHost(pOurTcpsrv, isPermittedHost));
		CHKiRet(tcpsrv.SetCBRcvData(pOurTcpsrv, doRcvData));
		CHKiRet(tcpsrv.SetCBOpenLstnSocks(pOurTcpsrv, doOpenLstnSocks));
		CHKiRet(tcpsrv.SetCBOnRegularClose(pOurTcpsrv, onRegularClose));
		CHKiRet(tcpsrv.SetCBOnErrClose(pOurTcpsrv, onErrClose));
		/* params */
		CHKiRet(tcpsrv.SetKeepAlive(pOurTcpsrv, modConf->bKeepAlive));
		CHKiRet(tcpsrv.SetSessMax(pOurTcpsrv, modConf->iTCPSessMax));
		CHKiRet(tcpsrv.SetLstnMax(pOurTcpsrv, modConf->iTCPLstnMax));
		CHKiRet(tcpsrv.SetDrvrMode(pOurTcpsrv, modConf->iStrmDrvrMode));
		CHKiRet(tcpsrv.SetUseFlowControl(pOurTcpsrv, modConf->bUseFlowControl));
		CHKiRet(tcpsrv.SetAddtlFrameDelim(pOurTcpsrv, modConf->iAddtlFrameDelim));
		CHKiRet(tcpsrv.SetbDisableLFDelim(pOurTcpsrv, modConf->bDisableLFDelim));
		CHKiRet(tcpsrv.SetNotificationOnRemoteClose(pOurTcpsrv, modConf->bEmitMsgOnClose));
		/* now set optional params, but only if they were actually configured */
		if(modConf->pszStrmDrvrAuthMode != NULL) {
			CHKiRet(tcpsrv.SetDrvrAuthMode(pOurTcpsrv, modConf->pszStrmDrvrAuthMode));
		}
		if(pPermPeersRoot != NULL) {
			CHKiRet(tcpsrv.SetDrvrPermPeers(pOurTcpsrv, pPermPeersRoot));
		}
	}

	/* initialized, now add socket and listener params */
	DBGPRINTF("imtcp: trying to add port *:%s\n", inst->pszBindPort);
	CHKiRet(tcpsrv.SetRuleset(pOurTcpsrv, inst->pBindRuleset));
	CHKiRet(tcpsrv.SetInputName(pOurTcpsrv, inst->pszInputName == NULL ?
						UCHAR_CONSTANT("imtcp") : inst->pszInputName));
	CHKiRet(tcpsrv.SetLinuxLikeRatelimiters(pOurTcpsrv, inst->ratelimitInterval, inst->ratelimitBurst));
	tcpsrv.configureTCPListen(pOurTcpsrv, inst->pszBindPort, inst->bSuppOctetFram);

finalize_it:
	if(iRet != RS_RET_OK) {
		errmsg.LogError(0, NO_ERRCODE, "imtcp: error %d trying to add listener", iRet);
	}
	RETiRet;
}


BEGINnewInpInst
	struct cnfparamvals *pvals;
	instanceConf_t *inst;
	int i;
CODESTARTnewInpInst
	DBGPRINTF("newInpInst (imtcp)\n");

	pvals = nvlstGetParams(lst, &inppblk, NULL);
	if(pvals == NULL) {
		errmsg.LogError(0, RS_RET_MISSING_CNFPARAMS,
			        "imtcp: required parameter are missing\n");
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	if(Debug) {
		dbgprintf("input param blk in imtcp:\n");
		cnfparamsPrint(&inppblk, pvals);
	}

	CHKiRet(createInstance(&inst));

	for(i = 0 ; i < inppblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(inppblk.descr[i].name, "port")) {
			inst->pszBindPort = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(inppblk.descr[i].name, "name")) {
			inst->pszInputName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(inppblk.descr[i].name, "ruleset")) {
			inst->pszBindRuleset = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(inppblk.descr[i].name, "supportOctetCountedFraming")) {
			inst->bSuppOctetFram = (int) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "ratelimit.burst")) {
			inst->ratelimitBurst = (int) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "ratelimit.interval")) {
			inst->ratelimitInterval = (int) pvals[i].val.d.n;
		} else {
			dbgprintf("imtcp: program error, non-handled "
			  "param '%s'\n", inppblk.descr[i].name);
		}
	}
finalize_it:
CODE_STD_FINALIZERnewInpInst
	cnfparamvalsDestruct(pvals, &inppblk);
ENDnewInpInst


BEGINbeginCnfLoad
CODESTARTbeginCnfLoad
	loadModConf = pModConf;
	pModConf->pConf = pConf;
	/* init our settings */
	loadModConf->iTCPSessMax = 200;
	loadModConf->iTCPLstnMax = 20;
	loadModConf->bSuppOctetFram = 1;
	loadModConf->iStrmDrvrMode = 0;
	loadModConf->bUseFlowControl = 0;
	loadModConf->bKeepAlive = 0;
	loadModConf->bEmitMsgOnClose = 0;
	loadModConf->iAddtlFrameDelim = TCPSRV_NO_ADDTL_DELIMITER;
	loadModConf->bDisableLFDelim = 0;
	loadModConf->pszStrmDrvrAuthMode = NULL;
	loadModConf->permittedPeers = NULL;
	loadModConf->configSetViaV2Method = 0;
	bLegacyCnfModGlobalsPermitted = 1;
	/* init legacy config variables */
	cs.pszStrmDrvrAuthMode = NULL;
	resetConfigVariables(NULL, NULL); /* dummy parameters just to fulfill interface def */
ENDbeginCnfLoad


BEGINsetModCnf
	struct cnfparamvals *pvals = NULL;
	int i;
CODESTARTsetModCnf
	pvals = nvlstGetParams(lst, &modpblk, NULL);
	if(pvals == NULL) {
		errmsg.LogError(0, RS_RET_MISSING_CNFPARAMS, "imtcp: error processing module "
				"config parameters [module(...)]");
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	if(Debug) {
		dbgprintf("module (global) param blk for imtcp:\n");
		cnfparamsPrint(&modpblk, pvals);
	}

	for(i = 0 ; i < modpblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(modpblk.descr[i].name, "flowcontrol")) {
			loadModConf->bUseFlowControl = (int) pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "disablelfdelimiter")) {
			loadModConf->bDisableLFDelim = (int) pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "octetcountedframing")) {
			loadModConf->bSuppOctetFram = (int) pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "notifyonconnectionclose")) {
			loadModConf->bEmitMsgOnClose = (int) pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "addtlframedelimiter")) {
			loadModConf->iAddtlFrameDelim = (int) pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "maxsessions")) {
			loadModConf->iTCPSessMax = (int) pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "maxlistners")) {
			loadModConf->iTCPLstnMax = (int) pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "keepalive")) {
			loadModConf->bKeepAlive = (int) pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "streamdriver.mode")) {
			loadModConf->iStrmDrvrMode = (int) pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "streamdriver.authmode")) {
			loadModConf->pszStrmDrvrAuthMode = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(modpblk.descr[i].name, "permittedpeer")) {
			loadModConf->permittedPeers = cnfarrayDup(pvals[i].val.d.ar);
		} else {
			dbgprintf("imtcp: program error, non-handled "
			  "param '%s' in beginCnfLoad\n", modpblk.descr[i].name);
		}
	}

	/* remove all of our legacy handlers, as they can not used in addition
	 * the the new-style config method.
	 */
	bLegacyCnfModGlobalsPermitted = 0;
	loadModConf->configSetViaV2Method = 1;

finalize_it:
	if(pvals != NULL)
		cnfparamvalsDestruct(pvals, &modpblk);
ENDsetModCnf


BEGINendCnfLoad
CODESTARTendCnfLoad
	if(!loadModConf->configSetViaV2Method) {
		/* persist module-specific settings from legacy config system */
		pModConf->iTCPSessMax = cs.iTCPSessMax;
		pModConf->iTCPLstnMax = cs.iTCPLstnMax;
		pModConf->iStrmDrvrMode = cs.iStrmDrvrMode;
		pModConf->bEmitMsgOnClose = cs.bEmitMsgOnClose;
		pModConf->bSuppOctetFram = cs.bSuppOctetFram;
		pModConf->iAddtlFrameDelim = cs.iAddtlFrameDelim;
		pModConf->bDisableLFDelim = cs.bDisableLFDelim;
		pModConf->bUseFlowControl = cs.bUseFlowControl;
		pModConf->bKeepAlive = cs.bKeepAlive;
		if((cs.pszStrmDrvrAuthMode == NULL) || (cs.pszStrmDrvrAuthMode[0] == '\0')) {
			loadModConf->pszStrmDrvrAuthMode = NULL;
		} else {
			loadModConf->pszStrmDrvrAuthMode = cs.pszStrmDrvrAuthMode;
			cs.pszStrmDrvrAuthMode = NULL;
		}
	}
	free(cs.pszStrmDrvrAuthMode);
	cs.pszStrmDrvrAuthMode = NULL;

	loadModConf = NULL; /* done loading */
ENDendCnfLoad


/* function to generate error message if framework does not find requested ruleset */
static inline void
std_checkRuleset_genErrMsg(__attribute__((unused)) modConfData_t *modConf, instanceConf_t *inst)
{
	errmsg.LogError(0, NO_ERRCODE, "imtcp: ruleset '%s' for port %s not found - "
			"using default ruleset instead", inst->pszBindRuleset,
			inst->pszBindPort);
}

BEGINcheckCnf
	instanceConf_t *inst;
CODESTARTcheckCnf
	for(inst = pModConf->root ; inst != NULL ; inst = inst->next) {
		std_checkRuleset(pModConf, inst);
	}
	if(pModConf->root == NULL) {
		errmsg.LogError(0, RS_RET_NO_LISTNERS , "imtcp: module loaded, but "
				"no listeners defined - no input will be gathered");
		iRet = RS_RET_NO_LISTNERS;
	}
ENDcheckCnf


BEGINactivateCnfPrePrivDrop
	instanceConf_t *inst;
	int i;
CODESTARTactivateCnfPrePrivDrop
	runModConf = pModConf;
	if(runModConf->permittedPeers != NULL) {
		for(i = 0 ; i <  runModConf->permittedPeers->nmemb ; ++i) {
			setPermittedPeer(NULL, (uchar*)
			    es_str2cstr(runModConf->permittedPeers->arr[i], NULL));
		}
	}
	for(inst = runModConf->root ; inst != NULL ; inst = inst->next) {
		addListner(pModConf, inst);
	}
	if(pOurTcpsrv == NULL)
		ABORT_FINALIZE(RS_RET_NO_RUN);
	CHKiRet(tcpsrv.ConstructFinalize(pOurTcpsrv));
finalize_it:
ENDactivateCnfPrePrivDrop


BEGINactivateCnf
CODESTARTactivateCnf
	/* sorry, nothing to do here... */
ENDactivateCnf


BEGINfreeCnf
	instanceConf_t *inst, *del;
CODESTARTfreeCnf
	free(pModConf->pszStrmDrvrAuthMode);
	if(pModConf->permittedPeers != NULL) {
		cnfarrayContentDestruct(pModConf->permittedPeers);
		free(pModConf->permittedPeers);
	}
	for(inst = pModConf->root ; inst != NULL ; ) {
		free(inst->pszBindPort);
		free(inst->pszInputName);
		del = inst;
		inst = inst->next;
		free(del);
	}
ENDfreeCnf

/* This function is called to gather input.
 */
BEGINrunInput
CODESTARTrunInput
	iRet = tcpsrv.Run(pOurTcpsrv);
ENDrunInput


/* initialize and return if will run or not */
BEGINwillRun
CODESTARTwillRun
	net.PrintAllowedSenders(2); /* TCP */
ENDwillRun


BEGINafterRun
CODESTARTafterRun
	if(pOurTcpsrv != NULL)
		iRet = tcpsrv.Destruct(&pOurTcpsrv);

	net.clearAllowedSenders(UCHAR_CONSTANT("TCP"));
ENDafterRun


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURENonCancelInputTermination)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINmodExit
CODESTARTmodExit
	if(pPermPeersRoot != NULL) {
		net.DestructPermittedPeers(&pPermPeersRoot);
	}

	/* release objects we used */
	objRelease(net, LM_NET_FILENAME);
	objRelease(netstrm, LM_NETSTRMS_FILENAME);
	objRelease(tcps_sess, LM_TCPSRV_FILENAME);
	objRelease(tcpsrv, LM_TCPSRV_FILENAME);
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(ruleset, CORE_COMPONENT);
ENDmodExit


static rsRetVal
resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	cs.iTCPSessMax = 200;
	cs.iTCPLstnMax = 20;
	cs.bSuppOctetFram = 1;
	cs.iStrmDrvrMode = 0;
	cs.bUseFlowControl = 0;
	cs.bKeepAlive = 0;
	cs.bEmitMsgOnClose = 0;
	cs.iAddtlFrameDelim = TCPSRV_NO_ADDTL_DELIMITER;
	cs.bDisableLFDelim = 0;
	free(cs.pszInputName);
	cs.pszInputName = NULL;
	free(cs.pszStrmDrvrAuthMode);
	cs.pszStrmDrvrAuthMode = NULL;
	return RS_RET_OK;
}



BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_IMOD_QUERIES
CODEqueryEtryPt_STD_CONF2_QUERIES
CODEqueryEtryPt_STD_CONF2_setModCnf_QUERIES
CODEqueryEtryPt_STD_CONF2_PREPRIVDROP_QUERIES
CODEqueryEtryPt_STD_CONF2_IMOD_QUERIES
CODEqueryEtryPt_IsCompatibleWithFeature_IF_OMOD_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	pOurTcpsrv = NULL;
	/* request objects we use */
	CHKiRet(objUse(net, LM_NET_FILENAME));
	CHKiRet(objUse(netstrm, LM_NETSTRMS_FILENAME));
	CHKiRet(objUse(tcps_sess, LM_TCPSRV_FILENAME));
	CHKiRet(objUse(tcpsrv, LM_TCPSRV_FILENAME));
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(ruleset, CORE_COMPONENT));

	/* register config file handlers */
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("inputtcpserverrun"), 0, eCmdHdlrGetWord,
				   addInstance, NULL, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("inputtcpserverinputname"), 0, eCmdHdlrGetWord,
				   NULL, &cs.pszInputName, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("inputtcpserverbindruleset"), 0, eCmdHdlrGetWord,
				   NULL, &cs.pszBindRuleset, STD_LOADABLE_MODULE_ID));
	/* module-global config params - will be disabled in configs that are loaded
	 * via module(...).
	 */
	CHKiRet(regCfSysLineHdlr2(UCHAR_CONSTANT("inputtcpserverstreamdriverpermittedpeer"), 0, eCmdHdlrGetWord,
			   setPermittedPeer, NULL, STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
	CHKiRet(regCfSysLineHdlr2(UCHAR_CONSTANT("inputtcpserverstreamdriverauthmode"), 0, eCmdHdlrGetWord,
			   NULL, &cs.pszStrmDrvrAuthMode, STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
	CHKiRet(regCfSysLineHdlr2(UCHAR_CONSTANT("inputtcpserverkeepalive"), 0, eCmdHdlrBinary,
			   NULL, &cs.bKeepAlive, STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
	CHKiRet(regCfSysLineHdlr2(UCHAR_CONSTANT("inputtcpflowcontrol"), 0, eCmdHdlrBinary,
			   NULL, &cs.bUseFlowControl, STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
	CHKiRet(regCfSysLineHdlr2(UCHAR_CONSTANT("inputtcpserverdisablelfdelimiter"), 0, eCmdHdlrBinary,
			   NULL, &cs.bDisableLFDelim, STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
	CHKiRet(regCfSysLineHdlr2(UCHAR_CONSTANT("inputtcpserveraddtlframedelimiter"), 0, eCmdHdlrInt,
			   NULL, &cs.iAddtlFrameDelim, STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
	CHKiRet(regCfSysLineHdlr2(UCHAR_CONSTANT("inputtcpserversupportoctetcountedframing"), 0, eCmdHdlrBinary,
			   NULL, &cs.bSuppOctetFram, STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
	CHKiRet(regCfSysLineHdlr2(UCHAR_CONSTANT("inputtcpmaxsessions"), 0, eCmdHdlrInt,
			   NULL, &cs.iTCPSessMax, STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
	CHKiRet(regCfSysLineHdlr2(UCHAR_CONSTANT("inputtcpmaxlisteners"), 0, eCmdHdlrInt,
			   NULL, &cs.iTCPLstnMax, STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
	CHKiRet(regCfSysLineHdlr2(UCHAR_CONSTANT("inputtcpservernotifyonconnectionclose"), 0, eCmdHdlrBinary,
			   NULL, &cs.bEmitMsgOnClose, STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
	CHKiRet(regCfSysLineHdlr2(UCHAR_CONSTANT("inputtcpserverstreamdrivermode"), 0, eCmdHdlrInt,
			   NULL, &cs.iStrmDrvrMode, STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));

	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("resetconfigvariables"), 1, eCmdHdlrCustomHandler,
				   resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit

/* vim:set ai:
 */
