#include "config.h"
#include "omazuredce_utils.h"

omazuredce_request_size_result_t omazuredceClassifyRequestSize(const size_t requestBytes,
                                                               const size_t maxRequestBytes) {
    return requestBytes <= maxRequestBytes ? OMAZUREDCE_REQUEST_FITS : OMAZUREDCE_REQUEST_RETRY;
}

void omazuredceRequestTimerStop(pthread_mutex_t *const mutex,
                                pthread_cond_t *const cond,
                                signed char *const stopRequested) {
    (void)pthread_mutex_lock(mutex);
    *stopRequested = 1;
    (void)pthread_cond_signal(cond);
    (void)pthread_mutex_unlock(mutex);
}
