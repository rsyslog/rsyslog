/* omhiredis.c
 * Copyright 2012 Talksum, Inc
 * Copyright 2015 DigitalOcean, Inc
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
 * Author: Brian Knox
 * <bknox@digitalocean.com>
 *
 * Author: Jérémie Jourdin (TLS support)
 * <jeremie.jourdin@advens.fr>
 */


#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <hiredis/hiredis.h>
#ifdef HIREDIS_SSL
    #include <hiredis/hiredis_ssl.h>
#endif

#include "rsyslog.h"
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include "cfsysline.h"
#include "unicode-helper.h"

MODULE_TYPE_OUTPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME("omhiredis")
/* internal structures
 */
DEF_OMOD_STATIC_DATA;

#define OMHIREDIS_MODE_TEMPLATE 0
#define OMHIREDIS_MODE_QUEUE 1
#define OMHIREDIS_MODE_PUBLISH 2
#define OMHIREDIS_MODE_SET 3
#define OMHIREDIS_MODE_STREAM 4

/* our instance data.
 * this will be accessable
 * via pData */
typedef struct _instanceData {
    uchar *server; /* redis server address */
    uchar *socketPath; /* redis server UDS address (This option only takes effect if the server is not set) */
    int port; /* redis port */
    uchar *serverpassword; /* redis password */
    uchar *tplName; /* template name */
    char *modeDescription; /* mode description */
    int mode; /* mode constant */
    uchar *key; /* key for QUEUE, PUBLISH and STREAM modes */
    uchar *streamKeyAck; /* key name for STREAM ACKs (when enabled) */
    uchar *streamGroupAck; /* group name for STREAM ACKs (when enabled) */
    uchar *streamIndexAck; /* index name for STREAM ACKs (when enabled) */
    int expiration; /* expiration value for SET/SETEX mode */
    sbool dynaKey; /* Should we treat the key as a template? */
    sbool streamDynaKeyAck; /* Should we treat the groupAck as a template? */
    sbool streamDynaGroupAck; /* Should we treat the groupAck as a template? */
    sbool streamDynaIndexAck; /* Should we treat the IndexAck as a template? */
    sbool useRPush; /* Should we use RPUSH instead of LPUSH? */
    uchar *streamOutField; /* Field to place message into (for stream insertions only) */
    uint streamCapacityLimit; /* zero means stream is not capped (default)
                setting a non-zero value ultimately activates the approximate MAXLEN option '~'
                (see Redis XADD docs)*/
    sbool streamAck; /* Should the module send an XACK for each inserted message?
                This feature requires that 3 infos are present in the '$.' object of the log:
                - $.redis!stream
                - $.redis!group
                - $.redis!index
                Those 3 infos can either be provided through usage of imhiredis
                or set manually with Rainerscript */
    sbool streamDel; /* Should the module send an XDEL for each inserted message?
                This feature requires that 2 infos are present in the '$.' object of the log:
                - $.redis!stream
                - $.redis!index
                Those 2 infos can either be provided through usage of imhiredis
                or set manually with Rainerscript */
#ifdef HIREDIS_SSL
    sbool use_tls; /* Should we use TLS to connect to redis ? */
    char *ca_cert_bundle; /* CA bundle file */
    char *ca_cert_dir; /* Path of trusted certificates */
    char *client_cert; /* Client certificate */
    char *client_key; /* Client private key */
    char *sni; /* TLS Server Name Indication */
#endif

} instanceData;

typedef struct wrkrInstanceData {
    instanceData *pData; /* instanc data */
    redisContext *conn; /* redis connection */
    int count; /* count of command sent for current batch */
#ifdef HIREDIS_SSL
    redisSSLContext *ssl_conn; /* redis ssl connection */
    redisSSLContextError ssl_error; /* ssl error handler */
#endif
} wrkrInstanceData_t;

