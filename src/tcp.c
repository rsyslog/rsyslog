/* This implements the relp mapping onto TCP.
 *
 * Copyright 2008-2014 by Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of librelp.
 *
 * Note: gnutls_certificate_set_verify_function is problematic, as it
 *       is not available in old GnuTLS versions, but rather important
 *       for verifying certificates correctly.
 *
 * Librelp is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Librelp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Librelp.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 *
 * If the terms of the GPL are unsuitable for your needs, you may obtain
 * a commercial license from Adiscon. Contact sales@adiscon.com for further
 * details.
 *
 * ALL CONTRIBUTORS PLEASE NOTE that by sending contributions, you assign
 * your copyright to Adiscon GmbH, Germany. This is necessary to permit the
 * dual-licensing set forth here. Our apologies for this inconvenience, but
 * we sincerely believe that the dual-licensing model helps us provide great
 * free software while at the same time obtaining some funding for further
 * development.
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include "relp.h"
#include "relpsrv.h"
#include "relpclt.h"
#include "relpsess.h"
#include "tcp.h"
#ifdef ENABLE_TLS
#	include <gnutls/gnutls.h>
#	include <gnutls/x509.h>
#	if GNUTLS_VERSION_NUMBER <= 0x020b00
#		include <gcrypt.h>
		GCRY_THREAD_OPTION_PTHREAD_IMPL;
#	endif
	static int called_gnutls_global_init = 0;
#endif



#ifdef ENABLE_TLS
/* forward definitions */
#ifdef HAVE_GNUTLS_CERTIFICATE_SET_VERIFY_FUNCTION
static int relpTcpVerifyCertificateCallback(gnutls_session_t session);
#endif /* #ifdef HAVE_GNUTLS_CERTIFICATE_SET_VERIFY_FUNCTION */
static relpRetVal relpTcpPermittedPeerWildcardCompile(tcpPermittedPeerEntry_t *pEtry);

/* helper to free permittedPeer structure */
static inline void
relpTcpFreePermittedPeers(relpTcp_t *pThis)
{
	int i;
	for(i = 0 ; i < pThis->permittedPeers.nmemb ; ++i)
		free(pThis->permittedPeers.peer[i].name);
	pThis->permittedPeers.nmemb = 0;
}
#endif /* #ifdef ENABLE_TLS */

/** Construct a RELP tcp instance
 * This is the first thing that a caller must do before calling any
 * RELP function. The relp tcp must only destructed after all RELP
 * operations have been finished. Parameter pParent contains a pointer
 * to the "parent" client or server object, depending on connType.
 */
relpRetVal
relpTcpConstruct(relpTcp_t **ppThis, relpEngine_t *pEngine, int connType, void *pParent)
{
	relpTcp_t *pThis;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	if((pThis = calloc(1, sizeof(relpTcp_t))) == NULL) {
		ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}

	RELP_CORE_CONSTRUCTOR(pThis, Tcp);
	if(connType == RELP_SRV_CONN) {
		pThis->pSrv = (relpSrv_t*) pParent;
	} else {
		pThis->pClt = (relpClt_t*) pParent;
	}
	pThis->sock = -1;
	pThis->pEngine = pEngine;
	pThis->iSessMax = 500;	/* default max nbr of sessions - TODO: make configurable -- rgerhards, 2008-03-17*/
	pThis->bTLSActive = 0;
	pThis->dhBits = DEFAULT_DH_BITS;
	pThis->pristring = NULL;
	pThis->authmode = eRelpAuthMode_None;
	pThis->caCertFile = NULL;
	pThis->ownCertFile = NULL;
	pThis->privKeyFile = NULL;
	pThis->pUsr = NULL;
	pThis->permittedPeers.nmemb = 0;

	*ppThis = pThis;

finalize_it:
	LEAVE_RELPFUNC;
}


/** Destruct a RELP tcp instance
 */
relpRetVal
relpTcpDestruct(relpTcp_t **ppThis)
{
	relpTcp_t *pThis;
	int i;
#ifdef ENABLE_TLS
	int gnuRet;
#endif /* #ifdef ENABLE_TLS */

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	pThis = *ppThis;
	RELPOBJ_assert(pThis, Tcp);

	if(pThis->sock != -1) {
		close(pThis->sock);
		pThis->sock = -1;
	}

	if(pThis->socks != NULL) {
		/* if we have some sockets at this stage, we need to close them */
		for(i = 1 ; i <= pThis->socks[0] ; ++i)
			close(pThis->socks[i]);
		free(pThis->socks);
	}

#ifdef ENABLE_TLS
	if(pThis->bTLSActive) {
		gnuRet = gnutls_bye(pThis->session, GNUTLS_SHUT_RDWR);
		while(gnuRet == GNUTLS_E_INTERRUPTED || gnuRet == GNUTLS_E_AGAIN) {
			gnuRet = gnutls_bye(pThis->session, GNUTLS_SHUT_RDWR);
		}
		gnutls_deinit(pThis->session);
	}
	relpTcpFreePermittedPeers(pThis);
#endif /* #ifdef ENABLE_TLS */

	free(pThis->pRemHostIP);
	free(pThis->pRemHostName);
	free(pThis->pristring);
	free(pThis->caCertFile);
	free(pThis->ownCertFile);
	free(pThis->privKeyFile);

	/* done with de-init work, now free tcp object itself */
	free(pThis);
	*ppThis = NULL;

	LEAVE_RELPFUNC;
}


/* helper to call onErr if set */
static void
callOnErr(const relpTcp_t *__restrict__ const pThis,
	char *__restrict__ const emsg,
	const relpRetVal ecode)
{
	char objinfo[1024];
	pThis->pEngine->dbgprint("librelp: generic error: ecode %d, "
		"emsg '%s'\n", ecode, emsg);
	if(pThis->pEngine->onErr != NULL) {
		if(pThis->pSrv == NULL) { /* client */
			snprintf(objinfo, sizeof(objinfo), "conn to srvr %s:%s",
				 pThis->pClt->pSess->srvAddr,
				 pThis->pClt->pSess->srvPort);
		} else if(pThis->pRemHostIP == NULL) { /* server listener */
			snprintf(objinfo, sizeof(objinfo), "lstn %s",
				 pThis->pSrv->pLstnPort);
		} else { /* server connection to client */
			snprintf(objinfo, sizeof(objinfo), "lstn %s: conn to clt %s/%s",
				 pThis->pSrv->pLstnPort, pThis->pRemHostIP,
				 pThis->pRemHostName);
		}
		objinfo[sizeof(objinfo)-1] = '\0';
		pThis->pEngine->onErr(pThis->pUsr, objinfo, emsg, ecode);
	}
}


#ifdef ENABLE_TLS
/* helper to call an error code handler if gnutls failed. If there is a failure, 
 * an error message is pulled form gnutls and the error message properly 
 * populated.
 * Returns 1 if an error was detected, 0 otherwise. This can be used as a
 * shortcut for error handling (safes doing it twice).
 */
static int
chkGnutlsCode(relpTcp_t *pThis, char *emsg, relpRetVal ecode, int gnuRet)
{
	char msgbuf[4096];
	int r;

	if(gnuRet == GNUTLS_E_SUCCESS) {
		r = 0;
	} else {
		r = 1;
		snprintf(msgbuf, sizeof(msgbuf), "%s [gnutls error %d: %s]",
			 emsg, gnuRet, gnutls_strerror(gnuRet));
		msgbuf[sizeof(msgbuf)-1] = '\0';
		callOnErr(pThis, msgbuf, ecode);
	}
	return r;
}

/* helper to call onAuthErr if set */
static inline void
callOnAuthErr(relpTcp_t *pThis, char *authdata, char *emsg, relpRetVal ecode)
{
	pThis->pEngine->dbgprint("librelp: auth error: authdata:'%s', ecode %d, "
		"emsg '%s'\n", authdata, ecode, emsg);
	if(pThis->pEngine->onAuthErr != NULL) {
		pThis->pEngine->onAuthErr(pThis->pUsr, authdata, emsg, ecode);
	}
}
#endif /* #ifdef ENABLE_TLS */

/* abort a tcp connection. This is much like relpTcpDestruct(), but tries
 * to discard any unsent data. -- rgerhards, 2008-03-24
 */
relpRetVal
relpTcpAbortDestruct(relpTcp_t **ppThis)
{
	struct linger ling;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	RELPOBJ_assert((*ppThis), Tcp);

	if((*ppThis)->sock != -1) {
		ling.l_onoff = 1;
		ling.l_linger = 0;
       		if(setsockopt((*ppThis)->sock, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling)) < 0 ) {
			(*ppThis)->pEngine->dbgprint("could not set SO_LINGER, errno %d\n", errno);
		}
	}

	iRet = relpTcpDestruct(ppThis);

	LEAVE_RELPFUNC;
}


