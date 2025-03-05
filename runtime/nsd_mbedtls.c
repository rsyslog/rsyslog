/* nsd_mbedtls.c
 *
 * An implementation of the nsd interface for Mbed TLS.
 *
 * Copyright (C) 2007-2023 Rainer Gerhards and Adiscon GmbH.
 * Copyright (C) 2023 CS Group.
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the rsyslog runtime library.	If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 * A copy of the LGPL can be found in the file "COPYING.LESSER" in this distribution.
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <mbedtls/debug.h>
#include <mbedtls/error.h>

#include "rsyslog.h"
#include "syslogd-types.h"
#include "module-template.h"
#include "obj.h"
#include "nsd_ptcp.h"
#include "nsd_mbedtls.h"
#include "rsconf.h"

MODULE_TYPE_LIB
MODULE_TYPE_KEEP

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(glbl)
DEFobjCurrIf(nsd_ptcp)

/* Mbed TLS debug level (0..5)
 * 5 is the most logs.
 */
#define MBEDTLS_DEBUG_LEVEL 0

#if MBEDTLS_DEBUG_LEVEL > 0
static void debug(void __attribute__((unused)) *ctx,
		  int __attribute__((unused)) level,
		  const char *file, int line,
		  const char *str)
{
	dbgprintf("%s:%04d: %s", file, line, str);
}
#endif

#define logMbedtlsError(err, mbedtls_err)						\
	do{										\
		char error_buf[100];							\
		int aux = (mbedtls_err < 0 ? -mbedtls_err : mbedtls_err);		\
		const char* sig = (mbedtls_err < 0 ? "-" : "");				\
		mbedtls_strerror(mbedtls_err, error_buf, 100);				\
		LogError(0, err, "Mbed TLS Error: %s0x%04X - %s", sig, aux, error_buf); \
	}while(0)

/* initialize Mbed TLS credential structure */
static rsRetVal
mbedtlsInitCred(nsd_mbedtls_t *const pThis)
{
	DEFiRet;
	const uchar *cafile;
	const uchar *crlfile;
	const uchar *keyFile;
	const uchar *certFile;

	keyFile = (pThis->pszKeyFile == NULL) ? glbl.GetDfltNetstrmDrvrKeyFile(runConf) : pThis->pszKeyFile;
	if(keyFile != NULL) {
		mbedtls_pk_free(&(pThis->pkey));
		mbedtls_pk_init(&(pThis->pkey));
		CHKiRet(mbedtls_pk_parse_keyfile(&(pThis->pkey), (const char*)keyFile, NULL,
						 mbedtls_ctr_drbg_random, &(pThis->ctr_drbg)));
		pThis->bHaveKey = 1;
	}

	certFile = (pThis->pszCertFile == NULL) ? glbl.GetDfltNetstrmDrvrCertFile(runConf) : pThis->pszCertFile;
	if(certFile != NULL) {
		mbedtls_x509_crt_free(&(pThis->srvcert));
		mbedtls_x509_crt_init(&(pThis->srvcert));
		CHKiRet(mbedtls_x509_crt_parse_file(&(pThis->srvcert), (const char*)certFile));
		pThis->bHaveCert = 1;
	}

	cafile = (pThis->pszCAFile == NULL) ? glbl.GetDfltNetstrmDrvrCAF(runConf) : pThis->pszCAFile;
	if(cafile != NULL) {
		mbedtls_x509_crt_init(&(pThis->cacert));
		mbedtls_x509_crt_free(&(pThis->cacert));
		CHKiRet(mbedtls_x509_crt_parse_file(&(pThis->cacert), (const char*)cafile));
		pThis->bHaveCaCert = 1;
	}

	crlfile = (pThis->pszCRLFile == NULL) ? glbl.GetDfltNetstrmDrvrCRLF(runConf) : pThis->pszCRLFile;
	if(crlfile != NULL) {
		mbedtls_x509_crl_init(&(pThis->crl));
		mbedtls_x509_crl_free(&(pThis->crl));
		CHKiRet(mbedtls_x509_crl_parse_file(&(pThis->crl), (const char*)crlfile));
		pThis->bHaveCrl = 1;
	}

finalize_it:
	if(iRet) {
		LogMsg(0, RS_RET_ERR, LOG_ERR, "nsd mbedtls: error parsing crypto config");
	}

	RETiRet;
}

/* globally initialize Mbed TLS */
static rsRetVal
mbedtlsGlblInit(void)
{
	DEFiRet;

	dbgprintf("mbedtlsGlblInit: Running Version: '%#010x'\n", MBEDTLS_VERSION_NUMBER);

	RETiRet;
}

