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
#include <sys/stat.h>
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
#include "srUtils.h"

/* static data */
MODULE_TYPE_OUTPUT /* this is technically an output plugin */
MODULE_TYPE_KEEP /* releasing the module would cause a leak through libcurl */
MODULE_CNFNAME("mmkubernetes")
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(errmsg)
DEFobjCurrIf(regexp)

/* original from fluentd plugin: var\.log\.containers\.(?<pod_name>[a-z0-9]([-a-z0-9]*[a-z0-9])?(\.[a-z0-9]([-a-z0-9]*[a-z0-9])?)*)_(?<namespace>[^_]+)_(?<container_name>.+)-(?<docker_id>[a-z0-9]{64})\.log$'
 * this is for _tag_ match, not actual filename match - in_tail turns filename into a fluentd tag
 */
#define DFLT_FILENAME_REGEX "/var/log/containers/([a-z0-9]([-a-z0-9]*[a-z0-9])?(\\.[a-z0-9]([-a-z0-9]*[a-z0-9])?)*)_([^_]+)_(.+)-([a-z0-9]{64})\\.log$"
/* original from fluentd plugin: '^(?<name_prefix>[^_]+)_(?<container_name>[^\._]+)(\.(?<container_hash>[^_]+))?_(?<pod_name>[^_]+)_(?<namespace>[^_]+)_[^_]+_[^_]+$'
 */
#define DFLT_CONTAINER_REGEX "^[^_]+_([^\\._]+)(\\.([^_]+))?_([^_]+)_([^_]+)_[^_]+_[^_]+$"
#define DFLT_SRCMD_PATH "$!metadata!filename"
#define DFLT_DSTMD_PATH "$!"
#define DFLT_DE_DOT 1 /* true */
#define DFLT_DE_DOT_SEPARATOR "_"
#define DFLT_CONTAINER_NAME "$!CONTAINER_NAME" /**< name of variable holding CONTAINER_NAME value */
#define DFLT_CONTAINER_ID_FULL "$!CONTAINER_ID_FULL" /**< name of variable holding CONTAINER_ID_FULL value */

static struct cache_s {
	const uchar *kbUrl;
	struct hashtable *mdHt;
	struct hashtable *nsHt;
	pthread_mutex_t *cacheMtx;
} **caches;

typedef struct {
	int nmemb;
	uchar **patterns;
	regex_t *regexps;
} annotation_match_t;

/* module configuration data */
struct modConfData_s {
	rsconf_t *pConf;	/* our overall config object */
	uchar *kubernetesUrl;	/**< scheme, host, port, and optional path prefix for Kubernetes API lookups */
	uchar *srcMetadataPath;	/**< where to get data for kubernetes queries */
	uchar *dstMetadataPath;	/**< where to put metadata obtained from kubernetes */
	uchar *caCertFile; /**< File holding the CA cert (+optional chain) of CA that issued the Kubernetes server cert */
	sbool allowUnsignedCerts; /**< For testing/debugging - do not check for CA certs (CURLOPT_SSL_VERIFYPEER FALSE) */
	uchar *token; /**< The token value to use to authenticate to Kubernetes - takes precedence over tokenFile */
	uchar *tokenFile; /**< The file whose contents is the token value to use to authenticate to Kubernetes */
	sbool de_dot; /**< If true (default true), convert '.' characters in labels and annotations to de_dot_separator (default '_') */
	uchar *de_dot_separator; /**< separator character (default '_') to use for de_dotting */
	size_t de_dot_separator_len; /**< length of separator character */
	annotation_match_t annotation_match; /**< annotation keys must match these to be included in record */
	uchar *fnRegexStr; /**< match for filename to get metadata from json-file log filenames */
	uchar *contRegexStr; /**< match for CONTAINER_NAME to get metadata from the value */
};

