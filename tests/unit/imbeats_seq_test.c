#include "config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "plugins/imbeats/seqnum.h"

static void expect_seq(const uint32_t lastAckedSeq,
                       const size_t batchIndex,
                       const uint32_t actualSeq,
                       const int expected,
                       const char *const label) {
    const int actual = imbeats_seq_is_expected(lastAckedSeq, batchIndex, actualSeq);
    if (actual != expected) {
        fprintf(stderr, "%s: last=%u index=%zu actual=%u expected result=%d got=%d\n", label, lastAckedSeq, batchIndex,
                actualSeq, expected, actual);
        exit(1);
    }
}

int main(void) {
    /* The imbeats Lumberjack sequence check is intentionally uint32 modulo
     * arithmetic. These cases document the wraparound invariant without sending
     * a huge protocol batch to drive the live session state to UINT32_MAX. */
    expect_seq(0, 0, 1, 1, "initial sequence");
    expect_seq(1, 0, 2, 1, "next one-event batch");
    expect_seq(1, 1, 3, 1, "second event in batch");
    expect_seq(1, 0, 1, 0, "reset rejected");
    expect_seq(UINT32_MAX, 0, 0, 1, "wrap from UINT32_MAX to zero");
    expect_seq(UINT32_MAX - 1u, 0, UINT32_MAX, 1, "pre-wrap event");
    expect_seq(UINT32_MAX - 1u, 1, 0, 1, "wrap within batch");
    expect_seq(UINT32_MAX, 0, 1, 0, "wrap gap rejected");
    return 0;
}
