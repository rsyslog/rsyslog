#include "config.h"

#include <stdint.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <string.h>  // for memset
#include <memory.h>
#include <ctype.h>
#include <limits.h>

#include <libestr.h>

/* Define DEBUG_FPRINTF to enable debug output, otherwise no debug output */
#ifdef DEBUG_FPRINTF_OUTPUT
    #define FPRINTF_DEBUG(...) fprintf(__VA_ARGS__)
#else
    #define FPRINTF_DEBUG(...)
#endif

#include "rsyslog.h"
#include "obj.h"
#include "msg.h"
#include "modules.h"
#include "parser.h"
#include "var.h"
#include "wti.h"
#include "batch.h"
#include "queue.h"
#include "ratelimit.h"
#include "linkedlist.h"
#include "ruleset.h"
#include "action.h"
#include "template.h"
#include "rainerscript.h"
#include "iminternal.h"
#include "errmsg.h"
#include "threads.h"
#include "dnscache.h"
#include "prop.h"
#include "unicode-helper.h"
#include "net.h"
#include "tcpsrv.h"
#include "tcps_sess.h"
#include "glbl.h"
#include "debug.h"
#include "srUtils.h"
#include "rsconf.h"
#include "cfsysline.h"
#include "datetime.h"
#include "operatingstate.h"
#include "dirty.h"
#include "janitor.h"
#include "typedefs.h"
#include <json_tokener.h>
#include "fuzzapi.h"
#include "pmrfc3164.h"
#include "pmrfc5424.h"
#include "omfile.h"
#include "ompipe.h"
#include "omshell.h"
#include "omdiscard.h"
#include "omfwd.h"
#include "omusrmsg.h"
#include "smfile.h"
#include "smtradfile.h"
#include "smfwd.h"
#include "smtradfwd.h"
#include "iminternal.h"

/* Stub AFL/coverage globals to satisfy objects possibly built with afl-clang-fast */
#ifndef __AFL_COMPILER
unsigned char *__afl_area_ptr __attribute__((weak)) = (unsigned char *)0;
void __sanitizer_cov_trace_pc_guard_init(uint32_t *start, uint32_t *stop) __attribute__((weak));
void __sanitizer_cov_trace_pc_guard(uint32_t *guard) __attribute__((weak));
void __sanitizer_cov_trace_pc_guard_init(uint32_t *start, uint32_t *stop) {
    (void)start;
    (void)stop;
}
void __sanitizer_cov_trace_pc_guard(uint32_t *guard) {
    (void)guard;
}
#endif

rsRetVal qqueueSetSpoolDir(qqueue_t *pThis, uchar *pszSpoolDir, int lenSpoolDir);
#ifdef FUZZ_HAVE_OMRELP
rsRetVal modInit(int iIFVersRequested,
                 int *ipIFVersProvided,
                 rsRetVal (**pQueryEtryPt)(),
                 rsRetVal (*pHostQueryEtryPt)(uchar *, rsRetVal (**)()),
                 modInfo_t *pModInfo);
#endif

/* Function forward declarations */
void rsyslogd_submitErrMsg(const int severity, const int iErr, const uchar *msg);
rsRetVal rsyslogd_InitGlobalClasses(void);

/* Define object interfaces similar to rsyslogd.c */
DEFobjCurrIf(obj) DEFobjCurrIf(prop) DEFobjCurrIf(parser) DEFobjCurrIf(ruleset) DEFobjCurrIf(rsconf)
    DEFobjCurrIf(module) DEFobjCurrIf(datetime) DEFobjCurrIf(glbl) DEFobjCurrIf(net) DEFobjCurrIf(tcps_sess)

    /* Global variables needed by rsyslog runtime */
    prop_t *pInputName = NULL; /* input name property */

qqueue_t *pMsgQueue = NULL; /* message queue */
rsconf_t *ourConf = NULL; /* configuration */
ratelimit_t *dflt_ratelimiter = NULL; /* default ratelimiter for submissions */
ratelimit_t *internalMsg_ratelimiter = NULL; /* ratelimiter for internal messages */
int bHaveMainQueue = 0; /* indicates if main queue is available */
int iConfigVerify = 0; /* config verification flag */
int MarkInterval = 20 * 60; /* interval between marks in seconds - default 20 minutes */
prop_t *pInternalInputName = NULL; /* internal input name for rsyslogd error paths */
int send_to_all = 0; /* forward to all addresses flag used by omfwd */

static int g_conf_ready = 0;
static struct template *g_fuzz_template = NULL;
static ruleset_t *g_default_ruleset = NULL;
static struct cnfstmt *g_discard_stmt = NULL;
static wti_t *g_dummy_wti = NULL;
static int g_dummy_shutdown_flag = 0;
static int g_builtin_templates_ready = 0;
static int g_parser_ready = 0;
typedef struct parse_result_s {
    rsRetVal rc;
} parse_result_t;
static rsRetVal register_builtin_module(rsRetVal (*modInit)(), const char *name);
static void register_basic_cfsysline_handlers(void);
static void ensure_builtin_templates(void);
static rsRetVal ensure_default_ruleset(void);
static struct template *ensure_default_template(void);
static struct nvlst *make_kv_pair(const char *name, const char *value);
static int append_kv_pair(struct nvlst **head, struct nvlst **tail, const char *name, const char *value);
static struct nvlst *make_action_kvlist(const char *type, const char *file);
static rsRetVal ensure_discard_action(void);
static wti_t *ensure_worker(void);
static rsRetVal ensure_conf_initialized(void);
static void reset_runtime_conf_pointers(void);
static parse_result_t parse_message_with_named_parser(smsg_t *pMsg, const uchar *parserName);

/* Stubs to avoid full rsyslogd main queue/runtime dependencies */
rsRetVal logmsgInternal(int iErr, syslog_pri_t pri, const uchar *msg, int flags) {
    (void)iErr;
    (void)pri;
    (void)msg;
    (void)flags;
    return RS_RET_OK;
}

rsRetVal submitMsg2(smsg_t *pMsg) {
    msgDestruct(&pMsg);
    return RS_RET_OK;
}

rsRetVal multiSubmitMsg2(multi_submit_t *const pMultiSub) {
    (void)pMultiSub;
    return RS_RET_OK;
}

rsRetVal createMainQueue(qqueue_t **ppQueue, uchar *pszQueueName, struct nvlst *lst) {
    (void)ppQueue;
    (void)pszQueueName;
    (void)lst;
    return RS_RET_OK;
}

rsRetVal startMainQueue(rsconf_t *cnf, qqueue_t *pQueue) {
    (void)cnf;
    (void)pQueue;
    return RS_RET_OK;
}

#ifndef _PATH_MODDIR
    #error "_PATH_MODDIR must be defined to an absolute in-tree runtime module directory"
#endif

void rsyslogd_submitErrMsg(const int severity, const int iErr, const uchar *msg) {
    (void)severity;
    (void)iErr;
    (void)msg;
}

rsRetVal queryLocalHostname(rsconf_t *const cnf) {
    (void)cnf;
    return RS_RET_OK;
}

