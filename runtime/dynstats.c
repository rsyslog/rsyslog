#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <json.h>
#include <assert.h>

#include "rsyslog.h"
#include "srUtils.h"
#include "errmsg.h"
#include "lookup.h"
#include "msg.h"
#include "rsconf.h"
#include "dirty.h"
#include "unicode-helper.h"

/* definitions for objects we access */
DEFobjStaticHelpers
DEFobjCurrIf(errmsg)
DEFobjCurrIf(statsobj)

#define DYNSTATS_PARAM_NAME "name"
#define DYNSTATS_PARAM_RESETTABLE "resettable"
#define DYNSTATS_PARAM_MAX_CARDINALITY "maxCardinality"
#define DYNSTATS_PARAM_UNUSED_METRIC_LIFE "unusedMetricLife" /* in seconds */

#define DYNSTATS_DEFAULT_RESETTABILITY 1
#define DYNSTATS_DEFAULT_MAX_CARDINALITY 2000
#define DYNSTATS_DEFAULT_UNUSED_METRIC_LIFE 3600 /* seconds */

#define DYNSTATS_MAX_BUCKET_NS_METRIC_LENGTH 100
#define DYNSTATS_METRIC_NAME_SEPARATOR ':'
#define DYNSTATS_HASHTABLE_SIZE_OVERPROVISIONING 1.25

static struct cnfparamdescr modpdescr[] = {
	{ DYNSTATS_PARAM_NAME, eCmdHdlrString, CNFPARAM_REQUIRED },
	{ DYNSTATS_PARAM_RESETTABLE, eCmdHdlrBinary, 0 },
	{ DYNSTATS_PARAM_MAX_CARDINALITY, eCmdHdlrPositiveInt, 0},
	{ DYNSTATS_PARAM_UNUSED_METRIC_LIFE, eCmdHdlrPositiveInt, 0} /* in minutes */
};

static struct cnfparamblk modpblk =
{ CNFPARAMBLK_VERSION,
  sizeof(modpdescr)/sizeof(struct cnfparamdescr),
  modpdescr
};

rsRetVal
dynstatsClassInit(void) {
	DEFiRet;
	CHKiRet(objGetObjInterface(&obj));
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(statsobj, CORE_COMPONENT));
finalize_it:
	RETiRet;
}

static inline void
dynstats_destroyCtr(dynstats_bucket_t *b, dynstats_ctr_t *ctr, uint8_t destructStatsCtr) {
	if (destructStatsCtr) {
		statsobj.DestructCounter(b->stats, ctr->pCtr);
	}
	free(ctr->metric);
	free(ctr);
}

static inline void /* assumes exclusive access to bucket */
dynstats_destroyCounters(dynstats_bucket_t *b) {
	dynstats_ctr_t *ctr;

	hdestroy_r(&b->table);
	statsobj.DestructAllCounters(b->stats);
	while(1) {
		ctr = SLIST_FIRST(&b->ctrs);
		if (ctr == NULL) {
			break;
		} else {
			SLIST_REMOVE_HEAD(&b->ctrs, link);
			dynstats_destroyCtr(b, ctr, 0);
		}
	}
	STATSCOUNTER_BUMP(b->ctrMetricsPurged, b->mutCtrMetricsPurged, b->metricCount);
}

