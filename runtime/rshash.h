/* Copyright 2026 Adiscon GmbH.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef INCLUDED_RSHASH_H
#define INCLUDED_RSHASH_H

struct hashtable;
/** Opaque hash table handle. */
typedef struct hashtable rshash_t;

/** Hash function for a caller-owned key representation. */
typedef unsigned int (*rshash_hash_fn)(void *key);
/** Key equality predicate; return non-zero when keys are equal. */
typedef int (*rshash_key_eq_fn)(void *key1, void *key2);
/** Destructor for keys or values owned by a table entry. */
typedef void (*rshash_destruct_fn)(void *ptr);
/**
 * Scan callback.
 *
 * Return non-zero to continue scanning, or zero to stop. The key and value
 * pointers are borrowed for the duration of the callback only.
 */
typedef int (*rshash_scan_fn)(void *key, void *value, void *usr);
/** Predicate used by rshash_scan_remove_if(); return non-zero to remove. */
typedef int (*rshash_scan_remove_pred_fn)(void *key, void *value, void *usr);
/**
 * Callback invoked after rshash_scan_remove_if() removes an entry.
 *
 * The key pointer remains table-owned and must not be freed by the callback.
 * The value pointer is returned to the callback and is not destroyed by the
 * table for that removal.
 */
typedef void (*rshash_scan_removed_fn)(void *key, void *value, void *usr);

/**
 * Create a hash table.
 *
 * @param minsize Requested initial bucket count. The implementation rounds up
 *        to an internal prime size.
 * @param hashfn Hash function. Must not be NULL and must be safe to call
 *        concurrently with point operations.
 * @param eqfn Equality predicate. Must not be NULL and must be safe to call
 *        concurrently with point operations.
 * @param key_destructor Destructor for table-owned keys. NULL means free().
 * @param value_destructor Destructor for table-owned values when requested.
 *        NULL means free().
 * @return New table, or NULL on invalid arguments or allocation failure.
 *
 * The table owns keys after successful insertion. Values are table-owned only
 * for destruction paths that explicitly request value destruction, such as
 * rshash_destroy(..., 1), rshash_put_unique() duplicate rejection, or failed
 * unique insertion cleanup.
 */
rshash_t *rshash_create(unsigned int minsize,
                        rshash_hash_fn hashfn,
                        rshash_key_eq_fn eqfn,
                        rshash_destruct_fn key_destructor,
                        rshash_destruct_fn value_destructor);
/**
 * Insert a key/value pair, permitting duplicate keys.
 *
 * @return Non-zero on success, zero on allocation failure.
 *
 * @p h, @p key, and @p value must not be NULL. NULL values are not supported
 * because lookup and removal use NULL as the not-found sentinel. On success,
 * the table owns @p key. On failure, ownership remains with the caller.
 * Duplicate-key entries are allowed for compatibility with historical rsyslog
 * hashtable users; lookup/removal returns one matching live entry.
 */
int rshash_put(rshash_t *h, void *key, void *value);
/**
 * Insert a key/value pair only if no equal key is live.
 *
 * @return Non-zero on success, zero on duplicate rejection or allocation
 *         failure.
 *
 * @p h, @p key, and @p value must not be NULL. NULL values are not supported
 * because lookup and removal use NULL as the not-found sentinel. On duplicate
 * rejection, the supplied key and value are destroyed using the table's
 * configured destructors. On invalid arguments or allocation failure before
 * publication, ownership remains with the caller.
 */
int rshash_put_unique(rshash_t *h, void *key, void *value);
/**
 * Replace the value for one matching live key.
 *
 * @param key Borrowed lookup key; it is not inserted or destroyed.
 * @param value New value. The table consumes it only when replacement succeeds.
 * @param old_value Optional output for the displaced value.
 * @return Non-zero if an entry was replaced, zero if no live entry matched.
 *
 * @p h, @p key, and @p value must not be NULL. NULL values are not supported
 * because lookup and removal use NULL as the not-found sentinel.
 *
 * The table-owned key is unchanged. When @p old_value is non-NULL, the caller
 * receives ownership of the displaced value. When @p old_value is NULL, the
 * displaced value is neither destroyed nor recoverable; use NULL only for
 * values with independent lifetime.
 */
int rshash_replace(rshash_t *h, void *key, void *value, void **old_value);
/**
 * Find one live value by key.
 *
 * @return Borrowed value pointer, or NULL if no live entry matches.
 *
 * The returned pointer is not pinned after the call returns. Callers that need
 * to retain it across concurrent updates must provide their own lifetime
 * ownership or external synchronization.
 */
void *rshash_find(rshash_t *h, void *key);
/**
 * Remove one live entry by key.
 *
 * @return Removed value, or NULL if no live entry matched.
 *
 * The table destroys the removed key after all active point operations have
 * quiesced. The returned value is caller-owned and is not destroyed by the
 * table.
 */
void *rshash_remove(rshash_t *h, void *key);
/**
 * Return the current live-entry count.
 *
 * The value is atomic but weakly consistent with concurrent updates.
 */
unsigned int rshash_count(rshash_t *h);
/**
 * Weakly consistent traversal.
 *
 * The scan is safe with concurrent point operations, but it is not a snapshot:
 * callbacks may observe entries that were live at some point during the scan,
 * may miss entries inserted concurrently, and may skip entries removed
 * concurrently. Callback key/value pointers are borrowed and must not be
 * retained without independent lifetime ownership. If concurrent removers or
 * replacers may destroy returned values immediately, callers must use external
 * synchronization or defer that value destruction until scans that may have
 * borrowed the values have quiesced.
 */
void rshash_scan(rshash_t *h, rshash_scan_fn cb, void *usr);
/**
 * Weakly consistent remove-while-scanning helper.
 *
 * For each live entry observed by the scan, @p pred decides whether to remove
 * it. If removal succeeds and @p removed is non-NULL, @p removed receives the
 * borrowed key and caller-owned value. The table still owns and later destroys
 * the key; the value is not destroyed by the table for that removal.
 *
 * @return Number of entries removed by this call.
 */
unsigned int rshash_scan_remove_if(rshash_t *h,
                                   rshash_scan_remove_pred_fn pred,
                                   rshash_scan_removed_fn removed,
                                   void *usr);
/**
 * Destroy a table.
 *
 * @param free_values Non-zero to destroy live values; zero to destroy keys only.
 *
 * Call only after callers have externally stopped concurrent operations on the
 * table. Keys are always destroyed. Live values are destroyed only when
 * @p free_values is non-zero. Values already returned by rshash_remove() or
 * rshash_scan_remove_if() are caller-owned and are not destroyed here.
 */
void rshash_destroy(rshash_t *h, int free_values);

/** Hash helper for NUL-terminated string keys. */
unsigned int hash_from_string(void *key);
/** Equality helper for NUL-terminated string keys. */
int key_equals_string(void *key1, void *key2);

#endif
