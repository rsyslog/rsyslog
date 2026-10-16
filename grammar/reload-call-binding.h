/* SPDX-License-Identifier: Apache-2.0
 * Private materializer implementation shared with the explicit-catalog unit.
 */
/* Bind private syntax only through the supplied active membership catalog.
 * Every nested static target is resolved before the recursive optimizer runs. */
static rsRetVal bindPreparedCalls(struct cnfstmt *stmt, rsconf_t *const active) {
    DEFiRet;
    for (; stmt != NULL; stmt = stmt->next) {
        switch (stmt->nodetype) {
            case S_CALL: {
                ruleset_t *target = NULL;
                char *name = NULL;
                if (stmt->d.s_call.name == NULL) ABORT_FINALIZE(RS_RET_PARAM_ERROR);
                CHKmalloc(name = es_str2cstr(stmt->d.s_call.name, NULL));
                iRet = rulesetGetRuleset(active, &target, (uchar *)name);
                free(name);
                if (iRet != RS_RET_OK) ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
                stmt->d.s_call.target = target;
                stmt->d.s_call.ruleset = rulesetHasQueue(target) ? target : NULL;
                break;
            }
            case S_IF:
                CHKiRet(bindPreparedCalls(stmt->d.s_if.t_then, active));
                CHKiRet(bindPreparedCalls(stmt->d.s_if.t_else, active));
                break;
            case S_FOREACH:
                CHKiRet(bindPreparedCalls(stmt->d.s_foreach.body, active));
                break;
            case S_PRIFILT:
                CHKiRet(bindPreparedCalls(stmt->d.s_prifilt.t_then, active));
                CHKiRet(bindPreparedCalls(stmt->d.s_prifilt.t_else, active));
                break;
            case S_PROPFILT:
                CHKiRet(bindPreparedCalls(stmt->d.s_propfilt.t_then, active));
                break;
            default:
                break;
        }
    }
finalize_it:
    RETiRet;
}
