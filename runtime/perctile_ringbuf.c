/*
 * This file is part of the rsyslog runtime library.
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

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>
#include "perctile_ringbuf.h"

static uint64_t min(uint64_t a, uint64_t b) {
	return a < b ? a : b;
}

/*
 * circ buf macros derived from linux/circ_buf.h
 */

/* Return count in buffer.  */
#define CIRC_CNT(head,tail,size) (((head) - (tail)) & ((size)-1))

/* Return space available, 0..size-1.  We always leave one free char
	 as a completely full buffer has head == tail, which is the same as
	 empty.  */
#define CIRC_SPACE(head,tail,size) CIRC_CNT((tail),((head)+1),(size))

/* Return count up to the end of the buffer.  Carefully avoid
	 accessing head and tail more than once, so they can change
	 underneath us without returning inconsistent results.  */
#define CIRC_CNT_TO_END(head,tail,size) \
	({int end = (size) - (tail); \
	 int n = ((head) + end) & ((size)-1); \
	 n < end ? n : end;})

/* Return space available up to the end of the buffer.  */
#define CIRC_SPACE_TO_END(head,tail,size) \
	({int end = (size) - 1 - (head); \
	 int n = (end + (tail)) & ((size)-1); \
	 n <= end ? n : end+1;})

/* Move head by size. */
#define CIRC_ADD(idx, size, offset)	(((idx) + (offset)) & ((size) - 1))

// simple use of the linux defined circular buffer.

ringbuf_t* ringbuf_new(size_t count) {
	// use nearest power of 2
	double x = ceil(log2(count));
	size_t bufsize = pow(2, x);

	ringbuf_t *rb = calloc(1, sizeof(ringbuf_t));
	// Note: count needs to be a power of 2, otherwise our macros won't work.
	ITEM *pbuf = calloc(bufsize, sizeof(ITEM));
	rb->cb.buf = pbuf;
	rb->cb.head = rb->cb.tail = 0;
	rb->size = bufsize;

	return rb;
}

void ringbuf_del(ringbuf_t *rb) {
	if (rb) {
		if (rb->cb.buf) {
			free(rb->cb.buf);
		}
		free(rb);
	}
}

int ringbuf_append(ringbuf_t *rb, ITEM item) {
	// lock it and add
	int head = rb->cb.head,
			tail = rb->cb.tail;

	if (!CIRC_SPACE(head, tail, rb->size)) {
		return -1;
	} else {
		/* insert item into buffer */
		rb->cb.buf[head] = item;
		// move head
		rb->cb.head = CIRC_ADD(head, rb->size, 1);
	}
	return 0;
}

int ringbuf_append_with_overwrite(ringbuf_t *rb, ITEM item) {
	int head = rb->cb.head,
			tail = rb->cb.tail;
	int ret = 0;

	if (!CIRC_SPACE(head, tail, rb->size)) {
		rb->cb.tail = CIRC_ADD(tail, rb->size, 1);
	}
	ret = ringbuf_append(rb, item);
	assert(ret == 0); // we shouldn't fail due to no space.
	return ret;
}

int ringbuf_read(ringbuf_t *rb, ITEM *buf, size_t count) {
	int head = rb->cb.head,
			tail = rb->cb.tail;
	size_t copy_size = 0;

	if (!CIRC_CNT(head, tail, rb->size)) {
		return 0;
	}

	// copy to end of buffer
	copy_size = min((size_t)CIRC_CNT_TO_END(head, tail, rb->size), count);
	memcpy(buf, rb->cb.buf+tail, copy_size*sizeof(ITEM));

	rb->cb.tail = CIRC_ADD(rb->cb.tail, rb->size, copy_size);
	return copy_size;
}

