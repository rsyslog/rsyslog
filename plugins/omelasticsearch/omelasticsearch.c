/* omelasticsearch.c
 * This is the http://www.elasticsearch.org/ output module.
 *
 * NOTE: read comments in module-template.h for more specifics!
 *
 * Copyright 2011 Nathan Scott.
 * Copyright 2009-2016 Rainer Gerhards and Adiscon GmbH.
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
#if defined(__FreeBSD__)
#include <unistd.h>
#endif
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

#ifndef O_LARGEFILE
#  define O_LARGEFILE 0
#endif

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
STATSCOUNTER_DEF(checkConnFail, mutCheckConnFail)
STATSCOUNTER_DEF(indexESFail, mutIndexESFail)


#	define META_STRT "{\"index\":{\"_index\": \""
#	define META_TYPE "\",\"_type\":\""
#	define META_PARENT "\",\"_parent\":\""
#	define META_ID "\", \"_id\":\""
#	define META_END  "\"}}\n"

/* REST API for elasticsearch hits this URL:
 * http://<hostName>:<restPort>/<searchIndex>/<searchType>
 */
typedef struct curl_slist HEADER;
typedef struct _instanceData {
	int defaultPort;
	int fdErrFile;		/* error file fd or -1 if not open */
	pthread_mutex_t mutErrFile;
	uchar **serverBaseUrls;
	int numServers;
	long healthCheckTimeout;
	uchar *uid;
	uchar *pwd;
	uchar *authBuf;
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
	size_t maxbytes;
	sbool useHttps;
	sbool allowUnsignedCerts;
} instanceData;

typedef struct wrkrInstanceData {
	instanceData *pData;
	int serverIndex;
	int replyLen;
	char *reply;
	CURL	*curlCheckConnHandle;	/* libcurl session handle for checking the server connection */
	CURL	*curlPostHandle;	/* libcurl session handle for posting data to the server */
	HEADER	*curlHeader;	/* json POST request info */
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
	{ "server", eCmdHdlrArray, 0 },
	{ "serverport", eCmdHdlrInt, 0 },
	{ "healthchecktimeout", eCmdHdlrInt, 0 },
	{ "uid", eCmdHdlrGetWord, 0 },
	{ "pwd", eCmdHdlrGetWord, 0 },
	{ "searchindex", eCmdHdlrGetWord, 0 },
	{ "searchtype", eCmdHdlrGetWord, 0 },
	{ "parent", eCmdHdlrGetWord, 0 },
	{ "dynsearchindex", eCmdHdlrBinary, 0 },
	{ "dynsearchtype", eCmdHdlrBinary, 0 },
	{ "dynparent", eCmdHdlrBinary, 0 },
	{ "bulkmode", eCmdHdlrBinary, 0 },
	{ "maxbytes", eCmdHdlrSize, 0 },
	{ "asyncrepl", eCmdHdlrGoneAway, 0 },
        { "usehttps", eCmdHdlrBinary, 0 },
	{ "timeout", eCmdHdlrGetWord, 0 },
	{ "errorfile", eCmdHdlrGetWord, 0 },
	{ "erroronly", eCmdHdlrBinary, 0 },
	{ "interleaved", eCmdHdlrBinary, 0 },
	{ "template", eCmdHdlrGetWord, 0 },
	{ "dynbulkid", eCmdHdlrBinary, 0 },
	{ "bulkid", eCmdHdlrGetWord, 0 },
	{ "allowunsignedcerts", eCmdHdlrBinary, 0 }
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
	pWrkrData->curlHeader = NULL;
	pWrkrData->curlPostHandle = NULL;
	pWrkrData->curlCheckConnHandle = NULL;
	pWrkrData->serverIndex = 0;
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
ENDcreateWrkrInstance

BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURERepeatedMsgReduction)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature

BEGINfreeInstance
	int i;
