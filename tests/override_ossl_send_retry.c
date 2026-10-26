/* SPDX-License-Identifier: Apache-2.0 */

/*
 * Copyright 2026 Jérémie Jourdin.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 */

/*
 * LD_PRELOAD fixture: make the first SSL_write of the process report a TLS
 * 1.3 post-handshake read that has nothing to read yet, so nsd_ossl's Send()
 * returns RS_RET_RETRY. The read stays empty for OVERRIDE_OSSL_SEND_RETRY_COUNT
 * sends (default 1); the next one completes it with one byte the client never
 * consumes, and from then on every call reaches OpenSSL unchanged.
 *
 * The sequence a real peer produces cannot be timed from a testbench script:
 *   SSL_write -1, SSL_get_error WANT_READ   (Send drives the pending read)
 *   SSL_read  -1, SSL_get_error WANT_READ   (no record yet: RS_RET_RETRY)
 *   ...                                     (once per further send)
 *   SSL_read   1                            (record arrived)
 *
 * When OVERRIDE_OSSL_SEND_RETRY_MARKER names a file, it is created when the
 * retry is injected and holds the length of the deferred write, so the test
 * can prove the fixture fired and on which send, followed by the length of
 * the short write that preceded it (0 if none).
 *
 * When OVERRIDE_OSSL_SEND_RETRY_PARTIAL is a positive N, the first write
 * longer than N is really written for its first N bytes only and returns N,
 * as a TLS driver that sends one record per call does; the retry is then
 * injected on the write of the rest. Shorter writes before it pass through,
 * so the retry never lands on a write that was not split.
 *
 * The deferred write is recorded. The next write on that session must
 * come from the same buffer, offer at least as many bytes and begin with
 * the same ones, which is what OpenSSL accepts as a repeat of a write that
 * returned a retry; anything else aborts the process, so the test fails.
 */
#include "config.h" /* _GNU_SOURCE, for RTLD_NEXT on older glibc */
#include <dlfcn.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <openssl/ssl.h>

/* The overridden functions reach libssl only through RTLD_NEXT. Without a
 * direct reference a linker that defaults to --as-needed (Ubuntu's gcc)
 * drops libssl from this library, and RTLD_NEXT then finds nothing. */
extern SSL_CTX *(*const override_ossl_send_retry_libssl_ref)(const SSL_METHOD *);
SSL_CTX *(*const override_ossl_send_retry_libssl_ref)(const SSL_METHOD *) = SSL_CTX_new;

enum { ARMED, WRITE_FAILED, READ_PENDING, READ_FAILED, READ_DONE, PASSTHROUGH };

static int state = ARMED;
static const SSL *owed_ssl; /* session that owes the repeat */
static const void *owed_buf; /* buffer of the deferred write */
static int owed_len = -1; /* length of the deferred write */
static uint32_t owed_sum; /* FNV-1a of its bytes */

/* 32-bit FNV-1a, bit for bit. It is computed in 64 bits and reduced
 * so that it does not depend on unsigned wraparound, which is fatal
 * under -fsanitize=unsigned-integer-overflow. */
static uint32_t fnv1a(const void *buf, int len) {
    const unsigned char *p = buf;
    uint64_t h = 2166136261u;
    for (int i = 0; i < len; ++i) {
        h ^= p[i];
        h = (h * 16777619u) & 0xffffffffu;
    }
    return (uint32_t)h;
}
static int empty_reads_left = -1; /* sends that still find nothing to read */
static int partial_done; /* the short write was made */
static int partial_len; /* its length */

/* rsyslog also builds without atomic builtins, so the state machine is
 * guarded by a mutex; this fixture is not on any measured path. */
static pthread_mutex_t state_mut = PTHREAD_MUTEX_INITIALIZER;

static int advance(int from, int to) {
    pthread_mutex_lock(&state_mut);
    const int moved = state == from;
    if (moved) state = to;
    pthread_mutex_unlock(&state_mut);
    return moved;
}

static int state_is(int s) {
    pthread_mutex_lock(&state_mut);
    const int is = state == s;
    pthread_mutex_unlock(&state_mut);
    return is;
}

static void set_empty_reads(int n) {
    pthread_mutex_lock(&state_mut);
    empty_reads_left = n;
    pthread_mutex_unlock(&state_mut);
}

/* claims the one short write, if one is requested and still due */
static int claim_partial(const int num) {
    const char *const env = getenv("OVERRIDE_OSSL_SEND_RETRY_PARTIAL");
    const int n = (env != NULL) ? atoi(env) : 0;
    if (n <= 0 || num <= n) return 0;
    pthread_mutex_lock(&state_mut);
    const int claimed = !partial_done && state == ARMED;
    if (claimed) {
        partial_done = 1;
        partial_len = n;
    }
    pthread_mutex_unlock(&state_mut);
    return claimed ? n : 0;
}

/* a short write is requested and not made yet */
static int partial_pending(void) {
    const char *const env = getenv("OVERRIDE_OSSL_SEND_RETRY_PARTIAL");
    if (env == NULL || atoi(env) <= 0) return 0;
    pthread_mutex_lock(&state_mut);
    const int pending = !partial_done && state == ARMED;
    pthread_mutex_unlock(&state_mut);
    return pending;
}

/* returns the number of empty reads still to report after this one */
static int consume_empty_read(void) {
    pthread_mutex_lock(&state_mut);
    const int left = --empty_reads_left;
    pthread_mutex_unlock(&state_mut);
    return left;
}

static void *real_symbol(const char *const name) {
    void *const sym = dlsym(RTLD_NEXT, name);
    if (sym == NULL) {
        fprintf(stderr, "override_ossl_send_retry: %s not found after the fixture\n", name);
        abort();
    }
    return sym;
}

/* The marker holds the length of the deferred write, so a test can check
 * which send path deferred. */
static void mark_injected(void) {
    static int marked;
    const char *const path = getenv("OVERRIDE_OSSL_SEND_RETRY_MARKER");
    char len[32];
    if (marked || path == NULL || path[0] == '\0') return;
    marked = 1;
    const int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (fd < 0) return;
    pthread_mutex_lock(&state_mut);
    const int partial = partial_len;
    pthread_mutex_unlock(&state_mut);
    const int n = snprintf(len, sizeof(len), "%d %d\n", owed_len, partial);
    if (n > 0 && write(fd, len, (size_t)n) < 0) {
        /* the test reports an empty marker */
    }
    close(fd);
}

int SSL_write(SSL *ssl, const void *buf, int num) {
    static int (*real)(SSL *, const void *, int);
    if (real == NULL) real = (int (*)(SSL *, const void *, int))real_symbol("SSL_write");
    const int partial = claim_partial(num);
    if (partial > 0) return real(ssl, buf, partial);
    if (partial_pending()) return real(ssl, buf, num);
    if (state_is(ARMED)) {
        owed_ssl = ssl;
        owed_buf = buf;
        owed_len = num;
        owed_sum = fnv1a(buf, num);
        if (advance(ARMED, WRITE_FAILED)) return -1;
    }
    if (owed_len >= 0 && ssl == owed_ssl) {
        if (buf != owed_buf) {
            fprintf(stderr, "override_ossl_send_retry: repeat of a deferred write comes from another buffer\n");
            abort();
        }
        if (num < owed_len || fnv1a(buf, owed_len) != owed_sum) {
            fprintf(stderr, "override_ossl_send_retry: repeat of a deferred write offers %d bytes, owed %d\n", num,
                    owed_len);
            abort();
        }
        owed_len = -1;
    }
    return real(ssl, buf, num);
}

/* A session torn down owes nothing; its address may be reused by the next. */
void SSL_free(SSL *ssl) {
    static void (*real)(SSL *);
    if (ssl != NULL && ssl == owed_ssl) owed_len = -1;
    if (real == NULL) real = (void (*)(SSL *))real_symbol("SSL_free");
    real(ssl);
}

int SSL_read(SSL *ssl, void *buf, int num) {
    static int (*real)(SSL *, void *, int);
    if (advance(READ_PENDING, READ_FAILED)) return -1;
    if (num > 0 && advance(READ_DONE, PASSTHROUGH)) {
        ((char *)buf)[0] = '\n';
        return 1;
    }
    if (real == NULL) real = (int (*)(SSL *, void *, int))real_symbol("SSL_read");
    return real(ssl, buf, num);
}

int SSL_get_error(const SSL *ssl, int ret) {
    static int (*real)(const SSL *, int);
    if (advance(WRITE_FAILED, READ_PENDING)) {
        const char *const count = getenv("OVERRIDE_OSSL_SEND_RETRY_COUNT");
        set_empty_reads((count != NULL && atoi(count) > 0) ? atoi(count) : 1);
        return SSL_ERROR_WANT_READ;
    }
    if (state_is(READ_FAILED)) {
        mark_injected();
        if (consume_empty_read() > 0)
            advance(READ_FAILED, READ_PENDING);
        else
            advance(READ_FAILED, READ_DONE);
        return SSL_ERROR_WANT_READ;
    }
    if (real == NULL) real = (int (*)(const SSL *, int))real_symbol("SSL_get_error");
    return real(ssl, ret);
}
