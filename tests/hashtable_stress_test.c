#include "config.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "rshash.h"

/*
 * Probabilistic rshash concurrency stress. The scenarios use fixed work counts
 * and condition-variable start gates, so success is proven by ownership/count
 * invariants rather than timing sleeps or snapshot scan expectations.
 */

#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) {                                                               \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
            return 1;                                                                \
        }                                                                            \
    } while (0)

#define STRESS_THREADS 8
#define STRESS_KEYS 384
#define SCAN_ROUNDS 120
#define MUTATION_ROUNDS 480

struct start_gate {
    pthread_mutex_t mut;
    pthread_cond_t cond;
    int start;
};

struct int_key {
    int value;
};

struct stress_value {
    int key;
    int generation;
};

struct remove_worker_arg {
    rshash_t *table;
    struct start_gate *gate;
    unsigned int *removed_count;
};

struct replace_remove_arg {
    rshash_t *table;
    struct start_gate *gate;
    int id;
    unsigned int *removed_count;
};

struct scan_mutation_arg {
    rshash_t *table;
    struct start_gate *gate;
    int id;
    int *stop;
    int *bad_scan;
    struct stress_value **retired_values;
    unsigned int retired_count;
};

struct growth_arg {
    rshash_t *table;
    struct start_gate *gate;
    int id;
};

struct scan_seen_ctx {
    int seen;
    int bad;
};

static unsigned int hash_int_key(void *ptr) {
    unsigned int v = (unsigned int)((const struct int_key *)ptr)->value;
    v ^= v >> 16;
    v = (unsigned int)((uint64_t)v * 0x7feb352dU);
    v ^= v >> 15;
    v = (unsigned int)((uint64_t)v * 0x846ca68bU);
    v ^= v >> 16;
    return v;
}

static int eq_int_key(void *lhs, void *rhs) {
    return ((const struct int_key *)lhs)->value == ((const struct int_key *)rhs)->value;
}

static struct int_key *new_key(const int value) {
    struct int_key *key = malloc(sizeof(*key));
    if (key == NULL) abort();
    key->value = value;
    return key;
}

static struct stress_value *new_value(const int key, const int generation) {
    struct stress_value *value = malloc(sizeof(*value));
    if (value == NULL) abort();
    value->key = key;
    value->generation = generation;
    return value;
}

static void gate_init(struct start_gate *gate) {
    if (pthread_mutex_init(&gate->mut, NULL) != 0) abort();
    if (pthread_cond_init(&gate->cond, NULL) != 0) abort();
    gate->start = 0;
}

static void gate_release(struct start_gate *gate) {
    if (pthread_mutex_lock(&gate->mut) != 0) abort();
    gate->start = 1;
    if (pthread_cond_broadcast(&gate->cond) != 0) abort();
    if (pthread_mutex_unlock(&gate->mut) != 0) abort();
}

static void gate_wait(struct start_gate *gate) {
    if (pthread_mutex_lock(&gate->mut) != 0) abort();
    while (gate->start == 0) {
        if (pthread_cond_wait(&gate->cond, &gate->mut) != 0) abort();
    }
    if (pthread_mutex_unlock(&gate->mut) != 0) abort();
}

static void gate_destroy(struct start_gate *gate) {
    if (pthread_cond_destroy(&gate->cond) != 0) abort();
    if (pthread_mutex_destroy(&gate->mut) != 0) abort();
}

static void *remove_same_keys_worker(void *argptr) {
    struct remove_worker_arg *arg = argptr;
    int i;

    gate_wait(arg->gate);
    for (i = 0; i < STRESS_KEYS; ++i) {
        struct int_key key = {.value = i};
        struct stress_value *removed = rshash_remove(arg->table, &key);
        if (removed != NULL) {
            if (removed->key != i) abort();
            __sync_fetch_and_add(arg->removed_count, 1);
            free(removed);
        }
    }
    return NULL;
}

/*
 * Competing removers cover the common physical unlink race without relying on
 * wall-clock sleeps. Success means every published value is returned once.
 */
static int test_concurrent_remove_same_keys(void) {
    pthread_t tids[STRESS_THREADS];
    struct remove_worker_arg args[STRESS_THREADS];
    struct start_gate gate;
    unsigned int removed_count = 0;
    rshash_t *table = rshash_create(3, hash_int_key, eq_int_key, free, free);
    int i;

    CHECK(table != NULL);
    for (i = 0; i < STRESS_KEYS; ++i) CHECK(rshash_put_unique(table, new_key(i), new_value(i, 0)) != 0);
    gate_init(&gate);
    for (i = 0; i < STRESS_THREADS; ++i) {
        args[i].table = table;
        args[i].gate = &gate;
        args[i].removed_count = &removed_count;
        CHECK(pthread_create(&tids[i], NULL, remove_same_keys_worker, &args[i]) == 0);
    }
    gate_release(&gate);
    for (i = 0; i < STRESS_THREADS; ++i) CHECK(pthread_join(tids[i], NULL) == 0);
    gate_destroy(&gate);
    CHECK(removed_count == STRESS_KEYS);
    CHECK(rshash_count(table) == 0);
    rshash_destroy(table, 1);
    return 0;
}

