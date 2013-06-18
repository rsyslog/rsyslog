/* The mapping for relp over TCP.
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
 * along with librelp.  If not, see <http://www.gnu.org/licenses/>.
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
#ifndef RELPTCP_H_INCLUDED
#define	RELPTCP_H_INCLUDED

#include <stdint.h>
#include <gnutls/gnutls.h>
#include "relp.h"

typedef enum { relpTCP_RETRY_none = 0,
       relpTCP_RETRY_handshake = 1,
       relpTCP_RETRY_recv = 2, 
       relpTCP_RETRY_send = 3 } relpTcpRtryState_t;

#define RELP_SRV_CONN 0	/**< this conection is a server connection */
#define RELP_CLT_CONN 1	/**< this conection is a client connection */	

/* the RELPTCP object 
 * rgerhards, 2008-03-16
 */
typedef struct relpTcp_s {
	BEGIN_RELP_OBJ;
	relpEngine_t *pEngine;
	unsigned char *pRemHostIP; /**< IP address of remote peer (currently used in server mode, only) */
	unsigned char *pRemHostName; /**< host name of remote peer (currently used in server mode, only) */
	int sock;	/**< the socket we use for regular, single-socket, operations */
	int *socks;	/**< the socket(s) we use for listeners, element 0 has nbr of socks */
	int iSessMax;	/**< maximum number of sessions permitted */
	/* variables for TLS support */
	uint8_t bEnableTLS;
	uint8_t bTLSActive;	/**< is TLS actually active (properly activated) on this session? */
	uint8_t bEnableTLSZip;
	uint8_t bIsClient;	/**< set if this belongs to a client, if unset --> server */
	int dhBits;	/**< number of bits for Diffie-Hellman key */
	char *pristring; /**< priority string for GnuTLS */
	gnutls_anon_client_credentials_t anoncred;	/**< client anon credentials */
	gnutls_anon_server_credentials_t anoncredSrv;	/**< server anon credentials */
	relpPermittedPeers_t permittedPeers;
	/* GnuTLS certificat support */
	gnutls_certificate_credentials_t xcred;		/**< certificate credentials */
	char *caCertFile;
	char *ownCertFile;
	char *privKeyFile;
	gnutls_session_t session;
	gnutls_dh_params_t dh_params; /**< server DH parameters for anon mode */
	relpTcpRtryState_t rtryOp;
} relpTcp_t;


/* macros for quick member access */
#define relpTcpGetNumSocks(pThis)    ((pThis)->socks[0])
#define relpTcpGetLstnSock(pThis, i) ((pThis)->socks[i])
#define relpTcpGetSock(pThis)        ((pThis)->sock)

/* inlines (only for library-internal use!) */
static inline relpTcpRtryState_t
relpTcpRtryOp(relpTcp_t *pThis)
{
	return pThis->rtryOp;
}

/* prototypes */
relpRetVal relpTcpConstruct(relpTcp_t **ppThis, relpEngine_t *pEngine, int connType);
relpRetVal relpTcpDestruct(relpTcp_t **ppThis);
relpRetVal relpTcpAbortDestruct(relpTcp_t **ppThis);
relpRetVal relpTcpLstnInit(relpTcp_t *pThis, unsigned char *pLstnPort, int ai_family);
relpRetVal relpTcpAcceptConnReq(relpTcp_t **ppThis, int sock, relpSrv_t *pSrv);
relpRetVal relpTcpRcv(relpTcp_t *pThis, relpOctet_t *pRcvBuf, ssize_t *pLenBuf);
relpRetVal relpTcpSend(relpTcp_t *pThis, relpOctet_t *pBuf, ssize_t *pLenBuf);
relpRetVal relpTcpConnect(relpTcp_t *pThis, int family, unsigned char *port, unsigned char *host, unsigned char *clientIP);
relpRetVal relpTcpEnableTLS(relpTcp_t *pThis);
relpRetVal relpTcpEnableTLSZip(relpTcp_t *pThis);
relpRetVal relpTcpSetDHBits(relpTcp_t *pThis, int bits);
relpRetVal relpTcpSetGnuTLSPriString(relpTcp_t *pThis, char *pristr);
relpRetVal relpTcpSetCACert(relpTcp_t *pThis, char *cert);
relpRetVal relpTcpSetOwnCert(relpTcp_t *pThis, char *cert);
relpRetVal relpTcpSetPrivKey(relpTcp_t *pThis, char *cert);
relpRetVal relpTcpSetPermittedPeers(relpTcp_t *pThis, relpPermittedPeers_t *pPeers);
relpRetVal relpTcpRtryHandshake(relpTcp_t *pThis);
int relpTcpGetRtryDirection(relpTcp_t *pThis);

#endif /* #ifndef RELPTCP_H_INCLUDED */
