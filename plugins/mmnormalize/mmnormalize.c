/* mmnormalize.c
 * This is a message modification module. It normalizes the input message with
 * the help of liblognorm. The message's JSON variables are updated.
 *
 * NOTE: read comments in module-template.h for details on the calling interface!
 *
 * File begun on 2010-01-01 by RGerhards
 *
 * Copyright 2010-2015 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Rsyslog is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rsyslog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Rsyslog.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
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
#include <libestr.h>
#include <json.h>
#include <liblognorm.h>
#ifdef HAVE_LOGNORM_TURBO
    #include <lognorm-features.h>
    #include <turbo.h>
    #include <turbo_result_fast.h>
    #include <turbo_snapshot.h>
#endif
#include "conf.h"
#include "syslogd-types.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include "cfsysline.h"
#include "dirty.h"
#include "unicode-helper.h"

MODULE_TYPE_OUTPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME("mmnormalize")

static rsRetVal resetConfigVariables(uchar __attribute__((unused)) * pp, void __attribute__((unused)) * pVal);

/* static data */

/* internal structures
 */
DEF_OMOD_STATIC_DATA;

static struct cnfparamdescr modpdescr[] = {{"allowregex", eCmdHdlrBinary, 0}};

static struct cnfparamblk modpblk = {CNFPARAMBLK_VERSION, sizeof(modpdescr) / sizeof(struct cnfparamdescr), modpdescr};

typedef struct _instanceData {
    sbool bUseRawMsg; /**< use %rawmsg% instead of %msg% */
    uchar *rule; /* rule to use */
    uchar *rulebase; /**< name of rulebase to use */
    ln_ctx ctxln; /**< context to be used for liblognorm */
    char *pszPath; /**< path of normalized data */
    msgPropDescr_t *varDescr; /**< name of variable to use */
#ifdef HAVE_LOGNORM_TURBO
    sbool bTurbo; /**< user requested turbo mode */
    sbool bTurboAvail; /**< turbo compilation succeeded at startup */
    uchar *rulebaseForClone; /**< saved rulebase path for per-worker cloning */
    uchar *ruleForClone; /**< saved inline rules for per-worker cloning */
    unsigned ctxOpts; /**< saved context options for per-worker cloning */
#endif
} instanceData;

typedef struct wrkrInstanceData {
    instanceData *pData;
#ifdef HAVE_LOGNORM_TURBO
    ln_ctx ctxlnTurbo; /**< per-worker turbo context (thread-safe) */
#endif
} wrkrInstanceData_t;

typedef struct configSettings_s {
    uchar *rulebase; /**< name of normalization rulebase to use */
    uchar *rule;
    int bUseRawMsg; /**< use %rawmsg% instead of %msg% */
} configSettings_t;
static configSettings_t cs;

/* tables for interfacing with the v6 config system */
/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {
    {"rulebase", eCmdHdlrGetWord, 0}, {"rule", eCmdHdlrArray, 0},       {"path", eCmdHdlrGetWord, 0},
    {"userawmsg", eCmdHdlrBinary, 0}, {"variable", eCmdHdlrGetWord, 0},
#ifdef HAVE_LOGNORM_TURBO
    {"turbo", eCmdHdlrBinary, 0},
#endif
};
static struct cnfparamblk actpblk = {CNFPARAMBLK_VERSION, sizeof(actpdescr) / sizeof(struct cnfparamdescr), actpdescr};

struct modConfData_s {
    rsconf_t *pConf; /* our overall config object */
    int allow_regex;
};

static modConfData_t *loadModConf = NULL; /* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL; /* modConf ptr to use for the current exec process */

/* callback for liblognorm error messages */
static void errCallBack(void __attribute__((unused)) * cookie, const char *msg, size_t __attribute__((unused)) lenMsg) {
    LogError(0, RS_RET_ERR_LIBLOGNORM, "liblognorm error: %s", msg);
}

/* to be called to build the liblognorm part of the instance ONCE ALL PARAMETERS ARE CORRECT
 * (and set within pData!).
 */
