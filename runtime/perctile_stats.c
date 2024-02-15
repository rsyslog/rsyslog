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

#include "unicode-helper.h"
#include "rsyslog.h"
#include "rsconf.h"
#include "errmsg.h"
#include "perctile_stats.h"
#include "hashtable_itr.h"
#include "perctile_ringbuf.h"
#include "datetime.h"

#include <stdio.h>
#include <pthread.h>
#include <math.h>

/* Use this macro to enable debug for this module */
#ifdef PERCTILE_STATS_DEBUG
#define _DEBUG 1
#else
#define _DEBUG 0
#endif
#define PERCTILE_STATS_LOG(...) do { if(_DEBUG) fprintf(stderr, __VA_ARGS__); } while(0)

/* definitions for objects we access */
DEFobjStaticHelpers
DEFobjCurrIf(statsobj)
DEFobjCurrIf(datetime)

#define PERCTILE_CONF_PARAM_NAME        "name"
#define PERCTILE_CONF_PARAM_PERCENTILES "percentiles"
#define PERCTILE_CONF_PARAM_WINDOW_SIZE "windowsize"
#define PERCTILE_CONF_PARAM_DELIM       "delimiter"

#define PERCTILE_MAX_BUCKET_NS_METRIC_LENGTH  128
#define PERCTILE_METRIC_NAME_SEPARATOR        '.'

static struct cnfparamdescr modpdescr[] = {
	{ PERCTILE_CONF_PARAM_NAME, eCmdHdlrString, CNFPARAM_REQUIRED },
	{ PERCTILE_CONF_PARAM_DELIM, eCmdHdlrString, 0},
	{ PERCTILE_CONF_PARAM_PERCENTILES, eCmdHdlrArray, 0},
	{ PERCTILE_CONF_PARAM_WINDOW_SIZE, eCmdHdlrPositiveInt, 0},
};

static struct cnfparamblk modpblk = {
	CNFPARAMBLK_VERSION,
	sizeof(modpdescr)/sizeof(struct cnfparamdescr),
	modpdescr
};

rsRetVal
perctileClassInit(void) {
	DEFiRet;
	CHKiRet(objGetObjInterface(&obj));
	CHKiRet(objUse(statsobj, CORE_COMPONENT));
	CHKiRet(objUse(datetime, CORE_COMPONENT));
finalize_it:
	RETiRet;
}

static uint64_t min(uint64_t a, uint64_t b) {
	return a < b ? a : b;
}

static uint64_t max(uint64_t a, uint64_t b) {
	return a > b ? a : b;
}

static void perctileStatDestruct(perctile_bucket_t *b, perctile_stat_t *pstat) {
	if (pstat) {
		if (pstat->rb_observed_stats) {
			ringbuf_del(pstat->rb_observed_stats);
		}

		if (pstat->ctrs) {
			for (size_t i = 0; i < pstat->perctile_ctrs_count; ++i) {
				perctile_ctr_t *ctr = &pstat->ctrs[i];
				if (ctr->ref_ctr_percentile_stat) {
					statsobj.DestructCounter(b->statsobj, ctr->ref_ctr_percentile_stat);
				}
			}
			free(pstat->ctrs);
		}

		if (pstat->refCtrWindowCount) {
			statsobj.DestructCounter(b->statsobj, pstat->refCtrWindowCount);
		}
		if (pstat->refCtrWindowMin) {
			statsobj.DestructCounter(b->statsobj, pstat->refCtrWindowMin);
		}
		if (pstat->refCtrWindowMax) {
			statsobj.DestructCounter(b->statsobj, pstat->refCtrWindowMax);
		}
		if (pstat->refCtrWindowSum) {
			statsobj.DestructCounter(b->statsobj, pstat->refCtrWindowSum);
		}
		pthread_rwlock_destroy(&pstat->stats_lock);
		free(pstat);
	}
}

