/*
 * Exercise the real PRI, RFC 3164, and RFC 5424 parser implementations with
 * arbitrary byte strings. Each parser receives an independent message so one
 * parser's field mutations cannot mask failures in the other.
 */
#include "config.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rsyslog.h"
#include "msg.h"
#include "parser.h"
#include "pmrfc3164.h"
#include "pmrfc5424.h"
#include "rsconf.h"

DEFobjCurrIf(obj);

static void fuzzAbortOnError(const rsRetVal ret) {
    if (ret != RS_RET_OK) abort();
}

static void fuzzCleanup(void) {
    pmrfc5424FuzzExit();
    pmrfc3164FuzzExit();
    fuzzAbortOnError(rsrtExit());
}

static void fuzzOneParser(const uint8_t *data, const size_t size, rsRetVal (*parseFn)(smsg_t *)) {
    smsg_t *msg = NULL;
    rsRetVal ret;

    fuzzAbortOnError(msgConstruct(&msg));
    MsgSetRawMsg(msg, (const char *)data, size);
    msg->msgFlags = NEEDS_PARSING | PARSE_HOSTNAME;
    fuzzAbortOnError(parserParsePRI(msg));

    ret = parseFn(msg);
    if (ret == RS_RET_OK) {
        if (msg->offAfterPRI < 0 || msg->offAfterPRI > msg->iLenRawMsg || msg->offMSG < 0 ||
            msg->offMSG > msg->iLenRawMsg) {
            abort();
        }
    }

    fuzzAbortOnError(msgDestruct(&msg));
}

int LLVMFuzzerInitialize(int *argc, char ***argv);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int LLVMFuzzerInitialize(int *argc __attribute__((unused)), char ***argv) {
    const char *errObj = "rsyslog runtime";
    const char *slash;
    static char modulePath[PATH_MAX];
    int len;

    slash = strrchr((*argv)[0], '/');
    if (slash == NULL) {
        len = snprintf(modulePath, sizeof(modulePath), "runtime/.libs");
    } else {
        len = snprintf(modulePath, sizeof(modulePath), "%.*s/../runtime/.libs", (int)(slash - (*argv)[0]), (*argv)[0]);
    }
    if (len < 0 || (size_t)len >= sizeof(modulePath) || setenv("RSYSLOG_MODDIR", modulePath, 1) != 0) abort();

    fuzzAbortOnError(rsrtInit(&errObj, &obj));
    fuzzAbortOnError(pmrfc3164FuzzInit());
    fuzzAbortOnError(pmrfc5424FuzzInit());
    if (atexit(fuzzCleanup) != 0) abort();
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, const size_t size) {
    if (size > INT_MAX) return 0;

    fuzzOneParser(data, size, pmrfc3164FuzzParse);
    fuzzOneParser(data, size, pmrfc5424FuzzParse);
    return 0;
}
