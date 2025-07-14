/* omclickhouse.c
 * This is the https://clickhouse.yandex/ output module.
 *
 * Copyright 2018 Pascal Withopf and Adiscon GmbH.
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
#include <json.h>
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include "statsobj.h"
#include "cfsysline.h"
#include "unicode-helper.h"
#include "obj-types.h"
#include "ratelimit.h"
#include "ruleset.h"

#ifndef O_LARGEFILE
#  define O_LARGEFILE 0
#endif

MODULE_TYPE_OUTPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME("omclickhouse")

/* internal structures */
DEF_OMOD_STATIC_DATA;
DEFobjCurrIf(statsobj)
DEFobjCurrIf(prop)
DEFobjCurrIf(ruleset)

statsobj_t *indexStats;
STATSCOUNTER_DEF(indexSubmit, mutIndexSubmit)
STATSCOUNTER_DEF(indexHTTPFail, mutIndexHTTPFail)
STATSCOUNTER_DEF(indexHTTPReqFail, mutIndexHTTPReqFail)
STATSCOUNTER_DEF(indexFail, mutIndexFail)
STATSCOUNTER_DEF(indexSuccess, mutIndexSuccess)



typedef struct curl_slist HEADER;
typedef struct instanceConf_s {
	uchar *serverBaseUrl;
	int port;
	uchar *user;
	uchar *pwd;
	long healthCheckTimeout;
	long timeout;
	uchar *authBuf;
	uchar *tplName;
	sbool useHttps;
	sbool allowUnsignedCerts;
	sbool skipVerifyHost;
	int fdErrFile;
	uchar *errorFile;
	sbool bulkmode;
	size_t maxbytes;
	uchar *caCertFile;
	uchar *myCertFile;
	uchar *myPrivKeyFile;
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
	CURL	*curlPostHandle;	/* libcurl session handle for posting data to the server */
	HEADER	*curlHeader;	/* json POST request info */
	CURL	*curlCheckConnHandle;	/* libcurl session handle for checking the server connection */
	int replyLen;
	char *reply;
	uchar *restURL;
	struct {
		es_str_t *data;
		int nmemb;	/* number of messages in batch (for statistics counting) */
	} batch;
	sbool insertErrorSent;  /* needed for insert error message */
} wrkrInstanceData_t;

/* tables for interfacing with the v6 config system */
/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {
	{ "server", eCmdHdlrGetWord, 0 },
	{ "port", eCmdHdlrInt, 0 },
	{ "user", eCmdHdlrGetWord, 0 },
	{ "pwd", eCmdHdlrGetWord, 0 },
	{ "healthchecktimeout", eCmdHdlrInt, 0 },
	{ "timeout", eCmdHdlrInt, 0 },
	{ "template", eCmdHdlrGetWord, 0 },
	{ "usehttps", eCmdHdlrBinary, 0 },
	{ "allowunsignedcerts", eCmdHdlrBinary, 0 },
	{ "skipverifyhost", eCmdHdlrBinary, 0 },
	{ "errorfile", eCmdHdlrGetWord, 0 },
	{ "bulkmode", eCmdHdlrBinary, 0 },
	{ "maxbytes", eCmdHdlrSize, 0 },
	{ "tls.cacert", eCmdHdlrString, 0 },
	{ "tls.mycert", eCmdHdlrString, 0 },
	{ "tls.myprivkey", eCmdHdlrString, 0 }
};
static struct cnfparamblk actpblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(actpdescr)/sizeof(struct cnfparamdescr),
	  actpdescr
	};

static rsRetVal curlSetup(wrkrInstanceData_t *pWrkrData);

BEGINcreateInstance
CODESTARTcreateInstance;
	pData->fdErrFile = -1;
	pData->caCertFile = NULL;
	pData->myCertFile = NULL;
	pData->myPrivKeyFile = NULL;
ENDcreateInstance

BEGINcreateWrkrInstance
CODESTARTcreateWrkrInstance;
	pWrkrData->curlHeader = NULL;
	pWrkrData->curlPostHandle = NULL;
	pWrkrData->curlCheckConnHandle = NULL;
	pWrkrData->restURL = NULL;
	if(pData->bulkmode) {
		if((pWrkrData->batch.data = es_newStr(1024)) == NULL) {
			LogError(0, RS_RET_OUT_OF_MEMORY,
				"omclickhouse: error creating batch string "
			        "turned off bulk mode\n");
			pData->bulkmode = 0; /* at least it works */
		}
	}
	pWrkrData->insertErrorSent = 0;

	iRet = curlSetup(pWrkrData);
