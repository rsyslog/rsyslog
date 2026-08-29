/* reload-candidate.c
 *
 * Capture-only configuration parsing used by transactional HUP reload. The
 * normal parser is reused for RainerScript and YAML, but dispatch into
 * globals, modules, actions, queues, and inputs is suppressed. A rejected
 * candidate therefore cannot mutate the active generation or external
 * resources.
 *
 * Concurrency & Locking:
 *   The parser and capture context are main-thread-only and non-reentrant.
 *   Runtime workers never access candidate objects.
 */
#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "rsyslog.h"
#include "reload-candidate.h"
#ifdef HAVE_LIBYAML
    #include "yamlconf.h"
#endif
#include "grammar/parserif.h"
#include "grammar/rainerscript.h"

extern int yyparse(void);

typedef struct rsReloadCandidateObject_s {
    struct cnfobj *object;
    struct rsReloadCandidateObject_s *next;
} rsReloadCandidateObject_t;

struct rsReloadCandidate_s {
    rsReloadCandidateObject_t *head;
    rsReloadCandidateObject_t *tail;
    size_t objectCount;
    int parseError;
};

static rsReloadCandidate_t *captureCandidate = NULL;

static void objectDestruct(struct cnfobj *const object) {
    if (object == NULL) return;
    cnfstmtDestructLst(object->script);
    object->script = NULL;
    cnfobjDestruct(object);
}

void rsReloadCandidateDestruct(rsReloadCandidate_t **const ppCandidate) {
    rsReloadCandidateObject_t *entry;
    rsReloadCandidateObject_t *next;

    if (ppCandidate == NULL || *ppCandidate == NULL) return;
    for (entry = (*ppCandidate)->head; entry != NULL; entry = next) {
        next = entry->next;
        objectDestruct(entry->object);
        free(entry);
    }
    free(*ppCandidate);
    *ppCandidate = NULL;
}

int rsReloadCandidateCaptureActive(void) {
    return captureCandidate != NULL;
}

rsRetVal rsReloadCandidateTakeObject(struct cnfobj *const object) {
    rsReloadCandidateObject_t *entry;

    if (captureCandidate == NULL || object == NULL) return RS_RET_PARAM_ERROR;
    entry = calloc(1, sizeof(*entry));
    if (entry == NULL) return RS_RET_OUT_OF_MEMORY;
    entry->object = object;
    if (captureCandidate->tail == NULL) {
        captureCandidate->head = entry;
    } else {
        captureCandidate->tail->next = entry;
    }
    captureCandidate->tail = entry;
    ++captureCandidate->objectCount;
    return RS_RET_OK;
}

rsRetVal rsReloadCandidateTakeDefaultScript(struct cnfstmt *const script) {
    struct cnfobj *object;
    rsRetVal ret;

    if (captureCandidate == NULL || script == NULL) return RS_RET_PARAM_ERROR;
    object = cnfobjNew(CNFOBJ_RULESET, NULL);
    if (object == NULL) return RS_RET_OUT_OF_MEMORY;
    object->script = script;
    ret = rsReloadCandidateTakeObject(object);
    if (ret != RS_RET_OK) {
        object->script = NULL;
        cnfobjDestruct(object);
    }
    return ret;
}

void rsReloadCandidateNoteParseError(void) {
    if (captureCandidate != NULL) captureCandidate->parseError = 1;
}

size_t rsReloadCandidateObjectCount(const rsReloadCandidate_t *const candidate) {
    return candidate == NULL ? 0 : candidate->objectCount;
}

rsRetVal rsReloadCandidateParse(const char *const path, rsReloadCandidate_t **const ppCandidate) {
    const char *ext;
    int parseResult;
    struct stat pathStat;
    rsReloadCandidate_t *candidate = NULL;
    DEFiRet;

    if (path == NULL || ppCandidate == NULL || *ppCandidate != NULL || captureCandidate != NULL) {
        return RS_RET_PARAM_ERROR;
    }
    if (stat(path, &pathStat) != 0) return RS_RET_CONF_FILE_NOT_FOUND;
    if (!S_ISREG(pathStat.st_mode)) return RS_RET_CONF_PARSE_ERROR;
    CHKmalloc(candidate = calloc(1, sizeof(*candidate)));
    captureCandidate = candidate;
    ext = strrchr(path, '.');
#ifdef HAVE_LIBYAML
    if (ext != NULL && (!strcmp(ext, ".yaml") || !strcmp(ext, ".yml"))) {
        const rsRetVal yamlResult = yamlconf_load(path);
        parseResult = yamlResult == RS_RET_OK ? 0 : (yamlResult == RS_RET_CONF_FILE_NOT_FOUND ? 2 : 1);
        if (parseResult == 0 && cnfHasPendingBuffers()) parseResult = yyparse();
    } else {
#else
    if (ext != NULL && (!strcmp(ext, ".yaml") || !strcmp(ext, ".yml"))) {
        parseResult = 1;
    } else {
#endif
        parseResult = cnfSetLexFile(path);
        if (parseResult == 0) parseResult = yyparse();
    }
    cnfResetParser();
    captureCandidate = NULL;
    if (parseResult == 2) ABORT_FINALIZE(RS_RET_CONF_FILE_NOT_FOUND);
    if (parseResult != 0 || candidate->parseError) ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
    *ppCandidate = candidate;
    candidate = NULL;

finalize_it:
    captureCandidate = NULL;
    rsReloadCandidateDestruct(&candidate);
    RETiRet;
}
