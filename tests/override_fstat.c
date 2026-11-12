/* SPDX-License-Identifier: Apache-2.0 */

/* Keep the compiler's file-offset/time ABI feature macros intact. On 32-bit
 * glibc, _TIME_BITS=64 requires _FILE_OFFSET_BITS=64. Do not include sys/stat.h:
 * it redirects both public fstat names to __fstat64_time64 in that mode. This
 * interposer needs their separate linker names and never inspects a stat buffer,
 * so use opaque ABI-specific types and forward each buffer to the same symbol.
 */
#define _GNU_SOURCE

#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct stat;
struct stat64;
struct __stat64_t64;

typedef int (*fstat_func_t)(int fd, struct stat *buf);
typedef int (*fstat64_func_t)(int fd, struct stat64 *buf);

static pthread_mutex_t fail_mutex = PTHREAD_MUTEX_INITIALIZER;
static int already_failed;

static int has_failed(void) {
    int failed;

    pthread_mutex_lock(&fail_mutex);
    failed = already_failed;
    pthread_mutex_unlock(&fail_mutex);
    return failed;
}

static int claim_failure(void) {
    int claimed;

    pthread_mutex_lock(&fail_mutex);
    claimed = !already_failed;
    already_failed = 1;
    pthread_mutex_unlock(&fail_mutex);
    return claimed;
}

/* Fails once, on the first fstat() of a fd whose name ends with
 * RSYSLOG_TEST_FSTAT_FAIL_SUFFIX and whose position is past 0. Opening a file
 * fstat()s it at position 0; only a stream seeked to a restored offset is
 * targeted.
 */
static int should_fail(int fd) {
    const char *const suffix = getenv("RSYSLOG_TEST_FSTAT_FAIL_SUFFIX");
    char link[64];
    char path[PATH_MAX];
    ssize_t len;
    size_t suffix_len;

    if (suffix == NULL || suffix[0] == '\0' || has_failed()) {
        return 0;
    }
    if (lseek(fd, 0, SEEK_CUR) <= 0) {
        return 0;
    }
    snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
    len = readlink(link, path, sizeof(path) - 1);
    if (len < 0) {
        return 0;
    }
    path[len] = '\0';
    suffix_len = strlen(suffix);
    if ((size_t)len < suffix_len || strcmp(path + len - suffix_len, suffix) != 0) {
        return 0;
    }
    return claim_failure();
}

int fstat(int fd, struct stat *buf) {
    static fstat_func_t real_fstat;

    if (real_fstat == NULL) {
        real_fstat = (fstat_func_t)dlsym(RTLD_NEXT, "fstat");
        if (real_fstat == NULL) {
            errno = ENOSYS;
            return -1;
        }
    }
    if (should_fail(fd)) {
        errno = EIO;
        return -1;
    }
    return real_fstat(fd, buf);
}

int fstat64(int fd, struct stat64 *buf) {
    static fstat64_func_t real_fstat64;

    if (real_fstat64 == NULL) {
        real_fstat64 = (fstat64_func_t)dlsym(RTLD_NEXT, "fstat64");
        if (real_fstat64 == NULL) {
            errno = ENOSYS;
            return -1;
        }
    }
    if (should_fail(fd)) {
        errno = EIO;
        return -1;
    }
    return real_fstat64(fd, buf);
}

/* glibc's time64 ABI on 32-bit targets has a different buffer layout from
 * legacy fstat64. Forward it unchanged to its matching libc entry point.
 */
typedef int (*fstat64_time64_func_t)(int fd, struct __stat64_t64 *buf);

int __fstat64_time64(int fd, struct __stat64_t64 *buf) {
    static fstat64_time64_func_t real_fstat64_time64;

    if (real_fstat64_time64 == NULL) {
        real_fstat64_time64 = (fstat64_time64_func_t)dlsym(RTLD_NEXT, "__fstat64_time64");
        if (real_fstat64_time64 == NULL) {
            errno = ENOSYS;
            return -1;
        }
    }
    if (should_fail(fd)) {
        errno = EIO;
        return -1;
    }
    return real_fstat64_time64(fd, buf);
}

/* Before glibc 2.33, fstat() and fstat64() are inline wrappers linked into
 * rsyslogd from libc_nonshared.a that call these, so only these are seen.
 */
typedef int (*fxstat_func_t)(int ver, int fd, struct stat *buf);
typedef int (*fxstat64_func_t)(int ver, int fd, struct stat64 *buf);

int __fxstat(int ver, int fd, struct stat *buf) {
    static fxstat_func_t real_fxstat;

    if (real_fxstat == NULL) {
        real_fxstat = (fxstat_func_t)dlsym(RTLD_NEXT, "__fxstat");
        if (real_fxstat == NULL) {
            errno = ENOSYS;
            return -1;
        }
    }
    if (should_fail(fd)) {
        errno = EIO;
        return -1;
    }
    return real_fxstat(ver, fd, buf);
}

int __fxstat64(int ver, int fd, struct stat64 *buf) {
    static fxstat64_func_t real_fxstat64;

    if (real_fxstat64 == NULL) {
        real_fxstat64 = (fxstat64_func_t)dlsym(RTLD_NEXT, "__fxstat64");
        if (real_fxstat64 == NULL) {
            errno = ENOSYS;
            return -1;
        }
    }
    if (should_fail(fd)) {
        errno = EIO;
        return -1;
    }
    return real_fxstat64(ver, fd, buf);
}
