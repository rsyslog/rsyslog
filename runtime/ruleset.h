/* The ruleset object.
 *
 * This implements rulesets within rsyslog.
 *
 * Copyright 2009-2012 Rainer Gerhards and Adiscon GmbH.
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
#ifndef INCLUDED_RULESET_H
#define INCLUDED_RULESET_H

#include "linkedlist.h"

/* the ruleset object */
struct ruleset_s {
	BEGINobjInstance;	/* Data to implement generic object - MUST be the first data element! */
	linkedList_t llRules;	/* this is NOT a pointer - no typo here ;) */
	uchar *pszName;		/* name of our ruleset */
};

/* interfaces */
BEGINinterface(ruleset) /* name must also be changed in ENDinterface macro! */
	INTERFACEObjDebugPrint(ruleset);
	rsRetVal (*DebugPrintAll)(void);
	rsRetVal (*Construct)(ruleset_t **ppThis);
	rsRetVal (*ConstructFinalize)(ruleset_t __attribute__((unused)) *pThis);
	rsRetVal (*Destruct)(ruleset_t **ppThis);
	rsRetVal (*IterateAllActions)(rsRetVal (*pFunc)(void*, void*), void* pParam);
	rsRetVal (*DestructAllActions)(void);
	rsRetVal (*AddRule)(ruleset_t *pThis, rule_t **ppRule);
	rsRetVal (*SetName)(ruleset_t *pThis, uchar *pszName);
	rsRetVal (*ProcessMsg)(msg_t *pMsg);
	rsRetVal (*GetRuleset)(ruleset_t **ppThis, uchar*);
	rsRetVal (*SetDefaultRuleset)(uchar*);
	rsRetVal (*SetCurrRuleset)(uchar*);
	ruleset_t* (*GetCurrent)(void);
ENDinterface(ruleset)
#define rulesetCURR_IF_VERSION 1 /* increment whenever you change the interface structure! */


/* prototypes */
PROTOTYPEObj(ruleset);

#endif /* #ifndef INCLUDED_RULESET_H */
