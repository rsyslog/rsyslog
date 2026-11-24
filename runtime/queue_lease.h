/* Internal batch-source lease helpers for queue workers.
 *
 * A worker-pool callback may only acquire and complete batches from its pool's
 * user queue. The caller holds that queue mutex for every operation here.
 */
#ifndef QUEUE_LEASE_H_INCLUDED
#define QUEUE_LEASE_H_INCLUDED

#include <pthread.h>

#include "rsyslog.h"

struct queue_s;

static inline rsRetVal qqueueLeaseBind(struct queue_s **const source_slot,
                                       struct queue_s **const owner_slot,
                                       struct queue_s *const source,
                                       struct queue_s *const logical_owner,
                                       const void *const pool_user,
                                       const pthread_mutex_t *const pool_mutex,
                                       const pthread_mutex_t *const source_mutex) {
    if (source_slot == NULL || owner_slot == NULL || source == NULL || logical_owner == NULL) return RS_RET_PARAM_ERROR;

    /* Equal mutexes do not authorize a worker to service another queue. */
    if (pool_user != source || pool_mutex != source_mutex) return RS_RET_INTERNAL_ERROR;
    if (*source_slot != NULL && (*source_slot != source || *owner_slot != logical_owner)) return RS_RET_INTERNAL_ERROR;

    *source_slot = source;
    *owner_slot = logical_owner;
    return RS_RET_OK;
}

static inline rsRetVal qqueueLeaseClear(struct queue_s **const source_slot,
                                        struct queue_s **const owner_slot,
                                        const struct queue_s *const source) {
    if (source_slot == NULL || owner_slot == NULL || source == NULL) return RS_RET_PARAM_ERROR;
    if (*source_slot != source) return RS_RET_INTERNAL_ERROR;

    *source_slot = NULL;
    *owner_slot = NULL;
    return RS_RET_OK;
}

static inline int qqueueLeaseHasResponsibility(const int n_elem, const int n_elem_deq, const void *const store_data) {
    return n_elem != 0 || n_elem_deq != 0 || store_data != NULL;
}

#endif