static rsRetVal
get_custom_string(char** out)
{
	DEFiRet;
	struct timeval tv;
	struct tm *tm;

	CHKiRet(gettimeofday(&tv, NULL));
	if((tm = localtime(&(tv.tv_sec))) == NULL) {
		ABORT_FINALIZE(RS_RET_NO_ERRCODE);
	}
	if(asprintf(out, "nsd_mbedtls-%04d-%02d-%02d %02d:%02d:%02d:%08ld",
		    tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
		    tm->tm_hour, tm->tm_min, tm->tm_sec, tv.tv_usec) == -1) {
	    *out = NULL;
	    ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}

finalize_it:
	RETiRet;
}

static rsRetVal
mbedtlsInitSession(nsd_mbedtls_t *pThis)
{
	DEFiRet;
	char* cust = NULL;

	CHKiRet(get_custom_string(&cust));
	CHKiRet(mbedtls_ctr_drbg_seed(&(pThis->ctr_drbg),
				      mbedtls_entropy_func,
				      &(pThis->entropy),
				      (const unsigned char*)cust,
				      strlen(cust)));

finalize_it:
	if(iRet != RS_RET_OK) {
		LogError(0, iRet, "mbedtlsInitSession failed to INIT Session");
	}
	free(cust);

	RETiRet;
}

/* globally de-initialize Mbed TLS */
static rsRetVal
mbedtlsGlblExit(void)
{
	DEFiRet;
	RETiRet;
}

/* end a Mbed TLS session
 * The function checks if we have a session and ends it only if so. So it can
 * always be called, even if there currently is no session.
 */
static rsRetVal
mbedtlsEndSess(nsd_mbedtls_t *pThis)
{
	DEFiRet;
	int mbedtlsRet;

	if(pThis->bHaveSess) {
		while((mbedtlsRet = mbedtls_ssl_close_notify(&(pThis->ssl))) != 0)
		{
			if(mbedtlsRet != MBEDTLS_ERR_SSL_WANT_READ &&
			   mbedtlsRet != MBEDTLS_ERR_SSL_WANT_WRITE)
				break;
		}
		pThis->bHaveSess = 0;
	}

	RETiRet;
}

/* Standard-Constructor */
BEGINobjConstruct(nsd_mbedtls) /* be sure to specify the object type also in END macro! */
	iRet = nsd_ptcp.Construct(&pThis->pTcp);

	mbedtls_ssl_init(&(pThis->ssl));
	mbedtls_ssl_config_init(&(pThis->conf));
	mbedtls_x509_crt_init(&(pThis->cacert));
	mbedtls_x509_crl_init(&(pThis->crl));
	mbedtls_pk_init(&(pThis->pkey));
	mbedtls_x509_crt_init(&(pThis->srvcert));
	mbedtls_ctr_drbg_init(&(pThis->ctr_drbg));
	mbedtls_entropy_init(&(pThis->entropy));

#if MBEDTLS_DEBUG_LEVEL > 0
	mbedtls_debug_set_threshold(MBEDTLS_DEBUG_LEVEL);
#endif

ENDobjConstruct(nsd_mbedtls)

/* destructor for the nsd_mbedtls object */
PROTOTYPEobjDestruct(nsd_mbedtls);
BEGINobjDestruct(nsd_mbedtls) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(nsd_mbedtls)
	if(pThis->iMode == 1) {
		mbedtlsEndSess(pThis);
	}

	if(pThis->pTcp != NULL) {
		nsd_ptcp.Destruct(&pThis->pTcp);
	}

	free((void*)pThis->pszCAFile);
	free((void*)pThis->pszCRLFile);
	free((void*)pThis->pszKeyFile);
	free((void*)pThis->pszCertFile);
	free((void*)pThis->pPermPeer);

	mbedtls_entropy_free(&(pThis->entropy));
	mbedtls_ctr_drbg_free(&(pThis->ctr_drbg));
	mbedtls_x509_crt_free(&(pThis->srvcert));
	mbedtls_pk_free(&(pThis->pkey));
	mbedtls_x509_crl_free(&(pThis->crl));
	mbedtls_x509_crt_free(&(pThis->cacert));
	mbedtls_ssl_config_free(&(pThis->conf));
	mbedtls_ssl_free(&(pThis->ssl));

ENDobjDestruct(nsd_mbedtls)

/* Set the driver mode. For us, this has the following meaning:
 * 0 - work in plain tcp mode, without tls (e.g. before a STARTTLS)
 * 1 - work in TLS mode
 * rgerhards, 2008-04-28
 */
static rsRetVal
SetMode(nsd_t *const pNsd, const int mode)
{
	DEFiRet;
	nsd_mbedtls_t *pThis = (nsd_mbedtls_t*) pNsd;

	ISOBJ_TYPE_assert((pThis), nsd_mbedtls);
	dbgprintf("(tls) mode: %d\n", mode);
	if(mode != 0 && mode != 1) {
		LogError(0, RS_RET_INVALID_DRVR_MODE, "error: driver mode %d not supported by "
			 "mbedtls netstream driver", mode);
		ABORT_FINALIZE(RS_RET_INVALID_DRVR_MODE);
	}

	pThis->iMode = mode;

finalize_it:
	RETiRet;
}

