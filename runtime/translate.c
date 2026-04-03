/** @file translate.c
 * @brief Canonical config translation support for rsyslog.
 *
 * Captures config objects and script bodies during config load so rsyslogd
 * can emit canonical RainerScript or YAML during validation runs.
 *
 * Copyright 2026 Rainer Gerhards and Adiscon GmbH.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *       -or-
 *       see COPYING.ASL20 in the source distribution
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "config.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rsyslog.h"
#include "translate.h"
#include "action.h"
#include "errmsg.h"
#include "msg.h"
#include "grammar/grammar.h"

static char *translateVasprintf(const char *fmt, va_list ap);
static void cnfarrayCloneDestruct(struct cnfarray *ar);
static int preferredKeyRank(const struct nvlst *n);
static int nvlstSortComesBefore(const struct nvlst *a, const struct nvlst *b);

struct rsconfTranslateWarning_s {
    char *msg;
    struct rsconfTranslateWarning_s *next;
};

struct rsconfTranslateItem_s {
    enum cnfobjType objType;
    struct nvlst *nvlst;
    struct objlst *subobjs;
    char *script;
    char *source;
    int line;
    struct rsconfTranslateWarning_s *warnings;
    struct rsconfTranslateItem_s *next;
};

struct rsconfTranslateState_s {
    enum rsconfTranslateFormat fmt;
    int fatal;
    struct rsconfTranslateWarning_s *globalWarnings;
    struct rsconfTranslateItem_s *globals;
    struct rsconfTranslateItem_s *mainqueue;
    struct rsconfTranslateItem_s *modules;
    struct rsconfTranslateItem_s *inputs;
    struct rsconfTranslateItem_s *templates;
    struct rsconfTranslateItem_s *rulesets;
    struct rsconfTranslateItem_s *lookups;
    struct rsconfTranslateItem_s *parsers;
    struct rsconfTranslateItem_s *timezones;
    struct rsconfTranslateItem_s *dynstats;
    struct rsconfTranslateItem_s *perctilestats;
    struct rsconfTranslateItem_s *ratelimits;
};

/* Concurrency & Locking:
 *   Translation capture is only active during single-threaded config load and
 *   validation. g_tx is process-global mutable state with no locking because it
 *   is not accessed concurrently; runtime worker threads are not started while
 *   translation state is being built or emitted.
 */
static struct rsconfTranslateState_s g_tx = {
    RSCONF_TRANSLATE_NONE, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

static void cnfarrayCloneDestruct(struct cnfarray *ar) {
    if (ar == NULL) return;
    cnfarrayContentDestruct(ar);
    free(ar);
}

static void warningsDestruct(struct rsconfTranslateWarning_s *w) {
    struct rsconfTranslateWarning_s *n;
    while (w != NULL) {
        n = w->next;
        free(w->msg);
        free(w);
        w = n;
    }
}

static void itemDestruct(struct rsconfTranslateItem_s *it) {
    while (it != NULL) {
        struct rsconfTranslateItem_s *n = it->next;
        nvlstDestruct(it->nvlst);
        objlstDestruct(it->subobjs);
        free(it->script);
        free(it->source);
        warningsDestruct(it->warnings);
        free(it);
        it = n;
    }
}

void rsconfTranslateCleanup(void) {
    warningsDestruct(g_tx.globalWarnings);
    itemDestruct(g_tx.globals);
    itemDestruct(g_tx.mainqueue);
    itemDestruct(g_tx.modules);
    itemDestruct(g_tx.inputs);
    itemDestruct(g_tx.templates);
    itemDestruct(g_tx.rulesets);
    itemDestruct(g_tx.lookups);
    itemDestruct(g_tx.parsers);
    itemDestruct(g_tx.timezones);
    itemDestruct(g_tx.dynstats);
    itemDestruct(g_tx.perctilestats);
    itemDestruct(g_tx.ratelimits);
    memset(&g_tx, 0, sizeof(g_tx));
}

void rsconfTranslateConfigure(enum rsconfTranslateFormat fmt) {
    rsconfTranslateCleanup();
    g_tx.fmt = fmt;
}

int rsconfTranslateEnabled(void) {
    return g_tx.fmt != RSCONF_TRANSLATE_NONE;
}

int rsconfTranslateHasFatal(void) {
    return g_tx.fatal;
}

PRAGMA_DIAGNOSTIC_PUSH
PRAGMA_IGNORE_Wformat_nonliteral static char *translateVasprintf(const char *fmt, va_list ap) {
    va_list ap_copy;
    char *buf;
    int len;

    va_copy(ap_copy, ap);
    len = vsnprintf(NULL, 0, fmt, ap_copy);
    va_end(ap_copy);
    if (len < 0) return NULL;
    buf = malloc((size_t)len + 1);
    if (buf == NULL) return NULL;
    va_copy(ap_copy, ap);
    if (vsnprintf(buf, (size_t)len + 1, fmt, ap_copy) != len) {
        va_end(ap_copy);
        free(buf);
        return NULL;
    }
    va_end(ap_copy);
    return buf;
}

PRAGMA_IGNORE_Wformat_nonliteral static void addWarning(struct rsconfTranslateWarning_s **head, const char *fmt, ...) {
    va_list ap;
    char *buf;
    struct rsconfTranslateWarning_s *w;

    va_start(ap, fmt);
    buf = translateVasprintf(fmt, ap);
    va_end(ap);
    if (buf == NULL) return;

    w = calloc(1, sizeof(*w));
    if (w == NULL) {
        free(buf);
        return;
    }
    w->msg = buf;
    if (*head == NULL) {
        *head = w;
    } else {
        struct rsconfTranslateWarning_s *cur = *head;
        while (cur->next != NULL) cur = cur->next;
        cur->next = w;
    }
}

void rsconfTranslateAddUnsupported(const char *source, int line, const char *fmt, ...) {
    va_list ap;
    char *buf;

    va_start(ap, fmt);
    buf = translateVasprintf(fmt, ap);
    va_end(ap);
    g_tx.fatal = 1;
    addWarning(&g_tx.globalWarnings, "%s:%d: %s", source == NULL ? "<config>" : source, line,
               buf == NULL ? "translator: failed to format error message" : buf);
    free(buf);
}

PRAGMA_DIAGNOSTIC_POP

static es_str_t *cloneEstr(const es_str_t *in) {
    if (in == NULL) return NULL;
    return es_newStrFromCStr((char *)es_getBufAddr((es_str_t *)in), es_strlen((es_str_t *)in));
}

static struct cnfarray *cloneArray(const struct cnfarray *ar) {
    int i;
    struct cnfarray *out;
    if (ar == NULL) return NULL;
    out = calloc(1, sizeof(*out));
    if (out == NULL) return NULL;
    out->nodetype = ar->nodetype;
    out->nmemb = ar->nmemb;
    if (ar->nmemb > 0) {
        out->arr = calloc(ar->nmemb, sizeof(es_str_t *));
        if (out->arr == NULL) {
            free(out);
            return NULL;
        }
        for (i = 0; i < ar->nmemb; ++i) {
            out->arr[i] = cloneEstr(ar->arr[i]);
            if (out->arr[i] == NULL && ar->arr[i] != NULL) {
                cnfarrayCloneDestruct(out);
                return NULL;
            }
        }
    }
    return out;
}

static struct nvlst *cloneNvlstNode(const struct nvlst *node) {
    struct nvlst *out;
    if (node == NULL) return NULL;
    out = calloc(1, sizeof(*out));
    if (out == NULL) return NULL;
    out->name = cloneEstr(node->name);
    if (out->name == NULL && node->name != NULL) {
        nvlstDestruct(out);
        return NULL;
    }
    out->val.datatype = node->val.datatype;
    switch (node->val.datatype) {
        case 'S':
            out->val.d.estr = cloneEstr(node->val.d.estr);
            if (out->val.d.estr == NULL && node->val.d.estr != NULL) {
                nvlstDestruct(out);
                return NULL;
            }
            break;
        case 'A':
            out->val.d.ar = cloneArray(node->val.d.ar);
            if (out->val.d.ar == NULL && node->val.d.ar != NULL) {
                nvlstDestruct(out);
                return NULL;
            }
            break;
        case 'N':
            out->val.d.n = node->val.d.n;
            break;
        default:
            break;
    }
    return out;
}

struct nvlst *rsconfTranslateCloneNvlst(const struct nvlst *lst) {
    struct nvlst *head = NULL, *n;

