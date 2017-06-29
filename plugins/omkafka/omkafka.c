/* omkafka.c
 * This output plugin make rsyslog talk to Apache Kafka.
 *
 * Copyright 2014-2017 by Adiscon GmbH.
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
#include <sys/queue.h>
#include <sys/types.h>
#ifdef HAVE_SYS_STAT_H
#	include <sys/stat.h>
#endif
#include <unistd.h>
#include <librdkafka/rdkafka.h>
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include "atomic.h"
#include "statsobj.h"
#include "unicode-helper.h"
#include "datetime.h"

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("omkafka")

/* internal structures
 */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(datetime)
DEFobjCurrIf(errmsg)
DEFobjCurrIf(strm)
DEFobjCurrIf(statsobj)

statsobj_t *kafkaStats;
int ctrQueueSize;
STATSCOUNTER_DEF(ctrTopicSubmit, mutCtrTopicSubmit);
STATSCOUNTER_DEF(ctrKafkaFail, mutCtrKafkaFail);
STATSCOUNTER_DEF(ctrCacheMiss, mutCtrCacheMiss);
STATSCOUNTER_DEF(ctrCacheEvict, mutCtrCacheEvict);
STATSCOUNTER_DEF(ctrCacheSkip, mutCtrCacheSkip);

#define MAX_ERRMSG 1024 /* max size of error messages that we support */

#define NO_FIXED_PARTITION -1	/* signifies that no fixed partition config exists */

struct kafka_params {
	const char *name;
	const char *val;
};

#if HAVE_ATOMIC_BUILTINS64
static uint64 clockTopicAccess = 0;
#else
static unsigned clockTopicAccess = 0;
#endif
/* and the "tick" function */
#ifndef HAVE_ATOMIC_BUILTINS
static pthread_mutex_t mutClock;
#endif
static inline uint64
getClockTopicAccess(void)
{
#if HAVE_ATOMIC_BUILTINS64
	return ATOMIC_INC_AND_FETCH_uint64(&clockTopicAccess, &mutClock);
#else
	return ATOMIC_INC_AND_FETCH_unsigned(&clockTopicAccess, &mutClock);
#endif
}

/* Needed for Kafka timestamp librdkafka > 0.9.4 */
#define KAFKA_TimeStamp "\"%timestamp:::date-unixtimestamp%\""

static int closeTimeout = 1000;
static pthread_mutex_t closeTimeoutMut = PTHREAD_MUTEX_INITIALIZER;

/* dynamic topic cache */
struct s_dynaTopicCacheEntry {
	uchar *pName;
	rd_kafka_topic_t *pTopic;
	uint64 clkTickAccessed;
	pthread_rwlock_t lock;
};
typedef struct s_dynaTopicCacheEntry dynaTopicCacheEntry;

/* Struct for Failed Messages Listitems */
struct s_failedmsg_entry {
	uchar* payload;
	uchar* topicname;
	LIST_ENTRY(s_failedmsg_entry) entries;	/*	List. */
} ;
typedef struct s_failedmsg_entry failedmsg_entry;

typedef struct _instanceData {
	uchar *topic;
	sbool dynaTopic;
	dynaTopicCacheEntry **dynCache;
	pthread_mutex_t mutDynCache;
	rd_kafka_topic_t *pTopic;
	int iCurrElt;
	int iCurrCacheSize;
	int bReportErrs;
	int iDynaTopicCacheSize;
	uchar *tplName;		/* assigned output template */
	char *brokers;
	sbool autoPartition;
	int fixedPartition;
	int nPartitions;
	uint32_t currPartition;
	int nConfParams;
	struct kafka_params *confParams;
	int nTopicConfParams;
	struct kafka_params *topicConfParams;
	uchar *errorFile;
	uchar *key;
	int bReopenOnHup;
	int bResubmitOnFailure;	/* Resubmit failed messages into kafka queue*/
	int bKeepFailedMessages;/* Keep Failed messages in memory,
							only works if bResubmitOnFailure is enabled */
	uchar *failedMsgFile;	/* file in which failed messages are being stored on
							shutdown and loaded on startup */

	int fdErrFile;		/* error file fd or -1 if not open */
	pthread_mutex_t mutErrFile;
	int bIsOpen;
	int bIsSuspended;	/* when broker fail, we need to suspend the action */
	pthread_rwlock_t rkLock;
	rd_kafka_t *rk;
	int closeTimeout;
	/*List objects */
	LIST_HEAD(failedmsg_listhead, s_failedmsg_entry) failedmsg_head;
//	struct failedmsg_listhead *failedmsg_headp;	/*	List head pointer */

} instanceData;

typedef struct wrkrInstanceData {
	instanceData *pData;
} wrkrInstanceData_t;


/* tables for interfacing with the v6 config system */
/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {
	{ "topic", eCmdHdlrString, CNFPARAM_REQUIRED },
	{ "dynatopic", eCmdHdlrBinary, 0 },
	{ "dynatopic.cachesize", eCmdHdlrInt, 0 },
	{ "partitions.auto", eCmdHdlrBinary, 0 },	/* use librdkafka's automatic partitioning function */
	{ "partitions.number", eCmdHdlrPositiveInt, 0 },
	{ "partitions.usefixed", eCmdHdlrNonNegInt, 0 }, /* expert parameter, "nails" partition */
	{ "broker", eCmdHdlrArray, 0 },
	{ "confparam", eCmdHdlrArray, 0 },
	{ "topicconfparam", eCmdHdlrArray, 0 },
	{ "errorfile", eCmdHdlrGetWord, 0 },
	{ "key", eCmdHdlrGetWord, 0 },
	{ "template", eCmdHdlrGetWord, 0 },
	{ "closeTimeout", eCmdHdlrPositiveInt, 0 },
	{ "reopenOnHup", eCmdHdlrBinary, 0 },
	{ "resubmitOnFailure", eCmdHdlrBinary, 0 },	/* Resubmit message into kafaj queue on failure */
	{ "keepFailedMessages", eCmdHdlrBinary, 0 },
	{ "failedMsgFile", eCmdHdlrGetWord, 0 }
};
static struct cnfparamblk actpblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(actpdescr)/sizeof(struct cnfparamdescr),
	  actpdescr
	};

BEGINinitConfVars		/* (re)set config variables to default values */
CODESTARTinitConfVars
ENDinitConfVars

static uint32_t
getPartition(instanceData *const __restrict__ pData)
{
	if (pData->autoPartition) {
		return RD_KAFKA_PARTITION_UA;
	} else {
		return (pData->fixedPartition == NO_FIXED_PARTITION) ?
		          ATOMIC_INC_AND_FETCH_unsigned(&pData->currPartition,
			      &pData->mutCurrPartition) % pData->nPartitions
			:  (unsigned) pData->fixedPartition;
	}
}

/* must always be called with appropriate locks taken */
static void
d_free_topic(rd_kafka_topic_t **topic)
{
	if (*topic != NULL) {
		DBGPRINTF("omkafka: closing topic %s\n", rd_kafka_topic_name(*topic));
		rd_kafka_topic_destroy(*topic);
		*topic = NULL;
	}
}

