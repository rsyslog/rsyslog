/* rswatch.c
 * generic watched-file support for runtime-managed reloaders
 *
 * Copyright 2026 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *       -or-
 *       see COPYING.ASL20 in the source distribution
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef HAVE_SYS_INOTIFY_H
    #include <sys/inotify.h>
#endif

#include "rswatch.h"

struct rswatch_handle_s {
    char *id;
    char *path;
    char *dir;
    char *base;
    unsigned int debounce_ms;
    rswatch_cb_t cb;
    void *ctx;
#if defined(HAVE_INOTIFY_INIT) && defined(HAVE_SYS_INOTIFY_H)
    int wd;
    sbool pending;
    uint64_t due_at_ms;
#endif
    struct rswatch_handle_s *next;
};

#if defined(HAVE_INOTIFY_INIT) && defined(HAVE_SYS_INOTIFY_H)
typedef struct rswatch_state_s {
    pthread_mutex_t mut;
    rswatch_handle_t *handles;
    int ino_fd;
    sbool disabled;
} rswatch_state_t;

/* One process-global watcher state is sufficient because all activity is
 * multiplexed through the main housekeeping loop.
 */

static rswatch_state_t g_rswatch = {PTHREAD_MUTEX_INITIALIZER, NULL, -1, 0};
#endif

static char *rswatchDupDirname(const char *path) {
    const char *slash;
    size_t len;
    char *out;

    if (path == NULL || *path == '\0') {
        return NULL;
    }

    slash = strrchr(path, '/');
    if (slash == NULL) {
        return strdup(".");
    }
    if (slash == path) {
        return strdup("/");
    }

    len = (size_t)(slash - path);
    out = malloc(len + 1);
    if (out == NULL) {
        return NULL;
    }
    memcpy(out, path, len);
    out[len] = '\0';
    return out;
}

static char *rswatchDupBasename(const char *path) {
    const char *slash;

    if (path == NULL || *path == '\0') {
        return NULL;
    }

    slash = strrchr(path, '/');
    return strdup((slash == NULL) ? path : slash + 1);
}

static void rswatchFreeHandle(rswatch_handle_t *handle) {
    if (handle == NULL) {
        return;
    }
    free(handle->id);
    free(handle->path);
    free(handle->dir);
    free(handle->base);
    free(handle);
}

