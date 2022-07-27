/* nsd_ossl.c
 *
 * An implementation of the nsd interface for OpenSSL.
 *
 * Copyright 2018-2021 Adiscon GmbH.
 * Author: Andre Lorbach
 *
 * This file is part of the rsyslog runtime library.
 *
 * The rsyslog runtime library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The rsyslog runtime library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the rsyslog runtime library.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 * A copy of the LGPL can be found in the file "COPYING.LESSER" in this distribution.
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <openssl/ssl.h>
#include <openssl/ocsp.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#if OPENSSL_VERSION_NUMBER >= 0x30000000L && !defined(LIBRESSL_VERSION_NUMBER)
#	include <openssl/bioerr.h>
#endif
#include <openssl/engine.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <netdb.h>

#include "rsyslog.h"
#include "syslogd-types.h"
#include "module-template.h"
#include "cfsysline.h"
#include "obj.h"
#include "stringbuf.h"
#include "errmsg.h"
#include "net.h"
#include "netstrm.h"
#include "netstrms.h"
#include "datetime.h"
#include "nsd_ptcp.h"
#include "nsdsel_ossl.h"
#include "nsd_ossl.h"
#include "unicode-helper.h"
#include "rsconf.h"
/* things to move to some better place/functionality - TODO */
// #define CRLFILE "crl.pem"

#define  OCSP_TIMEOUT 5

MODULE_TYPE_LIB
MODULE_TYPE_KEEP

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(glbl)
DEFobjCurrIf(net)
DEFobjCurrIf(datetime)
DEFobjCurrIf(nsd_ptcp)

/* OpenSSL API differences */
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	#define RSYSLOG_X509_NAME_oneline(X509CERT) X509_get_subject_name(X509CERT)
	#define RSYSLOG_BIO_method_name(SSLBIO) BIO_method_name(SSLBIO)
	#define RSYSLOG_BIO_number_read(SSLBIO) BIO_number_read(SSLBIO)
	#define RSYSLOG_BIO_number_written(SSLBIO) BIO_number_written(SSLBIO)
#else
	#define RSYSLOG_X509_NAME_oneline(X509CERT) (X509CERT != NULL ? X509CERT->cert_info->subject : NULL)
	#define RSYSLOG_BIO_method_name(SSLBIO) SSLBIO->method->name
	#define RSYSLOG_BIO_number_read(SSLBIO) SSLBIO->num
	#define RSYSLOG_BIO_number_written(SSLBIO) SSLBIO->num
#endif

static rsRetVal applyGnutlsPriorityString(nsd_ossl_t *const pNsd);

/*--------------------------------------MT OpenSSL helpers ------------------------------------------*/
static MUTEX_TYPE *mutex_buf = NULL;

void locking_function(int mode, int n,
	__attribute__((unused)) const char * file, __attribute__((unused)) int line)
{
	if (mode & CRYPTO_LOCK)
		MUTEX_LOCK(mutex_buf[n]);
	else
		MUTEX_UNLOCK(mutex_buf[n]);
}

unsigned long id_function(void)
{
	return ((unsigned long)THREAD_ID);
}


struct CRYPTO_dynlock_value * dyn_create_function(
	__attribute__((unused)) const char *file, __attribute__((unused)) int line)
{
	struct CRYPTO_dynlock_value *value;
	value = (struct CRYPTO_dynlock_value *)malloc(sizeof(struct CRYPTO_dynlock_value));
	if (!value)
		return NULL;

	MUTEX_SETUP(value->mutex);
	return value;
}

void dyn_lock_function(int mode, struct CRYPTO_dynlock_value *l,
	__attribute__((unused)) const char *file, __attribute__((unused)) int line)
{
	if (mode & CRYPTO_LOCK)
		MUTEX_LOCK(l->mutex);
	else
		MUTEX_UNLOCK(l->mutex);
}

void dyn_destroy_function(struct CRYPTO_dynlock_value *l,
	__attribute__((unused)) const char *file, __attribute__((unused)) int line)
{
	MUTEX_CLEANUP(l->mutex);
	free(l);
}

/* set up support functions for openssl multi-threading. This must
 * be done at library initialisation. If the function fails,
 * processing can not continue normally. On failure, 0 is
 * returned, on success 1.
 */
int opensslh_THREAD_setup(void)
{
	int i;
	mutex_buf = (MUTEX_TYPE *)malloc(CRYPTO_num_locks( ) * sizeof(MUTEX_TYPE));
	if (mutex_buf == NULL)
		return 0;
	for (i = 0; i < CRYPTO_num_locks( ); i++)
		MUTEX_SETUP(mutex_buf[i]);

	CRYPTO_set_id_callback(id_function);
	CRYPTO_set_locking_callback(locking_function);
	/* The following three CRYPTO_... functions are the OpenSSL functions
	for registering the callbacks we implemented above */
	CRYPTO_set_dynlock_create_callback(dyn_create_function);
	CRYPTO_set_dynlock_lock_callback(dyn_lock_function);
	CRYPTO_set_dynlock_destroy_callback(dyn_destroy_function);

	DBGPRINTF("openssl: multithread setup finished\n");
	return 1;
}

/* shut down openssl - do this only when you are totally done
 * with openssl.
 */
int opensslh_THREAD_cleanup(void)
{
	int i;
	if (!mutex_buf)
		return 0;

	CRYPTO_set_id_callback(NULL);
	CRYPTO_set_locking_callback(NULL);
	CRYPTO_set_dynlock_create_callback(NULL);
	CRYPTO_set_dynlock_lock_callback(NULL);
	CRYPTO_set_dynlock_destroy_callback(NULL);

	for (i = 0; i < CRYPTO_num_locks( ); i++)
		MUTEX_CLEANUP(mutex_buf[i]);

	free(mutex_buf);
	mutex_buf = NULL;

	DBGPRINTF("openssl: multithread cleanup finished\n");
	return 1;
}
/*-------------------------------------- MT OpenSSL helpers -----------------------------------------*/

/*--------------------------------------OpenSSL helpers ------------------------------------------*/

void osslLastSSLErrorMsg(int ret, SSL *ssl, int severity, const char* pszCallSource)
{
	unsigned long un_error = 0;
	int iSSLErr = 0;
	if (ssl == NULL) {
		/* Output Error Info*/
		dbgprintf("osslLastSSLErrorMsg: Error in '%s' with ret=%d\n", pszCallSource, ret);
	} else {
		/* if object is set, get error code */
		iSSLErr = SSL_get_error(ssl, ret);

		/* Output error message */
		LogMsg(0, RS_RET_NO_ERRCODE, severity, "%s Error in '%s': '%s(%d)' with ret=%d\n",
			(iSSLErr == SSL_ERROR_SSL ? "SSL_ERROR_SSL" :
			(iSSLErr == SSL_ERROR_SYSCALL ? "SSL_ERROR_SYSCALL" : "SSL_ERROR_UNKNOWN")),
			pszCallSource, ERR_error_string(iSSLErr, NULL), iSSLErr, ret);
	}

	/* Loop through ERR_get_error */
	while ((un_error = ERR_get_error()) > 0){
		LogMsg(0, RS_RET_NO_ERRCODE, severity,
			"nsd_ossl:OpenSSL Error Stack: %s", ERR_error_string(un_error, NULL) );
	}
}

/*
 * CRL is not implemented!
 *
 * This is just a sanity-check stub, to fail on CRL-only certificates.
 * CRL-only certificate means: Certificate CRL DP, but not OCSP/AuthorityInfoAccess information.
 *
 * Returns:
 *  0: if CRL check failed (Error, or Revoked when is_revoked is set)
 *  1: if the certificate status is "GOOD"
 *  2: if the certificate holds no CRL URL

 */
static int crl_check(X509 *current_cert, int *is_revoked) {

	int ret = 0;
	STACK_OF(DIST_POINT) *crldp;
	STACK_OF(OPENSSL_STRING) *ocsp_uris;

	/* Reset revoked status. Needs to be set in case CRL gets implemented. */
	if (is_revoked)
		*is_revoked = 0;

	crldp = X509_get_ext_d2i(current_cert, NID_crl_distribution_points, NULL, NULL);
	ocsp_uris = X509_get1_ocsp(current_cert);
	if (crldp &&  ocsp_uris == NULL) {
		LogError(0, RS_RET_NO_ERRCODE, "CRL support is not implemented. "
				"CRL-only certificates can't be validated!\n");
		ret = 0; /* abort verification. CRL-only not supported.  */
	} else if (!crldp) {
		ret = 2;
	}

	if (ocsp_uris)
		X509_email_free(ocsp_uris);

	if (crldp)
		sk_DIST_POINT_pop_free(crldp, DIST_POINT_free);

	return ret;


}

/*
 * Find the issuer certificate, which is mandatory to generate
 * a OCSP request.
 *
 * Query first locally configured and trusted certificates (e.g CAs).
 *
 * Last resort: Use the certificate chain of the peer, to get hold of
 * untrusted certificates (e.g. intermediate certificates).
 *
 * The resulting issuer certificate might be an untrusted certificate,
 * which should be used to generate OCSP requests.
 */
static X509 *ocsp_find_issuer(X509 *target_cert, const char *cert_name, STACK_OF(X509) *trusted_issuers,
		STACK_OF(X509) *untrusted_peer_certs)
{

	X509 *issuer = NULL;

	/* find issuer among local trusted issuers */
	for (int i = 0; i < sk_X509_num(trusted_issuers); i++) {
		X509 *cert = sk_X509_value(trusted_issuers, i);
		if (X509_check_issued(cert, target_cert) == X509_V_OK) {
			issuer = cert;
			break;
		}
	}

	if (!issuer) {
		/* Look for intermediate certificates in the remote cert-chain */
		for (int i = 0; i < sk_X509_num(untrusted_peer_certs); i++) {
			X509 *cert = sk_X509_value(untrusted_peer_certs, i);
			if (X509_check_issued(cert, target_cert) == X509_V_OK) {
				issuer = cert;
				break;
			}
		}
	}

	/* No issuer, no revocation check possible */
	if (!issuer) {
		LogError(0, RS_RET_NO_ERRCODE, "OCSP: Could not find any issuer for: \"%s\"\n", cert_name);
	}

	return issuer;
}

static const char *ocsp_get_response_status_err(int c)
{
	switch (c) {
	case OCSP_RESPONSE_STATUS_SUCCESSFUL:
		return "Successful";
	case OCSP_RESPONSE_STATUS_MALFORMEDREQUEST:
		return "Malformed request";
	case OCSP_RESPONSE_STATUS_INTERNALERROR:
		return "Internal error in OCSP responder";
	case OCSP_RESPONSE_STATUS_TRYLATER:
		return "Try again later";
	case OCSP_RESPONSE_STATUS_SIGREQUIRED:
		return "Must sign the request";
	case OCSP_RESPONSE_STATUS_UNAUTHORIZED:
		return "Request unauthorized";
	default:
		return "Unknown Response Status";
	}
}

/*
 * Returns 1 if the cert status is "GOOD"
 */
static int ocsp_check_validate_response_and_cert(OCSP_RESPONSE *rsp, OCSP_REQUEST *req,
		STACK_OF(X509) *untrusted_peer_certs, OCSP_CERTID *id,
		const char *cert_name, int *is_revoked)
{
	int s, ret = 0;
	long leeway_sec = 300; /* 5 Minutes */
	long maxage = -1; /* No maximum age for ThisUpdate response */

	int status, reason;
	ASN1_GENERALIZEDTIME *rev, *thisupd, *nextupd;
	OCSP_BASICRESP *bs = NULL;
	X509_STORE *store = SSL_CTX_get_cert_store(ctx);

	bs = OCSP_response_get1_basic(rsp);
	if (bs == NULL) {
		LogError(0, RS_RET_NO_ERRCODE, "Failed to decode the basic OCSP response.\n");
		goto err;
	}

	s = OCSP_check_nonce(req, bs);
	if (s == -1) {
		LogMsg(0, RS_RET_NO_ERRCODE, LOG_WARNING, "Warning: no Nonce in OCSP response from peer. "
				"(No replay attack protection available)\n");
	} else if (s == 0) {
		LogError(0, RS_RET_NO_ERRCODE, "OCSP Nonce verification error. (Replayed OCSP response?)\n");
		goto err;
	}

	if (!OCSP_basic_verify(bs, untrusted_peer_certs, store, 0)) {
		LogError(0, RS_RET_NO_ERRCODE, "Failed to verify the response message. "
				"(Incorrectly signed by OCSP responder? CA certificates correctly "
				"installed to validate the OCSP Signer?)\n");
		goto err;
	}

	if (!OCSP_resp_find_status(bs, id, &status, &reason,
	                           &rev, &thisupd, &nextupd)) {
		LogError(0, RS_RET_NO_ERRCODE, "No status included in the OCSP response for the "
				"requested certificate ID!\n");
		goto err;
	}
	if (!OCSP_check_validity(thisupd, nextupd, leeway_sec, maxage)) {
		LogError(0, RS_RET_NO_ERRCODE, "The OCSP response has expired and is no longer valid!\n");
		goto err;
	}

	if (is_revoked)
		*is_revoked = 0;

	switch (status) {
	case V_OCSP_CERTSTATUS_GOOD:
		break;
	case V_OCSP_CERTSTATUS_REVOKED:
		if (is_revoked)
			*is_revoked = 1;
		if (OCSP_crl_reason_str(reason)) {
			LogError(0, RS_RET_NO_ERRCODE, "OCSP: Certificate \"%s\" has been revoked: %s.\n",
					cert_name, OCSP_crl_reason_str(reason));
			goto err;
		} else {
			LogError(0, RS_RET_NO_ERRCODE, "OCSP: Certificate \"%s\" has been revoked: "
					"No reason provided.\n", cert_name);
			goto err;
		}
		break;
	case V_OCSP_CERTSTATUS_UNKNOWN:
		LogError(0, RS_RET_NO_ERRCODE, "OCSP: Status of certificate \"%s\" is unknown. "
				"(This is a fatal error).\n", cert_name);
		goto err;
		break;
	default:
		LogError(0, RS_RET_NO_ERRCODE, "OCSP: Unexpected certificate status \"%d\" for %s .\n",
				status, cert_name);
		goto err;
		break;
	}


	ret = 1;
err:

	if (bs)
		OCSP_BASICRESP_free(bs);

	return ret;
}

