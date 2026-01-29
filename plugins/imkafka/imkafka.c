/*
 * imkafka.c
 *
 * This input plugin is a consumer for Apache Kafka.
 *
 * File begun on 2017-04-25 by alorbach
 *
 * Copyright 2008-2017 Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 * -or-
 * see COPYING.ASL20 in the source distribution
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef _GNU_SOURCE
    #define _GNU_SOURCE /* for timegm */
#endif
#define _XOPEN_SOURCE 700 /* for strptime */
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
#include <time.h>
#include <sys/time.h>
#include <librdkafka/rdkafka.h>
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
#include "datetime.h"

/* If your build already injects the header, you may omit this include.
   Otherwise, prefer the canonical libfastjson path: */
// #include <json.h>
#include <libfastjson/json.h>
#include <libfastjson/json_tokener.h>

MODULE_TYPE_INPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME("imkafka")

/* =============================================================================
 * Concurrency & Locking
 * =============================================================================
 * - Each instance processes messages sequentially in its worker thread
 *   (imkafkawrkr). Messages are consumed via librdkafka's consumer API
 *   which handles thread-safety internally.
 * - JSON splitting (splitJsonRecords) operates on a per-message basis with
 *   no shared mutable state. Each split record is submitted independently
 *   to the rsyslog core, which handles its own queuing and threading.
 * - No additional locking required beyond existing instance-level processing
 *   and librdkafka's internal thread-safety guarantees.
 * ============================================================================= */

/* static data */
DEF_IMOD_STATIC_DATA;
DEFobjCurrIf(prop) DEFobjCurrIf(ruleset) DEFobjCurrIf(glbl) DEFobjCurrIf(statsobj) DEFobjCurrIf(datetime)

    /* =============================================================================
     * Stats (module-global + per-instance) — parity with omkafka, adapted for consumer
     * ============================================================================= */
    static statsobj_t *kafkaStats = NULL; /* module-global stats object */
/* Global counters (consumer-flavored) */
STATSCOUNTER_DEF(ctrReceived, mutCtrReceived); /* polled messages */
STATSCOUNTER_DEF(ctrSubmitted, mutCtrSubmitted); /* submitted to rsyslog core */
STATSCOUNTER_DEF(ctrKafkaFail, mutCtrKafkaFail); /* errors on poll/submit */
STATSCOUNTER_DEF(ctrEOF, mutCtrEOF); /* RD_KAFKA_RESP_ERR__PARTITION_EOF */
STATSCOUNTER_DEF(ctrPollEmpty, mutCtrPollEmpty); /* polls returning NULL */
STATSCOUNTER_DEF(ctrMaxLag, mutCtrMaxLag); /* maximum observed consumer lag */

/* Global categorized errors (mirrors omkafka) */
STATSCOUNTER_DEF(ctrKafkaRespTimedOut, mutCtrKafkaRespTimedOut);
STATSCOUNTER_DEF(ctrKafkaRespTransport, mutCtrKafkaRespTransport);
STATSCOUNTER_DEF(ctrKafkaRespBrokerDown, mutCtrKafkaRespBrokerDown);
STATSCOUNTER_DEF(ctrKafkaRespAuth, mutCtrKafkaRespAuth);
STATSCOUNTER_DEF(ctrKafkaRespSSL, mutCtrKafkaRespSSL);
STATSCOUNTER_DEF(ctrKafkaRespOther, mutCtrKafkaRespOther);

/* librdkafka window metrics (like omkafka) exposed as counters */
static uint64 rtt_avg_usec;
static uint64 throttle_avg_msec;
static uint64 int_latency_avg_usec;

/* convenience macros for per-instance stats */
#define INST_STATSCOUNTER_INC(inst, ctr, mut) \
    do {                                      \
        if ((inst)->stats) {                  \
            STATSCOUNTER_INC((ctr), (mut));   \
        }                                     \
    } while (0)
#define INST_STATSCOUNTER_SETMAX(inst, ctr, val)               \
    do {                                                       \
        if ((inst)->stats) {                                   \
            STATSCOUNTER_SETMAX_NOMUT((ctr), (uint64_t)(val)); \
        }                                                      \
    } while (0)

/* forward references */
static void *imkafkawrkr(void *myself);
struct kafka_params {
    const char *name;
    const char *val;
};

/* Module static data */
static struct configSettings_s {
    uchar *topic;
    uchar *consumergroup;
    char *brokers;
    uchar *pszBindRuleset;
    int nConfParams;
    struct kafka_params *confParams;
} cs;

typedef struct instanceConf_s {
    uchar *topic;
    uchar *consumergroup;
    char *brokers;
    int64_t offset;
    ruleset_t *pBindRuleset; /* ruleset to bind listener to (use system default if unspecified) */
    uchar *pszBindRuleset; /* default name of Ruleset to bind to */
    int bReportErrs;
    int nConfParams;
    struct kafka_params *confParams;
    int bIsConnected;
    rd_kafka_conf_t *conf;
    rd_kafka_t *rk;
    rd_kafka_topic_conf_t *topic_conf;
    int partition;
    int bIsSubscribed;
    int nMsgParsingFlags;
    int bSplitJsonRecords; /* if enabled, split {"records":[...]} into individual messages */
    struct instanceConf_s *next;
    /* per-instance stats object + counters */
    statsobj_t *stats;
    STATSCOUNTER_DEF(ctrReceived, mutCtrReceived);
    STATSCOUNTER_DEF(ctrSubmitted, mutCtrSubmitted);
    STATSCOUNTER_DEF(ctrKafkaFail, mutCtrKafkaFail);
    STATSCOUNTER_DEF(ctrEOF, mutCtrEOF);
    STATSCOUNTER_DEF(ctrPollEmpty, mutCtrPollEmpty);
    STATSCOUNTER_DEF(ctrMaxLag, mutCtrMaxLag);
} instanceConf_t;

typedef struct modConfData_s {
    rsconf_t *pConf; /* our overall config object */
    uchar *topic;
    uchar *consumergroup;
    char *brokers;
    instanceConf_t *root, *tail;
    ruleset_t *pBindRuleset; /* ruleset to bind listener to (use system default if unspecified) */
    uchar *pszBindRuleset; /* default name of Ruleset to bind to */
} modConfData_t;

/* global data */
pthread_attr_t wrkrThrdAttr; /* Attribute for worker threads ; read only after startup */
static int activeKafkaworkers = 0;
/* The following structure controls the worker threads. Global data is
 * needed for their access.
 */
struct kafkaWrkrInfo_s {
    pthread_t tid; /* the worker's thread ID */
    instanceConf_t *inst; /* Pointer to imkafka instance */
};
static struct kafkaWrkrInfo_s *kafkaWrkrInfo;

static modConfData_t *loadModConf = NULL; /* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL; /* modConf ptr to use for the current load process */
static prop_t *pInputName = NULL;
/* there is only one global inputName for all messages generated by this input */

/* module-global parameters */
static struct cnfparamdescr modpdescr[] = {
    {"ruleset", eCmdHdlrGetWord, 0},
};
static struct cnfparamblk modpblk = {CNFPARAMBLK_VERSION, sizeof(modpdescr) / sizeof(struct cnfparamdescr), modpdescr};

