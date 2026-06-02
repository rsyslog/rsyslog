/* imbeats.c
 * Input module for Elastic Beats / Lumberjack v2.
 *
 * Transport and TLS are delegated to the netstrm subsystem. Protocol parsing,
 * JSON decoding, and ACK timing live here.
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <poll.h>
#if defined(HAVE_SYS_EPOLL_H) && defined(HAVE_EPOLL_CREATE)
    #include <sys/epoll.h>
#endif
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <libfastjson/json.h>
#include <libfastjson/json_tokener.h>

#include "rsyslog.h"
#include "cfsysline.h"
#include "module-template.h"
#include "errmsg.h"
#include "unicode-helper.h"
#include "net.h"
#include "netstrm.h"
#include "ruleset.h"
#include "prop.h"
#include "statsobj.h"
#include "msg.h"
#include "dirty.h"
#include "glbl.h"
#include "ratelimit.h"
#include "srUtils.h"
#include "tcpsrv.h"

#include "lj_parser.h"

MODULE_TYPE_INPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME("imbeats")

#define IMBEATS_DEFAULT_MAX_WINDOW_SIZE 1024
#define IMBEATS_MAX_WINDOW_SIZE_LIMIT 1000000
#define IMBEATS_DEFAULT_MAX_FRAME_SIZE (10 * 1024 * 1024)
#define IMBEATS_DEFAULT_MAX_DECOMPRESSED_SIZE (64 * 1024 * 1024)
#define IMBEATS_DEFAULT_MAX_BATCH_BYTES IMBEATS_DEFAULT_MAX_DECOMPRESSED_SIZE
#define IMBEATS_MAX_BYTE_SIZE_LIMIT 200000000
#define IMBEATS_DEFAULT_MAX_SESSIONS 1000
#define IMBEATS_DEFAULT_WORKER_THREADS 2
#define IMBEATS_MAX_WORKER_THREADS 1024
#define IMBEATS_DEFAULT_STARVATION_MAX_READS 500
#define IMBEATS_ACCEPT_BATCH_LIMIT 64

#if defined(HAVE_SYS_EPOLL_H) && defined(HAVE_EPOLL_CREATE)
    #define IMBEATS_HAVE_EPOLL 1
#endif

DEF_IMOD_STATIC_DATA;
DEFobjCurrIf(glbl) DEFobjCurrIf(net) DEFobjCurrIf(netstrm) DEFobjCurrIf(netstrms) DEFobjCurrIf(prop)
    DEFobjCurrIf(ruleset) DEFobjCurrIf(statsobj)

        typedef struct session_s session_t;
typedef struct imbeats_io_s imbeats_io_t;

typedef struct instanceConf_s {
    struct instanceConf_s *next;
    uchar *port;
    uchar *address;
    uchar *pszBindRuleset;
    ruleset_t *pBindRuleset;
    uchar *pszInputName;
    prop_t *pInputName;
    uchar *pszLstnPortFileName;
    char *pszNetworkNamespace;

    uchar *pszStrmDrvrName;
    int iStrmDrvrMode;
    uchar *pszStrmDrvrAuthMode;
    uchar *pszStrmDrvrPermitExpiredCerts;
    uchar *pszStrmDrvrCAFile;
    uchar *pszStrmDrvrCRLFile;
    uchar *pszStrmDrvrKeyFile;
    uchar *pszStrmDrvrCertFile;
    uchar *gnutlsPriorityString;
    permittedPeers_t *pPermPeersRoot;
    int iStrmDrvrExtendedCertCheck;
    int iStrmDrvrSANPreference;
    int iStrmTlsVerifyDepth;
    int iStrmTlsRevocationCheck;

    int bKeepAlive;
    int iKeepAliveIntvl;
    int iKeepAliveProbes;
    int iKeepAliveTime;
    uint32_t maxWindowSize;
    size_t maxFrameSize;
    size_t maxDecompressedSize;
    size_t maxBatchBytes;

    netstrms_t *pNS;
    netstrm_t **listeners;
    int *listener_socks;
    size_t listener_count;
    size_t listener_cap;
    imbeats_io_t *listener_descs;
    pthread_t listener_tid;
    int listener_running;
    int shuttingDown;
    pthread_mutex_t mutSessions;
    pthread_mutex_t mutWork;
    pthread_cond_t condWork;
    int workShutdown;
    session_t *sessions;
    session_t *workHead;
    session_t *workTail;
    pthread_t *worker_tids;
    unsigned workerThreads;
    unsigned workersStarted;
    unsigned starvationMaxReads;
    unsigned maxSessions;
    unsigned activeSessions;
#if defined(IMBEATS_HAVE_EPOLL)
    int epoll_fd;
#endif
} instanceConf_t;

typedef struct modConfData_s {
    rsconf_t *pConf;
    instanceConf_t *root;
    instanceConf_t *tail;
} modConfData_t;

struct session_s {
    instanceConf_t *inst;
    netstrm_t *pStrm;
    int done;
    int inWorkQueue;
    int sock;
    unsigned ioDirection;
    uint32_t lastAckedSeq;
    uchar *fromHostFQDN;
    prop_t *fromHost;
    prop_t *fromHostIP;
    prop_t *fromHostPort;
    char *fromHostIPStr;
    char *fromHostPortStr;
    imbeats_io_t *io;
    struct lj_batch_s batch;
    int batchActive;
    size_t submitIdx;
    uint32_t pendingSeq;
    unsigned char fixed[8];
    unsigned char *payload;
    size_t payload_len;
    unsigned char *io_buf;
    size_t io_need;
    size_t io_off;
    unsigned char ack[6];
    size_t ack_off;
    enum {
        IMBEATS_ST_READ_WINDOW_HDR,
        IMBEATS_ST_READ_WINDOW_SIZE,
        IMBEATS_ST_READ_FRAME_HDR,
        IMBEATS_ST_READ_JSON_SEQ,
        IMBEATS_ST_READ_JSON_LEN,
        IMBEATS_ST_READ_JSON_PAYLOAD,
        IMBEATS_ST_READ_COMP_LEN,
        IMBEATS_ST_READ_COMP_PAYLOAD,
        IMBEATS_ST_SUBMIT_EVENTS,
        IMBEATS_ST_SEND_ACK
    } state;
    session_t *next;
    session_t *workNext;
};

struct imbeats_io_s {
    enum { IMBEATS_IO_LISTENER, IMBEATS_IO_SESSION } type;
    instanceConf_t *inst;
    session_t *sess;
    size_t listener_idx;
};

static modConfData_t *loadModConf = NULL;
static modConfData_t *runModConf = NULL;
static prop_t *pInputName = NULL;

static struct cnfparamdescr modpdescr[] = {};
static struct cnfparamblk modpblk = {CNFPARAMBLK_VERSION, 0, modpdescr};

static struct cnfparamdescr inppdescr[] = {
    {"port", eCmdHdlrString, CNFPARAM_REQUIRED},
    {"address", eCmdHdlrString, 0},
    {"ruleset", eCmdHdlrString, 0},
    {"name", eCmdHdlrString, 0},
    {"listenportfilename", eCmdHdlrString, 0},
    {"networknamespace", eCmdHdlrString, 0},
    {"streamdriver.name", eCmdHdlrString, 0},
    {"streamdriver.mode", eCmdHdlrNonNegInt, 0},
    {"streamdriver.authmode", eCmdHdlrString, 0},
    {"streamdriver.permitexpiredcerts", eCmdHdlrString, 0},
    {"streamdriver.cafile", eCmdHdlrString, 0},
    {"streamdriver.crlfile", eCmdHdlrString, 0},
    {"streamdriver.keyfile", eCmdHdlrString, 0},
    {"streamdriver.certfile", eCmdHdlrString, 0},
    {"streamdriver.checkextendedkeypurpose", eCmdHdlrBinary, 0},
    {"streamdriver.prioritizesan", eCmdHdlrBinary, 0},
    {"streamdriver.tlsverifydepth", eCmdHdlrPositiveInt, 0},
    {"streamdriver.tlsrevocationcheck", eCmdHdlrBinary, 0},
    {"permittedpeer", eCmdHdlrArray, 0},
    {"keepalive", eCmdHdlrBinary, 0},
    {"keepalive.probes", eCmdHdlrNonNegInt, 0},
    {"keepalive.time", eCmdHdlrNonNegInt, 0},
    {"keepalive.interval", eCmdHdlrNonNegInt, 0},
    {"gnutlsprioritystring", eCmdHdlrString, 0},
    {"maxwindowsize", eCmdHdlrPositiveInt, 0},
    {"maxframesize", eCmdHdlrPositiveInt, 0},
    {"maxdecompressedsize", eCmdHdlrPositiveInt, 0},
    {"maxbatchbytes", eCmdHdlrPositiveInt, 0},
    {"maxsessions", eCmdHdlrPositiveInt, 0},
    {"workerthreads", eCmdHdlrPositiveInt, 0},
    {"starvationprotection.maxreads", eCmdHdlrNonNegInt, 0},
};
static struct cnfparamblk inppblk = {CNFPARAMBLK_VERSION, sizeof(inppdescr) / sizeof(inppdescr[0]), inppdescr};

static struct {
    statsobj_t *stats;
    STATSCOUNTER_DEF(ctrConnectionsAccepted, mutCtrConnectionsAccepted)
    STATSCOUNTER_DEF(ctrConnectionsClosed, mutCtrConnectionsClosed)
    STATSCOUNTER_DEF(ctrProtocolErrors, mutCtrProtocolErrors)
    STATSCOUNTER_DEF(ctrBatchesReceived, mutCtrBatchesReceived)
    STATSCOUNTER_DEF(ctrBatchesAcked, mutCtrBatchesAcked)
    STATSCOUNTER_DEF(ctrEventsReceived, mutCtrEventsReceived)
    STATSCOUNTER_DEF(ctrEventsSubmitted, mutCtrEventsSubmitted)
    STATSCOUNTER_DEF(ctrEventsFailed, mutCtrEventsFailed)
    STATSCOUNTER_DEF(ctrCompressedFrames, mutCtrCompressedFrames)
    STATSCOUNTER_DEF(ctrJsonDecodeFailures, mutCtrJsonDecodeFailures)
    STATSCOUNTER_DEF(ctrAckFailures, mutCtrAckFailures)
    STATSCOUNTER_DEF(ctrWindowsRejected, mutCtrWindowsRejected)
    STATSCOUNTER_DEF(ctrFramesRejected, mutCtrFramesRejected)
    STATSCOUNTER_DEF(ctrCompressedRejected, mutCtrCompressedRejected)
    STATSCOUNTER_DEF(ctrConnectionsActive, mutCtrConnectionsActive)
    STATSCOUNTER_DEF(ctrConnectionsRejected, mutCtrConnectionsRejected)
    STATSCOUNTER_DEF(ctrStarvationProtect, mutCtrStarvationProtect)
} statsCounter;

#include "im-helper.h"

static inline void std_checkRuleset_genErrMsg(__attribute__((unused)) modConfData_t *modConf, instanceConf_t *inst) {
    LogError(0, NO_ERRCODE, "imbeats: ruleset '%s' for port '%s' not found - using default ruleset instead",
             inst->pszBindRuleset, inst->port);
}

static rsRetVal createInstance(instanceConf_t **const pinst);
static rsRetVal buildListeners(instanceConf_t *inst);
static rsRetVal destroyInstanceRuntime(instanceConf_t *inst);
static void destroySession(session_t *sess);
static void *listenerThread(void *arg);
static void *workerThread(void *arg);

static int instanceIsShuttingDown(instanceConf_t *const inst) {
    int shuttingDown;

    pthread_mutex_lock(&inst->mutSessions);
    shuttingDown = inst->shuttingDown;
    pthread_mutex_unlock(&inst->mutSessions);
    return shuttingDown;
}

static void setInstanceShuttingDown(instanceConf_t *const inst) {
    pthread_mutex_lock(&inst->mutSessions);
    inst->shuttingDown = 1;
    pthread_mutex_unlock(&inst->mutSessions);
}

static void clearInstanceShuttingDown(instanceConf_t *const inst) {
    pthread_mutex_lock(&inst->mutSessions);
    inst->shuttingDown = 0;
    pthread_mutex_unlock(&inst->mutSessions);
}

static rsRetVal validateUInt32Limit(const char *const name,
                                    const int64_t value,
                                    const uint32_t max,
                                    uint32_t *const target) {
    if (value < 1 || value > max) {
        LogError(0, RS_RET_PARAM_ERROR, "imbeats: invalid value for '%s' parameter given is %lld, valid range is 1..%u",
                 name, (long long)value, max);
        return RS_RET_PARAM_ERROR;
    }
    *target = (uint32_t)value;
    return RS_RET_OK;
}

static rsRetVal validateSizeLimit(const char *const name, const int64_t value, const size_t max, size_t *const target) {
    if (value < 1 || (uint64_t)value > max) {
        LogError(0, RS_RET_PARAM_ERROR,
                 "imbeats: invalid value for '%s' parameter given is %lld, valid range is 1..%zu", name,
                 (long long)value, max);
        return RS_RET_PARAM_ERROR;
    }
    *target = (size_t)value;
    return RS_RET_OK;
}

static rsRetVal setCnfString(uchar **const target, es_str_t *const value) {
    *target = (uchar *)es_str2cstr(value, NULL);
    return (*target == NULL) ? RS_RET_OUT_OF_MEMORY : RS_RET_OK;
}

static rsRetVal setCnfCString(char **const target, es_str_t *const value) {
    *target = es_str2cstr(value, NULL);
    return (*target == NULL) ? RS_RET_OUT_OF_MEMORY : RS_RET_OK;
}

static rsRetVal createPortProp(struct sockaddr_storage *addr, prop_t **const ppProp) {
    uint16_t port = 0;
    char portbuf[8];
    DEFiRet;

    if (addr->ss_family == AF_INET) {
        port = ntohs(((struct sockaddr_in *)addr)->sin_port);
    } else if (addr->ss_family == AF_INET6) {
        port = ntohs(((struct sockaddr_in6 *)addr)->sin6_port);
    }
    snprintf(portbuf, sizeof(portbuf), "%u", port);
    CHKiRet(prop.Construct(ppProp));
    CHKiRet(prop.SetString(*ppProp, (uchar *)portbuf, strlen(portbuf)));
    CHKiRet(prop.ConstructFinalize(*ppProp));

finalize_it:
    RETiRet;
}

static rsRetVal createIPProp(struct sockaddr_storage *addr, prop_t **const ppProp) {
    char ipbuf[INET6_ADDRSTRLEN];
    const void *rawAddr = NULL;
    DEFiRet;

    ipbuf[0] = '\0';
    if (addr->ss_family == AF_INET) {
        rawAddr = &((struct sockaddr_in *)addr)->sin_addr;
    } else if (addr->ss_family == AF_INET6) {
        rawAddr = &((struct sockaddr_in6 *)addr)->sin6_addr;
    }
    if (rawAddr != NULL && inet_ntop(addr->ss_family, rawAddr, ipbuf, sizeof(ipbuf)) == NULL) {
        ipbuf[0] = '\0';
    }

    CHKiRet(prop.Construct(ppProp));
    CHKiRet(prop.SetString(*ppProp, (uchar *)ipbuf, strlen(ipbuf)));
    CHKiRet(prop.ConstructFinalize(*ppProp));

finalize_it:
    RETiRet;
}

static rsRetVal addListener(void *usr, netstrm_t *pStrm) {
    instanceConf_t *const inst = (instanceConf_t *)usr;
    int sock = -1;
    netstrm_t **new_listeners = NULL;
    int *new_socks = NULL;
    DEFiRet;

    assert(inst != NULL);
    assert(pStrm != NULL);

    if (inst->listener_count == inst->listener_cap) {
        size_t new_cap = (inst->listener_cap == 0) ? 4 : inst->listener_cap * 2;
        if (new_cap < inst->listener_cap || new_cap > SIZE_MAX / sizeof(netstrm_t *) ||
            new_cap > SIZE_MAX / sizeof(int)) {
            return RS_RET_OUT_OF_MEMORY;
        }
        CHKmalloc(new_listeners = malloc(new_cap * sizeof(netstrm_t *)));
        CHKmalloc(new_socks = malloc(new_cap * sizeof(int)));
        if (inst->listener_count > 0) {
            memcpy(new_listeners, inst->listeners, inst->listener_count * sizeof(netstrm_t *));
            memcpy(new_socks, inst->listener_socks, inst->listener_count * sizeof(int));
        }
        free(inst->listeners);
        free(inst->listener_socks);
        inst->listeners = new_listeners;
        inst->listener_socks = new_socks;
        inst->listener_cap = new_cap;
        new_listeners = NULL;
        new_socks = NULL;
    }

    CHKiRet(netstrm.GetSock(pStrm, &sock));
    inst->listeners[inst->listener_count] = pStrm;
    inst->listener_socks[inst->listener_count] = sock;
    ++inst->listener_count;

finalize_it:
    free(new_listeners);
    free(new_socks);
    RETiRet;
}

static rsRetVal configureNetstrms(instanceConf_t *const inst) {
    DEFiRet;
    assert(inst != NULL);

    CHKiRet(netstrms.Construct(&inst->pNS));
    if (inst->pszStrmDrvrName != NULL) CHKiRet(netstrms.SetDrvrName(inst->pNS, inst->pszStrmDrvrName));
    CHKiRet(netstrms.SetDrvrMode(inst->pNS, inst->iStrmDrvrMode));
    CHKiRet(netstrms.SetDrvrCheckExtendedKeyUsage(inst->pNS, inst->iStrmDrvrExtendedCertCheck));
    CHKiRet(netstrms.SetDrvrPrioritizeSAN(inst->pNS, inst->iStrmDrvrSANPreference));
    CHKiRet(netstrms.SetDrvrTlsVerifyDepth(inst->pNS, inst->iStrmTlsVerifyDepth));
    CHKiRet(netstrms.SetDrvrTlsRevocationCheck(inst->pNS, inst->iStrmTlsRevocationCheck));
    if (inst->pszStrmDrvrAuthMode != NULL) CHKiRet(netstrms.SetDrvrAuthMode(inst->pNS, inst->pszStrmDrvrAuthMode));
    CHKiRet(netstrms.SetDrvrPermitExpiredCerts(inst->pNS, inst->pszStrmDrvrPermitExpiredCerts));
    CHKiRet(netstrms.SetDrvrTlsCAFile(inst->pNS, inst->pszStrmDrvrCAFile));
    CHKiRet(netstrms.SetDrvrTlsCRLFile(inst->pNS, inst->pszStrmDrvrCRLFile));
    CHKiRet(netstrms.SetDrvrTlsKeyFile(inst->pNS, inst->pszStrmDrvrKeyFile));
    CHKiRet(netstrms.SetDrvrTlsCertFile(inst->pNS, inst->pszStrmDrvrCertFile));
    if (inst->pPermPeersRoot != NULL) CHKiRet(netstrms.SetDrvrPermPeers(inst->pNS, inst->pPermPeersRoot));
    if (inst->gnutlsPriorityString != NULL)
        CHKiRet(netstrms.SetDrvrGnutlsPriorityString(inst->pNS, inst->gnutlsPriorityString));
    CHKiRet(netstrms.ConstructFinalize(inst->pNS));

finalize_it:
    RETiRet;
}

static rsRetVal buildListeners(instanceConf_t *inst) {
    tcpLstnParams_t params;
    size_t i;
    DEFiRet;

    memset(&params, 0, sizeof(params));
    params.pszPort = inst->port;
    params.pszAddr = inst->address;
    params.pszLstnPortFileName = inst->pszLstnPortFileName;
    params.pszNetworkNamespace = inst->pszNetworkNamespace;

    CHKiRet(configureNetstrms(inst));
    CHKiRet(netstrm.LstnInit(inst->pNS, inst, addListener, 0, &params));
    if (inst->listener_count > 0) {
        CHKmalloc(inst->listener_descs = calloc(inst->listener_count, sizeof(*inst->listener_descs)));
        for (i = 0; i < inst->listener_count; ++i) {
            inst->listener_descs[i].type = IMBEATS_IO_LISTENER;
            inst->listener_descs[i].inst = inst;
            inst->listener_descs[i].listener_idx = i;
        }
    }

finalize_it:
    RETiRet;
}

static rsRetVal submitEvent(session_t *const sess, const struct lj_event_s *const event) {
    smsg_t *pMsg = NULL;
    struct json_tokener *tok = NULL;
    struct fjson_object *json = NULL;
    struct fjson_object *meta = NULL;
    rsRetVal iRet = RS_RET_OK;
    char portbuf[16];

    assert(sess != NULL);
    assert(event != NULL);

    CHKmalloc(tok = json_tokener_new());
    json = json_tokener_parse_ex(tok, (const char *)event->payload, event->payload_len);
    if (tok->err != fjson_tokener_success || json == NULL || !fjson_object_is_type(json, fjson_type_object)) {
        STATSCOUNTER_INC(statsCounter.ctrJsonDecodeFailures, statsCounter.mutCtrJsonDecodeFailures);
        ABORT_FINALIZE(RS_RET_INVALID_VALUE);
    }

    CHKiRet(msgConstruct(&pMsg));
    MsgSetFlowControlType(pMsg, eFLOWCTL_LIGHT_DELAY);
    MsgSetInputName(pMsg, (sess->inst->pInputName != NULL) ? sess->inst->pInputName : pInputName);
    MsgSetRawMsg(pMsg, (const char *)event->payload, event->payload_len);
    MsgSetMSGoffs(pMsg, 0);
    if (sess->fromHostFQDN != NULL) {
        if (sess->fromHost != NULL) {
            MsgSetRcvFrom(pMsg, sess->fromHost);
        }
        MsgSetHOSTNAME(pMsg, sess->fromHostFQDN, ustrlen(sess->fromHostFQDN));
    }
    if (sess->fromHostIP != NULL) {
        CHKiRet(MsgSetRcvFromIP(pMsg, sess->fromHostIP));
    }
    if (sess->fromHostPort != NULL) {
        CHKiRet(MsgSetRcvFromPort(pMsg, sess->fromHostPort));
    }
    if (sess->inst->pBindRuleset != NULL) {
        MsgSetRuleset(pMsg, sess->inst->pBindRuleset);
    }

    /* TODO: representation mode is intentionally Elasticsearch-oriented for v1.
     * Revisit later whether this should be configurable (for example $! vs $!beats).
     */
    iRet = msgAddJSON(pMsg, (uchar *)"!", json, 0, 0);
    json = NULL;
    CHKiRet(iRet);

    CHKmalloc(meta = json_object_new_object());
    fjson_object_object_add(meta, "protocol", json_object_new_string("lumberjack-v2"));
    fjson_object_object_add(meta, "sequence", json_object_new_int64(event->seq));
    fjson_object_object_add(meta, "tls_enabled", json_object_new_boolean(sess->inst->iStrmDrvrMode != 0));
    if (sess->fromHostFQDN != NULL) {
        fjson_object_object_add(meta, "peer_hostname", json_object_new_string((char *)sess->fromHostFQDN));
    }
    if (sess->fromHostIPStr != NULL) {
        fjson_object_object_add(meta, "peer_ip", json_object_new_string(sess->fromHostIPStr));
    }
    if (sess->fromHostPortStr != NULL) {
        snprintf(portbuf, sizeof(portbuf), "%s", sess->fromHostPortStr);
        fjson_object_object_add(meta, "peer_port", json_object_new_string(portbuf));
    }
    iRet = msgAddJSON(pMsg, (uchar *)"!metadata!imbeats", meta, 1, 0);
    meta = NULL;
    CHKiRet(iRet);

    CHKiRet(submitMsg2(pMsg));
    STATSCOUNTER_INC(statsCounter.ctrEventsSubmitted, statsCounter.mutCtrEventsSubmitted);

