/*
 * Exercise imbeats' transport-independent Lumberjack parser resource and
 * framing invariants. The deterministic oracle is the parser return code,
 * parsed event content, and an exact reserve/release balance after batch
 * cleanup; no timing or live listener behavior is involved.
 */
#include "config.h"

#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "rsyslog.h"
#include "plugins/imbeats/lj_parser.h"

/* Compile the parser into this test translation unit. Keeping all test
 * sources below tests/ avoids Automake dependency-file paths that are not
 * portable to older make versions, while still exercising production code. */
#include "../../plugins/imbeats/lj_parser.c"

#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) {                                                               \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
            return 1;                                                                \
        }                                                                            \
    } while (0)

struct resource_account_s {
    size_t current;
    size_t peak;
    size_t reserved;
    size_t released;
    size_t limit;
};

static rsRetVal reserve_bytes(void *const ctx, const size_t bytes) {
    struct resource_account_s *const account = ctx;

    if (bytes > account->limit - account->current) {
        return RS_RET_OUT_OF_MEMORY;
    }
    account->current += bytes;
    account->reserved += bytes;
    if (account->current > account->peak) account->peak = account->current;
    return RS_RET_OK;
}

static void release_bytes(void *const ctx, const size_t bytes) {
    struct resource_account_s *const account = ctx;

    if (bytes > account->current) {
        fprintf(stderr, "resource accounting underflow: current=%zu release=%zu\n", account->current, bytes);
        abort();
    }
    account->current -= bytes;
    account->released += bytes;
}

static rsRetVal init_batch(struct lj_batch_s *const batch,
                           struct resource_account_s *const account,
                           const uint32_t window_size,
                           const size_t max_payload_len) {
    memset(account, 0, sizeof(*account));
    account->limit = SIZE_MAX;
    return lj_batch_alloc(batch, window_size, window_size, max_payload_len, reserve_bytes, release_bytes, account);
}

static size_t put_json_frame(unsigned char *const dst,
                             const uint32_t seq,
                             const unsigned char *const payload,
                             const size_t payload_len) {
    const uint32_t net_seq = htonl(seq);
    const uint32_t net_len = htonl((uint32_t)payload_len);

    dst[0] = LJ_VERSION_V2;
    dst[1] = LJ_FRAME_JSON;
    memcpy(dst + 2, &net_seq, sizeof(net_seq));
    memcpy(dst + 6, &net_len, sizeof(net_len));
    memcpy(dst + 10, payload, payload_len);
    return 10 + payload_len;
}

static int compress_bytes(const unsigned char *const input,
                          const size_t input_len,
                          unsigned char **const output,
                          size_t *const output_len) {
    uLongf compressed_len = compressBound(input_len);

    *output = malloc(compressed_len);
    if (*output == NULL) return 1;
    if (compress2(*output, &compressed_len, input, input_len, Z_BEST_COMPRESSION) != Z_OK) {
        free(*output);
        *output = NULL;
        return 1;
    }
    *output_len = compressed_len;
    return 0;
}

static int test_lazy_allocation_and_cleanup(void) {
    struct lj_batch_s batch;
    struct resource_account_s account;
    static const unsigned char payload[] = "one";
    const size_t descriptor_bytes = 3 * sizeof(struct lj_event_s);

    CHECK(init_batch(&batch, &account, 3, 64) == RS_RET_OK);
    CHECK(batch.events == NULL);
    CHECK(account.current == 0);
    CHECK(lj_append_json_event(&batch, 1, payload, sizeof(payload) - 1) == RS_RET_OK);
    CHECK(batch.events_capacity == 3);
    CHECK(batch.events_alloc_len == descriptor_bytes);
    CHECK(batch.events[0].payload_alloc_len == sizeof(payload));
    CHECK(account.current == descriptor_bytes + sizeof(payload));
    lj_batch_free(&batch);
    CHECK(account.current == 0);
    CHECK(account.reserved == account.released);
    return 0;
}

