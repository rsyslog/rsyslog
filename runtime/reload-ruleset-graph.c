/* reload-ruleset-graph.c
 *
 * Frontend-neutral serialization of effective, optimized ruleset programs.
 * This is control-path-only code and adds no message-processing work.
 */
#include "config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rsyslog.h"
#include "action.h"
#include "linkedlist.h"
#include "reload-ruleset-graph.h"
#include "rsconf.h"
#include "ruleset.h"
#include "stringbuf.h"
#include "grammar/grammar.h"
#include "grammar/rainerscript.h"

static rsRetVal appendBytes(es_str_t **out, const void *data, const size_t len) {
    static const char hex[] = "0123456789abcdef";
    const unsigned char *bytes = data;
    char lenbuf[32];
    size_t i;
    int n;

    n = snprintf(lenbuf, sizeof(lenbuf), "%zu:", len);
    if (n < 0 || (size_t)n >= sizeof(lenbuf) || es_addBuf(out, lenbuf, (size_t)n) != 0) {
        return RS_RET_OUT_OF_MEMORY;
    }
    for (i = 0; i < len; ++i) {
        const char pair[2] = {hex[bytes[i] >> 4], hex[bytes[i] & 0x0f]};
        if (es_addBuf(out, pair, sizeof(pair)) != 0) return RS_RET_OUT_OF_MEMORY;
    }
    return es_addChar(out, ';') == 0 ? RS_RET_OK : RS_RET_OUT_OF_MEMORY;
}

static rsRetVal appendCString(es_str_t **out, const char *str) {
    if (str == NULL) return appendBytes(out, "", 0);
    return appendBytes(out, str, strlen(str));
}

static rsRetVal appendEStr(es_str_t **out, es_str_t *str) {
    if (str == NULL) return appendBytes(out, "", 0);
    return appendBytes(out, es_getBufAddr(str), es_strlen(str));
}

static rsRetVal appendToken(es_str_t **out, const char *token) {
    return es_addBuf(out, token, strlen(token)) == 0 ? RS_RET_OK : RS_RET_OUT_OF_MEMORY;
}

static rsRetVal appendUnsigned(es_str_t **out, const unsigned long long value) {
    char buf[32];
    const int n = snprintf(buf, sizeof(buf), "%llu;", value);

    if (n < 0 || (size_t)n >= sizeof(buf) || es_addBuf(out, buf, (size_t)n) != 0) {
        return RS_RET_OUT_OF_MEMORY;
    }
    return RS_RET_OK;
}

static rsRetVal appendSigned(es_str_t **out, const long long value) {
    char buf[32];
    const int n = snprintf(buf, sizeof(buf), "%lld;", value);

    if (n < 0 || (size_t)n >= sizeof(buf) || es_addBuf(out, buf, (size_t)n) != 0) {
        return RS_RET_OUT_OF_MEMORY;
    }
    return RS_RET_OK;
}

static rsRetVal appendExpr(es_str_t **out, const struct cnfexpr *expr);

static rsRetVal appendExprList(es_str_t **out, const struct cnffunc *func) {
    unsigned short i;
    DEFiRet;

    CHKiRet(appendUnsigned(out, func->nParams));
    for (i = 0; i < func->nParams; ++i) CHKiRet(appendExpr(out, func->expr[i]));

finalize_it:
    RETiRet;
}

static rsRetVal appendExpr(es_str_t **out, const struct cnfexpr *expr) {
    const struct cnfarray *array;
    const struct cnffunc *func;
    int i;
    DEFiRet;

    if (expr == NULL) return RS_RET_PARAM_ERROR;
    CHKiRet(appendToken(out, "E"));
    CHKiRet(appendUnsigned(out, expr->nodetype));
    switch (expr->nodetype) {
        case 'N':
            CHKiRet(appendSigned(out, ((const struct cnfnumval *)expr)->val));
            break;
        case 'S':
            CHKiRet(appendEStr(out, ((const struct cnfstringval *)expr)->estr));
            break;
        case 'V':
            CHKiRet(appendCString(out, ((const struct cnfvar *)expr)->name));
            break;
        case 'A':
            array = (const struct cnfarray *)expr;
            CHKiRet(appendSigned(out, array->nmemb));
            for (i = 0; i < array->nmemb; ++i) CHKiRet(appendEStr(out, array->arr[i]));
            break;
        case 'F':
            func = (const struct cnffunc *)expr;
            CHKiRet(appendEStr(out, func->fname));
            CHKiRet(appendExprList(out, func));
            break;
        case S_FUNC_EXISTS:
            CHKiRet(appendCString(out, ((const struct cnffuncexists *)expr)->varname));
            break;
        case NOT:
        case 'M':
            CHKiRet(appendExpr(out, expr->r));
            break;
        case CMP_EQ:
        case CMP_NE:
        case CMP_LE:
        case CMP_GE:
        case CMP_LT:
        case CMP_GT:
        case CMP_CONTAINS:
        case CMP_CONTAINSI:
        case CMP_STARTSWITH:
        case CMP_STARTSWITHI:
        case CMP_ENDSWITH:
        case OR:
        case AND:
        case '&':
        case '+':
        case '-':
        case '*':
        case '/':
        case '%':
            CHKiRet(appendExpr(out, expr->l));
            CHKiRet(appendExpr(out, expr->r));
            break;
        default:
            ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
    }
    CHKiRet(appendToken(out, "."));

finalize_it:
    RETiRet;
}

