/* omkafka.c
 * This output plugin make rsyslog talk to Apache Kafka.
 *
 * Copyright 2014 by Adiscon GmbH.
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
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/uio.h>
#include <librdkafka/rdkafka.h>
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include "atomic.h"

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("omkafka")

/* internal structures
 */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(errmsg)

#define MAX_ERRMSG 1024 /* max size of error messages that we support */

#define NO_FIXED_PARTITION -1	/* signifies that no fixed partition config exists */

struct kafka_params {
	const char *name;
	const char *val;
};

typedef struct _instanceData {
	char *topic;
	uchar *tplName;		/* assigned output template */
	char *brokers;
	int fixedPartition;
	int nPartitions;
	int32_t currPartition;
	int nConfParams;
	struct kafka_params *confParams;
	int nTopicConfParams;
	struct kafka_params *topicConfParams;
	uchar *errorFile;
	int fdErrFile;		/* error file fd or -1 if not open */
	pthread_mutex_t mutErrFile;
} instanceData;

typedef struct wrkrInstanceData {
	instanceData *pData;
	int bIsOpen;
	int bReportErrs;
	rd_kafka_t *rk;
	rd_kafka_topic_t *rkt;
} wrkrInstanceData_t;


/* tables for interfacing with the v6 config system */
/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {
	{ "topic", eCmdHdlrString, CNFPARAM_REQUIRED },
	{ "partitions.number", eCmdHdlrPositiveInt, 0 },
	{ "partitions.usefixed", eCmdHdlrNonNegInt, 0 }, /* expert parameter, "nails" partition */
	{ "broker", eCmdHdlrArray, 0 },
	{ "confparam", eCmdHdlrArray, 0 },
	{ "topicconfparam", eCmdHdlrArray, 0 },
	{ "errorfile", eCmdHdlrGetWord, 0 },
	{ "template", eCmdHdlrGetWord, 0 }
};
static struct cnfparamblk actpblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(actpdescr)/sizeof(struct cnfparamdescr),
	  actpdescr
	};

BEGINinitConfVars		/* (re)set config variables to default values */
CODESTARTinitConfVars 
ENDinitConfVars

static inline int
getPartition(instanceData *const __restrict__ pData)
{
	return (pData->fixedPartition == NO_FIXED_PARTITION) ?
	          ATOMIC_INC_AND_FETCH_int(&pData->currPartition,
		      &pData->mutCurrPartition) % pData->nPartitions
		:  pData->fixedPartition;
}

BEGINdoHUP
CODESTARTdoHUP
	pthread_mutex_lock(&pData->mutErrFile);
	if(pData->fdErrFile != -1) {
		close(pData->fdErrFile);
		pData->fdErrFile = -1;
	}
	pthread_mutex_unlock(&pData->mutErrFile);
ENDdoHUP


/* write data error request/replies to separate error file
 * Note: we open the file but never close it before exit. If it
 * needs to be closed, HUP must be sent.
 */