static BIO* ocsp_connect(const char *host, const char *port, const char *device)
{
	int erropt, ret = 0, fd = 0;
	struct timeval tv;
	fd_set confds;
	struct addrinfo hints;
	struct addrinfo *rp = NULL;
	BIO *bio = NULL;
	socklen_t errlen;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	ret = getaddrinfo(host, port, &hints, &rp);
	if (ret != 0) {
		LogError(0, RS_RET_NO_ERRCODE, "OCSP: Could not lookup address for %s:%s\n", host, port);
		goto err;
	}

	fd = socket(rp->ai_family, SOCK_STREAM, 0);
	if (fd == -1) {
		LogError(0, RS_RET_NO_ERRCODE, "OCSP: Could not create socket for %s:%s\n", host, port);
		goto err;
	}

	if (device) {
#ifdef SO_BINDTODEVICE
		/* cppcheck-suppress ConfigurationNotChecked */
		if(setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, device,
		              strlen(device) + 1) < 0)
#endif
		{
			LogError(0, RS_RET_NO_ERRCODE, "Failed to bind to interface \"%s\" for "
					"OCSP request.\n", device);
			goto err;
		}
	}
	if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
		LogError(0, RS_RET_NO_ERRCODE, "OCSP: Failed make socket non-blocking for %s:%s: %s\n",
				host, port, strerror(errno));
		goto err;
	}

	if (connect(fd, rp->ai_addr, rp->ai_addrlen) != -1) {
		LogError(0, RS_RET_NO_ERRCODE, "OCSP: Failed to connect to %s:%s: %s\n",
				host, port, strerror(errno));
		goto err;
	}

	FD_ZERO(&confds);
	FD_SET(fd, &confds);
	tv.tv_usec = 0;
	tv.tv_sec = OCSP_TIMEOUT;
	ret = select(fd + 1, NULL, (void *)&confds, NULL, &tv);
	if (ret == 0) {
		LogError(0, RS_RET_NO_ERRCODE, "OCSP: Connection timeout while connecting to %s:%s\n",
				host, port);
		goto err;
	}

	errlen = sizeof(erropt);
	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &erropt, &errlen) != 0) {
		LogError(0, RS_RET_NO_ERRCODE, "OCSP: Failed to socket status for connection %s:%s: %s\n",
				host, port, strerror(errno));
		goto err;
	}

	if ((erropt != 0) && (erropt != EISCONN)) {
		LogError(0, RS_RET_NO_ERRCODE, "OCSP: Failed to establish connection for %s:%s: %s\n",
				host, port, strerror(erropt));
		goto err;
	}

	bio = BIO_new_socket(fd, BIO_CLOSE);
	if (bio == NULL) {
		LogError(0, RS_RET_NO_ERRCODE, "OCSP: Could attach socket to BIO object\n");
		goto err;
	}
err:

	freeaddrinfo(rp);

	if (!bio) /* don't close the socket in case of success */
		close(fd);

	return bio;
}

static OCSP_RESPONSE *ocsp_send_and_recieve(BIO *bio, const char *host, const char *path,
		OCSP_REQ_CTX *reqctx, OCSP_REQUEST *req, const char *url)
{
	int fd, ret;
	struct timeval tv;
	fd_set confds;
	OCSP_RESPONSE *rsp = NULL;

	if (BIO_get_fd(bio, &fd) < 0) {
		LogError(0, RS_RET_NO_ERRCODE, "Failed to send OCSP request (BIO_get_fd)\n");
		goto err;
	}

	reqctx = OCSP_sendreq_new(bio, path, NULL, -1);
	if (!reqctx) {
		LogError(0, RS_RET_NO_ERRCODE, "Failed to prepare sending of OCSP request. Out of memory?\n");
		goto err;
	}

	/* Some OCSP responders behind CDNs require the domain
	 * of the service as "Host" header. Not mentioned by RFC6960. */
	if (!OCSP_REQ_CTX_add1_header(reqctx, "Host", host)) {
		LogError(0, RS_RET_NO_ERRCODE, "Failed to attach \"Host\" to the HTTP header for the OCSP request. "
				"OCSP URI: \"%s\"\n", url);
		goto err;
	}

	if (!OCSP_REQ_CTX_set1_req(reqctx, req)) {
		LogError(0, RS_RET_NO_ERRCODE, "Failed to attach OCSP request to request context.\n");
		goto err;
	}

	while (1) {
		ret = OCSP_sendreq_nbio(&rsp, reqctx);
		if (ret != -1)
			break;
		FD_ZERO(&confds);
		FD_SET(fd, &confds);
		tv.tv_usec = 0;
		tv.tv_sec = OCSP_TIMEOUT;
		if (BIO_should_read(bio))
			ret = select(fd + 1, (void *)&confds, NULL, NULL, &tv);
		else if (BIO_should_write(bio))
			ret = select(fd + 1, NULL, (void *)&confds, NULL, &tv);
		else {
			LogError(0, RS_RET_NO_ERRCODE, "Unexpected socket activity while waiting for OCSP response "
					"from \"%s\"\n", url);
			goto err;
		}
		if (ret == 0) {
			LogError(0, RS_RET_NO_ERRCODE, "Timed out while waiting for OCSP response from \"%s\"\n",
					url);
			goto err;
		}
		if (ret == -1) {
			LogError(0, RS_RET_NO_ERRCODE, "Socket error while waiting for OCSP response from \"%s\"\n",
					url);
			goto err;
		}
	}

	return rsp;

err:
	return NULL;
}

/*
 * Returns 1 for supported OCSP responders/protocols.
 */
static int ocsp_is_supported_protocol(const char *url)
{
	if (strncasecmp("http:", url, 5) != 0)
		return 0;

	return 1;
}

static int ocsp_request_per_responder(const char *url, X509 *cert, X509 *issuer,
		STACK_OF(X509) *untrusted_peer_certs, const char *device, int *is_revoked)
{
	BIO *bio = NULL;
	OCSP_RESPONSE *rsp = NULL;
	OCSP_REQUEST *req = NULL;
	OCSP_CERTID *id = NULL;
	OCSP_REQ_CTX *reqctx = NULL;

	int use_ssl, ret = 0;
	char *host = NULL, *port = NULL, *path = NULL;
	char cert_name[256];

	const EVP_MD *cert_id_md = EVP_sha256();

	X509_NAME_oneline(X509_get_subject_name(cert), cert_name, sizeof(cert_name));

	if (!ocsp_is_supported_protocol(url))
		goto err; /* Skip unsupported protocols. That's not an error. */

	/*
	 * 1. Prepare OCSP request
	 */
	if (OCSP_parse_url(url, &host, &port, &path, &use_ssl) != 1) {
		LogError(0, RS_RET_NO_ERRCODE, "Parsing of OCSP URL \"%s\" failed.", url);
		goto err;
	}

	req = OCSP_REQUEST_new();
	if (req == NULL) {
		LogError(0, RS_RET_NO_ERRCODE, "Allocating new OCSP REQUEST failed: Out-of-memory?\n");
		goto err;
	}

	id = OCSP_cert_to_id(cert_id_md, cert, issuer);
	if (!id) {
		LogError(0, RS_RET_NO_ERRCODE, "Could not obtain ID of certificate \"%s\" for OCSP request.\n",
		        cert_name);
		goto err;
	}

	if (!OCSP_request_add0_id(req, id)) {
		LogError(0, RS_RET_NO_ERRCODE, "Failed to attach ID of certificate \"%s\" to OCSP request.\n",
		        cert_name);
		goto err;
	}

	/* Replay attack protection */
	if (!OCSP_request_add1_nonce(req, NULL, -1)) {
		LogError(0, RS_RET_NO_ERRCODE, "Failed to attach the OCSP Nonce extension to OCSP request.\n");
		goto err;
	}

	dbgprintf("OCSP: Connecting to OCSP URL: http://%s:%s%s use_ssl:%d\n", host, port, path, use_ssl);

	/*
	 * 2. Connect to OCSP responder via HTTP
	 */
	bio = ocsp_connect(host, port, device);
	if (!bio) {
		goto err;
	}

	/*
	 * 3. Send OCSP request & receive OCSP response
	 */
	rsp = ocsp_send_and_recieve(bio, host, path, reqctx, req, url);
	if (!rsp)
		goto err;

	/*
	 * 4. Process OCSP response
	 */
	ret = OCSP_response_status(rsp);
	if (ret != OCSP_RESPONSE_STATUS_SUCCESSFUL) {
		LogError(0, RS_RET_NO_ERRCODE, "OCSP response status: %s for cert \"%s\". (Received from: \"%s\")\n",
		        ocsp_get_response_status_err(ret), cert_name, url);
		goto err;
	}

	/*
	 * 5. Validate OCSP response and the included certificate status
	 */
	if (ocsp_check_validate_response_and_cert(rsp, req,
	                                          untrusted_peer_certs, id, cert_name, is_revoked))
		ret = 1; /* Success! */

err:
	/* cleanup */

	if (host)
		OPENSSL_free(host);
	if (port)
		OPENSSL_free(port);
	if (path)
		OPENSSL_free(path);
	if (req)
		OCSP_REQUEST_free(req);
	if (rsp)
		OCSP_RESPONSE_free(rsp);
	if (bio)
		BIO_free_all(bio);
	if (reqctx)
		OCSP_REQ_CTX_free(reqctx);

	return ret;
}

/*
 * ocsp_check is supposed to be called by the validate_callback, for each certificate
 * in the peers certificate chain.
 *
 * current_cert is pointing to the next certificate which requires a revocation check.
 *
 * untrusted_peer_certs holds the peer's certificate chain, which might be incomplete.
 *
 * Returns:
 *  0: if OCSP check failed (Error, or Revoked when is_revoked is set)
 *  1: if the certificate status is "GOOD"
 *  2: if the certificate holds no OCSP URL
 *
 */
static int ocsp_check(X509 *current_cert, STACK_OF(X509) *trusted_issuers, STACK_OF(X509) *untrusted_peer_certs,
		const char *device, int *is_revoked) {

	int ret = 0, at_least_one_responder = 0;
	char *url;
	X509 *issuer = NULL;
	STACK_OF(OPENSSL_STRING) *ocsp_responders = NULL;
	char cert_name[256];

	X509_NAME_oneline(X509_get_subject_name(current_cert), cert_name, sizeof(cert_name));


	/*
	 * 1. Lookup the issuer cert of the current certificate, required to marshal a OCSP request.
	 */
	if (!(issuer = ocsp_find_issuer(current_cert, cert_name, trusted_issuers, untrusted_peer_certs)))
		goto err;

	/*
	 * 2. Sanity check prior generating OCSP request.
	 *
	 * - No OCSP URL, no OCSP revocation verification required, skip the current cert.
	 * - Only HTTP protocol is supported. If no OCSP responder with a supported protocol
	 *   is available, this MUST fail the OCSP verification and abort the TLS handshake.
	 */
	ocsp_responders = X509_get1_ocsp(current_cert);
	if (ocsp_responders == NULL) {
		ret = 2;
		goto err; /* continue with cert path validation */
	}


	for (int i = 0; i < sk_OPENSSL_STRING_num(ocsp_responders); i++) {
		url = sk_OPENSSL_STRING_value(ocsp_responders, i);
		if (ocsp_is_supported_protocol(url)) {
			at_least_one_responder = 1;
			break;
		}
	}

	if (!at_least_one_responder) {
		LogError(0, RS_RET_NO_ERRCODE, "None of the OCSP responders are supported by "
				"this implementation:\n");
		for (int i = 0; i < sk_OPENSSL_STRING_num(ocsp_responders); i++) {
			url = sk_OPENSSL_STRING_value(ocsp_responders, i);
			LogError(0, RS_RET_NO_ERRCODE, "\t%s\n", url);
		}
		goto err;
	}

	/*
	 * Try all supported OCSP responders one by one.
	 *
	 * One successful OCSP status response is enough. Not all responders
	 * need to be queried.
	 */
	for (int i = 0; i < sk_OPENSSL_STRING_num(ocsp_responders); i++) {
		url = sk_OPENSSL_STRING_value(ocsp_responders, i);
		if (ocsp_request_per_responder(url, current_cert, issuer, untrusted_peer_certs,
					device, is_revoked)) {
			ret = 1;
			goto err;
		}

		/* When revoked don't try other sources/OCSP responders */
		if (is_revoked && *is_revoked)
			goto err;

		/* If the status is unknown, try the next available responder */
	}

err:


	if (ocsp_responders)
		X509_email_free(ocsp_responders);


	return ret;
}