/* Set the authentication mode. For us, the following is supported:
 * anon - no certificate checks whatsoever (discouraged, but supported)
 * x509/certvalid - (just) check certificate validity
 * x509/name - certificate name check
 * mode == NULL is valid and defaults to x509/name
 * rgerhards, 2008-05-16
 */
static rsRetVal
SetAuthMode(nsd_t *pNsd, uchar __attribute__((unused)) *mode)
{
	DEFiRet;
	nsd_mbedtls_t *pThis = (nsd_mbedtls_t*) pNsd;

	ISOBJ_TYPE_assert((pThis), nsd_mbedtls);

	pThis->bChkName = 0;

	if(mode == NULL || !strcasecmp((char*)mode, "x509/name")) {
		pThis->authMode = MBEDTLS_SSL_VERIFY_REQUIRED;
		pThis->bChkName = 1;
	} else if(!strcasecmp((char*) mode, "x509/certvalid")) {
		pThis->authMode = MBEDTLS_SSL_VERIFY_REQUIRED;
	} else if(!strcasecmp((char*) mode, "anon")) {
		pThis->authMode = MBEDTLS_SSL_VERIFY_NONE;
	} else {
		LogError(0, RS_RET_VALUE_NOT_SUPPORTED, "error: authentication mode '%s' not supported by "
			 "mbedtls netstream driver", mode);
		ABORT_FINALIZE(RS_RET_VALUE_NOT_SUPPORTED);
	}

	dbgprintf("SetAuthMode to %s\n", (mode != NULL ? (char*)mode : "NULL"));

finalize_it:
	RETiRet;
}

/* Set the PermitExpiredCerts mode. For us, the following is supported:
 * off - (mandatory) fail if certificate is expired
 * For now, this driver never permits expired certs
 */
static rsRetVal
SetPermitExpiredCerts(nsd_t *pNsd, uchar *mode)
{
	DEFiRet;
	nsd_mbedtls_t *pThis = (nsd_mbedtls_t*) pNsd;

	ISOBJ_TYPE_assert((pThis), nsd_mbedtls);
	/* default is set to off! */
	if(mode != NULL && strcasecmp((char*)mode, "off") != 0) {
		LogError(0, RS_RET_VALUE_NOT_SUPPORTED, "error: permitexpiredcerts mode '%s' not supported by "
			 "mbedtls netstream driver", mode);
		ABORT_FINALIZE(RS_RET_VALUE_NOT_SUPPORTED);
	}

finalize_it:
	RETiRet;
}

/* Set permitted peers. For now, this driver only supports one plain text string to be
 * compared against peer certificate CN/SAN.
 */
static rsRetVal
SetPermPeers(nsd_t *pNsd, permittedPeers_t *pPermPeers)
{
	DEFiRet;
	nsd_mbedtls_t *pThis = (nsd_mbedtls_t*) pNsd;

	ISOBJ_TYPE_assert((pThis), nsd_mbedtls);

	if(pPermPeers != NULL) {
		if(pThis->authMode != MBEDTLS_SSL_VERIFY_REQUIRED || !(pThis->bChkName)) {
			LogError(0, RS_RET_VALUE_NOT_IN_THIS_MODE, "SetPermPeers not supported by "
				 "mbedtls netstream driver in the configured authentication mode");
			ABORT_FINALIZE(RS_RET_VALUE_NOT_IN_THIS_MODE);
		}

		if(pPermPeers->etryType != PERM_PEER_TYPE_UNDECIDED &&
		   pPermPeers->etryType != PERM_PEER_TYPE_PLAIN) {
			LogError(0, RS_RET_VALUE_NOT_IN_THIS_MODE, "SetPermPeers: only plain text "
				 "peer authentication supported by mbedtls netstream driver");
			ABORT_FINALIZE(RS_RET_VALUE_NOT_SUPPORTED);
		}
		if(pPermPeers->pNext != NULL) {
			LogMsg(0, RS_RET_VALUE_NOT_SUPPORTED, LOG_WARNING, "SetPermPeers warning: only one peer "
			       "supported by mbedtls netstream driver");
		}
		free((void*)pThis->pPermPeer);
		CHKmalloc(pThis->pPermPeer = (const uchar*)strdup((const char*)(pPermPeers->pszID)));
	} else {
		free((void*)pThis->pPermPeer);
		pThis->pPermPeer = NULL;
	}

finalize_it:
	RETiRet;
}

/* gnutls priority string
 * PascalWithopf 2017-08-16
 */
static rsRetVal
SetGnutlsPriorityString(nsd_t *pNsd, uchar *gnutlsPriorityString)
{
	DEFiRet;
	nsd_mbedtls_t *pThis = (nsd_mbedtls_t*) pNsd;

	ISOBJ_TYPE_assert((pThis), nsd_mbedtls);

	if(gnutlsPriorityString == NULL)
		FINALIZE;

	LogError(0, RS_RET_VALUE_NOT_SUPPORTED, "error: SetGnutlsPriorityString: %s "
		 "not supported by mbedtls netstream driver", gnutlsPriorityString);
	ABORT_FINALIZE(RS_RET_VALUE_NOT_SUPPORTED);

finalize_it:
	RETiRet;
}

