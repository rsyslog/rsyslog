/* a very simplistic tcp receiver for the rsyslog testbench.
 *
 * Author Philippe Duveau
 *
 * This file is contribution of the rsyslog project.
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
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <inttypes.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#if defined(__FreeBSD__)
    #include <netinet/in.h>
#endif

#include "rsyslog.h"


#include <amqp.h>
#include <amqp_framing.h>

#define AMQP_STARTING ((uchar)0x10)
#define AMQP_STOP ((uchar)0x00)

#define AMQP_BEHAVIOR_STANDARD 1
#define AMQP_BEHAVIOR_NOEXCH 2
#define AMQP_BEHAVIOR_DECEXCH 3
#define AMQP_BEHAVIOR_BADEXCH 4

uchar connection_start[487] = {
    0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0xDF, 0x00, 0x0A, 0x00, 0x0A, 0x00, 0x09, 0x00, 0x00, 0x01, 0xBA, 0x0C, 0x63,
    0x61, 0x70, 0x61, 0x62, 0x69, 0x6C, 0x69, 0x74, 0x69, 0x65, 0x73, 0x46, 0x00, 0x00, 0x00, 0xC7, 0x12, 0x70, 0x75,
    0x62, 0x6C, 0x69, 0x73, 0x68, 0x65, 0x72, 0x5F, 0x63, 0x6F, 0x6E, 0x66, 0x69, 0x72, 0x6D, 0x73, 0x74, 0x01, 0x1A,
    0x65, 0x78, 0x63, 0x68, 0x61, 0x6E, 0x67, 0x65, 0x5F, 0x65, 0x78, 0x63, 0x68, 0x61, 0x6E, 0x67, 0x65, 0x5F, 0x62,
    0x69, 0x6E, 0x64, 0x69, 0x6E, 0x67, 0x73, 0x74, 0x01, 0x0A, 0x62, 0x61, 0x73, 0x69, 0x63, 0x2E, 0x6E, 0x61, 0x63,
    0x6B, 0x74, 0x01, 0x16, 0x63, 0x6F, 0x6E, 0x73, 0x75, 0x6D, 0x65, 0x72, 0x5F, 0x63, 0x61, 0x6E, 0x63, 0x65, 0x6C,
    0x5F, 0x6E, 0x6F, 0x74, 0x69, 0x66, 0x79, 0x74, 0x01, 0x12, 0x63, 0x6F, 0x6E, 0x6E, 0x65, 0x63, 0x74, 0x69, 0x6F,
    0x6E, 0x2E, 0x62, 0x6C, 0x6F, 0x63, 0x6B, 0x65, 0x64, 0x74, 0x01, 0x13, 0x63, 0x6F, 0x6E, 0x73, 0x75, 0x6D, 0x65,
    0x72, 0x5F, 0x70, 0x72, 0x69, 0x6F, 0x72, 0x69, 0x74, 0x69, 0x65, 0x73, 0x74, 0x01, 0x1C, 0x61, 0x75, 0x74, 0x68,
    0x65, 0x6E, 0x74, 0x69, 0x63, 0x61, 0x74, 0x69, 0x6F, 0x6E, 0x5F, 0x66, 0x61, 0x69, 0x6C, 0x75, 0x72, 0x65, 0x5F,
    0x63, 0x6C, 0x6F, 0x73, 0x65, 0x74, 0x01, 0x10, 0x70, 0x65, 0x72, 0x5F, 0x63, 0x6F, 0x6E, 0x73, 0x75, 0x6D, 0x65,
    0x72, 0x5F, 0x71, 0x6F, 0x73, 0x74, 0x01, 0x0F, 0x64, 0x69, 0x72, 0x65, 0x63, 0x74, 0x5F, 0x72, 0x65, 0x70, 0x6C,
    0x79, 0x5F, 0x74, 0x6F, 0x74, 0x01, 0x0C, 0x63, 0x6C, 0x75, 0x73, 0x74, 0x65, 0x72, 0x5F, 0x6E, 0x61, 0x6D, 0x65,
    0x53, 0x00, 0x00, 0x00, 0x0D, 0x72, 0x61, 0x62, 0x62, 0x69, 0x74, 0x40, 0x73, 0x65, 0x72, 0x76, 0x65, 0x72, 0x09,
    0x63, 0x6F, 0x70, 0x79, 0x72, 0x69, 0x67, 0x68, 0x74, 0x53, 0x00, 0x00, 0x00, 0x2E, 0x43, 0x6F, 0x70, 0x79, 0x72,
    0x69, 0x67, 0x68, 0x74, 0x20, 0x28, 0x43, 0x29, 0x20, 0x32, 0x30, 0x30, 0x37, 0x2D, 0x32, 0x30, 0x31, 0x36, 0x20,
    0x50, 0x69, 0x76, 0x6F, 0x74, 0x61, 0x6C, 0x20, 0x53, 0x6F, 0x66, 0x74, 0x77, 0x61, 0x72, 0x65, 0x2C, 0x20, 0x49,
    0x6E, 0x63, 0x2E, 0x0B, 0x69, 0x6E, 0x66, 0x6F, 0x72, 0x6D, 0x61, 0x74, 0x69, 0x6F, 0x6E, 0x53, 0x00, 0x00, 0x00,
    0x35, 0x4C, 0x69, 0x63, 0x65, 0x6E, 0x73, 0x65, 0x64, 0x20, 0x75, 0x6E, 0x64, 0x65, 0x72, 0x20, 0x74, 0x68, 0x65,
    0x20, 0x4D, 0x50, 0x4C, 0x2E, 0x20, 0x20, 0x53, 0x65, 0x65, 0x20, 0x68, 0x74, 0x74, 0x70, 0x3A, 0x2F, 0x2F, 0x77,
    0x77, 0x77, 0x2E, 0x72, 0x61, 0x62, 0x62, 0x69, 0x74, 0x6D, 0x71, 0x2E, 0x63, 0x6F, 0x6D, 0x2F, 0x08, 0x70, 0x6C,
    0x61, 0x74, 0x66, 0x6F, 0x72, 0x6D, 0x53, 0x00, 0x00, 0x00, 0x0A, 0x45, 0x72, 0x6C, 0x61, 0x6E, 0x67, 0x2F, 0x4F,
    0x54, 0x50, 0x07, 0x70, 0x72, 0x6F, 0x64, 0x75, 0x63, 0x74, 0x53, 0x00, 0x00, 0x00, 0x08, 0x52, 0x61, 0x62, 0x62,
    0x69, 0x74, 0x4D, 0x51, 0x07, 0x76, 0x65, 0x72, 0x73, 0x69, 0x6F, 0x6E, 0x53, 0x00, 0x00, 0x00, 0x05, 0x33, 0x2E,
    0x36, 0x2E, 0x32, 0x00, 0x00, 0x00, 0x0E, 0x41, 0x4D, 0x51, 0x50, 0x4C, 0x41, 0x49, 0x4E, 0x20, 0x50, 0x4C, 0x41,
    0x49, 0x4E, 0x00, 0x00, 0x00, 0x05, 0x65, 0x6E, 0x5F, 0x55, 0x53, 0xCE};

static uchar connection_tune[20] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x0A, 0x00,
                                    0x1E, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x3C, 0xCE};

static uchar connection_open_ok[13] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x0A, 0x00, 0x29, 0x00, 0xCE};

static uchar channel_open_ok[16] = {0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00,
                                    0x14, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x00, 0xCE};

static uchar exchange_declare_ok[12] = {0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x28, 0x00, 0x0B, 0xCE};

static uchar channel_close_ok[12] = {0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x14, 0x00, 0x29, 0xCE};

static uchar connection_close_ok[12] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x0A, 0x00, 0x33, 0xCE};

static uchar channel_close_ok_on_badexch[148] = {
    0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x8C, 0x00, 0x14, 0x00, 0x28, 0x01, 0x96, 0x81, 0x50, 0x52, 0x45, 0x43, 0x4F,
    0x4E, 0x44, 0x49, 0x54, 0x49, 0x4F, 0x4E, 0x5F, 0x46, 0x41, 0x49, 0x4C, 0x45, 0x44, 0x20, 0x2D, 0x20, 0x69, 0x6E,
    0x65, 0x71, 0x75, 0x69, 0x76, 0x61, 0x6C, 0x65, 0x6E, 0x74, 0x20, 0x61, 0x72, 0x67, 0x20, 0x27, 0x64, 0x75, 0x72,
    0x61, 0x62, 0x6C, 0x65, 0x27, 0x20, 0x66, 0x6F, 0x72, 0x20, 0x65, 0x78, 0x63, 0x68, 0x61, 0x6E, 0x67, 0x65, 0x20,
    0x27, 0x69, 0x6E, 0x27, 0x20, 0x69, 0x6E, 0x20, 0x76, 0x68, 0x6F, 0x73, 0x74, 0x20, 0x27, 0x2F, 0x6D, 0x65, 0x74,
    0x72, 0x6F, 0x6C, 0x6F, 0x67, 0x69, 0x65, 0x27, 0x3A, 0x20, 0x72, 0x65, 0x63, 0x65, 0x69, 0x76, 0x65, 0x64, 0x20,
    0x27, 0x66, 0x61, 0x6C, 0x73, 0x65, 0x27, 0x20, 0x62, 0x75, 0x74, 0x20, 0x63, 0x75, 0x72, 0x72, 0x65, 0x6E, 0x74,
    0x20, 0x69, 0x73, 0x20, 0x27, 0x74, 0x72, 0x75, 0x65, 0x27, 0x00, 0x28, 0x00, 0x0A, 0xCE};

typedef struct {
    uchar type;
    ushort ch;
    uint32_t method;
    uint16_t header_flags;
    size_t datalen;
    size_t framelen;
    uchar *data;
} amqp_frame_type_t;

#define DBGPRINTF0(f, ...)                                                                                   \
    if (debug > 0) {                                                                                         \
        struct timeval dbgtv;                                                                                \
        gettimeofday(&dbgtv, NULL);                                                                          \
        fprintf(stderr, "%02d.%03d " f, (int)(dbgtv.tv_sec % 60), (int)(dbgtv.tv_usec / 1000), __VA_ARGS__); \
    }
#define DBGPRINTF1(f, ...)                                                                                   \
    if (debug > 0) {                                                                                         \
        struct timeval dbgtv;                                                                                \
        gettimeofday(&dbgtv, NULL);                                                                          \
        dbgtv.tv_sec -= dbgtv_base.tv_sec;                                                                   \
        dbgtv.tv_usec -= dbgtv_base.tv_usec;                                                                 \
        if (dbgtv.tv_usec < 0) {                                                                             \
            dbgtv.tv_usec += 1000000;                                                                        \
            dbgtv.tv_sec--;                                                                                  \
        }                                                                                                    \
        fprintf(stderr, "%02d.%03d " f, (int)(dbgtv.tv_sec % 60), (int)(dbgtv.tv_usec / 1000), __VA_ARGS__); \
    }
#define DBGPRINTF2(f, ...)                                                                                   \
    if (debug == 2) {                                                                                        \
        struct timeval dbgtv;                                                                                \
        gettimeofday(&dbgtv, NULL);                                                                          \
        dbgtv.tv_sec -= dbgtv_base.tv_sec;                                                                   \
        dbgtv.tv_usec -= dbgtv_base.tv_usec;                                                                 \
        if (dbgtv.tv_usec < 0) {                                                                             \
            dbgtv.tv_usec += 1000000;                                                                        \
            dbgtv.tv_sec--;                                                                                  \
        }                                                                                                    \
        fprintf(stderr, "%02d.%03d " f, (int)(dbgtv.tv_sec % 60), (int)(dbgtv.tv_usec / 1000), __VA_ARGS__); \
    }

static struct timeval dbgtv_base;
static int server_behaviors = 0;
static int behaviors;
static int wait_after_accept = 200; /* milliseconds */
static char *outfile = NULL;
static int debug = 1;

