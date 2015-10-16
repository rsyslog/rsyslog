/* omczmq.c
 * Copyright (C) 2014 Brian Knox
 * Copyright (C) 2014 Rainer Gerhards
 *
 * Author: Brian Knox <bknox@digitalocean.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
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
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include "cfsysline.h"
#include <czmq.h>

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("omczmq")

DEF_OMOD_STATIC_DATA
DEFobjCurrIf(errmsg)

static pthread_mutex_t mutDoAct = PTHREAD_MUTEX_INITIALIZER;

typedef struct _instanceData {
	zactor_t *authActor;
	zactor_t *beaconActor;
	zsock_t *sock;
	zcert_t *clientCert;
	zcert_t *serverCert;
	char *sockEndpoints;
	int sockType;
	char *authType;
	char *clientCertPath;
	char *serverCertPath;
	uchar *tplName;
	char *beacon;
	int beaconport;
	char *topicList;
} instanceData;

typedef struct wrkrInstanceData {
	instanceData *pData;
} wrkrInstanceData_t;

static struct cnfparamdescr actpdescr[] = {
	{ "endpoints", eCmdHdlrGetWord, 1 },
	{ "socktype", eCmdHdlrGetWord, 1 },
	{ "beacon", eCmdHdlrGetWord, 0 },
	{ "beaconport", eCmdHdlrGetWord, 0 },
	{ "authtype", eCmdHdlrGetWord, 0 },
	{ "clientcertpath", eCmdHdlrGetWord, 0 },
	{ "servercertpath", eCmdHdlrGetWord, 0 },
	{ "template", eCmdHdlrGetWord, 0 },
	{ "topics", eCmdHdlrGetWord, 0 }
};

static struct cnfparamblk actpblk = {
	CNFPARAMBLK_VERSION,
	sizeof(actpdescr) / sizeof(struct cnfparamdescr),
	actpdescr
};

static rsRetVal initCZMQ(instanceData* pData) {
	DEFiRet;

	/* tell czmq to not use it's own signal handler */
	putenv ("ZSYS_SIGHANDLER=false");

	/* create new auth actor */
	DBGPRINTF ("omczmq: starting auth actor...\n");
	pData->authActor = zactor_new(zauth, NULL);
	if (!pData->authActor) {
		errmsg.LogError(0, RS_RET_NO_ERRCODE,
				"omczmq: could not create auth service");
		ABORT_FINALIZE (RS_RET_NO_ERRCODE);
	}

	/* create our zeromq socket */
	DBGPRINTF ("omczmq: creating zeromq socket...\n");
	pData->sock = zsock_new(pData->sockType);
	if (!pData->sock) {
		errmsg.LogError(0, RS_RET_NO_ERRCODE,
				"omczmq: new socket failed for endpoints: %s",
				pData->sockEndpoints);
		ABORT_FINALIZE(RS_RET_NO_ERRCODE);
	}

	bool is_server = false;

	/* if a beacon is set start it */
	if((pData->beacon != NULL) && (pData->beaconport > 0)) {
		DBGPRINTF ("omczmq: starting beacon actor...\n");

		/* create the beacon actor, if it fails abort */	
		pData->beaconActor = zactor_new(zbeacon, NULL);
		if (!pData->beaconActor) {
			errmsg.LogError(0, RS_RET_NO_ERRCODE,
					"omczmq: could not create beacon service");
			ABORT_FINALIZE (RS_RET_NO_ERRCODE);
		}

		/* configure the beacon and abort if UDP broadcasting ont available */
		zsock_send(pData->beaconActor, "si", "CONFIGURE", pData->beaconport);
		char *hostname = zstr_recv(pData->beaconActor);
		if (!*hostname) {
			errmsg.LogError(0, RS_RET_NO_ERRCODE,
					"omczmq: no UDP broadcasting available");
			ABORT_FINALIZE (RS_RET_NO_ERRCODE);
		}

		/* start publishing the beacon */
		zsock_send(pData->beaconActor, "sbi", "PUBLISH", pData->beacon, strlen(pData->beacon));
	}

	/* if we are a CURVE server */
	if ((pData->authType != NULL) && (!strcmp(pData->authType, "CURVESERVER"))) {
		DBGPRINTF("omczmq: we are a curve server...\n");
		
		is_server = true;

		/* set global auth domain */
		zsock_set_zap_domain(pData->sock, "global");

		/* set that we are a curve server */
		zsock_set_curve_server(pData->sock, 1);

		/* get and set our server cert */
		DBGPRINTF("omczmq: server cert is %s...\n", pData->serverCertPath);
		pData->serverCert = zcert_load(pData->serverCertPath);
		if (!pData->serverCert) {
			errmsg.LogError(0, NO_ERRCODE, "could not load server cert");
			ABORT_FINALIZE(RS_RET_ERR);
		}

		zcert_apply(pData->serverCert, pData->sock);

		/* set allowed clients */
		DBGPRINTF("omczmq: allowed clients are %s...\n", pData->clientCertPath);
		zstr_sendx(pData->authActor, "CURVE", pData->clientCertPath, NULL);
		zsock_wait(pData->authActor);
	}

	/* if we are a CURVE client */
	if ((pData->authType != NULL) && (!strcmp(pData->authType, "CURVECLIENT"))) {
		DBGPRINTF("omczmq: we are a curve client...\n");

		is_server = false;

		/* get our client cert */
		pData->clientCert = zcert_load(pData->clientCertPath);
		if (!pData->clientCert) {
			errmsg.LogError(0, NO_ERRCODE, "could not load client cert");
			ABORT_FINALIZE(RS_RET_ERR);
		}

		/* apply the client cert to the socket */
		zcert_apply(pData->clientCert, pData->sock);

		/* get the server cert */
		DBGPRINTF("omczmq: server cert is %s...\n", pData->serverCertPath);
		pData->serverCert = zcert_load(pData->serverCertPath);
		if (!pData->serverCert) {
			errmsg.LogError(0, NO_ERRCODE, "could not load server cert");
			ABORT_FINALIZE(RS_RET_ERR);
		}

		/* get the server public key and set it for the socket */
		char *server_key = zcert_public_txt(pData->serverCert);
		DBGPRINTF("omczmq: server public key is %s...\n", server_key);
		zsock_set_curve_serverkey (pData->sock, server_key);
	}

	/* if we are a PUB server */
	if (pData->sockType == ZMQ_PUB) {
		DBGPRINTF("omczmq: we are a pub server...\n");
		is_server = true;
	}

	/* we default to CONNECT unless told otherwise */
	int rc = zsock_attach(pData->sock, (const char*)pData->sockEndpoints, is_server);
	if (rc == -1) {
		errmsg.LogError(0, NO_ERRCODE, "zsock_attach to %s failed",
				pData->sockEndpoints);
		ABORT_FINALIZE(RS_RET_ERR);
	}