/* Set the driver cert extended key usage check setting
 * 1 - (mandatory) verify that cert contents is compatible with appropriate OID
 * For now, this driver always checks cert extended key usage
 * jvymazal, 2019-08-16
 */
static rsRetVal
SetCheckExtendedKeyUsage(nsd_t *pNsd, int ChkExtendedKeyUsage)
{
	DEFiRet;
	nsd_mbedtls_t *pThis = (nsd_mbedtls_t*) pNsd;

	ISOBJ_TYPE_assert((pThis), nsd_mbedtls);
	if(ChkExtendedKeyUsage != 1) {
		LogMsg(0, RS_RET_VALUE_NOT_SUPPORTED, LOG_WARNING, "warning: driver ChkExtendedKeyUsage %d "
		       "ignored as not supported by mbedtls netstream driver", ChkExtendedKeyUsage);
	}

	RETiRet;
}

/* Set the driver name checking strictness
 * 1 - (mandatory) more strict per RFC 6125 - if any SAN present it must match (CN is ignored)
 * For now, this driver always prioritize SAN if present.
 * jvymazal, 2019-08-16
 */
static rsRetVal
SetPrioritizeSAN(nsd_t *pNsd, int prioritizeSan)
{
	DEFiRet;
	nsd_mbedtls_t *pThis = (nsd_mbedtls_t*) pNsd;

	ISOBJ_TYPE_assert((pThis), nsd_mbedtls);
	if(prioritizeSan != 1) {
		LogMsg(0, RS_RET_VALUE_NOT_SUPPORTED, LOG_WARNING, "warning: driver PrioritizeSAN %d "
		       "ignored as not supported by mbedtls netstream driver", prioritizeSan);
	}

	RETiRet;
}

/* Set the driver tls  verifyDepth
 * alorbach, 2019-12-20
 */
static rsRetVal
SetTlsVerifyDepth(nsd_t *pNsd, int verifyDepth)
{
	DEFiRet;
	nsd_mbedtls_t *pThis = (nsd_mbedtls_t*) pNsd;

	ISOBJ_TYPE_assert((pThis), nsd_mbedtls);
	pThis->DrvrVerifyDepth = verifyDepth;

	RETiRet;
}

static rsRetVal
SetTlsCAFile(nsd_t *pNsd, const uchar *const caFile)
{
	DEFiRet;
	nsd_mbedtls_t *pThis = (nsd_mbedtls_t*) pNsd;

	ISOBJ_TYPE_assert((pThis), nsd_mbedtls);

	free((void*)pThis->pszCAFile);

	if(caFile == NULL) {
		pThis->pszCAFile = NULL;
	} else {
		CHKmalloc(pThis->pszCAFile = (const uchar*) strdup((const char*) caFile));
	}

finalize_it:
	RETiRet;
}

static rsRetVal
SetTlsCRLFile(nsd_t *pNsd, const uchar *const crlFile)
{
	DEFiRet;
	nsd_mbedtls_t *pThis = (nsd_mbedtls_t*) pNsd;

	ISOBJ_TYPE_assert((pThis), nsd_mbedtls);

	free((void*)pThis->pszCRLFile);

	if(crlFile == NULL) {
		pThis->pszCRLFile = NULL;
	} else {
		CHKmalloc(pThis->pszCRLFile = (const uchar*) strdup((const char*) crlFile));
	}

finalize_it:
	RETiRet;
}

static rsRetVal
SetTlsKeyFile(nsd_t *pNsd, const uchar *const pszFile)
{
	DEFiRet;
	nsd_mbedtls_t *pThis = (nsd_mbedtls_t*) pNsd;

	ISOBJ_TYPE_assert((pThis), nsd_mbedtls);

	free((void*)pThis->pszKeyFile);

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
	nsd_mbedtls_t *pThis = (nsd_mbedtls_t*) pNsd;

	ISOBJ_TYPE_assert((pThis), nsd_mbedtls);

	free((void*)pThis->pszCertFile);

	if(pszFile == NULL) {
		pThis->pszCertFile = NULL;
	} else {
		CHKmalloc(pThis->pszCertFile = (const uchar*) strdup((const char*) pszFile));
	}

finalize_it:
	RETiRet;
}

/* Provide access to the underlying OS socket. This is primarily
 * useful for other drivers (like nsd_mbedtls) who utilize ourselfs
 * for some of their functionality. -- rgerhards, 2008-04-18
 */
