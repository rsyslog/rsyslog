/* Copyright 2026 Adiscon GmbH.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "config.h"
#include "hashtable_private.h"
#include "rshash.h"
#include <stdlib.h>
#include <string.h>

/*
 * Concurrency & Locking
 *
 * Point operations use atomic bucket-chain updates when atomic builtins are
 * available. On older platforms the rsyslog atomic helper macros provide the
 * mutex-backed fallback, so callers use the same API with weaker performance
 * but the same ownership rules.
 *
 * Removed nodes are first marked and unlinked, then placed on a retired list.
 * rshash_enter()/rshash_leave() are a table-wide quiescence guard: reclamation
 * runs only when no point operation or scan is active. This is intentionally
 * simpler than per-thread hazard pointers because rsyslog needs old-platform
 * support and most tables are small operational registries.
 */
static const unsigned int primes[] = {53,        97,        193,       389,       769,       1543,     3079,
                                      6151,      12289,     24593,     49157,     98317,     196613,   393241,
                                      786433,    1572869,   3145739,   6291469,   12582917,  25165843, 50331653,
                                      100663319, 201326611, 402653189, 805306457, 1610612741};
static const unsigned int prime_table_length = sizeof(primes) / sizeof(primes[0]);

#ifdef RSHASH_TEST_HOOKS
/*
 * White-box tests include this file directly with RSHASH_TEST_HOOKS enabled.
 * The production librsyslog build never defines it, so these deterministic
 * race/failure hooks do not add state or branches to exported runtime objects.
 */
static struct {
    rshash_test_hook_fn after_mark;
    void *after_mark_usr;
    rshash_test_hook_fn after_retired_detach;
    void *after_retired_detach_usr;
    int fail_next_entry_alloc;
    int fail_next_table_alloc;
    int fail_next_table_bucket_alloc;
    int fail_next_unlink_cas;
} rshash_test_hooks;

void rshash_test_reset_hooks(void) {
    memset(&rshash_test_hooks, 0, sizeof(rshash_test_hooks));
}

void rshash_test_set_after_mark_hook(rshash_test_hook_fn hook, void *usr) {
    rshash_test_hooks.after_mark = hook;
    rshash_test_hooks.after_mark_usr = usr;
}

void rshash_test_set_after_retired_detach_hook(rshash_test_hook_fn hook, void *usr) {
    rshash_test_hooks.after_retired_detach = hook;
    rshash_test_hooks.after_retired_detach_usr = usr;
}

void rshash_test_fail_next_entry_alloc(void) {
    rshash_test_hooks.fail_next_entry_alloc = 1;
}

void rshash_test_fail_next_table_alloc(void) {
    rshash_test_hooks.fail_next_table_alloc = 1;
}

void rshash_test_fail_next_table_bucket_alloc(void) {
    rshash_test_hooks.fail_next_table_bucket_alloc = 1;
}

void rshash_test_fail_next_unlink_cas(void) {
    rshash_test_hooks.fail_next_unlink_cas = 1;
}

static void *rshash_alloc_entry(size_t size) {
    if (rshash_test_hooks.fail_next_entry_alloc) {
        rshash_test_hooks.fail_next_entry_alloc = 0;
        return NULL;
    }
    return malloc(size);
}

static void *rshash_alloc_table_struct(size_t size) {
    if (rshash_test_hooks.fail_next_table_alloc) {
        rshash_test_hooks.fail_next_table_alloc = 0;
        return NULL;
    }
    return malloc(size);
}

static void *rshash_alloc_table_buckets(size_t nmemb, size_t size) {
    if (rshash_test_hooks.fail_next_table_bucket_alloc) {
        rshash_test_hooks.fail_next_table_bucket_alloc = 0;
        return NULL;
    }
    return calloc(nmemb, size);
}

static int rshash_test_should_fail_unlink_cas(void) {
    if (rshash_test_hooks.fail_next_unlink_cas) {
        rshash_test_hooks.fail_next_unlink_cas = 0;
        return 1;
    }
    return 0;
}

static void rshash_test_after_mark(rshash_t *h) {
    if (rshash_test_hooks.after_mark != NULL) rshash_test_hooks.after_mark(h, rshash_test_hooks.after_mark_usr);
}