void
dynstats_destroyBucket(dynstats_bucket_t* b) {
	dynstats_buckets_t *bkts;

	bkts = &loadConf->dynstats_buckets;

	pthread_rwlock_wrlock(&b->lock);
	dynstats_destroyCounters(b);
	statsobj.Destruct(&b->stats);
	free(b->name);
	pthread_rwlock_unlock(&b->lock);
	pthread_rwlock_destroy(&b->lock);
	pthread_mutex_destroy(&b->mutMetricCount);
	statsobj.DestructCounter(bkts->global_stats, b->pOpsOverflowCtr);
	statsobj.DestructCounter(bkts->global_stats, b->pNewMetricAddCtr);
	statsobj.DestructCounter(bkts->global_stats, b->pNoMetricCtr);
	statsobj.DestructCounter(bkts->global_stats, b->pMetricsPurgedCtr);
	statsobj.DestructCounter(bkts->global_stats, b->pOpsIgnoredCtr);
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

	ustrncpy(metric_name_buff, name, name_len);
	metric_suffix = metric_name_buff + name_len;
	*metric_suffix = DYNSTATS_METRIC_NAME_SEPARATOR;
	metric_suffix++;

	suffix_litteral = UCHAR_CONSTANT("ops_overflow");
	ustrncpy(metric_suffix, suffix_litteral, DYNSTATS_MAX_BUCKET_NS_METRIC_LENGTH);
	STATSCOUNTER_INIT(b->ctrOpsOverflow, b->mutCtrOpsOverflow);
	CHKiRet(statsobj.AddManagedCounter(bkts->global_stats, metric_name_buff, ctrType_IntCtr,
									   CTR_FLAG_RESETTABLE, &(b->ctrOpsOverflow), &b->pOpsOverflowCtr));

	suffix_litteral = UCHAR_CONSTANT("new_metric_add");
	ustrncpy(metric_suffix, suffix_litteral, DYNSTATS_MAX_BUCKET_NS_METRIC_LENGTH);
	STATSCOUNTER_INIT(b->ctrNewMetricAdd, b->mutCtrNewMetricAdd);
	CHKiRet(statsobj.AddManagedCounter(bkts->global_stats, metric_name_buff, ctrType_IntCtr,
									   CTR_FLAG_RESETTABLE, &(b->ctrNewMetricAdd), &b->pNewMetricAddCtr));

	suffix_litteral = UCHAR_CONSTANT("no_metric");
	ustrncpy(metric_suffix, suffix_litteral, DYNSTATS_MAX_BUCKET_NS_METRIC_LENGTH);
	STATSCOUNTER_INIT(b->ctrNoMetric, b->mutCtrNoMetric);
	CHKiRet(statsobj.AddManagedCounter(bkts->global_stats, metric_name_buff, ctrType_IntCtr,
									   CTR_FLAG_RESETTABLE, &(b->ctrNoMetric), &b->pNoMetricCtr));

	suffix_litteral = UCHAR_CONSTANT("metrics_purged");
	ustrncpy(metric_suffix, suffix_litteral, DYNSTATS_MAX_BUCKET_NS_METRIC_LENGTH);
	STATSCOUNTER_INIT(b->ctrMetricsPurged, b->mutCtrMetricsPurged);
	CHKiRet(statsobj.AddManagedCounter(bkts->global_stats, metric_name_buff, ctrType_IntCtr,
									   CTR_FLAG_RESETTABLE, &(b->ctrMetricsPurged), &b->pMetricsPurgedCtr));

	suffix_litteral = UCHAR_CONSTANT("ops_ignored");
	ustrncpy(metric_suffix, suffix_litteral, DYNSTATS_MAX_BUCKET_NS_METRIC_LENGTH);
	STATSCOUNTER_INIT(b->ctrOpsIgnored, b->mutCtrOpsIgnored);
	CHKiRet(statsobj.AddManagedCounter(bkts->global_stats, metric_name_buff, ctrType_IntCtr,
									   CTR_FLAG_RESETTABLE, &(b->ctrOpsIgnored), &b->pOpsIgnoredCtr));

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
	}
	RETiRet;
}

static rsRetVal
dynstats_resetBucket(dynstats_bucket_t *b, uint8_t do_purge) {
	size_t htab_sz;
	DEFiRet;
	htab_sz = (size_t) (DYNSTATS_HASHTABLE_SIZE_OVERPROVISIONING * b->maxCardinality + 1);
	pthread_rwlock_wrlock(&b->lock);
	if (do_purge) {
		dynstats_destroyCounters(b);
	}
	ATOMIC_STORE_0_TO_INT(&b->metricCount, &b->mutMetricCount);
	SLIST_INIT(&b->ctrs);
	if (! hcreate_r(htab_sz, &b->table)) {
		errmsg.LogError(errno, RS_RET_INTERNAL_ERROR, "error trying to initialize hash-table for dyn-stats bucket named: %s", b->name);
		ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
	}
			
	timeoutComp(&b->metricCleanupTimeout, b->unusedMetricLife);
finalize_it:
	pthread_rwlock_unlock(&b->lock);
	if (iRet != RS_RET_OK) {
		statsobj.Destruct(&b->stats);
	}
	RETiRet;
}

static inline void
dynstats_resetIfExpired(dynstats_bucket_t *b) {
	long timeout;
	pthread_rwlock_rdlock(&b->lock);
	timeout = timeoutVal(&b->metricCleanupTimeout);
	pthread_rwlock_unlock(&b->lock);
	if (timeout == 0) {
		errmsg.LogMsg(0, RS_RET_TIMED_OUT, LOG_INFO, "dynstats: bucket '%s' is being reset", b->name);
		dynstats_resetBucket(b, 1);
	}
}