ENDcreateWrkrInstance

BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature;
	if(eFeat == sFEATURERepeatedMsgReduction)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature

BEGINfreeInstance
CODESTARTfreeInstance;
	free(pData->serverBaseUrl);
	free(pData->user);
	free(pData->pwd);
	free(pData->authBuf);
	if(pData->fdErrFile != -1)
		close(pData->fdErrFile);
	free(pData->errorFile);
	free(pData->tplName);
	free(pData->caCertFile);
	free(pData->myCertFile);
	free(pData->myPrivKeyFile);
ENDfreeInstance

BEGINfreeWrkrInstance
CODESTARTfreeWrkrInstance;
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
CODESTARTdbgPrintInstInfo;
	dbgprintf("omclickhouse\n");
	dbgprintf("\tserver='%s'\n", pData->serverBaseUrl);
	dbgprintf("\tport='%d'\n", pData->port);
	dbgprintf("\tuser='%s'\n", pData->user);
	dbgprintf("\tpwd='%s'\n", pData->pwd);
	dbgprintf("\thealthCheckTimeout=%lu\n", pData->healthCheckTimeout);
	dbgprintf("\ttimeout=%lu\n", pData->timeout);
	dbgprintf("\ttemplate='%s'\n", pData->tplName);
	dbgprintf("\tusehttps='%d'\n", pData->useHttps);
	dbgprintf("\tallowunsignedcerts='%d'\n", pData->allowUnsignedCerts);
	dbgprintf("\tskipverifyhost='%d'\n", pData->skipVerifyHost);
	dbgprintf("\terrorFile='%s'\n", pData->errorFile);
	dbgprintf("\tbulkmode='%d'\n", pData->bulkmode);
	dbgprintf("\tmaxbytes='%zu'\n", pData->maxbytes);
	dbgprintf("\ttls.cacert='%s'\n", pData->caCertFile);
	dbgprintf("\ttls.mycert='%s'\n", pData->myCertFile);
	dbgprintf("\ttls.myprivkey='%s'\n", pData->myPrivKeyFile);
ENDdbgPrintInstInfo


/* checks if connection to clickhouse can be established
 */
static rsRetVal ATTR_NONNULL()
checkConn(wrkrInstanceData_t *const pWrkrData)
{
	CURL *curl;
	CURLcode res;
	char errbuf[CURL_ERROR_SIZE] = "";
	const char* healthCheckMessage ="SELECT 1";
	DEFiRet;

	pWrkrData->reply = NULL;
	pWrkrData->replyLen = 0;
	curl = pWrkrData->curlCheckConnHandle;
	

	curl_easy_setopt(curl, CURLOPT_URL, pWrkrData->restURL);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, healthCheckMessage);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(healthCheckMessage));
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
	res = curl_easy_perform(curl);

	if (res == CURLE_OK) {
		DBGPRINTF("omclickhouse: checkConn completed with success\n");
		ABORT_FINALIZE(RS_RET_OK);
	}

	DBGPRINTF("omclickhouse: checkConn failed: %s\n",
		curl_easy_strerror(res));

	LogMsg(0, RS_RET_SUSPENDED, LOG_WARNING,
		"omclickhouse: checkConn failed.");
	ABORT_FINALIZE(RS_RET_SUSPENDED);

finalize_it:
	free(pWrkrData->reply);
	pWrkrData->reply = NULL; /* don't leave dangling pointer */
	RETiRet;
}


BEGINtryResume
CODESTARTtryResume;
	dbgprintf("omclickhouse: tryResume called\n");
	iRet = checkConn(pWrkrData);
ENDtryResume


/*
 * Dumps entire bulk request and response in error log
 */
static rsRetVal
getDataErrorDefault(wrkrInstanceData_t *pWrkrData, char *reply, uchar *reqmsg, char **rendered)
{
	DEFiRet;
	fjson_object *req=NULL;
	fjson_object *errRoot=NULL;

	if((req=fjson_object_new_object()) == NULL) ABORT_FINALIZE(RS_RET_ERR);
	fjson_object_object_add(req, "url", fjson_object_new_string((char*)pWrkrData->restURL));
	fjson_object_object_add(req, "postdata", fjson_object_new_string((char*)reqmsg));

	if((errRoot=fjson_object_new_object()) == NULL) ABORT_FINALIZE(RS_RET_ERR);
	fjson_object_object_add(errRoot, "request", req);
	fjson_object_object_add(errRoot, "reply", fjson_object_new_string(reply));
	*rendered = strdup((char*)fjson_object_to_json_string(errRoot));

	req=NULL;
	fjson_object_put(errRoot);

	finalize_it:
		fjson_object_put(req);
		RETiRet;
}