finalize_it:
    if (iRet != RS_RET_OK) {
        STATSCOUNTER_INC(statsCounter.ctrEventsFailed, statsCounter.mutCtrEventsFailed);
        if (pMsg != NULL) {
            msgDestruct(&pMsg);
        }
    }
    if (tok != NULL) {
        json_tokener_free(tok);
    }
    if (json != NULL) {
        fjson_object_put(json);
    }
    if (meta != NULL) {
        fjson_object_put(meta);
    }
    RETiRet;
}

static rsRetVal sessionReadStep(session_t *const sess, unsigned char *const buf, const size_t len) {
    int oserr = 0;
    rsRetVal iRet;

    assert(sess != NULL);
    assert(buf != NULL);
    if (sess->io_buf == NULL) {
        sess->io_buf = buf;
        sess->io_need = len;
        sess->io_off = 0;
    }
    while (sess->io_off < sess->io_need) {
        ssize_t need = (ssize_t)(sess->io_need - sess->io_off);
        unsigned nextIODirection = NSDSEL_RD;
        iRet = netstrm.Rcv(sess->pStrm, sess->io_buf + sess->io_off, &need, &oserr, &nextIODirection);
        if (iRet == RS_RET_OK) {
            if (need <= 0) {
                sess->ioDirection = NSDSEL_RD;
                return RS_RET_RETRY;
            }
            sess->io_off += (size_t)need;
        } else if (iRet == RS_RET_RETRY) {
            sess->ioDirection = nextIODirection;
            return RS_RET_RETRY;
        } else {
            return iRet;
        }
    }
    sess->io_buf = NULL;
    sess->io_need = 0;
    sess->io_off = 0;
    return RS_RET_OK;
}

