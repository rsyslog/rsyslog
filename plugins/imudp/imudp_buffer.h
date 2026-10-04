#ifndef INCLUDED_IMUDP_BUFFER_H
#define INCLUDED_IMUDP_BUFFER_H

#include <stddef.h>
#include <stdint.h>

/* Keep every receive slot and its aggregate allocation in one checked size_t domain. */
static inline int imudpCheckedSizeMul(const size_t count, const size_t size, size_t *const result) {
    if (result == NULL || (count != 0 && size > SIZE_MAX / count)) {
        return 0;
    }

    *result = count * size;
    return 1;
}

static inline int imudpComputeBufferLayout(const size_t maxMessageSize,
                                           const size_t batchSize,
                                           size_t *const slotSize,
                                           size_t *const bufferSize) {
    size_t slot;
    size_t total;

    if (slotSize == NULL || bufferSize == NULL || batchSize == 0 || maxMessageSize == SIZE_MAX) {
        return 0;
    }

    slot = maxMessageSize + 1;
    if (!imudpCheckedSizeMul(batchSize, slot, &total)) {
        return 0;
    }

    *slotSize = slot;
    *bufferSize = total;
    return 1;
}

#endif
