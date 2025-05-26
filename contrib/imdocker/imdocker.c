/* imdocker.c
 * This is an implementation of the docker container log input module. It uses the
 * Docker API in order to stream all container logs available on a host. Will also
 * update relevant container metadata.
 *
 * Copyright (C) 2018, 2019 the rsyslog project.
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

#ifdef __sun
#define _XPG4_2
#endif
#include "config.h"
#include "rsyslog.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <curl/curl.h>
#include <json.h>
#include <assert.h>
#include <signal.h>
#include <stdbool.h>
#include "cfsysline.h"  /* access to config file objects */
#include "unicode-helper.h"
#include "module-template.h"
#include "srUtils.h"    /* some utility functions */
#include "errmsg.h"
#include "net.h"
#include "glbl.h"
#include "msg.h"
#include "parser.h"
#include "prop.h"
#include "debug.h"
#include "statsobj.h"
#include "datetime.h"
#include "ratelimit.h"
#include "hashtable.h"
#include "hashtable_itr.h"

#if !defined(_AIX)
#pragma GCC diagnostic ignored "-Wswitch-enum"
#endif

MODULE_TYPE_INPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("imdocker")

extern int Debug;

#define USE_MULTI_LINE
#undef ENABLE_DEBUG_BYTE_BUFFER

/* defines */
#define DOCKER_TAG_NAME                     "docker:"

#define DOCKER_CONTAINER_ID_PARSE_NAME      "Id"
#define DOCKER_CONTAINER_NAMES_PARSE_NAME   "Names"
#define DOCKER_CONTAINER_IMAGEID_PARSE_NAME "ImageID"
#define DOCKER_CONTAINER_CREATED_PARSE_NAME "Created"
#define DOCKER_CONTAINER_LABELS_PARSE_NAME  "Labels"

/* label defines */
#define DOCKER_CONTAINER_LABEL_KEY_STARTREGEX "imdocker.startregex"

/* DEFAULT VALUES */
#define DFLT_pollingInterval   60      /* polling interval in seconds */
#define DFLT_retrieveNewLogsFromStart 1/* Process newly found containers logs from start */
#define DFLT_containersLimit   25      /* maximum number of containers */
#define DFLT_trimLineOverBytes 4194304 /* limit log lines to the value - 4MB default */
#define DFLT_bEscapeLF         1       /* whether line feeds should be escaped */

#define DFLT_SEVERITY pri2sev(LOG_INFO)
#define DFLT_FACILITY pri2fac(LOG_USER)

enum {
	dst_invalid = -1,
	dst_stdin,
	dst_stdout,
	dst_stderr,
	dst_stream_type_count
} docker_stream_type_t;

/* imdocker specific structs */
typedef struct imdocker_buf_s {
	uchar  *data;
	size_t len;
	size_t data_size;
} imdocker_buf_t;

typedef struct docker_cont_logs_buf_s {
	imdocker_buf_t *buf;
	int8_t         stream_type;
	size_t         bytes_remaining;
} docker_cont_logs_buf_t;

struct docker_cont_logs_inst_s;
typedef rsRetVal (*submitmsg_funcptr) (struct docker_cont_logs_inst_s *pInst, docker_cont_logs_buf_t *pBufdata,
		const uchar* pszTag);
typedef submitmsg_funcptr SubmitMsgFuncPtr;

/* curl request instance */
typedef struct docker_cont_logs_req_s {
	CURL     *curl;
	docker_cont_logs_buf_t* data_bufs[dst_stream_type_count];
	SubmitMsgFuncPtr submitMsg;
} docker_cont_logs_req_t;

typedef struct imdocker_req_s {
	CURL           *curl;
	imdocker_buf_t *buf;
} imdocker_req_t;

typedef struct docker_container_info_s {
	uchar *name;
	uchar *image_id;
	uint64_t created;
	/* json string container labels */
	uchar *json_str_labels;
} docker_container_info_t;

typedef struct docker_cont_logs_inst_s {
	char *id;
	char short_id[12];
	docker_container_info_t *container_info;
	docker_cont_logs_req_t  *logsReq;
	uchar *start_regex;
	regex_t start_preg;  /* compiled version of start_regex */
	uint32_t prevSegEnd;
} docker_cont_logs_inst_t;

typedef struct docker_cont_log_instances_s {
	struct hashtable* ht_container_log_insts;
	pthread_mutex_t mut;
	CURLM         *curlm;
	/* track the latest created container */
	uint64_t last_container_created;
	uchar   *last_container_id;
	time_t  time_started;
} docker_cont_log_instances_t;

/* FORWARD DEFINITIONS */

/* imdocker_buf_t */
static rsRetVal imdockerBufNew(imdocker_buf_t **ppThis);
static void imdockerBufDestruct(imdocker_buf_t *pThis);

/* docker_cont_logs_buf_t */
static rsRetVal dockerContLogsBufNew(docker_cont_logs_buf_t **ppThis);
static void dockerContLogsBufDestruct(docker_cont_logs_buf_t *pThis);
static rsRetVal dockerContLogsBufWrite(docker_cont_logs_buf_t *pThis, const uchar *pdata,
		size_t write_size);

/* imdocker_req_t */
static rsRetVal imdockerReqNew(imdocker_req_t **ppThis);
static void imdockerReqDestruct(imdocker_req_t *pThis);

/* docker_cont_logs_req_t */
static rsRetVal dockerContLogsReqNew(docker_cont_logs_req_t **ppThis, SubmitMsgFuncPtr submitMsg);
static void dockerContLogsReqDestruct(docker_cont_logs_req_t *pThis);

/* docker_cont_logs_inst_t */
static rsRetVal
dockerContLogsInstNew(docker_cont_logs_inst_t **ppThis, const char* id,
		docker_container_info_t *container_info, SubmitMsgFuncPtr submitMsg);

static void dockerContLogsInstDestruct(docker_cont_logs_inst_t *pThis);
static rsRetVal dockerContLogsInstSetUrlById (sbool isInit, docker_cont_logs_inst_t *pThis,
		CURLM *curlm, const char* containerId);

/* docker_cont_log_instances_t */
static rsRetVal dockerContLogReqsNew(docker_cont_log_instances_t **ppThis);
static rsRetVal dockerContLogReqsDestruct(docker_cont_log_instances_t *pThis);
static rsRetVal dockerContLogReqsGet(docker_cont_log_instances_t *pThis,
		docker_cont_logs_inst_t** ppContLogsInst, const char *id);
static rsRetVal dockerContLogReqsPrint(docker_cont_log_instances_t *pThis);
static rsRetVal dockerContLogReqsAdd(docker_cont_log_instances_t *pThis,
		docker_cont_logs_inst_t *pContLogsReqInst);
static rsRetVal dockerContLogReqsRemove(docker_cont_log_instances_t *pThis, const char *id);

/* docker_container_info_t */
static rsRetVal dockerContainerInfoNew(docker_container_info_t **pThis);
static void dockerContainerInfoDestruct(docker_container_info_t *pThis);

/* utility functions */
static CURLcode docker_get(imdocker_req_t *req, const char* url);
static char* dupDockerContainerName(const char* pname);
static rsRetVal SubmitMsg(docker_cont_logs_inst_t *pInst, docker_cont_logs_buf_t *pBufData,
		const uchar* pszTag);
/* support multi-line */
static rsRetVal
SubmitMsg2(docker_cont_logs_inst_t *pInst, docker_cont_logs_buf_t *pBufData, const uchar* pszTag);
static size_t imdocker_container_list_curlCB(void *data, size_t size, size_t nmemb, void *buffer);
static size_t imdocker_container_logs_curlCB(void *data, size_t size, size_t nmemb, void *buffer);
static sbool get_stream_info(const uchar* data, size_t size, int8_t *stream_type, size_t *payload_size);
static int8_t is_valid_stream_type(int8_t stream_type);

/* Module static data */
DEF_IMOD_STATIC_DATA
DEFobjCurrIf(glbl)
DEFobjCurrIf(prop)
DEFobjCurrIf(parser)
DEFobjCurrIf(datetime)
DEFobjCurrIf(statsobj)

statsobj_t *modStats;
STATSCOUNTER_DEF(ctrSubmit, mutCtrSubmit)
STATSCOUNTER_DEF(ctrLostRatelimit, mutCtrLostRatelimit)
STATSCOUNTER_DEF(ctrCurlError, mutCtrCurlError)

