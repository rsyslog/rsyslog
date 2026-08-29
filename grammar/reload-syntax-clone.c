/* Abort-safe, side-effect-free cloning for private reload syntax trees. */
#include "config.h"

#include "rsyslog.h"
#include "grammar.h"
#include "rainerscript.h"
#include "msg.h"

rsRetVal cnfexprCloneReloadSafe(const struct cnfexpr *src, struct cnfexpr **out) {
    struct cnfexpr *copy = NULL;
    DEFiRet;
    if (src == NULL || out == NULL || *out != NULL) return RS_RET_PARAM_ERROR;
    switch (src->nodetype) {
        case 'N':
            CHKmalloc(copy = malloc(sizeof(struct cnfnumval)));
            *(struct cnfnumval *)copy = *(const struct cnfnumval *)src;
            break;
        case 'S': {
            const struct cnfstringval *value = (const struct cnfstringval *)src;
            struct cnfstringval *target;
            CHKmalloc(target = calloc(1, sizeof(*target)));
            copy = (struct cnfexpr *)target;
            target->nodetype = 'S';
            if (value->estr != NULL) CHKmalloc(target->estr = es_strdup(value->estr));
            break;
        }
        case 'V': {
            const struct cnfvar *value = (const struct cnfvar *)src;
            struct cnfvar *target;
            if (value->name == NULL) ABORT_FINALIZE(RS_RET_PARAM_ERROR);
            CHKmalloc(target = calloc(1, sizeof(*target)));
            copy = (struct cnfexpr *)target;
            target->nodetype = 'V';
            CHKmalloc(target->name = strdup(value->name));
            /* prop is derived by the normal constructor, never borrowed. */
            CHKiRet(msgPropDescrFill(&target->prop, (uchar *)target->name, strlen(target->name)));
            break;
        }
        case 'A': {
            const struct cnfarray *value = (const struct cnfarray *)src;
            struct cnfarray *target;
            int i;
            if (value->nmemb < 0 || (value->nmemb != 0 && value->arr == NULL)) ABORT_FINALIZE(RS_RET_PARAM_ERROR);
            CHKmalloc(target = calloc(1, sizeof(*target)));
            copy = (struct cnfexpr *)target;
            target->nodetype = 'A';
            target->nmemb = value->nmemb;
            if (value->nmemb != 0) CHKmalloc(target->arr = calloc((size_t)value->nmemb, sizeof(*target->arr)));
            for (i = 0; i < value->nmemb; ++i)
                if (value->arr[i] != NULL) CHKmalloc(target->arr[i] = es_strdup(value->arr[i]));
            break;
        }
        case S_FUNC_EXISTS: {
            const struct cnffuncexists *value = (const struct cnffuncexists *)src;
            struct cnffuncexists *target;
            if (value->varname == NULL) ABORT_FINALIZE(RS_RET_PARAM_ERROR);
            CHKmalloc(target = calloc(1, sizeof(*target)));
            copy = (struct cnfexpr *)target;
            target->nodetype = S_FUNC_EXISTS;
            CHKmalloc(target->varname = strdup(value->varname));
            CHKiRet(msgPropDescrFill(&target->prop, (uchar *)target->varname, strlen(target->varname)));
            break;
        }
        case NOT:
        case 'M':
            CHKmalloc(copy = calloc(1, sizeof(*copy)));
            copy->nodetype = src->nodetype;
            CHKiRet(cnfexprCloneReloadSafe(src->r, &copy->r));
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
            CHKmalloc(copy = calloc(1, sizeof(*copy)));
            copy->nodetype = src->nodetype;
            CHKiRet(cnfexprCloneReloadSafe(src->l, &copy->l));
            CHKiRet(cnfexprCloneReloadSafe(src->r, &copy->r));
            break;
        default:
            ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
    }
    *out = copy;
    copy = NULL;
finalize_it:
    cnfexprDestruct(copy);
    RETiRet;
}

rsRetVal nvlstCloneReloadSafe(const struct nvlst *src, struct nvlst **out) {
    struct nvlst *head = NULL;
    struct nvlst **next = &head;
    DEFiRet;

    if (out == NULL || *out != NULL) return RS_RET_PARAM_ERROR;
    while (src != NULL) {
        struct nvlst *node;
        CHKmalloc(node = calloc(1, sizeof(*node)));
        *next = node;
        next = &node->next;
        if (src->name != NULL) CHKmalloc(node->name = es_strdup(src->name));
        node->val.datatype = src->val.datatype;
        switch (src->val.datatype) {
            case 'N':
                node->val.d.n = src->val.d.n;
                break;
            case 'S':
                if (src->val.d.estr != NULL) CHKmalloc(node->val.d.estr = es_strdup(src->val.d.estr));
                break;
            case 'A': {
                struct cnfexpr *arrayCopy = NULL;
                if (src->val.d.ar != NULL) {
                    CHKiRet(cnfexprCloneReloadSafe((const struct cnfexpr *)src->val.d.ar, &arrayCopy));
                    node->val.d.ar = (struct cnfarray *)arrayCopy;
                }
                break;
            }
            default:
                ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
        }
        src = src->next;
    }
    *out = head;
    head = NULL;
finalize_it:
    nvlstDestruct(head);
    RETiRet;
}

