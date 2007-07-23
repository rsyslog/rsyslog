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
	int		iModVers;	/* Interface version of module */
	eModType_t	eModType;	/* type of this module */
	eModLinkType_t	eModLinkType;
	uchar*		pszModName;	/* printable module name, e.g. for dprintf */
	/* functions supported by all types of modules */
	rsRetVal (*modInit)();		/* initialize the module */
		/* be sure to support version handshake! */
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
		};
		struct {/* data for output modules */
			/* below: perform the configured action
			 */
			rsRetVal (*doAction)();
		};
	} mod;
} modInfo_t;

/* prototypes */

#endif /* #ifndef MODULES_H_INCLUDED */
/*
 * vi:set ai:
 */