static int actionHasExplicitName(const action_t *action) {
    const char *moduleName;
    const char *actionName;
    char generated[512];
    int n;

    if (action == NULL || action->pMod == NULL) return 0;
    moduleName = (const char *)modGetName(action->pMod);
    actionName = (const char *)actionGetName(action);
    if (moduleName == NULL || actionName == NULL) return 0;
    n = snprintf(generated, sizeof(generated), "action-%d-%s", action->iActionNbr, moduleName);
    return n < 0 || (size_t)n >= sizeof(generated) || strcmp(actionName, generated) != 0;
}

static rsRetVal appendStmtList(es_str_t **out, const struct cnfstmt *stmt);

static rsRetVal appendStmt(es_str_t **out, const struct cnfstmt *stmt) {
    const action_t *action;
    DEFiRet;

    CHKiRet(appendToken(out, "T"));
    CHKiRet(appendUnsigned(out, stmt->nodetype));
    switch (stmt->nodetype) {
        case S_NOP:
        case S_STOP:
            break;
        case S_ACT:
            action = stmt->d.act;
            if (action == NULL || action->pMod == NULL) ABORT_FINALIZE(RS_RET_PARAM_ERROR);
            CHKiRet(appendCString(out, (const char *)modGetName(action->pMod)));
            if (actionHasExplicitName(action)) {
                CHKiRet(appendToken(out, "named"));
                CHKiRet(appendCString(out, (const char *)actionGetName(action)));
            } else {
                CHKiRet(appendToken(out, "auto"));
            }
            break;
        case S_IF:
            CHKiRet(appendExpr(out, stmt->d.s_if.expr));
            CHKiRet(appendStmtList(out, stmt->d.s_if.t_then));
            CHKiRet(appendStmtList(out, stmt->d.s_if.t_else));
            break;
        case S_FOREACH:
            if (stmt->d.s_foreach.iter == NULL) ABORT_FINALIZE(RS_RET_PARAM_ERROR);
            CHKiRet(appendCString(out, stmt->d.s_foreach.iter->var));
            CHKiRet(appendExpr(out, stmt->d.s_foreach.iter->collection));
            CHKiRet(appendStmtList(out, stmt->d.s_foreach.body));
            break;
        case S_SET:
            CHKiRet(appendCString(out, (const char *)stmt->d.s_set.varname));
            CHKiRet(appendSigned(out, stmt->d.s_set.force_reset));
            CHKiRet(appendExpr(out, stmt->d.s_set.expr));
            break;
        case S_UNSET:
            CHKiRet(appendCString(out, (const char *)stmt->d.s_unset.varname));
            break;
        case S_CALL:
            CHKiRet(appendEStr(out, stmt->d.s_call.name));
            break;
        case S_CALL_INDIRECT:
            CHKiRet(appendExpr(out, stmt->d.s_call_ind.expr));
            break;
        case S_PRIFILT:
            CHKiRet(appendBytes(out, stmt->d.s_prifilt.pmask, sizeof(stmt->d.s_prifilt.pmask)));
            CHKiRet(appendStmtList(out, stmt->d.s_prifilt.t_then));
            CHKiRet(appendStmtList(out, stmt->d.s_prifilt.t_else));
            break;
        case S_PROPFILT:
            CHKiRet(appendUnsigned(out, stmt->d.s_propfilt.prop.id));
            if (stmt->d.s_propfilt.prop.id == PROP_CEE || stmt->d.s_propfilt.prop.id == PROP_LOCAL_VAR ||
                stmt->d.s_propfilt.prop.id == PROP_GLOBAL_VAR) {
                if (stmt->d.s_propfilt.prop.name == NULL || stmt->d.s_propfilt.prop.nameLen < 0) {
                    ABORT_FINALIZE(RS_RET_PARAM_ERROR);
                }
                CHKiRet(appendBytes(out, stmt->d.s_propfilt.prop.name, (size_t)stmt->d.s_propfilt.prop.nameLen));
            } else {
                CHKiRet(appendBytes(out, "", 0));
            }
            CHKiRet(appendUnsigned(out, stmt->d.s_propfilt.operation));
            CHKiRet(appendSigned(out, stmt->d.s_propfilt.isNegated));
            if (stmt->d.s_propfilt.pCSCompValue == NULL) {
                CHKiRet(appendBytes(out, "", 0));
            } else {
                CHKiRet(appendBytes(out, rsCStrGetSzStrNoNULL(stmt->d.s_propfilt.pCSCompValue),
                                    rsCStrLen(stmt->d.s_propfilt.pCSCompValue)));
            }
            CHKiRet(appendStmtList(out, stmt->d.s_propfilt.t_then));
            CHKiRet(appendStmtList(out, stmt->d.s_propfilt.t_else));
            break;
        case S_RELOAD_LOOKUP_TABLE:
            CHKiRet(appendCString(out, (const char *)stmt->d.s_reload_lookup_table.table_name));
            CHKiRet(appendCString(out, (const char *)stmt->d.s_reload_lookup_table.stub_value));
            break;
        default:
            ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
    }
    CHKiRet(appendToken(out, "."));

finalize_it:
    RETiRet;
}

