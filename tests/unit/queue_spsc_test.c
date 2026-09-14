/* SPDX-License-Identifier: Apache-2.0 */

/*
 * Unit coverage for the bounded SPSC pointer ring.
 *
 * The sequential cases prove exact usable capacity, whole-batch no-fit,
 * oversize and empty handling, non-power-of-two cursor reuse, and both cursor
 * and sequence wraparound.  The cached-full case freezes a producer's stale
 * full observation, releases one consumer slot, and proves the next fitting
 * batch refreshes the consumer publication before admitting.  The pthread case
 * varies producer and consumer batch sizes and checks every generated payload
 * exactly once. Its payload bytes and checksum are initialized before enqueue
 * and checked after dequeue, making visibility after release/acquire part of
 * the concurrent-publication oracle rather than only pointer-ID ordering. A
 * 60-second alarm bounds a regression hang; it is not the success oracle.
 */
#include "config.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "queue_spsc.h"

#define CHECK(condition)                                                                    \
    do {                                                                                    \
        if (!(condition)) {                                                                 \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            return 1;                                                                       \
        }                                                                                   \
    } while (0)

static void *id_pointer(const uint32_t id) {
    return (void *)(uintptr_t)id;
}

static int test_exact_fit_and_no_fit(void) {
    rs_spsc_queue_t queue;
    void *slots[5];
    void *const input[] = {id_pointer(1), id_pointer(2), id_pointer(3), id_pointer(4), id_pointer(5)};
    void *output[5];

    CHECK(rsSpscQueueInit(&queue, slots, 5));
    CHECK(rsSpscQueueTryPush(&queue, input, 5));
    CHECK(rsSpscQueueProducerFree(&queue) == 0);
    CHECK(!rsSpscQueueTryPush(&queue, input, 1));
    CHECK(rsSpscQueuePop(&queue, output, 5) == 5);
    for (uint32_t i = 0; i < 5; ++i) CHECK(output[i] == input[i]);
    CHECK(rsSpscQueueConsumerAvailable(&queue) == 0);
    return 0;
}

static int test_reference_capacity_10000(void) {
    rs_spsc_queue_t queue;
    static void *slots[10000];
    static void *input[10000];
    static void *output[10000];

    for (uint32_t i = 0; i < 10000; ++i) input[i] = id_pointer(i + 1U);
    CHECK(rsSpscQueueInit(&queue, slots, 10000));
    CHECK(rsSpscQueueTryPush(&queue, input, 10000));
    CHECK(rsSpscQueueProducerFree(&queue) == 0);
    CHECK(rsSpscQueuePop(&queue, output, 10000) == 10000);
    for (uint32_t i = 0; i < 10000; ++i) CHECK(output[i] == input[i]);
    return 0;
}

static int test_whole_batch_no_fit(void) {
    rs_spsc_queue_t queue;
    void *slots[5];
    void *const resident[] = {id_pointer(1), id_pointer(2), id_pointer(3)};
    void *const rejected[] = {id_pointer(4), id_pointer(5), id_pointer(6)};
    void *output[5];

    CHECK(rsSpscQueueInit(&queue, slots, 5));
    CHECK(rsSpscQueueTryPush(&queue, resident, 3));
    CHECK(!rsSpscQueueTryPush(&queue, rejected, 3));
    CHECK(rsSpscQueuePop(&queue, output, 5) == 3);
    for (uint32_t i = 0; i < 3; ++i) CHECK(output[i] == resident[i]);
    CHECK(rsSpscQueuePop(&queue, output, 5) == 0);
    return 0;
}

static int test_oversize_and_empty(void) {
    rs_spsc_queue_t queue;
    void *slots[3];
    void *const oversized[] = {id_pointer(1), id_pointer(2), id_pointer(3), id_pointer(4)};

    CHECK(rsSpscQueueInit(&queue, slots, 3));
    CHECK(rsSpscQueueTryPush(&queue, NULL, 0));
    CHECK(rsSpscQueuePop(&queue, NULL, 0) == 0);
    CHECK(!rsSpscQueueTryPush(&queue, oversized, 4));
    CHECK(rsSpscQueuePop(&queue, slots, 1) == 0);
    CHECK(!rsSpscQueueInit(&queue, slots, 0));
    CHECK(!rsSpscQueueInit(&queue, slots, RS_SPSC_QUEUE_MAX_CAPACITY + 1U));
    return 0;
}

