/* glbl.c - this module holds global defintions and data items.
 * These are shared among the runtime library. Their use should be
 * limited to cases where it is actually needed. The main intension for
 * implementing them was support for the transistion from v2 to v4
 * (with fully modular design), but it turned out that there may also
 * be some other good use cases besides backwards-compatibility.
 *
 * Module begun 2008-04-16 by Rainer Gerhards
 *
 * Copyright 2008-2013 Rainer Gerhards and Adiscon GmbH.
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

#include "config.h"
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>

#include "rsyslog.h"
#include "obj.h"
#include "unicode-helper.h"
#include "cfsysline.h"
#include "glbl.h"
#include "prop.h"
#include "atomic.h"
#include "errmsg.h"
#include "rainerscript.h"
#include "net.h"

/* some defaults */
#ifndef DFLT_NETSTRM_DRVR
#	define DFLT_NETSTRM_DRVR ((uchar*)"ptcp")
#endif

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(prop)
DEFobjCurrIf(errmsg)
DEFobjCurrIf(net)

/* static data
 * For this object, these variables are obviously what makes the "meat" of the
 * class...
 */
static uchar *pszWorkDir = NULL;
static int bOptimizeUniProc = 1;	/* enable uniprocessor optimizations */
static int bParseHOSTNAMEandTAG = 1;	/* parser modification (based on startup params!) */
static int bPreserveFQDN = 0;		/* should FQDNs always be preserved? */
static int iMaxLine = 8096;		/* maximum length of a syslog message */
static int iDefPFFamily = PF_UNSPEC;     /* protocol family (IPv4, IPv6 or both) */
static int bDropMalPTRMsgs = 0;/* Drop messages which have malicious PTR records during DNS lookup */
static int option_DisallowWarning = 1;	/* complain if message from disallowed sender is received */
static int bDisableDNS = 0; /* don't look up IP addresses of remote messages */
static prop_t *propLocalIPIF = NULL;/* IP address to report for the local host (default is 127.0.0.1) */
static prop_t *propLocalHostName = NULL;/* our hostname as FQDN - read-only after startup */
static uchar *LocalHostName = NULL;/* our hostname  - read-only after startup, except HUP */
static uchar *LocalHostNameOverride = NULL;/* user-overridden hostname - read-only after startup */
static uchar *LocalFQDNName = NULL;/* our hostname as FQDN - read-only after startup, except HUP */
static uchar *LocalDomain = NULL;/* our local domain name  - read-only after startup, except HUP */
static char **StripDomains = NULL;/* these domains may be stripped before writing logs  - r/o after s.u., never touched by init */
static char **LocalHosts = NULL;/* these hosts are logged with their hostname  - read-only after startup, never touched by init */
static uchar *pszDfltNetstrmDrvr = NULL; /* module name of default netstream driver */
static uchar *pszDfltNetstrmDrvrCAF = NULL; /* default CA file for the netstrm driver */
static uchar *pszDfltNetstrmDrvrKeyFile = NULL; /* default key file for the netstrm driver (server) */
static uchar *pszDfltNetstrmDrvrCertFile = NULL; /* default cert file for the netstrm driver (server) */
static int bTerminateInputs = 0;		/* global switch that inputs shall terminate ASAP (1=> terminate) */
pid_t glbl_ourpid;
#ifndef HAVE_ATOMIC_BUILTINS
static DEF_ATOMIC_HELPER_MUT(mutTerminateInputs);
#endif
#ifdef USE_UNLIMITED_SELECT
static int iFdSetSize = howmany(FD_SETSIZE, __NFDBITS) * sizeof (fd_mask); /* size of select() bitmask in bytes */
#endif


/* tables for interfacing with the v6 config system */
static struct cnfparamdescr cnfparamdescr[] = {
	{ "workdirectory", eCmdHdlrString, 0 },
	{ "dropmsgswithmaliciousdnsptrrecords", eCmdHdlrBinary, 0 },
	{ "localhostname", eCmdHdlrGetWord, 0 },
	{ "preservefqdn", eCmdHdlrBinary, 0 },
	{ "defaultnetstreamdrivercafile", eCmdHdlrString, 0 },
	{ "defaultnetstreamdriverkeyfile", eCmdHdlrString, 0 },
	{ "defaultnetstreamdriver", eCmdHdlrString, 0 },
	{ "maxmessagesize", eCmdHdlrSize, 0 },
};
static struct cnfparamblk paramblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(cnfparamdescr)/sizeof(struct cnfparamdescr),
	  cnfparamdescr
	};

