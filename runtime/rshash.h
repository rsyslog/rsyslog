/* Copyright 2026 Adiscon GmbH.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef INCLUDED_RSHASH_H
#define INCLUDED_RSHASH_H

struct hashtable;
typedef struct hashtable rshash_t;

typedef unsigned int (*rshash_hash_fn)(void *key);
typedef int (*rshash_key_eq_fn)(void *key1, void *key2);
typedef void (*rshash_destruct_fn)(void *ptr);
typedef int (*rshash_scan_fn)(void *key, void *value, void *usr);
typedef int (*rshash_scan_remove_pred_fn)(void *key, void *value, void *usr);
typedef void (*rshash_scan_removed_fn)(void *key, void *value, void *usr);

rshash_t *rshash_create(unsigned int minsize,
                        rshash_hash_fn hashfn,
                        rshash_key_eq_fn eqfn,
                        rshash_destruct_fn key_destructor,
                        rshash_destruct_fn value_destructor);
int rshash_put(rshash_t *h, void *key, void *value);
/* On duplicate rejection, the supplied key/value pair is destroyed. */
int rshash_put_unique(rshash_t *h, void *key, void *value);
int rshash_replace(rshash_t *h, void *key, void *value, void **old_value);
void *rshash_find(rshash_t *h, void *key);
void *rshash_remove(rshash_t *h, void *key);
unsigned int rshash_count(rshash_t *h);
/*
 * Weakly consistent traversal: safe with concurrent point operations, but not
 * a snapshot. The callback must return non-zero to continue scanning.
 */
void rshash_scan(rshash_t *h, rshash_scan_fn cb, void *usr);
unsigned int rshash_scan_remove_if(rshash_t *h,
                                   rshash_scan_remove_pred_fn pred,
                                   rshash_scan_removed_fn removed,
                                   void *usr);
void rshash_destroy(rshash_t *h, int free_values);

unsigned int hash_from_string(void *key);
int key_equals_string(void *key1, void *key2);

#endif
