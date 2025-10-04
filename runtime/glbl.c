/* glbl.c - this module holds global defintions and data items.
 * These are shared among the runtime library. Their use should be
 * limited to cases where it is actually needed. The main intension for
 * implementing them was support for the transistion from v2 to v4
 * (with fully modular design), but it turned out that there may also
 * be some other good use cases besides backwards-compatibility.
 *
 * Module begun 2008-04-16 by Rainer Gerhards
 *
 * Copyright 2008-2024 Rainer Gerhards and Adiscon GmbH.
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
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <ctype.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <json.h>

#include "rsyslog.h"
#include "obj.h"
#include "unicode-helper.h"
#include "cfsysline.h"
#include "glbl.h"
#include "prop.h"
#include "atomic.h"
#include "errmsg.h"
#include "action.h"
#include "parserif.h"
#include "rainerscript.h"
#include "srUtils.h"
#include "operatingstate.h"
#include "net.h"
#include "rsconf.h"
#include "queue.h"
#include "dnscache.h"
#include "parser.h"
#include "timezones.h"

/* some defaults */
#ifndef DFLT_NETSTRM_DRVR
    #define DFLT_NETSTRM_DRVR ((uchar *)"ptcp")
#endif

/* static data */
DEFobjStaticHelpers;
DEFobjCurrIf(prop) DEFobjCurrIf(net)

    /* static data
     * For this object, these variables are obviously what makes the "meat" of the
     * class...
     */

    static struct cnfobj *mainqCnfObj = NULL; /* main queue object, to be used later in startup sequence */
static int bPreserveFQDN = 0; /* should FQDNs always be preserved? */
static prop_t *propLocalIPIF = NULL; /* IP address to report for the local host (default is 127.0.0.1) */
static int propLocalIPIF_set = 0; /* is propLocalIPIF already set? */
static prop_t *propLocalHostName = NULL; /* our hostname as FQDN - read-only after startup */
static prop_t *propLocalHostNameToDelete = NULL; /* see GenerateLocalHostName function hdr comment! */
static uchar *LocalHostName = NULL; /* our hostname  - read-only after startup, except HUP */
static uchar *LocalHostNameOverride = NULL; /* user-overridden hostname - read-only after startup */
static uchar *LocalFQDNName = NULL; /* our hostname as FQDN - read-only after startup, except HUP */
static uchar *LocalDomain = NULL; /* our local domain name  - read-only after startup, except HUP */
static int iMaxLine = 8096;
int bTerminateInputs = 0; /* global switch that inputs shall terminate ASAP (1=> terminate) */
int glblUnloadModules = 1;
int glblAbortOnProgramError = 0;
char **glblDbgFiles = NULL;
size_t glblDbgFilesNum = 0;
int glblDbgWhitelist = 1;
int glblPermitCtlC = 0;
/* For global option CompactJsonString:
 *   Compact the JSON variable string, without extra space.
 *   Considering compatibility issues, the default options(CompactJsonString = "off")
 *   keep the same as before.
 */
int glblJsonFormatOpt = JSON_C_TO_STRING_SPACED;


pid_t glbl_ourpid;
#ifndef HAVE_ATOMIC_BUILTINS
DEF_ATOMIC_HELPER_MUT(mutTerminateInputs);
#endif
#ifdef USE_UNLIMITED_SELECT
static int iFdSetSize = howmany(FD_SETSIZE, __NFDBITS) * sizeof(fd_mask); /* size of select() bitmask in bytes */
#endif
static uchar *SourceIPofLocalClient = NULL; /* [ar] Source IP for local client to be used on multihomed host */
static char *internalMsgRatelimitName = NULL; /* shared ratelimit reference for internal messages */
static sbool internalMsgRlParamsUsed = 0; /* track inline ratelimit.* usage for internal messages */

/* tables for interfacing with the v6 config system */
static struct cnfparamdescr cnfparamdescr[] = {
    {"workdirectory", eCmdHdlrString, 0},
    {"operatingstatefile", eCmdHdlrString, 0},
    {"dropmsgswithmaliciousdnsptrrecords", eCmdHdlrBinary, 0},
    {"localhostname", eCmdHdlrGetWord, 0},
    {"compactjsonstring", eCmdHdlrBinary, 0},
    {"preservefqdn", eCmdHdlrBinary, 0},
    {"debug.onshutdown", eCmdHdlrBinary, 0},
    {"debug.logfile", eCmdHdlrString, 0},
    {"debug.gnutls", eCmdHdlrNonNegInt, 0},
    {"debug.abortonprogramerror", eCmdHdlrBinary, 0},
    {"debug.unloadmodules", eCmdHdlrBinary, 0},
    {"defaultnetstreamdrivercafile", eCmdHdlrString, 0},
    {"defaultnetstreamdrivercrlfile", eCmdHdlrString, 0},
    {"defaultnetstreamdriverkeyfile", eCmdHdlrString, 0},
    {"defaultnetstreamdrivercertfile", eCmdHdlrString, 0},
    {"defaultnetstreamdriver", eCmdHdlrString, 0},
    {"defaultopensslengine", eCmdHdlrString, 0},
    {"netstreamdrivercaextrafiles", eCmdHdlrString, 0},
    {"maxmessagesize", eCmdHdlrSize, 0},
    {"oversizemsg.errorfile", eCmdHdlrGetWord, 0},
    {"oversizemsg.report", eCmdHdlrBinary, 0},
    {"oversizemsg.input.mode", eCmdHdlrGetWord, 0},
    {"reportchildprocessexits", eCmdHdlrGetWord, 0},
    {"action.reportsuspension", eCmdHdlrBinary, 0},
    {"action.reportsuspensioncontinuation", eCmdHdlrBinary, 0},
    {"parser.controlcharacterescapeprefix", eCmdHdlrGetChar, 0},
    {"parser.droptrailinglfonreception", eCmdHdlrBinary, 0},
    {"parser.escapecontrolcharactersonreceive", eCmdHdlrBinary, 0},
    {"parser.spacelfonreceive", eCmdHdlrBinary, 0},
    {"parser.escape8bitcharactersonreceive", eCmdHdlrBinary, 0},
    {"parser.escapecontrolcharactertab", eCmdHdlrBinary, 0},
    {"parser.escapecontrolcharacterscstyle", eCmdHdlrBinary, 0},
    {"parser.parsehostnameandtag", eCmdHdlrBinary, 0},
    {"parser.permitslashinprogramname", eCmdHdlrBinary, 0},
    {"stdlog.channelspec", eCmdHdlrString, 0},
    {"janitor.interval", eCmdHdlrPositiveInt, 0},
    {"senders.reportnew", eCmdHdlrBinary, 0},
    {"senders.reportgoneaway", eCmdHdlrBinary, 0},
    {"senders.timeoutafter", eCmdHdlrPositiveInt, 0},
    {"senders.keeptrack", eCmdHdlrBinary, 0},
    {"inputs.timeout.shutdown", eCmdHdlrPositiveInt, 0},
    {"privdrop.group.keepsupplemental", eCmdHdlrBinary, 0},
    {"privdrop.group.id", eCmdHdlrPositiveInt, 0},
    {"privdrop.group.name", eCmdHdlrGID, 0},
    {"privdrop.user.id", eCmdHdlrPositiveInt, 0},
    {"privdrop.user.name", eCmdHdlrUID, 0},
    {"net.ipprotocol", eCmdHdlrGetWord, 0},
    {"net.acladdhostnameonfail", eCmdHdlrBinary, 0},
    {"net.aclresolvehostname", eCmdHdlrBinary, 0},
    {"net.enabledns", eCmdHdlrBinary, 0},
    {"net.permitACLwarning", eCmdHdlrBinary, 0},
    {"abortonuncleanconfig", eCmdHdlrBinary, 0},
    {"abortonfailedqueuestartup", eCmdHdlrBinary, 0},
    {"variables.casesensitive", eCmdHdlrBinary, 0},
    {"environment", eCmdHdlrArray, 0},
    {"processinternalmessages", eCmdHdlrBinary, 0},
    {"umask", eCmdHdlrFileCreateMode, 0},
    {"security.abortonidresolutionfail", eCmdHdlrBinary, 0},
    {"internal.developeronly.options", eCmdHdlrInt, 0},
    {"internalmsg.ratelimit.interval", eCmdHdlrPositiveInt, 0},
    {"internalmsg.ratelimit.burst", eCmdHdlrPositiveInt, 0},
    {"internalmsg.ratelimit.name", eCmdHdlrString, 0},
    {"internalmsg.severity", eCmdHdlrSeverity, 0},
    {"allmessagestostderr", eCmdHdlrBinary, 0},
    {"errormessagestostderr.maxnumber", eCmdHdlrPositiveInt, 0},
    {"shutdown.enable.ctlc", eCmdHdlrBinary, 0},
    {"default.action.queue.timeoutshutdown", eCmdHdlrInt, 0},
    {"default.action.queue.timeoutactioncompletion", eCmdHdlrInt, 0},
    {"default.action.queue.timeoutenqueue", eCmdHdlrInt, 0},
    {"default.action.queue.timeoutworkerthreadshutdown", eCmdHdlrInt, 0},
    {"default.ruleset.queue.timeoutshutdown", eCmdHdlrInt, 0},
    {"default.ruleset.queue.timeoutactioncompletion", eCmdHdlrInt, 0},
    {"default.ruleset.queue.timeoutenqueue", eCmdHdlrInt, 0},
    {"default.ruleset.queue.timeoutworkerthreadshutdown", eCmdHdlrInt, 0},
    {"reverselookup.cache.ttl.default", eCmdHdlrNonNegInt, 0},
    {"reverselookup.cache.ttl.enable", eCmdHdlrBinary, 0},
    {"parser.supportcompressionextension", eCmdHdlrBinary, 0},
    {"shutdown.queue.doublesize", eCmdHdlrBinary, 0},
    {"debug.files", eCmdHdlrArray, 0},
    {"debug.whitelist", eCmdHdlrBinary, 0},
    {"libcapng.default", eCmdHdlrBinary, 0},
    {"libcapng.enable", eCmdHdlrBinary, 0},
};
static struct cnfparamblk paramblk = {CNFPARAMBLK_VERSION, sizeof(cnfparamdescr) / sizeof(struct cnfparamdescr),
                                      cnfparamdescr};