static void perctileBucketDestruct(perctile_bucket_t *bkt) {
	PERCTILE_STATS_LOG("destructing perctile bucket\n");
	if (bkt) {
		pthread_rwlock_wrlock(&bkt->lock);
		// Delete all items in hashtable
		size_t count = hashtable_count(bkt->htable);
		if (count) {
			int ret = 0;
			struct hashtable_itr *itr = hashtable_iterator(bkt->htable);
			dbgprintf("%s() - All container instances, count=%zu...\n", __FUNCTION__, count);
			do {
				perctile_stat_t *pstat = hashtable_iterator_value(itr);
				perctileStatDestruct(bkt, pstat);
				ret = hashtable_iterator_advance(itr);
			} while (ret);
			free (itr);
			dbgprintf("End of container instances.\n");
		}
		hashtable_destroy(bkt->htable, 0);
		statsobj.Destruct(&bkt->statsobj);
		pthread_rwlock_unlock(&bkt->lock);
		pthread_rwlock_destroy(&bkt->lock);
		free(bkt->perctile_values);
		free(bkt->delim);
		free(bkt->name);
		free(bkt);
	}
}

void perctileBucketsDestruct(void) {
	perctile_buckets_t *bkts = &runConf->perctile_buckets;

	if (bkts->initialized) {
		perctile_bucket_t *head = bkts->listBuckets;
		if (head) {
			pthread_rwlock_wrlock(&bkts->lock);
			perctile_bucket_t *pnode = head, *pnext = NULL;
			while (pnode) {
				pnext = pnode->next;
				perctileBucketDestruct(pnode);
				pnode = pnext;
			}
			pthread_rwlock_unlock(&bkts->lock);
		}
		statsobj.Destruct(&bkts->global_stats);
		// destroy any global stats we keep specifically for this.
		pthread_rwlock_destroy(&bkts->lock);
	}
}

static perctile_bucket_t*
findBucket(perctile_bucket_t *head, const uchar *name) {
	perctile_bucket_t *pbkt_found = NULL;
	// walk the linked list until the name is found
	pthread_rwlock_rdlock(&head->lock);
	for (perctile_bucket_t *pnode = head; pnode != NULL; pnode = pnode->next) {
		if (ustrcmp(name, pnode->name) == 0) {
			// found.
			pbkt_found = pnode;
		}
	}
	pthread_rwlock_unlock(&head->lock);
	return pbkt_found;
}

#ifdef PERCTILE_STATS_DEBUG
static void
print_perctiles(perctile_bucket_t *bkt) {
	if (hashtable_count(bkt->htable)) {
		struct hashtable_itr *itr = hashtable_iterator(bkt->htable);
		do {
			uchar* key = hashtable_iterator_key(itr);
			perctile_stat_t *perc_stat = hashtable_iterator_value(itr);
			PERCTILE_STATS_LOG("print_perctile() - key: %s, perctile stat name: %s ", key, perc_stat->name);
		} while (hashtable_iterator_advance(itr));
		PERCTILE_STATS_LOG("\n");
	}
}
#endif

