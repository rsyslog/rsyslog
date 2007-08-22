/* modules.c
 * This is the implementation of syslogd modules object.
 * This object handles plug-ins and buil-in modules of all kind.
 *
 * File begun on 2007-07-22 by RGerhards
 *
 * Copyright 2007 Rainer Gerhards and Adiscon GmbH.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#include <unistd.h>
#include <sys/file.h>

#include "syslogd.h"
#include "cfsysline.h"
#include "modules.h"

static modInfo_t *pLoadedModules = NULL;	/* list of currently-loaded modules */
static modInfo_t *pLoadedModulesLast = NULL; /* tail-pointer */
static int bCfsyslineInitialized = 0;


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


/* Destructs a module objects. The object must not be linked to the
 * linked list of modules.
 */
static void moduleDestruct(modInfo_t *pThis)
{
	if(pThis->pszName != NULL)
		free(pThis->pszName);
	if(pThis->pModHdlr != NULL)
		dlclose(pThis->pModHdlr);
	free(pThis);
}


/* The followind function is the queryEntryPoint for host-based entry points.
 * Modules may call it to get access to core interface functions. Please note
 * that utility functions can be accessed via shared libraries - at least this
 * is my current shool of thinking.
 * Please note that the implementation as a query interface allows to take
 * care of plug-in interface version differences. -- rgerhards, 2007-07-31
 */
rsRetVal queryHostEtryPt(uchar *name, rsRetVal (**pEtryPoint)())
{
	DEFiRet;

	if((name == NULL) || (pEtryPoint == NULL))
		return RS_RET_PARAM_ERROR;

	if(!strcmp((char*) name, "regCfSysLineHdlr")) {
		*pEtryPoint = regCfSysLineHdlr;
	}

	if(iRet == RS_RET_OK)
		iRet = (*pEtryPoint == NULL) ? RS_RET_NOT_FOUND : RS_RET_OK;
	return iRet;
}


/* get the state-name of a module. The state name is its name
 * together with a short description of the module state (which
 * is pulled from the module itself.
 * rgerhards, 2007-07-24
 * TODO: the actual state name is not yet pulled
 */
uchar *modGetStateName(modInfo_t *pThis)
{
	return(modGetName(pThis));
}


/* get the name of a module
 */
uchar *modGetName(modInfo_t *pThis)
{
	return((pThis->pszName == NULL) ? (uchar*) "" : pThis->pszName);
}


/* Add a module to the loaded module linked list
 */
static inline void addModToList(modInfo_t *pThis)
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
modInfo_t *modGetNxt(modInfo_t *pThis)
{
	modInfo_t *pNew;

	if(pThis == NULL)
		pNew = pLoadedModules;
	else
		pNew = pThis->pNext;

	return(pNew);
}


/* this function is like modGetNxt(), but it returns pointers to
 * output modules only. As we currently deal just with output modules,
 * it is a dummy, to be filled with real code later.
 * rgerhards, 2007-07-24
 */
modInfo_t *omodGetNxt(modInfo_t *pThis)
{
	return(modGetNxt(pThis));
}


/* unload a module. If this is called with a statically-linked
 * (builtin) module, nothing happens.
 * The module handle is invalid after this function call and 
 * MUST NOT be used any more.
 * This is currently a dummy, to be filled when we have a plug-in interface
 * rgerhards, 2007-08-09
 */
static rsRetVal modUnload(modInfo_t *pThis)
{
	DEFiRet;

	assert(pThis != NULL);

	if(pThis->eLinkType == eMOD_LINK_STATIC) {
		ABORT_FINALIZE(RS_RET_OK);
	}

	/* TODO: implement code */
	ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);

finalize_it:
	return iRet;
}


/* Add an already-loaded module to the module linked list. This function does
 * everything needed to fully initialize the module.
 */
