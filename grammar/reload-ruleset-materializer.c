/* reload-ruleset-materializer.c
 *
 * Prepare builds private ruleset roots and binds only unchanged named actions.
 * Activation swaps an already prepared existing-ruleset plan at queue batch
 * boundaries; unsupported dependencies remain fail-closed.
 */
#include "config.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "rsyslog.h"
#include "action.h"
#include "grammar/rainerscript.h"
#include "reload-ruleset-materializer.h"
#include "ruleset.h"
#include "queue.h"

typedef struct reloadActionBindingV1_s {
    action_t *action;
    ruleset_t *ruleset;
    struct cnfstmt *activeOwner;
    char *name;
    char *fingerprint;
    struct reloadActionBindingV1_s *next;
} reloadActionBindingV1_t;

typedef struct reloadPlanEntryV1_s {
    char *identity;
    ruleset_t *runtime;
    struct cnfstmt *root;
    struct cnfstmt *oldRoot;
    struct reloadPlanEntryV1_s *next;
} reloadPlanEntryV1_t;

typedef struct reloadActionTransferV1_s {
    struct cnfstmt *activeOwner;
    struct cnfstmt *preparedOwner;
    action_t *action;
    struct reloadActionTransferV1_s *next;
} reloadActionTransferV1_t;

struct rsReloadRulesetPlanV1_s {
    rsconf_t *active;
    reloadPlanEntryV1_t *head;
    reloadPlanEntryV1_t *tail;
    reloadActionTransferV1_t *actionTransfers;
    size_t count;
    int activated;
};

typedef struct prepareContextV1_s {
    rsconf_t *active;
    const rsReloadReportV1_t *report;
    reloadActionBindingV1_t *actions;
    rsReloadRulesetPlanV1_t *plan;
} prepareContextV1_t;

static rsRetVal rejectCachedSynchronousCall(ruleset_t *ruleset, struct cnfstmt *stmt, void *context);

static const rsReloadReportEntryV1_t *reportEntryAt(const rsReloadReportV1_t *report, const size_t index) {
    const uintptr_t address = (uintptr_t)(const void *)report->entries + index * report->entryStride;
    return (const rsReloadReportEntryV1_t *)(const void *)address;
}

static int reportModifiesRuleset(const rsReloadReportV1_t *report, const char *identity) {
    size_t i;
    for (i = 0; i < report->entryCount; ++i) {
        const rsReloadReportEntryV1_t *const entry = reportEntryAt(report, i);
        if (entry->objectKind == RS_RELOAD_OBJ_RULESET && entry->diffKind == RS_RELOAD_DIFF_MODIFIED &&
            entry->identity != NULL && !strcmp(entry->identity, identity)) {
            return 1;
        }
    }
    return 0;
}

static void actionBindingsDestruct(reloadActionBindingV1_t *binding) {
    while (binding != NULL) {
        reloadActionBindingV1_t *const next = binding->next;
        free(binding->name);
        free(binding->fingerprint);
        free(binding);
        binding = next;
    }
}

static rsRetVal collectActiveAction(ruleset_t *ruleset, struct cnfstmt *owner, action_t *action, void *context) {
    prepareContextV1_t *const prepare = (prepareContextV1_t *)context;
    reloadActionBindingV1_t *binding = NULL;
    const char *syntaxName;
    size_t syntaxNameLen;
    DEFiRet;
    (void)ruleset;

    if (owner == NULL || action == NULL || action->pszName == NULL || action->pSyntaxLst == NULL)
        return RS_RET_NOT_IMPLEMENTED;
    iRet = rsReloadActionSyntaxNameV1(action->pSyntaxLst, &syntaxName, &syntaxNameLen);
    if (iRet == RS_RET_NOT_IMPLEMENTED) {
        iRet = RS_RET_OK;
        FINALIZE;
    }
    CHKiRet(iRet);
    if (strlen((const char *)action->pszName) != syntaxNameLen || memcmp(action->pszName, syntaxName, syntaxNameLen)) {
        return RS_RET_NOT_IMPLEMENTED;
    }
    for (binding = prepare->actions; binding != NULL; binding = binding->next) {
        if (!strcmp(binding->name, (const char *)action->pszName)) return RS_RET_DUP_PARAM;
    }
    CHKmalloc(binding = calloc(1, sizeof(*binding)));
    CHKmalloc(binding->name = strdup((const char *)action->pszName));
    CHKiRet(rsReloadActionSyntaxFingerprintV1(action->pSyntaxLst, &binding->fingerprint));
    binding->action = action;
    binding->ruleset = ruleset;
    binding->activeOwner = owner;
    binding->next = prepare->actions;
    prepare->actions = binding;
    binding = NULL;

finalize_it:
    if (binding != NULL) {
        free(binding->name);
        free(binding->fingerprint);
        free(binding);
    }
    RETiRet;
}

