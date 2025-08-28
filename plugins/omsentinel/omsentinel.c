/* omsentinel.c
 * This is an http output module based on omhttp
 *
 * NOTE: read comments in module-template.h for more specifics!
 *
 * Copyright 2011 Nathan Scott.
 * Copyright 2009-2018 Rainer Gerhards and Adiscon GmbH.
 * Copyright 2018 Christian Tramnitz
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
#include <time.h>
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
#include <zlib.h>
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
#include "dirty.h"

#ifndef O_LARGEFILE
	#define O_LARGEFILE 0
#endif

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("omsentinel")

/* internal structures */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(prop) DEFobjCurrIf(ruleset) DEFobjCurrIf(statsobj)

statsobj_t *httpStats;
STATSCOUNTER_DEF(ctrMessagesSubmitted, mutCtrMessagesSubmitted);	// Number of message submitted to module
STATSCOUNTER_DEF(ctrMessagesSuccess, mutCtrMessagesSuccess);		// Number of messages successfully sent
STATSCOUNTER_DEF(ctrMessagesFail, mutCtrMessagesFail);			// Number of messages that failed to send
STATSCOUNTER_DEF(ctrMessagesRetry, mutCtrMessagesRetry);		// Number of messages requeued for retry
STATSCOUNTER_DEF(ctrHttpRequestCount, mutCtrHttpRequestCount);		// Number of attempted HTTP requests
STATSCOUNTER_DEF(ctrHttpRequestSuccess, mutCtrHttpRequestSuccess);	// Number of successful HTTP requests
STATSCOUNTER_DEF(ctrHttpRequestFail, mutCtrHttpRequestFail);		// Number of failed HTTP req, 4XX+ are NOT failures
STATSCOUNTER_DEF(ctrHttpStatusSuccess, mutCtrHttpStatusSuccess);	// Number of requests returning 1XX/2XX status
STATSCOUNTER_DEF(ctrHttpStatusFail, mutCtrHttpStatusFail);		// Number of requests returning 300+ status

static prop_t *pInputName = NULL;
static int omsentinelInstancesCnt = 0;

#define WRKR_DATA_TYPE_ES 0xBADF0001

#define HTTP_HEADER_CONTENT_JSON "Content-Type: application/json; charset=utf-8"
#define HTTP_HEADER_ENCODING_GZIP "Content-Encoding: gzip"
#define HTTP_HEADER_EXPECT_EMPTY "Expect:"

/* REST API uses this URL:
 * https://<hostName>:<restPort>/restPath
 */
typedef struct curl_slist HEADER;
typedef struct instanceConf_s
{
	uchar *stream_name;
	uchar *dce;
	uchar *dcr;
	uchar *tenant_id;
	uchar *client_id;	// client_id generated from app registration
	uchar *client_secret;	// client_secret generated from app registration
	uchar *scope;		// wanted resource
	uchar *grant_type;	// auth type
	uchar *auth_domain;
	uchar *authorizationHeader;
	time_t authExp;
	uchar *apiRestAuth;
	uchar *authReply;
	uchar *token;
	uchar *baseURL;
	uchar *authParams;	// auth purpose
	int fdErrFile;		// error file fd or -1 if not open
	pthread_mutex_t mutErrFile;
	uchar *authBuf;
	uchar *headerBuf;
	uchar *httpHeader;
	uchar *restPath;
	uchar *proxyHost;
	int proxyPort;
	uchar *tplName;
	uchar *errorFile;
	size_t maxBatchBytes;
	size_t maxBatchSize;
	sbool compress;
	int compressionLevel;	// Compression level for zlib, default=-1, fastest=1, best=9, none=0
	uchar *caCertFile;
	uchar *myCertFile;
	uchar *myPrivKeyFile;
	sbool retryFailures;
	int nhttpRetryCodes;
	unsigned int *httpRetryCodes;
	int nIgnorableCodes;
	unsigned int *ignorableCodes;
	unsigned int ratelimitInterval;
	unsigned int ratelimitBurst;
	/* for retries */
	ratelimit_t *ratelimiter;
	uchar *retryRulesetName;
	ruleset_t *retryRuleset;
	struct instanceConf_s *next;

	uchar *statsName;
	statsobj_t *stats;
	STATSCOUNTER_DEF(ctrHttpRequestsCount, mutCtrHttpRequestsCount); // Number of attempted HTTP requests
	STATSCOUNTER_DEF(httpRequestsBytes, mutHttpRequestsBytes);
	STATSCOUNTER_DEF(httpRequestsTimeMs, mutHttpRequestsTimeMs);
	STATSCOUNTER_DEF(ctrHttpRequestsStatus0xx, mutCtrHttpRequestsStatus0xx); // HTTP requests returning 0xx
	STATSCOUNTER_DEF(ctrHttpRequestsStatus1xx, mutCtrHttpRequestsStatus1xx); // HTTP requests returning 1xx
	STATSCOUNTER_DEF(ctrHttpRequestsStatus2xx, mutCtrHttpRequestsStatus2xx); // HTTP requests returning 2xx
	STATSCOUNTER_DEF(ctrHttpRequestsStatus3xx, mutCtrHttpRequestsStatus3xx); // HTTP requests returning 3xx
	STATSCOUNTER_DEF(ctrHttpRequestsStatus4xx, mutCtrHttpRequestsStatus4xx); // HTTP requests returning 4xx
	STATSCOUNTER_DEF(ctrHttpRequestsStatus5xx, mutCtrHttpRequestsStatus5xx); // HTTP requests returning 5xx
} instanceData;

struct modConfData_s
{
	rsconf_t *pConf; /* our overall config object */
	instanceConf_t *root, *tail;
};
static modConfData_t *loadModConf = NULL; /* modConf ptr to use for the current load process */

typedef struct wrkrInstanceData
{
	PTR_ASSERT_DEF
	instanceData *pData;
	int serverIndex;
	int replyLen;
	char *reply;
	long httpStatusCode;	   /* http status code of response */
	CURL *curlCheckConnHandle; /* libcurl session handle for checking the server connection */
	CURL *curlPostHandle;	   /* libcurl session handle for posting data to the server */
	HEADER *curlHeader;		   /* json POST request info */
	uchar *restURL;			   /* last used URL for error reporting */
	sbool bzInitDone;
	z_stream zstrm; /* zip stream to use for gzip http compression */
	struct
	{
		uchar **data;	  /* array of strings, this will be batched up lazily */
		uchar *restPath;  /* Helper for restpath in batch mode */
		size_t sizeBytes; /* total length of this batch in bytes */
		size_t nmemb;	  /* number of messages in batch (for statistics counting) */

	} batch;
	struct
	{
		uchar *buf;
		size_t curLen;
		size_t len;
	} compressCtx;
} wrkrInstanceData_t;

/* tables for interfacing with the v6 config system */
/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {
	{"stream_name", eCmdHdlrGetWord, 1},
	{"dce", eCmdHdlrGetWord, 1},
	{"dcr", eCmdHdlrGetWord, 1},
	{"tenant_id", eCmdHdlrGetWord, 1},
	{"client_id", eCmdHdlrGetWord, 1},
	{"client_secret", eCmdHdlrGetWord, 1},
	{"grant_type", eCmdHdlrGetWord, 0},
	{"scope", eCmdHdlrGetWord, 0},
	{"auth_domain", eCmdHdlrGetWord, 0},
	{"proxyhost", eCmdHdlrString, 0},
	{"proxyport", eCmdHdlrInt, 0},
	{"batch.maxbytes", eCmdHdlrSize, 0},
	{"batch.maxsize", eCmdHdlrSize, 0},
	{"compress", eCmdHdlrBinary, 0},
	{"compress.level", eCmdHdlrInt, 0},
	{"errorfile", eCmdHdlrGetWord, 0},
	{"template", eCmdHdlrGetWord, 0},
	{"tls.cacert", eCmdHdlrString, 0},
	{"tls.mycert", eCmdHdlrString, 0},
	{"tls.myprivkey", eCmdHdlrString, 0},
	{"httpretrycodes", eCmdHdlrArray, 0},
	{"retry", eCmdHdlrBinary, 0},
	{"retry.ruleset", eCmdHdlrString, 0},
	{"ratelimit.interval", eCmdHdlrInt, 0},
	{"ratelimit.burst", eCmdHdlrInt, 0},
	{"name", eCmdHdlrGetWord, 0},
	{"httpignorablecodes", eCmdHdlrArray, 0},
};
static struct cnfparamblk actpblk =
	{CNFPARAMBLK_VERSION,
	 sizeof(actpdescr) / sizeof(struct cnfparamdescr),
	 actpdescr};

static rsRetVal curlSetup(wrkrInstanceData_t *pWrkrData);
static void curlCleanup(wrkrInstanceData_t *pWrkrData);
static void curlCheckConnSetup(wrkrInstanceData_t *const pWrkrData);
static rsRetVal curlAuth(wrkrInstanceData_t *pWrkrData, uchar *message);

/* compressCtx functions */
static void ATTR_NONNULL()
initCompressCtx(wrkrInstanceData_t *pWrkrData);

static void ATTR_NONNULL()
freeCompressCtx(wrkrInstanceData_t *pWrkrData);

static rsRetVal ATTR_NONNULL()
resetCompressCtx(wrkrInstanceData_t *pWrkrData, size_t len);

static rsRetVal ATTR_NONNULL()
growCompressCtx(wrkrInstanceData_t *pWrkrData, size_t newLen);

static rsRetVal ATTR_NONNULL()
appendCompressCtx(wrkrInstanceData_t *pWrkrData, uchar *srcBuf, size_t srcLen);

