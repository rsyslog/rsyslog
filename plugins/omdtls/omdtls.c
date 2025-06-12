/* omdtls.c
 * The dtls output module, uses OpenSSL as library to implement DTLS.
 *
 * \author  Andre Lorbach <alorbach@adiscon.com>
 *
 * Copyright (C) 2023 Adiscon GmbH.
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
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/uio.h>
#include <sys/queue.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <math.h>
#ifdef HAVE_SYS_STAT_H
#	include <sys/stat.h>
#endif
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

// --- Include openssl headers as well
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#if OPENSSL_VERSION_NUMBER >= 0x30000000L && !defined(LIBRESSL_VERSION_NUMBER)
#	include <openssl/bioerr.h>
#endif
#ifndef OPENSSL_NO_ENGINE
#       include <openssl/engine.h>
#endif
// ---

// Include rsyslog headers
#include "rsyslog.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "glbl.h"
#include "errmsg.h"
#include "atomic.h"
#include "statsobj.h"
#include "unicode-helper.h"
#include "datetime.h"
#include "net_ossl.h"

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("omdtls")

/* internal structures
 */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(glbl)
DEFobjCurrIf(datetime)
DEFobjCurrIf(statsobj)
DEFobjCurrIf(net_ossl)

statsobj_t *dtlsStats;
STATSCOUNTER_DEF(ctrDtlsSubmit, mutCtrDtlsSubmit);
STATSCOUNTER_DEF(ctrDtlsFail, mutCtrDtlsFail);

#define DTLS_MAX_RCVBUF 8192	/* Maximum DTLS packet is 8192 bytes which fits into Jumbo Frames. If not enabled
				*  message fragmentation (Ethernet MTU of ~ 1500 bytes) can occur.*/
#define SETUP_DTLS_NONE 0
#define SETUP_DTLS_AUTOCLOSE 1

enum DTLSState {
	DTLS_CONNECTED,
	DTLS_CONNECTING,
	DTLS_DISCONNECTED
};

/* Struct for module InstanceData */
typedef struct _instanceData {
	uchar 	*tplName;	/* name of assigned template */

	uchar *target;
	uchar *port;

	uchar *tlscfgcmd;		/* OpenSSL Config Command used to override any OpenSSL Settings */
	int bHaveSess;			/* True if DTLS session is established */
	int CertVerifyDepth;		/* Verify Depth for certificate chains */

	/* OpenSSL and Config Cert vars inside net_ossl_t now */
	net_ossl_t *pNetOssl;		/* OSSL shared Config and object vars are here */

	uchar *statsName;
	statsobj_t *stats;		/* listener stats */
	STATSCOUNTER_DEF(ctrDtlsSubmit, mutCtrDtlsSubmit)
	STATSCOUNTER_DEF(ctrDtlsFail, mutCtrDtlsFail);

	struct _instanceData *next;
	struct _instanceData *prev;
} instanceData;

/* Struct for module workerInstanceData */
typedef struct wrkrInstanceData {
	instanceData *pData;

	enum DTLSState ConnectState;		/* DTLS Connection State Status */
	pthread_rwlock_t pnLock;

	// DTLS Helpers
	struct sockaddr_in dtls_client_addr;	/* local bind socket*/
	struct addrinfo* dtls_rcvr_addrinfo;	/* remote receiver address */

	int sockin;			/* UDP Socket used to receive DTLS */
	int sockout;			/* UDP Socket used to send DTLS */
	SSL *sslClient;			/* DTSL (SSL) Client */
} wrkrInstanceData_t;

#define INST_STATSCOUNTER_INC(inst, ctr, mut) \
	do { \
		if (inst->stats) { STATSCOUNTER_INC(ctr, mut); } \
	} while(0);

// QPID Proton Handler functions
static rsRetVal dtls_create_socket(wrkrInstanceData_t *const __restrict__ pWrkrData, int autoclose);
static rsRetVal dtls_init(wrkrInstanceData_t *pWrkrData);
static rsRetVal dtls_connect(wrkrInstanceData_t *pWrkrData);
static rsRetVal dtls_close(wrkrInstanceData_t *pWrkrData);
static rsRetVal dtls_send(wrkrInstanceData_t *pWrkrData, const actWrkrIParams_t *__restrict__ const pParam,
	const int iMsg);