/* destroy topic item */
/* must be called with write(rkLock) */
static void
closeTopic(instanceData *__restrict__ const pData)
{
	d_free_topic(&pData->pTopic);
}

/* these dynaTopic* functions are only slightly modified versions of those found in omfile.c.
 * check the sources in omfile.c for more descriptive comments about each of these functions.
 * i will only put the bare descriptions in this one. 2015-01-09 - Tait Clarridge
 */

/* delete a cache entry from the dynamic topic cache */
/* must be called with lock(mutDynCache) */
static rsRetVal
dynaTopicDelCacheEntry(instanceData *__restrict__ const pData, const int iEntry, const int bFreeEntry)
{
	dynaTopicCacheEntry **pCache = pData->dynCache;
	DEFiRet;
	ASSERT(pCache != NULL);

	if(pCache[iEntry] == NULL)
		FINALIZE;
	pthread_rwlock_wrlock(&pCache[iEntry]->lock);

	DBGPRINTF("Removing entry %d for topic '%s' from dynaCache.\n", iEntry,
		pCache[iEntry]->pName == NULL ? UCHAR_CONSTANT("[OPEN FAILED]") : pCache[iEntry]->pName);

	if(pCache[iEntry]->pName != NULL) {
		d_free(pCache[iEntry]->pName);
		pCache[iEntry]->pName = NULL;
	}

	pthread_rwlock_unlock(&pCache[iEntry]->lock);

	if(bFreeEntry) {
		pthread_rwlock_destroy(&pCache[iEntry]->lock);
		d_free(pCache[iEntry]);
		pCache[iEntry] = NULL;
	}

finalize_it:
	RETiRet;
}

/* clear the entire dynamic topic cache */
static void
dynaTopicFreeCacheEntries(instanceData *__restrict__ const pData)
{
	register int i;
	ASSERT(pData != NULL);

	BEGINfunc;
	pthread_mutex_lock(&pData->mutDynCache);
	for(i = 0 ; i < pData->iCurrCacheSize ; ++i) {
		dynaTopicDelCacheEntry(pData, i, 1);
	}
	pData->iCurrElt = -1; /* invalidate current element */
	pthread_mutex_unlock(&pData->mutDynCache);
	ENDfunc;
}

/* create the topic object */
/* must be called with _atleast_ read(rkLock) */
static rsRetVal
createTopic(instanceData *__restrict__ const pData, const uchar *__restrict__ const newTopicName,
rd_kafka_topic_t** topic) {
/* Get a new topic conf */
	rd_kafka_topic_conf_t *const topicconf = rd_kafka_topic_conf_new();
	char errstr[MAX_ERRMSG];
	rd_kafka_topic_t *rkt = NULL;
	DEFiRet;

	*topic = NULL;

	if(topicconf == NULL) {
		errmsg.LogError(0, RS_RET_KAFKA_ERROR,
						"omkafka: error creating kafka topic conf obj: %s\n",
						rd_kafka_err2str(rd_kafka_last_error()));
		ABORT_FINALIZE(RS_RET_KAFKA_ERROR);
	}
	for(int i = 0 ; i < pData->nTopicConfParams ; ++i) {
		DBGPRINTF("omkafka: setting custom topic configuration parameter: %s:%s\n",
			pData->topicConfParams[i].name,
			pData->topicConfParams[i].val);
		if(rd_kafka_topic_conf_set(topicconf,
								   pData->topicConfParams[i].name,
								   pData->topicConfParams[i].val,
								   errstr, sizeof(errstr))
		   != RD_KAFKA_CONF_OK) {
			if(pData->bReportErrs) {
				errmsg.LogError(0, RS_RET_PARAM_ERROR, "error in kafka "
								"topic conf parameter '%s=%s': %s",
								pData->topicConfParams[i].name,
								pData->topicConfParams[i].val, errstr);
			}
			ABORT_FINALIZE(RS_RET_PARAM_ERROR);
		}
	}
	rkt = rd_kafka_topic_new(pData->rk, (char *)newTopicName, topicconf);
	if(rkt == NULL) {
		errmsg.LogError(0, RS_RET_KAFKA_ERROR,
						"omkafka: error creating kafka topic: %s\n",
						rd_kafka_err2str(rd_kafka_last_error()));
		ABORT_FINALIZE(RS_RET_KAFKA_ERROR);
	}
	*topic = rkt;
finalize_it:
	RETiRet;
}

/* create the topic object */
/* must be called with write(rkLock) */
static rsRetVal
prepareTopic(instanceData *__restrict__ const pData, const uchar *__restrict__ const newTopicName)
{
	DEFiRet;
	CHKiRet(createTopic(pData, newTopicName, &pData->pTopic));
finalize_it:
	if(iRet != RS_RET_OK) {
		if(pData->pTopic != NULL) {
			closeTopic(pData);
		}
	}
	RETiRet;
}

/* check dynamic topic cache for existence of the already created topic.
 * if it does not exist, create a new one, or if we are currently using it
 * as of the last message, keep using it.
 *
 * important: topic is returned read-locked, user must unlock it after using it.
 *
 * must be called with read(rkLock)
 */
