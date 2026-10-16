/* SPDX-License-Identifier: Apache-2.0
 * Exercise production activation with deterministic queue callbacks. Every
 * entry queue (main, unchanged caller, and non-Direct action) must begin before
 * any wait. Cancellation during waiting or immediately before commit must
 * preserve both roots and action ownership and release every begun barrier.
 * Success publishes both roots atomically; Direct entry queues fail closed,
 * while ordinary Direct actions remain covered by their script worker.
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include "../../grammar/reload-ruleset-materializer.c"

/* Preparation is outside this fixture. Abort if activation unexpectedly
 * reaches any parser/catalog dependency; keep the test link portable. */
__attribute__((noreturn)) rsRetVal cnfstmtCloneReloadSafe(const struct cnfstmt *root, struct cnfstmt **out) {
    (void)root;
    (void)out;
    abort();
}
__attribute__((noreturn)) rsRetVal cnfstmtLowerReloadFilterV1(struct cnfstmt *stmt, rsconf_t *conf) {
    (void)stmt;
    (void)conf;
    abort();
}
__attribute__((noreturn)) struct cnfstmt *cnfstmtOptimize(struct cnfstmt *root) {
    (void)root;
    abort();
}
__attribute__((noreturn)) void nvlstDestruct(struct nvlst *list) {
    (void)list;
    abort();
}
__attribute__((noreturn)) void LogError(int error, int code, const char *format, ...) {
    (void)error;
    (void)code;
    (void)format;
    abort();
}
__attribute__((noreturn)) rsRetVal rsReloadActionSyntaxFingerprintV1(const struct nvlst *syntax, char **out) {
    (void)syntax;
    (void)out;
    abort();
}
__attribute__((noreturn)) rsRetVal rsReloadActionSyntaxNameV1(const struct nvlst *syntax,
                                                              const char **out,
                                                              size_t *length) {
    (void)syntax;
    (void)out;
    (void)length;
    abort();
}
__attribute__((noreturn)) rsRetVal rsReloadCandidateCheckAuthorizedReportV1(const rsReloadCandidate_t *active,
                                                                            const rsReloadCandidate_t *candidate,
                                                                            const rsReloadReportV1_t *report,
                                                                            unsigned authorization) {
    (void)active;
    (void)candidate;
    (void)report;
    (void)authorization;
    abort();
}
__attribute__((noreturn)) rsRetVal rsReloadCandidateVisitRulesetFragmentsV1(
    const rsReloadCandidate_t *candidate, rsReloadCandidateRulesetFragmentVisitorV1_t visitor, void *context) {
    (void)candidate;
    (void)visitor;
    (void)context;
    abort();
}
__attribute__((noreturn)) rsRetVal rulesetGetRuleset(rsconf_t *conf, ruleset_t **out, uchar *name) {
    (void)conf;
    (void)out;
    (void)name;
    abort();
}
__attribute__((noreturn)) rsRetVal rulesetVisitAllStatementsV1(rsconf_t *conf,
                                                               rulesetStatementVisitorV1_t visitor,
                                                               void *context) {
    (void)conf;
    (void)visitor;
    (void)context;
    abort();
}

static int namespaceSupported = 1;
int rulesetNameIndexSupportedV1(void) {
    return namespaceSupported;
}

static ruleset_t *privateShell;
static llElt_t *privateNode;
static struct cnfstmt *privateRoot;
static rulesetNameBlockV1_t *privateBlock;
static unsigned privateRootsDestroyed, privateShellsDestroyed, privateBlocksDestroyed;
void rulesetDiscardAdditionV1(ruleset_t **shell, llElt_t **node) {
    if (*shell != privateShell || *node != privateNode || (*shell)->root != NULL) abort();
    ++privateShellsDestroyed;
    free((*shell)->pszName);
    free(*shell);
    free((*node)->pKey);
    free(*node);
    *shell = NULL;
    *node = NULL;
}
__attribute__((noreturn)) void rulesetPublishAdditionV1(rsconf_t *config, llElt_t **node) {
    (void)config;
    (void)node;
    abort();
}
/* This fixture constructs plans directly; namespace preparation is covered
 * by the real name-index unit. Keep an explicit sentinel for unoptimized
 * builds so linking never relies on dead-code elimination. */