    while (lst != NULL) {
        struct nvlst *prev = NULL, *cur = head;
        n = cloneNvlstNode(lst);
        if (n == NULL) {
            nvlstDestruct(head);
            return NULL;
        }
        while (cur != NULL && !nvlstSortComesBefore(n, cur)) {
            prev = cur;
            cur = cur->next;
        }
        n->next = cur;
        if (prev == NULL) {
            head = n;
        } else {
            prev->next = n;
        }
        lst = lst->next;
    }
    return head;
}

static struct objlst *cloneObjlst(const struct objlst *lst) {
    struct objlst *head = NULL, *tail = NULL, *n;
    while (lst != NULL) {
        struct cnfobj *obj = calloc(1, sizeof(*obj));
        if (obj == NULL) {
            objlstDestruct(head);
            return NULL;
        }
        obj->objType = lst->obj->objType;
        obj->nvlst = rsconfTranslateCloneNvlst(lst->obj->nvlst);
        if (obj->nvlst == NULL && lst->obj->nvlst != NULL) {
            cnfobjDestruct(obj);
            objlstDestruct(head);
            return NULL;
        }
        n = objlstNew(obj);
        if (n == NULL) {
            cnfobjDestruct(obj);
            objlstDestruct(head);
            return NULL;
        }
        if (head == NULL) {
            head = tail = n;
        } else {
            tail->next = n;
            tail = n;
        }
        lst = lst->next;
    }
    return head;
}

static int stmtListIsSelectorCompatible(const struct cnfstmt *stmt) {
    while (stmt != NULL) {
        switch (stmt->nodetype) {
            case S_ACT:
            case S_STOP:
            case S_CALL:
            case S_CALL_INDIRECT:
            case S_NOP:
                break;
            default:
                return 0;
        }
        stmt = stmt->next;
    }
    return 1;
}

static int estrAppendCstr(es_str_t **s, const char *buf) {
    return es_addBuf(s, buf, strlen(buf));
}

static int estrAppendChar(es_str_t **s, char c) {
    return es_addChar(s, (unsigned char)c);
}

static int estrAppendQuoted(es_str_t **s, const char *buf) {
    size_t i;
    if (estrAppendChar(s, '"') != 0) return -1;
    for (i = 0; buf[i] != '\0'; ++i) {
        switch (buf[i]) {
            case '"':
            case '\\':
                if (estrAppendChar(s, '\\') != 0 || estrAppendChar(s, buf[i]) != 0) return -1;
                break;
            case '\n':
                if (estrAppendCstr(s, "\\n") != 0) return -1;
                break;
            case '\r':
                if (estrAppendCstr(s, "\\r") != 0) return -1;
                break;
            case '\t':
                if (estrAppendCstr(s, "\\t") != 0) return -1;
                break;
            default:
                if (estrAppendChar(s, buf[i]) != 0) return -1;
                break;
        }
    }
    return estrAppendChar(s, '"');
}

static const char *exprOpToStr(unsigned nodetype) {
    switch (nodetype) {
        case CMP_EQ:
            return "==";
        case CMP_NE:
            return "!=";
        case CMP_LE:
            return "<=";
        case CMP_GE:
            return ">=";
        case CMP_LT:
            return "<";
        case CMP_GT:
            return ">";
        case CMP_CONTAINS:
            return "contains";
        case CMP_CONTAINSI:
            return "contains_i";
        case CMP_STARTSWITH:
            return "startswith";
        case CMP_STARTSWITHI:
            return "startswith_i";
        case CMP_ENDSWITH:
            return "endswith";
        case OR:
            return "or";
        case AND:
            return "and";
        default:
            return NULL;
    }
}

static int exprToString(es_str_t **out, const struct cnfexpr *expr, struct rsconfTranslateWarning_s **warnings);

static int exprListToString(es_str_t **out,
                            unsigned short nParams,
                            const struct cnfexpr *const *exprs,
                            struct rsconfTranslateWarning_s **warnings) {
    unsigned short i;
    for (i = 0; i < nParams; ++i) {
        if (i > 0 && estrAppendCstr(out, ", ") != 0) return -1;
        if (exprToString(out, exprs[i], warnings) != 0) return -1;
    }
    return 0;
}

static int exprToString(es_str_t **out, const struct cnfexpr *expr, struct rsconfTranslateWarning_s **warnings) {
    const char *op;
    char nbuf[64];
    int i;
    char *cstr;
    const struct cnffunc *func;
    const struct cnfarray *ar;

    if (expr == NULL) return -1;
    op = exprOpToStr(expr->nodetype);
    if (op != NULL) {
        if (estrAppendChar(out, '(') != 0) return -1;
        if (exprToString(out, expr->l, warnings) != 0) return -1;
        if (estrAppendCstr(out, " ") != 0 || estrAppendCstr(out, op) != 0 || estrAppendCstr(out, " ") != 0) return -1;
        if (exprToString(out, expr->r, warnings) != 0) return -1;
        return estrAppendChar(out, ')');
    }

    switch (expr->nodetype) {
        case NOT:
            if (estrAppendCstr(out, "not ") != 0) return -1;
            return exprToString(out, expr->r, warnings);
        case 'N': {
            int len = snprintf(nbuf, sizeof(nbuf), "%lld", ((const struct cnfnumval *)expr)->val);
            if (len < 0 || len >= (int)sizeof(nbuf)) return -1;
            return estrAppendCstr(out, nbuf);
        }
        case 'S':
            cstr = es_str2cstr(((const struct cnfstringval *)expr)->estr, NULL);
            if (cstr == NULL) return -1;
            i = estrAppendQuoted(out, cstr);
            free(cstr);
            return i;
        case 'V':
            return estrAppendCstr(out, ((const struct cnfvar *)expr)->name);
        case 'A':
            ar = (const struct cnfarray *)expr;
            if (estrAppendChar(out, '[') != 0) return -1;
            for (i = 0; i < ar->nmemb; ++i) {
                char *aval = es_str2cstr(ar->arr[i], NULL);
                if (i > 0 && estrAppendCstr(out, ", ") != 0) {
                    free(aval);
                    return -1;
                }
                if (aval == NULL || estrAppendQuoted(out, aval) != 0) {
                    free(aval);
                    return -1;
                }
                free(aval);
            }
            return estrAppendChar(out, ']');
        case 'F':
            func = (const struct cnffunc *)expr;
            cstr = es_str2cstr(func->fname, NULL);
            if (cstr == NULL) return -1;
            if (estrAppendCstr(out, cstr) != 0 || estrAppendChar(out, '(') != 0) {
                free(cstr);
                return -1;
            }
            free(cstr);
            if (exprListToString(out, func->nParams, (const struct cnfexpr *const *)func->expr, warnings) != 0 ||
                estrAppendChar(out, ')') != 0)
                return -1;
            return 0;
        case S_FUNC_EXISTS:
            if (estrAppendCstr(out, "exists(") != 0) return -1;
            if (estrAppendCstr(out, ((const struct cnffuncexists *)expr)->varname) != 0) return -1;
            return estrAppendChar(out, ')');
        case '&':
        case '+':
        case '-':
        case '*':
        case '/':
        case '%':
        case 'M':
            if (expr->nodetype == 'M') {
                if (estrAppendChar(out, '-') != 0) return -1;
                return exprToString(out, expr->r, warnings);
            }
            if (estrAppendChar(out, '(') != 0) return -1;
            if (exprToString(out, expr->l, warnings) != 0) return -1;
            if (estrAppendCstr(out, " ") != 0 || estrAppendChar(out, (char)expr->nodetype) != 0 ||
                estrAppendCstr(out, " ") != 0)
                return -1;
            if (exprToString(out, expr->r, warnings) != 0) return -1;
            return estrAppendChar(out, ')');
        default:
            addWarning(warnings, "translator: unsupported expression node %u", expr->nodetype);
            return -1;
    }
}

static int nvlstValueToRs(es_str_t **out, const struct nvlst *node) {
    char *cstr;
    int i;
    struct cnfarray *ar;
    char nbuf[64];

    switch (node->val.datatype) {
        case 'S':
            cstr = es_str2cstr(node->val.d.estr, NULL);
            if (cstr == NULL) return -1;
            i = estrAppendQuoted(out, cstr);
            free(cstr);
            return i;
        case 'A':
            ar = node->val.d.ar;
            if (estrAppendChar(out, '[') != 0) return -1;
            for (i = 0; i < ar->nmemb; ++i) {
                cstr = es_str2cstr(ar->arr[i], NULL);
                if (cstr == NULL) return -1;
                if ((i > 0 && estrAppendCstr(out, ", ") != 0) || estrAppendQuoted(out, cstr) != 0) {
                    free(cstr);
                    return -1;
                }
                free(cstr);
            }
            return estrAppendChar(out, ']');
        case 'N': {
            int len = snprintf(nbuf, sizeof(nbuf), "%lld", node->val.d.n);
            if (len < 0 || len >= (int)sizeof(nbuf)) return -1;
            return estrAppendCstr(out, nbuf);
        }
        default:
            return -1;
    }
}

static int emitActionSingleline(es_str_t **out,
                                const struct cnfstmt *stmt,
                                struct rsconfTranslateWarning_s **warnings) {
    const struct nvlst *lst;
    char *name;
    int first = 1;

