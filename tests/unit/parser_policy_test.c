/* SPDX-License-Identifier: Apache-2.0
 * @file
 * Exercise the real daemon-linked sanitizer through its public object interface.
 * Getters change after their first read: exact output and call counts prove
 * scan/rewrite retains each sample. A rewrite-only publication deliberately
 * occurs after scan samples; independent settings are not a generation snapshot.
 * Real message construction/output/destruction runs, not a substituted sink.
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rsyslog.h"
#include "obj.h"
#include "msg.h"
#include "glbl.h"
#include "parser.h"
#include "rsconf.h"

#undef main
DEFobjStaticHelpers;
DEFobjCurrIf(parser);
DEFobjCurrIf(rsconf);

void parserPolicyTestExchangeGlobals(glbl_if_t *state);

#define CHECK(condition)                                                    \
    do {                                                                    \
        if (!(condition)) {                                                 \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
            return 1;                                                       \
        }                                                                   \
    } while (0)

enum { DROP_LF, DROP_CR, SPACE_LF, CONTROL, EIGHT_BIT, TAB, CSTYLE, PREFIX, MAX_LINE, POLICY_COUNT };
static int calls[POLICY_COUNT];
static int values[POLICY_COUNT];
static int publishRewrite;
static int publishedCStyle;
static uchar prefixValue = '#';

#define GETTER(name, index)                                          \
    static int name(rsconf_t *cnf) {                                 \
        (void)cnf;                                                   \
        return calls[index]++ == 0 ? values[index] : !values[index]; \
    }
GETTER(dropLF, DROP_LF)
GETTER(dropCR, DROP_CR)
GETTER(spaceLF, SPACE_LF)
GETTER(control, CONTROL)
GETTER(tab, TAB)
GETTER(cstyle, CSTYLE)

static int eightBit(rsconf_t *cnf) {
    (void)cnf;
    const int sampled = calls[EIGHT_BIT]++ == 0 ? values[EIGHT_BIT] : !values[EIGHT_BIT];
    if (publishRewrite) {
        values[CONTROL] = values[EIGHT_BIT] = 0;
        values[SPACE_LF] = 1;
        values[TAB] = 0;
        values[CSTYLE] = publishedCStyle;
        prefixValue = '!';
    }
    return sampled;
}

static uchar prefix(rsconf_t *cnf) {
    (void)cnf;
    return calls[PREFIX]++ == 0 ? prefixValue : '?';
}

static int maxLine(rsconf_t *cnf) {
    (void)cnf;
    ++calls[MAX_LINE];
    return 65536;
}

static int checkCase(const void *input, size_t length, const char *expected, int rewrite) {
    smsg_t *message = NULL;
    CHECK(msgConstruct(&message) == RS_RET_OK);
    MsgSetRawMsg(message, input, length);
    memset(calls, 0, sizeof(calls));
    const rsRetVal result = parser.SanitizeMsg(message);
    const int matched = result == RS_RET_OK && message->iLenRawMsg == (int)strlen(expected) &&
                        !memcmp(message->pszRawMsg, expected, strlen(expected) + 1);
    const rsRetVal destroyed = msgDestruct(&message);
    CHECK(result == RS_RET_OK && destroyed == RS_RET_OK && matched);
    for (int i = 0; i < POLICY_COUNT; ++i) CHECK(calls[i] == (i >= TAB ? rewrite : 1));
    return 0;
}

static int checkPolicies(void) {
    values[DROP_LF] = values[DROP_CR] = values[CONTROL] = values[EIGHT_BIT] = values[TAB] = 1;
    const uchar mixed[] = {'A', '\a', '\t', '\0', '\n', 0xff, '\a', '\r', '\n'};
    CHECK(checkCase(mixed, sizeof(mixed), "A#007#011#000#012#377#007", 1) == 0);
    values[CSTYLE] = 1;
    CHECK(checkCase(mixed, sizeof(mixed), "A\\a\\t\\0\\n\\xFF\\a", 1) == 0);
    values[SPACE_LF] = 1;
    values[TAB] = 0;
    CHECK(checkCase(mixed, sizeof(mixed), "A\\a\t\\0 \\xFF\\a", 1) == 0);
    values[CONTROL] = values[EIGHT_BIT] = 0;
    CHECK(checkCase(mixed, sizeof(mixed), "A\a\t\\0 \xff", 1) == 0);
    CHECK(checkCase("ASCII\r\n", 7, "ASCII", 0) == 0);
    publishRewrite = 1;
    publishedCStyle = 1;
    values[CONTROL] = values[EIGHT_BIT] = values[TAB] = 1;
    values[SPACE_LF] = values[CSTYLE] = 0;
    CHECK(checkCase(mixed, sizeof(mixed), "A\\a\t\\0\\n\\xFF\\a", 1) == 0);
    publishedCStyle = 0;
    values[CONTROL] = values[EIGHT_BIT] = values[TAB] = 1;
    values[SPACE_LF] = 0;
    values[CSTYLE] = 1;
    CHECK(checkCase(mixed, sizeof(mixed), "A!007\t!000!012!377!007", 1) == 0);
    publishRewrite = 0;

    /* Force the sanitizer's heap output path as well as its stack fast path. */
    char *largeInput = malloc(10000);
    char *largeOutput = malloc(40001);
    if (largeInput == NULL || largeOutput == NULL) {
        free(largeInput);
        free(largeOutput);
        return 1;
    }
    memset(largeInput, '\a', 10000);
    for (int i = 0; i < 10000; ++i) memcpy(largeOutput + i * 4, "#007", 4);
    largeOutput[40000] = '\0';
    values[CONTROL] = values[EIGHT_BIT] = values[TAB] = 1;
    values[SPACE_LF] = values[CSTYLE] = 0;
    prefixValue = '#';
    const int largeResult = checkCase(largeInput, 10000, largeOutput, 1);
    free(largeInput);
    free(largeOutput);
    CHECK(largeResult == 0);

    /* Zero-length guard runs before every getter and leaves the message owned. */
    smsg_t *empty = NULL;
    CHECK(msgConstruct(&empty) == RS_RET_OK);
    memset(calls, 0, sizeof(calls));
    const rsRetVal result = parser.SanitizeMsg(empty);
    const rsRetVal destroyed = msgDestruct(&empty);
    CHECK(result == RS_RET_EMPTY_MSG && destroyed == RS_RET_OK);
    for (int i = 0; i < POLICY_COUNT; ++i) CHECK(calls[i] == 0);
    return 0;
}

