/* SPDX-License-Identifier: Apache-2.0
 * Unit coverage for the private reload-candidate graph producer. The oracle
 * checks structural-only graph behavior without parsing, modules, a daemon,
 * or activation: default fragments merge, ruleset identities fold, secrets
 * never appear in digests, duplicate parameter keys fail, and enumeration is
 * deterministic; fixed imtcp endpoints are canonical and collision-safe.
 * An isolated elseif-marker mutation must change production
 * candidate serialization; restoring it must restore byte-identical output.
 * Ruleset-only report admission rejects NULL storage, misaligned bases/strides,
 * undersized strides, size overflow and address wrap before reading entries;
 * a normally built report remains accepted.
 * Named ruleset metadata has a separate row: source/candidate agreement,
 * case/order equivalence and metadata-only or combined edit rejection retain
 * supported program-only changes and duplicate-key failure.
 * Property-filter poison else slots are never traversed; effective operation,
 * negation and then-program changes remain visible in graph fingerprints.
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rsyslog.h"
#include "grammar/rainerscript.h"
#include "reload-normalized-graph.h"

#include "../../runtime/reload-normalized-graph.c"
#include "../../runtime/module-reload.c"
#include "../../runtime/reload-report.c"
#include "../../runtime/reload-candidate.c"

#define CHECK(condition)                                                                    \
    do {                                                                                    \
        if (!(condition)) {                                                                 \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            return 1;                                                                       \
        }                                                                                   \
    } while (0)

static int constructionFailed;
static int parserResult = 1;
static rsRetVal parserCaptureError = RS_RET_OK;
static rsRetVal parserRequestedFatalError = RS_RET_OK;
static rsRetVal parserFatalError = RS_RET_OK;

/* Parser-facing stubs let the unit drive classification without bringing up
 * either frontend. */
int cnfParse(void) {
    if (parserCaptureError != RS_RET_OK) rsReloadCandidateNoteError(parserCaptureError);
    parserFatalError = parserRequestedFatalError;
    return parserResult;
}
int cnfSetLexFile(const char __attribute__((unused)) * path) {
    return 0;
}
int cnfHasPendingBuffers(void) {
    return 0;
}
void cnfResetParser(void) {}
void cnfClearFatalParseError(void) {
    parserFatalError = RS_RET_OK;
}
rsRetVal cnfTakeFatalParseError(void) {
    const rsRetVal error = parserFatalError;
    parserFatalError = RS_RET_OK;
    return error;
}
rsRetVal yamlconf_load(const char __attribute__((unused)) * path) {
    return RS_RET_CONF_PARSE_ERROR;
}

const char *cnfobjType2str(const enum cnfobjType type) {
    switch (type) {
        case CNFOBJ_GLOBAL:
            return "global";
        case CNFOBJ_RULESET:
            return "ruleset";
        case CNFOBJ_INPUT:
            return "input";
        case CNFOBJ_ACTION:
            return "action";
        case CNFOBJ_MODULE:
            return "module";
        case CNFOBJ_TPL:
            return "template";
        case CNFOBJ_MAINQ:
            return "main_queue";
        case CNFOBJ_PROPERTY:
        case CNFOBJ_CONSTANT:
        case CNFOBJ_LOOKUP_TABLE:
        case CNFOBJ_PARSER:
        case CNFOBJ_TIMEZONE:
        case CNFOBJ_DYN_STATS:
        case CNFOBJ_PERCTILE_STATS:
        case CNFOBJ_RATELIMIT:
        default:
            return "opaque";
    }
}

struct cnfobj *cnfobjNew(enum cnfobjType type, struct nvlst *list) {
    struct cnfobj *object = calloc(1, sizeof(*object));
    if (object != NULL) {
        object->objType = type;
        object->nvlst = list;
    }
    return object;
}

struct nvlst *nvlstNewStr(es_str_t *value) {
    struct nvlst *node = calloc(1, sizeof(*node));
    if (node != NULL) {
        node->val.datatype = 'S';
        node->val.d.estr = value;
    }
    return node;
}

static struct nvlst *parameter(const char *name, const char *value);

void nvlstDestruct(struct nvlst *list) {
    while (list != NULL) {
        struct nvlst *next = list->next;
        es_deleteStr(list->name);
        if (list->val.datatype == 'S') es_deleteStr(list->val.d.estr);
        free(list);
        list = next;
    }
}

struct nvlst *rsconfTranslateCloneNvlst(const struct nvlst *list) {
    struct nvlst *head = NULL;
    struct nvlst *tail = NULL;

    for (; list != NULL; list = list->next) {
        struct nvlst *copy;
        es_str_t *value;
        if (list->val.datatype != 'S' || list->name == NULL || list->val.d.estr == NULL) {
            nvlstDestruct(head);
            return NULL;
        }
        value = es_strdup(list->val.d.estr);
        if (value == NULL) {
            nvlstDestruct(head);
            return NULL;
        }
        copy = nvlstNewStr(value);
        if (copy == NULL) es_deleteStr(value);
        if (copy == NULL || (copy->name = es_strdup(list->name)) == NULL) {
            nvlstDestruct(copy);
            nvlstDestruct(head);
            return NULL;
        }
        if (tail == NULL)
            head = copy;
        else
            tail->next = copy;
        tail = copy;
    }
    return head;
}

rsRetVal nvlstCloneReloadSafe(const struct nvlst *const source, struct nvlst **const output) {
    if (output == NULL || *output != NULL) return RS_RET_PARAM_ERROR;
    *output = rsconfTranslateCloneNvlst(source);
    return source != NULL && *output == NULL ? RS_RET_OUT_OF_MEMORY : RS_RET_OK;
}

void cnfstmtDestructLst(struct cnfstmt *list) {
    while (list != NULL) {
        struct cnfstmt *next = list->next;
        if (list->nodetype == S_RELOAD_ACT)
            nvlstDestruct(list->d.reload_action);
        else if (list->nodetype == S_RELOAD_PRIFILT) {
            cnfstmtDestructLst(list->d.s_prifilt.t_then);
            cnfstmtDestructLst(list->d.s_prifilt.t_else);
        } else if (list->nodetype == S_RELOAD_PROPFILT) {
            cnfstmtDestructLst(list->d.s_propfilt.t_then);
        }
        free(list->printable);
        free(list);
        list = next;
    }
}

void cnfobjDestruct(struct cnfobj *object) {
    if (object == NULL) return;
    nvlstDestruct(object->nvlst);
    free(object);
}

static struct nvlst *parameter(const char *name, const char *value) {
    struct nvlst *node = nvlstNewStr(es_newStrFromCStr(value, strlen(value)));
    if (node == NULL) {
        constructionFailed = 1;
        return NULL;
    }
    node->name = es_newStrFromCStr(name, strlen(name));
    if (node->name == NULL) constructionFailed = 1;
    return node;
}

static struct cnfobj *object(enum cnfobjType type, struct nvlst *parameters, struct cnfstmt *script) {
    struct cnfobj *result = calloc(1, sizeof(*result));
    if (result == NULL) {
        constructionFailed = 1;
        return NULL;
    }
    result->objType = type;
    result->nvlst = parameters;
    result->script = script;
    return result;
}

static struct cnfstmt *action(const char *name, const char *secret) {
    struct cnfstmt *result = calloc(1, sizeof(*result));
    struct nvlst *const head = parameter("type", "omfile");
    struct nvlst *tail = head;
    if (result == NULL || head == NULL) {
        constructionFailed = 1;
        nvlstDestruct(head);
        free(result);
        return NULL;
    }
    if (name != NULL) {
        tail->next = parameter("name", name);
        if (tail->next == NULL) {
            constructionFailed = 1;
            nvlstDestruct(head);
            free(result);
            return NULL;
        }
        tail = tail->next;
    }
    tail->next = parameter("password", secret);
    if (tail->next == NULL) {
        constructionFailed = 1;
        nvlstDestruct(head);
        free(result);
        return NULL;
    }
    result->nodetype = S_RELOAD_ACT;
    result->d.reload_action = head;
    return result;
}

static struct cnfstmt *propertyFilter(struct cnfstmt *child) {
    struct cnfstmt *result = calloc(1, sizeof(*result));
    if (result == NULL || child == NULL) {
        constructionFailed = 1;
        free(result);
        cnfstmtDestructLst(child);
        return NULL;
    }
    result->printable = (uchar *)strdup(":msg, contains, \"needle\"");
    if (result->printable == NULL) {
        constructionFailed = 1;
        free(result);
        cnfstmtDestructLst(child);
        return NULL;
    }
    result->nodetype = S_RELOAD_PROPFILT;
    result->d.s_propfilt.t_then = child;
    return result;
}

