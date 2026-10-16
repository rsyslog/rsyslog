/* SPDX-License-Identifier: Apache-2.0
 * Private materializer implementation shared with the explicit-catalog unit.
 */
/* Bind private syntax only through the supplied active membership catalog.
 * Every nested static target is resolved before the recursive optimizer runs. */
typedef rsRetVal (*reloadCallResolverV1_t)(void *context, ruleset_t **target, uchar *name);
static rsRetVal bindPreparedCallsWithResolver(struct cnfstmt *stmt, reloadCallResolverV1_t resolver, void *context) {
    DEFiRet;
    for (; stmt != NULL; stmt = stmt->next) {
        switch (stmt->nodetype) {
            case S_CALL: {
                ruleset_t *target = NULL;
                char *name = NULL;
                if (stmt->d.s_call.name == NULL) ABORT_FINALIZE(RS_RET_PARAM_ERROR);
                CHKmalloc(name = es_str2cstr(stmt->d.s_call.name, NULL));
                iRet = resolver(context, &target, (uchar *)name);
                free(name);
                if (iRet != RS_RET_OK) ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
                stmt->d.s_call.target = target;
                stmt->d.s_call.ruleset = rulesetHasQueue(target) ? target : NULL;
                break;
            }
            case S_IF:
                CHKiRet(bindPreparedCallsWithResolver(stmt->d.s_if.t_then, resolver, context));
                CHKiRet(bindPreparedCallsWithResolver(stmt->d.s_if.t_else, resolver, context));
                break;
            case S_FOREACH:
                CHKiRet(bindPreparedCallsWithResolver(stmt->d.s_foreach.body, resolver, context));
                break;
            case S_PRIFILT:
                CHKiRet(bindPreparedCallsWithResolver(stmt->d.s_prifilt.t_then, resolver, context));
                CHKiRet(bindPreparedCallsWithResolver(stmt->d.s_prifilt.t_else, resolver, context));
                break;
            case S_PROPFILT:
                CHKiRet(bindPreparedCallsWithResolver(stmt->d.s_propfilt.t_then, resolver, context));
                break;
            default:
                break;
        }
    }
finalize_it:
    RETiRet;
}

static rsRetVal resolveActiveReloadCall(void *context, ruleset_t **target, uchar *name) {
    return rulesetGetRuleset(context, target, name);
}
static inline rsRetVal bindPreparedCalls(struct cnfstmt *stmt, rsconf_t *active) {
    return bindPreparedCallsWithResolver(stmt, resolveActiveReloadCall, active);
}
