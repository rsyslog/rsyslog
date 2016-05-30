/* omczmq.c
 * Copyright (C) 2016 Brian Knox
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

static struct cnfparamdescr modpdescr[] = {
	{ "authenticator", eCmdHdlrBinary, 0 },
	{ "authtype", eCmdHdlrGetWord, 0 },
	{ "clientcertpath", eCmdHdlrGetWord, 0 },
	{ "servercertpath", eCmdHdlrGetWord, 0 }
};

static struct cnfparamblk modpblk = {
	CNFPARAMBLK_VERSION,
	sizeof(modpdescr)/sizeof(struct cnfparamdescr),
	modpdescr
};

struct modConfData_s {
	rsconf_t *pConf;
	uchar *tplName;
	int authenticator;
	char *authType;
	char *serverCertPath;
	char *clientCertPath;
};

static modConfData_t *runModConf = NULL;
static zactor_t *authActor;

typedef struct _instanceData {
	zsock_t *sock;
	bool serverish;
	int sendTimeout;
	zlist_t *topics;
	bool sendError;
	char *sockEndpoints;
	int sockType;
	uchar *tplName;
	bool topicFrame;
} instanceData;

typedef struct wrkrInstanceData {
	instanceData *pData;
} wrkrInstanceData_t;

static struct cnfparamdescr actpdescr[] = {
	{ "endpoints", eCmdHdlrGetWord, 1 },
	{ "socktype", eCmdHdlrGetWord, 1 },
	{ "sendtimeout", eCmdHdlrGetWord, 0 },
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
    putenv("ZSYS_SIGHANDLER=false");
	pData->sock = zsock_new(pData->sockType);
	if(!pData->sock) {
		errmsg.LogError(0, RS_RET_NO_ERRCODE,
				"omczmq: new socket failed for endpoints: %s",
				pData->sockEndpoints);
		ABORT_FINALIZE(RS_RET_SUSPENDED);
	}
	zsock_set_sndtimeo(pData->sock, pData->sendTimeout);

	if(runModConf->authType) {	
		if (!strcmp(runModConf->authType, "CURVESERVER")) {
			zcert_t *serverCert = zcert_load(runModConf->serverCertPath);
			if(!serverCert) {
				errmsg.LogError(0, NO_ERRCODE, "could not load cert %s",
					runModConf->serverCertPath);
				ABORT_FINALIZE(RS_RET_ERR);
			}
			zsock_set_zap_domain(pData->sock, "global");
			zsock_set_curve_server(pData->sock, 1);
			zcert_apply(serverCert, pData->sock);
			zcert_destroy(&serverCert);
		} 
		else if(!strcmp(runModConf->authType, "CURVECLIENT")) {
			zcert_t *serverCert = zcert_load(runModConf->serverCertPath);
			if(!serverCert) {
				errmsg.LogError(0, NO_ERRCODE, "could not load cert %s",
					runModConf->serverCertPath);
				ABORT_FINALIZE(RS_RET_ERR);
			}
			const char *server_key = zcert_public_txt(serverCert);
			zcert_destroy(&serverCert);
			zsock_set_curve_serverkey(pData->sock, server_key);
			
			zcert_t *clientCert = zcert_load(runModConf->clientCertPath);
			if(!clientCert) {
				errmsg.LogError(0, NO_ERRCODE, "could not load cert %s",
					runModConf->clientCertPath);
				ABORT_FINALIZE(RS_RET_ERR);
			}
			
			zcert_apply(clientCert, pData->sock);
			zcert_destroy(&clientCert);
		}
	}

	switch(pData->sockType) {
		case ZMQ_PUB:
#if defined(ZMQ_RADIO)
		case ZMQ_RADIO:
#endif
			pData->serverish = true;
			break;
		case ZMQ_PUSH:
#if defined(ZMQ_SCATTER)
		case ZMQ_SCATTER:
#endif
		case ZMQ_DEALER:
#if defined(ZMQ_CLIENT)
		case ZMQ_CLIENT:
#endif
			pData->serverish = false;
			break;
	}

	int rc = zsock_attach(pData->sock, pData->sockEndpoints, pData->serverish);
	if(rc == -1) {
		errmsg.LogError(0, NO_ERRCODE, "zsock_attach to %s failed",
				pData->sockEndpoints);
		ABORT_FINALIZE(RS_RET_SUSPENDED);
	}

finalize_it:
	RETiRet;
}

rsRetVal outputCZMQ(uchar* msg, instanceData* pData) {
	DEFiRet;

	if(NULL == pData->sock) {
		CHKiRet(initCZMQ(pData));
	}

#if defined(ZMQ_RADIO)
	if((pData->sockType == ZMQ_PUB || pData->sockType == ZMQ_RADIO) && pData->topics) {
#else
	if(pData->sockType == ZMQ_PUB && pData->topics) {
#endif
		char *topic = zlist_first(pData->topics);

		while(topic) {
			if(pData->topicFrame && pData->sockType == ZMQ_SUB) {
				int rc = zstr_sendx(pData->sock, topic, (char*)msg, NULL);
				if(rc != 0) {
					pData->sendError = true;
					ABORT_FINALIZE(RS_RET_SUSPENDED);
				}
			} 
#if defined(ZMQ_RADIO)
			else if(pData->sockType == ZMQ_RADIO) {
				zframe_t *frame = zframe_from((char*)msg);
				if(!frame) {
					pData->sendError = true;
					ABORT_FINALIZE(RS_RET_SUSPENDED);
				}
				int rc = zframe_set_group(frame, topic);
				if(rc != 0) {
					pData->sendError = true;
					ABORT_FINALIZE(RS_RET_SUSPENDED);
				}
				rc = zframe_send(&frame, pData->sock, 0);
				if(rc != 0) {
					pData->sendError = true;
					ABORT_FINALIZE(RS_RET_SUSPENDED);
				}
			}
#endif
			else {
				int rc = zstr_sendf(pData->sock, "%s%s", topic, (char*)msg);
				if(rc != 0) {
					pData->sendError = true;
					ABORT_FINALIZE(RS_RET_SUSPENDED);
				}

			}
			topic = zlist_next(pData->topics);
		}
	}
	else {
		int rc = zstr_send(pData->sock, (char*)msg);
		if(rc != 0) {
			pData->sendError = true;
			DBGPRINTF("imczmq send error: %d", rc);
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
	pData->tplName = NULL;
	pData->sockType = -1;
	pData->sendTimeout = -1;
	pData->topics = NULL;
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
	zlist_destroy(&pData->topics);
	zsock_destroy(&pData->sock);
	free(pData->sockEndpoints);
	free(pData->tplName);
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
	iRet = initCZMQ(pData);
	pthread_mutex_unlock(&mutDoAct);
ENDtryResume

BEGINbeginCnfLoad
CODESTARTbeginCnfLoad
	runModConf = pModConf;
	runModConf->pConf = pConf;
	runModConf->authenticator = 0;
	runModConf->authType = NULL;
	runModConf->serverCertPath = NULL;
	runModConf->clientCertPath = NULL;
ENDbeginCnfLoad

BEGINcheckCnf
CODESTARTcheckCnf
ENDcheckCnf

BEGINactivateCnf
CODESTARTactivateCnf
	runModConf = pModConf;
	if(runModConf->authenticator == 1) {
		if(!authActor) {
			DBGPRINTF("imczmq: starting authActor\n");
			authActor = zactor_new(zauth, NULL);
			if(!strcmp(runModConf->clientCertPath, "*")) {
				zstr_sendx(authActor, "CURVE", CURVE_ALLOW_ANY, NULL);
			}
			else {
				zstr_sendx(authActor, "CURVE", runModConf->clientCertPath, NULL);
			}
			zsock_wait(authActor);
		}
	}
ENDactivateCnf

BEGINfreeCnf
CODESTARTfreeCnf
	free(pModConf->tplName);
	free(pModConf->authType);
	free(pModConf->serverCertPath);
	free(pModConf->clientCertPath);
	DBGPRINTF("imczmq: stopping authActor\n");
	zactor_destroy(&authActor);
ENDfreeCnf

BEGINsetModCnf
	struct cnfparamvals *pvals = NULL;
	int i;
CODESTARTsetModCnf
	pvals = nvlstGetParams(lst, &modpblk, NULL);
	if (pvals == NULL) {
		errmsg.LogError(0, RS_RET_MISSING_CNFPARAMS, "error processing module");
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	for (i=0; i<modpblk.nParams; ++i) {
		if(!pvals[i].bUsed) {
			DBGPRINTF("imczmq: pvals[i].bUSed continuing\n");
			continue;
		}
		if(!strcmp(modpblk.descr[i].name, "authenticator")) {
			runModConf->authenticator = (int)pvals[i].val.d.n;
		}
		else if(!strcmp(modpblk.descr[i].name, "authtype")) {
			runModConf->authType = es_str2cstr(pvals[i].val.d.estr, NULL);
			DBGPRINTF("omczmq: authtype set to %s\n", runModConf->authType);
		}
		else if(!strcmp(modpblk.descr[i].name, "servercertpath")) {
			runModConf->serverCertPath = es_str2cstr(pvals[i].val.d.estr, NULL);
			DBGPRINTF("omczmq: serverCertPath set to %s\n", runModConf->serverCertPath);
		}
		else if(!strcmp(modpblk.descr[i].name, "clientcertpath")) {
			runModConf->clientCertPath = es_str2cstr(pvals[i].val.d.estr, NULL);
			DBGPRINTF("omczmq: clientCertPath set to %s\n", runModConf->clientCertPath);
		}
		else {
			errmsg.LogError(0, RS_RET_INVALID_PARAMS, 
						"imczmq: config error, unknown "
						"param %s in setModCnf\n", 
						modpblk.descr[i].name);
		} 
	}	

	DBGPRINTF("omczmq: authenticator set to %d\n", runModConf->authenticator);
	DBGPRINTF("omczmq: authType set to %s\n", runModConf->authType);
	DBGPRINTF("omczmq: serverCertPath set to %s\n", runModConf->serverCertPath);
	DBGPRINTF("omczmq: clientCertPath set to %s\n", runModConf->clientCertPath);

finalize_it:
		if(pvals != NULL)
			cnfparamvalsDestruct(pvals, &modpblk);
ENDsetModCnf

BEGINendCnfLoad
CODESTARTendCnfLoad
	runModConf = NULL;
ENDendCnfLoad


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

	for(i = 0; i < actpblk.nParams; ++i) {
		if(!pvals[i].bUsed) {
			continue;
		}

		if(!strcmp(actpblk.descr[i].name, "endpoints")) {
			pData->sockEndpoints = es_str2cstr(pvals[i].val.d.estr, NULL);
		} 
		else if(!strcmp(actpblk.descr[i].name, "template")) {
			pData->tplName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		}
		else if(!strcmp(actpblk.descr[i].name, "sendtimeout")) {
			pData->sendTimeout = atoi(es_str2cstr(pvals[i].val.d.estr, NULL));
		}
		else if(!strcmp(actpblk.descr[i].name, "socktype")){
			char *stringType = es_str2cstr(pvals[i].val.d.estr, NULL);
			if(stringType != NULL){
				if(!strcmp("PUB", stringType)) {
					pData->sockType = ZMQ_PUB;
				}
#if defined(ZMQ_RADIO)
				else if(!strcmp("RADIO", stringType)) {
					pData->sockType = ZMQ_RADIO;
				}
#endif
				else if(!strcmp("PUSH", stringType)) {
					pData->sockType = ZMQ_PUSH;
				}
#if defined(ZMQ_SCATTER)
				else if(!strcmp("SCATTER", stringType)) {
					pData->sockType = ZMQ_SCATTER;
				}
#endif
				else if(!strcmp("DEALER", stringType)) {
					pData->sockType = ZMQ_DEALER;
				}
#if defined(ZMQ_CLIENT)
				else if(!strcmp("CLIENT", stringType)) {
					pData->sockType = ZMQ_DEALER;
				}
#endif
				free(stringType);
			}
			else{
				errmsg.LogError(0, RS_RET_OUT_OF_MEMORY,
						"omczmq: out of memory");
				ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
			}
		} 
		else if(!strcmp(actpblk.descr[i].name, "topicframe")) {
			int tframe  = atoi(es_str2cstr(pvals[i].val.d.estr, NULL));
			if(tframe == 1) {
				pData->topicFrame = true;
			}
			else {
				pData->topicFrame = false;
			}
		}
		else if(!strcmp(actpblk.descr[i].name, "topics")) {
			pData->topics = zlist_new();
			char *topics = es_str2cstr(pvals[i].val.d.estr, NULL);
			char *topics_org = topics;
			char topic[256];
			if(topics == NULL){
				errmsg.LogError(0, RS_RET_OUT_OF_MEMORY,
					"out of memory");
				ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
			}
			
			while(*topics) {
				char *delimiter = strchr(topics, ',');
				if (!delimiter) {
					delimiter = topics + strlen(topics);
				}
				memcpy (topic, topics, delimiter - topics);
				topic[delimiter-topics] = 0;
				char *current_topic = strdup(topic);
				zlist_append (pData->topics, current_topic);
				if(*delimiter == 0) {
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

	if (pData->tplName == NULL) {
		CHKiRet(OMSRsetEntry(*ppOMSR, 0, (uchar*)strdup("RSYSLOG_ForwardFormat"),
					OMSR_NO_RQD_TPL_OPTS));
	} 
	else {
		CHKiRet(OMSRsetEntry(*ppOMSR, 0, (uchar*)pData->tplName, OMSR_NO_RQD_TPL_OPTS));
	}

	CODE_STD_FINALIZERnewActInst
	cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst

BEGINparseSelectorAct
CODESTARTparseSelectorAct
	CODE_STD_STRING_REQUESTparseSelectorAct(1)

	if(!strncmp((char*) p, ":omczmq:", sizeof(":omczmq:") - 1)) { 
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
    CODEqueryEtryPt_STD_CONF2_QUERIES
	CODEqueryEtryPt_STD_CONF2_setModCnf_QUERIES
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
