/* Log-structured serial store for queue.type="segmentedDisk". */
#ifndef INCLUDED_SEGDISK_STORE_H
#define INCLUDED_SEGDISK_STORE_H

#include <stddef.h>
#include <stdint.h>
#include "rsyslog.h"
#include "batch.h"

typedef struct segdisk_store_s segdisk_store_t;

typedef struct segdisk_store_config_s {
    const char *work_dir;
    const char *file_prefix;
    const char *queue_name;
    int64_t max_file_size;
    int64_t max_disk_space;
    unsigned int checkpoint_interval;
    sbool sync_files;
} segdisk_store_config_t;

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
    uint64_t recovery_pending;
} segdisk_store_stats_t;

rsRetVal segdiskStoreOpen(segdisk_store_t **store, const segdisk_store_config_t *config, int *queue_size);
rsRetVal segdiskStoreAppend(segdisk_store_t *store, smsg_t *msg, sbool internal_retry, int64_t *written);
rsRetVal segdiskStoreDequeueBatch(segdisk_store_t *store, batch_t *batch, int max, int *skipped, int *discovered);
rsRetVal segdiskStoreCompleteBatch(segdisk_store_t *store, batch_t *batch, int *committed, int *retried);
rsRetVal segdiskStoreCheckpoint(segdisk_store_t *store, sbool force_sync);
rsRetVal segdiskStoreClose(segdisk_store_t **store, sbool empty);
void segdiskStoreGetStats(const segdisk_store_t *store, segdisk_store_stats_t *stats);
sbool segdiskStoreMayHaveData(const segdisk_store_t *store);

#endif
