/* imtcp.c
 * This is the implementation of the TCP input module.
 *
 * File begun on 2007-12-21 by RGerhards (extracted from syslogd.c,
 * which at the time of the rsyslog fork was BSD-licensed)
 *
 * Copyright 2007-2025 Adiscon GmbH.
 *
 * This file is part of rsyslog.
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

/* This note shall explain the calling sequence while we do not have
 * have full RainerScript support for (TLS) sender authentication:
 *
 * imtcp --> tcpsrv --> netstrms (this sequence stored pPermPeers in netstrms class)
 * then a callback (doOpenLstnSocks) into imtcp happens, which in turn calls
 * into tcpsrv.create_tcp_socket(),
 * which calls into netstrm.LstnInit(), which receives a pointer to netstrms obj
 * which calls into the driver function LstnInit (again, netstrms obj passed)
 * which finally calls back into netstrms obj's get functions to obtain the auth
 * parameters and then applies them to the driver object instance
 *
 * rgerhards, 2008-05-19
 */
#include "config.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <ctype.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#if HAVE_FCNTL_H
    #include <fcntl.h>
#endif
#include "rsyslog.h"
#include "dirty.h"
#include "cfsysline.h"
#include "module-template.h"
#include "template.h"
#include "unicode-helper.h"
#include "net.h"
#include "netstrm.h"
#include "errmsg.h"
#include "glbl.h"
#include "tcpsrv.h"
#include "ruleset.h"
#include "rainerscript.h"
#include "parserif.h"
#include "reload-candidate.h"

MODULE_TYPE_INPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME("imtcp")

/* static data */
DEF_IMOD_STATIC_DATA;
DEFobjCurrIf(tcpsrv) DEFobjCurrIf(tcps_sess) DEFobjCurrIf(net) DEFobjCurrIf(netstrm) DEFobjCurrIf(ruleset)
    DEFobjCurrIf(prop)

        static rsRetVal resetConfigVariables(uchar __attribute__((unused)) * pp, void __attribute__((unused)) * pVal);

/* Module static data */
typedef struct tcpsrv_etry_s {
    tcpsrv_t *tcpsrv;
    char *endpoint_key; /* canonical runtime socket identity, never a config name */
    char *config_name; /* operator-facing inputname, retained separately for future diagnostics */
    enum { IMTCP_ENDPOINT_ACTIVE, IMTCP_ENDPOINT_NO_ACCEPT, IMTCP_ENDPOINT_RETIRING } state;
    pthread_t tid; /* the worker's thread ID */
    int thread_started;
    struct tcpsrv_etry_s *next;
} tcpsrv_etry_t;

/* Runtime-owned registry foundation for a later reload lifecycle.  It is
 * private to imtcp and deliberately has no lookup, activation, or message
 * path role yet: retaining the current list ordering preserves all behavior. */
static struct {
    tcpsrv_etry_t *head;
    int count;
} endpoint_registry = {0};

static permittedPeers_t *pPermPeersRoot = NULL;

static rsRetVal endpointKeyBuild(const tcpLstnParams_t *const params,
                                 const char *const networkNamespace,
                                 char **const key) {
    const char *const address = params->pszAddr == NULL ? "*" : (const char *)params->pszAddr;
    const char *const port = params->pszPort == NULL ? "" : (const char *)params->pszPort;
    const char *const ns = networkNamespace == NULL ? "" : networkNamespace;
    char *end = NULL;
    unsigned long numericPort;
    int printed;
    size_t required;

    if (key == NULL || *key != NULL) return RS_RET_PARAM_ERROR;
    if (params->pszLstnPortFileName != NULL) return RS_RET_NOT_IMPLEMENTED;
    if (*port == '\0') return RS_RET_PARAM_ERROR;
    errno = 0;
    numericPort = strtoul(port, &end, 10);
    if (errno != 0 || end == port || *end != '\0' || numericPort == 0 || numericPort > UINT16_MAX)
        return RS_RET_NOT_IMPLEMENTED;
    printed = snprintf(NULL, 0, "tcp|ns:%zu:%s|addr:%zu:%s|port:%u", strlen(ns), ns, strlen(address), address,
                       (unsigned)numericPort);
    if (printed < 0) return RS_RET_PARAM_ERROR;
    required = (size_t)printed;
    if ((*key = malloc(required + 1)) == NULL) return RS_RET_OUT_OF_MEMORY;
    snprintf(*key, required + 1, "tcp|ns:%zu:%s|addr:%zu:%s|port:%u", strlen(ns), ns, strlen(address), address,
             (unsigned)numericPort);
    return RS_RET_OK;
}

static rsRetVal endpointRegistryAdd(tcpsrv_t *const server,
                                    const tcpLstnParams_t *const params,
                                    const char *const networkNamespace,
                                    const uchar *const configName) {
    tcpsrv_etry_t *entry = NULL;
    const tcpsrv_etry_t *existing;
    DEFiRet;

    if (server == NULL || params == NULL) return RS_RET_PARAM_ERROR;
    CHKmalloc(entry = calloc(1, sizeof(*entry)));
    iRet = endpointKeyBuild(params, networkNamespace, &entry->endpoint_key);
    if (iRet == RS_RET_NOT_IMPLEMENTED)
        iRet = RS_RET_OK; /* dynamic/service endpoints remain active but are not reload-keyable */
    else if (iRet != RS_RET_OK)
        ABORT_FINALIZE(iRet);
    if (entry->endpoint_key != NULL) {
        for (existing = endpoint_registry.head; existing != NULL; existing = existing->next) {
            if (existing->endpoint_key != NULL && !strcmp(existing->endpoint_key, entry->endpoint_key))
                ABORT_FINALIZE(RS_RET_DUP_PARAM);
        }
    }
    if (configName != NULL) CHKmalloc(entry->config_name = strdup((const char *)configName));
    entry->tcpsrv = server;
    entry->state = IMTCP_ENDPOINT_ACTIVE;
    entry->next = endpoint_registry.head;
    endpoint_registry.head = entry;
    ++endpoint_registry.count;
    entry = NULL;

finalize_it:
    if (entry != NULL) {
        free(entry->endpoint_key);
        free(entry->config_name);
        free(entry);
    }
    RETiRet;
}

static void endpointRegistryRemove(tcpsrv_etry_t *const entry) {
    entry->state = IMTCP_ENDPOINT_RETIRING;
    free(entry->endpoint_key);
    free(entry->config_name);
    free(entry);
}

/* default number of workers to configure. We choose 2, as this is probably good for
 * many installations. High-Volume ones may need much higher number!
 */
#define DEFAULT_NUMWRKR 2
#define DEFAULT_STARVATIONMAXREADS 500

#define FRAMING_UNSET -1

/* config settings */
static struct configSettings_s {
    int iTCPSessMax;
    int iTCPLstnMax;
    int bSuppOctetFram;
    int iStrmDrvrMode;
    sbool bStrmDrvrModeSet; /* stream driver mode was explicitly configured */
    int bKeepAlive;
    int iKeepAliveIntvl;
    int iKeepAliveProbes;
    int iKeepAliveTime;
    int bEmitMsgOnClose;
    int bEmitMsgOnOpen;
    int iAddtlFrameDelim;
    int maxFrameSize;
    int bDisableLFDelim;
    int discardTruncatedMsg;
    int bUseFlowControl;
    int bPreserveCase;
    uchar *gnutlsPriorityString;
    uchar *pszStrmDrvrAuthMode;
    uchar *pszStrmDrvrPermitExpiredCerts;
    uchar *pszInputName;
    uchar *pszBindRuleset;
    uchar *lstnIP; /* which IP we should listen on? */
    uchar *lstnPortFile;
    int compressionMode;
    int compressionDriver;
    uint64_t compressionMaxExpansionRatio;
    uint64_t compressionMaxDecompressedBytesPerReceive;
    uint64_t compressionMaxTotalZstdWindowBytes;
    sbool compressionMaxTotalZstdWindowBytesSet;
} cs;

struct instanceConf_s {
    int iTCPSessMax;
    int iTCPLstnMax;
    unsigned numWrkr;
    tcpLstnParams_t *cnf_params; /**< listener config parameters */
    uchar *pszBindRuleset; /* name of ruleset to bind to */
    ruleset_t *pBindRuleset; /* ruleset to bind listener to (use system default if unspecified) */
    uchar *pszInputName; /* value for inputname property, NULL is OK and handled by core engine */
    uchar *dfltTZ;
    sbool bSPFramingFix;
    int ratelimitInterval;
    int ratelimitBurst;
    int iAddtlFrameDelim; /* addtl frame delimiter, e.g. for netscreen, default none */
    int maxFrameSize;
    int bUseFlowControl;
    int bDisableLFDelim;
    int discardTruncatedMsg;
    int bEmitMsgOnClose;
    int bEmitMsgOnOpen;
    int bPreserveCase;
    int iSynBacklog;
    char *pszNetworkNamespace; /**< optional network name to use */
    uchar *pszStrmDrvrName; /* stream driver to use */
    int iStrmDrvrMode;
    sbool bStrmDrvrModeSet; /* stream driver mode was explicitly configured */
    uchar *pszStrmDrvrAuthMode;
    uchar *pszStrmDrvrPermitExpiredCerts;
    uchar *pszStrmDrvrCAFile;
    uchar *pszStrmDrvrCRLFile;
    uchar *pszStrmDrvrKeyFile;
    uchar *pszStrmDrvrCertFile;
    permittedPeers_t *pPermPeersRoot;
    struct AllowedSenders *pAllowedSendersRoot;
    struct AllowedSenders *pAllowedSendersLast;
    sbool bAllowedSendersSet;
    uchar *gnutlsPriorityString;
    int iStrmDrvrExtendedCertCheck;
    int iStrmDrvrSANPreference;
    int iStrmTlsVerifyDepth;
    int iStrmTlsRevocationCheck; /**< Enable TLS revocation checking (OCSP/CRL) */
    int bKeepAlive;
    int iKeepAliveIntvl;
    int iKeepAliveProbes;
    int iKeepAliveTime;
    unsigned starvationMaxReads;
    int compressionMode;
    int compressionDriver;
    uint64_t compressionMaxExpansionRatio;
    uint64_t compressionMaxDecompressedBytesPerReceive;
    uint64_t compressionMaxTotalZstdWindowBytes;
    sbool compressionMaxTotalZstdWindowBytesSet;
    struct instanceConf_s *next;
};


struct modConfData_s {
    rsconf_t *pConf; /* our overall config object */
    instanceConf_t *root, *tail;
    int iTCPSessMax; /* max number of sessions */
    int iTCPLstnMax; /* max number of sessions */
    unsigned numWrkr;
    int iStrmDrvrMode; /* mode for stream driver, driver-dependent (0 mostly means plain tcp) */
    sbool bStrmDrvrModeSet; /* stream driver mode was explicitly configured */
    int iStrmDrvrExtendedCertCheck; /* verify also purpose OID in certificate extended field */
    int iStrmDrvrSANPreference; /* ignore CN when any SAN set */
    int iStrmTlsVerifyDepth; /**< Verify Depth for certificate chains */
    int iStrmTlsRevocationCheck; /**< Enable TLS revocation checking (OCSP/CRL) */
    int iAddtlFrameDelim; /* addtl frame delimiter, e.g. for netscreen, default none */
    int maxFrameSize;
    int bSuppOctetFram;
    sbool bDisableLFDelim; /* disable standard LF delimiter */
    sbool discardTruncatedMsg;
    sbool bUseFlowControl; /* use flow control, what means indicate ourselfs a "light delayable" */
    sbool bKeepAlive;
    int iKeepAliveIntvl;
    int iKeepAliveProbes;
    int iKeepAliveTime;
    sbool bEmitMsgOnClose; /* emit an informational message on close by remote peer */
    sbool bEmitMsgOnOpen; /* emit an informational message on close by remote peer */
    uchar *gnutlsPriorityString;
    char *pszNetworkNamespace; /**< default network namespace to use */
    uchar *pszStrmDrvrName; /* stream driver to use */
    uchar *pszStrmDrvrAuthMode; /* authentication mode to use */
    uchar *pszStrmDrvrPermitExpiredCerts; /* control how to handle expired certificates */
    uchar *pszStrmDrvrCAFile;
    uchar *pszStrmDrvrCRLFile;
    uchar *pszStrmDrvrKeyFile;
    uchar *pszStrmDrvrCertFile;
    permittedPeers_t *pPermPeersRoot;
    struct AllowedSenders *pAllowedSendersRoot;
    struct AllowedSenders *pAllowedSendersLast;
    sbool bAllowedSendersSet;
    sbool configSetViaV2Method;
    sbool bPreserveCase; /* preserve case of fromhost; true by default */
    unsigned starvationMaxReads;
    int compressionMode;
    int compressionDriver;
    uint64_t compressionMaxExpansionRatio;
    uint64_t compressionMaxDecompressedBytesPerReceive;
    uint64_t compressionMaxTotalZstdWindowBytes;
    sbool compressionMaxTotalZstdWindowBytesSet;
    /* Exact module origin, owned only by private reload snapshots. */
    char *reloadModuleLoadName;
};

static modConfData_t *loadModConf = NULL; /* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL; /* modConf ptr to use for the current load process */

