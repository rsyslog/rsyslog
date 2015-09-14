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
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "cfsysline.h"
#include "dirty.h"
#include "errmsg.h"
#include "glbl.h"
#include "module-template.h"
#include "msg.h"
#include "net.h"
#include "parser.h"
#include "prop.h"
#include "ruleset.h"
#include "srUtils.h"
#include "unicode-helper.h"
#include <czmq.h>

MODULE_TYPE_INPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("imczmq");

DEF_IMOD_STATIC_DATA
DEFobjCurrIf(errmsg)
DEFobjCurrIf(glbl)
DEFobjCurrIf(prop)
DEFobjCurrIf(ruleset)

 
typedef struct _pollerData_t {
	ruleset_t *ruleset;
	thrdInfo_t *thread;
} pollerData_t;


struct instanceConf_s {
	bool is_server;
	char *beacon;
	int beaconPort;
	int sockType;
	char *sockEndpoints;
	char *topicList;
	char *authType;
	char *clientCertPath;
	char *serverCertPath;
	uchar *pszBindRuleset;
	ruleset_t *pBindRuleset;
	struct instanceConf_s *next;
};

struct modConfData_s {
	rsconf_t *pConf;
	instanceConf_t *root;
	instanceConf_t *tail;
	int io_threads;
};

struct lstn_s {
	zactor_t *beaconActor;
	zsock_t *sock;
	zcert_t *clientCert;
	zcert_t *serverCert;
	ruleset_t *pRuleset;
	struct lstn_s *next;
};

static modConfData_t *runModConf = NULL;
static struct lstn_s *lcnfRoot = NULL;
static struct lstn_s *lcnfLast = NULL;
static prop_t *s_namep = NULL;
static zloop_t *zloopHandle = NULL;
static zactor_t *authActor = NULL;

static struct cnfparamdescr modpdescr[] = {
	{ "ioThreads", eCmdHdlrInt, 0 },
};

static struct cnfparamblk modpblk = {
	CNFPARAMBLK_VERSION,
	sizeof(modpdescr)/sizeof(struct cnfparamdescr),
	modpdescr
};

static struct cnfparamdescr inppdescr[] = {
	{ "beacon", eCmdHdlrGetWord, 0 },
	{ "beaconport", eCmdHdlrGetWord, 0 },
	{ "endpoints", eCmdHdlrGetWord, 1 },
	{ "socktype", eCmdHdlrGetWord, 1 },
	{ "authtype", eCmdHdlrGetWord, 0 },
	{ "topics", eCmdHdlrGetWord, 0 },
	{ "clientcertpath", eCmdHdlrGetWord, 0 },
	{ "servercertpath", eCmdHdlrGetWord, 0 },
	{ "ruleset", eCmdHdlrGetWord, 0 },
};

#include "im-helper.h"

static struct cnfparamblk inppblk = {
	CNFPARAMBLK_VERSION,
	sizeof(inppdescr) / sizeof(struct cnfparamdescr),
	inppdescr
};

static void setDefaults(instanceConf_t* iconf) {
	iconf->is_server = false;
	iconf->beacon = NULL;
	iconf->beaconPort = -1;
	iconf->sockType = -1;
	iconf->sockEndpoints = NULL;
	iconf->topicList = NULL;
	iconf->authType = NULL;
	iconf->clientCertPath = NULL;
	iconf->serverCertPath = NULL;
	iconf->pszBindRuleset = NULL;
	iconf->pBindRuleset = NULL;
	iconf->next = NULL;
};

static rsRetVal createInstance(instanceConf_t** pinst) {
	DEFiRet;
	instanceConf_t* inst;
	CHKmalloc(inst = MALLOC(sizeof(instanceConf_t)));
	
	setDefaults(inst);
	
	if (runModConf->root == NULL || runModConf->tail == NULL) {
		runModConf->tail = runModConf->root = inst;
	}

	else {
		runModConf->tail->next = inst;
		runModConf->tail = inst;
	}
	*pinst = inst;
finalize_it:
	RETiRet;
}