BEGINcreateInstance
CODESTARTcreateInstance
	pData->fdErrFile = -1;
	pthread_mutex_init(&pData->mutErrFile, NULL);
	pData->caCertFile = NULL;
	pData->myCertFile = NULL;
	pData->myPrivKeyFile = NULL;
	pData->ratelimiter = NULL;
	pData->retryRulesetName = NULL;
	pData->retryRuleset = NULL;
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
	pWrkrData->bzInitDone = 0;


	//batch
	pWrkrData->batch.nmemb = 0;
	pWrkrData->batch.sizeBytes = 0;
	batchData = (uchar **)malloc(pData->maxBatchSize * sizeof(uchar *));
	if (batchData == NULL)
	{
		LogError(0, RS_RET_OUT_OF_MEMORY,
				"omsentinel: cannot allocate memory for batch queue\n");
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}
	else
	{
		pWrkrData->batch.data = batchData;
		pWrkrData->batch.restPath = NULL;
	}


	// Log ingestion config parsing
	if (pData->dce && pData->dcr && pData->stream_name)
	{
		if (asprintf((char **)&pData->baseURL, "https://%s", pData->dce) < 0)
		{
			LogError(0, RS_RET_OUT_OF_MEMORY, "omsentinel: cannot allocate memory for base URL\n");
			ABORT_FINALIZE(RS_RET_ERR);
		}
		if (asprintf((char **)&pData->restPath, "/dataCollectionRules/%s/streams/%s?api-version=2023-01-01", pData->dcr, pData->stream_name) < 0)
		{
			LogError(0, RS_RET_OUT_OF_MEMORY, "omsentinel: cannot allocate memory for rest path\n");
			ABORT_FINALIZE(RS_RET_ERR);
		}
	}
	else
	{
		LogError(0, RS_RET_PARAM_ERROR,
				"omsentinel: Parameters missings 'dcr, dce, stream_name'");
		ABORT_FINALIZE(RS_RET_ERR);
	}

	// Authentification
	if (pData->scope && pData->client_secret && pData->client_id && pData->grant_type && pData->tenant_id)
	{
		if (asprintf((char **)&pData->apiRestAuth, "https://%s/%s/oauth2/v2.0/token",pData->auth_domain,pData->tenant_id) < 0)
		{
			LogError(0, RS_RET_OUT_OF_MEMORY, "omsentinel: cannot allocate memory for auth api\n");
			ABORT_FINALIZE(RS_RET_ERR);
		}

		if (asprintf((char **)&pData->authParams, "scope=%s&client_secret=%s&client_id=%s&grant_type=%s", pData->scope, pData->client_secret, pData->client_id, pData->grant_type) < 0)
		{
			LogError(0, RS_RET_OUT_OF_MEMORY, "omsentinel: cannot allocate memory for auth params\n");
			ABORT_FINALIZE(RS_RET_ERR);
		}

		curlAuth(pWrkrData, pData->authParams);
	}
	else
	{
		LogError(0, RS_RET_PARAM_ERROR, "parameters missings 'scope, client_secret, client_id, grant_type, tenant_id'");
	}

	initCompressCtx(pWrkrData);
	iRet = curlSetup(pWrkrData);

finalize_it:
ENDcreateWrkrInstance

BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if (eFeat == sFEATURERepeatedMsgReduction)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature

BEGINfreeInstance
CODESTARTfreeInstance
	if (pData->fdErrFile != -1)
	{
		close(pData->fdErrFile);
	}
	pthread_mutex_destroy(&pData->mutErrFile);
	free(pData->httpHeader);
	free(pData->authBuf);
	free(pData->headerBuf);
	free(pData->restPath);
	free(pData->client_id);		// sentinel auth
	free(pData->client_secret);	// sentinel auth
	free(pData->scope);		// sentinel auth
	free(pData->grant_type);	// sentinel auth
	free(pData->tenant_id);		// sentinel tenant_id
	free(pData->auth_domain);
	free(pData->dce);
	free(pData->dcr);
	free(pData->stream_name);
	free(pData->baseURL);
	free(pData->authReply);
	free(pData->authParams);
	free(pData->token);
	free(pData->apiRestAuth);
	free(pData->proxyHost);
	free(pData->tplName);
	free(pData->errorFile);
	free(pData->caCertFile);
	free(pData->myCertFile);
	free(pData->myPrivKeyFile);
	free(pData->httpRetryCodes);
	free(pData->retryRulesetName);
	free(pData->ignorableCodes);
	if (pData->ratelimiter != NULL)
	{
		ratelimitDestruct(pData->ratelimiter);
	}
	if (pData->stats)
	{
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

	if (pWrkrData->batch.restPath != NULL)
	{
		free(pWrkrData->batch.restPath);
		pWrkrData->batch.restPath = NULL;
	}

	if (pWrkrData->bzInitDone)
	{
		deflateEnd(&pWrkrData->zstrm);
	}
	freeCompressCtx(pWrkrData);

ENDfreeWrkrInstance

BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
ENDdbgPrintInstInfo

/* http POST result string ... useful for debugging */
static size_t
curlResult(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	char *p = (char *)ptr;
	wrkrInstanceData_t *pWrkrData = (wrkrInstanceData_t *)userdata;
	char *buf;
	size_t newlen;
	PTR_ASSERT_CHK(pWrkrData, WRKR_DATA_TYPE_ES);
	newlen = pWrkrData->replyLen + size * nmemb;
	if ((buf = realloc(pWrkrData->reply, newlen + 1)) == NULL)
	{
		LogError(errno, RS_RET_ERR, "omsentinel: realloc failed in curlResult");
		return 0; /* abort due to failure */
	}
	memcpy(buf + pWrkrData->replyLen, p, size * nmemb);
	pWrkrData->replyLen = newlen;
	pWrkrData->reply = buf;
	return size * nmemb;
}

BEGINtryResume
CODESTARTtryResume
	DBGPRINTF("omsentinel: tryResume called\n");
ENDtryResume


static rsRetVal ATTR_NONNULL(1)
setPostURL(wrkrInstanceData_t *const pWrkrData)
{
	uchar *restPath;
	char *baseUrl;
	es_str_t *url;
	int r;
	DEFiRet;
	instanceData *const pData = pWrkrData->pData;

	baseUrl = (char *)pData->baseURL;
	url = es_newStrFromCStr(baseUrl, strlen(baseUrl));
	if (url == NULL)
	{
		LogError(0, RS_RET_OUT_OF_MEMORY,
				 "omsentinel: error allocating new estr for POST url.");
		ABORT_FINALIZE(RS_RET_ERR);
	}

	if (pWrkrData->batch.restPath != NULL)
	{
		/* get from batch if set! */
		restPath = pWrkrData->batch.restPath;
	}
	else
	{
		restPath = pData->restPath;
	}

	r = 0;
	if (restPath != NULL)
	{
		r = es_addBuf(&url, (char *)restPath, ustrlen(restPath));
	}

	if (r != 0)
	{
		LogError(0, RS_RET_ERR, "omsentinel: failure in creating restURL, "
								"error code: %d",
				 r);
		ABORT_FINALIZE(RS_RET_ERR);
	}

	if (pWrkrData->restURL != NULL)
	{
		free(pWrkrData->restURL);
	}

	pWrkrData->restURL = (uchar *)es_str2cstr(url, NULL);
	curl_easy_setopt(pWrkrData->curlPostHandle, CURLOPT_URL, pWrkrData->restURL);
	DBGPRINTF("omsentinel: using REST URL: '%s'\n", pWrkrData->restURL);

finalize_it:
	if (url != NULL)
	{
		es_deleteStr(url);
	}
	RETiRet;
}

/*
 * Dumps entire bulk request and response in error log
 * {
 *	"request": {
 *	 	"url": "https://url.com:443/path",
 *	 	"postdata": "mypayload" }
 *	 "response" : {
 *	 	"status": 400,
 *	 	"response": "error string" }
 * }
 */
static rsRetVal
renderJsonErrorMessage(wrkrInstanceData_t *pWrkrData, uchar *reqmsg, char **rendered)
{
	DEFiRet;
	fjson_object *req = NULL;
	fjson_object *res = NULL;
	fjson_object *errRoot = NULL;

	if ((req = fjson_object_new_object()) == NULL)
	{
		ABORT_FINALIZE(RS_RET_ERR);
	}
	fjson_object_object_add(req, "url", fjson_object_new_string((char *)pWrkrData->restURL));
	fjson_object_object_add(req, "postdata", fjson_object_new_string((char *)reqmsg));

	if ((res = fjson_object_new_object()) == NULL)
	{
		fjson_object_put(req); // cleanup request object
		ABORT_FINALIZE(RS_RET_ERR);
	}

#define ERR_MSG_NULL "NULL: curl request failed or no response"
	fjson_object_object_add(res, "status", fjson_object_new_int(pWrkrData->httpStatusCode));
	if (pWrkrData->reply == NULL)
	{
		fjson_object_object_add(res, "message",
			fjson_object_new_string_len(ERR_MSG_NULL, strlen(ERR_MSG_NULL)));
	}
	else
	{
		fjson_object_object_add(res, "message",
			fjson_object_new_string_len(pWrkrData->reply, pWrkrData->replyLen));
	}

	if ((errRoot = fjson_object_new_object()) == NULL)
	{
		fjson_object_put(req); // cleanup request object
		fjson_object_put(res); // cleanup response object
		ABORT_FINALIZE(RS_RET_ERR);
	}

	fjson_object_object_add(errRoot, "request", req);
	fjson_object_object_add(errRoot, "response", res);

	*rendered = strdup((char *)fjson_object_to_json_string(errRoot));

finalize_it:
	if (errRoot != NULL)
	{
		fjson_object_put(errRoot);
	}

	RETiRet;
}

/* write data error request/replies to separate error file
 * Note: we open the file but never close it before exit. If it
 * needs to be closed, HUP must be sent.
 */
static rsRetVal ATTR_NONNULL()
writeDataError(wrkrInstanceData_t *const pWrkrData, instanceData *const pData, uchar *const reqmsg)
{
	char *rendered = NULL;
	size_t toWrite;
	ssize_t wrRet;
	sbool bMutLocked = 0;

	DEFiRet;

	if (pData->errorFile == NULL)
	{
		DBGPRINTF("omsentinel: no local error logger defined - "
				"ignoring REST error information\n");
		FINALIZE;
	}

	pthread_mutex_lock(&pData->mutErrFile);
	bMutLocked = 1;

	CHKiRet(renderJsonErrorMessage(pWrkrData, reqmsg, &rendered));

	if (pData->fdErrFile == -1)
	{
		pData->fdErrFile = open((char *)pData->errorFile,
					O_WRONLY | O_CREAT | O_APPEND | O_LARGEFILE | O_CLOEXEC,
					S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
		if (pData->fdErrFile == -1)
		{
			LogError(errno, RS_RET_ERR, "omsentinel: error opening error file %s",
					 pData->errorFile);
			ABORT_FINALIZE(RS_RET_ERR);
		}
	}

	/* we do not do real error-handling on the err file, as this finally complicates
	 * things way to much.
	 */
	DBGPRINTF("omsentinel: error record: '%s'\n", rendered);
	toWrite = strlen(rendered) + 1;
	/* Note: we overwrite the '\0' terminator with '\n' -- so we avoid
	 * caling malloc() -- write() does NOT need '\0'!
	 */
	rendered[toWrite - 1] = '\n'; /* NO LONGER A NULL-TERMINATED STRING! */
	wrRet = write(pData->fdErrFile, rendered, toWrite);
	if (wrRet != (ssize_t)toWrite)
	{
		LogError(errno, RS_RET_IO_ERROR,
				 "omsentinel: error writing error file %s, write returned %lld",
				 pData->errorFile, (long long)wrRet);
	}

finalize_it:
	if (bMutLocked)
	{
		pthread_mutex_unlock(&pData->mutErrFile);
	}
	free(rendered);
	RETiRet;
}

static rsRetVal
queueBatchOnRetryRuleset(wrkrInstanceData_t *const pWrkrData, instanceData *const pData)
{
	uchar *msgData;
	smsg_t *pMsg;
	DEFiRet;

	if (pData->retryRuleset == NULL)
	{
		LogError(0, RS_RET_ERR, "omsentinel: queueBatchOnRetryRuleset invalid call with a NULL retryRuleset");
		ABORT_FINALIZE(RS_RET_ERR);
	}

	for (size_t i = 0; i < pWrkrData->batch.nmemb; i++)
	{
		msgData = pWrkrData->batch.data[i];
		DBGPRINTF("omsentinel: queueBatchOnRetryRuleset putting message '%s' into retry ruleset '%s'\n",
				  msgData, pData->retryRulesetName);

		// Construct the message object
		CHKiRet(msgConstruct(&pMsg));
		CHKiRet(MsgSetFlowControlType(pMsg, eFLOWCTL_FULL_DELAY));
		MsgSetInputName(pMsg, pInputName);
		MsgSetRawMsg(pMsg, (const char *)msgData, ustrlen(msgData));
		MsgSetMSGoffs(pMsg, 0); // No header
		MsgSetTAG(pMsg, (const uchar *)"omsentinel-retry", 12);

		// And place it on the retry ruleset
		MsgSetRuleset(pMsg, pData->retryRuleset);

		ratelimitAddMsg(pData->ratelimiter, NULL, pMsg);

		// Count here in case not entire batch succeeds
		STATSCOUNTER_INC(ctrMessagesRetry, mutCtrMessagesRetry);
	}
finalize_it:
	RETiRet;
}

static rsRetVal
checkResult(wrkrInstanceData_t *pWrkrData, uchar *reqmsg)
{
	instanceData *pData;
	long statusCode;
	size_t numMessages;
	DEFiRet;
	CURLcode resCurl = 0;

	pData = pWrkrData->pData;
	statusCode = pWrkrData->httpStatusCode;

	numMessages = pWrkrData->batch.nmemb;

	// 500+ errors return RS_RET_SUSPENDED if NOT batchMode and should be retried
	// status 0 is the default and the request failed for some reason, retry this too
	// 400-499 are malformed input and should not be retried just logged instead
	if (statusCode == 0)
	{
		// request failed, suspend or retry
		STATSCOUNTER_ADD(ctrMessagesFail, mutCtrMessagesFail, numMessages);
		STATSCOUNTER_INC(pData->ctrHttpRequestsStatus0xx, pData->mutCtrHttpRequestsStatus0xx);
		iRet = RS_RET_SUSPENDED;
	}
	else if (statusCode >= 500)
	{
		// server error, suspend or retry
		STATSCOUNTER_INC(ctrHttpStatusFail, mutCtrHttpStatusFail);
		STATSCOUNTER_ADD(ctrMessagesFail, mutCtrMessagesFail, numMessages);
		iRet = RS_RET_SUSPENDED;
	}
	else if (statusCode >= 300)
	{
		// redirection or client error, NO suspend nor retry
		STATSCOUNTER_INC(ctrHttpStatusFail, mutCtrHttpStatusFail);
		STATSCOUNTER_ADD(ctrMessagesFail, mutCtrMessagesFail, numMessages);
		iRet = RS_RET_SUSPENDED;

		if (statusCode >= 300 && statusCode < 400)
		{
			STATSCOUNTER_INC(pData->ctrHttpRequestsStatus3xx, pData->mutCtrHttpRequestsStatus3xx);
		}
		else if (statusCode >= 400 && statusCode < 500)
		{
			STATSCOUNTER_INC(pData->ctrHttpRequestsStatus4xx, pData->mutCtrHttpRequestsStatus4xx);
		}
		else if (statusCode >= 500 && statusCode < 600)
		{
			STATSCOUNTER_INC(pData->ctrHttpRequestsStatus5xx, pData->mutCtrHttpRequestsStatus5xx);
		}
	}
	else
	{
		// success, normal state
		// includes 2XX (success like 200-OK)
		// includes 1XX (informational like 100-Continue)
		STATSCOUNTER_INC(ctrHttpStatusSuccess, mutCtrHttpStatusSuccess);
		STATSCOUNTER_ADD(ctrMessagesSuccess, mutCtrMessagesSuccess, numMessages);

		// increment instance counts if enabled
		if (statusCode >= 0 && statusCode < 100)
		{
			STATSCOUNTER_INC(pData->ctrHttpRequestsStatus0xx, pData->mutCtrHttpRequestsStatus0xx);
		}
		else if (statusCode >= 100 && statusCode < 200)
		{
			STATSCOUNTER_INC(pData->ctrHttpRequestsStatus1xx, pData->mutCtrHttpRequestsStatus1xx);
		}
		else if (statusCode >= 200 && statusCode < 300)
		{
			STATSCOUNTER_INC(pData->ctrHttpRequestsStatus2xx, pData->mutCtrHttpRequestsStatus2xx);
		}
		iRet = RS_RET_OK;
	}

	// get curl stats for instance
	{
		long req = 0;
		double total = 0;
		/* record total bytes */
		resCurl = curl_easy_getinfo(pWrkrData->curlPostHandle, CURLINFO_REQUEST_SIZE, &req);
		if (!resCurl)
		{
			STATSCOUNTER_ADD(pWrkrData->pData->httpRequestsBytes,
							 pWrkrData->pData->mutHttpRequestsBytes,
							 (uint64_t)req);
		}
		resCurl = curl_easy_getinfo(pWrkrData->curlPostHandle, CURLINFO_TOTAL_TIME, &total);
		if (CURLE_OK == resCurl)
		{
			/* this needs to be converted to milliseconds */
			long total_time_ms = (long)(total * 1000);
			STATSCOUNTER_ADD(pWrkrData->pData->httpRequestsTimeMs,
							 pWrkrData->pData->mutHttpRequestsTimeMs,
							 (uint64_t)total_time_ms);
		}
	}

	/* when retriable codes are configured, always check status codes */
	if (pData->nhttpRetryCodes)
	{
		for (int i = 0; i < pData->nhttpRetryCodes && pData->httpRetryCodes[i] != 0; ++i)
		{
			if (statusCode == (long)pData->httpRetryCodes[i])
			{
				ABORT_FINALIZE(RS_RET_SUSPENDED);
			}
		}
	}

	// also check if we can mark this as processed
	if (iRet != RS_RET_OK && pData->ignorableCodes)
	{
		for (int i = 0; i < pData->nIgnorableCodes && pData->ignorableCodes[i] != 0; ++i)
		{
			if (statusCode == (long)pData->ignorableCodes[i])
			{
				ABORT_FINALIZE(RS_RET_OK);
			}
		}
	}

	if (iRet != RS_RET_OK)
	{
		LogMsg(0, iRet, LOG_ERR, "omsentinel: checkResult error http status code: %ld reply: %s",
			   statusCode, pWrkrData->reply != NULL ? pWrkrData->reply : "NULL");

		writeDataError(pWrkrData, pWrkrData->pData, reqmsg);

		if (iRet == RS_RET_DATAFAIL)
		{
			ABORT_FINALIZE(iRet);
		}

		if (pData->maxBatchSize > 1)
		{
			// Write each message back to retry ruleset if configured
			if (pData->retryFailures && pData->retryRuleset != NULL)
			{
				// Retry stats counted inside this function call
				iRet = queueBatchOnRetryRuleset(pWrkrData, pData);
				if (iRet != RS_RET_OK)
				{
					LogMsg(0, iRet, LOG_ERR,
						   "omsentinel: checkResult error while queueing to retry ruleset"
						   "some messages may be lost");
				}
			}
			iRet = RS_RET_OK; // We've done all we can tell rsyslog to carry on
		}
	}

finalize_it:
	RETiRet;
}

/* Compress a buffer before sending using zlib. Based on code from tools/omfwd.c
 * Initialize the zstrm object for gzip compression, using this init function.
 * deflateInit2(z_stream strm, int level, int method,
 *                             int windowBits, int memLevel, int strategy);
 * strm: the zlib stream held in pWrkrData
 * level: the compression level held in pData
 * method: the operation constant Z_DEFLATED
 * windowBits: the size of the compression window 15 = log_2(32768)
 *     to configure as gzip add 16 to windowBits (w | 16) for final value 31
 * memLevel: the memory optimization level 8 is default)
 * strategy: using Z_DEFAULT_STRATEGY is default
 */
static rsRetVal
compressHttpPayload(wrkrInstanceData_t *pWrkrData, uchar *message, unsigned len)
{
	int zRet;
	unsigned outavail;
	uchar zipBuf[32 * 1024];

	DEFiRet;

	if (!pWrkrData->bzInitDone)
	{
		pWrkrData->zstrm.zalloc = Z_NULL;
		pWrkrData->zstrm.zfree = Z_NULL;
		pWrkrData->zstrm.opaque = Z_NULL;
		zRet = deflateInit2(&pWrkrData->zstrm, pWrkrData->pData->compressionLevel,
							Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY);
		if (zRet != Z_OK)
		{
			DBGPRINTF("omsentinel: compressHttpPayload error %d returned from zlib/deflateInit2()\n", zRet);
			ABORT_FINALIZE(RS_RET_ZLIB_ERR);
		}
		pWrkrData->bzInitDone = 1;
	}

	CHKiRet(resetCompressCtx(pWrkrData, len));

	/* now doing the compression */
	pWrkrData->zstrm.next_in = (Bytef *)message;
	pWrkrData->zstrm.avail_in = len;
	/* run deflate() on buffer until everything has been compressed */
	do
	{
		DBGPRINTF("omsentinel: compressHttpPayload in deflate() loop, avail_in %d, total_in %ld\n",
				  pWrkrData->zstrm.avail_in, pWrkrData->zstrm.total_in);
		pWrkrData->zstrm.avail_out = sizeof(zipBuf);
		pWrkrData->zstrm.next_out = zipBuf;

		zRet = deflate(&pWrkrData->zstrm, Z_NO_FLUSH);
		DBGPRINTF("omsentinel: compressHttpPayload after deflate, ret %d, avail_out %d\n",
				  zRet, pWrkrData->zstrm.avail_out);
		if (zRet != Z_OK)
		{
			ABORT_FINALIZE(RS_RET_ZLIB_ERR);
		}
		outavail = sizeof(zipBuf) - pWrkrData->zstrm.avail_out;
		if (outavail != 0)
		{
			CHKiRet(appendCompressCtx(pWrkrData, zipBuf, outavail));
		}

	} while (pWrkrData->zstrm.avail_out == 0);

	/* run deflate again with Z_FINISH with no new input */
	pWrkrData->zstrm.avail_in = 0;
	do
	{
		pWrkrData->zstrm.avail_out = sizeof(zipBuf);
		pWrkrData->zstrm.next_out = zipBuf;
		deflate(&pWrkrData->zstrm, Z_FINISH); /* returns Z_STREAM_END == 1 */
		outavail = sizeof(zipBuf) - pWrkrData->zstrm.avail_out;
		if (outavail != 0)
		{
			CHKiRet(appendCompressCtx(pWrkrData, zipBuf, outavail));
		}

	} while (pWrkrData->zstrm.avail_out == 0);

finalize_it:
	if (pWrkrData->bzInitDone)
	{
		deflateEnd(&pWrkrData->zstrm);
	}
	pWrkrData->bzInitDone = 0;
	RETiRet;
}

static void ATTR_NONNULL()
initCompressCtx(wrkrInstanceData_t *pWrkrData)
{
	pWrkrData->compressCtx.buf = NULL;
	pWrkrData->compressCtx.curLen = 0;
	pWrkrData->compressCtx.len = 0;
}

static void ATTR_NONNULL()
freeCompressCtx(wrkrInstanceData_t *pWrkrData)
{
	if (pWrkrData->compressCtx.buf != NULL)
	{
		free(pWrkrData->compressCtx.buf);
		pWrkrData->compressCtx.buf = NULL;
	}
}

static rsRetVal ATTR_NONNULL()
resetCompressCtx(wrkrInstanceData_t *pWrkrData, size_t len)
{
	DEFiRet;
	pWrkrData->compressCtx.curLen = 0;
	pWrkrData->compressCtx.len = len;
	CHKiRet(growCompressCtx(pWrkrData, len));

finalize_it:
	if (iRet != RS_RET_OK)
	{
		freeCompressCtx(pWrkrData);
	}
	RETiRet;
}

static rsRetVal ATTR_NONNULL()
growCompressCtx(wrkrInstanceData_t *pWrkrData, size_t newLen)
{
	DEFiRet;
	if (pWrkrData->compressCtx.buf == NULL)
	{
		CHKmalloc(pWrkrData->compressCtx.buf = (uchar *)malloc(sizeof(uchar) * newLen));
	}
	else
	{
		uchar *const newbuf = (uchar *)realloc(pWrkrData->compressCtx.buf, sizeof(uchar) * newLen);
		CHKmalloc(newbuf);
		pWrkrData->compressCtx.buf = newbuf;
	}
	pWrkrData->compressCtx.len = newLen;
finalize_it:
	RETiRet;
}

static rsRetVal ATTR_NONNULL()
appendCompressCtx(wrkrInstanceData_t *pWrkrData, uchar *srcBuf, size_t srcLen)
{
	size_t newLen;
	DEFiRet;
	newLen = pWrkrData->compressCtx.curLen + srcLen;
	if (newLen > pWrkrData->compressCtx.len)
	{
		CHKiRet(growCompressCtx(pWrkrData, newLen));
	}

	memcpy(pWrkrData->compressCtx.buf + pWrkrData->compressCtx.curLen, srcBuf, srcLen);
	pWrkrData->compressCtx.curLen = newLen;
finalize_it:
	if (iRet != RS_RET_OK)
	{
		freeCompressCtx(pWrkrData);
	}
	RETiRet;
}

/* Some duplicate code to curlSetup, but we need to add the gzip content-encoding
 * header at runtime, and if the compression fails, we do not want to send it.
 * Additionally, the curlCheckConnHandle should not be configured with a gzip header.
 */
static rsRetVal ATTR_NONNULL()
buildCurlHeaders(wrkrInstanceData_t *pWrkrData, sbool contentEncodeGzip)
{
	struct curl_slist *slist = NULL;

	DEFiRet;

	slist = curl_slist_append(slist, HTTP_HEADER_CONTENT_JSON);

	CHKmalloc(slist);

	// Configured headers..
	if (pWrkrData->pData->headerBuf != NULL)
	{
		slist = curl_slist_append(slist, (char *)pWrkrData->pData->headerBuf);
		CHKmalloc(slist);
	}

	if (pWrkrData->pData->httpHeader)
	{
		slist = curl_slist_append(slist, (char *)pWrkrData->pData->httpHeader);
		CHKmalloc(slist);
	}

	// When sending more than 1Kb, libcurl automatically sends an Except: 100-Continue header
	// and will wait 1s for a response, could make this configurable but for now disable
	slist = curl_slist_append(slist, HTTP_HEADER_EXPECT_EMPTY);
	CHKmalloc(slist);

	if (contentEncodeGzip)
	{
		slist = curl_slist_append(slist, HTTP_HEADER_ENCODING_GZIP);
		CHKmalloc(slist);
	}

	if (pWrkrData->curlHeader != NULL)
	{
		curl_slist_free_all(pWrkrData->curlHeader);
	}

	pWrkrData->curlHeader = slist;

finalize_it:
	if (iRet != RS_RET_OK)
	{
		curl_slist_free_all(slist);
		LogError(0, iRet, "omsentinel: error allocating curl header slist, using previous one");
	}
	RETiRet;
}

size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
	instanceData *pData = (instanceData *)userp;
	size_t totalSize = size * nmemb;

	pData->authReply = (uchar *)strdup((const char *)contents);

	return totalSize;
}

static rsRetVal curlAuth(wrkrInstanceData_t *pWrkrData, uchar *message)
{
	instanceData *pData = pWrkrData->pData;
	CURL *curl;
	CURLcode res;
	struct curl_slist *headers = NULL;
	long http_code = 0;
	DEFiRet;

	curl = curl_easy_init();
	if (curl)
	{
		headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
		curl_easy_setopt(curl, CURLOPT_URL, pData->apiRestAuth);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, message);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, pData);
		if (pData->proxyHost != NULL)
		{
			curl_easy_setopt(curl, CURLOPT_PROXY, pData->proxyHost);
		}
		if (pData->proxyPort != 0)
		{
			curl_easy_setopt(curl, CURLOPT_PROXYPORT, pData->proxyPort);
		}
		if (pData->authBuf != NULL)
		{
			curl_easy_setopt(curl, CURLOPT_USERPWD, pData->authBuf);
			curl_easy_setopt(curl, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
		}
		res = curl_easy_perform(curl);
		if (res != CURLE_OK)
		{
			LogError(0, RS_RET_ERR, "curl:  error: %s\n", curl_easy_strerror(res));
			ABORT_FINALIZE(RS_RET_ERR);
		}

		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
		if (http_code != 200)
		{
			dbgprintf("omsentinel: http_reply_code=%ld \n", http_code);
			ABORT_FINALIZE(RS_RET_SUSPENDED);
		}
	}

	// parsing and serializing http response
	if(pData->authReply){
		struct json_object *parsed_json = json_tokener_parse((char *)pData->authReply);
		if (parsed_json != NULL)
		{
			struct json_object *access_token = NULL;
			struct json_object *expires_in = NULL;
			// token bearer
			if (json_object_object_get_ex(parsed_json, "access_token", &access_token))
			{
				const char *tokenStr = json_object_get_string(access_token);
				if (tokenStr)
				{
					if(!(pData->token = (uchar *)strdup(tokenStr)))
					{
						LogError(0, RS_RET_OUT_OF_MEMORY,
							"omsentinel: could not allocate Bearer token \n");
						ABORT_FINALIZE(RS_RET_ERR);
					}
				}
			}
			// expiration date
			if (json_object_object_get_ex(parsed_json, "expires_in", &expires_in))
			{
				const int expireDate = json_object_get_int(expires_in);
				if (expireDate)
				{
					// Calculation of the expiration date from now
					pData->authExp = time(NULL) + expireDate;
				}
			}
			json_object_put(parsed_json);
		}
	}
	else
	{
		LogError(0, RS_RET_OUT_OF_MEMORY, "omsentinel: could not allocate http response \n");
		ABORT_FINALIZE(RS_RET_ERR);
	}

	// httpHeader
	if (asprintf((char **)&pData->httpHeader, (char *)pData->authorizationHeader, pData->token) < 0)
	{
		LogError(0, RS_RET_OUT_OF_MEMORY, "omsentinel: cannot allocate memory for http header\n");
		ABORT_FINALIZE(RS_RET_ERR);
	}

finalize_it:
	curl_easy_cleanup(curl);
	RETiRet;
}

