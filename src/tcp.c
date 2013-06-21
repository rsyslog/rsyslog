/* This implements the relp mapping onto TCP.
 *
 * Copyright 2008-2013 by Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of librelp.
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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include "relp.h"
#include "relpsrv.h"
#include "tcp.h"
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#if GNUTLS_VERSION_NUMBER <= 0x020b00
#	include <gcrypt.h>
#endif

#if GNUTLS_VERSION_NUMBER <= 0x020b00
GCRY_THREAD_OPTION_PTHREAD_IMPL;
#endif

static int called_gnutls_global_init = 0;


/* forward definitions */
static int relpTcpVerifyCertificateCallback(gnutls_session_t session);

/* helper to free permittedPeer structure */
static inline void
relpTcpFreePermittedPeers(relpTcp_t *pThis)
{
	int i;
	for(i = 0 ; i < pThis->permittedPeers.nmemb ; ++i)
		free(pThis->permittedPeers.name[i]);
	pThis->permittedPeers.nmemb = 0;
}

/** Construct a RELP tcp instance
 * This is the first thing that a caller must do before calling any
 * RELP function. The relp tcp must only destructed after all RELP
 * operations have been finished.
 */
relpRetVal
relpTcpConstruct(relpTcp_t **ppThis, relpEngine_t *pEngine, int connType)
{
	relpTcp_t *pThis;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	if((pThis = calloc(1, sizeof(relpTcp_t))) == NULL) {
		ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}

	RELP_CORE_CONSTRUCTOR(pThis, Tcp);
	pThis->bIsClient = (connType == RELP_SRV_CONN) ? 0 : 1;
	pThis->sock = -1;
	pThis->pEngine = pEngine;
	pThis->iSessMax = 500;	/* default max nbr of sessions - TODO: make configurable -- rgerhards, 2008-03-17*/
	pThis->bTLSActive = 0;
	pThis->dhBits = DEFAULT_DH_BITS;
	pThis->pristring = NULL;
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
	int gnuRet;

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

	if(pThis->bTLSActive) {
		gnuRet = gnutls_bye(pThis->session, GNUTLS_SHUT_RDWR);
		while(gnuRet == GNUTLS_E_INTERRUPTED || gnuRet == GNUTLS_E_AGAIN) {
			gnuRet = gnutls_bye(pThis->session, GNUTLS_SHUT_RDWR);
		}
		gnutls_deinit(pThis->session);
	}

	relpTcpFreePermittedPeers(pThis);
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
relpTcpSetPermittedPeers(relpTcp_t *pThis, relpPermittedPeers_t *pPeers)
{
	ENTER_RELPFUNC;
	int i;
	RELPOBJ_assert(pThis, Tcp);
	
	relpTcpFreePermittedPeers(pThis);
	if(pPeers->nmemb != 0) {
		if((pThis->permittedPeers.name =
			malloc(sizeof(char*) * pPeers->nmemb)) == NULL) {
			ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
		}
		for(i = 0 ; i < pPeers->nmemb ; ++i) {
			if((pThis->permittedPeers.name[i] = strdup(pPeers->name[i])) == NULL) {
				ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
			}
		}
	}
	pThis->permittedPeers.nmemb = pPeers->nmemb;
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
		if((pThis->privKeyFile = strdup(cert)) == NULL)
			ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}
finalize_it:
	LEAVE_RELPFUNC;
}


/* Enable TLS mode. */
relpRetVal
relpTcpEnableTLS(relpTcp_t *pThis)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Tcp);
	pThis->bEnableTLS = 1;
	LEAVE_RELPFUNC;
}

relpRetVal
relpTcpEnableTLSZip(relpTcp_t *pThis)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Tcp);
	pThis->bEnableTLSZip = 1;
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
	pThis->pEngine->dbgprint("DDDD: gnutls_priority_set_direct(\"%s\"): %d: %s\n", pristring, r, gnutls_strerror(r));
	if(r == GNUTLS_E_INVALID_REQUEST) {
		ABORT_FINALIZE(RELP_RET_INVLD_TLS_PRIO);
	} else if(r != GNUTLS_E_SUCCESS) {
		ABORT_FINALIZE(RELP_RET_ERR_TLS_SETUP);
	}
finalize_it:
	LEAVE_RELPFUNC;
}


