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
#include <limits.h>
#include <arpa/inet.h>
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
#include "srUtils.h"
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
    DEFobjCurrIf(prop) DEFobjCurrIf(glbl)

        static rsRetVal resetConfigVariables(uchar __attribute__((unused)) * pp, void __attribute__((unused)) * pVal);

/* Module static data */
typedef struct tcpsrv_etry_s {
    tcpsrv_t *tcpsrv;
    char *endpoint_key; /* canonical runtime socket identity, never a config name */
    char *config_name; /* operator-facing inputname, retained separately for future diagnostics */
    size_t source_ordinal; /* stable startup order for unkeyable dynamic endpoints */
    enum { IMTCP_ENDPOINT_ACTIVE, IMTCP_ENDPOINT_NO_ACCEPT, IMTCP_ENDPOINT_RETIRING } state;
    pthread_t tid; /* the worker's thread ID */
    int thread_started;
    pthread_mutex_t startMut;
    pthread_cond_t startCond;
    int startSyncInitialized;
    int startAuthorized;
    int startCancelled;
    int runFinished;
    struct tcpsrv_etry_s *next;
} tcpsrv_etry_t;

/* Runtime-owned endpoint registry. Reload preparation builds entries privately;
 * commit publishes additions or disables accepts for removals while established
 * sessions retain their tcpsrv generation until retirement. Registry access is
 * confined to the serialized input/control lifecycle, never the message path. */
static struct {
    tcpsrv_etry_t *head;
    int count;
} endpoint_registry = {0};

static permittedPeers_t *pPermPeersRoot = NULL;

static rsRetVal endpointKeyBuild(const tcpLstnParams_t *const params,
                                 const char *const networkNamespace,
                                 char **const key) {
    const char *const address = params->pszAddr == NULL ? "*" : (const char *)params->pszAddr;
    const char *identityAddress = address;
    const char *const port = params->pszPort == NULL ? "" : (const char *)params->pszPort;
    const char *const ns = networkNamespace == NULL ? "" : networkNamespace;
    char canonicalAddress[INET6_ADDRSTRLEN];
    struct in_addr ipv4;
    struct in6_addr ipv6;
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
    if (inet_pton(AF_INET, address, &ipv4) == 1) {
        if (inet_ntop(AF_INET, &ipv4, canonicalAddress, sizeof(canonicalAddress)) == NULL) return RS_RET_ERR;
        identityAddress = canonicalAddress;
    } else if (inet_pton(AF_INET6, address, &ipv6) == 1) {
        if (inet_ntop(AF_INET6, &ipv6, canonicalAddress, sizeof(canonicalAddress)) == NULL) return RS_RET_ERR;
        identityAddress = canonicalAddress;
    }
    printed = snprintf(NULL, 0, "tcp|ns:%zu:%s|addr:%zu:%s|port:%u", strlen(ns), ns, strlen(identityAddress),
                       identityAddress, (unsigned)numericPort);
    if (printed < 0) return RS_RET_PARAM_ERROR;
    required = (size_t)printed;
    if ((*key = malloc(required + 1)) == NULL) return RS_RET_OUT_OF_MEMORY;
    snprintf(*key, required + 1, "tcp|ns:%zu:%s|addr:%zu:%s|port:%u", strlen(ns), ns, strlen(identityAddress),
             identityAddress, (unsigned)numericPort);
    return RS_RET_OK;
}

static rsRetVal endpointRegistryBuild(tcpsrv_t *const server,
                                      const tcpLstnParams_t *const params,
                                      const char *const networkNamespace,
                                      const uchar *const configName,
                                      const size_t sourceOrdinal,
                                      tcpsrv_etry_t **const result) {
    tcpsrv_etry_t *entry = NULL;
    const tcpsrv_etry_t *existing;
    DEFiRet;

    if (server == NULL || params == NULL || result == NULL || *result != NULL) return RS_RET_PARAM_ERROR;
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
    if (pthread_mutex_init(&entry->startMut, NULL) != 0) ABORT_FINALIZE(RS_RET_ERR);
    if (pthread_cond_init(&entry->startCond, NULL) != 0) {
        pthread_mutex_destroy(&entry->startMut);
        ABORT_FINALIZE(RS_RET_ERR);
    }
    entry->startSyncInitialized = 1;
    entry->tcpsrv = server;
    entry->source_ordinal = sourceOrdinal;
    entry->state = IMTCP_ENDPOINT_ACTIVE;
    *result = entry;
    entry = NULL;

finalize_it:
    if (entry != NULL) {
        free(entry->endpoint_key);
        free(entry->config_name);
        free(entry);
    }
    RETiRet;
}

static void endpointRegistryPublish(tcpsrv_etry_t *const entry) {
    entry->next = endpoint_registry.head;
    endpoint_registry.head = entry;
    ++endpoint_registry.count;
}

static void endpointRegistryUnlink(tcpsrv_etry_t *const entry) {
    if (entry == NULL) return;
    tcpsrv_etry_t **cursor = &endpoint_registry.head;
    while (*cursor != NULL && *cursor != entry) cursor = &(*cursor)->next;
    if (*cursor == entry) {
        *cursor = entry->next;
        entry->next = NULL;
        --endpoint_registry.count;
    }
}

static rsRetVal endpointRegistryAdd(tcpsrv_t *const server,
                                    const tcpLstnParams_t *const params,
                                    const char *const networkNamespace,
                                    const uchar *const configName) {
    tcpsrv_etry_t *entry = NULL;
    const rsRetVal ret =
        endpointRegistryBuild(server, params, networkNamespace, configName, (size_t)endpoint_registry.count, &entry);
    if (ret == RS_RET_OK) endpointRegistryPublish(entry);
    return ret;
}

static void endpointRegistryRemove(tcpsrv_etry_t *const entry) {
    entry->state = IMTCP_ENDPOINT_RETIRING;
    if (entry->startSyncInitialized) {
        pthread_cond_destroy(&entry->startCond);
        pthread_mutex_destroy(&entry->startMut);
    }
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
    char **reloadAllowedSenderSpecs;
    size_t reloadAllowedSenderSpecCount;
    sbool reloadAllowedSendersMaterialized;
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

typedef struct reloadRateProfile_s {
    char *name;
    char *fingerprint;
    unsigned interval;
    unsigned burst;
    int severity;
    sbool simple;
    struct reloadRateProfile_s *next;
} reloadRateProfile_t;

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
    char **reloadAllowedSenderSpecs;
    size_t reloadAllowedSenderSpecCount;
    sbool reloadAllowedSendersMaterialized;
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
    sbool reloadSnapshot;
    rsconf_t *reloadRateConfig;
    sbool reloadRateConfigInitialized;
    reloadRateProfile_t *reloadRateProfiles;
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
    config->reloadAllowedSendersMaterialized = RSTRUE;
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
    inst->reloadAllowedSendersMaterialized = RSTRUE;
}

static void freeReloadAllowedSenderSpecs(char ***const specs, size_t *const count) {
    if (specs == NULL || count == NULL) return;
    if (*specs != NULL) {
        for (size_t i = 0; i < *count; ++i) free((*specs)[i]);
        free(*specs);
    }
    *specs = NULL;
    *count = 0;
}