static struct cnfparamvals *cnfparamvals = NULL;
/* we need to support multiple calls into our param block, so we need
 * to persist the current settings. Note that this must be re-set
 * each time a new config load begins. This is a hint if we will
 * ever implement multi-config support, which is just an idea right now.
 */

int glblGetMaxLine(rsconf_t *cnf) {
    /* glblGetMaxLine might be invoked before our configuration exists */
    return ((cnf != NULL) ? cnf->globals.iMaxLine : iMaxLine);
}


int GetGnuTLSLoglevel(rsconf_t *cnf) {
    return (cnf->globals.iGnuTLSLoglevel);
}

/* define a macro for the simple properties' set and get functions
 * (which are always the same). This is only suitable for pretty
 * simple cases which require neither checks nor memory allocation.
 */
#define SIMP_PROP(nameFunc, nameVar, dataType) \
    SIMP_PROP_GET(nameFunc, nameVar, dataType) \
    SIMP_PROP_SET(nameFunc, nameVar, dataType)
#define SIMP_PROP_SET(nameFunc, nameVar, dataType)   \
    static rsRetVal Set##nameFunc(dataType newVal) { \
        nameVar = newVal;                            \
        return RS_RET_OK;                            \
    }
#define SIMP_PROP_GET(nameFunc, nameVar, dataType) \
    static dataType Get##nameFunc(void) {          \
        return (nameVar);                          \
    }

SIMP_PROP(PreserveFQDN, bPreserveFQDN, int)
SIMP_PROP(mainqCnfObj, mainqCnfObj, struct cnfobj *)
#ifdef USE_UNLIMITED_SELECT
SIMP_PROP(FdSetSize, iFdSetSize, int)
#endif

#undef SIMP_PROP
#undef SIMP_PROP_SET
#undef SIMP_PROP_GET

/* This is based on the previous SIMP_PROP but as a getter it uses
 * additional parameter specifying the configuration it belongs to.
 * The setter uses loadConf
 */
#define SIMP_PROP(nameFunc, nameVar, dataType) \
    SIMP_PROP_GET(nameFunc, nameVar, dataType) \
    SIMP_PROP_SET(nameFunc, nameVar, dataType)
#define SIMP_PROP_SET(nameFunc, nameVar, dataType)   \
    static rsRetVal Set##nameFunc(dataType newVal) { \
        loadConf->globals.nameVar = newVal;          \
        return RS_RET_OK;                            \
    }
#define SIMP_PROP_GET(nameFunc, nameVar, dataType) \
    static dataType Get##nameFunc(rsconf_t *cnf) { \
        return (cnf->globals.nameVar);             \
    }

SIMP_PROP(DropMalPTRMsgs, bDropMalPTRMsgs, int)
SIMP_PROP(DisableDNS, bDisableDNS, int)
SIMP_PROP(ParserEscapeControlCharactersCStyle, parser.bParserEscapeCCCStyle, int)
SIMP_PROP(ParseHOSTNAMEandTAG, parser.bParseHOSTNAMEandTAG, int)
SIMP_PROP(OptionDisallowWarning, optionDisallowWarning, int)
/* We omit setter on purpose, because we want to customize it */
SIMP_PROP_GET(DfltNetstrmDrvrCAF, pszDfltNetstrmDrvrCAF, uchar *)
SIMP_PROP_GET(DfltNetstrmDrvrCRLF, pszDfltNetstrmDrvrCRLF, uchar *)
SIMP_PROP_GET(DfltNetstrmDrvrCertFile, pszDfltNetstrmDrvrCertFile, uchar *)
SIMP_PROP_GET(DfltNetstrmDrvrKeyFile, pszDfltNetstrmDrvrKeyFile, uchar *)
SIMP_PROP_GET(NetstrmDrvrCAExtraFiles, pszNetstrmDrvrCAExtraFiles, uchar *)
SIMP_PROP_GET(ParserControlCharacterEscapePrefix, parser.cCCEscapeChar, uchar)
SIMP_PROP_GET(ParserDropTrailingLFOnReception, parser.bDropTrailingLF, int)
SIMP_PROP_GET(ParserEscapeControlCharactersOnReceive, parser.bEscapeCCOnRcv, int)
SIMP_PROP_GET(ParserSpaceLFOnReceive, parser.bSpaceLFOnRcv, int)
SIMP_PROP_GET(ParserEscape8BitCharactersOnReceive, parser.bEscape8BitChars, int)
SIMP_PROP_GET(ParserEscapeControlCharacterTab, parser.bEscapeTab, int)

#undef SIMP_PROP
#undef SIMP_PROP_SET
#undef SIMP_PROP_GET

/* return global input termination status
 * rgerhards, 2009-07-20
 */
static int GetGlobalInputTermState(void) {
    return ATOMIC_FETCH_32BIT(&bTerminateInputs, &mutTerminateInputs);
}


/* set global termination state to "terminate". Note that this is a
 * "once in a lifetime" action which can not be undone. -- gerhards, 2009-07-20
 */
static void SetGlobalInputTermination(void) {
    ATOMIC_STORE_1_TO_INT(&bTerminateInputs, &mutTerminateInputs);
}


/* set the local host IP address to a specific string. Helper to
 * small set of functions. No checks done, caller must ensure it is
 * ok to call. Most importantly, the IP address must not already have
 * been set. -- rgerhards, 2012-03-21
 */
static rsRetVal storeLocalHostIPIF(uchar *myIP) {
    DEFiRet;
    if (propLocalIPIF != NULL) {
        CHKiRet(prop.Destruct(&propLocalIPIF));
    }
    CHKiRet(prop.Construct(&propLocalIPIF));
    CHKiRet(prop.SetString(propLocalIPIF, myIP, ustrlen(myIP)));
    CHKiRet(prop.ConstructFinalize(propLocalIPIF));
    DBGPRINTF("rsyslog/glbl: using '%s' as localhost IP\n", myIP);
finalize_it:
    RETiRet;
}


/* This function is used to set the IP address that is to be
 * reported for the local host. Note that in order to ease things
 * for the v6 config interface, we do not allow this to be set more
 * than once.
 * rgerhards, 2012-03-21
 */
static rsRetVal setLocalHostIPIF(void __attribute__((unused)) * pVal, uchar *pNewVal) {
    uchar myIP[128];
    rsRetVal localRet;
    DEFiRet;

    CHKiRet(objUse(net, CORE_COMPONENT));

    if (propLocalIPIF_set) {
        LogError(0, RS_RET_ERR,
                 "$LocalHostIPIF is already set "
                 "and cannot be reset; place it at TOP OF rsyslog.conf!");
        ABORT_FINALIZE(RS_RET_ERR);
    }

    localRet = net.GetIFIPAddr(pNewVal, AF_UNSPEC, myIP, (int)sizeof(myIP));
    if (localRet != RS_RET_OK) {
        LogError(0, RS_RET_ERR,
                 "$LocalHostIPIF: IP address for interface "
                 "'%s' cannnot be obtained - ignoring directive",
                 pNewVal);
    } else {
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
static rsRetVal setWorkDir(void __attribute__((unused)) * pVal, uchar *pNewVal) {
    size_t lenDir;
    int i;
    struct stat sb;
    DEFiRet;

    /* remove trailing slashes */
    lenDir = ustrlen(pNewVal);
    i = lenDir - 1;
    while (i > 0 && pNewVal[i] == '/') {
        --i;
    }

    if (i < 0) {
        LogError(0, RS_RET_ERR_WRKDIR,
                 "$WorkDirectory: empty value "
                 "- directive ignored");
        ABORT_FINALIZE(RS_RET_ERR_WRKDIR);
    }

    if (i != (int)lenDir - 1) {
        pNewVal[i + 1] = '\0';
        LogError(0, RS_RET_WRN_WRKDIR,
                 "$WorkDirectory: trailing slashes "
                 "removed, new value is '%s'",
                 pNewVal);
    }

    if (stat((char *)pNewVal, &sb) != 0) {
        LogError(0, RS_RET_ERR_WRKDIR,
                 "$WorkDirectory: %s can not be "
                 "accessed, probably does not exist - directive ignored",
                 pNewVal);
        ABORT_FINALIZE(RS_RET_ERR_WRKDIR);
    }

    if (!S_ISDIR(sb.st_mode)) {
        LogError(0, RS_RET_ERR_WRKDIR, "$WorkDirectory: %s not a directory - directive ignored", pNewVal);
        ABORT_FINALIZE(RS_RET_ERR_WRKDIR);
    }

    free(loadConf->globals.pszWorkDir);
    loadConf->globals.pszWorkDir = pNewVal;

finalize_it:
    RETiRet;
}


static rsRetVal setDfltNetstrmDrvrCAF(void __attribute__((unused)) * pVal, uchar *pNewVal) {
    DEFiRet;
    FILE *fp;
    free(loadConf->globals.pszDfltNetstrmDrvrCAF);
    loadConf->globals.pszDfltNetstrmDrvrCAF = pNewVal;
    fp = fopen((const char *)pNewVal, "r");
    if (fp == NULL) {
        LogError(errno, RS_RET_NO_FILE_ACCESS,
                 "error: defaultnetstreamdrivercafile file '%s' "
                 "could not be accessed",
                 pNewVal);
    } else {
        fclose(fp);
    }

    RETiRet;
}

static rsRetVal setNetstrmDrvrCAExtraFiles(void __attribute__((unused)) * pVal, uchar *pNewVal) {
    DEFiRet;
    FILE *fp;
    char *token;
    char *saveptr;
    int error = 0;
    char *valCopy = NULL;

    free(loadConf->globals.pszNetstrmDrvrCAExtraFiles);

    /* Validate files without modifying the original comma-separated string */
    if (pNewVal != NULL) {
        valCopy = strdup((const char *)pNewVal);
        if (valCopy == NULL) {
            LogError(errno, RS_RET_OUT_OF_MEMORY, "error: strdup failed in setNetstrmDrvrCAExtraFiles");
            free(pNewVal);
            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        }
        for (token = strtok_r(valCopy, ",", &saveptr); token != NULL; token = strtok_r(NULL, ",", &saveptr)) {
            while (isspace((unsigned char)*token)) {
                token++;
            }
            char *end = token + strlen(token) - 1;
            while (end > token && isspace((unsigned char)*end)) {
                end--;
            }
            *(end + 1) = '\0';

            if (*token == '\0') {
                continue;
            }

            fp = fopen((const char *)token, "r");
            if (fp == NULL) {
                LogError(errno, RS_RET_NO_FILE_ACCESS,
                         "error: netstreamdrivercaextrafiles file '%s' "
                         "could not be accessed",
                         token);
                error = 1;
            } else {
                fclose(fp);
            }
        }
    }

    if (!error) {
        loadConf->globals.pszNetstrmDrvrCAExtraFiles = pNewVal;
    } else {
        /* Validation failed: prevent leak of newly allocated pNewVal */
        free(pNewVal);
        loadConf->globals.pszNetstrmDrvrCAExtraFiles = NULL;
    }

finalize_it:
    if (valCopy != NULL) {
        free(valCopy);
    }
    RETiRet;
}

static rsRetVal setDfltNetstrmDrvrCRLF(void __attribute__((unused)) * pVal, uchar *pNewVal) {
    DEFiRet;
    FILE *fp;
    free(loadConf->globals.pszDfltNetstrmDrvrCRLF);
    loadConf->globals.pszDfltNetstrmDrvrCRLF = pNewVal;
    fp = fopen((const char *)pNewVal, "r");
    if (fp == NULL) {
        LogError(errno, RS_RET_NO_FILE_ACCESS,
                 "error: defaultnetstreamdrivercrlfile file '%s' "
                 "could not be accessed",
                 pNewVal);
    } else {
        fclose(fp);
    }

    RETiRet;
}


static rsRetVal setDfltNetstrmDrvrCertFile(void __attribute__((unused)) * pVal, uchar *pNewVal) {
    DEFiRet;
    FILE *fp;

    free(loadConf->globals.pszDfltNetstrmDrvrCertFile);
    loadConf->globals.pszDfltNetstrmDrvrCertFile = pNewVal;
    fp = fopen((const char *)pNewVal, "r");
    if (fp == NULL) {
        LogError(errno, RS_RET_NO_FILE_ACCESS,
                 "error: defaultnetstreamdrivercertfile '%s' "
                 "could not be accessed",
                 pNewVal);
    } else {
        fclose(fp);
    }

    RETiRet;
}

static rsRetVal setDfltNetstrmDrvrKeyFile(void __attribute__((unused)) * pVal, uchar *pNewVal) {
    DEFiRet;
    FILE *fp;

    free(loadConf->globals.pszDfltNetstrmDrvrKeyFile);
    loadConf->globals.pszDfltNetstrmDrvrKeyFile = pNewVal;
    fp = fopen((const char *)pNewVal, "r");
    if (fp == NULL) {
        LogError(errno, RS_RET_NO_FILE_ACCESS,
                 "error: defaultnetstreamdriverkeyfile '%s' "
                 "could not be accessed",
                 pNewVal);
    } else {
        fclose(fp);
    }

    RETiRet;
}

static rsRetVal setDfltNetstrmDrvr(void __attribute__((unused)) * pVal, uchar *pNewVal) {
    DEFiRet;
    free(loadConf->globals.pszDfltNetstrmDrvr);
    loadConf->globals.pszDfltNetstrmDrvr = pNewVal;
    RETiRet;
}

static rsRetVal setDfltOpensslEngine(void __attribute__((unused)) * pVal, uchar *pNewVal) {
    DEFiRet;
    free(loadConf->globals.pszDfltOpensslEngine);
    loadConf->globals.pszDfltOpensslEngine = pNewVal;
    RETiRet;
}


static rsRetVal setParserControlCharacterEscapePrefix(void __attribute__((unused)) * pVal, uchar *pNewVal) {
    DEFiRet;
    loadConf->globals.parser.cCCEscapeChar = *pNewVal;
    RETiRet;
}

static rsRetVal setParserDropTrailingLFOnReception(void __attribute__((unused)) * pVal, int pNewVal) {
    DEFiRet;
    loadConf->globals.parser.bDropTrailingLF = pNewVal;
    RETiRet;
}

static rsRetVal setParserEscapeControlCharactersOnReceive(void __attribute__((unused)) * pVal, int pNewVal) {
    DEFiRet;
    loadConf->globals.parser.bEscapeCCOnRcv = pNewVal;
    RETiRet;
}

static rsRetVal setParserSpaceLFOnReceive(void __attribute__((unused)) * pVal, int pNewVal) {
    DEFiRet;
    loadConf->globals.parser.bSpaceLFOnRcv = pNewVal;
    RETiRet;
}

static rsRetVal setParserEscape8BitCharactersOnReceive(void __attribute__((unused)) * pVal, int pNewVal) {
    DEFiRet;
    loadConf->globals.parser.bEscape8BitChars = pNewVal;
    RETiRet;
}

static rsRetVal setParserEscapeControlCharacterTab(void __attribute__((unused)) * pVal, int pNewVal) {
    DEFiRet;
    loadConf->globals.parser.bEscapeTab = pNewVal;
    RETiRet;
}

/* This function is used both by legacy and RainerScript conf. It is a real setter. */
static void setMaxLine(const int64_t iNew) {
    if (iNew < 128) {
        LogError(0, RS_RET_INVALID_VALUE,
                 "maxMessageSize tried to set "
                 "to %lld, but cannot be less than 128 - set to 128 "
                 "instead",
                 (long long)iNew);
        loadConf->globals.iMaxLine = 128;
    } else if (iNew > (int64_t)INT_MAX) {
        LogError(0, RS_RET_INVALID_VALUE,
                 "maxMessageSize larger than "
                 "INT_MAX (%d) - reduced to INT_MAX",
                 INT_MAX);
        loadConf->globals.iMaxLine = INT_MAX;
    } else {
        loadConf->globals.iMaxLine = (int)iNew;
    }
}


static rsRetVal legacySetMaxMessageSize(void __attribute__((unused)) * pVal, int64_t iNew) {
    setMaxLine(iNew);
    return RS_RET_OK;
}

static rsRetVal setDebugFile(void __attribute__((unused)) * pVal, uchar *pNewVal) {
    DEFiRet;
    dbgSetDebugFile(pNewVal);
    free(pNewVal);
    RETiRet;
}

static rsRetVal setDebugLevel(void __attribute__((unused)) * pVal, int level) {
    DEFiRet;
    dbgSetDebugLevel(level);
    dbgprintf("debug level %d set via config file\n", level);
    dbgprintf("This is rsyslog version " VERSION "\n");
    RETiRet;
}

static rsRetVal ATTR_NONNULL() setOversizeMsgInputMode(const uchar *const mode) {
    DEFiRet;
    if (!strcmp((char *)mode, "truncate")) {
        loadConf->globals.oversizeMsgInputMode = glblOversizeMsgInputMode_Truncate;
    } else if (!strcmp((char *)mode, "split")) {
        loadConf->globals.oversizeMsgInputMode = glblOversizeMsgInputMode_Split;
    } else if (!strcmp((char *)mode, "accept")) {
        loadConf->globals.oversizeMsgInputMode = glblOversizeMsgInputMode_Accept;
    } else {
        loadConf->globals.oversizeMsgInputMode = glblOversizeMsgInputMode_Truncate;
    }
    RETiRet;
}

static rsRetVal ATTR_NONNULL() setReportChildProcessExits(const uchar *const mode) {
    DEFiRet;
    if (!strcmp((char *)mode, "none")) {
        loadConf->globals.reportChildProcessExits = REPORT_CHILD_PROCESS_EXITS_NONE;
    } else if (!strcmp((char *)mode, "errors")) {
        loadConf->globals.reportChildProcessExits = REPORT_CHILD_PROCESS_EXITS_ERRORS;
    } else if (!strcmp((char *)mode, "all")) {
        loadConf->globals.reportChildProcessExits = REPORT_CHILD_PROCESS_EXITS_ALL;
    } else {
        LogError(0, RS_RET_CONF_PARAM_INVLD,
                 "invalid value '%s' for global parameter reportChildProcessExits -- ignored", mode);
        iRet = RS_RET_CONF_PARAM_INVLD;
    }
    RETiRet;
}

static int getDefPFFamily(rsconf_t *cnf) {
    return cnf->globals.iDefPFFamily;
}

/* return our local IP.
 * If no local IP is set, "127.0.0.1" is selected *and* set. This
 * is an intensional side effect that we do in order to keep things
 * consistent and avoid config errors (this will make us not accept
 * setting the local IP address once a module has obtained it - so
 * it forces the $LocalHostIPIF directive high up in rsyslog.conf)
 * rgerhards, 2012-03-21
 */
static prop_t *GetLocalHostIP(void) {
    assert(propLocalIPIF != NULL);
    return (propLocalIPIF);
}


/* set our local hostname. Free previous hostname, if it was already set.
 * Note that we do now do this in a thread. As such, the method here
 * is NOT 100% clean. If we run into issues, we need to think about
 * refactoring the whole way we handle the local host name processing.
 * Possibly using a PROP might be the right solution then.
 */
static rsRetVal SetLocalHostName(uchar *const newname) {
    uchar *toFree;
    if (LocalHostName == NULL || strcmp((const char *)LocalHostName, (const char *)newname)) {
        toFree = LocalHostName;
        LocalHostName = newname;
    } else {
        toFree = newname;
    }
    free(toFree);
    return RS_RET_OK;
}


/* return our local hostname. if it is not set, "[localhost]" is returned
 */
uchar *glblGetLocalHostName(void) {
    uchar *pszRet;

    if (LocalHostNameOverride != NULL) {
        pszRet = LocalHostNameOverride;
        goto done;
    }

    if (LocalHostName == NULL)
        pszRet = (uchar *)"[localhost]";
    else {
        if (GetPreserveFQDN() == 1)
            pszRet = LocalFQDNName;
        else
            pszRet = LocalHostName;
    }
done:
    return (pszRet);
}


/* return the name of the file where oversize messages are written to
 */
uchar *glblGetOversizeMsgErrorFile(rsconf_t *cnf) {
    return cnf->globals.oversizeMsgErrorFile;
}

const uchar *glblGetOperatingStateFile(rsconf_t *cnf) {
    return cnf->globals.operatingStateFile;
}

/* return the mode with which oversize messages will be put forward
 */
int glblGetOversizeMsgInputMode(rsconf_t *cnf) {
    return cnf->globals.oversizeMsgInputMode;
}

int glblReportOversizeMessage(rsconf_t *cnf) {
    return cnf->globals.reportOversizeMsg;
}


/* logs a message indicating that a child process has terminated.
 * If name != NULL, prints it as the program name.
 */
void glblReportChildProcessExit(rsconf_t *cnf, const uchar *name, pid_t pid, int status) {
    DBGPRINTF("waitpid for child %ld returned status: %2.2x\n", (long)pid, status);

    if (cnf->globals.reportChildProcessExits == REPORT_CHILD_PROCESS_EXITS_NONE ||
        (cnf->globals.reportChildProcessExits == REPORT_CHILD_PROCESS_EXITS_ERRORS && WIFEXITED(status) &&
         WEXITSTATUS(status) == 0)) {
        return;
    }

    if (WIFEXITED(status)) {
        int severity = WEXITSTATUS(status) == 0 ? LOG_INFO : LOG_WARNING;
        if (name != NULL) {
            LogMsg(0, NO_ERRCODE, severity, "program '%s' (pid %ld) exited with status %d", name, (long)pid,
                   WEXITSTATUS(status));
        } else {
            LogMsg(0, NO_ERRCODE, severity, "child process (pid %ld) exited with status %d", (long)pid,
                   WEXITSTATUS(status));
        }
    } else if (WIFSIGNALED(status)) {
        if (name != NULL) {
            LogMsg(0, NO_ERRCODE, LOG_WARNING, "program '%s' (pid %ld) terminated by signal %d", name, (long)pid,
                   WTERMSIG(status));
        } else {
            LogMsg(0, NO_ERRCODE, LOG_WARNING, "child process (pid %ld) terminated by signal %d", (long)pid,
                   WTERMSIG(status));
        }
    }
}


/* set our local domain name. Free previous domain, if it was already set.
 */
static rsRetVal SetLocalDomain(uchar *newname) {
    free(LocalDomain);
    LocalDomain = newname;
    return RS_RET_OK;
}


/* return our local hostname. if it is not set, "[localhost]" is returned
 */
static uchar *GetLocalDomain(void) {
    return LocalDomain;
}


/* generate the local hostname property. This must be done after the hostname info
 * has been set as well as PreserveFQDN.
 * rgerhards, 2009-06-30
 * NOTE: This function tries to avoid locking by not destructing the previous value
 * immediately. This is so that current readers can  continue to use the previous name.
 * Otherwise, we would need to use read/write locks to protect the update process.
 * In order to do so, we save the previous value and delete it when we are called again
 * the next time. Note that this in theory is racy and can lead to a double-free.
 * In practice, however, the window of exposure to trigger this is extremely short
 * and as this functions is very infrequently being called (on HUP), the trigger
 * condition for this bug is so highly unlikely that it never occurs in practice.
 * Probably if you HUP rsyslog every few milliseconds, but who does that...
 * To further reduce risk potential, we do only update the property when there
 * actually is a hostname change, which makes it even less likely.
 * rgerhards, 2013-10-28
 */
static rsRetVal GenerateLocalHostNameProperty(void) {
    uchar *pszPrev;
    int lenPrev;
    prop_t *hostnameNew;
    uchar *pszName;
    DEFiRet;

    if (propLocalHostNameToDelete != NULL) prop.Destruct(&propLocalHostNameToDelete);

    if (LocalHostNameOverride == NULL) {
        if (LocalHostName == NULL)
            pszName = (uchar *)"[localhost]";
        else {
            if (GetPreserveFQDN() == 1)
                pszName = LocalFQDNName;
            else
                pszName = LocalHostName;
        }
    } else { /* local hostname is overriden via config */
        pszName = LocalHostNameOverride;
    }
    DBGPRINTF("GenerateLocalHostName uses '%s'\n", pszName);

    if (propLocalHostName == NULL)
        pszPrev = (uchar *)""; /* make sure strcmp() below does not match */
    else
        prop.GetString(propLocalHostName, &pszPrev, &lenPrev);

    if (ustrcmp(pszPrev, pszName)) {
        /* we need to update */
        CHKiRet(prop.Construct(&hostnameNew));
        CHKiRet(prop.SetString(hostnameNew, pszName, ustrlen(pszName)));
        CHKiRet(prop.ConstructFinalize(hostnameNew));
        propLocalHostNameToDelete = propLocalHostName;
        propLocalHostName = hostnameNew;
    }

finalize_it:
    RETiRet;
}


/* return our local hostname as a string property
 */
static prop_t *GetLocalHostNameProp(void) {
    return (propLocalHostName);
}


static rsRetVal SetLocalFQDNName(uchar *newname) {
    free(LocalFQDNName);
    LocalFQDNName = newname;
    return RS_RET_OK;
}

/* return the current localhost name as FQDN (requires FQDN to be set)
 */
static uchar *GetLocalFQDNName(void) {
    return (LocalFQDNName == NULL ? (uchar *)"[localhost]" : LocalFQDNName);
}


/* return the current working directory */
static uchar *GetWorkDir(rsconf_t *cnf) {
    return (cnf->globals.pszWorkDir == NULL ? (uchar *)"" : cnf->globals.pszWorkDir);
}

/* return the "raw" working directory, which means
 * NULL if unset.
 */
const uchar *glblGetWorkDirRaw(rsconf_t *cnf) {
    return cnf->globals.pszWorkDir;
}

/* return the current default netstream driver */
static uchar *GetDfltNetstrmDrvr(rsconf_t *cnf) {
    return (cnf->globals.pszDfltNetstrmDrvr == NULL ? DFLT_NETSTRM_DRVR : cnf->globals.pszDfltNetstrmDrvr);
}

/* return the current default openssl engine name */
static uchar *GetDfltOpensslEngine(rsconf_t *cnf) {
    return (cnf->globals.pszDfltOpensslEngine);
}

/* [ar] Source IP for local client to be used on multihomed host */
static rsRetVal SetSourceIPofLocalClient(uchar *newname) {
    if (SourceIPofLocalClient != NULL) {
        free(SourceIPofLocalClient);
    }
    SourceIPofLocalClient = newname;
    return RS_RET_OK;
}

static uchar *GetSourceIPofLocalClient(void) {
    return (SourceIPofLocalClient);
}


/* queryInterface function
 * rgerhards, 2008-02-21
 */
BEGINobjQueryInterface(glbl)
    CODESTARTobjQueryInterface(glbl);
    if (pIf->ifVersion != glblCURR_IF_VERSION) { /* check for current version, increment on each change */
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
    pIf->GetSourceIPofLocalClient = GetSourceIPofLocalClient; /* [ar] */
    pIf->SetSourceIPofLocalClient = SetSourceIPofLocalClient; /* [ar] */
    pIf->GetDefPFFamily = getDefPFFamily;
    pIf->GetDisableDNS = GetDisableDNS;
    pIf->GetMaxLine = glblGetMaxLine;
    pIf->GetOptionDisallowWarning = GetOptionDisallowWarning;
    pIf->GetDfltNetstrmDrvrCAF = GetDfltNetstrmDrvrCAF;
    pIf->GetDfltNetstrmDrvrCRLF = GetDfltNetstrmDrvrCRLF;
    pIf->GetDfltNetstrmDrvrCertFile = GetDfltNetstrmDrvrCertFile;
    pIf->GetDfltNetstrmDrvrKeyFile = GetDfltNetstrmDrvrKeyFile;
    pIf->GetDfltNetstrmDrvr = GetDfltNetstrmDrvr;
    pIf->GetDfltOpensslEngine = GetDfltOpensslEngine;
    pIf->GetNetstrmDrvrCAExtraFiles = GetNetstrmDrvrCAExtraFiles;
    pIf->GetParserControlCharacterEscapePrefix = GetParserControlCharacterEscapePrefix;
    pIf->GetParserDropTrailingLFOnReception = GetParserDropTrailingLFOnReception;
    pIf->GetParserEscapeControlCharactersOnReceive = GetParserEscapeControlCharactersOnReceive;
    pIf->GetParserSpaceLFOnReceive = GetParserSpaceLFOnReceive;
    pIf->GetParserEscape8BitCharactersOnReceive = GetParserEscape8BitCharactersOnReceive;
    pIf->GetParserEscapeControlCharacterTab = GetParserEscapeControlCharacterTab;
    pIf->GetLocalHostName = glblGetLocalHostName;
    pIf->SetLocalHostName = SetLocalHostName;
#define SIMP_PROP(name)         \
    pIf->Get##name = Get##name; \
    pIf->Set##name = Set##name;
    SIMP_PROP(PreserveFQDN);
    SIMP_PROP(DropMalPTRMsgs);
    SIMP_PROP(mainqCnfObj);
    SIMP_PROP(LocalFQDNName)
    SIMP_PROP(LocalDomain)
    SIMP_PROP(ParserEscapeControlCharactersCStyle)
    SIMP_PROP(ParseHOSTNAMEandTAG)
#ifdef USE_UNLIMITED_SELECT
    SIMP_PROP(FdSetSize)
#endif
#undef SIMP_PROP
finalize_it:
ENDobjQueryInterface(glbl)

/* Reset config variables to default values.
 * rgerhards, 2008-04-17
 */
static rsRetVal resetConfigVariables(uchar __attribute__((unused)) * pp, void __attribute__((unused)) * pVal) {
    free(loadConf->globals.pszDfltNetstrmDrvr);
    loadConf->globals.pszDfltNetstrmDrvr = NULL;
    free(loadConf->globals.pszDfltNetstrmDrvrCAF);
    loadConf->globals.pszDfltNetstrmDrvrCAF = NULL;
    free(loadConf->globals.pszDfltNetstrmDrvrCRLF);
    loadConf->globals.pszDfltNetstrmDrvrCRLF = NULL;
    free(loadConf->globals.pszDfltNetstrmDrvrKeyFile);
    loadConf->globals.pszDfltNetstrmDrvrKeyFile = NULL;
    free(loadConf->globals.pszDfltNetstrmDrvrCertFile);
    loadConf->globals.pszDfltNetstrmDrvrCertFile = NULL;
    free(loadConf->globals.pszDfltOpensslEngine);
    loadConf->globals.pszDfltOpensslEngine = NULL;
    free(LocalHostNameOverride);
    LocalHostNameOverride = NULL;
    free(loadConf->globals.oversizeMsgErrorFile);
    loadConf->globals.oversizeMsgErrorFile = NULL;
    loadConf->globals.oversizeMsgInputMode = glblOversizeMsgInputMode_Accept;
    loadConf->globals.reportChildProcessExits = REPORT_CHILD_PROCESS_EXITS_ERRORS;
    free(loadConf->globals.pszWorkDir);
    loadConf->globals.pszWorkDir = NULL;
    free((void *)loadConf->globals.operatingStateFile);
    loadConf->globals.operatingStateFile = NULL;
    loadConf->globals.bDropMalPTRMsgs = 0;
    bPreserveFQDN = 0;
    loadConf->globals.iMaxLine = 8192;
    loadConf->globals.reportOversizeMsg = 1;
    loadConf->globals.parser.cCCEscapeChar = '#';
    loadConf->globals.parser.bDropTrailingLF = 1;
    loadConf->globals.parser.bEscapeCCOnRcv = 1; /* default is to escape control characters */
    loadConf->globals.parser.bSpaceLFOnRcv = 0;
    loadConf->globals.parser.bEscape8BitChars = 0; /* default is not to escape control characters */
    loadConf->globals.parser.bEscapeTab = 1; /* default is to escape tab characters */
    loadConf->globals.parser.bParserEscapeCCCStyle = 0;
#ifdef USE_UNLIMITED_SELECT
    iFdSetSize = howmany(FD_SETSIZE, __NFDBITS) * sizeof(fd_mask);
#endif
    return RS_RET_OK;
}


/* Prepare for new config
 */
void glblPrepCnf(void) {
    free(mainqCnfObj);
    mainqCnfObj = NULL;
    free(cnfparamvals);
    cnfparamvals = NULL;
}

/* handle the timezone() object. Each incarnation adds one additional
 * zone info to the global table of time zones.
 */

int bs_arrcmp_glblDbgFiles(const void *s1, const void *s2) {
    return strcmp((char *)s1, *(char **)s2);
}


/* handle a global config object. Note that multiple global config statements
 * are permitted (because of plugin support), so once we got a param block,
 * we need to hold to it.
 * rgerhards, 2011-07-19
 */
void glblProcessCnf(struct cnfobj *o) {
    int i;

    cnfparamvals = nvlstGetParams(o->nvlst, &paramblk, cnfparamvals);
    if (cnfparamvals == NULL) {
        LogError(0, RS_RET_MISSING_CNFPARAMS,
                 "error processing global "
                 "config parameters [global(...)]");
        goto done;
    }
    if (Debug) {
        dbgprintf("glbl param blk after glblProcessCnf:\n");
        cnfparamsPrint(&paramblk, cnfparamvals);
    }

    /* The next thing is a bit hackish and should be changed in higher
     * versions. There are a select few parameters which we need to
     * act on immediately. These are processed here.
     */
    for (i = 0; i < paramblk.nParams; ++i) {
        if (!cnfparamvals[i].bUsed) continue;
        if (!strcmp(paramblk.descr[i].name, "processinternalmessages")) {
            loadConf->globals.bProcessInternalMessages = (int)cnfparamvals[i].val.d.n;
            cnfparamvals[i].bUsed = TRUE;
        } else if (!strcmp(paramblk.descr[i].name, "internal.developeronly.options")) {
            loadConf->globals.glblDevOptions = (uint64_t)cnfparamvals[i].val.d.n;
            cnfparamvals[i].bUsed = TRUE;
        } else if (!strcmp(paramblk.descr[i].name, "stdlog.channelspec")) {
#ifndef ENABLE_LIBLOGGING_STDLOG
            LogError(0, RS_RET_ERR,
                     "rsyslog wasn't "
                     "compiled with liblogging-stdlog support. "
                     "The 'stdlog.channelspec' parameter "
                     "is ignored. Note: the syslog API is used instead.\n");
#else
            loadConf->globals.stdlog_chanspec = (uchar *)es_str2cstr(cnfparamvals[i].val.d.estr, NULL);
            /* we need to re-open with the new channel */
            stdlog_close(loadConf->globals.stdlog_hdl);
            loadConf->globals.stdlog_hdl =
                stdlog_open("rsyslogd", 0, STDLOG_SYSLOG, (char *)loadConf->globals.stdlog_chanspec);
            cnfparamvals[i].bUsed = TRUE;
#endif
        } else if (!strcmp(paramblk.descr[i].name, "operatingstatefile")) {
            if (loadConf->globals.operatingStateFile != NULL) {
                LogError(errno, RS_RET_PARAM_ERROR,
                         "error: operatingStateFile already set to '%s' - "
                         "new value ignored",
                         loadConf->globals.operatingStateFile);
            } else {
                loadConf->globals.operatingStateFile = (uchar *)es_str2cstr(cnfparamvals[i].val.d.estr, NULL);
                osf_open();
            }
        } else if (!strcmp(paramblk.descr[i].name, "security.abortonidresolutionfail")) {
            loadConf->globals.abortOnIDResolutionFail = (int)cnfparamvals[i].val.d.n;
            cnfparamvals[i].bUsed = TRUE;
        }
    }
done:
    return;
}

/* Set mainq parameters. Note that when this is not called, we'll use the
 * legacy parameter config. mainq parameters can only be set once.
 */
void glblProcessMainQCnf(struct cnfobj *o) {
    if (mainqCnfObj == NULL) {
        mainqCnfObj = o;
    } else {
        LogError(0, RS_RET_ERR,
                 "main_queue() object can only be specified "
                 "once - all but first ignored\n");
    }
}

/* destruct the main q cnf object after it is no longer needed. This is
 * also used to do some final checks.
 */
void glblDestructMainqCnfObj(void) {
    /* Only destruct if not NULL! */
    if (mainqCnfObj != NULL) {
        nvlstChkUnused(mainqCnfObj->nvlst);
        cnfobjDestruct(mainqCnfObj);
        mainqCnfObj = NULL;
    }
}

static int qs_arrcmp_glblDbgFiles(const void *s1, const void *s2) {
    return strcmp(*((char **)s1), *((char **)s2));
}

/* set an environment variable */
static rsRetVal do_setenv(const char *const var) {
    char varname[128];
    const char *val = var;
    size_t i;
    DEFiRet;

    for (i = 0; *val != '='; ++i, ++val) {
        if (i == sizeof(varname) - i) {
            parser_errmsg(
                "environment variable name too long "
                "[max %zu chars] or malformed entry: '%s'",
                sizeof(varname) - 1, var);
            ABORT_FINALIZE(RS_RET_ERR_SETENV);
        }
        if (*val == '\0') {
            parser_errmsg(
                "environment variable entry is missing "
                "equal sign (for value): '%s'",
                var);
            ABORT_FINALIZE(RS_RET_ERR_SETENV);
        }
        varname[i] = *val;
    }
    varname[i] = '\0';
    ++val;
    DBGPRINTF("do_setenv, var '%s', val '%s'\n", varname, val);

    if (setenv(varname, val, 1) != 0) {
        char errStr[1024];
        rs_strerror_r(errno, errStr, sizeof(errStr));
        parser_errmsg(
            "error setting environment variable "
            "'%s' to '%s': %s",
            varname, val, errStr);
        ABORT_FINALIZE(RS_RET_ERR_SETENV);
    }


finalize_it:
    RETiRet;
}


/* This processes the "regular" parameters which are to be set after the
 * config has been fully loaded.
 */
rsRetVal glblDoneLoadCnf(void) {
    int i;
    unsigned char *cstr;
    DEFiRet;
    CHKiRet(objUse(net, CORE_COMPONENT));

    sortTimezones(loadConf);
    DBGPRINTF("Timezone information table (%d entries):\n", loadConf->timezones.ntzinfos);
    displayTimezones(loadConf);

    if (cnfparamvals == NULL) goto finalize_it;

    for (i = 0; i < paramblk.nParams; ++i) {
        if (!cnfparamvals[i].bUsed) continue;
        if (!strcmp(paramblk.descr[i].name, "workdirectory")) {
            cstr = (uchar *)es_str2cstr(cnfparamvals[i].val.d.estr, NULL);
            setWorkDir(NULL, cstr);
        } else if (!strcmp(paramblk.descr[i].name, "libcapng.default")) {
#ifdef ENABLE_LIBCAPNG
            loadConf->globals.bAbortOnFailedLibcapngSetup = (int)cnfparamvals[i].val.d.n;
#else
            LogError(0, RS_RET_ERR,
                     "rsyslog wasn't "
                     "compiled with libcap-ng support.");
#endif
        } else if (!strcmp(paramblk.descr[i].name, "libcapng.enable")) {
#ifdef ENABLE_LIBCAPNG
            loadConf->globals.bCapabilityDropEnabled = (int)cnfparamvals[i].val.d.n;
#else
            LogError(0, RS_RET_ERR,
                     "rsyslog wasn't "
                     "compiled with libcap-ng support.");
#endif
        } else if (!strcmp(paramblk.descr[i].name, "variables.casesensitive")) {
            const int val = (int)cnfparamvals[i].val.d.n;
            fjson_global_do_case_sensitive_comparison(val);
            DBGPRINTF("global/config: set case sensitive variables to %d\n", val);
        } else if (!strcmp(paramblk.descr[i].name, "localhostname")) {
            free(LocalHostNameOverride);
            LocalHostNameOverride = (uchar *)es_str2cstr(cnfparamvals[i].val.d.estr, NULL);
        } else if (!strcmp(paramblk.descr[i].name, "defaultnetstreamdriverkeyfile")) {
            cstr = (uchar *)es_str2cstr(cnfparamvals[i].val.d.estr, NULL);
            setDfltNetstrmDrvrKeyFile(NULL, cstr);
        } else if (!strcmp(paramblk.descr[i].name, "defaultnetstreamdrivercertfile")) {
            cstr = (uchar *)es_str2cstr(cnfparamvals[i].val.d.estr, NULL);
            setDfltNetstrmDrvrCertFile(NULL, cstr);
        } else if (!strcmp(paramblk.descr[i].name, "defaultnetstreamdrivercafile")) {
            cstr = (uchar *)es_str2cstr(cnfparamvals[i].val.d.estr, NULL);
            setDfltNetstrmDrvrCAF(NULL, cstr);
        } else if (!strcmp(paramblk.descr[i].name, "defaultnetstreamdrivercrlfile")) {
            cstr = (uchar *)es_str2cstr(cnfparamvals[i].val.d.estr, NULL);
            setDfltNetstrmDrvrCRLF(NULL, cstr);
        } else if (!strcmp(paramblk.descr[i].name, "defaultnetstreamdriver")) {
            cstr = (uchar *)es_str2cstr(cnfparamvals[i].val.d.estr, NULL);
            setDfltNetstrmDrvr(NULL, cstr);
        } else if (!strcmp(paramblk.descr[i].name, "defaultopensslengine")) {
            cstr = (uchar *)es_str2cstr(cnfparamvals[i].val.d.estr, NULL);
            setDfltOpensslEngine(NULL, cstr);
        } else if (!strcmp(paramblk.descr[i].name, "netstreamdrivercaextrafiles")) {
            cstr = (uchar *)es_str2cstr(cnfparamvals[i].val.d.estr, NULL);
            setNetstrmDrvrCAExtraFiles(NULL, cstr);
        } else if (!strcmp(paramblk.descr[i].name, "preservefqdn")) {
            bPreserveFQDN = (int)cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "compactjsonstring")) {
            glblJsonFormatOpt = cnfparamvals[i].val.d.n ? JSON_C_TO_STRING_PLAIN : JSON_C_TO_STRING_SPACED;
        } else if (!strcmp(paramblk.descr[i].name, "dropmsgswithmaliciousdnsptrrecords")) {
            loadConf->globals.bDropMalPTRMsgs = (int)cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "action.reportsuspension")) {
            loadConf->globals.bActionReportSuspension = (int)cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "action.reportsuspensioncontinuation")) {
            loadConf->globals.bActionReportSuspensionCont = (int)cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "maxmessagesize")) {
            setMaxLine(cnfparamvals[i].val.d.n);
        } else if (!strcmp(paramblk.descr[i].name, "oversizemsg.errorfile")) {
            free(loadConf->globals.oversizeMsgErrorFile);
            loadConf->globals.oversizeMsgErrorFile = (uchar *)es_str2cstr(cnfparamvals[i].val.d.estr, NULL);
        } else if (!strcmp(paramblk.descr[i].name, "oversizemsg.report")) {
            loadConf->globals.reportOversizeMsg = (int)cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "oversizemsg.input.mode")) {
            const char *const tmp = es_str2cstr(cnfparamvals[i].val.d.estr, NULL);
            setOversizeMsgInputMode((uchar *)tmp);
            free((void *)tmp);
        } else if (!strcmp(paramblk.descr[i].name, "reportchildprocessexits")) {
            const char *const tmp = es_str2cstr(cnfparamvals[i].val.d.estr, NULL);
            setReportChildProcessExits((uchar *)tmp);
            free((void *)tmp);
        } else if (!strcmp(paramblk.descr[i].name, "debug.onshutdown")) {
            loadConf->globals.debugOnShutdown = (int)cnfparamvals[i].val.d.n;
            LogError(0, RS_RET_OK, "debug: onShutdown set to %d", loadConf->globals.debugOnShutdown);
        } else if (!strcmp(paramblk.descr[i].name, "debug.gnutls")) {
            loadConf->globals.iGnuTLSLoglevel = (int)cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "debug.abortonprogramerror")) {
            glblAbortOnProgramError = (int)cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "debug.unloadmodules")) {
            glblUnloadModules = (int)cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "parser.controlcharacterescapeprefix")) {
            uchar *tmp = (uchar *)es_str2cstr(cnfparamvals[i].val.d.estr, NULL);
            setParserControlCharacterEscapePrefix(NULL, tmp);
            free(tmp);
        } else if (!strcmp(paramblk.descr[i].name, "parser.droptrailinglfonreception")) {
            const int tmp = (int)cnfparamvals[i].val.d.n;
            setParserDropTrailingLFOnReception(NULL, tmp);
        } else if (!strcmp(paramblk.descr[i].name, "parser.escapecontrolcharactersonreceive")) {
            const int tmp = (int)cnfparamvals[i].val.d.n;
            setParserEscapeControlCharactersOnReceive(NULL, tmp);
        } else if (!strcmp(paramblk.descr[i].name, "parser.spacelfonreceive")) {
            const int tmp = (int)cnfparamvals[i].val.d.n;
            setParserSpaceLFOnReceive(NULL, tmp);
        } else if (!strcmp(paramblk.descr[i].name, "parser.escape8bitcharactersonreceive")) {
            const int tmp = (int)cnfparamvals[i].val.d.n;
            setParserEscape8BitCharactersOnReceive(NULL, tmp);
        } else if (!strcmp(paramblk.descr[i].name, "parser.escapecontrolcharactertab")) {
            const int tmp = (int)cnfparamvals[i].val.d.n;
            setParserEscapeControlCharacterTab(NULL, tmp);
        } else if (!strcmp(paramblk.descr[i].name, "parser.escapecontrolcharacterscstyle")) {
            const int tmp = (int)cnfparamvals[i].val.d.n;
            SetParserEscapeControlCharactersCStyle(tmp);
        } else if (!strcmp(paramblk.descr[i].name, "parser.parsehostnameandtag")) {
            const int tmp = (int)cnfparamvals[i].val.d.n;
            SetParseHOSTNAMEandTAG(tmp);
        } else if (!strcmp(paramblk.descr[i].name, "parser.permitslashinprogramname")) {
            loadConf->globals.parser.bPermitSlashInProgramname = (int)cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "debug.logfile")) {
            if (pszAltDbgFileName == NULL) {
                pszAltDbgFileName = es_str2cstr(cnfparamvals[i].val.d.estr, NULL);
                /* can actually happen if debug system also opened altdbg */
                if (altdbg != -1) {
                    close(altdbg);
                }
                if ((altdbg = open(pszAltDbgFileName, O_WRONLY | O_CREAT | O_TRUNC | O_NOCTTY | O_CLOEXEC,
                                   S_IRUSR | S_IWUSR)) == -1) {
                    LogError(0, RS_RET_ERR, "debug log file '%s' could not be opened", pszAltDbgFileName);
                }
            }
            LogError(0, RS_RET_OK, "debug log file is '%s', fd %d", pszAltDbgFileName, altdbg);
        } else if (!strcmp(paramblk.descr[i].name, "janitor.interval")) {
            loadConf->globals.janitorInterval = (int)cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "net.ipprotocol")) {
            char *proto = es_str2cstr(cnfparamvals[i].val.d.estr, NULL);
            if (!strcmp(proto, "unspecified")) {
                loadConf->globals.iDefPFFamily = PF_UNSPEC;
            } else if (!strcmp(proto, "ipv4-only")) {
                loadConf->globals.iDefPFFamily = PF_INET;
            } else if (!strcmp(proto, "ipv6-only")) {
                loadConf->globals.iDefPFFamily = PF_INET6;
            } else {
                LogError(0, RS_RET_ERR,
                         "invalid net.ipprotocol "
                         "parameter '%s' -- ignored",
                         proto);
            }
            free(proto);
        } else if (!strcmp(paramblk.descr[i].name, "senders.reportnew")) {
            loadConf->globals.reportNewSenders = (int)cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "senders.reportgoneaway")) {
            loadConf->globals.reportGoneAwaySenders = (int)cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "senders.timeoutafter")) {
            loadConf->globals.senderStatsTimeout = (int)cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "senders.keeptrack")) {
            loadConf->globals.senderKeepTrack = (int)cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "inputs.timeout.shutdown")) {
            loadConf->globals.inputTimeoutShutdown = (int)cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "privdrop.group.keepsupplemental")) {
            loadConf->globals.gidDropPrivKeepSupplemental = (int)cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "privdrop.group.id")) {
            loadConf->globals.gidDropPriv = (int)cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "privdrop.group.name")) {
            loadConf->globals.gidDropPriv = (int)cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "privdrop.user.id")) {
            loadConf->globals.uidDropPriv = (int)cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "privdrop.user.name")) {
            loadConf->globals.uidDropPriv = (int)cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "security.abortonidresolutionfail")) {
            loadConf->globals.abortOnIDResolutionFail = (int)cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "net.acladdhostnameonfail")) {
            loadConf->globals.ACLAddHostnameOnFail = (int)cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "net.aclresolvehostname")) {
            loadConf->globals.ACLDontResolve = !((int)cnfparamvals[i].val.d.n);
        } else if (!strcmp(paramblk.descr[i].name, "net.enabledns")) {
            SetDisableDNS(!((int)cnfparamvals[i].val.d.n));
        } else if (!strcmp(paramblk.descr[i].name, "net.permitwarning")) {
            SetOptionDisallowWarning(!((int)cnfparamvals[i].val.d.n));
        } else if (!strcmp(paramblk.descr[i].name, "abortonuncleanconfig")) {
            loadConf->globals.bAbortOnUncleanConfig = cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "abortonfailedqueuestartup")) {
            loadConf->globals.bAbortOnFailedQueueStartup = cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "internalmsg.ratelimit.burst")) {
            loadConf->globals.intMsgRateLimitBurst = (int)cnfparamvals[i].val.d.n;
            internalMsgRlParamsUsed = 1;
        } else if (!strcmp(paramblk.descr[i].name, "internalmsg.ratelimit.interval")) {
            loadConf->globals.intMsgRateLimitItv = (int)cnfparamvals[i].val.d.n;
            internalMsgRlParamsUsed = 1;
        } else if (!strcmp(paramblk.descr[i].name, "internalmsg.ratelimit.name")) {
            free(internalMsgRatelimitName);
            internalMsgRatelimitName = es_str2cstr(cnfparamvals[i].val.d.estr, NULL);
        } else if (!strcmp(paramblk.descr[i].name, "internalmsg.severity")) {
            loadConf->globals.intMsgsSeverityFilter = (int)cnfparamvals[i].val.d.n;
            if ((loadConf->globals.intMsgsSeverityFilter < 0) || (loadConf->globals.intMsgsSeverityFilter > 7)) {
                parser_errmsg("invalid internalmsg.severity value");
                loadConf->globals.intMsgsSeverityFilter = DFLT_INT_MSGS_SEV_FILTER;
            }
        } else if (!strcmp(paramblk.descr[i].name, "environment")) {
            for (int j = 0; j < cnfparamvals[i].val.d.ar->nmemb; ++j) {
                char *const var = es_str2cstr(cnfparamvals[i].val.d.ar->arr[j], NULL);
                do_setenv(var);
                free(var);
            }
        } else if (!strcmp(paramblk.descr[i].name, "errormessagestostderr.maxnumber")) {
            loadConf->globals.maxErrMsgToStderr = (int)cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "allmessagestostderr")) {
            loadConf->globals.bAllMsgToStderr = (int)cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "debug.files")) {
            free(glblDbgFiles); /* "fix" Coverity false positive */
            glblDbgFilesNum = cnfparamvals[i].val.d.ar->nmemb;
            glblDbgFiles = (char **)malloc(cnfparamvals[i].val.d.ar->nmemb * sizeof(char *));
            for (int j = 0; j < cnfparamvals[i].val.d.ar->nmemb; ++j) {
                glblDbgFiles[j] = es_str2cstr(cnfparamvals[i].val.d.ar->arr[j], NULL);
            }
            qsort(glblDbgFiles, glblDbgFilesNum, sizeof(char *), qs_arrcmp_glblDbgFiles);
        } else if (!strcmp(paramblk.descr[i].name, "debug.whitelist")) {
            glblDbgWhitelist = (int)cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "shutdown.queue.doublesize")) {
            loadConf->globals.shutdownQueueDoubleSize = (int)cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "umask")) {
            loadConf->globals.umask = (int)cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "shutdown.enable.ctlc")) {
            loadConf->globals.permitCtlC = (int)cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "default.action.queue.timeoutshutdown")) {
            loadConf->globals.actq_dflt_toQShutdown = cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "default.action.queue.timeoutactioncompletion")) {
            loadConf->globals.actq_dflt_toActShutdown = cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "default.action.queue.timeoutenqueue")) {
            loadConf->globals.actq_dflt_toEnq = cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "default.action.queue.timeoutworkerthreadshutdown")) {
            loadConf->globals.actq_dflt_toWrkShutdown = cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "default.ruleset.queue.timeoutshutdown")) {
            loadConf->globals.ruleset_dflt_toQShutdown = cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "default.ruleset.queue.timeoutactioncompletion")) {
            loadConf->globals.ruleset_dflt_toActShutdown = cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "default.ruleset.queue.timeoutenqueue")) {
            loadConf->globals.ruleset_dflt_toEnq = cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "default.ruleset.queue.timeoutworkerthreadshutdown")) {
            loadConf->globals.ruleset_dflt_toWrkShutdown = cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "reverselookup.cache.ttl.default")) {
            loadConf->globals.dnscacheDefaultTTL = cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "reverselookup.cache.ttl.enable")) {
            loadConf->globals.dnscacheEnableTTL = cnfparamvals[i].val.d.n;
        } else if (!strcmp(paramblk.descr[i].name, "parser.supportcompressionextension")) {
            loadConf->globals.bSupportCompressionExtension = cnfparamvals[i].val.d.n;
        } else {
            dbgprintf(
                "glblDoneLoadCnf: program error, non-handled "
                "param '%s'\n",
                paramblk.descr[i].name);
        }
    }

    const sbool needInternalRlCfg =
        (loadConf->globals.intMsgRateLimitItv > 0) || internalMsgRlParamsUsed || (internalMsgRatelimitName != NULL);
    if (needInternalRlCfg) {
        unsigned int interval = (unsigned int)loadConf->globals.intMsgRateLimitItv;
        unsigned int burst = (unsigned int)loadConf->globals.intMsgRateLimitBurst;
        const rsRetVal rlRet =
            ratelimitResolveFromValues(loadConf, "internalmsg", internalMsgRatelimitName, internalMsgRlParamsUsed,
                                       &interval, &burst, NULL, &loadConf->globals.internalMsgRatelimitCfg);
        if (rlRet == RS_RET_OK) {
            loadConf->globals.intMsgRateLimitItv = (int)interval;
            loadConf->globals.intMsgRateLimitBurst = (int)burst;
        } else {
            LogError(0, rlRet, "global: unable to resolve internalmsg ratelimit configuration");
        }
    }

    free(internalMsgRatelimitName);
    internalMsgRatelimitName = NULL;
    internalMsgRlParamsUsed = 0;

    if (loadConf->globals.debugOnShutdown && Debug != DEBUG_FULL) {
        Debug = DEBUG_ONDEMAND;
        stddbg = -1;
    }

