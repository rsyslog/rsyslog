/* imhttp.c
 * This is an input module for receiving http input.
 *
 * This file is originally contribution to rsyslog.
 *
 * Copyright 2025 Adiscon GmbH and Others
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
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <ctype.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "rsyslog.h"
#include "cfsysline.h" /* access to config file objects */
#include "module-template.h"
#include "ruleset.h"
#include "unicode-helper.h"
#include "rsyslog.h"
#include "errmsg.h"
#include "statsobj.h"
#include "ratelimit.h"
#include "dirty.h"

#include "civetweb.h"
#include <apr_base64.h>
#include <apr_md5.h>

MODULE_TYPE_INPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME("imhttp")

/* static data */
DEF_OMOD_STATIC_DATA;
DEFobjCurrIf(glbl) DEFobjCurrIf(prop) DEFobjCurrIf(ruleset) DEFobjCurrIf(statsobj)
#define CIVETWEB_OPTION_NAME_PORTS "listening_ports"
#define CIVETWEB_OPTION_NAME_DOCUMENT_ROOT "document_root"
#define MAX_READ_BUFFER_SIZE 16384
#define INIT_SCRATCH_BUF_SIZE 4096
/* General purpose buffer size. */
/**
 * Basic-auth credentials are capped at 2 KiB of Base64 text. That is well
 * above practical user-id/password sizes, yet still small enough to keep the
 * parser on a fixed stack buffer and to reject oversized headers as hostile.
 */
#define IMHTTP_MAX_BUF_LEN (8192)
#define BASIC_AUTH_MAX_ENCODED 2048
#define BASIC_AUTH_MAX_DECODED ((BASIC_AUTH_MAX_ENCODED / 4) * 3)
#define DEFAULT_HEALTH_CHECK_PATH "/healthz"
#define DEFAULT_PROM_METRICS_PATH "/metrics"

    struct option {
    const char *name;
    const char *val;
};

struct auth_s {
    char workbuf[BASIC_AUTH_MAX_DECODED + 1];
    char *pszUser;
    char *pszPasswd;
    const char *pszApiKey;
};

/**
 * Route auth settings are borrowed from the finalized module config.
 *
 * CivetWeb handlers only keep references because the config lifetime covers
 * the full server lifetime.
 */
struct route_auth_ctx_s {
    const char *pszBasicAuthFile;
    const char *pszApiKeyFile;
};

struct data_parse_s {
    sbool content_compressed;
    sbool bzInitDone; /* did we do an init of zstrm already? */
    z_stream zstrm; /* zip stream to use for tcp compression */
    // Currently only used for octet specific parsing
    enum {
        eAtStrtFram,
        eInOctetCnt,
        eInMsg,
    } inputState;
    size_t iOctetsRemain; /* Number of Octets remaining in message */
    enum { TCP_FRAMING_OCTET_STUFFING, TCP_FRAMING_OCTET_COUNTING } framingMode;
};

struct modConfData_s {
    rsconf_t *pConf; /* our overall config object */
    instanceConf_t *root, *tail;
    struct option ports;
    struct option docroot;
    struct option *options;
    int nOptions;
    char *pszHealthCheckPath;
    char *pszMetricsPath;
    char *pszHealthCheckAuthFile;
    char *pszMetricsAuthFile;
    char *pszHealthCheckApiKeyFile;
    char *pszMetricsApiKeyFile;
    struct route_auth_ctx_s healthCheckAuthCtx;
    struct route_auth_ctx_s metricsAuthCtx;
};

struct instanceConf_s {
    struct instanceConf_s *next;
    uchar *pszBindRuleset; /* name of ruleset to bind to */
    uchar *pszEndpoint; /* endpoint to configure */
    uchar *pszBasicAuthFile; /* file containing basic auth users/pass */
    uchar *pszApiKeyFile; /* file containing accepted API keys */
    ruleset_t *pBindRuleset; /* ruleset to bind listener to (use system default if unspecified) */
    ratelimit_t *ratelimiter;
    int ratelimitInterval;
    int ratelimitBurst;
    uchar *pszRatelimitName;
    uchar *pszInputName; /* value for inputname property, NULL is OK and handled by core engine */
    prop_t *pInputName;
    sbool flowControl;
    sbool bDisableLFDelim;
    sbool bSuppOctetFram;
    sbool bAddMetadata;
    struct route_auth_ctx_s authCtx;
};

struct conn_wrkr_s {
    struct data_parse_s parseState;
    uchar *pMsg; /* msg scratch buffer */
    size_t iMsg; /* index of next char to store in msg */
    uchar zipBuf[64 * 1024];
    multi_submit_t multiSub;
    smsg_t *pMsgs[CONF_NUM_MULTISUB];
    char *pReadBuf;
    size_t readBufSize;
    prop_t *propRemoteAddr;
    const struct mg_request_info *pri; /* do not free me - used to hold a reference only */
    char *pScratchBuf;
    size_t scratchBufSize;
};

static modConfData_t *loadModConf = NULL; /* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL; /* modConf ptr to use for the current load process */
static prop_t *pInputName = NULL;

// static size_t s_iMaxLine = 16; /* get maximum size we currently support */
static size_t s_iMaxLine = 16384; /* get maximum size we currently support */

/* module-global parameters */
static struct cnfparamdescr modpdescr[] = {{"ports", eCmdHdlrString, 0},
                                           {"documentroot", eCmdHdlrString, 0},
                                           {"liboptions", eCmdHdlrArray, 0},
                                           {"healthcheckpath", eCmdHdlrString, 0},
                                           {"metricspath", eCmdHdlrString, 0},
                                           {"healthcheckbasicauthfile", eCmdHdlrString, 0},
                                           {"metricsbasicauthfile", eCmdHdlrString, 0},
                                           {"healthcheckapikeyfile", eCmdHdlrString, 0},
                                           {"metricsapikeyfile", eCmdHdlrString, 0}};

static struct cnfparamblk modpblk = {CNFPARAMBLK_VERSION, sizeof(modpdescr) / sizeof(struct cnfparamdescr), modpdescr};

static struct cnfparamdescr inppdescr[] = {{"endpoint", eCmdHdlrString, 0},
                                           {"basicauthfile", eCmdHdlrString, 0},
                                           {"apikeyfile", eCmdHdlrString, 0},
                                           {"ruleset", eCmdHdlrString, 0},
                                           {"flowcontrol", eCmdHdlrBinary, 0},
                                           {"disablelfdelimiter", eCmdHdlrBinary, 0},
                                           {"supportoctetcountedframing", eCmdHdlrBinary, 0},
                                           {"name", eCmdHdlrString, 0},
                                           {"ratelimit.interval", eCmdHdlrInt, 0},
                                           {"ratelimit.burst", eCmdHdlrInt, 0},
                                           {"ratelimit.name", eCmdHdlrString, 0},
                                           {"addmetadata", eCmdHdlrBinary, 0}};

#include "im-helper.h" /* must be included AFTER the type definitions! */

static struct cnfparamblk inppblk = {CNFPARAMBLK_VERSION, sizeof(inppdescr) / sizeof(struct cnfparamdescr), inppdescr};

static struct {
    statsobj_t *stats;
    STATSCOUNTER_DEF(ctrSubmitted, mutCtrSubmitted)
    STATSCOUNTER_DEF(ctrFailed, mutCtrFailed);
    STATSCOUNTER_DEF(ctrDiscarded, mutCtrDiscarded);
} statsCounter;

#include "im-helper.h" /* must be included AFTER the type definitions! */

#define min(a, b)               \
    ({                          \
        __typeof__(a) _a = (a); \
        __typeof__(b) _b = (b); \
        _a < _b ? _a : _b;      \
    })

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0
#define EXIT_URI "/exit"
volatile int exitNow = 0;

struct mg_callbacks callbacks;

typedef struct httpserv_s {
    struct mg_context *ctx;
    struct mg_callbacks callbacks;
    const char **civetweb_options;
    size_t civetweb_options_count;
} httpserv_t;

static httpserv_t *s_httpserv;

/* FORWARD DECLARATIONS */
static rsRetVal processData(const instanceConf_t *const inst,
                            struct conn_wrkr_s *connWrkr,
                            const char *buf,
                            size_t len);

/**
 * Install borrowed auth sources for a route.
 *
 * @param authCtx route-local auth state to populate
 * @param basicAuthFile borrowed htpasswd path, or NULL
 * @param apiKeyFile borrowed API-key file path, or NULL
 *
 * @note The pointers are borrowed from the module config and must outlive the
 *       request handler registration.
 */
