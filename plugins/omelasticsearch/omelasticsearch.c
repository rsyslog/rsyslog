/* omelasticsearch.c
 * This is the http://www.elasticsearch.org/ output module.
 *
 * NOTE: read comments in module-template.h for more specifics!
 *
 * Copyright 2011 Nathan Scott.
 * Copyright 2009-2014 Rainer Gerhards and Adiscon GmbH.
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
#include <memory.h>
#include <string.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <assert.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "cJSON/cjson.h"
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
STATSCOUNTER_DEF(indexSubmit, mutIndexSubmit)
STATSCOUNTER_DEF(indexHTTPFail, mutIndexHTTPFail)
STATSCOUNTER_DEF(indexHTTPReqFail, mutIndexHTTPReqFail)
STATSCOUNTER_DEF(indexESFail, mutIndexESFail)

/* REST API for elasticsearch hits this URL:
 * http://<hostName>:<restPort>/<searchIndex>/<searchType>
 */
typedef struct curl_slist HEADER;
typedef struct _instanceData {
	int port;
	int fdErrFile;		/* error file fd or -1 if not open */
	pthread_mutex_t mutErrFile;
	uchar *server;
	uchar *uid;
	uchar *pwd;
	uchar *searchIndex;
	uchar *searchType;
	uchar *parent;
	uchar *tplName;
	uchar *timeout;
	uchar *bulkId;
	uchar *errorFile;
	sbool errorOnly;
	sbool interleaved;
	sbool dynSrchIdx;
	sbool dynSrchType;
	sbool dynParent;
	sbool dynBulkId;
	sbool bulkmode;
	sbool asyncRepl;
        sbool useHttps;
} instanceData;

typedef struct wrkrInstanceData {
	instanceData *pData;
	int replyLen;
	char *reply;
	CURL	*curlHandle;	/* libcurl session handle */
	HEADER	*postHeader;	/* json POST request info */
	uchar *restURL;		/* last used URL for error reporting */
	struct {
		es_str_t *data;
		int nmemb;	/* number of messages in batch (for statistics counting) */
		uchar *currTpl1;
		uchar *currTpl2;
	} batch;
} wrkrInstanceData_t;

/* tables for interfacing with the v6 config system */
/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {
	{ "server", eCmdHdlrGetWord, 0 },
	{ "serverport", eCmdHdlrInt, 0 },
	{ "uid", eCmdHdlrGetWord, 0 },
	{ "pwd", eCmdHdlrGetWord, 0 },
	{ "searchindex", eCmdHdlrGetWord, 0 },
	{ "searchtype", eCmdHdlrGetWord, 0 },
	{ "parent", eCmdHdlrGetWord, 0 },
	{ "dynsearchindex", eCmdHdlrBinary, 0 },
	{ "dynsearchtype", eCmdHdlrBinary, 0 },
	{ "dynparent", eCmdHdlrBinary, 0 },
	{ "bulkmode", eCmdHdlrBinary, 0 },
	{ "asyncrepl", eCmdHdlrBinary, 0 },
        { "usehttps", eCmdHdlrBinary, 0 },
	{ "timeout", eCmdHdlrGetWord, 0 },
	{ "errorfile", eCmdHdlrGetWord, 0 },
	{ "erroronly", eCmdHdlrBinary, 0 },
	{ "interleaved", eCmdHdlrBinary, 0 },
	{ "template", eCmdHdlrGetWord, 0 },
	{ "dynbulkid", eCmdHdlrBinary, 0 },
	{ "bulkid", eCmdHdlrGetWord, 0 },
};
static struct cnfparamblk actpblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(actpdescr)/sizeof(struct cnfparamdescr),
	  actpdescr
	};

static rsRetVal curlSetup(wrkrInstanceData_t *pWrkrData, instanceData *pData);

BEGINcreateInstance
CODESTARTcreateInstance
	pData->fdErrFile = -1;
	pthread_mutex_init(&pData->mutErrFile, NULL);
ENDcreateInstance

BEGINcreateWrkrInstance
CODESTARTcreateWrkrInstance
dbgprintf("omelasticsearch: createWrkrInstance\n");
	pWrkrData->restURL = NULL;
	if(pData->bulkmode) {
		pWrkrData->batch.currTpl1 = NULL;
		pWrkrData->batch.currTpl2 = NULL;
		if((pWrkrData->batch.data = es_newStr(1024)) == NULL) {
			DBGPRINTF("omelasticsearch: error creating batch string "
			          "turned off bulk mode\n");
			pData->bulkmode = 0; /* at least it works */
		}
	}
	CHKiRet(curlSetup(pWrkrData, pWrkrData->pData));
finalize_it:
dbgprintf("DDDD: createWrkrInstance,pData %p/%p, pWrkrData %p\n", pData, pWrkrData->pData, pWrkrData);
ENDcreateWrkrInstance

BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURERepeatedMsgReduction)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature

BEGINfreeInstance
CODESTARTfreeInstance
	if(pData->fdErrFile != -1)
		close(pData->fdErrFile);
	pthread_mutex_destroy(&pData->mutErrFile);
	free(pData->server);
	free(pData->uid);
	free(pData->pwd);
	free(pData->searchIndex);
	free(pData->searchType);
	free(pData->parent);
	free(pData->tplName);
	free(pData->timeout);
	free(pData->errorFile);
	free(pData->bulkId);
ENDfreeInstance

BEGINfreeWrkrInstance
CODESTARTfreeWrkrInstance
	if(pWrkrData->postHeader) {
		curl_slist_free_all(pWrkrData->postHeader);
		pWrkrData->postHeader = NULL;
	}
	if(pWrkrData->curlHandle) {
		curl_easy_cleanup(pWrkrData->curlHandle);
		pWrkrData->curlHandle = NULL;
	}
	free(pWrkrData->restURL);
ENDfreeWrkrInstance

BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	dbgprintf("omelasticsearch\n");
	dbgprintf("\ttemplate='%s'\n", pData->tplName);
	dbgprintf("\tserver='%s'\n", pData->server);
	dbgprintf("\tserverport=%d\n", pData->port);
	dbgprintf("\tuid='%s'\n", pData->uid == NULL ? (uchar*)"(not configured)" : pData->uid);
	dbgprintf("\tpwd=(%sconfigured)\n", pData->pwd == NULL ? "not " : "");
	dbgprintf("\tsearch index='%s'\n", pData->searchIndex);
	dbgprintf("\tsearch index='%s'\n", pData->searchType);
	dbgprintf("\tparent='%s'\n", pData->parent);
	dbgprintf("\ttimeout='%s'\n", pData->timeout);
	dbgprintf("\tdynamic search index=%d\n", pData->dynSrchIdx);
	dbgprintf("\tdynamic search type=%d\n", pData->dynSrchType);
	dbgprintf("\tdynamic parent=%d\n", pData->dynParent);
	dbgprintf("\tasync replication=%d\n", pData->asyncRepl);
        dbgprintf("\tuse https=%d\n", pData->useHttps);
	dbgprintf("\tbulkmode=%d\n", pData->bulkmode);
	dbgprintf("\terrorfile='%s'\n", pData->errorFile == NULL ?
		(uchar*)"(not configured)" : pData->errorFile);
	dbgprintf("\terroronly=%d\n", pData->errorOnly);
	dbgprintf("\tinterleaved=%d\n", pData->interleaved);
	dbgprintf("\tdynbulkid=%d\n", pData->dynBulkId);
	dbgprintf("\tbulkid='%s'\n", pData->bulkId);
ENDdbgPrintInstInfo


/* Build basic URL part, which includes hostname and port as follows:
 * http://hostname:port/
 * Newly creates an estr for this purpose.
 */
static rsRetVal
setBaseURL(instanceData *pData, es_str_t **url)
{
	char portBuf[64];
	int r;
	DEFiRet;

	*url = es_newStr(128);
	snprintf(portBuf, sizeof(portBuf), "%d", pData->port);
        if (pData->useHttps) {
  		r = es_addBuf(url, "https://", sizeof("https://")-1);
	}
	else {
		r = es_addBuf(url, "http://", sizeof("http://")-1);
	}
	if(r == 0) r = es_addBuf(url, (char*)pData->server, strlen((char*)pData->server));
	if(r == 0) r = es_addChar(url, ':');
	if(r == 0) r = es_addBuf(url, portBuf, strlen(portBuf));
	if(r == 0) es_addChar(url, '/');
	RETiRet;
}


static inline rsRetVal
checkConn(wrkrInstanceData_t *pWrkrData)
{
	es_str_t *url;
	CURL *curl = NULL;
	CURLcode res;
	char *cstr;
	DEFiRet;

	setBaseURL(pWrkrData->pData, &url);
	curl = curl_easy_init();
	if(curl == NULL) {
		DBGPRINTF("omelasticsearch: checkConn() curl_easy_init() failed\n");
		ABORT_FINALIZE(RS_RET_SUSPENDED);
	}
	/* Bodypart of request not needed, so set curl opt to nobody and httpget, otherwise lib-curl could sigsegv */
	curl_easy_setopt(curl, CURLOPT_HTTPGET, TRUE);
	curl_easy_setopt(curl, CURLOPT_NOBODY, TRUE);
	/* Only enable for debugging 
	curl_easy_setopt(curl, CURLOPT_VERBOSE, TRUE); */

	cstr = es_str2cstr(url, NULL);
	curl_easy_setopt(curl, CURLOPT_URL, cstr);
	free(cstr);

	pWrkrData->reply = NULL;
	pWrkrData->replyLen = 0;
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, pWrkrData);
	res = curl_easy_perform(curl);
	if(res != CURLE_OK) {
		DBGPRINTF("omelasticsearch: checkConn() curl_easy_perform() "
			  "failed: %s\n", curl_easy_strerror(res));
		ABORT_FINALIZE(RS_RET_SUSPENDED);
	}
	free(pWrkrData->reply);
	DBGPRINTF("omelasticsearch: checkConn() completed with success\n");

finalize_it:
	if(curl != NULL)
		curl_easy_cleanup(curl);
	RETiRet;
}


BEGINtryResume
CODESTARTtryResume
	DBGPRINTF("omelasticsearch: tryResume called\n");
	iRet = checkConn(pWrkrData);
ENDtryResume


/* get the current index and type for this message */
static inline void
getIndexTypeAndParent(instanceData *pData, uchar **tpls,
		      uchar **srchIndex, uchar **srchType, uchar **parent,
			  uchar **bulkId)
{
	if(pData->dynSrchIdx) {
		*srchIndex = tpls[1];
		if(pData->dynSrchType) {
			*srchType = tpls[2];
			if(pData->dynParent) {
				*parent = tpls[3];
				if(pData->dynBulkId) {
					*bulkId = tpls[4];
				}
			} else {
				*parent = pData->parent;
				if(pData->dynBulkId) {
					*bulkId = tpls[3];
				}
			}
		} else  {
			*srchType = pData->searchType;
			if(pData->dynParent) {
				*parent = tpls[2];
				if(pData->dynBulkId) {
					*bulkId = tpls[3];
				}
			} else {
				*parent = pData->parent;
				if(pData->dynBulkId) {
					*bulkId = tpls[2];
				}
			}
		}
	} else {
		*srchIndex = pData->searchIndex;
		if(pData->dynSrchType) {
			*srchType = tpls[1];
			if(pData->dynParent) {
				*parent = tpls[2];
                 if(pData->dynBulkId) {
                    *bulkId = tpls[3];
                }
			} else {
				*parent = pData->parent;
                if(pData->dynBulkId) {
                    *bulkId = tpls[2];
                }
			}
		} else  {
			*srchType = pData->searchType;
			if(pData->dynParent) {
				*parent = tpls[1];
                if(pData->dynBulkId) {
                    *bulkId = tpls[2];
                }
			} else {
				*parent = pData->parent;
                if(pData->dynBulkId) {
                    *bulkId = tpls[1];
                }
			}
		}
	}
}


