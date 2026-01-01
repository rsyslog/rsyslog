/* action.h
 * Header file for the action object
 *
 * File begun on 2007-08-06 by RGerhards (extracted from syslogd.c, which
 * was under BSD license at the time of rsyslog fork)
 *
 * Copyright 2007-2026 Adiscon GmbH.
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

/**
 * @file action.h
 * @brief Public API for output actions.
 *
 * Actions represent configured output destinations. Each action may run
 * via its own queue or directly from a worker thread. When supported by
 * the output module, actions treat a batch of messages as a transaction
 * and coordinate commits via this interface. These are not strict
 * database-style transactions; a rollback may not be possible and
 * message delivery follows an "at least once" principle.
 */
#ifndef ACTION_H_INCLUDED
#define ACTION_H_INCLUDED 1

#include "syslogd-types.h"
#include "queue.h"

/* external data */
extern int glbliActionResumeRetryCount;

/**
 * @struct action_s
 * @brief Runtime representation of an output action.
 *
 * Holds configuration and runtime state for a single action.
 *
 * Actions may be processed by multiple worker threads. The primary
 * mutex @c mutAction serializes state changes such as suspension and
 * resume handling. When the associated output module supports
 * transactions, the fields in this structure track in-flight batches
 * and commit status. These transactions are best-effort batches; on
 * failure only partial rollback may be possible, so delivery is
 * guaranteed at least once.
 */
struct action_s {
    time_t f_time; /* used for "max. n messages in m seconds" processing */
    time_t tActNow; /* the current time for an action execution. Initially set to -1 and
               populated on an as-needed basis. This is a performance optimization. */
    time_t tLastExec; /* time this action was last executed */
    int iActionNbr; /* this action's number (ID) */
    sbool bExecWhenPrevSusp; /* execute only when previous action is suspended? */
    sbool bWriteAllMarkMsgs;
    /* should all mark msgs be written (not matter how recent the action was executed)? */
    sbool bReportSuspension; /* should suspension (and reactivation) of the action reported */
    sbool bReportSuspensionCont;
    int bDisabled; /* disable flag; accessed atomically via mutCAS helper */
    sbool isTransactional;
    sbool bCopyMsg;
    int iSecsExecOnceInterval; /* if non-zero, minimum seconds to wait until action is executed again */
    time_t ttResumeRtry; /* when is it time to retry the resume? */
    int iResumeInterval; /* resume interval for this action */
    int iResumeIntervalMax; /* maximum resume interval for this action --> -1: unbounded */
    int iResumeRetryCount; /* how often shall we retry a suspended action? (-1 --> eternal) */
    int iNbrNoExec; /* number of matches that did not yet yield to an exec */
    int iExecEveryNthOccur; /* execute this action only every n-th occurrence (with n=0,1 -> always) */
    int iExecEveryNthOccurTO; /* timeout for n-th occurrence feature */
    time_t tLastOccur; /* time last occurrence was seen (for timing them out) */
    struct modInfo_s *pMod; /* pointer to output module handling this selector */
    void *pModData; /* pointer to module data - content is module-specific */
    sbool bRepMsgHasMsg; /* "message repeated..." has msg fragment in it (0-no, 1-yes) */
    rsRetVal (*submitToActQ)(action_t *, wti_t *, smsg_t *); /* function submit message to action queue */
    rsRetVal (*qConstruct)(struct queue_s *pThis);
    sbool bUsesMsgPassingMode;
    sbool bNeedReleaseBatch; /* do we need to release batch ressources? Depends on ParamPassig modes... */
    int iNumTpls; /* number of array entries for template element below */
    struct template **ppTpl; /* array of template to use - strings must be passed to doAction
                              * in this order. */
    paramPassing_t *peParamPassing; /* mode of parameter passing to action for that template */
    qqueue_t *pQueue; /* action queue */
    pthread_mutex_t mutAction; /* primary action mutex */
    uchar *pszName; /* action name */
    DEF_ATOMIC_HELPER_MUT(mutCAS);
    /* error file */
    const char *pszErrFile;
    int fdErrFile;
    size_t maxErrFileSize;
    size_t currentErrFileSize;
    pthread_mutex_t mutErrFile;
    /* external stat file system */
    const char *pszExternalStateFile;
    /* for per-worker HUP processing */
    pthread_mutex_t mutWrkrDataTable; /* protects table structures */
    void **wrkrDataTable;
    int wrkrDataTableSize;
    int nWrkr;
    /* for statistics subsystem */
    statsobj_t *statsobj;
    STATSCOUNTER_DEF(ctrProcessed, mutCtrProcessed)
    STATSCOUNTER_DEF(ctrFail, mutCtrFail)
    STATSCOUNTER_DEF(ctrSuspend, mutCtrSuspend)
    STATSCOUNTER_DEF(ctrSuspendDuration, mutCtrSuspendDuration)
    STATSCOUNTER_DEF(ctrResume, mutCtrResume)
};