/* Full initialization that mirrors rsyslogd.c initialization sequence */
rsRetVal rsyslogd_InitGlobalClasses(void) {
    FPRINTF_DEBUG(stderr, "Starting diagnostic rsyslog initialization for fuzzing...\n");

    DEFiRet;
    const char *pErrObj = NULL; /* tells us which object failed if that happens (useful for troubleshooting!) */
    char errStr[1024];
    (void)errStr; /* silence unused when debug logging is disabled */

    /* Initialize the runtime system - like rsyslogd.c */
    pErrObj = "rsyslog runtime"; /* set in case the runtime errors before setting an object */
    iRet = rsrtInit(&pErrObj, &obj);
    if (iRet != RS_RET_OK) {
        fprintf(stderr, "Init FAIL: rsrtInit [err object: %s] → %d (%s)\n", (pErrObj ? pErrObj : "unknown"), iRet,
                rs_strerror_r(iRet, errStr, sizeof(errStr)));
        return iRet;
    }
    rsrtSetErrLogger(rsyslogd_submitErrMsg);
    FPRINTF_DEBUG(stderr, "Init OK: rsyslog runtime\n");

    /* Now tell the system which classes we need ourselfs - like rsyslogd.c */
    pErrObj = "glbl";
    iRet = objUse(glbl, CORE_COMPONENT);
    if (iRet != RS_RET_OK) {
        FPRINTF_DEBUG(stderr, "Init FAIL: objUse (glbl) [err object: %s] → %d (%s)\n", pErrObj, iRet,
                      rs_strerror_r(iRet, errStr, sizeof(errStr)));
        return iRet;
    }
    FPRINTF_DEBUG(stderr, "Init OK: glbl\n");

    pErrObj = "module";
    iRet = objUse(module, CORE_COMPONENT);
    if (iRet != RS_RET_OK) {
        FPRINTF_DEBUG(stderr, "Init FAIL: objUse (module) [err object: %s] → %d (%s)\n", pErrObj, iRet,
                      rs_strerror_r(iRet, errStr, sizeof(errStr)));
        return iRet;
    }
    FPRINTF_DEBUG(stderr, "Init OK: module\n");
    iRet = module.SetModDir((uchar *)_PATH_MODDIR);
    if (iRet != RS_RET_OK) {
        FPRINTF_DEBUG(stderr, "Init FAIL: module.SetModDir(%s) → %d (%s)\n", _PATH_MODDIR, iRet,
                      rs_strerror_r(iRet, errStr, sizeof(errStr)));
    }

    pErrObj = "datetime";
    iRet = objUse(datetime, CORE_COMPONENT);
    if (iRet != RS_RET_OK) {
        FPRINTF_DEBUG(stderr, "Init FAIL: objUse (datetime) [err object: %s] → %d (%s)\n", pErrObj, iRet,
                      rs_strerror_r(iRet, errStr, sizeof(errStr)));
        return iRet;
    }
    FPRINTF_DEBUG(stderr, "Init OK: datetime\n");

    pErrObj = "ruleset";
    iRet = objUse(ruleset, CORE_COMPONENT);
    if (iRet != RS_RET_OK) {
        FPRINTF_DEBUG(stderr, "Init FAIL: objUse (ruleset) [err object: %s] → %d (%s)\n", pErrObj, iRet,
                      rs_strerror_r(iRet, errStr, sizeof(errStr)));
        return iRet;
    }
    FPRINTF_DEBUG(stderr, "Init OK: ruleset\n");

    pErrObj = "prop";
    iRet = objUse(prop, CORE_COMPONENT);
    if (iRet != RS_RET_OK) {
        FPRINTF_DEBUG(stderr, "Init FAIL: objUse (prop) [err object: %s] → %d (%s)\n", pErrObj, iRet,
                      rs_strerror_r(iRet, errStr, sizeof(errStr)));
        return iRet;
    }
    FPRINTF_DEBUG(stderr, "Init OK: prop\n");

    pErrObj = "parser";
    iRet = objUse(parser, CORE_COMPONENT);
    if (iRet != RS_RET_OK) {
        FPRINTF_DEBUG(stderr, "Init FAIL: objUse (parser) [err object: %s] → %d (%s)\n", pErrObj, iRet,
                      rs_strerror_r(iRet, errStr, sizeof(errStr)));
        return iRet;
    }
    FPRINTF_DEBUG(stderr, "Init OK: parser\n");

    pErrObj = "rsconf";
    iRet = objUse(rsconf, CORE_COMPONENT);
    if (iRet != RS_RET_OK) {
        FPRINTF_DEBUG(stderr, "Init FAIL: objUse (rsconf) [err object: %s] → %d (%s)\n", pErrObj, iRet,
                      rs_strerror_r(iRet, errStr, sizeof(errStr)));
        return iRet;
    }
    FPRINTF_DEBUG(stderr, "Init OK: rsconf\n");

    /* intialize some dummy classes that are not part of the runtime - like rsyslogd.c */
    pErrObj = "action";
    iRet = actionClassInit();
    if (iRet != RS_RET_OK) {
        FPRINTF_DEBUG(stderr, "Init FAIL: actionClassInit [err object: %s] → %d (%s)\n", pErrObj, iRet,
                      rs_strerror_r(iRet, errStr, sizeof(errStr)));
        return iRet;
    }
    FPRINTF_DEBUG(stderr, "Init OK: action class\n");

    pErrObj = "template";
    iRet = templateInit();
    if (iRet != RS_RET_OK) {
        FPRINTF_DEBUG(stderr, "Init FAIL: templateInit [err object: %s] → %d (%s)\n", pErrObj, iRet,
                      rs_strerror_r(iRet, errStr, sizeof(errStr)));
        return iRet;
    }
    FPRINTF_DEBUG(stderr, "Init OK: template class\n");

    /* TODO: the dependency on net shall go away! -- rgerhards, 2008-03-07 */
    pErrObj = "net";
    iRet = objUse(net, LM_NET_FILENAME);
    if (iRet != RS_RET_OK) {
        FPRINTF_DEBUG(stderr, "Init FAIL: objUse (net) [err object: %s] → %d (%s)\n", pErrObj, iRet,
                      rs_strerror_r(iRet, errStr, sizeof(errStr)));
        return iRet;
    }
    FPRINTF_DEBUG(stderr, "Init OK: net (lmnet)\n");

    /* we need to create the inputName property (only once during our lifetime) - like rsyslogd.c */
    if ((iRet = prop.Construct(&pInputName)) != RS_RET_OK) {
        FPRINTF_DEBUG(stderr, "Init FAIL: prop.Construct (inputName) → %d (%s)\n", iRet,
                      rs_strerror_r(iRet, errStr, sizeof(errStr)));
        return iRet;
    }
    if ((iRet = prop.SetString(pInputName, UCHAR_CONSTANT("rsyslogd"), sizeof("rsyslogd") - 1)) != RS_RET_OK) {
        FPRINTF_DEBUG(stderr, "Init FAIL: prop.SetString (inputName) → %d (%s)\n", iRet,
                      rs_strerror_r(iRet, errStr, sizeof(errStr)));
        return iRet;
    }
    if ((iRet = prop.ConstructFinalize(pInputName)) != RS_RET_OK) {
        FPRINTF_DEBUG(stderr, "Init FAIL: prop.ConstructFinalize (inputName) → %d (%s)\n", iRet,
                      rs_strerror_r(iRet, errStr, sizeof(errStr)));
        return iRet;
    } else {
        FPRINTF_DEBUG(stderr, "Init OK: inputName property\n");
    }

    /* internal input name mirrors rsyslogd.c for internal messages */
    if ((iRet = prop.Construct(&pInternalInputName)) != RS_RET_OK) {
        FPRINTF_DEBUG(stderr, "Init FAIL: prop.Construct (internalInputName) → %d (%s)\n", iRet,
                      rs_strerror_r(iRet, errStr, sizeof(errStr)));
        return iRet;
    }
    if ((iRet = prop.SetString(pInternalInputName, UCHAR_CONSTANT("rsyslogd"), sizeof("rsyslogd") - 1)) != RS_RET_OK) {
        FPRINTF_DEBUG(stderr, "Init FAIL: prop.SetString (internalInputName) → %d (%s)\n", iRet,
                      rs_strerror_r(iRet, errStr, sizeof(errStr)));
        return iRet;
    }
    if ((iRet = prop.ConstructFinalize(pInternalInputName)) != RS_RET_OK) {
        FPRINTF_DEBUG(stderr, "Init FAIL: prop.ConstructFinalize (internalInputName) → %d (%s)\n", iRet,
                      rs_strerror_r(iRet, errStr, sizeof(errStr)));
        return iRet;
    }

    /* Additional initialization like rsyslogd.c */
    dnscacheInit();
    initRainerscript();
    ratelimitModInit();

    FPRINTF_DEBUG(stderr, "Initialization complete.\n");
    return RS_RET_OK;
}

/* Minimal initialization that mirrors rsyslogd.c initialization sequence */
rsRetVal rs_fuzz_init(void) {
    static int initialized = 0;
    static rsRetVal cached = RS_RET_OK;
    if (initialized) {
        return cached;
    }
    initialized = 1;

    /* Force module search path to the freshly built tree so dlopen() pulls in local modules. */
    (void)setenv("RSYSLOG_MODDIR", _PATH_MODDIR, 1);
    glblModPath = (uchar *)_PATH_MODDIR;

    rsRetVal ret = rsyslogd_InitGlobalClasses();
    if (ret == RS_RET_OK) {
        ret = ensure_conf_initialized();
    }
    if (ret != RS_RET_OK) {
        fprintf(stderr, "rsyslog fuzz runtime initialization failed: %d\n", ret);
        cached = ret;
        return cached;
    }

    rsRetVal aux = modInitIminternal();
    if (aux != RS_RET_OK) {
        fprintf(stderr, "warning: could not initialize iminternal (error code %d) - may affect fuzzing\n", aux);
        if (ret == RS_RET_OK) {
            ret = aux;
        }
    }

    aux = queryLocalHostname(ourConf);
    if (aux != RS_RET_OK) {
        fprintf(stderr, "Warning: queryLocalHostname failed: %d, continuing for fuzzing\n", aux);
        if (ret == RS_RET_OK) {
            ret = aux;
        }
    }

    cached = ret;
    return cached;
}

static rsRetVal register_builtin_module(rsRetVal (*modInit)(), const char *name) {
    if (modInit == NULL || !module.ifIsLoaded) {
        return RS_RET_INVALID_VALUE;
    }
    modInfo_t *pMod = NULL;
    rsRetVal iRet = module.doModInit(modInit, (uchar *)name, NULL, &pMod);
    if (iRet != RS_RET_OK || pMod == NULL) {
        return iRet;
    }
    if (pMod->cnfName == NULL) {
        const char *cnfName = name;
        if (!strncmp(name, "builtin:", sizeof("builtin:") - 1) && strlen(name) > sizeof("builtin:") - 1) {
            cnfName = name + (sizeof("builtin:") - 1);
        }
        pMod->cnfName = (uchar *)strdup(cnfName);
    }
    if (loadConf != NULL) {
        cfgmodules_etry_t *pNew = NULL;
        cfgmodules_etry_t *pLast = NULL;
        rsRetVal readyRet = readyModForCnf(pMod, &pNew, &pLast);
        if (readyRet == RS_RET_OK && pNew != NULL) {
            addModToCnfList(&pNew, pLast);
        } else if (pNew != NULL) {
            free(pNew);
        } else if (readyRet != RS_RET_OK) {
            fprintf(stderr, "Warning: readyModForCnf failed for %s (%d)\n", name, readyRet);
        }
    }
    return RS_RET_OK;
}

static void register_basic_cfsysline_handlers(void) {
    static int handlers_registered = 0;
    if (handlers_registered || loadConf == NULL) {
        return;
    }
    (void)regCfSysLineHdlr((uchar *)"mainmsgqueuesize", 0, eCmdHdlrInt, NULL,
                           &loadConf->globals.mainQ.iMainMsgQueueSize, NULL);
    (void)regCfSysLineHdlr((uchar *)"mainmsgqueuefilename", 0, eCmdHdlrGetWord, NULL,
                           &loadConf->globals.mainQ.pszMainMsgQFName, NULL);
    (void)regCfSysLineHdlr((uchar *)"debugprinttemplatelist", 0, eCmdHdlrBinary, NULL,
                           &loadConf->globals.bDebugPrintTemplateList, NULL);
    (void)regCfSysLineHdlr((uchar *)"privdroptouserid", 0, eCmdHdlrInt, NULL, &loadConf->globals.uidDropPriv, NULL);
    (void)regCfSysLineHdlr((uchar *)"privdroptogroupid", 0, eCmdHdlrInt, NULL, &loadConf->globals.gidDropPriv, NULL);
    handlers_registered = 1;
}

