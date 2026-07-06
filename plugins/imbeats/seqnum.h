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

static inline int imbeats_seq_is_expected(const uint32_t lastAckedSeq,
                                          const size_t batchIndex,
                                          const uint32_t actualSeq) {
    const uint64_t expectedWide = (uint64_t)lastAckedSeq + (uint64_t)(uint32_t)batchIndex + 1u;
    const uint32_t expected = (uint32_t)expectedWide;
    return actualSeq == expected;
}

#endif
