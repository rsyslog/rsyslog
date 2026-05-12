/*
 * Minimal Lumberjack v2 parser helpers for imbeats.
 *
 * This layer deliberately stays transport-agnostic. Network reads, ACK timing,
 * and rsyslog message construction live in imbeats.c.
 */
#include "config.h"
#include "lj_parser.h"

#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <zlib.h>

static rsRetVal append_event_owned(struct lj_batch_s *batch, uint32_t seq, unsigned char *payload, size_t payload_len) {
    if (batch == NULL || batch->events == NULL) {
        free(payload);
        return RS_RET_PARAM_ERROR;
    }
    if (batch->count >= batch->window_size) {
        free(payload);
        return RS_RET_INVALID_VALUE;
    }

    batch->events[batch->count].seq = seq;
    batch->events[batch->count].payload = payload;
    batch->events[batch->count].payload_len = payload_len;
    ++batch->count;
    return RS_RET_OK;
}

rsRetVal lj_batch_alloc(struct lj_batch_s *batch,
                        uint32_t window_size,
                        uint32_t max_window_size,
                        size_t max_payload_len) {
    if (batch == NULL || window_size == 0 || window_size > max_window_size || max_payload_len == 0) {
        return RS_RET_PARAM_ERROR;
    }
#if SIZE_MAX <= UINT32_MAX
    if ((size_t)window_size > SIZE_MAX / sizeof(struct lj_event_s)) {
        return RS_RET_OUT_OF_MEMORY;
    }
#endif
    memset(batch, 0, sizeof(*batch));
    batch->window_size = window_size;
    batch->max_payload_len = max_payload_len;
    batch->events = calloc(window_size, sizeof(struct lj_event_s));
    return (batch->events == NULL) ? RS_RET_OUT_OF_MEMORY : RS_RET_OK;
}

void lj_batch_free(struct lj_batch_s *batch) {
    size_t i;
    if (batch == NULL) {
        return;
    }
    for (i = 0; i < batch->count; ++i) {
        free(batch->events[i].payload);
    }
    free(batch->events);
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
    if (batch == NULL || payload == NULL || payload_len == 0) {
        return RS_RET_PARAM_ERROR;
    }
    if (payload_len > batch->max_payload_len || batch->total_payload_len > batch->max_payload_len - payload_len) {
        return RS_RET_INVALID_VALUE;
    }
    if (payload_len > SIZE_MAX - 1) {
        return RS_RET_OUT_OF_MEMORY;
    }
    cpy = malloc(payload_len + 1);
    if (cpy == NULL) {
        return RS_RET_OUT_OF_MEMORY;
    }
    memcpy(cpy, payload, payload_len);
    cpy[payload_len] = '\0';
    iRet = append_event_owned(batch, seq, cpy, payload_len);
    if (iRet == RS_RET_OK) {
        batch->total_payload_len += payload_len;
    }
    return iRet;
}

static rsRetVal parse_frames_from_memory(struct lj_batch_s *batch,
                                         const unsigned char *buf,
                                         size_t len,
                                         size_t max_frame_size) {
    size_t off = 0;

    while (off + 2 <= len) {
        const unsigned char ver = buf[off++];
        const unsigned char type = buf[off++];
        uint32_t v1, v2;
        rsRetVal iRet;

        if (ver != LJ_VERSION_V2) {
            return RS_RET_INVALID_VALUE;
        }

        switch (type) {
            case LJ_FRAME_JSON:
                if (off + 8 > len) {
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
                if (off + 4 > len) {
                    return RS_RET_INVALID_VALUE;
                }
                memcpy(&v1, buf + off, 4);
                off += 4;
                v1 = ntohl(v1);
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
                                    size_t max_decompressed_size) {
    z_stream zstrm;
    unsigned char *out = NULL;
    size_t out_cap = 0;
    size_t out_len = 0;
    size_t old_count;
    int zrc;
    rsRetVal iRet = RS_RET_OK;

    if (batch == NULL || payload == NULL || max_frame_size == 0 || max_decompressed_size == 0) {
        return RS_RET_PARAM_ERROR;
    }
    if (payload_len == 0 || payload_len > max_frame_size) {
        return RS_RET_INVALID_VALUE;
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
            if (out_cap >= max_decompressed_size) {
                iRet = RS_RET_INVALID_VALUE;
                goto finalize_it;
            }
            if (new_cap < out_cap || new_cap > max_decompressed_size) {
                new_cap = max_decompressed_size;
            }
            unsigned char *tmp = realloc(out, new_cap);
            if (tmp == NULL) {
                iRet = RS_RET_OUT_OF_MEMORY;
                goto finalize_it;
            }
            out = tmp;
            out_cap = new_cap;
        }

        zstrm.next_out = out + out_len;
        zstrm.avail_out = out_cap - out_len;
        zrc = inflate(&zstrm, Z_NO_FLUSH);
        out_len = out_cap - zstrm.avail_out;
        if (zrc != Z_OK && zrc != Z_STREAM_END) {
            iRet = RS_RET_INVALID_VALUE;
            goto finalize_it;
        }
    } while (zrc != Z_STREAM_END);

    if (zstrm.avail_in != 0) {
        iRet = RS_RET_INVALID_VALUE;
        goto finalize_it;
    }

    if (out_len == 0) {
        iRet = RS_RET_INVALID_VALUE;
        goto finalize_it;
    }

    iRet = parse_frames_from_memory(batch, out, out_len, max_frame_size);
    if (iRet == RS_RET_OK && batch->count == old_count) {
        iRet = RS_RET_INVALID_VALUE;
    }

finalize_it:
    inflateEnd(&zstrm);
    free(out);
    return iRet;
}