static void *replace_remove_worker(void *argptr) {
    struct replace_remove_arg *arg = argptr;
    int round;

    gate_wait(arg->gate);
    for (round = 0; round < MUTATION_ROUNDS; ++round) {
        const int k = (round + arg->id * 17) % STRESS_KEYS;
        struct int_key key = {.value = k};
        if (((round + arg->id) & 3) == 0) {
            struct stress_value *removed = rshash_remove(arg->table, &key);
            if (removed != NULL) {
                if (removed->key != k) abort();
                __sync_fetch_and_add(arg->removed_count, 1);
                free(removed);
            }
        } else {
            struct stress_value *old = NULL;
            struct stress_value *replacement = new_value(k, arg->id * MUTATION_ROUNDS + round);
            if (rshash_replace(arg->table, &key, replacement, (void **)&old)) {
                if (old == NULL || old->key != k) abort();
                free(old);
            } else {
                free(replacement);
            }
        }
    }
    return NULL;
}

/*
 * Replace/remove contention exercises ENTRY_UPDATING and ENTRY_REMOVED retry
 * paths. The oracle permits either operation to win, but validates value
 * ownership and final table consistency.
 */
static int test_replace_remove_race(void) {
    pthread_t tids[STRESS_THREADS];
    struct replace_remove_arg args[STRESS_THREADS];
    struct start_gate gate;
    unsigned int removed_count = 0;
    rshash_t *table = rshash_create(STRESS_KEYS * 2, hash_int_key, eq_int_key, free, free);
    int i;

    CHECK(table != NULL);
    for (i = 0; i < STRESS_KEYS; ++i) CHECK(rshash_put_unique(table, new_key(i), new_value(i, 0)) != 0);
    gate_init(&gate);
    for (i = 0; i < STRESS_THREADS; ++i) {
        args[i].table = table;
        args[i].gate = &gate;
        args[i].id = i;
        args[i].removed_count = &removed_count;
        CHECK(pthread_create(&tids[i], NULL, replace_remove_worker, &args[i]) == 0);
    }
    gate_release(&gate);
    for (i = 0; i < STRESS_THREADS; ++i) CHECK(pthread_join(tids[i], NULL) == 0);
    gate_destroy(&gate);
    CHECK(removed_count <= STRESS_KEYS);
    CHECK(rshash_count(table) == STRESS_KEYS - removed_count);
    rshash_destroy(table, 1);
    return 0;
}

static int validate_scan_cb(void *keyptr, void *valueptr, void *usr) {
    struct scan_seen_ctx *ctx = usr;
    const struct int_key *key = keyptr;
    const struct stress_value *value = valueptr;

    ++ctx->seen;
    /*
     * rshash_scan() is weakly consistent: key and value pointers are borrowed
     * safely, but they are not promised to be a stable snapshot pair while
     * replace/remove operations race with the scan. Range checks still catch
     * corrupted or freed pointers without requiring snapshot semantics.
     */
    if (key == NULL || value == NULL || key->value < 0 || key->value >= STRESS_KEYS || value->key < 0 ||
        value->key >= STRESS_KEYS)
        ctx->bad = 1;
    return 1;
}

static void *scan_loop_worker(void *argptr) {
    struct scan_mutation_arg *arg = argptr;
    int round;

    gate_wait(arg->gate);
    for (round = 0; round < SCAN_ROUNDS && __sync_fetch_and_add(arg->stop, 0) == 0; ++round) {
        struct scan_seen_ctx ctx = {0, 0};
        rshash_scan(arg->table, validate_scan_cb, &ctx);
        if (ctx.bad) __sync_fetch_and_or(arg->bad_scan, 1);
    }
    return NULL;
}

static void *scan_mutator_worker(void *argptr) {
    struct scan_mutation_arg *arg = argptr;
    int round;

    gate_wait(arg->gate);
    for (round = 0; round < MUTATION_ROUNDS; ++round) {
        const int k = (round + arg->id * 31) % STRESS_KEYS;
        struct int_key lookup = {.value = k};
        struct stress_value *removed;

        if ((round & 1) == 0) {
            removed = rshash_remove(arg->table, &lookup);
            if (removed != NULL) arg->retired_values[arg->retired_count++] = removed;
        } else {
            (void)rshash_put_unique(arg->table, new_key(k), new_value(k, round));
        }
    }
    return NULL;
}

