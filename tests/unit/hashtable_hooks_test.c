#include "config.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Deterministic white-box rshash tests. This binary includes the implementation
 * with private hooks enabled to force rare cleanup, reclamation, and allocation
 * paths while preserving the production object's hook-free build.
 */
#define RSHASH_TEST_HOOKS 1
#include "../../runtime/rshash.c"

#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) {                                                               \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
            return 1;                                                                \
        }                                                                            \
    } while (0)

struct counted {
    int value;
    int *freed;
};

struct pause_ctx {
    pthread_mutex_t mut;
    pthread_cond_t cond;
    int entered;
    int release;
};

struct scan_thread_arg {
    rshash_t *table;
    struct pause_ctx *pause;
};

struct reattach_ctx {
    rshash_t *table;
    pthread_t tid;
    struct pause_ctx pause;
    int key;
};

static struct pause_ctx *blocking_eq_pause;

static unsigned int hash_counted(void *ptr) {
    unsigned int v = (unsigned int)((const struct counted *)ptr)->value;
    v ^= v >> 16;
    v = (unsigned int)((uint64_t)v * 0x7feb352dU);
    v ^= v >> 15;
    v = (unsigned int)((uint64_t)v * 0x846ca68bU);
    v ^= v >> 16;
    return v;
}

static int eq_counted(void *lhs, void *rhs) {
    return ((const struct counted *)lhs)->value == ((const struct counted *)rhs)->value;
}

static int eq_counted_blocking(void *lhs, void *rhs) {
    const struct counted *l = lhs;

    if (blocking_eq_pause != NULL && l->value == 99) {
        struct pause_ctx *pause = blocking_eq_pause;
        pthread_mutex_lock(&pause->mut);
        pause->entered = 1;
        pthread_cond_broadcast(&pause->cond);
        while (!pause->release) pthread_cond_wait(&pause->cond, &pause->mut);
        pthread_mutex_unlock(&pause->mut);
    }
    return eq_counted(lhs, rhs);
}

static struct counted *new_counted(int value, int *freed) {
    struct counted *ptr = malloc(sizeof(*ptr));
    if (ptr == NULL) abort();
    ptr->value = value;
    ptr->freed = freed;
    return ptr;
}

static void counted_free(void *ptr) {
    struct counted *counted = ptr;
    ++*counted->freed;
    free(counted);
}

static void pause_init(struct pause_ctx *pause) {
    if (pthread_mutex_init(&pause->mut, NULL) != 0) abort();
    if (pthread_cond_init(&pause->cond, NULL) != 0) abort();
    pause->entered = 0;
    pause->release = 0;
}

static void pause_wait_entered(struct pause_ctx *pause) {
    pthread_mutex_lock(&pause->mut);
    while (!pause->entered) pthread_cond_wait(&pause->cond, &pause->mut);
    pthread_mutex_unlock(&pause->mut);
}

static void pause_release(struct pause_ctx *pause) {
    pthread_mutex_lock(&pause->mut);
    pause->release = 1;
    pthread_cond_broadcast(&pause->cond);
    pthread_mutex_unlock(&pause->mut);
}

static void pause_destroy(struct pause_ctx *pause) {
    if (pthread_cond_destroy(&pause->cond) != 0) abort();
    if (pthread_mutex_destroy(&pause->mut) != 0) abort();
}

static int never_remove_pred(void *key __attribute__((unused)),
                             void *value __attribute__((unused)),
                             void *usr __attribute__((unused))) {
    return 0;
}

static int pause_scan_cb(void *key __attribute__((unused)), void *value __attribute__((unused)), void *usr) {
    struct pause_ctx *pause = usr;
    pthread_mutex_lock(&pause->mut);
    pause->entered = 1;
    pthread_cond_broadcast(&pause->cond);
    while (!pause->release) pthread_cond_wait(&pause->cond, &pause->mut);
    pthread_mutex_unlock(&pause->mut);
    return 1;
}

static void *scan_pause_thread(void *argptr) {
    struct scan_thread_arg *arg = argptr;
    rshash_scan(arg->table, pause_scan_cb, arg->pause);
    return NULL;
}

