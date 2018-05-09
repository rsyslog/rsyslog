/* nsd_ossl.c
 *
 * An implementation of the nsd interface for OpenSSL.
 *
 * Copyright 2018-2018 Adiscon GmbH.
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
// #include <openssl/bio.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#include <openssl/engine.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#include "rsyslog.h"
#include "syslogd-types.h"
#include "module-template.h"
#include "cfsysline.h"
#include "obj.h"
#include "stringbuf.h"
#include "errmsg.h"
#include "net.h"
#include "netstrms.h"
#include "netstrm.h"
#include "datetime.h"
#include "nsd_ptcp.h"
#include "nsdsel_ossl.h"
#include "nsd_ossl.h"
#include "unicode-helper.h"

/* things to move to some better place/functionality - TODO */
#define CRLFILE "crl.pem"


MODULE_TYPE_LIB
MODULE_TYPE_KEEP

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(errmsg)
DEFobjCurrIf(glbl)
DEFobjCurrIf(net)
DEFobjCurrIf(netstrms)
DEFobjCurrIf(netstrm)
DEFobjCurrIf(datetime)
DEFobjCurrIf(nsd_ptcp)


static int bGlblSrvrInitDone = 0;	/**< 0 - server global init not yet done, 1 - already done */

/* Main OpenSSL CTX pointer */
static SSL_CTX *ctx;

/*--------------------------------------OpenSSL helpers ------------------------------------------*/
void getLastSSLErrorMsg(int ret, SSL *ssl, char* pszCallSource)
{
	unsigned long un_error = 0;
	char psz[256];
	int iMyRet = SSL_get_error(ssl, ret);

	/* Check which kind of error we have */
	DBGPRINTF("Error in Method: %s\n", pszCallSource);
	if(iMyRet == SSL_ERROR_SSL) {
		un_error = ERR_peek_last_error();
		ERR_error_string_n(un_error, psz, 256);
		errmsg.LogError(0, RS_RET_NO_ERRCODE, "%s", psz);
	} else if(iMyRet == SSL_ERROR_SYSCALL){
		iMyRet = ERR_get_error();
		if(ret == 0) {
			iMyRet = SSL_get_error(ssl, iMyRet);
			if(iMyRet == 0) {
				*psz = '\0';
			} else {
				ERR_error_string_n(iMyRet, psz, 256);
			}
		} else {
			un_error = ERR_peek_last_error();
			ERR_error_string_n(un_error, psz, 256);
		}
		errmsg.LogError(0, RS_RET_NO_ERRCODE, "%s", psz);
	} else {
		errmsg.LogError(0, RS_RET_NO_ERRCODE, "Unknown SSL Error, SSL_get_error: %d", iMyRet);
	}
}

int verify_callback(int status, X509_STORE_CTX *store)
{
	char data[256];

	if(!status) {
		X509 *cert = X509_STORE_CTX_get_current_cert(store);
		int depth = X509_STORE_CTX_get_error_depth(store);
		int err = X509_STORE_CTX_get_error(store);

		errmsg.LogError(0, RS_RET_NO_ERRCODE, "Certificate error at depth: %i", depth);
		X509_NAME_oneline(X509_get_issuer_name(cert), data, 256);
		errmsg.LogError(0, RS_RET_NO_ERRCODE, " issuer  = %s", data);
		X509_NAME_oneline(X509_get_subject_name(cert), data, 256);
		errmsg.LogError(0, RS_RET_NO_ERRCODE, " subject = %s", data);
		errmsg.LogError(0, RS_RET_NO_ERRCODE, " err %i:%s", err, X509_verify_cert_error_string(err));
	}

	return status;
}

long BIO_debug_callback(BIO *bio, int cmd, const char __attribute__((unused)) *argp,
                        int argi, long __attribute__((unused)) argl, long ret)
{
    long r = 1;

    if (BIO_CB_RETURN & cmd)
        r = ret;

    dbgprintf("openssl debugmsg: BIO[%p]: ", (void *)bio);

    switch (cmd) {
    case BIO_CB_FREE:
        dbgprintf("Free - %s\n", bio->method->name);
        break;
    case BIO_CB_READ:
        if (bio->method->type & BIO_TYPE_DESCRIPTOR)
            dbgprintf("read(%d,%lu) - %s fd=%d\n",
                         bio->num, (unsigned long)argi,
                         bio->method->name, bio->num);
        else
            dbgprintf("read(%d,%lu) - %s\n",
                         bio->num, (unsigned long)argi, bio->method->name);
        break;
    case BIO_CB_WRITE:
        if (bio->method->type & BIO_TYPE_DESCRIPTOR)
            dbgprintf("write(%d,%lu) - %s fd=%d\n",
                         bio->num, (unsigned long)argi,
                         bio->method->name, bio->num);
        else
            dbgprintf("write(%d,%lu) - %s\n",
                         bio->num, (unsigned long)argi, bio->method->name);
        break;
    case BIO_CB_PUTS:
        dbgprintf("puts() - %s\n", bio->method->name);
        break;
    case BIO_CB_GETS:
        dbgprintf("gets(%lu) - %s\n", (unsigned long)argi,
                     bio->method->name);
        break;
    case BIO_CB_CTRL:
        dbgprintf("ctrl(%lu) - %s\n", (unsigned long)argi,
                     bio->method->name);
        break;
    case BIO_CB_RETURN | BIO_CB_READ:
        dbgprintf("read return %ld\n", ret);
        break;
    case BIO_CB_RETURN | BIO_CB_WRITE:
        dbgprintf("write return %ld\n", ret);
        break;
    case BIO_CB_RETURN | BIO_CB_GETS:
        dbgprintf("gets return %ld\n", ret);
        break;
    case BIO_CB_RETURN | BIO_CB_PUTS:
        dbgprintf("puts return %ld\n", ret);
        break;
    case BIO_CB_RETURN | BIO_CB_CTRL:
        dbgprintf("ctrl return %ld\n", ret);
        break;
    default:
        dbgprintf("bio callback - unknown type (%d)\n", cmd);
        break;
    }

    return (r);
}
/*--------------------------------------------------------------------------------*/


/* TODO: CHECK IF NEEDED !
 * a macro to abort if GnuTLS error is not acceptable. We split this off from
 * CHKgnutls() to avoid some Coverity report in cases where we know GnuTLS
 * failed. Note: gnuRet must already be set accordingly!

#define ABORTopenssl { \
		uchar *pErr = osslStrerror(gnuRet); \
		LogError(0, RS_RET_GNUTLS_ERR, "unexpected GnuTLS error %d in %s:%d: %s\n", \
	gnuRet, __FILE__, __LINE__, pErr); \
		free(pErr); \
		ABORT_FINALIZE(RS_RET_GNUTLS_ERR); \
}
// a macro to check GnuTLS calls against unexpected errors
#define CHKopenssl(x) { \
	gnuRet = (x); \
	if(gnuRet == GNUTLS_E_FILE_ERROR) { \
		LogError(0, RS_RET_GNUTLS_ERR, "error reading file - a common cause is that the " \
			"file  does not exist"); \
		ABORT_FINALIZE(RS_RET_GNUTLS_ERR); \
	} else if(gnuRet != 0) { \
		ABORTgnutls; \
	} \
}
*/

