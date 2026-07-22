#ifndef INCLUDED_OMAZUREDCE_UTILS_H
#define INCLUDED_OMAZUREDCE_UTILS_H

#include <pthread.h>
#include <stddef.h>

typedef enum { OMAZUREDCE_REQUEST_FITS = 0, OMAZUREDCE_REQUEST_RETRY } omazuredce_request_size_result_t;

omazuredce_request_size_result_t omazuredceClassifyRequestSize(size_t requestBytes, size_t maxRequestBytes);
void omazuredceRequestTimerStop(pthread_mutex_t *mutex, pthread_cond_t *cond, signed char *stopRequested);

#endif
