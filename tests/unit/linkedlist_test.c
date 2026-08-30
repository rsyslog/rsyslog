#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rsyslog.h"
#include "linkedlist.h"

#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) {                                                               \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
            return 1;                                                                \
        }                                                                            \
    } while (0)

struct destroy_counters {
    int data;
    int key;
};

struct exec_state {
    int seen;
    int sum;
    int delete_value;
};

static rsRetVal int_destructor(void *ptr) {
    free(ptr);
    return RS_RET_OK;
}

static rsRetVal counted_data_destructor(void *ptr) {
    struct destroy_counters *ctx = ptr;

    ctx->data++;
    return RS_RET_OK;
}

static rsRetVal counted_key_destructor(void *ptr) {
    struct destroy_counters *ctx = ptr;

    ctx->key++;
    return RS_RET_OK;
}

static int int_cmp(void *lhs, void *rhs) {
    const int l = *(int *)lhs;
    const int r = *(int *)rhs;

    return (l > r) - (l < r);
}

static rsRetVal exec_sum_or_delete(void *pData, void *pParam) {
    struct exec_state *state = pParam;
    const int value = *(int *)pData;

    state->seen++;
    state->sum += value;
    if (value == state->delete_value) {
        return RS_RET_OK_DELETE_LISTENTRY;
    }

    return RS_RET_OK;
}

static int *new_int(int value) {
    int *ptr = malloc(sizeof(*ptr));

    if (ptr == NULL) return NULL;

    *ptr = value;
    return ptr;
}

static rsRetVal append_int(linkedList_t *const list, const int value) {
    int *const data = new_int(value);
    rsRetVal ret;

    if (data == NULL) return RS_RET_OUT_OF_MEMORY;
    ret = llAppend(list, NULL, data);
    if (ret != RS_RET_OK) free(data);
    return ret;
}

static rsRetVal append_keyed_int(linkedList_t *const list, const int keyValue, const int dataValue) {
    int *const key = new_int(keyValue);
    int *const data = new_int(dataValue);
    rsRetVal ret;

    if (key == NULL || data == NULL) {
        free(key);
        free(data);
        return RS_RET_OUT_OF_MEMORY;
    }
    ret = llAppend(list, key, data);
    if (ret != RS_RET_OK) {
        free(key);
        free(data);
    }
    return ret;
}

static int test_init_and_empty_state(void) {
    linkedList_t list;
    int count = -1;

    CHECK(llInit(&list, int_destructor, NULL, NULL) == RS_RET_OK);
    CHECK(list.pRoot == NULL);
    CHECK(list.pLast == NULL);
    CHECK(list.iNumElts == 0);
    CHECK(llGetNumElts(&list, &count) == RS_RET_OK);
    CHECK(count == 0);
    CHECK(llDestroyRootElt(&list) == RS_RET_EMPTY_LIST);
    CHECK(llDestroy(&list) == RS_RET_OK);

    return 0;
}

static int test_append_iterate_and_count(void) {
    linkedList_t list;
    linkedListCookie_t cookie = NULL;
    void *data = NULL;
    int count = -1;

    CHECK(llInit(&list, int_destructor, NULL, NULL) == RS_RET_OK);
    CHECK(append_int(&list, 1) == RS_RET_OK);
    CHECK(append_int(&list, 2) == RS_RET_OK);
    CHECK(append_int(&list, 3) == RS_RET_OK);

    CHECK(list.pRoot != NULL);
    CHECK(list.pLast != NULL);
    CHECK(list.pRoot != list.pLast);
    CHECK(llGetNumElts(&list, &count) == RS_RET_OK);
    CHECK(count == 3);

    CHECK(llGetNextElt(&list, &cookie, &data) == RS_RET_OK);
    CHECK(*(int *)data == 1);
    CHECK(llGetNextElt(&list, &cookie, &data) == RS_RET_OK);
    CHECK(*(int *)data == 2);
    CHECK(llGetNextElt(&list, &cookie, &data) == RS_RET_OK);
    CHECK(*(int *)data == 3);
    CHECK(llGetNextElt(&list, &cookie, &data) == RS_RET_END_OF_LINKEDLIST);

    CHECK(llDestroy(&list) == RS_RET_OK);
    CHECK(list.pRoot == NULL);
    CHECK(list.pLast == NULL);
    CHECK(list.iNumElts == 0);

    return 0;
}

static int test_destroy_root_variants(void) {
    linkedList_t list;
    int count = -1;

    CHECK(llInit(&list, int_destructor, NULL, NULL) == RS_RET_OK);
    CHECK(append_int(&list, 11) == RS_RET_OK);
    CHECK(llDestroyRootElt(&list) == RS_RET_OK);
    CHECK(list.pRoot == NULL);
    CHECK(list.pLast == NULL);
    CHECK(llGetNumElts(&list, &count) == RS_RET_OK);
    CHECK(count == 0);

    CHECK(append_int(&list, 21) == RS_RET_OK);
    CHECK(append_int(&list, 22) == RS_RET_OK);
    CHECK(append_int(&list, 23) == RS_RET_OK);
    CHECK(llDestroyRootElt(&list) == RS_RET_OK);
    CHECK(list.pRoot != NULL);
    CHECK(*(int *)list.pRoot->pData == 22);
    CHECK(*(int *)list.pLast->pData == 23);
    CHECK(llGetNumElts(&list, &count) == RS_RET_OK);
    CHECK(count == 2);

    CHECK(llDestroy(&list) == RS_RET_OK);
    return 0;
}

static int test_find_and_delete_keyed_entries(void) {
    linkedList_t list;
    void *data = NULL;
    int count = -1;
    int key = 202;

    CHECK(llInit(&list, int_destructor, int_destructor, int_cmp) == RS_RET_OK);
    CHECK(append_keyed_int(&list, 101, 1) == RS_RET_OK);
    CHECK(append_keyed_int(&list, 202, 2) == RS_RET_OK);
    CHECK(append_keyed_int(&list, 303, 3) == RS_RET_OK);

    CHECK(llFind(&list, &key, &data) == RS_RET_OK);
    CHECK(*(int *)data == 2);
    CHECK(llFindAndDelete(&list, &key) == RS_RET_OK);
    CHECK(llFind(&list, &key, &data) == RS_RET_NOT_FOUND);
    CHECK(llGetNumElts(&list, &count) == RS_RET_OK);
    CHECK(count == 2);

    CHECK(llDestroy(&list) == RS_RET_OK);
    return 0;
}

static int test_destroy_callbacks(void) {
    linkedList_t list;
    struct destroy_counters counters = {0};

    CHECK(llInit(&list, counted_data_destructor, counted_key_destructor, NULL) == RS_RET_OK);
    CHECK(llAppend(&list, &counters, &counters) == RS_RET_OK);
    CHECK(llAppend(&list, &counters, &counters) == RS_RET_OK);
    CHECK(llDestroy(&list) == RS_RET_OK);
    CHECK(counters.data == 2);
    CHECK(counters.key == 2);

    return 0;
}

static int test_exec_func_delete_entry(void) {
    linkedList_t list;
    struct exec_state state = {.seen = 0, .sum = 0, .delete_value = 2};
    void *data = NULL;
    linkedListCookie_t cookie = NULL;
    int count = -1;

    CHECK(llInit(&list, int_destructor, NULL, NULL) == RS_RET_OK);
    CHECK(append_int(&list, 1) == RS_RET_OK);
    CHECK(append_int(&list, 2) == RS_RET_OK);
    CHECK(append_int(&list, 3) == RS_RET_OK);

    CHECK(llExecFunc(&list, exec_sum_or_delete, &state) == RS_RET_OK);
    CHECK(state.seen == 3);
    CHECK(state.sum == 6);
    CHECK(llGetNumElts(&list, &count) == RS_RET_OK);
    CHECK(count == 2);

    CHECK(llGetNextElt(&list, &cookie, &data) == RS_RET_OK);
    CHECK(*(int *)data == 1);
    CHECK(llGetNextElt(&list, &cookie, &data) == RS_RET_OK);
    CHECK(*(int *)data == 3);
    CHECK(llGetNextElt(&list, &cookie, &data) == RS_RET_END_OF_LINKEDLIST);

    CHECK(llDestroy(&list) == RS_RET_OK);
    return 0;
}

int main(void) {
    struct {
        const char *name;
        int (*fn)(void);
    } tests[] = {
        {"init_and_empty_state", test_init_and_empty_state},
        {"append_iterate_and_count", test_append_iterate_and_count},
        {"destroy_root_variants", test_destroy_root_variants},
        {"find_and_delete_keyed_entries", test_find_and_delete_keyed_entries},
        {"destroy_callbacks", test_destroy_callbacks},
        {"exec_func_delete_entry", test_exec_func_delete_entry},
    };
    size_t i;

    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        if (tests[i].fn() != 0) {
            fprintf(stderr, "FAILED: %s\n", tests[i].name);
            return 1;
        }
    }

    printf("linkedlist tests passed (%zu cases)\n", sizeof(tests) / sizeof(tests[0]));
    return 0;
}
