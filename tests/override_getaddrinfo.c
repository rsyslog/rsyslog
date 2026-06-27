#include "config.h"
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static int make_passive_ipv4(const char *service, struct addrinfo **res) {
    struct addrinfo *ai = NULL;
    struct sockaddr_in *sa = NULL;
    char *endptr = NULL;
    const long port = (service == NULL) ? 0 : strtol(service, &endptr, 10);

    if (res == NULL || (service != NULL && (*service == '\0' || *endptr != '\0' || port < 0 || port > 65535))) {
        return EAI_NONAME;
    }

    ai = calloc(1, sizeof(*ai));
    sa = calloc(1, sizeof(*sa));
    if (ai == NULL || sa == NULL) {
        free(ai);
        free(sa);
        return EAI_MEMORY;
    }

    sa->sin_family = AF_INET;
    sa->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sa->sin_port = htons((uint16_t)port);

    ai->ai_family = AF_INET;
    ai->ai_socktype = SOCK_STREAM;
    ai->ai_protocol = IPPROTO_TCP;
    ai->ai_addrlen = sizeof(*sa);
    ai->ai_addr = (struct sockaddr *)sa;
    *res = ai;
    return 0;
}

int getaddrinfo(const char *node,
                const char *service,
                const struct addrinfo *hints __attribute__((unused)),
                struct addrinfo **res) {
    if (node == NULL || strcmp(node, "**UNSPECIFIED**") == 0) {
        return make_passive_ipv4(service, res);
    }
    if (node != NULL && strcmp(node, "nonfqdn") == 0) {
        const char *marker = getenv("RSYSLOG_GETADDRINFO_MARKER");
        if (marker != NULL && marker[0] != '\0') {
            const int fd = open(marker, O_CREAT | O_WRONLY | O_APPEND, 0600);
            if (fd >= 0) {
                const ssize_t written = write(fd, "nonfqdn\n", sizeof("nonfqdn\n") - 1);
                (void)written;
                (void)close(fd);
            }
        }
        if (res != NULL) {
            *res = NULL;
        }
        return EAI_MEMORY;
    }
    if (res != NULL) {
        *res = NULL;
    }
    return EAI_NONAME;
}