int verify_callback(int status, X509_STORE_CTX *store)
{
	char szdbgdata1[256];
	char szdbgdata2[256];
	int ret, is_revoked = 0;

	SSL* ssl = X509_STORE_CTX_get_ex_data(store, SSL_get_ex_data_X509_STORE_CTX_idx());
	X509 *cert = X509_STORE_CTX_get_current_cert(store);
	STACK_OF(X509) *untrusted_peer_certs;
	nsd_ossl_t *pThis = (nsd_ossl_t*) SSL_get_ex_data(ssl, 0);
	assert(pThis != NULL);


	dbgprintf("verify_callback: status %d\n", status);

	if(status == 0) {
		/* Retrieve all needed pointers */
		int depth = X509_STORE_CTX_get_error_depth(store);
		int err = X509_STORE_CTX_get_error(store);
		int iVerifyMode = SSL_get_verify_mode(ssl);

		dbgprintf("verify_callback: Certificate validation failed, Mode (%d)!\n", iVerifyMode);

		X509_NAME_oneline(X509_get_issuer_name(cert), szdbgdata1, sizeof(szdbgdata1));
		X509_NAME_oneline(RSYSLOG_X509_NAME_oneline(cert), szdbgdata2, sizeof(szdbgdata2));

		if (iVerifyMode != SSL_VERIFY_NONE) {
			/* Handle expired Certificates **/
			if (err == X509_V_OK || err == X509_V_ERR_CERT_HAS_EXPIRED) {
				if (pThis->permitExpiredCerts == OSSL_EXPIRED_PERMIT) {
					dbgprintf("verify_callback: EXPIRED cert but PERMITTED at depth: %d \n\t"
						"issuer  = %s\n\t"
						"subject = %s\n\t"
						"err %d:%s\n", depth, szdbgdata1, szdbgdata2,
						err, X509_verify_cert_error_string(err));

					/* Set Status to OK*/
					status = 1;
				}
				else if (pThis->permitExpiredCerts == OSSL_EXPIRED_WARN) {
					LogMsg(0, RS_RET_NO_ERRCODE, LOG_WARNING,
						"Certificate EXPIRED warning at depth: %d \n\t"
						"issuer  = %s\n\t"
						"subject = %s\n\t"
						"err %d:%s",
						depth, szdbgdata1, szdbgdata2,
						err, X509_verify_cert_error_string(err));

					/* Set Status to OK*/
					status = 1;
				}
				else /* also default - if (pThis->permitExpiredCerts == OSSL_EXPIRED_DENY)*/ {
					LogMsg(0, RS_RET_NO_ERRCODE, LOG_ERR,
						"Certificate EXPIRED at depth: %d \n\t"
						"issuer  = %s\n\t"
						"subject = %s\n\t"
						"err %d:%s",
						depth, szdbgdata1, szdbgdata2,
						err, X509_verify_cert_error_string(err));
				}
			} else {
				/* all other error codes cause failure */
				LogMsg(0, RS_RET_NO_ERRCODE, LOG_ERR,
					"Certificate error at depth: %d \n\t"
					"issuer  = %s\n\t"
					"subject = %s\n\t"
					"err %d:%s",
					depth, szdbgdata1, szdbgdata2, err, X509_verify_cert_error_string(err));
			}
		} else {
			/* do not verify certs in ANON mode, just log into debug */
			dbgprintf("verify_callback: Certificate validation DISABLED but Error at depth: %d \n\t"
				"issuer  = %s\n\t"
				"subject = %s\n\t"
				"err %d:%s\n", depth, szdbgdata1, szdbgdata2,
				err, X509_verify_cert_error_string(err));

			/* Set Status to OK*/
			status = 1;
		}
	}

	if (status == 0)
		return 0; /* Verification failed */

	/*
	 * Certificate revocation checks
	 */
	untrusted_peer_certs = X509_STORE_CTX_get1_chain(store);

	/* 1. OCSP */
	ret = ocsp_check(cert, pThis->trusted_issuers, untrusted_peer_certs, pThis->device, &is_revoked);
	if (ret == 1) {
		/* Status is OK */
		status = 1;
		goto done;
	} else if (ret == 2) {
		/* No OCSP URL, give CRL a chance */
		status = 1;
	} else if (is_revoked) {
		/* Cert is revoked, fail verification */
		status = 0;
		goto done;
	} else {
		/* If OCSP failed, but cert was not revoked, then the Status might be still OK.
		 * Try alternative sources (e.g. CRL), in compliance with RFC 6960, Chapter 2.2, Page 7-8.
		 */
		status = 0;
	}

	/* 2. CRL */

	/* CRL support is not implemented.
	 * This stub will fail if the cert holds a CRL Distribution Point. */
	ret = crl_check(cert, &is_revoked);
	if (ret == 1) {
		/* Status is OK */
		status = 1;
	} else if (ret == 2) {
		/* No CRL, keep the existing status */
		;
	} else {
		/* Cert is revoked, or check failed -> fail verification */
		status = 0;
	}

done:

	sk_X509_pop_free(untrusted_peer_certs, X509_free);


	return status;
}

#if OPENSSL_VERSION_NUMBER >= 0x30000000L && !defined(LIBRESSL_VERSION_NUMBER)
long BIO_debug_callback_ex(BIO *bio, int cmd, const char __attribute__((unused)) *argp,
			   size_t __attribute__((unused)) len, int argi, long __attribute__((unused)) argl,
			   int ret, size_t __attribute__((unused)) *processed)
#else
long BIO_debug_callback(BIO *bio, int cmd, const char __attribute__((unused)) *argp,
			int argi, long __attribute__((unused)) argl, long ret)
#endif
{
	long ret2 = ret; // Helper value to avoid printf compile errors long<>int
	long r = 1;
	if (BIO_CB_RETURN & cmd)
		r = ret;
	dbgprintf("openssl debugmsg: BIO[%p]: ", (void *)bio);
	switch (cmd) {
	case BIO_CB_FREE:
		dbgprintf("Free - %s\n", RSYSLOG_BIO_method_name(bio));
		break;
/* Disabled due API changes for OpenSSL 1.1.0+ */
#if OPENSSL_VERSION_NUMBER < 0x10100000L
	case BIO_CB_READ:
		if (bio->method->type & BIO_TYPE_DESCRIPTOR)
			dbgprintf("read(%d,%lu) - %s fd=%d\n",
				RSYSLOG_BIO_number_read(bio), (unsigned long)argi,
				RSYSLOG_BIO_method_name(bio), RSYSLOG_BIO_number_read(bio));
		else
			dbgprintf("read(%d,%lu) - %s\n", RSYSLOG_BIO_number_read(bio),
					(unsigned long)argi, RSYSLOG_BIO_method_name(bio));
		break;
	case BIO_CB_WRITE:
		if (bio->method->type & BIO_TYPE_DESCRIPTOR)
			dbgprintf("write(%d,%lu) - %s fd=%d\n",
				RSYSLOG_BIO_number_written(bio), (unsigned long)argi,
				RSYSLOG_BIO_method_name(bio), RSYSLOG_BIO_number_written(bio));
		else
			dbgprintf("write(%d,%lu) - %s\n",
					RSYSLOG_BIO_number_written(bio),
					(unsigned long)argi,
					RSYSLOG_BIO_method_name(bio));
		break;
#else
	case BIO_CB_READ:
		dbgprintf("read %s\n", RSYSLOG_BIO_method_name(bio));
		break;
	case BIO_CB_WRITE:
		dbgprintf("write %s\n", RSYSLOG_BIO_method_name(bio));
		break;
#endif
	case BIO_CB_PUTS:
		dbgprintf("puts() - %s\n", RSYSLOG_BIO_method_name(bio));
		break;
	case BIO_CB_GETS:
		dbgprintf("gets(%lu) - %s\n", (unsigned long)argi,
			RSYSLOG_BIO_method_name(bio));
		break;
	case BIO_CB_CTRL:
		dbgprintf("ctrl(%lu) - %s\n", (unsigned long)argi,
			RSYSLOG_BIO_method_name(bio));
		break;
	case BIO_CB_RETURN | BIO_CB_READ:
		dbgprintf("read return %ld\n", ret2);
		break;
	case BIO_CB_RETURN | BIO_CB_WRITE:
		dbgprintf("write return %ld\n", ret2);
		break;
	case BIO_CB_RETURN | BIO_CB_GETS:
		dbgprintf("gets return %ld\n", ret2);
		break;
	case BIO_CB_RETURN | BIO_CB_PUTS:
		dbgprintf("puts return %ld\n", ret2);
		break;
	case BIO_CB_RETURN | BIO_CB_CTRL:
		dbgprintf("ctrl return %ld\n", ret2);
		break;
	default:
		dbgprintf("bio callback - unknown type (%d)\n", cmd);
		break;
	}

	return (r);
}


/* Convert a fingerprint to printable data. The  conversion is carried out
 * according IETF I-D syslog-transport-tls-12. The fingerprint string is
 * returned in a new cstr object. It is the caller's responsibility to
 * destruct that object.
 * rgerhards, 2008-05-08
 */
static rsRetVal
GenFingerprintStr(uchar *pFingerprint, size_t sizeFingerprint, cstr_t **ppStr)
{
	cstr_t *pStr = NULL;
	uchar buf[4];
	size_t i;
	DEFiRet;

	CHKiRet(rsCStrConstruct(&pStr));
	CHKiRet(rsCStrAppendStrWithLen(pStr, (uchar*)"SHA1", 4));
	for(i = 0 ; i < sizeFingerprint ; ++i) {
		snprintf((char*)buf, sizeof(buf), ":%2.2X", pFingerprint[i]);
		CHKiRet(rsCStrAppendStrWithLen(pStr, buf, 3));
	}
	cstrFinalize(pStr);

	*ppStr = pStr;

finalize_it:
	if(iRet != RS_RET_OK) {
		if(pStr != NULL)
			rsCStrDestruct(&pStr);
	}
	RETiRet;
}


/* globally initialize OpenSSL  */
static rsRetVal
osslGlblInit(void)
{
	DEFiRet;
	DBGPRINTF("openssl: entering osslGlblInit\n");

	/* Setup OpenSSL library */
	if((opensslh_THREAD_setup() == 0) || !SSL_library_init()) {
		LogError(0, RS_RET_NO_ERRCODE, "Error: OpenSSL initialization failed!");
	}

	/* Load readable error strings */
	SSL_load_error_strings();
#if OPENSSL_VERSION_NUMBER >= 0x30000000L && !defined(LIBRESSL_VERSION_NUMBER)
	/*
	* ERR_load_*(), ERR_func_error_string(), ERR_get_error_line(), ERR_get_error_line_data(), ERR_get_state()
	* OpenSSL now loads error strings automatically so these functions are not needed.
	* SEE FOR MORE:
	*	https://www.openssl.org/docs/manmaster/man7/migration_guide.html
	*
	*/
#else
	/* Load error strings into mem*/
	ERR_load_BIO_strings();
	ERR_load_crypto_strings();
#endif
	RETiRet;
}

/* try to receive a record from the remote peer. This works with
 * our own abstraction and handles local buffering and EAGAIN.
 * See details on local buffering in Rcv(9 header-comment.
 * This function MUST only be called when the local buffer is
 * empty. Calling it otherwise will cause losss of current buffer
 * data.
 * rgerhards, 2008-06-24
 */
rsRetVal
osslRecordRecv(nsd_ossl_t *pThis)
{
	ssize_t lenRcvd;
	DEFiRet;
	int err;
	int local_errno;

	ISOBJ_TYPE_assert(pThis, nsd_ossl);
	DBGPRINTF("osslRecordRecv: start\n");

	lenRcvd = SSL_read(pThis->ssl, pThis->pszRcvBuf, NSD_OSSL_MAX_RCVBUF);
	if(lenRcvd > 0) {
		DBGPRINTF("osslRecordRecv: SSL_read received %zd bytes\n", lenRcvd);
		pThis->lenRcvBuf = lenRcvd;
		pThis->ptrRcvBuf = 0;

		/* Check for additional data in SSL buffer */
		int iBytesLeft = SSL_pending(pThis->ssl);
		if (iBytesLeft > 0 ){
			DBGPRINTF("osslRecordRecv: %d Bytes pending after SSL_Read, expand buffer.\n", iBytesLeft);
			/* realloc buffer size and preserve char content */
			char *const newbuf = realloc(pThis->pszRcvBuf, NSD_OSSL_MAX_RCVBUF+iBytesLeft);
			CHKmalloc(newbuf);
			pThis->pszRcvBuf = newbuf;

			/* 2nd read will read missing bytes from the current SSL Packet */
			lenRcvd = SSL_read(pThis->ssl, pThis->pszRcvBuf+NSD_OSSL_MAX_RCVBUF, iBytesLeft);
			if(lenRcvd > 0) {
				DBGPRINTF("osslRecordRecv: 2nd SSL_read received %zd bytes\n",
					(NSD_OSSL_MAX_RCVBUF+lenRcvd));
				pThis->lenRcvBuf = NSD_OSSL_MAX_RCVBUF+lenRcvd;
			} else {
				goto sslerr;
			}
		}
	} else {
sslerr:
		err = SSL_get_error(pThis->ssl, lenRcvd);
		if(	err == SSL_ERROR_ZERO_RETURN ) {
			DBGPRINTF("osslRecordRecv: SSL_ERROR_ZERO_RETURN received, connection may closed already\n");
			ABORT_FINALIZE(RS_RET_RETRY);
		}
		else if(err != SSL_ERROR_WANT_READ &&
			err != SSL_ERROR_WANT_WRITE) {
			DBGPRINTF("osslRecordRecv: SSL_get_error #1 = %d, lenRcvd=%zd\n", err, lenRcvd);
			/* Save errno */
			local_errno = errno;

			/* Output OpenSSL error*/
			osslLastSSLErrorMsg(lenRcvd, pThis->ssl, LOG_ERR, "osslRecordRecv");
			/* Check for underlaying socket errors **/
			if (local_errno == ECONNRESET) {
				DBGPRINTF("osslRecordRecv: Errno %d, connection reset by peer\n", local_errno);
				ABORT_FINALIZE(RS_RET_CLOSED);
			} else {
				DBGPRINTF("osslRecordRecv: Errno %d\n", local_errno);
				ABORT_FINALIZE(RS_RET_NO_ERRCODE);
			}
		} else {
			DBGPRINTF("osslRecordRecv: SSL_get_error #2 = %d, lenRcvd=%zd\n", err, lenRcvd);
			pThis->rtryCall =  osslRtry_recv;
			pThis->rtryOsslErr = err; /* Store SSL ErrorCode into*/
			ABORT_FINALIZE(RS_RET_RETRY);
		}
	}

// TODO: Check if MORE retry logic needed?

finalize_it:
	dbgprintf("osslRecordRecv return. nsd %p, iRet %d, lenRcvd %zd, lenRcvBuf %d, ptrRcvBuf %d\n",
	pThis, iRet, lenRcvd, pThis->lenRcvBuf, pThis->ptrRcvBuf);
	RETiRet;
}