/* input instance parameters */
static struct cnfparamdescr inppdescr[] = {
    {"topic", eCmdHdlrString, CNFPARAM_REQUIRED}, {"broker", eCmdHdlrArray, 0},   {"confparam", eCmdHdlrArray, 0},
    {"consumergroup", eCmdHdlrString, 0},         {"ruleset", eCmdHdlrString, 0}, {"parsehostname", eCmdHdlrBinary, 0},
    {"split.json.records", eCmdHdlrBinary, 0},
};
static struct cnfparamblk inppblk = {CNFPARAMBLK_VERSION, sizeof(inppdescr) / sizeof(struct cnfparamdescr), inppdescr};

#include "im-helper.h" /* must be included AFTER the type definitions! */

/* -------------------------------------------------------------------------- */
/* Kafka logging callback                                                     */
/* -------------------------------------------------------------------------- */
static void kafkaLogger(const rd_kafka_t __attribute__((unused)) * rk, int level, const char *fac, const char *buf) {
    DBGPRINTF("imkafka: kafka log message [%d,%s]: %s\n", level, fac, buf);
}

/* -------------------------------------------------------------------------- */
/* JSON helpers for stats_cb parsing (same pattern as in omkafka)             */
/* librdkafka emits JSON stats when statistics.interval.ms > 0                */
/* See Confluent's librdkafka statistics docs for schema.                     */
/* -------------------------------------------------------------------------- */

/* get_object: exact key match (fix for previous prefix match bug) */
static struct fjson_object *get_object(struct fjson_object *fj_obj, const char *name) {
    struct fjson_object_iterator it = fjson_object_iter_begin(fj_obj);
    struct fjson_object_iterator itEnd = fjson_object_iter_end(fj_obj);
    while (!fjson_object_iter_equal(&it, &itEnd)) {
        const char *key = fjson_object_iter_peek_name(&it);
        struct fjson_object *val = fjson_object_iter_peek_value(&it);
        /* SUGGESTION #1: use strcmp for exact match */
        if (strcmp(key, name) == 0) {
            return val;
        }
        fjson_object_iter_next(&it);
    }
    return NULL;
}

/* Extract broker-window stats like rtt/throttle/int_latency averages */
static uint64 jsonExtractWindoStats(struct fjson_object *stats_object,
                                    const char *level1_obj_name,
                                    const char *level2_obj_name,
                                    unsigned long skip_threshold) {
    uint64 level2_val;
    uint64 agg_val = 0;
    uint64 ret_val = 0;
    int active_brokers = 0;

    struct fjson_object *brokers_obj = get_object(stats_object, "brokers");
    if (brokers_obj == NULL) {
        LogMsg(0, NO_ERRCODE, LOG_ERR, "imkafka: statscb: failed to find brokers object");
        return ret_val;
    }

    struct fjson_object_iterator it = fjson_object_iter_begin(brokers_obj);
    struct fjson_object_iterator itEnd = fjson_object_iter_end(brokers_obj);
    while (!fjson_object_iter_equal(&it, &itEnd)) {
        struct fjson_object *val = fjson_object_iter_peek_value(&it);

        struct fjson_object *level1_obj = get_object(val, level1_obj_name);
        /* SUGGESTION #2: do not abort on a single-broker miss; skip it */
        if (level1_obj == NULL) {
            fjson_object_iter_next(&it);
            continue;
        }

        struct fjson_object *level2_obj = get_object(level1_obj, level2_obj_name);
        /* SUGGESTION #2: do not abort on a single-broker miss; skip it */
        if (level2_obj == NULL) {
            fjson_object_iter_next(&it);
            continue;
        }

        level2_val = fjson_object_get_int64(level2_obj);
        if (level2_val > skip_threshold) {
            agg_val += level2_val;
            active_brokers++;
        }
        fjson_object_iter_next(&it);
    }

    if (active_brokers > 0) {
        ret_val = agg_val / active_brokers;
    }
    return ret_val;
}

/* librdkafka statistics callback: parse window metrics; emit as counters */
static int statsCallback(rd_kafka_t __attribute__((unused)) * rk,
                         char *json,
                         size_t __attribute__((unused)) json_len,
                         void __attribute__((unused)) * opaque) {
    struct fjson_object *stats_object = fjson_tokener_parse(json);
    if (stats_object == NULL) {
        LogMsg(0, NO_ERRCODE, LOG_ERR, "imkafka: statsCallback: fjson tokenizer failed");
        return 0;
    }

    /* Window stats extraction */
    rtt_avg_usec = jsonExtractWindoStats(stats_object, "rtt", "avg", 100);
    throttle_avg_msec = jsonExtractWindoStats(stats_object, "throttle", "avg", 0);
    int_latency_avg_usec = jsonExtractWindoStats(stats_object, "int_latency", "avg", 0);
    fjson_object_put(stats_object);

    /* Optional: visible info line for operator */
    LogMsg(0, NO_ERRCODE, LOG_INFO,
           "imkafka: statscb_window_stats: rtt_avg_usec=%lld throttle_avg_msec=%lld int_latency_avg_usec=%lld\n",
           rtt_avg_usec, throttle_avg_msec, int_latency_avg_usec);
    return 0;
}

/* Categorized error counts (same categories as omkafka) */
static void errorCallback(rd_kafka_t __attribute__((unused)) * rk,
                          int err,
                          const char *reason,
                          void __attribute__((unused)) * opaque) {
    if (err == RD_KAFKA_RESP_ERR__MSG_TIMED_OUT) {
        STATSCOUNTER_INC(ctrKafkaRespTimedOut, mutCtrKafkaRespTimedOut);
    } else if (err == RD_KAFKA_RESP_ERR__TRANSPORT) {
        STATSCOUNTER_INC(ctrKafkaRespTransport, mutCtrKafkaRespTransport);
    } else if (err == RD_KAFKA_RESP_ERR__ALL_BROKERS_DOWN) {
        STATSCOUNTER_INC(ctrKafkaRespBrokerDown, mutCtrKafkaRespBrokerDown);
    } else if (err == RD_KAFKA_RESP_ERR__AUTHENTICATION) {
        STATSCOUNTER_INC(ctrKafkaRespAuth, mutCtrKafkaRespAuth);
    } else if (err == RD_KAFKA_RESP_ERR__SSL) {
        STATSCOUNTER_INC(ctrKafkaRespSSL, mutCtrKafkaRespSSL);
    } else {
        STATSCOUNTER_INC(ctrKafkaRespOther, mutCtrKafkaRespOther);
    }
    LogError(0, RS_RET_KAFKA_ERROR, "imkafka: kafka error message: %d,'%s','%s'", err, rd_kafka_err2str(err), reason);
}

/* enqueue the kafka message. The provided string is
 * not freed - this must be done by the caller.
 */

/**
 * Helper: extract timestamp from JSON record's "time" field if present
 * Returns timestamp in UNIX epoch format, or 0 if not found/invalid
 */
