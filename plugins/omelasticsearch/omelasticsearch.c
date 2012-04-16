/* omelasticsearch.c
 * This is the http://www.elasticsearch.org/ output module.
 *
 * NOTE: read comments in module-template.h for more specifics!
 *
 * Copyright 2011 Nathan Scott.
 * Copyright 2009-2012 Rainer Gerhards and Adiscon GmbH.
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
#include <curl/curl.h>
//#include <curl/types.h>
#include <curl/easy.h>
#include <assert.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include "statsobj.h"
#include "cfsysline.h"
#include "unicode-helper.h"

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("omelasticsearch")

/* internal structures */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(errmsg)
DEFobjCurrIf(statsobj)

statsobj_t *indexStats;
STATSCOUNTER_DEF(indexConFail, mutIndexConFail)
STATSCOUNTER_DEF(indexSubmit, mutIndexSubmit)
STATSCOUNTER_DEF(indexFailed, mutIndexFailed)
STATSCOUNTER_DEF(indexSuccess, mutIndexSuccess)

/* REST API for elasticsearch hits this URL:
 * http://<hostName>:<restPort>/<searchIndex>/<searchType>
 */
typedef struct curl_slist HEADER;
typedef struct _instanceData {
	uchar *server;
	int port;
	uchar *searchIndex;
	uchar *searchType;
	uchar *tplName;
	uchar *timeout;
	sbool dynSrchIdx;
	sbool dynSrchType;
	sbool asyncRepl;
	CURL	*curlHandle;	/* libcurl session handle */
	HEADER	*postHeader;	/* json POST request info */
} instanceData;


/* tables for interfacing with the v6 config system */
/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {
	{ "server", eCmdHdlrGetWord, 0 },
	{ "serverport", eCmdHdlrInt, 0 },
	{ "searchindex", eCmdHdlrGetWord, 0 },
	{ "searchtype", eCmdHdlrGetWord, 0 },
	{ "dynsearchindex", eCmdHdlrBinary, 0 },
	{ "dynsearchtype", eCmdHdlrBinary, 0 },
	{ "asyncrepl", eCmdHdlrBinary, 0 },
	{ "timeout", eCmdHdlrGetWord, 0 },
	{ "template", eCmdHdlrGetWord, 1 }
};
static struct cnfparamblk actpblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(actpdescr)/sizeof(struct cnfparamdescr),
	  actpdescr
	};

BEGINcreateInstance
CODESTARTcreateInstance
ENDcreateInstance

BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURERepeatedMsgReduction)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature

BEGINfreeInstance
CODESTARTfreeInstance
	if (pData->postHeader) {
		curl_slist_free_all(pData->postHeader);
		pData->postHeader = NULL;
	}
	if (pData->curlHandle) {
		curl_easy_cleanup(pData->curlHandle);
		pData->curlHandle = NULL;
	}
	free(pData->server);
	free(pData->searchIndex);
	free(pData->searchType);
	free(pData->tplName);
ENDfreeInstance

BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
ENDdbgPrintInstInfo

BEGINtryResume
CODESTARTtryResume
ENDtryResume


static rsRetVal
setCurlURL(instanceData *pData, uchar *tpl1, uchar *tpl2)
{
	char restURL[2048];	/* libcurl makes a copy, using the stack here is OK */
	uchar *searchIndex;
	uchar *searchType;

	if(pData->dynSrchIdx) {
		searchIndex = tpl1;
		if(pData->dynSrchType)
			searchType = tpl2;
		else 
			searchType = pData->searchType;
	} else {
		searchIndex = pData->searchIndex;
		if(pData->dynSrchType)
			searchType = tpl1;
		else 
			searchType = pData->searchType;
	}
	if(pData->asyncRepl) {
		if(pData->timeout != NULL) {
			snprintf(restURL, sizeof(restURL)-1, "http://%s:%d/%s/%s?"
				"replication=async&timeout=%s",
				pData->server, pData->port, searchIndex, searchType,
				pData->timeout);
		} else {
			snprintf(restURL, sizeof(restURL)-1, "http://%s:%d/%s/%s?"
				"replication=async",
				pData->server, pData->port, searchIndex, searchType);
		}
	} else {
		snprintf(restURL, sizeof(restURL)-1, "http://%s:%d/%s/%s",
			pData->server, pData->port, searchIndex, searchType);
	}
	curl_easy_setopt(pData->curlHandle, CURLOPT_URL, restURL); 
	DBGPRINTF("omelasticsearch: using REST URL: '%s'\n", restURL);
	return RS_RET_OK;
}