static rsRetVal sessionValidateBatch(session_t *const sess) {
    size_t idx;
    assert(sess != NULL);
    for (idx = 0; idx < sess->batch.count; ++idx) {
        const uint32_t expected = sess->lastAckedSeq + (uint32_t)idx + 1;
        if (sess->batch.events[idx].seq != expected) {
            return RS_RET_INVALID_VALUE;
        }
    }
    STATSCOUNTER_ADD(statsCounter.ctrEventsReceived, statsCounter.mutCtrEventsReceived, sess->batch.count);
    return RS_RET_OK;
}

static void sessionPrepareAck(session_t *const sess) {
    uint32_t netseq;
    assert(sess != NULL);
    assert(sess->batch.count > 0);
    sess->pendingSeq = sess->batch.events[sess->batch.count - 1].seq;
    netseq = htonl(sess->pendingSeq);
    sess->ack[0] = LJ_VERSION_V2;
    sess->ack[1] = LJ_FRAME_ACK;
    memcpy(sess->ack + 2, &netseq, sizeof(netseq));
    sess->ack_off = 0;
    sess->state = IMBEATS_ST_SEND_ACK;
}

static void sessionResetForNextBatch(session_t *const sess) {
    assert(sess != NULL);
    lj_batch_free(&sess->batch);
    sess->batchActive = 0;
    sess->submitIdx = 0;
    sess->pendingSeq = 0;
    sess->payload_len = 0;
    sess->ack_off = 0;
    sess->state = IMBEATS_ST_READ_WINDOW_HDR;
}

static rsRetVal sessionSendAckStep(session_t *const sess) {
    rsRetVal iRet;
    assert(sess != NULL);
    while (sess->ack_off < sizeof(sess->ack)) {
        ssize_t to_send = (ssize_t)(sizeof(sess->ack) - sess->ack_off);
        iRet = netstrm.Send(sess->pStrm, sess->ack + sess->ack_off, &to_send);
        if (iRet != RS_RET_OK) {
            STATSCOUNTER_INC(statsCounter.ctrAckFailures, statsCounter.mutCtrAckFailures);
            return iRet;
        }
        if (to_send == 0) {
            sess->ioDirection = NSDSEL_WR;
            return RS_RET_RETRY;
        }
        sess->ack_off += (size_t)to_send;
    }
    sess->lastAckedSeq = sess->pendingSeq;
    STATSCOUNTER_INC(statsCounter.ctrBatchesAcked, statsCounter.mutCtrBatchesAcked);
    sessionResetForNextBatch(sess);
    return RS_RET_OK;
}