CODESTARTfreeInstance
	if(pData->fdErrFile != -1)
		close(pData->fdErrFile);
	pthread_mutex_destroy(&pData->mutErrFile);
	for(i = 0 ; i < pData->numServers ; ++i)
		free(pData->serverBaseUrls[i]);
	free(pData->serverBaseUrls);
	free(pData->uid);
	free(pData->pwd);
	if (pData->authBuf != NULL)
		free(pData->authBuf);
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
	if(pWrkrData->curlHeader != NULL) {
		curl_slist_free_all(pWrkrData->curlHeader);
		pWrkrData->curlHeader = NULL;
	}
	if(pWrkrData->curlCheckConnHandle != NULL) {
		curl_easy_cleanup(pWrkrData->curlCheckConnHandle);
		pWrkrData->curlCheckConnHandle = NULL;
	}
	if(pWrkrData->curlPostHandle != NULL) {
		curl_easy_cleanup(pWrkrData->curlPostHandle);
		pWrkrData->curlPostHandle = NULL;
	}
	if (pWrkrData->restURL != NULL) {
		free(pWrkrData->restURL);
		pWrkrData->restURL = NULL;
	}
	es_deleteStr(pWrkrData->batch.data);
ENDfreeWrkrInstance

BEGINdbgPrintInstInfo
	int i;
CODESTARTdbgPrintInstInfo
	dbgprintf("omelasticsearch\n");
	dbgprintf("\ttemplate='%s'\n", pData->tplName);
	dbgprintf("\tnumServers=%d\n", pData->numServers);
	dbgprintf("\thealthCheckTimeout=%lu\n", pData->healthCheckTimeout);
	dbgprintf("\tserverBaseUrls=");
	for(i = 0 ; i < pData->numServers ; ++i)
		dbgprintf("%c'%s'", i == 0 ? '[' : ' ', pData->serverBaseUrls[i]);
	dbgprintf("]\n");
	dbgprintf("\tdefaultPort=%d\n", pData->defaultPort);
	dbgprintf("\tuid='%s'\n", pData->uid == NULL ? (uchar*)"(not configured)" : pData->uid);
	dbgprintf("\tpwd=(%sconfigured)\n", pData->pwd == NULL ? "not " : "");
	dbgprintf("\tsearch index='%s'\n", pData->searchIndex);
	dbgprintf("\tsearch index='%s'\n", pData->searchType);
	dbgprintf("\tparent='%s'\n", pData->parent);
	dbgprintf("\ttimeout='%s'\n", pData->timeout);
	dbgprintf("\tdynamic search index=%d\n", pData->dynSrchIdx);
	dbgprintf("\tdynamic search type=%d\n", pData->dynSrchType);
	dbgprintf("\tdynamic parent=%d\n", pData->dynParent);
	dbgprintf("\tuse https=%d\n", pData->useHttps);
	dbgprintf("\tbulkmode=%d\n", pData->bulkmode);
	dbgprintf("\tmaxbytes=%zu\n", pData->maxbytes);
	dbgprintf("\tallowUnsignedCerts=%d\n", pData->allowUnsignedCerts);
	dbgprintf("\terrorfile='%s'\n", pData->errorFile == NULL ?
		(uchar*)"(not configured)" : pData->errorFile);
	dbgprintf("\terroronly=%d\n", pData->errorOnly);
	dbgprintf("\tinterleaved=%d\n", pData->interleaved);
	dbgprintf("\tdynbulkid=%d\n", pData->dynBulkId);
	dbgprintf("\tbulkid='%s'\n", pData->bulkId);
ENDdbgPrintInstInfo


/* Build basic URL part, which includes hostname and port as follows:
 * http://hostname:port/ based on a server param
 * Newly creates a cstr for this purpose.
 * Note: serverParam MUST NOT end in '/' (caller must strip if it exists)
 */
static rsRetVal
computeBaseUrl(const char*const serverParam,
	const int defaultPort,
	const sbool useHttps,
	uchar **baseUrl)
{
#	define SCHEME_HTTPS "https://"
#	define SCHEME_HTTP "http://"

	char portBuf[64];
	int r = 0;
	const char *host = serverParam;
	DEFiRet;

	assert(serverParam[strlen(serverParam)-1] != '/');

	es_str_t *urlBuf = es_newStr(256);
	if (urlBuf == NULL) {
		DBGPRINTF("omelasticsearch: failed to allocate es_str urlBuf in computeBaseUrl\n");
		ABORT_FINALIZE(RS_RET_ERR);
	}

	/* Find where the hostname/ip of the server starts. If the scheme is not specified
	  in the uri, start the buffer with a scheme corresponding to the useHttps parameter.
	*/
	if (strcasestr(serverParam, SCHEME_HTTP))
		host = serverParam + strlen(SCHEME_HTTP);
	else if (strcasestr(serverParam, SCHEME_HTTPS))
		host = serverParam + strlen(SCHEME_HTTPS);
	else
		r = useHttps ? es_addBuf(&urlBuf, SCHEME_HTTPS, sizeof(SCHEME_HTTPS)-1) :
			es_addBuf(&urlBuf, SCHEME_HTTP, sizeof(SCHEME_HTTP)-1);

	if (r == 0) r = es_addBuf(&urlBuf, serverParam, strlen(serverParam));
	if (r == 0 && !strchr(host, ':')) {
		snprintf(portBuf, sizeof(portBuf), ":%d", defaultPort);
		r = es_addBuf(&urlBuf, portBuf, strlen(portBuf));
	}
	if (r == 0) r = es_addChar(&urlBuf, '/');
	if (r == 0) *baseUrl = (uchar*) es_str2cstr(urlBuf, NULL);

	if (r != 0 || baseUrl == NULL) {
		DBGPRINTF("omelasticsearch: error occurred computing baseUrl from server %s\n", serverParam);
		ABORT_FINALIZE(RS_RET_ERR);
	}
finalize_it:
	if (urlBuf) {
		es_deleteStr(urlBuf);
	}
	RETiRet;
}

