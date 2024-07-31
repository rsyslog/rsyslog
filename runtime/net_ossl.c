/* net.c
 * Implementation of network-related stuff.
 *
 * File begun on 2023-08-29 by Alorbach (extracted from net.c)
 *
 * Copyright 2023 Andre Lorbach and Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *       -or-
 *       see COPYING.ASL20 in the source distribution
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <strings.h>

#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#include "rsyslog.h"
#include "syslogd-types.h"
#include "module-template.h"
#include "parse.h"
#include "srUtils.h"
#include "obj.h"
#include "errmsg.h"
#include "net.h"
#include "net_ossl.h"
#include "nsd_ptcp.h"
#include "rsconf.h"

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(glbl)
DEFobjCurrIf(net)
DEFobjCurrIf(nsd_ptcp)

/* Prototypes for openssl helper functions */
void net_ossl_lastOpenSSLErrorMsg
	(uchar *fromHost, int ret, SSL *ssl, int severity, const char* pszCallSource, const char* pszOsslApi);
void net_ossl_set_ssl_verify_callback(SSL *pSsl, int flags);
void net_ossl_set_ctx_verify_callback(SSL_CTX *pCtx, int flags);
void net_ossl_set_bio_callback(BIO *conn);
int net_ossl_verify_callback(int status, X509_STORE_CTX *store);
#if OPENSSL_VERSION_NUMBER >= 0x10002000L && !defined(LIBRESSL_VERSION_NUMBER)
rsRetVal net_ossl_apply_tlscgfcmd(net_ossl_t *pThis, uchar *tlscfgcmd);
#endif // OPENSSL_VERSION_NUMBER >= 0x10002000L
rsRetVal net_ossl_chkpeercertvalidity(net_ossl_t *pThis, SSL *ssl, uchar *fromHostIP);
X509* net_ossl_getpeercert(net_ossl_t *pThis, SSL *ssl, uchar *fromHostIP);
rsRetVal net_ossl_peerfingerprint(net_ossl_t *pThis, X509* certpeer, uchar *fromHostIP);
rsRetVal net_ossl_chkpeername(net_ossl_t *pThis, X509* certpeer, uchar *fromHostIP);


/*--------------------------------------MT OpenSSL helpers ------------------------------------------*/
static MUTEX_TYPE *mutex_buf = NULL;
static sbool openssl_initialized = 0; // Avoid multiple initialization / deinitialization

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
	if (openssl_initialized == 1) {
		DBGPRINTF("openssl: multithread setup already initialized\n");
		return 1;
	}

	mutex_buf = (MUTEX_TYPE *)malloc(CRYPTO_num_locks( ) * sizeof(MUTEX_TYPE));
	if (mutex_buf == NULL)
		return 0;
	for (i = 0; i < CRYPTO_num_locks( ); i++)
		MUTEX_SETUP(mutex_buf[i]);

#if OPENSSL_VERSION_NUMBER < 0x10100000L
	CRYPTO_set_id_callback(id_function);
#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */
	CRYPTO_set_locking_callback(locking_function);
	/* The following three CRYPTO_... functions are the OpenSSL functions
	for registering the callbacks we implemented above */
	CRYPTO_set_dynlock_create_callback(dyn_create_function);
	CRYPTO_set_dynlock_lock_callback(dyn_lock_function);
	CRYPTO_set_dynlock_destroy_callback(dyn_destroy_function);

	DBGPRINTF("openssl: multithread setup finished\n");
	openssl_initialized = 1;
	return 1;
}

/* shut down openssl - do this only when you are totally done
 * with openssl.
 */
int opensslh_THREAD_cleanup(void)
{
	int i;
	if (openssl_initialized == 0) {
		DBGPRINTF("openssl: multithread cleanup already done\n");
		return 1;
	}
	if (!mutex_buf)
		return 0;

#if OPENSSL_VERSION_NUMBER < 0x10100000L
	CRYPTO_set_id_callback(NULL);
#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */
	CRYPTO_set_locking_callback(NULL);
	CRYPTO_set_dynlock_create_callback(NULL);
	CRYPTO_set_dynlock_lock_callback(NULL);
	CRYPTO_set_dynlock_destroy_callback(NULL);

	for (i = 0; i < CRYPTO_num_locks( ); i++)
		MUTEX_CLEANUP(mutex_buf[i]);

	free(mutex_buf);
	mutex_buf = NULL;

	DBGPRINTF("openssl: multithread cleanup finished\n");
	openssl_initialized = 0;
	return 1;
}
/*-------------------------------------- MT OpenSSL helpers -----------------------------------------*/


/*--------------------------------------OpenSSL helpers ------------------------------------------*/

/* globally initialize OpenSSL
 */