static rsRetVal sessionProcess(session_t *const sess) {
    uint32_t net32;
    unsigned reads = 0;
    DEFiRet;

    assert(sess != NULL);
    while (glbl.GetGlobalInputTermState() == 0 && !instanceIsShuttingDown(sess->inst)) {
        if (sess->inst->starvationMaxReads != 0 && reads >= sess->inst->starvationMaxReads) {
            STATSCOUNTER_INC(statsCounter.ctrStarvationProtect, statsCounter.mutCtrStarvationProtect);
            return RS_RET_OK;
        }

        switch (sess->state) {
            case IMBEATS_ST_READ_WINDOW_HDR:
                iRet = sessionReadStep(sess, sess->fixed, 2);
                if (iRet == RS_RET_RETRY) return iRet;
                CHKiRet(iRet);
                ++reads;
                if (sess->fixed[0] != LJ_VERSION_V2 || sess->fixed[1] != LJ_FRAME_WINDOW) {
                    ABORT_FINALIZE(RS_RET_INVALID_VALUE);
                }
                sess->state = IMBEATS_ST_READ_WINDOW_SIZE;
                break;
            case IMBEATS_ST_READ_WINDOW_SIZE:
                iRet = sessionReadStep(sess, sess->fixed, 4);
                if (iRet == RS_RET_RETRY) return iRet;
                CHKiRet(iRet);
                ++reads;
                memcpy(&net32, sess->fixed, sizeof(net32));
                net32 = ntohl(net32);
                CHKiRet(lj_parse_window_header((const unsigned char[]){LJ_VERSION_V2, LJ_FRAME_WINDOW}, net32));
                if (net32 > sess->inst->maxWindowSize) {
                    STATSCOUNTER_INC(statsCounter.ctrWindowsRejected, statsCounter.mutCtrWindowsRejected);
                    ABORT_FINALIZE(RS_RET_INVALID_VALUE);
                }
                CHKiRet(lj_batch_alloc(&sess->batch, net32, sess->inst->maxWindowSize, sess->inst->maxBatchBytes));
                sess->batchActive = 1;
                STATSCOUNTER_INC(statsCounter.ctrBatchesReceived, statsCounter.mutCtrBatchesReceived);
                sess->state = IMBEATS_ST_READ_FRAME_HDR;
                break;
            case IMBEATS_ST_READ_FRAME_HDR:
                iRet = sessionReadStep(sess, sess->fixed, 2);
                if (iRet == RS_RET_RETRY) return iRet;
                CHKiRet(iRet);
                ++reads;
                if (sess->fixed[0] != LJ_VERSION_V2) {
                    ABORT_FINALIZE(RS_RET_INVALID_VALUE);
                }
                if (sess->fixed[1] == LJ_FRAME_JSON) {
                    sess->state = IMBEATS_ST_READ_JSON_SEQ;
                } else if (sess->fixed[1] == LJ_FRAME_COMPRESSED) {
                    STATSCOUNTER_INC(statsCounter.ctrCompressedFrames, statsCounter.mutCtrCompressedFrames);
                    sess->state = IMBEATS_ST_READ_COMP_LEN;
                } else {
                    ABORT_FINALIZE(RS_RET_INVALID_VALUE);
                }
                break;
            case IMBEATS_ST_READ_JSON_SEQ:
                iRet = sessionReadStep(sess, sess->fixed, 4);
                if (iRet == RS_RET_RETRY) return iRet;
                CHKiRet(iRet);
                ++reads;
                memcpy(&net32, sess->fixed, sizeof(net32));
                sess->pendingSeq = ntohl(net32);
                sess->state = IMBEATS_ST_READ_JSON_LEN;
                break;
            case IMBEATS_ST_READ_JSON_LEN:
                iRet = sessionReadStep(sess, sess->fixed, 4);
                if (iRet == RS_RET_RETRY) return iRet;
                CHKiRet(iRet);
                ++reads;
                memcpy(&net32, sess->fixed, sizeof(net32));
                sess->payload_len = ntohl(net32);
                if (sess->payload_len == 0 || sess->payload_len > sess->inst->maxFrameSize) {
                    STATSCOUNTER_INC(statsCounter.ctrFramesRejected, statsCounter.mutCtrFramesRejected);
                    ABORT_FINALIZE(RS_RET_INVALID_VALUE);
                }
                CHKmalloc(sess->payload = malloc(sess->payload_len));
                sess->state = IMBEATS_ST_READ_JSON_PAYLOAD;
                break;
            case IMBEATS_ST_READ_JSON_PAYLOAD:
                iRet = sessionReadStep(sess, sess->payload, sess->payload_len);
                if (iRet == RS_RET_RETRY) return iRet;
                CHKiRet(iRet);
                ++reads;
                iRet = lj_append_json_event(&sess->batch, sess->pendingSeq, sess->payload, sess->payload_len);
                free(sess->payload);
                sess->payload = NULL;
                sess->payload_len = 0;
                if (iRet == RS_RET_INVALID_VALUE) {
                    STATSCOUNTER_INC(statsCounter.ctrFramesRejected, statsCounter.mutCtrFramesRejected);
                }
                CHKiRet(iRet);
                sess->state = (sess->batch.count == sess->batch.window_size) ? IMBEATS_ST_SUBMIT_EVENTS
                                                                             : IMBEATS_ST_READ_FRAME_HDR;
                if (sess->state == IMBEATS_ST_SUBMIT_EVENTS) CHKiRet(sessionValidateBatch(sess));
                break;
            case IMBEATS_ST_READ_COMP_LEN:
                iRet = sessionReadStep(sess, sess->fixed, 4);
                if (iRet == RS_RET_RETRY) return iRet;
                CHKiRet(iRet);
                ++reads;
                memcpy(&net32, sess->fixed, sizeof(net32));
                sess->payload_len = ntohl(net32);
                if (sess->payload_len == 0 || sess->payload_len > sess->inst->maxFrameSize) {
                    STATSCOUNTER_INC(statsCounter.ctrFramesRejected, statsCounter.mutCtrFramesRejected);
                    STATSCOUNTER_INC(statsCounter.ctrCompressedRejected, statsCounter.mutCtrCompressedRejected);
                    ABORT_FINALIZE(RS_RET_INVALID_VALUE);
                }
                CHKmalloc(sess->payload = malloc(sess->payload_len));
                sess->state = IMBEATS_ST_READ_COMP_PAYLOAD;
                break;
            case IMBEATS_ST_READ_COMP_PAYLOAD:
                iRet = sessionReadStep(sess, sess->payload, sess->payload_len);
                if (iRet == RS_RET_RETRY) return iRet;
                CHKiRet(iRet);
                ++reads;
                iRet = lj_parse_compressed_frames(&sess->batch, sess->payload, sess->payload_len,
                                                  sess->inst->maxFrameSize, sess->inst->maxDecompressedSize);
                free(sess->payload);
                sess->payload = NULL;
                sess->payload_len = 0;
                if (iRet == RS_RET_INVALID_VALUE) {
                    STATSCOUNTER_INC(statsCounter.ctrCompressedRejected, statsCounter.mutCtrCompressedRejected);
                }
                CHKiRet(iRet);
                sess->state = (sess->batch.count == sess->batch.window_size) ? IMBEATS_ST_SUBMIT_EVENTS
                                                                             : IMBEATS_ST_READ_FRAME_HDR;
                if (sess->state == IMBEATS_ST_SUBMIT_EVENTS) CHKiRet(sessionValidateBatch(sess));
                break;
            case IMBEATS_ST_SUBMIT_EVENTS:
                while (sess->submitIdx < sess->batch.count) {
                    CHKiRet(submitEvent(sess, &sess->batch.events[sess->submitIdx]));
                    ++sess->submitIdx;
                    if (sess->inst->starvationMaxReads != 0 && ++reads >= sess->inst->starvationMaxReads) {
                        STATSCOUNTER_INC(statsCounter.ctrStarvationProtect, statsCounter.mutCtrStarvationProtect);
                        return RS_RET_OK;
                    }
                }
                sessionPrepareAck(sess);
                break;
            case IMBEATS_ST_SEND_ACK:
                iRet = sessionSendAckStep(sess);
                if (iRet == RS_RET_RETRY) return iRet;
                CHKiRet(iRet);
                ++reads;
                break;
            default:
                ABORT_FINALIZE(RS_RET_INVALID_VALUE);
        }
    }
    ABORT_FINALIZE(RS_RET_CLOSED);

finalize_it:
    if (iRet == RS_RET_INVALID_VALUE) {
        STATSCOUNTER_INC(statsCounter.ctrProtocolErrors, statsCounter.mutCtrProtocolErrors);
    }
    RETiRet;
}

static void unlinkSession(instanceConf_t *const inst, session_t *const target) {
    session_t **pp;
    assert(inst != NULL);
    assert(target != NULL);
    for (pp = &inst->sessions; *pp != NULL; pp = &(*pp)->next) {
        if (*pp == target) {
            *pp = target->next;
            target->next = NULL;
            break;
        }
    }
}

static void shutdownSessionSocket(session_t *const sess) {
    if (sess != NULL && sess->sock >= 0) {
        (void)shutdown(sess->sock, SHUT_RDWR);
    }
}

static void shutdownAllSessionSockets(instanceConf_t *const inst) {
    session_t *sess;

    pthread_mutex_lock(&inst->mutSessions);
    for (sess = inst->sessions; sess != NULL; sess = sess->next) {
        shutdownSessionSocket(sess);
    }
    pthread_mutex_unlock(&inst->mutSessions);
}

