/* mmleefparse.c
 * This is a message modification module that parses LEEF events and
 * exposes them as structured data inside the message object.
 *
 * Copyright (C) 2025 by Rainer Gerhards and Adiscon GmbH.
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
#include "parserif.h"
#include "dirty.h"

MODULE_TYPE_OUTPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME("mmleefparse")

static rsRetVal resetConfigVariables(uchar __attribute__((unused)) * pp, void __attribute__((unused)) * pVal);

/* static data */

/* internal structures
 */
DEF_OMOD_STATIC_DATA;

typedef struct _instanceData {
    sbool bUseRawMsg; /**< use %rawmsg% instead of %msg% */
    sbool bSearchCookie; /**< search for the cookie within a window */
    char *cookie;
    uchar *container;
    int lenCookie;
    char delimiter;
    int searchWindow; /**< maximum number of characters to search */
} instanceData;

typedef struct wrkrInstanceData {
    instanceData *pData;
} wrkrInstanceData_t;

struct modConfData_s {
    rsconf_t *pConf; /* our overall config object */
};
static modConfData_t *loadModConf = NULL; /* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL; /* modConf ptr to use for the current exec process */

/* tables for interfacing with the v6 config system */
/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {{"cookie", eCmdHdlrString, 0},       {"container", eCmdHdlrString, 0},
                                           {"userawmsg", eCmdHdlrBinary, 0},    {"delimiter", eCmdHdlrString, 0},
                                           {"searchcookie", eCmdHdlrBinary, 0}, {"searchwindow", eCmdHdlrInt, 0}};
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
    CHKmalloc(pData->cookie = strdup("LEEF:"));
    pData->lenCookie = strlen(pData->cookie);
    pData->delimiter = '\t';
    pData->bUseRawMsg = 1;
    pData->bSearchCookie = 1;
    pData->searchWindow = 64;
finalize_it:
ENDcreateInstance

BEGINcreateWrkrInstance
    CODESTARTcreateWrkrInstance;
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
ENDfreeWrkrInstance

BEGINdbgPrintInstInfo
    CODESTARTdbgPrintInstInfo;
    DBGPRINTF("mmleefparse\n");
ENDdbgPrintInstInfo

BEGINtryResume
    CODESTARTtryResume;
ENDtryResume

/**
 * Locate the next delimiter-separated segment in the payload.
 *
 * @param start     pointer to the beginning of the segment
 * @param delim     delimiter that terminates the segment
 * @param[out] len  optional length of the segment (excludes delimiter)
 * @param[out] hasDelim optional flag set when a delimiter was found
 * @retval pointer to the delimiter or the terminating NUL character
 */
static inline const char *find_next_segment(const char *start, char delim, size_t *len, sbool *hasDelim) {
    const char *p = start;
    sbool escaped = 0;

    while (*p != '\0') {
        if (!escaped && *p == '\\') {
            escaped = 1;
            ++p;
            continue;
        }
        if (!escaped && *p == delim) {
            if (len != NULL) *len = (size_t)(p - start);
            if (hasDelim != NULL) *hasDelim = 1;
            return p;
        }
        escaped = 0;
        ++p;
    }
    if (len != NULL) *len = (size_t)(p - start);
    if (hasDelim != NULL) *hasDelim = 0;
    return p;
}

/**
 * Find the first occurrence of a character that is not escaped with '\\'.
 *
 * @param start input buffer
 * @param len   number of bytes to inspect
 * @param ch    character to locate
 * @retval pointer to the matching character or NULL if not found
 */
static inline const char *find_unescaped_char(const char *start, size_t len, char ch) {
    const char *p = start;
    sbool escaped = 0;
    const char *end = start + len;

    while (p < end) {
        if (!escaped && *p == '\\') {
            escaped = 1;
            ++p;
            continue;
        }
        if (!escaped && *p == ch) return p;
        escaped = 0;
        ++p;
    }
    return NULL;
}

/**
 * Duplicate a LEEF segment while resolving escape sequences.
 *
 * @param start segment start pointer
 * @param len   segment length
 * @param delim delimiter used for the current parsing context
 * @param[out] out newly allocated C string with unescaped content
 * @retval RS_RET_OK on success
 */
static rsRetVal leef_unescape_copy(const char *start, size_t len, char delim, char **out) {
    char *buf;
    size_t i;
    size_t w = 0;
    DEFiRet;

    CHKmalloc(buf = (char *)malloc(len + 1));
    for (i = 0; i < len; ++i) {
        if (start[i] == '\\' && (i + 1) < len) {
            char next = start[i + 1];
            if (next == '\\' || next == '=' || next == delim || next == '|') {
                buf[w++] = next;
                ++i;
                continue;
            }
        }
        buf[w++] = start[i];
    }
    buf[w] = '\0';
    *out = buf;
finalize_it:
    RETiRet;
}

