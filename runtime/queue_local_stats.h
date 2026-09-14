/* Snapshot-backed impstats for experimental local queue frontends.
 *
 * The adapter owns the stats objects and their counter storage.  It samples
 * immutable local-queue counters only while stats are rendered; no producer or
 * consumer path calls into this file.
 */
#ifndef QUEUE_LOCAL_STATS_H_INCLUDED
#define QUEUE_LOCAL_STATS_H_INCLUDED

#include <stdint.h>

#include "rsyslog.h"

typedef struct queue_s qqueue_t;
typedef struct qqueueLocalStats_s qqueueLocalStats_t;

/* Obtain the adapter's private statsobj interface before construction. */
rsRetVal qqueueLocalStatsClassInit(void);

/*
 * Construct the always-on logical summary and, when frontend_detail is set,
 * one fixed stats object for every configured frontend slot.  frontend_limit
 * is the configuration bound, not the current number of registrations.
 *
 * All published values are lifetime snapshot values.  They deliberately use
 * CTR_FLAG_NONE: a collector reset must not race ownership accounting.
 */
rsRetVal qqueueLocalStatsConstruct(
    qqueue_t *owner, const uchar *logical_name, uint32_t frontend_limit, int frontend_detail, qqueueLocalStats_t **out);

/*
 * Unlink every stats object before qqueueLocalDestruct() releases owner->local.
 * The statsobj unlink holds its global list lock, so no pre-read callback can
 * retain this adapter after this function returns.
 */
void qqueueLocalStatsDestruct(qqueueLocalStats_t **stats);

#ifdef ENABLE_TESTBENCH
/* Test-only synchronization point inside the actual pre-read callback. */
typedef void (*qqueueLocalStatsTestPreReadHook_t)(void *context);
void qqueueLocalStatsSetTestPreReadHook(qqueueLocalStats_t *stats,
                                        qqueueLocalStatsTestPreReadHook_t hook,
                                        void *context);

/* Exercise real stats-list reader/destruction serialization on an initialized
 * local queue. This destructive test fixture is available only to imdiag. */
rsRetVal qqueueLocalStatsTestLifetime(qqueue_t *owner);
#endif

#endif
