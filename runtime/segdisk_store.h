/* Log-structured serial store for queue.type="segmentedDisk". */
#ifndef INCLUDED_SEGDISK_STORE_H
#define INCLUDED_SEGDISK_STORE_H

#include <stddef.h>
#include <stdint.h>
#include "rsyslog.h"
#include "batch.h"

/** Opaque segmented queue store. All access is serialized by its queue lock. */
typedef struct segdisk_store_s segdisk_store_t;

/** Configuration captured when a segmented store object is constructed.
 *
 * lazy_create keeps a DA child unmaterialized until its first append. It does
 * not change the eager lifecycle of a pure segmentedDisk queue.
 */
typedef struct segdisk_store_config_s {
    const char *work_dir;
    const char *file_prefix;
    const char *queue_name;
    int64_t max_file_size;
    int64_t max_disk_space;
    unsigned int checkpoint_interval;
    sbool sync_files;
    sbool lazy_create;
} segdisk_store_config_t;

/** Monotonic operation counters and current physical store gauges.
 *
 * bytes and segments describe the current topology. The remaining fields are
 * cumulative counters, including lazy materialization and idle-cleanup
 * outcomes; startup_payload_bytes_read is expected to remain zero.
 */
typedef struct segdisk_store_stats_s {
    int64_t bytes;
    int segments;
    uint64_t checkpoints;
    uint64_t replayed;
    uint64_t corruption_events;
    uint64_t corruption_bytes;
    uint64_t corruption_records;
    int64_t retry_overage_bytes;
    int64_t retry_overage_max_bytes;
    uint64_t state_writes;
    uint64_t forced_state_writes;
    uint64_t recovery_bytes;
    uint64_t recovery_records;
    uint64_t startup_payload_bytes_read;
    uint64_t startup_segment_files_probed;
    uint64_t recovery_pending;
    uint64_t corruption_segments;
    uint64_t materializations;
    uint64_t dematerializations;
    uint64_t idle_cleanup_failures;
} segdisk_store_stats_t;

#ifdef ENABLE_IMDIAG
typedef enum segdisk_test_fault_point_e {
    SEGDISK_TEST_FAULT_NONE = 0,
    SEGDISK_TEST_FAULT_RESERVATION_PUBLISHED,
    SEGDISK_TEST_FAULT_SEGMENT_CREATED,
    SEGDISK_TEST_FAULT_SEAL_WRITTEN,
    SEGDISK_TEST_FAULT_SEAL_RENAMED,
    SEGDISK_TEST_FAULT_CHECKPOINT_PUBLISHED,
    SEGDISK_TEST_FAULT_COMMIT_PUBLISHED,
    SEGDISK_TEST_FAULT_PREDELETE_PUBLISHED,
    SEGDISK_TEST_FAULT_SEGMENT_UNLINKED,
    SEGDISK_TEST_FAULT_IDLE_EMPTY_PUBLISHED,
    SEGDISK_TEST_FAULT_IDLE_SEGMENTS_UNLINKED,
    SEGDISK_TEST_FAULT_IDLE_STATE_UNLINKED,
    SEGDISK_TEST_FAULT_IDLE_DIRECTORY_REMOVED,
} segdisk_test_fault_point_t;
#endif

/**
 * Concurrency & Locking
 * ---------------------
 * Every function below is called with the owning queue mutex held. The same
 * mutex is shared by a DA parent and its child, so no store function acquires
 * an independent lifecycle lock. Append serializes synchronously and retains
 * no message reference. A dequeued batch retains internal completion state
 * until CompleteBatch commits or retry-appends every element.
 *
 * Dematerialize is destructive and succeeds only when CanDematerialize proves
 * there is no known data, undiscovered recovery work, pending batch, retry,
 * completion, or deletion. Failure leaves a materialized recoverable store.
 * Close destroys the object; Dematerialize resets the same object for a later
 * lazy append. GetStats and test-fault operations are safe before lazy
 * materialization.
 */
rsRetVal segdiskStoreOpen(segdisk_store_t **store, const segdisk_store_config_t *config, int *queue_size);
rsRetVal segdiskStoreAppend(segdisk_store_t *store, smsg_t *msg, sbool internal_retry, int64_t *written);
rsRetVal segdiskStoreDequeueBatch(segdisk_store_t *store, batch_t *batch, int max, int *skipped, int *discovered);
rsRetVal segdiskStoreCompleteBatch(segdisk_store_t *store, batch_t *batch, int *committed, int *retried);
rsRetVal segdiskStoreCheckpoint(segdisk_store_t *store, sbool force_sync);
rsRetVal segdiskStoreDematerialize(segdisk_store_t *store);
rsRetVal segdiskStoreClose(segdisk_store_t **store, sbool empty);
void segdiskStoreGetStats(const segdisk_store_t *store, segdisk_store_stats_t *stats);
sbool segdiskStoreMayHaveData(const segdisk_store_t *store);
sbool segdiskStoreCanDematerialize(const segdisk_store_t *store);
#ifdef ENABLE_IMDIAG
rsRetVal segdiskStoreSetTestFault(segdisk_store_t *store, const char *point, unsigned int hit_count);
void segdiskStoreClearTestFault(segdisk_store_t *store);
#endif

#endif