#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
#	define SALEN(sa) ((sa)->sa_len)
#else
static inline size_t SALEN(struct sockaddr *sa) {
	switch (sa->sa_family) {
	case AF_INET:  return (sizeof(struct sockaddr_in));
	case AF_INET6: return (sizeof(struct sockaddr_in6));
	default:       return 0;
	}
}
#endif


/* we may later change the criteria, thus we encapsulate it
 * into a function.
 */
static inline int8_t
isAnonAuth(relpTcp_t *pThis)
{
	return pThis->ownCertFile == NULL;
}

/* Set pRemHost based on the address provided. This is to be called upon accept()ing
 * a connection request. It must be provided by the socket we received the
 * message on as well as a NI_MAXHOST size large character buffer for the FQDN.
 * Please see http://www.hmug.org/man/3/getnameinfo.php (under Caveats)
 * for some explanation of the code found below. If we detect a malicious
 * hostname, we return RELP_RET_MALICIOUS_HNAME and let the caller decide
 * on how to deal with that.
 * rgerhards, 2008-03-31
 */
static relpRetVal
relpTcpSetRemHost(relpTcp_t *pThis, struct sockaddr *pAddr)
{
	relpEngine_t *pEngine;
	int error;
	unsigned char szIP[NI_MAXHOST] = "";
	unsigned char szHname[NI_MAXHOST] = "";
	struct addrinfo hints, *res;
	size_t len;
	
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Tcp);
	pEngine = pThis->pEngine;
	assert(pAddr != NULL);

        error = getnameinfo(pAddr, SALEN(pAddr), (char*)szIP, sizeof(szIP), NULL, 0, NI_NUMERICHOST);

        if(error) {
                pThis->pEngine->dbgprint("Malformed from address %s\n", gai_strerror(error));
		strcpy((char*)szHname, "???");
		strcpy((char*)szIP, "???");
		ABORT_FINALIZE(RELP_RET_INVALID_HNAME);
	}

	if(pEngine->bEnableDns) {
		error = getnameinfo(pAddr, SALEN(pAddr), (char*)szHname, NI_MAXHOST, NULL, 0, NI_NAMEREQD);
		if(error == 0) {
			memset (&hints, 0, sizeof (struct addrinfo));
			hints.ai_flags = AI_NUMERICHOST;
			hints.ai_socktype = SOCK_STREAM;
			/* we now do a lookup once again. This one should fail,
			 * because we should not have obtained a non-numeric address. If
			 * we got a numeric one, someone messed with DNS!
			 */
			if(getaddrinfo((char*)szHname, NULL, &hints, &res) == 0) {
				freeaddrinfo (res);
				/* OK, we know we have evil, so let's indicate this to our caller */
				snprintf((char*)szHname, NI_MAXHOST, "[MALICIOUS:IP=%s]", szIP);
				pEngine->dbgprint("Malicious PTR record, IP = \"%s\" HOST = \"%s\"", szIP, szHname);
				iRet = RELP_RET_MALICIOUS_HNAME;
			}
		} else {
			strcpy((char*)szHname, (char*)szIP);
		}
	} else {
		strcpy((char*)szHname, (char*)szIP);
	}

	/* We now have the names, so now let's allocate memory and store them permanently.
	 * (side note: we may hold on to these values for quite a while, thus we trim their
	 * memory consumption)
	 */
	len = strlen((char*)szIP) + 1; /* +1 for \0 byte */
	if((pThis->pRemHostIP = malloc(len)) == NULL)
		ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	memcpy(pThis->pRemHostIP, szIP, len);

	len = strlen((char*)szHname) + 1; /* +1 for \0 byte */
	if((pThis->pRemHostName = malloc(len)) == NULL) {
		free(pThis->pRemHostIP); /* prevent leak */
		pThis->pRemHostIP = NULL;
		ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}
	memcpy(pThis->pRemHostName, szHname, len);

finalize_it:
	LEAVE_RELPFUNC;
}

/* this copies a *complete* permitted peers structure into the
 * tcp object.
 */
relpRetVal
relpTcpSetPermittedPeers(relpTcp_t __attribute__((unused)) *pThis,
	relpPermittedPeers_t __attribute__((unused)) *pPeers)
{
	ENTER_RELPFUNC;
#ifdef ENABLE_TLS
	int i;
	relpTcpFreePermittedPeers(pThis);
	if(pPeers->nmemb != 0) {
		if((pThis->permittedPeers.peer =
			malloc(sizeof(tcpPermittedPeerEntry_t) * pPeers->nmemb)) == NULL) {
			ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
		}
		for(i = 0 ; i < pPeers->nmemb ; ++i) {
			if((pThis->permittedPeers.peer[i].name = strdup(pPeers->name[i])) == NULL) {
				ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
			}
			pThis->permittedPeers.peer[i].wildcardRoot = NULL;
			pThis->permittedPeers.peer[i].wildcardLast = NULL;
			CHKRet(relpTcpPermittedPeerWildcardCompile(&(pThis->permittedPeers.peer[i])));
		}
	}
	pThis->permittedPeers.nmemb = pPeers->nmemb;
#else
	ABORT_FINALIZE(RELP_RET_ERR_NO_TLS);
#endif /* #ifdef ENABLE_TLS */
finalize_it:
	LEAVE_RELPFUNC;
}

relpRetVal
relpTcpSetUsrPtr(relpTcp_t *pThis, void *pUsr)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Tcp);
	pThis->pUsr = pUsr;
	LEAVE_RELPFUNC;
}

relpRetVal
relpTcpSetAuthMode(relpTcp_t *pThis, relpAuthMode_t authmode)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Tcp);
	pThis->authmode = authmode;
	LEAVE_RELPFUNC;
}

relpRetVal
relpTcpSetGnuTLSPriString(relpTcp_t *pThis, char *pristr)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Tcp);
	
	free(pThis->pristring);
	if(pristr == NULL) {
		pThis->pristring = NULL;
	} else {
		if((pThis->pristring = strdup(pristr)) == NULL)
			ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}
finalize_it:
	LEAVE_RELPFUNC;
}

relpRetVal
relpTcpSetCACert(relpTcp_t *pThis, char *cert)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Tcp);
	
	free(pThis->caCertFile);
	if(cert == NULL) {
		pThis->caCertFile = NULL;
	} else {
		if((pThis->caCertFile = strdup(cert)) == NULL)
			ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}
finalize_it:
	LEAVE_RELPFUNC;
}

relpRetVal
relpTcpSetOwnCert(relpTcp_t *pThis, char *cert)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Tcp);
	
	free(pThis->ownCertFile);
	if(cert == NULL) {
		pThis->ownCertFile = NULL;
	} else {
		if((pThis->ownCertFile = strdup(cert)) == NULL)
			ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}
finalize_it:
	LEAVE_RELPFUNC;
}

relpRetVal
relpTcpSetPrivKey(relpTcp_t *pThis, char *cert)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Tcp);
	
	free(pThis->privKeyFile);
	if(cert == NULL) {
		pThis->privKeyFile = NULL;
	} else {
#		ifdef HAVE_GNUTLS_CERTIFICATE_SET_VERIFY_FUNCTION  
			if((pThis->privKeyFile = strdup(cert)) == NULL)
				ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
#		else
			ABORT_FINALIZE(RELP_RET_ERR_NO_TLS_AUTH);
#		endif
	}
finalize_it:
	LEAVE_RELPFUNC;
}


/* Enable TLS mode. */
relpRetVal
relpTcpEnableTLS(relpTcp_t __attribute__((unused)) *pThis)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Tcp);
#ifdef ENABLE_TLS
	pThis->bEnableTLS = 1;
#else
	iRet = RELP_RET_ERR_NO_TLS;
#endif /* #ifdef ENABLE_TLS */
	LEAVE_RELPFUNC;
}

relpRetVal
relpTcpEnableTLSZip(relpTcp_t __attribute__((unused)) *pThis)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Tcp);
#ifdef ENABLE_TLS
	pThis->bEnableTLSZip = 1;
#else
	iRet = RELP_RET_ERR_NO_TLS;
#endif /* #ifdef ENABLE_TLS */
	LEAVE_RELPFUNC;
}

relpRetVal
relpTcpSetDHBits(relpTcp_t *pThis, int bits)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Tcp);
	pThis->dhBits = bits;
	LEAVE_RELPFUNC;
}

