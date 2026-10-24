/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Force the OpenSSL post-handshake shared-group count query to return zero.
 * OpenSSL 3.x implements SSL_get_shared_curve() as SSL_ctrl() with
 * SSL_CTRL_GET_SHARED_GROUP; older supported releases use the curve alias.
 * This makes the obsolete rsyslog diagnostic deterministic without changing
 * the negotiated TLS parameters or the successful handshake.
 */
#include "config.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <openssl/ssl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef long (*ssl_ctrl_func_t)(SSL *, int, long, void *);

static ssl_ctrl_func_t real_ssl_ctrl = NULL;
static pthread_once_t resolve_once = PTHREAD_ONCE_INIT;

static void resolve_ssl_ctrl(void) {
    real_ssl_ctrl = (ssl_ctrl_func_t)dlsym(RTLD_NEXT, "SSL_ctrl");
    if (real_ssl_ctrl == NULL) {
        fputs("OpenSSL shared-curve test hook: cannot resolve SSL_ctrl\n", stderr);
        _exit(2);
    }
}

static void __attribute__((constructor)) mark_hook_loaded(void) {
    const char *const marker = getenv("RSYSLOG_TEST_SSL_SHARED_CURVE_PRELOAD_MARKER");
    int markerfd;

    if (marker != NULL) {
        markerfd = open(marker, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (markerfd < 0 || write(markerfd, "loaded\n", 7) != 7 || close(markerfd) != 0) {
            _exit(2);
        }
    }
}

long SSL_ctrl(SSL *ssl, int cmd, long larg, void *parg) {
#if defined(SSL_CTRL_GET_SHARED_GROUP)
    const int sharedGroupCmd = SSL_CTRL_GET_SHARED_GROUP;
#elif defined(SSL_CTRL_GET_SHARED_CURVE)
    const int sharedGroupCmd = SSL_CTRL_GET_SHARED_CURVE;
#else
    #error "OpenSSL shared-group control command is unavailable"
#endif

    if (cmd == sharedGroupCmd && larg == -1) {
        const char *const marker = getenv("RSYSLOG_TEST_SSL_SHARED_CURVE_QUERY_MARKER");

        /* Keep libssl as a dependency so RTLD_NEXT can find SSL_ctrl even
         * though rsyslog loads its OpenSSL netstream module locally. */
        (void)SSL_get_version(ssl);
        if (marker != NULL) {
            static const char hit[] = "SSL_ctrl shared-group query with larg=-1\n";
            int markerfd = open(marker, O_WRONLY | O_CREAT | O_APPEND, 0600);
            if (markerfd < 0 || write(markerfd, hit, sizeof(hit) - 1) != sizeof(hit) - 1 || close(markerfd) != 0) {
                _exit(2);
            }
        }
        return 0;
    }
    pthread_once(&resolve_once, resolve_ssl_ctrl);
    return real_ssl_ctrl(ssl, cmd, larg, parg);
}