static inline int actionLoadDisabled(action_t *const pAction) {
    return ATOMIC_FETCH_32BIT(&pAction->bDisabled, &pAction->mutCAS);
}

static inline void actionStoreDisabled(action_t *const pAction, const int value) {
    if (value == 0) {
        ATOMIC_STORE_0_TO_INT(&pAction->bDisabled, &pAction->mutCAS);
    } else {
        ATOMIC_STORE_1_TO_INT(&pAction->bDisabled, &pAction->mutCAS);
    }
}

static inline int actionIsDisabled(action_t *const pAction) {
    return actionLoadDisabled(pAction) != 0;
}


/* function prototypes */

/** Create a new action instance. */
rsRetVal actionConstruct(action_t **ppThis);

/** Finalize initialization after configuration parameters were applied. */
rsRetVal actionConstructFinalize(action_t *pThis, struct nvlst *lst);

/** Destroy an action instance and free all associated resources. */
rsRetVal actionDestruct(action_t *pThis);

/** Set the global default resume interval for actions. */
rsRetVal actionSetGlobalResumeInterval(int iNewVal);

/**
 * Execute an action outside of a queue context.
 * Primarily used for historic modules that expect this style.
 */
rsRetVal actionDoAction(action_t *pAction);

/** Enqueue a message for processing by an action. */
rsRetVal actionWriteToAction(action_t *pAction, smsg_t *pMsg, wti_t *);

/** Trigger the HUP handler of an action if provided by the module. */
rsRetVal actionCallHUPHdlr(action_t *pAction);

/** Initialize global resources used by the action subsystem. */
rsRetVal actionClassInit(void);

/**
 * Register a configured action with the ruleset.
 * The action instance becomes owned by the configuration once added.
 */
rsRetVal addAction(action_t **ppAction,
                   modInfo_t *pMod,
                   void *pModData,
                   omodStringRequest_t *pOMSR,
                   struct cnfparamvals *actParams,
                   struct nvlst *lst);

/** Start the message queues of all configured actions. */
rsRetVal activateActions(void);

/** Create a new action from configuration parameters. */
rsRetVal actionNewInst(struct nvlst *lst, action_t **ppAction);
rsRetVal actionProcessCnf(struct cnfobj *o);

/** Commit all outstanding transactions for direct queues. */
void actionCommitAllDirect(wti_t *pWti);

/** Remove a worker instance from the action's bookkeeping. */
void actionRemoveWorker(action_t *const pAction, void *const actWrkrData);

/** Release parameter memory allocated by prepareDoActionParams(). */
void releaseDoActionParams(action_t *const pAction, wti_t *const pWti, int action_destruct);

/** Return the action name; never returns NULL. */
const uchar *actionGetName(const action_t *const pAction);

#endif /* #ifndef ACTION_H_INCLUDED */