const char* DFLT_dockerAPIUnixSockAddr  = "/var/run/docker.sock";
const char* DFLT_dockerAPIAdd           = "http://localhost:2375";
const char* DFLT_apiVersionStr          = "v1.27";
const char* DFLT_listContainersOptions  = "";
const char* DFLT_getContainerLogOptions = "timestamps=0&follow=1&stdout=1&stderr=1&tail=1";
const char* DFLT_getContainerLogOptionsWithoutTail = "timestamps=0&follow=1&stdout=1&stderr=1";

/* Overall module configuration structure here. */
struct modConfData_s {
	rsconf_t *pConf;  /* our overall config object */
	uchar    *apiVersionStr;
	uchar    *listContainersOptions;
	uchar    *getContainerLogOptions;
	uchar    *getContainerLogOptionsWithoutTail;
	int      iPollInterval;  /* in seconds */
	uchar    *dockerApiUnixSockAddr;
	uchar    *dockerApiAddr;
	sbool    retrieveNewLogsFromStart;
	int      containersLimit;
	int      trimLineOverBytes;
	int      iDfltSeverity;
	int      iDfltFacility;
	sbool    bEscapeLf;
};

static modConfData_t *loadModConf = NULL;
static modConfData_t *runModConf = NULL;

static prop_t *pInputName = NULL;   /* our inputName currently is always "imdocker", and this will hold it */
static prop_t *pLocalHostIP = NULL; /* a pseudo-constant propterty for 127.0.0.1 */

static ratelimit_t *ratelimiter = NULL;

/* module-global parameters */
static struct cnfparamdescr modpdescr[] = {
	{ "apiversionstr", eCmdHdlrString, 0 },
	{ "dockerapiunixsockaddr", eCmdHdlrString, 0 },
	{ "dockerapiaddr", eCmdHdlrString, 0 },
	{ "listcontainersoptions", eCmdHdlrString, 0 },
	{ "getcontainerlogoptions", eCmdHdlrString, 0 },
	{ "pollinginterval", eCmdHdlrPositiveInt, 0 },
	{ "retrievenewlogsfromstart", eCmdHdlrBinary, 0 },
	{ "trimlineoverbytes", eCmdHdlrPositiveInt, 0 },
	{ "defaultseverity", eCmdHdlrSeverity, 0 },
	{ "defaultfacility", eCmdHdlrFacility, 0 },
	{ "escapelf", eCmdHdlrBinary, 0 },
};

static struct cnfparamblk modpblk =
	{ CNFPARAMBLK_VERSION,
		sizeof(modpdescr)/sizeof(struct cnfparamdescr),
		modpdescr
	};

static int bLegacyCnfModGlobalsPermitted; /* are legacy module-global config parameters permitted? */

/* imdocker specific functions */
static rsRetVal
imdockerBufNew(imdocker_buf_t **ppThis) {
	DEFiRet;

	imdocker_buf_t *pThis = (imdocker_buf_t*) calloc(1, sizeof(imdocker_buf_t));
	if (!pThis) { return RS_RET_OUT_OF_MEMORY; }
	*ppThis = pThis;

	RETiRet;
}

static void
imdockerBufDestruct(imdocker_buf_t *pThis) {
	if (pThis) {
		if (pThis->data) {
			free(pThis->data);
			pThis->data = NULL;
		}
		free(pThis);
	}
}

static rsRetVal
dockerContLogsBufNew(docker_cont_logs_buf_t **ppThis) {
	DEFiRet;

	docker_cont_logs_buf_t *pThis = (docker_cont_logs_buf_t*) calloc(1, sizeof(docker_cont_logs_buf_t));
	if (pThis && (iRet = imdockerBufNew(&pThis->buf)) == RS_RET_OK) {
		pThis->stream_type = dst_invalid;
		pThis->bytes_remaining = 0;
		*ppThis = pThis;
	} else {
		dockerContLogsBufDestruct(pThis);
	}

	RETiRet;
}

static void
dockerContLogsBufDestruct(docker_cont_logs_buf_t *pThis) {
	if (pThis) {
		if (pThis->buf) {
			imdockerBufDestruct(pThis->buf);
		}
		free(pThis);
	}
}

static rsRetVal
dockerContLogsBufWrite(docker_cont_logs_buf_t *const pThis, const uchar *const pdata, const size_t write_size) {
	DEFiRet;

	imdocker_buf_t *const mem = pThis->buf;
	if (mem->len + write_size + 1 > mem->data_size) {
		uchar *const pbuf = realloc(mem->data, mem->len + write_size + 1);
		if(pbuf == NULL) {
			LogError(errno, RS_RET_ERR, "%s() - realloc failed!\n", __FUNCTION__);
			ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
		}
		mem->data = pbuf;
		mem->data_size = mem->len+ write_size + 1;
	}
	/* copy the bytes, and advance pdata */
	memcpy(&(mem->data[mem->len]), pdata, write_size);
	mem->len += write_size;
	mem->data[mem->len] = '\0';

	if (write_size > pThis->bytes_remaining) {
		pThis->bytes_remaining = 0;
	} else {
		pThis->bytes_remaining -= write_size;
	}

finalize_it:
	RETiRet;
}

rsRetVal imdockerReqNew(imdocker_req_t **ppThis) {
	DEFiRet;

	imdocker_req_t *pThis = (imdocker_req_t*) calloc(1, sizeof(imdocker_req_t));
	CHKmalloc(pThis);
	pThis->curl = curl_easy_init();
	if (!pThis->curl) {
		ABORT_FINALIZE(RS_RET_ERR);
	}

	CHKiRet(imdockerBufNew(&(pThis->buf)));
	*ppThis = pThis;

finalize_it:
	if (iRet != RS_RET_OK && pThis) {
		imdockerReqDestruct(pThis);
	}
	RETiRet;
}

void imdockerReqDestruct(imdocker_req_t *pThis) {
	if (pThis) {
		if (pThis->buf) {
			imdockerBufDestruct(pThis->buf);
		}

		if (pThis->curl) {
			curl_easy_cleanup(pThis->curl);
			pThis->curl = NULL;
		}
		free(pThis);
	}
}

static rsRetVal
dockerContLogsReqNew(docker_cont_logs_req_t **ppThis, SubmitMsgFuncPtr submitMsg) {
	DEFiRet;

	docker_cont_logs_req_t *pThis = (docker_cont_logs_req_t*) calloc(1, sizeof(docker_cont_logs_req_t));
	CHKmalloc(pThis);
	pThis->submitMsg = submitMsg;
	pThis->curl = curl_easy_init();
	if (!pThis->curl) {
		ABORT_FINALIZE(RS_RET_ERR);
	}

	for (int i = 0; i < dst_stream_type_count; i ++) {
		CHKiRet(dockerContLogsBufNew(&pThis->data_bufs[i]));
	}

	*ppThis = pThis;

finalize_it:
	if (iRet != RS_RET_OK) {
		if (pThis) {
			dockerContLogsReqDestruct(pThis);
		}
	}
	RETiRet;
}

static void
dockerContLogsReqDestruct(docker_cont_logs_req_t *pThis) {
	if (pThis) {
		for (int i = 0; i < dst_stream_type_count; i++) {
			dockerContLogsBufDestruct(pThis->data_bufs[i]);
		}

		if (pThis->curl) {
			curl_easy_cleanup(pThis->curl);
			pThis->curl=NULL;
		}

		free(pThis);
	}
}

/**
 * debugging aide
 */
static rsRetVal
dockerContLogsInstPrint(docker_cont_logs_inst_t * pThis) {
	DEFiRet;
	DBGPRINTF("\t container id: %s\n", pThis->id);
	char* pUrl = NULL;
	curl_easy_getinfo(pThis->logsReq->curl, CURLINFO_EFFECTIVE_URL, &pUrl);
	DBGPRINTF("\t container url: %s\n", pUrl);

	RETiRet;
}

static void
dockerContLogsInstDestruct(docker_cont_logs_inst_t *pThis) {
	if (pThis) {
		if (pThis->id) {
			free((void*)pThis->id);
		}
		if (pThis->container_info) {
			dockerContainerInfoDestruct(pThis->container_info);
		}
		if (pThis->logsReq) {
			dockerContLogsReqDestruct(pThis->logsReq);
		}
		if (pThis->start_regex) {
			free(pThis->start_regex);
			regfree(&pThis->start_preg);
		}
		free(pThis);
	}
}