static rsRetVal buildInstance(instanceData *pData) {
    DEFiRet;
    if ((pData->ctxln = ln_initCtx()) == NULL) {
        LogError(0, RS_RET_ERR_LIBLOGNORM_INIT,
                 "error: could not initialize "
                 "liblognorm ctx, cannot activate action");
        ABORT_FINALIZE(RS_RET_ERR_LIBLOGNORM_INIT);
    }
    ln_setCtxOpts(pData->ctxln, loadModConf->allow_regex);
    ln_setErrMsgCB(pData->ctxln, errCallBack, NULL);
#ifdef HAVE_LOGNORM_TURBO
    /* Save context options for per-worker cloning */
    pData->ctxOpts = loadModConf->allow_regex;

    /* Enable turbo on shared ctx so compilation happens during loadSamples */
    if (pData->bTurbo) {
        ln_setCtxOpts(pData->ctxln, LN_CTXOPT_TURBO);
    }
#endif
    if (pData->rule != NULL && pData->rulebase == NULL) {
#ifdef HAVE_LOGNORM_TURBO
        /* Save rule string for per-worker cloning BEFORE it gets freed */
        if (pData->bTurbo) {
            CHKmalloc(pData->ruleForClone = (uchar *)strdup((char *)pData->rule));
        }
#endif
        if (ln_loadSamplesFromString(pData->ctxln, (char *)pData->rule) != 0) {
            LogError(0, RS_RET_NO_RULEBASE,
                     "error: normalization rule '%s' "
                     "could not be loaded cannot activate action",
                     pData->rule);
            ln_exitCtx(pData->ctxln);
            ABORT_FINALIZE(RS_RET_ERR_LIBLOGNORM_SAMPDB_LOAD);
        }
        free(pData->rule);
        pData->rule = NULL;
    } else if (pData->rule == NULL && pData->rulebase != NULL) {
#ifdef HAVE_LOGNORM_TURBO
        /* Save rulebase path for per-worker cloning */
        if (pData->bTurbo) {
            CHKmalloc(pData->rulebaseForClone = (uchar *)strdup((char *)pData->rulebase));
        }
#endif
        if (ln_loadSamples(pData->ctxln, (char *)pData->rulebase) != 0) {
            LogError(0, RS_RET_NO_RULEBASE,
                     "error: normalization rulebase '%s' "
                     "could not be loaded cannot activate action",
                     pData->rulebase);
            ln_exitCtx(pData->ctxln);
            ABORT_FINALIZE(RS_RET_ERR_LIBLOGNORM_SAMPDB_LOAD);
        }
    }
#ifdef HAVE_LOGNORM_TURBO
    /* Verify turbo compilation succeeded on the shared ctx */
    if (pData->bTurbo) {
        if (ln_turbo_is_available(pData->ctxln)) {
            pData->bTurboAvail = 1;
            LogMsg(0, RS_RET_OK, LOG_INFO, "mmnormalize: turbo mode available and enabled");
        } else {
            pData->bTurboAvail = 0;
            LogMsg(0, NO_ERRCODE, LOG_WARNING,
                   "mmnormalize: turbo mode requested but compilation "
                   "failed, using standard normalization");
        }
    }
#endif

finalize_it:
    RETiRet;
}


BEGINinitConfVars /* (re)set config variables to default values */
    CODESTARTinitConfVars;
    resetConfigVariables(NULL, NULL);
ENDinitConfVars


BEGINcreateInstance
    CODESTARTcreateInstance;
ENDcreateInstance


BEGINcreateWrkrInstance
    CODESTARTcreateWrkrInstance;
#ifdef HAVE_LOGNORM_TURBO
    pWrkrData->ctxlnTurbo = NULL;
    if (pData->bTurbo && pData->bTurboAvail) {
        /* Create a SEPARATE ln_ctx for this worker's turbo mode */
        pWrkrData->ctxlnTurbo = ln_initCtx();
        if (pWrkrData->ctxlnTurbo == NULL) {
            LogError(0, RS_RET_ERR_LIBLOGNORM_INIT,
                     "mmnormalize: turbo worker ctx init failed, "
                     "falling back to standard normalization");
        } else {
            int loadRet = -1;
            ln_setCtxOpts(pWrkrData->ctxlnTurbo, pData->ctxOpts);
            ln_setCtxOpts(pWrkrData->ctxlnTurbo, LN_CTXOPT_TURBO);
            ln_setErrMsgCB(pWrkrData->ctxlnTurbo, errCallBack, NULL);

            if (pData->ruleForClone != NULL) {
                loadRet = ln_loadSamplesFromString(pWrkrData->ctxlnTurbo, (char *)pData->ruleForClone);
            } else if (pData->rulebaseForClone != NULL) {
                loadRet = ln_loadSamples(pWrkrData->ctxlnTurbo, (char *)pData->rulebaseForClone);
            }

            if (loadRet != 0 || !ln_turbo_is_available(pWrkrData->ctxlnTurbo)) {
                LogError(0, RS_RET_ERR_LIBLOGNORM_SAMPDB_LOAD,
                         "mmnormalize: turbo worker rulebase "
                         "load/compile failed, falling back "
                         "to standard normalization");
                ln_exitCtx(pWrkrData->ctxlnTurbo);
                pWrkrData->ctxlnTurbo = NULL;
            } else {
                DBGPRINTF(
                    "mmnormalize: turbo worker context "
                    "ready\n");
            }
        }
    }