/* write data error request/replies to separate error file
 * Note: we open the file but never close it before exit. If it
 * needs to be closed, HUP must be sent.
 */
static rsRetVal ATTR_NONNULL()
writeDataError(wrkrInstanceData_t *const pWrkrData, uchar *const reqmsg)
{
	DEFiRet;
	instanceData *pData = pWrkrData->pData;
	char *rendered = pWrkrData->reply;
	size_t toWrite;
	ssize_t wrRet;

	if(pData->errorFile == NULL) {
		dbgprintf("omclickhouse: no local error logger defined - "
		          "ignoring ClickHouse error information\n");
		FINALIZE;
	}


	if(pData->fdErrFile == -1) {
		pData->fdErrFile = open((char*)pData->errorFile,
					O_WRONLY|O_CREAT|O_APPEND|O_LARGEFILE|O_CLOEXEC,
					S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
		if(pData->fdErrFile == -1) {
			LogError(errno, RS_RET_ERR, "omclickhouse: error opening error file %s",
				pData->errorFile);
			ABORT_FINALIZE(RS_RET_ERR);
		}
	}

	if(getDataErrorDefault(pWrkrData, pWrkrData->reply, reqmsg, &rendered) != RS_RET_OK) {
		ABORT_FINALIZE(RS_RET_ERR);
	}

	/* we do not do real error-handling on the err file, as this finally complicates
	 * things way to much.
	 */
	dbgprintf("omclickhouse: message sent: '%s'\n", reqmsg);
	dbgprintf("omclickhouse: error record: '%s'\n", rendered);
	toWrite = strlen(rendered) + 1;
	/* Note: we overwrite the '\0' terminator with '\n' -- so we avoid
	 * caling malloc() -- write() does NOT need '\0'!
	 */
	rendered[toWrite-1] = '\n'; /* NO LONGER A STRING! */
	wrRet = write(pData->fdErrFile, rendered, toWrite);
	if(wrRet != (ssize_t) toWrite) {
		LogError(errno, RS_RET_IO_ERROR,
			"omclickhouse: error writing error file %s, write returned %lld",
			pData->errorFile, (long long) wrRet);
	}

finalize_it:
	RETiRet;
}


static rsRetVal
checkResult(wrkrInstanceData_t *pWrkrData, uchar *reqmsg)
{
	DEFiRet;

	if((strstr(pWrkrData->reply, " = DB::Exception" ) != NULL)
		|| (strstr(pWrkrData->reply, "DB::NetException" ) != NULL)
		|| (strstr(pWrkrData->reply, "DB::ParsingException" ) != NULL)) {
		dbgprintf("omclickhouse: action failed with error: %s\n", pWrkrData->reply);
		iRet = RS_RET_DATAFAIL;
	}

	if(iRet == RS_RET_DATAFAIL) {
		STATSCOUNTER_INC(indexFail, mutIndexFail);
		writeDataError(pWrkrData, reqmsg);
		iRet = RS_RET_OK; /* we have handled the problem! */
	}


	if(iRet != RS_RET_OK) {
		STATSCOUNTER_INC(indexFail, mutIndexFail);
	}
	RETiRet;
}

static rsRetVal ATTR_NONNULL(1)
setPostURL(wrkrInstanceData_t *const pWrkrData)
{
	char* baseUrl;
	es_str_t *url;
	DEFiRet;
	instanceData *const pData = pWrkrData->pData;

	baseUrl = (char*)pData->serverBaseUrl;
	url = es_newStrFromCStr(baseUrl, strlen(baseUrl));
	if (url == NULL) {
		LogError(0, RS_RET_OUT_OF_MEMORY,
			"omclickhouse: error allocating new estr for POST url.");
		ABORT_FINALIZE(RS_RET_ERR);
	}

	if(pWrkrData->restURL != NULL)
		free(pWrkrData->restURL);

	pWrkrData->restURL = (uchar*)es_str2cstr(url, NULL);
	curl_easy_setopt(pWrkrData->curlPostHandle, CURLOPT_URL, pWrkrData->restURL);
	dbgprintf("omclickhouse: using REST URL: '%s'\n", pWrkrData->restURL);

finalize_it:
	if (url != NULL)
		es_deleteStr(url);
	RETiRet;
}


/* this method computes the next data set to be added to the batch
 * returns the expected size of adding the next message into the
 * batched request to clickhouse
 */
static size_t
computeBulkMessage(const wrkrInstanceData_t *const pWrkrData,
	const uchar *const message, char **newMessage)
{
	size_t r = 0;
	char *v;
	if (pWrkrData->batch.nmemb != 0
	&& (v = strstr((const char *)message, "VALUES")) != NULL
	&& (v = strchr(v, '(')) != NULL
	) {
		*newMessage = v;
		r = strlen(*newMessage);
	} else {
		*newMessage = (char*)message;
		r = strlen(*newMessage);
	}
	dbgprintf("omclickhouse: computeBulkMessage: new message part: %s\n", *newMessage);

	return r;
}


/* This method builds the batch, that will be submitted.
 */
static rsRetVal
buildBatch(wrkrInstanceData_t *pWrkrData, char *message)
{
	DEFiRet;
	int length = strlen(message);
	int r;

	r = es_addBuf(&pWrkrData->batch.data, message, length);
	if(r != 0) {
		LogError(0, RS_RET_ERR, "omclickhouse: growing batch failed with code %d", r);
		ABORT_FINALIZE(RS_RET_ERR);
	}
	++pWrkrData->batch.nmemb;
	iRet = RS_RET_OK;

finalize_it:
	RETiRet;
}


static void ATTR_NONNULL()
initializeBatch(wrkrInstanceData_t *pWrkrData)
{
	es_emptyStr(pWrkrData->batch.data);
	pWrkrData->batch.nmemb = 0;
}


static rsRetVal ATTR_NONNULL(1, 2)
curlPost(wrkrInstanceData_t *pWrkrData, uchar *message, int msglen, const int nmsgs)
{
	CURLcode code;
	CURL *const curl = pWrkrData->curlPostHandle;
	char errbuf[CURL_ERROR_SIZE] = "";
	DEFiRet;

	if(!strstr((char*)message, "INSERT INTO") && !pWrkrData->insertErrorSent) {
		indexHTTPFail += nmsgs;
		LogError(0, RS_RET_ERR, "omclickhouse: Message is no Insert query: "
				"Message suspended: %s", (char*)message);
		pWrkrData->insertErrorSent = 1;
		ABORT_FINALIZE(RS_RET_ERR);
	}

	pWrkrData->reply = NULL;
	pWrkrData->replyLen = 0;

	CHKiRet(setPostURL(pWrkrData));

	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, (char *)message);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, msglen);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
	code = curl_easy_perform(curl);
	dbgprintf("curl returned %lld\n", (long long) code);
	if (code != CURLE_OK && code != CURLE_HTTP_RETURNED_ERROR) {
		STATSCOUNTER_INC(indexHTTPReqFail, mutIndexHTTPReqFail);
		indexHTTPFail += nmsgs;
		LogError(0, RS_RET_SUSPENDED,
			"omclickhouse: we are suspending ourselfs due "
			"to server failure %lld: %s", (long long) code, errbuf);
		ABORT_FINALIZE(RS_RET_SUSPENDED);
	}

	if(pWrkrData->reply == NULL) {
		dbgprintf("omclickhouse: pWrkrData reply==NULL, replyLen = '%d'\n",
			pWrkrData->replyLen);
		STATSCOUNTER_INC(indexSuccess, mutIndexSuccess);
	} else {
		dbgprintf("omclickhouse: pWrkrData replyLen = '%d'\n", pWrkrData->replyLen);
		if(pWrkrData->replyLen > 0) {
			pWrkrData->reply[pWrkrData->replyLen] = '\0';
			/* Append 0 Byte if replyLen is above 0 - byte has been reserved in malloc */
		}
		dbgprintf("omclickhouse: pWrkrData reply: '%s'\n", pWrkrData->reply);
		CHKiRet(checkResult(pWrkrData, message));
	}

