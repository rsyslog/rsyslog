/* SPDX-License-Identifier: Apache-2.0 */
/*
 * This file is part of the rsyslog test suite.
 * Author: Rainer Gerhards.
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
/*
 * Verify the percentile ring's public contract independently of its layout:
 * rounded size, one reserved slot, overwrite order, contiguous reads, and
 * draining across a wrap. Each read is compared with a FIFO reference model.
 */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "perctile_ringbuf.h"

int main(void) {
    ITEM expected[7] = {0};
    ITEM actual[7] = {0};
    ITEM peeked = -1;
    size_t used = 0;
    ringbuf_t *rb = ringbuf_new(7);
    assert(rb != NULL);
    assert(ringbuf_capacity(rb) == 8);
    assert(!ringbuf_peek(rb, &peeked));
    assert(ringbuf_read_to_end(rb, actual, 7) == 0);
    assert(ringbuf_read(rb, actual, 0) == 0);

    /* The fixed sequence repeatedly fills, wraps, overwrites, and drains.
     * Equality with the FIFO model is the oracle for every returned item. */
    for (unsigned i = 0; i < 10000; ++i) {
        if (i % 5 == 0) {
            size_t requested = i % 9;
            size_t n = ringbuf_read_to_end(rb, actual, requested);
            size_t want = used < requested ? used : requested;
            assert(n == want);
            for (size_t j = 0; j < n; ++j) {
                assert(actual[j] == expected[j]);
            }
            for (size_t j = n; j < used; ++j) {
                expected[j - n] = expected[j];
            }
            used -= n;
        } else {
            ITEM value = (ITEM)i - 5000;
            if (i % 3 == 0) {
                int result = ringbuf_append(rb, value);
                assert(result == (used == 7 ? -1 : 0));
                if (result == 0) {
                    expected[used++] = value;
                }
            } else {
                assert(ringbuf_append_with_overwrite(rb, value) == 0);
                if (used == 7) {
                    for (size_t j = 1; j < used; ++j) {
                        expected[j - 1] = expected[j];
                    }
                    --used;
                }
                expected[used++] = value;
            }
        }
        assert(ringbuf_peek(rb, &peeked) == (used != 0));
        if (used != 0) {
            assert(peeked == expected[0]);
        }
    }
    assert(ringbuf_read_to_end(rb, actual, 7) == used);
    for (size_t j = 0; j < used; ++j) {
        assert(actual[j] == expected[j]);
    }
    ringbuf_del(rb);

    /* A single read stops at the physical end, while the next read returns
     * the wrapped item. The returned count is the oracle for this API. */
    rb = ringbuf_new(4);
    assert(rb != NULL);
    assert(ringbuf_append(rb, 1) == 0);
    assert(ringbuf_append(rb, 2) == 0);
    assert(ringbuf_append(rb, 3) == 0);
    assert(ringbuf_read(rb, actual, 2) == 2);
    assert(ringbuf_append(rb, 4) == 0);
    assert(ringbuf_append(rb, 5) == 0);
    assert(ringbuf_read(rb, actual, 3) == 2);
    assert(actual[0] == 3 && actual[1] == 4);
    assert(ringbuf_read(rb, actual, 3) == 1);
    assert(actual[0] == 5);
    ringbuf_del(rb);

    assert(ringbuf_new(0) == NULL);
    assert(ringbuf_new(SIZE_MAX) == NULL);
    ringbuf_del(NULL);
    return 0;
}