/* action (instance) configuration data */
typedef struct _instanceData {
	uchar *kubernetesUrl;	/**< scheme, host, port, and optional path prefix for Kubernetes API lookups */
	msgPropDescr_t *srcMetadataDescr;	/**< where to get data for kubernetes queries */
	uchar *dstMetadataPath;	/**< where to put metadata obtained from kubernetes */
	uchar *caCertFile; /**< File holding the CA cert (+optional chain) of CA that issued the Kubernetes server cert */
	sbool allowUnsignedCerts; /**< For testing/debugging - do not check for CA certs (CURLOPT_SSL_VERIFYPEER FALSE) */
	uchar *token; /**< The token value to use to authenticate to Kubernetes - takes precedence over tokenFile */
	uchar *tokenFile; /**< The file whose contents is the token value to use to authenticate to Kubernetes */
	sbool de_dot; /**< If true (default true), convert '.' characters in labels and annotations to de_dot_separator (default '_') */
	uchar *de_dot_separator; /**< separator character (default '_') to use for de_dotting */
	size_t de_dot_separator_len; /**< length of separator character */
	annotation_match_t annotation_match; /**< annotation keys must match these to be included in record */
	regex_t fnRegex;
	regex_t contRegex;
	msgPropDescr_t *contNameDescr; /**< CONTAINER_NAME field */
	msgPropDescr_t *contIdFullDescr; /**< CONTAINER_ID_FULL field */
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
	{ "dstmetadatapath", eCmdHdlrString, 0 },
	{ "tls.cacert", eCmdHdlrString, 0 },
	{ "allowunsignedcerts", eCmdHdlrBinary, 0 },
	{ "token", eCmdHdlrString, 0 },
	{ "tokenfile", eCmdHdlrString, 0 },
	{ "annotation_match", eCmdHdlrArray, 0 },
	{ "de_dot", eCmdHdlrBinary, 0 },
	{ "de_dot_separator", eCmdHdlrString, 0 },
	{ "filenameregex", eCmdHdlrString, 0 },
	{ "containerregex", eCmdHdlrString, 0 }
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
	{ "dstmetadatapath", eCmdHdlrString, 0 },
	{ "tls.cacert", eCmdHdlrString, 0 },
	{ "allowunsignedcerts", eCmdHdlrBinary, 0 },
	{ "token", eCmdHdlrString, 0 },
	{ "tokenfile", eCmdHdlrString, 0 },
	{ "annotation_match", eCmdHdlrArray, 0 },
	{ "de_dot", eCmdHdlrBinary, 0 },
	{ "de_dot_separator", eCmdHdlrString, 0 },
	{ "filenameregex", eCmdHdlrString, 0 },
	{ "containerregex", eCmdHdlrString, 0 }
};
static struct cnfparamblk actpblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(actpdescr)/sizeof(struct cnfparamdescr),
	  actpdescr
	};

static modConfData_t *loadModConf = NULL;	/* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL;	/* modConf ptr to use for the current exec process */

static void free_annotationmatch(annotation_match_t *match) {
	if (match) {
		for(int ii = 0 ; ii < match->nmemb; ++ii) {
			if (match->patterns)
				free(match->patterns[ii]);
			if (match->regexps)
				regexp.regfree(&match->regexps[ii]);
		}
		free(match->patterns);
		match->patterns = NULL;
		free(match->regexps);
		match->regexps = NULL;
		match->nmemb = 0;
	}
}

static int init_annotationmatch(annotation_match_t *match, struct cnfarray *ar) {
	DEFiRet;

	match->nmemb = ar->nmemb;
	CHKmalloc(match->patterns = calloc(sizeof(uchar*), match->nmemb));
	CHKmalloc(match->regexps = calloc(sizeof(regex_t), match->nmemb));
	for(int jj = 0; jj < ar->nmemb; ++jj) {
		int rexret = 0;
		match->patterns[jj] = (uchar*)es_str2cstr(ar->arr[jj], NULL);
		if (0 != (rexret = regexp.regcomp(&match->regexps[jj], (char *)match->patterns[jj], REG_EXTENDED|REG_NOSUB))) {
			char errMsg[512];
			regexp.regerror(rexret, &match->regexps[jj], errMsg, sizeof(errMsg));
			iRet = RS_RET_CONFIG_ERROR;
			errmsg.LogError(0, iRet,
					"error: could not compile annotation_match string [%s] into an extended regexp - %d: %s\n",
					match->patterns[jj], rexret, errMsg);
			break;
		}
	}
finalize_it:
	if (iRet)
		free_annotationmatch(match);
	RETiRet;
}

static int copy_annotationmatch(annotation_match_t *src, annotation_match_t *dest) {
	DEFiRet;

	dest->nmemb = src->nmemb;
	CHKmalloc(dest->patterns = malloc(sizeof(uchar*) * dest->nmemb));
	CHKmalloc(dest->regexps = calloc(sizeof(regex_t), dest->nmemb));
	for(int jj = 0 ; jj < src->nmemb ; ++jj) {
		CHKmalloc(dest->patterns[jj] = (uchar*)strdup((char *)src->patterns[jj]));
		/* assumes was already successfully compiled */
		regexp.regcomp(&dest->regexps[jj], (char *)dest->patterns[jj], REG_EXTENDED|REG_NOSUB);
	}
finalize_it:
    if (iRet)
    	free_annotationmatch(dest);
	RETiRet;
}

/* takes a hash of annotations and returns another json object hash containing only the
 * keys that match - this logic is taken directly from fluent-plugin-kubernetes_metadata_filter
 * except that we do not add the key multiple times to the object to be returned
 */
static struct json_object *match_annotations(annotation_match_t *match, struct json_object *annotations) {
	struct json_object *ret = NULL;
	size_t nmatch = 0; /* ignored REG_NOSUB */
	regmatch_t pmatch[nmatch]; /* ignored REG_NOSUB */

	for (int jj = 0; jj < match->nmemb; ++jj) {
		struct json_object_iterator it = json_object_iter_begin(annotations);
		struct json_object_iterator itEnd = json_object_iter_end(annotations);
		for (;!json_object_iter_equal(&it, &itEnd); json_object_iter_next(&it)) {
			const char *const key = json_object_iter_peek_name(&it);
			if (!ret || !fjson_object_object_get_ex(ret, key, NULL)) {
				if (!regexp.regexec(&match->regexps[jj], key, nmatch, pmatch, 0)) {
					if (!ret) {
						ret = json_object_new_object();
					}
					json_object_object_add(ret, key, json_object_get(json_object_iter_peek_value(&it)));
				}
			}
		}
	}
	return ret;
}

