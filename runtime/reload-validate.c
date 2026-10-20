/* SPDX-License-Identifier: Apache-2.0
 * Validate-only HUP recovery checkpoint.
 *
 * Concurrency & Locking:
 *   All state belongs to the main thread. Startup observes borrowed syntax
 *   before dispatch; HUP privately parses after legacy hooks. Workers never
 *   read this state. There is no runtime-object load, activation or mutation.
 */
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <stdint.h>
#include "rsyslog.h"
#include "errmsg.h"
#include "grammar/rainerscript.h"
#include "grammar/parserif.h"
#include "reload-candidate.h"
#include "reload-validate.h"

static int selectedMode = -1; /* -1 implicit OFF, 0 explicit OFF, 1 validate */
static int modeError;
static int sourceSeen;
static int sourceIncomplete;
static int modeSelectedLate;
static int startupFinished;
static char *candidatePath;
static char startupDirectory[PATH_MAX];
static int candidateModeSeen;
static rsReloadNormalizedGraphBuilderV1_t *activeBuilder;

typedef struct graphRows_s {
    rsReloadNormalizedNodeV1_t *rows;
    size_t count;
    size_t capacity;
} graphRows_t;
static graphRows_t activeRows;

static int equalWord(const es_str_t *value, const char *word) {
    size_t i;
    const size_t length = strlen(word);
    if (value == NULL || es_strlen((es_str_t *)value) != length) return 0;
    const unsigned char *data = es_getBufAddr((es_str_t *)value);
    for (i = 0; i < length; ++i) {
        const unsigned char c = data[i] >= 'A' && data[i] <= 'Z' ? data[i] + ('a' - 'A') : data[i];
        if (c != (unsigned char)word[i]) return 0;
    }
    return 1;
}

void rsReloadValidateObserveOther(void) {
    if (!startupFinished) sourceSeen = 1;
}

static void checkMode(const struct cnfobj *object, const int candidate) {
    if (object->objType != CNFOBJ_GLOBAL) return;
    for (const struct nvlst *entry = object->nvlst; entry != NULL; entry = entry->next) {
        if (!equalWord(entry->name, "config.reloadonhup")) continue;
        const int mode = entry->val.datatype != 'S'                 ? -1
                         : equalWord(entry->val.d.estr, "off")      ? 0
                         : equalWord(entry->val.d.estr, "validate") ? 1
                                                                    : -1;
        if (mode < 0) {
            parser_errmsg("config.reloadOnHUP supports only off or validate; on is not implemented%s",
                          candidate ? "" : "; using off mode");
            if (!candidate) modeError = 1;
        } else if (candidate) {
            candidateModeSeen = 1;
            if (mode != selectedMode)
                parser_errmsg("config.reloadOnHUP mode change requires restart; startup-selected mode is fixed");
        } else if (selectedMode >= 0 && selectedMode != mode) {
            parser_errmsg("conflicting config.reloadOnHUP settings; select one startup mode");
            modeError = 1;
        } else if (selectedMode < 0) {
            selectedMode = mode;
            if (mode == 1) {
                sourceIncomplete = sourceSeen;
                modeSelectedLate = sourceSeen;
                if (rsReloadCandidateSourceBegin() != RS_RET_OK) sourceIncomplete = 1;
            }
        }
    }
}

void rsReloadValidateObserveObject(const struct cnfobj *object) {
    if (startupFinished) return;
    checkMode(object, 0);
    sourceSeen = 1;
}

void rsReloadValidateCandidateBegin(void) {
    candidateModeSeen = 0;
}

void rsReloadValidateCandidateFinish(void) {
    if (selectedMode == 1 && !candidateModeSeen)
        parser_errmsg("removing config.reloadOnHUP selects off on startup; mode change requires restart");
}

char *rsReloadValidateResolvePath(const char *path) {
    if (path[0] == '/' || startupDirectory[0] == '\0') return strdup(path);
    const size_t length = strlen(startupDirectory) + strlen(path) + 2;
    char *result = malloc(length);
    if (result != NULL) snprintf(result, length, "%s/%s", startupDirectory, path);
    return result;
}

void rsReloadValidateCandidateObject(const struct cnfobj *object) {
    checkMode(object, 1);
}