static void routeAuthCtxSet(struct route_auth_ctx_s *const authCtx,
                            const char *const basicAuthFile,
                            const char *const apiKeyFile) {
    if (authCtx == NULL) {
        return;
    }

    authCtx->pszBasicAuthFile = basicAuthFile;
    authCtx->pszApiKeyFile = apiKeyFile;
}

/**
 * Check whether a route needs auth handling.
 *
 * @param authCtx route-local auth state
 * @return non-zero if Basic auth or API-key auth is configured
 *
 * @note Public routes skip auth handler registration entirely so the request
 *       path stays on the fast path.
 */
static sbool routeAuthConfigured(const struct route_auth_ctx_s *const authCtx) {
    return authCtx != NULL && (authCtx->pszBasicAuthFile != NULL || authCtx->pszApiKeyFile != NULL);
}

static rsRetVal createInstance(instanceConf_t **pinst) {
    instanceConf_t *inst;
    DEFiRet;
    CHKmalloc(inst = calloc(1, sizeof(instanceConf_t)));
    inst->next = NULL;
    inst->pszBindRuleset = NULL;
    inst->pBindRuleset = NULL;
    inst->pszEndpoint = NULL;
    inst->pszBasicAuthFile = NULL;
    inst->pszApiKeyFile = NULL;
    inst->ratelimiter = NULL;
    inst->pszInputName = NULL;
    inst->pInputName = NULL;
    inst->ratelimitBurst = -1;
    inst->ratelimitInterval = -1;
    inst->pszRatelimitName = NULL;
    inst->flowControl = 1;
    inst->bDisableLFDelim = 0;
    inst->bSuppOctetFram = 0;
    inst->bAddMetadata = 0;
    // construct statsobj

    /* node created, let's add to config */
    if (loadModConf->tail == NULL) {
        loadModConf->tail = loadModConf->root = inst;
    } else {
        loadModConf->tail->next = inst;
        loadModConf->tail = inst;
    }

    *pinst = inst;
finalize_it:
    RETiRet;
}

static rsRetVal processCivetwebOptions(char *const param, const char **const name, const char **const paramval) {
    DEFiRet;
    char *val = strstr(param, "=");
    if (val == NULL) {
        LogError(0, RS_RET_PARAM_ERROR,
                 "missing equal sign in "
                 "parameter '%s'",
                 param);
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }
    *val = '\0'; /* terminates name */
    ++val; /* now points to begin of value */
    CHKmalloc(*name = strdup(param));
    CHKmalloc(*paramval = strdup(val));

finalize_it:
    RETiRet;
}