static rsRetVal
setCurlURL(wrkrInstanceData_t *pWrkrData, instanceData *pData, uchar **tpls)
{
	char authBuf[1024];
	uchar *searchIndex;
	uchar *searchType;
	uchar *parent;
	uchar *bulkId;
	es_str_t *url;
	int rLocal;
	int r;
	DEFiRet;

	setBaseURL(pData, &url);

	if(pData->bulkmode) {
		r = es_addBuf(&url, "_bulk", sizeof("_bulk")-1);
		parent = NULL;
	} else {
		getIndexTypeAndParent(pData, tpls, &searchIndex, &searchType, &parent, &bulkId);
		r = es_addBuf(&url, (char*)searchIndex, ustrlen(searchIndex));
		if(r == 0) r = es_addChar(&url, '/');
		if(r == 0) r = es_addBuf(&url, (char*)searchType, ustrlen(searchType));
	}
	if(r == 0) r = es_addChar(&url, '?');
	if(pData->asyncRepl) {
		if(r == 0) r = es_addBuf(&url, "replication=async&",
					sizeof("replication=async&")-1);
	}
	if(pData->timeout != NULL) {
		if(r == 0) r = es_addBuf(&url, "timeout=", sizeof("timeout=")-1);
		if(r == 0) r = es_addBuf(&url, (char*)pData->timeout, ustrlen(pData->timeout));
		if(r == 0) r = es_addChar(&url, '&');
	}
	if(parent != NULL) {
		if(r == 0) r = es_addBuf(&url, "parent=", sizeof("parent=")-1);
		if(r == 0) es_addBuf(&url, (char*)parent, ustrlen(parent));
	}

	free(pWrkrData->restURL);
	pWrkrData->restURL = (uchar*)es_str2cstr(url, NULL);
	curl_easy_setopt(pWrkrData->curlHandle, CURLOPT_URL, pWrkrData->restURL);
	es_deleteStr(url);
	DBGPRINTF("omelasticsearch: using REST URL: '%s'\n", pWrkrData->restURL);

	if(pData->uid != NULL) {
		rLocal = snprintf(authBuf, sizeof(authBuf), "%s:%s", pData->uid,
			         (pData->pwd == NULL) ? "" : (char*)pData->pwd);
		if(rLocal < 1) {
			errmsg.LogError(0, RS_RET_ERR, "omelasticsearch: snprintf failed "
				"when trying to build auth string (return %d)\n",
				rLocal);
			ABORT_FINALIZE(RS_RET_ERR);
		}
		curl_easy_setopt(pWrkrData->curlHandle, CURLOPT_USERPWD, authBuf);
		curl_easy_setopt(pWrkrData->curlHandle, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
	}
finalize_it:
	RETiRet;
}


/* this method does not directly submit but builds a batch instead. It
 * may submit, if we have dynamic index/type and the current type or
 * index changes.
 */
static rsRetVal
buildBatch(wrkrInstanceData_t *pWrkrData, uchar *message, uchar **tpls)
{
	int length = strlen((char *)message);
	int r;
	uchar *searchIndex;
	uchar *searchType;
	uchar *parent;
	uchar *bulkId = NULL;
	DEFiRet;
#	define META_STRT "{\"index\":{\"_index\": \""
#	define META_TYPE "\",\"_type\":\""
#	define META_PARENT "\",\"_parent\":\""
#   define META_ID "\", \"_id\":\""
#	define META_END  "\"}}\n"

	getIndexTypeAndParent(pWrkrData->pData, tpls, &searchIndex, &searchType, &parent, &bulkId);
	r = es_addBuf(&pWrkrData->batch.data, META_STRT, sizeof(META_STRT)-1);
	if(r == 0) r = es_addBuf(&pWrkrData->batch.data, (char*)searchIndex,
				 ustrlen(searchIndex));
	if(r == 0) r = es_addBuf(&pWrkrData->batch.data, META_TYPE, sizeof(META_TYPE)-1);
	if(r == 0) r = es_addBuf(&pWrkrData->batch.data, (char*)searchType,
				 ustrlen(searchType));
	if(parent != NULL) {
		if(r == 0) r = es_addBuf(&pWrkrData->batch.data, META_PARENT, sizeof(META_PARENT)-1);
		if(r == 0) r = es_addBuf(&pWrkrData->batch.data, (char*)parent, ustrlen(parent));
	}
	if(bulkId != NULL) {
		if(r == 0) r = es_addBuf(&pWrkrData->batch.data, META_ID, sizeof(META_ID)-1);
		if(r == 0) r = es_addBuf(&pWrkrData->batch.data, (char*)bulkId, ustrlen(bulkId));
	}
	if(r == 0) r = es_addBuf(&pWrkrData->batch.data, META_END, sizeof(META_END)-1);
	if(r == 0) r = es_addBuf(&pWrkrData->batch.data, (char*)message, length);
	if(r == 0) r = es_addBuf(&pWrkrData->batch.data, "\n", sizeof("\n")-1);
	if(r != 0) {
		DBGPRINTF("omelasticsearch: growing batch failed with code %d\n", r);
		ABORT_FINALIZE(RS_RET_ERR);
	}
	++pWrkrData->batch.nmemb;
	iRet = RS_RET_DEFER_COMMIT;

finalize_it:
	RETiRet;
}

/*
 * Dumps entire bulk request and response in error log
 */
static inline rsRetVal
getDataErrorDefault(wrkrInstanceData_t *pWrkrData,cJSON **pReplyRoot,uchar *reqmsg,char **rendered)
{
	DEFiRet;
	cJSON *req=0;
	cJSON *errRoot=0;
	cJSON *replyRoot = *pReplyRoot;

	if((req=cJSON_CreateObject()) == NULL) ABORT_FINALIZE(RS_RET_ERR);
	cJSON_AddItemToObject(req, "url", cJSON_CreateString((char*)pWrkrData->restURL));
	cJSON_AddItemToObject(req, "postdata", cJSON_CreateString((char*)reqmsg));

	if((errRoot=cJSON_CreateObject()) == NULL) ABORT_FINALIZE(RS_RET_ERR);
	cJSON_AddItemToObject(errRoot, "request", req);
	cJSON_AddItemToObject(errRoot, "reply", replyRoot);
	*rendered = cJSON_Print(errRoot);

	req=0;
	cJSON_Delete(errRoot);

	*pReplyRoot = NULL; /* tell caller not to delete once again! */

	finalize_it:
		cJSON_Delete(req);
		RETiRet;
}

/*
 * Sets bulkRequestNextSectionStart pointer to next sections start in the buffer pointed by bulkRequest.
 * Sections are marked by { and }
 */
static inline rsRetVal
getSection(const char* bulkRequest,char **bulkRequestNextSectionStart )
{
		DEFiRet;
		char* index =0;
		if( (index = strchr(bulkRequest,'\n')) != 0)//intermediate section
		{
			*bulkRequestNextSectionStart = ++index;
		}
		else
		{
			*bulkRequestNextSectionStart=0;
			ABORT_FINALIZE(RS_RET_ERR);
		}

	     finalize_it:
	     	  RETiRet;
}

/*
 * Sets the new string in singleRequest for one request in bulkRequest
 * and sets lastLocation pointer to the location till which bulkrequest has been parsed.(used as input to make function thread safe.)
 */
static inline rsRetVal
getSingleRequest(const char* bulkRequest, char** singleRequest ,char **lastLocation)
{
	DEFiRet;
	char *req = bulkRequest;
	char *start = bulkRequest;
	if (getSection(req,&req)!=RS_RET_OK)
		ABORT_FINALIZE(RS_RET_ERR);

	if (getSection(req,&req)!=RS_RET_OK)
			ABORT_FINALIZE(RS_RET_ERR);

    *singleRequest = (char*) calloc (req - start+ 1 + 1,sizeof(char));// (req - start+ 1 == length of data + 1 for terminal char)
    if (*singleRequest==NULL) ABORT_FINALIZE(RS_RET_ERR);
    memcpy(*singleRequest,start,req - start);
    *lastLocation=req;

	finalize_it:
			RETiRet;
}

/*
 * check the status of response from ES
 */
int checkReplyStatus(cJSON* ok) {
	return (ok == NULL || ok->type != cJSON_Number || ok->valueint < 0 || ok->valueint > 299);
}

//Context object for error file content creation or status check
typedef struct exeContext{
	int statusCheckOnly;
	cJSON *errRoot;
	rsRetVal (*prepareErrorFileContent)(struct exeContext *ctx,int itemStatus,char *request,char *response);


} context;

//get content to be written in error file using context passed
static inline rsRetVal
parseRequestAndResponseForContext(wrkrInstanceData_t *pWrkrData,cJSON **pReplyRoot,uchar *reqmsg,context *ctx)
{
	DEFiRet;
	cJSON *replyRoot = *pReplyRoot;
	int i;
	int numitems;
	cJSON *items=0;


	//iterate over items
	items = cJSON_GetObjectItem(replyRoot, "items");
	if(items == NULL || items->type != cJSON_Array) {
		DBGPRINTF("omelasticsearch: error in elasticsearch reply: "
			  "bulkmode insert does not return array, reply is: %s\n",
			  pWrkrData->reply);
		ABORT_FINALIZE(RS_RET_DATAFAIL);
	}

	numitems = cJSON_GetArraySize(items);

	DBGPRINTF("omelasticsearch: Entire request %s\n",reqmsg);
	char *lastReqRead= (char*)reqmsg;

	DBGPRINTF("omelasticsearch: %d items in reply\n", numitems);
	for(i = 0 ; i < numitems ; ++i) {

		cJSON *item=0;
		cJSON *create=0;
		cJSON *ok=0;
		int itemStatus=0;
		item = cJSON_GetArrayItem(items, i);
		if(item == NULL)  {
			DBGPRINTF("omelasticsearch: error in elasticsearch reply: "
				  "cannot obtain reply array item %d\n", i);
			ABORT_FINALIZE(RS_RET_DATAFAIL);
		}
		create = cJSON_GetObjectItem(item, "create");
		if(create == NULL || create->type != cJSON_Object) {
			DBGPRINTF("omelasticsearch: error in elasticsearch reply: "
				  "cannot obtain 'create' item for #%d\n", i);
			ABORT_FINALIZE(RS_RET_DATAFAIL);
		}

		ok = cJSON_GetObjectItem(create, "status");
		itemStatus = checkReplyStatus(ok);

		char *request =0;
		char *response =0;
		if(ctx->statusCheckOnly)
		{
			if(itemStatus) {
				DBGPRINTF("omelasticsearch: error in elasticsearch reply: item %d, status is %d\n", i, ok->valueint);
				DBGPRINTF("omelasticsearch: status check found error.\n");
				ABORT_FINALIZE(RS_RET_DATAFAIL);
			}

		}
		else
		{
			if(getSingleRequest(lastReqRead,&request,&lastReqRead) != RS_RET_OK)
			{
				DBGPRINTF("omelasticsearch: Couldn't get post request\n");
				ABORT_FINALIZE(RS_RET_ERR);
			}

			response = cJSON_PrintUnformatted(create);

			if(*response==NULL)
			{
				free(request);//as its has been assigned.
				DBGPRINTF("omelasticsearch: Error getting cJSON_PrintUnformatted. Cannot continue\n");
				ABORT_FINALIZE(RS_RET_ERR);
			}

			//call the context
			rsRetVal ret = ctx->prepareErrorFileContent(ctx, itemStatus, request,response);

			//free memory in any case
			free(request);
			free(response);

			if(ret != RS_RET_OK)
			{
				DBGPRINTF("omelasticsearch: Error in preparing errorfileContent. Cannot continue\n");
				ABORT_FINALIZE(RS_RET_ERR);
			}

		}

	}

	finalize_it:
		RETiRet;
}

/*
 * Dumps only failed requests of bulk insert
 */
static inline rsRetVal
getDataErrorOnly(context *ctx,int itemStatus,char *request,char *response)
{
	DEFiRet;
	if(itemStatus)
	{
		cJSON *onlyErrorResponses =0;
		cJSON *onlyErrorRequests=0;

		if((onlyErrorResponses=cJSON_GetObjectItem(ctx->errRoot, "reply")) == NULL)
		{
			DBGPRINTF("omelasticsearch: Failed to get reply json array. Invalid context. Cannot continue\n");
			ABORT_FINALIZE(RS_RET_ERR);
		}
		cJSON_AddItemToArray(onlyErrorResponses, cJSON_CreateString(response));

		if((onlyErrorRequests=cJSON_GetObjectItem(ctx->errRoot, "request")) == NULL)
		{
			DBGPRINTF("omelasticsearch: Failed to get request json array. Invalid context. Cannot continue\n");
			ABORT_FINALIZE(RS_RET_ERR);
		}

		cJSON_AddItemToArray(onlyErrorRequests, cJSON_CreateString(request));

	}

	finalize_it:
		RETiRet;
}

/*
 * Dumps all requests of bulk insert interleaved with request and response
 */

static inline rsRetVal
getDataInterleaved(context *ctx,int itemStatus,char *request,char *response)
{
	DEFiRet;
	cJSON *interleaved =0;
	if((interleaved=cJSON_GetObjectItem(ctx->errRoot, "response")) == NULL)
	{
		DBGPRINTF("omelasticsearch: Failed to get response json array. Invalid context. Cannot continue\n");
		ABORT_FINALIZE(RS_RET_ERR);
	}

	cJSON *interleavedNode=0;
	//create interleaved node that has req and response json data
	if((interleavedNode=cJSON_CreateObject()) == NULL)
	{
		DBGPRINTF("omelasticsearch: Failed to create interleaved node. Cann't continue\n");
		ABORT_FINALIZE(RS_RET_ERR);
	}
	cJSON_AddItemToObject(interleavedNode,"request", cJSON_CreateString(request));
	cJSON_AddItemToObject(interleavedNode,"reply", cJSON_CreateString(response));

	cJSON_AddItemToArray(interleaved, interleavedNode);



	finalize_it:
		RETiRet;
}


/*
 * Dumps only failed requests of bulk insert interleaved with request and response
 */

static inline rsRetVal
getDataErrorOnlyInterleaved(context *ctx,int itemStatus,char *request,char *response)
{
	DEFiRet;
	if (itemStatus) {
		if(getDataInterleaved(ctx, itemStatus,request,response)!= RS_RET_OK) {
			ABORT_FINALIZE(RS_RET_ERR);
		}
	}

	finalize_it:
		RETiRet;
}

//get erroronly context
static inline rsRetVal
initializeErrorOnlyConext(wrkrInstanceData_t *pWrkrData,context *ctx){
	DEFiRet;
	ctx->statusCheckOnly=0;
	cJSON *errRoot=0;
	cJSON *onlyErrorResponses =0;
	cJSON *onlyErrorRequests=0;
	if((errRoot=cJSON_CreateObject()) == NULL) ABORT_FINALIZE(RS_RET_ERR);

	if((onlyErrorResponses=cJSON_CreateArray()) == NULL) {
		cJSON_Delete(errRoot);
		ABORT_FINALIZE(RS_RET_ERR);
	}
	if((onlyErrorRequests=cJSON_CreateArray()) == NULL) {
		cJSON_Delete(errRoot);
		cJSON_Delete(onlyErrorResponses);
		ABORT_FINALIZE(RS_RET_ERR);
	}

	cJSON_AddItemToObject(errRoot, "url", cJSON_CreateString((char*)pWrkrData->restURL));
	cJSON_AddItemToObject(errRoot,"request",onlyErrorRequests);
	cJSON_AddItemToObject(errRoot, "reply", onlyErrorResponses);
	ctx->errRoot = errRoot;
	ctx->prepareErrorFileContent= &getDataErrorOnly;
	finalize_it:
		RETiRet;
}

//get interleaved context
static inline rsRetVal
initializeInterleavedConext(wrkrInstanceData_t *pWrkrData,context *ctx){
	DEFiRet;
	ctx->statusCheckOnly=0;
	cJSON *errRoot=0;
	cJSON *interleaved =0;
	if((errRoot=cJSON_CreateObject()) == NULL) ABORT_FINALIZE(RS_RET_ERR);
	if((interleaved=cJSON_CreateArray()) == NULL) {
		cJSON_Delete(errRoot);
		ABORT_FINALIZE(RS_RET_ERR);
	}


	cJSON_AddItemToObject(errRoot, "url", cJSON_CreateString((char*)pWrkrData->restURL));
	cJSON_AddItemToObject(errRoot,"response",interleaved);
	ctx->errRoot = errRoot;
	ctx->prepareErrorFileContent= &getDataInterleaved;
	finalize_it:
		RETiRet;
}

//get interleaved context
static inline rsRetVal
initializeErrorInterleavedConext(wrkrInstanceData_t *pWrkrData,context *ctx){
	DEFiRet;
	ctx->statusCheckOnly=0;
	cJSON *errRoot=0;
	cJSON *interleaved =0;
	if((errRoot=cJSON_CreateObject()) == NULL) ABORT_FINALIZE(RS_RET_ERR);
	if((interleaved=cJSON_CreateArray()) == NULL) {
		cJSON_Delete(errRoot);
		ABORT_FINALIZE(RS_RET_ERR);
	}


	cJSON_AddItemToObject(errRoot, "url", cJSON_CreateString((char*)pWrkrData->restURL));
	cJSON_AddItemToObject(errRoot,"response",interleaved);
	ctx->errRoot = errRoot;
	ctx->prepareErrorFileContent= &getDataErrorOnlyInterleaved;
	finalize_it:
		RETiRet;
}


/* write data error request/replies to separate error file
 * Note: we open the file but never close it before exit. If it
 * needs to be closed, HUP must be sent.
 */
static inline rsRetVal
writeDataError(wrkrInstanceData_t *pWrkrData, instanceData *pData, cJSON **pReplyRoot, uchar *reqmsg)
{
	char *rendered = NULL;
	size_t toWrite;
	ssize_t wrRet;
	sbool bMutLocked = 0;
	char errStr[1024];
	DEFiRet;

	if(pData->errorFile == NULL) {
		DBGPRINTF("omelasticsearch: no local error logger defined - "
		          "ignoring ES error information\n");
		FINALIZE;
	}

	pthread_mutex_lock(&pData->mutErrFile);
	bMutLocked = 1;

	DBGPRINTF("omelasticsearch: error file mode: erroronly='%d' errorInterleaved='%d'\n", pData->errorOnly , pData->interleaved);

	context ctx;
	ctx.errRoot=0;
	if(pData->interleaved ==0 && pData->errorOnly ==0)//default write
	{
		if(getDataErrorDefault(pWrkrData,pReplyRoot,reqmsg,&rendered) != RS_RET_OK) {
			ABORT_FINALIZE(RS_RET_ERR);
		}
	}
	else
	{
		//get correct context.
		if(pData->interleaved && pData->errorOnly)
		{
			if(initializeErrorInterleavedConext(pWrkrData,&ctx) != RS_RET_OK) {
				DBGPRINTF("omelasticsearch: error initializing error interleaved context.\n");
				ABORT_FINALIZE(RS_RET_ERR);
			}

		}
		else if(pData->errorOnly)
		{

			if(initializeErrorOnlyConext(pWrkrData,&ctx) != RS_RET_OK) {

				DBGPRINTF("omelasticsearch: error initializing error only context.\n");
				ABORT_FINALIZE(RS_RET_ERR);
			}
		}
		else if(pData->interleaved)
		{
			if(initializeInterleavedConext(pWrkrData,&ctx) != RS_RET_OK) {
				DBGPRINTF("omelasticsearch: error initializing error interleaved context.\n");
				ABORT_FINALIZE(RS_RET_ERR);
			}
		}
		else
		{
			DBGPRINTF("omelasticsearch: None of the modes match file write. No data to write.\n");
			ABORT_FINALIZE(RS_RET_ERR);
		}

		//execute context
		if(parseRequestAndResponseForContext(pWrkrData,pReplyRoot,reqmsg,&ctx)!= RS_RET_OK) {
			DBGPRINTF("omelasticsearch: error creating file content.\n");
			ABORT_FINALIZE(RS_RET_ERR);
		}
		rendered = cJSON_Print(ctx.errRoot);
	}


	if(pData->fdErrFile == -1) {
		pData->fdErrFile = open((char*)pData->errorFile,
					O_WRONLY|O_CREAT|O_APPEND|O_LARGEFILE|O_CLOEXEC,
					S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
		if(pData->fdErrFile == -1) {
			rs_strerror_r(errno, errStr, sizeof(errStr));
			DBGPRINTF("omelasticsearch: error opening error file: %s\n", errStr);
			ABORT_FINALIZE(RS_RET_ERR);
		}
	}

	/* we do not do real error-handling on the err file, as this finally complicates
	 * things way to much.
	 */
	DBGPRINTF("omelasticsearch: error record: '%s'\n", rendered);
	toWrite = strlen(rendered);
	wrRet = write(pData->fdErrFile, rendered, toWrite);
	if(wrRet != (ssize_t) toWrite) {
		DBGPRINTF("omelasticsearch: error %d writing error file, write returns %lld\n",
			  errno, (long long) wrRet);
	}


finalize_it:
	if(bMutLocked)
		pthread_mutex_unlock(&pData->mutErrFile);
	cJSON_Delete(ctx.errRoot);
	free(rendered);
	RETiRet;
}


static inline rsRetVal
checkResultBulkmode(wrkrInstanceData_t *pWrkrData, cJSON *root)
{
	DEFiRet;
	context ctx;
	ctx.statusCheckOnly=1;
	ctx.errRoot = 0;
	if(parseRequestAndResponseForContext(pWrkrData,&root,0,&ctx)!= RS_RET_OK)
	{
		DBGPRINTF("omelasticsearch: error found in elasticsearch reply\n");
		ABORT_FINALIZE(RS_RET_DATAFAIL);
	}

	finalize_it:
		RETiRet;
}


static inline rsRetVal
checkResult(wrkrInstanceData_t *pWrkrData, uchar *reqmsg)
{
	cJSON *root;
	cJSON *status;
	DEFiRet;

	root = cJSON_Parse(pWrkrData->reply);
	if(root == NULL) {
		DBGPRINTF("omelasticsearch: could not parse JSON result \n");
		ABORT_FINALIZE(RS_RET_ERR);
	}

	if(pWrkrData->pData->bulkmode) {
		iRet = checkResultBulkmode(pWrkrData, root);
	} else {
		status = cJSON_GetObjectItem(root, "status");
		/* as far as we know, no "status" means all went well */
		if(status != NULL &&
		   (status->type == cJSON_Number || status->valueint >= 0 || status->valueint <= 299)) {
			iRet = RS_RET_DATAFAIL;
		}
	}

	/* Note: we ignore errors writing the error file, as we cannot handle
	 * these in any case.
	 */
	if(iRet == RS_RET_DATAFAIL) {
		STATSCOUNTER_INC(indexESFail, mutIndexESFail);
		writeDataError(pWrkrData, pWrkrData->pData, &root, reqmsg);
		iRet = RS_RET_OK; /* we have handled the problem! */
	}

finalize_it:
	if(root != NULL)
		cJSON_Delete(root);
	if(iRet != RS_RET_OK) {
		STATSCOUNTER_INC(indexESFail, mutIndexESFail);
	}
	RETiRet;
}


static rsRetVal
curlPost(wrkrInstanceData_t *pWrkrData, uchar *message, int msglen, uchar **tpls, int nmsgs)
{
	CURLcode code;
	CURL *curl = pWrkrData->curlHandle;
	DEFiRet;

	pWrkrData->reply = NULL;
	pWrkrData->replyLen = 0;

	if(pWrkrData->pData->dynSrchIdx || pWrkrData->pData->dynSrchType || pWrkrData->pData->dynParent)
		CHKiRet(setCurlURL(pWrkrData, pWrkrData->pData, tpls));

	curl_easy_setopt(curl, CURLOPT_WRITEDATA, pWrkrData);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, (char *)message);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, msglen);
	code = curl_easy_perform(curl);
	switch (code) {
		case CURLE_COULDNT_RESOLVE_HOST:
		case CURLE_COULDNT_RESOLVE_PROXY:
		case CURLE_COULDNT_CONNECT:
		case CURLE_WRITE_ERROR:
			STATSCOUNTER_INC(indexHTTPReqFail, mutIndexHTTPReqFail);
			indexHTTPFail += nmsgs;
			DBGPRINTF("omelasticsearch: we are suspending ourselfs due "
				  "to failure %lld of curl_easy_perform()\n",
				  (long long) code);
			ABORT_FINALIZE(RS_RET_SUSPENDED);
		default:
			break;
	}

	DBGPRINTF("omelasticsearch: pWrkrData replyLen = '%d'\n", pWrkrData->replyLen);
	if(pWrkrData->replyLen > 0) {
		pWrkrData->reply[pWrkrData->replyLen] = '\0'; /* Append 0 Byte if replyLen is above 0 - byte has been reserved in malloc */
	}
	DBGPRINTF("omelasticsearch: pWrkrData reply: '%s'\n", pWrkrData->reply);

	CHKiRet(checkResult(pWrkrData, message));
finalize_it:
	free(pWrkrData->reply);
	RETiRet;
}

