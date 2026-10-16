/* SPDX-License-Identifier: Apache-2.0
 * Unit coverage for the private reload-candidate graph producer. The oracle
 * checks structural-only graph behavior without parsing, modules, a daemon,
 * or activation: default fragments merge, ruleset identities fold, secrets
 * never appear in digests, duplicate parameter keys fail, and enumeration is
 * deterministic. An isolated elseif-marker mutation must change production
 * candidate serialization; restoring it must restore byte-identical output.
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
        if (list->val.datatype != 'S' || list->name == NULL || list->val.d.estr == NULL) {
            nvlstDestruct(head);
            return NULL;
        }
        copy = parameter((const char *)es_getBufAddr(list->name), (const char *)es_getBufAddr(list->val.d.estr));
        if (copy == NULL) {
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

void cnfstmtDestructLst(struct cnfstmt *list) {
    while (list != NULL) {
        struct cnfstmt *next = list->next;
        if (list->nodetype == S_RELOAD_ACT)
            nvlstDestruct(list->d.reload_action);
        else if (list->nodetype == S_RELOAD_PRIFILT)
            cnfstmtDestructLst(list->d.s_prifilt.t_then);
        else if (list->nodetype == S_RELOAD_PROPFILT)
            cnfstmtDestructLst(list->d.s_propfilt.t_then);
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
    CHECK(rsReloadCandidateBuildNormalizedGraphV1(candidate, &builder) == RS_RET_OK);
    CHECK(rsReloadNormalizedGraphBuilderV1GetGraph(builder, &graph) == RS_RET_OK);
    CHECK(graph.enumerate(graph.context, observe, &observed) == RS_RET_OK);
    CHECK(observed.count == 8);
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
        CHECK(rsReloadCandidateSourceFinish(&emptySourceBuilder) == RS_RET_OK);
        CHECK(rsReloadNormalizedGraphBuilderV1GetGraph(emptyCandidateBuilder, &emptyCandidateGraph) == RS_RET_OK);
        CHECK(rsReloadNormalizedGraphBuilderV1GetGraph(emptySourceBuilder, &emptySourceGraph) == RS_RET_OK);
        CHECK(emptyCandidateGraph.enumerate(emptyCandidateGraph.context, observe, &emptyCandidateObserved) ==
              RS_RET_OK);
        CHECK(emptySourceGraph.enumerate(emptySourceGraph.context, observe, &emptySourceObserved) == RS_RET_OK);
        CHECK(emptyCandidateObserved.count == 1 && emptySourceObserved.count == 1);
        CHECK(!strcmp(emptyCandidateObserved.nodes[0]->fingerprint, emptySourceObserved.nodes[0]->fingerprint));
        rsReloadNormalizedGraphBuilderV1Destruct(&emptyCandidateBuilder);
        rsReloadNormalizedGraphBuilderV1Destruct(&emptySourceBuilder);
        rsReloadCandidateDestruct(&emptyCandidate);
    }

    /* A normal-startup parser error invalidates the source mirror; no partial
     * baseline may be handed to the report-only HUP path afterwards. */
    {
        rsReloadNormalizedGraphBuilderV1_t *dirtySourceBuilder = NULL;
        CHECK(rsReloadCandidateSourceBegin() == RS_RET_OK);
        rsReloadCandidateSourceNoteError();
        CHECK(rsReloadCandidateSourceFinish(&dirtySourceBuilder) == RS_RET_NOT_IMPLEMENTED);
        CHECK(dirtySourceBuilder == NULL);
    }

    /* The startup source mirror uses the same per-type anonymous input
     * ordinal as the candidate. A named IMTCP sibling must not consume the
     * ordinal, and casing of the input type must not split that namespace. */
    {
        rsReloadNormalizedGraphBuilderV1_t *observedSourceBuilder = NULL;
        rsReloadNormalizedGraphV1_t sourceGraph;
        observed_t sourceObserved = OBSERVED_INIT;
        struct nvlst *namedInput = parameter("type", "IMTCP");
        struct cnfobj *namedObject;
        struct cnfobj *otherObject;
        struct cnfobj *anonymousObject;

        CHECK(namedInput != NULL);
        namedInput->next = parameter("name", "named-before");
        CHECK(namedInput->next != NULL);
        namedObject = object(CNFOBJ_INPUT, namedInput, NULL);
        otherObject = object(CNFOBJ_INPUT, parameter("type", "imudp"), NULL);
        anonymousObject = object(CNFOBJ_INPUT, parameter("type", "ImTcP"), NULL);
        CHECK(namedObject != NULL && otherObject != NULL && anonymousObject != NULL && otherObject->nvlst != NULL &&
              anonymousObject->nvlst != NULL);
        CHECK(rsReloadCandidateSourceBegin() == RS_RET_OK);
        rsReloadCandidateSourceCaptureObject(namedObject);
        rsReloadCandidateSourceCaptureObject(otherObject);
        rsReloadCandidateSourceCaptureObject(anonymousObject);
        CHECK(rsReloadCandidateSourceFinish(&observedSourceBuilder) == RS_RET_OK);
        CHECK(rsReloadNormalizedGraphBuilderV1GetGraph(observedSourceBuilder, &sourceGraph) == RS_RET_OK);
        CHECK(sourceGraph.enumerate(sourceGraph.context, observe, &sourceObserved) == RS_RET_OK);
        CHECK(sourceObserved.count == 3);
        CHECK(findObserved(&sourceObserved, "input:anonymous:5:imtcp:1") != NULL);
        rsReloadNormalizedGraphBuilderV1Destruct(&observedSourceBuilder);
        cnfobjDestruct(namedObject);
        cnfobjDestruct(otherObject);
        cnfobjDestruct(anonymousObject);
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