static sbool valid_civetweb_option(const struct mg_option *valid_opts, const char *option) {
    const struct mg_option *pvalid_opts = valid_opts;
    for (; pvalid_opts != NULL && pvalid_opts->name != NULL; pvalid_opts++) {
        if (strcmp(pvalid_opts->name, option) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

/*
 *   thread_type:
 *     0 indicates the master thread
 *     1 indicates a worker thread handling client connections
 *     2 indicates an internal helper thread (timer thread)
 */
static void *init_thread(ATTR_UNUSED const struct mg_context *ctx, int thread_type) {
    DEFiRet;
    struct conn_wrkr_s *data = NULL;
    if (thread_type == 1) {
        CHKmalloc(data = calloc(1, sizeof(struct conn_wrkr_s)));
        data->pMsg = NULL;
        data->iMsg = 0;
        data->parseState.bzInitDone = 0;
        data->parseState.content_compressed = 0;
        data->parseState.inputState = eAtStrtFram;
        data->parseState.iOctetsRemain = 0;
        data->multiSub.maxElem = CONF_NUM_MULTISUB;
        data->multiSub.ppMsgs = data->pMsgs;
        data->multiSub.nElem = 0;
        data->pReadBuf = malloc(MAX_READ_BUFFER_SIZE);
        data->readBufSize = MAX_READ_BUFFER_SIZE;

        data->parseState.bzInitDone = 0;
        data->parseState.content_compressed = 0;
        data->parseState.inputState = eAtStrtFram;
        data->parseState.iOctetsRemain = 0;

        CHKmalloc(data->pMsg = calloc(1, 1 + s_iMaxLine));
        data->iMsg = 0;
        data->propRemoteAddr = NULL;
        data->pScratchBuf = NULL;
        data->scratchBufSize = 0;
    }

finalize_it:
    if (iRet != RS_RET_OK) {
        free(data != NULL ? data->pReadBuf : NULL);
        free(data != NULL ? data->pMsg : NULL);
        free(data);
        return NULL;
    }
    return data;
}

static void exit_thread(ATTR_UNUSED const struct mg_context *ctx, ATTR_UNUSED int thread_type, void *thread_pointer) {
    if (thread_type == 1) {
        struct conn_wrkr_s *data = (struct conn_wrkr_s *)thread_pointer;
        if (data->propRemoteAddr) {
            prop.Destruct(&data->propRemoteAddr);
        }
        if (data->scratchBufSize) {
            free(data->pScratchBuf);
        }
        free(data->pReadBuf);
        free(data->pMsg);
        free(data);
    }
}

static rsRetVal msgAddMetadataFromHttpHeader(smsg_t *const __restrict__ pMsg, struct conn_wrkr_s *connWrkr) {
    struct json_object *json = NULL;
    DEFiRet;
    const struct mg_request_info *ri = connWrkr->pri;
#define MAX_HTTP_HEADERS 64 /* hard limit */
    int count = min(ri->num_headers, MAX_HTTP_HEADERS);

    CHKmalloc(json = json_object_new_object());
    for (int i = 0; i < count; i++) {
        struct json_object *const jval = json_object_new_string(ri->http_headers[i].value);
        CHKmalloc(jval);
        /* truncate header names bigger than INIT_SCRATCH_BUF_SIZE */
        strncpy(connWrkr->pScratchBuf, ri->http_headers[i].name, connWrkr->scratchBufSize - 1);
        connWrkr->pScratchBuf[connWrkr->scratchBufSize - 1] = '\0';
        /* make header lowercase */
        char *pname = connWrkr->pScratchBuf;
        while (pname && *pname != '\0') {
            *pname = tolower((unsigned char)*pname);
            pname++;
        }
        json_object_object_add(json, (const char *const)connWrkr->pScratchBuf, jval);
    }
    CHKiRet(msgAddJSON(pMsg, (uchar *)"!metadata!httpheaders", json, 0, 0));

finalize_it:
    if (iRet != RS_RET_OK && json) {
        json_object_put(json);
    }
    RETiRet;
}

static rsRetVal msgAddMetadataFromHttpQueryParams(smsg_t *const __restrict__ pMsg, struct conn_wrkr_s *connWrkr) {
    struct json_object *json = NULL;
    DEFiRet;
    const struct mg_request_info *ri = connWrkr->pri;

    if (ri && ri->query_string) {
        strncpy(connWrkr->pScratchBuf, ri->query_string, connWrkr->scratchBufSize - 1);
        connWrkr->pScratchBuf[connWrkr->scratchBufSize - 1] = '\0';
        char *pquery_str = connWrkr->pScratchBuf;
        if (pquery_str) {
            CHKmalloc(json = json_object_new_object());

            char *saveptr = NULL;
            char *kv_pair = strtok_r(pquery_str, "&;", &saveptr);

            for (; kv_pair != NULL; kv_pair = strtok_r(NULL, "&;", &saveptr)) {
                char *saveptr2 = NULL;
                char *key = strtok_r(kv_pair, "=", &saveptr2);
                if (key) {
                    char *value = strtok_r(NULL, "=", &saveptr2);
                    struct json_object *const jval = json_object_new_string(value != NULL ? value : "");
                    CHKmalloc(jval);
                    json_object_object_add(json, (const char *)key, jval);
                }
            }
            CHKiRet(msgAddJSON(pMsg, (uchar *)"!metadata!queryparams", json, 0, 0));
        }
    }
finalize_it:
    if (iRet != RS_RET_OK && json) {
        json_object_put(json);
    }
    RETiRet;
}

static rsRetVal doSubmitMsg(const instanceConf_t *const __restrict__ inst,
                            struct conn_wrkr_s *connWrkr,
                            const uchar *msg,
                            size_t len) {
    smsg_t *pMsg = NULL;
    DEFiRet;

    assert(len <= s_iMaxLine);
    if (len == 0) {
        DBGPRINTF("discarding zero-sized message\n");
        FINALIZE;
    }

    CHKiRet(msgConstruct(&pMsg));
    MsgSetFlowControlType(pMsg, inst->flowControl ? eFLOWCTL_LIGHT_DELAY : eFLOWCTL_NO_DELAY);
    if (inst->pInputName) {
        MsgSetInputName(pMsg, inst->pInputName);
    } else {
        MsgSetInputName(pMsg, pInputName);
    }
    MsgSetRawMsg(pMsg, (const char *)msg, len);
    MsgSetMSGoffs(pMsg, 0); /* we do not have a header... */
    if (connWrkr->propRemoteAddr) {
        MsgSetRcvFromIP(pMsg, connWrkr->propRemoteAddr);
    }
    if (inst) {
        MsgSetRuleset(pMsg, inst->pBindRuleset);
    }
    // TODO: make these flags configurable.
    pMsg->msgFlags = NEEDS_PARSING | PARSE_HOSTNAME;

    if (inst->bAddMetadata) {
        CHKiRet(msgAddMetadataFromHttpHeader(pMsg, connWrkr));
        CHKiRet(msgAddMetadataFromHttpQueryParams(pMsg, connWrkr));
    }

    CHKiRet(ratelimitAddMsg(inst->ratelimiter, &connWrkr->multiSub, pMsg));
    STATSCOUNTER_INC(statsCounter.ctrSubmitted, statsCounter.mutCtrSubmitted);
finalize_it:
    connWrkr->iMsg = 0;
    if (iRet != RS_RET_OK) {
        if (pMsg != NULL && iRet != RS_RET_DISCARDMSG) {
            msgDestruct(&pMsg);
        }
        STATSCOUNTER_INC(statsCounter.ctrDiscarded, statsCounter.mutCtrDiscarded);
    }
    RETiRet;
}


static rsRetVal processOctetMsgLen(const instanceConf_t *const inst, struct conn_wrkr_s *connWrkr, char ch) {
    DEFiRet;
    if (connWrkr->parseState.inputState == eAtStrtFram) {
        if (inst->bSuppOctetFram && isdigit(ch)) {
            connWrkr->parseState.inputState = eInOctetCnt;
            connWrkr->parseState.iOctetsRemain = 0;
            connWrkr->parseState.framingMode = TCP_FRAMING_OCTET_COUNTING;
        } else {
            connWrkr->parseState.inputState = eInMsg;
            connWrkr->parseState.framingMode = TCP_FRAMING_OCTET_STUFFING;
        }
    }

    // parsing character.
    if (connWrkr->parseState.inputState == eInOctetCnt) {
        if (isdigit(ch)) {
            if (connWrkr->parseState.iOctetsRemain <= 200000000) {
                connWrkr->parseState.iOctetsRemain = connWrkr->parseState.iOctetsRemain * 10 + ch - '0';
            }
            // temporarily save this character into the message buffer
            if (connWrkr->iMsg + 1 < s_iMaxLine) {
                connWrkr->pMsg[connWrkr->iMsg++] = ch;
            }
        } else {
            const char *remoteAddr = "";
            if (connWrkr->propRemoteAddr) {
                remoteAddr = (const char *)propGetSzStr(connWrkr->propRemoteAddr);
            }

            /* handle space delimeter */
            if (ch != ' ') {
                LogError(0, NO_ERRCODE,
                         "Framing Error in received TCP message "
                         "from peer: (ip) %s: to input: %s, delimiter is not "
                         "SP but has ASCII value %d.",
                         remoteAddr, inst->pszInputName, ch);
            }

            if (connWrkr->parseState.iOctetsRemain < 1) {
                LogError(0, NO_ERRCODE,
                         "Framing Error in received TCP message"
                         " from peer: (ip) %s: delimiter is not "
                         "SP but has ASCII value %d.",
                         remoteAddr, ch);
            } else if (connWrkr->parseState.iOctetsRemain > s_iMaxLine) {
                DBGPRINTF("truncating message with %lu octets - max msg size is %lu\n",
                          connWrkr->parseState.iOctetsRemain, s_iMaxLine);
                LogError(0, NO_ERRCODE,
                         "received oversize message from peer: "
                         "(hostname) (ip) %s: size is %lu bytes, max msg "
                         "size is %lu, truncating...",
                         remoteAddr, connWrkr->parseState.iOctetsRemain, s_iMaxLine);
            }
            connWrkr->parseState.inputState = eInMsg;
        }
        /* reset msg len for actual message processing */
        connWrkr->iMsg = 0;
        /* retrieve next character */
    }
    RETiRet;
}

static rsRetVal processOctetCounting(const instanceConf_t *const inst,
                                     struct conn_wrkr_s *connWrkr,
                                     const char *buf,
                                     size_t len) {
    DEFiRet;
    const uchar *pbuf = (const uchar *)buf;
    const uchar *pbufLast = pbuf + len;

    while (pbuf < pbufLast) {
        char ch = *pbuf;

        if (connWrkr->parseState.inputState == eAtStrtFram || connWrkr->parseState.inputState == eInOctetCnt) {
            processOctetMsgLen(inst, connWrkr, ch);
            if (connWrkr->parseState.framingMode == TCP_FRAMING_OCTET_COUNTING) {
                pbuf++;
            }
        } else if (connWrkr->parseState.inputState == eInMsg) {
            if (connWrkr->parseState.framingMode == TCP_FRAMING_OCTET_STUFFING) {
                if (connWrkr->iMsg < s_iMaxLine) {
                    if (ch == '\n') {
                        doSubmitMsg(inst, connWrkr, connWrkr->pMsg, connWrkr->iMsg);
                        connWrkr->parseState.inputState = eAtStrtFram;
                    } else {
                        connWrkr->pMsg[connWrkr->iMsg++] = ch;
                    }
                } else {
                    doSubmitMsg(inst, connWrkr, connWrkr->pMsg, connWrkr->iMsg);
                    connWrkr->parseState.inputState = eAtStrtFram;
                }
                pbuf++;
            } else {
                assert(connWrkr->parseState.framingMode == TCP_FRAMING_OCTET_COUNTING);
                /* parsing payload */
                size_t remainingBytes = pbufLast - pbuf;
                // figure out how much is in block
                size_t count = min(connWrkr->parseState.iOctetsRemain, remainingBytes);
                if (connWrkr->iMsg + count >= s_iMaxLine) {
                    count = s_iMaxLine - connWrkr->iMsg;
                }

                // just copy the bytes
                if (count) {
                    memcpy(connWrkr->pMsg + connWrkr->iMsg, pbuf, count);
                    pbuf += count;
                    connWrkr->iMsg += count;
                    connWrkr->parseState.iOctetsRemain -= count;
                }

                if (connWrkr->parseState.iOctetsRemain == 0) {
                    doSubmitMsg(inst, connWrkr, connWrkr->pMsg, connWrkr->iMsg);
                    connWrkr->parseState.inputState = eAtStrtFram;
                }
            }
        } else {
            // unexpected
            assert(0);
            break;
        }
    }
    RETiRet;
}

static rsRetVal processDisableLF(const instanceConf_t *const inst,
                                 struct conn_wrkr_s *connWrkr,
                                 const char *buf,
                                 size_t len) {
    DEFiRet;
    const uchar *pbuf = (const uchar *)buf;
    size_t remainingBytes = len;
    const uchar *pbufLast = pbuf + len;

    while (pbuf < pbufLast) {
        size_t count = 0;
        if (connWrkr->iMsg + remainingBytes >= s_iMaxLine) {
            count = s_iMaxLine - connWrkr->iMsg;
        } else {
            count = remainingBytes;
        }

        if (count) {
            memcpy(connWrkr->pMsg + connWrkr->iMsg, pbuf, count);
            pbuf += count;
            connWrkr->iMsg += count;
            remainingBytes -= count;
        }
        doSubmitMsg(inst, connWrkr, connWrkr->pMsg, connWrkr->iMsg);
    }
    RETiRet;
}

static rsRetVal processDataUncompressed(const instanceConf_t *const inst,
                                        struct conn_wrkr_s *connWrkr,
                                        const char *buf,
                                        size_t len) {
    const uchar *pbuf = (const uchar *)buf;
    DEFiRet;

    if (inst->bDisableLFDelim) {
        /* do block processing */
        iRet = processDisableLF(inst, connWrkr, buf, len);
    } else if (inst->bSuppOctetFram) {
        iRet = processOctetCounting(inst, connWrkr, buf, len);
    } else {
        const uchar *pbufLast = pbuf + len;
        while (pbuf < pbufLast) {
            char ch = *pbuf;
            if (connWrkr->iMsg < s_iMaxLine) {
                if (ch == '\n') {
                    doSubmitMsg(inst, connWrkr, connWrkr->pMsg, connWrkr->iMsg);
                } else {
                    connWrkr->pMsg[connWrkr->iMsg++] = ch;
                }
            } else {
                doSubmitMsg(inst, connWrkr, connWrkr->pMsg, connWrkr->iMsg);
            }
            pbuf++;
        }
    }
    RETiRet;
}

static rsRetVal processDataCompressed(const instanceConf_t *const inst,
                                      struct conn_wrkr_s *connWrkr,
                                      const char *buf,
                                      size_t len) {
    DEFiRet;

    if (!connWrkr->parseState.bzInitDone) {
        /* allocate deflate state */
        connWrkr->parseState.zstrm.zalloc = Z_NULL;
        connWrkr->parseState.zstrm.zfree = Z_NULL;
        connWrkr->parseState.zstrm.opaque = Z_NULL;
        int rc = inflateInit2(&connWrkr->parseState.zstrm, (MAX_WBITS | 16));
        if (rc != Z_OK) {
            dbgprintf("imhttp: error %d returned from zlib/inflateInit()\n", rc);
            ABORT_FINALIZE(RS_RET_ZLIB_ERR);
        }
        connWrkr->parseState.bzInitDone = 1;
    }

    connWrkr->parseState.zstrm.next_in = (Bytef *)buf;
    connWrkr->parseState.zstrm.avail_in = len;
    /* run inflate() on buffer until everything has been uncompressed */
    int outtotal = 0;
    do {
        int zRet = 0;
        int outavail = 0;
        dbgprintf("imhttp: in inflate() loop, avail_in %d, total_in %ld\n", connWrkr->parseState.zstrm.avail_in,
                  connWrkr->parseState.zstrm.total_in);

        connWrkr->parseState.zstrm.avail_out = sizeof(connWrkr->zipBuf);
        connWrkr->parseState.zstrm.next_out = connWrkr->zipBuf;
        zRet = inflate(&connWrkr->parseState.zstrm, Z_SYNC_FLUSH);
        dbgprintf("imhttp: inflate(), ret: %d, avail_out: %d\n", zRet, connWrkr->parseState.zstrm.avail_out);
        outavail = sizeof(connWrkr->zipBuf) - connWrkr->parseState.zstrm.avail_out;
        if (outavail != 0) {
            outtotal += outavail;
            CHKiRet(processDataUncompressed(inst, connWrkr, (const char *)connWrkr->zipBuf, outavail));
        }
    } while (connWrkr->parseState.zstrm.avail_out == 0);

    dbgprintf("imhttp: processDataCompressed complete, sizes: in %lld, out %llu\n", (long long)len,
              (long long unsigned)outtotal);

finalize_it:
    RETiRet;
}

static rsRetVal processData(const instanceConf_t *const inst,
                            struct conn_wrkr_s *connWrkr,
                            const char *buf,
                            size_t len) {
    DEFiRet;

    // inst->bDisableLFDelim = 0;
    if (connWrkr->parseState.content_compressed) {
        iRet = processDataCompressed(inst, connWrkr, buf, len);
    } else {
        iRet = processDataUncompressed(inst, connWrkr, buf, len);
    }

    RETiRet;
}

static const char *skipWhitespace(const char *str) {
    while (str != NULL && isspace((unsigned char)*str)) {
        ++str;
    }
    return str;
}

/**
 * Parse a Basic-auth header into the reusable auth scratch state.
 *
 * @param conn CivetWeb request connection
 * @param auth caller-owned auth scratch state
 * @return non-zero when the header was present and decodes to user:password
 *
 * @note The decoded buffer is reused across calls so the parser can avoid
 *       extra allocations for the common in-buffer case.
 */
static sbool parse_basic_auth_header(struct mg_connection *conn, struct auth_s *auth) {
    if (!auth || !conn) {
        return 0;
    }

    const char *auth_header = NULL;
    if (((auth_header = mg_get_header(conn, "Authorization")) == NULL) || strncasecmp(auth_header, "Basic ", 6) != 0) {
        return 0;
    }

    const char *src = auth_header + 6;
    size_t encoded_len = strlen(src);
    if (encoded_len > BASIC_AUTH_MAX_ENCODED) {
        return 0;
    }

    size_t len = apr_base64_decode(auth->workbuf, src);
    if (len == 0 || len > BASIC_AUTH_MAX_DECODED) {
        return 0;
    }
    if (memchr(auth->workbuf, '\0', len) != NULL) {
        return 0;
    }
    auth->workbuf[len] = '\0';

    char *passwd = NULL, *saveptr = NULL;
    char *user = strtok_r(auth->workbuf, ":", &saveptr);
    if (user) {
        passwd = strtok_r(NULL, ":", &saveptr);
    }

    auth->pszUser = user;
    auth->pszPasswd = passwd;

    return (auth->pszUser != NULL && auth->pszPasswd != NULL);
}

/**
 * Parse an Elastic-style API-key header from the request.
 *
 * @param conn CivetWeb request connection
 * @param auth parsed auth output
 * @return non-zero when a usable API key token was found
 *
 * @note Accept both Authorization: ApiKey and X-API-Key because Elastic
 *       agents and intermediaries do not always use the same header form.
 *       Leading whitespace after the scheme is ignored for robustness.
 */
static sbool parse_api_key_header(struct mg_connection *conn, struct auth_s *auth) {
    if (!auth || !conn) {
        return 0;
    }

    const char *auth_header = mg_get_header(conn, "Authorization");
    if (auth_header != NULL && strncasecmp(auth_header, "ApiKey ", 7) == 0) {
        auth->pszApiKey = skipWhitespace(auth_header + 7);
        return (auth->pszApiKey != NULL && auth->pszApiKey[0] != '\0');
    }

    auth->pszApiKey = skipWhitespace(mg_get_header(conn, "X-API-Key"));
    return (auth->pszApiKey != NULL && auth->pszApiKey[0] != '\0');
}

/**
 * Validate a Basic-auth credential pair against an htpasswd file.
 *
 * @param filep open htpasswd file
 * @param auth parsed Basic-auth credentials
 * @return non-zero if the first matching user/password pair validates
 *
 * @note Comments and blank lines are ignored so the file can stay human
 *       maintainable without affecting auth behavior.
 */
static sbool read_auth_file(FILE *filep, struct auth_s *auth) {
    if (filep == NULL || auth == NULL || auth->pszUser == NULL || auth->pszPasswd == NULL) {
        return 0;
    }

    char workbuf[IMHTTP_MAX_BUF_LEN];
    size_t l = 0;
    char *user;
    char *passwd;

    while (fgets(workbuf, sizeof(workbuf), filep)) {
        l = strnlen(workbuf, sizeof(workbuf));
        while (l > 0) {
            if (isspace((unsigned char)workbuf[l - 1]) || iscntrl((unsigned char)workbuf[l - 1])) {
                workbuf[--l] = '\0';
            } else {
                break;
            }
        }

        if (l < 1 || workbuf[0] == '#') {
            continue;
        }

        user = workbuf;
        passwd = strchr(workbuf, ':');
        if (!passwd) {
            continue;
        }
        *passwd++ = '\0';

        if (!strcasecmp(auth->pszUser, user)) {
            return (apr_password_validate(auth->pszPasswd, passwd) == APR_SUCCESS);
        }
    }
    return 0;
}

/**
 * Validate an API key against a token file.
 *
 * @param filep open token file
 * @param presentedApiKey key extracted from the request
 * @return non-zero if the first matching token validates
 *
 * @note The file format is deliberately one token per line so the auth path
 *       stays a simple string compare instead of a parser.
 */
static sbool read_api_key_file(FILE *filep, const char *presentedApiKey) {
    char workbuf[IMHTTP_MAX_BUF_LEN];
    size_t l = 0;

    if (filep == NULL || presentedApiKey == NULL || presentedApiKey[0] == '\0') {
        return 0;
    }

    while (fgets(workbuf, sizeof(workbuf), filep)) {
        l = strnlen(workbuf, sizeof(workbuf));
        while (l > 0) {
            if (isspace((unsigned char)workbuf[l - 1]) || iscntrl((unsigned char)workbuf[l - 1])) {
                workbuf[--l] = '\0';
            } else {
                break;
            }
        }

        if (l < 1 || workbuf[0] == '#') {
            continue;
        }

        if (strcmp(presentedApiKey, workbuf) == 0) {
            return 1;
        }
    }

    return 0;
}

/**
 * Open an auth file and report failures through the rsyslog error path.
 *
 * @param conn CivetWeb request connection used for request-scoped logging
 * @param authFile path to the auth file
 * @return open FILE handle, or NULL on error
 *
 * @note LogError() is used so errno expansion flows through rsyslog's normal
 *       message formatting instead of hand-formatting strerror strings here.
 */
static FILE *openAuthFile(struct mg_connection *conn, const char *authFile) {
    FILE *fp = NULL;

    if (authFile == NULL) {
        mg_cry(conn, "warning: auth file not configured for this endpoint.\n");
        return NULL;
    }

    fp = fopen(authFile, "r");
    if (fp == NULL) {
        LogError(errno, RS_RET_NO_FILE_ACCESS, "imhttp: auth file '%s' could not be accessed", authFile);
    }
    return fp;
}

/**
 * Validate a Basic-auth request against an htpasswd file.
 *
 * @param conn CivetWeb request connection
 * @param authFile htpasswd file path
 * @return non-zero on successful authorization
 *
 * @note This preserves the pre-existing Basic-auth contract while sharing the
 *       file-open path with API-key auth.
 */
static sbool authorize_basic(struct mg_connection *conn, const char *authFile) {
    sbool ret = 0;
    FILE *fp = NULL;
    struct auth_s auth = {.pszUser = NULL, .pszPasswd = NULL, .pszApiKey = NULL};

    fp = openAuthFile(conn, authFile);
    if (fp == NULL) {
        goto finalize;
    }

    if (!parse_basic_auth_header(conn, &auth)) {
        goto finalize;
    }

    ret = read_auth_file(fp, &auth);

finalize:
    if (fp != NULL) {
        fclose(fp);
    }
    return ret;
}

/**
 * Validate an API-key request against a token file.
 *
 * @param conn CivetWeb request connection
 * @param authFile API-key file path
 * @return non-zero on successful authorization
 *
 * @note One token per line keeps matching deterministic and avoids parser
 *       ambiguity in the auth path.
 */
static sbool authorize_api_key(struct mg_connection *conn, const char *authFile) {
    sbool ret = 0;
    FILE *fp = NULL;
    struct auth_s auth = {.pszUser = NULL, .pszPasswd = NULL, .pszApiKey = NULL};

    fp = openAuthFile(conn, authFile);
    if (fp == NULL) {
        goto finalize;
    }

    if (!parse_api_key_header(conn, &auth)) {
        goto finalize;
    }

    ret = read_api_key_file(fp, auth.pszApiKey);

finalize:
    if (fp != NULL) {
        fclose(fp);
    }
    return ret;
}

/**
 * Send the 401 response matching the route's configured auth scheme.
 *
 * @param conn CivetWeb request connection
 * @param authCtx route-local auth state
 *
 * @note Basic auth advertises a challenge header so standard clients can retry.
 *       API-key-only routes intentionally avoid a browser-oriented challenge.
 */
static void sendUnauthorized(struct mg_connection *conn, const struct route_auth_ctx_s *authCtx) {
    if (authCtx != NULL && authCtx->pszBasicAuthFile != NULL) {
        mg_send_http_error(conn, 401, "WWW-Authenticate: Basic realm=\"User Visible Realm\"\n");
    } else {
        mg_printf(conn, "HTTP/1.1 401 Unauthorized\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
    }
}

/**
 * CivetWeb auth callback for imhttp routes.
 *
 * @param conn CivetWeb request connection
 * @param cbdata route-local auth context
 * @return non-zero when the request is authorized
 *
 * @note When both schemes are configured, explicit Basic auth wins first so we
 *       preserve the existing htpasswd behavior; API key auth is the fallback.
 */
static int routeAuthHandler(struct mg_connection *conn, void *cbdata) {
    const struct route_auth_ctx_s *authCtx = (const struct route_auth_ctx_s *)cbdata;
    const char *authHeader = mg_get_header(conn, "Authorization");
    int ret = 0;

    if (!routeAuthConfigured(authCtx)) {
        return 1;
    }

    if (authCtx->pszBasicAuthFile != NULL && authHeader != NULL && strncasecmp(authHeader, "Basic ", 6) == 0) {
        ret = authorize_basic(conn, authCtx->pszBasicAuthFile);
    } else if (authCtx->pszApiKeyFile != NULL) {
        ret = authorize_api_key(conn, authCtx->pszApiKeyFile);
    }

    if (!ret) {
        sendUnauthorized(conn, authCtx);
    }

    return ret;
}

/**
 * Register a request handler and attach auth handling when needed.
 *
 * @param ctx CivetWeb context
 * @param path request path
 * @param handler request handler
 * @param handlerCbdata handler-specific callback data
 * @param authCtx optional route-local auth context
 *
 * @note Keeping handler and auth registration together makes it easier to add
 *       more mock endpoints later without duplicating the wiring rules.
 */
static void registerRoute(struct mg_context *ctx,
                          const char *path,
                          int (*handler)(struct mg_connection *, void *),
                          void *handlerCbdata,
                          struct route_auth_ctx_s *authCtx) {
    if (ctx == NULL || path == NULL || path[0] == '\0') {
        return;
    }

    mg_set_request_handler(ctx, path, handler, handlerCbdata);
    if (routeAuthConfigured(authCtx)) {
        mg_set_auth_handler(ctx, path, routeAuthHandler, authCtx);
    }
}

/* cbdata should actually contain instance data and we can actually use this instance data
 * to hold reusable scratch buffer.
 */
static int postHandler(struct mg_connection *conn, void *cbdata) {
    int rc = 1;
    rsRetVal localRet = RS_RET_OK;
    instanceConf_t *inst = (instanceConf_t *)cbdata;
    const struct mg_request_info *ri = mg_get_request_info(conn);
    struct conn_wrkr_s *connWrkr = mg_get_thread_pointer(conn);
    connWrkr->multiSub.nElem = 0;
    memset(&connWrkr->parseState, 0, sizeof(connWrkr->parseState));
    connWrkr->pri = ri;

    if (inst->bAddMetadata && connWrkr->scratchBufSize == 0) {
        connWrkr->pScratchBuf = calloc(1, INIT_SCRATCH_BUF_SIZE);
        if (!connWrkr->pScratchBuf) {
            mg_cry(conn, "%s() - could not alloc scratch buffer!\n", __FUNCTION__);
            rc = 500;
            FINALIZE;
        }
        connWrkr->scratchBufSize = INIT_SCRATCH_BUF_SIZE;
    }

    if (0 != strcmp(ri->request_method, "POST")) {
        /* Not a POST request */
        int ret = mg_get_request_link(conn, connWrkr->pReadBuf, connWrkr->readBufSize);
        mg_printf(conn, "HTTP/1.1 405 Method Not Allowed\r\nConnection: close\r\n");
        mg_printf(conn, "Content-Type: text/plain\r\n\r\n");
        mg_printf(conn, "%s method not allowed in the POST handler\n", ri->request_method);
        if (ret >= 0) {
            mg_printf(conn, "use a web tool to send a POST request to %s\n", connWrkr->pReadBuf);
        }
        STATSCOUNTER_INC(statsCounter.ctrFailed, statsCounter.mutCtrFailed);
        rc = 405;
        FINALIZE;
    }

    if (ri->remote_addr[0] != '\0') {
        size_t len = strnlen(ri->remote_addr, sizeof(ri->remote_addr));
        prop.CreateOrReuseStringProp(&connWrkr->propRemoteAddr, (const uchar *)ri->remote_addr, len);
    }

    if (ri->content_length >= 0) {
        /* We know the content length in advance */
        if (ri->content_length > (long long)connWrkr->readBufSize) {
            char *newReadBuf = realloc(connWrkr->pReadBuf, ri->content_length + 1);
            if (newReadBuf == NULL) {
                mg_cry(conn, "%s() - realloc failed!\n", __FUNCTION__);
                rc = 500;
                FINALIZE;
            }
            connWrkr->pReadBuf = newReadBuf;
            connWrkr->readBufSize = ri->content_length + 1;
        }
    } else {
        /* We must read until we find the end (chunked encoding
         * or connection close), indicated my mg_read returning 0 */
    }

    if (ri->num_headers > 0) {
        int i;
        for (i = 0; i < ri->num_headers; i++) {
            if (!strcasecmp(ri->http_headers[i].name, "content-encoding") &&
                !strcasecmp(ri->http_headers[i].value, "gzip")) {
                connWrkr->parseState.content_compressed = 1;
            }
        }
    }

    while (1) {
        int count = mg_read(conn, connWrkr->pReadBuf, connWrkr->readBufSize);
        if (count > 0) {
            localRet = processData(inst, connWrkr, (const char *)connWrkr->pReadBuf, count);
            if (localRet != RS_RET_OK) {
                LogError(0, localRet, "imhttp: failed processing request payload");
                STATSCOUNTER_INC(statsCounter.ctrFailed, statsCounter.mutCtrFailed);
                rc = 500;
                FINALIZE;
            }
        } else {
            break;
        }
    }

    /* submit remainder */
    localRet = doSubmitMsg(inst, connWrkr, connWrkr->pMsg, connWrkr->iMsg);
    if (localRet != RS_RET_OK) {
        LogError(0, localRet, "imhttp: failed submitting final request message");
        STATSCOUNTER_INC(statsCounter.ctrFailed, statsCounter.mutCtrFailed);
        rc = 500;
        FINALIZE;
    }
    localRet = multiSubmitFlush(&connWrkr->multiSub);
    if (localRet != RS_RET_OK) {
        LogError(0, localRet, "imhttp: failed flushing submitted messages");
        STATSCOUNTER_INC(statsCounter.ctrFailed, statsCounter.mutCtrFailed);
        rc = 500;
        FINALIZE;
    }

    mg_send_http_ok(conn, "text/plain", 0);
    rc = 200;

finalize_it:
    if (connWrkr->parseState.bzInitDone) {
        inflateEnd(&connWrkr->parseState.zstrm);
    }
    /* reset */
    connWrkr->iMsg = 0;

    return rc;
}


/* Health Check Handler */
static int health_check_handler(struct mg_connection *conn, ATTR_UNUSED void *cbdata) {
    const char *body = "OK\n";
    mg_send_http_ok(conn, "text/plain; charset=utf-8", strlen(body));
    mg_write(conn, body, strlen(body));
    return 200;  // CivetWeb expects handler to return HTTP status or specific flags
}


/*
 * prometheus_metrics_handler
 * Export rsyslog statistics in Prometheus text format.
 */
struct stats_buf {
    char *buf;
    size_t len;
    size_t cap;
};

static rsRetVal prom_stats_collect(void *usrptr, const char *line) {
    struct stats_buf *sb = (struct stats_buf *)usrptr;
    const size_t line_len = strlen(line);
    if (sb->len + line_len >= sb->cap) {
        size_t newcap = sb->cap ? sb->cap * 2 : 1024;
        while (newcap <= sb->len + line_len) {
            if (newcap > ((size_t)-1) / 2) {
                return RS_RET_OUT_OF_MEMORY;
            }
            newcap *= 2;
        }
        char *tmp = realloc(sb->buf, newcap);
        if (tmp == NULL) {
            return RS_RET_OUT_OF_MEMORY;
        }
        sb->buf = tmp;
        sb->cap = newcap;
    }
    memcpy(sb->buf + sb->len, line, line_len);
    sb->len += line_len;
    sb->buf[sb->len] = '\0';
    return RS_RET_OK;
}

static int prometheus_metrics_handler(struct mg_connection *conn, ATTR_UNUSED void *cbdata) {
    struct stats_buf sb = {.buf = NULL, .len = 0, .cap = 0};
    rsRetVal ret = RS_RET_OK;
    int http_status = 200;
    const char *imhttp_up_metric =
        "# HELP imhttp_up Indicates if the imhttp module is operational (1 for up, 0 for down).\n"
        "# TYPE imhttp_up gauge\n"
        "imhttp_up 1\n";

    ret = prom_stats_collect(&sb, imhttp_up_metric);
    if (ret != RS_RET_OK) {
        LogError(0, RS_RET_OUT_OF_MEMORY, "imhttp: failed to allocate initial buffer for statistics");
        http_status = 500;
        goto finalize;
    }

    ret = statsobj.GetAllStatsLines(prom_stats_collect, &sb, statsFmt_Prometheus, 0);
    if (ret != RS_RET_OK) {
        LogError(0, ret, "imhttp: failed to retrieve statistics");
        http_status = 500;
        goto finalize;
    }

    mg_printf(conn,
              "HTTP/1.1 200 OK\r\n"
              "Content-Type: text/plain; version=0.0.4; charset=utf-8\r\n"
              "Content-Length: %zu\r\n"
              "Connection: close\r\n"
              "\r\n",
              sb.len);

    mg_write(conn, sb.buf, sb.len);

finalize:
    if (http_status != 200) {
        mg_printf(conn, "HTTP/1.1 500 Internal Server Error\r\nConnection: close\r\n\r\n");
    }
    free(sb.buf);
    return http_status;
}


static int runloop(void) {
    dbgprintf("imhttp started.\n");

    /* Register handlers under context lock to avoid civetweb races */
    mg_lock_context(s_httpserv->ctx);

    for (instanceConf_t *inst = runModConf->root; inst != NULL; inst = inst->next) {
        assert(inst->pszEndpoint);
        if (inst->pszEndpoint) {
            dbgprintf("setting request handler: '%s'\n", inst->pszEndpoint);
            routeAuthCtxSet(&inst->authCtx, (const char *)inst->pszBasicAuthFile, (const char *)inst->pszApiKeyFile);
            registerRoute(s_httpserv->ctx, (const char *)inst->pszEndpoint, postHandler, inst, &inst->authCtx);
        }
    }

    if (runModConf->pszHealthCheckPath && runModConf->pszHealthCheckPath[0] != '\0') {
        dbgprintf("imhttp: setting request handler for global health check: '%s'\n", runModConf->pszHealthCheckPath);
        routeAuthCtxSet(&runModConf->healthCheckAuthCtx, runModConf->pszHealthCheckAuthFile,
                        runModConf->pszHealthCheckApiKeyFile);
        registerRoute(s_httpserv->ctx, runModConf->pszHealthCheckPath, health_check_handler, NULL,
                      &runModConf->healthCheckAuthCtx);
    }

    if (runModConf->pszMetricsPath && runModConf->pszMetricsPath[0] != '\0') {
        dbgprintf("imhttp: setting request handler for Prometheus metrics: '%s'\n", runModConf->pszMetricsPath);
        routeAuthCtxSet(&runModConf->metricsAuthCtx, runModConf->pszMetricsAuthFile, runModConf->pszMetricsApiKeyFile);
        registerRoute(s_httpserv->ctx, runModConf->pszMetricsPath, prometheus_metrics_handler,
                      NULL /* cbdata, not used */, &runModConf->metricsAuthCtx);
    }

    mg_unlock_context(s_httpserv->ctx);

    /* Wait until the server should be closed */
    while (glbl.GetGlobalInputTermState() == 0) {
        sleep(1);
    }
    return EXIT_SUCCESS;
}

BEGINnewInpInst
    struct cnfparamvals *pvals;
    instanceConf_t *inst;
    int i;
    CODESTARTnewInpInst;
    DBGPRINTF("newInpInst (imhttp)\n");
    pvals = nvlstGetParams(lst, &inppblk, NULL);
    if (pvals == NULL) {
        LogError(0, RS_RET_MISSING_CNFPARAMS, "imhttp: required parameter are missing\n");
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }

    if (Debug) {
        dbgprintf("input param blk in imtcp:\n");
        cnfparamsPrint(&inppblk, pvals);
    }

    CHKiRet(createInstance(&inst));

    for (i = 0; i < inppblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;
        if (!strcmp(inppblk.descr[i].name, "endpoint")) {
            inst->pszEndpoint = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(inppblk.descr[i].name, "basicauthfile")) {
            inst->pszBasicAuthFile = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(inppblk.descr[i].name, "apikeyfile")) {
            inst->pszApiKeyFile = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(inppblk.descr[i].name, "ruleset")) {
            inst->pszBindRuleset = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(inppblk.descr[i].name, "name")) {
            inst->pszInputName = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(inppblk.descr[i].name, "ratelimit.burst")) {
            inst->ratelimitBurst = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "ratelimit.interval")) {
            inst->ratelimitInterval = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "ratelimit.name")) {
            inst->pszRatelimitName = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(inppblk.descr[i].name, "flowcontrol")) {
            inst->flowControl = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "disablelfdelimiter")) {
            inst->bDisableLFDelim = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "supportoctetcountedframing")) {
            inst->bSuppOctetFram = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "addmetadata")) {
            inst->bAddMetadata = (int)pvals[i].val.d.n;
        } else {
            dbgprintf(
                "imhttp: program error, non-handled "
                "param '%s'\n",
                inppblk.descr[i].name);
        }
    }

    if (inst->pszRatelimitName != NULL) {
        if (inst->ratelimitInterval != -1 || inst->ratelimitBurst != -1) {
            LogError(0, RS_RET_INVALID_PARAMS,
                     "imhttp: ratelimit.name is mutually exclusive with "
                     "ratelimit.interval and ratelimit.burst - using ratelimit.name");
        }
        inst->ratelimitInterval = 0;
        inst->ratelimitBurst = 0;
    } else {
        if (inst->ratelimitInterval == -1) inst->ratelimitInterval = 0;
        if (inst->ratelimitBurst == -1) inst->ratelimitBurst = 10000;
    }

    if (inst->pszInputName) {
        CHKiRet(prop.Construct(&inst->pInputName));
        CHKiRet(prop.SetString(inst->pInputName, inst->pszInputName, ustrlen(inst->pszInputName)));
        CHKiRet(prop.ConstructFinalize(inst->pInputName));
    }
    if (inst->pszRatelimitName != NULL) {
        CHKiRet(ratelimitNewFromConfig(&inst->ratelimiter, loadModConf->pConf, (char *)inst->pszRatelimitName, "imhttp",
                                       NULL));
    } else {
        CHKiRet(ratelimitNew(&inst->ratelimiter, "imhttp", NULL));
        ratelimitSetLinuxLike(inst->ratelimiter, (unsigned)inst->ratelimitInterval, (unsigned)inst->ratelimitBurst);
    }

