/* generate an input file suitable for use by the testbench
 * Copyright (C) 2018-2022 by Rainer Gerhards and Adiscon GmbH.
 * Copyright (C) 2016-2018 by Pascal Withopf and Adiscon GmbH.
 *
 * usage: ./inputfilegen num-lines > file
 * -m number of messages
 * -i start number of message
 * -d extra data to add (all 'X' after the msgnum)
 * -s size of file to generate
 *  cannot be used together with -m
 *  size may be slightly smaller in order to be able to write
 *  the last complete line.
 * -M "message count file" - contains number of messages to be written
 *  This is especially useful with -s, as the testbench otherwise does
 *  not know how to do a seq_check. To keep things flexible, can also be
 *  used with -m (this may aid testbench framework generalization).
 * -f outputfile
 *  Permits to write data to file "outputfile" instead of stdout. Also
 *  enables support for SIGHUP.
 * -S sleep time
 *  ms to sleep between sending message bulks (bulks size given by -B)
 * -B number of messages in bulk
 *  number of messages to send without sleeping as specified in -S.
 *  IGNORED IF -S is not also given!
 * Part of rsyslog, licensed under ASL 2.0
 */
#include "config.h"
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#if defined(_AIX)
    #include <unistd.h>
#else
    #include <getopt.h>
#endif
#if defined(__FreeBSD__)
    #include <sys/time.h>
#else
    #include <time.h>
#endif
#if defined(HAVE_SYS_SELECT_H)
    #include <sys/select.h>
#endif

#define DEFMSGS 5
#define NOEXTRADATA -1

static volatile int bHadHUP = 0;
static void hdlr_sighup(int sig) {
    fprintf(stderr, "inputfilegen: had hup, sig %d\n", sig);
    bHadHUP = 1;
}
static void sighup_enable(void) {
    struct sigaction sigAct;
    memset(&sigAct, 0, sizeof(sigAct));
    sigemptyset(&sigAct.sa_mask);
    sigAct.sa_handler = hdlr_sighup;
    sigaction(SIGHUP, &sigAct, NULL);
}

void msleep(const int sleepTime) {
    struct timeval tvSelectTimeout;

    tvSelectTimeout.tv_sec = sleepTime / 1000;
    tvSelectTimeout.tv_usec = (sleepTime % 1000) * 1000; /* micro seconds */
    if (select(0, NULL, NULL, NULL, &tvSelectTimeout) == -1) {
        if (errno != EINTR) {
            perror("select");
            exit(1);
        }
    }
}

static FILE *open_output(const char *fn) {
    FILE *fh_output = fopen(fn, "w");
    if (fh_output == NULL) {
        perror(fn);
        exit(1);
    }
    return fh_output;
}

int main(int argc, char *argv[]) {
    int c, i;
    long long nmsgs = DEFMSGS;
    long long nmsgstart = 0;
    int nfinishedidle = 0;
    int nchars = NOEXTRADATA;
    int errflg = 0;
    long long filesize = -1;
    char *extradata = NULL;
    const char *msgcntfile = NULL;
    const char *outputfile = "-";
    FILE *fh_output;
    int sleep_ms = 0; /* How long to sleep between message writes */
    int sleep_msgs = 0; /* messages to xmit between sleeps (if configured) */
    int sleep_hubreopen_ms = 5; /* Wait until new file is being reopened, logrotate may need some time */
    int ctr = 0;

    while ((c = getopt(argc, argv, "m:M:i:I:h:d:s:f:S:B:")) != -1) {
        switch (c) {
            case 'm':
                nmsgs = atoi(optarg);
                break;
            case 'M':
                msgcntfile = optarg;
                break;
            case 'i':
                nmsgstart = atoi(optarg);
                break;
            case 'I':
                nfinishedidle = atoi(optarg);
                break;
            case 'd':
                nchars = atoi(optarg);
                break;
            case 's':
                filesize = atoll(optarg);
                break;
            case 'S':
                sleep_ms = atoi(optarg);
                break;
            case 'B':
                sleep_msgs = atoi(optarg);
                break;
            case 'h':
                sleep_hubreopen_ms = atoi(optarg);
                break;
            case 'f':
                outputfile = optarg;
                sighup_enable();
                break;
            case ':':
                fprintf(stderr, "Option -%c requires an operand\n", optopt);
                errflg++;
                break;
            case '?':
                fprintf(stderr, "inputfilegen: Unrecognized option: -%c\n", optopt);
                errflg++;
                break;
        }
    }
    if (errflg) {
        fprintf(stderr, "invalid call\n");
        exit(2);
    }

    if (filesize != -1) {
        const int linesize = (17 + nchars); /* 17 is std line size! */
        nmsgs = filesize / linesize;
        fprintf(stderr,
                "inputfilegen: file size requested %lld, actual %lld with "
                "%lld lines, %lld bytes less\n",
                filesize, nmsgs * linesize, nmsgs, filesize - nmsgs * linesize);
        if (nmsgs > 100000000) {
            fprintf(stderr,
                    "inputfilegen: number of lines exhaust 8-digit line numbers "
                    "which are standard inside the testbench.\n"
                    "Use -d switch to add extra data (e.g. -d111 for "
                    "128 byte lines or -d47 for 64 byte lines)\n");
            exit(1);
        }
    }

    if (msgcntfile != NULL) {
        FILE *const fh = fopen(msgcntfile, "w");
        if (fh == NULL) {
            perror(msgcntfile);
            exit(1);
        }
        fprintf(fh, "%lld", nmsgs);
        fclose(fh);
    }

    if (strcmp(outputfile, "-")) {
        fh_output = open_output(outputfile);
    } else {
        fh_output = stdout;
    }

    if (nchars != NOEXTRADATA) {
        extradata = (char *)malloc(nchars + 1);
        memset(extradata, 'X', nchars);
        extradata[nchars] = '\0';
    }
    for (i = nmsgstart; i < (nmsgs + nmsgstart); ++i) {
        if (sleep_ms > 0 && ctr++ >= sleep_msgs) {
            msleep(sleep_ms);
            ctr = 0;
        }
        if (bHadHUP) {
            fclose(fh_output);
            if (sleep_hubreopen_ms > 0) {
                /* Extra Sleep so logrotate or whatever can take place */
                msleep(sleep_hubreopen_ms);
            }
            fh_output = open_output(outputfile);
            fprintf(stderr, "inputfilegen: %s reopened\n", outputfile);
            fprintf(stderr, "inputfilegen: current message number %d\n", i);
            bHadHUP = 0;
        }
        fprintf(fh_output, "msgnum:%8.8d:", i);
        if (nchars != NOEXTRADATA) {
            fprintf(fh_output, "%s", extradata);
        }
        fprintf(fh_output, "\n");
    }
    if (nfinishedidle > 0) {
        /* Keep process open for nfinishedidle ms */
        for (int x = 0; x < nfinishedidle; ++x) {
            if (bHadHUP) {
                /* Create empty file */
                fh_output = open_output(outputfile);
                fclose(fh_output);
                fprintf(stderr, "inputfilegen: last message number was %d\n", i);
                bHadHUP = 0;
            }
            msleep(1);
        }
    }

    free(extradata);
    return 0;
}
