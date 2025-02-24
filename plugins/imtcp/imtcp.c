/* imtcp.c
 * This is the implementation of the TCP input module.
 *
 * File begun on 2007-12-21 by RGerhards (extracted from syslogd.c,
 * which at the time of the rsyslog fork was BSD-licensed)
 *
 * Copyright 2007-2025 Adiscon GmbH.
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
#include <signal.h>
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
#include "net.h"
#include "parserif.h"

MODULE_TYPE_INPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("imtcp")

/* static data */
DEF_IMOD_STATIC_DATA
DEFobjCurrIf(tcpsrv)
DEFobjCurrIf(tcps_sess)
DEFobjCurrIf(net)
DEFobjCurrIf(netstrm)
DEFobjCurrIf(ruleset)

static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal);

/* Module static data */
typedef struct tcpsrv_etry_s {
	tcpsrv_t *tcpsrv;
	pthread_t tid;	/* the worker's thread ID */
	struct tcpsrv_etry_s *next;
} tcpsrv_etry_t;
static tcpsrv_etry_t *tcpsrv_root = NULL;
static int n_tcpsrv = 0;

static permittedPeers_t *pPermPeersRoot = NULL;

/* default number of workes to configure. We choose 2, as this is probably good for
 * many installations. High-Volume ones may need much higher number!
 */
#define DEFAULT_NUMWRKR 2

#define FRAMING_UNSET -1

/* config settings */
static struct configSettings_s {
	int iTCPSessMax;
	int iTCPLstnMax;
	int bSuppOctetFram;
	int iStrmDrvrMode;
	int bKeepAlive;
	int iKeepAliveIntvl;
	int iKeepAliveProbes;
	int iKeepAliveTime;
	int bEmitMsgOnClose;
	int bEmitMsgOnOpen;
	int iAddtlFrameDelim;
	int maxFrameSize;
	int bDisableLFDelim;
	int discardTruncatedMsg;
	int bUseFlowControl;
	int bPreserveCase;
	uchar *gnutlsPriorityString;
	uchar *pszStrmDrvrAuthMode;
	uchar *pszStrmDrvrPermitExpiredCerts;
	uchar *pszInputName;
	uchar *pszBindRuleset;
	uchar *lstnIP;			/* which IP we should listen on? */
	uchar *lstnPortFile;
} cs;

struct instanceConf_s {
	int iTCPSessMax;
	int iTCPLstnMax;
	unsigned numWrkr;
	tcpLstnParams_t *cnf_params;	/**< listener config parameters */
	uchar *pszBindRuleset;		/* name of ruleset to bind to */
	ruleset_t *pBindRuleset;	/* ruleset to bind listener to (use system default if unspecified) */
	uchar *pszInputName;		/* value for inputname property, NULL is OK and handled by core engine */
	uchar *dfltTZ;
	sbool bSPFramingFix;
	unsigned int ratelimitInterval;
	unsigned int ratelimitBurst;
	int iAddtlFrameDelim; /* addtl frame delimiter, e.g. for netscreen, default none */
	int maxFrameSize;
	int bUseFlowControl;
	int bDisableLFDelim;
	int discardTruncatedMsg;
	int bEmitMsgOnClose;
	int bEmitMsgOnOpen;
	int bPreserveCase;
	int iSynBacklog;
	uchar *pszStrmDrvrName; /* stream driver to use */
	int iStrmDrvrMode;
	uchar *pszStrmDrvrAuthMode;
	uchar *pszStrmDrvrPermitExpiredCerts;
	uchar *pszStrmDrvrCAFile;
	uchar *pszStrmDrvrCRLFile;
	uchar *pszStrmDrvrKeyFile;
	uchar *pszStrmDrvrCertFile;
	permittedPeers_t *pPermPeersRoot;
	uchar *gnutlsPriorityString;
	int iStrmDrvrExtendedCertCheck;
	int iStrmDrvrSANPreference;
	int iStrmTlsVerifyDepth;
	int bKeepAlive;
	int iKeepAliveIntvl;
	int iKeepAliveProbes;
	int iKeepAliveTime;
	struct instanceConf_s *next;
};


struct modConfData_s {
	rsconf_t *pConf;		/* our overall config object */
	instanceConf_t *root, *tail;
	int iTCPSessMax; /* max number of sessions */
	int iTCPLstnMax; /* max number of sessions */
	unsigned numWrkr;
	int iStrmDrvrMode; /* mode for stream driver, driver-dependent (0 mostly means plain tcp) */
	int iStrmDrvrExtendedCertCheck; /* verify also purpose OID in certificate extended field */
	int iStrmDrvrSANPreference; /* ignore CN when any SAN set */
	int iStrmTlsVerifyDepth; /**< Verify Depth for certificate chains */
	int iAddtlFrameDelim; /* addtl frame delimiter, e.g. for netscreen, default none */
	int maxFrameSize;
	int bSuppOctetFram;
	sbool bDisableLFDelim; /* disable standard LF delimiter */
	sbool discardTruncatedMsg;
	sbool bUseFlowControl; /* use flow control, what means indicate ourselfs a "light delayable" */
	sbool bKeepAlive;
	int iKeepAliveIntvl;
	int iKeepAliveProbes;
	int iKeepAliveTime;
	sbool bEmitMsgOnClose; /* emit an informational message on close by remote peer */
	sbool bEmitMsgOnOpen; /* emit an informational message on close by remote peer */
	uchar *gnutlsPriorityString;
	uchar *pszStrmDrvrName; /* stream driver to use */
	uchar *pszStrmDrvrAuthMode; /* authentication mode to use */
	uchar *pszStrmDrvrPermitExpiredCerts; /* control how to handly expired certificates */
	uchar *pszStrmDrvrCAFile;
	uchar *pszStrmDrvrCRLFile;
	uchar *pszStrmDrvrKeyFile;
	uchar *pszStrmDrvrCertFile;
	permittedPeers_t *pPermPeersRoot;
	sbool configSetViaV2Method;
	sbool bPreserveCase; /* preserve case of fromhost; true by default */
};