/* tables for interfacing with the v6 config system */
/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {
	{ "target", eCmdHdlrGetWord, 0 },
	{ "port", eCmdHdlrGetWord, 0 },
	{ "tls.authmode", eCmdHdlrString, 0 },
	{ "tls.cacert", eCmdHdlrString, 0 },
	{ "tls.mycert", eCmdHdlrString, 0 },
	{ "tls.myprivkey", eCmdHdlrString, 0 },
	{ "tls.tlscfgcmd", eCmdHdlrString, 0 },
	{ "template", eCmdHdlrGetWord, 0 },
	{ "statsname", eCmdHdlrGetWord, 0 }
};
static struct cnfparamblk actpblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(actpdescr)/sizeof(struct cnfparamdescr),
	  actpdescr
	};

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
	instanceData *root, *tail;
	uchar 	*tplName;	/* default template */
};

static modConfData_t *loadModConf = NULL;	/* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL;	/* modConf ptr to use for the current exec process */

BEGINinitConfVars		/* (re)set config variables to default values */
CODESTARTinitConfVars
ENDinitConfVars

static rsRetVal
dtls_create_socket(wrkrInstanceData_t *const __restrict__ pWrkrData, int autoclose)
{
	DEFiRet;
	DBGPRINTF("omdtls[%p]: dtls_create_socket ENTER\n", pWrkrData);
	pthread_rwlock_wrlock(&pWrkrData->pnLock);

	if (autoclose == SETUP_DTLS_AUTOCLOSE) {
		// Close Existing Connection
		dtls_close(pWrkrData);
	}

	// Init & Open Connection
	CHKiRet(dtls_init(pWrkrData));
finalize_it:
	if (iRet != RS_RET_OK) {
		/* Parameter Error's cannot be resumed, so we need to disable the action */
		if (iRet == RS_RET_PARAM_ERROR) {
			iRet = RS_RET_DISABLE_ACTION;
			LogError(0, iRet, "omdtls[%p]: action will be disabled due invalid "
				"configuration parameters\n", pWrkrData);
		}
	}
	pthread_rwlock_unlock(&pWrkrData->pnLock);
	RETiRet;
}

BEGINbeginCnfLoad
CODESTARTbeginCnfLoad
	loadModConf = pModConf;
	pModConf->pConf = pConf;
	pModConf->tplName = NULL;
ENDbeginCnfLoad

BEGINsetModCnf
	int i;
