/* SPDX-License-Identifier: Apache-2.0 */

/* imbeats sequence-number helpers.
 *
 * Copyright 2026 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *       -or-
 *       see COPYING.ASL20 in the source distribution
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef IMBEATS_SEQNUM_H
#define IMBEATS_SEQNUM_H

#include <stddef.h>
#include <stdint.h>

/* Lumberjack v2 sequence numbers are scoped to a window.
 *
 * Beats clients (go-lumber, used by Filebeat and Elastic Agent) number the
 * events of EVERY window 1..n and expect the ACK for that window to carry n.
 * Some older or hand-written senders instead continue the numbering across
 * windows on one connection (the first event of a window is the last ACKed
 * sequence number plus one). Both forms are well defined and unambiguous, so
 * both are accepted:
 *
 * - the first event of a window must carry 1 or lastAckedSeq + 1;
 * - every further event must carry the previous sequence number plus one.
 *
 * Anything else (gaps, duplicates, reordering, an arbitrary start value) is a
 * protocol error. The ACK always carries the sequence number of the last event
 * of the window. All arithmetic is uint32 modulo arithmetic, so cumulative
 * numbering may wrap from UINT32_MAX to 0.
 */
static inline int imbeats_seq_window_start_valid(const uint32_t lastAckedSeq, const uint32_t firstSeq) {
    /* widen before adding so the intended uint32 wrap is explicit */
    const uint32_t next = (uint32_t)((uint64_t)lastAckedSeq + 1u);
    return firstSeq == 1u || firstSeq == next;
}

static inline int imbeats_seq_in_window_valid(const uint32_t firstSeq,
                                              const size_t batchIndex,
                                              const uint32_t actualSeq) {
    const uint64_t expectedWide = (uint64_t)firstSeq + (uint64_t)(uint32_t)batchIndex;
    const uint32_t expected = (uint32_t)expectedWide;
    return actualSeq == expected;
}

/* Combined check for event batchIndex of a window whose first event carries
 * firstSeq. For batchIndex 0, firstSeq must equal actualSeq. */
static inline int imbeats_seq_is_expected(const uint32_t lastAckedSeq,
                                          const uint32_t firstSeq,
                                          const size_t batchIndex,
                                          const uint32_t actualSeq) {
    return imbeats_seq_window_start_valid(lastAckedSeq, firstSeq) &&
           imbeats_seq_in_window_valid(firstSeq, batchIndex, actualSeq);
}

#endif
