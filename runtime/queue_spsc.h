/* SPDX-License-Identifier: Apache-2.0 */

/*
 * Bounded single-producer/single-consumer pointer queue.
 *
 * The caller owns both this object and the slot storage passed to init().  A
 * queue has one producer and one consumer for its entire initialized lifetime.
 * Initialization and destruction happen while neither role is active.  The
 * queue neither allocates memory nor retains or releases message references.
 *
 * A producer publishes initialized slots with release semantics.  The consumer
 * acquires that publication before reading them, and publishes a slot release
 * only after copying its pointer to the caller.  The producer acquires that
 * release before reusing the slot.  The four mutable state blocks are spaced
 * by 128 bytes so producer and consumer state cannot share a cache line up to
 * 64 bytes wide, including the measured x86 platform, even when callers use
 * ordinary malloc()/calloc() alignment.
 */
#ifndef INCLUDED_QUEUE_SPSC_H
#define INCLUDED_QUEUE_SPSC_H

#include <stddef.h>
#include <stdint.h>

#include "typedefs.h"

#define RS_SPSC_QUEUE_CACHE_SEPARATION 128U
#define RS_SPSC_QUEUE_MAX_CAPACITY (UINT32_MAX / 2U)

typedef struct rs_spsc_queue_private_state_s {
    uint32_t sequence;
    uint32_t cached_sequence;
    uint32_t slot_cursor;
    unsigned char padding[RS_SPSC_QUEUE_CACHE_SEPARATION - (3U * sizeof(uint32_t))];
} rs_spsc_queue_private_state_t;

typedef struct rs_spsc_queue_publication_s {
    uint32_t sequence;
    unsigned char padding[RS_SPSC_QUEUE_CACHE_SEPARATION - sizeof(uint32_t)];
} rs_spsc_queue_publication_t;

typedef struct rs_spsc_queue_s {
    void **slots;
    uint32_t capacity;
    unsigned char immutable_padding[RS_SPSC_QUEUE_CACHE_SEPARATION - sizeof(void **) - sizeof(uint32_t)];

    /* Written only by the producer. */
    rs_spsc_queue_private_state_t producer;
    /* Written by the producer, acquired by the consumer. */
    rs_spsc_queue_publication_t producer_publication;
    /* Written only by the consumer. */
    rs_spsc_queue_private_state_t consumer;
    /* Written by the consumer, acquired by the producer. */
    rs_spsc_queue_publication_t consumer_publication;
} rs_spsc_queue_t;

/* Producer-only observation.  It is valid only while the caller remains the
 * queue's sole producer; it is intentionally not a cross-thread snapshot. */
uint32_t rsSpscQueueProducerFree(const rs_spsc_queue_t *queue);

/* Consumer-only observation.  It is valid only while the caller remains the
 * queue's sole consumer; it is intentionally not a cross-thread snapshot. */
uint32_t rsSpscQueueConsumerAvailable(const rs_spsc_queue_t *queue);

/* Try to publish every entry in items.  A false result leaves queue contents
 * unchanged; count == 0 succeeds without requiring an items array. */
sbool rsSpscQueueTryPush(rs_spsc_queue_t *queue, void *const *items, size_t count);

/* Remove at most maximum entries and return their actual count.  The consumer
 * obtains independent pointer ownership before making those slots reusable. */
sbool rsSpscQueueAtomicsAvailable(void);
sbool rsSpscQueueInit(rs_spsc_queue_t *queue, void **slots, uint32_t capacity);
size_t rsSpscQueuePop(rs_spsc_queue_t *queue, void **items, size_t maximum);

#endif /* INCLUDED_QUEUE_SPSC_H */