static modConfData_t *loadModConf = NULL;/* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL;/* modConf ptr to use for the current load process */

/* module-global parameters */
static struct cnfparamdescr modpdescr[] = {
	{ "flowcontrol", eCmdHdlrBinary, 0 },
	{ "disablelfdelimiter", eCmdHdlrBinary, 0 },
	{ "discardtruncatedmsg", eCmdHdlrBinary, 0 },
	{ "octetcountedframing", eCmdHdlrBinary, 0 },
	{ "notifyonconnectionclose", eCmdHdlrBinary, 0 },
	{ "notifyonconnectionopen", eCmdHdlrBinary, 0 },
	{ "addtlframedelimiter", eCmdHdlrNonNegInt, 0 },
	{ "maxframesize", eCmdHdlrInt, 0 },
	{ "maxsessions", eCmdHdlrPositiveInt, 0 },
	{ "maxlistners", eCmdHdlrPositiveInt, 0 },
	{ "maxlisteners", eCmdHdlrPositiveInt, 0 },
	{ "workerthreads", eCmdHdlrPositiveInt, 0 },
	{ "streamdriver.mode", eCmdHdlrNonNegInt, 0 },
	{ "streamdriver.authmode", eCmdHdlrString, 0 },
	{ "streamdriver.permitexpiredcerts", eCmdHdlrString, 0 },
	{ "streamdriver.name", eCmdHdlrString, 0 },
	{ "streamdriver.CheckExtendedKeyPurpose", eCmdHdlrBinary, 0 },
	{ "streamdriver.PrioritizeSAN", eCmdHdlrBinary, 0 },
	{ "streamdriver.TlsVerifyDepth", eCmdHdlrPositiveInt, 0 },
	{ "streamdriver.cafile", eCmdHdlrString, 0 },
	{ "streamdriver.crlfile", eCmdHdlrString, 0 },
	{ "streamdriver.keyfile", eCmdHdlrString, 0 },
	{ "streamdriver.certfile", eCmdHdlrString, 0 },
	{ "permittedpeer", eCmdHdlrArray, 0 },
	{ "keepalive", eCmdHdlrBinary, 0 },
	{ "keepalive.probes", eCmdHdlrNonNegInt, 0 },
	{ "keepalive.time", eCmdHdlrNonNegInt, 0 },
	{ "keepalive.interval", eCmdHdlrNonNegInt, 0 },
	{ "gnutlsprioritystring", eCmdHdlrString, 0 },
	{ "preservecase", eCmdHdlrBinary, 0 }
};
static struct cnfparamblk modpblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(modpdescr)/sizeof(struct cnfparamdescr),
	  modpdescr
	};

/* input instance parameters */
static struct cnfparamdescr inppdescr[] = {
	{ "port", eCmdHdlrString, CNFPARAM_REQUIRED }, /* legacy: InputTCPServerRun */
	{ "maxsessions", eCmdHdlrPositiveInt, 0 },
	{ "maxlisteners", eCmdHdlrPositiveInt, 0 },
	{ "workerthreads", eCmdHdlrPositiveInt, 0 },
	{ "flowcontrol", eCmdHdlrBinary, 0 },
	{ "disablelfdelimiter", eCmdHdlrBinary, 0 },
	{ "discardtruncatedmsg", eCmdHdlrBinary, 0 },
	{ "notifyonconnectionclose", eCmdHdlrBinary, 0 },
	{ "notifyonconnectionopen", eCmdHdlrBinary, 0 },
	{ "addtlframedelimiter", eCmdHdlrNonNegInt, 0 },
	{ "maxframesize", eCmdHdlrInt, 0 },
	{ "preservecase", eCmdHdlrBinary, 0 },
	{ "listenportfilename", eCmdHdlrString, 0 },
	{ "address", eCmdHdlrString, 0 },
	{ "name", eCmdHdlrString, 0 },
	{ "defaulttz", eCmdHdlrString, 0 },
	{ "ruleset", eCmdHdlrString, 0 },
	{ "streamdriver.mode", eCmdHdlrNonNegInt, 0 },
	{ "streamdriver.authmode", eCmdHdlrString, 0 },
	{ "streamdriver.permitexpiredcerts", eCmdHdlrString, 0 },
	{ "streamdriver.name", eCmdHdlrString, 0 },
	{ "streamdriver.CheckExtendedKeyPurpose", eCmdHdlrBinary, 0 },
	{ "streamdriver.PrioritizeSAN", eCmdHdlrBinary, 0 },
	{ "streamdriver.TlsVerifyDepth", eCmdHdlrPositiveInt, 0 },
	{ "streamdriver.cafile", eCmdHdlrString, 0 },
	{ "streamdriver.crlfile", eCmdHdlrString, 0 },
	{ "streamdriver.keyfile", eCmdHdlrString, 0 },
	{ "streamdriver.certfile", eCmdHdlrString, 0 },
	{ "permittedpeer", eCmdHdlrArray, 0 },
	{ "gnutlsprioritystring", eCmdHdlrString, 0 },
	{ "keepalive", eCmdHdlrBinary, 0 },
	{ "keepalive.probes", eCmdHdlrNonNegInt, 0 },
	{ "keepalive.time", eCmdHdlrNonNegInt, 0 },
	{ "keepalive.interval", eCmdHdlrNonNegInt, 0 },
	{ "supportoctetcountedframing", eCmdHdlrBinary, 0 },
	{ "ratelimit.interval", eCmdHdlrInt, 0 },
	{ "framingfix.cisco.asa", eCmdHdlrBinary, 0 },
	{ "ratelimit.burst", eCmdHdlrInt, 0 },
	{ "socketbacklog", eCmdHdlrNonNegInt, 0 }
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
	dbgprintf("in imtcp doOpenLstnSocks\n");
	return tcpsrv.create_tcp_socket(pSrv);
}