// Assumes a fully created pstat and bkt, also initiliazes some values in pstat.
static rsRetVal
initAndAddPerctileMetrics(perctile_stat_t *pstat, perctile_bucket_t *bkt, uchar* key) {
	char stat_name[128];
	int bytes = 0;
	int stat_name_len = sizeof(stat_name);
	DEFiRet;

	bytes = snprintf((char*)pstat->name, sizeof(pstat->name), "%s", key);
	if (bytes < 0 || bytes >= (int) sizeof(pstat->name)) {
		LogError(0, iRet, "statname '%s' truncated - too long for buffer size: %d\n", key, bytes);
		ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
	}

	int offset = snprintf(stat_name, stat_name_len, "%s%s", (char*)pstat->name, (char*)bkt->delim);
	if (offset < 0 || offset >= stat_name_len) {
		LogError(0, iRet, "statname '%s' delim '%s' truncated - too long for buffer size: %d\n",
			(char*)pstat->name, (char*)bkt->delim, offset);
		ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
	}

	int remaining_size = stat_name_len - offset - 1;
	// initialize the counters array
	for (size_t i = 0; i < pstat->perctile_ctrs_count; ++i) {
		perctile_ctr_t *ctr = &pstat->ctrs[i];

		// bucket contains the supported percentile values.
		ctr->percentile = bkt->perctile_values[i];
		bytes = snprintf(stat_name+offset, remaining_size, "p%d", bkt->perctile_values[i]);
		if (bytes < 0 || bytes >= remaining_size) {
			LogError(0, iRet, "statname '%s' truncated - too long for buffer size: %d\n",
				stat_name, stat_name_len);
			ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
		}
		CHKiRet(statsobj.AddManagedCounter(bkt->statsobj, (uchar *)stat_name, ctrType_IntCtr,
			CTR_FLAG_NONE, &ctr->ctr_perctile_stat, &ctr->ref_ctr_percentile_stat, 1));
	}

	bytes = snprintf(stat_name+offset, remaining_size, "window_min");
	if (bytes < 0 || bytes >= remaining_size) {
		LogError(0, iRet, "statname '%s' truncated - too long for buffer size: %d\n", stat_name, stat_name_len);
		ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
	}
	CHKiRet(statsobj.AddManagedCounter(bkt->statsobj, (uchar *)stat_name, ctrType_IntCtr,
		CTR_FLAG_NONE, &pstat->ctrWindowMin, &pstat->refCtrWindowMin, 1));

	bytes = snprintf(stat_name+offset, remaining_size, "window_max");
	if (bytes < 0 || bytes >= remaining_size) {
		LogError(0, iRet, "statname '%s' truncated - too long for buffer size: %d\n", stat_name, stat_name_len);
		ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
	}
	CHKiRet(statsobj.AddManagedCounter(bkt->statsobj, (uchar *)stat_name, ctrType_IntCtr,
		CTR_FLAG_NONE, &pstat->ctrWindowMax, &pstat->refCtrWindowMax, 1));

	bytes = snprintf(stat_name+offset, remaining_size, "window_sum");
	if (bytes < 0 || bytes >= remaining_size) {
		LogError(0, iRet, "statname '%s' truncated - too long for buffer size: %d\n", stat_name, stat_name_len);
		ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
	}
	CHKiRet(statsobj.AddManagedCounter(bkt->statsobj, (uchar *)stat_name, ctrType_IntCtr,
		CTR_FLAG_NONE, &pstat->ctrWindowSum, &pstat->refCtrWindowSum, 1));

	bytes = snprintf(stat_name+offset, remaining_size, "window_count");
	if (bytes < 0 || bytes >= remaining_size) {
		LogError(0, iRet, "statname '%s' truncated - too long for buffer size: %d\n", stat_name, stat_name_len);
		ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
	}
	CHKiRet(statsobj.AddManagedCounter(bkt->statsobj, (uchar *)stat_name, ctrType_IntCtr,
		CTR_FLAG_NONE, &pstat->ctrWindowCount, &pstat->refCtrWindowCount, 1));

finalize_it:
	if (iRet != RS_RET_OK) {
		LogError(0, iRet, "Could not initialize percentile stats.");
	}
	RETiRet;
}

