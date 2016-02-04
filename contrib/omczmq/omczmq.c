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
	zactor_t *authActor;	/* reference to a CZMQ auth actor */
	zactor_t *beaconActor;	/* reference to a CZMQ beacon actor */
	zsock_t *sock;			/* zeromq socket */
	bool serverish;			/* whether socket will be a server (bind) or client */
	int sendTimeout;		/* send timeout for socket (defaults to -1) */  
	zcert_t *clientCert;	/* client curve certificate */
	zcert_t *serverCert;	/* server curve certificate */
	zlist_t *topics;		/* publish topics if set */
	bool sendError;			/* set to true of a socket send timed out */
	char *sockEndpoints;	/* (required) comma delimited list of endpoints */
	int sockType;			/* (required) the type of our zeromq socket */
	char *authType;			/* (optional) type of authentication */
	char *clientCertPath;	/* (optional) path to curve client cert(s) */
	char *serverCertPath;	/* (optional) path to curve server cert */
	uchar *tplName;			/* (optional) name of template to use */
	char *beacon;			/* (optional) content of beacon if beaconing */
	int beaconport;			/* (optional) port of beacon if beaconing */
	char *topicList;		/* (optional) topic list */
	bool topicFrame;		/* (optional) whether to set topic as separate frame */
} instanceData;

typedef struct wrkrInstanceData {
	instanceData *pData;
} wrkrInstanceData_t;

/* -------------------------------------------------------------
 * Explanation of Options
 * ---------------------- 
 * endpoints
 *		A comma delimited list of endpoints to connect to. Whether the
 *		endpoint connects or binds is determined by defaults for 
 *		each socket type.
 * socktype
 *		The type of zeromq socket to use; supports PUB, PUSH and DEALER.
 * sndTimeout
 *		Send timeout. Defaults to -1, which is "block forever" and the
 *		standard ZeroMQ default. 0 = immediate, otherwise is value
 *		in milliseconds.
 * beacon
 *		The beacon is lit! If beacon is set to a value, that value
 *		will be used to advertise the existence of this endpoint
 *		over udp multicast beacons.
 * beaconport
 *		The port to use for the beacon
 * authtype
 *		The authentication type to use - currently only supports
 *		CURVESERVER and CURVECLIENT.
 * clientcertpath
 *		The path to client certificate(s). Must be set if authtype is
 *		set to CURVE.
 * servercertpath
 *		The path to server certificate. Must be set if authtype is
 *		set to CURVE
 * template
 *		The template to use to format outgoing logs. If not set,
 *		defaults to RSYSLOG_ForwardFormat.
 * topics
 *		A comma delimited list of topics to publish messages on.
 *		If this is set, each message will be sent once per topic
 *		in the list. Note that ZeroMQ "topics" are simply matches
 *		against the first bytes in a message, so dynamic topics
 *		per message can be constructed using templates as well.
 * ------------------------------------------------------------- */
static struct cnfparamdescr actpdescr[] = {
	{ "endpoints", eCmdHdlrGetWord, 1 },
	{ "socktype", eCmdHdlrGetWord, 1 },
	{ "sendtimeout", eCmdHdlrGetWord, 0 },
	{ "beacon", eCmdHdlrGetWord, 0 },
	{ "beaconport", eCmdHdlrGetWord, 0 },
	{ "authtype", eCmdHdlrGetWord, 0 },
	{ "clientcertpath", eCmdHdlrGetWord, 0 },
	{ "servercertpath", eCmdHdlrGetWord, 0 },
	{ "template", eCmdHdlrGetWord, 0 },
	{ "topics", eCmdHdlrGetWord, 0 },
	{ "topicframe", eCmdHdlrGetWord, 0}
};

static struct cnfparamblk actpblk = {
	CNFPARAMBLK_VERSION,
	sizeof(actpdescr) / sizeof(struct cnfparamdescr),
	actpdescr
};

