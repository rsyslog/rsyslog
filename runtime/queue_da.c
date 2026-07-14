/* Disk-assisted queue engine selection and durable marker handling. */
#include "config.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "queue_da.h"

#define QDA_MARKER_MAGIC "RSYSLOG-DA-ENGINE-V1 "

static rsRetVal make_path(char **path, const char *dir, const char *prefix, const char *suffix) {
    if (asprintf(path, "%s/%s%s", dir, prefix, suffix) < 0) {
        *path = NULL;
        return RS_RET_OUT_OF_MEMORY;
    }
    return RS_RET_OK;
}

static rsRetVal path_exists(const char *path, sbool *exists) {
    struct stat st;
    if (lstat(path, &st) == 0) {
        *exists = 1;
        return RS_RET_OK;
    }
    if (errno == ENOENT) {
        *exists = 0;
        return RS_RET_OK;
    }
    return RS_RET_IO_ERROR;
}

static sbool is_classic_segment_name(const char *name, const char *prefix) {
    const size_t prefix_len = strlen(prefix);
    if (strncmp(name, prefix, prefix_len) != 0 || name[prefix_len] != '.') return 0;
    const char *p = name + prefix_len + 1;
    if (*p == '\0') return 0;
    for (; *p != '\0'; ++p) {
        if (*p < '0' || *p > '9') return 0;
    }
    return 1;
}

static rsRetVal find_classic_segments(const char *dir, const char *prefix, sbool *found) {
    DIR *d = opendir(dir);
    if (d == NULL) return errno == ENOENT ? RS_RET_FILE_NOT_FOUND : RS_RET_IO_ERROR;
    *found = 0;
    errno = 0;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (is_classic_segment_name(entry->d_name, prefix)) {
            *found = 1;
            break;
        }
    }
    const int saved_errno = errno;
    closedir(d);
    if (!*found && saved_errno != 0) return RS_RET_IO_ERROR;
    return RS_RET_OK;
}

const char *qdaEngineName(qda_engine_mode_t engine) {
    switch (engine) {
        case QDA_ENGINE_AUTO:
            return "auto";
        case QDA_ENGINE_DISK:
            return "disk";
        case QDA_ENGINE_SEGMENTED_DISK:
            return "segmentedDisk";
        default:
            return "invalid";
    }
}

static rsRetVal read_marker(const char *path, sbool *present, qda_engine_mode_t *engine) {
    char buf[64];
    *present = 0;
    const int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return errno == ENOENT ? RS_RET_OK : RS_RET_IO_ERROR;
    struct stat st;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0 || st.st_size >= (off_t)sizeof(buf)) {
        close(fd);
        return RS_RET_INVALID_VALUE;
    }
    size_t off = 0;
    while (off < (size_t)st.st_size) {
        const ssize_t n = read(fd, buf + off, (size_t)st.st_size - off);
        if (n < 0) {
            if (errno == EINTR) continue;
            close(fd);
            return RS_RET_IO_ERROR;
        }
        if (n == 0) break;
        off += (size_t)n;
    }
    close(fd);
    if (off != (size_t)st.st_size) return RS_RET_IO_ERROR;
    buf[off] = '\0';
    const char *value = buf + sizeof(QDA_MARKER_MAGIC) - 1;
    if (strncmp(buf, QDA_MARKER_MAGIC, sizeof(QDA_MARKER_MAGIC) - 1) != 0) return RS_RET_INVALID_VALUE;
    if (!strcmp(value, "disk\n"))
        *engine = QDA_ENGINE_DISK;
    else if (!strcmp(value, "segmentedDisk\n"))
        *engine = QDA_ENGINE_SEGMENTED_DISK;
    else
        return RS_RET_INVALID_VALUE;
    *present = 1;
    return RS_RET_OK;
}

