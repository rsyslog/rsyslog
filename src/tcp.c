/* This implements the relp mapping onto TCP.
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
#include "tcp.h"


/** Construct a RELP tcp instance
 * This is the first thing that a caller must do before calling any
 * RELP function. The relp tcp must only destructed after all RELP
 * operations have been finished.
 */
relpRetVal
relpTcpConstruct(relpTcp_t **ppThis, relpEngine_t *pEngine)
{
	relpTcp_t *pThis;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	if((pThis = calloc(1, sizeof(relpTcp_t))) == NULL) {
		ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}

	RELP_CORE_CONSTRUCTOR(pThis, Tcp);
	pThis->sock = -1;
	pThis->pEngine = pEngine;
	pThis->iSessMax = 500;	/* default max nbr of sessions - TODO: make configurable -- rgerhards, 2008-03-17*/

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

	if(pThis->pRemHostIP != NULL)
		free(pThis->pRemHostIP);
	if(pThis->pRemHostName != NULL)
		free(pThis->pRemHostName);

	/* done with de-init work, now free tcp object itself */
	free(pThis);
	*ppThis = NULL;

	LEAVE_RELPFUNC;
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
pEngine->dbgprint("getnameinfo returns %d\n", error);

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



/* accept an incoming connection request, sock provides the socket on which we can
 * accept the new session.
 * rgerhards, 2008-03-17
 */
relpRetVal
relpTcpAcceptConnReq(relpTcp_t **ppThis, int sock, relpEngine_t *pEngine)
{
	relpTcp_t *pThis = NULL;
	int sockflags;
	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof(addr);
	int iNewSock = -1;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);

	iNewSock = accept(sock, (struct sockaddr*) &addr, &addrlen);
	if(iNewSock < 0) {
		ABORT_FINALIZE(RELP_RET_ACCEPT_ERR);
	}

	/* construct our object so that we can use it... */
	CHKRet(relpTcpConstruct(&pThis, pEngine));

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


/* initialize the tcp socket for a listner
 * pLstnPort is a pointer to a port name. NULL is not permitted.
 * gerhards, 2008-03-17
 */
relpRetVal
relpTcpLstnInit(relpTcp_t *pThis, unsigned char *pLstnPort)
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
        hints.ai_family = PF_UNSPEC; /* TODO: permit to configure IPv4/v6 only! */
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


#if 0 // Do we really (still) need this?

		/* We need to enable BSD compatibility. Otherwise an attacker
		 * could flood our log files by sending us tons of ICMP errors.
		 */
#ifndef BSD	
		if(net.should_use_so_bsdcompat()) {
			if (setsockopt(*s, SOL_SOCKET, SO_BSDCOMPAT,
					(char *) &on, sizeof(on)) < 0) {
				errmsg.LogError(NO_ERRCODE, "TCP setsockopt(BSDCOMPAT)");
                                close(*s);
				*s = -1;
				continue;
			}
		}
#endif
#endif // #if 0

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
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Tcp);

	*pLenBuf = recv(pThis->sock, pRcvBuf, *pLenBuf, MSG_DONTWAIT);

	LEAVE_RELPFUNC;
}


/* send a buffer. On entry, pLenBuf contains the number of octets to
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

	*pLenBuf = written;
finalize_it:
pThis->pEngine->dbgprint("tcpSend returns %d\n", (int) *pLenBuf);
	LEAVE_RELPFUNC;
}


/* open a connection to a remote host (server).
 * rgerhards, 2008-03-19
 */
relpRetVal
relpTcpConnect(relpTcp_t *pThis, int family, unsigned char *port, unsigned char *host)
{
	struct addrinfo *res = NULL;
	struct addrinfo hints;

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

	if(connect(pThis->sock, res->ai_addr, res->ai_addrlen) != 0) {
		ABORT_FINALIZE(RELP_RET_IO_ERR);
	}

finalize_it:
	if(res != NULL)
               freeaddrinfo(res);
		
	if(iRet != RELP_RET_OK) {
		if(pThis->sock != -1) {
			close(pThis->sock);
			pThis->sock = -1;
		}
	}

	LEAVE_RELPFUNC;
}