static void ensure_builtin_templates(void) {
    if (g_builtin_templates_ready || ourConf == NULL) {
        return;
    }
    static uchar tpl_file[] = "\"%TIMESTAMP% %HOSTNAME% %syslogtag%%msg:::sp-if-no-1st-sp%%msg%\\n\"";
    static uchar tpl_trad_file[] = "\"%timegenerated% %HOSTNAME% %syslogtag%%msg:::sp-if-no-1st-sp%%msg%\\n\"";
    static uchar tpl_fwd[] = "=RSYSLOG_ForwardFormat";
    static uchar tpl_trad_fwd[] = "=RSYSLOG_TraditionalForwardFormat";
    static uchar tpl_fuzz_default[] = "\"%TIMESTAMP% %HOSTNAME% %msg%\"";
    uchar *pTpl = tpl_file;
    tplAddLine(ourConf, "RSYSLOG_FileFormat", &pTpl);
    pTpl = tpl_trad_file;
    tplAddLine(ourConf, "RSYSLOG_TraditionalFileFormat", &pTpl);
    pTpl = tpl_fwd;
    tplAddLine(ourConf, "RSYSLOG_ForwardFormat", &pTpl);
    pTpl = tpl_trad_fwd;
    tplAddLine(ourConf, "RSYSLOG_TraditionalForwardFormat", &pTpl);
    pTpl = tpl_fuzz_default;
    g_fuzz_template = tplAddLine(ourConf, "FUZZ_DefaultTemplate", &pTpl);
    tplLastStaticInit(ourConf, ourConf->templates.last);
    g_builtin_templates_ready = 1;
}

static rsRetVal ensure_default_ruleset(void) {
    DEFiRet;
    ruleset_t *pRuleset = NULL;

    if (g_default_ruleset != NULL || ourConf == NULL) {
        RETiRet;
    }

    CHKiRet(ruleset.Construct(&pRuleset));
    CHKiRet(ruleset.SetName(pRuleset, UCHAR_CONSTANT("RSYSLOG_DefaultRuleset")));
    CHKiRet(ruleset.ConstructFinalize(ourConf, pRuleset));
    rulesetSetCurrRulesetPtr(pRuleset);
    if (ruleset.SetDefaultRuleset != NULL) {
        CHKiRet(ruleset.SetDefaultRuleset(ourConf, (uchar *)"RSYSLOG_DefaultRuleset"));
    }
    g_default_ruleset = pRuleset;
    pRuleset = NULL;

finalize_it:
    if (pRuleset != NULL && g_default_ruleset != pRuleset) {
        ruleset.Destruct(&pRuleset);
    }
    RETiRet;
}

static struct template *ensure_default_template(void) {
    if (g_fuzz_template != NULL || ourConf == NULL) {
        return g_fuzz_template;
    }
    static uchar tplDef[] = "\"%TIMESTAMP% %HOSTNAME% %msg%\"";
    uchar *pTpl = tplDef;
    g_fuzz_template = tplAddLine(ourConf, "FUZZ_DefaultTemplate", &pTpl);
    tplLastStaticInit(ourConf, ourConf->templates.last);
    return g_fuzz_template;
}

static struct nvlst *make_kv_pair(const char *name, const char *value) {
    es_str_t *nm = es_newStrFromCStr(name, strlen(name));
    es_str_t *val = es_newStrFromCStr(value, strlen(value));
    if (nm == NULL || val == NULL) {
        if (nm != NULL) {
            es_deleteStr(nm);
        }
        if (val != NULL) {
            es_deleteStr(val);
        }
        return NULL;
    }
    struct nvlst *lst = nvlstNewStr(val);
    if (lst == NULL) {
        es_deleteStr(nm);
        return NULL;
    }
    return nvlstSetName(lst, nm);
}

static int append_kv_pair(struct nvlst **head, struct nvlst **tail, const char *name, const char *value) {
    if (head == NULL || tail == NULL) {
        return -1;
    }
    struct nvlst *node = make_kv_pair(name, value);
    if (node == NULL) {
        return -1;
    }
    if (*head == NULL) {
        *head = node;
    } else {
        (*tail)->next = node;
    }
    *tail = node;
    return 0;
}

static struct nvlst *make_action_kvlist(const char *type, const char *file) {
    struct nvlst *head = NULL;
    struct nvlst *tail = NULL;
    if (append_kv_pair(&head, &tail, "type", type) != 0) {
        return NULL;
    }
    if (file != NULL) {
        if (append_kv_pair(&head, &tail, "file", file) != 0) {
            nvlstDestruct(head);
            return NULL;
        }
    }
    return head;
}

static rsRetVal ensure_discard_action(void) {
    DEFiRet;
    struct nvlst *lst = NULL;
    struct cnfstmt *stmt = NULL;
    action_t *pAction = NULL;
    rsRetVal actRet;
    const char *actionType = "omdiscard";

    if (!module.ifIsLoaded || module.FindWithCnfName == NULL || module.doModInit == NULL) {
        ABORT_FINALIZE(RS_RET_NOT_FOUND);
    }

    if (g_discard_stmt != NULL) {
        RETiRet;
    }

    CHKiRet(ensure_default_ruleset());
    if ((lst = make_action_kvlist("omdiscard", NULL)) == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }
    actRet = actionNewInst(lst, &pAction);
    if (actRet == RS_RET_CONFOBJ_UNSUPPORTED) {
        nvlstDestruct(lst);
        lst = make_action_kvlist("omfile", "/dev/null");
        if (lst == NULL) {
            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        }
        actionType = "omfile";
        actRet = actionNewInst(lst, &pAction);
    }
    if (actRet != RS_RET_OK) {
        fprintf(stderr, "ensure_discard_action: actionNewInst(%s) failed with %d\n", actionType, actRet);
        CHKiRet(actRet);
    }
    CHKiRet(actRet);

    if ((stmt = cnfstmtNew(S_ACT)) == NULL) {
        actionDestruct(pAction);
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }
    stmt->d.act = pAction;
    stmt->printable = (uchar *)strdup("action(type=\"omdiscard\")");
    ruleset.AddScript(g_default_ruleset, stmt);
    g_discard_stmt = stmt;

finalize_it:
    if (lst != NULL) {
        nvlstDestruct(lst);
    }
    RETiRet;
}

static wti_t *ensure_worker(void) {
    if (g_dummy_wti != NULL) {
        return g_dummy_wti;
    }
    if (ensure_discard_action() != RS_RET_OK) {
        return NULL;
    }
    g_dummy_wti = wtiGetDummy();
    if (g_dummy_wti != NULL) {
        g_dummy_shutdown_flag = 0;
        g_dummy_wti->pbShutdownImmediate = &g_dummy_shutdown_flag;
    }
    return g_dummy_wti;
}

static void reset_runtime_conf_pointers(void) {
    loadConf = ourConf;
    runConf = ourConf;
    if (ourConf != NULL && g_default_ruleset != NULL) {
        ourConf->rulesets.pDflt = g_default_ruleset;
        rulesetSetCurrRulesetPtr(g_default_ruleset);
    }
}

static rsRetVal ensure_conf_initialized(void) {
    DEFiRet;
    int step = 0;
    if (g_conf_ready) {
        reset_runtime_conf_pointers();
        RETiRet;
    }

    if (ourConf == NULL) {
        step = 1;
        CHKiRet(rsconfConstruct(&ourConf));
    }
    loadConf = ourConf;
    runConf = ourConf;

    static const struct {
        rsRetVal (*initFn)();
        const char *name;
    } builtinMods[] = {{modInitFile, "builtin:omfile"},
                       {modInitPipe, "builtin:ompipe"},
                       {modInitShell, "builtin-shell"},
                       {modInitDiscard, "builtin:omdiscard"},
#if defined(SYSLOG_INET) && !defined(FUZZING_BUILD)
                       {modInitFwd, "builtin:omfwd"},
#endif
                       {modInitUsrMsg, "builtin:omusrmsg"},
                       {modInitpmrfc5424, "builtin:pmrfc5424"},
                       {modInitpmrfc3164, "builtin:pmrfc3164"},
#ifdef FUZZ_HAVE_OMRELP
                       {modInit, "builtin:omrelp"},
#endif
                       {modInitsmfile, "builtin:smfile"},
                       {modInitsmtradfile, "builtin:smtradfile"},
                       {modInitsmfwd, "builtin:smfwd"},
                       {modInitsmtradfwd, "builtin:smtradfwd"}};
    for (size_t i = 0; i < sizeof(builtinMods) / sizeof(builtinMods[0]); ++i) {
        const rsRetVal modRet = register_builtin_module(builtinMods[i].initFn, builtinMods[i].name);
        if (modRet != RS_RET_OK) {
            fprintf(stderr, "Warning: failed to register module %s (%d)\n", builtinMods[i].name, modRet);
        }
    }

    if (parser.ifIsLoaded && parser.AddDfltParser != NULL) {
        rsRetVal pRet5424 = parser.AddDfltParser((uchar *)"rsyslog.rfc5424");
        rsRetVal pRet3164 = parser.AddDfltParser((uchar *)"rsyslog.rfc3164");
        if (pRet5424 != RS_RET_OK || pRet3164 != RS_RET_OK) {
            fprintf(stderr, "Warning: default parser registration failed (rfc5424=%d, rfc3164=%d)\n", pRet5424,
                    pRet3164);
        } else {
            FPRINTF_DEBUG(stderr, "Init OK: default parsers (rfc5424, rfc3164) registered\n");
            g_parser_ready = 1;
        }
    }

    register_basic_cfsysline_handlers();
    ensure_builtin_templates();
    step = 2;
    CHKiRet(ensure_default_ruleset());
    step = 3;
    CHKiRet(ensure_discard_action());
    (void)ensure_default_template();
    if (!tcps_sess.ifIsLoaded) {
        (void)objUse(tcps_sess, LM_TCPSRV_FILENAME);
    }

    if (rsconf.ifIsLoaded && rsconf.Activate != NULL) {
        (void)rsconf.Activate(ourConf);
        reset_runtime_conf_pointers();
    }

    reset_runtime_conf_pointers();
    g_conf_ready = 1;

finalize_it:
    (void)step;
#ifdef FUZZING_BUILD
    if (iRet != RS_RET_OK) {
        fprintf(stderr, "ensure_conf_initialized failed at step %d (%d)\n", step, iRet);
    }
#endif
    RETiRet;
}