static void destroyAllSessions(instanceConf_t *const inst) {
    session_t *sess;

    pthread_mutex_lock(&inst->mutSessions);
    sess = inst->sessions;
    inst->sessions = NULL;
    inst->activeSessions = 0;
    pthread_mutex_unlock(&inst->mutSessions);

    while (sess != NULL) {
        session_t *const next = sess->next;
        sess->next = NULL;
        shutdownSessionSocket(sess);
        destroySession(sess);
        sess = next;
    }
}

static void destroySession(session_t *sess) {
    if (sess == NULL) {
        return;
    }
    free(sess->payload);
    if (sess->batchActive) {
        lj_batch_free(&sess->batch);
    }
    if (sess->pStrm != NULL) {
        netstrm.Destruct(&sess->pStrm);
    }
    if (sess->fromHostIP != NULL) {
        prop.Destruct(&sess->fromHostIP);
    }
    if (sess->fromHost != NULL) {
        prop.Destruct(&sess->fromHost);
    }
    if (sess->fromHostPort != NULL) {
        prop.Destruct(&sess->fromHostPort);
    }
    free(sess->fromHostFQDN);
    free(sess->fromHostIPStr);
    free(sess->fromHostPortStr);
    free(sess->io);
    free(sess);
}

static void closeSession(session_t *const sess) {
    instanceConf_t *const inst = sess->inst;
#if defined(IMBEATS_HAVE_EPOLL)
    if (inst->epoll_fd >= 0 && sess->sock >= 0) {
        (void)epoll_ctl(inst->epoll_fd, EPOLL_CTL_DEL, sess->sock, NULL);
    }
#endif
    STATSCOUNTER_INC(statsCounter.ctrConnectionsClosed, statsCounter.mutCtrConnectionsClosed);
    pthread_mutex_lock(&inst->mutSessions);
    if (!sess->done) {
        sess->done = 1;
        if (inst->activeSessions > 0) {
            --inst->activeSessions;
            STATSCOUNTER_DEC(statsCounter.ctrConnectionsActive, statsCounter.mutCtrConnectionsActive);
        }
        unlinkSession(inst, sess);
    }
    pthread_mutex_unlock(&inst->mutSessions);
    destroySession(sess);
}

static void enqueueSession(session_t *const sess) {
    instanceConf_t *const inst = sess->inst;

    pthread_mutex_lock(&inst->mutWork);
    if (!sess->done && !sess->inWorkQueue) {
        sess->workNext = NULL;
        if (inst->workTail == NULL) {
            inst->workHead = sess;
        } else {
            inst->workTail->workNext = sess;
        }
        inst->workTail = sess;
        sess->inWorkQueue = 1;
        pthread_cond_signal(&inst->condWork);
    }
    pthread_mutex_unlock(&inst->mutWork);
}

static session_t *dequeueSession(instanceConf_t *const inst) {
    session_t *sess;

    pthread_mutex_lock(&inst->mutWork);
    while (inst->workHead == NULL && !inst->workShutdown && glbl.GetGlobalInputTermState() == 0) {
        pthread_cond_wait(&inst->condWork, &inst->mutWork);
    }
    sess = inst->workHead;
    if (sess != NULL) {
        inst->workHead = sess->workNext;
        if (inst->workHead == NULL) {
            inst->workTail = NULL;
        }
        sess->workNext = NULL;
        sess->inWorkQueue = 0;
    }
    pthread_mutex_unlock(&inst->mutWork);
    return sess;
}

static rsRetVal rearmSession(session_t *const sess) {
#if defined(IMBEATS_HAVE_EPOLL)
    struct epoll_event ev;
    const uint32_t readiness = (sess->ioDirection == NSDSEL_WR) ? EPOLLOUT : EPOLLIN;

    memset(&ev, 0, sizeof(ev));
    ev.events = readiness | EPOLLONESHOT;
    ev.data.ptr = sess->io;
    if (epoll_ctl(sess->inst->epoll_fd, EPOLL_CTL_MOD, sess->sock, &ev) < 0) {
        LogError(errno, RS_RET_ERR_EPOLL_CTL, "imbeats: epoll_ctl MOD failed on session socket %d", sess->sock);
        return RS_RET_ERR_EPOLL_CTL;
    }
    return RS_RET_OK;
#else
    srSleep(0, 100000);
    enqueueSession(sess);
    return RS_RET_OK;
#endif
}

static void *workerThread(void *arg) {
    instanceConf_t *const inst = (instanceConf_t *)arg;
    session_t *sess;

    assert(inst != NULL);
    while ((sess = dequeueSession(inst)) != NULL) {
        const rsRetVal iRet = sessionProcess(sess);
        if (iRet == RS_RET_RETRY) {
            if (rearmSession(sess) != RS_RET_OK) {
                closeSession(sess);
            }
        } else if (iRet == RS_RET_OK) {
            enqueueSession(sess);
        } else {
            closeSession(sess);
        }
    }
    return NULL;
}

static rsRetVal spawnSession(instanceConf_t *const inst, netstrm_t **const ppNewStrm) {
    session_t *sess = NULL;
    struct sockaddr_storage *addr = NULL;
    int reservedSession = 0;
#if defined(IMBEATS_HAVE_EPOLL)
    struct epoll_event ev;
#endif
    DEFiRet;

    assert(ppNewStrm != NULL);
    assert(*ppNewStrm != NULL);

    if (ppNewStrm == NULL || *ppNewStrm == NULL) {
        return RS_RET_INVALID_VALUE;
    }

    pthread_mutex_lock(&inst->mutSessions);
    if (inst->shuttingDown) {
        pthread_mutex_unlock(&inst->mutSessions);
        ABORT_FINALIZE(RS_RET_FORCE_TERM);
    }
    if (inst->activeSessions >= inst->maxSessions) {
        pthread_mutex_unlock(&inst->mutSessions);
        STATSCOUNTER_INC(statsCounter.ctrConnectionsRejected, statsCounter.mutCtrConnectionsRejected);
        LogError(0, RS_RET_MAX_SESS_REACHED, "imbeats: too many sessions - dropping incoming request");
        ABORT_FINALIZE(RS_RET_MAX_SESS_REACHED);
    }
    ++inst->activeSessions;
    reservedSession = 1;
    STATSCOUNTER_INC(statsCounter.ctrConnectionsActive, statsCounter.mutCtrConnectionsActive);
    pthread_mutex_unlock(&inst->mutSessions);

    CHKmalloc(sess = calloc(1, sizeof(*sess)));
    sess->sock = -1;
    sess->inst = inst;
    sess->pStrm = *ppNewStrm;
    *ppNewStrm = NULL;
    sess->ioDirection = NSDSEL_RD;
    sess->state = IMBEATS_ST_READ_WINDOW_HDR;
    CHKiRet(netstrm.GetSock(sess->pStrm, &sess->sock));
    if (inst->bKeepAlive) {
        CHKiRet(netstrm.SetKeepAliveProbes(sess->pStrm, inst->iKeepAliveProbes));
        CHKiRet(netstrm.SetKeepAliveTime(sess->pStrm, inst->iKeepAliveTime));
        CHKiRet(netstrm.SetKeepAliveIntvl(sess->pStrm, inst->iKeepAliveIntvl));
        CHKiRet(netstrm.EnableKeepAlive(sess->pStrm));
    }
    if (inst->gnutlsPriorityString != NULL) {
        CHKiRet(netstrm.SetGnutlsPriorityString(sess->pStrm, inst->gnutlsPriorityString));
    }
    CHKiRet(netstrm.GetRemoteHName(sess->pStrm, &sess->fromHostFQDN));
    if (sess->fromHostFQDN != NULL) {
        CHKiRet(prop.Construct(&sess->fromHost));
        CHKiRet(prop.SetString(sess->fromHost, sess->fromHostFQDN, ustrlen(sess->fromHostFQDN)));
        CHKiRet(prop.ConstructFinalize(sess->fromHost));
    }
    CHKiRet(netstrm.GetRemAddr(sess->pStrm, &addr));
    CHKiRet(createIPProp(addr, &sess->fromHostIP));
    CHKmalloc(sess->fromHostIPStr = strdup(propGetSzStrOrDefault(sess->fromHostIP, "")));
    CHKiRet(createPortProp(addr, &sess->fromHostPort));
    CHKmalloc(sess->fromHostPortStr = strdup(propGetSzStrOrDefault(sess->fromHostPort, "")));
    CHKmalloc(sess->io = calloc(1, sizeof(*sess->io)));
    sess->io->type = IMBEATS_IO_SESSION;
    sess->io->inst = inst;
    sess->io->sess = sess;

    pthread_mutex_lock(&inst->mutSessions);
    if (inst->shuttingDown || sess->done) {
        pthread_mutex_unlock(&inst->mutSessions);
        ABORT_FINALIZE(RS_RET_FORCE_TERM);
    }
    sess->next = inst->sessions;
    inst->sessions = sess;
    pthread_mutex_unlock(&inst->mutSessions);

#if defined(IMBEATS_HAVE_EPOLL)
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN | EPOLLONESHOT;
    ev.data.ptr = sess->io;
    if (epoll_ctl(inst->epoll_fd, EPOLL_CTL_ADD, sess->sock, &ev) < 0) {
        LogError(errno, RS_RET_ERR_EPOLL_CTL, "imbeats: epoll_ctl ADD failed on session socket %d", sess->sock);
        ABORT_FINALIZE(RS_RET_ERR_EPOLL_CTL);
    }
#else
    enqueueSession(sess);
#endif
    STATSCOUNTER_INC(statsCounter.ctrConnectionsAccepted, statsCounter.mutCtrConnectionsAccepted);
    reservedSession = 0;
    sess = NULL;

finalize_it:
    if (iRet != RS_RET_OK && reservedSession) {
        pthread_mutex_lock(&inst->mutSessions);
        if (inst->activeSessions > 0) {
            --inst->activeSessions;
            STATSCOUNTER_DEC(statsCounter.ctrConnectionsActive, statsCounter.mutCtrConnectionsActive);
        }
        if (sess != NULL) {
            unlinkSession(inst, sess);
        }
        pthread_mutex_unlock(&inst->mutSessions);
    }
    if (iRet != RS_RET_OK && sess != NULL) {
        destroySession(sess);
    }
    RETiRet;
}

