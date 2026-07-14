/* Fixed-size durable state codec for queue.type="segmentedDisk". */
#include "config.h"
#include <stdint.h>
#include <string.h>
#include "segdisk_crc.h"
#include "segdisk_format.h"
#include "segdisk_state.h"

#define STATE_MAGIC "RSSEGST2"

static void put16(unsigned char *p, uint16_t v) {
    p[0] = (unsigned char)(v >> 8);
    p[1] = (unsigned char)v;
}

static void put32(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static void put64(unsigned char *p, uint64_t v) {
    put32(p, (uint32_t)(v >> 32));
    put32(p + 4, (uint32_t)v);
}

static uint16_t get16(const unsigned char *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t get32(const unsigned char *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static uint64_t get64(const unsigned char *p) {
    return ((uint64_t)get32(p) << 32) | get32(p + 4);
}

void segdiskStateEncode(const segdisk_state_image_t *s,
                        const uint64_t generation,
                        unsigned char b[SEGDISK_STATE_SLOT_LEN]) {
    memset(b, 0, SEGDISK_STATE_SLOT_LEN);
    memcpy(b, STATE_MAGIC, 8);
    put16(b + 8, SEGDISK_STATE_STORE_VERSION);
    put16(b + 10, SEGDISK_CODEC_VERSION);
    memcpy(b + 12, s->uuid, sizeof(s->uuid));
    put64(b + 28, generation);
    put32(b + 36, s->flags);
    put64(b + 40, s->committed_segment);
    put64(b + 48, (uint64_t)s->committed_offset);
    put64(b + 56, s->committed_record_sequence);
    put64(b + 64, s->first_live_segment);
    put64(b + 72, s->last_data_segment);
    put64(b + 80, s->active_segment);
    put64(b + 88, s->recovery_first);
    put64(b + 96, s->recovery_last);
    put64(b + 104, s->next_segment);
    put64(b + 112, s->known_queue_size);
    put64(b + 120, (uint64_t)s->bytes);
    put64(b + 128, s->segments);
    put64(b + 136, s->writer_segment);
    put64(b + 144, (uint64_t)s->writer_end);
    put64(b + 152, s->writer_sequence);
    put64(b + 160, s->writer_count);
    put64(b + 168, s->delete_first);
    put64(b + 176, s->delete_last);
    put64(b + 184, (uint64_t)s->delete_bytes);
    put64(b + 192, s->delete_segments);
    put32(b + SEGDISK_STATE_SLOT_LEN - 4, segdiskCrc32c(b, SEGDISK_STATE_SLOT_LEN - 4));
}

sbool segdiskStateValidate(const unsigned char b[SEGDISK_STATE_SLOT_LEN]) {
    return !memcmp(b, STATE_MAGIC, 8) && get16(b + 8) == SEGDISK_STATE_STORE_VERSION &&
           get16(b + 10) == SEGDISK_CODEC_VERSION &&
           get32(b + SEGDISK_STATE_SLOT_LEN - 4) == segdiskCrc32c(b, SEGDISK_STATE_SLOT_LEN - 4);
}

void segdiskStateDecode(const unsigned char b[SEGDISK_STATE_SLOT_LEN], segdisk_state_image_t *s) {
    memset(s, 0, sizeof(*s));
    memcpy(s->uuid, b + 12, sizeof(s->uuid));
    s->generation = get64(b + 28);
    s->flags = get32(b + 36);
    s->committed_segment = get64(b + 40);
    s->committed_offset = (int64_t)get64(b + 48);
    s->committed_record_sequence = get64(b + 56);
    s->first_live_segment = get64(b + 64);
    s->last_data_segment = get64(b + 72);
    s->active_segment = get64(b + 80);
    s->recovery_first = get64(b + 88);
    s->recovery_last = get64(b + 96);
    s->next_segment = get64(b + 104);
    s->known_queue_size = get64(b + 112);
    s->bytes = (int64_t)get64(b + 120);
    s->segments = get64(b + 128);
    s->writer_segment = get64(b + 136);
    s->writer_end = (int64_t)get64(b + 144);
    s->writer_sequence = get64(b + 152);
    s->writer_count = get64(b + 160);
    s->delete_first = get64(b + 168);
    s->delete_last = get64(b + 176);
    s->delete_bytes = (int64_t)get64(b + 184);
    s->delete_segments = get64(b + 192);
}

sbool segdiskStateGenerationNewer(const uint64_t candidate, const uint64_t reference) {
    if (candidate == reference) return 0;
    const uint64_t distance = candidate > reference ? candidate - reference : UINT64_MAX - reference + candidate + 1;
    return distance < (UINT64_C(1) << 63);
}

rsRetVal segdiskStateSelect(const unsigned char slot0[SEGDISK_STATE_SLOT_LEN],
                            const sbool present0,
                            const unsigned char slot1[SEGDISK_STATE_SLOT_LEN],
                            const sbool present1,
                            segdisk_state_image_t *state,
                            int *selected_slot) {
    const sbool valid0 = present0 && segdiskStateValidate(slot0);
    const sbool valid1 = present1 && segdiskStateValidate(slot1);
    if (!valid0 && !valid1) return RS_RET_INVALID_VALUE;
    if (valid0 && valid1 && memcmp(slot0 + 12, slot1 + 12, 16) != 0) return RS_RET_INVALID_VALUE;
    if (valid0 && valid1) {
        const uint64_t generation0 = get64(slot0 + 28);
        const uint64_t generation1 = get64(slot1 + 28);
        if (generation0 != generation1 && !segdiskStateGenerationNewer(generation0, generation1) &&
            !segdiskStateGenerationNewer(generation1, generation0))
            return RS_RET_INVALID_VALUE;
    }
    const int selected =
        valid1 && (!valid0 || segdiskStateGenerationNewer(get64(slot1 + 28), get64(slot0 + 28))) ? 1 : 0;
    segdiskStateDecode(selected == 0 ? slot0 : slot1, state);
    if (selected_slot != NULL) *selected_slot = selected;
    return RS_RET_OK;
}