static rsRetVal
parseLabels(docker_cont_logs_inst_t *inst, const uchar* json) {
	DEFiRet;

	/* parse out if we need to do special handling for mult-line */
	DBGPRINTF("%s() - parsing json=%s\n", __FUNCTION__, json);

	struct fjson_object *json_obj = fjson_tokener_parse((const char*)json);
	struct fjson_object_iterator it = fjson_object_iter_begin(json_obj);
	struct fjson_object_iterator itEnd = fjson_object_iter_end(json_obj);
	while (!fjson_object_iter_equal(&it, &itEnd)) {
		if (Debug) {
			DBGPRINTF("%s - \t%s: '%s'\n",
					__FUNCTION__,
					fjson_object_iter_peek_name(&it),
					fjson_object_get_string(fjson_object_iter_peek_value(&it)));
		}

		if (strcmp(fjson_object_iter_peek_name(&it), DOCKER_CONTAINER_LABEL_KEY_STARTREGEX) == 0) {
			inst->start_regex = (uchar*)strdup(fjson_object_get_string(fjson_object_iter_peek_value(&it)));
			// compile the regex for future use.
			int err = regcomp(&inst->start_preg, fjson_object_get_string(fjson_object_iter_peek_value(&it)),
					REG_EXTENDED);
			if (err != 0) {
				char errbuf[512];
				regerror(err, &inst->start_preg, errbuf, sizeof(errbuf));
				LogError(0, err, "%s() - error in startregex compile: %s", __FUNCTION__, errbuf);
				ABORT_FINALIZE(RS_RET_ERR);
			}
		}
		fjson_object_iter_next(&it);
	}

finalize_it:
	if (json_obj) {
		json_object_put(json_obj);
	}
	RETiRet;
}

static rsRetVal
dockerContLogsInstNew(docker_cont_logs_inst_t **ppThis, const char* id,
		docker_container_info_t *container_info, SubmitMsgFuncPtr submitMsg) {
	DEFiRet;

	docker_cont_logs_inst_t *pThis = NULL;
	CHKmalloc(pThis = calloc(1, sizeof(docker_cont_logs_inst_t)));

	pThis->id = strdup((char*)id);
	strncpy((char*) pThis->short_id, id, sizeof(pThis->short_id)-1);
	CHKiRet(dockerContLogsReqNew(&pThis->logsReq, submitMsg));
	/* make a copy */
	if (container_info) {
		CHKiRet(dockerContainerInfoNew(&pThis->container_info));
		if (container_info->image_id) {
			pThis->container_info->image_id = (uchar*)strdup((char*)container_info->image_id);
		}
		if (container_info->name) {
			const char *pname = (const char*)container_info->name;
			/* removes un-needed characters */
			pThis->container_info->name = (uchar*)dupDockerContainerName(pname);
		}
		if (container_info->json_str_labels) {
			pThis->container_info->json_str_labels =
				(uchar*)strdup((char*)container_info->json_str_labels);
		}
		pThis->container_info->created = container_info->created;
	}
	pThis->start_regex = NULL;
	pThis->prevSegEnd = 0;
	/* initialize based on labels found */
	if (pThis->container_info && pThis->container_info->json_str_labels) {
		parseLabels(pThis, pThis->container_info->json_str_labels);
	}

	*ppThis = pThis;

finalize_it:
	if (iRet != RS_RET_OK) {
		if (pThis) {
			dockerContLogsInstDestruct(pThis);
		}
	}
	RETiRet;
}

static rsRetVal
dockerContLogsInstSetUrl(docker_cont_logs_inst_t *pThis, CURLM *curlm, const char* pUrl) {
	DEFiRet;
	CURLcode ccode = CURLE_OK;
	CURLMcode mcode = CURLM_OK;

	if (curlm) {
		docker_cont_logs_req_t *req = pThis->logsReq;
		if (!runModConf->dockerApiAddr) {
			ccode = curl_easy_setopt(req->curl, CURLOPT_UNIX_SOCKET_PATH,
					runModConf->dockerApiUnixSockAddr);
			if (ccode != CURLE_OK) {
				LogError(0, RS_RET_ERR,
						"imdocker: curl_easy_setopt(CURLOPT_UNIX_SOCKET_PATH) error - %d:%s\n",
						ccode, curl_easy_strerror(ccode));
				ABORT_FINALIZE(RS_RET_ERR);
			}
		}
		ccode = curl_easy_setopt(req->curl, CURLOPT_WRITEFUNCTION, imdocker_container_logs_curlCB);
		if (ccode != CURLE_OK) {
				LogError(0, RS_RET_ERR,
						"imdocker: curl_easy_setopt(CURLOPT_WRITEFUNCTION) error - %d:%s\n",
						ccode, curl_easy_strerror(ccode));
				ABORT_FINALIZE(RS_RET_ERR);
		}

		ccode = curl_easy_setopt(req->curl, CURLOPT_WRITEDATA, pThis);
		if (ccode != CURLE_OK) {
				LogError(0, RS_RET_ERR, "imdocker: curl_easy_setopt(CURLOPT_WRITEDATA) error - %d:%s\n",
						ccode, curl_easy_strerror(ccode));
				ABORT_FINALIZE(RS_RET_ERR);
		}

		ccode = curl_easy_setopt(pThis->logsReq->curl, CURLOPT_URL, pUrl);
		if (ccode != CURLE_OK) {
			LogError(0, RS_RET_ERR, "imdocker: could not set url - %d:%s\n",
					ccode, curl_easy_strerror(ccode));
			ABORT_FINALIZE(RS_RET_ERR);
		}

		ccode = curl_easy_setopt(pThis->logsReq->curl, CURLOPT_PRIVATE, pThis->id);
		if (ccode != CURLE_OK) {
			LogError(0, RS_RET_ERR, "imdocker: could not set private data - %d:%s\n",
					ccode, curl_easy_strerror(ccode));
			ABORT_FINALIZE(RS_RET_ERR);
		}

		mcode = curl_multi_add_handle(curlm, pThis->logsReq->curl);
		if (mcode != CURLM_OK) {
			LogError(0, RS_RET_ERR, "imdocker: error curl_multi_add_handle ret- %d:%s\n",
					mcode, curl_multi_strerror(mcode));
			ABORT_FINALIZE(RS_RET_ERR);
		}
	}

finalize_it:
	if (ccode != CURLE_OK) {
		STATSCOUNTER_INC(ctrCurlError, mutCtrCurlError);
	}
	RETiRet;
}

static rsRetVal
dockerContLogsInstSetUrlById (sbool isInit, docker_cont_logs_inst_t *pThis, CURLM *curlm,
		const char* containerId) {
	char url[256];
	const uchar* container_log_options = runModConf->getContainerLogOptionsWithoutTail;

	if (isInit || !runModConf->retrieveNewLogsFromStart) {
		container_log_options = runModConf->getContainerLogOptions;
	}

	const uchar* pApiAddr = (uchar*)"http:";
	if (runModConf->dockerApiAddr) {
		pApiAddr = runModConf->dockerApiAddr;
	}

	snprintf(url, sizeof(url), "%s/%s/containers/%s/logs?%s",
			pApiAddr, runModConf->apiVersionStr, containerId, container_log_options);
	DBGPRINTF("%s() - url: %s\n", __FUNCTION__, url);

	return dockerContLogsInstSetUrl(pThis, curlm, url);
}

/* special destructor for hashtable object. */
static void
dockerContLogReqsDestructForHashtable(void *pData) {
	docker_cont_logs_inst_t *pThis = (docker_cont_logs_inst_t *) pData;
	dockerContLogsInstDestruct(pThis);
}

static rsRetVal
dockerContLogReqsNew(docker_cont_log_instances_t **ppThis) {
	DEFiRet;

	docker_cont_log_instances_t *pThis = calloc(1, sizeof(docker_cont_log_instances_t));
	CHKmalloc(pThis);
	CHKmalloc(pThis->ht_container_log_insts =
			create_hashtable(7, hash_from_string, key_equals_string,
				dockerContLogReqsDestructForHashtable));

	CHKiConcCtrl(pthread_mutex_init(&pThis->mut, NULL));

	pThis->curlm = curl_multi_init();
	if (!pThis->curlm) {
		ABORT_FINALIZE(RS_RET_ERR);
	}

	*ppThis = pThis;

finalize_it:
	if (iRet != RS_RET_OK) {
		if (pThis) {
			dockerContLogReqsDestruct(pThis);
		}
	}
	RETiRet;
}

static rsRetVal
dockerContLogReqsDestruct(docker_cont_log_instances_t *pThis) {
	DEFiRet;

	if (pThis) {
		if (pThis->ht_container_log_insts) {
			pthread_mutex_lock(&pThis->mut);
			hashtable_destroy(pThis->ht_container_log_insts, 1);
			pthread_mutex_unlock(&pThis->mut);
		}
		if (pThis->last_container_id) {
			free(pThis->last_container_id);
		}
		curl_multi_cleanup(pThis->curlm);
		pthread_mutex_destroy(&pThis->mut);
		free(pThis);
	}

	RETiRet;
}

