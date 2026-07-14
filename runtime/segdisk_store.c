/*
 * Log-structured serial queue store.
 *
 * Concurrency & locking: callers hold the owning queue mutex for every public
 * operation. The store therefore owns no mutex. Batch contexts may complete
 * out of order, but the durable frontier advances only in dequeue order.
 *
 * Startup reads only the fixed-size state file. Existing segments are opened
 * and validated lazily as dequeue reaches them, so startup cost is independent
 * of backlog bytes and segment count.
 */
#include "config.h"
#include "rsyslog.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "errmsg.h"
#include "segdisk_codec.h"
#include "segdisk_crc.h"
#include "segdisk_state.h"
#include "segdisk_store.h"

#define SEG_MAGIC "RSSEGH02"
#define REC_MAGIC "RSRECD02"
#define FOOT_MAGIC "RSSEAL02"
#define STORE_VERSION 2u
#define STATE_SLOT_LEN SEGDISK_STATE_SLOT_LEN
#define STATE_FILE_LEN SEGDISK_STATE_FILE_LEN
#define SEG_HDR_LEN 52u
#define REC_HDR_LEN 32u
#define FOOT_LEN 48u
#define MAX_RECORD_SIZE (128u * 1024u * 1024u)
#define RECOVERY_SCAN_BUDGET (1024u * 1024u)
#define STATE_FLAG_RECOVERY 1u
#define STATE_FLAG_DEMATERIALIZING 2u

typedef struct segdisk_segment_s {
    uint64_t id;
    uint64_t first_sequence;
    uint64_t last_sequence;
    uint64_t record_count;
    int64_t data_end;
    int64_t file_size;
    uint32_t rolling_crc;
    sbool sealed;
    sbool recovery;
    char *path;
} segdisk_segment_t;

typedef struct segdisk_batch_ctx_s {
    uint64_t sequence;
    uint64_t end_segment;
    uint64_t end_record_sequence;
    int64_t end_offset;
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
    int state_fd;
    int active_fd;
    segdisk_store_config_t cfg;
    unsigned char uuid[16];
    uint64_t generation;
    uint64_t committed_segment;
    uint64_t committed_record_sequence;
    int64_t committed_offset;
    uint64_t first_live_segment;
    uint64_t last_data_segment;
    uint64_t active_segment;
    uint64_t recovery_first;
    uint64_t recovery_last;
    uint64_t next_segment;
    uint64_t persisted_queue_size;
    uint64_t persisted_writer_segment;
    int64_t persisted_writer_end;
    uint64_t persisted_writer_sequence;
    uint64_t persisted_writer_count;
    uint64_t delete_first;
    uint64_t delete_last;
    int64_t delete_bytes;
    uint64_t delete_segments;
    uint64_t discovery_through_segment;
    uint64_t read_segment_id;
    int64_t read_offset;
    segdisk_segment_t *read_segment;
    segdisk_segment_t *active;
    segdisk_batch_ctx_t *pending_head;
    segdisk_batch_ctx_t *pending_tail;
    uint64_t next_batch_sequence;
    uint64_t known_queue_size;
    unsigned int updates_since_checkpoint;
    sbool dematerializing;
    segdisk_store_stats_t stats;
#ifdef ENABLE_IMDIAG
    segdisk_test_fault_point_t test_fault_point;
    unsigned int test_fault_hits_remaining;
#endif
};

#ifdef ENABLE_IMDIAG
typedef struct segdisk_test_fault_name_s {
    const char *name;
    segdisk_test_fault_point_t point;
} segdisk_test_fault_name_t;

static const segdisk_test_fault_name_t test_fault_names[] = {
    {"reservation-published", SEGDISK_TEST_FAULT_RESERVATION_PUBLISHED},
    {"segment-created", SEGDISK_TEST_FAULT_SEGMENT_CREATED},
    {"seal-written", SEGDISK_TEST_FAULT_SEAL_WRITTEN},
    {"seal-renamed", SEGDISK_TEST_FAULT_SEAL_RENAMED},
    {"checkpoint-published", SEGDISK_TEST_FAULT_CHECKPOINT_PUBLISHED},
    {"commit-published", SEGDISK_TEST_FAULT_COMMIT_PUBLISHED},
    {"predelete-published", SEGDISK_TEST_FAULT_PREDELETE_PUBLISHED},
    {"segment-unlinked", SEGDISK_TEST_FAULT_SEGMENT_UNLINKED},
    {"idle-empty-published", SEGDISK_TEST_FAULT_IDLE_EMPTY_PUBLISHED},
    {"idle-segments-unlinked", SEGDISK_TEST_FAULT_IDLE_SEGMENTS_UNLINKED},
    {"idle-state-unlinked", SEGDISK_TEST_FAULT_IDLE_STATE_UNLINKED},
    {"idle-directory-removed", SEGDISK_TEST_FAULT_IDLE_DIRECTORY_REMOVED},
};

static void test_fault(segdisk_store_t *s, const segdisk_test_fault_point_t point) {
    if (s->test_fault_point != point || s->test_fault_hits_remaining == 0) return;
    if (--s->test_fault_hits_remaining != 0) return;
    s->test_fault_point = SEGDISK_TEST_FAULT_NONE;
    kill(getpid(), SIGKILL);
}
#else
    #define test_fault(store, point) ((void)0)
#endif

static void update_retry_overage(segdisk_store_t *s);

/* Directory enumeration is restricted to the exceptional missing-state path.
 * A valid state file keeps normal startup independent of segment count. */
static rsRetVal directory_has_entries(const int dir_fd, sbool *has_entries) {
    const int fd = dup(dir_fd);
    if (fd < 0) return RS_RET_IO_ERROR;
    DIR *const dir = fdopendir(fd);
    if (dir == NULL) {
        close(fd);
        return RS_RET_IO_ERROR;
    }
    *has_entries = 0;
    errno = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
            *has_entries = 1;
            break;
        }
    }
    const int saved_errno = errno;
    if (closedir(dir) != 0 || (entry == NULL && saved_errno != 0)) return RS_RET_IO_ERROR;
    return RS_RET_OK;
}

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