static rsRetVal createListener(struct cnfparamvals* pvals) {
	instanceConf_t* inst;
	int i;
	DEFiRet;
	
	CHKiRet(createInstance(&inst));
	for(i = 0 ; i < inppblk.nParams ; ++i) {
		if(!pvals[i].bUsed) {
			continue;
		}

		/* get the ruleset to bind to */
		if(!strcmp(inppblk.descr[i].name, "ruleset")) {
			inst->pszBindRuleset = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
		} 
	
		/* get the socket endpoints to use */
		else if(!strcmp(inppblk.descr[i].name, "endpoints")) {
			inst->sockEndpoints = es_str2cstr(pvals[i].val.d.estr, NULL);
		} 

		/* get beacon argument */
		else if (!strcmp(inppblk.descr[i].name, "beacon")) {
			inst->beacon = es_str2cstr(pvals[i].val.d.estr, NULL);
		}
		
		/* get beacon port */
		else if (!strcmp(inppblk.descr[i].name, "beaconport")) {
			inst->beaconPort = atoi(es_str2cstr(pvals[i].val.d.estr, NULL));
		}

		/* get the socket type */
		else if(!strcmp(inppblk.descr[i].name, "socktype")){
			char *stringType = es_str2cstr(pvals[i].val.d.estr, NULL);
			
			if (!strcmp("SUB", stringType)) {
				inst->sockType = ZMQ_SUB;
			}

			else if (!strcmp("PULL", stringType)) {
				inst->sockType = ZMQ_PULL;
			}

			else {
				errmsg.LogError(0, RS_RET_CONFIG_ERROR,
						"imczmq: invalid sockType");
				ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
			}
		} 
	
		/* get the subscription topics */
		else if(!strcmp(inppblk.descr[i].name, "topics")) {
			if (inst->sockType != ZMQ_SUB) {
				errmsg.LogError(0, RS_RET_CONFIG_ERROR,
						"topics is invalid unless socktype is SUB");
				ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
			}

			inst->topicList = es_str2cstr(pvals[i].val.d.estr, NULL);
		} 
	
		/* get the authentication type to use */
		else if(!strcmp(inppblk.descr[i].name, "authtype")) {
			inst->authType = es_str2cstr(pvals[i].val.d.estr, NULL);

			/* make sure defined type is supported */
			if ((inst->authType != NULL) && 
					strcmp("CURVESERVER", inst->authType) &&
					strcmp("CURVECLIENT", inst->authType)) {

				errmsg.LogError(0, RS_RET_CONFIG_ERROR,
						"imczmq: %s is not a valid authType",
						inst->authType);
				ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
			}
		} 

		/* get client cert argument */
		else if(!strcmp(inppblk.descr[i].name, "clientcertpath")) {
			inst->clientCertPath = es_str2cstr(pvals[i].val.d.estr, NULL);
		} 

		/* get server cert argument */
		else if(!strcmp(inppblk.descr[i].name, "servercertpath")) {
			inst->serverCertPath = es_str2cstr(pvals[i].val.d.estr, NULL);
		} 

		/* the config has a bad option */
		else {
			errmsg.LogError(0, NO_ERRCODE,
					"imczmq: program error, non-handled "
					"param '%s'\n", inppblk.descr[i].name);
		}
	
	}
finalize_it:
	RETiRet;
}