static rsRetVal ATTR_NONNULL(1, 2)
curlPost(wrkrInstanceData_t *pWrkrData, uchar *message, int msglen, const int nmsgs __attribute__((unused)))
{

	CURLcode curlCode;
	CURL *const curl = pWrkrData->curlPostHandle;
	char errbuf[CURL_ERROR_SIZE] = "";

	char *postData;
	int postLen;
	sbool compressed;
	DEFiRet;

	PTR_ASSERT_SET_TYPE(pWrkrData, WRKR_DATA_TYPE_ES);

	CHKiRet(setPostURL(pWrkrData));

	pWrkrData->reply = NULL;
	pWrkrData->replyLen = 0;
	pWrkrData->httpStatusCode = 0;

	postData = (char *)message;
	postLen = msglen;
	compressed = 0;

	if (pWrkrData->pData->compress)
	{
		iRet = compressHttpPayload(pWrkrData, message, msglen);
		if (iRet != RS_RET_OK)
		{
			LogError(0, iRet, "omsentinel: curlPost error while compressing, will default to uncompressed");
		}
		else
		{
			postData = (char *)pWrkrData->compressCtx.buf;
			postLen = pWrkrData->compressCtx.curLen;
			compressed = 1;
			DBGPRINTF("omsentinel: curlPost compressed %d to %d bytes\n", msglen, postLen);
		}
	}

	buildCurlHeaders(pWrkrData, compressed);

	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, postLen);
	curl_easy_setopt(pWrkrData->curlPostHandle, CURLOPT_HTTPHEADER, pWrkrData->curlHeader);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

	curlCode = curl_easy_perform(curl);
	DBGPRINTF("omsentinel: curlPost curl returned %lld\n", (long long)curlCode);
	STATSCOUNTER_INC(ctrHttpRequestCount, mutCtrHttpRequestCount);
	STATSCOUNTER_INC(pWrkrData->pData->ctrHttpRequestsCount, pWrkrData->pData->mutCtrHttpRequestsCount);

	if (curlCode != CURLE_OK)
	{
		STATSCOUNTER_INC(ctrHttpRequestFail, mutCtrHttpRequestFail);
		LogError(0, RS_RET_SUSPENDED,
				 "omsentinel: suspending ourselves due to server failure %lld: %s",
				 (long long)curlCode, errbuf);
		// Check the result here too and retry if needed, then we should suspend
		// Usually in batch mode we clobber any iRet values, but probably not a great
		// idea to keep hitting a dead server. The http status code will be 0 at this point.
		checkResult(pWrkrData, message);
		ABORT_FINALIZE(RS_RET_SUSPENDED);
	}
	else
	{
		STATSCOUNTER_INC(ctrHttpRequestSuccess, mutCtrHttpRequestSuccess);
	}

	// Grab the HTTP Response code
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &pWrkrData->httpStatusCode);
	if (pWrkrData->reply == NULL)
	{
		DBGPRINTF("omsentinel: curlPost pWrkrData reply==NULL, replyLen = '%d'\n",
				  pWrkrData->replyLen);
	}
	else
	{
		DBGPRINTF("omsentinel: curlPost pWrkrData replyLen = '%d'\n", pWrkrData->replyLen);
		if (pWrkrData->replyLen > 0)
		{
			pWrkrData->reply[pWrkrData->replyLen] = '\0';
			/* Append 0 Byte if replyLen is above 0 - byte has been reserved in malloc */
		}
		// TODO: replyLen++? because 0 Byte is appended
		DBGPRINTF("omsentinel: curlPost pWrkrData reply: '%s'\n", pWrkrData->reply);
	}

	CHKiRet(checkResult(pWrkrData, message));

