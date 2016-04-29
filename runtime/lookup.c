/* lookup.c
 * Support for lookup tables in RainerScript.
 *
 * Copyright 2013-2016 Adiscon GmbH.
 *
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
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
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

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

/* definitions for objects we access */
DEFobjStaticHelpers
DEFobjCurrIf(errmsg)
DEFobjCurrIf(glbl)

/* forward definitions */
static rsRetVal lookupReadFile(lookup_t *pThis, const uchar* name, const uchar* filename);
static void lookupDestruct(lookup_t *pThis);

/* static data */
/* tables for interfacing with the v6 config system (as far as we need to) */
static struct cnfparamdescr modpdescr[] = {
	{ "name", eCmdHdlrString, CNFPARAM_REQUIRED },
	{ "file", eCmdHdlrString, CNFPARAM_REQUIRED },
	{ "reloadOnHUP", eCmdHdlrBinary, 0 }
};
static struct cnfparamblk modpblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(modpdescr)/sizeof(struct cnfparamdescr),
	  modpdescr
	};

/* internal data-types */
typedef struct uint32_index_val_s {
	uint32_t index;
	uchar *val;
} uint32_index_val_t;

const char * reloader_prefix = "lkp_tbl_reloader:";

static void *
lookupTableReloader(void *self);

/* create a new lookup table object AND include it in our list of
 * lookup tables.
 */
rsRetVal
lookupNew(lookup_ref_t **ppThis)
{
	lookup_ref_t *pThis = NULL;
	lookup_t *t = NULL;
	DEFiRet;

	CHKmalloc(pThis = calloc(1, sizeof(lookup_ref_t)));
	CHKmalloc(t = calloc(1, sizeof(lookup_t)));
	pthread_rwlock_init(&pThis->rwlock, NULL);
	pthread_mutex_init(&pThis->reloader_mut, NULL);
	pthread_cond_init(&pThis->run_reloader, NULL);
	pthread_attr_init(&pThis->reloader_thd_attr);
	pThis->do_reload = pThis->do_stop = 0;
	pThis->reload_on_hup = 1; /*DO reload on HUP (default)*/
	pthread_create(&pThis->reloader, &pThis->reloader_thd_attr, lookupTableReloader, pThis);

	pThis->next = NULL;
	if(loadConf->lu_tabs.root == NULL) {
		loadConf->lu_tabs.root = pThis;
	} else {
		loadConf->lu_tabs.last->next = pThis;
	}
	loadConf->lu_tabs.last = pThis;

	pThis->self = t;

	*ppThis = pThis;
finalize_it:
	if(iRet != RS_RET_OK) {
		free(t);
		free(pThis);
	}
	RETiRet;
}

static inline void
freeStubValueForReloadFailure(lookup_ref_t *pThis) {/*must be called with reloader_mut acquired*/
	if (pThis->stub_value_for_reload_failure != NULL) {
		free(pThis->stub_value_for_reload_failure);
		pThis->stub_value_for_reload_failure = NULL;
	}
}

static void
lookupStopReloader(lookup_ref_t *pThis) {
	pthread_mutex_lock(&pThis->reloader_mut);
	freeStubValueForReloadFailure(pThis);
	pThis->do_reload = 0;
	pThis->do_stop = 1;
	pthread_cond_signal(&pThis->run_reloader);
	pthread_mutex_unlock(&pThis->reloader_mut);
	pthread_join(pThis->reloader, NULL);
}

static void
lookupRefDestruct(lookup_ref_t *pThis)
{
	lookupStopReloader(pThis);
	pthread_mutex_destroy(&pThis->reloader_mut);
	pthread_cond_destroy(&pThis->run_reloader);
	pthread_attr_destroy(&pThis->reloader_thd_attr);

	pthread_rwlock_destroy(&pThis->rwlock);
	lookupDestruct(pThis->self);
	free(pThis->name);
	free(pThis->filename);
	free(pThis);
}

static void
destructTable_str(lookup_t *pThis) {
	uint32_t i = 0;
	lookup_string_tab_entry_t *entries = pThis->table.str->entries;
	for (i = 0; i < pThis->nmemb; i++) {
		free(entries[i].key);
	}
	free(entries);
	free(pThis->table.str);
}


static void
destructTable_arr(lookup_t *pThis) {
	free(pThis->table.arr->interned_val_refs);
	free(pThis->table.arr);
}

