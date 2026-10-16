/* modules.h
 *
 * Definition for build-in and plug-ins module handler. This file is the base
 * for all dynamically loadable module support. In theory, in v3 all modules
 * are dynamically loaded, in practice we currently do have a few build-in
 * once. This may become removed.
 *
 * The loader keeps track of what is loaded. For library modules, it is also
 * used to find objects (libraries) and to obtain the queryInterface function
 * for them. A reference count is maintened for libraries, so that they are
 * unloaded only when nobody still accesses them.
 *
 * File begun on 2007-07-22 by RGerhards
 *
 * Copyright 2007-2026 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
 *
 * The rsyslog runtime library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The rsyslog runtime library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the rsyslog runtime library.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 * A copy of the LGPL can be found in the file "COPYING.LESSER" in this distribution.
 */
#ifndef MODULES_H_INCLUDED
#define MODULES_H_INCLUDED 1

#include "objomsr.h"
#include "rainerscript.h"

typedef struct action_s action_t;


/* the following define defines the current version of the module interface.
 * It can be used by any module which want's to simply prevent version conflicts
 * and does not intend to do specific old-version emulations.
 * rgerhards, 2008-03-04
 * version 3 adds modInfo_t ptr to call of modInit -- rgerhards, 2008-03-10
 * version 4 removes needUDPSocket OM callback -- rgerhards, 2008-03-22
 * version 5 changes the way parsing works for input modules. This is
 *           an important change, parseAndSubmitMessage() goes away. Other
 *           module types are not affected. -- rgerhards, 2008-10-09
 * version 6 introduces scoping support (starting with the output
 *           modules) -- rgerhards, 2010-07-27
 */
#define CURR_MOD_IF_VERSION 6

typedef enum eModType_ {
    eMOD_IN = 0, /* input module */
    eMOD_OUT = 1, /* output module */
    eMOD_LIB = 2, /* library module */
    eMOD_PARSER = 3, /* parser module */
    eMOD_STRGEN = 4, /* strgen module */
    eMOD_FUNCTION = 5, /*rscript function module*/
    eMOD_ANY = 6 /* meta-name for "any type of module" -- to be used in function calls */
} eModType_t;


#ifdef DEBUG
typedef struct modUsr_s {
    struct modUsr_s *pNext;
    char *pszFile;
} modUsr_t;
#endif


/* how is this module linked? */
typedef enum eModLinkType_ {
    eMOD_LINK_STATIC,
    eMOD_LINK_DYNAMIC_UNLOADED, /* dynalink module, currently not loaded */
    eMOD_LINK_DYNAMIC_LOADED, /* dynalink module, currently loaded */
    eMOD_LINK_ALL /* special: all linkage types, e.g. for unload */
} eModLinkType_t;

/* remember which shared libs we dlopen()-ed */
struct dlhandle_s {
    uchar *pszName;
    void *pModHdlr;
    struct dlhandle_s *next;
};

/* should this module be kept linked? */
typedef enum eModKeepType_ { eMOD_NOKEEP, eMOD_KEEP } eModKeepType_t;

/**
 * @brief Capability for one old/new configuration pair; numeric values are ABI.
 * Reload capability declared by a module for a particular old/new
 * configuration pair.  A module must never return a less restrictive class
 * unless it can preserve the stated lifecycle guarantee.
 *
 * The enum describes a capability, not a request to reload. Classification
 * alone never authorizes activation; the controller applies its scope gates.
 */
typedef enum eModReloadCapability_ {
    eMOD_RELOAD_RESTART_REQUIRED = 0, /**< Conservative default; process restart needed. */
    eMOD_RELOAD_LIVE_SWAP = 1, /**< Switch existing users to the replacement. */
    eMOD_RELOAD_NEW_SESSIONS = 2, /**< Existing users retain the old configuration. */
    eMOD_RELOAD_DRAIN_REPLACE = 3, /**< Drain old users before using the replacement. */
    eMOD_RELOAD_REUSE = 4 /**< Effective configuration is unchanged. */
} eModReloadCapability_t;

/**
 * @brief Optional version-1 module reload lifecycle and ownership contract.
 * Optional module reload lifecycle, version 1.  The configuration pointers
 * are core-owned, opaque module configuration objects; they are const because
 * classification and preparation must not mutate either configuration.
 * pReloadState is module-owned
 * state allocated by reloadPrepare and consumed by reloadAbort, or by
 * reloadCommit followed by reloadRetire.
 *
 * reloadCommit must be infallible: it has no return value and may not leave
 * the module in a state that requires rollback.  Any fallible work belongs in
 * reloadPrepare, which returns an rsRetVal and leaves pReloadState NULL on
 * failure.  reloadRetire reports failure so a future controller can retain
 * state and retry retirement.  The hooks are intentionally optional so that
 * modules built for the established module interface remain supported.
 */
