/* Copyright 2026 Adiscon GmbH.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "config.h"
#include "hashtable_private.h"
#include "rshash.h"
#include <stdlib.h>
#include <string.h>

struct hashtable *create_hashtable(unsigned int minsize,
                                   unsigned int (*hashf)(void *),
                                   int (*eqf)(void *, void *),
                                   void (*dest)(void *)) {
    return rshash_create(minsize, hashf, eqf, free, dest);
}

int hashtable_insert(struct hashtable *h, void *k, void *v) {
    return rshash_put(h, k, v);
}

void *hashtable_search(struct hashtable *h, void *k) {
    return rshash_find(h, k);
}

void *hashtable_remove(struct hashtable *h, void *k) {
    return rshash_remove(h, k);
}

unsigned int hashtable_count(struct hashtable *h) {
    return rshash_count(h);
}

void hashtable_destroy(struct hashtable *h, int free_values) {
    rshash_destroy(h, free_values);
}

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