static void
destructTable_sparseArr(lookup_t *pThis) {
	free(pThis->table.sprsArr->entries);
	free(pThis->table.sprsArr);
}

static void
lookupDestruct(lookup_t *pThis) {
	uint32_t i;

	if (pThis == NULL) return;
	
	if (pThis->type == STRING_LOOKUP_TABLE) {
		destructTable_str(pThis);
	} else if (pThis->type == ARRAY_LOOKUP_TABLE) {
		destructTable_arr(pThis);
	} else if (pThis->type == SPARSE_ARRAY_LOOKUP_TABLE) {
		destructTable_sparseArr(pThis);
	} else if (pThis->type == STUBBED_LOOKUP_TABLE) {
		/*nothing to be done*/
	}
	
	for (i = 0; i < pThis->interned_val_count; i++) {
		free(pThis->interned_vals[i]);
	}
	free(pThis->interned_vals);
	free(pThis->nomatch);
	free(pThis);
}

void
lookupInitCnf(lookup_tables_t *lu_tabs)
{
	lu_tabs->root = NULL;
	lu_tabs->last = NULL;
}

void
lookupDestroyCnf() {
	lookup_ref_t *luref, *luref_next;
	for(luref = loadConf->lu_tabs.root ; luref != NULL ; ) {
		luref_next = luref->next;
		lookupRefDestruct(luref);
		luref = luref_next;
	}	
}

/* comparison function for qsort() */
static int
qs_arrcmp_strtab(const void *s1, const void *s2)
{
	return ustrcmp(((lookup_string_tab_entry_t*)s1)->key, ((lookup_string_tab_entry_t*)s2)->key);
}

static int
qs_arrcmp_ustrs(const void *s1, const void *s2)
{
	return ustrcmp(*(uchar**)s1, *(uchar**)s2);
}

static int
qs_arrcmp_uint32_index_val(const void *s1, const void *s2)
{
	return ((uint32_index_val_t*)s1)->index - ((uint32_index_val_t*)s2)->index;
}

static int
qs_arrcmp_sprsArrtab(const void *s1, const void *s2)
{
	return ((lookup_sparseArray_tab_entry_t*)s1)->key - ((lookup_sparseArray_tab_entry_t*)s2)->key;
}

/* comparison function for bsearch() and string array compare
 * this is for the string lookup table type
 */
static int
bs_arrcmp_strtab(const void *s1, const void *s2)
{
	return strcmp((char*)s1, (char*)((lookup_string_tab_entry_t*)s2)->key);
}

static int
bs_arrcmp_str(const void *s1, const void *s2)
{
	return ustrcmp((uchar*)s1, *(uchar**)s2);
}

static int
bs_arrcmp_sprsArrtab(const void *s1, const void *s2)
{
	return *(uint32_t*)s1 - ((lookup_sparseArray_tab_entry_t*)s2)->key;
}

static inline const char*
defaultVal(lookup_t *pThis) {
	return (pThis->nomatch == NULL) ? "" : (const char*) pThis->nomatch;
}

/* lookup_fn for different types of tables */
static es_str_t*
lookupKey_stub(lookup_t *pThis, lookup_key_t __attribute__((unused)) key) {
	return es_newStrFromCStr((char*) pThis->nomatch, ustrlen(pThis->nomatch));
}

static es_str_t*
lookupKey_str(lookup_t *pThis, lookup_key_t key) {
	lookup_string_tab_entry_t *entry;
	const char *r;
	entry = bsearch(key.k_str, pThis->table.str->entries, pThis->nmemb, sizeof(lookup_string_tab_entry_t), bs_arrcmp_strtab);
	if(entry == NULL) {
		r = defaultVal(pThis);
	} else {
		r = (const char*)entry->interned_val_ref;
	}
	return es_newStrFromCStr(r, strlen(r));
}

static es_str_t*
lookupKey_arr(lookup_t *pThis, lookup_key_t key) {
	const char *r;
	uint32_t uint_key = key.k_uint;
	uint32_t idx = uint_key - pThis->table.arr->first_key;

	if (idx >= pThis->nmemb) {
		r = defaultVal(pThis);
	} else {
		r = (char*) pThis->table.arr->interned_val_refs[uint_key - pThis->table.arr->first_key];
	}
	return es_newStrFromCStr(r, strlen(r));
}

