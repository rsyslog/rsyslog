/* The mapping for relp over TCP.
 *
 * Copyright 2008 by Rainer Gerhards and Adiscon GmbH.
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

#include "relp.h"

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
} relpTcp_t;


/* macros for quick member access */
#define relpTcpGetNumSocks(pThis)    ((pThis)->socks[0])
#define relpTcpGetLstnSock(pThis, i) ((pThis)->socks[i])
#define relpTcpGetSock(pThis)        ((pThis)->sock)

/* prototypes */
relpRetVal relpTcpConstruct(relpTcp_t **ppThis, relpEngine_t *pEngine);
relpRetVal relpTcpDestruct(relpTcp_t **ppThis);
relpRetVal relpTcpAbortDestruct(relpTcp_t **ppThis);
relpRetVal relpTcpLstnInit(relpTcp_t *pThis, unsigned char *pLstnPort);
relpRetVal relpTcpAcceptConnReq(relpTcp_t **ppThis, int sock, relpEngine_t *pEngine);
relpRetVal relpTcpRcv(relpTcp_t *pThis, relpOctet_t *pRcvBuf, ssize_t *pLenBuf);
relpRetVal relpTcpSend(relpTcp_t *pThis, relpOctet_t *pBuf, ssize_t *pLenBuf);
relpRetVal relpTcpConnect(relpTcp_t *pThis, int family, unsigned char *port, unsigned char *host);

#endif /* #ifndef RELPTCP_H_INCLUDED */