static rsRetVal
perctile_observe(perctile_bucket_t *bkt, uchar* key, int64_t value) {
	uint8_t lock_initialized = 0;
	uchar* hash_key = NULL;
	DEFiRet;
	time_t now;
	datetime.GetTime(&now);

	pthread_rwlock_wrlock(&bkt->lock);
	lock_initialized = 1;
	perctile_stat_t *pstat = (perctile_stat_t*) hashtable_search(bkt->htable, key);
	if (!pstat) {
		PERCTILE_STATS_LOG("perctile_observe(): key '%s' not found - creating new pstat", key);
		// create the pstat if not found
		CHKmalloc(pstat = calloc(1, sizeof(perctile_stat_t)));
		pstat->ctrs = calloc(bkt->perctile_values_count, sizeof(perctile_ctr_t));
		if (!pstat->ctrs) {
			free(pstat);
			ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
		}
		pstat->perctile_ctrs_count = bkt->perctile_values_count;
		pstat->rb_observed_stats = ringbuf_new(bkt->window_size);
		if (!pstat->rb_observed_stats) {
			free(pstat->ctrs);
			free(pstat);
			ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
		}
		pstat->bReported = 0;
		pthread_rwlock_init(&pstat->stats_lock, NULL);

		// init all stat counters here
		pthread_rwlock_wrlock(&pstat->stats_lock);
		pstat->ctrWindowCount = pstat->ctrWindowMax = pstat->ctrWindowSum = 0;
		pstat->ctrWindowMin = value;
		pthread_rwlock_unlock(&pstat->stats_lock);

		iRet = initAndAddPerctileMetrics(pstat, bkt, key);
		if (iRet != RS_RET_OK) {
			perctileStatDestruct(bkt, pstat);
			ABORT_FINALIZE(iRet);
		}

		CHKmalloc(hash_key = ustrdup(key));
		if (!hashtable_insert(bkt->htable, hash_key, pstat)) {
			perctileStatDestruct(bkt, pstat);
			free(hash_key);
			ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
		}
		PERCTILE_STATS_LOG("perctile_observe - new pstat created - name: %s\n", pstat->name);
		STATSCOUNTER_INC(bkt->ctrNewKeyAdd, bkt->mutCtrNewKeyAdd);
	}

	// add this value into the ringbuffer
	assert(pstat->rb_observed_stats);
	if (ringbuf_append_with_overwrite(pstat->rb_observed_stats, value) != 0) {
		// ringbuffer is operating in overwrite mode, so should never see this.
		ABORT_FINALIZE(RS_RET_ERR);
	}

	// update perctile specific stats
	pthread_rwlock_wrlock(&pstat->stats_lock);
	{
		if (pstat->bReported) {
			// reset window values
			pstat->ctrWindowCount = pstat->ctrWindowSum = 0;
			pstat->ctrWindowMin = pstat->ctrWindowMax = value;
			pstat->bReported = 0;
		}
		++(pstat->ctrWindowCount);
		pstat->ctrWindowSum += value;
		pstat->ctrWindowMin = min(pstat->ctrWindowMin, value);
		pstat->ctrWindowMax = max(pstat->ctrWindowMax, value);
	}
	pthread_rwlock_unlock(&pstat->stats_lock);

#ifdef PERCTILE_STATS_DEBUG
	PERCTILE_STATS_LOG("perctile_observe - appended value: %lld to ringbuffer\n", value);
	PERCTILE_STATS_LOG("ringbuffer contents... \n");
	for (size_t i = 0; i < pstat->rb_observed_stats->size; ++i) {
		PERCTILE_STATS_LOG("%lld ", pstat->rb_observed_stats->cb.buf[i]);
	}
	PERCTILE_STATS_LOG("\n");
	print_perctiles(bkt);
#endif
finalize_it:
	if (lock_initialized) {
		pthread_rwlock_unlock(&bkt->lock);
	}
	if (iRet != RS_RET_OK) {
		// clean up if there was an error
		if (iRet == RS_RET_OUT_OF_MEMORY) {
			STATSCOUNTER_INC(bkt->ctrOpsOverflow, bkt->mutCtrOpsOverflow);
		}
	}
	RETiRet;
}

static int cmp(const void* p1, const void* p2) {
	return (*(ITEM*)p1) - (*(ITEM*)p2);
}