static int test_budget_rejection_is_balanced(void) {
    struct lj_batch_s batch;
    struct resource_account_s account;
    static const unsigned char payload[] = "budget";
    const size_t descriptor_bytes = sizeof(struct lj_event_s);

    CHECK(init_batch(&batch, &account, 1, 64) == RS_RET_OK);
    account.limit = descriptor_bytes + sizeof(payload) - 1;
    CHECK(lj_append_json_event(&batch, 1, payload, sizeof(payload) - 1) == RS_RET_OUT_OF_MEMORY);
    CHECK(batch.count == 0);
    CHECK(account.current == descriptor_bytes);
    lj_batch_free(&batch);
    CHECK(account.current == 0);
    CHECK(account.reserved == account.released);
    return 0;
}

static int test_owned_transfer_and_failure_contract(void) {
    struct lj_batch_s batch;
    struct resource_account_s account;
    unsigned char *owned;
    unsigned char *rejected;
    const size_t alloc_len = 6;

    CHECK(init_batch(&batch, &account, 1, 64) == RS_RET_OK);
    CHECK(reserve_bytes(&account, alloc_len) == RS_RET_OK);
    owned = malloc(alloc_len);
    CHECK(owned != NULL);
    memcpy(owned, "owned", alloc_len);
    CHECK(lj_append_json_event_owned(&batch, 9, owned, 5, alloc_len) == RS_RET_OK);
    CHECK(batch.events[0].payload == owned);
    CHECK(batch.events[0].payload_alloc_len == alloc_len);

    CHECK(reserve_bytes(&account, 2) == RS_RET_OK);
    rejected = malloc(2);
    CHECK(rejected != NULL);
    memcpy(rejected, "x", 2);
    CHECK(lj_append_json_event_owned(&batch, 10, rejected, 1, 2) == RS_RET_INVALID_VALUE);
    /* Failure retains caller ownership, so this free is required and is also
     * the oracle that the parser did not consume the second allocation. */
    free(rejected);
    release_bytes(&account, 2);

    lj_batch_free(&batch);
    CHECK(account.current == 0);
    CHECK(account.reserved == account.released);
    return 0;
}

static int test_valid_compressed_batch(void) {
    struct lj_batch_s batch;
    struct resource_account_s account;
    unsigned char frames[128];
    unsigned char *compressed = NULL;
    size_t frames_len = 0;
    size_t compressed_len = 0;

    frames_len += put_json_frame(frames + frames_len, 1, (const unsigned char *)"{\"a\":1}", 7);
    frames_len += put_json_frame(frames + frames_len, 2, (const unsigned char *)"{\"b\":2}", 7);
    CHECK(compress_bytes(frames, frames_len, &compressed, &compressed_len) == 0);
    CHECK(init_batch(&batch, &account, 2, 128) == RS_RET_OK);
    CHECK(lj_parse_compressed_frames(&batch, compressed, compressed_len, 128, 4096, 64) == RS_RET_OK);
    CHECK(batch.count == 2);
    CHECK(batch.events[0].seq == 1 && batch.events[1].seq == 2);
    CHECK(memcmp(batch.events[0].payload, "{\"a\":1}", 7) == 0);
    lj_batch_free(&batch);
    CHECK(account.current == 0);
    CHECK(account.reserved == account.released);

    /* A stream ending at the exact decompressed-size limit must consume its
     * zlib trailer and terminate. Success is the oracle against a boundary
     * spin or an off-by-one rejection. */
    CHECK(init_batch(&batch, &account, 2, 128) == RS_RET_OK);
    CHECK(lj_parse_compressed_frames(&batch, compressed, compressed_len, 128, frames_len, 64) == RS_RET_OK);
    CHECK(batch.count == 2);
    lj_batch_free(&batch);
    CHECK(account.current == 0);
    CHECK(account.reserved == account.released);

    CHECK(init_batch(&batch, &account, 2, 128) == RS_RET_OK);
    compressed = realloc(compressed, compressed_len + 1);
    CHECK(compressed != NULL);
    compressed[compressed_len++] = 0;
    CHECK(lj_parse_compressed_frames(&batch, compressed, compressed_len, 128, 4096, 64) == RS_RET_INVALID_VALUE);
    free(compressed);
    lj_batch_free(&batch);
    CHECK(account.current == 0);
    CHECK(account.reserved == account.released);
    return 0;
}

