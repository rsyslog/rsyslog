/* yamlconf.c - YAML configuration file loader for rsyslog
 *
 * Parses a .yaml/.yml rsyslog config file and drives the same processing
 * pipeline that the RainerScript lex/bison parser uses:
 *   - builds struct cnfobj + struct nvlst for each declarative block
 *   - calls cnfDoObj() to let rsconf.c dispatch to the right processor
 *   - feeds ruleset script: strings back into the existing parser via
 *     cnfAddConfigBuffer() so RainerScript logic is fully supported
 *
 * The YAML schema mirrors the RainerScript object model:
 *
 *   global:          { key: value, ... }
 *   modules:         [ { load: name, param: val, ... }, ... ]
 *   inputs:          [ { type: name, param: val, ... }, ... ]
 *   templates:       [ { name: n, type: t, string: s, ... }, ... ]
 *   rulesets:        [ { name: n, script: |rainerscript..., ... }, ... ]
 *   mainqueue:       { type: linkedlist, size: 100000, ... }
 *   include:         [ { path: glob, optional: on }, ... ]
 *   parser:          [ { name: n, type: t, ... }, ... ]
 *   lookup_table:    [ { name: n, file: f, ... }, ... ]
 *   dyn_stats:       [ { name: n, ... }, ... ]
 *   timezone:        [ { id: z, offset: o }, ... ]
 *   ratelimit:       [ { name: n, interval: i, burst: b }, ... ]
 *
 * Parameter names within each block are identical to their RainerScript
 * counterparts; all type conversion and validation happens downstream in
 * nvlstGetParams() as usual.
 *
 * Concurrency & Locking: this code runs only during config load, which is
 * single-threaded.  No synchronisation primitives are needed.
 *
 * Copyright 2025 Rainer Gerhards and Adiscon GmbH.
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

#ifdef HAVE_LIBYAML

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <yaml.h>
#include <libestr.h>

#include "rsyslog.h"
#include "errmsg.h"
#include "yamlconf.h"
#include "grammar/rainerscript.h"
#include "grammar/parserif.h"

/* Maximum nesting depth we track (YAML spec allows arbitrary depth, but
 * rsyslog config objects are shallow; 16 is more than enough). */
#define YAMLCONF_MAX_DEPTH 16

/* --------------------------------------------------------------------------
 * Forward declarations
 * -------------------------------------------------------------------------- */
static rsRetVal process_top_level(yaml_parser_t *parser, const char *key,
                                  const char *fname);
static rsRetVal parse_singleton_obj(yaml_parser_t *parser,
                                    enum cnfobjType objType, const char *fname);
static rsRetVal parse_obj_sequence(yaml_parser_t *parser,
                                   enum cnfobjType objType, const char *fname);
static rsRetVal parse_ruleset_sequence(yaml_parser_t *parser,
                                       const char *fname);
static rsRetVal parse_actions_sequence(yaml_parser_t *parser,
                                       struct cnfstmt **head_out,
                                       const char *fname);
static rsRetVal parse_include_sequence(yaml_parser_t *parser,
                                       const char *fname);
static rsRetVal build_nvlst_from_mapping(yaml_parser_t *parser,
                                         struct nvlst **out, const char *fname);
static rsRetVal build_array_from_sequence(yaml_parser_t *parser,
                                          struct cnfarray **out,
                                          const char *fname);
static rsRetVal expect_event(yaml_parser_t *parser, yaml_event_t *ev,
                             yaml_event_type_t expected, const char *ctx,
                             const char *fname);
static void skip_node(yaml_parser_t *parser);

/* --------------------------------------------------------------------------
 * Helpers
 * -------------------------------------------------------------------------- */

/* Convert a YAML scalar value (C string) into a heap-allocated es_str_t.
 * The caller owns the returned pointer. */
static es_str_t *
scalar_to_estr(const char *val)
{
    return es_newStrFromCStr(val, strlen(val));
}

/* Append a C string to an es_str_t, growing as needed.
 * Returns 0 on success, -1 on OOM. */
static int
estr_append_cstr(es_str_t **s, const char *buf, size_t len)
{
    return es_addBuf(s, buf, (es_size_t)len);
}

/* Append a single character to an es_str_t. */
static int
estr_append_char(es_str_t **s, char c)
{
    return es_addChar(s, (unsigned char)c);
}

/* Append a string value enclosed in double-quotes, escaping backslash and
 * double-quote characters with a preceding backslash.  This produces valid
 * RainerScript string literals. */
static int
estr_append_quoted(es_str_t **s, const char *val, size_t len)
{
    if (estr_append_char(s, '"') != 0) return -1;
    for (size_t i = 0; i < len; i++) {
        char c = val[i];
        if (c == '"' || c == '\\') {
            if (estr_append_char(s, '\\') != 0) return -1;
        }
        if (estr_append_char(s, c) != 0) return -1;
    }
    return estr_append_char(s, '"');
}

