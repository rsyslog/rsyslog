/*
 * This file is part of the rsyslog runtime library.
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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <json.h>
#include <sys/stat.h>

#include "rsyslog.h"
#include "srUtils.h"
#include "errmsg.h"
#include "rsconf.h"
#include "datetime.h"
#include "unicode-helper.h"
#include "hashtable_itr.h"
#include <sys/queue.h>

/* definitions for objects we access */
DEFobjStaticHelpers
DEFobjCurrIf(statsobj)
DEFobjCurrIf(datetime)

#define DYNSTATS_PARAM_NAME "name"
#define DYNSTATS_PARAM_RESETTABLE "resettable"
#define DYNSTATS_PARAM_MAX_CARDINALITY "maxCardinality"
#define DYNSTATS_PARAM_UNUSED_METRIC_LIFE "unusedMetricLife" /* in seconds */
#define DYNSTATS_PARAM_PERSISTSTATEINTERVAL "persistStateInterval"
#define DYNSTATS_PARAM_PERSISTSTATE_TIME_INTERVAL "persistStateTimeInterval"
#define DYNSTATS_PARAM_STATEFILE_DIRECTORY "statefile.directory"

#define DYNSTATS_DEFAULT_RESETTABILITY 1
#define DYNSTATS_DEFAULT_MAX_CARDINALITY 2000
#define DYNSTATS_DEFAULT_UNUSED_METRIC_LIFE 3600 /* seconds */
#define DYNSTATS_DEFAULT_PERSISTSTATEINTERVAL 0  /* seconds, default is 0, or never */
#define DYNSTATS_DEFAULT_PERSISTSTATE_TIME_INTERVAL 0  /* seconds, default is 0, or never */

#define DYNSTATS_MAX_BUCKET_NS_METRIC_LENGTH 100
#define DYNSTATS_METRIC_NAME_SEPARATOR '.'
#define DYNSTATS_HASHTABLE_SIZE_OVERPROVISIONING 1.25
#define DYNSTATS_BUCKET_PERSIST_NAME "name"
#define DYNSTATS_BUCKET_PERSIST_VALUES_NAME "values"

static struct cnfparamdescr modpdescr[] = {
	{ DYNSTATS_PARAM_NAME, eCmdHdlrString, CNFPARAM_REQUIRED },
	{ DYNSTATS_PARAM_RESETTABLE, eCmdHdlrBinary, 0 },
	{ DYNSTATS_PARAM_MAX_CARDINALITY, eCmdHdlrPositiveInt, 0},
	{ DYNSTATS_PARAM_UNUSED_METRIC_LIFE, eCmdHdlrPositiveInt, 0}, /* in minutes */
	{ DYNSTATS_PARAM_PERSISTSTATEINTERVAL, eCmdHdlrPositiveInt, 0 },
	{ DYNSTATS_PARAM_PERSISTSTATE_TIME_INTERVAL, eCmdHdlrPositiveInt, 0 },
	{ DYNSTATS_PARAM_STATEFILE_DIRECTORY, eCmdHdlrString, 0 }
};

static struct cnfparamblk modpblk =
{
	CNFPARAMBLK_VERSION,
	sizeof(modpdescr)/sizeof(struct cnfparamdescr),
	modpdescr
};

static rsRetVal dynstats_addNewCtr(dynstats_bucket_t *b, const uchar* metric, uint8_t doInitialIncrement,
		uint64_t doInitialOffset);
static int getFullStateFileName(dynstats_bucket_t *b, uchar* pszstatefile, uchar* pszout, int buflen);
static uchar* getStateFileName(const dynstats_bucket_t *const pbucket,
		uchar *const __restrict__ buf, const size_t lenbuf) ATTR_NONNULL(1, 2);
static rsRetVal persistBucketState(dynstats_bucket_t *b, sbool useWorker) ATTR_NONNULL(1);
static rsRetVal initFileWriteWorker(dynstats_buckets_t *bkts);
static rsRetVal enqueueFileWriteTask(dynstats_bucket_t *b, const char* file_name, const char* content);
static void startFileWriteWorker(dynstats_buckets_t *bkts);
static void stopFileWriteWorker(dynstats_buckets_t *bkts);
static void destroyFileWriteWorker(dynstats_buckets_t *bkts);

rsRetVal
dynstatsClassInit(void) {
	DEFiRet;
	CHKiRet(objGetObjInterface(&obj));
	CHKiRet(objUse(statsobj, CORE_COMPONENT));
	CHKiRet(objUse(datetime, CORE_COMPONENT));
finalize_it:
	RETiRet;
}

static void
dynstats_destroyCtr(dynstats_ctr_t *ctr) {
	statsobj.DestructUnlinkedCounter(ctr->pCtr);
	free(ctr->metric);
	free(ctr);
}

static void /* assumes exclusive access to bucket */
dynstats_destroyCountersIn(dynstats_bucket_t *b, htable *table, dynstats_ctr_t *ctrs) {
	dynstats_ctr_t *ctr;
	int ctrs_purged = 0;
	hashtable_destroy(table, 0);
	while (ctrs != NULL) {
		ctr = ctrs;
		ctrs = ctrs->next;
		dynstats_destroyCtr(ctr);
		ctrs_purged++;
	}
	STATSCOUNTER_ADD(b->ctrMetricsPurged, b->mutCtrMetricsPurged, ctrs_purged);
	ATOMIC_SUB_unsigned(&b->metricCount, ctrs_purged, &b->mutMetricCount);
}

static void /* assumes exclusive access to bucket */
dynstats_destroyCounters(dynstats_bucket_t *b) {
	statsobj.UnlinkAllCounters(b->stats);
	dynstats_destroyCountersIn(b, b->table, b->ctrs);
}