finalize_it:
    CODE_STD_FINALIZERnewInpInst cnfparamvalsDestruct(pvals, &inppblk);
ENDnewInpInst


BEGINbeginCnfLoad
    CODESTARTbeginCnfLoad;
    loadModConf = pModConf;
    pModConf->pConf = pConf;
    loadModConf->ports.name = NULL;
    loadModConf->docroot.name = NULL;
    loadModConf->nOptions = 0;
    loadModConf->options = NULL;
    loadModConf->pszHealthCheckAuthFile = NULL;
    loadModConf->pszMetricsAuthFile = NULL;
    loadModConf->pszHealthCheckApiKeyFile = NULL;
    loadModConf->pszMetricsApiKeyFile = NULL;
ENDbeginCnfLoad


BEGINsetModCnf
    struct cnfparamvals *pvals = NULL;
    CODESTARTsetModCnf;
    pvals = nvlstGetParams(lst, &modpblk, NULL);
    if (pvals == NULL) {
        LogError(0, RS_RET_MISSING_CNFPARAMS,
                 "imhttp: error processing module "
                 "config parameters [module(...)]");
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }

    if (Debug) {
        dbgprintf("module (global) param blk for imhttp:\n");
        cnfparamsPrint(&modpblk, pvals);
    }

    for (int i = 0; i < modpblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;
        if (!strcmp(modpblk.descr[i].name, "ports")) {
            assert(loadModConf->ports.name == NULL);
            assert(loadModConf->ports.val == NULL);
            loadModConf->ports.name = strdup(CIVETWEB_OPTION_NAME_PORTS);
            loadModConf->ports.val = es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(modpblk.descr[i].name, "documentroot")) {
            assert(loadModConf->docroot.name == NULL);
            assert(loadModConf->docroot.val == NULL);
            loadModConf->docroot.name = strdup(CIVETWEB_OPTION_NAME_DOCUMENT_ROOT);
            loadModConf->docroot.val = es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(modpblk.descr[i].name, "liboptions")) {
            loadModConf->nOptions = pvals[i].val.d.ar->nmemb;
            CHKmalloc(loadModConf->options = malloc(sizeof(struct option) * pvals[i].val.d.ar->nmemb));
            for (int j = 0; j < pvals[i].val.d.ar->nmemb; ++j) {
                char *cstr = es_str2cstr(pvals[i].val.d.ar->arr[j], NULL);
                CHKiRet(processCivetwebOptions(cstr, &loadModConf->options[j].name, &loadModConf->options[j].val));
                free(cstr);
            }
        } else if (!strcmp(modpblk.descr[i].name, "healthcheckpath")) {
            free(loadModConf->pszHealthCheckPath);
            loadModConf->pszHealthCheckPath = es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(modpblk.descr[i].name, "metricspath")) {
            free(loadModConf->pszMetricsPath);
            loadModConf->pszMetricsPath = es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(modpblk.descr[i].name, "healthcheckbasicauthfile")) {
            free(loadModConf->pszHealthCheckAuthFile);
            loadModConf->pszHealthCheckAuthFile = es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(modpblk.descr[i].name, "metricsbasicauthfile")) {
            free(loadModConf->pszMetricsAuthFile);
            loadModConf->pszMetricsAuthFile = es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(modpblk.descr[i].name, "healthcheckapikeyfile")) {
            free(loadModConf->pszHealthCheckApiKeyFile);
            loadModConf->pszHealthCheckApiKeyFile = es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(modpblk.descr[i].name, "metricsapikeyfile")) {
            free(loadModConf->pszMetricsApiKeyFile);
            loadModConf->pszMetricsApiKeyFile = es_str2cstr(pvals[i].val.d.estr, NULL);
        } else {
            dbgprintf(
                "imhttp: program error, non-handled "
                "param '%s' in beginCnfLoad\n",
                modpblk.descr[i].name);
        }
    }

    /* Set defaults if not configured but enabled */
    // TODO: add ability to disable functionality, especially important if we
    // use this solely for health checking etc
    if (loadModConf->pszHealthCheckPath == NULL) {
        CHKmalloc(loadModConf->pszHealthCheckPath = strdup(DEFAULT_HEALTH_CHECK_PATH));
    }

    if (loadModConf->pszMetricsPath == NULL) {
        CHKmalloc(loadModConf->pszMetricsPath = strdup(DEFAULT_PROM_METRICS_PATH));
    }

