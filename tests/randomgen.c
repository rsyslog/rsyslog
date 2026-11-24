/* SPDX-License-Identifier: Apache-2.0 */
/* generates random data for later use in test cases. Of course,
 * we could generate random data during the testcase itself, but
 * the core idea is that we record the random data so that we have
 * a chance to reproduce a problem should it occur. IMHO this
 * provides the best compromise, by a) having randomness but
 * b) knowing what was used during the test.
 *
 * Params
 * -f	output file name (stdout if not given)
 * -s	size of test data, plain number is size in k, 1MB default
 * -u   uses /dev/urandom instead of libc random number generator
 *      (when available). Note that this is usually much slower.
 *
 * Part of the testbench for rsyslog.
 *
 * Copyright 2010-2016 Rainer Gerhards and Adiscon GmbH.
 *
 * Licensed under the Apache License, Version 2.0. See COPYING.ASL20.
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>

#define EXIT_FAILURE 1

static char *fileName = NULL; /* name of output file */
static int tryUseURandom = 0; /* try to use /dev/urandom? */
static long long fileSize = 1024 * 1024; /* file size in K, 1MB default */


/* generate the random file. This code really can be improved (e.g. read /dev/urandom
 * when available)
 */
static int genFile(void) {
    long i;
    FILE *fp = NULL;
    FILE *rfp = NULL;
    int ret = 0;

    if (fileName == NULL) {
        fp = stdout;
    } else {
        if ((fp = fopen(fileName, "w")) == NULL) {
            perror(fileName);
            return 1;
        }
    }

    /* try to use /dev/urandom, if available */
    if (tryUseURandom) rfp = fopen("/dev/urandom", "r");

    if (rfp == NULL) {
        /* fallback, use libc random number generator */
        for (i = 0; i < fileSize; ++i) {
            if (fputc((char)rand(), fp) == EOF) {
                perror(fileName);
                ret = 1;
                break;
            }
        }
    } else {
        /* use /dev/urandom */
        printf("using /dev/urandom");
        for (i = 0; i < fileSize; ++i) {
            if (fputc(fgetc(rfp), fp) == EOF) {
                perror(fileName);
                ret = 1;
                break;
            }
        }
    }

    if (rfp != NULL) fclose(rfp);
    if (fileName != NULL && fclose(fp) != 0) {
        perror(fileName);
        ret = 1;
    }
    if (fileName == NULL && fflush(fp) != 0) {
        perror("stdout");
        ret = 1;
    }
    return ret;
}


/* Run the test.
 * rgerhards, 2009-04-03
 */
int main(int argc, char *argv[]) {
    int ret = 0;
    int opt;

    srand(time(NULL)); /* seed is good enough for our needs */

    while ((opt = getopt(argc, argv, "f:s:u")) != -1) {
        switch (opt) {
            case 'f':
                fileName = optarg;
                break;
            case 's':
                fileSize = atol(optarg) * 1024;
                break;
            case 'u':
                tryUseURandom = 1;
                break;
            default:
                printf("invalid option '%c' or value missing - terminating...\n", opt);
                return 1;
        }
    }

    printf("generating random data file '%s' of %ldkb - may take a short while...\n", fileName,
           (long)(fileSize / 1024));
    ret = genFile();
    return ret;
}