static inline void
incrementServerIndex(wrkrInstanceData_t *pWrkrData)
{
	pWrkrData->serverIndex = (pWrkrData->serverIndex + 1) % pWrkrData->pData->numServers;
}

static rsRetVal
checkConn(wrkrInstanceData_t *pWrkrData)
{
#	define HEALTH_URI "_cat/health"
	CURL *curl;
	CURLcode res;
	es_str_t *urlBuf;
	char* healthUrl;
	char* serverUrl;
	int i;
	int r;
	DEFiRet;

	curl = pWrkrData->curlCheckConnHandle;
	urlBuf = es_newStr(256);
	if (urlBuf == NULL) {
		DBGPRINTF("omelasticsearch: unable to allocate buffer for health check uri.\n");
		ABORT_FINALIZE(RS_RET_SUSPENDED);
	}

	for(i = 0; i < pWrkrData->pData->numServers; ++i) {
		serverUrl = (char*) pWrkrData->pData->serverBaseUrls[pWrkrData->serverIndex];

		es_emptyStr(urlBuf);
		r = es_addBuf(&urlBuf, serverUrl, strlen(serverUrl));
		if(r == 0) r = es_addBuf(&urlBuf, HEALTH_URI, sizeof(HEALTH_URI)-1);
		if(r == 0) healthUrl = es_str2cstr(urlBuf, NULL);
		if(r != 0 || healthUrl == NULL) {
			DBGPRINTF("omelasticsearch: unable to allocate buffer for health check uri.\n");
			ABORT_FINALIZE(RS_RET_SUSPENDED);
		}

		curl_easy_setopt(curl, CURLOPT_URL, healthUrl);
		res = curl_easy_perform(curl);
		free(healthUrl);

		if (res == CURLE_OK) {
			DBGPRINTF("omelasticsearch: checkConn(%s) completed with success on attempt %d\n", serverUrl, i);
			ABORT_FINALIZE(RS_RET_OK);
		}

		DBGPRINTF("omelasticsearch: checkConn(%s) failed on attempt %d: %s\n", serverUrl, i, curl_easy_strerror(res));
		STATSCOUNTER_INC(checkConnFail, mutCheckConnFail);
		incrementServerIndex(pWrkrData);
	}

	DBGPRINTF("omelasticsearch: checkConn() failed after %d attempts.\n", i);
	ABORT_FINALIZE(RS_RET_SUSPENDED);

finalize_it:
	if(urlBuf != NULL)
		es_deleteStr(urlBuf);
	RETiRet;
}


BEGINtryResume
CODESTARTtryResume
	DBGPRINTF("omelasticsearch: tryResume called\n");
	iRet = checkConn(pWrkrData);
ENDtryResume


/* get the current index and type for this message */
static void
getIndexTypeAndParent(instanceData *pData, uchar **tpls,
		      uchar **srchIndex, uchar **srchType, uchar **parent,
		      uchar **bulkId)
{
	if(tpls == NULL) {
		*srchIndex = pData->searchIndex;
		*parent = pData->parent;
		*srchType = pData->searchType;
		*bulkId = NULL;
		goto done;
	}

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
done:	return;
}


