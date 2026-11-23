/* SPDX-License-Identifier: Apache-2.0 */
/* sleeps for the specified number of MILLIseconds.
 * Primarily meant as a portable tool available everywhere for the
 * testbench (sleep 0.1 does not work on all platforms).
 *
 * Part of the testbench for rsyslog.
 *
 * Copyright 2010 Rainer Gerhards and Adiscon GmbH.
 *
 * Licensed under the Apache License, Version 2.0. See COPYING.ASL20.
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#if defined(__FreeBSD__)
    #include <sys/time.h>
#else
    #include <time.h>
#endif
#if defined(HAVE_SYS_SELECT_H)
    #include <sys/select.h>
#endif

int main(int argc, char *argv[]) {
    struct timeval tvSelectTimeout;
    long sleepTime;

    if (argc != 2) {
        fprintf(stderr, "usage: msleep <milliseconds>\n");
        return 1;
    }

    sleepTime = atoi(argv[1]);
    tvSelectTimeout.tv_sec = sleepTime / 1000;
    tvSelectTimeout.tv_usec = (sleepTime % 1000) * 1000; /* micro seconds */
    if (select(0, NULL, NULL, NULL, &tvSelectTimeout) == -1) {
        perror("select");
        return 1;
    }

    return 0;
}