static struct cnfparamdescr actpdescr[] = {
    {"server", eCmdHdlrGetWord, 0},
    {"serverport", eCmdHdlrInt, 0},
    {"socketpath", eCmdHdlrGetWord, 0},
    {"serverpassword", eCmdHdlrGetWord, 0},
    {"template", eCmdHdlrGetWord, 0},
    {"mode", eCmdHdlrGetWord, 0},
    {"key", eCmdHdlrGetWord, 0},
    {"expiration", eCmdHdlrInt, 0},
    {"dynakey", eCmdHdlrBinary, 0},
    {"userpush", eCmdHdlrBinary, 0},
    {"stream.outField", eCmdHdlrGetWord, 0},
    {"stream.capacityLimit", eCmdHdlrNonNegInt, 0},
    {"stream.ack", eCmdHdlrBinary, 0},
    {"stream.del", eCmdHdlrBinary, 0},
    {"stream.keyAck", eCmdHdlrGetWord, 0},
    {"stream.groupAck", eCmdHdlrGetWord, 0},
    {"stream.indexAck", eCmdHdlrGetWord, 0},
    {"stream.dynaKeyAck", eCmdHdlrBinary, 0},
    {"stream.dynaGroupAck", eCmdHdlrBinary, 0},
    {"stream.dynaIndexAck", eCmdHdlrBinary, 0},
#ifdef HIREDIS_SSL
    {"use_tls", eCmdHdlrBinary, 0},
    {"ca_cert_bundle", eCmdHdlrGetWord, 0},
    {"ca_cert_dir", eCmdHdlrGetWord, 0},
    {"client_cert", eCmdHdlrGetWord, 0},
    {"client_key", eCmdHdlrGetWord, 0},
    {"sni", eCmdHdlrGetWord, 0},
#endif
};

static struct cnfparamblk actpblk = {CNFPARAMBLK_VERSION, sizeof(actpdescr) / sizeof(struct cnfparamdescr), actpdescr};

BEGINcreateInstance
    CODESTARTcreateInstance;
ENDcreateInstance

BEGINcreateWrkrInstance
    CODESTARTcreateWrkrInstance;
    pWrkrData->conn = NULL; /* Connect later */
#ifdef HIREDIS_SSL
    pWrkrData->ssl_conn = NULL; /* Connect later */
    pWrkrData->ssl_error = REDIS_SSL_CTX_NONE;
#endif
ENDcreateWrkrInstance

BEGINisCompatibleWithFeature
    CODESTARTisCompatibleWithFeature;
    if (eFeat == sFEATURERepeatedMsgReduction) iRet = RS_RET_OK;
ENDisCompatibleWithFeature

/* called when closing */
static void closeHiredis(wrkrInstanceData_t *pWrkrData) {
    if (pWrkrData->conn != NULL) {
        redisFree(pWrkrData->conn);
        pWrkrData->conn = NULL;
    }
#ifdef HIREDIS_SSL
    if (pWrkrData->ssl_conn != NULL) {
        redisFreeSSLContext(pWrkrData->ssl_conn);
        pWrkrData->ssl_conn = NULL;
    }
#endif
}

/* Free our instance data. */
BEGINfreeInstance
    CODESTARTfreeInstance;
    if (pData->server != NULL) {
        free(pData->server);
    }
    if (pData->socketPath != NULL) {
        free(pData->socketPath);
    }
    free(pData->key);
    free(pData->modeDescription);
    free(pData->serverpassword);
    free(pData->tplName);
    free(pData->streamKeyAck);
    free(pData->streamGroupAck);
    free(pData->streamIndexAck);
    free(pData->streamOutField);
#ifdef HIREDIS_SSL
    free(pData->ca_cert_bundle);
    free(pData->ca_cert_dir);
    free(pData->client_cert);
    free(pData->client_key);
    free(pData->sni);
#endif
ENDfreeInstance

BEGINfreeWrkrInstance
    CODESTARTfreeWrkrInstance;
    closeHiredis(pWrkrData);
ENDfreeWrkrInstance

BEGINdbgPrintInstInfo
    CODESTARTdbgPrintInstInfo;
    /* nothing special here */
ENDdbgPrintInstInfo

