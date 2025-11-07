/* imhiredis.c
 * Copyright 2021 aDvens
 *
 * This file is contrib for rsyslog.
 * This input plugin is a log consumer from REDIS
 * See README for doc
 *
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Author: Jérémie Jourdin
 * <jeremie.jourdin@advens.fr>
 */

#include "config.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/uio.h>
#include <hiredis/hiredis.h>
#ifdef HIREDIS_SSL
    #include <hiredis/hiredis_ssl.h>
#endif
#include <hiredis/async.h>
#include <hiredis/adapters/libevent.h>
#include <event2/thread.h>

#include "rsyslog.h"
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include "atomic.h"
#include "statsobj.h"
#include "unicode-helper.h"
#include "prop.h"
#include "ruleset.h"
#include "glbl.h"
#include "cfsysline.h"
#include "msg.h"
#include "dirty.h"

MODULE_TYPE_INPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME("imhiredis")

/* static data */
DEF_IMOD_STATIC_DATA;
#define BATCH_SIZE 10
#define WAIT_TIME_MS 500
#define STREAM_INDEX_STR_MAXLEN 44  // "18446744073709551615-18446744073709551615"
#define IMHIREDIS_MODE_QUEUE 1
#define IMHIREDIS_MODE_SUBSCRIBE 2
#define IMHIREDIS_MODE_STREAM 3
DEFobjCurrIf(prop) DEFobjCurrIf(ruleset) DEFobjCurrIf(glbl) DEFobjCurrIf(statsobj)


    typedef struct redisNode_s {
    sbool isMaster;
    sbool usesSocket;
    uchar *socketPath;
    uchar *server;
    int port;
    struct redisNode_s *next;
} redisNode;


struct instanceConf_s {
    uchar *password;
    uchar *key;
    uchar *modeDescription;
    uchar *streamConsumerGroup;
    uchar *streamConsumerName;
    uchar *streamReadFrom;
    int streamAutoclaimIdleTime;
    sbool streamConsumerACK;
    int mode;
    uint batchsize;
    sbool useLPop;
#ifdef HIREDIS_SSL
    sbool use_tls; /* Should we use TLS to connect to redis ? */
    char *ca_cert_bundle; /* CA bundle file */
    char *ca_cert_dir; /* Path of trusted certificates */
    char *client_cert; /* Client certificate */
    char *client_key; /* Client private key */
    char *sni; /* TLS Server Name Indication */
#endif

    struct {
        int nmemb;
        char **name;
        char **varname;
    } fieldList;

    ruleset_t *pBindRuleset; /* ruleset to bind listener to (use system default if unspecified) */
    uchar *pszBindRuleset; /* default name of Ruleset to bind to */

    redisContext *conn;
    redisAsyncContext *aconn;
#ifdef HIREDIS_SSL
    redisSSLContext *ssl_conn; /* redis ssl connection */
#endif
    struct event_base *evtBase;

    redisNode *currentNode; /* currently used redis node, can be any of the nodes in the redisNodesList list */
    /* the list of seen nodes
     * the preferred node (the one from configuration) will always be the first element
     * second one is a master (if preferred one is unavailable/replica) or a replica, others are replicas
     * the preferred node may appear twice, but it is accepted
     */
    redisNode *redisNodesList;

    struct instanceConf_s *next;
};


struct modConfData_s {
    rsconf_t *pConf; /* our overall config object */
    instanceConf_t *root, *tail;
};

/* The following structure controls the worker threads. Global data is
 * needed for their access.
 */
static struct imhiredisWrkrInfo_s {
    pthread_t tid; /* the worker's thread ID */
    instanceConf_t *inst; /* Pointer to imhiredis instance */
    rsRetVal (*fnConnectMaster)(instanceConf_t *inst);
    sbool (*fnIsConnected)(instanceConf_t *inst);
    rsRetVal (*fnRun)(instanceConf_t *inst);
} *imhiredisWrkrInfo;


/* GLOBAL DATA */
pthread_attr_t wrkrThrdAttr; /* Attribute for worker threads ; read only after startup */

static int activeHiredisworkers = 0;
static const char *REDIS_REPLIES[] = {
    "unknown",  // 0
    "string",  // 1
    "array",  // 2
    "integer",  // 3
    "nil",  // 4
    "status",  // 5
    "error",  // 6
    "double",  // 7
    "bool",  // 8
    "map",  // 9
    "set",  // 10
    "attr",  // 11
    "push",  // 12
    "bignum",  // 13
    "verb",  // 14
};

static modConfData_t *loadModConf = NULL; /* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL; /* modConf ptr to use for the current load process */

static prop_t *pInputName = NULL;
/* there is only one global inputName for all messages generated by this input */


/* module-global parameters */
static struct cnfparamdescr modpdescr[] = {};
static struct cnfparamblk modpblk = {CNFPARAMBLK_VERSION, sizeof(modpdescr) / sizeof(struct cnfparamdescr), modpdescr};

/* input instance parameters */
static struct cnfparamdescr inppdescr[] = {
    {"socketPath", eCmdHdlrGetWord, 0},
    {"server", eCmdHdlrGetWord, 0},
    {"port", eCmdHdlrInt, 0},
    {"password", eCmdHdlrGetWord, 0},
    {"mode", eCmdHdlrGetWord, 0},
    {"batchsize", eCmdHdlrInt, 0},
    {"key", eCmdHdlrGetWord, CNFPARAM_REQUIRED},
    {"uselpop", eCmdHdlrBinary, 0},
    {"ruleset", eCmdHdlrString, 0},
    {"stream.consumerGroup", eCmdHdlrGetWord, 0},
    {"stream.consumerName", eCmdHdlrGetWord, 0},
    {"stream.readFrom", eCmdHdlrGetWord, 0},
    {"stream.consumerACK", eCmdHdlrBinary, 0},
    {"stream.autoclaimIdleTime", eCmdHdlrNonNegInt, 0},
    {"fields", eCmdHdlrArray, 0},
#ifdef HIREDIS_SSL
    {"use_tls", eCmdHdlrBinary, 0},
    {"ca_cert_bundle", eCmdHdlrGetWord, 0},
    {"ca_cert_dir", eCmdHdlrGetWord, 0},
    {"client_cert", eCmdHdlrGetWord, 0},
    {"client_key", eCmdHdlrGetWord, 0},
    {"sni", eCmdHdlrGetWord, 0},
#endif
};
static struct cnfparamblk inppblk = {CNFPARAMBLK_VERSION, sizeof(inppdescr) / sizeof(struct cnfparamdescr), inppdescr};

struct timeval glblRedisConnectTimeout = { 3, 0 }; /* 3 seconds */
struct timeval glblRedisCommandTimeout = { 5, 0 }; /* 5 seconds */


#include "im-helper.h" /* must be included AFTER the type definitions! */


/* forward references */
static void redisAsyncRecvCallback(redisAsyncContext __attribute__((unused)) * c, void *reply, void *inst_obj);
static void redisAsyncConnectCallback(const redisAsyncContext *c, int status);
static void redisAsyncDisconnectCallback(const redisAsyncContext *c, int status);
redisReply *getRole(redisContext *c);
static struct json_object *_redisParseIntegerReply(const redisReply *reply);
static struct json_object *_redisParseStringReply(const redisReply *reply);
static struct json_object *_redisParseArrayReply(const redisReply *reply);
#ifdef REDIS_REPLY_DOUBLE
static struct json_object *_redisParseDoubleReply(const redisReply *reply);
#endif
static rsRetVal enqMsg(instanceConf_t *const inst, const char *message, size_t msgLen);
static rsRetVal enqMsgJson(instanceConf_t *const inst, struct json_object *json, struct json_object *metadata);
rsRetVal redisAuthentSynchronous(redisContext *conn, uchar *password);
rsRetVal redisAuthentAsynchronous(redisAsyncContext *aconn, uchar *password);
rsRetVal redisActualizeCurrentNode(instanceConf_t *inst);
rsRetVal redisGetServersList(redisNode *node, instanceConf_t *const inst, redisNode **result);
rsRetVal redisAuthenticate(instanceConf_t *inst);
#ifdef HIREDIS_SSL
rsRetVal redisInitSSLContext(redisContext *conn, redisSSLContext *ssl_context);
#endif
rsRetVal redisConnectSync(redisContext **conn, redisNode *node);
rsRetVal connectMasterSync(instanceConf_t *inst);
static sbool isConnectedSync(instanceConf_t *inst);
rsRetVal redisConnectAsync(redisAsyncContext **aconn, redisNode *node);
rsRetVal connectMasterAsync(instanceConf_t *inst);
static sbool isConnectedAsync(instanceConf_t *inst);
rsRetVal redisDequeue(instanceConf_t *inst);
rsRetVal ensureConsumerGroupCreated(instanceConf_t *inst);
rsRetVal ackStreamIndex(instanceConf_t *inst, uchar *stream, uchar *group, uchar *index);
static rsRetVal enqueueRedisStreamReply(instanceConf_t *const inst, redisReply *reply);
static rsRetVal handleRedisXREADReply(instanceConf_t *const inst, const redisReply *reply);
static rsRetVal handleRedisXAUTOCLAIMReply(instanceConf_t *const inst, const redisReply *reply, char **autoclaimIndex);
rsRetVal redisStreamRead(instanceConf_t *inst);
rsRetVal redisSubscribe(instanceConf_t *inst);
void workerLoop(struct imhiredisWrkrInfo_s *me);
static void *imhirediswrkr(void *myself);
static rsRetVal createRedisNode(redisNode **root);
rsRetVal copyNode(redisNode *src, redisNode **dst);
redisNode *freeNode(redisNode *node);
void insertNodeAfter(redisNode *root, redisNode *elem);
void dbgPrintNode(redisNode *node);


/* create input instance, set default parameters, and
 * add it to the list of instances.
 */