static rsRetVal
prepareDynTopic(instanceData *__restrict__ const pData, const uchar *__restrict__ const newTopicName,
				rd_kafka_topic_t** topic, pthread_rwlock_t** lock)
{
	uint64 ctOldest;
	int iOldest;
	int i;
	int iFirstFree;
	rsRetVal localRet;
	dynaTopicCacheEntry **pCache;
	dynaTopicCacheEntry *entry = NULL;
	rd_kafka_topic_t *tmpTopic = NULL;
	DEFiRet;
	ASSERT(pData != NULL);
	ASSERT(newTopicName != NULL);

	pthread_mutex_lock(&pData->mutDynCache);

	pCache = pData->dynCache;
	/* first check, if we still have the current topic */
	if ((pData->iCurrElt != -1)
		&& !ustrcmp(newTopicName, pCache[pData->iCurrElt]->pName)) {
			/* great, we are all set */
			pCache[pData->iCurrElt]->clkTickAccessed = getClockTopicAccess();
			entry = pCache[pData->iCurrElt];
			STATSCOUNTER_INC(ctrCacheSkip, mutCtrCacheSkip);
			FINALIZE;
	}

	/* ok, no luck. Now let's search the table if we find a matching spot.
	 * While doing so, we also prepare for creation of a new one.
	 */
	pData->iCurrElt = -1;
	iFirstFree = -1;
	iOldest = 0;
	ctOldest = getClockTopicAccess();
	for(i = 0 ; i < pData->iCurrCacheSize ; ++i) {
		if(pCache[i] == NULL || pCache[i]->pName == NULL) {
			if(iFirstFree == -1)
				iFirstFree = i;
		} else { /*got an element, let's see if it matches */
			if(!ustrcmp(newTopicName, pCache[i]->pName)) {
				/* we found our element! */
				entry = pCache[i];
				pData->iCurrElt = i;
				pCache[i]->clkTickAccessed = getClockTopicAccess(); /* update "timestamp" for LRU */
				FINALIZE;
			}
			/* did not find it - so lets keep track of the counters for LRU */
			if(pCache[i]->clkTickAccessed < ctOldest) {
				ctOldest = pCache[i]->clkTickAccessed;
				iOldest = i;
			}
		}
	}
	STATSCOUNTER_INC(ctrCacheMiss, mutCtrCacheMiss);

	/* invalidate iCurrElt as we may error-exit out of this function when the currrent
	 * iCurrElt has been freed or otherwise become unusable. This is a precaution, and
	 * performance-wise it may be better to do that in each of the exits. However, that
	 * is error-prone, so I prefer to do it here. -- rgerhards, 2010-03-02
	 */
	pData->iCurrElt = -1;

	if(iFirstFree == -1 && (pData->iCurrCacheSize < pData->iDynaTopicCacheSize)) {
		/* there is space left, so set it to that index */
		iFirstFree = pData->iCurrCacheSize++;
	}

	if(iFirstFree == -1) {
		dynaTopicDelCacheEntry(pData, iOldest, 0);
		STATSCOUNTER_INC(ctrCacheEvict, mutCtrCacheEvict);
		iFirstFree = iOldest; /* this one *is* now free ;) */
	} else {
		pCache[iFirstFree] = NULL;
	}
	/* we need to allocate memory for the cache structure */
	if(pCache[iFirstFree] == NULL) {
		CHKmalloc(pCache[iFirstFree] = (dynaTopicCacheEntry*) calloc(1, sizeof(dynaTopicCacheEntry)));
		CHKiRet(pthread_rwlock_init(&pCache[iFirstFree]->lock, NULL));
	}

	/* Ok, we finally can open the topic */
	localRet = createTopic(pData, newTopicName, &tmpTopic);

	if(localRet != RS_RET_OK) {
		errmsg.LogError(0, localRet, "Could not open dynamic topic '%s' [state %d] - discarding message",
		newTopicName, localRet);
		ABORT_FINALIZE(localRet);
	}

	if((pCache[iFirstFree]->pName = ustrdup(newTopicName)) == NULL) {
		d_free_topic(&tmpTopic);
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}
	pCache[iFirstFree]->pTopic = tmpTopic;
	pCache[iFirstFree]->clkTickAccessed = getClockTopicAccess();
	entry = pCache[iFirstFree];
	pData->iCurrElt = iFirstFree;
	DBGPRINTF("Added new entry %d for topic cache, topic '%s'.\n", iFirstFree, newTopicName);

finalize_it:
	if (iRet == RS_RET_OK && entry != NULL) {
		pthread_rwlock_rdlock(&entry->lock);
	}
	pthread_mutex_unlock(&pData->mutDynCache);
	if (iRet == RS_RET_OK && entry != NULL) {
		*topic = entry->pTopic;
		*lock = &entry->lock;
	} else {
		*topic = NULL;
		*lock = NULL;
	}
	RETiRet;
}

/* write data error request/replies to separate error file
 * Note: we open the file but never close it before exit. If it
 * needs to be closed, HUP must be sent.
 */