static rsRetVal collectRow(const rsReloadNormalizedNodeV1_t *node, void *context) {
    graphRows_t *graph = context;
    if (node == NULL || node->identity == NULL || node->fingerprint == NULL) return RS_RET_PARAM_ERROR;
    if (graph->count == graph->capacity) {
        const size_t capacity = graph->capacity == 0 ? 16 : graph->capacity * 2;
        if (capacity < graph->capacity || capacity > SIZE_MAX / sizeof(*graph->rows)) return RS_RET_OUT_OF_MEMORY;
        void *rows = realloc(graph->rows, capacity * sizeof(*graph->rows));
        if (rows == NULL) return RS_RET_OUT_OF_MEMORY;
        graph->rows = rows;
        graph->capacity = capacity;
    }
    if (graph->count != 0) {
        const rsReloadNormalizedNodeV1_t *last = &graph->rows[graph->count - 1];
        if (last->objectKind == node->objectKind && !strcmp(last->identity, node->identity))
            return RS_RET_CONF_PARSE_ERROR;
    }
    graph->rows[graph->count++] = *node; /* graph builder retains string ownership */
    return RS_RET_OK;
}

static rsRetVal collectGraph(rsReloadNormalizedGraphBuilderV1_t *builder, graphRows_t *rows) {
    rsReloadNormalizedGraphV1_t graph;
    rsRetVal ret = rsReloadNormalizedGraphBuilderV1GetGraph(builder, &graph);
    return ret == RS_RET_OK ? graph.enumerate(graph.context, collectRow, rows) : ret;
}

rsRetVal rsReloadValidateStartupFinish(const char *path, const rsRetVal loadResult, const int unclean) {
    startupFinished = 1;
    if (modeError) {
        rsReloadCandidateSourceAbort();
        selectedMode = 0;
        return RS_RET_OK;
    }
    if (selectedMode != 1) return RS_RET_OK;
    if (modeSelectedLate)
        LogMsg(0, RS_RET_OK, LOG_WARNING,
               "config.reloadOnHUP=\"validate\" was seen too late; HUP analysis will report incomplete");
    if (loadResult != RS_RET_OK || unclean) sourceIncomplete = 1;
    if (rsReloadCandidateSourceFinish(&activeBuilder) != RS_RET_OK) sourceIncomplete = 1;
    if (!sourceIncomplete && collectGraph(activeBuilder, &activeRows) != RS_RET_OK) sourceIncomplete = 1;
    if (getcwd(startupDirectory, sizeof(startupDirectory)) == NULL) {
        sourceIncomplete = 1;
        startupDirectory[0] = '\0';
    } else {
        candidatePath = rsReloadValidateResolvePath(path);
    }
    if (candidatePath == NULL) sourceIncomplete = 1;
    return RS_RET_OK;
}

static const char *objectKindName(const rsReloadObjectKind_t kind) {
    switch (kind) {
        case RS_RELOAD_OBJ_GLOBAL:
            return "global";
        case RS_RELOAD_OBJ_MODULE:
            return "module";
        case RS_RELOAD_OBJ_INPUT:
            return "input";
        case RS_RELOAD_OBJ_RULESET:
            return "ruleset";
        case RS_RELOAD_OBJ_TEMPLATE:
            return "template";
        case RS_RELOAD_OBJ_ACTION:
            return "action";
        case RS_RELOAD_OBJ_PARSER:
            return "parser";
        case RS_RELOAD_OBJ_MAIN_QUEUE:
            return "main_queue";
        case RS_RELOAD_OBJ_TIMEZONE:
            return "timezone";
        case RS_RELOAD_OBJ_LOOKUP_TABLE:
            return "lookup_table";
        case RS_RELOAD_OBJ_DYN_STATS:
            return "dyn_stats";
        case RS_RELOAD_OBJ_PERCTILE_STATS:
            return "perctile_stats";
        case RS_RELOAD_OBJ_RATELIMIT:
            return "ratelimit";
        case RS_RELOAD_OBJ_PROPERTY:
            return "property";
        case RS_RELOAD_OBJ_CONSTANT:
            return "constant";
        case RS_RELOAD_OBJ_LEGACY_OPAQUE:
            return "legacy_opaque";
        case RS_RELOAD_OBJ_OPAQUE:
            return "opaque";
        case RS_RELOAD_OBJ_COUNT:
        default:
            return "unknown";
    }
}

