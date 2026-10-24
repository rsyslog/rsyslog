/* SPDX-License-Identifier: Apache-2.0 */
/* Supply a deterministic raw RNG value only for full-long reads from the
 * Linux /dev/urandom descriptor. All unrelated reads keep libc behavior. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

static ssize_t (*next_read)(int, void *, size_t);
static pthread_once_t once = PTHREAD_ONCE_INIT;

static void resolve_read(void) {
    next_read = (ssize_t(*)(int, void *, size_t))dlsym(RTLD_NEXT, "read");
}

ssize_t read(int fd, void *buffer, size_t count) {
    char path[64], target[64];
    const int saved_errno = errno;
    pthread_once(&once, resolve_read);
    if (next_read == NULL) {
        errno = EIO;
        return -1;
    }
    snprintf(path, sizeof(path), "/proc/self/fd/%d", fd);
    const ssize_t len = readlink(path, target, sizeof(target) - 1);
    if (len >= 0) target[len] = '\0';
    if (count == sizeof(long) && len >= 0 && strcmp(target, "/dev/urandom") == 0) {
        const long value = LONG_MIN;
        const char *marker = getenv("RSYSLOG_RANDOM_RANGE_MARKER");
        memcpy(buffer, &value, sizeof(value));
        if (marker != NULL && *marker != '\0') {
            const int marker_fd = open(marker, O_WRONLY | O_CREAT | O_APPEND, 0600);
            if (marker_fd >= 0) {
                const ssize_t written = write(marker_fd, "injected\n", 9);
                (void)written;
                close(marker_fd);
            }
        }
        errno = saved_errno;
        return sizeof(value);
    }
    errno = saved_errno;
    return next_read(fd, buffer, count);
}