static void rshash_test_after_retired_detach(rshash_t *h) {
    if (rshash_test_hooks.after_retired_detach != NULL)
        rshash_test_hooks.after_retired_detach(h, rshash_test_hooks.after_retired_detach_usr);
}
#else
static void *rshash_alloc_entry(size_t size) {
    return malloc(size);
}

static void *rshash_alloc_table_struct(size_t size) {
    return malloc(size);
}

static void *rshash_alloc_table_buckets(size_t nmemb, size_t size) {
    return calloc(nmemb, size);
}
#endif

static inline unsigned int wrapping_add_unsigned(unsigned int lhs, unsigned int rhs) {
    return (unsigned int)((uint64_t)lhs + rhs);
}

static inline struct entry *atomic_fetch_entry(struct entry **slot, rshash_t *h) {
    (void)h;
    return (struct entry *)ATOMIC_FETCH_PTR((void **)slot, &h->mutTable);
}

static inline int atomic_cas_entry(struct entry **slot, struct entry *oldval, struct entry *newval, rshash_t *h) {
    (void)h;
    return ATOMIC_CAS_PTR((void **)slot, oldval, newval, &h->mutTable);
}

static inline struct rshash_table *atomic_fetch_table(struct rshash_table **slot, rshash_t *h) {
    (void)h;
    return (struct rshash_table *)ATOMIC_FETCH_PTR((void **)slot, &h->mutTables);
}

static inline int atomic_cas_table(struct rshash_table **slot,
                                   struct rshash_table *oldval,
                                   struct rshash_table *newval,
                                   rshash_t *h) {
    (void)h;
    return ATOMIC_CAS_PTR((void **)slot, oldval, newval, &h->mutTables);
}

static void rshash_enter(rshash_t *h) {
    ATOMIC_INC(&h->active_ops, &h->mutActive);
}

static void free_entry(rshash_t *h, struct entry *e, const int free_value) {
    if (h->key_dest == NULL)
        free(e->k);
    else
        h->key_dest(e->k);
    if (free_value) {
        if (h->dest == NULL)
            free(e->v);
        else
            h->dest(e->v);
    }
    free(e);
}

static void free_retired_list(rshash_t *h, struct entry *list) {
    while (list != NULL) {
        struct entry *next = list->retired_next;
        free_entry(h, list, list->retired_free_value);
        list = next;
    }
}

static void rshash_try_reclaim(rshash_t *h) {
    struct entry *list;

    if (ATOMIC_FETCH_32BIT_unsigned(&h->active_ops, &h->mutActive) != 0) return;

    for (;;) {
        list = (struct entry *)ATOMIC_FETCH_PTR((void **)&h->retired, &h->mutRetired);
        if (list == NULL) return;
        if (ATOMIC_CAS_PTR((void **)&h->retired, list, NULL, &h->mutRetired)) break;
    }
#ifdef RSHASH_TEST_HOOKS
    rshash_test_after_retired_detach(h);
#endif
    if (ATOMIC_FETCH_32BIT_unsigned(&h->active_ops, &h->mutActive) == 0) {
        free_retired_list(h, list);
    } else {
        /*
         * A new operation may start after we detach the retired list. Put the
         * list back instead of freeing memory that the new operation can still
         * observe through an old bucket pointer.
         */
        struct entry *tail = list;
        while (tail->retired_next != NULL) tail = tail->retired_next;
        for (;;) {
            struct entry *old_head = (struct entry *)ATOMIC_FETCH_PTR((void **)&h->retired, &h->mutRetired);
            tail->retired_next = old_head;
            if (ATOMIC_CAS_PTR((void **)&h->retired, old_head, list, &h->mutRetired)) break;
        }
    }
}

static void rshash_leave(rshash_t *h) {
    ATOMIC_DEC(&h->active_ops, &h->mutActive);
    rshash_try_reclaim(h);
}

static void retire_entry(rshash_t *h, struct entry *e, const int free_value) {
    if (free_value) e->retired_free_value = 1;
    if (!ATOMIC_CAS(&e->retired, 0, 1, &h->mutRetired)) return;
    for (;;) {
        struct entry *old_head = (struct entry *)ATOMIC_FETCH_PTR((void **)&h->retired, &h->mutRetired);
        e->retired_next = old_head;
        if (ATOMIC_CAS_PTR((void **)&h->retired, old_head, e, &h->mutRetired)) break;
    }
    rshash_try_reclaim(h);
}

