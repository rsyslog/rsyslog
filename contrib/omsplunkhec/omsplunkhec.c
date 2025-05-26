/* omsplunkhec.c
 * This module is a middleware that is used to send logs received by Rsyslog to a HEC Splunk server
 * This method of collect is a alternative to the module UF of Splunk
 *
 * THANKS :
 * I would like to thanks the creator of the OMHTTP module first.
 * I take a lots of function of his code to adapt this code for HEC Splunk
 * This module is more like "a fork" of the OMHTTP module
 * Thanks to Nathan Scott, Rainer Gerhards and Adiscon GmbH, Christian Tramnitz !
 *
 *
 * Copyright 2025 Adrien GANDARIAS.
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
#include <json.h>
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include "cfsysline.h"
#include "unicode-helper.h"
#include "obj-types.h"
#include "ratelimit.h"
#include "ruleset.h"
#include "statsobj.h"

#ifndef O_LARGEFILE
#  define O_LARGEFILE 0
#endif

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("omsplunkhec")

/* internal structures */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(prop)
DEFobjCurrIf(statsobj)


/* Stats Object */
statsobj_t *httpStats;
STATSCOUNTER_DEF(ctrSplkRequestsSubmitted, mutCtrSplkRequestsSubmitted);
STATSCOUNTER_DEF(splkRequestsSuccess, mutSplkRequestsSuccess);
STATSCOUNTER_DEF(splkRequestsFailed, mutSplkRequestsFailed);
STATSCOUNTER_DEF(ctrSplkRequestsCount, mutCtrSplkRequestsCount);
STATSCOUNTER_DEF(splkRequestsBytes, mutSplkRequestsBytes);
STATSCOUNTER_DEF(splkRequestsTimeMs, mutSplkRequestsTimeMs);



static prop_t *pInputName = NULL;
static int omSplunkHecInstancesCnt = 0;

#define WRKR_DATA_TYPE_ES 0xBADF0001
#define ERR_MSG_NULL "NULL: curl request failed or no response"
#define HTTP_HEADER_CONTENT_JSON "Content-Type: application/json; charset=utf-8"
#define HTTP_HEADER_CONTENT_TEXT "Content-Type: text/plain"
#define HTTP_HEADER_AUTHORIZATION "Authorization: Splunk "
#define HTTP_HEADER_EXPECT_EMPTY "Expect:"
#define HTTPS "https://"
#define HTTP "http://"
#define DEFAULTJSONTEMPLATE "StdJSONFmt"

typedef struct curl_slist HEADER;

/* config variables */
typedef struct instanceConf_s {
	/* Thread */
	pthread_mutex_t mutErrFile;

	/* Splunk */
	uchar **serverList; // List of all HEC Splunk
	int numListServerName; // Number of server HEC Splunk
	int lastServerUsed;
	uchar *token;
	uchar *template;
	uchar *restpath;
	int restPathTimeout;
	int port;

	/* Buffer */
	uchar *bufHeaderContentType;

	/* Batch */
	sbool batch;
	size_t batchMaxBatchBytes;
	size_t batchMaxBatchSize;

	/* Proxy */
	uchar *proxyHost;
	int proxyPort;
	
	/* TLS */
	sbool useTLS;
	sbool allowUnsignedCerts;
	sbool skipVerifyHost;
	uchar *caCertFile;
	uchar *myCertFile;
	uchar *myPrivKeyFile;

	/* Error file */
	uchar *errorFilename;
	int fdErrorfile;
	sbool reloadOnHup;

	/* Rate Limit */
	//unsigned int ratelimitInterval;
	//unsigned int ratelimitBurst;

	/* Stats Counter */
	statsobj_t *stats;
	uchar *statsName;
	STATSCOUNTER_DEF(ctrSplkRequestsSubmitted, mutCtrSplkRequestsSubmitted);
	STATSCOUNTER_DEF(splkRequestsSuccess, mutSplkRequestsSuccess);
	STATSCOUNTER_DEF(splkRequestsFailed, mutSplkRequestsFailed);
	STATSCOUNTER_DEF(ctrSplkRequestsCount, mutCtrSplkRequestsCount);
	STATSCOUNTER_DEF(splkRequestsBytes, mutSplkRequestsBytes);
	STATSCOUNTER_DEF(splkRequestsTimeMs, mutSplkRequestsTimeMs);

	struct instanceConf_s *next;
} instanceData;

struct modConfData_s {
	rsconf_t *pConf;		/* our overall config object */
	instanceConf_t *root, *tail;
};

static modConfData_t *loadModConf = NULL;	/* modConf ptr to use for the current load process */

typedef struct wrkrInstanceData {
	PTR_ASSERT_DEF
	instanceData *pData;
	int serverIndex;
	int replyLen;
	char *reply;
	long httpStatusCode;	/* http status code of response */
	CURL	*curlCheckConnHandle;	/* libcurl session handle for checking the server connection */
	CURL	*curlPostHandle;	/* libcurl session handle for posting data to the server */
	HEADER	*curlHeader;	/* json POST request info */
	uchar *restURL;		/* last used URL for error reporting */
	struct {
		uchar **data;		/* array of strings, this will be batched up lazily */
		uchar *restPath;	/* Helper for restpath in batch mode */
		size_t sizeBytes;	/* total length of this batch in bytes */
		size_t nmemb;		/* number of messages in batch (for statistics counting) */

	} batch;
} wrkrInstanceData_t;

static struct cnfparamdescr actpdescr[] = {
	{ "name", eCmdHdlrGetWord, 0 },
	{ "server", eCmdHdlrArray, 0 },
	{ "restpath", eCmdHdlrGetWord, 0 },
	{ "port", eCmdHdlrInt, 0 },
	{ "token", eCmdHdlrGetWord, 0 },
	{ "template", eCmdHdlrGetWord, 0 },
	{ "batch", eCmdHdlrBinary, 0 },
	{ "batch.maxsize", eCmdHdlrInt, 0 },
	{ "batch.maxbyte", eCmdHdlrInt, 0 },
	{ "useTls", eCmdHdlrBinary, 0 },
	{ "allowunsignedcerts", eCmdHdlrBinary, 0 },
	{ "skipverifyhost", eCmdHdlrBinary, 0 },
	{ "tls.cacert", eCmdHdlrGetWord, 0 },
	{ "tls.mycert", eCmdHdlrGetWord, 0 },
	{ "tls.myprivkey", eCmdHdlrGetWord, 0 },
	{ "errorFilename", eCmdHdlrGetWord, 0 },
	{ "reloadonhup", eCmdHdlrBinary, 0 },
};

static struct cnfparamblk actpblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(actpdescr)/sizeof(struct cnfparamdescr),
	  actpdescr
	};

