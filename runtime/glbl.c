/* glbl.c - this module holds global defintions and data items.
 * These are shared among the runtime library. Their use should be
 * limited to cases where it is actually needed. The main intension for
 * implementing them was support for the transistion from v2 to v4
 * (with fully modular design), but it turned out that there may also
 * be some other good use cases besides backwards-compatibility.
 *
 * Module begun 2008-04-16 by Rainer Gerhards
 *
 * Copyright 2008 Rainer Gerhards and Adiscon GmbH.
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
#include <stdlib.h>
#include <sys/socket.h>
#include <assert.h>

#include "rsyslog.h"
#include "obj.h"
#include "cfsysline.h"
#include "glbl.h"

/* some defaults */
#ifndef DFLT_NETSTRM_DRVR
#	define DFLT_NETSTRM_DRVR ((uchar*)"ptcp")
#endif

/* static data */
DEFobjStaticHelpers

/* static data
 * For this object, these variables are obviously what makes the "meat" of the
 * class...
 */
static uchar *pszWorkDir = NULL;
static int bOptimizeUniProc = 1;	/* enable uniprocessor optimizations */
static int bHUPisRestart = 1;		/* should SIGHUP cause a full system restart? */
static int bPreserveFQDN = 0;		/* should FQDNs always be preserved? */
static int iMaxLine = 2048;		/* maximum length of a syslog message */
static int iDefPFFamily = PF_UNSPEC;     /* protocol family (IPv4, IPv6 or both) */
static int bDropMalPTRMsgs = 0;/* Drop messages which have malicious PTR records during DNS lookup */
static int option_DisallowWarning = 1;	/* complain if message from disallowed sender is received */
static int bDisableDNS = 0; /* don't look up IP addresses of remote messages */
static uchar *LocalHostName = NULL;/* our hostname  - read-only after startup */
static uchar *LocalFQDNName = NULL;/* our hostname as FQDN - read-only after startup */
static uchar *LocalDomain;	/* our local domain name  - read-only after startup */
static char **StripDomains = NULL;/* these domains may be stripped before writing logs  - r/o after s.u., never touched by init */
static char **LocalHosts = NULL;/* these hosts are logged with their hostname  - read-only after startup, never touched by init */
static uchar *pszDfltNetstrmDrvr = NULL; /* module name of default netstream driver */
static uchar *pszDfltNetstrmDrvrCAF = NULL; /* default CA file for the netstrm driver */
static uchar *pszDfltNetstrmDrvrKeyFile = NULL; /* default key file for the netstrm driver (server) */
static uchar *pszDfltNetstrmDrvrCertFile = NULL; /* default cert file for the netstrm driver (server) */


/* define a macro for the simple properties' set and get functions
 * (which are always the same). This is only suitable for pretty
 * simple cases which require neither checks nor memory allocation.
 */
#define SIMP_PROP(nameFunc, nameVar, dataType) \
	SIMP_PROP_GET(nameFunc, nameVar, dataType) \
	SIMP_PROP_SET(nameFunc, nameVar, dataType) 
#define SIMP_PROP_SET(nameFunc, nameVar, dataType) \
static rsRetVal Set##nameFunc(dataType newVal) \
{ \
	nameVar = newVal; \
	return RS_RET_OK; \
} 
#define SIMP_PROP_GET(nameFunc, nameVar, dataType) \
static dataType Get##nameFunc(void) \
{ \
	return(nameVar); \
}

SIMP_PROP(OptimizeUniProc, bOptimizeUniProc, int)
SIMP_PROP(PreserveFQDN, bPreserveFQDN, int)
SIMP_PROP(HUPisRestart, bHUPisRestart, int)
SIMP_PROP(MaxLine, iMaxLine, int)
SIMP_PROP(DefPFFamily, iDefPFFamily, int) /* note that in the future we may check the family argument */
SIMP_PROP(DropMalPTRMsgs, bDropMalPTRMsgs, int)
SIMP_PROP(Option_DisallowWarning, option_DisallowWarning, int)
SIMP_PROP(DisableDNS, bDisableDNS, int)
SIMP_PROP(LocalDomain, LocalDomain, uchar*)
SIMP_PROP(StripDomains, StripDomains, char**)
SIMP_PROP(LocalHosts, LocalHosts, char**)

