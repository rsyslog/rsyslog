/* SPDX-License-Identifier: Apache-2.0 */
/* This is a quick and dirty "tail implementation", one which always
 * skips the first line, but nothing else. I have done this to prevent
 * the various incompatible options of tail come into my way. One could
 * probably work around this by using autoconf magic, but for me it
 * was much quicker writing this small C program, which really should
 * be portable across all platforms.
 *
 * Part of the testbench for rsyslog.
 *
 * Copyright 2009 Rainer Gerhards and Adiscon GmbH.
 *
 * Licensed under the Apache License, Version 2.0. See COPYING.ASL20.
 */
#include "config.h"
#include <stdio.h>

int main(int __attribute__((unused)) argc, char __attribute__((unused)) * argv[]) {
    int c;

    for (c = getchar(); c != EOF && c != '\n'; c = getchar()) /*skip to newline*/
        ;

    if (c == '\n') c = getchar();

    for (; c != EOF; c = getchar()) putchar(c);

    return 0;
}