size_t ringbuf_read_to_end(ringbuf_t *rb, ITEM *buf, size_t count) {
	size_t nread = 0;
	nread += ringbuf_read(rb, buf, count);
	if (nread == 0) {
		return nread;
	}
	// read the rest if buf circled around
	nread += ringbuf_read(rb, buf + nread, count - nread);
	assert(nread <= count);
	return nread;
}

bool ringbuf_peek(ringbuf_t *rb, ITEM *item) {
	if (CIRC_CNT(rb->cb.head, rb->cb.tail, rb->size) == 0) {
		return false;
	}

	*item = rb->cb.buf[rb->cb.tail];
	return true;
}

size_t ringbuf_capacity(ringbuf_t *rb) {
	return rb->size;
}

/* ---- RINGBUFFER TESTS ---- */
void ringbuf_init_test(void) {
	size_t count = 4;
	ringbuf_t *rb = ringbuf_new(count);
	// verify ringbuf empty state
	assert(rb->cb.head == 0);
	assert(rb->cb.tail == 0);
	assert(rb->size == 4);
	ringbuf_del(rb);
}

void ringbuf_simple_test(void) {
	size_t count = 3;
	ITEM item = 0;
	ringbuf_t *rb = ringbuf_new(count);
	assert(ringbuf_append(rb, 1) == 0);
	assert(ringbuf_append(rb, 2) == 0);
	assert(ringbuf_append(rb, 3) == 0);

	item = 0;
	assert(ringbuf_peek(rb, &item) == 0);
	assert(item == 1);
}

void ringbuf_append_test(void) {
	size_t count = 4;
	ringbuf_t *rb = ringbuf_new(count);

	assert(ringbuf_append(rb, 1) == 0);
	assert(ringbuf_append(rb, 2) == 0);
	assert(ringbuf_append(rb, 3) == 0);

	// check state of ringbuffer:
	// {1, 2, 3, X}
	//  T        H
	assert(rb->cb.buf[0] == 1);
	assert(rb->cb.buf[1] == 2);
	assert(rb->cb.buf[2] == 3);
	assert(rb->cb.buf[3] == 0);
	assert(rb->cb.head == 3);
	assert(rb->cb.tail == 0);

	ringbuf_del(rb);
}

void ringbuf_append_wrap_test(void) {
	size_t count = 4;
	ITEM item = 0;

	ringbuf_t *rb = ringbuf_new(count);
	assert(ringbuf_append(rb, 1) == 0);
	assert(ringbuf_append(rb, 2) == 0);
	assert(ringbuf_append(rb, 3) == 0);
	assert(ringbuf_append(rb, 4) != 0);

	// check state of ringbuffer:
	// {1, 2, 3, X}
	//  T        H
	assert(rb->cb.buf[0] == 1);
	assert(rb->cb.buf[1] == 2);
	assert(rb->cb.buf[2] == 3);
	assert(rb->cb.head == 3);
	assert(rb->cb.tail == 0);

	item = 0;
	assert(ringbuf_read(rb, &item, 1) == 1);
	assert(item == 1);

	assert(ringbuf_append(rb, 4) == 0);

	// state of ringbuffer:
	// {1, 2, 3, 4}
	//  H  T
	assert(rb->cb.buf[0] == 1);
	assert(rb->cb.buf[1] == 2);
	assert(rb->cb.buf[2] == 3);
	assert(rb->cb.buf[3] == 4);
	assert(rb->cb.head == 0);
	assert(rb->cb.tail == 1);

	item = 0;
	assert(ringbuf_read(rb, &item, 1) == 1);
	assert(item == 2);

	// test wraparound
	assert(ringbuf_append(rb, 5) == 0);

	// state of ringbuffer:
	// {1, 2, 3, 4}
	//     H  T
	assert(rb->cb.buf[0] == 5);
	assert(rb->cb.buf[1] == 2);
	assert(rb->cb.buf[2] == 3);
	assert(rb->cb.buf[3] == 4);
	assert(rb->cb.head == 1);
	assert(rb->cb.tail == 2);

	ringbuf_del(rb);
}