/** @brief Classify borrowed, immutable configurations without external effects.
 * @param pOldCnf Core-owned old module configuration.
 * @param pNewCnf Core-owned candidate module configuration.
 * @param pCapability Required output, written on RS_RET_OK.
 * @return RS_RET_OK on classification; any error requires restart.
 */
typedef rsRetVal (*modReloadClassifyV1_t)(const void *pOldCnf,
                                          const void *pNewCnf,
                                          eModReloadCapability_t *pCapability);
/** @brief Prepare private resources without changing either borrowed configuration.
 * @param pOldCnf Core-owned old module configuration.
 * @param pNewCnf Core-owned candidate module configuration.
 * @param pReloadState Required output for module-owned transaction state.
 * @return RS_RET_OK when prepared; on error unwind all acquisitions and set
 * *pReloadState to NULL. Successful state is consumed by abort, or commit
 * followed by successful retire. The controller must keep configurations alive
 * for as long as the state borrows them.
 */
typedef rsRetVal (*modReloadPrepareV1_t)(const void *pOldCnf, const void *pNewCnf, void **pReloadState);
/** @brief Infallibly publish successfully prepared state; retain it for retire.
 * @param pReloadState State returned by successful prepare, not yet committed.
 */
typedef void (*modReloadCommitV1_t)(void *pReloadState);
/** @brief Unwind and consume successfully prepared, uncommitted state.
 * @param pReloadState State returned by successful prepare. Never call after
 * commit or failed prepare, and never release resources owned by the active
 * configuration. NULL is valid only if successful prepare returned NULL.
 */
typedef void (*modReloadAbortV1_t)(void *pReloadState);
/** @brief Retire committed state after all old consumers have quiesced.
 * @param pReloadState Committed module-owned transaction state.
 * @return RS_RET_OK after consuming state; on failure retain valid state so
 * the controller can retry. Never reactivate the old configuration.
 */
typedef rsRetVal (*modReloadRetireV1_t)(void *pReloadState);
/** @brief Explicit guarantees required before invoking the live lifecycle. */
typedef enum eModReloadCapabilityFlags_ {
    eMOD_RELOAD_CAP_VALIDATE_PRIVATE = 1U << 0, /**< Validation has no external effects. */
    eMOD_RELOAD_CAP_PREPARE = 1U << 1, /**< Fallible preparation can be unwound. */
    /** Unchanged effective configuration retains current runtime state;
     * this alone does not imply support for a mutating transition. */
    eMOD_RELOAD_CAP_REUSE = 1U << 2,
    eMOD_RELOAD_CAP_COMMIT = 1U << 3, /**< Commit cannot fail. */
    eMOD_RELOAD_CAP_RETIRE = 1U << 4 /**< Old resources survive until quiescence. */
} eModReloadCapabilityFlags_t;
/** @brief Module-supplied optional interface, copied into the core module record. */
typedef struct modReloadInterfaceV1_s {
    unsigned version; /**< Must equal eMOD_RELOAD_INTERFACE_V1. */
    size_t structSize; /**< Supplied structure size, at least the v1 size. */
    unsigned capabilityFlags; /**< Bitset of eModReloadCapabilityFlags_t. */
    modReloadClassifyV1_t classify; /**< Optional side-effect-free classifier. */
    modReloadPrepareV1_t prepare; /**< Private preparation callback. */
    modReloadCommitV1_t commit; /**< Infallible publication callback. */
    modReloadAbortV1_t abort; /**< Uncommitted-state cleanup callback. */
    modReloadRetireV1_t retire; /**< Quiescent committed-state cleanup callback. */
} modReloadInterfaceV1_t;
/** @brief Version identifier for the optional reload interface. */
#define eMOD_RELOAD_INTERFACE_V1 1
/** @brief Populate the core-owned interface output without transferring resources.
 * @param pInterface Writable core-owned v1 storage with version and size preset.
 * @return RS_RET_OK when populated; any error aborts module loading. Legacy
 * modules remain supported by not exporting this optional entry point.
 */
typedef rsRetVal (*modReloadGetInterfaceV1_t)(modReloadInterfaceV1_t *pInterface);

/** @brief Optional source-lowering companion for modules whose effective
 * configuration cannot be reconstructed generically by the core. */