void
osslGlblInit(void)
{
	DBGPRINTF("osslGlblInit: ENTER\n");

	if((opensslh_THREAD_setup() == 0) ||
#if OPENSSL_VERSION_NUMBER < 0x10100000L
		/* Setup OpenSSL library  < 1.1.0 */
		!SSL_library_init()
#else
		/* Setup OpenSSL library >= 1.1.0 with system default settings */
		OPENSSL_init_ssl(0, NULL) == 0
#endif
		) {
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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

	// Initialize OpenSSL engine library
	ENGINE_load_builtin_engines();
	/* Register all of them for every algorithm they collectively implement */
	ENGINE_register_all_complete();

	// Iterate through all available engines
	ENGINE *osslEngine = ENGINE_get_first();
	const char *engine_id = NULL;
	const char *engine_name = NULL;
	while (osslEngine) {
		// Print engine ID and name if the engine is loaded
		if (ENGINE_get_init_function(osslEngine)) { // Check if engine is initialized
			engine_id = ENGINE_get_id(osslEngine);
			engine_name = ENGINE_get_name(osslEngine);
			DBGPRINTF("osslGlblInit: Loaded Engine: ID = %s, Name = %s\n", engine_id, engine_name);
		}
		osslEngine = ENGINE_get_next(osslEngine);
	}
	// Free the engine reference when done
	ENGINE_free(osslEngine);
#pragma GCC diagnostic pop
}

/* globally de-initialize OpenSSL */
void
osslGlblExit(void)
{
	DBGPRINTF("openssl: entering osslGlblExit\n");
	ENGINE_cleanup();
	ERR_free_strings();
	EVP_cleanup();
	CRYPTO_cleanup_all_ex_data();
}


/* initialize openssl context; called on
 * - listener creation
 * - outbound connection creation
 * Once created, the ctx object is used by-subobjects (accepted inbound connections)
 */
static rsRetVal
net_ossl_osslCtxInit(net_ossl_t *pThis, const SSL_METHOD *method)
{
	DEFiRet;
	int bHaveCA;
	int bHaveCRL;
	int bHaveCert;
	int bHaveKey;
	int bHaveExtraCAFiles;
	const char *caFile, *crlFile, *certFile, *keyFile;
	char *extraCaFiles, *extraCaFile;
	/* Setup certificates */
	caFile = (char*) ((pThis->pszCAFile == NULL) ? glbl.GetDfltNetstrmDrvrCAF(runConf) : pThis->pszCAFile);
	if(caFile == NULL) {
		LogMsg(0, RS_RET_CA_CERT_MISSING, LOG_WARNING,
			"Warning: CA certificate is not set");
		bHaveCA = 0;
	} else {
		dbgprintf("osslCtxInit: OSSL CA file: '%s'\n", caFile);
		bHaveCA	= 1;
	}
	crlFile = (char*) ((pThis->pszCRLFile == NULL) ? glbl.GetDfltNetstrmDrvrCRLF(runConf) : pThis->pszCRLFile);
	if(crlFile == NULL) {
		bHaveCRL = 0;
	} else {
		dbgprintf("osslCtxInit: OSSL CRL file: '%s'\n", crlFile);
		bHaveCRL = 1;
	}
	certFile = (char*) ((pThis->pszCertFile == NULL) ?
		glbl.GetDfltNetstrmDrvrCertFile(runConf) : pThis->pszCertFile);
	if(certFile == NULL) {
		LogMsg(0, RS_RET_CERT_MISSING, LOG_WARNING,
			"Warning: Certificate file is not set");
		bHaveCert = 0;
	} else {
		dbgprintf("osslCtxInit: OSSL CERT file: '%s'\n", certFile);
		bHaveCert = 1;
	}
	keyFile = (char*) ((pThis->pszKeyFile == NULL) ? glbl.GetDfltNetstrmDrvrKeyFile(runConf) : pThis->pszKeyFile);
	if(keyFile == NULL) {
		LogMsg(0, RS_RET_CERTKEY_MISSING, LOG_WARNING,
			"Warning: Key file is not set");
		bHaveKey = 0;
	} else {
		dbgprintf("osslCtxInit: OSSL KEY file: '%s'\n", keyFile);
		bHaveKey = 1;
	}
	extraCaFiles = (char*) ((pThis->pszExtraCAFiles == NULL) ? glbl.GetNetstrmDrvrCAExtraFiles(runConf) :
				pThis->pszExtraCAFiles);
	if(extraCaFiles == NULL) {
		bHaveExtraCAFiles = 0;
	} else {
		dbgprintf("osslCtxInit: OSSL EXTRA CA files: '%s'\n", extraCaFiles);
	        bHaveExtraCAFiles = 1;
	}

	/* Create main CTX Object based on method parameter */
	pThis->ctx = SSL_CTX_new(method);

	if(bHaveExtraCAFiles == 1) {
		while((extraCaFile = strsep(&extraCaFiles, ","))) {
			if(SSL_CTX_load_verify_locations(pThis->ctx, extraCaFile, NULL) != 1) {
				LogError(0, RS_RET_TLS_CERT_ERR, "Error: Extra Certificate file could not be accessed. "
					"Check at least: 1) file path is correct, 2) file exist, "
					"3) permissions are correct, 4) file content is correct. "
					"OpenSSL error info may follow in next messages");
				net_ossl_lastOpenSSLErrorMsg(NULL, 0, NULL, LOG_ERR, "osslCtxInit",
					"SSL_CTX_load_verify_locations");
				ABORT_FINALIZE(RS_RET_TLS_CERT_ERR);
			}
		}
	}
	if(bHaveCA == 1 && SSL_CTX_load_verify_locations(pThis->ctx, caFile, NULL) != 1) {
		LogError(0, RS_RET_TLS_CERT_ERR, "Error: CA certificate could not be accessed. "
				"Check at least: 1) file path is correct, 2) file exist, "
				"3) permissions are correct, 4) file content is correct. "
				"OpenSSL error info may follow in next messages");
		net_ossl_lastOpenSSLErrorMsg(NULL, 0, NULL, LOG_ERR, "osslCtxInit", "SSL_CTX_load_verify_locations");
		ABORT_FINALIZE(RS_RET_TLS_CERT_ERR);
	}
	if(bHaveCRL == 1) {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L && !defined(LIBRESSL_VERSION_NUMBER)
		// Get X509_STORE reference
		X509_STORE *store = SSL_CTX_get_cert_store(pThis->ctx);
		if (!X509_STORE_load_file(store, crlFile)) {
			LogError(0, RS_RET_CRL_INVALID, "Error: CRL could not be accessed. "
					"Check at least: 1) file path is correct, 2) file exist, "
					"3) permissions are correct, 4) file content is correct. "
					"OpenSSL error info may follow in next messages");
			net_ossl_lastOpenSSLErrorMsg(NULL, 0, NULL, LOG_ERR, "osslCtxInit", "X509_STORE_load_file");
			ABORT_FINALIZE(RS_RET_CRL_INVALID);
		}
		X509_STORE_set_flags(store, X509_V_FLAG_CRL_CHECK);
#else
#	if OPENSSL_VERSION_NUMBER >= 0x10002000L
		// Get X509_STORE reference
		X509_STORE *store = SSL_CTX_get_cert_store(pThis->ctx);
		// Load the CRL PEM file
		FILE *fp = fopen(crlFile, "r");
		if(fp == NULL) {
			LogError(0, RS_RET_CRL_MISSING, "Error: CRL could not be accessed. "
					"Check at least: 1) file path is correct, 2) file exist, "
					"3) permissions are correct, 4) file content is correct. "
					"OpenSSL error info may follow in next messages");
			net_ossl_lastOpenSSLErrorMsg(NULL, 0, NULL, LOG_ERR, "osslCtxInit", "fopen");
			ABORT_FINALIZE(RS_RET_CRL_MISSING);
		}
		X509_CRL *crl = PEM_read_X509_CRL(fp, NULL, NULL, NULL);
		fclose(fp);
		if(crl == NULL) {
			LogError(0, RS_RET_CRL_INVALID, "Error: Unable to read CRL."
					"OpenSSL error info may follow in next messages");
			net_ossl_lastOpenSSLErrorMsg(NULL, 0, NULL, LOG_ERR, "osslCtxInit", "PEM_read_X509_CRL");
			ABORT_FINALIZE(RS_RET_CRL_INVALID);
		}
		// Add the CRL to the X509_STORE
		if(!X509_STORE_add_crl(store, crl)) {
			LogError(0, RS_RET_CRL_INVALID, "Error: Unable to add CRL to store."
					"OpenSSL error info may follow in next messages");
			net_ossl_lastOpenSSLErrorMsg(NULL, 0, NULL, LOG_ERR, "osslCtxInit", "X509_STORE_add_crl");
			X509_CRL_free(crl);
			ABORT_FINALIZE(RS_RET_CRL_INVALID);
		}
		// Set the X509_STORE to the SSL_CTX
		// SSL_CTX_set_cert_store(pThis->ctx, store);
		// Enable CRL checking
		X509_VERIFY_PARAM *param = X509_VERIFY_PARAM_new();
		X509_VERIFY_PARAM_set_flags(param, X509_V_FLAG_CRL_CHECK);
		SSL_CTX_set1_param(pThis->ctx, param);
		X509_VERIFY_PARAM_free(param);
#	else
		LogError(0, RS_RET_SYS_ERR, "Warning: TLS library does not support X509_STORE_load_file"
			"(requires OpenSSL 3.x or higher). Cannot use Certificate revocation list (CRL) '%s'.",
			crlFile);
#	endif
#endif
	}
	if(bHaveCert == 1 && SSL_CTX_use_certificate_chain_file(pThis->ctx, certFile) != 1) {
		LogError(0, RS_RET_TLS_CERT_ERR, "Error: Certificate file could not be accessed. "
				"Check at least: 1) file path is correct, 2) file exist, "
				"3) permissions are correct, 4) file content is correct. "
				"OpenSSL error info may follow in next messages");
		net_ossl_lastOpenSSLErrorMsg(NULL, 0, NULL, LOG_ERR, "osslCtxInit",
			"SSL_CTX_use_certificate_chain_file");
		ABORT_FINALIZE(RS_RET_TLS_CERT_ERR);
	}
	if(bHaveKey == 1 && SSL_CTX_use_PrivateKey_file(pThis->ctx, keyFile, SSL_FILETYPE_PEM) != 1) {
		LogError(0, RS_RET_TLS_KEY_ERR , "Error: Key could not be accessed. "
				"Check at least: 1) file path is correct, 2) file exist, "
				"3) permissions are correct, 4) file content is correct. "
				"OpenSSL error info may follow in next messages");
		net_ossl_lastOpenSSLErrorMsg(NULL, 0, NULL, LOG_ERR, "osslCtxInit", "SSL_CTX_use_PrivateKey_file");
		ABORT_FINALIZE(RS_RET_TLS_KEY_ERR);
	}

	/* Set CTX Options */
	SSL_CTX_set_options(pThis->ctx, SSL_OP_NO_SSLv2);		/* Disable insecure SSLv2 Protocol */
	SSL_CTX_set_options(pThis->ctx, SSL_OP_NO_SSLv3);		/* Disable insecure SSLv3 Protocol */
	SSL_CTX_sess_set_cache_size(pThis->ctx,1024);			/* TODO: make configurable? */

	/* Set default VERIFY Options for OpenSSL CTX - and CALLBACK */
	if (pThis->authMode == OSSL_AUTH_CERTANON) {
		dbgprintf("osslCtxInit: SSL_VERIFY_NONE\n");
		net_ossl_set_ctx_verify_callback(pThis->ctx, SSL_VERIFY_NONE);
	} else {
		dbgprintf("osslCtxInit: SSL_VERIFY_PEER\n");
		net_ossl_set_ctx_verify_callback(pThis->ctx, SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT);
	}

	SSL_CTX_set_timeout(pThis->ctx, 30);	/* Default Session Timeout, TODO: Make configureable */
	SSL_CTX_set_mode(pThis->ctx, SSL_MODE_AUTO_RETRY);

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
	dbgprintf("osslCtxInit: openssl to old, cannot use SSL_CTX_set_ecdh_auto."
		"Using SSL_CTX_set_tmp_ecdh with NID_X9_62_prime256v1/() instead.\n");
	SSL_CTX_set_tmp_ecdh(pThis->ctx, EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
#endif
finalize_it:
	RETiRet;
}

/* Helper function to print usefull OpenSSL errors
 */
void net_ossl_lastOpenSSLErrorMsg
	(uchar *fromHost, int ret, SSL *ssl, int severity, const char* pszCallSource, const char* pszOsslApi)
{
	unsigned long un_error = 0;
	int iSSLErr = 0;
	if (ssl == NULL) {
		/* Output Error Info*/
		DBGPRINTF("lastOpenSSLErrorMsg: Error in '%s' with ret=%d\n", pszCallSource, ret);
	} else {
		/* if object is set, get error code */
		iSSLErr = SSL_get_error(ssl, ret);
		/* Output Debug as well */
		DBGPRINTF("lastOpenSSLErrorMsg: %s Error in '%s': '%s(%d)' with ret=%d, errno=%d(%s), sslapi='%s'\n",
			(iSSLErr == SSL_ERROR_SSL ? "SSL_ERROR_SSL" :
			(iSSLErr == SSL_ERROR_SYSCALL ? "SSL_ERROR_SYSCALL" : "SSL_ERROR_UNKNOWN")),
			pszCallSource, ERR_error_string(iSSLErr, NULL),
			iSSLErr,
			ret,
			errno,
			strerror(errno),
			pszOsslApi);

		/* Output error message */
		LogMsg(0, RS_RET_NO_ERRCODE, severity,
			"%s Error in '%s': '%s(%d)' with ret=%d, errno=%d(%s), sslapi='%s'\n",
			(iSSLErr == SSL_ERROR_SSL ? "SSL_ERROR_SSL" :
			(iSSLErr == SSL_ERROR_SYSCALL ? "SSL_ERROR_SYSCALL" : "SSL_ERROR_UNKNOWN")),
			pszCallSource, ERR_error_string(iSSLErr, NULL),
			iSSLErr,
			ret,
			errno,
			strerror(errno),
			pszOsslApi);
	}

	/* Loop through ERR_get_error */
	while ((un_error = ERR_get_error()) > 0){
		LogMsg(0, RS_RET_NO_ERRCODE, severity,
			"net_ossl:remote '%s' OpenSSL Error Stack: %s", fromHost, ERR_error_string(un_error, NULL) );
	}
}

#if OPENSSL_VERSION_NUMBER >= 0x10002000L && !defined(LIBRESSL_VERSION_NUMBER)
/* initialize tls config commands in openssl context
 */
rsRetVal net_ossl_apply_tlscgfcmd(net_ossl_t *pThis, uchar *tlscfgcmd)
{
	DEFiRet;
	char *pCurrentPos;
	char *pNextPos;
	char *pszCmd;
	char *pszValue;
	int iConfErr;

	if (tlscfgcmd == NULL) {
		FINALIZE;
	}

	dbgprintf("net_ossl_apply_tlscgfcmd: Apply tlscfgcmd: '%s'\n", tlscfgcmd);

	/* Set working pointer */
	pCurrentPos = (char*) tlscfgcmd;
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
				pNextPos = (pNextPos == NULL ? index(pCurrentPos, ';') : pNextPos);
				pszValue = (pNextPos == NULL ?
						strdup(pCurrentPos) :
						strndup(pCurrentPos, pNextPos - pCurrentPos));
				pCurrentPos = (pNextPos == NULL ? NULL : pNextPos+1);

				/* Add SSL Conf Command */
				iConfErr = SSL_CONF_cmd(cctx, pszCmd, pszValue);
				if (iConfErr > 0) {
					dbgprintf("net_ossl_apply_tlscgfcmd: Successfully added Command "
						"'%s':'%s'\n",
						pszCmd, pszValue);
				}
				else {
					LogError(0, RS_RET_SYS_ERR, "Failed to added Command: %s:'%s' "
						"in net_ossl_apply_tlscgfcmd with error '%d'",
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
					"OpenSSL error info may follow in next messages",
					tlscfgcmd);
			net_ossl_lastOpenSSLErrorMsg(NULL, 0, NULL, LOG_ERR,
				"net_ossl_apply_tlscgfcmd", "SSL_CONF_CTX_finish");
		}
		SSL_CONF_CTX_free(cctx);
	}

finalize_it:
	RETiRet;
}
#endif // OPENSSL_VERSION_NUMBER >= 0x10002000L

/* Convert a fingerprint to printable data. The  conversion is carried out
 * according IETF I-D syslog-transport-tls-12. The fingerprint string is
 * returned in a new cstr object. It is the caller's responsibility to
 * destruct that object.
 * rgerhards, 2008-05-08
 */
static rsRetVal
net_ossl_genfingerprintstr(uchar *pFingerprint, size_t sizeFingerprint, cstr_t **ppStr, const char* prefix)
{
	cstr_t *pStr = NULL;
	uchar buf[4];
	size_t i;
	DEFiRet;

	CHKiRet(rsCStrConstruct(&pStr));
	CHKiRet(rsCStrAppendStrWithLen(pStr, (uchar*) prefix, strlen(prefix)));
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



/* Perform a match on ONE peer name obtained from the certificate. This name
 * is checked against the set of configured credentials. *pbFoundPositiveMatch is
 * set to 1 if the ID matches. *pbFoundPositiveMatch must have been initialized
 * to 0 by the caller (this is a performance enhancement as we expect to be
 * called multiple times).
 * TODO: implemet wildcards?
 * rgerhards, 2008-05-26
 */
static rsRetVal
net_ossl_chkonepeername(net_ossl_t *pThis, X509 *certpeer, uchar *pszPeerID, int *pbFoundPositiveMatch)
{
	permittedPeers_t *pPeer;
#if OPENSSL_VERSION_NUMBER >= 0x10002000L
	int osslRet;
#endif
	char *x509name = NULL;
	DEFiRet;
	
	if (certpeer == NULL) {
		ABORT_FINALIZE(RS_RET_TLS_NO_CERT);
	}

	ISOBJ_TYPE_assert(pThis, net_ossl);
	assert(pszPeerID != NULL);
	assert(pbFoundPositiveMatch != NULL);

	/* Obtain Namex509 name from subject */
	x509name = X509_NAME_oneline(RSYSLOG_X509_NAME_oneline(certpeer), NULL, 0);

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
			osslRet = X509_check_host(	certpeer, (const char*)pPeer->pszID,
							strlen((const char*)pPeer->pszID), 0, NULL);
			if ( osslRet == 1 ) {
				/* Found Peer cert in allowed Peerslist */
				dbgprintf("net_ossl_chkonepeername: Client ('%s') is allowed (X509_check_host)\n",
					x509name);
				*pbFoundPositiveMatch = 1;
				break;
			} else if ( osslRet < 0 ) {
				net_ossl_lastOpenSSLErrorMsg(NULL, osslRet, NULL, LOG_ERR,
					"net_ossl_chkonepeername", "X509_check_host");
				ABORT_FINALIZE(RS_RET_NO_ERRCODE);
			}
#endif
			/* Check next peer */
			pPeer = pPeer->pNext;
		}
	} else {
		LogMsg(0, RS_RET_TLS_NO_CERT, LOG_WARNING,
			"net_ossl_chkonepeername: Peername check could not be done: "
			"no peernames configured.");
	}
finalize_it:
	if (x509name != NULL){
		OPENSSL_free(x509name);
	}

	RETiRet;
}