static rsRetVal initCZMQ(instanceData* pData) {
	DEFiRet;

	/* Turn off CZMQ signal handling */
	/* ----------------------------- */	

    putenv ("ZSYS_SIGHANDLER=false");

	/* Create the authentication actor */
	/* ------------------------------ */

	pData->authActor = zactor_new(zauth, NULL);

	if (!pData->authActor) {
		errmsg.LogError(0, RS_RET_NO_ERRCODE,
				"omczmq: could not create auth actor");
		ABORT_FINALIZE (RS_RET_SUSPENDED);
	}

	DBGPRINTF ("omczmq: auth actor started\n");

	/* Create the zeromq socket */
	/* ------------------------ */

	pData->sock = zsock_new(pData->sockType);

	if (!pData->sock) {
		errmsg.LogError(0, RS_RET_NO_ERRCODE,
				"omczmq: new socket failed for endpoints: %s",
				pData->sockEndpoints);
		ABORT_FINALIZE(RS_RET_SUSPENDED);
	}

	zsock_set_sndtimeo (pData->sock, pData->sendTimeout);
	DBGPRINTF ("omczmq: created socket with timeout %d...\n", pData->sendTimeout);

	/* Create the beacon actor if configured */
	/* ------------------------------------- */

	if((pData->beacon != NULL) && (pData->beaconport > 0)) {

		pData->beaconActor = zactor_new(zbeacon, NULL);

		if (!pData->beaconActor) {
			errmsg.LogError(0, RS_RET_NO_ERRCODE,
					"omczmq: could not create beacon service");
			ABORT_FINALIZE (RS_RET_SUSPENDED);
		}

		zsock_send(pData->beaconActor, "si", "CONFIGURE", pData->beaconport);
		char *hostname = zstr_recv(pData->beaconActor);
		
		if (!*hostname) {
			errmsg.LogError(0, RS_RET_NO_ERRCODE,
					"omczmq: no UDP broadcasting available");
			ABORT_FINALIZE (RS_RET_SUSPENDED);
		}

		zsock_send(pData->beaconActor, "sbi", "PUBLISH", pData->beacon, strlen(pData->beacon));

		DBGPRINTF ("omczmq: beacon is lit: hostname: '%s', port: '%d'...\n", 
				hostname, pData->beaconport);

		zstr_free (&hostname);
	}

	/* Load certs for auth if auth is used */
	/* ----------------------------------- */

	if (pData->authType != NULL) {

		/* CURVESERVER */
		/* ---------- */

		if (!strcmp(pData->authType, "CURVESERVER")) {
		
			zsock_set_zap_domain(pData->sock, "global");
			zsock_set_curve_server(pData->sock, 1);

			pData->serverCert = zcert_load(pData->serverCertPath);

			if (!pData->serverCert) {
				errmsg.LogError(0, NO_ERRCODE, "could not load server cert");
				ABORT_FINALIZE(RS_RET_ERR);
			}

			zcert_apply(pData->serverCert, pData->sock);

			zstr_sendx(pData->authActor, "CURVE", pData->clientCertPath, NULL);
			zsock_wait(pData->authActor);

			DBGPRINTF("omczmq: CURVESERVER: serverCertPath: '%s'\n", pData->serverCertPath);
			DBGPRINTF("omczmq: CURVESERVER: clientCertPath: '%s'\n", pData->clientCertPath);
		}

		/* CURVECLIENT */
		/* ----------- */

		else if (!strcmp(pData->authType, "CURVECLIENT")) {
			if (!strcmp(pData->clientCertPath, "*")) {
				pData->clientCert = zcert_new();
			}
			else {
				pData->clientCert = zcert_load(pData->clientCertPath);
			}
		
			if (!pData->clientCert) {
				errmsg.LogError(0, NO_ERRCODE, "could not load client cert");
				ABORT_FINALIZE(RS_RET_ERR);
			}

			zcert_apply(pData->clientCert, pData->sock);

			pData->serverCert = zcert_load(pData->serverCertPath);

			if (!pData->serverCert) {
				errmsg.LogError(0, NO_ERRCODE, "could not load server cert");
				ABORT_FINALIZE(RS_RET_ERR);
			}

			char *server_key = zcert_public_txt(pData->serverCert);
			zsock_set_curve_serverkey (pData->sock, server_key);

			DBGPRINTF("omczmq: CURVECLIENT: serverCertPath: '%s'\n", pData->serverCertPath);
			DBGPRINTF("omczmq: CURVECLIENT: clientCertPath: '%s'\n", pData->clientCertPath);
			DBGPRINTF("omczmq: CURVECLIENT: server_key: '%s'\n", server_key);
		}
		else {
			errmsg.LogError(0, NO_ERRCODE, "unrecognized auth type: '%s'", pData->authType);
				ABORT_FINALIZE(RS_RET_ERR);
		}
	}
	
	switch (pData->sockType) {
		case ZMQ_PUB:
			pData->serverish = true;
			break;
		case ZMQ_PUSH:
		case ZMQ_DEALER:
			pData->serverish = false;
			break;
	}

	int rc = zsock_attach(pData->sock, (const char*)pData->sockEndpoints, pData->serverish);
	if (rc == -1) {
		errmsg.LogError(0, NO_ERRCODE, "zsock_attach to %s failed",
				pData->sockEndpoints);
		ABORT_FINALIZE(RS_RET_SUSPENDED);
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

	/* if we have a ZMQ_PUB sock and topics, send once per topic */
	if (pData->sockType == ZMQ_PUB && pData->topics) {
		char *topic = zlist_first(pData->topics);

		while (topic) {
			if (pData->topicFrame) {
				int rc = zstr_sendx(pData->sock, topic, (char*)msg, NULL);
				if (rc != 0) {
					pData->sendError = true;
					ABORT_FINALIZE(RS_RET_SUSPENDED);
				}
			} 
			else {
				int rc = zstr_sendf(pData->sock, "%s%s", topic, (char*)msg);
				if (rc != 0) {
					pData->sendError = true;
					ABORT_FINALIZE(RS_RET_SUSPENDED);
				}

			}
			topic = zlist_next(pData->topics);
		}
	} 
	/* otherwise do a normal send */
	else {
		int rc = zstr_send(pData->sock, (char*)msg);
		if (rc != 0) {
			pData->sendError = true;
			ABORT_FINALIZE(RS_RET_SUSPENDED);
		}
	}
finalize_it:
	RETiRet;
}

static inline void
setInstParamDefaults(instanceData* pData) {
	pData->sockEndpoints = NULL;
	pData->sock = NULL;
	pData->sendError = false;
	pData->serverish = false;
	pData->clientCert = NULL;
	pData->serverCert = NULL;
	pData->tplName = NULL;
	pData->sockType = -1;
	pData->sendTimeout = -1;
	pData->topics = NULL;
	pData->authActor = NULL;
	pData->beaconActor = NULL;
	pData->beaconport = 0;
	pData->authType = NULL;
	pData->clientCertPath = NULL;
	pData->serverCertPath = NULL;
	pData->topicList = NULL;
	pData->topicFrame = true;
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
	if (pData->topics != NULL)
		zlist_destroy(&pData->topics);
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
	instanceData *pData;
CODESTARTtryResume
	pthread_mutex_lock(&mutDoAct);
	pData = pWrkrData->pData;
	DBGPRINTF("omczmq: trying to resume...\n");
	zsock_destroy(&pData->sock);
	zactor_destroy(&pData->beaconActor);
	zactor_destroy(&pData->authActor);
	iRet = initCZMQ(pData);
	pthread_mutex_unlock(&mutDoAct);
ENDtryResume


BEGINdoAction
	instanceData *pData;
CODESTARTdoAction
	pthread_mutex_lock(&mutDoAct);
	pData = pWrkrData->pData;
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

	/* handle our configuration arguments
	* - endpoints - required
	* - socktype - required
	* - beacon - optional
	* - beaconport - required if beacon is set
	* - authtype - optional and NULL == no auth
	* - clientcertpath - required if authtype is CURVESERVER or CURVECLIENT
	* - servercertpath - required if authtype is CURVESERVER or CURVECLIENT
	* - template - optional defaults to RSYSLOG_ForwardFormat
	* - topics - optional
	*/

	/* Handle options from the configuration */
	/* ------------------------------------- */

	for (i = 0; i < actpblk.nParams; ++i) {
		if (!pvals[i].bUsed) {
			continue;
		}

		if (!strcmp(actpblk.descr[i].name, "endpoints")) {
			pData->sockEndpoints = es_str2cstr(pvals[i].val.d.estr, NULL);
		} 
		else if (!strcmp(actpblk.descr[i].name, "template")) {
			pData->tplName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		}
		else if (!strcmp(actpblk.descr[i].name, "sendtimeout")) {
			pData->sendTimeout = atoi(es_str2cstr(pvals[i].val.d.estr, NULL));
		}
		else if (!strcmp(actpblk.descr[i].name, "socktype")){
			char *stringType = es_str2cstr(pvals[i].val.d.estr, NULL);
			if(stringType != NULL){
				if (!strcmp("PUB", stringType)) {
					pData->sockType = ZMQ_PUB;
				}
				else if (!strcmp("PUSH", stringType)) {
					pData->sockType = ZMQ_PUSH;
				}
				else if (!strcmp("DEALER", stringType)) {
					pData->sockType = ZMQ_DEALER;
				}
				else {
					free(stringType);
					errmsg.LogError(0, RS_RET_CONFIG_ERROR,
							"omczmq: invalid socktype");
					ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
				}
				free(stringType);
			}
			else{
				errmsg.LogError(0, RS_RET_OUT_OF_MEMORY,
						"omczmq: out of memory");
				ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
			}
		} 
		else if (!strcmp(actpblk.descr[i].name, "authtype")) {
			pData->authType = es_str2cstr(pvals[i].val.d.estr, NULL);

			/* auth must be NULL, CURVESERVER or CURVECLIENT */
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
		else if (!strcmp(actpblk.descr[i].name, "clientcertpath")) {
			pData->clientCertPath = es_str2cstr(pvals[i].val.d.estr, NULL);
		}
		else if (!strcmp(actpblk.descr[i].name, "servercertpath")) {
			pData->serverCertPath = es_str2cstr(pvals[i].val.d.estr, NULL);
		}
		else if (!strcmp(actpblk.descr[i].name, "beacon")) {
			pData->beacon = es_str2cstr(pvals[i].val.d.estr, NULL);
		}
		else if (!strcmp(actpblk.descr[i].name, "beaconport")) {
			pData->beaconport = atoi(es_str2cstr(pvals[i].val.d.estr, NULL));
		}
		else if (!strcmp(actpblk.descr[i].name, "topicframe")) {
			int tframe  = atoi(es_str2cstr(pvals[i].val.d.estr, NULL));
			if (tframe == 1) {
				pData->topicFrame = true;
			}
			else {
				pData->topicFrame = false;
			}
		}
		else if(!strcmp(actpblk.descr[i].name, "topics")) {
			if (pData->sockType != ZMQ_PUB) {
				errmsg.LogError(0, RS_RET_CONFIG_ERROR,
						"topics is invalid unless socktype is PUB");
				ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
			}

			// create a list of topics
			pData->topics = zlist_new();
			char *topics = es_str2cstr(pvals[i].val.d.estr, NULL);
			char *topics_org = topics;
			char topic[256];
			if(topics == NULL){
				errmsg.LogError(0, RS_RET_OUT_OF_MEMORY,
					"out of memory");
				ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
			}
			
			while (*topics) {
				char *delimiter = strchr(topics, ',');
				if (!delimiter) {
					delimiter = topics + strlen(topics);
				}
				if (delimiter - topics > 255) {
					errmsg.LogError(0, RS_RET_CONFIG_ERROR,
						"topics must be under 256 characters");
					free(topics_org);
					ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
				}
				memcpy (topic, topics, delimiter - topics);
				topic[delimiter-topics] = 0;
				char *current_topic = strdup(topic);
				zlist_append (pData->topics, current_topic);
				if (*delimiter == 0) {
					break;
				}
				topics = delimiter + 1;
			}
			free(topics_org);

		}
		else {
			errmsg.LogError(0, NO_ERRCODE,
					"omczmq: config error - '%s' is not a valid option",
					actpblk.descr[i].name);
			ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
		}
	}

	/* Defaults and sanity checking  */
	/* ----------------------------- */

	if (pData->tplName == NULL) {
		CHKiRet(OMSRsetEntry(*ppOMSR, 0, (uchar*)strdup("RSYSLOG_ForwardFormat"),
					OMSR_NO_RQD_TPL_OPTS));
	} 
	else {
		CHKiRet(OMSRsetEntry(*ppOMSR, 0, (uchar*)pData->tplName, OMSR_NO_RQD_TPL_OPTS));
	}

	if ((pData->beacon != NULL) && (pData->beaconport == 0)) {
		errmsg.LogError(0, NO_ERRCODE,
				"omczmq: config error - beaconport is required if beacon is set");
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