struct rsReloadCandidate_s;
/** @brief Controller-proven assumptions available during private lowering. */
typedef enum modReloadSourceBuildFlagsV1_e {
    /* The controller proved that globals/base settings used by the module did
     * not change. V1 lowerers may therefore consult activeBase. */
    MOD_RELOAD_SOURCE_BASE_UNCHANGED = 1U << 0
} modReloadSourceBuildFlagsV1_t;
/** @brief Borrowed inputs valid only for the duration of buildCandidate(). */
typedef struct modReloadSourceBuildContextV1_s {
    unsigned version; /**< MOD_RELOAD_SOURCE_BUILD_CONTEXT_V1. */
    size_t structSize; /**< Available context bytes, for versioned extension. */
    unsigned flags; /**< Controller proofs from modReloadSourceBuildFlagsV1_t. */
    const struct rsReloadCandidate_s *sourceCatalog; /**< Immutable module/input syntax catalog. */
    const rsconf_t *activeBase; /**< Borrowed active base; consult only with BASE_UNCHANGED. */
} modReloadSourceBuildContextV1_t;
#define MOD_RELOAD_SOURCE_BUILD_CONTEXT_V1 1
/** @brief Lower a catalog into a privately owned effective snapshot.
 *
 * buildCandidate borrows the context and catalog and publishes output only on
 * success. The controller owns that effective source snapshot and keeps the
 * module referenced. It lowers both the published active source catalog and
 * the candidate catalog for comparison, so consumed runtime module configs are
 * never used as baselines. Classification and preparation only borrow these
 * snapshots; preparation must deep-own anything retained. The controller
 * destroys both snapshots after prepare and atomically publishes the candidate
 * source catalog only if commit succeeds. */
typedef rsRetVal (*modReloadSourceBuildV1_t)(const modReloadSourceBuildContextV1_t *context, void **pCandidateCnf);
/** @brief Destroy a module-allocated snapshot and clear its caller-owned pointer. */
typedef void (*modReloadSourceDestructV1_t)(void **pCandidateCnf);
/** @brief Compare borrowed snapshots without mutating them or live configuration.
 * @param pOldCnf Independently lowered active source snapshot.
 * @param pNewCnf Independently lowered candidate source snapshot.
 * @param pCapability Output capability; failure must not authorize activation.
 * @return RS_RET_OK for a classification, or the lowering-specific error.
 */
typedef rsRetVal (*modReloadSourceClassifyV1_t)(const void *pOldCnf,
                                                const void *pNewCnf,
                                                eModReloadCapability_t *pCapability);
/** @brief Size-versioned source companion; module must outlive its snapshots. */
typedef struct modReloadSourceInterfaceV1_s {
    unsigned version; /**< eMOD_RELOAD_SOURCE_INTERFACE_V1. */
    size_t structSize; /**< Available bytes; older providers may omit classification. */
    modReloadSourceBuildV1_t buildCandidate; /**< Required private snapshot builder. */
    modReloadSourceDestructV1_t destructCandidate; /**< Required matching destructor. */
    /** Optional exact comparison of two independently lowered snapshots. */
    modReloadSourceClassifyV1_t classifyCandidate;
} modReloadSourceInterfaceV1_t;
#define eMOD_RELOAD_SOURCE_INTERFACE_V1 1
typedef rsRetVal (*modReloadSourceGetInterfaceV1_t)(modReloadSourceInterfaceV1_t *pInterface);