/* Additional stub implementations needed for linking */
/* submitMsg2, createMainQueue, startMainQueue, logmsgInternal are provided by rsyslogd */

/* Function to extract and print parsed message properties */
static void dump_parsed_message(smsg_t *pMsg, parse_result_t res) {
#ifndef DEBUG_FPRINTF_OUTPUT
    (void)pMsg;
    (void)res;
#else
    if (pMsg == NULL) {
        FPRINTF_DEBUG(stderr, "Error: pMsg is NULL in dump_parsed_message\n");
        return;
    }

    char errStr[128];
    const char *err = rs_strerror_r(res.rc, errStr, sizeof(errStr));
    (void)err;

    FPRINTF_DEBUG(stdout, "\n=== PARSED MESSAGE ===\n");

    // Extract message properties after triggering parsing
    uchar *pszMsg = getMSG(pMsg);
    const char *pszHostname = getHOSTNAME(pMsg);
    uchar *pszAppName = (uchar *)getAPPNAME(pMsg, 0);  // 0 = no mutex lock needed in fuzzer context
    const char *pszTimestamp = getTimeReported(pMsg, tplFmtDefault);

    // Print the extracted properties
    if (pszMsg != NULL && pszMsg[0] != '\0') {
        FPRINTF_DEBUG(stdout, "msg: %s\n", (char *)pszMsg);
    } else {
        FPRINTF_DEBUG(stdout, "msg: (not available)\n");
    }

    if (pszHostname != NULL && pszHostname[0] != '\0') {
        FPRINTF_DEBUG(stdout, "hostname: %s\n", pszHostname);
    } else {
        FPRINTF_DEBUG(stdout, "hostname: (not available)\n");
    }

    if (pszAppName != NULL && pszAppName[0] != '\0') {
        FPRINTF_DEBUG(stdout, "app-name: %s\n", (char *)pszAppName);
    } else {
        // If getAPPNAME is not available, try using getProgramName
        uchar *pszProgName = getProgramName(pMsg, 0);  // 0 = no mutex lock
        if (pszProgName != NULL && pszProgName[0] != '\0') {
            FPRINTF_DEBUG(stdout, "app-name: %s\n", (char *)pszProgName);
        } else {
            FPRINTF_DEBUG(stdout, "app-name: -\n");
        }
    }

    if (pszTimestamp != NULL && pszTimestamp[0] != '\0') {
        FPRINTF_DEBUG(stdout, "timestamp: %s\n", pszTimestamp);
    } else {
        // Try different format types
        pszTimestamp = getTimeReported(pMsg, tplFmtRFC3164Date);
        if (pszTimestamp != NULL && pszTimestamp[0] != '\0') {
            FPRINTF_DEBUG(stdout, "timestamp: %s\n", pszTimestamp);
        } else {
            pszTimestamp = getTimeReported(pMsg, tplFmtRFC3339Date);
            if (pszTimestamp != NULL && pszTimestamp[0] != '\0') {
                FPRINTF_DEBUG(stdout, "timestamp: %s\n", pszTimestamp);
            } else {
                FPRINTF_DEBUG(stdout, "timestamp: (not available)\n");
            }
        }
    }

    // Check for structured data
    if (MsgHasStructuredData(pMsg)) {
        FPRINTF_DEBUG(stdout, "structured-data: present\n");
    } else {
        FPRINTF_DEBUG(stdout, "structured-data: not present\n");
    }

    FPRINTF_DEBUG(stdout, "parse-success: %s (rc=%d, %s)\n", (pMsg->bParseSuccess) ? "true" : "false", res.rc, err);
    FPRINTF_DEBUG(stdout, "=======================\n");
    fflush(stdout);
#endif
}

static smsg_t *construct_msg_from_input(const uint8_t *Data, size_t Size) {
    static const uint8_t empty_input = 0;

    if (Data == NULL && Size != 0) {
        return NULL;
    }
    if (Data == NULL) {
        Data = &empty_input;
    }
    smsg_t *pMsg = NULL;
    if (msgConstruct(&pMsg) != RS_RET_OK) {
        return NULL;
    }
    char *buf = malloc(Size + 1);
    if (buf == NULL) {
        msgDestruct(&pMsg);
        return NULL;
    }
    if (Size != 0) {
        memcpy(buf, Data, Size);
    }
    buf[Size] = '\0';
    MsgSetRawMsg(pMsg, buf, Size);
    free(buf);
    MsgSetMSGoffs(pMsg, 0);
    pMsg->msgFlags = NEEDS_PARSING | IGNDATE;
    if (pInputName != NULL) {
        MsgSetInputName(pMsg, pInputName);
    }
    return pMsg;
}

static int make_temp_dir(char *buf, size_t len, const char *tag) {
    if (buf == NULL || len == 0 || tag == NULL) {
        return -1;
    }
    const char *tmpBase = getenv("TMPDIR");
    if (tmpBase == NULL || tmpBase[0] == '\0') {
        tmpBase = "/tmp";
    }
    const int n = snprintf(buf, len, "%s/rsyslog-fuzz-%s-XXXXXX", tmpBase, tag);
    if (n < 0 || (size_t)n >= len) {
        return -1;
    }
    return mkdtemp(buf) == NULL ? -1 : 0;
}

static void cleanup_temp_dir(const char *path) {
    if (path == NULL || path[0] == '\0') {
        return;
    }
    DIR *dir = opendir(path);
    if (dir != NULL) {
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) {
                continue;
            }
            char filePath[PATH_MAX];
            const int n = snprintf(filePath, sizeof(filePath), "%s/%s", path, ent->d_name);
            if (n > 0 && (size_t)n < sizeof(filePath)) {
                (void)unlink(filePath);
            }
        }
        closedir(dir);
    }
    (void)rmdir(path);
}

static void format_with_template(struct template *tpl, smsg_t *pMsg) {
    if (tpl == NULL || pMsg == NULL) {
        return;
    }
    actWrkrIParams_t iparam = {0};
    if (tplToString(tpl, pMsg, &iparam, NULL) == RS_RET_OK && iparam.param != NULL) {
        free(iparam.param);
    } else if (iparam.param != NULL) {
        free(iparam.param);
    }
}

static parse_result_t parse_message_with_rsyslog(smsg_t *pMsg) {
    parse_result_t res = {.rc = RS_RET_COULD_NOT_PARSE};

    if (pMsg == NULL) {
        res.rc = RS_RET_PARAM_ERROR;
        return res;
    }
    /* parser.SanitizeMsg() requires a non-empty raw message and asserts that
     * contract. The dispatcher still delivers zero-length testcases to this
     * target; reject them here as an API parameter error. */
    if (pMsg->iLenRawMsg == 0) {
        MsgSetParseSuccess(pMsg, 0);
        res.rc = RS_RET_PARAM_ERROR;
        return res;
    }
    if (parser.ifIsLoaded && parser.SanitizeMsg != NULL) {
        parser.SanitizeMsg(pMsg);
    }

    const int parser_ready = parser.ifIsLoaded && parser.ParseMsg != NULL && ourConf != NULL;
    if (parser_ready) {
        res.rc = parser.ParseMsg(pMsg);
        if (res.rc == RS_RET_OK) {
            MsgSetParseSuccess(pMsg, 1);
        }
        return res;
    }

    MsgSetParseSuccess(pMsg, 0);
    return res;
}

static parse_result_t parse_message_with_named_parser(smsg_t *pMsg, const uchar *parserName) {
    parse_result_t res = {.rc = RS_RET_COULD_NOT_PARSE};
    parser_t *pParser = NULL;
    parserList_t *tmpList = NULL;
    parserList_t *savedList = NULL;

    if (pMsg == NULL) {
        res.rc = RS_RET_PARAM_ERROR;
        return res;
    }
    if (parserName == NULL || !parser.ifIsLoaded || parser.FindParser == NULL || parser.AddParserToList == NULL ||
        parser.DestructParserList == NULL || ourConf == NULL || runConf == NULL) {
        MsgSetParseSuccess(pMsg, 0);
        return res;
    }
    if (parser.FindParser(ourConf->parsers.pParsLstRoot, &pParser, (uchar *)parserName) != RS_RET_OK ||
        pParser == NULL) {
        MsgSetParseSuccess(pMsg, 0);
        return res;
    }
    if (parser.AddParserToList(&tmpList, pParser) != RS_RET_OK) {
        MsgSetParseSuccess(pMsg, 0);
        return res;
    }

    savedList = runConf->parsers.pDfltParsLst;
    runConf->parsers.pDfltParsLst = tmpList;
    res = parse_message_with_rsyslog(pMsg);
    runConf->parsers.pDfltParsLst = savedList;
    parser.DestructParserList(&tmpList);
    return res;
}

static char *build_json_path(const uint8_t *Data, size_t Size) {
    const size_t pathLen = Size > 32 ? 32 : Size;
    char *path = malloc(pathLen + 2);
    if (path == NULL) {
        return NULL;
    }
    path[0] = '!';
    for (size_t i = 0; i < pathLen; ++i) {
        unsigned char c = Data[i];
        if (!isalnum(c)) {
            c = (unsigned char)('a' + (c % 26));
        }
        path[i + 1] = (char)c;
    }
    path[pathLen + 1] = '\0';
    return path;
}

