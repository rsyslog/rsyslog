/* receives messages from a specified unix sockets and writes
 * output to specfied file.
 *
 * Command line options:
 * -s name of socket (required)
 * -o name of output file (stdout if not given)
 * -l add newline after each message received (default: do not add anything)
 * -t timeout in seconds (default 60)
 *
 * Part of the testbench for rsyslog.
 *
 * Copyright 2010 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Rsyslog is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rsyslog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Rsyslog.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#if defined(_AIX)
    #include <sys/types.h>
    #include <unistd.h>
    #include <sys/socket.h>
    #include <sys/socketvar.h>
#else
    #include <getopt.h>
#endif
#include <sys/un.h>
#include <netdb.h>
#include <poll.h>
#include <errno.h>
#if defined(__FreeBSD__)
    #include <sys/socket.h>
#endif

#define DFLT_TIMEOUT 60

char *sockName = NULL;
int sockType = SOCK_DGRAM;
int sockConnected = 0;
int addNL = 0;
int abstract;
#define MAX_FDS 4
int sockBacklog = MAX_FDS - 1;
struct pollfd fds[MAX_FDS];
int nfds;


/* called to clean up on exit
 */
void cleanup(void) {
    int index;

    if (!abstract) unlink(sockName);
    for (index = 0; index < nfds; ++index) {
        close(fds[index].fd);
    }
}


void doTerm(int __attribute__((unused)) signum) {
    exit(1);
}


void usage(void) {
    fprintf(stderr,
            "usage: uxsockrcvr -s /socket/name -o /output/file -l\n"
            "-l adds newline after each message received\n"
            "-s MUST be specified\n"
            "-a Use abstract socket name\n"
            "-T {DGRAM|STREAM"
#ifdef SOCK_SEQPACKET
            "|SEQPACKET"
#endif /* def SOCK_SEQPACKET */
            "} Set socket type (default DGRAM)\n"
            "if -o ist not specified, stdout is used\n");
    exit(1);
}

static struct {
    const char *id;
    int val;
    int connected;
} _sockType_map[] = {
    {"DGRAM", SOCK_DGRAM, 0},
    {"STREAM", SOCK_STREAM, 1},
#ifdef SOCK_SEQPACKET
    {"SEQPACKET", SOCK_SEQPACKET, 1},
#endif /* def SOCK_SEQPACKET */
    {NULL, 0, 0},
};

static void _decode_sockType(const char *s) {
    int index;

    for (index = 0; _sockType_map[index].id; ++index) {
        if (!strcasecmp(s, _sockType_map[index].id)) {
            sockType = _sockType_map[index].val;
            sockConnected = _sockType_map[index].connected;
            return;
        }
    }
    fprintf(stderr, "?Illegal socket type '%s'. Valid socket types:\n", s);

    for (index = 0; _sockType_map[index].id; ++index) {
        fprintf(stderr, "  %s\n", _sockType_map[index].id);
    }
    exit(1);
}

int main(int argc, char *argv[]) {
    int opt;
    int rlen;
    int timeout = DFLT_TIMEOUT;
    FILE *fp = stdout;
    unsigned char data[128 * 1024];
    struct sockaddr_un addr; /* address of server */
    struct sockaddr from;
    socklen_t fromlen;
    int index;

    if (argc < 2) {
        fprintf(stderr, "error: too few arguments!\n");
        usage();
    }

    while ((opt = getopt(argc, argv, "as:o:lt:T:")) != EOF) {
        switch ((char)opt) {
            case 'a':
                abstract = 1;
                break;
            case 'l':
                addNL = 1;
                break;
            case 's':
                sockName = optarg;
                break;
            case 'o':
                if ((fp = fopen(optarg, "w")) == NULL) {
                    perror(optarg);
                    exit(1);
                }
                break;
            case 't':
                timeout = atoi(optarg);
                break;
            case 'T':
                _decode_sockType(optarg);
                break;
            default:
                usage();
        }
    }

    timeout = timeout * 1000;

    if (sockName == NULL) {
        fprintf(stderr, "error: -s /socket/name must be specified!\n");
        exit(1);
    }

    if (signal(SIGTERM, doTerm) == SIG_ERR) {
        perror("signal(SIGTERM, ...)");
        exit(1);
    }
    if (signal(SIGINT, doTerm) == SIG_ERR) {
        perror("signal(SIGINT, ...)");
        exit(1);
    }

    /*      Create a UNIX datagram socket for server        */
    if ((fds[0].fd = socket(AF_UNIX, sockType, 0)) < 0) {
        perror("server: socket");
        exit(1);
    }

    atexit(cleanup);

    /*      Set up address structure for server socket      */
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path + abstract, sockName, sizeof(addr.sun_path) - abstract);
    /*
     * Abstract socket addresses do not require termination.
     */
    if (!abstract && addr.sun_path[sizeof(addr.sun_path) - 1]) {
        addr.sun_path[sizeof(addr.sun_path) - 1] = 0;
        fprintf(stderr, "warning: socket path truncated: %s\n", addr.sun_path);
    }

    /*
     * For pathname addresses, the +1 is the terminating \0 character.
     * For abstract addresses, the +1 is the leading \0 character.
     */
    if (bind(fds[0].fd, (struct sockaddr *)&addr, offsetof(struct sockaddr_un, sun_path) + strlen(sockName) + 1) < 0) {
        perror("server: bind");
        close(fds[0].fd);
        exit(1);
    }

    if (sockConnected && listen(fds[0].fd, sockBacklog) == -1) {
        perror("server: listen");
        close(fds[0].fd);
        exit(1);
    }

    fds[0].events = POLLIN;
    nfds = 1;

    /* we now run in an endless loop. We do not check who sends us
     * data. This should be no problem for our testbench use.
     */

    while (1) {
        rlen = poll(fds, nfds, timeout);
        if (rlen == -1) {
            perror("uxsockrcvr : poll\n");
            exit(1);
        } else if (rlen == 0) {
            fprintf(stderr, "Socket timed out - nothing to receive\n");
            exit(1);
        }
        if (sockConnected && fds[0].revents & POLLIN) {
            fds[nfds].fd = accept(fds[0].fd, NULL, NULL);
            fds[nfds].events = POLLIN;
            fds[nfds].revents = 0;
            if (fds[nfds].fd < 0) {
                perror("accept");
            } else if (++nfds == MAX_FDS) {
                fds[0].events = 0;
            }
        }
        for (index = sockConnected; index < nfds; ++index) {
            if (fds[index].revents & POLLIN) {
                fromlen = sizeof(from);
                rlen = recvfrom(fds[index].fd, data, 2000, 0, &from, &fromlen);
                if (rlen == -1) {
                    perror("uxsockrcvr : recv\n");
                    exit(1);
                } else {
                    fwrite(data, 1, rlen, fp);
                    if (addNL) fputc('\n', fp);
                }
            }
            if (fds[index].revents & POLLHUP) {
                close(fds[index].fd);
                fds[index].fd = -1;
                fds[index].events = 0;
            }
        }
        while (nfds && fds[nfds - 1].fd < 0) {
            --nfds;
        }
    }

    return 0;
}