static rsRetVal
osslInitSession(nsd_ossl_t *pThis, osslSslState_t osslType) /* , nsd_ossl_t *pServer) */
{
	DEFiRet;
	BIO *conn;
	char pristringBuf[4096];
	nsd_ptcp_t *pPtcp = (nsd_ptcp_t*) pThis->pTcp;

	if(!(pThis->ssl = SSL_new(pThis->ctx))) {
		pThis->ssl = NULL;
		osslLastSSLErrorMsg(0, pThis->ssl, LOG_ERR, "osslInitSession");
		ABORT_FINALIZE(RS_RET_NO_ERRCODE);
	}

	// Set SSL_MODE_AUTO_RETRY to SSL obj
	SSL_set_mode(pThis->ssl, SSL_MODE_AUTO_RETRY);

	if (pThis->authMode != OSSL_AUTH_CERTANON) {
		dbgprintf("osslInitSession: enable certificate checking (Mode=%d, VerifyDepth=%d)\n",
			pThis->authMode, pThis->DrvrVerifyDepth);
		/* Enable certificate valid checking */
		SSL_set_verify(pThis->ssl, SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT, verify_callback);
		if (pThis->DrvrVerifyDepth != 0) {
			SSL_set_verify_depth(pThis->ssl, pThis->DrvrVerifyDepth);
		}
	} else 	if (pThis->gnutlsPriorityString == NULL) {
		/* Allow ANON Ciphers only in ANON Mode and if no custom priority string is defined */
		#if OPENSSL_VERSION_NUMBER >= 0x10100000L && !defined(LIBRESSL_VERSION_NUMBER)
		 /* NOTE: do never use: +eNULL, it DISABLES encryption! */
		strncpy(pristringBuf, "ALL:+COMPLEMENTOFDEFAULT:+ADH:+ECDH:+aNULL@SECLEVEL=0",
			sizeof(pristringBuf));
		#else
		strncpy(pristringBuf, "ALL:+COMPLEMENTOFDEFAULT:+ADH:+ECDH:+aNULL",
			sizeof(pristringBuf));
		#endif

		dbgprintf("osslInitSession: setting anon ciphers: %s\n", pristringBuf);
		if ( SSL_set_cipher_list(pThis->ssl, pristringBuf) == 0 ){
			dbgprintf("osslInitSession: Error setting ciphers '%s'\n", pristringBuf);
			ABORT_FINALIZE(RS_RET_SYS_ERR);
		}
	}

	/* Create BIO from ptcp socket! */
	conn = BIO_new_socket(pPtcp->sock, BIO_CLOSE /*BIO_NOCLOSE*/);
	dbgprintf("osslInitSession: Init conn BIO[%p] done\n", (void *)conn);

	/* Set debug Callback for conn BIO as well! */
#if OPENSSL_VERSION_NUMBER >= 0x30000000L && !defined(LIBRESSL_VERSION_NUMBER)
	BIO_set_callback_ex(conn, BIO_debug_callback_ex);
#else
	BIO_set_callback(conn, BIO_debug_callback);
#endif

	/* TODO: still needed? Set to NON blocking ! */
	BIO_set_nbio( conn, 1 );
	SSL_set_bio(pThis->ssl, conn, conn);

	if (osslType == osslServer) {
		/* Server Socket */
		SSL_set_accept_state(pThis->ssl); /* sets ssl to work in server mode. */
		pThis->sslState = osslServer; /*set Server state */
	} else {
		/* Client Socket */
		SSL_set_connect_state(pThis->ssl); /*sets ssl to work in client mode.*/
		pThis->sslState = osslClient; /*set Client state */
	}
	pThis->bHaveSess = 1;

	/* we are done */
	FINALIZE;

finalize_it:
	RETiRet;
}


/* Check the peer's ID in fingerprint auth mode.
 * rgerhards, 2008-05-22
 */
static rsRetVal
osslChkPeerFingerprint(nsd_ossl_t *pThis, X509 *pCert)
{
	DEFiRet;
	unsigned int n;
	uchar *fromHostIP = NULL;
	uchar fingerprint[20 /*EVP_MAX_MD_SIZE**/];
	size_t size;
	cstr_t *pstrFingerprint = NULL;
	int bFoundPositiveMatch;
	permittedPeers_t *pPeer;
	const EVP_MD *fdig = EVP_sha1();

	ISOBJ_TYPE_assert(pThis, nsd_ossl);

	/* obtain the SHA1 fingerprint */
	size = sizeof(fingerprint);
	if (!X509_digest(pCert,fdig,fingerprint,&n)) {
		dbgprintf("osslChkPeerFingerprint: error X509cert is not valid!\n");
		ABORT_FINALIZE(RS_RET_INVALID_FINGERPRINT);
	}

	CHKiRet(GenFingerprintStr(fingerprint, size, &pstrFingerprint));
	dbgprintf("osslChkPeerFingerprint: peer's certificate SHA1 fingerprint: %s\n",
		cstrGetSzStrNoNULL(pstrFingerprint));

	/* now search through the permitted peers to see if we can find a permitted one */
	bFoundPositiveMatch = 0;
	pPeer = pThis->pPermPeers;
	while(pPeer != NULL && !bFoundPositiveMatch) {
		if(!rsCStrSzStrCmp(pstrFingerprint, pPeer->pszID, strlen((char*) pPeer->pszID))) {
			dbgprintf("osslChkPeerFingerprint: peer's certificate MATCH found: %s\n", pPeer->pszID);
			bFoundPositiveMatch = 1;
		} else {
			dbgprintf("osslChkPeerFingerprint: NOMATCH peer certificate: %s\n", pPeer->pszID);
			pPeer = pPeer->pNext;
		}
	}

	if(!bFoundPositiveMatch) {
		dbgprintf("osslChkPeerFingerprint: invalid peer fingerprint, not permitted to talk to it\n");
		if(pThis->bReportAuthErr == 1) {
			nsd_ptcp.GetRemoteHName((nsd_t*)pThis->pTcp, &fromHostIP);
			errno = 0;
			LogMsg(0, RS_RET_INVALID_FINGERPRINT, LOG_WARNING,
				"nsd_ossl:TLS session terminated with remote syslog server '%s': "
				"Fingerprint check failed, not permitted to talk to %s",
				fromHostIP, cstrGetSzStrNoNULL(pstrFingerprint));
			pThis->bReportAuthErr = 0;
		}
		ABORT_FINALIZE(RS_RET_INVALID_FINGERPRINT);
	}

finalize_it:
	if (fromHostIP != NULL) {
		free(fromHostIP);
	}
	if(pstrFingerprint != NULL)
		cstrDestruct(&pstrFingerprint);
	RETiRet;
}


/* Perform a match on ONE peer name obtained from the certificate. This name
 * is checked against the set of configured credentials. *pbFoundPositiveMatch is
 * set to 1 if the ID matches. *pbFoundPositiveMatch must have been initialized
 * to 0 by the caller (this is a performance enhancement as we expect to be
 * called multiple times).
 * TODO: implemet wildcards?
 * rgerhards, 2008-05-26
 */
static rsRetVal
osslChkOnePeerName(nsd_ossl_t *pThis, X509 *pCert, uchar *pszPeerID, int *pbFoundPositiveMatch)
{
	permittedPeers_t *pPeer;
#if OPENSSL_VERSION_NUMBER >= 0x10002000L
	int osslRet;
#endif
	char *x509name = NULL;
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, nsd_ossl);
	assert(pszPeerID != NULL);
	assert(pbFoundPositiveMatch != NULL);

	/* Obtain Namex509 name from subject */
	x509name = X509_NAME_oneline(RSYSLOG_X509_NAME_oneline(pCert), NULL, 0);

	if(pThis->pPermPeers) { /* do we have configured peer IDs? */
		pPeer = pThis->pPermPeers;
		while(pPeer != NULL) {
			CHKiRet(net.PermittedPeerWildcardMatch(pPeer, pszPeerID, pbFoundPositiveMatch));
			if(*pbFoundPositiveMatch)
				break;

#if OPENSSL_VERSION_NUMBER >= 0x10002000L
			/* if we did not succeed so far, try ossl X509_check_host
			* ( Includes check against SubjectAlternativeName )
			*/
			osslRet = X509_check_host(	pCert, (const char*)pPeer->pszID,
							strlen((const char*)pPeer->pszID), 0, NULL);
			if ( osslRet == 1 ) {
				/* Found Peer cert in allowed Peerslist */
				dbgprintf("osslChkOnePeerName: Client ('%s') is allowed (X509_check_host)\n",
					x509name);
				*pbFoundPositiveMatch = 1;
				break;
			} else if ( osslRet < 0 ) {
				osslLastSSLErrorMsg(osslRet, pThis->ssl, LOG_ERR, "osslChkOnePeerName");
				ABORT_FINALIZE(RS_RET_NO_ERRCODE);
			}
#endif
			/* Check next peer */
			pPeer = pPeer->pNext;
		}
	} else {
		/* we do not have configured peer IDs, so we use defaults */
		if(   pThis->pszConnectHost
		   && !strcmp((char*)pszPeerID, (char*)pThis->pszConnectHost)) {
			*pbFoundPositiveMatch = 1;
		}
	}
finalize_it:
	if (x509name != NULL){
		OPENSSL_free(x509name);
	}

	RETiRet;
}


/* Check the peer's ID in name auth mode.
 */
static rsRetVal
osslChkPeerName(nsd_ossl_t *pThis, X509 *pCert)
{
	uchar lnBuf[256];
	int bFoundPositiveMatch;
	cstr_t *pStr = NULL;
	char *x509name = NULL;
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, nsd_ossl);

	bFoundPositiveMatch = 0;
	CHKiRet(rsCStrConstruct(&pStr));

	/* Obtain Namex509 name from subject */
	x509name = X509_NAME_oneline(RSYSLOG_X509_NAME_oneline(pCert), NULL, 0);

	dbgprintf("osslChkPeerName: checking - peername '%s'\n", x509name);
	snprintf((char*)lnBuf, sizeof(lnBuf), "name: %s; ", x509name);
	CHKiRet(rsCStrAppendStr(pStr, lnBuf));
	CHKiRet(osslChkOnePeerName(pThis, pCert, (uchar*)x509name, &bFoundPositiveMatch));

	if(!bFoundPositiveMatch) {
		dbgprintf("osslChkPeerName: invalid peername, not permitted to talk to it\n");
		if(pThis->bReportAuthErr == 1) {
			cstrFinalize(pStr);
			errno = 0;
			LogMsg(0, RS_RET_INVALID_FINGERPRINT, LOG_WARNING,
				"nsd_ossl:TLS session terminated with remote syslog server: "
				"peer name not authorized, not permitted to talk to %s",
				cstrGetSzStrNoNULL(pStr));
			pThis->bReportAuthErr = 0;
		}
		ABORT_FINALIZE(RS_RET_INVALID_FINGERPRINT);
	} else {
		dbgprintf("osslChkPeerName: permitted to talk!\n");
	}

finalize_it:
	if (x509name != NULL){
		OPENSSL_free(x509name);
	}

	if(pStr != NULL)
		rsCStrDestruct(&pStr);
	RETiRet;
}


/* check the ID of the remote peer - used for both fingerprint and
 * name authentication.
 */
static rsRetVal
osslChkPeerID(nsd_ossl_t *pThis)
{
	DEFiRet;
	X509* certpeer;
	uchar *fromHostIP = NULL;

	ISOBJ_TYPE_assert(pThis, nsd_ossl);

	/* Get peer certificate from SSL */
	certpeer = SSL_get_peer_certificate(pThis->ssl);
	if ( certpeer == NULL ) {
		if(pThis->bReportAuthErr == 1) {
			errno = 0;
			pThis->bReportAuthErr = 0;
			nsd_ptcp.GetRemoteHName((nsd_t*)pThis->pTcp, &fromHostIP);
			LogMsg(0, RS_RET_TLS_NO_CERT, LOG_WARNING,
				"nsd_ossl:TLS session terminated with remote syslog server '%s': "
				"Peer check failed, peer did not provide a certificate.", fromHostIP);
		}
		ABORT_FINALIZE(RS_RET_TLS_NO_CERT);
	}

	/* Now we see which actual authentication code we must call.  */
	if(pThis->authMode == OSSL_AUTH_CERTFINGERPRINT) {
		CHKiRet(osslChkPeerFingerprint(pThis, certpeer));
	} else {
		assert(pThis->authMode == OSSL_AUTH_CERTNAME);
		CHKiRet(osslChkPeerName(pThis, certpeer));
	}

finalize_it:
	if (fromHostIP != NULL) {
		free(fromHostIP);
	}
	RETiRet;
}


/* Verify the validity of the remote peer's certificate.
 */
static rsRetVal
osslChkPeerCertValidity(nsd_ossl_t *pThis)
{
	DEFiRet;
	int iVerErr = X509_V_OK;
	uchar *fromHostIP = NULL;

	ISOBJ_TYPE_assert(pThis, nsd_ossl);

	iVerErr = SSL_get_verify_result(pThis->ssl);
	if (iVerErr != X509_V_OK) {
		nsd_ptcp.GetRemoteHName((nsd_t*)pThis->pTcp, &fromHostIP);
		if (iVerErr == X509_V_ERR_CERT_HAS_EXPIRED) {
			if (pThis->permitExpiredCerts == OSSL_EXPIRED_DENY) {
				LogMsg(0, RS_RET_CERT_EXPIRED, LOG_INFO,
					"nsd_ossl:TLS session terminated with remote syslog server '%s': "
					"not permitted to talk to peer, "
					"Certificate expired: %s",
					fromHostIP, X509_verify_cert_error_string(iVerErr));
				ABORT_FINALIZE(RS_RET_CERT_EXPIRED);
			} else if (pThis->permitExpiredCerts == OSSL_EXPIRED_WARN) {
				LogMsg(0, RS_RET_NO_ERRCODE, LOG_WARNING,
					"nsd_ossl:CertValidity check - warning talking to peer '%s': "
					"certificate expired: %s",
					fromHostIP, X509_verify_cert_error_string(iVerErr));
			} else {
				dbgprintf("osslChkPeerCertValidity: talking to peer '%s': "
					"certificate expired: %s\n",
					fromHostIP, X509_verify_cert_error_string(iVerErr));
			}/* Else do nothing */
		} else {
			LogMsg(0, RS_RET_CERT_INVALID, LOG_INFO,
				"nsd_ossl:TLS session terminated with remote syslog server '%s': "
				"not permitted to talk to peer, "
				"Certificate validation failed: %s",
				fromHostIP, X509_verify_cert_error_string(iVerErr));
			ABORT_FINALIZE(RS_RET_CERT_INVALID);
		}
	} else {
		dbgprintf("osslChkPeerCertValidity: client certificate validation success: %s\n",
			X509_verify_cert_error_string(iVerErr));
	}

finalize_it:
	if (fromHostIP != NULL) {
		free(fromHostIP);
	}
	RETiRet;
}


/* check if it is OK to talk to the remote peer
 * rgerhards, 2008-05-21
 */
rsRetVal
osslChkPeerAuth(nsd_ossl_t *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, nsd_ossl);

	/* call the actual function based on current auth mode */
	switch(pThis->authMode) {
		case OSSL_AUTH_CERTNAME:
			/* if we check the name, we must ensure the cert is valid */
			dbgprintf("osslChkPeerAuth: Check peer certname[%p]\n", (void *)pThis->ssl);
			CHKiRet(osslChkPeerCertValidity(pThis));
			CHKiRet(osslChkPeerID(pThis));
			break;
		case OSSL_AUTH_CERTFINGERPRINT:
			dbgprintf("osslChkPeerAuth: Check peer fingerprint[%p]\n", (void *)pThis->ssl);
			CHKiRet(osslChkPeerID(pThis));
			break;
		case OSSL_AUTH_CERTVALID:
			dbgprintf("osslChkPeerAuth: Check peer valid[%p]\n", (void *)pThis->ssl);
			CHKiRet(osslChkPeerCertValidity(pThis));
			break;
		case OSSL_AUTH_CERTANON:
			FINALIZE;
			break;
	}

finalize_it:
	RETiRet;
}


/* globally de-initialize OpenSSL */
static rsRetVal
osslGlblExit(void)
{
	DEFiRet;
	DBGPRINTF("openssl: entering osslGlblExit\n");
	ENGINE_cleanup();
	ERR_free_strings();
	EVP_cleanup();
	CRYPTO_cleanup_all_ex_data();
	RETiRet;
}


/* end a OpenSSL session
 * The function checks if we have a session and ends it only if so. So it can
 * always be called, even if there currently is no session.
 */
static rsRetVal
osslEndSess(nsd_ossl_t *pThis)
{
	DEFiRet;
	uchar *fromHostIP = NULL;
	int ret;
	int err;

	/* try closing SSL Connection */
	if(pThis->bHaveSess) {
		DBGPRINTF("osslEndSess: closing SSL Session ...\n");
		ret = SSL_shutdown(pThis->ssl);
		nsd_ptcp.GetRemoteHName((nsd_t*)pThis->pTcp, &fromHostIP);
		if (ret <= 0) {
			err = SSL_get_error(pThis->ssl, ret);
			DBGPRINTF("osslEndSess: shutdown failed with err = %d\n", err);

			/* ignore those SSL Errors on shutdown */
			if(	err != SSL_ERROR_SYSCALL &&
					err != SSL_ERROR_ZERO_RETURN &&
					err != SSL_ERROR_WANT_READ &&
					err != SSL_ERROR_WANT_WRITE) {
				/* Output Warning only */
				osslLastSSLErrorMsg(ret, pThis->ssl, LOG_WARNING, "osslEndSess");
			}

			/* Shutdown not finished, call SSL_read to do a bidirectional shutdown, see doc for more:
			*	https://www.openssl.org/docs/man1.1.1/man3/SSL_shutdown.html
			*/
			char rcvBuf[NSD_OSSL_MAX_RCVBUF];
			int iBytesRet = SSL_read(pThis->ssl, rcvBuf, NSD_OSSL_MAX_RCVBUF);
			DBGPRINTF("osslEndSess: Forcing ssl shutdown SSL_read (%d) to do a bidirectional shutdown\n",
				iBytesRet);
			LogMsg(0, RS_RET_NO_ERRCODE, LOG_INFO, "nsd_ossl:"
			"TLS session terminated with remote syslog server '%s': End Session", fromHostIP);
			DBGPRINTF("osslEndSess: session closed (un)successfully \n");
		} else {
			LogMsg(0, RS_RET_NO_ERRCODE, LOG_INFO, "nsd_ossl:"
			"TLS session terminated with remote syslog server '%s': End Session", fromHostIP);
			DBGPRINTF("osslEndSess: session closed successfully \n");
		}

		/* Session closed */
		pThis->bHaveSess = 0;
	}

	if (fromHostIP != NULL) {
		free(fromHostIP);
	}
	RETiRet;
}
/* ---------------------------- end OpenSSL specifics ---------------------------- */


/* Standard-Constructor */
BEGINobjConstruct(nsd_ossl) /* be sure to specify the object type also in END macro! */
	iRet = nsd_ptcp.Construct(&pThis->pTcp);
	pThis->bReportAuthErr = 1;
ENDobjConstruct(nsd_ossl)


/* destructor for the nsd_ossl object */
PROTOTYPEobjDestruct(nsd_ossl);
BEGINobjDestruct(nsd_ossl) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(nsd_ossl)
	DBGPRINTF("nsd_ossl_destruct: [%p] Mode %d\n", pThis, pThis->iMode);
	if(pThis->iMode == 1) {
		osslEndSess(pThis);
	}
	/* Free SSL obj also if we do not have a session - or are NOT in TLS mode! */
	if (pThis->ssl != NULL) {
		DBGPRINTF("nsd_ossl_destruct: [%p] FREE pThis->ssl \n", pThis);
		SSL_free(pThis->ssl);
		pThis->ssl = NULL;
	}

	if(pThis->pTcp != NULL) {
		nsd_ptcp.Destruct(&pThis->pTcp);
	}

	free(pThis->pszConnectHost);
	free(pThis->pszRcvBuf);
	if(pThis->ctx != NULL && !pThis->ctx_is_copy) {
		SSL_CTX_free(pThis->ctx);
	}
	free((void*) pThis->pszCAFile);
	free((void*) pThis->pszKeyFile);
	free((void*) pThis->pszCertFile);
ENDobjDestruct(nsd_ossl)


/* Set the driver mode. For us, this has the following meaning:
 * 0 - work in plain tcp mode, without tls (e.g. before a STARTTLS)
 * 1 - work in TLS mode
 * rgerhards, 2008-04-28
 */
static rsRetVal
SetMode(nsd_t *pNsd, const int mode)
{
	DEFiRet;
	nsd_ossl_t *pThis = (nsd_ossl_t*) pNsd;

	ISOBJ_TYPE_assert((pThis), nsd_ossl);
	if(mode != 0 && mode != 1) {
		LogError(0, RS_RET_INVALID_DRVR_MODE, "error: driver mode %d not supported by"
				" ossl netstream driver", mode);
	}
	pThis->iMode = mode;

	RETiRet;
}

/* Set the authentication mode. For us, the following is supported:
 * anon - no certificate checks whatsoever (discouraged, but supported)
 * x509/certvalid - (just) check certificate validity
 * x509/fingerprint - certificate fingerprint
 * x509/name - cerfificate name check
 * mode == NULL is valid and defaults to x509/name
 * rgerhards, 2008-05-16
 */
static rsRetVal
SetAuthMode(nsd_t *const pNsd, uchar *const mode)
{
	DEFiRet;
	nsd_ossl_t *pThis = (nsd_ossl_t*) pNsd;

	ISOBJ_TYPE_assert((pThis), nsd_ossl);
	if(mode == NULL || !strcasecmp((char*)mode, "x509/name")) {
		pThis->authMode = OSSL_AUTH_CERTNAME;
	} else if(!strcasecmp((char*) mode, "x509/fingerprint")) {
		pThis->authMode = OSSL_AUTH_CERTFINGERPRINT;
	} else if(!strcasecmp((char*) mode, "x509/certvalid")) {
		pThis->authMode = OSSL_AUTH_CERTVALID;
	} else if(!strcasecmp((char*) mode, "anon")) {
		pThis->authMode = OSSL_AUTH_CERTANON;

	} else {
		LogError(0, RS_RET_VALUE_NOT_SUPPORTED, "error: authentication mode '%s' not supported by "
				"ossl netstream driver", mode);
		ABORT_FINALIZE(RS_RET_VALUE_NOT_SUPPORTED);
	}

	dbgprintf("SetAuthMode: Set Mode %s/%d\n", mode, pThis->authMode);

finalize_it:
	RETiRet;
}


/* Set the PermitExpiredCerts mode. For us, the following is supported:
 * on - fail if certificate is expired
 * off - ignore expired certificates
 * warn - warn if certificate is expired
 * alorbach, 2018-12-20
 */
static rsRetVal
SetPermitExpiredCerts(nsd_t *pNsd, uchar *mode)
{
	DEFiRet;
	nsd_ossl_t *pThis = (nsd_ossl_t*) pNsd;

	ISOBJ_TYPE_assert((pThis), nsd_ossl);
	/* default is set to off! */
	if(mode == NULL || !strcasecmp((char*)mode, "off")) {
		pThis->permitExpiredCerts = OSSL_EXPIRED_DENY;
	} else if(!strcasecmp((char*) mode, "warn")) {
		pThis->permitExpiredCerts = OSSL_EXPIRED_WARN;
	} else if(!strcasecmp((char*) mode, "on")) {
		pThis->permitExpiredCerts = OSSL_EXPIRED_PERMIT;
	} else {
		LogError(0, RS_RET_VALUE_NOT_SUPPORTED, "error: permitexpiredcerts mode '%s' not supported by "
				"ossl netstream driver", mode);
		ABORT_FINALIZE(RS_RET_VALUE_NOT_SUPPORTED);
	}

	dbgprintf("SetPermitExpiredCerts: Set Mode %s/%d\n", mode, pThis->permitExpiredCerts);

/* TODO: clear stored IDs! */

finalize_it:
	RETiRet;
}


/* Set permitted peers. It is depending on the auth mode if this are
 * fingerprints or names. -- rgerhards, 2008-05-19
 */
static rsRetVal
SetPermPeers(nsd_t *pNsd, permittedPeers_t *pPermPeers)
{
	DEFiRet;
	nsd_ossl_t *pThis = (nsd_ossl_t*) pNsd;

	ISOBJ_TYPE_assert((pThis), nsd_ossl);
	if(pPermPeers == NULL)
		FINALIZE;

	if(pThis->authMode != OSSL_AUTH_CERTFINGERPRINT && pThis->authMode != OSSL_AUTH_CERTNAME) {
		LogError(0, RS_RET_VALUE_NOT_IN_THIS_MODE, "authentication not supported by "
				"ossl netstream driver in the configured authentication mode - ignored");
		ABORT_FINALIZE(RS_RET_VALUE_NOT_IN_THIS_MODE);
	}
	pThis->pPermPeers = pPermPeers;

finalize_it:
	RETiRet;
}


/* Provide access to the underlying OS socket. This is primarily
 * useful for other drivers (like nsd_ossl) who utilize ourselfs
 * for some of their functionality. -- rgerhards, 2008-04-18
 */
static rsRetVal
SetSock(nsd_t *pNsd, int sock)
{
	DEFiRet;
	nsd_ossl_t *pThis = (nsd_ossl_t*) pNsd;
	ISOBJ_TYPE_assert((pThis), nsd_ossl);
	assert(sock >= 0);

	DBGPRINTF("SetSock for [%p]: Setting sock %d\n", pNsd, sock);
	nsd_ptcp.SetSock(pThis->pTcp, sock);

	RETiRet;
}


/* Keep Alive Options
 */
static rsRetVal
SetKeepAliveIntvl(nsd_t *pNsd, int keepAliveIntvl)
{
	DEFiRet;
	nsd_ossl_t *pThis = (nsd_ossl_t*) pNsd;

	ISOBJ_TYPE_assert((pThis), nsd_ossl);
	assert(keepAliveIntvl >= 0);

	dbgprintf("SetKeepAliveIntvl: keepAliveIntvl=%d\n", keepAliveIntvl);
	nsd_ptcp.SetKeepAliveIntvl(pThis->pTcp, keepAliveIntvl);

	RETiRet;
}


