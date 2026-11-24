/* SPDX-License-Identifier: Apache-2.0 */
/* Replace the journal transport, including systemd's default location wrappers.
 * Each API independently fails its retry message once. No real journal is used.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>

static int record_send(const char *api, const char *message) {
    static unsigned int retried[2];
    const unsigned int index = strcmp(api, "sendv") == 0;
    int result = 0;
    const char *token = strstr(message, "journal-test-");
    if (token == NULL) return 0;
    if (strstr(token, "retry") != NULL && retried[index]++ == 0) result = -EAGAIN;
    if (strstr(token, "invalid") != NULL) result = -EINVAL;
    if (strstr(token, "oversize") != NULL) result = -EMSGSIZE;
    const char *path = getenv("RSYSLOG_JOURNAL_SEND_EVENTS");
    if (path == NULL) _exit(90);
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0600);
    if (fd < 0) _exit(91);
    if (dprintf(fd, "%s %s %d\n", api, token, result) < 0) _exit(92);
    if (close(fd) != 0) _exit(93);
    /* systemd returns negative errno values without setting errno. */
    errno = EDOM;
    return result;
}

int sd_journal_send(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    const char *message = va_arg(ap, const char *);
    int result = strcmp(format, "MESSAGE=%s") == 0 ? record_send("send", message) : -EINVAL;
    va_end(ap);
    return result;
}

int sd_journal_send_with_location(const char *file, const char *line, const char *func, const char *format, ...) {
    (void)file;
    (void)line;
    (void)func;
    va_list ap;
    va_start(ap, format);
    const char *message = va_arg(ap, const char *);
    int result = strcmp(format, "MESSAGE=%s") == 0 ? record_send("send", message) : -EINVAL;
    va_end(ap);
    return result;
}

int sd_journal_sendv(const struct iovec *iov, int count) {
    for (int i = 0; i < count; ++i) {
        if (iov[i].iov_len >= 8 && memcmp(iov[i].iov_base, "MESSAGE=", 8) == 0)
            return record_send("sendv", (const char *)iov[i].iov_base + 8);
    }
    return -EINVAL;
}

int sd_journal_sendv_with_location(
    const char *file, const char *line, const char *func, const struct iovec *iov, int count) {
    (void)file;
    (void)line;
    (void)func;
    return sd_journal_sendv(iov, count);
}
