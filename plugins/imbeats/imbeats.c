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
#define IMBEATS_MAX_BYTE_SIZE_LIMIT 200000000

DEF_IMOD_STATIC_DATA;
DEFobjCurrIf(glbl) DEFobjCurrIf(net) DEFobjCurrIf(netstrm) DEFobjCurrIf(netstrms) DEFobjCurrIf(prop)
    DEFobjCurrIf(ruleset) DEFobjCurrIf(statsobj)

        typedef struct session_s session_t;

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

    netstrms_t *pNS;
    netstrm_t **listeners;
    int *listener_socks;
    size_t listener_count;
    size_t listener_cap;
    pthread_t listener_tid;
    int listener_running;
    int shuttingDown;
    pthread_mutex_t mutSessions;
    session_t *sessions;
} instanceConf_t;

typedef struct modConfData_s {
    rsconf_t *pConf;
    instanceConf_t *root;
    instanceConf_t *tail;
} modConfData_t;

struct session_s {
    instanceConf_t *inst;
    netstrm_t *pStrm;
    pthread_t tid;
    int done;
    int threadStarted;
    int sock;
    uchar *fromHostFQDN;
    prop_t *fromHost;
    prop_t *fromHostIP;
    prop_t *fromHostPort;
    session_t *next;
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
static void *sessionThread(void *arg);

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
    DEFiRet;

    memset(&params, 0, sizeof(params));
    params.pszPort = inst->port;
    params.pszAddr = inst->address;
    params.pszLstnPortFileName = inst->pszLstnPortFileName;
    params.pszNetworkNamespace = inst->pszNetworkNamespace;

    CHKiRet(configureNetstrms(inst));
    CHKiRet(netstrm.LstnInit(inst->pNS, inst, addListener, 0, &params));

finalize_it:
    RETiRet;
}

static rsRetVal sessionPollWait(const int fd, const short events, const int timeout_ms) {
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = events;
    pfd.revents = 0;
    if (poll(&pfd, 1, timeout_ms) < 0) {
        return RS_RET_IO_ERROR;
    }
    return RS_RET_OK;
}

static rsRetVal sessionRead(session_t *const sess, unsigned char *buf, const size_t len) {
    size_t off = 0;
    int oserr;
    unsigned nextIODirection;
    rsRetVal iRet;

    assert(sess != NULL);
    while (off < len && glbl.GetGlobalInputTermState() == 0) {
        ssize_t need = (ssize_t)(len - off);
        iRet = netstrm.Rcv(sess->pStrm, buf + off, &need, &oserr, &nextIODirection);
        if (iRet == RS_RET_OK) {
            off += need;
        } else if (iRet == RS_RET_RETRY) {
            CHKiRet(sessionPollWait(sess->sock, (nextIODirection == NSDSEL_WR) ? POLLOUT : POLLIN, 1000));
        } else {
            ABORT_FINALIZE(iRet);
        }
    }

    if (off != len) {
        ABORT_FINALIZE(RS_RET_CLOSED);
    }

finalize_it:
    RETiRet;
}

