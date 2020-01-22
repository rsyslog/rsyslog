/* Very simple program to listen for a TLS connection and write the SNI received during the handshake.
 * Used for the rsyslog testbench.
 * Adapted from https://www.gnutls.org/manual/html_node/Echo-server-with-X_002e509-authentication.html
 * and https://wiki.openssl.org/index.php/Simple_TLS_Server
 * Author: John Cantu
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <gnutls/gnutls.h>
#include <assert.h>

#define KEYFILE "tls-certs/key.pem"
#define CERTFILE "tls-certs/cert.pem"

#define MAX_FQDN_LENGTH 255 /* Defined in RFC 1035 (DNS) */

#define TRY(x) if(x == -1) { \
	perror("Error in " #x); \
}

/* these macros are GnuTLS specific */

#define GTLS_CHECK(x) do { \
	int GTLS_RETVAL = x; \
	if (GTLS_RETVAL != GNUTLS_E_SUCCESS) { \
		fprintf(stderr, "error: " #x "returned %d", GTLS_RETVAL); \
	} \
} while (0);

#define LOOP_CHECK(rval, cmd) \
		do { \
				rval = cmd; \
				fprintf(stderr, "LOOP_CHECK: " #cmd " returned %d\n", rval); \
		} while(rval == GNUTLS_E_AGAIN || rval == GNUTLS_E_INTERRUPTED)

int create_socket(int port)
{
	int s;
	struct sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0) {
	perror("GTLS-SNI: Unable to create socket");
	exit(EXIT_FAILURE);
	}

	if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
	perror("GTLS-SNI: Unable to bind");
	exit(EXIT_FAILURE);
	}

	if (listen(s, 1) < 0) {
	perror("GTLS-SNI: Unable to listen");
	exit(EXIT_FAILURE);
	}

	return s;
}

void init_gnutls(gnutls_certificate_credentials_t *x509_cred,
					gnutls_priority_t *priority_cache,
					gnutls_session_t *session,
					char *cert_file_path,
					char *key_file_path
) {

		/* for backwards compatibility with gnutls < 3.3.0 */
		GTLS_CHECK(gnutls_global_init());

		GTLS_CHECK(gnutls_certificate_allocate_credentials(x509_cred));

		GTLS_CHECK(gnutls_certificate_set_x509_key_file(*x509_cred,
			cert_file_path,
			key_file_path,
			GNUTLS_X509_FMT_PEM));

		GTLS_CHECK(gnutls_priority_init(priority_cache, NULL, NULL));
}

int main(int argc, char **argv)
{
		int listen_sd, ret;
		gnutls_certificate_credentials_t x509_cred;
		gnutls_priority_t priority_cache;
		gnutls_session_t session;

		setbuf(stdout, NULL);

		if (argc < 4) {
			printf("Usage: gnutls_sni_server PORT CERT_FILE_PATH KEY_FILE_PATH\n");
			exit(EXIT_SUCCESS);
		}

		init_gnutls(&x509_cred, &priority_cache, &session, argv[2], argv[3]);

		/* Socket operations
		 */
		listen_sd = create_socket(atoi(argv[1]));

		fprintf(stderr, "GTLS-SNI: Server ready, listening on port %s\n", argv[1]);

		GTLS_CHECK(gnutls_init(&session, GNUTLS_SERVER));
		GTLS_CHECK(gnutls_priority_set(session, priority_cache));
		GTLS_CHECK(gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE,
										x509_cred));

		gnutls_certificate_server_set_request(session,
												GNUTLS_CERT_IGNORE);

#if GNUTLS_VERSION_NUMBER >= 0x030506
		/* only available since GnuTLS 3.5.6, on previous versions see
		 * gnutls_certificate_set_dh_params(). */
		gnutls_certificate_set_known_dh_params(x509_cred, GNUTLS_SEC_PARAM_MEDIUM);
#endif


		/* Handle connections */
		struct sockaddr_in addr;
		uint len = sizeof(addr);
		char sni[MAX_FQDN_LENGTH];
		size_t sni_size = sizeof(sni);
		uint sni_type = GNUTLS_NAME_DNS;

		while(1) {

			int client = accept(listen_sd, (struct sockaddr*)&addr, &len);

			fprintf(stderr, "GTLS-SNI: Accept() completed\n");

			if (client < 0) {
				perror("GTLS-SNI: Unable to accept");
				exit(EXIT_FAILURE);
			}

			fprintf(stderr, "GTLS-SNI: Accepted connection\n");

			gnutls_transport_set_int(session, client);

			LOOP_CHECK(ret, gnutls_handshake(session));
			if (ret < 0) {
					close(client);
					gnutls_deinit(session);
					fprintf(stderr,
							"GTLS-SNI: Handshake failed (%s)\n",
							gnutls_strerror(ret));
					exit(EXIT_FAILURE);
			}

			fprintf(stderr, "GTLS-SNI: Handshake completed\n");

			int name_retval = gnutls_server_name_get(session, sni, &sni_size, &sni_type, 0);

			if (name_retval == GNUTLS_E_SUCCESS) {
				fprintf(stderr, "GTLS-SNI: Received SNI: %s\n", sni);
				printf("SNI: %s\n", sni);
			} else if (name_retval == GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE) {
				fprintf(stderr, "GTLS-SNI: Received SNI: (NULL)\n");
				/* Use this to indicate null SNI */
				printf("SNI: (NULL)\n");
			} else {
				close(client);
				gnutls_deinit(session);
				fprintf(stderr, "GTLS-SNI: server_name_get failed with code: %d\n", name_retval);
				exit(EXIT_FAILURE);
			}

			fprintf(stderr, "GTLS-SNI: Starting teardown\n");

			LOOP_CHECK(ret, gnutls_bye(session, GNUTLS_SHUT_WR));

			fprintf(stderr, "GTLS-SNI: Closed connection\n");

			TRY(close(client));
			gnutls_deinit(session);
		}

		TRY(close(listen_sd));

		gnutls_certificate_free_credentials(x509_cred);
		gnutls_priority_deinit(priority_cache);

		gnutls_global_deinit();

		return EXIT_SUCCESS;
}
