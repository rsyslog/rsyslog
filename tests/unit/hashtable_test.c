#include "config.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rshash.h"

#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) {                                                               \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
            return 1;                                                                \
        }                                                                            \
    } while (0)

#define THREADS 4
#define ITEMS_PER_THREAD 1000

struct counted {
    int value;
    int *count;
};

struct worker_arg {
    rshash_t *table;
    int id;
    int failed;
};

struct unique_worker_arg {
    rshash_t *table;
    pthread_mutex_t *mut;
    pthread_cond_t *cond;
    int *start;
    int id;
    int inserted;
};

struct scan_count_ctx {
    int seen;
    int sum;
};

struct scan_stop_ctx {
    int seen;
};

struct remove_even_ctx {
    int values_freed;
};

static void counted_free(void *ptr) {
    struct counted *counted = ptr;
    ++*counted->count;
    free(counted);
}

static unsigned int hash_int(void *ptr) {
    unsigned int v = *(unsigned int *)ptr;
    v ^= v >> 16;
    v = (unsigned int)((uint64_t)v * 0x7feb352dU);
    v ^= v >> 15;
    v = (unsigned int)((uint64_t)v * 0x846ca68bU);
    v ^= v >> 16;
    return v;
}

static int eq_int(void *lhs, void *rhs) {
    return *(int *)lhs == *(int *)rhs;
}

static int *new_int(int value) {
    int *ptr = malloc(sizeof(*ptr));
    if (ptr == NULL) {
        fprintf(stderr, "malloc failed\n");
        exit(2);
    }
    *ptr = value;
    return ptr;
}

/* Duplicate-tolerant put semantics: duplicate keys are accepted, remove returns only the value, and table-owned keys
 * are freed.
 */
static int test_rshash_basic_and_duplicate(void) {
    rshash_t *table = rshash_create(7, hash_int, eq_int, free, NULL);
    int key = 10;

    CHECK(table != NULL);
    CHECK(rshash_put(table, new_int(10), new_int(100)) != 0);
    CHECK(rshash_put(table, new_int(10), new_int(200)) != 0);
    CHECK(rshash_count(table) == 2);
    CHECK(rshash_find(table, &key) != NULL);
    CHECK(*(int *)rshash_find(table, &key) == 100 || *(int *)rshash_find(table, &key) == 200);
    free(rshash_remove(table, &key));
    CHECK(rshash_count(table) == 1);
    free(rshash_remove(table, &key));
    CHECK(rshash_count(table) == 0);
    CHECK(rshash_remove(table, &key) == NULL);
    rshash_destroy(table, 1);
    return 0;
}

/*
 * Modern API ownership: unique insert rejects duplicates, replace returns the displaced value to the caller, remove
 * returns the removed value, and key destructors run through the deferred reclamation path.
 */
static int test_modern_unique_replace_and_destructors(void) {
    int keys_freed = 0;
    int values_freed = 0;
    int key = 7;
    struct counted *old_value = NULL;
    struct counted *removed_value = NULL;
    rshash_t *table = rshash_create(3, hash_int, eq_int, counted_free, counted_free);
    struct counted *k1 = calloc(1, sizeof(*k1));
    struct counted *v1 = calloc(1, sizeof(*v1));
    struct counted *k2 = calloc(1, sizeof(*k2));
    struct counted *v2 = calloc(1, sizeof(*v2));
    struct counted *v3 = calloc(1, sizeof(*v3));

    CHECK(table != NULL);
    CHECK(k1 != NULL && v1 != NULL && k2 != NULL && v2 != NULL && v3 != NULL);
    k1->count = &keys_freed;
    v1->count = &values_freed;
    k2->count = &keys_freed;
    v2->count = &values_freed;
    v3->count = &values_freed;
    k1->value = key;
    k2->value = key;

    CHECK(rshash_put_unique(table, k1, v1) != 0);
    CHECK(rshash_put_unique(table, k2, v2) == 0);
    CHECK(keys_freed == 1);
    CHECK(values_freed == 1);
    CHECK(rshash_count(table) == 1);
    CHECK(rshash_replace(table, &key, v3, (void **)&old_value) != 0);
    counted_free(old_value);
    CHECK(values_freed == 2);
    removed_value = rshash_remove(table, &key);
    CHECK(removed_value == v3);
    counted_free(removed_value);
    CHECK(keys_freed == 2);
    CHECK(values_freed == 3);
    rshash_destroy(table, 1);
    CHECK(keys_freed == 2);
    CHECK(values_freed == 3);
    return 0;
}

/*
 * Create contract: a table needs explicit hash and equality callbacks. Invalid callback arguments must fail cleanly
 * without constructing a partial table.
 */