/********************************* CUSTOM FUNCTIONS **************************************************/
/* HEADER */
static rsRetVal buildBatch(wrkrInstanceData_t *pWrkrData, uchar *message);
static rsRetVal submitBatch(wrkrInstanceData_t *pWrkrData, uchar **tp);
static rsRetVal renderJsonErrorMessage(wrkrInstanceData_t *pWrkrData, uchar *reqmsg, char **rendered);
static rsRetVal serializeBatchJsonArray(wrkrInstanceData_t *pWrkrData, char **batchBuf);
static void ATTR_NONNULL() initializeBatch(wrkrInstanceData_t *pWrkrData);
static inline void incrementServerIndex(wrkrInstanceData_t *pWrkrData);
static void ATTR_NONNULL() setInstDefaultParams(instanceData *const pData);
static rsRetVal ATTR_NONNULL() writeDataError(wrkrInstanceData_t *const pWrkrData,
instanceData *const pData, uchar *const reqmsg);
static void ATTR_NONNULL() curlCleanup(wrkrInstanceData_t *const pWrkrData);
static rsRetVal ATTR_NONNULL(1, 2) curlPost(wrkrInstanceData_t *pWrkrData, uchar *message,
int msglen, const int nmsgs __attribute__((unused)));
static void ATTR_NONNULL(1) curlPostMethod(wrkrInstanceData_t *const pWrkrData);
static void ATTR_NONNULL() checkConnSetupCurl(wrkrInstanceData_t *const pWrkrData);
static rsRetVal ATTR_NONNULL() curlSetup(wrkrInstanceData_t *const pWrkrData);
static rsRetVal checkResult(wrkrInstanceData_t *pWrkrData, uchar *reqmsg);
static rsRetVal ATTR_NONNULL(1) setPostURL(wrkrInstanceData_t *const pWrkrData);
static rsRetVal ATTR_NONNULL() generateCurlHeader(wrkrInstanceData_t *pWrkrData);
static size_t curlResponse(void *ptr, size_t size, size_t nmemb, void *userdata);
static void ATTR_NONNULL() curlSetupProxySSL(wrkrInstanceData_t *const pWrkrData, CURL *const handle);
static void ATTR_NONNULL() curlCheckConnSetup(wrkrInstanceData_t *const pWrkrData);
static rsRetVal ATTR_NONNULL() checkConn(wrkrInstanceData_t *const pWrkrData);
static rsRetVal createApiHeader(char* token, uchar** headerBuf);
static rsRetVal createBaseUrl(const char*const serverParam, const int defaultPort,
const sbool useHttps, uchar **baseUrl);


/********************** BATCH *****************************************/

static rsRetVal
buildBatch(wrkrInstanceData_t *pWrkrData, uchar *message)
{
	DEFiRet;

	if (pWrkrData->batch.nmemb >= pWrkrData->pData->batchMaxBatchSize) {
		LogError(0, RS_RET_ERR, "omsplunkhec: buildBatch something has gone wrong,"
			"nBmessages in batch is bigger than the max batch size");
		ABORT_FINALIZE(RS_RET_ERR);
	}
	pWrkrData->batch.data[pWrkrData->batch.nmemb] = message;
	pWrkrData->batch.sizeBytes += strlen((char *)message);
	pWrkrData->batch.nmemb++;

finalize_it:
	RETiRet;
}

static rsRetVal
submitBatch(wrkrInstanceData_t *pWrkrData, uchar **tp)
{
	DEFiRet;
	char *batchBuf = NULL;

	/* Serialize the data into JSON */
	iRet = serializeBatchJsonArray(pWrkrData, &batchBuf);

	if (iRet != RS_RET_OK || batchBuf == NULL)
		ABORT_FINALIZE(iRet);

	DBGPRINTF("omsplunkhec: submitBatch, batch: '%s' tpls: '%p'\n", batchBuf, tp);

	CHKiRet(curlPost(pWrkrData, (uchar*) batchBuf, strlen(batchBuf), pWrkrData->batch.nmemb));

finalize_it:
	if (batchBuf != NULL)
		free(batchBuf);
	RETiRet;
}
/********************** END BATCH *****************************************/

/********************** JSON *****************************************/
static rsRetVal
renderJsonErrorMessage(wrkrInstanceData_t *pWrkrData, uchar *reqmsg, char **rendered)
{
	DEFiRet;
	fjson_object *req = NULL;
	fjson_object *res = NULL;
	fjson_object *errRoot = NULL;

	if ((req = fjson_object_new_object()) == NULL)
		ABORT_FINALIZE(RS_RET_ERR);
	fjson_object_object_add(req, "url", fjson_object_new_string((char *)pWrkrData->restURL));
	fjson_object_object_add(req, "postdata", fjson_object_new_string((char *)reqmsg));

	if ((res = fjson_object_new_object()) == NULL) {
		fjson_object_put(req); // cleanup request object
		ABORT_FINALIZE(RS_RET_ERR);
	}

	fjson_object_object_add(res, "status", fjson_object_new_int(pWrkrData->httpStatusCode));
	if (pWrkrData->reply == NULL) {
		fjson_object_object_add(res, "message",
			fjson_object_new_string_len(ERR_MSG_NULL, strlen(ERR_MSG_NULL)));
	} else {
		fjson_object_object_add(res, "message",
			fjson_object_new_string_len(pWrkrData->reply, pWrkrData->replyLen));
	}

	if ((errRoot = fjson_object_new_object()) == NULL) {
		fjson_object_put(req); // cleanup request object
		fjson_object_put(res); // cleanup response object
		ABORT_FINALIZE(RS_RET_ERR);
	}

	fjson_object_object_add(errRoot, "request", req);
	fjson_object_object_add(errRoot, "response", res);

	*rendered = strdup((char *) fjson_object_to_json_string(errRoot));

finalize_it:
	if (errRoot != NULL)
		fjson_object_put(errRoot);

	RETiRet;
}

static rsRetVal
serializeBatchJsonArray(wrkrInstanceData_t *pWrkrData, char **batchBuf)
{
	fjson_object *batchArray = NULL;
	fjson_object *msgObj = NULL;
	size_t numMessages = pWrkrData->batch.nmemb;
	size_t sizeTotal = pWrkrData->batch.sizeBytes + numMessages + 1;
	DBGPRINTF("omsplunkhec: serializeBatchJsonArray numMessages=%zd sizeTotal=%zd\n", numMessages, sizeTotal);

	DEFiRet;

	batchArray = fjson_object_new_array();
	if (batchArray == NULL) {
		LogError(0, RS_RET_ERR, "omsplunkhec: serializeBatchJsonArray failed to create array");
		ABORT_FINALIZE(RS_RET_ERR);
	}

	for (size_t i = 0; i < numMessages; i++) {
		msgObj = fjson_tokener_parse((char *) pWrkrData->batch.data[i]);
		if (msgObj == NULL) {
			LogError(0, NO_ERRCODE,
				"omsplunkhec: serializeBatchJsonArray failed to parse %s as json, ignoring it",
				pWrkrData->batch.data[i]);
			continue;
		}
		fjson_object_array_add(batchArray, msgObj);
	}

	const char *batchString = fjson_object_to_json_string_ext(batchArray, FJSON_TO_STRING_PLAIN);
	*batchBuf = strndup(batchString, strlen(batchString));

finalize_it:
	if (batchArray != NULL) {
		fjson_object_put(batchArray);
		batchArray = NULL;
	}
	RETiRet;
}
/********************** END JSON *****************************************/

/********************** INIT *****************************************/
static void ATTR_NONNULL()
initializeBatch(wrkrInstanceData_t *pWrkrData)
{
	pWrkrData->batch.sizeBytes = 0;
	pWrkrData->batch.nmemb = 0;
	if (pWrkrData->batch.restPath != NULL)  {
		free(pWrkrData->batch.restPath);
		pWrkrData->batch.restPath = NULL;
	}
}

static inline void
incrementServerIndex(wrkrInstanceData_t *pWrkrData)
{
	pWrkrData->serverIndex = (pWrkrData->serverIndex + 1) % pWrkrData->pData->numListServerName;
}