rsRetVal qdaEngineResolve(const qda_engine_config_t *config, qda_engine_result_t *result) {
    if (config == NULL || result == NULL || config->spool_dir == NULL || config->file_prefix == NULL)
        return RS_RET_PARAM_ERROR;
    memset(result, 0, sizeof(*result));
    char *qi_path = NULL;
    char *seg_path = NULL;
    char *marker_path = NULL;
    rsRetVal r = make_path(&qi_path, config->spool_dir, config->file_prefix, ".qi");
    if (r == RS_RET_OK) r = make_path(&seg_path, config->spool_dir, config->file_prefix, ".segq");
    if (r == RS_RET_OK) r = make_path(&marker_path, config->spool_dir, config->file_prefix, ".da-engine");
    if (r != RS_RET_OK) goto done;
    r = path_exists(qi_path, &result->classic_data);
    if (r == RS_RET_OK) r = path_exists(seg_path, &result->segmented_data);
    if (r == RS_RET_OK) r = read_marker(marker_path, &result->marker_present, &result->marker_engine);
    if (r != RS_RET_OK) goto done;

    sbool numeric_segments = 0;
    r = find_classic_segments(config->spool_dir, config->file_prefix, &numeric_segments);
    if (r == RS_RET_FILE_NOT_FOUND) r = RS_RET_OK;
    if (r != RS_RET_OK) goto done;
    result->classic_data = result->classic_data || numeric_segments;
    if (result->classic_data && result->segmented_data) {
        r = RS_RET_INVALID_VALUE;
        goto done;
    }
    if (result->marker_present && ((result->classic_data && result->marker_engine == QDA_ENGINE_SEGMENTED_DISK) ||
                                   (result->segmented_data && result->marker_engine == QDA_ENGINE_DISK))) {
        r = RS_RET_INVALID_VALUE;
        goto done;
    }

    /* An explicit selector may replace an opposite marker only after the
     * probes above proved that the old engine has no state or segments.  The
     * marker is intentionally not data: it is retained after a clean drain so
     * auto mode stays sticky, while an operator can explicitly choose the
     * engine for the next materialization.  Opposite-engine data remains a
     * hard conflict in both branches. */
    if (config->requested == QDA_ENGINE_DISK) {
        if (result->segmented_data) {
            r = RS_RET_INVALID_VALUE;
            goto done;
        }
        result->effective = QDA_ENGINE_DISK;
        result->reason = QDA_REASON_CONFIGURED;
    } else if (config->requested == QDA_ENGINE_SEGMENTED_DISK) {
        if (result->classic_data || config->requires_classic_features) {
            r = RS_RET_INVALID_VALUE;
            goto done;
        }
        result->effective = QDA_ENGINE_SEGMENTED_DISK;
        result->reason = QDA_REASON_CONFIGURED;
    } else if (config->requested != QDA_ENGINE_AUTO) {
        r = RS_RET_INVALID_VALUE;
        goto done;
    } else if (result->classic_data) {
        result->effective = QDA_ENGINE_DISK;
        result->reason = QDA_REASON_CLASSIC_DATA;
    } else if (result->segmented_data) {
        if (config->requires_classic_features) {
            r = RS_RET_INVALID_VALUE;
            goto done;
        }
        result->effective = QDA_ENGINE_SEGMENTED_DISK;
        result->reason = QDA_REASON_SEGMENTED_DATA;
    } else if (config->requires_classic_features) {
        result->effective = QDA_ENGINE_DISK;
        result->reason = QDA_REASON_CLASSIC_FEATURE;
    } else if (result->marker_present && result->marker_engine == QDA_ENGINE_DISK && !config->auto_upgrade) {
        result->effective = QDA_ENGINE_DISK;
        result->reason = QDA_REASON_MARKER;
    } else if (result->marker_present && result->marker_engine == QDA_ENGINE_DISK && config->auto_upgrade) {
        /* This is the requested upgrade boundary: the durable marker proves
         * that the previous engine was classic, while the absence of .qi and
         * numeric segments proves that it drained.  Selection changes only
         * during this restart; StartDA atomically replaces the marker before
         * constructing the new child, and no records are converted. */
        result->effective = QDA_ENGINE_SEGMENTED_DISK;
        result->reason = QDA_REASON_AUTO_UPGRADE;
    } else if (result->marker_present) {
        result->effective = result->marker_engine;
        result->reason = QDA_REASON_MARKER;
    } else {
        result->effective = QDA_ENGINE_SEGMENTED_DISK;
        result->reason = QDA_REASON_FRESH_DEFAULT;
    }

done:
    free(qi_path);
    free(seg_path);
    free(marker_path);
    return r;
}

static rsRetVal write_all(int fd, const char *buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        const ssize_t n = write(fd, buf + off, len - off);
        if (n < 0) {
            if (errno == EINTR) continue;
            return RS_RET_IO_ERROR;
        }
        off += (size_t)n;
    }
    return RS_RET_OK;
}

rsRetVal qdaEngineWriteMarker(const char *spool_dir, const char *file_prefix, qda_engine_mode_t engine) {
    if (spool_dir == NULL || file_prefix == NULL || engine == QDA_ENGINE_AUTO) return RS_RET_PARAM_ERROR;
    char *path = NULL;
    char *tmp = NULL;
    rsRetVal r = make_path(&path, spool_dir, file_prefix, ".da-engine");
    if (r != RS_RET_OK) return r;
    const char *name = qdaEngineName(engine);
    char content[64];
    const int content_len = snprintf(content, sizeof(content), "%s%s\n", QDA_MARKER_MAGIC, name);
    if (content_len < 0 || content_len >= (int)sizeof(content)) {
        free(path);
        return RS_RET_INTERNAL_ERROR;
    }
    if (asprintf(&tmp, "%s.tmp.%ld", path, (long)getpid()) < 0) {
        free(path);
        return RS_RET_OUT_OF_MEMORY;
    }
    int fd = open(tmp, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (fd < 0) {
        if (errno == EEXIST && unlink(tmp) == 0)
            fd = open(tmp, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
    }
    if (fd < 0) {
        r = RS_RET_IO_ERROR;
        goto done;
    }
    r = write_all(fd, content, (size_t)content_len);
    if (r == RS_RET_OK && fdatasync(fd) != 0) r = RS_RET_IO_ERROR;
    if (close(fd) != 0 && r == RS_RET_OK) r = RS_RET_IO_ERROR;
    if (r == RS_RET_OK && rename(tmp, path) != 0) r = RS_RET_IO_ERROR;
    if (r == RS_RET_OK) {
        const int dirfd = open(spool_dir, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
        if (dirfd < 0 || fsync(dirfd) != 0) r = RS_RET_IO_ERROR;
        if (dirfd >= 0) close(dirfd);
    }
    if (r != RS_RET_OK) unlink(tmp);
done:
    free(tmp);
    free(path);
    return r;
}
