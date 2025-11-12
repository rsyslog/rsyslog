/* mmjsonparse.c
 * This is a message modification module. If give, it extracts JSON data
 * and populates the EE event structure with it.
 *
 * NOTE: read comments in module-template.h for details on the calling interface!
 *
 * File begun on 2012-02-20 by RGerhards
 *
 * Copyright 2012-2025 Adiscon GmbH.
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
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <json.h>

#include "rsyslog.h"
#include "conf.h"
#include "syslogd-types.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include "cfsysline.h"
#include "dirty.h"
#include "statsobj.h"

MODULE_TYPE_OUTPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME("mmjsonparse")

static rsRetVal resetConfigVariables(uchar __attribute__((unused)) * pp, void __attribute__((unused)) * pVal);

/* static data */

/* internal structures
 */
DEF_OMOD_STATIC_DATA;
DEFobjCurrIf(glbl) DEFobjCurrIf(statsobj)

    /* parsing modes */
    typedef enum {
        PARSE_MODE_COOKIE = 0, /**< legacy CEE cookie mode (default) */
        PARSE_MODE_FIND_JSON = 1 /**< find first JSON object mode */
    } parse_mode_t;

typedef struct _instanceData {
    sbool bUseRawMsg; /**< use %rawmsg% instead of %msg% */
    char *cookie;
    uchar *container;
    int lenCookie;
    parse_mode_t mode; /**< parsing mode: cookie or find-json */
    int max_scan_bytes; /**< max bytes to scan in find-json mode */
    sbool allow_trailing; /**< allow trailing data after JSON in find-json mode */
    /* TODO: add start_regex support in future enhancement */
} instanceData;

typedef struct wrkrInstanceData {
    instanceData *pData;
    struct json_tokener *tokener;
    /* Statistics counters - manually expanded from STATSCOUNTER_DEF */
    intctr_t ctrScanAttempted;
    DEF_ATOMIC_HELPER_MUT64(mutCtrScanAttempted);
    intctr_t ctrScanFound;
    DEF_ATOMIC_HELPER_MUT64(mutCtrScanFound);
    intctr_t ctrScanFailed;
    DEF_ATOMIC_HELPER_MUT64(mutCtrScanFailed);
    intctr_t ctrScanTruncated;
    DEF_ATOMIC_HELPER_MUT64(mutCtrScanTruncated);
    statsobj_t *statsobj;
} wrkrInstanceData_t;

struct modConfData_s {
    rsconf_t *pConf; /* our overall config object */
};
static modConfData_t *loadModConf = NULL; /* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL; /* modConf ptr to use for the current exec process */

/* tables for interfacing with the v6 config system */
/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {
    {"cookie", eCmdHdlrString, 0}, {"container", eCmdHdlrString, 0},           {"userawmsg", eCmdHdlrBinary, 0},
    {"mode", eCmdHdlrString, 0},   {"max_scan_bytes", eCmdHdlrPositiveInt, 0}, {"allow_trailing", eCmdHdlrBinary, 0}};
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
    CHKmalloc(pData->container = (uchar *)strdup("!"));
    CHKmalloc(pData->cookie = strdup(CONST_CEE_COOKIE));
    pData->lenCookie = CONST_LEN_CEE_COOKIE;
    pData->mode = PARSE_MODE_COOKIE; /* default to legacy behavior */
    pData->max_scan_bytes = 65536; /* default scan limit */
    pData->allow_trailing = 1; /* default allow trailing data */
finalize_it:
ENDcreateInstance