void rs_fuzz_parse_entry(const uint8_t *Data, size_t Size) {
    smsg_t *pMsg = construct_msg_from_input(Data, Size);
    if (pMsg == NULL) {
        return;
    }
    const parse_result_t res = parse_message_with_rsyslog(pMsg);
    dump_parsed_message(pMsg, res);
    msgDestruct(&pMsg);
}

void rs_fuzz_parse_rfc3164_entry(const uint8_t *Data, size_t Size) {
    smsg_t *pMsg = construct_msg_from_input(Data, Size);
    if (pMsg == NULL) {
        return;
    }
    const parse_result_t res = parse_message_with_named_parser(pMsg, (const uchar *)"rsyslog.rfc3164");
    dump_parsed_message(pMsg, res);
    msgDestruct(&pMsg);
}

void rs_fuzz_parse_rfc5424_entry(const uint8_t *Data, size_t Size) {
    smsg_t *pMsg = construct_msg_from_input(Data, Size);
    if (pMsg == NULL) {
        return;
    }
    const parse_result_t res = parse_message_with_named_parser(pMsg, (const uchar *)"rsyslog.rfc5424");
    dump_parsed_message(pMsg, res);
    msgDestruct(&pMsg);
}

void rs_fuzz_ruleset_entry(const uint8_t *Data, size_t Size) {
    smsg_t *pMsg = construct_msg_from_input(Data, Size);
    if (pMsg == NULL) {
        return;
    }
    const parse_result_t res = parse_message_with_rsyslog(pMsg);
    if (pMsg->bParseSuccess && ensure_discard_action() == RS_RET_OK) {
        pMsg->pRuleset = g_default_ruleset;
        wti_t *pWti = ensure_worker();
        if (pWti != NULL) {
            batch_t batch;
            if (batchInit(&batch, 1) == RS_RET_OK) {
                batch.nElem = 1;
                batch.pElem[0].pMsg = pMsg;
                batch.eltState[0] = BATCH_STATE_RDY;
                if (ruleset.ifIsLoaded && ruleset.ProcessBatch != NULL) {
                    ruleset.ProcessBatch(&batch, pWti);
                }
                batchFree(&batch);
            }
        }
    }
    dump_parsed_message(pMsg, res);
    msgDestruct(&pMsg);
}

void rs_fuzz_template_entry(const uint8_t *Data, size_t Size) {
    smsg_t *pMsg = construct_msg_from_input(Data, Size);
    if (pMsg == NULL) {
        return;
    }
    const parse_result_t res = parse_message_with_rsyslog(pMsg);
    struct template *tpl = ensure_default_template();
    if (tpl != NULL) {
        actWrkrIParams_t iparam = {0};
        tplToString(tpl, pMsg, &iparam, NULL);
        if (iparam.param != NULL) {
            free(iparam.param);
        }
    }
    dump_parsed_message(pMsg, res);
    msgDestruct(&pMsg);
}

void rs_fuzz_timestamp_entry(const uint8_t *Data, size_t Size) {
    if (Size == 0 || !datetime.ifIsLoaded) {
        return;
    }
    const size_t chunk = Size > 64 ? 64 : Size;
    uchar *buf1 = malloc(chunk + 1);
    if (buf1 == NULL) {
        return;
    }
    memcpy(buf1, Data, chunk);
    buf1[chunk] = '\0';
    uchar *ptr1 = buf1;
    int len1 = (int)chunk;
    struct syslogTime ts;
    datetime.ParseTIMESTAMP3339(&ts, &ptr1, &len1);
    uchar *buf2 = malloc(chunk + 1);
    if (buf2 != NULL) {
        memcpy(buf2, Data, chunk);
        buf2[chunk] = '\0';
        uchar *ptr2 = buf2;
        int len2 = (int)chunk;
        datetime.ParseTIMESTAMP3164(&ts, &ptr2, &len2, PARSE3164_TZSTRING, PERMIT_YEAR_AFTER_TIME);
        free(buf2);
    }
    free(buf1);
}

void rs_fuzz_json_props_entry(const uint8_t *Data, size_t Size) {
    smsg_t *pMsg = construct_msg_from_input(Data, Size);
    if (pMsg == NULL) {
        return;
    }

    const size_t chunk = Size > 64 ? 64 : Size;
    const size_t rest = Size > chunk ? Size - chunk : 0;
    const size_t sdChunk = rest > 64 ? 64 : rest;

    struct json_object *root = json_object_new_object();
    if (root != NULL) {
        struct json_object *msgStr = json_object_new_string_len((const char *)Data, (int)chunk);
        struct json_object *sdStr = sdChunk > 0 ? json_object_new_string_len((const char *)(Data + chunk), (int)sdChunk)
                                                : json_object_new_string("fuzz");
        struct json_object *appStr = json_object_new_string("fuzzer");
        struct json_object *hostStr = json_object_new_string("hfuzz");
        if (msgStr != NULL) {
            json_object_object_add(root, "msg", msgStr);
        }
        if (sdStr != NULL) {
            json_object_object_add(root, "structured-data", sdStr);
        }
        if (appStr != NULL) {
            json_object_object_add(root, "app", appStr);
        }
        if (hostStr != NULL) {
            json_object_object_add(root, "hostname", hostStr);
        }

        struct json_object *copy = json_object_get(root);
        if (copy != NULL) {
            if (msgAddJSON(pMsg, (uchar *)"!", copy, 0, 0) != RS_RET_OK) {
                json_object_put(copy);
            }
        }
        json_object_put(root);
    }

    msgDestruct(&pMsg);
}

void rs_fuzz_json_accessor_entry(const uint8_t *Data, size_t Size) {
    if (Size == 0) {
        return;
    }
    smsg_t *pMsg = construct_msg_from_input(Data, Size);
    if (pMsg == NULL) {
        return;
    }
    size_t mid = Size / 2;
    if (mid == 0) {
        mid = Size;
    }

    const size_t keyChunk = mid > 64 ? 64 : mid;

    struct json_object *root = json_object_new_object();
    if (root != NULL) {
        struct json_object *nested = json_object_new_object();
        if (nested != NULL) {
            struct json_object *val = json_object_new_string_len((const char *)Data, (int)keyChunk);
            if (val != NULL) {
                json_object_object_add(nested, "value", val);
            }
            json_object_object_add(root, "nested", nested);
        }
        struct json_object *copy = json_object_get(root);
        if (copy != NULL) {
            if (msgAddJSON(pMsg, (uchar *)"!", copy, 0, 0) != RS_RET_OK) {
                json_object_put(copy);
            }
        }
        json_object_put(root);
    }

    char *path = build_json_path(Data + mid, Size - mid);
    if (path != NULL) {
        msgPropDescr_t msgProp;
        msgProp.id = PROP_CEE;
        msgProp.name = (uchar *)path;
        msgProp.nameLen = (int)strlen(path);
        uchar *value = NULL;
        rs_size_t bufLen = 0;
        unsigned short mustFree = 0;
        getJSONPropVal(pMsg, &msgProp, &value, &bufLen, &mustFree);
        if (mustFree && value != NULL) {
            free(value);
        }
        free(path);
    }
    msgDestruct(&pMsg);
}

void rs_fuzz_template_json_entry(const uint8_t *Data, size_t Size) {
    smsg_t *pMsg = construct_msg_from_input(Data, Size);
    if (pMsg == NULL) {
        return;
    }
    const parse_result_t res = parse_message_with_rsyslog(pMsg);
    struct template *tpl = ensure_default_template();
    if (tpl != NULL) {
        struct json_object *out = NULL;
        if (tplToJSON(tpl, pMsg, &out, NULL) == RS_RET_OK && out != NULL) {
            json_object_put(out);
        }
    }
    (void)res;
    msgDestruct(&pMsg);
}

void rs_fuzz_json_msgset_entry(const uint8_t *Data, size_t Size) {
    smsg_t *pMsg = construct_msg_from_input(Data, Size);
    if (pMsg == NULL) {
        return;
    }

    /* Use the byte stream as JSON string */
    const size_t strLen = Size;
    char *jsonStr = malloc(strLen + 1);
    if (jsonStr != NULL) {
        for (size_t i = 0; i < Size; ++i) {
            unsigned char c = Data[i];
            if (c == '\0' || c == '\n' || c == '\r') {
                c = ' ';
            }
            jsonStr[i] = (char)c;
        }
        jsonStr[Size] = '\0';
        MsgSetPropsViaJSON(pMsg, (uchar *)jsonStr);
        free(jsonStr);
    }
    msgDestruct(&pMsg);

    pMsg = construct_msg_from_input(Data, Size);
    if (pMsg == NULL) {
        return;
    }

    /* Try object API as well */
    struct fjson_tokener *tok = fjson_tokener_new();
    if (tok != NULL) {
        struct json_object *root = fjson_tokener_parse_ex(tok, (const char *)Data, (int)Size);
        enum fjson_tokener_error jerr = fjson_tokener_get_error(tok);
        if (jerr == fjson_tokener_success && root != NULL) {
            if (json_object_is_type(root, json_type_object)) {
                (void)MsgSetPropsViaJSON_Object(pMsg, root);
                root = NULL; /* MsgSetPropsViaJSON_Object consumes object references. */
            }
            if (root != NULL) {
                json_object_put(root);
            }
        }
        fjson_tokener_free(tok);
    }

    /* Exercise getters */
    msgPropDescr_t msgProp = {0};
    msgProp.id = PROP_CEE;
    msgProp.name = (uchar *)"msg";
    msgProp.nameLen = 3;
    struct json_object *out = NULL;
    uchar *val = NULL;
    msgGetJSONPropJSONorString(pMsg, &msgProp, &out, &val);
    if (out != NULL) {
        json_object_put(out);
    }
    if (val != NULL) {
        free(val);
    }

    msgDestruct(&pMsg);
}