static void bucket_find(rshash_t *h,
                        struct rshash_table *tbl,
                        const unsigned int idx,
                        const unsigned int hashvalue,
                        void *key,
                        struct entry ***slot_out,
                        struct entry **entry_out) {
retry: {
    struct entry **slot = &tbl->table[idx];
    struct entry *cur = entry_unmark(atomic_fetch_entry(slot, h));
    while (cur != NULL) {
        struct entry *next = atomic_fetch_entry(&cur->next, h);
        if (entry_is_marked(next)) {
            struct entry *unmarked_next = entry_unmark(next);
            if (!atomic_cas_entry(slot, cur, unmarked_next, h)) goto retry;
            retire_entry(h, cur, 0);
            cur = unmarked_next;
            continue;
        }
        if (ATOMIC_FETCH_32BIT(&cur->state, &h->mutTable) == ENTRY_REMOVED) {
            struct entry *unmarked_next = entry_unmark(next);
            if (!atomic_cas_entry(&cur->next, next, entry_mark(next), h)) goto retry;
            if (!atomic_cas_entry(slot, cur, unmarked_next, h)) goto retry;
            retire_entry(h, cur, 0);
            cur = unmarked_next;
            continue;
        }
        if (cur->h == hashvalue && h->eqfn(key, cur->k)) {
            *slot_out = slot;
            *entry_out = cur;
            return;
        }
        slot = &cur->next;
        cur = entry_unmark(next);
    }
    *slot_out = slot;
    *entry_out = NULL;
}
}

static void bucket_help_unlink(
    rshash_t *h, struct rshash_table *tbl, const unsigned int idx, struct entry *target, const int free_value) {
retry: {
    struct entry **slot = &tbl->table[idx];
    struct entry *cur = entry_unmark(atomic_fetch_entry(slot, h));

    while (cur != NULL) {
        struct entry *next = atomic_fetch_entry(&cur->next, h);
        struct entry *unmarked_next = entry_unmark(next);

        if (entry_is_marked(next)) {
            if (!atomic_cas_entry(slot, cur, unmarked_next, h)) goto retry;
            retire_entry(h, cur, cur == target ? free_value : 0);
            if (cur == target) return;
            cur = unmarked_next;
            continue;
        }
        if (cur == target) return;
        slot = &cur->next;
        cur = unmarked_next;
    }

    /*
     * A node that vanished before we reached it was unlinked by a competing
     * remover or cleanup traversal. retire_entry() is idempotent, so this
     * closes the hand-off race without double-freeing.
     */
    retire_entry(h, target, free_value);
}
}

static int entry_is_present(rshash_t *h, struct entry *entry, struct entry *next) {
    (void)h;
    return !entry_is_marked(next) && ATOMIC_FETCH_32BIT(&entry->state, &h->mutTable) != ENTRY_REMOVED;
}

static int bucket_snapshot_has_duplicate(
    rshash_t *h, struct entry *head, const unsigned int hashvalue, void *key, struct entry *self) {
    struct entry *cur;

    for (cur = entry_unmark(head); cur != NULL;) {
        struct entry *next = atomic_fetch_entry(&cur->next, h);
        if (cur != self && entry_is_present(h, cur, next) && cur->h == hashvalue && h->eqfn(key, cur->k)) return 1;
        cur = entry_unmark(next);
    }
    return 0;
}

static int has_preferred_duplicate(rshash_t *h, const unsigned int hashvalue, void *key, struct entry *self) {
    struct rshash_table *tbl;

    for (tbl = atomic_fetch_table(&h->tables, h); tbl != NULL; tbl = tbl->next) {
        const unsigned int idx = indexFor(tbl->tablelength, hashvalue);
        struct entry *cur = entry_unmark(atomic_fetch_entry(&tbl->table[idx], h));

        while (cur != NULL) {
            struct entry *next = atomic_fetch_entry(&cur->next, h);
            if (cur != self && (uintptr_t)cur < (uintptr_t)self && entry_is_present(h, cur, next) &&
                cur->h == hashvalue && h->eqfn(key, cur->k))
                return 1;
            cur = entry_unmark(next);
        }
    }
    return 0;
}

