/* reload-ruleset-materializer.c
 *
 * B1 prepares private ruleset roots and binds only unchanged named actions.
 * It deliberately provides no activation API and never mutates active state.
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

typedef struct reloadActionBindingV1_s {
    action_t *action;
    struct cnfstmt *activeOwner;
    char *name;
    char *fingerprint;
    struct reloadActionBindingV1_s *next;
} reloadActionBindingV1_t;

typedef struct reloadPlanEntryV1_s {
    char *identity;
    ruleset_t *runtime;
    struct cnfstmt *root;
    struct reloadPlanEntryV1_s *next;
} reloadPlanEntryV1_t;

typedef struct reloadActionTransferV1_s {
    struct cnfstmt *activeOwner;
    struct cnfstmt *preparedOwner;
    action_t *action;
    struct reloadActionTransferV1_s *next;
} reloadActionTransferV1_t;

struct rsReloadRulesetPlanV1_s {
    reloadPlanEntryV1_t *head;
    reloadPlanEntryV1_t *tail;
    reloadActionTransferV1_t *actionTransfers;
    size_t count;
};

typedef struct prepareContextV1_s {
    rsconf_t *active;
    const rsReloadReportV1_t *report;
    reloadActionBindingV1_t *actions;
    rsReloadRulesetPlanV1_t *plan;
} prepareContextV1_t;

static const rsReloadReportEntryV1_t *reportEntryAt(const rsReloadReportV1_t *report, const size_t index) {
    return (const rsReloadReportEntryV1_t *)((const unsigned char *)report->entries + index * report->entryStride);
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

rsRetVal rsReloadRulesetPlanPrepareV1(rsconf_t *active,
                                      const rsReloadCandidate_t *candidate,
                                      const rsReloadReportV1_t *report,
                                      rsReloadRulesetPlanV1_t **out) {
    prepareContextV1_t prepare = {0};
    rsReloadRulesetPlanV1_t *plan = NULL;
    DEFiRet;
    if (active == NULL || candidate == NULL || report == NULL || out == NULL || *out != NULL) return RS_RET_PARAM_ERROR;
    if (report->entryStride < sizeof(rsReloadReportEntryV1_t)) return RS_RET_PARAM_ERROR;
    CHKiRet(rsReloadCandidateCheckRulesetOnlyReportV1(report));
    CHKmalloc(plan = calloc(1, sizeof(*plan)));
    if (report->modifiedCount == 0) {
        *out = plan;
        plan = NULL;
        FINALIZE;
    }
    prepare.active = active;
    prepare.report = report;
    prepare.plan = plan;
    CHKiRet(rulesetVisitAllActionsV1(active, collectActiveAction, &prepare));
    CHKiRet(rsReloadCandidateVisitRulesetFragmentsV1(candidate, prepareFragment, &prepare));
    if (plan->count != report->modifiedCount) ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
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