finalize_it:
	if (pWrkrData->reply != NULL)
	{
		free(pWrkrData->reply);
		pWrkrData->reply = NULL; /* don't leave dangling pointer */
	}
	RETiRet;
}

/* Build a JSON batch by placing each element in an array.
 */
static rsRetVal
serializeBatchJsonArray(wrkrInstanceData_t *pWrkrData, char **batchBuf)
{
	fjson_object *batchArray = NULL;
	fjson_object *msgObj = NULL;
	size_t numMessages = pWrkrData->batch.nmemb;
	size_t sizeTotal = pWrkrData->batch.sizeBytes + numMessages + 1; // messages + brackets + commas
	DBGPRINTF("omsentinel: serializeBatchJsonArray numMessages=%zd sizeTotal=%zd\n", numMessages, sizeTotal);

	DEFiRet;

	batchArray = fjson_object_new_array();
	if (batchArray == NULL)
	{
		LogError(0, RS_RET_ERR, "omsentinel: serializeBatchJsonArray failed to create array");
		ABORT_FINALIZE(RS_RET_ERR);
	}

	for (size_t i = 0; i < numMessages; i++)
	{
		msgObj = fjson_tokener_parse((char *)pWrkrData->batch.data[i]);
		if (msgObj == NULL)
		{
			LogError(0, NO_ERRCODE,
					 "omsentinel: serializeBatchJsonArray failed to parse %s as json, ignoring it",
					 pWrkrData->batch.data[i]);
			continue;
		}
		fjson_object_array_add(batchArray, msgObj);
	}

	const char *batchString = fjson_object_to_json_string_ext(batchArray, FJSON_TO_STRING_PLAIN);
	*batchBuf = strndup(batchString, strlen(batchString));

finalize_it:
	if (batchArray != NULL)
	{
		fjson_object_put(batchArray);
		batchArray = NULL;
	}
	RETiRet;
}

/* Return the final batch size in bytes for each serialization method.
 * Used to decide if a batch should be flushed early.
 */
static size_t
computeBatchSize(wrkrInstanceData_t *pWrkrData)
{
	size_t extraBytes = 0;
	size_t sizeBytes = pWrkrData->batch.sizeBytes;
	size_t numMessages = pWrkrData->batch.nmemb;

	// square brackets, commas between each message
	// 2 + numMessages - 1 = numMessages + 1
	extraBytes = numMessages > 0 ? numMessages + 1 : 2;

	return sizeBytes + extraBytes + 1; // plus a null
}

static void ATTR_NONNULL()
initializeBatch(wrkrInstanceData_t *pWrkrData)
{
	pWrkrData->batch.sizeBytes = 0;
	pWrkrData->batch.nmemb = 0;
	if (pWrkrData->batch.restPath != NULL)
	{
		free(pWrkrData->batch.restPath);
		pWrkrData->batch.restPath = NULL;
	}
}

/* Adds a message to this worker's batch
 */
static rsRetVal
buildBatch(wrkrInstanceData_t *pWrkrData, uchar *message)
{
	DEFiRet;

	if (pWrkrData->batch.nmemb >= pWrkrData->pData->maxBatchSize)
	{
		LogError(0, RS_RET_ERR, "omsentinel: buildBatch something has gone wrong,"
				"number of messages in batch is bigger than the max batch size, bailing");
		ABORT_FINALIZE(RS_RET_ERR);
	}
	pWrkrData->batch.data[pWrkrData->batch.nmemb] = message;
	pWrkrData->batch.sizeBytes += strlen((char *)message);
	pWrkrData->batch.nmemb++;

finalize_it:
	RETiRet;
}

static rsRetVal
submitBatch(wrkrInstanceData_t *pWrkrData, uchar **tpls)
{
	DEFiRet;
	char *batchBuf = NULL;

	iRet = serializeBatchJsonArray(pWrkrData, &batchBuf);

	if (iRet != RS_RET_OK || batchBuf == NULL)
	{
		ABORT_FINALIZE(iRet);
	}

	DBGPRINTF("omsentinel: submitBatch, batch: '%s' tpls: '%p'\n", batchBuf, tpls);

	CHKiRet(curlPost(pWrkrData, (uchar *)batchBuf, strlen(batchBuf), pWrkrData->batch.nmemb));

finalize_it:
	if (batchBuf != NULL)
	{
		free(batchBuf);
	}
	RETiRet;
}

BEGINbeginTransaction
CODESTARTbeginTransaction
	instanceData *pData = pWrkrData->pData;

	if (time(NULL) >= pData->authExp && pData->token)
	{
		if (pData->authReply)
		{
			free(pData->authReply);
		}
		if (pData->token)
		{
			free(pData->token);
		}
		if (pData->httpHeader)
		{
			free(pData->httpHeader);
		}

		// nullify to prevent dangling pointers
		pData->token = NULL;
		pData->authReply = NULL;
		pData->httpHeader = NULL;

		curlAuth(pWrkrData, pData->authParams);
	}

	initializeBatch(pWrkrData);
