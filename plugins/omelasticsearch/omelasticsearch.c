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
STATSCOUNTER_DEF(indexConFail, mutIndexConFail)
STATSCOUNTER_DEF(indexSubmit, mutIndexSubmit)
STATSCOUNTER_DEF(indexFailed, mutIndexFailed)
STATSCOUNTER_DEF(indexSuccess, mutIndexSuccess)

/* REST API for elasticsearch hits this URL:
 * http://<hostName>:<restPort>/<searchIndex>/<searchType>
 */
typedef struct curl_slist HEADER;
typedef struct _instanceData {
	int port;
	int replyLen;
	int fdErrFile;		/* error file fd or -1 if not open */
	uchar *server;
	uchar *uid;
	uchar *pwd;
	uchar *searchIndex;
	uchar *searchType;
	uchar *parent;
	uchar *tplName;
	uchar *timeout;
	uchar *bulkId;
	uchar *restURL;		/* last used URL for error reporting */
	uchar *errorFile;
	char *reply;
	sbool dynSrchIdx;
	sbool dynSrchType;
	sbool dynParent;
	sbool dynBulkId;
	sbool bulkmode;
	sbool asyncRepl;
	struct {
		es_str_t *data;
		uchar *currTpl1;
		uchar *currTpl2;
	} batch;
	CURL	*curlHandle;	/* libcurl session handle */
	HEADER	*postHeader;	/* json POST request info */
} instanceData;


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
	{ "timeout", eCmdHdlrGetWord, 0 },
	{ "errorfile", eCmdHdlrGetWord, 0 },
	{ "template", eCmdHdlrGetWord, 1 },
	{ "dynbulkid", eCmdHdlrBinary, 0 },
	{ "bulkid", eCmdHdlrGetWord, 0 },
};
static struct cnfparamblk actpblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(actpdescr)/sizeof(struct cnfparamdescr),
	  actpdescr
	};

BEGINcreateInstance
CODESTARTcreateInstance
	pData->restURL = NULL;
	pData->fdErrFile = -1;
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
	if(pData->fdErrFile != -1)
		close(pData->fdErrFile);
	free(pData->server);
	free(pData->uid);
	free(pData->pwd);
	free(pData->searchIndex);
	free(pData->searchType);
	free(pData->parent);
	free(pData->tplName);
	free(pData->timeout);
	free(pData->restURL);
	free(pData->errorFile);
	free(pData->bulkId);
ENDfreeInstance

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
	dbgprintf("\tbulkmode=%d\n", pData->bulkmode);
	dbgprintf("\terrorfile='%s'\n", pData->errorFile == NULL ?
		(uchar*)"(not configured)" : pData->errorFile);
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
	r = es_addBuf(url, "http://", sizeof("http://")-1);
	if(r == 0) r = es_addBuf(url, (char*)pData->server, strlen((char*)pData->server));
	if(r == 0) r = es_addChar(url, ':');
	if(r == 0) r = es_addBuf(url, portBuf, strlen(portBuf));
	if(r == 0) r = es_addChar(url, '/');
	RETiRet;
}


static inline rsRetVal
checkConn(instanceData *pData)
{
	es_str_t *url;
	CURL *curl = NULL;
	CURLcode res;
	char *cstr;
	DEFiRet;

	setBaseURL(pData, &url);
	curl = curl_easy_init();
	if(curl == NULL) {
		DBGPRINTF("omelasticsearch: checkConn() curl_easy_init() failed\n");
		ABORT_FINALIZE(RS_RET_SUSPENDED);
	}
	cstr = es_str2cstr(url, NULL);
	curl_easy_setopt(curl, CURLOPT_URL, cstr);
	free(cstr);

	pData->reply = NULL;
	pData->replyLen = 0;
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, pData);
	res = curl_easy_perform(curl);
	if(res != CURLE_OK) {
		DBGPRINTF("omelasticsearch: checkConn() curl_easy_perform() "
			  "failed: %s\n", curl_easy_strerror(res));
		ABORT_FINALIZE(RS_RET_SUSPENDED);
	}
	free(pData->reply);
	DBGPRINTF("omelasticsearch: checkConn() completed with success\n");