static struct cnfparamvals *cnfparamvals = NULL;
/* we need to support multiple calls into our param block, so we need
 * to persist the current settings. Note that this must be re-set
 * each time a new config load begins (TODO: create interface?)
 */

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

SIMP_PROP(ParseHOSTNAMEandTAG, bParseHOSTNAMEandTAG, int)
SIMP_PROP(OptimizeUniProc, bOptimizeUniProc, int)
SIMP_PROP(PreserveFQDN, bPreserveFQDN, int)
SIMP_PROP(MaxLine, iMaxLine, int)
SIMP_PROP(DefPFFamily, iDefPFFamily, int) /* note that in the future we may check the family argument */
SIMP_PROP(DropMalPTRMsgs, bDropMalPTRMsgs, int)
SIMP_PROP(Option_DisallowWarning, option_DisallowWarning, int)
SIMP_PROP(DisableDNS, bDisableDNS, int)
SIMP_PROP(StripDomains, StripDomains, char**)
SIMP_PROP(LocalHosts, LocalHosts, char**)
#ifdef USE_UNLIMITED_SELECT
SIMP_PROP(FdSetSize, iFdSetSize, int)
#endif

SIMP_PROP_SET(DfltNetstrmDrvr, pszDfltNetstrmDrvr, uchar*) /* TODO: use custom function which frees existing value */
SIMP_PROP_SET(DfltNetstrmDrvrCAF, pszDfltNetstrmDrvrCAF, uchar*) /* TODO: use custom function which frees existing value */
SIMP_PROP_SET(DfltNetstrmDrvrKeyFile, pszDfltNetstrmDrvrKeyFile, uchar*) /* TODO: use custom function which frees existing value */
SIMP_PROP_SET(DfltNetstrmDrvrCertFile, pszDfltNetstrmDrvrCertFile, uchar*) /* TODO: use custom function which frees existing value */

#undef SIMP_PROP
#undef SIMP_PROP_SET
#undef SIMP_PROP_GET


/* return global input termination status
 * rgerhards, 2009-07-20
 */
static int GetGlobalInputTermState(void)
{
	return ATOMIC_FETCH_32BIT(&bTerminateInputs, &mutTerminateInputs);
}


/* set global termination state to "terminate". Note that this is a
 * "once in a lifetime" action which can not be undone. -- gerhards, 2009-07-20
 */
static void SetGlobalInputTermination(void)
{
	ATOMIC_STORE_1_TO_INT(&bTerminateInputs, &mutTerminateInputs);
}


/* set the local host IP address to a specific string. Helper to
 * small set of functions. No checks done, caller must ensure it is
 * ok to call. Most importantly, the IP address must not already have 
 * been set. -- rgerhards, 2012-03-21
 */
static inline rsRetVal
storeLocalHostIPIF(uchar *myIP)
{
	DEFiRet;
	CHKiRet(prop.Construct(&propLocalIPIF));
	CHKiRet(prop.SetString(propLocalIPIF, myIP, ustrlen(myIP)));
	CHKiRet(prop.ConstructFinalize(propLocalIPIF));
	DBGPRINTF("rsyslog/glbl: using '%s' as localhost IP\n", myIP);
finalize_it:
	RETiRet;
}


/* This function is used to set the IP address that is to be
 * reported for the local host. Note that in order to ease things
 * for the v6 config interface, we do not allow to set this more
 * than once.
 * rgerhards, 2012-03-21
 */
static rsRetVal
setLocalHostIPIF(void __attribute__((unused)) *pVal, uchar *pNewVal)
{
	uchar myIP[128];
	rsRetVal localRet;
	DEFiRet;

	CHKiRet(objUse(net, CORE_COMPONENT));

	if(propLocalIPIF != NULL) {
		errmsg.LogError(0, RS_RET_ERR, "$LocalHostIPIF is already set "
				"and cannot be reset; place it at TOP OF rsyslog.conf!");
		ABORT_FINALIZE(RS_RET_ERR);
	}

	localRet = net.GetIFIPAddr(pNewVal, AF_UNSPEC, myIP, (int) sizeof(myIP));
	if(localRet != RS_RET_OK) {
		errmsg.LogError(0, RS_RET_ERR, "$LocalHostIPIF: IP address for interface "
				"'%s' cannnot be obtained - ignoring directive", pNewVal);
	} else  {
		storeLocalHostIPIF(myIP);
	}


finalize_it:
	free(pNewVal); /* no longer needed -> is in prop! */
	RETiRet;
}