static rsRetVal appendStmtList(es_str_t **out, const struct cnfstmt *stmt) {
    DEFiRet;

    CHKiRet(appendToken(out, "L"));
    while (stmt != NULL) {
        CHKiRet(appendStmt(out, stmt));
        stmt = stmt->next;
    }
    CHKiRet(appendToken(out, "."));

finalize_it:
    RETiRet;
}

rsRetVal rsReloadRulesetFingerprintV1(const ruleset_t *ruleset, char **ppFingerprint) {
    es_str_t *serialized = NULL;
    char *fingerprint = NULL;
    DEFiRet;

    if (ruleset == NULL || ppFingerprint == NULL || *ppFingerprint != NULL) return RS_RET_PARAM_ERROR;
    CHKmalloc(serialized = es_newStr(256));
    CHKiRet(appendToken(&serialized, RS_RELOAD_RULESET_FINGERPRINT_V1 ";"));
    CHKiRet(appendStmtList(&serialized, ruleset->root));
    CHKmalloc(fingerprint = es_str2cstr(serialized, NULL));
    *ppFingerprint = fingerprint;
    fingerprint = NULL;

finalize_it:
    free(fingerprint);
    if (serialized != NULL) es_deleteStr(serialized);
    RETiRet;
}

rsRetVal rsReloadRulesetGraphBuildV1(const rsconf_t *conf, rsReloadNormalizedGraphBuilderV1_t **ppBuilder) {
    rsReloadNormalizedGraphBuilderV1_t *builder = NULL;
    const llElt_t *entry;
    char *identity = NULL;
    char *fingerprint = NULL;
    DEFiRet;

    if (conf == NULL || ppBuilder == NULL || *ppBuilder != NULL) return RS_RET_PARAM_ERROR;
    CHKiRet(rsReloadNormalizedGraphBuilderV1Construct(&builder));
    for (entry = conf->rulesets.llRulesets.pRoot; entry != NULL; entry = entry->pNext) {
        const ruleset_t *ruleset = entry->pData;
        const char *name;
        size_t identityLen;

        if (ruleset == NULL || rulesetGetName(ruleset) == NULL) ABORT_FINALIZE(RS_RET_PARAM_ERROR);
        name = (const char *)rulesetGetName(ruleset);
        if (strlen(name) > SIZE_MAX - sizeof("ruleset:")) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        identityLen = sizeof("ruleset:") + strlen(name);
        CHKmalloc(identity = malloc(identityLen));
        snprintf(identity, identityLen, "ruleset:%s", name);
        CHKiRet(rsReloadRulesetFingerprintV1(ruleset, &fingerprint));
        CHKiRet(rsReloadNormalizedGraphBuilderV1Add(builder, RS_RELOAD_OBJ_RULESET, identity, fingerprint));
        free(identity);
        identity = NULL;
        free(fingerprint);
        fingerprint = NULL;
    }
    *ppBuilder = builder;
    builder = NULL;

finalize_it:
    free(identity);
    free(fingerprint);
    rsReloadNormalizedGraphBuilderV1Destruct(&builder);
    RETiRet;
}