static rsRetVal sessionSendAll(session_t *const sess, const unsigned char *buf, const size_t len) {
    size_t off = 0;
    rsRetVal iRet;

    while (off < len && glbl.GetGlobalInputTermState() == 0) {
        ssize_t to_send = (ssize_t)(len - off);
        iRet = netstrm.Send(sess->pStrm, (uchar *)buf + off, &to_send);
        if (iRet == RS_RET_OK) {
            if (to_send == 0) {
                CHKiRet(sessionPollWait(sess->sock, POLLOUT, 1000));
            } else {
                off += to_send;
            }
        } else {
            ABORT_FINALIZE(iRet);
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
    CHKiRet(msgAddJSON(pMsg, (uchar *)"!", json, 0, 0));
    json = NULL;

    CHKmalloc(meta = json_object_new_object());
    fjson_object_object_add(meta, "protocol", json_object_new_string("lumberjack-v2"));
    fjson_object_object_add(meta, "sequence", json_object_new_int64(event->seq));
    fjson_object_object_add(meta, "tls_enabled", json_object_new_boolean(sess->inst->iStrmDrvrMode != 0));
    if (sess->fromHostFQDN != NULL) {
        fjson_object_object_add(meta, "peer_hostname", json_object_new_string((char *)sess->fromHostFQDN));
    }
    if (sess->fromHostIP != NULL) {
        fjson_object_object_add(meta, "peer_ip", json_object_new_string(propGetSzStrOrDefault(sess->fromHostIP, "")));
    }
    if (sess->fromHostPort != NULL) {
        snprintf(portbuf, sizeof(portbuf), "%s", propGetSzStrOrDefault(sess->fromHostPort, ""));
        fjson_object_object_add(meta, "peer_port", json_object_new_string(portbuf));
    }
    CHKiRet(msgAddJSON(pMsg, (uchar *)"!metadata!imbeats", meta, 0, 0));
    meta = NULL;

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

static rsRetVal receiveBatch(session_t *const sess, struct lj_batch_s *batch) {
    unsigned char hdr[2];
    uint32_t net32;
    uint32_t seq = 0;
    size_t idx;
    DEFiRet;

    CHKiRet(sessionRead(sess, hdr, sizeof(hdr)));
    CHKiRet(sessionRead(sess, (unsigned char *)&net32, sizeof(net32)));
    net32 = ntohl(net32);
    CHKiRet(lj_parse_window_header(hdr, net32));
    if (net32 > sess->inst->maxWindowSize) {
        STATSCOUNTER_INC(statsCounter.ctrWindowsRejected, statsCounter.mutCtrWindowsRejected);
        ABORT_FINALIZE(RS_RET_INVALID_VALUE);
    }
    CHKiRet(lj_batch_alloc(batch, net32, sess->inst->maxWindowSize));
    STATSCOUNTER_INC(statsCounter.ctrBatchesReceived, statsCounter.mutCtrBatchesReceived);

    while (batch->count < batch->window_size) {
        unsigned char frame_hdr[2];
        CHKiRet(sessionRead(sess, frame_hdr, sizeof(frame_hdr)));
        if (frame_hdr[0] != LJ_VERSION_V2) {
            ABORT_FINALIZE(RS_RET_INVALID_VALUE);
        }
        if (frame_hdr[1] == LJ_FRAME_JSON) {
            uint32_t payload_len;
            unsigned char *payload = NULL;
            CHKiRet(sessionRead(sess, (unsigned char *)&net32, sizeof(net32)));
            seq = ntohl(net32);
            CHKiRet(sessionRead(sess, (unsigned char *)&net32, sizeof(net32)));
            payload_len = ntohl(net32);
            if (payload_len == 0 || (size_t)payload_len > sess->inst->maxFrameSize) {
                STATSCOUNTER_INC(statsCounter.ctrFramesRejected, statsCounter.mutCtrFramesRejected);
                ABORT_FINALIZE(RS_RET_INVALID_VALUE);
            }
            CHKmalloc(payload = malloc(payload_len));
            iRet = sessionRead(sess, payload, payload_len);
            if (iRet != RS_RET_OK) {
                free(payload);
                ABORT_FINALIZE(iRet);
            }
            iRet = lj_append_json_event(batch, seq, payload, payload_len);
            free(payload);
            CHKiRet(iRet);
        } else if (frame_hdr[1] == LJ_FRAME_COMPRESSED) {
            uint32_t compressed_len;
            unsigned char *payload = NULL;
            STATSCOUNTER_INC(statsCounter.ctrCompressedFrames, statsCounter.mutCtrCompressedFrames);
            CHKiRet(sessionRead(sess, (unsigned char *)&net32, sizeof(net32)));
            compressed_len = ntohl(net32);
            if (compressed_len == 0 || (size_t)compressed_len > sess->inst->maxFrameSize) {
                STATSCOUNTER_INC(statsCounter.ctrFramesRejected, statsCounter.mutCtrFramesRejected);
                STATSCOUNTER_INC(statsCounter.ctrCompressedRejected, statsCounter.mutCtrCompressedRejected);
                ABORT_FINALIZE(RS_RET_INVALID_VALUE);
            }
            CHKmalloc(payload = malloc(compressed_len));
            iRet = sessionRead(sess, payload, compressed_len);
            if (iRet != RS_RET_OK) {
                free(payload);
                ABORT_FINALIZE(iRet);
            }
            iRet = lj_parse_compressed_frames(batch, payload, compressed_len, sess->inst->maxFrameSize,
                                              sess->inst->maxDecompressedSize);
            free(payload);
            if (iRet == RS_RET_INVALID_VALUE) {
                STATSCOUNTER_INC(statsCounter.ctrCompressedRejected, statsCounter.mutCtrCompressedRejected);
            }
            CHKiRet(iRet);
        } else {
            ABORT_FINALIZE(RS_RET_INVALID_VALUE);
        }
    }

    for (idx = 0; idx < batch->count; ++idx) {
        const uint32_t expected = (uint32_t)idx + 1;
        if (batch->events[idx].seq != expected) {
            ABORT_FINALIZE(RS_RET_INVALID_VALUE);
        }
    }
    STATSCOUNTER_ADD(statsCounter.ctrEventsReceived, statsCounter.mutCtrEventsReceived, batch->count);

finalize_it:
    RETiRet;
}

static rsRetVal ackBatch(session_t *const sess, const uint32_t seq) {
    unsigned char ack[6];
    uint32_t netseq = htonl(seq);
    rsRetVal iRet;

    ack[0] = LJ_VERSION_V2;
    ack[1] = LJ_FRAME_ACK;
    memcpy(ack + 2, &netseq, sizeof(netseq));
    iRet = sessionSendAll(sess, ack, sizeof(ack));
    if (iRet == RS_RET_OK) {
        STATSCOUNTER_INC(statsCounter.ctrBatchesAcked, statsCounter.mutCtrBatchesAcked);
    } else {
        STATSCOUNTER_INC(statsCounter.ctrAckFailures, statsCounter.mutCtrAckFailures);
    }
    return iRet;
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

static session_t *takeSession(instanceConf_t *const inst, const int takeAll) {
    session_t *sess = NULL;
    session_t **pp;

    pthread_mutex_lock(&inst->mutSessions);
    for (pp = &inst->sessions; *pp != NULL; pp = &(*pp)->next) {
        if (takeAll || (*pp)->done) {
            sess = *pp;
            *pp = sess->next;
            sess->next = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&inst->mutSessions);
    return sess;
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

static void reapSessions(instanceConf_t *const inst, const int reapAll) {
    session_t *sess;

    while ((sess = takeSession(inst, reapAll)) != NULL) {
        if (reapAll) {
            shutdownSessionSocket(sess);
        }
        if (sess->threadStarted) {
            pthread_join(sess->tid, NULL);
        }
        destroySession(sess);
    }
}

static void destroySession(session_t *sess) {
    if (sess == NULL) {
        return;
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
    free(sess);
}

static void *sessionThread(void *arg) {
    session_t *const sess = (session_t *)arg;
    rsRetVal iRet = RS_RET_OK;

    assert(sess != NULL);

    while (glbl.GetGlobalInputTermState() == 0 && !instanceIsShuttingDown(sess->inst)) {
        struct lj_batch_s batch;
        size_t idx;

        memset(&batch, 0, sizeof(batch));
        iRet = receiveBatch(sess, &batch);
        if (iRet != RS_RET_OK) {
            if (iRet == RS_RET_INVALID_VALUE) {
                STATSCOUNTER_INC(statsCounter.ctrProtocolErrors, statsCounter.mutCtrProtocolErrors);
            }
            lj_batch_free(&batch);
            break;
        }

        for (idx = 0; idx < batch.count; ++idx) {
            iRet = submitEvent(sess, &batch.events[idx]);
            if (iRet != RS_RET_OK) {
                break;
            }
        }
        if (iRet == RS_RET_OK) {
            iRet = ackBatch(sess, batch.events[batch.count - 1].seq);
        }
        lj_batch_free(&batch);
        if (iRet != RS_RET_OK) {
            break;
        }
    }

    STATSCOUNTER_INC(statsCounter.ctrConnectionsClosed, statsCounter.mutCtrConnectionsClosed);
    pthread_mutex_lock(&sess->inst->mutSessions);
    sess->done = 1;
    pthread_mutex_unlock(&sess->inst->mutSessions);
    return NULL;
}

static rsRetVal spawnSession(instanceConf_t *const inst, netstrm_t **const ppNewStrm) {
    session_t *sess = NULL;
    struct sockaddr_storage *addr = NULL;
    DEFiRet;

    assert(ppNewStrm != NULL);
    assert(*ppNewStrm != NULL);

    if (ppNewStrm == NULL || *ppNewStrm == NULL) {
        return RS_RET_INVALID_VALUE;
    }
    CHKmalloc(sess = calloc(1, sizeof(*sess)));
    sess->sock = -1;
    sess->inst = inst;
    sess->pStrm = *ppNewStrm;
    *ppNewStrm = NULL;
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
    CHKiRet(netstrm.GetRemoteIP(sess->pStrm, &sess->fromHostIP));
    CHKiRet(netstrm.GetRemAddr(sess->pStrm, &addr));
    CHKiRet(createPortProp(addr, &sess->fromHostPort));

    pthread_mutex_lock(&inst->mutSessions);
    if (inst->shuttingDown) {
        pthread_mutex_unlock(&inst->mutSessions);
        ABORT_FINALIZE(RS_RET_FORCE_TERM);
    }
    sess->next = inst->sessions;
    inst->sessions = sess;
    if (pthread_create(&sess->tid, NULL, sessionThread, sess) != 0) {
        unlinkSession(inst, sess);
        pthread_mutex_unlock(&inst->mutSessions);
        ABORT_FINALIZE(RS_RET_IO_ERROR);
    }
    sess->threadStarted = 1;
    pthread_mutex_unlock(&inst->mutSessions);
    STATSCOUNTER_INC(statsCounter.ctrConnectionsAccepted, statsCounter.mutCtrConnectionsAccepted);
    sess = NULL;

finalize_it:
    if (iRet != RS_RET_OK && sess != NULL) {
        destroySession(sess);
    }
    RETiRet;
}

static void *listenerThread(void *arg) {
    instanceConf_t *const inst = (instanceConf_t *)arg;
    struct pollfd *pfds = NULL;
    size_t i;

    assert(inst != NULL);
    pfds = calloc(inst->listener_count, sizeof(struct pollfd));
    if (pfds == NULL) {
        return NULL;
    }
    for (i = 0; i < inst->listener_count; ++i) {
        pfds[i].fd = inst->listener_socks[i];
        pfds[i].events = POLLIN;
    }

    while (glbl.GetGlobalInputTermState() == 0 && !instanceIsShuttingDown(inst)) {
        if (poll(pfds, inst->listener_count, 1000) <= 0) {
            reapSessions(inst, 0);
            continue;
        }
        for (i = 0; i < inst->listener_count; ++i) {
            if ((pfds[i].revents & POLLIN) != 0 && !instanceIsShuttingDown(inst)) {
                netstrm_t *pNewStrm = NULL;
                char connInfo[TCPSRV_CONNINFO_SIZE];
                rsRetVal iRet;

                memset(connInfo, 0, sizeof(connInfo));
                iRet = netstrm.AcceptConnReq(inst->listeners[i], &pNewStrm, connInfo);
                if (iRet == RS_RET_OK) {
                    iRet = spawnSession(inst, &pNewStrm);
                    if (iRet != RS_RET_OK && pNewStrm != NULL) {
                        netstrm.Destruct(&pNewStrm);
                    }
                }
            }
        }
        reapSessions(inst, 0);
    }

    free(pfds);
    return NULL;
}

static rsRetVal destroyInstanceRuntime(instanceConf_t *inst) {
    size_t i;

    setInstanceShuttingDown(inst);
    if (inst->listener_running) {
        pthread_join(inst->listener_tid, NULL);
        inst->listener_running = 0;
    }
    shutdownAllSessionSockets(inst);
    reapSessions(inst, 1);

    for (i = 0; i < inst->listener_count; ++i) {
        if (inst->listeners[i] != NULL) {
            netstrm.Destruct(&inst->listeners[i]);
        }
    }
    free(inst->listeners);
    inst->listeners = NULL;
    free(inst->listener_socks);
    inst->listener_socks = NULL;
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
    inst->iStrmDrvrMode = 0;
    inst->maxWindowSize = IMBEATS_DEFAULT_MAX_WINDOW_SIZE;
    inst->maxFrameSize = IMBEATS_DEFAULT_MAX_FRAME_SIZE;
    inst->maxDecompressedSize = IMBEATS_DEFAULT_MAX_DECOMPRESSED_SIZE;
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
        free(del);
    }
ENDfreeCnf

BEGINrunInput
    instanceConf_t *inst;
    CODESTARTrunInput;
    for (inst = runModConf->root; inst != NULL; inst = inst->next) {
        clearInstanceShuttingDown(inst);
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
    CHKiRet(statsobj.ConstructFinalize(statsCounter.stats));
ENDmodInit