static rsRetVal
doRcvData(tcps_sess_t *pSess, char *buf, size_t lenBuf, ssize_t *piLenRcvd, int *const oserr)
{
	assert(pSess != NULL);
	assert(piLenRcvd != NULL);
	*piLenRcvd = lenBuf;
	return netstrm.Rcv(pSess->pStrm, (uchar*) buf, piLenRcvd, oserr);
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


/* create input instance, set default parameters, and
 * add it to the list of instances.
 */
static rsRetVal
createInstance(instanceConf_t **pinst)
{
	instanceConf_t *inst = NULL;

	DEFiRet;
	CHKmalloc(inst = (instanceConf_t*) calloc(1, sizeof(instanceConf_t)));
	CHKmalloc(inst->cnf_params = (tcpLstnParams_t*) calloc(1, sizeof(tcpLstnParams_t)));
	inst->next = NULL;
	inst->pszBindRuleset = NULL;
	inst->pszInputName = NULL;
	inst->dfltTZ = NULL;
	inst->cnf_params->bSuppOctetFram = -1; /* unset */
	inst->bSPFramingFix = 0;
	inst->ratelimitInterval = 0;
	inst->ratelimitBurst = 10000;

	inst->pszStrmDrvrName = NULL;
	inst->pszStrmDrvrAuthMode = NULL;
	inst->pszStrmDrvrPermitExpiredCerts = NULL;
	inst->pszStrmDrvrCAFile = NULL;
	inst->pszStrmDrvrCRLFile = NULL;
	inst->pszStrmDrvrKeyFile = NULL;
	inst->pszStrmDrvrCertFile = NULL;
	inst->pPermPeersRoot = NULL;
	inst->gnutlsPriorityString = NULL;
	inst->iStrmDrvrMode = loadModConf->iStrmDrvrMode;
	inst->iStrmDrvrExtendedCertCheck = loadModConf->iStrmDrvrExtendedCertCheck;
	inst->iStrmDrvrSANPreference = loadModConf->iStrmDrvrSANPreference;
	inst->iStrmTlsVerifyDepth = loadModConf->iStrmTlsVerifyDepth;
	inst->bKeepAlive = loadModConf->bKeepAlive;
	inst->iKeepAliveIntvl = loadModConf->iKeepAliveIntvl;
	inst->iKeepAliveProbes = loadModConf->iKeepAliveProbes;
	inst->iKeepAliveTime = loadModConf->iKeepAliveTime;
	inst->iAddtlFrameDelim = loadModConf->iAddtlFrameDelim;
	inst->maxFrameSize = loadModConf->maxFrameSize;
	inst->bUseFlowControl = loadModConf->bUseFlowControl;
	inst->bDisableLFDelim = loadModConf->bDisableLFDelim;
	inst->discardTruncatedMsg = loadModConf->discardTruncatedMsg;
	inst->bEmitMsgOnClose = loadModConf->bEmitMsgOnClose;
	inst->bEmitMsgOnOpen = loadModConf->bEmitMsgOnOpen;
	inst->bPreserveCase = loadModConf->bPreserveCase;
	inst->iSynBacklog = 0; /* default: unset */
	inst->iTCPLstnMax = loadModConf->iTCPLstnMax;
	inst->iTCPSessMax = loadModConf->iTCPSessMax;
	inst->numWrkr = loadModConf->numWrkr;

	inst->cnf_params->pszLstnPortFileName = NULL;

	/* node created, let's add to config */
	if(loadModConf->tail == NULL) {
		loadModConf->tail = loadModConf->root = inst;
	} else {
		loadModConf->tail->next = inst;
		loadModConf->tail = inst;
	}

	*pinst = inst;
finalize_it:
	if(iRet != RS_RET_OK) {
		free(inst->cnf_params);
		free(inst);
	}
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

	CHKmalloc(inst->cnf_params->pszPort = ustrdup((pNewVal == NULL || *pNewVal == '\0')
				 	       ? (uchar*) "10514" : pNewVal));
	if((cs.pszBindRuleset == NULL) || (cs.pszBindRuleset[0] == '\0')) {
		inst->pszBindRuleset = NULL;
	} else {
		CHKmalloc(inst->pszBindRuleset = ustrdup(cs.pszBindRuleset));
	}
	if((cs.lstnIP == NULL) || (cs.lstnIP[0] == '\0')) {
		inst->cnf_params->pszAddr = NULL;
	} else {
		CHKmalloc(inst->cnf_params->pszAddr = ustrdup(cs.lstnIP));
	}
	if((cs.lstnPortFile == NULL) || (cs.lstnPortFile[0] == '\0')) {
		inst->cnf_params->pszLstnPortFileName = NULL;
	} else {
		CHKmalloc(inst->cnf_params->pszLstnPortFileName = ustrdup(cs.lstnPortFile));
	}

	if((cs.pszInputName == NULL) || (cs.pszInputName[0] == '\0')) {
		inst->pszInputName = NULL;
	} else {
		CHKmalloc(inst->pszInputName = ustrdup(cs.pszInputName));
	}
	inst->cnf_params->bSuppOctetFram = cs.bSuppOctetFram;
	inst->iStrmDrvrMode = cs.iStrmDrvrMode;
	inst->bKeepAlive = cs.bKeepAlive ;
	inst->bUseFlowControl = cs.bUseFlowControl;
	inst->bDisableLFDelim = cs.bDisableLFDelim;
	inst->bEmitMsgOnClose = cs.bEmitMsgOnClose;
	inst->bPreserveCase = cs.bPreserveCase;
	inst->iKeepAliveProbes = cs.iKeepAliveProbes;
	inst->iKeepAliveIntvl = cs.iKeepAliveIntvl;
	inst->iKeepAliveTime = cs.iKeepAliveTime;
	inst->iKeepAliveTime = cs.iKeepAliveTime;
	inst->iAddtlFrameDelim = cs.iAddtlFrameDelim;
	inst->iTCPLstnMax = cs.iTCPLstnMax;
	inst->iTCPSessMax = cs.iTCPSessMax;
	inst->numWrkr = DEFAULT_NUMWRKR;
	inst->iStrmDrvrMode = cs.iStrmDrvrMode;

finalize_it:
	free(pNewVal);
	RETiRet;
}


static rsRetVal
addListner(modConfData_t *modConf, instanceConf_t *inst)
{
	DEFiRet;
	uchar *psz;	/* work variable */
	permittedPeers_t *peers;

	tcpsrv_t *pOurTcpsrv;
	CHKiRet(tcpsrv.Construct(&pOurTcpsrv));
	/* callbacks */
	CHKiRet(tcpsrv.SetCBIsPermittedHost(pOurTcpsrv, isPermittedHost));
	CHKiRet(tcpsrv.SetCBRcvData(pOurTcpsrv, doRcvData));
	CHKiRet(tcpsrv.SetCBOpenLstnSocks(pOurTcpsrv, doOpenLstnSocks));
	CHKiRet(tcpsrv.SetCBOnRegularClose(pOurTcpsrv, onRegularClose));
	CHKiRet(tcpsrv.SetCBOnErrClose(pOurTcpsrv, onErrClose));
	/* params */
	CHKiRet(tcpsrv.SetNumWrkr(pOurTcpsrv, inst->numWrkr));
	CHKiRet(tcpsrv.SetKeepAlive(pOurTcpsrv, inst->bKeepAlive));
	CHKiRet(tcpsrv.SetKeepAliveIntvl(pOurTcpsrv, inst->iKeepAliveIntvl));
	CHKiRet(tcpsrv.SetKeepAliveProbes(pOurTcpsrv, inst->iKeepAliveProbes));
	CHKiRet(tcpsrv.SetKeepAliveTime(pOurTcpsrv, inst->iKeepAliveTime));
	CHKiRet(tcpsrv.SetSessMax(pOurTcpsrv, inst->iTCPSessMax));
	CHKiRet(tcpsrv.SetLstnMax(pOurTcpsrv, inst->iTCPLstnMax));
	CHKiRet(tcpsrv.SetDrvrMode(pOurTcpsrv, inst->iStrmDrvrMode));
	CHKiRet(tcpsrv.SetDrvrCheckExtendedKeyUsage(pOurTcpsrv, inst->iStrmDrvrExtendedCertCheck));
	CHKiRet(tcpsrv.SetDrvrPrioritizeSAN(pOurTcpsrv, inst->iStrmDrvrSANPreference));
	CHKiRet(tcpsrv.SetDrvrTlsVerifyDepth(pOurTcpsrv, inst->iStrmTlsVerifyDepth));
	CHKiRet(tcpsrv.SetUseFlowControl(pOurTcpsrv, inst->bUseFlowControl));
	CHKiRet(tcpsrv.SetAddtlFrameDelim(pOurTcpsrv, inst->iAddtlFrameDelim));
	CHKiRet(tcpsrv.SetMaxFrameSize(pOurTcpsrv, inst->maxFrameSize));
	CHKiRet(tcpsrv.SetbDisableLFDelim(pOurTcpsrv, inst->bDisableLFDelim));
	CHKiRet(tcpsrv.SetDiscardTruncatedMsg(pOurTcpsrv, inst->discardTruncatedMsg));
	CHKiRet(tcpsrv.SetNotificationOnRemoteClose(pOurTcpsrv, inst->bEmitMsgOnClose));
	CHKiRet(tcpsrv.SetNotificationOnRemoteOpen(pOurTcpsrv, inst->bEmitMsgOnOpen));
	CHKiRet(tcpsrv.SetPreserveCase(pOurTcpsrv, inst->bPreserveCase));
	CHKiRet(tcpsrv.SetSynBacklog(pOurTcpsrv, inst->iSynBacklog));
	/* now set optional params, but only if they were actually configured */
	psz = (inst->pszStrmDrvrName == NULL) ? modConf->pszStrmDrvrName : inst->pszStrmDrvrName;
	if(psz != NULL) {
		CHKiRet(tcpsrv.SetDrvrName(pOurTcpsrv, psz));
	}
	psz = (inst->pszStrmDrvrAuthMode == NULL) ? modConf->pszStrmDrvrAuthMode : inst->pszStrmDrvrAuthMode;
	if(psz != NULL) {
		CHKiRet(tcpsrv.SetDrvrAuthMode(pOurTcpsrv, psz));
	}
	psz = (inst->gnutlsPriorityString == NULL)
			? modConf->gnutlsPriorityString : inst->gnutlsPriorityString;
	CHKiRet(tcpsrv.SetGnutlsPriorityString(pOurTcpsrv, psz));
	/* Call SetDrvrPermitExpiredCerts required
	 * when param is NULL default handling for ExpiredCerts is set! */
	psz = (inst->pszStrmDrvrPermitExpiredCerts == NULL)
			? modConf->pszStrmDrvrPermitExpiredCerts : inst->pszStrmDrvrPermitExpiredCerts;
	CHKiRet(tcpsrv.SetDrvrPermitExpiredCerts(pOurTcpsrv, psz));

	psz = (inst->pszStrmDrvrCAFile == NULL)
			? modConf->pszStrmDrvrCAFile : inst->pszStrmDrvrCAFile;
	CHKiRet(tcpsrv.SetDrvrCAFile(pOurTcpsrv, psz));

	psz = (inst->pszStrmDrvrCRLFile == NULL)
			? modConf->pszStrmDrvrCRLFile : inst->pszStrmDrvrCRLFile;
	CHKiRet(tcpsrv.SetDrvrCRLFile(pOurTcpsrv, psz));

	psz = (inst->pszStrmDrvrKeyFile == NULL)
			? modConf->pszStrmDrvrKeyFile : inst->pszStrmDrvrKeyFile;
	CHKiRet(tcpsrv.SetDrvrKeyFile(pOurTcpsrv, psz));

	psz = (inst->pszStrmDrvrCertFile == NULL)
			? modConf->pszStrmDrvrCertFile : inst->pszStrmDrvrCertFile;
	CHKiRet(tcpsrv.SetDrvrCertFile(pOurTcpsrv, psz));

	peers = (inst->pPermPeersRoot == NULL)
			? modConf->pPermPeersRoot : inst->pPermPeersRoot;
	if(peers != NULL) {
			CHKiRet(tcpsrv.SetDrvrPermPeers(pOurTcpsrv, peers));
	}

	/* initialized, now add socket and listener params */
	DBGPRINTF("imtcp: trying to add port *:%s\n", inst->cnf_params->pszPort);
	inst->cnf_params->pRuleset = inst->pBindRuleset;
	CHKiRet(tcpsrv.SetInputName(pOurTcpsrv, inst->cnf_params, inst->pszInputName == NULL ?
						UCHAR_CONSTANT("imtcp") : inst->pszInputName));
	CHKiRet(tcpsrv.SetOrigin(pOurTcpsrv, (uchar*)"imtcp"));
	CHKiRet(tcpsrv.SetDfltTZ(pOurTcpsrv, (inst->dfltTZ == NULL) ? (uchar*)"" : inst->dfltTZ));
	CHKiRet(tcpsrv.SetbSPFramingFix(pOurTcpsrv, inst->bSPFramingFix));
	CHKiRet(tcpsrv.SetLinuxLikeRatelimiters(pOurTcpsrv, inst->ratelimitInterval, inst->ratelimitBurst));

	if((ustrcmp(inst->cnf_params->pszPort, UCHAR_CONSTANT("0")) == 0
		&& inst->cnf_params->pszLstnPortFileName == NULL)
			|| ustrcmp(inst->cnf_params->pszPort, UCHAR_CONSTANT("0")) < 0) {
		LogMsg(0, RS_RET_OK, LOG_WARNING, "imtcp: port 0 and no port file set -> using port 514 instead");
		CHKmalloc(inst->cnf_params->pszPort = (uchar*)strdup("514"));
	}
	tcpsrv.configureTCPListen(pOurTcpsrv, inst->cnf_params);

	tcpsrv_etry_t *etry;
	CHKmalloc(etry = (tcpsrv_etry_t*) calloc(1, sizeof(tcpsrv_etry_t)));
	etry->tcpsrv = pOurTcpsrv;
	etry->next = tcpsrv_root;
	tcpsrv_root = etry;
	++n_tcpsrv;

finalize_it:
	if(iRet != RS_RET_OK) {
		LogError(0, NO_ERRCODE, "imtcp: error %d trying to add listener", iRet);
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
		LogError(0, RS_RET_MISSING_CNFPARAMS,
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
			inst->cnf_params->pszPort = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(inppblk.descr[i].name, "address")) {
			inst->cnf_params->pszAddr = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(inppblk.descr[i].name, "name")) {
			inst->pszInputName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(inppblk.descr[i].name, "defaulttz")) {
			inst->dfltTZ = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(inppblk.descr[i].name, "framingfix.cisco.asa")) {
			inst->bSPFramingFix = (int) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "ruleset")) {
			inst->pszBindRuleset = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(inppblk.descr[i].name, "streamdriver.mode")) {
			inst->iStrmDrvrMode = (int) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "streamdriver.CheckExtendedKeyPurpose")) {
			inst->iStrmDrvrExtendedCertCheck = (int) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "streamdriver.PrioritizeSAN")) {
			inst->iStrmDrvrSANPreference = (int) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "streamdriver.TlsVerifyDepth")) {
			if (pvals[i].val.d.n >= 2) {
				inst->iStrmTlsVerifyDepth = (int) pvals[i].val.d.n;
			} else {
				parser_errmsg("streamdriver.TlsVerifyDepth must be 2 or higher but is %d",
									(int) pvals[i].val.d.n);
			}
		} else if(!strcmp(inppblk.descr[i].name, "streamdriver.authmode")) {
			inst->pszStrmDrvrAuthMode = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(inppblk.descr[i].name, "streamdriver.permitexpiredcerts")) {
			inst->pszStrmDrvrPermitExpiredCerts = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(inppblk.descr[i].name, "streamdriver.cafile")) {
			inst->pszStrmDrvrCAFile = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(inppblk.descr[i].name, "streamdriver.crlfile")) {
			inst->pszStrmDrvrCRLFile = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(inppblk.descr[i].name, "streamdriver.keyfile")) {
			inst->pszStrmDrvrKeyFile = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(inppblk.descr[i].name, "streamdriver.certfile")) {
			inst->pszStrmDrvrCertFile = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(inppblk.descr[i].name, "streamdriver.name")) {
			inst->pszStrmDrvrName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(inppblk.descr[i].name, "gnutlsprioritystring")) {
			inst->gnutlsPriorityString = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(inppblk.descr[i].name, "permittedpeer")) {
			for(int j = 0 ; j <  pvals[i].val.d.ar->nmemb ; ++j) {
				uchar *const peer = (uchar*) es_str2cstr(pvals[i].val.d.ar->arr[j], NULL);
				CHKiRet(net.AddPermittedPeer(&inst->pPermPeersRoot, peer));
				free(peer);
			}
		} else if(!strcmp(inppblk.descr[i].name, "flowcontrol")) {
			inst->bUseFlowControl = (int) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "disablelfdelimiter")) {
			inst->bDisableLFDelim = (int) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "discardtruncatedmsg")) {
			inst->discardTruncatedMsg = (int) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "notifyonconnectionclose")) {
			inst->bEmitMsgOnClose = (int) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "notifyonconnectionopen")) {
			inst->bEmitMsgOnOpen = (int) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "addtlframedelimiter")) {
			inst->iAddtlFrameDelim = (int) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "maxframesize")) {
			const int max = (int) pvals[i].val.d.n;
			if(max <= 200000000) {
				inst->maxFrameSize = max;
			} else {
				LogError(0, RS_RET_PARAM_ERROR, "imtcp: invalid value for 'maxFrameSize' "
						"parameter given is %d, max is 200000000", max);
				ABORT_FINALIZE(RS_RET_PARAM_ERROR);
			}
		} else if(!strcmp(inppblk.descr[i].name, "maxsessions")) {
			inst->iTCPSessMax = (int) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "maxlisteners")) {
			inst->iTCPLstnMax = (int) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "workerthreads")) {
			inst->numWrkr = (int) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "supportoctetcountedframing")) {
			inst->cnf_params->bSuppOctetFram = (int) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "keepalive")) {
			inst->bKeepAlive = (int) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "keepalive.probes")) {
			inst->iKeepAliveProbes = (int) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "keepalive.time")) {
			inst->iKeepAliveTime = (int) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "keepalive.interval")) {
			inst->iKeepAliveIntvl = (int) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "ratelimit.burst")) {
			inst->ratelimitBurst = (unsigned int) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "ratelimit.interval")) {
			inst->ratelimitInterval = (unsigned int) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "preservecase")) {
			inst->bPreserveCase = (int) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "socketbacklog")) {
			inst->iSynBacklog = (int) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "listenportfilename")) {
			inst->cnf_params->pszLstnPortFileName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
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
	loadModConf->numWrkr = DEFAULT_NUMWRKR;
	loadModConf->bSuppOctetFram = 1;
	loadModConf->iStrmDrvrMode = 0;
	loadModConf->iStrmDrvrExtendedCertCheck = 0;
	loadModConf->iStrmDrvrSANPreference = 0;
	loadModConf->iStrmTlsVerifyDepth = 0;
	loadModConf->bUseFlowControl = 1;
	loadModConf->bKeepAlive = 0;
	loadModConf->iKeepAliveIntvl = 0;
	loadModConf->iKeepAliveProbes = 0;
	loadModConf->iKeepAliveTime = 0;
	loadModConf->bEmitMsgOnClose = 0;
	loadModConf->bEmitMsgOnOpen = 0;
	loadModConf->iAddtlFrameDelim = TCPSRV_NO_ADDTL_DELIMITER;
	loadModConf->maxFrameSize = 200000;
	loadModConf->bDisableLFDelim = 0;
	loadModConf->discardTruncatedMsg = 0;
	loadModConf->gnutlsPriorityString = NULL;
	loadModConf->pszStrmDrvrName = NULL;
	loadModConf->pszStrmDrvrAuthMode = NULL;
	loadModConf->pszStrmDrvrPermitExpiredCerts = NULL;
	loadModConf->pszStrmDrvrCAFile = NULL;
	loadModConf->pszStrmDrvrCRLFile = NULL;
	loadModConf->pszStrmDrvrKeyFile = NULL;
	loadModConf->pszStrmDrvrCertFile = NULL;
	loadModConf->pPermPeersRoot = NULL;
	loadModConf->configSetViaV2Method = 0;
	loadModConf->bPreserveCase = 1; /* default to true */
	bLegacyCnfModGlobalsPermitted = 1;
	/* init legacy config variables */
	resetConfigVariables(NULL, NULL); /* dummy parameters just to fulfill interface def */
ENDbeginCnfLoad


BEGINsetModCnf
	struct cnfparamvals *pvals = NULL;
	int i;
CODESTARTsetModCnf
	pvals = nvlstGetParams(lst, &modpblk, NULL);
	if(pvals == NULL) {
		LogError(0, RS_RET_MISSING_CNFPARAMS, "imtcp: error processing module "
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
		} else if(!strcmp(modpblk.descr[i].name, "discardtruncatedmsg")) {
			loadModConf->discardTruncatedMsg = (int) pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "octetcountedframing")) {
			loadModConf->bSuppOctetFram = (int) pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "notifyonconnectionclose")) {
			loadModConf->bEmitMsgOnClose = (int) pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "notifyonconnectionopen")) {
			loadModConf->bEmitMsgOnOpen = (int) pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "addtlframedelimiter")) {
			loadModConf->iAddtlFrameDelim = (int) pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "maxframesize")) {
			const int max = (int) pvals[i].val.d.n;
			if(max <= 200000000) {
				loadModConf->maxFrameSize = max;
			} else {
				LogError(0, RS_RET_PARAM_ERROR, "imtcp: invalid value for 'maxFrameSize' "
						"parameter given is %d, max is 200000000", max);
				ABORT_FINALIZE(RS_RET_PARAM_ERROR);
			}
		} else if(!strcmp(modpblk.descr[i].name, "maxsessions")) {
			loadModConf->iTCPSessMax = (int) pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "maxlisteners") ||
			  !strcmp(modpblk.descr[i].name, "maxlistners")) { /* keep old name for a while */
			loadModConf->iTCPLstnMax = (int) pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "workerthreads")) {
			loadModConf->numWrkr = (int) pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "keepalive")) {
			loadModConf->bKeepAlive = (int) pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "keepalive.probes")) {
			loadModConf->iKeepAliveProbes = (int) pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "keepalive.time")) {
			loadModConf->iKeepAliveTime = (int) pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "keepalive.interval")) {
			loadModConf->iKeepAliveIntvl = (int) pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "gnutlsprioritystring")) {
			loadModConf->gnutlsPriorityString = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(modpblk.descr[i].name, "streamdriver.mode")) {
			loadModConf->iStrmDrvrMode = (int) pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "streamdriver.CheckExtendedKeyPurpose")) {
			loadModConf->iStrmDrvrExtendedCertCheck = (int) pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "streamdriver.PrioritizeSAN")) {
			loadModConf->iStrmDrvrSANPreference = (int) pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "streamdriver.TlsVerifyDepth")) {
			if (pvals[i].val.d.n >= 2) {
				loadModConf->iStrmTlsVerifyDepth = (int) pvals[i].val.d.n;
			} else {
				parser_errmsg("streamdriver.TlsVerifyDepth must be 2 or higher but is %d",
									(int) pvals[i].val.d.n);
			}
		} else if(!strcmp(modpblk.descr[i].name, "streamdriver.authmode")) {
			loadModConf->pszStrmDrvrAuthMode = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(modpblk.descr[i].name, "streamdriver.permitexpiredcerts")) {
			loadModConf->pszStrmDrvrPermitExpiredCerts = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(modpblk.descr[i].name, "streamdriver.cafile")) {
			loadModConf->pszStrmDrvrCAFile = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(modpblk.descr[i].name, "streamdriver.crlfile")) {
			loadModConf->pszStrmDrvrCRLFile = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(modpblk.descr[i].name, "streamdriver.keyfile")) {
			loadModConf->pszStrmDrvrKeyFile = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(modpblk.descr[i].name, "streamdriver.certfile")) {
			loadModConf->pszStrmDrvrCertFile = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(modpblk.descr[i].name, "streamdriver.name")) {
			loadModConf->pszStrmDrvrName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(modpblk.descr[i].name, "permittedpeer")) {
			for(int j = 0 ; j <  pvals[i].val.d.ar->nmemb ; ++j) {
				uchar *const peer = (uchar*) es_str2cstr(pvals[i].val.d.ar->arr[j], NULL);
				CHKiRet(net.AddPermittedPeer(&loadModConf->pPermPeersRoot, peer));
				free(peer);
			}
		} else if(!strcmp(modpblk.descr[i].name, "preservecase")) {
			loadModConf->bPreserveCase = (int) pvals[i].val.d.n;
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
		pModConf->maxFrameSize = cs.maxFrameSize;
		pModConf->bDisableLFDelim = cs.bDisableLFDelim;
		pModConf->bUseFlowControl = cs.bUseFlowControl;
		pModConf->bKeepAlive = cs.bKeepAlive;
		pModConf->iKeepAliveProbes = cs.iKeepAliveProbes;
		pModConf->iKeepAliveIntvl = cs.iKeepAliveIntvl;
		pModConf->iKeepAliveTime = cs.iKeepAliveTime;
		if(pPermPeersRoot != NULL) {
			assert(pModConf->pPermPeersRoot == NULL);
			pModConf->pPermPeersRoot = pPermPeersRoot;
			pPermPeersRoot = NULL; /* memory handed over! */
		}
		if((cs.pszStrmDrvrAuthMode == NULL) || (cs.pszStrmDrvrAuthMode[0] == '\0')) {
			loadModConf->pszStrmDrvrAuthMode = NULL;
		} else {
			loadModConf->pszStrmDrvrAuthMode = cs.pszStrmDrvrAuthMode;
			cs.pszStrmDrvrAuthMode = NULL;
		}
		pModConf->bPreserveCase = cs.bPreserveCase;
	}
	free(cs.pszStrmDrvrAuthMode);
	cs.pszStrmDrvrAuthMode = NULL;

	loadModConf = NULL; /* done loading */
