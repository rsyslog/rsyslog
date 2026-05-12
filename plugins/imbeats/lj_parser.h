#ifndef IMBEATS_LJ_PARSER_H
#define IMBEATS_LJ_PARSER_H

#include "config.h"
#include <stddef.h>
#include <stdint.h>
#include "rsyslog.h"

#define LJ_VERSION_V2 ((unsigned char)'2')
#define LJ_FRAME_WINDOW ((unsigned char)'W')
#define LJ_FRAME_JSON ((unsigned char)'J')
#define LJ_FRAME_COMPRESSED ((unsigned char)'C')
#define LJ_FRAME_ACK ((unsigned char)'A')

struct lj_event_s {
    uint32_t seq;
    unsigned char *payload;
    size_t payload_len;
};

struct lj_batch_s {
    uint32_t window_size;
    size_t count;
    size_t total_payload_len;
    size_t max_payload_len;
    struct lj_event_s *events;
};

rsRetVal lj_batch_alloc(struct lj_batch_s *batch,
                        uint32_t window_size,
                        uint32_t max_window_size,
                        size_t max_payload_len);
void lj_batch_free(struct lj_batch_s *batch);
rsRetVal lj_parse_window_header(const unsigned char hdr[2], uint32_t window_size);
rsRetVal lj_append_json_event(struct lj_batch_s *batch, uint32_t seq, const unsigned char *payload, size_t payload_len);
rsRetVal lj_parse_compressed_frames(struct lj_batch_s *batch,
                                    const unsigned char *payload,
                                    size_t payload_len,
                                    size_t max_frame_size,
                                    size_t max_decompressed_size);

#endif
