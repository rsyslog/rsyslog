/* SPDX-License-Identifier: Apache-2.0 */
/*
 * This file is part of the rsyslog runtime library.
 * Authors: Nelson Yen (original file), Rainer Gerhards (replacement).
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

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "perctile_ringbuf.h"

/* The historical API reports the rounded allocation size. One slot remains
 * unavailable to callers, including when the size is rounded up. */
ringbuf_t *ringbuf_new(size_t count) {
    size_t size = 1;
    ringbuf_t *rb;
    while (size < count) {
        if (size > (size_t)INT_MAX / 2) {
            return NULL;
        }
        size *= 2;
    }
    if (size < 2 || size > INT_MAX || size > SIZE_MAX / sizeof(ITEM)) {
        return NULL;
    }

    rb = calloc(1, sizeof(*rb));
    if (rb == NULL) {
        return NULL;
    }
    rb->items = calloc(size, sizeof(*rb->items));
    if (rb->items == NULL) {
        free(rb);
        return NULL;
    }
    rb->size = size;
    return rb;
}

void ringbuf_del(ringbuf_t *rb) {
    if (rb != NULL) {
        free(rb->items);
        free(rb);
    }
}

/* Advance a valid index without depending on a power-of-two mask. */
static size_t next_index(const ringbuf_t *rb, size_t index) {
    return index + 1 == rb->size ? 0 : index + 1;
}

int ringbuf_append(ringbuf_t *rb, ITEM item) {
    size_t end;
    if (rb->used == rb->size - 1) {
        return -1;
    }
    end = rb->first + rb->used;
    if (end >= rb->size) {
        end -= rb->size;
    }
    rb->items[end] = item;
    ++rb->used;
    return 0;
}

int ringbuf_append_with_overwrite(ringbuf_t *rb, ITEM item) {
    if (rb->used == rb->size - 1) {
        rb->first = next_index(rb, rb->first);
        --rb->used;
    }
    return ringbuf_append(rb, item);
}

/* This API reads only the first physically contiguous run. */
int ringbuf_read(ringbuf_t *rb, ITEM *buf, size_t count) {
    size_t n = rb->used;
    if (n > count) {
        n = count;
    }
    if (n > rb->size - rb->first) {
        n = rb->size - rb->first;
    }
    if (n != 0) {
        memcpy(buf, rb->items + rb->first, n * sizeof(*buf));
        rb->first += n;
        if (rb->first == rb->size) {
            rb->first = 0;
        }
        rb->used -= n;
    }
    return (int)n;
}

size_t ringbuf_read_to_end(ringbuf_t *rb, ITEM *buf, size_t count) {
    size_t n = ringbuf_read(rb, buf, count);
    if (n < count && rb->used != 0) {
        n += ringbuf_read(rb, buf + n, count - n);
    }
    return n;
}

bool ringbuf_peek(ringbuf_t *rb, ITEM *item) {
    if (rb->used == 0) {
        return false;
    }
    *item = rb->items[rb->first];
    return true;
}

size_t ringbuf_capacity(ringbuf_t *rb) {
    return rb->size;
}