static int test_rejects_invalid_arguments(void) {
    int key = 1;
    int value = 2;
    rshash_t *table = rshash_create(3, hash_int, eq_int, free, free);

    CHECK(table != NULL);
    CHECK(rshash_create(3, NULL, eq_int, free, free) == NULL);
    CHECK(rshash_create(3, hash_int, NULL, free, free) == NULL);
    CHECK(rshash_put(NULL, &key, &value) == 0);
    CHECK(rshash_put_unique(NULL, &key, &value) == 0);
    CHECK(rshash_replace(NULL, &key, &value, NULL) == 0);
    CHECK(rshash_put(table, &key, NULL) == 0);
    CHECK(rshash_put_unique(table, &key, NULL) == 0);
    CHECK(rshash_replace(table, &key, NULL, NULL) == 0);
    CHECK(rshash_find(NULL, &key) == NULL);
    CHECK(rshash_remove(NULL, &key) == NULL);
    rshash_destroy(table, 1);
    return 0;
}

/*
 * Replace miss contract: replacing a missing key returns false and leaves ownership of the supplied replacement value
 * with the caller.
 */
static int test_replace_miss_keeps_value_ownership(void) {
    int values_freed = 0;
    int key = 17;
    rshash_t *table = rshash_create(3, hash_int, eq_int, free, counted_free);
    struct counted *replacement = calloc(1, sizeof(*replacement));

    CHECK(table != NULL);
    CHECK(replacement != NULL);
    replacement->count = &values_freed;
    replacement->value = 1234;

    CHECK(rshash_replace(table, &key, replacement, NULL) == 0);
    CHECK(values_freed == 0);
    counted_free(replacement);
    CHECK(values_freed == 1);
    rshash_destroy(table, 1);
    CHECK(values_freed == 1);
    return 0;
}

/*
 * Destroy contract: keys are always table-owned and freed at destroy time; values are destroyed only when free_values
 * is non-zero.
 */
static int test_destroy_free_values_contract(void) {
    int keys_freed = 0;
    int values_freed = 0;
    rshash_t *table = rshash_create(3, hash_int, eq_int, counted_free, counted_free);
    struct counted *k1 = calloc(1, sizeof(*k1));
    struct counted *v1 = calloc(1, sizeof(*v1));
    struct counted *k2 = calloc(1, sizeof(*k2));
    struct counted *v2 = calloc(1, sizeof(*v2));

    CHECK(table != NULL);
    CHECK(k1 != NULL && v1 != NULL && k2 != NULL && v2 != NULL);
    k1->count = &keys_freed;
    k2->count = &keys_freed;
    v1->count = &values_freed;
    v2->count = &values_freed;
    k1->value = 1;
    k2->value = 2;
    CHECK(rshash_put_unique(table, k1, v1) != 0);
    CHECK(rshash_put_unique(table, k2, v2) != 0);
    rshash_destroy(table, 0);
    CHECK(keys_freed == 2);
    CHECK(values_freed == 0);
    counted_free(v1);
    counted_free(v2);
    CHECK(values_freed == 2);

    keys_freed = 0;
    values_freed = 0;
    table = rshash_create(3, hash_int, eq_int, counted_free, counted_free);
    CHECK(table != NULL);
    k1 = calloc(1, sizeof(*k1));
    v1 = calloc(1, sizeof(*v1));
    k2 = calloc(1, sizeof(*k2));
    v2 = calloc(1, sizeof(*v2));
    CHECK(k1 != NULL && v1 != NULL && k2 != NULL && v2 != NULL);
    k1->count = &keys_freed;
    k2->count = &keys_freed;
    v1->count = &values_freed;
    v2->count = &values_freed;
    k1->value = 1;
    k2->value = 2;
    CHECK(rshash_put_unique(table, k1, v1) != 0);
    CHECK(rshash_put_unique(table, k2, v2) != 0);
    rshash_destroy(table, 1);
    CHECK(keys_freed == 2);
    CHECK(values_freed == 2);
    return 0;
}

/*
 * Default ownership contract: NULL destructors mean rshash falls back to free() for table-owned keys and, when
 * requested, values. The oracle is clean completion under normal and memory-checking test lanes.
 */
static int test_default_destructors_use_free(void) {
    rshash_t *table = rshash_create(3, hash_int, eq_int, NULL, NULL);

    CHECK(table != NULL);
    CHECK(rshash_put_unique(table, new_int(1), new_int(11)) != 0);
    CHECK(rshash_put_unique(table, new_int(2), new_int(22)) != 0);
    rshash_destroy(table, 1);
    return 0;
}