static rsRetVal addListener(instanceConf_t* iconf){
	struct lstn_s* pData;
	DEFiRet;

	CHKmalloc(pData=(struct lstn_s*)MALLOC(sizeof(struct lstn_s)));
	pData->next = NULL;
	pData->pRuleset = iconf->pBindRuleset;

	/* create our zeromq socket */
	pData->sock = zsock_new(iconf->sockType);
   	if (!pData->sock) {
		errmsg.LogError(0, RS_RET_NO_ERRCODE,
				"imczmq: new socket failed for endpoints: %s",
				iconf->sockEndpoints);
		ABORT_FINALIZE(RS_RET_NO_ERRCODE);
	}

	/* if a beacon is set start it */
	if((iconf->beacon != NULL) && (iconf->beaconPort > 0)) {
		DBGPRINTF ("imczmq: starting beacon actor...\n");

		/* create the beacon actor, if it fails abort */	
		pData->beaconActor = zactor_new(zbeacon, NULL);
		if (!pData->beaconActor) {
			errmsg.LogError(0, RS_RET_NO_ERRCODE,
					"imczmq: could not create beacon service");
			ABORT_FINALIZE (RS_RET_NO_ERRCODE);
		}

		/* configure the beacon and abort if UDP broadcasting ont available */
		zsock_send(pData->beaconActor, "si", "CONFIGURE", iconf->beaconPort);
		char *hostname = zstr_recv(pData->beaconActor);
		if (!*hostname) {
			errmsg.LogError(0, RS_RET_NO_ERRCODE,
					"imczmq: no UDP broadcasting available");
			ABORT_FINALIZE (RS_RET_NO_ERRCODE);
		}

		/* start publishing the beacon */
		zsock_send(pData->beaconActor, "sbi", "PUBLISH", pData->beaconActor, strlen(iconf->beacon));
	}

	DBGPRINTF("imczmq: authtype is: %s\n", iconf->authType);

	/* if we are a CURVE server */
	if ((iconf->authType != NULL) && (!strcmp(iconf->authType, "CURVESERVER"))) {

		iconf->is_server = true;

		/* set global auth domain */
		zsock_set_zap_domain(pData->sock, "global");

		/* set that we are a curve server */
		zsock_set_curve_server(pData->sock, 1);

		/* get and set our server cert */
		DBGPRINTF("imczmq: server cert is %s...\n", iconf->serverCertPath);
		pData->serverCert = zcert_load(iconf->serverCertPath);
		if (!pData->serverCert) {
			errmsg.LogError(0, NO_ERRCODE, "could not load server cert");
			ABORT_FINALIZE(RS_RET_ERR);
		}

		zcert_apply(pData->serverCert, pData->sock);

		/* set allowed clients */
		DBGPRINTF("imczmq: allowed clients are %s...\n", iconf->clientCertPath);
		zstr_sendx(authActor, "CURVE", iconf->clientCertPath, NULL);
		zsock_wait(authActor);
	}

	/* if we are a CURVE client */
	if ((iconf->authType != NULL) && (!strcmp(iconf->authType, "CURVECLIENT"))) {
		DBGPRINTF("imczmq: we are a curve client...\n");

		iconf->is_server = false;

		/* get our client cert */
		pData->clientCert = zcert_load(iconf->clientCertPath);
		if (!pData->clientCert) {
			errmsg.LogError(0, NO_ERRCODE, "could not load client cert");
			ABORT_FINALIZE(RS_RET_ERR);
		}

		/* apply the client cert to the socket */
		zcert_apply(pData->clientCert, pData->sock);

		/* get the server cert */
		DBGPRINTF("omczmq: server cert is %s...\n", iconf->serverCertPath);
		pData->serverCert = zcert_load(iconf->serverCertPath);
		if (!pData->serverCert) {
			errmsg.LogError(0, NO_ERRCODE, "could not load server cert");
			ABORT_FINALIZE(RS_RET_ERR);
		}

		/* get the server public key and set it for the socket */
		char *server_key = zcert_public_txt(pData->serverCert);
		DBGPRINTF("omczmq: server public key is %s...\n", server_key);
		zsock_set_curve_serverkey (pData->sock, server_key);
	}

	/* if we have a ZMQ_SUB sock, subscribe to topics */
	if (iconf->sockType == ZMQ_SUB) {
		iconf->is_server = false;

		char topic[256], *list = iconf->topicList;
		while (list) {
			char *delimiter = strchr(list, ',');
			if (!delimiter) {
				delimiter = list + strlen (list);
			}

			if (delimiter - list > 255) {
				errmsg.LogError(0, NO_ERRCODE,
						"iconf->topicList must be under 256 characters");
				ABORT_FINALIZE(RS_RET_ERR);
			}
		
			memcpy(topic, list, delimiter - list);
			topic[delimiter - list] = 0;

			zsock_set_subscribe(pData->sock, topic);

			if (*delimiter == 0) {
				break;
			}

			list = delimiter + 1;
		}
	}

	int rc = zsock_attach(pData->sock, (const char*)iconf->sockEndpoints,
			iconf->is_server);
	if (rc == -1) {
		errmsg.LogError(0, NO_ERRCODE, "zsock_attach to %s",
				iconf->sockEndpoints);
		ABORT_FINALIZE(RS_RET_ERR);
	}

	/* add this struct to the global */
	if(lcnfRoot == NULL) {
		lcnfRoot = pData;
	} 
	if(lcnfLast == NULL) {
		lcnfLast = pData;
	} else {
		lcnfLast->next = pData;
		lcnfLast = pData;
	}

finalize_it:
	RETiRet;
}

