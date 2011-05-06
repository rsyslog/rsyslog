/* imtcp.c
 * This is the implementation of the TCP input module.
 *
 * File begun on 2007-12-21 by RGerhards (extracted from syslogd.c)
 *
 * Copyright 2007-2011 Rainer Gerhards and Adiscon GmbH.
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
#include "net.h" /* for permittedPeers, may be removed when this is removed */

MODULE_TYPE_INPUT
MODULE_TYPE_NOKEEP

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
	int iStrmDrvrMode;
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
	struct instanceConf_s *next;
};


struct modConfData_s {
	rsconf_t *pConf;		/* our overall config object */
	instanceConf_t *root, *tail;
	int iTCPSessMax; /* max number of sessions */
	int iTCPLstnMax; /* max number of sessions */
	int iStrmDrvrMode; /* mode for stream driver, driver-dependent (0 mostly means plain tcp) */
	int bEmitMsgOnClose; /* emit an informational message on close by remote peer */
	int iAddtlFrameDelim; /* addtl frame delimiter, e.g. for netscreen, default none */
	int bDisableLFDelim; /* disable standard LF delimiter */
	int bUseFlowControl; /* use flow control, what means indicate ourselfs a "light delayable" */
	uchar *pszStrmDrvrAuthMode; /* authentication mode to use */
};

static modConfData_t *loadModConf = NULL;/* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL;/* modConf ptr to use for the current load process */

#include "im-helper.h" /* must be included AFTER the type definitions! */

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


/* This function is called when a new listener instace shall be added to 
 * the current config object via the legacy config system. It just shuffles
 * all parameters to the listener in-memory instance.
 * rgerhards, 2011-05-04
 */
static rsRetVal addInstance(void __attribute__((unused)) *pVal, uchar *pNewVal)
{
	instanceConf_t *inst;
	DEFiRet;

	CHKmalloc(inst = MALLOC(sizeof(instanceConf_t)));

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
	inst->next = NULL;

	/* node created, let's add to config */
	if(loadModConf->tail == NULL) {
		loadModConf->tail = loadModConf->root = inst;
	} else {
		loadModConf->tail->next = inst;
		loadModConf->tail = inst;
	}

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
	tcpsrv.configureTCPListen(pOurTcpsrv, inst->pszBindPort);

finalize_it:
	if(iRet != RS_RET_OK) {
		errmsg.LogError(0, NO_ERRCODE, "imtcp: error %d trying to add listener", iRet);
	}
	RETiRet;
}


BEGINbeginCnfLoad
CODESTARTbeginCnfLoad
	loadModConf = pModConf;
	pModConf->pConf = pConf;
	/* init legacy config variables */
	cs.pszStrmDrvrAuthMode = NULL;
	resetConfigVariables(NULL, NULL); /* dummy parameters just to fulfill interface def */
ENDbeginCnfLoad


BEGINendCnfLoad
CODESTARTendCnfLoad
	/* persist module-specific settings from legacy config system */
	pModConf->iTCPSessMax = cs.iTCPSessMax;
	pModConf->iTCPLstnMax = cs.iTCPLstnMax;
	pModConf->iStrmDrvrMode = cs.iStrmDrvrMode;
	pModConf->bEmitMsgOnClose = cs.bEmitMsgOnClose;
	pModConf->iAddtlFrameDelim = cs.iAddtlFrameDelim;
	pModConf->bDisableLFDelim = cs.bDisableLFDelim;
	pModConf->bUseFlowControl = cs.bUseFlowControl;
	if((cs.pszStrmDrvrAuthMode == NULL) || (cs.pszStrmDrvrAuthMode[0] == '\0')) {
		loadModConf->pszStrmDrvrAuthMode = NULL;
		free(cs.pszStrmDrvrAuthMode);
	} else {
		loadModConf->pszStrmDrvrAuthMode = cs.pszStrmDrvrAuthMode;
	}
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
ENDcheckCnf


BEGINactivateCnfPrePrivDrop
	instanceConf_t *inst;
CODESTARTactivateCnfPrePrivDrop
	runModConf = pModConf;
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
CODESTARTfreeCnf
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
	/* do cleanup here */
	net.clearAllowedSenders(UCHAR_CONSTANT("TCP"));
ENDafterRun


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURENonCancelInputTermination)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINmodExit
CODESTARTmodExit
	if(pOurTcpsrv != NULL)
		iRet = tcpsrv.Destruct(&pOurTcpsrv);

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
	cs.iStrmDrvrMode = 0;
	cs.bUseFlowControl = 0;
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
CODEqueryEtryPt_STD_CONF2_PREPRIVDROP_QUERIES
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
				   addInstance, NULL, STD_LOADABLE_MODULE_ID, eConfObjGlobal));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("inputtcpmaxsessions"), 0, eCmdHdlrInt,
				   NULL, &cs.iTCPSessMax, STD_LOADABLE_MODULE_ID, eConfObjGlobal));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("inputtcpmaxlisteners"), 0, eCmdHdlrInt,
				   NULL, &cs.iTCPLstnMax, STD_LOADABLE_MODULE_ID, eConfObjGlobal));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("inputtcpservernotifyonconnectionclose"), 0, eCmdHdlrBinary,
				   NULL, &cs.bEmitMsgOnClose, STD_LOADABLE_MODULE_ID, eConfObjGlobal));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("inputtcpserverstreamdrivermode"), 0, eCmdHdlrInt,
				   NULL, &cs.iStrmDrvrMode, STD_LOADABLE_MODULE_ID, eConfObjGlobal));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("inputtcpserverstreamdriverauthmode"), 0, eCmdHdlrGetWord,
				   NULL, &cs.pszStrmDrvrAuthMode, STD_LOADABLE_MODULE_ID, eConfObjGlobal));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("inputtcpserverstreamdriverpermittedpeer"), 0, eCmdHdlrGetWord,
				   setPermittedPeer, NULL, STD_LOADABLE_MODULE_ID, eConfObjGlobal));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("inputtcpserveraddtlframedelimiter"), 0, eCmdHdlrInt,
				   NULL, &cs.iAddtlFrameDelim, STD_LOADABLE_MODULE_ID, eConfObjGlobal));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("inputtcpserverdisablelfdelimiter"), 0, eCmdHdlrBinary,
				   NULL, &cs.bDisableLFDelim, STD_LOADABLE_MODULE_ID, eConfObjGlobal));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("inputtcpserverinputname"), 0, eCmdHdlrGetWord,
				   NULL, &cs.pszInputName, STD_LOADABLE_MODULE_ID, eConfObjGlobal));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("inputtcpserverbindruleset"), 0, eCmdHdlrGetWord,
				   NULL, &cs.pszBindRuleset, STD_LOADABLE_MODULE_ID, eConfObjGlobal));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("inputtcpflowcontrol"), 0, eCmdHdlrBinary,
				   NULL, &cs.bUseFlowControl, STD_LOADABLE_MODULE_ID, eConfObjGlobal));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("resetconfigvariables"), 1, eCmdHdlrCustomHandler,
				   resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID, eConfObjGlobal));
ENDmodInit


/* vim:set ai:
 */