static rsRetVal report_perctile_stats(perctile_bucket_t* pbkt) {
	ITEM *buf = NULL;
	struct hashtable_itr *itr = NULL;
	DEFiRet;

	pthread_rwlock_rdlock(&pbkt->lock);
	if (hashtable_count(pbkt->htable)) {
		itr = hashtable_iterator(pbkt->htable);
		CHKmalloc(buf = malloc(pbkt->window_size*sizeof(ITEM)));
		do {
			memset(buf, 0, pbkt->window_size*sizeof(ITEM));
			perctile_stat_t *perc_stat = hashtable_iterator_value(itr);
			// ringbuffer read
			size_t count = ringbuf_read_to_end(perc_stat->rb_observed_stats, buf, pbkt->window_size);
			if (!count) {
				continue;
			}
			PERCTILE_STATS_LOG("read %zu values\n", count);
			// calculate the p95 based on the
#ifdef PERCTILE_STATS_DEBUG
			PERCTILE_STATS_LOG("ringbuffer contents... \n");
			for (size_t i = 0; i < perc_stat->rb_observed_stats->size; ++i) {
				PERCTILE_STATS_LOG("%lld ", perc_stat->rb_observed_stats->cb.buf[i]);
			}
			PERCTILE_STATS_LOG("\n");

			PERCTILE_STATS_LOG("buffer contents... \n");
			for (size_t i = 0; i < perc_stat->rb_observed_stats->size; ++i) {
				PERCTILE_STATS_LOG("%lld ", buf[i]);
			}
			PERCTILE_STATS_LOG("\n");
#endif
			qsort(buf, count, sizeof(ITEM), cmp);
#ifdef PERCTILE_STATS_DEBUG
			PERCTILE_STATS_LOG("buffer contents after sort... \n");
			for (size_t i = 0; i < perc_stat->rb_observed_stats->size; ++i) {
				PERCTILE_STATS_LOG("%lld ", buf[i]);
			}
			PERCTILE_STATS_LOG("\n");
#endif
			PERCTILE_STATS_LOG("report_perctile_stats() - perctile stat has %zu counters.",
				perc_stat->perctile_ctrs_count);
			for (size_t i = 0; i < perc_stat->perctile_ctrs_count; ++i) {
				perctile_ctr_t *pctr = &perc_stat->ctrs[i];
				// get percentile - this can be cached.
				int index = max(0, ((pctr->percentile/100.0) * count)-1);
				// look into if we need to lock this.
				pctr->ctr_perctile_stat = buf[index];
				PERCTILE_STATS_LOG("report_perctile_stats() - index: %d, perctile stat [%s, %d, %llu]",
					index, perc_stat->name, pctr->percentile, pctr->ctr_perctile_stat);
			}
			perc_stat->bReported = 1;
		} while (hashtable_iterator_advance(itr));
	}

finalize_it:
	pthread_rwlock_unlock(&pbkt->lock);
	free(itr);
	free(buf);
	RETiRet;
}

static void
perctile_readCallback(statsobj_t __attribute__((unused)) *ignore, void __attribute__((unused)) *b) {
	perctile_buckets_t *bkts = &runConf->perctile_buckets;

	pthread_rwlock_rdlock(&bkts->lock);
	for (perctile_bucket_t *pbkt = bkts->listBuckets; pbkt != NULL; pbkt = pbkt->next) {
		report_perctile_stats(pbkt);
	}
	pthread_rwlock_unlock(&bkts->lock);
}

static rsRetVal
perctileInitNewBucketStats(perctile_bucket_t *b) {
	DEFiRet;

	CHKiRet(statsobj.Construct(&b->statsobj));
	CHKiRet(statsobj.SetOrigin(b->statsobj, UCHAR_CONSTANT("percentile.bucket")));
	CHKiRet(statsobj.SetName(b->statsobj, b->name));
	CHKiRet(statsobj.SetReportingNamespace(b->statsobj, UCHAR_CONSTANT("values")));
	statsobj.SetReadNotifier(b->statsobj, perctile_readCallback, b);
	CHKiRet(statsobj.ConstructFinalize(b->statsobj));

finalize_it:
	RETiRet;
}

