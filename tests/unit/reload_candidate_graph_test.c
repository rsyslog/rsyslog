/*
 * Unit coverage for the private reload-candidate graph producer. The oracle
 * checks structural-only graph behavior without parsing, modules, a daemon,
 * or activation: default fragments merge, ruleset identities fold, secrets
 * never appear in digests, duplicate parameter keys fail, fixed imtcp endpoint
 * identities are canonical and collision-safe, and enumeration is deterministic.
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
static struct nvlst *parameter(const char *name, const char *value);

/* Parser-facing stubs let the unit drive classification without bringing up
 * either frontend. */
int yyparse(void) {
    if (parserCaptureError != RS_RET_OK) rsReloadCandidateNoteError(parserCaptureError);
    return parserResult;
}
int cnfSetLexFile(const char __attribute__((unused)) * path) {
    return 0;
}
int cnfHasPendingBuffers(void) {
    return 0;
}
void cnfResetParser(void) {}
void cnfClearFatalParseError(void) {}
rsRetVal cnfTakeFatalParseError(void) {
    return RS_RET_OK;
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

static rsRetVal imtcpGateResult(const rsReloadCandidate_t *const activeCatalog,
                                const rsReloadCandidate_t *const candidate,
                                const rsReloadObjectKind_t kind,
                                const rsReloadDiffKind_t diff,
                                const char *const identity) {
    rsReloadReportEntryV1_t entry = {.objectKind = kind, .diffKind = diff, .identity = (char *)identity};
    rsReloadReportV1_t report = {.version = RS_RELOAD_REPORT_V1,
                                 .structSize = sizeof(report),
                                 .entryCount = 1,
                                 .entryStride = sizeof(entry),
                                 .entries = &entry};
    return rsReloadCandidateCheckRulesetImtcpReportV1(activeCatalog, candidate, &report);
}

static struct nvlst *parameterPair(const char *const firstName,
                                   const char *const firstValue,
                                   const char *const secondName,
                                   const char *const secondValue) {
    struct nvlst *const first = parameter(firstName, firstValue);
    if (first != NULL) first->next = parameter(secondName, secondValue);
    return first;
}

struct nvlst *nvlstNewStr(es_str_t *value) {
    struct nvlst *node = calloc(1, sizeof(*node));
    if (node != NULL) {
        node->val.datatype = 'S';
        node->val.d.estr = value;
    }
    return node;
}

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
            cnfstmtDestructLst(list->d.s_propfilt.t_else);
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
    result->d.s_propfilt.t_else = elseBranch;
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
    CHECK(observed.count == 8);
    CHECK(!strcmp(observed.nodes[0]->identity, "global"));
    CHECK(!strncmp(observed.nodes[0]->fingerprint, "sha256:", strlen("sha256:")));
    CHECK(strstr(observed.nodes[0]->fingerprint, "supersecret") == NULL);
    CHECK(!strcmp(observed.nodes[1]->identity, "input:imtcp:anonymous:1"));
    CHECK(!strcmp(observed.nodes[2]->identity, "input:imtcp:anonymous:2"));
    CHECK(!strcmp(observed.nodes[3]->identity, "ruleset:default"));
    CHECK(!strcmp(observed.nodes[4]->identity, "ruleset:mixed"));
    CHECK(!strcmp(observed.nodes[5]->identity, "action:first"));
    CHECK(!strcmp(observed.nodes[6]->identity, "action:named-action"));
    CHECK(!strcmp(observed.nodes[7]->identity, "action:second"));

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

    /* Core graph identities remain module-neutral. Anonymous imtcp inputs use
     * stable per-type ordinals; the imtcp source lowerer separately derives
     * effective endpoint keys from module and input defaults. */
    {
        rsReloadCandidate_t *endpoints = calloc(1, sizeof(*endpoints));
        rsReloadNormalizedGraphBuilderV1_t *endpointBuilder = NULL;
        rsReloadNormalizedGraphV1_t endpointGraph;
        observed_t endpointObserved = {0};
        struct nvlst *fixed = parameter("type", "ImTcP");
        struct nvlst *dynamic = parameter("type", "imtcp");
        struct nvlst *portFile = parameter("type", "imtcp");
        CHECK(endpoints != NULL && fixed != NULL && dynamic != NULL && portFile != NULL);
        fixed->next = parameter("port", "0514");
        fixed->next->next = parameter("address", "2001:db8::1");
        fixed->next->next->next = parameter("networknamespace", "blue:prod");
        dynamic->next = parameter("port", "0");
        portFile->next = parameter("port", "10514");
        portFile->next->next = parameter("listenportfilename", "dynamic.port");
        CHECK(fixed->next != NULL && fixed->next->next != NULL && fixed->next->next->next != NULL &&
              dynamic->next != NULL && portFile->next != NULL && portFile->next->next != NULL);
        addObject(endpoints, object(CNFOBJ_INPUT, fixed, NULL));
        addObject(endpoints, object(CNFOBJ_TPL, parameter("name", "unrelated"), NULL));
        addObject(endpoints, object(CNFOBJ_INPUT, dynamic, NULL));
        addObject(endpoints, object(CNFOBJ_INPUT, portFile, NULL));
        CHECK(rsReloadCandidateBuildNormalizedGraphV1(endpoints, &endpointBuilder) == RS_RET_OK);
        CHECK(rsReloadNormalizedGraphBuilderV1GetGraph(endpointBuilder, &endpointGraph) == RS_RET_OK);
        CHECK(endpointGraph.enumerate(endpointGraph.context, observe, &endpointObserved) == RS_RET_OK);
        CHECK(findObserved(&endpointObserved, "input:imtcp:anonymous:1") != NULL);
        CHECK(findObserved(&endpointObserved, "input:imtcp:anonymous:2") != NULL);
        CHECK(findObserved(&endpointObserved, "input:imtcp:anonymous:3") != NULL);
        rsReloadNormalizedGraphBuilderV1Destruct(&endpointBuilder);
        rsReloadCandidateDestruct(&endpoints);
    }

    /* Both branches of legacy priority/property filters are part of the
     * ruleset program and action graph. A change confined to t_else must
     * therefore change the ruleset fingerprint and expose that branch's
     * action node instead of being reported as a no-op. */
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
        CHECK(strcmp(firstRuleset->fingerprint, secondRuleset->fingerprint));
        CHECK(findObserved(&firstObserved, "action:else-first") != NULL);
        CHECK(findObserved(&firstObserved, "action:then-action") != NULL);
        CHECK(findObserved(&secondObserved, "action:else-second") != NULL);
        CHECK(findObserved(&secondObserved, "action:then-action") != NULL);
        priorityFirstRuleset = findObserved(&priorityFirstObserved, "ruleset:priority-branches");
        prioritySecondRuleset = findObserved(&prioritySecondObserved, "ruleset:priority-branches");
        CHECK(priorityFirstRuleset != NULL && prioritySecondRuleset != NULL);
        CHECK(strcmp(priorityFirstRuleset->fingerprint, prioritySecondRuleset->fingerprint));
        CHECK(findObserved(&priorityFirstObserved, "action:priority-else-first") != NULL);
        CHECK(findObserved(&priorityFirstObserved, "action:priority-then") != NULL);
        CHECK(findObserved(&prioritySecondObserved, "action:priority-else-second") != NULL);
        CHECK(findObserved(&prioritySecondObserved, "action:priority-then") != NULL);
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
        CHECK(findObserved(&baseObserved, "input:imtcp:anonymous:1") != NULL);
        CHECK(findObserved(&unrelatedObserved, "input:imtcp:anonymous:1") != NULL);
        CHECK(findObserved(&baseObserved, "action:ruleset:default:anonymous:1") != NULL);
        CHECK(findObserved(&unrelatedObserved, "action:ruleset:default:anonymous:1") != NULL);
        CHECK(!strcmp(findObserved(&baseObserved, "action:ruleset:default:anonymous:1")->fingerprint,
                      findObserved(&unrelatedObserved, "action:ruleset:default:anonymous:1")->fingerprint));
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
        CHECK(findObserved(&sourceObserved, "input:imtcp:anonymous:1") != NULL);
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

    /* The broadened materializer gate is authorized only for candidate-side
     * imtcp objects already classified LIVE_SWAP. Existing imtcp inputs and
     * newly added and active-catalog removed imtcp inputs are accepted; other
     * modules/inputs and module additions remain fail-closed. */
    candidate = calloc(1, sizeof(*candidate));
    rsReloadCandidate_t *activeCatalog = calloc(1, sizeof(*activeCatalog));
    CHECK(candidate != NULL && activeCatalog != NULL);
    addObject(candidate, object(CNFOBJ_MODULE, parameter("load", "imtcp"), NULL));
    addObject(candidate, object(CNFOBJ_MODULE, parameter("load", "omfile"), NULL));
    addObject(candidate, object(CNFOBJ_INPUT, parameterPair("type", "imtcp", "name", "tcp1"), NULL));
    addObject(candidate, object(CNFOBJ_INPUT, parameterPair("type", "imudp", "name", "udp1"), NULL));
    addObject(activeCatalog, object(CNFOBJ_MODULE, parameter("load", "imtcp"), NULL));
    addObject(activeCatalog, object(CNFOBJ_INPUT, parameterPair("type", "imtcp", "name", "retired"), NULL));
    CHECK(!constructionFailed);
    CHECK(imtcpGateResult(activeCatalog, candidate, RS_RELOAD_OBJ_MODULE, RS_RELOAD_DIFF_MODIFIED, "module:imtcp") ==
          RS_RET_OK);
    CHECK(imtcpGateResult(activeCatalog, candidate, RS_RELOAD_OBJ_INPUT, RS_RELOAD_DIFF_MODIFIED, "input:tcp1") ==
          RS_RET_OK);
    CHECK(imtcpGateResult(activeCatalog, candidate, RS_RELOAD_OBJ_MODULE, RS_RELOAD_DIFF_MODIFIED, "module:omfile") ==
          RS_RET_NOT_IMPLEMENTED);
    CHECK(imtcpGateResult(activeCatalog, candidate, RS_RELOAD_OBJ_INPUT, RS_RELOAD_DIFF_MODIFIED, "input:udp1") ==
          RS_RET_NOT_IMPLEMENTED);
    CHECK(imtcpGateResult(activeCatalog, candidate, RS_RELOAD_OBJ_INPUT, RS_RELOAD_DIFF_REMOVED, "input:udp1") ==
          RS_RET_NOT_IMPLEMENTED);
    CHECK(imtcpGateResult(activeCatalog, candidate, RS_RELOAD_OBJ_MODULE, RS_RELOAD_DIFF_ADDED, "module:imtcp") ==
          RS_RET_NOT_IMPLEMENTED);
    CHECK(imtcpGateResult(activeCatalog, candidate, RS_RELOAD_OBJ_INPUT, RS_RELOAD_DIFF_ADDED, "input:tcp1") ==
          RS_RET_OK);
    CHECK(imtcpGateResult(activeCatalog, candidate, RS_RELOAD_OBJ_INPUT, RS_RELOAD_DIFF_REMOVED, "input:retired") ==
          RS_RET_OK);
    CHECK(imtcpGateResult(activeCatalog, candidate, RS_RELOAD_OBJ_INPUT, RS_RELOAD_DIFF_REMOVED, "input:tcp1") ==
          RS_RET_NOT_IMPLEMENTED);
    rsReloadCandidateDestruct(&activeCatalog);
    rsReloadCandidateDestruct(&candidate);
    puts("reload candidate graph tests passed");
    return 0;
}