__attribute__((noreturn)) rsRetVal rulesetNameIndexPrepareV1(rsconf_t *config,
                                                             ruleset_t *const *shells,
                                                             size_t count,
                                                             rulesetNameBlockV1_t **out) {
    (void)config;
    (void)shells;
    (void)count;
    (void)out;
    abort();
}
void rulesetNameIndexDiscardV1(rulesetNameBlockV1_t **block) {
    if (*block == NULL) return;
    if (*block != privateBlock) abort();
    ++privateBlocksDestroyed;
    free(*block);
    *block = NULL;
}
__attribute__((noreturn)) void rulesetNameIndexPublishV1(rsconf_t *config, rulesetNameBlockV1_t **block) {
    (void)config;
    (void)block;
    abort();
}

__attribute__((noreturn)) rsRetVal rsReloadCandidateVisitObjectsV1(const rsReloadCandidate_t *candidate,
                                                                   rsReloadCandidateObjectVisitorV1_t visitor,
                                                                   void *context) {
    (void)candidate;
    (void)visitor;
    (void)context;
    abort();
}

static ruleset_t shells[3];
static qqueue_t queues[5];
static unsigned begun, waited, released, retired, published, cancelAt;
static int oracleFailure;
static struct cnfstmt oldRoots[2], newRoots[2], oldOwner, newOwner;
static action_t action;