/* This function is used to set the global work directory name. 
 * It verifies that the provided directory actually exists and
 * emits an error message if not.
 * rgerhards, 2011-02-16
 */
static rsRetVal setWorkDir(void __attribute__((unused)) *pVal, uchar *pNewVal)
{
	size_t lenDir;
	int i;
	struct stat sb;
	DEFiRet;

	/* remove trailing slashes */
	lenDir = ustrlen(pNewVal);
	i = lenDir - 1;
	while(i > 0 && pNewVal[i] == '/') {
		--i;
	}

	if(i < 0) {
		errmsg.LogError(0, RS_RET_ERR_WRKDIR, "$WorkDirectory: empty value "
				"- directive ignored");
		ABORT_FINALIZE(RS_RET_ERR_WRKDIR);
	}

	if(i != (int) lenDir - 1) {
		pNewVal[i+1] = '\0';
		errmsg.LogError(0, RS_RET_WRN_WRKDIR, "$WorkDirectory: trailing slashes "
			"removed, new value is '%s'", pNewVal);
	}

	if(stat((char*) pNewVal, &sb) != 0) {
		errmsg.LogError(0, RS_RET_ERR_WRKDIR, "$WorkDirectory: %s can not be "
				"accessed, probably does not exist - directive ignored", pNewVal);
		ABORT_FINALIZE(RS_RET_ERR_WRKDIR);
	}

	if(!S_ISDIR(sb.st_mode)) {
		errmsg.LogError(0, RS_RET_ERR_WRKDIR, "$WorkDirectory: %s not a directory - directive ignored", 
				pNewVal);
		ABORT_FINALIZE(RS_RET_ERR_WRKDIR);
	}

	free(pszWorkDir);
	pszWorkDir = pNewVal;

finalize_it:
	RETiRet;
}


static rsRetVal
setDebugFile(void __attribute__((unused)) *pVal, uchar *pNewVal)
{
	DEFiRet;
	dbgSetDebugFile(pNewVal);
	free(pNewVal);
	RETiRet;
}


static rsRetVal
setDebugLevel(void __attribute__((unused)) *pVal, int level)
{
	DEFiRet;
	dbgSetDebugLevel(level);
	dbgprintf("debug level %d set via config file\n", level);
	dbgprintf("This is rsyslog version " VERSION "\n");
	RETiRet;
}


/* return our local IP.
 * If no local IP is set, "127.0.0.1" is selected *and* set. This
 * is an intensional side effect that we do in order to keep things
 * consistent and avoid config errors (this will make us not accept
 * setting the local IP address once a module has obtained it - so
 * it forces the $LocalHostIPIF directive high up in rsyslog.conf)
 * rgerhards, 2012-03-21
 */
static prop_t*
GetLocalHostIP(void)
{
	if(propLocalIPIF == NULL)
		storeLocalHostIPIF((uchar*)"127.0.0.1");
	return(propLocalIPIF);
}


/* set our local hostname. Free previous hostname, if it was already set.
 * Note that we do now do this in a thread
 * "once in a lifetime" action which can not be undone. -- gerhards, 2009-07-20
 */
static rsRetVal
SetLocalHostName(uchar *newname)
{
	free(LocalHostName);
	LocalHostName = newname;
	return RS_RET_OK;
}


/* return our local hostname. if it is not set, "[localhost]" is returned
 */
static uchar*
GetLocalHostName(void)
{
	uchar *pszRet;

	if(LocalHostNameOverride != NULL) {
		pszRet = LocalHostNameOverride;
		goto done;
	}

	if(LocalHostName == NULL)
		pszRet = (uchar*) "[localhost]";
	else {
		if(GetPreserveFQDN() == 1)
			pszRet = LocalFQDNName;
		else
			pszRet = LocalHostName;
	}
done:
	return(pszRet);
}


/* set our local domain name. Free previous domain, if it was already set.
 */
static rsRetVal
SetLocalDomain(uchar *newname)
{
	free(LocalDomain);
	LocalDomain = newname;
	return RS_RET_OK;
}


/* return our local hostname. if it is not set, "[localhost]" is returned
 */
static uchar*
GetLocalDomain(void)
{
	return LocalDomain;
}


/* generate the local hostname property. This must be done after the hostname info
 * has been set as well as PreserveFQDN.
 * rgerhards, 2009-06-30
 */