#ifdef ENABLE_TLS
/* set TLS priority string, common code both for client and server */
static relpRetVal
relpTcpTLSSetPrio(relpTcp_t *pThis)
{
	int r;
	char pristringBuf[4096];
	char *pristring;
	ENTER_RELPFUNC;
	/* Compute priority string (in simple cases where the user does not care...) */
	if(pThis->pristring == NULL) {
		if(pThis->bEnableTLSZip) {
			strncpy(pristringBuf, "NORMAL:+ANON-DH:+COMP-ALL", sizeof(pristringBuf));
		} else {
			strncpy(pristringBuf, "NORMAL:+ANON-DH:+COMP-NULL", sizeof(pristringBuf));
		}
		pristringBuf[sizeof(pristringBuf)-1] = '\0';
		pristring = pristringBuf;
	} else {
		pristring = pThis->pristring;
	}

	r = gnutls_priority_set_direct(pThis->session, pristring, NULL);
	if(r == GNUTLS_E_INVALID_REQUEST) {
		ABORT_FINALIZE(RELP_RET_INVLD_TLS_PRIO);
	} else if(r != GNUTLS_E_SUCCESS) {
		ABORT_FINALIZE(RELP_RET_ERR_TLS_SETUP);
	}
finalize_it:
	if(iRet != RELP_RET_OK)
		chkGnutlsCode(pThis, "Failed to set GnuTLS priority", iRet, r);
	LEAVE_RELPFUNC;
}


static relpRetVal
relpTcpAcceptConnReqInitTLS(relpTcp_t *pThis, relpSrv_t *pSrv)
{
	int r;
	ENTER_RELPFUNC;

	r = gnutls_init(&pThis->session, GNUTLS_SERVER);
	if(chkGnutlsCode(pThis, "Failed to initialize GnuTLS", RELP_RET_ERR_TLS_SETUP, r)) {
		ABORT_FINALIZE(RELP_RET_ERR_TLS_SETUP);
	}

	gnutls_session_set_ptr(pThis->session, pThis);

	if(pSrv->pTcp->pristring != NULL)
		pThis->pristring = strdup(pSrv->pTcp->pristring);
	pThis->authmode = pSrv->pTcp->authmode;
	pThis->pUsr = pSrv->pUsr;
	CHKRet(relpTcpTLSSetPrio(pThis));

	if(isAnonAuth(pSrv->pTcp)) {
		r = gnutls_credentials_set(pThis->session, GNUTLS_CRD_ANON, pSrv->pTcp->anoncredSrv);
		if(chkGnutlsCode(pThis, "Failed setting anonymous credentials", RELP_RET_ERR_TLS_SETUP, r)) {
			ABORT_FINALIZE(RELP_RET_ERR_TLS_SETUP);
		}
	} else { /* cert-based auth */
		if(pSrv->pTcp->caCertFile == NULL) {
			gnutls_certificate_send_x509_rdn_sequence(pThis->session, 0);
		}
		r = gnutls_credentials_set(pThis->session, GNUTLS_CRD_CERTIFICATE, pSrv->pTcp->xcred);
		if(chkGnutlsCode(pThis, "Failed setting certificate credentials", RELP_RET_ERR_TLS_SETUP, r)) {
			ABORT_FINALIZE(RELP_RET_ERR_TLS_SETUP);
		}
	}
	gnutls_dh_set_prime_bits(pThis->session, pThis->dhBits);
	gnutls_certificate_server_set_request(pThis->session, GNUTLS_CERT_REQUEST);

	gnutls_transport_set_ptr(pThis->session, (gnutls_transport_ptr_t) pThis->sock);
	r = gnutls_handshake(pThis->session);
	if(r == GNUTLS_E_INTERRUPTED || r == GNUTLS_E_AGAIN) {
		pThis->pEngine->dbgprint("librelp: gnutls_handshake retry necessary (this is OK and expected)\n");
		pThis->rtryOp = relpTCP_RETRY_handshake;
	} else if(r != GNUTLS_E_SUCCESS) {
		chkGnutlsCode(pThis, "TLS handshake failed", RELP_RET_ERR_TLS_HANDS, r);
		ABORT_FINALIZE(RELP_RET_ERR_TLS_HANDS);
	}

	pThis->bTLSActive = 1;

finalize_it:
  	LEAVE_RELPFUNC;
}
#endif /* #ifdef ENABLE_TLS */

/* Enable KEEPALIVE handling on the socket.  */
static void
EnableKeepAlive(const relpTcp_t *__restrict__ const pThis,
	const relpSrv_t *__restrict__ const pSrv,
	const int sock)
{
	int ret;
	int optval;
	socklen_t optlen;

	optval = 1;
	optlen = sizeof(optval);
	ret = setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen);
	if(ret < 0) {
		pThis->pEngine->dbgprint("librelp: EnableKeepAlive socket call "
					"returns error %d\n", ret);
		goto done;
	}

#	if defined(TCP_KEEPCNT)
	if(pSrv->iKeepAliveProbes > 0) {
		optval = pSrv->iKeepAliveProbes;
		optlen = sizeof(optval);
		ret = setsockopt(sock, SOL_TCP, TCP_KEEPCNT, &optval, optlen);
	} else {
		ret = 0;
	}
#	else
	ret = -1;
#	endif
	if(ret < 0) {
		callOnErr(pThis, "librelp cannot set keepalive probes - ignored",
			  RELP_RET_WRN_NO_KEEPALIVE);
	}

#	if defined(TCP_KEEPCNT)
	if(pSrv->iKeepAliveTime > 0) {
		optval = pSrv->iKeepAliveTime;
		optlen = sizeof(optval);
		ret = setsockopt(sock, SOL_TCP, TCP_KEEPIDLE, &optval, optlen);
	} else {
		ret = 0;
	}
#	else
	ret = -1;
#	endif
	if(ret < 0) {
		callOnErr(pThis, "librelp cannot set keepalive time - ignored",
			  RELP_RET_WRN_NO_KEEPALIVE);
	}

#	if defined(TCP_KEEPCNT)
	if(pSrv->iKeepAliveIntvl > 0) {
		optval = pSrv->iKeepAliveIntvl;
		optlen = sizeof(optval);
		ret = setsockopt(sock, SOL_TCP, TCP_KEEPINTVL, &optval, optlen);
	} else {
		ret = 0;
	}
#	else
	ret = -1;
#	endif
	if(ret < 0) {
		callOnErr(pThis, "librelp cannot set keepalive intvl - ignored",
			  RELP_RET_WRN_NO_KEEPALIVE);
	}

	pThis->pEngine->dbgprint("KEEPALIVE enabled for socket %d\n", sock);

done:
  	return;
}

/* accept an incoming connection request, sock provides the socket on which we can
 * accept the new session.
 * rgerhards, 2008-03-17
 */
relpRetVal
relpTcpAcceptConnReq(relpTcp_t **ppThis, int sock, relpSrv_t *pSrv)
{
	relpTcp_t *pThis = NULL;
	int sockflags;
	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof(addr);
	int iNewSock = -1;
	relpEngine_t *pEngine = pSrv->pEngine;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);

	iNewSock = accept(sock, (struct sockaddr*) &addr, &addrlen);
	if(iNewSock < 0) {
		ABORT_FINALIZE(RELP_RET_ACCEPT_ERR);
	}

	if(pSrv->bKeepAlive)
		EnableKeepAlive(pThis, pSrv, iNewSock);

	/* construct our object so that we can use it... */
	CHKRet(relpTcpConstruct(&pThis, pEngine, RELP_SRV_CONN, pSrv));

	/* TODO: obtain hostname, normalize (callback?), save it */
	CHKRet(relpTcpSetRemHost(pThis, (struct sockaddr*) &addr));
	pThis->pEngine->dbgprint("remote host is '%s', ip '%s'\n", pThis->pRemHostName, pThis->pRemHostIP);

	/* set the new socket to non-blocking IO */
	if((sockflags = fcntl(iNewSock, F_GETFL)) != -1) {
		sockflags |= O_NONBLOCK;
		/* SETFL could fail too, so get it caught by the subsequent
		 * error check.
		 */
		sockflags = fcntl(iNewSock, F_SETFL, sockflags);
	}
	if(sockflags == -1) {
		pThis->pEngine->dbgprint("error %d setting fcntl(O_NONBLOCK) on relp socket %d", errno, iNewSock);
		ABORT_FINALIZE(RELP_RET_IO_ERR);
	}

	pThis->sock = iNewSock;
#ifdef ENABLE_TLS
	if(pSrv->pTcp->bEnableTLS) {
		pThis->bEnableTLS = 1;
		pThis->pSrv = pSrv;
		CHKRet(relpTcpSetPermittedPeers(pThis, &(pSrv->permittedPeers)));
		CHKRet(relpTcpAcceptConnReqInitTLS(pThis, pSrv));
	}
