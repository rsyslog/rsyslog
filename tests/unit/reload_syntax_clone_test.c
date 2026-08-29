/*
 * Unit coverage for abort-safe reload syntax cloning. The source tree is
 * destroyed before inspecting the clone, proving independent ownership.
 * Unsupported executable nodes must fail atomically with a NULL output.
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rsyslog.h"
#include "grammar.h"
#include "rainerscript.h"
#include "msg.h"

static int failPropFill;

rsRetVal msgPropDescrFill(msgPropDescr_t *const prop, uchar *const name, const int nameLen) {
    memset(prop, 0, sizeof(*prop));
    if (failPropFill) return RS_RET_OUT_OF_MEMORY;
    if (nameLen > 2 && name[0] == '$' && name[1] == '.') {
        prop->name = (uchar *)strdup((const char *)name + 1);
        if (prop->name == NULL) return RS_RET_OUT_OF_MEMORY;
        prop->name[0] = '!';
        prop->nameLen = nameLen - 1;
        prop->id = PROP_LOCAL_VAR;
    }
    return RS_RET_OK;
}

void msgPropDescrDestruct(msgPropDescr_t *const prop) {
    if (prop->id == PROP_CEE || prop->id == PROP_LOCAL_VAR || prop->id == PROP_GLOBAL_VAR) free(prop->name);
    memset(prop, 0, sizeof(*prop));
}

static void arrayDestruct(struct cnfarray *const array) {
    if (array == NULL) return;
    for (int i = 0; i < array->nmemb; ++i) es_deleteStr(array->arr[i]);
    free(array->arr);
    free(array);
}

void cnfexprDestruct(struct cnfexpr *const expr) {
    if (expr == NULL) return;
    switch (expr->nodetype) {
        case CMP_EQ:
        case CMP_CONTAINS:
            cnfexprDestruct(expr->l);
            cnfexprDestruct(expr->r);
            break;
        case 'S':
            es_deleteStr(((struct cnfstringval *)expr)->estr);
            break;
        case 'V':
            free(((struct cnfvar *)expr)->name);
            msgPropDescrDestruct(&((struct cnfvar *)expr)->prop);
            break;
        case 'A':
            arrayDestruct((struct cnfarray *)expr);
            return;
        case S_FUNC_EXISTS:
            free((void *)((struct cnffuncexists *)expr)->varname);
            msgPropDescrDestruct(&((struct cnffuncexists *)expr)->prop);
            break;
        default:
            break;
    }
    free(expr);
}

void nvlstDestruct(struct nvlst *list) {
    while (list != NULL) {
        struct nvlst *const next = list->next;
        es_deleteStr(list->name);
        if (list->val.datatype == 'S')
            es_deleteStr(list->val.d.estr);
        else if (list->val.datatype == 'A')
            arrayDestruct(list->val.d.ar);
        free(list);
        list = next;
    }
}

void cnfIteratorDestruct(struct cnfitr *const iterator) {
    if (iterator == NULL) return;
    free(iterator->var);
    cnfexprDestruct(iterator->collection);
    free(iterator);
}

void cnfstmtDestructLst(struct cnfstmt *stmt) {
    while (stmt != NULL) {
        struct cnfstmt *const next = stmt->next;
        switch (stmt->nodetype) {
            case S_IF:
                cnfexprDestruct(stmt->d.s_if.expr);
                cnfstmtDestructLst(stmt->d.s_if.t_then);
                cnfstmtDestructLst(stmt->d.s_if.t_else);
                break;
            case S_SET:
                free(stmt->d.s_set.varname);
                cnfexprDestruct(stmt->d.s_set.expr);
                break;
            case S_RELOAD_ACT:
                nvlstDestruct(stmt->d.reload_action);
                break;
            case S_RELOAD_PRIFILT:
                cnfstmtDestructLst(stmt->d.s_prifilt.t_then);
                cnfstmtDestructLst(stmt->d.s_prifilt.t_else);
                break;
            default:
                break;
        }
        free(stmt->printable);
        free(stmt);
        stmt = next;
    }
}

#include "../../grammar/reload-syntax-clone.c"

#define CHECK(condition)                                                                    \
    do {                                                                                    \
        if (!(condition)) {                                                                 \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            return 1;                                                                       \
        }                                                                                   \
    } while (0)

static int estrEquals(es_str_t *const value, const char *const expected) {
    const size_t len = strlen(expected);
    return value != NULL && es_strlen(value) == len && memcmp(es_getBufAddr(value), expected, len) == 0;
}

static struct cnfnumval *number(const long long value) {
    struct cnfnumval *const result = calloc(1, sizeof(*result));
    if (result != NULL) {
        result->nodetype = 'N';
        result->val = value;
    }
    return result;
}

static struct cnfexpr *binary(const unsigned type, struct cnfexpr *const left, struct cnfexpr *const right) {
    struct cnfexpr *const result = calloc(1, sizeof(*result));
    if (result != NULL) {
        result->nodetype = type;
        result->l = left;
        result->r = right;
    }
    return result;
}

static struct cnfstmt *statement(const unsigned type) {
    struct cnfstmt *const result = calloc(1, sizeof(*result));
    if (result != NULL) result->nodetype = type;
    return result;
}

static int testExpressionClone(void) {
    struct cnfarray *array = calloc(1, sizeof(*array));
    struct cnfvar *variable = calloc(1, sizeof(*variable));
    struct cnfexpr *source;
    struct cnfexpr *clone = NULL;
    struct cnfexpr unsupported = {.nodetype = 'F'};

    CHECK(array != NULL && variable != NULL);
    array->nodetype = 'A';
    array->nmemb = 2;
    array->arr = calloc(2, sizeof(*array->arr));
    CHECK(array->arr != NULL);
    array->arr[0] = es_newStrFromCStr("alpha", 5);
    array->arr[1] = es_newStrFromCStr("beta", 4);
    variable->nodetype = 'V';
    variable->name = strdup("$.route");
    CHECK(array->arr[0] != NULL && array->arr[1] != NULL && variable->name != NULL);
    source = binary(CMP_CONTAINS, (struct cnfexpr *)variable, (struct cnfexpr *)array);
    CHECK(source != NULL);
    CHECK(cnfexprCloneReloadSafe(source, &clone) == RS_RET_OK);
    cnfexprDestruct(source);
    CHECK(strcmp(((struct cnfvar *)clone->l)->name, "$.route") == 0);
    CHECK(((struct cnfvar *)clone->l)->prop.id == PROP_LOCAL_VAR);
    CHECK(strcmp((const char *)((struct cnfvar *)clone->l)->prop.name, "!route") == 0);
    CHECK(estrEquals(((struct cnfarray *)clone->r)->arr[1], "beta"));
    cnfexprDestruct(clone);
    clone = NULL;

    failPropFill = 1;
    variable = calloc(1, sizeof(*variable));
    CHECK(variable != NULL);
    variable->nodetype = 'V';
    variable->name = strdup("$.route");
    CHECK(variable->name != NULL);
    CHECK(cnfexprCloneReloadSafe((struct cnfexpr *)variable, &clone) == RS_RET_OUT_OF_MEMORY);
    CHECK(clone == NULL);
    failPropFill = 0;
    cnfexprDestruct((struct cnfexpr *)variable);

    CHECK(cnfexprCloneReloadSafe(&unsupported, &clone) == RS_RET_NOT_IMPLEMENTED);
    CHECK(clone == NULL);
    return 0;
}

static struct cnfstmt *reloadAction(void) {
    struct cnfstmt *const result = statement(S_RELOAD_ACT);
    struct nvlst *const param = calloc(1, sizeof(*param));
    if (result == NULL || param == NULL) {
        free(result);
        free(param);
        return NULL;
    }
    param->name = es_newStrFromCStr("type", 4);
    param->val.datatype = 'S';
    param->val.d.estr = es_newStrFromCStr("omfile", 6);
    result->d.reload_action = param;
    return result;
}

static int testStatementClone(void) {
    struct cnfstmt *source = statement(S_IF);
    struct cnfstmt *clone = NULL;
    struct cnfstmt *filter;

    CHECK(source != NULL);
    source->printable = (uchar *)strdup("if expression");
    source->d.s_if.expr = binary(CMP_EQ, (struct cnfexpr *)number(1), (struct cnfexpr *)number(1));
    source->d.s_if.t_then = statement(S_SET);
    CHECK(source->printable != NULL && source->d.s_if.expr != NULL && source->d.s_if.t_then != NULL);
    source->d.s_if.t_then->d.s_set.varname = (uchar *)strdup("$.route");
    source->d.s_if.t_then->d.s_set.expr = (struct cnfexpr *)number(7);
    filter = statement(S_RELOAD_PRIFILT);
    CHECK(filter != NULL);
    filter->printable = (uchar *)strdup("*.info");
    filter->d.s_prifilt.t_then = reloadAction();
    filter->d.s_prifilt.t_else = statement(S_STOP);
    CHECK(filter->printable != NULL && filter->d.s_prifilt.t_then != NULL && filter->d.s_prifilt.t_else != NULL);
    source->d.s_if.t_else = filter;

    CHECK(cnfstmtCloneReloadSafe(source, &clone) == RS_RET_OK);
    cnfstmtDestructLst(source);
    CHECK(strcmp((char *)clone->printable, "if expression") == 0);
    CHECK(strcmp((char *)clone->d.s_if.t_then->d.s_set.varname, "$.route") == 0);
    CHECK(clone->d.s_if.t_else->d.s_prifilt.t_then->nodetype == S_RELOAD_ACT);
    CHECK(estrEquals(clone->d.s_if.t_else->d.s_prifilt.t_then->d.reload_action->val.d.estr, "omfile"));
    cnfstmtDestructLst(clone);
    clone = NULL;

    source = statement(S_RELOAD_LOOKUP_TABLE);
    CHECK(source != NULL);
    CHECK(cnfstmtCloneReloadSafe(source, &clone) == RS_RET_NOT_IMPLEMENTED);
    CHECK(clone == NULL);
    cnfstmtDestructLst(source);
    return 0;
}

int main(void) {
    if (testExpressionClone() != 0 || testStatementClone() != 0) return 1;
    puts("reload syntax clone tests passed");
    return 0;
}