static int handlePoll(zloop_t __attribute__((unused)) *loop, zmq_pollitem_t *poller, void* args) {
	msg_t* pMsg;
	pollerData_t *pollerData = (pollerData_t *)args;

	zmsg_t *msg = zmsg_recv(poller->socket);
	zframe_t *payload = zmsg_last(msg);
	char* buf = zframe_strdup(payload);
	
	if (msgConstruct(&pMsg) == RS_RET_OK) {
		MsgSetRawMsg(pMsg, buf, strlen(buf));
		MsgSetInputName(pMsg, s_namep);
		MsgSetHOSTNAME(pMsg, glbl.GetLocalHostName(), ustrlen(glbl.GetLocalHostName()));
		MsgSetRcvFrom(pMsg, glbl.GetLocalHostNameProp());
		MsgSetRcvFromIP(pMsg, glbl.GetLocalHostIP());
		MsgSetMSGoffs(pMsg, 0);
		MsgSetFlowControlType(pMsg, eFLOWCTL_NO_DELAY);
		MsgSetRuleset(pMsg, pollerData->ruleset);
		pMsg->msgFlags = NEEDS_PARSING | PARSE_HOSTNAME;
		submitMsg2(pMsg);
	}
	
	free(buf);
	zmsg_destroy(&msg);
	
	if( pollerData->thread->bShallStop == TRUE) {
		return -1; 
	}
	
	return 0;
}

static rsRetVal rcvData(thrdInfo_t* pThrd){
	DEFiRet;

	DBGPRINTF("imczmq: starting auth actor...\n");
	authActor = zactor_new(zauth, NULL);
	if (!authActor) {
		errmsg.LogError(0, RS_RET_NO_ERRCODE,
				"imczmq: could not create auth service");
		ABORT_FINALIZE(RS_RET_NO_ERRCODE);
	}

	instanceConf_t *inst;
	for (inst = runModConf->root; inst != NULL; inst=inst->next) {
		addListener(inst);
	}

	if (lcnfRoot == NULL) {
		errmsg.LogError(0, NO_ERRCODE, "imczmq: no listeners were "
						"started, input not activated.\n");
		ABORT_FINALIZE(RS_RET_NO_RUN);
	}

	size_t n_items = 0;
	struct lstn_s *current;
	for(current=lcnfRoot;current!=NULL;current=current->next) {
		n_items++;
	}

	zmq_pollitem_t *items = NULL;
	CHKmalloc(items = (zmq_pollitem_t*)MALLOC(sizeof(zmq_pollitem_t)*n_items));

	pollerData_t *pollerData = NULL;
	CHKmalloc(pollerData = (pollerData_t *)MALLOC(sizeof(pollerData_t)*n_items));

	size_t i;
	for(i=0, current = lcnfRoot; current != NULL; current = current->next, i++) {
		items[i].socket=zsock_resolve(current->sock);
		items[i].events = ZMQ_POLLIN;
		
		pollerData[i].thread  = pThrd;
		pollerData[i].ruleset = current->pRuleset;
	}

	zloopHandle = zloop_new();
	int rc;
	for(i=0; i<n_items; ++i) {
		
		rc = zloop_poller(zloopHandle, &items[i], handlePoll, &pollerData[i]);
		if (rc) {
			errmsg.LogError(0, NO_ERRCODE,
					"imczmq: zloop_poller failed for item %zu: %s",
					i, zmq_strerror(errno));
		} 
	}
	DBGPRINTF("imczmq: zloop_poller starting...");
	zloop_start(zloopHandle);   
	zloop_destroy(&zloopHandle);
	DBGPRINTF("imczmq: zloop_poller stopped.");
finalize_it:
	free(items);
	free(pollerData);
	zactor_destroy(&authActor);
	RETiRet;
}

BEGINrunInput
CODESTARTrunInput
	CHKiRet(rcvData(pThrd));
finalize_it:
	RETiRet;
ENDrunInput


BEGINwillRun
CODESTARTwillRun
	CHKiRet(prop.Construct(&s_namep));
	CHKiRet(prop.SetString(s_namep,
				UCHAR_CONSTANT("imczmq"),
				   sizeof("imczmq") - 1));

	CHKiRet(prop.ConstructFinalize(s_namep));

finalize_it:
ENDwillRun


BEGINafterRun
CODESTARTafterRun
	if (s_namep != NULL) {
		prop.Destruct(&s_namep);
	}
ENDafterRun


BEGINmodExit
CODESTARTmodExit
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(glbl, CORE_COMPONENT);
	objRelease(prop, CORE_COMPONENT);
	objRelease(ruleset, CORE_COMPONENT);