finalize_it:
    if (pvals != NULL) cnfparamvalsDestruct(pvals, &modpblk);
ENDsetModCnf


BEGINendCnfLoad
    CODESTARTendCnfLoad;
    loadModConf = NULL; /* done loading */
ENDendCnfLoad

/* function to generate error message if framework does not find requested ruleset */
static inline void std_checkRuleset_genErrMsg(ATTR_UNUSED modConfData_t *modConf, instanceConf_t *inst) {
    LogError(0, NO_ERRCODE,
             "imhttp: ruleset '%s' for %s not found - "
             "using default ruleset instead",
             inst->pszBindRuleset, inst->pszEndpoint);
}

BEGINcheckCnf
    instanceConf_t *inst;
    CODESTARTcheckCnf;
    for (inst = pModConf->root; inst != NULL; inst = inst->next) {
        std_checkRuleset(pModConf, inst);
    }
    /* verify civetweb options are valid */
    const struct mg_option *valid_opts = mg_get_valid_options();
    for (int i = 0; i < pModConf->nOptions; ++i) {
        if (!valid_civetweb_option(valid_opts, pModConf->options[i].name)) {
            LogError(0, RS_RET_CONF_PARSE_WARNING,
                     "imhttp: module loaded, but "
                     "invalid civetweb option found - imhttp may not receive connections.");
            iRet = RS_RET_CONF_PARSE_WARNING;
        }
    }

    if (pModConf->pszHealthCheckPath == NULL || pModConf->pszHealthCheckPath[0] != '/') {
        LogError(0, RS_RET_CONF_PARSE_WARNING,
                 "imhttp: healthCheckPath '%s' is invalid, must start with '/'. Using default '%s'.",
                 pModConf->pszHealthCheckPath ? pModConf->pszHealthCheckPath : "(null)", DEFAULT_HEALTH_CHECK_PATH);
        free(pModConf->pszHealthCheckPath);
        pModConf->pszHealthCheckPath = strdup(DEFAULT_HEALTH_CHECK_PATH);
    }

    if (pModConf->pszMetricsPath == NULL || pModConf->pszMetricsPath[0] != '/') {
        LogError(0, RS_RET_CONF_PARSE_WARNING,
                 "imhttp: metricsPath '%s' is invalid, must start with '/'. Using default '%s'.",
                 pModConf->pszMetricsPath ? pModConf->pszMetricsPath : "(null)", DEFAULT_PROM_METRICS_PATH);
        free(pModConf->pszMetricsPath);
        pModConf->pszMetricsPath = strdup(DEFAULT_PROM_METRICS_PATH);
    }

