/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright 2026 Rainer Gerhards and Adiscon GmbH.
 */

#ifndef INCLUDED_IMUDP_BUFFER_H
#define INCLUDED_IMUDP_BUFFER_H

#include <stddef.h>
#include <stdint.h>

/* Keep every receive slot and its aggregate allocation in one checked size_t domain. */
static inline int imudp_checked_size_mul(const size_t count, const size_t size, size_t *const result) {
    if (result == NULL || (count != 0 && size > SIZE_MAX / count)) {
        return 0;
    }

    *result = count * size;
    return 1;
}

static inline int imudp_compute_buffer_layout(const size_t max_message_size,
                                              const size_t batch_size,
                                              size_t *const slot_size,
                                              size_t *const buffer_size) {
    size_t slot;
    size_t total;

    if (slot_size == NULL || buffer_size == NULL || batch_size == 0 || max_message_size == SIZE_MAX) {
        return 0;
    }

    slot = max_message_size + 1;
    if (!imudp_checked_size_mul(batch_size, slot, &total)) {
        return 0;
    }

    *slot_size = slot;
    *buffer_size = total;
    return 1;
}

#endif