/* module-global parameters */
static struct cnfparamdescr modpdescr[] = {{"flowcontrol", eCmdHdlrBinary, 0},
                                           {"disablelfdelimiter", eCmdHdlrBinary, 0},
                                           {"discardtruncatedmsg", eCmdHdlrBinary, 0},
                                           {"octetcountedframing", eCmdHdlrBinary, 0},
                                           {"notifyonconnectionclose", eCmdHdlrBinary, 0},
                                           {"notifyonconnectionopen", eCmdHdlrBinary, 0},
                                           {"addtlframedelimiter", eCmdHdlrNonNegInt, 0},
                                           {"maxframesize", eCmdHdlrInt, 0},
                                           {"maxsessions", eCmdHdlrPositiveInt, 0},
                                           {"maxlistners", eCmdHdlrPositiveInt, 0},
                                           {"maxlisteners", eCmdHdlrPositiveInt, 0},
                                           {"workerthreads", eCmdHdlrPositiveInt, 0},
                                           {"starvationprotection.maxreads", eCmdHdlrNonNegInt, 0},
                                           {"streamdriver.mode", eCmdHdlrNonNegInt, 0},
                                           {"streamdriver.authmode", eCmdHdlrString, 0},
                                           {"streamdriver.permitexpiredcerts", eCmdHdlrString, 0},
                                           {"streamdriver.name", eCmdHdlrString, 0},
                                           {"streamdriver.CheckExtendedKeyPurpose", eCmdHdlrBinary, 0},
                                           {"streamdriver.PrioritizeSAN", eCmdHdlrBinary, 0},
                                           {"streamdriver.TlsVerifyDepth", eCmdHdlrPositiveInt, 0},
                                           {"streamdriver.TlsRevocationCheck", eCmdHdlrBinary, 0},
                                           {"streamdriver.cafile", eCmdHdlrString, 0},
                                           {"streamdriver.crlfile", eCmdHdlrString, 0},
                                           {"streamdriver.keyfile", eCmdHdlrString, 0},
                                           {"streamdriver.certfile", eCmdHdlrString, 0},
                                           {"allowedsender", eCmdHdlrArray, 0},
                                           {"permittedpeer", eCmdHdlrArray, 0},
                                           {"keepalive", eCmdHdlrBinary, 0},
                                           {"keepalive.probes", eCmdHdlrNonNegInt, 0},
                                           {"keepalive.time", eCmdHdlrNonNegInt, 0},
                                           {"keepalive.interval", eCmdHdlrNonNegInt, 0},
                                           {"gnutlsprioritystring", eCmdHdlrString, 0},
                                           {"preservecase", eCmdHdlrBinary, 0},
                                           {"compression.mode", eCmdHdlrString, 0},
                                           {"compression.driver", eCmdHdlrString, 0},
                                           {"compression.maxexpansionratio", eCmdHdlrNonNegInt, 0},
                                           {"compression.maxdecompressedbytesperreceive", eCmdHdlrNonNegInt, 0},
                                           {"compression.maxtotalzstdwindowbytes", eCmdHdlrNonNegInt, 0},
                                           {"networknamespace", eCmdHdlrString, 0}};
static struct cnfparamblk modpblk = {CNFPARAMBLK_VERSION, sizeof(modpdescr) / sizeof(struct cnfparamdescr), modpdescr};

/* input instance parameters */
static struct cnfparamdescr inppdescr[] = {{"port", eCmdHdlrString, CNFPARAM_REQUIRED}, /* legacy: InputTCPServerRun */
                                           {"maxsessions", eCmdHdlrPositiveInt, 0},
                                           {"maxlisteners", eCmdHdlrPositiveInt, 0},
                                           {"workerthreads", eCmdHdlrPositiveInt, 0},
                                           {"flowcontrol", eCmdHdlrBinary, 0},
                                           {"disablelfdelimiter", eCmdHdlrBinary, 0},
                                           {"discardtruncatedmsg", eCmdHdlrBinary, 0},
                                           {"notifyonconnectionclose", eCmdHdlrBinary, 0},
                                           {"notifyonconnectionopen", eCmdHdlrBinary, 0},
                                           {"addtlframedelimiter", eCmdHdlrNonNegInt, 0},
                                           {"maxframesize", eCmdHdlrInt, 0},
                                           {"preservecase", eCmdHdlrBinary, 0},
                                           {"listenportfilename", eCmdHdlrString, 0},
                                           {"address", eCmdHdlrString, 0},
                                           {"name", eCmdHdlrString, 0},
                                           {"defaulttz", eCmdHdlrString, 0},
                                           {"ruleset", eCmdHdlrString, 0},
                                           {"starvationprotection.maxreads", eCmdHdlrNonNegInt, 0},
                                           {"streamdriver.mode", eCmdHdlrNonNegInt, 0},
                                           {"streamdriver.authmode", eCmdHdlrString, 0},
                                           {"streamdriver.permitexpiredcerts", eCmdHdlrString, 0},
                                           {"streamdriver.name", eCmdHdlrString, 0},
                                           {"streamdriver.CheckExtendedKeyPurpose", eCmdHdlrBinary, 0},
                                           {"streamdriver.PrioritizeSAN", eCmdHdlrBinary, 0},
                                           {"streamdriver.TlsVerifyDepth", eCmdHdlrPositiveInt, 0},
                                           {"streamdriver.TlsRevocationCheck", eCmdHdlrBinary, 0},
                                           {"streamdriver.cafile", eCmdHdlrString, 0},
                                           {"streamdriver.crlfile", eCmdHdlrString, 0},
                                           {"streamdriver.keyfile", eCmdHdlrString, 0},
                                           {"streamdriver.certfile", eCmdHdlrString, 0},
                                           {"allowedsender", eCmdHdlrArray, 0},
                                           {"permittedpeer", eCmdHdlrArray, 0},
                                           {"gnutlsprioritystring", eCmdHdlrString, 0},
                                           {"keepalive", eCmdHdlrBinary, 0},
                                           {"keepalive.probes", eCmdHdlrNonNegInt, 0},
                                           {"keepalive.time", eCmdHdlrNonNegInt, 0},
                                           {"keepalive.interval", eCmdHdlrNonNegInt, 0},
                                           {"supportoctetcountedframing", eCmdHdlrBinary, 0},
                                           {"ratelimit.interval", eCmdHdlrInt, 0},
                                           {"framingfix.cisco.asa", eCmdHdlrBinary, 0},
                                           {"ratelimit.burst", eCmdHdlrInt, 0},
                                           {"ratelimit.name", eCmdHdlrString, 0},
                                           {"socketbacklog", eCmdHdlrNonNegInt, 0},
                                           {"networknamespace", eCmdHdlrString, 0},
                                           {"compression.mode", eCmdHdlrString, 0},
                                           {"compression.driver", eCmdHdlrString, 0},
                                           {"compression.maxexpansionratio", eCmdHdlrNonNegInt, 0},
                                           {"compression.maxdecompressedbytesperreceive", eCmdHdlrNonNegInt, 0},
                                           {"compression.maxtotalzstdwindowbytes", eCmdHdlrNonNegInt, 0},
                                           {"multiline", eCmdHdlrBinary, 0},
                                           {"framing.delimiter.regex", eCmdHdlrString, 0}};
static struct cnfparamblk inppblk = {CNFPARAMBLK_VERSION, sizeof(inppdescr) / sizeof(struct cnfparamdescr), inppdescr};

#include "im-helper.h" /* must be included AFTER the type definitions! */

static int bLegacyCnfModGlobalsPermitted; /* are legacy module-global config parameters permitted? */

static void destructModuleConfigContents(modConfData_t *pModConf);

#define MAX_FRAME_SIZE_LIMIT 200000000

static rsRetVal validateMaxFrameSize(const int maxFrameSize, const sbool emitDiagnostics) {
    if (maxFrameSize < 1 || maxFrameSize > MAX_FRAME_SIZE_LIMIT) {
        if (emitDiagnostics)
            LogError(0, RS_RET_PARAM_ERROR,
                     "imtcp: invalid value for 'maxFrameSize' parameter given is %d, valid range is 1..%d",
                     maxFrameSize, MAX_FRAME_SIZE_LIMIT);
        return RS_RET_PARAM_ERROR;
    }

    return RS_RET_OK;
}

static rsRetVal validateLegacySessionLimits(void) {
    if (cs.iTCPSessMax < 1) {
        LogError(0, RS_RET_PARAM_ERROR,
                 "imtcp: invalid value for legacy 'inputtcpmaxsessions' parameter given is %d, minimum is 1",
                 cs.iTCPSessMax);
        return RS_RET_PARAM_ERROR;
    }

    if (cs.iTCPLstnMax < 1) {
        LogError(0, RS_RET_PARAM_ERROR,
                 "imtcp: invalid value for legacy 'inputtcpmaxlisteners' parameter given is %d, minimum is 1",
                 cs.iTCPLstnMax);
        return RS_RET_PARAM_ERROR;
    }

    return RS_RET_OK;
}

