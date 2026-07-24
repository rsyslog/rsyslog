/*
 * Minimal Lumberjack v2 parser helpers for imbeats.
 *
 * This layer deliberately stays transport-agnostic. Network reads, ACK timing,
 * and rsyslog message construction live in imbeats.c.
 */
#include "config.h"
#include "lj_parser.h"

#include <arpa/inet.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <zlib.h>

static rsRetVal reserve_resource(struct lj_batch_s *batch, const size_t bytes) {
    if (bytes == 0 || batch->reserve == NULL) {
        return RS_RET_OK;
    }
    return batch->reserve(batch->resource_ctx, bytes);
}

static void release_resource(struct lj_batch_s *batch, const size_t bytes) {
    if (bytes != 0 && batch->release != NULL) {
        batch->release(batch->resource_ctx, bytes);
    }
}

static rsRetVal ensure_event_capacity(struct lj_batch_s *batch) {
    struct lj_event_s *events;
    size_t new_capacity;
    size_t new_alloc_len;
    size_t additional_len;
    rsRetVal iRet;

    if (batch->count < batch->events_capacity) {
        return RS_RET_OK;
    }
    if (batch->count >= batch->window_size) {
        return RS_RET_INVALID_VALUE;
    }

    new_capacity = (batch->events_capacity == 0) ? 8 : batch->events_capacity * 2;
    if (new_capacity < batch->events_capacity || new_capacity > batch->window_size) {
        new_capacity = batch->window_size;
    }
    if (new_capacity > SIZE_MAX / sizeof(*events)) {
        return RS_RET_OUT_OF_MEMORY;
    }
    new_alloc_len = new_capacity * sizeof(*events);
    additional_len = new_alloc_len - batch->events_alloc_len;
    iRet = reserve_resource(batch, additional_len);
    if (iRet != RS_RET_OK) {
        return iRet;
    }
    events = realloc(batch->events, new_alloc_len);
    if (events == NULL) {
        release_resource(batch, additional_len);
        return RS_RET_OUT_OF_MEMORY;
    }
    memset(events + batch->events_capacity, 0, (new_capacity - batch->events_capacity) * sizeof(*events));
    batch->events = events;
    batch->events_capacity = new_capacity;
    batch->events_alloc_len = new_alloc_len;
    return RS_RET_OK;
}

static rsRetVal validate_event(struct lj_batch_s *batch, const unsigned char *payload, const size_t payload_len) {
    if (batch == NULL || payload == NULL || payload_len == 0) {
        return RS_RET_PARAM_ERROR;
    }
    if (batch->count >= batch->window_size || payload_len > batch->max_payload_len ||
        batch->total_payload_len > batch->max_payload_len - payload_len) {
        return RS_RET_INVALID_VALUE;
    }
    return RS_RET_OK;
}

rsRetVal lj_append_json_event_owned(struct lj_batch_s *batch,
                                    const uint32_t seq,
                                    unsigned char *payload,
                                    const size_t payload_len,
                                    const size_t payload_alloc_len) {
    rsRetVal iRet;

    iRet = validate_event(batch, payload, payload_len);
    if (iRet != RS_RET_OK) {
        return iRet;
    }
    if (payload_alloc_len < payload_len) {
        return RS_RET_PARAM_ERROR;
    }
    iRet = ensure_event_capacity(batch);
    if (iRet != RS_RET_OK) {
        return iRet;
    }

    batch->events[batch->count].seq = seq;
    batch->events[batch->count].payload = payload;
    batch->events[batch->count].payload_len = payload_len;
    batch->events[batch->count].payload_alloc_len = payload_alloc_len;
    ++batch->count;
    batch->total_payload_len += payload_len;
    return RS_RET_OK;
}

rsRetVal lj_batch_alloc(struct lj_batch_s *batch,
                        uint32_t window_size,
                        uint32_t max_window_size,
                        size_t max_payload_len,
                        lj_resource_reserve_cb reserve,
                        lj_resource_release_cb release,
                        void *resource_ctx) {
    if (batch == NULL || window_size == 0 || window_size > max_window_size || max_payload_len == 0) {
        return RS_RET_PARAM_ERROR;
    }
    if ((reserve == NULL) != (release == NULL)) {
        return RS_RET_PARAM_ERROR;
    }
    memset(batch, 0, sizeof(*batch));
    batch->window_size = window_size;
    batch->max_payload_len = max_payload_len;
    batch->reserve = reserve;
    batch->release = release;
    batch->resource_ctx = resource_ctx;
    return RS_RET_OK;
}