static rsRetVal
SetSock(nsd_t *pNsd, int sock)
{
	DEFiRet;
	nsd_mbedtls_t *pThis = (nsd_mbedtls_t*) pNsd;

	ISOBJ_TYPE_assert((pThis), nsd_mbedtls);

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
	nsd_mbedtls_t *pThis = (nsd_mbedtls_t*) pNsd;

	ISOBJ_TYPE_assert((pThis), nsd_mbedtls);
	assert(keepAliveIntvl >= 0);

	nsd_ptcp.SetKeepAliveIntvl(pThis->pTcp, keepAliveIntvl);

	RETiRet;
}

/* Keep Alive Options
 */
static rsRetVal
SetKeepAliveProbes(nsd_t *pNsd, int keepAliveProbes)
{
	DEFiRet;
	nsd_mbedtls_t *pThis = (nsd_mbedtls_t*) pNsd;

	ISOBJ_TYPE_assert((pThis), nsd_mbedtls);
	assert(keepAliveProbes >= 0);

	nsd_ptcp.SetKeepAliveProbes(pThis->pTcp, keepAliveProbes);

	RETiRet;
}

/* Keep Alive Options
 */
static rsRetVal
SetKeepAliveTime(nsd_t *pNsd, int keepAliveTime)
{
	DEFiRet;
	nsd_mbedtls_t *pThis = (nsd_mbedtls_t*) pNsd;

	ISOBJ_TYPE_assert((pThis), nsd_mbedtls);
	assert(keepAliveTime >= 0);

	nsd_ptcp.SetKeepAliveTime(pThis->pTcp, keepAliveTime);

	RETiRet;
}

/* abort a connection. This is meant to be called immediately
 * before the Destruct call. -- rgerhards, 2008-03-24
 */
static rsRetVal
Abort(nsd_t *pNsd)
{
	nsd_mbedtls_t *pThis = (nsd_mbedtls_t*) pNsd;
	DEFiRet;

	ISOBJ_TYPE_assert((pThis), nsd_mbedtls);

	if(pThis->iMode == 0) {
		nsd_ptcp.Abort(pThis->pTcp);
	}

	RETiRet;
}

/* Allow Mbed TLS to read data from the network.
 */
static int mbedtlsNetRecv(void *ctx, unsigned char *buf, size_t len)
{
	nsd_mbedtls_t *pThis = (nsd_mbedtls_t*)ctx;
	ssize_t slen = len;
	int oserr;
	DEFiRet;

	slen = recv(pThis->sock, buf, len, 0);
	oserr = errno;

	if(slen < 0) {
		switch(oserr) {
		case EAGAIN:
#if EAGAIN != EWOULDBLOCK
		case EWOULDBLOCK:
#endif
		case EINTR:
			ABORT_FINALIZE(MBEDTLS_ERR_SSL_WANT_READ);
		case EPIPE:
		case ECONNRESET:
			ABORT_FINALIZE(MBEDTLS_ERR_NET_CONN_RESET);
		default:
			ABORT_FINALIZE(MBEDTLS_ERR_NET_RECV_FAILED);
		}
	}
	iRet = slen;

finalize_it:
	RETiRet;
}

/* Allow Mbed TLS to write data to the network.
 */
static int mbedtlsNetSend(void *ctx, const unsigned char *buf, size_t len)
{
	nsd_mbedtls_t *pThis = (nsd_mbedtls_t*)ctx;
	ssize_t slen = len;
	DEFiRet;

	iRet = nsd_ptcp.Send(pThis->pTcp, (unsigned char *)buf, &slen);

	if(iRet == RS_RET_IO_ERROR)
		ABORT_FINALIZE(MBEDTLS_ERR_NET_SEND_FAILED);

	iRet = slen;

finalize_it:
	RETiRet;
}