/* Keep Alive Options
 */
static rsRetVal
SetKeepAliveProbes(nsd_t *pNsd, int keepAliveProbes)
{
	DEFiRet;
	nsd_ossl_t *pThis = (nsd_ossl_t*) pNsd;

	ISOBJ_TYPE_assert((pThis), nsd_ossl);
	assert(keepAliveProbes >= 0);

	dbgprintf("SetKeepAliveProbes: keepAliveProbes=%d\n", keepAliveProbes);
	nsd_ptcp.SetKeepAliveProbes(pThis->pTcp, keepAliveProbes);

	RETiRet;
}


/* Keep Alive Options
 */
static rsRetVal
SetKeepAliveTime(nsd_t *pNsd, int keepAliveTime)
{
	DEFiRet;
	nsd_ossl_t *pThis = (nsd_ossl_t*) pNsd;

	ISOBJ_TYPE_assert((pThis), nsd_ossl);
	assert(keepAliveTime >= 0);

	dbgprintf("SetKeepAliveTime: keepAliveTime=%d\n", keepAliveTime);
	nsd_ptcp.SetKeepAliveTime(pThis->pTcp, keepAliveTime);

	RETiRet;
}


/* abort a connection. This is meant to be called immediately
 * before the Destruct call. -- rgerhards, 2008-03-24
 */
static rsRetVal
Abort(nsd_t *pNsd)
{
	nsd_ossl_t *pThis = (nsd_ossl_t*) pNsd;
	DEFiRet;

	ISOBJ_TYPE_assert((pThis), nsd_ossl);

	if(pThis->iMode == 0) {
		nsd_ptcp.Abort(pThis->pTcp);
	}

	RETiRet;
}


/* initialize openssl context; called on
 * - listener creation
 * - outbound connection creation
 * Once created, the ctx object is used by-subobjects (accepted inbound connections)
 */