#endif /* #ifdef ENABLE_TLS */

	*ppThis = pThis;

finalize_it:
	if(iRet != RELP_RET_OK) {
		if(pThis != NULL)
			relpTcpDestruct(&pThis);
		/* the close may be redundant, but that doesn't hurt... */
		if(iNewSock >= 0)
			close(iNewSock);
	}

	LEAVE_RELPFUNC;
}

#ifdef ENABLE_TLS
#ifdef HAVE_GNUTLS_CERTIFICATE_SET_VERIFY_FUNCTION  
/* Convert a fingerprint to printable data. The function must be provided a
 * sufficiently large buffer. 512 bytes shall always do.
 */
static void
GenFingerprintStr(char *pFingerprint, int sizeFingerprint, char *fpBuf)
{
	int iSrc, iDst;

	fpBuf[0] = 'S', fpBuf[1] = 'H', fpBuf[2] = 'A'; fpBuf[3] = '1';
	// TODO: length check fo fpBuf (but far from being urgent...)
	for(iSrc = 0, iDst = 4 ; iSrc < sizeFingerprint ; ++iSrc, iDst += 3) {
		sprintf(fpBuf+iDst, ":%2.2X", (unsigned char) pFingerprint[iSrc]);
	}
}

/* Check the peer's ID in fingerprint auth mode. */
static int
relpTcpChkPeerFingerprint(relpTcp_t *pThis, gnutls_x509_crt cert)
{
	int r = 0;
	int i;
	char fingerprint[20];
	char fpPrintable[512];
	size_t size;
	int8_t found;

	/* obtain the SHA1 fingerprint */
	size = sizeof(fingerprint);
	r = gnutls_x509_crt_get_fingerprint(cert, GNUTLS_DIG_SHA1, fingerprint, &size);
	if(chkGnutlsCode(pThis, "Failed to obtain fingerprint from certificate", RELP_RET_ERR_TLS, r)) {
		r = GNUTLS_E_CERTIFICATE_ERROR; goto done;
	}
	GenFingerprintStr(fingerprint, (int) size, fpPrintable);
	pThis->pEngine->dbgprint("DDDD: peer's certificate SHA1 fingerprint: %s\n", fpPrintable);

	/* now search through the permitted peers to see if we can find a permitted one */
	found = 0;
	pThis->pEngine->dbgprint("DDDD: n peers %d\n", pThis->permittedPeers.nmemb);
	for(i = 0 ; i < pThis->permittedPeers.nmemb ; ++i) {
	pThis->pEngine->dbgprint("DDDD: checking peer '%s','%s'\n", fpPrintable, pThis->permittedPeers.peer[i].name);
		if(!strcmp(fpPrintable, pThis->permittedPeers.peer[i].name)) {
			found = 1;
			break;
		}
	}
	if(!found) {
		r = GNUTLS_E_CERTIFICATE_ERROR; goto done;
	}
done:
	if(r != 0) {
		callOnAuthErr(pThis, fpPrintable, "non-permited fingerprint", RELP_RET_AUTH_ERR_FP);
	}
	return r;
}
#endif /* #ifdef HAVE_GNUTLS_CERTIFICATE_SET_VERIFY_FUNCTION */

/* add a wildcard entry to this permitted peer. Entries are always
 * added at the tail of the list. pszStr and lenStr identify the wildcard
 * entry to be added. Note that the string is NOT \0 terminated, so
 * we must rely on lenStr for when it is finished.
 * rgerhards, 2008-05-27
 */
static relpRetVal
AddPermittedPeerWildcard(tcpPermittedPeerEntry_t *pEtry, char* pszStr, int lenStr)
{
	tcpPermittedPeerWildcardComp_t *pNew = NULL;
	int iSrc;
	int iDst;
	ENTER_RELPFUNC;

	if((pNew = calloc(1, sizeof(tcpPermittedPeerWildcardComp_t))) == NULL) {
		ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}

	if(lenStr == 0) {
		pNew->wildcardType = tcpPEER_WILDCARD_EMPTY_COMPONENT;
		FINALIZE;
	} else {
		/* alloc memory for the domain component. We may waste a byte or
		 * two, but that's ok.
		 */
		if((pNew->pszDomainPart = malloc(lenStr +1 )) == NULL) {
			ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
		}
	}

	if(pszStr[0] == '*') {
		pNew->wildcardType = tcpPEER_WILDCARD_AT_START;
		iSrc = 1; /* skip '*' */
	} else {
		iSrc = 0;
	}

	for(iDst = 0 ; iSrc < lenStr && pszStr[iSrc] != '*' ; ++iSrc, ++iDst)  {
		pNew->pszDomainPart[iDst] = pszStr[iSrc];
	}

	if(iSrc < lenStr) {
		if(iSrc + 1 == lenStr && pszStr[iSrc] == '*') {
			if(pNew->wildcardType == tcpPEER_WILDCARD_AT_START) {
				ABORT_FINALIZE(RELP_RET_INVLD_WILDCARD);
			} else {
				pNew->wildcardType = tcpPEER_WILDCARD_AT_END;
			}
		} else {
			/* we have an invalid wildcard, something follows the asterisk! */
			ABORT_FINALIZE(RELP_RET_INVLD_WILDCARD);
		}
	}

	if(lenStr == 1 && pNew->wildcardType == tcpPEER_WILDCARD_AT_START) {
		pNew->wildcardType = tcpPEER_WILDCARD_MATCH_ALL;
	}

	/* if we reach this point, we had a valid wildcard. We now need to
	 * properly terminate the domain component string.
	 */
	pNew->pszDomainPart[iDst] = '\0';
	pNew->lenDomainPart = strlen((char*)pNew->pszDomainPart);

finalize_it:
	if(iRet != RELP_RET_OK) {
		if(pNew != NULL) {
			if(pNew->pszDomainPart != NULL)
				free(pNew->pszDomainPart);
			free(pNew);
		}
	} else {
		/* add the element to linked list */
		if(pEtry->wildcardRoot == NULL) {
			pEtry->wildcardRoot = pNew;
			pEtry->wildcardLast = pNew;
		} else {
			pEtry->wildcardLast->pNext = pNew;
		}
		pEtry->wildcardLast = pNew;
	}
	LEAVE_RELPFUNC;
}

/* Compile a wildcard - must not yet be compiled */
static relpRetVal
relpTcpPermittedPeerWildcardCompile(tcpPermittedPeerEntry_t *pEtry)
{
	char *pC;
	char *pStart;
	ENTER_RELPFUNC;

	/* first check if we have a wildcard */
	for(pC = pEtry->name ; *pC != '\0' && *pC != '*' ; ++pC)
		/*EMPTY, just skip*/;

	if(*pC == '\0') { /* no wildcard found, we are done */
		FINALIZE;
	}

	/* if we reach this point, the string contains wildcards. So let's
	 * compile the structure. To do so, we must parse from dot to dot
	 * and create a wildcard entry for each domain component we find.
	 * We must also flag problems if we have an asterisk in the middle
	 * of the text (it is supported at the start or end only).
	 */
	pC = pEtry->name;
	while(*pC) {
		pStart = pC;
		/* find end of domain component */
		for( ; *pC != '\0' && *pC != '.' ; ++pC)
			/*EMPTY, just skip*/;
		CHKRet(AddPermittedPeerWildcard(pEtry, pStart, pC - pStart));
		/* now check if we have an empty component at end of string */
		if(*pC == '.' && *(pC + 1) == '\0') {
			/* pStart is a dummy, it is not used if length is 0 */
			CHKRet(AddPermittedPeerWildcard(pEtry, pStart, 0)); 
		}
		if(*pC != '\0')
			++pC;
	}

finalize_it:
	LEAVE_RELPFUNC;
}

#ifdef HAVE_GNUTLS_CERTIFICATE_SET_VERIFY_FUNCTION  
/* check a peer against a wildcard entry. This is a more lengthy
 * operation.
 */