/* establish our connection to redis */
static rsRetVal initHiredis(wrkrInstanceData_t *pWrkrData, int bSilent) {
    char *server;
    sbool udsAddr = 0;
    redisReply *reply = NULL;
    DEFiRet;

    if (pWrkrData->pData->server != NULL) {
        server = (char *)pWrkrData->pData->server;
    } else if (pWrkrData->pData->socketPath != NULL) {
        udsAddr = 1;
        server = (char *)pWrkrData->pData->socketPath;
    } else {
        server = (char *)"127.0.0.1";
    }

    struct timeval timeout = {1, 500000}; /* 1.5 seconds */
    if (udsAddr) {
        DBGPRINTF("omhiredis: trying connect to UDS socket '%s'\n", server);
        pWrkrData->conn = redisConnectUnixWithTimeout(server, timeout);
    } else {
        DBGPRINTF("omhiredis: trying connect to '%s' at port %d\n", server, pWrkrData->pData->port);
        pWrkrData->conn = redisConnectWithTimeout(server, pWrkrData->pData->port, timeout);
    }

    if (pWrkrData->conn == NULL || pWrkrData->conn->err) {
        if (!bSilent) {
            const char *err_str = pWrkrData->conn == NULL ? "could not allocate context!" : pWrkrData->conn->errstr;
            if (udsAddr) {
                LogError(0, RS_RET_REDIS_ERROR, "omhiredis: can not connect to redis UDS socket '%s' -> %s", server,
                         err_str);
            } else {
                LogError(0, RS_RET_REDIS_ERROR,
                         "omhiredis: can not connect to redis server '%s', "
                         "port %d -> %s",
                         server, pWrkrData->pData->port, err_str);
            }
        }
        ABORT_FINALIZE(RS_RET_SUSPENDED);
    }

#ifdef HIREDIS_SSL
    if (pWrkrData->pData->use_tls) {
        pWrkrData->ssl_conn = redisCreateSSLContext(pWrkrData->pData->ca_cert_bundle, pWrkrData->pData->ca_cert_dir,
                                                    pWrkrData->pData->client_cert, pWrkrData->pData->client_key,
                                                    pWrkrData->pData->sni, &pWrkrData->ssl_error);
        if (!pWrkrData->ssl_conn || pWrkrData->ssl_error != REDIS_SSL_CTX_NONE) {
            LogError(0, NO_ERRCODE, "omhiredis: SSL Context error: %s", redisSSLContextGetError(pWrkrData->ssl_error));
            if (!bSilent) LogError(0, RS_RET_SUSPENDED, "[TLS] can not initialize redis handle");
            ABORT_FINALIZE(RS_RET_SUSPENDED);
        }
        if (redisInitiateSSLWithContext(pWrkrData->conn, pWrkrData->ssl_conn) != REDIS_OK) {
            LogError(0, NO_ERRCODE, "omhiredis: %s", pWrkrData->conn->errstr);
            if (!bSilent) LogError(0, RS_RET_SUSPENDED, "[TLS] can not initialize redis handle");
            ABORT_FINALIZE(RS_RET_SUSPENDED);
        }
    }
#endif

    if (pWrkrData->pData->serverpassword != NULL) {
        reply = redisCommand(pWrkrData->conn, "AUTH %s", (char *)pWrkrData->pData->serverpassword);
        if (reply == NULL) {
            DBGPRINTF("omhiredis: could not get reply from AUTH command\n");
            ABORT_FINALIZE(RS_RET_SUSPENDED);
        } else if (reply->type == REDIS_REPLY_ERROR) {
            LogError(0, NO_ERRCODE, "omhiredis: error while authenticating: %s", reply->str);
            ABORT_FINALIZE(RS_RET_ERR);
        }
    }

finalize_it:
    if (iRet != RS_RET_OK && pWrkrData->conn != NULL) {
        redisFree(pWrkrData->conn);
        pWrkrData->conn = NULL;
    }
#ifdef HIREDIS_SSL
    if (iRet != RS_RET_OK && pWrkrData->ssl_conn != NULL) {
        redisFreeSSLContext(pWrkrData->ssl_conn);
        pWrkrData->ssl_conn = NULL;
    }
#endif
    if (reply != NULL) freeReplyObject(reply);
    RETiRet;
}

static rsRetVal isMaster(wrkrInstanceData_t *pWrkrData) {
    DEFiRet;
    redisReply *reply = NULL;

    assert(pWrkrData->conn != NULL);

    reply = redisCommand(pWrkrData->conn, "ROLE");
    if (reply == NULL) {
        DBGPRINTF("omhiredis: could not get reply from ROLE command\n");
        ABORT_FINALIZE(RS_RET_SUSPENDED);
    } else if (reply->type == REDIS_REPLY_ERROR) {
        LogMsg(0, RS_RET_REDIS_ERROR, LOG_WARNING,
               "omhiredis: got an error while querying role -> "
               "%s\n",
               reply->str);
        ABORT_FINALIZE(RS_RET_SUSPENDED);
    } else if (reply->type != REDIS_REPLY_ARRAY || reply->element[0]->type != REDIS_REPLY_STRING) {
        LogMsg(0, RS_RET_REDIS_ERROR, LOG_ERR, "omhiredis: did not get a proper reply from ROLE command");
        ABORT_FINALIZE(RS_RET_SUSPENDED);
    } else {
        if (strncmp(reply->element[0]->str, "master", 6)) {
            LogMsg(0, RS_RET_OK, LOG_WARNING, "omhiredis: current connected node is not a master");
            ABORT_FINALIZE(RS_RET_SUSPENDED);
        }
    }

finalize_it:
    free(reply);
    RETiRet;
}