ENDendCnfLoad


/* function to generate error message if framework does not find requested ruleset */
static inline void
std_checkRuleset_genErrMsg(__attribute__((unused)) modConfData_t *modConf, instanceConf_t *inst)
{
	LogError(0, NO_ERRCODE, "imtcp: ruleset '%s' for port %s not found - "
			"using default ruleset instead", inst->pszBindRuleset,
			inst->cnf_params->pszPort);
}

BEGINcheckCnf
	instanceConf_t *inst;
CODESTARTcheckCnf
	for(inst = pModConf->root ; inst != NULL ; inst = inst->next) {
		std_checkRuleset(pModConf, inst);
		if(inst->cnf_params->bSuppOctetFram == FRAMING_UNSET)
			inst->cnf_params->bSuppOctetFram = pModConf->bSuppOctetFram;
	}
	if(pModConf->root == NULL) {
		LogError(0, RS_RET_NO_LISTNERS , "imtcp: module loaded, but "
				"no listeners defined - no input will be gathered");
		iRet = RS_RET_NO_LISTNERS;
	}
ENDcheckCnf


BEGINactivateCnfPrePrivDrop
	instanceConf_t *inst;
CODESTARTactivateCnfPrePrivDrop
	runModConf = pModConf;
	for(inst = runModConf->root ; inst != NULL ; inst = inst->next) {
		addListner(runModConf, inst);
	}
	if(tcpsrv_root == NULL)
		ABORT_FINALIZE(RS_RET_NO_RUN);
	tcpsrv_etry_t *etry = tcpsrv_root;
	while(etry != NULL) {
		CHKiRet(tcpsrv.ConstructFinalize(etry->tcpsrv));
		etry = etry->next;
	}