/* NOTE: not thread safe - used internally to update container log requests */
static rsRetVal
dockerContLogReqsGet(docker_cont_log_instances_t *pThis,
		docker_cont_logs_inst_t** ppContLogsInst, const char *id) {
	DEFiRet;

	if (ppContLogsInst && id) {
		docker_cont_logs_inst_t *pSearchObj = hashtable_search(pThis->ht_container_log_insts, (void*)id);
		if (!pSearchObj) {
			return RS_RET_NOT_FOUND;
		}
		*ppContLogsInst = pSearchObj;
	}

	RETiRet;
}

/* debug print
 *
 * NOTE: not thread safe
 *
 */
static rsRetVal
dockerContLogReqsPrint(docker_cont_log_instances_t *pThis) {
	DEFiRet;
	int count = 0;

	count = hashtable_count(pThis->ht_container_log_insts);
	if (count) {
		int ret = 0;
		struct hashtable_itr *itr = hashtable_iterator(pThis->ht_container_log_insts);

		DBGPRINTF("%s() - All container instances, count=%d...\n", __FUNCTION__, count);
		do {
			docker_cont_logs_inst_t *pObj = hashtable_iterator_value(itr);
			dockerContLogsInstPrint(pObj);
			ret = hashtable_iterator_advance(itr);
		} while (ret);
		free (itr);
		DBGPRINTF("End of container instances.\n");
	}

	RETiRet;
}

/* NOTE: not thread safe */
static rsRetVal
dockerContLogReqsAdd(docker_cont_log_instances_t *pThis,
		docker_cont_logs_inst_t *pContLogsReqInst)
{
	DEFiRet;
	if (!pContLogsReqInst) {
		return RS_RET_ERR;
	}

	uchar *keyName = (uchar*)strdup((char*)pContLogsReqInst->id);

	if (keyName) {
		docker_cont_logs_inst_t *pFind;
		if (RS_RET_NOT_FOUND == dockerContLogReqsGet(pThis, &pFind, (void*)keyName)) {
			if (!hashtable_insert(pThis->ht_container_log_insts, keyName, pContLogsReqInst)) {
				ABORT_FINALIZE(RS_RET_ERR);
			}
			keyName = NULL;
		}
	}
finalize_it:
	free(keyName);
	RETiRet;
}

static rsRetVal
dockerContLogReqsRemove(docker_cont_log_instances_t *pThis, const char *id) {
	DEFiRet;

	if (pThis && id) {
		CHKiConcCtrl(pthread_mutex_lock(&pThis->mut));
		docker_cont_logs_inst_t *pRemoved =
			hashtable_remove(pThis->ht_container_log_insts, (void*)id);
		pthread_mutex_unlock(&pThis->mut);
		if (pRemoved) {
			dockerContLogsInstDestruct(pRemoved);
		} else {
			iRet = RS_RET_NOT_FOUND;
		}
	}
finalize_it:
	RETiRet;
}

static rsRetVal
dockerContainerInfoNew(docker_container_info_t **ppThis) {
	DEFiRet;
	docker_container_info_t* pThis = calloc(1, sizeof(docker_container_info_t));
	CHKmalloc(pThis);
	*ppThis = pThis;

finalize_it:
	RETiRet;
}

static void
dockerContainerInfoDestruct(docker_container_info_t *pThis) {
	if (pThis) {
		if (pThis->image_id) { free(pThis->image_id); }
		if (pThis->name) { free(pThis->name); }
		if (pThis->json_str_labels) { free(pThis->json_str_labels); }
		free(pThis);
	}
}

BEGINbeginCnfLoad
CODESTARTbeginCnfLoad

	dbgprintf("imdocker: beginCnfLoad\n");

	loadModConf = pModConf;
	pModConf->pConf = pConf;

	/* init our settings */
	loadModConf->iPollInterval     = DFLT_pollingInterval; /* in seconds */
	loadModConf->retrieveNewLogsFromStart  = DFLT_retrieveNewLogsFromStart;
	loadModConf->containersLimit   = DFLT_containersLimit;
	loadModConf->trimLineOverBytes = DFLT_trimLineOverBytes;
	loadModConf->bEscapeLf         = DFLT_bEscapeLF;

	/* Use the default url */
	loadModConf->apiVersionStr          = NULL;
	loadModConf->dockerApiUnixSockAddr  = NULL;
	loadModConf->dockerApiAddr          = NULL;
	loadModConf->listContainersOptions  = NULL;
	loadModConf->getContainerLogOptions = NULL;
	loadModConf->getContainerLogOptionsWithoutTail = NULL;
	loadModConf->iDfltFacility = DFLT_FACILITY;
	loadModConf->iDfltSeverity = DFLT_SEVERITY;
ENDbeginCnfLoad

BEGINsetModCnf
	struct cnfparamvals *pvals = NULL;
	int i;
CODESTARTsetModCnf
	pvals = nvlstGetParams(lst, &modpblk, NULL);
	if(pvals == NULL) {
		LogError(0, RS_RET_MISSING_CNFPARAMS, "error processing module "
				"config parameters [module(...)]");
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	if (Debug) {
		dbgprintf("module (global) param blk for imdocker:\n");
		cnfparamsPrint(&modpblk, pvals);
	}

	for(i = 0 ; i < modpblk.nParams ; ++i) {
		dbgprintf("%s() - iteration %d\n", __FUNCTION__,i);
		dbgprintf("%s() - modpblk descr: %s\n", __FUNCTION__, modpblk.descr[i].name);
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(modpblk.descr[i].name, "pollinginterval")) {
			loadModConf->iPollInterval = (int) pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "containterlimit")) {
			loadModConf->containersLimit = (int) pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "trimlineoverbytes")) {
			loadModConf->trimLineOverBytes = (int) pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "listcontainersoptions")) {
			loadModConf->listContainersOptions = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(modpblk.descr[i].name, "getcontainerlogoptions")) {
			loadModConf->getContainerLogOptions = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
			/* also intialize the non-tail version */
			size_t offset = 0;
			char buf[256];
			size_t buf_size = sizeof(buf);
			strncpy(buf, (char*)loadModConf->getContainerLogOptions, buf_size-1);
			size_t option_str_len = strlen((char*)loadModConf->getContainerLogOptions);
			uchar *option_str = calloc(1, option_str_len);
			CHKmalloc(option_str);

			const char *search_str = "tail=";
			size_t search_str_len = strlen(search_str);
			char *token = strtok(buf, "&");

			while (token != NULL) {
				if (strncmp(token, search_str, search_str_len) == 0) {
					token = strtok(NULL, "&");
					continue;
				}
				int len = strlen(token);
				if (offset + len + 1 >= option_str_len) {
					break;
				}
				int bytes = snprintf((char*)option_str + offset,
						(option_str_len - offset), "%s&", token);
				if (bytes <= 0) {
					break;
				}
				offset += bytes;
				token = strtok(NULL, "&");
			}
			loadModConf->getContainerLogOptionsWithoutTail = option_str;
		} else if(!strcmp(modpblk.descr[i].name, "dockerapiunixsockaddr")) {
			loadModConf->dockerApiUnixSockAddr = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(modpblk.descr[i].name, "dockerapiaddr")) {
			loadModConf->dockerApiAddr = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(modpblk.descr[i].name, "apiversionstr")) {
			loadModConf->apiVersionStr = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(modpblk.descr[i].name, "retrievenewlogsfromstart")) {
			loadModConf->retrieveNewLogsFromStart = (sbool) pvals[i].val.d.n;
		} else if (!strcmp(modpblk.descr[i].name, "defaultseverity")) {
			loadModConf->iDfltSeverity = (int) pvals[i].val.d.n;
		} else if (!strcmp(modpblk.descr[i].name, "defaultfacility")) {
			loadModConf->iDfltFacility = (int) pvals[i].val.d.n;
		} else if(!strcmp(modpblk.descr[i].name, "escapelf")) {
			loadModConf->bEscapeLf = (sbool) pvals[i].val.d.n;
		} else {
			LogError(0, RS_RET_INVALID_PARAMS,
					"imdocker: program error, non-handled "
					"param '%s' in setModCnf\n", modpblk.descr[i].name);
		}
	}

	/* disable legacy module-global config directives */
	bLegacyCnfModGlobalsPermitted = 0;

finalize_it:
	if(pvals != NULL)
		cnfparamvalsDestruct(pvals, &modpblk);
ENDsetModCnf

BEGINendCnfLoad
CODESTARTendCnfLoad
ENDendCnfLoad

