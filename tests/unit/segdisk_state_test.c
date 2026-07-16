/*
 * Unit coverage for the segmentedDisk two-slot state selector. The oracle is
 * exact slot selection or rejection for CRC, version, UUID, and wraparound
 * generation cases; no filesystem or daemon timing is involved.
 */
#include "config.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "segdisk_crc.h"
#include "segdisk_state.h"

/* Keep the unit binary's dependency files in the test-owning subtree. This
 * also avoids recursive distclean ordering dependencies on runtime/.deps. */
#include "../../runtime/segdisk_crc.c"
#include "../../runtime/segdisk_state.c"

#define CHECK(condition)                                                                    \
    do {                                                                                    \
        if (!(condition)) {                                                                 \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            exit(1);                                                                        \
        }                                                                                   \
    } while (0)

static void repair_crc(unsigned char slot[SEGDISK_STATE_SLOT_LEN]) {
    const uint32_t crc = segdiskCrc32c(slot, SEGDISK_STATE_SLOT_LEN - 4);
    slot[SEGDISK_STATE_SLOT_LEN - 4] = (unsigned char)(crc >> 24);
    slot[SEGDISK_STATE_SLOT_LEN - 3] = (unsigned char)(crc >> 16);
    slot[SEGDISK_STATE_SLOT_LEN - 2] = (unsigned char)(crc >> 8);
    slot[SEGDISK_STATE_SLOT_LEN - 1] = (unsigned char)crc;
}

static void init_state(segdisk_state_image_t *state, unsigned char uuid_byte) {
    memset(state, 0, sizeof(*state));
    memset(state->uuid, uuid_byte, sizeof(state->uuid));
    state->committed_offset = 52;
    state->next_segment = 1;
    state->delete_first = 2;
    state->delete_last = 4;
    state->delete_bytes = 12345;
    state->delete_segments = 3;
}

static void expect_selected(const unsigned char slot0[SEGDISK_STATE_SLOT_LEN],
                            const unsigned char slot1[SEGDISK_STATE_SLOT_LEN],
                            int expected) {
    segdisk_state_image_t selected;
    int selected_slot = -1;
    CHECK(segdiskStateSelect(slot0, 1, slot1, 1, &selected, &selected_slot) == RS_RET_OK);
    CHECK(selected_slot == expected);
    CHECK(selected.delete_first == 2);
    CHECK(selected.delete_last == 4);
    CHECK(selected.delete_bytes == 12345);
    CHECK(selected.delete_segments == 3);
}

int main(void) {
    /* RFC 3720's canonical CRC32C vector detects polynomial, reflection, and
     * lookup-table regressions independently of the state-slot test data. The
     * empty, zero, ramp, and 32-KiB patterned vectors also cover table-index
     * boundaries and the large-message path used by queue records. */
    CHECK(segdiskCrc32c("123456789", 9) == UINT32_C(0xe3069283));
    CHECK(segdiskCrc32c("", 0) == UINT32_C(0));
    unsigned char vector[32768] = {0};
    CHECK(segdiskCrc32c(vector, 32) == UINT32_C(0x8a9136aa));
    for (size_t i = 0; i < 256; ++i) vector[i] = (unsigned char)i;
    CHECK(segdiskCrc32c(vector, 256) == UINT32_C(0x9c44184b));
    for (size_t i = 0; i < sizeof(vector); ++i) vector[i] = (unsigned char)(i * 37 + 11);
    CHECK(segdiskCrc32c(vector, sizeof(vector)) == UINT32_C(0xff0064c4));

    segdisk_state_image_t state;
    unsigned char slot0[SEGDISK_STATE_SLOT_LEN];
    unsigned char slot1[SEGDISK_STATE_SLOT_LEN];
    init_state(&state, 0x11);

    segdiskStateEncode(&state, 10, slot0);
    segdiskStateEncode(&state, 11, slot1);
    expect_selected(slot0, slot1, 1);

    slot1[200] ^= 1;
    expect_selected(slot0, slot1, 0);
    slot0[201] ^= 1;
    CHECK(segdiskStateSelect(slot0, 1, slot1, 1, &state, NULL) == RS_RET_INVALID_VALUE);

    segdiskStateEncode(&state, UINT64_MAX, slot0);
    segdiskStateEncode(&state, 0, slot1);
    expect_selected(slot0, slot1, 1);

    segdiskStateEncode(&state, 0, slot0);
    segdiskStateEncode(&state, UINT64_C(1) << 63, slot1);
    CHECK(segdiskStateSelect(slot0, 1, slot1, 1, &state, NULL) == RS_RET_INVALID_VALUE);

    segdiskStateEncode(&state, 20, slot0);
    segdiskStateEncode(&state, 21, slot1);
    slot1[8] = 0;
    slot1[9] = 1;
    repair_crc(slot1);
    expect_selected(slot0, slot1, 0);

    segdiskStateEncode(&state, 21, slot1);
    slot1[10] = 0;
    slot1[11] ^= 1;
    repair_crc(slot1);
    expect_selected(slot0, slot1, 0);

    segdiskStateEncode(&state, 21, slot1);
    slot1[12] ^= 1;
    /* Recalculate the CRC by encoding a second valid image with another UUID. */
    segdisk_state_image_t other = state;
    memset(other.uuid, 0x22, sizeof(other.uuid));
    segdiskStateEncode(&other, 21, slot1);
    CHECK(segdiskStateSelect(slot0, 1, slot1, 1, &state, NULL) == RS_RET_INVALID_VALUE);

    puts("segmentedDisk state-slot tests passed");
    return 0;
}