static struct rshash_table *alloc_table_generation(const unsigned int size) {
    struct rshash_table *tbl = rshash_alloc_table_struct(sizeof(*tbl));
    if (tbl == NULL) return NULL;
    tbl->table = rshash_alloc_table_buckets(size, sizeof(struct entry *));
    if (tbl->table == NULL) {
        free(tbl);
        return NULL;
    }
    tbl->tablelength = size;
    tbl->next = NULL;
    return tbl;
}

static unsigned int next_table_size(const unsigned int current_size) {
    unsigned int pindex;
    for (pindex = 0; pindex < prime_table_length; pindex++) {
        if (primes[pindex] > current_size) return primes[pindex];
    }
    return 0;
}

static struct entry *find_in_tables(rshash_t *h,
                                    const unsigned int hashvalue,
                                    void *key,
                                    struct rshash_table **tbl_out,
                                    unsigned int *idx_out,
                                    struct entry ***slot_out) {
    struct rshash_table *tbl;

    for (tbl = atomic_fetch_table(&h->tables, h); tbl != NULL; tbl = tbl->next) {
        struct entry *entry;
        struct entry **slot;
        const unsigned int idx = indexFor(tbl->tablelength, hashvalue);

        bucket_find(h, tbl, idx, hashvalue, key, &slot, &entry);
        if (entry != NULL) {
            if (tbl_out != NULL) *tbl_out = tbl;
            if (idx_out != NULL) *idx_out = idx;
            if (slot_out != NULL) *slot_out = slot;
            return entry;
        }
    }

    if (tbl_out != NULL) *tbl_out = NULL;
    if (idx_out != NULL) *idx_out = 0;
    if (slot_out != NULL) *slot_out = NULL;
    return NULL;
}

static void rshash_maybe_grow(rshash_t *h) {
    struct rshash_table *current = atomic_fetch_table(&h->tables, h);
    struct rshash_table *newtbl;
    unsigned int next_size;
    const unsigned int count = ATOMIC_FETCH_32BIT_unsigned(&h->entrycount, &h->mutCount);

    if (current == NULL || count <= (current->tablelength * 65u) / 100u) return;
    next_size = next_table_size(current->tablelength);
    if (next_size == 0) return;

    /*
     * Growth is append-only: a new, larger generation becomes the preferred
     * insertion table while old generations remain searchable. Rehashing old
     * nodes would require a table-wide stop-the-world phase or per-node move
     * protocol, both of which would weaken the Stage 1 point-operation goal.
     */
    newtbl = alloc_table_generation(next_size);
    if (newtbl == NULL) return;
    newtbl->next = current;
    if (!atomic_cas_table(&h->tables, current, newtbl, h)) {
        free(newtbl->table);
        free(newtbl);
    }
}

static int entry_try_remove(rshash_t *h, struct entry *entry, void **value);
static void entry_mark_removed(rshash_t *h, struct entry *entry);

static int insert_at_position(rshash_t *h, void *key, void *value, const int unique) {
    const unsigned int hashvalue = hash(h, key);
    struct entry *e = rshash_alloc_entry(sizeof(*e));
    if (e == NULL) return 0;
    e->k = key;
    e->v = value;
    e->h = hashvalue;
    e->retired_next = NULL;
    e->retired = 0;
    e->retired_free_value = 0;
    e->state = ENTRY_LIVE;

    rshash_enter(h);
    for (;;) {
        struct rshash_table *tbl = atomic_fetch_table(&h->tables, h);
        struct entry **slot;
        struct entry *head;
        struct entry *next;
        unsigned int idx;

        if (tbl == NULL) {
            rshash_leave(h);
            free(e);
            return 0;
        }

        if (unique && find_in_tables(h, hashvalue, key, NULL, NULL, NULL) != NULL) {
            rshash_leave(h);
            free_entry(h, e, 1);
            return 0;
        }
        if (unique && tbl != atomic_fetch_table(&h->tables, h)) continue;

        idx = indexFor(tbl->tablelength, hashvalue);
        slot = &tbl->table[idx];
        head = atomic_fetch_entry(slot, h);
        if (entry_is_marked(head)) continue;
        if (unique && bucket_snapshot_has_duplicate(h, head, hashvalue, key, NULL)) {
            rshash_leave(h);
            free_entry(h, e, 1);
            return 0;
        }

        e->next = head;
        if (atomic_cas_entry(slot, head, e, h)) {
            ATOMIC_INC(&h->entrycount, &h->mutCount);
            if (unique && has_preferred_duplicate(h, hashvalue, key, e)) {
                /*
                 * Two unique insertions can publish equal keys concurrently.
                 * Keep the stable lower-address entry and retire this one so
                 * exactly one insertion reports success without global locking.
                 */
                void *discard_value;
                for (;;) {
                    if (entry_try_remove(h, e, &discard_value)) {
                        (void)discard_value;
                        entry_mark_removed(h, e);
                        next = atomic_fetch_entry(&e->next, h);
                        ATOMIC_DEC(&h->entrycount, &h->mutCount);
                        if (atomic_cas_entry(slot, e, entry_unmark(next), h))
                            retire_entry(h, e, 1);
                        else
                            bucket_help_unlink(h, tbl, idx, e, 1);
                        rshash_leave(h);
                        return 0;
                    }
                    if (ATOMIC_FETCH_32BIT(&e->state, &h->mutTable) == ENTRY_REMOVED) break;
                }
            }
            rshash_leave(h);
            rshash_maybe_grow(h);
            return -1;
        }
    }
}

