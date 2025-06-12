/* Definitions for generic OpenSSL include stuff.
 *
 * Copyright 2023 Andre Lorbach and Adiscon GmbH.
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

#ifndef INCLUDED_NET_OSSL_H
#define INCLUDED_NET_OSSL_H

/* Needed OpenSSL Includes */
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#if OPENSSL_VERSION_NUMBER >= 0x30000000L && !defined(LIBRESSL_VERSION_NUMBER)
#	include <openssl/bioerr.h>
#endif
#ifndef OPENSSL_NO_ENGINE
#	include <openssl/engine.h>
#endif
#include <openssl/rand.h>
#include <openssl/evp.h>

/* Internal OpenSSL defined ENUMS */
typedef enum {
	OSSL_AUTH_CERTNAME = 0,
	OSSL_AUTH_CERTFINGERPRINT = 1,
	OSSL_AUTH_CERTVALID = 2,
	OSSL_AUTH_CERTANON = 3
} AuthMode;

typedef enum {
	OSSL_EXPIRED_PERMIT = 0,
	OSSL_EXPIRED_DENY = 1,
	OSSL_EXPIRED_WARN = 2
} PermitExpiredCerts;

typedef enum {
	osslServer = 0,	/**< Server SSL Object */
	osslClient = 1	/**< Client SSL Object */
} osslSslState_t;

/* the net_ossl object */
struct net_ossl_s {
	BEGINobjInstance;	/* Data to implement generic object - MUST be the first data element! */
	/* Config Cert vars */
	const uchar *pszCAFile;
	const uchar *pszCRLFile;
	const uchar *pszExtraCAFiles;
	const uchar *pszKeyFile;
	const uchar *pszCertFile;
	AuthMode authMode;
	permittedPeers_t *pPermPeers; /* permitted peers */
	int bReportAuthErr;	/* only the first auth error is to be reported, this var triggers it. Initially, it is
				 * set to 1 and changed to 0 after the first report. It is changed back to 1 after
				 * one successful authentication. */
	/* Open SSL objects */
	BIO *bio;		/* OpenSSL main BIO obj */
	int ctx_is_copy;
	SSL_CTX *ctx;		/* credentials, ciphers, ... */
	SSL *ssl;		/* OpenSSL main SSL obj */
	osslSslState_t sslState;/**< what must we retry? */
};

/* interface */
BEGINinterface(net_ossl) /* name must also be changed in ENDinterface macro! */
	rsRetVal (*Construct)(net_ossl_t **ppThis);
	rsRetVal (*Destruct)(net_ossl_t **ppThis);
	rsRetVal (*osslCtxInit)(net_ossl_t *pThis, const SSL_METHOD *method);
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	rsRetVal (*osslCtxInitCookie)(net_ossl_t *pThis);
#endif // OPENSSL_VERSION_NUMBER >= 0x10100000L
	rsRetVal (*osslInitEngine)(net_ossl_t *pThis);
	// OpenSSL Helper function exports
	rsRetVal (*osslChkpeername)(net_ossl_t *pThis, X509* certpeer, uchar *fromHostIP);
	rsRetVal (*osslPeerfingerprint)(net_ossl_t *pThis, X509* certpeer, uchar *fromHostIP);
	X509* (*osslGetpeercert)(net_ossl_t *pThis, SSL *ssl, uchar *fromHostIP);
	rsRetVal (*osslChkpeercertvalidity)(net_ossl_t *pThis, SSL *ssl, uchar *fromHostIP);
#if OPENSSL_VERSION_NUMBER >= 0x10002000L && !defined(LIBRESSL_VERSION_NUMBER)
	rsRetVal (*osslApplyTlscgfcmd)(net_ossl_t *pThis, uchar *tlscfgcmd);
#endif // OPENSSL_VERSION_NUMBER >= 0x10002000L
	void (*osslSetBioCallback)(BIO *conn);
	void (*osslSetCtxVerifyCallback)(SSL_CTX *pCtx, int flags);
	void (*osslSetSslVerifyCallback)(SSL *pSsl, int flags);
	void (*osslLastOpenSSLErrorMsg)(uchar *fromHost,
		const int ret, SSL *ssl, int severity, const char* pszCallSource, const char* pszOsslApi);
ENDinterface(net_ossl)

#define net_osslCURR_IF_VERSION 1 /* increment whenever you change the interface structure! */
// ------------------------------------------------------

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

/*-----------------------------------------------------------------------------*/
/* OpenSSL Global Helper functions prototypes */
#define MUTEX_TYPE       pthread_mutex_t
#define MUTEX_SETUP(x)   pthread_mutex_init(&(x), NULL)
#define MUTEX_CLEANUP(x) pthread_mutex_destroy(&(x))
#define MUTEX_LOCK(x)    pthread_mutex_lock(&(x))
#define MUTEX_UNLOCK(x)  pthread_mutex_unlock(&(x))
#define THREAD_ID        pthread_self()

/* This array will store all of the mutexes available to OpenSSL. */
struct CRYPTO_dynlock_value
{
	MUTEX_TYPE mutex;
};

void dyn_destroy_function(struct CRYPTO_dynlock_value *l,
	__attribute__((unused)) const char *file, __attribute__((unused)) int line);
void dyn_lock_function(int mode, struct CRYPTO_dynlock_value *l,
	__attribute__((unused)) const char *file, __attribute__((unused)) int line);
struct CRYPTO_dynlock_value * dyn_create_function(
	__attribute__((unused)) const char *file, __attribute__((unused)) int line);
unsigned long id_function(void);
void locking_function(int mode, int n,
	__attribute__((unused)) const char * file, __attribute__((unused)) int line);

int opensslh_THREAD_setup(void);
int opensslh_THREAD_cleanup(void);

void osslGlblInit(void);
void osslGlblExit(void);
/*-----------------------------------------------------------------------------*/

/* prototypes */
PROTOTYPEObj(net_ossl);

/* the name of our library binary */
// #define LM_NET_OSSL_FILENAME "lmnet_ossl"
#define LM_NET_OSSL_FILENAME "lmnsd_ossl"


#endif /* #ifndef INCLUDED_NET_OSSL_H */