finalize_it:
	if(curl != NULL)
		curl_easy_cleanup(curl);
	RETiRet;
}


BEGINtryResume
CODESTARTtryResume
	DBGPRINTF("omelasticsearch: tryResume called\n");
	iRet = checkConn(pData);
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
setCurlURL(instanceData *pData, uchar **tpls)
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
		if(r == 0) r = es_addBuf(&url, (char*)parent, ustrlen(parent));
	}

	free(pData->restURL);
	pData->restURL = (uchar*)es_str2cstr(url, NULL);
	curl_easy_setopt(pData->curlHandle, CURLOPT_URL, pData->restURL);
	es_deleteStr(url);
	DBGPRINTF("omelasticsearch: using REST URL: '%s'\n", pData->restURL);

	if(pData->uid != NULL) {
		rLocal = snprintf(authBuf, sizeof(authBuf), "%s:%s", pData->uid,
			         (pData->pwd == NULL) ? "" : (char*)pData->pwd);
		if(rLocal < 1) {
			errmsg.LogError(0, RS_RET_ERR, "omelasticsearch: snprintf failed "
				"when trying to build auth string (return %d)\n",
				rLocal);
			ABORT_FINALIZE(RS_RET_ERR);
		}
		curl_easy_setopt(pData->curlHandle, CURLOPT_USERPWD, authBuf);
		curl_easy_setopt(pData->curlHandle, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
	}
finalize_it:
	RETiRet;
}


/* this method does not directly submit but builds a batch instead. It
 * may submit, if we have dynamic index/type and the current type or
 * index changes.
 */
static rsRetVal
buildBatch(instanceData *pData, uchar *message, uchar **tpls)
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

	getIndexTypeAndParent(pData, tpls, &searchIndex, &searchType, &parent, &bulkId);
	r = es_addBuf(&pData->batch.data, META_STRT, sizeof(META_STRT)-1);
	if(r == 0) r = es_addBuf(&pData->batch.data, (char*)searchIndex,
				 ustrlen(searchIndex));
	if(r == 0) r = es_addBuf(&pData->batch.data, META_TYPE, sizeof(META_TYPE)-1);
	if(r == 0) r = es_addBuf(&pData->batch.data, (char*)searchType,
				 ustrlen(searchType));
	if(parent != NULL) {
		if(r == 0) r = es_addBuf(&pData->batch.data, META_PARENT, sizeof(META_PARENT)-1);
		if(r == 0) r = es_addBuf(&pData->batch.data, (char*)parent, ustrlen(parent));
	}
	if(bulkId != NULL) {
		if(r == 0) r = es_addBuf(&pData->batch.data, META_ID, sizeof(META_ID)-1);
		if(r == 0) r = es_addBuf(&pData->batch.data, (char*)bulkId, ustrlen(bulkId));
	}
	if(r == 0) r = es_addBuf(&pData->batch.data, META_END, sizeof(META_END)-1);
	if(r == 0) r = es_addBuf(&pData->batch.data, (char*)message, length);
	if(r == 0) r = es_addBuf(&pData->batch.data, "\n", sizeof("\n")-1);
	if(r != 0) {
		DBGPRINTF("omelasticsearch: growing batch failed with code %d\n", r);
		ABORT_FINALIZE(RS_RET_ERR);
	}
	iRet = RS_RET_DEFER_COMMIT;

finalize_it:
	RETiRet;
}


/* write data error request/replies to separate error file
 * Note: we open the file but never close it before exit. If it
 * needs to be closed, HUP must be sent.
 */