static reloadActionBindingV1_t *findAction(prepareContextV1_t *prepare, const char *name, const size_t nameLen) {
    reloadActionBindingV1_t *binding;
    for (binding = prepare->actions; binding != NULL; binding = binding->next) {
        if (strlen(binding->name) == nameLen && !memcmp(binding->name, name, nameLen)) return binding;
    }
    return NULL;
}

static rsRetVal validatePreparedStatements(const struct cnfstmt *stmt) {
    DEFiRet;

    for (; stmt != NULL; stmt = stmt->next) {
        switch (stmt->nodetype) {
            case S_NOP:
            case S_STOP:
            case S_SET:
            case S_UNSET:
            case S_RELOAD_ACT:
                break;
            case S_IF:
                CHKiRet(validatePreparedStatements(stmt->d.s_if.t_then));
                CHKiRet(validatePreparedStatements(stmt->d.s_if.t_else));
                break;
            case S_FOREACH:
                CHKiRet(validatePreparedStatements(stmt->d.s_foreach.body));
                break;
            case S_RELOAD_PRIFILT:
                CHKiRet(validatePreparedStatements(stmt->d.s_prifilt.t_then));
                CHKiRet(validatePreparedStatements(stmt->d.s_prifilt.t_else));
                break;
            case S_RELOAD_PROPFILT:
                CHKiRet(validatePreparedStatements(stmt->d.s_propfilt.t_then));
                CHKiRet(validatePreparedStatements(stmt->d.s_propfilt.t_else));
                break;
            default:
                ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
        }
    }
finalize_it:
    RETiRet;
}

static rsRetVal lowerPreparedFilters(struct cnfstmt *stmt, rsconf_t *const active) {
    DEFiRet;

    for (; stmt != NULL; stmt = stmt->next) {
        switch (stmt->nodetype) {
            case S_NOP:
            case S_STOP:
            case S_SET:
            case S_UNSET:
            case S_RELOAD_ACT:
                break;
            case S_IF:
                CHKiRet(lowerPreparedFilters(stmt->d.s_if.t_then, active));
                CHKiRet(lowerPreparedFilters(stmt->d.s_if.t_else, active));
                break;
            case S_FOREACH:
                CHKiRet(lowerPreparedFilters(stmt->d.s_foreach.body, active));
                break;
            case S_RELOAD_PRIFILT:
                CHKiRet(cnfstmtLowerReloadFilterV1(stmt, active));
                CHKiRet(lowerPreparedFilters(stmt->d.s_prifilt.t_then, active));
                CHKiRet(lowerPreparedFilters(stmt->d.s_prifilt.t_else, active));
                break;
            case S_RELOAD_PROPFILT:
                CHKiRet(cnfstmtLowerReloadFilterV1(stmt, active));
                CHKiRet(lowerPreparedFilters(stmt->d.s_propfilt.t_then, active));
                CHKiRet(lowerPreparedFilters(stmt->d.s_propfilt.t_else, active));
                break;
            default:
                ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
        }
    }

finalize_it:
    RETiRet;
}