finalize_it:
ENDactivateCnfPrePrivDrop


BEGINactivateCnf
CODESTARTactivateCnf
	/* sorry, nothing to do here... */
ENDactivateCnf


BEGINfreeCnf
	instanceConf_t *inst, *del;
CODESTARTfreeCnf
	free(pModConf->gnutlsPriorityString);
	free(pModConf->pszStrmDrvrName);
	free(pModConf->pszStrmDrvrAuthMode);
	free(pModConf->pszStrmDrvrPermitExpiredCerts);
	free(pModConf->pszStrmDrvrCAFile);
	free(pModConf->pszStrmDrvrCRLFile);
	free(pModConf->pszStrmDrvrKeyFile);
	free(pModConf->pszStrmDrvrCertFile);
	if(pModConf->pPermPeersRoot != NULL) {
		net.DestructPermittedPeers(&pModConf->pPermPeersRoot);
	}

	for(inst = pModConf->root ; inst != NULL ; ) {
		free((void*)inst->pszBindRuleset);
		free((void*)inst->pszStrmDrvrAuthMode);
		free((void*)inst->pszStrmDrvrName);
		free((void*)inst->pszStrmDrvrPermitExpiredCerts);
		free((void*)inst->pszStrmDrvrCAFile);
		free((void*)inst->pszStrmDrvrCRLFile);
		free((void*)inst->pszStrmDrvrKeyFile);
		free((void*)inst->pszStrmDrvrCertFile);
		free((void*)inst->gnutlsPriorityString);
		free((void*)inst->pszInputName);
		free((void*)inst->dfltTZ);
		if(inst->pPermPeersRoot != NULL) {
			net.DestructPermittedPeers(&inst->pPermPeersRoot);
		}
		del = inst;
		inst = inst->next;
		free(del);
	}
