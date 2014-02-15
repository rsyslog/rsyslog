/* lookup.c
 * Support for lookup tables in RainerScript.
 *
 * Copyright 2013 Adiscon GmbH.
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
DEFobjCurrIf(glbl)

/* forward definitions */
static rsRetVal lookupReadFile(lookup_t *pThis);

/* static data */
/* tables for interfacing with the v6 config system (as far as we need to) */
static struct cnfparamdescr modpdescr[] = {
	{ "name", eCmdHdlrString, CNFPARAM_REQUIRED },
	{ "file", eCmdHdlrString, CNFPARAM_REQUIRED }
};
static struct cnfparamblk modpblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(modpdescr)/sizeof(struct cnfparamdescr),
	  modpdescr
	};


/* create a new lookup table object AND include it in our list of
 * lookup tables.
 */
rsRetVal
lookupNew(lookup_t **ppThis)
{
	lookup_t *pThis = NULL;
	DEFiRet;

	CHKmalloc(pThis = malloc(sizeof(lookup_t)));
	pthread_rwlock_init(&pThis->rwlock, NULL);
	pThis->name = NULL;

	if(loadConf->lu_tabs.root == NULL) {
		loadConf->lu_tabs.root = pThis;
		pThis->next = NULL;
	} else {
		pThis->next = loadConf->lu_tabs.last;
	}
	loadConf->lu_tabs.last = pThis;

	*ppThis = pThis;
finalize_it:
	if(iRet != RS_RET_OK) {
		free(pThis);
	}
	RETiRet;
}
void
lookupDestruct(lookup_t *pThis)
{
	pthread_rwlock_destroy(&pThis->rwlock);
	free(pThis->name);
	free(pThis);
}

void
lookupInitCnf(lookup_tables_t *lu_tabs)
{
	lu_tabs->root = NULL;
	lu_tabs->last = NULL;
}


/* comparison function for qsort() and string array compare
 * this is for the string lookup table type
 */
static int
qs_arrcmp_strtab(const void *s1, const void *s2)
{
	return ustrcmp(((lookup_string_tab_etry_t*)s1)->key, ((lookup_string_tab_etry_t*)s2)->key);
}
/* comparison function for bsearch() and string array compare
 * this is for the string lookup table type
 */
static int
bs_arrcmp_strtab(const void *s1, const void *s2)
{
	return strcmp((char*)s1, (char*)((lookup_string_tab_etry_t*)s2)->key);
}

rsRetVal
lookupBuildTable(lookup_t *pThis, struct json_object *jroot)
{
	//struct json_object *jversion, *jnomatch, *jtype, *jtab;
	struct json_object *jtab;
	struct json_object *jrow, *jindex, *jvalue;
	uint32_t i;
	uint32_t maxStrSize;
	DEFiRet;

#if 0 // enable when we continue to work on this module
	jversion = json_object_object_get(jroot, "version");
	jnomatch = json_object_object_get(jroot, "nomatch");
	jtype = json_object_object_get(jroot, "type");
#endif
	jtab = json_object_object_get(jroot, "table");
	pThis->nmemb = json_object_array_length(jtab);
	CHKmalloc(pThis->d.strtab = malloc(pThis->nmemb * sizeof(lookup_string_tab_etry_t)));

	maxStrSize = 0;
	for(i = 0 ; i < pThis->nmemb ; ++i) {
		jrow = json_object_array_get_idx(jtab, i);
		jindex = json_object_object_get(jrow, "index");
		jvalue = json_object_object_get(jrow, "value");
		CHKmalloc(pThis->d.strtab[i].key = (uchar*) strdup(json_object_get_string(jindex)));
		CHKmalloc(pThis->d.strtab[i].val = (uchar*) strdup(json_object_get_string(jvalue)));
		maxStrSize += ustrlen(pThis->d.strtab[i].val);
	}

	qsort(pThis->d.strtab, pThis->nmemb, sizeof(lookup_string_tab_etry_t), qs_arrcmp_strtab);
dbgprintf("DDDD: table loaded (max size %u):\n", maxStrSize);
for(i = 0 ; i < pThis->nmemb ; ++i)
  dbgprintf("key: '%s', val: '%s'\n", pThis->d.strtab[i].key, pThis->d.strtab[i].val);

finalize_it:
	RETiRet;
}


/* find a lookup table. This is a naive O(n) algo, but this really
 * doesn't matter as it is called only a few times during config
 * load. The function returns either a pointer to the requested
 * table or NULL, if not found.
 */