#if 0 /* copied CODE needs to be converted first! */

	/* read in the whole content of a file. The caller is responsible for
	 * freeing the buffer. To prevent DOS, this function can NOT read
	 * files larger than 1MB (which still is *very* large).
	 * rgerhards, 2008-05-26
	 */
	static rsRetVal
	readFile(uchar *pszFile, gnutls_datum_t *pBuf)
	{
		int fd;
		struct stat stat_st;
		DEFiRet;

		assert(pszFile != NULL);
		assert(pBuf != NULL);

		pBuf->data = NULL;

		if((fd = open((char*)pszFile, O_RDONLY)) == -1) {
			LogError(errno, RS_RET_FILE_NOT_FOUND, "can not read file '%s'", pszFile);
			ABORT_FINALIZE(RS_RET_FILE_NOT_FOUND);
		}

		if(fstat(fd, &stat_st) == -1) {
			LogError(errno, RS_RET_FILE_NO_STAT, "can not stat file '%s'", pszFile);
			ABORT_FINALIZE(RS_RET_FILE_NO_STAT);
		}

		/* 1MB limit */
		if(stat_st.st_size > 1024 * 1024) {
			LogError(0, RS_RET_FILE_TOO_LARGE, "file '%s' too large, max 1MB", pszFile);
			ABORT_FINALIZE(RS_RET_FILE_TOO_LARGE);
		}

		CHKmalloc(pBuf->data = MALLOC(stat_st.st_size));
		pBuf->size = stat_st.st_size;
		if(read(fd,  pBuf->data, stat_st.st_size) != stat_st.st_size) {
			LogError(0, RS_RET_IO_ERROR, "error or incomplete read of file '%s'", pszFile);
			ABORT_FINALIZE(RS_RET_IO_ERROR);
		}

	finalize_it:
		if(fd != -1)
			close(fd);
		if(iRet != RS_RET_OK) {
			if(pBuf->data != NULL) {
				free(pBuf->data);
				pBuf->data = NULL;
				pBuf->size = 0;
				}
		}
		RETiRet;
	}


	/* Load the certificate and the private key into our own store. We need to do
	 * this in the client case, to support fingerprint authentication. In that case,
	 * we may be presented no matching root certificate, but we must provide ours.
	 * The only way to do that is via the cert callback interface, but for it we
	 * need to load certificates into our private store.
	 * rgerhards, 2008-05-26
	 */
	static rsRetVal
	osslLoadOurCertKey(nsd_ossl_t *pThis)
	{
		DEFiRet;
		int gnuRet;
		gnutls_datum_t data = { NULL, 0 };
		uchar *keyFile;
		uchar *certFile;

		ISOBJ_TYPE_assert(pThis, nsd_ossl);

		certFile = glbl.GetDfltNetstrmDrvrCertFile();
		keyFile = glbl.GetDfltNetstrmDrvrKeyFile();

		if(certFile == NULL || keyFile == NULL) {
			/* in this case, we can not set our certificate. If we are
			 * a client and the server is running in "anon" auth mode, this
			 * may be well acceptable. In other cases, we will see some
			 * more error messages down the road. -- rgerhards, 2008-07-02
			 */
			dbgprintf("our certificate is not set, file name values are cert: '%s', key: '%s'\n",
				  certFile, keyFile);
			ABORT_FINALIZE(RS_RET_CERTLESS);
		}

		/* try load certificate */
		CHKiRet(readFile(certFile, &data));
		CHKgnutls(gnutls_x509_crt_init(&pThis->ourCert));
		pThis->bOurCertIsInit = 1;
		CHKgnutls(gnutls_x509_crt_import(pThis->ourCert, &data, GNUTLS_X509_FMT_PEM));
		free(data.data);
		data.data = NULL;

		/* try load private key */
		CHKiRet(readFile(keyFile, &data));
		CHKgnutls(gnutls_x509_privkey_init(&pThis->ourKey));
		pThis->bOurKeyIsInit = 1;
		CHKgnutls(gnutls_x509_privkey_import(pThis->ourKey, &data, GNUTLS_X509_FMT_PEM));
		free(data.data);

	finalize_it:
		if(iRet != RS_RET_OK) {
			if(data.data != NULL)
				free(data.data);
			if(pThis->bOurCertIsInit) {
				gnutls_x509_crt_deinit(pThis->ourCert);
				pThis->bOurCertIsInit = 0;
			}
			if(pThis->bOurKeyIsInit) {
				gnutls_x509_privkey_deinit(pThis->ourKey);
				pThis->bOurKeyIsInit = 0;
			}
		}
		RETiRet;
	}


	/* This callback must be associated with a session by calling
	 * gnutls_certificate_client_set_retrieve_function(session, cert_callback),
	 * before a handshake. We will always return the configured certificate,
	 * even if it does not match the peer's trusted CAs. This is necessary
	 * to use self-signed certs in fingerprint mode. And, yes, this usage
	 * of the callback is quite a hack. But it seems the only way to
	 * obey to the IETF -transport-tls I-D.
	 * Note: GnuTLS requires the function to return 0 on success and
	 * -1 on failure.
	 * rgerhards, 2008-05-27
	 */
	static int
	osslClientCertCallback(gnutls_session_t session,
		__attribute__((unused)) const gnutls_datum_t* req_ca_rdn,
		int __attribute__((unused)) nreqs,
		__attribute__((unused)) const gnutls_pk_algorithm_t* sign_algos,
		int __attribute__((unused)) sign_algos_length,
	#if HAVE_GNUTLS_CERTIFICATE_SET_RETRIEVE_FUNCTION
		gnutls_retr2_st* st
	#else
		gnutls_retr_st *st
	#endif
		)
	{
		nsd_ossl_t *pThis;

		pThis = (nsd_ossl_t*) gnutls_session_get_ptr(session);

	#if HAVE_GNUTLS_CERTIFICATE_SET_RETRIEVE_FUNCTION
		st->cert_type = GNUTLS_CRT_X509;
	#else
		st->type = GNUTLS_CRT_X509;
	#endif
		st->ncerts = 1;
		st->cert.x509 = &pThis->ourCert;
		st->key.x509 = pThis->ourKey;
		st->deinit_all = 0;

		return 0;
	}


	/* This function extracts some information about this session's peer
	 * certificate. Works for X.509 certificates only. Adds all
	 * of the info to a cstr_t, which is handed over to the caller.
	 * Caller must destruct it when no longer needed.
	 * rgerhards, 2008-05-21
	 */
	static rsRetVal
	osslGetCertInfo(nsd_ossl_t *const pThis, cstr_t **ppStr)
	{
		uchar szBufA[1024];
		uchar *szBuf = szBufA;
		size_t szBufLen = sizeof(szBufA), tmp;
		unsigned int algo, bits;
		time_t expiration_time, activation_time;
		const gnutls_datum_t *cert_list;
		unsigned cert_list_size = 0;
		gnutls_x509_crt_t cert;
		cstr_t *pStr = NULL;
		int gnuRet;
		DEFiRet;
		unsigned iAltName;

		assert(ppStr != NULL);
		ISOBJ_TYPE_assert(pThis, nsd_ossl);

		if(gnutls_certificate_type_get(pThis->sess) != GNUTLS_CRT_X509)
			return RS_RET_TLS_CERT_ERR;

		cert_list = gnutls_certificate_get_peers(pThis->sess, &cert_list_size);
		CHKiRet(rsCStrConstructFromszStrf(&pStr, "peer provided %d certificate(s). ", cert_list_size));

		if(cert_list_size > 0) {
			/* we only print information about the first certificate */
			CHKgnutls(gnutls_x509_crt_init(&cert));
			CHKgnutls(gnutls_x509_crt_import(cert, &cert_list[0], GNUTLS_X509_FMT_DER));

			expiration_time = gnutls_x509_crt_get_expiration_time(cert);
			activation_time = gnutls_x509_crt_get_activation_time(cert);
			ctime_r(&activation_time, (char*)szBuf);
			szBuf[ustrlen(szBuf) - 1] = '\0'; /* strip linefeed */
			CHKiRet(rsCStrAppendStrf(pStr, "Certificate 1 info: "
				"certificate valid from %s ", szBuf));
			ctime_r(&expiration_time, (char*)szBuf);
			szBuf[ustrlen(szBuf) - 1] = '\0'; /* strip linefeed */
			CHKiRet(rsCStrAppendStrf(pStr, "to %s; ", szBuf));

			/* Extract some of the public key algorithm's parameters */
			algo = gnutls_x509_crt_get_pk_algorithm(cert, &bits);
			CHKiRet(rsCStrAppendStrf(pStr, "Certificate public key: %s; ",
				gnutls_pk_algorithm_get_name(algo)));

			/* names */
			tmp = szBufLen;
			if(gnutls_x509_crt_get_dn(cert, (char*)szBuf, &tmp)
			    == GNUTLS_E_SHORT_MEMORY_BUFFER) {
				szBufLen = tmp;
				szBuf = malloc(tmp);
				gnutls_x509_crt_get_dn(cert, (char*)szBuf, &tmp);
			}
			CHKiRet(rsCStrAppendStrf(pStr, "DN: %s; ", szBuf));

			tmp = szBufLen;
			if(gnutls_x509_crt_get_issuer_dn(cert, (char*)szBuf, &tmp)
			    == GNUTLS_E_SHORT_MEMORY_BUFFER) {
				szBufLen = tmp;
				szBuf = realloc((szBuf == szBufA) ? NULL : szBuf, tmp);
				gnutls_x509_crt_get_issuer_dn(cert, (char*)szBuf, &tmp);
			}
			CHKiRet(rsCStrAppendStrf(pStr, "Issuer DN: %s; ", szBuf));

			/* dNSName alt name */
			iAltName = 0;
			while(1) { /* loop broken below */
				tmp = szBufLen;
				gnuRet = gnutls_x509_crt_get_subject_alt_name(cert, iAltName,
						szBuf, &tmp, NULL);
				if(gnuRet == GNUTLS_E_SHORT_MEMORY_BUFFER) {
					szBufLen = tmp;
					szBuf = realloc((szBuf == szBufA) ? NULL : szBuf, tmp);
					continue;
				} else if(gnuRet < 0)
					break;
				else if(gnuRet == GNUTLS_SAN_DNSNAME) {
					/* we found it! */
					CHKiRet(rsCStrAppendStrf(pStr, "SAN:DNSname: %s; ", szBuf));
					/* do NOT break, because there may be multiple dNSName's! */
				}
				++iAltName;
			}

			gnutls_x509_crt_deinit(cert);
		}

		cstrFinalize(pStr);
		*ppStr = pStr;

	finalize_it:
		if(iRet != RS_RET_OK) {
			if(pStr != NULL)
				rsCStrDestruct(&pStr);
		}
		if(szBuf != szBufA)
			free(szBuf);

		RETiRet;
	}
