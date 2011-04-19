/* The rsconf object. It models a complete rsyslog configuration.
 *
 * Copyright 2011 Rainer Gerhards and Adiscon GmbH.
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
#ifndef INCLUDED_RSCONF_H
#define INCLUDED_RSCONF_H

#include "linkedlist.h"

/* --- configuration objects (the plan is to have ALL upper layers in this file) --- */

/* globals are data items that are really global, and can be set only
 * once (at least in theory, because the legacy system permits them to 
 * be re-set as often as the user likes).
 */
struct globals_s {
	int bDebugPrintTemplateList;
	int bDebugPrintModuleList;
	int bDebugPrintCfSysLineHandlerList;
	int bLogStatusMsgs;	/* log rsyslog start/stop/HUP messages? */
	int bErrMsgToStderr;	/* print error messages to stderr (in addition to everything else)? */
};

/* (global) defaults are global in the sense that they are accessible
 * to all code, but they can change value and other objects (like
 * actions) actually copy the value a global had at the time the action
 * was defined. In that sense, a global default is just that, a default,
 * wich can (and will) be changed in the course of config file
 * processing. Once the config file has been processed, defaults
 * can be dropped. The current code does not do this for simplicity.
 * That is not a problem, because the defaults do not take up much memory.
 * At a later stage, we may think about dropping them. -- rgerhards, 2011-04-19
 */
struct defaults_s {
};


struct templates_s {
	struct template *root;	/* the root of the template list */
	struct template *last;	/* points to the last element of the template list */
	struct template *lastStatic; /* last static element of the template list */
};


struct actions_s {
	unsigned nbrActions;		/* number of actions */
};


struct rulesets_s {
	linkedList_t llRulesets; /* this is NOT a pointer - no typo here ;) */

	/* support for legacy rsyslog.conf format */
	ruleset_t *pCurr; /* currently "active" ruleset */
	ruleset_t *pDflt; /* current default ruleset, e.g. for binding to actions which have no other */
};


/* --- end configuration objects --- */

/* the rsconf object */
struct rsconf_s {
	BEGINobjInstance;	/* Data to implement generic object - MUST be the first data element! */
	globals_t globals;
	defaults_t defaults;
	templates_t templates;
	actions_t actions;
	rulesets_t rulesets;
	/* note: rulesets include the complete output part:
	 *  - rules
	 *  - filter (as part of the action)
	 *  - actions
	 * Of course, we need to debate if we shall change that some time...
	 */
};


/* interfaces */
BEGINinterface(rsconf) /* name must also be changed in ENDinterface macro! */
	INTERFACEObjDebugPrint(rsconf);
	rsRetVal (*Construct)(rsconf_t **ppThis);
	rsRetVal (*ConstructFinalize)(rsconf_t __attribute__((unused)) *pThis);
	rsRetVal (*Destruct)(rsconf_t **ppThis);
ENDinterface(rsconf)
#define rsconfCURR_IF_VERSION 1 /* increment whenever you change the interface above! */


/* prototypes */
PROTOTYPEObj(rsconf);

/* some defaults (to be removed?) */
#define DFLT_bLogStatusMsgs 1

#endif /* #ifndef INCLUDED_RSCONF_H */