static rsRetVal lowerPreparedStatements(struct cnfstmt *stmt, prepareContextV1_t *prepare) {
    DEFiRet;
    for (; stmt != NULL; stmt = stmt->next) {
        switch (stmt->nodetype) {
            case S_NOP:
            case S_STOP:
            case S_SET:
            case S_UNSET:
                break;
            case S_IF:
                CHKiRet(lowerPreparedStatements(stmt->d.s_if.t_then, prepare));
                CHKiRet(lowerPreparedStatements(stmt->d.s_if.t_else, prepare));
                break;
            case S_FOREACH:
                CHKiRet(lowerPreparedStatements(stmt->d.s_foreach.body, prepare));
                break;
            case S_PRIFILT:
                CHKiRet(lowerPreparedStatements(stmt->d.s_prifilt.t_then, prepare));
                CHKiRet(lowerPreparedStatements(stmt->d.s_prifilt.t_else, prepare));
                break;
            case S_PROPFILT:
                CHKiRet(lowerPreparedStatements(stmt->d.s_propfilt.t_then, prepare));
                CHKiRet(lowerPreparedStatements(stmt->d.s_propfilt.t_else, prepare));
                break;
            case S_RELOAD_ACT: {
                char *fingerprint = NULL;
                reloadActionTransferV1_t *transfer = NULL;
                const char *name;
                size_t nameLen;
                reloadActionBindingV1_t *binding;
                CHKiRet(rsReloadActionSyntaxNameV1(stmt->d.reload_action, &name, &nameLen));
                binding = findAction(prepare, name, nameLen);
                if (binding == NULL) ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
                CHKiRet(rsReloadActionSyntaxFingerprintV1(stmt->d.reload_action, &fingerprint));
                if (strcmp(fingerprint, binding->fingerprint)) {
                    free(fingerprint);
                    ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
                }
                free(fingerprint);
                CHKmalloc(transfer = calloc(1, sizeof(*transfer)));
                transfer->activeOwner = binding->activeOwner;
                transfer->preparedOwner = stmt;
                transfer->action = binding->action;
                nvlstDestruct(stmt->d.reload_action);
                stmt->d.reload_action = NULL;
                stmt->nodetype = S_ACT;
                stmt->flags |= CNFSTMT_FLAG_BORROWED_ACTION;
                stmt->d.act = binding->action;
                transfer->next = prepare->plan->actionTransfers;
                prepare->plan->actionTransfers = transfer;
                break;
            }
            default:
                ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
        }
    }
finalize_it:
    RETiRet;
}

static rsRetVal prepareFragment(
    const char *identity, const struct cnfstmt *fragment, int firstForIdentity, int isSyntheticDefault, void *context) {
    prepareContextV1_t *const prepare = (prepareContextV1_t *)context;
    reloadPlanEntryV1_t *entry = NULL;
    ruleset_t *runtime = NULL;
    const char *name;
    DEFiRet;

    if (!reportModifiesRuleset(prepare->report, identity)) return RS_RET_OK;
    if (!firstForIdentity || isSyntheticDefault || strncmp(identity, "ruleset:", 8)) return RS_RET_NOT_IMPLEMENTED;
    name = identity + 8;
    if (*name == '\0') return RS_RET_PARAM_ERROR;
    CHKiRet(rulesetGetRuleset(prepare->active, &runtime, (uchar *)name));
    if ((runtime->pQueue != NULL && runtime->pQueue->qType == QUEUETYPE_DIRECT) ||
        (runtime->pQueue == NULL && prepare->active->pMsgQueue != NULL &&
         prepare->active->pMsgQueue->qType == QUEUETYPE_DIRECT)) {
        ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
    }
    CHKmalloc(entry = calloc(1, sizeof(*entry)));
    CHKmalloc(entry->identity = strdup(identity));
    entry->runtime = runtime;
    CHKiRet(cnfstmtCloneReloadSafe(fragment, &entry->root));
    CHKiRet(validatePreparedStatements(entry->root));
    CHKiRet(lowerPreparedFilters(entry->root, prepare->active));
    /* Match startup's runtime-required expression canonicalization before
     * binding borrowed actions. In particular, array equality relies on the
     * optimizer to move arrays to the RHS and sort them for bsearch(). */
    entry->root = cnfstmtOptimize(entry->root);
    CHKiRet(lowerPreparedStatements(entry->root, prepare));
    if (prepare->plan->tail == NULL)
        prepare->plan->head = entry;
    else
        prepare->plan->tail->next = entry;
    prepare->plan->tail = entry;
    ++prepare->plan->count;
    entry = NULL;

finalize_it:
    if (entry != NULL) {
        free(entry->identity);
        cnfstmtDestructLst(entry->root);
        free(entry);
    }
    RETiRet;
}

static int planContainsRuleset(const rsReloadRulesetPlanV1_t *const plan, const ruleset_t *const ruleset) {
    const reloadPlanEntryV1_t *entry;

    for (entry = plan->head; entry != NULL; entry = entry->next)
        if (entry->runtime == ruleset) return 1;
    return 0;
}

