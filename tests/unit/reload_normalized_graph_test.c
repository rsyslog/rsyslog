/* SPDX-License-Identifier: Apache-2.0
 * Unit coverage for the owned normalized-graph builder.  The oracle verifies
 * that copied identity/fingerprint input survives caller mutation and that the
 * graph view enumerates deterministically; no parser, activation, or daemon
 * lifecycle is involved. Synthetic ruleset AST controls differ in exactly one
 * effective field (elseif, property-filter negation, or queued-call dispatch); their
 * production fingerprints must differ and become equal when restored.
 * Property filters execute only their then branch: a deliberately poisoned
 * unused else field must never be read by the production fingerprint walker.
 */
#include "config.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rsyslog.h"
#include "reload-normalized-graph.h"
#include "reload-report.h"

/* Keep this API unit independent of the full runtime link closure. */
#include "../../runtime/module-reload.c"
#include "../../runtime/reload-normalized-graph.c"
#include "../../runtime/reload-report.c"
#include "../../runtime/reload-ruleset-graph.c"

/* The AST fixtures contain no actions or comparison strings. Keep unrelated
 * daemon dependencies inert and assert that serialization never uses them. */
static int unexpectedDependencyCalls;
uchar *modGetName(modInfo_t __attribute__((unused)) * module) {
    ++unexpectedDependencyCalls;
    return NULL;
}
const uchar *actionGetName(const action_t __attribute__((unused)) * action) {
    ++unexpectedDependencyCalls;
    return NULL;
}
uchar *cstrGetSzStrNoNULL(cstr_t __attribute__((unused)) * string) {
    ++unexpectedDependencyCalls;
    return NULL;
}
#ifndef cstrLen
size_t cstrLen(cstr_t __attribute__((unused)) * string) {
    ++unexpectedDependencyCalls;
    return 0;
}
#endif

#define CHECK(condition)                                                                    \
    do {                                                                                    \
        if (!(condition)) {                                                                 \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            return 1;                                                                       \
        }                                                                                   \
    } while (0)

typedef struct observedNodes_s {
    const rsReloadNormalizedNodeV1_t *nodes[4];
    size_t count;
} observedNodes_t;

static rsRetVal observeNode(const rsReloadNormalizedNodeV1_t *node, void *context) {
    observedNodes_t *observed = context;

    if (observed->count >= sizeof(observed->nodes) / sizeof(observed->nodes[0])) return RS_RET_ERR;
    observed->nodes[observed->count++] = node;
    return RS_RET_OK;
}

static rsRetVal failVisitor(const rsReloadNormalizedNodeV1_t *node, void *context) {
    (void)node;
    (void)context;
    return RS_RET_ERR;
}