finalize_it:
	free(pWrkrData->reply);
	pWrkrData->reply = NULL; /* don't leave dangling pointer */
	RETiRet;
}


static rsRetVal
submitBatch(wrkrInstanceData_t *pWrkrData)
{
	char *cstr = NULL;
	DEFiRet;

	cstr = es_str2cstr(pWrkrData->batch.data, NULL);
	dbgprintf("omclickhouse: submitBatch, batch: '%s'\n", cstr);

	CHKiRet(curlPost(pWrkrData, (uchar*) cstr, strlen(cstr), pWrkrData->batch.nmemb));

finalize_it:
	free(cstr);
	RETiRet;
}


BEGINbeginTransaction
CODESTARTbeginTransaction;
	if(!pWrkrData->pData->bulkmode) {
		FINALIZE;
	}

	initializeBatch(pWrkrData);
finalize_it:
ENDbeginTransaction


BEGINdoAction
	char *batchPart = NULL;
CODESTARTdoAction;
	dbgprintf("CODESTARTdoAction: entered\n");
	STATSCOUNTER_INC(indexSubmit, mutIndexSubmit);

	if(pWrkrData->pData->bulkmode) {
		const size_t nBytes = computeBulkMessage(pWrkrData, ppString[0], &batchPart);
		dbgprintf("pascal: doAction: message: %s\n", batchPart);

		/* If max bytes is set and this next message will put us over the limit,
		* submit the current buffer and reset */
		if(pWrkrData->pData->maxbytes > 0
			&& es_strlen(pWrkrData->batch.data) + nBytes > pWrkrData->pData->maxbytes) {

			dbgprintf("omclickhouse: maxbytes limit reached, submitting partial "
				"batch of %d elements.\n", pWrkrData->batch.nmemb);
			CHKiRet(submitBatch(pWrkrData));
			initializeBatch(pWrkrData);
			batchPart = (char*)ppString[0];
		}

		CHKiRet(buildBatch(pWrkrData, batchPart));

		iRet = pWrkrData->batch.nmemb == 1 ? RS_RET_PREVIOUS_COMMITTED : RS_RET_DEFER_COMMIT;
	} else {
		CHKiRet(curlPost(pWrkrData, ppString[0], strlen((char*)ppString[0]), 1));
	}
