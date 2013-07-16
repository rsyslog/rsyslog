/* header for lookup.c
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
#ifndef INCLUDED_LOOKUP_H
#define INCLUDED_LOOKUP_H
#include <libestr.h>

struct lookup_tables_s {
	lookup_t *root;	/* the root of the template list */
	lookup_t *last;	/* points to the last element of the template list */
};

struct lookup_string_tab_etry_s {
	uchar *key;
	uchar *val;
};

/* a single lookup table */
struct lookup_s {
	pthread_rwlock_t rwlock;	/* protect us in case of dynamic reloads */
	uchar *name;
	uchar *filename;
	uint32_t nmemb;
	union {
		lookup_string_tab_etry_t *strtab;
	} d;
	lookup_t *next;
};

/* prototypes */
void lookupInitCnf(lookup_tables_t *lu_tabs);
rsRetVal lookupProcessCnf(struct cnfobj *o);
lookup_t *lookupFindTable(uchar *name);
es_str_t * lookupKey_estr(lookup_t *pThis, uchar *key);
void lookupDestruct(lookup_t *pThis);
void lookupClassExit(void);
void lookupDoHUP();
rsRetVal lookupClassInit(void);

#endif /* #ifndef INCLUDED_LOOKUP_H */