void rs_fuzz_structured_data_entry(const uint8_t *Data, size_t Size) {
    smsg_t *pMsg = construct_msg_from_input(Data, Size);
    if (pMsg == NULL) {
        return;
    }
    if (Size > 0) {
        char *sd = malloc(Size + 1);
        if (sd != NULL) {
            memcpy(sd, Data, Size);
            sd[Size] = '\0';
            MsgSetStructuredData(pMsg, sd);
            free(sd);
        }
    }
    uchar *buf = NULL;
    rs_size_t len = 0;
    MsgGetStructuredData(pMsg, &buf, &len);
    (void)buf;
    msgDestruct(&pMsg);
}
/* Drive template logic with data-dependent structure */
void rs_fuzz_template_dynamic_entry(const uint8_t *Data, size_t Size) {
    struct template *tpl = NULL;
    if (ourConf == NULL || Size == 0) {
        return;
    }
    /* Build a dynamic template string mixing tmpl vars and raw data */
    const size_t maxTpl = 128;
    char *tplBuf = malloc(maxTpl + 16);
    if (tplBuf == NULL) {
        return;
    }
    size_t out = 0;
    tplBuf[out++] = '"';
    for (size_t i = 0; i < Size && out + 6 < maxTpl; ++i) {
        unsigned char c = Data[i];
        if ((c % 11) == 0) {
            const char *tok = "%msg%";
            size_t tokLen = strlen(tok);
            if (out + tokLen < maxTpl) {
                memcpy(&tplBuf[out], tok, tokLen);
                out += tokLen;
                continue;
            }
        } else if ((c % 13) == 0) {
            const char *tok = "%json%";
            size_t tokLen = strlen(tok);
            if (out + tokLen < maxTpl) {
                memcpy(&tplBuf[out], tok, tokLen);
                out += tokLen;
                continue;
            }
        } else if ((c % 17) == 0) {
            const char *tok = "%hostname%";
            size_t tokLen = strlen(tok);
            if (out + tokLen < maxTpl) {
                memcpy(&tplBuf[out], tok, tokLen);
                out += tokLen;
                continue;
            }
        }
        if (!isprint(c)) {
            c = (unsigned char)('a' + (c % 26));
        }
        tplBuf[out++] = (char)c;
    }
    tplBuf[out++] = '"';
    tplBuf[out] = '\0';

    uchar *pTpl = (uchar *)tplBuf;
    tpl = tplAddLine(ourConf, "FUZZ_DYN_TEMPLATE", &pTpl);
    if (tpl != NULL) {
        smsg_t *pMsg = construct_msg_from_input(Data, Size);
        if (pMsg != NULL) {
            const parse_result_t res = parse_message_with_rsyslog(pMsg);
            actWrkrIParams_t iparam = {0};
            tplToString(tpl, pMsg, &iparam, NULL);
            if (iparam.param != NULL) {
                free(iparam.param);
            }
            (void)res;
            msgDestruct(&pMsg);
        }
    }
    /* tplAddLine returns internal pointer; no template free here */
    tplDeleteNew(ourConf);
    free(tplBuf);
}

static rsRetVal fuzz_tcp_submit(tcps_sess_t *pSess, uchar *msg, int len) {
    (void)pSess;
    if (msg == NULL || len <= 0) {
        return RS_RET_OK;
    }
    smsg_t *pMsg = construct_msg_from_input(msg, (size_t)len);
    if (pMsg == NULL) {
        return RS_RET_OK;
    }
    const parse_result_t res = parse_message_with_rsyslog(pMsg);
    dump_parsed_message(pMsg, res);
    msgDestruct(&pMsg);
    return RS_RET_OK;
}

void rs_fuzz_tcp_framing_entry(const uint8_t *Data, size_t Size) {
    if (Size == 0) {
        return;
    }
    (void)ensure_conf_initialized();
    if (!tcps_sess.ifIsLoaded || tcps_sess.Construct == NULL) {
        return;
    }

    tcps_sess_t *sess = NULL;
    tcpsrv_t srv;
    tcpLstnParams_t params;
    tcpLstnPortList_t lstn;
    memset(&srv, 0, sizeof(srv));
    memset(&params, 0, sizeof(params));
    memset(&lstn, 0, sizeof(lstn));

    srv.bDisableLFDelim = (Data[0] & 0x20) != 0;
    srv.discardTruncatedMsg = (Data[0] & 0x40) != 0;
    srv.addtlFrameDelim = (Data[0] & 0x80) ? (int)Data[Size > 1 ? 1 : 0] : TCPSRV_NO_ADDTL_DELIMITER;
    srv.maxFrameSize = 1 + (int)(Data[Size > 2 ? 2 : 0] * 1024);

    params.bSuppOctetFram = 1;
    params.bSPFramingFix = (Data[0] & 0x10) != 0;
    params.pszInputName = (uchar *)"fuzz-tcp";
    params.pInputName = pInputName;
    params.pRuleset = g_default_ruleset;
    lstn.cnf_params = &params;
    lstn.pSrv = &srv;

    if (tcps_sess.Construct(&sess) != RS_RET_OK || sess == NULL) {
        return;
    }
    (void)tcps_sess.SetTcpsrv(sess, &srv);
    (void)tcps_sess.SetLstnInfo(sess, &lstn);
    (void)tcps_sess.SetOnMsgReceive(sess, fuzz_tcp_submit);
    if (tcps_sess.ConstructFinalize != NULL) {
        (void)tcps_sess.ConstructFinalize(sess);
    }

    size_t off = 0;
    while (off < Size) {
        size_t chunk = 1 + (Data[off] % 64);
        if (chunk > Size - off) {
            chunk = Size - off;
        }
        (void)tcps_sess.DataRcvd(sess, (char *)Data + off, chunk);
        off += chunk;
    }
    if (tcps_sess.PrepareClose != NULL) {
        (void)tcps_sess.PrepareClose(sess);
    }
    tcps_sess.Destruct(&sess);
}

static rsRetVal fuzz_queue_consumer(void *usr, batch_t *pBatch, wti_t *pWti) {
    (void)usr;
    (void)pWti;
    if (pBatch == NULL) {
        return RS_RET_OK;
    }
    struct template *tpl = ensure_default_template();
    for (int i = 0; i < pBatch->nElem; ++i) {
        smsg_t *pMsg = pBatch->pElem[i].pMsg;
        if (pMsg == NULL) {
            continue;
        }
        (void)parse_message_with_rsyslog(pMsg);
        format_with_template(tpl, pMsg);
        pBatch->eltState[i] = BATCH_STATE_COMM;
    }
    return RS_RET_OK;
}

static void enqueue_slices(qqueue_t *queue, const uint8_t *Data, size_t Size) {
    if (queue == NULL || Data == NULL || Size == 0) {
        return;
    }
    const size_t nMsgs = 1 + (Data[0] % 4);
    size_t off = Size > 1 ? 1 : 0;
    for (size_t i = 0; i < nMsgs; ++i) {
        if (off >= Size) {
            off = 0;
        }
        size_t avail = Size - off;
        if (avail == 0) {
            break;
        }
        size_t len = 1 + (Data[off] % 128);
        if (len > avail) {
            len = avail;
        }
        smsg_t *pMsg = construct_msg_from_input(Data + off, len);
        if (pMsg == NULL) {
            break;
        }
        pMsg->flowCtlType = eFLOWCTL_NO_DELAY;
        (void)qqueueEnqMsg(queue, pMsg->flowCtlType, pMsg);
        off += len;
    }
}

static void run_queue_variant(queueType_t qType, const uint8_t *Data, size_t Size, const char *name, int diskBacked) {
    qqueue_t *queue = NULL;
    char tmpDir[PATH_MAX] = {0};
    char prefix[64];
    const int maxQueueSize = qType == QUEUETYPE_DIRECT || qType == QUEUETYPE_DISK ? 0 : 16;

    if (ensure_conf_initialized() != RS_RET_OK) {
        return;
    }
    if (diskBacked && make_temp_dir(tmpDir, sizeof(tmpDir), "queue") != 0) {
        return;
    }

    if (qqueueConstruct(&queue, qType, 1, maxQueueSize, fuzz_queue_consumer) != RS_RET_OK || queue == NULL) {
        if (diskBacked) {
            cleanup_temp_dir(tmpDir);
        }
        return;
    }
    if (name != NULL) {
        obj.SetName((obj_t *)queue, (uchar *)name);
    }
    /* The direct queue exercises the consumer synchronously. Keep the
     * asynchronous variants enqueue-only so a testcase owns no background
     * worker beyond its dispatch lifecycle. */
    queue->bEnqOnly = qType != QUEUETYPE_DIRECT;
    (void)qqueueSettoEnq(queue, 0);
    (void)qqueueSettoWrkShutdown(queue, 100);
    (void)qqueueSettoQShutdown(queue, 100);
    (void)qqueueSetiDeqBatchSize(queue, 4);
    if (diskBacked) {
        snprintf(prefix, sizeof(prefix), "fq-%02x%02x", Size > 0 ? Data[0] : 0, Size > 1 ? Data[1] : 0);
        (void)qqueueSetSpoolDir(queue, (uchar *)tmpDir, (int)strlen(tmpDir));
        (void)qqueueSetFilePrefix(queue, (uchar *)prefix, strlen(prefix));
        (void)qqueueSetMaxFileSize(queue, 256 + (size_t)((Size > 2 ? Data[2] : 0) * 16));
        (void)qqueueSetsizeOnDiskMax(queue, 0);
        if (qType != QUEUETYPE_DISK) {
            (void)qqueueSetiHighWtrMrk(queue, 1);
            (void)qqueueSetiLowWtrMrk(queue, 0);
        }
    }
    if (qqueueStart(ourConf, queue) == RS_RET_OK) {
        enqueue_slices(queue, Data, Size);
    }
    qqueueDestruct(&queue);
    if (diskBacked) {
        qqueueDoneLoadCnf();
        cleanup_temp_dir(tmpDir);
    }
    reset_runtime_conf_pointers();
}