SIMP_PROP_SET(LocalFQDNName, LocalFQDNName, uchar*)
SIMP_PROP_SET(LocalHostName, LocalHostName, uchar*)
SIMP_PROP_SET(DfltNetstrmDrvr, pszDfltNetstrmDrvr, uchar*) /* TODO: use custom function which frees existing value */
SIMP_PROP_SET(DfltNetstrmDrvrCAF, pszDfltNetstrmDrvrCAF, uchar*) /* TODO: use custom function which frees existing value */
SIMP_PROP_SET(DfltNetstrmDrvrKeyFile, pszDfltNetstrmDrvrKeyFile, uchar*) /* TODO: use custom function which frees existing value */
SIMP_PROP_SET(DfltNetstrmDrvrCertFile, pszDfltNetstrmDrvrCertFile, uchar*) /* TODO: use custom function which frees existing value */

#undef SIMP_PROP
#undef SIMP_PROP_SET
#undef SIMP_PROP_GET


/* return our local hostname. if it is not set, "[localhost]" is returned
 */
static uchar*
GetLocalHostName(void)
{
	uchar *pszRet;

	if(LocalHostName == NULL)
		pszRet = (uchar*) "[localhost]";
	else {
		if(GetPreserveFQDN() == 1)
			pszRet = LocalFQDNName;
		else
			pszRet = LocalHostName;
	}
	return(pszRet);
}


/* return the current localhost name as FQDN (requires FQDN to be set) 
 * TODO: we should set the FQDN ourselfs in here!
 */
static uchar*
GetLocalFQDNName(void)
{
	return(LocalFQDNName == NULL ? (uchar*) "[localhost]" : LocalFQDNName);
}


/* return the current working directory */
static uchar*
GetWorkDir(void)
{
	return(pszWorkDir == NULL ? (uchar*) "" : pszWorkDir);
}


/* return the current default netstream driver */
static uchar*
GetDfltNetstrmDrvr(void)
{
	return(pszDfltNetstrmDrvr == NULL ? DFLT_NETSTRM_DRVR : pszDfltNetstrmDrvr);
}


/* return the current default netstream driver CA File */
static uchar*
GetDfltNetstrmDrvrCAF(void)
{
	return(pszDfltNetstrmDrvrCAF);
}


/* return the current default netstream driver key File */
static uchar*
GetDfltNetstrmDrvrKeyFile(void)
{
	return(pszDfltNetstrmDrvrKeyFile);
}


/* return the current default netstream driver certificate File */
static uchar*
GetDfltNetstrmDrvrCertFile(void)
{
	return(pszDfltNetstrmDrvrCertFile);
}


/* queryInterface function
 * rgerhards, 2008-02-21
 */