#endif
ENDcreateWrkrInstance


BEGINbeginCnfLoad
    CODESTARTbeginCnfLoad;
    loadModConf = pModConf;
    pModConf->pConf = pConf;
ENDbeginCnfLoad


BEGINendCnfLoad
    CODESTARTendCnfLoad;
    loadModConf = NULL; /* done loading */
    /* free legacy config vars */
    free(cs.rulebase);
    free(cs.rule);
    cs.rulebase = NULL;
    cs.rule = NULL;
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


BEGINisCompatibleWithFeature
    CODESTARTisCompatibleWithFeature;
ENDisCompatibleWithFeature


BEGINfreeInstance
    CODESTARTfreeInstance;
    free(pData->rulebase);
    free(pData->rule);
    ln_exitCtx(pData->ctxln);
    free(pData->pszPath);
    msgPropDescrDestruct(pData->varDescr);
    free(pData->varDescr);
#ifdef HAVE_LOGNORM_TURBO
    free(pData->rulebaseForClone);
    free(pData->ruleForClone);
#endif
ENDfreeInstance


BEGINfreeWrkrInstance
    CODESTARTfreeWrkrInstance;
#ifdef HAVE_LOGNORM_TURBO
    if (pWrkrData->ctxlnTurbo != NULL) {
        ln_exitCtx(pWrkrData->ctxlnTurbo);
        pWrkrData->ctxlnTurbo = NULL;
    }
#endif
ENDfreeWrkrInstance


BEGINdbgPrintInstInfo
    CODESTARTdbgPrintInstInfo;
    dbgprintf("mmnormalize\n");
    dbgprintf("\tvariable='%s'\n", pData->varDescr ? (char *)pData->varDescr->name : "(none)");
    dbgprintf("\trulebase='%s'\n", pData->rulebase);
    dbgprintf("\trule='%s'\n", pData->rule);
    dbgprintf("\tpath='%s'\n", pData->pszPath);
    dbgprintf("\tbUseRawMsg='%d'\n", pData->bUseRawMsg);
#ifdef HAVE_LOGNORM_TURBO
    dbgprintf("\tturbo='%d' (available=%d)\n", pData->bTurbo, pData->bTurboAvail);
#endif
ENDdbgPrintInstInfo


BEGINtryResume
    CODESTARTtryResume;
ENDtryResume

#ifdef HAVE_LOGNORM_TURBO
    /* Maximum field name length for stack-allocated buffer */
    #define MMNORM_MAX_FIELDNAME 256

/* Forward declaration -- defined below, needed by turbo_result_to_json_cb */
static struct json_object *fast_result_to_json(const ln_fast_result_t *result);

/**
 * Callback for lazy JSON materialization from turbo result snapshot.
 * Called by msg.c when template/property access needs pMsg->json.
 * turbo_result points to an ln_fast_result_snapshot_t* (deep copy).
 */
static void turbo_result_to_json_cb(void *result_ptr, struct json_object **json) {
    const ln_fast_result_snapshot_t *snap = (const ln_fast_result_snapshot_t *)result_ptr;
    if (snap != NULL) {
        const ln_fast_result_t *r = ln_fast_result_snapshot_get(snap);
        if (r != NULL) *json = fast_result_to_json(r);
    }
}

/**
 * Turbo fast-path callback: resolve a single field from the snapshot
 * without building the json-c tree.
 *
 * Path conversion: rsyslog "!sns!src" -> liblognorm "sns.src"
 *   - Skip leading '!' (rsyslog root indicator)
 *   - Replace remaining '!' with '.' (liblognorm uses dot separator)
 *   - Stack buffer: no malloc, max MMNORM_MAX_FIELDNAME bytes
 *
 * Returns 0 on hit (val/vlen set, zero-copy into snapshot), -1 on miss.
 */