finalize_it:
	RETiRet;
}

rsRetVal outputCZMQ(uchar* msg, instanceData* pData) {
	DEFiRet;

	/* if auth or socket is missing, initialize */
	if((NULL == pData->sock) || (NULL == pData->authActor)) {
		CHKiRet(initCZMQ(pData));
	}

	/* if we have a ZMQ_PUB sock and topics, send with topics */
	if (pData->sockType == ZMQ_PUB && pData->topicList) {
		char topic[256], *list = pData->topicList;
		int rc;

		while (list) {
			char *delimiter = strchr(list, ',');
			if (!delimiter) {
				delimiter = list + strlen (list);
			}

			if (delimiter - list > 255) {
				errmsg.LogError(0, NO_ERRCODE,
						"pData->topicList must be under 256 characters");
				ABORT_FINALIZE(RS_RET_ERR);
			}

			memcpy(topic, list, delimiter - list);
			topic[delimiter - list] = 0;

			/* send the topic */
			rc = zstr_sendm(pData->sock, topic);

			/* something is very wrong */
			if (rc == -1) {
				errmsg.LogError(0, NO_ERRCODE, "omczmq: send of topic %s failed"
						": %s", topic, zmq_strerror(errno));
				ABORT_FINALIZE(RS_RET_ERR);
			}

			/* send the log line */
			rc = zstr_send(pData->sock, (char*)msg);

			/* something is very wrong */
			if (rc == -1) {
				errmsg.LogError(0, NO_ERRCODE, "omczmq: send of %s failed: %s",
						msg, zmq_strerror(errno));
				ABORT_FINALIZE(RS_RET_ERR);
			}

			if (*delimiter == 0) {
				break;
			}

			list = delimiter + 1;
		}
	} else {
		/* send the log line */
		int rc = zstr_send(pData->sock, (char*)msg);

		/* something is very wrong */
		if (rc == -1) {
			errmsg.LogError(0, NO_ERRCODE, "omczmq: send of %s failed: %s",
					msg, zmq_strerror(errno));
			ABORT_FINALIZE(RS_RET_ERR);
		}
	}
finalize_it:
	RETiRet;
}

