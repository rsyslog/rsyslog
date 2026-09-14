/* SPDX-License-Identifier: Apache-2.0 */

/*
 * Bounded single-producer/single-consumer pointer queue implementation.
 *
 * The public contract, ownership rules, and acquire/release publication order
 * are documented in queue_spsc.h.  This source file intentionally keeps the
 * nontrivial batch loops out of callers compiled with -Werror=inline.
 */
#include "config.h"

#include "queue_spsc.h"

static void rsSpscQueueAdvanceCursor(uint32_t *cursor, const uint32_t capacity) {
    ++(*cursor);
    if (*cursor == capacity) *cursor = 0;
}

/* Keep the intentional uint32_t sequence wrap explicit under the unsigned
 * overflow sanitizer.  The arithmetic in the widened type is bounded by less
 * than twice UINT32_MAX, and the cast expresses the ring sequence domain. */
static uint32_t rsSpscQueueSequenceAdd(const uint32_t sequence, const uint32_t count) {
    return (uint32_t)((uint64_t)sequence + count);
}

static uint32_t rsSpscQueueSequenceDifference(const uint32_t newer, const uint32_t older) {
    if (newer >= older) return newer - older;
    return (uint32_t)((uint64_t)UINT32_MAX - older + 1U + newer);
}

static uint32_t rsSpscQueueBoundedDifference(const uint32_t newer, const uint32_t older, const uint32_t capacity) {
    const uint32_t difference = rsSpscQueueSequenceDifference(newer, older);

    /* This is defensive clamping for a role-local observation.  It is not a
     * global snapshot: only the producer may query free space and only the
     * consumer may query available entries. */
    return difference > capacity ? capacity : difference;
}

sbool rsSpscQueueAtomicsAvailable(void) {
#ifdef HAVE_ATOMIC_BUILTINS
    /* This queue must never turn a nominal atomic operation into an internal
     * compiler/runtime lock.  A platform without always-lock-free 32-bit
     * atomics is unsupported until it gains an explicitly reviewed backend. */
    return __atomic_always_lock_free(sizeof(uint32_t), 0) ? 1 : 0;
#else
    return 0;
#endif
}

sbool rsSpscQueueInit(rs_spsc_queue_t *queue, void **slots, const uint32_t capacity) {
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

uint32_t rsSpscQueueProducerFree(const rs_spsc_queue_t *queue) {
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

uint32_t rsSpscQueueConsumerAvailable(const rs_spsc_queue_t *queue) {
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

sbool rsSpscQueueTryPush(rs_spsc_queue_t *queue, void *const *items, const size_t count) {
#ifdef HAVE_ATOMIC_BUILTINS
    uint32_t used;
    size_t i;

    if (queue == NULL || !rsSpscQueueAtomicsAvailable() || (count != 0 && items == NULL) || count > queue->capacity)
        return 0;
    if (count == 0) return 1;

    used = rsSpscQueueSequenceDifference(queue->producer.sequence, queue->producer.cached_sequence);
    if (used > queue->capacity || queue->capacity - used < count) {
        queue->producer.cached_sequence = __atomic_load_n(&queue->consumer_publication.sequence, __ATOMIC_ACQUIRE);
        used = rsSpscQueueSequenceDifference(queue->producer.sequence, queue->producer.cached_sequence);
        if (used > queue->capacity || queue->capacity - used < count) return 0;
    }

    for (i = 0; i < count; ++i) {
        queue->slots[queue->producer.slot_cursor] = items[i];
        rsSpscQueueAdvanceCursor(&queue->producer.slot_cursor, queue->capacity);
    }
    queue->producer.sequence = rsSpscQueueSequenceAdd(queue->producer.sequence, (uint32_t)count);
    __atomic_store_n(&queue->producer_publication.sequence, queue->producer.sequence, __ATOMIC_RELEASE);
    return 1;
#else
    (void)queue;
    (void)items;
    (void)count;
    return 0;
#endif
}

size_t rsSpscQueuePop(rs_spsc_queue_t *queue, void **items, const size_t maximum) {
#ifdef HAVE_ATOMIC_BUILTINS
    uint32_t available;
    size_t count;
    size_t i;

    if (queue == NULL || !rsSpscQueueAtomicsAvailable() || (maximum != 0 && items == NULL) || maximum == 0) return 0;

    available = rsSpscQueueSequenceDifference(queue->consumer.cached_sequence, queue->consumer.sequence);
    if (available == 0) {
        queue->consumer.cached_sequence = __atomic_load_n(&queue->producer_publication.sequence, __ATOMIC_ACQUIRE);
        available = rsSpscQueueSequenceDifference(queue->consumer.cached_sequence, queue->consumer.sequence);
        if (available == 0 || available > queue->capacity) return 0;
    }
    if (available > queue->capacity) return 0;

    count = maximum < available ? maximum : available;
    for (i = 0; i < count; ++i) {
        items[i] = queue->slots[queue->consumer.slot_cursor];
        queue->slots[queue->consumer.slot_cursor] = NULL;
        rsSpscQueueAdvanceCursor(&queue->consumer.slot_cursor, queue->capacity);
    }
    queue->consumer.sequence = rsSpscQueueSequenceAdd(queue->consumer.sequence, (uint32_t)count);
    __atomic_store_n(&queue->consumer_publication.sequence, queue->consumer.sequence, __ATOMIC_RELEASE);
    return count;
#else
    (void)queue;
    (void)items;
    (void)maximum;
    return 0;
#endif
}