static int scan_count_cb(void *key __attribute__((unused)), void *value, void *usr) {
    struct scan_count_ctx *ctx = usr;
    ctx->seen++;
    ctx->sum += *(int *)value;
    return 1;
}

static int scan_stop_after_one_cb(void *key __attribute__((unused)), void *value __attribute__((unused)), void *usr) {
    struct scan_stop_ctx *ctx = usr;
    ++ctx->seen;
    return 0;
}

static int remove_even_pred(void *key, void *value __attribute__((unused)), void *usr __attribute__((unused))) {
    return (*(int *)key % 2) == 0;
}

static void remove_even_removed(void *key __attribute__((unused)), void *value, void *usr) {
    struct remove_even_ctx *ctx = usr;
    ++ctx->values_freed;
    free(value);
}

/* Null scan callbacks are no-op API calls; they must not enter traversal or disturb table contents. */
static int test_scan_null_callbacks_are_noops(void) {
    rshash_t *table = rshash_create(3, hash_int, eq_int, free, free);
    int key = 1;

    CHECK(table != NULL);
    CHECK(rshash_put_unique(table, new_int(1), new_int(11)) != 0);
    rshash_scan(table, NULL, NULL);
    CHECK(rshash_scan_remove_if(table, NULL, NULL, NULL) == 0);
    CHECK(rshash_find(table, &key) != NULL);
    CHECK(rshash_count(table) == 1);
    rshash_destroy(table, 1);
    return 0;
}

/* Modern weak scans visit live entries safely, and scan-remove removes matching nodes without cursor state. */
static int test_scan_and_scan_remove(void) {
    rshash_t *table = rshash_create(7, hash_int, eq_int, free, free);
    struct scan_count_ctx scan_ctx = {0, 0};
    struct scan_stop_ctx stop_ctx = {0};
    struct remove_even_ctx remove_ctx = {0};
    int key_one = 1;
    int key_two = 2;
    int key_three = 3;

    CHECK(table != NULL);
    CHECK(rshash_put_unique(table, new_int(1), new_int(11)) != 0);
    CHECK(rshash_put_unique(table, new_int(2), new_int(22)) != 0);
    CHECK(rshash_put_unique(table, new_int(3), new_int(33)) != 0);
    rshash_scan(table, scan_count_cb, &scan_ctx);
    CHECK(scan_ctx.seen == 3);
    CHECK(scan_ctx.sum == 66);
    rshash_scan(table, scan_stop_after_one_cb, &stop_ctx);
    CHECK(stop_ctx.seen == 1);
    CHECK(rshash_scan_remove_if(table, remove_even_pred, remove_even_removed, &remove_ctx) == 1);
    CHECK(remove_ctx.values_freed == 1);
    CHECK(rshash_find(table, &key_two) == NULL);
    CHECK(rshash_find(table, &key_one) != NULL);
    CHECK(rshash_find(table, &key_three) != NULL);
    CHECK(rshash_count(table) == 2);
    rshash_destroy(table, 1);
    return 0;
}

/*
 * Growth regression: a small initial table must expand without moving live nodes, preserving point lookups and scans
 * across all generations.
 */
static int test_growth_preserves_entries(void) {
    enum { NUM_ITEMS = 200 };
    rshash_t *table = rshash_create(3, hash_int, eq_int, free, free);
    struct scan_count_ctx scan_ctx = {0, 0};
    int i;

    CHECK(table != NULL);
    for (i = 0; i < NUM_ITEMS; ++i) CHECK(rshash_put_unique(table, new_int(i), new_int(i * 3)) != 0);
    CHECK(rshash_count(table) == NUM_ITEMS);

    for (i = 0; i < NUM_ITEMS; ++i) {
        int *found = rshash_find(table, &i);
        CHECK(found != NULL);
        CHECK(*found == i * 3);
    }

    rshash_scan(table, scan_count_cb, &scan_ctx);
    CHECK(scan_ctx.seen == NUM_ITEMS);
    CHECK(scan_ctx.sum == (NUM_ITEMS - 1) * NUM_ITEMS / 2 * 3);

    for (i = 0; i < NUM_ITEMS; ++i) free(rshash_remove(table, &i));
    CHECK(rshash_count(table) == 0);
    rshash_destroy(table, 1);
    return 0;
}

/*
 * Point-operation stress: concurrent writers/readers/removers exercise lock-free bucket operations. The oracle is exact
 * lookup values during the run and a final count of the half of entries intentionally left installed.
 */