// Function used to set default value
static void ATTR_NONNULL()
setInstDefaultParams(instanceData *const pData)
{
	pData->serverList = NULL;
	pData->token = NULL;
	pData->template = NULL;
	pData->restpath = NULL;
	pData->restPathTimeout = 3600;
	pData->port = 8088;

	pData->bufHeaderContentType = NULL;
	
	pData->batch = 0;
	pData->batchMaxBatchBytes = 10485760; // around 10 MB
	pData->batchMaxBatchSize = 100; // 100 messages

	pData->proxyHost = NULL;
	pData->proxyPort = 0;

	pData->useTLS = 0;
	pData->allowUnsignedCerts = 0;
	pData->skipVerifyHost = 0;
	pData->caCertFile = NULL;
	pData->myCertFile = NULL;
	pData->myPrivKeyFile = NULL;

	pData->errorFilename = NULL;
	pData->fdErrorfile = -1;

	pData->reloadOnHup= 0;

	// increment number of instances
	++omSplunkHecInstancesCnt;
}

/********************** END INIT *****************************************/

/********************** CURL *****************************************/

static rsRetVal ATTR_NONNULL()
writeDataError(wrkrInstanceData_t *const pWrkrData,
	instanceData *const pData, uchar *const reqmsg)
{
	char *rendered = NULL;
	size_t toWrite;
	ssize_t wrRet;
	sbool bMutLocked = 0;

	DEFiRet;

	if(pData->errorFilename == NULL) {
		DBGPRINTF("omsplunkhec: no local error logger defined - "
			"ignoring REST error information\n");
		FINALIZE;
	}

	pthread_mutex_lock(&pData->mutErrFile);
	bMutLocked = 1;

	CHKiRet(renderJsonErrorMessage(pWrkrData, reqmsg, &rendered));

	if(pData->fdErrorfile == -1) {
		pData->fdErrorfile = open((char*)pData->errorFilename,
					O_WRONLY|O_CREAT|O_APPEND|O_LARGEFILE|O_CLOEXEC,
					S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
		if(pData->fdErrorfile == -1) {
			LogError(errno, RS_RET_ERR, "omsplunkhec: error opening error file %s",
				pData->errorFilename);
			ABORT_FINALIZE(RS_RET_ERR);
		}
	}

	DBGPRINTF("omsplunkhec: error record: '%s'\n", rendered);
	toWrite = strlen(rendered) + 1;

	rendered[toWrite-1] = '\n';
	wrRet = write(pData->fdErrorfile, rendered, toWrite);
	if(wrRet != (ssize_t) toWrite) {
		LogError(errno, RS_RET_IO_ERROR,
			"omsplunkhec: error writing error file %s, write returned %lld",
			pData->errorFilename, (long long) wrRet);
	}

finalize_it:
	if(bMutLocked)
		pthread_mutex_unlock(&pData->mutErrFile);
	free(rendered);
	RETiRet;
}

static void ATTR_NONNULL()
curlCleanup(wrkrInstanceData_t *const pWrkrData)
{
	if (pWrkrData->curlHeader != NULL) {
		curl_slist_free_all(pWrkrData->curlHeader);
		pWrkrData->curlHeader = NULL;
	}
	if (pWrkrData->curlCheckConnHandle != NULL) {
		curl_easy_cleanup(pWrkrData->curlCheckConnHandle);
		pWrkrData->curlCheckConnHandle = NULL;
	}
	if (pWrkrData->curlPostHandle != NULL) {
		curl_easy_cleanup(pWrkrData->curlPostHandle);
		pWrkrData->curlPostHandle = NULL;
	}
}

static rsRetVal ATTR_NONNULL(1, 2)
curlPost(wrkrInstanceData_t *pWrkrData, uchar *message, int msglen,
		const int nmsgs __attribute__((unused)))
{
	CURLcode curlCode;
	CURL *const curl = pWrkrData->curlPostHandle;
	char errbuf[CURL_ERROR_SIZE] = "";

	char *postData;
	int postLen;
	DEFiRet;

	PTR_ASSERT_SET_TYPE(pWrkrData, WRKR_DATA_TYPE_ES);

	// Check what server is available
	if(pWrkrData->pData->numListServerName > 1) {
		CHKiRet(checkConn(pWrkrData));
	}
	CHKiRet(setPostURL(pWrkrData));

	pWrkrData->reply = NULL;
	pWrkrData->replyLen = 0;
	pWrkrData->httpStatusCode = 0;

	postData = (char *)message;
	postLen = msglen;

	generateCurlHeader(pWrkrData);

	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, postLen);
	curl_easy_setopt(pWrkrData->curlPostHandle, CURLOPT_HTTPHEADER, pWrkrData->curlHeader);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

	curlCode = curl_easy_perform(curl);
	DBGPRINTF("omsplunkhec: curlPost curl returned %lld\n", (long long) curlCode);
	STATSCOUNTER_INC(ctrSplkRequestsCount, mutCtrSplkRequestsCount);
	STATSCOUNTER_INC(pWrkrData->pData->ctrSplkRequestsCount, pWrkrData->pData->mutCtrSplkRequestsCount);

	if (curlCode != CURLE_OK) {
		LogError(0, RS_RET_SUSPENDED,
			"omsplunkhec: suspending ourselves due to server failure %lld: %s",
			(long long) curlCode, errbuf);
		STATSCOUNTER_INC(splkRequestsFailed, mutSplkRequestsFailed);
		checkResult(pWrkrData, message);
		ABORT_FINALIZE(RS_RET_SUSPENDED);
	} else {
		STATSCOUNTER_INC(splkRequestsSuccess, mutSplkRequestsSuccess);
	}

	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &pWrkrData->httpStatusCode);
	if(pWrkrData->reply == NULL) {
		DBGPRINTF("omsplunkhec: curlPost pWrkrData reply==NULL, replyLen = '%d'\n",
			pWrkrData->replyLen);
	} else {
		DBGPRINTF("omsplunkhec: curlPost pWrkrData replyLen = '%d'\n", pWrkrData->replyLen);
		if(pWrkrData->replyLen > 0) {
			pWrkrData->reply[pWrkrData->replyLen] = '\0';
			/* Append 0 Byte if replyLen is above 0 - byte has been reserved in malloc */
		}
		//TODO: replyLen++? because 0 Byte is appended
		DBGPRINTF("omsplunkhec: curlPost pWrkrData reply: '%s'\n", pWrkrData->reply);
	}
	CHKiRet(checkResult(pWrkrData, message));

finalize_it:
	incrementServerIndex(pWrkrData);
	if (pWrkrData->reply != NULL) {
		free(pWrkrData->reply);
		pWrkrData->reply = NULL; /* don't leave dangling pointer */
	}
	RETiRet;
}

static void ATTR_NONNULL(1)
curlPostMethod(wrkrInstanceData_t *const pWrkrData)
{
	PTR_ASSERT_SET_TYPE(pWrkrData, WRKR_DATA_TYPE_ES);
	curlSetupProxySSL(pWrkrData, pWrkrData->curlPostHandle);
	curl_easy_setopt(pWrkrData->curlPostHandle, CURLOPT_POST, 1);
	CURLcode cRet;
	/* Enable TCP keep-alive for this transfer */
	cRet = curl_easy_setopt(pWrkrData->curlPostHandle, CURLOPT_TCP_KEEPALIVE, 1L);
	if (cRet != CURLE_OK)
		DBGPRINTF("omsplunkhec: curlPostMethod unknown option CURLOPT_TCP_KEEPALIVE\n");
	/* keep-alive idle time to 120 seconds */
	cRet = curl_easy_setopt(pWrkrData->curlPostHandle, CURLOPT_TCP_KEEPIDLE, 120L);
	if (cRet != CURLE_OK)
		DBGPRINTF("omsplunkhec: curlPostMethod unknown option CURLOPT_TCP_KEEPIDLE\n");
	/* interval time between keep-alive probes: 60 seconds */
	cRet = curl_easy_setopt(pWrkrData->curlPostHandle, CURLOPT_TCP_KEEPINTVL, 60L);
	if (cRet != CURLE_OK)
		DBGPRINTF("omsplunkhec: curlPostMethod unknown option CURLOPT_TCP_KEEPINTVL\n");
}

