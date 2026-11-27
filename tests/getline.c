/* SPDX-License-Identifier: Apache-2.0 */
/* getline() replacement for platforms that do not have it.
 *
 * Part of the testbench for rsyslog.
 *
 * Copyright 2009 Rainer Gerhards and Adiscon GmbH.
 *
 * Developed by Adiscon GmbH for the rsyslog testbench.
 * Licensed under the Apache License, Version 2.0. See COPYING.ASL20.
 */
#include "config.h"
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>

/* we emulate getline (the dirty way) if we do not have it
 * We do not try very hard, as this is just a test driver.
 * rgerhards, 2009-03-31
 */
#ifndef HAVE_GETLINE
ssize_t getline(char **lineptr, size_t *n, FILE *fp) {
    int c;
    int len = 0;

    if (*lineptr == NULL) *lineptr = malloc(4096); /* quick and dirty! */

    c = fgetc(fp);
    while (c != EOF && c != '\n') {
        (*lineptr)[len++] = c;
        c = fgetc(fp);
    }
    if (c != EOF) /* need to add NL? */
        (*lineptr)[len++] = c;

    (*lineptr)[len] = '\0';

    *n = len;
    // printf("getline returns: '%s'\n", *lineptr);

    return (len > 0) ? len : -1;
}
#endif /* #ifndef HAVE_GETLINE */