static relpRetVal
relpTcpAcceptConnReqInitTLS(relpTcp_t *pThis, relpSrv_t *pSrv)
{
	int r;
	ENTER_RELPFUNC;

	r = gnutls_init(&pThis->session, GNUTLS_SERVER);
pThis->pEngine->dbgprint("DDDD: gnutls_init %d: %s\n", r, gnutls_strerror(r));
	gnutls_session_set_ptr(pThis->session, pThis);

	pThis->pristring = strdup(pSrv->pTcp->pristring);
	pThis->pUsr = pSrv->pUsr;
	CHKRet(relpTcpTLSSetPrio(pThis));

	if(isAnonAuth(pSrv->pTcp)) {
		r = gnutls_credentials_set(pThis->session, GNUTLS_CRD_ANON, pSrv->pTcp->anoncredSrv);
		pThis->pEngine->dbgprint("DDDD: gnutls_credentials_set (anon) %d: %s\n", r, gnutls_strerror(r));
	} else { /* cert-based auth */
		if(pSrv->pTcp->caCertFile == NULL) {
			gnutls_certificate_send_x509_rdn_sequence(pThis->session, 0);
		}
		r = gnutls_credentials_set(pThis->session, GNUTLS_CRD_CERTIFICATE, pSrv->pTcp->xcred);
		pThis->pEngine->dbgprint("DDDD: gnutls_credentials_set(cert) %d: %s\n", r, gnutls_strerror(r));
	}
	gnutls_dh_set_prime_bits(pThis->session, pThis->dhBits);
	gnutls_certificate_server_set_request(pThis->session, GNUTLS_CERT_REQUEST);

	gnutls_transport_set_ptr(pThis->session, (gnutls_transport_ptr_t) pThis->sock);
	r = gnutls_handshake(pThis->session);
pThis->pEngine->dbgprint("DDDD: gnutls_handshake: %d: %s\n", r, gnutls_strerror(r));
	if(r == GNUTLS_E_INTERRUPTED || r == GNUTLS_E_AGAIN) {
		pThis->pEngine->dbgprint("librelp: gnutls_handshake retry necessary (this is OK and expected)\n");
		pThis->rtryOp = relpTCP_RETRY_handshake;
	} else if(r != GNUTLS_E_SUCCESS) {
		ABORT_FINALIZE(RELP_RET_ERR_TLS_SETUP);
	}
	pThis->bTLSActive = 1;

finalize_it:
  	LEAVE_RELPFUNC;
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

	/* construct our object so that we can use it... */
	CHKRet(relpTcpConstruct(&pThis, pEngine, RELP_SRV_CONN));

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
	if(pSrv->pTcp->bEnableTLS) {
		pThis->bEnableTLS = 1;
		CHKRet(relpTcpSetPermittedPeers(pThis, &(pSrv->permittedPeers)));
		CHKRet(relpTcpAcceptConnReqInitTLS(pThis, pSrv));
	}

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


/* Check the peer's ID in fingerprint auth mode.
 * rgerhards, 2008-05-22
 */
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
	gnutls_x509_crt_get_fingerprint(cert, GNUTLS_DIG_SHA1, fingerprint, &size);
pThis->pEngine->dbgprint("DDDD: crt_get_fingerprint returned %d: %s\n", r, gnutls_strerror(r));
	GenFingerprintStr(fingerprint, (int) size, fpPrintable);
	pThis->pEngine->dbgprint("DDDD: peer's certificate SHA1 fingerprint: %s\n", fpPrintable);

	/* now search through the permitted peers to see if we can find a permitted one */
	found = 0;
pThis->pEngine->dbgprint("DDDD: n peers %d\n", pThis->permittedPeers.nmemb);
	for(i = 0 ; i < pThis->permittedPeers.nmemb ; ++i) {
pThis->pEngine->dbgprint("DDDD: checking peer '%s','%s'\n", fpPrintable, pThis->permittedPeers.name[i]);
		if(!strcmp(fpPrintable, pThis->permittedPeers.name[i])) {
			found = 1;
			break;
		}
	}
	if(!found) {
		r = GNUTLS_E_CERTIFICATE_ERROR; goto done;
	}
#if 0
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
			errmsg.LogError(0, RS_RET_INVALID_FINGERPRINT, "error: peer fingerprint '%s' unknown - we are "
					"not permitted to talk to it", cstrGetSzStr(pstrFingerprint));
			pThis->bReportAuthErr = 0;
		}
		ABORT_FINALIZE(RS_RET_INVALID_FINGERPRINT);
	}
#endif
done:
	if(r != 0) {
		callOnAuthErr(pThis, fpPrintable, "non-permited fingerprint", RELP_RET_AUTH_ERR_FP);
	}
	return r;
}

/* This function will verify the peer's certificate, and check
 * if the hostname matches, as well as the activation, expiration dates.
 */