struct modInfo_s {
    struct modInfo_s *pPrev; /* support for creating a double linked module list */
    struct modInfo_s *pNext; /* support for creating a linked module list */
    int iIFVers; /* Interface version of module */
    eModType_t eType; /* type of this module */
    eModLinkType_t eLinkType;
    eModKeepType_t eKeepType; /* keep the module dynamically linked on unload */
    uchar *pszName; /* printable module name, e.g. for dbgprintf */
    uchar *cnfName; /* name to be used in config statements (e.g. 'name="omusrmsg"') */
    unsigned uRefCnt; /* reference count for this module; 0 -> may be unloaded */
    sbool bSetModCnfCalled; /* is setModCnf already called? Needed for built-in modules */
    /* functions supported by all types of modules */
    rsRetVal (*modInit)(int, int *, rsRetVal (**)(void *)); /* initialize the module */
    /* be sure to support version handshake! */
    rsRetVal (*modQueryEtryPt)(uchar *name, rsRetVal (**EtryPoint)()); /* query entry point addresses */
    rsRetVal (*isCompatibleWithFeature)(syslogFeature);
    rsRetVal (*freeInstance)(void *); /* called before termination or module unload */
    rsRetVal (*dbgPrintInstInfo)(void *); /* called before termination or module unload */
    rsRetVal (*tryResume)(void *); /* called to see if module actin can be resumed now */
    rsRetVal (*modExit)(void); /* called before termination or module unload */
    rsRetVal (*modGetID)(void **); /* get its unique ID from module */
    rsRetVal (*doHUP)(void *); /* HUP handler, action level */
    rsRetVal (*doHUPWrkr)(void *); /* HUP handler, wrkr instance level */
    /* v2 config system specific */
    rsRetVal (*beginCnfLoad)(void *newCnf, rsconf_t *pConf);
    rsRetVal (*setModCnf)(struct nvlst *lst);
    rsRetVal (*endCnfLoad)(void *Cnf);
    rsRetVal (*checkCnf)(void *Cnf);
    rsRetVal (*activateCnfPrePrivDrop)(void *Cnf);
    rsRetVal (*activateCnf)(void *Cnf); /* make provided config the running conf */
    rsRetVal (*freeCnf)(void *Cnf);
    /* end v2 config system specific */
    union {
        struct { /* data for input modules */
            /* TODO: remove? */ rsRetVal (*willRun)(void); /* check if the current config will be able to run*/
            rsRetVal (*runInput)(thrdInfo_t *); /* function to gather input and submit to queue */
            rsRetVal (*afterRun)(thrdInfo_t *); /* function to gather input and submit to queue */
            rsRetVal (*newInpInst)(struct nvlst *lst);
            int bCanRun; /* cached value of whether willRun() succeeded */
        } im;
        struct { /* data for output modules */
            /* below: perform the configured action
             */
            rsRetVal (*beginTransaction)(void *);
            rsRetVal (*commitTransaction)(void *const, actWrkrIParams_t *const, const unsigned);
            rsRetVal (*doAction)(void **params, void *pWrkrData);
            rsRetVal (*endTransaction)(void *);
            rsRetVal (*parseSelectorAct)(uchar **, void **, omodStringRequest_t **);
            rsRetVal (*newActInst)(uchar *modName, struct nvlst *lst, void **, omodStringRequest_t **);
            rsRetVal (*SetShutdownImmdtPtr)(void *pData, void *pPtr);
            rsRetVal (*createWrkrInstance)(void *ppWrkrData, void *pData);
            rsRetVal (*freeWrkrInstance)(void *pWrkrData);
            rsRetVal (*setActionInfo)(void *pData, action_t *pAction);
            sbool supportsTX; /* set if the module supports transactions */
        } om;
        struct { /* data for library modules */
            char dummy;
        } lm;
        struct { /* data for parser modules */
            rsRetVal (*newParserInst)(struct nvlst *lst, void *pinst);
            rsRetVal (*checkParserInst)(void *pinst);
            rsRetVal (*freeParserInst)(void *pinst);
            rsRetVal (*parse2)(instanceConf_t *const, smsg_t *);
            rsRetVal (*parse)(smsg_t *);
        } pm;
        struct { /* data for strgen modules */
            rsRetVal (*strgen)(const smsg_t *const, actWrkrIParams_t *const iparam);
        } sm;
        struct { /* data for rscript modules */
            rsRetVal (*getFunctArray)(int *const, struct scriptFunct **);
        } fm;
    } mod;
    void *pModHdlr; /* handler to the dynamic library holding the module */
#ifdef DEBUG
    /* we add some home-grown support to track our users (and detect who does not free us). */
    modUsr_t *pModUsrRoot;
#endif
    /** Optional reload lifecycle; unavailable hooks mean restart required. */
    modReloadInterfaceV1_t reloadV1;
    /* Optional private source lowerer used before reload classification. */
    modReloadSourceInterfaceV1_t reloadSourceV1;
};

/** @brief Test the complete lifecycle required for a mutating reload transition.
 * @param pMod Borrowed module record, or NULL for unsupported.
 * @return True only for a valid interface, complete flags, and lifecycle hooks.
 * REUSE changes no runtime state: it requires a valid classifier and
 * eMOD_RELOAD_CAP_REUSE but not prepare/commit/retire hooks. Use
 * modReloadClassify() to apply that distinction conservatively.
 */
sbool modReloadHasLifecycleHooks(const modInfo_t *pMod);
/** @brief Check version and minimum size without invoking callbacks.
 * @param pMod Borrowed module record; NULL is accepted.
 * @return True for a supported v1 header, false otherwise.
 */