BEGINcheckCnf
CODESTARTcheckCnf
ENDcheckCnf

BEGINactivateCnf
CODESTARTactivateCnf
	if (!loadModConf->dockerApiUnixSockAddr) {
		loadModConf->dockerApiUnixSockAddr = (uchar*) strdup(DFLT_dockerAPIUnixSockAddr);
	}
	if (!loadModConf->apiVersionStr) {
		loadModConf->apiVersionStr = (uchar*) strdup(DFLT_apiVersionStr);
	}
	if (!loadModConf->listContainersOptions) {
		loadModConf->listContainersOptions = (uchar*) strdup(DFLT_listContainersOptions);
	}
	if (!loadModConf->getContainerLogOptions) {
		loadModConf->getContainerLogOptions = (uchar*) strdup(DFLT_getContainerLogOptions);
	}
if (!loadModConf->getContainerLogOptionsWithoutTail) {
		loadModConf->getContainerLogOptionsWithoutTail =
			(uchar*) strdup(DFLT_getContainerLogOptionsWithoutTail);
	}
	runModConf = loadModConf;

	/* support statistics gathering */
	CHKiRet(statsobj.Construct(&modStats));
	CHKiRet(statsobj.SetName(modStats, UCHAR_CONSTANT("imdocker")));
	CHKiRet(statsobj.SetOrigin(modStats, UCHAR_CONSTANT("imdocker")));

	STATSCOUNTER_INIT(ctrSubmit, mutCtrSubmit);
	CHKiRet(statsobj.AddCounter(modStats, UCHAR_CONSTANT("submitted"),
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrSubmit));

	STATSCOUNTER_INIT(ctrLostRatelimit, mutCtrLostRatelimit);
	CHKiRet(statsobj.AddCounter(modStats, UCHAR_CONSTANT("ratelimit.discarded"),
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrLostRatelimit));

	STATSCOUNTER_INIT(ctrCurlError, mutCtrCurlError);
	CHKiRet(statsobj.AddCounter(modStats, UCHAR_CONSTANT("curl.errors"),
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrCurlError));

	CHKiRet(statsobj.ConstructFinalize(modStats));
	/* end stats */
finalize_it:
ENDactivateCnf

BEGINfreeCnf
CODESTARTfreeCnf
	if (loadModConf->dockerApiUnixSockAddr) {
		free(loadModConf->dockerApiUnixSockAddr);
	}
	if (loadModConf->dockerApiAddr) {
		free(loadModConf->dockerApiAddr);
	}
	if (loadModConf->apiVersionStr) {
		free(loadModConf->apiVersionStr);
	}
	if (loadModConf->getContainerLogOptions) {
		free(loadModConf->getContainerLogOptions);
	}
	if (loadModConf->getContainerLogOptionsWithoutTail) {
		free(loadModConf->getContainerLogOptionsWithoutTail);
	}
	if (loadModConf->listContainersOptions) {
		free(loadModConf->listContainersOptions);
	}
	statsobj.Destruct(&modStats);
ENDfreeCnf

static rsRetVal
addDockerMetaData(const uchar* container_id, docker_container_info_t* pinfo, smsg_t *pMsg) {
	const uchar *names[4] = {
		(const uchar*) DOCKER_CONTAINER_ID_PARSE_NAME,
		(const uchar*) DOCKER_CONTAINER_NAMES_PARSE_NAME,
		(const uchar*) DOCKER_CONTAINER_IMAGEID_PARSE_NAME,
		(const uchar*) DOCKER_CONTAINER_LABELS_PARSE_NAME
	};

	const uchar * empty_str= (const uchar*) "";
	const uchar *id = container_id ? container_id : empty_str;
	const uchar *name = pinfo->name ? pinfo->name : empty_str;
	const uchar *image_id = pinfo->image_id ? pinfo->image_id : empty_str;
	const uchar *json_str_labels = pinfo->json_str_labels ? pinfo->json_str_labels : empty_str;

	const uchar *values[4] = {
		id,
		name,
		image_id,
		json_str_labels
	};

	return msgAddMultiMetadata(pMsg, names, values, 4);
}

static rsRetVal
enqMsg(docker_cont_logs_inst_t *pInst, uchar *msg, size_t len, const uchar *pszTag,
		int facility, int severity, struct timeval *tp)
{
	struct syslogTime st;
	smsg_t *pMsg;
	DEFiRet;

	if (!msg) {
		return RS_RET_ERR;
	}

	if (tp == NULL) {
		CHKiRet(msgConstruct(&pMsg));
	} else {
		datetime.timeval2syslogTime(tp, &st, TIME_IN_LOCALTIME);
		CHKiRet(msgConstructWithTime(&pMsg, &st, tp->tv_sec));
	}
	MsgSetFlowControlType(pMsg, eFLOWCTL_LIGHT_DELAY);
	MsgSetInputName(pMsg, pInputName);
	MsgSetRawMsg(pMsg, (char*)msg, len);

	if (loadModConf->bEscapeLf) {
		parser.SanitizeMsg(pMsg);
	} else {
		/* Perform some of the SanitizeMsg operations here - specifically:
		 * - remove NULL character at end of message.
		 * - drop trailing LFs.
		 * See SanitizeMsg() for more info.
		 */
		size_t lenMsg = pMsg->iLenRawMsg;
		uchar *pszMsg = pMsg->pszRawMsg;

		if(pszMsg[lenMsg-1] == '\0') {
			DBGPRINTF("dropped NULL at very end of message\n");
			lenMsg--;
		}

		if(glbl.GetParserDropTrailingLFOnReception(loadModConf->pConf)
				&& lenMsg > 0 && pszMsg[lenMsg-1] == '\n') {
			DBGPRINTF("dropped LF at very end of message (DropTrailingLF is set)\n");
			lenMsg--;
			pszMsg[lenMsg] = '\0';
		}
		pMsg->iLenRawMsg = lenMsg;
	}

	MsgSetMSGoffs(pMsg, 0);  /* we do not have a header... */
	MsgSetRcvFrom(pMsg, glbl.GetLocalHostNameProp());
	if (pLocalHostIP) { MsgSetRcvFromIP(pMsg, pLocalHostIP); }
	MsgSetHOSTNAME(pMsg, glbl.GetLocalHostName(), ustrlen(glbl.GetLocalHostName()));
	MsgSetTAG(pMsg, pszTag, ustrlen(pszTag));
	pMsg->iFacility = facility;
	pMsg->iSeverity = severity;

	/* docker container metadata */
	addDockerMetaData((const uchar*)pInst->short_id, pInst->container_info, pMsg);

	const char *name = (const char*)pInst->container_info->name;
	DBGPRINTF("imdocker: %s - %s:%s\n", __FUNCTION__, name, msg);
	CHKiRet(ratelimitAddMsg(ratelimiter, NULL, pMsg));
	STATSCOUNTER_INC(ctrSubmit, mutCtrSubmit);

finalize_it:
	if (iRet == RS_RET_DISCARDMSG)
		STATSCOUNTER_INC(ctrLostRatelimit, mutCtrLostRatelimit)

	RETiRet;
}

static int8_t
is_valid_stream_type(int8_t stream_type) {
	return (dst_invalid < stream_type && stream_type < dst_stream_type_count);
}

/* For use to get docker specific stream information */
static sbool
get_stream_info(const uchar* data, size_t size, int8_t *stream_type, size_t *payload_size) {
	if (size < 8 || !data || !stream_type || !payload_size) {
		return 0;
	}
	const uchar* pdata = data;
	*stream_type = pdata[0];
	pdata += 4;
	uint32_t len = 0;
	memcpy(&len, pdata, sizeof(len));
	*payload_size = ntohl(len);
	return 1;
}
#ifdef ENABLE_DEBUG_BYTE_BUFFER
static void debug_byte_buffer(const uchar* data, size_t size) {
	if (Debug) {
		DBGPRINTF("%s() - ENTER, size=%lu\n", __FUNCTION__, size);
		for (size_t i = 0; i < size; i++) {
			DBGPRINTF("0x%02x,", data[i]);
		}
		DBGPRINTF("\n");
	}
}
#endif

/**
 * imdocker_container_list_curlCB
 *
 * Callback function for CURLOPT_WRITEFUNCTION to get
 * the results of a docker api call to list all containers.
 *
 */