static struct cnfstmt *propertyFilterBranches(struct cnfstmt *thenBranch, struct cnfstmt *elseBranch) {
    struct cnfstmt *result = propertyFilter(thenBranch);

    if (result == NULL) {
        cnfstmtDestructLst(elseBranch);
        return NULL;
    }
    cnfstmtDestructLst(elseBranch);
    /* This deliberately synthetic poison tests semantic independence from
     * the unused slot; the S4 production constructor initializes it to NULL. */
    result->d.s_propfilt.t_else = (struct cnfstmt *)(uintptr_t)1;
    return result;
}

static struct cnfstmt *priorityFilterBranches(struct cnfstmt *thenBranch, struct cnfstmt *elseBranch) {
    struct cnfstmt *result = calloc(1, sizeof(*result));

    if (result == NULL || thenBranch == NULL || elseBranch == NULL) {
        constructionFailed = 1;
        free(result);
        cnfstmtDestructLst(thenBranch);
        cnfstmtDestructLst(elseBranch);
        return NULL;
    }
    result->printable = (uchar *)strdup("*.info");
    if (result->printable == NULL) {
        constructionFailed = 1;
        free(result);
        cnfstmtDestructLst(thenBranch);
        cnfstmtDestructLst(elseBranch);
        return NULL;
    }
    result->nodetype = S_RELOAD_PRIFILT;
    result->d.s_prifilt.t_then = thenBranch;
    result->d.s_prifilt.t_else = elseBranch;
    return result;
}

static void addObject(rsReloadCandidate_t *candidate, struct cnfobj *object) {
    rsReloadCandidateObject_t *entry = calloc(1, sizeof(*entry));
    if (entry == NULL || object == NULL) {
        constructionFailed = 1;
        free(entry);
        return;
    }
    entry->object = object;
    if (candidate->tail == NULL)
        candidate->head = entry;
    else
        candidate->tail->next = entry;
    candidate->tail = entry;
    ++candidate->objectCount;
}

typedef struct observed_s {
    const rsReloadNormalizedNodeV1_t *nodes[16];
    size_t count;
} observed_t;
#define OBSERVED_INIT \
    { {NULL}, 0 }

static rsRetVal observe(const rsReloadNormalizedNodeV1_t *node, void *context) {
    observed_t *observed = context;
    if (observed->count >= sizeof(observed->nodes) / sizeof(observed->nodes[0])) return RS_RET_OUT_OF_MEMORY;
    observed->nodes[observed->count++] = node;
    return RS_RET_OK;
}

static const rsReloadNormalizedNodeV1_t *findObserved(const observed_t *observed, const char *identity) {
    size_t i;

    for (i = 0; i < observed->count; ++i) {
        if (!strcmp(observed->nodes[i]->identity, identity)) return observed->nodes[i];
    }
    return NULL;
}

typedef struct objectObserved_s {
    enum cnfobjType types[8];
    size_t ordinals[8];
    char discriminator[8][32];
    size_t count;
    size_t failAt;
} objectObserved_t;

static rsRetVal observeObject(const struct cnfobj *const object, const size_t parseOrdinal, void *const context) {
    objectObserved_t *const observed = context;
    const char *value;
    size_t length = 0;

    if (observed->count >= sizeof(observed->types) / sizeof(observed->types[0])) return RS_RET_OUT_OF_MEMORY;
    observed->types[observed->count] = object->objType;
    value = nvlstString(object->nvlst, object->objType == CNFOBJ_MODULE ? "load" : "type", &length);
    if (value != NULL) {
        if (length >= sizeof(observed->discriminator[observed->count])) return RS_RET_OUT_OF_MEMORY;
        memcpy(observed->discriminator[observed->count], value, length);
        observed->discriminator[observed->count][length] = '\0';
    }
    observed->ordinals[observed->count++] = parseOrdinal;
    return parseOrdinal == observed->failAt ? RS_RET_NOT_IMPLEMENTED : RS_RET_OK;
}