static inline void
setInstParamDefaults(instanceData* pData) {
	pData->sockEndpoints = NULL;
	pData->sock = NULL;
	pData->clientCert = NULL;
	pData->serverCert = NULL;
	pData->tplName = NULL;
	pData->sockType = -1;
	pData->authActor = NULL;
	pData->beaconActor = NULL;
	pData->authType = NULL;
	pData->clientCertPath = NULL;
	pData->serverCertPath = NULL;
	pData->topicList = NULL;
}


BEGINcreateInstance
CODESTARTcreateInstance
ENDcreateInstance


BEGINcreateWrkrInstance
CODESTARTcreateWrkrInstance
ENDcreateWrkrInstance


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURERepeatedMsgReduction) {
		iRet = RS_RET_OK;
	}
ENDisCompatibleWithFeature


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
ENDdbgPrintInstInfo


BEGINfreeInstance
CODESTARTfreeInstance
	zsock_destroy(&pData->sock);
	zactor_destroy(&pData->authActor);
	zactor_destroy(&pData->beaconActor);
	zcert_destroy(&pData->serverCert);
	zcert_destroy(&pData->clientCert);

	free(pData->sockEndpoints);
	free(pData->authType);
	free(pData->clientCertPath);
	free(pData->serverCertPath);
	free(pData->tplName);
	free(pData->topicList);
ENDfreeInstance


BEGINfreeWrkrInstance
CODESTARTfreeWrkrInstance
ENDfreeWrkrInstance


BEGINtryResume
CODESTARTtryResume
	pthread_mutex_lock(&mutDoAct);
	if(NULL == pWrkrData->pData->sock) {
		iRet = initCZMQ(pWrkrData->pData);
	}
	pthread_mutex_unlock(&mutDoAct);
ENDtryResume


BEGINdoAction
	instanceData *pData = pWrkrData->pData;
CODESTARTdoAction
	pthread_mutex_lock(&mutDoAct);
	iRet = outputCZMQ(ppString[0], pData);
	pthread_mutex_unlock(&mutDoAct);
ENDdoAction


BEGINnewActInst
	struct cnfparamvals *pvals;
	int i;