static rsRetVal
osslInit_ctx(nsd_ossl_t *const pThis)
{
	DEFiRet;
	int bHaveCA;
	int bHaveCert;
	int bHaveKey;
	const char *caFile, *certFile, *keyFile;
	/* Setup certificates */
	caFile = (char*) ((pThis->pszCAFile == NULL) ? glbl.GetDfltNetstrmDrvrCAF(runConf) : pThis->pszCAFile);
	if(caFile == NULL) {
		LogMsg(0, RS_RET_CA_CERT_MISSING, LOG_WARNING,
			"Warning: CA certificate is not set");
		bHaveCA = 0;
	} else {
		bHaveCA	= 1;
	}
	certFile = (char*) ((pThis->pszCertFile == NULL) ?
		glbl.GetDfltNetstrmDrvrCertFile(runConf) : pThis->pszCertFile);
	if(certFile == NULL) {
		LogMsg(0, RS_RET_CERT_MISSING, LOG_WARNING,
			"Warning: Certificate file is not set");
		bHaveCert = 0;
	} else {
		bHaveCert = 1;
	}
	keyFile = (char*) ((pThis->pszKeyFile == NULL) ? glbl.GetDfltNetstrmDrvrKeyFile(runConf) : pThis->pszKeyFile);
	if(keyFile == NULL) {
		LogMsg(0, RS_RET_CERTKEY_MISSING, LOG_WARNING,
			"Warning: Key file is not set");
		bHaveKey = 0;
	} else {
		bHaveKey = 1;
	}

	/* Create main CTX Object */
	pThis->ctx = SSL_CTX_new(SSLv23_method());
	if(bHaveCA == 1 && SSL_CTX_load_verify_locations(pThis->ctx, caFile, NULL) != 1) {
		LogError(0, RS_RET_TLS_CERT_ERR, "Error: CA certificate could not be accessed. "
				"Check at least: 1) file path is correct, 2) file exist, "
				"3) permissions are correct, 4) file content is correct. "
				"Open ssl error info may follow in next messages");
		osslLastSSLErrorMsg(0, NULL, LOG_ERR, "osslGlblInit");
		ABORT_FINALIZE(RS_RET_TLS_CERT_ERR);
	}
	if(bHaveCert == 1 && SSL_CTX_use_certificate_chain_file(pThis->ctx, certFile) != 1) {
		LogError(0, RS_RET_TLS_CERT_ERR, "Error: Certificate file could not be accessed. "
				"Check at least: 1) file path is correct, 2) file exist, "
				"3) permissions are correct, 4) file content is correct. "
				"Open ssl error info may follow in next messages");
		osslLastSSLErrorMsg(0, NULL, LOG_ERR, "osslGlblInit");
		ABORT_FINALIZE(RS_RET_TLS_CERT_ERR);
	}
	if(bHaveKey == 1 && SSL_CTX_use_PrivateKey_file(pThis->ctx, keyFile, SSL_FILETYPE_PEM) != 1) {
		LogError(0, RS_RET_TLS_KEY_ERR , "Error: Key could not be accessed. "
				"Check at least: 1) file path is correct, 2) file exist, "
				"3) permissions are correct, 4) file content is correct. "
				"Open ssl error info may follow in next messages");
		osslLastSSLErrorMsg(0, NULL, LOG_ERR, "osslGlblInit");
		ABORT_FINALIZE(RS_RET_TLS_KEY_ERR);
	}

	/* Set CTX Options */
	SSL_CTX_set_options(pThis->ctx, SSL_OP_NO_SSLv2);		/* Disable insecure SSLv2 Protocol */
	SSL_CTX_set_options(pThis->ctx, SSL_OP_NO_SSLv3);		/* Disable insecure SSLv3 Protocol */
	SSL_CTX_sess_set_cache_size(pThis->ctx,1024);			/* TODO: make configurable? */

	/* Set default VERIFY Options for OpenSSL CTX - and CALLBACK */
	SSL_CTX_set_verify(pThis->ctx, SSL_VERIFY_NONE, verify_callback);

	SSL_CTX_set_timeout(pThis->ctx, 30);	/* Default Session Timeout, TODO: Make configureable */
	SSL_CTX_set_mode(pThis->ctx, SSL_MODE_AUTO_RETRY);

	applyGnutlsPriorityString(pThis);

#if OPENSSL_VERSION_NUMBER >= 0x10002000L
#	if OPENSSL_VERSION_NUMBER <= 0x101010FFL
	/* Enable Support for automatic EC temporary key parameter selection. */
	SSL_CTX_set_ecdh_auto(pThis->ctx, 1);
#	else
	/*
	* SSL_CTX_set_ecdh_auto and SSL_CTX_set_tmp_ecdh are depreceated in higher
	* OpenSSL Versions, so we no more need them - see for more:
	* https://www.openssl.org/docs/manmaster/man3/SSL_CTX_set_ecdh_auto.html
	*/
#	endif
#else
	dbgprintf("osslAnonInit: openssl to old, cannot use SSL_CTX_set_ecdh_auto."
		"Using SSL_CTX_set_tmp_ecdh with NID_X9_62_prime256v1/() instead.\n");
	SSL_CTX_set_tmp_ecdh(pThis->ctx, EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
#endif
finalize_it:
	RETiRet;
}


/* Callback after netstrm obj init in nsd_ptcp - permits us to add some data */
static rsRetVal
LstnInitDrvr(netstrm_t *const pThis)
{
	DEFiRet;
	CHKiRet(osslInit_ctx((nsd_ossl_t*) pThis->pDrvrData));
finalize_it:
	RETiRet;
}


/* initialize the tcp socket for a listner
 * Here, we use the ptcp driver - because there is nothing special
 * at this point with OpenSSL. Things become special once we accept
 * a session, but not during listener setup.
 */
static rsRetVal
LstnInit(netstrms_t *pNS, void *pUsr, rsRetVal(*fAddLstn)(void*,netstrm_t*),
	 const int iSessMax, const tcpLstnParams_t *const cnf_params)
{
	DEFiRet;

	dbgprintf("LstnInit for openssl: entering LstnInit (%p) for %s:%s SessMax=%d\n",
		fAddLstn, cnf_params->pszAddr, cnf_params->pszPort, iSessMax);

	pNS->fLstnInitDrvr = LstnInitDrvr;
	/* Init TCP Listener using base ptcp class */
	iRet = nsd_ptcp.LstnInit(pNS, pUsr, fAddLstn, iSessMax, cnf_params);
	RETiRet;
}


/* This function checks if the connection is still alive - well, kind of...
 * This is a dummy here. For details, check function common in ptcp driver.
 * rgerhards, 2008-06-09
 */
static rsRetVal
CheckConnection(nsd_t __attribute__((unused)) *pNsd)
{
	nsd_ossl_t *pThis = (nsd_ossl_t*) pNsd;
	ISOBJ_TYPE_assert(pThis, nsd_ossl);

	dbgprintf("CheckConnection for %p\n", pNsd);
	return nsd_ptcp.CheckConnection(pThis->pTcp);
}


/* get the remote hostname. The returned hostname must be freed by the caller.
 * rgerhards, 2008-04-25
 */
static rsRetVal
GetRemoteHName(nsd_t *pNsd, uchar **ppszHName)
{
	DEFiRet;
	nsd_ossl_t *pThis = (nsd_ossl_t*) pNsd;
	ISOBJ_TYPE_assert(pThis, nsd_ossl);
	iRet = nsd_ptcp.GetRemoteHName(pThis->pTcp, ppszHName);
	dbgprintf("GetRemoteHName for %p = %s\n", pNsd, *ppszHName);
	RETiRet;
}


/* Provide access to the sockaddr_storage of the remote peer. This
 * is needed by the legacy ACL system. --- gerhards, 2008-12-01
 */
static rsRetVal
GetRemAddr(nsd_t *pNsd, struct sockaddr_storage **ppAddr)
{
	DEFiRet;
	nsd_ossl_t *pThis = (nsd_ossl_t*) pNsd;
	ISOBJ_TYPE_assert(pThis, nsd_ossl);
	iRet = nsd_ptcp.GetRemAddr(pThis->pTcp, ppAddr);
	dbgprintf("GetRemAddr for %p\n", pNsd);
	RETiRet;
}


/* get the remote host's IP address. Caller must Destruct the object. */
static rsRetVal
GetRemoteIP(nsd_t *pNsd, prop_t **ip)
{
	DEFiRet;
	nsd_ossl_t *pThis = (nsd_ossl_t*) pNsd;
	ISOBJ_TYPE_assert(pThis, nsd_ossl);
	iRet = nsd_ptcp.GetRemoteIP(pThis->pTcp, ip);
	dbgprintf("GetRemoteIP for %p\n", pNsd);
	RETiRet;
}


/* Perform all necessary checks after Handshake
 */
rsRetVal
osslPostHandshakeCheck(nsd_ossl_t *pNsd)
{
	DEFiRet;
	char szDbg[255];
	const SSL_CIPHER* sslCipher;

	/* Some extra output for debugging openssl */
	if (SSL_get_shared_ciphers(pNsd->ssl,szDbg, sizeof szDbg) != NULL)
		dbgprintf("osslPostHandshakeCheck: Debug Shared ciphers = %s\n", szDbg);

	#if OPENSSL_VERSION_NUMBER >= 0x10002000L
	if(SSL_get_shared_curve(pNsd->ssl, -1) == 0) {
		// This is not a failure
		LogMsg(0, RS_RET_NO_ERRCODE, LOG_INFO, "nsd_ossl: "
		"Information, no shared curve between syslog client and server");
	}
	#endif
	sslCipher = (const SSL_CIPHER*) SSL_get_current_cipher(pNsd->ssl);
	if (sslCipher != NULL){
		if(SSL_CIPHER_get_version(sslCipher) == NULL) {
			LogError(0, RS_RET_NO_ERRCODE, "nsd_ossl:"
		"TLS version mismatch between syslog client and server.");
		}
		dbgprintf("osslPostHandshakeCheck: Debug Version: %s Name: %s\n",
			SSL_CIPHER_get_version(sslCipher), SSL_CIPHER_get_name(sslCipher));
	}else {
		LogError(0, RS_RET_NO_ERRCODE, "nsd_ossl:No shared ciphers between syslog client and server.");
	}

	FINALIZE;

finalize_it:
	RETiRet;
}


/* Perform all necessary actions for Handshake
 */
rsRetVal
osslHandshakeCheck(nsd_ossl_t *pNsd)
{
	DEFiRet;
	uchar *fromHostIP = NULL;
	int res, resErr;
	dbgprintf("osslHandshakeCheck: Starting TLS Handshake for ssl[%p]\n", (void *)pNsd->ssl);

	if (pNsd->sslState == osslServer) {
		/* Handle Server SSL Object */
		if((res = SSL_accept(pNsd->ssl)) <= 0) {
			/* Obtain SSL Error code */
			nsd_ptcp.GetRemoteHName((nsd_t*)pNsd->pTcp, &fromHostIP);
			resErr = SSL_get_error(pNsd->ssl, res);
			if(	resErr == SSL_ERROR_WANT_READ ||
				resErr == SSL_ERROR_WANT_WRITE) {
				pNsd->rtryCall = osslRtry_handshake;
				pNsd->rtryOsslErr = resErr; /* Store SSL ErrorCode into*/
				dbgprintf("osslHandshakeCheck: OpenSSL Server handshake does not complete "
					"immediately - setting to retry (this is OK and normal)\n");
				FINALIZE;
			} else if(resErr == SSL_ERROR_SYSCALL) {
				dbgprintf("osslHandshakeCheck: OpenSSL Server handshake failed with SSL_ERROR_SYSCALL "
					"- Aborting handshake.\n");
				osslLastSSLErrorMsg(res, pNsd->ssl, LOG_WARNING, "osslHandshakeCheck Server");
				LogMsg(0, RS_RET_NO_ERRCODE, LOG_WARNING,
					"nsd_ossl:TLS session terminated with remote client '%s': "
					"Handshake failed with SSL_ERROR_SYSCALL", fromHostIP);
				ABORT_FINALIZE(RS_RET_NO_ERRCODE);
			} else {
				osslLastSSLErrorMsg(res, pNsd->ssl, LOG_ERR, "osslHandshakeCheck Server");
				LogMsg(0, RS_RET_NO_ERRCODE, LOG_WARNING,
					"nsd_ossl:TLS session terminated with remote client '%s': "
					"Handshake failed with error code: %d", fromHostIP, resErr);
				ABORT_FINALIZE(RS_RET_NO_ERRCODE);
			}
		}
	} else {
		/* Handle Client SSL Object */
		if((res = SSL_do_handshake(pNsd->ssl)) <= 0) {
			/* Obtain SSL Error code */
			nsd_ptcp.GetRemoteHName((nsd_t*)pNsd->pTcp, &fromHostIP);
			resErr = SSL_get_error(pNsd->ssl, res);
			if(	resErr == SSL_ERROR_WANT_READ ||
				resErr == SSL_ERROR_WANT_WRITE) {
				pNsd->rtryCall = osslRtry_handshake;
				pNsd->rtryOsslErr = resErr; /* Store SSL ErrorCode into*/
				dbgprintf("osslHandshakeCheck: OpenSSL Client handshake does not complete "
					"immediately - setting to retry (this is OK and normal)\n");
				FINALIZE;
			} else if(resErr == SSL_ERROR_SYSCALL) {
				dbgprintf("osslHandshakeCheck: OpenSSL Client handshake failed with SSL_ERROR_SYSCALL "
					"- Aborting handshake.\n");
				osslLastSSLErrorMsg(res, pNsd->ssl, LOG_WARNING, "osslHandshakeCheck Client");
				ABORT_FINALIZE(RS_RET_NO_ERRCODE /*RS_RET_RETRY*/);
			} else {
				osslLastSSLErrorMsg(res, pNsd->ssl, LOG_ERR, "osslHandshakeCheck Client");
				LogMsg(0, RS_RET_NO_ERRCODE, LOG_WARNING,
					"nsd_ossl:TLS session terminated with remote syslog server '%s':"
					"Handshake failed with error code: %d", fromHostIP, resErr);
				ABORT_FINALIZE(RS_RET_NO_ERRCODE);
			}
		}
	}

	/* Do post handshake stuff */
	CHKiRet(osslPostHandshakeCheck(pNsd));

	/* Now check authorization */
	CHKiRet(osslChkPeerAuth(pNsd));
finalize_it:
	if (fromHostIP != NULL) {
		free(fromHostIP);
	}
	if(iRet == RS_RET_OK) {
		/* If no error occurred, set socket to SSL mode */
		pNsd->iMode = 1;
	}

	RETiRet;
}

/* accept an incoming connection request - here, we do the usual accept
 * handling. TLS specific handling is done thereafter (and if we run in TLS
 * mode at this time).
 * rgerhards, 2008-04-25
 */
static rsRetVal
AcceptConnReq(nsd_t *pNsd, nsd_t **ppNew)
{
	DEFiRet;
	nsd_ossl_t *pNew = NULL;
	nsd_ossl_t *pThis = (nsd_ossl_t*) pNsd;

	ISOBJ_TYPE_assert((pThis), nsd_ossl);
	CHKiRet(nsd_osslConstruct(&pNew));
	CHKiRet(nsd_ptcp.Destruct(&pNew->pTcp));
	dbgprintf("AcceptConnReq for [%p]: Accepting connection ... \n", (void *)pThis);
	CHKiRet(nsd_ptcp.AcceptConnReq(pThis->pTcp, &pNew->pTcp));

	if(pThis->iMode == 0) {
		/*we are in non-TLS mode, so we are done */
		DBGPRINTF("AcceptConnReq: NOT in TLS mode!\n");
		*ppNew = (nsd_t*) pNew;
		FINALIZE;
	}

	/* If we reach this point, we are in TLS mode */
	pNew->authMode = pThis->authMode;
	pNew->permitExpiredCerts = pThis->permitExpiredCerts;
	pNew->pPermPeers = pThis->pPermPeers;
	pNew->DrvrVerifyDepth = pThis->DrvrVerifyDepth;
	pNew->gnutlsPriorityString = pThis->gnutlsPriorityString;
	pNew->ctx = pThis->ctx;
	pNew->ctx_is_copy = 1; // do not free on pNew Destruction
	CHKiRet(osslInitSession(pNew, osslServer));

	/* Store nsd_ossl_t* reference in SSL obj */
	SSL_set_ex_data(pNew->ssl, 0, pThis);

	/* We now do the handshake */
	CHKiRet(osslHandshakeCheck(pNew));

	*ppNew = (nsd_t*) pNew;
finalize_it:
	/* Accept appears to be done here */
	if(pNew != NULL) {
		DBGPRINTF("AcceptConnReq: END iRet = %d, pNew=[%p], pNsd->rtryCall=%d\n",
			iRet, pNew, pNew->rtryCall);
	}
	if(iRet != RS_RET_OK) {
		if(pNew != NULL) {
			nsd_osslDestruct(&pNew);
		}
	}
	RETiRet;
}


/* receive data from a tcp socket
 * The lenBuf parameter must contain the max buffer size on entry and contains
 * the number of octets read on exit. This function
 * never blocks, not even when called on a blocking socket. That is important
 * for client sockets, which are set to block during send, but should not
 * block when trying to read data. -- rgerhards, 2008-03-17
 * The function now follows the usual iRet calling sequence.
 * With GnuTLS, we may need to restart a recv() system call. If so, we need
 * to supply the SAME buffer on the retry. We can not assure this, as the
 * caller is free to call us with any buffer location (and in current
 * implementation, it is on the stack and extremely likely to change). To
 * work-around this problem, we allocate a buffer ourselfs and always receive
 * into that buffer. We pass data on to the caller only after we have received it.
 * To save some space, we allocate that internal buffer only when it is actually
 * needed, which means when we reach this function for the first time. To keep
 * the algorithm simple, we always supply data only from the internal buffer,
 * even if it is a single byte. As we have a stream, the caller must be prepared
 * to accept messages in any order, so we do not need to take care about this.
 * Please note that the logic also forces us to do some "faking" in select(), as
 * we must provide a fake "is ready for readign" status if we have data inside our
 * buffer. -- rgerhards, 2008-06-23
 */
static rsRetVal
Rcv(nsd_t *pNsd, uchar *pBuf, ssize_t *pLenBuf, int *const oserr)
{
	DEFiRet;
	ssize_t iBytesCopy; /* how many bytes are to be copied to the client buffer? */
	nsd_ossl_t *pThis = (nsd_ossl_t*) pNsd;
	ISOBJ_TYPE_assert(pThis, nsd_ossl);
	DBGPRINTF("Rcv for %p\n", pNsd);

	if(pThis->bAbortConn)
		ABORT_FINALIZE(RS_RET_CONNECTION_ABORTREQ);

	if(pThis->iMode == 0) {
		CHKiRet(nsd_ptcp.Rcv(pThis->pTcp, pBuf, pLenBuf, oserr));
		FINALIZE;
	}

	/* --- in TLS mode now --- */

	/* Buffer logic applies only if we are in TLS mode. Here we
	 * assume that we will switch from plain to TLS, but never back. This
	 * assumption may be unsafe, but it is the model for the time being and I
	 * do not see any valid reason why we should switch back to plain TCP after
	 * we were in TLS mode. However, in that case we may lose something that
	 * is already in the receive buffer ... risk accepted. -- rgerhards, 2008-06-23
	 */

	if(pThis->pszRcvBuf == NULL) {
		/* we have no buffer, so we need to malloc one */
		CHKmalloc(pThis->pszRcvBuf = malloc(NSD_OSSL_MAX_RCVBUF));
		pThis->lenRcvBuf = -1;
	}

	/* now check if we have something in our buffer. If so, we satisfy
	 * the request from buffer contents.
	 */
	if(pThis->lenRcvBuf == -1) { /* no data present, must read */
		CHKiRet(osslRecordRecv(pThis));
	}

	if(pThis->lenRcvBuf == 0) { /* EOS */
		*oserr = errno;
		ABORT_FINALIZE(RS_RET_CLOSED);
	}

	/* if we reach this point, data is present in the buffer and must be copied */
	iBytesCopy = pThis->lenRcvBuf - pThis->ptrRcvBuf;
	if(iBytesCopy > *pLenBuf) {
		iBytesCopy = *pLenBuf;
	} else {
		pThis->lenRcvBuf = -1; /* buffer will be emptied below */
	}

	memcpy(pBuf, pThis->pszRcvBuf + pThis->ptrRcvBuf, iBytesCopy);
	pThis->ptrRcvBuf += iBytesCopy;
	*pLenBuf = iBytesCopy;

finalize_it:
	if (iRet != RS_RET_OK) {
		if (iRet == RS_RET_CLOSED) {
			if (pThis->ssl != NULL) {
				/* Set SSL Shutdown */
				SSL_shutdown(pThis->ssl);
				dbgprintf("osslRcv SSL_shutdown done\n");
			}
		} else if (iRet != RS_RET_RETRY) {
			/* We need to free the receive buffer in error error case unless a retry is wanted. , if we
			 * allocated one. -- rgerhards, 2008-12-03 -- moved here by alorbach, 2015-12-01
			 */
			*pLenBuf = 0;
			free(pThis->pszRcvBuf);
			pThis->pszRcvBuf = NULL;
		} else {
			/* RS_RET_RETRY | Check for SSL Shutdown */
			if (SSL_get_shutdown(pThis->ssl) == SSL_RECEIVED_SHUTDOWN) {
				dbgprintf("osslRcv received SSL_RECEIVED_SHUTDOWN!\n");
				iRet = RS_RET_CLOSED;

				/* Send Shutdown message back */
				SSL_shutdown(pThis->ssl);
			}
		}
	}
	dbgprintf("osslRcv return. nsd %p, iRet %d, lenRcvBuf %d, ptrRcvBuf %d\n", pThis,
	iRet, pThis->lenRcvBuf, pThis->ptrRcvBuf);
	RETiRet;
}


/* send a buffer. On entry, pLenBuf contains the number of octets to
 * write. On exit, it contains the number of octets actually written.
 * If this number is lower than on entry, only a partial buffer has
 * been written.
 * rgerhards, 2008-03-19
 */
static rsRetVal
Send(nsd_t *pNsd, uchar *pBuf, ssize_t *pLenBuf)
{
	DEFiRet;
	int iSent;
	int err;
	nsd_ossl_t *pThis = (nsd_ossl_t*) pNsd;
	DBGPRINTF("Send for %p\n", pNsd);

	ISOBJ_TYPE_assert(pThis, nsd_ossl);

	if(pThis->bAbortConn)
		ABORT_FINALIZE(RS_RET_CONNECTION_ABORTREQ);

	if(pThis->iMode == 0) {
		CHKiRet(nsd_ptcp.Send(pThis->pTcp, pBuf, pLenBuf));
		FINALIZE;
	}

	while(1) {
		iSent = SSL_write(pThis->ssl, pBuf, *pLenBuf);
		if(iSent > 0) {
			*pLenBuf = iSent;
			break;
		} else {
			err = SSL_get_error(pThis->ssl, iSent);
			if(	err == SSL_ERROR_ZERO_RETURN ) {
				DBGPRINTF("Send: SSL_ERROR_ZERO_RETURN received, retry next time\n");
				ABORT_FINALIZE(RS_RET_RETRY);
			}
			else if(err != SSL_ERROR_WANT_READ &&
				err != SSL_ERROR_WANT_WRITE) {
				/* Output error and abort */
				osslLastSSLErrorMsg(iSent, pThis->ssl, LOG_ERR, "Send");
				ABORT_FINALIZE(RS_RET_NO_ERRCODE);
			} else {
				/* Check for SSL Shutdown */
				if (SSL_get_shutdown(pThis->ssl) == SSL_RECEIVED_SHUTDOWN) {
					dbgprintf("osslRcv received SSL_RECEIVED_SHUTDOWN!\n");
					ABORT_FINALIZE(RS_RET_CLOSED);
				}
			}
		}
	}
finalize_it:
	RETiRet;
}


/* Enable KEEPALIVE handling on the socket.
 * rgerhards, 2009-06-02
 */
static rsRetVal
EnableKeepAlive(nsd_t *pNsd)
{
	nsd_ossl_t *pThis = (nsd_ossl_t*) pNsd;
	ISOBJ_TYPE_assert(pThis, nsd_ossl);
	return nsd_ptcp.EnableKeepAlive(pThis->pTcp);
}


/* open a connection to a remote host (server). With OpenSSL, we always
 * open a plain tcp socket and then, if in TLS mode, do a handshake on it.
 */
static rsRetVal
Connect(nsd_t *pNsd, int family, uchar *port, uchar *host, char *device)
{
	DEFiRet;
	DBGPRINTF("openssl: entering Connect family=%d, device=%s\n", family, device);
	nsd_ossl_t* pThis = (nsd_ossl_t*) pNsd;

	ISOBJ_TYPE_assert(pThis, nsd_ossl);
	assert(port != NULL);
	assert(host != NULL);

	CHKiRet(osslInit_ctx(pThis));
	CHKiRet(nsd_ptcp.Connect(pThis->pTcp, family, port, host, device));

	if(pThis->iMode == 0) {
		/*we are in non-TLS mode, so we are done */
		DBGPRINTF("Connect: NOT in TLS mode!\n");
		FINALIZE;
	}

	LogMsg(0, RS_RET_NO_ERRCODE, LOG_INFO, "nsd_ossl: "
		"TLS Connection initiated with remote syslog server.");
	/*if we reach this point we are in tls mode */
	DBGPRINTF("Connect: TLS Mode\n");

	/* Do SSL Session init */
	CHKiRet(osslInitSession(pThis, osslClient));

	/* Store nsd_ossl_t* reference in SSL obj */
	SSL_set_ex_data(pThis->ssl, 0, pThis);

	/* We now do the handshake */
	iRet = osslHandshakeCheck(pThis);
finalize_it:
	/* Connect appears to be done here */
	dbgprintf("Connect: END iRet = %d, pThis=[%p], pNsd->rtryCall=%d\n",
		iRet, pThis, pThis->rtryCall);
	if(iRet != RS_RET_OK) {
		if(pThis->bHaveSess) {
			pThis->bHaveSess = 0;
			SSL_free(pThis->ssl);
			pThis->ssl = NULL;
		}
	}
	RETiRet;
}

static rsRetVal
SetGnutlsPriorityString(nsd_t *const pNsd, uchar *const gnutlsPriorityString)
{
	DEFiRet;
	nsd_ossl_t* pThis = (nsd_ossl_t*) pNsd;
	ISOBJ_TYPE_assert(pThis, nsd_ossl);

#if OPENSSL_VERSION_NUMBER >= 0x10002000L && !defined(LIBRESSL_VERSION_NUMBER)
	sbool ApplySettings = 0;
	if (	(gnutlsPriorityString != NULL && pThis->gnutlsPriorityString == NULL) ||
		(gnutlsPriorityString != NULL &&
		strcmp( (const char*)pThis->gnutlsPriorityString, (const char*)gnutlsPriorityString) != 0)
		) {
		ApplySettings = 1;
	}

	pThis->gnutlsPriorityString = gnutlsPriorityString;
	dbgprintf("gnutlsPriorityString: set to '%s' Apply %s\n",
		(gnutlsPriorityString != NULL ? (char*)gnutlsPriorityString : "NULL"),
		(ApplySettings == 1? "TRUE" : "FALSE"));
	if (ApplySettings == 1) {
		/* Apply Settings if necessary */
		applyGnutlsPriorityString(pThis);
	}

#else
	LogError(0, RS_RET_SYS_ERR, "Warning: TLS library does not support SSL_CONF_cmd API"
		"(maybe it is too old?). Cannot use gnutlsPriorityString ('%s'). For more see: "
		"https://www.rsyslog.com/doc/master/configuration/modules/imtcp.html#gnutlsprioritystring",
		gnutlsPriorityString);
#endif
	RETiRet;
}


static rsRetVal
applyGnutlsPriorityString(nsd_ossl_t *const pThis)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, nsd_ossl);