static rsRetVal writeHiredis(uchar *key, uchar *message, wrkrInstanceData_t *pWrkrData) {
    DEFiRet;
    int rc, expire;
    size_t msgLen;
    char *formattedMsg = NULL;

    /* if we do not have a redis connection, call
     * initHiredis and try to establish one */
    if (pWrkrData->conn == NULL) CHKiRet(initHiredis(pWrkrData, 0));

    /* try to append the command to the pipeline.
     * REDIS_ERR reply indicates something bad
     * happened, in which case abort. otherwise
     * increase our current pipeline count
     * by 1 and continue. */
    switch (pWrkrData->pData->mode) {
        case OMHIREDIS_MODE_TEMPLATE:
            rc = redisAppendCommand(pWrkrData->conn, (char *)message);
            break;
        case OMHIREDIS_MODE_QUEUE:
            rc = redisAppendCommand(pWrkrData->conn, pWrkrData->pData->useRPush ? "RPUSH %s %s" : "LPUSH %s %s", key,
                                    (char *)message);
            break;
        case OMHIREDIS_MODE_PUBLISH:
            rc = redisAppendCommand(pWrkrData->conn, "PUBLISH %s %s", key, (char *)message);
            break;
        case OMHIREDIS_MODE_SET:
            expire = pWrkrData->pData->expiration;

            if (expire > 0)
                msgLen = redisFormatCommand(&formattedMsg, "SETEX %s %d %s", key, expire, message);
            else
                msgLen = redisFormatCommand(&formattedMsg, "SET %s %s", key, message);
            if (msgLen)
                rc = redisAppendFormattedCommand(pWrkrData->conn, formattedMsg, msgLen);
            else {
                dbgprintf("omhiredis: could not append SET command\n");
                rc = REDIS_ERR;
            }
            break;
        case OMHIREDIS_MODE_STREAM:
            if (pWrkrData->pData->streamCapacityLimit != 0) {
                rc = redisAppendCommand(pWrkrData->conn, "XADD %s MAXLEN ~ %d * %s %s", key,
                                        pWrkrData->pData->streamCapacityLimit, pWrkrData->pData->streamOutField,
                                        message);
            } else {
                rc = redisAppendCommand(pWrkrData->conn, "XADD %s * %s %s", key, pWrkrData->pData->streamOutField,
                                        message);
            }
            break;
        default:
            dbgprintf("omhiredis: mode %d is invalid something is really wrong\n", pWrkrData->pData->mode);
            ABORT_FINALIZE(RS_RET_ERR);
    }

    if (rc == REDIS_ERR) {
        LogError(0, NO_ERRCODE, "omhiredis: %s", pWrkrData->conn->errstr);
        dbgprintf("omhiredis: %s\n", pWrkrData->conn->errstr);
        ABORT_FINALIZE(RS_RET_ERR);
    } else {
        pWrkrData->count++;
    }

finalize_it:
    free(formattedMsg);
    RETiRet;
}

static rsRetVal ackHiredisStreamIndex(wrkrInstanceData_t *pWrkrData, uchar *key, uchar *group, uchar *index) {
    DEFiRet;

    if (REDIS_ERR == redisAppendCommand(pWrkrData->conn, "XACK %s %s %s", key, group, index)) {
        LogError(0, NO_ERRCODE, "omhiredis: %s", pWrkrData->conn->errstr);
        DBGPRINTF("omhiredis: %s\n", pWrkrData->conn->errstr);
        ABORT_FINALIZE(RS_RET_ERR);
    } else {
        pWrkrData->count++;
    }

finalize_it:
    RETiRet;
}

static rsRetVal delHiredisStreamIndex(wrkrInstanceData_t *pWrkrData, uchar *key, uchar *index) {
    DEFiRet;

    if (REDIS_ERR == redisAppendCommand(pWrkrData->conn, "XDEL %s %s", key, index)) {
        LogError(0, NO_ERRCODE, "omhiredis: %s", pWrkrData->conn->errstr);
        DBGPRINTF("omhiredis: %s\n", pWrkrData->conn->errstr);
        ABORT_FINALIZE(RS_RET_ERR);
    } else {
        pWrkrData->count++;
    }

finalize_it:
    RETiRet;
}

/* called when resuming from suspended state.
 * try to restablish our connection to redis */
