/* Definition of globally-accessible data items.
 *
 * This module provides access methods to items of global scope. Most often,
 * these globals serve as defaults to initialize local settings. Currently,
 * many of them are either constants or global variable references. However,
 * this module provides the necessary hooks to change that at any time.
 *
 * Please note that there currently is no glbl.c file as we do not yet
 * have any implementations.
 *
 * Copyright 2008, 2009 Rainer Gerhards and Adiscon GmbH.
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

#ifndef GLBL_H_INCLUDED
#define GLBL_H_INCLUDED

#include "prop.h"

#define glblGetIOBufSize() 4096 /* size of the IO buffer, e.g. for strm class */

/* interfaces */
BEGINinterface(glbl) /* name must also be changed in ENDinterface macro! */
	uchar* (*GetWorkDir)(void);
#define SIMP_PROP(name, dataType) \
	dataType (*Get##name)(void); \
	rsRetVal (*Set##name)(dataType);
	SIMP_PROP(MaxLine, int)
	SIMP_PROP(OptimizeUniProc, int)
	SIMP_PROP(PreserveFQDN, int)
	SIMP_PROP(DefPFFamily, int)
	SIMP_PROP(DropMalPTRMsgs, int)
	SIMP_PROP(Option_DisallowWarning, int)
	SIMP_PROP(DisableDNS, int)
	SIMP_PROP(LocalFQDNName, uchar*)
	SIMP_PROP(LocalHostName, uchar*)
	SIMP_PROP(LocalDomain, uchar*)
	SIMP_PROP(StripDomains, char**)
	SIMP_PROP(LocalHosts, char**)
	SIMP_PROP(DfltNetstrmDrvr, uchar*)
	SIMP_PROP(DfltNetstrmDrvrCAF, uchar*)
	SIMP_PROP(DfltNetstrmDrvrKeyFile, uchar*)
	SIMP_PROP(DfltNetstrmDrvrCertFile, uchar*)
	/* added v3, 2009-06-30 */
	rsRetVal (*GenerateLocalHostNameProperty)(void);
	prop_t* (*GetLocalHostNameProp)(void);
	/* added v4, 2009-07-20 */
	int (*GetGlobalInputTermState)(void);
	void (*SetGlobalInputTermination)(void);
	/* added v5, 2009-11-03 */
	SIMP_PROP(ParseHOSTNAMEandTAG, int)
	/* note: v4, v5 are already used by more recent versions, so we need to skip them! */
	/* added v6, 2009-11-16 as part of varmojfekoj's "unlimited select()" patch
	 * Note that it must be always present, otherwise the interface would have different
	 * versions depending on compile settings, what is not acceptable.
	 * Use this property with care, it is only truly available if UNLIMITED_SELECT is enabled
	 * (I did not yet further investigate the details, because that code hopefully can be removed
	 * at some later stage).
	 */
	SIMP_PROP(FdSetSize, int)
	/* v7: was neeeded to mean v5+v6 - do NOT add anything else for that version! */
	/* next is v8! */
#undef	SIMP_PROP
ENDinterface(glbl)
#define glblCURR_IF_VERSION 7 /* increment whenever you change the interface structure! */
/* version 2 had PreserveFQDN added - rgerhards, 2008-12-08 */

/* the remaining prototypes */
PROTOTYPEObj(glbl);

#endif /* #ifndef GLBL_H_INCLUDED */