static size_t planActionOwnerTransferCount(const rsReloadRulesetPlanV1_t *const plan,
                                           const struct cnfstmt *const owner) {
    const reloadActionTransferV1_t *transfer;
    size_t count = 0;

    for (transfer = plan->actionTransfers; transfer != NULL; transfer = transfer->next)
        if (transfer->activeOwner == owner) ++count;
    return count;
}

static rsRetVal requireEveryActiveActionTransfer(const prepareContextV1_t *const prepare) {
    const reloadActionBindingV1_t *binding;

    for (binding = prepare->actions; binding != NULL; binding = binding->next) {
        if (planContainsRuleset(prepare->plan, binding->ruleset) &&
            planActionOwnerTransferCount(prepare->plan, binding->activeOwner) != 1)
            return RS_RET_NOT_IMPLEMENTED;
    }
    return RS_RET_OK;
}

rsRetVal rsReloadRulesetPlanPrepareV1(rsconf_t *active,
                                      const rsReloadCandidate_t *candidate,
                                      const rsReloadReportV1_t *report,
                                      const eModReloadCapability_t sourceCapability,
                                      rsReloadRulesetPlanV1_t **out) {
    prepareContextV1_t prepare = {0};
    rsReloadRulesetPlanV1_t *plan = NULL;
    size_t modifiedRulesets = 0;
    DEFiRet;
    if (active == NULL || candidate == NULL || report == NULL || out == NULL || *out != NULL) return RS_RET_PARAM_ERROR;
    if (report->entryStride < sizeof(rsReloadReportEntryV1_t) ||
        report->entryStride % _Alignof(rsReloadReportEntryV1_t) != 0)
        return RS_RET_PARAM_ERROR;
    if (sourceCapability == eMOD_RELOAD_LIVE_SWAP) {
        CHKiRet(rsReloadCandidateCheckRulesetImtcpReportV1(candidate, report));
    } else {
        CHKiRet(rsReloadCandidateCheckRulesetOnlyReportV1(report));
    }
    for (size_t i = 0; i < report->entryCount; ++i) {
        const uintptr_t address = (uintptr_t)(const void *)report->entries + i * report->entryStride;
        const rsReloadReportEntryV1_t *const entry = (const rsReloadReportEntryV1_t *)(const void *)address;
        if (entry->objectKind == RS_RELOAD_OBJ_RULESET && entry->diffKind == RS_RELOAD_DIFF_MODIFIED)
            ++modifiedRulesets;
    }
    CHKmalloc(plan = calloc(1, sizeof(*plan)));
    if (modifiedRulesets == 0) {
        *out = plan;
        plan = NULL;
        FINALIZE;
    }
    prepare.active = active;
    prepare.report = report;
    prepare.plan = plan;
    plan->active = active;
    CHKiRet(rulesetVisitAllActionsV1(active, collectActiveAction, &prepare));
    CHKiRet(rsReloadCandidateVisitRulesetFragmentsV1(candidate, prepareFragment, &prepare));
    if (plan->count != modifiedRulesets) ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
    /* The private optimizer may remove unreachable syntax. Do not let that
     * silently retire an active action (and its queue) while the normalized
     * action node is otherwise reported unchanged. Action replacement/removal
     * belongs to a later lifecycle-capable release. */
    CHKiRet(requireEveryActiveActionTransfer(&prepare));
    /* Cached direct calls from unchanged rulesets and all indirect calls can
     * execute a root being replaced from a queue outside this plan. Keep this
     * deterministic limitation in the capability/prepare phase, before any
     * queue is paused. */
    CHKiRet(rulesetVisitAllStatementsV1(active, rejectCachedSynchronousCall, plan));
    *out = plan;
    plan = NULL;

finalize_it:
    actionBindingsDestruct(prepare.actions);
    rsReloadRulesetPlanDestructV1(&plan);
    RETiRet;
}

void rsReloadRulesetPlanDestructV1(rsReloadRulesetPlanV1_t **plan) {
    reloadPlanEntryV1_t *entry;
    reloadActionTransferV1_t *transfer;
    if (plan == NULL || *plan == NULL) return;
    entry = (*plan)->head;
    while (entry != NULL) {
        reloadPlanEntryV1_t *const next = entry->next;
        free(entry->identity);
        cnfstmtDestructLst(entry->root);
        free(entry);
        entry = next;
    }
    transfer = (*plan)->actionTransfers;
    while (transfer != NULL) {
        reloadActionTransferV1_t *const next = transfer->next;
        free(transfer);
        transfer = next;
    }
    free(*plan);
    *plan = NULL;
}