static void
relpTcpChkOnePeerWildcard(tcpPermittedPeerWildcardComp_t *pRoot, char *peername, int *pbFoundPositiveMatch)
{
	tcpPermittedPeerWildcardComp_t *pWildcard;
	char *pC;
	char *pStart; /* start of current domain component */
	int iWildcard, iName; /* work indexes for backward comparisons */

	*pbFoundPositiveMatch = 0;
	pWildcard = pRoot;
	pC = peername;
	while(*pC != '\0') {
		if(pWildcard == NULL) {
			/* we have more domain components than we have wildcards --> no match */
			goto done;
		}
		pStart = pC;
		while(*pC != '\0' && *pC != '.') {
			++pC;
		}

		/* got the component, now do the match */
		switch(pWildcard->wildcardType) {
			case tcpPEER_WILDCARD_NONE:
				if(   pWildcard->lenDomainPart != pC - pStart
				   || strncmp((char*)pStart, (char*)pWildcard->pszDomainPart, pC - pStart)) {
					goto done;
				}
				break;
			case tcpPEER_WILDCARD_AT_START:
				/* we need to do the backwards-matching manually */
				if(pWildcard->lenDomainPart > pC - pStart) {
					goto done;
				}
				iName = (size_t) (pC - pStart) - pWildcard->lenDomainPart;
				iWildcard = 0;
				while(iWildcard < pWildcard->lenDomainPart) {
					if(pWildcard->pszDomainPart[iWildcard] != pStart[iName]) {
						goto done;
					}
					++iName;
					++iWildcard;
				}
				break;
			case tcpPEER_WILDCARD_AT_END:
				if(   pWildcard->lenDomainPart > pC - pStart
				   || strncmp((char*)pStart, (char*)pWildcard->pszDomainPart, pWildcard->lenDomainPart)) {
					goto done;
				}
				break;
			case tcpPEER_WILDCARD_MATCH_ALL:
				/* everything is OK, just continue */
				break;
			case tcpPEER_WILDCARD_EMPTY_COMPONENT:
				if(pC - pStart > 0) {
				   	/* if it is not empty, it is no match... */
					goto done;
				}
				break;
		}
		pWildcard =  pWildcard->pNext; /* we processed this entry */

		/* skip '.' if we had it and so prepare for next iteration */
		if(*pC == '.')
			++pC;
	}
	
	/* we need to adjust for a border case, that is if the last component is
	 * empty. That happens frequently if the domain root (e.g. "example.com.")
	 * is properly given.
	 */
	if(pWildcard != NULL && pWildcard->wildcardType == tcpPEER_WILDCARD_EMPTY_COMPONENT)
		pWildcard = pWildcard->pNext;

	if(pWildcard != NULL) {
		/* we have more domain components than in the name to be
		 * checked. So this is no match.
		 */
		goto done;
	}
	*pbFoundPositiveMatch = 1;
done:	return;
}

/* Perform a match on ONE peer name obtained from the certificate. This name
 * is checked against the set of configured credentials. *pbFoundPositiveMatch is
 * set to 1 if the ID matches. *pbFoundPositiveMatch must have been initialized
 * to 0 by the caller (this is a performance enhancement as we expect to be
 * called multiple times).
 */
static void
relpTcpChkOnePeerName(relpTcp_t *pThis, char *peername, int *pbFoundPositiveMatch)
{
	int i;

	for(i = 0 ; i < pThis->permittedPeers.nmemb ; ++i) {
		if(pThis->permittedPeers.peer[i].wildcardRoot == NULL) {
			/* simple string, only, no wildcards */
			if(!strcmp(peername, pThis->permittedPeers.peer[i].name)) {
				*pbFoundPositiveMatch = 1;
				break;
			}
		} else {
			relpTcpChkOnePeerWildcard(pThis->permittedPeers.peer[i].wildcardRoot,
			        peername, pbFoundPositiveMatch);
			if (*pbFoundPositiveMatch)
				break;
		}
	}
}

/* Obtain the CN from the DN field and hand it back to the caller
 * (which is responsible for destructing it). We try to follow 
 * RFC2253 as far as it makes sense for our use-case. This function
 * is considered a compromise providing good-enough correctness while
 * limiting code size and complexity. If a problem occurs, we may enhance
 * this function. A (pointer to a) certificate must be caller-provided.
 * The buffer for the name (namebuf) must also be caller-provided. A
 * size of 1024 is most probably sufficien. The
 * function returns 0 if all went well, something else otherwise.
 * Note that non-0 is also returned if no CN is found.
 */
static int
relpTcpGetCN(relpTcp_t *pThis, gnutls_x509_crt cert, char *namebuf, int lenNamebuf)
{
	int r;
	int gnuRet;
	int i,j;
	int bFound;
	size_t size;
	char szDN[1024]; /* this should really be large enough for any non-malicious case... */

	size = sizeof(szDN);
	gnuRet = gnutls_x509_crt_get_dn(cert, (char*)szDN, &size);
	if(chkGnutlsCode(pThis, "Failed to obtain DN from certificate", RELP_RET_ERR_TLS, gnuRet)) {
		r = 1; goto done;
	}

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
		r = 1; goto done;
	}

	/* we found a common name, now extract it */
	j = 0;
	while(szDN[i] != '\0' && szDN[i] != ',' && j < lenNamebuf-1) {
		if(szDN[i] == '\\') {
			/* hex escapes are not implemented */
			r = 2; goto done;
		} else {
			namebuf[j++] = szDN[i];
		}
		++i; /* char processed */
	}
	namebuf[j] = '\0';

	/* we got it - we ignore the rest of the DN string (if any). So we may
	 * not detect if it contains more than one CN
	 */
	r = 0;

done:
	return r;
}

/* Check the peer's ID in name auth mode. */
static int
relpTcpChkPeerName(relpTcp_t *pThis, gnutls_x509_crt cert)
{
	int r = 0;
	int ret;
	unsigned int status = 0;
	char cnBuf[1024]; /* this is sufficient for the DNSNAME... */
	char szAltName[1024]; /* this is sufficient for the DNSNAME... */
	int iAltName;
	char allNames[32*1024]; /* for error-reporting */
	int iAllNames;
	size_t szAltNameLen;
	int bFoundPositiveMatch;
	int gnuRet;

	ret = gnutls_certificate_verify_peers2(pThis->session, &status);
	if(ret < 0) {
		callOnAuthErr(pThis, "", "certificate validation failed",
			RELP_RET_AUTH_CERT_INVL);
		r = GNUTLS_E_CERTIFICATE_ERROR; goto done;
	}
	if(status != 0) { /* Certificate is not trusted */
		callOnAuthErr(pThis, "", "certificate validation failed",
			RELP_RET_AUTH_CERT_INVL);
		r = GNUTLS_E_CERTIFICATE_ERROR; goto done;
	}

	bFoundPositiveMatch = 0;
	iAllNames = 0;

	/* first search through the dNSName subject alt names */
	iAltName = 0;
	while(!bFoundPositiveMatch) { /* loop broken below */
		szAltNameLen = sizeof(szAltName);
		gnuRet = gnutls_x509_crt_get_subject_alt_name(cert, iAltName,
				szAltName, &szAltNameLen, NULL);
		if(gnuRet < 0)
			break;
		else if(gnuRet == GNUTLS_SAN_DNSNAME) {
			pThis->pEngine->dbgprint("librelp: subject alt dnsName: '%s'\n", szAltName);
			iAllNames += snprintf(allNames+iAllNames, sizeof(allNames)-iAllNames,
					      "DNSname: %s; ", szAltName);
			relpTcpChkOnePeerName(pThis, szAltName, &bFoundPositiveMatch);
			/* do NOT break, because there may be multiple dNSName's! */
		}
		++iAltName;
	}

	if(!bFoundPositiveMatch) {
		/* if we did not succeed so far, we try the CN part of the DN... */
		if(relpTcpGetCN(pThis, cert, cnBuf, sizeof(cnBuf)) == 0) {
			pThis->pEngine->dbgprint("librelp: relpTcp now checking auth for CN '%s'\n", cnBuf);
			iAllNames += snprintf(allNames+iAllNames, sizeof(allNames)-iAllNames,
					      "CN: %s; ", cnBuf);
			relpTcpChkOnePeerName(pThis, cnBuf, &bFoundPositiveMatch);
		}
	}

	if(!bFoundPositiveMatch) {
		callOnAuthErr(pThis, allNames, "no permited name found", RELP_RET_AUTH_ERR_NAME);
		r = GNUTLS_E_CERTIFICATE_ERROR; goto done;
	}
	r = 0;
done:
	return r;
}

/* This function will verify the peer's certificate, and check
 * if the hostname matches, as well as the activation, expiration dates.
 */