    switch (stmt->nodetype) {
        case S_ACT:
            if (stmt->d.act != NULL && stmt->d.act->pSyntaxLst != NULL) {
                int rank;
                if (estrAppendCstr(out, "action(") != 0) return -1;
                for (rank = 0; rank < 4; ++rank) {
                    for (lst = stmt->d.act->pSyntaxLst; lst != NULL; lst = lst->next) {
                        if (preferredKeyRank(lst) != rank) continue;
                        name = es_str2cstr(lst->name, NULL);
                        if (name == NULL) return -1;
                        if (!first && estrAppendCstr(out, " ") != 0) {
                            free(name);
                            return -1;
                        }
                        first = 0;
                        if (estrAppendCstr(out, name) != 0 || estrAppendChar(out, '=') != 0 ||
                            nvlstValueToRs(out, lst) != 0) {
                            free(name);
                            return -1;
                        }
                        free(name);
                    }
                }
                return estrAppendChar(out, ')');
            }
            if (stmt->printable != NULL) {
                addWarning(warnings, "legacy action syntax preserved as script text");
                return estrAppendCstr(out, (char *)stmt->printable);
            }
            addWarning(warnings, "translator: action statement lost original syntax");
            return -1;
        case S_STOP:
            return estrAppendCstr(out, "stop");
        case S_CALL:
            if (estrAppendCstr(out, "call ") != 0) return -1;
            name = es_str2cstr(stmt->d.s_call.name, NULL);
            if (name == NULL) return -1;
            first = estrAppendCstr(out, name);
            free(name);
            return first;
        case S_CALL_INDIRECT:
            if (estrAppendCstr(out, "call_indirect ") != 0) return -1;
            if (exprToString(out, stmt->d.s_call_ind.expr, warnings) != 0) return -1;
            return estrAppendChar(out, ';');
        case S_NOP:
            if (stmt->printable != NULL && !strcmp((char *)stmt->printable, "continue")) {
                return estrAppendCstr(out, "continue");
            }
            addWarning(warnings, "translator: omitted internal NOP statement");
            return -1;
        default:
            return -1;
    }
}

static int stmtListToString(es_str_t **out,
                            const struct cnfstmt *stmt,
                            int indent,
                            struct rsconfTranslateWarning_s **warnings);

static int appendIndent(es_str_t **out, int indent) {
    while (indent-- > 0) {
        if (estrAppendCstr(out, "  ") != 0) return -1;
    }
    return 0;
}

static int emitSelectorBlock(es_str_t **out,
                             const struct cnfstmt *stmt,
                             int indent,
                             const char *selector,
                             struct rsconfTranslateWarning_s **warnings) {
    const struct cnfstmt *cur = stmt;
    int first = 1;
    while (cur != NULL) {
        if (appendIndent(out, indent) != 0) return -1;
        if (!first && estrAppendCstr(out, "& ") != 0) return -1;
        if (first) {
            if (estrAppendCstr(out, selector) != 0 || estrAppendCstr(out, " ") != 0) return -1;
        }
        first = 0;
        if (emitActionSingleline(out, cur, warnings) != 0 || estrAppendChar(out, '\n') != 0) return -1;
        cur = cur->next;
    }
    return 0;
}

static int stmtListToString(es_str_t **out,
                            const struct cnfstmt *stmt,
                            int indent,
                            struct rsconfTranslateWarning_s **warnings) {
    while (stmt != NULL) {
        switch (stmt->nodetype) {
            case S_ACT:
            case S_STOP:
            case S_CALL:
            case S_CALL_INDIRECT:
            case S_NOP:
                if (appendIndent(out, indent) != 0 || emitActionSingleline(out, stmt, warnings) != 0 ||
                    estrAppendChar(out, '\n') != 0)
                    return -1;
                break;
            case S_SET:
                if (appendIndent(out, indent) != 0) return -1;
                if (estrAppendCstr(out, stmt->d.s_set.force_reset ? "reset " : "set ") != 0 ||
                    estrAppendCstr(out, (char *)stmt->d.s_set.varname) != 0 || estrAppendCstr(out, " = ") != 0 ||
                    exprToString(out, stmt->d.s_set.expr, warnings) != 0 || estrAppendCstr(out, ";\n") != 0)
                    return -1;
                break;
            case S_UNSET:
                if (appendIndent(out, indent) != 0 || estrAppendCstr(out, "unset ") != 0 ||
                    estrAppendCstr(out, (char *)stmt->d.s_unset.varname) != 0 || estrAppendCstr(out, ";\n") != 0)
                    return -1;
                break;
            case S_IF:
                if (appendIndent(out, indent) != 0 || estrAppendCstr(out, "if ") != 0 ||
                    exprToString(out, stmt->d.s_if.expr, warnings) != 0 || estrAppendCstr(out, " then {\n") != 0 ||
                    stmtListToString(out, stmt->d.s_if.t_then, indent + 1, warnings) != 0)
                    return -1;
                if (appendIndent(out, indent) != 0) return -1;
                if (stmt->d.s_if.t_else != NULL) {
                    if (estrAppendCstr(out, "} else {\n") != 0 ||
                        stmtListToString(out, stmt->d.s_if.t_else, indent + 1, warnings) != 0 ||
                        appendIndent(out, indent) != 0 || estrAppendCstr(out, "}\n") != 0)
                        return -1;
                } else if (estrAppendCstr(out, "}\n") != 0) {
                    return -1;
                }
                break;
            case S_FOREACH:
                if (appendIndent(out, indent) != 0 || estrAppendCstr(out, "foreach (") != 0 ||
                    estrAppendCstr(out, stmt->d.s_foreach.iter->var) != 0 || estrAppendCstr(out, " in ") != 0 ||
                    exprToString(out, stmt->d.s_foreach.iter->collection, warnings) != 0 ||
                    estrAppendCstr(out, ") do {\n") != 0 ||
                    stmtListToString(out, stmt->d.s_foreach.body, indent + 1, warnings) != 0 ||
                    appendIndent(out, indent) != 0 || estrAppendCstr(out, "}\n") != 0)
                    return -1;
                break;
            case S_PRIFILT:
                if (stmt->d.s_prifilt.t_else == NULL && stmtListIsSelectorCompatible(stmt->d.s_prifilt.t_then)) {
                    if (emitSelectorBlock(out, stmt->d.s_prifilt.t_then, indent, (char *)stmt->printable, warnings) !=
                        0)
                        return -1;
                } else {
                    addWarning(warnings, "PRI selector normalized into if/prifilt() form");
                    if (appendIndent(out, indent) != 0 || estrAppendCstr(out, "if prifilt(") != 0 ||
                        estrAppendQuoted(out, (char *)stmt->printable) != 0 || estrAppendCstr(out, ") then {\n") != 0 ||
                        stmtListToString(out, stmt->d.s_prifilt.t_then, indent + 1, warnings) != 0 ||
                        appendIndent(out, indent) != 0)
                        return -1;
                    if (stmt->d.s_prifilt.t_else != NULL) {
                        if (estrAppendCstr(out, "} else {\n") != 0 ||
                            stmtListToString(out, stmt->d.s_prifilt.t_else, indent + 1, warnings) != 0 ||
                            appendIndent(out, indent) != 0 || estrAppendCstr(out, "}\n") != 0)
                            return -1;
                    } else if (estrAppendCstr(out, "}\n") != 0) {
                        return -1;
                    }
                }
                break;
            case S_PROPFILT:
                if (stmt->d.s_propfilt.t_else == NULL && stmtListIsSelectorCompatible(stmt->d.s_propfilt.t_then)) {
                    if (emitSelectorBlock(out, stmt->d.s_propfilt.t_then, indent, (char *)stmt->printable, warnings) !=
                        0)
                        return -1;
                } else {
                    addWarning(warnings, "property selector with else/complex body is not safely translatable");
                    return -1;
                }
                break;
            case S_RELOAD_LOOKUP_TABLE:
                if (appendIndent(out, indent) != 0 || estrAppendCstr(out, "reload_lookup_table(") != 0 ||
                    estrAppendQuoted(out, (char *)stmt->d.s_reload_lookup_table.table_name) != 0 ||
                    estrAppendCstr(out, ", stub_value=") != 0 ||
                    estrAppendQuoted(out, (char *)stmt->d.s_reload_lookup_table.stub_value) != 0 ||
                    estrAppendCstr(out, ")\n") != 0)
                    return -1;
                break;
            default:
                addWarning(warnings, "translator: unsupported statement node %u", stmt->nodetype);
                return -1;
        }
        stmt = stmt->next;
    }
    return 0;
}

static struct rsconfTranslateItem_s **selectList(enum cnfobjType t) {
    switch (t) {
        case CNFOBJ_GLOBAL:
            return &g_tx.globals;
        case CNFOBJ_MAINQ:
            return &g_tx.mainqueue;
        case CNFOBJ_MODULE:
            return &g_tx.modules;
        case CNFOBJ_INPUT:
            return &g_tx.inputs;
        case CNFOBJ_TPL:
            return &g_tx.templates;
        case CNFOBJ_RULESET:
            return &g_tx.rulesets;
        case CNFOBJ_LOOKUP_TABLE:
            return &g_tx.lookups;
        case CNFOBJ_PARSER:
            return &g_tx.parsers;
        case CNFOBJ_TIMEZONE:
            return &g_tx.timezones;
        case CNFOBJ_DYN_STATS:
            return &g_tx.dynstats;
        case CNFOBJ_PERCTILE_STATS:
            return &g_tx.perctilestats;
        case CNFOBJ_RATELIMIT:
            return &g_tx.ratelimits;
        case CNFOBJ_ACTION:
        case CNFOBJ_PROPERTY:
        case CNFOBJ_CONSTANT:
        default:
            return NULL;
    }
}

static void mergeSingleton(struct rsconfTranslateItem_s *dst, const struct nvlst *lst) {
    const struct nvlst *src;
    for (src = lst; src != NULL; src = src->next) {
        struct nvlst *prev = NULL;
        struct nvlst *cur = dst->nvlst;
        while (cur != NULL) {
            char *lhs = es_str2cstr(cur->name, NULL);
            char *rhs = es_str2cstr(src->name, NULL);
            int eq = lhs != NULL && rhs != NULL && !strcasecmp(lhs, rhs);
            free(lhs);
            free(rhs);
            if (eq) break;
            prev = cur;
            cur = cur->next;
        }
        if (cur != NULL) {
            struct nvlst *rep = cloneNvlstNode(src);
            if (rep == NULL) continue;
            rep->next = cur->next;
            if (prev == NULL)
                dst->nvlst = rep;
            else
                prev->next = rep;
            cur->next = NULL;
            nvlstDestruct(cur);
        } else {
            struct nvlst *rep = cloneNvlstNode(src);
            struct nvlst *tail;
            if (rep == NULL) continue;
            rep->next = NULL;
            if (dst->nvlst == NULL) {
                dst->nvlst = rep;
            } else {
                tail = dst->nvlst;
                while (tail->next != NULL) tail = tail->next;
                tail->next = rep;
            }
        }
    }
}

void rsconfTranslateCaptureObj(const struct cnfobj *o, const char *source, int line) {
    struct rsconfTranslateItem_s **list;
    struct rsconfTranslateItem_s *it;
    es_str_t *estr;

    if (!rsconfTranslateEnabled() || o == NULL) return;
    list = selectList(o->objType);
    if (list == NULL) return;

    if ((o->objType == CNFOBJ_GLOBAL || o->objType == CNFOBJ_MAINQ) && *list != NULL) {
        mergeSingleton(*list, o->nvlst);
        return;
    }

    it = calloc(1, sizeof(*it));
    if (it == NULL) {
        g_tx.fatal = 1;
        return;
    }
    it->objType = o->objType;
    it->nvlst = rsconfTranslateCloneNvlst(o->nvlst);
    if (it->nvlst == NULL && o->nvlst != NULL) {
        g_tx.fatal = 1;
        itemDestruct(it);
        return;
    }
    it->subobjs = cloneObjlst(o->subobjs);
    if (it->subobjs == NULL && o->subobjs != NULL) {
        g_tx.fatal = 1;
        itemDestruct(it);
        return;
    }
    it->source = source ? strdup(source) : NULL;
    if (source != NULL && it->source == NULL) {
        g_tx.fatal = 1;
        itemDestruct(it);
        return;
    }
    it->line = line;
    if (o->objType == CNFOBJ_RULESET && o->script != NULL) {
        estr = es_newStr(256);
        if (estr == NULL || stmtListToString(&estr, o->script, 1, &it->warnings) != 0) {
            addWarning(&it->warnings, "translator: failed to serialize ruleset body");
            g_tx.fatal = 1;
            if (estr != NULL) es_deleteStr(estr);
        } else {
            it->script = es_str2cstr(estr, NULL);
            es_deleteStr(estr);
        }
    }
    if (*list == NULL) {
        *list = it;
    } else {
        struct rsconfTranslateItem_s *tail = *list;
        while (tail->next != NULL) tail = tail->next;
        tail->next = it;
    }
}

void rsconfTranslateCaptureScript(const struct cnfstmt *script, const char *source, int line) {
    struct rsconfTranslateItem_s *it = g_tx.rulesets;
    es_str_t *estr;
    struct nvlst *nameNode;

    if (!rsconfTranslateEnabled() || script == NULL) return;
    while (it != NULL) {
        struct nvlst *cur;
        for (cur = it->nvlst; cur != NULL; cur = cur->next) {
            char *k = es_str2cstr(cur->name, NULL);
            int match = k != NULL && !strcasecmp(k, "name");
            free(k);
            if (match) break;
        }
        if (cur != NULL) {
            char *v = es_str2cstr(cur->val.d.estr, NULL);
            int match = v != NULL && !strcmp(v, "RSYSLOG_DefaultRuleset");
            free(v);
            if (match) break;
        }
        it = it->next;
    }
    if (it == NULL) {
        es_str_t *nm = es_newStrFromCStr("name", 4);
        es_str_t *val = es_newStrFromCStr("RSYSLOG_DefaultRuleset", strlen("RSYSLOG_DefaultRuleset"));
        nameNode = nvlstSetName(nvlstNewStr(val), nm);
        it = calloc(1, sizeof(*it));
        if (it == NULL) {
            nvlstDestruct(nameNode);
            g_tx.fatal = 1;
            return;
        }
        it->objType = CNFOBJ_RULESET;
        it->nvlst = nameNode;
        it->source = source ? strdup(source) : NULL;
        it->line = line;
        addWarning(&it->warnings, "top-level statements normalized into explicit RSYSLOG_DefaultRuleset");
        if (g_tx.rulesets == NULL) {
            g_tx.rulesets = it;
        } else {
            struct rsconfTranslateItem_s *tail = g_tx.rulesets;
            while (tail->next != NULL) tail = tail->next;
            tail->next = it;
        }
    }
    estr = es_newStr(256);
    if (estr == NULL || stmtListToString(&estr, script, 1, &it->warnings) != 0) {
        addWarning(&it->warnings, "translator: failed to serialize top-level script");
        g_tx.fatal = 1;
        if (estr != NULL) es_deleteStr(estr);
        return;
    }
    if (it->script == NULL) {
        it->script = es_str2cstr(estr, NULL);
        if (it->script == NULL) {
            addWarning(&it->warnings, "translator: out of memory when capturing script");
            g_tx.fatal = 1;
        }
    } else {
        char *more = es_str2cstr(estr, NULL);
        if (more == NULL) {
            addWarning(&it->warnings, "translator: out of memory when capturing script");
            g_tx.fatal = 1;
            es_deleteStr(estr);
            return;
        }
        size_t oldlen = strlen(it->script);
        size_t addlen = strlen(more);
        char *merged = realloc(it->script, oldlen + addlen + 1);
        if (merged == NULL) {
            addWarning(&it->warnings, "translator: out of memory when merging script parts");
            g_tx.fatal = 1;
        } else {
            memcpy(merged + oldlen, more, addlen + 1);
            it->script = merged;
        }
        free(more);
    }
    es_deleteStr(estr);
}

static void writeWarningComments(FILE *fp, const struct rsconfTranslateWarning_s *w, int indent) {
    while (w != NULL) {
        int i;
        for (i = 0; i < indent; ++i) fputs("  ", fp);
        fprintf(fp, "# TRANSLATION WARNING: %s\n", w->msg);
        w = w->next;
    }
}

static void logWarnings(const struct rsconfTranslateWarning_s *w) {
    while (w != NULL) {
        LogError(0, RS_RET_CONF_PARSE_ERROR, "%s", w->msg);
        w = w->next;
    }
}

static void logItemWarnings(const struct rsconfTranslateItem_s *items) {
    while (items != NULL) {
        logWarnings(items->warnings);
        items = items->next;
    }
}

static void logAllWarnings(void) {
    logWarnings(g_tx.globalWarnings);
    logItemWarnings(g_tx.globals);
    logItemWarnings(g_tx.mainqueue);
    logItemWarnings(g_tx.modules);
    logItemWarnings(g_tx.inputs);
    logItemWarnings(g_tx.templates);
    logItemWarnings(g_tx.rulesets);
    logItemWarnings(g_tx.lookups);
    logItemWarnings(g_tx.parsers);
    logItemWarnings(g_tx.timezones);
    logItemWarnings(g_tx.dynstats);
    logItemWarnings(g_tx.perctilestats);
    logItemWarnings(g_tx.ratelimits);
}

static void writeYamlQuoted(FILE *fp, const char *s) {
    fputc('"', fp);
    while (*s != '\0') {
        switch (*s) {
            case '"':
                fputs("\\\"", fp);
                break;
            case '\\':
                fputs("\\\\", fp);
                break;
            case '\n':
                fputs("\\n", fp);
                break;
            case '\r':
                fputs("\\r", fp);
                break;
            case '\t':
                fputs("\\t", fp);
                break;
            default:
                fputc(*s, fp);
                break;
        }
        ++s;
    }
    fputc('"', fp);
}

static void writeYamlValue(FILE *fp, const struct nvlst *n) {
    int i;
    char *s;
    switch (n->val.datatype) {
        case 'N':
            fprintf(fp, "%lld", n->val.d.n);
            break;
        case 'A':
            fputc('[', fp);
            for (i = 0; i < n->val.d.ar->nmemb; ++i) {
                s = es_str2cstr(n->val.d.ar->arr[i], NULL);
                if (i > 0) fputs(", ", fp);
                writeYamlQuoted(fp, s == NULL ? "" : s);
                free(s);
            }
            fputc(']', fp);
            break;
        case 'S':
        default:
            s = es_str2cstr(n->val.d.estr, NULL);
            writeYamlQuoted(fp, s == NULL ? "" : s);
            free(s);
            break;
    }
}

static int preferredKeyRank(const struct nvlst *n) {
    char *name;
    int rank = 3;
    if (n == NULL) return rank;
    name = es_str2cstr(n->name, NULL);
    if (name != NULL) {
        if (!strcmp(name, "name")) {
            rank = 0;
        } else if (!strcmp(name, "type")) {
            rank = 1;
        } else if (!strcmp(name, "load")) {
            rank = 2;
        }
    }
    free(name);
    return rank;
}

static int nvlstSortComesBefore(const struct nvlst *a, const struct nvlst *b) {
    char *nameA;
    char *nameB;
    int rankA;
    int rankB;
    int cmp;

    if (b == NULL) return 1;
    if (a == NULL) return 0;

    rankA = preferredKeyRank(a);
    rankB = preferredKeyRank(b);
    if (rankA != rankB) return rankA < rankB;

    nameA = es_str2cstr(a->name, NULL);
    nameB = es_str2cstr(b->name, NULL);
    if (nameA == NULL || nameB == NULL) {
        free(nameA);
        free(nameB);
        return 0;
    }
    cmp = strcmp(nameA, nameB);
    free(nameA);
    free(nameB);
    return cmp < 0;
}

static void writeYamlEntry(FILE *fp, const struct nvlst *n, int indent, int list_prefix) {
    char *name;
    int i;
    for (i = 0; i < indent; ++i) fputs("  ", fp);
    if (list_prefix) fputs("- ", fp);
    name = es_str2cstr(n->name, NULL);
    fprintf(fp, "%s: ", name == NULL ? "" : name);
    free(name);
    writeYamlValue(fp, n);
    fputc('\n', fp);
}

static void writeYamlMappingExcept(FILE *fp, const struct nvlst *lst, int indent, const struct nvlst *skip) {
    const struct nvlst *n;
    int rank;
    for (rank = 0; rank < 4; ++rank) {
        for (n = lst; n != NULL; n = n->next) {
            if (n == skip || preferredKeyRank(n) != rank) continue;
            writeYamlEntry(fp, n, indent, 0);
        }
    }
}

static void writeYamlMapping(FILE *fp, const struct nvlst *lst, int indent) {
    writeYamlMappingExcept(fp, lst, indent, NULL);
}

static void writeYamlBlockScalar(FILE *fp, const char *content, int indent) {
    const char *cur = content;
    int i;

    while (cur != NULL && *cur != '\0') {
        for (i = 0; i < indent; ++i) fputs("  ", fp);
        while (*cur != '\0' && *cur != '\n') {
            fputc(*cur++, fp);
        }
        fputc('\n', fp);
        if (*cur == '\n') ++cur;
    }
}

static void writeYamlListSection(FILE *fp, const char *name, const struct rsconfTranslateItem_s *items) {
    const struct rsconfTranslateItem_s *it;
    if (items == NULL) return;
    fprintf(fp, "%s:\n", name);
    for (it = items; it != NULL; it = it->next) {
        const struct nvlst *n;
        const struct nvlst *firstNode = NULL;
        int firstRank = 4;
        writeWarningComments(fp, it->warnings, 1);
        for (n = it->nvlst; n != NULL; n = n->next) {
            int rank = preferredKeyRank(n);
            if (rank < firstRank) {
                firstNode = n;
                firstRank = rank;
            }
        }
        if (it->nvlst == NULL && it->script == NULL) {
            fputs("  -\n", fp);
            continue;
        }
        if (firstNode != NULL) {
            writeYamlEntry(fp, firstNode, 1, 1);
        } else {
            fputs("  -\n", fp);
        }
        writeYamlMappingExcept(fp, it->nvlst, 2, firstNode);
        if (it->subobjs != NULL) {
            fputs("    elements:\n", fp);
            for (const struct objlst *obj = it->subobjs; obj != NULL; obj = obj->next) {
                fprintf(fp, "      - %s:\n", obj->obj->objType == CNFOBJ_PROPERTY ? "property" : "constant");
                writeYamlMapping(fp, obj->obj->nvlst, 4);
            }
        }
        if (it->script != NULL) {
            fputs("    script: |\n", fp);
            writeYamlBlockScalar(fp, it->script, 3);
        }
    }
}

static void writeRsParams(FILE *fp, const struct nvlst *lst) {
    const struct nvlst *n;
    char *name;
    int first = 1, i, rank;
    for (rank = 0; rank < 4; ++rank) {
        for (n = lst; n != NULL; n = n->next) {
            if (preferredKeyRank(n) != rank) continue;
            if (!first) fputc(' ', fp);
            first = 0;
            name = es_str2cstr(n->name, NULL);
            fprintf(fp, "%s=", name == NULL ? "" : name);
            free(name);
            switch (n->val.datatype) {
                case 'N':
                    fprintf(fp, "%lld", n->val.d.n);
                    break;
                case 'A':
                    fputc('[', fp);
                    for (i = 0; i < n->val.d.ar->nmemb; ++i) {
                        char *aval = es_str2cstr(n->val.d.ar->arr[i], NULL);
                        if (i > 0) fputs(", ", fp);
                        writeYamlQuoted(fp, aval == NULL ? "" : aval);
                        free(aval);
                    }
                    fputc(']', fp);
                    break;
                case 'S':
                default: {
                    char *val = es_str2cstr(n->val.d.estr, NULL);
                    writeYamlQuoted(fp, val == NULL ? "" : val);
                    free(val);
                    break;
                }
            }
        }
    }
}

static void writeRsTemplate(FILE *fp, const struct rsconfTranslateItem_s *it) {
    fprintf(fp, "template(");
    writeRsParams(fp, it->nvlst);
    if (it->subobjs == NULL) {
        fprintf(fp, ")\n\n");
        return;
    }
    fprintf(fp, ") {\n");
    for (const struct objlst *obj = it->subobjs; obj != NULL; obj = obj->next) {
        fprintf(fp, "  %s(", obj->obj->objType == CNFOBJ_PROPERTY ? "property" : "constant");
        writeRsParams(fp, obj->obj->nvlst);
        fprintf(fp, ")\n");
    }
    fprintf(fp, "}\n\n");
}

static void writeRsList(FILE *fp, const char *keyword, const struct rsconfTranslateItem_s *items) {
    const struct rsconfTranslateItem_s *it;
    for (it = items; it != NULL; it = it->next) {
        writeWarningComments(fp, it->warnings, 0);
        if (it->objType == CNFOBJ_TPL) {
            writeRsTemplate(fp, it);
            continue;
        }
        if (it->objType == CNFOBJ_RULESET) {
            fprintf(fp, "ruleset(");
            writeRsParams(fp, it->nvlst);
            if (it->script != NULL) {
                fprintf(fp, ") {\n%s}\n\n", it->script);
            } else {
                fprintf(fp, ") {}\n\n");
            }
            continue;
        }
        fprintf(fp, "%s(", keyword);
        writeRsParams(fp, it->nvlst);
        fprintf(fp, ")\n\n");
    }
}

rsRetVal rsconfTranslateWriteFile(const char *path) {
    FILE *fp = NULL;
    DEFiRet;

    if (!rsconfTranslateEnabled()) {
        FINALIZE;
    }
    if (g_tx.fatal) {
        logAllWarnings();
        ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
    }

    if (path == NULL || !strcmp(path, "-")) {
        fp = stdout;
    } else {
        fp = fopen(path, "w");
    }
    if (fp == NULL) {
        ABORT_FINALIZE(RS_RET_FILE_NOT_FOUND);
    }

    if (g_tx.fmt == RSCONF_TRANSLATE_YAML) {
        fputs("version: 2\n\n", fp);
        writeWarningComments(fp, g_tx.globalWarnings, 0);
        if (g_tx.globals != NULL) {
            writeWarningComments(fp, g_tx.globals->warnings, 0);
            fputs("global:\n", fp);
            writeYamlMapping(fp, g_tx.globals->nvlst, 1);
            fputc('\n', fp);
        }
        if (g_tx.mainqueue != NULL) {
            writeWarningComments(fp, g_tx.mainqueue->warnings, 0);
            fputs("mainqueue:\n", fp);
            writeYamlMapping(fp, g_tx.mainqueue->nvlst, 1);
            fputc('\n', fp);
        }
        writeYamlListSection(fp, "modules", g_tx.modules);
        writeYamlListSection(fp, "inputs", g_tx.inputs);
        writeYamlListSection(fp, "templates", g_tx.templates);
        writeYamlListSection(fp, "rulesets", g_tx.rulesets);
        writeYamlListSection(fp, "lookup_tables", g_tx.lookups);
        writeYamlListSection(fp, "parsers", g_tx.parsers);
        writeYamlListSection(fp, "timezones", g_tx.timezones);
        writeYamlListSection(fp, "dyn_stats", g_tx.dynstats);
        writeYamlListSection(fp, "perctile_stats", g_tx.perctilestats);
        writeYamlListSection(fp, "ratelimits", g_tx.ratelimits);
    } else {
        writeWarningComments(fp, g_tx.globalWarnings, 0);
        writeRsList(fp, "global", g_tx.globals);
        writeRsList(fp, "main_queue", g_tx.mainqueue);
        writeRsList(fp, "module", g_tx.modules);
        writeRsList(fp, "input", g_tx.inputs);
        writeRsList(fp, "template", g_tx.templates);
        writeRsList(fp, "lookup_table", g_tx.lookups);
        writeRsList(fp, "parser", g_tx.parsers);
        writeRsList(fp, "timezone", g_tx.timezones);
        writeRsList(fp, "dyn_stats", g_tx.dynstats);
        writeRsList(fp, "perctile_stats", g_tx.perctilestats);
        writeRsList(fp, "ratelimit", g_tx.ratelimits);
        writeRsList(fp, "ruleset", g_tx.rulesets);
    }

finalize_it:
    if (fp != NULL && fp != stdout) fclose(fp);
    RETiRet;
}