int main(void) {
    rsReloadCandidate_t *candidate = calloc(1, sizeof(*candidate));
    rsReloadNormalizedGraphBuilderV1_t *builder = NULL;
    rsReloadNormalizedGraphV1_t graph;
    rsReloadNormalizedGraphV1_t permutedGraph;
    observed_t observed = OBSERVED_INIT;
    observed_t permuted = OBSERVED_INIT;
    struct nvlst *global = parameter("secret", "supersecret");
    struct nvlst *named = parameter("name", "MiXeD");

    /* Only is_else_if changes: the same condition and branch pointers remain
     * alive throughout. Repeated/restored controls rule out unstable output. */
    {
        struct cnfnumval condition = {.nodetype = 'N', .val = 1};
        struct cnfstmt stop = {.nodetype = S_STOP};
        struct cnfstmt ifStmt = {.nodetype = S_IF};
        es_str_t *control = es_newStr(32);
        es_str_t *repeat = es_newStr(32);
        es_str_t *changed = es_newStr(32);
        es_str_t *restored = es_newStr(32);

        CHECK(control != NULL && repeat != NULL && changed != NULL && restored != NULL);
        ifStmt.d.s_if.expr = (struct cnfexpr *)&condition;
        ifStmt.d.s_if.t_then = &stop;
        CHECK(appendStmt(&control, &ifStmt) == RS_RET_OK);
        CHECK(appendStmt(&repeat, &ifStmt) == RS_RET_OK);
        CHECK(es_strlen(control) == es_strlen(repeat));
        CHECK(!memcmp(es_getBufAddr(control), es_getBufAddr(repeat), es_strlen(control)));
        ifStmt.d.s_if.is_else_if = 1;
        CHECK(appendStmt(&changed, &ifStmt) == RS_RET_OK);
        CHECK(es_strlen(control) != es_strlen(changed) ||
              memcmp(es_getBufAddr(control), es_getBufAddr(changed), es_strlen(control)) != 0);
        ifStmt.d.s_if.is_else_if = 0;
        CHECK(appendStmt(&restored, &ifStmt) == RS_RET_OK);
        CHECK(es_strlen(control) == es_strlen(restored));
        CHECK(!memcmp(es_getBufAddr(control), es_getBufAddr(restored), es_strlen(control)));
        es_deleteStr(control);
        es_deleteStr(repeat);
        es_deleteStr(changed);
        es_deleteStr(restored);
    }

    /* Parser resource failures are control-plane failures, not malformed
     * configuration. Both Bison's documented memory-exhaustion result and a
     * capture callback preserve RS_RET_OUT_OF_MEMORY exactly. */
    {
        char configPath[] = "/tmp/rsyslog-reload-candidate-XXXXXX";
        rsReloadCandidate_t *parsedCandidate = NULL;
        const int configFd = mkstemp(configPath);

        CHECK(configFd >= 0);
        CHECK(close(configFd) == 0);
        parserResult = 2;
        CHECK(rsReloadCandidateParse(configPath, &parsedCandidate) == RS_RET_OUT_OF_MEMORY);
        CHECK(parsedCandidate == NULL);
        parserResult = 0;
        parserCaptureError = RS_RET_OUT_OF_MEMORY;
        CHECK(rsReloadCandidateParse(configPath, &parsedCandidate) == RS_RET_OUT_OF_MEMORY);
        CHECK(parsedCandidate == NULL);
        parserCaptureError = RS_RET_OK;
        /* A fatal scanner latch must reject even a parser acceptance, and
         * dominate a generic syntax error. Taking the latch clears it so a
         * later legitimate parse can succeed without inherited fatal state. */
        parserRequestedFatalError = RS_RET_OUT_OF_MEMORY;
        CHECK(rsReloadCandidateParse(configPath, &parsedCandidate) == RS_RET_OUT_OF_MEMORY);
        CHECK(parsedCandidate == NULL && parserFatalError == RS_RET_OK);
        parserResult = 1;
        CHECK(rsReloadCandidateParse(configPath, &parsedCandidate) == RS_RET_OUT_OF_MEMORY);
        CHECK(parsedCandidate == NULL && parserFatalError == RS_RET_OK);
        parserRequestedFatalError = RS_RET_OK;
        parserResult = 0;
        CHECK(rsReloadCandidateParse(configPath, &parsedCandidate) == RS_RET_OK);
        CHECK(parsedCandidate != NULL);
        rsReloadCandidateDestruct(&parsedCandidate);
        parserResult = 1;
        CHECK(unlink(configPath) == 0);

        /* A later resource failure must dominate earlier syntax/I/O errors;
         * subsequent weaker errors must not downgrade it. */
        parsedCandidate = calloc(1, sizeof(*parsedCandidate));
        CHECK(parsedCandidate != NULL);
        captureCandidate = parsedCandidate;
        rsReloadCandidateNoteError(RS_RET_CONF_PARSE_ERROR);
        rsReloadCandidateNoteError(RS_RET_OUT_OF_MEMORY);
        rsReloadCandidateNoteError(RS_RET_IO_ERROR);
        CHECK(parsedCandidate->captureError == RS_RET_OUT_OF_MEMORY);
        captureCandidate = NULL;
        rsReloadCandidateDestruct(&parsedCandidate);
    }

    /* Independent known vector for the private SHA-256 helper, including its
     * domain separator. This prevents a self-consistent broken digest from
     * satisfying only the graph determinism checks below. */
    {
        es_str_t *knownInput = es_newStrFromCStr("abc", 3);
        char *knownDigest = NULL;
        CHECK(knownInput != NULL);
        CHECK(sha256Fingerprint(knownInput, &knownDigest) == RS_RET_OK);
        CHECK(!strcmp(knownDigest, "sha256:9edfd6a7295560a88bc3b4cfedd32fdae33d7374e12fc0cdae52393f506b7348"));
        free(knownDigest);
        es_deleteStr(knownInput);
    }

    CHECK(candidate != NULL);
    /* Names that spell old synthetic identities are legitimate controls, not
     * duplicate nodes. Exercise input and action namespaces independently. */
    {
        struct cnfobj *namedInput = object(CNFOBJ_INPUT, parameter("name", "imtcp:anonymous:1"), NULL);
        struct cnfobj *unnamedInput = object(CNFOBJ_INPUT, parameter("type", "imtcp"), NULL);
        char *namedIdentity = NULL;
        char *unnamedIdentity = NULL;
        CHECK(namedInput != NULL && unnamedInput != NULL);
        CHECK(makeIdentity(namedInput, 1, &namedIdentity) == RS_RET_OK);
        CHECK(makeIdentity(unnamedInput, 1, &unnamedIdentity) == RS_RET_OK);
        CHECK(strcmp(namedIdentity, unnamedIdentity) != 0);
        CHECK(!strcmp(namedIdentity, "input:named:17:imtcp:anonymous:1"));
        free(namedIdentity);
        free(unnamedIdentity);
        cnfobjDestruct(namedInput);
        cnfobjDestruct(unnamedInput);
        /* The same named/anonymous namespace separation is shared by every
         * other source object, including module load and timezone id keys. */
        const enum cnfobjType kinds[] = {CNFOBJ_MODULE,       CNFOBJ_TPL,       CNFOBJ_PARSER,         CNFOBJ_TIMEZONE,
                                         CNFOBJ_LOOKUP_TABLE, CNFOBJ_DYN_STATS, CNFOBJ_PERCTILE_STATS, CNFOBJ_RATELIMIT,
                                         CNFOBJ_PROPERTY,     CNFOBJ_CONSTANT,  CNFOBJ_ACTION};
        for (size_t i = 0; i < sizeof(kinds) / sizeof(kinds[0]); ++i) {
            const char *key = kinds[i] == CNFOBJ_MODULE ? "load" : kinds[i] == CNFOBJ_TIMEZONE ? "id" : "name";
            struct cnfobj *namedObject = object(kinds[i], parameter(key, "anonymous:1"), NULL);
            struct cnfobj *unnamedObject = object(kinds[i], NULL, NULL);
            namedIdentity = unnamedIdentity = NULL;
            CHECK(namedObject != NULL && unnamedObject != NULL);
            CHECK(makeIdentity(namedObject, 1, &namedIdentity) == RS_RET_OK);
            CHECK(makeIdentity(unnamedObject, 1, &unnamedIdentity) == RS_RET_OK);
            CHECK(strcmp(namedIdentity, unnamedIdentity) != 0);
            free(namedIdentity);
            free(unnamedIdentity);
            cnfobjDestruct(namedObject);
            cnfobjDestruct(unnamedObject);
        }
        struct cnfobj *invalidRuleset = object(CNFOBJ_RULESET, parameter("queue.type", "LinkedList"), NULL);
        unnamedIdentity = NULL;
        CHECK(invalidRuleset != NULL);
        CHECK(makeIdentity(invalidRuleset, 1, &unnamedIdentity) == RS_RET_CONF_PARSE_ERROR);
        CHECK(unnamedIdentity == NULL);
        cnfobjDestruct(invalidRuleset);
        rsReloadNormalizedGraphBuilderV1_t *collisionBuilder = NULL;
        rsReloadNormalizedGraphV1_t collisionGraph;
        struct observed_s collisionObserved = {0};
        struct cnfstmt *actions = action("ruleset:default:anonymous:1", "control");
        size_t ordinal = 0;
        CHECK(actions != NULL);
        actions->next = action(NULL, "control");
        CHECK(actions->next != NULL);
        CHECK(rsReloadNormalizedGraphBuilderV1Construct(&collisionBuilder) == RS_RET_OK);
        CHECK(addActionNodes(collisionBuilder, "ruleset:default", actions, &ordinal) == RS_RET_OK);
        CHECK(rsReloadNormalizedGraphBuilderV1GetGraph(collisionBuilder, &collisionGraph) == RS_RET_OK);
        CHECK(collisionGraph.enumerate(collisionGraph.context, observe, &collisionObserved) == RS_RET_OK);
        CHECK(collisionObserved.count == 2);
        CHECK(strcmp(collisionObserved.nodes[0]->identity, collisionObserved.nodes[1]->identity) != 0);
        rsReloadNormalizedGraphBuilderV1Destruct(&collisionBuilder);
        cnfstmtDestructLst(actions);
    }
    CHECK(global != NULL);
    global->next = parameter("zeta", "one");
    CHECK(global->next != NULL);
    addObject(candidate, object(CNFOBJ_GLOBAL, global, NULL));
    addObject(candidate, object(CNFOBJ_RULESET, named, propertyFilter(action("named-action", "supersecret"))));
    addObject(candidate, object(CNFOBJ_RULESET, NULL, action("first", "supersecret")));
    addObject(candidate, object(CNFOBJ_RULESET, NULL, action("second", "supersecret")));
    addObject(candidate, object(CNFOBJ_INPUT, parameter("type", "imtcp"), NULL));
    addObject(candidate, object(CNFOBJ_INPUT, parameter("type", "imtcp"), NULL));
    CHECK(!constructionFailed);
    {
        objectObserved_t objectObserved = {.failAt = SIZE_MAX};
        objectObserved_t failedObserved = {.failAt = 2};
        objectObserved_t catalogObserved = {.failAt = SIZE_MAX};
        rsReloadCandidate_t *objectCatalog = NULL;

        CHECK(rsReloadCandidateVisitObjectsV1(NULL, observeObject, &objectObserved) == RS_RET_PARAM_ERROR);
        CHECK(rsReloadCandidateVisitObjectsV1(candidate, NULL, &objectObserved) == RS_RET_PARAM_ERROR);
        CHECK(rsReloadCandidateVisitObjectsV1(candidate, observeObject, &objectObserved) == RS_RET_OK);
        CHECK(objectObserved.count == 6);
        CHECK(objectObserved.types[0] == CNFOBJ_GLOBAL && objectObserved.ordinals[0] == 0);
        CHECK(objectObserved.types[1] == CNFOBJ_RULESET && objectObserved.ordinals[1] == 1);
        CHECK(objectObserved.types[5] == CNFOBJ_INPUT && objectObserved.ordinals[5] == 5);
        CHECK(rsReloadCandidateVisitObjectsV1(candidate, observeObject, &failedObserved) == RS_RET_NOT_IMPLEMENTED);
        CHECK(failedObserved.count == 3);
        CHECK(rsReloadCandidateBuildObjectCatalogV1(candidate, &objectCatalog) == RS_RET_OK);
        CHECK(rsReloadCandidateVisitObjectsV1(objectCatalog, observeObject, &catalogObserved) == RS_RET_OK);
        CHECK(catalogObserved.count == 2);
        CHECK(catalogObserved.types[0] == CNFOBJ_INPUT && !strcmp(catalogObserved.discriminator[0], "imtcp"));
        CHECK(catalogObserved.types[1] == CNFOBJ_INPUT && !strcmp(catalogObserved.discriminator[1], "imtcp"));
        rsReloadCandidateDestruct(&objectCatalog);
    }
    CHECK(rsReloadCandidateBuildNormalizedGraphV1(candidate, &builder) == RS_RET_OK);
    CHECK(rsReloadNormalizedGraphBuilderV1GetGraph(builder, &graph) == RS_RET_OK);
    CHECK(graph.enumerate(graph.context, observe, &observed) == RS_RET_OK);
    CHECK(observed.count == 9);
    CHECK(!strcmp(observed.nodes[0]->identity, "global"));
    CHECK(!strncmp(observed.nodes[0]->fingerprint, "sha256:", strlen("sha256:")));
    CHECK(strstr(observed.nodes[0]->fingerprint, "supersecret") == NULL);
    CHECK(!strcmp(observed.nodes[1]->identity, "input:anonymous:5:imtcp:1"));
    CHECK(!strcmp(observed.nodes[2]->identity, "input:anonymous:5:imtcp:2"));
    CHECK(!strcmp(observed.nodes[3]->identity, "ruleset:default"));
    CHECK(!strcmp(observed.nodes[4]->identity, "ruleset:mixed"));
    CHECK(!strcmp(observed.nodes[5]->identity, "action:named:12:named-action"));
    CHECK(!strcmp(observed.nodes[6]->identity, "action:named:5:first"));
    CHECK(!strcmp(observed.nodes[7]->identity, "action:named:6:second"));
    CHECK(observed.nodes[8]->objectKind == RS_RELOAD_OBJ_RULESET_METADATA);
    CHECK(!strcmp(observed.nodes[8]->identity, "ruleset:mixed"));

    {
        rsReloadCandidate_t *permutedCandidate = calloc(1, sizeof(*permutedCandidate));
        rsReloadNormalizedGraphBuilderV1_t *permutedBuilder = NULL;
        struct nvlst *permutedGlobal = parameter("zeta", "one");
        CHECK(permutedGlobal != NULL);
        permutedGlobal->next = parameter("secret", "supersecret");
        CHECK(permutedGlobal->next != NULL);
        CHECK(permutedCandidate != NULL);
        addObject(permutedCandidate, object(CNFOBJ_GLOBAL, permutedGlobal, NULL));
        CHECK(!constructionFailed);
        CHECK(rsReloadCandidateBuildNormalizedGraphV1(permutedCandidate, &permutedBuilder) == RS_RET_OK);
        CHECK(rsReloadNormalizedGraphBuilderV1GetGraph(permutedBuilder, &permutedGraph) == RS_RET_OK);
        CHECK(permutedGraph.enumerate(permutedGraph.context, observe, &permuted) == RS_RET_OK);
        CHECK(permuted.count == 1);
        CHECK(!strcmp(observed.nodes[0]->fingerprint, permuted.nodes[0]->fingerprint));
        rsReloadNormalizedGraphBuilderV1Destruct(&permutedBuilder);
        rsReloadCandidateDestruct(&permutedCandidate);
    }
    rsReloadNormalizedGraphBuilderV1Destruct(&builder);
    rsReloadCandidateDestruct(&candidate);

    /* Fixed imtcp listeners use their endpoint. Bare port zero follows
     * imtcp's effective fixed-port fallback to 514. Port-file allocation,
     * other zero spellings, and unknown inherited namespaces stay unkeyed. */
    {
        rsReloadCandidate_t *endpoints = calloc(1, sizeof(*endpoints));
        rsReloadNormalizedGraphBuilderV1_t *endpointBuilder = NULL;
        rsReloadNormalizedGraphV1_t endpointGraph;
        observed_t endpointObserved = {0};
        struct nvlst *fixed = parameter("type", "ImTcP");
        struct nvlst *dynamic = parameter("type", "imtcp");
        struct nvlst *portFile = parameter("type", "imtcp");
        struct nvlst *zeroAlias = parameter("type", "imtcp");
        struct nvlst *inheritedNamespace = parameter("type", "imtcp");
        CHECK(endpoints != NULL && fixed != NULL && dynamic != NULL && portFile != NULL);
        CHECK(zeroAlias != NULL && inheritedNamespace != NULL);
        fixed->next = parameter("port", "0514");
        fixed->next->next = parameter("address", "2001:db8::1");
        fixed->next->next->next = parameter("networknamespace", "blue:prod");
        dynamic->next = parameter("port", "0");
        dynamic->next->next = parameter("networknamespace", "");
        portFile->next = parameter("port", "10514");
        portFile->next->next = parameter("listenportfilename", "dynamic.port");
        zeroAlias->next = parameter("port", "00");
        inheritedNamespace->next = parameter("port", "5514");
        CHECK(zeroAlias->next != NULL && inheritedNamespace->next != NULL);
        CHECK(fixed->next != NULL && fixed->next->next != NULL && fixed->next->next->next != NULL &&
              dynamic->next != NULL && portFile->next != NULL && portFile->next->next != NULL);
        addObject(endpoints, object(CNFOBJ_INPUT, fixed, NULL));
        addObject(endpoints, object(CNFOBJ_TPL, parameter("name", "unrelated"), NULL));
        addObject(endpoints, object(CNFOBJ_INPUT, dynamic, NULL));
        addObject(endpoints, object(CNFOBJ_INPUT, portFile, NULL));
        addObject(endpoints, object(CNFOBJ_INPUT, zeroAlias, NULL));
        addObject(endpoints, object(CNFOBJ_INPUT, inheritedNamespace, NULL));
        CHECK(rsReloadCandidateBuildNormalizedGraphV1(endpoints, &endpointBuilder) == RS_RET_OK);
        CHECK(rsReloadNormalizedGraphBuilderV1GetGraph(endpointBuilder, &endpointGraph) == RS_RET_OK);
        CHECK(endpointGraph.enumerate(endpointGraph.context, observe, &endpointObserved) == RS_RET_OK);
        CHECK(findObserved(&endpointObserved, "input:imtcp:endpoint:n9:blue:proda11:2001:db8::1p514") != NULL);
        CHECK(findObserved(&endpointObserved, "input:imtcp:endpoint:n0:a1:*p514") != NULL);
        CHECK(findObserved(&endpointObserved, "input:anonymous:5:imtcp:3") != NULL);
        CHECK(findObserved(&endpointObserved, "input:anonymous:5:imtcp:4") != NULL);
        CHECK(findObserved(&endpointObserved, "input:anonymous:5:imtcp:5") != NULL);
        rsReloadNormalizedGraphBuilderV1Destruct(&endpointBuilder);
        rsReloadCandidateDestruct(&endpoints);

        /* Canonical decimal spelling and length-prefixing are properties of
         * the endpoint identity itself, independent of graph enumeration. */
        {
            struct cnfobj *canonical = object(CNFOBJ_INPUT, parameter("port", "514"), NULL);
            struct cnfobj *defaults = object(CNFOBJ_INPUT, parameter("port", "5514"), NULL);
            struct cnfobj *separatorLeft = object(CNFOBJ_INPUT, parameter("port", "6514"), NULL);
            struct cnfobj *separatorRight = object(CNFOBJ_INPUT, parameter("port", "6514"), NULL);
            char *canonicalIdentity = NULL;
            char *defaultIdentity = NULL;
            char *separatorLeftIdentity = NULL;
            char *separatorRightIdentity = NULL;
            CHECK(canonical != NULL && defaults != NULL && separatorLeft != NULL && separatorRight != NULL);
            canonical->nvlst->next = parameter("address", "2001:db8::1");
            canonical->nvlst->next->next = parameter("networknamespace", "blue:prod");
            CHECK(canonical->nvlst->next != NULL && canonical->nvlst->next->next != NULL);
            separatorLeft->nvlst->next = parameter("address", "b:c");
            separatorLeft->nvlst->next->next = parameter("networknamespace", "a");
            separatorRight->nvlst->next = parameter("address", "c");
            separatorRight->nvlst->next->next = parameter("networknamespace", "a:b");
            CHECK(separatorLeft->nvlst->next != NULL && separatorLeft->nvlst->next->next != NULL &&
                  separatorRight->nvlst->next != NULL && separatorRight->nvlst->next->next != NULL);
            CHECK(makeImtcpEndpointIdentity(canonical, &canonicalIdentity) == RS_RET_OK);
            CHECK(!strcmp(canonicalIdentity, "input:imtcp:endpoint:n9:blue:proda11:2001:db8::1p514"));
            CHECK(makeImtcpEndpointIdentity(defaults, &defaultIdentity) == RS_RET_NOT_FOUND);
            CHECK(defaultIdentity == NULL);
            unsigned parsedPort = 0;
            CHECK(parseFixedTcpPort("0", 1, &parsedPort) == RS_RET_OK && parsedPort == 514);
            CHECK(parseFixedTcpPort("0514", 4, &parsedPort) == RS_RET_OK && parsedPort == 514);
            CHECK(parseFixedTcpPort("00", 2, &parsedPort) == RS_RET_NOT_FOUND);
            CHECK(parseFixedTcpPort("000", 3, &parsedPort) == RS_RET_NOT_FOUND);
            CHECK(makeImtcpEndpointIdentity(separatorLeft, &separatorLeftIdentity) == RS_RET_OK);
            CHECK(makeImtcpEndpointIdentity(separatorRight, &separatorRightIdentity) == RS_RET_OK);
            CHECK(strcmp(separatorLeftIdentity, separatorRightIdentity));
            free(canonicalIdentity);
            free(defaultIdentity);
            free(separatorLeftIdentity);
            free(separatorRightIdentity);
            cnfobjDestruct(canonical);
            cnfobjDestruct(defaults);
            cnfobjDestruct(separatorLeft);
            cnfobjDestruct(separatorRight);
        }
    }

    /* Property filters have only an effective then branch: poison in the
     * unused else slot must not affect graph construction or destruction.
     * Priority filters retain real else semantics and branch identities. */
    {
        rsReloadCandidate_t *first = calloc(1, sizeof(*first));
        rsReloadCandidate_t *second = calloc(1, sizeof(*second));
        rsReloadCandidate_t *priorityFirst = calloc(1, sizeof(*priorityFirst));
        rsReloadCandidate_t *prioritySecond = calloc(1, sizeof(*prioritySecond));
        rsReloadNormalizedGraphBuilderV1_t *firstBuilder = NULL;
        rsReloadNormalizedGraphBuilderV1_t *secondBuilder = NULL;
        rsReloadNormalizedGraphBuilderV1_t *priorityFirstBuilder = NULL;
        rsReloadNormalizedGraphBuilderV1_t *prioritySecondBuilder = NULL;
        rsReloadNormalizedGraphV1_t firstGraph;
        rsReloadNormalizedGraphV1_t secondGraph;
        rsReloadNormalizedGraphV1_t priorityFirstGraph;
        rsReloadNormalizedGraphV1_t prioritySecondGraph;
        observed_t firstObserved = OBSERVED_INIT;
        observed_t secondObserved = OBSERVED_INIT;
        observed_t priorityFirstObserved = OBSERVED_INIT;
        observed_t prioritySecondObserved = OBSERVED_INIT;
        const rsReloadNormalizedNodeV1_t *firstRuleset;
        const rsReloadNormalizedNodeV1_t *secondRuleset;
        const rsReloadNormalizedNodeV1_t *priorityFirstRuleset;
        const rsReloadNormalizedNodeV1_t *prioritySecondRuleset;

        CHECK(first != NULL && second != NULL && priorityFirst != NULL && prioritySecond != NULL);
        addObject(first, object(CNFOBJ_RULESET, parameter("name", "branches"),
                                propertyFilterBranches(action("then-action", "same"), action("else-first", "same"))));
        addObject(second, object(CNFOBJ_RULESET, parameter("name", "branches"),
                                 propertyFilterBranches(action("then-action", "same"), action("else-second", "same"))));
        addObject(priorityFirst, object(CNFOBJ_RULESET, parameter("name", "priority-branches"),
                                        priorityFilterBranches(action("priority-then", "same"),
                                                               action("priority-else-first", "same"))));
        addObject(prioritySecond, object(CNFOBJ_RULESET, parameter("name", "priority-branches"),
                                         priorityFilterBranches(action("priority-then", "same"),
                                                                action("priority-else-second", "same"))));
        CHECK(!constructionFailed);
        CHECK(rsReloadCandidateBuildNormalizedGraphV1(first, &firstBuilder) == RS_RET_OK);
        CHECK(rsReloadCandidateBuildNormalizedGraphV1(second, &secondBuilder) == RS_RET_OK);
        CHECK(rsReloadCandidateBuildNormalizedGraphV1(priorityFirst, &priorityFirstBuilder) == RS_RET_OK);
        CHECK(rsReloadCandidateBuildNormalizedGraphV1(prioritySecond, &prioritySecondBuilder) == RS_RET_OK);
        CHECK(rsReloadNormalizedGraphBuilderV1GetGraph(firstBuilder, &firstGraph) == RS_RET_OK);
        CHECK(rsReloadNormalizedGraphBuilderV1GetGraph(secondBuilder, &secondGraph) == RS_RET_OK);
        CHECK(rsReloadNormalizedGraphBuilderV1GetGraph(priorityFirstBuilder, &priorityFirstGraph) == RS_RET_OK);
        CHECK(rsReloadNormalizedGraphBuilderV1GetGraph(prioritySecondBuilder, &prioritySecondGraph) == RS_RET_OK);
        CHECK(firstGraph.enumerate(firstGraph.context, observe, &firstObserved) == RS_RET_OK);
        CHECK(secondGraph.enumerate(secondGraph.context, observe, &secondObserved) == RS_RET_OK);
        CHECK(priorityFirstGraph.enumerate(priorityFirstGraph.context, observe, &priorityFirstObserved) == RS_RET_OK);
        CHECK(prioritySecondGraph.enumerate(prioritySecondGraph.context, observe, &prioritySecondObserved) ==
              RS_RET_OK);
        firstRuleset = findObserved(&firstObserved, "ruleset:branches");
        secondRuleset = findObserved(&secondObserved, "ruleset:branches");
        CHECK(firstRuleset != NULL && secondRuleset != NULL);
        CHECK(!strcmp(firstRuleset->fingerprint, secondRuleset->fingerprint));
        CHECK(findObserved(&firstObserved, "action:named:10:else-first") == NULL);
        CHECK(findObserved(&firstObserved, "action:named:11:then-action") != NULL);
        CHECK(findObserved(&secondObserved, "action:named:11:else-second") == NULL);
        CHECK(findObserved(&secondObserved, "action:named:11:then-action") != NULL);
        /* Effective operation, negation, and then-action changes must still
         * change the fingerprint despite ignoring the unused child slot. */
        for (int variant = 0; variant < 3; ++variant) {
            rsReloadCandidate_t *changed = calloc(1, sizeof(*changed));
            rsReloadNormalizedGraphBuilderV1_t *changedBuilder = NULL;
            rsReloadNormalizedGraphV1_t changedGraph;
            observed_t changedObserved = OBSERVED_INIT;
            struct cnfstmt *filter = propertyFilter(action("then-action", "same"));
            const rsReloadNormalizedNodeV1_t *changedRuleset;
            CHECK(changed != NULL && filter != NULL);
            if (variant == 2) {
                filter->d.s_propfilt.t_then->next = calloc(1, sizeof(struct cnfstmt));
                CHECK(filter->d.s_propfilt.t_then->next != NULL);
                filter->d.s_propfilt.t_then->next->nodetype = S_STOP;
            }
            if (variant != 2) {
                free(filter->printable);
                filter->printable =
                    (uchar *)strdup(variant == 0 ? ":msg, startswith, \"needle\"" : ":msg, !contains, \"needle\"");
                CHECK(filter->printable != NULL);
            }
            filter->d.s_propfilt.t_else = (struct cnfstmt *)(uintptr_t)1;
            addObject(changed, object(CNFOBJ_RULESET, parameter("name", "branches"), filter));
            CHECK(!constructionFailed);
            CHECK(rsReloadCandidateBuildNormalizedGraphV1(changed, &changedBuilder) == RS_RET_OK);
            CHECK(rsReloadNormalizedGraphBuilderV1GetGraph(changedBuilder, &changedGraph) == RS_RET_OK);
            CHECK(changedGraph.enumerate(changedGraph.context, observe, &changedObserved) == RS_RET_OK);
            changedRuleset = findObserved(&changedObserved, "ruleset:branches");
            CHECK(changedRuleset != NULL);
            CHECK(strcmp(firstRuleset->fingerprint, changedRuleset->fingerprint));
            CHECK(findObserved(&changedObserved, "action:named:11:then-action") != NULL);
            rsReloadNormalizedGraphBuilderV1Destruct(&changedBuilder);
            rsReloadCandidateDestruct(&changed);
        }
        priorityFirstRuleset = findObserved(&priorityFirstObserved, "ruleset:priority-branches");
        prioritySecondRuleset = findObserved(&prioritySecondObserved, "ruleset:priority-branches");
        CHECK(priorityFirstRuleset != NULL && prioritySecondRuleset != NULL);
        CHECK(strcmp(priorityFirstRuleset->fingerprint, prioritySecondRuleset->fingerprint));
        CHECK(findObserved(&priorityFirstObserved, "action:named:19:priority-else-first") != NULL);
        CHECK(findObserved(&priorityFirstObserved, "action:named:13:priority-then") != NULL);
        CHECK(findObserved(&prioritySecondObserved, "action:named:20:priority-else-second") != NULL);
        CHECK(findObserved(&prioritySecondObserved, "action:named:13:priority-then") != NULL);
        rsReloadNormalizedGraphBuilderV1Destruct(&firstBuilder);
        rsReloadNormalizedGraphBuilderV1Destruct(&secondBuilder);
        rsReloadNormalizedGraphBuilderV1Destruct(&priorityFirstBuilder);
        rsReloadNormalizedGraphBuilderV1Destruct(&prioritySecondBuilder);
        rsReloadCandidateDestruct(&first);
        rsReloadCandidateDestruct(&second);
        rsReloadCandidateDestruct(&priorityFirst);
        rsReloadCandidateDestruct(&prioritySecond);
    }

    /* Global fragments are a case-folded last-write map. Equivalent split
     * RainerScript and combined YAML blocks therefore hash identically. */
    {
        rsReloadCandidate_t *split = calloc(1, sizeof(*split));
        rsReloadCandidate_t *combined = calloc(1, sizeof(*combined));
        rsReloadNormalizedGraphBuilderV1_t *splitBuilder = NULL;
        rsReloadNormalizedGraphBuilderV1_t *combinedBuilder = NULL;
        rsReloadNormalizedGraphV1_t splitGraph;
        rsReloadNormalizedGraphV1_t combinedGraph;
        observed_t splitObserved = OBSERVED_INIT;
        observed_t combinedObserved = OBSERVED_INIT;
        struct nvlst *combinedParams = parameter("alpha", "two");
        CHECK(split != NULL && combined != NULL && combinedParams != NULL);
        addObject(split, object(CNFOBJ_GLOBAL, parameter("alpha", "one"), NULL));
        addObject(split, object(CNFOBJ_GLOBAL, parameter("ALPHA", "two"), NULL));
        addObject(combined, object(CNFOBJ_GLOBAL, combinedParams, NULL));
        CHECK(!constructionFailed);
        CHECK(rsReloadCandidateBuildNormalizedGraphV1(split, &splitBuilder) == RS_RET_OK);
        CHECK(rsReloadCandidateBuildNormalizedGraphV1(combined, &combinedBuilder) == RS_RET_OK);
        CHECK(rsReloadNormalizedGraphBuilderV1GetGraph(splitBuilder, &splitGraph) == RS_RET_OK);
        CHECK(rsReloadNormalizedGraphBuilderV1GetGraph(combinedBuilder, &combinedGraph) == RS_RET_OK);
        CHECK(splitGraph.enumerate(splitGraph.context, observe, &splitObserved) == RS_RET_OK);
        CHECK(combinedGraph.enumerate(combinedGraph.context, observe, &combinedObserved) == RS_RET_OK);
        CHECK(splitObserved.count == 1 && combinedObserved.count == 1);
        CHECK(!strcmp(splitObserved.nodes[0]->identity, "global"));
        CHECK(!strcmp(splitObserved.nodes[0]->fingerprint, combinedObserved.nodes[0]->fingerprint));
        rsReloadNormalizedGraphBuilderV1Destruct(&splitBuilder);
        rsReloadNormalizedGraphBuilderV1Destruct(&combinedBuilder);
        rsReloadCandidateDestruct(&split);
        rsReloadCandidateDestruct(&combined);
    }

    /* Unrelated objects and named actions cannot rename anonymous runtime
     * identities. Only anonymous siblings of the same kind/type contribute
     * to their conservative occurrence discriminator. */
    {
        rsReloadCandidate_t *base = calloc(1, sizeof(*base));
        rsReloadCandidate_t *withUnrelated = calloc(1, sizeof(*withUnrelated));
        rsReloadNormalizedGraphBuilderV1_t *baseBuilder = NULL;
        rsReloadNormalizedGraphBuilderV1_t *unrelatedBuilder = NULL;
        rsReloadNormalizedGraphV1_t baseGraph;
        rsReloadNormalizedGraphV1_t unrelatedGraph;
        observed_t baseObserved = OBSERVED_INIT;
        observed_t unrelatedObserved = OBSERVED_INIT;
        struct cnfstmt *namedAction = action("named-before", "secret");
        struct cnfstmt *anonymousAction = action(NULL, "same-secret");
        struct nvlst *namedInput = parameter("type", "IMTCP");
        CHECK(base != NULL && withUnrelated != NULL && namedAction != NULL && anonymousAction != NULL);
        CHECK(namedInput != NULL);
        namedInput->next = parameter("name", "named-before");
        CHECK(namedInput->next != NULL);
        namedAction->next = anonymousAction;
        addObject(base, object(CNFOBJ_INPUT, parameter("type", "imtcp"), NULL));
        addObject(base, object(CNFOBJ_RULESET, NULL, action(NULL, "same-secret")));
        addObject(withUnrelated, object(CNFOBJ_TPL, parameter("name", "unrelated"), NULL));
        addObject(withUnrelated, object(CNFOBJ_INPUT, namedInput, NULL));
        addObject(withUnrelated, object(CNFOBJ_INPUT, parameter("type", "ImTcP"), NULL));
        addObject(withUnrelated, object(CNFOBJ_RULESET, NULL, namedAction));
        CHECK(!constructionFailed);
        CHECK(rsReloadCandidateBuildNormalizedGraphV1(base, &baseBuilder) == RS_RET_OK);
        CHECK(rsReloadCandidateBuildNormalizedGraphV1(withUnrelated, &unrelatedBuilder) == RS_RET_OK);
        CHECK(rsReloadNormalizedGraphBuilderV1GetGraph(baseBuilder, &baseGraph) == RS_RET_OK);
        CHECK(rsReloadNormalizedGraphBuilderV1GetGraph(unrelatedBuilder, &unrelatedGraph) == RS_RET_OK);
        CHECK(baseGraph.enumerate(baseGraph.context, observe, &baseObserved) == RS_RET_OK);
        CHECK(unrelatedGraph.enumerate(unrelatedGraph.context, observe, &unrelatedObserved) == RS_RET_OK);
        CHECK(findObserved(&baseObserved, "input:anonymous:5:imtcp:1") != NULL);
        CHECK(findObserved(&unrelatedObserved, "input:anonymous:5:imtcp:1") != NULL);
        CHECK(findObserved(&baseObserved, "action:anonymous:15:ruleset:default:1") != NULL);
        CHECK(findObserved(&unrelatedObserved, "action:anonymous:15:ruleset:default:1") != NULL);
        CHECK(!strcmp(findObserved(&baseObserved, "action:anonymous:15:ruleset:default:1")->fingerprint,
                      findObserved(&unrelatedObserved, "action:anonymous:15:ruleset:default:1")->fingerprint));
        rsReloadNormalizedGraphBuilderV1Destruct(&baseBuilder);
        rsReloadNormalizedGraphBuilderV1Destruct(&unrelatedBuilder);
        rsReloadCandidateDestruct(&base);
        rsReloadCandidateDestruct(&withUnrelated);
    }

    /* An explicit empty global object exists in both graphs. The source
     * mirror must not lose it merely because it has no parameter list. */
    {
        rsReloadCandidate_t *emptyCandidate = calloc(1, sizeof(*emptyCandidate));
        rsReloadNormalizedGraphBuilderV1_t *emptyCandidateBuilder = NULL;
        rsReloadNormalizedGraphBuilderV1_t *emptySourceBuilder = NULL;
        rsReloadCandidate_t *emptySourceCatalog = NULL;
        rsReloadNormalizedGraphV1_t emptyCandidateGraph;
        rsReloadNormalizedGraphV1_t emptySourceGraph;
        observed_t emptyCandidateObserved = OBSERVED_INIT;
        observed_t emptySourceObserved = OBSERVED_INIT;
        struct cnfobj emptySourceObject = {.objType = CNFOBJ_GLOBAL};

        CHECK(emptyCandidate != NULL);
        addObject(emptyCandidate, object(CNFOBJ_GLOBAL, NULL, NULL));
        CHECK(!constructionFailed);
        CHECK(rsReloadCandidateBuildNormalizedGraphV1(emptyCandidate, &emptyCandidateBuilder) == RS_RET_OK);
        CHECK(rsReloadCandidateSourceBegin() == RS_RET_OK);
        rsReloadCandidateSourceCaptureObject(&emptySourceObject);
        CHECK(rsReloadCandidateSourceFinish(&emptySourceBuilder, &emptySourceCatalog) == RS_RET_OK);
        CHECK(rsReloadCandidateObjectCount(emptySourceCatalog) == 0);
        CHECK(rsReloadNormalizedGraphBuilderV1GetGraph(emptyCandidateBuilder, &emptyCandidateGraph) == RS_RET_OK);
        CHECK(rsReloadNormalizedGraphBuilderV1GetGraph(emptySourceBuilder, &emptySourceGraph) == RS_RET_OK);
        CHECK(emptyCandidateGraph.enumerate(emptyCandidateGraph.context, observe, &emptyCandidateObserved) ==
              RS_RET_OK);
        CHECK(emptySourceGraph.enumerate(emptySourceGraph.context, observe, &emptySourceObserved) == RS_RET_OK);
        CHECK(emptyCandidateObserved.count == 1 && emptySourceObserved.count == 1);
        CHECK(!strcmp(emptyCandidateObserved.nodes[0]->fingerprint, emptySourceObserved.nodes[0]->fingerprint));
        rsReloadNormalizedGraphBuilderV1Destruct(&emptyCandidateBuilder);
        rsReloadNormalizedGraphBuilderV1Destruct(&emptySourceBuilder);
        rsReloadCandidateDestruct(&emptySourceCatalog);
        rsReloadCandidateDestruct(&emptyCandidate);
    }

    /* A normal-startup parser error invalidates the source mirror; no partial
     * baseline may be handed to the report-only HUP path afterwards. */
    {
        rsReloadNormalizedGraphBuilderV1_t *dirtySourceBuilder = NULL;
        rsReloadCandidate_t *dirtySourceCatalog = NULL;
        CHECK(rsReloadCandidateSourceBegin() == RS_RET_OK);
        rsReloadCandidateSourceNoteError();
        CHECK(rsReloadCandidateSourceFinish(&dirtySourceBuilder, &dirtySourceCatalog) == RS_RET_NOT_IMPLEMENTED);
        CHECK(dirtySourceBuilder == NULL);
        CHECK(dirtySourceCatalog == NULL);
    }

    /* The startup source mirror uses the same per-type anonymous input
     * ordinal as the candidate. A named IMTCP sibling must not consume the
     * ordinal, and casing of the input type must not split that namespace. */
    {
        rsReloadNormalizedGraphBuilderV1_t *observedSourceBuilder = NULL;
        rsReloadCandidate_t *observedSourceCatalog = NULL;
        rsReloadNormalizedGraphV1_t sourceGraph;
        observed_t sourceObserved = OBSERVED_INIT;
        struct nvlst *namedInput = parameter("type", "IMTCP");
        struct nvlst *moduleParams = parameter("load", "imtcp");
        struct cnfobj *moduleObject;
        struct cnfobj *namedObject;
        struct cnfobj *otherObject;
        struct cnfobj *anonymousObject;

        CHECK(namedInput != NULL && moduleParams != NULL);
        moduleParams->next = parameter("networknamespace", "blue");
        CHECK(moduleParams->next != NULL);
        namedInput->next = parameter("name", "named-before");
        CHECK(namedInput->next != NULL);
        moduleObject = object(CNFOBJ_MODULE, moduleParams, NULL);
        namedObject = object(CNFOBJ_INPUT, namedInput, NULL);
        otherObject = object(CNFOBJ_INPUT, parameter("type", "imudp"), NULL);
        anonymousObject = object(CNFOBJ_INPUT, parameter("type", "ImTcP"), NULL);
        CHECK(moduleObject != NULL && namedObject != NULL && otherObject != NULL && anonymousObject != NULL &&
              otherObject->nvlst != NULL && anonymousObject->nvlst != NULL);
        CHECK(rsReloadCandidateSourceBegin() == RS_RET_OK);
        rsReloadCandidateSourceCaptureObject(moduleObject);
        rsReloadCandidateSourceCaptureObject(namedObject);
        rsReloadCandidateSourceCaptureObject(otherObject);
        rsReloadCandidateSourceCaptureObject(anonymousObject);
        CHECK(rsReloadCandidateSourceFinish(&observedSourceBuilder, &observedSourceCatalog) == RS_RET_OK);
        CHECK(rsReloadCandidateObjectCount(observedSourceCatalog) == 4);
        cnfobjDestruct(moduleObject);
        cnfobjDestruct(namedObject);
        cnfobjDestruct(otherObject);
        cnfobjDestruct(anonymousObject);
        {
            objectObserved_t catalogObserved = {.failAt = SIZE_MAX};
            CHECK(rsReloadCandidateVisitObjectsV1(observedSourceCatalog, observeObject, &catalogObserved) == RS_RET_OK);
            CHECK(catalogObserved.count == 4);
            CHECK(catalogObserved.types[0] == CNFOBJ_MODULE);
            CHECK(!strcmp(catalogObserved.discriminator[0], "imtcp"));
            CHECK(catalogObserved.types[3] == CNFOBJ_INPUT);
            CHECK(!strcmp(catalogObserved.discriminator[3], "ImTcP"));
        }
        CHECK(rsReloadNormalizedGraphBuilderV1GetGraph(observedSourceBuilder, &sourceGraph) == RS_RET_OK);
        CHECK(sourceGraph.enumerate(sourceGraph.context, observe, &sourceObserved) == RS_RET_OK);
        CHECK(sourceObserved.count == 4);
        CHECK(findObserved(&sourceObserved, "input:anonymous:5:imtcp:1") != NULL);
        rsReloadNormalizedGraphBuilderV1Destruct(&observedSourceBuilder);
        rsReloadCandidateDestruct(&observedSourceCatalog);
    }

    candidate = calloc(1, sizeof(*candidate));
    builder = NULL;
    global = parameter("duplicate", "one");
    CHECK(global != NULL);
    global->next = parameter("DuPlicate", "two");
    CHECK(global->next != NULL);
    addObject(candidate, object(CNFOBJ_GLOBAL, global, NULL));
    CHECK(rsReloadCandidateBuildNormalizedGraphV1(candidate, &builder) == RS_RET_CONF_PARSE_ERROR);
    CHECK(builder == NULL);
    rsReloadCandidateDestruct(&candidate);

    /* The first live-reload gate accepts only a changed program for an
     * existing ruleset while the referenced named action stays identical.
     * Any global change remains fail-closed at this milestone. */
    {
        rsReloadCandidate_t *activeCandidate = calloc(1, sizeof(*activeCandidate));
        rsReloadCandidate_t *rulesetCandidate = calloc(1, sizeof(*rulesetCandidate));
        rsReloadCandidate_t *globalCandidate = calloc(1, sizeof(*globalCandidate));
        rsReloadNormalizedGraphBuilderV1_t *activeBuilder = NULL;
        rsReloadNormalizedGraphV1_t activeGraph;
        rsReloadReportV1_t *report = NULL;
        struct cnfstmt *changedFilter;

        CHECK(activeCandidate != NULL && rulesetCandidate != NULL && globalCandidate != NULL);
        addObject(activeCandidate, object(CNFOBJ_RULESET, parameter("name", "route"),
                                          propertyFilter(action("stable-action", "same-config"))));
        changedFilter = propertyFilter(action("stable-action", "same-config"));
        CHECK(changedFilter != NULL);
        free(changedFilter->printable);
        changedFilter->printable = (uchar *)strdup(":msg, contains, \"changed\"");
        CHECK(changedFilter->printable != NULL);
        addObject(rulesetCandidate, object(CNFOBJ_RULESET, parameter("name", "route"), changedFilter));
        addObject(globalCandidate, object(CNFOBJ_GLOBAL, parameter("workdirectory", "/changed"), NULL));
        CHECK(!constructionFailed);
        CHECK(rsReloadCandidateBuildNormalizedGraphV1(activeCandidate, &activeBuilder) == RS_RET_OK);
        CHECK(rsReloadNormalizedGraphBuilderV1GetGraph(activeBuilder, &activeGraph) == RS_RET_OK);
        {
            rsReloadNormalizedGraphBuilderV1_t *candidateBuilder = NULL;
            rsReloadNormalizedGraphV1_t candidateGraph;
            CHECK(rsReloadCandidateBuildNormalizedGraphV1(rulesetCandidate, &candidateBuilder) == RS_RET_OK);
            CHECK(rsReloadNormalizedGraphBuilderV1GetGraph(candidateBuilder, &candidateGraph) == RS_RET_OK);
            CHECK(rsReloadReportBuildV1(&activeGraph, &candidateGraph, &report) == RS_RET_OK);
            rsReloadNormalizedGraphBuilderV1Destruct(&candidateBuilder);
        }
        CHECK(rsReloadCandidateCheckRulesetOnlyReportV1(report) == RS_RET_OK);
        CHECK(report != NULL && report->modifiedCount == 1 && report->invalidCount == 0);
        {
            rsReloadReportV1_t malformed = *report;
            malformed.entries = NULL;
            CHECK(rsReloadCandidateCheckRulesetOnlyReportV1(&malformed) == RS_RET_PARAM_ERROR);
            malformed = *report;
            /* Deliberately invalid numeric address: admission must reject its
             * alignment before any entry is dereferenced. */
            malformed.entries = (rsReloadReportEntryV1_t *)((uintptr_t)report->entries + 1);
            CHECK(rsReloadCandidateCheckRulesetOnlyReportV1(&malformed) == RS_RET_PARAM_ERROR);
            malformed = *report;
            malformed.entryStride = sizeof(rsReloadReportEntryV1_t) - 1;
            CHECK(rsReloadCandidateCheckRulesetOnlyReportV1(&malformed) == RS_RET_PARAM_ERROR);
            malformed = *report;
            malformed.entryStride = sizeof(rsReloadReportEntryV1_t) + 1;
            CHECK(rsReloadCandidateCheckRulesetOnlyReportV1(&malformed) == RS_RET_PARAM_ERROR);
            malformed = *report;
            malformed.entryCount = SIZE_MAX / malformed.entryStride + 1;
            CHECK(rsReloadCandidateCheckRulesetOnlyReportV1(&malformed) == RS_RET_PARAM_ERROR);
            malformed = *report;
            malformed.entries = (rsReloadReportEntryV1_t *)(uintptr_t)(UINTPTR_MAX - malformed.entryStride + 1);
            CHECK(rsReloadCandidateCheckRulesetOnlyReportV1(&malformed) == RS_RET_PARAM_ERROR);
            CHECK(rsReloadCandidateCheckRulesetOnlyReportV1(report) == RS_RET_OK);
        }
        rsReloadReportDestructV1(&report);
        {
            rsReloadNormalizedGraphBuilderV1_t *candidateBuilder = NULL;
            rsReloadNormalizedGraphV1_t candidateGraph;
            CHECK(rsReloadCandidateBuildNormalizedGraphV1(globalCandidate, &candidateBuilder) == RS_RET_OK);
            CHECK(rsReloadNormalizedGraphBuilderV1GetGraph(candidateBuilder, &candidateGraph) == RS_RET_OK);
            CHECK(rsReloadReportBuildV1(&activeGraph, &candidateGraph, &report) == RS_RET_OK);
            rsReloadNormalizedGraphBuilderV1Destruct(&candidateBuilder);
        }
        CHECK(rsReloadCandidateCheckRulesetOnlyReportV1(report) == RS_RET_NOT_IMPLEMENTED);
        rsReloadReportDestructV1(&report);
        rsReloadNormalizedGraphBuilderV1Destruct(&activeBuilder);
        rsReloadCandidateDestruct(&activeCandidate);
        rsReloadCandidateDestruct(&rulesetCandidate);
        rsReloadCandidateDestruct(&globalCandidate);
    }

    /* Source capture and detached graphs must agree on body/attribute rows.
     * Attribute order and registry name case are equivalent. Queue/parser-only
     * and combined edits cannot enter a program-only materialization plan. */
    {
        rsReloadNormalizedGraphBuilderV1_t *source = NULL;
        rsReloadNormalizedGraphV1_t sourceGraph;
        struct nvlst *attributes = parameter("name", "Route");
        struct cnfobj *active;
        CHECK(attributes != NULL);
        attributes->next = parameter("queue.type", "LinkedList");
        CHECK(attributes->next != NULL);
        active = object(CNFOBJ_RULESET, attributes, propertyFilter(action("sink", "same")));
        CHECK(active != NULL);
        CHECK(rsReloadCandidateSourceBegin() == RS_RET_OK);
        rsReloadCandidateSourceCaptureObject(active);
        CHECK(rsReloadCandidateSourceFinish(&source) == RS_RET_OK);
        CHECK(rsReloadNormalizedGraphBuilderV1GetGraph(source, &sourceGraph) == RS_RET_OK);
        for (int variant = 0; variant < 6; ++variant) {
            rsReloadCandidate_t *edited = calloc(1, sizeof(*edited));
            rsReloadNormalizedGraphBuilderV1_t *editedBuilder = NULL;
            rsReloadNormalizedGraphV1_t editedGraph;
            rsReloadReportV1_t *report = NULL;
            struct nvlst *params = parameter("QUEUE.TYPE", variant == 1 || variant == 4 ? "FixedArray" : "LinkedList");
            struct cnfstmt *body = propertyFilter(action("sink", "same"));
            size_t metadataChanges = 0, programChanges = 0;
            CHECK(edited != NULL && params != NULL && body != NULL);
            params->next = parameter("NAME", "rOuTe");
            CHECK(params->next != NULL);
            if (variant == 2 || variant == 5) {
                params->next->next = parameter(variant == 5 ? "Queue.Type" : "parser", "custom");
                CHECK(params->next->next != NULL);
            }
            if (variant == 3 || variant == 4) {
                free(body->printable);
                body->printable = (uchar *)strdup(":msg, contains, \"changed\"");
                CHECK(body->printable != NULL);
            }
            addObject(edited, object(CNFOBJ_RULESET, params, body));
            if (variant == 5) {
                CHECK(rsReloadCandidateBuildNormalizedGraphV1(edited, &editedBuilder) == RS_RET_CONF_PARSE_ERROR);
                CHECK(editedBuilder == NULL);
                CHECK(rsReloadCandidateSourceBegin() == RS_RET_OK);
                rsReloadCandidateSourceCaptureObject(edited->head->object);
                CHECK(rsReloadCandidateSourceFinish(&editedBuilder) == RS_RET_NOT_IMPLEMENTED);
                CHECK(editedBuilder == NULL);
            } else {
                CHECK(rsReloadCandidateBuildNormalizedGraphV1(edited, &editedBuilder) == RS_RET_OK);
                CHECK(rsReloadNormalizedGraphBuilderV1GetGraph(editedBuilder, &editedGraph) == RS_RET_OK);
                CHECK(rsReloadReportBuildV1(&sourceGraph, &editedGraph, &report) == RS_RET_OK);
                for (size_t row = 0; row < report->entryCount; ++row) {
                    const rsReloadReportEntryV1_t *entry = &report->entries[row];
                    if (entry->diffKind == RS_RELOAD_DIFF_UNCHANGED) continue;
                    CHECK(entry->diffKind == RS_RELOAD_DIFF_MODIFIED);
                    if (entry->objectKind == RS_RELOAD_OBJ_RULESET_METADATA)
                        ++metadataChanges;
                    else if (entry->objectKind == RS_RELOAD_OBJ_RULESET)
                        ++programChanges;
                    else
                        CHECK(0);
                }
                CHECK(metadataChanges == (size_t)(variant == 1 || variant == 2 || variant == 4));
                CHECK(programChanges == (size_t)(variant == 3 || variant == 4));
                CHECK(report->invalidCount == 0 && report->addedCount == 0 && report->removedCount == 0);
                CHECK(rsReloadCandidateCheckRulesetOnlyReportV1(report) ==
                      (metadataChanges ? RS_RET_NOT_IMPLEMENTED : RS_RET_OK));
            }
            rsReloadReportDestructV1(&report);
            rsReloadNormalizedGraphBuilderV1Destruct(&editedBuilder);
            rsReloadCandidateDestruct(&edited);
        }
        rsReloadNormalizedGraphBuilderV1Destruct(&source);
        cnfstmtDestructLst(active->script);
        cnfobjDestruct(active);
    }

    /* Main queue is a singleton runtime resource. Two syntax objects cannot
     * be normalized into an unambiguous report-only candidate. */
    candidate = calloc(1, sizeof(*candidate));
    builder = NULL;
    CHECK(candidate != NULL);
    addObject(candidate, object(CNFOBJ_MAINQ, NULL, NULL));
    addObject(candidate, object(CNFOBJ_MAINQ, NULL, NULL));
    CHECK(!constructionFailed);
    CHECK(rsReloadCandidateBuildNormalizedGraphV1(candidate, &builder) == RS_RET_CONF_PARSE_ERROR);
    CHECK(builder == NULL);
    rsReloadCandidateDestruct(&candidate);
    puts("reload candidate graph tests passed");
    return 0;
}