typedef int (comp_fn_t)(const void *s1, const void *s2);

static inline void *
bsearch_lte(const void *key, const void *base, size_t nmemb, size_t size, comp_fn_t *comp_fn)
{
	size_t l, u, idx;
	const void *p;
	int comparison;

	l = 0;
	u = nmemb;
	if (l == u) {
		return NULL;
	}
	while (l < u) {
		idx = (l + u) / 2;
		p = (void *) (((const char *) base) + (idx * size));
		comparison = (*comp_fn)(key, p);
		if (comparison < 0)
			u = idx;
		else if (comparison > 0)
			l = idx + 1;
		else
			return (void *) p;
	}
	if (comparison < 0) {
		if (idx == 0) {
			return NULL;
		}
		idx--;
	}
	return (void *) (((const char *) base) + ( idx * size));
}

static es_str_t*
lookupKey_sprsArr(lookup_t *pThis, lookup_key_t key) {
	lookup_sparseArray_tab_entry_t *entry;
	const char *r;
	entry = bsearch_lte(&key.k_uint, pThis->table.sprsArr->entries, pThis->nmemb, sizeof(lookup_sparseArray_tab_entry_t), bs_arrcmp_sprsArrtab);
	if(entry == NULL) {
		r = defaultVal(pThis);
	} else {
		r = (const char*)entry->interned_val_ref;
	}
	return es_newStrFromCStr(r, strlen(r));
}

/* builders for different table-types */

#define NO_INDEX_ERROR(type, name)				\
	errmsg.LogError(0, RS_RET_INVALID_VALUE, "'%s' lookup table named: '%s' has record(s) without 'index' field", type, name); \
	ABORT_FINALIZE(RS_RET_INVALID_VALUE);

static inline rsRetVal
build_StringTable(lookup_t *pThis, struct json_object *jtab, const uchar* name) {
	uint32_t i;
	struct json_object *jrow, *jindex, *jvalue;
	uchar *value, *canonicalValueRef;
	DEFiRet;
	
	pThis->table.str = NULL;
	CHKmalloc(pThis->table.str = calloc(1, sizeof(lookup_string_tab_t)));
	if (pThis->nmemb > 0) {
		CHKmalloc(pThis->table.str->entries = calloc(pThis->nmemb, sizeof(lookup_string_tab_entry_t)));

		for(i = 0; i < pThis->nmemb; i++) {
			jrow = json_object_array_get_idx(jtab, i);
			jindex = json_object_object_get(jrow, "index");
			jvalue = json_object_object_get(jrow, "value");
			if (jindex == NULL || json_object_is_type(jindex, json_type_null)) {
				NO_INDEX_ERROR("string", name);
			}
			CHKmalloc(pThis->table.str->entries[i].key = ustrdup((uchar*) json_object_get_string(jindex)));
			value = (uchar*) json_object_get_string(jvalue);
			canonicalValueRef = *(uchar**) bsearch(value, pThis->interned_vals, pThis->interned_val_count, sizeof(uchar*), bs_arrcmp_str);
			assert(canonicalValueRef != NULL);
			pThis->table.str->entries[i].interned_val_ref = canonicalValueRef;
		}
		qsort(pThis->table.str->entries, pThis->nmemb, sizeof(lookup_string_tab_entry_t), qs_arrcmp_strtab);
	}
		
	pThis->lookup = lookupKey_str;
	pThis->key_type = LOOKUP_KEY_TYPE_STRING;
finalize_it:
	RETiRet;
}

