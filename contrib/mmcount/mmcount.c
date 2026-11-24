/* SPDX-License-Identifier: Apache-2.0 */

/* mmcount.c
 * count messages by priority or json property of given app-name.
 *
 * Copyright 2013 Red Hat Inc.
 * Copyright 2014 Rainer Gerhards
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
#include "config.h"
#include "rsyslog.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <limits.h>
#include <json.h>
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include "hashtable.h"


#define JSON_COUNT_NAME "!mmcount"
#define SEVERITY_COUNT 8
#define DFLT_MAX_CARDINALITY 10000

MODULE_TYPE_OUTPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME("mmcount")


DEF_OMOD_STATIC_DATA;

/* config variables */

typedef struct _instanceData {
    char *pszAppName;
    int severity[SEVERITY_COUNT];
    char *pszKey;
    char *pszValue;
    int valueCounter;
    struct hashtable *ht;
    unsigned int max_cardinality; /* max distinct values tracked in ht */
    sbool cardinality_warned; /* limit-reached warning already emitted? */
    pthread_mutex_t mut;
} instanceData;

typedef struct wrkrInstanceData {
    instanceData *pData;
} wrkrInstanceData_t;

struct modConfData_s {
    rsconf_t *pConf; /* our overall config object */
};
static modConfData_t *loadModConf = NULL; /* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL; /* modConf ptr to use for the current exec process */

/* Key for the per-value counter table hash. The table is filled with values
 * taken from messages, so the hash must not be predictable: the generic
 * hash_from_string() lets a sender craft any number of distinct values with
 * the same hash (e.g. concatenations of "ab" and "bA"), which turns every
 * lookup into a linear scan of one chain while pData->mut is held.
 * SipHash-2-4 keyed with a per-process random key removes that ability.
 */
static uint64_t hash_key[2];

#define SIP_ROTL(x, b) (uint64_t)(((x) << (b)) | ((x) >> (64 - (b))))
#define SIP_ROUND              \
    do {                       \
        v0 += v1;              \
        v1 = SIP_ROTL(v1, 13); \
        v1 ^= v0;              \
        v0 = SIP_ROTL(v0, 32); \
        v2 += v3;              \
        v3 = SIP_ROTL(v3, 16); \
        v3 ^= v2;              \
        v0 += v3;              \
        v3 = SIP_ROTL(v3, 21); \
        v3 ^= v0;              \
        v2 += v1;              \
        v1 = SIP_ROTL(v1, 17); \
        v1 ^= v2;              \
        v2 = SIP_ROTL(v2, 32); \
    } while (0)

/* SipHash-2-4 (Aumasson/Bernstein) of a NUL-terminated string, keyed with
 * hash_key. Wraparound arithmetic is intended.
 */
#if defined(__clang__)
    #pragma GCC diagnostic ignored "-Wunknown-attributes"
#endif
static unsigned int
#if defined(__clang__)
    __attribute__((no_sanitize("unsigned-integer-overflow")))
#endif
    hash_value_keyed(void *k) {
    const uchar *in = (const uchar *)k;
    const size_t len = strlen((const char *)in);
    const uchar *const end = in + (len - (len % 8));
    uint64_t v0 = UINT64_C(0x736f6d6570736575) ^ hash_key[0];
    uint64_t v1 = UINT64_C(0x646f72616e646f6d) ^ hash_key[1];
    uint64_t v2 = UINT64_C(0x6c7967656e657261) ^ hash_key[0];
    uint64_t v3 = UINT64_C(0x7465646279746573) ^ hash_key[1];
    uint64_t m;
    uint64_t b = ((uint64_t)len) << 56;
    int i;

    for (; in != end; in += 8) {
        m = 0;
        for (i = 0; i < 8; ++i) m |= ((uint64_t)in[i]) << (8 * i);
        v3 ^= m;
        SIP_ROUND;
        SIP_ROUND;
        v0 ^= m;
    }
    for (i = 0; i < (int)(len % 8); ++i) b |= ((uint64_t)in[i]) << (8 * i);
    v3 ^= b;
    SIP_ROUND;
    SIP_ROUND;
    v0 ^= b;
    v2 ^= 0xff;
    SIP_ROUND;
    SIP_ROUND;
    SIP_ROUND;
    SIP_ROUND;
    b = v0 ^ v1 ^ v2 ^ v3;
    return (unsigned int)(b ^ (b >> 32));
}
#undef SIP_ROUND
#undef SIP_ROTL

/* Fill hash_key with 128 bits from the OS entropy pool. /dev/urandom exists
 * on all platforms rsyslog supports; randomNumber() is only a fallback since
 * outside Linux it is time-seeded random(). The cardinality limit still bounds
 * the worst case in that fallback.
 */
