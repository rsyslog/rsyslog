/* modules.c
 * This is the implementation of syslogd modules object.
 * This object handles plug-ins and build-in modules of all kind.
 *
 * File begun on 2007-07-22 by RGerhards
 *
 * Copyright 2007 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Rsyslog is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rsyslog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Rsyslog.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */
#include "config.h"
#include "rsyslog.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <errno.h>

#include <dlfcn.h> /* TODO: replace this with the libtools equivalent! */

#include <unistd.h>
#include <sys/file.h>

#include "syslogd.h"
#include "cfsysline.h"
#include "modules.h"
#include "errmsg.h"

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(errmsg)

static modInfo_t *pLoadedModules = NULL;	/* list of currently-loaded modules */
static modInfo_t *pLoadedModulesLast = NULL;	/* tail-pointer */

/* config settings */
uchar	*pModDir = NULL; /* read-only after startup */


/* Construct a new module object
 */
static rsRetVal moduleConstruct(modInfo_t **pThis)
{
	modInfo_t *pNew;

	if((pNew = calloc(1, sizeof(modInfo_t))) == NULL)
		return RS_RET_OUT_OF_MEMORY;

	/* OK, we got the element, now initialize members that should
	 * not be zero-filled.
	 */

	*pThis = pNew;
	return RS_RET_OK;
}


/* Destructs a module object. The object must not be linked to the
 * linked list of modules. Please note that all other dependencies on this
 * modules must have been removed before (e.g. CfSysLineHandlers!)
 */
static void moduleDestruct(modInfo_t *pThis)
{
	if(pThis->pszName != NULL)
		free(pThis->pszName);
	if(pThis->pModHdlr != NULL)
		dlclose(pThis->pModHdlr);
	free(pThis);
}


/* The following function is the queryEntryPoint for host-based entry points.
 * Modules may call it to get access to core interface functions. Please note
 * that utility functions can be accessed via shared libraries - at least this
 * is my current shool of thinking.
 * Please note that the implementation as a query interface allows to take
 * care of plug-in interface version differences. -- rgerhards, 2007-07-31
 */
static rsRetVal queryHostEtryPt(uchar *name, rsRetVal (**pEtryPoint)())
{
	DEFiRet;

	if((name == NULL) || (pEtryPoint == NULL))
		return RS_RET_PARAM_ERROR;

	if(!strcmp((char*) name, "regCfSysLineHdlr")) {
		*pEtryPoint = regCfSysLineHdlr;
	} else if(!strcmp((char*) name, "objGetObjInterface")) {
		*pEtryPoint = objGetObjInterface;
	} else {
		*pEtryPoint = NULL; /* to  be on the safe side */
		ABORT_FINALIZE(RS_RET_ENTRY_POINT_NOT_FOUND);
	}

finalize_it:
	RETiRet;
}


/* get the name of a module
 */
static uchar *modGetName(modInfo_t *pThis)
{
	return((pThis->pszName == NULL) ? (uchar*) "" : pThis->pszName);
}


/* get the state-name of a module. The state name is its name
 * together with a short description of the module state (which
 * is pulled from the module itself.
 * rgerhards, 2007-07-24
 * TODO: the actual state name is not yet pulled
 */
static uchar *modGetStateName(modInfo_t *pThis)
{
	return(modGetName(pThis));
}


/* Add a module to the loaded module linked list
 */
static inline void
addModToList(modInfo_t *pThis)
{
	assert(pThis != NULL);

	if(pLoadedModules == NULL) {
		pLoadedModules = pLoadedModulesLast = pThis;
	} else {
		/* there already exist entries */
		pLoadedModulesLast->pNext = pThis;
		pLoadedModulesLast = pThis;
	}
}


/* Get the next module pointer - this is used to traverse the list.
 * The function returns the next pointer or NULL, if there is no next one.
 * The last object must be provided to the function. If NULL is provided,
 * it starts at the root of the list. Even in this case, NULL may be 
 * returned - then, the list is empty.
 * rgerhards, 2007-07-23
 */
static modInfo_t *GetNxt(modInfo_t *pThis)
{
	modInfo_t *pNew;

	if(pThis == NULL)
		pNew = pLoadedModules;
	else
		pNew = pThis->pNext;

	return(pNew);
}


/* this function is like GetNxt(), but it returns pointers to
 * modules of specific type only. As we currently deal just with output modules,
 * it is a dummy, to be filled with real code later.
 * rgerhards, 2007-07-24
 */
