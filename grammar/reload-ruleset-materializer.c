/* SPDX-License-Identifier: Apache-2.0
 */
/** @file reload-ruleset-materializer.c
 * @brief Private ruleset preparation and atomic batch-boundary activation.
 *
 * Prepare builds private ruleset roots and binds only unchanged named actions.
 * Activation swaps an already prepared existing-ruleset plan at queue batch
 * boundaries; unsupported dependencies remain fail-closed.
 *
 * Concurrency & Locking: a serialized control thread owns the plan. Prepare
 * never mutates live roots. Activate owns queue barriers, then enters the
 * caller's commit fence for pointer/ownership transfers and metadata publish.
 * Every success/error path releases acquired barriers on the owning thread.
 */
#include "config.h"

#include <stdint.h>
#include <limits.h>
#include <strings.h>
#include <stdlib.h>
#include <string.h>

#include "rsyslog.h"
#include "action.h"
#include "grammar/rainerscript.h"
#include "reload-ruleset-materializer.h"
#include "ruleset.h"
#include "ruleset-name-index.h"
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
    llElt_t *detachedNode;
    int added;
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
    size_t additions;
    rulesetNameBlockV1_t *nameBlock;
    int activated;
};

typedef struct reloadAddedNameV1_s {
    char *identity;
    char *name;
    struct reloadAddedNameV1_s *next;
} reloadAddedNameV1_t;

typedef struct prepareContextV1_s {
    rsconf_t *active;
    const rsReloadReportV1_t *report;
    reloadActionBindingV1_t *actions;
    rsReloadRulesetPlanV1_t *plan;
    int preparingAddition;
    size_t validatedAdditions;
    reloadAddedNameV1_t *addedNames;
} prepareContextV1_t;

static rsRetVal validateActiveCall(ruleset_t *ruleset, struct cnfstmt *stmt, void *context);
static rsRetVal collectAffectedQueues(const rsReloadRulesetPlanV1_t *plan, qqueue_t ***queuesOut, size_t *countOut);

static const rsReloadReportEntryV1_t *reportEntryAt(const rsReloadReportV1_t *report, const size_t index) {
    const uintptr_t address = (uintptr_t)(const void *)report->entries + index * report->entryStride;
    return (const rsReloadReportEntryV1_t *)(const void *)address;
}

static int reportChangesRuleset(const rsReloadReportV1_t *report, const char *identity) {
    size_t i;
    for (i = 0; i < report->entryCount; ++i) {
        const rsReloadReportEntryV1_t *const entry = reportEntryAt(report, i);
        if (entry->objectKind == RS_RELOAD_OBJ_RULESET &&
            (entry->diffKind == RS_RELOAD_DIFF_MODIFIED || entry->diffKind == RS_RELOAD_DIFF_ADDED) &&
            entry->identity != NULL && !strcmp(entry->identity, identity)) {
            return entry->diffKind == RS_RELOAD_DIFF_ADDED ? 2 : 1;
        }
    }
    return 0;
}

/* Added shells have no runtime metadata lifecycle in this slice. Validate
 * their complete source declaration before any addition authorization exists. */
