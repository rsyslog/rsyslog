/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2026 Adiscon GmbH.
 * Licensed under the Apache License, Version 2.0.
 */

/**
 * @file queue_local_stats.h
 * @brief Snapshot-backed impstats interface for experimental local queues.
 *
 * The adapter owns the stats objects and their counter storage.  It samples
 * immutable local-queue counters only while stats are rendered; no producer or
 * consumer path calls into this file.
 */
#ifndef QUEUE_LOCAL_STATS_H_INCLUDED
#define QUEUE_LOCAL_STATS_H_INCLUDED

#include <stdint.h>

#include "rsyslog.h"

/** @brief Queue owning the local statistics adapter. */
typedef struct queue_s qqueue_t;
/** @brief Opaque owner of the logical and optional per-frontend stats objects. */
typedef struct qqueue_local_stats_s qqueue_local_stats_t;

/**
 * @brief Obtain the statsobj interface needed before adapter construction.
 * @return RS_RET_OK on success, otherwise the interface acquisition error.
 */
rsRetVal qqueue_local_stats_class_init(void);

/**
 * @brief Construct the always-on logical summary and optional frontend objects.
 *
 * When frontend_detail is set, construct one fixed stats object for every
 * configured frontend slot. frontend_limit is the configuration bound, not
 * the current number of registrations.
 *
 * All published values are lifetime snapshot values.  They deliberately use
 * CTR_FLAG_NONE: a collector reset must not race ownership accounting.
 * @param owner Queue supplying lock-free lifetime snapshots; remains live until destruction.
 * @param logical_name Queue name used to derive the stats object names.
 * @param frontend_limit Configured maximum number of frontend slots.
 * @param frontend_detail Whether to publish per-frontend objects.
 * @param out Receives the adapter on success; set to NULL before construction.
 * @return RS_RET_OK on success, otherwise a construction error.
 */
rsRetVal qqueue_local_stats_construct(qqueue_t *owner,
                                      const uchar *logical_name,
                                      uint32_t frontend_limit,
                                      int frontend_detail,
                                      qqueue_local_stats_t **out);

/**
 * @brief Unlink and destroy all stats objects, then clear the caller's pointer.
 *
 * Call before qqueueLocalDestruct() releases owner->local.
 * The statsobj unlink holds its global list lock, so no pre-read callback can
 * retain this adapter after this function returns.
 * @param stats Adapter pointer to clear; NULL and already cleared pointers are accepted.
 */
void qqueue_local_stats_destruct(qqueue_local_stats_t **stats);

#ifdef ENABLE_TESTBENCH
/** @brief Test-only callback invoked inside the actual stats pre-read callback. */
typedef void (*qqueue_local_stats_test_pre_read_hook_t)(void *context);
/**
 * @brief Install or clear a test-only pre-read hook and its opaque context.
 *
 * Clearing the hook waits for stats-object readers which may already have
 * loaded its context. Call outside a stats callback and before adapter
 * destruction.
 * @param stats Adapter to instrument; NULL is ignored.
 * @param hook Hook to invoke during a stats pre-read, or NULL to clear it.
 * @param context Context passed to the hook; clear before its lifetime ends.
 */
void qqueue_local_stats_set_test_pre_read_hook(qqueue_local_stats_t *stats,
                                               qqueue_local_stats_test_pre_read_hook_t hook,
                                               void *context);

/**
 * @brief Exercise stats-list reader/destructor serialization on a live local queue.
 *
 * This destructive test fixture is available only to imdiag.
 * @param owner Initialized queue whose stats adapter is consumed by the fixture.
 * @return RS_RET_OK when the lifetime oracle passes, otherwise a fixture error.
 */
rsRetVal qqueue_local_stats_test_lifetime(qqueue_t *owner);
#endif

#endif