static rsRetVal
writeDataError(instanceData *const pData,
	const char *const __restrict__ data,
	const size_t lenData,
	const int kafkaErr)
{
	int bLocked = 0;
	struct json_object *json = NULL;
	DEFiRet;

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
	iov[1].iov_base = (char *) "\n";
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

/* must be called with read(rkLock) */
static rsRetVal
writeKafka(instanceData *pData, uchar *msg, uchar *msgTimestamp, uchar *topic)
{
	DEFiRet;
	const int partition = getPartition(pData);
	rd_kafka_topic_t *rkt = NULL;
	pthread_rwlock_t *dynTopicLock = NULL;
	failedmsg_entry* fmsgEntry;
#if RD_KAFKA_VERSION >= 0x00090400
	rd_kafka_resp_err_t msg_kafka_response;
	int64_t ttMsgTimestamp;
#else
	int msg_enqueue_status = 0;
#endif

	DBGPRINTF("omkafka: trying to send: key:'%s', msg:'%s', timestamp:'%s'\n", pData->key, msg, msgTimestamp);

	if(pData->dynaTopic) {
		DBGPRINTF("omkafka: topic to insert to: %s\n", topic);
		CHKiRet(prepareDynTopic(pData, topic, &rkt, &dynTopicLock));
	} else {
		rkt = pData->pTopic;
	}

#if RD_KAFKA_VERSION >= 0x00090400
	if (msgTimestamp == NULL) {
		/* Resubmitted items don't have a timestamp :/*/
		ttMsgTimestamp = time(NULL);
	} else {
		ttMsgTimestamp = atoi((char*)msgTimestamp); /* Convert timestamp into int */
		ttMsgTimestamp *= 1000; /* Timestamp in Milliseconds for kafka */
	}
	DBGPRINTF("omkafka: rd_kafka_producev timestamp=%s/%" PRId64 "\n", msgTimestamp, ttMsgTimestamp);

	/* Using new kafka producev API, includes Timestamp! */
	if (pData->key == NULL) {
		msg_kafka_response = rd_kafka_producev(pData->rk,
						RD_KAFKA_V_RKT(rkt),
						RD_KAFKA_V_PARTITION(partition),
						RD_KAFKA_V_VALUE(msg, strlen((char*)msg)),
						RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
						RD_KAFKA_V_TIMESTAMP(ttMsgTimestamp),
						RD_KAFKA_V_END);
	} else {
		msg_kafka_response = rd_kafka_producev(pData->rk,
						RD_KAFKA_V_RKT(rkt),
						RD_KAFKA_V_PARTITION(partition),
						RD_KAFKA_V_VALUE(msg, strlen((char*)msg)),
						RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
						RD_KAFKA_V_TIMESTAMP(ttMsgTimestamp),
						RD_KAFKA_V_KEY(pData->key,strlen((char*)pData->key)),
						RD_KAFKA_V_END);
	}

	if (msg_kafka_response != RD_KAFKA_RESP_ERR_NO_ERROR ) {
		/* Put into kafka queue, again if configured! */
		if (pData->bResubmitOnFailure) {
			DBGPRINTF("omkafka: Failed to produce to topic '%s' (rd_kafka_producev)"
			"partition %d: '%d/%s' - adding MSG '%s' to failed for RETRY!\n",
				rd_kafka_topic_name(rkt), partition, msg_kafka_response,
				rd_kafka_err2str(msg_kafka_response), msg);

			/* Create new Listitem */
			CHKmalloc(fmsgEntry = malloc(sizeof(struct s_failedmsg_entry)));
			fmsgEntry->payload = (uchar*)strdup((char*)msg);
			fmsgEntry->topicname = (uchar*)strdup(rd_kafka_topic_name(rkt));

			/* Insert at the head. */
			LIST_INSERT_HEAD(&pData->failedmsg_head, fmsgEntry, entries);
		} else {
			errmsg.LogError(0, RS_RET_KAFKA_PRODUCE_ERR,
			"omkafka: Failed to produce to topic '%s' (rd_kafka_producev)"
			"partition %d: %d/%s\n",
			rd_kafka_topic_name(rkt), partition, msg_kafka_response,
			rd_kafka_err2str(msg_kafka_response));
		}
	}
#else

	DBGPRINTF("omkafka: rd_kafka_produce\n");
	/* Using old kafka produce API */
	msg_enqueue_status = rd_kafka_produce(rkt, partition, RD_KAFKA_MSG_F_COPY,
										  msg, strlen((char*)msg), pData->key,
										  pData->key == NULL ? 0 : strlen((char*)pData->key),
										  NULL);
	if(msg_enqueue_status == -1) {
		/* Put into kafka queue, again if configured! */
		if (pData->bResubmitOnFailure) {
			DBGPRINTF("omkafka: Failed to produce to topic '%s' (rd_kafka_produce)"
			"partition %d: '%d/%s' - adding MSG '%s' to failed for RETRY!\n",
				rd_kafka_topic_name(rkt), partition, rd_kafka_last_error(),
				rd_kafka_err2str(rd_kafka_last_error()), msg);

			/* Create new Listitem */
			CHKmalloc(fmsgEntry = malloc(sizeof(struct s_failedmsg_entry)));
			fmsgEntry->payload = (uchar*)strdup((char*)msg);
			fmsgEntry->topicname = (uchar*)strdup(rd_kafka_topic_name(rkt));

			/* Insert at the head. */
			LIST_INSERT_HEAD(&pData->failedmsg_head, fmsgEntry, entries);
		} else {
			errmsg.LogError(0, RS_RET_KAFKA_PRODUCE_ERR,
			"omkafka: Failed to produce to topic '%s' (rd_kafka_produce) "
			"partition %d: %d/%s\n",
			rd_kafka_topic_name(rkt), partition, rd_kafka_last_error(),
			rd_kafka_err2str(rd_kafka_last_error()));
		}
	}
#endif

	const int callbacksCalled = rd_kafka_poll(pData->rk, 0); /* call callbacks */
	if (pData->dynaTopic) {
		pthread_rwlock_unlock(dynTopicLock);/* dynamic topic can't be used beyond this pt */
	}
	DBGPRINTF("omkafka: writeKafka kafka outqueue length: %d, callbacks called %d\n",
			  rd_kafka_outq_len(pData->rk), callbacksCalled);

#if RD_KAFKA_VERSION >= 0x00090400
	if (msg_kafka_response != RD_KAFKA_RESP_ERR_NO_ERROR) {
#else
	if (msg_enqueue_status == -1) {
#endif
		STATSCOUNTER_INC(ctrKafkaFail, mutCtrKafkaFail);
		ABORT_FINALIZE(RS_RET_KAFKA_PRODUCE_ERR);
		/* ABORT_FINALIZE isn't absolutely necessary as of now,
		   because this is the last line anyway, but its useful to ensure
		   correctness in case we add more stuff below this line at some point*/
	}

finalize_it:
	DBGPRINTF("omkafka: writeKafka returned %d\n", iRet);
	if(iRet != RS_RET_OK) {
		iRet = RS_RET_SUSPENDED;
	}
    STATSCOUNTER_SETMAX_NOMUT(ctrQueueSize, rd_kafka_outq_len(pData->rk));
	STATSCOUNTER_INC(ctrTopicSubmit, mutCtrTopicSubmit);
	RETiRet;
}

static void
deliveryCallback(rd_kafka_t __attribute__((unused)) *rk,
	const rd_kafka_message_t *rkmessage,
	void *opaque)
{
	instanceData *const pData = (instanceData *) opaque;
	failedmsg_entry* fmsgEntry;
	DEFiRet;

	if (rkmessage->err) {
		/* Put into kafka queue, again if configured! */
		if (pData->bResubmitOnFailure) {
			DBGPRINTF("omkafka: kafka delivery FAIL on Topic '%s', msg '%.*s', key '%.*s' -"
				" adding to FAILED MSGs for RETRY!\n",
				rd_kafka_topic_name(rkmessage->rkt),
				(int)(rkmessage->len-1), (char*)rkmessage->payload,
				(int)(rkmessage->key_len), (char*)rkmessage->key);

			/* Create new Listitem */
			CHKmalloc(fmsgEntry = malloc(sizeof(struct s_failedmsg_entry)));
			fmsgEntry->payload = (uchar*)strndup(rkmessage->payload, rkmessage->len);
			fmsgEntry->topicname = (uchar*)strdup(rd_kafka_topic_name(rkmessage->rkt));

			/* Insert at the head. */
			LIST_INSERT_HEAD(&pData->failedmsg_head, fmsgEntry, entries);
		} else {
			DBGPRINTF("omkafka: kafka delivery FAIL on Topic '%s', msg '%.*s', key '%.*s'\n",
				rd_kafka_topic_name(rkmessage->rkt),
				(int)(rkmessage->len-1), (char*)rkmessage->payload,
				(int)(rkmessage->key_len), (char*)rkmessage->key);
			writeDataError(pData, (char*) rkmessage->payload, rkmessage->len, rkmessage->err);
		}
		STATSCOUNTER_INC(ctrKafkaFail, mutCtrKafkaFail);
	} else {
		DBGPRINTF("omkafka: kafka delivery SUCCESS on msg '%.*s'\n", (int)(rkmessage->len-1), (char*)rkmessage->payload);
	}
finalize_it:
	if(iRet != RS_RET_OK) {
		DBGPRINTF("omkafka: deliveryCallback returned failure %d\n", iRet);
	}
}

static void
kafkaLogger(const rd_kafka_t __attribute__((unused)) *rk, int level,
	    const char *fac, const char *buf)
{
	DBGPRINTF("omkafka: kafka log message [%d,%s]: %s\n",
		  level, fac, buf);
}

/* should be called with write(rkLock) */
static void
do_rd_kafka_destroy(instanceData *const __restrict__ pData)
{
	if (pData->rk == NULL) {
		DBGPRINTF("omkafka: onDestroy can't close, handle wasn't open\n");
	} else {
		int queuedCount = rd_kafka_outq_len(pData->rk);
		DBGPRINTF("omkafka: onDestroy closing - items left in outqueue: %d\n", queuedCount);

		struct timespec tOut;
		timeoutComp(&tOut, pData->closeTimeout);

		while (timeoutVal(&tOut) > 0) {
			queuedCount = rd_kafka_outq_len(pData->rk);
			if (queuedCount > 0) {
				/* Flush all remaining kafka messages (rd_kafka_poll is called inside) */
                const int flushStatus = rd_kafka_flush(pData->rk, 5000);
				if (flushStatus != RD_KAFKA_RESP_ERR_NO_ERROR) /* TODO: Handle unsend messages here! */ {
					errmsg.LogError(0, RS_RET_KAFKA_ERROR,
						"omkafka: onDestroy Failed to send remaing '%d' messages to topic '%s' on shutdown with error: '%s'",
						queuedCount,
						rd_kafka_topic_name(pData->pTopic),
						rd_kafka_err2str(flushStatus));
				} else {
					DBGPRINTF("omkafka: onDestroyflushed remaining '%d' messages to kafka topic '%s'\n",
							  queuedCount, rd_kafka_topic_name(pData->pTopic));

				/* Trigger callbacks a last time before shutdown */
				const int callbacksCalled = rd_kafka_poll(pData->rk, 0); /* call callbacks */
				DBGPRINTF("omkafka: onDestroy kafka outqueue length: %d, callbacks called %d\n",
						  rd_kafka_outq_len(pData->rk), callbacksCalled);
				}
			} else {
				break;
			}
		}
		if (queuedCount > 0) {
			DBGPRINTF("omkafka: queue-drain for close timed-out took too long, "
					 "items left in outqueue: %d\n",
					 rd_kafka_outq_len(pData->rk));
		}
		if (pData->dynaTopic) {
			dynaTopicFreeCacheEntries(pData);
		} else {
			closeTopic(pData);
		}
		rd_kafka_destroy(pData->rk);
		pData->rk = NULL;
	}
}

/* should be called with write(rkLock) */
static void
closeKafka(instanceData *const __restrict__ pData)
{
	if(pData->bIsOpen) {
		do_rd_kafka_destroy(pData);
		pData->bIsOpen = 0;
	}
}

static void
errorCallback(rd_kafka_t __attribute__((unused)) *rk,
	int __attribute__((unused)) err,
	const char *reason,
	void __attribute__((unused)) *opaque)
{
	/* Get InstanceData pointer */
	instanceData *const pData = (instanceData *) opaque;

	/* Handle common transport error codes*/
	if (err == RD_KAFKA_RESP_ERR__MSG_TIMED_OUT ||
		err == RD_KAFKA_RESP_ERR__TRANSPORT ||
		err == RD_KAFKA_RESP_ERR__ALL_BROKERS_DOWN ||
		err == RD_KAFKA_RESP_ERR__AUTHENTICATION) {
		/* Broker transport error, we need to disable the action for now!*/
		pData->bIsSuspended = 1;
		DBGPRINTF("omkafka: kafka error handled, action will be suspended: %d,'%s'\n", err, rd_kafka_err2str(err));
	} else {
		errmsg.LogError(0, RS_RET_KAFKA_ERROR,
			"omkafka: kafka error message: %d,'%s','%s'", err, rd_kafka_err2str(err), reason);
	}
}



#if 0 /* the stock librdkafka version in Ubuntu 14.04 LTS does NOT support metadata :-( */
/* Note: this is a skeleton, with some code missing--> add it when it is actually implemented. */
static int
getConfiguredPartitions()
{
	struct rd_kafka_metadata *pMetadata;
	if(rd_kafka_metadata(pData->rk, 0, rkt, &pMetadata, 8)
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

/* should be called with write(rkLock) */
static rsRetVal
openKafka(instanceData *const __restrict__ pData)
{
	char errstr[MAX_ERRMSG];
	int nBrokers = 0;
	DEFiRet;

	if(pData->bIsOpen)
		FINALIZE;

	pData->pTopic = NULL;

	/* main conf */
	rd_kafka_conf_t *const conf = rd_kafka_conf_new();
	if(conf == NULL) {
		errmsg.LogError(0, RS_RET_KAFKA_ERROR,
			"omkafka: error creating kafka conf obj: %s\n",
			rd_kafka_err2str(rd_kafka_last_error()));
		ABORT_FINALIZE(RS_RET_KAFKA_ERROR);
	}

#ifdef DEBUG
	/* enable kafka debug output */
	if(	rd_kafka_conf_set(conf, "debug", RD_KAFKA_DEBUG_CONTEXTS,
		errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
		ABORT_FINALIZE(RS_RET_PARAM_ERROR);
	}
#endif

	for(int i = 0 ; i < pData->nConfParams ; ++i) {
		DBGPRINTF("omkafka: setting custom configuration parameter: %s:%s\n",
			pData->confParams[i].name,
			pData->confParams[i].val);
		if(rd_kafka_conf_set(conf,
				     pData->confParams[i].name,
				     pData->confParams[i].val,
				     errstr, sizeof(errstr))
	 	   != RD_KAFKA_CONF_OK) {
			if(pData->bReportErrs) {
				errmsg.LogError(0, RS_RET_PARAM_ERROR, "error in kafka "
						"parameter '%s=%s': %s",
						pData->confParams[i].name,
						pData->confParams[i].val, errstr);
			}
			ABORT_FINALIZE(RS_RET_PARAM_ERROR);
		}
	}
	rd_kafka_conf_set_opaque(conf, (void *) pData);
	rd_kafka_conf_set_dr_msg_cb(conf, deliveryCallback);
	rd_kafka_conf_set_error_cb(conf, errorCallback);
# if RD_KAFKA_VERSION >= 0x00090001
	rd_kafka_conf_set_log_cb(conf, kafkaLogger);
# endif

	char kafkaErrMsg[1024];
	pData->rk = rd_kafka_new(RD_KAFKA_PRODUCER, conf,
				     kafkaErrMsg, sizeof(kafkaErrMsg));
	if(pData->rk == NULL) {
		errmsg.LogError(0, RS_RET_KAFKA_ERROR,
			"omkafka: error creating kafka handle: %s\n", kafkaErrMsg);
		ABORT_FINALIZE(RS_RET_KAFKA_ERROR);
	}

# if RD_KAFKA_VERSION < 0x00090001
	rd_kafka_set_logger(pData->rk, kafkaLogger);
# endif
	DBGPRINTF("omkafka setting brokers: '%s'n", pData->brokers);
	if((nBrokers = rd_kafka_brokers_add(pData->rk, (char*)pData->brokers)) == 0) {
		errmsg.LogError(0, RS_RET_KAFKA_NO_VALID_BROKERS,
			"omkafka: no valid brokers specified: %s\n", pData->brokers);
		ABORT_FINALIZE(RS_RET_KAFKA_NO_VALID_BROKERS);
	}

	pData->bIsOpen = 1;
finalize_it:
	if(iRet == RS_RET_OK) {
		pData->bReportErrs = 1;
	} else {
		pData->bReportErrs = 0;
		if(pData->rk != NULL) {
			do_rd_kafka_destroy(pData);
		}
	}
	RETiRet;
}

static rsRetVal
setupKafkaHandle(instanceData *const __restrict__ pData, int recreate)
{
	DEFiRet;
	pthread_rwlock_wrlock(&pData->rkLock);
	if (recreate) {
		closeKafka(pData);
	}
	CHKiRet(openKafka(pData));
	if (! pData->dynaTopic) {
		if( pData->pTopic == NULL)
			CHKiRet(prepareTopic(pData, pData->topic));
	}
finalize_it:
	if (iRet != RS_RET_OK) {
		if (pData->rk != NULL) {
			closeKafka(pData);
		}
	}
	pthread_rwlock_unlock(&pData->rkLock);
	RETiRet;
}

static rsRetVal
checkFailedMessages(instanceData *const __restrict__ pData)
{
	failedmsg_entry* fmsgEntry;
	DEFiRet;

	/* Loop through failed messages, reprocess them first! */
	while (!LIST_EMPTY(&pData->failedmsg_head)) {
		fmsgEntry = LIST_FIRST(&pData->failedmsg_head);
		/* Put back into kafka! */
		iRet = writeKafka(pData, (uchar*) fmsgEntry->payload, NULL, fmsgEntry->topicname);
		if(iRet != RS_RET_OK) {
			DBGPRINTF("omkafka: failed to delivery failed msg '%.*s' with status %d. - suspending AGAIN!\n",
				(int)(strlen((char*)fmsgEntry->payload)-1), (char*)fmsgEntry->payload, iRet);
			ABORT_FINALIZE(RS_RET_SUSPENDED);
		} else {
			DBGPRINTF("omkafka: successfully delivered failed msg '%.*s'.\n",
				(int)(strlen((char*)fmsgEntry->payload)-1), (char*)fmsgEntry->payload);
			LIST_REMOVE(fmsgEntry, entries);
			free(fmsgEntry);
		}
	}

finalize_it:
	RETiRet;
}

/* This function persists failed messages into a data file, so they can
 * Be resend on next startup.
 * alorbach, 2017-06-02
 */
static rsRetVal
persistFailedMsgs(instanceData *const __restrict__ pData)
{
	DEFiRet;
	int fdMsgFile = -1;
	ssize_t nwritten;

	/*	Clear Failed Msg List */
	failedmsg_entry* fmsgEntry;

	fmsgEntry	= LIST_FIRST(&pData->failedmsg_head);
	if (fmsgEntry != NULL) {
		fdMsgFile = open((char*)pData->failedMsgFile,
					O_WRONLY|O_CREAT|O_APPEND|O_LARGEFILE|O_CLOEXEC,
					S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
		if(fdMsgFile == -1) {
			char errStr[1024];
			rs_strerror_r(errno, errStr, sizeof(errStr));
			DBGPRINTF("omkafka: persistFailedMsgs error opening failed msg file: %s\n", errStr);
			ABORT_FINALIZE(RS_RET_ERR);
		}

		/* Loop through failed messages, reprocess them first! */
		while (fmsgEntry != NULL)	{
			/* Put into into kafka! */
			write(fdMsgFile, fmsgEntry->topicname, ustrlen(fmsgEntry->topicname) );
			write(fdMsgFile, "\t", 1);
			nwritten = write(fdMsgFile, fmsgEntry->payload, ustrlen(fmsgEntry->payload) );
			if(nwritten == -1) {
				DBGPRINTF("omkafka: persistFailedMsgs error %d writing failed msg file\n", errno);
				ABORT_FINALIZE(RS_RET_ERR);
			} else {
				DBGPRINTF("omkafka: persistFailedMsgs successfully written loaded msg '%.*s' for topic '%s'\n",
					(int)(strlen((char*)fmsgEntry->payload)-1), fmsgEntry->payload, fmsgEntry->topicname);
			}
			/* Get next item */
			fmsgEntry = LIST_NEXT(fmsgEntry, entries);
		}
	} else {
		DBGPRINTF("omkafka: persistFailedMsgs We do not need to persist failed messages.\n");
	}
finalize_it:
	if(fdMsgFile == -1) {
		close(fdMsgFile);
	}
	if(iRet != RS_RET_OK) {
		errmsg.LogError(0, iRet, "omkafka: could not persist failed messages "
			"file %s - failed messages will be lost.",
			(char*)pData->failedMsgFile);
	}

	RETiRet;
}

/* This function loads failed messages from a data file, so they can
 * be resend after action startup.
 * alorbach, 2017-06-06
 */
static rsRetVal
loadFailedMsgs(instanceData *const __restrict__ pData)
{
	DEFiRet;
	struct stat stat_buf;
	failedmsg_entry* fmsgEntry;
	strm_t *pstrmFMSG = NULL;
	cstr_t *pCStr = NULL;
	uchar *puStr;
	char *pStrTabPos;

	/* check if the file exists */
	if(stat((char*) pData->failedMsgFile, &stat_buf) == -1) {
		if(errno == ENOENT) {
			DBGPRINTF("omkafka: loadFailedMsgs failed messages file %s wasn't found, continue startup\n",
				pData->failedMsgFile);
			ABORT_FINALIZE(RS_RET_FILE_NOT_FOUND);
		} else {
			char errStr[1024];
			rs_strerror_r(errno, errStr, sizeof(errStr));
			DBGPRINTF("omkafka: loadFailedMsgs failed messages file %s could not be opened with error %s\n",
				pData->failedMsgFile, errStr);
			ABORT_FINALIZE(RS_RET_IO_ERROR);
		}
	} else {
		DBGPRINTF("omkafka: loadFailedMsgs found failed message file %s.\n",
			pData->failedMsgFile);
	}

	/* File exists, we can load and process it */
	CHKiRet(strm.Construct(&pstrmFMSG));
	CHKiRet(strm.SettOperationsMode(pstrmFMSG, STREAMMODE_READ));
	CHKiRet(strm.SetsType(pstrmFMSG, STREAMTYPE_FILE_SINGLE));
	CHKiRet(strm.SetFName(pstrmFMSG, pData->failedMsgFile, ustrlen(pData->failedMsgFile)));
	CHKiRet(strm.ConstructFinalize(pstrmFMSG));

	while(strm.ReadLine(pstrmFMSG, &pCStr, 0, 0, 0) == RS_RET_OK) {
		if(rsCStrLen(pCStr) == 0) {
			/* we do not process empty lines */
			DBGPRINTF("omkafka: loadFailedMsgs msg was empty!");
		} else {
			puStr = rsCStrGetSzStrNoNULL(pCStr);

			pStrTabPos = index((char*)puStr, '\t');
			if (pStrTabPos != NULL) {
				DBGPRINTF("omkafka: loadFailedMsgs successfully loaded msg '%s' for topic '%.*s':%d\n",
					pStrTabPos+1, (int)(pStrTabPos-(char*)puStr), (char*)puStr, (int)(pStrTabPos-(char*)puStr));

				/* Create new Listitem */
				CHKmalloc(fmsgEntry = malloc(sizeof(struct s_failedmsg_entry)));
				fmsgEntry->payload = (uchar*)strdup(pStrTabPos+1); /* Copy after TAB */
				fmsgEntry->topicname = (uchar*)strndup((char*)puStr, (int)(pStrTabPos-(char*)puStr)); /* copy until TAB */

				/* Insert at the head. */
				LIST_INSERT_HEAD(&pData->failedmsg_head, fmsgEntry, entries);
			} else {
				DBGPRINTF("omkafka: loadFailedMsgs invalid msg found: %s\n",
					(char*)rsCStrGetSzStrNoNULL(pCStr));
			}
		}

		rsCStrDestruct(&pCStr); /* discard string (must be done by us!) */
	}
finalize_it:
	if(pstrmFMSG != NULL) {
		strm.Destruct(&pstrmFMSG);
	}

	if(iRet != RS_RET_OK) {
		/* We ignore FILE NOT FOUND here */
		if (iRet != RS_RET_FILE_NOT_FOUND) {
			errmsg.LogError(0, iRet, "omkafka: could not load failed messages "
			"from file %s error %d - failed messages will not be resend.",
			(char*)pData->failedMsgFile, iRet);
		}
	} else {
		DBGPRINTF("omkafka: loadFailedMsgs unlinking '%s'\n", (char*)pData->failedMsgFile);
		if(unlink((char*)pData->failedMsgFile) != 0) {
			char errStr[1024];
			rs_strerror_r(errno, errStr, sizeof(errStr));
			errmsg.LogError(0, RS_RET_ERR, "omkafka: loadFailedMsgs failed to remove "
				"file \"%s\": %s", (char*)pData->failedMsgFile, errStr);
		}
	}

	RETiRet;
}

BEGINdoHUP
CODESTARTdoHUP
	pthread_mutex_lock(&pData->mutErrFile);
	if(pData->fdErrFile != -1) {
		close(pData->fdErrFile);
		pData->fdErrFile = -1;
	}
	pthread_mutex_unlock(&pData->mutErrFile);
	if (pData->bReopenOnHup) {
		CHKiRet(setupKafkaHandle(pData, 1));
	}
finalize_it:
ENDdoHUP

BEGINcreateInstance
CODESTARTcreateInstance
	pData->currPartition = 0;
	pData->bIsOpen = 0;
	pData->bIsSuspended = 0;
	pData->fdErrFile = -1;
	pData->pTopic = NULL;
	pData->bReportErrs = 1;
	pData->bReopenOnHup = 1;
	pData->bResubmitOnFailure = 0;
	pData->bKeepFailedMessages = 0;
	pData->failedMsgFile = NULL;
	LIST_INIT(&pData->failedmsg_head);
	CHKiRet(pthread_mutex_init(&pData->mutErrFile, NULL));
	CHKiRet(pthread_rwlock_init(&pData->rkLock, NULL));
	CHKiRet(pthread_mutex_init(&pData->mutDynCache, NULL));
finalize_it:
ENDcreateInstance


BEGINcreateWrkrInstance
CODESTARTcreateWrkrInstance
ENDcreateWrkrInstance


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
ENDisCompatibleWithFeature


BEGINfreeInstance
CODESTARTfreeInstance
	/* Helpers for Failed Msg List */
	failedmsg_entry* fmsgEntry1;
	failedmsg_entry* fmsgEntry2;
	/* Close error file */
	if(pData->fdErrFile != -1)
		close(pData->fdErrFile);
	/* Closing Kafka first! */
	pthread_rwlock_wrlock(&pData->rkLock);
	closeKafka(pData);
	if(pData->dynaTopic && pData->dynCache != NULL) {
		d_free(pData->dynCache);
		pData->dynCache = NULL;
	}
	/* Persist failed messages */
	if (pData->bResubmitOnFailure && pData->bKeepFailedMessages && pData->failedMsgFile != NULL) {
		persistFailedMsgs(pData);
	}
	pthread_rwlock_unlock(&pData->rkLock);

	/* Delete Linked List for failed msgs */
	fmsgEntry1	= LIST_FIRST(&pData->failedmsg_head);
	while (fmsgEntry1 != NULL)	{
		fmsgEntry2	= LIST_NEXT(fmsgEntry1,	entries);
		free(fmsgEntry1->payload);
		free(fmsgEntry1->topicname);
		free(fmsgEntry1);
		fmsgEntry1	= fmsgEntry2;
	}
	LIST_INIT(&pData->failedmsg_head);
	/* Free other mem */
	free(pData->errorFile);
	free(pData->failedMsgFile);
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

	pthread_rwlock_destroy(&pData->rkLock);
	pthread_mutex_destroy(&pData->mutErrFile);
	pthread_mutex_destroy(&pData->mutDynCache);
ENDfreeInstance

BEGINfreeWrkrInstance
CODESTARTfreeWrkrInstance
ENDfreeWrkrInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
ENDdbgPrintInstInfo


BEGINtryResume
	int iKafkaRet;
	const struct rd_kafka_metadata *metadata;
CODESTARTtryResume
	CHKiRet(setupKafkaHandle(pWrkrData->pData, 0));

	if ((iKafkaRet = rd_kafka_metadata(pWrkrData->pData->rk, 0, NULL, &metadata, 1000)) != RD_KAFKA_RESP_ERR_NO_ERROR) {
		DBGPRINTF("omkafka: tryResume failed, brokers down %d,%s\n", iKafkaRet, rd_kafka_err2str(iKafkaRet));
		ABORT_FINALIZE(RS_RET_SUSPENDED);
	} else {
		DBGPRINTF("omkafka: tryResume success, %d brokers UP\n", metadata->broker_cnt);
		/* Reset suspended state */
		pWrkrData->pData->bIsSuspended = 0;
		/* free mem*/
		rd_kafka_metadata_destroy(metadata);
	}

finalize_it:
	DBGPRINTF("omkafka: tryResume returned %d\n", iRet);
ENDtryResume

BEGINdoAction
CODESTARTdoAction
	failedmsg_entry* fmsgEntry;
	instanceData *const pData = pWrkrData->pData;
	if (! pData->bIsOpen)
		CHKiRet(setupKafkaHandle(pData, 0));

	/* Lock here to prevent msg loss */
	pthread_rwlock_rdlock(&pData->rkLock);

	/* We need to trigger callbacks first in order to suspend the Action properly on failure */
	const int callbacksCalled = rd_kafka_poll(pData->rk, 0); /* call callbacks */
	DBGPRINTF("omkafka: doAction kafka outqueue length: %d, callbacks called %d\n",
			  rd_kafka_outq_len(pData->rk), callbacksCalled);

	/* Reprocess failed messages! */
	if (pData->bResubmitOnFailure) {
		iRet = checkFailedMessages(pData);
		if(iRet != RS_RET_OK) {
			DBGPRINTF("omkafka: doAction failed to submit FAILED messages with status %d\n", iRet);

			if (pData->bResubmitOnFailure) {
				DBGPRINTF("omkafka: also adding MSG '%.*s' for topic '%s' to failed for RETRY!\n",
					(int)(strlen((char*)ppString[0])-1), ppString[0],
					pData->dynaTopic ? ppString[2] : pData->topic);

				/* Create new Listitem */
				CHKmalloc(fmsgEntry = malloc(sizeof(struct s_failedmsg_entry)));
				fmsgEntry->payload = (uchar*)strdup((char*)ppString[0]);
				fmsgEntry->topicname = (uchar*)strdup( pData->dynaTopic ? (char*)ppString[2] : (char*)pData->topic);

				/* Insert at the head. */
				LIST_INSERT_HEAD(&pData->failedmsg_head, fmsgEntry, entries);
			}
			/* Unlock now */
			pthread_rwlock_unlock(&pData->rkLock);

			ABORT_FINALIZE(iRet);
		}
	}

	/* support dynamic topic */
	iRet = writeKafka(pData, ppString[0], ppString[1], pData->dynaTopic ? ppString[2] : pData->topic);

	/* Unlock now */
	pthread_rwlock_unlock(&pData->rkLock);
finalize_it:
	if(iRet != RS_RET_OK) {
		DBGPRINTF("omkafka: doAction failed with status %d\n", iRet);
	}

	/* Suspend Action if broker problems were reported in error callback */
	if (pData->bIsSuspended) {
		DBGPRINTF("omkafka: doAction broker failure detected, suspending action\n");

		/* Suspend Action now */
		iRet = RS_RET_SUSPENDED;
	}
ENDdoAction


static void
setInstParamDefaults(instanceData *pData)
{
	pData->topic = NULL;
	pData->dynaTopic = 0;
	pData->iDynaTopicCacheSize = 50;
	pData->brokers = NULL;
	pData->autoPartition = 0;
	pData->fixedPartition = NO_FIXED_PARTITION;
	pData->nPartitions = 1;
	pData->nConfParams = 0;
	pData->confParams = NULL;
	pData->nTopicConfParams = 0;
	pData->topicConfParams = NULL;
	pData->errorFile = NULL;
	pData->failedMsgFile = NULL;
	pData->key = NULL;
	pData->closeTimeout = 2000;
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
		if(!strcmp(actpblk.descr[i].name, "topic")) {
			pData->topic = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "dynatopic")) {
			pData->dynaTopic = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "dynatopic.cachesize")) {
			pData->iDynaTopicCacheSize = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "closeTimeout")) {
			pData->closeTimeout = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "partitions.auto")) {
			pData->autoPartition = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "partitions.number")) {
			pData->nPartitions = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "partitions.usefixed")) {
			pData->fixedPartition = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "broker")) {
			es_str_t *es = es_newStr(128);
			int bNeedComma = 0;
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
				char *cstr = es_str2cstr(pvals[i].val.d.ar->arr[j], NULL);
				CHKiRet(processKafkaParam(cstr,
							&pData->confParams[j].name,
							&pData->confParams[j].val));
				free(cstr);
			}
		} else if(!strcmp(actpblk.descr[i].name, "topicconfparam")) {
			pData->nTopicConfParams = pvals[i].val.d.ar->nmemb;
			CHKmalloc(pData->topicConfParams = malloc(sizeof(struct kafka_params) *
			                                      pvals[i].val.d.ar->nmemb ));
			for(int j = 0 ; j <  pvals[i].val.d.ar->nmemb ; ++j) {
				char *cstr = es_str2cstr(pvals[i].val.d.ar->arr[j], NULL);
				CHKiRet(processKafkaParam(cstr,
							&pData->topicConfParams[j].name,
							&pData->topicConfParams[j].val));
				free(cstr);
			}
		} else if(!strcmp(actpblk.descr[i].name, "errorfile")) {
			pData->errorFile = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "key")) {
			pData->key = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "template")) {
			pData->tplName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "reopenOnHup")) {
			pData->bReopenOnHup = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "resubmitOnFailure")) {
			pData->bResubmitOnFailure = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "keepFailedMessages")) {
			pData->bKeepFailedMessages = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "failedMsgFile")) {
			pData->failedMsgFile = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else {
			dbgprintf("omkafka: program error, non-handled param '%s'\n", actpblk.descr[i].name);
		}
	}

	if(pData->brokers == NULL)
		CHKmalloc(pData->brokers = strdup("localhost:9092"));

	if(pData->dynaTopic && pData->topic == NULL) {
        errmsg.LogError(0, RS_RET_CONFIG_ERROR,
            "omkafka: requested dynamic topic, but no "
            "name for topic template given - action definition invalid");
        ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
	}

	iNumTpls = 2;
	if(pData->dynaTopic) ++iNumTpls;
	CODE_STD_STRING_REQUESTnewActInst(iNumTpls);
	CHKiRet(OMSRsetEntry(*ppOMSR, 0, (uchar*)strdup((pData->tplName == NULL) ?
						"RSYSLOG_FileFormat" : (char*)pData->tplName),
						OMSR_NO_RQD_TPL_OPTS));

	CHKiRet(OMSRsetEntry(*ppOMSR, 1, (uchar*)strdup(" KAFKA_TimeStamp"),
						OMSR_NO_RQD_TPL_OPTS));

	if(pData->dynaTopic) {
        CHKiRet(OMSRsetEntry(*ppOMSR, 2, ustrdup(pData->topic),
            OMSR_NO_RQD_TPL_OPTS));
        CHKmalloc(pData->dynCache = (dynaTopicCacheEntry**)
			calloc(pData->iDynaTopicCacheSize, sizeof(dynaTopicCacheEntry*)));
        pData->iCurrElt = -1;
	}
	pthread_mutex_lock(&closeTimeoutMut);
	if (closeTimeout < pData->closeTimeout) {
		closeTimeout = pData->closeTimeout;
	}
	pthread_mutex_unlock(&closeTimeoutMut);

	/* Load failed messages here, do NOT check for IRET!*/
	loadFailedMsgs(pData);

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
	statsobj.Destruct(&kafkaStats);
	CHKiRet(objRelease(errmsg, CORE_COMPONENT));
	CHKiRet(objRelease(statsobj, CORE_COMPONENT));
	DESTROY_ATOMIC_HELPER_MUT(mutClock);

	pthread_mutex_lock(&closeTimeoutMut);
	int timeout = closeTimeout;
	pthread_mutex_unlock(&closeTimeoutMut);
	pthread_mutex_destroy(&closeTimeoutMut);
	if (rd_kafka_wait_destroyed(timeout) != 0) {
		DBGPRINTF("omkafka: couldn't close all resources gracefully. %d threads still remain.\n", rd_kafka_thread_cnt());
	}
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
	uchar *pTmp;