static modInfo_t *GetNxtType(modInfo_t *pThis, eModType_t rqtdType)
{
	modInfo_t *pMod = pThis;

	do {
		pMod = GetNxt(pMod);
	} while(!(pMod == NULL || pMod->eType == rqtdType)); /* warning: do ... while() */

	return pMod;
}


/* Prepare a module for unloading.
 * This is currently a dummy, to be filled when we have a plug-in
 * interface - rgerhards, 2007-08-09
 * rgerhards, 2007-11-21:
 * When this function is called, all instance-data must already have
 * been destroyed. In the case of output modules, this happens when the
 * rule set is being destroyed. When we implement other module types, we
 * need to think how we handle it there (and if we have any instance data).
 */
static rsRetVal modPrepareUnload(modInfo_t *pThis)
{
	DEFiRet;
	void *pModCookie;

	assert(pThis != NULL);

	CHKiRet(pThis->modGetID(&pModCookie));
	pThis->modExit(); /* tell the module to get ready for unload */
	CHKiRet(unregCfSysLineHdlrs4Owner(pModCookie));

finalize_it:
	RETiRet;
}


/* Add an already-loaded module to the module linked list. This function does
 * everything needed to fully initialize the module.
 */
static rsRetVal
doModInit(rsRetVal (*modInit)(int, int*, rsRetVal(**)(), rsRetVal(*)()), uchar *name, void *pModHdlr)
{
	DEFiRet;
	modInfo_t *pNew = NULL;
	rsRetVal (*modGetType)(eModType_t *pType);

	assert(modInit != NULL);

	if((iRet = moduleConstruct(&pNew)) != RS_RET_OK) {
		pNew = NULL;
		ABORT_FINALIZE(iRet);
	}

	CHKiRet((*modInit)(CURR_MOD_IF_VERSION, &pNew->iIFVers, &pNew->modQueryEtryPt, queryHostEtryPt));

	if(pNew->iIFVers != CURR_MOD_IF_VERSION) {
		ABORT_FINALIZE(RS_RET_MISSING_INTERFACE);
	}

	/* We now poll the module to see what type it is. We do this only once as this
	 * can never change in the lifetime of an module. -- rgerhards, 2007-12-14
	 */
	CHKiRet((*pNew->modQueryEtryPt)((uchar*)"getType", &modGetType));
	CHKiRet((iRet = (*modGetType)(&pNew->eType)) != RS_RET_OK);
	dbgprintf("module of type %d being loaded.\n", pNew->eType);
	
	/* OK, we know we can successfully work with the module. So we now fill the
	 * rest of the data elements. First we load the interfaces common to all
	 * module types.
	 */
	CHKiRet((*pNew->modQueryEtryPt)((uchar*)"modGetID", &pNew->modGetID));
	CHKiRet((*pNew->modQueryEtryPt)((uchar*)"modExit", &pNew->modExit));

	/* ... and now the module-specific interfaces */
	switch(pNew->eType) {
		case eMOD_IN:
			CHKiRet((*pNew->modQueryEtryPt)((uchar*)"runInput", &pNew->mod.im.runInput));
			CHKiRet((*pNew->modQueryEtryPt)((uchar*)"willRun", &pNew->mod.im.willRun));
			CHKiRet((*pNew->modQueryEtryPt)((uchar*)"afterRun", &pNew->mod.im.afterRun));
			break;
		case eMOD_OUT:
			CHKiRet((*pNew->modQueryEtryPt)((uchar*)"freeInstance", &pNew->freeInstance));
			CHKiRet((*pNew->modQueryEtryPt)((uchar*)"dbgPrintInstInfo", &pNew->dbgPrintInstInfo));
			CHKiRet((*pNew->modQueryEtryPt)((uchar*)"doAction", &pNew->mod.om.doAction));
			CHKiRet((*pNew->modQueryEtryPt)((uchar*)"parseSelectorAct", &pNew->mod.om.parseSelectorAct));
			CHKiRet((*pNew->modQueryEtryPt)((uchar*)"isCompatibleWithFeature", &pNew->isCompatibleWithFeature));
			CHKiRet((*pNew->modQueryEtryPt)((uchar*)"needUDPSocket", &pNew->needUDPSocket));
			CHKiRet((*pNew->modQueryEtryPt)((uchar*)"tryResume", &pNew->tryResume));
			break;
		case eMOD_LIB:
			break;
	}

	pNew->pszName = (uchar*) strdup((char*)name); /* we do not care if strdup() fails, we can accept that */
	pNew->pModHdlr = pModHdlr;
	/* TODO: take this from module */
	if(pModHdlr == NULL)
		pNew->eLinkType = eMOD_LINK_STATIC;
	else
		pNew->eLinkType = eMOD_LINK_DYNAMIC_LOADED;

	/* we initialized the structure, now let's add it to the linked list of modules */
	addModToList(pNew);

finalize_it:
RUNLOG_VAR("%d", iRet);
	if(iRet != RS_RET_OK) {
		if(pNew != NULL)
			moduleDestruct(pNew);
	}

	RETiRet;
}