BEGINbeginTransaction
CODESTARTbeginTransaction
dbgprintf("omelasticsearch: beginTransaction, pWrkrData %p, pData %p\n", pWrkrData, pWrkrData->pData);
	if(!pWrkrData->pData->bulkmode) {
		FINALIZE;
	}

	es_emptyStr(pWrkrData->batch.data);
	pWrkrData->batch.nmemb = 0;
finalize_it:
ENDbeginTransaction


BEGINdoAction
CODESTARTdoAction
	STATSCOUNTER_INC(indexSubmit, mutIndexSubmit);
	if(pWrkrData->pData->bulkmode) {
		CHKiRet(buildBatch(pWrkrData, ppString[0], ppString));
	} else {
		CHKiRet(curlPost(pWrkrData, ppString[0], strlen((char*)ppString[0]),
		                 ppString, 1));
	}
finalize_it:
dbgprintf("omelasticsearch: result doAction: %d (bulkmode %d)\n", iRet, pWrkrData->pData->bulkmode);
ENDdoAction


BEGINendTransaction
	char *cstr = NULL;
CODESTARTendTransaction
dbgprintf("omelasticsearch: endTransaction init\n");
	/* End Transaction only if batch data is not empty */
	if (pWrkrData->batch.data != NULL ) {
		cstr = es_str2cstr(pWrkrData->batch.data, NULL);
		dbgprintf("omelasticsearch: endTransaction, batch: '%s'\n", cstr);
		CHKiRet(curlPost(pWrkrData, (uchar*) cstr, strlen(cstr), NULL, pWrkrData->batch.nmemb));
	}
	else
		dbgprintf("omelasticsearch: endTransaction, pWrkrData->batch.data is NULL, nothing to send. \n");