CODESTARTsetModCnf
	const struct cnfparamvals *const __restrict__ pvals = nvlstGetParams(lst, &modpblk, NULL);
	if(pvals == NULL) {
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	if(Debug) {
		dbgprintf("module (global) param blk for omdtls:\n");
		cnfparamsPrint(&modpblk, pvals);
	}

	for(i = 0 ; i < modpblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(modpblk.descr[i].name, "template")) {
			loadModConf->tplName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else {
			dbgprintf("omdtls: program error, non-handled "
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
ENDendCnfLoad

BEGINcheckCnf
CODESTARTcheckCnf
ENDcheckCnf

BEGINactivateCnfPrePrivDrop
	instanceData *inst;
CODESTARTactivateCnfPrePrivDrop
	runModConf = pModConf;
	DBGPRINTF("omdtls: activateCnfPrePrivDrop\n");

	for(inst = runModConf->root ; inst != NULL ; inst = inst->next) {
		CHKiRet(net_ossl.osslCtxInit(inst->pNetOssl, DTLS_method()));
		// Run openssl config commands in Context
		CHKiRet(net_ossl.osslApplyTlscgfcmd(inst->pNetOssl, inst->tlscfgcmd));
	}
finalize_it:
ENDactivateCnfPrePrivDrop

BEGINactivateCnf
CODESTARTactivateCnf
ENDactivateCnf

BEGINfreeCnf
CODESTARTfreeCnf
	free(pModConf->tplName);
ENDfreeCnf

BEGINcreateInstance
CODESTARTcreateInstance
	DBGPRINTF("createInstance[%p]: ENTER\n", pData);
	pData->tplName = NULL;
	pData->target = NULL;
	pData->port = NULL;
	pData->tlscfgcmd = NULL;
	pData->statsName = NULL;

	/* node created, let's add to config */
	if(loadModConf->tail == NULL) {
		loadModConf->tail = loadModConf->root = pData;
	} else {
		loadModConf->tail->next = pData;
		loadModConf->tail = pData;
	}

	// Construct pNetOssl helper
	CHKiRet(net_ossl.Construct(&pData->pNetOssl));
	pData->pNetOssl->authMode = OSSL_AUTH_CERTANON;
finalize_it:
ENDcreateInstance


BEGINcreateWrkrInstance
CODESTARTcreateWrkrInstance
	DBGPRINTF("createWrkrInstance[%p]: ENTER\n", pWrkrData);
	pWrkrData->ConnectState = DTLS_DISCONNECTED;

	CHKiRet(pthread_rwlock_init(&pWrkrData->pnLock, NULL));
finalize_it:
	DBGPRINTF("createWrkrInstance[%p] returned %d\n", pWrkrData, iRet);
ENDcreateWrkrInstance


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
ENDisCompatibleWithFeature


BEGINfreeInstance
CODESTARTfreeInstance
	DBGPRINTF("freeInstance[%p]: ENTER\n", pData);

	if (pData->stats) {
		statsobj.Destruct(&pData->stats);
	}

	// DeConstruct pNetOssl helper
	net_ossl.Destruct(&pData->pNetOssl);

	/* Free other mem */
	free(pData->target);
	free(pData->port);
	free(pData->tlscfgcmd);

	free(pData->tplName);
	free(pData->statsName);
ENDfreeInstance

BEGINfreeWrkrInstance
CODESTARTfreeWrkrInstance
	DBGPRINTF("freeWrkrInstance[%p]: ENTER\n", pWrkrData);
	pthread_rwlock_wrlock(&pWrkrData->pnLock);

	// Close DTLS Connection
	dtls_close(pWrkrData);

	pthread_rwlock_unlock(&pWrkrData->pnLock);
	pthread_rwlock_destroy(&pWrkrData->pnLock);
ENDfreeWrkrInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
ENDdbgPrintInstInfo

BEGINtryResume
CODESTARTtryResume
	DBGPRINTF("omdtls[%p]: tryResume ENTER\n", pWrkrData);
	if (pWrkrData->ConnectState == DTLS_DISCONNECTED) {
		CHKiRet(dtls_create_socket(pWrkrData, SETUP_DTLS_AUTOCLOSE));
	}
finalize_it:
	DBGPRINTF("omdtls[%p]: tryResume returned %d\n", pWrkrData, iRet);
ENDtryResume

BEGINbeginTransaction
CODESTARTbeginTransaction
	/* we have nothing to do to begin a transaction */
	DBGPRINTF("omdtls[%p]: beginTransaction ENTER\n", pWrkrData);
	if (pWrkrData->ConnectState == DTLS_DISCONNECTED) {
		CHKiRet(dtls_create_socket(pWrkrData, SETUP_DTLS_NONE));
	}
finalize_it:
	RETiRet;
ENDbeginTransaction

/*
 *	New Transaction action interface
*/
BEGINcommitTransaction
	unsigned i;
	sbool bDone = 0;
CODESTARTcommitTransaction
#ifndef NDEBUG
	DBGPRINTF("omdtls[%p]: commitTransaction [%d msgs] ENTER\n", pWrkrData, nParams);
#endif
	do {
		for(i = 0 ; i < nParams ; ++i) {
			CHKiRet(dtls_send(pWrkrData, pParams, i));
		}
		bDone = 1;
	} while (bDone == 0);
finalize_it:
	if(iRet != RS_RET_OK) {
		DBGPRINTF("omdtls[%p]: commitTransaction failed with status %d\n", pWrkrData, iRet);
		pWrkrData->ConnectState = DTLS_DISCONNECTED;
		iRet = RS_RET_SUSPENDED;
	}
	DBGPRINTF("omdtls[%p]: commitTransaction [%d msgs] EXIT\n", pWrkrData, nParams);
ENDcommitTransaction

BEGINnewActInst
	struct cnfparamvals *pvals;
	int i;
	int iNumTpls;
	FILE *fp;
	DBGPRINTF("newActInst: ENTER\n");
CODESTARTnewActInst
	if((pvals = nvlstGetParams(lst, &actpblk, NULL)) == NULL) {
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	// Create new instance and set default params
	CHKiRet(createInstance(&pData));

	for(i = 0 ; i < actpblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(actpblk.descr[i].name, "target")) {
			pData->target = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "port")) {
			pData->port = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "template")) {
			pData->tplName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "statsname")) {
			pData->statsName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "tls.authmode")) {
			char* pszAuthMode = es_str2cstr(pvals[i].val.d.estr, NULL);
			if(!strcasecmp(pszAuthMode, "fingerprint"))
				pData->pNetOssl->authMode = OSSL_AUTH_CERTFINGERPRINT;
			else if(!strcasecmp(pszAuthMode, "name"))
				pData->pNetOssl->authMode = OSSL_AUTH_CERTNAME;
			else if(!strcasecmp(pszAuthMode, "certvalid"))
				pData->pNetOssl->authMode = OSSL_AUTH_CERTVALID;
			else
				pData->pNetOssl->authMode = OSSL_AUTH_CERTANON;
			free(pszAuthMode);
		} else if(!strcmp(actpblk.descr[i].name, "tls.cacert")) {
			pData->pNetOssl->pszCAFile = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
			fp = fopen((const char*)pData->pNetOssl->pszCAFile, "r");
			if(fp == NULL) {
				char errStr[1024];
				rs_strerror_r(errno, errStr, sizeof(errStr));
				LogError(0, RS_RET_NO_FILE_ACCESS,
				"error: certificate file %s couldn't be accessed: %s\n",
				pData->pNetOssl->pszCAFile, errStr);
			} else {
				fclose(fp);
			}
		} else if(!strcmp(actpblk.descr[i].name, "tls.mycert")) {
			pData->pNetOssl->pszCertFile = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
			fp = fopen((const char*)pData->pNetOssl->pszCertFile, "r");
			if(fp == NULL) {
				char errStr[1024];
				rs_strerror_r(errno, errStr, sizeof(errStr));
				LogError(0, RS_RET_NO_FILE_ACCESS,
				"error: certificate file %s couldn't be accessed: %s\n",
				pData->pNetOssl->pszCertFile, errStr);
			} else {
				fclose(fp);
			}
		} else if(!strcmp(actpblk.descr[i].name, "tls.myprivkey")) {
			pData->pNetOssl->pszKeyFile = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
			fp = fopen((const char*)pData->pNetOssl->pszKeyFile, "r");
			if(fp == NULL) {
				char errStr[1024];
				rs_strerror_r(errno, errStr, sizeof(errStr));
				LogError(0, RS_RET_NO_FILE_ACCESS,
				"error: certificate file %s couldn't be accessed: %s\n",
				pData->pNetOssl->pszKeyFile, errStr);
			} else {
				fclose(fp);
			}
		} else if(!strcmp(actpblk.descr[i].name, "tls.tlscfgcmd")) {
			pData->tlscfgcmd = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else {
			LogError(0, RS_RET_INTERNAL_ERROR,
				"omdtls: program error, non-handled param '%s'\n", actpblk.descr[i].name);
		}
	}

	/* check if no port is set. If so, we use DEFAULT of 433 */
	if(pData->port == NULL) {
		CHKmalloc(pData->port = (uchar *)strdup("443"));
	}

	iNumTpls = 1;
	CODE_STD_STRING_REQUESTnewActInst(iNumTpls);

	CHKiRet(OMSRsetEntry(*ppOMSR, 0, (uchar*)strdup((pData->tplName == NULL) ?
						"RSYSLOG_FileFormat" : (char*)pData->tplName),
						OMSR_NO_RQD_TPL_OPTS));

	if (pData->statsName) {
		CHKiRet(statsobj.Construct(&pData->stats));
		CHKiRet(statsobj.SetName(pData->stats, (uchar *)pData->statsName));
		CHKiRet(statsobj.SetOrigin(pData->stats, (uchar *)"omdtls"));

		/* Track following stats */
		STATSCOUNTER_INIT(pData->ctrDtlsSubmit, pData->mutCtrDtlsSubmit);
		CHKiRet(statsobj.AddCounter(pData->stats, (uchar *)"submitted",
			ctrType_IntCtr, CTR_FLAG_RESETTABLE, &pData->ctrDtlsSubmit));
		STATSCOUNTER_INIT(pData->ctrDtlsFail, pData->mutCtrDtlsFail);
		CHKiRet(statsobj.AddCounter(pData->stats, (uchar *)"failures",
			ctrType_IntCtr, CTR_FLAG_RESETTABLE, &pData->ctrDtlsFail));
		CHKiRet(statsobj.ConstructFinalize(pData->stats));
	}
CODE_STD_FINALIZERnewActInst
	cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst


BEGINmodExit
CODESTARTmodExit
	DBGPRINTF("modExit: ENTER\n");
	statsobj.Destruct(&dtlsStats);
	objRelease(net_ossl, LM_NET_OSSL_FILENAME);
	objRelease(statsobj, CORE_COMPONENT);
	objRelease(datetime, CORE_COMPONENT);
	objRelease(glbl, CORE_COMPONENT);
ENDmodExit

NO_LEGACY_CONF_parseSelectorAct
BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMODTX_QUERIES
CODEqueryEtryPt_STD_OMOD8_QUERIES
CODEqueryEtryPt_STD_CONF2_QUERIES
CODEqueryEtryPt_STD_CONF2_setModCnf_QUERIES
CODEqueryEtryPt_STD_CONF2_PREPRIVDROP_QUERIES
CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	DBGPRINTF("modInit: ENTER\n");
INITLegCnfVars
	*ipIFVersProvided = CURR_MOD_IF_VERSION;
CODEmodInit_QueryRegCFSLineHdlr
	/* request objects we use */
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(net_ossl, LM_NET_OSSL_FILENAME));
	CHKiRet(objUse(datetime, CORE_COMPONENT));
	CHKiRet(objUse(statsobj, CORE_COMPONENT));

	CHKiRet(statsobj.Construct(&dtlsStats));
	CHKiRet(statsobj.SetName(dtlsStats, (uchar *)"omdtls"));
	CHKiRet(statsobj.SetOrigin(dtlsStats, (uchar*)"omdtls"));
	STATSCOUNTER_INIT(ctrDtlsSubmit, mutCtrDtlsSubmit);
	CHKiRet(statsobj.AddCounter(dtlsStats, (uchar *)"submitted",
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrDtlsSubmit));
	STATSCOUNTER_INIT(ctrDtlsFail, mutCtrDtlsFail);
	CHKiRet(statsobj.AddCounter(dtlsStats, (uchar *)"failures",
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrDtlsFail));

	CHKiRet(statsobj.ConstructFinalize(dtlsStats));