/* Real atomic accessors and constructor defaults complement the injected
 * getter-count oracle. The worker-free driver never aliases a live config. */
static int checkRuntimeScalars(void) {
    CHECK(rsconfGetReloadOnHUPMode(runConf) == RELOAD_ON_HUP_OFF);
    CHECK(glblGetReportChildProcessExits(runConf) == REPORT_CHILD_PROCESS_EXITS_ERRORS);
    CHECK(glblReportOversizeMessage(runConf) == 1);
    CHECK(glblGetParserDropTrailingLFOnReception(runConf) == 1);
    CHECK(glblGetParserDropTrailingCROnReception(runConf) == 0);
    for (int value = 0; value <= 1; ++value) {
        glblSetReportOversizeMessage(runConf, value);
        glblSetCompactJsonString(value);
        glblSetParserDropTrailingLFOnReception(runConf, value);
        glblSetParserDropTrailingCROnReception(runConf, value);
        CHECK(glblReportOversizeMessage(runConf) == value);
        CHECK(glblGetCompactJsonString() == value);
        CHECK(glblGetParserDropTrailingLFOnReception(runConf) == value);
        CHECK(glblGetParserDropTrailingCROnReception(runConf) == value);
    }
    for (int value = RELOAD_ON_HUP_OFF; value <= RELOAD_ON_HUP_ON; ++value) {
        glblSetReloadOnHUPMode(runConf, value);
        CHECK((int)rsconfGetReloadOnHUPMode(runConf) == value);
    }
    for (int value = REPORT_CHILD_PROCESS_EXITS_NONE; value <= REPORT_CHILD_PROCESS_EXITS_ALL; ++value) {
        glblSetReportChildProcessExits(runConf, value);
        CHECK(glblGetReportChildProcessExits(runConf) == value);
    }
    return 0;
}

int main(void) {
    const char *errorObject = "runtime";
    glbl_if_t getters = {0};
    int result = 1;
    int parserUsed = 0, configUsed = 0, swapped = 0;
    if (rsrtInit(&errorObject, &obj) != RS_RET_OK) return 1;
    if (objUse(parser, CORE_COMPONENT) != RS_RET_OK) goto cleanup;
    parserUsed = 1;
    if (objUse(rsconf, CORE_COMPONENT) != RS_RET_OK) goto cleanup;
    configUsed = 1;
    if (rsconfConstruct(&runConf) != RS_RET_OK) goto cleanup;
    if (checkRuntimeScalars() != 0) goto cleanup;
    getters.GetParserDropTrailingLFOnReception = dropLF;
    getters.GetParserDropTrailingCROnReception = dropCR;
    getters.GetParserSpaceLFOnReceive = spaceLF;
    getters.GetParserEscapeControlCharactersOnReceive = control;
    getters.GetParserEscape8BitCharactersOnReceive = eightBit;
    getters.GetParserEscapeControlCharacterTab = tab;
    getters.GetParserEscapeControlCharactersCStyle = cstyle;
    getters.GetParserControlCharacterEscapePrefix = prefix;
    getters.GetMaxLine = maxLine;
    parserPolicyTestExchangeGlobals(&getters);
    swapped = 1;
    result = checkPolicies();
cleanup:
    if (swapped) parserPolicyTestExchangeGlobals(&getters);
    if (runConf != NULL && rsconf.Destruct(&runConf) != RS_RET_OK) result = 1;
    if (configUsed) objRelease(rsconf, CORE_COMPONENT);
    if (parserUsed) objRelease(parser, CORE_COMPONENT);
    if (rsrtExit() != RS_RET_OK) result = 1;
    if (result == 0) puts("daemon-linked parser policy tests passed");
    return result;
}
