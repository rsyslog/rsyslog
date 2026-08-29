/*
 * Unit coverage for abort-safe reload syntax cloning. Every expression and
 * statement kind accepted by the production clone is represented here. The
 * source tree is destroyed before inspecting the clone, proving independent
 * ownership. Unsupported nodes placed after supported subtrees must fail
 * atomically with a NULL output; leak checking is the cleanup oracle.
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
        case CMP_NE:
        case CMP_EQ:
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
            cnfexprDestruct(expr->l);
            cnfexprDestruct(expr->r);
            break;
        case NOT:
        case 'M':
            cnfexprDestruct(expr->r);
            break;
        case 'N':
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
            case S_NOP:
            case S_STOP:
                break;
            case S_CALL:
                es_deleteStr(stmt->d.s_call.name);
                break;
            case S_CALL_INDIRECT:
                cnfexprDestruct(stmt->d.s_call_ind.expr);
                break;
            case S_IF:
                cnfexprDestruct(stmt->d.s_if.expr);
                cnfstmtDestructLst(stmt->d.s_if.t_then);
                cnfstmtDestructLst(stmt->d.s_if.t_else);
                break;
            case S_FOREACH:
                cnfIteratorDestruct(stmt->d.s_foreach.iter);
                cnfstmtDestructLst(stmt->d.s_foreach.body);
                break;
            case S_SET:
                free(stmt->d.s_set.varname);
                cnfexprDestruct(stmt->d.s_set.expr);
                break;
            case S_UNSET:
                free(stmt->d.s_unset.varname);
                break;
            case S_RELOAD_ACT:
                nvlstDestruct(stmt->d.reload_action);
                break;
            case S_RELOAD_PRIFILT:
                cnfstmtDestructLst(stmt->d.s_prifilt.t_then);
                cnfstmtDestructLst(stmt->d.s_prifilt.t_else);
                break;
            case S_RELOAD_PROPFILT:
                cnfstmtDestructLst(stmt->d.s_propfilt.t_then);
                cnfstmtDestructLst(stmt->d.s_propfilt.t_else);
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

static struct cnfexpr *unary(const unsigned type, struct cnfexpr *const operand) {
    struct cnfexpr *const result = calloc(1, sizeof(*result));
    if (result != NULL) {
        result->nodetype = type;
        result->r = operand;
    }
    return result;
}

static struct cnfstringval *stringValue(const char *const value) {
    struct cnfstringval *const result = calloc(1, sizeof(*result));
    if (result != NULL) {
        result->nodetype = 'S';
        result->estr = es_newStrFromCStr(value, strlen(value));
        if (result->estr == NULL) {
            free(result);
            return NULL;
        }
    }
    return result;
}

static struct cnfvar *variableExpression(const char *const name) {
    struct cnfvar *const result = calloc(1, sizeof(*result));
    if (result != NULL) {
        result->nodetype = 'V';
        result->name = strdup(name);
        if (result->name == NULL) {
            free(result);
            return NULL;
        }
    }
    return result;
}

static struct cnffuncexists *existsExpression(const char *const name) {
    struct cnffuncexists *const result = calloc(1, sizeof(*result));
    if (result != NULL) {
        result->nodetype = S_FUNC_EXISTS;
        result->varname = strdup(name);
        if (result->varname == NULL) {
            free(result);
            return NULL;
        }
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
    static const unsigned binaryTypes[] = {CMP_EQ,
                                           CMP_NE,
                                           CMP_LE,
                                           CMP_GE,
                                           CMP_LT,
                                           CMP_GT,
                                           CMP_CONTAINS,
                                           CMP_CONTAINSI,
                                           CMP_STARTSWITH,
                                           CMP_STARTSWITHI,
                                           CMP_ENDSWITH,
                                           OR,
                                           AND,
                                           '&',
                                           '+',
                                           '-',
                                           '*',
                                           '/',
                                           '%'};

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

    for (size_t i = 0; i < sizeof(binaryTypes) / sizeof(binaryTypes[0]); ++i) {
        source = binary(binaryTypes[i], (struct cnfexpr *)number(1), (struct cnfexpr *)number(2));
        CHECK(source != NULL && source->l != NULL && source->r != NULL);
        CHECK(cnfexprCloneReloadSafe(source, &clone) == RS_RET_OK);
        cnfexprDestruct(source);
        CHECK(clone->nodetype == binaryTypes[i]);
        cnfexprDestruct(clone);
        clone = NULL;
    }

    source = binary(CMP_EQ, (struct cnfexpr *)stringValue("value"), (struct cnfexpr *)existsExpression("$!present"));
    CHECK(source != NULL && source->l != NULL && source->r != NULL);
    CHECK(cnfexprCloneReloadSafe(source, &clone) == RS_RET_OK);
    cnfexprDestruct(source);
    CHECK(estrEquals(((struct cnfstringval *)clone->l)->estr, "value"));
    CHECK(strcmp(((struct cnffuncexists *)clone->r)->varname, "$!present") == 0);
    cnfexprDestruct(clone);
    clone = NULL;

    for (size_t i = 0; i < 2; ++i) {
        const unsigned type = i == 0 ? NOT : 'M';
        source = unary(type, (struct cnfexpr *)number(3));
        CHECK(source != NULL && source->r != NULL);
        CHECK(cnfexprCloneReloadSafe(source, &clone) == RS_RET_OK);
        cnfexprDestruct(source);
        CHECK(clone->nodetype == type);
        cnfexprDestruct(clone);
        clone = NULL;
    }

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

    struct cnfexpr *const unsupportedHeap = calloc(1, sizeof(*unsupportedHeap));
    CHECK(unsupportedHeap != NULL);
    unsupportedHeap->nodetype = 'F';
    source = binary(CMP_EQ, (struct cnfexpr *)variableExpression("$.partial"), unsupportedHeap);
    CHECK(source != NULL && source->l != NULL && source->r != NULL);
    CHECK(cnfexprCloneReloadSafe(source, &clone) == RS_RET_NOT_IMPLEMENTED);
    CHECK(clone == NULL);
    cnfexprDestruct(source);
    return 0;
}

static struct cnfstmt *reloadAction(void) {
    struct cnfstmt *const result = statement(S_RELOAD_ACT);
    struct nvlst *const stringParam = calloc(1, sizeof(*stringParam));
    struct nvlst *const numberParam = calloc(1, sizeof(*numberParam));
    struct nvlst *const arrayParam = calloc(1, sizeof(*arrayParam));
    if (result == NULL || stringParam == NULL || numberParam == NULL || arrayParam == NULL) {
        free(result);
        free(stringParam);
        free(numberParam);
        free(arrayParam);
        return NULL;
    }
    stringParam->name = es_newStrFromCStr("type", 4);
    stringParam->val.datatype = 'S';
    stringParam->val.d.estr = es_newStrFromCStr("omfile", 6);
    numberParam->name = es_newStrFromCStr("retry", 5);
    numberParam->val.datatype = 'N';
    numberParam->val.d.n = 7;
    arrayParam->name = es_newStrFromCStr("tags", 4);
    arrayParam->val.datatype = 'A';
    arrayParam->val.d.ar = calloc(1, sizeof(*arrayParam->val.d.ar));
    if (stringParam->name == NULL || stringParam->val.d.estr == NULL || numberParam->name == NULL ||
        arrayParam->name == NULL || arrayParam->val.d.ar == NULL) {
        stringParam->next = numberParam;
        numberParam->next = arrayParam;
        nvlstDestruct(stringParam);
        free(result);
        return NULL;
    }
    arrayParam->val.d.ar->nodetype = 'A';
    arrayParam->val.d.ar->nmemb = 1;
    arrayParam->val.d.ar->arr = calloc(1, sizeof(*arrayParam->val.d.ar->arr));
    if (arrayParam->val.d.ar->arr != NULL) arrayParam->val.d.ar->arr[0] = es_newStrFromCStr("reload", 6);
    if (arrayParam->val.d.ar->arr == NULL || arrayParam->val.d.ar->arr[0] == NULL) {
        stringParam->next = numberParam;
        numberParam->next = arrayParam;
        nvlstDestruct(stringParam);
        free(result);
        return NULL;
    }
    stringParam->next = numberParam;
    numberParam->next = arrayParam;
    result->d.reload_action = stringParam;
    return result;
}

static int testStatementClone(void) {
    struct cnfstmt *source = statement(S_IF);
    struct cnfstmt *clone = NULL;
    struct cnfstmt *filter;
    struct cnfstmt *tail;

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

    tail = source;
    tail->next = statement(S_FOREACH);
    tail = tail->next;
    CHECK(tail != NULL);
    tail->d.s_foreach.iter = calloc(1, sizeof(*tail->d.s_foreach.iter));
    CHECK(tail->d.s_foreach.iter != NULL);
    tail->d.s_foreach.iter->var = strdup("$.item");
    tail->d.s_foreach.iter->collection = (struct cnfexpr *)variableExpression("$!items");
    tail->d.s_foreach.body = reloadAction();
    CHECK(tail->d.s_foreach.iter->var != NULL && tail->d.s_foreach.iter->collection != NULL &&
          tail->d.s_foreach.body != NULL);

    tail->next = statement(S_CALL);
    tail = tail->next;
    CHECK(tail != NULL);
    tail->d.s_call.name = es_newStrFromCStr("target", 6);
    CHECK(tail->d.s_call.name != NULL);

    tail->next = statement(S_UNSET);
    tail = tail->next;
    CHECK(tail != NULL);
    tail->d.s_unset.varname = (uchar *)strdup("$.old");
    CHECK(tail->d.s_unset.varname != NULL);

    tail->next = statement(S_RELOAD_PROPFILT);
    tail = tail->next;
    CHECK(tail != NULL);
    tail->printable = (uchar *)strdup(":msg, contains, value");
    tail->d.s_propfilt.t_then = statement(S_NOP);
    tail->d.s_propfilt.t_else = reloadAction();
    CHECK(tail->printable != NULL && tail->d.s_propfilt.t_then != NULL && tail->d.s_propfilt.t_else != NULL);

    tail->next = statement(S_CALL_INDIRECT);
    tail = tail->next;
    CHECK(tail != NULL);
    tail->d.s_call_ind.expr = (struct cnfexpr *)stringValue("target");
    CHECK(tail->d.s_call_ind.expr != NULL);

    CHECK(cnfstmtCloneReloadSafe(source, &clone) == RS_RET_OK);
    cnfstmtDestructLst(source);
    CHECK(strcmp((char *)clone->printable, "if expression") == 0);
    CHECK(strcmp((char *)clone->d.s_if.t_then->d.s_set.varname, "$.route") == 0);
    CHECK(clone->d.s_if.t_else->d.s_prifilt.t_then->nodetype == S_RELOAD_ACT);
    CHECK(estrEquals(clone->d.s_if.t_else->d.s_prifilt.t_then->d.reload_action->val.d.estr, "omfile"));
    CHECK(clone->next->nodetype == S_FOREACH);
    CHECK(strcmp(clone->next->d.s_foreach.iter->var, "$.item") == 0);
    CHECK(clone->next->next->nodetype == S_CALL);
    CHECK(estrEquals(clone->next->next->d.s_call.name, "target"));
    CHECK(clone->next->next->next->nodetype == S_UNSET);
    CHECK(strcmp((char *)clone->next->next->next->d.s_unset.varname, "$.old") == 0);
    CHECK(clone->next->next->next->next->nodetype == S_RELOAD_PROPFILT);
    CHECK(clone->next->next->next->next->d.s_propfilt.t_else->d.reload_action->next->val.d.n == 7);
    CHECK(estrEquals(clone->next->next->next->next->d.s_propfilt.t_else->d.reload_action->next->next->val.d.ar->arr[0],
                     "reload"));
    CHECK(clone->next->next->next->next->next->nodetype == S_CALL_INDIRECT);
    CHECK(estrEquals(((struct cnfstringval *)clone->next->next->next->next->next->d.s_call_ind.expr)->estr, "target"));
    cnfstmtDestructLst(clone);
    clone = NULL;

    source = statement(S_FOREACH);
    CHECK(source != NULL);
    source->d.s_foreach.iter = calloc(1, sizeof(*source->d.s_foreach.iter));
    CHECK(source->d.s_foreach.iter != NULL);
    source->d.s_foreach.iter->var = strdup("$.partial");
    source->d.s_foreach.iter->collection = (struct cnfexpr *)variableExpression("$!partial");
    source->d.s_foreach.body = statement(S_CALL_INDIRECT);
    CHECK(source->d.s_foreach.iter->var != NULL && source->d.s_foreach.iter->collection != NULL &&
          source->d.s_foreach.body != NULL);
    source->d.s_foreach.body->d.s_call_ind.expr = (struct cnfexpr *)stringValue("partial");
    CHECK(source->d.s_foreach.body->d.s_call_ind.expr != NULL);
    source->next = statement(S_RELOAD_PROPFILT);
    CHECK(source->next != NULL);
    source->next->printable = (uchar *)strdup(":msg, contains, partial");
    source->next->d.s_propfilt.t_then = statement(S_UNSET);
    CHECK(source->next->printable != NULL && source->next->d.s_propfilt.t_then != NULL);
    source->next->d.s_propfilt.t_then->d.s_unset.varname = (uchar *)strdup("$.partial");
    source->next->d.s_propfilt.t_else = statement(S_RELOAD_LOOKUP_TABLE);
    CHECK(source->next->d.s_propfilt.t_then->d.s_unset.varname != NULL && source->next->d.s_propfilt.t_else != NULL);
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