ENDmodInit

/*
*	Setup socket and establish virtual Connect on a SOCKET Level for DTLS
*/
static rsRetVal
dtls_send(wrkrInstanceData_t *pWrkrData, const actWrkrIParams_t *__restrict__ const pParam, const int iMsg)
{
	DEFiRet;
	int iErr, sslerr;
	instanceData *const pData = (instanceData *const) pWrkrData->pData;

	// Get Msg String
	const char* pszParamStr = (const char*)actParam(pParam, 1 /*pData->iNumTpls*/, iMsg, 0).param;
	size_t tzParamStrLen = actParam(pParam, 1 /*pData->iNumTpls*/, iMsg, 0).lenStr;
	DBGPRINTF("dtls_send[%p]: msg %d msg:'%.*s%s'\n",
		pWrkrData,
		iMsg,
		(int)(tzParamStrLen > 0 ? (tzParamStrLen > 64 ? 64 : tzParamStrLen-1) : 0),
		pszParamStr,
		(tzParamStrLen > 64 ? "..." : ""));

	iErr = SSL_write(pWrkrData->sslClient, pszParamStr, tzParamStrLen);
	if(iErr > 0) {
		DBGPRINTF("dtls_send[%p]: Successfully send message '%s' with %ld bytes to %s:%s\n",
			pWrkrData, pszParamStr, tzParamStrLen, pData->target, pData->port);

		// Increment Stats Counter
		STATSCOUNTER_INC(ctrDtlsSubmit, mutCtrDtlsSubmit);
		INST_STATSCOUNTER_INC(pData, pData->ctrDtlsSubmit, pData->mutCtrDtlsSubmit);
	} else {
		sslerr = SSL_get_error(pWrkrData->sslClient, iErr);
		if (sslerr == SSL_ERROR_SYSCALL) {
			dbgprintf("dtls_send[%p]: SSL_write failed with SSL_ERROR_SYSCALL(%s)"
				" - Aborting Connection.\n", pWrkrData, strerror(errno));
			net_ossl.osslLastOpenSSLErrorMsg(pData->target, iErr, pWrkrData->sslClient, LOG_WARNING,
				"omdtls", "SSL_write");
			ABORT_FINALIZE(RS_RET_ERR);
		} else {
			dbgprintf("dtls_send[%p]: SSL_write failed with ERROR [%d]: %s"
				" - Aborting Connection.\n", pWrkrData, sslerr, ERR_error_string(sslerr, NULL));
			net_ossl.osslLastOpenSSLErrorMsg(pData->target, iErr, pWrkrData->sslClient, LOG_WARNING,
				"omdtls", "SSL_write");
			ABORT_FINALIZE(RS_RET_ERR);
		}

		// Increment Stats Counter
		STATSCOUNTER_INC(ctrDtlsFail, mutCtrDtlsFail);
		INST_STATSCOUNTER_INC(pData, pData->ctrDtlsFail, pData->mutCtrDtlsFail);
	}
finalize_it:
	DBGPRINTF("dtls_send[%p]: Exit with %d for %s:%s\n", pWrkrData, iRet, pData->target, pData->port);
	if(iRet != RS_RET_OK) {
		dtls_close(pWrkrData);
	}
	RETiRet;
}

