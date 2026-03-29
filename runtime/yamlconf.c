/** @file yamlconf.c
 * @brief YAML configuration file loader for rsyslog.
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
 *                  or [ { name: n, statements: [{if:...,action:...}, ...] }, ... ]
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

    /* Maximum nesting depth tracked during YAML node skipping.
     * rsyslog config objects are inherently shallow (2-3 levels); 16 guards
     * against malformed or adversarial YAML without needing a full DFS. */
    /** @brief Maximum YAML nesting depth allowed when skipping unknown nodes. */
    #define YAMLCONF_MAX_DEPTH 16

/* --------------------------------------------------------------------------
 * Forward declarations
 * -------------------------------------------------------------------------- */
static rsRetVal process_top_level(yaml_parser_t *parser, const char *key, const char *fname);
static rsRetVal parse_singleton_obj(yaml_parser_t *parser, enum cnfobjType objType, const char *fname);
static rsRetVal parse_obj_sequence(yaml_parser_t *parser, enum cnfobjType objType, const char *fname);
static rsRetVal parse_template_sequence(yaml_parser_t *parser, const char *fname);
static rsRetVal parse_elements_sequence(yaml_parser_t *parser, struct objlst **out, const char *fname);
static rsRetVal parse_ruleset_sequence(yaml_parser_t *parser, const char *fname);
static rsRetVal parse_actions_sequence(yaml_parser_t *parser, struct cnfstmt **head_out, const char *fname);
static rsRetVal parse_include_sequence(yaml_parser_t *parser, const char *fname);
static rsRetVal build_nvlst_from_mapping(yaml_parser_t *parser, struct nvlst **out, const char *fname);
static rsRetVal build_array_from_sequence(yaml_parser_t *parser, struct cnfarray **out, const char *fname);
static rsRetVal yaml_value_to_nvlst_node(
    yaml_parser_t *parser, const char *fname, yaml_event_t *ev, const char *key, struct nvlst **out);
static rsRetVal build_stmts_rs(yaml_parser_t *parser, es_str_t **s, const char *fname);
static rsRetVal expect_event(
    yaml_parser_t *parser, yaml_event_t *ev, yaml_event_type_t expected, const char *ctx, const char *fname);
static void skip_node(yaml_parser_t *parser);

/* --------------------------------------------------------------------------
 * Helpers
 * -------------------------------------------------------------------------- */

/* Convert a YAML scalar value (C string) into a heap-allocated es_str_t.
 * The caller owns the returned pointer. */
static es_str_t *scalar_to_estr(const char *val) {
    return es_newStrFromCStr(val, strlen(val));
}

/* Append a C string to an es_str_t, growing as needed.
 * Returns 0 on success, -1 on error. */
static int estr_append_cstr(es_str_t **s, const char *buf, size_t len) {
    if (len > (size_t)INT_MAX) {
        LogError(0, RS_RET_OUT_OF_MEMORY, "yamlconf: string length %zu exceeds maximum supported size", len);
        return -1;
    }
    return es_addBuf(s, buf, (es_size_t)len);
}

/* Append a single character to an es_str_t. */
static int estr_append_char(es_str_t **s, char c) {
    return es_addChar(s, (unsigned char)c);
}

/* Append a string value enclosed in double-quotes, escaping backslash and
 * double-quote characters with a preceding backslash.  This produces valid
 * RainerScript string literals. */
