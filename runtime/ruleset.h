/* The ruleset object.
 *
 * This implements rulesets within rsyslog.
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
#ifndef INCLUDED_RULESET_H
#define INCLUDED_RULESET_H

#include "queue.h"
#include "linkedlist.h"

/* the ruleset object */
struct ruleset_s {
	BEGINobjInstance;	/* Data to implement generic object - MUST be the first data element! */
	linkedList_t llRules;	/* this is NOT a pointer - no typo here ;) */
	uchar *pszName;		/* name of our ruleset */
	qqueue_t *pQueue;	/* "main" message queue, if the ruleset has its own (else NULL) */
	parserList_t *pParserLst;/* list of parsers to use for this ruleset */
};

/* interfaces */
BEGINinterface(ruleset) /* name must also be changed in ENDinterface macro! */
	INTERFACEObjDebugPrint(ruleset);
	rsRetVal (*DebugPrintAll)(rsconf_t *conf);
	rsRetVal (*Construct)(ruleset_t **ppThis);
	rsRetVal (*ConstructFinalize)(rsconf_t *conf, ruleset_t __attribute__((unused)) *pThis);
	rsRetVal (*Destruct)(ruleset_t **ppThis);
	rsRetVal (*IterateAllActions)(rsconf_t *conf, rsRetVal (*pFunc)(void*, void*), void* pParam);
	rsRetVal (*DestructAllActions)(rsconf_t *conf);
	rsRetVal (*AddRule)(rsconf_t *conf, ruleset_t *pThis, rule_t **ppRule);
	rsRetVal (*SetName)(rsconf_t *conf, ruleset_t *pThis, uchar *pszName);
	rsRetVal (*ProcessBatch)(batch_t*);
	rsRetVal (*GetRuleset)(rsconf_t *conf, ruleset_t **ppThis, uchar*);
	rsRetVal (*SetDefaultRuleset)(rsconf_t *conf, uchar*);
	rsRetVal (*SetCurrRuleset)(rsconf_t *conf, uchar*);
	ruleset_t* (*GetCurrent)(rsconf_t *conf);
	qqueue_t* (*GetRulesetQueue)(ruleset_t*);
	/* v3, 2009-11-04 */
	parserList_t* (*GetParserList)(rsconf_t *conf, msg_t *);
	/* v5, 2011-04-19
	 * added support for the rsconf object -- fundamental change
	 */
ENDinterface(ruleset)
#define rulesetCURR_IF_VERSION 5 /* increment whenever you change the interface structure! */


/* prototypes */
PROTOTYPEObj(ruleset);

/* TODO: remove these -- currently done dirty for config file
 * redo -- rgerhards, 2011-04-19
 */
rsRetVal rulesetDestructForLinkedList(void *pData);
rsRetVal rulesetKeyDestruct(void __attribute__((unused)) *pData);
#endif /* #ifndef INCLUDED_RULESET_H */
