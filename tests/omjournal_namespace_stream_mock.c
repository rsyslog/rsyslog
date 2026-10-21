/* SPDX-License-Identifier: Apache-2.0 */
/* Test preload for omjournal's systemd namespace stream path. Capture the API
 * arguments and return a regular file descriptor so the test has a deterministic
 * oracle without depending on a running systemd-journald namespace instance.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <systemd/sd-journal.h>
#include <unistd.h>

int sd_journal_stream_fd_with_namespace(const char *name_space,
                                        const char *identifier,
                                        int priority,
                                        int level_prefix) {
    const char *args_path = getenv("RSYSLOG_OMJOURNAL_MOCK_ARGS");
    const char *stream_path = getenv("RSYSLOG_OMJOURNAL_MOCK_STREAM");
    FILE *args;
    int fd;

    if (args_path == NULL || stream_path == NULL || name_space == NULL || identifier == NULL) return -EINVAL;

    args = fopen(args_path, "a");
    if (args == NULL) return -errno;
    if (fprintf(args, "namespace=%s\nidentifier=%s\npriority=%d\nlevel_prefix=%d\n", name_space, identifier, priority,
                level_prefix) < 0) {
        int saved_errno = errno;
        fclose(args);
        return -(saved_errno != 0 ? saved_errno : EIO);
    }
    if (fclose(args) != 0) return -(errno != 0 ? errno : EIO);

    fd = open(stream_path, O_WRONLY | O_CREAT | O_APPEND, 0600);
    if (fd < 0) return -errno;
    return fd;
}