static rsRetVal
GenerateLocalHostNameProperty(void)
{
	DEFiRet;
	uchar *pszName;

	if(propLocalHostName != NULL)
		prop.Destruct(&propLocalHostName);

	CHKiRet(prop.Construct(&propLocalHostName));
	if(LocalHostNameOverride == NULL) {
		if(LocalHostName == NULL)
			pszName = (uchar*) "[localhost]";
		else {
			if(GetPreserveFQDN() == 1)
				pszName = LocalFQDNName;
			else
				pszName = LocalHostName;
		}
	} else { /* local hostname is overriden via config */
		pszName = LocalHostNameOverride;
	}
	DBGPRINTF("GenerateLocalHostName uses '%s'\n", pszName);
	CHKiRet(prop.SetString(propLocalHostName, pszName, ustrlen(pszName)));
	CHKiRet(prop.ConstructFinalize(propLocalHostName));

finalize_it:
	RETiRet;
}


/* return our local hostname as a string property
 */
static prop_t*
GetLocalHostNameProp(void)
{
	return(propLocalHostName);
}


static rsRetVal
SetLocalFQDNName(uchar *newname)
{
	free(LocalFQDNName);
	LocalFQDNName = newname;
	return RS_RET_OK;
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
	pIf->GenerateLocalHostNameProperty = GenerateLocalHostNameProperty;
	pIf->GetLocalHostNameProp = GetLocalHostNameProp;
	pIf->GetLocalHostIP = GetLocalHostIP;
	pIf->SetGlobalInputTermination = SetGlobalInputTermination;
	pIf->GetGlobalInputTermState = GetGlobalInputTermState;
#define SIMP_PROP(name) \
	pIf->Get##name = Get##name; \
	pIf->Set##name = Set##name;
	SIMP_PROP(MaxLine);
	SIMP_PROP(OptimizeUniProc);
	SIMP_PROP(ParseHOSTNAMEandTAG);
	SIMP_PROP(PreserveFQDN);
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
#ifdef USE_UNLIMITED_SELECT
	SIMP_PROP(FdSetSize)
#endif
#undef	SIMP_PROP
finalize_it:
ENDobjQueryInterface(glbl)


/* Reset config variables to default values.
 * rgerhards, 2008-04-17
 */
static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	free(pszDfltNetstrmDrvr);
	pszDfltNetstrmDrvr = NULL;
	free(pszDfltNetstrmDrvrCAF);
	pszDfltNetstrmDrvrCAF = NULL;
	free(pszDfltNetstrmDrvrKeyFile);
	pszDfltNetstrmDrvrKeyFile = NULL;
	free(pszDfltNetstrmDrvrCertFile);
	pszDfltNetstrmDrvrCertFile = NULL;
	free(LocalHostNameOverride);
	LocalHostNameOverride = NULL;
	free(pszWorkDir);
	pszWorkDir = NULL;
	bDropMalPTRMsgs = 0;
	bOptimizeUniProc = 1;
	bPreserveFQDN = 0;
	iMaxLine = 8192;
#ifdef USE_UNLIMITED_SELECT
	iFdSetSize = howmany(FD_SETSIZE, __NFDBITS) * sizeof (fd_mask);
#endif
	return RS_RET_OK;
}


/* Prepare for new config
 */
void
glblPrepCnf(void)
{
	free(cnfparamvals);
	cnfparamvals = NULL;
}

/* handle a global config object. Note that multiple global config statements
 * are permitted (because of plugin support), so once we got a param block,
 * we need to hold to it.
 * rgerhards, 2011-07-19
 */
void
glblProcessCnf(struct cnfobj *o)
{
	cnfparamvals = nvlstGetParams(o->nvlst, &paramblk, cnfparamvals);
	dbgprintf("glbl param blk after glblProcessCnf:\n");
	cnfparamsPrint(&paramblk, cnfparamvals);
}