static int
relpTcpVerifyCertificateCallback(gnutls_session_t session)
{
	unsigned int status = 0;
	relpRetVal relpRet = RELP_RET_OK;
	int r = 0;
	int ret, type;
	relpTcp_t *pThis;
	gnutls_datum_t out;
	const gnutls_datum *cert_list;
	unsigned int list_size = 0;
	gnutls_x509_crt cert;
	int bMustDeinitCert = 0;

	pThis = (relpTcp_t*) gnutls_session_get_ptr(session);
	pThis->pEngine->dbgprint("DDDD: in cert verify function (server)\n");

	/* This function only works for X.509 certificates.  */
	if(gnutls_certificate_type_get(session) != GNUTLS_CRT_X509) {
		r = GNUTLS_E_CERTIFICATE_ERROR; goto done;
	}

	cert_list = gnutls_certificate_get_peers(pThis->session, &list_size);

	if(list_size < 1) {
		callOnAuthErr(pThis, fpPrintable, "", "peer did not provide a "
			"certificate", RELP_RET_AUTH_NO_CERT);
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
pThis->pEngine->dbgprint("DDDD: got hold of the cert, on to actual checking...\n");
	r = relpTcpChkPeerFingerprint(pThis, cert);
	if(r != 0) goto done;


char szAltName[1024]; /* this is sufficient for the DNSNAME... */
int iAltName;
size_t szAltNameLen;
szAltNameLen = sizeof(szAltName);
	gnutls_x509_crt_get_subject_alt_name(cert, 0, szAltName, &szAltNameLen, NULL);
pThis->pEngine->dbgprint("DDDD: subject alt name is %s'\n", szAltName);


#if 0
  /* This verification function uses the trusted CAs in the credentials
   * structure. So you must have installed one or more CA certificates.
   */ ret = gnutls_certificate_verify_peers3 (session, hostname, &status);
  if (ret < 0)
    {
      printf ("Error\n");
      return GNUTLS_E_CERTIFICATE_ERROR;
    }

  type = gnutls_certificate_type_get (session);

  ret = gnutls_certificate_verification_status_print( status, type, &out, 0);
  if (ret < 0)
    {
      printf ("Error\n");
      return GNUTLS_E_CERTIFICATE_ERROR;
    }
  
  printf ("%s", out.data);
  
  gnutls_free(out.data);

  if (status != 0) /* Certificate is not trusted */
      return GNUTLS_E_CERTIFICATE_ERROR;
#endif

	/* notify gnutls to continue handshake normally */
	r = 0;

done:
	if(bMustDeinitCert)
		gnutls_x509_crt_deinit(cert);
	return r;
}

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
		pThis->pEngine->dbgprint("DDDD: dh_param_init returns %d\n", r);
		r = gnutls_dh_params_generate2(pThis->dh_params, pThis->dhBits);
		pThis->pEngine->dbgprint("DDDD: paramgenerate returns %d\n", r);
		r = gnutls_anon_allocate_server_credentials(&pThis->anoncredSrv);
		pThis->pEngine->dbgprint("DDDD: generating server DH params...\n");
		gnutls_anon_set_server_dh_params(pThis->anoncredSrv, pThis->dh_params);
	} else {
		gnutls_certificate_allocate_credentials(&pThis->xcred);
		if(pThis->caCertFile != NULL) {
			r = gnutls_certificate_set_x509_trust_file(pThis->xcred,
				pThis->caCertFile, GNUTLS_X509_FMT_PEM);
			pThis->pEngine->dbgprint("DDDD: certificate_set_x509_trust_file returns %d: %s\n", r, gnutls_strerror(r));
			if(r >= 0) {
				pThis->pEngine->dbgprint("librelp: obtained %d certificates from %s\n",
					r, pThis->caCertFile);
			} else {
			 // TODO: save error message
			}
		}
		r = gnutls_certificate_set_x509_key_file (pThis->xcred,
			pThis->ownCertFile, pThis->privKeyFile, GNUTLS_X509_FMT_PEM);
		pThis->pEngine->dbgprint("DDDD: certificate_set_x509_key_file returns %d\n", r);
		//gnutls_certificate_set_dh_params(pThis->xcred, pThis->dh_params);
		gnutls_certificate_set_verify_function(pThis->xcred, relpTcpVerifyCertificateCallback);
	}

	pThis->pEngine->dbgprint("DDDD: done Lstn  InitTLS\n");
	LEAVE_RELPFUNC;
}


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
	assert(pLstnPt != NULL); pThis->pEngine->dbgprint("creating relp tcp listen socket on port %s\n", pLstnPt);

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

		if(pThis->bEnableTLS) {
			CHKRet(relpTcpLstnInitTLS(pThis));
		}

	        if( (bind(*s, r->ai_addr, r->ai_addrlen) < 0)
#ifndef IPV6_V6ONLY
		     && (errno != EADDRINUSE)
#endif
	           ) {
                        pThis->pEngine->dbgprint("error %d while binding relp tcp socket", errno);
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
	int r;
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Tcp);

	if(pThis->bEnableTLS) {
		r = gnutls_record_recv(pThis->session, pRcvBuf, *pLenBuf);
		if(r == GNUTLS_E_INTERRUPTED || r == GNUTLS_E_AGAIN) {
			pThis->pEngine->dbgprint("librelp: gnutls_record_recv must be retried\n");
			pThis->rtryOp = relpTCP_RETRY_recv;
		} else {
			pThis->rtryOp = relpTCP_RETRY_none;
		}
		*pLenBuf = (r < 0) ? -1 : r;
	} else {
		*pLenBuf = recv(pThis->sock, pRcvBuf, *pLenBuf, MSG_DONTWAIT);
	}

	LEAVE_RELPFUNC;
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

	if(pThis->bEnableTLS) {
		written = gnutls_record_send(pThis->session, pBuf, *pLenBuf);
		pThis->pEngine->dbgprint("librelp: TLS send returned %d\n", (int) written);
		if(written == GNUTLS_E_AGAIN || written == GNUTLS_E_INTERRUPTED) {
			pThis->rtryOp = relpTCP_RETRY_send;
			written = 0;
		} else {
			pThis->rtryOp = relpTCP_RETRY_none;
			if(written < 1) {
				pThis->pEngine->dbgprint("librelp: error in gnutls_record_send: %s\n",
					gnutls_strerror(written));
				ABORT_FINALIZE(RELP_RET_IO_ERR);
			}
		}
	} else {
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
	}

	*pLenBuf = written;