static rsRetVal startWorkerPool(instanceConf_t *const inst) {
    unsigned i;
    DEFiRet;

    assert(inst != NULL);
    CHKmalloc(inst->worker_tids = calloc(inst->workerThreads, sizeof(pthread_t)));
    for (i = 0; i < inst->workerThreads; ++i) {
        if (pthread_create(&inst->worker_tids[i], NULL, workerThread, inst) != 0) {
            ABORT_FINALIZE(RS_RET_IO_ERROR);
        }
        ++inst->workersStarted;
    }

finalize_it:
    RETiRet;
}

static void stopWorkerPool(instanceConf_t *const inst) {
    unsigned i;

    pthread_mutex_lock(&inst->mutWork);
    inst->workShutdown = 1;
    pthread_cond_broadcast(&inst->condWork);
    pthread_mutex_unlock(&inst->mutWork);
    for (i = 0; i < inst->workersStarted; ++i) {
        pthread_join(inst->worker_tids[i], NULL);
    }
    free(inst->worker_tids);
    inst->worker_tids = NULL;
    inst->workersStarted = 0;
}

static rsRetVal acceptReadyListener(instanceConf_t *const inst, const size_t idx) {
    unsigned accepted = 0;
    DEFiRet;

    while (accepted < IMBEATS_ACCEPT_BATCH_LIMIT && !instanceIsShuttingDown(inst) &&
           glbl.GetGlobalInputTermState() == 0) {
        netstrm_t *pNewStrm = NULL;
        char connInfo[TCPSRV_CONNINFO_SIZE];

        memset(connInfo, 0, sizeof(connInfo));
        iRet = netstrm.AcceptConnReq(inst->listeners[idx], &pNewStrm, connInfo);
        if (iRet == RS_RET_NO_MORE_DATA) {
            iRet = RS_RET_OK;
            break;
        }
        if (iRet == RS_RET_OK) {
            ++accepted;
            iRet = spawnSession(inst, &pNewStrm);
            if (iRet != RS_RET_OK && pNewStrm != NULL) {
                netstrm.Destruct(&pNewStrm);
            }
            if (iRet == RS_RET_MAX_SESS_REACHED) {
                iRet = RS_RET_OK;
            }
            CHKiRet(iRet);
        } else {
            break;
        }
    }

finalize_it:
    RETiRet;
}

#if defined(IMBEATS_HAVE_EPOLL)
static rsRetVal addEpollListener(instanceConf_t *const inst, const size_t idx) {
    struct epoll_event ev;
    DEFiRet;

    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN | EPOLLONESHOT;
    ev.data.ptr = &inst->listener_descs[idx];
    if (epoll_ctl(inst->epoll_fd, EPOLL_CTL_ADD, inst->listener_socks[idx], &ev) < 0) {
        LogError(errno, RS_RET_ERR_EPOLL_CTL, "imbeats: epoll_ctl ADD failed on listener socket %d",
                 inst->listener_socks[idx]);
        ABORT_FINALIZE(RS_RET_ERR_EPOLL_CTL);
    }

finalize_it:
    RETiRet;
}

static rsRetVal rearmListener(instanceConf_t *const inst, const size_t idx) {
    struct epoll_event ev;
    DEFiRet;

    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN | EPOLLONESHOT;
    ev.data.ptr = &inst->listener_descs[idx];
    if (epoll_ctl(inst->epoll_fd, EPOLL_CTL_MOD, inst->listener_socks[idx], &ev) < 0) {
        LogError(errno, RS_RET_ERR_EPOLL_CTL, "imbeats: epoll_ctl MOD failed on listener socket %d",
                 inst->listener_socks[idx]);
        ABORT_FINALIZE(RS_RET_ERR_EPOLL_CTL);
    }

finalize_it:
    RETiRet;
}
#endif

static void *listenerThread(void *arg) {
    instanceConf_t *const inst = (instanceConf_t *)arg;
    size_t i;

    assert(inst != NULL);
    if (startWorkerPool(inst) != RS_RET_OK) {
        setInstanceShuttingDown(inst);
        stopWorkerPool(inst);
        return NULL;
    }

#if defined(IMBEATS_HAVE_EPOLL)
    inst->epoll_fd = epoll_create(100);
    if (inst->epoll_fd < 0) {
        LogError(errno, RS_RET_EPOLL_CR_FAILED, "imbeats: epoll_create failed");
        setInstanceShuttingDown(inst);
        stopWorkerPool(inst);
        return NULL;
    }
    for (i = 0; i < inst->listener_count; ++i) {
        if (addEpollListener(inst, i) != RS_RET_OK) {
            setInstanceShuttingDown(inst);
            break;
        }
    }

    while (glbl.GetGlobalInputTermState() == 0 && !instanceIsShuttingDown(inst)) {
        struct epoll_event events[64];
        const int nfds = epoll_wait(inst->epoll_fd, events, sizeof(events) / sizeof(events[0]), 1000);
        int n;

        if (nfds < 0 && errno == EINTR) {
            continue;
        }
        if (nfds < 0) {
            LogError(errno, RS_RET_ERR_EPOLL, "imbeats: epoll_wait failed");
            continue;
        }
        for (n = 0; n < nfds && !instanceIsShuttingDown(inst); ++n) {
            imbeats_io_t *const io = (imbeats_io_t *)events[n].data.ptr;
            if (io == NULL) {
                continue;
            }
            if (io->type == IMBEATS_IO_LISTENER) {
                if (acceptReadyListener(inst, io->listener_idx) == RS_RET_OK && !instanceIsShuttingDown(inst)) {
                    (void)rearmListener(inst, io->listener_idx);
                }
            } else if (io->type == IMBEATS_IO_SESSION && io->sess != NULL) {
                if ((events[n].events & (EPOLLERR | EPOLLHUP)) != 0) {
                    closeSession(io->sess);
                } else {
                    enqueueSession(io->sess);
                }
            }
        }
    }
    close(inst->epoll_fd);
    inst->epoll_fd = -1;
#else
    while (glbl.GetGlobalInputTermState() == 0 && !instanceIsShuttingDown(inst)) {
        struct pollfd *pfds = calloc(inst->listener_count, sizeof(struct pollfd));
        if (pfds == NULL) {
            break;
        }
        for (i = 0; i < inst->listener_count; ++i) {
            pfds[i].fd = inst->listener_socks[i];
            pfds[i].events = POLLIN;
        }
        if (poll(pfds, inst->listener_count, 1000) > 0) {
            for (i = 0; i < inst->listener_count; ++i) {
                if ((pfds[i].revents & POLLIN) != 0) {
                    (void)acceptReadyListener(inst, i);
                }
            }
        }
        free(pfds);
    }
#endif
    stopWorkerPool(inst);
    return NULL;
}

static rsRetVal destroyInstanceRuntime(instanceConf_t *inst) {
    size_t i;

    setInstanceShuttingDown(inst);
    shutdownAllSessionSockets(inst);
    if (inst->listener_running) {
        pthread_join(inst->listener_tid, NULL);
        inst->listener_running = 0;
    }
    destroyAllSessions(inst);

    for (i = 0; i < inst->listener_count; ++i) {
        if (inst->listeners[i] != NULL) {
            netstrm.Destruct(&inst->listeners[i]);
        }
    }
    free(inst->listeners);
    inst->listeners = NULL;
    free(inst->listener_socks);
    inst->listener_socks = NULL;
    free(inst->listener_descs);
    inst->listener_descs = NULL;
    inst->listener_count = 0;
    inst->listener_cap = 0;
    if (inst->pNS != NULL) {
        netstrms.Destruct(&inst->pNS);
    }
    return RS_RET_OK;
}

static rsRetVal createInstance(instanceConf_t **const pinst) {
    instanceConf_t *inst;
    DEFiRet;

    CHKmalloc(inst = calloc(1, sizeof(*inst)));
    pthread_mutex_init(&inst->mutSessions, NULL);
    pthread_mutex_init(&inst->mutWork, NULL);
    pthread_cond_init(&inst->condWork, NULL);
    inst->iStrmDrvrMode = 0;
    inst->maxWindowSize = IMBEATS_DEFAULT_MAX_WINDOW_SIZE;
    inst->maxFrameSize = IMBEATS_DEFAULT_MAX_FRAME_SIZE;
    inst->maxDecompressedSize = IMBEATS_DEFAULT_MAX_DECOMPRESSED_SIZE;
    inst->maxBatchBytes = IMBEATS_DEFAULT_MAX_BATCH_BYTES;
    inst->maxSessions = IMBEATS_DEFAULT_MAX_SESSIONS;
    inst->workerThreads = IMBEATS_DEFAULT_WORKER_THREADS;
    inst->starvationMaxReads = IMBEATS_DEFAULT_STARVATION_MAX_READS;
#if defined(IMBEATS_HAVE_EPOLL)
    inst->epoll_fd = -1;
#endif
    if (loadModConf->tail == NULL) {
        loadModConf->root = loadModConf->tail = inst;
    } else {
        loadModConf->tail->next = inst;
        loadModConf->tail = inst;
    }
    *pinst = inst;

finalize_it:
    RETiRet;
}

