/* SPDX-License-Identifier: Apache-2.0 */
/* Replace klogctl() for imklog tests. Each call is recorded in the file
 * named by RSYSLOG_KLOGCTL_EVENTS and reported as successful, so tests
 * need no privileges and never change the host's kernel settings.
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int klogctl(int type, char *bufp, int len) {
    (void)bufp;
    const char *path = getenv("RSYSLOG_KLOGCTL_EVENTS");
    if (path == NULL) _exit(90);
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0600);
    if (fd < 0) _exit(91);
    if (dprintf(fd, "klogctl %d %d\n", type, len) < 0) _exit(92);
    if (close(fd) != 0) _exit(93);
    return 0;
}