BEGINcreateWrkrInstance
    CODESTARTcreateWrkrInstance;
    pWrkrData->tokener = json_tokener_new();
    if (pWrkrData->tokener == NULL) {
        LogError(0, RS_RET_ERR,
                 "error: could not create json "
                 "tokener, cannot activate instance");
        ABORT_FINALIZE(RS_RET_ERR);
    }

    /* Initialize statistics counters */
    STATSCOUNTER_INIT(pWrkrData->ctrScanAttempted, pWrkrData->mutCtrScanAttempted);
    STATSCOUNTER_INIT(pWrkrData->ctrScanFound, pWrkrData->mutCtrScanFound);
    STATSCOUNTER_INIT(pWrkrData->ctrScanFailed, pWrkrData->mutCtrScanFailed);
    STATSCOUNTER_INIT(pWrkrData->ctrScanTruncated, pWrkrData->mutCtrScanTruncated);

    /* Create stats object for this worker instance */
    CHKiRet(statsobj.Construct(&(pWrkrData->statsobj)));
    CHKiRet(statsobj.SetName(pWrkrData->statsobj, (uchar *)"mmjsonparse"));
    CHKiRet(statsobj.SetOrigin(pWrkrData->statsobj, (uchar *)"mmjsonparse"));
    CHKiRet(statsobj.AddCounter(pWrkrData->statsobj, (uchar *)"scan.attempted", ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &(pWrkrData->ctrScanAttempted)));
    CHKiRet(statsobj.AddCounter(pWrkrData->statsobj, (uchar *)"scan.found", ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &(pWrkrData->ctrScanFound)));
    CHKiRet(statsobj.AddCounter(pWrkrData->statsobj, (uchar *)"scan.failed", ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &(pWrkrData->ctrScanFailed)));
    CHKiRet(statsobj.AddCounter(pWrkrData->statsobj, (uchar *)"scan.truncated", ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &(pWrkrData->ctrScanTruncated)));
    CHKiRet(statsobj.ConstructFinalize(pWrkrData->statsobj));

finalize_it:
ENDcreateWrkrInstance


BEGINisCompatibleWithFeature
    CODESTARTisCompatibleWithFeature;
ENDisCompatibleWithFeature


BEGINfreeInstance
    CODESTARTfreeInstance;
    free(pData->cookie);
    free(pData->container);
ENDfreeInstance

BEGINfreeWrkrInstance
    CODESTARTfreeWrkrInstance;
    if (pWrkrData->tokener != NULL) json_tokener_free(pWrkrData->tokener);
    if (pWrkrData->statsobj != NULL) {
        statsobj.Destruct(&pWrkrData->statsobj);
    }
ENDfreeWrkrInstance


BEGINdbgPrintInstInfo
    CODESTARTdbgPrintInstInfo;
    DBGPRINTF("mmjsonparse\n");
ENDdbgPrintInstInfo


BEGINtryResume
    CODESTARTtryResume;
ENDtryResume


/**
 * Find the first valid JSON object in a message buffer using the actual JSON parser.
 * This function scans for '{' characters and uses json_tokener to validate complete objects.
 *
 * @param pWrkrData Worker instance data (contains tokener)
 * @param msg       Message buffer to scan
 * @param len       Length of message buffer
 * @param start_off Starting offset for scan
 * @param max_scan  Maximum bytes to scan
 * @param obj_off   [OUT] Offset where JSON object starts
 * @param obj_len   [OUT] Length of JSON object
 * @param parsed_json [OUT] Already parsed JSON object (caller must release)
 * @param allow_trailing Whether trailing data after JSON is allowed
 * @return 0 on success, 1 if no JSON found, 2 if scan truncated, 3 if trailing data not allowed
 */
static int find_first_json_object(wrkrInstanceData_t *pWrkrData,
                                  const uchar *msg,
                                  size_t len,
                                  size_t start_off,
                                  size_t max_scan,
                                  size_t *obj_off,
                                  size_t *obj_len,
                                  struct json_object **parsed_json,
                                  sbool allow_trailing) {
    size_t i = start_off;
    size_t scan_end = start_off + max_scan;
    if (scan_end > len) scan_end = len;

    *parsed_json = NULL;

    /* Find potential JSON start positions ('{' characters) */
    while (i < scan_end) {
        /* Find the next '{' */
        const uchar *p = memchr(msg + i, '{', scan_end - i);
        if (p == NULL) {
            i = scan_end;
        } else {
            i = p - msg;
        }

        if (i >= scan_end) {
            return (i >= len) ? 1 : 2; /* no JSON found or scan truncated */
        }

        /* Try to parse JSON starting from this position */
        size_t remaining = len - i;
        json_tokener_reset(pWrkrData->tokener);

        struct json_object *json = json_tokener_parse_ex(pWrkrData->tokener, (const char *)(msg + i), remaining);

        if (json != NULL && json_object_is_type(json, json_type_object)) {
            /* Valid JSON object found */
            size_t parsed_len = pWrkrData->tokener->char_offset;

            /* Check for trailing data if allow_trailing is false */
            if (!allow_trailing && parsed_len < remaining) {
                size_t check_pos = i + parsed_len;
                while (check_pos < len && isspace(msg[check_pos])) {
                    check_pos++;
                }
                if (check_pos < len) {
                    json_object_put(json); /* release the object */
                    return 3; /* trailing data not allowed */
                }
            }

            *obj_off = i;
            *obj_len = parsed_len;
            *parsed_json = json; /* pass ownership to caller */
            return 0; /* success */
        }

        /* Clean up if parsing failed */
        if (json != NULL) {
            json_object_put(json);
        }

        /* Move to next potential start position */
        i++;
    }

    return 2; /* scan truncated or no valid JSON found */
}