BEGINnewInpInst
    struct cnfparamvals *pvals;
    instanceConf_t *inst = NULL;
    int i;
    CODESTARTnewInpInst;
    pvals = nvlstGetParams(lst, &inppblk, NULL);
    if (pvals == NULL) {
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }
    CHKiRet(createInstance(&inst));
    for (i = 0; i < inppblk.nParams; ++i) {
        int j;
        if (!pvals[i].bUsed) continue;
        if (!strcmp(inppblk.descr[i].name, "port")) {
            CHKiRet(setCnfString(&inst->port, pvals[i].val.d.estr));
        } else if (!strcmp(inppblk.descr[i].name, "address")) {
            CHKiRet(setCnfString(&inst->address, pvals[i].val.d.estr));
        } else if (!strcmp(inppblk.descr[i].name, "ruleset")) {
            CHKiRet(setCnfString(&inst->pszBindRuleset, pvals[i].val.d.estr));
        } else if (!strcmp(inppblk.descr[i].name, "name")) {
            CHKiRet(setCnfString(&inst->pszInputName, pvals[i].val.d.estr));
        } else if (!strcmp(inppblk.descr[i].name, "listenportfilename")) {
            CHKiRet(setCnfString(&inst->pszLstnPortFileName, pvals[i].val.d.estr));
        } else if (!strcmp(inppblk.descr[i].name, "networknamespace")) {
            CHKiRet(setCnfCString(&inst->pszNetworkNamespace, pvals[i].val.d.estr));
        } else if (!strcmp(inppblk.descr[i].name, "streamdriver.name")) {
            CHKiRet(setCnfString(&inst->pszStrmDrvrName, pvals[i].val.d.estr));
        } else if (!strcmp(inppblk.descr[i].name, "streamdriver.mode")) {
            inst->iStrmDrvrMode = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "streamdriver.authmode")) {
            CHKiRet(setCnfString(&inst->pszStrmDrvrAuthMode, pvals[i].val.d.estr));
        } else if (!strcmp(inppblk.descr[i].name, "streamdriver.permitexpiredcerts")) {
            CHKiRet(setCnfString(&inst->pszStrmDrvrPermitExpiredCerts, pvals[i].val.d.estr));
        } else if (!strcmp(inppblk.descr[i].name, "streamdriver.cafile")) {
            CHKiRet(setCnfString(&inst->pszStrmDrvrCAFile, pvals[i].val.d.estr));
        } else if (!strcmp(inppblk.descr[i].name, "streamdriver.crlfile")) {
            CHKiRet(setCnfString(&inst->pszStrmDrvrCRLFile, pvals[i].val.d.estr));
        } else if (!strcmp(inppblk.descr[i].name, "streamdriver.keyfile")) {
            CHKiRet(setCnfString(&inst->pszStrmDrvrKeyFile, pvals[i].val.d.estr));
        } else if (!strcmp(inppblk.descr[i].name, "streamdriver.certfile")) {
            CHKiRet(setCnfString(&inst->pszStrmDrvrCertFile, pvals[i].val.d.estr));
        } else if (!strcmp(inppblk.descr[i].name, "streamdriver.checkextendedkeypurpose")) {
            inst->iStrmDrvrExtendedCertCheck = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "streamdriver.prioritizesan")) {
            inst->iStrmDrvrSANPreference = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "streamdriver.tlsverifydepth")) {
            inst->iStrmTlsVerifyDepth = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "streamdriver.tlsrevocationcheck")) {
            inst->iStrmTlsRevocationCheck = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "permittedpeer")) {
            for (j = 0; j < pvals[i].val.d.ar->nmemb; ++j) {
                uchar *peer = NULL;
                CHKiRet(setCnfString(&peer, pvals[i].val.d.ar->arr[j]));
                iRet = net.AddPermittedPeer(&inst->pPermPeersRoot, peer);
                free(peer);
                CHKiRet(iRet);
            }
        } else if (!strcmp(inppblk.descr[i].name, "keepalive")) {
            inst->bKeepAlive = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "keepalive.probes")) {
            inst->iKeepAliveProbes = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "keepalive.time")) {
            inst->iKeepAliveTime = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "keepalive.interval")) {
            inst->iKeepAliveIntvl = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "gnutlsprioritystring")) {
            CHKiRet(setCnfString(&inst->gnutlsPriorityString, pvals[i].val.d.estr));
        } else if (!strcmp(inppblk.descr[i].name, "maxwindowsize")) {
            CHKiRet(validateUInt32Limit("maxWindowSize", pvals[i].val.d.n, IMBEATS_MAX_WINDOW_SIZE_LIMIT,
                                        &inst->maxWindowSize));
        } else if (!strcmp(inppblk.descr[i].name, "maxframesize")) {
            CHKiRet(
                validateSizeLimit("maxFrameSize", pvals[i].val.d.n, IMBEATS_MAX_BYTE_SIZE_LIMIT, &inst->maxFrameSize));
        } else if (!strcmp(inppblk.descr[i].name, "maxdecompressedsize")) {
            CHKiRet(validateSizeLimit("maxDecompressedSize", pvals[i].val.d.n, IMBEATS_MAX_BYTE_SIZE_LIMIT,
                                      &inst->maxDecompressedSize));
        } else if (!strcmp(inppblk.descr[i].name, "maxbatchbytes")) {
            CHKiRet(validateSizeLimit("maxBatchBytes", pvals[i].val.d.n, IMBEATS_MAX_BYTE_SIZE_LIMIT,
                                      &inst->maxBatchBytes));
        } else if (!strcmp(inppblk.descr[i].name, "maxsessions")) {
            inst->maxSessions = (unsigned)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "workerthreads")) {
            if (pvals[i].val.d.n > IMBEATS_MAX_WORKER_THREADS) {
                LogError(0, RS_RET_PARAM_ERROR,
                         "imbeats: invalid value for 'workerThreads' parameter given is %lld, valid range is 1..%u",
                         (long long)pvals[i].val.d.n, IMBEATS_MAX_WORKER_THREADS);
                ABORT_FINALIZE(RS_RET_PARAM_ERROR);
            }
            inst->workerThreads = (unsigned)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "starvationprotection.maxreads")) {
            inst->starvationMaxReads = (unsigned)pvals[i].val.d.n;
        }
    }
    if (inst->pszInputName != NULL) {
        CHKiRet(prop.Construct(&inst->pInputName));
        CHKiRet(prop.SetString(inst->pInputName, inst->pszInputName, ustrlen(inst->pszInputName)));
        CHKiRet(prop.ConstructFinalize(inst->pInputName));
    }

finalize_it:
    CODE_STD_FINALIZERnewInpInst if (pvals != NULL) cnfparamvalsDestruct(pvals, &inppblk);
ENDnewInpInst

BEGINbeginCnfLoad
    CODESTARTbeginCnfLoad;
    loadModConf = pModConf;
    pModConf->pConf = pConf;
ENDbeginCnfLoad

BEGINsetModCnf
    struct cnfparamvals *pvals = NULL;
    CODESTARTsetModCnf;
    pvals = nvlstGetParams(lst, &modpblk, NULL);
    if (pvals != NULL) cnfparamvalsDestruct(pvals, &modpblk);
ENDsetModCnf

BEGINendCnfLoad
    CODESTARTendCnfLoad;
    loadModConf = NULL;
ENDendCnfLoad

BEGINcheckCnf
    instanceConf_t *inst;
    CODESTARTcheckCnf;
    for (inst = pModConf->root; inst != NULL; inst = inst->next) {
        std_checkRuleset(pModConf, inst);
    }
ENDcheckCnf

BEGINactivateCnfPrePrivDrop
    CODESTARTactivateCnfPrePrivDrop;
    runModConf = pModConf;
ENDactivateCnfPrePrivDrop

BEGINactivateCnf
    instanceConf_t *inst;
    CODESTARTactivateCnf;
    for (inst = pModConf->root; inst != NULL; inst = inst->next) {
        if (inst->pszBindRuleset != NULL) {
            const rsRetVal localRet = ruleset.GetRuleset(pModConf->pConf, &inst->pBindRuleset, inst->pszBindRuleset);
            if (localRet != RS_RET_OK) {
                iRet = localRet;
                break;
            }
        }
    }
ENDactivateCnf

BEGINfreeCnf
    instanceConf_t *inst, *del;
    CODESTARTfreeCnf;
    for (inst = pModConf->root; inst != NULL;) {
        del = inst;
        inst = inst->next;
        destroyInstanceRuntime(del);
        free(del->port);
        free(del->address);
        free(del->pszBindRuleset);
        free(del->pszInputName);
        free(del->pszLstnPortFileName);
        free(del->pszNetworkNamespace);
        free(del->pszStrmDrvrName);
        free(del->pszStrmDrvrAuthMode);
        free(del->pszStrmDrvrPermitExpiredCerts);
        free(del->pszStrmDrvrCAFile);
        free(del->pszStrmDrvrCRLFile);
        free(del->pszStrmDrvrKeyFile);
        free(del->pszStrmDrvrCertFile);
        free(del->gnutlsPriorityString);
        if (del->pPermPeersRoot != NULL) {
            net.DestructPermittedPeers(&del->pPermPeersRoot);
        }
        if (del->pInputName != NULL) {
            prop.Destruct(&del->pInputName);
        }
        pthread_mutex_destroy(&del->mutSessions);
        pthread_mutex_destroy(&del->mutWork);
        pthread_cond_destroy(&del->condWork);
        free(del);
    }
