/* Very simple program to listen for a mutual-TLS connection, require a valid
 * client certificate, and capture application data to a file.
 * Used for the rsyslog testbench.
 *
 * Copyright 2026 Cisco Systems, Inc., and/or its affiliates.
 * Copyright 2026 Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Rsyslog is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rsyslog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Rsyslog.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */

#include <arpa/inet.h>
#include <netdb.h>
#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void write_listen_port_file(const int sock, const char *const port_file) {
    if (port_file == NULL) {
        return;
    }

    struct sockaddr_storage addr;
    socklen_t addrlen = sizeof(addr);
    char port[NI_MAXSERV];
    FILE *fp;

    if (getsockname(sock, (struct sockaddr *)&addr, &addrlen) != 0) {
        perror("OPENSSL-MTLS: getsockname failed");
        exit(EXIT_FAILURE);
    }
    if (getnameinfo((struct sockaddr *)&addr, addrlen, NULL, 0, port, sizeof(port), NI_NUMERICSERV) != 0) {
        perror("OPENSSL-MTLS: getnameinfo failed");
        exit(EXIT_FAILURE);
    }

    fp = fopen(port_file, "w");
    if (fp == NULL) {
        perror("OPENSSL-MTLS: unable to open port file");
        exit(EXIT_FAILURE);
    }
    fprintf(fp, "%s\n", port);
    fclose(fp);
}

static int create_socket(const char *const port, const char *const port_file) {
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    struct addrinfo *rp;
    int s = -1;
    int optval = 1;
    int v6only = 0;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, port, &hints, &res) != 0) {
        perror("OPENSSL-MTLS: getaddrinfo failed");
        exit(EXIT_FAILURE);
    }

    for (rp = res; rp != NULL; rp = rp->ai_next) {
        s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (s < 0) {
            continue;
        }

        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
        if (rp->ai_family == AF_INET6) {
            setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only));
        }

        if (bind(s, rp->ai_addr, rp->ai_addrlen) == 0) {
            if (listen(s, 1) == 0) {
                write_listen_port_file(s, port_file);
                break;
            }
        }
        close(s);
        s = -1;
    }

    freeaddrinfo(res);

    if (s < 0) {
        perror("OPENSSL-MTLS: unable to bind or listen");
        exit(EXIT_FAILURE);
    }

    return s;
}

static SSL_CTX *create_context(void) {
    const SSL_METHOD *method;
    SSL_CTX *ctx;

#if OPENSSL_VERSION_NUMBER < 0x10100000L
    method = SSLv23_server_method();
#else
    method = TLS_server_method();
#endif

    ctx = SSL_CTX_new(method);
    if (ctx == NULL) {
        fprintf(stderr, "OPENSSL-MTLS: unable to create SSL context\n");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    return ctx;
}

static void apply_ssl_cfg(SSL_CTX *const ctx, const char *const cfg) {
    SSL_CONF_CTX *cctx = NULL;
    char *cfg_copy = NULL;
    char *cursor;

    if (cfg == NULL || *cfg == '\0') {
        return;
    }

    cctx = SSL_CONF_CTX_new();
    if (cctx == NULL) {
        fprintf(stderr, "OPENSSL-MTLS: unable to allocate SSL_CONF_CTX\n");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    SSL_CONF_CTX_set_flags(cctx, SSL_CONF_FLAG_FILE | SSL_CONF_FLAG_SERVER | SSL_CONF_FLAG_CERTIFICATE);
    SSL_CONF_CTX_set_ssl_ctx(cctx, ctx);

    cfg_copy = strdup(cfg);
    if (cfg_copy == NULL) {
        perror("OPENSSL-MTLS: strdup failed");
        SSL_CONF_CTX_free(cctx);
        exit(EXIT_FAILURE);
    }

    cursor = cfg_copy;
    while (cursor != NULL && *cursor != '\0') {
        char *line = cursor;
        char *sep;
        char *eq;
        int conf_ret;

        sep = strpbrk(cursor, "\n;");
        if (sep != NULL) {
            *sep = '\0';
            cursor = sep + 1;
        } else {
            cursor = NULL;
        }

        while (*line == ' ' || *line == '\t' || *line == '\r') {
            ++line;
        }
        if (*line == '\0') {
            continue;
        }

        eq = strchr(line, '=');
        if (eq == NULL) {
            fprintf(stderr, "OPENSSL-MTLS: invalid SSL config command '%s'\n", line);
            free(cfg_copy);
            SSL_CONF_CTX_free(cctx);
            exit(EXIT_FAILURE);
        }
        *eq = '\0';
        ++eq;

        conf_ret = SSL_CONF_cmd(cctx, line, eq);
        if (conf_ret <= 0) {
            fprintf(stderr, "OPENSSL-MTLS: SSL_CONF_cmd failed for '%s=%s'\n", line, eq);
            ERR_print_errors_fp(stderr);
            free(cfg_copy);
            SSL_CONF_CTX_free(cctx);
            exit(EXIT_FAILURE);
        }
    }

    if (SSL_CONF_CTX_finish(cctx) != 1) {
        fprintf(stderr, "OPENSSL-MTLS: SSL_CONF_CTX_finish failed\n");
        ERR_print_errors_fp(stderr);
        free(cfg_copy);
        SSL_CONF_CTX_free(cctx);
        exit(EXIT_FAILURE);
    }

    free(cfg_copy);
    SSL_CONF_CTX_free(cctx);
}

static void configure_context(SSL_CTX *const ctx,
                              const char *const cert_file_path,
                              const char *const key_file_path,
                              const char *const ca_file_path) {
    if (SSL_CTX_use_certificate_file(ctx, cert_file_path, SSL_FILETYPE_PEM) <= 0) {
        fprintf(stderr, "OPENSSL-MTLS: unable to load server certificate\n");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, key_file_path, SSL_FILETYPE_PEM) <= 0) {
        fprintf(stderr, "OPENSSL-MTLS: unable to load server private key\n");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_check_private_key(ctx) != 1) {
        fprintf(stderr, "OPENSSL-MTLS: server certificate/key mismatch\n");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_load_verify_locations(ctx, ca_file_path, NULL) != 1) {
        fprintf(stderr, "OPENSSL-MTLS: unable to load CA certificate\n");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
    SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);
    apply_ssl_cfg(ctx, getenv("OPENSSL_MTLS_CFG"));
}

static FILE *open_output_file(const char *const output_file) {
    FILE *fp = fopen(output_file, "wb");
    if (fp == NULL) {
        perror("OPENSSL-MTLS: unable to open output file");
        exit(EXIT_FAILURE);
    }
    return fp;
}

int main(int argc, char **argv) {
    int sock = -1;
    int client = -1;
    int exit_status = EXIT_FAILURE;
    struct sockaddr_storage addr;
    socklen_t addrlen = sizeof(addr);
    SSL_CTX *ctx = NULL;
    SSL *ssl = NULL;
    FILE *output = NULL;
    X509 *peer_cert = NULL;
    unsigned char buf[4096];

    if (argc < 7) {
        fprintf(stderr, "Usage: %s PORT CERT_FILE KEY_FILE CA_FILE PORT_FILE OUTPUT_FILE\n", argv[0]);
        return EXIT_FAILURE;
    }

    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    ctx = create_context();
    configure_context(ctx, argv[2], argv[3], argv[4]);
    sock = create_socket(argv[1], argv[5]);
    output = open_output_file(argv[6]);

    client = accept(sock, (struct sockaddr *)&addr, &addrlen);
    if (client < 0) {
        perror("OPENSSL-MTLS: accept failed");
        goto done;
    }

    ssl = SSL_new(ctx);
    if (ssl == NULL) {
        fprintf(stderr, "OPENSSL-MTLS: SSL_new failed\n");
        ERR_print_errors_fp(stderr);
        goto done;
    }

    SSL_set_fd(ssl, client);
    if (SSL_accept(ssl) <= 0) {
        fprintf(stderr, "OPENSSL-MTLS: SSL_accept failed\n");
        ERR_print_errors_fp(stderr);
        goto done;
    }

    if (SSL_get_verify_result(ssl) != X509_V_OK) {
        fprintf(stderr, "OPENSSL-MTLS: client certificate verification failed: %ld\n", SSL_get_verify_result(ssl));
        goto done;
    }

    peer_cert = SSL_get_peer_certificate(ssl);
    if (peer_cert == NULL) {
        fprintf(stderr, "OPENSSL-MTLS: no client certificate presented\n");
        goto done;
    }

    for (;;) {
        const int nread = SSL_read(ssl, buf, sizeof(buf));
        if (nread > 0) {
            if (fwrite(buf, 1, (size_t)nread, output) != (size_t)nread) {
                perror("OPENSSL-MTLS: fwrite failed");
                goto done;
            }
            fflush(output);
            continue;
        }
        if (nread == 0) {
            exit_status = EXIT_SUCCESS;
            break;
        }

        fprintf(stderr, "OPENSSL-MTLS: SSL_read failed\n");
        ERR_print_errors_fp(stderr);
        goto done;
    }

done:
    if (ssl != NULL) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
    }
    if (peer_cert != NULL) {
        X509_free(peer_cert);
    }
    if (client >= 0) {
        close(client);
    }
    if (sock >= 0) {
        close(sock);
    }
    if (output != NULL) {
        fclose(output);
    }
    if (ctx != NULL) {
        SSL_CTX_free(ctx);
    }
    EVP_cleanup();
    return exit_status;
}