#if defined(HAVE_INOTIFY_INIT) && defined(HAVE_SYS_INOTIFY_H)
static rsRetVal rswatchSetNonBlocking(int fd) {
    int flags;
    DEFiRet;

    if (fd == -1) {
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    if ((flags = fcntl(fd, F_GETFL)) == -1) {
        ABORT_FINALIZE(RS_RET_IO_ERROR);
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        ABORT_FINALIZE(RS_RET_IO_ERROR);
    }

finalize_it:
    RETiRet;
}

static rsRetVal rswatchSetCloseOnExec(int fd) {
    int flags;
    DEFiRet;

    if (fd == -1) {
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    if ((flags = fcntl(fd, F_GETFD)) == -1) {
        ABORT_FINALIZE(RS_RET_IO_ERROR);
    }
    if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1) {
        ABORT_FINALIZE(RS_RET_IO_ERROR);
    }

finalize_it:
    RETiRet;
}

static rsRetVal rswatchEnsureInfraLocked(void) {
    int saved_errno = 0;
    DEFiRet;

    if (g_rswatch.disabled) {
        ABORT_FINALIZE(RS_RET_IO_ERROR);
    }

    if (g_rswatch.ino_fd == -1) {
        g_rswatch.ino_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
        saved_errno = errno;
        if (g_rswatch.ino_fd == -1) {
    #ifndef IN_CLOEXEC
            g_rswatch.ino_fd = inotify_init1(IN_NONBLOCK);
            if (g_rswatch.ino_fd != -1) {
                CHKiRet(rswatchSetCloseOnExec(g_rswatch.ino_fd));
            }
            saved_errno = errno;
    #endif
            if (g_rswatch.ino_fd == -1 && saved_errno == ENOSYS) {
                g_rswatch.ino_fd = inotify_init();
                if (g_rswatch.ino_fd != -1) {
                    CHKiRet(rswatchSetNonBlocking(g_rswatch.ino_fd));
                    CHKiRet(rswatchSetCloseOnExec(g_rswatch.ino_fd));
                }
            }
            if (g_rswatch.ino_fd == -1) {
                ABORT_FINALIZE(RS_RET_INOTIFY_INIT_FAILED);
            }
        }
    }

finalize_it:
    if (iRet != RS_RET_OK && g_rswatch.ino_fd != -1) {
        close(g_rswatch.ino_fd);
        g_rswatch.ino_fd = -1;
    }
    RETiRet;
}

static int rswatchFindDirWatchLocked(const char *dir) {
    rswatch_handle_t *handle;

    for (handle = g_rswatch.handles; handle != NULL; handle = handle->next) {
        if (!strcmp(handle->dir, dir)) {
            return handle->wd;
        }
    }
    return -1;
}

static sbool rswatchDirInUseLocked(int wd) {
    rswatch_handle_t *handle;

    for (handle = g_rswatch.handles; handle != NULL; handle = handle->next) {
        if (handle->wd == wd) {
            return 1;
        }
    }
    return 0;
}

static void rswatchDisableLocked(int err, const char *reason) {
    rswatch_handle_t *handle;

    if (!g_rswatch.disabled) {
        LogError(err, RS_RET_IO_ERROR, "rswatch: %s, disabling watched-file reload support", reason);
    }
    g_rswatch.disabled = 1;
    if (g_rswatch.ino_fd != -1) {
        close(g_rswatch.ino_fd);
        g_rswatch.ino_fd = -1;
    }
    for (handle = g_rswatch.handles; handle != NULL; handle = handle->next) {
        handle->wd = -1;
        handle->pending = 0;
        handle->due_at_ms = 0;
    }
}

/* Each matching event resets the quiet-period deadline. This coalesces bursty
 * editor save patterns into a single later reload callback.
 */
static void rswatchMarkEventLocked(int wd, const char *name, uint64_t now_ms) {
    rswatch_handle_t *handle;

    if (name == NULL || *name == '\0') {
        return;
    }

    for (handle = g_rswatch.handles; handle != NULL; handle = handle->next) {
        if (handle->wd == wd && !strcmp(handle->base, name)) {
            handle->pending = 1;
            handle->due_at_ms = now_ms + handle->debounce_ms;
        }
    }
}

static rswatch_handle_t *rswatchPopDueLocked(uint64_t now_ms) {
    rswatch_handle_t *handle;

    for (handle = g_rswatch.handles; handle != NULL; handle = handle->next) {
        if (handle->pending && handle->due_at_ms <= now_ms) {
            handle->pending = 0;
            return handle;
        }
    }
    return NULL;
}
#endif

rsRetVal rswatchRegister(const rswatch_desc_t *desc, rswatch_handle_t **out) {
    rswatch_handle_t *handle = NULL;
    sbool bLocked = 0;
    int wd;
    DEFiRet;

    if (out != NULL) {
        *out = NULL;
    }
    if (desc == NULL || desc->id == NULL || desc->path == NULL || *desc->path == '\0' || desc->cb == NULL) {
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

#if !defined(HAVE_INOTIFY_INIT) || !defined(HAVE_SYS_INOTIFY_H)
    ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
#else
    CHKmalloc(handle = calloc(1, sizeof(*handle)));
    CHKmalloc(handle->id = strdup(desc->id));
    CHKmalloc(handle->path = strdup(desc->path));
    CHKmalloc(handle->dir = rswatchDupDirname(desc->path));
    CHKmalloc(handle->base = rswatchDupBasename(desc->path));
    handle->debounce_ms = desc->debounce_ms;
    handle->cb = desc->cb;
    handle->ctx = desc->ctx;
    handle->wd = -1;

    pthread_mutex_lock(&g_rswatch.mut);
    bLocked = 1;
    CHKiRet(rswatchEnsureInfraLocked());
    wd = rswatchFindDirWatchLocked(handle->dir);
    if (wd == -1) {
        wd = inotify_add_watch(g_rswatch.ino_fd, handle->dir, IN_CLOSE_WRITE | IN_MOVED_TO | IN_ATTRIB | IN_CREATE);
        if (wd == -1) {
            ABORT_FINALIZE(RS_RET_IO_ERROR);
        }
    }
    handle->wd = wd;
    handle->next = g_rswatch.handles;
    g_rswatch.handles = handle;
    if (out != NULL) {
        *out = handle;
    }
    handle = NULL;
#endif

finalize_it:
#if defined(HAVE_INOTIFY_INIT) && defined(HAVE_SYS_INOTIFY_H)
    if (bLocked) {
        pthread_mutex_unlock(&g_rswatch.mut);
    }
#endif
    if (handle != NULL) {
        rswatchFreeHandle(handle);
    }
    RETiRet;
}

void rswatchUnregister(rswatch_handle_t *handle) {
#if defined(HAVE_INOTIFY_INIT) && defined(HAVE_SYS_INOTIFY_H)
    rswatch_handle_t **pp;

    if (handle == NULL) {
        return;
    }

    pthread_mutex_lock(&g_rswatch.mut);
    pp = &g_rswatch.handles;
    while (*pp != NULL) {
        if (*pp != handle) {
            pp = &(*pp)->next;
            continue;
        }

        *pp = handle->next;
        if (!g_rswatch.disabled && g_rswatch.ino_fd != -1 && handle->wd != -1 && !rswatchDirInUseLocked(handle->wd)) {
            inotify_rm_watch(g_rswatch.ino_fd, handle->wd);
        }
        if (g_rswatch.handles == NULL && g_rswatch.ino_fd != -1) {
            close(g_rswatch.ino_fd);
            g_rswatch.ino_fd = -1;
            g_rswatch.disabled = 0;
        }
        pthread_mutex_unlock(&g_rswatch.mut);
        rswatchFreeHandle(handle);
        return;
    }
    pthread_mutex_unlock(&g_rswatch.mut);
#else
    (void)handle;
#endif
}

int rswatchGetWaitFd(void) {
#if defined(HAVE_INOTIFY_INIT) && defined(HAVE_SYS_INOTIFY_H)
    int fd;
    pthread_mutex_lock(&g_rswatch.mut);
    fd = (g_rswatch.disabled || g_rswatch.handles == NULL) ? -1 : g_rswatch.ino_fd;
    pthread_mutex_unlock(&g_rswatch.mut);
    return fd;
#else
    return -1;
#endif
}

int rswatchComputeTimeoutMs(uint64_t now_ms, int default_timeout_ms) {
#if defined(HAVE_INOTIFY_INIT) && defined(HAVE_SYS_INOTIFY_H)
    rswatch_handle_t *handle;
    uint64_t diff;
    int timeout_ms = default_timeout_ms;

    pthread_mutex_lock(&g_rswatch.mut);
    if (g_rswatch.disabled || g_rswatch.handles == NULL) {
        pthread_mutex_unlock(&g_rswatch.mut);
        return timeout_ms;
    }

    for (handle = g_rswatch.handles; handle != NULL; handle = handle->next) {
        if (!handle->pending) {
            continue;
        }
        if (handle->due_at_ms <= now_ms) {
            timeout_ms = 0;
            break;
        }
        diff = handle->due_at_ms - now_ms;
        if (diff > (uint64_t)INT_MAX) {
            diff = (uint64_t)INT_MAX;
        }
        if (timeout_ms < 0 || (int)diff < timeout_ms) {
            timeout_ms = (int)diff;
        }
    }
    pthread_mutex_unlock(&g_rswatch.mut);
    return timeout_ms;
#else
    (void)now_ms;
    return default_timeout_ms;
#endif
}

void rswatchProcessIo(uint64_t now_ms) {
#if defined(HAVE_INOTIFY_INIT) && defined(HAVE_SYS_INOTIFY_H)
    char buf[4096];
    ssize_t rd;

    pthread_mutex_lock(&g_rswatch.mut);
    if (g_rswatch.disabled || g_rswatch.ino_fd == -1) {
        pthread_mutex_unlock(&g_rswatch.mut);
        return;
    }
    pthread_mutex_unlock(&g_rswatch.mut);

    while ((rd = read(g_rswatch.ino_fd, buf, sizeof(buf))) > 0) {
        ssize_t off = 0;

        pthread_mutex_lock(&g_rswatch.mut);
        /* inotify read() is expected to return complete events, but keep the
         * parser defensive in case a short trailing fragment is ever seen.
         */
        while (off + (ssize_t)sizeof(struct inotify_event) <= rd) {
            struct inotify_event evhdr;
            const char *name = NULL;

            memcpy(&evhdr, buf + off, sizeof(evhdr));
            /* This is not expected from inotify read(), but keep the parser
             * defensive if the trailing name payload is ever truncated.
             */
            if (off + (ssize_t)sizeof(struct inotify_event) + (ssize_t)evhdr.len > rd) {
                break;
            }
            if (evhdr.len > 0) {
                name = buf + off + sizeof(evhdr);
            }
            if ((evhdr.mask & (IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE | IN_ATTRIB)) != 0) {
                rswatchMarkEventLocked(evhdr.wd, name, now_ms);
            }
            off += (ssize_t)(sizeof(struct inotify_event) + evhdr.len);
        }
        pthread_mutex_unlock(&g_rswatch.mut);
    }

    if (rd < 0 && errno != EAGAIN && errno != EINTR) {
        pthread_mutex_lock(&g_rswatch.mut);
        rswatchDisableLocked(errno, "watch backend read failed");
        pthread_mutex_unlock(&g_rswatch.mut);
    }
#else
    (void)now_ms;
#endif
}

void rswatchDispatchDue(uint64_t now_ms) {
#if defined(HAVE_INOTIFY_INIT) && defined(HAVE_SYS_INOTIFY_H)
    while (1) {
        rswatch_handle_t *handle;
        rswatch_cb_t cb;
        void *ctx;

        pthread_mutex_lock(&g_rswatch.mut);
        if (g_rswatch.disabled) {
            pthread_mutex_unlock(&g_rswatch.mut);
            return;
        }
        handle = rswatchPopDueLocked(now_ms);
        if (handle == NULL) {
            pthread_mutex_unlock(&g_rswatch.mut);
            return;
        }
        cb = handle->cb;
        ctx = handle->ctx;
        pthread_mutex_unlock(&g_rswatch.mut);

        /* Dispatch unlocked so reload code can take its own locks without
         * inheriting rswatch's internal lock ordering.
         */
        cb(ctx, "watch");
    }
#else
    (void)now_ms;
#endif
}

void rswatchExit(void) {
#if defined(HAVE_INOTIFY_INIT) && defined(HAVE_SYS_INOTIFY_H)
    rswatch_handle_t *handle;

    pthread_mutex_lock(&g_rswatch.mut);
    handle = g_rswatch.handles;
    g_rswatch.handles = NULL;
    if (g_rswatch.ino_fd != -1) {
        close(g_rswatch.ino_fd);
        g_rswatch.ino_fd = -1;
    }
    g_rswatch.disabled = 0;
    pthread_mutex_unlock(&g_rswatch.mut);

    while (handle != NULL) {
        rswatch_handle_t *next = handle->next;
        rswatchFreeHandle(handle);
        handle = next;
    }
#endif
}