static inline rsRetVal
build_ArrayTable(lookup_t *pThis, struct json_object *jtab, const uchar *name) {
	uint32_t i;
	struct json_object *jrow, *jindex, *jvalue;
	uchar *canonicalValueRef;
	uint32_t prev_index, index;
	uint8_t prev_index_set;
	uint32_index_val_t *indexes = NULL;
	DEFiRet;

	prev_index_set = 0;
	
	pThis->table.arr = NULL;
	CHKmalloc(pThis->table.arr = calloc(1, sizeof(lookup_array_tab_t)));
	if (pThis->nmemb > 0) {
		CHKmalloc(indexes = calloc(pThis->nmemb, sizeof(uint32_index_val_t)));
		CHKmalloc(pThis->table.arr->interned_val_refs = calloc(pThis->nmemb, sizeof(uchar*)));

		for(i = 0; i < pThis->nmemb; i++) {
			jrow = json_object_array_get_idx(jtab, i);
			jindex = json_object_object_get(jrow, "index");
			jvalue = json_object_object_get(jrow, "value");
			if (jindex == NULL || json_object_is_type(jindex, json_type_null)) {
				NO_INDEX_ERROR("array", name);
			}
			indexes[i].index = (uint32_t) json_object_get_int(jindex);
			indexes[i].val = (uchar*) json_object_get_string(jvalue);
		}
		qsort(indexes, pThis->nmemb, sizeof(uint32_index_val_t), qs_arrcmp_uint32_index_val);
		for(i = 0; i < pThis->nmemb; i++) {
			index = indexes[i].index;
			if (prev_index_set == 0) {
				prev_index = index;
				prev_index_set = 1;
				pThis->table.arr->first_key = index;
			} else {
				if (index != ++prev_index) {
					errmsg.LogError(0, RS_RET_INVALID_VALUE, "'array' lookup table name: '%s' has non-contiguous members between index '%d' and '%d'",
									name, prev_index, index);
					ABORT_FINALIZE(RS_RET_INVALID_VALUE);
				}
			}
			canonicalValueRef = *(uchar**) bsearch(indexes[i].val, pThis->interned_vals, pThis->interned_val_count, sizeof(uchar*), bs_arrcmp_str);
			assert(canonicalValueRef != NULL);
			pThis->table.arr->interned_val_refs[i] = canonicalValueRef;
		}
	}
		
	pThis->lookup = lookupKey_arr;
	pThis->key_type = LOOKUP_KEY_TYPE_UINT;

finalize_it:
	free(indexes);
	RETiRet;
}

static inline rsRetVal
build_SparseArrayTable(lookup_t *pThis, struct json_object *jtab, const uchar* name) {
	uint32_t i;
	struct json_object *jrow, *jindex, *jvalue;
	uchar *value, *canonicalValueRef;
	DEFiRet;
	
	pThis->table.str = NULL;
	CHKmalloc(pThis->table.sprsArr = calloc(1, sizeof(lookup_sparseArray_tab_t)));
	if (pThis->nmemb > 0) {
		CHKmalloc(pThis->table.sprsArr->entries = calloc(pThis->nmemb, sizeof(lookup_sparseArray_tab_entry_t)));

		for(i = 0; i < pThis->nmemb; i++) {
			jrow = json_object_array_get_idx(jtab, i);
			jindex = json_object_object_get(jrow, "index");
			jvalue = json_object_object_get(jrow, "value");
			if (jindex == NULL || json_object_is_type(jindex, json_type_null)) {
				NO_INDEX_ERROR("sparseArray", name);
			}
			pThis->table.sprsArr->entries[i].key = (uint32_t) json_object_get_int(jindex);
			value = (uchar*) json_object_get_string(jvalue);
			canonicalValueRef = *(uchar**) bsearch(value, pThis->interned_vals, pThis->interned_val_count, sizeof(uchar*), bs_arrcmp_str);
			assert(canonicalValueRef != NULL);
			pThis->table.sprsArr->entries[i].interned_val_ref = canonicalValueRef;
		}
		qsort(pThis->table.sprsArr->entries, pThis->nmemb, sizeof(lookup_sparseArray_tab_entry_t), qs_arrcmp_sprsArrtab);
	}
		
	pThis->lookup = lookupKey_sprsArr;
	pThis->key_type = LOOKUP_KEY_TYPE_UINT;
	
finalize_it:
	RETiRet;
}

static rsRetVal
lookupBuildStubbedTable(lookup_t *pThis, const uchar* stub_val) {
	DEFiRet;
	
	CHKmalloc(pThis->nomatch = ustrdup(stub_val));
	pThis->lookup = lookupKey_stub;
	pThis->type = STUBBED_LOOKUP_TABLE;
	pThis->key_type = LOOKUP_KEY_TYPE_NONE;
	
finalize_it:
	RETiRet;
}

