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
#include <assert.h>

#include "rsyslog.h"
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
lookupDestruct(lookup_t *lookup)
{
	free(lookup);
}

void
lookupInitCnf(lookup_tables_t *lu_tabs)
{
	lu_tabs->root = NULL;
	lu_tabs->last = NULL;
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