/* Canonical identities identify affected instances without printing values.
 * Bound and flatten user-controlled names so each diagnostic is one record. */
static void reportChange(const char *change, const rsReloadNormalizedNodeV1_t *node) {
    char identity[256];
    size_t i;
    for (i = 0; i + 1 < sizeof(identity) && node->identity[i] != '\0'; ++i) {
        const unsigned char c = node->identity[i];
        identity[i] = c < 32 || c == 127 ? '?' : c;
    }
    identity[i] = '\0';
    LogMsg(0, RS_RET_OK, LOG_INFO,
           "reload_validate change=%s instance=%s kind=%s reason=activation_not_implemented activated=0", change,
           identity, objectKindName(node->objectKind));
}

void rsReloadValidateHUP(void) {
    rsReloadCandidate_t *candidate = NULL;
    rsReloadNormalizedGraphBuilderV1_t *builder = NULL;
    graphRows_t rows = {0};
    size_t old = 0, next = 0, added = 0, removed = 0, modified = 0, unchanged = 0;
    rsRetVal ret;
    if (selectedMode != 1) return;
    if (candidatePath == NULL) {
        LogMsg(0, RS_RET_OK, LOG_INFO,
               "reload_validate result=incomplete activated=0 cause=candidate_path_unavailable");
        return;
    }
    ret = rsReloadCandidateParse(candidatePath, &candidate);
    if (ret != RS_RET_OK) {
        LogMsg(0, RS_RET_OK, LOG_INFO, "reload_validate result=rejected activated=0 cause=private_parse_failed code=%d",
               ret);
        goto done;
    }
    ret = rsReloadCandidateBuildNormalizedGraphV1(candidate, &builder);
    if (ret == RS_RET_OK) ret = collectGraph(builder, &rows);
    if (ret != RS_RET_OK) {
        LogMsg(0, RS_RET_OK, LOG_INFO,
               "reload_validate result=incomplete activated=0 cause=unsupported_syntax_or_identity code=%d", ret);
        goto done;
    }
    if (sourceIncomplete) {
        LogMsg(0, RS_RET_OK, LOG_INFO, "reload_validate result=incomplete activated=0 cause=active_source_incomplete");
        goto done;
    }
    while (old < activeRows.count || next < rows.count) {
        int cmp;
        if (old == activeRows.count)
            cmp = 1;
        else if (next == rows.count)
            cmp = -1;
        else {
            cmp = (int)activeRows.rows[old].objectKind - (int)rows.rows[next].objectKind;
            if (cmp == 0) cmp = strcmp(activeRows.rows[old].identity, rows.rows[next].identity);
        }
        if (cmp < 0) {
            reportChange("removed", &activeRows.rows[old++]);
            ++removed;
        } else if (cmp > 0) {
            reportChange("added", &rows.rows[next++]);
            ++added;
        } else {
            if (strcmp(activeRows.rows[old].fingerprint, rows.rows[next].fingerprint)) {
                reportChange("modified", &rows.rows[next]);
                ++modified;
            } else
                ++unchanged;
            ++old;
            ++next;
        }
    }
    LogMsg(0, RS_RET_OK, LOG_INFO,
           "reload_validate result=structurally_compared activated=0 analysis=structural_only added=%zu removed=%zu "
           "modified=%zu unchanged=%zu limitation=module_parameters_and_runtime_semantics_not_checked",
           added, removed, modified, unchanged);
done:
    free(rows.rows);
    rsReloadNormalizedGraphBuilderV1Destruct(&builder);
    rsReloadCandidateDestruct(&candidate);
}

void rsReloadValidateDestruct(void) {
    rsReloadCandidateSourceAbort();
    rsReloadNormalizedGraphBuilderV1Destruct(&activeBuilder);
    free(activeRows.rows);
    memset(&activeRows, 0, sizeof(activeRows));
    free(candidatePath);
    candidatePath = NULL;
}
