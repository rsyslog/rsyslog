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

static inline sbool rsSpscQueueAtomicsAvailable(void) {
#ifdef HAVE_ATOMIC_BUILTINS
    /* This queue must never turn a nominal atomic operation into an internal
     * compiler/runtime lock.  A platform without always-lock-free 32-bit
     * atomics is unsupported until it gains an explicitly reviewed backend. */
    return __atomic_always_lock_free(sizeof(uint32_t), 0) ? 1 : 0;
#else
    return 0;
#endif
}

static inline void rsSpscQueueAdvanceCursor(uint32_t *cursor, const uint32_t capacity) {
    ++(*cursor);
    if (*cursor == capacity) *cursor = 0;
}

static inline uint32_t rsSpscQueueBoundedDifference(const uint32_t newer,
                                                    const uint32_t older,
                                                    const uint32_t capacity) {
    const uint32_t difference = newer - older;

    /* This is defensive clamping for a role-local observation.  It is not a
     * global snapshot: only the producer may query free space and only the
     * consumer may query available entries. */
    return difference > capacity ? capacity : difference;
}

static inline sbool rsSpscQueueInit(rs_spsc_queue_t *queue, void **slots, const uint32_t capacity) {
    if (queue == NULL || slots == NULL || capacity == 0 || capacity > RS_SPSC_QUEUE_MAX_CAPACITY ||
        !rsSpscQueueAtomicsAvailable())
        return 0;

    queue->slots = slots;
    queue->capacity = capacity;
    queue->producer.sequence = 0;
    queue->producer.cached_sequence = 0;
    queue->producer.slot_cursor = 0;
    queue->producer_publication.sequence = 0;
    queue->consumer.sequence = 0;
    queue->consumer.cached_sequence = 0;
    queue->consumer.slot_cursor = 0;
    queue->consumer_publication.sequence = 0;
    return 1;
}

/* Producer-only observation.  It is valid only while the caller remains the
 * queue's sole producer; it is intentionally not a cross-thread snapshot. */
static inline uint32_t rsSpscQueueProducerFree(const rs_spsc_queue_t *queue) {
#ifdef HAVE_ATOMIC_BUILTINS
    uint32_t consumed;
    uint32_t used;

    if (queue == NULL || !rsSpscQueueAtomicsAvailable()) return 0;
    consumed = __atomic_load_n(&queue->consumer_publication.sequence, __ATOMIC_ACQUIRE);
    used = rsSpscQueueBoundedDifference(queue->producer.sequence, consumed, queue->capacity);
    return queue->capacity - used;
#else
    (void)queue;
    return 0;
#endif
}

/* Consumer-only observation.  It is valid only while the caller remains the
 * queue's sole consumer; it is intentionally not a cross-thread snapshot. */
static inline uint32_t rsSpscQueueConsumerAvailable(const rs_spsc_queue_t *queue) {
#ifdef HAVE_ATOMIC_BUILTINS
    uint32_t produced;

    if (queue == NULL || !rsSpscQueueAtomicsAvailable()) return 0;
    produced = __atomic_load_n(&queue->producer_publication.sequence, __ATOMIC_ACQUIRE);
    return rsSpscQueueBoundedDifference(produced, queue->consumer.sequence, queue->capacity);
#else
    (void)queue;
    return 0;
#endif
}

/* Try to publish every entry in items.  A false result leaves queue contents
 * unchanged; count == 0 succeeds without requiring an items array. */
static inline sbool rsSpscQueueTryPush(rs_spsc_queue_t *queue, void *const *items, const size_t count) {
#ifdef HAVE_ATOMIC_BUILTINS
    uint32_t used;
    size_t i;

    if (queue == NULL || !rsSpscQueueAtomicsAvailable() || (count != 0 && items == NULL) || count > queue->capacity)
        return 0;
    if (count == 0) return 1;

    used = queue->producer.sequence - queue->producer.cached_sequence;
    if (used > queue->capacity || queue->capacity - used < count) {
        queue->producer.cached_sequence = __atomic_load_n(&queue->consumer_publication.sequence, __ATOMIC_ACQUIRE);
        used = queue->producer.sequence - queue->producer.cached_sequence;
        if (used > queue->capacity || queue->capacity - used < count) return 0;
    }

    for (i = 0; i < count; ++i) {
        queue->slots[queue->producer.slot_cursor] = items[i];
        rsSpscQueueAdvanceCursor(&queue->producer.slot_cursor, queue->capacity);
    }
    queue->producer.sequence += (uint32_t)count;
    __atomic_store_n(&queue->producer_publication.sequence, queue->producer.sequence, __ATOMIC_RELEASE);
    return 1;
#else
    (void)queue;
    (void)items;
    (void)count;
    return 0;
#endif
}

/* Remove at most maximum entries and return their actual count.  The consumer
 * obtains independent pointer ownership before making those slots reusable. */
static inline size_t rsSpscQueuePop(rs_spsc_queue_t *queue, void **items, const size_t maximum) {
#ifdef HAVE_ATOMIC_BUILTINS
    uint32_t available;
    size_t count;
    size_t i;

    if (queue == NULL || !rsSpscQueueAtomicsAvailable() || (maximum != 0 && items == NULL) || maximum == 0) return 0;

    available = queue->consumer.cached_sequence - queue->consumer.sequence;
    if (available == 0) {
        queue->consumer.cached_sequence = __atomic_load_n(&queue->producer_publication.sequence, __ATOMIC_ACQUIRE);
        available = queue->consumer.cached_sequence - queue->consumer.sequence;
        if (available == 0 || available > queue->capacity) return 0;
    }
    if (available > queue->capacity) return 0;

    count = maximum < available ? maximum : available;
    for (i = 0; i < count; ++i) {
        items[i] = queue->slots[queue->consumer.slot_cursor];
        queue->slots[queue->consumer.slot_cursor] = NULL;
        rsSpscQueueAdvanceCursor(&queue->consumer.slot_cursor, queue->capacity);
    }
    queue->consumer.sequence += (uint32_t)count;
    __atomic_store_n(&queue->consumer_publication.sequence, queue->consumer.sequence, __ATOMIC_RELEASE);
    return count;
#else
    (void)queue;
    (void)items;
    (void)maximum;
    return 0;
#endif
}

#endif /* INCLUDED_QUEUE_SPSC_H */