FILE *fpout = NULL;

static ATTR_NORETURN void errout(const char *reason, int server) {
    char txt[256];
    snprintf(txt, 256, "%s server %d", reason, server);
    perror(txt);
    if (fpout && fpout != stdout) {
        fclose(fpout);
        fpout = NULL;
    }
    if (outfile) unlink(outfile);
    exit(1);
}

static ATTR_NORETURN void usage(void) {
    fprintf(stderr,
            "usage: minirmqsrvr -f outfile [-b behaviour] "
            "[-t keep_alive_max] [-w delay_after_fail] [-d]\n");
    exit(1);
}

/* Those three functions are "endianess" insensitive */
static uint16_t buf2uint16(uchar *b) {
    return ((uint16_t)b[0]) << 8 | ((uint16_t)b[1]);
}

static uint32_t buf2uint32(uchar *b) {
    return ((uint32_t)b[0]) << 24 | ((uint32_t)b[1]) << 16 | ((uint32_t)b[2]) << 8 | ((uint32_t)b[3]);
}

static uint64_t buf2uint64(uchar *b) {
    return ((uint64_t)b[0]) << 56 | ((uint64_t)b[1]) << 48 | ((uint64_t)b[2]) << 40 | ((uint64_t)b[3]) << 32 |
           ((uint64_t)b[4]) << 24 | ((uint64_t)b[5]) << 16 | ((uint64_t)b[6]) << 8 | ((uint64_t)b[7]);
}


