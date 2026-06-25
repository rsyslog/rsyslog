#include <errno.h>

int libnet_write(void *handle __attribute__((unused))) {
    errno = EMSGSIZE;
    return -1;
}