static void
dynstats_destroyBucket(dynstats_bucket_t* b) {
	dynstats_buckets_t *bkts;

	bkts = &loadConf->dynstats_buckets;

	if(b->persist_state_write_count_interval) {
		persistBucketState(b, 0);
	}
	pthread_rwlock_wrlock(&b->lock);
	dynstats_destroyCounters(b);
	dynstats_destroyCountersIn(b, b->survivor_table, b->survivor_ctrs);
	statsobj.DestructCounter(b->stats, b->pCtrFlushedBytes);
	statsobj.DestructCounter(b->stats, b->pCtrFlushed);
	statsobj.DestructCounter(b->stats, b->pCtrFlushedErrors);
	statsobj.Destruct(&b->stats);
	free(b->name);
	free(b->state_file_directory);
	pthread_rwlock_unlock(&b->lock);
	pthread_rwlock_destroy(&b->lock);
	pthread_mutex_destroy(&b->mutMetricCount);
	statsobj.DestructCounter(bkts->global_stats, b->pOpsOverflowCtr);
	statsobj.DestructCounter(bkts->global_stats, b->pNewMetricAddCtr);
	statsobj.DestructCounter(bkts->global_stats, b->pNoMetricCtr);
	statsobj.DestructCounter(bkts->global_stats, b->pMetricsPurgedCtr);
	statsobj.DestructCounter(bkts->global_stats, b->pOpsIgnoredCtr);
	statsobj.DestructCounter(bkts->global_stats, b->pPurgeTriggeredCtr);
	free(b);
}

static rsRetVal
dynstats_addBucketMetrics(dynstats_buckets_t *bkts, dynstats_bucket_t *b, const uchar* name) {
	uchar *metric_name_buff, *metric_suffix;
	const uchar *suffix_litteral;
	int name_len;
	DEFiRet;

	name_len = ustrlen(name);
	CHKmalloc(metric_name_buff = malloc((name_len + DYNSTATS_MAX_BUCKET_NS_METRIC_LENGTH + 1) * sizeof(uchar)));

	strcpy((char*)metric_name_buff, (char*)name);
	metric_suffix = metric_name_buff + name_len;
	*metric_suffix = DYNSTATS_METRIC_NAME_SEPARATOR;
	metric_suffix++;

	suffix_litteral = UCHAR_CONSTANT("ops_overflow");
	ustrncpy(metric_suffix, suffix_litteral, DYNSTATS_MAX_BUCKET_NS_METRIC_LENGTH);
	STATSCOUNTER_INIT(b->ctrOpsOverflow, b->mutCtrOpsOverflow);
	CHKiRet(statsobj.AddManagedCounter(bkts->global_stats, metric_name_buff, ctrType_IntCtr,
									   CTR_FLAG_RESETTABLE,
										&(b->ctrOpsOverflow),
										&b->pOpsOverflowCtr, 1));

	suffix_litteral = UCHAR_CONSTANT("new_metric_add");
	ustrncpy(metric_suffix, suffix_litteral, DYNSTATS_MAX_BUCKET_NS_METRIC_LENGTH);
	STATSCOUNTER_INIT(b->ctrNewMetricAdd, b->mutCtrNewMetricAdd);
	CHKiRet(statsobj.AddManagedCounter(bkts->global_stats, metric_name_buff, ctrType_IntCtr,
									   CTR_FLAG_RESETTABLE,
										&(b->ctrNewMetricAdd),
										&b->pNewMetricAddCtr, 1));

	suffix_litteral = UCHAR_CONSTANT("no_metric");
	ustrncpy(metric_suffix, suffix_litteral, DYNSTATS_MAX_BUCKET_NS_METRIC_LENGTH);
	STATSCOUNTER_INIT(b->ctrNoMetric, b->mutCtrNoMetric);
	CHKiRet(statsobj.AddManagedCounter(bkts->global_stats, metric_name_buff, ctrType_IntCtr,
									   CTR_FLAG_RESETTABLE,
										&(b->ctrNoMetric),
										&b->pNoMetricCtr, 1));

	suffix_litteral = UCHAR_CONSTANT("metrics_purged");
	ustrncpy(metric_suffix, suffix_litteral, DYNSTATS_MAX_BUCKET_NS_METRIC_LENGTH);
	STATSCOUNTER_INIT(b->ctrMetricsPurged, b->mutCtrMetricsPurged);
	CHKiRet(statsobj.AddManagedCounter(bkts->global_stats, metric_name_buff, ctrType_IntCtr,
									   CTR_FLAG_RESETTABLE,
										&(b->ctrMetricsPurged),
										&b->pMetricsPurgedCtr, 1));

	suffix_litteral = UCHAR_CONSTANT("ops_ignored");
	ustrncpy(metric_suffix, suffix_litteral, DYNSTATS_MAX_BUCKET_NS_METRIC_LENGTH);
	STATSCOUNTER_INIT(b->ctrOpsIgnored, b->mutCtrOpsIgnored);
	CHKiRet(statsobj.AddManagedCounter(bkts->global_stats, metric_name_buff, ctrType_IntCtr,
									   CTR_FLAG_RESETTABLE,
										&(b->ctrOpsIgnored),
										&b->pOpsIgnoredCtr, 1));

	suffix_litteral = UCHAR_CONSTANT("purge_triggered");
	ustrncpy(metric_suffix, suffix_litteral, DYNSTATS_MAX_BUCKET_NS_METRIC_LENGTH);
	STATSCOUNTER_INIT(b->ctrPurgeTriggered, b->mutCtrPurgeTriggered);
	CHKiRet(statsobj.AddManagedCounter(bkts->global_stats, metric_name_buff, ctrType_IntCtr,
									   CTR_FLAG_RESETTABLE,
										&(b->ctrPurgeTriggered),
										&b->pPurgeTriggeredCtr, 1));

finalize_it:
	free(metric_name_buff);
	if (iRet != RS_RET_OK) {
		if (b->pOpsOverflowCtr != NULL) {
			statsobj.DestructCounter(bkts->global_stats, b->pOpsOverflowCtr);
		}
		if (b->pNewMetricAddCtr != NULL) {
			statsobj.DestructCounter(bkts->global_stats, b->pNewMetricAddCtr);
		}
		if (b->pNoMetricCtr != NULL) {
			statsobj.DestructCounter(bkts->global_stats, b->pNoMetricCtr);
		}
		if (b->pMetricsPurgedCtr != NULL) {
			statsobj.DestructCounter(bkts->global_stats, b->pMetricsPurgedCtr);
		}
		if (b->pOpsIgnoredCtr != NULL) {
			statsobj.DestructCounter(bkts->global_stats, b->pOpsIgnoredCtr);
		}
		if (b->pPurgeTriggeredCtr != NULL) {
			statsobj.DestructCounter(bkts->global_stats, b->pPurgeTriggeredCtr);
		}
	}
	RETiRet;
}