static void ATTR_NONNULL()
checkConnSetupCurl(wrkrInstanceData_t *const pWrkrData)
{
	PTR_ASSERT_SET_TYPE(pWrkrData, WRKR_DATA_TYPE_ES);
	curlSetupProxySSL(pWrkrData, pWrkrData->curlCheckConnHandle);
	curl_easy_setopt(pWrkrData->curlCheckConnHandle,
		CURLOPT_TIMEOUT_MS, pWrkrData->pData->restPathTimeout);
}

static rsRetVal ATTR_NONNULL()
curlSetup(wrkrInstanceData_t *const pWrkrData)
{
	struct curl_slist *slist = NULL;

	DEFiRet;

	if (pWrkrData->pData->bufHeaderContentType != NULL) {
		slist = curl_slist_append(slist, (char *)pWrkrData->pData->bufHeaderContentType);
		CHKmalloc(slist);
	}

	/*
	 * Thanks OMHTTP for this "bug"
	 * When sending more than 1Kb, libcurl automatically sends an Except: 100-Continue header
	 * and will wait 1s for a response, could make this configurable but for now disable
	 */
	slist = curl_slist_append(slist, HTTP_HEADER_EXPECT_EMPTY);
	pWrkrData->curlHeader = slist;
	CHKmalloc(pWrkrData->curlPostHandle = curl_easy_init());
	curlPostMethod(pWrkrData);

	CHKmalloc(pWrkrData->curlCheckConnHandle = curl_easy_init());
	checkConnSetupCurl(pWrkrData);

finalize_it:
	if(iRet != RS_RET_OK && pWrkrData->curlPostHandle != NULL) {
		curl_easy_cleanup(pWrkrData->curlPostHandle);
		pWrkrData->curlPostHandle = NULL;
	}
	RETiRet;
}

static rsRetVal
checkResult(wrkrInstanceData_t *pWrkrData, uchar *reqmsg)
{
	long statusCode;
	DEFiRet;
	int numMessages;
	CURLcode resCurl = 0;

	statusCode = pWrkrData->httpStatusCode;

	if (pWrkrData->pData->batch) {
		numMessages = pWrkrData->batch.nmemb;
	} else {
		numMessages = 1;
	}

	if (statusCode == 0) {
		STATSCOUNTER_ADD(splkRequestsFailed, mutCtrMessagesFail, numMessages);
		// request failed, suspend or retry
		iRet = RS_RET_SUSPENDED;
	} else if (statusCode >= 500) {
		STATSCOUNTER_ADD(splkRequestsFailed, mutCtrMessagesFail, numMessages);
		// server error, suspend or retry
		iRet = RS_RET_SUSPENDED;
	} else if (statusCode >= 300) {
		STATSCOUNTER_ADD(splkRequestsFailed, mutCtrMessagesFail, numMessages);
		// redirection or client error, NO suspend nor retry
		iRet = RS_RET_SUSPENDED;
	} else {
		// success, normal state
		STATSCOUNTER_INC(splkRequestsSuccess, mutCtrMessagesSuccess);
		STATSCOUNTER_ADD(splkRequestsSuccess, mutCtrMessagesSuccess, numMessages);
		iRet = RS_RET_OK;
	}

	long req = 0;
	double total = 0;
	/* record total bytes */
	resCurl = curl_easy_getinfo(pWrkrData->curlPostHandle, CURLINFO_REQUEST_SIZE, &req);
	if (!resCurl) {
		STATSCOUNTER_ADD(pWrkrData->pData->splkRequestsBytes,
			pWrkrData->pData->mutSplkRequestsBytes,
			(uint64_t)req);
	}
	resCurl = curl_easy_getinfo(pWrkrData->curlPostHandle, CURLINFO_TOTAL_TIME, &total);
	if(CURLE_OK == resCurl) {
		long total_time_ms = (long)(total*1000);
		STATSCOUNTER_ADD(pWrkrData->pData->splkRequestsTimeMs,
			pWrkrData->pData->mutSplkRequestsTimeMs,
			(uint64_t)total_time_ms);
	}

	if (iRet != RS_RET_OK) {
		LogMsg(0, iRet, LOG_ERR, "omsplunkhec: checkResult error http status code: %ld reply: %s",
			statusCode, pWrkrData->reply != NULL ? pWrkrData->reply : "NULL");

		writeDataError(pWrkrData, pWrkrData->pData, reqmsg);

		if (iRet == RS_RET_DATAFAIL)
			ABORT_FINALIZE(iRet);
	}

finalize_it:
	RETiRet;
}

static rsRetVal ATTR_NONNULL(1)
setPostURL(wrkrInstanceData_t *const pWrkrData)
{
	uchar *restPath;
	char* baseUrl;
	es_str_t *url;
	int answer;
	DEFiRet;
	instanceData *const pData = pWrkrData->pData;

	baseUrl = (char*)pData->serverList[pWrkrData->serverIndex];
	url = es_newStrFromCStr(baseUrl, strlen(baseUrl));
	if (url == NULL) {
		LogError(0, RS_RET_OUT_OF_MEMORY,
			"omsplunkhec: error allocating new estr for POST url.");
		ABORT_FINALIZE(RS_RET_ERR);
	}
	
	restPath = pData->restpath;

	answer = 0;
	if (restPath != NULL)
		answer = es_addBuf(&url, (char*)restPath, ustrlen(restPath));
	if(answer != 0) {
		LogError(0, RS_RET_ERR, "omsplunkhec: failure in creating restURL, "
				"error code: %d", answer);
		ABORT_FINALIZE(RS_RET_ERR);
	}

	if(pWrkrData->restURL != NULL)
		free(pWrkrData->restURL);

	pWrkrData->restURL = (uchar*)es_str2cstr(url, NULL);
	curl_easy_setopt(pWrkrData->curlPostHandle, CURLOPT_URL, pWrkrData->restURL);
	DBGPRINTF("omsplunkhec: using REST URL: '%s'\n", pWrkrData->restURL);

finalize_it:
	if (url != NULL)
		es_deleteStr(url);
	RETiRet;
}

static rsRetVal ATTR_NONNULL()
generateCurlHeader(wrkrInstanceData_t *pWrkrData)
{
	struct curl_slist *slist = NULL;

	DEFiRet;

	slist = curl_slist_append(slist, (char *)pWrkrData->pData->bufHeaderContentType);

	CHKmalloc(slist);

	// When sending more than 1Kb, libcurl automatically sends an Except: 100-Continue header
	// and will wait 1s for a response, could make this configurable but for now disable
	slist = curl_slist_append(slist, HTTP_HEADER_EXPECT_EMPTY);
	CHKmalloc(slist);

	if (pWrkrData->curlHeader != NULL)
		curl_slist_free_all(pWrkrData->curlHeader);

	pWrkrData->curlHeader = slist;

finalize_it:
	if (iRet != RS_RET_OK) {
		curl_slist_free_all(slist);
		LogError(0, iRet, "omsplunkhec: error allocating curl header slist, using previous one");
	}
	RETiRet;
}