static int turbo_result_get_str_cb(
    void *result_ptr, const uchar *name, int nameLen, const uchar **val, rs_size_t *vlen) {
    const ln_fast_result_snapshot_t *snap = (const ln_fast_result_snapshot_t *)result_ptr;
    const ln_fast_result_t *r;
    char keybuf[MMNORM_MAX_FIELDNAME];
    int keylen;

    if (snap == NULL) return -1;

    r = ln_fast_result_snapshot_get(snap);
    if (r == NULL) return -1;

    /* Convert rsyslog path to liblognorm key.
     * pProp->name is "!sns!src" (leading ! = CEE root, ! separators).
     * liblognorm uses "sns.src" (dot separators, no root prefix).
     * Skip first char (!), replace ! with . */
    if (nameLen < 2 || name[0] != '!') return -1; /* bare "!" = full tree request, not a single field */

    keylen = nameLen - 1; /* skip leading '!' */
    if (keylen >= (int)sizeof(keybuf)) return -1;

    const uchar *src = name + 1;
    for (int i = 0; i < keylen; i++) keybuf[i] = (src[i] == '!') ? '.' : (char)src[i];
    keybuf[keylen] = '\0';

    const char *sval;
    size_t slen;
    if (ln_fast_result_get_string(r, keybuf, &sval, &slen) != 0)
        return -1; /* field not in snapshot -> fall through to json */

    *val = (const uchar *)sval;
    *vlen = (rs_size_t)slen;
    return 0;
}


/**
 * Destructor for turbo_result snapshot.
 * The snapshot is a self-contained deep copy (single malloc),
 * so a single free reclaims everything.
 */
static void turbo_result_snapshot_free(void *ptr) {
    if (ptr != NULL) ln_fast_result_snapshot_free((ln_fast_result_snapshot_t *)ptr);
}


/**
 * Convert ln_fast_result_t to json_object directly from typed fields.
 * This avoids the JSON-string serialize + parse roundtrip, which is the
 * key performance win of the turbo integration.
 *
 * Handles nested fields (dotted names like "event.src.ip") by building
 * nested json_objects on the fly.
 */
static struct json_object *fast_result_to_json(const ln_fast_result_t *result) {
    struct json_object *root;
    int nfields;
    int i;
    int ntags;
    char namebuf[MMNORM_MAX_FIELDNAME];

    root = json_object_new_object();
    if (root == NULL) return NULL;

    nfields = ln_fast_result_field_count(result);
    for (i = 0; i < nfields; i++) {
        const ln_fast_field_t *f = &result->fields[i];
        struct json_object *jval = NULL;

        switch (f->type) {
            case LN_FTYPE_STRING:
                jval = json_object_new_string_len(f->v.str.ptr, f->v.str.len);
                break;
            case LN_FTYPE_STRING_INLINE:
                jval = json_object_new_string(f->v.inl);
                break;
            case LN_FTYPE_INT:
                jval = json_object_new_int64(f->v.i);
                break;
            case LN_FTYPE_DOUBLE:
                jval = json_object_new_double(f->v.d);
                break;
            case LN_FTYPE_BOOL:
                jval = json_object_new_boolean(f->v.b);
                break;
            default:
                continue;
        }

        if (jval == NULL) continue;

        /* Field name: stack buffer for common short names, heap
         * fallback for names >= MMNORM_MAX_FIELDNAME.  Silently
         * truncating (the previous behaviour) would collide two
         * distinct field names sharing the first 255 bytes, letting
         * the second write overwrite the first.  The heap path pays
         * one malloc+free per oversized field; pathological inputs
         * should not happen in well-designed rulebases, but the
         * contract is now "no truncation, ever" for correctness. */
        char *name_ptr;
        char *heap_name = NULL;
        if (f->name_len < MMNORM_MAX_FIELDNAME) {
            memcpy(namebuf, f->name, f->name_len);
            namebuf[f->name_len] = '\0';
            name_ptr = namebuf;
        } else {
            heap_name = (char *)malloc(f->name_len + 1);
            if (heap_name == NULL) {
                /* OOM: drop the field rather than truncate.  json_object
                 * created above must be released to avoid leaking. */
                json_object_put(jval);
                continue;
            }
            memcpy(heap_name, f->name, f->name_len);
            heap_name[f->name_len] = '\0';
            name_ptr = heap_name;
        }

        /* Handle nested fields (dotted names).
         * Detect dots directly -- LN_FFIELD_NESTED flag is not
         * always set by liblognorm rule compilation. */
        if (memchr(f->name, '.', f->name_len) != NULL) {
            struct json_object *parent = root;
            char *saveptr = NULL;
            char *tok = strtok_r(name_ptr, ".", &saveptr);
            char *next = strtok_r(NULL, ".", &saveptr);

            while (next != NULL) {
                struct json_object *child = NULL;
                if (!json_object_object_get_ex(parent, tok, &child) || !json_object_is_type(child, json_type_object)) {
                    child = json_object_new_object();
                    json_object_object_add(parent, tok, child);
                }
                parent = child;
                tok = next;
                next = strtok_r(NULL, ".", &saveptr);
            }
            json_object_object_add(parent, tok, jval);
        } else {
            json_object_object_add(root, name_ptr, jval);
        }

        /* json_object_object_add copies the key internally, so
         * heap_name is safe to release on every iteration. */
        free(heap_name);
    }