static rsRetVal validateAddedRulesetObject(const struct cnfobj *object, size_t ordinal, void *context) {
    prepareContextV1_t *const prepare = context;
    const struct nvlst *nameParam = NULL;
    char *name = NULL;
    char *identity = NULL;
    size_t parameters = 0;
    DEFiRet;
    (void)ordinal;
    if (object->objType != CNFOBJ_RULESET || object->nvlst == NULL) return RS_RET_OK;
    for (const struct nvlst *param = object->nvlst; param != NULL; param = param->next) {
        char *key;
        if (param->name == NULL) return RS_RET_PARAM_ERROR;
        CHKmalloc(key = es_str2cstr(param->name, NULL));
        if (!strcasecmp(key, "name")) nameParam = param;
        free(key);
        ++parameters;
    }
    if (nameParam == NULL || nameParam->val.datatype != 'S' || nameParam->val.d.estr == NULL) return RS_RET_PARAM_ERROR;
    CHKmalloc(name = es_str2cstr(nameParam->val.d.estr, NULL));
    if (strlen(name) > SIZE_MAX - 9) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    CHKmalloc(identity = malloc(strlen(name) + 9));
    strcpy(identity, "ruleset:");
    strcpy(identity + 8, name);
    for (char *p = identity + 8; *p; ++p)
        if (*p >= 'A' && *p <= 'Z') *p += 'a' - 'A';
    if (reportChangesRuleset(prepare->report, identity) != 2) FINALIZE;
    if (parameters != 1 || object->subobjs != NULL || *name == '\0') ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
    if (!rulesetNameIndexSupportedV1() || prepare->active->rulesets.nameIndex == NULL)
        ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
    for (reloadAddedNameV1_t *known = prepare->addedNames; known != NULL; known = known->next)
        if (!strcmp(known->identity, identity)) ABORT_FINALIZE(RS_RET_DUP_PARAM);
    reloadAddedNameV1_t *addedName;
    CHKmalloc(addedName = calloc(1, sizeof(*addedName)));
    addedName->identity = identity;
    addedName->name = name;
    addedName->next = prepare->addedNames;
    prepare->addedNames = addedName;
    identity = name = NULL;
    ++prepare->validatedAdditions;
finalize_it:
    free(identity);
    free(name);
    RETiRet;
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

    if (owner == NULL || action == NULL || action->pszName == NULL || action->pSyntaxLst == NULL)
        return RS_RET_NOT_IMPLEMENTED;
    iRet = rsReloadActionSyntaxNameV1(action->pSyntaxLst, &syntaxName, &syntaxNameLen);
    if (iRet == RS_RET_NOT_IMPLEMENTED) {
        /* An unnamed action cannot transfer ownership. Reject a modified
         * owner even if private optimization would erase its action syntax;
         * unnamed actions in untouched rulesets remain independently owned. */
        for (size_t i = 0; i < prepare->report->entryCount; ++i) {
            const rsReloadReportEntryV1_t *const entry = reportEntryAt(prepare->report, i);
            if (entry->objectKind == RS_RELOAD_OBJ_RULESET && entry->diffKind == RS_RELOAD_DIFF_MODIFIED &&
                entry->identity != NULL && !strncmp(entry->identity, "ruleset:", 8) &&
                !strcmp(entry->identity + 8, (const char *)rulesetGetName(ruleset)))
                return RS_RET_NOT_IMPLEMENTED;
        }
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

static rsRetVal resolvePreparedTarget(void *context, ruleset_t **target, uchar *name) {
    prepareContextV1_t *const prepare = context;
    for (reloadPlanEntryV1_t *entry = prepare->plan->head; entry != NULL; entry = entry->next)
        if (entry->added && !strcasecmp(entry->identity + 8, (char *)name)) {
            *target = entry->runtime;
            return RS_RET_OK;
        }
    return rulesetGetRuleset(prepare->active, target, name);
}
static rsRetVal validatePreparedCall(struct cnfstmt *stmt, prepareContextV1_t *prepare) {
    ruleset_t *target = NULL;
    char *name;
    rsRetVal ret;
    if (stmt->d.s_call.name == NULL || stmt->d.s_call.target == NULL) return RS_RET_NOT_IMPLEMENTED;
    if ((name = es_str2cstr(stmt->d.s_call.name, NULL)) == NULL) return RS_RET_OUT_OF_MEMORY;
    ret = resolvePreparedTarget(prepare, &target, (uchar *)name);
    free(name);
    if (ret != RS_RET_OK) return ret;
    return target == stmt->d.s_call.target && stmt->d.s_call.ruleset == (rulesetHasQueue(target) ? target : NULL)
               ? RS_RET_OK
               : RS_RET_NOT_IMPLEMENTED;
}

static rsRetVal validatePreparedStatements(const struct cnfstmt *stmt) {
    DEFiRet;

    for (; stmt != NULL; stmt = stmt->next) {
        switch (stmt->nodetype) {
            case S_NOP:
            case S_STOP:
            case S_SET:
            case S_UNSET:
            case S_CALL_INDIRECT:
            case S_RELOAD_ACT:
            case S_CALL:
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
            case S_CALL_INDIRECT:
            case S_RELOAD_ACT:
            case S_CALL:
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
            case S_CALL_INDIRECT:
                break;
            case S_CALL:
                CHKiRet(validatePreparedCall(stmt, prepare));
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
                break;
            case S_RELOAD_ACT: {
                if (prepare->preparingAddition) ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
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

#include "reload-call-binding.h"

static rsRetVal prepareShellFragment(
    const char *identity, const struct cnfstmt *fragment, int firstForIdentity, int isSyntheticDefault, void *context) {
    prepareContextV1_t *const prepare = context;
    reloadPlanEntryV1_t *entry = NULL;
    const int change = reportChangesRuleset(prepare->report, identity);
    DEFiRet;
    (void)fragment;
    if (!change) return RS_RET_OK;
    if (!firstForIdentity || isSyntheticDefault || strncmp(identity, "ruleset:", 8) || identity[8] == '\0')
        return RS_RET_NOT_IMPLEMENTED;
    CHKmalloc(entry = calloc(1, sizeof(*entry)));
    CHKmalloc(entry->identity = strdup(identity));
    entry->added = change == 2;
    if (entry->added) {
        ruleset_t *existing = NULL;
        if (!rulesetNameIndexSupportedV1() || prepare->active->rulesets.nameIndex == NULL)
            ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
        if (rulesetGetRuleset(prepare->active, &existing, (uchar *)identity + 8) != RS_RET_NOT_FOUND)
            ABORT_FINALIZE(RS_RET_DUP_PARAM);
        if (prepare->plan->additions >= (size_t)INT_MAX || prepare->active->rulesets.llRulesets.iNumElts < 0 ||
            prepare->active->rulesets.llRulesets.iNumElts > INT_MAX - (int)prepare->plan->additions - 1)
            ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
        reloadAddedNameV1_t *declared;
        for (declared = prepare->addedNames; declared != NULL && strcmp(declared->identity, identity);
             declared = declared->next) {
        }
        if (declared == NULL) ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
        CHKiRet(rulesetPrepareAdditionV1(declared->name, &entry->runtime, &entry->detachedNode));
    } else {
        CHKiRet(rulesetGetRuleset(prepare->active, &entry->runtime, (uchar *)identity + 8));
    }
    if ((entry->runtime->pQueue != NULL && entry->runtime->pQueue->qType == QUEUETYPE_DIRECT) ||
        (entry->runtime->pQueue == NULL && prepare->active->pMsgQueue != NULL &&
         prepare->active->pMsgQueue->qType == QUEUETYPE_DIRECT))
        ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
    if (prepare->plan->tail == NULL)
        prepare->plan->head = entry;
    else
        prepare->plan->tail->next = entry;
    prepare->plan->tail = entry;
    ++prepare->plan->count;
    if (entry->added) ++prepare->plan->additions;
    entry = NULL;
finalize_it:
    if (entry != NULL) {
        if (entry->added) rulesetDiscardAdditionV1(&entry->runtime, &entry->detachedNode);
        free(entry->identity);
        free(entry);
    }
    RETiRet;
}
static rsRetVal prepareFragment(
    const char *identity, const struct cnfstmt *fragment, int firstForIdentity, int isSyntheticDefault, void *context) {
    prepareContextV1_t *const prepare = context;
    reloadPlanEntryV1_t *entry;
    DEFiRet;
    (void)firstForIdentity;
    (void)isSyntheticDefault;
    if (!reportChangesRuleset(prepare->report, identity)) return RS_RET_OK;
    for (entry = prepare->plan->head; entry != NULL && strcmp(entry->identity, identity); entry = entry->next) {
    }
    if (entry == NULL) return RS_RET_NOT_IMPLEMENTED;
    prepare->preparingAddition = entry->added;
    CHKiRet(cnfstmtCloneReloadSafe(fragment, &entry->root));
    CHKiRet(validatePreparedStatements(entry->root));
    CHKiRet(lowerPreparedFilters(entry->root, prepare->active));
    CHKiRet(bindPreparedCallsWithResolver(entry->root, resolvePreparedTarget, prepare));
    entry->root = cnfstmtOptimize(entry->root);
    CHKiRet(lowerPreparedStatements(entry->root, prepare));
finalize_it:
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
                                      const rsReloadCandidate_t *activeSourceCatalog,
                                      const rsReloadCandidate_t *candidate,
                                      const rsReloadReportV1_t *report,
                                      const eModReloadCapability_t sourceCapability,
                                      const int baseAuthorized,
                                      rsReloadRulesetPlanV1_t **out) {
    prepareContextV1_t prepare = {0};
    rsReloadRulesetPlanV1_t *plan = NULL;
    size_t changedRulesets = 0;
    DEFiRet;
    if (active == NULL || candidate == NULL || report == NULL || out == NULL || *out != NULL) return RS_RET_PARAM_ERROR;
    /* Local and mixed graphs have producer-owned FE workers and cached routes
     * outside the legacy queue barrier. Reject before preparing any live swap
     * until their complete graph can participate in the activation fence. */
    if (active->local_queue_graph != NULL && (report->modifiedCount != 0 || report->addedCount != 0)) {
        LogError(0, RS_RET_NOT_IMPLEMENTED,
                 "reload unsupported for active local-queue graph, including mixed boundaries; "
                 "ruleset roots unchanged; restart to apply configuration");
        return RS_RET_NOT_IMPLEMENTED;
    }
    if (report->entryStride < sizeof(rsReloadReportEntryV1_t) ||
        report->entryStride % _Alignof(rsReloadReportEntryV1_t) != 0)
        return RS_RET_PARAM_ERROR;
    prepare.active = active;
    prepare.report = report;
    CHKiRet(rsReloadCandidateVisitObjectsV1(candidate, validateAddedRulesetObject, &prepare));
    unsigned authorizations = baseAuthorized ? RS_RELOAD_AUTHORIZE_BASE_V1 : 0;
    if (sourceCapability == eMOD_RELOAD_REUSE || sourceCapability == eMOD_RELOAD_LIVE_SWAP ||
        sourceCapability == eMOD_RELOAD_NEW_SESSIONS || sourceCapability == eMOD_RELOAD_LIVE_AND_NEW_SESSIONS ||
        sourceCapability == eMOD_RELOAD_DRAIN_REPLACE)
        authorizations |= RS_RELOAD_AUTHORIZE_IMTCP_V1;
    if (prepare.validatedAdditions == 0) {
        CHKiRet(rsReloadCandidateCheckAuthorizedReportV1(activeSourceCatalog, candidate, report, authorizations));
    } else {
        /* Membership additions cannot be combined with input retargeting or
         * any other lifecycle change. Modified metadata never gains admission. */
        for (size_t i = 0; i < report->entryCount; ++i) {
            const rsReloadReportEntryV1_t *const entry = reportEntryAt(report, i);
            if (entry->diffKind == RS_RELOAD_DIFF_UNCHANGED) continue;
            if (entry->objectKind == RS_RELOAD_OBJ_RULESET &&
                (entry->diffKind == RS_RELOAD_DIFF_ADDED || entry->diffKind == RS_RELOAD_DIFF_MODIFIED))
                continue;
            if (entry->objectKind == RS_RELOAD_OBJ_RULESET_METADATA && entry->diffKind == RS_RELOAD_DIFF_ADDED &&
                entry->identity != NULL && reportChangesRuleset(report, entry->identity) == 2)
                continue;
            ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
        }
    }
    for (size_t i = 0; i < report->entryCount; ++i) {
        const uintptr_t address = (uintptr_t)(const void *)report->entries + i * report->entryStride;
        const rsReloadReportEntryV1_t *const entry = (const rsReloadReportEntryV1_t *)(const void *)address;
        if (entry->objectKind == RS_RELOAD_OBJ_RULESET &&
            (entry->diffKind == RS_RELOAD_DIFF_MODIFIED || entry->diffKind == RS_RELOAD_DIFF_ADDED))
            ++changedRulesets;
    }
    CHKmalloc(plan = calloc(1, sizeof(*plan)));
    if (changedRulesets == 0) {
        *out = plan;
        plan = NULL;
        FINALIZE;
    }
    prepare.active = active;
    prepare.report = report;
    prepare.plan = plan;
    plan->active = active;
    CHKiRet(rulesetVisitAllActionsV1(active, collectActiveAction, &prepare));
    CHKiRet(rsReloadCandidateVisitRulesetFragmentsV1(candidate, prepareShellFragment, &prepare));
    CHKiRet(rsReloadCandidateVisitRulesetFragmentsV1(candidate, prepareFragment, &prepare));
    if (plan->additions != 0) {
        ruleset_t **shells;
        if (plan->additions > SIZE_MAX / sizeof(*shells)) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        shells = calloc(plan->additions, sizeof(*shells));
        size_t i = 0;
        if (shells == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        for (reloadPlanEntryV1_t *entry = plan->head; entry != NULL; entry = entry->next)
            if (entry->added) shells[i++] = entry->runtime;
        iRet = rulesetNameIndexPrepareV1(active, shells, i, &plan->nameBlock);
        free(shells);
        CHKiRet(iRet);
    }
    if (plan->count != changedRulesets) ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
    /* The private optimizer may remove unreachable syntax. Do not let that
     * silently retire an active action (and its queue) while the normalized
     * action node is otherwise reported unchanged. Action replacement/removal
     * belongs to a later lifecycle-capable release. */
    CHKiRet(requireEveryActiveActionTransfer(&prepare));
    /* Every potential entry queue participates, including unchanged callers.
     * Reject unsupported entry paths before any live queue is paused. */
    CHKiRet(rulesetVisitAllStatementsV1(active, validateActiveCall, active));
    {
        qqueue_t **queues = NULL;
        size_t count = 0;
        iRet = collectAffectedQueues(plan, &queues, &count);
        free(queues);
        CHKiRet(iRet);
    }
    if (plan->additions != prepare.validatedAdditions) ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
    if (plan->additions != 0) {
        /* Only complete private shells, roots, ownership and queue eligibility
         * can grant the explicit additions authorization. */
        CHKiRet(rsReloadCandidateCheckAuthorizedReportV1(activeSourceCatalog, candidate, report,
                                                         authorizations | RS_RELOAD_AUTHORIZE_RULESET_ADDITIONS_V1));
    }
    *out = plan;
    plan = NULL;

finalize_it:
    while (prepare.addedNames != NULL) {
        reloadAddedNameV1_t *const next = prepare.addedNames->next;
        free(prepare.addedNames->identity);
        free(prepare.addedNames->name);
        free(prepare.addedNames);
        prepare.addedNames = next;
    }
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
        if (entry->added) rulesetDiscardAdditionV1(&entry->runtime, &entry->detachedNode);
        free(entry);
        entry = next;
    }
    transfer = (*plan)->actionTransfers;
    while (transfer != NULL) {
        reloadActionTransferV1_t *const next = transfer->next;
        free(transfer);
        transfer = next;
    }
    rulesetNameIndexDiscardV1(&(*plan)->nameBlock);
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

typedef struct reloadQueueSetV1_s {
    qqueue_t **queues;
    size_t count;
} reloadQueueSetV1_t;

static rsRetVal addEntryQueue(reloadQueueSetV1_t *set, qqueue_t *queue) {
    qqueue_t **grown;
    if (queue == NULL || queue->qType == QUEUETYPE_DIRECT) return RS_RET_NOT_IMPLEMENTED;
    for (size_t i = 0; i < set->count; ++i)
        if (set->queues[i] == queue) return RS_RET_OK;
    if (set->count == SIZE_MAX / sizeof(*grown)) return RS_RET_OUT_OF_MEMORY;
    grown = realloc(set->queues, (set->count + 1) * sizeof(*grown));
    if (grown == NULL) return RS_RET_OUT_OF_MEMORY;
    set->queues = grown;
    set->queues[set->count++] = queue;
    return RS_RET_OK;
}

DEFFUNC_llExecFunc(collectRulesetEntryQueue) {
    ruleset_t *const shell = pData;
    /* A shell without its own queue enters through the main queue. */
    return shell->pQueue == NULL ? RS_RET_OK : addEntryQueue(pParam, shell->pQueue);
}

static rsRetVal collectActionEntryQueue(ruleset_t *ruleset, struct cnfstmt *owner, action_t *action, void *context) {
    (void)ruleset;
    (void)owner;
    /* Ordinary Direct actions run under the enclosing script worker. Other
     * action workers may invoke module callbacks that resubmit messages. */
    if (action->pQueue == NULL || action->pQueue->qType == QUEUETYPE_DIRECT) return RS_RET_OK;
    return addEntryQueue(context, action->pQueue);
}

static rsRetVal collectAffectedQueues(const rsReloadRulesetPlanV1_t *const plan,
                                      qqueue_t ***const queuesOut,
                                      size_t *const countOut) {
    reloadQueueSetV1_t set = {0};
    rsRetVal ret;
    if (plan == NULL || queuesOut == NULL || *queuesOut != NULL || countOut == NULL) return RS_RET_PARAM_ERROR;
    if (plan->count == 0) return RS_RET_OK;
    ret = addEntryQueue(&set, plan->active->pMsgQueue);
    if (ret == RS_RET_OK) ret = llExecFunc(&plan->active->rulesets.llRulesets, collectRulesetEntryQueue, &set);
    if (ret == RS_RET_OK) ret = rulesetVisitAllActionsV1(plan->active, collectActionEntryQueue, &set);
    if (ret != RS_RET_OK) {
        free(set.queues);
        return ret;
    }
    *queuesOut = set.queues;
    *countOut = set.count;
    return RS_RET_OK;
}

static rsRetVal validateActiveCall(ruleset_t *ruleset, struct cnfstmt *stmt, void *context) {
    ruleset_t *target = NULL;
    char *name;
    rsRetVal ret;
    (void)ruleset;
    if (stmt->nodetype != S_CALL) return RS_RET_OK;
    if (stmt->d.s_call.name == NULL || stmt->d.s_call.target == NULL) return RS_RET_NOT_IMPLEMENTED;
    name = es_str2cstr(stmt->d.s_call.name, NULL);
    if (name == NULL) return RS_RET_OUT_OF_MEMORY;
    ret = rulesetGetRuleset(context, &target, (uchar *)name);
    free(name);
    if (ret != RS_RET_OK) return ret;
    return target == stmt->d.s_call.target && stmt->d.s_call.ruleset == (rulesetHasQueue(target) ? target : NULL)
               ? RS_RET_OK
               : RS_RET_NOT_IMPLEMENTED;
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
    for (entry = plan->head; entry != NULL; entry = entry->next) {
        if (!entry->added) continue;
        rulesetPublishAdditionV1(plan->active, &entry->detachedNode);
        entry->added = 0; /* Owning list now owns this shell/root. */
    }
    if (plan->nameBlock != NULL) rulesetNameIndexPublishV1(plan->active, &plan->nameBlock);
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