rsRetVal llExecFunc(linkedList_t *list, rsRetVal (*visitor)(void *, void *), void *context) {
    (void)list;
    for (unsigned i = 0; i < 3; ++i) {
        rsRetVal ret = visitor(&shells[i], context);
        if (ret != RS_RET_OK) return ret;
    }
    return RS_RET_OK;
}
rsRetVal rulesetVisitAllActionsV1(rsconf_t *conf, rulesetActionVisitorV1_t visitor, void *context) {
    (void)conf;
    action_t direct = {.pQueue = &queues[4]}, async = {.pQueue = &queues[3]};
    rsRetVal ret = visitor(&shells[0], &oldOwner, &direct, context);
    return ret == RS_RET_OK ? visitor(&shells[0], &oldOwner, &async, context) : ret;
}
rsRetVal qqueueBatchBarrierBegin(qqueue_t *queue, qqueue_batch_barrier_t **barrier) {
    ++begun;
    *barrier = (qqueue_batch_barrier_t *)queue;
    return RS_RET_OK;
}
rsRetVal qqueueBatchBarrierWait(qqueue_batch_barrier_t *barrier, const struct timespec *deadline) {
    (void)barrier;
    (void)deadline;
    if (begun != 4 || released != 0) oracleFailure = 1;
    ++waited;
    return RS_RET_OK;
}
rsRetVal qqueueBatchBarrierRelease(qqueue_batch_barrier_t **barrier) {
    ++released;
    *barrier = NULL;
    return RS_RET_OK;
}
void cnfstmtDestructLst(struct cnfstmt *root) {
    if (root == privateRoot) {
        ++privateRootsDestroyed;
        free(root);
        return;
    }
    if (root == NULL) return;
    if (released != 4 || (root != &oldRoots[0] && root != &oldRoots[1])) oracleFailure = 1;
    ++retired;
}
static int cancelled(void *context) {
    (void)context;
    return cancelAt != 0 && waited >= cancelAt;
}
static void publish(void *context) {
    (void)context;
    if (shells[0].root != &newRoots[0] || shells[1].root != &newRoots[1] || released != 0) oracleFailure = 1;
    ++published;
}
#define CHECK(c)                                                   \
    do {                                                           \
        if (!(c)) {                                                \
            fprintf(stderr, "failed line %d: %s\n", __LINE__, #c); \
            return 1;                                              \
        }                                                          \
    } while (0)
int main(void) {
    rsconf_t conf = {0};
    reloadPlanEntryV1_t second = {.runtime = &shells[1], .root = &newRoots[1]};
    reloadPlanEntryV1_t first = {.runtime = &shells[0], .root = &newRoots[0], .next = &second};
    reloadActionTransferV1_t transfer = {.activeOwner = &oldOwner, .preparedOwner = &newOwner, .action = &action};
    rsReloadRulesetPlanV1_t plan = {
        .active = &conf, .head = &first, .tail = &second, .count = 2, .actionTransfers = &transfer};
    struct timespec deadline;
    /* Addition authorization must be based on a complete name-only source
     * declaration, not merely an added normalized program identity. */
    char addedIdentity[] = "ruleset:mixed";
    rsReloadReportEntryV1_t addedReportEntry = {
        .objectKind = RS_RELOAD_OBJ_RULESET, .diffKind = RS_RELOAD_DIFF_ADDED, .identity = addedIdentity};
    rsReloadReportV1_t addedReport = {
        .entries = &addedReportEntry, .entryCount = 1, .entryStride = sizeof(addedReportEntry)};
    prepareContextV1_t metadata = {.active = &conf, .report = &addedReport};
    struct nvlst nameParameter = {0}, queueParameter = {0};
    struct cnfobj addedObject = {.objType = CNFOBJ_RULESET, .nvlst = &nameParameter};
    nameParameter.name = es_newStrFromCStr("name", 4);
    nameParameter.val.datatype = 'S';
    nameParameter.val.d.estr = es_newStrFromCStr("MiXeD", 5);
    queueParameter.name = es_newStrFromCStr("queue.type", 10);
    queueParameter.val.datatype = 'S';
    queueParameter.val.d.estr = es_newStrFromCStr("LinkedList", 10);
    CHECK(nameParameter.name && nameParameter.val.d.estr && queueParameter.name && queueParameter.val.d.estr);
    conf.rulesets.nameIndex = (rulesetNameBlockV1_t *)&conf;
    namespaceSupported = 0;
    CHECK(validateAddedRulesetObject(&addedObject, 0, &metadata) == RS_RET_NOT_IMPLEMENTED);
    CHECK(metadata.addedNames == NULL && metadata.validatedAdditions == 0);
    namespaceSupported = 1;
    nameParameter.next = &queueParameter;
    CHECK(validateAddedRulesetObject(&addedObject, 0, &metadata) == RS_RET_NOT_IMPLEMENTED);
    CHECK(metadata.addedNames == NULL && metadata.validatedAdditions == 0);
    nameParameter.next = NULL;
    CHECK(validateAddedRulesetObject(&addedObject, 0, &metadata) == RS_RET_OK);
    CHECK(metadata.validatedAdditions == 1 && metadata.addedNames != NULL);
    CHECK(!strcmp(metadata.addedNames->name, "MiXeD") && !strcmp(metadata.addedNames->identity, "ruleset:mixed"));
    CHECK(validateAddedRulesetObject(&addedObject, 0, &metadata) == RS_RET_DUP_PARAM);
    free(metadata.addedNames->name);
    free(metadata.addedNames->identity);
    free(metadata.addedNames);
    es_deleteStr(nameParameter.name);
    es_deleteStr(nameParameter.val.d.estr);
    es_deleteStr(queueParameter.name);
    es_deleteStr(queueParameter.val.d.estr);
    conf.rulesets.nameIndex = NULL;
    for (unsigned i = 0; i < 5; ++i) queues[i].qType = QUEUETYPE_LINKEDLIST;
    queues[4].qType = QUEUETYPE_DIRECT;
    conf.pMsgQueue = &queues[0];
    shells[0].pQueue = &queues[0]; /* Main queue alias must deduplicate. */
    shells[1].pQueue = &queues[1];
    shells[2].pQueue = &queues[2]; /* Unchanged caller outside the plan. */
    shells[0].root = &oldRoots[0];
    shells[1].root = &oldRoots[1];
    newOwner.flags = CNFSTMT_FLAG_BORROWED_ACTION;
    reloadPlanEntryV1_t *addition = calloc(1, sizeof(*addition));
    rsReloadRulesetPlanV1_t *aborted = calloc(1, sizeof(*aborted));
    privateShell = calloc(1, sizeof(*privateShell));
    privateNode = calloc(1, sizeof(*privateNode));
    privateRoot = calloc(1, sizeof(*privateRoot));
    privateBlock = malloc(1);
    CHECK(addition && aborted && privateShell && privateNode && privateRoot && privateBlock);
    privateShell->pszName = (uchar *)strdup("private");
    privateNode->pKey = strdup("private");
    addition->identity = strdup("ruleset:private");
    CHECK(privateShell->pszName && privateNode->pKey && addition->identity);
    privateNode->pData = privateShell;
    addition->runtime = privateShell;
    addition->root = privateRoot;
    addition->detachedNode = privateNode;
    addition->added = 1;
    second.next = addition;
    plan.tail = addition;
    plan.count = 3;
    plan.additions = 1;
    plan.nameBlock = privateBlock;
    for (unsigned phase = 0; phase < 2; ++phase) {
        begun = waited = released = 0;
        cancelAt = phase ? 4 : 1;
        CHECK(clock_gettime(CLOCK_REALTIME, &deadline) == 0);
        deadline.tv_sec += 30;
        CHECK(rsReloadRulesetPlanActivateV1(&plan, &deadline, cancelled, NULL, NULL, NULL, NULL, publish, NULL, NULL) ==
              RS_RET_NO_RUN);
        CHECK(begun == 4 && released == 4 && waited == cancelAt && !oracleFailure);
        CHECK(shells[0].root == &oldRoots[0] && shells[1].root == &oldRoots[1]);
        CHECK(!plan.activated && retired == 0 && published == 0);
        CHECK(oldOwner.flags == 0 && newOwner.flags == CNFSTMT_FLAG_BORROWED_ACTION);
        CHECK(addition->added && addition->root == privateRoot && addition->detachedNode == privateNode);
        CHECK(privateShell->root == NULL && plan.nameBlock == privateBlock);
    }
    /* Cancelled unpublished roots/shells/nodes/block remain private. The real
     * plan destructor must invoke each ownership cleanup exactly once. */
    second.next = NULL;
    plan.tail = &second;
    plan.count = 2;
    plan.additions = 0;
    plan.nameBlock = NULL;
    aborted->head = aborted->tail = addition;
    aborted->count = aborted->additions = 1;
    aborted->nameBlock = privateBlock;
    rsReloadRulesetPlanDestructV1(&aborted);
    CHECK(aborted == NULL && privateRootsDestroyed == 1 && privateShellsDestroyed == 1 && privateBlocksDestroyed == 1);
    rsReloadRulesetPlanDestructV1(&aborted);
    CHECK(privateRootsDestroyed == 1 && privateShellsDestroyed == 1 && privateBlocksDestroyed == 1);
    begun = waited = released = 0;
    cancelAt = 0;
    queues[0].qType = QUEUETYPE_DIRECT;
    CHECK(rsReloadRulesetPlanActivateV1(&plan, &deadline, NULL, NULL, NULL, NULL, NULL, publish, NULL, NULL) ==
          RS_RET_NOT_IMPLEMENTED);
    CHECK(begun == 0 && !plan.activated);
    queues[0].qType = QUEUETYPE_LINKEDLIST;
    queues[2].qType = QUEUETYPE_DIRECT;
    CHECK(rsReloadRulesetPlanActivateV1(&plan, &deadline, NULL, NULL, NULL, NULL, NULL, publish, NULL, NULL) ==
          RS_RET_NOT_IMPLEMENTED);
    CHECK(begun == 0 && !plan.activated);
    queues[2].qType = QUEUETYPE_LINKEDLIST;
    CHECK(rsReloadRulesetPlanActivateV1(&plan, &deadline, NULL, NULL, NULL, NULL, NULL, publish, NULL, NULL) ==
          RS_RET_OK);
    CHECK(begun == 4 && waited == 4 && released == 4 && retired == 2 && published == 1 && !oracleFailure);
    CHECK(plan.activated && oldOwner.flags == CNFSTMT_FLAG_BORROWED_ACTION && newOwner.flags == 0);
    return 0;
}
