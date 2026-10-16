/* SPDX-License-Identifier: Apache-2.0
 * Production immutable namespace coverage. A complete startup baseline precedes
 * readers; old names remain findable during repeated release/acquire additions.
 * Private blocks are invisible until publication, case-folded collisions fail,
 * modifications require no block, and copied-name/block allocation counts return
 * to zero only after readers stop. No shell ownership is transferred to blocks.
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "rsyslog.h"
#include "ruleset.h"

#ifdef HAVE_ATOMIC_BUILTINS
static size_t liveAllocations;
static void *trackedCalloc(size_t count, size_t size) {
    void *p = calloc(count, size);
    if (p != NULL) ++liveAllocations;
    return p;
}
static char *trackedStrdup(const char *string) {
    char *p = strdup(string);
    if (p != NULL) ++liveAllocations;
    return p;
}
static void trackedFree(void *p) {
    if (p != NULL) {
        --liveAllocations;
        free(p);
    }
}
    #define calloc trackedCalloc
    #define strdup trackedStrdup
    #define free trackedFree
    #include "../../runtime/ruleset-name-index.c"
    #undef calloc
    #undef strdup
    #undef free

    #define ADDITIONS 64
static rsconf_t conf;
static ruleset_t baseline, additions[ADDITIONS];
static char names[ADDITIONS][32];
static unsigned publishedCount, readerStop, readerFailure, observed, acknowledged;
static pthread_mutex_t progressMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t progressCond = PTHREAD_COND_INITIALIZER;
static void *reader(void *context) {
    (void)context;
    while (!__atomic_load_n(&readerStop, __ATOMIC_ACQUIRE)) {
        ruleset_t *found = NULL;
        int indexed = 0;
        unsigned count = __atomic_load_n(&publishedCount, __ATOMIC_ACQUIRE);
        if (rulesetNameIndexLookupV1(&conf, &found, "BaSeLiNe", &indexed) != RS_RET_OK || !indexed ||
            found != &baseline)
            __atomic_store_n(&readerFailure, 1, __ATOMIC_RELEASE);
        for (unsigned i = 0; i < count; ++i)
            if (rulesetNameIndexLookupV1(&conf, &found, names[i], &indexed) != RS_RET_OK || found != &additions[i])
                __atomic_store_n(&readerFailure, 1, __ATOMIC_RELEASE);
        __atomic_add_fetch(&observed, 1, __ATOMIC_RELAXED);
        pthread_mutex_lock(&progressMutex);
        if (count > acknowledged) acknowledged = count;
        pthread_cond_signal(&progressCond);
        pthread_mutex_unlock(&progressMutex);
    }
    return NULL;
}
    #define CHECK(c)                                                   \
        do {                                                           \
            if (!(c)) {                                                \
                fprintf(stderr, "failed line %d: %s\n", __LINE__, #c); \
                return 1;                                              \
            }                                                          \
        } while (0)
int main(void) {
    rulesetNameBlockV1_t *block = NULL;
    ruleset_t *found = NULL;
    ruleset_t duplicate = {.pszName = (uchar *)"BASELINE"};
    llElt_t node = {.pData = &baseline};
    ruleset_t *one[2];
    ruleset_t duplicateOne = {.pszName = (uchar *)"fresh"}, duplicateTwo = {.pszName = (uchar *)"FRESH"};
    int indexed = -1;
    pthread_t thread;
    if (!rulesetNameIndexSupportedV1()) return 77;
    baseline.pszName = (uchar *)"baseline";
    conf.rulesets.llRulesets.pRoot = conf.rulesets.llRulesets.pLast = &node;
    conf.rulesets.llRulesets.iNumElts = 1;
    CHECK(rulesetNameIndexLookupV1(&conf, &found, "baseline", &indexed) == RS_RET_NOT_FOUND && !indexed);
    CHECK(rulesetNameIndexInitV1(&conf) == RS_RET_OK);
    CHECK(liveAllocations == 2); /* One baseline block and copied name. */
    CHECK(rulesetNameIndexLookupV1(&conf, &found, "missing", &indexed) == RS_RET_NOT_FOUND && indexed);
    one[0] = &duplicate;
    CHECK(rulesetNameIndexPrepareV1(&conf, one, 1, &block) == RS_RET_DUP_PARAM && block == NULL);
    CHECK(liveAllocations == 2);
    one[0] = &duplicateOne;
    one[1] = &duplicateTwo;
    CHECK(rulesetNameIndexPrepareV1(&conf, one, 2, &block) == RS_RET_DUP_PARAM && block == NULL);
    CHECK(liveAllocations == 2);
    CHECK(pthread_create(&thread, NULL, reader, NULL) == 0);
    for (unsigned i = 0; i < ADDITIONS; ++i) {
        snprintf(names[i], sizeof(names[i]), "added-%u", i);
        additions[i].pszName = (uchar *)names[i];
        one[0] = &additions[i];
        CHECK(rulesetNameIndexPrepareV1(&conf, one, 1, &block) == RS_RET_OK);
        CHECK(rulesetNameIndexLookupV1(&conf, &found, names[i], &indexed) == RS_RET_NOT_FOUND && indexed);
        rulesetNameIndexDiscardV1(&block); /* Private abort is invisible and balanced. */
        CHECK(rulesetNameIndexPrepareV1(&conf, one, 1, &block) == RS_RET_OK);
        rulesetNameIndexPublishV1(&conf, &block);
        CHECK(block == NULL);
        __atomic_store_n(&publishedCount, i + 1, __ATOMIC_RELEASE);
        /* Each published prefix is observed before the next addition. The
         * reader keeps searching while the writer prepares/publishes again. */
        pthread_mutex_lock(&progressMutex);
        while (acknowledged < i + 1) pthread_cond_wait(&progressCond, &progressMutex);
        pthread_mutex_unlock(&progressMutex);
        CHECK(liveAllocations == 2 * (i + 2));
    }
    __atomic_store_n(&readerStop, 1, __ATOMIC_RELEASE);
    CHECK(pthread_join(thread, NULL) == 0);
    CHECK(!readerFailure && observed != 0);
    /* Shell-name mutation does not change copied namespace identity. */
    baseline.pszName = (uchar *)"changed-shell-display-name";
    CHECK(rulesetNameIndexLookupV1(&conf, &found, "BASELINE", &indexed) == RS_RET_OK && found == &baseline);
    rulesetNameIndexDestroyV1(&conf);
    CHECK(liveAllocations == 0 && conf.rulesets.nameIndex == NULL);
    return 0;
}

#else
int main(void) {
    return 77;
}
#endif