#endif


#if 0 /* copied CODE needs to be converted first! */

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

#endif

/* globally initialize GnuTLS */
static rsRetVal
osslGlblInit(void)
{
	DEFiRet;
	DBGPRINTF("openssl: entering osslGlblInit\n");
	const char *caFile, *certFile, *keyFile;

	/*TODO: pascal: setup multithreading */
	if(!SSL_library_init()) {
		errmsg.LogError(0, RS_RET_NO_ERRCODE, "Error: OpenSSL initialization failed!");
	}

	/* Load readable error strings */
	SSL_load_error_strings();
	ERR_load_BIO_strings();
	ERR_load_crypto_strings();

	caFile = (const char *) glbl.GetDfltNetstrmDrvrCAF();
	if(caFile == NULL) {
		errmsg.LogError(0, RS_RET_CA_CERT_MISSING, "Error: CA certificate is not set, cannot continue");
		ABORT_FINALIZE(RS_RET_CA_CERT_MISSING);
	}
	certFile = (const char *) glbl.GetDfltNetstrmDrvrCertFile();
	if(certFile == NULL) {
		errmsg.LogError(0, RS_RET_CERT_MISSING, "Error: Certificate file is not set, cannot continue");
		ABORT_FINALIZE(RS_RET_CERT_MISSING);

	}
	keyFile = (const char *) glbl.GetDfltNetstrmDrvrKeyFile();
	if(keyFile == NULL) {
		errmsg.LogError(0, RS_RET_CERTKEY_MISSING, "Error: Key file is not set, cannot continue");
		ABORT_FINALIZE(RS_RET_CERTKEY_MISSING);

	}
	ctx = SSL_CTX_new(SSLv23_method());
	if(SSL_CTX_load_verify_locations(ctx, caFile, NULL) != 1) {
		errmsg.LogError(0, RS_RET_NO_ERRCODE, "Error: CA certificate could not be accessed."
				" Is the file at the right path? And do we have the permissions?");
		ABORT_FINALIZE(RS_RET_NO_ERRCODE);
	}
	if(SSL_CTX_use_certificate_file(ctx, certFile, SSL_FILETYPE_PEM) != 1) {
		errmsg.LogError(0, RS_RET_NO_ERRCODE, "Error: Certificate file could not be "
				"accessed. Is the file at the right path? And do we have the "
				"permissions?");
		ABORT_FINALIZE(RS_RET_NO_ERRCODE);
	}
	if(SSL_CTX_use_PrivateKey_file(ctx, keyFile, SSL_FILETYPE_PEM) != 1) {
		errmsg.LogError(0, RS_RET_NO_ERRCODE, "Error: Key file could not be accessed. "
				"Is the file at the right path? And do we have the permissions?");
		ABORT_FINALIZE(RS_RET_NO_ERRCODE);
	}

	/* Set CTX Options */
	SSL_CTX_set_cipher_list(ctx,"ALL");  /* Support all ciphers */

// --- TODO: HANDLE based on TLS MODE!
	/* pascal: wird bei gnutls in methode gnutlsInitSession gemacht!!!*/
	SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT, verify_callback);
	/*TODO: pascal: Wie tief sollen Ketten geprüft werden? Zur Zeit 4 */
	SSL_CTX_set_verify_depth(ctx, 4);
// ---

	// TODO: Use higher timeout on release !
	SSL_CTX_set_timeout(ctx, 5);
	SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);

	bGlblSrvrInitDone = 1;

finalize_it:
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

	ISOBJ_TYPE_assert(pThis, nsd_ossl);
	DBGPRINTF("osslRecordRecv: start\n");

	lenRcvd = SSL_read(pThis->ssl, pThis->pszRcvBuf, NSD_OSSL_MAX_RCVBUF);
	if(lenRcvd > 0) {
		pThis->lenRcvBuf = lenRcvd;
		pThis->ptrRcvBuf = 0;
	} else {
		err = SSL_get_error(pThis->ssl, lenRcvd);
		if(	err != SSL_ERROR_ZERO_RETURN &&
			err != SSL_ERROR_WANT_READ &&
			err != SSL_ERROR_WANT_WRITE) {
				getLastSSLErrorMsg(lenRcvd, pThis->ssl, "osslRecordRecv");
				ABORT_FINALIZE(RS_RET_NO_ERRCODE);
		} else {
			pThis->rtryCall =  osslRtry_recv;
			ABORT_FINALIZE(RS_RET_RETRY);
		}
	}

	// TODO: Is retry logic needed?

finalize_it:
	dbgprintf("osslRecordRecv return. nsd %p, iRet %d, lenRcvd %d, lenRcvBuf %d, ptrRcvBuf %d\n",
	pThis, iRet, (int) lenRcvd, pThis->lenRcvBuf, pThis->ptrRcvBuf);
	RETiRet;
}

#if 0 /* copied CODE needs to be converted first! */

/* add our own certificate to the certificate set, so that the peer
 * can identify us. Please note that we try to use mutual authentication,
 * so we always add a cert, even if we are in the client role (later,
 * this may be controlled by a config setting).
 * rgerhards, 2008-05-15
 */
static rsRetVal
osslAddOurCert(void)
{
	int gnuRet = 0;
	uchar *keyFile;
	uchar *certFile;
	uchar *pGnuErr; /* for GnuTLS error reporting */
	DEFiRet;

	certFile = glbl.GetDfltNetstrmDrvrCertFile();
	keyFile = glbl.GetDfltNetstrmDrvrKeyFile();
	dbgprintf("OSSL certificate file: '%s'\n", certFile);
	dbgprintf("OSSL key file: '%s'\n", keyFile);
	if(certFile == NULL) {
		LogError(0, RS_RET_CERT_MISSING, "error: certificate file is not set, cannot "
				"continue");
		ABORT_FINALIZE(RS_RET_CERT_MISSING);
	}
	if(keyFile == NULL) {
		LogError(0, RS_RET_CERTKEY_MISSING, "error: key file is not set, cannot "
				"continue");
		ABORT_FINALIZE(RS_RET_CERTKEY_MISSING);
	}
	CHKgnutls(gnutls_certificate_set_x509_key_file(xcred, (char*)certFile, (char*)keyFile, GNUTLS_X509_FMT_PEM));

finalize_it:
	if(iRet != RS_RET_OK && iRet != RS_RET_CERT_MISSING && iRet != RS_RET_CERTKEY_MISSING) {
!!!		pGnuErr = osslStrerror(gnuRet);
		errno = 0;
		LogError(0, iRet, "error adding our certificate. GnuTLS error %d, message: '%s', "
				"key: '%s', cert: '%s'", gnuRet, pGnuErr, keyFile, certFile);
		free(pGnuErr);
	}
	RETiRet;
}
#endif

static rsRetVal
osslInitSession(nsd_ossl_t *pThis, nsd_ossl_t *pServer)
{
	DEFiRet;
	BIO *client;

	if(!(pThis->ssl = SSL_new(ctx))) {
		getLastSSLErrorMsg(0, pThis->ssl, "Error while creating SSL context in osslInitSession");
	}
	client = BIO_pop(pServer->acc);

	dbgprintf("osslInitSession: Init client BIO[%p] done\n", (void *)client);

/* TODO: Correct? Set to NON blocking ! */
BIO_set_nbio( client, 1 );

	SSL_set_bio(pThis->ssl, client, client);
	SSL_set_accept_state(pThis->ssl);

	pThis->bHaveSess = 1;

	/* we are done */
	FINALIZE;

finalize_it:
	RETiRet;
}

#if 0 /* copied CODE needs to be converted first! */

/* Obtain the CN from the DN field and hand it back to the caller
 * (which is responsible for destructing it). We try to follow
 * RFC2253 as far as it makes sense for our use-case. This function
 * is considered a compromise providing good-enough correctness while
 * limiting code size and complexity. If a problem occurs, we may enhance
 * this function. A (pointer to a) certificate must be caller-provided.
 * If no CN is contained in the cert, no string is returned
 * (*ppstrCN remains NULL). *ppstrCN MUST be NULL on entry!
 * rgerhards, 2008-05-22
 */
static rsRetVal
osslGetCN(gnutls_x509_crt_t *pCert, cstr_t **ppstrCN)
{
	DEFiRet;
	int gnuRet;
	int i;
	int bFound;
	cstr_t *pstrCN = NULL;
	size_t size;
	/* big var the last, so we hope to have all we usually neeed within one mem cache line */
	uchar szDN[1024]; /* this should really be large enough for any non-malicious case... */

	assert(pCert != NULL);
	assert(ppstrCN != NULL);
	assert(*ppstrCN == NULL);

	size = sizeof(szDN);
	CHKgnutls(gnutls_x509_crt_get_dn(*pCert, (char*)szDN, &size));

	/* now search for the CN part */
	i = 0;
	bFound = 0;
	while(!bFound && szDN[i] != '\0') {
		/* note that we do not overrun our string due to boolean shortcut
		 * operations. If we have '\0', the if does not match and evaluation
		 * stops. Order of checks is obviously important!
		 */
		if(szDN[i] == 'C' && szDN[i+1] == 'N' && szDN[i+2] == '=') {
			bFound = 1;
			i += 2;
		}
		i++;

	}

	if(!bFound) {
		FINALIZE; /* we are done */
	}

	/* we found a common name, now extract it */
	CHKiRet(cstrConstruct(&pstrCN));
	while(szDN[i] != '\0' && szDN[i] != ',') {
		if(szDN[i] == '\\') {
			/* hex escapes are not implemented */
			++i; /* escape char processed */
			if(szDN[i] == '\0')
				ABORT_FINALIZE(RS_RET_CERT_INVALID_DN);
			CHKiRet(cstrAppendChar(pstrCN, szDN[i]));
		} else {
			CHKiRet(cstrAppendChar(pstrCN, szDN[i]));
		}
		++i; /* char processed */
	}
	cstrFinalize(pstrCN);

	/* we got it - we ignore the rest of the DN string (if any). So we may
	 * not detect if it contains more than one CN
	 */

	*ppstrCN = pstrCN;

finalize_it:
	if(iRet != RS_RET_OK) {
		if(pstrCN != NULL)
			cstrDestruct(&pstrCN);
	}

	RETiRet;
}


/* Check the peer's ID in fingerprint auth mode.
 * rgerhards, 2008-05-22
 */