lookup_t *
lookupFindTable(uchar *name)
{
	lookup_t *curr;

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
lookupReload(lookup_t *pThis)
{
	uint32_t i;
	lookup_t newlu; /* dummy to be able to use support functions without 
	                   affecting current settings. */
	DEFiRet;
	
	DBGPRINTF("reload requested for lookup table '%s'\n", pThis->name);
	memset(&newlu, 0, sizeof(newlu));
	CHKmalloc(newlu.name = ustrdup(pThis->name));
	CHKmalloc(newlu.filename = ustrdup(pThis->filename));
	CHKiRet(lookupReadFile(&newlu));
	/* all went well, copy over data members */
	pthread_rwlock_wrlock(&pThis->rwlock);
	for(i = 0 ; i < pThis->nmemb ; ++i) {
		free(pThis->d.strtab[i].key), /* we don't care about exec order of frees */
		free(pThis->d.strtab[i].val);
	}
	free(pThis->d.strtab);
	pThis->d.strtab = newlu.d.strtab; /* hand table AND ALL STRINGS over! */
	pthread_rwlock_unlock(&pThis->rwlock);
	errmsg.LogError(0, RS_RET_OK, "lookup table '%s' reloaded from file '%s'",
			pThis->name, pThis->filename);
finalize_it:
	free(newlu.name);
	free(newlu.filename);
	RETiRet;
}


/* reload all lookup tables on HUP */
void
lookupDoHUP()
{
	lookup_t *lu;
	for(lu = loadConf->lu_tabs.root ; lu != NULL ; lu = lu->next) {
		lookupReload(lu);
	}
}


/* returns either a pointer to the value (read only!) or NULL
 * if either the key could not be found or an error occured.
 * Note that an estr_t object is returned. The caller is 
 * responsible for freeing it.
 */
es_str_t *
lookupKey_estr(lookup_t *pThis, uchar *key)
{
	lookup_string_tab_etry_t *etry;
	char *r;
	es_str_t *estr;

	pthread_rwlock_rdlock(&pThis->rwlock);
	etry = bsearch(key, pThis->d.strtab, pThis->nmemb, sizeof(lookup_string_tab_etry_t), bs_arrcmp_strtab);
	if(etry == NULL) {
		r = ""; // TODO: use set default 
	} else {
		r = (char*)etry->val;
	}
	estr = es_newStrFromCStr(r, strlen(r));
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
lookupReadFile(lookup_t *pThis)
{
	struct json_tokener *tokener = NULL;
	struct json_object *json = NULL;
	int eno = errno;
	char errStr[1024];
	char *iobuf = NULL;
	int fd;
	ssize_t nread;
	struct stat sb;
	DEFiRet;


	if(stat((char*)pThis->filename, &sb) == -1) {
		eno = errno;
		errmsg.LogError(0, RS_RET_FILE_NOT_FOUND,
			"lookup table file '%s' stat failed: %s",
			pThis->filename, rs_strerror_r(eno, errStr, sizeof(errStr)));
		ABORT_FINALIZE(RS_RET_FILE_NOT_FOUND);
	}

	CHKmalloc(iobuf = malloc(sb.st_size));

	if((fd = open((const char*) pThis->filename, O_RDONLY)) == -1) {
		eno = errno;
		errmsg.LogError(0, RS_RET_FILE_NOT_FOUND,
			"lookup table file '%s' could not be opened: %s",
			pThis->filename, rs_strerror_r(eno, errStr, sizeof(errStr)));
		ABORT_FINALIZE(RS_RET_FILE_NOT_FOUND);
	}

	tokener = json_tokener_new();
	nread = read(fd, iobuf, sb.st_size);
	if(nread != (ssize_t) sb.st_size) {
		eno = errno;
		errmsg.LogError(0, RS_RET_READ_ERR,
			"lookup table file '%s' read error: %s",
			pThis->filename, rs_strerror_r(eno, errStr, sizeof(errStr)));
		ABORT_FINALIZE(RS_RET_READ_ERR);
	}

	json = json_tokener_parse_ex(tokener, iobuf, sb.st_size);
	if(json == NULL) {
		errmsg.LogError(0, RS_RET_JSON_PARSE_ERR,
			"lookup table file '%s' json parsing error",
			pThis->filename);
		ABORT_FINALIZE(RS_RET_JSON_PARSE_ERR);
	}
	free(iobuf); /* early free to sever resources*/
	iobuf = NULL; /* make sure no double-free */

	/* got json object, now populate our own in-memory structure */
	CHKiRet(lookupBuildTable(pThis, json));

finalize_it:
	free(iobuf);
	if(tokener != NULL)
		json_tokener_free(tokener);
	if(json != NULL)
		json_object_put(json);
	RETiRet;
}


rsRetVal
lookupProcessCnf(struct cnfobj *o)
{
	struct cnfparamvals *pvals;
	lookup_t *lu;
	short i;
	DEFiRet;

	pvals = nvlstGetParams(o->nvlst, &modpblk, NULL);
	if(pvals == NULL) {
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}
	DBGPRINTF("lookupProcessCnf params:\n");
	cnfparamsPrint(&modpblk, pvals);
	
	CHKiRet(lookupNew(&lu));

	for(i = 0 ; i < modpblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(modpblk.descr[i].name, "file")) {
			CHKmalloc(lu->filename = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL));
		} else if(!strcmp(modpblk.descr[i].name, "name")) {
			CHKmalloc(lu->name = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL));
		} else {
			dbgprintf("lookup_table: program error, non-handled "
			  "param '%s'\n", modpblk.descr[i].name);
		}
	}
	CHKiRet(lookupReadFile(lu));
	DBGPRINTF("lookup table '%s' loaded from file '%s'\n", lu->name, lu->filename);

finalize_it:
	cnfparamvalsDestruct(pvals, &modpblk);
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