INITLegCnfVars
	*ipIFVersProvided = CURR_MOD_IF_VERSION;
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(datetime, CORE_COMPONENT));
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(strm, CORE_COMPONENT));
	CHKiRet(objUse(statsobj, CORE_COMPONENT));

	INIT_ATOMIC_HELPER_MUT(mutClock);

	DBGPRINTF("omkafka %s using librdkafka version %s\n",
	          VERSION, rd_kafka_version_str());
	CHKiRet(statsobj.Construct(&kafkaStats));
	CHKiRet(statsobj.SetName(kafkaStats, (uchar *)"omkafka"));
	CHKiRet(statsobj.SetOrigin(kafkaStats, (uchar*)"omkafka"));
	STATSCOUNTER_INIT(ctrTopicSubmit, mutCtrTopicSubmit);
	CHKiRet(statsobj.AddCounter(kafkaStats, (uchar *)"submitted",
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrTopicSubmit));
    ctrQueueSize = 0;
    CHKiRet(statsobj.AddCounter(kafkaStats, (uchar *)"maxoutqsize",
        ctrType_Int, CTR_FLAG_NONE, &ctrQueueSize));
	STATSCOUNTER_INIT(ctrKafkaFail, mutCtrKafkaFail);
	CHKiRet(statsobj.AddCounter(kafkaStats, (uchar *)"failures",
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrKafkaFail));
	STATSCOUNTER_INIT(ctrCacheSkip, mutCtrCacheSkip);
	CHKiRet(statsobj.AddCounter(kafkaStats, (uchar *)"topicdynacache.skipped",
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrCacheSkip));
	STATSCOUNTER_INIT(ctrCacheMiss, mutCtrCacheMiss);
	CHKiRet(statsobj.AddCounter(kafkaStats, (uchar *)"topicdynacache.miss",
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrCacheMiss));
	STATSCOUNTER_INIT(ctrCacheEvict, mutCtrCacheEvict);
	CHKiRet(statsobj.AddCounter(kafkaStats, (uchar *)"topicdynacache.evicted",
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrCacheEvict));
	CHKiRet(statsobj.ConstructFinalize(kafkaStats));

	DBGPRINTF("omkafka: Add KAFKA_TimeStamp to template system ONCE\n");
	pTmp = (uchar*) KAFKA_TimeStamp;
	tplAddLine(ourConf, " KAFKA_TimeStamp", &pTmp);

CODEmodInit_QueryRegCFSLineHdlr
ENDmodInit