static size_t
curlResponse(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	char *p = (char *)ptr;
	wrkrInstanceData_t *pWrkrData = (wrkrInstanceData_t*) userdata;
	char *buf;
	size_t newlen;
	PTR_ASSERT_CHK(pWrkrData, WRKR_DATA_TYPE_ES);
	newlen = pWrkrData->replyLen + size*nmemb;
	if((buf = realloc(pWrkrData->reply, newlen + 1)) == NULL) {
		LogError(errno, RS_RET_ERR, "omsplunkhec: realloc failed in curlResponse");
		return 0;
	}
	memcpy(buf+pWrkrData->replyLen, p, size*nmemb);
	pWrkrData->replyLen = newlen;
	pWrkrData->reply = buf;
	return size*nmemb;
}

static void ATTR_NONNULL()
curlSetupProxySSL(wrkrInstanceData_t *const pWrkrData, CURL *const handle)
{
	PTR_ASSERT_SET_TYPE(pWrkrData, WRKR_DATA_TYPE_ES);
	/* Debug CURL ON/OFF
	curl_easy_setopt(handle, CURLOPT_VERBOSE, TRUE); */
	
	curl_easy_setopt(handle, CURLOPT_HTTPHEADER, pWrkrData->curlHeader);
	curl_easy_setopt(handle, CURLOPT_NOSIGNAL, TRUE);
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, curlResponse);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, pWrkrData);
	if (pWrkrData->pData->proxyHost != NULL) {
		curl_easy_setopt(handle, CURLOPT_PROXY, pWrkrData->pData->proxyHost);
	}
	if (pWrkrData->pData->proxyPort != 0) {
		curl_easy_setopt(handle, CURLOPT_PROXYPORT, pWrkrData->pData->proxyPort);
	}
	if (pWrkrData->pData->restPathTimeout) {
		curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, pWrkrData->pData->restPathTimeout);
	}
	if(pWrkrData->pData->allowUnsignedCerts)
		curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, FALSE);
	if(pWrkrData->pData->skipVerifyHost)
		curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, FALSE);
	if(pWrkrData->pData->caCertFile)
		curl_easy_setopt(handle, CURLOPT_CAINFO, pWrkrData->pData->caCertFile);
	if(pWrkrData->pData->myCertFile)
		curl_easy_setopt(handle, CURLOPT_SSLCERT, pWrkrData->pData->myCertFile);
	if(pWrkrData->pData->myPrivKeyFile)
		curl_easy_setopt(handle, CURLOPT_SSLKEY, pWrkrData->pData->myPrivKeyFile);

}

static void ATTR_NONNULL()
curlCheckConnSetup(wrkrInstanceData_t *const pWrkrData)
{
	PTR_ASSERT_SET_TYPE(pWrkrData, WRKR_DATA_TYPE_ES);
	curlSetupProxySSL(pWrkrData, pWrkrData->curlCheckConnHandle);
	curl_easy_setopt(pWrkrData->curlCheckConnHandle,
		CURLOPT_TIMEOUT_MS, pWrkrData->pData->restPathTimeout);
}

static rsRetVal ATTR_NONNULL()
checkConn(wrkrInstanceData_t *const pWrkrData)
{
	CURL *curl;
	CURLcode res;
	char* serverUrl;
	char* checkPath;
	es_str_t *urlBuf = NULL;
	char* healthUrl;
	int i;
	int answer;
	DEFiRet;

	if (pWrkrData->pData->restpath == NULL) {
		DBGPRINTF("omsplunkhec: checkConn no health check uri configured skipping it\n");
		FINALIZE;
	}

	pWrkrData->reply = NULL;
	pWrkrData->replyLen = 0;
	curl = pWrkrData->curlCheckConnHandle;
	urlBuf = es_newStr(256);
	if (urlBuf == NULL) {
		LogError(0, RS_RET_OUT_OF_MEMORY,
			"omsplunkhec: unable to allocate buffer for health check uri.");
		ABORT_FINALIZE(RS_RET_SUSPENDED);
	}

	for(i = 0; i < pWrkrData->pData->numListServerName; ++i) {
		serverUrl = (char*) pWrkrData->pData->serverList[pWrkrData->serverIndex];
		checkPath = (char*) pWrkrData->pData->restpath;


		es_emptyStr(urlBuf);
		answer = es_addBuf(&urlBuf, serverUrl, strlen(serverUrl));
		if(answer == 0 && checkPath != NULL)
			answer = es_addBuf(&urlBuf, checkPath, strlen(checkPath));
		if(answer == 0)
			healthUrl = es_str2cstr(urlBuf, NULL);
		if(answer != 0 || healthUrl == NULL) {
			LogError(0, RS_RET_OUT_OF_MEMORY,
				"omsplunkhec: unable to allocate buffer for health check uri.");
			ABORT_FINALIZE(RS_RET_SUSPENDED);
		}

		curlCheckConnSetup(pWrkrData);
		curl_easy_setopt(curl, CURLOPT_URL, healthUrl);
		res = curl_easy_perform(curl);
		free(healthUrl);

		if (res == CURLE_OK) {
			DBGPRINTF("omsplunkhec: checkConn %s completed with success "
				"on attempt %d\n", healthUrl, i);
			ABORT_FINALIZE(RS_RET_OK);
		}

		DBGPRINTF("omsplunkhec: checkConn %s failed on attempt %d: %s\n",
			healthUrl, i, curl_easy_strerror(res));
		incrementServerIndex(pWrkrData);
	}

	LogMsg(0, RS_RET_SUSPENDED, LOG_WARNING,
		"omsplunkhec: checkConn failed after %d attempts.", i);
	ABORT_FINALIZE(RS_RET_SUSPENDED);

finalize_it:
	if(urlBuf != NULL)
		es_deleteStr(urlBuf);

	free(pWrkrData->reply);
	pWrkrData->reply = NULL; /* don't leave dangling pointer */
	RETiRet;
}


/********************** END CURL *****************************************/

/********************** TOOLS *****************************************/
static rsRetVal
createApiHeader(char* token, uchar** headerBuf)
{
	int answer;
	DEFiRet;

	es_str_t* header = es_newStr(20480);
	if (header == NULL) {
		LogError(0, RS_RET_OUT_OF_MEMORY,
		"omsplunkhec: failed to allocate es_str api header");
		ABORT_FINALIZE(RS_RET_ERR);
	}

	answer = es_addBuf(&header, (char *) HTTP_HEADER_CONTENT_JSON,
						strlen( (char *) HTTP_HEADER_CONTENT_JSON));
	if(answer == 0) answer = es_addChar(&header, ',');
	if(answer == 0) answer = es_addChar(&header, '\n');
	if(answer == 0) {
		answer = es_addBuf(&header, (char *) HTTP_HEADER_AUTHORIZATION,
							strlen((char *) HTTP_HEADER_AUTHORIZATION));
	}
	if(answer == 0 && token != NULL) answer = es_addBuf(&header, token, strlen(token));
	if(answer == 0) *headerBuf = (uchar*) es_str2cstr(header, NULL);

	if (answer != 0 || *headerBuf == NULL) {
		LogError(0, RS_RET_ERR, "omsplunkhec: failed to build http header\n");
		ABORT_FINALIZE(RS_RET_ERR);
	}

finalize_it:
	if (header != NULL)
		es_deleteStr(header);
	RETiRet;
}