static void *blocking_find_thread(void *argptr) {
    struct reattach_ctx *ctx = argptr;
    struct counted lookup = {.value = ctx->key};

    (void)rshash_find(ctx->table, &lookup);
    return NULL;
}

static void start_blocking_find_hook(rshash_t *h __attribute__((unused)), void *usr) {
    struct reattach_ctx *ctx = usr;

    rshash_test_set_after_retired_detach_hook(NULL, NULL);
    blocking_eq_pause = &ctx->pause;
    if (pthread_create(&ctx->tid, NULL, blocking_find_thread, ctx) != 0) abort();
    pause_wait_entered(&ctx->pause);
}

/* Forced unlink CAS failure must fall into helper unlink and retire the key exactly once. */
static int test_forced_helper_unlink(void) {
    int keys_freed = 0;
    int values_freed = 0;
    struct counted lookup = {.value = 1};
    struct counted *removed;
    rshash_t *table = rshash_create(3, hash_counted, eq_counted, counted_free, counted_free);

    CHECK(table != NULL);
    CHECK(rshash_put_unique(table, new_counted(1, &keys_freed), new_counted(10, &values_freed)) != 0);
    rshash_test_fail_next_unlink_cas();
    removed = rshash_remove(table, &lookup);
    CHECK(removed != NULL);
    counted_free(removed);
    CHECK(keys_freed == 1);
    CHECK(values_freed == 1);
    CHECK(rshash_count(table) == 0);
    rshash_destroy(table, 1);
    CHECK(keys_freed == 1);
    CHECK(values_freed == 1);
    return 0;
}

/* A pre-marked removed node must be cleaned by bucket_find users without exposing the removed value. */
static int test_marked_node_cleanup_in_replace(void) {
    int keys_freed = 0;
    int values_freed = 0;
    struct counted lookup = {.value = 1};
    struct counted *replacement = new_counted(1, &values_freed);
    rshash_t *table = rshash_create(3, hash_counted, eq_counted, counted_free, counted_free);

    CHECK(table != NULL);
    CHECK(rshash_put_unique(table, new_counted(1, &keys_freed), new_counted(10, &values_freed)) != 0);
    CHECK(rshash_test_mark_removed_for_key(table, &lookup) != 0);
    CHECK(rshash_replace(table, &lookup, replacement, NULL) == 0);
    counted_free(replacement);
    CHECK(keys_freed == 1);
    CHECK(values_freed == 2);
    CHECK(rshash_count(table) == 0);
    rshash_destroy(table, 1);
    CHECK(keys_freed == 1);
    CHECK(values_freed == 2);
    return 0;
}

/* Scan-remove must also clean pre-marked removed nodes even when its predicate does not match live entries. */
static int test_scan_remove_cleans_marked_node(void) {
    int keys_freed = 0;
    int values_freed = 0;
    struct counted lookup = {.value = 1};
    rshash_t *table = rshash_create(3, hash_counted, eq_counted, counted_free, counted_free);

    CHECK(table != NULL);
    CHECK(rshash_put_unique(table, new_counted(1, &keys_freed), new_counted(10, &values_freed)) != 0);
    CHECK(rshash_test_mark_removed_for_key(table, &lookup) != 0);
    CHECK(rshash_scan_remove_if(table, never_remove_pred, NULL, NULL) == 0);
    CHECK(keys_freed == 1);
    CHECK(values_freed == 1);
    CHECK(rshash_count(table) == 0);
    rshash_destroy(table, 1);
    CHECK(keys_freed == 1);
    CHECK(values_freed == 1);
    return 0;
}

/* Active scans pin removed entry storage; key destruction waits until the scan callback returns. */
static int test_reclamation_defers_until_scan_leaves(void) {
    int keys_freed = 0;
    int values_freed = 0;
    struct counted lookup = {.value = 1};
    struct counted *removed;
    pthread_t tid;
    struct pause_ctx pause;
    struct scan_thread_arg arg;
    rshash_t *table = rshash_create(3, hash_counted, eq_counted, counted_free, counted_free);

    CHECK(table != NULL);
    CHECK(rshash_put_unique(table, new_counted(1, &keys_freed), new_counted(10, &values_freed)) != 0);
    pause_init(&pause);
    arg.table = table;
    arg.pause = &pause;
    CHECK(pthread_create(&tid, NULL, scan_pause_thread, &arg) == 0);
    pause_wait_entered(&pause);
    removed = rshash_remove(table, &lookup);
    CHECK(removed != NULL);
    counted_free(removed);
    CHECK(keys_freed == 0);
    CHECK(values_freed == 1);
    pause_release(&pause);
    CHECK(pthread_join(tid, NULL) == 0);
    pause_destroy(&pause);
    CHECK(keys_freed == 1);
    rshash_destroy(table, 1);
    CHECK(keys_freed == 1);
    CHECK(values_freed == 1);
    return 0;
}