static void
dynstats_readCallback(statsobj_t *ignore, void *b) {
	dynstats_buckets_t *bkts;
	bkts = &loadConf->dynstats_buckets;

	pthread_rwlock_rdlock(&bkts->lock);
	dynstats_resetIfExpired((dynstats_bucket_t *) b);
	pthread_rwlock_unlock(&bkts->lock);
}

static inline rsRetVal
dynstats_initNewBucketStats(dynstats_bucket_t *b) {
	DEFiRet;
	
	CHKiRet(statsobj.Construct(&b->stats));
	CHKiRet(statsobj.SetOrigin(b->stats, UCHAR_CONSTANT("dynstats.bucket")));
	CHKiRet(statsobj.SetName(b->stats, b->name));
	statsobj.SetReadNotifier(b->stats, dynstats_readCallback, b);
	CHKiRet(statsobj.ConstructFinalize(b->stats));
	
finalize_it:
	RETiRet;
}

static rsRetVal
dynstats_newBucket(const uchar* name, uint8_t resettable, uint32_t maxCardinality, uint32_t unusedMetricLife) {
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
		CHKmalloc(b->name = ustrdup(name));

		pthread_rwlockattr_init(&bucket_lock_attr);
		pthread_rwlockattr_setkind_np(&bucket_lock_attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);

		pthread_rwlock_init(&b->lock, &bucket_lock_attr);
		lock_initialized = 1;
		pthread_mutex_init(&b->mutMetricCount, NULL);
		metric_count_mutex_initialized = 1;

		CHKiRet(dynstats_initNewBucketStats(b));

		CHKiRet(dynstats_resetBucket(b, 0));

		CHKiRet(dynstats_addBucketMetrics(bkts, b, name));

		pthread_rwlock_wrlock(&bkts->lock);
		SLIST_INSERT_HEAD(&bkts->list, b, link);
		pthread_rwlock_unlock(&bkts->lock);
	} else {
		errmsg.LogError(0, RS_RET_INTERNAL_ERROR, "dynstats: bucket creation failed, as global-initialization of buckets was unsuccessful");
		ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
	}
finalize_it:
	if (iRet != RS_RET_OK) {
		if (metric_count_mutex_initialized) {
			pthread_mutex_destroy(&b->mutMetricCount);
		}
		if (lock_initialized) {
			pthread_rwlock_destroy(&b->lock);
		}
		if (b != NULL) {
			free(b->name);
			free(b);
		}
	}
	RETiRet;
}

rsRetVal
dynstats_processCnf(struct cnfobj *o) {
	struct cnfparamvals *pvals;
	short i;
	uchar *name;
	uint8_t resettable = DYNSTATS_DEFAULT_RESETTABILITY;
	uint32_t maxCardinality = DYNSTATS_DEFAULT_MAX_CARDINALITY;
	uint32_t unusedMetricLife = DYNSTATS_DEFAULT_UNUSED_METRIC_LIFE;
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
		} else {
			dbgprintf("dyn_stats: program error, non-handled "
					  "param '%s'\n", modpblk.descr[i].name);
		}
	}
	CHKiRet(dynstats_newBucket(name, resettable, maxCardinality, unusedMetricLife));

finalize_it:
	free(name);
	cnfparamvalsDestruct(pvals, &modpblk);
	RETiRet;
}

rsRetVal
dynstats_initCnf(dynstats_buckets_t *bkts) {
	DEFiRet;

	bkts->initialized = 0;
	
	SLIST_INIT(&bkts->list);
	CHKiRet(statsobj.Construct(&bkts->global_stats));
	CHKiRet(statsobj.SetOrigin(bkts->global_stats, UCHAR_CONSTANT("dynstats")));
	CHKiRet(statsobj.SetName(bkts->global_stats, UCHAR_CONSTANT("global")));
	CHKiRet(statsobj.ConstructFinalize(bkts->global_stats));
	pthread_rwlock_init(&bkts->lock, NULL);

	bkts->initialized = 1;
	
finalize_it:
	if (iRet != RS_RET_OK) {
		statsobj.Destruct(&bkts->global_stats);
	}
	RETiRet;
}