/*
 * Weak scans must be memory-safe while mutations run. The oracle deliberately
 * avoids snapshot expectations and checks only pointer validity invariants.
 * Removed values are freed only after scanners stop, matching the rshash value
 * lifetime contract for callers that scan concurrently with removal.
 */
static int test_scan_during_mutation(void) {
    pthread_t scanners[2];
    pthread_t mutators[STRESS_THREADS];
    struct scan_mutation_arg scanner_args[2];
    struct scan_mutation_arg mutator_args[STRESS_THREADS];
    struct start_gate gate;
    int stop = 0;
    int bad_scan = 0;
    rshash_t *table = rshash_create(3, hash_int_key, eq_int_key, free, free);
    int i;

    CHECK(table != NULL);
    for (i = 0; i < STRESS_KEYS; ++i) CHECK(rshash_put_unique(table, new_key(i), new_value(i, 0)) != 0);
    gate_init(&gate);
    for (i = 0; i < STRESS_THREADS; ++i) {
        mutator_args[i].retired_values = calloc(MUTATION_ROUNDS, sizeof(*mutator_args[i].retired_values));
        mutator_args[i].retired_count = 0;
        CHECK(mutator_args[i].retired_values != NULL);
    }
    for (i = 0; i < 2; ++i) {
        scanner_args[i].table = table;
        scanner_args[i].gate = &gate;
        scanner_args[i].id = i;
        scanner_args[i].stop = &stop;
        scanner_args[i].bad_scan = &bad_scan;
        CHECK(pthread_create(&scanners[i], NULL, scan_loop_worker, &scanner_args[i]) == 0);
    }
    for (i = 0; i < STRESS_THREADS; ++i) {
        mutator_args[i].table = table;
        mutator_args[i].gate = &gate;
        mutator_args[i].id = i;
        mutator_args[i].stop = &stop;
        mutator_args[i].bad_scan = &bad_scan;
        CHECK(pthread_create(&mutators[i], NULL, scan_mutator_worker, &mutator_args[i]) == 0);
    }
    gate_release(&gate);
    for (i = 0; i < STRESS_THREADS; ++i) CHECK(pthread_join(mutators[i], NULL) == 0);
    __sync_fetch_and_or(&stop, 1);
    for (i = 0; i < 2; ++i) CHECK(pthread_join(scanners[i], NULL) == 0);
    gate_destroy(&gate);
    CHECK(__sync_fetch_and_add(&bad_scan, 0) == 0);
    for (i = 0; i < STRESS_THREADS; ++i) {
        unsigned int j;
        for (j = 0; j < mutator_args[i].retired_count; ++j) free(mutator_args[i].retired_values[j]);
        free(mutator_args[i].retired_values);
    }
    rshash_destroy(table, 1);
    return 0;
}

static void *growth_worker(void *argptr) {
    struct growth_arg *arg = argptr;
    int i;

    gate_wait(arg->gate);
    for (i = 0; i < STRESS_KEYS; ++i) {
        const int key = arg->id * STRESS_KEYS + i;
        struct int_key lookup = {.value = key};
        if (!rshash_put_unique(arg->table, new_key(key), new_value(key, arg->id))) abort();
        if (rshash_find(arg->table, &lookup) == NULL) abort();
    }
    return NULL;
}

/*
 * Small initial capacity forces append-only generation growth while writers
 * publish concurrently. Every key must remain reachable after growth.
 */
static int test_growth_under_contention(void) {
    pthread_t tids[STRESS_THREADS];
    struct growth_arg args[STRESS_THREADS];
    struct start_gate gate;
    rshash_t *table = rshash_create(3, hash_int_key, eq_int_key, free, free);
    int i;

    CHECK(table != NULL);
    gate_init(&gate);
    for (i = 0; i < STRESS_THREADS; ++i) {
        args[i].table = table;
        args[i].gate = &gate;
        args[i].id = i;
        CHECK(pthread_create(&tids[i], NULL, growth_worker, &args[i]) == 0);
    }
    gate_release(&gate);
    for (i = 0; i < STRESS_THREADS; ++i) CHECK(pthread_join(tids[i], NULL) == 0);
    gate_destroy(&gate);
    CHECK(rshash_count(table) == STRESS_THREADS * STRESS_KEYS);
    for (i = 0; i < STRESS_THREADS * STRESS_KEYS; ++i) {
        struct int_key lookup = {.value = i};
        struct stress_value *value = rshash_find(table, &lookup);
        CHECK(value != NULL);
        CHECK(value->key == i);
    }
    rshash_destroy(table, 1);
    return 0;
}

int main(void) {
    if (test_concurrent_remove_same_keys()) return 1;
    if (test_replace_remove_race()) return 1;
    if (test_scan_during_mutation()) return 1;
    if (test_growth_under_contention()) return 1;
    return 0;
}
