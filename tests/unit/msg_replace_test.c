#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rsyslog.h"
#include "msg_replace_helper.h"

#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) {                                                               \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
            return 1;                                                                \
        }                                                                            \
    } while (0)

static int test_stack_to_heap_growth(void) {
    /* Replacing a stack-backed MSG with a longer value must preserve the suffix
     * after moving storage to the heap. The exact raw message content is the
     * oracle.
     */
    static const uchar initialRawMsg[] = "prefix old suffix";
    static const uchar replacement[] = "much longer replacement";
    static const uchar expectedRawMsg[] = "prefix much longer replacement suffix";
    uchar stackBuf[CONF_RAWMSG_BUFSIZE];
    uchar *rawMsg = stackBuf;
    int lenRawMsg = sizeof(initialRawMsg) - 1;
    rsRetVal ret;

    memcpy(stackBuf, initialRawMsg, sizeof(initialRawMsg) - 1);
    ret = msgReplaceRawMsgSegment(&rawMsg, stackBuf, sizeof(stackBuf), sizeof("prefix ") - 1, sizeof("old") - 1,
                                  &lenRawMsg, replacement, sizeof(replacement) - 1);
    CHECK(ret == RS_RET_OK);
    CHECK(lenRawMsg == (int)sizeof(expectedRawMsg) - 1);
    CHECK(memcmp(rawMsg, expectedRawMsg, sizeof(expectedRawMsg) - 1) == 0);
    CHECK(memcmp(rawMsg + sizeof("prefix ") - 1 + sizeof(replacement) - 1, " suffix", sizeof(" suffix") - 1) == 0);

    if (rawMsg != stackBuf) {
        free(rawMsg);
    }

    return 0;
}

static int test_heap_realloc_growth(void) {
    /* Replacing a heap-backed MSG with a longer value must realloc when the new
     * length exceeds the current heap allocation even if it still fits inside
     * CONF_RAWMSG_BUFSIZE. The preserved suffix and final full raw message are
     * the oracle.
     */
    static const uchar initialRawMsg[] = "prefix old suffix";
    static const uchar replacement[] = "much longer replacement";
    static const uchar expectedRawMsg[] = "prefix much longer replacement suffix";
    uchar stackBuf[CONF_RAWMSG_BUFSIZE];
    uchar *rawMsg = malloc(sizeof(initialRawMsg));
    int lenRawMsg = sizeof(initialRawMsg) - 1;
    rsRetVal ret;

    CHECK(rawMsg != NULL);
    memcpy(rawMsg, initialRawMsg, sizeof(initialRawMsg) - 1);
    ret = msgReplaceRawMsgSegment(&rawMsg, stackBuf, sizeof(stackBuf), sizeof("prefix ") - 1, sizeof("old") - 1,
                                  &lenRawMsg, replacement, sizeof(replacement) - 1);
    CHECK(ret == RS_RET_OK);
    CHECK(lenRawMsg == (int)sizeof(expectedRawMsg) - 1);
    CHECK(memcmp(rawMsg, expectedRawMsg, sizeof(expectedRawMsg) - 1) == 0);
    CHECK(memcmp(rawMsg + sizeof("prefix ") - 1 + sizeof(replacement) - 1, " suffix", sizeof(" suffix") - 1) == 0);

    free(rawMsg);

    return 0;
}

int main(void) {
    int ret;

    ret = test_stack_to_heap_growth();
    if (ret != 0) {
        return ret;
    }

    return test_heap_realloc_growth();
}