static int
relpTcpVerifyCertificateCallback(gnutls_session_t session)
{
	int r = 0;
	relpTcp_t *pThis;
	const gnutls_datum *cert_list;
	unsigned int list_size = 0;
	gnutls_x509_crt cert;
	int bMustDeinitCert = 0;

	pThis = (relpTcp_t*) gnutls_session_get_ptr(session);

	/* This function only works for X.509 certificates.  */
	if(gnutls_certificate_type_get(session) != GNUTLS_CRT_X509) {
		r = GNUTLS_E_CERTIFICATE_ERROR; goto done;
	}

	cert_list = gnutls_certificate_get_peers(pThis->session, &list_size);

	if(list_size < 1) {
		callOnAuthErr(pThis, "", "peer did not provide a certificate",
			      RELP_RET_AUTH_NO_CERT);
		r = GNUTLS_E_CERTIFICATE_ERROR; goto done;
	}

	/* If we reach this point, we have at least one valid certificate. 
	 * We always use only the first certificate. As of GnuTLS documentation, the
	 * first certificate always contains the remote peer's own certificate. All other
	 * certificates are issuer's certificates (up the chain). We are only interested
	 * in the first certificate, which is our peer. -- rgerhards, 2008-05-08
	 */
	gnutls_x509_crt_init(&cert);
	bMustDeinitCert = 1; /* indicate cert is initialized and must be freed on exit */
	gnutls_x509_crt_import(cert, &cert_list[0], GNUTLS_X509_FMT_DER);
	if(pThis->authmode == eRelpAuthMode_Fingerprint) {
		r = relpTcpChkPeerFingerprint(pThis, cert);
	} else {
		r = relpTcpChkPeerName(pThis, cert);
	}
	if(r != 0) goto done;

	/* notify gnutls to continue handshake normally */
	r = 0;

done:
	if(bMustDeinitCert)
		gnutls_x509_crt_deinit(cert);
	return r;
}
#endif /* #ifdef HAVE_GNUTLS_CERTIFICATE_SET_VERIFY_FUNCTION */

#if 0 /* enable if needed for debugging */
static void logFunction(int level, const char *msg)
{
	fprintf(stdout, "DDDD: GnuTLS log msg, level %d: %s", level, msg);
	fflush(stdout);
}
#endif

/* initialize the listener for TLS use */
static relpRetVal
relpTcpLstnInitTLS(relpTcp_t *pThis)
{
	int r;
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Tcp);

	gnutls_global_init();
	/* uncomment for (very intense) debug help
	 * gnutls_global_set_log_function(logFunction);
	 * gnutls_global_set_log_level(10); // 0 (no) to 9 (most), 10 everything
	 */

	if(isAnonAuth(pThis)) {
		r = gnutls_dh_params_init(&pThis->dh_params);
		if(chkGnutlsCode(pThis, "Failed to initialize dh_params", RELP_RET_ERR_TLS_SETUP, r)) {
			ABORT_FINALIZE(RELP_RET_ERR_TLS_SETUP);
		}
		r = gnutls_dh_params_generate2(pThis->dh_params, pThis->dhBits);
		if(chkGnutlsCode(pThis, "Failed to generate dh_params", RELP_RET_ERR_TLS_SETUP, r)) {
			ABORT_FINALIZE(RELP_RET_ERR_TLS_SETUP);
		}
		r = gnutls_anon_allocate_server_credentials(&pThis->anoncredSrv);
		if(chkGnutlsCode(pThis, "Failed to allocate server credentials", RELP_RET_ERR_TLS_SETUP, r)) {
			ABORT_FINALIZE(RELP_RET_ERR_TLS_SETUP);
		}
		gnutls_anon_set_server_dh_params(pThis->anoncredSrv, pThis->dh_params);
	} else {
#		ifdef HAVE_GNUTLS_CERTIFICATE_SET_VERIFY_FUNCTION  
		r = gnutls_certificate_allocate_credentials(&pThis->xcred);
		if(chkGnutlsCode(pThis, "Failed to allocate certificate credentials", RELP_RET_ERR_TLS_SETUP, r)) {
			ABORT_FINALIZE(RELP_RET_ERR_TLS_SETUP);
		}
		if(pThis->caCertFile != NULL) {
			r = gnutls_certificate_set_x509_trust_file(pThis->xcred,
				pThis->caCertFile, GNUTLS_X509_FMT_PEM);
			if(r < 0) {
				chkGnutlsCode(pThis, "Failed to set certificate trust files", RELP_RET_ERR_TLS_SETUP, r);
				ABORT_FINALIZE(RELP_RET_ERR_TLS_SETUP);
			}
			pThis->pEngine->dbgprint("librelp: obtained %d certificates from %s\n", r, pThis->caCertFile);
		}
		r = gnutls_certificate_set_x509_key_file (pThis->xcred,
			pThis->ownCertFile, pThis->privKeyFile, GNUTLS_X509_FMT_PEM);
		if(chkGnutlsCode(pThis, "Failed to set certificate key files", RELP_RET_ERR_TLS_SETUP, r)) {
			ABORT_FINALIZE(RELP_RET_ERR_TLS_SETUP);
		}
		if(pThis->authmode == eRelpAuthMode_None)
			pThis->authmode = eRelpAuthMode_Fingerprint;
		gnutls_certificate_set_verify_function(pThis->xcred, relpTcpVerifyCertificateCallback);
#		else /* #ifdef HAVE_GNUTLS_CERTIFICATE_SET_VERIFY_FUNCTION   */
		ABORT_FINALIZE(RELP_RET_ERR_NO_TLS_AUTH);
#		endif /* #ifdef HAVE_GNUTLS_CERTIFICATE_SET_VERIFY_FUNCTION   */
	}
finalize_it:
	LEAVE_RELPFUNC;
}
#endif /* #ifdef ENABLE_TLS */


/* initialize the tcp socket for a listner
 * pLstnPort is a pointer to a port name. NULL is not permitted.
 * gerhards, 2008-03-17
 */
relpRetVal
relpTcpLstnInit(relpTcp_t *pThis, unsigned char *pLstnPort, int ai_family)
{
        struct addrinfo hints, *res, *r;
        int error, maxs, *s, on = 1;
	int sockflags;
	unsigned char *pLstnPt;

	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Tcp);

	pLstnPt = pLstnPort;
	assert(pLstnPt != NULL);
	
	pThis->pEngine->dbgprint("creating relp tcp listen socket on port %s\n", pLstnPt);

        memset(&hints, 0, sizeof(hints));
        hints.ai_flags = AI_PASSIVE;
        hints.ai_family = ai_family;
        hints.ai_socktype = SOCK_STREAM;

        error = getaddrinfo(NULL, (char*) pLstnPt, &hints, &res);
        if(error) {
		pThis->pEngine->dbgprint("error %d querying port '%s'\n", error, pLstnPt);
		ABORT_FINALIZE(RELP_RET_INVALID_PORT);
	}

        /* Count max number of sockets we may open */
        for(maxs = 0, r = res; r != NULL ; r = r->ai_next, maxs++)
		/* EMPTY */;
        pThis->socks = malloc((maxs+1) * sizeof(int));
        if (pThis->socks == NULL) {
               pThis->pEngine->dbgprint("couldn't allocate memory for TCP listen sockets, suspending RELP message reception.");
               freeaddrinfo(res);
               ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
        }

        *pThis->socks = 0;   /* num of sockets counter at start of array */
        s = pThis->socks + 1;
	for(r = res; r != NULL ; r = r->ai_next) {
               *s = socket(r->ai_family, r->ai_socktype, r->ai_protocol);
        	if (*s < 0) {
			if(!(r->ai_family == PF_INET6 && errno == EAFNOSUPPORT))
				pThis->pEngine->dbgprint("creating relp tcp listen socket");
				/* it is debatable if PF_INET with EAFNOSUPPORT should
				 * also be ignored...
				 */
                        continue;
                }

#ifdef IPV6_V6ONLY
                if (r->ai_family == AF_INET6) {
                	int iOn = 1;
			if (setsockopt(*s, IPPROTO_IPV6, IPV6_V6ONLY,
			      (char *)&iOn, sizeof (iOn)) < 0) {
			close(*s);
			*s = -1;
			continue;
                	}
                }
#endif
       		if(setsockopt(*s, SOL_SOCKET, SO_REUSEADDR, (char *) &on, sizeof(on)) < 0 ) {
			pThis->pEngine->dbgprint("error %d setting relp/tcp socket option\n", errno);
                        close(*s);
			*s = -1;
			continue;
		}

		/* We use non-blocking IO! */
		if((sockflags = fcntl(*s, F_GETFL)) != -1) {
			sockflags |= O_NONBLOCK;
			/* SETFL could fail too, so get it caught by the subsequent
			 * error check.
			 */
			sockflags = fcntl(*s, F_SETFL, sockflags);
		}
		if(sockflags == -1) {
			pThis->pEngine->dbgprint("error %d setting fcntl(O_NONBLOCK) on relp socket", errno);
                        close(*s);
			*s = -1;
			continue;
		}

#ifdef ENABLE_TLS
		if(pThis->bEnableTLS) {
			CHKRet(relpTcpLstnInitTLS(pThis));
		}
#endif /* #ifdef ENABLE_TLS */

	        if( (bind(*s, r->ai_addr, r->ai_addrlen) < 0)
#ifndef IPV6_V6ONLY
		     && (errno != EADDRINUSE)
#endif
	           ) {
			char msgbuf[4096];
			snprintf(msgbuf, sizeof(msgbuf), "error while binding relp tcp socket "
				 "on port '%s'", pLstnPort);
			msgbuf[sizeof(msgbuf)-1] = '\0';
			callOnErr(pThis, msgbuf, errno);
                	close(*s);
			*s = -1;
                        continue;
                }

		if(listen(*s,pThis->iSessMax / 10 + 5) < 0) {
			/* If the listen fails, it most probably fails because we ask
			 * for a too-large backlog. So in this case we first set back
			 * to a fixed, reasonable, limit that should work. Only if
			 * that fails, too, we give up.
			 */
			pThis->pEngine->dbgprint("listen with a backlog of %d failed - retrying with default of 32.",
				    pThis->iSessMax / 10 + 5);
			if(listen(*s, 32) < 0) {
				pThis->pEngine->dbgprint("relp listen error %d, suspending\n", errno);
	                	close(*s);
				*s = -1;
               		        continue;
			}
		}

		(*pThis->socks)++;
		s++;
	}

        if(res != NULL)
               freeaddrinfo(res);

	if(*pThis->socks != maxs)
		pThis->pEngine->dbgprint("We could initialize %d RELP TCP listen sockets out of %d we received "
		 	"- this may or may not be an error indication.\n", *pThis->socks, maxs);

        if(*pThis->socks == 0) {
		pThis->pEngine->dbgprint("No RELP TCP listen socket could successfully be initialized, "
			 "message reception via RELP disabled.\n");
        	free(pThis->socks);
		ABORT_FINALIZE(RELP_RET_COULD_NOT_BIND);
	}