/*
*	Setup socket and establish virtual Connect on a SOCKET Level for DTLS
*/
static rsRetVal
dtls_connect(wrkrInstanceData_t *pWrkrData) {
	DEFiRet;
	int iErr;
	instanceData *const pData = (instanceData *const) pWrkrData->pData;
	BIO *bio_client;
	const SSL_CIPHER *sslCipher;

	DBGPRINTF("dtls_connect[%p]: setup DTLS for %s:%s\n", pWrkrData, pData->target, pData->port);
	pWrkrData->ConnectState = DTLS_CONNECTING;

	// Create SSL object
	pWrkrData->sslClient = SSL_new(pData->pNetOssl->ctx);
	if(!pWrkrData->sslClient) {
		dbgprintf("dtls_connect[%p]: SSL_new failed failed\n", pWrkrData);
		net_ossl.osslLastOpenSSLErrorMsg(pData->target, 0, pWrkrData->sslClient,
			LOG_WARNING, "omdtls", "SSL_new");
		ABORT_FINALIZE(RS_RET_ERR);
	}

	// Set certifcate check callback
	if (pData->pNetOssl->authMode != OSSL_AUTH_CERTANON) {
		dbgprintf("dtls_connect[%p]: enable certificate checking (Mode=%d, VerifyDepth=%d)\n",
			pWrkrData, pData->pNetOssl->authMode, pData->CertVerifyDepth);
		/* Enable certificate valid checking */
		net_ossl.osslSetSslVerifyCallback(pWrkrData->sslClient,
			SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT);
		if (pData->CertVerifyDepth != 0) {
			SSL_set_verify_depth(pWrkrData->sslClient, pData->CertVerifyDepth);
		}
	} else {
		dbgprintf("dtls_connect[%p]: disable certificate checking\n", pWrkrData);
		net_ossl.osslSetSslVerifyCallback(pWrkrData->sslClient, SSL_VERIFY_NONE);
	}

	/* Create BIO from socket array! */
	bio_client = BIO_new_dgram(pWrkrData->sockout, BIO_NOCLOSE);
	if (!bio_client) {
		net_ossl.osslLastOpenSSLErrorMsg(pData->target, 0, pWrkrData->sslClient, LOG_INFO,
			"dtls_connect", "BIO_new_dgram");
		ABORT_FINALIZE(RS_RET_ERR);
	}
	BIO_ctrl(bio_client, BIO_CTRL_DGRAM_SET_CONNECTED, 0, &pWrkrData->dtls_client_addr);
	SSL_set_bio(pWrkrData->sslClient, bio_client, bio_client);

	/* Set debug Callback for conn BIO as well! */
	net_ossl.osslSetBioCallback(bio_client);

	dbgprintf("dtls_connect[%p]: Starting DTLS session ...\n", pWrkrData);
	/* Perform handshake */
	iErr = SSL_connect(pWrkrData->sslClient);
	if (iErr <= 0) {
		net_ossl.osslLastOpenSSLErrorMsg(pData->target, iErr, pWrkrData->sslClient, LOG_INFO,
			"dtls_connect", "SSL_connect");
		ABORT_FINALIZE(RS_RET_ERR);
	}

	// Print Cipher info
	sslCipher = SSL_get_current_cipher(pWrkrData->sslClient);
	dbgprintf("dtls_connect[%p]: Cipher Version: %s Name: %s\n", pWrkrData,
		SSL_CIPHER_get_version(sslCipher), SSL_CIPHER_get_name(sslCipher));

	if(Debug) {
		// Print Peer Certificate info
		X509 *cert = SSL_get_peer_certificate(pWrkrData->sslClient);
		if (cert != NULL) {
			char *line;
			line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
			dbgprintf("dtls_connect[%p]: Subject: %s\n", pWrkrData, line);
			OPENSSL_free(line);

			line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
			dbgprintf("dtls_connect[%p]: Issuer: %s\n", pWrkrData, line);
			OPENSSL_free(line);
			X509_free(cert);
		} else {
			dbgprintf("dtls_connect[%p]: No certificates.\n", pWrkrData);
		}
	}

	/* Set reference in SSL obj */
	SSL_set_ex_data(pWrkrData->sslClient, 0, NULL);
	SSL_set_ex_data(pWrkrData->sslClient, 1, NULL);

	/* Set and activate timeouts */
	struct timeval timeout;
	timeout.tv_sec = 3;
	timeout.tv_usec = 0;
	BIO_ctrl(bio_client, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &timeout);

	pWrkrData->ConnectState = DTLS_CONNECTED;
finalize_it:
	DBGPRINTF("dtls_connect[%p]: Exit with %d for %s:%s\n", pWrkrData, iRet, pData->target, pData->port);
	if(iRet != RS_RET_OK) {
		pWrkrData->ConnectState = DTLS_DISCONNECTED;
	}
	RETiRet;
}

