#define _GNU_SOURCE
#include "config.h"

#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <unistd.h>

typedef int (*epoll_ctl_func_t)(int epfd, int op, int fd, struct epoll_event *event);

int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
    static epoll_ctl_func_t real_epoll_ctl;
    const char *const marker = getenv("RSYSLOG_TEST_EPOLL_CTL_FAIL_MARKER");

    if (op == EPOLL_CTL_ADD && marker != NULL && marker[0] != '\0' && access(marker, F_OK) == 0) {
        errno = ENOMEM;
        return -1;
    }

    if (real_epoll_ctl == NULL) {
        real_epoll_ctl = (epoll_ctl_func_t)dlsym(RTLD_NEXT, "epoll_ctl");
        if (real_epoll_ctl == NULL) {
            errno = ENOSYS;
            return -1;
        }
    }

    return real_epoll_ctl(epfd, op, fd, event);
}