/**
 * Parse the header portion of a LEEF payload.
 *
 * @param payload          pointer to the LEEF header (right after the cookie)
 * @param[out] protocolVersion extracted protocol version component
 * @param[out] vendor           extracted vendor name
 * @param[out] product          extracted product identifier
 * @param[out] productVersion   extracted product version
 * @param[out] eventID          extracted event identifier
 * @param[out] extensionStart   pointer positioned at the key/value extension block
 * @retval RS_RET_OK on success
 */
static rsRetVal parse_leef_header(const char *payload,
                                  char **protocolVersion,
                                  char **vendor,
                                  char **product,
                                  char **productVersion,
                                  char **eventID,
                                  const char **extensionStart) {
    const char *cursor = payload;
    const char *segmentEnd;
    size_t len = 0;
    sbool hasDelim = 0;
    DEFiRet;

    /* protocol version */
    segmentEnd = find_next_segment(cursor, '|', &len, &hasDelim);
    if (!hasDelim) ABORT_FINALIZE(RS_RET_NO_CEE_MSG);
    CHKiRet(leef_unescape_copy(cursor, len, '|', protocolVersion));
    cursor = segmentEnd + 1;

    /* vendor */
    segmentEnd = find_next_segment(cursor, '|', &len, &hasDelim);
    if (!hasDelim) ABORT_FINALIZE(RS_RET_NO_CEE_MSG);
    CHKiRet(leef_unescape_copy(cursor, len, '|', vendor));
    cursor = segmentEnd + 1;

    /* product */
    segmentEnd = find_next_segment(cursor, '|', &len, &hasDelim);
    if (!hasDelim) ABORT_FINALIZE(RS_RET_NO_CEE_MSG);
    CHKiRet(leef_unescape_copy(cursor, len, '|', product));
    cursor = segmentEnd + 1;

    /* product version */
    segmentEnd = find_next_segment(cursor, '|', &len, &hasDelim);
    if (!hasDelim && len == 0) ABORT_FINALIZE(RS_RET_NO_CEE_MSG);
    CHKiRet(leef_unescape_copy(cursor, len, '|', productVersion));
    cursor = hasDelim ? segmentEnd + 1 : segmentEnd;

    /* event id */
    segmentEnd = find_next_segment(cursor, '|', &len, &hasDelim);
    CHKiRet(leef_unescape_copy(cursor, len, '|', eventID));
    cursor = hasDelim ? segmentEnd + 1 : segmentEnd;

    if (extensionStart != NULL) *extensionStart = cursor;

finalize_it:
    RETiRet;
}

/**
 * Parse the key/value extension block of a LEEF message.
 *
 * @param pData   module instance data (provides delimiter configuration)
 * @param payload pointer to the start of the extension block
 * @param fields  JSON object that receives the parsed key/value pairs
 * @retval RS_RET_OK on success
 */
static rsRetVal parse_leef_extensions(const instanceData *pData, const char *payload, struct json_object *fields) {
    const char *cursor = payload;
    const char *segmentEnd;
    size_t len = 0;
    sbool hasDelim = 0;
    char delim = pData->delimiter;
    DEFiRet;

    if (cursor == NULL || *cursor == '\0') RETiRet;

    while (*cursor != '\0') {
        segmentEnd = find_next_segment(cursor, delim, &len, &hasDelim);
        if (len == 0 && !hasDelim) break;
        if (len == 0) {
            cursor = hasDelim ? segmentEnd + 1 : segmentEnd;
            continue;
        }

        const char *eq = find_unescaped_char(cursor, len, '=');
        char *key = NULL;
        char *value = NULL;

        if (eq == NULL) {
            CHKiRet(leef_unescape_copy(cursor, len, delim, &key));
            value = strdup("");
            if (value == NULL) {
                free(key);
                ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
            }
        } else {
            size_t keyLen = (size_t)(eq - cursor);
            size_t valueLen = len - keyLen - 1;
            CHKiRet(leef_unescape_copy(cursor, keyLen, delim, &key));
            CHKiRet(leef_unescape_copy(eq + 1, valueLen, delim, &value));
        }

        if (key != NULL && value != NULL) {
            struct json_object *jval = json_object_new_string(value);
            if (jval == NULL) {
                free(key);
                free(value);
                ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
            }
            json_object_object_add(fields, key, jval);
        }
        free(key);
        free(value);

        cursor = hasDelim ? segmentEnd + 1 : segmentEnd;
    }

finalize_it:
    RETiRet;
}

