/* SPDX-License-Identifier: Apache-2.0
 * Immutable additions-only ruleset namespace. Published blocks own copied names
 * and borrow shells owned by the serialized configuration list. Baseline
 * publication precedes activation; release/acquire publication is available
 * only when pointer atomics are always lock-free. Blocks remain until stopped
 * configuration teardown, so readers need no lock, refcount or reclamation.
 */
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include "ruleset-name-index.h"
#include "ruleset.h"

typedef struct rulesetNameEntryV1_s {
    char *name;
    ruleset_t *shell;
} rulesetNameEntryV1_t;
struct rulesetNameBlockV1_s {
    struct rulesetNameBlockV1_s *previous;
    size_t count;
    rulesetNameEntryV1_t entries[];
};

int rulesetNameIndexSupportedV1(void) {
#ifdef HAVE_ATOMIC_BUILTINS
    return __atomic_always_lock_free(sizeof(void *), 0);
#else
    return 0;
#endif
}
static rulesetNameBlockV1_t *indexHead(rsconf_t *conf) {
#ifdef HAVE_ATOMIC_BUILTINS
    if (__atomic_always_lock_free(sizeof(void *), 0))
        return __atomic_load_n(&conf->rulesets.nameIndex, __ATOMIC_ACQUIRE);
#else
    (void)conf;
#endif
    return NULL;
}
static rsRetVal findName(rulesetNameBlockV1_t *block, ruleset_t **out, const char *name) {
    for (; block != NULL; block = block->previous)
        for (size_t i = 0; i < block->count; ++i)
            if (!strcasecmp(block->entries[i].name, name)) {
                *out = block->entries[i].shell;
                return RS_RET_OK;
            }
    return RS_RET_NOT_FOUND;
}
rsRetVal rulesetNameIndexLookupV1(rsconf_t *conf, ruleset_t **out, const char *name, int *indexed) {
    rulesetNameBlockV1_t *const head = indexHead(conf);
    *indexed = head != NULL;
    return findName(head, out, name);
}
void rulesetNameIndexDiscardV1(rulesetNameBlockV1_t **block) {
    if (block == NULL || *block == NULL) return;
    for (size_t i = 0; i < (*block)->count; ++i) free((*block)->entries[i].name);
    free(*block);
    *block = NULL;
}
static rsRetVal prepareBlock(rulesetNameBlockV1_t *head,
                             ruleset_t *const *shells,
                             size_t count,
                             rulesetNameBlockV1_t **out) {
    rulesetNameBlockV1_t *block = NULL;
    ruleset_t *existing;
    DEFiRet;
    if (count == 0 || count > (SIZE_MAX - sizeof(*block)) / sizeof(block->entries[0])) return RS_RET_PARAM_ERROR;
    CHKmalloc(block = calloc(1, sizeof(*block) + count * sizeof(block->entries[0])));
    for (size_t i = 0; i < count; ++i) {
        if (shells[i] == NULL || shells[i]->pszName == NULL || shells[i]->pszName[0] == '\0')
            ABORT_FINALIZE(RS_RET_PARAM_ERROR);
        if (findName(head, &existing, (char *)shells[i]->pszName) == RS_RET_OK) ABORT_FINALIZE(RS_RET_DUP_PARAM);
        for (size_t j = 0; j < i; ++j)
            if (!strcasecmp(block->entries[j].name, (char *)shells[i]->pszName)) ABORT_FINALIZE(RS_RET_DUP_PARAM);
        CHKmalloc(block->entries[i].name = strdup((char *)shells[i]->pszName));
        block->entries[i].shell = shells[i];
        ++block->count;
    }
    block->previous = head;
    *out = block;
    block = NULL;
finalize_it:
    rulesetNameIndexDiscardV1(&block);
    RETiRet;
}
rsRetVal rulesetNameIndexPrepareV1(rsconf_t *conf, ruleset_t *const *shells, size_t count, rulesetNameBlockV1_t **out) {
    rulesetNameBlockV1_t *head;
    if (conf == NULL || shells == NULL || out == NULL || *out != NULL) return RS_RET_PARAM_ERROR;
    if (!rulesetNameIndexSupportedV1() || (head = indexHead(conf)) == NULL) return RS_RET_NOT_IMPLEMENTED;
    return prepareBlock(head, shells, count, out);
}
void rulesetNameIndexPublishV1(rsconf_t *conf, rulesetNameBlockV1_t **block) {
#ifdef HAVE_ATOMIC_BUILTINS
    /* The prepare path proved support and serialized head identity. */
    if (__atomic_always_lock_free(sizeof(void *), 0)) {
        __atomic_store_n(&conf->rulesets.nameIndex, *block, __ATOMIC_RELEASE);
        *block = NULL;
    }
#else
    (void)conf;
    (void)block;
#endif
}
rsRetVal rulesetNameIndexInitV1(rsconf_t *conf) {
    ruleset_t **shells = NULL;
    rulesetNameBlockV1_t *block = NULL;
    size_t count = 0, capacity = 0;
    rsRetVal ret;
    if (conf == NULL) return RS_RET_PARAM_ERROR;
    if (!rulesetNameIndexSupportedV1()) return RS_RET_OK;
    if (indexHead(conf) != NULL) return RS_RET_PARAM_ERROR;
    for (llElt_t *node = conf->rulesets.llRulesets.pRoot; node != NULL; node = node->pNext) ++capacity;
    if (capacity == 0 || capacity > SIZE_MAX / sizeof(*shells)) return RS_RET_PARAM_ERROR;
    if ((shells = calloc(capacity, sizeof(*shells))) == NULL) return RS_RET_OUT_OF_MEMORY;
    for (llElt_t *node = conf->rulesets.llRulesets.pRoot; node != NULL; node = node->pNext)
        shells[count++] = node->pData;
    ret = prepareBlock(NULL, shells, count, &block);
    free(shells);
    if (ret == RS_RET_OK) rulesetNameIndexPublishV1(conf, &block);
    return ret;
}
void rulesetNameIndexDestroyV1(rsconf_t *conf) {
    rulesetNameBlockV1_t *block = indexHead(conf);
    while (block != NULL) {
        rulesetNameBlockV1_t *previous = block->previous;
        rulesetNameIndexDiscardV1(&block);
        block = previous;
    }
    conf->rulesets.nameIndex = NULL;
}