void ringbuf_append_overwrite_test(void) {
	size_t count = 4;
	ringbuf_t *rb = ringbuf_new(count);
	assert(ringbuf_append_with_overwrite(rb, 1) == 0);
	assert(ringbuf_append_with_overwrite(rb, 2) == 0);
	assert(ringbuf_append_with_overwrite(rb, 3) == 0);
	assert(ringbuf_append_with_overwrite(rb, 4) == 0);

	// check state of ringbuffer:
	// {1, 2, 3, 4}
	//  H  T
	assert(rb->cb.buf[0] == 1);
	assert(rb->cb.buf[1] == 2);
	assert(rb->cb.buf[2] == 3);
	assert(rb->cb.buf[3] == 4);
	assert(rb->cb.head == 0);
	assert(rb->cb.tail == 1);

	assert(ringbuf_append_with_overwrite(rb, 5) == 0);
	// check state of ringbuffer:
	// {5, 2, 3, 4}
	//     H  T
	assert(rb->cb.buf[0] == 5);
	assert(rb->cb.buf[1] == 2);
	assert(rb->cb.buf[2] == 3);
	assert(rb->cb.buf[3] == 4);
	assert(rb->cb.head == 1);
	assert(rb->cb.tail == 2);

	ringbuf_del(rb);
}

void ringbuf_read_test(void) {
	size_t count = 4;
	ringbuf_t *rb = ringbuf_new(count);
	ITEM item_array[3];
	ITEM expects[] = {1, 2, 3};
	ITEM expects2[] = {4};

	assert(ringbuf_append_with_overwrite(rb, 1) == 0);
	assert(ringbuf_append_with_overwrite(rb, 2) == 0);
	assert(ringbuf_append_with_overwrite(rb, 3) == 0);

	assert(ringbuf_read(rb, item_array, count) == 3);
	assert(memcmp(item_array, expects, 3) == 0);

	// check state of ringbuffer:
	// {X, X, X, X}
	//           H
	//           T
	assert(rb->cb.head == 3);
	assert(rb->cb.tail == 3);

	assert(ringbuf_append_with_overwrite(rb, 4) == 0);
	assert(ringbuf_read(rb, item_array, count) == 1);
	assert(memcmp(item_array, expects2, 1) == 0);
	assert(rb->cb.head == 0);
	assert(rb->cb.tail == 0);

	ringbuf_del(rb);
}

void ringbuf_read_to_end_test(void) {
	size_t count = 4;
	ringbuf_t *rb = ringbuf_new(count);
	ITEM item_array[3];
	ITEM expects[] = {1, 2, 3};
	ITEM expects2[] = {4};

	assert(ringbuf_append_with_overwrite(rb, 1) == 0);
	assert(ringbuf_append_with_overwrite(rb, 2) == 0);
	assert(ringbuf_append_with_overwrite(rb, 3) == 0);

	// state of ringbuffer:
	// {1, 2, 3, X}
	//  T        H
	assert(ringbuf_read_to_end(rb, item_array, 3) == 3);
	assert(memcmp(item_array, expects, 3) == 0);

	// check state of ringbuffer:
	// {1, 2, 3, X}
	//           H
	//           T
	assert(rb->cb.head == 3);
	assert(rb->cb.tail == 3);

	assert(ringbuf_append_with_overwrite(rb, 4) == 0);
	// state of ringbuffer:
	// {1, 2, 3, 4}
	//  H        T
	assert(ringbuf_read_to_end(rb, item_array, count) == 1);

	// check state of ringbuffer (empty):
	// {1, 2, 3, 4}
	//  H
	//  T
	assert(memcmp(item_array, expects2, 1) == 0);
	assert(rb->cb.head == 0);
	assert(rb->cb.tail == 0);

	ringbuf_del(rb);
}
/* ---- END RINGBUFFER TESTS ---- */