sbool modReloadHasValidInterfaceV1(const modInfo_t *pMod);
/** @brief Check source companion version, size, builder, and destructor. */
sbool modReloadHasValidSourceInterfaceV1(const modInfo_t *pMod);
/** @brief Build a privately owned snapshot; clean up partial output on failure.
 * @param pMod Referenced module, retained until snapshot destruction.
 * @param context Borrowed catalog/base context for the callback duration.
 * @param pCandidateCnf Non-NULL output address initially containing NULL.
 * @return RS_RET_OK with non-NULL output, or an error with output cleaned up.
 */
rsRetVal modReloadBuildSourceCandidateV1(const modInfo_t *pMod,
                                         const modReloadSourceBuildContextV1_t *context,
                                         void **pCandidateCnf);
/** @brief Release a snapshot through its still-referenced module; NULL is harmless. */
void modReloadDestructSourceCandidateV1(const modInfo_t *pMod, void **pCandidateCnf);
/** @brief Compare borrowed source snapshots, defaulting absent callbacks to restart.
 * @param pMod Referenced module with a valid source companion.
 * @param pOldCnf Borrowed active snapshot, non-NULL.
 * @param pNewCnf Borrowed candidate snapshot, non-NULL.
 * @param pCapability Non-NULL output; restart-required on callback failure.
 * @return Callback status, or RS_RET_PARAM_ERROR for invalid inputs/results.
 */
rsRetVal modReloadClassifySourceCandidateV1(const modInfo_t *pMod,
                                            const void *pOldCnf,
                                            const void *pNewCnf,
                                            eModReloadCapability_t *pCapability);

/** @brief Conservatively classify a change using borrowed immutable objects.
 * @param pMod Borrowed module record, or NULL for unsupported.
 * @param pOldCnf Core-owned old module configuration, passed to the classifier.
 * @param pNewCnf Core-owned candidate configuration, passed to the classifier.
 * @return Restart-required for unsupported, failed, or incomplete interfaces;
 * otherwise the validated module capability. No ownership is transferred.
 * Classify a module change without enabling reload.  Legacy modules and
 * missing, failing, or invalid classifiers are deliberately classified as
 * restart-required.  Callers that need the classifier's exact failure can
 * invoke the validated pMod->reloadV1.classify callback directly after this
 * conservative preflight.
 */
eModReloadCapability_t modReloadClassify(const modInfo_t *pMod, const void *pOldCnf, const void *pNewCnf);


/* interfaces */
BEGINinterface(module) /* name must also be changed in ENDinterface macro! */
    modInfo_t *(*GetNxt)(modInfo_t *pThis);
    cfgmodules_etry_t *(*GetNxtCnfType)(rsconf_t *cnf, cfgmodules_etry_t *pThis, eModType_t rqtdType);
    uchar *(*GetName)(modInfo_t *pThis);
    uchar *(*GetStateName)(modInfo_t *pThis);
    rsRetVal (*Use)(const char *srcFile, modInfo_t *pThis);
    /**< must be called before a module is used (ref counting) */
    rsRetVal (*Release)(const char *srcFile, modInfo_t **ppThis); /**< release a module (ref counting) */
    void (*PrintList)(void);
    rsRetVal (*UnloadAndDestructAll)(eModLinkType_t modLinkTypesToUnload);
    rsRetVal (*doModInit)(rsRetVal (*modInit)(), uchar *name, void *pModHdlr, modInfo_t **pNew);
    rsRetVal (*Load)(uchar *name, sbool bConfLoad, struct nvlst *lst);
    rsRetVal (*SetModDir)(uchar *name);
    modInfo_t *(*FindWithCnfName)(rsconf_t *cnf, uchar *name, eModType_t rqtdType); /* added v3, 2011-07-19 */
ENDinterface(module)
#define moduleCURR_IF_VERSION 5 /* increment whenever you change the interface structure! */
/* Changes:
 * v2
 * - added param bCondLoad to Load call - 2011-04-27
 * - removed GetNxtType, added GetNxtCnfType - 2011-04-27
 * v3 (see above)
 * v4
 * - added third parameter to Load() - 2012-06-20
 */

/* prototypes */
PROTOTYPEObj(module);
/* in v6, we go back to in-core static link for core objects, at least those
 * that are not called from plugins.
 * ... and we need to know that none of the module functions are called from plugins!
 * rgerhards, 2012-09-24
 */
rsRetVal modulesProcessCnf(struct cnfobj *o);
uchar *modGetName(modInfo_t *pThis);
rsRetVal ATTR_NONNULL(1) addModToCnfList(cfgmodules_etry_t **pNew, cfgmodules_etry_t *pLast);
rsRetVal readyModForCnf(modInfo_t *pThis, cfgmodules_etry_t **ppNew, cfgmodules_etry_t **ppLast);
void modDoHUP(void);

#endif /* #ifndef MODULES_H_INCLUDED */