static void
no_op_free(void __attribute__((unused)) *ignore)  {}

static rsRetVal  /* assumes exclusive access to bucket */
dynstats_rebuildSurvivorTable(dynstats_bucket_t *b) {
	htable *survivor_table = NULL;
	htable *new_table = NULL;
	size_t htab_sz;
	DEFiRet;

	htab_sz = (size_t) (DYNSTATS_HASHTABLE_SIZE_OVERPROVISIONING * b->maxCardinality + 1);
	if (b->table == NULL) {
		CHKmalloc(survivor_table = create_hashtable(htab_sz, hash_from_string, key_equals_string,
			no_op_free));
	}
	CHKmalloc(new_table = create_hashtable(htab_sz, hash_from_string, key_equals_string, no_op_free));
	statsobj.UnlinkAllCounters(b->stats);
	if (b->survivor_table != NULL) {
		dynstats_destroyCountersIn(b, b->survivor_table, b->survivor_ctrs);
	}
	b->survivor_table = (b->table == NULL) ? survivor_table : b->table;
	b->survivor_ctrs = b->ctrs;
	b->table = new_table;
	b->ctrs = NULL;
finalize_it:
	if (iRet != RS_RET_OK) {
		LogError(errno, RS_RET_INTERNAL_ERROR, "error trying to evict "
			"TTL-expired metrics of dyn-stats bucket named: %s", b->name);
		if (new_table == NULL) {
			LogError(errno, RS_RET_INTERNAL_ERROR, "error trying to "
				"initialize hash-table for dyn-stats bucket named: %s", b->name);
		} else {
			assert(0); /* "can" not happen -- triggers Coverity CID 184307:
			hashtable_destroy(new_table, 0);
			We keep this as guard should code above change in the future */
		}
		if (b->table == NULL) {
			if (survivor_table == NULL) {
				LogError(errno, RS_RET_INTERNAL_ERROR, "error trying to initialize "
				"ttl-survivor hash-table for dyn-stats bucket named: %s", b->name);
			} else {
				hashtable_destroy(survivor_table, 0);
			}
		}
	}
	RETiRet;
}

static rsRetVal
dynstats_resetBucket(dynstats_bucket_t *b) {
	DEFiRet;
	pthread_rwlock_wrlock(&b->lock);
	CHKiRet(dynstats_rebuildSurvivorTable(b));
	STATSCOUNTER_INC(b->ctrPurgeTriggered, b->mutCtrPurgeTriggered);
	timeoutComp(&b->metricCleanupTimeout, b->unusedMetricLife);
finalize_it:
	pthread_rwlock_unlock(&b->lock);
	RETiRet;
}

static void
dynstats_resetIfExpired(dynstats_bucket_t *b) {
	long timeout;
	pthread_rwlock_rdlock(&b->lock);
	timeout = timeoutVal(&b->metricCleanupTimeout);
	pthread_rwlock_unlock(&b->lock);
	if (timeout == 0) {
		LogMsg(0, RS_RET_TIMED_OUT, LOG_INFO, "dynstats: bucket '%s' is being reset", b->name);
		dynstats_resetBucket(b);
	}
}

static void
dynstats_readCallback(statsobj_t __attribute__((unused)) *ignore, void *b) {
	dynstats_buckets_t *bkts;
	bkts = &loadConf->dynstats_buckets;

	pthread_rwlock_rdlock(&bkts->lock);
	dynstats_resetIfExpired((dynstats_bucket_t *) b);
	pthread_rwlock_unlock(&bkts->lock);
}

static rsRetVal
dynstats_initNewBucketStats(dynstats_bucket_t *b) {
	DEFiRet;

	CHKiRet(statsobj.Construct(&b->stats));
	CHKiRet(statsobj.SetOrigin(b->stats, UCHAR_CONSTANT("dynstats.bucket")));
	CHKiRet(statsobj.SetName(b->stats, b->name));
	CHKiRet(statsobj.SetReportingNamespace(b->stats, UCHAR_CONSTANT("values")));

	char bucket_stat_name[512];
	int bufsize = sizeof(bucket_stat_name);
	int rc = 0;
	rc = snprintf(bucket_stat_name, bufsize, "%s_%s", b->name, "flushed_bytes");
	assert(rc < 0 || rc <= bufsize);
	STATSCOUNTER_INIT(b->ctrFlushedBytes, b->mutCtrFlushedBytes);
	CHKiRet(statsobj.AddManagedCounter(b->stats, (uchar*)bucket_stat_name,
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &b->ctrFlushedBytes, &b->pCtrFlushedBytes, 0));

	rc = snprintf(bucket_stat_name, bufsize, "%s_%s", b->name, "flushed_counts");
	assert(rc < 0 || rc <= bufsize);
	STATSCOUNTER_INIT(b->ctrFlushed, b->mutCtrFlushed);
	CHKiRet(statsobj.AddManagedCounter(b->stats, (uchar*)bucket_stat_name,
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &b->ctrFlushed, &b->pCtrFlushed, 0));

	rc = snprintf(bucket_stat_name, bufsize, "%s_%s", b->name, "flush_errors");
	assert(rc < 0 || rc <= bufsize);
	STATSCOUNTER_INIT(b->ctrFlushedErrors, b->mutCtrFlushedErrors);
	CHKiRet(statsobj.AddManagedCounter(b->stats, (uchar*)bucket_stat_name,
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &b->ctrFlushedErrors, &b->pCtrFlushedErrors, 0));

	statsobj.SetReadNotifier(b->stats, dynstats_readCallback, b);
	CHKiRet(statsobj.ConstructFinalize(b->stats));

finalize_it:
	RETiRet;
}