static rsRetVal
perctileAddBucketMetrics(perctile_buckets_t *bkts, perctile_bucket_t *b, const uchar* name) {
	uchar *metric_name_buff, *metric_suffix;
	const uchar *suffix_litteral;
	int name_len;
	DEFiRet;

	name_len = ustrlen(name);
	CHKmalloc(metric_name_buff = malloc((name_len + PERCTILE_MAX_BUCKET_NS_METRIC_LENGTH + 1) * sizeof(uchar)));

	strcpy((char*)metric_name_buff, (char*)name);
	metric_suffix = metric_name_buff + name_len;
	*metric_suffix = PERCTILE_METRIC_NAME_SEPARATOR;
	metric_suffix++;

	suffix_litteral = UCHAR_CONSTANT("new_metric_add");
	ustrncpy(metric_suffix, suffix_litteral, PERCTILE_MAX_BUCKET_NS_METRIC_LENGTH);
	STATSCOUNTER_INIT(b->ctrNewKeyAdd, b->mutCtrNewKeyAdd);
	CHKiRet(statsobj.AddManagedCounter(bkts->global_stats, metric_name_buff, ctrType_IntCtr,
				CTR_FLAG_RESETTABLE,
				&(b->ctrNewKeyAdd),
				&b->pNewKeyAddCtr, 1));

	suffix_litteral = UCHAR_CONSTANT("ops_overflow");
	ustrncpy(metric_suffix, suffix_litteral, PERCTILE_MAX_BUCKET_NS_METRIC_LENGTH);
	STATSCOUNTER_INIT(b->ctrOpsOverflow, b->mutCtrOpsOverflow);
	CHKiRet(statsobj.AddManagedCounter(bkts->global_stats, metric_name_buff, ctrType_IntCtr,
				CTR_FLAG_RESETTABLE,
				&(b->ctrOpsOverflow),
				&b->pOpsOverflowCtr, 1));

finalize_it:
	free(metric_name_buff);
	if (iRet != RS_RET_OK) {
		if (b->pOpsOverflowCtr != NULL) {
			statsobj.DestructCounter(bkts->global_stats, b->pOpsOverflowCtr);
		}
		if (b->pNewKeyAddCtr != NULL) {
			statsobj.DestructCounter(bkts->global_stats, b->pNewKeyAddCtr);
		}
	}
	RETiRet;
}

/* Create new perctile bucket, and add it to our list of perctile buckets.
*/
static rsRetVal
perctile_newBucket(const uchar *name, const uchar *delim,
	uint8_t *perctiles, uint32_t perctilesCount, uint32_t windowSize) {
	perctile_buckets_t *bkts;
	perctile_bucket_t* b = NULL;
	pthread_rwlockattr_t bucket_lock_attr;
	DEFiRet;

	bkts = &loadConf->perctile_buckets;

	if (bkts->initialized)
	{
		CHKmalloc(b = calloc(1, sizeof(perctile_bucket_t)));

		// initialize
		pthread_rwlockattr_init(&bucket_lock_attr);
		pthread_rwlock_init(&b->lock, &bucket_lock_attr);
		CHKmalloc(b->htable = create_hashtable(7, hash_from_string, key_equals_string, NULL));
		CHKmalloc(b->name = ustrdup(name));
		if (delim) {
			CHKmalloc(b->delim = ustrdup(delim));
		} else {
			CHKmalloc(b->delim = ustrdup("."));
		}

		CHKmalloc(b->perctile_values = calloc(perctilesCount, sizeof(uint8_t)));
		b->perctile_values_count = perctilesCount;
		memcpy(b->perctile_values, perctiles, perctilesCount * sizeof(uint8_t));
		b->window_size = windowSize;
		b->next = NULL;
		PERCTILE_STATS_LOG("perctile_newBucket: create new bucket for %s,"
			"with windowsize: %d,  values_count: %zu\n",
			b->name, b->window_size, b->perctile_values_count);

		// add bucket to list of buckets
		if (!bkts->listBuckets)
		{
			// none yet
			bkts->listBuckets = b;
			PERCTILE_STATS_LOG("perctile_newBucket: Adding new bucket to empty list \n");
		}
		else
		{
			b->next = bkts->listBuckets;
			bkts->listBuckets = b;
			PERCTILE_STATS_LOG("perctile_newBucket: prepended new bucket list \n");
		}

		// create the statsobj for this bucket
		CHKiRet(perctileInitNewBucketStats(b));
		CHKiRet(perctileAddBucketMetrics(bkts, b, name));
	}
	else
	{
		LogError(0, RS_RET_INTERNAL_ERROR, "perctile: bucket creation failed, as "
				"global-initialization of buckets was unsuccessful");
		ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
	}
finalize_it:
	if (iRet != RS_RET_OK)
	{
		if (b != NULL) {
			perctileBucketDestruct(b);
		}
	}
	RETiRet;
}