static rsRetVal createInstance(instanceConf_t **pinst) {
    DEFiRet;
    instanceConf_t *inst;
    CHKmalloc(inst = calloc(1, sizeof(instanceConf_t)));

    inst->next = NULL;
    inst->password = NULL;
    inst->key = NULL;
    inst->mode = 0;
    inst->batchsize = 0;
    inst->useLPop = 0;
    inst->streamConsumerGroup = NULL;
    inst->streamConsumerName = NULL;
    CHKmalloc(inst->streamReadFrom = calloc(1, STREAM_INDEX_STR_MAXLEN));
    inst->streamAutoclaimIdleTime = 0;
    inst->streamConsumerACK = 1;
    inst->pszBindRuleset = NULL;
    inst->pBindRuleset = NULL;
    inst->fieldList.nmemb = 0;
#ifdef HIREDIS_SSL
    inst->use_tls = 0;
    inst->ca_cert_bundle = NULL;
    inst->ca_cert_dir = NULL;
    inst->client_cert = NULL;
    inst->client_key = NULL;
    inst->sni = NULL;
#endif

    /* Redis objects */
    inst->conn = NULL;
    inst->aconn = NULL;
#ifdef HIREDIS_SSL
    inst->ssl_conn = NULL; /* Connect later */
#endif

    /* redis nodes list */
    CHKiRet(createRedisNode(&(inst->redisNodesList)));
    inst->currentNode = inst->redisNodesList;

    /* libevent base for async connection */
    inst->evtBase = NULL;

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

/* this function checks instance parameters and does some required pre-processing
 */
static rsRetVal ATTR_NONNULL() checkInstance(instanceConf_t *const inst) {
    DEFiRet;
    /* first node should be created from configuration */
    assert(inst->redisNodesList != NULL);

    /* check and print redis connection settings */
    if (inst->redisNodesList->server != NULL && inst->redisNodesList->socketPath != NULL) {
        LogMsg(0, RS_RET_CONFIG_ERROR, LOG_WARNING,
               "imhiredis: both 'server' and 'socketPath' are given, "
               "ignoring 'socketPath'.");
        free(inst->redisNodesList->socketPath);
        inst->redisNodesList->socketPath = NULL;
    }

    if (inst->redisNodesList->server != NULL && inst->redisNodesList->server[0] != '\0') {
        if (inst->redisNodesList->port == 0) {
            LogMsg(0, RS_RET_OK_WARN, LOG_WARNING, "imhiredis: port not set, setting default 6379");
            inst->redisNodesList->port = 6379;
        }
        DBGPRINTF("imhiredis: preferred server is %s (%d)\n", inst->redisNodesList->server, inst->redisNodesList->port);
        inst->redisNodesList->usesSocket = 0;
    } else if (inst->redisNodesList->socketPath != NULL && inst->redisNodesList->socketPath[0] != '\0') {
        DBGPRINTF("imhiredis: preferred server is %s\n", inst->redisNodesList->socketPath);
        inst->redisNodesList->usesSocket = 1;
    } else {
        LogError(0, RS_RET_CONFIG_ERROR, "imhiredis: neither 'server' nor 'socketPath' are defined!");
        ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
    }

#ifdef HIREDIS_SSL
    redisSSLContextError ssl_error = REDIS_SSL_CTX_NONE;

    // Check and initialize SSL context
    if (inst->use_tls) {
        if ((inst->client_cert == NULL) ^ (inst->client_key == NULL)) {
            LogMsg(0, RS_RET_CONFIG_ERROR, LOG_ERR,
                   "imhiredis: \"client_cert\" and \"client_key\" must be specified together!");
            ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
        }

        inst->ssl_conn = redisCreateSSLContext(inst->ca_cert_bundle, inst->ca_cert_dir, inst->client_cert,
                                               inst->client_key, inst->sni, &ssl_error);

        if (!inst->ssl_conn || ssl_error != REDIS_SSL_CTX_NONE) {
            LogError(0, RS_RET_REDIS_ERROR, "imhiredis: TLS configuration Error: %s",
                     redisSSLContextGetError(ssl_error));
            if (inst->ssl_conn != NULL) {
                redisFreeSSLContext(inst->ssl_conn);
                inst->ssl_conn = NULL;
            }
            ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
        }
    }
#endif

    if (inst->mode < IMHIREDIS_MODE_QUEUE || inst->mode > IMHIREDIS_MODE_STREAM) {
        LogError(0, RS_RET_CONFIG_ERROR,
                 "imhiredis: invalid mode, please choose 'subscribe', "
                 "'queue' or 'stream' mode.");
        ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
    }

    if (inst->mode != IMHIREDIS_MODE_QUEUE && inst->useLPop) {
        LogMsg(0, RS_RET_CONFIG_ERROR, LOG_WARNING, "imhiredis: 'uselpop' set with mode != queue : ignored.");
    }

    if (inst->mode == IMHIREDIS_MODE_STREAM) {
        if (inst->streamConsumerGroup != NULL && inst->streamConsumerName == NULL) {
            LogError(0, RS_RET_CONFIG_ERROR,
                     "imhiredis: invalid configuration, "
                     "please set a consumer name when mode is 'stream' and a consumer group is set");
            ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
        }
        if (inst->streamAutoclaimIdleTime != 0 && inst->streamConsumerGroup == NULL) {
            LogMsg(0, RS_RET_CONFIG_ERROR, LOG_WARNING,
                   "imhiredis: 'stream.autoclaimIdleTime' "
                   "set with no consumer group set : ignored.");
        }
        if (inst->streamReadFrom[0] == '\0') {
            inst->streamReadFrom[0] = '$';
        }
    } else {
        if (inst->streamConsumerGroup != NULL) {
            LogMsg(0, RS_RET_CONFIG_ERROR, LOG_WARNING,
                   "imhiredis: 'stream.consumerGroup' "
                   "set with mode != stream : ignored.");
        }
        if (inst->streamConsumerName != NULL) {
            LogMsg(0, RS_RET_CONFIG_ERROR, LOG_WARNING,
                   "imhiredis: 'stream.consumerName' "
                   "set with mode != stream : ignored.");
        }
        if (inst->streamAutoclaimIdleTime != 0) {
            LogMsg(0, RS_RET_CONFIG_ERROR, LOG_WARNING,
                   "imhiredis: 'stream.autoclaimIdleTime' "
                   "set with mode != stream : ignored.");
        }
        if (inst->streamConsumerACK == 0) {
            LogMsg(0, RS_RET_CONFIG_ERROR, LOG_WARNING,
                   "imhiredis: 'stream.consumerACK' "
                   "disabled with mode != stream : ignored.");
        }
        if (inst->fieldList.nmemb > 0) {
            LogMsg(0, RS_RET_CONFIG_ERROR, LOG_WARNING,
                   "imhiredis: 'fields' "
                   "unused for mode != stream : ignored.");
        }
    }

    if (inst->batchsize != 0) {
        DBGPRINTF("imhiredis: batchsize is '%d'\n", inst->batchsize);
    } else {
        LogMsg(0, RS_RET_OK_WARN, LOG_WARNING, "imhiredis: batchsize not set, setting to default (%d)", BATCH_SIZE);
        inst->batchsize = BATCH_SIZE;
    }

    if (inst->password != NULL) {
        DBGPRINTF("imhiredis: password is '%s'\n", inst->password);
    }

    // set default current node as first node in list (preferred node)
    inst->currentNode = inst->redisNodesList;
    // search master node (should be either preferred node or its master)
    if (RS_RET_OK != redisActualizeCurrentNode(inst) || inst->currentNode == NULL) {
        LogMsg(0, RS_RET_REDIS_ERROR, LOG_WARNING, "imhiredis: could not connect to a valid master!");
    }

finalize_it:
    RETiRet;
}

/* function to generate an error message if the ruleset cannot be found */
static inline void std_checkRuleset_genErrMsg(__attribute__((unused)) modConfData_t *modConf, instanceConf_t *inst) {
    LogError(0, NO_ERRCODE,
             "imhiredis: ruleset '%s' not found - "
             "using default ruleset instead",
             inst->pszBindRuleset);
}


BEGINnewInpInst
    struct cnfparamvals *pvals;
    instanceConf_t *inst;
    int i;
    CODESTARTnewInpInst;
    DBGPRINTF("newInpInst (imhiredis)\n");

    if ((pvals = nvlstGetParams(lst, &inppblk, NULL)) == NULL) {
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }

    if (Debug) {
        dbgprintf("input param blk in imhiredis:\n");
        cnfparamsPrint(&inppblk, pvals);
    }

    CHKiRet(createInstance(&inst));
    for (i = 0; i < inppblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;

        if (!strcmp(inppblk.descr[i].name, "server")) {
            inst->redisNodesList->server = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(inppblk.descr[i].name, "socketPath")) {
            inst->redisNodesList->socketPath = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(inppblk.descr[i].name, "ruleset")) {
            inst->pszBindRuleset = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(inppblk.descr[i].name, "port")) {
            inst->redisNodesList->port = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "password")) {
            inst->password = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(inppblk.descr[i].name, "stream.consumerGroup")) {
            inst->streamConsumerGroup = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(inppblk.descr[i].name, "stream.consumerName")) {
            inst->streamConsumerName = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(inppblk.descr[i].name, "stream.consumerACK")) {
            inst->streamConsumerACK = pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "stream.readFrom")) {
            // inst->streamReadFrom is already allocated, only copy contents
            memcpy(inst->streamReadFrom, es_getBufAddr(pvals[i].val.d.estr), es_strlen(pvals[i].val.d.estr));
            inst->streamReadFrom[es_strlen(pvals[i].val.d.estr)] = '\0';
        } else if (!strcmp(inppblk.descr[i].name, "stream.autoclaimIdleTime")) {
            inst->streamAutoclaimIdleTime = pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "uselpop")) {
            inst->useLPop = pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "mode")) {
            inst->modeDescription = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
            if (!strcmp((const char *)inst->modeDescription, "queue")) {
                inst->mode = IMHIREDIS_MODE_QUEUE;
            } else if (!strcmp((const char *)inst->modeDescription, "subscribe")) {
                inst->mode = IMHIREDIS_MODE_SUBSCRIBE;
            } else if (!strcmp((const char *)inst->modeDescription, "stream")) {
                inst->mode = IMHIREDIS_MODE_STREAM;
            } else {
                LogMsg(0, RS_RET_PARAM_ERROR, LOG_ERR,
                       "imhiredis: unsupported mode "
                       "'%s'",
                       inst->modeDescription);
                ABORT_FINALIZE(RS_RET_PARAM_ERROR);
            }
        } else if (!strcmp(inppblk.descr[i].name, "fields")) {
            inst->fieldList.nmemb = pvals[i].val.d.ar->nmemb;
            CHKmalloc(inst->fieldList.name = calloc(inst->fieldList.nmemb, sizeof(char *)));
            CHKmalloc(inst->fieldList.varname = calloc(inst->fieldList.nmemb, sizeof(char *)));
            for (int j = 0; j < pvals[i].val.d.ar->nmemb; ++j) {
                char *const param = es_str2cstr(pvals[i].val.d.ar->arr[j], NULL);
                char *varname = NULL;
                char *name;
                if (*param == ':') {
                    char *b = strchr(param + 1, ':');
                    if (b == NULL) {
                        LogMsg(0, RS_RET_PARAM_ERROR, LOG_ERR, "imhiredis: missing closing colon: '%s'", param);
                        ABORT_FINALIZE(RS_RET_ERR);
                    }
                    *b = '\0'; /* split name & varname */
                    varname = param + 1;
                    name = b + 1;
                } else {
                    name = param;
                }
                CHKmalloc(inst->fieldList.name[j] = strdup(name));
                char vnamebuf[1024];
                snprintf(vnamebuf, sizeof(vnamebuf), "!%s", (varname == NULL) ? name : varname);
                CHKmalloc(inst->fieldList.varname[j] = strdup(vnamebuf));
                DBGPRINTF("will map '%s' to '%s'\n", inst->fieldList.name[j], inst->fieldList.varname[j]);
                free(param);
            }
        } else if (!strcmp(inppblk.descr[i].name, "batchsize")) {
            inst->batchsize = (int)pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "key")) {
            inst->key = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
#ifdef HIREDIS_SSL
        } else if (!strcmp(inppblk.descr[i].name, "use_tls")) {
            inst->use_tls = pvals[i].val.d.n;
        } else if (!strcmp(inppblk.descr[i].name, "ca_cert_bundle")) {
            inst->ca_cert_bundle = (char *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(inppblk.descr[i].name, "ca_cert_dir")) {
            inst->ca_cert_dir = (char *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(inppblk.descr[i].name, "client_cert")) {
            inst->client_cert = (char *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(inppblk.descr[i].name, "client_key")) {
            inst->client_key = (char *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(inppblk.descr[i].name, "sni")) {
            inst->sni = (char *)es_str2cstr(pvals[i].val.d.estr, NULL);
#endif
        } else {
            dbgprintf(
                "imhiredis: program error, non-handled "
                "param '%s'\n",
                inppblk.descr[i].name);
        }
    }

    DBGPRINTF("imhiredis: checking config sanity\n");
    if (inst->modeDescription == NULL) {
        CHKmalloc(inst->modeDescription = (uchar *)strdup("subscribe"));
        inst->mode = IMHIREDIS_MODE_SUBSCRIBE;
        LogMsg(0, RS_RET_OK_WARN, LOG_WARNING,
               "imhiredis: \"mode\" parameter not specified "
               "using default redis 'subscribe' mode -- this may not be what you want!");
    }
    if (inst->key == NULL) {
        LogMsg(0, RS_RET_PARAM_ERROR, LOG_ERR, "imhiredis: \"key\" required parameter not specified!");
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }
    if (inst->redisNodesList->server == NULL && inst->redisNodesList->socketPath == NULL) {
        CHKmalloc(inst->redisNodesList->server = (uchar *)strdup("127.0.0.1"));
        inst->redisNodesList->port = 6379;
        LogMsg(0, RS_RET_OK_WARN, LOG_WARNING,
               "imhiredis: no server parameter specified "
               "using default 127.0.0.1:6379 -- this may not be what you want!");
    }
    if (inst->password == NULL) {
        LogMsg(0, RS_RET_OK, LOG_INFO, "imhiredis: no password specified");
    }

    DBGPRINTF("imhiredis: newInpInst key=%s, mode=%s, uselpop=%d\n", inst->key, inst->modeDescription, inst->useLPop);

finalize_it:
    CODE_STD_FINALIZERnewInpInst cnfparamvalsDestruct(pvals, &inppblk);
ENDnewInpInst


BEGINbeginCnfLoad
    CODESTARTbeginCnfLoad;
    loadModConf = pModConf;
    pModConf->pConf = pConf;
ENDbeginCnfLoad


BEGINsetModCnf
    struct cnfparamvals *pvals = NULL;
    int i;
    CODESTARTsetModCnf;
    pvals = nvlstGetParams(lst, &modpblk, NULL);
    if (pvals == NULL) {
        LogError(0, RS_RET_MISSING_CNFPARAMS,
                 "imhiredis: error processing module "
                 "config parameters [module(...)]");
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }

    if (Debug) {
        dbgprintf("module (global) param blk for imhiredis:\n");
        cnfparamsPrint(&modpblk, pvals);
    }

    for (i = 0; i < modpblk.nParams; ++i) {
        if (!pvals[i].bUsed) {
            continue;
        } else {
            dbgprintf(
                "imhiredis: program error, non-handled "
                "param '%s' in beginCnfLoad\n",
                modpblk.descr[i].name);
        }
    }
finalize_it:
    if (pvals != NULL) cnfparamvalsDestruct(pvals, &modpblk);
ENDsetModCnf

BEGINendCnfLoad
    CODESTARTendCnfLoad;
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
    CODESTARTactivateCnf;
    for (instanceConf_t *inst = pModConf->root; inst != NULL; inst = inst->next) {
        iRet = checkInstance(inst);
        if (inst->mode == IMHIREDIS_MODE_SUBSCRIBE) inst->evtBase = event_base_new();
    }
ENDactivateCnf


BEGINfreeCnf
    instanceConf_t *inst, *del;
    redisNode *node;
    CODESTARTfreeCnf;
    for (inst = pModConf->root; inst != NULL;) {
        if (inst->evtBase) event_base_free(inst->evtBase);
        if (inst->password != NULL) free(inst->password);
        free(inst->modeDescription);
        free(inst->key);
        if (inst->streamConsumerGroup != NULL) free(inst->streamConsumerGroup);
        if (inst->streamConsumerName != NULL) free(inst->streamConsumerName);
        free(inst->streamReadFrom);
        free(inst->pszBindRuleset);
        if (inst->fieldList.name != NULL) {
            for (int i = 0; i < inst->fieldList.nmemb; ++i) {
                free(inst->fieldList.name[i]);
                free(inst->fieldList.varname[i]);
            }
            free(inst->fieldList.name);
            free(inst->fieldList.varname);
        }
        if (inst->conn != NULL) {
            redisFree(inst->conn);
            inst->conn = NULL;
        }
        if (inst->aconn != NULL) {
            redisAsyncFree(inst->aconn);
            inst->aconn = NULL;
        }
#ifdef HIREDIS_SSL
        if (inst->ssl_conn != NULL) {
            redisFreeSSLContext(inst->ssl_conn);
            inst->ssl_conn = NULL;
        }
        if (inst->ca_cert_bundle != NULL) free(inst->ca_cert_bundle);
        if (inst->ca_cert_dir != NULL) free(inst->ca_cert_dir);
        if (inst->client_cert != NULL) free(inst->client_cert);
        if (inst->client_key != NULL) free(inst->client_key);
        if (inst->sni) free(inst->sni);
#endif

        for (node = inst->redisNodesList; node != NULL; node = freeNode(node)) {
            ;
        }

        del = inst;
        inst = inst->next;
        free(del);
    }
ENDfreeCnf


/* Cleanup imhiredis worker threads */
static void shutdownImhiredisWorkers(void) {
    int i;
    instanceConf_t *inst;

    assert(imhiredisWrkrInfo != NULL);

    for (inst = runModConf->root; inst != NULL; inst = inst->next) {
        if (inst->mode == IMHIREDIS_MODE_SUBSCRIBE && inst->aconn) {
            DBGPRINTF("imhiredis: disconnecting async worker\n");
            redisAsyncDisconnect(inst->aconn);
        }
    }

    // event_base_loopbreak(runModConf->evtBase);

    DBGPRINTF("imhiredis: waiting on imhiredis workerthread termination\n");
    for (i = 0; i < activeHiredisworkers; ++i) {
        pthread_join(imhiredisWrkrInfo[i].tid, NULL);
        DBGPRINTF("imhiredis: Stopped worker %d\n", i);
    }
    free(imhiredisWrkrInfo);
    imhiredisWrkrInfo = NULL;

    return;
}


/* This function is called to gather input.  */
BEGINrunInput
    int i;
    instanceConf_t *inst;
    CODESTARTrunInput;
    DBGPRINTF("imhiredis: runInput loop started ...\n");
    activeHiredisworkers = 0;
    for (inst = runModConf->root; inst != NULL; inst = inst->next) {
        ++activeHiredisworkers;
    }

    if (activeHiredisworkers == 0) {
        LogError(0, RS_RET_ERR,
                 "imhiredis: no active inputs, input does "
                 "not run - there should have been additional error "
                 "messages given previously");
        ABORT_FINALIZE(RS_RET_ERR);
    }


    DBGPRINTF("imhiredis: Starting %d imhiredis workerthreads\n", activeHiredisworkers);
    imhiredisWrkrInfo = calloc(activeHiredisworkers, sizeof(struct imhiredisWrkrInfo_s));
    if (imhiredisWrkrInfo == NULL) {
        LogError(errno, RS_RET_OUT_OF_MEMORY, "imhiredis: worker-info array allocation failed.");
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    /* Start worker threads for each imhiredis input source
     */
    i = 0;
    for (inst = runModConf->root; inst != NULL; inst = inst->next) {
        /* init worker info structure! */
        imhiredisWrkrInfo[i].inst = inst; /* Set reference pointer */
        pthread_create(&imhiredisWrkrInfo[i].tid, &wrkrThrdAttr, imhirediswrkr, &(imhiredisWrkrInfo[i]));
        i++;
    }

    // This thread simply runs the various threads, then waits for Rsyslog to stop
    while (glbl.GetGlobalInputTermState() == 0) {
        if (glbl.GetGlobalInputTermState() == 0)
            /* Check termination state every 0.5s
             * should be sufficient to grant fast response to shutdown while not hogging CPU
             */
            srSleep(0, 500000);
    }
    DBGPRINTF("imhiredis: terminating upon request of rsyslog core\n");

    shutdownImhiredisWorkers();
finalize_it:
ENDrunInput


BEGINwillRun
    CODESTARTwillRun;
    /* we need to create the inputName property (only once during our lifetime) */
    CHKiRet(prop.Construct(&pInputName));
    CHKiRet(prop.SetString(pInputName, UCHAR_CONSTANT("imhiredis"), sizeof("imhiredis") - 1));
    CHKiRet(prop.ConstructFinalize(pInputName));
finalize_it:
ENDwillRun


BEGINafterRun
    CODESTARTafterRun;
    if (pInputName != NULL) prop.Destruct(&pInputName);

ENDafterRun


BEGINmodExit
    CODESTARTmodExit;
    pthread_attr_destroy(&wrkrThrdAttr);

    /* force cleaning of all libevent-related structures
     * (clean shutdowns are not always guaranteed without it)
     */
    libevent_global_shutdown();

    /* release objects we used */
    objRelease(statsobj, CORE_COMPONENT);
    objRelease(ruleset, CORE_COMPONENT);
    objRelease(glbl, CORE_COMPONENT);
    objRelease(prop, CORE_COMPONENT);
ENDmodExit


BEGINisCompatibleWithFeature
    CODESTARTisCompatibleWithFeature;
    if (eFeat == sFEATURENonCancelInputTermination) iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINqueryEtryPt
    CODESTARTqueryEtryPt;
    CODEqueryEtryPt_STD_IMOD_QUERIES;
    CODEqueryEtryPt_STD_CONF2_QUERIES;
    CODEqueryEtryPt_STD_CONF2_PREPRIVDROP_QUERIES;
    CODEqueryEtryPt_STD_CONF2_IMOD_QUERIES;
    CODEqueryEtryPt_STD_CONF2_setModCnf_QUERIES;
    CODEqueryEtryPt_IsCompatibleWithFeature_IF_OMOD_QUERIES;
ENDqueryEtryPt


BEGINmodInit()
    CODESTARTmodInit;
    *ipIFVersProvided = CURR_MOD_IF_VERSION;
    CODEmodInit_QueryRegCFSLineHdlr
        /* request objects we use */
        CHKiRet(objUse(glbl, CORE_COMPONENT));
    CHKiRet(objUse(prop, CORE_COMPONENT));
    CHKiRet(objUse(ruleset, CORE_COMPONENT));
    CHKiRet(objUse(statsobj, CORE_COMPONENT));

    /* initialize "read-only" thread attributes */
    pthread_attr_init(&wrkrThrdAttr);
    pthread_attr_setstacksize(&wrkrThrdAttr, 4096 * 1024);

    /* activate libevent for (p)threads support */
    evthread_use_pthreads();

#ifdef HIREDIS_SSL
    // initialize OpenSSL
    redisInitOpenSSL();
#endif

ENDmodInit


/* ------------------------------ callbacks ------------------------------ */


/**
 *	Asynchronous subscribe callback handler
 */
static void redisAsyncRecvCallback(redisAsyncContext *aconn, void *reply, void __attribute__((unused)) * unused) {
    /*
        redisReply is supposed to be an array of three elements: [''message', <channel>, <message>]


        JJO: For future reference (https://github.com/redis/hiredis/blob/master/README.md)

        Important: the current version of hiredis (1.0.0) frees replies when the asynchronous API is used.
        This means you should not call freeReplyObject when you use this API.
        The reply is cleaned up by hiredis after the callback returns.
        TODO We may have to change this function in the future to free replies.
    */
    instanceConf_t *const inst = (instanceConf_t *)aconn->data;
    redisReply *r = (redisReply *)reply;
    if (r == NULL) return;

    if (r->elements < 3 || r->element[2]->str == NULL) {
        return;
    }
    enqMsg(inst, r->element[2]->str, r->element[2]->len);

    return;
}


/**
 *	Asynchronous connection callback handler
 */
static void redisAsyncConnectCallback(const redisAsyncContext *c, int status) {
    if (status != REDIS_OK) {
        LogMsg(0, RS_RET_REDIS_ERROR, LOG_ERR,
               "imhiredis (async): could not connect to redis: "
               "%s",
               c->errstr);
        // remove async context from instance config object, still contained in context's 'data' field
        instanceConf_t *inst = (instanceConf_t *)c->data;
        assert(inst != NULL);
        inst->aconn = NULL;
        return;
    }
    DBGPRINTF("imhiredis (async): successfully connected!\n");

    return;
}


/**
 *	Asynchronous disconnection callback handler
 */
static void redisAsyncDisconnectCallback(const redisAsyncContext *c, int status) {
    // remove context from instance config object (which is stored in the context 'data' field by us)
    // context will be freed by the library, so it's only set to NULL here
    instanceConf_t *inst = (instanceConf_t *)c->data;
    assert(inst != NULL);
    inst->aconn = NULL;
    inst->currentNode = NULL;

    if (status != REDIS_OK) {
        LogMsg(0, RS_RET_REDIS_ERROR, LOG_ERR,
               "imhiredis (async): got disconnected from redis: "
               "%s",
               c->errstr);
        return;
    }
    DBGPRINTF("imhiredis (async): successfully disconnected!\n");

    return;
}


/* ------------------------------ end callbacks ------------------------------ */

/*
 *	sends a ROLE command to the redis pointed by context
 *	context should be a valid redis context
 *	returns a valid redisReply pointer if an array reply was received, NULL otherwise
 */
redisReply *getRole(redisContext *c) {
    redisReply *reply;

    assert(c != NULL);

    reply = redisCommand(c, "ROLE");
    if (reply == NULL) {
        DBGPRINTF("imhiredis: could not get reply from ROLE command\n");
    } else if (reply->type == REDIS_REPLY_ERROR) {
        LogMsg(0, RS_RET_REDIS_ERROR, LOG_WARNING,
               "imhiredis got an error while querying role -> "
               "%s\n",
               reply->str);
        freeReplyObject(reply);
        reply = NULL;
    } else if (reply->type != REDIS_REPLY_ARRAY) {
        LogMsg(0, RS_RET_REDIS_ERROR, LOG_ERR, "imhiredis: did not get an array from ROLE command");
        freeReplyObject(reply);
        reply = NULL;
    }

    return reply;
}


static struct json_object *_redisParseIntegerReply(const redisReply *reply) {
    return json_object_new_int64(reply->integer);
}

#ifdef REDIS_REPLY_DOUBLE
static struct json_object *_redisParseDoubleReply(const redisReply *reply) {
    return json_object_new_double_s(reply->dval, reply->str);
}
#endif

static struct json_object *_redisParseStringReply(const redisReply *reply) {
    return json_object_new_string_len(reply->str, reply->len);
}

static struct json_object *_redisParseArrayReply(const redisReply *reply) {
    struct json_object *result = NULL;
    struct json_object *res = NULL;
    char *key = NULL;

    result = json_object_new_object();  // the redis type name is ARRAY, but represents a dict

    if (result != NULL) {
        for (size_t i = 0; i < reply->elements; i++) {
            if (reply->element[i]->type == REDIS_REPLY_STRING && i % 2 == 0) {
                key = reply->element[i]->str;
            } else {
                switch (reply->element[i]->type) {
                    case REDIS_REPLY_INTEGER:
                        res = _redisParseIntegerReply(reply->element[i]);
                        json_object_object_add(result, key, res);
                        break;
#ifdef REDIS_REPLY_DOUBLE
                    case REDIS_REPLY_DOUBLE:
                        res = _redisParseDoubleReply(reply->element[i]);
                        json_object_object_add(result, key, res);
                        break;
#endif
                    case REDIS_REPLY_STRING:
                        res = _redisParseStringReply(reply->element[i]);
                        json_object_object_add(result, key, res);
                        break;
                    case REDIS_REPLY_ARRAY:
                        res = _redisParseArrayReply(reply->element[i]);
                        json_object_object_add(result, key, res);
                        break;
                    default:
                        DBGPRINTF("Unhandled case!\n");
                        LogMsg(0, RS_RET_OK_WARN, LOG_WARNING, "Redis reply object contains an unhandled type!");
                        break;
                }
            }
        }
    }

    return result;
}


/*
 *	enqueue the hiredis message. The provided string is
 *	not freed - this must be done by the caller.
 */
static rsRetVal enqMsg(instanceConf_t *const inst, const char *message, size_t msgLen) {
    DEFiRet;
    smsg_t *pMsg;

    if (message == NULL || message[0] == '\0') {
        /* we do not process empty lines */
        FINALIZE;
    }

    DBGPRINTF("imhiredis: enqMsg: Msg -> '%s'\n", message);

    CHKiRet(msgConstruct(&pMsg));
    MsgSetInputName(pMsg, pInputName);
    MsgSetRawMsg(pMsg, message, msgLen);
    MsgSetFlowControlType(pMsg, eFLOWCTL_LIGHT_DELAY);
    MsgSetRuleset(pMsg, inst->pBindRuleset);
    MsgSetMSGoffs(pMsg, 0); /* we do not have a header... */
    CHKiRet(submitMsg2(pMsg));

finalize_it:
    RETiRet;
}


static rsRetVal enqMsgJson(instanceConf_t *const inst, struct json_object *json, struct json_object *metadata) {
    DEFiRet;
    smsg_t *pMsg;
    struct json_object *tempJson = NULL;

    assert(json != NULL);

    CHKiRet(msgConstruct(&pMsg));  // In case of allocation error -> needs to break
    MsgSetInputName(pMsg, pInputName);
    MsgSetRuleset(pMsg, inst->pBindRuleset);
    MsgSetMSGoffs(pMsg, 0); /* we do not have a header... */
    if (RS_RET_OK != MsgSetFlowControlType(pMsg, eFLOWCTL_LIGHT_DELAY))
        LogMsg(0, RS_RET_OK_WARN, LOG_WARNING, "Could not set Flow Control on message.");
    if (inst->fieldList.nmemb != 0) {
        for (int i = 0; i < inst->fieldList.nmemb; i++) {
            DBGPRINTF("processing field '%s'\n", inst->fieldList.name[i]);

            /* case 1: static field. We simply forward it */
            if (inst->fieldList.name[i][0] != '!' && inst->fieldList.name[i][0] != '.') {
                DBGPRINTF("field is static, taking it as it is...\n");
                tempJson = json_object_new_string(inst->fieldList.name[i]);
            }
            /* case 2: dynamic field. We retrieve its value from the JSON logline and add it */
            else {
                DBGPRINTF("field is dynamic, searching in root object...\n");
                if (!json_object_object_get_ex(json, inst->fieldList.name[i] + 1, &tempJson)) {
                    DBGPRINTF("Did not find value %s in message\n", inst->fieldList.name[i]);
                    continue;
                }
                // Getting object as it will not keep the same lifetime as its origin object
                tempJson = json_object_get(tempJson);
                // original object is put: no need for it anymore
                json_object_put(json);
            }

            DBGPRINTF("got value of field '%s'\n", inst->fieldList.name[i]);
            DBGPRINTF("will insert to '%s'\n", inst->fieldList.varname[i]);

            if (RS_RET_OK != msgAddJSON(pMsg, (uchar *)inst->fieldList.varname[i], tempJson, 0, 0)) {
                LogMsg(0, RS_RET_OBJ_CREATION_FAILED, LOG_ERR, "Failed to add value to '%s'",
                       inst->fieldList.varname[i]);
            }

            tempJson = NULL;
        }
    } else {
        if (RS_RET_OK != msgAddJSON(pMsg, (uchar *)"!", json, 0, 0)) {
            LogMsg(0, RS_RET_OBJ_CREATION_FAILED, LOG_ERR, "Failed to add json info to message!");
            ABORT_FINALIZE(RS_RET_OBJ_CREATION_FAILED);
        }
    }
    if (metadata != NULL && RS_RET_OK != msgAddJSON(pMsg, (uchar *)".", metadata, 0, 0)) {
        LogMsg(0, RS_RET_OBJ_CREATION_FAILED, LOG_ERR, "Failed to add metadata to message!");
        ABORT_FINALIZE(RS_RET_OBJ_CREATION_FAILED);
    }
    if (RS_RET_OK != submitMsg2(pMsg)) {
        LogMsg(0, RS_RET_OBJ_CREATION_FAILED, LOG_ERR, "Failed to submit message to main queue!");
        ABORT_FINALIZE(RS_RET_OBJ_CREATION_FAILED);
    }
    DBGPRINTF("enqMsgJson: message enqueued!\n");

finalize_it:
    RETiRet;
}


/*
 *	execute a synchronous authentication using the context conn
 *	conn and password should be non-NULL
 *	conn should be a valid context
 */
rsRetVal redisAuthentSynchronous(redisContext *conn, uchar *password) {
    DEFiRet;
    redisReply *reply = NULL;

    assert(conn != NULL);
    assert(password != NULL);
    assert(password[0] != '\0');

    reply = (redisReply *)redisCommand(conn, "AUTH %s", password);
    if (reply == NULL) {
        LogError(0, RS_RET_REDIS_ERROR, "imhiredis: Could not authenticate!\n");
        ABORT_FINALIZE(RS_RET_REDIS_ERROR);
    } else if (strncmp(reply->str, "OK", 2)) {
        LogError(0, RS_RET_REDIS_AUTH_FAILED, "imhiredis: Authentication failure -> %s\n", reply->str);
        ABORT_FINALIZE(RS_RET_REDIS_AUTH_FAILED);
    }

finalize_it:
    if (reply) freeReplyObject(reply);
    RETiRet;
}


/*
 *	execute an asynchronous authentication using the context aconn
 *	aconn and password should be non-NULL
 *	aconn should be a valid (async) context
 */
rsRetVal redisAuthentAsynchronous(redisAsyncContext *aconn, uchar *password) {
    DEFiRet;

    assert(aconn != NULL);
    assert(password != NULL);
    assert(password[0] != '\0');

    if (REDIS_OK != redisAsyncCommand(aconn, NULL, NULL, "AUTH %s", password)) {
        LogError(0, RS_RET_REDIS_ERROR, "imhiredis: error while authenticating asynchronously -> %s\n", aconn->errstr);
        ABORT_FINALIZE(RS_RET_REDIS_ERROR);
    }

finalize_it:
    RETiRet;
}


/*
 *	connect to node, authenticate (if necessary), get role, then get all node information provided by ROLE
 *	node should be a non-NULL valid redisNode pointer
 *	inst is the instance configuration object (used for SSL/password settings)
 *	result will hold the result of the ROLE command executed on the node:
 *		- NULL if the node was a single master instance
 *		- a single (master) node if the provided node was a replica
 *		- a list of (replica) nodes if the provided node was a master
 */
rsRetVal redisGetServersList(redisNode *node, instanceConf_t *inst, redisNode **result) {
    DEFiRet;
    redisContext *context;
    redisReply *reply = NULL, *replica;
    unsigned int i;

    assert(node != NULL);

    CHKiRet(redisConnectSync(&context, node));

#ifdef HIREDIS_SSL
    if (inst->use_tls && inst->ssl_conn) {
        CHKiRet(redisInitSSLContext(context, inst->ssl_conn));
    }
#endif

    if (inst->password != NULL && inst->password[0] != '\0') {
        CHKiRet(redisAuthentSynchronous(context, inst->password));
    }

    reply = getRole(context);

    if (reply == NULL) {
        LogMsg(0, RS_RET_REDIS_ERROR, LOG_WARNING, "imhiredis: did not get the role of the server");
        ABORT_FINALIZE(RS_RET_REDIS_ERROR);
    }

    /*
     * string comparisons for ROLE could be skipped
     * as each role returns a different number of elements,
     * but lets keep it as a security...
     */
    if (reply->elements == 5 && strncmp(reply->element[0]->str, "slave", 5) == 0) {
        CHKiRet(createRedisNode(result));
        (*result)->server = (uchar *)strdup((const char *)reply->element[1]->str);
        (*result)->port = reply->element[2]->integer;
        (*result)->isMaster = 1;
    } else if (reply->elements == 3 && reply->element[2]->type == REDIS_REPLY_ARRAY &&
               strncmp(reply->element[0]->str, "master", 6) == 0) {
        // iterate on all replicas given in the reply (if any)
        for (i = 0; i < reply->element[2]->elements; i++) {
            replica = reply->element[2]->element[i];

            if (replica->type == REDIS_REPLY_ARRAY && replica->elements == 3) {
                /* node will be a new node every time
                 * with old ones shifted in the list
                 */
                CHKiRet(createRedisNode(result));
                (*result)->server = (uchar *)strdup((const char *)replica->element[0]->str);
                // yes, the value in that case is a string and NOT an integer!
                (*result)->port = atoi(replica->element[1]->str);
            }
        }
    } else {
        // we have a sentinel, or a problem
        ABORT_FINALIZE(RS_RET_NOT_FOUND);
    }

finalize_it:
    if (reply != NULL) freeReplyObject(reply);
    if (context != NULL) redisFree(context);
    RETiRet;
}


/*
 *	actualize the current master node to use during connection for instance inst
 *	inst should be a valid, non-NULL instanceConf object
 *	inst should also possess at least a single node in inst->redisNodeList
 *	if the function returns RS_RET_OK, inst->currentNode and inst->redisNodeList have been both updated
 *	to reflect new master and potential replicas
 *	the first configured node (called preferred node) is always kept as the first entry in redisNodeList
 */
rsRetVal redisActualizeCurrentNode(instanceConf_t *inst) {
    DEFiRet;
    redisReply *reply = NULL;
    redisNode *node, *tmp, *newList = NULL;

    assert(inst != NULL);
    assert(inst->redisNodesList != NULL);

    inst->currentNode = NULL;
    // keep first node in list = preferred node (comes from configuration)
    copyNode(inst->redisNodesList, &newList);
    newList->next = NULL;

    for (node = inst->redisNodesList; node != NULL; node = node->next) {
        tmp = NULL;

        DBGPRINTF("imhiredis: trying to connect to node to get info...\n");
        dbgPrintNode(node);

        if (RS_RET_OK == redisGetServersList(node, inst, &tmp)) {
            // server replied

            if (tmp && tmp->isMaster) {
                DBGPRINTF("imhiredis: node replied with a master node, is a replica\n");
                // master node, keep it as potential new active node
                inst->currentNode = tmp;
                tmp = NULL;

                // try to connect to the master and get replicas
                if (RS_RET_OK != redisGetServersList(inst->currentNode, inst, &tmp)) {
                    /* had a master, but cannot connect
                     * save suspected master in new list but keep searching with other nodes
                     */
                    DBGPRINTF("imhiredis: had a master but cannot connect, keeping in list\n");
                    dbgPrintNode(inst->currentNode);
                    insertNodeAfter(newList, inst->currentNode);
                    inst->currentNode = NULL;
                    continue;
                }
            } else {
                DBGPRINTF("imhiredis: node replied with a list of replicas, is a master\n");
                // copy the node to the new currentNode, list owning node will be freed
                node->isMaster = 1;
                copyNode(node, &(inst->currentNode));
                inst->currentNode->next = NULL;
            }

            /*
             * here, tmp is a list of replicas or NULL (single node)
             * inst->currentNode is the new active master
             */

            // add the replicas to the list
            if (tmp) {
                insertNodeAfter(newList, tmp);
                DBGPRINTF("imhiredis: inserting replicas to list\n");
                for (tmp = newList->next; tmp != NULL; tmp = tmp->next) {
                    dbgPrintNode(tmp);
                }
            }
            // insert the master after the preferred node (configuration)
            DBGPRINTF("imhiredis: inserting new master node in list\n");
            dbgPrintNode(inst->currentNode);
            insertNodeAfter(newList, inst->currentNode);

            // swap newList and redisNodesList to free old list at the end of the function
            tmp = newList;
            newList = inst->redisNodesList;
            inst->redisNodesList = tmp;
            FINALIZE;
        }
    }

    DBGPRINTF("imhiredis: did not find a valid master");
    iRet = RS_RET_NOT_FOUND;
    inst->currentNode = NULL;

finalize_it:
    if (reply != NULL) freeReplyObject(reply);
    // newList is always completely freed
    for (node = newList; node != NULL;) {
        node = freeNode(node);
    }

    RETiRet;
}


/*
 *	authentication function, for both synchronous and asynchronous modes (queue or subscribe)
 *	inst, inst->curentMode and inst->password should not be NULL
 */
rsRetVal redisAuthenticate(instanceConf_t *inst) {
    DEFiRet;
    redisContext *usedContext = NULL;
    redisReply *reply = NULL;

    assert(inst != NULL);
    assert(inst->currentNode != NULL);
    assert(inst->password != NULL);
    assert(inst->password[0] != '\0');

    DBGPRINTF("imhiredis: authenticating...\n");

    // Create a temporary context for synchronous connection, used to validate AUTH command in asynchronous contexts
    if (inst->mode == IMHIREDIS_MODE_SUBSCRIBE) {
        if (RS_RET_OK != redisConnectSync(&usedContext, inst->currentNode)) {
            LogMsg(0, RS_RET_REDIS_ERROR, LOG_WARNING,
                   "imhiredis: could not connect to current "
                   "active node synchronously to validate authentication");
            ABORT_FINALIZE(RS_RET_REDIS_ERROR);
        }
#ifdef HIREDIS_SSL
        if (inst->use_tls && inst->ssl_conn) {
            CHKiRet(redisInitSSLContext(usedContext, inst->ssl_conn));
        }
#endif
    } else {
        usedContext = inst->conn;
    }

    /*
     * Try synchronous connection, whatever the method for the instance
     * This is also done for the asynchronous mode, to validate the successful authentication
     */
    CHKiRet(redisAuthentSynchronous(usedContext, inst->password));

    if (inst->mode == IMHIREDIS_MODE_SUBSCRIBE) {
        CHKiRet(redisAuthentAsynchronous(inst->aconn, inst->password));
    }

    DBGPRINTF("imhiredis: authentication successful\n");

finalize_it:
    if (inst->mode == IMHIREDIS_MODE_SUBSCRIBE && usedContext) redisFree(usedContext);
    if (reply) freeReplyObject(reply);
    RETiRet;
}


#ifdef HIREDIS_SSL
rsRetVal redisInitSSLContext(redisContext *conn, redisSSLContext *ssl_context) {
    DEFiRet;

    assert(conn != NULL);
    assert(ssl_context != NULL);

    if (redisInitiateSSLWithContext(conn, ssl_context) != REDIS_OK) {
        LogError(0, RS_RET_REDIS_ERROR, "imhiredis error while initializing TLS context: %s", conn->errstr);
        ABORT_FINALIZE(RS_RET_REDIS_ERROR);
    }
finalize_it:
    RETiRet;
}
#endif


/*
 *	connection function for synchronous (queue) mode
 *	node should not be NULL
 */
rsRetVal redisConnectSync(redisContext **conn, redisNode *node) {
    DEFiRet;
    redisOptions options = {0};

    options.connect_timeout = &glblRedisConnectTimeout;
    options.command_timeout = &glblRedisCommandTimeout;

    assert(node != NULL);

    if (node->usesSocket)
        REDIS_OPTIONS_SET_UNIX(&options, (const char *)node->socketPath);
    else
        REDIS_OPTIONS_SET_TCP(&options, (const char *)node->server, node->port);

    *conn = redisConnectWithOptions(&options);

    if (*conn == NULL) {
        if (node->usesSocket) {
            LogError(0, RS_RET_REDIS_ERROR,
                     "imhiredis: can not connect to redis server '%s' "
                     "-> could not allocate context!\n",
                     node->socketPath);
        } else {
            LogError(0, RS_RET_REDIS_ERROR,
                     "imhiredis: can not connect to redis server '%s', "
                     "port %d -> could not allocate context!\n",
                     node->server, node->port);
        }
        ABORT_FINALIZE(RS_RET_REDIS_ERROR);
    } else if ((*conn)->err) {
        if (node->usesSocket) {
            LogError(0, RS_RET_REDIS_ERROR,
                     "imhiredis: can not connect to redis server '%s' "
                     "-> %s\n",
                     node->socketPath, (*conn)->errstr);
        } else {
            LogError(0, RS_RET_REDIS_ERROR,
                     "imhiredis: can not connect to redis server '%s', "
                     "port %d -> %s\n",
                     node->server, node->port, (*conn)->errstr);
        }
        ABORT_FINALIZE(RS_RET_REDIS_ERROR);
    }

finalize_it:
    if (iRet != RS_RET_OK) {
        if (*conn) redisFree(*conn);
        *conn = NULL;
    }
    RETiRet;
}


/*
 *	connection function for asynchronous (subscribe) mode
 *	node should not be NULL
 */
rsRetVal redisConnectAsync(redisAsyncContext **aconn, redisNode *node) {
    DEFiRet;
    redisOptions options = {0};

    options.connect_timeout = &glblRedisConnectTimeout;
    options.command_timeout = &glblRedisCommandTimeout;

    assert(node != NULL);

    if (node->usesSocket)
        REDIS_OPTIONS_SET_UNIX(&options, (const char *)node->socketPath);
    else
        REDIS_OPTIONS_SET_TCP(&options, (const char *)node->server, node->port);

    *aconn = redisAsyncConnectWithOptions(&options);

    if (*aconn == NULL) {
        LogError(0, RS_RET_REDIS_ERROR, "imhiredis (async): could not allocate context!\n");
        ABORT_FINALIZE(RS_RET_REDIS_ERROR);
    } else if ((*aconn)->err) {
        if (node->usesSocket) {
            LogError(0, RS_RET_REDIS_ERROR,
                     "imhiredis (async): cannot connect to server '%s' "
                     "-> %s\n",
                     node->socketPath, (*aconn)->errstr);
        } else {
            LogError(0, RS_RET_REDIS_ERROR,
                     "imhiredis (async): cannot connect to server '%s', port '%d' "
                     "-> %s\n",
                     node->server, node->port, (*aconn)->errstr);
        }
        ABORT_FINALIZE(RS_RET_REDIS_ERROR);
    }

finalize_it:
    if (iRet != RS_RET_OK) {
        if (*aconn) redisAsyncFree(*aconn);
        *aconn = NULL;
    }
    RETiRet;
}

/*
 *	Helper method to connect to the current master asynchronously
 *	'inst' parameter should be non-NULL and have a valid currentNode object
 */
rsRetVal connectMasterAsync(instanceConf_t *inst) {
    DEFiRet;

    if (RS_RET_OK != redisConnectAsync(&(inst->aconn), inst->currentNode)) {
        inst->currentNode = NULL;
        ABORT_FINALIZE(RS_RET_REDIS_ERROR);
    }

#ifdef HIREDIS_SSL
    if (inst->use_tls && inst->ssl_conn && redisInitSSLContext(&(inst->aconn->c), inst->ssl_conn) != RS_RET_OK) {
        redisAsyncFree(inst->aconn);
        inst->aconn = NULL;
        inst->currentNode = NULL;
        ABORT_FINALIZE(RS_RET_REDIS_ERROR);
    }
#endif

    if (inst->password != NULL && inst->password[0] != '\0' && RS_RET_OK != redisAuthenticate(inst)) {
        redisAsyncFree(inst->aconn);
        inst->aconn = NULL;
        inst->currentNode = NULL;
        ABORT_FINALIZE(RS_RET_REDIS_AUTH_FAILED);
    }

    // finalize context creation
    inst->aconn->data = (void *)inst;
    redisAsyncSetConnectCallback(inst->aconn, redisAsyncConnectCallback);
    redisAsyncSetDisconnectCallback(inst->aconn, redisAsyncDisconnectCallback);
    redisLibeventAttach(inst->aconn, inst->evtBase);

finalize_it:
    RETiRet;
}


/*
 *	Helper method to check if (async) instance is connected
 */
static sbool isConnectedAsync(instanceConf_t *inst) {
    return inst->aconn != NULL;
}


/*
 *	Helper method to connect to the current master synchronously
 *	'inst' parameter should be non-NULL and have a valid currentNode object
 */
rsRetVal connectMasterSync(instanceConf_t *inst) {
    DEFiRet;

    if (RS_RET_OK != redisConnectSync(&(inst->conn), inst->currentNode)) {
        inst->currentNode = NULL;
        ABORT_FINALIZE(RS_RET_REDIS_ERROR);
    }

#ifdef HIREDIS_SSL
    if (inst->use_tls && inst->ssl_conn) {
        if (redisInitSSLContext(inst->conn, inst->ssl_conn) != RS_RET_OK) {
            redisFree(inst->conn);
            inst->conn = NULL;
            inst->currentNode = NULL;
            ABORT_FINALIZE(RS_RET_REDIS_ERROR);
        }
    }
#endif

    if (inst->password != NULL && inst->password[0] != '\0' && RS_RET_OK != redisAuthenticate(inst)) {
        redisFree(inst->conn);
        inst->conn = NULL;
        inst->currentNode = NULL;
        ABORT_FINALIZE(RS_RET_REDIS_AUTH_FAILED);
    }

finalize_it:
    RETiRet;
}


/*
 *	Helper method to check if instance is connected
 */
static sbool isConnectedSync(instanceConf_t *inst) {
    return inst->conn != NULL;
}

/*
 *	dequeue all entries in the redis list, using batches of 10 commands
 */
rsRetVal redisDequeue(instanceConf_t *inst) {
    DEFiRet;
    redisReply *reply = NULL;
    uint replyType = 0, i;

    assert(inst != NULL);

    DBGPRINTF("redisDequeue: beginning to dequeue key '%s'\n", inst->key);

    do {
        // append a batch of inst->batchsize POP commands (either LPOP or RPOP depending on conf)
        if (inst->useLPop == 1) {
            DBGPRINTF("redisDequeue: Queuing #%d LPOP commands on key '%s' \n", inst->batchsize, inst->key);
            for (i = 0; i < inst->batchsize; ++i) {
                if (REDIS_OK != redisAppendCommand(inst->conn, "LPOP %s", inst->key)) break;
            }
        } else {
            DBGPRINTF("redisDequeue: Queuing #%d RPOP commands on key '%s' \n", inst->batchsize, inst->key);
            for (i = 0; i < inst->batchsize; i++) {
                if (REDIS_OK != redisAppendCommand(inst->conn, "RPOP %s", inst->key)) break;
            }
        }

        DBGPRINTF("redisDequeue: Dequeuing...\n")
        // parse responses from appended commands
        do {
            if (REDIS_OK != redisGetReply(inst->conn, (void **)&reply)) {
                // error getting reply, must stop
                LogError(0, RS_RET_REDIS_ERROR,
                         "redisDequeue: Error reading reply after POP #%d "
                         "on key '%s'",
                         (inst->batchsize - i), inst->key);
                // close connection
                redisFree(inst->conn);
                inst->currentNode = NULL;
                inst->conn = NULL;
                ABORT_FINALIZE(RS_RET_REDIS_ERROR);
            } else {
                if (reply != NULL) {
                    replyType = reply->type;
                    switch (replyType) {
                        case REDIS_REPLY_STRING:
                            enqMsg(inst, reply->str, reply->len);
                            break;
                        case REDIS_REPLY_NIL:
                            // replies are dequeued but are empty = end of list
                            break;
                        case REDIS_REPLY_ERROR:
                            // There is a problem with the key or the Redis instance
                            LogMsg(0, RS_RET_REDIS_ERROR, LOG_ERR,
                                   "redisDequeue: error "
                                   "while POP'ing key '%s' -> %s",
                                   inst->key, reply->str);
                            ABORT_FINALIZE(RS_RET_REDIS_ERROR);
                        default:
                            LogMsg(0, RS_RET_OK_WARN, LOG_WARNING,
                                   "redisDequeue: "
                                   "unexpected reply type: %s",
                                   REDIS_REPLIES[replyType % 15]);
                    }
                    freeReplyObject(reply);
                    reply = NULL;
                } else { /* reply == NULL */
                    LogMsg(0, RS_RET_REDIS_ERROR, LOG_ERR,
                           "redisDequeue: unexpected empty reply "
                           "for successful return");
                    ABORT_FINALIZE(RS_RET_REDIS_ERROR);
                }
            }

            // while there are replies to unpack, continue
        } while (--i > 0);

        if (replyType == REDIS_REPLY_NIL) {
            /* sleep 1s between 2 POP tries, when no new entries are available (list is empty)
             * this does NOT limit dequeing rate, but prevents the input from polling Redis too often
             */
            for (i = 0; i < 10; i++) {
                // Time to stop the thread
                if (glbl.GetGlobalInputTermState() != 0) FINALIZE;
                // 100ms sleeps
                srSleep(0, 100000);
            }
        }

        // while input can run, continue with a new batch
    } while (glbl.GetGlobalInputTermState() == 0);

    DBGPRINTF("redisDequeue: finished to dequeue key '%s'\n", inst->key);

finalize_it:
    if (reply) freeReplyObject(reply);
    RETiRet;
}


rsRetVal ensureConsumerGroupCreated(instanceConf_t *inst) {
    DEFiRet;
    redisReply *reply = NULL;

    DBGPRINTF("ensureConsumerGroupCreated: Creating group %s on stream %s\n", inst->streamConsumerGroup, inst->key);

    reply = (redisReply *)redisCommand(inst->conn, "XGROUP CREATE %s %s %s MKSTREAM", inst->key,
                                       inst->streamConsumerGroup, inst->streamReadFrom);
    if (reply != NULL) {
        switch (reply->type) {
            case REDIS_REPLY_STATUS:
            case REDIS_REPLY_STRING:
                if (0 == strncmp("OK", reply->str, reply->len))
                    DBGPRINTF(
                        "ensureConsumerGroupCreated: Consumer group %s created successfully "
                        "for stream %s\n",
                        inst->streamConsumerGroup, inst->key);
                break;
            case REDIS_REPLY_ERROR:
                if (strcasestr(reply->str, "BUSYGROUP") != NULL) {
                    DBGPRINTF(
                        "ensureConsumerGroupCreated: Consumer group %s already exists for "
                        "stream %s, ignoring\n",
                        inst->streamConsumerGroup, inst->key);
                } else {
                    LogError(0, RS_RET_ERR,
                             "ensureConsumerGroupCreated: An unknown error "
                             "occurred while creating a Consumer group %s on stream %s -> %s",
                             inst->streamConsumerGroup, inst->key, reply->str);
                    ABORT_FINALIZE(RS_RET_ERR);
                }
                break;
            default:
                LogError(0, RS_RET_ERR,
                         "ensureConsumerGroupCreated: An unknown reply was received "
                         "-> %s",
                         REDIS_REPLIES[(reply->type) % 15]);
                ABORT_FINALIZE(RS_RET_ERR);
        }
    } else {
        LogError(0, RS_RET_REDIS_ERROR, "ensureConsumerGroupCreated: Could not create group %s on stream %s!",
                 inst->streamConsumerGroup, inst->key);
        ABORT_FINALIZE(RS_RET_REDIS_ERROR);
    }

finalize_it:
    if (reply != NULL) freeReplyObject(reply);
    RETiRet;
}


rsRetVal ackStreamIndex(instanceConf_t *inst, uchar *stream, uchar *group, uchar *index) {
    DEFiRet;
    redisReply *reply = NULL;

    DBGPRINTF("ackStream: Acknowledging index '%s' in stream %s\n", index, stream);

    reply = (redisReply *)redisCommand(inst->conn, "XACK %s %s %s", stream, group, index);
    if (reply != NULL) {
        switch (reply->type) {
            case REDIS_REPLY_INTEGER:
                if (reply->integer == 1) {
                    DBGPRINTF(
                        "ackStreamIndex: index successfully acknowledged "
                        "for stream %s\n",
                        inst->key);
                } else {
                    DBGPRINTF(
                        "ackStreamIndex: message was not acknowledged "
                        "-> already done?");
                }
                break;
            case REDIS_REPLY_ERROR:
                LogError(0, RS_RET_ERR,
                         "ackStreamIndex: An error occurred "
                         "while trying to ACK message %s on %s[%s] -> %s",
                         index, stream, group, reply->str);
                ABORT_FINALIZE(RS_RET_ERR);
            default:
                LogError(0, RS_RET_ERR, "ackStreamIndex: unexpected reply type: %s", REDIS_REPLIES[(reply->type) % 15]);
                ABORT_FINALIZE(RS_RET_ERR);
        }

    } else {
        LogError(0, RS_RET_REDIS_ERROR, "ackStreamIndex: Could not ACK message with index %s for %s[%s]!", index,
                 stream, group);
        ABORT_FINALIZE(RS_RET_REDIS_ERROR);
    }

finalize_it:
    if (reply != NULL) {
        freeReplyObject(reply);
    }
    RETiRet;
}


static rsRetVal enqueueRedisStreamReply(instanceConf_t *const inst, redisReply *reply) {
    DEFiRet;
    struct json_object *json = NULL, *metadata = NULL, *redis = NULL;

    json = _redisParseArrayReply(reply->element[1]);

    CHKmalloc(metadata = json_object_new_object());
    CHKmalloc(redis = json_object_new_object());
    json_object_object_add(redis, "stream", json_object_new_string((char *)inst->key));
    json_object_object_add(redis, "index", _redisParseStringReply(reply->element[0]));
    if (inst->streamConsumerGroup != NULL) {
        json_object_object_add(redis, "group", json_object_new_string((char *)inst->streamConsumerGroup));
    }
    if (inst->streamConsumerName != NULL) {
        json_object_object_add(redis, "consumer", json_object_new_string((char *)inst->streamConsumerName));
    }

    // ownership of redis object allocated by json_object_new_object() is taken by json
    // no need to free/destroy/put redis object
    json_object_object_add(metadata, "redis", redis);

    CHKiRet(enqMsgJson(inst, json, metadata));
    // enqueued message successfully, json and metadata objects are now owned by enqueued message
    // no need to free/destroy/put json objects
    json = NULL;
    metadata = NULL;

    if (inst->streamConsumerGroup != NULL && inst->streamConsumerACK) {
        CHKiRet(ackStreamIndex(inst, (uchar *)inst->key, inst->streamConsumerGroup, (uchar *)reply->element[0]->str));
    }

finalize_it:
    // If that happens, there was an error during one of the steps and the json object is not enqueued
    if (json != NULL) json_object_put(json);
    if (metadata != NULL) json_object_put(metadata);
    RETiRet;
}


/*
 *	handle the hiredis Stream XREAD/XREADGROUP return objects. The provided reply is
 *	not freed - this must be done by the caller.
 *	example of stream to parse:
 *	  1) 1) "mystream"			<- name of the stream indexes are from (list of streams requested)
 *	     2) 1) 1) "1681749395006-0"		<- list of indexes returned for stream
 *	           2) 1) "key1"
 *	              2) "value1"
 *	        2) 1) "1681749409349-0"
 *	           2) 1) "key2"
 *	              2) "value2"
 *	              3) "key2.2"
 *	              4) "value2.2"
 *	json equivalent:
 *	  [
 *	    "mystream": [
 *	      {
 *	        "1681749395006-0": {
 *	          "key1": "value1"
 *	        }
 *	      },
 *	      {
 *	        "1681749409349-0": {
 *	          "key2": "value2",
 *	          "key2.2": "value2.2"
 *	        }
 *	      }
 *	    ]
 *	  ]
 */
static rsRetVal handleRedisXREADReply(instanceConf_t *const inst, const redisReply *reply) {
    DEFiRet;
    redisReply *streamObj = NULL, *msgList = NULL, *msgObj = NULL;

    if (reply == NULL || reply->type != REDIS_REPLY_ARRAY) {
        /* we do not process empty or non-ARRAY lines */
        DBGPRINTF("handleRedisXREADReply: object is not an array, ignoring\n");
        LogMsg(0, RS_RET_OK_WARN, LOG_WARNING, "handleRedisXREADReply: object is not an array, ignoring");
        ABORT_FINALIZE(RS_RET_OK_WARN);
    } else {
        // iterating on streams
        for (size_t i = 0; i < reply->elements; i++) {
            streamObj = reply->element[i];
            // object should contain the name of the stream, and an array containing the messages
            if (streamObj->type != REDIS_REPLY_ARRAY || streamObj->elements != 2) {
                LogMsg(0, RS_RET_OK_WARN, LOG_WARNING,
                       "handleRedisXREADReply: wrong object format, "
                       "object should contain the name of the stream and an array of messages");
                ABORT_FINALIZE(RS_RET_OK_WARN);
            }
            if (streamObj->element[0]->type != REDIS_REPLY_STRING) {
                LogMsg(0, RS_RET_OK_WARN, LOG_WARNING,
                       "handleRedisXREADReply: wrong field format, "
                       "first entry is not a string (supposed to be stream name)");
                ABORT_FINALIZE(RS_RET_OK_WARN);
            }

            msgList = streamObj->element[1];

            if (msgList->type != REDIS_REPLY_ARRAY) {
                LogMsg(0, RS_RET_OK_WARN, LOG_WARNING,
                       "handleRedisXREADReply: wrong field format, "
                       "second entry is not an array (supposed to be list of messages for stream)");
                ABORT_FINALIZE(RS_RET_OK_WARN);
            }

            DBGPRINTF("handleRedisXREADReply: enqueuing messages for stream '%s'\n", streamObj->element[0]->str);

            for (size_t j = 0; j < msgList->elements; j++) {
                msgObj = msgList->element[j];
                // Object should contain the name of the index, and its content(s)
                if (msgObj->type != REDIS_REPLY_ARRAY || msgObj->elements != 2) {
                    LogMsg(0, RS_RET_OK_WARN, LOG_WARNING,
                           "handleRedisXREADReply: wrong object "
                           "format, object should contain the index and its content(s)");
                    ABORT_FINALIZE(RS_RET_OK_WARN);
                }
                if (msgObj->element[0]->type != REDIS_REPLY_STRING) {
                    LogMsg(0, RS_RET_OK_WARN, LOG_WARNING,
                           "handleRedisXREADReply: wrong field "
                           "format, first entry should be a string (index name)");
                    ABORT_FINALIZE(RS_RET_OK_WARN);
                }

                if (msgObj->type != REDIS_REPLY_ARRAY) {
                    LogMsg(0, RS_RET_OK_WARN, LOG_WARNING,
                           "handleRedisXREADReply: wrong field "
                           "format, second entry should be an array (index content(s))");
                    ABORT_FINALIZE(RS_RET_OK_WARN);
                }

                CHKiRet(enqueueRedisStreamReply(inst, msgObj));

                // Update current stream index
                memcpy(inst->streamReadFrom, msgObj->element[0]->str, msgObj->element[0]->len);
                inst->streamReadFrom[msgObj->element[0]->len] = '\0';
                DBGPRINTF("handleRedisXREADReply: current stream index is %s\n", inst->streamReadFrom);
            }
        }
    }

    DBGPRINTF("handleRedisXREADReply: finished enqueuing!\n");
finalize_it:
    RETiRet;
}


/*
 *	handle the hiredis Stream XAUTOCLAIM return object. The provided reply is
 *	not freed - this must be done by the caller.
 *	example of stream to parse:
 *	  1) "1681904437564-0"		<- next index to use for XAUTOCLAIM
 *	  2)  1) 1) "1681904437525-0"	<- list of indexes reclaimed
 *	         2) 1) "toto"
 *	            2) "tata"
 *	      2) 1) "1681904437532-0"
 *	         2) 1) "titi"
 *	            2) "tutu"
 *	  3) (empty)			<- indexes that no longer exist, were deleted from the PEL
 *	json equivalent:
 *	  "1681904437564-0": [
 *	    {
 *	      "1681904437525-0": {
 *	        "toto": "tata"
 *	      }
 *	    },
 *	    {
 *	      "1681904437532-0": {
 *	        "titi": "tutu"
 *	      }
 *	    }
 *	  ]
 */
static rsRetVal handleRedisXAUTOCLAIMReply(instanceConf_t *const inst, const redisReply *reply, char **autoclaimIndex) {
    DEFiRet;
    redisReply *msgList = NULL, *msgObj = NULL;

    if (reply == NULL || reply->type != REDIS_REPLY_ARRAY) {
        /* we do not process empty or non-ARRAY lines */
        DBGPRINTF("handleRedisXAUTOCLAIMReply: object is not an array, ignoring\n");
        FINALIZE;
    } else {
        // Object should contain between 2 and 3 elements (depends on Redis server version)
        if (reply->elements < 2 || reply->elements > 3) {
            LogMsg(0, RS_RET_OK_WARN, LOG_WARNING,
                   "handleRedisXAUTOCLAIMReply: wrong number of fields, "
                   "cannot process entry");
            ABORT_FINALIZE(RS_RET_OK_WARN);
        }
        if (reply->element[0]->type != REDIS_REPLY_STRING) {
            LogMsg(0, RS_RET_OK_WARN, LOG_WARNING,
                   "handleRedisXAUTOCLAIMReply: the first element "
                   "is not a string, cannot process entry");
            ABORT_FINALIZE(RS_RET_OK_WARN);
        }

        msgList = reply->element[1];

        if (msgList->type != REDIS_REPLY_ARRAY) {
            LogMsg(0, RS_RET_OK_WARN, LOG_WARNING,
                   "handleRedisXAUTOCLAIMReply: the second element "
                   "is not an array, cannot process entry");
            ABORT_FINALIZE(RS_RET_OK_WARN);
        }

        DBGPRINTF("handleRedisXAUTOCLAIMReply: re-claiming messages for stream '%s'\n", inst->key);

        for (size_t j = 0; j < msgList->elements; j++) {
            msgObj = msgList->element[j];
            // Object should contain the name of the index, and its content(s)
            if (msgObj->type != REDIS_REPLY_ARRAY || msgObj->elements != 2) {
                LogMsg(0, RS_RET_OK_WARN, LOG_WARNING,
                       "handleRedisXAUTOCLAIMReply: wrong message "
                       "format, cannot process");
                ABORT_FINALIZE(RS_RET_OK_WARN);
            }
            if (msgObj->element[0]->type != REDIS_REPLY_STRING) {
                LogMsg(0, RS_RET_OK_WARN, LOG_WARNING,
                       "handleRedisXAUTOCLAIMReply: first message "
                       "element not a string, cannot process");
                ABORT_FINALIZE(RS_RET_OK_WARN);
            }

            if (msgObj->type != REDIS_REPLY_ARRAY) {
                LogMsg(0, RS_RET_OK_WARN, LOG_WARNING,
                       "handleRedisXAUTOCLAIMReply: second message "
                       "element not an array, cannot process");
                ABORT_FINALIZE(RS_RET_OK_WARN);
            }

            CHKiRet(enqueueRedisStreamReply(inst, msgObj));
        }

        // Update current stream index with next index from XAUTOCLAIM
        // No message has to be claimed after that if value is "0-0"
        memcpy(*autoclaimIndex, reply->element[0]->str, reply->element[0]->len);
        (*autoclaimIndex)[reply->element[0]->len] = '\0';
        DBGPRINTF("handleRedisXAUTOCLAIMReply: next stream index is %s\n", (*autoclaimIndex));
    }

    DBGPRINTF("handleRedisXAUTOCLAIMReply: finished re-claiming!\n");
finalize_it:
    RETiRet;
}


/*
 *	Read Redis stream
 */
rsRetVal redisStreamRead(instanceConf_t *inst) {
    DEFiRet;
    redisReply *reply = NULL;
    uint replyType = 0;
    sbool mustClaimIdle = 0;
    char *autoclaimIndex = NULL;

    assert(inst != NULL);

    // Ensure stream group is created before reading from it
    if (inst->streamConsumerGroup != NULL) {
        CHKiRet(ensureConsumerGroupCreated(inst));
    }


    if (inst->streamAutoclaimIdleTime != 0) {
        DBGPRINTF("redisStreamRead: getting pending entries for stream '%s' from '%s', with idle time %d\n", inst->key,
                  inst->streamReadFrom, inst->streamAutoclaimIdleTime);
        CHKmalloc(autoclaimIndex = calloc(1, STREAM_INDEX_STR_MAXLEN));
        // Cannot claim from '$', will have to claim from the beginning of the stream
        if (inst->streamReadFrom[0] == '$') {
            LogMsg(0, RS_RET_OK, LOG_WARNING,
                   "Cannot claim pending entries from '$', "
                   "will have to claim from the beginning of the stream");
            memcpy(autoclaimIndex, "0-0", 4);
        } else {
            memcpy(autoclaimIndex, inst->streamReadFrom, STREAM_INDEX_STR_MAXLEN);
        }
        mustClaimIdle = 1;
    } else {
        DBGPRINTF("redisStreamRead: beginning to read stream '%s' from '%s'\n", inst->key, inst->streamReadFrom);
    }

    do {
        if (inst->streamConsumerGroup == NULL) {
            reply = (redisReply *)redisCommand(inst->conn, "XREAD COUNT %d BLOCK %d STREAMS %s %s", BATCH_SIZE,
                                               WAIT_TIME_MS, inst->key, inst->streamReadFrom);
        } else {
            if (mustClaimIdle) {
                reply = (redisReply *)redisCommand(inst->conn, "XAUTOCLAIM %s %s %s %d %s COUNT %d", inst->key,
                                                   inst->streamConsumerGroup, inst->streamConsumerName,
                                                   inst->streamAutoclaimIdleTime, autoclaimIndex, BATCH_SIZE);
            } else {
                reply = (redisReply *)redisCommand(inst->conn, "XREADGROUP GROUP %s %s COUNT %d BLOCK %d STREAMS %s >",
                                                   inst->streamConsumerGroup, inst->streamConsumerName, BATCH_SIZE,
                                                   WAIT_TIME_MS, inst->key);
            }
        }
        if (reply == NULL) {
            LogError(0, RS_RET_REDIS_ERROR, "redisStreamRead: Error while trying to read stream '%s'", inst->key);
            ABORT_FINALIZE(RS_RET_REDIS_ERROR);
        }

        replyType = reply->type;
        switch (replyType) {
            case REDIS_REPLY_ARRAY:
                DBGPRINTF("reply is an array, proceeding...\n");
                if (mustClaimIdle) {
                    CHKiRet(handleRedisXAUTOCLAIMReply(inst, reply, &autoclaimIndex));
                    if (!strncmp(autoclaimIndex, "0-0", 4)) {
                        DBGPRINTF(
                            "redisStreamRead: Caught up with pending messages, "
                            "getting back to regular reads\n");
                        mustClaimIdle = 0;
                    }
                } else {
                    CHKiRet(handleRedisXREADReply(inst, reply));
                }
                break;
            case REDIS_REPLY_NIL:
                // replies are dequeued but are empty = end of list
                if (mustClaimIdle) mustClaimIdle = 0;
                break;
            case REDIS_REPLY_ERROR:
                // There is a problem with the key or the Redis instance
                LogMsg(0, RS_RET_REDIS_ERROR, LOG_ERR,
                       "redisStreamRead: error "
                       "while reading stream(s) -> %s",
                       reply->str);
                srSleep(1, 0);
                ABORT_FINALIZE(RS_RET_REDIS_ERROR);
            default:
                LogMsg(0, RS_RET_OK_WARN, LOG_WARNING,
                       "redisStreamRead: unexpected "
                       "reply type: %s",
                       REDIS_REPLIES[replyType % 15]);
        }
        freeReplyObject(reply);
        reply = NULL;

        // while input can run, continue with a new batch
    } while (glbl.GetGlobalInputTermState() == 0);

    DBGPRINTF("redisStreamRead: finished to dequeue key '%s'\n", inst->key);

finalize_it:
    if (reply != NULL) freeReplyObject(reply);
    if (inst->conn != NULL) {
        redisFree(inst->conn);
        inst->conn = NULL;
        inst->currentNode = NULL;
    }
    if (autoclaimIndex != NULL) free(autoclaimIndex);
    RETiRet;
}


/*
 *	Subscribe to Redis channel
 */
rsRetVal redisSubscribe(instanceConf_t *inst) {
    DEFiRet;

    DBGPRINTF("redisSubscribe: subscribing to channel '%s'\n", inst->key);
    int ret = redisAsyncCommand(inst->aconn, redisAsyncRecvCallback, NULL, "SUBSCRIBE %s", inst->key);

    if (ret != REDIS_OK) {
        LogMsg(0, RS_RET_REDIS_ERROR, LOG_ERR, "redisSubscribe: Could not subscribe");
        ABORT_FINALIZE(RS_RET_REDIS_ERROR);
    }

    // Will block on this function as long as connection is open and event loop is not stopped
    event_base_dispatch(inst->evtBase);
    DBGPRINTF("redisSubscribe: finished.\n");

finalize_it:
    RETiRet;
}


/*
 *	generic worker function
 */
void workerLoop(struct imhiredisWrkrInfo_s *me) {
    uint i;
    DBGPRINTF("workerLoop: beginning of worker loop...\n");

    // Connect first time without delay
    if (me->inst->currentNode != NULL) {
        rsRetVal ret = me->fnConnectMaster(me->inst);
        if (ret != RS_RET_OK) {
            LogMsg(0, ret, LOG_WARNING, "workerLoop: Could not connect successfully to master");
        }
    }

    while (glbl.GetGlobalInputTermState() == 0) {
        if (!me->fnIsConnected(me->inst)) {
            /*
             * Sleep 10 seconds before attempting to resume a broken connexion
             * (sleep small amounts to avoid missing termination status)
             */
            LogMsg(0, RS_RET_OK, LOG_INFO,
                   "workerLoop: "
                   "no valid connection, sleeping 10 seconds before retrying...");
            for (i = 0; i < 100; i++) {
                // Rsyslog asked for shutdown, thread should be stopped
                if (glbl.GetGlobalInputTermState() != 0) goto end_loop;
                // 100ms sleeps
                srSleep(0, 100000);
            }

            // search the current master node
            if (me->inst->currentNode == NULL) {
                if (RS_RET_OK != redisActualizeCurrentNode(me->inst)) continue;
            }

            // connect to current master
            if (me->inst->currentNode != NULL) {
                rsRetVal ret = me->fnConnectMaster(me->inst);
                if (ret != RS_RET_OK) {
                    LogMsg(0, ret, LOG_WARNING,
                           "workerLoop: "
                           "Could not connect successfully to master");
                }
            }
        }
        if (me->fnIsConnected(me->inst)) {
            me->fnRun(me->inst);
        }
    }

end_loop:
    return;
}


/*
 *	Workerthread function for a single hiredis consumer
 */
static void *imhirediswrkr(void *myself) {
    struct imhiredisWrkrInfo_s *me = (struct imhiredisWrkrInfo_s *)myself;
    DBGPRINTF("imhiredis: started hiredis consumer workerthread\n");
    dbgPrintNode(me->inst->currentNode);

    if (me->inst->mode == IMHIREDIS_MODE_QUEUE) {
        me->fnConnectMaster = connectMasterSync;
        me->fnIsConnected = isConnectedSync;
        me->fnRun = redisDequeue;
    } else if (me->inst->mode == IMHIREDIS_MODE_STREAM) {
        me->fnConnectMaster = connectMasterSync;
        me->fnIsConnected = isConnectedSync;
        me->fnRun = redisStreamRead;
    } else if (me->inst->mode == IMHIREDIS_MODE_SUBSCRIBE) {
        me->fnConnectMaster = connectMasterAsync;
        me->fnIsConnected = isConnectedAsync;
        me->fnRun = redisSubscribe;
    }

    workerLoop(me);

    DBGPRINTF("imhiredis: stopped hiredis consumer workerthread\n");
    return NULL;
}


// -------------------------- redisNode functions -----------------------------------

/*
 *	create a redisNode and set default values
 *	if a valid node is given as parameter, the new node is inserted as the new head of the linked list
 */
static rsRetVal createRedisNode(redisNode **root) {
    redisNode *node;
    DEFiRet;
    CHKmalloc(node = malloc(sizeof(redisNode)));
    node->port = 0;
    node->server = NULL;
    node->socketPath = NULL;
    node->usesSocket = 0;
    node->isMaster = 0;
    node->next = NULL;

    if ((*root) == NULL) {
        *root = node;
    } else {
        node->next = (*root);
        *root = node;
    }

finalize_it:
    RETiRet;
}

/*
 *	make a complete copy of the src node into the newly-created node in dst
 *	if dst already contains a node, the new node will be added as the new head of the provided list
 *	src should not be NULL
 */
rsRetVal copyNode(redisNode *src, redisNode **dst) {
    DEFiRet;

    assert(src != NULL);

    CHKiRet(createRedisNode(dst));

    (*dst)->isMaster = src->isMaster;
    (*dst)->next = src->next;
    (*dst)->port = src->port;
    (*dst)->usesSocket = src->usesSocket;

    if (src->server) (*dst)->server = (uchar *)strdup((const char *)src->server);
    if (src->socketPath) (*dst)->socketPath = (uchar *)strdup((const char *)src->socketPath);

finalize_it:
    RETiRet;
}

/*
 *	free all ressources of the node
 *	will return next node if one is present, NULL otherwise
 */
redisNode *freeNode(redisNode *node) {
    redisNode *ret = NULL;
    if (node != NULL) {
        if (node->next != NULL) ret = node->next;

        if (node->server != NULL) free(node->server);
        if (node->socketPath != NULL) free(node->socketPath);
        free(node);
    }

    return ret;
}

/*
 *	insert node 'elem' after node 'root' in the linked list
 *	both root and elem should not be NULL
 */
void insertNodeAfter(redisNode *root, redisNode *elem) {
    assert(root != NULL);
    assert(elem != NULL);

    if (root->next != NULL) {
        elem->next = root->next;
    }
    root->next = elem;

    return;
}

void dbgPrintNode(redisNode *node) {
    if (node != NULL) {
        if (node->usesSocket) {
            if (node->isMaster) {
                DBGPRINTF("imhiredis: node is %s (master)\n", node->socketPath);
            } else {
                DBGPRINTF("imhiredis: node is %s (replica)\n", node->socketPath);
            }
        } else {
            if (node->isMaster) {
                DBGPRINTF("imhiredis: node is %s:%d (master)\n", node->server, node->port);
            } else {
                DBGPRINTF("imhiredis: node is %s:%d (replica)\n", node->server, node->port);
            }
        }
    }
    return;
}