size_t rsReloadRulesetPlanCountV1(const rsReloadRulesetPlanV1_t *plan) {
    return plan == NULL ? 0 : plan->count;
}

static struct cnfstmt *lastStatement(struct cnfstmt *stmt) {
    if (stmt == NULL) return NULL;
    while (stmt->next != NULL) stmt = stmt->next;
    return stmt;
}

static rsRetVal collectAffectedQueues(const rsReloadRulesetPlanV1_t *const plan,
                                      qqueue_t ***const queuesOut,
                                      size_t *const countOut) {
    qqueue_t **queues;
    reloadPlanEntryV1_t *entry;
    size_t count = 0;

    if (plan == NULL || queuesOut == NULL || *queuesOut != NULL || countOut == NULL) return RS_RET_PARAM_ERROR;
    if (plan->count == 0) return RS_RET_OK;
    if ((queues = calloc(plan->count, sizeof(*queues))) == NULL) return RS_RET_OUT_OF_MEMORY;
    for (entry = plan->head; entry != NULL; entry = entry->next) {
        qqueue_t *const queue = entry->runtime->pQueue != NULL ? entry->runtime->pQueue : plan->active->pMsgQueue;
        size_t i;

        if (queue == NULL || queue->qType == QUEUETYPE_DIRECT) {
            free(queues);
            return RS_RET_NOT_IMPLEMENTED;
        }
        for (i = 0; i < count && queues[i] != queue; ++i) {
        }
        if (i == count) queues[count++] = queue;
    }
    *queuesOut = queues;
    *countOut = count;
    return RS_RET_OK;
}

static rsRetVal rejectCachedSynchronousCall(ruleset_t __attribute__((unused)) *const ruleset,
                                            struct cnfstmt *const stmt,
                                            void *const context) {
    const rsReloadRulesetPlanV1_t *const plan = context;
    reloadPlanEntryV1_t *entry;

    /* An indirect call can resolve any active ruleset at execution time. Until
     * all possible caller queues join the barrier, retiring any swapped root
     * would race such a caller. */
    if (stmt->nodetype == S_CALL_INDIRECT) return RS_RET_NOT_IMPLEMENTED;
    if (stmt->nodetype != S_CALL || stmt->d.s_call.ruleset != NULL) return RS_RET_OK;
    for (entry = plan->head; entry != NULL; entry = entry->next) {
        if (stmt->d.s_call.stmt == entry->runtime->root) return RS_RET_NOT_IMPLEMENTED;
    }
    return RS_RET_OK;
}

static int compareTimespec(const struct timespec *const left, const struct timespec *const right) {
    if (left->tv_sec != right->tv_sec) return left->tv_sec < right->tv_sec ? -1 : 1;
    if (left->tv_nsec != right->tv_nsec) return left->tv_nsec < right->tv_nsec ? -1 : left->tv_nsec != right->tv_nsec;
    return 0;
}

static int monotonicUsec(uint64_t *const value) {
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0;
    *value = (uint64_t)now.tv_sec * UINT64_C(1000000) + (uint64_t)now.tv_nsec / UINT64_C(1000);
    return 1;
}

static void finishPauseMeasurement(const uint64_t started, uint64_t *const pauseUsec) {
    uint64_t finished;

    if (pauseUsec != NULL && started != 0 && monotonicUsec(&finished) && finished >= started)
        *pauseUsec = finished - started;
}

static rsRetVal waitBarrierCancellable(qqueue_batch_barrier_t *const barrier,
                                       const struct timespec *const deadline,
                                       rsReloadCancellationCheckV1_t const cancelled,
                                       void *const cancelContext) {
    struct timespec now;
    struct timespec slice;
    rsRetVal ret;

    for (;;) {
        if (cancelled != NULL && cancelled(cancelContext)) return RS_RET_NO_RUN;
        if (clock_gettime(CLOCK_REALTIME, &now) != 0 || compareTimespec(&now, deadline) >= 0) return RS_RET_TIMED_OUT;
        slice = now;
        slice.tv_nsec += 100000000L;
        if (slice.tv_nsec >= 1000000000L) {
            ++slice.tv_sec;
            slice.tv_nsec -= 1000000000L;
        }
        if (compareTimespec(&slice, deadline) > 0) slice = *deadline;
        ret = qqueueBatchBarrierWait(barrier, &slice);
        if (ret == RS_RET_OK) return RS_RET_OK;
        if (ret != RS_RET_TIMED_OUT) return ret;
    }
}

