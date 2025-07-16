/* getline() replacement for platforms that do not have it.
 *
 * Part of the testbench for rsyslog.
 *
 * Copyright 2009 Rainer Gerhards and Adiscon GmbH.
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