static void run_queue_da_config_variant(const uint8_t *Data, size_t Size) {
    qqueue_t *queue = NULL;
    char tmpDir[PATH_MAX] = {0};
    char prefix[64];

    if (ensure_conf_initialized() != RS_RET_OK || make_temp_dir(tmpDir, sizeof(tmpDir), "queue-da") != 0) {
        return;
    }
    if (qqueueConstruct(&queue, QUEUETYPE_LINKEDLIST, 1, 16, fuzz_queue_consumer) != RS_RET_OK || queue == NULL) {
        cleanup_temp_dir(tmpDir);
        return;
    }
    obj.SetName((obj_t *)queue, (uchar *)"fuzz-da-config-queue");
    snprintf(prefix, sizeof(prefix), "fqda-%02x%02x", Size > 0 ? Data[0] : 0, Size > 1 ? Data[1] : 0);
    (void)qqueueSetSpoolDir(queue, (uchar *)tmpDir, (int)strlen(tmpDir));
    (void)qqueueSetFilePrefix(queue, (uchar *)prefix, strlen(prefix));
    (void)qqueueSetMaxFileSize(queue, 256 + (size_t)((Size > 2 ? Data[2] : 0) * 16));
    (void)qqueueSetiHighWtrMrk(queue, 1);
    (void)qqueueSetiLowWtrMrk(queue, 0);
    (void)qqueueSetiNumWorkerThreads(queue, 1);
    (void)qqueueSettoEnq(queue, 0);
    qqueueCorrectParams(queue);
    qqueueDestruct(&queue);
    cleanup_temp_dir(tmpDir);
    reset_runtime_conf_pointers();
}

static void run_queue_da_start_variant(const uint8_t *Data, size_t Size) {
    qqueue_t *queue = NULL;
    char tmpDir[PATH_MAX] = {0};
    char prefix[64];

    if (ensure_conf_initialized() != RS_RET_OK || make_temp_dir(tmpDir, sizeof(tmpDir), "queue-da-start") != 0) {
        return;
    }
    if (qqueueConstruct(&queue, QUEUETYPE_LINKEDLIST, 1, 16, fuzz_queue_consumer) != RS_RET_OK || queue == NULL) {
        cleanup_temp_dir(tmpDir);
        return;
    }
    obj.SetName((obj_t *)queue, (uchar *)"fuzz-da-start-queue");
    snprintf(prefix, sizeof(prefix), "fqds-%02x%02x", Size > 0 ? Data[0] : 0, Size > 1 ? Data[1] : 0);
    (void)qqueueSetSpoolDir(queue, (uchar *)tmpDir, (int)strlen(tmpDir));
    (void)qqueueSetFilePrefix(queue, (uchar *)prefix, strlen(prefix));
    (void)qqueueSetMaxFileSize(queue, 256 + (size_t)((Size > 2 ? Data[2] : 0) * 16));
    (void)qqueueSetsizeOnDiskMax(queue, 0);
    (void)qqueueSetiHighWtrMrk(queue, 1);
    (void)qqueueSetiLowWtrMrk(queue, 0);
    (void)qqueueSetiDiscardMrk(queue, 0);
    (void)qqueueSetiDeqBatchSize(queue, 4);
    (void)qqueueSetiNumWorkerThreads(queue, 1);
    (void)qqueueSettoEnq(queue, 0);
    (void)qqueueSettoWrkShutdown(queue, 0);
    (void)qqueueSettoQShutdown(queue, 0);
    if (qqueueStart(ourConf, queue) == RS_RET_OK && getenv("RSYSLOG_FUZZ_ENABLE_DA_WORKER") != NULL) {
        enqueue_slices(queue, Data, Size);
    }
    qqueueDestruct(&queue);
    qqueueDoneLoadCnf();
    cleanup_temp_dir(tmpDir);
    reset_runtime_conf_pointers();
}

void rs_fuzz_queue_lifecycle_entry(const uint8_t *Data, size_t Size) {
    if (Size == 0) {
        return;
    }
    run_queue_variant(QUEUETYPE_DIRECT, Data, Size, "fuzz-direct-queue", 0);
    run_queue_variant(QUEUETYPE_LINKEDLIST, Data, Size, "fuzz-linkedlist-queue", 0);
    run_queue_variant(QUEUETYPE_DISK, Data, Size, "fuzz-disk-queue", 1);
    run_queue_da_config_variant(Data, Size);
    if (getenv("RSYSLOG_FUZZ_ENABLE_DA_START") != NULL) {
        run_queue_da_start_variant(Data, Size);
    }
}

static void process_imfile_like_line(const char *line, size_t len) {
    if (line == NULL || len == 0) {
        return;
    }
    smsg_t *pMsg = construct_msg_from_input((const uint8_t *)line, len);
    if (pMsg == NULL) {
        return;
    }
    const parse_result_t res = parse_message_with_rsyslog(pMsg);
    format_with_template(ensure_default_template(), pMsg);
    (void)res;
    msgDestruct(&pMsg);
}

void rs_fuzz_imfile_line_entry(const uint8_t *Data, size_t Size) {
    if (Size == 0) {
        return;
    }
    const char *tmpBase = getenv("TMPDIR");
    if (tmpBase == NULL || tmpBase[0] == '\0') {
        tmpBase = "/tmp";
    }
    char path[PATH_MAX];
    const int pathLen = snprintf(path, sizeof(path), "%s/rsyslog-fuzz-imfile-XXXXXX", tmpBase);
    if (pathLen < 0 || (size_t)pathLen >= sizeof(path)) {
        return;
    }
    int fd = mkstemp(path);
    if (fd < 0) {
        return;
    }
    size_t written = 0;
    while (written < Size) {
        ssize_t n = write(fd, Data + written, Size - written);
        if (n <= 0) {
            close(fd);
            (void)unlink(path);
            return;
        }
        written += (size_t)n;
    }
    if (lseek(fd, 0, SEEK_SET) < 0) {
        close(fd);
        (void)unlink(path);
        return;
    }
    FILE *fp = fdopen(fd, "rb");
    if (fp == NULL) {
        close(fd);
        (void)unlink(path);
        return;
    }
    char *line = malloc(Size + 1);
    if (line == NULL) {
        fclose(fp);
        (void)unlink(path);
        return;
    }
    size_t len = 0;
    int ch;
    while ((ch = fgetc(fp)) != EOF) {
        if (ch == '\n') {
            process_imfile_like_line(line, len);
            len = 0;
        } else if (len < Size) {
            line[len++] = (char)ch;
        }
    }
    if (len > 0) {
        process_imfile_like_line(line, len);
    }
    free(line);
    fclose(fp);
    (void)unlink(path);
}

static void format_named_template(const char *name, smsg_t *pMsg) {
    if (name == NULL || pMsg == NULL || ourConf == NULL) {
        return;
    }
    struct template *tpl = tplFind(ourConf, (char *)name, (int)strlen(name));
    format_with_template(tpl, pMsg);
}

static void format_named_template_to_fd(const char *name, smsg_t *pMsg, int fd) {
    if (name == NULL || pMsg == NULL || ourConf == NULL) {
        return;
    }
    struct template *tpl = tplFind(ourConf, (char *)name, (int)strlen(name));
    if (tpl == NULL) {
        return;
    }
    actWrkrIParams_t iparam = {0};
    if (tplToString(tpl, pMsg, &iparam, NULL) == RS_RET_OK && iparam.param != NULL) {
        if (fd >= 0 && iparam.lenStr > 0) {
            ssize_t wr = write(fd, iparam.param, iparam.lenStr);
            (void)wr;
        }
    }
    free(iparam.param);
}

static void exercise_action_config(struct nvlst *lst) {
    if (lst == NULL) {
        return;
    }
    action_t *pAction = NULL;
    const rsRetVal ret = actionNewInst(lst, &pAction);
    if (pAction != NULL) {
        actionDestruct(pAction);
    }
    (void)ret;
    nvlstDestruct(lst);
}

static void exercise_omfile_action_config(const char *file) {
    if (file == NULL) {
        return;
    }
    exercise_action_config(make_action_kvlist("omfile", file));
}

static void exercise_omfwd_action_config(const uint8_t *Data, size_t Size) {
    struct nvlst *head = NULL;
    struct nvlst *tail = NULL;
    char port[8];
    const unsigned int portNum = 1024U + (unsigned int)(Size > 0 ? Data[0] : 0);
    snprintf(port, sizeof(port), "%u", portNum);
    if (append_kv_pair(&head, &tail, "type", "omfwd") != 0 ||
        append_kv_pair(&head, &tail, "target", "127.0.0.1") != 0 || append_kv_pair(&head, &tail, "port", port) != 0 ||
        append_kv_pair(&head, &tail, "protocol", (Size > 1 && (Data[1] & 1)) ? "tcp" : "udp") != 0 ||
        append_kv_pair(&head, &tail, "tcp_framing", (Size > 2 && (Data[2] & 1)) ? "octet-counted" : "traditional") !=
            0 ||
        append_kv_pair(&head, &tail, "template", "RSYSLOG_ForwardFormat") != 0) {
        if (head != NULL) {
            nvlstDestruct(head);
        }
        return;
    }
    exercise_action_config(head);
}

