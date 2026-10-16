/* SPDX-License-Identifier: Apache-2.0
 * The production recursive binder must resolve every nested static call using
 * its explicit active catalog, even while loadConf is NULL or points elsewhere.
 * A missing target returns an error without binding or mutating live roots.
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rsyslog.h"
#include "msg.h"
#include "ruleset.h"
#include "rainerscript.h"
#include "rsconf.h"
static rsconf_t activeFixture, wrongFixture;
rsconf_t *loadConf;
static ruleset_t syncShell, queuedShell;
static unsigned lookups;
rsRetVal rulesetGetRuleset(rsconf_t *conf, ruleset_t **out, uchar *name) {
    if (conf != &activeFixture) return RS_RET_ERR;
    ++lookups;
    if (!strcmp((char *)name, "sync"))
        *out = &syncShell;
    else if (!strcmp((char *)name, "queued"))
        *out = &queuedShell;
    else
        return RS_RET_RULESET_NOT_FOUND;
    return RS_RET_OK;
}
#include "../../grammar/reload-call-binding.h"
#define CHECK(c)                                                   \
    do {                                                           \
        if (!(c)) {                                                \
            fprintf(stderr, "failed line %d: %s\n", __LINE__, #c); \
            return 1;                                              \
        }                                                          \
    } while (0)
int main(void) {
    qqueue_t queue = {.qType = QUEUETYPE_LINKEDLIST};
    queuedShell.pQueue = &queue;
    struct cnfstmt first = {.nodetype = S_CALL}, second = {.nodetype = S_CALL};
    struct cnfstmt branch = {.nodetype = S_IF}, loop = {.nodetype = S_FOREACH};
    struct cnfstmt missing = {.nodetype = S_CALL};
    branch.d.s_if.t_then = &loop;
    branch.d.s_if.t_else = &second;
    loop.d.s_foreach.body = &first;
    first.d.s_call.name = es_newStrFromCStr("sync", 4);
    second.d.s_call.name = es_newStrFromCStr("queued", 6);
    missing.d.s_call.name = es_newStrFromCStr("missing", 7);
    CHECK(first.d.s_call.name && second.d.s_call.name && missing.d.s_call.name);
    for (unsigned phase = 0; phase < 2; ++phase) {
        loadConf = phase ? &wrongFixture : NULL;
        CHECK(bindPreparedCalls(&branch, &activeFixture) == RS_RET_OK);
        CHECK(loadConf == (phase ? &wrongFixture : NULL));
        CHECK(first.d.s_call.target == &syncShell && first.d.s_call.ruleset == NULL);
        CHECK(second.d.s_call.target == &queuedShell && second.d.s_call.ruleset == &queuedShell);
    }
    CHECK(lookups == 4);
    CHECK(bindPreparedCalls(&missing, &activeFixture) == RS_RET_NOT_IMPLEMENTED);
    CHECK(missing.d.s_call.target == NULL && missing.d.s_call.ruleset == NULL);
    CHECK(syncShell.root == NULL && queuedShell.root == NULL);
    es_deleteStr(first.d.s_call.name);
    es_deleteStr(second.d.s_call.name);
    es_deleteStr(missing.d.s_call.name);
    return 0;
}
