/* Private poll-array reservation shared with the focused capacity unit test. */
#ifndef INCLUDED_TCPSRV_POLL_H
#define INCLUDED_TCPSRV_POLL_H

#include <poll.h>
#include <stdint.h>
#include <stdlib.h>

/* maxfds counts usable descriptors; every allocation also owns one sentinel.
 * Only the single-threaded poll event loop mutates this storage. On failure,
 * the caller retains the original allocation and descriptor capacity. */
static rsRetVal tcpsrvPollReserve(struct pollfd **const fds,
                                  uint32_t *const maxfds,
                                  const uint32_t currfds,
                                  const uint32_t additional) {
    const uint64_t required = (uint64_t)currfds + additional;
    if (*fds != NULL && required <= *maxfds) return RS_RET_OK;
    const uint64_t capacity = ((required + 1023) / 1024) * 1024;
    if (capacity > UINT32_MAX || capacity + 1 > SIZE_MAX / sizeof(**fds)) return RS_RET_OUT_OF_MEMORY;
    struct pollfd *const replacement = realloc(*fds, (size_t)(capacity + 1) * sizeof(**fds));
    if (replacement == NULL) return RS_RET_OUT_OF_MEMORY;
    *fds = replacement;
    *maxfds = (uint32_t)capacity;
    return RS_RET_OK;
}

#endif
