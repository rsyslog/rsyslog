/* SPDX-License-Identifier: Apache-2.0
 * Private implementation shared with the bounded dispatch unit test.
 * Include after the synchronous executor and message APIs are declared.
 */
static rsRetVal execCall(struct cnfstmt *stmt, smsg_t *pMsg, wti_t *pWti) {
    DEFiRet;
    if (stmt->d.s_call.ruleset == NULL) {
        CHKiRet(execSynchronousRulesetCall(stmt->d.s_call.target->root,
                                           (const char *)es_getBufAddr(stmt->d.s_call.name),
                                           es_strlen(stmt->d.s_call.name), pMsg, pWti));
    } else {
        CHKmalloc(pMsg = MsgDup((smsg_t *)pMsg));
        DBGPRINTF("CALL: forwarding message to async ruleset %p\n", stmt->d.s_call.ruleset->pQueue);
        MsgSetFlowControlType(pMsg, eFLOWCTL_NO_DELAY);
        MsgSetRuleset(pMsg, stmt->d.s_call.ruleset);
        /* Note: we intentionally use submitMsg2() here, as we process messages
         * that were already run through the rate-limiter.
         */
        submitMsg2(pMsg);
    }
finalize_it:
    RETiRet;
}