/*
*	Setup socket and establish virtual Connect on a SOCKET Level for DTLS
*/
static rsRetVal
dtls_init(wrkrInstanceData_t *pWrkrData)
{
	DEFiRet;
	int iErr;
	instanceData *const pData = (instanceData *const) pWrkrData->pData;
	struct addrinfo *addrResolved = NULL;
	struct addrinfo hints;
	DBGPRINTF("dtls_init[%p]: setup for %s:%s\n", pWrkrData, pData->target, pData->port);

	// Close previous ressources if any left open
	dtls_close(pWrkrData);

	// Setup receiving Socket for DTLS
	if((pWrkrData->sockin = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		return 1;
	memset(&pWrkrData->dtls_client_addr, 0, sizeof(pWrkrData->dtls_client_addr));
	pWrkrData->dtls_client_addr.sin_family = AF_INET;
	pWrkrData->dtls_client_addr.sin_port = htons(0);
	pWrkrData->dtls_client_addr.sin_addr.s_addr = INADDR_ANY;
	if (bind(pWrkrData->sockin,
		(struct sockaddr*)&pWrkrData->dtls_client_addr,
		sizeof(pWrkrData->dtls_client_addr)) < 0) {
		LogError(0, RS_RET_SUSPENDED, "omdtls[%p]: dtls_init Unable to bind DTLS CLient socket with errno %d",
			pWrkrData, errno);
		ABORT_FINALIZE(RS_RET_COULD_NOT_BIND);
	}

	// Resolve target address
	memset(&hints, 0, sizeof(hints));
	/* port must be numeric, because config file syntax requires this */
	hints.ai_family = glbl.GetDefPFFamily(runModConf->pConf);
	hints.ai_socktype = SOCK_DGRAM;
	if((iErr = (getaddrinfo((char*)pData->target, (char*)pData->port, &hints, &addrResolved))) != 0) {
		LogError(0, RS_RET_SUSPENDED,
			"omdtls[%p]: could not get addrinfo for hostname '%s':'%s': %s",
			pWrkrData, pData->target, pData->port, gai_strerror(iErr));
		ABORT_FINALIZE(RS_RET_ADDRESS_UNKNOWN);
	}
	pWrkrData->dtls_rcvr_addrinfo = addrResolved;
	addrResolved = NULL;

	// Init Socket Connection (Which technically does not connect but prepares socket for DTLS)
	DBGPRINTF("dtls_init[%p]: Init Session to %s:%s\n", pWrkrData, pData->target, pData->port);

	// Connect the UDP socket to the receiver's address
	pWrkrData->sockout = socket(AF_INET, SOCK_DGRAM, 0);
	if (connect(pWrkrData->sockout,
		pWrkrData->dtls_rcvr_addrinfo->ai_addr,
		pWrkrData->dtls_rcvr_addrinfo->ai_addrlen) < 0) {
		LogError(0, RS_RET_SUSPENDED,
			"dtls_init[%p]: Failed to connect to hostname '%s':'%s': %s",
			pWrkrData, pData->target, pData->port, gai_strerror(iErr));
		ABORT_FINALIZE(RS_RET_ERR);
	}

	// Socket Connect successfull, no continue with TLS
	CHKiRet(dtls_connect(pWrkrData));

	// We reached this position means we are ready to send!
	pWrkrData->ConnectState = DTLS_CONNECTED;
finalize_it:
	DBGPRINTF("dtls_init[%p]: doTryResume %s iRet %d\n", pWrkrData, pData->target, iRet);
	if(addrResolved != NULL) {
		freeaddrinfo(addrResolved);
	}
	if(iRet != RS_RET_OK) {
		if(pWrkrData->dtls_rcvr_addrinfo != NULL) {
			freeaddrinfo(pWrkrData->dtls_rcvr_addrinfo);
			pWrkrData->dtls_rcvr_addrinfo = NULL;
		}
		iRet = RS_RET_SUSPENDED;

		// Force DTLS Disconnect State
		dtls_close(pWrkrData);
		// pWrkrData->ConnectState = DTLS_DISCONNECTED;
	}
	RETiRet;
}

static rsRetVal
dtls_close(wrkrInstanceData_t *pWrkrData)
{
	DEFiRet;
	int iErr;
	instanceData *const pData = (instanceData *const) pWrkrData->pData;
	if (pWrkrData->ConnectState == DTLS_CONNECTED) {
		DBGPRINTF("dtls_close[%p]: close session for %s:%s\n", pWrkrData, pData->target, pData->port);
		if (	pWrkrData->sslClient != NULL &&
			pWrkrData->ConnectState == DTLS_CONNECTED) {
			iErr = SSL_shutdown(pWrkrData->sslClient);
			if (iErr <= 0){
				/* Shutdown not finished, call SSL_read to do a bidirectional shutdown, see doc:
				*	https://www.openssl.org/docs/man1.1.1/man3/SSL_shutdown.html
				*	We do a single SSL_read which should be fine for protocol handling.
				*	No data is to be expected from the receiver side.
				*/
				char rcvBuf[DTLS_MAX_RCVBUF];
				SSL_read(pWrkrData->sslClient, rcvBuf, DTLS_MAX_RCVBUF);
			}
			SSL_free(pWrkrData->sslClient);
			pWrkrData->sslClient = NULL;
		}
	}

	// Close Sockets
	if (	pWrkrData->sockout != 0) {
		close(pWrkrData->sockout);
		pWrkrData->sockout = 0;
	}
	if (	pWrkrData->sockin != 0) {
		close(pWrkrData->sockin);
		pWrkrData->sockin = 0;
	}

	// Free Memory
	if(pWrkrData->dtls_rcvr_addrinfo != NULL) {
		freeaddrinfo(pWrkrData->dtls_rcvr_addrinfo);
		pWrkrData->dtls_rcvr_addrinfo = NULL;
	}

	// Reset Helpers
	pWrkrData->ConnectState = DTLS_DISCONNECTED;
FINALIZE;
finalize_it:
	RETiRet;
}