/* If an operation starts after the retired list is detached, reclamation must reattach and free later. */
static int test_retired_list_reattach_race(void) {
    int keys_freed = 0;
    int values_freed = 0;
    struct counted lookup = {.value = 1};
    struct counted *removed;
    struct reattach_ctx ctx;
    rshash_t *table = rshash_create(3, hash_counted, eq_counted_blocking, counted_free, counted_free);

    CHECK(table != NULL);
    CHECK(rshash_put_unique(table, new_counted(1, &keys_freed), new_counted(10, &values_freed)) != 0);
    CHECK(rshash_put_unique(table, new_counted(99, &keys_freed), new_counted(990, &values_freed)) != 0);
    pause_init(&ctx.pause);
    ctx.table = table;
    ctx.key = 99;
    rshash_test_set_after_retired_detach_hook(start_blocking_find_hook, &ctx);
    removed = rshash_remove(table, &lookup);
    CHECK(removed != NULL);
    counted_free(removed);
    CHECK(keys_freed == 0);
    CHECK(values_freed == 1);
    pause_release(&ctx.pause);
    CHECK(pthread_join(ctx.tid, NULL) == 0);
    blocking_eq_pause = NULL;
    rshash_test_set_after_retired_detach_hook(NULL, NULL);
    pause_destroy(&ctx.pause);
    CHECK(keys_freed == 1);
    rshash_destroy(table, 1);
    CHECK(keys_freed == 2);
    CHECK(values_freed == 2);
    return 0;
}

/* Allocation failures must preserve ownership exactly as documented. */
static int test_allocation_failures(void) {
    int keys_freed = 0;
    int values_freed = 0;
    rshash_t *table;
    int i;

    rshash_test_fail_next_table_alloc();
    CHECK(rshash_create(3, hash_counted, eq_counted, counted_free, counted_free) == NULL);
    table = rshash_create(3, hash_counted, eq_counted, counted_free, counted_free);
    CHECK(table != NULL);
    rshash_test_fail_next_entry_alloc();
    CHECK(rshash_put_unique(table, new_counted(1, &keys_freed), new_counted(10, &values_freed)) == 0);
    CHECK(keys_freed == 0);
    CHECK(values_freed == 0);
    counted_free(new_counted(1, &keys_freed));
    counted_free(new_counted(10, &values_freed));
    CHECK(keys_freed == 1);
    CHECK(values_freed == 1);
    for (i = 0; i < 34; ++i)
        CHECK(rshash_put_unique(table, new_counted(i, &keys_freed), new_counted(i, &values_freed)) != 0);
    rshash_test_fail_next_table_alloc();
    CHECK(rshash_put_unique(table, new_counted(100, &keys_freed), new_counted(100, &values_freed)) != 0);
    CHECK(rshash_count(table) == 35);
    rshash_destroy(table, 1);
    CHECK(keys_freed == 36);
    CHECK(values_freed == 36);
    return 0;
}

int main(void) {
    rshash_test_reset_hooks();
    if (test_forced_helper_unlink()) return 1;
    rshash_test_reset_hooks();
    if (test_marked_node_cleanup_in_replace()) return 1;
    rshash_test_reset_hooks();
    if (test_scan_remove_cleans_marked_node()) return 1;
    rshash_test_reset_hooks();
    if (test_reclamation_defers_until_scan_leaves()) return 1;
    rshash_test_reset_hooks();
    if (test_retired_list_reattach_race()) return 1;
    rshash_test_reset_hooks();
    if (test_allocation_failures()) return 1;
    return 0;
}