static char AMQP091[8] = {'A', 'M', 'Q', 'P', 0x00, 0x00, 0x09, 0x01};

static int decode_frame_type(uchar *buf, amqp_frame_type_t *frame, size_t nread) {
    if (nread == 8) {
        if (memcmp(buf, AMQP091, sizeof(AMQP091))) return -1;
        frame->framelen = 8;
        frame->type = AMQP_STARTING;
        frame->ch = 0;
        return 0;
    }
    frame->type = buf[0];
    frame->ch = buf2uint16(buf + 1);
    frame->datalen = buf2uint32(buf + 3);
    frame->framelen = frame->datalen + 8;
    frame->method = buf2uint32(buf + 7);
    switch (frame->type) {
        case AMQP_FRAME_BODY:
            frame->data = buf + 7;
            break;
        default:
            frame->data = buf + 11;
    }
    return 0;
}

static ssize_t amqp_write(int fdc, uchar *buf, size_t blen, unsigned short channel) {
    buf[1] = (char)(channel >> 8);
    buf[2] = (char)(channel & 0xFF);
    return write(fdc, buf, blen);
}

static uchar *amqpFieldUint64(uint64_t *d, uchar *s) {
    *d = buf2uint64(s);
    return s + 8;
}

static uchar *amqpFieldUint32(uint32_t *d, uchar *s) {
    *d = buf2uint32(s);
    return s + 4;
}

