/* rsconf.c - the rsyslog configuration system.
 *
 * Module begun 2011-04-19 by Rainer Gerhards
 *
 * Copyright 2011 by Rainer Gerhards and Adiscon GmbH.
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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include "rsyslog.h"
#include "obj.h"
#include "srUtils.h"
#include "ruleset.h"
#include "modules.h"
#include "conf.h"
#include "rsconf.h"
#include "cfsysline.h"
#include "errmsg.h"
#include "unicode-helper.h"

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(ruleset)
DEFobjCurrIf(module)
DEFobjCurrIf(conf)
DEFobjCurrIf(errmsg)

/* exported static data */
rsconf_t *runConf = NULL;/* the currently running config */
rsconf_t *loadConf = NULL;/* the config currently being loaded (no concurrent config load supported!) */

/* Standard-Constructor
 */
BEGINobjConstruct(rsconf) /* be sure to specify the object type also in END macro! */
	pThis->globals.bDebugPrintTemplateList = 1;
	pThis->globals.bDebugPrintModuleList = 1;
	pThis->globals.bDebugPrintCfSysLineHandlerList = 1;
	pThis->globals.bLogStatusMsgs = DFLT_bLogStatusMsgs;
	pThis->globals.bErrMsgToStderr = 1;
	pThis->templates.root = NULL;
	pThis->templates.last = NULL;
	pThis->templates.lastStatic = NULL;
	pThis->actions.nbrActions = 0;
	CHKiRet(llInit(&pThis->rulesets.llRulesets, rulesetDestructForLinkedList, rulesetKeyDestruct, strcasecmp));
finalize_it:
ENDobjConstruct(rsconf)


/* ConstructionFinalizer
 */
rsRetVal rsconfConstructFinalize(rsconf_t __attribute__((unused)) *pThis)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, rsconf);
	RETiRet;
}


/* destructor for the rsconf object */
BEGINobjDestruct(rsconf) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(rsconf)
	llDestroy(&(pThis->rulesets.llRulesets));
ENDobjDestruct(rsconf)


/* DebugPrint support for the rsconf object */
BEGINobjDebugPrint(rsconf) /* be sure to specify the object type also in END and CODESTART macros! */
	dbgprintf("configuration object %p\n", pThis);
	dbgprintf("Global Settings:\n");
	dbgprintf("  bDebugPrintTemplateList.............: %d\n",
		  pThis->globals.bDebugPrintTemplateList);
	dbgprintf("  bDebugPrintModuleList               : %d\n",
		  pThis->globals.bDebugPrintModuleList);
	dbgprintf("  bDebugPrintCfSysLineHandlerList.....: %d\n",
		  pThis->globals.bDebugPrintCfSysLineHandlerList);
	dbgprintf("  bLogStatusMsgs                      : %d\n",
		  pThis->globals.bLogStatusMsgs);
	dbgprintf("  bErrMsgToStderr.....................: %d\n",
		  pThis->globals.bErrMsgToStderr);
	ruleset.DebugPrintAll(pThis);
	DBGPRINTF("\n");
	if(pThis->globals.bDebugPrintTemplateList)
		tplPrintList(pThis);
	if(pThis->globals.bDebugPrintModuleList)
		module.PrintList();
	if(pThis->globals.bDebugPrintCfSysLineHandlerList)
		dbgPrintCfSysLineHandlers();
CODESTARTobjDebugPrint(rsconf)
ENDobjDebugPrint(rsconf)


/* Activate an already-loaded configuration. The configuration will become
 * the new running conf (if successful). Note that in theory this method may
 * be called when there already is a running conf. In practice, the current
 * version of rsyslog does not support this. Future versions probably will.
 * Begun 2011-04-20, rgerhards
 */
rsRetVal
activate(rsconf_t *cnf)
{
	DEFiRet;
	runConf = cnf;
	dbgprintf("configuration %p activated\n", cnf);
	RETiRet;
}


/* intialize the legacy config system */
static inline rsRetVal 
initLegacyConf(void)
{
	DEFiRet;
	ruleset_t *pRuleset;

	/* construct the default ruleset */
	ruleset.Construct(&pRuleset);
	ruleset.SetName(loadConf, pRuleset, UCHAR_CONSTANT("RSYSLOG_DefaultRuleset"));
	ruleset.ConstructFinalize(loadConf, pRuleset);

	RETiRet;
}


/* Load a configuration. This will do all necessary steps to create
 * the in-memory representation of the configuration, including support
 * for multiple configuration languages.
 * Note that to support the legacy language we must provide some global
 * object that holds the currently-being-loaded config ptr.
 * Begun 2011-04-20, rgerhards
 */
