/* The rule object.
 *
 * This implements rules within rsyslog.
 *
 * Copyright 2009-2012 Adiscon GmbH.
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
#ifndef INCLUDED_RULE_H
#define INCLUDED_RULE_H

#include "libestr.h"
#include "linkedlist.h"
#include "regexp.h"
#include "expr.h"

/* the rule object */
struct rule_s {
	BEGINobjInstance;	/* Data to implement generic object - MUST be the first data element! */
	/* filter properties */
	enum {
		FILTER_PRI = 0,		/* traditional PRI based filer */
		FILTER_PROP = 1,	/* extended filter, property based */
		FILTER_EXPR = 2		/* extended filter, expression based */
	} f_filter_type;
	EHostnameCmpMode eHostnameCmpMode;
	cstr_t *pCSHostnameComp;	/* hostname to check */
	cstr_t *pCSProgNameComp;	/* tag to check or NULL, if not to be checked */
	union {
		u_char	f_pmask[LOG_NFACILITIES+1];	/* priority mask */
		struct {
			fiop_t operation;
			regex_t *regex_cache;		/* cache for compiled REs, if such are used */
			cstr_t *pCSCompValue;		/* value to "compare" against */
			sbool isNegated;	
			propid_t propID;		/* ID of the requested property */
			es_str_t *propName;		/* name of property for CEE-based filters */
		} prop;
		expr_t *f_expr;				/* expression object */
	} f_filterData;

	ruleset_t *pRuleset;	/* associated ruleset */
	linkedList_t llActList;	/* list of configured actions */
};

/* interfaces */
BEGINinterface(rule) /* name must also be changed in ENDinterface macro! */
	INTERFACEObjDebugPrint(rule);
	rsRetVal (*Construct)(rule_t **ppThis);
	rsRetVal (*ConstructFinalize)(rule_t __attribute__((unused)) *pThis);
	rsRetVal (*Destruct)(rule_t **ppThis);
	rsRetVal (*IterateAllActions)(rule_t *pThis, rsRetVal (*pFunc)(void*, void*), void *pParam);
	rsRetVal (*ProcessBatch)(rule_t *pThis, batch_t *pBatch);
	rsRetVal (*SetAssRuleset)(rule_t *pThis, ruleset_t*);
	ruleset_t* (*GetAssRuleset)(rule_t *pThis);
ENDinterface(rule)
#define ruleCURR_IF_VERSION 2 /* increment whenever you change the interface structure! */
/* change for v2: ProcessMsg replaced by ProcessBatch - 2010-06-10 */


/* prototypes */
PROTOTYPEObj(rule);

#endif /* #ifndef INCLUDED_RULE_H */