static size_t
imdocker_container_list_curlCB(void *data, size_t size, size_t nmemb, void *buffer) {
	DEFiRet;

	size_t realsize = size*nmemb;
	uchar		*pbuf=NULL;
	imdocker_buf_t *mem = (imdocker_buf_t*)buffer;

	if ((pbuf = realloc(mem->data, mem->len + realsize + 1)) == NULL) {
		LogError(errno, RS_RET_ERR, "%s() - realloc failed!\n", __FUNCTION__);
		ABORT_FINALIZE(RS_RET_ERR);
	}

	mem->data = pbuf;
	mem->data_size = mem->len + realsize + 1;

	memcpy(&(mem->data[mem->len]), data, realsize);
	mem->len += realsize;
	mem->data[mem->len] = 0;

#ifdef ENABLE_DEBUG_BYTE_BUFFER
	debug_byte_buffer((const uchar*) data, realsize);
#endif
finalize_it:
	if (iRet != RS_RET_OK) {
		return 0;
	}
	return realsize;
}

static rsRetVal
SubmitMultiLineMsg(docker_cont_logs_inst_t *pInst, docker_cont_logs_buf_t *pBufData,
		const uchar* pszTag, size_t len) {
	DEFiRet;

	imdocker_buf_t *mem = (imdocker_buf_t*)pBufData->buf;
	DBGPRINTF("%s() {type=%d, len=%u} %s\n",
			__FUNCTION__, pBufData->stream_type, (unsigned int)mem->len, mem->data);

	uchar* message = (uchar*)mem->data;
	int facility = loadModConf->iDfltFacility;
	int severity = pBufData->stream_type == dst_stderr ? LOG_ERR : loadModConf->iDfltSeverity;
	enqMsg(pInst, message, len, (const uchar*)pszTag, facility, severity, NULL);

	size_t size = mem->len - pInst->prevSegEnd;
	memmove(mem->data, mem->data+pInst->prevSegEnd, size);
	mem->data[len] = '\0';
	mem->len = size;
	pBufData->bytes_remaining = 0;

	RETiRet;
}

static rsRetVal
SubmitMsgWithStartRegex(docker_cont_logs_inst_t *pInst, docker_cont_logs_buf_t *pBufData, const uchar* pszTag) {
	imdocker_buf_t *mem = (imdocker_buf_t*)pBufData->buf;
	/* must be null terminated string */
	assert(mem->data[mem->len] == 0 || mem->data[mem->len] == '\0');
	const char* thisLine = (const char*) mem->data;

	if (pInst->prevSegEnd) {
		thisLine = (const char*) mem->data+pInst->prevSegEnd;
	}
	DBGPRINTF("prevSeg: %d, thisLine: '%s'\n", pInst->prevSegEnd, thisLine);
	DBGPRINTF("line(s) so far: '%s'\n", mem->data);

	/* check if this line is a start of multi-line message */
	regex_t *start_preg = (pInst->start_regex == NULL) ? NULL : &pInst->start_preg;
	const int isStartMatch = start_preg ?
		!regexec(start_preg, (char*)thisLine, 0, NULL, 0) : 0;

	if (isStartMatch && pInst->prevSegEnd != 0) {
		SubmitMultiLineMsg(pInst, pBufData, pszTag, pInst->prevSegEnd);
		pInst->prevSegEnd = 0;
		FINALIZE;
	} else {
		/* just continue parsing using same buffer */
		pInst->prevSegEnd = mem->len;
	}

finalize_it:
	return RS_RET_OK;
}

static rsRetVal
SubmitMsg2(docker_cont_logs_inst_t *pInst, docker_cont_logs_buf_t *pBufData, const uchar* pszTag) {
	imdocker_buf_t *mem = (imdocker_buf_t*)pBufData->buf;
	DBGPRINTF("%s() - {type=%d, len=%u} %s\n",
			__FUNCTION__, pBufData->stream_type, (unsigned int)mem->len, mem->data);

	if (pInst->start_regex) {
		SubmitMsgWithStartRegex(pInst, pBufData, pszTag);
	} else {
		SubmitMsg(pInst, pBufData, pszTag);
	}
	return RS_RET_OK;
}

static rsRetVal
SubmitMsg(docker_cont_logs_inst_t *pInst, docker_cont_logs_buf_t *pBufData, const uchar* pszTag) {
	imdocker_buf_t *mem = (imdocker_buf_t*)pBufData->buf;
	DBGPRINTF("%s() - {type=%d, len=%u} %s\n",
			__FUNCTION__, pBufData->stream_type, (unsigned int)mem->len, mem->data);

	uchar* message = mem->data;
	int facility = loadModConf->iDfltFacility;
	int severity = pBufData->stream_type == dst_stderr ? LOG_ERR : loadModConf->iDfltSeverity;
	enqMsg(pInst, message, mem->len, (const uchar*)pszTag, facility, severity, NULL);

	/* clear existing buffer. */
	mem->len = 0;
	memset(mem->data, 0, mem->data_size);
	pBufData->bytes_remaining = 0;

	return RS_RET_OK;
}

/** imdocker_container_logs_curlCB
 *
 * Callback function for CURLOPT_WRITEFUNCTION, gets container logs
 *
 * The main container log stream handler. This function is registerred with curl to
 * as callback to handle container log streaming. It follows the docker stream protocol
 * as described in the docker container logs api. As per docker's api documentation,
 * Docker Stream format:
 * When the TTY setting is disabled in POST /containers/create, the stream over the
 * hijacked connected is multiplexed to separate out stdout and stderr. The stream
 * consists of a series of frames, each containing a header and a payload.
 *
 * The header contains the information which the stream writes (stdout or stderr). It also
 * contains the size of the associated frame encoded in the last four bytes (uint32).
 *
 * It is encoded on the first eight bytes like this:
 *
 * header := [8]byte{STREAM_TYPE, 0, 0, 0, SIZE1, SIZE2, SIZE3, SIZE4}
 * STREAM_TYPE can be:
 * 0: stdin (is written on stdout)
 * 1: stdout
 * 2: stderr
 *
 * Docker sends out data in 16KB sized frames, however with the addition of a header
 * of 8 bytes, a frame may be split into 2 chunks by curl. The 2nd chunk will only
 * contain enough data to complete the frame (8 leftever bytes). Including the header,
 * this amounts to 16 bytes; 8 bytes for the header, and 8 bytes for the remaining frame
 * data.
 *
 */
static size_t
imdocker_container_logs_curlCB(void *data, size_t size, size_t nmemb, void *buffer) {
	DEFiRet;

	const uint8_t frame_size = 8;
	const char imdocker_eol_char = '\n';
	int8_t stream_type = dst_invalid;

	docker_cont_logs_inst_t* pInst = (docker_cont_logs_inst_t*) buffer;
	docker_cont_logs_req_t* req = pInst->logsReq;

	size_t realsize = size*nmemb;
	const uchar* pdata = data;
	size_t write_size = 0;

#ifdef ENABLE_DEBUG_BYTE_BUFFER
	debug_byte_buffer((const uchar*) data, realsize);
#endif

	if (req->data_bufs[dst_stdout]->bytes_remaining || req->data_bufs[dst_stderr]->bytes_remaining) {
		/* on continuation, stream types should matches with previous */
		if (req->data_bufs[dst_stdout]->bytes_remaining) {
			if (req->data_bufs[dst_stderr]->bytes_remaining != 0) {
				ABORT_FINALIZE(RS_RET_ERR);
			}
		} else if (req->data_bufs[dst_stderr]->bytes_remaining) {
			if (req->data_bufs[dst_stdout]->bytes_remaining != 0) {
				ABORT_FINALIZE(RS_RET_ERR);
			}
		}

		stream_type = req->data_bufs[dst_stdout]->bytes_remaining ? dst_stdout : dst_stderr;
		docker_cont_logs_buf_t *pDataBuf = req->data_bufs[stream_type];

		/* read off the remaining bytes */
		DBGPRINTF("Chunk continuation, remaining bytes: type: %d, "
				"bytes remaining: %u, realsize: %u, data pos: %u\n",
				stream_type, (unsigned int)pDataBuf->bytes_remaining,
				(unsigned int)realsize, (unsigned int)pDataBuf->buf->len);

		write_size = MIN(pDataBuf->bytes_remaining, realsize);
		CHKiRet(dockerContLogsBufWrite(pDataBuf, pdata, write_size));

		/* submit it */
		if (pDataBuf->bytes_remaining == 0) {
			imdocker_buf_t *mem = pDataBuf->buf;
			if (mem->data[mem->len-1] == imdocker_eol_char) {
				const char* szContainerId = NULL;
				CURLcode ccode;
				if(CURLE_OK != (ccode = curl_easy_getinfo(req->curl,
								CURLINFO_PRIVATE,
								&szContainerId))) {
					LogError(0, RS_RET_ERR,
							"imdocker: could not get private data req[%p] - %d:%s\n",
							req->curl, ccode, curl_easy_strerror(ccode));
					ABORT_FINALIZE(RS_RET_ERR);
				}
				req->submitMsg(pInst, pDataBuf, (const uchar*)DOCKER_TAG_NAME);
			}
		}

		pdata += write_size;
	}

	/* not enough room left */
	if ((size_t)(pdata - (const uchar*)data) >= realsize) {
		return (pdata - (const uchar*)data);
	}

	size_t payload_size = 0;
	const uchar* pread = pdata + frame_size;
	docker_cont_logs_buf_t* pDataBuf = NULL;

	if (get_stream_info(pdata, realsize, &stream_type, &payload_size)
				&& is_valid_stream_type(stream_type)) {
		pDataBuf = req->data_bufs[stream_type];
		pDataBuf->stream_type = stream_type;
		pDataBuf->bytes_remaining = payload_size;
		write_size = MIN(payload_size, realsize - frame_size);
	} else {
		/* copy all the data and submit to prevent data loss */
		stream_type = req->data_bufs[dst_stderr]->bytes_remaining ? dst_stderr : dst_stdout;

		pDataBuf = req->data_bufs[stream_type];
		pDataBuf->stream_type = stream_type;

		/* just write everything out */
		pDataBuf->bytes_remaining = 0;
		write_size = realsize;
		pread = pdata;
	}

	/* allocate the expected payload size */
	CHKiRet(dockerContLogsBufWrite(pDataBuf, pread, write_size));
	if (pDataBuf->bytes_remaining == 0) {
		DBGPRINTF("%s() - write size is same as payload_size\n", __FUNCTION__);
		req->submitMsg(pInst, pDataBuf, (const uchar*)DOCKER_TAG_NAME);
	}

finalize_it:
	if (iRet != RS_RET_OK) {
		return 0;
	}
	return realsize;
}