static uchar *amqpFieldUint16(uint16_t *d, uchar *s) {
    *d = buf2uint16(s);
    return s + 2;
}

static uchar *amqpFieldLenFprintf(const char *pfx, uchar *s, uint32_t len) {
    if (fpout) fprintf(fpout, "%s%.*s", pfx, (int)len, (char *)s);
    return s + len;
}

static uchar *amqpFieldFprintf(const char *pfx, uchar *s) {
    uint32_t len = *s++;
    return amqpFieldLenFprintf(pfx, s, len);
}

static uchar *amqpHeaderFprintf(uchar *s, uint32_t *size) {
    uint32_t len;
    uchar *p = amqpFieldFprintf(", ", s);
    p++; /* value type */
    p = amqpFieldUint32(&len, p);
    *size -= (p - s) + len;
    return amqpFieldLenFprintf(":", p, len);
}

static void amqp_srvr(int port, int srvr, int fds, int piperead, int pipewrite) {
    uchar wrkBuf[8192], *p;
    size_t nRead = 0, bsize = 0;
    ssize_t nSent;
    amqp_frame_type_t frame;
    uint64_t body_ui64 = 0;
    uint32_t props_header_size;
    uint16_t props_flags;
    int my_behaviour;
    struct timeval tv;
    fd_set rfds;
    int nfds = ((piperead > fds) ? piperead : fds) + 1;

    int fdc;

    my_behaviour = behaviors & 0x000F;
    behaviors = behaviors >> 4; /* for next server */
    ;

    if (listen(fds, 0) != 0) errout("listen", port);

    DBGPRINTF1("Server AMQP %d on port %d started\n", srvr, port);

    tv.tv_sec = 120;
    tv.tv_usec = 0;
    FD_ZERO(&rfds);
    FD_SET(fds, &rfds);
    if (piperead > 0) FD_SET(piperead, &rfds);

    if (select(nfds, &rfds, NULL, NULL, &tv) == 0) {
        exit(1);
    }

    if (piperead > 0 && FD_ISSET(piperead, &rfds)) {
        char c;
        int l = read(piperead, &c, 1);
        if (l == 1) {
            my_behaviour = behaviors & 0x000F;
            if (my_behaviour != 0) {
                DBGPRINTF1("Server AMQP %d on port %d switch behaviour", srvr, port);
            } else {
                DBGPRINTF1("Server AMQP %d on port %d leaving", srvr, port);
                if (fpout && fpout != stdout) {
                    fclose(fpout);
                    fpout = NULL;
                }
                exit(1);
            }
        }
    }

    fdc = accept(fds, NULL, NULL);

    if (pipewrite > 0) nSent = write(pipewrite, "N", 1);

    close(fds);
    fds = -1;

    /* this let the os understand that the port is closed */
    usleep(1000 * wait_after_accept);

    frame.type = AMQP_STARTING;

    while (fdc > 0) {
        nSent = 0;
        ssize_t rd = 0;
        if (nRead < 12) {
            rd = read(fdc, wrkBuf + nRead, sizeof(wrkBuf) - nRead);
            if (rd <= 0) {
                DBGPRINTF1("Server AMQP %d on port %d disconnected\n", srvr, port);
                close(fdc);
                fdc = 0;
                break;
            } else {
                nRead += (size_t)rd;
            }
        }

        if (decode_frame_type(wrkBuf, &frame, nRead)) {
            DBGPRINTF1("Server AMQP %d on port %d killed : bad protocol\n", srvr, port);
            close(fdc);
            fdc = 0;
            break;
        }
        if (rd > 4) DBGPRINTF2("Server received : %zd\n", rd);

        switch (frame.type) {
            case AMQP_STARTING: /* starting handshake */

                DBGPRINTF1("Server AMQP %d on port %d type %d connected\n", srvr, port, my_behaviour);
                DBGPRINTF2("Server %d connection.start\n", srvr);
                nSent = amqp_write(fdc, connection_start, sizeof(connection_start), frame.ch);
                break;

            case AMQP_FRAME_METHOD:

                DBGPRINTF2("Server %d method : 0x%X\n", srvr, frame.method);

                switch (frame.method) {
                    case AMQP_CONNECTION_START_OK_METHOD:

                        DBGPRINTF2("Server %d connection.tune\n", srvr);
                        nSent = amqp_write(fdc, connection_tune, sizeof(connection_tune), frame.ch);
                        break;

                    case AMQP_CONNECTION_TUNE_OK_METHOD:

                        DBGPRINTF2("Client %d connection.tune-ok\n", srvr);
                        nSent = 0;
                        break;

                    case AMQP_CONNECTION_OPEN_METHOD:

                        nSent = amqp_write(fdc, connection_open_ok, sizeof(connection_open_ok), frame.ch);
                        DBGPRINTF2("Server %d connection.open\n", srvr);
                        break;

                    case AMQP_CHANNEL_OPEN_METHOD:

                        nSent = amqp_write(fdc, channel_open_ok, sizeof(channel_open_ok), frame.ch);
                        DBGPRINTF2("Server %d channel.open\n", srvr);
                        if (my_behaviour == AMQP_BEHAVIOR_NOEXCH) {
                            close(fdc);
                            DBGPRINTF1("Server AMQP %d on port %d stopped\n", srvr, port);
                            fdc = 0;
                            frame.type = 0;
                        }
                        break;

                    case AMQP_EXCHANGE_DECLARE_METHOD:

                        if (my_behaviour == AMQP_BEHAVIOR_BADEXCH) {
                            nSent = amqp_write(fdc, channel_close_ok_on_badexch, sizeof(channel_close_ok_on_badexch),
                                               frame.ch);
                        } else {
                            nSent = amqp_write(fdc, exchange_declare_ok, sizeof(exchange_declare_ok), frame.ch);
                        }
                        DBGPRINTF2("Server %d exchange.declare\n", srvr);
                        if (my_behaviour == AMQP_BEHAVIOR_DECEXCH) {
                            close(fdc);
                            DBGPRINTF1("Server AMQP %d on port %d stopped\n", srvr, port);
                            fdc = 0;
                            frame.type = 0;
                        }
                        break;

                    case AMQP_CHANNEL_CLOSE_METHOD:

                        nSent = amqp_write(fdc, channel_close_ok, sizeof(channel_close_ok), frame.ch);
                        DBGPRINTF2("Server %d channel.close\n", srvr);
                        break;

                    case AMQP_CONNECTION_CLOSE_METHOD:

                        nSent = amqp_write(fdc, connection_close_ok, sizeof(connection_close_ok), frame.ch);
                        DBGPRINTF2("Server %d connection.close\n", srvr);
                        break;

                    case AMQP_BASIC_PUBLISH_METHOD:

                        p = amqpFieldFprintf("Exchange:", frame.data + 2);
                        amqpFieldFprintf(", routing-key:", p);
                        break;

                    default:

                        nSent = 0;
                }
                break;

            case AMQP_FRAME_HEADER:

                DBGPRINTF2("Server %d HEADERS\n", srvr);
                p = amqpFieldUint64(&body_ui64, frame.data);
                bsize = (size_t)body_ui64;
                p = amqpFieldUint16(&props_flags, p);
                if (props_flags & AMQP_BASIC_CONTENT_TYPE_FLAG) {
                    p = amqpFieldFprintf(", content-type:", p);
                }
                if (props_flags & AMQP_BASIC_HEADERS_FLAG) {
                    p = amqpFieldUint32(&props_header_size, p);
                    while (props_header_size) {
                        p = amqpHeaderFprintf(p, &props_header_size);
                    }
                }
                if (props_flags & AMQP_BASIC_DELIVERY_MODE_FLAG) {
                    if (fpout) fprintf(fpout, ", delivery-mode:%s", (*p++) ? "transient" : "persistent");
                }
                if (props_flags & AMQP_BASIC_EXPIRATION_FLAG) {
                    p = amqpFieldFprintf(", expiration:", p);
                }
                if (props_flags & AMQP_BASIC_TIMESTAMP_FLAG) {
                    if (fpout) fprintf(fpout, ", timestamp:OK");
                    p += sizeof(uint64_t);
                }
                if (props_flags & AMQP_BASIC_APP_ID_FLAG) {
                    amqpFieldFprintf(", app-id:", p);
                }
                if (fpout) fprintf(fpout, ", msg:");
                break;

            case AMQP_FRAME_BODY:

                DBGPRINTF2("Server %d Body size left : %zu, received : %zu\n", srvr, bsize, frame.datalen);
                bsize -= frame.datalen;
                if (fpout) {
                    fprintf(fpout, "%.*s", (int)frame.datalen, frame.data);
                    if (frame.data[frame.datalen - 1] != '\n') fprintf(fpout, "\n");
                    fflush(fpout);
                }
                break;

            default:

                DBGPRINTF1("Server %d unsupported frame type %d\n", srvr, frame.type);
                close(fdc);
                fdc = 0;
                frame.type = 0;
                frame.framelen = 0;
        } /* switch (frame.type) */

        nRead -= frame.framelen;
        if (nRead > 0) memmove(wrkBuf, wrkBuf + frame.framelen, nRead);

        if (nSent < 0) {
            close(fdc);
            fdc = 0;
        }
    } /* while(fdc) */
    DBGPRINTF2("Leaving thread %d\n", srvr);
}