static int verify(void *ctx, mbedtls_x509_crt __attribute__((unused)) *crt,
		  int depth, uint32_t __attribute__((unused)) *flags)
{
	nsd_mbedtls_t *pThis = (nsd_mbedtls_t*)ctx;

	if(pThis->DrvrVerifyDepth && depth > pThis->DrvrVerifyDepth)
		return MBEDTLS_ERR_X509_FATAL_ERROR;

	return 0;
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
	nsd_mbedtls_t *pNew = NULL;
	nsd_mbedtls_t *pThis = (nsd_mbedtls_t*)pNsd;
	int mbedtlsRet;

	ISOBJ_TYPE_assert(pThis, nsd_mbedtls);
	CHKiRet(nsd_mbedtlsConstruct(&pNew)); // TODO: prevent construct/destruct!
	CHKiRet(nsd_ptcp.Destruct(&pNew->pTcp));
	CHKiRet(nsd_ptcp.AcceptConnReq(pThis->pTcp, &pNew->pTcp));

	if(pThis->iMode == 0) {
		/* we are in non-TLS mode, so we are done */
		*ppNew = (nsd_t*)pNew;
		FINALIZE;
	}
	/* copy Properties to pnew first */
	pNew->authMode = pThis->authMode;
	pNew->bChkName = pThis->bChkName;
	if(pThis->pPermPeer)
		CHKmalloc(pNew->pPermPeer = (const uchar*)strdup((const char*)(pThis->pPermPeer)));
	pNew->DrvrVerifyDepth = pThis->DrvrVerifyDepth;
	pNew->pszCertFile = pThis->pszCertFile;
	pNew->pszKeyFile = pThis->pszKeyFile;
	pNew->pszCAFile = pThis->pszCAFile;
	pNew->pszCRLFile = pThis->pszCRLFile;
	pNew->iMode = pThis->iMode;

	/* if we reach this point, we are in TLS mode */
	CHKiRet(mbedtlsInitSession(pNew));
	CHKiRet(mbedtlsInitCred(pNew));
	CHKiRet(mbedtls_ssl_config_defaults(&(pNew->conf),
					    MBEDTLS_SSL_IS_SERVER,
					    MBEDTLS_SSL_TRANSPORT_STREAM,
					    MBEDTLS_SSL_PRESET_DEFAULT));

	mbedtls_ssl_conf_rng(&(pNew->conf), mbedtls_ctr_drbg_random, &(pNew->ctr_drbg));
	mbedtls_ssl_conf_authmode(&(pNew->conf), pNew->authMode);
	mbedtls_ssl_conf_ca_chain(&(pNew->conf),
				  pNew->bHaveCaCert ? &(pNew->cacert) : NULL,
				  pNew->bHaveCrl ? &(pNew->crl) : NULL);

	mbedtls_ssl_conf_verify(&(pNew->conf), verify, pNew);

	if(pNew->bHaveKey && pNew->bHaveCert)
		CHKiRet(mbedtls_ssl_conf_own_cert(&(pNew->conf), &(pNew->srvcert), &(pNew->pkey)));

#if MBEDTLS_DEBUG_LEVEL > 0
	mbedtls_ssl_conf_dbg(&(pNew->conf), debug, stdout);
#endif

	CHKiRet(mbedtls_ssl_setup(&(pNew->ssl), &(pNew->conf)));

	if(pNew->bChkName && pNew->pPermPeer) {
		dbgprintf("mbedtls_ssl_set_hostname: '%s'\n", pNew->pPermPeer);
		CHKiRet(mbedtls_ssl_set_hostname(&(pNew->ssl),(const char*)(pNew->pPermPeer)));
	}

	CHKiRet(nsd_ptcp.GetSock(pNew->pTcp, &(pNew->sock)));
	mbedtls_ssl_set_bio(&(pNew->ssl), pNew, mbedtlsNetSend, mbedtlsNetRecv, NULL);

	mbedtlsRet = mbedtls_ssl_handshake(&(pNew->ssl));
	if(mbedtlsRet != 0 && mbedtlsRet != MBEDTLS_ERR_SSL_WANT_READ &&
	   mbedtlsRet != MBEDTLS_ERR_SSL_WANT_WRITE) {
		logMbedtlsError(RS_RET_TLS_HANDSHAKE_ERR, mbedtlsRet);
		ABORT_FINALIZE(RS_RET_TLS_HANDSHAKE_ERR);
	}

	pNew->bHaveSess = 1;
	*ppNew = (nsd_t*)pNew;

finalize_it:
	if(iRet != RS_RET_OK) {
		if(pNew != NULL)
			nsd_mbedtlsDestruct(&pNew);
	}
	RETiRet;
}

/* receive data from a tcp socket
 * The lenBuf parameter must contain the max buffer size on entry and contains
 * the number of octets read on exit. This function
 * never blocks, not even when called on a blocking socket. That is important
 * for client sockets, which are set to block during send, but should not
 * block when trying to read data. -- rgerhards, 2008-03-17
 */