CURLcode docker_get(imdocker_req_t *req, const char* url) {
	CURLcode ccode;

	if (!runModConf->dockerApiAddr) {
		if ((ccode = curl_easy_setopt(req->curl, CURLOPT_UNIX_SOCKET_PATH, runModConf->dockerApiUnixSockAddr))
				!= CURLE_OK) {
			STATSCOUNTER_INC(ctrCurlError, mutCtrCurlError);
			LogError(0, RS_RET_ERR, "imdocker: curl_easy_setopt(CURLOPT_UNIX_SOCKET_PATH) error - %d:%s\n",
					ccode, curl_easy_strerror(ccode));
			return ccode;
		}
	}
	if ((ccode = curl_easy_setopt(req->curl, CURLOPT_WRITEFUNCTION, imdocker_container_list_curlCB)) != CURLE_OK) {
		STATSCOUNTER_INC(ctrCurlError, mutCtrCurlError);
		LogError(0, RS_RET_ERR, "imdocker: curl_easy_setopt(CURLOPT_WRITEFUNCTION) error - %d:%s\n",
				ccode, curl_easy_strerror(ccode));
		return ccode;
	}
	if ((ccode = curl_easy_setopt(req->curl, CURLOPT_WRITEDATA, req->buf)) != CURLE_OK) {
		STATSCOUNTER_INC(ctrCurlError, mutCtrCurlError);
		LogError(0, RS_RET_ERR, "imdocker: curl_easy_setopt(CURLOPT_WRITEDATA) error - %d:%s\n",
				ccode, curl_easy_strerror(ccode));
		return ccode;
	}

	if ((ccode = curl_easy_setopt(req->curl, CURLOPT_URL, url)) != CURLE_OK) {
		STATSCOUNTER_INC(ctrCurlError, mutCtrCurlError);
		LogError(0, RS_RET_ERR, "imdocker: curl_easy_setopt(CURLOPT_URL) error - %d:%s\n",
				ccode, curl_easy_strerror(ccode));
		return ccode;
	}
	CURLcode response = curl_easy_perform(req->curl);

	return response;
}

static char*
dupDockerContainerName(const char* pname) {
	int len = strlen(pname);
	if (len >= 2 && *pname == '/') {
		/* skip '/' character */
		return strdup(pname+1);
	} else {
		return strdup(pname);
	}
}