rsRetVal doModInit(rsRetVal (*modInit)(int, int*, rsRetVal(**)(), rsRetVal(*)()), uchar *name, void *pModHdlr)
{
	DEFiRet;
	modInfo_t *pNew;

	assert(modInit != NULL);

	if(bCfsyslineInitialized == 0) {
		/* we need to initialize the cfsysline subsystem first */
		CHKiRet(cfsyslineInit());
		bCfsyslineInitialized = 1;
	}

	if((iRet = moduleConstruct(&pNew)) != RS_RET_OK)
		return iRet;

	if((iRet = (*modInit)(1, &pNew->iIFVers, &pNew->modQueryEtryPt, queryHostEtryPt)) != RS_RET_OK) {
		moduleDestruct(pNew);
		return iRet;
	}

	if(pNew->iIFVers != 1) {
		moduleDestruct(pNew);
		return RS_RET_MISSING_INTERFACE;
	}

	/* OK, we know we can successfully work with the module. So we now fill the
	 * rest of the data elements.
	 */
	if((iRet = (*pNew->modQueryEtryPt)((uchar*)"doAction", &pNew->mod.om.doAction)) != RS_RET_OK) {
		moduleDestruct(pNew);
		return iRet;
	}
	if((iRet = (*pNew->modQueryEtryPt)((uchar*)"parseSelectorAct", &pNew->mod.om.parseSelectorAct)) != RS_RET_OK) {
		moduleDestruct(pNew);
		return iRet;
	}
	if((iRet = (*pNew->modQueryEtryPt)((uchar*)"isCompatibleWithFeature",
			                   &pNew->isCompatibleWithFeature)) != RS_RET_OK) {
		moduleDestruct(pNew);
		return iRet;
	}
	if((iRet = (*pNew->modQueryEtryPt)((uchar*)"dbgPrintInstInfo",
			                   &pNew->dbgPrintInstInfo)) != RS_RET_OK) {
		moduleDestruct(pNew);
		return iRet;
	}
	if((iRet = (*pNew->modQueryEtryPt)((uchar*)"getWriteFDForSelect", &pNew->getWriteFDForSelect)) != RS_RET_OK) {
		moduleDestruct(pNew);
		return iRet;
	}
	if((iRet = (*pNew->modQueryEtryPt)((uchar*)"onSelectReadyWrite", &pNew->onSelectReadyWrite)) != RS_RET_OK) {
		moduleDestruct(pNew);
		return iRet;
	}
	if((iRet = (*pNew->modQueryEtryPt)((uchar*)"needUDPSocket", &pNew->needUDPSocket)) != RS_RET_OK) {
		moduleDestruct(pNew);
		return iRet;
	}
	if((iRet = (*pNew->modQueryEtryPt)((uchar*)"tryResume", &pNew->tryResume)) != RS_RET_OK) {
		moduleDestruct(pNew);
		return iRet;
	}
	if((iRet = (*pNew->modQueryEtryPt)((uchar*)"freeInstance", &pNew->freeInstance)) != RS_RET_OK) {
		moduleDestruct(pNew);
		return iRet;
	}

	pNew->pszName = (uchar*) strdup((char*)name); /* we do not care if strdup() fails, we can accept that */
	pNew->pModHdlr = pModHdlr;
	pNew->eType = eMOD_OUT; /* TODO: take this from module */
	/* TODO: take this from module */
	if(pModHdlr == NULL)
		pNew->eLinkType = eMOD_LINK_STATIC;
	else
		pNew->eLinkType = eMOD_LINK_DYNAMIC_LOADED;

	/* we initialized the structure, now let's add it to the linked list of modules */
	addModToList(pNew);

finalize_it:
	return iRet;
}

/* Print loaded modules. This is more or less a 
 * debug or test aid, but anyhow I think it's worth it...
 * This only works if the dbgprintf() subsystem is initialized.
 */
void modPrintList(void)
{
	modInfo_t *pMod;

	pMod = modGetNxt(NULL);
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
		case eMOD_FILTER:
			dbgprintf("filter");
			break;
		}
		dbgprintf(" module.\n");
		dbgprintf("Entry points:\n");
		dbgprintf("\tqueryEtryPt:        0x%x\n", (unsigned) pMod->modQueryEtryPt);
		dbgprintf("\tdoAction:           0x%x\n", (unsigned) pMod->mod.om.doAction);
		dbgprintf("\tparseSelectorAct:   0x%x\n", (unsigned) pMod->mod.om.parseSelectorAct);
		dbgprintf("\tdbgPrintInstInfo:   0x%x\n", (unsigned) pMod->dbgPrintInstInfo);
		dbgprintf("\tfreeInstance:       0x%x\n", (unsigned) pMod->freeInstance);
		dbgprintf("\n");
		pMod = modGetNxt(pMod); /* done, go next */
	}
}


/* unload all modules and free module linked list
 * rgerhards, 2007-08-09
 */
rsRetVal modUnloadAndDestructAll(void)
{
	DEFiRet;
	modInfo_t *pMod;
	modInfo_t *pModPrev;

	pMod = modGetNxt(NULL);
	while(pMod != NULL) {
		pModPrev = pMod;
		pMod = modGetNxt(pModPrev); /* get next */
		/* now we can destroy the previous module */
		dbgprintf("Unloading module %s\n", modGetName(pModPrev));
		modUnload(pModPrev);
		moduleDestruct(pModPrev);
	}

	return iRet;
}


rsRetVal modUnloadAndDestructDynamic(void)
{
	DEFiRet;
	modInfo_t *pMod;
	modInfo_t *pModPrev;

	pLoadedModulesLast = NULL;

	pMod = modGetNxt(NULL);
	while(pMod != NULL) {
		pModPrev = pMod;
		pMod = modGetNxt(pModPrev); /* get next */
		/* now we can destroy the previous module */
		if(pModPrev->eLinkType != eMOD_LINK_STATIC) {
			dbgprintf("Unloading module %s\n", modGetName(pModPrev));
			moduleDestruct(pModPrev);
		} else {
			pLoadedModulesLast = pModPrev;
		}
	}

	return iRet;
}
/*
 * vi:set ai:
 */