ENDcheckCnf


BEGINactivateCnf
    CODESTARTactivateCnf;
    runModConf = pModConf;

    if (!s_httpserv) {
        CHKmalloc(s_httpserv = calloc(1, sizeof(httpserv_t)));
    }
    /* options represents (key, value) so allocate 2x, and null terminated */
    size_t count = 1;
    if (runModConf->ports.val) {
        count += 2;
    }
    if (runModConf->docroot.val) {
        count += 2;
    }
    count += (2 * runModConf->nOptions);
    CHKmalloc(s_httpserv->civetweb_options = calloc(count, sizeof(*s_httpserv->civetweb_options)));

    const char **pcivetweb_options = s_httpserv->civetweb_options;
    if (runModConf->nOptions) {
        s_httpserv->civetweb_options_count = count;
        for (int i = 0; i < runModConf->nOptions; ++i) {
            *pcivetweb_options = runModConf->options[i].name;
            pcivetweb_options++;
            *pcivetweb_options = runModConf->options[i].val;
            pcivetweb_options++;
        }
    }
    /* append port, docroot */
    if (runModConf->ports.val) {
        *pcivetweb_options = runModConf->ports.name;
        pcivetweb_options++;
        *pcivetweb_options = runModConf->ports.val;
        pcivetweb_options++;
    }
    if (runModConf->docroot.val) {
        *pcivetweb_options = runModConf->docroot.name;
        pcivetweb_options++;
        *pcivetweb_options = runModConf->docroot.val;
        pcivetweb_options++;
    }

    const char **option = s_httpserv->civetweb_options;
    for (; option && *option != NULL; option++) {
        dbgprintf("imhttp: civetweb option: %s\n", *option);
    }

    CHKiRet(statsobj.Construct(&statsCounter.stats));
    CHKiRet(statsobj.SetName(statsCounter.stats, UCHAR_CONSTANT("imhttp")));
    CHKiRet(statsobj.SetOrigin(statsCounter.stats, UCHAR_CONSTANT("imhttp")));
    STATSCOUNTER_INIT(statsCounter.ctrSubmitted, statsCounter.mutCtrSubmitted);
    CHKiRet(statsobj.AddCounter(statsCounter.stats, UCHAR_CONSTANT("submitted"), ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &(statsCounter.ctrSubmitted)));

    STATSCOUNTER_INIT(statsCounter.ctrFailed, statsCounter.mutCtrFailed);
    CHKiRet(statsobj.AddCounter(statsCounter.stats, UCHAR_CONSTANT("failed"), ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &(statsCounter.ctrFailed)));

    STATSCOUNTER_INIT(statsCounter.ctrDiscarded, statsCounter.mutCtrDiscarded);
    CHKiRet(statsobj.AddCounter(statsCounter.stats, UCHAR_CONSTANT("discarded"), ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &(statsCounter.ctrDiscarded)));

    CHKiRet(statsobj.ConstructFinalize(statsCounter.stats));

    /* init civetweb libs and start server w/no input */
    mg_init_library(MG_FEATURES_TLS);
    memset(&callbacks, 0, sizeof(callbacks));
    // callbacks.log_message = log_message;
    // callbacks.init_ssl = init_ssl;
    callbacks.init_thread = init_thread;
    callbacks.exit_thread = exit_thread;
    s_httpserv->ctx = mg_start(&callbacks, NULL, s_httpserv->civetweb_options);
    /* Check return value: */
    if (s_httpserv->ctx == NULL) {
        LogError(0, RS_RET_INTERNAL_ERROR, "Cannot start CivetWeb - mg_start failed.\n");
        ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
    }