static rsRetVal
curlPost(instanceData *instance, uchar *message, uchar *tpl1, uchar *tpl2)
{
	CURLcode code;
	CURL *curl = instance->curlHandle;
	int length = strlen((char *)message);
	DEFiRet;

	if(instance->dynSrchIdx || instance->dynSrchType)
		CHKiRet(setCurlURL(instance, tpl1, tpl2));

	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (char *)message);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, (char *)message); 
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, length); 
	code = curl_easy_perform(curl);
	switch (code) {
		case CURLE_COULDNT_RESOLVE_HOST:
		case CURLE_COULDNT_RESOLVE_PROXY:
		case CURLE_COULDNT_CONNECT:
		case CURLE_WRITE_ERROR:
			STATSCOUNTER_INC(indexConFail, mutIndexConFail);
			return RS_RET_SUSPENDED;
		default:
			STATSCOUNTER_INC(indexSubmit, mutIndexSubmit);
			return RS_RET_OK;
	}
finalize_it:
	RETiRet;
}

BEGINdoAction
CODESTARTdoAction
	CHKiRet(curlPost(pData, ppString[0], ppString[1], ppString[2]));
finalize_it:
ENDdoAction

/* elasticsearch POST result string ... useful for debugging */
size_t
curlResult(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	unsigned int i;
	char *p = (char *)ptr;
	char *jsonData = (char *)userdata;
	static char ok[] = "{\"ok\":true,";

	ASSERT(size == 1);

	if (size == 1 &&
	    nmemb > sizeof(ok)-1 &&
	    strncmp(p, ok, sizeof(ok)-1) == 0) {
		STATSCOUNTER_INC(indexSuccess, mutIndexSuccess);
	} else {
		STATSCOUNTER_INC(indexFailed, mutIndexFailed);
		if (Debug) {
			DBGPRINTF("omelasticsearch request: %s\n", jsonData);
			DBGPRINTF("omelasticsearch result: ");
			for (i = 0; i < nmemb; i++)
				DBGPRINTF("%c", p[i]);
			DBGPRINTF("\n");
		}
	}
	return size * nmemb;
}


static rsRetVal
curlSetup(instanceData *pData)
{
	HEADER *header;
	CURL *handle;

	handle = curl_easy_init();
	if (handle == NULL) {
		return RS_RET_OBJ_CREATION_FAILED;
	}

	header = curl_slist_append(NULL, "Content-Type: text/json; charset=utf-8");
	curl_easy_setopt(handle, CURLOPT_HTTPHEADER, header); 

	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, curlResult);
	curl_easy_setopt(handle, CURLOPT_POST, 1); 

	pData->curlHandle = handle;
	pData->postHeader = header;

	if(pData->dynSrchIdx == 0 && pData->dynSrchType == 0) {
		/* in this case, we know no tpls are involved --> NULL OK! */
		setCurlURL(pData, NULL, NULL);
	}

	if(Debug) {
		if(pData->dynSrchIdx == 0 && pData->dynSrchType == 0)
			dbgprintf("omelasticsearch setup, using static REST URL\n");
		else
			dbgprintf("omelasticsearch setup, we have a dynamic REST URL\n");
	}
	return RS_RET_OK;
}

static inline void
setInstParamDefaults(instanceData *pData)
{
	pData->server = NULL;
	pData->port = 9200;
	pData->searchIndex = NULL;
	pData->searchType = NULL;
	pData->timeout = NULL;
	pData->dynSrchIdx = 0;
	pData->dynSrchType = 0;
	pData->asyncRepl = 0;
	pData->tplName = NULL;
}

BEGINnewActInst
	struct cnfparamvals *pvals;
	int i;
	int iNumTpls;