finalize_it:
	LEAVE_RELPFUNC;
}

/* receive data from a tcp socket
 * The lenBuf parameter must contain the max buffer size on entry and contains
 * the number of octets read (or -1 in case of error) on exit. This function
 * never blocks, not even when called on a blocking socket. That is important
 * for client sockets, which are set to block during send, but should not
 * block when trying to read data. If *pLenBuf is -1, an error occured and
 * errno holds the exact error cause.
 * rgerhards, 2008-03-17
 */
relpRetVal
relpTcpRcv(relpTcp_t *pThis, relpOctet_t *pRcvBuf, ssize_t *pLenBuf)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Tcp);

#ifdef ENABLE_TLS
	int r;
	if(pThis->bEnableTLS) {
		r = gnutls_record_recv(pThis->session, pRcvBuf, *pLenBuf);
		if(r == GNUTLS_E_INTERRUPTED || r == GNUTLS_E_AGAIN) {
			pThis->pEngine->dbgprint("librelp: gnutls_record_recv must be retried\n");
			pThis->rtryOp = relpTCP_RETRY_recv;
		} else {
			if(r < 0)
				chkGnutlsCode(pThis, "TLS record reception failed", RELP_RET_IO_ERR, r);
			pThis->rtryOp = relpTCP_RETRY_none;
		}
		*pLenBuf = (r < 0) ? -1 : r;
	} else {
#endif /* #ifdef ENABLE_TLS */
		*pLenBuf = recv(pThis->sock, pRcvBuf, *pLenBuf, MSG_DONTWAIT);
#ifdef ENABLE_TLS
	}
#endif /* #ifdef ENABLE_TLS */

	LEAVE_RELPFUNC;
}


/* helper for CORK option manipulation. As this is not portable, the
 * helper abstracts it. Note that it is a null-operation if no
 * such option is available on the platform in question.
 */
static inline void
setCORKopt(int sock, int onOff)
{
#if defined(TCP_CORK)
	setsockopt(sock, SOL_TCP, TCP_CORK, &onOff, sizeof (onOff));
#elif defined(TCP_NOPUSH)
	setsockopt(sock, IPPROTO_TCP, TCP_NOPUSH, &onOff, sizeof (onOff));
#endif
}
/* this function is called to hint librelp that a "burst" of data is to be
 * sent. librelp can than try to optimize it's handling. Right now, this 
 * means we turn on the CORK option and will turn it off when we are
 * hinted that the burst is over.
 * The function is intentionally void as it must operate in a way that
 * does not interfere with normal operations.
 */
void
relpTcpHintBurstBegin(relpTcp_t *pThis)
{
	setCORKopt(pThis->sock, 1);
}
/* this is the counterpart to relpTcpHintBurstBegin -- see there for doc */
void
relpTcpHintBurstEnd(relpTcp_t *pThis)
{
	setCORKopt(pThis->sock, 0);
}

/* send a buffer via TCP.
 * On entry, pLenBuf contains the number of octets to
 * write. On exit, it contains the number of octets actually written.
 * If this number is lower than on entry, only a partial buffer has
 * been written.
 * rgerhards, 2008-03-19
 */
relpRetVal
relpTcpSend(relpTcp_t *pThis, relpOctet_t *pBuf, ssize_t *pLenBuf)
{
	ssize_t written;
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Tcp);

#ifdef ENABLE_TLS
	if(pThis->bEnableTLS) {
		written = gnutls_record_send(pThis->session, pBuf, *pLenBuf);
		pThis->pEngine->dbgprint("librelp: TLS send returned %d\n", (int) written);
		if(written == GNUTLS_E_AGAIN || written == GNUTLS_E_INTERRUPTED) {
			pThis->rtryOp = relpTCP_RETRY_send;
			written = 0;
		} else {
			pThis->rtryOp = relpTCP_RETRY_none;
			if(written < 1) {
				chkGnutlsCode(pThis, "TLS record write failed", RELP_RET_IO_ERR, written);
				ABORT_FINALIZE(RELP_RET_IO_ERR);
			}
		}
	} else {
#endif /* #ifdef ENABLE_TLS */
		written = send(pThis->sock, pBuf, *pLenBuf, 0);
		if(written == -1) {
			switch(errno) {
				case EAGAIN:
				case EINTR:
					/* this is fine, just retry... */
					written = 0;
					break;
				default:
					ABORT_FINALIZE(RELP_RET_IO_ERR);
					break;
			}
		}
#ifdef ENABLE_TLS
	}
#endif /* #ifdef ENABLE_TLS */

	*pLenBuf = written;
finalize_it:
	LEAVE_RELPFUNC;
}