unsigned int hash(rshash_t *h, void *k) {
    unsigned int i = h->hashfn(k);
    i = wrapping_add_unsigned(i, ~(i << 9));
    i ^= ((i >> 14) | (i << 18));
    i = wrapping_add_unsigned(i, i << 4);
    i ^= ((i >> 10) | (i << 22));
    return i;
}

rshash_t *rshash_create(unsigned int minsize,
                        rshash_hash_fn hashfn,
                        rshash_key_eq_fn eqfn,
                        rshash_destruct_fn key_destructor,
                        rshash_destruct_fn value_destructor) {
    rshash_t *h;
    unsigned int pindex;
    unsigned int size = primes[0];
    struct rshash_table *tbl;

    if (hashfn == NULL || eqfn == NULL || minsize > (1u << 30)) return NULL;
    for (pindex = 0; pindex < prime_table_length; pindex++) {
        if (primes[pindex] > minsize) {
            size = primes[pindex];
            break;
        }
    }

    h = malloc(sizeof(*h));
    if (h == NULL) return NULL;
    memset(h, 0, sizeof(*h));
    tbl = alloc_table_generation(size);
    if (tbl == NULL) {
        free(h);
        return NULL;
    }
    h->tables = tbl;
    h->hashfn = hashfn;
    h->eqfn = eqfn;
    h->key_dest = key_destructor;
    h->dest = value_destructor;
    INIT_ATOMIC_HELPER_MUT(h->mutTable);
    INIT_ATOMIC_HELPER_MUT(h->mutTables);
    INIT_ATOMIC_HELPER_MUT(h->mutCount);
    INIT_ATOMIC_HELPER_MUT(h->mutActive);
    INIT_ATOMIC_HELPER_MUT(h->mutRetired);
    return h;
}

int rshash_put(rshash_t *h, void *key, void *value) {
    if (h == NULL || key == NULL || value == NULL) return 0;
    return insert_at_position(h, key, value, 0);
}

int rshash_put_unique(rshash_t *h, void *key, void *value) {
    if (h == NULL || key == NULL || value == NULL) return 0;
    return insert_at_position(h, key, value, 1);
}

int rshash_replace(rshash_t *h, void *key, void *value, void **old_value) {
    unsigned int hashvalue;
    int found = 0;

    if (h == NULL || key == NULL || value == NULL) return 0;
    hashvalue = hash(h, key);
    rshash_enter(h);
    for (;;) {
        struct entry *entry;
        void *oldv;

        entry = find_in_tables(h, hashvalue, key, NULL, NULL, NULL);
        if (entry == NULL) break;
        if (entry_is_marked(atomic_fetch_entry(&entry->next, h))) break;
        if (!ATOMIC_CAS(&entry->state, ENTRY_LIVE, ENTRY_UPDATING, &h->mutTable)) {
            if (ATOMIC_FETCH_32BIT(&entry->state, &h->mutTable) == ENTRY_REMOVED) break;
            continue;
        }
        if (entry_is_marked(atomic_fetch_entry(&entry->next, h))) {
            ATOMIC_STORE_INT(&entry->state, &h->mutTable, ENTRY_LIVE);
            continue;
        }
        oldv = ATOMIC_FETCH_PTR((void **)&entry->v, &h->mutTable);
        ATOMIC_STORE_PTR((void **)&entry->v, &h->mutTable, value);
        ATOMIC_STORE_INT(&entry->state, &h->mutTable, ENTRY_LIVE);
        if (old_value != NULL) *old_value = oldv;
        found = -1;
        break;
    }
    rshash_leave(h);
    return found;
}