finalize_it:
ENDdoAction


BEGINendTransaction
CODESTARTendTransaction;
/* End Transaction only if batch data is not empty */
	if (pWrkrData->batch.data != NULL && pWrkrData->batch.nmemb > 0) {
		CHKiRet(submitBatch(pWrkrData));
	} else {
		dbgprintf("omclickhouse: endTransaction, pWrkrData->batch.data is NULL, "
			"nothing to send. \n");
	}
finalize_it:
ENDendTransaction

static void ATTR_NONNULL()
setInstParamDefaults(instanceData *const pData)
{
	pData->serverBaseUrl = NULL;
	pData->port = 8123;
	pData->user = NULL;
	pData->pwd = NULL;
	pData->healthCheckTimeout = 3500;
	pData->timeout = 0;
	pData->authBuf = NULL;
	pData->tplName = NULL;
	pData->useHttps = 1;
	pData->allowUnsignedCerts = 1;
	pData->skipVerifyHost = 0;
	pData->errorFile = NULL;
	pData->bulkmode = 1;
	pData->maxbytes = 104857600; //100MB
	pData->caCertFile = NULL;
	pData->myCertFile = NULL;
	pData->myPrivKeyFile = NULL;
}

/* POST result string ... useful for debugging */
static size_t
curlResult(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	char *p = (char *)ptr;
	wrkrInstanceData_t *pWrkrData = (wrkrInstanceData_t*) userdata;
	char *buf;
	size_t newlen;
	newlen = pWrkrData->replyLen + size*nmemb;
	if((buf = realloc(pWrkrData->reply, newlen + 1)) == NULL) {
		LogError(errno, RS_RET_ERR, "omclickhouse: realloc failed in curlResult");
		return 0; /* abort due to failure */
	}
	memcpy(buf+pWrkrData->replyLen, p, size*nmemb);
	pWrkrData->replyLen = newlen;
	pWrkrData->reply = buf;
	return size*nmemb;
}

static void ATTR_NONNULL()
curlSetupCommon(wrkrInstanceData_t *const pWrkrData, CURL *const handle)
{
	curl_easy_setopt(handle, CURLOPT_HTTPHEADER, pWrkrData->curlHeader);
	curl_easy_setopt(handle, CURLOPT_NOSIGNAL, TRUE);
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, curlResult);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, pWrkrData);
	if(pWrkrData->pData->allowUnsignedCerts)
		curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, FALSE);
	if(pWrkrData->pData->skipVerifyHost)
		curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, FALSE);
	if(pWrkrData->pData->authBuf != NULL) {
		curl_easy_setopt(handle, CURLOPT_USERPWD, pWrkrData->pData->authBuf);
		curl_easy_setopt(handle, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
	}

	if(pWrkrData->pData->caCertFile) {
		curl_easy_setopt(handle, CURLOPT_CAINFO, pWrkrData->pData->caCertFile);
	}
	if(pWrkrData->pData->myCertFile) {
		curl_easy_setopt(handle, CURLOPT_SSLCERT, pWrkrData->pData->myCertFile);
	}
	if(pWrkrData->pData->myPrivKeyFile) {
		curl_easy_setopt(handle, CURLOPT_SSLKEY, pWrkrData->pData->myPrivKeyFile);
	}
	/* uncomment for in-dept debuggung:
	curl_easy_setopt(handle, CURLOPT_VERBOSE, TRUE); */
}


