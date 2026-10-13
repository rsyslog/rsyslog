/* SPDX-License-Identifier: Apache-2.0 */
/* Authors: Nelson Yen (original file), Rainer Gerhards (replacement). */
#ifndef PERCTILE_RINGBUF_H
#define PERCTILE_RINGBUF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef int64_t ITEM;

typedef struct ringbuf_s {
    ITEM *items;
    size_t size;
    size_t first;
    size_t used;
} ringbuf_t;

ringbuf_t *ringbuf_new(size_t count);
void ringbuf_del(ringbuf_t *rb);
int ringbuf_append(ringbuf_t *rb, ITEM item);
int ringbuf_append_with_overwrite(ringbuf_t *rb, ITEM item);
int ringbuf_read(ringbuf_t *rb, ITEM *buf, size_t count);
size_t ringbuf_read_to_end(ringbuf_t *rb, ITEM *buf, size_t count);
bool ringbuf_peek(ringbuf_t *rb, ITEM *item);
size_t ringbuf_capacity(ringbuf_t *rb);

#endif