finalize_it:
    /* we have now read the config. We need to query the local host name now
     * as it was set by the config.
     *
     * Note: early messages are already emited, and have "[localhost]" as
     * hostname. These messages are currently in iminternal queue. Once they
     * are taken from that queue, the hostname will be adapted.
     */
    queryLocalHostname(loadConf);
    RETiRet;
}


/* Initialize the glbl class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINAbstractObjClassInit(glbl, 1, OBJ_IS_CORE_MODULE) /* class, version */
    /* request objects we use */
    CHKiRet(objUse(prop, CORE_COMPONENT));

    /* intialize properties */
    storeLocalHostIPIF((uchar *)"127.0.0.1");

    /* config handlers are never unregistered and need not be - we are always loaded ;) */
    CHKiRet(regCfSysLineHdlr((uchar *)"debugfile", 0, eCmdHdlrGetWord, setDebugFile, NULL, NULL));
    CHKiRet(regCfSysLineHdlr((uchar *)"debuglevel", 0, eCmdHdlrInt, setDebugLevel, NULL, NULL));
    CHKiRet(regCfSysLineHdlr((uchar *)"workdirectory", 0, eCmdHdlrGetWord, setWorkDir, NULL, NULL));
    CHKiRet(regCfSysLineHdlr((uchar *)"dropmsgswithmaliciousdnsptrrecords", 0, eCmdHdlrBinary, SetDropMalPTRMsgs, NULL,
                             NULL));
    CHKiRet(regCfSysLineHdlr((uchar *)"defaultnetstreamdriver", 0, eCmdHdlrGetWord, setDfltNetstrmDrvr, NULL, NULL));
    CHKiRet(regCfSysLineHdlr((uchar *)"defaultopensslengine", 0, eCmdHdlrGetWord, setDfltOpensslEngine, NULL, NULL));
    CHKiRet(regCfSysLineHdlr((uchar *)"defaultnetstreamdrivercafile", 0, eCmdHdlrGetWord, setDfltNetstrmDrvrCAF, NULL,
                             NULL));
    CHKiRet(regCfSysLineHdlr((uchar *)"defaultnetstreamdrivercrlfile", 0, eCmdHdlrGetWord, setDfltNetstrmDrvrCRLF, NULL,
                             NULL));
    CHKiRet(regCfSysLineHdlr((uchar *)"defaultnetstreamdriverkeyfile", 0, eCmdHdlrGetWord, setDfltNetstrmDrvrKeyFile,
                             NULL, NULL));
    CHKiRet(regCfSysLineHdlr((uchar *)"defaultnetstreamdrivercertfile", 0, eCmdHdlrGetWord, setDfltNetstrmDrvrCertFile,
                             NULL, NULL));
    CHKiRet(regCfSysLineHdlr((uchar *)"localhostname", 0, eCmdHdlrGetWord, NULL, &LocalHostNameOverride, NULL));
    CHKiRet(regCfSysLineHdlr((uchar *)"localhostipif", 0, eCmdHdlrGetWord, setLocalHostIPIF, NULL, NULL));
    CHKiRet(regCfSysLineHdlr((uchar *)"netstreamdrivercaextrafiles", 0, eCmdHdlrGetWord, setNetstrmDrvrCAExtraFiles,
                             NULL, NULL));
    CHKiRet(regCfSysLineHdlr((uchar *)"optimizeforuniprocessor", 0, eCmdHdlrGoneAway, NULL, NULL, NULL));
    CHKiRet(regCfSysLineHdlr((uchar *)"preservefqdn", 0, eCmdHdlrBinary, NULL, &bPreserveFQDN, NULL));
    CHKiRet(regCfSysLineHdlr((uchar *)"maxmessagesize", 0, eCmdHdlrSize, legacySetMaxMessageSize, NULL, NULL));

    /* Deprecated parser config options */
    CHKiRet(regCfSysLineHdlr((uchar *)"controlcharacterescapeprefix", 0, eCmdHdlrGetChar,
                             setParserControlCharacterEscapePrefix, NULL, NULL));
    CHKiRet(regCfSysLineHdlr((uchar *)"droptrailinglfonreception", 0, eCmdHdlrBinary,
                             setParserDropTrailingLFOnReception, NULL, NULL));
    CHKiRet(regCfSysLineHdlr((uchar *)"escapecontrolcharactersonreceive", 0, eCmdHdlrBinary,
                             setParserEscapeControlCharactersOnReceive, NULL, NULL));
    CHKiRet(regCfSysLineHdlr((uchar *)"spacelfonreceive", 0, eCmdHdlrBinary, setParserSpaceLFOnReceive, NULL, NULL));
    CHKiRet(regCfSysLineHdlr((uchar *)"escape8bitcharactersonreceive", 0, eCmdHdlrBinary,
                             setParserEscape8BitCharactersOnReceive, NULL, NULL));
    CHKiRet(regCfSysLineHdlr((uchar *)"escapecontrolcharactertab", 0, eCmdHdlrBinary,
                             setParserEscapeControlCharacterTab, NULL, NULL));

    CHKiRet(
        regCfSysLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL, NULL));

    INIT_ATOMIC_HELPER_MUT(mutTerminateInputs);
ENDObjClassInit(glbl)


/* Exit the glbl class.
 * rgerhards, 2008-04-17
 */
BEGINObjClassExit(glbl, OBJ_IS_CORE_MODULE) /* class, version */
    free(LocalDomain);
    free(LocalHostName);
    free(LocalHostNameOverride);
    free(LocalFQDNName);
    objRelease(prop, CORE_COMPONENT);
    if (propLocalHostNameToDelete != NULL) prop.Destruct(&propLocalHostNameToDelete);
    DESTROY_ATOMIC_HELPER_MUT(mutTerminateInputs);
ENDObjClassExit(glbl)