rsRetVal
load(rsconf_t **cnf, uchar *confFile)
{
	rsRetVal localRet;
	int iNbrActions;
	int bHadConfigErr = 0;
	char cbuf[BUFSIZ];
	DEFiRet;

	CHKiRet(rsconfConstruct(&loadConf));
ourConf = loadConf; // TODO: remove, once ourConf is gone!

	CHKiRet(initLegacyConf());

#if 0
dbgprintf("XXXX: 2, conf=%p\n", conf.processConfFile);
	/* open the configuration file */
	localRet = conf.processConfFile(loadConf, confFile);
	CHKiRet(conf.GetNbrActActions(loadConf, &iNbrActions));

dbgprintf("XXXX: 4\n");
	if(localRet != RS_RET_OK) {
		errmsg.LogError(0, localRet, "CONFIG ERROR: could not interpret master config file '%s'.", confFile);
		bHadConfigErr = 1;
	} else if(iNbrActions == 0) {
		errmsg.LogError(0, RS_RET_NO_ACTIONS, "CONFIG ERROR: there are no active actions configured. Inputs will "
			 "run, but no output whatsoever is created.");
		bHadConfigErr = 1;
	}

dbgprintf("XXXX: 10\n");
	if((localRet != RS_RET_OK && localRet != RS_RET_NONFATAL_CONFIG_ERR) || iNbrActions == 0) {

dbgprintf("XXXX: 20\n");
		/* rgerhards: this code is executed to set defaults when the
		 * config file could not be opened. We might think about
		 * abandoning the run in this case - but this, too, is not
		 * very clever... So we stick with what we have.
		 * We ignore any errors while doing this - we would be lost anyhow...
		 */
		errmsg.LogError(0, NO_ERRCODE, "EMERGENCY CONFIGURATION ACTIVATED - fix rsyslog config file!");

		/* note: we previously used _POSIY_TTY_NAME_MAX+1, but this turned out to be
		 * too low on linux... :-S   -- rgerhards, 2008-07-28
		 */
		char szTTYNameBuf[128];
		rule_t *pRule = NULL; /* initialization to NULL is *vitally* important! */
		conf.cfline(loadConf, UCHAR_CONSTANT("*.ERR\t" _PATH_CONSOLE), &pRule);
		conf.cfline(loadConf, UCHAR_CONSTANT("syslog.*\t" _PATH_CONSOLE), &pRule);
		conf.cfline(loadConf, UCHAR_CONSTANT("*.PANIC\t*"), &pRule);
		conf.cfline(loadConf, UCHAR_CONSTANT("syslog.*\troot"), &pRule);
		if(ttyname_r(0, szTTYNameBuf, sizeof(szTTYNameBuf)) == 0) {
			snprintf(cbuf,sizeof(cbuf), "*.*\t%s", szTTYNameBuf);
			conf.cfline(loadConf, (uchar*)cbuf, &pRule);
		} else {
			DBGPRINTF("error %d obtaining controlling terminal, not using that emergency rule\n", errno);
		}
		ruleset.AddRule(loadConf, ruleset.GetCurrent(loadConf), &pRule);
	}

#endif


	/* all OK, pass loaded conf to caller */
	*cnf = loadConf;
	loadConf = NULL;

	dbgprintf("rsyslog finished loading initial config %p\n", loadConf);
//	rsconfDebugPrint(loadConf);

finalize_it:
	RETiRet;
}


/* queryInterface function
 */
BEGINobjQueryInterface(rsconf)
CODESTARTobjQueryInterface(rsconf)
	if(pIf->ifVersion != rsconfCURR_IF_VERSION) { /* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->Construct = rsconfConstruct;
	pIf->ConstructFinalize = rsconfConstructFinalize;
	pIf->Destruct = rsconfDestruct;
	pIf->DebugPrint = rsconfDebugPrint;
	pIf->Load = load;
	pIf->Activate = activate;
finalize_it:
ENDobjQueryInterface(rsconf)


/* Initialize the rsconf class. Must be called as the very first method
 * before anything else is called inside this class.
 */
BEGINObjClassInit(rsconf, 1, OBJ_IS_CORE_MODULE) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(ruleset, CORE_COMPONENT));
	CHKiRet(objUse(module, CORE_COMPONENT));
	CHKiRet(objUse(conf, CORE_COMPONENT));
	CHKiRet(objUse(errmsg, CORE_COMPONENT));

	/* now set our own handlers */
	OBJSetMethodHandler(objMethod_DEBUGPRINT, rsconfDebugPrint);
	OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, rsconfConstructFinalize);
ENDObjClassInit(rsconf)


/* De-initialize the rsconf class.
 */
BEGINObjClassExit(rsconf, OBJ_IS_CORE_MODULE) /* class, version */
	objRelease(ruleset, CORE_COMPONENT);
	objRelease(module, CORE_COMPONENT);
	objRelease(conf, CORE_COMPONENT);
	objRelease(errmsg, CORE_COMPONENT);
ENDObjClassExit(rsconf)

/* vi:set ai:
 */