int main(int argc, char *argv[]) {
    int port[2], fds[2], i, opt, nb_port = 1;
    int pipeS1toS2[2] = {-1, -1};
    int pipeS2toS1[2] = {-1, -1};
    int pipeRead[2], pipeWrite[2];


    struct sockaddr_in srvAddr[2];
    unsigned int addrLen = sizeof(struct sockaddr_in), len;
    pid_t pid[2];

    fpout = stdout;

    while ((opt = getopt(argc, argv, "f:b:w:d")) != -1) {
        switch (opt) {
            case 'w':
                wait_after_accept = atoi(optarg);
                break;
            case 'd':
                debug = 2;
                break;
            case 'b':
                server_behaviors = atoi(optarg);
                break;
            case 'f':
                if (strcmp(optarg, "-")) {
                    outfile = optarg;
                    fpout = fopen(optarg, "w");
                    if (fpout == NULL) {
                        fprintf(stderr, "file %s could not be created\n", outfile);
                        exit(1);
                    }
                }
                break;
            default:
                fprintf(stderr, "invalid option '%c' or value missing - terminating...\n", opt);
                usage();
                break;
        }
    }

    switch (server_behaviors) {
        case 0:
            behaviors = AMQP_BEHAVIOR_STANDARD;
            nb_port = 1;
            break;
        case 1: /* two standard servers get message successfully */
            behaviors = AMQP_BEHAVIOR_STANDARD;
            nb_port = 2;
            break;
        case 2: /* 2 servers first server which disconnect after after open channel : no declare exchange */
            behaviors = AMQP_BEHAVIOR_NOEXCH | AMQP_BEHAVIOR_STANDARD << 4;
            nb_port = 2;
            break;
        case 3: /* 2 servers first server which disconnect after declare exchange*/
            behaviors = AMQP_BEHAVIOR_DECEXCH | AMQP_BEHAVIOR_STANDARD << 4;
            nb_port = 2;
            break;
        case 4: /* one server with bad exchange declare */
            behaviors = AMQP_BEHAVIOR_BADEXCH;
            nb_port = 1;
            break;
        default:
            fprintf(stderr, "Invalid behavior");
            exit(1);
    }

    gettimeofday(&dbgtv_base, NULL);

    port[0] = port[1] = -1;

    if (nb_port == 2) {
        if (pipe(pipeS1toS2) == -1 || pipe(pipeS2toS1) == -1) {
            fprintf(stderr, "Pipe failed !");
            exit(1);
        }
    }

    pipeRead[0] = pipeS2toS1[0];
    pipeWrite[0] = pipeS1toS2[1];

    pipeRead[1] = pipeS1toS2[0];
    pipeWrite[1] = pipeS2toS1[1];

    for (i = 0; i < nb_port; i++) {
        fds[i] = socket(AF_INET, SOCK_STREAM, 0);
        srvAddr[i].sin_family = AF_INET;
        srvAddr[i].sin_addr.s_addr = INADDR_ANY;
        srvAddr[i].sin_port = 0;
        if (bind(fds[i], (struct sockaddr *)&srvAddr[i], addrLen) != 0) errout("bind", 1);
        len = addrLen;
        if (getsockname(fds[i], (struct sockaddr *)&srvAddr[i], &len) == -1) errout("bind", i + 1);
        if ((port[i] = ntohs(srvAddr[i].sin_port)) <= 0) errout("get port", i + 1);
    }

    for (i = 0; i < nb_port; i++) {
        if ((pid[i] = fork()) == -1) {
            fprintf(stderr, "Fork failed !");
            exit(1);
        }
        if (pid[i] == 0) {
            /* this is the child */
            if (fds[1 - i] > 0) close(fds[1 - i]);
            amqp_srvr(port[i], i + 1, fds[i], pipeRead[i], pipeWrite[i]);

            if (fpout && fpout != stdout) fclose(fpout);

            DBGPRINTF2("%s\n", "Leaving server");
            return 0;
        }
    }

    if (nb_port == 2)
        printf("export AMQPSRVRPID1=%ld AMQPSRVRPID2=%ld PORT_AMQP1=%d PORT_AMQP2=%d", (long)pid[0], (long)pid[1],
               port[0], port[1]);
    else
        printf("export AMQPSRVRPID1=%ld PORT_AMQP1=%d", (long)pid[0], port[0]);
    return 0;
}