static rsRetVal
osslChkPeerFingerprint(nsd_ossl_t *pThis, gnutls_x509_crt_t *pCert)
{
	uchar fingerprint[20];
	size_t size;
	cstr_t *pstrFingerprint = NULL;
	int bFoundPositiveMatch;
	permittedPeers_t *pPeer;
	int gnuRet;
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, nsd_ossl);

	/* obtain the SHA1 fingerprint */
	size = sizeof(fingerprint);
	CHKgnutls(gnutls_x509_crt_get_fingerprint(*pCert, GNUTLS_DIG_SHA1, fingerprint, &size));
	CHKiRet(GenFingerprintStr(fingerprint, size, &pstrFingerprint));
	dbgprintf("peer's certificate SHA1 fingerprint: %s\n", cstrGetSzStrNoNULL(pstrFingerprint));

	/* now search through the permitted peers to see if we can find a permitted one */
	bFoundPositiveMatch = 0;
	pPeer = pThis->pPermPeers;
	while(pPeer != NULL && !bFoundPositiveMatch) {
		if(!rsCStrSzStrCmp(pstrFingerprint, pPeer->pszID, strlen((char*) pPeer->pszID))) {
			bFoundPositiveMatch = 1;
		} else {
			pPeer = pPeer->pNext;
		}
	}

	if(!bFoundPositiveMatch) {
		dbgprintf("invalid peer fingerprint, not permitted to talk to it\n");
		if(pThis->bReportAuthErr == 1) {
			errno = 0;
			LogError(0, RS_RET_INVALID_FINGERPRINT, "error: peer fingerprint '%s' unknown - we are "
					"not permitted to talk to it", cstrGetSzStrNoNULL(pstrFingerprint));
			pThis->bReportAuthErr = 0;
		}
		ABORT_FINALIZE(RS_RET_INVALID_FINGERPRINT);
	}

finalize_it:
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
osslChkOnePeerName(nsd_ossl_t *pThis, uchar *pszPeerID, int *pbFoundPositiveMatch)
{
	permittedPeers_t *pPeer;
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, nsd_ossl);
	assert(pszPeerID != NULL);
	assert(pbFoundPositiveMatch != NULL);

	if(pThis->pPermPeers) { /* do we have configured peer IDs? */
		pPeer = pThis->pPermPeers;
		while(pPeer != NULL) {
			CHKiRet(net.PermittedPeerWildcardMatch(pPeer, pszPeerID, pbFoundPositiveMatch));
			if(*pbFoundPositiveMatch)
				break;
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
	RETiRet;
}


/* Check the peer's ID in name auth mode.
 * rgerhards, 2008-05-22
 */
static rsRetVal
osslChkPeerName(nsd_ossl_t *pThis, gnutls_x509_crt_t *pCert)
{
	uchar lnBuf[256];
	char szAltName[1024]; /* this is sufficient for the DNSNAME... */
	int iAltName;
	size_t szAltNameLen;
	int bFoundPositiveMatch;
	cstr_t *pStr = NULL;
	cstr_t *pstrCN = NULL;
	int gnuRet;
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, nsd_ossl);

	bFoundPositiveMatch = 0;
	CHKiRet(rsCStrConstruct(&pStr));

	/* first search through the dNSName subject alt names */
	iAltName = 0;
	while(!bFoundPositiveMatch) { /* loop broken below */
		szAltNameLen = sizeof(szAltName);
		gnuRet = gnutls_x509_crt_get_subject_alt_name(*pCert, iAltName,
				szAltName, &szAltNameLen, NULL);
		if(gnuRet < 0)
			break;
		else if(gnuRet == GNUTLS_SAN_DNSNAME) {
			dbgprintf("subject alt dnsName: '%s'\n", szAltName);
			snprintf((char*)lnBuf, sizeof(lnBuf), "DNSname: %s; ", szAltName);
			CHKiRet(rsCStrAppendStr(pStr, lnBuf));
			CHKiRet(osslChkOnePeerName(pThis, (uchar*)szAltName, &bFoundPositiveMatch));
			/* do NOT break, because there may be multiple dNSName's! */
		}
		++iAltName;
	}

	if(!bFoundPositiveMatch) {
		/* if we did not succeed so far, we try the CN part of the DN... */
		CHKiRet(osslGetCN(pCert, &pstrCN));
		if(pstrCN != NULL) { /* NULL if there was no CN present */
			dbgprintf("ossl now checking auth for CN '%s'\n", cstrGetSzStrNoNULL(pstrCN));
			snprintf((char*)lnBuf, sizeof(lnBuf), "CN: %s; ", cstrGetSzStrNoNULL(pstrCN));
			CHKiRet(rsCStrAppendStr(pStr, lnBuf));
			CHKiRet(osslChkOnePeerName(pThis, cstrGetSzStrNoNULL(pstrCN), &bFoundPositiveMatch));
		}
	}

	if(!bFoundPositiveMatch) {
		dbgprintf("invalid peer name, not permitted to talk to it\n");
		if(pThis->bReportAuthErr == 1) {
			cstrFinalize(pStr);
			errno = 0;
			LogError(0, RS_RET_INVALID_FINGERPRINT, "error: peer name not authorized -  "
					"not permitted to talk to it. Names: %s",
					cstrGetSzStrNoNULL(pStr));
			pThis->bReportAuthErr = 0;
		}
		ABORT_FINALIZE(RS_RET_INVALID_FINGERPRINT);
	}

finalize_it:
	if(pStr != NULL)
		rsCStrDestruct(&pStr);
	if(pstrCN != NULL)
		rsCStrDestruct(&pstrCN);
	RETiRet;
}


/* check the ID of the remote peer - used for both fingerprint and
 * name authentication. This is common code. Will call into specific
 * drivers once the certificate has been obtained.
 * rgerhards, 2008-05-08
 */
static rsRetVal
osslChkPeerID(nsd_ossl_t *pThis)
{
	const gnutls_datum_t *cert_list;
	unsigned int list_size = 0;
	gnutls_x509_crt_t cert;
	int bMustDeinitCert = 0;
	int gnuRet;
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, nsd_ossl);

	/* This function only works for X.509 certificates.  */
	if(gnutls_certificate_type_get(pThis->sess) != GNUTLS_CRT_X509)
		return RS_RET_TLS_CERT_ERR;

	cert_list = gnutls_certificate_get_peers(pThis->sess, &list_size);

	if(list_size < 1) {
		if(pThis->bReportAuthErr == 1) {
			errno = 0;
			LogError(0, RS_RET_TLS_NO_CERT, "error: peer did not provide a certificate, "
					"not permitted to talk to it");
			pThis->bReportAuthErr = 0;
		}
		ABORT_FINALIZE(RS_RET_TLS_NO_CERT);
	}

	/* If we reach this point, we have at least one valid certificate.
	 * We always use only the first certificate. As of GnuTLS documentation, the
	 * first certificate always contains the remote peer's own certificate. All other
	 * certificates are issuer's certificates (up the chain). We are only interested
	 * in the first certificate, which is our peer. -- rgerhards, 2008-05-08
	 */
	CHKgnutls(gnutls_x509_crt_init(&cert));
	bMustDeinitCert = 1; /* indicate cert is initialized and must be freed on exit */
	CHKgnutls(gnutls_x509_crt_import(cert, &cert_list[0], GNUTLS_X509_FMT_DER));

	/* Now we see which actual authentication code we must call.  */
	if(pThis->authMode == OSSL_AUTH_CERTFINGERPRINT) {
		CHKiRet(osslChkPeerFingerprint(pThis, &cert));
	} else {
		assert(pThis->authMode == OSSL_AUTH_CERTNAME);
		CHKiRet(osslChkPeerName(pThis, &cert));
	}

finalize_it:
	if(bMustDeinitCert)
		gnutls_x509_crt_deinit(cert);

	RETiRet;
}


/* Verify the validity of the remote peer's certificate.
 * rgerhards, 2008-05-21
 */