static rsRetVal
lookupBuildTable_v1(lookup_t *pThis, struct json_object *jroot, const uchar* name) {
	struct json_object *jnomatch, *jtype, *jtab;
	struct json_object *jrow, *jvalue;
	const char *table_type, *nomatch_value;
	const uchar **all_values;
	const uchar *curr, *prev;
	uint32_t i, j;
	uint32_t uniq_values;

	DEFiRet;
	all_values = NULL;

	jnomatch = json_object_object_get(jroot, "nomatch");
	jtype = json_object_object_get(jroot, "type");
	jtab = json_object_object_get(jroot, "table");
	if (jtab == NULL || !json_object_is_type(jtab, json_type_array)) {
		errmsg.LogError(0, RS_RET_INVALID_VALUE, "lookup table named: '%s' has invalid table definition", name);
		ABORT_FINALIZE(RS_RET_INVALID_VALUE);
	}
	pThis->nmemb = json_object_array_length(jtab);
	table_type = json_object_get_string(jtype);
	if (table_type == NULL) {
		table_type = "string";
	}

	CHKmalloc(all_values = malloc(pThis->nmemb * sizeof(uchar*)));

	/* before actual table can be loaded, prepare all-value list and remove duplicates*/
	for(i = 0; i < pThis->nmemb; i++) {
		jrow = json_object_array_get_idx(jtab, i);
		jvalue = json_object_object_get(jrow, "value");
		if (jvalue == NULL || json_object_is_type(jvalue, json_type_null)) {
			errmsg.LogError(0, RS_RET_INVALID_VALUE, "'%s' lookup table named: '%s' has record(s) without 'value' field", table_type, name);
			ABORT_FINALIZE(RS_RET_INVALID_VALUE);
		}
		all_values[i] = (const uchar*) json_object_get_string(jvalue);
	}
	qsort(all_values, pThis->nmemb, sizeof(uchar*), qs_arrcmp_ustrs);
	uniq_values = 1;
	for(i = 1; i < pThis->nmemb; i++) {
		curr = all_values[i];
		prev = all_values[i - 1];
		if (ustrcmp(prev, curr) != 0) {
			uniq_values++;
		}
	}

	if (pThis->nmemb > 0)  {
		CHKmalloc(pThis->interned_vals = malloc(uniq_values * sizeof(uchar*)));
		j = 0;
		CHKmalloc(pThis->interned_vals[j++] = ustrdup(all_values[0]));
		for(i = 1; i < pThis->nmemb ; ++i) {
			curr = all_values[i];
			prev = all_values[i - 1];
			if (ustrcmp(prev, curr) != 0) {
				CHKmalloc(pThis->interned_vals[j++] = ustrdup(all_values[i]));
			}
		}
		pThis->interned_val_count = uniq_values;
	}
	/* uniq values captured (sorted) */

	nomatch_value = json_object_get_string(jnomatch);
	if (nomatch_value != NULL) {
		CHKmalloc(pThis->nomatch = (uchar*) strdup(nomatch_value));
	}

	if (strcmp(table_type, "array") == 0) {
		pThis->type = ARRAY_LOOKUP_TABLE;
		CHKiRet(build_ArrayTable(pThis, jtab, name));
	} else if (strcmp(table_type, "sparseArray") == 0) {
		pThis->type = SPARSE_ARRAY_LOOKUP_TABLE;
		CHKiRet(build_SparseArrayTable(pThis, jtab, name));
	} else if (strcmp(table_type, "string") == 0) {
		pThis->type = STRING_LOOKUP_TABLE;
		CHKiRet(build_StringTable(pThis, jtab, name));
	} else {
		errmsg.LogError(0, RS_RET_INVALID_VALUE, "lookup table named: '%s' uses unupported type: '%s'", name, table_type);
		ABORT_FINALIZE(RS_RET_INVALID_VALUE);
	}
finalize_it:
	if (all_values != NULL) free(all_values);
	RETiRet;	
}

rsRetVal
lookupBuildTable(lookup_t *pThis, struct json_object *jroot, const uchar* name)
{
	struct json_object *jversion;
	int version = 1;

	DEFiRet;

	jversion = json_object_object_get(jroot, "version");
	if (jversion != NULL && !json_object_is_type(jversion, json_type_null)) {
		version = json_object_get_int(jversion);
	} else {
		errmsg.LogError(0, RS_RET_INVALID_VALUE, "lookup table named: '%s' doesn't specify version (will use default value: %d)", name, version);
	}
	if (version == 1) {
		CHKiRet(lookupBuildTable_v1(pThis, jroot, name));
	} else {
		errmsg.LogError(0, RS_RET_INVALID_VALUE, "lookup table named: '%s' uses unsupported version: %d", name, version);
		ABORT_FINALIZE(RS_RET_INVALID_VALUE);
	}

finalize_it:
	RETiRet;
}


