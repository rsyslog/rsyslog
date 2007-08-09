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
#ifndef	MODULES_H_INCLUDED
#define	MODULES_H_INCLUDED 1

#include "objomsr.h"

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
	rsRetVal (*getWriteFDForSelect)(void*,short *);/* called before termination or module unload */
	rsRetVal (*onSelectReadyWrite)(void*);/* called when fd is writeable after select() */
	rsRetVal (*needUDPSocket)(void*);/* called when fd is writeable after select() */
	rsRetVal (*dbgPrintInstInfo)(void*);/* called before termination or module unload */
	rsRetVal (*tryResume)(void*);/* called to see if module actin can be resumed now */
	rsRetVal (*modExit)();		/* called before termination or module unload */
	/* below: parse a configuration line - return if processed
	 * or not. If not, must be parsed to next module.
	 */
	rsRetVal (*parseConfigLine)(uchar **pConfLine);
	/* below: create an instance of this module. Most importantly the module
	 * can allocate instance memory in this call.
	 */
	rsRetVal (*createInstance)();
	union	{
		struct {/* data for input modules */
			/* input modules come after output modules are finished, I am
			 * currently not really thinking about them. rgerhards, 2007-07-19
			 */
		} im;
		struct {/* data for output modules */
			/* below: perform the configured action
			 */
			rsRetVal (*doAction)(uchar**, unsigned, void*);
			rsRetVal (*parseSelectorAct)(uchar**, void**,omodStringRequest_t**);
		} om;
	} mod;
} modInfo_t;

/* prototypes */
rsRetVal doModInit(rsRetVal (*modInit)(), uchar *name);
modInfo_t *omodGetNxt(modInfo_t *pThis);
uchar *modGetName(modInfo_t *pThis);
uchar *modGetStateName(modInfo_t *pThis);
void modPrintList(void);
rsRetVal modUnloadAndDestructAll(void);

#endif /* #ifndef MODULES_H_INCLUDED */
/*
 * vi:set ai:
 */