static rsRetVal parseCompressionModeStr(const char *const str, int *const mode, const sbool emitDiagnostics) {
    DEFiRet;

    if (strcasecmp(str, "none") == 0) {
        *mode = TCPSRV_COMPRESS_NEVER;
    } else if (strcasecmp(str, "stream:always") == 0) {
        *mode = TCPSRV_COMPRESS_STREAM_ALWAYS;
    } else {
        if (emitDiagnostics)
            parser_errmsg("imtcp: invalid compression.mode '%s', supported values are 'none' and 'stream:always'", str);
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

finalize_it:
    RETiRet;
}

static rsRetVal parseCompressionMode(es_str_t *const val, int *const mode, const sbool emitDiagnostics) {
    DEFiRet;
    char *const str = es_str2cstr(val, NULL);
    CHKmalloc(str);
    CHKiRet(parseCompressionModeStr(str, mode, emitDiagnostics));
finalize_it:
    free(str);
    RETiRet;
}

static rsRetVal parseCompressionDriverStr(const char *const str, int *const driver, const sbool emitDiagnostics) {
    DEFiRet;

    if (strcasecmp(str, "zlib") == 0) {
        *driver = TCPSRV_COMPRESS_DRIVER_ZLIB;
    } else if (strcasecmp(str, "zstd") == 0) {
#ifdef ENABLE_LIBZSTD
        *driver = TCPSRV_COMPRESS_DRIVER_ZSTD;
#else
        if (emitDiagnostics)
            parser_errmsg("imtcp: compression.driver='zstd' requires rsyslog to be built with libzstd support");
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
#endif
    } else {
        if (emitDiagnostics)
            parser_errmsg("imtcp: invalid compression.driver '%s', supported values are 'zlib' and 'zstd'", str);
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

finalize_it:
    RETiRet;
}

static rsRetVal parseCompressionDriver(es_str_t *const val, int *const driver, const sbool emitDiagnostics) {
    DEFiRet;
    char *const str = es_str2cstr(val, NULL);
    CHKmalloc(str);
    CHKiRet(parseCompressionDriverStr(str, driver, emitDiagnostics));
finalize_it:
    free(str);
    RETiRet;
}

static rsRetVal setLegacyCompressionMode(void __attribute__((unused)) * pVal, uchar *pNewVal) {
    DEFiRet;
    CHKiRet(parseCompressionModeStr((const char *)pNewVal, &cs.compressionMode, RSTRUE));
finalize_it:
    free(pNewVal);
    RETiRet;
}

static rsRetVal setLegacyCompressionDriver(void __attribute__((unused)) * pVal, uchar *pNewVal) {
    DEFiRet;
    CHKiRet(parseCompressionDriverStr((const char *)pNewVal, &cs.compressionDriver, RSTRUE));
finalize_it:
    free(pNewVal);
    RETiRet;
}

/** Warn in secure warn mode when imtcp listener transport/auth settings reduce security. */
static void warnIfInsecureListenerConfigured(const int streamDriverMode,
                                             const uchar *const effectiveStreamDriverName,
                                             const uchar *const authMode) {
    if (streamDriverMode == 0) {
        const int tlsHintsConfigured = (authMode != NULL) || glblIsTlsCapableNetstrmDrvr(effectiveStreamDriverName);
        if (tlsHintsConfigured) {
            glblWarnIfInsecureDefault(loadConf,
                                      "imtcp has TLS-related settings but streamdriver.mode=\"0\"; mode 0 uses plain "
                                      "TCP so TLS is not active "
                                      "(see https://docs.rsyslog.com/doc/faq/tls_mode0_disables_tls.html)");
        } else {
            glblWarnIfInsecureDefault(loadConf,
                                      "imtcp input uses streamdriver.mode=\"0\" (plain TCP without TLS); "
                                      "see https://docs.rsyslog.com/doc/faq/tls_mode0_disables_tls.html");
        }
    }

    if (streamDriverMode != 0 && authMode != NULL && strcasecmp((const char *)authMode, "anon") == 0) {
        glblWarnIfInsecureDefault(
            loadConf,
            "imtcp uses streamdriver.authmode=\"anon\"; server identity is not authenticated, so MITM is possible "
            "(see https://docs.rsyslog.com/doc/faq/tls_anon_auth_mitm.html)");
    }
}

static rsRetVal applySecureDefaultsToStreamDriver(rsconf_t *const cnf,
                                                  int *const streamDriverMode,
                                                  const sbool streamDriverModeSet,
                                                  const uchar *const effectiveStreamDriverName,
                                                  const char *const context,
                                                  const sbool emitDiagnostics) {
    if (glblIsTlsCapableNetstrmDrvr(effectiveStreamDriverName) && *streamDriverMode == 0 &&
        cnf->globals.compatDefaultsSecure == COMPAT_DEFAULTS_SECURE_STRICT) {
        if (streamDriverModeSet) {
            if (emitDiagnostics)
                LogError(0, RS_RET_PARAM_ERROR,
                         "%s: compatibility.defaults.secure=\"strict\" rejects explicit streamdriver.mode=\"0\" "
                         "with TLS-capable stream driver \"%s\"; use streamdriver.mode=\"1\" to enable TLS or "
                         "select ptcp/plain TCP intentionally",
                         context, (const char *)effectiveStreamDriverName);
            return RS_RET_PARAM_ERROR;
        }
        *streamDriverMode = 1;
    }
    return RS_RET_OK;
}

static const uchar *getEffectiveModuleStreamDriver(const modConfData_t *const modConf) {
    return glblGetEffectiveNetstrmDrvr(modConf->pConf, modConf->pszStrmDrvrName);
}

static const uchar *getEffectiveInstanceStreamDriver(const instanceConf_t *const inst,
                                                     const modConfData_t *const modConf) {
    const uchar *const localOrModuleDrvr =
        inst->pszStrmDrvrName == NULL ? modConf->pszStrmDrvrName : inst->pszStrmDrvrName;
    return glblGetEffectiveNetstrmDrvr(modConf->pConf, localOrModuleDrvr);
}

static const uchar *getEffectiveInstanceAuthMode(const instanceConf_t *const inst, const modConfData_t *const modConf) {
    return inst->pszStrmDrvrAuthMode == NULL ? modConf->pszStrmDrvrAuthMode : inst->pszStrmDrvrAuthMode;
}

static rsRetVal applySecureDefaultsToModuleConfig(modConfData_t *const modConf, const sbool emitDiagnostics) {
    return applySecureDefaultsToStreamDriver(modConf->pConf, &modConf->iStrmDrvrMode, modConf->bStrmDrvrModeSet,
                                             getEffectiveModuleStreamDriver(modConf), "imtcp module", emitDiagnostics);
}

static rsRetVal applySecureDefaultsToInstanceConfig(instanceConf_t *const inst,
                                                    const modConfData_t *const modConf,
                                                    const sbool emitDiagnostics) {
    return applySecureDefaultsToStreamDriver(modConf->pConf, &inst->iStrmDrvrMode, inst->bStrmDrvrModeSet,
                                             getEffectiveInstanceStreamDriver(inst, modConf), "imtcp input",
                                             emitDiagnostics);
}

static rsRetVal applySecureDefaultsToZstdWindow(instanceConf_t *const inst,
                                                const modConfData_t *const modConf,
                                                const sbool emitDiagnostics) {
    const int secure_policy = modConf->pConf->globals.compatDefaultsSecure;

    if (inst->compressionMode != TCPSRV_COMPRESS_STREAM_ALWAYS ||
        inst->compressionDriver != TCPSRV_COMPRESS_DRIVER_ZSTD) {
        return RS_RET_OK;
    }

    if (secure_policy == COMPAT_DEFAULTS_SECURE_STRICT &&
        (!inst->compressionMaxTotalZstdWindowBytesSet || inst->compressionMaxTotalZstdWindowBytes == 0)) {
        if (emitDiagnostics)
            LogError(0, RS_RET_PARAM_ERROR,
                     "imtcp input: compatibility.defaults.secure=\"strict\" requires an explicit positive "
                     "compression.maxTotalZstdWindowBytes value for zstd stream decompression");
        return RS_RET_PARAM_ERROR;
    }

    return RS_RET_OK;
}

static rsRetVal setLegacyStrmDrvrMode(void __attribute__((unused)) * pVal, int mode) {
    cs.iStrmDrvrMode = mode;
    cs.bStrmDrvrModeSet = 1;
    return RS_RET_OK;
}

/* callbacks */
/* this shall go into a specific ACL module! */
static int isPermittedHost(struct sockaddr *addr,
                           char *fromHostFQDN,
                           void *pUsrSrv,
                           void __attribute__((unused)) * pUsrSess) {
    const tcpLstnParams_t *const cnf_params = pUsrSrv;

    if (cnf_params != NULL && !cnf_params->bUseLegacyAllowedSender) {
        return net.isAllowedSenderList(cnf_params->pAllowedSenderRoot, addr, fromHostFQDN, 1);
    }
    return net.isAllowedSender2(UCHAR_CONSTANT("TCP"), addr, fromHostFQDN, 1);
}


static rsRetVal doOpenLstnSocks(tcpsrv_t *pSrv) {
    ISOBJ_TYPE_assert(pSrv, tcpsrv);
    dbgprintf("in imtcp doOpenLstnSocks\n");
    return tcpsrv.create_tcp_socket(pSrv);
}


static rsRetVal doRcvData(
    tcps_sess_t *pSess, char *buf, size_t lenBuf, ssize_t *piLenRcvd, int *const oserr, unsigned *nextIODirection) {
    assert(pSess != NULL);
    assert(piLenRcvd != NULL);
    *piLenRcvd = lenBuf;
    return netstrm.Rcv(pSess->pStrm, (uchar *)buf, piLenRcvd, oserr, nextIODirection);
}

static rsRetVal onRegularClose(tcps_sess_t *pSess) {
    DEFiRet;
    rsRetVal closeRet;
    assert(pSess != NULL);

    /* process any incomplete frames left over */
    iRet = tcps_sess.PrepareClose(pSess);
    /* Session closed */
    closeRet = tcps_sess.Close(pSess);
    if (iRet == RS_RET_OK) iRet = closeRet;
    RETiRet;
}


static rsRetVal onErrClose(tcps_sess_t *pSess) {
    DEFiRet;
    assert(pSess != NULL);

    tcps_sess.Close(pSess);
    RETiRet;
}

/* ------------------------------ end callbacks ------------------------------ */


/* set permitted peer -- rgerhards, 2008-05-19
 */
static rsRetVal setPermittedPeer(void __attribute__((unused)) * pVal, uchar *pszID) {
    DEFiRet;
    CHKiRet(net.AddPermittedPeer(&pPermPeersRoot, pszID));
    free(pszID); /* no longer needed, but we need to free as of interface def */
finalize_it:
    RETiRet;
}

/* Keep effective defaults in side-effect-free helpers so normal startup and
 * private reload lowering cannot drift apart. Pointer fields remain NULL from
 * calloc and are populated only by the normal or private parameter pass. */
static void initModuleDefaults(modConfData_t *const config) {
    config->iTCPSessMax = 200;
    config->iTCPLstnMax = 20;
    config->numWrkr = DEFAULT_NUMWRKR;
    config->starvationMaxReads = DEFAULT_STARVATIONMAXREADS;
    config->bSuppOctetFram = 1;
    config->iStrmDrvrMode = 0;
    config->bUseFlowControl = 1;
    config->iAddtlFrameDelim = TCPSRV_NO_ADDTL_DELIMITER;
    config->maxFrameSize = 200000;
    config->bPreserveCase = 1;
    config->compressionMode = TCPSRV_COMPRESS_NEVER;
    config->compressionDriver = TCPSRV_COMPRESS_DRIVER_ZLIB;
    config->compressionMaxExpansionRatio = TCPSRV_COMPRESS_MAX_EXPANSION_RATIO_DEFAULT;
    config->compressionMaxDecompressedBytesPerReceive = TCPSRV_COMPRESS_MAX_DECOMPRESSED_BYTES_PER_RECEIVE_DEFAULT;
    config->compressionMaxTotalZstdWindowBytes = TCPSRV_COMPRESS_MAX_TOTAL_ZSTD_WINDOW_BYTES_DEFAULT;
}

static void initInstanceDefaults(instanceConf_t *const inst, const modConfData_t *const moduleConfig) {
    inst->cnf_params->bSuppOctetFram = FRAMING_UNSET;
    inst->ratelimitInterval = -1;
    inst->ratelimitBurst = -1;
    inst->iStrmDrvrMode = moduleConfig->iStrmDrvrMode;
    inst->bStrmDrvrModeSet = moduleConfig->bStrmDrvrModeSet;
    inst->iStrmDrvrExtendedCertCheck = moduleConfig->iStrmDrvrExtendedCertCheck;
    inst->iStrmDrvrSANPreference = moduleConfig->iStrmDrvrSANPreference;
    inst->iStrmTlsVerifyDepth = moduleConfig->iStrmTlsVerifyDepth;
    inst->iStrmTlsRevocationCheck = moduleConfig->iStrmTlsRevocationCheck;
    inst->bKeepAlive = moduleConfig->bKeepAlive;
    inst->iKeepAliveIntvl = moduleConfig->iKeepAliveIntvl;
    inst->iKeepAliveProbes = moduleConfig->iKeepAliveProbes;
    inst->iKeepAliveTime = moduleConfig->iKeepAliveTime;
    inst->iAddtlFrameDelim = moduleConfig->iAddtlFrameDelim;
    inst->maxFrameSize = moduleConfig->maxFrameSize;
    inst->bUseFlowControl = moduleConfig->bUseFlowControl;
    inst->bDisableLFDelim = moduleConfig->bDisableLFDelim;
    inst->discardTruncatedMsg = moduleConfig->discardTruncatedMsg;
    inst->bEmitMsgOnClose = moduleConfig->bEmitMsgOnClose;
    inst->bEmitMsgOnOpen = moduleConfig->bEmitMsgOnOpen;
    inst->bPreserveCase = moduleConfig->bPreserveCase;
    inst->iTCPLstnMax = moduleConfig->iTCPLstnMax;
    inst->iTCPSessMax = moduleConfig->iTCPSessMax;
    inst->numWrkr = moduleConfig->numWrkr;
    inst->starvationMaxReads = moduleConfig->starvationMaxReads;
    inst->compressionMode = moduleConfig->compressionMode;
    inst->compressionDriver = moduleConfig->compressionDriver;
    inst->compressionMaxExpansionRatio = moduleConfig->compressionMaxExpansionRatio;
    inst->compressionMaxDecompressedBytesPerReceive = moduleConfig->compressionMaxDecompressedBytesPerReceive;
    inst->compressionMaxTotalZstdWindowBytes = moduleConfig->compressionMaxTotalZstdWindowBytes;
    inst->compressionMaxTotalZstdWindowBytesSet = moduleConfig->compressionMaxTotalZstdWindowBytesSet;
}


/* create input instance, set default parameters, and
 * add it to the list of instances.
 */
/* Allocate an owned input configuration without touching loadModConf or
 * appending it to a live parse tree. Candidate lowering uses this boundary;
 * callers own the result until they explicitly append or destroy it. */
static rsRetVal createDetachedInstance(const modConfData_t *const moduleConfig, instanceConf_t **const pinst) {
    instanceConf_t *inst = NULL;

    DEFiRet;
    if (moduleConfig == NULL || pinst == NULL || *pinst != NULL) ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    CHKmalloc(inst = (instanceConf_t *)calloc(1, sizeof(instanceConf_t)));
    CHKmalloc(inst->cnf_params = (tcpLstnParams_t *)calloc(1, sizeof(tcpLstnParams_t)));
    initInstanceDefaults(inst, moduleConfig);

    *pinst = inst;
    inst = NULL;
finalize_it:
    if (iRet != RS_RET_OK) {
        if (inst != NULL) free(inst->cnf_params);
        free(inst);
    }
    RETiRet;
}

static rsRetVal createInstance(instanceConf_t **const pinst) {
    instanceConf_t *inst = NULL;
    DEFiRet;

    if (loadModConf == NULL) return RS_RET_PARAM_ERROR;
    CHKiRet(createDetachedInstance(loadModConf, &inst));
    if (loadModConf->tail == NULL)
        loadModConf->tail = loadModConf->root = inst;
    else {
        loadModConf->tail->next = inst;
        loadModConf->tail = inst;
    }
    *pinst = inst;
    inst = NULL;
finalize_it:
    if (inst != NULL) {
        free(inst->cnf_params);
        free(inst);
    }
    RETiRet;
}


/* This function is called when a new listener instance shall be added to
 * the current config object via the legacy config system. It just shuffles
 * all parameters to the listener in-memory instance.
 * rgerhards, 2011-05-04
 */
static rsRetVal addInstance(void __attribute__((unused)) * pVal, uchar *pNewVal) {
    instanceConf_t *inst;
    DEFiRet;

    CHKiRet(createInstance(&inst));

    CHKmalloc(inst->cnf_params->pszPort = ustrdup((pNewVal == NULL || *pNewVal == '\0') ? (uchar *)"10514" : pNewVal));
    if ((cs.pszBindRuleset == NULL) || (cs.pszBindRuleset[0] == '\0')) {
        inst->pszBindRuleset = NULL;
    } else {
        CHKmalloc(inst->pszBindRuleset = ustrdup(cs.pszBindRuleset));
    }
    if ((cs.lstnIP == NULL) || (cs.lstnIP[0] == '\0')) {
        inst->cnf_params->pszAddr = NULL;
    } else {
        CHKmalloc(inst->cnf_params->pszAddr = ustrdup(cs.lstnIP));
    }
    if ((cs.lstnPortFile == NULL) || (cs.lstnPortFile[0] == '\0')) {
        inst->cnf_params->pszLstnPortFileName = NULL;
    } else {
        CHKmalloc(inst->cnf_params->pszLstnPortFileName = ustrdup(cs.lstnPortFile));
    }

    if ((cs.pszInputName == NULL) || (cs.pszInputName[0] == '\0')) {
        inst->pszInputName = NULL;
    } else {
        CHKmalloc(inst->pszInputName = ustrdup(cs.pszInputName));
    }
    inst->cnf_params->bSuppOctetFram = cs.bSuppOctetFram;
    inst->iStrmDrvrMode = cs.iStrmDrvrMode;
    inst->bStrmDrvrModeSet = cs.bStrmDrvrModeSet;
    inst->bKeepAlive = cs.bKeepAlive;
    inst->bUseFlowControl = cs.bUseFlowControl;
    inst->bDisableLFDelim = cs.bDisableLFDelim;
    inst->bEmitMsgOnClose = cs.bEmitMsgOnClose;
    inst->bPreserveCase = cs.bPreserveCase;
    inst->iKeepAliveProbes = cs.iKeepAliveProbes;
    inst->iKeepAliveIntvl = cs.iKeepAliveIntvl;
    inst->iKeepAliveTime = cs.iKeepAliveTime;
    inst->iAddtlFrameDelim = cs.iAddtlFrameDelim;
    inst->iTCPLstnMax = cs.iTCPLstnMax;
    inst->iTCPSessMax = cs.iTCPSessMax;
    inst->numWrkr = DEFAULT_NUMWRKR;
    inst->starvationMaxReads = DEFAULT_STARVATIONMAXREADS;
    inst->compressionMode = cs.compressionMode;
    inst->compressionDriver = cs.compressionDriver;
    inst->compressionMaxExpansionRatio = cs.compressionMaxExpansionRatio;
    inst->compressionMaxDecompressedBytesPerReceive = cs.compressionMaxDecompressedBytesPerReceive;
    inst->compressionMaxTotalZstdWindowBytes = cs.compressionMaxTotalZstdWindowBytes;
    inst->compressionMaxTotalZstdWindowBytesSet = cs.compressionMaxTotalZstdWindowBytesSet;
    CHKiRet(applySecureDefaultsToInstanceConfig(inst, loadModConf, RSTRUE));
    CHKiRet(applySecureDefaultsToZstdWindow(inst, loadModConf, RSTRUE));
    warnIfInsecureListenerConfigured(inst->iStrmDrvrMode, getEffectiveInstanceStreamDriver(inst, loadModConf),
                                     getEffectiveInstanceAuthMode(inst, loadModConf));

finalize_it:
    free(pNewVal);
    RETiRet;
}


static rsRetVal addListner(modConfData_t *modConf, instanceConf_t *inst) {
    DEFiRet;
    uchar *psz; /* work variable */
    char *ns; /**< network namespace */
    permittedPeers_t *peers;

    tcpsrv_t *pOurTcpsrv = NULL;
    CHKiRet(tcpsrv.Construct(&pOurTcpsrv));
    /* callbacks */
    CHKiRet(tcpsrv.SetCBIsPermittedHost(pOurTcpsrv, isPermittedHost));
    CHKiRet(tcpsrv.SetCBRcvData(pOurTcpsrv, doRcvData));
    CHKiRet(tcpsrv.SetCBOpenLstnSocks(pOurTcpsrv, doOpenLstnSocks));
    CHKiRet(tcpsrv.SetCBOnRegularClose(pOurTcpsrv, onRegularClose));
    CHKiRet(tcpsrv.SetCBOnErrClose(pOurTcpsrv, onErrClose));
    /* params */
    CHKiRet(tcpsrv.SetNumWrkr(pOurTcpsrv, inst->numWrkr));
    CHKiRet(tcpsrv.SetStarvationMaxReads(pOurTcpsrv, inst->starvationMaxReads));
    CHKiRet(tcpsrv.SetKeepAlive(pOurTcpsrv, inst->bKeepAlive));
    CHKiRet(tcpsrv.SetKeepAliveIntvl(pOurTcpsrv, inst->iKeepAliveIntvl));
    CHKiRet(tcpsrv.SetKeepAliveProbes(pOurTcpsrv, inst->iKeepAliveProbes));
    CHKiRet(tcpsrv.SetKeepAliveTime(pOurTcpsrv, inst->iKeepAliveTime));
    CHKiRet(tcpsrv.SetSessMax(pOurTcpsrv, inst->iTCPSessMax));
    CHKiRet(tcpsrv.SetLstnMax(pOurTcpsrv, inst->iTCPLstnMax));
    CHKiRet(tcpsrv.SetDrvrMode(pOurTcpsrv, inst->iStrmDrvrMode));
    CHKiRet(tcpsrv.SetDrvrCheckExtendedKeyUsage(pOurTcpsrv, inst->iStrmDrvrExtendedCertCheck));
    CHKiRet(tcpsrv.SetDrvrPrioritizeSAN(pOurTcpsrv, inst->iStrmDrvrSANPreference));
    CHKiRet(tcpsrv.SetDrvrTlsVerifyDepth(pOurTcpsrv, inst->iStrmTlsVerifyDepth));
    CHKiRet(tcpsrv.SetDrvrTlsRevocationCheck(pOurTcpsrv, inst->iStrmTlsRevocationCheck));
    CHKiRet(tcpsrv.SetUseFlowControl(pOurTcpsrv, inst->bUseFlowControl));
    CHKiRet(tcpsrv.SetAddtlFrameDelim(pOurTcpsrv, inst->iAddtlFrameDelim));
    CHKiRet(tcpsrv.SetMaxFrameSize(pOurTcpsrv, inst->maxFrameSize));
    CHKiRet(tcpsrv.SetCompressionMode(pOurTcpsrv, inst->compressionMode));
    CHKiRet(tcpsrv.SetCompressionDriver(pOurTcpsrv, inst->compressionDriver));
    CHKiRet(tcpsrv.SetCompressionMaxExpansionRatio(pOurTcpsrv, inst->compressionMaxExpansionRatio));
    CHKiRet(tcpsrv.SetCompressionMaxDecompressedBytesPerReceive(pOurTcpsrv,
                                                                inst->compressionMaxDecompressedBytesPerReceive));
    CHKiRet(tcpsrv.SetCompressionMaxTotalZstdWindowBytes(pOurTcpsrv, inst->compressionMaxTotalZstdWindowBytes));
    CHKiRet(tcpsrv.SetbDisableLFDelim(pOurTcpsrv, inst->bDisableLFDelim));
    CHKiRet(tcpsrv.SetDiscardTruncatedMsg(pOurTcpsrv, inst->discardTruncatedMsg));
    CHKiRet(tcpsrv.SetNotificationOnRemoteClose(pOurTcpsrv, inst->bEmitMsgOnClose));
    CHKiRet(tcpsrv.SetNotificationOnRemoteOpen(pOurTcpsrv, inst->bEmitMsgOnOpen));
    CHKiRet(tcpsrv.SetPreserveCase(pOurTcpsrv, inst->bPreserveCase));
    CHKiRet(tcpsrv.SetSynBacklog(pOurTcpsrv, inst->iSynBacklog));
    /* now set optional params, but only if they were actually configured */
    psz = (inst->pszStrmDrvrName == NULL) ? modConf->pszStrmDrvrName : inst->pszStrmDrvrName;
    if (psz != NULL) {
        CHKiRet(tcpsrv.SetDrvrName(pOurTcpsrv, psz));
    }
    psz = (inst->pszStrmDrvrAuthMode == NULL) ? modConf->pszStrmDrvrAuthMode : inst->pszStrmDrvrAuthMode;
    if (psz != NULL) {
        CHKiRet(tcpsrv.SetDrvrAuthMode(pOurTcpsrv, psz));
    }
    psz = (inst->gnutlsPriorityString == NULL) ? modConf->gnutlsPriorityString : inst->gnutlsPriorityString;
    CHKiRet(tcpsrv.SetGnutlsPriorityString(pOurTcpsrv, psz));
    /* Call SetDrvrPermitExpiredCerts required
     * when param is NULL default handling for ExpiredCerts is set! */
    psz = (inst->pszStrmDrvrPermitExpiredCerts == NULL) ? modConf->pszStrmDrvrPermitExpiredCerts
                                                        : inst->pszStrmDrvrPermitExpiredCerts;
    CHKiRet(tcpsrv.SetDrvrPermitExpiredCerts(pOurTcpsrv, psz));

    psz = (inst->pszStrmDrvrCAFile == NULL) ? modConf->pszStrmDrvrCAFile : inst->pszStrmDrvrCAFile;
    CHKiRet(tcpsrv.SetDrvrCAFile(pOurTcpsrv, psz));

    psz = (inst->pszStrmDrvrCRLFile == NULL) ? modConf->pszStrmDrvrCRLFile : inst->pszStrmDrvrCRLFile;
    CHKiRet(tcpsrv.SetDrvrCRLFile(pOurTcpsrv, psz));

    psz = (inst->pszStrmDrvrKeyFile == NULL) ? modConf->pszStrmDrvrKeyFile : inst->pszStrmDrvrKeyFile;
    CHKiRet(tcpsrv.SetDrvrKeyFile(pOurTcpsrv, psz));

    psz = (inst->pszStrmDrvrCertFile == NULL) ? modConf->pszStrmDrvrCertFile : inst->pszStrmDrvrCertFile;
    CHKiRet(tcpsrv.SetDrvrCertFile(pOurTcpsrv, psz));

    peers = (inst->pPermPeersRoot == NULL) ? modConf->pPermPeersRoot : inst->pPermPeersRoot;
    if (peers != NULL) {
        CHKiRet(tcpsrv.SetDrvrPermPeers(pOurTcpsrv, peers));
    }

    /* initialized, now add socket and listener params */
    DBGPRINTF("imtcp: trying to add port *:%s\n", inst->cnf_params->pszPort);
    inst->cnf_params->pRuleset = inst->pBindRuleset;

    ns = (inst->pszNetworkNamespace == NULL) ? modConf->pszNetworkNamespace : inst->pszNetworkNamespace;
    CHKiRet(tcpsrv.SetNetworkNamespace(pOurTcpsrv, inst->cnf_params, ns));

    CHKiRet(tcpsrv.SetInputName(pOurTcpsrv, inst->cnf_params,
                                inst->pszInputName == NULL ? UCHAR_CONSTANT("imtcp") : inst->pszInputName));
    CHKiRet(tcpsrv.SetOrigin(pOurTcpsrv, (uchar *)"imtcp"));
    CHKiRet(tcpsrv.SetDfltTZ(pOurTcpsrv, (inst->dfltTZ == NULL) ? (uchar *)"" : inst->dfltTZ));
    CHKiRet(tcpsrv.SetbSPFramingFix(pOurTcpsrv, inst->bSPFramingFix));
    CHKiRet(tcpsrv.SetLinuxLikeRatelimiters(pOurTcpsrv, inst->ratelimitInterval, inst->ratelimitBurst));

    if ((ustrcmp(inst->cnf_params->pszPort, UCHAR_CONSTANT("0")) == 0 &&
         inst->cnf_params->pszLstnPortFileName == NULL) ||
        ustrcmp(inst->cnf_params->pszPort, UCHAR_CONSTANT("0")) < 0) {
        uchar *newPort = NULL;
        if (runConf->globals.bRequireAllInputs) {
            LogError(0, RS_RET_INVALID_PORT, "imtcp: port 0 needs listenPortFileName in require-all mode");
            ABORT_FINALIZE(RS_RET_INVALID_PORT);
        }
        LogMsg(0, RS_RET_OK, LOG_WARNING, "imtcp: port 0 and no port file set -> using port 514 instead");
        CHKmalloc(newPort = (uchar *)strdup("514"));
        free((void *)inst->cnf_params->pszPort);
        inst->cnf_params->pszPort = newPort;
    }
    if (inst->bAllowedSendersSet) {
        inst->cnf_params->pAllowedSenderRoot = inst->pAllowedSendersRoot;
        inst->cnf_params->bUseLegacyAllowedSender = 0;
    } else if (modConf->bAllowedSendersSet) {
        inst->cnf_params->pAllowedSenderRoot = modConf->pAllowedSendersRoot;
        inst->cnf_params->bUseLegacyAllowedSender = 0;
    } else {
        inst->cnf_params->pAllowedSenderRoot = NULL;
        inst->cnf_params->bUseLegacyAllowedSender = 1;
    }
    CHKiRet(tcpsrv.SetUsrP(pOurTcpsrv, inst->cnf_params));
    tcpLstnParams_t *const listenerParams = inst->cnf_params;
    iRet = tcpsrv.configureTCPListen(pOurTcpsrv, listenerParams);
    inst->cnf_params = NULL; /* ownership transferred to tcpsrv, including setup failure cleanup */
    CHKiRet(iRet);

    CHKiRet(endpointRegistryAdd(pOurTcpsrv, listenerParams, ns,
                                inst->pszInputName == NULL ? UCHAR_CONSTANT("imtcp") : inst->pszInputName));
    pOurTcpsrv = NULL; /* endpoint registry owns the configured runtime server */

finalize_it:
    if (iRet != RS_RET_OK) {
        LogError(0, NO_ERRCODE, "imtcp: error %d trying to add listener", iRet);
        if (pOurTcpsrv != NULL) {
            tcpsrv.SetUsrP(pOurTcpsrv, NULL);
            tcpsrv.Destruct(&pOurTcpsrv);
        }
    }
    RETiRet;
}


/* Apply the parsed input parameter block to an already initialized, owned
 * instance.  The caller controls whether that instance is detached or linked
 * into the normal load configuration. */
static rsRetVal applyInputParams(const modConfData_t *const moduleConfig,
                                 instanceConf_t *const inst,
                                 struct cnfparamvals *const pvals,
                                 const sbool emitDiagnostics) {
    int i;
    DEFiRet;

    if (moduleConfig == NULL || inst == NULL || inst->cnf_params == NULL || pvals == NULL) return RS_RET_PARAM_ERROR;

    for (i = 0; i < inppblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;
        if (!strcmp(inppblk.descr[i].name, "port")) {
            CHKmalloc(inst->cnf_params->pszPort = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(inppblk.descr[i].name, "networknamespace")) {
            CHKmalloc(inst->pszNetworkNamespace = es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(inppblk.descr[i].name, "address")) {
            CHKmalloc(inst->cnf_params->pszAddr = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(inppblk.descr[i].name, "name")) {
            CHKmalloc(inst->pszInputName = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(inppblk.descr[i].name, "defaulttz")) {
            CHKmalloc(inst->dfltTZ = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(inppblk.descr[i].name, "framingfix.cisco.asa")) {
            inst->bSPFramingFix = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "ruleset")) {
            CHKmalloc(inst->pszBindRuleset = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(inppblk.descr[i].name, "streamdriver.mode")) {
            inst->iStrmDrvrMode = (int)pvals[i].val.d.n;
            inst->bStrmDrvrModeSet = 1;
        } else if (!strcmp(inppblk.descr[i].name, "streamdriver.CheckExtendedKeyPurpose")) {
            inst->iStrmDrvrExtendedCertCheck = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "streamdriver.PrioritizeSAN")) {
            inst->iStrmDrvrSANPreference = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "streamdriver.TlsVerifyDepth")) {
            if (pvals[i].val.d.n >= 2) {
                inst->iStrmTlsVerifyDepth = (int)pvals[i].val.d.n;
            } else {
                if (emitDiagnostics)
                    parser_errmsg("streamdriver.TlsVerifyDepth must be 2 or higher but is %d", (int)pvals[i].val.d.n);
                else
                    ABORT_FINALIZE(RS_RET_INVALID_PARAMS);
            }
        } else if (!strcmp(inppblk.descr[i].name, "streamdriver.TlsRevocationCheck")) {
            inst->iStrmTlsRevocationCheck = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "streamdriver.authmode")) {
            CHKmalloc(inst->pszStrmDrvrAuthMode = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(inppblk.descr[i].name, "streamdriver.permitexpiredcerts")) {
            CHKmalloc(inst->pszStrmDrvrPermitExpiredCerts = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(inppblk.descr[i].name, "streamdriver.cafile")) {
            CHKmalloc(inst->pszStrmDrvrCAFile = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(inppblk.descr[i].name, "streamdriver.crlfile")) {
            CHKmalloc(inst->pszStrmDrvrCRLFile = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(inppblk.descr[i].name, "streamdriver.keyfile")) {
            CHKmalloc(inst->pszStrmDrvrKeyFile = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(inppblk.descr[i].name, "streamdriver.certfile")) {
            CHKmalloc(inst->pszStrmDrvrCertFile = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(inppblk.descr[i].name, "streamdriver.name")) {
            CHKmalloc(inst->pszStrmDrvrName = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(inppblk.descr[i].name, "starvationprotection.maxreads")) {
            inst->starvationMaxReads = (unsigned)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "gnutlsprioritystring")) {
            CHKmalloc(inst->gnutlsPriorityString = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(inppblk.descr[i].name, "permittedpeer")) {
            for (int j = 0; j < pvals[i].val.d.ar->nmemb; ++j) {
                uchar *const peer = (uchar *)es_str2cstr(pvals[i].val.d.ar->arr[j], NULL);
                CHKiRet(net.AddPermittedPeer(&inst->pPermPeersRoot, peer));
                free(peer);
            }
        } else if (!strcmp(inppblk.descr[i].name, "allowedsender")) {
            if (pvals[i].val.d.ar == NULL || pvals[i].val.d.ar->nmemb == 0) {
                if (emitDiagnostics) LogError(0, RS_RET_INVALID_PARAMS, "imtcp: allowedSender array must not be empty");
                ABORT_FINALIZE(RS_RET_INVALID_PARAMS);
            }
            inst->bAllowedSendersSet = 1;
            for (int j = 0; j < pvals[i].val.d.ar->nmemb; ++j) {
                uchar *sender = (uchar *)es_str2cstr(pvals[i].val.d.ar->arr[j], NULL);
                CHKmalloc(sender);
                const rsRetVal entryRet =
                    net.addAllowedSenderEntry(&inst->pAllowedSendersRoot, &inst->pAllowedSendersLast, sender);
                free(sender);
                CHKiRet(entryRet);
            }
        } else if (!strcmp(inppblk.descr[i].name, "flowcontrol")) {
            inst->bUseFlowControl = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "disablelfdelimiter")) {
            inst->bDisableLFDelim = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "discardtruncatedmsg")) {
            inst->discardTruncatedMsg = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "notifyonconnectionclose")) {
            inst->bEmitMsgOnClose = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "notifyonconnectionopen")) {
            inst->bEmitMsgOnOpen = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "addtlframedelimiter")) {
            inst->iAddtlFrameDelim = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "maxframesize")) {
            const int max = (int)pvals[i].val.d.n;
            CHKiRet(validateMaxFrameSize(max, emitDiagnostics));
            inst->maxFrameSize = max;
        } else if (!strcmp(inppblk.descr[i].name, "maxsessions")) {
            inst->iTCPSessMax = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "maxlisteners")) {
            inst->iTCPLstnMax = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "workerthreads")) {
            inst->numWrkr = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "supportoctetcountedframing")) {
            inst->cnf_params->bSuppOctetFram = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "keepalive")) {
            inst->bKeepAlive = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "keepalive.probes")) {
            inst->iKeepAliveProbes = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "keepalive.time")) {
            inst->iKeepAliveTime = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "keepalive.interval")) {
            inst->iKeepAliveIntvl = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "ratelimit.burst")) {
            inst->ratelimitBurst = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "ratelimit.interval")) {
            inst->ratelimitInterval = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "ratelimit.name")) {
            CHKmalloc(inst->cnf_params->pszRatelimitName = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(inppblk.descr[i].name, "preservecase")) {
            inst->bPreserveCase = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "socketbacklog")) {
            inst->iSynBacklog = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "listenportfilename")) {
            CHKmalloc(inst->cnf_params->pszLstnPortFileName = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(inppblk.descr[i].name, "compression.mode")) {
            CHKiRet(parseCompressionMode(pvals[i].val.d.estr, &inst->compressionMode, emitDiagnostics));
        } else if (!strcmp(inppblk.descr[i].name, "compression.driver")) {
            CHKiRet(parseCompressionDriver(pvals[i].val.d.estr, &inst->compressionDriver, emitDiagnostics));
        } else if (!strcmp(inppblk.descr[i].name, "compression.maxexpansionratio")) {
            inst->compressionMaxExpansionRatio = (uint64_t)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "compression.maxdecompressedbytesperreceive")) {
            inst->compressionMaxDecompressedBytesPerReceive = (uint64_t)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "compression.maxtotalzstdwindowbytes")) {
            inst->compressionMaxTotalZstdWindowBytes = (uint64_t)pvals[i].val.d.n;
            inst->compressionMaxTotalZstdWindowBytesSet = RSTRUE;
        } else if (!strcmp(inppblk.descr[i].name, "multiline")) {
            inst->cnf_params->bMultiLine = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "framing.delimiter.regex")) {
            CHKmalloc(inst->cnf_params->pszStartRegex = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else {
            dbgprintf(
                "imtcp: program error, non-handled "
                "param '%s'\n",
                inppblk.descr[i].name);
        }
    }

    if (inst->cnf_params->pszRatelimitName != NULL) {
        if (inst->ratelimitInterval != -1 || inst->ratelimitBurst != -1) {
            if (emitDiagnostics)
                LogError(0, RS_RET_INVALID_PARAMS,
                         "imtcp: ratelimit.name is mutually exclusive with "
                         "ratelimit.interval and ratelimit.burst - using named "
                         "ratelimit");
        }
    } else {
        if (inst->ratelimitInterval == -1) {
            inst->ratelimitInterval = 0; /* off by default */
        }
        if (inst->ratelimitBurst == -1) {
            inst->ratelimitBurst = 10000;
        }
    }
    CHKiRet(applySecureDefaultsToInstanceConfig(inst, moduleConfig, emitDiagnostics));
    CHKiRet(applySecureDefaultsToZstdWindow(inst, moduleConfig, emitDiagnostics));
    if (emitDiagnostics)
        warnIfInsecureListenerConfigured(inst->iStrmDrvrMode, getEffectiveInstanceStreamDriver(inst, moduleConfig),
                                         getEffectiveInstanceAuthMode(inst, moduleConfig));

finalize_it:
    RETiRet;
}