/* Print loaded modules. This is more or less a 
 * debug or test aid, but anyhow I think it's worth it...
 * This only works if the dbgprintf() subsystem is initialized.
 * TODO: update for new input modules!
 */
static void modPrintList(void)
{
	modInfo_t *pMod;

	pMod = GetNxt(NULL);
	while(pMod != NULL) {
		dbgprintf("Loaded Module: Name='%s', IFVersion=%d, ",
			(char*) modGetName(pMod), pMod->iIFVers);
		dbgprintf("type=");
		switch(pMod->eType) {
		case eMOD_OUT:
			dbgprintf("output");
			break;
		case eMOD_IN:
			dbgprintf("input");
			break;
		case eMOD_LIB:
			dbgprintf("library");
			break;
		}
		dbgprintf(" module.\n");
		dbgprintf("Entry points:\n");
		dbgprintf("\tqueryEtryPt:        0x%lx\n", (unsigned long) pMod->modQueryEtryPt);
		dbgprintf("\tdoAction:           0x%lx\n", (unsigned long) pMod->mod.om.doAction);
		dbgprintf("\tparseSelectorAct:   0x%lx\n", (unsigned long) pMod->mod.om.parseSelectorAct);
		dbgprintf("\tdbgPrintInstInfo:   0x%lx\n", (unsigned long) pMod->dbgPrintInstInfo);
		dbgprintf("\tfreeInstance:       0x%lx\n", (unsigned long) pMod->freeInstance);
		dbgprintf("\n");
		pMod = GetNxt(pMod); /* done, go next */
	}
}


/* unload all modules and free module linked list
 * rgerhards, 2007-08-09
 */
static rsRetVal modUnloadAndDestructAll(void)
{
	DEFiRet;
	modInfo_t *pMod;
	modInfo_t *pModPrev;

	pMod = GetNxt(NULL);
	while(pMod != NULL) {
		pModPrev = pMod;
		pMod = GetNxt(pModPrev); /* get next */
		/* TODO: library modules are currently never unloaded! */
		if(pModPrev->eType == eMOD_LIB) {
			dbgprintf("NOT unloading library module %s\n", modGetName(pModPrev));
		} else {
			/* now we can destroy the previous module */
			dbgprintf("Unloading module %s\n", modGetName(pModPrev));
			modPrepareUnload(pModPrev);
			moduleDestruct(pModPrev);
		}
	}

	/* indicate list is now empty */
	pLoadedModules = NULL;
	pLoadedModulesLast = NULL;

	RETiRet;
}


/* unlink and destroy a module. The caller must provide a pointer to the module
 * itself as well as one to its immediate predecessor.
 * rgerhards, 2008-02-26
 */
static rsRetVal
modUnlinkAndDestroy(modInfo_t *pThis, modInfo_t *pPrev)
{
	DEFiRet;

	/* we need to unlink the module before we can destruct it -- rgerhards, 2008-02-26 */
	if(pPrev == NULL) {
		/* module is root, so we need to set a new root */
		pLoadedModules = pThis->pNext;
	} else {
		pPrev->pNext = pThis->pNext;
	}

	/* check if we need to update the "last" pointer */
	if(pLoadedModulesLast == pThis) {
		pLoadedModulesLast = pPrev;
	}

	/* finally, we are ready for the module to go away... */
	dbgprintf("Unloading module %s\n", modGetName(pThis));
	modPrepareUnload(pThis);
	moduleDestruct(pThis);

	RETiRet;
}


/* unload dynamically loaded modules
 */
static rsRetVal modUnloadAndDestructDynamic(void)
{
	DEFiRet;
	modInfo_t *pMod;
	modInfo_t *pModCurr; /* module currently being processed */
	modInfo_t *pModPrev; /* last module in active linked list */

	pModPrev = NULL; /* we do not yet have a previous module */
	pMod = GetNxt(NULL);
	while(pMod != NULL) {
		pModCurr = pMod;
		pMod = GetNxt(pModCurr); /* get next */
		/* now we can destroy the previous module */
		if(pModCurr->eLinkType != eMOD_LINK_STATIC) {
			modUnlinkAndDestroy(pModCurr, pModPrev);
		} else {
			pModPrev = pModCurr; /* don't delete, so this is the new prev ptr */
		}
	}

	RETiRet;
}


