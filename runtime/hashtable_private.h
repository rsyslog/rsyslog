/* Copyright 2026 Adiscon GmbH.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef INCLUDED_HASHTABLE_PRIVATE_H
#define INCLUDED_HASHTABLE_PRIVATE_H

#include "rshash.h"
#include "atomic.h"
#include <stdint.h>
#include <stdlib.h>

/*****************************************************************************/
struct entry {
    void *k, *v;
    unsigned int h;
    /*
     * Low bit is used as the logical deletion mark by the lock-free point
     * operation core. malloc() returns sufficiently aligned storage for this.
     */
    struct entry *next;
    struct entry *retired_next;
    int retired;
    int retired_free_value;
    int state;
};

#define ENTRY_LIVE 0
#define ENTRY_UPDATING 1
#define ENTRY_REMOVED 2

struct rshash_table {
    unsigned int tablelength;
    struct entry **table;
    struct rshash_table *next;
};

struct hashtable {
    unsigned int entrycount;
    unsigned int (*hashfn)(void *k);
    int (*eqfn)(void *k1, void *k2);
    void (*key_dest)(void *k); /* destructor for keys, if NULL use free() */
    void (*dest)(void *v); /* destructor for values, if NULL use free() */
    struct rshash_table *tables;
    unsigned int active_ops;
    struct entry *retired;
    DEF_ATOMIC_HELPER_MUT(mutTable);
    DEF_ATOMIC_HELPER_MUT(mutTables);
    DEF_ATOMIC_HELPER_MUT(mutCount);
    DEF_ATOMIC_HELPER_MUT(mutActive);
    DEF_ATOMIC_HELPER_MUT(mutRetired);
};

/*****************************************************************************/
unsigned int hash(rshash_t *h, void *k);

#ifdef RSHASH_TEST_HOOKS
typedef void (*rshash_test_hook_fn)(rshash_t *h, void *usr);

void rshash_test_reset_hooks(void);
void rshash_test_set_after_mark_hook(rshash_test_hook_fn hook, void *usr);
void rshash_test_set_after_retired_detach_hook(rshash_test_hook_fn hook, void *usr);
void rshash_test_fail_next_entry_alloc(void);
void rshash_test_fail_next_table_alloc(void);
void rshash_test_fail_next_table_bucket_alloc(void);
void rshash_test_fail_next_unlink_cas(void);
int rshash_test_mark_removed_for_key(rshash_t *h, void *key);
#endif

/*****************************************************************************/
/* indexFor */
#define indexFor(tablelength, hashvalue) ((hashvalue) % (tablelength))


/*****************************************************************************/
#define freekey(X) free(X)

static inline struct entry *entry_unmark(struct entry *e) {
    return (struct entry *)((uintptr_t)e & ~(uintptr_t)1);
}

static inline struct entry *entry_mark(struct entry *e) {
    return (struct entry *)((uintptr_t)e | (uintptr_t)1);
}

static inline int entry_is_marked(struct entry *e) {
    return ((uintptr_t)e & (uintptr_t)1) != 0;
}


/*****************************************************************************/

#endif
