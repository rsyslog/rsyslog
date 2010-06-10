/* The rule object.
 *
 * This implements rules within rsyslog.
 *
 * Copyright 2009 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
 *
 * The rsyslog runtime library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The rsyslog runtime library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the rsyslog runtime library.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 * A copy of the LGPL can be found in the file "COPYING.LESSER" in this distribution.
 */
#ifndef INCLUDED_RULE_H
#define INCLUDED_RULE_H

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