/* This will take a hash of labels or annotations and will de_dot the keys.
 * It will return a brand new hash.  AFAICT, there is no safe way to
 * iterate over the hash while modifying it in place.
 */
static struct json_object *de_dot_json_object(struct json_object *jobj, const char *delim, size_t delim_len) {
	struct json_object *ret = NULL;
	struct json_object_iterator it = json_object_iter_begin(jobj);
	struct json_object_iterator itEnd = json_object_iter_end(jobj);
	es_str_t *new_es_key = NULL;
	DEFiRet;

	ret = json_object_new_object();
	while (!json_object_iter_equal(&it, &itEnd)) {
		const char *const key = json_object_iter_peek_name(&it);
		const char *cc = strstr(key, ".");
		if (NULL == cc) {
			json_object_object_add(ret, key, json_object_get(json_object_iter_peek_value(&it)));
		} else {
			char *new_key = NULL;
			const char *prevcc = key;
			new_es_key = es_newStrFromCStr(key, (es_size_t)(cc-prevcc));
			while (cc) {
				if (es_addBuf(&new_es_key, (char *)delim, (es_size_t)delim_len))
					ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
				cc += 1; /* one past . */
				prevcc = cc; /* beginning of next substring */
				if ((cc = strstr(prevcc, ".")) || (cc = strchr(prevcc, '\0'))) {
					if (es_addBuf(&new_es_key, (char *)prevcc, (es_size_t)(cc-prevcc)))
						ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
					if (!*cc)
						cc = NULL; /* EOS - done */
				}
			}
			new_key = es_str2cstr(new_es_key, NULL);
			es_deleteStr(new_es_key);
			new_es_key = NULL;
			json_object_object_add(ret, new_key, json_object_get(json_object_iter_peek_value(&it)));
			free(new_key);
		}
		json_object_iter_next(&it);
	}
finalize_it:
	if (iRet != RS_RET_OK) {
		json_object_put(ret);
		ret = NULL;
	}
	if (new_es_key)
		es_deleteStr(new_es_key);
	return ret;
}

/* given a "metadata" object field, do
 * - make sure "annotations" field has only the matching keys
 * - de_dot the "labels" and "annotations" fields keys
 * This modifies the jMetadata object in place
 */
static void parse_labels_annotations(struct json_object *jMetadata, annotation_match_t *match, sbool de_dot, const char *delim, size_t delim_len) {
	struct json_object *jo = NULL;

	if (fjson_object_object_get_ex(jMetadata, "annotations", &jo)) {
		if ((jo = match_annotations(match, jo)))
			json_object_object_add(jMetadata, "annotations", jo);
		else
			json_object_object_del(jMetadata, "annotations");
	}
	/* dedot labels and annotations */
	if (de_dot) {
		struct json_object *jo2 = NULL;
		if (fjson_object_object_get_ex(jMetadata, "annotations", &jo)) {
			if ((jo2 = de_dot_json_object(jo, delim, delim_len))) {
				json_object_object_add(jMetadata, "annotations", jo2);
			}
		}
		if (fjson_object_object_get_ex(jMetadata, "labels", &jo)) {
			if ((jo2 = de_dot_json_object(jo, delim, delim_len))) {
				json_object_object_add(jMetadata, "labels", jo2);
			}
		}
	}
}

BEGINbeginCnfLoad
CODESTARTbeginCnfLoad
	loadModConf = pModConf;
	pModConf->pConf = pConf;
ENDbeginCnfLoad


