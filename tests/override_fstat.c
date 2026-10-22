/* Both names are wrapped and _FILE_OFFSET_BITS is kept undefined: with it,
 * glibc redirects fstat() to fstat64(), which rsyslogd calls on LP64 too.
 */
#undef _FILE_OFFSET_BITS
#define _GNU_SOURCE

#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef int (*fstat_func_t)(int fd, struct stat *buf);
typedef int (*fstat64_func_t)(int fd, struct stat64 *buf);

static int already_failed;

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

    if (suffix == NULL || suffix[0] == '\0' || __atomic_load_n(&already_failed, __ATOMIC_SEQ_CST)) {
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
    return !__atomic_exchange_n(&already_failed, 1, __ATOMIC_SEQ_CST);
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