/* find a lookup table. This is a naive O(n) algo, but this really
 * doesn't matter as it is called only a few times during config
 * load. The function returns either a pointer to the requested
 * table or NULL, if not found.
 */
lookup_ref_t *
lookupFindTable(uchar *name)
{
	lookup_ref_t *curr;

	for(curr = loadConf->lu_tabs.root ; curr != NULL ; curr = curr->next) {
		if(!ustrcmp(curr->name, name))
			break;
	}
	return curr;
}


/* this reloads a lookup table. This is done while the engine is running,
 * as such the function must ensure proper locking and proper order of
 * operations (so that nothing can interfere). If the table cannot be loaded,
 * the old table is continued to be used.
 */
static rsRetVal
lookupReloadOrStub(lookup_ref_t *pThis, const uchar* stub_val) {
	lookup_t *newlu, *oldlu; /* dummy to be able to use support functions without 
								affecting current settings. */
	DEFiRet;

	oldlu = pThis->self;
	newlu = NULL;
	
	DBGPRINTF("reload requested for lookup table '%s'\n", pThis->name);
	CHKmalloc(newlu = calloc(1, sizeof(lookup_t)));
	if (stub_val == NULL) {
		CHKiRet(lookupReadFile(newlu, pThis->name, pThis->filename));
	} else {
		CHKiRet(lookupBuildStubbedTable(newlu, stub_val));
	}
	/* all went well, copy over data members */
	pthread_rwlock_wrlock(&pThis->rwlock);
	pThis->self = newlu;
	pthread_rwlock_unlock(&pThis->rwlock);
finalize_it:
	if (iRet != RS_RET_OK) {
		if (stub_val == NULL) {
			errmsg.LogError(0, RS_RET_INTERNAL_ERROR,
							"lookup table '%s' could not be reloaded from file '%s'",
							pThis->name, pThis->filename);
		} else {
			errmsg.LogError(0, RS_RET_INTERNAL_ERROR,
							"lookup table '%s' could not be stubbed with value '%s'",
							pThis->name, stub_val);
		}
		lookupDestruct(newlu);
	} else {
		if (stub_val == NULL) {
			errmsg.LogError(0, RS_RET_OK, "lookup table '%s' reloaded from file '%s'",
							pThis->name, pThis->filename);
		} else {
			errmsg.LogError(0, RS_RET_OK, "lookup table '%s' stubbed with value '%s'",
							pThis->name, stub_val);
		}
		lookupDestruct(oldlu);
	}
	RETiRet;
}

static rsRetVal
lookupDoStub(lookup_ref_t *pThis, const uchar* stub_val)
{
	int already_stubbed = 0;
	DEFiRet;
	pthread_rwlock_rdlock(&pThis->rwlock);
	if (pThis->self->type == STUBBED_LOOKUP_TABLE &&
		ustrcmp(pThis->self->nomatch, stub_val) == 0)
		already_stubbed = 1;
	pthread_rwlock_unlock(&pThis->rwlock);
	if (! already_stubbed) {
		errmsg.LogError(0, RS_RET_OK, "stubbing lookup table '%s' with value '%s'",
						pThis->name, stub_val);
		CHKiRet(lookupReloadOrStub(pThis, stub_val));
	} else {
		errmsg.LogError(0, RS_RET_OK, "lookup table '%s' is already stubbed with value '%s'",
						pThis->name, stub_val);
	}
finalize_it:
	RETiRet;
}

uint8_t
lookupIsReloadPending(lookup_ref_t *pThis) {
	uint8_t reload_pending;
	pthread_mutex_lock(&pThis->reloader_mut);
	reload_pending = pThis->do_reload;
	pthread_mutex_unlock(&pThis->reloader_mut);
	return reload_pending;
}

