/* modules.h
 *
 * Definition for build-in and plug-ins module handler. This file is the base
 * for all dynamically loadable module support. In theory, in v3 all modules
 * are dynamically loaded, in practice we currently do have a few build-in
 * once. This may become removed.
 *
 * The loader keeps track of what is loaded. For library modules, it is also
 * used to find objects (libraries) and to obtain the queryInterface function
 * for them. A reference count is maintened for libraries, so that they are
 * unloaded only when nobody still accesses them.
 *
 * File begun on 2007-07-22 by RGerhards
 *
 * Copyright 2007, 2008 Rainer Gerhards and Adiscon GmbH.
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
#ifndef	MODULES_H_INCLUDED
#define	MODULES_H_INCLUDED 1

#include "objomsr.h"


/* the following define defines the current version of the module interface.
 * It can be used by any module which want's to simply prevent version conflicts
 * and does not intend to do specific old-version emulations.
 * rgerhards, 2008-03-04
 * version 3 adds modInfo_t ptr to call of modInit -- rgerhards, 2008-03-10
 * version 4 removes needUDPSocket OM callback -- rgerhards, 2008-03-22
 * version 5 changes the way parsing works for input modules. This is
 *           an important change, parseAndSubmitMessage() goes away. Other
 *           module types are not affected. -- rgerhards, 2008-10-09
 */
#define CURR_MOD_IF_VERSION 5

typedef enum eModType_ {
	eMOD_IN,	/* input module */
	eMOD_OUT,	/* output module */
	eMOD_LIB	/* library module - this module provides one or many interfaces */
} eModType_t;


#ifdef DEBUG
typedef struct modUsr_s {
	struct modUsr_s *pNext;
	char *pszFile;
} modUsr_t;
#endif


/* how is this module linked? */
typedef enum eModLinkType_ {
	eMOD_LINK_STATIC,
	eMOD_LINK_DYNAMIC_UNLOADED,	/* dynalink module, currently not loaded */
	eMOD_LINK_DYNAMIC_LOADED,	/* dynalink module, currently loaded */
	eMOD_LINK_ALL			/* special: all linkage types, e.g. for unload */
} eModLinkType_t;

typedef struct modInfo_s {
	struct modInfo_s *pPrev;	/* support for creating a double linked module list */
	struct modInfo_s *pNext;	/* support for creating a linked module list */
	int		iIFVers;	/* Interface version of module */
	eModType_t	eType;		/* type of this module */
	eModLinkType_t	eLinkType;
	uchar*		pszName;	/* printable module name, e.g. for dbgprintf */
	unsigned	uRefCnt;	/* reference count for this module; 0 -> may be unloaded */
	/* functions supported by all types of modules */
	rsRetVal (*modInit)(int, int*, rsRetVal(**)());		/* initialize the module */
		/* be sure to support version handshake! */
	rsRetVal (*modQueryEtryPt)(uchar *name, rsRetVal (**EtryPoint)()); /* query entry point addresses */
	rsRetVal (*isCompatibleWithFeature)(syslogFeature);
	rsRetVal (*freeInstance)(void*);/* called before termination or module unload */
	rsRetVal (*dbgPrintInstInfo)(void*);/* called before termination or module unload */
	rsRetVal (*tryResume)(void*);/* called to see if module actin can be resumed now */
	rsRetVal (*modExit)(void);		/* called before termination or module unload */
	rsRetVal (*modGetID)(void **);		/* get its unique ID from module */
	rsRetVal (*doHUP)(void *);		/* non-restart type HUP handler */
	/* below: parse a configuration line - return if processed
	 * or not. If not, must be parsed to next module.
	 */
	rsRetVal (*parseConfigLine)(uchar **pConfLine);
	/* below: create an instance of this module. Most importantly the module
	 * can allocate instance memory in this call.
	 */
	rsRetVal (*createInstance)();
	/* TODO: pass pointer to msg submit function to IM  rger, 2007-12-14 */
	union	{
		struct {/* data for input modules */
			rsRetVal (*runInput)(thrdInfo_t*);	/* function to gather input and submit to queue */
			rsRetVal (*willRun)(void); 		/* function to gather input and submit to queue */
			rsRetVal (*afterRun)(thrdInfo_t*);	/* function to gather input and submit to queue */
		} im;
		struct {/* data for output modules */
			/* below: perform the configured action
			 */
			rsRetVal (*doAction)(uchar**, unsigned, void*);
			rsRetVal (*parseSelectorAct)(uchar**, void**,omodStringRequest_t**);
		} om;
		struct { /* data for library modules */
		} fm;
	} mod;
	void *pModHdlr; /* handler to the dynamic library holding the module */
#	ifdef DEBUG
	/* we add some home-grown support to track our users (and detect who does not free us). In
	 * the long term, this should probably be migrated into debug.c (TODO). -- rgerhards, 2008-03-11
	 */
	modUsr_t *pModUsrRoot;
#	endif
} modInfo_t;

/* interfaces */
BEGINinterface(module) /* name must also be changed in ENDinterface macro! */
	modInfo_t *(*GetNxt)(modInfo_t *pThis);
	modInfo_t *(*GetNxtType)(modInfo_t *pThis, eModType_t rqtdType);
	uchar *(*GetName)(modInfo_t *pThis);
	uchar *(*GetStateName)(modInfo_t *pThis);
	rsRetVal (*Use)(char *srcFile, modInfo_t *pThis);	/**< must be called before a module is used (ref counting) */
	rsRetVal (*Release)(char *srcFile, modInfo_t **ppThis);	/**< release a module (ref counting) */
	void (*PrintList)(void);
	rsRetVal (*UnloadAndDestructAll)(eModLinkType_t modLinkTypesToUnload);
	rsRetVal (*doModInit)(rsRetVal (*modInit)(), uchar *name, void *pModHdlr);
	rsRetVal (*Load)(uchar *name);
	rsRetVal (*SetModDir)(uchar *name);
ENDinterface(module)
#define moduleCURR_IF_VERSION 1 /* increment whenever you change the interface structure! */

/* prototypes */
PROTOTYPEObj(module);

/* TODO: remove them below (means move the config init code) -- rgerhards, 2008-02-19 */
extern uchar *pModDir; /* read-only after startup */


#endif /* #ifndef MODULES_H_INCLUDED */
/*
 * vi:set ai:
 */