ENDfreeCnf

static void *
RunServerThread(void *myself)
{
	tcpsrv_etry_t *const etry = (tcpsrv_etry_t*) myself;
	rsRetVal iRet;
	iRet = tcpsrv.Run(etry->tcpsrv);
	if(iRet != RS_RET_OK) {
		LogError(0, iRet, "imtcp: error while terminating server; rsyslog may hang on shutdown");
	}
	return NULL;
}


/* support for running multiple servers on multiple threads (one server per thread) */
static void
startSrvWrkr(tcpsrv_etry_t *const etry)
{
	int r;
	pthread_attr_t sessThrdAttr;

	/* We need to temporarily block all signals because the new thread
	 * inherits our signal mask. There is a race if we do not block them
	 * now, and we have seen in practice that this race causes grief.
	 * So we 1. save the current set, 2. block evertyhing, 3. start
	 * threads, and 4 reset the current set to saved state.
	 * rgerhards, 2019-08-16
	 */
	sigset_t sigSet, sigSetSave;
	sigfillset(&sigSet);
	/* enable signals we still need */
	sigdelset(&sigSet, SIGTTIN);
	sigdelset(&sigSet, SIGSEGV);
	pthread_sigmask(SIG_SETMASK, &sigSet, &sigSetSave);

	pthread_attr_init(&sessThrdAttr);
	pthread_attr_setstacksize(&sessThrdAttr, 4096*1024);
	r = pthread_create(&etry->tid, &sessThrdAttr, RunServerThread, etry);
	if(r != 0) {
		LogError(r, NO_ERRCODE, "imtcp error creating server thread");
		/* we do NOT abort, as other servers may run - after all, we logged an error */
	}
	pthread_attr_destroy(&sessThrdAttr);
	pthread_sigmask(SIG_SETMASK, &sigSetSave, NULL);
}

