/* mmkubernetes.c
 * This is a message modification module. It uses metadata obtained
 * from the message to query Kubernetes and obtain additional metadata
 * relating to the container instance.
 *
 * Inspired by:
 * https://github.com/fabric8io/fluent-plugin-kubernetes_metadata_filter
 *
 * NOTE: read comments in module-template.h for details on the calling interface!
 *
 * Copyright 2016 Red Hat Inc.
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

/* todo:
 * - cache cleanup
 * - SIGHUP handling
 * - authentication
 * - statistics generation
 * - other missing configuration options
 * - documentation
 * - more robust error-handling + debugging information
 * - batching, parallel queries, failover, ...
 */

/* needed for asprintf */
#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif

#include "config.h"
#include "rsyslog.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <libestr.h>
#include <json.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <pthread.h>
#include "conf.h"
#include "syslogd-types.h"
#include "module-template.h"
#include "errmsg.h"
#include "regexp.h"
#include "hashtable.h"

/* static data */
MODULE_TYPE_OUTPUT /* this is technically an output plugin */
MODULE_TYPE_KEEP /* releasing the module would cause a leak through libcurl */
MODULE_CNFNAME("mmkubernetes")
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(errmsg)
DEFobjCurrIf(regexp)

#define DFLT_FILENAME_REGEX "var\\.log\\.containers\\.([a-z0-9]([-a-z0-9]*[a-z0-9])?(\\.[a-z0-9]([-a-z0-9]*[a-z0-9])?)*)_([^_]+)_(.+)-([a-z0-9]{64})\\.log$"
#define DFLT_SRCMD_PATH "$!metadata!filename"
#define DFLT_DSTMD_PATH "$!metadata"

static struct cache_s {
	const uchar *kbUrl;
	struct hashtable *mdHt;
	struct hashtable *nsHt;
	pthread_mutex_t *cacheMtx;
} **caches;

/* module configuration data */
struct modConfData_s {
	rsconf_t *pConf;	/* our overall config object */
	uchar *kubernetesUrl;	/**< where to place queries */
	uchar *srcMetadataPath;	/**< where to get data for kubernetes queries */
	uchar *dstMetadataPath;	/**< where to put metadata obtained from kubernetes */
};

/* action (instance) configuration data */
typedef struct _instanceData {
	uchar *kubernetesUrl;	/**< where to place queries */
	uchar *srcMetadataPath;	/**< where to get data for kubernetes queries */
	uchar *dstMetadataPath;	/**< where to put metadata obtained from kubernetes */
	regex_t fnRegex;
	struct cache_s *cache;
} instanceData;

typedef struct wrkrInstanceData {
	instanceData *pData;
	CURL *curlCtx;
	struct curl_slist *curlHdr;
	char *curlRply;
	size_t curlRplyLen;
} wrkrInstanceData_t;

/* module parameters (v6 config format) */
static struct cnfparamdescr modpdescr[] = {
	{ "kubernetesurl", eCmdHdlrString, 0 },
	{ "srcmetadatapath", eCmdHdlrString, 0 },
	{ "dstmetadatapath", eCmdHdlrString, 0 }
};
static struct cnfparamblk modpblk = {
	CNFPARAMBLK_VERSION,
	sizeof(modpdescr)/sizeof(struct cnfparamdescr),
	modpdescr
};

/* action (instance) parameters (v6 config format) */
static struct cnfparamdescr actpdescr[] = {
	{ "kubernetesurl", eCmdHdlrString, 0 },
	{ "srcmetadatapath", eCmdHdlrString, 0 },
	{ "dstmetadatapath", eCmdHdlrString, 0 }
};
static struct cnfparamblk actpblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(actpdescr)/sizeof(struct cnfparamdescr),
	  actpdescr
	};

static modConfData_t *loadModConf = NULL;	/* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL;	/* modConf ptr to use for the current exec process */


BEGINbeginCnfLoad
CODESTARTbeginCnfLoad
	loadModConf = pModConf;
	pModConf->pConf = pConf;