static rsRetVal
Rcv(nsd_t *pNsd, uchar *pBuf, ssize_t *pLenBuf, int *const oserr)
{
	DEFiRet;
	nsd_mbedtls_t *pThis = (nsd_mbedtls_t*)pNsd;
	ISOBJ_TYPE_assert(pThis, nsd_mbedtls);
	int n = 0;

	if(pThis->bAbortConn)
		ABORT_FINALIZE(RS_RET_CONNECTION_ABORTREQ);

	if(pThis->iMode == 0) {
		CHKiRet(nsd_ptcp.Rcv(pThis->pTcp, pBuf, pLenBuf, oserr));
		FINALIZE;
	}

	/* --- in TLS mode now --- */
	n = mbedtls_ssl_read(&(pThis->ssl), pBuf, *pLenBuf);

	if(n == MBEDTLS_ERR_SSL_WANT_READ || n == MBEDTLS_ERR_SSL_WANT_WRITE)
		ABORT_FINALIZE(RS_RET_RETRY);

	if(n == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
		ABORT_FINALIZE(RS_RET_CLOSED);

	if(n < 0) {
		logMbedtlsError(RS_RET_RCV_ERR, n);
		ABORT_FINALIZE(RS_RET_RCV_ERR);
	}

	if(n == 0)
		ABORT_FINALIZE(RS_RET_EOF);

	*pLenBuf = n;

finalize_it:
	dbgprintf("mbedtlsRcv return. nsd %p, iRet %d, lenRcvBuf %ld\n", pThis,
		  iRet, (long)(*pLenBuf));

	if (iRet != RS_RET_OK && iRet != RS_RET_RETRY)
		*pLenBuf = 0;

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
	int iSent;
	nsd_mbedtls_t *pThis = (nsd_mbedtls_t*)pNsd;
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, nsd_mbedtls);

	if(pThis->bAbortConn)
		ABORT_FINALIZE(RS_RET_CONNECTION_ABORTREQ);

	if(pThis->iMode == 0) {
		CHKiRet(nsd_ptcp.Send(pThis->pTcp, pBuf, pLenBuf));
		FINALIZE;
	}

	/* in TLS mode now */
	while((iSent = mbedtls_ssl_write(&(pThis->ssl), pBuf, *pLenBuf)) <= 0) {
		if(iSent != MBEDTLS_ERR_SSL_WANT_READ && iSent != MBEDTLS_ERR_SSL_WANT_WRITE) {
			logMbedtlsError(RS_RET_NO_ERRCODE, iSent);
			ABORT_FINALIZE(iSent);
		}
	}
	*pLenBuf = iSent;

finalize_it:
	RETiRet;
}

/* Enable KEEPALIVE handling on the socket.
 * rgerhards, 2009-06-02
 */
static rsRetVal
EnableKeepAlive(nsd_t *pNsd)
{
	nsd_mbedtls_t *pThis = (nsd_mbedtls_t*) pNsd;
	ISOBJ_TYPE_assert(pThis, nsd_mbedtls);
	return nsd_ptcp.EnableKeepAlive(pThis->pTcp);
}

/* open a connection to a remote host (server).
 */
static rsRetVal
Connect(nsd_t *pNsd, int family, uchar *port, uchar *host, char *device)
{
	DEFiRet;
	nsd_mbedtls_t *pThis = (nsd_mbedtls_t*) pNsd;
	int mbedtlsRet;

	dbgprintf("Connect to %s:%s\n", host, port);

	ISOBJ_TYPE_assert(pThis, nsd_mbedtls);
	assert(port != NULL);
	assert(host != NULL);

	CHKiRet(mbedtlsInitSession(pThis));
	CHKiRet(mbedtlsInitCred(pThis));
	CHKiRet(nsd_ptcp.Connect(pThis->pTcp, family, port, host, device));

	if(pThis->iMode == 0)
		FINALIZE;

	/* we reach this point if in TLS mode */
	CHKiRet(mbedtls_ssl_config_defaults(&(pThis->conf),
					    MBEDTLS_SSL_IS_CLIENT,
					    MBEDTLS_SSL_TRANSPORT_STREAM,
					    MBEDTLS_SSL_PRESET_DEFAULT));

	mbedtls_ssl_conf_rng(&(pThis->conf), mbedtls_ctr_drbg_random, &(pThis->ctr_drbg));
	mbedtls_ssl_conf_authmode(&(pThis->conf), pThis->authMode);
	mbedtls_ssl_conf_ca_chain(&(pThis->conf),
				  pThis->bHaveCaCert ? &(pThis->cacert) : NULL,
				  pThis->bHaveCrl ? &(pThis->crl) : NULL);

	mbedtls_ssl_conf_verify(&(pThis->conf), verify, pThis);

	if(pThis->bHaveKey && pThis->bHaveCert)
		CHKiRet(mbedtls_ssl_conf_own_cert(&(pThis->conf), &(pThis->srvcert), &(pThis->pkey)));

#if MBEDTLS_DEBUG_LEVEL > 0
	mbedtls_ssl_conf_dbg(&(pThis->conf), debug, stdout);
#endif
	CHKiRet(mbedtls_ssl_setup(&(pThis->ssl), &(pThis->conf)));

	if(pThis->bChkName) {
		CHKiRet(mbedtls_ssl_set_hostname(&(pThis->ssl),
						 (const char*)(pThis->pPermPeer ? pThis->pPermPeer : host)));
	}

	CHKiRet(nsd_ptcp.GetSock(pThis->pTcp, &(pThis->sock)));
	mbedtls_ssl_set_bio(&(pThis->ssl), pThis, mbedtlsNetSend, mbedtlsNetRecv, NULL);

	mbedtlsRet = mbedtls_ssl_handshake(&(pThis->ssl));
	if(mbedtlsRet != 0 && mbedtlsRet != MBEDTLS_ERR_SSL_WANT_READ &&
	   mbedtlsRet != MBEDTLS_ERR_SSL_WANT_WRITE) {
		logMbedtlsError(RS_RET_TLS_HANDSHAKE_ERR, mbedtlsRet);
		ABORT_FINALIZE(RS_RET_TLS_HANDSHAKE_ERR);
	}
	pThis->bHaveSess = 1;

finalize_it:

	RETiRet;
}