static rsRetVal
process_json(sbool isInit, const char* json, docker_cont_log_instances_t *pInstances) {
	DEFiRet;
	struct fjson_object *json_obj = NULL;
	int mut_locked = 0;
	DBGPRINTF("%s() - parsing json=%s\n", __FUNCTION__, json);

	if (!pInstances) {
		ABORT_FINALIZE(RS_RET_OK);
	}

	json_obj = fjson_tokener_parse(json);
	if (!json_obj || !fjson_object_is_type(json_obj, fjson_type_array)) {
		ABORT_FINALIZE(RS_RET_OK);
	}

	int length = fjson_object_array_length(json_obj);
	/* LOCK the update process. */
	CHKiConcCtrl(pthread_mutex_lock(&pInstances->mut));
	mut_locked = 1;

	for (int i = 0; i < length; i++) {
		fjson_object* p_json_elm = json_object_array_get_idx(json_obj, i);

		DBGPRINTF("element: %d...\n", i);
		if (p_json_elm) {
			const char *containerId=NULL;
			docker_container_info_t containerInfo = {
				.name=NULL,
				.image_id=NULL,
				.created=0,
				.json_str_labels=NULL
			};

			struct fjson_object_iterator it = fjson_object_iter_begin(p_json_elm);
			struct fjson_object_iterator itEnd = fjson_object_iter_end(p_json_elm);
			while (!fjson_object_iter_equal(&it, &itEnd)) {
				if (Debug) {
					DBGPRINTF("\t%s: '%s'\n",
							fjson_object_iter_peek_name(&it),
							fjson_object_get_string(fjson_object_iter_peek_value(&it)));
				}

				if (strcmp(fjson_object_iter_peek_name(&it), DOCKER_CONTAINER_ID_PARSE_NAME) == 0) {
					containerId =
						fjson_object_get_string(fjson_object_iter_peek_value(&it));
				} else if (strcmp(fjson_object_iter_peek_name(&it),
							DOCKER_CONTAINER_NAMES_PARSE_NAME) == 0) {
					int names_array_length =
						fjson_object_array_length(fjson_object_iter_peek_value(&it));
					if (names_array_length) {
						fjson_object* names_elm =
							json_object_array_get_idx(fjson_object_iter_peek_value(&it), 0);
						containerInfo.name = (uchar*)fjson_object_get_string(names_elm);
					}
				} else if (strcmp(fjson_object_iter_peek_name(&it),
							DOCKER_CONTAINER_IMAGEID_PARSE_NAME) == 0) {
					containerInfo.image_id =
						(uchar*)fjson_object_get_string(
									fjson_object_iter_peek_value(&it)
									);
				} else if (strcmp(fjson_object_iter_peek_name(&it),
							DOCKER_CONTAINER_CREATED_PARSE_NAME) == 0) {
					containerInfo.created =
						fjson_object_get_int64(
									fjson_object_iter_peek_value(&it)
									);
				} else if (strcmp(fjson_object_iter_peek_name(&it),
							DOCKER_CONTAINER_LABELS_PARSE_NAME) == 0) {
					containerInfo.json_str_labels =
						(uchar*) fjson_object_get_string(
									fjson_object_iter_peek_value(&it)
									);
					DBGPRINTF("labels: %s\n", containerInfo.json_str_labels);
				}
				fjson_object_iter_next(&it);
			}

			if (containerId) {
				docker_cont_logs_inst_t *pInst = NULL;
				iRet = dockerContLogReqsGet(pInstances, &pInst, containerId);
				if (iRet == RS_RET_NOT_FOUND) {
#ifdef USE_MULTI_LINE
					if (dockerContLogsInstNew(&pInst, containerId, &containerInfo, SubmitMsg2)
#else
					if (dockerContLogsInstNew(&pInst, containerId, &containerInfo, SubmitMsg)
#endif
							== RS_RET_OK) {
						if (pInstances->last_container_created < containerInfo.created) {
							pInstances->last_container_created = containerInfo.created;
							if (pInstances->last_container_id) {
								free(pInstances->last_container_id);
							}
							pInstances->last_container_id = (uchar*)strdup(containerId);
							DBGPRINTF("last_container_id updated: ('%s', %u)\n",
									pInstances->last_container_id,
									(unsigned)pInstances->last_container_created);
						}
						CHKiRet(dockerContLogsInstSetUrlById(isInit, pInst,
									pInstances->curlm, containerId));
						CHKiRet(dockerContLogReqsAdd(pInstances, pInst));
					}
				}
			}
		}
	}

finalize_it:
	if (mut_locked) {
		pthread_mutex_unlock(&pInstances->mut);
	}
	if (json_obj) {
		json_object_put(json_obj);
	}
	RETiRet;
}

static rsRetVal
getContainerIds(sbool isInit, docker_cont_log_instances_t *pInstances, const char* url) {
	DEFiRet;
	imdocker_req_t *req=NULL;

	CHKiRet(imdockerReqNew(&req));

	CURLcode response = docker_get(req, url);
	if (response != CURLE_OK) {
		DBGPRINTF("%s() - curl response: %d\n", __FUNCTION__, response);
		ABORT_FINALIZE(RS_RET_ERR);
	}

	CHKiRet(process_json(isInit, (const char*)req->buf->data, pInstances));

finalize_it:
	if (req) {
		imdockerReqDestruct(req);
	}
	RETiRet;
}

static rsRetVal
getContainerIdsAndAppend(sbool isInit, docker_cont_log_instances_t *pInstances) {
	DEFiRet;

	char url[256];
	const uchar* pApiAddr = (uchar*)"http:";

	if (runModConf->dockerApiAddr) {
		pApiAddr = runModConf->dockerApiAddr;
	}

	/*
	 * TODO: consider if we really need 'isInit' parameter. I suspect we don't need it
	 * and i'm almost certain Travis CI will complain its not used.
	 */
	if (pInstances->last_container_id) {
		snprintf(url, sizeof(url), "%s/%s/containers/json?%s&filters={\"since\":[\"%s\"]}",
				pApiAddr, runModConf->apiVersionStr, runModConf->listContainersOptions,
				pInstances->last_container_id);
	} else {
		snprintf(url, sizeof(url), "%s/%s/containers/json?%s",
			pApiAddr, runModConf->apiVersionStr, runModConf->listContainersOptions);
	}
	DBGPRINTF("listcontainers url: %s\n", url);

	CHKiRet(getContainerIds(isInit, pInstances, (const char*)url));
	if (Debug) { dockerContLogReqsPrint(pInstances); }

finalize_it:
	RETiRet;
}

static void
cleanupCompletedContainerRequests(docker_cont_log_instances_t *pInstances) {
	// clean up
	int rc=0, msgs_left=0;
	CURLMsg *msg=NULL;
	CURL *pCurl;

	while ((msg = curl_multi_info_read(pInstances->curlm, &msgs_left))) {
		if (msg->msg == CURLMSG_DONE) {
			pCurl = msg->easy_handle;
			rc = msg->data.result;
			if (rc != CURLE_OK) {
				STATSCOUNTER_INC(ctrCurlError, mutCtrCurlError);
				LogError(0, RS_RET_ERR, "imdocker: %s() - curl error code: %d:%s\n",
						__FUNCTION__, rc, curl_multi_strerror(rc));
				continue;
			}

			CURLcode ccode;
			if (Debug) {
				long http_status=0;
				curl_easy_getinfo(pCurl, CURLINFO_RESPONSE_CODE, &http_status);
				DBGPRINTF("http status: %lu\n", http_status);
			}
			curl_multi_remove_handle(pInstances->curlm, pCurl);

			const char* szContainerId = NULL;
			if ((ccode = curl_easy_getinfo(pCurl, CURLINFO_PRIVATE, &szContainerId)) == CURLE_OK) {
				DBGPRINTF("container disconnected: %s\n", szContainerId);
				dockerContLogReqsRemove(pInstances, szContainerId);
				DBGPRINTF("container removed...\n");
			} else {
				LogError(0, RS_RET_ERR, "imdocker: private data not found "
						"curl_easy_setopt(CURLINFO_PRIVATE) error - %d:%s\n",
						ccode, curl_easy_strerror(ccode));
				STATSCOUNTER_INC(ctrCurlError, mutCtrCurlError);
			}
		}
	}
}

static rsRetVal
processAndPollContainerLogs(docker_cont_log_instances_t *pInstances) {
	DEFiRet;
	int count=0;

	count = hashtable_count(pInstances->ht_container_log_insts);
	DBGPRINTF("%s() - container instances: %d\n", __FUNCTION__, count);

	int still_running=0;

	curl_multi_perform(pInstances->curlm, &still_running);
	do {
		int numfds = 0;

		int res = curl_multi_wait(pInstances->curlm, NULL, 0, 1000, &numfds);
		if (res != CURLM_OK) {
			LogError(0, RS_RET_ERR, "error: curl_multi_wait() numfds=%d, res=%d:%s\n",
					numfds, res, curl_multi_strerror(res));
			return res;
		}

		int prev_still_running = still_running;
		curl_multi_perform(pInstances->curlm, &still_running);

		if (prev_still_running > still_running) {
			cleanupCompletedContainerRequests(pInstances);
		}

	} while (still_running && glbl.GetGlobalInputTermState() == 0);

	cleanupCompletedContainerRequests(pInstances);

	RETiRet;
}

static void*
getContainersTask(void *pdata) {
	docker_cont_log_instances_t *pInstances = (docker_cont_log_instances_t*) pdata;

	while(glbl.GetGlobalInputTermState() == 0) {
		srSleep(runModConf->iPollInterval, 10);
		getContainerIdsAndAppend(false, pInstances);
	}
	return pdata;
}

/* This function is called to gather input. */
BEGINrunInput
	rsRetVal localRet = RS_RET_OK;
	docker_cont_log_instances_t *pInstances=NULL;
	pthread_t thrd_id; /* the worker's thread ID */
	pthread_attr_t thrd_attr;
	int get_containers_thread_initialized = 0;
	time_t now;
CODESTARTrunInput
	datetime.GetTime(&now);

	CHKiRet(ratelimitNew(&ratelimiter, "imdocker", NULL));
	curl_global_init(CURL_GLOBAL_ALL);
	localRet = dockerContLogReqsNew(&pInstances);
	if (localRet != RS_RET_OK) {
		return localRet;
	}
	pInstances->time_started = now;

	/* get all current containers now */
	CHKiRet(getContainerIdsAndAppend(true, pInstances));

	/* using default stacksize */
	CHKiConcCtrl(pthread_attr_init(&thrd_attr));
	CHKiConcCtrl(pthread_create(&thrd_id, &thrd_attr, getContainersTask, pInstances));
	get_containers_thread_initialized = 1;

	while(glbl.GetGlobalInputTermState() == 0) {
		CHKiRet(processAndPollContainerLogs(pInstances));
		if (glbl.GetGlobalInputTermState() == 0) {
			/* exited from processAndPollContainerLogs, sleep before retrying */
			srSleep(1, 10);
		}
	}

finalize_it:
	if (get_containers_thread_initialized) {
		pthread_kill(thrd_id, SIGTTIN);
		pthread_join(thrd_id, NULL);
		pthread_attr_destroy(&thrd_attr);
	}
	if (pInstances) {
		dockerContLogReqsDestruct(pInstances);
	}
	if (ratelimiter) {
		ratelimitDestruct(ratelimiter);
	}
ENDrunInput

BEGINwillRun
CODESTARTwillRun
ENDwillRun

BEGINafterRun
CODESTARTafterRun
ENDafterRun

BEGINmodExit
CODESTARTmodExit
	if(pInputName != NULL)
		prop.Destruct(&pInputName);

	if(pLocalHostIP != NULL)
		prop.Destruct(&pLocalHostIP);

	objRelease(parser, CORE_COMPONENT);
	objRelease(glbl, CORE_COMPONENT);
	objRelease(prop, CORE_COMPONENT);
	objRelease(statsobj, CORE_COMPONENT);
	objRelease(datetime, CORE_COMPONENT);
ENDmodExit

BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURENonCancelInputTermination)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature

BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_IMOD_QUERIES
CODEqueryEtryPt_STD_CONF2_QUERIES
CODEqueryEtryPt_STD_CONF2_setModCnf_QUERIES
CODEqueryEtryPt_IsCompatibleWithFeature_IF_OMOD_QUERIES
ENDqueryEtryPt

BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr

	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(prop, CORE_COMPONENT));
	CHKiRet(objUse(statsobj, CORE_COMPONENT));
	CHKiRet(objUse(datetime, CORE_COMPONENT));
	CHKiRet(objUse(parser, CORE_COMPONENT));

	DBGPRINTF("imdocker version %s initializing\n", VERSION);

	/* we need to create the inputName property (only once during our lifetime) */
	CHKiRet(prop.Construct(&pInputName));
	CHKiRet(prop.SetString(pInputName, UCHAR_CONSTANT("imdocker"), sizeof("imdocker") - 1));
	CHKiRet(prop.ConstructFinalize(pInputName));

ENDmodInit