/* Synthesise a complete RainerScript ruleset block:
 *
 *   ruleset(name="<n>" [key="val" ...]) {
 *   <script_str>
 *   }
 *
 * All entries from 'lst' (the nvlst collected from the YAML mapping) are
 * serialised as key="value" pairs inside the parentheses.  String values are
 * double-quoted and escaped.  Array values use ["v1","v2"] syntax.
 *
 * The resulting es_str_t has two trailing NUL bytes appended so it is safe
 * to pass directly to cnfAddConfigBuffer() / yy_scan_buffer().
 *
 * Returns RS_RET_OK on success, RS_RET_OUT_OF_MEMORY on allocation failure. */
static rsRetVal
build_ruleset_rainerscript(struct nvlst *lst, const char *script_str,
                            es_str_t **out)
{
    es_str_t *s = es_newStr(256);
    DEFiRet;

    if (s == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

    if (estr_append_cstr(&s, "ruleset(", 8) != 0) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

    for (struct nvlst *n = lst; n != NULL; n = n->next) {
        /* key name */
        size_t klen = es_strlen(n->name);
        if (estr_append_cstr(&s, (char *)es_getBufAddr(n->name), klen) != 0)
            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        if (estr_append_char(&s, '=') != 0) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

        if (n->val.datatype == 'S') {
            /* String value → "escaped" */
            size_t vlen = es_strlen(n->val.d.estr);
            if (estr_append_quoted(&s, (char *)es_getBufAddr(n->val.d.estr), vlen) != 0)
                ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        } else if (n->val.datatype == 'A') {
            /* Array value → ["v1","v2",...] */
            struct cnfarray *ar = n->val.d.ar;
            if (estr_append_char(&s, '[') != 0) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
            for (int i = 0; i < ar->nmemb; i++) {
                if (i > 0 && estr_append_char(&s, ',') != 0)
                    ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
                size_t elen = es_strlen(ar->arr[i]);
                if (estr_append_quoted(&s, (char *)es_getBufAddr(ar->arr[i]), elen) != 0)
                    ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
            }
            if (estr_append_char(&s, ']') != 0) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        } else {
            /* Unexpected type; skip with a warning (should not happen for rulesets) */
            LogError(0, RS_RET_CONF_PARSE_ERROR,
                     "yamlconf: unexpected nvlst value datatype '%c' in ruleset "
                     "parameter serialisation, skipping", n->val.datatype);
            if (estr_append_cstr(&s, "\"\"", 2) != 0)
                ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        }
        if (estr_append_char(&s, ' ') != 0) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    /* Close parameter list, open script body */
    if (estr_append_cstr(&s, ") {\n", 4) != 0) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

    /* Script body */
    size_t slen = strlen(script_str);
    if (estr_append_cstr(&s, script_str, slen) != 0) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

    /* Ensure the script ends with a newline before the closing brace */
    if (slen == 0 || script_str[slen - 1] != '\n') {
        if (estr_append_char(&s, '\n') != 0) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    if (estr_append_cstr(&s, "}\n", 2) != 0) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

    /* Two trailing NUL bytes required by yy_scan_buffer() */
    if (estr_append_char(&s, '\0') != 0) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    if (estr_append_char(&s, '\0') != 0) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

    *out = s;
    s = NULL;    /* caller owns it */
    iRet = RS_RET_OK;

finalize_it:
    if (s != NULL) es_deleteStr(s);
    RETiRet;
}

/* Emit a parse-error message that includes the YAML source location. */
static void
yaml_errmsg(yaml_parser_t *parser, const char *fname, const char *extra)
{
    if (parser->problem != NULL) {
        LogError(0, RS_RET_CONF_PARSE_ERROR,
                 "yamlconf: %s: %s (line %lu col %lu)%s%s",
                 fname, parser->problem,
                 (unsigned long)parser->problem_mark.line + 1,
                 (unsigned long)parser->problem_mark.column + 1,
                 extra ? " " : "", extra ? extra : "");
    } else {
        LogError(0, RS_RET_CONF_PARSE_ERROR,
                 "yamlconf: %s: parse error%s%s",
                 fname, extra ? " " : "", extra ? extra : "");
    }
}

/* Pull the next event from the parser; on error log and return RS_RET_CONF_PARSE_ERROR. */
static rsRetVal
next_event(yaml_parser_t *parser, yaml_event_t *ev, const char *fname)
{
    DEFiRet;
    if (!yaml_parser_parse(parser, ev)) {
        yaml_errmsg(parser, fname, NULL);
        ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
    }
finalize_it:
    RETiRet;
}

/* Pull the next event and assert it has the expected type. */
static rsRetVal
expect_event(yaml_parser_t *parser, yaml_event_t *ev,
             yaml_event_type_t expected, const char *ctx, const char *fname)
{
    DEFiRet;
    CHKiRet(next_event(parser, ev, fname));
    if (ev->type != expected) {
        LogError(0, RS_RET_CONF_PARSE_ERROR,
                 "yamlconf: %s: expected event %d got %d in %s",
                 fname, (int)expected, (int)ev->type, ctx);
        yaml_event_delete(ev);
        ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
    }
finalize_it:
    RETiRet;
}

/* Skip over the current node (scalar, sequence, or mapping) and all its
 * children.  Assumes the opening event for this node has already been consumed
 * (i.e. the caller has just read MAPPING_START or SEQUENCE_START; for a
 * scalar the caller passes the scalar event and we do nothing). */
static void
skip_node(yaml_parser_t *parser)
{
    yaml_event_t ev;
    int depth = 1;
    while (depth > 0) {
        if (!yaml_parser_parse(parser, &ev)) break;
        PRAGMA_DIAGNOSTIC_PUSH
        PRAGMA_IGNORE_Wswitch_enum
        switch (ev.type) {
            case YAML_MAPPING_START_EVENT:
            case YAML_SEQUENCE_START_EVENT:
                depth++;
                break;
            case YAML_MAPPING_END_EVENT:
            case YAML_SEQUENCE_END_EVENT:
                depth--;
                break;
            default:
                break;
        }
        PRAGMA_DIAGNOSTIC_POP
        yaml_event_delete(&ev);
    }
}

/* --------------------------------------------------------------------------
 * nvlst / cnfarray builders
 * -------------------------------------------------------------------------- */

/* Build a struct cnfarray* from the remaining items of a YAML sequence.
 * The SEQUENCE_START event must already have been consumed by the caller.
 * Consumes up to and including the matching SEQUENCE_END event. */
static rsRetVal
build_array_from_sequence(yaml_parser_t *parser,
                           struct cnfarray **out, const char *fname)
{
    yaml_event_t ev;
    struct cnfarray *arr = NULL;
    es_str_t *estr;
    DEFiRet;

    *out = NULL;
    while (1) {
        CHKiRet(next_event(parser, &ev, fname));
        if (ev.type == YAML_SEQUENCE_END_EVENT) {
            yaml_event_delete(&ev);
            break;
        }
        if (ev.type != YAML_SCALAR_EVENT) {
            LogError(0, RS_RET_CONF_PARSE_ERROR,
                     "yamlconf: %s: array elements must be scalars", fname);
            yaml_event_delete(&ev);
            ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
        }
        estr = scalar_to_estr((char *)ev.data.scalar.value);
        yaml_event_delete(&ev);
        if (estr == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

        if (arr == NULL)
            arr = cnfarrayNew(estr);
        else
            arr = cnfarrayAdd(arr, estr);

        if (arr == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }
    *out = arr;

finalize_it:
    if (iRet != RS_RET_OK && arr != NULL) {
        cnfarrayContentDestruct(arr);
        free(arr);
    }
    RETiRet;
}

/* Build a struct nvlst* from a YAML mapping.
 * The MAPPING_START event must already have been consumed by the caller.
 * Consumes up to and including the matching MAPPING_END event.
 *
 * Each key/value pair becomes one nvlst node:
 *   scalar value  → nvlstNewStr(estr)
 *   sequence      → nvlstNewArray(cnfarray)
 *   nested mapping → logged as unsupported and skipped (rsyslog params are flat)
 */
static rsRetVal
build_nvlst_from_mapping(yaml_parser_t *parser,
                          struct nvlst **out, const char *fname)
{
    yaml_event_t ev;
    struct nvlst *lst = NULL;
    struct nvlst *node;
    char *key = NULL;
    es_str_t *keyestr;
    DEFiRet;

    *out = NULL;

    while (1) {
        /* Expect: key scalar or mapping end */
        CHKiRet(next_event(parser, &ev, fname));

        if (ev.type == YAML_MAPPING_END_EVENT) {
            yaml_event_delete(&ev);
            break;
        }

        if (ev.type != YAML_SCALAR_EVENT) {
            LogError(0, RS_RET_CONF_PARSE_ERROR,
                     "yamlconf: %s: expected scalar key in mapping", fname);
            yaml_event_delete(&ev);
            ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
        }

        free(key);
        key = strdup((char *)ev.data.scalar.value);
        yaml_event_delete(&ev);
        if (key == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

        /* Now read the value */
        CHKiRet(next_event(parser, &ev, fname));

        keyestr = scalar_to_estr(key);
        if (keyestr == NULL) {
            yaml_event_delete(&ev);
            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        }

        if (ev.type == YAML_SCALAR_EVENT) {
            es_str_t *val = scalar_to_estr((char *)ev.data.scalar.value);
            yaml_event_delete(&ev);
            if (val == NULL) { es_deleteStr(keyestr); ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY); }
            node = nvlstNewStr(val);
            if (node == NULL) { es_deleteStr(keyestr); es_deleteStr(val); ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY); }
        } else if (ev.type == YAML_SEQUENCE_START_EVENT) {
            yaml_event_delete(&ev);
            struct cnfarray *arr = NULL;
            rsRetVal arrRet = build_array_from_sequence(parser, &arr, fname);
            if (arrRet != RS_RET_OK) { es_deleteStr(keyestr); ABORT_FINALIZE(arrRet); }
            node = nvlstNewArray(arr);
            if (node == NULL) { es_deleteStr(keyestr); ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY); }
        } else if (ev.type == YAML_MAPPING_START_EVENT) {
            /* Nested mappings are not part of the rsyslog parameter model.
             * Skip and warn so the user knows the key was ignored. */
            yaml_event_delete(&ev);
            es_deleteStr(keyestr);
            LogError(0, RS_RET_CONF_PARSE_ERROR,
                     "yamlconf: %s: nested mapping for key '%s' is not supported"
                     " – ignoring", fname, key);
            skip_node(parser);
            continue;
        } else {
            yaml_event_delete(&ev);
            es_deleteStr(keyestr);
            LogError(0, RS_RET_CONF_PARSE_ERROR,
                     "yamlconf: %s: unexpected YAML event for value of key '%s'",
                     fname, key);
            ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
        }

        nvlstSetName(node, keyestr);
        /* Prepend; cnfDoObj() does not depend on order */
        node->next = lst;
        lst = node;
    }

    *out = lst;

finalize_it:
    free(key);
    if (iRet != RS_RET_OK && lst != NULL) {
        nvlstDestruct(lst);
    }
    RETiRet;
}

/* --------------------------------------------------------------------------
 * Object section parsers
 * -------------------------------------------------------------------------- */

/* Parse a singleton mapping section (e.g. global:, mainqueue:) and emit one
 * cnfobj of the given type.
 * The MAPPING_START event for the section body must already be consumed. */
static rsRetVal
parse_singleton_obj(yaml_parser_t *parser,
                    enum cnfobjType objType, const char *fname)
{
    struct nvlst *lst = NULL;
    struct cnfobj *obj = NULL;
    DEFiRet;

    CHKiRet(build_nvlst_from_mapping(parser, &lst, fname));
    obj = cnfobjNew(objType, lst);
    if (obj == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    lst = NULL; /* owned by obj now */
    cnfDoObj(obj);

finalize_it:
    if (lst != NULL) nvlstDestruct(lst);
    RETiRet;
}

/* Parse a sequence of mapping items, each becoming one cnfobj.
 * The SEQUENCE_START event for the list must already be consumed.
 *
 * Used for: modules, inputs, templates, parser, lookup_table, dyn_stats,
 *           ratelimit, timezone. */
static rsRetVal
parse_obj_sequence(yaml_parser_t *parser,
                   enum cnfobjType objType, const char *fname)
{
    yaml_event_t ev;
    DEFiRet;

    while (1) {
        CHKiRet(next_event(parser, &ev, fname));

        if (ev.type == YAML_SEQUENCE_END_EVENT) {
            yaml_event_delete(&ev);
            break;
        }
        if (ev.type != YAML_MAPPING_START_EVENT) {
            LogError(0, RS_RET_CONF_PARSE_ERROR,
                     "yamlconf: %s: sequence item must be a mapping", fname);
            yaml_event_delete(&ev);
            ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
        }
        yaml_event_delete(&ev);

        struct nvlst *lst = NULL;
        CHKiRet(build_nvlst_from_mapping(parser, &lst, fname));

        struct cnfobj *obj = cnfobjNew(objType, lst);
        if (obj == NULL) {
            nvlstDestruct(lst);
            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        }
        cnfDoObj(obj);
    }

finalize_it:
    RETiRet;
}

/* Parse an actions: sequence inside a ruleset mapping.
 *
 * Each item is a YAML mapping that mirrors an action() object: the "type"
 * key names the output module and all other keys are passed as parameters.
 * We build an nvlst for each item and call cnfstmtNewAct() directly, then
 * chain the resulting cnfstmt nodes via ->next.
 *
 * The SEQUENCE_START event must already have been consumed by the caller.
 * On success *head_out points to the first node of the chain (possibly NULL
 * if the sequence was empty).  Ownership passes to the caller. */
static rsRetVal
parse_actions_sequence(yaml_parser_t *parser, struct cnfstmt **head_out,
                       const char *fname)
{
    yaml_event_t ev;
    struct cnfstmt *head = NULL, *tail = NULL;
    DEFiRet;

    *head_out = NULL;

    while (1) {
        CHKiRet(next_event(parser, &ev, fname));
        if (ev.type == YAML_SEQUENCE_END_EVENT) {
            yaml_event_delete(&ev);
            break;
        }
        if (ev.type != YAML_MAPPING_START_EVENT) {
            LogError(0, RS_RET_CONF_PARSE_ERROR,
                     "yamlconf: %s: actions: item must be a mapping", fname);
            yaml_event_delete(&ev);
            ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
        }
        yaml_event_delete(&ev);

        struct nvlst *alst = NULL;
        CHKiRet(build_nvlst_from_mapping(parser, &alst, fname));

        /* cnfstmtNewAct() takes ownership of alst and frees it internally. */
        struct cnfstmt *act = cnfstmtNewAct(alst);
        if (act == NULL) {
            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        }

        if (head == NULL) {
            head = tail = act;
        } else {
            tail->next = act;
            tail = act;
        }
    }

    *head_out = head;
    head = NULL;

finalize_it:
    if (head != NULL) cnfstmtDestructLst(head);
    RETiRet;
}

/* Parse the rulesets: sequence.
 *
 * Each item is a mapping with at least a "name" key.  The special "script"
 * key causes a complete  ruleset(params) { script }  RainerScript block to
 * be synthesised and pushed onto the flex buffer stack via
 * cnfAddConfigBuffer(); the ongoing yyparse() then processes it naturally.
 *
 * ORDERING NOTE: The synthesised ruleset buffer is pushed AFTER any .conf
 * files that were included via the include: section (which are also pushed
 * via cnfAddConfigBuffer / cnfSetLexFile).  Because the flex buffer stack is
 * LIFO, the ruleset script is processed BEFORE those included .conf files.
 * Consequently, any templates or objects that a ruleset script references
 * must be defined in the YAML file's own templates:/global:/etc. sections
 * (processed immediately via cnfDoObj()), not in included .conf fragments.
 *
 * Structured filter shortcut (Phase 2):
 *   filter:  "<filter-string>"   # optional; "*.info" (PRI) or ":prop,op,val" (property)
 *   actions:                     # mutually exclusive with script:
 *     - type: omfile
 *       file: /var/log/messages
 * When filter: + actions: are used, cnfstmt nodes are built directly via
 * cnfstmtNewPRIFILT / cnfstmtNewPROPFILT / cnfstmtNewAct and set on the
 * cnfobj->script field before calling cnfDoObj() — no text buffer needed. */
static rsRetVal
parse_ruleset_sequence(yaml_parser_t *parser, const char *fname)
{
    yaml_event_t ev;
    DEFiRet;

    while (1) {
        CHKiRet(next_event(parser, &ev, fname));
        if (ev.type == YAML_SEQUENCE_END_EVENT) {
            yaml_event_delete(&ev);
            break;
        }
        if (ev.type != YAML_MAPPING_START_EVENT) {
            LogError(0, RS_RET_CONF_PARSE_ERROR,
                     "yamlconf: %s: rulesets sequence item must be a mapping",
                     fname);
            yaml_event_delete(&ev);
            ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
        }
        yaml_event_delete(&ev);

        /* Collect all keys into nvlst, pull out "script"/"filter"/"actions" specially */
        struct nvlst *lst = NULL;
        char *script_str = NULL;
        char *filter_str = NULL;       /* Phase 2: filter: "*.info" or ":prop,op,val" */
        struct cnfstmt *actions = NULL; /* Phase 2: actions: [ {type:..., ...}, ... ] */

        /* We parse the mapping manually here so we can intercept special keys */
        while (1) {
            CHKiRet(next_event(parser, &ev, fname));
            if (ev.type == YAML_MAPPING_END_EVENT) {
                yaml_event_delete(&ev);
                break;
            }
            if (ev.type != YAML_SCALAR_EVENT) {
                LogError(0, RS_RET_CONF_PARSE_ERROR,
                         "yamlconf: %s: expected scalar key in ruleset mapping",
                         fname);
                yaml_event_delete(&ev);
                free(script_str); free(filter_str);
                if (actions) cnfstmtDestructLst(actions);
                nvlstDestruct(lst);
                ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
            }

            char *kname = strdup((char *)ev.data.scalar.value);
            yaml_event_delete(&ev);
            if (kname == NULL) {
                free(script_str); free(filter_str);
                if (actions) cnfstmtDestructLst(actions);
                nvlstDestruct(lst);
                ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
            }

            CHKiRet(next_event(parser, &ev, fname));

            if (!strcmp(kname, "script")) {
                /* script: must be a scalar (RainerScript text) */
                if (ev.type != YAML_SCALAR_EVENT) {
                    LogError(0, RS_RET_CONF_PARSE_ERROR,
                             "yamlconf: %s: 'script' value must be a scalar"
                             " (use a YAML block scalar '|' for multi-line)",
                             fname);
                    yaml_event_delete(&ev);
                    free(kname); free(script_str); free(filter_str);
                    if (actions) cnfstmtDestructLst(actions);
                    nvlstDestruct(lst);
                    ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
                }
                free(script_str);
                script_str = strdup((char *)ev.data.scalar.value);
                yaml_event_delete(&ev);
                free(kname);
                if (script_str == NULL) {
                    free(filter_str);
                    if (actions) cnfstmtDestructLst(actions);
                    nvlstDestruct(lst);
                    ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
                }
            } else if (!strcmp(kname, "filter")) {
                /* filter: "<pri-filter>" or ":<property-filter>" */
                if (ev.type != YAML_SCALAR_EVENT) {
                    LogError(0, RS_RET_CONF_PARSE_ERROR,
                             "yamlconf: %s: 'filter' value must be a scalar string",
                             fname);
                    yaml_event_delete(&ev);
                    free(kname); free(script_str); free(filter_str);
                    if (actions) cnfstmtDestructLst(actions);
                    nvlstDestruct(lst);
                    ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
                }
                free(filter_str);
                filter_str = strdup((char *)ev.data.scalar.value);
                yaml_event_delete(&ev);
                free(kname);
                if (filter_str == NULL) {
                    free(script_str);
                    if (actions) cnfstmtDestructLst(actions);
                    nvlstDestruct(lst);
                    ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
                }
            } else if (!strcmp(kname, "actions")) {
                /* actions: [ {type: ..., ...}, ... ] */
                if (ev.type != YAML_SEQUENCE_START_EVENT) {
                    LogError(0, RS_RET_CONF_PARSE_ERROR,
                             "yamlconf: %s: 'actions' value must be a sequence",
                             fname);
                    yaml_event_delete(&ev);
                    free(kname); free(script_str); free(filter_str);
                    if (actions) cnfstmtDestructLst(actions);
                    nvlstDestruct(lst);
                    ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
                }
                yaml_event_delete(&ev);
                free(kname);
                if (actions) cnfstmtDestructLst(actions);
                rsRetVal ar = parse_actions_sequence(parser, &actions, fname);
                if (ar != RS_RET_OK) {
                    free(script_str); free(filter_str);
                    nvlstDestruct(lst);
                    ABORT_FINALIZE(ar);
                }
            } else {
                /* All other keys go into the header nvlst */
                es_str_t *keyestr = scalar_to_estr(kname);
                free(kname);
                if (keyestr == NULL) {
                    yaml_event_delete(&ev);
                    free(script_str); free(filter_str);
                    if (actions) cnfstmtDestructLst(actions);
                    nvlstDestruct(lst);
                    ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
                }

                struct nvlst *node = NULL;
                if (ev.type == YAML_SCALAR_EVENT) {
                    es_str_t *val = scalar_to_estr((char *)ev.data.scalar.value);
                    yaml_event_delete(&ev);
                    if (val == NULL) {
                        es_deleteStr(keyestr); free(script_str); free(filter_str);
                        if (actions) cnfstmtDestructLst(actions);
                        nvlstDestruct(lst);
                        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
                    }
                    node = nvlstNewStr(val);
                    if (node == NULL) {
                        es_deleteStr(keyestr); es_deleteStr(val);
                        free(script_str); free(filter_str);
                        if (actions) cnfstmtDestructLst(actions);
                        nvlstDestruct(lst);
                        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
                    }
                } else if (ev.type == YAML_SEQUENCE_START_EVENT) {
                    yaml_event_delete(&ev);
                    struct cnfarray *arr = NULL;
                    rsRetVal arrRet = build_array_from_sequence(parser, &arr, fname);
                    if (arrRet != RS_RET_OK) {
                        es_deleteStr(keyestr); free(script_str); free(filter_str);
                        if (actions) cnfstmtDestructLst(actions);
                        nvlstDestruct(lst);
                        ABORT_FINALIZE(arrRet);
                    }
                    node = nvlstNewArray(arr);
                    if (node == NULL) {
                        es_deleteStr(keyestr); free(script_str); free(filter_str);
                        if (actions) cnfstmtDestructLst(actions);
                        nvlstDestruct(lst);
                        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
                    }
                } else {
                    yaml_event_delete(&ev);
                    es_deleteStr(keyestr); free(script_str); free(filter_str);
                    if (actions) cnfstmtDestructLst(actions);
                    nvlstDestruct(lst);
                    LogError(0, RS_RET_CONF_PARSE_ERROR,
                             "yamlconf: %s: unexpected value type in ruleset mapping",
                             fname);
                    ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
                }

                nvlstSetName(node, keyestr);
                node->next = lst;
                lst = node;
            }
        } /* end per-ruleset key loop */

        /* Sanity check: script: and actions: are mutually exclusive */
        if (script_str != NULL && actions != NULL) {
            LogError(0, RS_RET_CONF_PARSE_ERROR,
                     "yamlconf: %s: ruleset cannot have both 'script' and 'actions';"
                     " use one or the other", fname);
            free(script_str); free(filter_str);
            cnfstmtDestructLst(actions);
            nvlstDestruct(lst);
            ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
        }

        if (script_str != NULL && *script_str != '\0') {
            /* Synthesise a complete RainerScript  ruleset(...)  {  script  }
             * block and push it onto the flex buffer stack.  The bison parser
             * will call cnfDoObj() itself after parsing the whole block,
             * setting cnfobj->script correctly via rulesetProcessCnf(). */
            free(filter_str);
            filter_str = NULL;
            es_str_t *estr = NULL;
            rsRetVal br = build_ruleset_rainerscript(lst, script_str, &estr);
            nvlstDestruct(lst);
            lst = NULL;
            free(script_str);
            script_str = NULL;
            if (br != RS_RET_OK) ABORT_FINALIZE(br);
            /* cnfAddConfigBuffer() takes ownership of estr */
            cnfAddConfigBuffer(estr, fname);
        } else if (actions != NULL) {
            /* Phase 2 structured shortcut: build cnfstmt chain directly.
             * If filter: was given, wrap the action chain in a filter node.
             * Otherwise the actions run unconditionally (equivalent to *.*). */
            struct cnfstmt *body = actions;
            actions = NULL;
            if (filter_str != NULL && *filter_str != '\0') {
                struct cnfstmt *filt;
                /* Property filters start with ':', PRI filters do not */
                if (filter_str[0] == ':') {
                    filt = cnfstmtNewPROPFILT(filter_str, body);
                } else {
                    filt = cnfstmtNewPRIFILT(filter_str, body);
                }
                if (filt == NULL) {
                    cnfstmtDestructLst(body);
                    nvlstDestruct(lst);
                    free(filter_str);
                    ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
                }
                /* filter_str ownership taken by cnfstmt (printable field) */
                filter_str = NULL;
                body = filt;
            }
            free(filter_str);
            filter_str = NULL;
            struct cnfobj *obj = cnfobjNew(CNFOBJ_RULESET, lst);
            lst = NULL;
            if (obj == NULL) {
                cnfstmtDestructLst(body);
                ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
            }
            obj->script = body;
            cnfDoObj(obj);
        } else {
            /* No script or actions — emit just the ruleset header. */
            free(script_str);
            script_str = NULL;
            free(filter_str);
            filter_str = NULL;
            struct cnfobj *obj = cnfobjNew(CNFOBJ_RULESET, lst);
            lst = NULL;  /* cnfobjNew takes ownership */
            if (obj == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
            cnfDoObj(obj);
        }
    } /* end ruleset sequence loop */

finalize_it:
    RETiRet;
}

/* Parse the include: sequence and trigger file inclusion.
 * Each item is a mapping with "path" (required) and "optional" (optional).
 * Delegates to cnfDoInclude() which already handles .yaml/.yml routing. */
static rsRetVal
parse_include_sequence(yaml_parser_t *parser, const char *fname)
{
    yaml_event_t ev;
    DEFiRet;

    while (1) {
        CHKiRet(next_event(parser, &ev, fname));
        if (ev.type == YAML_SEQUENCE_END_EVENT) {
            yaml_event_delete(&ev);
            break;
        }
        if (ev.type != YAML_MAPPING_START_EVENT) {
            LogError(0, RS_RET_CONF_PARSE_ERROR,
                     "yamlconf: %s: include sequence item must be a mapping",
                     fname);
            yaml_event_delete(&ev);
            ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
        }
        yaml_event_delete(&ev);

        char *path = NULL;
        int optional = 0;

        /* Read the mapping */
        while (1) {
            CHKiRet(next_event(parser, &ev, fname));
            if (ev.type == YAML_MAPPING_END_EVENT) {
                yaml_event_delete(&ev);
                break;
            }
            if (ev.type != YAML_SCALAR_EVENT) {
                yaml_event_delete(&ev);
                free(path);
                ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
            }
            char *k = strdup((char *)ev.data.scalar.value);
            yaml_event_delete(&ev);
            if (k == NULL) { free(path); ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY); }

            yaml_event_t vev;
            CHKiRet(next_event(parser, &vev, fname));
            if (vev.type == YAML_SCALAR_EVENT) {
                const char *v = (char *)vev.data.scalar.value;
                if (!strcmp(k, "path")) {
                    free(path);
                    path = strdup(v);
                } else if (!strcmp(k, "optional")) {
                    optional = (!strcasecmp(v, "on") || !strcasecmp(v, "yes") ||
                                !strcmp(v, "1")) ? 1 : 0;
                }
                yaml_event_delete(&vev);
            } else {
                yaml_event_delete(&vev);
            }
            free(k);
        }

        if (path != NULL) {
            cnfDoInclude(path, optional);
            free(path);
        } else {
            LogError(0, RS_RET_CONF_PARSE_ERROR,
                     "yamlconf: %s: include item missing required 'path' key",
                     fname);
            /* non-fatal: continue */
        }
    }

finalize_it:
    RETiRet;
}

/* --------------------------------------------------------------------------
 * Top-level dispatcher
 * -------------------------------------------------------------------------- */

/* Map a top-level YAML key to its cnfobjType and call the right parser.
 * The opening event for the section value (MAPPING_START or SEQUENCE_START)
 * has already been consumed when this function is called. */
static rsRetVal
process_top_level(yaml_parser_t *parser, const char *key, const char *fname)
{
    DEFiRet;

    if (!strcmp(key, "global")) {
        CHKiRet(parse_singleton_obj(parser, CNFOBJ_GLOBAL, fname));
    } else if (!strcmp(key, "mainqueue") || !strcmp(key, "main_queue")) {
        CHKiRet(parse_singleton_obj(parser, CNFOBJ_MAINQ, fname));
    } else if (!strcmp(key, "modules")) {
        CHKiRet(parse_obj_sequence(parser, CNFOBJ_MODULE, fname));
    } else if (!strcmp(key, "inputs")) {
        CHKiRet(parse_obj_sequence(parser, CNFOBJ_INPUT, fname));
    } else if (!strcmp(key, "templates")) {
        CHKiRet(parse_obj_sequence(parser, CNFOBJ_TPL, fname));
    } else if (!strcmp(key, "rulesets")) {
        CHKiRet(parse_ruleset_sequence(parser, fname));
    } else if (!strcmp(key, "parsers")) {
        CHKiRet(parse_obj_sequence(parser, CNFOBJ_PARSER, fname));
    } else if (!strcmp(key, "lookup_tables")) {
        CHKiRet(parse_obj_sequence(parser, CNFOBJ_LOOKUP_TABLE, fname));
    } else if (!strcmp(key, "dyn_stats")) {
        CHKiRet(parse_obj_sequence(parser, CNFOBJ_DYN_STATS, fname));
    } else if (!strcmp(key, "perctile_stats")) {
        CHKiRet(parse_obj_sequence(parser, CNFOBJ_PERCTILE_STATS, fname));
    } else if (!strcmp(key, "ratelimits")) {
        CHKiRet(parse_obj_sequence(parser, CNFOBJ_RATELIMIT, fname));
    } else if (!strcmp(key, "timezones")) {
        CHKiRet(parse_obj_sequence(parser, CNFOBJ_TIMEZONE, fname));
    } else if (!strcmp(key, "include")) {
        CHKiRet(parse_include_sequence(parser, fname));
    } else if (!strcmp(key, "version")) {
        /* Informational; skip the scalar value */
        yaml_event_t ev;
        if (next_event(parser, &ev, fname) == RS_RET_OK)
            yaml_event_delete(&ev);
    } else {
        LogError(0, RS_RET_CONF_PARSE_ERROR,
                 "yamlconf: %s: unknown top-level key '%s' – ignoring",
                 fname, key);
        /* Skip the value node so the parser stays in sync */
        yaml_event_t ev;
        if (next_event(parser, &ev, fname) == RS_RET_OK) {
            if (ev.type == YAML_MAPPING_START_EVENT ||
                ev.type == YAML_SEQUENCE_START_EVENT) {
                yaml_event_delete(&ev);
                skip_node(parser);
            } else {
                yaml_event_delete(&ev);
            }
        }
    }

finalize_it:
    RETiRet;
}

/* --------------------------------------------------------------------------
 * Public entry point
 * -------------------------------------------------------------------------- */

rsRetVal
yamlconf_load(const char *fname)
{
    yaml_parser_t parser;
    yaml_event_t ev;
    int parserInit = 0;
    FILE *fh = NULL;
    DEFiRet;

    fh = fopen(fname, "r");
    if (fh == NULL) {
        LogError(errno, RS_RET_CONF_FILE_NOT_FOUND,
                 "yamlconf: cannot open config file '%s'", fname);
        ABORT_FINALIZE(RS_RET_CONF_FILE_NOT_FOUND);
    }

    if (!yaml_parser_initialize(&parser)) {
        LogError(0, RS_RET_INTERNAL_ERROR,
                 "yamlconf: failed to initialize YAML parser");
        ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
    }
    parserInit = 1;
    yaml_parser_set_input_file(&parser, fh);

    /* Consume stream/document start */
    CHKiRet(expect_event(&parser, &ev, YAML_STREAM_START_EVENT,
                          "stream start", fname));
    yaml_event_delete(&ev);
    CHKiRet(expect_event(&parser, &ev, YAML_DOCUMENT_START_EVENT,
                          "document start", fname));
    yaml_event_delete(&ev);

    /* The top-level node must be a mapping */
    CHKiRet(expect_event(&parser, &ev, YAML_MAPPING_START_EVENT,
                          "top-level mapping", fname));
    yaml_event_delete(&ev);

    /* Walk the top-level key/value pairs */
    while (1) {
        CHKiRet(next_event(&parser, &ev, fname));

        if (ev.type == YAML_MAPPING_END_EVENT) {
            yaml_event_delete(&ev);
            break;
        }
        if (ev.type != YAML_SCALAR_EVENT) {
            LogError(0, RS_RET_CONF_PARSE_ERROR,
                     "yamlconf: %s: expected scalar key at top level", fname);
            yaml_event_delete(&ev);
            ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
        }

        char *topkey = strdup((char *)ev.data.scalar.value);
        yaml_event_delete(&ev);
        if (topkey == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

        /* Consume the opening event of the value node before dispatching */
        yaml_event_t vev;
        rsRetVal vret = next_event(&parser, &vev, fname);
        if (vret != RS_RET_OK) {
            free(topkey);
            ABORT_FINALIZE(vret);
        }

        /* For sections that expect a sequence we need SEQUENCE_START;
         * for singleton sections we need MAPPING_START.
         * process_top_level() takes responsibility from here, but we already
         * consumed the opening event – so verify it makes sense. */
        if (vev.type != YAML_MAPPING_START_EVENT &&
            vev.type != YAML_SEQUENCE_START_EVENT &&
            vev.type != YAML_SCALAR_EVENT) {
            LogError(0, RS_RET_CONF_PARSE_ERROR,
                     "yamlconf: %s: unexpected value type for key '%s'",
                     fname, topkey);
            yaml_event_delete(&vev);
            free(topkey);
            ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
        }

        /* For scalar values (e.g. version: 2) put the event back via a small
         * detour: handle scalars inline here before calling process_top_level. */
        if (vev.type == YAML_SCALAR_EVENT) {
            if (!strcmp(topkey, "version")) {
                /* silently accept */
            } else {
                LogError(0, RS_RET_CONF_PARSE_ERROR,
                         "yamlconf: %s: key '%s' has a scalar value but expects"
                         " a mapping or sequence", fname, topkey);
                /* non-fatal */
            }
            yaml_event_delete(&vev);
            free(topkey);
            continue;
        }

        yaml_event_delete(&vev);
        /* process_top_level() will now call the right parser which will consume
         * the rest of the value node (up to and including its END event). */
        rsRetVal pret = process_top_level(&parser, topkey, fname);
        free(topkey);
        if (pret != RS_RET_OK) {
            iRet = pret; /* record but try to continue for better diagnostics */
        }
    }

finalize_it:
    if (parserInit) yaml_parser_delete(&parser);
    if (fh) fclose(fh);
    RETiRet;
}

#endif /* HAVE_LIBYAML */