void lj_batch_free(struct lj_batch_s *batch) {
    size_t i;
    if (batch == NULL) {
        return;
    }
    for (i = 0; i < batch->count; ++i) {
        free(batch->events[i].payload);
        release_resource(batch, batch->events[i].payload_alloc_len);
    }
    free(batch->events);
    release_resource(batch, batch->events_alloc_len);
    memset(batch, 0, sizeof(*batch));
}

rsRetVal lj_parse_window_header(const unsigned char hdr[2], uint32_t window_size) {
    if (hdr[0] != LJ_VERSION_V2 || hdr[1] != LJ_FRAME_WINDOW || window_size == 0) {
        return RS_RET_INVALID_VALUE;
    }
    return RS_RET_OK;
}

rsRetVal lj_append_json_event(struct lj_batch_s *batch,
                              uint32_t seq,
                              const unsigned char *payload,
                              size_t payload_len) {
    unsigned char *cpy;
    rsRetVal iRet;

    iRet = validate_event(batch, payload, payload_len);
    if (iRet != RS_RET_OK) return iRet;
    if (payload_len > SIZE_MAX - 1) {
        return RS_RET_OUT_OF_MEMORY;
    }
    iRet = ensure_event_capacity(batch);
    if (iRet != RS_RET_OK) return iRet;
    iRet = reserve_resource(batch, payload_len + 1);
    if (iRet != RS_RET_OK) return iRet;
    cpy = malloc(payload_len + 1);
    if (cpy == NULL) {
        release_resource(batch, payload_len + 1);
        return RS_RET_OUT_OF_MEMORY;
    }
    memcpy(cpy, payload, payload_len);
    cpy[payload_len] = '\0';
    iRet = lj_append_json_event_owned(batch, seq, cpy, payload_len, payload_len + 1);
    if (iRet != RS_RET_OK) {
        free(cpy);
        release_resource(batch, payload_len + 1);
    }
    return iRet;
}

static rsRetVal parse_frames_from_memory(struct lj_batch_s *batch,
                                         const unsigned char *buf,
                                         size_t len,
                                         size_t max_frame_size) {
    size_t off = 0;

    while (len - off >= 2) {
        const unsigned char ver = buf[off++];
        const unsigned char type = buf[off++];
        uint32_t v1, v2;
        rsRetVal iRet;

        if (ver != LJ_VERSION_V2) {
            return RS_RET_INVALID_VALUE;
        }

        switch (type) {
            case LJ_FRAME_JSON:
                if (len - off < 8) {
                    return RS_RET_INVALID_VALUE;
                }
                memcpy(&v1, buf + off, 4);
                memcpy(&v2, buf + off + 4, 4);
                off += 8;
                v1 = ntohl(v1);
                v2 = ntohl(v2);
                if (v2 == 0 || (size_t)v2 > max_frame_size || (size_t)v2 > len - off) {
                    return RS_RET_INVALID_VALUE;
                }
                iRet = lj_append_json_event(batch, v1, buf + off, v2);
                if (iRet != RS_RET_OK) {
                    return iRet;
                }
                off += v2;
                break;
            case LJ_FRAME_COMPRESSED:
                /* Compressed frames are only valid as top-level session frames.
                 * After inflation this helper accepts JSON frames only; accepting
                 * another compressed frame would enable unsupported nesting. */
                return RS_RET_INVALID_VALUE;
            default:
                return RS_RET_INVALID_VALUE;
        }
    }

    return (off == len) ? RS_RET_OK : RS_RET_INVALID_VALUE;
}

