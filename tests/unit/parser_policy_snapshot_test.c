/* Exercise the real sanitizer with getters that change on a second read.
 * This deterministic publication simulation needs no scheduling race: exact
 * output plus getter counts prove scan/rewrite reuse the operation's policy.
 * Independently sampled settings are not promised to share a generation.
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "rsyslog.h"
#include "msg.h"
#include "glbl.h"

/* Only policy reads and the message-output sink are substituted. Both parsing
 * bodies below are the same private implementations compiled by the runtime.
 */
DEFobjCurrIf(glbl) rsconf_t *runConf;
int Debug = 0;
void r_dbgprintf(const char *srcname, const char *fmt, ...) {
    (void)srcname;
    (void)fmt;
}

#include "parser_sanitize_helper.h"
#include "msg_programname_helper.h"

#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) {                                                               \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
            return 1;                                                                \
        }                                                                            \
    } while (0)

enum { DROP_LF, DROP_CR, SPACE_LF, CONTROL, EIGHT_BIT, TAB, CSTYLE, PREFIX, MAX_LINE, POLICY_COUNT };
static int calls[POLICY_COUNT];
static int initial[POLICY_COUNT];
static int slashCalls;
static int permitSlash;
static int publishColdOnScan;
static int publishedCStyle;
static uchar initialPrefix = '#';

int glblGetParserPermitSlashInProgramName(rsconf_t *cnf) {
    (void)cnf;
    return slashCalls++ == 0 ? permitSlash : !permitSlash;
}

/* Every callback simulates publication after its first sample. */
#define POLICY_GETTER(name, index)                                     \
    static int name(rsconf_t *cnf) {                                   \
        (void)cnf;                                                     \
        return calls[index]++ == 0 ? initial[index] : !initial[index]; \
    }
POLICY_GETTER(dropLF, DROP_LF)
POLICY_GETTER(dropCR, DROP_CR)
POLICY_GETTER(spaceLF, SPACE_LF)
POLICY_GETTER(control, CONTROL)
static int eightBit(rsconf_t *cnf) {
    (void)cnf;
    const int sampled = calls[EIGHT_BIT]++ == 0 ? initial[EIGHT_BIT] : !initial[EIGHT_BIT];
    if (publishColdOnScan) {
        /* Publish after the scan-policy samples but before rewrite's first
         * samples. Previously sampled control/LF/8bit values must be retained.
         */
        initial[CONTROL] = initial[EIGHT_BIT] = 0;
        initial[SPACE_LF] = 1;
        initial[TAB] = 0;
        initial[CSTYLE] = publishedCStyle;
        initialPrefix = '!';
    }
    return sampled;
}
POLICY_GETTER(tab, TAB)
POLICY_GETTER(cstyle, CSTYLE)
static uchar prefix(rsconf_t *cnf) {
    (void)cnf;
    return calls[PREFIX]++ == 0 ? initialPrefix : (initialPrefix == '#' ? '!' : '#');
}
static int maxLine(rsconf_t *cnf) {
    (void)cnf;
    ++calls[MAX_LINE];
    return 4096;
}

static int check_case(const uchar *input, size_t len, const char *expected, int rewrite) {
    smsg_t msg;
    uchar raw[4096];
    memset(&msg, 0, sizeof(msg));
    memcpy(raw, input, len);
    raw[len] = '\0';
    msg.pszRawMsg = raw;
    msg.iLenRawMsg = len;
    memset(calls, 0, sizeof(calls));
    CHECK(SanitizeMsg(&msg) == RS_RET_OK);
    CHECK(msg.iLenRawMsg == (int)strlen(expected));
    CHECK(memcmp(msg.pszRawMsg, expected, strlen(expected) + 1) == 0);
    for (int i = 0; i < POLICY_COUNT; ++i) {
        CHECK(calls[i] == ((i >= TAB) ? rewrite : 1));
    }
    return 0;
}

/* Sanitizer output capture avoids initializing the message-object subsystem. */
void MsgSetRawMsg(smsg_t *const msg, const char *const raw, const size_t len) {
    memcpy(msg->pszRawMsg, raw, len + 1);
    msg->iLenRawMsg = len;
}