finalize_it:
    if (iRet != RS_RET_OK) {
        free(s_httpserv);
        s_httpserv = NULL;
        LogError(0, NO_ERRCODE, "imhttp: error %d trying to activate configuration", iRet);
    }
    RETiRet;
ENDactivateCnf

BEGINfreeCnf
    instanceConf_t *inst, *del;
    CODESTARTfreeCnf;
    for (inst = pModConf->root; inst != NULL;) {
        if (inst->ratelimiter) {
            ratelimitDestruct(inst->ratelimiter);
        }
        if (inst->pInputName) {
            prop.Destruct(&inst->pInputName);
        }
        free(inst->pszEndpoint);
        free(inst->pszBasicAuthFile);
        free(inst->pszApiKeyFile);
        free(inst->pszBindRuleset);
        free(inst->pszInputName);
        free(inst->pszRatelimitName);

        del = inst;
        inst = inst->next;
        free(del);
    }

    for (int i = 0; i < pModConf->nOptions; ++i) {
        free((void *)pModConf->options[i].name);
        free((void *)pModConf->options[i].val);
    }
    free(pModConf->options);

    free((void *)pModConf->ports.name);
    free((void *)pModConf->ports.val);
    free((void *)pModConf->docroot.name);
    free((void *)pModConf->docroot.val);
    free((void *)pModConf->pszHealthCheckPath);
    free((void *)pModConf->pszMetricsPath);
    free((void *)pModConf->pszHealthCheckAuthFile);
    free((void *)pModConf->pszMetricsAuthFile);
    free((void *)pModConf->pszHealthCheckApiKeyFile);
    free((void *)pModConf->pszMetricsApiKeyFile);

    if (statsCounter.stats) {
        statsobj.Destruct(&statsCounter.stats);
    }