#if OPENSSL_VERSION_NUMBER >= 0x10002000L && !defined(LIBRESSL_VERSION_NUMBER)
	/* Note: we disable unkonwn functions. The corresponding error message is
	 * generated during SetGntuTLSPriorityString().
	 */

	if(pThis->gnutlsPriorityString == NULL || pThis->ctx == NULL) {
		FINALIZE;
	} else {
		dbgprintf("applying gnutlsPriorityString: '%s'\n", pThis->gnutlsPriorityString);
		char *pCurrentPos;
		char *pNextPos;
		char *pszCmd;
		char *pszValue;
		int iConfErr;

		/* Set working pointer */
		pCurrentPos = (char*) pThis->gnutlsPriorityString;
		if (pCurrentPos != NULL && strlen(pCurrentPos) > 0) {
			// Create CTX Config Helper
			SSL_CONF_CTX *cctx;
			cctx = SSL_CONF_CTX_new();
			if (pThis->sslState == osslServer) {
				SSL_CONF_CTX_set_flags(cctx, SSL_CONF_FLAG_SERVER);
			} else {
				SSL_CONF_CTX_set_flags(cctx, SSL_CONF_FLAG_CLIENT);
			}
			SSL_CONF_CTX_set_flags(cctx, SSL_CONF_FLAG_FILE);
			SSL_CONF_CTX_set_flags(cctx, SSL_CONF_FLAG_SHOW_ERRORS);
			SSL_CONF_CTX_set_ssl_ctx(cctx, pThis->ctx);

			do
			{
				pNextPos = index(pCurrentPos, '=');
				if (pNextPos != NULL) {
					while (	*pCurrentPos != '\0' &&
						(*pCurrentPos == ' ' || *pCurrentPos == '\t') )
						pCurrentPos++;
					pszCmd = strndup(pCurrentPos, pNextPos-pCurrentPos);
					pCurrentPos = pNextPos+1;
					pNextPos = index(pCurrentPos, '\n');
					pszValue = (pNextPos == NULL ?
							strdup(pCurrentPos) :
							strndup(pCurrentPos, pNextPos - pCurrentPos));
					pCurrentPos = (pNextPos == NULL ? NULL : pNextPos+1);

					/* Add SSL Conf Command */
					iConfErr = SSL_CONF_cmd(cctx, pszCmd, pszValue);
					if (iConfErr > 0) {
						dbgprintf("gnutlsPriorityString: Successfully added Command "
							"'%s':'%s'\n",
							pszCmd, pszValue);
					}
					else {
						LogError(0, RS_RET_SYS_ERR, "Failed to added Command: %s:'%s' "
							"in gnutlsPriorityString with error '%d'",
							pszCmd, pszValue, iConfErr);
					}

					free(pszCmd);
					free(pszValue);
				} else {
					/* Abort further parsing */
					pCurrentPos = NULL;
				}
			}
			while (pCurrentPos != NULL);

			/* Finalize SSL Conf */
			iConfErr = SSL_CONF_CTX_finish(cctx);
			if (!iConfErr) {
				LogError(0, RS_RET_SYS_ERR, "Error: setting openssl command parameters: %s"
						"Open ssl error info may follow in next messages",
						pThis->gnutlsPriorityString);
				osslLastSSLErrorMsg(0, NULL, LOG_ERR, "SetGnutlsPriorityString");
			}
			SSL_CONF_CTX_free(cctx);
		}
	}
#endif

finalize_it:
	RETiRet;
}

/* Set the driver cert extended key usage check setting, for now it is empty wrapper.
 * TODO: implement openSSL version
 * jvymazal, 2019-08-16
 */
static rsRetVal
SetCheckExtendedKeyUsage(nsd_t __attribute__((unused)) *pNsd, int ChkExtendedKeyUsage)
{
	DEFiRet;
	if(ChkExtendedKeyUsage != 0) {
		LogError(0, RS_RET_VALUE_NOT_SUPPORTED, "error: driver ChkExtendedKeyUsage %d "
				"not supported by ossl netstream driver", ChkExtendedKeyUsage);
		ABORT_FINALIZE(RS_RET_VALUE_NOT_SUPPORTED);
	}
finalize_it:
	RETiRet;
}

/* Set the driver name checking strictness, for now it is empty wrapper.
 * TODO: implement openSSL version
 * jvymazal, 2019-08-16
 */
static rsRetVal
SetPrioritizeSAN(nsd_t __attribute__((unused)) *pNsd, int prioritizeSan)
{
	DEFiRet;
	if(prioritizeSan != 0) {
		LogError(0, RS_RET_VALUE_NOT_SUPPORTED, "error: driver prioritizeSan %d "
				"not supported by ossl netstream driver", prioritizeSan);
		ABORT_FINALIZE(RS_RET_VALUE_NOT_SUPPORTED);
	}
finalize_it:
	RETiRet;
}

/* Set the driver tls  verifyDepth
 * alorbach, 2019-12-20
 */
static rsRetVal
SetTlsVerifyDepth(nsd_t *pNsd, int verifyDepth)
{
	DEFiRet;
	nsd_ossl_t *pThis = (nsd_ossl_t*) pNsd;

	ISOBJ_TYPE_assert((pThis), nsd_ossl);
	if (verifyDepth == 0) {
		FINALIZE;
	}
	assert(verifyDepth >= 2);
	pThis->DrvrVerifyDepth = verifyDepth;

finalize_it:
	RETiRet;
}


static rsRetVal
SetTlsCAFile(nsd_t *pNsd, const uchar *const caFile)
{
	DEFiRet;
	nsd_ossl_t *const pThis = (nsd_ossl_t*) pNsd;

	ISOBJ_TYPE_assert((pThis), nsd_ossl);
	if(caFile == NULL) {
		pThis->pszCAFile = NULL;
	} else {
		CHKmalloc(pThis->pszCAFile = (const uchar*) strdup((const char*) caFile));
	}

finalize_it:
	RETiRet;
}

static rsRetVal
SetTlsKeyFile(nsd_t *pNsd, const uchar *const pszFile)
{
	DEFiRet;
	nsd_ossl_t *const pThis = (nsd_ossl_t*) pNsd;

	ISOBJ_TYPE_assert((pThis), nsd_ossl);
	if(pszFile == NULL) {
		pThis->pszKeyFile = NULL;
	} else {
		CHKmalloc(pThis->pszKeyFile = (const uchar*) strdup((const char*) pszFile));
	}

finalize_it:
	RETiRet;
}

static rsRetVal
SetTlsCertFile(nsd_t *pNsd, const uchar *const pszFile)
{
	DEFiRet;
	nsd_ossl_t *const pThis = (nsd_ossl_t*) pNsd;

	ISOBJ_TYPE_assert((pThis), nsd_ossl);
	if(pszFile == NULL) {
		pThis->pszCertFile = NULL;
	} else {
		CHKmalloc(pThis->pszCertFile = (const uchar*) strdup((const char*) pszFile));
	}

finalize_it:
	RETiRet;
}


/* queryInterface function */
BEGINobjQueryInterface(nsd_ossl)
CODESTARTobjQueryInterface(nsd_ossl)
	if(pIf->ifVersion != nsdCURR_IF_VERSION) {/* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->Construct = (rsRetVal(*)(nsd_t**)) nsd_osslConstruct;
	pIf->Destruct = (rsRetVal(*)(nsd_t**)) nsd_osslDestruct;
	pIf->Abort = Abort;
	pIf->LstnInit = LstnInit;
	pIf->AcceptConnReq = AcceptConnReq;
	pIf->Rcv = Rcv;
	pIf->Send = Send;
	pIf->Connect = Connect;
	pIf->SetSock = SetSock;
	pIf->SetMode = SetMode;
	pIf->SetAuthMode = SetAuthMode;
	pIf->SetPermitExpiredCerts = SetPermitExpiredCerts;
	pIf->SetPermPeers =SetPermPeers;
	pIf->CheckConnection = CheckConnection;
	pIf->GetRemoteHName = GetRemoteHName;
	pIf->GetRemoteIP = GetRemoteIP;
	pIf->GetRemAddr = GetRemAddr;
	pIf->EnableKeepAlive = EnableKeepAlive;
	pIf->SetKeepAliveIntvl = SetKeepAliveIntvl;
	pIf->SetKeepAliveProbes = SetKeepAliveProbes;
	pIf->SetKeepAliveTime = SetKeepAliveTime;
	pIf->SetGnutlsPriorityString = SetGnutlsPriorityString; /* we don't NEED this interface! */
	pIf->SetCheckExtendedKeyUsage = SetCheckExtendedKeyUsage; /* we don't NEED this interface! */
	pIf->SetPrioritizeSAN = SetPrioritizeSAN; /* we don't NEED this interface! */
	pIf->SetTlsVerifyDepth = SetTlsVerifyDepth;
	pIf->SetTlsCAFile = SetTlsCAFile;
	pIf->SetTlsKeyFile = SetTlsKeyFile;
	pIf->SetTlsCertFile = SetTlsCertFile;

finalize_it:
ENDobjQueryInterface(nsd_ossl)


/* exit our class
 */
BEGINObjClassExit(nsd_ossl, OBJ_IS_LOADABLE_MODULE) /* CHANGE class also in END MACRO! */
CODESTARTObjClassExit(nsd_ossl)
	osslGlblExit();	/* shut down OpenSSL */

	/* release objects we no longer need */
	objRelease(nsd_ptcp, LM_NSD_PTCP_FILENAME);
	objRelease(net, LM_NET_FILENAME);
	objRelease(glbl, CORE_COMPONENT);
	objRelease(datetime, CORE_COMPONENT);
ENDObjClassExit(nsd_ossl)


/* Initialize the nsd_ossl class. Must be called as the very first method
 * before anything else is called inside this class.
 */
BEGINObjClassInit(nsd_ossl, 1, OBJ_IS_LOADABLE_MODULE) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(datetime, CORE_COMPONENT));
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(net, LM_NET_FILENAME));
	CHKiRet(objUse(nsd_ptcp, LM_NSD_PTCP_FILENAME));

	/* now do global TLS init stuff */
	CHKiRet(osslGlblInit());
ENDObjClassInit(nsd_ossl)


/* --------------- here now comes the plumbing that makes as a library module --------------- */


BEGINmodExit
CODESTARTmodExit
	nsdsel_osslClassExit();
	nsd_osslClassExit();
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_LIB_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */

	/* Initialize all classes that are in our module - this includes ourselfs */
	CHKiRet(nsd_osslClassInit(pModInfo)); /* must be done after tcps_sess, as we use it */
	CHKiRet(nsdsel_osslClassInit(pModInfo)); /* must be done after tcps_sess, as we use it */
ENDmodInit