static int estr_append_quoted(es_str_t **s, const char *val, size_t len) {
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
static rsRetVal build_ruleset_rainerscript(struct nvlst *lst, const char *script_str, es_str_t **out) {
    es_str_t *s = es_newStr(256);
    DEFiRet;

    if (s == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

    if (estr_append_cstr(&s, "ruleset(", 8) != 0) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

    for (struct nvlst *n = lst; n != NULL; n = n->next) {
        if (n != lst) {
            if (estr_append_char(&s, ' ') != 0) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        }
        /* key name */
        size_t klen = es_strlen(n->name);
        if (estr_append_cstr(&s, (char *)es_getBufAddr(n->name), klen) != 0) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
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
                if (i > 0 && estr_append_char(&s, ',') != 0) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
                size_t elen = es_strlen(ar->arr[i]);
                if (estr_append_quoted(&s, (char *)es_getBufAddr(ar->arr[i]), elen) != 0)
                    ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
            }
            if (estr_append_char(&s, ']') != 0) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        } else {
            /* Unexpected type; skip with a warning (should not happen for rulesets) */
            LogError(0, RS_RET_CONF_PARSE_ERROR,
                     "yamlconf: unexpected nvlst value datatype '%c' in ruleset "
                     "parameter serialisation, skipping",
                     n->val.datatype);
            if (estr_append_cstr(&s, "\"\"", 2) != 0) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        }
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
    s = NULL; /* caller owns it */
    iRet = RS_RET_OK;

finalize_it:
    if (s != NULL) es_deleteStr(s);
    RETiRet;
}

/* Emit a parse-error message that includes the YAML source location. */
static void yaml_errmsg(yaml_parser_t *parser, const char *fname, const char *extra) {
    if (parser->problem != NULL) {
        LogError(0, RS_RET_CONF_PARSE_ERROR, "yamlconf: %s: %s (line %lu col %lu)%s%s", fname, parser->problem,
                 (unsigned long)parser->problem_mark.line + 1, (unsigned long)parser->problem_mark.column + 1,
                 extra ? " " : "", extra ? extra : "");
    } else {
        LogError(0, RS_RET_CONF_PARSE_ERROR, "yamlconf: %s: parse error%s%s", fname, extra ? " " : "",
                 extra ? extra : "");
    }
}

/* Pull the next event from the parser; on error log and return RS_RET_CONF_PARSE_ERROR. */
static rsRetVal next_event(yaml_parser_t *parser, yaml_event_t *ev, const char *fname) {
    DEFiRet;
    if (!yaml_parser_parse(parser, ev)) {
        yaml_errmsg(parser, fname, NULL);
        ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
    }
finalize_it:
    RETiRet;
}

/* Pull the next event and assert it has the expected type. */
static rsRetVal expect_event(
    yaml_parser_t *parser, yaml_event_t *ev, yaml_event_type_t expected, const char *ctx, const char *fname) {
    DEFiRet;
    CHKiRet(next_event(parser, ev, fname));
    if (ev->type != expected) {
        LogError(0, RS_RET_CONF_PARSE_ERROR, "yamlconf: %s: expected event %d got %d in %s", fname, (int)expected,
                 (int)ev->type, ctx);
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
static void skip_node(yaml_parser_t *parser) {
    yaml_event_t ev;
    int depth = 1;
    while (depth > 0) {
        if (!yaml_parser_parse(parser, &ev)) break;
        PRAGMA_DIAGNOSTIC_PUSH
        PRAGMA_IGNORE_Wswitch_enum switch (ev.type) {
            case YAML_MAPPING_START_EVENT:
            case YAML_SEQUENCE_START_EVENT:
                depth++;
                if (depth > YAMLCONF_MAX_DEPTH) {
                    yaml_event_delete(&ev);
                    LogError(0, RS_RET_CONF_PARSE_ERROR,
                             "yamlconf: YAML nesting exceeds maximum depth %d; "
                             "skipping remainder",
                             YAMLCONF_MAX_DEPTH);
                    return;
                }
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
static rsRetVal build_array_from_sequence(yaml_parser_t *parser, struct cnfarray **out, const char *fname) {
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
            LogError(0, RS_RET_CONF_PARSE_ERROR, "yamlconf: %s: array elements must be scalars", fname);
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

/**
 * @brief Converts one YAML value event into a named nvlst node.
 *
 * The event @p ev must already have been pulled from the parser by the
 * caller (the value event following a key scalar).  The event is always
 * consumed before returning, regardless of success or failure.
 *
 * Callers that need to handle YAML_MAPPING_START_EVENT specially (e.g. to
 * skip nested mappings with a warning) should check ev->type before calling
 * and only invoke this function for SCALAR and SEQUENCE_START events.
 *
 * @param parser  libyaml parser; read-from only when ev is SEQUENCE_START.
 * @param fname   Config file name used in diagnostic messages.
 * @param ev      Pre-read value event; always consumed.
 * @param key     Parameter key (C string); copied into the node's name field.
 * @param out     Set to the new nvlst node on success; NULL on any error.
 * @return RS_RET_OK, RS_RET_OUT_OF_MEMORY, or RS_RET_CONF_PARSE_ERROR.
 */
static rsRetVal yaml_value_to_nvlst_node(
    yaml_parser_t *parser, const char *fname, yaml_event_t *ev, const char *key, struct nvlst **out) {
    es_str_t *keyestr = NULL;
    struct nvlst *node = NULL;
    DEFiRet;

    *out = NULL;
    keyestr = scalar_to_estr(key);
    if (keyestr == NULL) {
        yaml_event_delete(ev);
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    if (ev->type == YAML_SCALAR_EVENT) {
        es_str_t *val = scalar_to_estr((char *)ev->data.scalar.value);
        yaml_event_delete(ev);
        if (val == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        node = nvlstNewStr(val);
        if (node == NULL) {
            es_deleteStr(val);
            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        }
    } else if (ev->type == YAML_SEQUENCE_START_EVENT) {
        struct cnfarray *arr = NULL;
        yaml_event_delete(ev);
        CHKiRet(build_array_from_sequence(parser, &arr, fname));
        node = nvlstNewArray(arr);
        if (node == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    } else {
        yaml_event_delete(ev);
        LogError(0, RS_RET_CONF_PARSE_ERROR, "yamlconf: %s: unsupported value type for parameter '%s'", fname, key);
        ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
    }

    nvlstSetName(node, keyestr);
    keyestr = NULL; /* owned by node */
    *out = node;
    node = NULL;

finalize_it:
    es_deleteStr(keyestr);
    if (node != NULL) nvlstDestruct(node);
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
static rsRetVal build_nvlst_from_mapping(yaml_parser_t *parser, struct nvlst **out, const char *fname) {
    yaml_event_t ev;
    struct nvlst *lst = NULL;
    struct nvlst *node;
    char *key = NULL;
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
            LogError(0, RS_RET_CONF_PARSE_ERROR, "yamlconf: %s: expected scalar key in mapping", fname);
            yaml_event_delete(&ev);
            ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
        }

        free(key);
        key = strdup((char *)ev.data.scalar.value);
        yaml_event_delete(&ev);
        if (key == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

        /* Now read the value */
        CHKiRet(next_event(parser, &ev, fname));

        if (ev.type == YAML_MAPPING_START_EVENT) {
            /* Nested mappings are not part of the rsyslog parameter model.
             * Skip and warn so the user knows the key was ignored. */
            yaml_event_delete(&ev);
            LogError(0, RS_RET_CONF_PARSE_ERROR,
                     "yamlconf: %s: nested mapping for key '%s' is not supported – ignoring", fname, key);
            skip_node(parser);
            continue;
        }
        CHKiRet(yaml_value_to_nvlst_node(parser, fname, &ev, key, &node));
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
static rsRetVal parse_singleton_obj(yaml_parser_t *parser, enum cnfobjType objType, const char *fname) {
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
static rsRetVal parse_obj_sequence(yaml_parser_t *parser, enum cnfobjType objType, const char *fname) {
    yaml_event_t ev;
    DEFiRet;

    while (1) {
        CHKiRet(next_event(parser, &ev, fname));

        if (ev.type == YAML_SEQUENCE_END_EVENT) {
            yaml_event_delete(&ev);
            break;
        }
        if (ev.type != YAML_MAPPING_START_EVENT) {
            LogError(0, RS_RET_CONF_PARSE_ERROR, "yamlconf: %s: sequence item must be a mapping", fname);
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

/* Parse the elements: sequence of a list template.
 *
 * Each item is a mapping with exactly one key, "property" or "constant",
 * whose value is a mapping of parameters:
 *
 *   elements:
 *     - property:
 *         name: msg
 *     - constant:
 *         value: "\n"
 *
 * Builds a struct objlst of CNFOBJ_PROPERTY / CNFOBJ_CONSTANT nodes that
 * mirrors what the bison grammar constructs for:
 *   template(name="t" type="list") { property(name="msg") constant(value="\n") }
 *
 * The SEQUENCE_START event must already have been consumed by the caller.
 * Ownership of the returned objlst passes to the caller. */
static rsRetVal parse_elements_sequence(yaml_parser_t *parser, struct objlst **out, const char *fname) {
    yaml_event_t ev;
    struct objlst *subobjs = NULL;
    char *elemtype = NULL;
    DEFiRet;

    *out = NULL;

    while (1) {
        CHKiRet(next_event(parser, &ev, fname));
        if (ev.type == YAML_SEQUENCE_END_EVENT) {
            yaml_event_delete(&ev);
            break;
        }
        if (ev.type != YAML_MAPPING_START_EVENT) {
            LogError(0, RS_RET_CONF_PARSE_ERROR,
                     "yamlconf: %s: each elements: item must be a mapping with a "
                     "'property' or 'constant' key",
                     fname);
            yaml_event_delete(&ev);
            ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
        }
        yaml_event_delete(&ev);

        /* Read the element type key: "property" or "constant" */
        CHKiRet(next_event(parser, &ev, fname));
        if (ev.type != YAML_SCALAR_EVENT) {
            LogError(0, RS_RET_CONF_PARSE_ERROR, "yamlconf: %s: elements: item key must be 'property' or 'constant'",
                     fname);
            yaml_event_delete(&ev);
            ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
        }
        free(elemtype);
        elemtype = strdup((char *)ev.data.scalar.value);
        yaml_event_delete(&ev);
        if (elemtype == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

        enum cnfobjType etype;
        if (!strcmp(elemtype, "property")) {
            etype = CNFOBJ_PROPERTY;
        } else if (!strcmp(elemtype, "constant")) {
            etype = CNFOBJ_CONSTANT;
        } else {
            LogError(0, RS_RET_CONF_PARSE_ERROR,
                     "yamlconf: %s: elements: item key must be 'property' or 'constant', got '%s'", fname, elemtype);
            ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
        }

        /* Read MAPPING_START for the element parameters */
        CHKiRet(expect_event(parser, &ev, YAML_MAPPING_START_EVENT, "element params mapping", fname));
        yaml_event_delete(&ev);

        struct nvlst *elst = NULL;
        CHKiRet(build_nvlst_from_mapping(parser, &elst, fname));

        struct cnfobj *eobj = cnfobjNew(etype, elst);
        elst = NULL; /* owned by eobj */
        if (eobj == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

        subobjs = objlstAdd(subobjs, eobj);

        /* Consume the MAPPING_END of the outer element item mapping */
        CHKiRet(expect_event(parser, &ev, YAML_MAPPING_END_EVENT, "element item end", fname));
        yaml_event_delete(&ev);
    }

    *out = subobjs;
    subobjs = NULL;

finalize_it:
    free(elemtype);
    if (subobjs != NULL) objlstDestruct(subobjs);
    RETiRet;
}

/* Parse the templates: sequence.
 *
 * Simple templates (type=string/subtree/plugin) use the flat nvlst path
 * identical to other object sequences.
 *
 * List templates (type=list) need sub-objects: the "elements:" key takes
 * a sequence of property:/constant: items that are built as
 * CNFOBJ_PROPERTY/CNFOBJ_CONSTANT nodes and attached to the template
 * cnfobj->subobjs — exactly what the bison grammar does for:
 *   template(name="t" type="list") { property(...) constant(...) }
 *
 * YAML schema summary:
 *   # string template
 *   - name: t1
 *     type: string
 *     string: "%msg%\n"
 *
 *   # subtree template
 *   - name: t2
 *     type: subtree
 *     subtree: "$!all-json"
 *
 *   # list template
 *   - name: t3
 *     type: list
 *     elements:
 *       - property:
 *           name: msg
 *       - constant:
 *           value: "\n"
 *
 * SEQUENCE_START must already be consumed by the caller. */
static rsRetVal parse_template_sequence(yaml_parser_t *parser, const char *fname) {
    yaml_event_t ev;
    struct nvlst *lst = NULL;
    struct objlst *subobjs = NULL;
    char *kname = NULL;
    DEFiRet;

    while (1) {
        CHKiRet(next_event(parser, &ev, fname));
        if (ev.type == YAML_SEQUENCE_END_EVENT) {
            yaml_event_delete(&ev);
            break;
        }
        if (ev.type != YAML_MAPPING_START_EVENT) {
            LogError(0, RS_RET_CONF_PARSE_ERROR, "yamlconf: %s: templates sequence item must be a mapping", fname);
            yaml_event_delete(&ev);
            ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
        }
        yaml_event_delete(&ev);

        /* Reset per-template state */
        lst = NULL;
        subobjs = NULL;

        /* Walk the template mapping manually to intercept "elements:" */
        while (1) {
            CHKiRet(next_event(parser, &ev, fname));
            if (ev.type == YAML_MAPPING_END_EVENT) {
                yaml_event_delete(&ev);
                break;
            }
            if (ev.type != YAML_SCALAR_EVENT) {
                LogError(0, RS_RET_CONF_PARSE_ERROR, "yamlconf: %s: expected scalar key in template mapping", fname);
                yaml_event_delete(&ev);
                ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
            }
            free(kname);
            kname = strdup((char *)ev.data.scalar.value);
            yaml_event_delete(&ev);
            if (kname == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

            CHKiRet(next_event(parser, &ev, fname));

            if (!strcmp(kname, "elements")) {
                /* elements: [...] – sub-objects for list template */
                if (ev.type != YAML_SEQUENCE_START_EVENT) {
                    LogError(0, RS_RET_CONF_PARSE_ERROR,
                             "yamlconf: %s: 'elements' value must be a sequence of "
                             "property/constant items",
                             fname);
                    yaml_event_delete(&ev);
                    ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
                }
                yaml_event_delete(&ev);
                if (subobjs != NULL) objlstDestruct(subobjs);
                subobjs = NULL;
                CHKiRet(parse_elements_sequence(parser, &subobjs, fname));
            } else {
                /* Regular template param: add to nvlst */
                struct nvlst *node = NULL;
                CHKiRet(yaml_value_to_nvlst_node(parser, fname, &ev, kname, &node));
                node->next = lst;
                lst = node;
            }
        } /* end per-template key loop */

        struct cnfobj *obj = cnfobjNew(CNFOBJ_TPL, lst);
        lst = NULL; /* owned by obj */
        if (obj == NULL) {
            if (subobjs != NULL) objlstDestruct(subobjs);
            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        }
        obj->subobjs = subobjs; /* NULL for non-list templates */
        subobjs = NULL;
        cnfDoObj(obj);
    } /* end template sequence loop */

finalize_it:
    free(kname);
    nvlstDestruct(lst);
    if (subobjs != NULL) objlstDestruct(subobjs);
    RETiRet;
}

/* Parse the actions: sequence.
 *
 * Each item is a YAML mapping that mirrors an action() object: the "type"
 * key names the output module and all other keys are passed as parameters.
 * We build an nvlst for each item and call cnfstmtNewAct() directly, then
 * chain the resulting cnfstmt nodes via ->next.
 *
 * The SEQUENCE_START event must already have been consumed by the caller.
 * On success *head_out points to the first node of the chain (possibly NULL
 * if the sequence was empty).  Ownership passes to the caller. */
static rsRetVal parse_actions_sequence(yaml_parser_t *parser, struct cnfstmt **head_out, const char *fname) {
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
            LogError(0, RS_RET_CONF_PARSE_ERROR, "yamlconf: %s: actions: item must be a mapping", fname);
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

/* --------------------------------------------------------------------------
 * YAML-native statements: block
 *
 * Converts a YAML statements: sequence into a RainerScript body string that
 * is then injected via cnfAddConfigBuffer(), following the same proven path
 * as the raw script: key.
 *
 * Supported statement types in a statements: sequence item:
 *
 *   Unconditional action (same schema as actions: items):
 *     - type: omfile
 *       file: /path
 *       template: outfmt
 *
 *   Conditional (single-action shorthand):
 *     - if: '$msg contains "msgnum:"'
 *       action:
 *         type: omfile
 *         file: /path
 *       else:          # optional
 *         - stop: true
 *
 *   Conditional (multi-statement then/else):
 *     - if: '$msg contains "msgnum:"'
 *       then:
 *         - type: omfile
 *           file: /path
 *       else:          # optional
 *         - stop: true
 *
 *   Control flow:
 *     - stop: true          # or stop:
 *     - continue: true      # or continue:
 *     - call: rulesetname
 *
 *   Variable assignment:
 *     - set:
 *         var: "$.nbr"
 *         expr: 'field($msg, 58, 2)'
 *     - unset: "$.var"
 *
 *   foreach loop:
 *     - foreach:
 *         var: "$.item"
 *         in: "$!array"
 *         do:
 *           - type: omfile
 *             file: /tmp/out
 *
 *   Indirect ruleset call:
 *     - call_indirect: "$!rulesetname"
 *
 *   Reload a lookup table (triggers an asynchronous reload of a previously
 *   loaded lookup table).  Unlike action() parameters, reload_lookup_table
 *   takes positional string arguments in RainerScript, so the YAML keys
 *   "table" and "stub_value" are extracted and emitted as quoted literals:
 *     - reload_lookup_table:
 *         table: myLookupTable
 *         stub_value: unknown    # optional; returned on reload failure
 *
 * -------------------------------------------------------------------------- */

/* Append all entries of 'lst' as  key="val"  pairs to the RainerScript
 * string *s.  Used to serialise action() parameter lists.
 * Reuses the same quoting logic as build_ruleset_rainerscript().
 * Returns 0 on success, -1 on allocation failure (int, not rsRetVal). */
static int append_nvlst_as_rs_params(es_str_t **s, struct nvlst *lst) {
    for (struct nvlst *n = lst; n != NULL; n = n->next) {
        if (n != lst) {
            if (estr_append_char(s, ' ') != 0) return -1;
        }
        size_t klen = es_strlen(n->name);
        if (estr_append_cstr(s, (char *)es_getBufAddr(n->name), klen) != 0) return -1;
        if (estr_append_char(s, '=') != 0) return -1;
        if (n->val.datatype == 'S') {
            size_t vlen = es_strlen(n->val.d.estr);
            if (estr_append_quoted(s, (char *)es_getBufAddr(n->val.d.estr), vlen) != 0) return -1;
        } else if (n->val.datatype == 'A') {
            struct cnfarray *ar = n->val.d.ar;
            if (estr_append_char(s, '[') != 0) return -1;
            for (int i = 0; i < ar->nmemb; i++) {
                if (i > 0 && estr_append_char(s, ',') != 0) return -1;
                size_t elen = es_strlen(ar->arr[i]);
                if (estr_append_quoted(s, (char *)es_getBufAddr(ar->arr[i]), elen) != 0) return -1;
            }
            if (estr_append_char(s, ']') != 0) return -1;
        } else {
            if (estr_append_cstr(s, "\"\"", 2) != 0) return -1;
        }
    }
    return 0;
}

/* Parse the sub-mapping for a foreach: statement.
 * MAPPING_START already consumed.  Fills *p_var, *p_in, *p_body (all
 * owned by caller; freed via the finalize_it: of build_one_stmt_rs). */
static rsRetVal parse_foreach_mapping(
    yaml_parser_t *parser, const char *fname, char **p_var, char **p_in, es_str_t **p_body) {
    yaml_event_t ev;
    DEFiRet;

    while (1) {
        CHKiRet(next_event(parser, &ev, fname));
        if (ev.type == YAML_MAPPING_END_EVENT) {
            yaml_event_delete(&ev);
            break;
        }
        if (ev.type != YAML_SCALAR_EVENT) {
            yaml_event_delete(&ev);
            ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
        }
        char *fk = strdup((char *)ev.data.scalar.value);
        yaml_event_delete(&ev);
        if (fk == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

        if (!strcmp(fk, "var") || !strcmp(fk, "in")) {
            yaml_event_t fv;
            rsRetVal fret = next_event(parser, &fv, fname);
            if (fret != RS_RET_OK) {
                free(fk);
                ABORT_FINALIZE(fret);
            }
            if (fv.type == YAML_SCALAR_EVENT) {
                char **dst = !strcmp(fk, "var") ? p_var : p_in;
                free(*dst);
                *dst = strdup((char *)fv.data.scalar.value);
                if (*dst == NULL) {
                    yaml_event_delete(&fv);
                    free(fk);
                    ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
                }
            }
            yaml_event_delete(&fv);
        } else if (!strcmp(fk, "do")) {
            yaml_event_t sev;
            rsRetVal sret = next_event(parser, &sev, fname);
            if (sret != RS_RET_OK) {
                free(fk);
                ABORT_FINALIZE(sret);
            }
            if (sev.type != YAML_SEQUENCE_START_EVENT) {
                LogError(0, RS_RET_CONF_PARSE_ERROR, "yamlconf: %s: foreach 'do' must be a sequence of statements",
                         fname);
                yaml_event_delete(&sev);
                free(fk);
                ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
            }
            yaml_event_delete(&sev);
            es_deleteStr(*p_body);
            *p_body = es_newStr(64);
            if (*p_body == NULL) {
                free(fk);
                ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
            }
            rsRetVal bret = build_stmts_rs(parser, p_body, fname);
            if (bret != RS_RET_OK) {
                free(fk);
                ABORT_FINALIZE(bret);
            }
        } else {
            /* skip unknown foreach sub-key */
            yaml_event_t fv;
            rsRetVal fret = next_event(parser, &fv, fname);
            if (fret == RS_RET_OK) yaml_event_delete(&fv);
        }
        free(fk);
    }

finalize_it:
    RETiRet;
}

/* Parse the sub-mapping for a set: statement.
 * MAPPING_START already consumed.  Fills *p_var and *p_expr. */
static rsRetVal parse_set_subkeys(yaml_parser_t *parser, const char *fname, char **p_var, char **p_expr) {
    yaml_event_t ev;
    DEFiRet;

    while (1) {
        CHKiRet(next_event(parser, &ev, fname));
        if (ev.type == YAML_MAPPING_END_EVENT) {
            yaml_event_delete(&ev);
            break;
        }
        if (ev.type != YAML_SCALAR_EVENT) {
            yaml_event_delete(&ev);
            ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
        }
        char *sk = strdup((char *)ev.data.scalar.value);
        yaml_event_delete(&ev);
        if (sk == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

        yaml_event_t sv;
        rsRetVal sret = next_event(parser, &sv, fname);
        if (sret != RS_RET_OK) {
            free(sk);
            ABORT_FINALIZE(sret);
        }
        if (sv.type == YAML_SCALAR_EVENT) {
            char **dst = !strcmp(sk, "var") ? p_var : (!strcmp(sk, "expr") ? p_expr : NULL);
            if (dst != NULL) {
                free(*dst);
                *dst = strdup((char *)sv.data.scalar.value);
                if (*dst == NULL) {
                    yaml_event_delete(&sv);
                    free(sk);
                    ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
                }
            }
        }
        yaml_event_delete(&sv);
        free(sk);
    }

finalize_it:
    RETiRet;
}

/* Build RainerScript for one statement mapping.  MAPPING_START already consumed.
 * Appends to *s.
 *
 * Design note: this function intentionally uses a two-phase collect-then-emit
 * pattern rather than being split further.  The statement type cannot be
 * determined until all keys in the YAML mapping have been read (e.g. an
 * item with only 'type:' is a flat action, but one with 'if:' and 'action:'
 * is an if/action shorthand).  Splitting collect from emit would require a
 * ~15-field context struct with its own cleanup, adding boilerplate without
 * clarity.  The foreach and set sub-mapping parsers are extracted into
 * parse_foreach_mapping() and parse_set_subkeys() to keep nesting shallow. */
static rsRetVal build_one_stmt_rs(yaml_parser_t *parser, es_str_t **s, const char *fname) {
    yaml_event_t ev;
    DEFiRet;

    /* Collect all top-level keys from this mapping so we can determine the
     * statement type.  Some values (then:/else:/action:/set:) are nested
     * sequences or mappings that we handle inline. */
    char *if_expr = NULL; /* value of if: */
    char *call_name = NULL; /* value of call: */
    char *call_indirect_expr = NULL; /* value of call_indirect: */
    char *set_var = NULL; /* value of set.var */
    char *set_expr = NULL; /* value of set.expr */
    char *unset_var = NULL; /* value of unset: */
    char *foreach_var = NULL; /* value of foreach.var */
    char *foreach_in = NULL; /* value of foreach.in */
    es_str_t *foreach_body = NULL; /* compiled body of foreach.do */
    es_str_t *then_rs = NULL;
    es_str_t *else_rs = NULL;
    es_str_t *action_rs = NULL; /* action: nested mapping → serialised RS */
    struct nvlst *act_lst = NULL; /* for flat action (type: key at top level) */
    int has_stop = 0;
    int has_continue = 0;
    int has_if = 0;
    int has_call = 0;
    int has_call_indirect = 0;
    int has_set = 0;
    int has_unset = 0;
    int has_foreach = 0;
    int has_type = 0; /* flat action form */
    /* reload_lookup_table: uses positional args: ("tableName"[, "stubValue"]) */
    char *rlt_table = NULL;
    char *rlt_stub = NULL;
    int has_rlt = 0;

    while (1) {
        CHKiRet(next_event(parser, &ev, fname));
        if (ev.type == YAML_MAPPING_END_EVENT) {
            yaml_event_delete(&ev);
            break;
        }
        if (ev.type != YAML_SCALAR_EVENT) {
            LogError(0, RS_RET_CONF_PARSE_ERROR, "yamlconf: %s: expected scalar key in statements item", fname);
            yaml_event_delete(&ev);
            ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
        }
        char *kname = strdup((char *)ev.data.scalar.value);
        yaml_event_delete(&ev);
        if (kname == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

        if (!strcmp(kname, "if")) {
            /* value: scalar expression string */
            yaml_event_t vev;
            CHKiRet_Hdlr(next_event(parser, &vev, fname)) {
                free(kname);
                FINALIZE;
            }
            if (vev.type != YAML_SCALAR_EVENT) {
                LogError(0, RS_RET_CONF_PARSE_ERROR, "yamlconf: %s: 'if' value must be a scalar expression", fname);
                yaml_event_delete(&vev);
                free(kname);
                ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
            }
            free(if_expr);
            if_expr = strdup((char *)vev.data.scalar.value);
            yaml_event_delete(&vev);
            has_if = 1;
            free(kname);
            if (if_expr == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

        } else if (!strcmp(kname, "then") || !strcmp(kname, "else")) {
            /* value: sequence of statement items */
            int is_else = (kname[0] == 'e');
            free(kname);
            yaml_event_t sev;
            CHKiRet(next_event(parser, &sev, fname));
            if (sev.type != YAML_SEQUENCE_START_EVENT) {
                LogError(0, RS_RET_CONF_PARSE_ERROR,
                         "yamlconf: %s: 'then'/'else' value must be a sequence of statements", fname);
                yaml_event_delete(&sev);
                ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
            }
            yaml_event_delete(&sev);
            es_str_t *branch_rs = es_newStr(64);
            if (branch_rs == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
            rsRetVal bret = build_stmts_rs(parser, &branch_rs, fname);
            if (bret != RS_RET_OK) {
                es_deleteStr(branch_rs);
                ABORT_FINALIZE(bret);
            }
            if (is_else) {
                es_deleteStr(else_rs);
                else_rs = branch_rs;
            } else {
                es_deleteStr(then_rs);
                then_rs = branch_rs;
            }

        } else if (!strcmp(kname, "action")) {
            /* value: mapping with action params (type: + others) */
            free(kname);
            yaml_event_t mev;
            CHKiRet(next_event(parser, &mev, fname));
            if (mev.type != YAML_MAPPING_START_EVENT) {
                LogError(0, RS_RET_CONF_PARSE_ERROR,
                         "yamlconf: %s: 'action' value must be a mapping of action parameters", fname);
                yaml_event_delete(&mev);
                ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
            }
            yaml_event_delete(&mev);
            struct nvlst *alst = NULL;
            rsRetVal aret = build_nvlst_from_mapping(parser, &alst, fname);
            if (aret != RS_RET_OK) ABORT_FINALIZE(aret);
            /* serialise to action(...) string */
            es_deleteStr(action_rs);
            action_rs = es_newStr(128);
            if (action_rs == NULL) {
                nvlstDestruct(alst);
                ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
            }
            if (estr_append_cstr(&action_rs, "action(", 7) != 0 || append_nvlst_as_rs_params(&action_rs, alst) != 0 ||
                estr_append_cstr(&action_rs, ")\n", 2) != 0) {
                nvlstDestruct(alst);
                ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
            }
            nvlstDestruct(alst);

        } else if (!strcmp(kname, "stop")) {
            free(kname);
            /* Only emit stop if value is truthy.  Canonical YAML falsy values
             * (false/no/off/0) must NOT trigger the statement — stop: false is
             * a valid way to conditionally disable a stop. Non-scalar values
             * are a config error. */
            yaml_event_t vev;
            CHKiRet(next_event(parser, &vev, fname));
            if (vev.type != YAML_SCALAR_EVENT) {
                LogError(0, RS_RET_CONF_PARSE_ERROR, "yamlconf: %s: 'stop' value must be a scalar (true/false)", fname);
                yaml_event_delete(&vev);
                ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
            }
            if (strcasecmp((char *)vev.data.scalar.value, "false") != 0 &&
                strcasecmp((char *)vev.data.scalar.value, "no") != 0 &&
                strcasecmp((char *)vev.data.scalar.value, "off") != 0 &&
                strcmp((char *)vev.data.scalar.value, "0") != 0) {
                has_stop = 1;
            }
            yaml_event_delete(&vev);

        } else if (!strcmp(kname, "continue")) {
            free(kname);
            yaml_event_t vev;
            CHKiRet(next_event(parser, &vev, fname));
            if (vev.type != YAML_SCALAR_EVENT) {
                LogError(0, RS_RET_CONF_PARSE_ERROR, "yamlconf: %s: 'continue' value must be a scalar (true/false)",
                         fname);
                yaml_event_delete(&vev);
                ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
            }
            if (strcasecmp((char *)vev.data.scalar.value, "false") != 0 &&
                strcasecmp((char *)vev.data.scalar.value, "no") != 0 &&
                strcasecmp((char *)vev.data.scalar.value, "off") != 0 &&
                strcmp((char *)vev.data.scalar.value, "0") != 0) {
                has_continue = 1;
            }
            yaml_event_delete(&vev);

        } else if (!strcmp(kname, "call")) {
            free(kname);
            yaml_event_t vev;
            CHKiRet(next_event(parser, &vev, fname));
            if (vev.type != YAML_SCALAR_EVENT) {
                LogError(0, RS_RET_CONF_PARSE_ERROR, "yamlconf: %s: 'call' value must be a ruleset name", fname);
                yaml_event_delete(&vev);
                ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
            }
            free(call_name);
            call_name = strdup((char *)vev.data.scalar.value);
            yaml_event_delete(&vev);
            has_call = 1;
            if (call_name == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

        } else if (!strcmp(kname, "call_indirect")) {
            free(kname);
            yaml_event_t vev;
            CHKiRet(next_event(parser, &vev, fname));
            if (vev.type != YAML_SCALAR_EVENT) {
                LogError(0, RS_RET_CONF_PARSE_ERROR,
                         "yamlconf: %s: 'call_indirect' value must be a variable expression", fname);
                yaml_event_delete(&vev);
                ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
            }
            free(call_indirect_expr);
            call_indirect_expr = strdup((char *)vev.data.scalar.value);
            yaml_event_delete(&vev);
            has_call_indirect = 1;
            if (call_indirect_expr == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

        } else if (!strcmp(kname, "unset")) {
            free(kname);
            yaml_event_t vev;
            CHKiRet(next_event(parser, &vev, fname));
            if (vev.type != YAML_SCALAR_EVENT) {
                LogError(0, RS_RET_CONF_PARSE_ERROR, "yamlconf: %s: 'unset' value must be a variable name", fname);
                yaml_event_delete(&vev);
                ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
            }
            free(unset_var);
            unset_var = strdup((char *)vev.data.scalar.value);
            yaml_event_delete(&vev);
            has_unset = 1;
            if (unset_var == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

        } else if (!strcmp(kname, "foreach")) {
            free(kname);
            has_foreach = 1;
            yaml_event_t mev;
            CHKiRet(next_event(parser, &mev, fname));
            if (mev.type != YAML_MAPPING_START_EVENT) {
                LogError(0, RS_RET_CONF_PARSE_ERROR,
                         "yamlconf: %s: 'foreach' value must be a mapping with var:, in:, and do: keys", fname);
                yaml_event_delete(&mev);
                ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
            }
            yaml_event_delete(&mev);
            CHKiRet(parse_foreach_mapping(parser, fname, &foreach_var, &foreach_in, &foreach_body));

        } else if (!strcmp(kname, "set")) {
            free(kname);
            has_set = 1;
            yaml_event_t mev;
            CHKiRet(next_event(parser, &mev, fname));
            if (mev.type != YAML_MAPPING_START_EVENT) {
                LogError(0, RS_RET_CONF_PARSE_ERROR,
                         "yamlconf: %s: 'set' value must be a mapping with var: and expr: keys", fname);
                yaml_event_delete(&mev);
                ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
            }
            yaml_event_delete(&mev);
            CHKiRet(parse_set_subkeys(parser, fname, &set_var, &set_expr));

        } else if (!strcmp(kname, "reload_lookup_table")) {
            free(kname);
            has_rlt = 1;
            yaml_event_t mev;
            CHKiRet(next_event(parser, &mev, fname));
            if (mev.type != YAML_MAPPING_START_EVENT) {
                LogError(0, RS_RET_CONF_PARSE_ERROR,
                         "yamlconf: %s: 'reload_lookup_table' value must be a mapping with at "
                         "least a 'table' key",
                         fname);
                yaml_event_delete(&mev);
                ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
            }
            yaml_event_delete(&mev);
            /* Extract positional args: table (required) and stub_value (optional).
             * The RainerScript syntax is reload_lookup_table("name"[, "stub"])
             * so we cannot use append_nvlst_as_rs_params(). */
            struct nvlst *rlt_nl = NULL;
            rsRetVal rlt_r = build_nvlst_from_mapping(parser, &rlt_nl, fname);
            if (rlt_r != RS_RET_OK) ABORT_FINALIZE(rlt_r);
            for (struct nvlst *n = rlt_nl; n != NULL; n = n->next) {
                if (n->val.datatype != 'S') continue;
                /* es_getBufAddr() is NOT null-terminated; use es_str2cstr() */
                char *nkey = es_str2cstr(n->name, NULL);
                char *nval = es_str2cstr(n->val.d.estr, NULL);
                if (nkey != NULL && nval != NULL) {
                    if (!strcmp(nkey, "table")) {
                        free(rlt_table);
                        rlt_table = strdup(nval);
                    } else if (!strcmp(nkey, "stub_value")) {
                        free(rlt_stub);
                        rlt_stub = strdup(nval);
                    }
                }
                free(nkey);
                free(nval);
            }
            nvlstDestruct(rlt_nl);
            if (rlt_table == NULL && has_rlt) {
                LogError(0, RS_RET_CONF_PARSE_ERROR, "yamlconf: %s: reload_lookup_table: 'table' key is required",
                         fname);
                ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
            }

        } else {
            /* Unknown key at statement level: treat as action param (flat action form).
             * Collect into act_lst so we can emit action(...) at the end. */
            has_type = has_type || (!strcmp(kname, "type") ? 1 : 0);
            yaml_event_t vev;
            rsRetVal next_ret = next_event(parser, &vev, fname);
            if (next_ret != RS_RET_OK) {
                free(kname);
                ABORT_FINALIZE(next_ret);
            }
            struct nvlst *node = NULL;
            rsRetVal nret = yaml_value_to_nvlst_node(parser, fname, &vev, kname, &node);
            free(kname);
            kname = NULL;
            if (nret != RS_RET_OK) ABORT_FINALIZE(nret);
            node->next = act_lst;
            act_lst = node;
        }
    } /* end mapping loop */

    /* ---- Generate RainerScript from collected fields ---- */

    if (has_if) {
        /* if (<expr>) then { ... } [else { ... }] */
        if (estr_append_cstr(s, "if (", 4) != 0 || estr_append_cstr(s, if_expr, strlen(if_expr)) != 0 ||
            estr_append_cstr(s, ") then {\n", 9) != 0)
            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

        /* then branch: either action_rs (from action: key) or then_rs (from then: key) */
        if (action_rs != NULL) {
            if (es_addStr(s, action_rs) != 0) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        } else if (then_rs != NULL) {
            if (es_addStr(s, then_rs) != 0) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        }

        if (estr_append_char(s, '}') != 0) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

        if (else_rs != NULL) {
            if (estr_append_cstr(s, " else {\n", 8) != 0 || es_addStr(s, else_rs) != 0 || estr_append_char(s, '}') != 0)
                ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        }
        if (estr_append_char(s, '\n') != 0) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

    } else if (has_stop) {
        if (estr_append_cstr(s, "stop\n", 5) != 0) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

    } else if (has_continue) {
        if (estr_append_cstr(s, "continue\n", 9) != 0) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

    } else if (has_call) {
        if (estr_append_cstr(s, "call ", 5) != 0 || estr_append_cstr(s, call_name, strlen(call_name)) != 0 ||
            estr_append_char(s, '\n') != 0)
            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

    } else if (has_call_indirect) {
        if (estr_append_cstr(s, "call_indirect ", 14) != 0 ||
            estr_append_cstr(s, call_indirect_expr, strlen(call_indirect_expr)) != 0 ||
            estr_append_cstr(s, ";\n", 2) != 0)
            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

    } else if (has_unset) {
        if (estr_append_cstr(s, "unset ", 6) != 0 || estr_append_cstr(s, unset_var, strlen(unset_var)) != 0 ||
            estr_append_cstr(s, ";\n", 2) != 0)
            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

    } else if (has_foreach) {
        if (foreach_var == NULL || foreach_in == NULL || foreach_body == NULL) {
            LogError(0, RS_RET_CONF_PARSE_ERROR, "yamlconf: %s: foreach: requires 'var', 'in', and 'do' keys", fname);
            ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
        }
        if (estr_append_cstr(s, "foreach (", 9) != 0 || estr_append_cstr(s, foreach_var, strlen(foreach_var)) != 0 ||
            estr_append_cstr(s, " in ", 4) != 0 || estr_append_cstr(s, foreach_in, strlen(foreach_in)) != 0 ||
            estr_append_cstr(s, ") do {\n", 7) != 0 || es_addStr(s, foreach_body) != 0 ||
            estr_append_cstr(s, "}\n", 2) != 0)
            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

    } else if (has_set) {
        if (set_var == NULL || set_expr == NULL) {
            LogError(0, RS_RET_CONF_PARSE_ERROR, "yamlconf: %s: set: requires both 'var' and 'expr' keys", fname);
            ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
        }
        if (estr_append_cstr(s, "set ", 4) != 0 || estr_append_cstr(s, set_var, strlen(set_var)) != 0 ||
            estr_append_cstr(s, " = ", 3) != 0 || estr_append_cstr(s, set_expr, strlen(set_expr)) != 0 ||
            estr_append_cstr(s, ";\n", 2) != 0)
            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

    } else if (has_type || act_lst != NULL) {
        /* Flat action: item with type: key directly at statement level */
        if (estr_append_cstr(s, "action(", 7) != 0 || append_nvlst_as_rs_params(s, act_lst) != 0 ||
            estr_append_cstr(s, ")\n", 2) != 0)
            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

    } else if (has_rlt) {
        /* Emit: reload_lookup_table("tableName"[, "stubValue"]) */
        if (estr_append_cstr(s, "reload_lookup_table(", 20) != 0 ||
            estr_append_quoted(s, rlt_table, strlen(rlt_table)) != 0)
            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        if (rlt_stub != NULL) {
            if (estr_append_cstr(s, ", ", 2) != 0 || estr_append_quoted(s, rlt_stub, strlen(rlt_stub)) != 0)
                ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        }
        if (estr_append_cstr(s, ")\n", 2) != 0) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

    } else {
        LogError(0, RS_RET_CONF_PARSE_ERROR,
                 "yamlconf: %s: unrecognised statement item in statements: "
                 "(expected if/type/stop/continue/call/call_indirect/unset/set/foreach/reload_lookup_table)",
                 fname);
        ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
    }

finalize_it:
    free(if_expr);
    free(call_name);
    free(call_indirect_expr);
    free(unset_var);
    free(foreach_var);
    free(foreach_in);
    free(set_var);
    free(set_expr);
    es_deleteStr(then_rs);
    es_deleteStr(else_rs);
    es_deleteStr(action_rs);
    es_deleteStr(foreach_body);
    nvlstDestruct(act_lst);
    free(rlt_table);
    free(rlt_stub);
    RETiRet;
}

/* Parse a statements: sequence and append the equivalent RainerScript to *s.
 * SEQUENCE_START must already be consumed by the caller.
 *
 * This function is RECURSIVE: build_one_stmt_rs() calls build_stmts_rs() for
 * then:/else: branch bodies, and parse_foreach_mapping() calls it for do:
 * bodies.  Recursion depth is bounded by YAML nesting depth which is shallow
 * in practice (foreach inside if is the deepest realistic case). */
static rsRetVal build_stmts_rs(yaml_parser_t *parser, es_str_t **s, const char *fname) {
    yaml_event_t ev;
    DEFiRet;

    while (1) {
        CHKiRet(next_event(parser, &ev, fname));
        if (ev.type == YAML_SEQUENCE_END_EVENT) {
            yaml_event_delete(&ev);
            break;
        }
        if (ev.type != YAML_MAPPING_START_EVENT) {
            LogError(0, RS_RET_CONF_PARSE_ERROR, "yamlconf: %s: each item in a statements: sequence must be a mapping",
                     fname);
            yaml_event_delete(&ev);
            ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
        }
        yaml_event_delete(&ev);
        CHKiRet(build_one_stmt_rs(parser, s, fname));
    }

finalize_it:
    RETiRet;
}

/* Parse the rulesets: sequence.
 *
 * Each item is a mapping with at least a "name" key.  The special "script"
 * key causes a complete  ruleset(params) { script }  RainerScript block to
 * be synthesised and pushed onto the flex buffer stack via
 * cnfAddConfigBuffer(); the ongoing yyparse() then processes it naturally.
 *
 * The "statements" key is the YAML-native alternative to "script".  It takes
 * a sequence of statement mappings (if/action/stop/continue/call/set) that are
 * translated to an equivalent RainerScript body and then processed via the
 * same cnfAddConfigBuffer() path.  Only the filter expression in "if:" remains
 * as a RainerScript expression string; action parameters and control-flow are
 * expressed as YAML keys.  script:/statements: and filter:/actions: are all
 * mutually exclusive.
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
static rsRetVal parse_ruleset_sequence(yaml_parser_t *parser, const char *fname) {
    yaml_event_t ev;
    /* Hoisted to function scope so finalize_it: can clean up on error */
    struct nvlst *lst = NULL;
    char *script_str = NULL;
    char *filter_str = NULL;
    struct cnfstmt *actions = NULL;
    char *kname = NULL;
    DEFiRet;

    while (1) {
        CHKiRet(next_event(parser, &ev, fname));
        if (ev.type == YAML_SEQUENCE_END_EVENT) {
            yaml_event_delete(&ev);
            break;
        }
        if (ev.type != YAML_MAPPING_START_EVENT) {
            LogError(0, RS_RET_CONF_PARSE_ERROR, "yamlconf: %s: rulesets sequence item must be a mapping", fname);
            yaml_event_delete(&ev);
            ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
        }
        yaml_event_delete(&ev);

        /* Reset per-ruleset state (all NULL at function entry or after prior iteration) */
        lst = NULL;
        script_str = NULL;
        filter_str = NULL;
        actions = NULL;

        /* We parse the mapping manually here so we can intercept special keys */
        while (1) {
            CHKiRet(next_event(parser, &ev, fname));
            if (ev.type == YAML_MAPPING_END_EVENT) {
                yaml_event_delete(&ev);
                break;
            }
            if (ev.type != YAML_SCALAR_EVENT) {
                LogError(0, RS_RET_CONF_PARSE_ERROR, "yamlconf: %s: expected scalar key in ruleset mapping", fname);
                yaml_event_delete(&ev);
                ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
            }

            free(kname);
            kname = strdup((char *)ev.data.scalar.value);
            yaml_event_delete(&ev);
            if (kname == NULL) {
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
                    ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
                }
                free(script_str);
                script_str = strdup((char *)ev.data.scalar.value);
                yaml_event_delete(&ev);
                if (script_str == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

            } else if (!strcmp(kname, "statements")) {
                /* statements: sequence — YAML-native scripting.
                 * Translates to RainerScript and stored in script_str. */
                if (ev.type != YAML_SEQUENCE_START_EVENT) {
                    LogError(0, RS_RET_CONF_PARSE_ERROR, "yamlconf: %s: 'statements' value must be a sequence", fname);
                    yaml_event_delete(&ev);
                    ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
                }
                yaml_event_delete(&ev);
                es_str_t *stmts_estr = es_newStr(256);
                if (stmts_estr == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
                rsRetVal sret = build_stmts_rs(parser, &stmts_estr, fname);
                if (sret != RS_RET_OK) {
                    es_deleteStr(stmts_estr);
                    ABORT_FINALIZE(sret);
                }
                free(script_str);
                script_str = es_str2cstr(stmts_estr, NULL);
                es_deleteStr(stmts_estr);
                if (script_str == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

            } else if (!strcmp(kname, "filter")) {
                /* filter: "<pri-filter>" or ":<property-filter>" */
                if (ev.type != YAML_SCALAR_EVENT) {
                    LogError(0, RS_RET_CONF_PARSE_ERROR, "yamlconf: %s: 'filter' value must be a scalar string", fname);
                    yaml_event_delete(&ev);
                    ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
                }
                free(filter_str);
                filter_str = strdup((char *)ev.data.scalar.value);
                yaml_event_delete(&ev);
                if (filter_str == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
            } else if (!strcmp(kname, "actions")) {
                /* actions: [ {type: ..., ...}, ... ] */
                if (ev.type != YAML_SEQUENCE_START_EVENT) {
                    LogError(0, RS_RET_CONF_PARSE_ERROR, "yamlconf: %s: 'actions' value must be a sequence", fname);
                    yaml_event_delete(&ev);
                    ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
                }
                yaml_event_delete(&ev);
                if (actions) {
                    cnfstmtDestructLst(actions);
                    actions = NULL;
                }
                CHKiRet(parse_actions_sequence(parser, &actions, fname));
            } else {
                /* All other keys go into the header nvlst */
                struct nvlst *node = NULL;
                CHKiRet(yaml_value_to_nvlst_node(parser, fname, &ev, kname, &node));
                node->next = lst;
                lst = node;
            }
        } /* end per-ruleset key loop */

        /* Sanity: script:/statements: and actions: are mutually exclusive */
        if (script_str != NULL && actions != NULL) {
            LogError(0, RS_RET_CONF_PARSE_ERROR,
                     "yamlconf: %s: ruleset cannot have both 'script'/'statements' and 'actions';"
                     " use one or the other",
                     fname);
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
            lst = NULL; /* cnfobjNew takes ownership */
            if (obj == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
            cnfDoObj(obj);
        }
    } /* end ruleset sequence loop */

finalize_it:
    free(kname);
    free(script_str);
    free(filter_str);
    if (actions != NULL) cnfstmtDestructLst(actions);
    nvlstDestruct(lst);
    RETiRet;
}

/* Parse the include: sequence and trigger file inclusion.
 * Each item is a mapping with "path" (required) and "optional" (optional).
 *
 * Processing order matters because the flex buffer stack is LIFO:
 *   - .yaml/.yml paths: cnfDoInclude() → yamlconf_load() immediately and
 *     recursively, so they must be processed in forward (document) order.
 *   - .conf / other paths: cnfDoInclude() → cnfSetLexFile() which pushes
 *     onto the LIFO flex buffer stack.  To ensure the bison parser later
 *     sees them in document order (A then B then C), we must push them in
 *     REVERSE order (C, B, A → stack top to bottom A, B, C → pop A first).
 *
 * We therefore collect all items first, then do a forward pass for YAML
 * files and a reverse pass for non-YAML files.
 *
 * ORDERING LIMITATION: when the include: list mixes .yaml and non-YAML files
 * (e.g. [a.conf, b.yaml, c.conf]), YAML files are always processed before
 * non-YAML files, regardless of their relative position in the document.
 * This is a fundamental architectural constraint — the LIFO flex buffer stack
 * cannot interleave with synchronous YAML loads.  A warning is emitted when
 * such a mixed list is detected.  If strict order is required, use separate
 * include: blocks or consolidate to a single file type. */
static rsRetVal parse_include_sequence(yaml_parser_t *parser, const char *fname) {
    yaml_event_t ev;
    char *k = NULL;
    char *path = NULL; /* current item path; function-scope so finalize_it can free it */
    /* Dynamic array of collected items */
    struct inc_item {
        char *path;
        int optional;
    } *items = NULL;
    int nitems = 0, cap = 0;
    DEFiRet;

    /* ---- Phase 1: collect all (path, optional) pairs ---- */
    while (1) {
        free(path); /* free any leftover from previous iteration */
        path = NULL;
        CHKiRet(next_event(parser, &ev, fname));
        if (ev.type == YAML_SEQUENCE_END_EVENT) {
            yaml_event_delete(&ev);
            break;
        }
        if (ev.type != YAML_MAPPING_START_EVENT) {
            LogError(0, RS_RET_CONF_PARSE_ERROR, "yamlconf: %s: include sequence item must be a mapping", fname);
            yaml_event_delete(&ev);
            ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
        }
        yaml_event_delete(&ev);

        int optional = 0;

        while (1) {
            CHKiRet(next_event(parser, &ev, fname));
            if (ev.type == YAML_MAPPING_END_EVENT) {
                yaml_event_delete(&ev);
                break;
            }
            if (ev.type != YAML_SCALAR_EVENT) {
                yaml_event_delete(&ev);
                ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
            }
            k = strdup((char *)ev.data.scalar.value);
            yaml_event_delete(&ev);
            if (k == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

            yaml_event_t vev;
            CHKiRet(next_event(parser, &vev, fname));
            if (vev.type == YAML_SCALAR_EVENT) {
                const char *v = (char *)vev.data.scalar.value;
                if (!strcmp(k, "path")) {
                    free(path);
                    path = strdup(v);
                } else if (!strcmp(k, "optional")) {
                    optional = (!strcasecmp(v, "on") || !strcasecmp(v, "yes") || !strcmp(v, "1")) ? 1 : 0;
                }
                yaml_event_delete(&vev);
            } else {
                /* Non-scalar value for an include key is a config error.
                 * Consume the nested structure so the parser stays in sync. */
                LogError(0, RS_RET_CONF_PARSE_ERROR,
                         "yamlconf: %s: include item key '%s' must have a scalar value, skipping", fname, k);
                if (vev.type == YAML_MAPPING_START_EVENT || vev.type == YAML_SEQUENCE_START_EVENT) {
                    yaml_event_delete(&vev);
                    skip_node(parser);
                } else {
                    yaml_event_delete(&vev);
                }
            }
            free(k);
            k = NULL;
        }

        if (path != NULL) {
            if (nitems >= cap) {
                int newcap = cap == 0 ? 8 : cap * 2;
                struct inc_item *tmp = realloc(items, newcap * sizeof(*items));
                if (tmp == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
                items = tmp;
                cap = newcap;
            }
            items[nitems].path = path;
            path = NULL; /* ownership transferred to items[]; finalize_it must not free */
            items[nitems].optional = optional;
            nitems++;
        } else {
            LogError(0, RS_RET_CONF_PARSE_ERROR, "yamlconf: %s: include item missing required 'path' key", fname);
            /* non-fatal: skip this item */
        }
    }

    /* ---- Phase 2: warn on mixed list, then forward pass for YAML files ---- */
    /* Detect mixed .yaml / non-YAML to warn about ordering limitation. */
    int has_yaml_item = 0, has_conf_item = 0;
    for (int i = 0; i < nitems; i++) {
        const char *ext = strrchr(items[i].path, '.');
        if (ext != NULL && (!strcmp(ext, ".yaml") || !strcmp(ext, ".yml")))
            has_yaml_item = 1;
        else
            has_conf_item = 1;
    }
    if (has_yaml_item && has_conf_item) {
        LogError(0, RS_RET_OK,
                 "yamlconf: %s: include: list mixes .yaml and non-YAML files; "
                 "YAML files are loaded first regardless of document order — "
                 "use separate include: blocks if strict ordering is required",
                 fname);
    }

    for (int i = 0; i < nitems; i++) {
        const char *ext = strrchr(items[i].path, '.');
        int is_yaml = (ext != NULL && (!strcmp(ext, ".yaml") || !strcmp(ext, ".yml")));
        if (!is_yaml) continue;
        int iret = cnfDoInclude(items[i].path, items[i].optional);
        if (iret != 0 && !items[i].optional) {
            LogError(0, RS_RET_CONF_PARSE_ERROR, "yamlconf: %s: include of '%s' failed (non-optional)", fname,
                     items[i].path);
            ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
        }
    }

    /* ---- Phase 3: reverse pass — non-YAML files → LIFO stack ---- */
    /* Pushing in reverse order ensures the bison parser processes them in
     * the same forward (document) order as they appear in the YAML list. */
    for (int i = nitems - 1; i >= 0; i--) {
        const char *ext = strrchr(items[i].path, '.');
        int is_yaml = (ext != NULL && (!strcmp(ext, ".yaml") || !strcmp(ext, ".yml")));
        if (is_yaml) continue;
        int iret = cnfDoInclude(items[i].path, items[i].optional);
        if (iret != 0 && !items[i].optional) {
            LogError(0, RS_RET_CONF_PARSE_ERROR, "yamlconf: %s: include of '%s' failed (non-optional)", fname,
                     items[i].path);
            ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
        }
    }

finalize_it:
    free(k);
    free(path); /* NULL-safe; only non-NULL if ABORT_FINALIZE before item was stored */
    for (int i = 0; i < nitems; i++) free(items[i].path);
    free(items);
    RETiRet;
}

/* --------------------------------------------------------------------------
 * Top-level dispatcher
 * -------------------------------------------------------------------------- */

/* Map a top-level YAML key to its cnfobjType and call the right parser.
 * The opening event for the section value (MAPPING_START or SEQUENCE_START)
 * has already been consumed when this function is called. */
static rsRetVal process_top_level(yaml_parser_t *parser, const char *key, const char *fname) {
    DEFiRet;

    if (!strcmp(key, "global")) {
        CHKiRet(parse_singleton_obj(parser, CNFOBJ_GLOBAL, fname));
    } else if (!strcmp(key, "mainqueue") || !strcmp(key, "main_queue")) {
        CHKiRet(parse_singleton_obj(parser, CNFOBJ_MAINQ, fname));
    } else if (!strcmp(key, "modules") || !strcmp(key, "testbench_modules")) {
        CHKiRet(parse_obj_sequence(parser, CNFOBJ_MODULE, fname));
    } else if (!strcmp(key, "inputs")) {
        CHKiRet(parse_obj_sequence(parser, CNFOBJ_INPUT, fname));
    } else if (!strcmp(key, "templates")) {
        CHKiRet(parse_template_sequence(parser, fname));
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
        if (next_event(parser, &ev, fname) == RS_RET_OK) yaml_event_delete(&ev);
    } else {
        LogError(0, RS_RET_CONF_PARSE_ERROR, "yamlconf: %s: unknown top-level key '%s' – ignoring", fname, key);
        /* Skip the value node so the parser stays in sync */
        yaml_event_t ev;
        if (next_event(parser, &ev, fname) == RS_RET_OK) {
            if (ev.type == YAML_MAPPING_START_EVENT || ev.type == YAML_SEQUENCE_START_EVENT) {
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

/**
 * @brief Load and process a YAML rsyslog configuration file.
 *
 * Opens @p fname, drives the libyaml event parser over the top-level
 * mapping, and dispatches each section to the appropriate handler.
 * All resulting config objects are committed to rsconf via cnfDoObj() or
 * cnfAddConfigBuffer() before this function returns.
 *
 * @param fname  Absolute or relative path to the .yaml/.yml config file.
 * @return RS_RET_OK on success; RS_RET_FILE_NOT_FOUND if @p fname cannot
 *         be opened; RS_RET_CONF_PARSE_ERROR on any YAML or semantic error.
 */
rsRetVal yamlconf_load(const char *fname) {
    yaml_parser_t parser;
    yaml_event_t ev;
    int parserInit = 0;
    FILE *fh = NULL;
    #define YAMLCONF_MAX_TOPKEYS 32
    char *seen_keys[YAMLCONF_MAX_TOPKEYS];
    int seen_count = 0;
    DEFiRet;

    fh = fopen(fname, "r");
    if (fh == NULL) {
        LogError(errno, RS_RET_CONF_FILE_NOT_FOUND, "yamlconf: cannot open config file '%s'", fname);
        ABORT_FINALIZE(RS_RET_CONF_FILE_NOT_FOUND);
    }

    if (!yaml_parser_initialize(&parser)) {
        LogError(0, RS_RET_INTERNAL_ERROR, "yamlconf: failed to initialize YAML parser");
        ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
    }
    parserInit = 1;
    yaml_parser_set_input_file(&parser, fh);

    /* Consume stream/document start */
    CHKiRet(expect_event(&parser, &ev, YAML_STREAM_START_EVENT, "stream start", fname));
    yaml_event_delete(&ev);
    CHKiRet(expect_event(&parser, &ev, YAML_DOCUMENT_START_EVENT, "document start", fname));
    yaml_event_delete(&ev);

    /* The top-level node must be a mapping */
    CHKiRet(expect_event(&parser, &ev, YAML_MAPPING_START_EVENT, "top-level mapping", fname));
    yaml_event_delete(&ev);

    /* Track seen top-level keys to warn on duplicates.  Duplicate keys are
     * undefined behaviour in the YAML spec and unsupported by rsyslog; use
     * sequences for multiple items or include: for file-level composition. */

    /* Walk the top-level key/value pairs */
    while (1) {
        CHKiRet(next_event(&parser, &ev, fname));

        if (ev.type == YAML_MAPPING_END_EVENT) {
            yaml_event_delete(&ev);
            break;
        }
        if (ev.type != YAML_SCALAR_EVENT) {
            LogError(0, RS_RET_CONF_PARSE_ERROR, "yamlconf: %s: expected scalar key at top level", fname);
            yaml_event_delete(&ev);
            ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
        }

        char *topkey = strdup((char *)ev.data.scalar.value);
        yaml_event_delete(&ev);
        if (topkey == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

        /* Warn on duplicate top-level key. */
        for (int ki = 0; ki < seen_count; ++ki) {
            if (!strcmp(seen_keys[ki], topkey)) {
                LogError(0, RS_RET_CONF_PARSE_ERROR,
                         "yamlconf: %s: duplicate top-level key '%s' - "
                         "this is unsupported; use sequences for multiple items "
                         "or include: for file-level composition",
                         fname, topkey);
                break;
            }
        }
        int topkey_owned = 0; /* 1 if seen_keys owns topkey, 0 if we must free it */
        if (seen_count < YAMLCONF_MAX_TOPKEYS) {
            seen_keys[seen_count++] = topkey; /* ownership transferred to seen_keys */
            topkey_owned = 1;
        }
        /* else: array full (>32 distinct top-level keys — highly unusual);
         * duplicate detection skipped for this key; topkey freed below. */

        /* Consume the opening event of the value node before dispatching */
        yaml_event_t vev;
        rsRetVal vret = next_event(&parser, &vev, fname);
        if (vret != RS_RET_OK) {
            if (!topkey_owned) free(topkey);
            ABORT_FINALIZE(vret);
        }

        /* For sections that expect a sequence we need SEQUENCE_START;
         * for singleton sections we need MAPPING_START.
         * process_top_level() takes responsibility from here, but we already
         * consumed the opening event – so verify it makes sense. */
        if (vev.type != YAML_MAPPING_START_EVENT && vev.type != YAML_SEQUENCE_START_EVENT &&
            vev.type != YAML_SCALAR_EVENT) {
            LogError(0, RS_RET_CONF_PARSE_ERROR, "yamlconf: %s: unexpected value type for key '%s'", fname, topkey);
            yaml_event_delete(&vev);
            if (!topkey_owned) free(topkey);
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
                         " a mapping or sequence",
                         fname, topkey);
                /* non-fatal */
            }
            yaml_event_delete(&vev);
            if (!topkey_owned) free(topkey);
            continue;
        }

        yaml_event_delete(&vev);
        /* process_top_level() will now call the right parser which will consume
         * the rest of the value node (up to and including its END event). */
        rsRetVal pret = process_top_level(&parser, topkey, fname);
        if (!topkey_owned) free(topkey);
        if (pret != RS_RET_OK) ABORT_FINALIZE(pret);
    }

finalize_it:
    for (int ki = 0; ki < seen_count; ++ki) free(seen_keys[ki]);
    if (parserInit) yaml_parser_delete(&parser);
    if (fh) fclose(fh);
    RETiRet;
}

#endif /* HAVE_LIBYAML */