static int test_high_ratio_and_scratch_budget_rejection(void) {
    struct lj_batch_s batch;
    struct resource_account_s account;
    unsigned char *frames;
    unsigned char *compressed = NULL;
    unsigned char payload[8192];
    size_t compressed_len = 0;
    const size_t frames_len = sizeof(payload) + 10;

    memset(payload, 'A', sizeof(payload));
    frames = malloc(frames_len);
    CHECK(frames != NULL);
    CHECK(put_json_frame(frames, 1, payload, sizeof(payload)) == frames_len);
    CHECK(compress_bytes(frames, frames_len, &compressed, &compressed_len) == 0);
    free(frames);
    CHECK(frames_len > compressed_len * 2);

    CHECK(init_batch(&batch, &account, 1, 16384) == RS_RET_OK);
    CHECK(lj_parse_compressed_frames(&batch, compressed, compressed_len, 16384, 16384, 2) == RS_RET_INVALID_VALUE);
    CHECK(batch.count == 0);
    CHECK(account.current == 0);
    lj_batch_free(&batch);

    CHECK(init_batch(&batch, &account, 1, 16384) == RS_RET_OK);
    account.limit = 32;
    CHECK(lj_parse_compressed_frames(&batch, compressed, compressed_len, 16384, 16384, 1000) == RS_RET_OUT_OF_MEMORY);
    CHECK(batch.count == 0);
    CHECK(account.current == 0);
    free(compressed);
    lj_batch_free(&batch);
    CHECK(account.reserved == account.released);
    return 0;
}

static int expect_invalid_compressed(const unsigned char *const frames, const size_t frames_len) {
    struct lj_batch_s batch;
    struct resource_account_s account;
    unsigned char *compressed = NULL;
    size_t compressed_len = 0;

    CHECK(compress_bytes(frames, frames_len, &compressed, &compressed_len) == 0);
    CHECK(init_batch(&batch, &account, 2, 128) == RS_RET_OK);
    CHECK(lj_parse_compressed_frames(&batch, compressed, compressed_len, 128, 4096, 128) == RS_RET_INVALID_VALUE);
    free(compressed);
    lj_batch_free(&batch);
    CHECK(account.current == 0);
    CHECK(account.reserved == account.released);
    return 0;
}

static int test_truncation_and_invalid_framing(void) {
    static const unsigned char invalid_version[] = {'1', LJ_FRAME_JSON};
    static const unsigned char nested[] = {LJ_VERSION_V2, LJ_FRAME_COMPRESSED};
    unsigned char valid[32];
    unsigned char valid_with_trailing[32];
    size_t valid_len;
    size_t prefix_len;

    valid_len = put_json_frame(valid, 1, (const unsigned char *)"{}", 2);
    /* Every strict prefix is a distinct truncated-frame boundary. Each must
     * fail, while test_valid_compressed_batch supplies the positive control. */
    for (prefix_len = 0; prefix_len < valid_len; ++prefix_len) {
        CHECK(expect_invalid_compressed(valid, prefix_len) == 0);
    }
    CHECK(expect_invalid_compressed(invalid_version, sizeof(invalid_version)) == 0);
    CHECK(expect_invalid_compressed(nested, sizeof(nested)) == 0);
    valid_len = put_json_frame(valid_with_trailing, 1, (const unsigned char *)"{}", 2);
    valid_with_trailing[valid_len++] = LJ_VERSION_V2;
    CHECK(expect_invalid_compressed(valid_with_trailing, valid_len) == 0);
    return 0;
}

int main(void) {
    struct {
        const char *name;
        int (*fn)(void);
    } tests[] = {
        {"lazy_allocation_and_cleanup", test_lazy_allocation_and_cleanup},
        {"budget_rejection_is_balanced", test_budget_rejection_is_balanced},
        {"owned_transfer_and_failure_contract", test_owned_transfer_and_failure_contract},
        {"valid_compressed_batch", test_valid_compressed_batch},
        {"high_ratio_and_scratch_budget_rejection", test_high_ratio_and_scratch_budget_rejection},
        {"truncation_and_invalid_framing", test_truncation_and_invalid_framing},
    };
    size_t i;

    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        if (tests[i].fn() != 0) {
            fprintf(stderr, "FAILED: %s\n", tests[i].name);
            return 1;
        }
    }
    printf("imbeats parser tests passed (%zu cases)\n", sizeof(tests) / sizeof(tests[0]));
    return 0;
}
