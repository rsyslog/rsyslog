/* SPDX-License-Identifier: Apache-2.0
 * Exercise the actual static-call dispatch with bounded fake executors. A
 * changed shell root must be observed on the next call, including an empty
 * root. Queue mode, synchronous errors, and duplicate allocation failure keep
 * their existing behavior; no daemon, threads, waits, or reload are involved.
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include "rsyslog.h"
#include "msg.h"
#include "ruleset.h"
#include "rainerscript.h"
#include "wti.h"

static struct cnfstmt *observedRoot;
static ruleset_t *observedQueue;
static unsigned syncCalls, dupCalls, submits;
static int failDup;
static rsRetVal syncResult;
static smsg_t duplicate;
static rsRetVal execSynchronousRulesetCall(
    struct cnfstmt *root, const char *name, size_t len, smsg_t *msg, wti_t *worker) {
    (void)name;
    (void)len;
    (void)msg;
    (void)worker;
    observedRoot = root;
    ++syncCalls;
    return syncResult;
}
smsg_t *MsgDup(smsg_t *msg) {
    (void)msg;
    ++dupCalls;
    return failDup ? NULL : &duplicate;
}
rsRetVal MsgSetFlowControlType(smsg_t *msg, flowControl_t flow) {
    (void)msg;
    (void)flow;
    return RS_RET_OK;
}
void MsgSetRuleset(smsg_t *msg, ruleset_t *target) {
    (void)msg;
    observedQueue = target;
}
static void submitMsg2(smsg_t *msg) {
    (void)msg;
    ++submits;
}
#undef DBGPRINTF
#define DBGPRINTF(...) ((void)0)
#include "../../runtime/ruleset-call.h"
#define CHECK(c)                                                   \
    do {                                                           \
        if (!(c)) {                                                \
            fprintf(stderr, "failed line %d: %s\n", __LINE__, #c); \
            return 1;                                              \
        }                                                          \
    } while (0)
int main(void) {
    struct cnfstmt first = {.nodetype = S_STOP}, second = {.nodetype = S_NOP};
    ruleset_t shell = {0}, queued = {0};
    struct cnfstmt call = {.nodetype = S_CALL};
    smsg_t msg = {0};
    wti_t worker = {0};
    call.d.s_call.name = es_newStrFromCStr("callee", 6);
    CHECK(call.d.s_call.name != NULL);
    call.d.s_call.target = &shell;
    shell.root = &first;
    CHECK(execCall(&call, &msg, &worker) == RS_RET_OK && observedRoot == &first);
    shell.root = &second;
    CHECK(execCall(&call, &msg, &worker) == RS_RET_OK && observedRoot == &second);
    shell.root = NULL;
    CHECK(execCall(&call, &msg, &worker) == RS_RET_OK && observedRoot == NULL);
    syncResult = RS_RET_DISCARDMSG;
    CHECK(execCall(&call, &msg, &worker) == RS_RET_DISCARDMSG);
    CHECK(syncCalls == 4 && dupCalls == 0);
    call.d.s_call.target = &queued;
    call.d.s_call.ruleset = &queued;
    CHECK(execCall(&call, &msg, &worker) == RS_RET_OK);
    CHECK(syncCalls == 4 && dupCalls == 1 && submits == 1 && observedQueue == &queued);
    failDup = 1;
    CHECK(execCall(&call, &msg, &worker) == RS_RET_OUT_OF_MEMORY);
    CHECK(syncCalls == 4 && dupCalls == 2 && submits == 1);
    es_deleteStr(call.d.s_call.name);
    return 0;
}