static rsRetVal ATTR_NONNULL(1,3,5)
LstnInit(netstrms_t *pNS,
	 void *pUsr,
	 rsRetVal (*fAddLstn)(void*,netstrm_t*),
	 const int iSessMax,
	 const tcpLstnParams_t *cnf_params)
{
	DEFiRet;

	iRet = nsd_ptcp.LstnInit(pNS, pUsr, fAddLstn, iSessMax, cnf_params);

	RETiRet;
}

/* This function checks if the connection is still alive - well, kind of...
 * This is a dummy here. For details, check function common in ptcp driver.
 * rgerhards, 2008-06-09
 */
static rsRetVal
CheckConnection(nsd_t *pNsd)
{
	nsd_mbedtls_t *pThis = (nsd_mbedtls_t*)pNsd;
	ISOBJ_TYPE_assert(pThis, nsd_mbedtls);

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
	nsd_mbedtls_t *pThis = (nsd_mbedtls_t*)pNsd;
	ISOBJ_TYPE_assert(pThis, nsd_mbedtls);
	iRet = nsd_ptcp.GetRemoteHName(pThis->pTcp, ppszHName);
	RETiRet;
}


/* Provide access to the sockaddr_storage of the remote peer. This
 * is needed by the legacy ACL system. --- gerhards, 2008-12-01
 */
static rsRetVal
GetRemAddr(nsd_t *pNsd, struct sockaddr_storage **ppAddr)
{
	DEFiRet;
	nsd_mbedtls_t *pThis = (nsd_mbedtls_t*)pNsd;
	ISOBJ_TYPE_assert(pThis, nsd_mbedtls);
	iRet = nsd_ptcp.GetRemAddr(pThis->pTcp, ppAddr);
	RETiRet;
}

/* get the remote host's IP address. Caller must Destruct the object. */
static rsRetVal
GetRemoteIP(nsd_t *pNsd, prop_t **ip)
{
	DEFiRet;
	nsd_mbedtls_t *pThis = (nsd_mbedtls_t*)pNsd;
	ISOBJ_TYPE_assert(pThis, nsd_mbedtls);
	iRet = nsd_ptcp.GetRemoteIP(pThis->pTcp, ip);
	RETiRet;
}

/* queryInterface function */
BEGINobjQueryInterface(nsd_mbedtls)
CODESTARTobjQueryInterface(nsd_mbedtls)
	if(pIf->ifVersion != nsdCURR_IF_VERSION) {/* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->Construct = (rsRetVal(*)(nsd_t**)) nsd_mbedtlsConstruct;
	pIf->Destruct = (rsRetVal(*)(nsd_t**)) nsd_mbedtlsDestruct;
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
	pIf->SetGnutlsPriorityString = SetGnutlsPriorityString;
	pIf->SetCheckExtendedKeyUsage = SetCheckExtendedKeyUsage;
	pIf->SetPrioritizeSAN = SetPrioritizeSAN;
	pIf->SetTlsVerifyDepth = SetTlsVerifyDepth;
	pIf->SetTlsCAFile = SetTlsCAFile;
	pIf->SetTlsCRLFile = SetTlsCRLFile;
	pIf->SetTlsKeyFile = SetTlsKeyFile;
	pIf->SetTlsCertFile = SetTlsCertFile;
finalize_it:
ENDobjQueryInterface(nsd_mbedtls)

/* exit our class
 */
BEGINObjClassExit(nsd_mbedtls, OBJ_IS_LOADABLE_MODULE) /* CHANGE class also in END MACRO! */
CODESTARTObjClassExit(nsd_mbedtls)
	mbedtlsGlblExit();		/* shut down Mbed TLS */

	/* release objects we no longer need */
	objRelease(nsd_ptcp, LM_NSD_PTCP_FILENAME);
	objRelease(glbl, CORE_COMPONENT);
ENDObjClassExit(nsd_mbedtls)

/* Initialize the nsd_mbedtls class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINObjClassInit(nsd_mbedtls, 1, OBJ_IS_LOADABLE_MODULE) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(nsd_ptcp, LM_NSD_PTCP_FILENAME));

	/* now do global TLS init stuff */
	CHKiRet(mbedtlsGlblInit());
ENDObjClassInit(nsd_mbedtls)

/* --------------- here now comes the plumbing that makes as a library module --------------- */

BEGINmodExit
CODESTARTmodExit
	nsd_mbedtlsClassExit();
ENDmodExit

BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_LIB_QUERIES
ENDqueryEtryPt

BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */

	/* Initialize all classes that are in our module - this includes ourselfs */
	CHKiRet(nsd_mbedtlsClassInit(pModInfo)); /* must be done after tcps_sess, as we use it */
ENDmodInit