ENDbeginTransaction

BEGINdoAction
	size_t nBytes;
	sbool submit;
CODESTARTdoAction
	instanceData *const pData = pWrkrData->pData;

	STATSCOUNTER_INC(ctrMessagesSubmitted, mutCtrMessagesSubmitted);

	if (pData->token && pData->dce && pData->dcr && pData->stream_name)
	{

		/* If the maxbatchsize is 1, then build and immediately post a batch with 1 element.
		* This mode will play nicely with rsyslog's action.resumeRetryCount logic.
		*/
		if (pWrkrData->pData->maxBatchSize == 1)
		{
			initializeBatch(pWrkrData);
			CHKiRet(buildBatch(pWrkrData, ppString[0]));
			CHKiRet(submitBatch(pWrkrData, ppString));
			FINALIZE;
		}

		/* We should submit if any of these conditions are true
		* 1. Total batch size > pWrkrData->pData->maxBatchSize
		* 2. Total bytes > pWrkrData->pData->maxBatchBytes
		*/
		nBytes = ustrlen((char *)ppString[0]) - 1;
		submit = 0;

		if (pWrkrData->batch.nmemb >= pWrkrData->pData->maxBatchSize)
		{
			submit = 1;
			DBGPRINTF("omsentinel: maxbatchsize limit reached submitting batch of %zd elements.\n",
					pWrkrData->batch.nmemb);
		}
		else if (computeBatchSize(pWrkrData) + nBytes > pWrkrData->pData->maxBatchBytes)
		{
			submit = 1;
			DBGPRINTF("omsentinel: maxbytes limit reached submitting partial batch of %zd elements.\n",
					pWrkrData->batch.nmemb);
		}

		if (submit)
		{
			CHKiRet(submitBatch(pWrkrData, ppString));
			initializeBatch(pWrkrData);
		}

		CHKiRet(buildBatch(pWrkrData, ppString[0]));

		/* If there is only one item in the batch, all previous items have been
		* submitted or this is the first item for this transaction. Return previous
		* committed so that all items leading up to the current (exclusive)
		* are not replayed should a failure occur anywhere else in the transaction. */
		iRet = pWrkrData->batch.nmemb == 1 ? RS_RET_PREVIOUS_COMMITTED : RS_RET_DEFER_COMMIT;

	}
	else
	{
		LogError(0, RS_RET_ERR, "omsentinel: an error occured, aborting...");
		ABORT_FINALIZE(RS_RET_ERR);
	}

finalize_it:
ENDdoAction

BEGINendTransaction
CODESTARTendTransaction
	/* End Transaction only if batch data is not empty */
	if (pWrkrData->batch.nmemb > 0)
	{
		CHKiRet(submitBatch(pWrkrData, NULL));
	}
	else
	{
		dbgprintf("omsentinel: endTransaction, pWrkrData->batch.nmemb = 0, "
				"nothing to send. \n");
	}
finalize_it:
ENDendTransaction

static void ATTR_NONNULL()
curlSetupCommon(wrkrInstanceData_t *const pWrkrData, CURL *const handle)
{
	PTR_ASSERT_SET_TYPE(pWrkrData, WRKR_DATA_TYPE_ES);
	curl_easy_setopt(handle, CURLOPT_HTTPHEADER, pWrkrData->curlHeader);
	curl_easy_setopt(handle, CURLOPT_NOSIGNAL, TRUE);
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, curlResult);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, pWrkrData);
	if (pWrkrData->pData->proxyHost != NULL)
	{
		curl_easy_setopt(handle, CURLOPT_PROXY, pWrkrData->pData->proxyHost);
	}
	if (pWrkrData->pData->proxyPort != 0)
	{
		curl_easy_setopt(handle, CURLOPT_PROXYPORT, pWrkrData->pData->proxyPort);
	}
	if (pWrkrData->pData->authBuf != NULL)
	{
		curl_easy_setopt(handle, CURLOPT_USERPWD, pWrkrData->pData->authBuf);
		curl_easy_setopt(handle, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
	}
	if (pWrkrData->pData->caCertFile)
	{
		curl_easy_setopt(handle, CURLOPT_CAINFO, pWrkrData->pData->caCertFile);
	}
	if (pWrkrData->pData->myCertFile)
	{
		curl_easy_setopt(handle, CURLOPT_SSLCERT, pWrkrData->pData->myCertFile);
	}
	if (pWrkrData->pData->myPrivKeyFile)
	{
		curl_easy_setopt(handle, CURLOPT_SSLKEY, pWrkrData->pData->myPrivKeyFile);
	}
	/* uncomment for in-depth debugging:
	curl_easy_setopt(handle, CURLOPT_VERBOSE, TRUE); */
}

static void ATTR_NONNULL()
curlCheckConnSetup(wrkrInstanceData_t *const pWrkrData)
{
	PTR_ASSERT_SET_TYPE(pWrkrData, WRKR_DATA_TYPE_ES);
	curlSetupCommon(pWrkrData, pWrkrData->curlCheckConnHandle);
}

static void ATTR_NONNULL(1)
curlPostSetup(wrkrInstanceData_t *const pWrkrData)
{
	PTR_ASSERT_SET_TYPE(pWrkrData, WRKR_DATA_TYPE_ES);
	curlSetupCommon(pWrkrData, pWrkrData->curlPostHandle);
	curl_easy_setopt(pWrkrData->curlPostHandle, CURLOPT_POST, 1);
	CURLcode cRet;
	/* Enable TCP keep-alive for this transfer */
	cRet = curl_easy_setopt(pWrkrData->curlPostHandle, CURLOPT_TCP_KEEPALIVE, 1L);
	if (cRet != CURLE_OK)
	{
		DBGPRINTF("omsentinel: curlPostSetup unknown option CURLOPT_TCP_KEEPALIVE\n");
	}
	/* keep-alive idle time to 120 seconds */
	cRet = curl_easy_setopt(pWrkrData->curlPostHandle, CURLOPT_TCP_KEEPIDLE, 120L);
	if (cRet != CURLE_OK)
	{
		DBGPRINTF("omsentinel: curlPostSetup unknown option CURLOPT_TCP_KEEPIDLE\n");
	}
	/* interval time between keep-alive probes: 60 seconds */
	cRet = curl_easy_setopt(pWrkrData->curlPostHandle, CURLOPT_TCP_KEEPINTVL, 60L);
	if (cRet != CURLE_OK)
	{
		DBGPRINTF("omsentinel: curlPostSetup unknown option CURLOPT_TCP_KEEPINTVL\n");
	}
}

static rsRetVal ATTR_NONNULL()
curlSetup(wrkrInstanceData_t *const pWrkrData)
{
	struct curl_slist *slist = NULL;

	DEFiRet;

	slist = curl_slist_append(slist, HTTP_HEADER_CONTENT_JSON);

	if (pWrkrData->pData->headerBuf != NULL)
	{
		slist = curl_slist_append(slist, (char *)pWrkrData->pData->headerBuf);
		CHKmalloc(slist);
	}

	if (pWrkrData->pData->httpHeader)
	{
		slist = curl_slist_append(slist, (char *)pWrkrData->pData->httpHeader);
		CHKmalloc(slist);
	}

	// When sending more than 1Kb, libcurl automatically sends an Except: 100-Continue header
	// and will wait 1s for a response, could make this configurable but for now disable
	slist = curl_slist_append(slist, HTTP_HEADER_EXPECT_EMPTY);
	pWrkrData->curlHeader = slist;
	CHKmalloc(pWrkrData->curlPostHandle = curl_easy_init());
	curlPostSetup(pWrkrData);

	CHKmalloc(pWrkrData->curlCheckConnHandle = curl_easy_init());
	curlCheckConnSetup(pWrkrData);

finalize_it:
	if (iRet != RS_RET_OK && pWrkrData->curlPostHandle != NULL)
	{
		curl_easy_cleanup(pWrkrData->curlPostHandle);
		pWrkrData->curlPostHandle = NULL;
	}
	RETiRet;
}

static void ATTR_NONNULL()
curlCleanup(wrkrInstanceData_t *const pWrkrData)
{
	if (pWrkrData->curlHeader != NULL)
	{
		curl_slist_free_all(pWrkrData->curlHeader);
		pWrkrData->curlHeader = NULL;
	}
	if (pWrkrData->curlCheckConnHandle != NULL)
	{
		curl_easy_cleanup(pWrkrData->curlCheckConnHandle);
		pWrkrData->curlCheckConnHandle = NULL;
	}
	if (pWrkrData->curlPostHandle != NULL)
	{
		curl_easy_cleanup(pWrkrData->curlPostHandle);
		pWrkrData->curlPostHandle = NULL;
	}
}

static void ATTR_NONNULL()
setInstParamDefaults(instanceData *const pData)
{
	pData->dcr = NULL;
	pData->dce = NULL;
	pData->stream_name = NULL;
	pData->baseURL = NULL;
	pData->authorizationHeader = (uchar *)"Authorization: Bearer %s";
	pData->authExp = 0;
	pData->apiRestAuth = NULL;
	pData->authReply = NULL;
	pData->token = NULL;
	pData->authParams = NULL;
	pData->httpHeader = NULL;
	pData->authBuf = NULL;
	pData->client_id = NULL;
	pData->tenant_id = NULL;
	pData->client_secret = NULL;
	pData->scope = (uchar*)"https://monitor.azure.com/.default";
	pData->grant_type = (uchar*)"client_credentials";
	pData->auth_domain= (uchar*)"login.microsoftonline.com";
	pData->restPath = NULL;
	pData->proxyHost = NULL;
	pData->proxyPort = 0;
	pData->maxBatchBytes = 10485760;	// i.e. 10 MB Is the default max message size for AWS API Gateway
	pData->maxBatchSize = 1;		// 100 messages
	pData->compress = 0;			// off
	pData->compressionLevel = -1;		// default compression
	pData->tplName = NULL;
	pData->errorFile = NULL;
	pData->caCertFile = NULL;
	pData->myCertFile = NULL;
	pData->myPrivKeyFile = NULL;
	pData->retryFailures = 0;
	pData->nhttpRetryCodes = 0;
	pData->httpRetryCodes = NULL;
	pData->ratelimitBurst = 20000;
	pData->ratelimitInterval = 600;
	pData->ratelimiter = NULL;
	pData->retryRulesetName = NULL;
	pData->retryRuleset = NULL;
	pData->nIgnorableCodes = 0;
	pData->ignorableCodes = NULL;
	// increment number of instances
	++omsentinelInstancesCnt;
}

