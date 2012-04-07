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
 * Copyright 2008-2012 Rainer Gerhards and Adiscon GmbH.
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

#ifndef GLBL_H_INCLUDED
#define GLBL_H_INCLUDED

#include "rainerscript.h"
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
	/* next change is v9! */
	/* v8 - 2012-03-21 */
	prop_t* (*GetLocalHostIP)(void);
#undef	SIMP_PROP
ENDinterface(glbl)
#define glblCURR_IF_VERSION 7 /* increment whenever you change the interface structure! */
/* version 2 had PreserveFQDN added - rgerhards, 2008-12-08 */

/* the remaining prototypes */
PROTOTYPEObj(glbl);

void glblPrepCnf(void);
void glblProcessCnf(struct cnfobj *o);
void glblDoneLoadCnf(void);

#endif /* #ifndef GLBL_H_INCLUDED */