/* try to open a file which has a state file. If the state file does not
 * exist or cannot be read, an error is returned.
 */
static rsRetVal ATTR_NONNULL(1) loadPersistedState(dynstats_bucket_t *b) {
	DEFiRet;
	struct stat stat_buf;
	uchar statefile[MAXFNAME];
	uchar state_file_path[MAXFNAME];
	json_object *json_obj = NULL;

	uchar *const state_file_name = getStateFileName(b, statefile, sizeof(statefile));
	assert(state_file_name);
	/* Get full path and file name */
	getFullStateFileName(b, state_file_name, state_file_path, sizeof(state_file_path));
	DBGPRINTF("opening statefile for '%s', state file '%s'\n", b->name, state_file_path);

	/* check if the file exists */
	if(stat((char*) state_file_path, &stat_buf) == -1) {
		LogMsg(0, RS_RET_FILE_NOT_FOUND, LOG_INFO,
				"dyn_stats: warning state file doesn't exist: '%s'", state_file_path);
		ABORT_FINALIZE(RS_RET_FILE_NOT_FOUND);
	}

	json_obj = fjson_object_from_file((const char*)state_file_path);
	if (!json_obj) {
		LogMsg(0, RS_RET_JSON_UNUSABLE, LOG_INFO,
				"dyn_stats: error couldn't read json from file.");
		ABORT_FINALIZE(RS_RET_JSON_UNUSABLE);
	}

	/* expected json format
	 * { "name": "bucketname" "values": { "foo": "1" } }
	 */

	json_object *json_obj_metrics = NULL;
	if (json_object_object_get_ex(json_obj, DYNSTATS_BUCKET_PERSIST_VALUES_NAME, &json_obj_metrics)) {
		struct json_object_iterator it, itEnd;
		for (it = json_object_iter_begin(json_obj_metrics), itEnd = json_object_iter_end(json_obj_metrics);
				!json_object_iter_equal(&it, &itEnd);
				json_object_iter_next(&it)) {
			const uchar *metric = (const uchar*)json_object_iter_peek_name(&it);
			uint64_t val = json_object_get_int64(json_object_iter_peek_value(&it));
			// now add the counter to the bucket with offset
			dynstats_addNewCtr(b, metric, 0, val);
		}
	}

finalize_it:
	if (json_obj) {
		json_object_put(json_obj);
	}
	RETiRet;
}

static rsRetVal
dynstats_openStateFile(dynstats_bucket_t *b) {
	DEFiRet;

	CHKiRet(loadPersistedState(b));

finalize_it:
	RETiRet;
}

static rsRetVal
dynstats_newBucket(const uchar* name, uint8_t resettable, uint32_t maxCardinality, uint32_t unusedMetricLife,
		uint32_t persistStateInterval, uint32_t persistStateTimeInterval, const uchar *stateFileDirectory) {
	dynstats_bucket_t *b;
	dynstats_buckets_t *bkts;
	uint8_t lock_initialized, metric_count_mutex_initialized;
	pthread_rwlockattr_t bucket_lock_attr;
	DEFiRet;

	lock_initialized = metric_count_mutex_initialized = 0;
	b = NULL;

	bkts = &loadConf->dynstats_buckets;

	if (bkts->initialized) {
		CHKmalloc(b = calloc(1, sizeof(dynstats_bucket_t)));
		b->resettable = resettable;
		b->maxCardinality = maxCardinality;
		b->unusedMetricLife = 1000 * unusedMetricLife;
		b->persist_state_write_count_interval = persistStateInterval;
		b->persist_state_interval_secs = persistStateTimeInterval;
		b->n_updates = 0;
		CHKmalloc(b->name = ustrdup(name));
		if (stateFileDirectory) {
			CHKmalloc(b->state_file_directory = ustrdup(stateFileDirectory));
		}

		pthread_rwlockattr_init(&bucket_lock_attr);
#ifdef HAVE_PTHREAD_RWLOCKATTR_SETKIND_NP
		pthread_rwlockattr_setkind_np(&bucket_lock_attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
#endif

		pthread_rwlock_init(&b->lock, &bucket_lock_attr);
		lock_initialized = 1;
		pthread_mutex_init(&b->mutMetricCount, NULL);
		metric_count_mutex_initialized = 1;

		CHKiRet(dynstats_initNewBucketStats(b));

		CHKiRet(dynstats_resetBucket(b));

		CHKiRet(dynstats_addBucketMetrics(bkts, b, name));

		if (b && (b->persist_state_write_count_interval || b->persist_state_interval_secs)) {
			time_t now;
			rsRetVal rc = dynstats_openStateFile(b);
			if (rc != RS_RET_OK) {
				LogMsg(0, rc, LOG_INFO, "dynstats: openStateFile failed - "
				"there should be messages before this one with the reason.\n");
			}
			if (datetime.GetTime(&now) == -1) {
				ABORT_FINALIZE(RS_RET_INVLD_TIME);
			}
			b->persist_expiration_time = now + persistStateTimeInterval;
		}

		pthread_rwlock_wrlock(&bkts->lock);
		if (bkts->list == NULL) {
			bkts->list = b;
		} else {
			b->next = bkts->list;
			bkts->list = b;
		}
		pthread_rwlock_unlock(&bkts->lock);
	} else {
		LogError(0, RS_RET_INTERNAL_ERROR, "dynstats: bucket creation failed, as "
		"global-initialization of buckets was unsuccessful");
		ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
	}
finalize_it:
	if (iRet != RS_RET_OK) {
		if (metric_count_mutex_initialized) {
			pthread_mutex_destroy(&b->mutMetricCount);
		}
		if (b != NULL) {
			dynstats_destroyBucket(b);
		}
		if (lock_initialized) {
			pthread_rwlock_destroy(&b->lock);
		}
	}
	RETiRet;
}

rsRetVal
dynstats_processCnf(struct cnfobj *o) {
	struct cnfparamvals *pvals;
	short i;
	uchar *name = NULL;
	uchar *stateFileDirectory = NULL;
	uint8_t resettable = DYNSTATS_DEFAULT_RESETTABILITY;
	uint32_t maxCardinality = DYNSTATS_DEFAULT_MAX_CARDINALITY;
	uint32_t unusedMetricLife = DYNSTATS_DEFAULT_UNUSED_METRIC_LIFE;
	uint32_t persistStateInterval = DYNSTATS_DEFAULT_PERSISTSTATEINTERVAL;
	uint32_t persistStateTimeInterval = DYNSTATS_DEFAULT_PERSISTSTATE_TIME_INTERVAL;
	DEFiRet;

	pvals = nvlstGetParams(o->nvlst, &modpblk, NULL);
	if(pvals == NULL) {
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	for(i = 0 ; i < modpblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(modpblk.descr[i].name, DYNSTATS_PARAM_NAME)) {
			CHKmalloc(name = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL));
		} else if (!strcmp(modpblk.descr[i].name, DYNSTATS_PARAM_RESETTABLE)) {
			resettable = (pvals[i].val.d.n != 0);
		} else if (!strcmp(modpblk.descr[i].name, DYNSTATS_PARAM_MAX_CARDINALITY)) {
			maxCardinality = (uint32_t) pvals[i].val.d.n;
		} else if (!strcmp(modpblk.descr[i].name, DYNSTATS_PARAM_UNUSED_METRIC_LIFE)) {
			unusedMetricLife = (uint32_t) pvals[i].val.d.n;
		} else if (!strcmp(modpblk.descr[i].name, DYNSTATS_PARAM_PERSISTSTATEINTERVAL)) {
			persistStateInterval = (uint32_t) pvals[i].val.d.n;
		} else if (!strcmp(modpblk.descr[i].name, DYNSTATS_PARAM_PERSISTSTATE_TIME_INTERVAL)) {
			persistStateTimeInterval = (uint32_t) pvals[i].val.d.n;
		} else if (!strcmp(modpblk.descr[i].name, DYNSTATS_PARAM_STATEFILE_DIRECTORY)) {
			CHKmalloc(stateFileDirectory = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL));
		} else {
			dbgprintf("dyn_stats: program error, non-handled "
					  "param '%s'\n", modpblk.descr[i].name);
		}
	}
	if (name != NULL) {
		CHKiRet(dynstats_newBucket(name, resettable, maxCardinality, unusedMetricLife,
					persistStateInterval, persistStateTimeInterval, stateFileDirectory));
	}