static int entry_try_remove(rshash_t *h, struct entry *entry, void **value) {
    (void)h;
    if (!ATOMIC_CAS(&entry->state, ENTRY_LIVE, ENTRY_REMOVED, &h->mutTable)) {
        *value = NULL;
        return 0;
    }
    *value = ATOMIC_FETCH_PTR((void **)&entry->v, &h->mutTable);
    return 1;
}

static void entry_mark_removed(rshash_t *h, struct entry *entry) {
    for (;;) {
        struct entry *next = atomic_fetch_entry(&entry->next, h);
        if (entry_is_marked(next)) break;
        if (atomic_cas_entry(&entry->next, next, entry_mark(next), h)) break;
    }
#ifdef RSHASH_TEST_HOOKS
    rshash_test_after_mark(h);
#endif
}

void *rshash_find(rshash_t *h, void *key) {
    unsigned int hashvalue;
    struct rshash_table *tbl;
    void *value = NULL;

    if (h == NULL || key == NULL) return NULL;
    hashvalue = hash(h, key);
    rshash_enter(h);
    for (tbl = atomic_fetch_table(&h->tables, h); tbl != NULL && value == NULL; tbl = tbl->next) {
        const unsigned int idx = indexFor(tbl->tablelength, hashvalue);
        struct entry *e = entry_unmark(atomic_fetch_entry(&tbl->table[idx], h));
        while (e != NULL) {
            struct entry *next = atomic_fetch_entry(&e->next, h);
            if (!entry_is_marked(next) && ATOMIC_FETCH_32BIT(&e->state, &h->mutTable) != ENTRY_REMOVED) {
                if (e->h == hashvalue && h->eqfn(key, e->k)) {
                    value = ATOMIC_FETCH_PTR((void **)&e->v, &h->mutTable);
                    break;
                }
            }
            e = entry_unmark(next);
        }
    }
    rshash_leave(h);
    return value;
}

void *rshash_remove(rshash_t *h, void *key) {
    unsigned int hashvalue;
    void *value = NULL;

    if (h == NULL || key == NULL) return NULL;
    hashvalue = hash(h, key);
    rshash_enter(h);
    for (;;) {
        struct rshash_table *tbl;
        struct entry **slot;
        struct entry *entry;
        struct entry *next;
        unsigned int idx;

        entry = find_in_tables(h, hashvalue, key, &tbl, &idx, &slot);
        if (entry == NULL) break;
        if (!entry_try_remove(h, entry, &value)) continue;
        entry_mark_removed(h, entry);
        next = atomic_fetch_entry(&entry->next, h);
        ATOMIC_DEC(&h->entrycount, &h->mutCount);
        if (
#ifdef RSHASH_TEST_HOOKS
            !rshash_test_should_fail_unlink_cas() &&
#endif
            atomic_cas_entry(slot, entry, entry_unmark(next), h))
            retire_entry(h, entry, 0);
        else
            bucket_help_unlink(h, tbl, idx, entry, 0);
        break;
    }
    rshash_leave(h);
    return value;
}

#ifdef RSHASH_TEST_HOOKS
int rshash_test_mark_removed_for_key(rshash_t *h, void *key) {
    const unsigned int hashvalue = hash(h, key);
    struct entry *entry;
    void *value;

    rshash_enter(h);
    entry = find_in_tables(h, hashvalue, key, NULL, NULL, NULL);
    if (entry == NULL || !entry_try_remove(h, entry, &value)) {
        rshash_leave(h);
        return 0;
    }
    (void)value;
    entry->retired_free_value = 1;
    entry_mark_removed(h, entry);
    ATOMIC_DEC(&h->entrycount, &h->mutCount);
    rshash_leave(h);
    return 1;
}
#endif

unsigned int rshash_count(rshash_t *h) {
    return ATOMIC_FETCH_32BIT_unsigned(&h->entrycount, &h->mutCount);
}