/**
 * Convert a LEEF payload into structured JSON fields and attach them to a message.
 *
 * @param pWrkrData worker-specific instance data
 * @param pMsg      message being processed
 * @param payload   pointer to the payload section following the cookie
 * @retval RS_RET_OK on success
 */
static ATTR_NONNULL() rsRetVal processLEEF(wrkrInstanceData_t *pWrkrData, smsg_t *pMsg, const char *payload) {
    char *protocolVersion = NULL;
    char *vendor = NULL;
    char *product = NULL;
    char *productVersion = NULL;
    char *eventID = NULL;
    const char *extensionStart = NULL;
    struct json_object *root = NULL;
    struct json_object *header = NULL;
    struct json_object *fields = NULL;
    sbool added = 0;
    DEFiRet;

    CHKiRet(
        parse_leef_header(payload, &protocolVersion, &vendor, &product, &productVersion, &eventID, &extensionStart));

    root = json_object_new_object();
    if (root == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    header = json_object_new_object();
    if (header == NULL) {
        json_object_put(root);
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }
    fields = json_object_new_object();
    if (fields == NULL) {
        json_object_put(header);
        json_object_put(root);
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    json_object_object_add(root, "header", header);
    json_object_object_add(root, "fields", fields);

    json_object_object_add(header, "protocolVersion", json_object_new_string(protocolVersion));
    json_object_object_add(header, "vendor", json_object_new_string(vendor));
    json_object_object_add(header, "product", json_object_new_string(product));
    json_object_object_add(header, "productVersion", json_object_new_string(productVersion));
    json_object_object_add(header, "eventID", json_object_new_string(eventID));

    CHKiRet(parse_leef_extensions(pWrkrData->pData, extensionStart, fields));
    msgAddJSON(pMsg, pWrkrData->pData->container, root, 0, 0);
    added = 1;

finalize_it:
    if (!added && root != NULL) json_object_put(root);
    free(protocolVersion);
    free(vendor);
    free(product);
    free(productVersion);
    free(eventID);
    RETiRet;
}

BEGINdoAction_NoStrings
    smsg_t **ppMsg = (smsg_t **)pMsgData;
    smsg_t *pMsg = ppMsg[0];
    uchar *buf;
    uchar *bufBase;
    rs_size_t len = 0;
    int bSuccess = 0;
    instanceData *pData;
    CODESTARTdoAction;
    pData = pWrkrData->pData;

    if (pData->bUseRawMsg) {
        getRawMsg(pMsg, &bufBase, &len);
        buf = bufBase;
    } else {
        bufBase = getMSG(pMsg);
        buf = bufBase;
    }

    while (*buf && isspace(*buf)) ++buf;

    if (pData->bUseRawMsg && buf > bufBase) {
        rs_size_t consumed = (rs_size_t)(buf - bufBase);
        if (len > consumed)
            len -= consumed;
        else
            len = 0;
    }

    if (pData->bSearchCookie) {
        /*
         * Raw syslog transports often prepend a PRI and timestamp before the
         * LEEF cookie.  We scan a limited window from the trimmed start to
         * locate the cookie without risking large linear passes over the full
         * message.
         */
        size_t available;
        if (pData->bUseRawMsg)
            available = (size_t)len;
        else
            available = strlen((char *)buf);

        size_t window = (size_t)pData->searchWindow;
        if (window > available) window = available;

        if (window >= (size_t)pData->lenCookie) {
            size_t maxOffset = window - (size_t)pData->lenCookie;
            /* Sequential search keeps complexity minimal for small windows. */
            for (size_t offset = 0; offset <= maxOffset; ++offset) {
                if (memcmp(buf + offset, pData->cookie, (size_t)pData->lenCookie) == 0) {
                    buf += offset;
                    if (pData->bUseRawMsg) {
                        if (len >= (rs_size_t)(offset))
                            len -= (rs_size_t)offset;
                        else
                            len = 0;
                    }
                    break;
                }
            }
        }
    }

    sbool haveCookie = 0;
    if (pData->bUseRawMsg) {
        if (len >= (rs_size_t)pData->lenCookie && memcmp(buf, pData->cookie, (size_t)pData->lenCookie) == 0) {
            haveCookie = 1;
        }
    } else {
        if (strncmp((char *)buf, pData->cookie, pData->lenCookie) == 0) haveCookie = 1;
    }

    if (!haveCookie) {
        DBGPRINTF("mmleefparse: no LEEF cookie: '%s'\n", buf);
        ABORT_FINALIZE(RS_RET_NO_CEE_MSG);
    }

    buf += pData->lenCookie;
    CHKiRet(processLEEF(pWrkrData, pMsg, (char *)buf));
    bSuccess = 1;
finalize_it:
    if (iRet == RS_RET_NO_CEE_MSG) {
        iRet = RS_RET_OK;
    }
    MsgSetParseSuccess(pMsg, bSuccess);
ENDdoAction

static inline void setInstParamDefaults(instanceData *pData) {
    pData->bUseRawMsg = 1;
    pData->delimiter = '\t';
    pData->bSearchCookie = 1;
    pData->searchWindow = 64;
}

BEGINnewActInst
    struct cnfparamvals *pvals;
    int i;
    CODESTARTnewActInst;
    DBGPRINTF("newActInst (mmleefparse)\n");
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
                memmove(pData->container, pData->container + 1, lenvar);
                --lenvar;
            }
            if ((lenvar == 0) ||
                (!(pData->container[0] == '!' || pData->container[0] == '.' || pData->container[0] == '/'))) {
                parser_errmsg(
                    "mmleefparse: invalid container name '%s', name must start with either '$!', '$.', or '$/'",
                    pData->container);
                ABORT_FINALIZE(RS_RET_INVALID_VAR);
            }
        } else if (!strcmp(actpblk.descr[i].name, "userawmsg")) {
            pData->bUseRawMsg = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "delimiter")) {
            char *tmp = es_str2cstr(pvals[i].val.d.estr, NULL);
            if (tmp == NULL || tmp[0] == '\0') {
                free(tmp);
                parser_errmsg("mmleefparse: delimiter must not be empty");
                ABORT_FINALIZE(RS_RET_INVALID_VALUE);
            }
            if (tmp[0] == '\\') {
                if (tmp[1] == 't' && tmp[2] == '\0')
                    pData->delimiter = '\t';
                else if (tmp[1] == 'n' && tmp[2] == '\0')
                    pData->delimiter = '\n';
                else if (tmp[1] == '\\' && tmp[2] == '\0')
                    pData->delimiter = '\\';
                else {
                    parser_errmsg("mmleefparse: unsupported escape sequence '%s' for delimiter", tmp);
                    free(tmp);
                    ABORT_FINALIZE(RS_RET_INVALID_VALUE);
                }
            } else {
                if (tmp[1] != '\0') {
                    parser_errmsg("mmleefparse: delimiter must be a single character");
                    free(tmp);
                    ABORT_FINALIZE(RS_RET_INVALID_VALUE);
                }
                pData->delimiter = tmp[0];
            }
            free(tmp);
        } else if (!strcmp(actpblk.descr[i].name, "searchcookie")) {
            pData->bSearchCookie = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "searchwindow")) {
            int window = (int)pvals[i].val.d.n;
            if (window <= 0) {
                parser_errmsg("mmleefparse: searchWindow must be greater than zero");
                ABORT_FINALIZE(RS_RET_INVALID_VALUE);
            }
            pData->searchWindow = window;
        } else {
            dbgprintf("mmleefparse: program error, non-handled param '%s'\n", actpblk.descr[i].name);
        }
    }

    if (pData->container == NULL) CHKmalloc(pData->container = (uchar *)strdup("!"));
    if (pData->cookie == NULL) CHKmalloc(pData->cookie = strdup("LEEF:"));
    pData->lenCookie = strlen(pData->cookie);
    CODE_STD_FINALIZERnewActInst;
    cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst

BEGINparseSelectorAct
    CODESTARTparseSelectorAct;
    CODE_STD_STRING_REQUESTparseSelectorAct(1) if (strncmp((char *)p, ":mmleefparse:", sizeof(":mmleefparse:") - 1)) {
        ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
    }

    p += sizeof(":mmleefparse:") - 1;
    CHKiRet(createInstance(&pData));

    if (*(p - 1) == ';') --p;
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
    CODEmodInit_QueryRegCFSLineHdlr DBGPRINTF("mmleefparse: module compiled with rsyslog version %s.\n", VERSION);
    bMsgPassingSupported = 0;
    localRet = pHostQueryEtryPt((uchar *)"OMSRgetSupportedTplOpts", &pomsrGetSupportedTplOpts);
    if (localRet == RS_RET_OK) {
        CHKiRet((*pomsrGetSupportedTplOpts)(&opts));
        if (opts & OMSR_TPL_AS_MSG) bMsgPassingSupported = 1;
    } else if (localRet != RS_RET_ENTRY_POINT_NOT_FOUND) {
        ABORT_FINALIZE(localRet);
    }

    if (!bMsgPassingSupported) {
        DBGPRINTF("mmleefparse: msg-passing is not supported by rsyslog core, can not continue.\n");
        ABORT_FINALIZE(RS_RET_NO_MSG_PASSING);
    }

    CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL,
                               STD_LOADABLE_MODULE_ID));
ENDmodInit
