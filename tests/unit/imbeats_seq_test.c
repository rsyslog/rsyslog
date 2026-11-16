#include "config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "plugins/imbeats/seqnum.h"

static void expect_seq(const uint32_t lastAckedSeq,
                       const uint32_t firstSeq,
                       const size_t batchIndex,
                       const uint32_t actualSeq,
                       const int expected,
                       const char *const label) {
    const int actual = imbeats_seq_is_expected(lastAckedSeq, firstSeq, batchIndex, actualSeq);
    if (actual != expected) {
        fprintf(stderr, "%s: last=%u first=%u index=%zu actual=%u expected result=%d got=%d\n", label, lastAckedSeq,
                firstSeq, batchIndex, actualSeq, expected, actual);
        exit(1);
    }
}

int main(void) {
    /* Lumberjack v2 sequence numbers are scoped to one window. Beats clients
     * (go-lumber) number every window 1..n; cumulative senders continue from
     * the last ACKed sequence number. Both are accepted, everything else is
     * rejected. The arithmetic is uint32 modulo arithmetic; these cases
     * document the wraparound invariant without sending a huge protocol batch
     * to drive the live session state to UINT32_MAX. */
    expect_seq(0, 1, 0, 1, 1, "initial sequence");
    expect_seq(0, 1, 2, 3, 1, "third event of first window");
    expect_seq(0, 2, 0, 2, 0, "first window not starting at 1 rejected");
    expect_seq(0, 0, 0, 0, 0, "first window starting at 0 rejected");

    /* per-window numbering, as sent by Beats clients */
    expect_seq(1, 1, 0, 1, 1, "per-window restart after one-event window");
    expect_seq(50, 1, 0, 1, 1, "per-window restart after 50-event window");
    expect_seq(50, 1, 49, 50, 1, "last event of restarted window");
    expect_seq(50, 1, 1, 3, 0, "gap inside restarted window rejected");
    expect_seq(50, 1, 1, 1, 0, "duplicate inside restarted window rejected");

    /* cumulative numbering, kept for existing senders */
    expect_seq(1, 2, 0, 2, 1, "next one-event cumulative batch");
    expect_seq(1, 2, 1, 3, 1, "second event in cumulative batch");
    expect_seq(4, 5, 3, 8, 1, "last event of cumulative batch");
    expect_seq(4, 6, 0, 6, 0, "cumulative gap at window start rejected");
    expect_seq(4, 4, 0, 4, 0, "replay of last acked sequence rejected");
    expect_seq(4, 3, 0, 3, 0, "arbitrary start rejected");
    expect_seq(4, 5, 1, 7, 0, "gap inside cumulative batch rejected");
    expect_seq(4, 5, 2, 6, 0, "reordering inside cumulative batch rejected");

    /* uint32 wraparound */
    expect_seq(UINT32_MAX, 0, 0, 0, 1, "wrap from UINT32_MAX to zero");
    expect_seq(UINT32_MAX - 1u, UINT32_MAX, 0, UINT32_MAX, 1, "pre-wrap event");
    expect_seq(UINT32_MAX - 1u, UINT32_MAX, 1, 0, 1, "wrap within batch");
    expect_seq(UINT32_MAX - 1u, UINT32_MAX, 2, 1, 1, "continue after wrap within batch");
    expect_seq(UINT32_MAX, 1, 0, 1, 1, "per-window restart after UINT32_MAX");
    expect_seq(UINT32_MAX, 2, 0, 2, 0, "wrap gap rejected");
    return 0;
}