BEGINtryResume
    CODESTARTtryResume;
    closeHiredis(pWrkrData);
    CHKiRet(initHiredis(pWrkrData, 0));
    // Must get a master node for all modes, except 'publish'
    if (pWrkrData->pData->mode != OMHIREDIS_MODE_PUBLISH) {
        CHKiRet(isMaster(pWrkrData));
    }
finalize_it:
ENDtryResume

/* begin a transaction.
 * if I decide to use MULTI ... EXEC in the
 * future, this block should send the
 * MULTI command to redis. */
BEGINbeginTransaction
    CODESTARTbeginTransaction;
    dbgprintf("omhiredis: beginTransaction called\n");
    pWrkrData->count = 0;
ENDbeginTransaction

/* call writeHiredis for this log line,
 * which appends it as a command to the
 * current pipeline */
BEGINdoAction
    uchar *message, *key, *keyNameAck, *groupNameAck, *IndexNameAck;
    int inputIndex = 0;
    CODESTARTdoAction;
    // Don't change the order of conditions/assignations here without changing the end of the newActInst function!
    message = ppString[inputIndex++];
    key = pWrkrData->pData->dynaKey ? ppString[inputIndex++] : pWrkrData->pData->key;
    keyNameAck = pWrkrData->pData->streamDynaKeyAck ? ppString[inputIndex++] : pWrkrData->pData->streamKeyAck;
    groupNameAck = pWrkrData->pData->streamDynaGroupAck ? ppString[inputIndex++] : pWrkrData->pData->streamGroupAck;
    IndexNameAck = pWrkrData->pData->streamDynaIndexAck ? ppString[inputIndex++] : pWrkrData->pData->streamIndexAck;

    CHKiRet(writeHiredis(key, message, pWrkrData));

    if (pWrkrData->pData->streamAck) {
        CHKiRet(ackHiredisStreamIndex(pWrkrData, keyNameAck, groupNameAck, IndexNameAck));
    }
    if (pWrkrData->pData->streamDel) {
        CHKiRet(delHiredisStreamIndex(pWrkrData, keyNameAck, IndexNameAck));
    }

    iRet = RS_RET_DEFER_COMMIT;
finalize_it:
ENDdoAction

/* called when we have reached the end of a
 * batch (queue.dequeuebatchsize).  this
 * iterates over the replies, putting them
 * into the pData->replies buffer. we currently
 * don't really bother to check for errors
 * which should be fixed */
BEGINendTransaction
    CODESTARTendTransaction;
    dbgprintf("omhiredis: endTransaction called\n");
    redisReply *reply;
    int i;
    for (i = 0; i < pWrkrData->count; i++) {
        if (REDIS_OK != redisGetReply(pWrkrData->conn, (void *)&reply) || pWrkrData->conn->err) {
            dbgprintf("omhiredis: %s\n", pWrkrData->conn->errstr);
            LogError(0, RS_RET_REDIS_ERROR, "Error while processing replies: %s", pWrkrData->conn->errstr);
            closeHiredis(pWrkrData);
            ABORT_FINALIZE(RS_RET_SUSPENDED);
        } else {
            if (reply->type == REDIS_REPLY_ERROR) {
                LogError(0, RS_RET_REDIS_ERROR, "Received error from redis -> %s", reply->str);
                closeHiredis(pWrkrData);
                freeReplyObject(reply);
                ABORT_FINALIZE(RS_RET_SUSPENDED);
            }
            freeReplyObject(reply);
        }
    }

finalize_it:
ENDendTransaction

/* set defaults. note server is set to NULL
 * and is set to a default in initHiredis if
 * it is still null when it's called - I should
 * probable just set the default here instead */
static void setInstParamDefaults(instanceData *pData) {
    pData->server = NULL;
    pData->socketPath = NULL;
    pData->port = 6379;
    pData->serverpassword = NULL;
    pData->tplName = NULL;
    pData->mode = OMHIREDIS_MODE_TEMPLATE;
    pData->expiration = 0;
    pData->modeDescription = NULL;
    pData->key = NULL;
    pData->dynaKey = 0;
    pData->useRPush = 0;
    pData->streamOutField = NULL;
    pData->streamKeyAck = NULL;
    pData->streamDynaKeyAck = 0;
    pData->streamGroupAck = NULL;
    pData->streamDynaGroupAck = 0;
    pData->streamIndexAck = NULL;
    pData->streamDynaIndexAck = 0;
    pData->streamCapacityLimit = 0;
    pData->streamAck = 0;
    pData->streamDel = 0;
#ifdef HIREDIS_SSL
    pData->use_tls = 0;
    pData->ca_cert_bundle = NULL;
    pData->ca_cert_dir = NULL;
    pData->client_cert = NULL;
    pData->client_key = NULL;
    pData->sni = NULL;
#endif
}