#ifdef ENABLE_TLS
/* this is only called for client-initiated sessions */
static relpRetVal
relpTcpConnectTLSInit(relpTcp_t *pThis)
{
	int r;
	int sockflags;
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Tcp);

	if(!called_gnutls_global_init) {
		gnutls_global_init();
		/* uncomment for (very intense) debug help
		 * gnutls_global_set_log_function(logFunction);
		 * gnutls_global_set_log_level(10); // 0 (no) to 9 (most), 10 everything
		 */
		pThis->pEngine->dbgprint("DDDD: gnutls_global_init() called\n");
		called_gnutls_global_init = 1;
	}
	r = gnutls_init(&pThis->session, GNUTLS_CLIENT);
	if(chkGnutlsCode(pThis, "Failed to initialize GnuTLS", RELP_RET_ERR_TLS_SETUP, r)) {
		ABORT_FINALIZE(RELP_RET_ERR_TLS_SETUP);
	}

	gnutls_session_set_ptr(pThis->session, pThis);
	CHKRet(relpTcpTLSSetPrio(pThis));

	if(isAnonAuth(pThis)) {
		r = gnutls_anon_allocate_client_credentials(&pThis->anoncred);
		if(chkGnutlsCode(pThis, "Failed to allocate client credentials", RELP_RET_ERR_TLS_SETUP, r)) {
			ABORT_FINALIZE(RELP_RET_ERR_TLS_SETUP);
		}
		/* put the anonymous credentials to the current session */
		r = gnutls_credentials_set(pThis->session, GNUTLS_CRD_ANON, pThis->anoncred);
		if(chkGnutlsCode(pThis, "Failed to set credentials", RELP_RET_ERR_TLS_SETUP, r)) {
			ABORT_FINALIZE(RELP_RET_ERR_TLS_SETUP);
		}
	} else {
#		ifdef HAVE_GNUTLS_CERTIFICATE_SET_VERIFY_FUNCTION  
		r = gnutls_certificate_allocate_credentials(&pThis->xcred);
		if(chkGnutlsCode(pThis, "Failed to allocate certificate credentials", RELP_RET_ERR_TLS_SETUP, r)) {
			ABORT_FINALIZE(RELP_RET_ERR_TLS_SETUP);
		}
		if(pThis->caCertFile != NULL) {
			r = gnutls_certificate_set_x509_trust_file(pThis->xcred,
				pThis->caCertFile, GNUTLS_X509_FMT_PEM);
			if(r < 0) {
				chkGnutlsCode(pThis, "Failed to set certificate trust file", RELP_RET_ERR_TLS_SETUP, r);
				ABORT_FINALIZE(RELP_RET_ERR_TLS_SETUP);
			}
			pThis->pEngine->dbgprint("librelp: obtained %d certificates from %s\n", r, pThis->caCertFile);
		}
		if(pThis->ownCertFile != NULL) {
			r = gnutls_certificate_set_x509_key_file (pThis->xcred,
				pThis->ownCertFile, pThis->privKeyFile, GNUTLS_X509_FMT_PEM);
			if(chkGnutlsCode(pThis, "Failed to set certificate key file", RELP_RET_ERR_TLS_SETUP, r)) {
				ABORT_FINALIZE(RELP_RET_ERR_TLS_SETUP);
			}
		}
		r = gnutls_credentials_set(pThis->session, GNUTLS_CRD_CERTIFICATE, pThis->xcred);
		if(chkGnutlsCode(pThis, "Failed to set credentials", RELP_RET_ERR_TLS_SETUP, r)) {
			ABORT_FINALIZE(RELP_RET_ERR_TLS_SETUP);
		}
		if(pThis->authmode == eRelpAuthMode_None)
			pThis->authmode = eRelpAuthMode_Fingerprint;
		gnutls_certificate_set_verify_function(pThis->xcred, relpTcpVerifyCertificateCallback);
#		else /* #ifdef HAVE_GNUTLS_CERTIFICATE_SET_VERIFY_FUNCTION   */
		ABORT_FINALIZE(RELP_RET_ERR_NO_TLS_AUTH);
#		endif /* #ifdef HAVE_GNUTLS_CERTIFICATE_SET_VERIFY_FUNCTION   */
	}

	gnutls_transport_set_ptr(pThis->session, (gnutls_transport_ptr_t) pThis->sock);
	//gnutls_handshake_set_timeout(pThis->session, GNUTLS_DEFAULT_HANDSHAKE_TIMEOUT);

	/* Perform the TLS handshake */
	do {
		r = gnutls_handshake(pThis->session);
		pThis->pEngine->dbgprint("DDDD: gnutls_handshake: %d: %s\n", r, gnutls_strerror(r));
		if(r == GNUTLS_E_INTERRUPTED || r == GNUTLS_E_AGAIN) {
			pThis->pEngine->dbgprint("librelp: gnutls_handshake must be retried\n");
			pThis->rtryOp = relpTCP_RETRY_handshake;
		} else if(r != GNUTLS_E_SUCCESS) {
			chkGnutlsCode(pThis, "TLS handshake failed", RELP_RET_ERR_TLS_SETUP, r);
			ABORT_FINALIZE(RELP_RET_ERR_TLS_SETUP);
		}
	}
	while(0);
	//while (r < 0 && gnutls_error_is_fatal(r) == 0);

	/* set the socket to non-blocking IO (we do this on the recv() for non-TLS */
	if((sockflags = fcntl(pThis->sock, F_GETFL)) != -1) {
		sockflags |= O_NONBLOCK;
		/* SETFL could fail too, so get it caught by the subsequent
		 * error check.  */
		sockflags = fcntl(pThis->sock, F_SETFL, sockflags);
	}
finalize_it:
	LEAVE_RELPFUNC;
}
#endif /* #ifdef ENABLE_TLS */

/* open a connection to a remote host (server).
 * This is only use for client initiated connections.
 * rgerhards, 2008-03-19
 */
relpRetVal
relpTcpConnect(relpTcp_t *pThis, int family, unsigned char *port, unsigned char *host, unsigned char *clientIP)
{
	struct addrinfo *res = NULL;
	struct addrinfo hints;
	struct addrinfo *reslocal = NULL;

	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Tcp);
	assert(port != NULL);
	assert(host != NULL);
	assert(pThis->sock == -1);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = family;
	hints.ai_socktype = SOCK_STREAM;
	if(getaddrinfo((char*)host, (char*)port, &hints, &res) != 0) {
		pThis->pEngine->dbgprint("error %d in getaddrinfo\n", errno);
		ABORT_FINALIZE(RELP_RET_IO_ERR);
	}
	
	if((pThis->sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
		ABORT_FINALIZE(RELP_RET_IO_ERR);
	}

	if(clientIP != NULL) {
		if(getaddrinfo((char*)clientIP, (char*)NULL, &hints, &reslocal) != 0) {
			pThis->pEngine->dbgprint("error %d in getaddrinfo of clientIP\n", errno);
			ABORT_FINALIZE(RELP_RET_IO_ERR);
		}
		if(bind(pThis->sock, reslocal->ai_addr, reslocal->ai_addrlen) != 0) {
			ABORT_FINALIZE(RELP_RET_IO_ERR);
		}
	}

	if(connect(pThis->sock, res->ai_addr, res->ai_addrlen) != 0) {
		ABORT_FINALIZE(RELP_RET_IO_ERR);
	}

#ifdef ENABLE_TLS
	if(pThis->bEnableTLS) {
		CHKRet(relpTcpConnectTLSInit(pThis));
		pThis->bTLSActive = 1;
	}
#endif /* #ifdef ENABLE_TLS */

finalize_it:
	if(res != NULL)
               freeaddrinfo(res);
	if(reslocal != NULL)
               freeaddrinfo(reslocal);
		
	if(iRet != RELP_RET_OK) {
		if(pThis->sock != -1) {
			close(pThis->sock);
			pThis->sock = -1;
		}
	}

	LEAVE_RELPFUNC;
}

#ifdef ENABLE_TLS
/* return direction in which retry must be done. We return the original
 * gnutls code, which means:
 * "0 if trying to read data, 1 if trying to write data."
 */
int
relpTcpGetRtryDirection(relpTcp_t *pThis)
{
	return gnutls_record_get_direction(pThis->session);
}

relpRetVal
relpTcpRtryHandshake(relpTcp_t *pThis)
{
	int r;
	ENTER_RELPFUNC;
	r = gnutls_handshake(pThis->session);
	if(r < 0) {
                pThis->pEngine->dbgprint("librelp: state %d during retry handshake: %s\n", r, gnutls_strerror(r));
	}
	if(r == GNUTLS_E_INTERRUPTED || r == GNUTLS_E_AGAIN) {
		; /* nothing to do, just keep our status... */
	} else if(r == 0) {
		pThis->rtryOp = relpTCP_RETRY_none;
	} else {
		chkGnutlsCode(pThis, "TLS handshake failed", RELP_RET_ERR_TLS_SETUP, r);
		ABORT_FINALIZE(RELP_RET_ERR_TLS_SETUP);
	}

finalize_it:
	LEAVE_RELPFUNC;
}
#endif /* #ifdef ENABLE_TLS */


/* wait until a socket is writable again. This is primarily for use in client cases.
 * It does NOT take care of our regular event loop, and must not be used by parts
 * of the code that are driven by this loop. Returns 0 if a timeout occured and 1
 * otherwise.
 */
int
relpTcpWaitWriteable(relpTcp_t *pThis, struct timespec *tTimeout)
{
	int r;
	fd_set writefds;
	struct timespec tCurr; /* current time */
	struct timeval tvSelect;

	clock_gettime(CLOCK_REALTIME, &tCurr);
	tvSelect.tv_sec = tTimeout->tv_sec - tCurr.tv_sec;
	tvSelect.tv_usec = (tTimeout->tv_nsec - tCurr.tv_nsec) / 1000000;
	if(tvSelect.tv_usec < 0) {
		tvSelect.tv_usec += 1000000;
		tvSelect.tv_sec--;
	}
	if(tvSelect.tv_sec < 0) {
		r = 0; goto done;
	}

	FD_ZERO(&writefds);
	FD_SET(pThis->sock, &writefds);
	pThis->pEngine->dbgprint("librelp: telpTcpWaitWritable doing select() "
		"on fd %d, timoeut %lld.%lld\n", pThis->sock,
		(long long) tTimeout->tv_sec, (long long) tTimeout->tv_nsec);
	r = select(pThis->sock+1, NULL, &writefds, NULL, &tvSelect);
done:	return r;
}
