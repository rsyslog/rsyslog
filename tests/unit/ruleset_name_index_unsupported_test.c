/* SPDX-License-Identifier: Apache-2.0
 * Force the compiler's always-lock-free predicate false in the production
 * implementation. Startup keeps the legacy lookup contract; additions reject
 * before allocation/publication, with no fallback mutex or atomic helper path.
 */
#include "config.h"
#define __atomic_always_lock_free(size, pointer) 0
#include "../../runtime/ruleset-name-index.c"
#include <stdio.h>
int main(void) {
    rsconf_t conf = {0};
    ruleset_t shell = {.pszName = (uchar *)"new"}, *found = NULL;
    ruleset_t *shells[1] = {&shell};
    rulesetNameBlockV1_t *block = NULL;
    int indexed = 1;
    if (rulesetNameIndexSupportedV1() || rulesetNameIndexInitV1(&conf) != RS_RET_OK ||
        conf.rulesets.nameIndex != NULL ||
        rulesetNameIndexPrepareV1(&conf, shells, 1, &block) != RS_RET_NOT_IMPLEMENTED || block != NULL ||
        rulesetNameIndexLookupV1(&conf, &found, "new", &indexed) != RS_RET_NOT_FOUND || indexed) {
        fputs("unsupported atomic namespace did not fail closed\n", stderr);
        return 1;
    }
    return 0;
}