finalize_it:
	free(cstr);
dbgprintf("omelasticsearch: endTransaction done with %d\n", iRet);
ENDendTransaction

/* elasticsearch POST result string ... useful for debugging */
size_t
curlResult(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	char *p = (char *)ptr;
	wrkrInstanceData_t *pWrkrData = (wrkrInstanceData_t*) userdata;
	char *buf;
	size_t newlen;

	newlen = pWrkrData->replyLen + size*nmemb;
	if((buf = realloc(pWrkrData->reply, newlen + 1)) == NULL) {
		DBGPRINTF("omelasticsearch: realloc failed in curlResult\n");
		return 0; /* abort due to failure */
	}
	memcpy(buf+pWrkrData->replyLen, p, size*nmemb);
	pWrkrData->replyLen = newlen;
	pWrkrData->reply = buf;
	return size*nmemb;
}


static rsRetVal
curlSetup(wrkrInstanceData_t *pWrkrData, instanceData *pData)
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

	pWrkrData->curlHandle = handle;
	pWrkrData->postHeader = header;

	if(    pData->bulkmode
	   || (pData->dynSrchIdx == 0 && pData->dynSrchType == 0 && pData->dynParent == 0)) {
		/* in this case, we know no tpls are involved in the request-->NULL OK! */
		setCurlURL(pWrkrData, pData, NULL);
	}

	if(Debug) {
		if(pData->dynSrchIdx == 0 && pData->dynSrchType == 0 && pData->dynParent == 0)
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
	pData->uid = NULL;
	pData->pwd = NULL;
	pData->searchIndex = NULL;
	pData->searchType = NULL;
	pData->parent = NULL;
	pData->timeout = NULL;
	pData->dynSrchIdx = 0;
	pData->dynSrchType = 0;
	pData->dynParent = 0;
	pData->asyncRepl = 0;
        pData->useHttps = 0;
	pData->bulkmode = 0;
	pData->tplName = NULL;
	pData->errorFile = NULL;
	pData->errorOnly=0;
	pData->interleaved=0;
	pData->dynBulkId= 0;
	pData->bulkId = NULL;
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
		} else if(!strcmp(actpblk.descr[i].name, "errorfile")) {
			pData->errorFile = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		}else if(!strcmp(actpblk.descr[i].name, "erroronly")) {
			pData->errorOnly = pvals[i].val.d.n;
		}else if(!strcmp(actpblk.descr[i].name, "interleaved")) {
			pData->interleaved = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "serverport")) {
			pData->port = (int) pvals[i].val.d.n, NULL;
		} else if(!strcmp(actpblk.descr[i].name, "uid")) {
			pData->uid = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "pwd")) {
			pData->pwd = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "searchindex")) {
			pData->searchIndex = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "searchtype")) {
			pData->searchType = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "parent")) {
			pData->parent = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "dynsearchindex")) {
			pData->dynSrchIdx = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "dynsearchtype")) {
			pData->dynSrchType = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "dynparent")) {
			pData->dynParent = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "bulkmode")) {
			pData->bulkmode = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "timeout")) {
			pData->timeout = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "asyncrepl")) {
			pData->asyncRepl = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "usehttps")) {
			pData->useHttps = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "template")) {
			pData->tplName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "dynbulkid")) {
			pData->dynBulkId = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "bulkid")) {
			pData->bulkId = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else {
			dbgprintf("omelasticsearch: program error, non-handled "
			  "param '%s'\n", actpblk.descr[i].name);
		}
	}

	if(pData->pwd != NULL && pData->uid == NULL) {
		errmsg.LogError(0, RS_RET_UID_MISSING,
			"omelasticsearch: password is provided, but no uid "
			"- action definition invalid");
		ABORT_FINALIZE(RS_RET_UID_MISSING);
	}
	if(pData->dynSrchIdx && pData->searchIndex == NULL) {
		errmsg.LogError(0, RS_RET_CONFIG_ERROR,
			"omelasticsearch: requested dynamic search index, but no "
			"name for index template given - action definition invalid");
		ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
	}
	if(pData->dynSrchType && pData->searchType == NULL) {
		errmsg.LogError(0, RS_RET_CONFIG_ERROR,
			"omelasticsearch: requested dynamic search type, but no "
			"name for type template given - action definition invalid");
		ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
	}
	if(pData->dynParent && pData->parent == NULL) {
		errmsg.LogError(0, RS_RET_CONFIG_ERROR,
			"omelasticsearch: requested dynamic parent, but no "
			"name for parent template given - action definition invalid");
		ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
	}
	if(pData->dynBulkId && pData->bulkId == NULL) {
		errmsg.LogError(0, RS_RET_CONFIG_ERROR,
			"omelasticsearch: requested dynamic bulkid, but no "
			"name for bulkid template given - action definition invalid");
		ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
	}

	iNumTpls = 1;
	if(pData->dynSrchIdx) ++iNumTpls;
	if(pData->dynSrchType) ++iNumTpls;
	if(pData->dynParent) ++iNumTpls;
	if(pData->dynBulkId) ++iNumTpls;
	DBGPRINTF("omelasticsearch: requesting %d templates\n", iNumTpls);
	CODE_STD_STRING_REQUESTnewActInst(iNumTpls)

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
			if(pData->dynParent) {
				CHKiRet(OMSRsetEntry(*ppOMSR, 3, ustrdup(pData->parent),
					OMSR_NO_RQD_TPL_OPTS));
				if(pData->dynBulkId) {
					CHKiRet(OMSRsetEntry(*ppOMSR, 4, ustrdup(pData->bulkId),
						OMSR_NO_RQD_TPL_OPTS));
				}
			} else {
				if(pData->dynBulkId) {
					CHKiRet(OMSRsetEntry(*ppOMSR, 3, ustrdup(pData->bulkId),
						OMSR_NO_RQD_TPL_OPTS));
				}
			}
		} else {
			if(pData->dynParent) {
				CHKiRet(OMSRsetEntry(*ppOMSR, 2, ustrdup(pData->parent),
					OMSR_NO_RQD_TPL_OPTS));
				if(pData->dynBulkId) {
					CHKiRet(OMSRsetEntry(*ppOMSR, 3, ustrdup(pData->bulkId),
						OMSR_NO_RQD_TPL_OPTS));
				}
			} else {
				if(pData->dynBulkId) {
					CHKiRet(OMSRsetEntry(*ppOMSR, 2, ustrdup(pData->bulkId),
						OMSR_NO_RQD_TPL_OPTS));
				}
			}
		}
	} else {
		if(pData->dynSrchType) {
			CHKiRet(OMSRsetEntry(*ppOMSR, 1, ustrdup(pData->searchType),
				OMSR_NO_RQD_TPL_OPTS));
			if(pData->dynParent) {
				CHKiRet(OMSRsetEntry(*ppOMSR, 2, ustrdup(pData->parent),
					OMSR_NO_RQD_TPL_OPTS));
                if(pData->dynBulkId) {
                    CHKiRet(OMSRsetEntry(*ppOMSR, 3, ustrdup(pData->bulkId),
                        OMSR_NO_RQD_TPL_OPTS));
                }
			} else {
				if(pData->dynBulkId) {
					CHKiRet(OMSRsetEntry(*ppOMSR, 2, ustrdup(pData->bulkId),
						OMSR_NO_RQD_TPL_OPTS));
				}
			}
		} else {
			if(pData->dynParent) {
				CHKiRet(OMSRsetEntry(*ppOMSR, 1, ustrdup(pData->parent),
					OMSR_NO_RQD_TPL_OPTS));
                if(pData->dynBulkId) {
                    CHKiRet(OMSRsetEntry(*ppOMSR, 2, ustrdup(pData->bulkId),
                        OMSR_NO_RQD_TPL_OPTS));
                }
			} else {
				if(pData->dynBulkId) {
					CHKiRet(OMSRsetEntry(*ppOMSR, 1, ustrdup(pData->bulkId),
						OMSR_NO_RQD_TPL_OPTS));
				}
            }
		}
	}

	if(pData->server == NULL)
		pData->server = (uchar*) strdup("localhost");
	if(pData->searchIndex == NULL)
		pData->searchIndex = (uchar*) strdup("system");
	if(pData->searchType == NULL)
		pData->searchType = (uchar*) strdup("events");
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

