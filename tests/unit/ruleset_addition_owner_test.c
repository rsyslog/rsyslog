/* SPDX-License-Identifier: Apache-2.0
 * Execute production detached-shell/node ownership helpers under deterministic
 * allocation failure. Every partial constructor must release exactly its owned
 * names/node/shell once. Publication splices a prepared node without allocation
 * or changes to current/default; roots stay separately owned until commit and
 * require explicit shutdown destruction before shell teardown.
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rsyslog.h"
#include "ruleset.h"
static size_t allocations, live, failAt, destructed;
static void *testCalloc(size_t count, size_t size) {
    if (++allocations == failAt) return NULL;
    void *ptr = calloc(count, size);
    if (ptr != NULL) ++live;
    return ptr;
}
static char *testStrdup(const char *name) {
    if (++allocations == failAt) return NULL;
    char *ptr = strdup(name);
    if (ptr != NULL) ++live;
    return ptr;
}
static void testFree(void *ptr) {
    if (ptr != NULL) {
        --live;
        free(ptr);
    }
}
rsRetVal rulesetConstruct(ruleset_t **shell) {
    *shell = testCalloc(1, sizeof(**shell));
    return *shell == NULL ? RS_RET_OUT_OF_MEMORY : RS_RET_OK;
}
static rsRetVal rulesetSetName(ruleset_t *shell, uchar *name) {
    shell->pszName = (uchar *)testStrdup((char *)name);
    return shell->pszName == NULL ? RS_RET_OUT_OF_MEMORY : RS_RET_OK;
}
rsRetVal rulesetDestruct(ruleset_t **shell) {
    ++destructed;
    testFree((*shell)->pszName);
    testFree(*shell);
    *shell = NULL;
    return RS_RET_OK;
}
#define calloc testCalloc
#define strdup testStrdup
#define free testFree
#include "../../runtime/ruleset-addition.h"
#undef calloc
#undef strdup
#undef free
#define CHECK(c)                                                   \
    do {                                                           \
        if (!(c)) {                                                \
            fprintf(stderr, "failed line %d: %s\n", __LINE__, #c); \
            return 1;                                              \
        }                                                          \
    } while (0)
int main(void) {
    ruleset_t *shell = NULL, baseline = {0};
    llElt_t *node = NULL, initial = {.pData = &baseline};
    rsconf_t conf = {0};
    char name[] = "private";
    size_t before;
    for (size_t failure = 1; failure <= 4; ++failure) {
        allocations = 0;
        failAt = failure;
        before = destructed;
        CHECK(rulesetPrepareAdditionV1(name, &shell, &node) == RS_RET_OUT_OF_MEMORY);
        CHECK(shell == NULL && node == NULL && live == 0);
        CHECK(destructed == before + (failure != 1));
    }
    allocations = 0;
    failAt = 0;
    CHECK(rulesetPrepareAdditionV1(name, &shell, &node) == RS_RET_OK);
    CHECK(live == 4 && allocations == 4 && node->pData == shell);
    CHECK(shell->root == NULL && shell->pQueue == NULL && shell->pParserLst == NULL);
    name[0] = 'X';
    CHECK(!strcmp((char *)shell->pszName, "private") && !strcmp(node->pKey, "private"));
    before = destructed;
    rulesetDiscardAdditionV1(&shell, &node);
    CHECK(shell == NULL && node == NULL && live == 0 && destructed == before + 1);
    rulesetDiscardAdditionV1(&shell, &node);
    CHECK(destructed == before + 1);
    CHECK(rulesetPrepareAdditionV1("committed", &shell, &node) == RS_RET_OK);
    conf.rulesets.pCurr = conf.rulesets.pDflt = &baseline;
    conf.rulesets.llRulesets.pRoot = conf.rulesets.llRulesets.pLast = &initial;
    conf.rulesets.llRulesets.iNumElts = 1;
    before = allocations;
    rulesetPublishAdditionV1(&conf, &node);
    CHECK(node == NULL && allocations == before);
    CHECK(conf.rulesets.pCurr == &baseline && conf.rulesets.pDflt == &baseline);
    CHECK(conf.rulesets.llRulesets.iNumElts == 2 && initial.pNext == conf.rulesets.llRulesets.pLast);
    CHECK(initial.pNext->pData == shell);
    /* Owning-list shutdown destroys the root separately, then key/shell/node. */
    testFree(initial.pNext->pKey);
    rulesetDestruct(&shell);
    testFree(initial.pNext);
    CHECK(live == 0);
    return 0;
}