static int test_many_wraps(void) {
    rs_spsc_queue_t queue;
    void *slots[7];
    void *input[7];
    void *output[7];
    uint32_t expected[7];
    uint32_t expected_head = 0;
    uint32_t expected_tail = 0;
    uint32_t expected_count = 0;
    uint32_t next_id = 1;

    CHECK(rsSpscQueueInit(&queue, slots, 7));
    while (next_id <= 50000 || expected_count != 0) {
        uint32_t requested = (next_id % 5U) + 1U;

        if (next_id <= 50000 && requested > 50001U - next_id) requested = 50001U - next_id;
        if (next_id <= 50000 && requested > 7U - expected_count) {
            const uint32_t pop_count = ((next_id % 3U) + 1U) < expected_count ? (next_id % 3U) + 1U : expected_count;

            for (uint32_t i = 0; i < requested; ++i) input[i] = id_pointer(next_id + i);
            CHECK(!rsSpscQueueTryPush(&queue, input, requested));
            CHECK(rsSpscQueuePop(&queue, output, pop_count) == pop_count);
            for (uint32_t i = 0; i < pop_count; ++i) {
                CHECK((uint32_t)(uintptr_t)output[i] == expected[expected_head]);
                expected_head = (expected_head + 1U) % 7U;
                --expected_count;
            }
            continue;
        }
        if (next_id <= 50000) {
            for (uint32_t i = 0; i < requested; ++i) {
                input[i] = id_pointer(next_id + i);
                expected[expected_tail] = next_id + i;
                expected_tail = (expected_tail + 1U) % 7U;
            }
            CHECK(rsSpscQueueTryPush(&queue, input, requested));
            next_id += requested;
            expected_count += requested;
        } else {
            const uint32_t pop_count = expected_count < 4U ? expected_count : 4U;
            CHECK(rsSpscQueuePop(&queue, output, pop_count) == pop_count);
            for (uint32_t i = 0; i < pop_count; ++i) {
                CHECK((uint32_t)(uintptr_t)output[i] == expected[expected_head]);
                expected_head = (expected_head + 1U) % 7U;
                --expected_count;
            }
        }
    }
    return 0;
}

static int test_uint32_sequence_wrap(void) {
    rs_spsc_queue_t queue;
    void *slots[5];
    void *const input[] = {id_pointer(11), id_pointer(12), id_pointer(13)};
    void *output[3];
    const uint32_t before_wrap = UINT32_MAX - 2U;

    CHECK(rsSpscQueueInit(&queue, slots, 5));
    queue.producer.sequence = before_wrap;
    queue.producer.cached_sequence = before_wrap;
    queue.producer.slot_cursor = 4;
    queue.producer_publication.sequence = before_wrap;
    queue.consumer.sequence = before_wrap;
    queue.consumer.cached_sequence = before_wrap;
    queue.consumer.slot_cursor = 4;
    queue.consumer_publication.sequence = before_wrap;

    CHECK(rsSpscQueueTryPush(&queue, input, 3));
    CHECK(queue.producer.sequence == 0);
    CHECK(queue.producer.slot_cursor == 2);
    CHECK(rsSpscQueuePop(&queue, output, 3) == 3);
    CHECK(queue.consumer.sequence == 0);
    CHECK(queue.consumer.slot_cursor == 2);
    for (uint32_t i = 0; i < 3; ++i) CHECK(output[i] == input[i]);
    return 0;
}

static int test_cached_full_and_slot_reuse(void) {
    rs_spsc_queue_t queue;
    void *slots[5];
    void *const initial[] = {id_pointer(1), id_pointer(2), id_pointer(3), id_pointer(4), id_pointer(5)};
    void *const replacement[] = {id_pointer(6)};
    void *output[5];

    CHECK(rsSpscQueueInit(&queue, slots, 5));
    CHECK(rsSpscQueueTryPush(&queue, initial, 5));
    CHECK(!rsSpscQueueTryPush(&queue, replacement, 1));
    CHECK(rsSpscQueuePop(&queue, output, 1) == 1);
    CHECK(output[0] == id_pointer(1));
    /* The producer only knew the queue was full.  This push must acquire the
     * consumer's release and reuse slot zero without disturbing slots 1..4. */
    CHECK(rsSpscQueueTryPush(&queue, replacement, 1));
    CHECK(rsSpscQueuePop(&queue, output, 5) == 4);
    for (uint32_t i = 0; i < 4; ++i) CHECK(output[i] == initial[i + 1]);
    /* Consumer cached the first producer publication.  It must refresh after
     * draining that publication before observing the reused slot. */
    CHECK(rsSpscQueuePop(&queue, output, 5) == 1);
    CHECK(output[0] == replacement[0]);
    return 0;
}

#define CONCURRENT_ITEMS 100000U
#define PAYLOAD_BYTES 64U

typedef struct publication_payload_s {
    uint32_t id;
    uint32_t checksum;
    unsigned char bytes[PAYLOAD_BYTES];
} publication_payload_t;

typedef struct spsc_thread_test_state_s {
    rs_spsc_queue_t queue;
    void *slots[31];
    unsigned char seen[CONCURRENT_ITEMS];
    publication_payload_t payloads[CONCURRENT_ITEMS];
    pthread_mutex_t start_mutex;
    pthread_cond_t start_cond;
    int started;
    int abort_start;
    int consumer_result;
} spsc_thread_test_state_t;