    /* Add tags at root level as JSON array (ECS standard) */
    ntags = ln_fast_result_tag_count(result);
    if (ntags > 0) {
        struct json_object *tags = json_object_new_array();
        for (i = 0; i < ntags; i++) {
            const char *tag = ln_fast_result_get_tag(result, i);
            if (tag) json_object_array_add(tags, json_object_new_string(tag));
        }
        json_object_object_add(root, "tags", tags);
    }

    return root;
}
#endif /* HAVE_LOGNORM_TURBO */


BEGINdoAction_NoStrings
    smsg_t **ppMsg = (smsg_t **)pMsgData;
    smsg_t *pMsg = ppMsg[0];
    uchar *buf;
    rs_size_t len;
    int r;
    struct json_object *json = NULL;
    unsigned short freeBuf = 0;
    CODESTARTdoAction;
    if (pWrkrData->pData->bUseRawMsg) {
        getRawMsg(pMsg, &buf, &len);
    } else if (pWrkrData->pData->varDescr) {
        buf = MsgGetProp(pMsg, NULL, pWrkrData->pData->varDescr, &len, &freeBuf, NULL);
    } else {
        buf = getMSG(pMsg);
        len = getMSGLen(pMsg);
    }
#ifdef HAVE_LOGNORM_TURBO
    if (pWrkrData->ctxlnTurbo != NULL) {
        const ln_fast_result_t *result = NULL;
        r = ln_turbo_normalize_raw(pWrkrData->ctxlnTurbo, (char *)buf, len, &result);
        if (r == 0 && result != NULL) {
            /* SNAPSHOT PATH: when path is "$!" (CEE root).
             * Create a deep-copy snapshot of the turbo result.
             * The snapshot is a single allocation (~6KB) that owns
             * all string data -- safe for async output actions and
             * non-DIRECT queues. The lazy materializer builds json
             * on-demand only when templates access $! or $!field. */
            if (pWrkrData->pData->pszPath[0] == '$' && pWrkrData->pData->pszPath[1] == '!' &&
                pWrkrData->pData->pszPath[2] == '\0') {
                ln_fast_result_snapshot_t *snap = ln_turbo_snapshot_result(pWrkrData->ctxlnTurbo);
                if (snap == NULL) {
                    DBGPRINTF(
                        "mmnormalize: turbo snapshot alloc failed, "
                        "falling back to JSON materialization\n");
                    json = fast_result_to_json(result);
                    if (json != NULL) goto add_json;
                    goto turbo_done;
                }
                /* If a prior mmnormalize action on the same pMsg
                 * already populated a snapshot, release it before
                 * overwriting — rsyslog action chains can stack
                 * parsers, and each snapshot owns ~6KB of string
                 * data that would otherwise leak. */
                if (pMsg->turbo_result != NULL && pMsg->turbo_result_free != NULL) {
                    pMsg->turbo_result_free(pMsg->turbo_result);
                }
                pMsg->turbo_result = (void *)snap;
                pMsg->turbo_result_free = turbo_result_snapshot_free;
                pMsg->turbo_result_to_json = turbo_result_to_json_cb;
                pMsg->turbo_result_get_str = turbo_result_get_str_cb;
                MsgSetParseSuccess(pMsg, 1);
                goto turbo_done;
            }
            /* Non-$! path: build json from turbo result directly */
            json = fast_result_to_json(result);
            if (json != NULL) goto add_json;
        } else {
            DBGPRINTF(
                "mmnormalize: turbo normalize failed (r=%d), "
                "falling back to standard\n",
                r);
        }
    }
#endif

    /* STANDARD PATH: original ln_normalize (fallback or non-turbo) */
    r = ln_normalize(pWrkrData->pData->ctxln, (char *)buf, len, &json);

#ifdef HAVE_LOGNORM_TURBO
add_json:
#endif
    if (r != 0) {
        DBGPRINTF("error %d during ln_normalize\n", r);
        MsgSetParseSuccess(pMsg, 0);
    } else {
        MsgSetParseSuccess(pMsg, 1);
    }

    msgAddJSON(pMsg, (uchar *)pWrkrData->pData->pszPath + 1, json, 0, 0);

#ifdef HAVE_LOGNORM_TURBO
turbo_done:
#endif
    if (freeBuf) {
        free(buf);
        buf = NULL;
    }

ENDdoAction


static void setInstParamDefaults(instanceData *pData) {
    pData->rulebase = NULL;
    pData->rule = NULL;
    pData->bUseRawMsg = 0;
    pData->pszPath = strdup("$!");
    pData->varDescr = NULL;
#ifdef HAVE_LOGNORM_TURBO
    pData->bTurbo = 0;
    pData->bTurboAvail = 0;
    pData->rulebaseForClone = NULL;
    pData->ruleForClone = NULL;
    pData->ctxOpts = 0;
#endif
}

BEGINsetModCnf
    struct cnfparamvals *pvals = NULL;
    int i;
    CODESTARTsetModCnf;
    pvals = nvlstGetParams(lst, &modpblk, NULL);
    if (pvals == NULL) {
        LogError(0, RS_RET_MISSING_CNFPARAMS,
                 "mmnormalize: error processing module "
                 "config parameters missing [module(...)]");
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }

    if (Debug) {
        dbgprintf("module (global) param blk for mmnormalize:\n");
        cnfparamsPrint(&modpblk, pvals);
    }

    for (i = 0; i < modpblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;
        if (!strcmp(modpblk.descr[i].name, "allowregex")) {
            loadModConf->allow_regex = (int)pvals[i].val.d.n;
        } else {
            dbgprintf(
                "mmnormalize: program error, non-handled "
                "param '%s' in setModCnf\n",
                modpblk.descr[i].name);
        }
    }

finalize_it:
    if (pvals != NULL) cnfparamvalsDestruct(pvals, &modpblk);
ENDsetModCnf


BEGINnewActInst
    struct cnfparamvals *pvals;
    int i;
    int bDestructPValsOnExit;
    char *cstr;
    char *varName = NULL;
    char *buffer;
    char *tStr;
    int size = 0;
    CODESTARTnewActInst;
    DBGPRINTF("newActInst (mmnormalize)\n");

    bDestructPValsOnExit = 0;
    pvals = nvlstGetParams(lst, &actpblk, NULL);
    if (pvals == NULL) {
        LogError(0, RS_RET_MISSING_CNFPARAMS,
                 "mmnormalize: error reading "
                 "config parameters");
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }
    bDestructPValsOnExit = 1;

    if (Debug) {
        dbgprintf("action param blk in mmnormalize:\n");
        cnfparamsPrint(&actpblk, pvals);
    }

    CHKiRet(createInstance(&pData));
    setInstParamDefaults(pData);

    for (i = 0; i < actpblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;
        if (!strcmp(actpblk.descr[i].name, "rulebase")) {
            pData->rulebase = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "rule")) {
            for (int j = 0; j < pvals[i].val.d.ar->nmemb; ++j) {
                tStr = (char *)es_str2cstr(pvals[i].val.d.ar->arr[j], NULL);
                size += strlen(tStr);
                free(tStr);
            }
            buffer = malloc(size + pvals[i].val.d.ar->nmemb + 1);
            tStr = (char *)es_str2cstr(pvals[i].val.d.ar->arr[0], NULL);
            char *dst = buffer;
            memcpy(dst, tStr, strlen(tStr));
            dst += strlen(tStr);
            free(tStr);
            *dst++ = '\n';
            for (int j = 1; j < pvals[i].val.d.ar->nmemb; ++j) {
                tStr = (char *)es_str2cstr(pvals[i].val.d.ar->arr[j], NULL);
                memcpy(dst, tStr, strlen(tStr));
                dst += strlen(tStr);
                free(tStr);
                *dst++ = '\n';
            }
            *dst = '\0';
            pData->rule = (uchar *)buffer;
        } else if (!strcmp(actpblk.descr[i].name, "userawmsg")) {
            pData->bUseRawMsg = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "variable")) {
            varName = es_str2cstr(pvals[i].val.d.estr, NULL);
#ifdef HAVE_LOGNORM_TURBO
        } else if (!strcmp(actpblk.descr[i].name, "turbo")) {
            pData->bTurbo = (int)pvals[i].val.d.n;
#endif
        } else if (!strcmp(actpblk.descr[i].name, "path")) {
            cstr = es_str2cstr(pvals[i].val.d.estr, NULL);
            if (strlen(cstr) < 2) {
                LogError(0, RS_RET_VALUE_NOT_SUPPORTED,
                         "mmnormalize: valid path name should be at least "
                         "2 symbols long, got %s",
                         cstr);
                free(cstr);
            } else if (cstr[0] != '$') {
                LogError(0, RS_RET_VALUE_NOT_SUPPORTED,
                         "mmnormalize: valid path name should start with $,"
                         "got %s",
                         cstr);
                free(cstr);
            } else {
                free(pData->pszPath);
                pData->pszPath = cstr;
            }
            continue;
        } else {
            DBGPRINTF(
                "mmnormalize: program error, non-handled "
                "param '%s'\n",
                actpblk.descr[i].name);
        }
    }

    if (varName) {
        if (pData->bUseRawMsg) {
            LogError(0, RS_RET_CONFIG_ERROR,
                     "mmnormalize: 'variable' param can't be used with 'useRawMsg'. "
                     "Ignoring 'variable', will use raw message.");
        } else {
            CHKmalloc(pData->varDescr = malloc(sizeof(msgPropDescr_t)));
            CHKiRet(msgPropDescrFill(pData->varDescr, (uchar *)varName, strlen(varName)));
        }
        free(varName);
        varName = NULL;
    }
    if (!pData->rulebase) {
        if (!pData->rule) {
            LogError(0, RS_RET_CONFIG_ERROR,
                     "mmnormalize: rulebase needed. "
                     "Use option rulebase or rule.");
        }
    }
    if (pData->rulebase) {
        if (pData->rule) {
            LogError(0, RS_RET_CONFIG_ERROR,
                     "mmnormalize: only one rulebase possible, rulebase "
                     "can't be used with rule");
        }
    }

    CODE_STD_STRING_REQUESTnewActInst(1);
    CHKiRet(OMSRsetEntry(*ppOMSR, 0, NULL, OMSR_TPL_AS_MSG));
    iRet = buildInstance(pData);
    CODE_STD_FINALIZERnewActInst;
    if (bDestructPValsOnExit) cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst


BEGINparseSelectorAct
    CODESTARTparseSelectorAct;
    CODE_STD_STRING_REQUESTparseSelectorAct(1)
        /* first check if this config line is actually for us */
        if (strncmp((char *)p, ":mmnormalize:", sizeof(":mmnormalize:") - 1)) {
        ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
    }

    if (cs.rulebase == NULL && cs.rule == NULL) {
        LogError(0, RS_RET_NO_RULEBASE,
                 "error: no normalization rulebase was specified, use "
                 "$MMNormalizeSampleDB directive first!");
        ABORT_FINALIZE(RS_RET_NO_RULEBASE);
    }

    /* ok, if we reach this point, we have something for us */
    p += sizeof(":mmnormalize:") - 1; /* eat indicator sequence  (-1 because of '\0'!) */
    CHKiRet(createInstance(&pData));

    pData->rulebase = cs.rulebase;
    pData->rule = cs.rule;
    pData->bUseRawMsg = cs.bUseRawMsg;
    pData->pszPath = strdup("$!"); /* old interface does not support this feature */
    /* all config vars auto-reset! */
    cs.bUseRawMsg = 0;
    cs.rulebase = NULL; /* we used it up! */
    cs.rule = NULL;

    /* check if a non-standard template is to be applied */
    if (*(p - 1) == ';') --p;
    /* we call the function below because we need to call it via our interface definition. However,
     * the format specified (if any) is always ignored.
     */
    CHKiRet(cflineParseTemplateName(&p, *ppOMSR, 0, OMSR_TPL_AS_MSG, (uchar *)"RSYSLOG_FileFormat"));
    CHKiRet(buildInstance(pData));
    CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct

#ifdef HAVE_LOGNORM_TURBO
BEGINdoHUPWrkr
    CODESTARTdoHUPWrkr DBGPRINTF("mmnormalize: HUP received\n");
    if (pWrkrData->ctxlnTurbo != NULL && pWrkrData->pData->bTurbo) {
        DBGPRINTF("mmnormalize: HUP - rebuilding turbo worker context\n");
        ln_exitCtx(pWrkrData->ctxlnTurbo);
        pWrkrData->ctxlnTurbo = ln_initCtx();
        if (pWrkrData->ctxlnTurbo != NULL) {
            int loadRet = -1;
            ln_setCtxOpts(pWrkrData->ctxlnTurbo, pWrkrData->pData->ctxOpts);
            ln_setCtxOpts(pWrkrData->ctxlnTurbo, LN_CTXOPT_TURBO);
            ln_setErrMsgCB(pWrkrData->ctxlnTurbo, errCallBack, NULL);

            if (pWrkrData->pData->ruleForClone != NULL) {
                loadRet = ln_loadSamplesFromString(pWrkrData->ctxlnTurbo, (char *)pWrkrData->pData->ruleForClone);
            } else if (pWrkrData->pData->rulebaseForClone != NULL) {
                loadRet = ln_loadSamples(pWrkrData->ctxlnTurbo, (char *)pWrkrData->pData->rulebaseForClone);
            }

            if (loadRet != 0 || !ln_turbo_is_available(pWrkrData->ctxlnTurbo)) {
                LogError(0, RS_RET_ERR_LIBLOGNORM_SAMPDB_LOAD,
                         "mmnormalize: HUP turbo reload failed, "
                         "falling back to standard");
                ln_exitCtx(pWrkrData->ctxlnTurbo);
                pWrkrData->ctxlnTurbo = NULL;
            } else {
                LogMsg(0, RS_RET_OK, LOG_INFO,
                       "mmnormalize: turbo worker context "
                       "reloaded");
            }
        } else {
            LogError(0, RS_RET_ERR_LIBLOGNORM_INIT, "mmnormalize: HUP turbo ctx init failed");
        }
    }
ENDdoHUPWrkr
#endif /* HAVE_LOGNORM_TURBO */

BEGINmodExit
    CODESTARTmodExit;
ENDmodExit


BEGINqueryEtryPt
    CODESTARTqueryEtryPt;
    CODEqueryEtryPt_STD_OMOD_QUERIES;
    CODEqueryEtryPt_STD_OMOD8_QUERIES;
    CODEqueryEtryPt_STD_CONF2_QUERIES;
    CODEqueryEtryPt_STD_CONF2_setModCnf_QUERIES;
    CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES;
#ifdef HAVE_LOGNORM_TURBO
    CODEqueryEtryPt_doHUPWrkr
#endif
ENDqueryEtryPt


/* Reset config variables for this module to default values.
 */
static rsRetVal resetConfigVariables(uchar __attribute__((unused)) * pp, void __attribute__((unused)) * pVal) {
    DEFiRet;
    cs.rulebase = NULL;
    cs.rule = NULL;
    cs.bUseRawMsg = 0;
    RETiRet;
}

/* set the rulebase name */
static rsRetVal setRuleBase(void __attribute__((unused)) * pVal, uchar *pszName) {
    DEFiRet;
    cs.rulebase = pszName;
    pszName = NULL;
    RETiRet;
}

BEGINmodInit()
    rsRetVal localRet;
    rsRetVal (*pomsrGetSupportedTplOpts)(unsigned long *pOpts);
    unsigned long opts;
    int bMsgPassingSupported;
    CODESTARTmodInit;
    INITLegCnfVars;
    *ipIFVersProvided = CURR_MOD_IF_VERSION;
    /* we only support the current interface specification */
    CODEmodInit_QueryRegCFSLineHdlr DBGPRINTF("mmnormalize: module compiled with rsyslog version %s.\n", VERSION);
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
            "mmnormalize: msg-passing is not supported by rsyslog core, "
            "can not continue.\n");
        ABORT_FINALIZE(RS_RET_NO_MSG_PASSING);
    }

    CHKiRet(omsdRegCFSLineHdlr((uchar *)"mmnormalizerulebase", 0, eCmdHdlrGetWord, setRuleBase, NULL,
                               STD_LOADABLE_MODULE_ID));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"mmnormalizerule", 0, eCmdHdlrGetWord, NULL, NULL, STD_LOADABLE_MODULE_ID));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"mmnormalizeuserawmsg", 0, eCmdHdlrBinary, NULL, &cs.bUseRawMsg,
                               STD_LOADABLE_MODULE_ID));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL,
                               STD_LOADABLE_MODULE_ID));
ENDmodInit

/* vi:set ai:
 */