static void ATTR_NONNULL()
curlCheckConnSetup(wrkrInstanceData_t *const pWrkrData)
{
	curlSetupCommon(pWrkrData, pWrkrData->curlCheckConnHandle);
	curl_easy_setopt(pWrkrData->curlCheckConnHandle,
		CURLOPT_TIMEOUT_MS, pWrkrData->pData->healthCheckTimeout);
}


static void ATTR_NONNULL(1)
curlPostSetup(wrkrInstanceData_t *const pWrkrData)
{
	curlSetupCommon(pWrkrData, pWrkrData->curlPostHandle);
	curl_easy_setopt(pWrkrData->curlPostHandle, CURLOPT_POST, 1);
	if(pWrkrData->pData->timeout) {
		curl_easy_setopt(pWrkrData->curlPostHandle,
			CURLOPT_TIMEOUT_MS, pWrkrData->pData->timeout);
	}
}

#define CONTENT_JSON "Content-Type: application/json; charset=utf-8"

static rsRetVal ATTR_NONNULL()
curlSetup(wrkrInstanceData_t *const pWrkrData)
{
	DEFiRet;
	pWrkrData->curlHeader = curl_slist_append(NULL, CONTENT_JSON);
	CHKmalloc(pWrkrData->curlPostHandle = curl_easy_init());
	curlPostSetup(pWrkrData);

	CHKmalloc(pWrkrData->curlCheckConnHandle = curl_easy_init());
	curlCheckConnSetup(pWrkrData);

finalize_it:
	if(iRet != RS_RET_OK && pWrkrData->curlPostHandle != NULL) {
		curl_easy_cleanup(pWrkrData->curlPostHandle);
		pWrkrData->curlPostHandle = NULL;
	}
	RETiRet;
}

static rsRetVal
computeAuthHeader(char* user, char* pwd, uchar** authBuf)
{
	DEFiRet;
	int r;

	es_str_t* auth = es_newStr(1024);
	if (auth == NULL) {
		LogError(0, RS_RET_OUT_OF_MEMORY,
			"omclickhouse: failed to allocate es_str auth for auth header construction");
		ABORT_FINALIZE(RS_RET_ERR);
	}

	r = es_addBuf(&auth, user, strlen(user));
	if(r == 0)
		r = es_addChar(&auth, ':');
	if(r == 0 && pwd != NULL)
		r = es_addBuf(&auth, pwd, strlen(pwd));
	if(r == 0)
		*authBuf = (uchar*) es_str2cstr(auth, NULL);

	if (r != 0 || *authBuf == NULL) {
		LogError(0, RS_RET_ERR, "omclickhouse: failed to build auth header\n");
		ABORT_FINALIZE(RS_RET_ERR);
	}

finalize_it:
	if (auth != NULL)
		es_deleteStr(auth);
	RETiRet;
}

/* Build basic URL part, which includes hostname, port, user, pwd as follows:
 * http://user:pwd@hostname:port/ based on a server param
 * Newly creates a cstr for this purpose.
 */