static rsRetVal
createBaseUrl(const char*const serverParam,
	const int defaultPort,
	const sbool useHttps,
	uchar **baseUrl)
{
	char portBuf[64];
	int answer = 0;
	const char *host = serverParam;
	DEFiRet;

	/* check if dns name does not end with "/" */
	assert(serverParam[strlen(serverParam)-1] != '/');

	es_str_t *urlBuf = es_newStr(256);
	if (urlBuf == NULL) {
		LogError(0, RS_RET_OUT_OF_MEMORY,
		"omsplunkhec: failed to allocate es_str urlBuf in createBaseUrl");
		ABORT_FINALIZE(RS_RET_ERR);
	}

	if (strcasestr(serverParam, HTTP))
		host = serverParam + strlen(HTTP);
	else if (strcasestr(serverParam, HTTPS))
		host = serverParam + strlen(HTTPS);
	else {
		if (useHttps){
			answer = es_addBuf(&urlBuf, HTTPS, sizeof(HTTPS)-1);
		} else {
			answer = es_addBuf(&urlBuf, HTTP, sizeof(HTTP)-1);
		}
	}

	if (answer == 0) answer = es_addBuf(&urlBuf, (char *)serverParam, strlen(serverParam));
	if (answer == 0 && !strchr(host, ':')) {
		snprintf(portBuf, sizeof(portBuf), ":%d", defaultPort);
		answer = es_addBuf(&urlBuf, portBuf, strlen(portBuf));
	}
	if (answer == 0) answer = es_addChar(&urlBuf, '/');
	if (answer == 0) *baseUrl = (uchar*) es_str2cstr(urlBuf, NULL);

	if (answer != 0 || baseUrl == NULL) {
		LogError(0, RS_RET_ERR,
			"omsplunkhec: error occurred computing baseUrl from server %s", serverParam);
		ABORT_FINALIZE(RS_RET_ERR);
	}
finalize_it:
	if (urlBuf) {
		es_deleteStr(urlBuf);
	}
	RETiRet;
}

/********************** END TOOLS *****************************************/

/*****************************************************************************************************************/

BEGINbeginCnfLoad
CODESTARTbeginCnfLoad
	loadModConf = pModConf;
	pModConf->pConf = pConf;
	pModConf->root = pModConf->tail = NULL;
ENDbeginCnfLoad

BEGINendCnfLoad
CODESTARTendCnfLoad
	loadModConf = NULL;
ENDendCnfLoad

BEGINcheckCnf
CODESTARTcheckCnf
ENDcheckCnf

BEGINactivateCnf
CODESTARTactivateCnf
ENDactivateCnf

BEGINfreeCnf
CODESTARTfreeCnf
ENDfreeCnf


BEGINcreateInstance
	int answer;
CODESTARTcreateInstance
	/* Error FD */
	pData->fdErrorfile = -1;

	if((answer = pthread_mutex_init(&pData->mutErrFile, NULL)) != 0) {
		LogError(answer, RS_RET_ERR, "omsplunkhec: cannot create "
			"error file mutex, failing this action");
		ABORT_FINALIZE(RS_RET_ERR);
	}
	/* TLS */
	pData->caCertFile = NULL;
	pData->myCertFile = NULL;
	pData->myPrivKeyFile = NULL;
finalize_it:
ENDcreateInstance


BEGINcreateWrkrInstance
	uchar **batchData;
CODESTARTcreateWrkrInstance
	PTR_ASSERT_SET_TYPE(pWrkrData, WRKR_DATA_TYPE_ES);
	pWrkrData->curlHeader = NULL;
	pWrkrData->curlPostHandle = NULL;
	pWrkrData->curlCheckConnHandle = NULL;

	pWrkrData->serverIndex = 0;
	pWrkrData->httpStatusCode = 0;
	pWrkrData->restURL = NULL;
	if(pData->batch) {
		pWrkrData->batch.nmemb = 0;
		pWrkrData->batch.sizeBytes = 0;
		batchData = (uchar **) malloc(pData->batchMaxBatchSize * sizeof(uchar *));
		if (batchData == NULL) {
			LogError(0, RS_RET_OUT_OF_MEMORY,
				"omsplunkhec: cannot allocate memory for batch queue turning off batch mode\n");
			pData->batch = 0;
		} else {
			pWrkrData->batch.data = batchData;
			pWrkrData->batch.restPath = NULL;
		}
	}
	iRet = curlSetup(pWrkrData);
ENDcreateWrkrInstance


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURERepeatedMsgReduction)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINfreeInstance
	int i;
CODESTARTfreeInstance
	pthread_mutex_destroy(&pData->mutErrFile);
	free(pData->token);
	free(pData->template);
	free(pData->restpath);
	for(i = 0 ; i < pData->numListServerName ; ++i)
		free(pData->serverList[i]);
	free(pData->serverList);
	free(pData->bufHeaderContentType);
	free(pData->proxyHost);
	free(pData->caCertFile);
	free(pData->myCertFile);
	free(pData->myPrivKeyFile);
	if (pData->stats) {
		statsobj.Destruct(&pData->stats);
	}
	free(pData->statsName);
ENDfreeInstance


BEGINfreeWrkrInstance
CODESTARTfreeWrkrInstance
	curlCleanup(pWrkrData);

	free(pWrkrData->restURL);
	pWrkrData->restURL = NULL;

	free(pWrkrData->batch.data);
	pWrkrData->batch.data = NULL;

	if (pWrkrData->batch.restPath != NULL)  {
		free(pWrkrData->batch.restPath);
		pWrkrData->batch.restPath = NULL;
	}
ENDfreeWrkrInstance



BEGINnewActInst
	struct cnfparamvals *pvals;
	char* serverParam = NULL;
	struct cnfarray* servers = NULL;
	int i;
	int iNumTpls;
	FILE *fp;
	char errStr[1024];