static void init_hash_key(void) {
    unsigned i;
    int fd;
    ssize_t r = -1;

    fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (fd >= 0) {
        r = read(fd, hash_key, sizeof(hash_key));
        close(fd);
    }
    if (r != (ssize_t)sizeof(hash_key)) {
        LogMsg(errno, RS_RET_IO_ERROR, LOG_WARNING,
               "mmcount: could not read /dev/urandom, per-value counter hash "
               "uses a weaker fallback key");
        for (i = 0; i < 2; ++i) {
            hash_key[i] = ((uint64_t)(uint32_t)randomNumber() << 32) | (uint32_t)randomNumber();
        }
    }
}


/* tables for interfacing with the v6 config system */
/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {
    {"appname", eCmdHdlrGetWord, 0},
    {"key", eCmdHdlrGetWord, 0},
    {"value", eCmdHdlrGetWord, 0},
    {"maxcardinality", eCmdHdlrPositiveInt, 0},
};
static struct cnfparamblk actpblk = {CNFPARAMBLK_VERSION, sizeof(actpdescr) / sizeof(struct cnfparamdescr), actpdescr};

BEGINbeginCnfLoad
    CODESTARTbeginCnfLoad;
    loadModConf = pModConf;
    pModConf->pConf = pConf;
ENDbeginCnfLoad

BEGINendCnfLoad
    CODESTARTendCnfLoad;
ENDendCnfLoad

BEGINcheckCnf
    CODESTARTcheckCnf;
ENDcheckCnf

BEGINactivateCnf
    CODESTARTactivateCnf;
    runModConf = pModConf;
ENDactivateCnf

BEGINfreeCnf
    CODESTARTfreeCnf;
ENDfreeCnf


BEGINcreateInstance
    CODESTARTcreateInstance;
    pthread_mutex_init(&pData->mut, NULL);
ENDcreateInstance

BEGINcreateWrkrInstance
    CODESTARTcreateWrkrInstance;
ENDcreateWrkrInstance


BEGINisCompatibleWithFeature
    CODESTARTisCompatibleWithFeature;
ENDisCompatibleWithFeature


BEGINfreeInstance
    CODESTARTfreeInstance;
    if (pData->ht != NULL) hashtable_destroy(pData->ht, 1);
ENDfreeInstance


BEGINfreeWrkrInstance
    CODESTARTfreeWrkrInstance;
ENDfreeWrkrInstance

static inline void setInstParamDefaults(instanceData *pData) {
    int i;

    pData->pszAppName = NULL;
    for (i = 0; i < SEVERITY_COUNT; i++) pData->severity[i] = 0;
    pData->pszKey = NULL;
    pData->pszValue = NULL;
    pData->valueCounter = 0;
    pData->ht = NULL;
    pData->max_cardinality = DFLT_MAX_CARDINALITY;
    pData->cardinality_warned = 0;
}

BEGINnewActInst
    struct cnfparamvals *pvals;
    int i;
    CODESTARTnewActInst;
    DBGPRINTF("newActInst (mmcount)\n");
    if ((pvals = nvlstGetParams(lst, &actpblk, NULL)) == NULL) {
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }

    CODE_STD_STRING_REQUESTnewActInst(1);
    CHKiRet(OMSRsetEntry(*ppOMSR, 0, NULL, OMSR_TPL_AS_MSG));
    CHKiRet(createInstance(&pData));
    setInstParamDefaults(pData);

    for (i = 0; i < actpblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;
        if (!strcmp(actpblk.descr[i].name, "appname")) {
            CHKmalloc(pData->pszAppName = es_str2cstr(pvals[i].val.d.estr, NULL));
            continue;
        }
        if (!strcmp(actpblk.descr[i].name, "key")) {
            CHKmalloc(pData->pszKey = es_str2cstr(pvals[i].val.d.estr, NULL));
            continue;
        }
        if (!strcmp(actpblk.descr[i].name, "value")) {
            CHKmalloc(pData->pszValue = es_str2cstr(pvals[i].val.d.estr, NULL));
            continue;
        }
        if (!strcmp(actpblk.descr[i].name, "maxcardinality")) {
            pData->max_cardinality =
                (pvals[i].val.d.n > (long long)UINT_MAX) ? UINT_MAX : (unsigned int)pvals[i].val.d.n;
            continue;
        }
        dbgprintf(
            "mmcount: program error, non-handled "
            "param '%s'\n",
            actpblk.descr[i].name);
    }

    if (pData->pszAppName == NULL) {
        dbgprintf("mmcount: action requires a appname");
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }

    if (pData->pszKey != NULL && pData->pszValue == NULL) {
        if (NULL == (pData->ht = create_hashtable(100, hash_value_keyed, key_equals_string, NULL))) {
            DBGPRINTF("mmcount: error creating hash table!\n");
            ABORT_FINALIZE(RS_RET_ERR);
        }
    }
    CODE_STD_FINALIZERnewActInst;
    cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst


BEGINdbgPrintInstInfo
    CODESTARTdbgPrintInstInfo;
ENDdbgPrintInstInfo


BEGINtryResume
    CODESTARTtryResume;
ENDtryResume

