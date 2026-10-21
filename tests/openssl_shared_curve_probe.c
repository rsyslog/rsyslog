/* SPDX-License-Identifier: Apache-2.0 */
/* Probe that the shared-curve interposer catches the OpenSSL query used by rsyslog. */
#include "config.h"

#include <openssl/ssl.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--supports-tls12-pinning") == 0) {
#if OPENSSL_VERSION_NUMBER >= 0x10100000L && !defined(LIBRESSL_VERSION_NUMBER)
        return 0;
#else
        return 1;
#endif
    }
    if (argc != 1) return 2;

#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
    SSL_library_init();
#endif
#if OPENSSL_VERSION_NUMBER >= 0x10100000L && !defined(LIBRESSL_VERSION_NUMBER)
    const SSL_METHOD *method = TLS_method();
#else
    const SSL_METHOD *method = SSLv23_method();
#endif
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (ctx == NULL) return 3;
    SSL *ssl = SSL_new(ctx);
    if (ssl == NULL) {
        SSL_CTX_free(ctx);
        return 3;
    }

    /* With the interposer loaded this targeted API query must be forced to 0. */
    const int sharedCurveCount = SSL_get_shared_curve(ssl, -1);
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    return sharedCurveCount == 0 ? 0 : 4;
}
