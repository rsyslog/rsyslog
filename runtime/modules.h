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

/*
 * Reload capability declared by a module for a particular old/new
 * configuration pair.  A module must never return a less restrictive class
 * unless it can preserve the stated lifecycle guarantee.
 *
 * The enum deliberately describes a capability, not a request to reload. No
 * current caller acts on it; a future configuration-diff manager may use it
 * to decide whether a running process can apply a change.
 */
typedef enum eModReloadCapability_ {
    eMOD_RELOAD_RESTART_REQUIRED = 0, /* the conservative zero/default */
    eMOD_RELOAD_LIVE_SWAP = 1, /* switch existing users to the replacement */
    eMOD_RELOAD_NEW_SESSIONS = 2, /* existing users retain the old configuration */
    eMOD_RELOAD_DRAIN_REPLACE = 3 /* drain old users before using the replacement */
} eModReloadCapability_t;

/*
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
typedef rsRetVal (*modReloadClassifyV1_t)(const void *pOldCnf,
                                          const void *pNewCnf,
                                          eModReloadCapability_t *pCapability);
typedef rsRetVal (*modReloadPrepareV1_t)(const void *pOldCnf, const void *pNewCnf, void **pReloadState);
typedef void (*modReloadCommitV1_t)(void *pReloadState);
typedef void (*modReloadAbortV1_t)(void *pReloadState);
typedef rsRetVal (*modReloadRetireV1_t)(void *pReloadState);
typedef enum eModReloadCapabilityFlags_ {
    eMOD_RELOAD_CAP_VALIDATE_PRIVATE = 1U << 0,
    eMOD_RELOAD_CAP_PREPARE = 1U << 1,
    eMOD_RELOAD_CAP_REUSE = 1U << 2,
    eMOD_RELOAD_CAP_COMMIT = 1U << 3,
    eMOD_RELOAD_CAP_RETIRE = 1U << 4
} eModReloadCapabilityFlags_t;
typedef struct modReloadInterfaceV1_s {
    unsigned version;
    size_t structSize;
    unsigned capabilityFlags;
    modReloadClassifyV1_t classify;
    modReloadPrepareV1_t prepare;
    modReloadCommitV1_t commit;
    modReloadAbortV1_t abort;
    modReloadRetireV1_t retire;
} modReloadInterfaceV1_t;
#define eMOD_RELOAD_INTERFACE_V1 1
typedef rsRetVal (*modReloadGetInterfaceV1_t)(modReloadInterfaceV1_t *pInterface);

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
    /* Optional reload lifecycle; unavailable hooks mean restart required. */
    modReloadInterfaceV1_t reloadV1;
};

/*
 * A future diff manager must use this predicate before accepting any
 * non-restart capability.  It also makes partially implemented optional
 * interfaces conservative.
 */
sbool modReloadHasLifecycleHooks(const modInfo_t *pMod);
sbool modReloadHasValidInterfaceV1(const modInfo_t *pMod);

/*
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