CODESTARTnewActInst
	if((pvals = nvlstGetParams(lst, &actpblk, NULL)) == NULL) {
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	CHKiRet(createInstance(&pData));
	// Set default value
	setInstDefaultParams(pData);

	for(i = 0 ; i < actpblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;

		if(!strcmp(actpblk.descr[i].name, "name")) {
			pData->statsName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "errorFilename")) {
			pData->errorFilename = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "server")) {
			servers = pvals[i].val.d.ar;
		} else if(!strcmp(actpblk.descr[i].name, "restpath")) {
			pData->restpath = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "port")) {
			pData->port = (int) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "token")) {
			pData->token = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "template")) {
			pData->template = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "batch")) {
			pData->batch = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "batch.maxbytes")) {
			pData->batchMaxBatchBytes = (size_t) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "batch.maxsize")) {
			pData->batchMaxBatchSize = (size_t) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "useTls")) {
			pData->useTLS = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "allowunsignedcerts")) {
			pData->allowUnsignedCerts = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "skipverifyhost")) {
			pData->skipVerifyHost = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "reloadonhup")) {
			pData->reloadOnHup = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "tls.cacert")) {
			pData->caCertFile = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
			fp = fopen((const char*)pData->caCertFile, "r");
			if(fp == NULL) {
				rs_strerror_r(errno, errStr, sizeof(errStr));
				LogError(0, RS_RET_NO_FILE_ACCESS,
					"error: 'tls.cacert' file %s couldn't be accessed or existed: %s\n",
					pData->caCertFile, errStr);
			} else {
				fclose(fp);
			}
		// Checking access to file each time we need
		} else if(!strcmp(actpblk.descr[i].name, "tls.mycert")) {
			pData->myCertFile = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
			fp = fopen((const char*)pData->myCertFile, "r");
			if(fp == NULL) {
				rs_strerror_r(errno, errStr, sizeof(errStr));
				LogError(0, RS_RET_NO_FILE_ACCESS,
					"error: 'tls.mycert' file %s couldn't be accessed or existed: %s\n",
					pData->myCertFile, errStr);
			} else {
				fclose(fp);
			}
		// Checking access to file each time we need
		} else if(!strcmp(actpblk.descr[i].name, "tls.myprivkey")) {
			pData->myPrivKeyFile = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
			fp = fopen((const char*)pData->myPrivKeyFile, "r");
			if(fp == NULL) {
				rs_strerror_r(errno, errStr, sizeof(errStr));
				LogError(0, RS_RET_NO_FILE_ACCESS,
					"error: 'tls.myprivkey' file %s couldn't be accessed or existed: %s\n",
					pData->myPrivKeyFile, errStr);
			} else {
				fclose(fp);
			}
		}
	}

	/* Checking if token is set */
	if(pData->token == NULL) {
		LogError(0, RS_RET_UID_MISSING,
			"omsplunkhec: token must be provided"
			" - action definition invalid");
		ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
	}

	/* Checking if restpath is set */
	if(pData->restpath == NULL) {
		LogError(0, RS_RET_UID_MISSING,
			"omsplunkhec: a rest path must be provided"
			" - action definition invalid");
		ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
	}

	if (pData->proxyHost == NULL) {
		/* Get env proxy information */
		const char *http_proxy;
		if((http_proxy = getenv("http_proxy")) == NULL) {
			http_proxy = getenv("HTTP_PROXY");
		}
		if(http_proxy != NULL) {
			pData->proxyHost = ustrdup(http_proxy);
		}
	}
	if (pData->template == NULL){
		iNumTpls = 1;
		CODE_STD_STRING_REQUESTnewActInst(iNumTpls)
		CHKiRet(OMSRsetEntry(*ppOMSR, 0,(uchar*) strdup(DEFAULTJSONTEMPLATE), OMSR_NO_RQD_TPL_OPTS));
	} else {
		iNumTpls = 1;
		CODE_STD_STRING_REQUESTnewActInst(iNumTpls)
		CHKiRet(OMSRsetEntry(*ppOMSR, 0, (uchar*)strdup((char*)pData->template), OMSR_NO_RQD_TPL_OPTS));
	}

	/* Checking if server is set */
	if (servers != NULL) {
		pData->numListServerName = servers->nmemb;
		pData->serverList = malloc(servers->nmemb * sizeof(uchar*));
		if (pData->serverList == NULL) {
			LogError(0, RS_RET_ERR, "omsplunkhec: unable to allocate buffer "
					"for http server configuration.");
			ABORT_FINALIZE(RS_RET_ERR);
		}

		for(i = 0 ; i < servers->nmemb ; ++i) {
			serverParam = es_str2cstr(servers->arr[i], NULL);
			if (serverParam == NULL) {
				LogError(0, RS_RET_ERR, "omsplunkhec: unable to allocate buffer "
					"for http server configuration.");
				ABORT_FINALIZE(RS_RET_ERR);
			}
			/* Remove a trailing slash if it exists */
			const size_t serverParamLastChar = strlen(serverParam)-1;
			if (serverParam[serverParamLastChar] == '/') {
				serverParam[serverParamLastChar] = '\0';
			}
			CHKiRet(createBaseUrl(serverParam, pData->port, pData->useTLS,
				pData->serverList + i));
			free(serverParam);
			serverParam = NULL;
		}
	} else {
		LogError(0, RS_RET_UID_MISSING,
			"omsplunkhec: one server must be provided"
			"- action definition invalid");
		ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
	}

	if(!pData->statsName) {
		uchar pszAName[64];
		snprintf((char*) pszAName, sizeof(pszAName), "omsplunkhec-%d", omSplunkHecInstancesCnt);
		pData->statsName = ustrdup(pszAName);
	}

	/* Creation header buf (Same header for each server) */
	CHKiRet(createApiHeader((char*) pData->token, &pData->bufHeaderContentType));

	/* Stats object */
	CHKiRet(statsobj.Construct(&pData->stats));
	CHKiRet(statsobj.SetName(pData->stats, (uchar *)pData->statsName));
	CHKiRet(statsobj.SetOrigin(pData->stats, (uchar *)"omsplunkhec"));

	STATSCOUNTER_INIT(pData->ctrSplkRequestsSubmitted, pData->mutCtrSplkRequestsSubmitted);
	CHKiRet(statsobj.AddCounter(pData->stats, (uchar *)"requests_submitted",
			ctrType_IntCtr, CTR_FLAG_RESETTABLE, &pData->ctrSplkRequestsSubmitted));

	STATSCOUNTER_INIT(pData->splkRequestsSuccess, pData->mutSplkRequestsSuccess);
	CHKiRet(statsobj.AddCounter(pData->stats, (uchar *)"requests_success",
			ctrType_IntCtr, CTR_FLAG_RESETTABLE, &pData->splkRequestsSuccess));

	STATSCOUNTER_INIT(pData->splkRequestsFailed, pData->mutSplkRequestsFailed);
	CHKiRet(statsobj.AddCounter(pData->stats, (uchar *)"requests_fail",
			ctrType_IntCtr, CTR_FLAG_RESETTABLE, &pData->splkRequestsFailed));

	STATSCOUNTER_INIT(pData->ctrSplkRequestsCount, pData->mutCtrSplkRequestsCount);
	CHKiRet(statsobj.AddCounter(pData->stats, (uchar *)"requests_count",
			ctrType_IntCtr, CTR_FLAG_RESETTABLE, &pData->ctrSplkRequestsCount));

	STATSCOUNTER_INIT(pData->splkRequestsBytes, pData->mutSplkRequestsBytes);
	CHKiRet(statsobj.AddCounter(pData->stats, (uchar *)"requests_bytes",
			ctrType_IntCtr, CTR_FLAG_RESETTABLE, &pData->splkRequestsBytes));

	STATSCOUNTER_INIT(pData->splkRequestsTimeMs, pData->mutSplkRequestsTimeMs);
	CHKiRet(statsobj.AddCounter(pData->stats, (uchar *)"requests_time_ms",
			ctrType_IntCtr, CTR_FLAG_RESETTABLE, &pData->splkRequestsTimeMs));

	CHKiRet(statsobj.ConstructFinalize(pData->stats));

	/* node created */
	if(loadModConf->tail == NULL) {
		loadModConf->tail = loadModConf->root = pData;
	} else {
		loadModConf->tail->next = pData;
		loadModConf->tail = pData;
	}
CODE_STD_FINALIZERnewActInst
	cnfparamvalsDestruct(pvals, &actpblk);
	if (serverParam)
		free(serverParam);
ENDnewActInst


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
//TODO
ENDdbgPrintInstInfo


BEGINtryResume
CODESTARTtryResume
	DBGPRINTF("omsplunkhec: function tryResume called\n");
	iRet = checkConn(pWrkrData);
ENDtryResume


NO_LEGACY_CONF_parseSelectorAct


BEGINmodExit
CODESTARTmodExit
	if(pInputName != NULL)
		prop.Destruct(&pInputName);
	curl_global_cleanup();
	objRelease(prop, CORE_COMPONENT);