finalize_it:
	free(name);
	free(stateFileDirectory);
	cnfparamvalsDestruct(pvals, &modpblk);
	RETiRet;
}

rsRetVal
dynstats_initCnf(dynstats_buckets_t *bkts) {
	DEFiRet;

	bkts->initialized = 0;

	bkts->list = NULL;
	CHKiRet(statsobj.Construct(&bkts->global_stats));
	CHKiRet(statsobj.SetOrigin(bkts->global_stats, UCHAR_CONSTANT("dynstats")));
	CHKiRet(statsobj.SetName(bkts->global_stats, UCHAR_CONSTANT("global")));
	CHKiRet(statsobj.SetReportingNamespace(bkts->global_stats, UCHAR_CONSTANT("values")));
	CHKiRet(statsobj.ConstructFinalize(bkts->global_stats));
	pthread_rwlock_init(&bkts->lock, NULL);

	bkts->initialized = 1;
	initFileWriteWorker(bkts);
	startFileWriteWorker(bkts);

finalize_it:
	if (iRet != RS_RET_OK) {
		statsobj.Destruct(&bkts->global_stats);
	}
	RETiRet;
}

void
dynstats_destroyAllBuckets(void) {
	dynstats_buckets_t *bkts;
	dynstats_bucket_t *b;
	bkts = &loadConf->dynstats_buckets;
	if (bkts->initialized) {
		pthread_rwlock_wrlock(&bkts->lock);
		stopFileWriteWorker(bkts);
		destroyFileWriteWorker(bkts);
		while(1) {
			b = bkts->list;
			if (b == NULL) {
				break;
			} else {
				bkts->list = b->next;
				dynstats_destroyBucket(b);
			}
		}
		statsobj.Destruct(&bkts->global_stats);
		pthread_rwlock_unlock(&bkts->lock);
		pthread_rwlock_destroy(&bkts->lock);
	}
}

dynstats_bucket_t *
dynstats_findBucket(const uchar* name) {
	dynstats_buckets_t *bkts;
	dynstats_bucket_t *b;
	bkts = &loadConf->dynstats_buckets;
	if (bkts->initialized) {
		pthread_rwlock_rdlock(&bkts->lock);
		b = bkts->list;
		while(b != NULL) {
			if (! ustrcmp(name, b->name)) {
				break;
			}
			b = b->next;
		}
		pthread_rwlock_unlock(&bkts->lock);
	} else {
		b = NULL;
		LogError(0, RS_RET_INTERNAL_ERROR, "dynstats: bucket lookup failed, as global-initialization "
		"of buckets was unsuccessful");
	}

	return b;
}

static rsRetVal
dynstats_createCtr(dynstats_bucket_t *b, const uchar* metric, dynstats_ctr_t **ctr) {
	DEFiRet;

	CHKmalloc(*ctr = calloc(1, sizeof(dynstats_ctr_t)));
	CHKmalloc((*ctr)->metric = ustrdup(metric));
	STATSCOUNTER_INIT((*ctr)->ctr, (*ctr)->mutCtr);
	CHKiRet(statsobj.AddManagedCounter(b->stats, metric, ctrType_IntCtr,
				b->resettable ? CTR_FLAG_MUST_RESET : CTR_FLAG_NONE,
				&(*ctr)->ctr, &(*ctr)->pCtr, 0));
finalize_it:
	if (iRet != RS_RET_OK) {
		if ((*ctr) != NULL) {
			free((*ctr)->metric);
			free(*ctr);
			*ctr = NULL;
		}
	}
	RETiRet;
}

/* not supported in older platforms - disable */
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif
/*
 *	Helper function adapted from imfile of the same name - possible candidate for refactor.
 *	Updates configured state file with the state of the specified bucket
 */