CODESTARTnewActInst
	if((pvals = nvlstGetParams(lst, &actpblk, NULL)) == NULL) {
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	CHKiRet(createInstance(&pData));
	setInstParamDefaults(pData);

	for(i = 0 ; i < actpblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(actpblk.descr[i].name, "server")) {
			pData->server = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "serverport")) {
			pData->port = (int) pvals[i].val.d.n, NULL;
		} else if(!strcmp(actpblk.descr[i].name, "searchindex")) {
			pData->searchIndex = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "searchtype")) {
			pData->searchType = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "dynsearchindex")) {
			pData->dynSrchIdx = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "dynsearchtype")) {
			pData->dynSrchType = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "timeout")) {
			pData->timeout = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "asyncrepl")) {
			pData->asyncRepl = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "template")) {
			pData->tplName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else {
			dbgprintf("omelasticsearch: program error, non-handled "
			  "param '%s'\n", actpblk.descr[i].name);
		}
	}
	
	if(pData->dynSrchIdx && pData->searchIndex == NULL) {
		errmsg.LogError(0, RS_RET_LEGA_ACT_NOT_SUPPORTED,
			"omelasticsearch: requested dynamic search index, but no "
			"name for index template given - action definition invalid");
		ABORT_FINALIZE(RS_RET_LEGA_ACT_NOT_SUPPORTED);
	}
	if(pData->dynSrchType && pData->searchType == NULL) {
		errmsg.LogError(0, RS_RET_LEGA_ACT_NOT_SUPPORTED,
			"omelasticsearch: requested dynamic search type, but no "
			"name for type template given - action definition invalid");
		ABORT_FINALIZE(RS_RET_LEGA_ACT_NOT_SUPPORTED);
	}

	iNumTpls = 1;
	if(pData->dynSrchIdx) ++iNumTpls;
	if(pData->dynSrchType) ++iNumTpls;
	DBGPRINTF("omelasticsearch: requesting %d templates\n", iNumTpls);
	CODE_STD_STRING_REQUESTparseSelectorAct(iNumTpls)

	CHKiRet(OMSRsetEntry(*ppOMSR, 0, (uchar*)strdup((pData->tplName == NULL) ?
					    " StdJSONFmt" : (char*)pData->tplName),
		OMSR_NO_RQD_TPL_OPTS));


	/* we need to request additional templates. If we have a dynamic search index,
	 * it will always be string 1. Type may be 1 or 2, depending on whether search
	 * index is dynamic as well. Rule needs to be followed throughout the module.
	 */
	if(pData->dynSrchIdx) {
		CHKiRet(OMSRsetEntry(*ppOMSR, 1, ustrdup(pData->searchIndex),
			OMSR_NO_RQD_TPL_OPTS));
		if(pData->dynSrchType) {
			CHKiRet(OMSRsetEntry(*ppOMSR, 2, ustrdup(pData->searchType),
				OMSR_NO_RQD_TPL_OPTS));
		}
	} else {
		if(pData->dynSrchType) {
			CHKiRet(OMSRsetEntry(*ppOMSR, 1, ustrdup(pData->searchType),
				OMSR_NO_RQD_TPL_OPTS));
		}
	}

	if(pData->server == NULL)
		pData->server = (uchar*) strdup("localhost");
	if(pData->searchIndex == NULL)
		pData->searchIndex = (uchar*) strdup("system");
	if(pData->searchType == NULL)
		pData->searchType = (uchar*) strdup("events");

	CHKiRet(curlSetup(pData));

CODE_STD_FINALIZERnewActInst
	cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst


BEGINparseSelectorAct
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	if(!strncmp((char*) p, ":omelasticsearch:", sizeof(":omelasticsearch:") - 1)) {
		errmsg.LogError(0, RS_RET_LEGA_ACT_NOT_SUPPORTED,
			"omelasticsearch supports only v6 config format, use: "
			"action(type=\"omelasticsearch\" server=...)");
	}
	ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINmodExit
CODESTARTmodExit
	curl_global_cleanup();
	statsobj.Destruct(&indexStats);
	objRelease(errmsg, CORE_COMPONENT);
        objRelease(statsobj, CORE_COMPONENT);
ENDmodExit

BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
CODEqueryEtryPt_IsCompatibleWithFeature_IF_OMOD_QUERIES
CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(statsobj, CORE_COMPONENT));

	if (curl_global_init(CURL_GLOBAL_ALL) != 0) {
		errmsg.LogError(0, RS_RET_OBJ_CREATION_FAILED, "CURL fail. -elasticsearch indexing disabled");
		ABORT_FINALIZE(RS_RET_OBJ_CREATION_FAILED);
	}

	/* support statistics gathering */
	CHKiRet(statsobj.Construct(&indexStats));
	CHKiRet(statsobj.SetName(indexStats, (uchar *)"elasticsearch"));
	CHKiRet(statsobj.AddCounter(indexStats, (uchar *)"connfail",
		ctrType_IntCtr, &indexConFail));
	CHKiRet(statsobj.AddCounter(indexStats, (uchar *)"submits",
		ctrType_IntCtr, &indexSubmit));
	CHKiRet(statsobj.AddCounter(indexStats, (uchar *)"failed",
		ctrType_IntCtr, &indexFailed));
	CHKiRet(statsobj.AddCounter(indexStats, (uchar *)"success",
		ctrType_IntCtr, &indexSuccess));
	CHKiRet(statsobj.ConstructFinalize(indexStats));
ENDmodInit

/* vi:set ai:
 */
