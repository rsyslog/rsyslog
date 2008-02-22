/* modules.h
 * Definition for build-in and plug-ins module handler.
 *
 * The following definitions are to be used for modularization. Currently,
 * the code is NOT complete. I am just adding pieces to it as I
 * go along in designing the interface.
 * rgerhards, 2007-07-19
 *
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
#ifndef	MODULES_H_INCLUDED
#define	MODULES_H_INCLUDED 1

#include "objomsr.h"
#include "threads.h"

typedef enum eModType_ {
	eMOD_IN,	/* input module */
	eMOD_OUT,	/* output module */
	eMOD_FILTER	/* filter module (not know yet if we will once have such at all...) */
} eModType_t;

/* how is this module linked? */
typedef enum eModLinkType_ {
	eMOD_LINK_STATIC,
	eMOD_LINK_DYNAMIC_UNLOADED,	/* dynalink module, currently not loaded */
	eMOD_LINK_DYNAMIC_LOADED	/* dynalink module, currently loaded */
} eModLinkType_t;

typedef struct moduleInfo {
	struct moduleInfo *pNext;	/* support for creating a linked module list */
	int		iIFVers;	/* Interface version of module */
	eModType_t	eType;		/* type of this module */
	eModLinkType_t	eLinkType;
	uchar*		pszName;	/* printable module name, e.g. for dbgprintf */
	/* functions supported by all types of modules */
	rsRetVal (*modInit)(int, int*, rsRetVal(**)());		/* initialize the module */
		/* be sure to support version handshake! */
	rsRetVal (*modQueryEtryPt)(uchar *name, rsRetVal (**EtryPoint)()); /* query entry point addresses */
	rsRetVal (*isCompatibleWithFeature)(syslogFeature);
	rsRetVal (*freeInstance)(void*);/* called before termination or module unload */
	rsRetVal (*needUDPSocket)(void*);/* called when fd is writeable after select() */
	rsRetVal (*dbgPrintInstInfo)(void*);/* called before termination or module unload */
	rsRetVal (*tryResume)(void*);/* called to see if module actin can be resumed now */
	rsRetVal (*modExit)(void);		/* called before termination or module unload */
	rsRetVal (*modGetID)(void **);		/* get its unique ID from module */
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
	} mod;
	void *pModHdlr; /* handler to the dynamic library holding the module */
} modInfo_t;

/* prototypes */
rsRetVal doModInit(rsRetVal (*modInit)(), uchar *name, void *pModHdlr);
modInfo_t *modGetNxt(modInfo_t *pThis);
modInfo_t *modGetNxtType(modInfo_t *pThis, eModType_t rqtdType);
uchar *modGetName(modInfo_t *pThis);
uchar *modGetStateName(modInfo_t *pThis);
void modPrintList(void);
rsRetVal modUnloadAndDestructAll(void);
rsRetVal modUnloadAndDestructDynamic(void);

#endif /* #ifndef MODULES_H_INCLUDED */
/*
 * vi:set ai:
 */