static rsRetVal processJSON(wrkrInstanceData_t *pWrkrData, smsg_t *pMsg, struct json_object *json) {
    DEFiRet;

    DBGPRINTF("mmjsonparse: processing already parsed JSON object\n");

    /* JSON is already parsed and validated, just add it to the message */
    CHKiRet(msgAddJSON(pMsg, pWrkrData->pData->container, json, 0, 0));
    /* Note: msgAddJSON takes ownership of the json object on success,
     * but we need to clean up on failure */

finalize_it:
    if (iRet != RS_RET_OK) {
        /* msgAddJSON failed, we need to clean up the JSON object */
        json_object_put(json);
    }
    RETiRet;
}

static rsRetVal processJSONBuffer(wrkrInstanceData_t *pWrkrData, smsg_t *pMsg, char *buf, size_t lenBuf) {
    struct json_object *json;
    const char *errMsg;
    DEFiRet;

    assert(pWrkrData->tokener != NULL);
    DBGPRINTF("mmjsonparse: toParse: '%s'\n", buf);
    json_tokener_reset(pWrkrData->tokener);

    json = json_tokener_parse_ex(pWrkrData->tokener, buf, lenBuf);
    if (Debug) {
        errMsg = NULL;
        if (json == NULL) {
            enum json_tokener_error err;

            err = pWrkrData->tokener->err;
            if (err != json_tokener_continue)
                errMsg = json_tokener_error_desc(err);
            else
                errMsg = "Unterminated input";
        } else if ((size_t)pWrkrData->tokener->char_offset < lenBuf)
            errMsg = "Extra characters after JSON object";
        else if (!json_object_is_type(json, json_type_object))
            errMsg = "JSON value is not an object";
        if (errMsg != NULL) {
            DBGPRINTF("mmjsonparse: Error parsing JSON '%s': %s\n", buf, errMsg);
        }
    }
    if (json == NULL || ((size_t)pWrkrData->tokener->char_offset < lenBuf) ||
        (!json_object_is_type(json, json_type_object))) {
        if (json != NULL) {
            /* Release json object as we are not going to add it to pMsg */
            json_object_put(json);
        }
        ABORT_FINALIZE(RS_RET_NO_CEE_MSG);
    }

    msgAddJSON(pMsg, pWrkrData->pData->container, json, 0, 0);
finalize_it:
    RETiRet;
}