ENDbeginCnfLoad


BEGINsetModCnf
	struct cnfparamvals *pvals = NULL;
	int i;
CODESTARTsetModCnf
	pvals = nvlstGetParams(lst, &modpblk, NULL);
	if(pvals == NULL) {
		errmsg.LogError(0, RS_RET_MISSING_CNFPARAMS, "mmkubernetes: "
			"error processing module config parameters [module(...)]");
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	if(Debug) {
		dbgprintf("module (global) param blk for mmkubernetes:\n");
		cnfparamsPrint(&modpblk, pvals);
	}

	for(i = 0 ; i < modpblk.nParams ; ++i) {
		if(!pvals[i].bUsed) {
			continue;
		} else if(!strcmp(modpblk.descr[i].name, "kubernetesurl")) {
			free(loadModConf->kubernetesUrl);
			loadModConf->kubernetesUrl = (uchar *) es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(modpblk.descr[i].name, "srcmetadatapath")) {
			free(loadModConf->srcMetadataPath);
			loadModConf->srcMetadataPath = (uchar *) es_str2cstr(pvals[i].val.d.estr, NULL);
			/* todo: sanitize the path */
		} else if(!strcmp(modpblk.descr[i].name, "dstmetadatapath")) {
			free(loadModConf->dstMetadataPath);
			loadModConf->dstMetadataPath = (uchar *) es_str2cstr(pvals[i].val.d.estr, NULL);
			/* todo: sanitize the path */
		} else {
			dbgprintf("mmkubernetes: program error, non-handled "
				"param '%s' in module() block\n", modpblk.descr[i].name);
			/* todo: error message? */
		}
	}

	/* set defaults */
	if(loadModConf->srcMetadataPath == NULL)
		loadModConf->srcMetadataPath = (uchar *) strdup(DFLT_SRCMD_PATH);
	if(loadModConf->dstMetadataPath == NULL)
		loadModConf->dstMetadataPath = (uchar *) strdup(DFLT_DSTMD_PATH);

	caches = calloc(1, sizeof(struct cache_s *));

finalize_it:
	if(pvals != NULL)
		cnfparamvalsDestruct(pvals, &modpblk);
ENDsetModCnf


BEGINcreateInstance
CODESTARTcreateInstance
ENDcreateInstance


BEGINfreeInstance
CODESTARTfreeInstance
	free(pData->kubernetesUrl);
	free(pData->srcMetadataPath);
	free(pData->dstMetadataPath);
	regexp.regfree(&pData->fnRegex);
ENDfreeInstance

size_t curlCB(char *data, size_t size, size_t nmemb, void *usrptr)
{
	wrkrInstanceData_t *pWrkrData = (wrkrInstanceData_t *) usrptr;
	char * buf;
	size_t newlen;

	newlen = pWrkrData->curlRplyLen + size * nmemb;
	buf = realloc(pWrkrData->curlRply, newlen);
	memcpy(buf + pWrkrData->curlRplyLen, data, size * nmemb);
	pWrkrData->curlRply = buf;
	pWrkrData->curlRplyLen = newlen;

	return size * nmemb;
}

BEGINcreateWrkrInstance
CODESTARTcreateWrkrInstance
	CURL *ctx;
	struct curl_slist *hdr;

	hdr = curl_slist_append(NULL, "Content-Type: text/json; charset=utf-8");
	pWrkrData->curlHdr = hdr;
	ctx = curl_easy_init();
	curl_easy_setopt(ctx, CURLOPT_HTTPHEADER, hdr);
	curl_easy_setopt(ctx, CURLOPT_WRITEFUNCTION, curlCB);
	curl_easy_setopt(ctx, CURLOPT_WRITEDATA, pWrkrData);
	pWrkrData->curlCtx = ctx;
ENDcreateWrkrInstance


BEGINfreeWrkrInstance
CODESTARTfreeWrkrInstance
	curl_easy_cleanup(pWrkrData->curlCtx);
	curl_slist_free_all(pWrkrData->curlHdr);
ENDfreeWrkrInstance


static struct cache_s *cacheNew(const uchar *url)
{
	struct cache_s *cache;

	cache = calloc(1, sizeof(struct cache_s));
	cache->kbUrl = url;
	cache->mdHt = create_hashtable(100, hash_from_string,
		key_equals_string, (void (*)(void *)) json_object_put);
	cache->nsHt = create_hashtable(100, hash_from_string,
		key_equals_string, (void (*)(void *)) json_object_put);
	cache->cacheMtx = malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(cache->cacheMtx, NULL);

	return cache;
}


static void cacheFree(struct cache_s *cache)
{
	hashtable_destroy(cache->mdHt, 1);
	hashtable_destroy(cache->nsHt, 1);
	pthread_mutex_destroy(cache->cacheMtx);
	free(cache->cacheMtx);
	free(cache);
}


BEGINnewActInst
	struct cnfparamvals *pvals = NULL;
	int i, ret;
CODESTARTnewActInst
	DBGPRINTF("newActInst (mmkubernetes)\n");

	pvals = nvlstGetParams(lst, &actpblk, NULL);
	if(pvals == NULL) {
		errmsg.LogError(0, RS_RET_MISSING_CNFPARAMS, "mmkubernetes: "
			"error processing config parameters [action(...)]");
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	if(Debug) {
		dbgprintf("action param blk in mmkubernetes:\n");
		cnfparamsPrint(&actpblk, pvals);
	}

	CODE_STD_STRING_REQUESTnewActInst(1)
	CHKiRet(OMSRsetEntry(*ppOMSR, 0, NULL, OMSR_TPL_AS_MSG));
	CHKiRet(createInstance(&pData));

	for(i = 0 ; i < actpblk.nParams ; ++i) {
		if(!pvals[i].bUsed) {
			continue;
		} else if(!strcmp(actpblk.descr[i].name, "kubernetesurl")) {
			free(pData->kubernetesUrl);
			pData->kubernetesUrl = (uchar *) es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "srcmetadatapath")) {
			free(pData->srcMetadataPath);
			pData->srcMetadataPath = (uchar *) es_str2cstr(pvals[i].val.d.estr, NULL);
			/* todo: sanitize the path */
		} else if(!strcmp(actpblk.descr[i].name, "dstmetadatapath")) {
			free(pData->dstMetadataPath);
			pData->dstMetadataPath = (uchar *) es_str2cstr(pvals[i].val.d.estr, NULL);
			/* todo: sanitize the path */
		} else {
			dbgprintf("mmkubernetes: program error, non-handled "
				"param '%s' in action() block\n", actpblk.descr[i].name);
			/* todo: error message? */
		}
	}

	if(pData->kubernetesUrl == NULL) {
		if(loadModConf->kubernetesUrl == NULL)
			ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
		pData->kubernetesUrl = (uchar *) strdup((char *) loadModConf->kubernetesUrl);
	}
	if(pData->srcMetadataPath == NULL)
		pData->srcMetadataPath = (uchar *) strdup((char *) loadModConf->srcMetadataPath);
	if(pData->dstMetadataPath == NULL)
		pData->dstMetadataPath = (uchar *) strdup((char *) loadModConf->dstMetadataPath);

	/* todo: make file regexp configurable */
	ret = regexp.regcomp(&pData->fnRegex, DFLT_FILENAME_REGEX, REG_EXTENDED);
	if(ret) {
		dbgprintf("mmkubernetes: regexp compilation failed with code %d.\n", ret);
		ABORT_FINALIZE(RS_RET_ERR);
	}

	/* get the cache for this url */
	for(i = 0; caches[i] != NULL; i++) {
		if(!strcmp((char *) pData->kubernetesUrl, (char *) caches[i]->kbUrl))
			break;
	}
	if(caches[i] != NULL) {
		pData->cache = caches[i];
	} else {
		pData->cache = cacheNew(pData->kubernetesUrl);

		caches = realloc(caches, (i + 2) * sizeof(struct cache_s *));
		caches[i] = pData->cache;
		caches[i + 1] = NULL;
	}
CODE_STD_FINALIZERnewActInst
	if(pvals != NULL)
		cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst


/* legacy config format is not supported */
BEGINparseSelectorAct
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	if(strncmp((char *) p, ":mmkubernetes:", sizeof(":mmkubernetes:") - 1)) {
		errmsg.LogError(0, RS_RET_LEGA_ACT_NOT_SUPPORTED,
			"mmkubernetes supports only v6+ config format, use: "
			"action(type=\"mmkubernetes\" ...)");
	}
	ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINendCnfLoad
CODESTARTendCnfLoad
ENDendCnfLoad


BEGINcheckCnf
CODESTARTcheckCnf
ENDcheckCnf


BEGINactivateCnf
CODESTARTactivateCnf
	runModConf = pModConf;
ENDactivateCnf


BEGINfreeCnf
CODESTARTfreeCnf
	int i;

	free(pModConf->kubernetesUrl);
	free(pModConf->srcMetadataPath);
	free(pModConf->dstMetadataPath);
	for(i = 0; caches[i] != NULL; i++)
		cacheFree(caches[i]);
	free(caches);
ENDfreeCnf


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	dbgprintf("mmkubernetes\n");
	dbgprintf("\tkubernetesUrl='%s'\n", pData->kubernetesUrl);
	dbgprintf("\tsrcMetadataPath='%s'\n", pData->srcMetadataPath);
	dbgprintf("\tdstMetadataPath='%s'\n", pData->dstMetadataPath);
ENDdbgPrintInstInfo


BEGINtryResume
CODESTARTtryResume
ENDtryResume


static rsRetVal
extractMsgMetadata(smsg_t *pMsg, uchar *propName, regex_t *fnRegex,
	char **podName, char **ns, char **contName, char **dockerID)
{
	DEFiRet;
	msgPropDescr_t prop;
	char *filename, *p;
	rs_size_t fnLen;
	unsigned short freeFn = 0;
	size_t nmatch = 8, len;
	regmatch_t pmatch[nmatch];

	/* extract metadata from the file name */
	msgPropDescrFill(&prop, propName, strlen((char *) propName));
	filename = (char *) MsgGetProp(pMsg, NULL, &prop, &fnLen, &freeFn, NULL);
	dbgprintf("mmkubernetes: filename: '%s'.\n", filename);
	msgPropDescrDestruct(&prop);
	if(filename == NULL)
		ABORT_FINALIZE(RS_RET_NOT_FOUND);

	if(REG_NOMATCH == regexp.regexec(fnRegex, filename, nmatch, pmatch, 0))
		ABORT_FINALIZE(RS_RET_NOT_FOUND);

	if(pmatch[1].rm_so != -1) {
		len = pmatch[1].rm_eo - pmatch[1].rm_so;
		p = malloc(len + 1);
		memcpy(p, filename + pmatch[1].rm_so, len);
		p[len] = '\0';
		*podName = p;
	}
	if(pmatch[5].rm_so != -1) {
		len = pmatch[5].rm_eo - pmatch[5].rm_so;
		p = malloc(len + 1);
		memcpy(p, filename + pmatch[5].rm_so, len);
		p[len] = '\0';
		*ns = p;
	}
	if(pmatch[6].rm_so != -1) {
		len = pmatch[6].rm_eo - pmatch[6].rm_so;
		p = malloc(len + 1);
		memcpy(p, filename + pmatch[6].rm_so, len);
		p[len] = '\0';
		*contName = p;
	}
	if(pmatch[7].rm_so != -1) {
		len = pmatch[7].rm_eo - pmatch[7].rm_so;
		p = malloc(len + 1);
		memcpy(p, filename + pmatch[7].rm_so, len);
		p[len] = '\0';
		*dockerID = p;
	}

finalize_it:
	if(freeFn)
		free(filename);
	RETiRet;
}


static rsRetVal
queryKB(wrkrInstanceData_t *pWrkrData, char *url, struct json_object **rply)
{
	DEFiRet;
	CURLcode ccode;
	struct json_tokener *jt = NULL;
	struct json_object *jo;

	/* query kubernetes for pod info */
	ccode = curl_easy_setopt(pWrkrData->curlCtx, CURLOPT_URL, url);
	if(ccode != CURLE_OK)
		ABORT_FINALIZE(RS_RET_ERR);
	ccode = curl_easy_perform(pWrkrData->curlCtx);
	switch(ccode) {
		case CURLE_COULDNT_CONNECT:
		case CURLE_COULDNT_RESOLVE_HOST:
		case CURLE_COULDNT_RESOLVE_PROXY:
		case CURLE_HTTP_RETURNED_ERROR:
		case CURLE_WRITE_ERROR:
			dbgprintf("mmkubernetes: curl connection failed "
				"with code %d.\n", ccode);
			ABORT_FINALIZE(RS_RET_ERR);
		default:
			break;
	}

	/* parse retrieved data */
	jt = json_tokener_new();
	json_tokener_reset(jt);
	jo = json_tokener_parse_ex(jt, pWrkrData->curlRply, pWrkrData->curlRplyLen);
	json_tokener_free(jt);
	if(!json_object_is_type(jo, json_type_object)) {
		json_object_put(jo);
		ABORT_FINALIZE(RS_RET_JSON_PARSE_ERR);
	}

	dbgprintf("mmkubernetes: queryKB reply:\n%s\n",
		json_object_to_json_string_ext(jo, JSON_C_TO_STRING_PRETTY));

	*rply = jo;

finalize_it:
	if(pWrkrData->curlRply != NULL) {
		free(pWrkrData->curlRply);
		pWrkrData->curlRply = NULL;
		pWrkrData->curlRplyLen = 0;
	}
	RETiRet;
}


/* versions < 8.16.0 don't support BEGINdoAction_NoStrings */
#if defined(BEGINdoAction_NoStrings)
BEGINdoAction_NoStrings
	smsg_t **ppMsg = (smsg_t **) pMsgData;
	smsg_t *pMsg = ppMsg[0];
#else
BEGINdoAction
	smsg_t *pMsg = (smsg_t*) ppString[0];
#endif
	char *podName = NULL, *ns = NULL, *containerName = NULL,
		*dockerID = NULL, *mdKey = NULL;
	struct json_object *jMetadata = NULL;
CODESTARTdoAction
	CHKiRet_Hdlr(extractMsgMetadata(pMsg, pWrkrData->pData->srcMetadataPath,
		&pWrkrData->pData->fnRegex, &podName, &ns, &containerName, &dockerID)) {
		ABORT_FINALIZE((iRet == RS_RET_NOT_FOUND) ? RS_RET_OK : iRet);
	}

	assert(podName != NULL);
	assert(ns != NULL);
	assert(containerName != NULL);
	assert(dockerID != NULL);

	dbgprintf("mmkubernetes:\n  podName: '%s'\n  namespace: '%s'\n  containerName: '%s'\n"
		"  dockerID: '%s'\n", podName, ns, containerName, dockerID);

	/* check cache for metadata */
	asprintf(&mdKey, "%s_%s_%s", ns, podName, containerName);
	pthread_mutex_lock(pWrkrData->pData->cache->cacheMtx);
	jMetadata = hashtable_search(pWrkrData->pData->cache->mdHt, mdKey);

	if(jMetadata == NULL) {
		char *url = NULL;
		struct json_object *jReply = NULL, *jo = NULL, *jo2 = NULL, *jNewNS = NULL;

		/* check cache for namespace id */
		jo2 = hashtable_search(pWrkrData->pData->cache->nsHt, ns);
		pthread_mutex_unlock(pWrkrData->pData->cache->cacheMtx);

		if(jo2 == NULL) {
			/* query kubernetes for namespace info */
			/* todo: move url definitions elsewhere */
			asprintf(&url, "%s/api/v1/namespaces/%s",
				 (char *) pWrkrData->pData->kubernetesUrl, ns);
			iRet = queryKB(pWrkrData, url, &jReply);
			free(url);
			/* todo: opt to ignore the missing uid? */
			CHKiRet(iRet);

			if(fjson_object_object_get_ex(jReply, "metadata", &jo2)
				&& fjson_object_object_get_ex(jo2, "uid", &jo2))
				jo2 = jNewNS = json_object_get(jo2);
			else
				jo2 = NULL;

			json_object_put(jReply);
		}

		asprintf(&url, "%s/api/v1/namespaces/%s/pods/%s",
			 (char *) pWrkrData->pData->kubernetesUrl, ns, podName);
		iRet = queryKB(pWrkrData, url, &jReply);
		free(url);
		if(iRet != RS_RET_OK) {
			if(jNewNS) {
				pthread_mutex_lock(pWrkrData->pData->cache->cacheMtx);
				hashtable_insert(pWrkrData->pData->cache->nsHt, ns, jNewNS);
				ns = NULL;
				pthread_mutex_unlock(pWrkrData->pData->cache->cacheMtx);
			}
			FINALIZE;
		}

		jo = json_object_new_object();
		if(jo2)
			json_object_object_add(jo, "namespace_id", json_object_get(jo2));
		if(fjson_object_object_get_ex(jReply, "nodeName", &jo2))
			json_object_object_add(jo, "host", json_object_get(jo2));
		if(fjson_object_object_get_ex(jReply, "uid", &jo2))
			json_object_object_add(jo, "pod_id", json_object_get(jo2));
		/* todo: labels */
		json_object_put(jReply);

		json_object_object_add(jo, "namespace", json_object_new_string(ns));
		json_object_object_add(jo, "pod_name", json_object_new_string(podName));
		json_object_object_add(jo, "container_name", json_object_new_string(containerName));
		jMetadata = json_object_new_object();
		json_object_object_add(jMetadata, "kubernetes", jo);
		jo = json_object_new_object();
		json_object_object_add(jo, "container_id", json_object_new_string(dockerID));
		json_object_object_add(jMetadata, "docker", jo);

		pthread_mutex_lock(pWrkrData->pData->cache->cacheMtx);
		hashtable_insert(pWrkrData->pData->cache->mdHt, mdKey, jMetadata);
		mdKey = NULL;
		if(jNewNS) {
			hashtable_insert(pWrkrData->pData->cache->nsHt, ns, jNewNS);
			ns = NULL;
		}
		pthread_mutex_unlock(pWrkrData->pData->cache->cacheMtx);
	} else {
		pthread_mutex_unlock(pWrkrData->pData->cache->cacheMtx);
	}

	/* the +1 is there to skip the leading '$' */
	msgAddJSON(pMsg, (uchar *) pWrkrData->pData->dstMetadataPath + 1, json_object_get(jMetadata), 0, 0);

finalize_it:
	free(podName);
	free(ns);
	free(containerName);
	free(dockerID);
	free(mdKey);
ENDdoAction


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
ENDisCompatibleWithFeature


/* all the macros bellow have to be in a specific order */
BEGINmodExit
CODESTARTmodExit
	curl_global_cleanup();

	objRelease(regexp, LM_REGEXP_FILENAME);
	objRelease(errmsg, CORE_COMPONENT);
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
CODEqueryEtryPt_STD_OMOD8_QUERIES
CODEqueryEtryPt_STD_CONF2_QUERIES
CODEqueryEtryPt_STD_CONF2_setModCnf_QUERIES
CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	DBGPRINTF("mmkubernetes: module compiled with rsyslog version %s.\n", VERSION);
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(regexp, LM_REGEXP_FILENAME));

	/* CURL_GLOBAL_ALL initializes more than is needed but the
	 * libcurl documentation discourages use of other values
	 */
	curl_global_init(CURL_GLOBAL_ALL);
ENDmodInit