/* stop server worker thread
 */
static void
stopSrvWrkr(tcpsrv_etry_t *const etry)
{
	DBGPRINTF("Wait for thread shutdown etry %p\n", etry);
	pthread_kill(etry->tid, SIGTTIN);
	pthread_join(etry->tid, NULL);
	DBGPRINTF("input %p terminated\n", etry);
}

/* This function is called to gather input.
 */
BEGINrunInput
CODESTARTrunInput
	tcpsrv_etry_t *etry = tcpsrv_root->next;
	while(etry != NULL) {
		startSrvWrkr(etry);
		etry = etry->next;
	}

	iRet = tcpsrv.Run(tcpsrv_root->tcpsrv);

	/* de-init remaining servers */
	etry = tcpsrv_root->next;
	while(etry != NULL) {
		stopSrvWrkr(etry);
		etry = etry->next;
	}
ENDrunInput


/* initialize and return if will run or not */
BEGINwillRun
CODESTARTwillRun
	net.PrintAllowedSenders(2); /* TCP */
ENDwillRun


BEGINafterRun
CODESTARTafterRun
	tcpsrv_etry_t *etry = tcpsrv_root;
	tcpsrv_etry_t *del;
	while(etry != NULL) {
		iRet = tcpsrv.Destruct(&etry->tcpsrv);
		// TODO: check iRet, reprot error
		del = etry;
		etry = etry->next;
		free(del);
	}
	net.clearAllowedSenders(UCHAR_CONSTANT("TCP"));