rsRetVal lj_parse_compressed_frames(struct lj_batch_s *batch,
                                    const unsigned char *payload,
                                    size_t payload_len,
                                    size_t max_frame_size,
                                    size_t max_decompressed_size,
                                    uint32_t max_compression_ratio) {
    z_stream zstrm;
    unsigned char *out = NULL;
    size_t out_cap = 0;
    size_t out_len = 0;
    size_t expansion_limit;
    size_t old_count;
    int zrc;
    rsRetVal iRet = RS_RET_OK;

    if (batch == NULL || payload == NULL || max_frame_size == 0 || max_decompressed_size == 0 ||
        max_compression_ratio == 0) {
        return RS_RET_PARAM_ERROR;
    }
    if (payload_len == 0 || payload_len > max_frame_size) {
        return RS_RET_INVALID_VALUE;
    }
#if SIZE_MAX > UINT_MAX
    if (payload_len > UINT_MAX) {
        return RS_RET_INVALID_VALUE;
    }
#endif
    expansion_limit =
        (payload_len > SIZE_MAX / max_compression_ratio) ? SIZE_MAX : payload_len * (size_t)max_compression_ratio;
    if (expansion_limit > max_decompressed_size) {
        expansion_limit = max_decompressed_size;
    }
    old_count = batch->count;

    memset(&zstrm, 0, sizeof(zstrm));
    zstrm.next_in = (Bytef *)payload;
    zstrm.avail_in = payload_len;

    zrc = inflateInit(&zstrm);
    if (zrc != Z_OK) {
        return RS_RET_ZLIB_ERR;
    }

    do {
        if (out_len == out_cap) {
            size_t new_cap = (out_cap == 0) ? 4096 : out_cap * 2;
            size_t additional_len;
            rsRetVal reserveRet;

            if (out_cap >= expansion_limit) {
                unsigned char overflow_probe;
                const uInt avail_in_before = zstrm.avail_in;

                /* Give zlib one byte of unaccounted probe space so a stream
                 * ending exactly at the configured limit can consume its
                 * trailer. Producing that byte proves the limit was exceeded.
                 * When inflate returns Z_STREAM_END without output, continue
                 * evaluates the do-while condition and exits the loop. */
                zstrm.next_out = &overflow_probe;
                zstrm.avail_out = 1;
                zrc = inflate(&zstrm, Z_NO_FLUSH);
                if (zstrm.avail_out == 0 || (zrc != Z_OK && zrc != Z_STREAM_END) ||
                    (zrc == Z_OK && zstrm.avail_in == avail_in_before)) {
                    iRet = RS_RET_INVALID_VALUE;
                    goto finalize_it;
                }
                continue;
            }
            if (new_cap < out_cap || new_cap > expansion_limit) {
                new_cap = expansion_limit;
            }
            additional_len = new_cap - out_cap;
            reserveRet = reserve_resource(batch, additional_len);
            if (reserveRet != RS_RET_OK) {
                iRet = reserveRet;
                goto finalize_it;
            }
            unsigned char *tmp = realloc(out, new_cap);
            if (tmp == NULL) {
                release_resource(batch, additional_len);
                iRet = RS_RET_OUT_OF_MEMORY;
                goto finalize_it;
            }
            out = tmp;
            out_cap = new_cap;
        }

        const size_t available = out_cap - out_len;
#if SIZE_MAX > UINT_MAX
        const uInt offered = (available > UINT_MAX) ? UINT_MAX : (uInt)available;
#else
        const uInt offered = (uInt)available;
#endif
        const uInt avail_in_before = zstrm.avail_in;
        size_t produced;
        zstrm.next_out = out + out_len;
        zstrm.avail_out = offered;
        zrc = inflate(&zstrm, Z_NO_FLUSH);
        produced = offered - zstrm.avail_out;
        out_len += produced;
        if (zrc != Z_OK && zrc != Z_STREAM_END) {
            iRet = RS_RET_INVALID_VALUE;
            goto finalize_it;
        }
        if (zrc == Z_OK && produced == 0 && zstrm.avail_in == avail_in_before) {
            iRet = RS_RET_INVALID_VALUE;
            goto finalize_it;
        }
    } while (zrc != Z_STREAM_END);

    if (zstrm.avail_in != 0) {
        iRet = RS_RET_INVALID_VALUE;
        goto finalize_it;
    }

    /* Empty deflate streams are invalid Lumberjack payloads and must not
     * produce an ACK-able no-op compressed frame. */
    if (out_len == 0) {
        iRet = RS_RET_INVALID_VALUE;
        goto finalize_it;
    }

    iRet = parse_frames_from_memory(batch, out, out_len, max_frame_size);
    /* A syntactically valid compressed frame must advance the current batch.
     * Treat no-progress payloads as malformed input instead of spinning. */
    if (iRet == RS_RET_OK && batch->count == old_count) {
        iRet = RS_RET_INVALID_VALUE;
    }

finalize_it:
    inflateEnd(&zstrm);
    free(out);
    release_resource(batch, out_cap);
    return iRet;
}