static rsRetVal ATTR_NONNULL(1)
writeStateFile(dynstats_bucket_t *b, const char *file_name, const char *content) {
	assert(b);
	const size_t toWrite = strlen(content);
	ssize_t w = 0;
	DEFiRet;
	const int fd = open(file_name, O_CLOEXEC | O_NOCTTY | O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if(fd < 0) {
		LogError(errno, RS_RET_IO_ERROR, "dynstats: cannot open state file '%s' for "
			"persisting file state - some data will probably be duplicated "
			"on next startup", file_name);
		ABORT_FINALIZE(RS_RET_IO_ERROR);
	}

	/* note: write is immediately sync'd */
	w = write(fd, content, toWrite);
	if(w != (ssize_t) toWrite) {
		LogError(errno, RS_RET_IO_ERROR, "dynstats: partial write to state file '%s' "
			"this may cause trouble in the future. We will try to delete the "
			"state file, as this provides most consistent state", file_name);
		unlink(file_name);
		ABORT_FINALIZE(RS_RET_IO_ERROR);
	}

finalize_it:
	if (iRet == RS_RET_IO_ERROR) {
		STATSCOUNTER_INC(b->ctrFlushedErrors, b->mutCtrFlushedErrors);
	}
	if (w != -1) {
		STATSCOUNTER_ADD(b->ctrFlushedBytes, b->mutCtrFlushedBytes, w);
		STATSCOUNTER_INC(b->ctrFlushed, b->mutCtrFlushed);
	}

	if(fd >= 0) {
		close(fd);
	}
	RETiRet;
}

static const uchar *
getStateFileDir(dynstats_bucket_t *b)
{
	const uchar *wrkdir;
	assert(b != NULL);
	if (b->state_file_directory == NULL) {
		wrkdir = glblGetWorkDirRaw();
	} else {
		wrkdir = b->state_file_directory;
	}
	return(wrkdir);
}

/*
*	Helper function to combine statefile and workdir
*/
static int getFullStateFileName(dynstats_bucket_t *b, uchar* pszstatefile, uchar* pszout, int buflen)
{
	int lenout;
	const uchar* pszworkdir;

	/* Get Raw Workdir, if it is NULL we need to propper handle it */
	pszworkdir = getStateFileDir(b);

	/* Construct file name */
	lenout = snprintf((char*)pszout, buflen, "%s/%s",
					(char*) (pszworkdir == NULL ? "." : (char*) pszworkdir), (char*)pszstatefile);

	/* return out length */
	return lenout;
}

/* this generates a state file name suitable for the given file. To avoid
 * malloc calls, it must be passed a buffer which should be MAXFNAME large.
 * Note: the buffer is not necessarily populated ... always ONLY use the
 * RETURN VALUE!
 * This function is guranteed to work only on config data and DOES NOT
 * open or otherwise modify disk file state.
 */
static uchar * ATTR_NONNULL(1, 2)
getStateFileName(const dynstats_bucket_t *const pbucket,
		uchar *const __restrict__ buf,
		const size_t lenbuf) {

	DBGPRINTF("getStateFileName for '%s'\n", pbucket->name);
	snprintf((char*)buf, lenbuf - 1, "dynstats-state:%s", pbucket->name);
	DBGPRINTF("getStateFileName:  state file name now is %s\n", buf);
	return buf;
}

/* This function persists dynstats_bucket_t data, which for the time being:
 * metric bucket counters
 */
static rsRetVal ATTR_NONNULL(1)
persistBucketState(dynstats_bucket_t *b, sbool useWorker) {
	DEFiRet;
	uchar statefile[MAXFNAME];
	uchar statefname[MAXFNAME];
	struct json_object *jval = NULL;
	struct json_object *json = NULL;
	struct json_object *json_bucket_values = NULL;

	assert (b->name);
	uchar *const statefn = getStateFileName(b, statefile, sizeof(statefile));
	/* Get full path and file name */
	getFullStateFileName(b, statefn, statefname, sizeof(statefname));
	DBGPRINTF("persisting state for '%s', state file '%s'\n", b->name, statefname);

	CHKmalloc(json = json_object_new_object());
	jval = json_object_new_string((char*) b->name);
	json_object_object_add(json, DYNSTATS_BUCKET_PERSIST_NAME, jval);

	// walk the hashtable and create a val for each
	CHKmalloc(json_bucket_values = json_object_new_object());
	json_object_object_add(json, DYNSTATS_BUCKET_PERSIST_VALUES_NAME, json_bucket_values);

	if (hashtable_count(b->table) > 0) {
		struct hashtable_itr *itr = hashtable_iterator(b->table);
		dynstats_ctr_t *pctr = NULL;
		do {
			pctr = (dynstats_ctr_t*) hashtable_iterator_value(itr);
			jval = json_object_new_int64(pctr->ctr);
			json_object_object_add(json_bucket_values, (const char*)pctr->metric, jval);
		} while (hashtable_iterator_advance(itr));
		free(itr);
	}

	const char *jstr =  json_object_to_json_string_ext(json, JSON_C_TO_STRING_PLAIN);
	if (useWorker) {
		CHKiRet(enqueueFileWriteTask(b, (const char*)statefname, jstr));
	} else {
		writeStateFile(b, (const char*)statefname, jstr);
	}

finalize_it:
	if(iRet != RS_RET_OK) {
		LogError(0, iRet, "dynstats: could not persist state "
				"file %s - data may be repeated on next "
				"startup. Is WorkDirectory set?",
				statefname);
	}
	if (json) {
		json_object_put(json);
	}

	RETiRet;
}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized" /* TODO: how can we fix these warnings? */
#endif
static rsRetVal
dynstats_addNewCtr(dynstats_bucket_t *b, const uchar* metric, uint8_t doInitialIncrement, uint64_t doInitialOffset) {
	dynstats_ctr_t *ctr;
	dynstats_ctr_t *found_ctr, *survivor_ctr, *effective_ctr;
	int created;
	uchar *copy_of_key = NULL;
	DEFiRet;

	created = 0;
	ctr = NULL;

	if ((unsigned) ATOMIC_FETCH_32BIT_unsigned(&b->metricCount, &b->mutMetricCount) >= b->maxCardinality) {
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}

	CHKiRet(dynstats_createCtr(b, metric, &ctr));

	pthread_rwlock_wrlock(&b->lock);
	found_ctr = (dynstats_ctr_t*) hashtable_search(b->table, ctr->metric);
	if (found_ctr != NULL) {
		if (doInitialIncrement) {
			STATSCOUNTER_INC(found_ctr->ctr, found_ctr->mutCtr);
		}
	} else {
		copy_of_key = ustrdup(ctr->metric);
		if (copy_of_key != NULL) {
			survivor_ctr = (dynstats_ctr_t*) hashtable_search(b->survivor_table, ctr->metric);
			if (survivor_ctr == NULL) {
				effective_ctr = ctr;
			} else {
				effective_ctr = survivor_ctr;
				if (survivor_ctr->prev != NULL) {
					survivor_ctr->prev->next = survivor_ctr->next;
				}
				if (survivor_ctr->next != NULL) {
					survivor_ctr->next->prev = survivor_ctr->prev;
				}
				if (survivor_ctr == b->survivor_ctrs) {
					b->survivor_ctrs = survivor_ctr->next;
				}
			}
			if ((created = hashtable_insert(b->table, copy_of_key, effective_ctr))) {
				statsobj.AddPreCreatedCtr(b->stats, effective_ctr->pCtr);
			}
		}
		if (created) {
			if (b->ctrs != NULL) {
				b->ctrs->prev = effective_ctr;
			}
			effective_ctr->prev = NULL;
			effective_ctr->next = b->ctrs;
			b->ctrs = effective_ctr;
			if (doInitialIncrement) {
				STATSCOUNTER_INC(effective_ctr->ctr, effective_ctr->mutCtr);
			}
			if (doInitialOffset) {
				// doInitialOffset regardless of the state of GatherStats
				effective_ctr->ctr += doInitialOffset;
			}
		}
	}
	pthread_rwlock_unlock(&b->lock);

	if (found_ctr != NULL) {
		//ignore
	} else if (created && (effective_ctr != survivor_ctr)) {
		ATOMIC_INC(&b->metricCount, &b->mutMetricCount);
		STATSCOUNTER_INC(b->ctrNewMetricAdd, b->mutCtrNewMetricAdd);
	} else if (! created) {
		if (copy_of_key != NULL) {
			free(copy_of_key);
		}
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}

finalize_it:
	if (((! created) || (effective_ctr != ctr)) && (ctr != NULL)) {
		dynstats_destroyCtr(ctr);
	}
	RETiRet;
}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

static sbool dynstats_shouldPersist(dynstats_bucket_t *b, time_t now) {
	sbool shouldPersist = FALSE;
	pthread_rwlock_rdlock(&b->lock);
	shouldPersist =
		((b->persist_state_write_count_interval > 0 &&
			++b->n_updates >= b->persist_state_write_count_interval) ||
			(b->persist_expiration_time && now >= b->persist_expiration_time));
	pthread_rwlock_unlock(&b->lock);
	return shouldPersist;
}

rsRetVal
dynstats_inc(dynstats_bucket_t *b, uchar* metric) {
	dynstats_ctr_t *ctr = NULL;
	DEFiRet;

	if (! GatherStats) {
		FINALIZE;
	}

	if (ustrlen(metric) == 0) {
		STATSCOUNTER_INC(b->ctrNoMetric, b->mutCtrNoMetric);
		FINALIZE;
	}

	if (pthread_rwlock_tryrdlock(&b->lock) == 0) {
		ctr = (dynstats_ctr_t *) hashtable_search(b->table, metric);
		if (ctr != NULL) {
			STATSCOUNTER_INC(ctr->ctr, ctr->mutCtr);
		}
		pthread_rwlock_unlock(&b->lock);
	} else {
		ABORT_FINALIZE(RS_RET_NOENTRY);
	}

	if (ctr == NULL) {
		CHKiRet(dynstats_addNewCtr(b, metric, 1, 0));
	}

	time_t now;
	datetime.GetTime(&now);
	if (dynstats_shouldPersist(b, now)) {
		pthread_rwlock_rdlock(&b->lock);
		persistBucketState(b, 1);
		if (b->persist_state_write_count_interval) {
			b->n_updates = 0;
		}
		if (b->persist_state_interval_secs) {
			b->persist_expiration_time = now + b->persist_state_interval_secs;
		}
		pthread_rwlock_unlock(&b->lock);
	}

finalize_it:
	if (iRet != RS_RET_OK) {
		if (iRet == RS_RET_NOENTRY) {
			/* NOTE: this is not tested (because it requires very strong orchestration to
			guarantee contended lock for testing) */
			STATSCOUNTER_INC(b->ctrOpsIgnored, b->mutCtrOpsIgnored);
		} else {
			STATSCOUNTER_INC(b->ctrOpsOverflow, b->mutCtrOpsOverflow);
		}
	}
	RETiRet;
}

typedef struct file_write_entry_s {
	STAILQ_ENTRY(file_write_entry_s) link;
	const char* name;
	const char* content;
	dynstats_bucket_t *bucket;
} file_write_entry_t;


static void
processWorkItem(file_write_entry_t *task) {
	// write out the file
	DBGPRINTF("dynstats: worker processing - '%s', '%.100s'\n", task->name, task->content);
	writeStateFile(task->bucket, task->name, task->content);
}

static void
destroyFileWriteTask(file_write_entry_t *task)
{
	free((void *)task->name);
	free((void *)task->content);
	free(task);
}

static void*
fileWriteWorker(void* data) {
	dynstats_buckets_t *bkts = (dynstats_buckets_t *)data;
	if (!bkts) {
		return NULL;
	}

	pthread_mutex_lock(&bkts->work_q.mut);
	++bkts->wrkrRunning;
	pthread_mutex_unlock(&bkts->work_q.mut);

	file_write_entry_t *task;
	while(1) {
		task = NULL;
		pthread_mutex_lock(&bkts->work_q.mut);
		if (bkts->work_q.size == 0) {
			--bkts->wrkrRunning;
			if (bkts->wkrTermState == 1) {
				pthread_mutex_unlock(&bkts->work_q.mut);
				break;
			} else {
				DBGPRINTF("dynstats: worker %llu waiting on new work items\n",
					(unsigned long long) bkts->wrkrInfo.tid);
				pthread_cond_wait(&bkts->work_q.wakeup_worker, &bkts->work_q.mut);
				DBGPRINTF("dynstats: worker %llu awoken\n", (unsigned long long) bkts->wrkrInfo.tid);
			}
			++bkts->wrkrRunning;
		}
		if (bkts->work_q.size > 0) {
			task = STAILQ_FIRST(&bkts->work_q.q);
			STAILQ_REMOVE_HEAD(&bkts->work_q.q, link);
			bkts->work_q.size--;
		}
		pthread_mutex_unlock(&bkts->work_q.mut);

		if (task != NULL) {
			processWorkItem(task);
			destroyFileWriteTask(task);
		}
	}
	return NULL;
}

static rsRetVal
initFileWriteWorker(dynstats_buckets_t *bkts) {
	DEFiRet;
	CHKiConcCtrl(pthread_mutex_init(&bkts->work_q.mut, NULL));
	CHKiConcCtrl(pthread_cond_init(&bkts->work_q.wakeup_worker, NULL));
	STAILQ_INIT(&bkts->work_q.q);
	bkts->work_q.size = 0;
	bkts->work_q.ctrMaxSz = 0;
	CHKiRet(statsobj.Construct(&bkts->work_q.stats));
	CHKiRet(statsobj.SetName(bkts->work_q.stats, (uchar*) "file-write-worker"));
	CHKiRet(statsobj.SetOrigin(bkts->work_q.stats, (uchar*) "dynstats"));
	STATSCOUNTER_INIT(bkts->work_q.ctrEnq, bkts->work_q.mutCtrEnq);
	CHKiRet(statsobj.AddCounter(bkts->work_q.stats, UCHAR_CONSTANT("enqueued"),
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &bkts->work_q.ctrEnq));
	CHKiRet(statsobj.AddCounter(bkts->work_q.stats, UCHAR_CONSTANT("maxqsize"),
		ctrType_Int, CTR_FLAG_NONE, &bkts->work_q.ctrMaxSz));
	CHKiRet(statsobj.ConstructFinalize(bkts->work_q.stats));
finalize_it:
	RETiRet;
}

static void ATTR_NONNULL(1)
destroyFileWriteWorker(dynstats_buckets_t *bkts) {
	file_write_entry_t *task;
	if (bkts->work_q.stats != NULL) {
		statsobj.Destruct(&bkts->work_q.stats);
	}
	pthread_mutex_lock(&bkts->work_q.mut);
	while (!STAILQ_EMPTY(&bkts->work_q.q)) {
		task = STAILQ_FIRST(&bkts->work_q.q);
		STAILQ_REMOVE_HEAD(&bkts->work_q.q, link);
		LogError(0, RS_RET_INTERNAL_ERROR, "dynstats: discarded enqueued io-work to allow shutdown "
								"- ignored");
		destroyFileWriteTask(task);
	}
	bkts->work_q.size = 0;
	pthread_mutex_unlock(&bkts->work_q.mut);
	pthread_cond_destroy(&bkts->work_q.wakeup_worker);
	pthread_mutex_destroy(&bkts->work_q.mut);
}

static rsRetVal ATTR_NONNULL(1)
enqueueFileWriteTask(dynstats_bucket_t *b, const char* file_name, const char* content) {
	file_write_entry_t *task;
	dynstats_buckets_t *bkts = &loadConf->dynstats_buckets;
	DEFiRet;
	if (!bkts) {
		FINALIZE
	}

	CHKmalloc(task = malloc(sizeof(file_write_entry_t)));
	task->bucket = b;
	task->name = strdup(file_name);
	task->content = strdup(content);

	pthread_mutex_lock(&bkts->work_q.mut);
	STAILQ_INSERT_TAIL(&bkts->work_q.q, task, link);
	bkts->work_q.size++;
	STATSCOUNTER_INC(bkts->work_q.ctrEnq, bkts->work_q.mutCtrEnq);
	STATSCOUNTER_SETMAX_NOMUT(bkts->work_q.ctrMaxSz, bkts->work_q.size);
	if (bkts->work_q.size == 1) {
		pthread_cond_signal(&bkts->work_q.wakeup_worker);
	}
	pthread_mutex_unlock(&bkts->work_q.mut);

finalize_it:
	if (iRet != RS_RET_OK) {
		if (task == NULL) {
			LogError(0, iRet, "dynstats: couldn't allocate memory to enqueue io-request - ignored");
		}
	}
	RETiRet;
}

static void
startFileWriteWorker(dynstats_buckets_t *bkts) {
	pthread_mutex_lock(&bkts->work_q.mut); /* locking to keep Coverity happy */
	bkts->wrkrRunning = 0;
	bkts->wkrTermState = 0;
	pthread_mutex_unlock(&bkts->work_q.mut);
	pthread_create(&bkts->wrkrInfo.tid, NULL, fileWriteWorker, bkts);
	assert(bkts->wrkrInfo.tid);
	DBGPRINTF("dynstats: worker started tid: %llu workers\n", (unsigned long long)bkts->wrkrInfo.tid);
}

static void ATTR_NONNULL(1)
stopFileWriteWorker(dynstats_buckets_t *bkts) {
	DBGPRINTF("dynstats: stoping worker pool\n");
	pthread_mutex_lock(&bkts->work_q.mut);
	bkts->wkrTermState = 1;
	pthread_cond_broadcast(&bkts->work_q.wakeup_worker); /* awake wrkr if not running */
	pthread_mutex_unlock(&bkts->work_q.mut);
	pthread_join(bkts->wrkrInfo.tid, NULL);
	DBGPRINTF("dynstats: info: worker stopped\n");
}