static uint32_t payload_checksum(const publication_payload_t *payload) {
    uint32_t checksum = payload->id;

    for (uint32_t i = 0; i < PAYLOAD_BYTES; ++i) checksum = (checksum * 33U) ^ payload->bytes[i];
    return checksum;
}

static void initialize_payload(publication_payload_t *payload, const uint32_t id) {
    payload->id = id;
    for (uint32_t i = 0; i < PAYLOAD_BYTES; ++i) payload->bytes[i] = (unsigned char)(id + (i * 17U));
    payload->checksum = payload_checksum(payload);
}

static int wait_for_start(spsc_thread_test_state_t *state) {
    int abort_start;

    (void)pthread_mutex_lock(&state->start_mutex);
    while (!state->started) (void)pthread_cond_wait(&state->start_cond, &state->start_mutex);
    abort_start = state->abort_start;
    (void)pthread_mutex_unlock(&state->start_mutex);
    return abort_start;
}

static void *producer_main(void *arg) {
    spsc_thread_test_state_t *const state = arg;
    void *batch[11];
    uint32_t next_id = 1;

    if (wait_for_start(state)) return NULL;
    while (next_id <= CONCURRENT_ITEMS) {
        uint32_t count = (next_id % 11U) + 1U;

        if (count > CONCURRENT_ITEMS + 1U - next_id) count = CONCURRENT_ITEMS + 1U - next_id;
        for (uint32_t i = 0; i < count; ++i) {
            publication_payload_t *const payload = &state->payloads[next_id + i - 1U];

            initialize_payload(payload, next_id + i);
            batch[i] = payload;
        }
        if (rsSpscQueueTryPush(&state->queue, batch, count)) next_id += count;
    }
    return NULL;
}

static void *consumer_main(void *arg) {
    spsc_thread_test_state_t *const state = arg;
    void *batch[13];
    uint32_t received = 0;

    if (wait_for_start(state)) return NULL;
    while (received < CONCURRENT_ITEMS) {
        const size_t count = rsSpscQueuePop(&state->queue, batch, (received % 13U) + 1U);

        if (count == 0) continue;
        for (size_t i = 0; i < count; ++i) {
            const publication_payload_t *const payload = batch[i];
            const uint32_t id = payload->id;

            if (id == 0 || id > CONCURRENT_ITEMS) {
                state->consumer_result = 1;
            } else if (state->seen[id - 1]) {
                state->consumer_result = 1;
            } else {
                state->seen[id - 1] = 1;
            }
            if (payload->checksum != payload_checksum(payload)) state->consumer_result = 1;
            ++received;
        }
    }
    return NULL;
}

static int test_variable_batch_threads(void) {
    static spsc_thread_test_state_t state = {
        .start_mutex = PTHREAD_MUTEX_INITIALIZER,
        .start_cond = PTHREAD_COND_INITIALIZER,
    };
    pthread_t producer;
    pthread_t consumer;

    CHECK(rsSpscQueueInit(&state.queue, state.slots, 31));
    CHECK(pthread_create(&producer, NULL, producer_main, &state) == 0);
    if (pthread_create(&consumer, NULL, consumer_main, &state) != 0) {
        (void)pthread_mutex_lock(&state.start_mutex);
        state.abort_start = 1;
        state.started = 1;
        (void)pthread_cond_broadcast(&state.start_cond);
        (void)pthread_mutex_unlock(&state.start_mutex);
        (void)pthread_join(producer, NULL);
        return 1;
    }
    (void)pthread_mutex_lock(&state.start_mutex);
    state.started = 1;
    (void)pthread_cond_broadcast(&state.start_cond);
    (void)pthread_mutex_unlock(&state.start_mutex);
    (void)alarm(60);
    CHECK(pthread_join(producer, NULL) == 0);
    CHECK(pthread_join(consumer, NULL) == 0);
    (void)alarm(0);
    CHECK(state.consumer_result == 0);
    for (uint32_t i = 0; i < CONCURRENT_ITEMS; ++i) CHECK(state.seen[i]);
    return 0;
}

int main(void) {
    if (!rsSpscQueueAtomicsAvailable()) {
        rs_spsc_queue_t queue;
        void *slots[1];

        if (rsSpscQueueInit(&queue, slots, 1)) {
            fputs("SPSC queue accepted unsupported atomics\n", stderr);
            return 1;
        }
        fputs("32-bit lock-free atomics unavailable; SPSC queue is intentionally unsupported\n", stderr);
        return 77;
    }
    if (test_exact_fit_and_no_fit() || test_reference_capacity_10000() || test_whole_batch_no_fit() ||
        test_oversize_and_empty() || test_many_wraps() || test_uint32_sequence_wrap() ||
        test_cached_full_and_slot_reuse() || test_variable_batch_threads())
        return 1;
    puts("SPSC queue tests passed");
    return 0;
}