static rsRetVal captureReloadAllowedSenderSpecs(const struct cnfarray *const values,
                                                rsconf_t *const config,
                                                char ***const specs,
                                                size_t *const count,
                                                int *const allReloadSafe) {
    char **captured = NULL;
    DEFiRet;

    if (values == NULL || config == NULL || specs == NULL || count == NULL || allReloadSafe == NULL || *specs != NULL ||
        *count != 0)
        return RS_RET_PARAM_ERROR;
    if (values->nmemb < 0 || (size_t)values->nmemb > SIZE_MAX / sizeof(captured[0])) return RS_RET_OUT_OF_MEMORY;
    CHKmalloc(captured = calloc((size_t)values->nmemb, sizeof(captured[0])));
    *allReloadSafe = 1;
    for (int i = 0; i < values->nmemb; ++i) {
        int reloadSafe;
        CHKmalloc(captured[i] = es_str2cstr(values->arr[i], NULL));
        *count = (size_t)i + 1;
        CHKiRet(net.allowedSenderEntryIsReloadSafe(config, (uchar *)captured[i], &reloadSafe));
        if (!reloadSafe) *allReloadSafe = 0;
    }
    *specs = captured;
    captured = NULL;

finalize_it:
    if (captured != NULL) {
        for (size_t i = 0; i < *count; ++i) free(captured[i]);
        free(captured);
        *count = 0;
    }
    RETiRet;
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


static void freeRuntimeListenerParams(tcpLstnParams_t *const params) {
    if (params == NULL) return;
    if (params->bOwnAllowedSenderRoot) net.DestructAllowedSenders(&params->pAllowedSenderRoot);
    free((void *)params->pszPort);
    free((void *)params->pszAddr);
    free((void *)params->pszLstnPortFileName);
    free((void *)params->pszNetworkNamespace);
    free((void *)params->pszStrmDrvrName);
    free((void *)params->pszInputName);
    free((void *)params->pszStartRegex);
    free((void *)params->pszRatelimitName);
    free(params);
}

static rsRetVal cloneRuntimeListenerParams(const tcpLstnParams_t *const source, tcpLstnParams_t **const destination) {
    tcpLstnParams_t *clone = NULL;
    struct AllowedSenders *allowedLast = NULL;
    DEFiRet;

    if (source == NULL || destination == NULL || *destination != NULL) return RS_RET_PARAM_ERROR;
    CHKmalloc(clone = calloc(1, sizeof(*clone)));
    clone->bSuppOctetFram = source->bSuppOctetFram;
    clone->bSPFramingFix = source->bSPFramingFix;
    clone->bPreserveCase = source->bPreserveCase;
    clone->bMultiLine = source->bMultiLine;
    clone->pRuleset = source->pRuleset;
    clone->bUseLegacyAllowedSender = source->bUseLegacyAllowedSender;
    if (source->pAllowedSenderRoot != NULL) {
        CHKiRet(net.cloneAllowedSenders(source->pAllowedSenderRoot, &clone->pAllowedSenderRoot, &allowedLast));
        clone->bOwnAllowedSenderRoot = 1;
    }
    memcpy(clone->dfltTZ, source->dfltTZ, sizeof(clone->dfltTZ));
    if (source->pszPort != NULL) CHKmalloc(clone->pszPort = ustrdup(source->pszPort));
    if (source->pszAddr != NULL) CHKmalloc(clone->pszAddr = ustrdup(source->pszAddr));
    if (source->pszLstnPortFileName != NULL)
        CHKmalloc(clone->pszLstnPortFileName = ustrdup(source->pszLstnPortFileName));
    if (source->pszStartRegex != NULL) CHKmalloc(clone->pszStartRegex = ustrdup(source->pszStartRegex));
    if (source->pszRatelimitName != NULL) CHKmalloc(clone->pszRatelimitName = ustrdup(source->pszRatelimitName));
    *destination = clone;
    clone = NULL;

finalize_it:
    freeRuntimeListenerParams(clone);
    RETiRet;
}

static rsRetVal prepareListenerServer(modConfData_t *const modConf,
                                      instanceConf_t *const inst,
                                      tcpsrv_t **const preparedServer,
                                      tcpLstnParams_t **const preparedParams) {
    DEFiRet;
    uchar *psz; /* work variable */
    char *ns; /**< network namespace */
    permittedPeers_t *peers;
    tcpLstnParams_t *listenerParams = NULL;
    tcpLstnParams_t *configuredParams;

    tcpsrv_t *pOurTcpsrv = NULL;
    if (preparedServer == NULL || *preparedServer != NULL || preparedParams == NULL || *preparedParams != NULL)
        return RS_RET_PARAM_ERROR;
    CHKiRet(cloneRuntimeListenerParams(inst->cnf_params, &listenerParams));
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
    listenerParams->pRuleset = inst->pBindRuleset;

    ns = (inst->pszNetworkNamespace == NULL) ? modConf->pszNetworkNamespace : inst->pszNetworkNamespace;
    CHKiRet(tcpsrv.SetNetworkNamespace(pOurTcpsrv, listenerParams, ns));

    CHKiRet(tcpsrv.SetInputName(pOurTcpsrv, listenerParams,
                                inst->pszInputName == NULL ? UCHAR_CONSTANT("imtcp") : inst->pszInputName));
    CHKiRet(tcpsrv.SetOrigin(pOurTcpsrv, (uchar *)"imtcp"));
    CHKiRet(tcpsrv.SetDfltTZ(pOurTcpsrv, (inst->dfltTZ == NULL) ? (uchar *)"" : inst->dfltTZ));
    CHKiRet(tcpsrv.SetbSPFramingFix(pOurTcpsrv, inst->bSPFramingFix));
    CHKiRet(tcpsrv.SetLinuxLikeRatelimiters(pOurTcpsrv, inst->ratelimitInterval, inst->ratelimitBurst));

    if ((ustrcmp(listenerParams->pszPort, UCHAR_CONSTANT("0")) == 0 && listenerParams->pszLstnPortFileName == NULL) ||
        ustrcmp(listenerParams->pszPort, UCHAR_CONSTANT("0")) < 0) {
        uchar *newPort = NULL;
        LogMsg(0, RS_RET_OK, LOG_WARNING, "imtcp: port 0 and no port file set -> using port 514 instead");
        CHKmalloc(newPort = (uchar *)strdup("514"));
        free((void *)listenerParams->pszPort);
        listenerParams->pszPort = newPort;
    }
    if (inst->bAllowedSendersSet) {
        if (listenerParams->bOwnAllowedSenderRoot) net.DestructAllowedSenders(&listenerParams->pAllowedSenderRoot);
        listenerParams->bOwnAllowedSenderRoot = 0;
        struct AllowedSenders *allowedLast = NULL;
        CHKiRet(net.cloneAllowedSenders(inst->pAllowedSendersRoot, &listenerParams->pAllowedSenderRoot, &allowedLast));
        listenerParams->bOwnAllowedSenderRoot = 1;
        listenerParams->bUseLegacyAllowedSender = 0;
    } else if (modConf->bAllowedSendersSet) {
        if (listenerParams->bOwnAllowedSenderRoot) net.DestructAllowedSenders(&listenerParams->pAllowedSenderRoot);
        listenerParams->bOwnAllowedSenderRoot = 0;
        struct AllowedSenders *allowedLast = NULL;
        CHKiRet(
            net.cloneAllowedSenders(modConf->pAllowedSendersRoot, &listenerParams->pAllowedSenderRoot, &allowedLast));
        listenerParams->bOwnAllowedSenderRoot = 1;
        listenerParams->bUseLegacyAllowedSender = 0;
    } else {
        if (listenerParams->bOwnAllowedSenderRoot) net.DestructAllowedSenders(&listenerParams->pAllowedSenderRoot);
        listenerParams->pAllowedSenderRoot = NULL;
        listenerParams->bOwnAllowedSenderRoot = 0;
        listenerParams->bUseLegacyAllowedSender = 1;
    }
    CHKiRet(tcpsrv.SetUsrP(pOurTcpsrv, listenerParams));
    configuredParams = listenerParams;
    iRet = tcpsrv.configureTCPListen(pOurTcpsrv, configuredParams);
    listenerParams = NULL; /* configureTCPListen consumes the clone on every path */
    CHKiRet(iRet);

    *preparedServer = pOurTcpsrv;
    *preparedParams = configuredParams; /* borrowed from the prepared server */
    pOurTcpsrv = NULL;

finalize_it:
    if (iRet != RS_RET_OK) {
        LogError(0, NO_ERRCODE, "imtcp: error %d trying to add listener", iRet);
        if (pOurTcpsrv != NULL) {
            tcpsrv.SetUsrP(pOurTcpsrv, NULL);
            tcpsrv.Destruct(&pOurTcpsrv);
        }
    }
    freeRuntimeListenerParams(listenerParams);
    RETiRet;
}

static rsRetVal addListner(modConfData_t *const modConf, instanceConf_t *const inst) {
    tcpsrv_t *server = NULL;
    tcpLstnParams_t *params = NULL;
    DEFiRet;

    CHKiRet(prepareListenerServer(modConf, inst, &server, &params));
    CHKiRet(endpointRegistryAdd(server, params, params->pszNetworkNamespace,
                                inst->pszInputName == NULL ? UCHAR_CONSTANT("imtcp") : inst->pszInputName));
    server = NULL; /* endpoint registry owns the configured runtime server */

finalize_it:
    if (server != NULL) {
        tcpsrv.SetUsrP(server, NULL);
        tcpsrv.Destruct(&server);
    }
    RETiRet;
}


static rsRetVal applyInputEndpointParam(const char *const name,
                                        const struct cnfparamvals *const value,
                                        instanceConf_t *const inst,
                                        int *const handled) {
    DEFiRet;

    *handled = 1;
    if (!strcmp(name, "port")) {
        CHKmalloc(inst->cnf_params->pszPort = (uchar *)es_str2cstr(value->val.d.estr, NULL));
    } else if (!strcmp(name, "networknamespace")) {
        CHKmalloc(inst->pszNetworkNamespace = es_str2cstr(value->val.d.estr, NULL));
    } else if (!strcmp(name, "address")) {
        CHKmalloc(inst->cnf_params->pszAddr = (uchar *)es_str2cstr(value->val.d.estr, NULL));
    } else if (!strcmp(name, "name")) {
        CHKmalloc(inst->pszInputName = (uchar *)es_str2cstr(value->val.d.estr, NULL));
    } else if (!strcmp(name, "defaulttz")) {
        CHKmalloc(inst->dfltTZ = (uchar *)es_str2cstr(value->val.d.estr, NULL));
    } else if (!strcmp(name, "ruleset")) {
        CHKmalloc(inst->pszBindRuleset = (uchar *)es_str2cstr(value->val.d.estr, NULL));
    } else if (!strcmp(name, "maxsessions")) {
        inst->iTCPSessMax = (int)value->val.d.n;
    } else if (!strcmp(name, "maxlisteners")) {
        inst->iTCPLstnMax = (int)value->val.d.n;
    } else if (!strcmp(name, "workerthreads")) {
        inst->numWrkr = (int)value->val.d.n;
    } else if (!strcmp(name, "starvationprotection.maxreads")) {
        inst->starvationMaxReads = (unsigned)value->val.d.n;
    } else if (!strcmp(name, "socketbacklog")) {
        inst->iSynBacklog = (int)value->val.d.n;
    } else if (!strcmp(name, "listenportfilename")) {
        CHKmalloc(inst->cnf_params->pszLstnPortFileName = (uchar *)es_str2cstr(value->val.d.estr, NULL));
    } else {
        *handled = 0;
    }

finalize_it:
    RETiRet;
}

static rsRetVal applyInputStreamStringParam(const char *const name,
                                            const struct cnfparamvals *const value,
                                            instanceConf_t *const inst,
                                            int *const handled) {
    DEFiRet;

    *handled = 1;
    if (!strcmp(name, "streamdriver.authmode")) {
        CHKmalloc(inst->pszStrmDrvrAuthMode = (uchar *)es_str2cstr(value->val.d.estr, NULL));
    } else if (!strcmp(name, "streamdriver.permitexpiredcerts")) {
        CHKmalloc(inst->pszStrmDrvrPermitExpiredCerts = (uchar *)es_str2cstr(value->val.d.estr, NULL));
    } else if (!strcmp(name, "streamdriver.cafile")) {
        CHKmalloc(inst->pszStrmDrvrCAFile = (uchar *)es_str2cstr(value->val.d.estr, NULL));
    } else if (!strcmp(name, "streamdriver.crlfile")) {
        CHKmalloc(inst->pszStrmDrvrCRLFile = (uchar *)es_str2cstr(value->val.d.estr, NULL));
    } else if (!strcmp(name, "streamdriver.keyfile")) {
        CHKmalloc(inst->pszStrmDrvrKeyFile = (uchar *)es_str2cstr(value->val.d.estr, NULL));
    } else if (!strcmp(name, "streamdriver.certfile")) {
        CHKmalloc(inst->pszStrmDrvrCertFile = (uchar *)es_str2cstr(value->val.d.estr, NULL));
    } else if (!strcmp(name, "streamdriver.name")) {
        CHKmalloc(inst->pszStrmDrvrName = (uchar *)es_str2cstr(value->val.d.estr, NULL));
    } else if (!strcmp(name, "gnutlsprioritystring")) {
        CHKmalloc(inst->gnutlsPriorityString = (uchar *)es_str2cstr(value->val.d.estr, NULL));
    } else if (!strcmp(name, "permittedpeer")) {
        for (int j = 0; j < value->val.d.ar->nmemb; ++j) {
            uchar *const peer = (uchar *)es_str2cstr(value->val.d.ar->arr[j], NULL);
            CHKmalloc(peer);
            const rsRetVal entryRet = net.AddPermittedPeer(&inst->pPermPeersRoot, peer);
            free(peer);
            CHKiRet(entryRet);
        }
    } else {
        *handled = 0;
    }

finalize_it:
    RETiRet;
}

static rsRetVal applyInputStreamScalarParam(const char *const name,
                                            const struct cnfparamvals *const value,
                                            instanceConf_t *const inst,
                                            const sbool emitDiagnostics,
                                            int *const handled) {
    DEFiRet;

    *handled = 1;
    if (!strcmp(name, "streamdriver.mode")) {
        inst->iStrmDrvrMode = (int)value->val.d.n;
        inst->bStrmDrvrModeSet = 1;
    } else if (!strcmp(name, "streamdriver.CheckExtendedKeyPurpose")) {
        inst->iStrmDrvrExtendedCertCheck = (int)value->val.d.n;
    } else if (!strcmp(name, "streamdriver.PrioritizeSAN")) {
        inst->iStrmDrvrSANPreference = (int)value->val.d.n;
    } else if (!strcmp(name, "streamdriver.TlsVerifyDepth")) {
        if (value->val.d.n >= 2) {
            inst->iStrmTlsVerifyDepth = (int)value->val.d.n;
        } else if (emitDiagnostics) {
            parser_errmsg("streamdriver.TlsVerifyDepth must be 2 or higher but is %d", (int)value->val.d.n);
        } else {
            ABORT_FINALIZE(RS_RET_INVALID_PARAMS);
        }
    } else if (!strcmp(name, "streamdriver.TlsRevocationCheck")) {
        inst->iStrmTlsRevocationCheck = (int)value->val.d.n;
    } else if (!strcmp(name, "keepalive")) {
        inst->bKeepAlive = (int)value->val.d.n;
    } else if (!strcmp(name, "keepalive.probes")) {
        inst->iKeepAliveProbes = (int)value->val.d.n;
    } else if (!strcmp(name, "keepalive.time")) {
        inst->iKeepAliveTime = (int)value->val.d.n;
    } else if (!strcmp(name, "keepalive.interval")) {
        inst->iKeepAliveIntvl = (int)value->val.d.n;
    } else {
        *handled = 0;
    }

finalize_it:
    RETiRet;
}

static rsRetVal applyInputMessageParam(const char *const name,
                                       const struct cnfparamvals *const value,
                                       instanceConf_t *const inst,
                                       const sbool emitDiagnostics,
                                       int *const handled) {
    DEFiRet;

    *handled = 1;
    if (!strcmp(name, "framingfix.cisco.asa")) {
        inst->bSPFramingFix = (int)value->val.d.n;
    } else if (!strcmp(name, "flowcontrol")) {
        inst->bUseFlowControl = (int)value->val.d.n;
    } else if (!strcmp(name, "disablelfdelimiter")) {
        inst->bDisableLFDelim = (int)value->val.d.n;
    } else if (!strcmp(name, "discardtruncatedmsg")) {
        inst->discardTruncatedMsg = (int)value->val.d.n;
    } else if (!strcmp(name, "notifyonconnectionclose")) {
        inst->bEmitMsgOnClose = (int)value->val.d.n;
    } else if (!strcmp(name, "notifyonconnectionopen")) {
        inst->bEmitMsgOnOpen = (int)value->val.d.n;
    } else if (!strcmp(name, "addtlframedelimiter")) {
        inst->iAddtlFrameDelim = (int)value->val.d.n;
    } else if (!strcmp(name, "maxframesize")) {
        const int max = (int)value->val.d.n;
        CHKiRet(validateMaxFrameSize(max, emitDiagnostics));
        inst->maxFrameSize = max;
    } else if (!strcmp(name, "supportoctetcountedframing")) {
        inst->cnf_params->bSuppOctetFram = (int)value->val.d.n;
    } else if (!strcmp(name, "preservecase")) {
        inst->bPreserveCase = (int)value->val.d.n;
    } else if (!strcmp(name, "multiline")) {
        inst->cnf_params->bMultiLine = (int)value->val.d.n;
    } else if (!strcmp(name, "framing.delimiter.regex")) {
        CHKmalloc(inst->cnf_params->pszStartRegex = (uchar *)es_str2cstr(value->val.d.estr, NULL));
    } else {
        *handled = 0;
    }

finalize_it:
    RETiRet;
}

static rsRetVal applyInputRateCompressionParam(const char *const name,
                                               const struct cnfparamvals *const value,
                                               const modConfData_t *const moduleConfig,
                                               instanceConf_t *const inst,
                                               const sbool emitDiagnostics,
                                               int *const handled) {
    DEFiRet;

    *handled = 1;
    if (!strcmp(name, "allowedsender")) {
        int allReloadSafe;
        if (value->val.d.ar == NULL || value->val.d.ar->nmemb == 0) {
            if (emitDiagnostics) LogError(0, RS_RET_INVALID_PARAMS, "imtcp: allowedSender array must not be empty");
            ABORT_FINALIZE(RS_RET_INVALID_PARAMS);
        }
        inst->bAllowedSendersSet = 1;
        CHKiRet(captureReloadAllowedSenderSpecs(value->val.d.ar, moduleConfig->pConf, &inst->reloadAllowedSenderSpecs,
                                                &inst->reloadAllowedSenderSpecCount, &allReloadSafe));
        inst->reloadAllowedSendersMaterialized = !moduleConfig->reloadSnapshot || allReloadSafe;
        if (!inst->reloadAllowedSendersMaterialized) FINALIZE;
        for (int j = 0; j < value->val.d.ar->nmemb; ++j) {
            uchar *sender = (uchar *)es_str2cstr(value->val.d.ar->arr[j], NULL);
            CHKmalloc(sender);
            const rsRetVal entryRet = net.addAllowedSenderEntryForConfig(
                moduleConfig->pConf, &inst->pAllowedSendersRoot, &inst->pAllowedSendersLast, sender);
            free(sender);
            CHKiRet(entryRet);
        }
    } else if (!strcmp(name, "ratelimit.burst")) {
        inst->ratelimitBurst = (int)value->val.d.n;
    } else if (!strcmp(name, "ratelimit.interval")) {
        inst->ratelimitInterval = (int)value->val.d.n;
    } else if (!strcmp(name, "ratelimit.name")) {
        CHKmalloc(inst->cnf_params->pszRatelimitName = (uchar *)es_str2cstr(value->val.d.estr, NULL));
    } else if (!strcmp(name, "compression.mode")) {
        CHKiRet(parseCompressionMode(value->val.d.estr, &inst->compressionMode, emitDiagnostics));
    } else if (!strcmp(name, "compression.driver")) {
        CHKiRet(parseCompressionDriver(value->val.d.estr, &inst->compressionDriver, emitDiagnostics));
    } else if (!strcmp(name, "compression.maxexpansionratio")) {
        inst->compressionMaxExpansionRatio = (uint64_t)value->val.d.n;
    } else if (!strcmp(name, "compression.maxdecompressedbytesperreceive")) {
        inst->compressionMaxDecompressedBytesPerReceive = (uint64_t)value->val.d.n;
    } else if (!strcmp(name, "compression.maxtotalzstdwindowbytes")) {
        inst->compressionMaxTotalZstdWindowBytes = (uint64_t)value->val.d.n;
        inst->compressionMaxTotalZstdWindowBytesSet = RSTRUE;
    } else {
        *handled = 0;
    }

finalize_it:
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
        int handled;
        if (!pvals[i].bUsed) continue;
        CHKiRet(applyInputEndpointParam(inppblk.descr[i].name, &pvals[i], inst, &handled));
        if (handled) continue;
        CHKiRet(applyInputStreamStringParam(inppblk.descr[i].name, &pvals[i], inst, &handled));
        if (handled) continue;
        CHKiRet(applyInputStreamScalarParam(inppblk.descr[i].name, &pvals[i], inst, emitDiagnostics, &handled));
        if (handled) continue;
        CHKiRet(applyInputMessageParam(inppblk.descr[i].name, &pvals[i], inst, emitDiagnostics, &handled));
        if (handled) continue;
        CHKiRet(applyInputRateCompressionParam(inppblk.descr[i].name, &pvals[i], moduleConfig, inst, emitDiagnostics,
                                               &handled));
        if (!handled) dbgprintf("imtcp: program error, non-handled param '%s'\n", inppblk.descr[i].name);
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
            int allReloadSafe;
            if (pvals[i].val.d.ar == NULL || pvals[i].val.d.ar->nmemb == 0) {
                if (emitDiagnostics) LogError(0, RS_RET_INVALID_PARAMS, "imtcp: allowedSender array must not be empty");
                ABORT_FINALIZE(RS_RET_INVALID_PARAMS);
            }
            moduleConfig->bAllowedSendersSet = 1;
            CHKiRet(captureReloadAllowedSenderSpecs(pvals[i].val.d.ar, moduleConfig->pConf,
                                                    &moduleConfig->reloadAllowedSenderSpecs,
                                                    &moduleConfig->reloadAllowedSenderSpecCount, &allReloadSafe));
            moduleConfig->reloadAllowedSendersMaterialized = !moduleConfig->reloadSnapshot || allReloadSafe;
            if (!moduleConfig->reloadAllowedSendersMaterialized) continue;
            for (int j = 0; j < pvals[i].val.d.ar->nmemb; ++j) {
                uchar *sender = (uchar *)es_str2cstr(pvals[i].val.d.ar->arr[j], NULL);
                CHKmalloc(sender);
                const rsRetVal entryRet =
                    net.addAllowedSenderEntryForConfig(moduleConfig->pConf, &moduleConfig->pAllowedSendersRoot,
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

static void reloadRateProfilesDestruct(reloadRateProfile_t **const head) {
    reloadRateProfile_t *profile;

    if (head == NULL) return;
    while ((profile = *head) != NULL) {
        *head = profile->next;
        free(profile->name);
        free(profile->fingerprint);
        free(profile);
    }
}

static const reloadRateProfile_t *reloadRateProfileFind(const modConfData_t *const config, const uchar *const name) {
    const reloadRateProfile_t *profile;

    if (config == NULL || name == NULL) return NULL;
    for (profile = config->reloadRateProfiles; profile != NULL; profile = profile->next)
        if (!strcmp(profile->name, (const char *)name)) return profile;
    return NULL;
}

static rsRetVal reloadRateNonNegativeInt(const struct nvlst *const parameter, unsigned *const value) {
    char *text = NULL;
    char *end;
    long parsed;

    if (parameter == NULL || value == NULL) return RS_RET_PARAM_ERROR;
    if (parameter->val.datatype == 'N') {
        if (parameter->val.d.n < 0 || parameter->val.d.n > INT_MAX) return RS_RET_INVALID_VALUE;
        *value = (unsigned)parameter->val.d.n;
        return RS_RET_OK;
    }
    if (parameter->val.datatype != 'S' || parameter->val.d.estr == NULL) return RS_RET_INVALID_VALUE;
    text = es_str2cstr(parameter->val.d.estr, NULL);
    if (text == NULL) return RS_RET_OUT_OF_MEMORY;
    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno != 0 || *text == '\0' || *end != '\0' || parsed < 0 || parsed > INT_MAX) {
        free(text);
        return RS_RET_INVALID_VALUE;
    }
    *value = (unsigned)parsed;
    free(text);
    return RS_RET_OK;
}

static rsRetVal lowerReloadRateProfile(const struct cnfobj *const object, modConfData_t *const config) {
    const struct nvlst *nameParameter;
    const struct nvlst *parameter;
    reloadRateProfile_t *profile = NULL;
    reloadRateProfile_t **tail;
    DEFiRet;

    if (object == NULL || object->objType != CNFOBJ_RATELIMIT || config == NULL || config->reloadRateConfig == NULL ||
        !config->reloadRateConfigInitialized)
        return RS_RET_PARAM_ERROR;
    nameParameter = findSourceParam(object->nvlst, "name");
    if (nameParameter == NULL || nameParameter->val.datatype != 'S' || nameParameter->val.d.estr == NULL)
        return RS_RET_CONF_PARSE_ERROR;
    CHKmalloc(profile = calloc(1, sizeof(*profile)));
    CHKmalloc(profile->name = es_str2cstr(nameParameter->val.d.estr, NULL));
    if (*profile->name == '\0') ABORT_FINALIZE(RS_RET_INVALID_VALUE);
    if (reloadRateProfileFind(config, (const uchar *)profile->name) != NULL) ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
    CHKiRet(rsReloadObjectSyntaxFingerprintV1(object, &profile->fingerprint));
    profile->interval = 0;
    profile->burst = 10000;
    profile->severity = -1;
    profile->simple = RSTRUE;
    for (parameter = object->nvlst; parameter != NULL; parameter = parameter->next) {
        if (nvlstNameEquals(parameter->name, "name") || nvlstNameEquals(parameter->name, "config.enabled")) continue;
        if (nvlstNameEquals(parameter->name, "interval")) {
            CHKiRet(reloadRateNonNegativeInt(parameter, &profile->interval));
        } else if (nvlstNameEquals(parameter->name, "burst")) {
            CHKiRet(reloadRateNonNegativeInt(parameter, &profile->burst));
        } else if (nvlstNameEquals(parameter->name, "severity")) {
            char *severity;
            if (parameter->val.datatype != 'S' || parameter->val.d.estr == NULL) ABORT_FINALIZE(RS_RET_INVALID_VALUE);
            CHKmalloc(severity = es_str2cstr(parameter->val.d.estr, NULL));
            profile->severity = decodeSyslogName((uchar *)severity, syslogPriNames);
            free(severity);
        } else {
            profile->simple = RSFALSE;
        }
    }
    if (profile->simple)
        CHKiRet(ratelimitAddConfig(config->reloadRateConfig, profile->name, profile->interval, profile->burst,
                                   profile->severity, NULL, RSFALSE, NULL, RSFALSE, NULL, NULL, 0, 0, RSTRUE, RSFALSE));
    tail = &config->reloadRateProfiles;
    while (*tail != NULL) tail = &(*tail)->next;
    *tail = profile;
    profile = NULL;

finalize_it:
    if (profile != NULL) {
        free(profile->name);
        free(profile->fingerprint);
        free(profile);
    }
    RETiRet;
}

static int reloadStringEqual(const char *const left, const char *const right) {
    return left == right || (left != NULL && right != NULL && !strcmp(left, right));
}

static int reloadUStringEqual(const uchar *const left, const uchar *const right) {
    return reloadStringEqual((const char *)left, (const char *)right);
}

static int reloadNamedRateProfileEqual(const instanceConf_t *const left,
                                       const modConfData_t *const leftConfig,
                                       const instanceConf_t *const right,
                                       const modConfData_t *const rightConfig) {
    const uchar *const leftName = left->cnf_params->pszRatelimitName;
    const uchar *const rightName = right->cnf_params->pszRatelimitName;
    const reloadRateProfile_t *leftProfile;
    const reloadRateProfile_t *rightProfile;

    if (!reloadUStringEqual(leftName, rightName)) return 0;
    if (leftName == NULL) return 1;
    leftProfile = reloadRateProfileFind(leftConfig, leftName);
    rightProfile = reloadRateProfileFind(rightConfig, rightName);
    if (leftProfile == NULL || rightProfile == NULL) return leftProfile == rightProfile;
    return !strcmp(leftProfile->fingerprint, rightProfile->fingerprint);
}

static int reloadNamedRateProfileChangeSupported(const instanceConf_t *const left,
                                                 const modConfData_t *const leftConfig,
                                                 const instanceConf_t *const right,
                                                 const modConfData_t *const rightConfig) {
    const uchar *const leftName = left->cnf_params->pszRatelimitName;
    const uchar *const rightName = right->cnf_params->pszRatelimitName;
    const reloadRateProfile_t *leftProfile;
    const reloadRateProfile_t *rightProfile;

    if (!reloadUStringEqual(leftName, rightName)) {
        rightProfile = reloadRateProfileFind(rightConfig, rightName);
        leftProfile = reloadRateProfileFind(leftConfig, rightName);
        return rightProfile == NULL || rightProfile->simple || leftProfile != NULL;
    }
    if (leftName == NULL) return 1;
    leftProfile = reloadRateProfileFind(leftConfig, leftName);
    rightProfile = reloadRateProfileFind(rightConfig, rightName);
    if (leftProfile == NULL || rightProfile == NULL || !leftProfile->simple || !rightProfile->simple) return 0;
    return 1;
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

static int reloadAllowedSenderSpecsEqual(char *const *const left,
                                         const size_t leftCount,
                                         char *const *const right,
                                         const size_t rightCount) {
    if (leftCount != rightCount) return 0;
    for (size_t i = 0; i < leftCount; ++i)
        if (!reloadStringEqual(left[i], right[i])) return 0;
    return 1;
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

static void reloadEffectiveAllowedSenderSpecs(const instanceConf_t *const inst,
                                              const modConfData_t *const module,
                                              char *const **const specs,
                                              size_t *const count,
                                              int *const materialized) {
    if (inst->bAllowedSendersSet) {
        *specs = inst->reloadAllowedSenderSpecs;
        *count = inst->reloadAllowedSenderSpecCount;
        *materialized = inst->reloadAllowedSendersMaterialized;
    } else if (module->bAllowedSendersSet) {
        *specs = module->reloadAllowedSenderSpecs;
        *count = module->reloadAllowedSenderSpecCount;
        *materialized = module->reloadAllowedSendersMaterialized;
    } else {
        *specs = NULL;
        *count = 0;
        *materialized = 1;
    }
}

static int reloadAllowedSenderConfigEqual(const instanceConf_t *const left,
                                          const modConfData_t *const leftModule,
                                          const instanceConf_t *const right,
                                          const modConfData_t *const rightModule) {
    char *const *leftSpecs;
    char *const *rightSpecs;
    size_t leftCount;
    size_t rightCount;
    int leftMaterialized;
    int rightMaterialized;

    reloadEffectiveAllowedSenderSpecs(left, leftModule, &leftSpecs, &leftCount, &leftMaterialized);
    reloadEffectiveAllowedSenderSpecs(right, rightModule, &rightSpecs, &rightCount, &rightMaterialized);
    (void)leftMaterialized;
    (void)rightMaterialized;
    return reloadAllowedSenderSpecsEqual(leftSpecs, leftCount, rightSpecs, rightCount);
}

static int reloadAllowedSenderChangeSupported(const instanceConf_t *const left,
                                              const modConfData_t *const leftModule,
                                              const instanceConf_t *const right,
                                              const modConfData_t *const rightModule) {
    char *const *specs;
    size_t count;
    int leftMaterialized;
    int rightMaterialized;

    reloadEffectiveAllowedSenderSpecs(left, leftModule, &specs, &count, &leftMaterialized);
    reloadEffectiveAllowedSenderSpecs(right, rightModule, &specs, &count, &rightMaterialized);
    return leftMaterialized && rightMaterialized;
}

static int reloadEndpointConfigEqual(const instanceConf_t *const left,
                                     const modConfData_t *const leftModule,
                                     const instanceConf_t *const right,
                                     const modConfData_t *const rightModule) {
    char *leftKey = NULL;
    char *rightKey = NULL;
    const rsRetVal leftRet = endpointKeyBuild(left->cnf_params, reloadEffectiveNamespace(left, leftModule), &leftKey);
    const rsRetVal rightRet =
        endpointKeyBuild(right->cnf_params, reloadEffectiveNamespace(right, rightModule), &rightKey);
    int equal = 0;

    if (leftRet == RS_RET_OK && rightRet == RS_RET_OK) {
        equal = !strcmp(leftKey, rightKey);
    } else if (leftRet == RS_RET_NOT_IMPLEMENTED && rightRet == RS_RET_NOT_IMPLEMENTED) {
        equal = reloadStringEqual(reloadEffectiveNamespace(left, leftModule),
                                  reloadEffectiveNamespace(right, rightModule)) &&
                reloadUStringEqual(left->cnf_params->pszPort, right->cnf_params->pszPort) &&
                reloadUStringEqual(left->cnf_params->pszAddr, right->cnf_params->pszAddr) &&
                reloadUStringEqual(left->cnf_params->pszLstnPortFileName, right->cnf_params->pszLstnPortFileName);
    }
    free(leftKey);
    free(rightKey);
    return equal;
}

static int reloadInstanceEqual(const instanceConf_t *const left,
                               const modConfData_t *const leftModule,
                               const instanceConf_t *const right,
                               const modConfData_t *const rightModule,
                               const int ignoreLiveFields,
                               const int ignoreNewSessionFields) {
    int leftLegacyAcl;
    int rightLegacyAcl;
    const struct AllowedSenders *const leftAllowed = reloadEffectiveAllowedSenders(left, leftModule, &leftLegacyAcl);
    const struct AllowedSenders *const rightAllowed =
        reloadEffectiveAllowedSenders(right, rightModule, &rightLegacyAcl);
    const tcpLstnParams_t *const leftParams = left->cnf_params;
    const tcpLstnParams_t *const rightParams = right->cnf_params;

    if (leftParams == NULL || rightParams == NULL) return 0;
    return (ignoreLiveFields || (left->iTCPSessMax == right->iTCPSessMax && left->iTCPLstnMax == right->iTCPLstnMax)) &&
           left->numWrkr == right->numWrkr &&
           reloadUStringEqual(left->pszInputName == NULL ? UCHAR_CONSTANT("imtcp") : left->pszInputName,
                              right->pszInputName == NULL ? UCHAR_CONSTANT("imtcp") : right->pszInputName) &&
           (leftParams->pszRatelimitName != NULL || rightParams->pszRatelimitName != NULL || ignoreLiveFields ||
            (left->ratelimitInterval == right->ratelimitInterval && left->ratelimitBurst == right->ratelimitBurst)) &&
           (ignoreNewSessionFields ||
            (left->bSPFramingFix == right->bSPFramingFix && left->iAddtlFrameDelim == right->iAddtlFrameDelim &&
             left->maxFrameSize == right->maxFrameSize && left->bDisableLFDelim == right->bDisableLFDelim &&
             left->discardTruncatedMsg == right->discardTruncatedMsg)) &&
           (ignoreLiveFields ||
            (reloadRulesetNameEqual(left->pszBindRuleset, right->pszBindRuleset) &&
             left->bUseFlowControl == right->bUseFlowControl && left->starvationMaxReads == right->starvationMaxReads &&
             left->bEmitMsgOnClose == right->bEmitMsgOnClose && left->bEmitMsgOnOpen == right->bEmitMsgOnOpen &&
             reloadUStringEqual(left->dfltTZ == NULL ? UCHAR_CONSTANT("") : left->dfltTZ,
                                right->dfltTZ == NULL ? UCHAR_CONSTANT("") : right->dfltTZ))) &&
           (ignoreNewSessionFields || left->bPreserveCase == right->bPreserveCase) &&
           left->iSynBacklog == right->iSynBacklog && reloadEndpointConfigEqual(left, leftModule, right, rightModule) &&
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
           left->iStrmTlsRevocationCheck == right->iStrmTlsRevocationCheck &&
           (ignoreNewSessionFields ||
            (left->bKeepAlive == right->bKeepAlive && left->iKeepAliveIntvl == right->iKeepAliveIntvl &&
             left->iKeepAliveProbes == right->iKeepAliveProbes && left->iKeepAliveTime == right->iKeepAliveTime)) &&
           (ignoreNewSessionFields ||
            (left->compressionMode == right->compressionMode && left->compressionDriver == right->compressionDriver &&
             left->compressionMaxExpansionRatio == right->compressionMaxExpansionRatio &&
             left->compressionMaxDecompressedBytesPerReceive == right->compressionMaxDecompressedBytesPerReceive &&
             left->compressionMaxTotalZstdWindowBytes == right->compressionMaxTotalZstdWindowBytes &&
             left->compressionMaxTotalZstdWindowBytesSet == right->compressionMaxTotalZstdWindowBytesSet)) &&
           (ignoreNewSessionFields || leftParams->bSuppOctetFram == rightParams->bSuppOctetFram) &&
           (ignoreNewSessionFields || leftParams->bMultiLine == rightParams->bMultiLine) &&
           (ignoreNewSessionFields || reloadUStringEqual(leftParams->pszStartRegex, rightParams->pszStartRegex)) &&
           (ignoreLiveFields || reloadNamedRateProfileEqual(left, leftModule, right, rightModule)) &&
           (ignoreLiveFields ||
            (leftLegacyAcl == rightLegacyAcl && reloadAllowedSenderConfigEqual(left, leftModule, right, rightModule) &&
             reloadAllowedSendersEqual(leftAllowed, rightAllowed))) &&
           reloadPermittedPeersEqual(reloadEffectivePermittedPeers(left, leftModule),
                                     reloadEffectivePermittedPeers(right, rightModule));
}

static rsRetVal reloadEndpointIdentity(const instanceConf_t *const inst,
                                       const modConfData_t *const module,
                                       char **const identity) {
    rsRetVal ret;
    char *key = NULL;
    int printed;
    size_t required;

    if (inst == NULL || inst->cnf_params == NULL || module == NULL || identity == NULL || *identity != NULL)
        return RS_RET_PARAM_ERROR;
    ret = endpointKeyBuild(inst->cnf_params, reloadEffectiveNamespace(inst, module), &key);
    if (ret == RS_RET_OK) {
        *identity = key;
        return RS_RET_OK;
    }
    if (ret != RS_RET_NOT_IMPLEMENTED || inst->pszInputName == NULL) return ret;
    printed = snprintf(NULL, 0, "config-name|%zu:%s", ustrlen(inst->pszInputName), inst->pszInputName);
    if (printed < 0) return RS_RET_PARAM_ERROR;
    required = (size_t)printed;
    if ((*identity = malloc(required + 1)) == NULL) return RS_RET_OUT_OF_MEMORY;
    snprintf(*identity, required + 1, "config-name|%zu:%s", ustrlen(inst->pszInputName), inst->pszInputName);
    return RS_RET_OK;
}

static rsRetVal reloadConfigHasUniqueIdentities(const modConfData_t *const config, int *const usable) {
    const instanceConf_t *current;
    const instanceConf_t *previous;
    char *currentIdentity = NULL;
    char *previousIdentity = NULL;
    DEFiRet;

    *usable = 0;
    for (current = config->root; current != NULL; current = current->next) {
        iRet = reloadEndpointIdentity(current, config, &currentIdentity);
        if (iRet == RS_RET_NOT_IMPLEMENTED) {
            iRet = RS_RET_OK;
            FINALIZE;
        }
        CHKiRet(iRet);
        for (previous = config->root; previous != current; previous = previous->next) {
            iRet = reloadEndpointIdentity(previous, config, &previousIdentity);
            if (iRet == RS_RET_NOT_IMPLEMENTED) {
                iRet = RS_RET_OK;
                FINALIZE;
            }
            CHKiRet(iRet);
            if (!strcmp(currentIdentity, previousIdentity)) FINALIZE;
            free(previousIdentity);
            previousIdentity = NULL;
        }
        free(currentIdentity);
        currentIdentity = NULL;
    }
    *usable = 1;

finalize_it:
    free(currentIdentity);
    free(previousIdentity);
    RETiRet;
}

static rsRetVal reloadFindInstanceByIdentity(const modConfData_t *const config,
                                             const char *const identity,
                                             const instanceConf_t **const match) {
    const instanceConf_t *inst;
    char *candidateIdentity = NULL;
    DEFiRet;

    *match = NULL;
    for (inst = config->root; inst != NULL; inst = inst->next) {
        CHKiRet(reloadEndpointIdentity(inst, config, &candidateIdentity));
        if (!strcmp(identity, candidateIdentity)) {
            *match = inst;
            free(candidateIdentity);
            candidateIdentity = NULL;
            break;
        }
        free(candidateIdentity);
        candidateIdentity = NULL;
    }

finalize_it:
    free(candidateIdentity);
    RETiRet;
}

static void mergeReloadCapability(eModReloadCapability_t *const capability,
                                  const int liveChanged,
                                  const int newSessionsChanged,
                                  const int drainReplace) {
    int haveLive;
    int haveNewSessions;

    if (drainReplace || *capability == eMOD_RELOAD_DRAIN_REPLACE) {
        *capability = eMOD_RELOAD_DRAIN_REPLACE;
        return;
    }
    haveLive = liveChanged || *capability == eMOD_RELOAD_LIVE_SWAP || *capability == eMOD_RELOAD_LIVE_AND_NEW_SESSIONS;
    haveNewSessions = newSessionsChanged || *capability == eMOD_RELOAD_NEW_SESSIONS ||
                      *capability == eMOD_RELOAD_LIVE_AND_NEW_SESSIONS;
    if (haveLive && haveNewSessions)
        *capability = eMOD_RELOAD_LIVE_AND_NEW_SESSIONS;
    else if (haveLive)
        *capability = eMOD_RELOAD_LIVE_SWAP;
    else if (haveNewSessions)
        *capability = eMOD_RELOAD_NEW_SESSIONS;
}

static int reloadEffectiveSynBacklog(const instanceConf_t *const inst) {
    return inst->iSynBacklog == 0 ? inst->iTCPSessMax / 10 + 5 : inst->iSynBacklog;
}

static int mergeReloadInstanceCapability(const instanceConf_t *const oldInst,
                                         const modConfData_t *const oldConfig,
                                         const instanceConf_t *const newInst,
                                         const modConfData_t *const newConfig,
                                         eModReloadCapability_t *const capability) {
    int liveChanged;
    int newSessionsChanged;

    if (reloadInstanceEqual(oldInst, oldConfig, newInst, newConfig, 0, 0)) return 1;
    if (!reloadNamedRateProfileChangeSupported(oldInst, oldConfig, newInst, newConfig)) return 0;
    if (!reloadAllowedSenderConfigEqual(oldInst, oldConfig, newInst, newConfig) &&
        !reloadAllowedSenderChangeSupported(oldInst, oldConfig, newInst, newConfig))
        return 0;
    /* Resizing the session table is allocation-free at commit because Prepare
     * reserves the replacement table. Shrink eligibility is checked while
     * the listener is fenced; changing cold-start backlog semantics remains
     * fail-closed here. */
    if (oldInst->iTCPSessMax != newInst->iTCPSessMax &&
        reloadEffectiveSynBacklog(oldInst) != reloadEffectiveSynBacklog(newInst))
        return 0;
    if (!reloadInstanceEqual(oldInst, oldConfig, newInst, newConfig, 1, 1)) return 0;
    liveChanged = !reloadInstanceEqual(oldInst, oldConfig, newInst, newConfig, 0, 1);
    newSessionsChanged = !reloadInstanceEqual(oldInst, oldConfig, newInst, newConfig, 1, 0);
    mergeReloadCapability(capability, liveChanged, newSessionsChanged, 0);
    return 1;
}

static rsRetVal classifyReloadSourceCandidateV1(const void *const pOldCnf,
                                                const void *const pNewCnf,
                                                eModReloadCapability_t *const pCapability) {
    const modConfData_t *const oldConfig = pOldCnf;
    const modConfData_t *const newConfig = pNewCnf;
    const instanceConf_t *oldInst;
    const instanceConf_t *newInst;
    const instanceConf_t *matchedInst;
    eModReloadCapability_t capability = eMOD_RELOAD_REUSE;
    int oldIdentitiesUsable;
    int newIdentitiesUsable;
    char *identity = NULL;
    size_t oldCount = 0;
    size_t newCount = 0;
    size_t matchedCount = 0;
    DEFiRet;

    if (oldConfig == NULL || newConfig == NULL || pCapability == NULL) return RS_RET_PARAM_ERROR;
    *pCapability = eMOD_RELOAD_RESTART_REQUIRED;
    if (!reloadStringEqual(oldConfig->reloadModuleLoadName, newConfig->reloadModuleLoadName)) return RS_RET_OK;
    for (oldInst = oldConfig->root; oldInst != NULL; oldInst = oldInst->next) ++oldCount;
    for (newInst = newConfig->root; newInst != NULL; newInst = newInst->next) ++newCount;
    /* Existing-listener-only milestone: adding/removing/replacing accept
     * endpoints must remain fail-closed until prepared workers can prove
     * their event loop is activation-ready before old accepts are closed.
     * Classify before Prepare allocates or binds any live resource; validate
     * mode therefore reports the same restart requirement without activation.
     * Keep the private lifecycle implementation for that future milestone. */
    if (oldCount != newCount) FINALIZE;
    CHKiRet(reloadConfigHasUniqueIdentities(oldConfig, &oldIdentitiesUsable));
    CHKiRet(reloadConfigHasUniqueIdentities(newConfig, &newIdentitiesUsable));
    if (oldIdentitiesUsable && newIdentitiesUsable) {
        for (oldInst = oldConfig->root; oldInst != NULL; oldInst = oldInst->next) {
            CHKiRet(reloadEndpointIdentity(oldInst, oldConfig, &identity));
            CHKiRet(reloadFindInstanceByIdentity(newConfig, identity, &matchedInst));
            free(identity);
            identity = NULL;
            if (matchedInst == NULL) FINALIZE;
            ++matchedCount;
            if (!mergeReloadInstanceCapability(oldInst, oldConfig, matchedInst, newConfig, &capability)) FINALIZE;
        }
        if (matchedCount != newCount) FINALIZE;
        *pCapability = capability;
        FINALIZE;
    }
    oldInst = oldConfig->root;
    newInst = newConfig->root;
    while (oldInst != NULL && newInst != NULL) {
        if (!mergeReloadInstanceCapability(oldInst, oldConfig, newInst, newConfig, &capability)) FINALIZE;
        oldInst = oldInst->next;
        newInst = newInst->next;
    }
    if (oldInst == NULL && newInst == NULL) *pCapability = capability;

finalize_it:
    free(identity);
    RETiRet;
}

typedef struct imtcpReloadEntryV1_s {
    tcpsrv_etry_t *runtime;
    int addition;
    int additionPublished;
    int removal;
    int flowControl;
    unsigned starvationMaxReads;
    int applyRateLimitScalars;
    unsigned ratelimitInterval;
    unsigned ratelimitBurst;
    int swapRateLimiter;
    ratelimit_t *preparedRateLimiter;
    ratelimit_t *retiredRateLimiter;
    uchar *preparedRatelimitName;
    uchar *retiredRatelimitName;
    int notifyOnConnectionClose;
    int notifyOnConnectionOpen;
    int preserveCase;
    int keepAlive;
    int keepAliveInterval;
    int keepAliveProbes;
    int keepAliveTime;
    int framingFix;
    int additionalFrameDelimiter;
    int maxFrameSize;
    int disableLFDelimiter;
    int discardTruncatedMessage;
    int supportOctetCountedFraming;
    int compressionMode;
    int compressionDriver;
    uint64_t compressionMaxExpansionRatio;
    uint64_t compressionMaxDecompressedBytesPerReceive;
    uint64_t compressionMaxTotalZstdWindowBytes;
    int multiLine;
    uchar *startRegex;
    uchar defaultTZ[8];
    ruleset_t *ruleset;
    tcps_sess_t **preparedSessionSlots;
    tcps_sess_t **retiredSessionSlots;
    int sessionMax;
    tcpsrv_listener_tables_t preparedListenerTables;
    tcpsrv_listener_tables_t retiredListenerTables;
    uint64_t fenceToken;
    int fenceAcquired;
    struct AllowedSenders *preparedAllowedSenders;
    struct AllowedSenders *retiredAllowedSenders;
    int retiredAllowedSendersOwned;
    int useLegacyAllowedSender;
    int applyAllowedSenders;
    unsigned char *sessionAllowed;
    size_t sessionAllowedCount;
} imtcpReloadEntryV1_t;

typedef struct imtcpReloadStateV1_s {
    size_t count;
    imtcpReloadEntryV1_t entries[];
} imtcpReloadStateV1_t;

static rsRetVal prepareReloadStartRegex(const uchar *const source, uchar **const prepared) {
    DEFiRet;

    if (prepared == NULL || *prepared != NULL) return RS_RET_PARAM_ERROR;
    if (source == NULL) FINALIZE;
#ifdef FEATURE_REGEXP
    regex_t validationRegex;
    CHKmalloc(*prepared = ustrdup(source));
    const int errcode = regcomp(&validationRegex, (const char *)source, REG_EXTENDED);
    if (errcode != 0) {
        char errbuff[512];
        regerror(errcode, &validationRegex, errbuff, sizeof(errbuff));
        LogError(0, RS_RET_INVALID_VALUE, "imtcp: invalid reload framing.delimiter.regex: %s", errbuff);
        ABORT_FINALIZE(RS_RET_INVALID_VALUE);
    }
    regfree(&validationRegex);
#else
    ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
#endif

finalize_it:
    if (iRet != RS_RET_OK) {
        free(*prepared);
        *prepared = NULL;
    }
    RETiRet;
}

static rsRetVal prepareReloadRateLimiter(imtcpReloadEntryV1_t *const entry,
                                         const modConfData_t *const oldConfig,
                                         const instanceConf_t *const newInst,
                                         const modConfData_t *const newConfig) {
    tcpLstnPortList_t *listener;
    const uchar *newName;
    const reloadRateProfile_t *oldProfile;
    const reloadRateProfile_t *newProfile;
    DEFiRet;

    if (entry == NULL || entry->runtime == NULL || entry->runtime->tcpsrv == NULL || oldConfig == NULL ||
        newInst == NULL || newInst->cnf_params == NULL || newConfig == NULL || newConfig->pConf == NULL)
        return RS_RET_PARAM_ERROR;
    listener = entry->runtime->tcpsrv->pLstnPorts;
    if (listener == NULL || listener->pNext != NULL || listener->cnf_params == NULL) return RS_RET_NOT_IMPLEMENTED;
    newName = newInst->cnf_params->pszRatelimitName;
    oldProfile = reloadRateProfileFind(oldConfig, listener->cnf_params->pszRatelimitName);
    newProfile = reloadRateProfileFind(newConfig, newName);
    if (reloadUStringEqual(listener->cnf_params->pszRatelimitName, newName) &&
        ((oldProfile == NULL && newProfile == NULL) ||
         (oldProfile != NULL && newProfile != NULL && !strcmp(oldProfile->fingerprint, newProfile->fingerprint))))
        FINALIZE;

    if (newName != NULL) {
        CHKmalloc(entry->preparedRatelimitName = ustrdup(newName));
        CHKiRet(ratelimitNewFromConfig(
            &entry->preparedRateLimiter,
            newProfile != NULL && newProfile->simple ? newConfig->reloadRateConfig : newConfig->pConf,
            (const char *)newName, "tcpsrv", NULL));
    } else {
        CHKiRet(ratelimitNew(&entry->preparedRateLimiter, "tcpsrv", NULL));
        ratelimitSetLinuxLike(entry->preparedRateLimiter, (unsigned)newInst->ratelimitInterval,
                              (unsigned)newInst->ratelimitBurst);
    }
    ratelimitSetThreadSafe(entry->preparedRateLimiter);
    entry->swapRateLimiter = 1;

finalize_it:
    if (iRet != RS_RET_OK) {
        if (entry->preparedRateLimiter != NULL) {
            ratelimitDestruct(entry->preparedRateLimiter);
            entry->preparedRateLimiter = NULL;
        }
        free(entry->preparedRatelimitName);
        entry->preparedRatelimitName = NULL;
    }
    RETiRet;
}

static void freeReloadPreparedValues(imtcpReloadStateV1_t *const state) {
    if (state == NULL) return;
    for (size_t i = 0; i < state->count; ++i) {
        free(state->entries[i].startRegex);
        state->entries[i].startRegex = NULL;
        free(state->entries[i].preparedSessionSlots);
        state->entries[i].preparedSessionSlots = NULL;
        free(state->entries[i].retiredSessionSlots);
        state->entries[i].retiredSessionSlots = NULL;
        free(state->entries[i].preparedListenerTables.streams);
        free(state->entries[i].preparedListenerTables.ports);
        free(state->entries[i].preparedListenerTables.descriptors);
        memset(&state->entries[i].preparedListenerTables, 0, sizeof(state->entries[i].preparedListenerTables));
        free(state->entries[i].retiredListenerTables.streams);
        free(state->entries[i].retiredListenerTables.ports);
        free(state->entries[i].retiredListenerTables.descriptors);
        memset(&state->entries[i].retiredListenerTables, 0, sizeof(state->entries[i].retiredListenerTables));
        net.DestructAllowedSenders(&state->entries[i].preparedAllowedSenders);
        if (state->entries[i].retiredAllowedSendersOwned)
            net.DestructAllowedSenders(&state->entries[i].retiredAllowedSenders);
        state->entries[i].retiredAllowedSenders = NULL;
        state->entries[i].retiredAllowedSendersOwned = 0;
        free(state->entries[i].sessionAllowed);
        state->entries[i].sessionAllowed = NULL;
        if (state->entries[i].preparedRateLimiter != NULL) ratelimitDestruct(state->entries[i].preparedRateLimiter);
        state->entries[i].preparedRateLimiter = NULL;
        if (state->entries[i].retiredRateLimiter != NULL) ratelimitDestruct(state->entries[i].retiredRateLimiter);
        state->entries[i].retiredRateLimiter = NULL;
        free(state->entries[i].preparedRatelimitName);
        state->entries[i].preparedRatelimitName = NULL;
        free(state->entries[i].retiredRatelimitName);
        state->entries[i].retiredRatelimitName = NULL;
    }
}

typedef struct imtcpAclEvaluation_s {
    struct AllowedSenders *root;
    int useLegacy;
} imtcpAclEvaluation_t;

static rsRetVal evaluateReloadAclSession(tcps_sess_t *const session, void *const context, int *const allowed) {
    imtcpAclEvaluation_t *const acl = context;
    struct sockaddr *address = NULL;
    rsRetVal ret;

    if (session == NULL || acl == NULL || allowed == NULL) return RS_RET_PARAM_ERROR;
    ret = netstrm.GetRemAddr(session->pStrm, &address);
    if (ret != RS_RET_OK) return ret;
    const char *const hostname = propGetSzStrOrDefault(session->fromHost, "(hostname unknown)");
    *allowed = acl->useLegacy ? net.isAllowedSender2(UCHAR_CONSTANT("TCP"), address, hostname, 1)
                              : net.isAllowedSenderList(acl->root, address, hostname, 1);
    return RS_RET_OK;
}

static rsRetVal startSrvWrkr(tcpsrv_etry_t *etry, int parked);
static void authorizeSrvWrkr(tcpsrv_etry_t *etry);
static void cancelParkedSrvWrkr(tcpsrv_etry_t *etry);
static void stopSrvWrkr(tcpsrv_etry_t *etry);

static void destructPreparedEndpoint(tcpsrv_etry_t **const pEntry) {
    tcpsrv_etry_t *entry;
    if (pEntry == NULL || *pEntry == NULL) return;
    entry = *pEntry;
    *pEntry = NULL;
    cancelParkedSrvWrkr(entry);
    if (entry->tcpsrv != NULL) tcpsrv.Destruct(&entry->tcpsrv);
    endpointRegistryRemove(entry);
}

static void destructUnpublishedAdditions(imtcpReloadStateV1_t *const state) {
    if (state == NULL) return;
    for (size_t i = 0; i < state->count; ++i) {
        if (state->entries[i].addition && !state->entries[i].additionPublished)
            destructPreparedEndpoint(&state->entries[i].runtime);
    }
}

static tcpsrv_etry_t *findRuntimeEndpoint(const char *const key) {
    tcpsrv_etry_t *entry;
    if (key == NULL) return NULL;
    for (entry = endpoint_registry.head; entry != NULL; entry = entry->next)
        if (entry->endpoint_key != NULL && !strcmp(entry->endpoint_key, key)) return entry;
    return NULL;
}

static tcpsrv_etry_t *findRuntimeEndpointBySourceOrdinal(const size_t ordinal, const size_t count) {
    tcpsrv_etry_t *entry;
    if (endpoint_registry.count < 0 || (size_t)endpoint_registry.count != count || ordinal >= count) return NULL;
    for (entry = endpoint_registry.head; entry != NULL; entry = entry->next)
        if (entry->source_ordinal == ordinal) return entry;
    return NULL;
}

static tcpsrv_etry_t *findRuntimeEndpointByConfigName(const uchar *const name) {
    tcpsrv_etry_t *entry;
    tcpsrv_etry_t *match = NULL;
    if (name == NULL) return NULL;
    for (entry = endpoint_registry.head; entry != NULL; entry = entry->next) {
        if (entry->config_name == NULL || strcmp(entry->config_name, (const char *)name)) continue;
        if (match != NULL) return NULL;
        match = entry;
    }
    return match;
}

static rsRetVal findRuntimeForInstance(const instanceConf_t *const inst,
                                       const modConfData_t *const config,
                                       const size_t ordinal,
                                       const size_t count,
                                       tcpsrv_etry_t **const runtime) {
    char *key = NULL;
    const rsRetVal keyRet = endpointKeyBuild(inst->cnf_params, reloadEffectiveNamespace(inst, config), &key);
    if (runtime == NULL) return RS_RET_PARAM_ERROR;
    *runtime = NULL;
    if (keyRet == RS_RET_OK)
        *runtime = findRuntimeEndpoint(key);
    else if (keyRet == RS_RET_NOT_IMPLEMENTED)
        *runtime = inst->pszInputName == NULL ? findRuntimeEndpointBySourceOrdinal(ordinal, count)
                                              : findRuntimeEndpointByConfigName(inst->pszInputName);
    free(key);
    return keyRet == RS_RET_NOT_IMPLEMENTED ? RS_RET_OK : keyRet;
}

static rsRetVal prepareReloadV1(const void *const pOldCnf, const void *const pNewCnf, void **const pReloadState) {
    const modConfData_t *const oldConfig = pOldCnf;
    const modConfData_t *const newConfig = pNewCnf;
    const instanceConf_t *newInst;
    const instanceConf_t *oldInst;
    imtcpReloadStateV1_t *state = NULL;
    eModReloadCapability_t capability;
    size_t count = 0;
    size_t oldCount = 0;
    size_t capacity;
    size_t index = 0;
    int oldIdentitiesUsable;
    int newIdentitiesUsable;
    DEFiRet;

    if (oldConfig == NULL || newConfig == NULL || newConfig->pConf == NULL || pReloadState == NULL ||
        *pReloadState != NULL)
        return RS_RET_PARAM_ERROR;
    CHKiRet(classifyReloadSourceCandidateV1(oldConfig, newConfig, &capability));
    if (capability != eMOD_RELOAD_LIVE_SWAP && capability != eMOD_RELOAD_REUSE &&
        capability != eMOD_RELOAD_NEW_SESSIONS && capability != eMOD_RELOAD_LIVE_AND_NEW_SESSIONS &&
        capability != eMOD_RELOAD_DRAIN_REPLACE)
        ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
    for (newInst = newConfig->root; newInst != NULL; newInst = newInst->next) ++count;
    for (oldInst = oldConfig->root; oldInst != NULL; oldInst = oldInst->next) ++oldCount;
    CHKiRet(reloadConfigHasUniqueIdentities(oldConfig, &oldIdentitiesUsable));
    CHKiRet(reloadConfigHasUniqueIdentities(newConfig, &newIdentitiesUsable));
    if (oldCount > SIZE_MAX - count) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    capacity = oldCount + count;
    if (capacity > (SIZE_MAX - sizeof(*state)) / sizeof(state->entries[0])) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    CHKmalloc(state = calloc(1, sizeof(*state) + capacity * sizeof(state->entries[0])));
    for (newInst = newConfig->root; newInst != NULL; newInst = newInst->next) {
        char *key = NULL;
        tcpsrv_t *preparedServer = NULL;
        tcpLstnParams_t *preparedParams = NULL;
        const rsRetVal keyRet =
            endpointKeyBuild(newInst->cnf_params, reloadEffectiveNamespace(newInst, newConfig), &key);
        if (keyRet == RS_RET_OK)
            state->entries[index].runtime = findRuntimeEndpoint(key);
        else if (keyRet == RS_RET_NOT_IMPLEMENTED)
            state->entries[index].runtime = newInst->pszInputName == NULL
                                                ? findRuntimeEndpointBySourceOrdinal(index, oldCount)
                                                : findRuntimeEndpointByConfigName(newInst->pszInputName);
        else
            ABORT_FINALIZE(keyRet);
        free(key);
        key = NULL;
        if (state->entries[index].runtime == NULL && keyRet == RS_RET_OK) {
            CHKiRet(prepareListenerServer((modConfData_t *)newConfig, (instanceConf_t *)newInst, &preparedServer,
                                          &preparedParams));
            iRet =
                endpointRegistryBuild(preparedServer, preparedParams, preparedParams->pszNetworkNamespace,
                                      newInst->pszInputName == NULL ? UCHAR_CONSTANT("imtcp") : newInst->pszInputName,
                                      index, &state->entries[index].runtime);
            if (iRet != RS_RET_OK) {
                tcpsrv.SetUsrP(preparedServer, NULL);
                tcpsrv.Destruct(&preparedServer);
                ABORT_FINALIZE(iRet);
            }
            preparedServer = NULL;
            state->entries[index].addition = 1;
            state->count = index + 1;
            CHKiRet(tcpsrv.ConstructFinalizePrepared(state->entries[index].runtime->tcpsrv));
            CHKiRet(startSrvWrkr(state->entries[index].runtime, 1));
        }
        if (state->entries[index].runtime == NULL || state->entries[index].runtime->state != IMTCP_ENDPOINT_ACTIVE)
            ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
        if (state->entries[index].runtime->tcpsrv->pLstnPorts == NULL ||
            state->entries[index].runtime->tcpsrv->pLstnPorts->pNext != NULL)
            ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
        state->count = index + 1;
        state->entries[index].sessionMax = newInst->iTCPSessMax;
        {
            char *const *aclSpecs;
            size_t aclSpecCount;
            int aclMaterialized;
            struct AllowedSenders *allowedLast = NULL;
            const struct AllowedSenders *const allowed =
                reloadEffectiveAllowedSenders(newInst, newConfig, &state->entries[index].useLegacyAllowedSender);
            reloadEffectiveAllowedSenderSpecs(newInst, newConfig, &aclSpecs, &aclSpecCount, &aclMaterialized);
            (void)aclSpecs;
            (void)aclSpecCount;
            if (!aclMaterialized) {
                if (state->entries[index].addition) ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
            } else {
                CHKiRet(net.cloneAllowedSenders(allowed, &state->entries[index].preparedAllowedSenders, &allowedLast));
                state->entries[index].applyAllowedSenders = 1;
                if (!state->entries[index].addition) {
                    state->entries[index].sessionAllowedCount =
                        (size_t)(state->entries[index].sessionMax > state->entries[index].runtime->tcpsrv->iSessMax
                                     ? state->entries[index].sessionMax
                                     : state->entries[index].runtime->tcpsrv->iSessMax);
                    CHKmalloc(state->entries[index].sessionAllowed =
                                  calloc(state->entries[index].sessionAllowedCount,
                                         sizeof(state->entries[index].sessionAllowed[0])));
                }
            }
        }
        if (!state->entries[index].addition &&
            state->entries[index].sessionMax != state->entries[index].runtime->tcpsrv->iSessMax) {
            CHKmalloc(state->entries[index].preparedSessionSlots =
                          calloc((size_t)state->entries[index].sessionMax,
                                 sizeof(state->entries[index].preparedSessionSlots[0])));
        }
        if (!state->entries[index].addition &&
            newInst->iTCPLstnMax != state->entries[index].runtime->tcpsrv->iLstnMax) {
            tcpsrv_listener_tables_t *const tables = &state->entries[index].preparedListenerTables;
            CHKiRet(tcpsrv.ValidateListenerTableCapacity(state->entries[index].runtime->tcpsrv, newInst->iTCPLstnMax));
            tables->capacity = newInst->iTCPLstnMax;
            CHKmalloc(tables->streams = calloc((size_t)tables->capacity, sizeof(tables->streams[0])));
            CHKmalloc(tables->ports = calloc((size_t)tables->capacity, sizeof(tables->ports[0])));
            CHKmalloc(tables->descriptors = calloc((size_t)tables->capacity, sizeof(tables->descriptors[0])));
        }
        if (!state->entries[index].addition)
            CHKiRet(prepareReloadRateLimiter(&state->entries[index], oldConfig, newInst, newConfig));
        state->entries[index].flowControl = newInst->bUseFlowControl;
        state->entries[index].starvationMaxReads = newInst->starvationMaxReads;
        state->entries[index].applyRateLimitScalars =
            newInst->cnf_params->pszRatelimitName == NULL && !state->entries[index].swapRateLimiter;
        state->entries[index].ratelimitInterval = (unsigned)newInst->ratelimitInterval;
        state->entries[index].ratelimitBurst = (unsigned)newInst->ratelimitBurst;
        state->entries[index].notifyOnConnectionClose = newInst->bEmitMsgOnClose;
        state->entries[index].notifyOnConnectionOpen = newInst->bEmitMsgOnOpen;
        state->entries[index].preserveCase = newInst->bPreserveCase;
        state->entries[index].keepAlive = newInst->bKeepAlive;
        state->entries[index].keepAliveInterval = newInst->iKeepAliveIntvl;
        state->entries[index].keepAliveProbes = newInst->iKeepAliveProbes;
        state->entries[index].keepAliveTime = newInst->iKeepAliveTime;
        state->entries[index].framingFix = newInst->bSPFramingFix;
        state->entries[index].additionalFrameDelimiter = newInst->iAddtlFrameDelim;
        state->entries[index].maxFrameSize = newInst->maxFrameSize;
        state->entries[index].disableLFDelimiter = newInst->bDisableLFDelim;
        state->entries[index].discardTruncatedMessage = newInst->discardTruncatedMsg;
        state->entries[index].supportOctetCountedFraming = newInst->cnf_params->bSuppOctetFram;
        state->entries[index].compressionMode = newInst->compressionMode;
        state->entries[index].compressionDriver = newInst->compressionDriver;
        state->entries[index].compressionMaxExpansionRatio = newInst->compressionMaxExpansionRatio;
        state->entries[index].compressionMaxDecompressedBytesPerReceive =
            newInst->compressionMaxDecompressedBytesPerReceive;
        state->entries[index].compressionMaxTotalZstdWindowBytes = newInst->compressionMaxTotalZstdWindowBytes;
        state->entries[index].multiLine = newInst->cnf_params->bMultiLine;
        CHKiRet(prepareReloadStartRegex(newInst->cnf_params->pszStartRegex, &state->entries[index].startRegex));
        u_cstr_copy(state->entries[index].defaultTZ, newInst->dfltTZ == NULL ? UCHAR_CONSTANT("") : newInst->dfltTZ,
                    sizeof(state->entries[index].defaultTZ));
        if (newInst->pszBindRuleset == NULL) {
            state->entries[index].ruleset = newConfig->pConf->rulesets.pDflt;
        } else {
            CHKiRet(rulesetGetRuleset(newConfig->pConf, &state->entries[index].ruleset, newInst->pszBindRuleset));
        }
        if (state->entries[index].ruleset == NULL) ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
        ++index;
    }
    if (oldIdentitiesUsable && newIdentitiesUsable) {
        size_t oldOrdinal = 0;
        for (oldInst = oldConfig->root; oldInst != NULL; oldInst = oldInst->next, ++oldOrdinal) {
            char *identity = NULL;
            const instanceConf_t *matched = NULL;
            iRet = reloadEndpointIdentity(oldInst, oldConfig, &identity);
            if (iRet == RS_RET_OK) iRet = reloadFindInstanceByIdentity(newConfig, identity, &matched);
            free(identity);
            if (iRet != RS_RET_OK) ABORT_FINALIZE(iRet);
            if (matched != NULL) continue;
            CHKiRet(findRuntimeForInstance(oldInst, oldConfig, oldOrdinal, oldCount, &state->entries[index].runtime));
            if (state->entries[index].runtime == NULL || state->entries[index].runtime->state != IMTCP_ENDPOINT_ACTIVE)
                ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
            state->entries[index].removal = 1;
            ++index;
            state->count = index;
        }
    }
    *pReloadState = state;
    state = NULL;

finalize_it:
    destructUnpublishedAdditions(state);
    freeReloadPreparedValues(state);
    free(state);
    RETiRet;
}

static rsRetVal quiesceReloadV1(void *const pReloadState, const struct timespec *const deadline) {
    imtcpReloadStateV1_t *const state = pReloadState;
    rsRetVal ret = RS_RET_OK;
    if (state == NULL || deadline == NULL) return RS_RET_PARAM_ERROR;
    for (size_t i = 0; i < state->count; ++i) {
        if (state->entries[i].addition) continue;
        ret = tcpsrv.RequestFence(state->entries[i].runtime->tcpsrv, &state->entries[i].fenceToken);
        if (ret != RS_RET_OK) break;
        ret = tcpsrv.WaitFence(state->entries[i].runtime->tcpsrv, state->entries[i].fenceToken, deadline);
        if (ret != RS_RET_OK) break;
        state->entries[i].fenceAcquired = 1;
    }
    if (ret == RS_RET_OK) {
        for (size_t i = 0; i < state->count; ++i) {
            imtcpReloadEntryV1_t *const entry = &state->entries[i];
            tcpsrv_t *server;
            if (entry->preparedSessionSlots == NULL) continue;
            server = entry->runtime->tcpsrv;
            if (entry->sessionMax >= server->iSessMax) continue;
            for (int slot = entry->sessionMax; slot < server->iSessMax; ++slot) {
                if (server->pSessions[slot] != NULL) {
                    ret = RS_RET_NOT_IMPLEMENTED;
                    break;
                }
            }
            if (ret != RS_RET_OK) break;
        }
    }
    if (ret == RS_RET_OK) {
        for (size_t i = 0; i < state->count; ++i) {
            imtcpReloadEntryV1_t *const entry = &state->entries[i];
            tcpsrv_listener_tables_t *const tables = &entry->preparedListenerTables;
            tcpsrv_t *server;
            if (tables->streams == NULL) continue;
            server = entry->runtime->tcpsrv;
            memcpy(tables->streams, server->ppLstn, (size_t)server->iLstnCurr * sizeof(tables->streams[0]));
            memcpy(tables->ports, server->ppLstnPort, (size_t)server->iLstnCurr * sizeof(tables->ports[0]));
            memcpy(tables->descriptors, server->ppioDescrPtr,
                   (size_t)server->iLstnCurr * sizeof(tables->descriptors[0]));
        }
    }
    if (ret == RS_RET_OK) {
        for (size_t i = 0; i < state->count; ++i) {
            imtcpReloadEntryV1_t *const entry = &state->entries[i];
            imtcpAclEvaluation_t acl;
            if (entry->addition || !entry->applyAllowedSenders) continue;
            acl.root = entry->preparedAllowedSenders;
            acl.useLegacy = entry->useLegacyAllowedSender;
            ret = tcpsrv.EvaluateSessionPolicyWhileFenced(entry->runtime->tcpsrv, evaluateReloadAclSession, &acl,
                                                          entry->sessionAllowed, entry->sessionAllowedCount);
            if (ret != RS_RET_OK) break;
        }
    }
    if (ret == RS_RET_OK) {
        for (size_t i = 0; i < state->count; ++i) {
            imtcpReloadEntryV1_t *const entry = &state->entries[i];
            tcpsrv_t *server;
            if (entry->preparedSessionSlots == NULL) continue;
            server = entry->runtime->tcpsrv;
            memcpy(entry->preparedSessionSlots, server->pSessions,
                   (size_t)(server->iSessMax < entry->sessionMax ? server->iSessMax : entry->sessionMax) *
                       sizeof(entry->preparedSessionSlots[0]));
        }
    }
    if (ret == RS_RET_OK) {
        for (size_t i = 0; i < state->count; ++i) {
            if (!state->entries[i].addition) continue;
            ret = tcpsrv.ActivatePreparedListeners(state->entries[i].runtime->tcpsrv);
            if (ret != RS_RET_OK) break;
        }
    }
    if (ret != RS_RET_OK) {
        for (size_t i = state->count; i != 0; --i) {
            imtcpReloadEntryV1_t *const entry = &state->entries[i - 1];
            if (!entry->fenceAcquired) continue;
            (void)tcpsrv.ReleaseFence(entry->runtime->tcpsrv, entry->fenceToken);
            entry->fenceAcquired = 0;
        }
    }
    return ret;
}

static rsRetVal resumeReloadV1(void *const pReloadState) {
    imtcpReloadStateV1_t *const state = pReloadState;
    rsRetVal ret = RS_RET_OK;
    if (state == NULL) return RS_RET_PARAM_ERROR;
    for (size_t i = state->count; i != 0; --i) {
        imtcpReloadEntryV1_t *const entry = &state->entries[i - 1];
        if (!entry->fenceAcquired) continue;
        const rsRetVal releaseRet = tcpsrv.ReleaseFence(entry->runtime->tcpsrv, entry->fenceToken);
        if (ret == RS_RET_OK && releaseRet != RS_RET_OK) ret = releaseRet;
        if (releaseRet == RS_RET_OK) entry->fenceAcquired = 0;
    }
    return ret;
}

static void commitReloadV1(void *const pReloadState) {
    imtcpReloadStateV1_t *const state = pReloadState;
    for (size_t i = 0; i < state->count; ++i) {
        if (state->entries[i].removal) continue;
        tcpsrv_t *const server = state->entries[i].runtime->tcpsrv;
        if (state->entries[i].preparedSessionSlots != NULL) {
            state->entries[i].retiredSessionSlots = server->pSessions;
            server->pSessions = state->entries[i].preparedSessionSlots;
            server->iSessMax = state->entries[i].sessionMax;
            state->entries[i].preparedSessionSlots = NULL;
        }
        if (state->entries[i].preparedListenerTables.streams != NULL)
            tcpsrv.SwapListenerTablesLive(server, &state->entries[i].preparedListenerTables,
                                          &state->entries[i].retiredListenerTables);
        if (!state->entries[i].addition && state->entries[i].applyAllowedSenders) {
            tcpsrv.SwapAllowedSendersLive(
                server, state->entries[i].preparedAllowedSenders, state->entries[i].useLegacyAllowedSender,
                &state->entries[i].retiredAllowedSenders, &state->entries[i].retiredAllowedSendersOwned);
            state->entries[i].preparedAllowedSenders = NULL;
            tcpsrv.ApplySessionPolicyLive(server, state->entries[i].sessionAllowed,
                                          (size_t)state->entries[i].sessionMax);
        }
        if (!state->entries[i].addition && state->entries[i].swapRateLimiter) {
            tcpsrv.SwapRateLimiterLive(server, state->entries[i].preparedRateLimiter,
                                       state->entries[i].preparedRatelimitName, &state->entries[i].retiredRateLimiter,
                                       &state->entries[i].retiredRatelimitName);
            state->entries[i].preparedRateLimiter = NULL;
            state->entries[i].preparedRatelimitName = NULL;
        }
        tcpsrv_reload_profile_t profile = {
            .useFlowControl = state->entries[i].flowControl,
            .starvationMaxReads = state->entries[i].starvationMaxReads,
            .applyRateLimitScalars = state->entries[i].applyRateLimitScalars,
            .ratelimitInterval = state->entries[i].ratelimitInterval,
            .ratelimitBurst = state->entries[i].ratelimitBurst,
            .notifyOnConnectionClose = state->entries[i].notifyOnConnectionClose,
            .notifyOnConnectionOpen = state->entries[i].notifyOnConnectionOpen,
            .preserveCase = state->entries[i].preserveCase,
            .keepAlive = state->entries[i].keepAlive,
            .keepAliveInterval = state->entries[i].keepAliveInterval,
            .keepAliveProbes = state->entries[i].keepAliveProbes,
            .keepAliveTime = state->entries[i].keepAliveTime,
            .framingFix = state->entries[i].framingFix,
            .additionalFrameDelimiter = state->entries[i].additionalFrameDelimiter,
            .maxFrameSize = state->entries[i].maxFrameSize,
            .disableLFDelimiter = state->entries[i].disableLFDelimiter,
            .discardTruncatedMessage = state->entries[i].discardTruncatedMessage,
            .supportOctetCountedFraming = state->entries[i].supportOctetCountedFraming,
            .compressionMode = state->entries[i].compressionMode,
            .compressionDriver = state->entries[i].compressionDriver,
            .compressionMaxExpansionRatio = state->entries[i].compressionMaxExpansionRatio,
            .compressionMaxDecompressedBytesPerReceive = state->entries[i].compressionMaxDecompressedBytesPerReceive,
            .compressionMaxTotalZstdWindowBytes = state->entries[i].compressionMaxTotalZstdWindowBytes,
            .multiLine = state->entries[i].multiLine,
            .startRegex = state->entries[i].startRegex,
            .ruleset = state->entries[i].ruleset,
        };
        u_cstr_copy(profile.defaultTZ, state->entries[i].defaultTZ, sizeof(profile.defaultTZ));
        tcpsrv.ApplyReloadProfile(server, &profile);
        state->entries[i].startRegex = NULL;
    }
    for (size_t i = 0; i < state->count; ++i) {
        imtcpReloadEntryV1_t *const entry = &state->entries[i];
        if (!entry->removal) continue;
        tcpsrv.DisableAcceptWhileFenced(entry->runtime->tcpsrv);
        entry->runtime->state = IMTCP_ENDPOINT_NO_ACCEPT;
    }
    for (size_t i = 0; i < state->count; ++i) {
        imtcpReloadEntryV1_t *const entry = &state->entries[i];
        if (!entry->addition) continue;
        endpointRegistryPublish(entry->runtime);
        entry->additionPublished = 1;
        authorizeSrvWrkr(entry->runtime);
    }
}

static void abortReloadV1(void *const pReloadState) {
    if (pReloadState == NULL) return;
    (void)resumeReloadV1(pReloadState);
    destructUnpublishedAdditions(pReloadState);
    freeReloadPreparedValues(pReloadState);
    free(pReloadState);
}

static rsRetVal retireReloadV1(void *const pReloadState) {
    imtcpReloadStateV1_t *const state = pReloadState;
    rsRetVal ret;
    if (pReloadState == NULL) return RS_RET_PARAM_ERROR;
    ret = resumeReloadV1(pReloadState);
    if (ret != RS_RET_OK) return ret;
    for (size_t i = 0; i < state->count; ++i) {
        imtcpReloadEntryV1_t *const reloadEntry = &state->entries[i];
        if (!reloadEntry->removal || reloadEntry->runtime == NULL) continue;
        pthread_mutex_lock(&reloadEntry->runtime->startMut);
        const int finished = reloadEntry->runtime->runFinished;
        pthread_mutex_unlock(&reloadEntry->runtime->startMut);
        if (!finished) return RS_RET_RETRY;
    }
    for (size_t i = 0; i < state->count; ++i) {
        imtcpReloadEntryV1_t *const reloadEntry = &state->entries[i];
        if (!reloadEntry->removal || reloadEntry->runtime == NULL) continue;
        stopSrvWrkr(reloadEntry->runtime);
        endpointRegistryUnlink(reloadEntry->runtime);
        tcpsrv.Destruct(&reloadEntry->runtime->tcpsrv);
        endpointRegistryRemove(reloadEntry->runtime);
        reloadEntry->runtime = NULL;
    }
    freeReloadPreparedValues(state);
    free(pReloadState);
    return RS_RET_OK;
}

static rsRetVal getReloadInterfaceV1(modReloadInterfaceV1_t *const interface) {
    const size_t minimumSize = offsetof(modReloadInterfaceV1_t, retire) + sizeof(interface->retire);
    const size_t quiesceSize = offsetof(modReloadInterfaceV1_t, resume) + sizeof(interface->resume);
    if (interface == NULL || interface->version != eMOD_RELOAD_INTERFACE_V1 || interface->structSize < minimumSize)
        return RS_RET_MISSING_INTERFACE;
    interface->capabilityFlags = eMOD_RELOAD_CAP_VALIDATE_PRIVATE | eMOD_RELOAD_CAP_PREPARE | eMOD_RELOAD_CAP_REUSE |
                                 eMOD_RELOAD_CAP_COMMIT | eMOD_RELOAD_CAP_RETIRE;
    interface->classify = classifyReloadSourceCandidateV1;
    interface->prepare = prepareReloadV1;
    interface->commit = commitReloadV1;
    interface->abort = abortReloadV1;
    interface->retire = retireReloadV1;
    if (interface->structSize >= quiesceSize) {
        interface->capabilityFlags |= eMOD_RELOAD_CAP_QUIESCE;
        interface->quiesce = quiesceReloadV1;
        interface->resume = resumeReloadV1;
    }
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

    if (object->objType == CNFOBJ_RATELIMIT) {
        CHKiRet(lowerReloadRateProfile(object, build->config));
        FINALIZE;
    }
    if (object->objType == CNFOBJ_MODULE && sourceModuleIsImtcp(object)) {
        const struct nvlst *const load = findSourceParam(object->nvlst, "load");
        if (build->moduleSeen) ABORT_FINALIZE(RS_RET_MODULE_ALREADY_IN_CONF);
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
    CHKmalloc(candidate->reloadRateConfig = calloc(1, sizeof(*candidate->reloadRateConfig)));
    CHKiRet(ratelimit_cfgsInit(&candidate->reloadRateConfig->ratelimit_cfgs));
    candidate->reloadRateConfigInitialized = RSTRUE;
    initModuleDefaults(candidate);
    candidate->reloadSnapshot = RSTRUE;
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
        addListner(runModConf, inst);
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
    reloadRateProfilesDestruct(&pModConf->reloadRateProfiles);
    if (pModConf->reloadRateConfig != NULL) {
        if (pModConf->reloadRateConfigInitialized) ratelimit_cfgsDestruct(&pModConf->reloadRateConfig->ratelimit_cfgs);
        free(pModConf->reloadRateConfig);
        pModConf->reloadRateConfig = NULL;
        pModConf->reloadRateConfigInitialized = RSFALSE;
    }
    free(pModConf->gnutlsPriorityString);
    free(pModConf->reloadModuleLoadName);
    freeReloadAllowedSenderSpecs(&pModConf->reloadAllowedSenderSpecs, &pModConf->reloadAllowedSenderSpecCount);
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
        freeReloadAllowedSenderSpecs(&inst->reloadAllowedSenderSpecs, &inst->reloadAllowedSenderSpecCount);
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
            free((void *)inst->cnf_params->pszStartRegex);
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
    pthread_mutex_lock(&etry->startMut);
    while (!etry->startAuthorized && !etry->startCancelled) pthread_cond_wait(&etry->startCond, &etry->startMut);
    const int cancelled = etry->startCancelled;
    pthread_mutex_unlock(&etry->startMut);
    if (cancelled) return NULL;
    iRet = tcpsrv.Run(etry->tcpsrv);
    if (iRet != RS_RET_OK) {
        LogError(0, iRet, "imtcp: error while terminating server; rsyslog may hang on shutdown");
    }
    pthread_mutex_lock(&etry->startMut);
    etry->runFinished = 1;
    pthread_cond_broadcast(&etry->startCond);
    pthread_mutex_unlock(&etry->startMut);
    return NULL;
}


/* support for running multiple servers on multiple threads (one server per thread) */
static rsRetVal startSrvWrkr(tcpsrv_etry_t *const etry, const int parked) {
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
    pthread_mutex_lock(&etry->startMut);
    etry->startAuthorized = !parked;
    etry->startCancelled = 0;
    etry->runFinished = 0;
    pthread_mutex_unlock(&etry->startMut);
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
    return r == 0 ? RS_RET_OK : RS_RET_ERR;
}

static void authorizeSrvWrkr(tcpsrv_etry_t *const etry) {
    pthread_mutex_lock(&etry->startMut);
    etry->startAuthorized = 1;
    pthread_cond_signal(&etry->startCond);
    pthread_mutex_unlock(&etry->startMut);
}

static void cancelParkedSrvWrkr(tcpsrv_etry_t *const etry) {
    if (!etry->thread_started) return;
    pthread_mutex_lock(&etry->startMut);
    etry->startCancelled = 1;
    pthread_cond_signal(&etry->startCond);
    pthread_mutex_unlock(&etry->startMut);
    pthread_join(etry->tid, NULL);
    etry->thread_started = 0;
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
    tcpsrv_etry_t *etry = endpoint_registry.head;
    while (etry != NULL) {
        (void)startSrvWrkr(etry, 0);
        etry = etry->next;
    }

    while (glbl.GetGlobalInputTermState() == 0) srSleep(0, 100000);

    /* De-initialize every runtime server, including endpoints added by HUP. */
    etry = endpoint_registry.head;
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
    objRelease(prop, CORE_COMPONENT);
    objRelease(glbl, CORE_COMPONENT);
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
    CODEqueryEtryPt_RELOAD_V1_QUERIES;
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
    CHKiRet(objUse(glbl, CORE_COMPONENT));

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