BEGINnewInpInst
    struct cnfparamvals *pvals = NULL;
    instanceConf_t *inst = NULL;
    CODESTARTnewInpInst;
    DBGPRINTF("newInpInst (imtcp)\n");

    pvals = nvlstGetParams(lst, &inppblk, NULL);
    if (pvals == NULL) {
        LogError(0, RS_RET_MISSING_CNFPARAMS, "imtcp: required parameter are missing\n");
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }

    if (Debug) {
        dbgprintf("input param blk in imtcp:\n");
        cnfparamsPrint(&inppblk, pvals);
    }

    CHKiRet(createInstance(&inst));
    CHKiRet(applyInputParams(loadModConf, inst, pvals, RSTRUE));

finalize_it:
    CODE_STD_FINALIZERnewInpInst if (pvals != NULL) cnfparamvalsDestruct(pvals, &inppblk);
ENDnewInpInst


BEGINbeginCnfLoad
    CODESTARTbeginCnfLoad;
    loadModConf = pModConf;
    pModConf->pConf = pConf;
    initModuleDefaults(loadModConf);
    bLegacyCnfModGlobalsPermitted = 1;
    /* init legacy config variables */
    resetConfigVariables(NULL, NULL); /* dummy parameters just to fulfill interface def */
ENDbeginCnfLoad


