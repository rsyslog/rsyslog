/* A testing tool that just emits a number of
 * messages to the system log socket.
 *
 * Options
 *
 * -s severity (0..7 accoding to syslog spec, r "rolling", default 6)
 * -m number of messages to generate (default 500)
 * -C liblognorm-stdlog channel description
 * -f message format to use
 * -u Unix datagram socket path
 * -L generated message length for Unix datagram mode
 * -w milliseconds to wait after sending messages
 *
 * Part of the testbench for rsyslog.
 *
 * Copyright 2010-2018 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *       -or-
 *       see COPYING.ASL20 in the source distribution
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(_AIX)
    #include <unistd.h>
#else
    #include <getopt.h>
#endif
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <syslog.h>
#include <unistd.h>
#ifdef HAVE_LIBLOGGING_STDLOG
    #include <liblogging/stdlog.h>
#endif

static enum { FMT_NATIVE, FMT_SYSLOG_INJECT_L, FMT_SYSLOG_INJECT_C } fmt = FMT_NATIVE;

static void usage(void) {
    fprintf(stderr, "usage: syslog_caller [-m messages] [-s severity] [-u socket] [-L length] [-w milliseconds]\n");
    exit(1);
}

static void genRawMsg(char *buf, const size_t buflen, const int sev, const int iRun, const size_t msgLen) {
    if (msgLen > 0) {
        const size_t prefixLen = (size_t)snprintf(buf, buflen, "<%d>", sev % 8);
        if (prefixLen >= buflen) return;
        const size_t avail = buflen - prefixLen - 1;
        const size_t len = msgLen < avail ? msgLen : avail;
        memset(buf + prefixLen, 'A', len);
        buf[prefixLen + len] = '\0';
    } else {
        snprintf(buf, buflen, "<%d>test message nbr %d, severity=%d", sev % 8, iRun, sev % 8);
    }
}

static void sendRawUnix(const char *sockName, const int sev, const int iRun, const size_t msgLen) {
    int sock;
    struct sockaddr_un sa;
    char msgbuf[4096];

    if (strlen(sockName) >= sizeof(sa.sun_path)) {
        fprintf(stderr, "Unix socket path too long: %s\n", sockName);
        exit(1);
    }
    if ((sock = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }
    memset(&sa, 0, sizeof(sa));
    sa.sun_family = AF_UNIX;
    strncpy(sa.sun_path, sockName, sizeof(sa.sun_path) - 1);
    sa.sun_path[sizeof(sa.sun_path) - 1] = '\0';
    genRawMsg(msgbuf, sizeof(msgbuf), sev, iRun, msgLen);
    if (sendto(sock, msgbuf, strlen(msgbuf), 0, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        perror("sendto");
        close(sock);
        exit(1);
    }
    close(sock);
}


#ifdef HAVE_LIBLOGGING_STDLOG
/* buffer must be large "enough" [4K?] */
static void genMsg(char *buf, const int sev, const int iRun) {
    switch (fmt) {
        case FMT_NATIVE:
            snprintf(buf, 4096, "test message nbr %d, severity=%d", iRun, sev);
            break;
        case FMT_SYSLOG_INJECT_L:
            snprintf(buf, 4096, "%s", "test\n");
            break;
        case FMT_SYSLOG_INJECT_C:
            snprintf(buf, 4096, "%s", "test 1\t2");
            break;
    }
}
#endif

int main(int argc, char *argv[]) {
    int i;
    int opt;
    int bRollingSev = 0;
    int sev = 6;
    int msgs = 500;
    const char *unixSockName = NULL;
    size_t msgLen = 0;
    int waitMs = 0;
#ifdef HAVE_LIBLOGGING_STDLOG
    stdlog_channel_t logchan = NULL;
    const char *chandesc = "syslog:";
    char msgbuf[4096];
#endif

#ifdef HAVE_LIBLOGGING_STDLOG
    stdlog_init(STDLOG_USE_DFLT_OPTS);
    while ((opt = getopt(argc, argv, "m:s:C:f:u:L:w:")) != -1) {
#else
    while ((opt = getopt(argc, argv, "m:s:u:L:w:")) != -1) {
#endif
        switch (opt) {
            case 's':
                if (*optarg == 'r') {
                    bRollingSev = 1;
                    sev = 0;
                } else
#ifdef HAVE_LIBLOGGING_STDLOG
                    sev = atoi(optarg) % 8;
#else
                    sev = atoi(optarg);
#endif
                break;
            case 'm':
                msgs = atoi(optarg);
                break;
            case 'u':
                unixSockName = optarg;
                break;
            case 'L':
                msgLen = (size_t)atoi(optarg);
                if (msgLen >= 4096) msgLen = 4095;
                break;
            case 'w':
                waitMs = atoi(optarg);
                break;
#ifdef HAVE_LIBLOGGING_STDLOG
            case 'C':
                chandesc = optarg;
                break;
            case 'f':
                if (!strcmp(optarg, "syslog_inject-l"))
                    fmt = FMT_SYSLOG_INJECT_L;
                else if (!strcmp(optarg, "syslog_inject-c"))
                    fmt = FMT_SYSLOG_INJECT_C;
                else
                    usage();
                break;
#endif
            default:
                usage();
#ifdef HAVE_LIBLOGGING_STDLOG
                exit(1);
#endif
                break;
        }
    }

    if (unixSockName != NULL) {
        for (i = 0; i < msgs; ++i) {
            sendRawUnix(unixSockName, sev, i, msgLen);
            if (bRollingSev) sev++;
        }
        if (waitMs > 0) usleep((unsigned int)waitMs * 1000U);
        return 0;
    }

#ifdef HAVE_LIBLOGGING_STDLOG
    if ((logchan = stdlog_open(argv[0], 0, STDLOG_LOCAL1, chandesc)) == NULL) {
        fprintf(stderr, "error opening logchannel '%s': %s\n", chandesc, strerror(errno));
        exit(1);
    }
#endif
    for (i = 0; i < msgs; ++i) {
#ifdef HAVE_LIBLOGGING_STDLOG
        genMsg(msgbuf, sev, i);
        if (stdlog_log(logchan, sev, "%s", msgbuf) != 0) {
            perror("error writing log record");
            exit(1);
        }
#else
        syslog(sev % 8, "test message nbr %d, severity=%d", i, sev % 8);
#endif
        if (bRollingSev)
#ifdef HAVE_LIBLOGGING_STDLOG
            sev = (sev + 1) % 8;
#else
            sev++;
#endif
    }
    return (0);
}