static inline rsRetVal
writeDataError(instanceData *pData, cJSON **pReplyRoot, uchar *reqmsg)
{
	char *rendered = NULL;
	cJSON *errRoot;
	cJSON *req;
	cJSON *replyRoot = *pReplyRoot;
	size_t toWrite;
	ssize_t wrRet;
	char errStr[1024];
	DEFiRet;

	if(pData->errorFile == NULL) {
		DBGPRINTF("omelasticsearch: no local error logger defined - "
		          "ignoring ES error information\n");
		FINALIZE;
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
	if((req=cJSON_CreateObject()) == NULL) ABORT_FINALIZE(RS_RET_ERR);
	cJSON_AddItemToObject(req, "url", cJSON_CreateString((char*)pData->restURL));
	cJSON_AddItemToObject(req, "postdata", cJSON_CreateString((char*)reqmsg));

	if((errRoot=cJSON_CreateObject()) == NULL) ABORT_FINALIZE(RS_RET_ERR);
	cJSON_AddItemToObject(errRoot, "request", req);
	cJSON_AddItemToObject(errRoot, "reply", replyRoot);
	rendered = cJSON_Print(errRoot);
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
	free(rendered);
	cJSON_Delete(errRoot);
	*pReplyRoot = NULL; /* tell caller not to delete once again! */

finalize_it:
	if(rendered != NULL)
		free(rendered);
	RETiRet;
}


static inline rsRetVal
checkResultBulkmode(instanceData *pData, cJSON *root)
{
	int i;
	int numitems;
	cJSON *items;
	cJSON *item;
	cJSON *create;
	cJSON *ok;
	DEFiRet;

	items = cJSON_GetObjectItem(root, "items");
	if(items == NULL || items->type != cJSON_Array) {
		DBGPRINTF("omelasticsearch: error in elasticsearch reply: "
			  "bulkmode insert does not return array, reply is: %s\n",
			  pData->reply);
		ABORT_FINALIZE(RS_RET_DATAFAIL);
	}
	numitems = cJSON_GetArraySize(items);
DBGPRINTF("omelasticsearch: %d items in reply\n", numitems);
	for(i = 0 ; i < numitems ; ++i) {
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
		ok = cJSON_GetObjectItem(create, "ok");
		if(ok == NULL || ok->type != cJSON_True) {
			DBGPRINTF("omelasticsearch: error in elasticsearch reply: "
				  "item %d, prop ok (%p) not ok\n", i, ok);
			ABORT_FINALIZE(RS_RET_DATAFAIL);
		}
	}

finalize_it:
	RETiRet;
}


static inline rsRetVal
checkResult(instanceData *pData, uchar *reqmsg)
{
	cJSON *root;
	cJSON *ok;
	DEFiRet;

	root = cJSON_Parse(pData->reply);
	if(root == NULL) {
		DBGPRINTF("omelasticsearch: could not parse JSON result \n");
		ABORT_FINALIZE(RS_RET_ERR);
	}

	if(pData->bulkmode) {
		iRet = checkResultBulkmode(pData, root);
	} else {
		ok = cJSON_GetObjectItem(root, "ok");
		if(ok == NULL || ok->type != cJSON_True) {
			iRet = RS_RET_DATAFAIL;
		}
	}

	/* Note: we ignore errors writing the error file, as we cannot handle
	 * these in any case.
	 */
	if(iRet == RS_RET_DATAFAIL) {
		writeDataError(pData, &root, reqmsg);
		iRet = RS_RET_OK; /* we have handled the problem! */
	}

finalize_it:
	if(root != NULL)
		cJSON_Delete(root);
	RETiRet;
}


static rsRetVal
curlPost(instanceData *pData, uchar *message, int msglen, uchar **tpls)
{
	CURLcode code;
	CURL *curl = pData->curlHandle;
	DEFiRet;

	pData->reply = NULL;
	pData->replyLen = 0;

	if(pData->dynSrchIdx || pData->dynSrchType || pData->dynParent)
		CHKiRet(setCurlURL(pData, tpls));

	curl_easy_setopt(curl, CURLOPT_WRITEDATA, pData);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, (char *)message);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, msglen);
	code = curl_easy_perform(curl);
	switch (code) {
		case CURLE_COULDNT_RESOLVE_HOST:
		case CURLE_COULDNT_RESOLVE_PROXY:
		case CURLE_COULDNT_CONNECT:
		case CURLE_WRITE_ERROR:
			STATSCOUNTER_INC(indexConFail, mutIndexConFail);
			DBGPRINTF("omelasticsearch: we are suspending ourselfs due "
				  "to failure %lld of curl_easy_perform()\n",
				  (long long) code);
			ABORT_FINALIZE(RS_RET_SUSPENDED);
		default:
			STATSCOUNTER_INC(indexSubmit, mutIndexSubmit);
			break;
	}

	pData->reply[pData->replyLen] = '\0'; /* byte has been reserved in malloc */
	DBGPRINTF("omelasticsearch: es reply: '%s'\n", pData->reply);

	CHKiRet(checkResult(pData, message));