static rsRetVal
setPostURL(wrkrInstanceData_t *pWrkrData, instanceData *pData, uchar **tpls)
{
	uchar *searchIndex = 0;
	uchar *searchType;
	uchar *parent;
	uchar *bulkId;
	char* baseUrl;
	es_str_t *url;
	int r;
	DEFiRet;
	char separator;
	const int bulkmode = pData->bulkmode;

	baseUrl = (char*)pData->serverBaseUrls[pWrkrData->serverIndex];
	url = es_newStrFromCStr(baseUrl, strlen(baseUrl));
	if (url == NULL) {
		DBGPRINTF("omelasticsearch: error allocating new estr for POST url.\n");
		ABORT_FINALIZE(RS_RET_ERR);
	}

	if(bulkmode) {
		r = es_addBuf(&url, "_bulk", sizeof("_bulk")-1);
		parent = NULL;
	} else {
		getIndexTypeAndParent(pData, tpls, &searchIndex, &searchType, &parent, &bulkId);
		r = es_addBuf(&url, (char*)searchIndex, ustrlen(searchIndex));
		if(r == 0) r = es_addChar(&url, '/');
		if(r == 0) r = es_addBuf(&url, (char*)searchType, ustrlen(searchType));
	}

	separator = '?';

	if(pData->timeout != NULL) {
		if(r == 0) r = es_addChar(&url, separator);
		if(r == 0) r = es_addBuf(&url, "timeout=", sizeof("timeout=")-1);
		if(r == 0) r = es_addBuf(&url, (char*)pData->timeout, ustrlen(pData->timeout));
		separator = '&';
	}

	if(parent != NULL) {
		if(r == 0) r = es_addChar(&url, separator);
		if(r == 0) r = es_addBuf(&url, "parent=", sizeof("parent=")-1);
		if(r == 0) es_addBuf(&url, (char*)parent, ustrlen(parent));
	}

	if(pWrkrData->restURL != NULL)
		free(pWrkrData->restURL);

	pWrkrData->restURL = (uchar*)es_str2cstr(url, NULL);
	curl_easy_setopt(pWrkrData->curlPostHandle, CURLOPT_URL, pWrkrData->restURL);
	DBGPRINTF("omelasticsearch: using REST URL: '%s'\n", pWrkrData->restURL);

finalize_it:
	if (url != NULL)
		es_deleteStr(url);
	RETiRet;
}


/* this method computes the expected size of adding the next message into
 * the batched request to elasticsearch
 */
static size_t
computeMessageSize(wrkrInstanceData_t *pWrkrData, uchar *message, uchar **tpls)
{
	size_t r = sizeof(META_STRT)-1 + sizeof(META_TYPE)-1 + sizeof(META_END)-1 + sizeof("\n")-1;

	uchar *searchIndex = 0;
	uchar *searchType;
	uchar *parent = NULL;
	uchar *bulkId = NULL;

	getIndexTypeAndParent(pWrkrData->pData, tpls, &searchIndex, &searchType, &parent, &bulkId);
	r += ustrlen((char *)message) + ustrlen(searchIndex) + ustrlen(searchType);

	if(parent != NULL) {
		r += sizeof(META_PARENT)-1 + ustrlen(parent);
	}
	if(bulkId != NULL) {
		r += sizeof(META_ID)-1 + ustrlen(bulkId);
	}

	return r;
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
	uchar *searchIndex = 0;
	uchar *searchType;
	uchar *parent = NULL;
	uchar *bulkId = NULL;
	DEFiRet;

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
	iRet = RS_RET_OK;

finalize_it:
	RETiRet;
}

/*
 * Dumps entire bulk request and response in error log
 */