void
dynstats_destroyAllBuckets() {
	dynstats_buckets_t *bkts;
	dynstats_bucket_t *b;
	bkts = &loadConf->dynstats_buckets;
	if (bkts->initialized) {
		pthread_rwlock_wrlock(&bkts->lock);
		while(1) {
			b = SLIST_FIRST(&bkts->list);
			if (b == NULL) {
				break;
			} else {
				SLIST_REMOVE_HEAD(&bkts->list, link);
				dynstats_destroyBucket(b);
			}
		}
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
		SLIST_FOREACH(b, &bkts->list, link) {
			if (! ustrcmp(name, b->name)) {
				break;
			}
		}
		pthread_rwlock_unlock(&bkts->lock);
	} else {
		b = NULL;
		errmsg.LogError(0, RS_RET_INTERNAL_ERROR, "dynstats: bucket lookup failed, as global-initialization of buckets was unsuccessful");
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
									   b->resettable, &(*ctr)->ctr, &(*ctr)->pCtr));
finalize_it:
	if (iRet != RS_RET_OK) {
		free((*ctr)->metric);
		free(*ctr);
		*ctr = NULL;
	}
	RETiRet;
}

static rsRetVal
dynstats_addNewCtr(dynstats_bucket_t *b, const uchar* metric, uint8_t doInitialIncrement) {
	dynstats_ctr_t *ctr;
	dynstats_ctr_t *found_ctr;
	ENTRY lookup, *entry;
	int found, created;
	DEFiRet;

	found = created = 0;
	ctr = NULL;

	if (ATOMIC_FETCH_32BIT(&b->metricCount, &b->mutMetricCount) >= b->maxCardinality) {
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}
	
	CHKiRet(dynstats_createCtr(b, metric, &ctr));
	lookup.data = ctr;
	lookup.key = ctr->metric;

	pthread_rwlock_wrlock(&b->lock);
	found = hsearch_r(lookup, FIND, &entry, &b->table);//TODO: see what happens on 2nd ENTER for same key, it may be simplifiable.
	if (found) {
		found_ctr = (dynstats_ctr_t*) entry->data;
		if (doInitialIncrement) {
			STATSCOUNTER_INC(found_ctr->ctr, found_ctr->mutCtr);
		}
	} else {
		created = hsearch_r(lookup, ENTER, &entry, &b->table);
		if (created) {
			SLIST_INSERT_HEAD(&b->ctrs, ctr, link);
			if (doInitialIncrement) {
				STATSCOUNTER_INC(ctr->ctr, ctr->mutCtr);
			}
		}
	}
	pthread_rwlock_unlock(&b->lock);

	if (found) {
		//ignore
	} else if (created) {
		ATOMIC_INC(&b->metricCount, &b->mutMetricCount);
		STATSCOUNTER_INC(b->ctrNewMetricAdd, b->mutCtrNewMetricAdd);
	} else {
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}
	
finalize_it:
	if ((! created) && (ctr != NULL)) {
		dynstats_destroyCtr(b, ctr, 1);
	}
	RETiRet;
}

rsRetVal
dynstats_inc(dynstats_bucket_t *b, uchar* metric) {
	ENTRY lookup;
	ENTRY *found;
	int succeed;
	dynstats_ctr_t *ctr;
	DEFiRet;

	if (! GatherStats) {
		FINALIZE;
	}

	if (ustrlen(metric) == 0) {
		STATSCOUNTER_INC(b->ctrNoMetric, b->mutCtrNoMetric);
		FINALIZE;
	}

	lookup.key = metric;
	
	if (pthread_rwlock_tryrdlock(&b->lock) == 0) {
		succeed = hsearch_r(lookup, FIND, &found, &b->table);
		if (succeed) {
			ctr = (dynstats_ctr_t *) found->data;
			STATSCOUNTER_INC(ctr->ctr, ctr->mutCtr);
		}
		pthread_rwlock_unlock(&b->lock);
	} else {
		ABORT_FINALIZE(RS_RET_NOENTRY);
	}

	if (!succeed) {
		CHKiRet(dynstats_addNewCtr(b, metric, 1));
	}
finalize_it:
	if (iRet != RS_RET_OK) {
		if (iRet == RS_RET_NOENTRY) {
			/* NOTE: this is not tested (because it requires very strong orchestration to gurantee contended lock for testing) */
			STATSCOUNTER_INC(b->ctrOpsIgnored, b->mutCtrOpsIgnored);
		} else {
			STATSCOUNTER_INC(b->ctrOpsOverflow, b->mutCtrOpsOverflow);
		}
	}
	RETiRet;
}