BEGINobjQueryInterface(glbl)
CODESTARTobjQueryInterface(glbl)
	if(pIf->ifVersion != glblCURR_IF_VERSION) { /* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->GetWorkDir = GetWorkDir;
#define SIMP_PROP(name) \
	pIf->Get##name = Get##name; \
	pIf->Set##name = Set##name;
	SIMP_PROP(MaxLine);
	SIMP_PROP(OptimizeUniProc);
	SIMP_PROP(PreserveFQDN);
	SIMP_PROP(HUPisRestart);
	SIMP_PROP(DefPFFamily);
	SIMP_PROP(DropMalPTRMsgs);
	SIMP_PROP(Option_DisallowWarning);
	SIMP_PROP(DisableDNS);
	SIMP_PROP(LocalFQDNName)
	SIMP_PROP(LocalHostName)
	SIMP_PROP(LocalDomain)
	SIMP_PROP(StripDomains)
	SIMP_PROP(LocalHosts)
	SIMP_PROP(DfltNetstrmDrvr)
	SIMP_PROP(DfltNetstrmDrvrCAF)
	SIMP_PROP(DfltNetstrmDrvrKeyFile)
	SIMP_PROP(DfltNetstrmDrvrCertFile)
#undef	SIMP_PROP
finalize_it:
ENDobjQueryInterface(glbl)


/* Reset config variables to default values.
 * rgerhards, 2008-04-17
 */
static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	if(pszDfltNetstrmDrvr != NULL) {
		free(pszDfltNetstrmDrvr);
		pszDfltNetstrmDrvr = NULL;
	}
	if(pszDfltNetstrmDrvrCAF != NULL) {
		free(pszDfltNetstrmDrvrCAF);
		pszDfltNetstrmDrvrCAF = NULL;
	}
	if(pszDfltNetstrmDrvrKeyFile != NULL) {
		free(pszDfltNetstrmDrvrKeyFile);
		pszDfltNetstrmDrvrKeyFile = NULL;
	}
	if(pszDfltNetstrmDrvrCertFile != NULL) {
		free(pszDfltNetstrmDrvrCertFile);
		pszDfltNetstrmDrvrCertFile = NULL;
	}
	if(pszWorkDir != NULL) {
		free(pszWorkDir);
		pszWorkDir = NULL;
	}
	bDropMalPTRMsgs = 0;
	bOptimizeUniProc = 1;
	bHUPisRestart = 1;
	bPreserveFQDN = 0;
	return RS_RET_OK;
}



/* Initialize the glbl class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINAbstractObjClassInit(glbl, 1, OBJ_IS_CORE_MODULE) /* class, version */
	/* request objects we use */

	/* register config handlers (TODO: we need to implement a way to unregister them) */
	CHKiRet(regCfSysLineHdlr((uchar *)"workdirectory", 0, eCmdHdlrGetWord, NULL, &pszWorkDir, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"dropmsgswithmaliciousdnsptrrecords", 0, eCmdHdlrBinary, NULL, &bDropMalPTRMsgs, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"defaultnetstreamdriver", 0, eCmdHdlrGetWord, NULL, &pszDfltNetstrmDrvr, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"defaultnetstreamdrivercafile", 0, eCmdHdlrGetWord, NULL, &pszDfltNetstrmDrvrCAF, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"defaultnetstreamdriverkeyfile", 0, eCmdHdlrGetWord, NULL, &pszDfltNetstrmDrvrKeyFile, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"defaultnetstreamdrivercertfile", 0, eCmdHdlrGetWord, NULL, &pszDfltNetstrmDrvrCertFile, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"optimizeforuniprocessor", 0, eCmdHdlrBinary, NULL, &bOptimizeUniProc, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"hupisrestart", 0, eCmdHdlrBinary, NULL, &bHUPisRestart, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"preservefqdn", 0, eCmdHdlrBinary, NULL, &bPreserveFQDN, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL, NULL));
ENDObjClassInit(glbl)


/* Exit the glbl class.
 * rgerhards, 2008-04-17
 */
BEGINObjClassExit(glbl, OBJ_IS_CORE_MODULE) /* class, version */
	if(pszDfltNetstrmDrvr != NULL)
		free(pszDfltNetstrmDrvr);
	if(pszDfltNetstrmDrvrCAF != NULL)
		free(pszDfltNetstrmDrvrCAF);
	if(pszDfltNetstrmDrvrKeyFile != NULL)
		free(pszDfltNetstrmDrvrKeyFile);
	if(pszDfltNetstrmDrvrCertFile != NULL)
		free(pszDfltNetstrmDrvrCertFile);
	if(pszWorkDir != NULL)
		free(pszWorkDir);
	if(LocalHostName != NULL)
		free(LocalHostName);
	if(LocalFQDNName != NULL)
		free(LocalFQDNName);
ENDObjClassExit(glbl)

/* vi:set ai:
 */