static rsRetVal
computeBaseUrl(const char* server, const int port, const sbool useHttps, instanceData *pData)
{
#	define SCHEME_HTTPS "https://"
#	define SCHEME_HTTP "http://"

	char portBuf[64];
	int r = 0;
	const char *host = server;
	DEFiRet;

	assert(server[strlen(server)-1] != '/');

	es_str_t *urlBuf = es_newStr(256);
	if (urlBuf == NULL) {
		LogError(0, RS_RET_OUT_OF_MEMORY,
		"omclickhouse: failed to allocate es_str urlBuf in computeBaseUrl");
		ABORT_FINALIZE(RS_RET_ERR);
	}

	/* Find where the hostname/ip of the server starts. If the scheme is not specified
	  in the uri, start the buffer with a scheme corresponding to the useHttps parameter.
	*/
	if(strcasestr(server, SCHEME_HTTP)) {
		host = server + strlen(SCHEME_HTTP);
	} else if(strcasestr(server, SCHEME_HTTPS)) {
		host = server + strlen(SCHEME_HTTPS);
	} else {
		r = useHttps ? es_addBuf(&urlBuf, SCHEME_HTTPS, sizeof(SCHEME_HTTPS)-1) :
			es_addBuf(&urlBuf, SCHEME_HTTP, sizeof(SCHEME_HTTP)-1);
	}
	if (r == 0)
		r = es_addBuf(&urlBuf, (char *)server, strlen(server));
	if (r == 0 && !strchr(host, ':')) {
		snprintf(portBuf, sizeof(portBuf), ":%d", port);
		r = es_addBuf(&urlBuf, portBuf, strlen(portBuf));
	}
	if (r == 0)
		r = es_addChar(&urlBuf, '/');
	if (r == 0)
		pData->serverBaseUrl = (uchar*) es_str2cstr(urlBuf, NULL);

	if (r != 0 || pData->serverBaseUrl == NULL) {
		LogError(0, RS_RET_ERR, "omclickhouse: error occurred computing baseUrl from "
			"server %s", server);
		ABORT_FINALIZE(RS_RET_ERR);
	}
finalize_it:
	if (urlBuf) {
		es_deleteStr(urlBuf);
	}
	RETiRet;
}

BEGINnewActInst
	struct cnfparamvals *pvals;
	uchar *server = NULL;
	int i;
	FILE *fp;
	char errStr[1024];
CODESTARTnewActInst;
	if((pvals = nvlstGetParams(lst, &actpblk, NULL)) == NULL) {
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	CHKiRet(createInstance(&pData));
	setInstParamDefaults(pData);

	for(i = 0 ; i < actpblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(actpblk.descr[i].name, "server")) {
			server = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "port")) {
			pData->port = (int) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "user")) {
			pData->user = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "pwd")) {
			pData->pwd = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "healthchecktimeout")) {
			pData->healthCheckTimeout = (long) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "timeout")) {
			pData->timeout = (long) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "template")) {
			pData->tplName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "usehttps")) {
			pData->useHttps = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "allowunsignedcerts")) {
			pData->allowUnsignedCerts = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "skipverifyhost")) {
			pData->skipVerifyHost = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "errorfile")) {
			pData->errorFile = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "bulkmode")) {
			pData->bulkmode = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "maxbytes")) {
			pData->maxbytes = (size_t) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "tls.cacert")) {
			pData->caCertFile = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
			fp = fopen((const char*)pData->caCertFile, "r");
			if(fp == NULL) {
				rs_strerror_r(errno, errStr, sizeof(errStr));
				LogError(0, RS_RET_NO_FILE_ACCESS,
					"error: omclickhouse: 'tls.cacert' file %s couldn't be accessed: %s\n",
						pData->caCertFile, errStr);
			} else {
				fclose(fp);
			}
		} else if(!strcmp(actpblk.descr[i].name, "tls.mycert")) {
			pData->myCertFile = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
			fp = fopen((const char*)pData->myCertFile, "r");
			if(fp == NULL) {
				rs_strerror_r(errno, errStr, sizeof(errStr));
				LogError(0, RS_RET_NO_FILE_ACCESS,
					"error: omclickhouse: 'tls.mycert' file %s couldn't be accessed: %s\n",
						pData->myCertFile, errStr);
			} else {
				fclose(fp);
			}
		} else if(!strcmp(actpblk.descr[i].name, "tls.myprivkey")) {
			pData->myPrivKeyFile = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
			fp = fopen((const char*)pData->myPrivKeyFile, "r");
			if(fp == NULL) {
				rs_strerror_r(errno, errStr, sizeof(errStr));
				LogError(0, RS_RET_NO_FILE_ACCESS,
					"error: omclickhouse: 'tls.myprivkey' file %s couldn't be accessed: %s\n",
						pData->myPrivKeyFile, errStr);
			} else {
				fclose(fp);
			}
		} else {
			LogError(0, RS_RET_INTERNAL_ERROR, "omclickhouse: program error, "
				"non-handled param '%s'", actpblk.descr[i].name);
		}
	}


	if(pData->user == NULL && pData->pwd != NULL) {
		LogMsg(0, RS_RET_OK, LOG_WARNING, "omclickhouse: No user was specified "
				"but a password was given.");
	}

	if(pData->user != NULL)
		CHKiRet(computeAuthHeader((char*) pData->user, (char*) pData->pwd, &pData->authBuf));

	CODE_STD_STRING_REQUESTnewActInst(1);
	CHKiRet(OMSRsetEntry(*ppOMSR, 0, (uchar*)strdup((pData->tplName == NULL) ?
		" StdClickHouseFmt" : (char*)pData->tplName), OMSR_RQD_TPL_OPT_SQL));

	if(server != NULL) {
		CHKiRet(computeBaseUrl((const char*)server, pData->port, pData->useHttps, pData));
	} else {
		LogMsg(0, RS_RET_OK, LOG_WARNING,
			"omclickhouse: No servers specified, using localhost");
		CHKiRet(computeBaseUrl("localhost", pData->port, pData->useHttps,
					pData));
	}

	/* node created, let's add to list of instance configs for the module */
	if(loadModConf->tail == NULL) {
		loadModConf->tail = loadModConf->root = pData;
	} else {
		loadModConf->tail->next = pData;
		loadModConf->tail = pData;
	}