void rshash_scan(rshash_t *h, rshash_scan_fn cb, void *usr) {
    struct rshash_table *tbl;
    unsigned int i;

    if (cb == NULL) return;
    /*
     * The active-operation guard pins entry storage, not logical membership.
     * Callbacks therefore see only borrowed pointers and weak scan semantics.
     */
    rshash_enter(h);
    for (tbl = atomic_fetch_table(&h->tables, h); tbl != NULL; tbl = tbl->next) {
        for (i = 0; i < tbl->tablelength; ++i) {
            struct entry *e = entry_unmark(atomic_fetch_entry(&tbl->table[i], h));
            while (e != NULL) {
                struct entry *next = atomic_fetch_entry(&e->next, h);
                if (!entry_is_marked(next) && ATOMIC_FETCH_32BIT(&e->state, &h->mutTable) != ENTRY_REMOVED &&
                    cb(e->k, ATOMIC_FETCH_PTR((void **)&e->v, &h->mutTable), usr) == 0) {
                    rshash_leave(h);
                    return;
                }
                e = entry_unmark(next);
            }
        }
    }
    rshash_leave(h);
}

unsigned int rshash_scan_remove_if(rshash_t *h,
                                   rshash_scan_remove_pred_fn pred,
                                   rshash_scan_removed_fn removed,
                                   void *usr) {
    struct rshash_table *tbl;
    unsigned int i;
    unsigned int removed_count = 0;

    if (pred == NULL) return 0;
    rshash_enter(h);
    for (tbl = atomic_fetch_table(&h->tables, h); tbl != NULL; tbl = tbl->next) {
        for (i = 0; i < tbl->tablelength; ++i) {
        retry_bucket: {
            struct entry **slot = &tbl->table[i];
            struct entry *e = entry_unmark(atomic_fetch_entry(slot, h));

            while (e != NULL) {
                struct entry *next = atomic_fetch_entry(&e->next, h);
                struct entry *unmarked_next = entry_unmark(next);
                void *value;

                if (entry_is_marked(next) || ATOMIC_FETCH_32BIT(&e->state, &h->mutTable) == ENTRY_REMOVED) {
                    if (!entry_is_marked(next)) {
                        if (!atomic_cas_entry(&e->next, next, entry_mark(next), h)) goto retry_bucket;
                    }
                    if (!atomic_cas_entry(slot, e, unmarked_next, h)) goto retry_bucket;
                    retire_entry(h, e, 0);
                    e = unmarked_next;
                    continue;
                }

                value = ATOMIC_FETCH_PTR((void **)&e->v, &h->mutTable);
                if (pred(e->k, value, usr)) {
                    if (!entry_try_remove(h, e, &value)) goto retry_bucket;
                    entry_mark_removed(h, e);
                    next = atomic_fetch_entry(&e->next, h);
                    unmarked_next = entry_unmark(next);
                    ATOMIC_DEC(&h->entrycount, &h->mutCount);
                    if (!atomic_cas_entry(slot, e, unmarked_next, h)) bucket_help_unlink(h, tbl, i, e, 0);
                    if (removed != NULL) removed(e->k, value, usr);
                    retire_entry(h, e, 0);
                    ++removed_count;
                    e = unmarked_next;
                    continue;
                }

                slot = &e->next;
                e = unmarked_next;
            }
        }
        }
    }
    rshash_leave(h);
    return removed_count;
}

void rshash_destroy(rshash_t *h, int free_values) {
    struct rshash_table *tbl;
    unsigned int i;

    if (h == NULL) return;
    for (tbl = h->tables; tbl != NULL;) {
        struct rshash_table *next_tbl = tbl->next;
        for (i = 0; i < tbl->tablelength; ++i) {
            struct entry *e = entry_unmark(tbl->table[i]);
            while (e != NULL) {
                struct entry *raw_next = e->next;
                struct entry *next = entry_unmark(raw_next);
                if (!entry_is_marked(raw_next) || !e->retired) free_entry(h, e, free_values);
                e = next;
            }
        }
        free(tbl->table);
        free(tbl);
        tbl = next_tbl;
    }
    free_retired_list(h, h->retired);
    DESTROY_ATOMIC_HELPER_MUT(h->mutTable);
    DESTROY_ATOMIC_HELPER_MUT(h->mutTables);
    DESTROY_ATOMIC_HELPER_MUT(h->mutCount);
    DESTROY_ATOMIC_HELPER_MUT(h->mutActive);
    DESTROY_ATOMIC_HELPER_MUT(h->mutRetired);
    free(h);
}
