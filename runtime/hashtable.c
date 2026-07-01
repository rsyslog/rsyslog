/* Copyright 2026 Adiscon GmbH.
 * Copyright (C) 2004 Christopher Clark <firstname.lastname@cl.cam.ac.uk>
 *
 * SPDX-License-Identifier: Apache-2.0 AND BSD-3-Clause
 */
#include "config.h"
#include "rshash.h"
#include <string.h>

#if defined(__clang__)
    #pragma GCC diagnostic ignored "-Wunknown-attributes"
#endif
unsigned __attribute__((nonnull(1))) int
#if defined(__clang__)
    __attribute__((no_sanitize("unsigned-integer-overflow")))
#endif
    hash_from_string(void *k) {
    char *rkey = (char *)k;
    unsigned hashval = 1;

    while (*rkey) hashval = hashval * 33 + *rkey++;

    return hashval;
}

int key_equals_string(void *key1, void *key2) {
    return !strcmp(key1, key2);
}