static int *getCounter(instanceData *const pData, const char *str, sbool *const warn_once) {
    struct hashtable *const ht = pData->ht;
    int *pCounter;
    char *pKey;

    /* Keep the complete value so distinct strings with the same hash
       retain independent counters. The table owns a copy of the key. */
    pCounter = hashtable_search(ht, (void *)str);
    if (pCounter) {
        return pCounter;
    }

    /* Values come from messages: bound the number of tracked values so a
       sender cannot grow the table (and memory) without limit. */
    if (hashtable_count(ht) >= pData->max_cardinality) {
        if (!pData->cardinality_warned) {
            pData->cardinality_warned = 1;
            *warn_once = 1; /* caller logs after releasing pData->mut */
        }
        return NULL;
    }

    /* counter is not found for the str, so add new entry and
       return the counter */
    if (NULL == (pKey = strdup(str))) {
        DBGPRINTF("mmcount: memory allocation for key failed\n");
        return NULL;
    }

    if (NULL == (pCounter = (int *)malloc(sizeof(int)))) {
        DBGPRINTF("mmcount: memory allocation for value failed\n");
        free(pKey);
        return NULL;
    }
    *pCounter = 0;

    if (!hashtable_insert(ht, pKey, pCounter)) {
        DBGPRINTF("mmcount: inserting element into hashtable failed\n");
        free(pKey);
        free(pCounter);
        return NULL;
    }
    return pCounter;
}

BEGINdoAction_NoStrings
    smsg_t **ppMsg = (smsg_t **)pMsgData;
    smsg_t *pMsg = ppMsg[0];
    char *appname;
    struct json_object *json = NULL;
    struct json_object *keyjson = NULL;
    const char *pszValue;
    int *pCounter;
    sbool cardinality_warn = 0;
    instanceData *const pData = pWrkrData->pData;
    CODESTARTdoAction;
    appname = getAPPNAME(pMsg, LOCK_MUTEX);

    pthread_mutex_lock(&pData->mut);
    if (0 != strcmp(appname, pData->pszAppName)) {
        /* we are not working for this appname. nothing to do */
        ABORT_FINALIZE(RS_RET_OK);
    }

    if (!pData->pszKey) {
        /* no key given for count, so we count severity */
        if (pMsg->iSeverity < SEVERITY_COUNT) {
            pData->severity[pMsg->iSeverity]++;
            json = json_object_new_int(pData->severity[pMsg->iSeverity]);
        }
        ABORT_FINALIZE(RS_RET_OK);
    }

    /* key is given, so get the property json */
    msgPropDescr_t pProp;
    msgPropDescrFill(&pProp, (uchar *)pData->pszKey, strlen(pData->pszKey));
    rsRetVal localRet = msgGetJSONPropJSON(pMsg, &pProp, &keyjson);
    msgPropDescrDestruct(&pProp);
    if (localRet != RS_RET_OK) {
        /* key not found in the message. nothing to do */
        ABORT_FINALIZE(RS_RET_OK);
    }

    /* key found, so get the value */
    pszValue = (char *)json_object_get_string(keyjson);
    if (pszValue == NULL) { /* json null object returns NULL! */
        pszValue = "";
    }

    if (pData->pszValue) {
        /* value also given for count */
        if (!strcmp(pszValue, pData->pszValue)) {
            /* count for (value and key and appname) matched */
            pData->valueCounter++;
            json = json_object_new_int(pData->valueCounter);
        }
        ABORT_FINALIZE(RS_RET_OK);
    }

    /* value is not given, so we count for each value of given key */
    pCounter = getCounter(pData, pszValue, &cardinality_warn);
    if (pCounter) {
        (*pCounter)++;
        json = json_object_new_int(*pCounter);
    }
finalize_it:
    pthread_mutex_unlock(&pData->mut);

    if (cardinality_warn) {
        /* logged outside pData->mut: an internal message may be processed
           synchronously and pass through this action again */
        LogMsg(0, RS_RET_OK_WARN, LOG_WARNING,
               "mmcount: action for appname '%s', key '%s' reached "
               "maxCardinality %u; messages with further new values are "
               "not counted. This warning is emitted only once per action.",
               pData->pszAppName, pData->pszKey, pData->max_cardinality);
    }

    if (json) {
        msgAddJSON(pMsg, (uchar *)JSON_COUNT_NAME, json, 0, 0);
    }
ENDdoAction


NO_LEGACY_CONF_parseSelectorAct


    BEGINmodExit CODESTARTmodExit;
ENDmodExit


BEGINqueryEtryPt
    CODESTARTqueryEtryPt;
    CODEqueryEtryPt_STD_OMOD_QUERIES;
    CODEqueryEtryPt_STD_OMOD8_QUERIES;
    CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES;
    CODEqueryEtryPt_STD_CONF2_QUERIES;
ENDqueryEtryPt


BEGINmodInit()
    CODESTARTmodInit;
    *ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
    init_hash_key();
    CODEmodInit_QueryRegCFSLineHdlr DBGPRINTF("mmcount: module compiled with rsyslog version %s.\n", VERSION);
ENDmodInit