CODESTARTnewActInst
	if ((pvals = nvlstGetParams(lst, &actpblk, NULL)) == NULL) {
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	CHKiRet(createInstance(&pData));
	setInstParamDefaults(pData);

	CODE_STD_STRING_REQUESTnewActInst(1)
	for (i = 0; i < actpblk.nParams; ++i) {
		if (!pvals[i].bUsed) {
			continue;
		}

		/* get the socket endpoints to use */
		if (!strcmp(actpblk.descr[i].name, "endpoints")) {
			pData->sockEndpoints = es_str2cstr(pvals[i].val.d.estr, NULL);
		} 

		/* get the output template to use */
		else if (!strcmp(actpblk.descr[i].name, "template")) {
			pData->tplName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} 

		/* get the socket type */
		else if (!strcmp(actpblk.descr[i].name, "socktype")){
			char *stringType = es_str2cstr(pvals[i].val.d.estr, NULL);

			if (!strcmp("PUB", stringType)) {
				pData->sockType = ZMQ_PUB;
			}

			else if (!strcmp("PUSH", stringType)) {
				pData->sockType = ZMQ_PUSH;
			}

			else {
				errmsg.LogError(0, RS_RET_CONFIG_ERROR,
						"omczmq: invalid socktype");
				ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
			}
		} 

		/* get the authentication type to use */
		else if (!strcmp(actpblk.descr[i].name, "authtype")) {
			pData->authType = es_str2cstr(pvals[i].val.d.estr, NULL);

			/* make sure defined type is supported */
			if ((pData->authType != NULL) && 
					strcmp("CURVESERVER", pData->authType) &&
					strcmp("CURVECLIENT", pData->authType))
			{

				errmsg.LogError(0, RS_RET_CONFIG_ERROR,
						"omczmq: %s is not a valid authType",
						pData->authType);
				ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
			}
		} 

		/* get client cert argument */
		else if (!strcmp(actpblk.descr[i].name, "clientcertpath")) {
			pData->clientCertPath = es_str2cstr(pvals[i].val.d.estr, NULL);
		}

		/* get server cert argument */
		else if (!strcmp(actpblk.descr[i].name, "servercertpath")) {
			pData->serverCertPath = es_str2cstr(pvals[i].val.d.estr, NULL);
		}

		/* get beacon argument */
		else if (!strcmp(actpblk.descr[i].name, "beacon")) {
			pData->beacon = es_str2cstr(pvals[i].val.d.estr, NULL);
		}
		
		/* get beacon port */
		else if (!strcmp(actpblk.descr[i].name, "beaconport")) {
			pData->beaconport = atoi(es_str2cstr(pvals[i].val.d.estr, NULL));
		}

		/* get the subscription topics */
		else if(!strcmp(actpblk.descr[i].name, "topics")) {
			if (pData->sockType != ZMQ_PUB) {
				errmsg.LogError(0, RS_RET_CONFIG_ERROR,
						"topics is invalid unless socktype is PUB");
				ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
			}

			pData->topicList = es_str2cstr(pvals[i].val.d.estr, NULL);
		}

		/* the config has a bad option */
		else {
			errmsg.LogError(0, NO_ERRCODE, "omczmq: %s is not a valid option",
					actpblk.descr[i].name);
			ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
		}
	}

	/* if we don't have a template name, 
	 * use the default RSYSLOG_ForwardFormat template */
	if (pData->tplName == NULL) {
		CHKiRet(OMSRsetEntry(*ppOMSR, 0, (uchar*)strdup("RSYSLOG_ForwardFormat"),
					OMSR_NO_RQD_TPL_OPTS));
	} 

	/* use the requested template */
	else {
		CHKiRet(OMSRsetEntry(*ppOMSR, 0, (uchar*)pData->tplName, OMSR_NO_RQD_TPL_OPTS));
	}

	/* error if no socket endpoints are defined */
	if (NULL == pData->sockEndpoints) {
		errmsg.LogError(0, RS_RET_CONFIG_ERROR, "omczmq: sockEndpoint required");
		ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
	}

	/* error if no socket type is defined */
	if (pData->sockType == -1) {
		errmsg.LogError(0, RS_RET_CONFIG_ERROR, "omczmq: socktype required");
		ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
	}

	CODE_STD_FINALIZERnewActInst
	cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst

BEGINparseSelectorAct
CODESTARTparseSelectorAct
	/* tell the engine we only want one template string */
	CODE_STD_STRING_REQUESTparseSelectorAct(1)

	if (!strncmp((char*) p, ":omczmq:", sizeof(":omczmq:") - 1)) { 
		errmsg.LogError(0, RS_RET_LEGA_ACT_NOT_SUPPORTED,
			"omczmq supports only v6 config format, use: "
			"action(type=\"omczmq\" serverport=...)");
	}

	ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
	CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct

BEGINinitConfVars
CODESTARTinitConfVars
ENDinitConfVars


BEGINmodExit
CODESTARTmodExit
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
	CODEqueryEtryPt_STD_OMOD_QUERIES
	CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES
	CODEqueryEtryPt_STD_OMOD8_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION;
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	INITChkCoreFeature(bCoreSupportsBatching, CORE_FEATURE_BATCHING);
	DBGPRINTF("omczmq: module compiled with rsyslog version %s.\n", VERSION);

	INITLegCnfVars
ENDmodInit