rsRetVal
lookupReload(lookup_ref_t *pThis, const uchar *stub_val_if_reload_fails)
{
	uint8_t locked = 0;
	uint8_t duplicated_stub_value = 0;
	int lock_errno = 0;
	DEFiRet;
	if ((lock_errno = pthread_mutex_trylock(&pThis->reloader_mut)) == 0) {
		locked = 1;
		/*so it doesn't leak memory in situation where 2 reload requests are issued back to back*/
		freeStubValueForReloadFailure(pThis);
		if (stub_val_if_reload_fails != NULL) {
			CHKmalloc(pThis->stub_value_for_reload_failure = ustrdup(stub_val_if_reload_fails));
			duplicated_stub_value = 1;
		}
		pThis->do_reload = 1;
		pthread_cond_signal(&pThis->run_reloader);
	} else {
		errmsg.LogError(lock_errno, RS_RET_INTERNAL_ERROR, "attempt to trigger reload of lookup table '%s' failed (not stubbing)",
						pThis->name);
		ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
		/* we can choose to stub the table here, but it'll hurt because
		   the table reloader may take time to complete the reload
		   and stubbing because of a concurrent reload message may
		   not be desirable (except in very tightly controled environments
		   where reload-triggering messages pushed are timed accurately
		   and an idempotency-filter is used to reject re-deliveries) */
	}
finalize_it:
	if ((iRet != RS_RET_OK) && duplicated_stub_value) {
		freeStubValueForReloadFailure(pThis);
	}
	if (locked) {
		pthread_mutex_unlock(&pThis->reloader_mut);
	}
	RETiRet;
}

static rsRetVal
lookupDoReload(lookup_ref_t *pThis)
{
	DEFiRet;
	CHKiRet(lookupReloadOrStub(pThis, NULL));
finalize_it:
	if ((iRet != RS_RET_OK) &&
		(pThis->stub_value_for_reload_failure != NULL)) {
		CHKiRet(lookupDoStub(pThis, pThis->stub_value_for_reload_failure));
	}
	freeStubValueForReloadFailure(pThis);
	RETiRet;
}

static void *
lookupTableReloader(void *self)
{
	lookup_ref_t *pThis = (lookup_ref_t*) self;
	pthread_mutex_lock(&pThis->reloader_mut);
	while(1) {
		if (pThis->do_stop) {
			break;
		} else if (pThis->do_reload) {
			pThis->do_reload = 0;
			lookupDoReload(pThis);
		} else {
			pthread_cond_wait(&pThis->run_reloader, &pThis->reloader_mut);
		}
	}
	pthread_mutex_unlock(&pThis->reloader_mut);
	return NULL;
}

/* reload all lookup tables on HUP */
void
lookupDoHUP()
{
	lookup_ref_t *luref;
	for(luref = loadConf->lu_tabs.root ; luref != NULL ; luref = luref->next) {
		if (luref->reload_on_hup) {
			lookupReload(luref, NULL);
		}
	}
}

uint
lookupPendingReloadCount()
{
	uint pending_reload_count = 0;
	lookup_ref_t *luref;
	for(luref = loadConf->lu_tabs.root ; luref != NULL ; luref = luref->next) {
		if (lookupIsReloadPending(luref)) {
			pending_reload_count++;
		}
	}
	return pending_reload_count;
}


/* returns either a pointer to the value (read only!) or NULL
 * if either the key could not be found or an error occured.
 * Note that an estr_t object is returned. The caller is 
 * responsible for freeing it.
 */
es_str_t *
lookupKey(lookup_ref_t *pThis, lookup_key_t key)
{
	es_str_t *estr;
	lookup_t *t;
	pthread_rwlock_rdlock(&pThis->rwlock);
	t = pThis->self;
	estr = t->lookup(t, key);
	pthread_rwlock_unlock(&pThis->rwlock);
	return estr;
}


/* note: widely-deployed json_c 0.9 does NOT support incremental
 * parsing. In order to keep compatible with e.g. Ubuntu 12.04LTS,
 * we read the file into one big memory buffer and parse it at once.
 * While this is not very elegant, it will not pose any real issue
 * for "reasonable" lookup tables (and "unreasonably" large ones
 * will probably have other issues as well...).
 */
