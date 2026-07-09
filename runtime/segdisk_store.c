/*
 * Log-structured serial queue store.
 *
 * Concurrency & locking: callers hold the owning queue mutex for every public
 * operation. The store therefore owns no mutex in v1. Batch contexts may be
 * completed out of order, but the durable frontier advances only in dequeue
 * order.
 */
#include "config.h"
#include "rsyslog.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "errmsg.h"
#include "segdisk_codec.h"
#include "segdisk_store.h"

#define META_MAGIC "RSSEGQ01"
#define SEG_MAGIC "RSSEGH01"
#define REC_MAGIC "RSRECD01"
#define FOOT_MAGIC "RSSEAL01"
#define CKPT_MAGIC "RSCKPT01"
#define STORE_VERSION 1u
#define META_LEN 32u
#define SEG_HDR_LEN 52u
#define REC_HDR_LEN 32u
#define FOOT_LEN 48u
#define CKPT_LEN 64u
#define MAX_RECORD_SIZE (128u * 1024u * 1024u)

typedef struct segdisk_segment_s {
    uint64_t id;
    uint64_t first_ordinal;
    uint64_t last_ordinal;
    uint64_t record_count;
    int64_t data_end;
    int64_t file_size;
    uint32_t rolling_crc;
    sbool sealed;
    char *path;
    struct segdisk_segment_s *next;
} segdisk_segment_t;

typedef struct segdisk_batch_ctx_s {
    uint64_t sequence;
    uint64_t end_segment;
    uint64_t end_ordinal;
    int64_t end_offset;
    /* Physical records consumed, including corrupt records salvaged past.
     * Recovery counts every framed record in iQueueSize, so completion must
     * retire skipped records too even though no message reaches a worker. */
    int records;
    int retry_index;
    int retry_count;
    unsigned int refs;
    sbool pending;
    sbool complete;
    struct segdisk_batch_ctx_s *next;
} segdisk_batch_ctx_t;

struct segdisk_store_s {
    char *dir;
    char *queue_name;
    int dir_fd;
    int active_fd;
    segdisk_store_config_t cfg;
    unsigned char uuid[16];
    uint64_t generation;
    uint64_t committed_segment;
    uint64_t committed_ordinal;
    int64_t committed_offset;
    uint64_t next_segment;
    uint64_t next_ordinal;
    uint64_t read_ordinal;
    segdisk_segment_t *read_segment;
    int64_t read_offset;
    segdisk_segment_t *segments;
    segdisk_segment_t *active;
    segdisk_batch_ctx_t *pending_head;
    segdisk_batch_ctx_t *pending_tail;
    uint64_t next_batch_sequence;
    unsigned int updates_since_checkpoint;
    segdisk_store_stats_t stats;
};

static void update_retry_overage(segdisk_store_t *s);

static int sync_file_data(const int fd) {
#if defined(HAVE_FDATASYNC) && !defined(__APPLE__)
    return fdatasync(fd);
#else
    return fsync(fd);
#endif
}

static void release_batch_ctx(segdisk_batch_ctx_t *ctx) {
    if (--ctx->refs == 0) free(ctx);
}

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

static rsRetVal read_full_at(int fd, void *buf, size_t len, int64_t off) {
    unsigned char *p = buf;
    while (len != 0) {
        const ssize_t n = pread(fd, p, len, off);
        if (n == 0) return RS_RET_EOF;
        if (n < 0) {
            if (errno == EINTR) continue;
            return RS_RET_IO_ERROR;
        }
        p += n;
        len -= (size_t)n;
        off += n;
    }
    return RS_RET_OK;
}

static rsRetVal read_full(int fd, void *buf, size_t len) {
    unsigned char *p = buf;
    while (len != 0) {
        const ssize_t n = read(fd, p, len);
        if (n == 0) return RS_RET_EOF;
        if (n < 0) {
            if (errno == EINTR) continue;
            return RS_RET_IO_ERROR;
        }
        p += n;
        len -= (size_t)n;
    }
    return RS_RET_OK;
}

static rsRetVal write_full(int fd, const void *buf, size_t len) {
    const unsigned char *p = buf;
    while (len != 0) {
        const ssize_t n = write(fd, p, len);
        if (n < 0) {
            if (errno == EINTR) continue;
            return RS_RET_IO_ERROR;
        }
        p += n;
        len -= (size_t)n;
    }
    return RS_RET_OK;
}

static char *path_join(const char *a, const char *b) {
    char *p = NULL;
    if (asprintf(&p, "%s/%s", a, b) < 0) return NULL;
    return p;
}

static char *segment_path(const segdisk_store_t *s, uint64_t id, sbool sealed) {
    char name[64];
    snprintf(name, sizeof(name), "segment-%020" PRIu64 ".%s", id, sealed ? "seg" : "open");
    return path_join(s->dir, name);
}

static rsRetVal sync_dir(segdisk_store_t *s) {
    return fsync(s->dir_fd) == 0 || errno == EINVAL || errno == ENOTSUP ? RS_RET_OK : RS_RET_IO_ERROR;
}

static rsRetVal random_uuid(unsigned char uuid[16]) {
    const int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (fd < 0) return RS_RET_IO_ERROR;
    const rsRetVal r = read_full(fd, uuid, 16);
    close(fd);
    if (r == RS_RET_OK) {
        uuid[6] = (uuid[6] & 0x0f) | 0x40;
        uuid[8] = (uuid[8] & 0x3f) | 0x80;
    }
    return r;
}