CODE_STD_FINALIZERnewActInst;
	free(server);
	cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst


BEGINbeginCnfLoad
CODESTARTbeginCnfLoad;
	loadModConf = pModConf;
	pModConf->pConf = pConf;
	pModConf->root = pModConf->tail = NULL;
ENDbeginCnfLoad


BEGINendCnfLoad
CODESTARTendCnfLoad;
	loadModConf = NULL; /* done loading */
ENDendCnfLoad


BEGINcheckCnf
CODESTARTcheckCnf;
ENDcheckCnf


BEGINactivateCnf
CODESTARTactivateCnf;
ENDactivateCnf


BEGINfreeCnf
CODESTARTfreeCnf;
ENDfreeCnf


BEGINdoHUP
CODESTARTdoHUP;
	if(pData->fdErrFile != -1) {
		close(pData->fdErrFile);
		pData->fdErrFile = -1;
	}
ENDdoHUP


BEGINmodExit
CODESTARTmodExit;
	curl_global_cleanup();
	statsobj.Destruct(&indexStats);
	objRelease(statsobj, CORE_COMPONENT);
	objRelease(prop, CORE_COMPONENT);
	objRelease(ruleset, CORE_COMPONENT);
ENDmodExit

NO_LEGACY_CONF_parseSelectorAct

BEGINqueryEtryPt
CODESTARTqueryEtryPt;
CODEqueryEtryPt_STD_OMOD_QUERIES;
CODEqueryEtryPt_STD_OMOD8_QUERIES;
CODEqueryEtryPt_IsCompatibleWithFeature_IF_OMOD_QUERIES;
CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES;
CODEqueryEtryPt_doHUP
CODEqueryEtryPt_TXIF_OMOD_QUERIES /* we support the transactional interface! */
CODEqueryEtryPt_STD_CONF2_QUERIES;
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit;
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(statsobj, CORE_COMPONENT));
	CHKiRet(objUse(prop, CORE_COMPONENT));
	CHKiRet(objUse(ruleset, CORE_COMPONENT));

	if (curl_global_init(CURL_GLOBAL_ALL) != 0) {
		LogError(0, RS_RET_OBJ_CREATION_FAILED, "CURL fail. -indexing disabled");
		ABORT_FINALIZE(RS_RET_OBJ_CREATION_FAILED);
	}

	/* support statistics gathering */
	CHKiRet(statsobj.Construct(&indexStats));
	CHKiRet(statsobj.SetName(indexStats, (uchar *)"omclickhouse"));
	CHKiRet(statsobj.SetOrigin(indexStats, (uchar *)"omclickhouse"));
	STATSCOUNTER_INIT(indexSubmit, mutIndexSubmit);
	CHKiRet(statsobj.AddCounter(indexStats, (uchar *)"submitted",
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &indexSubmit));
	STATSCOUNTER_INIT(indexHTTPFail, mutIndexHTTPFail);
	CHKiRet(statsobj.AddCounter(indexStats, (uchar *)"failed.http",
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &indexHTTPFail));
	STATSCOUNTER_INIT(indexHTTPReqFail, mutIndexHTTPReqFail);
	CHKiRet(statsobj.AddCounter(indexStats, (uchar *)"failed.httprequests",
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &indexHTTPReqFail));
	STATSCOUNTER_INIT(indexFail, mutIndexFail);
	CHKiRet(statsobj.AddCounter(indexStats, (uchar *)"failed.clickhouse",
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &indexFail));
	STATSCOUNTER_INIT(indexSuccess, mutIndexSuccess);
	CHKiRet(statsobj.AddCounter(indexStats, (uchar *)"response.success",
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &indexSuccess));
	CHKiRet(statsobj.ConstructFinalize(indexStats));

ENDmodInit

/* vi:set ai:
 */