static rsRetVal
lookupReadFile(lookup_t *pThis, const uchar *name, const uchar *filename)
{
	struct json_tokener *tokener = NULL;
	struct json_object *json = NULL;
	int eno;
	char errStr[1024];
	char *iobuf = NULL;
	int fd;
	ssize_t nread;
	struct stat sb;
	DEFiRet;


	if(stat((char*)filename, &sb) == -1) {
		eno = errno;
		errmsg.LogError(0, RS_RET_FILE_NOT_FOUND,
			"lookup table file '%s' stat failed: %s",
			filename, rs_strerror_r(eno, errStr, sizeof(errStr)));
		ABORT_FINALIZE(RS_RET_FILE_NOT_FOUND);
	}

	CHKmalloc(iobuf = malloc(sb.st_size));

	if((fd = open((const char*) filename, O_RDONLY)) == -1) {
		eno = errno;
		errmsg.LogError(0, RS_RET_FILE_NOT_FOUND,
			"lookup table file '%s' could not be opened: %s",
			filename, rs_strerror_r(eno, errStr, sizeof(errStr)));
		ABORT_FINALIZE(RS_RET_FILE_NOT_FOUND);
	}

	tokener = json_tokener_new();
	nread = read(fd, iobuf, sb.st_size);
	if(nread != (ssize_t) sb.st_size) {
		eno = errno;
		errmsg.LogError(0, RS_RET_READ_ERR,
			"lookup table file '%s' read error: %s",
			filename, rs_strerror_r(eno, errStr, sizeof(errStr)));
		ABORT_FINALIZE(RS_RET_READ_ERR);
	}

	json = json_tokener_parse_ex(tokener, iobuf, sb.st_size);
	if(json == NULL) {
		errmsg.LogError(0, RS_RET_JSON_PARSE_ERR,
			"lookup table file '%s' json parsing error",
			filename);
		ABORT_FINALIZE(RS_RET_JSON_PARSE_ERR);
	}
	free(iobuf); /* early free to sever resources*/
	iobuf = NULL; /* make sure no double-free */

	/* got json object, now populate our own in-memory structure */
	CHKiRet(lookupBuildTable(pThis, json, name));

finalize_it:
	free(iobuf);
	if(tokener != NULL)
		json_tokener_free(tokener);
	if(json != NULL)
		json_object_put(json);
	RETiRet;
}


rsRetVal
lookupTableDefProcessCnf(struct cnfobj *o)
{
	struct cnfparamvals *pvals;
	lookup_ref_t *lu;
	short i;
#ifdef HAVE_PTHREAD_SETNAME_NP
	char *reloader_thd_name = NULL;
	int thd_name_len = 0;
#endif
	DEFiRet;
	lu = NULL;

	pvals = nvlstGetParams(o->nvlst, &modpblk, NULL);
	if(pvals == NULL) {
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}
	DBGPRINTF("lookupTableDefProcessCnf params:\n");
	cnfparamsPrint(&modpblk, pvals);
	
	CHKiRet(lookupNew(&lu));

	for(i = 0 ; i < modpblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(modpblk.descr[i].name, "file")) {
			CHKmalloc(lu->filename = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL));
		} else if(!strcmp(modpblk.descr[i].name, "name")) {
			CHKmalloc(lu->name = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL));
		} else if(!strcmp(modpblk.descr[i].name, "reloadOnHUP")) {
			lu->reload_on_hup = (pvals[i].val.d.n != 0);
		} else {
			dbgprintf("lookup_table: program error, non-handled "
			  "param '%s'\n", modpblk.descr[i].name);
		}
	}
#ifdef HAVE_PTHREAD_SETNAME_NP
	thd_name_len = ustrlen(lu->name) + strlen(reloader_prefix) + 1;
	CHKmalloc(reloader_thd_name = malloc(thd_name_len));
	strcpy(reloader_thd_name, reloader_prefix);
	strcpy(reloader_thd_name + strlen(reloader_prefix), (char*) lu->name);
	reloader_thd_name[thd_name_len - 1] = '\0';
	pthread_setname_np(lu->reloader, reloader_thd_name);
#endif
	CHKiRet(lookupReadFile(lu->self, lu->name, lu->filename));
	DBGPRINTF("lookup table '%s' loaded from file '%s'\n", lu->name, lu->filename);

finalize_it:
#ifdef HAVE_PTHREAD_SETNAME_NP
	free(reloader_thd_name);
#endif
	cnfparamvalsDestruct(pvals, &modpblk);
	if (iRet != RS_RET_OK) {
		if (lu != NULL) {
			lookupDestruct(lu->self);
			lu->self = NULL;
		}
	}
	RETiRet;
}

void
lookupClassExit(void)
{
	objRelease(glbl, CORE_COMPONENT);
	objRelease(errmsg, CORE_COMPONENT);
}

rsRetVal
lookupClassInit(void)
{
	DEFiRet;
	CHKiRet(objGetObjInterface(&obj));
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
finalize_it:
	RETiRet;
}