/* here is where the work to set up a new instance
 * is done.  this reads the config options from
 * the rsyslog conf and takes appropriate setup
 * actions. */
BEGINnewActInst
    struct cnfparamvals *pvals;
    int i;
    int iNumTpls;
    uchar *strDup = NULL;
    CODESTARTnewActInst;
    if ((pvals = nvlstGetParams(lst, &actpblk, NULL)) == NULL) ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);

    CHKiRet(createInstance(&pData));
    setInstParamDefaults(pData);

    for (i = 0; i < actpblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;

        if (!strcmp(actpblk.descr[i].name, "server")) {
            pData->server = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "serverport")) {
            pData->port = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "socketpath")) {
            pData->socketPath = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "serverpassword")) {
            pData->serverpassword = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "template")) {
            pData->tplName = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "dynakey")) {
            pData->dynaKey = pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "userpush")) {
            pData->useRPush = pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "stream.outField")) {
            pData->streamOutField = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "stream.keyAck")) {
            pData->streamKeyAck = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "stream.dynaKeyAck")) {
            pData->streamDynaKeyAck = pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "stream.groupAck")) {
            pData->streamGroupAck = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "stream.dynaGroupAck")) {
            pData->streamDynaGroupAck = pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "stream.indexAck")) {
            pData->streamIndexAck = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "stream.dynaIndexAck")) {
            pData->streamDynaIndexAck = pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "stream.capacityLimit")) {
            pData->streamCapacityLimit = pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "stream.ack")) {
            pData->streamAck = pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "stream.del")) {
            pData->streamDel = pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "mode")) {
            pData->modeDescription = es_str2cstr(pvals[i].val.d.estr, NULL);
            if (!strcmp(pData->modeDescription, "template")) {
                pData->mode = OMHIREDIS_MODE_TEMPLATE;
            } else if (!strcmp(pData->modeDescription, "queue")) {
                pData->mode = OMHIREDIS_MODE_QUEUE;
            } else if (!strcmp(pData->modeDescription, "publish")) {
                pData->mode = OMHIREDIS_MODE_PUBLISH;
            } else if (!strcmp(pData->modeDescription, "set")) {
                pData->mode = OMHIREDIS_MODE_SET;
            } else if (!strcmp(pData->modeDescription, "stream")) {
                pData->mode = OMHIREDIS_MODE_STREAM;
            } else {
                dbgprintf("omhiredis: unsupported mode %s\n", actpblk.descr[i].name);
                ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
            }
        } else if (!strcmp(actpblk.descr[i].name, "key")) {
            pData->key = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "expiration")) {
            pData->expiration = pvals[i].val.d.n;
            dbgprintf("omhiredis: expiration set to %d\n", pData->expiration);
#ifdef HIREDIS_SSL
        } else if (!strcmp(actpblk.descr[i].name, "use_tls")) {
            pData->use_tls = pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "ca_cert_bundle")) {
            pData->ca_cert_bundle = (char *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "ca_cert_dir")) {
            pData->ca_cert_dir = (char *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "client_cert")) {
            pData->client_cert = (char *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "client_key")) {
            pData->client_key = (char *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "sni")) {
            pData->sni = (char *)es_str2cstr(pvals[i].val.d.estr, NULL);
#endif
        } else {
            dbgprintf(
                "omhiredis: program error, non-handled "
                "param '%s'\n",
                actpblk.descr[i].name);
        }
    }

    dbgprintf("omhiredis: checking config sanity\n");

    if (!pData->modeDescription) {
        dbgprintf("omhiredis: no mode specified, setting it to 'template'\n");
        pData->mode = OMHIREDIS_MODE_TEMPLATE;
    }

    if (pData->mode == OMHIREDIS_MODE_STREAM && !pData->streamOutField) {
        LogError(0, RS_RET_CONF_PARSE_WARNING,
                 "omhiredis: no stream.outField set, "
                 "using 'msg' as default");
        pData->streamOutField = ustrdup("msg");
    }

    if (pData->server != NULL && pData->socketPath != NULL) {
        LogError(0, RS_RET_CONF_PARSE_WARNING,
                 "omhiredis: both 'server' and 'socketpath' are set; 'socketpath' will be ignored");
        free(pData->socketPath);
        pData->socketPath = NULL;
    }

    if (pData->tplName == NULL) {
        if (pData->mode == OMHIREDIS_MODE_TEMPLATE) {
            LogError(0, RS_RET_CONF_PARSE_ERROR, "omhiredis: selected mode requires a template");
            ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
        } else {
            CHKmalloc(pData->tplName = ustrdup("RSYSLOG_ForwardFormat"));
            LogError(0, RS_RET_CONF_PARSE_WARNING,
                     "omhiredis: no template set, "
                     "using RSYSLOG_ForwardFormat as default");
        }
    }

    if (pData->mode != OMHIREDIS_MODE_TEMPLATE && pData->key == NULL) {
        LogError(0, RS_RET_CONF_PARSE_ERROR, "omhiredis: mode %s requires a key", pData->modeDescription);
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }

    if (pData->expiration && pData->mode != OMHIREDIS_MODE_SET) {
        LogError(0, RS_RET_CONF_PARSE_WARNING,
                 "omhiredis: expiration set but mode is not "
                 "'set', expiration will be ignored");
    }

    if (pData->mode != OMHIREDIS_MODE_STREAM) {
        if (pData->streamOutField) {
            LogError(0, RS_RET_CONF_PARSE_WARNING,
                     "omhiredis: stream.outField set "
                     "but mode is not 'stream', field will be ignored");
        }
        if (pData->streamAck) {
            LogError(0, RS_RET_CONF_PARSE_WARNING,
                     "omhiredis: stream.ack set "
                     "but mode is not 'stream', XACK will be ignored");
        }
        if (pData->streamDel) {
            LogError(0, RS_RET_CONF_PARSE_WARNING,
                     "omhiredis: stream.del set "
                     "but mode is not 'stream', XDEL will be ignored");
        }
        if (pData->streamCapacityLimit) {
            LogError(0, RS_RET_CONF_PARSE_WARNING,
                     "omhiredis: stream.capacityLimit set "
                     "but mode is not 'stream', stream trimming will be ignored");
        }
        if (pData->streamKeyAck) {
            LogError(0, RS_RET_CONF_PARSE_WARNING,
                     "omhiredis: stream.keyAck set "
                     "but mode is not 'stream', parameter will be ignored");
        }
        if (pData->streamDynaKeyAck) {
            LogError(0, RS_RET_CONF_PARSE_WARNING,
                     "omhiredis: stream.dynaKeyAck set "
                     "but mode is not 'stream', parameter will be ignored");
        }
        if (pData->streamGroupAck) {
            LogError(0, RS_RET_CONF_PARSE_WARNING,
                     "omhiredis: stream.groupAck set "
                     "but mode is not 'stream', parameter will be ignored");
        }
        if (pData->streamDynaGroupAck) {
            LogError(0, RS_RET_CONF_PARSE_WARNING,
                     "omhiredis: stream.dynaGroupAck set "
                     "but mode is not 'stream', parameter will be ignored");
        }
        if (pData->streamIndexAck) {
            LogError(0, RS_RET_CONF_PARSE_WARNING,
                     "omhiredis: stream.indexAck set "
                     "but mode is not 'stream', parameter will be ignored");
        }
        if (pData->streamDynaIndexAck) {
            LogError(0, RS_RET_CONF_PARSE_WARNING,
                     "omhiredis: stream.dynaIndexAck set "
                     "but mode is not 'stream', parameter will be ignored");
        }
    } else {
        if (pData->streamAck) {
            if (!pData->streamKeyAck || !pData->streamGroupAck || !pData->streamIndexAck) {
                LogError(0, RS_RET_CONF_PARSE_ERROR,
                         "omhiredis: 'stream.ack' is set but one of "
                         "'stream.keyAck', 'stream.groupAck' or 'stream.indexAck' is missing");
                ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
            }
        }
        if (pData->streamDel) {
            if (!pData->streamKeyAck || !pData->streamIndexAck) {
                LogError(0, RS_RET_CONF_PARSE_ERROR,
                         "omhiredis: 'stream.del' is set but one of "
                         "'stream.keyAck' or 'stream.indexAck' is missing");
                ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
            }
        }
    }

    if (pData->streamDynaKeyAck && pData->streamKeyAck == NULL) {
        LogError(0, RS_RET_CONF_PARSE_WARNING,
                 "omhiredis: 'stream.dynaKeyAck' set "
                 "but 'stream.keyAck' is empty, disabling");
        pData->streamDynaKeyAck = 0;
    }
    if (pData->streamDynaGroupAck && pData->streamGroupAck == NULL) {
        LogError(0, RS_RET_CONF_PARSE_WARNING,
                 "omhiredis: 'stream.dynaGroupAck' set "
                 "but 'stream.groupAck' is empty, disabling");
        pData->streamDynaGroupAck = 0;
    }
    if (pData->streamDynaIndexAck && pData->streamIndexAck == NULL) {
        LogError(0, RS_RET_CONF_PARSE_WARNING,
                 "omhiredis: 'stream.dynaGroupAck' set "
                 "but 'stream.indexAck' is empty, disabling");
        pData->streamDynaIndexAck = 0;
    }

    iNumTpls = 1;

    if (pData->dynaKey) {
        assert(pData->key != NULL);
        iNumTpls += 1;
    }
    if (pData->streamDynaKeyAck) {
        assert(pData->streamKeyAck != NULL);
        iNumTpls += 1;
    }
    if (pData->streamDynaGroupAck) {
        assert(pData->streamGroupAck != NULL);
        iNumTpls += 1;
    }
    if (pData->streamDynaIndexAck) {
        assert(pData->streamIndexAck != NULL);
        iNumTpls += 1;
    }
    CODE_STD_STRING_REQUESTnewActInst(iNumTpls);

    /* Insert templates in opposite order (keep in sync with doAction), order will be
     * - tplName
     * - key
     * - streamKeyAck
     * - streamGroupAck
     * - streamIndexAck
     */
    if (pData->streamDynaIndexAck) {
        CHKmalloc(strDup = ustrdup(pData->streamIndexAck));
        CHKiRet(OMSRsetEntry(*ppOMSR, --iNumTpls, strDup, OMSR_NO_RQD_TPL_OPTS));
        strDup = NULL; /* handed over */
    }

    if (pData->streamDynaGroupAck) {
        CHKmalloc(strDup = ustrdup(pData->streamGroupAck));
        CHKiRet(OMSRsetEntry(*ppOMSR, --iNumTpls, strDup, OMSR_NO_RQD_TPL_OPTS));
        strDup = NULL; /* handed over */
    }

    if (pData->streamDynaKeyAck) {
        CHKmalloc(strDup = ustrdup(pData->streamKeyAck));
        CHKiRet(OMSRsetEntry(*ppOMSR, --iNumTpls, strDup, OMSR_NO_RQD_TPL_OPTS));
        strDup = NULL; /* handed over */
    }

    if (pData->dynaKey) {
        CHKmalloc(strDup = ustrdup(pData->key));
        CHKiRet(OMSRsetEntry(*ppOMSR, --iNumTpls, strDup, OMSR_NO_RQD_TPL_OPTS));
        strDup = NULL; /* handed over */
    }

    CHKiRet(OMSRsetEntry(*ppOMSR, --iNumTpls, ustrdup(pData->tplName), OMSR_NO_RQD_TPL_OPTS));

    CODE_STD_FINALIZERnewActInst;
    cnfparamvalsDestruct(pvals, &actpblk);
    free(strDup);