// Public functions
rsRetVal
perctile_processCnf(struct cnfobj *o) {
	struct cnfparamvals *pvals;
	uchar *name = NULL;
	uchar *delim = NULL;
	uint8_t *perctiles = NULL;
	uint32_t perctilesCount = 0;
	uint64_t windowSize = 0;
	DEFiRet;

	pvals = nvlstGetParams(o->nvlst, &modpblk, NULL);
	if(pvals == NULL) {
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	for(short i = 0 ; i < modpblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(modpblk.descr[i].name, PERCTILE_CONF_PARAM_NAME)) {
			CHKmalloc(name = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL));
		} else if(!strcmp(modpblk.descr[i].name, PERCTILE_CONF_PARAM_DELIM)) {
			CHKmalloc(delim = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL));
		} else if (!strcmp(modpblk.descr[i].name, PERCTILE_CONF_PARAM_PERCENTILES)) {
			/* Only the first instance of this parameter will be accepted */
			if (!perctiles) {
				perctilesCount = pvals[i].val.d.ar->nmemb;
				if (perctilesCount) {
					CHKmalloc(perctiles = calloc(perctilesCount, sizeof(uint8_t)));
					for (int j = 0; j < pvals[i].val.d.ar->nmemb; ++j) {
						char *cstr = es_str2cstr(pvals[i].val.d.ar->arr[j], NULL);
						perctiles[j] = atoi(cstr);
						free(cstr);
					}
				}
			}
		} else if (!strcmp(modpblk.descr[i].name, PERCTILE_CONF_PARAM_WINDOW_SIZE)) {
			windowSize = pvals[i].val.d.n;
		} else {
			dbgprintf("perctile: program error, non-handled "
					"param '%s'\n", modpblk.descr[i].name);
		}
	}

	if (name != NULL && perctiles != NULL) {
		CHKiRet(perctile_newBucket(name, delim, perctiles, perctilesCount, windowSize));
	}

finalize_it:
	free(name);
	free(delim);
	free(perctiles);
	cnfparamvalsDestruct(pvals, &modpblk);
	RETiRet;
}

rsRetVal
perctile_initCnf(perctile_buckets_t *bkts) {
	DEFiRet;

	bkts->initialized = 0;
	bkts->listBuckets = NULL;
	CHKiRet(statsobj.Construct(&bkts->global_stats));
	CHKiRet(statsobj.SetOrigin(bkts->global_stats, UCHAR_CONSTANT("percentile")));
	CHKiRet(statsobj.SetName(bkts->global_stats, UCHAR_CONSTANT("global")));
	CHKiRet(statsobj.SetReportingNamespace(bkts->global_stats, UCHAR_CONSTANT("values")));
	CHKiRet(statsobj.ConstructFinalize(bkts->global_stats));
	pthread_rwlock_init(&bkts->lock, NULL);
	bkts->initialized = 1;

finalize_it:
	if (iRet != RS_RET_OK) {
		statsobj.Destruct(&bkts->global_stats);
	}
	RETiRet;
}

perctile_bucket_t*
perctile_findBucket(const uchar* name) {
	perctile_bucket_t *b = NULL;

	perctile_buckets_t *bkts = &loadConf->perctile_buckets;
	if (bkts->initialized) {
		pthread_rwlock_rdlock(&bkts->lock);
		if (bkts->listBuckets) {
			b = findBucket(bkts->listBuckets, name);
		}
		pthread_rwlock_unlock(&bkts->lock);
	} else {
		LogError(0, RS_RET_INTERNAL_ERROR, "perctile: bucket lookup failed, as global-initialization "
				"of buckets was unsuccessful");
	}
	return b;
}

rsRetVal
perctile_obs(perctile_bucket_t *perctile_bkt, uchar* key, int64_t value) {
	DEFiRet;
	if (!perctile_bkt) {
		LogError(0, RS_RET_INTERNAL_ERROR, "perctile() - perctile bkt not available");
		FINALIZE;
	}
	PERCTILE_STATS_LOG("perctile_obs() - bucket name: %s, key: %s, val: %" PRId64 "\n",
		perctile_bkt->name, key, value);

	CHKiRet(perctile_observe(perctile_bkt, key, value));

finalize_it:
	if (iRet != RS_RET_OK) {
		LogError(0, RS_RET_INTERNAL_ERROR, "perctile_obs(): name: %s, key: %s, val: %" PRId64 "\n",
			perctile_bkt->name, key, value);
	}
	RETiRet;
}