static rsRetVal
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
static rsRetVal
getSection(const char* bulkRequest, const char **bulkRequestNextSectionStart )
{
		DEFiRet;
		char* index =0;
		if( (index = strchr(bulkRequest,'\n')) != 0)/*intermediate section*/
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
 * and sets lastLocation pointer to the location till which bulkrequest has been parsed.
 * (used as input to make function thread safe.)
 */
static rsRetVal
getSingleRequest(const char* bulkRequest, char** singleRequest, const char **lastLocation)
{
	DEFiRet;
	const char *req = bulkRequest;
	const char *start = bulkRequest;
	if (getSection(req,&req)!=RS_RET_OK)
		ABORT_FINALIZE(RS_RET_ERR);

	if (getSection(req,&req)!=RS_RET_OK)
			ABORT_FINALIZE(RS_RET_ERR);

	CHKmalloc(*singleRequest = (char*) calloc (req - start+ 1 + 1,1));
	/* (req - start+ 1 == length of data + 1 for terminal char)*/
	memcpy(*singleRequest,start,req - start);
	*lastLocation=req;

finalize_it:
	RETiRet;
}

/*
 * check the status of response from ES
 */
static int checkReplyStatus(cJSON* ok) {
	return (ok == NULL || ok->type != cJSON_Number || ok->valueint < 0 || ok->valueint > 299);
}

/*
 * Context object for error file content creation or status check
 */
typedef struct exeContext{
	int statusCheckOnly;
	cJSON *errRoot;
	rsRetVal (*prepareErrorFileContent)(struct exeContext *ctx,int itemStatus,char *request,char *response);


} context;

/*
 * get content to be written in error file using context passed
 */
static rsRetVal
parseRequestAndResponseForContext(wrkrInstanceData_t *pWrkrData,cJSON **pReplyRoot,uchar *reqmsg,context *ctx)
{
	DEFiRet;
	cJSON *replyRoot = *pReplyRoot;
	int i;
	int numitems;
	cJSON *items=0;


	/*iterate over items*/
	items = cJSON_GetObjectItem(replyRoot, "items");
	if(items == NULL || items->type != cJSON_Array) {
		DBGPRINTF("omelasticsearch: error in elasticsearch reply: "
			  "bulkmode insert does not return array, reply is: %s\n",
			  pWrkrData->reply);
		ABORT_FINALIZE(RS_RET_DATAFAIL);
	}

	numitems = cJSON_GetArraySize(items);

	DBGPRINTF("omelasticsearch: Entire request %s\n",reqmsg);
	const char *lastReqRead= (char*)reqmsg;

	DBGPRINTF("omelasticsearch: %d items in reply\n", numitems);
	for(i = 0 ; i < numitems ; ++i) {

		cJSON *item=0;
		cJSON *result=0;
		cJSON *ok=0;
		int itemStatus=0;
		item = cJSON_GetArrayItem(items, i);
		if(item == NULL)  {
			DBGPRINTF("omelasticsearch: error in elasticsearch reply: "
				  "cannot obtain reply array item %d\n", i);
			ABORT_FINALIZE(RS_RET_DATAFAIL);
		}
		result = item->child;
		if(result == NULL || result->type != cJSON_Object) {
			DBGPRINTF("omelasticsearch: error in elasticsearch reply: "
				  "cannot obtain 'result' item for #%d\n", i);
			ABORT_FINALIZE(RS_RET_DATAFAIL);
		}

		ok = cJSON_GetObjectItem(result, "status");
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

			response = cJSON_PrintUnformatted(result);

			if(response==NULL)
			{
				free(request);/*as its has been assigned.*/
				DBGPRINTF("omelasticsearch: Error getting cJSON_PrintUnformatted. Cannot continue\n");
				ABORT_FINALIZE(RS_RET_ERR);
			}

			/*call the context*/
			rsRetVal ret = ctx->prepareErrorFileContent(ctx, itemStatus, request,response);

			/*free memory in any case*/
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
static rsRetVal
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

static rsRetVal
getDataInterleaved(context *ctx,
	int __attribute__((unused)) itemStatus,
	char *request,
	char *response)
{
	DEFiRet;
	cJSON *interleaved =0;
	if((interleaved=cJSON_GetObjectItem(ctx->errRoot, "response")) == NULL)
	{
		DBGPRINTF("omelasticsearch: Failed to get response json array. Invalid context. Cannot continue\n");
		ABORT_FINALIZE(RS_RET_ERR);
	}

	cJSON *interleavedNode=0;
	/*create interleaved node that has req and response json data*/
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

static rsRetVal
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

/*
 * get erroronly context
 */
static rsRetVal
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

/*
 * get interleaved context
 */
static rsRetVal
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

/*get interleaved context*/
static rsRetVal
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
static rsRetVal
writeDataError(wrkrInstanceData_t *pWrkrData, instanceData *pData, cJSON **pReplyRoot, uchar *reqmsg)
{
	char *rendered = NULL;
	size_t toWrite;
	ssize_t wrRet;
	sbool bMutLocked = 0;
	char errStr[1024];
	context ctx;
	ctx.errRoot=0;
	DEFiRet;

	if(pData->errorFile == NULL) {
		DBGPRINTF("omelasticsearch: no local error logger defined - "
		          "ignoring ES error information\n");
		FINALIZE;
	}

	pthread_mutex_lock(&pData->mutErrFile);
	bMutLocked = 1;

	DBGPRINTF("omelasticsearch: error file mode: erroronly='%d' errorInterleaved='%d'\n", pData->errorOnly,
	pData->interleaved);

	if(pData->interleaved ==0 && pData->errorOnly ==0)/*default write*/
	{
		if(getDataErrorDefault(pWrkrData,pReplyRoot,reqmsg,&rendered) != RS_RET_OK) {
			ABORT_FINALIZE(RS_RET_ERR);
		}
	}
	else
	{
		/*get correct context.*/
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

		/*execute context*/
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


static rsRetVal
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


static rsRetVal
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

static void
initializeBatch(wrkrInstanceData_t *pWrkrData) {
	es_emptyStr(pWrkrData->batch.data);
	pWrkrData->batch.nmemb = 0;
}

static rsRetVal
curlPost(wrkrInstanceData_t *pWrkrData, uchar *message, int msglen, uchar **tpls, int nmsgs)
{
	CURLcode code;
	CURL *curl = pWrkrData->curlPostHandle;
	DEFiRet;

	pWrkrData->reply = NULL;
	pWrkrData->replyLen = 0;

	CHKiRet(checkConn(pWrkrData));
	CHKiRet(setPostURL(pWrkrData, pWrkrData->pData, tpls));

	curl_easy_setopt(curl, CURLOPT_WRITEDATA, pWrkrData);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, (char *)message);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, msglen);
	code = curl_easy_perform(curl);
	if (   code == CURLE_COULDNT_RESOLVE_HOST
	    || code == CURLE_COULDNT_RESOLVE_PROXY
	    || code == CURLE_COULDNT_CONNECT
	    || code == CURLE_WRITE_ERROR
	   ) {
		STATSCOUNTER_INC(indexHTTPReqFail, mutIndexHTTPReqFail);
		indexHTTPFail += nmsgs;
		DBGPRINTF("omelasticsearch: we are suspending ourselfs due "
			  "to failure %lld of curl_easy_perform()\n",
			  (long long) code);
		ABORT_FINALIZE(RS_RET_SUSPENDED);
	}

	DBGPRINTF("omelasticsearch: pWrkrData replyLen = '%d'\n", pWrkrData->replyLen);
	if(pWrkrData->replyLen > 0) {
		pWrkrData->reply[pWrkrData->replyLen] = '\0';
		/* Append 0 Byte if replyLen is above 0 - byte has been reserved in malloc */
	}
	DBGPRINTF("omelasticsearch: pWrkrData reply: '%s'\n", pWrkrData->reply);

	CHKiRet(checkResult(pWrkrData, message));
finalize_it:
	incrementServerIndex(pWrkrData);
	free(pWrkrData->reply);
	RETiRet;
}

static rsRetVal
submitBatch(wrkrInstanceData_t *pWrkrData)
{
	char *cstr = NULL;
	DEFiRet;

	cstr = es_str2cstr(pWrkrData->batch.data, NULL);
	dbgprintf("omelasticsearch: submitBatch, batch: '%s'\n", cstr);

	CHKiRet(curlPost(pWrkrData, (uchar*) cstr, strlen(cstr), NULL, pWrkrData->batch.nmemb));

finalize_it:
	free(cstr);
	RETiRet;
}

BEGINbeginTransaction
CODESTARTbeginTransaction
	if(!pWrkrData->pData->bulkmode) {
		FINALIZE;
	}

	initializeBatch(pWrkrData);
finalize_it:
ENDbeginTransaction

BEGINdoAction
CODESTARTdoAction
	STATSCOUNTER_INC(indexSubmit, mutIndexSubmit);

	if(pWrkrData->pData->bulkmode) {
		size_t nBytes = computeMessageSize(pWrkrData, ppString[0], ppString);

		/* If max bytes is set and this next message will put us over the limit, submit the current buffer and reset */
		if (pWrkrData->pData->maxbytes > 0 && es_strlen(pWrkrData->batch.data) + nBytes > pWrkrData->pData->maxbytes ) {
			dbgprintf("omelasticsearch: maxbytes limit reached, submitting partial batch of %d "
			"elements.\n", pWrkrData->batch.nmemb);
			CHKiRet(submitBatch(pWrkrData));
			initializeBatch(pWrkrData);
		}
		CHKiRet(buildBatch(pWrkrData, ppString[0], ppString));

		/* If there is only one item in the batch, all previous items have been submitted or this is the first item
		   for this transaction. Return previous committed so that all items leading up to the current (exclusive)
		   are not replayed should a failure occur anywhere else in the transaction. */
		iRet = pWrkrData->batch.nmemb == 1 ? RS_RET_PREVIOUS_COMMITTED : RS_RET_DEFER_COMMIT;
	} else {
		CHKiRet(curlPost(pWrkrData, ppString[0], strlen((char*)ppString[0]),
		                 ppString, 1));
	}
finalize_it:
ENDdoAction


BEGINendTransaction
CODESTARTendTransaction
	/* End Transaction only if batch data is not empty */
	if (pWrkrData->batch.data != NULL && pWrkrData->batch.nmemb > 0) {
		CHKiRet(submitBatch(pWrkrData));
	} else {
		dbgprintf("omelasticsearch: endTransaction, pWrkrData->batch.data is NULL, nothing to send. \n");
	}
finalize_it:
ENDendTransaction

/* elasticsearch POST result string ... useful for debugging */
static size_t
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
computeAuthHeader(char* uid, char* pwd, uchar** authBuf) {
	int r;
	DEFiRet;
	es_str_t* auth = es_newStr(1024);

	if (auth == NULL) {
		DBGPRINTF("omelasticsearch: failed to allocate es_str auth for auth header construction\n");
		ABORT_FINALIZE(RS_RET_ERR);
	}

	r = es_addBuf(&auth, uid, strlen(uid));
	if(r == 0) r = es_addChar(&auth, ':');
	if(r == 0 && pwd != NULL) r = es_addBuf(&auth, pwd, strlen(pwd));
	if(r == 0) *authBuf = (uchar*) es_str2cstr(auth, NULL);

	if (r != 0 || *authBuf == NULL) {
		errmsg.LogError(0, RS_RET_ERR, "omelasticsearch: failed to build auth header\n");
		ABORT_FINALIZE(RS_RET_ERR);
	}

finalize_it:
	if (auth != NULL)
		es_deleteStr(auth);
	RETiRet;
}

static void
curlCheckConnSetup(CURL *handle, HEADER *header, long timeout, sbool allowUnsignedCerts)
{
	curl_easy_setopt(handle, CURLOPT_HTTPHEADER, header);
	curl_easy_setopt(handle, CURLOPT_NOBODY, TRUE);
	curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, timeout);
	curl_easy_setopt(handle, CURLOPT_NOSIGNAL, TRUE);

	if(allowUnsignedCerts)
		curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, FALSE);

	/* Only enable for debugging
	curl_easy_setopt(curl, CURLOPT_VERBOSE, TRUE); */
}

static void
curlPostSetup(CURL *handle, HEADER *header, uchar* authBuf)
{
	curl_easy_setopt(handle, CURLOPT_HTTPHEADER, header);
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, curlResult);
	curl_easy_setopt(handle, CURLOPT_POST, 1);
	curl_easy_setopt(handle, CURLOPT_NOSIGNAL, TRUE);

	if(authBuf != NULL) {
		curl_easy_setopt(handle, CURLOPT_USERPWD, authBuf);
		curl_easy_setopt(handle, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
	}
}

static rsRetVal
curlSetup(wrkrInstanceData_t *pWrkrData, instanceData *pData)
{
	pWrkrData->curlHeader = curl_slist_append(NULL, "Content-Type: text/json; charset=utf-8");
	pWrkrData->curlPostHandle = curl_easy_init();
	if (pWrkrData->curlPostHandle == NULL) {
		return RS_RET_OBJ_CREATION_FAILED;
	}
	curlPostSetup(pWrkrData->curlPostHandle, pWrkrData->curlHeader, pData->authBuf);

	pWrkrData->curlCheckConnHandle = curl_easy_init();
	if (pWrkrData->curlCheckConnHandle == NULL) {
		curl_easy_cleanup(pWrkrData->curlPostHandle);
		pWrkrData->curlPostHandle = NULL;
		return RS_RET_OBJ_CREATION_FAILED;
	}
	curlCheckConnSetup(pWrkrData->curlCheckConnHandle, pWrkrData->curlHeader,
		pData->healthCheckTimeout, pData->allowUnsignedCerts);

	return RS_RET_OK;
}

static void
setInstParamDefaults(instanceData *pData)
{
	pData->serverBaseUrls = NULL;
	pData->defaultPort = 9200;
	pData->healthCheckTimeout = 3500;
	pData->uid = NULL;
	pData->pwd = NULL;
	pData->authBuf = NULL;
	pData->searchIndex = NULL;
	pData->searchType = NULL;
	pData->parent = NULL;
	pData->timeout = NULL;
	pData->dynSrchIdx = 0;
	pData->dynSrchType = 0;
	pData->dynParent = 0;
	pData->useHttps = 0;
	pData->bulkmode = 0;
	pData->maxbytes = 104857600; //100 MB Is the default max message size that ships with ElasticSearch
	pData->allowUnsignedCerts = 0;
	pData->tplName = NULL;
	pData->errorFile = NULL;
	pData->errorOnly=0;
	pData->interleaved=0;
	pData->dynBulkId= 0;
	pData->bulkId = NULL;
}

BEGINnewActInst
	struct cnfparamvals *pvals;
	char* serverParam = NULL;
	struct cnfarray* servers = NULL;
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
			servers = pvals[i].val.d.ar;
		} else if(!strcmp(actpblk.descr[i].name, "errorfile")) {
			pData->errorFile = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		}else if(!strcmp(actpblk.descr[i].name, "erroronly")) {
			pData->errorOnly = pvals[i].val.d.n;
		}else if(!strcmp(actpblk.descr[i].name, "interleaved")) {
			pData->interleaved = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "serverport")) {
			pData->defaultPort = (int) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "healthchecktimeout")) {
			pData->healthCheckTimeout = (long) pvals[i].val.d.n;
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
		} else if(!strcmp(actpblk.descr[i].name, "maxbytes")) {
			pData->maxbytes = (size_t) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "allowunsignedcerts")) {
			pData->allowUnsignedCerts = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "timeout")) {
			pData->timeout = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
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

	if (pData->uid != NULL)
		CHKiRet(computeAuthHeader((char*) pData->uid, (char*) pData->pwd, &pData->authBuf));

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

	if (servers != NULL) {
		pData->numServers = servers->nmemb;
		pData->serverBaseUrls = malloc(servers->nmemb * sizeof(uchar*));
		if (pData->serverBaseUrls == NULL) {
			errmsg.LogError(0, RS_RET_ERR, "omelasticsearch: unable to allocate buffer "
					"for ElasticSearch server configuration.");
			ABORT_FINALIZE(RS_RET_ERR);
		}

		for(i = 0 ; i < servers->nmemb ; ++i) {
			serverParam = es_str2cstr(servers->arr[i], NULL);
			if (serverParam == NULL) {
				errmsg.LogError(0, RS_RET_ERR, "omelasticsearch: unable to allocate buffer "
					"for ElasticSearch server configuration.");
				ABORT_FINALIZE(RS_RET_ERR);
			}
			/* Remove a trailing slash if it exists */
			const size_t serverParamLastChar = strlen(serverParam)-1;
			if (serverParam[serverParamLastChar] == '/') {
				serverParam[serverParamLastChar] = '\0';
			}
			CHKiRet(computeBaseUrl(serverParam, pData->defaultPort, pData->useHttps, pData->serverBaseUrls + i));
			free(serverParam);
			serverParam = NULL;
		}
	} else {
		dbgprintf("omelasticsearch: No servers specified, using localhost\n");
		pData->numServers = 1;
		pData->serverBaseUrls = malloc(sizeof(uchar*));
		if (pData->serverBaseUrls == NULL) {
			errmsg.LogError(0, RS_RET_ERR, "omelasticsearch: unable to allocate buffer "
					"for ElasticSearch server configuration.");
			ABORT_FINALIZE(RS_RET_ERR);
		}
		CHKiRet(computeBaseUrl("localhost", pData->defaultPort, pData->useHttps, pData->serverBaseUrls));
	}

	if(pData->searchIndex == NULL)
		pData->searchIndex = (uchar*) strdup("system");
	if(pData->searchType == NULL)
		pData->searchType = (uchar*) strdup("events");
CODE_STD_FINALIZERnewActInst
	cnfparamvalsDestruct(pvals, &actpblk);
	if (serverParam)
		free(serverParam);
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
	STATSCOUNTER_INIT(checkConnFail, mutCheckConnFail);
	CHKiRet(statsobj.AddCounter(indexStats, (uchar *)"failed.checkConn",
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &checkConnFail));
	STATSCOUNTER_INIT(indexESFail, mutIndexESFail);
	CHKiRet(statsobj.AddCounter(indexStats, (uchar *)"failed.es",
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &indexESFail));
	CHKiRet(statsobj.ConstructFinalize(indexStats));
ENDmodInit

/* vi:set ai:
 */
