/* omelasticsearch.c
 * This is the http://www.elasticsearch.org/ output module.
 *
 * NOTE: read comments in module-template.h for more specifics!
 *
 * Copyright 2011 Nathan Scott.
 * Copyright 2009 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Rsyslog is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rsyslog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Rsyslog.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */
#include "config.h"
#include "rsyslog.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <curl/types.h>
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

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP

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
	CURL	*curlHandle;	/* libcurl session handle */
	HEADER	*postHeader;	/* json POST request info */
} instanceData;

/* config variables */
static int restPort = 9200;
static char *hostName = "localhost";
static char *searchIndex = "system";
static char *searchType = "events";

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
ENDfreeInstance

BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
ENDdbgPrintInstInfo

BEGINtryResume
CODESTARTtryResume
ENDtryResume

rsRetVal
curlPost(instanceData *instance, uchar *message)
{
	CURLcode code;
	CURL *curl = instance->curlHandle;
	int length = strlen((char *)message);

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
}

BEGINdoAction
CODESTARTdoAction
	CHKiRet(curlPost(pData, ppString[0]));
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
curlSetup(instanceData *instance)
{
	char restURL[2048];	/* libcurl makes a copy, using the stack here is OK */
	HEADER *header;
	CURL *handle;

	handle = curl_easy_init();
	if (handle == NULL) {
		return RS_RET_OBJ_CREATION_FAILED;
	}

	snprintf(restURL, sizeof(restURL)-1, "http://%s:%d/%s/%s",
		hostName, restPort, searchIndex, searchType);
	header = curl_slist_append(NULL, "Content-Type: text/json; charset=utf-8");

	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, curlResult);
	curl_easy_setopt(handle, CURLOPT_HTTPHEADER, header); 
	curl_easy_setopt(handle, CURLOPT_URL, restURL); 
	curl_easy_setopt(handle, CURLOPT_POST, 1); 

	instance->curlHandle = handle;
	instance->postHeader = header;

	DBGPRINTF("omelasticsearch setup, using REST URL: %s\n", restURL);
	return RS_RET_OK;
}

BEGINparseSelectorAct
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	if(strncmp((char*) p, ":omelasticsearch:", sizeof(":omelasticsearch:") - 1)) {
		ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
	}
	p += sizeof(":omelasticsearch:") - 1; /* eat indicator sequence  (-1 because of '\0'!) */
	CHKiRet(createInstance(&pData));

	/* check if a non-standard template is to be applied */
	if(*(p-1) == ';')
		--p;
	CHKiRet(cflineParseTemplateName(&p, *ppOMSR, 0, OMSR_NO_RQD_TPL_OPTS, (uchar*) " StdJSONFmt"));

	/* all good, we can now initialise our private data */
	CHKiRet(curlSetup(pData));
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
ENDqueryEtryPt

static rsRetVal
resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	DEFiRet;
	restPort = 9200;
	hostName = "localhost";
	searchIndex = "system";
	searchType = "events";
	RETiRet;
}

BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(statsobj, CORE_COMPONENT));

	/* register config file handlers */
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"elasticsearchindex", 0, eCmdHdlrGetWord, NULL, &searchIndex, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"elasticsearchtype", 0, eCmdHdlrGetWord, NULL, &searchType, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"elasticsearchhost", 0, eCmdHdlrGetWord, NULL, &hostName, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"elasticsearchport", 0, eCmdHdlrInt, NULL, &restPort, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));

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