/* load a module and initialize it, based on doModLoad() from conf.c
 * rgerhards, 2008-03-05
 * varmojfekoj added support for dynamically loadable modules on 2007-08-13
 * rgerhards, 2007-09-25: please note that the non-threadsafe function dlerror() is
 * called below. This is ok because modules are currently only loaded during
 * configuration file processing, which is executed on a single thread. Should we
 * change that design at any stage (what is unlikely), we need to find a
 * replacement.
 */
static rsRetVal
Load(uchar *pModName)
{
	DEFiRet;
	
        uchar szPath[512];
        uchar errMsg[1024];
	uchar *pModNameBase;
	uchar *pModNameDup;
        void *pModHdlr, *pModInit;
	modInfo_t *pModInfo;

	assert(pModName != NULL);
	dbgprintf("Requested to load module '%s'\n", pModName);

	if((pModNameDup = (uchar *) strdup((char *) pModName)) == NULL)
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

	pModNameBase = (uchar *) basename((char*)pModNameDup);
	pModInfo = GetNxt(NULL);
	while(pModInfo != NULL) {
		if(!strcmp((char *) pModNameBase, (char *) modGetName(pModInfo))) {
			dbgprintf("Module '%s' already loaded\n", pModName);
			free(pModNameDup);
			ABORT_FINALIZE(RS_RET_OK);
		}
		pModInfo = GetNxt(pModInfo);
	}
	free(pModNameDup);

	if(*pModName == '/') {
		*szPath = '\0';	/* we do not need to append the path - its already in the module name */
	} else {
		strncpy((char *) szPath, (pModDir == NULL) ? _PATH_MODDIR : (char*) pModDir, sizeof(szPath));
	}
	strncat((char *) szPath, (char *) pModName, sizeof(szPath) - strlen((char*) szPath) - 1);
	if(!(pModHdlr = dlopen((char *) szPath, RTLD_NOW))) {
		snprintf((char *) errMsg, sizeof(errMsg), "could not load module '%s', dlopen: %s\n", szPath, dlerror());
		errMsg[sizeof(errMsg)/sizeof(uchar) - 1] = '\0';
		errmsg.LogError(NO_ERRCODE, "%s", errMsg);
		ABORT_FINALIZE(RS_RET_ERR);
	}
	if(!(pModInit = dlsym(pModHdlr, "modInit"))) {
		snprintf((char *) errMsg, sizeof(errMsg), "could not load module '%s', dlsym: %s\n", szPath, dlerror());
		errMsg[sizeof(errMsg)/sizeof(uchar) - 1] = '\0';
		errmsg.LogError(NO_ERRCODE, "%s", errMsg);
		dlclose(pModHdlr);
		ABORT_FINALIZE(RS_RET_ERR);
	}
	if((iRet = doModInit(pModInit, (uchar*) pModName, pModHdlr)) != RS_RET_OK) {
		snprintf((char *) errMsg, sizeof(errMsg), "could not load module '%s', rsyslog error %d\n", szPath, iRet);
		errMsg[sizeof(errMsg)/sizeof(uchar) - 1] = '\0';
		errmsg.LogError(NO_ERRCODE, "%s", errMsg);
		dlclose(pModHdlr);
		ABORT_FINALIZE(RS_RET_ERR);
	}

finalize_it:
	RETiRet;
}


/* queryInterface function
 * rgerhards, 2008-03-05
 */
BEGINobjQueryInterface(module)
CODESTARTobjQueryInterface(module)
	if(pIf->ifVersion != moduleCURR_IF_VERSION) { /* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->GetNxt = GetNxt;
	pIf->GetNxtType = GetNxtType;
	pIf->GetName = modGetName;
	pIf->GetStateName = modGetStateName;
	pIf->PrintList = modPrintList;
	pIf->UnloadAndDestructAll = modUnloadAndDestructAll;
	pIf->UnloadAndDestructDynamic = modUnloadAndDestructDynamic;
	pIf->doModInit = doModInit;
	pIf->Load = Load;
finalize_it:
ENDobjQueryInterface(module)


/* Initialize our class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-03-05
 */
BEGINAbstractObjClassInit(module, 1, OBJ_IS_CORE_MODULE) /* class, version - CHANGE class also in END MACRO! */
	/* request objects we use */
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
ENDObjClassInit(module)

/* vi:set ai:
 */