#ifdef FUZZ_HAVE_OMRELP
static void exercise_omrelp_action_config(const uint8_t *Data, size_t Size) {
    struct nvlst *head = NULL;
    struct nvlst *tail = NULL;
    char port[8];
    const unsigned int portNum = 20514U + (unsigned int)(Size > 0 ? Data[0] : 0);
    snprintf(port, sizeof(port), "%u", portNum);
    if (append_kv_pair(&head, &tail, "type", "omrelp") != 0 ||
        append_kv_pair(&head, &tail, "target", "127.0.0.1") != 0 || append_kv_pair(&head, &tail, "port", port) != 0 ||
        append_kv_pair(&head, &tail, "windowsize", (Size > 1 && (Data[1] & 1)) ? "1" : "32") != 0 ||
        append_kv_pair(&head, &tail, "timeout", "1") != 0 || append_kv_pair(&head, &tail, "conn.timeout", "1") != 0 ||
        append_kv_pair(&head, &tail, "template", "RSYSLOG_ForwardFormat") != 0) {
        if (head != NULL) {
            nvlstDestruct(head);
        }
        return;
    }
    exercise_action_config(head);
}
#endif

void rs_fuzz_action_format_entry(const uint8_t *Data, size_t Size) {
    if (Size == 0 || ensure_conf_initialized() != RS_RET_OK) {
        return;
    }
    char tmpDir[PATH_MAX] = {0};
    if (make_temp_dir(tmpDir, sizeof(tmpDir), "action") != 0) {
        return;
    }
    char outPath[PATH_MAX];
    const int n = snprintf(outPath, sizeof(outPath), "%s/out.log", tmpDir);
    if (n <= 0 || (size_t)n >= sizeof(outPath)) {
        cleanup_temp_dir(tmpDir);
        return;
    }

    smsg_t *pMsg = construct_msg_from_input(Data, Size);
    if (pMsg == NULL) {
        cleanup_temp_dir(tmpDir);
        return;
    }
    (void)parse_message_with_rsyslog(pMsg);
    format_named_template("RSYSLOG_FileFormat", pMsg);
    format_named_template("RSYSLOG_TraditionalFileFormat", pMsg);
    format_named_template("RSYSLOG_ForwardFormat", pMsg);
    format_named_template("RSYSLOG_TraditionalForwardFormat", pMsg);

    int fd = open(outPath, O_CREAT | O_TRUNC | O_WRONLY, 0600);
    if (fd >= 0) {
        format_named_template_to_fd("RSYSLOG_FileFormat", pMsg, fd);
        format_named_template_to_fd("RSYSLOG_ForwardFormat", pMsg, fd);
        format_named_template_to_fd("RSYSLOG_TraditionalForwardFormat", pMsg, fd);
        close(fd);
    }
    if (getenv("RSYSLOG_FUZZ_ENABLE_ACTION_CONFIG") != NULL) {
        exercise_omfile_action_config(outPath);
        exercise_omfwd_action_config(Data, Size);
#ifdef FUZZ_HAVE_OMRELP
        exercise_omrelp_action_config(Data, Size);
#endif
    }
    msgDestruct(&pMsg);
    cleanup_temp_dir(tmpDir);
}

void rs_fuzz_timestamp_format_entry(const uint8_t *Data, size_t Size) {
    if (Size == 0 || !datetime.ifIsLoaded) {
        return;
    }
    struct syslogTime ts = {0};
    /* derive fields from data to drive different branches */
    ts.year = 1970 + (int)(Data[0] % 60);
    ts.month = (Data[1 % Size] % 12) + 1;
    ts.day = (Data[2 % Size] % 28) + 1;
    ts.hour = Data[3 % Size] % 24;
    ts.minute = Data[4 % Size] % 60;
    ts.second = Data[5 % Size] % 60;
    ts.secfrac = (int)(Data[6 % Size] * 1000);
    ts.secfracPrecision = 6;
    int qh = (int)(int8_t)Data[7 % Size];
    if (qh < 0) {
        ts.OffsetMode = '-';
        qh = -qh;
    } else {
        ts.OffsetMode = '+';
    }
    int totalMin = (int)qh * 15;
    ts.OffsetHour = (intTiny)(totalMin / 60);
    ts.OffsetMinute = (intTiny)(totalMin % 60);

    char buf[128];
    datetime.formatTimestamp3164(&ts, buf, 0);
    datetime.formatTimestamp3339(&ts, buf);
    datetime.formatTimestampToMySQL(&ts, buf);
    datetime.formatTimestampToPgSQL(&ts, buf);
    datetime.formatTimestampSecFrac(&ts, buf);
    datetime.formatTimestampUnix(&ts, buf);
}

typedef enum legacy_arg_type_e { LEGACY_ARG_INT, LEGACY_ARG_STRING } legacy_arg_type_t;

typedef struct legacy_conf_snapshot_s {
    int main_queue_size;
    int debug_print_template_list;
    int uid_drop_priv;
    int gid_drop_priv;
    uchar *main_queue_filename;
} legacy_conf_snapshot_t;

int rs_fuzz_get_legacy_state(rs_fuzz_legacy_state_t *state) {
    if (state == NULL || loadConf == NULL) {
        return -1;
    }

    memset(state, 0, sizeof(*state));
    state->main_queue_size = loadConf->globals.mainQ.iMainMsgQueueSize;
    state->debug_print_template_list = loadConf->globals.bDebugPrintTemplateList;
    state->uid_drop_priv = loadConf->globals.uidDropPriv;
    state->gid_drop_priv = loadConf->globals.gidDropPriv;
    if (loadConf->globals.mainQ.pszMainMsgQFName != NULL) {
        const uchar *p = loadConf->globals.mainQ.pszMainMsgQFName;
        uint64_t hash = UINT64_C(1469598103934665603);
        state->main_queue_filename_present = 1;
        while (*p != '\0') {
            hash ^= *p++;
            hash *= UINT64_C(1099511628211);
            ++state->main_queue_filename_length;
        }
        state->main_queue_filename_hash = hash;
    }
    return 0;
}

static int save_legacy_conf(legacy_conf_snapshot_t *snapshot) {
    if (snapshot == NULL || loadConf == NULL) {
        return -1;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->main_queue_size = loadConf->globals.mainQ.iMainMsgQueueSize;
    snapshot->debug_print_template_list = loadConf->globals.bDebugPrintTemplateList;
    snapshot->uid_drop_priv = loadConf->globals.uidDropPriv;
    snapshot->gid_drop_priv = loadConf->globals.gidDropPriv;
    if (loadConf->globals.mainQ.pszMainMsgQFName != NULL) {
        snapshot->main_queue_filename = (uchar *)strdup((const char *)loadConf->globals.mainQ.pszMainMsgQFName);
        if (snapshot->main_queue_filename == NULL) {
            return -1;
        }
    }
    return 0;
}

static void restore_legacy_conf(legacy_conf_snapshot_t *snapshot) {
    if (snapshot == NULL || loadConf == NULL) {
        return;
    }
    loadConf->globals.mainQ.iMainMsgQueueSize = snapshot->main_queue_size;
    loadConf->globals.bDebugPrintTemplateList = snapshot->debug_print_template_list;
    loadConf->globals.uidDropPriv = snapshot->uid_drop_priv;
    loadConf->globals.gidDropPriv = snapshot->gid_drop_priv;
    free(loadConf->globals.mainQ.pszMainMsgQFName);
    loadConf->globals.mainQ.pszMainMsgQFName = snapshot->main_queue_filename;
    snapshot->main_queue_filename = NULL;
}

static const struct {
    const char *cmd;
    legacy_arg_type_t type;
} kLegacyCmds[] = {{"mainmsgqueuesize", LEGACY_ARG_INT},
                   {"mainmsgqueuefilename", LEGACY_ARG_STRING},
                   {"debugprinttemplatelist", LEGACY_ARG_INT},
                   {"privdroptouserid", LEGACY_ARG_INT},
                   {"privdroptogroupid", LEGACY_ARG_INT}};

void rs_fuzz_cfsysline_entry(const uint8_t *Data, size_t Size) {
    if (Size <= 1 || loadConf == NULL) {
        return;
    }
    legacy_conf_snapshot_t snapshot;
    if (save_legacy_conf(&snapshot) != 0) {
        return;
    }
    const size_t cmdIdx = Data[0] % (sizeof(kLegacyCmds) / sizeof(kLegacyCmds[0]));
    const char *cmdName = kLegacyCmds[cmdIdx].cmd;
    const legacy_arg_type_t argType = kLegacyCmds[cmdIdx].type;
    if (argType == LEGACY_ARG_INT) {
        uint32_t val = 0;
        for (size_t i = 1; i < Size && i < 5; ++i) {
            val = (val << 8) | Data[i];
        }
        char buf[32];
        snprintf(buf, sizeof(buf), "%u", val);
        uchar *line = (uchar *)strdup(buf);
        if (line == NULL) {
            goto finalize_it;
        }
        uchar *linePtr = line;
        processCfSysLineCommand((uchar *)cmdName, &linePtr);
        free(line);
    } else {
        const size_t avail = Size - 1;
        if (avail == 0) {
            goto finalize_it;
        }
        char *tmp = malloc(avail + 1);
        if (tmp == NULL) {
            goto finalize_it;
        }
        for (size_t i = 0; i < avail; ++i) {
            unsigned char c = Data[1 + i];
            if (!isprint(c)) {
                c = (unsigned char)('a' + (c % 26));
            }
            tmp[i] = (char)c;
        }
        tmp[avail] = '\0';
        uchar *linePtr = (uchar *)tmp;
        processCfSysLineCommand((uchar *)cmdName, &linePtr);
        free(tmp);
    }

finalize_it:
    restore_legacy_conf(&snapshot);
}

void rs_fuzz_reset_iteration(void) {
    if (g_dummy_wti != NULL) {
        g_dummy_shutdown_flag = 0;
        g_dummy_wti->pbShutdownImmediate = &g_dummy_shutdown_flag;
    }
    if (g_conf_ready) {
        reset_runtime_conf_pointers();
    }
}