ENDnewActInst


NO_LEGACY_CONF_parseSelectorAct


    BEGINmodExit CODESTARTmodExit;
ENDmodExit

/* register our plugin entry points
 * with the rsyslog core engine */
BEGINqueryEtryPt
    CODESTARTqueryEtryPt;
    CODEqueryEtryPt_STD_OMOD_QUERIES;
    CODEqueryEtryPt_STD_OMOD8_QUERIES;
    CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES;
    CODEqueryEtryPt_TXIF_OMOD_QUERIES /*  supports transaction interface */
ENDqueryEtryPt

/* note we do not support rsyslog v5 syntax */
BEGINmodInit()
    CODESTARTmodInit;
    *ipIFVersProvided = CURR_MOD_IF_VERSION; /* only supports rsyslog 6 configs */
    CODEmodInit_QueryRegCFSLineHdlr INITChkCoreFeature(bCoreSupportsBatching, CORE_FEATURE_BATCHING);
    if (!bCoreSupportsBatching) {
        LogError(0, NO_ERRCODE, "omhiredis: rsyslog core does not support batching - abort");
        ABORT_FINALIZE(RS_RET_ERR);
    }
    DBGPRINTF("omhiredis: module compiled with rsyslog version %s.\n", VERSION);

#ifdef HIREDIS_SSL
    // initialize OpenSSL
    redisInitOpenSSL();
#endif
ENDmodInit