ENDafterRun


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURENonCancelInputTermination)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINmodExit
CODESTARTmodExit
	/* release objects we used */
	objRelease(net, LM_NET_FILENAME);
	objRelease(netstrm, LM_NETSTRMS_FILENAME);
	objRelease(tcps_sess, LM_TCPSRV_FILENAME);
	objRelease(tcpsrv, LM_TCPSRV_FILENAME);
	objRelease(ruleset, CORE_COMPONENT);
ENDmodExit


static rsRetVal
resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	cs.iTCPSessMax = 200;
	cs.iTCPLstnMax = 20;
	cs.bSuppOctetFram = 1;
	cs.iStrmDrvrMode = 0;
	cs.bUseFlowControl = 1;
	cs.bKeepAlive = 0;
	cs.iKeepAliveProbes = 0;
	cs.iKeepAliveTime = 0;
	cs.iKeepAliveIntvl = 0;
	cs.bEmitMsgOnClose = 0;
	cs.iAddtlFrameDelim = TCPSRV_NO_ADDTL_DELIMITER;
	cs.maxFrameSize = 200000;
	cs.bDisableLFDelim = 0;
	cs.bPreserveCase = 1;
	free(cs.pszStrmDrvrAuthMode);
	cs.pszStrmDrvrAuthMode = NULL;
	free(cs.pszInputName);
	cs.pszInputName = NULL;
	free(cs.lstnPortFile);
	cs.lstnPortFile = NULL;
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
	tcpsrv_root = NULL;
	/* request objects we use */
	CHKiRet(objUse(net, LM_NET_FILENAME));
	CHKiRet(objUse(netstrm, LM_NETSTRMS_FILENAME));
	CHKiRet(objUse(tcps_sess, LM_TCPSRV_FILENAME));
	CHKiRet(objUse(tcpsrv, LM_TCPSRV_FILENAME));
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
	CHKiRet(regCfSysLineHdlr2(UCHAR_CONSTANT("inputtcpserverkeepalive_probes"), 0, eCmdHdlrInt,
			   NULL, &cs.iKeepAliveProbes, STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
	CHKiRet(regCfSysLineHdlr2(UCHAR_CONSTANT("inputtcpserverkeepalive_intvl"), 0, eCmdHdlrInt,
			   NULL, &cs.iKeepAliveIntvl, STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
	CHKiRet(regCfSysLineHdlr2(UCHAR_CONSTANT("inputtcpserverkeepalive_time"), 0, eCmdHdlrInt,
			   NULL, &cs.iKeepAliveTime, STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
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
	CHKiRet(regCfSysLineHdlr2(UCHAR_CONSTANT("inputtcpserverpreservecase"), 1, eCmdHdlrBinary,
			   NULL, &cs.bPreserveCase, STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
	CHKiRet(regCfSysLineHdlr2(UCHAR_CONSTANT("inputtcpserverlistenportfile"), 1, eCmdHdlrGetWord,
			   NULL, &cs.lstnPortFile, STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("resetconfigvariables"), 1, eCmdHdlrCustomHandler,
				   resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit
