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
 *
 * Concurrency and locking:
 * - ctxln is shared by all action workers. Normalization holds ctxlnLock for
 *   reading; HUP builds a replacement privately and swaps it under the write
 *   lock.
 * - ctxlnTurbo is owned and used only by its action worker. HUP publishes an
 *   atomic reload generation, and each worker rebuilds its context between
 *   messages. The HUP thread never reads, replaces, or destroys worker state.
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
    #include <lognorm-turbo.h>
#endif
#include "conf.h"
#include "syslogd-types.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include "cfsysline.h"
#include "dirty.h"
#include "unicode-helper.h"
#include "atomic.h"

MODULE_TYPE_OUTPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME("mmnormalize")

/* Concurrency & Locking
 * ---------------------
 * Turbo normalization contexts are private to their workers. When debugFile
 * is configured, callbacks from the action and its worker contexts share the
 * instance-owned FILE; flockfile serializes writes. The action lifetime ends
 * after workers stop, before freeInstance closes that FILE.
 */

static rsRetVal resetConfigVariables(uchar __attribute__((unused)) * pp, void __attribute__((unused)) * pVal);

/* static data */

/* internal structures
 */
DEF_OMOD_STATIC_DATA;

static struct cnfparamdescr modpdescr[] = {{"allowregex", eCmdHdlrBinary, 0}};

static struct cnfparamblk modpblk = {CNFPARAMBLK_VERSION, sizeof(modpdescr) / sizeof(struct cnfparamdescr), modpdescr};

typedef struct _instanceData {
    sbool bUseRawMsg; /**< use %rawmsg% instead of %msg% */
    sbool bDebug; /**< enable liblognorm debugging */
    uchar *rule; /* rule to use */
    uchar *rulebase; /**< name of rulebase to use */
    ln_ctx ctxln; /**< context to be used for liblognorm */
    pthread_rwlock_t ctxlnLock; /**< protects replacement and use of shared ctxln */
    int ctxlnLockInitialized;
    unsigned reloadGeneration; /**< HUP generation adopted by action workers */
    pthread_mutex_t mutReloadGeneration; /**< no-atomics fallback for reloadGeneration */
    int mutReloadGenerationInitialized;
    uchar *rulebaseForReload; /**< saved rulebase path for HUP reload */
    uchar *ruleForReload; /**< saved inline rules for HUP reload */
    unsigned ctxOpts; /**< saved context options for HUP reload */
    FILE *debugFile; /**< optional liblognorm debug output */
    char *pszPath; /**< path of normalized data */
    msgPropDescr_t *varDescr; /**< name of variable to use */
#ifdef HAVE_LOGNORM_TURBO
    sbool bTurbo; /**< user requested turbo mode */
#endif
} instanceData;

typedef struct wrkrInstanceData {
    instanceData *pData;
#ifdef HAVE_LOGNORM_TURBO
    ln_ctx ctxlnTurbo; /**< per-worker turbo context (thread-safe) */
    unsigned reloadGeneration; /**< rulebase generation loaded into ctxlnTurbo */
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
    {"rulebase", eCmdHdlrGetWord, 0},  {"rule", eCmdHdlrArray, 0},       {"path", eCmdHdlrGetWord, 0},
    {"userawmsg", eCmdHdlrBinary, 0},  {"variable", eCmdHdlrGetWord, 0}, {"debug", eCmdHdlrBinary, 0},
    {"debugfile", eCmdHdlrGetWord, 0},
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

/* callback for liblognorm debug messages */
static void debugCallBack(void *cookie, const char *msg, size_t lenMsg) {
    FILE *const debugFile = cookie;

    if (debugFile == NULL) {
        LogMsg(0, RS_RET_OK, LOG_DEBUG, "mmnormalize: liblognorm debug: %s", msg);
        return;
    }

    flockfile(debugFile);
    (void)fwrite(msg, 1, lenMsg, debugFile);
    if (lenMsg == 0 || msg[lenMsg - 1] != '\n') (void)fputc('\n', debugFile);
    (void)fflush(debugFile);
    funlockfile(debugFile);
}

/* Build a complete context without publishing it. The saved rule source is
 * immutable after configuration, so this helper can be used by HUP and by an
 * action worker without protecting the potentially expensive compilation. */
static ln_ctx buildContext(const instanceData *const pData, const sbool enableTurbo, sbool *const turboAvailable) {
    ln_ctx ctx;
    int loadRet = -1;

    if (turboAvailable != NULL) *turboAvailable = 0;
    ctx = ln_initCtx();
    if (ctx == NULL) return NULL;

    ln_setCtxOpts(ctx, pData->ctxOpts);
#ifdef HAVE_LOGNORM_TURBO
    if (enableTurbo) ln_setCtxOpts(ctx, LN_CTXOPT_TURBO);
#else
    (void)enableTurbo;
#endif
    ln_setErrMsgCB(ctx, errCallBack, NULL);
    if (pData->bDebug) {
        if (ln_setDebugCB(ctx, debugCallBack, pData->debugFile) != 0) {
            ln_exitCtx(ctx);
            return NULL;
        }
        ln_enableDebug(ctx, 1);
    }

    if (pData->ruleForReload != NULL) {
        loadRet = ln_loadSamplesFromString(ctx, (char *)pData->ruleForReload);
    } else if (pData->rulebaseForReload != NULL) {
        loadRet = ln_loadSamples(ctx, (char *)pData->rulebaseForReload);
    }

    if (loadRet != 0) {
        ln_exitCtx(ctx);
        return NULL;
    }

#ifdef HAVE_LOGNORM_TURBO
    if (enableTurbo && turboAvailable != NULL) *turboAvailable = ln_turbo_is_available(ctx);
#endif
    return ctx;
}

#ifdef HAVE_LOGNORM_TURBO
static ln_ctx buildTurboWorkerContext(const instanceData *const pData) {
    sbool turboAvailable = 0;
    ln_ctx ctx = buildContext(pData, 1, &turboAvailable);

    if (ctx != NULL && !turboAvailable) {
        ln_exitCtx(ctx);
        ctx = NULL;
    }
    return ctx;
}
#endif
/* to be called to build the liblognorm part of the instance ONCE ALL PARAMETERS ARE CORRECT
 * (and set within pData!).
 */
static rsRetVal buildInstance(instanceData *pData) {
    DEFiRet;
    pData->ctxOpts = loadModConf->allow_regex;
    if ((pData->ctxln = ln_initCtx()) == NULL) {
        LogError(0, RS_RET_ERR_LIBLOGNORM_INIT,
                 "error: could not initialize "
                 "liblognorm ctx, cannot activate action");
        ABORT_FINALIZE(RS_RET_ERR_LIBLOGNORM_INIT);
    }
    ln_setCtxOpts(pData->ctxln, pData->ctxOpts);
    ln_setErrMsgCB(pData->ctxln, errCallBack, NULL);
    if (pData->bDebug) {
        if (ln_setDebugCB(pData->ctxln, debugCallBack, pData->debugFile) != 0) {
            LogError(0, RS_RET_ERR_LIBLOGNORM_INIT, "mmnormalize: could not set liblognorm debug callback");
            ABORT_FINALIZE(RS_RET_ERR_LIBLOGNORM_INIT);
        }
        ln_enableDebug(pData->ctxln, 1);
    }
#ifdef HAVE_LOGNORM_TURBO
    /* Enable turbo on shared ctx so compilation happens during loadSamples */
    if (pData->bTurbo) {
        ln_setCtxOpts(pData->ctxln, LN_CTXOPT_TURBO);
    }
#endif
    if (pData->rule != NULL && pData->rulebase == NULL) {
        /* Preserve the inline rule before the configuration copy is freed. */
        CHKmalloc(pData->ruleForReload = (uchar *)strdup((char *)pData->rule));
        if (ln_loadSamplesFromString(pData->ctxln, (char *)pData->rule) != 0) {
            LogError(0, RS_RET_NO_RULEBASE,
                     "error: normalization rule '%s' "
                     "could not be loaded cannot activate action",
                     pData->rule);
            ln_exitCtx(pData->ctxln);
            pData->ctxln = NULL;
            ABORT_FINALIZE(RS_RET_ERR_LIBLOGNORM_SAMPDB_LOAD);
        }
        free(pData->rule);
        pData->rule = NULL;
    } else if (pData->rule == NULL && pData->rulebase != NULL) {
        CHKmalloc(pData->rulebaseForReload = (uchar *)strdup((char *)pData->rulebase));
        if (ln_loadSamples(pData->ctxln, (char *)pData->rulebase) != 0) {
            LogError(0, RS_RET_NO_RULEBASE,
                     "error: normalization rulebase '%s' "
                     "could not be loaded cannot activate action",
                     pData->rulebase);
            ln_exitCtx(pData->ctxln);
            pData->ctxln = NULL;
            ABORT_FINALIZE(RS_RET_ERR_LIBLOGNORM_SAMPDB_LOAD);
        }
    }
#ifdef HAVE_LOGNORM_TURBO
    /* Verify turbo compilation succeeded on the shared ctx */
    if (pData->bTurbo) {
        if (ln_turbo_is_available(pData->ctxln)) {
            LogMsg(0, RS_RET_OK, LOG_INFO, "mmnormalize: turbo mode available and enabled");
        } else {
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
    int lockRet;
    CODESTARTcreateInstance;
    if ((lockRet = pthread_rwlock_init(&pData->ctxlnLock, NULL)) != 0) {
        errno = lockRet;
        ABORT_FINALIZE(RS_RET_CONC_CTRL_ERR);
    }
    pData->ctxlnLockInitialized = 1;
    if ((lockRet = pthread_mutex_init(&pData->mutReloadGeneration, NULL)) != 0) {
        errno = lockRet;
        pthread_rwlock_destroy(&pData->ctxlnLock);
        pData->ctxlnLockInitialized = 0;
        ABORT_FINALIZE(RS_RET_CONC_CTRL_ERR);
    }
    pData->mutReloadGenerationInitialized = 1;
    ATOMIC_STORE_32BIT_unsigned(&pData->reloadGeneration, &pData->mutReloadGeneration, 0);
finalize_it:
ENDcreateInstance


BEGINcreateWrkrInstance
    CODESTARTcreateWrkrInstance;
#ifdef HAVE_LOGNORM_TURBO
    pWrkrData->reloadGeneration = ATOMIC_LOAD_32BIT_unsigned(&pData->reloadGeneration, &pData->mutReloadGeneration);
    pWrkrData->ctxlnTurbo = NULL;
    if (pData->bTurbo) {
        /* Create a SEPARATE ln_ctx for this worker's turbo mode */
        pWrkrData->ctxlnTurbo = buildTurboWorkerContext(pData);
        if (pWrkrData->ctxlnTurbo == NULL) {
            LogError(0, RS_RET_ERR_LIBLOGNORM_SAMPDB_LOAD,
                     "mmnormalize: turbo worker context build failed, "
                     "falling back to standard normalization");
        } else {
            DBGPRINTF("mmnormalize: turbo worker context ready\n");
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
    if (pData->ctxln != NULL) ln_exitCtx(pData->ctxln);
    if (pData->ctxlnLockInitialized) pthread_rwlock_destroy(&pData->ctxlnLock);
    if (pData->mutReloadGenerationInitialized) pthread_mutex_destroy(&pData->mutReloadGeneration);
    free(pData->rulebaseForReload);
    free(pData->ruleForReload);
    free(pData->pszPath);
    msgPropDescrDestruct(pData->varDescr);
    free(pData->varDescr);
    if (pData->debugFile != NULL) fclose(pData->debugFile);
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
    dbgprintf("\tdebug='%d'\n", pData->bDebug);
#ifdef HAVE_LOGNORM_TURBO
    dbgprintf("\tturbo='%d'\n", pData->bTurbo);
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
        /* Read the field through the public opaque accessor instead of
         * reaching into the result struct, so mmnormalize depends only on
         * lognorm-turbo.h and not on the internal fast-result layout. */
        const char *fname = NULL;
        size_t fname_len = 0;
        unsigned ftype = 0;
        const char *sval = NULL;
        size_t slen = 0;
        int64_t ival = 0;
        double dval = 0;
        struct json_object *jval = NULL;

        /* flags out-param is NULL: nesting is detected via memchr below,
         * which does not rely on the LN_FFIELD_NESTED flag being set. */
        if (ln_fast_result_get_field_typed(result, i, &fname, &fname_len, &ftype, NULL, &sval, &slen, &ival, &dval) !=
            0)
            continue;

        switch (ftype) {
            case LN_FTYPE_STRING:
            case LN_FTYPE_STRING_INLINE:
                jval = json_object_new_string_len(sval, (int)slen);
                break;
            case LN_FTYPE_INT:
                jval = json_object_new_int64(ival);
                break;
            case LN_FTYPE_DOUBLE:
                jval = json_object_new_double(dval);
                break;
            case LN_FTYPE_BOOL:
                jval = json_object_new_boolean((json_bool)ival);
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
        if (fname_len < MMNORM_MAX_FIELDNAME) {
            memcpy(namebuf, fname, fname_len);
            namebuf[fname_len] = '\0';
            name_ptr = namebuf;
        } else {
            heap_name = (char *)malloc(fname_len + 1);
            if (heap_name == NULL) {
                /* OOM: drop the field rather than truncate.  json_object
                 * created above must be released to avoid leaking. */
                json_object_put(jval);
                continue;
            }
            memcpy(heap_name, fname, fname_len);
            heap_name[fname_len] = '\0';
            name_ptr = heap_name;
        }

        /* Handle nested fields (dotted names).
         * Detect dots directly -- LN_FFIELD_NESTED flag is not
         * always set by liblognorm rule compilation. */
        if (memchr(fname, '.', fname_len) != NULL) {
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

#ifdef HAVE_LOGNORM_TURBO
/* Adopt a pending HUP generation at a worker-owned safe point. Building the
 * replacement before swapping keeps the previous context usable during
 * compilation. Both the swap and reclamation occur on the owning worker. */
static void refreshTurboContext(wrkrInstanceData_t *const pWrkrData) {
    instanceData *const pData = pWrkrData->pData;
    const unsigned targetGeneration = ATOMIC_LOAD_32BIT_unsigned(&pData->reloadGeneration, &pData->mutReloadGeneration);
    ln_ctx newCtx;
    ln_ctx oldCtx;

    if (!pData->bTurbo || pWrkrData->reloadGeneration == targetGeneration) return;

    newCtx = buildTurboWorkerContext(pData);
    oldCtx = pWrkrData->ctxlnTurbo;
    pWrkrData->ctxlnTurbo = newCtx;
    pWrkrData->reloadGeneration = targetGeneration;
    if (oldCtx != NULL) ln_exitCtx(oldCtx);

    if (newCtx == NULL) {
        LogError(0, RS_RET_ERR_LIBLOGNORM_SAMPDB_LOAD,
                 "mmnormalize: HUP turbo worker context build failed, "
                 "falling back to standard normalization");
    } else {
        DBGPRINTF("mmnormalize: turbo worker adopted HUP generation %u\n", targetGeneration);
    }
}
#endif


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
    refreshTurboContext(pWrkrData);
    if (pWrkrData->ctxlnTurbo != NULL) {
        const sbool isCeeRoot = pWrkrData->pData->pszPath[0] == '$' && pWrkrData->pData->pszPath[1] == '!' &&
                                pWrkrData->pData->pszPath[2] == '\0';
        const ln_fast_result_t *result = NULL;
        r = ln_turbo_normalize_raw(pWrkrData->ctxlnTurbo, (char *)buf, len, &result);
        if (r == 0 && result != NULL) {
            /* SNAPSHOT PATH: when path is "$!" (CEE root).
             * Create a deep-copy snapshot of the turbo result.
             * The snapshot is a single allocation (~6KB) that owns
             * all string data -- safe for async output actions and
             * non-DIRECT queues. The lazy materializer builds json
             * on-demand only when templates access $! or $!field. */
            if (isCeeRoot) {
                ln_fast_result_snapshot_t *snap = ln_turbo_snapshot_result(pWrkrData->ctxlnTurbo);
                if (snap == NULL) {
                    DBGPRINTF(
                        "mmnormalize: turbo snapshot alloc failed, "
                        "trying JSON materialization\n");
                    json = fast_result_to_json(result);
                    if (json != NULL) goto add_json;
                    DBGPRINTF(
                        "mmnormalize: turbo JSON materialization failed, "
                        "falling back to standard normalization\n");
                } else {
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
            }
            /* Non-$! paths cannot retain a snapshot, so build JSON directly.
             * A failed $! snapshot/materialization above deliberately falls
             * through to standard normalization rather than retrying Turbo. */
            if (!isCeeRoot) {
                json = fast_result_to_json(result);
                if (json != NULL) goto add_json;
            }
        } else {
            DBGPRINTF(
                "mmnormalize: turbo normalize failed (r=%d), "
                "falling back to standard\n",
                r);
        }
    }
#endif

    /* STANDARD PATH: original ln_normalize (fallback or non-turbo) */
    pthread_rwlock_rdlock(&pWrkrData->pData->ctxlnLock);
    r = ln_normalize(pWrkrData->pData->ctxln, (char *)buf, len, &json);
    pthread_rwlock_unlock(&pWrkrData->pData->ctxlnLock);

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
    pData->rulebaseForReload = NULL;
    pData->ruleForReload = NULL;
    pData->ctxOpts = 0;
    pData->bDebug = 0;
    pData->debugFile = NULL;
    pData->pszPath = strdup("$!");
    pData->varDescr = NULL;
#ifdef HAVE_LOGNORM_TURBO
    pData->bTurbo = 0;
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
    char *debugFileName = NULL;
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
            CHKmalloc(pData->rulebase = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
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
            CHKmalloc(varName = es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(actpblk.descr[i].name, "debug")) {
            pData->bDebug = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "debugfile")) {
            CHKmalloc(debugFileName = es_str2cstr(pvals[i].val.d.estr, NULL));
#ifdef HAVE_LOGNORM_TURBO
        } else if (!strcmp(actpblk.descr[i].name, "turbo")) {
            pData->bTurbo = (int)pvals[i].val.d.n;
#endif
        } else if (!strcmp(actpblk.descr[i].name, "path")) {
            CHKmalloc(cstr = es_str2cstr(pvals[i].val.d.estr, NULL));
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
    if (debugFileName != NULL) {
        if (!pData->bDebug) {
            LogError(0, RS_RET_CONFIG_ERROR, "mmnormalize: 'debugFile' requires 'debug=on'");
            ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
        }
        pData->debugFile = fopen(debugFileName, "a");
        if (pData->debugFile == NULL) {
            LogError(errno, RS_RET_CONFIG_ERROR, "mmnormalize: could not open debug file '%s'", debugFileName);
            ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
        }
        free(debugFileName);
        debugFileName = NULL;
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
    free(debugFileName);
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

BEGINdoHUP
    ln_ctx newCtx;
    ln_ctx oldCtx;
    sbool turboAvailable = 0;
    sbool enableTurbo = 0;
    CODESTARTdoHUP;
    DBGPRINTF("mmnormalize: HUP received\n");
#ifdef HAVE_LOGNORM_TURBO
    enableTurbo = pData->bTurbo;
#endif
    newCtx = buildContext(pData, enableTurbo, &turboAvailable);
    if (newCtx == NULL) {
        LogError(0, RS_RET_ERR_LIBLOGNORM_SAMPDB_LOAD,
                 "mmnormalize: HUP rulebase reload failed, "
                 "keeping the previous context");
        ABORT_FINALIZE(RS_RET_ERR_LIBLOGNORM_SAMPDB_LOAD);
    }

    pthread_rwlock_wrlock(&pData->ctxlnLock);
    oldCtx = pData->ctxln;
    pData->ctxln = newCtx;
    pthread_rwlock_unlock(&pData->ctxlnLock);
    ln_exitCtx(oldCtx);

#ifdef HAVE_LOGNORM_TURBO
    if (pData->bTurbo) {
        const unsigned generation =
            ATOMIC_INC_AND_FETCH_unsigned(&pData->reloadGeneration, &pData->mutReloadGeneration);
        if (!turboAvailable) {
            LogMsg(0, NO_ERRCODE, LOG_WARNING,
                   "mmnormalize: HUP reloaded the standard rulebase, "
                   "but Turbo compilation is unavailable");
        }
        DBGPRINTF("mmnormalize: published HUP generation %u for Turbo workers\n", generation);
    }
#else
    (void)turboAvailable;
#endif
    LogMsg(0, RS_RET_OK, LOG_INFO, "mmnormalize: rulebase reloaded");
finalize_it:
ENDdoHUP

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
    CODEqueryEtryPt_doHUP
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
