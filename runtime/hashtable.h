/* Copyright 2026 Adiscon GmbH.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef INCLUDED_HASHTABLE_COMPAT_H
#define INCLUDED_HASHTABLE_COMPAT_H

struct hashtable;

struct hashtable *create_hashtable(unsigned int minsize,
                                   unsigned int (*hashf)(void *),
                                   int (*eqf)(void *, void *),
                                   void (*dest)(void *));
int hashtable_insert(struct hashtable *h, void *k, void *v);
void *hashtable_search(struct hashtable *h, void *k);
void *hashtable_remove(struct hashtable *h, void *k);
unsigned int hashtable_count(struct hashtable *h);
void hashtable_destroy(struct hashtable *h, int free_values);

unsigned int hash_from_string(void *k);
int key_equals_string(void *key1, void *key2);

#define DEFINE_HASHTABLE_INSERT(fnname, keytype, valuetype)     \
    int fnname(struct hashtable *h, keytype *k, valuetype *v) { \
        return hashtable_insert(h, (void *)k, (void *)v);       \
    }

#define DEFINE_HASHTABLE_SEARCH(fnname, keytype, valuetype) \
    valuetype *fnname(struct hashtable *h, keytype *k) {    \
        return (valuetype *)hashtable_search(h, (void *)k); \
    }

#define DEFINE_HASHTABLE_REMOVE(fnname, keytype, valuetype) \
    valuetype *fnname(struct hashtable *h, keytype *k) {    \
        return (valuetype *)hashtable_remove(h, (void *)k); \
    }

#define DEFINE_HASHTABLE_COUNT(fnname)         \
    unsigned int fnname(struct hashtable *h) { \
        return hashtable_count(h);             \
    }

#endif