static rsRetVal
writeDataError(wrkrInstanceData_t *const pWrkrData,
	const char *const __restrict__ data,
	const size_t lenData,
	const int kafkaErr)
{
	int bLocked = 0;
	struct json_object *json = NULL;
	DEFiRet;

	instanceData *const pData = pWrkrData->pData;
	if(pData->errorFile == NULL) {
		FINALIZE;
	}

	json = json_object_new_object();
	if(json == NULL) {
		ABORT_FINALIZE(RS_RET_ERR);
	}
	struct json_object *jval;
	jval = json_object_new_int(kafkaErr);
	json_object_object_add(json, "errcode", jval);
	jval = json_object_new_string(rd_kafka_err2str(kafkaErr));
	json_object_object_add(json, "errmsg", jval);
	jval = json_object_new_string_len(data, lenData);
	json_object_object_add(json, "data", jval);

	struct iovec iov[2];
	iov[0].iov_base = (void*) json_object_get_string(json);
	iov[0].iov_len = strlen(iov[0].iov_base);
	iov[1].iov_base = "\n";
	iov[1].iov_len = 1;

	/* we must protect the file write do operations due to other wrks & HUP */
	pthread_mutex_lock(&pData->mutErrFile);
	bLocked = 1;
	if(pData->fdErrFile == -1) {
		pData->fdErrFile = open((char*)pData->errorFile,
					O_WRONLY|O_CREAT|O_APPEND|O_LARGEFILE|O_CLOEXEC,
					S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
		if(pData->fdErrFile == -1) {
			char errStr[1024];
			rs_strerror_r(errno, errStr, sizeof(errStr));
			DBGPRINTF("omkafka: error opening error file: %s\n", errStr);
			ABORT_FINALIZE(RS_RET_ERR);
		}
	}

	/* Note: we do not do real error-handling on the err file, as this
	 * complicates things way to much.
	 */
	const ssize_t nwritten = writev(pData->fdErrFile, iov, sizeof(iov)/sizeof(struct iovec));
	if(nwritten != (ssize_t) iov[0].iov_len + 1) {
		DBGPRINTF("omkafka: error %d writing error file, write returns %lld\n",
			  errno, (long long) nwritten);
	}

finalize_it:
	if(bLocked)
		pthread_mutex_unlock(&pData->mutErrFile);
	if(json != NULL)
		json_object_put(json);
	RETiRet;
}

static void
deliveryCallback(rd_kafka_t __attribute__((unused)) *rk,
	   void *payload, size_t len,
	   int error_code,
	   void *opaque, void __attribute__((unused)) *msg_opaque)
{
	wrkrInstanceData_t *const pWrkrData = (wrkrInstanceData_t *) opaque;
	if(error_code != 0)
		writeDataError(pWrkrData, (char*) payload, len, error_code);
}

static void
kafkaLogger(const rd_kafka_t __attribute__((unused)) *rk, int level,
	    const char *fac, const char *buf)
{
	DBGPRINTF("omkafka: kafka log message [%d,%s]: %s\n",
		  level, fac, buf);
}

static inline void
do_rd_kafka_destroy(wrkrInstanceData_t *const __restrict pWrkrData)
{
	rd_kafka_destroy(pWrkrData->rk);
	pWrkrData->rk = NULL;
}


static rsRetVal
closeKafka(wrkrInstanceData_t *const __restrict__ pWrkrData)
{
	DEFiRet;
	if(!pWrkrData->bIsOpen)
		FINALIZE;
	do_rd_kafka_destroy(pWrkrData);
	pWrkrData->bIsOpen = 0;
finalize_it:
	RETiRet;
}

static void
errorCallback(rd_kafka_t __attribute__((unused)) *rk,
	int __attribute__((unused)) err,
	const char *reason,
	void __attribute__((unused)) *opaque)
{
	errmsg.LogError(0, RS_RET_KAFKA_ERROR,
		"omkafka: kafka message %s", reason);
}


#if 0 /* the stock librdkafka version in Ubuntu 14.04 LTS does NOT support metadata :-( */
/* Note: this is a skeleton, with some code missing--> add it when it is actually implemented. */
static int
getConfiguredPartitions()
{
	struct rd_kafka_metadata *pMetadata;
	if(rd_kafka_metadata(pWrkrData->rk, 0, rkt, &pMetadata, 8)
		== RD_KAFKA_RESP_ERR_NO_ERROR) {
		dbgprintf("omkafka: topic '%s' has %d partitions\n",
			  pData->topic, pMetadata->topics[0]->partition_cnt);
		rd_kafka_metadata_destroy(pMetadata);
	} else {
		dbgprintf("omkafka: error reading metadata\n");
		// TODO: handle this gracefull **when** we actually need
		// the metadata -- or remove completely. 2014-12-12 rgerhards
	}
}
#endif

static rsRetVal
openKafka(wrkrInstanceData_t *const __restrict__ pWrkrData)
{
	char errstr[MAX_ERRMSG];
	const instanceData *const pData = pWrkrData->pData;
	int nBrokers = 0;
	DEFiRet;

	if(pWrkrData->bIsOpen)
		FINALIZE;

	/* main conf */
	rd_kafka_conf_t *const conf = rd_kafka_conf_new();
	if(conf == NULL) {
		errmsg.LogError(0, RS_RET_KAFKA_ERROR,
			"omkafka: error creating kafka conf obj: %s\n", 
			rd_kafka_err2str(rd_kafka_errno2err(errno)));
		ABORT_FINALIZE(RS_RET_KAFKA_ERROR);
	}
	for(int i = 0 ; i < pData->nConfParams ; ++i) {
		if(rd_kafka_conf_set(conf,
				     pData->confParams[i].name, 
				     pData->confParams[i].val, 
				     errstr, sizeof(errstr))
	 	   != RD_KAFKA_CONF_OK) {
			if(pWrkrData->bReportErrs) {
				errmsg.LogError(0, RS_RET_PARAM_ERROR, "error in kafka "
						"parameter '%s=%s': %s", 
						pData->confParams[i].name, 
						pData->confParams[i].val, errstr);
			}
			ABORT_FINALIZE(RS_RET_PARAM_ERROR);
		}
	} 
	rd_kafka_conf_set_opaque(conf, (void *) pWrkrData);
	rd_kafka_conf_set_dr_cb(conf, deliveryCallback);
	rd_kafka_conf_set_error_cb(conf, errorCallback);

	/* topic conf */
	rd_kafka_topic_conf_t *const topicconf = rd_kafka_topic_conf_new();
	if(topicconf == NULL) {
		errmsg.LogError(0, RS_RET_KAFKA_ERROR,
			"omkafka: error creating kafka topic conf obj: %s\n", 
			rd_kafka_err2str(rd_kafka_errno2err(errno)));
		ABORT_FINALIZE(RS_RET_KAFKA_ERROR);
	}
	for(int i = 0 ; i < pData->nTopicConfParams ; ++i) {
		if(rd_kafka_topic_conf_set(topicconf,
				     pData->topicConfParams[i].name, 
				     pData->topicConfParams[i].val, 
				     errstr, sizeof(errstr))
	 	   != RD_KAFKA_CONF_OK) {
			if(pWrkrData->bReportErrs) {
				errmsg.LogError(0, RS_RET_PARAM_ERROR, "error in kafka "
						"topic conf parameter '%s=%s': %s", 
						pData->topicConfParams[i].name, 
						pData->topicConfParams[i].val, errstr);
			}
			ABORT_FINALIZE(RS_RET_PARAM_ERROR);
		}
	} 

	char kafkaErrMsg[1024];
	pWrkrData->rk = rd_kafka_new(RD_KAFKA_PRODUCER, conf,
				     kafkaErrMsg, sizeof(kafkaErrMsg));
	if(pWrkrData->rk == NULL) {
		errmsg.LogError(0, RS_RET_KAFKA_ERROR,
			"omkafka: error creating kafka handle: %s\n", kafkaErrMsg);
		ABORT_FINALIZE(RS_RET_KAFKA_ERROR);
	}
	rd_kafka_set_logger(pWrkrData->rk, kafkaLogger);
	if((nBrokers = rd_kafka_brokers_add(pWrkrData->rk, (char*)pData->brokers)) == 0) {
		errmsg.LogError(0, RS_RET_KAFKA_NO_VALID_BROKERS,
			"omkafka: no valid brokers specified: %s\n", pData->brokers);
		ABORT_FINALIZE(RS_RET_KAFKA_NO_VALID_BROKERS);
	}

	pWrkrData->rkt = rd_kafka_topic_new(pWrkrData->rk, pData->topic, topicconf);
	if(pWrkrData->rkt == NULL) {
		errmsg.LogError(0, RS_RET_KAFKA_ERROR,
			"omkafka: error creating kafka topic: %s\n", 
			rd_kafka_err2str(rd_kafka_errno2err(errno)));
		ABORT_FINALIZE(RS_RET_KAFKA_ERROR);
	}

	pWrkrData->bIsOpen = 1;
finalize_it:
	if(iRet == RS_RET_OK) {
		pWrkrData->bReportErrs = 1;
	} else {
		pWrkrData->bReportErrs = 0;
		if(pWrkrData->rk != NULL) {
			do_rd_kafka_destroy(pWrkrData);
		}
	}
	RETiRet;
}

BEGINcreateInstance
CODESTARTcreateInstance
	pData->currPartition = 0;
	pData->fdErrFile = -1;
	pthread_mutex_init(&pData->mutErrFile, NULL);
ENDcreateInstance


BEGINcreateWrkrInstance
CODESTARTcreateWrkrInstance
	pWrkrData->bIsOpen = 0;
	pWrkrData->bReportErrs = 1;
ENDcreateWrkrInstance


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
ENDisCompatibleWithFeature


BEGINfreeInstance
CODESTARTfreeInstance
	free(pData->errorFile);
	free(pData->topic);
	free(pData->brokers);
	free(pData->tplName);
	for(int i = 0 ; i < pData->nConfParams ; ++i) {
		free((void*) pData->confParams[i].name);
		free((void*) pData->confParams[i].val);
	}
	for(int i = 0 ; i < pData->nTopicConfParams ; ++i) {
		free((void*) pData->topicConfParams[i].name);
		free((void*) pData->topicConfParams[i].val);
	}
	if(pData->fdErrFile != -1)
		close(pData->fdErrFile);
	pthread_mutex_destroy(&pData->mutErrFile);
ENDfreeInstance

BEGINfreeWrkrInstance
CODESTARTfreeWrkrInstance
	closeKafka(pWrkrData);
ENDfreeWrkrInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
ENDdbgPrintInstInfo


BEGINtryResume
CODESTARTtryResume
	/* Note: we do have an issue here: some errors unfortunately
	 * show up only when we actually produce data. As such, we cannot
	 * detect those kinds of problems in tryResume, what can yield
	 * in a false "RS_RET_OK" response in those cases. However, this
	 * problem is shared with other modules, and the rsyslog core engine
	 * has some mitigation against these kinds of problems. On the plus
	 * side, we can at least detect if something goes wrong during
	 * openKafka(), and we save ourself any hassle in those cases.
	 * The current way is not ideal, but currently the best we can
	 * think of. If someone knows how to do a produce call without
	 * actually producing something (or otherwise check the health
	 * of the Kafka subsystem), please let us know on the rsyslog
	 * mailing list. -- rgerhards, 2014-12-14
	 */
	iRet = openKafka(pWrkrData);
	DBGPRINTF("omkafka: tryResume returned %d\n", iRet);
ENDtryResume


static rsRetVal
writeKafka(wrkrInstanceData_t *pWrkrData, uchar *msg)
{
	DEFiRet;
	const int partition = getPartition(pWrkrData->pData);

	DBGPRINTF("omkafka: trying to send: '%s'\n", msg);
	CHKiRet(openKafka(pWrkrData));
	if(rd_kafka_produce(pWrkrData->rkt, partition, RD_KAFKA_MSG_F_COPY,
	                    msg, strlen((char*)msg), NULL, 0, NULL) == -1) {
		errmsg.LogError(0, RS_RET_KAFKA_PRODUCE_ERR,
			"omkafka: Failed to produce to topic '%s' "
			"partition %d: %s\n",
			rd_kafka_topic_name(pWrkrData->rkt), partition, 
			rd_kafka_err2str(rd_kafka_errno2err(errno)));
		ABORT_FINALIZE(RS_RET_KAFKA_PRODUCE_ERR);
	}
	const int callbacksCalled = rd_kafka_poll(pWrkrData->rk, 0); /* call callbacks */

	DBGPRINTF("omkafka: kafka outqueue length: %d, callbacks called %d\n",
		  rd_kafka_outq_len(pWrkrData->rk), callbacksCalled);

finalize_it:
	DBGPRINTF("omkafka: writeKafka returned %d\n", iRet);
	if(iRet != RS_RET_OK) {
		closeKafka(pWrkrData);
		iRet = RS_RET_SUSPENDED;
	}
	RETiRet;
}


BEGINdoAction
CODESTARTdoAction
	iRet = writeKafka(pWrkrData, ppString[0]);
ENDdoAction


static inline void
setInstParamDefaults(instanceData *pData)
{
	pData->topic = NULL;
	pData->brokers = NULL;
	pData->fixedPartition = NO_FIXED_PARTITION;
	pData->nPartitions = 1;
	pData->nConfParams = 0;
	pData->confParams = NULL;
	pData->nTopicConfParams = 0;
	pData->topicConfParams = NULL;
	pData->errorFile = NULL;
}

static rsRetVal
processKafkaParam(char *const param,
	const char **const name,
	const char **const paramval)
{
	DEFiRet;
	char *val = strstr(param, "=");
	if(val == NULL) {
		errmsg.LogError(0, RS_RET_PARAM_ERROR, "missing equal sign in "
				"parameter '%s'", param);
		ABORT_FINALIZE(RS_RET_PARAM_ERROR);
	}
	*val = '\0'; /* terminates name */
	++val; /* now points to begin of value */
	CHKmalloc(*name = strdup(param));
	CHKmalloc(*paramval = strdup(val));
finalize_it:
	RETiRet;
}

BEGINnewActInst
	struct cnfparamvals *pvals;
	int i;
CODESTARTnewActInst
	if((pvals = nvlstGetParams(lst, &actpblk, NULL)) == NULL) {
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	CHKiRet(createInstance(&pData));
	setInstParamDefaults(pData);

	CODE_STD_STRING_REQUESTnewActInst(1)
	for(i = 0 ; i < actpblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(actpblk.descr[i].name, "topic")) {
			pData->topic = es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "partitions.number")) {
			pData->nPartitions = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "partitions.usefixed")) {
			pData->fixedPartition = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "broker")) {
			es_str_t *es = es_newStr(128);
			int bNeedComma = 0;
			CHKmalloc(pData->brokers = malloc(sizeof(char*) *
			                                  pvals[i].val.d.ar->nmemb ));
			for(int j = 0 ; j <  pvals[i].val.d.ar->nmemb ; ++j) {
				if(bNeedComma)
					es_addChar(&es, ',');
				es_addStr(&es, pvals[i].val.d.ar->arr[j]);
				bNeedComma = 1;
			}
			pData->brokers = es_str2cstr(es, NULL);
			es_deleteStr(es);
		} else if(!strcmp(actpblk.descr[i].name, "confparam")) {
			pData->nConfParams = pvals[i].val.d.ar->nmemb;
			CHKmalloc(pData->confParams = malloc(sizeof(struct kafka_params) *
			                                      pvals[i].val.d.ar->nmemb ));
			for(int j = 0 ; j <  pvals[i].val.d.ar->nmemb ; ++j) {
				CHKiRet(processKafkaParam(es_str2cstr(pvals[i].val.d.ar->arr[j], NULL),
							&pData->confParams[j].name,
							&pData->confParams[j].val));
			}
		} else if(!strcmp(actpblk.descr[i].name, "topicconfparam")) {
			pData->nTopicConfParams = pvals[i].val.d.ar->nmemb;
			CHKmalloc(pData->topicConfParams = malloc(sizeof(struct kafka_params) *
			                                      pvals[i].val.d.ar->nmemb ));
			for(int j = 0 ; j <  pvals[i].val.d.ar->nmemb ; ++j) {
				CHKiRet(processKafkaParam(es_str2cstr(pvals[i].val.d.ar->arr[j], NULL),
							&pData->topicConfParams[j].name,
							&pData->topicConfParams[j].val));
			}
		} else if(!strcmp(actpblk.descr[i].name, "errorfile")) {
			pData->errorFile = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "template")) {
			pData->tplName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else {
			dbgprintf("omkafka: program error, non-handled param '%s'\n", actpblk.descr[i].name);
		}
	}

	if(pData->brokers == NULL)
		CHKmalloc(pData->brokers = strdup("localhost:9092"));

	CHKiRet(OMSRsetEntry(*ppOMSR, 0, (uchar*)strdup((pData->tplName == NULL) ? 
						"RSYSLOG_FileFormat" : (char*)pData->tplName),
						OMSR_NO_RQD_TPL_OPTS));
CODE_STD_FINALIZERnewActInst
	cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst

BEGINparseSelectorAct
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	/* first check if this config line is actually for us */
	if(strncmp((char*) p, ":omkafka:", sizeof(":omkafka:") - 1)) {
		ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
	}
	errmsg.LogError(0, RS_RET_LEGA_ACT_NOT_SUPPORTED,
			"omkafka supports only RainerScript config format, use: "
			"action(type=\"omkafka\" ...parameters...)");
	ABORT_FINALIZE(RS_RET_LEGA_ACT_NOT_SUPPORTED);
CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINmodExit
CODESTARTmodExit
	CHKiRet(objRelease(errmsg, CORE_COMPONENT));
finalize_it:
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
CODEqueryEtryPt_STD_OMOD8_QUERIES
CODEqueryEtryPt_STD_CONF2_CNFNAME_QUERIES 
CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES
CODEqueryEtryPt_doHUP
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
INITLegCnfVars
	*ipIFVersProvided = CURR_MOD_IF_VERSION;
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	DBGPRINTF("omkafka %s using librdkafka version %s\n",
	          VERSION, rd_kafka_version_str());
CODEmodInit_QueryRegCFSLineHdlr
ENDmodInit