BEGINdoAction_NoStrings
    smsg_t **ppMsg = (smsg_t **)pMsgData;
    smsg_t *pMsg = ppMsg[0];
    uchar *buf;
    rs_size_t len;
    int bSuccess = 0;
    struct json_object *jval;
    struct json_object *json;
    instanceData *pData;
    CODESTARTdoAction;
    pData = pWrkrData->pData;

    /* Get message buffer */
    if (pWrkrData->pData->bUseRawMsg)
        getRawMsg(pMsg, &buf, &len);
    else {
        buf = getMSG(pMsg);
        len = getMSGLen(pMsg);
    }

    /* Handle different parsing modes */
    if (pData->mode == PARSE_MODE_COOKIE) {
        /* Legacy CEE cookie mode */
        size_t remaining = len;

        /* Skip leading whitespace - length-aware */
        while (remaining > 0 && isspace(*buf)) {
            ++buf;
            --remaining;
        }

        /* Check for cookie presence - length-aware */
        if (remaining == 0 || remaining < (size_t)pData->lenCookie ||
            memcmp(buf, pData->cookie, pData->lenCookie) != 0) {
            DBGPRINTF("mmjsonparse: no JSON cookie found\n");
            ABORT_FINALIZE(RS_RET_NO_CEE_MSG);
        }

        buf += pData->lenCookie;
        remaining -= pData->lenCookie;

        CHKiRet(processJSONBuffer(pWrkrData, pMsg, (char *)buf, remaining));
        bSuccess = 1;
    } else if (pData->mode == PARSE_MODE_FIND_JSON) {
        /* Find-JSON mode */
        size_t obj_off, obj_len;
        int scan_result;
        struct json_object *parsed_json = NULL;

        STATSCOUNTER_INC(pWrkrData->ctrScanAttempted, pWrkrData->mutCtrScanAttempted);

        scan_result = find_first_json_object(pWrkrData, buf, len, 0, pData->max_scan_bytes, &obj_off, &obj_len,
                                             &parsed_json, pData->allow_trailing);

        if (scan_result == 0) {
            /* JSON object found and already parsed */
            STATSCOUNTER_INC(pWrkrData->ctrScanFound, pWrkrData->mutCtrScanFound);
            iRet = processJSON(pWrkrData, pMsg, parsed_json);
            if (iRet == RS_RET_OK) {
                bSuccess = 1;
            } else {
                /* JSON was found but processing failed */
                STATSCOUNTER_INC(pWrkrData->ctrScanFailed, pWrkrData->mutCtrScanFailed);
                /* Will be handled in finalize_it */
            }
        } else {
            /* Handle scan failures */
            if (scan_result == 2) {
                STATSCOUNTER_INC(pWrkrData->ctrScanTruncated, pWrkrData->mutCtrScanTruncated);
                DBGPRINTF("mmjsonparse: JSON scan truncated at max_scan_bytes=%d\n", pData->max_scan_bytes);
            } else if (scan_result == 3) {
                STATSCOUNTER_INC(pWrkrData->ctrScanFound,
                                 pWrkrData->mutCtrScanFound); /* JSON was found but trailing data rejected */
                DBGPRINTF("mmjsonparse: trailing data found but allow_trailing=off\n");
                /* Note: find_first_json_object already cleaned up the JSON object for error case 3 */
            } else {
                DBGPRINTF("mmjsonparse: no JSON object found in message\n");
            }
            ABORT_FINALIZE(RS_RET_NO_CEE_MSG);
        }
    }

finalize_it:
    if (iRet == RS_RET_NO_CEE_MSG) {
        /* add buf as msg */
        json = json_object_new_object();
        jval = json_object_new_string((char *)buf);
        json_object_object_add(json, "msg", jval);
        msgAddJSON(pMsg, pData->container, json, 0, 0);
        iRet = RS_RET_OK;
    }
    MsgSetParseSuccess(pMsg, bSuccess);
ENDdoAction

static inline void setInstParamDefaults(instanceData *pData) {
    pData->bUseRawMsg = 0;
}

BEGINnewActInst
    struct cnfparamvals *pvals;
    int i;
    CODESTARTnewActInst;
    DBGPRINTF("newActInst (mmjsonparse)\n");
    if ((pvals = nvlstGetParams(lst, &actpblk, NULL)) == NULL) {
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }


    CODE_STD_STRING_REQUESTnewActInst(1);
    CHKiRet(OMSRsetEntry(*ppOMSR, 0, NULL, OMSR_TPL_AS_MSG));
    CHKiRet(createInstance(&pData));
    setInstParamDefaults(pData);

    for (i = 0; i < actpblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;
        if (!strcmp(actpblk.descr[i].name, "cookie")) {
            free(pData->cookie);
            pData->cookie = es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "container")) {
            free(pData->container);
            size_t lenvar = es_strlen(pvals[i].val.d.estr);
            pData->container = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
            if (pData->container[0] == '$') {
                /* pre 8.35, the container name needed to be specified without
                 * the leading $. This was confusing, so we now require a full
                 * variable name. Nevertheless, we still need to support the
                 * version without $. -- rgerhards, 2018-05-16
                 */
                /* copy lenvar size because of \0 string terminator */
                memmove(pData->container, pData->container + 1, lenvar);
                --lenvar;
            }
            if ((lenvar == 0) ||
                (!(pData->container[0] == '!' || pData->container[0] == '.' || pData->container[0] == '/'))) {
                LogError(0, RS_RET_INVALID_VAR,
                         "mmjsonparse: invalid container name '%s', name must "
                         "start with either '$!', '$.', or '$/'",
                         pData->container);
                ABORT_FINALIZE(RS_RET_INVALID_VAR);
            }
        } else if (!strcmp(actpblk.descr[i].name, "userawmsg")) {
            pData->bUseRawMsg = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "mode")) {
            char *mode_str = es_str2cstr(pvals[i].val.d.estr, NULL);
            if (!strcmp(mode_str, "cookie")) {
                pData->mode = PARSE_MODE_COOKIE;
            } else if (!strcmp(mode_str, "find-json")) {
                pData->mode = PARSE_MODE_FIND_JSON;
            } else {
                LogError(0, RS_RET_CONF_PARAM_INVLD, "mmjsonparse: invalid mode '%s', must be 'cookie' or 'find-json'",
                         mode_str);
                free(mode_str);
                ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
            }
            free(mode_str);
        } else if (!strcmp(actpblk.descr[i].name, "max_scan_bytes")) {
            pData->max_scan_bytes = (int)pvals[i].val.d.n;
            if (pData->max_scan_bytes <= 0) {
                LogError(0, RS_RET_CONF_PARAM_INVLD, "mmjsonparse: max_scan_bytes must be positive, got %d",
                         pData->max_scan_bytes);
                ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
            }
        } else if (!strcmp(actpblk.descr[i].name, "allow_trailing")) {
            pData->allow_trailing = (int)pvals[i].val.d.n;
        } else {
            dbgprintf("mmjsonparse: program error, non-handled param '%s'\n", actpblk.descr[i].name);
        }
    }

    if (pData->container == NULL) CHKmalloc(pData->container = (uchar *)strdup("!"));
    pData->lenCookie = strlen(pData->cookie);
    CODE_STD_FINALIZERnewActInst;
    cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst

BEGINparseSelectorAct
    CODESTARTparseSelectorAct;
    CODE_STD_STRING_REQUESTparseSelectorAct(1)
        /* first check if this config line is actually for us */
        if (strncmp((char *)p, ":mmjsonparse:", sizeof(":mmjsonparse:") - 1)) {
        ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
    }

    /* ok, if we reach this point, we have something for us */
    p += sizeof(":mmjsonparse:") - 1; /* eat indicator sequence  (-1 because of '\0'!) */
    CHKiRet(createInstance(&pData));

    /* check if a non-standard template is to be applied */
    if (*(p - 1) == ';') --p;
    /* we call the function below because we need to call it via our interface definition. However,
     * the format specified (if any) is always ignored.
     */
    iRet = cflineParseTemplateName(&p, *ppOMSR, 0, OMSR_TPL_AS_MSG, (uchar *)"RSYSLOG_FileFormat");
    CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINmodExit
    CODESTARTmodExit;
ENDmodExit


BEGINqueryEtryPt
    CODESTARTqueryEtryPt;
    CODEqueryEtryPt_STD_OMOD_QUERIES;
    CODEqueryEtryPt_STD_OMOD8_QUERIES;
    CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES;
    CODEqueryEtryPt_STD_CONF2_QUERIES;
ENDqueryEtryPt


/* Reset config variables for this module to default values.
 */
static rsRetVal resetConfigVariables(uchar __attribute__((unused)) * pp, void __attribute__((unused)) * pVal) {
    DEFiRet;
    RETiRet;
}


BEGINmodInit()
    rsRetVal localRet;
    rsRetVal (*pomsrGetSupportedTplOpts)(unsigned long *pOpts);
    unsigned long opts;
    int bMsgPassingSupported;
    CODESTARTmodInit;
    *ipIFVersProvided = CURR_MOD_IF_VERSION;
    /* we only support the current interface specification */
    CODEmodInit_QueryRegCFSLineHdlr;
    CHKiRet(objUse(glbl, CORE_COMPONENT));
    CHKiRet(objUse(statsobj, CORE_COMPONENT));
    DBGPRINTF("mmjsonparse: module compiled with rsyslog version %s.\n", VERSION);
    /* check if the rsyslog core supports parameter passing code */
    bMsgPassingSupported = 0;
    localRet = pHostQueryEtryPt((uchar *)"OMSRgetSupportedTplOpts", &pomsrGetSupportedTplOpts);
    if (localRet == RS_RET_OK) {
        /* found entry point, so let's see if core supports msg passing */
        CHKiRet((*pomsrGetSupportedTplOpts)(&opts));
        if (opts & OMSR_TPL_AS_MSG) bMsgPassingSupported = 1;
    } else if (localRet != RS_RET_ENTRY_POINT_NOT_FOUND) {
        ABORT_FINALIZE(localRet); /* Something else went wrong, not acceptable */
    }

    if (!bMsgPassingSupported) {
        DBGPRINTF(
            "mmjsonparse: msg-passing is not supported by rsyslog core, "
            "can not continue.\n");
        ABORT_FINALIZE(RS_RET_NO_MSG_PASSING);
    }


    CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL,
                               STD_LOADABLE_MODULE_ID));
ENDmodInit

/* vi:set ai:
 */