ENDmodExit

// HUP handling for the instance...
BEGINdoHUP
CODESTARTdoHUP
	pthread_mutex_lock(&pData->mutErrFile);
	if (pData->fdErrorfile != -1) {
		close(pData->fdErrorfile);
		pData->fdErrorfile = -1;
	}
	pthread_mutex_unlock(&pData->mutErrFile);
ENDdoHUP


// HUP handling for the worker...
BEGINdoHUPWrkr
CODESTARTdoHUPWrkr
	if (pWrkrData->pData->reloadOnHup) {
		LogMsg(0, NO_ERRCODE, LOG_INFO, "omsplunkhec: received HUP reloading curl handles");
		curlCleanup(pWrkrData);
		CHKiRet(curlSetup(pWrkrData));
	}
finalize_it:
ENDdoHUPWrkr


BEGINbeginTransaction
CODESTARTbeginTransaction
	if(!pWrkrData->pData->batch) {
		FINALIZE;
	}

	initializeBatch(pWrkrData);
finalize_it:
ENDbeginTransaction

BEGINendTransaction
CODESTARTendTransaction
	/* End Transaction only if batch data is not empty */
	if (pWrkrData->batch.nmemb > 0) {
		CHKiRet(submitBatch(pWrkrData, NULL));
	} else {
		dbgprintf("omsplunkhec: endTransaction, pWrkrData->batch.nmemb = 0, "
			"nothing to send. \n");
	}
finalize_it:
ENDendTransaction



BEGINdoAction
size_t nBytes;
sbool submit;
CODESTARTdoAction

	STATSCOUNTER_INC(ctrSplkRequestsSubmitted, mutCtrMessagesSubmitted);

	if (pWrkrData->pData->batch) {
		/* If the batchMaxBatchSize is 1, then build and immediately post a batch with 1 element.
		 * This mode will play nicely with rsyslog's action.resumeRetryCount logic.
		 */
		if (pWrkrData->pData->batchMaxBatchSize == 1) {
			initializeBatch(pWrkrData);
			CHKiRet(buildBatch(pWrkrData, ppString[0]));
			CHKiRet(submitBatch(pWrkrData, ppString));
			FINALIZE;
		}

		/* Check conditions :
		 * - Total batch size > pWrkrData->pData->batchMaxBatchSize
		 * - Total bytes > pWrkrData->pData->batchMaxBatchBytes
		 */
		nBytes = ustrlen((char *)ppString[0]) - 1 ;
		submit = 0;

		size_t extraBytes = pWrkrData->batch.nmemb > 0 ? pWrkrData->batch.nmemb + 1 : 2;
		size_t batch_size = pWrkrData->batch.sizeBytes + extraBytes + 1;

		if (pWrkrData->batch.nmemb >= pWrkrData->pData->batchMaxBatchSize) {
			submit = 1;
			DBGPRINTF("omsplunkhec: batchMaxBatchSize limit reached submitting batch of %zd elements.\n",
				pWrkrData->batch.nmemb);
		} else if ((batch_size + nBytes) > pWrkrData->pData->batchMaxBatchBytes) {
			submit = 1;
			DBGPRINTF("omsplunkhec: maxbytes limit reached submitting partial batch of %zd elements.\n",
				pWrkrData->batch.nmemb);
		}

		DBGPRINTF("omsplunkhec: doAction, submit = %d\n", (int)submit);

		if (submit) {
			CHKiRet(submitBatch(pWrkrData, ppString));
			initializeBatch(pWrkrData);
		}

		CHKiRet(buildBatch(pWrkrData, ppString[0]));

		/* If there is only one item in the batch, all previous items have been
	 	 * submitted or this is the first item for this transaction. Return previous
		 * committed so that all items leading up to the current (exclusive)
		 * are not replayed should a failure occur anywhere else in the transaction. */
		iRet = pWrkrData->batch.nmemb == 1 ? RS_RET_PREVIOUS_COMMITTED : RS_RET_DEFER_COMMIT;
	} else {
		CHKiRet(curlPost(pWrkrData, ppString[0], strlen((char*)ppString[0]), 1));
	}
finalize_it:
ENDdoAction


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
CODEqueryEtryPt_STD_OMOD8_QUERIES
CODEqueryEtryPt_IsCompatibleWithFeature_IF_OMOD_QUERIES
CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES
CODEqueryEtryPt_doHUP
CODEqueryEtryPt_doHUPWrkr /* Load the worker HUP handling code */
CODEqueryEtryPt_TXIF_OMOD_QUERIES /* we support the transactional interface! */
CODEqueryEtryPt_STD_CONF2_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	/* we only support the current interface specification */
	*ipIFVersProvided = CURR_MOD_IF_VERSION;
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(prop, CORE_COMPONENT));
	CHKiRet(objUse(statsobj, CORE_COMPONENT));

	CHKiRet(statsobj.Construct(&httpStats));
	CHKiRet(statsobj.SetName(httpStats, (uchar *)"omsplunkhec"));
	CHKiRet(statsobj.SetOrigin(httpStats, (uchar*)"omsplunkhec"));

	STATSCOUNTER_INIT(ctrSplkRequestsSubmitted, mutCtrSplkRequestsSubmitted);
	CHKiRet(statsobj.AddCounter(httpStats, (uchar *)"requests_submitted",
			ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrSplkRequestsSubmitted));

	STATSCOUNTER_INIT(splkRequestsSuccess, mutSplkRequestsSuccess);
	CHKiRet(statsobj.AddCounter(httpStats, (uchar *)"requests_success",
			ctrType_IntCtr, CTR_FLAG_RESETTABLE, &splkRequestsSuccess));

	STATSCOUNTER_INIT(splkRequestsFailed, mutSplkRequestsFailed);
	CHKiRet(statsobj.AddCounter(httpStats, (uchar *)"requests_fail",
			ctrType_IntCtr, CTR_FLAG_RESETTABLE, &splkRequestsFailed));

	STATSCOUNTER_INIT(ctrSplkRequestsCount, mutCtrSplkRequestsCount);
	CHKiRet(statsobj.AddCounter(httpStats, (uchar *)"requests_count",
			ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrSplkRequestsCount));

	STATSCOUNTER_INIT(splkRequestsBytes, mutSplkRequestsBytes);
	CHKiRet(statsobj.AddCounter(httpStats, (uchar *)"requests_bytes",
			ctrType_IntCtr, CTR_FLAG_RESETTABLE, &splkRequestsBytes));

	STATSCOUNTER_INIT(splkRequestsTimeMs, mutSplkRequestsTimeMs);
	CHKiRet(statsobj.AddCounter(httpStats, (uchar *)"requests_time_ms",
			ctrType_IntCtr, CTR_FLAG_RESETTABLE, &splkRequestsTimeMs));

	CHKiRet(statsobj.ConstructFinalize(httpStats));

	if (curl_global_init(CURL_GLOBAL_ALL) != 0) {
		LogError(0, RS_RET_OBJ_CREATION_FAILED, "CURL fail. -http disabled");
		ABORT_FINALIZE(RS_RET_OBJ_CREATION_FAILED);
	}

	CHKiRet(prop.Construct(&pInputName));
	CHKiRet(prop.SetString(pInputName, UCHAR_CONSTANT("omsplunkhec"), sizeof("omsplunkhec") - 1));
	CHKiRet(prop.ConstructFinalize(pInputName));
ENDmodInit