/* Check the peer's ID in fingerprint auth mode.
 * rgerhards, 2008-05-22
 */
rsRetVal
net_ossl_peerfingerprint(net_ossl_t *pThis, X509* certpeer, uchar *fromHostIP)
{
	DEFiRet;
	unsigned int n;
	uchar fingerprint[20 /*EVP_MAX_MD_SIZE**/];
	uchar fingerprintSha256[32 /*EVP_MAX_MD_SIZE**/];
	size_t size;
	size_t sizeSha256;
	cstr_t *pstrFingerprint = NULL;
	cstr_t *pstrFingerprintSha256 = NULL;
	int bFoundPositiveMatch;
	permittedPeers_t *pPeer;
	const EVP_MD *fdig = EVP_sha1();
	const EVP_MD *fdigSha256 = EVP_sha256();

	ISOBJ_TYPE_assert(pThis, net_ossl);

	if (certpeer == NULL) {
		ABORT_FINALIZE(RS_RET_TLS_NO_CERT);
	}

	/* obtain the SHA1 fingerprint */
	size = sizeof(fingerprint);
	if (!X509_digest(certpeer,fdig,fingerprint,&n)) {
		dbgprintf("net_ossl_peerfingerprint: error X509cert is not valid!\n");
		ABORT_FINALIZE(RS_RET_INVALID_FINGERPRINT);
	}
	sizeSha256 = sizeof(fingerprintSha256);
	if (!X509_digest(certpeer,fdigSha256,fingerprintSha256,&n)) {
		dbgprintf("net_ossl_peerfingerprint: error X509cert is not valid!\n");
		ABORT_FINALIZE(RS_RET_INVALID_FINGERPRINT);
	}
	CHKiRet(net_ossl_genfingerprintstr(fingerprint, size, &pstrFingerprint, "SHA1"));
	dbgprintf("net_ossl_peerfingerprint: peer's certificate SHA1 fingerprint: %s\n",
		cstrGetSzStrNoNULL(pstrFingerprint));
	CHKiRet(net_ossl_genfingerprintstr(fingerprintSha256, sizeSha256, &pstrFingerprintSha256, "SHA256"));
	dbgprintf("net_ossl_peerfingerprint: peer's certificate SHA256 fingerprint: %s\n",
		cstrGetSzStrNoNULL(pstrFingerprintSha256));

	/* now search through the permitted peers to see if we can find a permitted one */
	bFoundPositiveMatch = 0;
	pPeer = pThis->pPermPeers;
	while(pPeer != NULL && !bFoundPositiveMatch) {
		if(!rsCStrSzStrCmp(pstrFingerprint, pPeer->pszID, strlen((char*) pPeer->pszID))) {
			dbgprintf("net_ossl_peerfingerprint: peer's certificate SHA1 MATCH found: %s\n",
				pPeer->pszID);
			bFoundPositiveMatch = 1;
		} else if(!rsCStrSzStrCmp(pstrFingerprintSha256, pPeer->pszID, strlen((char*) pPeer->pszID))) {
			dbgprintf("net_ossl_peerfingerprint: peer's certificate SHA256 MATCH found: %s\n",
				pPeer->pszID);
			bFoundPositiveMatch = 1;
		} else {
			dbgprintf("net_ossl_peerfingerprint: NOMATCH peer certificate: %s\n", pPeer->pszID);
			pPeer = pPeer->pNext;
		}
	}

	if(!bFoundPositiveMatch) {
		dbgprintf("net_ossl_peerfingerprint: invalid peer fingerprint, not permitted to talk to it\n");
		if(pThis->bReportAuthErr == 1) {
			errno = 0;
			LogMsg(0, RS_RET_INVALID_FINGERPRINT, LOG_WARNING,
				"net_ossl:TLS session terminated with remote syslog server '%s': "
				"Fingerprint check failed, not permitted to talk to %s",
				fromHostIP, cstrGetSzStrNoNULL(pstrFingerprint));
			pThis->bReportAuthErr = 0;
		}
		ABORT_FINALIZE(RS_RET_INVALID_FINGERPRINT);
	}

finalize_it:
	if(pstrFingerprint != NULL)
		cstrDestruct(&pstrFingerprint);
	RETiRet;
}