ENDmodExit


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if (eFeat == sFEATURENonCancelInputTermination) {
		iRet = RS_RET_OK;
	}
ENDisCompatibleWithFeature


BEGINbeginCnfLoad
CODESTARTbeginCnfLoad
	runModConf = pModConf;
	runModConf->pConf = pConf;
	runModConf->io_threads = 0;
ENDbeginCnfLoad


BEGINsetModCnf
	struct cnfparamvals* pvals = NULL;
	int i;
CODESTARTsetModCnf
	pvals = nvlstGetParams(lst, &modpblk, NULL);
	if (NULL == pvals) {
		 errmsg.LogError(0, RS_RET_MISSING_CNFPARAMS,
				"imczmq: error processing module "
				"config parameters ['module(...)']");

		 ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}
 
	for (i=0; i < modpblk.nParams; ++i) {
		if (!pvals[i].bUsed) {
			continue;
		}

		if (!strcmp(modpblk.descr[i].name, "ioThreads")) {
			runModConf->io_threads = (int)pvals[i].val.d.n;
		}

		else {
			errmsg.LogError(0, RS_RET_INVALID_PARAMS, 
						"imczmq: config error, unknown "
						"param %s in setModCnf\n", 
						modpblk.descr[i].name);
		}   
	}
 
finalize_it:
	if (pvals != NULL) {
		cnfparamvalsDestruct(pvals, &modpblk);
	}
ENDsetModCnf


BEGINendCnfLoad
CODESTARTendCnfLoad
ENDendCnfLoad


/* function to generate error message if framework does not find requested ruleset */
static inline void
std_checkRuleset_genErrMsg(__attribute__((unused)) modConfData_t *modConf, instanceConf_t *inst)
{
	errmsg.LogError(0, NO_ERRCODE, "imczmq: ruleset '%s' for socket %s not found - "
					"using default ruleset instead", inst->pszBindRuleset,
					inst->sockEndpoints);
}


BEGINcheckCnf
instanceConf_t* inst;
CODESTARTcheckCnf
	for(inst = pModConf->root; inst!=NULL; inst=inst->next) {
		std_checkRuleset(pModConf, inst);
	}
ENDcheckCnf


BEGINactivateCnfPrePrivDrop
CODESTARTactivateCnfPrePrivDrop
	runModConf = pModConf;

	/* tell czmq to not use it's own signal handler */
	putenv("ZSYS_SIGHANDLER=false");
ENDactivateCnfPrePrivDrop


BEGINactivateCnf
CODESTARTactivateCnf
ENDactivateCnf


BEGINfreeCnf
	struct lstn_s *lstn, *lstn_r;
	instanceConf_t *inst, *inst_r;
CODESTARTfreeCnf
	DBGPRINTF("imczmq: BEGINfreeCnf ...\n");
	for (lstn = lcnfRoot; lstn != NULL; ) {
		lstn_r = lstn;
		zcert_destroy(&lstn->clientCert);
		zcert_destroy(&lstn->serverCert);
		lstn = lstn_r->next;
		free(lstn_r);
	}

	for (inst = pModConf->root ; inst != NULL ; ) {
		free(inst->pszBindRuleset);
		free(inst->sockEndpoints);
		free(inst->topicList);
		free(inst->authType);
		free(inst->clientCertPath);
		free(inst->serverCertPath);
		inst_r = inst;
		inst = inst->next;
		free(inst_r);
	}

ENDfreeCnf


BEGINnewInpInst
	struct cnfparamvals* pvals;
CODESTARTnewInpInst

	DBGPRINTF("newInpInst (imczmq)\n");
	pvals = nvlstGetParams(lst, &inppblk, NULL);
	
	if(NULL==pvals) {
		errmsg.LogError(0, RS_RET_MISSING_CNFPARAMS,
						"imczmq: required parameters are missing\n");
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	DBGPRINTF("imczmq: input param blk:\n");
	cnfparamsPrint(&inppblk, pvals);
	
	CHKiRet(createListener(pvals));

finalize_it:
CODE_STD_FINALIZERnewInpInst
	cnfparamvalsDestruct(pvals, &inppblk);
ENDnewInpInst


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
	*ipIFVersProvided = CURR_MOD_IF_VERSION;
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(prop, CORE_COMPONENT));
	CHKiRet(objUse(ruleset, CORE_COMPONENT));
ENDmodInit