static void *worker(void *argptr) {
    struct worker_arg *arg = argptr;
    int i;

    for (i = 0; i < ITEMS_PER_THREAD; ++i) {
        const int value = arg->id * ITEMS_PER_THREAD + i;
        if (!rshash_put_unique(arg->table, new_int(value), new_int(value * 2))) {
            arg->failed = 1;
            return NULL;
        }
    }
    for (i = 0; i < ITEMS_PER_THREAD; ++i) {
        const int value = arg->id * ITEMS_PER_THREAD + i;
        int *found = rshash_find(arg->table, (void *)&value);
        if (found == NULL || *found != value * 2) {
            arg->failed = 1;
            return NULL;
        }
    }
    for (i = 0; i < ITEMS_PER_THREAD; i += 2) {
        const int value = arg->id * ITEMS_PER_THREAD + i;
        int *removed = rshash_remove(arg->table, (void *)&value);
        if (removed == NULL || *removed != value * 2) {
            free(removed);
            arg->failed = 1;
            return NULL;
        }
        free(removed);
    }
    return NULL;
}

static int test_concurrent_point_operations(void) {
    pthread_t tids[THREADS];
    struct worker_arg args[THREADS];
    rshash_t *table = rshash_create(THREADS * ITEMS_PER_THREAD * 2, hash_int, eq_int, free, free);
    int i;

    CHECK(table != NULL);
    for (i = 0; i < THREADS; ++i) {
        args[i].table = table;
        args[i].id = i;
        args[i].failed = 0;
        CHECK(pthread_create(&tids[i], NULL, worker, &args[i]) == 0);
    }
    for (i = 0; i < THREADS; ++i) CHECK(pthread_join(tids[i], NULL) == 0);
    for (i = 0; i < THREADS; ++i) CHECK(args[i].failed == 0);
    CHECK(rshash_count(table) == THREADS * ITEMS_PER_THREAD / 2);
    rshash_destroy(table, 1);
    return 0;
}

static void *unique_worker(void *argptr) {
    struct unique_worker_arg *arg = argptr;
    int key = 42;

    pthread_mutex_lock(arg->mut);
    while (*arg->start == 0) pthread_cond_wait(arg->cond, arg->mut);
    pthread_mutex_unlock(arg->mut);
    arg->inserted = rshash_put_unique(arg->table, new_int(key), new_int(arg->id));
    return NULL;
}

/*
 * Unique insert race: all workers try to publish the same key together. The oracle is that exactly one insert succeeds
 * and the table contains one live entry; rejected entries must be consumed by the table API.
 */
static int test_concurrent_unique_insert_same_key(void) {
    enum { ROUNDS = 100 };
    int round;

    for (round = 0; round < ROUNDS; ++round) {
        pthread_t tids[THREADS];
        struct unique_worker_arg args[THREADS];
        pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
        pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
        int start = 0;
        rshash_t *table = rshash_create(3, hash_int, eq_int, free, free);
        int i;
        int inserted = 0;
        int key = 42;

        CHECK(table != NULL);
        for (i = 0; i < THREADS; ++i) {
            args[i].table = table;
            args[i].mut = &mut;
            args[i].cond = &cond;
            args[i].start = &start;
            args[i].id = i;
            args[i].inserted = 0;
            CHECK(pthread_create(&tids[i], NULL, unique_worker, &args[i]) == 0);
        }
        CHECK(pthread_mutex_lock(&mut) == 0);
        start = 1;
        CHECK(pthread_cond_broadcast(&cond) == 0);
        CHECK(pthread_mutex_unlock(&mut) == 0);
        for (i = 0; i < THREADS; ++i) {
            CHECK(pthread_join(tids[i], NULL) == 0);
            if (args[i].inserted) ++inserted;
        }
        CHECK(inserted == 1);
        CHECK(rshash_count(table) == 1);
        CHECK(rshash_find(table, &key) != NULL);
        free(rshash_remove(table, &key));
        CHECK(rshash_count(table) == 0);
        rshash_destroy(table, 1);
        CHECK(pthread_cond_destroy(&cond) == 0);
        CHECK(pthread_mutex_destroy(&mut) == 0);
    }
    return 0;
}

int main(void) {
    if (test_rshash_basic_and_duplicate()) return 1;
    if (test_modern_unique_replace_and_destructors()) return 1;
    if (test_rejects_invalid_arguments()) return 1;
    if (test_replace_miss_keeps_value_ownership()) return 1;
    if (test_destroy_free_values_contract()) return 1;
    if (test_default_destructors_use_free()) return 1;
    if (test_scan_null_callbacks_are_noops()) return 1;
    if (test_scan_and_scan_remove()) return 1;
    if (test_growth_preserves_entries()) return 1;
    if (test_concurrent_point_operations()) return 1;
    if (test_concurrent_unique_insert_same_key()) return 1;
    return 0;
}