void
glblDoneLoadCnf(void)
{
	int i;
	unsigned char *cstr;

	if(cnfparamvals == NULL)
		goto finalize_it;

	for(i = 0 ; i < paramblk.nParams ; ++i) {
		if(!cnfparamvals[i].bUsed)
			continue;
		if(!strcmp(paramblk.descr[i].name, "workdirectory")) {
			cstr = (uchar*) es_str2cstr(cnfparamvals[i].val.d.estr, NULL);
			setWorkDir(NULL, cstr);
		} else if(!strcmp(paramblk.descr[i].name, "localhostname")) {
			free(LocalHostNameOverride);
			LocalHostNameOverride = (uchar*)
				es_str2cstr(cnfparamvals[i].val.d.estr, NULL);
		} else if(!strcmp(paramblk.descr[i].name, "defaultnetstreamdriverkeyfile")) {
			free(pszDfltNetstrmDrvrKeyFile);
			pszDfltNetstrmDrvrKeyFile = (uchar*)
				es_str2cstr(cnfparamvals[i].val.d.estr, NULL);
		} else if(!strcmp(paramblk.descr[i].name, "defaultnetstreamdrivercafile")) {
			free(pszDfltNetstrmDrvrCAF);
			pszDfltNetstrmDrvrCAF = (uchar*)
				es_str2cstr(cnfparamvals[i].val.d.estr, NULL);
		} else if(!strcmp(paramblk.descr[i].name, "defaultnetstreamdriver")) {
			free(pszDfltNetstrmDrvr);
			pszDfltNetstrmDrvr = (uchar*)
				es_str2cstr(cnfparamvals[i].val.d.estr, NULL);
		} else if(!strcmp(paramblk.descr[i].name, "preservefqdn")) {
			bPreserveFQDN = (int) cnfparamvals[i].val.d.n;
		} else if(!strcmp(paramblk.descr[i].name,
				"dropmsgswithmaliciousdnsptrrecords")) {
			bDropMalPTRMsgs = (int) cnfparamvals[i].val.d.n;
		} else if(!strcmp(paramblk.descr[i].name, "maxmessagesize")) {
			iMaxLine = (int) cnfparamvals[i].val.d.n;
		} else {
			dbgprintf("glblDoneLoadCnf: program error, non-handled "
			  "param '%s'\n", paramblk.descr[i].name);
		}
	}
finalize_it:	;
}


/* Initialize the glbl class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINAbstractObjClassInit(glbl, 1, OBJ_IS_CORE_MODULE) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(prop, CORE_COMPONENT));
	CHKiRet(objUse(errmsg, CORE_COMPONENT));

	/* config handlers are never unregistered and need not be - we are always loaded ;) */
	CHKiRet(regCfSysLineHdlr((uchar *)"debugfile", 0, eCmdHdlrGetWord, setDebugFile, NULL, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"debuglevel", 0, eCmdHdlrInt, setDebugLevel, NULL, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"workdirectory", 0, eCmdHdlrGetWord, setWorkDir, NULL, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"dropmsgswithmaliciousdnsptrrecords", 0, eCmdHdlrBinary, NULL, &bDropMalPTRMsgs, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"defaultnetstreamdriver", 0, eCmdHdlrGetWord, NULL, &pszDfltNetstrmDrvr, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"defaultnetstreamdrivercafile", 0, eCmdHdlrGetWord, NULL, &pszDfltNetstrmDrvrCAF, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"defaultnetstreamdriverkeyfile", 0, eCmdHdlrGetWord, NULL, &pszDfltNetstrmDrvrKeyFile, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"defaultnetstreamdrivercertfile", 0, eCmdHdlrGetWord, NULL, &pszDfltNetstrmDrvrCertFile, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"localhostname", 0, eCmdHdlrGetWord, NULL, &LocalHostNameOverride, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"localhostipif", 0, eCmdHdlrGetWord, setLocalHostIPIF, NULL, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"optimizeforuniprocessor", 0, eCmdHdlrBinary, NULL, &bOptimizeUniProc, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"preservefqdn", 0, eCmdHdlrBinary, NULL, &bPreserveFQDN, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"maxmessagesize", 0, eCmdHdlrSize,
		NULL, &iMaxLine, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL, NULL));

	INIT_ATOMIC_HELPER_MUT(mutTerminateInputs);
ENDObjClassInit(glbl)


/* Exit the glbl class.
 * rgerhards, 2008-04-17
 */
BEGINObjClassExit(glbl, OBJ_IS_CORE_MODULE) /* class, version */
	free(pszDfltNetstrmDrvr);
	free(pszDfltNetstrmDrvrCAF);
	free(pszDfltNetstrmDrvrKeyFile);
	free(pszDfltNetstrmDrvrCertFile);
	free(pszWorkDir);
	free(LocalDomain);
	free(LocalHostName);
	free(LocalHostNameOverride);
	free(LocalFQDNName);
	objRelease(prop, CORE_COMPONENT);
	DESTROY_ATOMIC_HELPER_MUT(mutTerminateInputs);
ENDObjClassExit(glbl)

void glblProcessCnf(struct cnfobj *o);

/* vi:set ai:
 */