rsRetVal rsReloadRulesetPlanActivateV1(rsReloadRulesetPlanV1_t *const plan,
                                       const struct timespec *const deadline,
                                       rsReloadCancellationCheckV1_t const cancelled,
                                       void *const cancelContext,
                                       rsReloadCommitEnterV1_t const enterCommit,
                                       rsReloadCommitLeaveV1_t const leaveCommit,
                                       void *const commitContext,
                                       rsReloadCommitPublishV1_t const publish,
                                       void *const publishContext,
                                       uint64_t *const pauseUsec) {
    qqueue_t **queues = NULL;
    qqueue_batch_barrier_t **barriers = NULL;
    size_t queueCount = 0;
    size_t begun = 0;
    uint64_t pauseStartedUsec = 0;
    reloadPlanEntryV1_t *entry;
    reloadActionTransferV1_t *transfer;
    int commitEntered = 0;
    rsRetVal ret;

    if (plan == NULL || deadline == NULL || plan->activated || ((enterCommit == NULL) != (leaveCommit == NULL)))
        return RS_RET_PARAM_ERROR;
    if (pauseUsec != NULL) *pauseUsec = 0;
    if (cancelled != NULL && cancelled(cancelContext)) return RS_RET_NO_RUN;
    if ((ret = collectAffectedQueues(plan, &queues, &queueCount)) != RS_RET_OK) return ret;
    if (queueCount != 0 && (barriers = calloc(queueCount, sizeof(*barriers))) == NULL) {
        free(queues);
        return RS_RET_OUT_OF_MEMORY;
    }
    if (queueCount != 0) monotonicUsec(&pauseStartedUsec);
    for (; begun < queueCount;) {
        if ((ret = qqueueBatchBarrierBegin(queues[begun], &barriers[begun])) != RS_RET_OK) goto abort;
        ++begun;
    }
    for (size_t i = 0; i < begun; ++i) {
        if ((ret = waitBarrierCancellable(barriers[i], deadline, cancelled, cancelContext)) != RS_RET_OK) goto abort;
    }
    /* Block termination delivery around the last cancellation check and the
     * infallible pointer publication. A termination already delivered before
     * the block is observed by the check below; one arriving after it remains
     * pending until the complete new generation is visible. */
    if (enterCommit != NULL) {
        if ((ret = enterCommit(commitContext)) != RS_RET_OK) goto abort;
        commitEntered = 1;
    }
    /* No operation below the cancellation check can allocate or fail. */
    if (cancelled != NULL && cancelled(cancelContext)) {
        ret = RS_RET_NO_RUN;
        goto leave_and_abort;
    }
    for (transfer = plan->actionTransfers; transfer != NULL; transfer = transfer->next) {
        transfer->activeOwner->flags |= CNFSTMT_FLAG_BORROWED_ACTION;
        transfer->preparedOwner->flags &= ~CNFSTMT_FLAG_BORROWED_ACTION;
    }
    for (entry = plan->head; entry != NULL; entry = entry->next) {
        entry->oldRoot = entry->runtime->root;
        entry->runtime->root = entry->root;
        entry->runtime->last = lastStatement(entry->root);
        entry->root = NULL;
    }
    if (publish != NULL) publish(publishContext);
    plan->activated = 1;
    if (commitEntered) {
        leaveCommit(commitContext);
    }
    for (begun = 0; begun < queueCount; ++begun) qqueueBatchBarrierRelease(&barriers[begun]);
    finishPauseMeasurement(pauseStartedUsec, pauseUsec);
    for (entry = plan->head; entry != NULL; entry = entry->next) {
        cnfstmtDestructLst(entry->oldRoot);
        entry->oldRoot = NULL;
    }
    free(barriers);
    free(queues);
    return RS_RET_OK;

leave_and_abort:
    if (commitEntered) leaveCommit(commitContext);
abort:
    while (begun != 0) qqueueBatchBarrierRelease(&barriers[--begun]);
    finishPauseMeasurement(pauseStartedUsec, pauseUsec);
    free(barriers);
    free(queues);
    return ret;
}