finalize_it:
	free(pData->reply);
	RETiRet;
}

BEGINbeginTransaction
CODESTARTbeginTransaction
dbgprintf("omelasticsearch: beginTransaction\n");
	if(!pData->bulkmode) {
		FINALIZE;
	}

	es_emptyStr(pData->batch.data);
finalize_it:
ENDbeginTransaction


BEGINdoAction
CODESTARTdoAction
	if(pData->bulkmode) {
		CHKiRet(buildBatch(pData, ppString[0], ppString));
	} else {
		CHKiRet(curlPost(pData, ppString[0], strlen((char*)ppString[0]),
		                 ppString));
	}
finalize_it:
dbgprintf("omelasticsearch: result doAction: %d (bulkmode %d)\n", iRet, pData->bulkmode);
ENDdoAction


BEGINendTransaction
	char *cstr;
CODESTARTendTransaction
dbgprintf("omelasticsearch: endTransaction init\n");
	cstr = es_str2cstr(pData->batch.data, NULL);
	dbgprintf("omelasticsearch: endTransaction, batch: '%s'\n", cstr);
	CHKiRet(curlPost(pData, (uchar*) cstr, strlen(cstr), NULL));
finalize_it:
	free(cstr);
dbgprintf("omelasticsearch: endTransaction done with %d\n", iRet);
ENDendTransaction

/* elasticsearch POST result string ... useful for debugging */
size_t
curlResult(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	char *p = (char *)ptr;
	instanceData *pData = (instanceData*) userdata;
	char *buf;
	size_t newlen;

	newlen = pData->replyLen + size*nmemb;
	if((buf = realloc(pData->reply, newlen + 1)) == NULL) {
		DBGPRINTF("omelasticsearch: realloc failed in curlResult\n");
		return 0; /* abort due to failure */
	}
	memcpy(buf+pData->replyLen, p, size*nmemb);
	pData->replyLen = newlen;
	pData->reply = buf;
	return size*nmemb;
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

	if(    pData->bulkmode
	   || (pData->dynSrchIdx == 0 && pData->dynSrchType == 0 && pData->dynParent == 0)) {
		/* in this case, we know no tpls are involved in the request-->NULL OK! */
		setCurlURL(pData, NULL);
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
	pData->bulkmode = 0;
	pData->tplName = NULL;
	pData->errorFile = NULL;
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

	if(pData->bulkmode) {
		pData->batch.currTpl1 = NULL;
		pData->batch.currTpl2 = NULL;
		if((pData->batch.data = es_newStr(1024)) == NULL) {
			DBGPRINTF("omelasticsearch: error creating batch string "
			          "turned off bulk mode\n");
			pData->bulkmode = 0; /* at least it works */
		}
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
