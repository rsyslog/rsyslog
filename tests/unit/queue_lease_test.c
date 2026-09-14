/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Verify the internal queue batch-source lease. The oracle is purely pointer
 * state: normal and partial batches retain their source, retry store context
 * retains it, cancellation clears only that source, and mismatched callback or
 * mutex identities leave an existing lease untouched. This test deliberately
 * avoids daemon timing and queue storage implementation details.
 */
#include "config.h"

#include <stdio.h>

#include "queue_lease.h"

#define CHECK(condition)                                                                    \
    do {                                                                                    \
        if (!(condition)) {                                                                 \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            return 1;                                                                       \
        }                                                                                   \
    } while (0)

int main(void) {
    int source_a_object;
    int source_b_object;
    int parent_object;
    int store_context;
    struct queue_s *source_a = (struct queue_s *)&source_a_object;
    struct queue_s *source_b = (struct queue_s *)&source_b_object;
    struct queue_s *parent = (struct queue_s *)&parent_object;
    struct queue_s *source_slot = NULL;
    struct queue_s *owner_slot = NULL;
    pthread_mutex_t mutex_a = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t mutex_b = PTHREAD_MUTEX_INITIALIZER;

    CHECK(qqueueLeaseBind(&source_slot, &owner_slot, source_a, parent, source_a, &mutex_a, &mutex_a) == RS_RET_OK);
    CHECK(source_slot == source_a && owner_slot == parent);
    CHECK(qqueueLeaseHasResponsibility(1, 1, NULL)); /* normal batch */
    CHECK(qqueueLeaseHasResponsibility(0, 1, NULL)); /* discard-only/partial retirement */
    CHECK(qqueueLeaseHasResponsibility(0, 0, &store_context)); /* retry store context */
    CHECK(!qqueueLeaseHasResponsibility(0, 0, NULL));

    /* A callback owner for another queue cannot complete source_a's batch. */
    CHECK(qqueueLeaseBind(&source_slot, &owner_slot, source_b, source_b, source_b, &mutex_b, &mutex_b) ==
          RS_RET_INTERNAL_ERROR);
    CHECK(source_slot == source_a && owner_slot == parent);

    /* A matching pool user with a different mutex is equally invalid. */
    CHECK(qqueueLeaseBind(&source_slot, &owner_slot, source_a, parent, source_a, &mutex_b, &mutex_a) ==
          RS_RET_INTERNAL_ERROR);
    CHECK(source_slot == source_a && owner_slot == parent);

    CHECK(qqueueLeaseClear(&source_slot, &owner_slot, source_b) == RS_RET_INTERNAL_ERROR);
    CHECK(source_slot == source_a && owner_slot == parent);
    CHECK(qqueueLeaseClear(&source_slot, &owner_slot, source_a) == RS_RET_OK); /* cancellation completion */
    CHECK(source_slot == NULL && owner_slot == NULL);

    return 0;
}