BEGINnewActInst struct cnfparamvals *pvals;
	int i;
	int iNumTpls = 1;
	FILE *fp;
	char errStr[1024];
	int compressionLevel = -1;
CODESTARTnewActInst
	if ((pvals = nvlstGetParams(lst, &actpblk, NULL)) == NULL)
	{
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	CHKiRet(createInstance(&pData));
	setInstParamDefaults(pData);

	for (i = 0; i < actpblk.nParams; ++i)
	{
		if (!pvals[i].bUsed)
		{
			continue;
		}
		if (!strcmp(actpblk.descr[i].name, "dce"))
		{
			pData->dce = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
		}
		else if (!strcmp(actpblk.descr[i].name, "errorfile"))
		{
			pData->errorFile = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
		}
		else if (!strcmp(actpblk.descr[i].name, "dcr"))
		{
			pData->dcr = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
		}
		else if (!strcmp(actpblk.descr[i].name, "stream_name"))
		{
			pData->stream_name = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
		}
		else if (!strcmp(actpblk.descr[i].name, "restpath"))
		{
			pData->restPath = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
		}
		else if (!strcmp(actpblk.descr[i].name, "client_secret"))
		{
			pData->client_secret = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
		}
		else if (!strcmp(actpblk.descr[i].name, "client_id"))
		{
			pData->client_id = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
		}
		else if (!strcmp(actpblk.descr[i].name, "tenant_id"))
		{
			pData->tenant_id = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
		}
		else if (!strcmp(actpblk.descr[i].name, "scope"))
		{
			pData->scope = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
		}
		else if (!strcmp(actpblk.descr[i].name, "grant_type"))
		{
			pData->grant_type = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
		}
		else if (!strcmp(actpblk.descr[i].name, "auth_domain"))
		{
			pData->auth_domain = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
		}
		else if (!strcmp(actpblk.descr[i].name, "proxyhost"))
		{
			pData->proxyHost = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
		}
		else if (!strcmp(actpblk.descr[i].name, "proxyport"))
		{
			pData->proxyPort = (int)pvals[i].val.d.n;
		}
		else if (!strcmp(actpblk.descr[i].name, "batch.maxbytes"))
		{
			pData->maxBatchBytes = (size_t)pvals[i].val.d.n;
		}
		else if (!strcmp(actpblk.descr[i].name, "batch.maxsize"))
		{
			pData->maxBatchSize = (size_t)pvals[i].val.d.n;
		}
		else if (!strcmp(actpblk.descr[i].name, "compress"))
		{
			pData->compress = pvals[i].val.d.n;
		}
		else if (!strcmp(actpblk.descr[i].name, "compress.level"))
		{
			compressionLevel = pvals[i].val.d.n;
			if (compressionLevel == -1 || (compressionLevel >= 0 && compressionLevel < 10))
			{
				pData->compressionLevel = compressionLevel;
			}
			else
			{
				LogError(0, NO_ERRCODE, "omsentinel: invalid compress.level %d using default instead,"
							"valid levels are -1 and 0-9",
							compressionLevel);
			}
		}
		else if (!strcmp(actpblk.descr[i].name, "template"))
		{
			pData->tplName = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
		}
		else if (!strcmp(actpblk.descr[i].name, "tls.cacert"))
		{
			pData->caCertFile = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
			fp = fopen((const char *)pData->caCertFile, "r");
			if (fp == NULL)
			{
				rs_strerror_r(errno, errStr, sizeof(errStr));
				LogError(0, RS_RET_NO_FILE_ACCESS,
						"error: 'tls.cacert' file %s couldn't be accessed: %s\n",
						pData->caCertFile, errStr);
			}
			else
			{
				fclose(fp);
			}
		}
		else if (!strcmp(actpblk.descr[i].name, "tls.mycert"))
		{
			pData->myCertFile = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
			fp = fopen((const char *)pData->myCertFile, "r");
			if (fp == NULL)
			{
				rs_strerror_r(errno, errStr, sizeof(errStr));
				LogError(0, RS_RET_NO_FILE_ACCESS,
				"error: 'tls.mycert' file %s couldn't be accessed: %s\n",
				pData->myCertFile, errStr);
			}
			else
			{
				fclose(fp);
			}
		}
		else if (!strcmp(actpblk.descr[i].name, "tls.myprivkey"))
		{
			pData->myPrivKeyFile = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
			fp = fopen((const char *)pData->myPrivKeyFile, "r");
			if (fp == NULL)
			{
				rs_strerror_r(errno, errStr, sizeof(errStr));
				LogError(0, RS_RET_NO_FILE_ACCESS,
					"error: 'tls.myprivkey' file %s couldn't be accessed: %s\n",
					pData->myPrivKeyFile, errStr);
			}
			else
			{
				fclose(fp);
			}
		}
		else if (!strcmp(actpblk.descr[i].name, "httpretrycodes"))
		{
			pData->nhttpRetryCodes = pvals[i].val.d.ar->nmemb;
			// note: use zero as sentinel value
			CHKmalloc(pData->httpRetryCodes = calloc(pvals[i].val.d.ar->nmemb, sizeof(unsigned int)));
			int count = 0;
			for (int j = 0; j < pvals[i].val.d.ar->nmemb; ++j)
			{
				int bSuccess = 0;
				long long n = es_str2num(pvals[i].val.d.ar->arr[j], &bSuccess);
				if (!bSuccess)
				{
					char *cstr = es_str2cstr(pvals[i].val.d.ar->arr[j], NULL);
					LogError(0, RS_RET_NO_FILE_ACCESS,
						"error: 'httpRetryCode' '%s' is not a number - ignored\n", cstr);
					free(cstr);
				}
				else
				{
					pData->httpRetryCodes[count++] = n;
				}
			}
		}
		else if (!strcmp(actpblk.descr[i].name, "retry"))
		{
			pData->retryFailures = pvals[i].val.d.n;
		}
		else if (!strcmp(actpblk.descr[i].name, "retry.ruleset"))
		{
			pData->retryRulesetName = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
		}
		else if (!strcmp(actpblk.descr[i].name, "ratelimit.burst"))
		{
			pData->ratelimitBurst = (unsigned int)pvals[i].val.d.n;
		}
		else if (!strcmp(actpblk.descr[i].name, "ratelimit.interval"))
		{
			pData->ratelimitInterval = (unsigned int)pvals[i].val.d.n;
		}
		else if (!strcmp(actpblk.descr[i].name, "name"))
		{
			pData->statsName = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
		}
		else if (!strcmp(actpblk.descr[i].name, "httpignorablecodes"))
		{
			pData->nIgnorableCodes = pvals[i].val.d.ar->nmemb;
			// note: use zero as sentinel value
			CHKmalloc(pData->ignorableCodes = calloc(pvals[i].val.d.ar->nmemb, sizeof(unsigned int)));
			int count = 0;
			for (int j = 0; j < pvals[i].val.d.ar->nmemb; ++j)
			{
				int bSuccess = 0;
				long long n = es_str2num(pvals[i].val.d.ar->arr[j], &bSuccess);
				if (!bSuccess)
				{
					char *cstr = es_str2cstr(pvals[i].val.d.ar->arr[j], NULL);
					LogError(0, RS_RET_NO_FILE_ACCESS,
						"error: 'httpIgnorableCodes' '%s' is not a number - ignored\n", cstr);
					free(cstr);
				}
				else
				{
					pData->ignorableCodes[count++] = n;
				}
			}
		}
		else
		{
			LogError(0, RS_RET_INTERNAL_ERROR, "omsentinel: program error, "
				"non-handled param '%s'",
				actpblk.descr[i].name);
		}
	}

	if (pData->proxyHost == NULL)
	{
		const char *http_proxy;
		if ((http_proxy = getenv("http_proxy")) == NULL)
		{
			http_proxy = getenv("HTTP_PROXY");
		}
		if (http_proxy != NULL)
		{
			pData->proxyHost = ustrdup(http_proxy);
		}
	}

	DBGPRINTF("omsentinel: requesting %d templates\n", iNumTpls);
	CODE_STD_STRING_REQUESTnewActInst(iNumTpls)

		CHKiRet(OMSRsetEntry(*ppOMSR, 0,
			(uchar *)strdup((pData->tplName == NULL) ? " StdJSONFmt" : (char *)pData->tplName),
			OMSR_NO_RQD_TPL_OPTS));

	if (pData->retryFailures)
	{
		CHKiRet(ratelimitNew(&pData->ratelimiter, "omsentinel", NULL));
		ratelimitSetLinuxLike(pData->ratelimiter, pData->ratelimitInterval, pData->ratelimitBurst);
		ratelimitSetNoTimeCache(pData->ratelimiter);
	}

	if (!pData->statsName)
	{
		uchar pszAName[64];
		snprintf((char *)pszAName, sizeof(pszAName), "omsentinel-%d", omsentinelInstancesCnt);
		pData->statsName = ustrdup(pszAName);
	}
	// instantiate the stats object and add the counters
	CHKiRet(statsobj.Construct(&pData->stats));
	CHKiRet(statsobj.SetName(pData->stats, (uchar *)pData->statsName));
	CHKiRet(statsobj.SetOrigin(pData->stats, (uchar *)"omsentinel"));

	STATSCOUNTER_INIT(pData->ctrHttpRequestsCount, pData->mutCtrHttpRequestsCount);
	CHKiRet(statsobj.AddCounter(pData->stats, (uchar *)"requests.count",
					ctrType_IntCtr, CTR_FLAG_RESETTABLE, &pData->ctrHttpRequestsCount));

	STATSCOUNTER_INIT(pData->ctrHttpRequestsStatus0xx, pData->mutCtrHttpRequestsStatus0xx);
	CHKiRet(statsobj.AddCounter(pData->stats, (uchar *)"requests.status.0xx",
					ctrType_IntCtr, CTR_FLAG_RESETTABLE, &pData->ctrHttpRequestsStatus0xx));

	STATSCOUNTER_INIT(pData->ctrHttpRequestsStatus1xx, pData->mutCtrHttpRequestsStatus1xx);
	CHKiRet(statsobj.AddCounter(pData->stats, (uchar *)"requests.status.1xx",
					ctrType_IntCtr, CTR_FLAG_RESETTABLE, &pData->ctrHttpRequestsStatus1xx));

	STATSCOUNTER_INIT(pData->ctrHttpRequestsStatus2xx, pData->mutCtrHttpRequestsStatus2xx);
	CHKiRet(statsobj.AddCounter(pData->stats, (uchar *)"requests.status.2xx",
					ctrType_IntCtr, CTR_FLAG_RESETTABLE, &pData->ctrHttpRequestsStatus2xx));

	STATSCOUNTER_INIT(pData->ctrHttpRequestsStatus3xx, pData->mutCtrHttpRequestsStatus3xx);
	CHKiRet(statsobj.AddCounter(pData->stats, (uchar *)"requests.status.3xx",
					ctrType_IntCtr, CTR_FLAG_RESETTABLE, &pData->ctrHttpRequestsStatus3xx));

	STATSCOUNTER_INIT(pData->ctrHttpRequestsStatus4xx, pData->mutCtrHttpRequestsStatus4xx);
	CHKiRet(statsobj.AddCounter(pData->stats, (uchar *)"requests.status.4xx",
					ctrType_IntCtr, CTR_FLAG_RESETTABLE, &pData->ctrHttpRequestsStatus4xx));

	STATSCOUNTER_INIT(pData->ctrHttpRequestsStatus5xx, pData->mutCtrHttpRequestsStatus5xx);
	CHKiRet(statsobj.AddCounter(pData->stats, (uchar *)"requests.status.5xx",
					ctrType_IntCtr, CTR_FLAG_RESETTABLE, &pData->ctrHttpRequestsStatus5xx));

	STATSCOUNTER_INIT(pData->httpRequestsBytes, pData->mutHttpRequestsBytes);
	CHKiRet(statsobj.AddCounter(pData->stats, (uchar *)"requests.bytes",
					ctrType_IntCtr, CTR_FLAG_RESETTABLE, &pData->httpRequestsBytes));

	STATSCOUNTER_INIT(pData->httpRequestsTimeMs, pData->mutHttpRequestsTimeMs);
	CHKiRet(statsobj.AddCounter(pData->stats, (uchar *)"requests.time_ms",
					ctrType_IntCtr, CTR_FLAG_RESETTABLE, &pData->httpRequestsTimeMs));

	CHKiRet(statsobj.ConstructFinalize(pData->stats));

	/* node created, let's add to list of instance configs for the module */
	if (loadModConf->tail == NULL)
	{
		loadModConf->tail = loadModConf->root = pData;
	}
	else
	{
		loadModConf->tail->next = pData;
		loadModConf->tail = pData;
	}

CODE_STD_FINALIZERnewActInst
	cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst

BEGINbeginCnfLoad
CODESTARTbeginCnfLoad
	loadModConf = pModConf;
	pModConf->pConf = pConf;
	pModConf->root = pModConf->tail = NULL;
ENDbeginCnfLoad

BEGINendCnfLoad
CODESTARTendCnfLoad
	loadModConf = NULL; /* done loading */
ENDendCnfLoad

BEGINcheckCnf
	instanceConf_t *inst;
CODESTARTcheckCnf

	for (inst = pModConf->root; inst != NULL; inst = inst->next)
	{
		ruleset_t *pRuleset;
		rsRetVal localRet;

		if (inst->retryRulesetName)
		{
			localRet = ruleset.GetRuleset(pModConf->pConf, &pRuleset, inst->retryRulesetName);
			if (localRet == RS_RET_NOT_FOUND)
			{
				LogError(0, localRet, "omsentinel: retry.ruleset '%s' not found - "
						"no retry ruleset will be used",
						inst->retryRulesetName);
			}
			else
			{
				inst->retryRuleset = pRuleset;
			}
		}
	}
ENDcheckCnf

BEGINactivateCnf
CODESTARTactivateCnf
ENDactivateCnf

BEGINfreeCnf
CODESTARTfreeCnf
ENDfreeCnf

// HUP handling for the instance...
BEGINdoHUP
CODESTARTdoHUP
	pthread_mutex_lock(&pData->mutErrFile);
	if (pData->fdErrFile != -1)
	{
		close(pData->fdErrFile);
		pData->fdErrFile = -1;
	}
	pthread_mutex_unlock(&pData->mutErrFile);
ENDdoHUP

// HUP handling for the worker...
BEGINdoHUPWrkr
CODESTARTdoHUPWrkr
ENDdoHUPWrkr

BEGINmodExit
CODESTARTmodExit
	if (pInputName != NULL)
	{
		prop.Destruct(&pInputName);
	}
	curl_global_cleanup();
	objRelease(prop, CORE_COMPONENT);
	objRelease(ruleset, CORE_COMPONENT);
	objRelease(statsobj, CORE_COMPONENT);
	statsobj.Destruct(&httpStats);
ENDmodExit

NO_LEGACY_CONF_parseSelectorAct

BEGINqueryEtryPt CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
CODEqueryEtryPt_STD_OMOD8_QUERIES
CODEqueryEtryPt_IsCompatibleWithFeature_IF_OMOD_QUERIES
CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES
CODEqueryEtryPt_doHUP
CODEqueryEtryPt_doHUPWrkr		/* Load the worker HUP handling code */
CODEqueryEtryPt_TXIF_OMOD_QUERIES	/* we support the transactional interface! */
CODEqueryEtryPt_STD_CONF2_QUERIES
ENDqueryEtryPt

BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(prop, CORE_COMPONENT));
	CHKiRet(objUse(ruleset, CORE_COMPONENT));
	CHKiRet(objUse(statsobj, CORE_COMPONENT));

	CHKiRet(statsobj.Construct(&httpStats));
	CHKiRet(statsobj.SetName(httpStats, (uchar *)"omsentinel"));
	CHKiRet(statsobj.SetOrigin(httpStats, (uchar *)"omsentinel"));

	STATSCOUNTER_INIT(ctrMessagesSubmitted, mutCtrMessagesSubmitted);
	CHKiRet(statsobj.AddCounter(httpStats, (uchar *)"messages.submitted",
					ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrMessagesSubmitted));

	STATSCOUNTER_INIT(ctrMessagesSuccess, mutCtrMessagesSuccess);
	CHKiRet(statsobj.AddCounter(httpStats, (uchar *)"messages.success",
					ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrMessagesSuccess));

	STATSCOUNTER_INIT(ctrMessagesFail, mutCtrMessagesFail);
	CHKiRet(statsobj.AddCounter(httpStats, (uchar *)"messages.fail",
					ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrMessagesFail));

	STATSCOUNTER_INIT(ctrMessagesRetry, mutCtrMessagesRetry);
	CHKiRet(statsobj.AddCounter(httpStats, (uchar *)"messages.retry",
					ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrMessagesRetry));

	STATSCOUNTER_INIT(ctrHttpRequestCount, mutCtrHttpRequestCount);
	CHKiRet(statsobj.AddCounter(httpStats, (uchar *)"request.count",
					ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrHttpRequestCount));

	STATSCOUNTER_INIT(ctrHttpRequestSuccess, mutCtrHttpRequestSuccess);
	CHKiRet(statsobj.AddCounter(httpStats, (uchar *)"request.success",
					ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrHttpRequestSuccess));

	STATSCOUNTER_INIT(ctrHttpRequestFail, mutCtrHttpRequestFail);
	CHKiRet(statsobj.AddCounter(httpStats, (uchar *)"request.fail",
					ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrHttpRequestFail));

	STATSCOUNTER_INIT(ctrHttpStatusSuccess, mutCtrHttpStatusSuccess);
	CHKiRet(statsobj.AddCounter(httpStats, (uchar *)"request.status.success",
					ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrHttpStatusSuccess));

	STATSCOUNTER_INIT(ctrHttpStatusFail, mutCtrHttpStatusFail);
	CHKiRet(statsobj.AddCounter(httpStats, (uchar *)"request.status.fail",
					ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrHttpStatusFail));

	CHKiRet(statsobj.ConstructFinalize(httpStats));

	if (curl_global_init(CURL_GLOBAL_ALL) != 0)
	{
		LogError(0, RS_RET_OBJ_CREATION_FAILED, "CURL fail. -http disabled");
		ABORT_FINALIZE(RS_RET_OBJ_CREATION_FAILED);
	}

	CHKiRet(prop.Construct(&pInputName));
	CHKiRet(prop.SetString(pInputName, UCHAR_CONSTANT("omsentinel"), sizeof("omsentinel") - 1));
	CHKiRet(prop.ConstructFinalize(pInputName));
ENDmodInit

/* vi:set ai: */