static rsRetVal write_meta(segdisk_store_t *s) {
    unsigned char b[META_LEN] = {0};
    memcpy(b, META_MAGIC, 8);
    put16(b + 8, STORE_VERSION);
    put16(b + 10, SEGDISK_CODEC_VERSION);
    memcpy(b + 12, s->uuid, 16);
    put32(b + 28, segdiskCrc32c(b, 28));
    const int fd = openat(s->dir_fd, "meta.tmp", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (fd < 0) return RS_RET_IO_ERROR;
    rsRetVal r = write_full(fd, b, sizeof(b));
    if (r == RS_RET_OK && fsync(fd) != 0) r = RS_RET_IO_ERROR;
    if (close(fd) != 0 && r == RS_RET_OK) r = RS_RET_IO_ERROR;
    if (r == RS_RET_OK && renameat(s->dir_fd, "meta.tmp", s->dir_fd, "meta") != 0) r = RS_RET_IO_ERROR;
    if (r == RS_RET_OK) r = sync_dir(s);
    return r;
}

static rsRetVal read_meta(segdisk_store_t *s) {
    unsigned char b[META_LEN];
    const int fd = openat(s->dir_fd, "meta", O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return errno == ENOENT ? RS_RET_FILE_NOT_FOUND : RS_RET_IO_ERROR;
    rsRetVal r = read_full_at(fd, b, sizeof(b), 0);
    close(fd);
    if (r != RS_RET_OK || memcmp(b, META_MAGIC, 8) || get16(b + 8) != STORE_VERSION ||
        get16(b + 10) != SEGDISK_CODEC_VERSION || get32(b + 28) != segdiskCrc32c(b, 28))
        return RS_RET_INVALID_VALUE;
    memcpy(s->uuid, b + 12, 16);
    return RS_RET_OK;
}

static rsRetVal write_checkpoint(segdisk_store_t *s, sbool force_sync) {
    unsigned char b[CKPT_LEN] = {0};
    memcpy(b, CKPT_MAGIC, 8);
    put16(b + 8, STORE_VERSION);
    put16(b + 10, SEGDISK_CODEC_VERSION);
    memcpy(b + 12, s->uuid, 16);
    put64(b + 28, ++s->generation);
    put64(b + 36, s->committed_segment);
    put64(b + 44, (uint64_t)s->committed_offset);
    put64(b + 52, s->committed_ordinal);
    put32(b + 60, segdiskCrc32c(b, 60));
    const int fd = openat(s->dir_fd, "checkpoint.tmp", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (fd < 0) return RS_RET_IO_ERROR;
    rsRetVal r = write_full(fd, b, sizeof(b));
    if (r == RS_RET_OK && (s->cfg.sync_files || force_sync) && fsync(fd) != 0) r = RS_RET_IO_ERROR;
    if (close(fd) != 0 && r == RS_RET_OK) r = RS_RET_IO_ERROR;
    if (r == RS_RET_OK && renameat(s->dir_fd, "checkpoint.tmp", s->dir_fd, "checkpoint") != 0) r = RS_RET_IO_ERROR;
    if (r == RS_RET_OK && (s->cfg.sync_files || force_sync)) r = sync_dir(s);
    if (r == RS_RET_OK) {
        ++s->stats.checkpoints;
        s->updates_since_checkpoint = 0;
    }
    return r;
}

static rsRetVal read_checkpoint(segdisk_store_t *s) {
    unsigned char b[CKPT_LEN];
    const int fd = openat(s->dir_fd, "checkpoint", O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return errno == ENOENT ? RS_RET_FILE_NOT_FOUND : RS_RET_IO_ERROR;
    rsRetVal r = read_full_at(fd, b, sizeof(b), 0);
    close(fd);
    if (r != RS_RET_OK || memcmp(b, CKPT_MAGIC, 8) || get16(b + 8) != STORE_VERSION || memcmp(b + 12, s->uuid, 16) ||
        get32(b + 60) != segdiskCrc32c(b, 60))
        return RS_RET_INVALID_VALUE;
    s->generation = get64(b + 28);
    s->committed_segment = get64(b + 36);
    s->committed_offset = (int64_t)get64(b + 44);
    s->committed_ordinal = get64(b + 52);
    return RS_RET_OK;
}

static rsRetVal write_segment_header(segdisk_store_t *s, int fd, uint64_t id, uint64_t first) {
    unsigned char b[SEG_HDR_LEN] = {0};
    memcpy(b, SEG_MAGIC, 8);
    put16(b + 8, STORE_VERSION);
    put16(b + 10, SEGDISK_CODEC_VERSION);
    memcpy(b + 12, s->uuid, 16);
    put64(b + 28, id);
    put64(b + 36, first);
    put32(b + 44, SEG_HDR_LEN);
    put32(b + 48, segdiskCrc32c(b, 48));
    return write_full(fd, b, sizeof(b));
}

static rsRetVal validate_segment_header(segdisk_store_t *s, int fd, segdisk_segment_t *seg) {
    unsigned char b[SEG_HDR_LEN];
    rsRetVal r = read_full_at(fd, b, sizeof(b), 0);
    if (r != RS_RET_OK || memcmp(b, SEG_MAGIC, 8) || get16(b + 8) != STORE_VERSION ||
        get16(b + 10) != SEGDISK_CODEC_VERSION || memcmp(b + 12, s->uuid, 16) || get32(b + 44) != SEG_HDR_LEN ||
        get32(b + 48) != segdiskCrc32c(b, 48))
        return RS_RET_INVALID_VALUE;
    seg->id = get64(b + 28);
    seg->first_ordinal = get64(b + 36);
    return RS_RET_OK;
}

static rsRetVal create_active(segdisk_store_t *s) {
    segdisk_segment_t *seg = calloc(1, sizeof(*seg));
    if (seg == NULL) return RS_RET_OUT_OF_MEMORY;
    seg->id = s->next_segment++;
    seg->first_ordinal = s->next_ordinal;
    seg->data_end = SEG_HDR_LEN;
    seg->file_size = SEG_HDR_LEN;
    seg->sealed = 0;
    seg->path = segment_path(s, seg->id, 0);
    if (seg->path == NULL) {
        free(seg);
        return RS_RET_OUT_OF_MEMORY;
    }
    const int fd = open(seg->path, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (fd < 0) {
        free(seg->path);
        free(seg);
        return RS_RET_IO_ERROR;
    }
    rsRetVal r = write_segment_header(s, fd, seg->id, seg->first_ordinal);
    if (r != RS_RET_OK) {
        close(fd);
        unlink(seg->path);
        free(seg->path);
        free(seg);
        return r;
    }
    if (lseek(fd, 0, SEEK_END) < 0) {
        close(fd);
        unlink(seg->path);
        free(seg->path);
        free(seg);
        return RS_RET_IO_ERROR;
    }
    segdisk_segment_t **tail = &s->segments;
    while (*tail != NULL) tail = &(*tail)->next;
    *tail = seg;
    s->active = seg;
    s->active_fd = fd;
    ++s->stats.segments;
    s->stats.bytes += SEG_HDR_LEN;
    return RS_RET_OK;
}

static rsRetVal seal_active(segdisk_store_t *s) {
    if (s->active == NULL || s->active->sealed) return RS_RET_OK;
    unsigned char b[FOOT_LEN] = {0};
    memcpy(b, FOOT_MAGIC, 8);
    put64(b + 8, s->active->id);
    put64(b + 16, s->active->first_ordinal);
    put64(b + 24, s->active->last_ordinal);
    put64(b + 32, s->active->record_count);
    put32(b + 40, s->active->rolling_crc);
    put32(b + 44, segdiskCrc32c(b, 44));
    rsRetVal r = write_full(s->active_fd, b, sizeof(b));
    if (r == RS_RET_OK && s->cfg.sync_files && sync_file_data(s->active_fd) != 0) r = RS_RET_IO_ERROR;
    if (close(s->active_fd) != 0 && r == RS_RET_OK) r = RS_RET_IO_ERROR;
    s->active_fd = -1;
    if (r != RS_RET_OK) return r;
    char *sealed = segment_path(s, s->active->id, 1);
    if (sealed == NULL) return RS_RET_OUT_OF_MEMORY;
    if (rename(s->active->path, sealed) != 0) {
        free(sealed);
        return RS_RET_IO_ERROR;
    }
    free(s->active->path);
    s->active->path = sealed;
    s->active->sealed = 1;
    s->active->file_size += FOOT_LEN;
    s->stats.bytes += FOOT_LEN;
    s->active = NULL;
    return s->cfg.sync_files ? sync_dir(s) : RS_RET_OK;
}

static rsRetVal make_record(smsg_t *msg, uint64_t ordinal, unsigned char **out, size_t *outlen) {
    unsigned char *payload = NULL;
    size_t payload_len = 0;
    rsRetVal r = segdiskCodecEncode(msg, &payload, &payload_len);
    if (r != RS_RET_OK) return r;
    if (payload_len > MAX_RECORD_SIZE) {
        free(payload);
        return RS_RET_FILE_TOO_LARGE;
    }
    const size_t len = REC_HDR_LEN + payload_len;
    unsigned char *record = malloc(len);
    if (record == NULL) {
        free(payload);
        return RS_RET_OUT_OF_MEMORY;
    }
    memset(record, 0, REC_HDR_LEN);
    memcpy(record, REC_MAGIC, 8);
    put16(record + 8, STORE_VERSION);
    put16(record + 10, 0);
    put32(record + 12, (uint32_t)payload_len);
    put64(record + 16, ordinal);
    put32(record + 24, segdiskCrc32c(record, 24));
    put32(record + 28, segdiskCrc32c(payload, payload_len));
    memcpy(record + REC_HDR_LEN, payload, payload_len);
    free(payload);
    *out = record;
    *outlen = len;
    return RS_RET_OK;
}

static rsRetVal append_record(segdisk_store_t *s, smsg_t *msg, sbool internal, int64_t *written) {
    unsigned char *record = NULL;
    size_t len = 0;
    rsRetVal r = make_record(msg, s->next_ordinal, &record, &len);
    if (r != RS_RET_OK) return r;
    int64_t needed = (int64_t)len;
    if (s->active == NULL) needed += SEG_HDR_LEN;
    const sbool rotate = s->active != NULL && s->active->record_count != 0 && s->cfg.max_file_size > 0 &&
                         s->active->file_size + (int64_t)len + FOOT_LEN > s->cfg.max_file_size;
    if (rotate) needed += FOOT_LEN + SEG_HDR_LEN;
    const int64_t target_size =
        rotate ? SEG_HDR_LEN + (int64_t)len
               : (s->active == NULL ? SEG_HDR_LEN + (int64_t)len : s->active->file_size + (int64_t)len);
    if (s->cfg.max_file_size > 0 && target_size + FOOT_LEN > s->cfg.max_file_size) needed += FOOT_LEN;
    if (!internal && s->cfg.max_disk_space > 0 && s->stats.bytes + needed > s->cfg.max_disk_space) {
        free(record);
        return RS_RET_QUEUE_FULL;
    }
    if (s->active == NULL) {
        r = create_active(s);
        if (r != RS_RET_OK) {
            free(record);
            return r;
        }
    }
    if (s->active->record_count != 0 && s->cfg.max_file_size > 0 &&
        s->active->file_size + (int64_t)len + FOOT_LEN > s->cfg.max_file_size) {
        r = seal_active(s);
        if (r == RS_RET_OK) r = create_active(s);
        if (r != RS_RET_OK) {
            free(record);
            return r;
        }
    }
    r = write_full(s->active_fd, record, len);
    if (r == RS_RET_OK && s->cfg.sync_files && sync_file_data(s->active_fd) != 0) r = RS_RET_IO_ERROR;
    if (r == RS_RET_OK) {
        s->active->data_end += len;
        s->active->file_size += len;
        s->active->last_ordinal = s->next_ordinal++;
        ++s->active->record_count;
        s->active->rolling_crc ^= segdiskCrc32c(record, REC_HDR_LEN);
        s->active->rolling_crc ^= segdiskCrc32c(record + REC_HDR_LEN, len - REC_HDR_LEN);
        s->stats.bytes += len;
        if (written != NULL) *written = len;
    }
    free(record);
    if (r == RS_RET_OK && s->active->record_count == 1 && s->cfg.max_file_size > 0 &&
        s->active->file_size + FOOT_LEN > s->cfg.max_file_size)
        r = seal_active(s);
    if (r == RS_RET_OK && internal) update_retry_overage(s);
    return r;
}

static int segment_name(const char *name, uint64_t *id, sbool *sealed) {
    char suffix[8];
    unsigned long long value;
    if (sscanf(name, "segment-%20llu.%7s", &value, suffix) != 2) return 0;
    if (!strcmp(suffix, "seg"))
        *sealed = 1;
    else if (!strcmp(suffix, "open"))
        *sealed = 0;
    else
        return 0;
    *id = value;
    return 1;
}

static rsRetVal directory_has_segments(segdisk_store_t *s, sbool *has_segment) {
    const int scan_fd = openat(s->dir_fd, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (scan_fd < 0) return RS_RET_IO_ERROR;
    DIR *dir = fdopendir(scan_fd);
    if (dir == NULL) {
        close(scan_fd);
        return RS_RET_IO_ERROR;
    }
    *has_segment = 0;
    struct dirent *de;
    while ((de = readdir(dir)) != NULL) {
        uint64_t id;
        sbool sealed;
        if (segment_name(de->d_name, &id, &sealed)) {
            *has_segment = 1;
            break;
        }
    }
    closedir(dir);
    return RS_RET_OK;
}

static void update_retry_overage(segdisk_store_t *s) {
    s->stats.retry_overage_bytes = s->cfg.max_disk_space > 0 && s->stats.bytes > s->cfg.max_disk_space
                                       ? s->stats.bytes - s->cfg.max_disk_space
                                       : 0;
    if (s->stats.retry_overage_bytes > s->stats.retry_overage_max_bytes)
        s->stats.retry_overage_max_bytes = s->stats.retry_overage_bytes;
}

static void insert_segment(segdisk_store_t *s, segdisk_segment_t *seg) {
    segdisk_segment_t **p = &s->segments;
    while (*p != NULL && (*p)->id < seg->id) p = &(*p)->next;
    seg->next = *p;
    *p = seg;
}

static rsRetVal scan_segment(segdisk_store_t *s, segdisk_segment_t *seg, int *queue_size) {
    const int fd = open(seg->path, O_RDWR | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return RS_RET_IO_ERROR;
    struct stat st;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
        close(fd);
        return RS_RET_IO_ERROR;
    }
    rsRetVal r = validate_segment_header(s, fd, seg);
    if (r != RS_RET_OK) {
        close(fd);
        return r;
    }
    int64_t end = st.st_size;
    sbool footer_valid = 0;
    uint64_t footer_first = 0, footer_last = 0, footer_count = 0;
    uint32_t footer_rolling = 0;
    if (seg->sealed) {
        if (end < (int64_t)(SEG_HDR_LEN + FOOT_LEN)) {
            close(fd);
            return RS_RET_INVALID_VALUE;
        }
        unsigned char foot[FOOT_LEN];
        r = read_full_at(fd, foot, sizeof(foot), end - FOOT_LEN);
        if (r == RS_RET_OK && !memcmp(foot, FOOT_MAGIC, 8) && get64(foot + 8) == seg->id &&
            get32(foot + 44) == segdiskCrc32c(foot, 44)) {
            footer_valid = 1;
            footer_first = get64(foot + 16);
            footer_last = get64(foot + 24);
            footer_count = get64(foot + 32);
            footer_rolling = get32(foot + 40);
            end -= FOOT_LEN;
        } else {
            ++s->stats.corruption_events;
            /* load_segments() treats this as a recovered active segment and
             * immediately reseals it, replacing the invalid footer in-place. */
            seg->sealed = 0;
        }
    }
    int64_t off = SEG_HDR_LEN, last_valid = off;
    uint64_t count = 0, last_ord = 0;
    int live_count = 0;
    uint32_t rolling = 0;
    while (off + (int64_t)REC_HDR_LEN <= end) {
        unsigned char h[REC_HDR_LEN];
        r = read_full_at(fd, h, sizeof(h), off);
        if (r != RS_RET_OK) break;
        if (memcmp(h, REC_MAGIC, 8) || get16(h + 8) != STORE_VERSION || get32(h + 24) != segdiskCrc32c(h, 24) ||
            get32(h + 12) > MAX_RECORD_SIZE || off + REC_HDR_LEN + get32(h + 12) > end) {
            ++s->stats.corruption_events;
            ++s->stats.corruption_bytes;
            ++off;
            continue;
        }
        const uint32_t n = get32(h + 12);
        unsigned char *payload = malloc(n == 0 ? 1 : n);
        if (payload == NULL) {
            close(fd);
            return RS_RET_OUT_OF_MEMORY;
        }
        r = read_full_at(fd, payload, n, off + REC_HDR_LEN);
        if (r != RS_RET_OK) {
            free(payload);
            break;
        }
        const uint32_t crc = segdiskCrc32c(payload, n);
        const uint64_t ord = get64(h + 16);
        rolling ^= segdiskCrc32c(h, sizeof(h));
        rolling ^= crc;
        free(payload);
        if (count == 0) seg->first_ordinal = ord;
        last_ord = ord;
        ++count;
        if (ord > s->committed_ordinal) ++live_count;
        off += REC_HDR_LEN + n;
        last_valid = off;
        if (crc != get32(h + 28)) {
            ++s->stats.corruption_events;
            ++s->stats.corruption_records;
        }
    }
    if (footer_valid && (footer_first != seg->first_ordinal || footer_last != last_ord || footer_count != count ||
                         footer_rolling != rolling)) {
        ++s->stats.corruption_events;
        seg->sealed = 0;
        if (ftruncate(fd, end) != 0) r = RS_RET_IO_ERROR;
    }
    if (!seg->sealed && last_valid < end) {
        if (ftruncate(fd, last_valid) != 0) r = RS_RET_IO_ERROR;
        end = last_valid;
    }
    seg->last_ordinal = last_ord;
    seg->record_count = count;
    seg->rolling_crc = rolling;
    seg->data_end = end;
    seg->file_size = end + (seg->sealed ? FOOT_LEN : 0);
    if (seg->id >= s->next_segment) s->next_segment = seg->id + 1;
    if (last_ord >= s->next_ordinal) s->next_ordinal = last_ord + 1;
    close(fd);
    if (r == RS_RET_EOF) r = RS_RET_OK;
    if (r == RS_RET_OK) *queue_size += live_count;
    return r;
}

static rsRetVal load_segments(segdisk_store_t *s, int *queue_size) {
    const int scan_fd = openat(s->dir_fd, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (scan_fd < 0) return RS_RET_IO_ERROR;
    DIR *dir = fdopendir(scan_fd);
    if (dir == NULL) {
        close(scan_fd);
        return RS_RET_IO_ERROR;
    }
    struct dirent *de;
    rsRetVal r = RS_RET_OK;
    while ((de = readdir(dir)) != NULL) {
        uint64_t id;
        sbool sealed;
        if (!segment_name(de->d_name, &id, &sealed)) continue;
        segdisk_segment_t *seg = calloc(1, sizeof(*seg));
        if (seg == NULL) {
            r = RS_RET_OUT_OF_MEMORY;
            break;
        }
        seg->id = id;
        seg->sealed = sealed;
        if (id >= s->next_segment) s->next_segment = id + 1;
        seg->path = path_join(s->dir, de->d_name);
        if (seg->path == NULL) {
            free(seg);
            r = RS_RET_OUT_OF_MEMORY;
            break;
        }
        insert_segment(s, seg);
    }
    closedir(dir);
    if (r != RS_RET_OK) return r;
    segdisk_segment_t **next = &s->segments;
    while (*next != NULL) {
        segdisk_segment_t *const seg = *next;
        r = scan_segment(s, seg, queue_size);
        if (r != RS_RET_OK) {
            DBGPRINTF("%s: segmentedDisk scan of '%s' failed with return code %d, errno %d\n", s->queue_name, seg->path,
                      r, errno);
            LogError(0, r, "%s: segmentedDisk could not validate segment '%s'; skipping it", s->queue_name, seg->path);
            ++s->stats.corruption_events;
            *next = seg->next;
            free(seg->path);
            free(seg);
            continue;
        }
        ++s->stats.segments;
        s->stats.bytes += seg->file_size;
        if (!seg->sealed) {
            s->active = seg;
            s->active_fd = open(seg->path, O_RDWR | O_APPEND | O_CLOEXEC | O_NOFOLLOW);
            if (s->active_fd < 0) return RS_RET_IO_ERROR;
            r = seal_active(s);
            if (r != RS_RET_OK) return r;
        }
        next = &seg->next;
    }
    return RS_RET_OK;
}

static rsRetVal record_at(
    segdisk_store_t *s, segdisk_segment_t *seg, int64_t *off, smsg_t **msg, uint64_t *ordinal, int *skipped) {
    const int fd = open(seg->path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return RS_RET_IO_ERROR;
    while (*off + (int64_t)REC_HDR_LEN <= seg->data_end) {
        unsigned char h[REC_HDR_LEN];
        rsRetVal r = read_full_at(fd, h, sizeof(h), *off);
        if (r != RS_RET_OK) {
            close(fd);
            return r;
        }
        if (memcmp(h, REC_MAGIC, 8) || get16(h + 8) != STORE_VERSION || get32(h + 24) != segdiskCrc32c(h, 24) ||
            get32(h + 12) > MAX_RECORD_SIZE || *off + REC_HDR_LEN + get32(h + 12) > seg->data_end) {
            ++s->stats.corruption_events;
            ++s->stats.corruption_bytes;
            ++*off;
            continue;
        }
        const uint32_t n = get32(h + 12);
        unsigned char *payload = malloc(n == 0 ? 1 : n);
        if (payload == NULL) {
            close(fd);
            return RS_RET_OUT_OF_MEMORY;
        }
        r = read_full_at(fd, payload, n, *off + REC_HDR_LEN);
        const uint64_t ord = get64(h + 16);
        *ordinal = ord;
        if (r != RS_RET_OK || segdiskCrc32c(payload, n) != get32(h + 28) ||
            segdiskCodecDecode(payload, n, msg) != RS_RET_OK) {
            free(payload);
            ++s->stats.corruption_events;
            ++s->stats.corruption_records;
            ++*skipped;
            *off += REC_HDR_LEN + n;
            continue;
        }
        free(payload);
        *off += REC_HDR_LEN + n;
        *ordinal = ord;
        close(fd);
        return RS_RET_OK;
    }
    close(fd);
    return RS_RET_NO_DATA;
}

static segdisk_segment_t *first_readable(segdisk_store_t *s, uint64_t after) {
    for (segdisk_segment_t *seg = s->segments; seg != NULL; seg = seg->next)
        if (seg->last_ordinal > after) return seg;
    return NULL;
}

static rsRetVal delete_committed(segdisk_store_t *s, sbool *frontier_durable) {
    *frontier_durable = 0;
    if (s->active != NULL && s->active->record_count != 0 && s->active->last_ordinal <= s->committed_ordinal &&
        s->pending_head == NULL) {
        rsRetVal r = seal_active(s);
        if (r != RS_RET_OK) return r;
        r = create_active(s);
        if (r != RS_RET_OK) return r;
    }
    sbool needs_delete = 0;
    for (segdisk_segment_t *seg = s->segments; seg != NULL; seg = seg->next)
        if (seg->sealed && seg->last_ordinal != 0 && seg->last_ordinal <= s->committed_ordinal) needs_delete = 1;
    if (!needs_delete) return RS_RET_OK;
    if (s->active_fd >= 0 && sync_file_data(s->active_fd) != 0) return RS_RET_IO_ERROR;
    rsRetVal r = write_checkpoint(s, 1);
    if (r != RS_RET_OK) return r;
    *frontier_durable = 1;
    segdisk_segment_t **p = &s->segments;
    while (*p != NULL) {
        segdisk_segment_t *seg = *p;
        if (!seg->sealed || seg->last_ordinal == 0 || seg->last_ordinal > s->committed_ordinal) {
            p = &seg->next;
            continue;
        }
        if (unlink(seg->path) != 0 && errno != ENOENT) return RS_RET_IO_ERROR;
        s->stats.bytes -= seg->file_size;
        --s->stats.segments;
        *p = seg->next;
        if (s->read_segment == seg) {
            s->read_segment = NULL;
            s->read_offset = SEG_HDR_LEN;
        }
        free(seg->path);
        free(seg);
    }
    update_retry_overage(s);
    return sync_dir(s);
}

static void append_pending(segdisk_store_t *s, segdisk_batch_ctx_t *ctx) {
    if (s->pending_tail == NULL)
        s->pending_head = s->pending_tail = ctx;
    else {
        s->pending_tail->next = ctx;
        s->pending_tail = ctx;
    }
}

static rsRetVal advance_completed(segdisk_store_t *s) {
    unsigned int advanced = 0;
    segdisk_batch_ctx_t *last = NULL;
    for (segdisk_batch_ctx_t *ctx = s->pending_head; ctx != NULL && ctx->complete; ctx = ctx->next) {
        last = ctx;
        advanced += (unsigned int)ctx->records;
    }
    if (last == NULL) return RS_RET_OK;
    const uint64_t old_segment = s->committed_segment;
    const int64_t old_offset = s->committed_offset;
    const uint64_t old_ordinal = s->committed_ordinal;
    const unsigned int old_updates = s->updates_since_checkpoint;
    s->committed_segment = last->end_segment;
    s->committed_offset = last->end_offset;
    s->committed_ordinal = last->end_ordinal;
    s->updates_since_checkpoint += advanced;
    rsRetVal r = RS_RET_OK;
    sbool frontier_durable = 0;
    if (s->cfg.checkpoint_interval == 0 || s->updates_since_checkpoint >= s->cfg.checkpoint_interval) {
        r = write_checkpoint(s, 0);
        if (r == RS_RET_OK) frontier_durable = 1;
    }
    sbool delete_durable = 0;
    if (r == RS_RET_OK) r = delete_committed(s, &delete_durable);
    frontier_durable |= delete_durable;
    if (r != RS_RET_OK && !frontier_durable) {
        s->committed_segment = old_segment;
        s->committed_offset = old_offset;
        s->committed_ordinal = old_ordinal;
        s->updates_since_checkpoint = old_updates;
        return r;
    }
    if (r != RS_RET_OK) {
        LogError(0, r, "%s: segmentedDisk cleanup failed after the completion frontier was durable; deferring cleanup",
                 s->queue_name);
        r = RS_RET_OK;
    }
    segdisk_batch_ctx_t *const after = last->next;
    while (s->pending_head != after) {
        segdisk_batch_ctx_t *ctx = s->pending_head;
        s->pending_head = ctx->next;
        ctx->pending = 0;
        release_batch_ctx(ctx);
    }
    if (s->pending_head == NULL) s->pending_tail = NULL;
    return r;
}

rsRetVal segdiskStoreOpen(segdisk_store_t **out, const segdisk_store_config_t *cfg, int *queue_size) {
    if (out == NULL || cfg == NULL || cfg->work_dir == NULL || cfg->file_prefix == NULL || queue_size == NULL)
        return RS_RET_PARAM_ERROR;
    if (cfg->file_prefix[0] == '\0' || strchr(cfg->file_prefix, '/') != NULL ||
        strchr(cfg->file_prefix, '\\') != NULL || !strcmp(cfg->file_prefix, ".") || !strcmp(cfg->file_prefix, ".."))
        return RS_RET_INVALID_VALUE;
    const char *stage = "allocate store";
    segdisk_store_t *s = calloc(1, sizeof(*s));
    if (s == NULL) return RS_RET_OUT_OF_MEMORY;
    s->dir_fd = s->active_fd = -1;
    s->cfg = *cfg;
    s->next_segment = s->next_ordinal = 1;
    s->queue_name = strdup(cfg->queue_name == NULL ? cfg->file_prefix : cfg->queue_name);
    if (s->queue_name == NULL || asprintf(&s->dir, "%s/%s.segq", cfg->work_dir, cfg->file_prefix) < 0) goto oom;
    char *legacy_qi = NULL;
    if (asprintf(&legacy_qi, "%s/%s.qi", cfg->work_dir, cfg->file_prefix) < 0) goto oom;
    struct stat legacy_st;
    if (lstat(legacy_qi, &legacy_st) == 0) {
        LogError(0, RS_RET_INVALID_VALUE, "%s: segmentedDisk refuses legacy queue state '%s'", s->queue_name,
                 legacy_qi);
        free(legacy_qi);
        goto invalid;
    }
    if (errno != ENOENT) {
        free(legacy_qi);
        goto ioerr;
    }
    free(legacy_qi);
    stage = "create queue directory";
    if (mkdir(s->dir, 0700) != 0) {
        const int mkdir_errno = errno;
        if (mkdir_errno != EEXIST) {
            DBGPRINTF("%s: segmentedDisk mkdir '%s' failed with errno %d\n", s->queue_name, s->dir, mkdir_errno);
            errno = mkdir_errno;
            goto ioerr;
        }
    }
    stage = "open queue directory";
    s->dir_fd = open(s->dir, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (s->dir_fd < 0) goto ioerr;
    struct stat st;
    if (fstat(s->dir_fd, &st) != 0 || !S_ISDIR(st.st_mode)) goto ioerr;
    sbool has_segment = 0;
    rsRetVal r = directory_has_segments(s, &has_segment);
    if (r != RS_RET_OK) goto fail;
    stage = "read metadata";
    r = read_meta(s);
    if (r == RS_RET_FILE_NOT_FOUND) {
        if (has_segment) {
            LogError(0, RS_RET_INVALID_VALUE,
                     "%s: segmentedDisk metadata is missing; replay requires a valid segment UUID", s->queue_name);
            goto invalid;
        }
        if ((r = random_uuid(s->uuid)) != RS_RET_OK || (r = write_meta(s)) != RS_RET_OK) goto fail;
    } else if (r != RS_RET_OK)
        goto invalid;
    r = read_checkpoint(s);
    if (r != RS_RET_OK) {
        s->generation = s->committed_segment = s->committed_ordinal = 0;
        s->committed_offset = SEG_HDR_LEN;
        if (r != RS_RET_FILE_NOT_FOUND || has_segment)
            LogMsg(0, RS_RET_OK, LOG_WARNING,
                   "%s: segmentedDisk checkpoint missing or invalid; replaying all valid records", s->queue_name);
    }
    stage = "load segments";
    *queue_size = 0;
    if ((r = load_segments(s, queue_size)) != RS_RET_OK) goto fail;
    stage = "create active segment";
    if (s->active == NULL && (r = create_active(s)) != RS_RET_OK) goto fail;
    s->read_ordinal = s->committed_ordinal;
    s->read_segment = first_readable(s, s->read_ordinal);
    s->read_offset = SEG_HDR_LEN;
    if (s->read_segment != NULL && s->read_segment->id == s->committed_segment && s->committed_offset >= SEG_HDR_LEN &&
        s->committed_offset <= s->read_segment->data_end)
        s->read_offset = s->committed_offset;
    s->stats.replayed = *queue_size;
    *out = s;
    return RS_RET_OK;
oom:
    r = RS_RET_OUT_OF_MEMORY;
    goto fail;
ioerr:
    r = RS_RET_IO_ERROR;
    goto fail;
invalid:
    r = RS_RET_INVALID_VALUE;
fail:
    if (r != RS_RET_OK)
        DBGPRINTF("%s: segmentedDisk failed to %s: return code %d, errno %d\n",
                  s->queue_name == NULL ? "queue" : s->queue_name, stage, r, errno);
    if (s->active_fd >= 0) close(s->active_fd);
    while (s->segments != NULL) {
        segdisk_segment_t *next = s->segments->next;
        free(s->segments->path);
        free(s->segments);
        s->segments = next;
    }
    while (s->pending_head != NULL) {
        segdisk_batch_ctx_t *next = s->pending_head->next;
        s->pending_head->pending = 0;
        release_batch_ctx(s->pending_head);
        s->pending_head = next;
    }
    if (s->dir_fd >= 0) close(s->dir_fd);
    free(s->dir);
    free(s->queue_name);
    free(s);
    return r;
}

rsRetVal segdiskStoreAppend(segdisk_store_t *s, smsg_t *msg, sbool internal, int64_t *written) {
    return append_record(s, msg, internal, written);
}

rsRetVal segdiskStoreDequeueBatch(segdisk_store_t *s, batch_t *batch, int max, int *skipped) {
    if (batch->storeData != NULL) return RS_RET_INTERNAL_ERROR;
    segdisk_batch_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) return RS_RET_OUT_OF_MEMORY;
    ctx->sequence = s->next_batch_sequence++;
    ctx->refs = 1;
    ctx->pending = 1;
    int n = 0;
    int consumed = 0;
    *skipped = 0;
    if (s->read_segment == NULL) s->read_segment = first_readable(s, s->read_ordinal);
    while (n < max) {
        if (s->read_segment == NULL) break;
        smsg_t *msg = NULL;
        uint64_t ord = 0;
        const uint64_t read_before = s->read_ordinal;
        const int skipped_before = *skipped;
        rsRetVal r = record_at(s, s->read_segment, &s->read_offset, &msg, &ord, skipped);
        consumed += *skipped - skipped_before;
        if (ord > s->read_ordinal) {
            s->read_ordinal = ord;
            ctx->end_segment = s->read_segment->id;
            ctx->end_offset = s->read_offset;
            ctx->end_ordinal = ord;
        }
        if (r == RS_RET_NO_DATA) {
            s->read_segment = s->read_segment->next;
            s->read_offset = SEG_HDR_LEN;
            continue;
        }
        if (r != RS_RET_OK) {
            free(ctx);
            return r;
        }
        if (ord <= read_before) {
            msgDestruct(&msg);
            continue;
        }
        ++consumed;
        batch->pElem[n].pMsg = msg;
        batch->eltState[n] = BATCH_STATE_RDY;
        ++n;
    }
    if (consumed == 0) {
        free(ctx);
        return RS_RET_NO_DATA;
    }
    ctx->records = consumed;
    append_pending(s, ctx);
    if (n == 0) {
        ctx->complete = 1;
        const rsRetVal r = advance_completed(s);
        return r == RS_RET_OK ? RS_RET_NO_DATA : r;
    }
    batch->storeData = ctx;
    ++ctx->refs;
    batch->nElem = n;
    batch->nElemDeq = ctx->records;
    return RS_RET_OK;
}

rsRetVal segdiskStoreCompleteBatch(segdisk_store_t *s, batch_t *batch, int *committed, int *retried) {
    segdisk_batch_ctx_t *ctx = batch->storeData;
    if (ctx == NULL) return RS_RET_INTERNAL_ERROR;
    *committed = ctx->records;
    if (!ctx->pending) {
        *retried = ctx->retry_count;
        batch->storeData = NULL;
        release_batch_ctx(ctx);
        return RS_RET_OK;
    }
    while (ctx->retry_index < batch->nElem) {
        const int i = ctx->retry_index;
        if (batch->eltState[i] == BATCH_STATE_RDY || batch->eltState[i] == BATCH_STATE_SUB) {
            rsRetVal r = append_record(s, batch->pElem[i].pMsg, 1, NULL);
            if (r != RS_RET_OK) {
                *retried = ctx->retry_count;
                return r;
            }
            ++ctx->retry_count;
        }
        ++ctx->retry_index;
    }
    *retried = ctx->retry_count;
    ctx->complete = 1;
    const rsRetVal r = advance_completed(s);
    if (r == RS_RET_OK) {
        batch->storeData = NULL;
        release_batch_ctx(ctx);
    }
    return r;
}

rsRetVal segdiskStoreCheckpoint(segdisk_store_t *s, sbool force_sync) {
    return write_checkpoint(s, force_sync);
}

rsRetVal segdiskStoreClose(segdisk_store_t **ps, sbool empty) {
    if (ps == NULL || *ps == NULL) return RS_RET_OK;
    segdisk_store_t *s = *ps;
    rsRetVal r = RS_RET_OK;
    if (s->active_fd >= 0) {
        if (!empty && s->active != NULL && s->active->record_count != 0)
            r = seal_active(s);
        else
            close(s->active_fd);
        s->active_fd = -1;
    }
    if (empty && s->dir_fd >= 0) {
        for (segdisk_segment_t *seg = s->segments; seg != NULL; seg = seg->next) {
            if (unlink(seg->path) != 0 && errno != ENOENT && r == RS_RET_OK) r = RS_RET_IO_ERROR;
        }
        const char *const control_files[] = {"checkpoint", "checkpoint.tmp", "meta", "meta.tmp"};
        for (size_t i = 0; i < sizeof(control_files) / sizeof(control_files[0]); ++i) {
            if (unlinkat(s->dir_fd, control_files[i], 0) != 0 && errno != ENOENT && r == RS_RET_OK) r = RS_RET_IO_ERROR;
        }
    } else if (s->dir_fd >= 0 && r == RS_RET_OK) {
        r = write_checkpoint(s, 1);
    }
    while (s->segments != NULL) {
        segdisk_segment_t *n = s->segments->next;
        free(s->segments->path);
        free(s->segments);
        s->segments = n;
    }
    while (s->pending_head != NULL) {
        segdisk_batch_ctx_t *n = s->pending_head->next;
        s->pending_head->pending = 0;
        release_batch_ctx(s->pending_head);
        s->pending_head = n;
    }
    if (s->dir_fd >= 0) close(s->dir_fd);
    if (empty && rmdir(s->dir) != 0 && errno != ENOENT && r == RS_RET_OK) r = RS_RET_IO_ERROR;
    free(s->dir);
    free(s->queue_name);
    free(s);
    *ps = NULL;
    return r;
}

void segdiskStoreGetStats(const segdisk_store_t *s, segdisk_store_stats_t *stats) {
    *stats = s->stats;
}