static rsRetVal
osslChkPeerCertValidity(nsd_ossl_t *pThis)
{
	DEFiRet;
	const char *pszErrCause;
	int gnuRet;
	cstr_t *pStr = NULL;
	unsigned stateCert;
	const gnutls_datum_t *cert_list;
	unsigned cert_list_size = 0;
	gnutls_x509_crt_t cert;
	unsigned i;
	time_t ttCert;
	time_t ttNow;

	ISOBJ_TYPE_assert(pThis, nsd_ossl);

	/* check if we have at least one cert */
	cert_list = gnutls_certificate_get_peers(pThis->sess, &cert_list_size);
	if(cert_list_size < 1) {
		errno = 0;
		LogError(0, RS_RET_TLS_NO_CERT,
			"peer did not provide a certificate, not permitted to talk to it");
		ABORT_FINALIZE(RS_RET_TLS_NO_CERT);
	}

	CHKgnutls(gnutls_certificate_verify_peers2(pThis->sess, &stateCert));

	if(stateCert & GNUTLS_CERT_INVALID) {
		/* provide error details if we have them */
		if(stateCert & GNUTLS_CERT_SIGNER_NOT_FOUND) {
			pszErrCause = "signer not found";
		} else if(stateCert & GNUTLS_CERT_SIGNER_NOT_CA) {
			pszErrCause = "signer is not a CA";
		} else if(stateCert & GNUTLS_CERT_INSECURE_ALGORITHM) {
			pszErrCause = "insecure algorithm";
		} else if(stateCert & GNUTLS_CERT_REVOKED) {
			pszErrCause = "certificate revoked";
		} else {
			pszErrCause = "GnuTLS returned no specific reason";
			dbgprintf("GnuTLS returned no specific reason for GNUTLS_CERT_INVALID, certificate "
				 "status is %d\n", stateCert);
		}
		LogError(0, NO_ERRCODE, "not permitted to talk to peer, certificate invalid: %s",
				pszErrCause);
		osslGetCertInfo(pThis, &pStr);
		LogError(0, NO_ERRCODE, "invalid cert info: %s", cstrGetSzStrNoNULL(pStr));
		cstrDestruct(&pStr);
		ABORT_FINALIZE(RS_RET_CERT_INVALID);
	}

	/* get current time for certificate validation */
	if(datetime.GetTime(&ttNow) == -1)
		ABORT_FINALIZE(RS_RET_SYS_ERR);

	/* as it looks, we need to validate the expiration dates ourselves...
	 * We need to loop through all certificates as we need to make sure the
	 * interim certificates are also not expired.
	 */
	for(i = 0 ; i < cert_list_size ; ++i) {
		CHKgnutls(gnutls_x509_crt_init(&cert));
		CHKgnutls(gnutls_x509_crt_import(cert, &cert_list[i], GNUTLS_X509_FMT_DER));
		ttCert = gnutls_x509_crt_get_activation_time(cert);
		if(ttCert == -1)
			ABORT_FINALIZE(RS_RET_TLS_CERT_ERR);
		else if(ttCert > ttNow) {
			LogError(0, RS_RET_CERT_NOT_YET_ACTIVE, "not permitted to talk to peer: "
					"certificate %d not yet active", i);
			osslGetCertInfo(pThis, &pStr);
			LogError(0, RS_RET_CERT_NOT_YET_ACTIVE,
				"invalid cert info: %s", cstrGetSzStrNoNULL(pStr));
			cstrDestruct(&pStr);
			ABORT_FINALIZE(RS_RET_CERT_NOT_YET_ACTIVE);
		}

		ttCert = gnutls_x509_crt_get_expiration_time(cert);
		if(ttCert == -1)
			ABORT_FINALIZE(RS_RET_TLS_CERT_ERR);
		else if(ttCert < ttNow) {
			LogError(0, RS_RET_CERT_EXPIRED, "not permitted to talk to peer: certificate"
				" %d expired", i);
			osslGetCertInfo(pThis, &pStr);
			LogError(0, RS_RET_CERT_EXPIRED, "invalid cert info: %s", cstrGetSzStrNoNULL(pStr));
			cstrDestruct(&pStr);
			ABORT_FINALIZE(RS_RET_CERT_EXPIRED);
		}
		gnutls_x509_crt_deinit(cert);
	}

finalize_it:
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
			CHKiRet(osslChkPeerCertValidity(pThis));
			CHKiRet(osslChkPeerID(pThis));
			break;
		case OSSL_AUTH_CERTFINGERPRINT:
			CHKiRet(osslChkPeerID(pThis));
			break;
		case OSSL_AUTH_CERTVALID:
			CHKiRet(osslChkPeerCertValidity(pThis));
			break;
		case OSSL_AUTH_CERTANON:
			FINALIZE;
			break;
	}