BEGINdoHUP
CODESTARTdoHUP
	if(pData->fdErrFile != -1) {
		close(pData->fdErrFile);
		pData->fdErrFile = -1;
	}
ENDdoHUP


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
CODEqueryEtryPt_STD_OMOD8_QUERIES
CODEqueryEtryPt_IsCompatibleWithFeature_IF_OMOD_QUERIES
CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES
CODEqueryEtryPt_doHUP
CODEqueryEtryPt_TXIF_OMOD_QUERIES /* we support the transactional interface! */
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
	CHKiRet(statsobj.SetName(indexStats, (uchar *)"omelasticsearch"));
	CHKiRet(statsobj.SetOrigin(indexStats, (uchar *)"omelasticsearch"));
	STATSCOUNTER_INIT(indexSubmit, mutIndexSubmit);
	CHKiRet(statsobj.AddCounter(indexStats, (uchar *)"submitted",
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &indexSubmit));
	STATSCOUNTER_INIT(indexHTTPFail, mutIndexHTTPFail);
	CHKiRet(statsobj.AddCounter(indexStats, (uchar *)"failed.http",
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &indexHTTPFail));
	STATSCOUNTER_INIT(indexHTTPReqFail, mutIndexHTTPReqFail);
	CHKiRet(statsobj.AddCounter(indexStats, (uchar *)"failed.httprequests",
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &indexHTTPReqFail));
	STATSCOUNTER_INIT(indexESFail, mutIndexESFail);
	CHKiRet(statsobj.AddCounter(indexStats, (uchar *)"failed.es",
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &indexESFail));
	CHKiRet(statsobj.ConstructFinalize(indexStats));
ENDmodInit

/* vi:set ai:
 */