int main(void) {
    rsReloadNormalizedGraphBuilderV1_t *oldBuilder = NULL;
    rsReloadNormalizedGraphBuilderV1_t *newBuilder = NULL;
    rsReloadNormalizedGraphV1_t oldGraph;
    rsReloadNormalizedGraphV1_t newGraph;
    rsReloadReportV1_t *report = NULL;
    observedNodes_t observed;
    char identity[16] = "z-rule";
    char fingerprint[16] = "fp-z";
    struct cnfnumval condition = {.nodetype = 'N', .val = 1};
    struct cnfstmt stop = {.nodetype = S_STOP};
    struct cnfstmt ifStmt = {.nodetype = S_IF};
    struct cnfstmt propStmt = {.nodetype = S_PROPFILT};
    ruleset_t fixtureRuleset = {0};
    char *controlFingerprint = NULL;
    char *changedFingerprint = NULL;
    struct cnfstmt callStmt = {.nodetype = S_CALL};
    action_t namedAction = {.pszName = (uchar *)"action-0-builtin:omfile"};
    action_t anonymousAction = {.pszName = (uchar *)"action-0-builtin:omfile", .bNameWasGenerated = 1};

    /* The spelling is identical; only construction provenance distinguishes
     * an explicit generated-looking identity from an anonymous fallback. */
    CHECK(actionHasExplicitName(&namedAction));
    CHECK(!actionHasExplicitName(&anonymousAction));

    callStmt.d.s_call.name = es_newStrFromCStr("callee", 6);
    CHECK(callStmt.d.s_call.name != NULL);
    fixtureRuleset.root = &callStmt;
    CHECK(rsReloadRulesetFingerprintV1(&fixtureRuleset, &controlFingerprint) == RS_RET_OK);
    callStmt.d.s_call.ruleset = &fixtureRuleset;
    CHECK(rsReloadRulesetFingerprintV1(&fixtureRuleset, &changedFingerprint) == RS_RET_OK);
    CHECK(strcmp(controlFingerprint, changedFingerprint) != 0);
    free(changedFingerprint);
    changedFingerprint = NULL;
    callStmt.d.s_call.ruleset = NULL;
    CHECK(rsReloadRulesetFingerprintV1(&fixtureRuleset, &changedFingerprint) == RS_RET_OK);
    CHECK(!strcmp(controlFingerprint, changedFingerprint));
    free(controlFingerprint);
    free(changedFingerprint);
    controlFingerprint = changedFingerprint = NULL;
    es_deleteStr(callStmt.d.s_call.name);

    ifStmt.d.s_if.expr = (struct cnfexpr *)&condition;
    ifStmt.d.s_if.t_then = &stop;
    fixtureRuleset.root = &ifStmt;
    CHECK(rsReloadRulesetFingerprintV1(&fixtureRuleset, &controlFingerprint) == RS_RET_OK);
    CHECK(rsReloadRulesetFingerprintV1(&fixtureRuleset, &changedFingerprint) == RS_RET_OK);
    CHECK(!strcmp(controlFingerprint, changedFingerprint));
    free(changedFingerprint);
    changedFingerprint = NULL;
    ifStmt.d.s_if.is_else_if = 1;
    CHECK(rsReloadRulesetFingerprintV1(&fixtureRuleset, &changedFingerprint) == RS_RET_OK);
    CHECK(strcmp(controlFingerprint, changedFingerprint) != 0);
    free(changedFingerprint);
    changedFingerprint = NULL;
    ifStmt.d.s_if.is_else_if = 0;
    CHECK(rsReloadRulesetFingerprintV1(&fixtureRuleset, &changedFingerprint) == RS_RET_OK);
    CHECK(!strcmp(controlFingerprint, changedFingerprint));
    free(controlFingerprint);
    free(changedFingerprint);
    controlFingerprint = changedFingerprint = NULL;

    propStmt.d.s_propfilt.t_then = &stop;
    fixtureRuleset.root = &propStmt;
    CHECK(rsReloadRulesetFingerprintV1(&fixtureRuleset, &controlFingerprint) == RS_RET_OK);
    CHECK(rsReloadRulesetFingerprintV1(&fixtureRuleset, &changedFingerprint) == RS_RET_OK);
    CHECK(!strcmp(controlFingerprint, changedFingerprint));
    free(changedFingerprint);
    changedFingerprint = NULL;
    propStmt.d.s_propfilt.isNegated = 1;
    CHECK(rsReloadRulesetFingerprintV1(&fixtureRuleset, &changedFingerprint) == RS_RET_OK);
    CHECK(strcmp(controlFingerprint, changedFingerprint) != 0);
    free(changedFingerprint);
    changedFingerprint = NULL;
    propStmt.d.s_propfilt.isNegated = 0;
    CHECK(rsReloadRulesetFingerprintV1(&fixtureRuleset, &changedFingerprint) == RS_RET_OK);
    CHECK(!strcmp(controlFingerprint, changedFingerprint));
    free(changedFingerprint);
    changedFingerprint = NULL;
    /* The real property-filter constructor leaves this unused field
     * uninitialized. Poison it explicitly so reading it fails deterministically
     * instead of relying on allocator contents or sanitizer fill patterns. */
    propStmt.d.s_propfilt.t_else = (struct cnfstmt *)(uintptr_t)1;
    CHECK(rsReloadRulesetFingerprintV1(&fixtureRuleset, &changedFingerprint) == RS_RET_OK);
    CHECK(!strcmp(controlFingerprint, changedFingerprint));
    free(changedFingerprint);
    changedFingerprint = NULL;
    propStmt.d.s_propfilt.t_then = NULL;
    CHECK(rsReloadRulesetFingerprintV1(&fixtureRuleset, &changedFingerprint) == RS_RET_OK);
    CHECK(strcmp(controlFingerprint, changedFingerprint) != 0);
    free(controlFingerprint);
    free(changedFingerprint);
    CHECK(unexpectedDependencyCalls == 0);

    memset(&observed, 0, sizeof(observed));
    CHECK(rsReloadNormalizedGraphBuilderV1Construct(&oldBuilder) == RS_RET_OK);
    CHECK(rsReloadNormalizedGraphBuilderV1Construct(&oldBuilder) == RS_RET_PARAM_ERROR);
    CHECK(rsReloadNormalizedGraphBuilderV1Construct(&newBuilder) == RS_RET_OK);
    CHECK(rsReloadNormalizedGraphBuilderV1Add(oldBuilder, RS_RELOAD_OBJ_RULESET, identity, fingerprint) == RS_RET_OK);
    strcpy(identity, "changed");
    strcpy(fingerprint, "changed");
    CHECK(rsReloadNormalizedGraphBuilderV1Add(oldBuilder, RS_RELOAD_OBJ_ACTION, "a-action", "fp-a") == RS_RET_OK);
    CHECK(rsReloadNormalizedGraphBuilderV1Add(oldBuilder, RS_RELOAD_OBJ_RULESET, "a-rule", "fp-b") == RS_RET_OK);
    CHECK(rsReloadNormalizedGraphBuilderV1Add(oldBuilder, RS_RELOAD_OBJ_COUNT, "bad", "bad") == RS_RET_PARAM_ERROR);
    CHECK(rsReloadNormalizedGraphBuilderV1Add(oldBuilder, RS_RELOAD_OBJ_ACTION, "", "bad") == RS_RET_PARAM_ERROR);
    CHECK(rsReloadNormalizedGraphBuilderV1Add(oldBuilder, RS_RELOAD_OBJ_ACTION, "bad", NULL) == RS_RET_PARAM_ERROR);
    CHECK(rsReloadNormalizedGraphBuilderV1Add(newBuilder, RS_RELOAD_OBJ_ACTION, "a-action", "fp-a") == RS_RET_OK);
    CHECK(rsReloadNormalizedGraphBuilderV1Add(newBuilder, RS_RELOAD_OBJ_RULESET, "a-rule", "fp-new") == RS_RET_OK);
    CHECK(rsReloadNormalizedGraphBuilderV1Add(newBuilder, RS_RELOAD_OBJ_RULESET, "z-rule", "fp-z") == RS_RET_OK);
    CHECK(rsReloadNormalizedGraphBuilderV1GetGraph(oldBuilder, &oldGraph) == RS_RET_OK);
    CHECK(oldGraph.enumerate(oldGraph.context, observeNode, &observed) == RS_RET_OK);
    CHECK(observed.count == 3);
    CHECK(observed.nodes[0]->objectKind == RS_RELOAD_OBJ_RULESET);
    CHECK(!strcmp(observed.nodes[0]->identity, "a-rule"));
    CHECK(!strcmp(observed.nodes[1]->identity, "z-rule"));
    CHECK(!strcmp(observed.nodes[1]->fingerprint, "fp-z"));
    CHECK(observed.nodes[2]->objectKind == RS_RELOAD_OBJ_ACTION);
    CHECK(oldGraph.enumerate(oldGraph.context, NULL, NULL) == RS_RET_PARAM_ERROR);
    CHECK(oldGraph.enumerate(oldGraph.context, failVisitor, NULL) == RS_RET_ERR);
    CHECK(rsReloadNormalizedGraphBuilderV1Add(oldBuilder, RS_RELOAD_OBJ_ACTION, "after-view", "fp") ==
          RS_RET_PARAM_ERROR);
    CHECK(rsReloadNormalizedGraphBuilderV1GetGraph(newBuilder, &newGraph) == RS_RET_OK);
    CHECK(rsReloadReportBuildV1(&oldGraph, &newGraph, &report) == RS_RET_OK);
    rsReloadNormalizedGraphBuilderV1Destruct(&newBuilder);
    rsReloadNormalizedGraphBuilderV1Destruct(&oldBuilder);
    CHECK(newBuilder == NULL && oldBuilder == NULL);
    CHECK(report->entryCount == 3);
    CHECK(report->unchangedCount == 2);
    CHECK(report->modifiedCount == 1);
    CHECK(report->entries[0].objectKind == RS_RELOAD_OBJ_RULESET);
    CHECK(!strcmp(report->entries[0].identity, "a-rule"));
    CHECK(report->entries[0].diffKind == RS_RELOAD_DIFF_MODIFIED);
    CHECK(report->entries[1].diffKind == RS_RELOAD_DIFF_UNCHANGED);
    CHECK(report->entries[2].objectKind == RS_RELOAD_OBJ_ACTION);
    CHECK(report->entries[2].diffKind == RS_RELOAD_DIFF_UNCHANGED);
    rsReloadReportDestructV1(&report);

    CHECK(rsReloadNormalizedGraphBuilderV1Construct(&oldBuilder) == RS_RET_OK);
    CHECK(rsReloadNormalizedGraphBuilderV1Add(oldBuilder, RS_RELOAD_OBJ_RULESET, "duplicate", "one") == RS_RET_OK);
    CHECK(rsReloadNormalizedGraphBuilderV1Add(oldBuilder, RS_RELOAD_OBJ_RULESET, "duplicate", "two") == RS_RET_OK);
    CHECK(rsReloadNormalizedGraphBuilderV1GetGraph(oldBuilder, &oldGraph) == RS_RET_OK);
    CHECK(rsReloadNormalizedGraphBuilderV1Construct(&newBuilder) == RS_RET_OK);
    CHECK(rsReloadNormalizedGraphBuilderV1GetGraph(newBuilder, &newGraph) == RS_RET_OK);
    CHECK(rsReloadReportBuildV1(&oldGraph, &newGraph, &report) == RS_RET_OK);
    CHECK(report->invalidCount == 2);
    CHECK(report->entries[0].reason == RS_RELOAD_REASON_DUPLICATE_IDENTITY);
    rsReloadReportDestructV1(&report);
    rsReloadNormalizedGraphBuilderV1Destruct(&newBuilder);
    rsReloadNormalizedGraphBuilderV1Destruct(&oldBuilder);
    puts("reload normalized graph tests passed");
    return 0;
}