finalize_it:
	RETiRet;
}
#endif


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
	DBGPRINTF("openssl: entering osslEndSess\n");
	int ret;
	int err;

	if(pThis->bHaveSess) {
		ret = SSL_shutdown(pThis->ssl);
		while(ret == 0) {
// TODO: Not a good Idea to LOOP like this!
			ret = SSL_shutdown(pThis->ssl);
		}
		if(ret < 0) {
			err = SSL_get_error(pThis->ssl, ret);
			if(err != SSL_ERROR_ZERO_RETURN && err != SSL_ERROR_WANT_READ &&
				err != SSL_ERROR_WANT_WRITE) {
				errmsg.LogError(0, RS_RET_NO_ERRCODE, "Error while closing "
						"session: [%d] %s", err,
						ERR_error_string(err, NULL));
				errmsg.LogError(0, RS_RET_NO_ERRCODE, "Error is: %s",
						ERR_reason_error_string(err));
			}
		}
		pThis->bHaveSess = 0;
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
	if(pThis->iMode == 1) {
		osslEndSess(pThis);
	}

	if(pThis->pTcp != NULL) {
		nsd_ptcp.Destruct(&pThis->pTcp);
	}

	if(pThis->pszConnectHost != NULL) {
		free(pThis->pszConnectHost);
	}

	if(pThis->pszRcvBuf == NULL) {
		free(pThis->pszRcvBuf);
	}

	if(pThis->bHaveSess) {
		SSL_shutdown(pThis->ssl);
	}
ENDobjDestruct(nsd_ossl)


/* Set the driver mode. For us, this has the following meaning:
 * 0 - work in plain tcp mode, without tls (e.g. before a STARTTLS)
 * 1 - work in TLS mode
 * rgerhards, 2008-04-28
 */
static rsRetVal
SetMode(nsd_t *pNsd, int mode)
{
	DEFiRet;
	nsd_ossl_t *pThis = (nsd_ossl_t*) pNsd;

	ISOBJ_TYPE_assert((pThis), nsd_ossl);
	if(mode != 0 && mode != 1) {
		errmsg.LogError(0, RS_RET_INVALID_DRVR_MODE, "error: driver mode %d not supported by"
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
SetAuthMode(nsd_t *pNsd, uchar *mode)
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
		errmsg.LogError(0, RS_RET_VALUE_NOT_IN_THIS_MODE, "authentication not supported by "
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

	DBGPRINTF("SetSock: Setting sock %d\n", sock);
	nsd_ptcp.SetSock(pThis->pTcp, sock);

	RETiRet;
}


/* Provide access to the underlying OS socket. This is primarily
 * useful for other drivers (like nsd_ossl) who utilize ourselfs
 * for some of their functionality.
 */
static rsRetVal
SetBio(nsd_t *pNsd, BIO *acc)
{
	DEFiRet;
	nsd_ossl_t *pThis = (nsd_ossl_t*) pNsd;
	ISOBJ_TYPE_assert((pThis), nsd_ossl);
	assert(acc != NULL);

	DBGPRINTF("SetBio: Setting BIO %p\n", acc);
	pThis->acc = acc;

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



/* initialize the tcp socket for a listner
 * Here, we use the ptcp driver - because there is nothing special
 * at this point with OpenSSL. Things become special once we accept
 * a session, but not during listener setup.
 */
static rsRetVal
LstnInit(netstrms_t *pNS, void *pUsr, rsRetVal(*fAddLstn)(void*,netstrm_t*),
	 uchar *pLstnPort, uchar *pLstnIP, int iSessMax)
{
	DEFiRet;

	nsd_t *pNewNsd = NULL;
	netstrm_t *pNewStrm = NULL;
	BIO *acc;
	int sock;
	assert(fAddLstn != NULL);
	assert(pLstnPort != NULL);
	assert(iSessMax >= 0);

	dbgprintf("LstnInit: entering LstnInit for %s:%s SessMAx=%d\n", pLstnIP, pLstnPort, iSessMax);

	acc = BIO_new_accept((const char*)pLstnPort);
	if(!acc) {
		errmsg.LogError(0, RS_RET_NO_ERRCODE, "Error creating server socket");
		ABORT_FINALIZE(RS_RET_NO_ERRCODE);
	}

DBGPRINTF("LstnInit: Set BIO NON BLOCKING\n");
BIO_set_nbio( acc, 1 );

	DBGPRINTF("LstnInit: Server socket created\n");
	if(BIO_do_accept(acc) <= 0) {
		errmsg.LogError(0, RS_RET_NO_ERRCODE, "Error binding server socket");
		ABORT_FINALIZE(RS_RET_NO_ERRCODE);
	}
	DBGPRINTF("LstnInit: Server socket bound (BIO_do_accept)\n");

	BIO_set_callback(acc, BIO_debug_callback);
	DBGPRINTF("LstnInit: Set BIO to NON BLocking socket (BIO_set_nbio_accept)\n");
	BIO_set_nbio_accept(acc, 1);

	CHKiRet(nsd_osslConstruct( (nsd_ossl_t**) &pNewNsd));
	dbgprintf("LstnInit: after construct\n");

	CHKiRet(SetBio(pNewNsd, acc));
	/* Get socket from BIO and set */
	BIO_get_fd(acc, &sock);
	CHKiRet(SetSock(pNewNsd, sock));

	CHKiRet(SetMode(pNewNsd, netstrms.GetDrvrMode(pNS)));
	CHKiRet(SetAuthMode(pNewNsd, netstrms.GetDrvrAuthMode(pNS)));
	CHKiRet(SetPermPeers(pNewNsd, netstrms.GetDrvrPermPeers(pNS)));
	CHKiRet(netstrms.CreateStrm(pNS, &pNewStrm));
	pNewStrm->pDrvrData = (nsd_t*) pNewNsd;

	CHKiRet(fAddLstn(pUsr, pNewStrm));

finalize_it:
	if(iRet != RS_RET_OK) {
		if(pNewStrm != NULL) {
			netstrm.Destruct(&pNewStrm);
		}
	}
	RETiRet;
/* old
	DEFiRet;
	CHKiRet(osslGlblInitLstn());
	iRet = nsd_ptcp.LstnInit(pNS, pUsr, fAddLstn, pLstnPort, pLstnIP, iSessMax);
finalize_it:
	RETiRet;
*/
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
	dbgprintf("GetRemoteHName for %p = %d\n", pNsd, *ppszHName);
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


/* Certificate of the peer is checked
 */
rsRetVal
post_connection_check(SSL *ssl)
{
	DEFiRet;
	DBGPRINTF("post_connection_check: SSL=%p \n", ssl);

/*TODO: IMPLEMENTE X509 cert check HERE !*/
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
	long err;
//	struct sockaddr_in addr;
//	socklen_t addr_size;
	int res;
//	char clientip[20];
	char szDbg[255];
	const SSL_CIPHER* sslCipher;

	ISOBJ_TYPE_assert((pThis), nsd_ossl);
	CHKiRet(nsd_osslConstruct(&pNew));
	CHKiRet(nsd_ptcp.Destruct(&pNew->pTcp));
	CHKiRet(nsd_ptcp.AcceptConnReq(pThis->pTcp, &pNew->pTcp));

// NEEDED ?
// BIO_free(pNew->acc);

	dbgprintf("AcceptConnReq: BIO[%p] accepting connection ... \n", (void *)pThis->acc);
	if( (res = BIO_do_accept(pThis->acc)) <= 0) {
		errmsg.LogError(0, RS_RET_NO_ERRCODE, "Error accepting SSL connection, "
			"BIO_do_accept failed with return %d", res);
		ABORT_FINALIZE(RS_RET_NO_ERRCODE);
	}
// *((char*)0)= 0;

	if(pThis->iMode == 0) {
		/*we are in non-TLS mode, so we are done */
		DBGPRINTF("AcceptConnReq: We are NOT in TLS mode?!\n");
		*ppNew = (nsd_t*) pNew;
		FINALIZE;
	}

	/* If we reach this point, we are in TLS mode */
	CHKiRet(osslInitSession(pNew, pThis));
	// pNew->ssl = pThis->ssl;
	pNew->authMode = pThis->authMode;
	pNew->pPermPeers = pThis->pPermPeers;

	/* We now do the handshake */
// TODO: THIS NEEDS TO BE NON BLOCKING !
	dbgprintf("AcceptConnReq: Starting TLS Handshake for pNew->ssl[%p\n", (void *)pNew->ssl);
	if((res = SSL_accept(pNew->ssl)) <= 0) {
		getLastSSLErrorMsg(res, pNew->ssl, "AcceptConnReq|Error accepting SSL connection");
		ABORT_FINALIZE(RS_RET_NO_ERRCODE);
	}

/*
	dbgprintf("AcceptConnReq: socket client fd=%d\n", SSL_get_fd(pNew->ssl));
	pNew->sock = SSL_get_fd(pNew->ssl);
	addr_size = sizeof(struct sockaddr_in);
	res = getpeername(pNew->sock, (struct sockaddr *)&addr, &addr_size);
	if (res == -1) {
		errmsg.LogError(0, RS_RET_NO_ERRCODE, "Error accepting SSL connection, "
			"failed to get remote address with error %d", errno);
		ABORT_FINALIZE(RS_RET_NO_ERRCODE);
	}
	strcpy(clientip, inet_ntoa(addr.sin_addr));

	// Get readable hostname property!
	prop.CreateStringProp(&pNew->remoteIP, (uchar*) clientip, strlen(clientip));
	dbgprintf("AcceptConnReq: hostname=%s\n", clientip);

	// fill remotehost from ip addr
	memcpy(&pNew->remAddr, &addr, sizeof(struct sockaddr_storage));
	CHKiRet(FillRemHost(pNew, (struct sockaddr_storage*)&addr));
*/

	if((err = post_connection_check(pNew->ssl)) != X509_V_OK) {
		errmsg.LogError(0, RS_RET_NO_ERRCODE, "Error checking SSL object after connection, peer"
				" certificate: %s", X509_verify_cert_error_string(err));
		ABORT_FINALIZE(RS_RET_NO_ERRCODE);
	}

	if (SSL_get_shared_ciphers(pNew->ssl,szDbg, sizeof szDbg) != NULL)
		dbgprintf("AcceptConnReq: Debug Shared ciphers = %s\n", szDbg);

	sslCipher = (const SSL_CIPHER*) SSL_get_current_cipher(pNew->ssl);
	if (sslCipher != NULL)
		dbgprintf("AcceptConnReq: Debug Version: %s Name: %s\n",
			SSL_CIPHER_get_version(sslCipher), SSL_CIPHER_get_name(sslCipher));

	dbgprintf("AcceptConnReq: init done\n");

/*TODO: retry when handshake is not done emediatly because it is non-blocking */

	/* Set socket to SSL mode */
	pNew->iMode = 1;

	*ppNew = (nsd_t*) pNew;
finalize_it:
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
		CHKmalloc(pThis->pszRcvBuf = MALLOC(NSD_OSSL_MAX_RCVBUF));
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
	if (iRet != RS_RET_OK &&
		iRet != RS_RET_RETRY) {
		/* We need to free the receive buffer in error error case unless a retry is wanted. , if we
		 * allocated one. -- rgerhards, 2008-12-03 -- moved here by alorbach, 2015-12-01
		 */
		*pLenBuf = 0;
		free(pThis->pszRcvBuf);
		pThis->pszRcvBuf = NULL;
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
			if(err != SSL_ERROR_ZERO_RETURN && err != SSL_ERROR_WANT_READ &&
				err != SSL_ERROR_WANT_WRITE) {
				/*SSL_ERROR_ZERO_RETURN: TLS connection has been closed. This
				 * result code is returned only if a closure alert has occurred
				 * in the protocol, i.e. if the connection has been closed cleanly.
				 *SSL_ERROR_WANT_READ/WRITE: The operation did not complete, try
				 * again later. */
				errmsg.LogError(0, RS_RET_NO_ERRCODE, "Error while sending data: "
						"[%d] %s", err, ERR_error_string(err, NULL));
				errmsg.LogError(0, RS_RET_NO_ERRCODE, "Error is: %s",
						ERR_reason_error_string(err));
				ABORT_FINALIZE(RS_RET_NO_ERRCODE);
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
//#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored "-Wdeprecated-declarations" /* TODO: FIX Warnings! */
static rsRetVal
Connect(nsd_t *pNsd, int family, uchar *port, uchar *host, char *device)
{
	DEFiRet;
	DBGPRINTF("openssl: entering Connect family=%d, device=%s\n", family, device);
	nsd_ossl_t*pThis = (nsd_ossl_t*) pNsd;
	BIO *conn;
	SSL * ssl;
	long err;
	char *name;

	ISOBJ_TYPE_assert(pThis, nsd_ossl);
	assert(port != NULL);
	assert(host != NULL);

	if((name = malloc(ustrlen(host)+ustrlen(port)+2)) != NULL) {
		name[0] = '\0';
		strcat(name, (char*)host);
		strcat(name, ":");
		strcat(name, (char*)port);
	} else {
		errmsg.LogError(0, RS_RET_NO_ERRCODE, "Error, malloc failed");
	}

// TODO: Change to NON BLOCKING or USE 	nsd_ptcp
	conn = BIO_new_connect(name);
	if(!conn) {
		errmsg.LogError(0, RS_RET_NO_ERRCODE, "Error creating connection Bio");
		ABORT_FINALIZE(RS_RET_NO_ERRCODE);
	}
	if(BIO_do_connect(conn) <= 0) {
		errmsg.LogError(0, RS_RET_NO_ERRCODE, "Error connecting to remote machine");
		ABORT_FINALIZE(RS_RET_NO_ERRCODE);
	}

	if(pThis->iMode == 0) {
		FINALIZE;
	}

	DBGPRINTF("We are in tls mode\n");
	/*if we reach this point we are in tls mode */
	if(!(ssl = SSL_new(ctx))) {
		errmsg.LogError(0, RS_RET_NO_ERRCODE, "Error creating an SSL context");
		ABORT_FINALIZE(RS_RET_NO_ERRCODE);
	}
	SSL_set_bio(ssl, conn, conn);
	if(SSL_connect(ssl) <= 0) {
		errmsg.LogError(0, RS_RET_NO_ERRCODE, "Error connecting SSL object");
		ABORT_FINALIZE(RS_RET_NO_ERRCODE);
	}
	pThis->bHaveSess = 1;
	if((err = post_connection_check(ssl)) != X509_V_OK) {
		errmsg.LogError(0, RS_RET_NO_ERRCODE, "Error checking SSL object after connection, peer"
				" certificate: %s", X509_verify_cert_error_string(err));
		ABORT_FINALIZE(RS_RET_NO_ERRCODE);
	}

finalize_it:
	if(name != NULL) {
		free(name);
	}
	if(iRet != RS_RET_OK) {
		if(pThis->bHaveSess) {
			pThis->bHaveSess = 0;
			SSL_free(ssl);
		}
	}
	RETiRet;
}
//#pragma GCC diagnostic pop


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
	pIf->SetPermPeers =SetPermPeers;
	pIf->CheckConnection = CheckConnection;
	pIf->GetRemoteHName = GetRemoteHName;
	pIf->GetRemoteIP = GetRemoteIP;
	pIf->GetRemAddr = GetRemAddr;
	pIf->EnableKeepAlive = EnableKeepAlive;
	pIf->SetKeepAliveIntvl = SetKeepAliveIntvl;
	pIf->SetKeepAliveProbes = SetKeepAliveProbes;
	pIf->SetKeepAliveTime = SetKeepAliveTime;
finalize_it:
ENDobjQueryInterface(nsd_ossl)


/* exit our class
 */
BEGINObjClassExit(nsd_ossl, OBJ_IS_LOADABLE_MODULE) /* CHANGE class also in END MACRO! */
CODESTARTObjClassExit(nsd_ossl)
	osslGlblExit();	/* shut down GnuTLS */

	/* release objects we no longer need */
	objRelease(nsd_ptcp, LM_NSD_PTCP_FILENAME);
	objRelease(net, LM_NET_FILENAME);
	objRelease(glbl, CORE_COMPONENT);
	objRelease(datetime, CORE_COMPONENT);
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(netstrm, DONT_LOAD_LIB);
	objRelease(netstrms, LM_NETSTRMS_FILENAME);
ENDObjClassExit(nsd_ossl)


/* Initialize the nsd_ossl class. Must be called as the very first method
 * before anything else is called inside this class.
 */
BEGINObjClassInit(nsd_ossl, 1, OBJ_IS_LOADABLE_MODULE) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(datetime, CORE_COMPONENT));
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(net, LM_NET_FILENAME));
	CHKiRet(objUse(nsd_ptcp, LM_NSD_PTCP_FILENAME));
	CHKiRet(objUse(netstrms, LM_NETSTRMS_FILENAME));
	CHKiRet(objUse(netstrm, DONT_LOAD_LIB));

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
/* vi:set ai:
 */