/* Check the peer's ID in name auth mode.
 */
rsRetVal
net_ossl_chkpeername(net_ossl_t *pThis, X509* certpeer, uchar *fromHostIP)
{
	DEFiRet;
	uchar lnBuf[256];
	int bFoundPositiveMatch;
	cstr_t *pStr = NULL;
	char *x509name = NULL;

	ISOBJ_TYPE_assert(pThis, net_ossl);

	bFoundPositiveMatch = 0;
	CHKiRet(rsCStrConstruct(&pStr));

	/* Obtain Namex509 name from subject */
	x509name = X509_NAME_oneline(RSYSLOG_X509_NAME_oneline(certpeer), NULL, 0);

	dbgprintf("net_ossl_chkpeername: checking - peername '%s' on server '%s'\n", x509name, fromHostIP);
	snprintf((char*)lnBuf, sizeof(lnBuf), "name: %s; ", x509name);
	CHKiRet(rsCStrAppendStr(pStr, lnBuf));
	CHKiRet(net_ossl_chkonepeername(pThis, certpeer, (uchar*)x509name, &bFoundPositiveMatch));

	if(!bFoundPositiveMatch) {
		dbgprintf("net_ossl_chkpeername: invalid peername, not permitted to talk to it\n");
		if(pThis->bReportAuthErr == 1) {
			cstrFinalize(pStr);
			errno = 0;
			LogMsg(0, RS_RET_INVALID_FINGERPRINT, LOG_WARNING,
				"net_ossl:TLS session terminated with remote syslog server: "
				"peer name not authorized, not permitted to talk to %s",
				cstrGetSzStrNoNULL(pStr));
			pThis->bReportAuthErr = 0;
		}
		ABORT_FINALIZE(RS_RET_INVALID_FINGERPRINT);
	} else {
		dbgprintf("net_ossl_chkpeername: permitted to talk!\n");
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
X509*
net_ossl_getpeercert(net_ossl_t *pThis, SSL *ssl, uchar *fromHostIP)
{
	X509* certpeer;

	ISOBJ_TYPE_assert(pThis, net_ossl);

	/* Get peer certificate from SSL */
	certpeer = SSL_get_peer_certificate(ssl);
	if ( certpeer == NULL ) {
		if(pThis->bReportAuthErr == 1 && 1) {
			errno = 0;
			pThis->bReportAuthErr = 0;
			LogMsg(0, RS_RET_TLS_NO_CERT, LOG_WARNING,
				"net_ossl:TLS session terminated with remote syslog server '%s': "
				"Peer check failed, peer did not provide a certificate.", fromHostIP);
		}
	}
	return certpeer;
}


/* Verify the validity of the remote peer's certificate.
 */
rsRetVal
net_ossl_chkpeercertvalidity(net_ossl_t __attribute__((unused)) *pThis, SSL *ssl, uchar *fromHostIP)
{
	DEFiRet;
	int iVerErr = X509_V_OK;

	ISOBJ_TYPE_assert(pThis, net_ossl);
	PermitExpiredCerts* pPermitExpiredCerts = (PermitExpiredCerts*) SSL_get_ex_data(ssl, 1);

	iVerErr = SSL_get_verify_result(ssl);
	if (iVerErr != X509_V_OK) {
		if (iVerErr == X509_V_ERR_CERT_HAS_EXPIRED) {
			if (pPermitExpiredCerts != NULL && *pPermitExpiredCerts == OSSL_EXPIRED_DENY) {
				LogMsg(0, RS_RET_CERT_EXPIRED, LOG_INFO,
					"net_ossl:TLS session terminated with remote syslog server '%s': "
					"not permitted to talk to peer, certificate invalid: "
					"Certificate expired: %s",
					fromHostIP, X509_verify_cert_error_string(iVerErr));
				ABORT_FINALIZE(RS_RET_CERT_EXPIRED);
			} else if (pPermitExpiredCerts != NULL && *pPermitExpiredCerts == OSSL_EXPIRED_WARN) {
				LogMsg(0, RS_RET_NO_ERRCODE, LOG_WARNING,
					"net_ossl:CertValidity check - warning talking to peer '%s': "
					"certificate expired: %s",
					fromHostIP, X509_verify_cert_error_string(iVerErr));
			} else {
				dbgprintf("net_ossl_chkpeercertvalidity: talking to peer '%s': "
					"certificate expired: %s\n",
					fromHostIP, X509_verify_cert_error_string(iVerErr));
			}/* Else do nothing */
		} else if (iVerErr == X509_V_ERR_CERT_REVOKED) {
			LogMsg(0, RS_RET_CERT_REVOKED, LOG_INFO,
				"net_ossl:TLS session terminated with remote syslog server '%s': "
				"not permitted to talk to peer, certificate invalid: "
				"certificate revoked '%s'",
				fromHostIP, X509_verify_cert_error_string(iVerErr));
			ABORT_FINALIZE(RS_RET_CERT_EXPIRED);
		} else {
			LogMsg(0, RS_RET_CERT_INVALID, LOG_INFO,
				"net_ossl:TLS session terminated with remote syslog server '%s': "
				"not permitted to talk to peer, certificate validation failed: %s",
				fromHostIP, X509_verify_cert_error_string(iVerErr));
			ABORT_FINALIZE(RS_RET_CERT_INVALID);
		}
	} else {
		dbgprintf("net_ossl_chkpeercertvalidity: client certificate validation success: %s\n",
			X509_verify_cert_error_string(iVerErr));
	}

finalize_it:
	RETiRet;
}

/* Verify Callback for X509 Certificate validation. Force visibility as this function is not called anywhere but
*  only used as callback!
*/
int
net_ossl_verify_callback(int status, X509_STORE_CTX *store)
{
	char szdbgdata1[256];
	char szdbgdata2[256];

	dbgprintf("verify_callback: status %d\n", status);

	if(status == 0) {
		/* Retrieve all needed pointers */
		X509 *cert = X509_STORE_CTX_get_current_cert(store);
		int depth = X509_STORE_CTX_get_error_depth(store);
		int err = X509_STORE_CTX_get_error(store);
		SSL* ssl = X509_STORE_CTX_get_ex_data(store, SSL_get_ex_data_X509_STORE_CTX_idx());
		int iVerifyMode = SSL_get_verify_mode(ssl);
		nsd_t *pNsdTcp = (nsd_t*) SSL_get_ex_data(ssl, 0);
		PermitExpiredCerts* pPermitExpiredCerts = (PermitExpiredCerts*) SSL_get_ex_data(ssl, 1);

		dbgprintf("verify_callback: Certificate validation failed, Mode (%d)!\n", iVerifyMode);

		X509_NAME_oneline(X509_get_issuer_name(cert), szdbgdata1, sizeof(szdbgdata1));
		X509_NAME_oneline(RSYSLOG_X509_NAME_oneline(cert), szdbgdata2, sizeof(szdbgdata2));

		uchar *fromHost = NULL;
		if (pNsdTcp != NULL) {
			nsd_ptcp.GetRemoteHName(pNsdTcp, &fromHost);
		}

		if (iVerifyMode != SSL_VERIFY_NONE) {
			/* Handle expired Certificates **/
			if (err == X509_V_OK || err == X509_V_ERR_CERT_HAS_EXPIRED) {
				if (pPermitExpiredCerts != NULL && *pPermitExpiredCerts == OSSL_EXPIRED_PERMIT) {
					dbgprintf("verify_callback: EXPIRED cert but PERMITTED at depth: %d \n\t"
						"issuer  = %s\n\t"
						"subject = %s\n\t"
						"err %d:%s\n", depth, szdbgdata1, szdbgdata2,
						err, X509_verify_cert_error_string(err));

					/* Set Status to OK*/
					status = 1;
				}
				else if (pPermitExpiredCerts != NULL && *pPermitExpiredCerts == OSSL_EXPIRED_WARN) {
					LogMsg(0, RS_RET_CERT_EXPIRED, LOG_WARNING,
						"Certificate EXPIRED warning at depth: %d \n\t"
						"issuer  = %s\n\t"
						"subject = %s\n\t"
						"err %d:%s peer '%s'",
						depth, szdbgdata1, szdbgdata2,
						err, X509_verify_cert_error_string(err), fromHost);

					/* Set Status to OK*/
					status = 1;
				}
				else /* also default - if (pPermitExpiredCerts == OSSL_EXPIRED_DENY)*/ {
					LogMsg(0, RS_RET_CERT_EXPIRED, LOG_ERR,
						"Certificate EXPIRED at depth: %d \n\t"
						"issuer  = %s\n\t"
						"subject = %s\n\t"
						"err %d:%s\n\t"
						"not permitted to talk to peer '%s', certificate invalid: "
						"certificate expired",
						depth, szdbgdata1, szdbgdata2,
						err, X509_verify_cert_error_string(err), fromHost);
				}
			} else if (err == X509_V_ERR_CERT_REVOKED) {
				LogMsg(0, RS_RET_CERT_REVOKED, LOG_ERR,
					"Certificate REVOKED at depth: %d \n\t"
					"issuer  = %s\n\t"
					"subject = %s\n\t"
					"err %d:%s\n\t"
					"not permitted to talk to peer '%s', certificate invalid: "
					"certificate revoked",
					depth, szdbgdata1, szdbgdata2,
					err, X509_verify_cert_error_string(err), fromHost);
			} else {
				/* all other error codes cause failure */
				LogMsg(0, RS_RET_NO_ERRCODE, LOG_ERR,
					"Certificate error at depth: %d \n\t"
					"issuer  = %s\n\t"
					"subject = %s\n\t"
					"err %d:%s - peer '%s'",
					depth, szdbgdata1, szdbgdata2,
					err, X509_verify_cert_error_string(err), fromHost);
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
		free(fromHost);
	}

	return status;
}


#if OPENSSL_VERSION_NUMBER >= 0x30000000L && !defined(LIBRESSL_VERSION_NUMBER)
static long
RSYSLOG_BIO_debug_callback_ex(BIO *bio, int cmd, const char __attribute__((unused)) *argp,
			   size_t __attribute__((unused)) len, int argi, long __attribute__((unused)) argl,
			   int ret, size_t __attribute__((unused)) *processed)
#else
static long
RSYSLOG_BIO_debug_callback(BIO *bio, int cmd, const char __attribute__((unused)) *argp,
			int argi, long __attribute__((unused)) argl, long ret)
#endif
{
	long ret2 = ret;
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

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
// Requires at least OpenSSL v1.1.1
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
static int
net_ossl_generate_cookie(SSL *ssl, unsigned char *cookie, unsigned int *cookie_len)
{
	unsigned char result[EVP_MAX_MD_SIZE];
	unsigned int resultlength;
	unsigned char *sslHello;
	unsigned int length;

	sslHello = (unsigned char *) "rsyslog";
	length = strlen((char *)sslHello);

	// Generate the cookie using SHA256 hash
	if (!EVP_Digest(sslHello, length, result, &resultlength, EVP_sha256(), NULL)) {
		return 0;
	}

	memcpy(cookie, result, resultlength);
	*cookie_len = resultlength;
	dbgprintf("net_ossl_generate_cookie: generate cookie SUCCESS\n");

	return 1;
}
#pragma GCC diagnostic pop

static int
net_ossl_verify_cookie(SSL *ssl, const unsigned char *cookie, unsigned int cookie_len)
{
	unsigned char cookie_gen[EVP_MAX_MD_SIZE];
	unsigned int cookie_gen_len;

	// Generate a cookie using the same method as in net_ossl_generate_cookie
	if (!net_ossl_generate_cookie(ssl, cookie_gen, &cookie_gen_len)) {
		dbgprintf("net_ossl_verify_cookie: generate cookie FAILED\n");
		return 0;
	}

	// Check if the generated cookie matches the cookie received
	if (cookie_len == cookie_gen_len && memcmp(cookie, cookie_gen, cookie_len) == 0) {
		dbgprintf("net_ossl_verify_cookie: compare cookie SUCCESS\n");
		return 1;
	}

	dbgprintf("net_ossl_verify_cookie: compare cookie FAILED\n");
	return 0;
}

static rsRetVal
net_ossl_init_engine(__attribute__((unused)) net_ossl_t *pThis)
{
	DEFiRet;
	const char *engine_id = NULL;
	const char *engine_name = NULL;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
	// Get the default RSA engine
	ENGINE *default_engine = ENGINE_get_default_RSA();
	if (default_engine) {
		engine_id = ENGINE_get_id(default_engine);
		engine_name = ENGINE_get_name(default_engine);
		DBGPRINTF("net_ossl_init_engine: Default RSA Engine: ID = %s, Name = %s\n", engine_id, engine_name);

		// Free the engine reference when done
		ENGINE_free(default_engine);
	} else {
		DBGPRINTF("net_ossl_init_engine: No default RSA Engine set.\n");
	}

	/* Setting specific Engine */
	if (runConf != NULL && glbl.GetDfltOpensslEngine(runConf) != NULL) {
		default_engine = ENGINE_by_id((char *)glbl.GetDfltOpensslEngine(runConf));
		if (default_engine && ENGINE_init(default_engine)) {
			/* engine initialised */
			ENGINE_set_default_DSA(default_engine);
			ENGINE_set_default_ciphers(default_engine);

			/* Switch to Engine */
			DBGPRINTF("net_ossl_init_engine: Changed default Engine to %s\n",
				glbl.GetDfltOpensslEngine(runConf));

			/* Release the functional reference from ENGINE_init() */
			ENGINE_finish(default_engine);
		} else {
			LogError(0, RS_RET_VALUE_NOT_SUPPORTED, "error: ENGINE_init failed to load Engine '%s'"
					"ossl netstream driver", glbl.GetDfltOpensslEngine(runConf));
			net_ossl_lastOpenSSLErrorMsg(NULL, 0, NULL, LOG_ERR, "net_ossl_init_engine", "ENGINE_init");
		}
		// Free the engine reference when done
		ENGINE_free(default_engine);
	} else {
		DBGPRINTF("net_ossl_init_engine: use openssl default Engine");
	}
#pragma GCC diagnostic pop

	RETiRet;
}


static rsRetVal
net_ossl_ctx_init_cookie(net_ossl_t *pThis)
{
	DEFiRet;
	// Set our cookie generation and verification callbacks
	SSL_CTX_set_options(pThis->ctx, SSL_OP_COOKIE_EXCHANGE);
	SSL_CTX_set_cookie_generate_cb(pThis->ctx, net_ossl_generate_cookie);
	SSL_CTX_set_cookie_verify_cb(pThis->ctx, net_ossl_verify_cookie);
	RETiRet;
}
#endif // OPENSSL_VERSION_NUMBER >= 0x10100000L

/* ------------------------------ end OpenSSL helpers ------------------------------------------*/

/* ------------------------------ OpenSSL Callback set helpers ---------------------------------*/
void
net_ossl_set_ssl_verify_callback(SSL *pSsl, int flags)
{
	/* Enable certificate valid checking */
	SSL_set_verify(pSsl, flags, net_ossl_verify_callback);
}

void
net_ossl_set_ctx_verify_callback(SSL_CTX *pCtx, int flags)
{
	/* Enable certificate valid checking */
	SSL_CTX_set_verify(pCtx, flags, net_ossl_verify_callback);
}

void
net_ossl_set_bio_callback(BIO *conn)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L && !defined(LIBRESSL_VERSION_NUMBER)
	BIO_set_callback_ex(conn, RSYSLOG_BIO_debug_callback_ex);
#else
	BIO_set_callback(conn, RSYSLOG_BIO_debug_callback);
#endif // OPENSSL_VERSION_NUMBER >= 0x10100000L
}
/* ------------------------------ End OpenSSL Callback set helpers -----------------------------*/


/* Standard-Constructor */
BEGINobjConstruct(net_ossl) /* be sure to specify the object type also in END macro! */
	DBGPRINTF("net_ossl_construct: [%p]\n", pThis);
	pThis->bReportAuthErr = 1;
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	CHKiRet(net_ossl_init_engine(pThis));
finalize_it:
#endif
ENDobjConstruct(net_ossl)

/* destructor for the net_ossl object */
BEGINobjDestruct(net_ossl) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(net_ossl)
	DBGPRINTF("net_ossl_destruct: [%p]\n", pThis);
	/* Free SSL obj also if we do not have a session - or are NOT in TLS mode! */
	if (pThis->ssl != NULL) {
		DBGPRINTF("net_ossl_destruct: [%p] FREE pThis->ssl \n", pThis);
		SSL_free(pThis->ssl);
		pThis->ssl = NULL;
	}
	if(pThis->ctx != NULL && !pThis->ctx_is_copy) {
		SSL_CTX_free(pThis->ctx);
	}
	free((void*) pThis->pszCAFile);
	free((void*) pThis->pszCRLFile);
	free((void*) pThis->pszKeyFile);
	free((void*) pThis->pszCertFile);
	free((void*) pThis->pszExtraCAFiles);
ENDobjDestruct(net_ossl)

/* queryInterface function */
BEGINobjQueryInterface(net_ossl)
CODESTARTobjQueryInterface(net_ossl)
	DBGPRINTF("netosslQueryInterface\n");
	if(pIf->ifVersion != net_osslCURR_IF_VERSION) {/* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}
	pIf->Construct			= (rsRetVal(*)(net_ossl_t**)) net_osslConstruct;
	pIf->Destruct			= (rsRetVal(*)(net_ossl_t**)) net_osslDestruct;
	pIf->osslCtxInit		= net_ossl_osslCtxInit;
	pIf->osslChkpeername		= net_ossl_chkpeername;
	pIf->osslPeerfingerprint	= net_ossl_peerfingerprint;
	pIf->osslGetpeercert		= net_ossl_getpeercert;
	pIf->osslChkpeercertvalidity	= net_ossl_chkpeercertvalidity;
#if OPENSSL_VERSION_NUMBER >= 0x10002000L && !defined(LIBRESSL_VERSION_NUMBER)
	pIf->osslApplyTlscgfcmd		= net_ossl_apply_tlscgfcmd;
#endif // OPENSSL_VERSION_NUMBER >= 0x10002000L
	pIf->osslSetBioCallback		= net_ossl_set_bio_callback;
	pIf->osslSetCtxVerifyCallback	= net_ossl_set_ctx_verify_callback;
	pIf->osslSetSslVerifyCallback	= net_ossl_set_ssl_verify_callback;
	pIf->osslLastOpenSSLErrorMsg	= net_ossl_lastOpenSSLErrorMsg;
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	pIf->osslCtxInitCookie	= net_ossl_ctx_init_cookie;
	pIf->osslInitEngine	= net_ossl_init_engine;
#endif
finalize_it:
ENDobjQueryInterface(net_ossl)


/* exit our class
 */
BEGINObjClassExit(net_ossl, OBJ_IS_CORE_MODULE) /* CHANGE class also in END MACRO! */
CODESTARTObjClassExit(net_ossl)
	DBGPRINTF("netosslClassExit\n");
	/* release objects we no longer need */
	objRelease(nsd_ptcp, LM_NSD_PTCP_FILENAME);
	objRelease(net, LM_NET_FILENAME);
	objRelease(glbl, CORE_COMPONENT);
	/* shut down OpenSSL */
	osslGlblExit();
ENDObjClassExit(net_ossl)


/* Initialize the net_ossl class. Must be called as the very first method
 * before anything else is called inside this class.
 */
BEGINObjClassInit(net_ossl, 1, OBJ_IS_CORE_MODULE) /* class, version */
	DBGPRINTF("net_osslClassInit\n");
	// request objects we use
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(net, LM_NET_FILENAME));
	CHKiRet(objUse(nsd_ptcp, LM_NSD_PTCP_FILENAME));
	// Do global TLS init stuff
	osslGlblInit();
ENDObjClassInit(net_ossl)

/* --------------- here now comes the plumbing that makes as a library module --------------- */

/* vi:set ai:
 */