/* Apply a parsed module parameter block to the explicitly supplied
 * configuration generation.  No load-list selection happens here. */
static rsRetVal applyModuleParams(modConfData_t *const moduleConfig,
                                  struct cnfparamvals *const pvals,
                                  const sbool emitDiagnostics) {
    int i;
    DEFiRet;

    if (moduleConfig == NULL || pvals == NULL) return RS_RET_PARAM_ERROR;

    for (i = 0; i < modpblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;
        if (!strcmp(modpblk.descr[i].name, "flowcontrol")) {
            moduleConfig->bUseFlowControl = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "disablelfdelimiter")) {
            moduleConfig->bDisableLFDelim = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "discardtruncatedmsg")) {
            moduleConfig->discardTruncatedMsg = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "octetcountedframing")) {
            moduleConfig->bSuppOctetFram = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "notifyonconnectionclose")) {
            moduleConfig->bEmitMsgOnClose = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "notifyonconnectionopen")) {
            moduleConfig->bEmitMsgOnOpen = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "addtlframedelimiter")) {
            moduleConfig->iAddtlFrameDelim = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "maxframesize")) {
            const int max = (int)pvals[i].val.d.n;
            CHKiRet(validateMaxFrameSize(max, emitDiagnostics));
            moduleConfig->maxFrameSize = max;
        } else if (!strcmp(modpblk.descr[i].name, "maxsessions")) {
            moduleConfig->iTCPSessMax = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "starvationprotection.maxreads")) {
            moduleConfig->starvationMaxReads = (unsigned)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "maxlisteners") ||
                   !strcmp(modpblk.descr[i].name, "maxlistners")) { /* keep old name for a while */
            moduleConfig->iTCPLstnMax = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "workerthreads")) {
            moduleConfig->numWrkr = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "keepalive")) {
            moduleConfig->bKeepAlive = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "keepalive.probes")) {
            moduleConfig->iKeepAliveProbes = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "keepalive.time")) {
            moduleConfig->iKeepAliveTime = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "keepalive.interval")) {
            moduleConfig->iKeepAliveIntvl = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "gnutlsprioritystring")) {
            CHKmalloc(moduleConfig->gnutlsPriorityString = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(modpblk.descr[i].name, "networknamespace")) {
            CHKmalloc(moduleConfig->pszNetworkNamespace = es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(modpblk.descr[i].name, "streamdriver.mode")) {
            moduleConfig->iStrmDrvrMode = (int)pvals[i].val.d.n;
            moduleConfig->bStrmDrvrModeSet = 1;
        } else if (!strcmp(modpblk.descr[i].name, "streamdriver.CheckExtendedKeyPurpose")) {
            moduleConfig->iStrmDrvrExtendedCertCheck = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "streamdriver.PrioritizeSAN")) {
            moduleConfig->iStrmDrvrSANPreference = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "streamdriver.TlsVerifyDepth")) {
            if (pvals[i].val.d.n >= 2) {
                moduleConfig->iStrmTlsVerifyDepth = (int)pvals[i].val.d.n;
            } else {
                if (emitDiagnostics)
                    parser_errmsg("streamdriver.TlsVerifyDepth must be 2 or higher but is %d", (int)pvals[i].val.d.n);
                else
                    ABORT_FINALIZE(RS_RET_INVALID_PARAMS);
            }
        } else if (!strcmp(modpblk.descr[i].name, "streamdriver.TlsRevocationCheck")) {
            moduleConfig->iStrmTlsRevocationCheck = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "streamdriver.authmode")) {
            CHKmalloc(moduleConfig->pszStrmDrvrAuthMode = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(modpblk.descr[i].name, "streamdriver.permitexpiredcerts")) {
            CHKmalloc(moduleConfig->pszStrmDrvrPermitExpiredCerts = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(modpblk.descr[i].name, "streamdriver.cafile")) {
            CHKmalloc(moduleConfig->pszStrmDrvrCAFile = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(modpblk.descr[i].name, "streamdriver.crlfile")) {
            CHKmalloc(moduleConfig->pszStrmDrvrCRLFile = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(modpblk.descr[i].name, "streamdriver.keyfile")) {
            CHKmalloc(moduleConfig->pszStrmDrvrKeyFile = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(modpblk.descr[i].name, "streamdriver.certfile")) {
            CHKmalloc(moduleConfig->pszStrmDrvrCertFile = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(modpblk.descr[i].name, "streamdriver.name")) {
            CHKmalloc(moduleConfig->pszStrmDrvrName = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(modpblk.descr[i].name, "permittedpeer")) {
            for (int j = 0; j < pvals[i].val.d.ar->nmemb; ++j) {
                uchar *const peer = (uchar *)es_str2cstr(pvals[i].val.d.ar->arr[j], NULL);
                CHKiRet(net.AddPermittedPeer(&moduleConfig->pPermPeersRoot, peer));
                free(peer);
            }
        } else if (!strcmp(modpblk.descr[i].name, "allowedsender")) {
            if (pvals[i].val.d.ar == NULL || pvals[i].val.d.ar->nmemb == 0) {
                if (emitDiagnostics) LogError(0, RS_RET_INVALID_PARAMS, "imtcp: allowedSender array must not be empty");
                ABORT_FINALIZE(RS_RET_INVALID_PARAMS);
            }
            moduleConfig->bAllowedSendersSet = 1;
            for (int j = 0; j < pvals[i].val.d.ar->nmemb; ++j) {
                uchar *sender = (uchar *)es_str2cstr(pvals[i].val.d.ar->arr[j], NULL);
                CHKmalloc(sender);
                const rsRetVal entryRet = net.addAllowedSenderEntry(&moduleConfig->pAllowedSendersRoot,
                                                                    &moduleConfig->pAllowedSendersLast, sender);
                free(sender);
                CHKiRet(entryRet);
            }
        } else if (!strcmp(modpblk.descr[i].name, "preservecase")) {
            moduleConfig->bPreserveCase = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "compression.mode")) {
            CHKiRet(parseCompressionMode(pvals[i].val.d.estr, &moduleConfig->compressionMode, emitDiagnostics));
        } else if (!strcmp(modpblk.descr[i].name, "compression.driver")) {
            CHKiRet(parseCompressionDriver(pvals[i].val.d.estr, &moduleConfig->compressionDriver, emitDiagnostics));
        } else if (!strcmp(modpblk.descr[i].name, "compression.maxexpansionratio")) {
            moduleConfig->compressionMaxExpansionRatio = (uint64_t)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "compression.maxdecompressedbytesperreceive")) {
            moduleConfig->compressionMaxDecompressedBytesPerReceive = (uint64_t)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "compression.maxtotalzstdwindowbytes")) {
            moduleConfig->compressionMaxTotalZstdWindowBytes = (uint64_t)pvals[i].val.d.n;
            moduleConfig->compressionMaxTotalZstdWindowBytesSet = RSTRUE;
        } else {
            dbgprintf(
                "imtcp: program error, non-handled "
                "param '%s' in beginCnfLoad\n",
                modpblk.descr[i].name);
        }
    }

    /* remove all of our legacy handlers, as they can not used in addition
     * the the new-style config method.
     */
    moduleConfig->configSetViaV2Method = 1;
    CHKiRet(applySecureDefaultsToModuleConfig(moduleConfig, emitDiagnostics));
    if (emitDiagnostics)
        warnIfInsecureListenerConfigured(moduleConfig->iStrmDrvrMode, getEffectiveModuleStreamDriver(moduleConfig),
                                         moduleConfig->pszStrmDrvrAuthMode);

finalize_it:
    RETiRet;
}


BEGINsetModCnf
    struct cnfparamvals *pvals = NULL;
    CODESTARTsetModCnf;
    pvals = nvlstGetParams(lst, &modpblk, NULL);
    if (pvals == NULL) {
        LogError(0, RS_RET_MISSING_CNFPARAMS,
                 "imtcp: error processing module "
                 "config parameters [module(...)]");
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }

    if (Debug) {
        dbgprintf("module (global) param blk for imtcp:\n");
        cnfparamsPrint(&modpblk, pvals);
    }

    CHKiRet(applyModuleParams(loadModConf, pvals, RSTRUE));
    /* New-style module configuration disables the legacy handlers only after
     * the explicit generation has accepted the complete parameter block. */
    bLegacyCnfModGlobalsPermitted = 0;

finalize_it:
    if (pvals != NULL) cnfparamvalsDestruct(pvals, &modpblk);
ENDsetModCnf


static int nvlstNameEquals(const es_str_t *const name, const char *const expected) {
    const size_t expectedLength = strlen(expected);
    return name != NULL && es_strlen((es_str_t *)name) == expectedLength &&
           strncasecmp((const char *)es_getBufAddr((es_str_t *)name), expected, expectedLength) == 0;
}

static const struct nvlst *findSourceParam(const struct nvlst *list, const char *const name) {
    for (; list != NULL; list = list->next) {
        if (nvlstNameEquals(list->name, name)) return list;
    }
    return NULL;
}

static int sourceStringEquals(const struct nvlst *const param, const char *const expected) {
    const size_t expectedLength = strlen(expected);
    return param != NULL && param->val.datatype == 'S' && param->val.d.estr != NULL &&
           es_strlen(param->val.d.estr) == expectedLength &&
           strncasecmp((const char *)es_getBufAddr(param->val.d.estr), expected, expectedLength) == 0;
}

static int sourceModuleIsImtcp(const struct cnfobj *const object) {
    const struct nvlst *const load = findSourceParam(object->nvlst, "load");
    const char *path;
    size_t length;
    size_t baseOffset = 0;
    size_t i;

    if (load == NULL || load->val.datatype != 'S' || load->val.d.estr == NULL) return 0;
    path = (const char *)es_getBufAddr(load->val.d.estr);
    length = es_strlen(load->val.d.estr);
    for (i = 0; i < length; ++i) {
        if (path[i] == '/') baseOffset = i + 1;
    }
    length -= baseOffset;
    return (length == sizeof("imtcp") - 1 && !strncasecmp(path + baseOffset, "imtcp", length)) ||
           (length == sizeof("imtcp.so") - 1 && !strncasecmp(path + baseOffset, "imtcp.so", length));
}

static int sourceInputIsImtcp(const struct cnfobj *const object) {
    return sourceStringEquals(findSourceParam(object->nvlst, "type"), "imtcp");
}

static int sourceParamKnown(const es_str_t *const name,
                            const struct cnfparamblk *const block,
                            const char *const genericName) {
    int i;
    if (nvlstNameEquals(name, genericName) || nvlstNameEquals(name, "config.enabled")) return 1;
    for (i = 0; i < block->nParams; ++i) {
        if (nvlstNameEquals(name, block->descr[i].name)) return 1;
    }
    return 0;
}

static rsRetVal validateSourceParams(const struct nvlst *list,
                                     const struct cnfparamblk *const block,
                                     const char *const genericName) {
    for (; list != NULL; list = list->next) {
        if (!sourceParamKnown(list->name, block, genericName)) return RS_RET_NOT_IMPLEMENTED;
    }
    return RS_RET_OK;
}

typedef struct imtcpReloadBuildContext_s {
    modConfData_t *config;
    int moduleSeen;
} imtcpReloadBuildContext_t;

static int reloadStringEqual(const char *const left, const char *const right) {
    return left == right || (left != NULL && right != NULL && !strcmp(left, right));
}

static int reloadUStringEqual(const uchar *const left, const uchar *const right) {
    return reloadStringEqual((const char *)left, (const char *)right);
}

static int reloadRulesetNameEqual(const uchar *const left, const uchar *const right) {
    return left == right || (left != NULL && right != NULL && !strcasecmp((const char *)left, (const char *)right));
}

static int reloadPermittedPeersEqual(const permittedPeers_t *left, const permittedPeers_t *right) {
    while (left != NULL && right != NULL) {
        if (!reloadUStringEqual(left->pszID, right->pszID)) return 0;
        left = left->pNext;
        right = right->pNext;
    }
    return left == NULL && right == NULL;
}

static int reloadSockaddrEqual(const struct sockaddr *const left, const struct sockaddr *const right) {
    if (left == right) return 1;
    if (left == NULL || right == NULL || left->sa_family != right->sa_family) return 0;
    if (left->sa_family == AF_INET) {
        const struct sockaddr_in *const left4 = (const struct sockaddr_in *)(const void *)left;
        const struct sockaddr_in *const right4 = (const struct sockaddr_in *)(const void *)right;
        return left4->sin_addr.s_addr == right4->sin_addr.s_addr;
    }
    if (left->sa_family == AF_INET6) {
        const struct sockaddr_in6 *const left6 = (const struct sockaddr_in6 *)(const void *)left;
        const struct sockaddr_in6 *const right6 = (const struct sockaddr_in6 *)(const void *)right;
        return left6->sin6_scope_id == right6->sin6_scope_id &&
               !memcmp(&left6->sin6_addr, &right6->sin6_addr, sizeof(left6->sin6_addr));
    }
    return SALEN((struct sockaddr *)left) == SALEN((struct sockaddr *)right) &&
           !memcmp(left, right, SALEN((struct sockaddr *)left));
}

static int reloadAllowedSendersEqual(const struct AllowedSenders *left, const struct AllowedSenders *right) {
    while (left != NULL && right != NULL) {
        if (left->SignificantBits != right->SignificantBits || left->allowedSender.flags != right->allowedSender.flags)
            return 0;
        if (F_ISSET(left->allowedSender.flags, ADDR_NAME)) {
            if (!reloadStringEqual(left->allowedSender.addr.HostWildcard, right->allowedSender.addr.HostWildcard))
                return 0;
        } else if (!reloadSockaddrEqual(left->allowedSender.addr.NetAddr, right->allowedSender.addr.NetAddr)) {
            return 0;
        }
        left = left->pNext;
        right = right->pNext;
    }
    return left == NULL && right == NULL;
}

static const uchar *reloadEffectiveString(const uchar *const inputValue, const uchar *const moduleValue) {
    return inputValue == NULL ? moduleValue : inputValue;
}

static const char *reloadEffectiveNamespace(const instanceConf_t *const inst, const modConfData_t *const module) {
    return inst->pszNetworkNamespace == NULL ? module->pszNetworkNamespace : inst->pszNetworkNamespace;
}

static const permittedPeers_t *reloadEffectivePermittedPeers(const instanceConf_t *const inst,
                                                             const modConfData_t *const module) {
    return inst->pPermPeersRoot == NULL ? module->pPermPeersRoot : inst->pPermPeersRoot;
}

static const struct AllowedSenders *reloadEffectiveAllowedSenders(const instanceConf_t *const inst,
                                                                  const modConfData_t *const module,
                                                                  int *const usesLegacy) {
    if (inst->bAllowedSendersSet) {
        *usesLegacy = 0;
        return inst->pAllowedSendersRoot;
    }
    if (module->bAllowedSendersSet) {
        *usesLegacy = 0;
        return module->pAllowedSendersRoot;
    }
    *usesLegacy = 1;
    return NULL;
}

static int reloadInstanceEqual(const instanceConf_t *const left,
                               const modConfData_t *const leftModule,
                               const instanceConf_t *const right,
                               const modConfData_t *const rightModule) {
    int leftLegacyAcl;
    int rightLegacyAcl;
    const struct AllowedSenders *const leftAllowed = reloadEffectiveAllowedSenders(left, leftModule, &leftLegacyAcl);
    const struct AllowedSenders *const rightAllowed =
        reloadEffectiveAllowedSenders(right, rightModule, &rightLegacyAcl);
    const tcpLstnParams_t *const leftParams = left->cnf_params;
    const tcpLstnParams_t *const rightParams = right->cnf_params;

    if (leftParams == NULL || rightParams == NULL) return 0;
    return left->iTCPSessMax == right->iTCPSessMax && left->iTCPLstnMax == right->iTCPLstnMax &&
           left->numWrkr == right->numWrkr && reloadRulesetNameEqual(left->pszBindRuleset, right->pszBindRuleset) &&
           reloadUStringEqual(left->pszInputName == NULL ? UCHAR_CONSTANT("imtcp") : left->pszInputName,
                              right->pszInputName == NULL ? UCHAR_CONSTANT("imtcp") : right->pszInputName) &&
           reloadUStringEqual(left->dfltTZ == NULL ? UCHAR_CONSTANT("") : left->dfltTZ,
                              right->dfltTZ == NULL ? UCHAR_CONSTANT("") : right->dfltTZ) &&
           left->bSPFramingFix == right->bSPFramingFix && left->ratelimitInterval == right->ratelimitInterval &&
           left->ratelimitBurst == right->ratelimitBurst && left->iAddtlFrameDelim == right->iAddtlFrameDelim &&
           left->maxFrameSize == right->maxFrameSize && left->bUseFlowControl == right->bUseFlowControl &&
           left->bDisableLFDelim == right->bDisableLFDelim && left->discardTruncatedMsg == right->discardTruncatedMsg &&
           left->bEmitMsgOnClose == right->bEmitMsgOnClose && left->bEmitMsgOnOpen == right->bEmitMsgOnOpen &&
           left->bPreserveCase == right->bPreserveCase && left->iSynBacklog == right->iSynBacklog &&
           reloadStringEqual(reloadEffectiveNamespace(left, leftModule),
                             reloadEffectiveNamespace(right, rightModule)) &&
           reloadUStringEqual(reloadEffectiveString(left->pszStrmDrvrName, leftModule->pszStrmDrvrName),
                              reloadEffectiveString(right->pszStrmDrvrName, rightModule->pszStrmDrvrName)) &&
           left->iStrmDrvrMode == right->iStrmDrvrMode &&
           reloadUStringEqual(reloadEffectiveString(left->pszStrmDrvrAuthMode, leftModule->pszStrmDrvrAuthMode),
                              reloadEffectiveString(right->pszStrmDrvrAuthMode, rightModule->pszStrmDrvrAuthMode)) &&
           reloadUStringEqual(
               reloadEffectiveString(left->pszStrmDrvrPermitExpiredCerts, leftModule->pszStrmDrvrPermitExpiredCerts),
               reloadEffectiveString(right->pszStrmDrvrPermitExpiredCerts,
                                     rightModule->pszStrmDrvrPermitExpiredCerts)) &&
           reloadUStringEqual(reloadEffectiveString(left->pszStrmDrvrCAFile, leftModule->pszStrmDrvrCAFile),
                              reloadEffectiveString(right->pszStrmDrvrCAFile, rightModule->pszStrmDrvrCAFile)) &&
           reloadUStringEqual(reloadEffectiveString(left->pszStrmDrvrCRLFile, leftModule->pszStrmDrvrCRLFile),
                              reloadEffectiveString(right->pszStrmDrvrCRLFile, rightModule->pszStrmDrvrCRLFile)) &&
           reloadUStringEqual(reloadEffectiveString(left->pszStrmDrvrKeyFile, leftModule->pszStrmDrvrKeyFile),
                              reloadEffectiveString(right->pszStrmDrvrKeyFile, rightModule->pszStrmDrvrKeyFile)) &&
           reloadUStringEqual(reloadEffectiveString(left->pszStrmDrvrCertFile, leftModule->pszStrmDrvrCertFile),
                              reloadEffectiveString(right->pszStrmDrvrCertFile, rightModule->pszStrmDrvrCertFile)) &&
           reloadUStringEqual(reloadEffectiveString(left->gnutlsPriorityString, leftModule->gnutlsPriorityString),
                              reloadEffectiveString(right->gnutlsPriorityString, rightModule->gnutlsPriorityString)) &&
           left->iStrmDrvrExtendedCertCheck == right->iStrmDrvrExtendedCertCheck &&
           left->iStrmDrvrSANPreference == right->iStrmDrvrSANPreference &&
           left->iStrmTlsVerifyDepth == right->iStrmTlsVerifyDepth &&
           left->iStrmTlsRevocationCheck == right->iStrmTlsRevocationCheck && left->bKeepAlive == right->bKeepAlive &&
           left->iKeepAliveIntvl == right->iKeepAliveIntvl && left->iKeepAliveProbes == right->iKeepAliveProbes &&
           left->iKeepAliveTime == right->iKeepAliveTime && left->starvationMaxReads == right->starvationMaxReads &&
           left->compressionMode == right->compressionMode && left->compressionDriver == right->compressionDriver &&
           left->compressionMaxExpansionRatio == right->compressionMaxExpansionRatio &&
           left->compressionMaxDecompressedBytesPerReceive == right->compressionMaxDecompressedBytesPerReceive &&
           left->compressionMaxTotalZstdWindowBytes == right->compressionMaxTotalZstdWindowBytes &&
           left->compressionMaxTotalZstdWindowBytesSet == right->compressionMaxTotalZstdWindowBytesSet &&
           reloadUStringEqual(leftParams->pszPort, rightParams->pszPort) &&
           reloadUStringEqual(leftParams->pszAddr, rightParams->pszAddr) &&
           leftParams->bSuppOctetFram == rightParams->bSuppOctetFram &&
           reloadUStringEqual(leftParams->pszLstnPortFileName, rightParams->pszLstnPortFileName) &&
           leftParams->bMultiLine == rightParams->bMultiLine &&
           reloadUStringEqual(leftParams->pszStartRegex, rightParams->pszStartRegex) &&
           reloadUStringEqual(leftParams->pszRatelimitName, rightParams->pszRatelimitName) &&
           leftLegacyAcl == rightLegacyAcl && reloadAllowedSendersEqual(leftAllowed, rightAllowed) &&
           reloadPermittedPeersEqual(reloadEffectivePermittedPeers(left, leftModule),
                                     reloadEffectivePermittedPeers(right, rightModule));
}

static rsRetVal classifyReloadSourceCandidateV1(const void *const pOldCnf,
                                                const void *const pNewCnf,
                                                eModReloadCapability_t *const pCapability) {
    const modConfData_t *const oldConfig = pOldCnf;
    const modConfData_t *const newConfig = pNewCnf;
    const instanceConf_t *oldInst;
    const instanceConf_t *newInst;

    if (oldConfig == NULL || newConfig == NULL || pCapability == NULL) return RS_RET_PARAM_ERROR;
    *pCapability = eMOD_RELOAD_RESTART_REQUIRED;
    if (!reloadStringEqual(oldConfig->reloadModuleLoadName, newConfig->reloadModuleLoadName)) return RS_RET_OK;
    oldInst = oldConfig->root;
    newInst = newConfig->root;
    while (oldInst != NULL && newInst != NULL) {
        if (!reloadInstanceEqual(oldInst, oldConfig, newInst, newConfig)) return RS_RET_OK;
        oldInst = oldInst->next;
        newInst = newInst->next;
    }
    if (oldInst == NULL && newInst == NULL) *pCapability = eMOD_RELOAD_REUSE;
    return RS_RET_OK;
}

static rsRetVal lowerReloadSourceObject(const struct cnfobj *const object,
                                        const size_t __attribute__((unused)) parseOrdinal,
                                        void *const context) {
    imtcpReloadBuildContext_t *const build = context;
    struct cnfparamvals *pvals = NULL;
    struct nvlst *paramsClone = NULL;
    instanceConf_t *inst = NULL;
    DEFiRet;

    if (object->objType == CNFOBJ_MODULE && sourceModuleIsImtcp(object)) {
        const struct nvlst *const load = findSourceParam(object->nvlst, "load");
        if (build->moduleSeen) ABORT_FINALIZE(RS_RET_MODULE_ALREADY_IN_CONF);
        if (findSourceParam(object->nvlst, "allowedsender") != NULL) ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
        CHKiRet(validateSourceParams(object->nvlst, &modpblk, "load"));
        CHKmalloc(build->config->reloadModuleLoadName = es_str2cstr(load->val.d.estr, NULL));
        CHKiRet(nvlstCloneReloadSafe(object->nvlst, &paramsClone));
        pvals = nvlstGetParams(paramsClone, &modpblk, NULL);
        if (pvals == NULL) ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
        CHKiRet(applyModuleParams(build->config, pvals, RSFALSE));
        build->moduleSeen = 1;
        FINALIZE;
    }
    if (object->objType != CNFOBJ_INPUT || !sourceInputIsImtcp(object)) FINALIZE;
    if (!build->moduleSeen) ABORT_FINALIZE(RS_RET_NOT_FOUND);
    if (findSourceParam(object->nvlst, "allowedsender") != NULL) ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
    CHKiRet(validateSourceParams(object->nvlst, &inppblk, "type"));
    CHKiRet(nvlstCloneReloadSafe(object->nvlst, &paramsClone));
    pvals = nvlstGetParams(paramsClone, &inppblk, NULL);
    if (pvals == NULL) ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
    CHKiRet(createDetachedInstance(build->config, &inst));
    if (build->config->tail == NULL)
        build->config->root = build->config->tail = inst;
    else {
        build->config->tail->next = inst;
        build->config->tail = inst;
    }
    inst = NULL; /* the detached candidate configuration now owns it */
    CHKiRet(applyInputParams(build->config, build->config->tail, pvals, RSFALSE));

finalize_it:
    if (pvals != NULL) cnfparamvalsDestruct(pvals, object->objType == CNFOBJ_MODULE ? &modpblk : &inppblk);
    nvlstDestruct(paramsClone);
    if (inst != NULL) {
        free(inst->cnf_params);
        free(inst);
    }
    RETiRet;
}

static rsRetVal buildReloadSourceCandidateV1(const modReloadSourceBuildContextV1_t *const context,
                                             void **const pCandidateCnf) {
    modConfData_t *candidate = NULL;
    imtcpReloadBuildContext_t build;
    instanceConf_t *inst;
    DEFiRet;

    if (context == NULL || context->version != MOD_RELOAD_SOURCE_BUILD_CONTEXT_V1 ||
        context->structSize < sizeof(*context) || (context->flags & MOD_RELOAD_SOURCE_BASE_UNCHANGED) == 0 ||
        context->sourceCatalog == NULL || context->activeBase == NULL || pCandidateCnf == NULL ||
        *pCandidateCnf != NULL)
        return RS_RET_PARAM_ERROR;
    CHKmalloc(candidate = calloc(1, sizeof(*candidate)));
    candidate->pConf = (rsconf_t *)context->activeBase;
    initModuleDefaults(candidate);
    build.config = candidate;
    build.moduleSeen = 0;
    CHKiRet(rsReloadCandidateVisitObjectsV1(context->sourceCatalog, lowerReloadSourceObject, &build));
    if (!build.moduleSeen) ABORT_FINALIZE(RS_RET_NOT_FOUND);
    for (inst = candidate->root; inst != NULL; inst = inst->next) {
        if (inst->cnf_params->bSuppOctetFram == FRAMING_UNSET)
            inst->cnf_params->bSuppOctetFram = candidate->bSuppOctetFram;
        if (inst->cnf_params->pszLstnPortFileName == NULL &&
            reloadUStringEqual(inst->cnf_params->pszPort, UCHAR_CONSTANT("0"))) {
            uchar *normalizedPort = (uchar *)strdup("514");
            CHKmalloc(normalizedPort);
            free((void *)inst->cnf_params->pszPort);
            inst->cnf_params->pszPort = normalizedPort;
        }
    }
    *pCandidateCnf = candidate;
    candidate = NULL;

finalize_it:
    if (candidate != NULL) {
        destructModuleConfigContents(candidate);
        free(candidate);
    }
    RETiRet;
}

static void destructReloadSourceCandidateV1(void **const pCandidateCnf) {
    modConfData_t *candidate;
    if (pCandidateCnf == NULL || *pCandidateCnf == NULL) return;
    candidate = *pCandidateCnf;
    *pCandidateCnf = NULL;
    destructModuleConfigContents(candidate);
    free(candidate);
}

static rsRetVal getReloadSourceInterfaceV1(modReloadSourceInterfaceV1_t *const interface) {
    const size_t minimumSize =
        offsetof(modReloadSourceInterfaceV1_t, destructCandidate) + sizeof(interface->destructCandidate);
    const size_t classifierSize =
        offsetof(modReloadSourceInterfaceV1_t, classifyCandidate) + sizeof(interface->classifyCandidate);
    if (interface == NULL || interface->version != eMOD_RELOAD_SOURCE_INTERFACE_V1 ||
        interface->structSize < minimumSize)
        return RS_RET_MISSING_INTERFACE;
    interface->buildCandidate = buildReloadSourceCandidateV1;
    interface->destructCandidate = destructReloadSourceCandidateV1;
    if (interface->structSize >= classifierSize) interface->classifyCandidate = classifyReloadSourceCandidateV1;
    return RS_RET_OK;
}


BEGINendCnfLoad
    CODESTARTendCnfLoad;
    if (!loadModConf->configSetViaV2Method) {
        iRet = validateLegacySessionLimits();
        if (iRet != RS_RET_OK) {
            free(cs.pszStrmDrvrAuthMode);
            cs.pszStrmDrvrAuthMode = NULL;
            loadModConf = NULL;
            return iRet;
        }
        /* persist module-specific settings from legacy config system */
        pModConf->iTCPSessMax = cs.iTCPSessMax;
        pModConf->iTCPLstnMax = cs.iTCPLstnMax;
        pModConf->iStrmDrvrMode = cs.iStrmDrvrMode;
        pModConf->bStrmDrvrModeSet = cs.bStrmDrvrModeSet;
        pModConf->bEmitMsgOnClose = cs.bEmitMsgOnClose;
        pModConf->bSuppOctetFram = cs.bSuppOctetFram;
        pModConf->iAddtlFrameDelim = cs.iAddtlFrameDelim;
        pModConf->maxFrameSize = cs.maxFrameSize;
        pModConf->bDisableLFDelim = cs.bDisableLFDelim;
        pModConf->bUseFlowControl = cs.bUseFlowControl;
        pModConf->bKeepAlive = cs.bKeepAlive;
        pModConf->iKeepAliveProbes = cs.iKeepAliveProbes;
        pModConf->iKeepAliveIntvl = cs.iKeepAliveIntvl;
        pModConf->iKeepAliveTime = cs.iKeepAliveTime;
        pModConf->compressionMode = cs.compressionMode;
        pModConf->compressionDriver = cs.compressionDriver;
        pModConf->compressionMaxTotalZstdWindowBytes = cs.compressionMaxTotalZstdWindowBytes;
        pModConf->compressionMaxTotalZstdWindowBytesSet = cs.compressionMaxTotalZstdWindowBytesSet;
        if (pPermPeersRoot != NULL) {
            assert(pModConf->pPermPeersRoot == NULL);
            pModConf->pPermPeersRoot = pPermPeersRoot;
            pPermPeersRoot = NULL; /* memory handed over! */
        }
        if ((cs.pszStrmDrvrAuthMode == NULL) || (cs.pszStrmDrvrAuthMode[0] == '\0')) {
            loadModConf->pszStrmDrvrAuthMode = NULL;
        } else {
            loadModConf->pszStrmDrvrAuthMode = cs.pszStrmDrvrAuthMode;
            cs.pszStrmDrvrAuthMode = NULL;
        }
        pModConf->bPreserveCase = cs.bPreserveCase;
        iRet = applySecureDefaultsToModuleConfig(pModConf, RSTRUE);
        if (iRet != RS_RET_OK) {
            free(cs.pszStrmDrvrAuthMode);
            cs.pszStrmDrvrAuthMode = NULL;
            loadModConf = NULL;
            return iRet;
        }
        warnIfInsecureListenerConfigured(pModConf->iStrmDrvrMode, getEffectiveModuleStreamDriver(pModConf),
                                         pModConf->pszStrmDrvrAuthMode);
    }
    free(cs.pszStrmDrvrAuthMode);
    cs.pszStrmDrvrAuthMode = NULL;

    loadModConf = NULL; /* done loading */
ENDendCnfLoad


/* function to generate error message if framework does not find requested ruleset */
static inline void std_checkRuleset_genErrMsg(__attribute__((unused)) modConfData_t *modConf, instanceConf_t *inst) {
    LogError(0, NO_ERRCODE,
             "imtcp: ruleset '%s' for port %s not found - "
             "using default ruleset instead",
             inst->pszBindRuleset, inst->cnf_params->pszPort);
}

BEGINcheckCnf
    instanceConf_t *inst;
    CODESTARTcheckCnf;
    for (inst = pModConf->root; inst != NULL; inst = inst->next) {
        std_checkRuleset(pModConf, inst);
        if (inst->cnf_params->bSuppOctetFram == FRAMING_UNSET)
            inst->cnf_params->bSuppOctetFram = pModConf->bSuppOctetFram;
    }
    if (pModConf->root == NULL) {
        LogError(0, RS_RET_NO_LISTNERS,
                 "imtcp: module loaded, but "
                 "no listeners defined - no input will be gathered");
        iRet = RS_RET_NO_LISTNERS;
    }
ENDcheckCnf


BEGINactivateCnfPrePrivDrop
    instanceConf_t *inst;
    CODESTARTactivateCnfPrePrivDrop;
    runModConf = pModConf;
    for (inst = runModConf->root; inst != NULL; inst = inst->next) {
        const rsRetVal localRet = addListner(runModConf, inst);
        if (runConf->globals.bRequireAllInputs && localRet != RS_RET_OK) ABORT_FINALIZE(localRet);
    }
    if (endpoint_registry.head == NULL) ABORT_FINALIZE(RS_RET_NO_RUN);
    tcpsrv_etry_t *etry = endpoint_registry.head;
    while (etry != NULL) {
        CHKiRet(tcpsrv.ConstructFinalize(etry->tcpsrv));
        etry = etry->next;
    }
finalize_it:
ENDactivateCnfPrePrivDrop


BEGINactivateCnf
    CODESTARTactivateCnf;
    /* sorry, nothing to do here... */
ENDactivateCnf


static void destructModuleConfigContents(modConfData_t *const pModConf) {
    instanceConf_t *inst, *del;
    if (pModConf == NULL) return;
    free(pModConf->gnutlsPriorityString);
    free(pModConf->reloadModuleLoadName);
    free(pModConf->pszNetworkNamespace);
    free(pModConf->pszStrmDrvrName);
    free(pModConf->pszStrmDrvrAuthMode);
    free(pModConf->pszStrmDrvrPermitExpiredCerts);
    free(pModConf->pszStrmDrvrCAFile);
    free(pModConf->pszStrmDrvrCRLFile);
    free(pModConf->pszStrmDrvrKeyFile);
    free(pModConf->pszStrmDrvrCertFile);
    if (pModConf->pPermPeersRoot != NULL) {
        net.DestructPermittedPeers(&pModConf->pPermPeersRoot);
    }
    if (pModConf->pAllowedSendersRoot != NULL) {
        net.DestructAllowedSenders(&pModConf->pAllowedSendersRoot);
    }

    for (inst = pModConf->root; inst != NULL;) {
        free((void *)inst->pszBindRuleset);
        free((void *)inst->pszStrmDrvrAuthMode);
        free((void *)inst->pszNetworkNamespace);
        free((void *)inst->pszStrmDrvrName);
        free((void *)inst->pszStrmDrvrPermitExpiredCerts);
        free((void *)inst->pszStrmDrvrCAFile);
        free((void *)inst->pszStrmDrvrCRLFile);
        free((void *)inst->pszStrmDrvrKeyFile);
        free((void *)inst->pszStrmDrvrCertFile);
        free((void *)inst->gnutlsPriorityString);
        if (inst->cnf_params != NULL) {
            if (inst->cnf_params->pInputName != NULL) {
                prop.Destruct(&inst->cnf_params->pInputName);
            }
            free((void *)inst->cnf_params->pszPort);
            free((void *)inst->cnf_params->pszAddr);
            free((void *)inst->cnf_params->pszLstnPortFileName);
            free((void *)inst->cnf_params->pszNetworkNamespace);
            free((void *)inst->cnf_params->pszStrmDrvrName);
            free((void *)inst->cnf_params->pszInputName);
            free((void *)inst->cnf_params->pszRatelimitName);
            free((void *)inst->cnf_params);
        }
        free((void *)inst->pszInputName);
        free((void *)inst->dfltTZ);
        if (inst->pPermPeersRoot != NULL) {
            net.DestructPermittedPeers(&inst->pPermPeersRoot);
        }
        if (inst->pAllowedSendersRoot != NULL) {
            net.DestructAllowedSenders(&inst->pAllowedSendersRoot);
        }
        del = inst;
        inst = inst->next;
        free(del);
    }
}


BEGINfreeCnf
    CODESTARTfreeCnf;
    destructModuleConfigContents(pModConf);
ENDfreeCnf

static void *RunServerThread(void *myself) {
    tcpsrv_etry_t *const etry = (tcpsrv_etry_t *)myself;
    rsRetVal iRet;
    iRet = tcpsrv.Run(etry->tcpsrv);
    if (iRet != RS_RET_OK) {
        LogError(0, iRet, "imtcp: error while terminating server; rsyslog may hang on shutdown");
    }
    return NULL;
}


/* support for running multiple servers on multiple threads (one server per thread) */
static void startSrvWrkr(tcpsrv_etry_t *const etry) {
    int r;
    pthread_attr_t sessThrdAttr;

    /* We need to temporarily block all signals because the new thread
     * inherits our signal mask. There is a race if we do not block them
     * now, and we have seen in practice that this race causes grief.
     * So we 1. save the current set, 2. block evertyhing, 3. start
     * threads, and 4 reset the current set to saved state.
     * rgerhards, 2019-08-16
     */
    sigset_t sigSet, sigSetSave;
    sigfillset(&sigSet);
    /* enable signals we still need */
    sigdelset(&sigSet, SIGTTIN);
    sigdelset(&sigSet, SIGSEGV);
    pthread_sigmask(SIG_SETMASK, &sigSet, &sigSetSave);

    pthread_attr_init(&sessThrdAttr);
    pthread_attr_setstacksize(&sessThrdAttr, 4096 * 1024);
    r = pthread_create(&etry->tid, &sessThrdAttr, RunServerThread, etry);
    if (r != 0) {
        LogError(r, NO_ERRCODE, "imtcp error creating server thread");
        /* we do NOT abort, as other servers may run - after all, we logged an error */
        etry->thread_started = 0;
    } else {
        etry->thread_started = 1;
    }
    pthread_attr_destroy(&sessThrdAttr);
    pthread_sigmask(SIG_SETMASK, &sigSetSave, NULL);
}

/* stop server worker thread
 */
static void stopSrvWrkr(tcpsrv_etry_t *const etry) {
    if (!etry->thread_started) {
        return;
    }

    DBGPRINTF("Wait for thread shutdown etry %p\n", etry);
    pthread_kill(etry->tid, SIGTTIN);
    pthread_join(etry->tid, NULL);
    etry->thread_started = 0;
    DBGPRINTF("input %p terminated\n", etry);
}

/* This function is called to gather input.
 */
BEGINrunInput
    CODESTARTrunInput;
    tcpsrv_etry_t *etry = endpoint_registry.head->next;
    while (etry != NULL) {
        startSrvWrkr(etry);
        etry = etry->next;
    }

    iRet = tcpsrv.Run(endpoint_registry.head->tcpsrv);

    /* de-init remaining servers */
    etry = endpoint_registry.head->next;
    while (etry != NULL) {
        stopSrvWrkr(etry);
        etry = etry->next;
    }
ENDrunInput


/* initialize and return if will run or not */
BEGINwillRun
    CODESTARTwillRun;
    net.PrintAllowedSenders(2); /* TCP */
ENDwillRun


BEGINafterRun
    CODESTARTafterRun;
    tcpsrv_etry_t *etry = endpoint_registry.head;
    tcpsrv_etry_t *del;
    while (etry != NULL) {
        iRet = tcpsrv.Destruct(&etry->tcpsrv);
        del = etry;
        etry = etry->next;
        endpointRegistryRemove(del);
    }
    endpoint_registry.head = NULL;
    endpoint_registry.count = 0;
    net.clearAllowedSenders(UCHAR_CONSTANT("TCP"));
ENDafterRun


BEGINisCompatibleWithFeature
    CODESTARTisCompatibleWithFeature;
    if (eFeat == sFEATURENonCancelInputTermination) iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINmodExit
    CODESTARTmodExit;
    /* release objects we used */
    objRelease(net, LM_NET_FILENAME);
    objRelease(netstrm, LM_NETSTRMS_FILENAME);
    objRelease(tcps_sess, LM_TCPSRV_FILENAME);
    objRelease(tcpsrv, LM_TCPSRV_FILENAME);
    objRelease(ruleset, CORE_COMPONENT);
ENDmodExit


static rsRetVal resetConfigVariables(uchar __attribute__((unused)) * pp, void __attribute__((unused)) * pVal) {
    cs.iTCPSessMax = 200;
    cs.iTCPLstnMax = 20;
    cs.bSuppOctetFram = 1;
    cs.iStrmDrvrMode = 0;
    cs.bStrmDrvrModeSet = 0;
    cs.bUseFlowControl = 1;
    cs.bKeepAlive = 0;
    cs.iKeepAliveProbes = 0;
    cs.iKeepAliveTime = 0;
    cs.iKeepAliveIntvl = 0;
    cs.bEmitMsgOnClose = 0;
    cs.iAddtlFrameDelim = TCPSRV_NO_ADDTL_DELIMITER;
    cs.maxFrameSize = 200000;
    cs.bDisableLFDelim = 0;
    cs.bPreserveCase = 1;
    cs.compressionMode = TCPSRV_COMPRESS_NEVER;
    cs.compressionDriver = TCPSRV_COMPRESS_DRIVER_ZLIB;
    cs.compressionMaxExpansionRatio = TCPSRV_COMPRESS_MAX_EXPANSION_RATIO_DEFAULT;
    cs.compressionMaxDecompressedBytesPerReceive = TCPSRV_COMPRESS_MAX_DECOMPRESSED_BYTES_PER_RECEIVE_DEFAULT;
    cs.compressionMaxTotalZstdWindowBytes = TCPSRV_COMPRESS_MAX_TOTAL_ZSTD_WINDOW_BYTES_DEFAULT;
    cs.compressionMaxTotalZstdWindowBytesSet = RSFALSE;
    free(cs.pszStrmDrvrAuthMode);
    cs.pszStrmDrvrAuthMode = NULL;
    free(cs.pszInputName);
    cs.pszInputName = NULL;
    free(cs.pszBindRuleset);
    cs.pszBindRuleset = NULL;
    free(cs.lstnPortFile);
    cs.lstnPortFile = NULL;
    return RS_RET_OK;
}


BEGINqueryEtryPt
    CODESTARTqueryEtryPt;
    CODEqueryEtryPt_STD_IMOD_QUERIES;
    CODEqueryEtryPt_STD_CONF2_QUERIES;
    CODEqueryEtryPt_STD_CONF2_setModCnf_QUERIES;
    CODEqueryEtryPt_STD_CONF2_PREPRIVDROP_QUERIES;
    CODEqueryEtryPt_STD_CONF2_IMOD_QUERIES;
    CODEqueryEtryPt_RELOAD_SOURCE_V1_QUERIES;
    CODEqueryEtryPt_IsCompatibleWithFeature_IF_OMOD_QUERIES;
ENDqueryEtryPt


BEGINmodInit()
    CODESTARTmodInit;
    *ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
    CODEmodInit_QueryRegCFSLineHdlr endpoint_registry.head = NULL;
    endpoint_registry.count = 0;
    /* request objects we use */
    CHKiRet(objUse(net, LM_NET_FILENAME));
    CHKiRet(objUse(netstrm, LM_NETSTRMS_FILENAME));
    CHKiRet(objUse(tcps_sess, LM_TCPSRV_FILENAME));
    CHKiRet(objUse(tcpsrv, LM_TCPSRV_FILENAME));
    CHKiRet(objUse(ruleset, CORE_COMPONENT));
    CHKiRet(objUse(prop, CORE_COMPONENT));

    /* register config file handlers */
    CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("inputtcpserverrun"), 0, eCmdHdlrGetWord, addInstance, NULL,
                               STD_LOADABLE_MODULE_ID));
    CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("inputtcpserverinputname"), 0, eCmdHdlrGetWord, NULL, &cs.pszInputName,
                               STD_LOADABLE_MODULE_ID));
    CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("inputtcpserverbindruleset"), 0, eCmdHdlrGetWord, NULL,
                               &cs.pszBindRuleset, STD_LOADABLE_MODULE_ID));
    /* module-global config params - will be disabled in configs that are loaded
     * via module(...).
     */
    CHKiRet(regCfSysLineHdlr2(UCHAR_CONSTANT("inputtcpserverstreamdriverpermittedpeer"), 0, eCmdHdlrGetWord,
                              setPermittedPeer, NULL, STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
    CHKiRet(regCfSysLineHdlr2(UCHAR_CONSTANT("inputtcpserverstreamdriverauthmode"), 0, eCmdHdlrGetWord, NULL,
                              &cs.pszStrmDrvrAuthMode, STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
    CHKiRet(regCfSysLineHdlr2(UCHAR_CONSTANT("inputtcpserverkeepalive"), 0, eCmdHdlrBinary, NULL, &cs.bKeepAlive,
                              STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
    CHKiRet(regCfSysLineHdlr2(UCHAR_CONSTANT("inputtcpserverkeepalive_probes"), 0, eCmdHdlrInt, NULL,
                              &cs.iKeepAliveProbes, STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
    CHKiRet(regCfSysLineHdlr2(UCHAR_CONSTANT("inputtcpserverkeepalive_intvl"), 0, eCmdHdlrInt, NULL,
                              &cs.iKeepAliveIntvl, STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
    CHKiRet(regCfSysLineHdlr2(UCHAR_CONSTANT("inputtcpserverkeepalive_time"), 0, eCmdHdlrInt, NULL, &cs.iKeepAliveTime,
                              STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
    CHKiRet(regCfSysLineHdlr2(UCHAR_CONSTANT("inputtcpflowcontrol"), 0, eCmdHdlrBinary, NULL, &cs.bUseFlowControl,
                              STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
    CHKiRet(regCfSysLineHdlr2(UCHAR_CONSTANT("inputtcpserverdisablelfdelimiter"), 0, eCmdHdlrBinary, NULL,
                              &cs.bDisableLFDelim, STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
    CHKiRet(regCfSysLineHdlr2(UCHAR_CONSTANT("inputtcpserveraddtlframedelimiter"), 0, eCmdHdlrInt, NULL,
                              &cs.iAddtlFrameDelim, STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
    CHKiRet(regCfSysLineHdlr2(UCHAR_CONSTANT("inputtcpserversupportoctetcountedframing"), 0, eCmdHdlrBinary, NULL,
                              &cs.bSuppOctetFram, STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
    CHKiRet(regCfSysLineHdlr2(UCHAR_CONSTANT("inputtcpmaxsessions"), 0, eCmdHdlrInt, NULL, &cs.iTCPSessMax,
                              STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
    CHKiRet(regCfSysLineHdlr2(UCHAR_CONSTANT("inputtcpmaxlisteners"), 0, eCmdHdlrInt, NULL, &cs.iTCPLstnMax,
                              STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
    CHKiRet(regCfSysLineHdlr2(UCHAR_CONSTANT("inputtcpservernotifyonconnectionclose"), 0, eCmdHdlrBinary, NULL,
                              &cs.bEmitMsgOnClose, STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
    CHKiRet(regCfSysLineHdlr2(UCHAR_CONSTANT("inputtcpserverstreamdrivermode"), 0, eCmdHdlrInt, setLegacyStrmDrvrMode,
                              NULL, STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
    CHKiRet(regCfSysLineHdlr2(UCHAR_CONSTANT("inputtcpserverpreservecase"), 1, eCmdHdlrBinary, NULL, &cs.bPreserveCase,
                              STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
    CHKiRet(regCfSysLineHdlr2(UCHAR_CONSTANT("inputtcpserverlistenportfile"), 1, eCmdHdlrGetWord, NULL,
                              &cs.lstnPortFile, STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
    CHKiRet(regCfSysLineHdlr2(UCHAR_CONSTANT("inputtcpservercompressionmode"), 0, eCmdHdlrGetWord,
                              setLegacyCompressionMode, NULL, STD_LOADABLE_MODULE_ID, &bLegacyCnfModGlobalsPermitted));
    CHKiRet(regCfSysLineHdlr2(UCHAR_CONSTANT("inputtcpservercompressiondriver"), 0, eCmdHdlrGetWord,
                              setLegacyCompressionDriver, NULL, STD_LOADABLE_MODULE_ID,
                              &bLegacyCnfModGlobalsPermitted));
    CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("resetconfigvariables"), 1, eCmdHdlrCustomHandler, resetConfigVariables,
                               NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit
