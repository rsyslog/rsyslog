/* Very simple program to listen for a TLS connection and write the SNI received during the handshake.
 * Used for the rsyslog testbench.
 * Adapted from https://wiki.openssl.org/index.php/Simple_TLS_Server
 * Author: John Cantu
*/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define MAX_FQDN_LENGTH 255 /* Defined in RFC 1035 (DNS) */

int create_socket(int port)
{
	int s;
	struct sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0) {
	perror("Unable to create socket");
	exit(EXIT_FAILURE);
	}

	if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
	perror("Unable to bind");
	exit(EXIT_FAILURE);
	}

	if (listen(s, 1) < 0) {
	perror("Unable to listen");
	exit(EXIT_FAILURE);
	}

	return s;
}

void init_openssl()
{
	SSL_load_error_strings();
	OpenSSL_add_ssl_algorithms();
}

void cleanup_openssl()
{
	EVP_cleanup();
}

SSL_CTX *create_context()
{
	const SSL_METHOD *method;
	SSL_CTX *ctx;

	method = SSLv23_server_method();

	ctx = SSL_CTX_new(method);
	if (!ctx) {
	perror("Unable to create SSL context");
	ERR_print_errors_fp(stderr);
	exit(EXIT_FAILURE);
	}

	return ctx;
}

void configure_context(SSL_CTX *ctx, char *cert_file_path, char *key_file_path)
{
	/* Set the key and cert */
	if (SSL_CTX_use_certificate_file(ctx, cert_file_path, SSL_FILETYPE_PEM) <= 0) {
		ERR_print_errors_fp(stderr);
	exit(EXIT_FAILURE);
	}

	if (SSL_CTX_use_PrivateKey_file(ctx, key_file_path, SSL_FILETYPE_PEM) <= 0 ) {
		ERR_print_errors_fp(stderr);
	exit(EXIT_FAILURE);
	}
}

int main(int argc, char **argv)
{

	if (argc < 4) {
		printf("Usage: openssl_sni_server PORT CERT_FILE_PATH KEY_FILE_PATH\n");
		exit(EXIT_SUCCESS);
	}

	int sock;
	SSL_CTX *ctx;

	init_openssl();
	ctx = create_context();

	fprintf(stderr, "created context\n");

	configure_context(ctx, argv[2], argv[3]);

	fprintf(stderr, "configured context\n");

	sock = create_socket(atoi(argv[1]));

	fprintf(stderr, "Server ready, listening on port %s\n", argv[1]);

	/* Handle connections */
	struct sockaddr_in addr;
	uint len = sizeof(addr);
	SSL *ssl;

	int client = accept(sock, (struct sockaddr*)&addr, &len);
	if (client < 0) {
		perror("Unable to accept");
		exit(EXIT_FAILURE);
	}
	else {
		fprintf(stderr, "accepted connection\n");
	}

	ssl = SSL_new(ctx);
	SSL_set_fd(ssl, client);

	if (SSL_accept(ssl) <= 0) {
		ERR_print_errors_fp(stderr);
	}

	const char *sni = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);

	if (sni != NULL) {
		fprintf(stderr, "RECEIVED SNI: %s\n", sni);
		printf("SNI: %s\n", sni);
	} else {
		fprintf(stderr, "RECEIVED SNI: (NULL)\n");
		/* Use this to indicate null SNI */
		printf("SNI: (NULL)\n");
	}

	fprintf(stderr, "beginning teardown\n");

	SSL_shutdown(ssl);
	SSL_free(ssl);
	close(client);

	fprintf(stderr, "closed connection\n");

	close(sock);
	SSL_CTX_free(ctx);
	cleanup_openssl();

	fprintf(stderr, "completed teardown\n");

	return EXIT_SUCCESS;
}