static rsRetVal pwrite_full(int fd, const void *buf, size_t len, int64_t off) {
    const unsigned char *p = buf;
    while (len != 0) {
        const ssize_t n = pwrite(fd, p, len, off);
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

static char *path_join(const char *a, const char *b) {
    char *p = NULL;
    if (asprintf(&p, "%s/%s", a, b) < 0) return NULL;
    return p;
}

static char *segment_path(const segdisk_store_t *s, uint64_t id, const char *suffix) {
    char name[64];
    snprintf(name, sizeof(name), "segment-%020" PRIu64 ".%s", id, suffix);
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

static void capture_state_image(const segdisk_store_t *s, segdisk_state_image_t *state) {
    memset(state, 0, sizeof(*state));
    memcpy(state->uuid, s->uuid, sizeof(state->uuid));
    state->generation = s->generation;
    state->flags =
        (s->recovery_first != 0 ? STATE_FLAG_RECOVERY : 0u) | (s->dematerializing ? STATE_FLAG_DEMATERIALIZING : 0u);
    state->committed_segment = s->committed_segment;
    state->committed_offset = s->committed_offset;
    state->committed_record_sequence = s->committed_record_sequence;
    state->first_live_segment = s->first_live_segment;
    state->last_data_segment = s->last_data_segment;
    state->active_segment = s->active_segment;
    state->recovery_first = s->recovery_first;
    state->recovery_last = s->recovery_last;
    state->next_segment = s->next_segment;
    state->known_queue_size = s->known_queue_size;
    state->bytes = s->stats.bytes;
    state->segments = (uint64_t)s->stats.segments;
    state->writer_segment = s->persisted_writer_segment;
    state->writer_end = s->persisted_writer_end;
    state->writer_sequence = s->persisted_writer_sequence;
    state->writer_count = s->persisted_writer_count;
    state->delete_first = s->delete_first;
    state->delete_last = s->delete_last;
    state->delete_bytes = s->delete_bytes;
    state->delete_segments = s->delete_segments;
}

static void apply_state_image(segdisk_store_t *s, const segdisk_state_image_t *state) {
    memcpy(s->uuid, state->uuid, sizeof(s->uuid));
    s->generation = state->generation;
    s->dematerializing = (state->flags & STATE_FLAG_DEMATERIALIZING) != 0;
    s->committed_segment = state->committed_segment;
    s->committed_offset = state->committed_offset;
    s->committed_record_sequence = state->committed_record_sequence;
    s->first_live_segment = state->first_live_segment;
    s->last_data_segment = state->last_data_segment;
    s->active_segment = state->active_segment;
    s->recovery_first = state->recovery_first;
    s->recovery_last = state->recovery_last;
    s->next_segment = state->next_segment;
    s->persisted_queue_size = state->known_queue_size;
    s->stats.bytes = state->bytes;
    s->stats.segments = state->segments > INT_MAX ? INT_MAX : (int)state->segments;
    s->persisted_writer_segment = state->writer_segment;
    s->persisted_writer_end = state->writer_end;
    s->persisted_writer_sequence = state->writer_sequence;
    s->persisted_writer_count = state->writer_count;
    s->delete_first = state->delete_first;
    s->delete_last = state->delete_last;
    s->delete_bytes = state->delete_bytes;
    s->delete_segments = state->delete_segments;
}

static rsRetVal write_state(segdisk_store_t *s, sbool force_sync, sbool topology) {
    unsigned char b[STATE_SLOT_LEN];
    const uint64_t next_generation = s->generation + 1;
    segdisk_state_image_t state;
    capture_state_image(s, &state);
    segdiskStateEncode(&state, next_generation, b);
    const int slot = (int)(next_generation & 1u);
    rsRetVal r = pwrite_full(s->state_fd, b, sizeof(b), slot * STATE_SLOT_LEN);
    if (r == RS_RET_OK && (force_sync || s->cfg.sync_files) && sync_file_data(s->state_fd) != 0) r = RS_RET_IO_ERROR;
    if (r == RS_RET_OK) {
        s->generation = next_generation;
        ++s->stats.checkpoints;
        ++s->stats.state_writes;
        if (force_sync || topology) ++s->stats.forced_state_writes;
        s->updates_since_checkpoint = 0;
    }
    return r;
}

static rsRetVal read_state(segdisk_store_t *s) {
    unsigned char slots[2][STATE_SLOT_LEN];
    const rsRetVal r0 = read_full_at(s->state_fd, slots[0], STATE_SLOT_LEN, 0);
    const rsRetVal r1 = read_full_at(s->state_fd, slots[1], STATE_SLOT_LEN, STATE_SLOT_LEN);
    segdisk_state_image_t state;
    const rsRetVal r = segdiskStateSelect(slots[0], r0 == RS_RET_OK, slots[1], r1 == RS_RET_OK, &state, NULL);
    if (r == RS_RET_OK &&
        ((state.flags & ~(STATE_FLAG_RECOVERY | STATE_FLAG_DEMATERIALIZING)) != 0 ||
         (state.delete_first == 0) != (state.delete_last == 0) ||
         (state.delete_first != 0 && state.delete_first > state.delete_last) || state.delete_bytes < 0))
        return RS_RET_INVALID_VALUE;
    if (r == RS_RET_OK) apply_state_image(s, &state);
    return r;
}

static int segment_name(const char *name) {
    unsigned long long id;
    char suffix[16];
    if (sscanf(name, "segment-%20llu.%15s", &id, suffix) != 2) return 0;
    return !strcmp(suffix, "seg") || !strcmp(suffix, "open") || !strcmp(suffix, "recover");
}

static rsRetVal remove_remaining_segments(segdisk_store_t *s) {
    const int scan_fd = openat(s->dir_fd, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (scan_fd < 0) return RS_RET_IO_ERROR;
    DIR *dir = fdopendir(scan_fd);
    if (dir == NULL) {
        close(scan_fd);
        return RS_RET_IO_ERROR;
    }
    rsRetVal r = RS_RET_OK;
    struct dirent *de;
    while (1) {
        errno = 0;
        de = readdir(dir);
        if (de == NULL) {
            if (errno != 0) r = RS_RET_IO_ERROR;
            break;
        }
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..") || !strcmp(de->d_name, "state")) continue;
        if (!segment_name(de->d_name)) {
            r = RS_RET_IO_ERROR;
            /* A foreign entry prevents rmdir(), but it must not prevent us
             * from removing the queue's own segments.  Continuing here is
             * important for cleanup-error recovery: the replacement empty
             * state may only exclude the old segment IDs after every old
             * segment has actually gone. */
            continue;
        }
        if (unlinkat(s->dir_fd, de->d_name, 0) != 0 && errno != ENOENT) {
            r = RS_RET_IO_ERROR;
            continue;
        }
    }
    if (closedir(dir) != 0) r = RS_RET_IO_ERROR;
    return r;
}

static rsRetVal directory_has_segments(segdisk_store_t *s, sbool *has_segments) {
    const int scan_fd = openat(s->dir_fd, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (scan_fd < 0) return RS_RET_IO_ERROR;
    DIR *dir = fdopendir(scan_fd);
    if (dir == NULL) {
        close(scan_fd);
        return RS_RET_IO_ERROR;
    }
    *has_segments = 0;
    struct dirent *de;
    errno = 0;
    while ((de = readdir(dir)) != NULL) {
        if (segment_name(de->d_name)) {
            *has_segments = 1;
            break;
        }
    }
    const int scan_errno = errno;
    if (closedir(dir) != 0) return RS_RET_IO_ERROR;
    return !*has_segments && scan_errno != 0 ? RS_RET_IO_ERROR : RS_RET_OK;
}

static rsRetVal write_segment_header(segdisk_store_t *s, int fd, uint64_t id) {
    unsigned char b[SEG_HDR_LEN] = {0};
    memcpy(b, SEG_MAGIC, 8);
    put16(b + 8, STORE_VERSION);
    put16(b + 10, SEGDISK_CODEC_VERSION);
    memcpy(b + 12, s->uuid, 16);
    put64(b + 28, id);
    put64(b + 36, 1);
    put32(b + 44, SEG_HDR_LEN);
    put32(b + 48, segdiskCrc32c(b, 48));
    return write_full(fd, b, sizeof(b));
}

static rsRetVal validate_segment_header(segdisk_store_t *s, int fd, segdisk_segment_t *seg) {
    unsigned char b[SEG_HDR_LEN];
    rsRetVal r = read_full_at(fd, b, sizeof(b), 0);
    if (r != RS_RET_OK || memcmp(b, SEG_MAGIC, 8) || get16(b + 8) != STORE_VERSION ||
        get16(b + 10) != SEGDISK_CODEC_VERSION || memcmp(b + 12, s->uuid, 16) || get64(b + 28) != seg->id ||
        get32(b + 44) != SEG_HDR_LEN || get32(b + 48) != segdiskCrc32c(b, 48))
        return RS_RET_INVALID_VALUE;
    seg->first_sequence = get64(b + 36);
    return RS_RET_OK;
}

static void free_segment(segdisk_segment_t **seg) {
    if (*seg == NULL) return;
    free((*seg)->path);
    free(*seg);
    *seg = NULL;
}

static rsRetVal probe_segment_path(segdisk_store_t *s, uint64_t id, const char *suffix, char **path) {
    *path = segment_path(s, id, suffix);
    if (*path == NULL) return RS_RET_OUT_OF_MEMORY;
    struct stat st;
    if (lstat(*path, &st) == 0) return S_ISREG(st.st_mode) ? RS_RET_OK : RS_RET_INVALID_VALUE;
    const int saved_errno = errno;
    free(*path);
    *path = NULL;
    errno = saved_errno;
    return saved_errno == ENOENT ? RS_RET_FILE_NOT_FOUND : RS_RET_IO_ERROR;
}

static rsRetVal load_segment(segdisk_store_t *s, uint64_t id, segdisk_segment_t **out) {
    segdisk_segment_t *seg = calloc(1, sizeof(*seg));
    if (seg == NULL) return RS_RET_OUT_OF_MEMORY;
    seg->id = id;
    rsRetVal r = probe_segment_path(s, id, "seg", &seg->path);
    if (r == RS_RET_OK) {
        seg->sealed = 1;
    } else if (r == RS_RET_FILE_NOT_FOUND) {
        r = probe_segment_path(s, id, "recover", &seg->path);
        if (r == RS_RET_OK) seg->recovery = 1;
    }
    if (r == RS_RET_FILE_NOT_FOUND) r = probe_segment_path(s, id, "open", &seg->path);
    if (r != RS_RET_OK) {
        free_segment(&seg);
        return r;
    }
    const int fd = open(seg->path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) {
        free_segment(&seg);
        return RS_RET_IO_ERROR;
    }
    struct stat st;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
        close(fd);
        free_segment(&seg);
        return RS_RET_IO_ERROR;
    }
    r = validate_segment_header(s, fd, seg);
    if (r == RS_RET_OK && seg->sealed) {
        if (st.st_size < (int64_t)(SEG_HDR_LEN + FOOT_LEN)) {
            r = RS_RET_INVALID_VALUE;
        } else {
            unsigned char foot[FOOT_LEN];
            r = read_full_at(fd, foot, sizeof(foot), st.st_size - FOOT_LEN);
            if (r != RS_RET_OK || memcmp(foot, FOOT_MAGIC, 8) || get64(foot + 8) != id ||
                get32(foot + 44) != segdiskCrc32c(foot, 44)) {
                r = RS_RET_INVALID_VALUE;
            } else {
                seg->first_sequence = get64(foot + 16);
                seg->last_sequence = get64(foot + 24);
                seg->record_count = get64(foot + 32);
                seg->rolling_crc = get32(foot + 40);
                seg->data_end = st.st_size - FOOT_LEN;
            }
        }
    } else if (r == RS_RET_OK) {
        seg->data_end = st.st_size;
    }
    close(fd);
    if (r != RS_RET_OK) {
        ++s->stats.corruption_events;
        LogError(0, r, "%s: segmentedDisk could not validate segment metadata '%s'", s->queue_name, seg->path);
        free_segment(&seg);
        return r;
    }
    seg->file_size = st.st_size;
    *out = seg;
    return RS_RET_OK;
}

static void capture_writer(segdisk_store_t *s) {
    if (s->active == NULL) return;
    s->persisted_writer_segment = s->active->id;
    s->persisted_writer_end = s->active->data_end;
    s->persisted_writer_sequence = s->active->last_sequence;
    s->persisted_writer_count = s->active->record_count;
}

static rsRetVal create_active(segdisk_store_t *s) {
    const uint64_t old_first_live = s->first_live_segment;
    const uint64_t old_next_segment = s->next_segment;
    segdisk_segment_t *seg = calloc(1, sizeof(*seg));
    if (seg == NULL) return RS_RET_OUT_OF_MEMORY;
    seg->id = s->next_segment++;
    seg->first_sequence = 1;
    seg->data_end = SEG_HDR_LEN;
    seg->file_size = SEG_HDR_LEN;
    seg->path = segment_path(s, seg->id, "open");
    if (seg->path == NULL) {
        s->next_segment = old_next_segment;
        free(seg);
        return RS_RET_OUT_OF_MEMORY;
    }
    s->active_segment = seg->id;
    if (s->first_live_segment == 0) s->first_live_segment = seg->id;
    ++s->stats.segments;
    s->stats.bytes += SEG_HDR_LEN;
    rsRetVal r = write_state(s, 1, 1);
    if (r != RS_RET_OK) {
        s->active_segment = 0;
        s->first_live_segment = old_first_live;
        s->next_segment = old_next_segment;
        --s->stats.segments;
        s->stats.bytes -= SEG_HDR_LEN;
        free_segment(&seg);
        return r;
    }
    test_fault(s, SEGDISK_TEST_FAULT_RESERVATION_PUBLISHED);
    const int fd = open(seg->path, O_RDWR | O_CREAT | O_EXCL | O_APPEND | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (fd < 0) {
        s->active_segment = 0;
        /* The published reservation keeps next_segment monotonic, but an
         * absent file cannot be the live frontier for an in-process retry. */
        s->first_live_segment = old_first_live;
        --s->stats.segments;
        s->stats.bytes -= SEG_HDR_LEN;
        free_segment(&seg);
        return RS_RET_IO_ERROR;
    }
    r = write_segment_header(s, fd, seg->id);
    if (r == RS_RET_OK && s->cfg.sync_files && sync_file_data(fd) != 0) r = RS_RET_IO_ERROR;
    if (r != RS_RET_OK) {
        close(fd);
        unlink(seg->path);
        s->active_segment = 0;
        s->first_live_segment = old_first_live;
        --s->stats.segments;
        s->stats.bytes -= SEG_HDR_LEN;
        free_segment(&seg);
        return r;
    }
    test_fault(s, SEGDISK_TEST_FAULT_SEGMENT_CREATED);
    s->active = seg;
    s->active_fd = fd;
    return RS_RET_OK;
}

static rsRetVal materialize_empty(segdisk_store_t *s) {
    if (s->dir_fd >= 0) return RS_RET_OK;
    rsRetVal r = RS_RET_OK;
    if (mkdir(s->dir, 0700) != 0 && errno != EEXIST) return RS_RET_IO_ERROR;
    s->dir_fd = open(s->dir, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (s->dir_fd < 0) return RS_RET_IO_ERROR;
    s->state_fd = openat(s->dir_fd, "state", O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (s->state_fd < 0 || ftruncate(s->state_fd, STATE_FILE_LEN) != 0 || random_uuid(s->uuid) != RS_RET_OK)
        r = RS_RET_IO_ERROR;
    s->committed_offset = SEG_HDR_LEN;
    s->persisted_writer_end = SEG_HDR_LEN;
    s->next_segment = 1;
    if (r == RS_RET_OK) r = create_active(s);
    if (r == RS_RET_OK) r = sync_dir(s);
    if (r == RS_RET_OK) {
        ++s->stats.materializations;
        return RS_RET_OK;
    }
    if (s->active_fd >= 0) close(s->active_fd);
    s->active_fd = -1;
    free_segment(&s->active);
    if (s->dir_fd >= 0) {
        remove_remaining_segments(s);
        unlinkat(s->dir_fd, "state", 0);
    }
    if (s->state_fd >= 0) close(s->state_fd);
    if (s->dir_fd >= 0) close(s->dir_fd);
    s->state_fd = s->dir_fd = -1;
    rmdir(s->dir);
    return r;
}

static void restore_active_after_seal_failure(segdisk_store_t *s) {
    const int fd = open(s->active->path, O_RDWR | O_APPEND | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return;
    if (ftruncate(fd, s->active->data_end) != 0) {
        close(fd);
        return;
    }
    s->active_fd = fd;
}

static rsRetVal seal_active(segdisk_store_t *s) {
    if (s->active == NULL || s->active->sealed || s->active->record_count == 0) return RS_RET_OK;
    char *sealed = segment_path(s, s->active->id, "seg");
    if (sealed == NULL) return RS_RET_OUT_OF_MEMORY;
    char *read_path = NULL;
    if (s->read_segment != NULL && s->read_segment->id == s->active->id) {
        read_path = strdup(sealed);
        if (read_path == NULL) {
            free(sealed);
            return RS_RET_OUT_OF_MEMORY;
        }
    }
    unsigned char b[FOOT_LEN] = {0};
    memcpy(b, FOOT_MAGIC, 8);
    put64(b + 8, s->active->id);
    put64(b + 16, s->active->first_sequence);
    put64(b + 24, s->active->last_sequence);
    put64(b + 32, s->active->record_count);
    put32(b + 40, s->active->rolling_crc);
    put32(b + 44, segdiskCrc32c(b, 44));
    rsRetVal r = write_full(s->active_fd, b, sizeof(b));
    if (r == RS_RET_OK && s->cfg.sync_files && sync_file_data(s->active_fd) != 0) r = RS_RET_IO_ERROR;
    if (close(s->active_fd) != 0 && r == RS_RET_OK) r = RS_RET_IO_ERROR;
    s->active_fd = -1;
    if (r != RS_RET_OK) {
        restore_active_after_seal_failure(s);
        free(read_path);
        free(sealed);
        return r;
    }
    test_fault(s, SEGDISK_TEST_FAULT_SEAL_WRITTEN);
    if (rename(s->active->path, sealed) != 0) {
        restore_active_after_seal_failure(s);
        free(read_path);
        free(sealed);
        return RS_RET_IO_ERROR;
    }
    test_fault(s, SEGDISK_TEST_FAULT_SEAL_RENAMED);
    free(s->active->path);
    s->active->path = sealed;
    s->active->sealed = 1;
    s->active->file_size += FOOT_LEN;
    s->active->data_end = s->active->file_size - FOOT_LEN;
    s->stats.bytes += FOOT_LEN;
    s->last_data_segment = s->active->id;
    if (s->read_segment != NULL && s->read_segment->id == s->active->id) {
        free(s->read_segment->path);
        s->read_segment->path = read_path;
        s->read_segment->sealed = 1;
        s->read_segment->data_end = s->active->data_end;
        s->read_segment->file_size = s->active->file_size;
    }
    capture_writer(s);
    s->active_segment = 0;
    free_segment(&s->active);
    return s->cfg.sync_files ? sync_dir(s) : RS_RET_OK;
}

static rsRetVal make_record(smsg_t *msg, uint64_t sequence, unsigned char **out, size_t *outlen) {
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
    put32(record + 12, (uint32_t)payload_len);
    put64(record + 16, sequence);
    put32(record + 24, segdiskCrc32c(record, 24));
    put32(record + 28, segdiskCrc32c(payload, payload_len));
    memcpy(record + REC_HDR_LEN, payload, payload_len);
    free(payload);
    *out = record;
    *outlen = len;
    return RS_RET_OK;
}

static void update_retry_overage(segdisk_store_t *s) {
    s->stats.retry_overage_bytes = s->cfg.max_disk_space > 0 && s->stats.bytes > s->cfg.max_disk_space
                                       ? s->stats.bytes - s->cfg.max_disk_space
                                       : 0;
    if (s->stats.retry_overage_bytes > s->stats.retry_overage_max_bytes)
        s->stats.retry_overage_max_bytes = s->stats.retry_overage_bytes;
}

static rsRetVal append_record(segdisk_store_t *s, smsg_t *msg, sbool internal, int64_t *written) {
    if (s->dir_fd < 0) {
        const rsRetVal materialize_ret = materialize_empty(s);
        if (materialize_ret != RS_RET_OK) return materialize_ret;
    }
    if (s->active == NULL) {
        rsRetVal r = create_active(s);
        if (r != RS_RET_OK) return r;
    }
    unsigned char *record = NULL;
    size_t len = 0;
    rsRetVal r = make_record(msg, s->active->record_count + 1, &record, &len);
    if (r != RS_RET_OK) return r;
    const sbool rotate = s->active->record_count != 0 && s->cfg.max_file_size > 0 &&
                         s->active->file_size + (int64_t)len + FOOT_LEN > s->cfg.max_file_size;
    if (rotate) {
        r = seal_active(s);
        if (r == RS_RET_OK) r = create_active(s);
        if (r != RS_RET_OK) {
            free(record);
            return r;
        }
        free(record);
        record = NULL;
        r = make_record(msg, 1, &record, &len);
        if (r != RS_RET_OK) return r;
    }
    r = write_full(s->active_fd, record, len);
    if (r == RS_RET_OK && s->cfg.sync_files && sync_file_data(s->active_fd) != 0) r = RS_RET_IO_ERROR;
    if (r == RS_RET_OK) {
        s->active->data_end += len;
        s->active->file_size += len;
        s->active->last_sequence = ++s->active->record_count;
        s->active->rolling_crc ^= segdiskCrc32c(record, REC_HDR_LEN);
        s->active->rolling_crc ^= segdiskCrc32c(record + REC_HDR_LEN, len - REC_HDR_LEN);
        s->last_data_segment = s->active->id;
        ++s->known_queue_size;
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

static sbool segment_is_undiscovered(const segdisk_store_t *s, uint64_t id) {
    return id != 0 && id <= s->discovery_through_segment;
}

static rsRetVal record_at(segdisk_store_t *s,
                          segdisk_segment_t *seg,
                          int64_t *off,
                          smsg_t **msg,
                          uint64_t *sequence,
                          int *skipped,
                          int *discovered,
                          size_t *scan_budget) {
    if (seg->id == s->active_segment && s->active != NULL) seg->data_end = s->active->data_end;
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
            ++s->stats.recovery_bytes;
            ++*off;
            if (*scan_budget == 0) {
                close(fd);
                return RS_RET_RETRY;
            }
            --*scan_budget;
            continue;
        }
        const uint32_t n = get32(h + 12);
        unsigned char *payload = malloc(n == 0 ? 1 : n);
        if (payload == NULL) {
            close(fd);
            return RS_RET_OUT_OF_MEMORY;
        }
        r = read_full_at(fd, payload, n, *off + REC_HDR_LEN);
        *sequence = get64(h + 16);
        const sbool newly_discovered = segment_is_undiscovered(s, seg->id);
        if (newly_discovered) {
            ++*discovered;
            ++s->known_queue_size;
            ++s->stats.recovery_records;
            s->stats.recovery_bytes += REC_HDR_LEN + n;
        }
        *off += REC_HDR_LEN + n;
        if (r != RS_RET_OK || segdiskCrc32c(payload, n) != get32(h + 28) ||
            segdiskCodecDecode(payload, n, msg) != RS_RET_OK) {
            free(payload);
            ++s->stats.corruption_events;
            ++s->stats.corruption_records;
            ++*skipped;
            continue;
        }
        free(payload);
        close(fd);
        return RS_RET_OK;
    }
    if (*off < seg->data_end && seg->id != s->active_segment) {
        const size_t tail = (size_t)(seg->data_end - *off);
        s->stats.corruption_bytes += tail;
        s->stats.recovery_bytes += tail;
        *off = seg->data_end;
    }
    close(fd);
    return RS_RET_NO_DATA;
}

static void append_pending(segdisk_store_t *s, segdisk_batch_ctx_t *ctx) {
    if (s->pending_tail == NULL)
        s->pending_head = s->pending_tail = ctx;
    else {
        s->pending_tail->next = ctx;
        s->pending_tail = ctx;
    }
}

static rsRetVal unlink_segment_id(segdisk_store_t *s, uint64_t id, sbool account) {
    const char *const suffixes[] = {"seg", "recover", "open"};
    rsRetVal r = RS_RET_OK;
    for (size_t i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); ++i) {
        char *path = segment_path(s, id, suffixes[i]);
        if (path == NULL) return RS_RET_OUT_OF_MEMORY;
        struct stat st;
        if (lstat(path, &st) == 0) {
            if (unlink(path) != 0 && errno != ENOENT)
                r = RS_RET_IO_ERROR;
            else {
                test_fault(s, SEGDISK_TEST_FAULT_SEGMENT_UNLINKED);
                if (account) {
                    s->stats.bytes -= st.st_size;
                    if (s->stats.segments > 0) --s->stats.segments;
                }
            }
        } else if (errno != ENOENT) {
            r = RS_RET_IO_ERROR;
        }
        free(path);
        if (r != RS_RET_OK) break;
    }
    return r;
}

static rsRetVal cleanup_one_pending_delete(segdisk_store_t *s) {
    if (s->delete_first == 0) return RS_RET_OK;
    if (unlink_segment_id(s, s->delete_first, 0) != RS_RET_OK || sync_dir(s) != RS_RET_OK) return RS_RET_IO_ERROR;

    const uint64_t old_delete_first = s->delete_first;
    const uint64_t old_delete_last = s->delete_last;
    const int64_t old_delete_bytes = s->delete_bytes;
    const uint64_t old_delete_segments = s->delete_segments;
    const int64_t old_bytes = s->stats.bytes;
    const int old_segments = s->stats.segments;
    if (s->delete_first == s->delete_last) {
        s->delete_first = s->delete_last = 0;
        s->delete_bytes = 0;
        s->delete_segments = 0;
        s->stats.bytes = old_delete_bytes > s->stats.bytes ? 0 : s->stats.bytes - old_delete_bytes;
        s->stats.segments =
            old_delete_segments > (uint64_t)s->stats.segments ? 0 : s->stats.segments - (int)old_delete_segments;
    } else {
        ++s->delete_first;
    }
    const rsRetVal r = write_state(s, 1, 1);
    if (r != RS_RET_OK) {
        s->delete_first = old_delete_first;
        s->delete_last = old_delete_last;
        s->delete_bytes = old_delete_bytes;
        s->delete_segments = old_delete_segments;
        s->stats.bytes = old_bytes;
        s->stats.segments = old_segments;
    }
    return r;
}

static rsRetVal delete_committed(segdisk_store_t *s) {
    const uint64_t target = s->committed_segment;
    if (target != 0 && s->first_live_segment < target) {
        const uint64_t old_first = s->first_live_segment;
        const uint64_t old_recovery_first = s->recovery_first;
        const uint64_t old_recovery_last = s->recovery_last;
        const uint64_t old_delete_first = s->delete_first;
        const uint64_t old_delete_last = s->delete_last;
        const int64_t old_delete_bytes = s->delete_bytes;
        const uint64_t old_delete_segments = s->delete_segments;
        int64_t bytes = 0;
        uint64_t segments = 0;
        for (uint64_t id = old_first; id < target; ++id) {
            const char *const suffixes[] = {"seg", "recover", "open"};
            for (size_t i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); ++i) {
                char *path = segment_path(s, id, suffixes[i]);
                if (path == NULL) return RS_RET_OUT_OF_MEMORY;
                struct stat st;
                if (lstat(path, &st) == 0) {
                    if (st.st_size < 0 || st.st_size > INT64_MAX - bytes || segments == UINT64_MAX) {
                        free(path);
                        return RS_RET_FILE_TOO_LARGE;
                    }
                    bytes += st.st_size;
                    ++segments;
                }
                free(path);
            }
        }
        if (bytes > INT64_MAX - s->delete_bytes || segments > UINT64_MAX - s->delete_segments)
            return RS_RET_FILE_TOO_LARGE;
        if (s->delete_first == 0) s->delete_first = old_first;
        s->delete_last = target - 1;
        s->delete_bytes += bytes;
        s->delete_segments += segments;
        s->first_live_segment = target;
        if (s->recovery_first != 0 && target > s->recovery_first) {
            if (target > s->recovery_last)
                s->recovery_first = s->recovery_last = 0;
            else
                s->recovery_first = target;
        }
        const rsRetVal r = write_state(s, 1, 1);
        if (r != RS_RET_OK) {
            s->first_live_segment = old_first;
            s->recovery_first = old_recovery_first;
            s->recovery_last = old_recovery_last;
            s->delete_first = old_delete_first;
            s->delete_last = old_delete_last;
            s->delete_bytes = old_delete_bytes;
            s->delete_segments = old_delete_segments;
            return r;
        }
        test_fault(s, SEGDISK_TEST_FAULT_PREDELETE_PUBLISHED);
    }

    while (s->delete_first != 0 && cleanup_one_pending_delete(s) == RS_RET_OK) {
    }
    if (s->delete_first != 0)
        LogError(0, RS_RET_IO_ERROR,
                 "%s: segmentedDisk could not remove all committed segment files; "
                 "the durable pending-delete range will be retried",
                 s->queue_name);
    update_retry_overage(s);
    return RS_RET_OK;
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
    const uint64_t old_sequence = s->committed_record_sequence;
    const uint64_t old_size = s->known_queue_size;
    const unsigned int old_updates = s->updates_since_checkpoint;
    s->committed_segment = last->end_segment;
    s->committed_offset = last->end_offset;
    s->committed_record_sequence = last->end_record_sequence;
    s->known_queue_size = advanced > s->known_queue_size ? 0 : s->known_queue_size - advanced;
    s->updates_since_checkpoint += advanced;
    rsRetVal r = RS_RET_OK;
    if (s->cfg.checkpoint_interval != 0 && s->updates_since_checkpoint >= s->cfg.checkpoint_interval) {
        capture_writer(s);
        r = write_state(s, 0, 0);
        if (r == RS_RET_OK) {
            test_fault(s, SEGDISK_TEST_FAULT_CHECKPOINT_PUBLISHED);
            test_fault(s, SEGDISK_TEST_FAULT_COMMIT_PUBLISHED);
        }
    }
    if (r == RS_RET_OK) r = delete_committed(s);
    if (r != RS_RET_OK) {
        s->committed_segment = old_segment;
        s->committed_offset = old_offset;
        s->committed_record_sequence = old_sequence;
        s->known_queue_size = old_size;
        s->updates_since_checkpoint = old_updates;
        return r;
    }
    segdisk_batch_ctx_t *const after = last->next;
    while (s->pending_head != after) {
        segdisk_batch_ctx_t *ctx = s->pending_head;
        s->pending_head = ctx->next;
        ctx->pending = 0;
        release_batch_ctx(ctx);
    }
    if (s->pending_head == NULL) s->pending_tail = NULL;
    return RS_RET_OK;
}

static rsRetVal prepare_existing_active(segdisk_store_t *s) {
    if (s->active_segment == 0) return RS_RET_OK;
    const int64_t accounted_bytes =
        s->persisted_writer_segment == s->active_segment && s->persisted_writer_end >= SEG_HDR_LEN
            ? s->persisted_writer_end
            : SEG_HDR_LEN;
    int64_t actual_bytes = -1;
    char *open_path = NULL;
    ++s->stats.startup_segment_files_probed;
    rsRetVal r = probe_segment_path(s, s->active_segment, "open", &open_path);
    if (r == RS_RET_OK) {
        struct stat st;
        if (lstat(open_path, &st) != 0 || !S_ISREG(st.st_mode)) {
            free(open_path);
            return RS_RET_IO_ERROR;
        }
        actual_bytes = st.st_size;
        char *recover_path = segment_path(s, s->active_segment, "recover");
        if (recover_path == NULL) {
            free(open_path);
            return RS_RET_OUT_OF_MEMORY;
        }
        if (rename(open_path, recover_path) != 0) r = RS_RET_IO_ERROR;
        free(recover_path);
        if (r == RS_RET_OK) {
            if (s->recovery_first == 0) s->recovery_first = s->active_segment;
            s->recovery_last = s->active_segment;
            if (s->last_data_segment < s->active_segment) s->last_data_segment = s->active_segment;
        }
        free(open_path);
    } else if (r == RS_RET_FILE_NOT_FOUND) {
        char *recover = NULL;
        ++s->stats.startup_segment_files_probed;
        r = probe_segment_path(s, s->active_segment, "recover", &recover);
        if (r == RS_RET_OK) {
            struct stat st;
            if (lstat(recover, &st) != 0 || !S_ISREG(st.st_mode)) {
                free(recover);
                return RS_RET_IO_ERROR;
            }
            actual_bytes = st.st_size;
        }
        free(recover);
        if (r == RS_RET_OK) {
            if (s->recovery_first == 0) s->recovery_first = s->active_segment;
            s->recovery_last = s->active_segment;
            if (s->last_data_segment < s->active_segment) s->last_data_segment = s->active_segment;
        }
    }
    if (r == RS_RET_FILE_NOT_FOUND) {
        char *sealed = NULL;
        ++s->stats.startup_segment_files_probed;
        r = probe_segment_path(s, s->active_segment, "seg", &sealed);
        if (r == RS_RET_OK) {
            struct stat st;
            if (lstat(sealed, &st) != 0 || !S_ISREG(st.st_mode)) {
                free(sealed);
                return RS_RET_IO_ERROR;
            }
            actual_bytes = st.st_size;
        }
        free(sealed);
        if (r == RS_RET_OK && s->last_data_segment < s->active_segment) s->last_data_segment = s->active_segment;
    }
    if (r == RS_RET_FILE_NOT_FOUND) {
        if (s->stats.segments > 0) --s->stats.segments;
        if (s->stats.bytes >= accounted_bytes) s->stats.bytes -= accounted_bytes;
        /* Reservation publication can survive a crash before file creation.
         * The ID remains consumed, but the nonexistent segment is not live. */
        if (s->first_live_segment == s->active_segment) s->first_live_segment = 0;
        r = RS_RET_OK;
    } else if (r == RS_RET_OK && actual_bytes >= 0) {
        s->stats.bytes += actual_bytes - accounted_bytes;
    }
    s->active_segment = 0;
    return r;
}

/* A valid older state slot may be selected after the newer reservation slot
 * was torn. In that case only the older slot's next ID can already exist. It
 * is adopted as recovery data without enumerating the spool directory. */
static rsRetVal adopt_reserved_segment(segdisk_store_t *s) {
    const uint64_t id = s->next_segment;
    const char *found_suffix = NULL;
    char *path = NULL;
    const char *const suffixes[] = {"open", "recover", "seg"};
    for (size_t i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); ++i) {
        ++s->stats.startup_segment_files_probed;
        rsRetVal r = probe_segment_path(s, id, suffixes[i], &path);
        if (r == RS_RET_OK) {
            found_suffix = suffixes[i];
            break;
        }
        if (r != RS_RET_FILE_NOT_FOUND) return r;
    }
    if (found_suffix == NULL) return RS_RET_OK;
    struct stat st;
    if (lstat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
        free(path);
        return RS_RET_IO_ERROR;
    }
    if (!strcmp(found_suffix, "open")) {
        char *recover = segment_path(s, id, "recover");
        if (recover == NULL) {
            free(path);
            return RS_RET_OUT_OF_MEMORY;
        }
        if (rename(path, recover) != 0) {
            free(recover);
            free(path);
            return RS_RET_IO_ERROR;
        }
        free(recover);
    }
    free(path);
    /* If the missing published active ID was the old frontier, this adopted
     * next-ID segment is now the earliest live recovery data. */
    if (s->first_live_segment == 0) s->first_live_segment = id;
    if (s->recovery_first == 0) s->recovery_first = id;
    s->recovery_last = id;
    if (s->last_data_segment < id) s->last_data_segment = id;
    ++s->stats.segments;
    s->stats.bytes += st.st_size;
    ++s->next_segment;
    return RS_RET_OK;
}

static void reset_after_dematerialize(segdisk_store_t *s);

static rsRetVal finish_interrupted_dematerialization(segdisk_store_t *s) {
    rsRetVal r = remove_remaining_segments(s);
    if (r == RS_RET_OK) r = sync_dir(s);
    if (r == RS_RET_OK && unlinkat(s->dir_fd, "state", 0) != 0 && errno != ENOENT) r = RS_RET_IO_ERROR;
    if (r == RS_RET_OK) r = sync_dir(s);
    if (r == RS_RET_OK && rmdir(s->dir) != 0 && errno != ENOENT) r = RS_RET_IO_ERROR;
    if (r != RS_RET_OK) return r;
    if (s->state_fd >= 0) close(s->state_fd);
    if (s->dir_fd >= 0) close(s->dir_fd);
    s->state_fd = s->dir_fd = -1;
    reset_after_dematerialize(s);
    ++s->stats.dematerializations;
    return RS_RET_OK;
}

rsRetVal segdiskStoreOpen(segdisk_store_t **out, const segdisk_store_config_t *cfg, int *queue_size) {
    if (out == NULL || cfg == NULL || cfg->work_dir == NULL || cfg->file_prefix == NULL || queue_size == NULL)
        return RS_RET_PARAM_ERROR;
    if (cfg->file_prefix[0] == '\0' || strchr(cfg->file_prefix, '/') != NULL ||
        strchr(cfg->file_prefix, '\\') != NULL || !strcmp(cfg->file_prefix, ".") || !strcmp(cfg->file_prefix, ".."))
        return RS_RET_INVALID_VALUE;
    segdisk_store_t *s = calloc(1, sizeof(*s));
    if (s == NULL) return RS_RET_OUT_OF_MEMORY;
    rsRetVal fail_ret = RS_RET_IO_ERROR;
    s->dir_fd = s->state_fd = s->active_fd = -1;
    s->cfg = *cfg;
    s->committed_offset = SEG_HDR_LEN;
    s->persisted_writer_end = SEG_HDR_LEN;
    s->next_segment = 1;
    s->queue_name = strdup(cfg->queue_name == NULL ? cfg->file_prefix : cfg->queue_name);
    if (s->queue_name == NULL || asprintf(&s->dir, "%s/%s.segq", cfg->work_dir, cfg->file_prefix) < 0) goto oom;
    char *legacy_qi = NULL;
    if (asprintf(&legacy_qi, "%s/%s.qi", cfg->work_dir, cfg->file_prefix) < 0) goto oom;
    struct stat legacy_st;
    if (lstat(legacy_qi, &legacy_st) == 0) {
        /* DA engine resolution happens before this constructor.  Auto mode
         * sends any existing .qi to the classic child, and an explicit
         * segmented selector is rejected as a data conflict.  Keep this
         * store-level guard for pure segmentedDisk queues and races: a .qi is
         * persistent classic state even if it describes zero records, so it
         * must be drained/removed before auto-upgrade can select segmented. */
        LogError(0, RS_RET_INVALID_VALUE, "%s: segmentedDisk refuses legacy queue state '%s'", s->queue_name,
                 legacy_qi);
        free(legacy_qi);
        goto invalid;
    }
    free(legacy_qi);
    if (cfg->lazy_create) {
        struct stat dir_st;
        if (lstat(s->dir, &dir_st) != 0 && errno == ENOENT) {
            *queue_size = 0;
            *out = s;
            return RS_RET_OK;
        }
    }
    sbool directory_created = 0;
    if (mkdir(s->dir, 0700) == 0)
        directory_created = 1;
    else if (errno != EEXIST)
        goto ioerr;
    s->dir_fd = open(s->dir, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (s->dir_fd < 0) goto ioerr;
    s->state_fd = openat(s->dir_fd, "state", O_RDWR | O_CLOEXEC | O_NOFOLLOW);
    if (s->state_fd < 0 && errno != ENOENT) goto ioerr;
    if (s->state_fd < 0) {
        const sbool has_v1_state =
            faccessat(s->dir_fd, "meta", F_OK, 0) == 0 || faccessat(s->dir_fd, "checkpoint", F_OK, 0) == 0;
        if (has_v1_state) {
            LogError(0, RS_RET_INVALID_VALUE,
                     "%s: experimental segmentedDisk v1 state is incompatible with store format v2; "
                     "offline recovery is required",
                     s->queue_name);
            goto invalid;
        }
        sbool has_entries = 0;
        if (!directory_created && directory_has_entries(s->dir_fd, &has_entries) != RS_RET_OK) goto ioerr;
        if (has_entries) {
            LogError(0, RS_RET_INVALID_VALUE,
                     "%s: segmentedDisk state is missing while queue files exist; offline recovery is required",
                     s->queue_name);
            goto invalid;
        }
        if (cfg->lazy_create && !directory_created) {
            /* A crash after durable state removal but before rmdir leaves an
             * empty directory. For a DA child this is the final, unambiguous
             * cleanup residue, so finish dematerialization instead of
             * creating a fresh empty store during startup. */
            close(s->dir_fd);
            s->dir_fd = -1;
            if (rmdir(s->dir) != 0 && errno != ENOENT) goto ioerr;
            *queue_size = 0;
            *out = s;
            return RS_RET_OK;
        }
        s->state_fd = openat(s->dir_fd, "state", O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
        if (s->state_fd < 0 || ftruncate(s->state_fd, STATE_FILE_LEN) != 0 || random_uuid(s->uuid) != RS_RET_OK)
            goto ioerr;
        if (create_active(s) != RS_RET_OK) goto ioerr;
        if (sync_dir(s) != RS_RET_OK) goto ioerr;
    } else {
        struct stat state_st;
        if (fstat(s->state_fd, &state_st) != 0 || state_st.st_size != STATE_FILE_LEN) goto invalid;
        if (read_state(s) != RS_RET_OK) {
            LogError(0, RS_RET_INVALID_VALUE, "%s: segmentedDisk has no valid state slot; offline recovery is required",
                     s->queue_name);
            goto invalid;
        }
        if (s->dematerializing) {
            if (finish_interrupted_dematerialization(s) != RS_RET_OK) goto ioerr;
            if (cfg->lazy_create) {
                *queue_size = 0;
                *out = s;
                return RS_RET_OK;
            }
            if (materialize_empty(s) != RS_RET_OK) goto ioerr;
            *queue_size = 0;
            *out = s;
            return RS_RET_OK;
        }
        if (prepare_existing_active(s) != RS_RET_OK) goto ioerr;
        if (adopt_reserved_segment(s) != RS_RET_OK) goto ioerr;
        s->discovery_through_segment = s->last_data_segment;
        if (create_active(s) != RS_RET_OK) goto ioerr;
    }
    ++s->stats.materializations;
    s->known_queue_size = 0;
    *queue_size = 0;
    s->stats.replayed = s->persisted_queue_size;
    s->read_segment_id = s->committed_segment != 0 ? s->committed_segment : s->first_live_segment;
    if (s->read_segment_id == 0) s->read_segment_id = s->first_live_segment;
    s->read_offset = s->committed_segment == s->read_segment_id && s->committed_offset >= SEG_HDR_LEN
                         ? s->committed_offset
                         : SEG_HDR_LEN;
    *out = s;
    return RS_RET_OK;
oom:
    fail_ret = RS_RET_OUT_OF_MEMORY;
    goto fail;
ioerr:
    goto fail;
invalid:
    fail_ret = RS_RET_INVALID_VALUE;
fail:
    if (s->active_fd >= 0) close(s->active_fd);
    if (s->state_fd >= 0) close(s->state_fd);
    if (s->dir_fd >= 0) close(s->dir_fd);
    free_segment(&s->active);
    free_segment(&s->read_segment);
    free(s->dir);
    free(s->queue_name);
    free(s);
    return fail_ret;
}

rsRetVal segdiskStoreAppend(segdisk_store_t *s, smsg_t *msg, sbool internal, int64_t *written) {
    return append_record(s, msg, internal, written);
}

sbool segdiskStoreMayHaveData(const segdisk_store_t *s) {
    if (s == NULL) return 0;
    if (s->delete_first != 0) return 1;
    if (s->read_segment != NULL && s->read_offset < s->read_segment->data_end) return 1;
    if (s->read_segment == NULL && s->read_segment_id != 0 && s->read_segment_id <= s->last_data_segment) return 1;
    return s->active != NULL && s->active->record_count != 0 &&
           (s->read_segment_id < s->active->id || s->read_offset < s->active->data_end);
}

sbool segdiskStoreCanDematerialize(const segdisk_store_t *s) {
    /* recovery_first is durable topology, not an undiscovered-work flag: it
     * may still name the final, fully inspected recovery segment until the
     * commit frontier enters a later segment.  segdiskStoreMayHaveData() uses
     * the live read cursor and is the authoritative recovery-work check. A
     * durable cleanup already in progress is also eligible: failed repair may
     * retain stale cursors, but cleanup intent proves the store was empty. */
    return s != NULL && s->dir_fd >= 0 && s->known_queue_size == 0 && s->pending_head == NULL && s->delete_first == 0 &&
           (s->dematerializing || !segdiskStoreMayHaveData(s));
}

rsRetVal segdiskStoreDequeueBatch(segdisk_store_t *s, batch_t *batch, int max, int *skipped, int *discovered) {
    if (s->dir_fd < 0) return RS_RET_NO_DATA;
    if (batch->storeData != NULL) return RS_RET_INTERNAL_ERROR;
    if (s->delete_first != 0) {
        if (cleanup_one_pending_delete(s) != RS_RET_OK)
            LogError(0, RS_RET_IO_ERROR, "%s: segmentedDisk pending segment deletion failed and will be retried",
                     s->queue_name);
        if (s->delete_first != 0) return RS_RET_RETRY;
    }
    segdisk_batch_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) return RS_RET_OUT_OF_MEMORY;
    ctx->sequence = s->next_batch_sequence++;
    ctx->refs = 1;
    ctx->pending = 1;
    int n = 0;
    int consumed = 0;
    sbool recovery_pending = 0;
    *skipped = 0;
    *discovered = 0;
    size_t scan_budget = RECOVERY_SCAN_BUDGET;
    while (n < max) {
        if (s->read_segment == NULL) {
            if (s->read_segment_id == 0) s->read_segment_id = s->first_live_segment;
            while (s->read_segment_id != 0 && s->read_segment_id <= s->last_data_segment) {
                rsRetVal r = load_segment(s, s->read_segment_id, &s->read_segment);
                if (r == RS_RET_FILE_NOT_FOUND) {
                    ++s->stats.corruption_events;
                    ++s->stats.corruption_segments;
                    LogError(0, RS_RET_FILE_NOT_FOUND,
                             "%s: segmentedDisk expected segment %" PRIu64
                             " is missing during lazy dequeue; continuing with the next segment",
                             s->queue_name, s->read_segment_id);
                    ++s->read_segment_id;
                    s->read_offset = SEG_HDR_LEN;
                    continue;
                }
                if (r != RS_RET_OK) {
                    free(ctx);
                    return r;
                }
                if (s->committed_segment == s->read_segment_id && s->committed_offset >= SEG_HDR_LEN &&
                    s->committed_offset <= s->read_segment->data_end)
                    s->read_offset = s->committed_offset;
                else if (s->read_offset < SEG_HDR_LEN)
                    s->read_offset = SEG_HDR_LEN;
                break;
            }
            if (s->read_segment == NULL) break;
        }
        smsg_t *msg = NULL;
        uint64_t sequence = 0;
        const int skipped_before = *skipped;
        rsRetVal r = record_at(s, s->read_segment, &s->read_offset, &msg, &sequence, skipped, discovered, &scan_budget);
        consumed += *skipped - skipped_before;
        if (r == RS_RET_RETRY) {
            ++s->stats.recovery_pending;
            recovery_pending = 1;
            break;
        }
        if (r == RS_RET_NO_DATA) {
            if (s->read_segment->id == s->active_segment) break;
            ++s->read_segment_id;
            s->read_offset = SEG_HDR_LEN;
            free_segment(&s->read_segment);
            continue;
        }
        if (r != RS_RET_OK) {
            free(ctx);
            return r;
        }
        ++consumed;
        ctx->end_segment = s->read_segment->id;
        ctx->end_offset = s->read_offset;
        ctx->end_record_sequence = sequence;
        batch->pElem[n].pMsg = msg;
        batch->eltState[n] = BATCH_STATE_RDY;
        ++n;
    }
    if (consumed == 0) {
        free(ctx);
        return recovery_pending ? RS_RET_RETRY : RS_RET_NO_DATA;
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
    if (s->dir_fd < 0) return RS_RET_OK;
    capture_writer(s);
    const rsRetVal r = write_state(s, force_sync, force_sync);
    if (r == RS_RET_OK) test_fault(s, SEGDISK_TEST_FAULT_CHECKPOINT_PUBLISHED);
    return r;
}

static void reset_after_dematerialize(segdisk_store_t *s) {
    memset(s->uuid, 0, sizeof(s->uuid));
    s->generation = 0;
    s->committed_segment = 0;
    s->committed_record_sequence = 0;
    s->committed_offset = SEG_HDR_LEN;
    s->first_live_segment = 0;
    s->last_data_segment = 0;
    s->active_segment = 0;
    s->recovery_first = 0;
    s->recovery_last = 0;
    s->next_segment = 1;
    s->persisted_queue_size = 0;
    s->persisted_writer_segment = 0;
    s->persisted_writer_end = SEG_HDR_LEN;
    s->persisted_writer_sequence = 0;
    s->persisted_writer_count = 0;
    s->delete_first = 0;
    s->delete_last = 0;
    s->delete_bytes = 0;
    s->delete_segments = 0;
    s->discovery_through_segment = 0;
    s->read_segment_id = 0;
    s->read_offset = SEG_HDR_LEN;
    s->next_batch_sequence = 0;
    s->known_queue_size = 0;
    s->updates_since_checkpoint = 0;
    s->dematerializing = 0;
    s->stats.bytes = 0;
    s->stats.segments = 0;
    s->stats.retry_overage_bytes = 0;
}

static rsRetVal restore_empty_materialized(segdisk_store_t *s, uint64_t next_segment) {
    unsigned char old_uuid[sizeof(s->uuid)];
    memcpy(old_uuid, s->uuid, sizeof(old_uuid));
    const uint64_t old_generation = s->generation;
    if (s->dir_fd < 0) {
        s->dir_fd = open(s->dir, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
        if (s->dir_fd < 0) return RS_RET_IO_ERROR;
    }
    /* The failed cleanup can have stopped after unlinking only part of the
     * old topology.  Make a best effort to finish removing queue-owned files,
     * then prove none remain before publishing a fresh empty topology.  A
     * foreign file may still make remove_remaining_segments() report failure;
     * that is harmless here because it is deliberately excluded from the
     * segment-only verification and will make the next rmdir retry fail.
     * Conversely, an undeletable segment leaves the cleanup-in-progress state
     * authoritative and prevents stale records from being hidden by a new
     * normal state. */
    (void)remove_remaining_segments(s);
    sbool has_segments;
    if (directory_has_segments(s, &has_segments) != RS_RET_OK || has_segments) return RS_RET_IO_ERROR;
    const sbool state_present = faccessat(s->dir_fd, "state", F_OK, 0) == 0;
    if (!state_present) {
        if (s->state_fd >= 0) close(s->state_fd);
        s->state_fd = openat(s->dir_fd, "state", O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
        if (s->state_fd < 0 || ftruncate(s->state_fd, STATE_FILE_LEN) != 0) return RS_RET_IO_ERROR;
    }
    reset_after_dematerialize(s);
    s->next_segment = next_segment == 0 ? 1 : next_segment;
    if (state_present) {
        memcpy(s->uuid, old_uuid, sizeof(s->uuid));
        s->generation = old_generation;
    } else if (random_uuid(s->uuid) != RS_RET_OK) {
        return RS_RET_IO_ERROR;
    }
    /* Keep every state publication in cleanup-in-progress form until the new
     * active segment is durable.  Thus a crash during reconstruction finishes
     * deletion on restart instead of accepting a partially rebuilt store. */
    s->dematerializing = 1;
    if (!state_present) {
        const rsRetVal state_ret = write_state(s, 1, 1);
        if (state_ret != RS_RET_OK) {
            close(s->state_fd);
            s->state_fd = -1;
            unlinkat(s->dir_fd, "state", 0);
            return state_ret;
        }
    }
    rsRetVal r = create_active(s);
    if (r == RS_RET_OK) {
        s->dematerializing = 0;
        r = write_state(s, 1, 1);
        if (r != RS_RET_OK) s->dematerializing = 1;
    }
    if (r == RS_RET_OK) r = sync_dir(s);
    return r;
}

rsRetVal segdiskStoreDematerialize(segdisk_store_t *s) {
    if (s == NULL) return RS_RET_PARAM_ERROR;
    if (s->dir_fd < 0) return RS_RET_OK;
    if (!segdiskStoreCanDematerialize(s)) return RS_RET_RETRY;
    const uint64_t restore_next_segment = s->next_segment;
    s->dematerializing = 1;
    rsRetVal r = write_state(s, 1, 1);
    if (r != RS_RET_OK) {
        /* No destructive step is permitted until the cleanup intent is
         * durable.  The previous normal state and the still-open active
         * segment therefore remain a complete recoverable store when state
         * publication itself fails. */
        s->dematerializing = 0;
        ++s->stats.idle_cleanup_failures;
        return r;
    }
    test_fault(s, SEGDISK_TEST_FAULT_IDLE_EMPTY_PUBLISHED);
    if (s->active_fd >= 0) {
        if (close(s->active_fd) != 0 && r == RS_RET_OK) r = RS_RET_IO_ERROR;
        s->active_fd = -1;
    }
    if (r == RS_RET_OK) r = remove_remaining_segments(s);
    if (r == RS_RET_OK) r = sync_dir(s);
    if (r == RS_RET_OK) test_fault(s, SEGDISK_TEST_FAULT_IDLE_SEGMENTS_UNLINKED);
    if (r == RS_RET_OK && unlinkat(s->dir_fd, "state", 0) != 0 && errno != ENOENT) r = RS_RET_IO_ERROR;
    if (r == RS_RET_OK) test_fault(s, SEGDISK_TEST_FAULT_IDLE_STATE_UNLINKED);
    if (r == RS_RET_OK) r = sync_dir(s);
    free_segment(&s->active);
    free_segment(&s->read_segment);
    if (r == RS_RET_OK && rmdir(s->dir) != 0 && errno != ENOENT) r = RS_RET_IO_ERROR;
    if (r == RS_RET_OK) test_fault(s, SEGDISK_TEST_FAULT_IDLE_DIRECTORY_REMOVED);
    if (r == RS_RET_OK) {
        if (s->state_fd >= 0) close(s->state_fd);
        if (s->dir_fd >= 0) close(s->dir_fd);
        s->state_fd = s->dir_fd = -1;
        reset_after_dematerialize(s);
        ++s->stats.dematerializations;
    } else {
        ++s->stats.idle_cleanup_failures;
        const rsRetVal restore_ret = restore_empty_materialized(s, restore_next_segment);
        if (restore_ret != RS_RET_OK) {
            /* r is already a hard failure returned to the queue callback.  A
             * failed repair does not soften it: the durable
             * cleanup-in-progress state (or a state-free empty directory
             * after segment removal) makes restart complete deletion, while
             * the live worker retries cleanup in this process. */
            LogError(0, restore_ret, "%s: failed to restore an empty segmentedDisk store after idle cleanup error",
                     s->queue_name);
        }
    }
    return r;
}

#ifdef ENABLE_IMDIAG
rsRetVal segdiskStoreSetTestFault(segdisk_store_t *s, const char *point, unsigned int hit_count) {
    if (s == NULL || point == NULL || hit_count == 0) return RS_RET_PARAM_ERROR;
    for (size_t i = 0; i < sizeof(test_fault_names) / sizeof(test_fault_names[0]); ++i) {
        if (!strcmp(point, test_fault_names[i].name)) {
            s->test_fault_point = test_fault_names[i].point;
            s->test_fault_hits_remaining = hit_count;
            return RS_RET_OK;
        }
    }
    return RS_RET_INVALID_VALUE;
}

void segdiskStoreClearTestFault(segdisk_store_t *s) {
    if (s == NULL) return;
    s->test_fault_point = SEGDISK_TEST_FAULT_NONE;
    s->test_fault_hits_remaining = 0;
}
#endif

rsRetVal segdiskStoreClose(segdisk_store_t **ps, sbool empty) {
    if (ps == NULL || *ps == NULL) return RS_RET_OK;
    segdisk_store_t *s = *ps;
    rsRetVal r = RS_RET_OK;
    if (s->dir_fd < 0) {
        free(s->dir);
        free(s->queue_name);
        free(s);
        *ps = NULL;
        return RS_RET_OK;
    }
    if (s->active_fd >= 0) {
        if (!empty && s->active != NULL && s->active->record_count != 0)
            r = seal_active(s);
        else {
            close(s->active_fd);
            s->active_fd = -1;
        }
    }
    if (empty && s->dir_fd >= 0) {
        const uint64_t end = s->active_segment > s->last_data_segment ? s->active_segment : s->last_data_segment;
        for (uint64_t id = s->first_live_segment; id != 0 && id <= end; ++id) {
            rsRetVal local = unlink_segment_id(s, id, 0);
            if (local != RS_RET_OK && r == RS_RET_OK) r = local;
        }
        if (r == RS_RET_OK) r = remove_remaining_segments(s);
        if (r == RS_RET_OK && unlinkat(s->dir_fd, "state", 0) != 0 && errno != ENOENT) r = RS_RET_IO_ERROR;
    } else if (s->dir_fd >= 0 && r == RS_RET_OK) {
        capture_writer(s);
        r = write_state(s, 1, 1);
    }
    free_segment(&s->active);
    free_segment(&s->read_segment);
    while (s->pending_head != NULL) {
        segdisk_batch_ctx_t *n = s->pending_head->next;
        s->pending_head->pending = 0;
        release_batch_ctx(s->pending_head);
        s->pending_head = n;
    }
    if (s->state_fd >= 0) close(s->state_fd);
    if (s->dir_fd >= 0) close(s->dir_fd);
    if (empty && r == RS_RET_OK && rmdir(s->dir) != 0 && errno != ENOENT) r = RS_RET_IO_ERROR;
    free(s->dir);
    free(s->queue_name);
    free(s);
    *ps = NULL;
    return r;
}

void segdiskStoreGetStats(const segdisk_store_t *s, segdisk_store_stats_t *stats) {
    *stats = s->stats;
}
