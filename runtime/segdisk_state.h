/* Fixed-size durable state codec for queue.type="segmentedDisk". */
#ifndef INCLUDED_SEGDISK_STATE_H
#define INCLUDED_SEGDISK_STATE_H

#include <stdint.h>
#include "rsyslog.h"

#define SEGDISK_STATE_STORE_VERSION 2u
#define SEGDISK_STATE_SLOT_LEN 256u
#define SEGDISK_STATE_FILE_LEN (2u * SEGDISK_STATE_SLOT_LEN)

typedef struct segdisk_state_image_s {
    unsigned char uuid[16];
    uint64_t generation;
    uint32_t flags;
    uint64_t committed_segment;
    int64_t committed_offset;
    uint64_t committed_record_sequence;
    uint64_t first_live_segment;
    uint64_t last_data_segment;
    uint64_t active_segment;
    uint64_t recovery_first;
    uint64_t recovery_last;
    uint64_t next_segment;
    uint64_t known_queue_size;
    int64_t bytes;
    uint64_t segments;
    uint64_t writer_segment;
    int64_t writer_end;
    uint64_t writer_sequence;
    uint64_t writer_count;
} segdisk_state_image_t;

void segdiskStateEncode(const segdisk_state_image_t *state,
                        uint64_t generation,
                        unsigned char slot[SEGDISK_STATE_SLOT_LEN]);
sbool segdiskStateValidate(const unsigned char slot[SEGDISK_STATE_SLOT_LEN]);
void segdiskStateDecode(const unsigned char slot[SEGDISK_STATE_SLOT_LEN], segdisk_state_image_t *state);
sbool segdiskStateGenerationNewer(uint64_t candidate, uint64_t reference);
rsRetVal segdiskStateSelect(const unsigned char slot0[SEGDISK_STATE_SLOT_LEN],
                            sbool present0,
                            const unsigned char slot1[SEGDISK_STATE_SLOT_LEN],
                            sbool present1,
                            segdisk_state_image_t *state,
                            int *selected_slot);

#endif