static rsRetVal cnfIteratorCloneReloadSyntax(const struct cnfitr *src, struct cnfitr **out) {
    struct cnfitr *copy = NULL;
    DEFiRet;

    if (src == NULL || src->var == NULL || out == NULL || *out != NULL) return RS_RET_PARAM_ERROR;
    CHKmalloc(copy = calloc(1, sizeof(*copy)));
    CHKmalloc(copy->var = strdup(src->var));
    CHKiRet(cnfexprCloneReloadSafe(src->collection, &copy->collection));
    *out = copy;
    copy = NULL;
finalize_it:
    cnfIteratorDestruct(copy);
    RETiRet;
}

rsRetVal cnfstmtCloneReloadSyntax(const struct cnfstmt *src, struct cnfstmt **out) {
    struct cnfstmt *head = NULL;
    struct cnfstmt **next = &head;
    DEFiRet;

    if (out == NULL || *out != NULL) return RS_RET_PARAM_ERROR;
    while (src != NULL) {
        struct cnfstmt *copy;
        CHKmalloc(copy = calloc(1, sizeof(*copy)));
        copy->nodetype = src->nodetype;
        *next = copy;
        next = &copy->next;
        if (src->printable != NULL) CHKmalloc(copy->printable = (uchar *)strdup((const char *)src->printable));
        switch (src->nodetype) {
            case S_NOP:
            case S_STOP:
                break;
            case S_IF:
                CHKiRet(cnfexprCloneReloadSafe(src->d.s_if.expr, &copy->d.s_if.expr));
                CHKiRet(cnfstmtCloneReloadSyntax(src->d.s_if.t_then, &copy->d.s_if.t_then));
                CHKiRet(cnfstmtCloneReloadSyntax(src->d.s_if.t_else, &copy->d.s_if.t_else));
                break;
            case S_FOREACH:
                CHKiRet(cnfIteratorCloneReloadSyntax(src->d.s_foreach.iter, &copy->d.s_foreach.iter));
                CHKiRet(cnfstmtCloneReloadSyntax(src->d.s_foreach.body, &copy->d.s_foreach.body));
                break;
            case S_SET:
                if (src->d.s_set.varname == NULL) ABORT_FINALIZE(RS_RET_PARAM_ERROR);
                CHKmalloc(copy->d.s_set.varname = (uchar *)strdup((const char *)src->d.s_set.varname));
                copy->d.s_set.force_reset = src->d.s_set.force_reset;
                CHKiRet(cnfexprCloneReloadSafe(src->d.s_set.expr, &copy->d.s_set.expr));
                break;
            case S_UNSET:
                if (src->d.s_set.varname == NULL) ABORT_FINALIZE(RS_RET_PARAM_ERROR);
                CHKmalloc(copy->d.s_set.varname = (uchar *)strdup((const char *)src->d.s_set.varname));
                break;
            case S_CALL:
                if (src->d.s_call.name == NULL) ABORT_FINALIZE(RS_RET_PARAM_ERROR);
                CHKmalloc(copy->d.s_call.name = es_strdup(src->d.s_call.name));
                break;
            case S_CALL_INDIRECT:
                CHKiRet(cnfexprCloneReloadSafe(src->d.s_call_ind.expr, &copy->d.s_call_ind.expr));
                break;
            case S_RELOAD_ACT:
                CHKiRet(nvlstCloneReloadSafe(src->d.reload_action, &copy->d.reload_action));
                break;
            case S_RELOAD_PRIFILT:
                CHKiRet(cnfstmtCloneReloadSyntax(src->d.s_prifilt.t_then, &copy->d.s_prifilt.t_then));
                CHKiRet(cnfstmtCloneReloadSyntax(src->d.s_prifilt.t_else, &copy->d.s_prifilt.t_else));
                break;
            case S_RELOAD_PROPFILT:
                CHKiRet(cnfstmtCloneReloadSyntax(src->d.s_propfilt.t_then, &copy->d.s_propfilt.t_then));
                CHKiRet(cnfstmtCloneReloadSyntax(src->d.s_propfilt.t_else, &copy->d.s_propfilt.t_else));
                break;
            default:
                ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
        }
        src = src->next;
    }
    *out = head;
    head = NULL;
finalize_it:
    cnfstmtDestructLst(head);
    RETiRet;
}