BEGINsetModCnf
	struct cnfparamvals *pvals = NULL;
	int i;
	FILE *fp;
	regex_t rx;
	int ret;
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

	loadModConf->de_dot = DFLT_DE_DOT;
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
		} else if(!strcmp(modpblk.descr[i].name, "tls.cacert")) {
			free(loadModConf->caCertFile);
			loadModConf->caCertFile = (uchar *) es_str2cstr(pvals[i].val.d.estr, NULL);
			fp = fopen((const char*)loadModConf->caCertFile, "r");
			if(fp == NULL) {
				char errStr[1024];
				rs_strerror_r(errno, errStr, sizeof(errStr));
				iRet = RS_RET_NO_FILE_ACCESS;
				errmsg.LogError(0, iRet,
						"error: certificate file %s couldn't be accessed: %s\n",
						loadModConf->caCertFile, errStr);
				ABORT_FINALIZE(iRet);
			} else {
				fclose(fp);
			}
		} else if(!strcmp(modpblk.descr[i].name, "allowunsignedcerts")) {
			loadModConf->allowUnsignedCerts = pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "token")) {
			free(loadModConf->token);
			loadModConf->token = (uchar *) es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(modpblk.descr[i].name, "tokenfile")) {
			free(loadModConf->tokenFile);
			loadModConf->tokenFile = (uchar *) es_str2cstr(pvals[i].val.d.estr, NULL);
			fp = fopen((const char*)loadModConf->tokenFile, "r");
			if(fp == NULL) {
				char errStr[1024];
				rs_strerror_r(errno, errStr, sizeof(errStr));
				iRet = RS_RET_NO_FILE_ACCESS;
				errmsg.LogError(0, iRet,
						"error: token file %s couldn't be accessed: %s\n",
						loadModConf->tokenFile, errStr);
				ABORT_FINALIZE(iRet);
			} else {
				fclose(fp);
			}
		} else if(!strcmp(modpblk.descr[i].name, "annotation_match")) {
			free_annotationmatch(&loadModConf->annotation_match);
			if ((ret = init_annotationmatch(&loadModConf->annotation_match, pvals[i].val.d.ar)))
				ABORT_FINALIZE(ret);
		} else if(!strcmp(modpblk.descr[i].name, "de_dot")) {
			loadModConf->de_dot = pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "de_dot_separator")) {
			free(loadModConf->de_dot_separator);
			loadModConf->de_dot_separator = (uchar *) es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(modpblk.descr[i].name, "filenameregex")) {
			free(loadModConf->fnRegexStr);
			loadModConf->fnRegexStr = (uchar *) es_str2cstr(pvals[i].val.d.estr, NULL);
			if((ret = regexp.regcomp(&rx, (char *)loadModConf->fnRegexStr, REG_EXTENDED))) {
				char errMsg[512];
				regexp.regerror(ret, &rx, errMsg, sizeof(errMsg));
				iRet = RS_RET_CONFIG_ERROR;
				errmsg.LogError(0, iRet,
						"error: could not compile 'filenameregex' string [%s] into an extended regexp - %d: %s\n",
						loadModConf->fnRegexStr, ret, errMsg);
				regexp.regfree(&rx);
				ABORT_FINALIZE(iRet);
			}
			regexp.regfree(&rx);
		} else if(!strcmp(modpblk.descr[i].name, "containerregex")) {
			free(loadModConf->contRegexStr);
			loadModConf->contRegexStr = (uchar *) es_str2cstr(pvals[i].val.d.estr, NULL);
			if((ret = regexp.regcomp(&rx, (char *)loadModConf->contRegexStr, REG_EXTENDED))) {
				char errMsg[512];
				regexp.regerror(ret, &rx, errMsg, sizeof(errMsg));
				iRet = RS_RET_CONFIG_ERROR;
				errmsg.LogError(0, iRet,
						"error: could not compile 'containerregex' string [%s] into an extended regexp - %d: %s\n",
						loadModConf->contRegexStr, ret, errMsg);
				regexp.regfree(&rx);
				ABORT_FINALIZE(iRet);
			}
			regexp.regfree(&rx);
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
	if(loadModConf->de_dot_separator == NULL)
		loadModConf->de_dot_separator = (uchar *) strdup(DFLT_DE_DOT_SEPARATOR);
	loadModConf->de_dot_separator_len = strlen((const char *)loadModConf->de_dot_separator);
	if (loadModConf->fnRegexStr == NULL)
		loadModConf->fnRegexStr = (uchar *) strdup(DFLT_FILENAME_REGEX);
	if (loadModConf->contRegexStr == NULL)
		loadModConf->contRegexStr = (uchar *) strdup(DFLT_CONTAINER_REGEX);

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
	msgPropDescrDestruct(pData->srcMetadataDescr);
	free(pData->srcMetadataDescr);
	free(pData->dstMetadataPath);
	free(pData->caCertFile);
	free(pData->token);
	free(pData->tokenFile);
	regexp.regfree(&pData->fnRegex);
	regexp.regfree(&pData->contRegex);
	free_annotationmatch(&pData->annotation_match);
	free(pData->de_dot_separator);
	msgPropDescrDestruct(pData->contNameDescr);
	free(pData->contNameDescr);
	msgPropDescrDestruct(pData->contIdFullDescr);
	free(pData->contIdFullDescr);
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
	struct curl_slist *hdr = NULL;
	char *tokenHdr = NULL;
	FILE *fp;

	hdr = curl_slist_append(hdr, "Content-Type: text/json; charset=utf-8");
	if (pWrkrData->pData->token) {
		asprintf(&tokenHdr, "Authorization: Bearer %s", pWrkrData->pData->token);
	} else if (pWrkrData->pData->tokenFile) {
		struct stat statbuf;
		char *token = NULL;
		fp = fopen((const char*)pWrkrData->pData->tokenFile, "r");
		if (fp && !fstat(fileno(fp), &statbuf)) {
			size_t bytesread;
			token = malloc((statbuf.st_size+1)*sizeof(char));
			if (0 < (bytesread = fread(token, sizeof(char), statbuf.st_size, fp))) {
				token[bytesread] = '\0';
				asprintf(&tokenHdr, "Authorization: Bearer %s", token);
			}
			free(token);
		}
		fclose(fp);
	}
	if (tokenHdr) {
		hdr = curl_slist_append(hdr, tokenHdr);
		free(tokenHdr);
	}
	pWrkrData->curlHdr = hdr;
	ctx = curl_easy_init();
	curl_easy_setopt(ctx, CURLOPT_HTTPHEADER, hdr);
	curl_easy_setopt(ctx, CURLOPT_WRITEFUNCTION, curlCB);
	curl_easy_setopt(ctx, CURLOPT_WRITEDATA, pWrkrData);
	if(pWrkrData->pData->caCertFile)
		curl_easy_setopt(ctx, CURLOPT_CAINFO, pWrkrData->pData->caCertFile);
	if(pWrkrData->pData->allowUnsignedCerts)
		curl_easy_setopt(ctx, CURLOPT_SSL_VERIFYPEER, 0);

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
	FILE *fp;
	char *rxstr = NULL;
	int haveFnRegex = 0;
	int haveContRegex = 0;
	char *srcMetadataPath = NULL;
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

	pData->de_dot = loadModConf->de_dot;
	pData->allowUnsignedCerts = loadModConf->allowUnsignedCerts;
	for(i = 0 ; i < actpblk.nParams ; ++i) {
		if(!pvals[i].bUsed) {
			continue;
		} else if(!strcmp(actpblk.descr[i].name, "kubernetesurl")) {
			free(pData->kubernetesUrl);
			pData->kubernetesUrl = (uchar *) es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "srcmetadatapath")) {
			msgPropDescrDestruct(pData->srcMetadataDescr);
			free(pData->srcMetadataDescr);
			CHKmalloc(pData->srcMetadataDescr = MALLOC(sizeof(msgPropDescr_t)));
			srcMetadataPath = es_str2cstr(pvals[i].val.d.estr, NULL);
			CHKiRet(msgPropDescrFill(pData->srcMetadataDescr, (uchar *)srcMetadataPath, strlen(srcMetadataPath)));
			/* todo: sanitize the path */
		} else if(!strcmp(actpblk.descr[i].name, "dstmetadatapath")) {
			free(pData->dstMetadataPath);
			pData->dstMetadataPath = (uchar *) es_str2cstr(pvals[i].val.d.estr, NULL);
			/* todo: sanitize the path */
		} else if(!strcmp(actpblk.descr[i].name, "tls.cacert")) {
			free(pData->caCertFile);
			pData->caCertFile = (uchar *) es_str2cstr(pvals[i].val.d.estr, NULL);
			fp = fopen((const char*)pData->caCertFile, "r");
			if(fp == NULL) {
				char errStr[1024];
				rs_strerror_r(errno, errStr, sizeof(errStr));
				iRet = RS_RET_NO_FILE_ACCESS;
				errmsg.LogError(0, iRet,
						"error: certificate file %s couldn't be accessed: %s\n",
						pData->caCertFile, errStr);
				ABORT_FINALIZE(iRet);
			} else {
				fclose(fp);
			}
		} else if(!strcmp(actpblk.descr[i].name, "allowunsignedcerts")) {
			pData->allowUnsignedCerts = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "token")) {
			free(pData->token);
			pData->token = (uchar *) es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "tokenfile")) {
			free(pData->tokenFile);
			pData->tokenFile = (uchar *) es_str2cstr(pvals[i].val.d.estr, NULL);
			fp = fopen((const char*)pData->tokenFile, "r");
			if(fp == NULL) {
				char errStr[1024];
				rs_strerror_r(errno, errStr, sizeof(errStr));
				iRet = RS_RET_NO_FILE_ACCESS;
				errmsg.LogError(0, iRet,
						"error: token file %s couldn't be accessed: %s\n",
						pData->tokenFile, errStr);
				ABORT_FINALIZE(iRet);
			} else {
				fclose(fp);
			}
		} else if(!strcmp(actpblk.descr[i].name, "annotation_match")) {
			free_annotationmatch(&pData->annotation_match);
			if (RS_RET_OK != (iRet = init_annotationmatch(&pData->annotation_match, pvals[i].val.d.ar)))
				ABORT_FINALIZE(iRet);
		} else if(!strcmp(actpblk.descr[i].name, "de_dot")) {
			pData->de_dot = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "de_dot_separator")) {
			free(pData->de_dot_separator);
			pData->de_dot_separator = (uchar *) es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "filenameregex")) {
			regexp.regfree(&pData->fnRegex);
			rxstr = es_str2cstr(pvals[i].val.d.estr, NULL);
			if((ret = regexp.regcomp(&pData->fnRegex, rxstr, REG_EXTENDED))) {
				char errMsg[512];
				regexp.regerror(ret, &pData->fnRegex, errMsg, sizeof(errMsg));
				iRet = RS_RET_CONFIG_ERROR;
				errmsg.LogError(0, iRet,
						"error: could not compile 'filenameregex' string [%s] into an extended regexp - %d: %s\n",
						rxstr, ret, errMsg);
				free(rxstr);
				rxstr = NULL;
				ABORT_FINALIZE(iRet);
			}
			free(rxstr);
			rxstr = NULL;
			haveFnRegex = 1;
		} else if(!strcmp(actpblk.descr[i].name, "containerregex")) {
			regexp.regfree(&pData->contRegex);
			rxstr = es_str2cstr(pvals[i].val.d.estr, NULL);
			if((ret = regexp.regcomp(&pData->contRegex, rxstr, REG_EXTENDED))) {
				char errMsg[512];
				regexp.regerror(ret, &pData->contRegex, errMsg, sizeof(errMsg));
				iRet = RS_RET_CONFIG_ERROR;
				errmsg.LogError(0, iRet,
						"error: could not compile 'containerregex' string [%s] into an extended regexp - %d: %s\n",
						rxstr, ret, errMsg);
				free(rxstr);
				rxstr = NULL;
				ABORT_FINALIZE(iRet);
			}
			free(rxstr);
			rxstr = NULL;
			haveContRegex = 1;
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
	if(pData->srcMetadataDescr == NULL) {
		CHKmalloc(pData->srcMetadataDescr = MALLOC(sizeof(msgPropDescr_t)));
		CHKiRet(msgPropDescrFill(pData->srcMetadataDescr, loadModConf->srcMetadataPath, strlen((char *)loadModConf->srcMetadataPath)));
	}
	if(pData->dstMetadataPath == NULL)
		pData->dstMetadataPath = (uchar *) strdup((char *) loadModConf->dstMetadataPath);
	if(pData->caCertFile == NULL && loadModConf->caCertFile)
		pData->caCertFile = (uchar *) strdup((char *) loadModConf->caCertFile);
	if(pData->token == NULL && loadModConf->token)
		pData->token = (uchar *) strdup((char *) loadModConf->token);
	if(pData->tokenFile == NULL && loadModConf->tokenFile)
		pData->tokenFile = (uchar *) strdup((char *) loadModConf->tokenFile);
	if(pData->de_dot_separator == NULL && loadModConf->de_dot_separator)
		pData->de_dot_separator = (uchar *) strdup((char *) loadModConf->de_dot_separator);
	if((pData->annotation_match.nmemb == 0) && (loadModConf->annotation_match.nmemb > 0))
		copy_annotationmatch(&loadModConf->annotation_match, &pData->annotation_match);

	pData->de_dot_separator_len = strlen((const char *)pData->de_dot_separator);

	if(!haveFnRegex && (ret = regexp.regcomp(&pData->fnRegex, (char *)loadModConf->fnRegexStr, REG_EXTENDED))) {
		char errMsg[512];
		regexp.regerror(ret, &pData->fnRegex, errMsg, sizeof(errMsg));
		iRet = RS_RET_CONFIG_ERROR;
		errmsg.LogError(0, iRet,
				"error: could not compile 'filenameregex' string [%s] into an extended regexp - %d: %s\n",
				loadModConf->fnRegexStr, ret, errMsg);
		ABORT_FINALIZE(iRet);
	}

	if(!haveContRegex && (ret = regexp.regcomp(&pData->contRegex, (char *)loadModConf->contRegexStr, REG_EXTENDED))) {
		char errMsg[512];
		regexp.regerror(ret, &pData->contRegex, errMsg, sizeof(errMsg));
		iRet = RS_RET_CONFIG_ERROR;
		errmsg.LogError(0, iRet,
				"error: could not compile 'containerregex' string [%s] into an extended regexp - %d: %s\n",
				loadModConf->contRegexStr, ret, errMsg);
		ABORT_FINALIZE(iRet);
	}

	CHKmalloc(pData->contNameDescr = MALLOC(sizeof(msgPropDescr_t)));
	CHKiRet(msgPropDescrFill(pData->contNameDescr, (uchar*) DFLT_CONTAINER_NAME, strlen(DFLT_CONTAINER_NAME)));
	CHKmalloc(pData->contIdFullDescr = MALLOC(sizeof(msgPropDescr_t)));
	CHKiRet(msgPropDescrFill(pData->contIdFullDescr, (uchar*) DFLT_CONTAINER_ID_FULL, strlen(DFLT_CONTAINER_NAME)));

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
	free(rxstr);
	free(srcMetadataPath);
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
	free(pModConf->caCertFile);
	free(pModConf->token);
	free(pModConf->tokenFile);
	free(pModConf->de_dot_separator);
	free(pModConf->fnRegexStr);
	free(pModConf->contRegexStr);
	free_annotationmatch(&pModConf->annotation_match);
	for(i = 0; caches[i] != NULL; i++)
		cacheFree(caches[i]);
	free(caches);
ENDfreeCnf


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	dbgprintf("mmkubernetes\n");
	dbgprintf("\tkubernetesUrl='%s'\n", pData->kubernetesUrl);
	dbgprintf("\tsrcMetadataPath='%s'\n", pData->srcMetadataDescr->name);
	dbgprintf("\tdstMetadataPath='%s'\n", pData->dstMetadataPath);
	dbgprintf("\ttls.cacert='%s'\n", pData->caCertFile);
	dbgprintf("\tallowUnsignedCerts='%d'\n", pData->allowUnsignedCerts);
	dbgprintf("\ttoken='%s'\n", pData->token);
	dbgprintf("\ttokenFile='%s'\n", pData->tokenFile);
	dbgprintf("\tde_dot='%d'\n", pData->de_dot);
	dbgprintf("\tde_dot_separator='%s'\n", pData->de_dot_separator);
ENDdbgPrintInstInfo


BEGINtryResume
CODESTARTtryResume
ENDtryResume

static rsRetVal
extractMsgMetadata(smsg_t *pMsg, instanceData *pData,
	char **podName, char **ns, char **contName, char **dockerID)
{
	DEFiRet;
	char *filename, *p;
	rs_size_t fnLen;
	unsigned short freeFn = 0;
	size_t nmatch = 8, len;
	regmatch_t pmatch[nmatch];
	uchar *container_name = NULL;
	rs_size_t container_name_len;
	unsigned short free_container_name = 0;
	uchar *container_id_full = NULL;
	rs_size_t container_id_full_len;
	unsigned short free_container_id_full = 0;

	/* extract metadata from the CONTAINER_NAME field and see if CONTAINER_ID_FULL is present */
	container_name = MsgGetProp(pMsg, NULL, pData->contNameDescr, &container_name_len, &free_container_name, NULL);
	container_id_full = MsgGetProp(pMsg, NULL, pData->contIdFullDescr, &container_id_full_len, &free_container_id_full, NULL);

	if (container_name && container_id_full && container_name_len && container_id_full_len) {
		dbgprintf("mmkubernetes: CONTAINER_NAME: '%s'  CONTAINER_ID_FULL: '%s'.\n", container_name, container_id_full);
		if(REG_NOERROR == regexp.regexec(&pData->contRegex, (char *)container_name, nmatch, pmatch, 0)) {
			if(pmatch[1].rm_so != -1) {
				len = pmatch[1].rm_eo - pmatch[1].rm_so;
				p = malloc(len + 1);
				memcpy(p, container_name + pmatch[1].rm_so, len);
				p[len] = '\0';
				*contName = p;
			}
			if(pmatch[4].rm_so != -1) {
				len = pmatch[4].rm_eo - pmatch[4].rm_so;
				p = malloc(len + 1);
				memcpy(p, container_name + pmatch[4].rm_so, len);
				p[len] = '\0';
				*podName = p;
			}
			if(pmatch[5].rm_so != -1) {
				len = pmatch[5].rm_eo - pmatch[5].rm_so;
				p = malloc(len + 1);
				memcpy(p, container_name + pmatch[5].rm_so, len);
				p[len] = '\0';
				*ns = p;
			}
			if (free_container_id_full) {
				/* pass the memory out */
				*dockerID = (char *)container_id_full;
				container_id_full = NULL;
			} else {
				/* make a copy */
				*dockerID = strdup((char *)container_id_full);
			}
			ABORT_FINALIZE(RS_RET_OK);
		}
	}

	/* extract metadata from the file name */
	filename = (char *) MsgGetProp(pMsg, NULL, pData->srcMetadataDescr, &fnLen, &freeFn, NULL);
	dbgprintf("mmkubernetes: filename: '%s'.\n", filename);
	if(filename == NULL)
		ABORT_FINALIZE(RS_RET_NOT_FOUND);

	if(REG_NOMATCH == regexp.regexec(&pData->fnRegex, filename, nmatch, pmatch, 0))
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
	if (free_container_name)
		free(container_name);
	if (free_container_id_full)
		free(container_id_full);
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
	if (CURLE_OK != (ccode = curl_easy_perform(pWrkrData->curlCtx))) {
		errmsg.LogMsg(0, RS_RET_ERR, LOG_ERR,
				      "mmkubernetes: failed to connect to [%s] - %d:%s\n",
				      url, ccode, curl_easy_strerror(ccode));
		ABORT_FINALIZE(RS_RET_ERR);
	}

	/* parse retrieved data */
	jt = json_tokener_new();
	json_tokener_reset(jt);
	jo = json_tokener_parse_ex(jt, pWrkrData->curlRply, pWrkrData->curlRplyLen);
	json_tokener_free(jt);
	if(!json_object_is_type(jo, json_type_object)) {
		json_object_put(jo);
		jo = NULL;
		errmsg.LogMsg(0, RS_RET_JSON_PARSE_ERR, LOG_INFO,
				      "mmkubernetes: unable to parse string as JSON:[%.*s]\n",
					  (int)pWrkrData->curlRplyLen, pWrkrData->curlRply);
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
	struct json_object *jMetadata = NULL, *jMetadataCopy = NULL;
	int add_ns_metadata = 0;
CODESTARTdoAction
	CHKiRet_Hdlr(extractMsgMetadata(pMsg, pWrkrData->pData,
				 &podName, &ns, &containerName, &dockerID)) {
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
		struct json_object *jReply = NULL, *jo = NULL, *jo2 = NULL, *jNsMeta = NULL;

		/* check cache for namespace metadata */
		jNsMeta = hashtable_search(pWrkrData->pData->cache->nsHt, ns);

		if(jNsMeta == NULL) {
			/* query kubernetes for namespace info */
			/* todo: move url definitions elsewhere */
			asprintf(&url, "%s/api/v1/namespaces/%s",
				 (char *) pWrkrData->pData->kubernetesUrl, ns);
			iRet = queryKB(pWrkrData, url, &jReply);
			free(url);
			/* todo: implement support for the .orphaned namespace */
			if (iRet != RS_RET_OK) {
				json_object_put(jReply);
				jReply = NULL;
				pthread_mutex_unlock(pWrkrData->pData->cache->cacheMtx);
				FINALIZE;
			}

			if(fjson_object_object_get_ex(jReply, "metadata", &jNsMeta)) {
				jNsMeta = json_object_get(jNsMeta);
				parse_labels_annotations(jNsMeta, &pWrkrData->pData->annotation_match, pWrkrData->pData->de_dot,
										 (const char *)pWrkrData->pData->de_dot_separator, pWrkrData->pData->de_dot_separator_len);
				add_ns_metadata = 1;
			} else {
				/* namespace with no metadata??? */
				errmsg.LogMsg(0, RS_RET_ERR, LOG_INFO,
						      "mmkubernetes: namespace [%s] has no metadata!\n", ns);
				jNsMeta = NULL;
			}

			json_object_put(jReply);
			jReply = NULL;
		}

		asprintf(&url, "%s/api/v1/namespaces/%s/pods/%s",
			 (char *) pWrkrData->pData->kubernetesUrl, ns, podName);
		iRet = queryKB(pWrkrData, url, &jReply);
		free(url);
		if(iRet != RS_RET_OK) {
			if(jNsMeta && add_ns_metadata) {
				hashtable_insert(pWrkrData->pData->cache->nsHt, ns, jNsMeta);
				ns = NULL;
			}
			json_object_put(jReply);
			jReply = NULL;
			pthread_mutex_unlock(pWrkrData->pData->cache->cacheMtx);
			FINALIZE;
		}

		jo = json_object_new_object();
		if(jNsMeta && fjson_object_object_get_ex(jNsMeta, "uid", &jo2))
			json_object_object_add(jo, "namespace_id", json_object_get(jo2));
		if(jNsMeta && fjson_object_object_get_ex(jNsMeta, "labels", &jo2))
			json_object_object_add(jo, "namespace_labels", json_object_get(jo2));
		if(jNsMeta && fjson_object_object_get_ex(jNsMeta, "annotations", &jo2))
			json_object_object_add(jo, "namespace_annotations", json_object_get(jo2));
		if(jNsMeta && fjson_object_object_get_ex(jNsMeta, "creationTimestamp", &jo2))
			json_object_object_add(jo, "creation_timestamp", json_object_get(jo2));
		if(fjson_object_object_get_ex(jReply, "nodeName", &jo2))
			json_object_object_add(jo, "host", json_object_get(jo2));
		if(fjson_object_object_get_ex(jReply, "uid", &jo2))
			json_object_object_add(jo, "pod_id", json_object_get(jo2));
		if(fjson_object_object_get_ex(jReply, "metadata", &jo2)) {
			struct json_object *jo3 = NULL;
			parse_labels_annotations(jo2, &pWrkrData->pData->annotation_match, pWrkrData->pData->de_dot,
									 (const char *)pWrkrData->pData->de_dot_separator, pWrkrData->pData->de_dot_separator_len);
			if(fjson_object_object_get_ex(jo2, "annotations", &jo3))
				json_object_object_add(jo, "annotations", json_object_get(jo3));
			if(fjson_object_object_get_ex(jo2, "labels", &jo3))
				json_object_object_add(jo, "labels", json_object_get(jo3));
		}
		json_object_put(jReply);
		jReply = NULL;

		json_object_object_add(jo, "namespace_name", json_object_new_string(ns));
		json_object_object_add(jo, "pod_name", json_object_new_string(podName));
		json_object_object_add(jo, "container_name", json_object_new_string(containerName));
		json_object_object_add(jo, "master_url", json_object_new_string((const char *)pWrkrData->pData->kubernetesUrl));
		jMetadata = json_object_new_object();
		json_object_object_add(jMetadata, "kubernetes", jo);
		jo = json_object_new_object();
		json_object_object_add(jo, "container_id", json_object_new_string(dockerID));
		json_object_object_add(jMetadata, "docker", jo);

		hashtable_insert(pWrkrData->pData->cache->mdHt, mdKey, jMetadata);
		mdKey = NULL;
		if(jNsMeta && add_ns_metadata) {
			hashtable_insert(pWrkrData->pData->cache->nsHt, ns, jNsMeta);
			ns = NULL;
		}
	}

	/* make a copy of the metadata for the msg to own */
	/* todo: use json_object_deep_copy when implementation available in libfastjson */
	/* yes, this is expensive - but there is no other way to make this thread safe - we
	 * can't allow the msg to have a shared pointer to an element inside the cache,
	 * outside of the cache lock
	 */
	jMetadataCopy = json_tokener_parse(json_object_get_string(jMetadata));
	pthread_mutex_unlock(pWrkrData->pData->cache->cacheMtx);
	/* the +1 is there to skip the leading '$' */
	msgAddJSON(pMsg, (uchar *) pWrkrData->pData->dstMetadataPath + 1, jMetadataCopy, 0, 0);

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