finalize_it:
	LEAVE_RELPFUNC;
}

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
	pThis->pEngine->dbgprint("DDDD: gnutls_init: %d\n", r);

	gnutls_session_set_ptr(pThis->session, pThis);
	CHKRet(relpTcpTLSSetPrio(pThis));

	if(isAnonAuth(pThis)) {
		r = gnutls_anon_allocate_client_credentials(&pThis->anoncred);
		pThis->pEngine->dbgprint("DDDD: gnutls_anon_allocat_client_credentials: %d\n", r);
		/* put the anonymous credentials to the current session */
		r = gnutls_credentials_set(pThis->session, GNUTLS_CRD_ANON, pThis->anoncred);
		pThis->pEngine->dbgprint("DDDD: gnutls_credentials_set: %d\n", r);
	} else {
		gnutls_certificate_allocate_credentials(&pThis->xcred);
		if(pThis->caCertFile != NULL) {
			r = gnutls_certificate_set_x509_trust_file(pThis->xcred,
				pThis->caCertFile, GNUTLS_X509_FMT_PEM);
			pThis->pEngine->dbgprint("DDDD: certificate_set_x509_trust_file returns %d: %s\n", r, gnutls_strerror(r));
			if(r >= 0) {
				pThis->pEngine->dbgprint("librelp: obtained %d certificates from %s\n",
					r, pThis->caCertFile);
			} else {
			 // TODO: save error message
			}
		}
		if(pThis->ownCertFile != NULL) {
			r = gnutls_certificate_set_x509_key_file (pThis->xcred,
				pThis->ownCertFile, pThis->privKeyFile, GNUTLS_X509_FMT_PEM);
			pThis->pEngine->dbgprint("DDDD: certificate_set_x509_key_file returns %d\n", r);
		}
		r = gnutls_credentials_set(pThis->session, GNUTLS_CRD_CERTIFICATE, pThis->xcred);
		pThis->pEngine->dbgprint("DDDD: gnutls_credentials_set(cert) %d: %s\n", r, gnutls_strerror(r));
		gnutls_certificate_set_verify_function(pThis->xcred, relpTcpVerifyCertificateCallback);
	}

	gnutls_transport_set_ptr(pThis->session, (gnutls_transport_ptr_t) pThis->sock);
	//gnutls_handshake_set_timeout(pThis->session, GNUTLS_DEFAULT_HANDSHAKE_TIMEOUT);

	/* Perform the TLS handshake */
	do
	{
		r = gnutls_handshake(pThis->session);
		pThis->pEngine->dbgprint("DDDD: gnutls_handshake: %d: %s\n", r, gnutls_strerror(r));
		if(r == GNUTLS_E_INTERRUPTED || r == GNUTLS_E_AGAIN) {
			pThis->pEngine->dbgprint("librelp: gnutls_handshake must be retried\n");
			pThis->rtryOp = relpTCP_RETRY_handshake;
		} else if(r != GNUTLS_E_SUCCESS) {
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

	if(pThis->bEnableTLS) {
		CHKRet(relpTcpConnectTLSInit(pThis));
		pThis->bTLSActive = 1;
	}

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
		ABORT_FINALIZE(RELP_RET_IO_ERR);
	}

finalize_it:
	LEAVE_RELPFUNC;
}