ENDfreeCnf


/* This function is called to gather input.
 */
BEGINrunInput
    CODESTARTrunInput;
    runloop();
ENDrunInput

/* initialize and return if will run or not */
BEGINwillRun
    CODESTARTwillRun;
ENDwillRun

BEGINafterRun
    CODESTARTafterRun;
    if (s_httpserv) {
        mg_stop(s_httpserv->ctx);
        mg_exit_library();
        free(s_httpserv->civetweb_options);
        free(s_httpserv);
    }
ENDafterRun


BEGINisCompatibleWithFeature
    CODESTARTisCompatibleWithFeature;
    // if(eFeat == sFEATURENonCancelInputTermination)
    // 	iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINmodExit
    CODESTARTmodExit;
    if (pInputName != NULL) {
        prop.Destruct(&pInputName);
    }

    /* release objects we used */
    objRelease(statsobj, CORE_COMPONENT);
    objRelease(glbl, CORE_COMPONENT);
    objRelease(prop, CORE_COMPONENT);
    objRelease(ruleset, CORE_COMPONENT);
ENDmodExit

BEGINqueryEtryPt
    CODESTARTqueryEtryPt;
    CODEqueryEtryPt_STD_IMOD_QUERIES;
    CODEqueryEtryPt_STD_CONF2_QUERIES;
    CODEqueryEtryPt_STD_CONF2_setModCnf_QUERIES;
    CODEqueryEtryPt_STD_CONF2_IMOD_QUERIES;
    CODEqueryEtryPt_IsCompatibleWithFeature_IF_OMOD_QUERIES;
ENDqueryEtryPt


BEGINmodInit()
    CODESTARTmodInit;
    *ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
    CODEmodInit_QueryRegCFSLineHdlr CHKiRet(objUse(ruleset, CORE_COMPONENT));
    CHKiRet(objUse(glbl, CORE_COMPONENT));
    CHKiRet(objUse(prop, CORE_COMPONENT));
    CHKiRet(objUse(statsobj, CORE_COMPONENT));

    /* we need to create the inputName property (only once during our lifetime) */
    CHKiRet(prop.Construct(&pInputName));
    CHKiRet(prop.SetString(pInputName, UCHAR_CONSTANT("imhttp"), sizeof("imhttp") - 1));
    CHKiRet(prop.ConstructFinalize(pInputName));
ENDmodInit