static int check_programname(int initialPermit, const char *expected) {
    /* Permitting slashes exercises heap-backed long program names; rejecting
     * them exercises inline short names. Both representations must match the
     * exact oracle, and heap storage is released before any failing assertion.
     */
    smsg_t msg;
    memset(&msg, 0, sizeof(msg));
    const char *tag = "app/component/worker[123]:";
    CHECK(strlen(tag) < CONF_TAG_BUFSIZE);
    memcpy(msg.TAG.szBuf, tag, strlen(tag) + 1);
    msg.iLenTAG = strlen(tag);
    permitSlash = initialPermit;
    slashCalls = 0;
    const rsRetVal ret = acquireProgramName(&msg);
    const int heapBacked = ret == RS_RET_OK && msg.iLenPROGNAME >= CONF_PROGNAME_BUFSIZE;
    const uchar *programname = heapBacked ? msg.PROGNAME.ptr : msg.PROGNAME.szBuf;
    const int nameMatches = ret == RS_RET_OK && strcmp((const char *)programname, expected) == 0;
    if (heapBacked) free(msg.PROGNAME.ptr);
    CHECK(ret == RS_RET_OK);
    CHECK(slashCalls == 1);
    CHECK(msg.iLenPROGNAME == (int)strlen(expected));
    CHECK(nameMatches);
    return 0;
}

int main(void) {
    glbl.GetParserDropTrailingLFOnReception = dropLF;
    glbl.GetParserDropTrailingCROnReception = dropCR;
    glbl.GetParserSpaceLFOnReceive = spaceLF;
    glbl.GetParserEscapeControlCharactersOnReceive = control;
    glbl.GetParserEscape8BitCharactersOnReceive = eightBit;
    glbl.GetParserEscapeControlCharacterTab = tab;
    glbl.GetParserEscapeControlCharactersCStyle = cstyle;
    glbl.GetParserControlCharacterEscapePrefix = prefix;
    glbl.GetMaxLine = maxLine;

    initial[DROP_LF] = initial[DROP_CR] = initial[CONTROL] = initial[EIGHT_BIT] = initial[TAB] = 1;
    const uchar mixed[] = {'A', '\a', '\t', '\0', '\n', 0xff, '\a', '\r', '\n'};
    CHECK(check_case(mixed, sizeof(mixed), "A#007#011#000#012#377#007", 1) == 0);
    initial[CSTYLE] = 1;
    CHECK(check_case(mixed, sizeof(mixed), "A\\a\\t\\0\\n\\xFF\\a", 1) == 0);
    initial[SPACE_LF] = 1;
    initial[TAB] = 0;
    CHECK(check_case(mixed, sizeof(mixed), "A\\a\t\\0 \\xFF\\a", 1) == 0);
    initial[CONTROL] = initial[EIGHT_BIT] = 0;
    CHECK(check_case(mixed, sizeof(mixed), "A\a\t\\0 \xff", 1) == 0);
    CHECK(check_case((const uchar *)"ASCII\r\n", 7, "ASCII", 0) == 0);
    /* New cold settings coexist with retained old scan settings. Exact mixed
     * output and the one-read counts distinguish read-once first-use semantics
     * from an atomic entry snapshot or per-character policy reads.
     */
    publishColdOnScan = 1;
    publishedCStyle = 1;
    initial[CONTROL] = initial[EIGHT_BIT] = initial[TAB] = 1;
    initial[SPACE_LF] = initial[CSTYLE] = 0;
    CHECK(check_case(mixed, sizeof(mixed), "A\\a\t\\0\\n\\xFF\\a", 1) == 0);
    publishedCStyle = 0;
    initial[CONTROL] = initial[EIGHT_BIT] = initial[TAB] = 1;
    initial[SPACE_LF] = 0;
    initial[CSTYLE] = 1;
    initialPrefix = '#';
    CHECK(check_case(mixed, sizeof(mixed), "A!007\t!000!012!377!007", 1) == 0);
    publishColdOnScan = 0;
    CHECK(check_programname(1, "app/component/worker") == 0);
    CHECK(check_programname(0, "app") == 0);
    return 0;
}