ENDfreeCnf

BEGINrunInput
    instanceConf_t *inst;
    CODESTARTrunInput;
    for (inst = runModConf->root; inst != NULL; inst = inst->next) {
        clearInstanceShuttingDown(inst);
        pthread_mutex_lock(&inst->mutWork);
        inst->workShutdown = 0;
        pthread_mutex_unlock(&inst->mutWork);
        CHKiRet(buildListeners(inst));
        if (pthread_create(&inst->listener_tid, NULL, listenerThread, inst) != 0) {
            ABORT_FINALIZE(RS_RET_IO_ERROR);
        }
        inst->listener_running = 1;
    }
    while (glbl.GetGlobalInputTermState() == 0) {
        srSleep(0, 250000);
    }
    for (inst = runModConf->root; inst != NULL; inst = inst->next) {
        destroyInstanceRuntime(inst);
    }
finalize_it:
ENDrunInput

BEGINwillRun
    CODESTARTwillRun;
ENDwillRun

BEGINafterRun
    CODESTARTafterRun;
ENDafterRun

BEGINmodExit
    CODESTARTmodExit;
    if (pInputName != NULL) {
        prop.Destruct(&pInputName);
    }
    if (statsCounter.stats != NULL) {
        statsobj.Destruct(&statsCounter.stats);
    }
    objRelease(statsobj, CORE_COMPONENT);
    objRelease(glbl, CORE_COMPONENT);
    objRelease(net, LM_NET_FILENAME);
    objRelease(netstrm, LM_NETSTRMS_FILENAME);
    objRelease(netstrms, LM_NETSTRMS_FILENAME);
    objRelease(prop, CORE_COMPONENT);
    objRelease(ruleset, CORE_COMPONENT);
ENDmodExit

BEGINisCompatibleWithFeature
    CODESTARTisCompatibleWithFeature;
    if (eFeat == sFEATURENonCancelInputTermination) iRet = RS_RET_OK;
ENDisCompatibleWithFeature

BEGINqueryEtryPt
    CODESTARTqueryEtryPt;
    CODEqueryEtryPt_STD_IMOD_QUERIES;
    CODEqueryEtryPt_STD_CONF2_QUERIES;
    CODEqueryEtryPt_STD_CONF2_setModCnf_QUERIES;
    CODEqueryEtryPt_STD_CONF2_PREPRIVDROP_QUERIES;
    CODEqueryEtryPt_STD_CONF2_IMOD_QUERIES;
    CODEqueryEtryPt_IsCompatibleWithFeature_IF_OMOD_QUERIES;
ENDqueryEtryPt

BEGINmodInit()
    CODESTARTmodInit;
    *ipIFVersProvided = CURR_MOD_IF_VERSION;
    CODEmodInit_QueryRegCFSLineHdlr;
    CHKiRet(objUse(glbl, CORE_COMPONENT));
    CHKiRet(objUse(net, LM_NET_FILENAME));
    CHKiRet(objUse(netstrm, LM_NETSTRMS_FILENAME));
    CHKiRet(objUse(netstrms, LM_NETSTRMS_FILENAME));
    CHKiRet(objUse(prop, CORE_COMPONENT));
    CHKiRet(objUse(ruleset, CORE_COMPONENT));
    CHKiRet(objUse(statsobj, CORE_COMPONENT));

    CHKiRet(prop.Construct(&pInputName));
    CHKiRet(prop.SetString(pInputName, UCHAR_CONSTANT("imbeats"), sizeof("imbeats") - 1));
    CHKiRet(prop.ConstructFinalize(pInputName));

    CHKiRet(statsobj.Construct(&statsCounter.stats));
    CHKiRet(statsobj.SetName(statsCounter.stats, UCHAR_CONSTANT("imbeats")));
    CHKiRet(statsobj.SetOrigin(statsCounter.stats, UCHAR_CONSTANT("imbeats")));
    STATSCOUNTER_INIT(statsCounter.ctrConnectionsAccepted, statsCounter.mutCtrConnectionsAccepted);
    STATSCOUNTER_INIT(statsCounter.ctrConnectionsClosed, statsCounter.mutCtrConnectionsClosed);
    STATSCOUNTER_INIT(statsCounter.ctrProtocolErrors, statsCounter.mutCtrProtocolErrors);
    STATSCOUNTER_INIT(statsCounter.ctrBatchesReceived, statsCounter.mutCtrBatchesReceived);
    STATSCOUNTER_INIT(statsCounter.ctrBatchesAcked, statsCounter.mutCtrBatchesAcked);
    STATSCOUNTER_INIT(statsCounter.ctrEventsReceived, statsCounter.mutCtrEventsReceived);
    STATSCOUNTER_INIT(statsCounter.ctrEventsSubmitted, statsCounter.mutCtrEventsSubmitted);
    STATSCOUNTER_INIT(statsCounter.ctrEventsFailed, statsCounter.mutCtrEventsFailed);
    STATSCOUNTER_INIT(statsCounter.ctrCompressedFrames, statsCounter.mutCtrCompressedFrames);
    STATSCOUNTER_INIT(statsCounter.ctrJsonDecodeFailures, statsCounter.mutCtrJsonDecodeFailures);
    STATSCOUNTER_INIT(statsCounter.ctrAckFailures, statsCounter.mutCtrAckFailures);
    STATSCOUNTER_INIT(statsCounter.ctrWindowsRejected, statsCounter.mutCtrWindowsRejected);
    STATSCOUNTER_INIT(statsCounter.ctrFramesRejected, statsCounter.mutCtrFramesRejected);
    STATSCOUNTER_INIT(statsCounter.ctrCompressedRejected, statsCounter.mutCtrCompressedRejected);
    STATSCOUNTER_INIT(statsCounter.ctrConnectionsActive, statsCounter.mutCtrConnectionsActive);
    STATSCOUNTER_INIT(statsCounter.ctrConnectionsRejected, statsCounter.mutCtrConnectionsRejected);
    STATSCOUNTER_INIT(statsCounter.ctrStarvationProtect, statsCounter.mutCtrStarvationProtect);
    CHKiRet(statsobj.AddCounter(statsCounter.stats, UCHAR_CONSTANT("connections.accepted"), ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &statsCounter.ctrConnectionsAccepted));
    CHKiRet(statsobj.AddCounter(statsCounter.stats, UCHAR_CONSTANT("connections.closed"), ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &statsCounter.ctrConnectionsClosed));
    CHKiRet(statsobj.AddCounter(statsCounter.stats, UCHAR_CONSTANT("protocol_errors"), ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &statsCounter.ctrProtocolErrors));
    CHKiRet(statsobj.AddCounter(statsCounter.stats, UCHAR_CONSTANT("batches.received"), ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &statsCounter.ctrBatchesReceived));
    CHKiRet(statsobj.AddCounter(statsCounter.stats, UCHAR_CONSTANT("batches.acked"), ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &statsCounter.ctrBatchesAcked));
    CHKiRet(statsobj.AddCounter(statsCounter.stats, UCHAR_CONSTANT("events.received"), ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &statsCounter.ctrEventsReceived));
    CHKiRet(statsobj.AddCounter(statsCounter.stats, UCHAR_CONSTANT("events.submitted"), ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &statsCounter.ctrEventsSubmitted));
    CHKiRet(statsobj.AddCounter(statsCounter.stats, UCHAR_CONSTANT("events.failed"), ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &statsCounter.ctrEventsFailed));
    CHKiRet(statsobj.AddCounter(statsCounter.stats, UCHAR_CONSTANT("compressed_frames"), ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &statsCounter.ctrCompressedFrames));
    CHKiRet(statsobj.AddCounter(statsCounter.stats, UCHAR_CONSTANT("json_decode_failures"), ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &statsCounter.ctrJsonDecodeFailures));
    CHKiRet(statsobj.AddCounter(statsCounter.stats, UCHAR_CONSTANT("ack_failures"), ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &statsCounter.ctrAckFailures));
    CHKiRet(statsobj.AddCounter(statsCounter.stats, UCHAR_CONSTANT("windows.rejected"), ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &statsCounter.ctrWindowsRejected));
    CHKiRet(statsobj.AddCounter(statsCounter.stats, UCHAR_CONSTANT("frames.rejected"), ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &statsCounter.ctrFramesRejected));
    CHKiRet(statsobj.AddCounter(statsCounter.stats, UCHAR_CONSTANT("compressed.rejected"), ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &statsCounter.ctrCompressedRejected));
    CHKiRet(statsobj.AddCounter(statsCounter.stats, UCHAR_CONSTANT("connections.active"), ctrType_IntCtr, CTR_FLAG_NONE,
                                &statsCounter.ctrConnectionsActive));
    CHKiRet(statsobj.AddCounter(statsCounter.stats, UCHAR_CONSTANT("connections.rejected"), ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &statsCounter.ctrConnectionsRejected));
    CHKiRet(statsobj.AddCounter(statsCounter.stats, UCHAR_CONSTANT("starvation_protect"), ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &statsCounter.ctrStarvationProtect));
    CHKiRet(statsobj.ConstructFinalize(statsCounter.stats));
ENDmodInit