static time_t extractJsonTimestamp(struct fjson_object *record) {
    struct fjson_object *time_obj;
    const char *time_str;
    struct tm tm;

    if (record == NULL) {
        return 0;
    }

    time_obj = get_object(record, "time");
    if (time_obj == NULL) {
        return 0;
    }

    time_str = fjson_object_get_string(time_obj);
    if (time_str == NULL) {
        return 0;
    }

    /* Parse ISO 8601 timestamp: 2025-02-20T03:19:34.655Z
     * Note: milliseconds (.655) are not parsed by strptime and will be lost */
    memset(&tm, 0, sizeof(tm));

    /* Parse timestamp; strptime will stop at non-matching characters like '.' or 'Z' */
    if (strptime(time_str, "%Y-%m-%dT%H:%M:%S", &tm) != NULL) {
        /* Convert to UTC time */
        return timegm(&tm);
    }

    return 0;
}

/**
 * Submit a single JSON record as a message to rsyslog
 * This is used both for normal messages and split records
 */
static rsRetVal submitJsonRecord(instanceConf_t *const __restrict__ inst,
                                 const char *payload,
                                 size_t len,
                                 const char *key,
                                 size_t key_len,
                                 time_t timestamp) {
    DEFiRet;
    smsg_t *pMsg;

    if (inst == NULL || payload == NULL) {
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    if (len == 0) {
        /* we do not process empty records */
        STATSCOUNTER_INC(ctrKafkaFail, mutCtrKafkaFail);
        INST_STATSCOUNTER_INC(inst, inst->ctrKafkaFail, inst->mutCtrKafkaFail);
        FINALIZE;
    }

    DBGPRINTF("imkafka: submitJsonRecord: Msg: %.*s\n", (int)len, payload);
    CHKiRet(msgConstruct(&pMsg));
    MsgSetInputName(pMsg, pInputName);
    MsgSetRawMsg(pMsg, (char *)payload, (int)len);
    MsgSetFlowControlType(pMsg, eFLOWCTL_LIGHT_DELAY);
    MsgSetRuleset(pMsg, inst->pBindRuleset);
    pMsg->msgFlags = inst->nMsgParsingFlags;

    /* Set timestamp if extracted from JSON */
    if (timestamp > 0) {
        struct timeval tv;
        tv.tv_sec = timestamp;
        tv.tv_usec = 0;
        datetime.timeval2syslogTime(&tv, &pMsg->tRcvdAt, TIME_IN_UTC);
        memcpy(&pMsg->tTIMESTAMP, &pMsg->tRcvdAt, sizeof(struct syslogTime));
    }

    /* Optional Fields */
    if (key_len > 0 && key != NULL) {
        DBGPRINTF("imkafka: submitJsonRecord: Key: %.*s\n", (int)key_len, key);
        MsgSetTAG(pMsg, (const uchar *)key, (int)key_len);
    }
    MsgSetMSGoffs(pMsg, 0); /* we do not have a header... */
    CHKiRet(submitMsg2(pMsg));

    /* submitted successfully */
    STATSCOUNTER_INC(ctrSubmitted, mutCtrSubmitted);
    INST_STATSCOUNTER_INC(inst, inst->ctrSubmitted, inst->mutCtrSubmitted);
finalize_it:
    RETiRet;
}

/**
 * Split JSON batch format {"records":[...]} into individual messages
 * Returns RS_RET_OK if splitting succeeded, error otherwise
 */
static rsRetVal splitJsonRecords(instanceConf_t *const __restrict__ inst,
                                 rd_kafka_message_t *const __restrict__ rkmessage) {
    DEFiRet;
    struct fjson_object *root = NULL;
    struct fjson_object *records_array = NULL;
    int record_count = 0;
    int submitted_count = 0;
    int i;

    if (inst == NULL || rkmessage == NULL) {
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    if (rkmessage->payload == NULL) {
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    /* Parse JSON using fjson_tokener_parse_ex to avoid malloc/free on hot path.
     * This works with non-null-terminated payloads by specifying length. */
    {
        struct fjson_tokener *tok = fjson_tokener_new();
        if (tok == NULL) {
            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        }
        root = fjson_tokener_parse_ex(tok, (const char *)rkmessage->payload, (int)rkmessage->len);
        enum fjson_tokener_error tok_err = fjson_tokener_get_error(tok);
        fjson_tokener_free(tok);
        if (tok_err != fjson_tokener_success || root == NULL) {
            if (root != NULL) {
                fjson_object_put(root);
                root = NULL;
            }
            LogMsg(0, NO_ERRCODE, LOG_WARNING, "imkafka: splitJsonRecords: failed to parse JSON, forwarding as-is");
            ABORT_FINALIZE(RS_RET_JSON_PARSE_ERR);
        }
    }

    /* Look for "records" array */
    records_array = get_object(root, "records");
    if (records_array == NULL) {
        /* No "records" field - treat as single message (not an error, just not applicable) */
        DBGPRINTF("imkafka: splitJsonRecords: no 'records' field found, treating as single message\n");
        ABORT_FINALIZE(RS_RET_ERR);
    }

    if (!fjson_object_is_type(records_array, fjson_type_array)) {
        LogMsg(0, NO_ERRCODE, LOG_WARNING, "imkafka: splitJsonRecords: 'records' is not an array, forwarding as-is");
        ABORT_FINALIZE(RS_RET_ERR);
    }

    record_count = fjson_object_array_length(records_array);
    DBGPRINTF("imkafka: splitJsonRecords: found %d records in batch\n", record_count);

    if (record_count == 0) {
        /* Empty array - forward original message (not an error, just no records to split) */
        DBGPRINTF("imkafka: splitJsonRecords: empty records array, forwarding as-is\n");
        ABORT_FINALIZE(RS_RET_ERR);
    }

    /* Process each record */
    for (i = 0; i < record_count; i++) {
        struct fjson_object *record = fjson_object_array_get_idx(records_array, i);
        time_t timestamp;
        const char *record_str;
        rsRetVal local_ret;

        if (record == NULL) {
            continue;
        }

        /* Extract timestamp if present */
        timestamp = extractJsonTimestamp(record);

        /* Convert record back to JSON string */
        record_str = fjson_object_to_json_string_ext(record, FJSON_TO_STRING_PLAIN);
        if (record_str == NULL) {
            LogMsg(0, NO_ERRCODE, LOG_WARNING, "imkafka: splitJsonRecords: failed to serialize record %d, skipping", i);
            continue;
        }

        /* Submit individual record */
        local_ret = submitJsonRecord(inst, record_str, strlen(record_str), (const char *)rkmessage->key,
                                     rkmessage->key_len, timestamp);
        if (local_ret == RS_RET_OK) {
            submitted_count++;
        } else {
            DBGPRINTF("imkafka: splitJsonRecords: failed to submit record %d\n", i);
        }
    }

    /* If no records were successfully submitted, return error to trigger fallback */
    if (submitted_count == 0) {
        LogMsg(0, NO_ERRCODE, LOG_WARNING,
               "imkafka: splitJsonRecords: failed to submit any records, forwarding batch as-is");
        ABORT_FINALIZE(RS_RET_ERR);
    }

finalize_it:
    if (root != NULL) {
        fjson_object_put(root);
    }
    RETiRet;
}

static rsRetVal enqMsg(instanceConf_t *const __restrict__ inst, rd_kafka_message_t *const __restrict__ rkmessage) {
    DEFiRet;
    smsg_t *pMsg;

    if ((int)rkmessage->len == 0) {
        /* we do not process empty lines */
        /* Treat as not-submitted */
        STATSCOUNTER_INC(ctrKafkaFail, mutCtrKafkaFail);
        INST_STATSCOUNTER_INC(inst, inst->ctrKafkaFail, inst->mutCtrKafkaFail);
        FINALIZE;
    }

    /* If JSON record splitting is enabled, try to split the message */
    if (inst->bSplitJsonRecords) {
        rsRetVal split_ret = splitJsonRecords(inst, rkmessage);
        if (split_ret == RS_RET_OK) {
            /* Successfully split and submitted all records */
            FINALIZE;
        }
        /* If splitting failed, fall through to process as single message */
        DBGPRINTF("imkafka: enqMsg: JSON splitting failed or not applicable, processing as single message\n");
    }

    /* Normal single message processing */
    DBGPRINTF("imkafka: enqMsg: Msg: %.*s\n", (int)rkmessage->len, (char *)rkmessage->payload);
    CHKiRet(msgConstruct(&pMsg));
    MsgSetInputName(pMsg, pInputName);
    MsgSetRawMsg(pMsg, (char *)rkmessage->payload, (int)rkmessage->len);
    MsgSetFlowControlType(pMsg, eFLOWCTL_LIGHT_DELAY);
    MsgSetRuleset(pMsg, inst->pBindRuleset);
    pMsg->msgFlags = inst->nMsgParsingFlags;

    /* Optional Fields */
    if (rkmessage->key_len) {
        DBGPRINTF("imkafka: enqMsg: Key: %.*s\n", (int)rkmessage->key_len, (char *)rkmessage->key);
        MsgSetTAG(pMsg, (const uchar *)rkmessage->key, (int)rkmessage->key_len);
    }
    MsgSetMSGoffs(pMsg, 0); /* we do not have a header... */
    CHKiRet(submitMsg2(pMsg));

    /* submitted successfully */
    STATSCOUNTER_INC(ctrSubmitted, mutCtrSubmitted);
    INST_STATSCOUNTER_INC(inst, inst->ctrSubmitted, inst->mutCtrSubmitted);
finalize_it:
    RETiRet;
}

/**
 * Handle Kafka Consumer Loop until all msgs are processed
 */
static void msgConsume(instanceConf_t *inst) {
    rd_kafka_message_t *rkmessage = NULL;

    do { /* Consume messages */
        rkmessage = rd_kafka_consumer_poll(inst->rk, 1000); /* Block for 1000 ms max */
        if (rkmessage == NULL) {
            DBGPRINTF("imkafka: msgConsume EMPTY Loop on %s/%s/%s\n", inst->topic, inst->consumergroup, inst->brokers);
            /* poll returned nothing */
            STATSCOUNTER_INC(ctrPollEmpty, mutCtrPollEmpty);
            INST_STATSCOUNTER_INC(inst, inst->ctrPollEmpty, inst->mutCtrPollEmpty);
            goto done;
        }

        if (rkmessage->err) {
            if (rkmessage->err == RD_KAFKA_RESP_ERR__PARTITION_EOF) {
                /* not an error, just a regular status! */
                DBGPRINTF("imkafka: Consumer reached end of topic \"%s\" [%" PRId32 "] message queue offset %" PRId64
                          "\n",
                          rd_kafka_topic_name(rkmessage->rkt), rkmessage->partition, rkmessage->offset);
                STATSCOUNTER_INC(ctrEOF, mutCtrEOF);
                INST_STATSCOUNTER_INC(inst, inst->ctrEOF, inst->mutCtrEOF);
                goto done;
            }
            if (rkmessage->rkt) {
                LogError(0, RS_RET_KAFKA_ERROR,
                         "imkafka: Consumer error for topic \"%s\" [%" PRId32 "] message queue offset %" PRId64
                         ": %s\n",
                         rd_kafka_topic_name(rkmessage->rkt), rkmessage->partition, rkmessage->offset,
                         rd_kafka_message_errstr(rkmessage));
            } else {
                LogError(0, RS_RET_KAFKA_ERROR, "imkafka: Consumer error for topic \"%s\": \"%s\"\n",
                         rd_kafka_err2str(rkmessage->err), rd_kafka_message_errstr(rkmessage));
            }
            STATSCOUNTER_INC(ctrKafkaFail, mutCtrKafkaFail);
            INST_STATSCOUNTER_INC(inst, inst->ctrKafkaFail, inst->mutCtrKafkaFail);
            goto done;
        }

        DBGPRINTF("imkafka: msgConsume Loop on %s/%s/%s: [%" PRId32 "], offset %" PRId64 ", %zd bytes):\n",
                  rd_kafka_topic_name(rkmessage->rkt) /*inst->topic*/, inst->consumergroup, inst->brokers,
                  rkmessage->partition, rkmessage->offset, rkmessage->len);

        /* message received */
        STATSCOUNTER_INC(ctrReceived, mutCtrReceived);
        INST_STATSCOUNTER_INC(inst, inst->ctrReceived, inst->mutCtrReceived);

        /* compute consumer lag and track max via watermarks (high - current - 1) */
        if (rkmessage->rkt) {
            int64_t lo = 0, hi = 0;
            const char *tname = rd_kafka_topic_name(rkmessage->rkt);
            if (rd_kafka_get_watermark_offsets(inst->rk, tname, rkmessage->partition, &lo, &hi) ==
                RD_KAFKA_RESP_ERR_NO_ERROR) {
                int64_t lag = hi - rkmessage->offset - 1;
                if (lag < 0) lag = 0;
                STATSCOUNTER_SETMAX_NOMUT(ctrMaxLag, (uint64_t)lag);
                INST_STATSCOUNTER_SETMAX(inst, inst->ctrMaxLag, lag);
            }
        }

        /* Hand off into rsyslog core */
        {
            rsRetVal __er = enqMsg(inst, rkmessage);
            if (__er != RS_RET_OK) {
                STATSCOUNTER_INC(ctrKafkaFail, mutCtrKafkaFail);
                INST_STATSCOUNTER_INC(inst, inst->ctrKafkaFail, inst->mutCtrKafkaFail);
            }
        }

        /* Destroy message and continue */
        rd_kafka_message_destroy(rkmessage);
        rkmessage = NULL;
    } while (1); /* loop broken inside */
done:
    /* Destroy message in case rkmessage->err was set */
    if (rkmessage != NULL) {
        rd_kafka_message_destroy(rkmessage);
    }
    return;
}

/* create input instance, set default parameters, and
 * add it to the list of instances.
 */
static rsRetVal createInstance(instanceConf_t **pinst) {
    instanceConf_t *inst;
    DEFiRet;
    CHKmalloc(inst = malloc(sizeof(instanceConf_t)));
    inst->next = NULL;
    inst->brokers = NULL;
    inst->topic = NULL;
    inst->consumergroup = NULL;
    inst->pszBindRuleset = NULL;
    inst->nConfParams = 0;
    inst->confParams = NULL;
    inst->pBindRuleset = NULL;
    inst->bReportErrs = 1; /* Fixed for now */
    inst->nMsgParsingFlags = NEEDS_PARSING;
    inst->bSplitJsonRecords = 0; /* disabled by default */
    inst->bIsConnected = 0;
    inst->bIsSubscribed = 0;
    /* Kafka objects */
    inst->conf = NULL;
    inst->rk = NULL;
    inst->topic_conf = NULL;
    inst->partition = RD_KAFKA_PARTITION_UA;
    /* stats */
    inst->stats = NULL;
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

/* this function checks instance parameters and does some required pre-processing */
static rsRetVal ATTR_NONNULL() checkInstance(instanceConf_t *const inst) {
    DEFiRet;
    char kafkaErrMsg[1024];

    /* main kafka conf */
    inst->conf = rd_kafka_conf_new();
    if (inst->conf == NULL) {
        if (inst->bReportErrs) {
            LogError(0, RS_RET_KAFKA_ERROR, "imkafka: error creating kafka conf obj: %s\n",
                     rd_kafka_err2str(rd_kafka_last_error()));
        }
        ABORT_FINALIZE(RS_RET_KAFKA_ERROR);
    }

#ifdef DEBUG
    /* enable kafka debug output */
    if (rd_kafka_conf_set(inst->conf, "debug", RD_KAFKA_DEBUG_CONTEXTS, kafkaErrMsg, sizeof(kafkaErrMsg)) !=
        RD_KAFKA_CONF_OK) {
        LogError(0, RS_RET_KAFKA_ERROR, "imkafka: error setting kafka debug option: %s\n", kafkaErrMsg);
        /* DO NOT ABORT IN THIS CASE! */
    }
#endif

    /* Set custom configuration parameters */
    for (int i = 0; i < inst->nConfParams; ++i) {
        assert(inst->confParams + i != NULL); /* invariant: nConfParams MUST exist! */
        DBGPRINTF("imkafka: setting custom configuration parameter: %s:%s\n", inst->confParams[i].name,
                  inst->confParams[i].val);
        if (rd_kafka_conf_set(inst->conf, inst->confParams[i].name, inst->confParams[i].val, kafkaErrMsg,
                              sizeof(kafkaErrMsg)) != RD_KAFKA_CONF_OK) {
            if (inst->bReportErrs) {
                LogError(0, RS_RET_PARAM_ERROR, "error setting custom configuration parameter '%s=%s': %s",
                         inst->confParams[i].name, inst->confParams[i].val, kafkaErrMsg);
            } else {
                DBGPRINTF("imkafka: error setting custom configuration parameter '%s=%s': %s", inst->confParams[i].name,
                          inst->confParams[i].val, kafkaErrMsg);
            }
            ABORT_FINALIZE(RS_RET_PARAM_ERROR);
        }
    }

    /* Topic configuration */
    inst->topic_conf = rd_kafka_topic_conf_new();

    /* Assign kafka group id */
    if (inst->consumergroup != NULL) {
        DBGPRINTF("imkafka: setting consumergroup: '%s'\n", inst->consumergroup);
        if (rd_kafka_conf_set(inst->conf, "group.id", (char *)inst->consumergroup, kafkaErrMsg, sizeof(kafkaErrMsg)) !=
            RD_KAFKA_CONF_OK) {
            if (inst->bReportErrs) {
                LogError(0, RS_RET_KAFKA_ERROR, "imkafka: error assigning consumergroup %s to kafka config: %s\n",
                         inst->consumergroup, kafkaErrMsg);
            }
            ABORT_FINALIZE(RS_RET_KAFKA_ERROR);
        }

        /* Set default for auto offset reset */
        if (rd_kafka_topic_conf_set(inst->topic_conf, "auto.offset.reset", "smallest", kafkaErrMsg,
                                    sizeof(kafkaErrMsg)) != RD_KAFKA_CONF_OK) {
            if (inst->bReportErrs) {
                LogError(0, RS_RET_KAFKA_ERROR, "imkafka: error setting kafka auto.offset.reset on %s: %s\n",
                         inst->consumergroup, kafkaErrMsg);
            }
            ABORT_FINALIZE(RS_RET_KAFKA_ERROR);
        }

        /* Consumer groups always use broker based offset storage */
        if (rd_kafka_topic_conf_set(inst->topic_conf, "offset.store.method", "broker", kafkaErrMsg,
                                    sizeof(kafkaErrMsg)) != RD_KAFKA_CONF_OK) {
            if (inst->bReportErrs) {
                LogError(0, RS_RET_KAFKA_ERROR, "imkafka: error setting kafka offset.store.method on %s: %s\n",
                         inst->consumergroup, kafkaErrMsg);
            }
            ABORT_FINALIZE(RS_RET_KAFKA_ERROR);
        }

        /* Set default topic config for pattern-matched topics. */
        rd_kafka_conf_set_default_topic_conf(inst->conf, inst->topic_conf);
    }

#if RD_KAFKA_VERSION >= 0x00090001
    rd_kafka_conf_set_log_cb(inst->conf, kafkaLogger);
#endif

    /* --- librdkafka parity with omkafka: register error + stats callbacks --- */
    rd_kafka_conf_set_error_cb(inst->conf, errorCallback);
    rd_kafka_conf_set_stats_cb(inst->conf, statsCallback);

    /* Create Kafka Consumer */
    inst->rk = rd_kafka_new(RD_KAFKA_CONSUMER, inst->conf, kafkaErrMsg, sizeof(kafkaErrMsg));
    if (inst->rk == NULL) {
        if (inst->bReportErrs) {
            LogError(0, RS_RET_KAFKA_ERROR, "imkafka: error creating kafka handle: %s\n", kafkaErrMsg);
        }
        ABORT_FINALIZE(RS_RET_KAFKA_ERROR);
    }

#if RD_KAFKA_VERSION < 0x00090001
    rd_kafka_set_logger(inst->rk, kafkaLogger);
#endif
    DBGPRINTF("imkafka: setting brokers: '%s'\n", inst->brokers);
    if (rd_kafka_brokers_add(inst->rk, (char *)inst->brokers) == 0) {
        if (inst->bReportErrs) {
            LogError(0, RS_RET_KAFKA_NO_VALID_BROKERS, "imkafka: no valid brokers specified: %s", inst->brokers);
        }
        ABORT_FINALIZE(RS_RET_KAFKA_NO_VALID_BROKERS);
    }

    /* Kafka Consumer is opened */
    inst->bIsConnected = 1;
finalize_it:
    if (iRet != RS_RET_OK) {
        if (inst->rk == NULL) {
            if (inst->conf != NULL) {
                rd_kafka_conf_destroy(inst->conf);
                inst->conf = NULL;
            }
        } else { /* inst->rk != NULL ! */
            rd_kafka_destroy(inst->rk);
            inst->rk = NULL;
        }
    }
    RETiRet;
}

/* function to generate an error message if the ruleset cannot be found */
static inline void std_checkRuleset_genErrMsg(__attribute__((unused)) modConfData_t *modConf, instanceConf_t *inst) {
    if (inst->bReportErrs) {
        LogError(0, NO_ERRCODE, "imkafka: ruleset '%s' not found - using default ruleset instead",
                 inst->pszBindRuleset);
    }
}

static rsRetVal ATTR_NONNULL(2) addConsumer(modConfData_t __attribute__((unused)) * modConf, instanceConf_t *inst) {
    DEFiRet;
    rd_kafka_resp_err_t err;
    assert(inst != NULL);

    rd_kafka_topic_partition_list_t *topics = NULL;

    DBGPRINTF("imkafka: creating kafka consumer on %s/%s/%s\n", inst->topic, inst->consumergroup, inst->brokers);

    /* Redirect rd_kafka_poll() to consumer_poll() */
    rd_kafka_poll_set_consumer(inst->rk);

    topics = rd_kafka_topic_partition_list_new(1);
    rd_kafka_topic_partition_list_add(topics, (const char *)inst->topic, inst->partition);
    DBGPRINTF("imkafka: Created topics(%d) for %s)\n", topics->cnt, inst->topic);

    if ((err = rd_kafka_subscribe(inst->rk, topics))) {
        /* Subscription failed */
        inst->bIsSubscribed = 0;
        LogError(0, RS_RET_KAFKA_ERROR, "imkafka: Failed to start consuming topics: %s\n", rd_kafka_err2str(err));
        ABORT_FINALIZE(RS_RET_KAFKA_ERROR);
    } else {
        DBGPRINTF("imkafka: Successfully subscribed to %s/%s/%s\n", inst->topic, inst->consumergroup, inst->brokers);
        /* Subscription is working */
        inst->bIsSubscribed = 1;
    }

finalize_it:
    if (topics != NULL) rd_kafka_topic_partition_list_destroy(topics);
    RETiRet;
}

static rsRetVal ATTR_NONNULL()
    processKafkaParam(char *const param, const char **const name, const char **const paramval) {
    DEFiRet;
    char *val = strstr(param, "=");
    if (val == NULL) {
        LogError(0, RS_RET_PARAM_ERROR, "missing equal sign in parameter '%s'", param);
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }
    *val = '\0'; /* terminates name */
    ++val; /* now points to begin of value */
    CHKmalloc(*name = strdup(param));
    CHKmalloc(*paramval = strdup(val));
finalize_it:
    RETiRet;
}

BEGINnewInpInst
    struct cnfparamvals *pvals;
    instanceConf_t *inst;
    int i;
    CODESTARTnewInpInst;
    DBGPRINTF("newInpInst (imkafka)\n");
    if ((pvals = nvlstGetParams(lst, &inppblk, NULL)) == NULL) {
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }
    if (Debug) {
        dbgprintf("input param blk in imkafka:\n");
        cnfparamsPrint(&inppblk, pvals);
    }

    CHKiRet(createInstance(&inst));
    for (i = 0; i < inppblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;
        if (!strcmp(inppblk.descr[i].name, "broker")) {
            es_str_t *es = es_newStr(128);
            int bNeedComma = 0;
            for (int j = 0; j < pvals[i].val.d.ar->nmemb; ++j) {
                if (bNeedComma) es_addChar(&es, ',');
                es_addStr(&es, pvals[i].val.d.ar->arr[j]);
                bNeedComma = 1;
            }
            inst->brokers = es_str2cstr(es, NULL);
            es_deleteStr(es);
        } else if (!strcmp(inppblk.descr[i].name, "confparam")) {
            inst->nConfParams = pvals[i].val.d.ar->nmemb;
            CHKmalloc(inst->confParams = malloc(sizeof(struct kafka_params) * inst->nConfParams));
            for (int j = 0; j < inst->nConfParams; j++) {
                char *cstr = es_str2cstr(pvals[i].val.d.ar->arr[j], NULL);
                CHKiRet(processKafkaParam(cstr, &inst->confParams[j].name, &inst->confParams[j].val));
                free(cstr);
            }
        } else if (!strcmp(inppblk.descr[i].name, "topic")) {
            inst->topic = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(inppblk.descr[i].name, "consumergroup")) {
            inst->consumergroup = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(inppblk.descr[i].name, "ruleset")) {
            inst->pszBindRuleset = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(inppblk.descr[i].name, "parsehostname")) {
            if (pvals[i].val.d.n) {
                inst->nMsgParsingFlags = NEEDS_PARSING | PARSE_HOSTNAME;
            } else {
                inst->nMsgParsingFlags = NEEDS_PARSING;
            }
        } else if (!strcmp(inppblk.descr[i].name, "split.json.records")) {
            inst->bSplitJsonRecords = pvals[i].val.d.n;
        } else {
            dbgprintf("imkafka: program error, non-handled param '%s'\n", inppblk.descr[i].name);
        }
    }
    if (inst->brokers == NULL) {
        CHKmalloc(inst->brokers = strdup("localhost:9092"));
        LogMsg(0, NO_ERRCODE, LOG_INFO,
               "imkafka: \"broker\" parameter not specified using default of localhost:9092 -- this may not be what "
               "you want!");
    }
    DBGPRINTF("imkafka: newInpIns brokers=%s, topic=%s, consumergroup=%s\n", inst->brokers, inst->topic,
              inst->consumergroup);
finalize_it:
    CODE_STD_FINALIZERnewInpInst cnfparamvalsDestruct(pvals, &inppblk);
ENDnewInpInst

BEGINbeginCnfLoad
    CODESTARTbeginCnfLoad;
    loadModConf = pModConf;
    pModConf->pConf = pConf;
    pModConf->pszBindRuleset = NULL;
ENDbeginCnfLoad

BEGINsetModCnf
    struct cnfparamvals *pvals = NULL;
    int i;
    CODESTARTsetModCnf;
    pvals = nvlstGetParams(lst, &modpblk, NULL);
    if (pvals == NULL) {
        LogError(0, RS_RET_MISSING_CNFPARAMS, "imkafka: error processing module config parameters [module(...)]");
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }
    if (Debug) {
        dbgprintf("module (global) param blk for imkafka:\n");
        cnfparamsPrint(&modpblk, pvals);
    }
    for (i = 0; i < modpblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;
        if (!strcmp(modpblk.descr[i].name, "ruleset")) {
            loadModConf->pszBindRuleset = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else {
            dbgprintf("imkafka: program error, non-handled param '%s' in beginCnfLoad\n", modpblk.descr[i].name);
        }
    }
finalize_it:
    if (pvals != NULL) cnfparamvalsDestruct(pvals, &modpblk);
ENDsetModCnf

BEGINendCnfLoad
    CODESTARTendCnfLoad;
    if (loadModConf->pszBindRuleset == NULL) {
        if ((cs.pszBindRuleset == NULL) || (cs.pszBindRuleset[0] == '\0')) {
            loadModConf->pszBindRuleset = NULL;
        } else {
            CHKmalloc(loadModConf->pszBindRuleset = ustrdup(cs.pszBindRuleset));
        }
    }
finalize_it:
    free(cs.pszBindRuleset);
    cs.pszBindRuleset = NULL;
    loadModConf = NULL; /* done loading */
ENDendCnfLoad

BEGINcheckCnf
    instanceConf_t *inst;
    CODESTARTcheckCnf;
    for (inst = pModConf->root; inst != NULL; inst = inst->next) {
        if (inst->pszBindRuleset == NULL && pModConf->pszBindRuleset != NULL) {
            CHKmalloc(inst->pszBindRuleset = ustrdup(pModConf->pszBindRuleset));
        }
        std_checkRuleset(pModConf, inst);
    }
finalize_it:
ENDcheckCnf

BEGINactivateCnfPrePrivDrop
    CODESTARTactivateCnfPrePrivDrop;
    runModConf = pModConf;
ENDactivateCnfPrePrivDrop

BEGINactivateCnf
    CODESTARTactivateCnf;
    for (instanceConf_t *inst = pModConf->root; inst != NULL; inst = inst->next) {
        iRet = checkInstance(inst);
        /* Create per-instance stats after successful instance setup */
        if (iRet == RS_RET_OK && inst->stats == NULL) {
            char namebuf[256];
            (void)statsobj.Construct(&inst->stats);
            snprintf(namebuf, sizeof(namebuf), "imkafka[%s_%s]", inst->topic ? (char *)inst->topic : "topic?",
                     inst->consumergroup ? (char *)inst->consumergroup : "group?");
            (void)statsobj.SetName(inst->stats, (uchar *)namebuf);
            (void)statsobj.SetOrigin(inst->stats, (uchar *)"imkafka");
            /* init and register per-instance counters */
            STATSCOUNTER_INIT(inst->ctrReceived, inst->mutCtrReceived);
            (void)statsobj.AddCounter(inst->stats, (uchar *)"received", ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                      &inst->ctrReceived);
            STATSCOUNTER_INIT(inst->ctrSubmitted, inst->mutCtrSubmitted);
            (void)statsobj.AddCounter(inst->stats, (uchar *)"submitted", ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                      &inst->ctrSubmitted);
            STATSCOUNTER_INIT(inst->ctrKafkaFail, inst->mutCtrKafkaFail);
            (void)statsobj.AddCounter(inst->stats, (uchar *)"failures", ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                      &inst->ctrKafkaFail);
            STATSCOUNTER_INIT(inst->ctrEOF, inst->mutCtrEOF);
            (void)statsobj.AddCounter(inst->stats, (uchar *)"eof", ctrType_IntCtr, CTR_FLAG_RESETTABLE, &inst->ctrEOF);
            STATSCOUNTER_INIT(inst->ctrPollEmpty, inst->mutCtrPollEmpty);
            (void)statsobj.AddCounter(inst->stats, (uchar *)"poll_empty", ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                      &inst->ctrPollEmpty);
            STATSCOUNTER_INIT(inst->ctrMaxLag, inst->mutCtrMaxLag);
            (void)statsobj.AddCounter(inst->stats, (uchar *)"maxlag", ctrType_Int, CTR_FLAG_NONE, &inst->ctrMaxLag);
            (void)statsobj.ConstructFinalize(inst->stats);
        }
    }
ENDactivateCnf

BEGINfreeCnf
    instanceConf_t *inst, *del;
    CODESTARTfreeCnf;
    for (inst = pModConf->root; inst != NULL;) {
        if (inst->stats) {
            statsobj.Destruct(&inst->stats);
        }
        free(inst->topic);
        free(inst->consumergroup);
        free(inst->brokers);
        free(inst->pszBindRuleset);
        for (int i = 0; i < inst->nConfParams; i++) {
            free((void *)inst->confParams[i].name);
            free((void *)inst->confParams[i].val);
        }
        free((void *)inst->confParams);
        del = inst;
        inst = inst->next;
        free(del);
    }
    free(pModConf->pszBindRuleset);
ENDfreeCnf

/* Cleanup imkafka worker threads */
static void shutdownKafkaWorkers(void) {
    int i;
    instanceConf_t *inst;

    assert(kafkaWrkrInfo != NULL);
    DBGPRINTF("imkafka: waiting on imkafka workerthread termination\n");
    for (i = 0; i < activeKafkaworkers; ++i) {
        pthread_join(kafkaWrkrInfo[i].tid, NULL);
        DBGPRINTF("imkafka: Stopped worker %d\n", i);
    }
    free(kafkaWrkrInfo);
    kafkaWrkrInfo = NULL;

    for (inst = runModConf->root; inst != NULL; inst = inst->next) {
        DBGPRINTF("imkafka: stop consuming %s/%s/%s\n", inst->topic, inst->consumergroup, inst->brokers);
        rd_kafka_consumer_close(inst->rk); /* Close the consumer, committing final offsets, etc. */
        rd_kafka_destroy(inst->rk); /* Destroy handle object */
        DBGPRINTF("imkafka: stopped consuming %s/%s/%s\n", inst->topic, inst->consumergroup, inst->brokers);
#if RD_KAFKA_VERSION < 0x00090001
        /* Wait for kafka being destroyed in old API */
        if (rd_kafka_wait_destroyed(10000) < 0) {
            DBGPRINTF("imkafka: error, rd_kafka_destroy didn't finish after grace timeout (10s)!\n");
        } else {
            DBGPRINTF("imkafka: rd_kafka_destroy successfully finished\n");
        }
#endif
    }
}

/* This function is called to gather input. */
BEGINrunInput
    int i;
    instanceConf_t *inst;
    CODESTARTrunInput;
    DBGPRINTF("imkafka: runInput loop started ...\n");
    activeKafkaworkers = 0;
    for (inst = runModConf->root; inst != NULL; inst = inst->next) {
        if (inst->rk != NULL) {
            ++activeKafkaworkers;
        }
    }
    if (activeKafkaworkers == 0) {
        LogError(
            0, RS_RET_ERR,
            "imkafka: no active inputs, input not run - other additional error messages should've given previously");
        ABORT_FINALIZE(RS_RET_ERR);
    }
    DBGPRINTF("imkafka: Starting %d imkafka workerthreads\n", activeKafkaworkers);
    kafkaWrkrInfo = calloc(activeKafkaworkers, sizeof(struct kafkaWrkrInfo_s));
    if (kafkaWrkrInfo == NULL) {
        LogError(errno, RS_RET_OUT_OF_MEMORY, "imkafka: worker-info array allocation failed.");
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }
    /* Start worker threads for each imkafka input source */
    i = 0;
    for (inst = runModConf->root; inst != NULL; inst = inst->next) {
        /* init worker info structure! */
        kafkaWrkrInfo[i].inst = inst; /* Set reference pointer */
        pthread_create(&kafkaWrkrInfo[i].tid, &wrkrThrdAttr, imkafkawrkr, &(kafkaWrkrInfo[i]));
        i++;
    }
    while (glbl.GetGlobalInputTermState() == 0) {
        /* Note: the additional 10000ns wait is vitally important. It guards rsyslog
         * against totally hogging the CPU if the users selects a polling interval
         * of 0 seconds. It doesn't hurt any other valid scenario. So do not remove.
         */
        if (glbl.GetGlobalInputTermState() == 0) srSleep(0, 100000);
    }
    DBGPRINTF("imkafka: terminating upon request of rsyslog core\n");
    /* we need to shutdown kafak worker threads here because this operation can
     * potentially block (e.g. when no kafka broker is available!). If this
     * happens in runInput, the rsyslog core can cancel our thread. However,
     * in afterRun this is not possible, because the core does not assume it
     * can block there. -- rgerhards, 2018-10-23
     */
    shutdownKafkaWorkers();
finalize_it:
ENDrunInput

BEGINwillRun
    CODESTARTwillRun;
    /* we need to create the inputName property (only once during our lifetime) */
    CHKiRet(prop.Construct(&pInputName));
    CHKiRet(prop.SetString(pInputName, UCHAR_CONSTANT("imkafka"), sizeof("imkafka") - 1));
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
    /* release objects we used */
    if (kafkaStats) {
        statsobj.Destruct(&kafkaStats);
    }
    objRelease(statsobj, CORE_COMPONENT);
    objRelease(ruleset, CORE_COMPONENT);
    objRelease(glbl, CORE_COMPONENT);
    objRelease(prop, CORE_COMPONENT);
    objRelease(datetime, CORE_COMPONENT);
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
    CHKiRet(objUse(datetime, CORE_COMPONENT));

    /* initialize "read-only" thread attributes */
    pthread_attr_init(&wrkrThrdAttr);
    pthread_attr_setstacksize(&wrkrThrdAttr, 4096 * 1024);

    DBGPRINTF("imkafka %s using librdkafka version %s, 0x%x\n", VERSION, rd_kafka_version_str(), rd_kafka_version());

    /* ---- module-global stats object and counters ---- */
    CHKiRet(statsobj.Construct(&kafkaStats));
    CHKiRet(statsobj.SetName(kafkaStats, (uchar *)"imkafka"));
    CHKiRet(statsobj.SetOrigin(kafkaStats, (uchar *)"imkafka"));

    STATSCOUNTER_INIT(ctrReceived, mutCtrReceived);
    CHKiRet(statsobj.AddCounter(kafkaStats, (uchar *)"received", ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrReceived));
    STATSCOUNTER_INIT(ctrSubmitted, mutCtrSubmitted);
    CHKiRet(statsobj.AddCounter(kafkaStats, (uchar *)"submitted", ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrSubmitted));
    STATSCOUNTER_INIT(ctrKafkaFail, mutCtrKafkaFail);
    CHKiRet(statsobj.AddCounter(kafkaStats, (uchar *)"failures", ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrKafkaFail));
    STATSCOUNTER_INIT(ctrEOF, mutCtrEOF);
    CHKiRet(statsobj.AddCounter(kafkaStats, (uchar *)"eof", ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrEOF));
    STATSCOUNTER_INIT(ctrPollEmpty, mutCtrPollEmpty);
    CHKiRet(statsobj.AddCounter(kafkaStats, (uchar *)"poll_empty", ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrPollEmpty));
    STATSCOUNTER_INIT(ctrMaxLag, mutCtrMaxLag);
    CHKiRet(statsobj.AddCounter(kafkaStats, (uchar *)"maxlag", ctrType_Int, CTR_FLAG_NONE, &ctrMaxLag));

    /* librdkafka window metrics */
    CHKiRet(statsobj.AddCounter(kafkaStats, UCHAR_CONSTANT("rtt_avg_usec"), ctrType_Int, CTR_FLAG_NONE, &rtt_avg_usec));
    CHKiRet(statsobj.AddCounter(kafkaStats, UCHAR_CONSTANT("throttle_avg_msec"), ctrType_Int, CTR_FLAG_NONE,
                                &throttle_avg_msec));
    CHKiRet(statsobj.AddCounter(kafkaStats, UCHAR_CONSTANT("int_latency_avg_usec"), ctrType_Int, CTR_FLAG_NONE,
                                &int_latency_avg_usec));

    /* categorized error counters */
    STATSCOUNTER_INIT(ctrKafkaRespTimedOut, mutCtrKafkaRespTimedOut);
    CHKiRet(statsobj.AddCounter(kafkaStats, (uchar *)"errors_timed_out", ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &ctrKafkaRespTimedOut));
    STATSCOUNTER_INIT(ctrKafkaRespTransport, mutCtrKafkaRespTransport);
    CHKiRet(statsobj.AddCounter(kafkaStats, (uchar *)"errors_transport", ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &ctrKafkaRespTransport));
    STATSCOUNTER_INIT(ctrKafkaRespBrokerDown, mutCtrKafkaRespBrokerDown);
    CHKiRet(statsobj.AddCounter(kafkaStats, (uchar *)"errors_broker_down", ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &ctrKafkaRespBrokerDown));
    STATSCOUNTER_INIT(ctrKafkaRespAuth, mutCtrKafkaRespAuth);
    CHKiRet(statsobj.AddCounter(kafkaStats, (uchar *)"errors_auth", ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &ctrKafkaRespAuth));
    STATSCOUNTER_INIT(ctrKafkaRespSSL, mutCtrKafkaRespSSL);
    CHKiRet(
        statsobj.AddCounter(kafkaStats, (uchar *)"errors_ssl", ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrKafkaRespSSL));
    STATSCOUNTER_INIT(ctrKafkaRespOther, mutCtrKafkaRespOther);
    CHKiRet(statsobj.AddCounter(kafkaStats, (uchar *)"errors_other", ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &ctrKafkaRespOther));

    CHKiRet(statsobj.ConstructFinalize(kafkaStats));
ENDmodInit

/*
 * Workerthread function for a single kafka consomer
 */
static void *imkafkawrkr(void *myself) {
    struct kafkaWrkrInfo_s *me = (struct kafkaWrkrInfo_s *)myself;
    DBGPRINTF("imkafka: started kafka consumer workerthread on %s/%s/%s\n", me->inst->topic, me->inst->consumergroup,
              me->inst->brokers);
    do {
        if (glbl.GetGlobalInputTermState() == 1) break; /* terminate input! */
        if (me->inst->rk == NULL) {
            continue;
        }
        // Try to add consumer only if connected!
        if (me->inst->bIsConnected == 1 && me->inst->bIsSubscribed == 0) {
            addConsumer(runModConf, me->inst);
        }
        if (me->inst->bIsSubscribed == 1) {
            msgConsume(me->inst);
        }
        /* Note: the additional 10000ns wait is vitally important. It guards rsyslog
         * against totally hogging the CPU if the users selects a polling interval
         * of 0 seconds. It doesn't hurt any other valid scenario. So do not remove.
         * rgerhards, 2008-02-14
         */
        if (glbl.GetGlobalInputTermState() == 0) srSleep(0, 100000);
    } while (glbl.GetGlobalInputTermState() == 0);
    DBGPRINTF("imkafka: stopped kafka consumer workerthread on %s/%s/%s\n", me->inst->topic, me->inst->consumergroup,
              me->inst->brokers);
    return NULL;
}
