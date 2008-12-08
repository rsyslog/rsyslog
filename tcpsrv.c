/* tcpsrv.c
 *
 * Common code for plain TCP based servers. This is currently being
 * utilized by imtcp and imgssapi. I suspect that when we implement
 * SSL/TLS, that module could also use tcpsrv.
 *
 * There are actually two classes within the tcpserver code: one is
 * the tcpsrv itself, the other one is its sessions. This is a helper
 * class to tcpsrv.
 *
 * The common code here calls upon specific functionality by using
 * callbacks. The specialised input modules need to set the proper
 * callbacks before the code is run. The tcpsrv then calls back
 * into the specific input modules at the appropriate time.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * File begun on 2007-12-21 by RGerhards (extracted from syslogd.c)
 *
 * Copyright 2007, 2008 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Rsyslog is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rsyslog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Rsyslog.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */

#include "config.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <ctype.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include "rsyslog.h"
#include "syslogd.h"
#include "cfsysline.h"
#include "module-template.h"
#include "net.h"
#include "srUtils.h"
#include "conf.h"
#include "tcpsrv.h"
#include "obj.h"
#include "errmsg.h"

MODULE_TYPE_LIB

/* defines */
#define TCPSESS_MAX_DEFAULT 200 /* default for nbr of tcp sessions if no number is given */

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(conf)
DEFobjCurrIf(tcps_sess)
DEFobjCurrIf(errmsg)
DEFobjCurrIf(net)



/* code to free all sockets within a socket table.
 * A socket table is a descriptor table where the zero
 * element has the count of elements. This is used for
 * listening sockets. The socket table itself is also
 * freed.
 * A POINTER to this structure must be provided, thus
 * double indirection!
 * rgerhards, 2007-06-28
 */
static void freeAllSockets(int **socks)
{
	assert(socks != NULL);
	assert(*socks != NULL);
	while(**socks) {
		dbgprintf("Closing socket %d.\n", (*socks)[**socks]);
		close((*socks)[**socks]);
		(**socks)--;
	}
	free(*socks);
	*socks = NULL;
}


/* configure TCP listener settings. This is called during command
 * line parsing. The argument following -t is supplied as an argument.
 * The format of this argument is
 * "<port-to-use>, <nbr-of-sessions>"
 * Typically, there is no whitespace between port and session number.
 * (but it may be...).
 * NOTE: you can not use dbgprintf() in here - the dbgprintf() system is
 * not yet initilized when this function is called.
 * rgerhards, 2007-06-21
 * The port in cOptarg is handed over to us - the caller MUST NOT free it!
 * rgerhards, 2008-03-20
 */
static void
configureTCPListen(tcpsrv_t *pThis, char *cOptarg)
{
	register int i;
	register char *pArg = cOptarg;

	assert(cOptarg != NULL);
	ISOBJ_TYPE_assert(pThis, tcpsrv);

	/* extract port */
	i = 0;
	while(isdigit((int) *pArg)) {
		i = i * 10 + *pArg++ - '0';
	}

	if(pThis->TCPLstnPort != NULL) {
		free(pThis->TCPLstnPort);
		pThis->TCPLstnPort = NULL;
	}
		
	if( i >= 0 && i <= 65535) {
		pThis->TCPLstnPort = cOptarg;
	} else {
		errmsg.LogError(NO_ERRCODE, "Invalid TCP listen port %s - changed to 514.\n", cOptarg);
	}
}


/* Initialize the session table
 * returns 0 if OK, somewhat else otherwise
 */
static rsRetVal
TCPSessTblInit(tcpsrv_t *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, tcpsrv);
	assert(pThis->pSessions == NULL);

	dbgprintf("Allocating buffer for %d TCP sessions.\n", pThis->iSessMax);
	if((pThis->pSessions = (tcps_sess_t **) calloc(pThis->iSessMax, sizeof(tcps_sess_t *))) == NULL) {
		dbgprintf("Error: TCPSessInit() could not alloc memory for TCP session table.\n");
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}

finalize_it:
	RETiRet;
}


/* find a free spot in the session table. If the table
 * is full, -1 is returned, else the index of the free
 * entry (0 or higher).
 */
static int
TCPSessTblFindFreeSpot(tcpsrv_t *pThis)
{
	register int i;

	ISOBJ_TYPE_assert(pThis, tcpsrv);

	for(i = 0 ; i < pThis->iSessMax ; ++i) {
		if(pThis->pSessions[i] == NULL)
			break;
	}

	return((i < pThis->iSessMax) ? i : -1);
}


/* Get the next session index. Free session tables entries are
 * skipped. This function is provided the index of the last
 * session entry, or -1 if no previous entry was obtained. It
 * returns the index of the next session or -1, if there is no
 * further entry in the table. Please note that the initial call
 * might as well return -1, if there is no session at all in the
 * session table.
 */
static int
TCPSessGetNxtSess(tcpsrv_t *pThis, int iCurr)
{
	register int i;

	ISOBJ_TYPE_assert(pThis, tcpsrv);
	for(i = iCurr + 1 ; i < pThis->iSessMax ; ++i)
		if(pThis->pSessions[i] != NULL)
			break;

	return((i < pThis->iSessMax) ? i : -1);
}


/* De-Initialize TCP listner sockets.
 * This function deinitializes everything, including freeing the
 * session table. No TCP listen receive operations are permitted
 * unless the subsystem is reinitialized.
 * rgerhards, 2007-06-21
 */
static void deinit_tcp_listener(tcpsrv_t *pThis)
{
	int iTCPSess;

	ISOBJ_TYPE_assert(pThis, tcpsrv);
	assert(pThis->pSessions != NULL);

	/* close all TCP connections! */
	iTCPSess = TCPSessGetNxtSess(pThis, -1);
	while(iTCPSess != -1) {
		tcps_sess.Destruct(&pThis->pSessions[iTCPSess]);
		/* now get next... */
		iTCPSess = TCPSessGetNxtSess(pThis, iTCPSess);
	}
	
	/* we are done with the session table - so get rid of it...
	*/
	free(pThis->pSessions);
	pThis->pSessions = NULL; /* just to make sure... */

	if(pThis->TCPLstnPort != NULL)
		free(pThis->TCPLstnPort);

	/* finally close the listen sockets themselfs */
	freeAllSockets(&pThis->pSocksLstn);
}


/* Initialize TCP sockets (for listener)
 * This function returns either NULL (which means it failed) or 
 * a pointer to an array of file descriptiors. If the pointer is
 * returned, the zeroest element [0] contains the count of valid
 * descriptors. The descriptors themself follow in range
 * [1] ... [num-descriptors]. It is guaranteed that each of these
 * descriptors is valid, at least when this function returns.
 * Please note that technically the array may be larger than the number
 * of valid pointers stored in it. The memory overhead is minimal, so
 * we do not bother to re-allocate an array of the exact size. Logically,
 * the array still contains the exactly correct number of descriptors.
 */
static int *create_tcp_socket(tcpsrv_t *pThis)
{
        struct addrinfo hints, *res, *r;
        int error, maxs, *s, *socks, on = 1;
	char *TCPLstnPort;

	ISOBJ_TYPE_assert(pThis, tcpsrv);

	if(!strcmp(pThis->TCPLstnPort, "0"))
		TCPLstnPort = "514";
		/* use default - we can not do service db update, because there is
		 * no IANA-assignment for syslog/tcp. In the long term, we might
		 * re-use RFC 3195 port of 601, but that would probably break to
		 * many existing configurations.
		 * rgerhards, 2007-06-28
		 */
	else
		TCPLstnPort = pThis->TCPLstnPort;
	dbgprintf("creating tcp socket on port %s\n", TCPLstnPort);
        memset(&hints, 0, sizeof(hints));
        hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;
        hints.ai_family = family;
        hints.ai_socktype = SOCK_STREAM;

        error = getaddrinfo(NULL, TCPLstnPort, &hints, &res);
        if(error) {
               errmsg.LogError(NO_ERRCODE, "%s", gai_strerror(error));
	       return NULL;
	}

        /* Count max number of sockets we may open */
        for (maxs = 0, r = res; r != NULL ; r = r->ai_next, maxs++)
		/* EMPTY */;
        socks = malloc((maxs+1) * sizeof(int));
        if (socks == NULL) {
               errmsg.LogError(NO_ERRCODE, "couldn't allocate memory for TCP listen sockets, suspending TCP message reception.");
               freeaddrinfo(res);
               return NULL;
        }

        *socks = 0;   /* num of sockets counter at start of array */
        s = socks + 1;
	for (r = res; r != NULL ; r = r->ai_next) {
               *s = socket(r->ai_family, r->ai_socktype, r->ai_protocol);
        	if (*s < 0) {
			if(!(r->ai_family == PF_INET6 && errno == EAFNOSUPPORT))
				errmsg.LogError(NO_ERRCODE, "create_tcp_socket(), socket");
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
			errmsg.LogError(NO_ERRCODE, "TCP setsockopt");
			close(*s);
			*s = -1;
			continue;
                	}
                }
#endif
       		if (setsockopt(*s, SOL_SOCKET, SO_REUSEADDR,
			       (char *) &on, sizeof(on)) < 0 ) {
			errmsg.LogError(NO_ERRCODE, "TCP setsockopt(REUSEADDR)");
                        close(*s);
			*s = -1;
			continue;
		}

		/* We need to enable BSD compatibility. Otherwise an attacker
		 * could flood our log files by sending us tons of ICMP errors.
		 */
#ifndef OS_BSD	
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

	        if( (bind(*s, r->ai_addr, r->ai_addrlen) < 0)
#ifndef IPV6_V6ONLY
		     && (errno != EADDRINUSE)
#endif
	           ) {
                        errmsg.LogError(NO_ERRCODE, "TCP bind");
                	close(*s);
			*s = -1;
                        continue;
                }

		if( listen(*s,pThis->iSessMax / 10 + 5) < 0) {
			/* If the listen fails, it most probably fails because we ask
			 * for a too-large backlog. So in this case we first set back
			 * to a fixed, reasonable, limit that should work. Only if
			 * that fails, too, we give up.
			 */
			errmsg.LogError(NO_ERRCODE, "listen with a backlog of %d failed - retrying with default of 32.",
				    pThis->iSessMax / 10 + 5);
			if(listen(*s, 32) < 0) {
				errmsg.LogError(NO_ERRCODE, "TCP listen, suspending tcp inet");
	                	close(*s);
				*s = -1;
               		        continue;
			}
		}

		(*socks)++;
		s++;
	}

        if(res != NULL)
               freeaddrinfo(res);

	if(Debug && *socks != maxs)
		dbgprintf("We could initialize %d TCP listen sockets out of %d we received "
		 	"- this may or may not be an error indication.\n", *socks, maxs);

        if(*socks == 0) {
		errmsg.LogError(NO_ERRCODE, "No TCP listen socket could successfully be initialized, "
			 "message reception via TCP disabled.\n");
        	free(socks);
		return(NULL);
	}

	/* OK, we had success. Now it is also time to
	 * initialize our connections
	 */
	if(TCPSessTblInit(pThis) != 0) {
		/* OK, we are in some trouble - we could not initialize the
		 * session table, so we can not continue. We need to free all
		 * we have assigned so far, because we can not really use it...
		 */
		errmsg.LogError(NO_ERRCODE, "Could not initialize TCP session table, suspending TCP message reception.");
		freeAllSockets(&socks); /* prevent a socket leak */
		return(NULL);
	}

	return(socks);
}


/* Accept new TCP connection; make entry in session table. If there
 * is no more space left in the connection table, the new TCP
 * connection is immediately dropped.
 * ppSess has a pointer to the newly created session, if it succeds.
 * If it does not succeed, no session is created and ppSess is
 * undefined. If the user has provided an OnSessAccept Callback,
 * this one is executed immediately after creation of the 
 * session object, so that it can do its own initialization.
 * rgerhards, 2008-03-02
 */
static rsRetVal
SessAccept(tcpsrv_t *pThis, tcps_sess_t **ppSess, int fd)
{
	DEFiRet;
	tcps_sess_t *pSess = NULL;
	int newConn;
	int iSess = -1;
	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof(struct sockaddr_storage);
	uchar fromHost[NI_MAXHOST];
	uchar fromHostFQDN[NI_MAXHOST];

	ISOBJ_TYPE_assert(pThis, tcpsrv);

	newConn = accept(fd, (struct sockaddr*) &addr, &addrlen);
	if (newConn < 0) {
		errmsg.LogError(NO_ERRCODE, "tcp accept, ignoring error and connection request");
		ABORT_FINALIZE(RS_RET_ERR); // TODO: better error code
		//was: return -1;
	}

	/* Add to session list */
	iSess = TCPSessTblFindFreeSpot(pThis);
	if(iSess == -1) {
		errno = 0;
		errmsg.LogError(NO_ERRCODE, "too many tcp sessions - dropping incoming request");
		close(newConn);
		ABORT_FINALIZE(RS_RET_ERR); // TODO: better error code
		//was: return -1;
	} else {
		/* we found a free spot and can construct our session object */
		CHKiRet(tcps_sess.Construct(&pSess));
		CHKiRet(tcps_sess.SetTcpsrv(pSess, pThis));
	}

	/* OK, we have a "good" index... */
	/* get the host name */
	if(net.cvthname(&addr, fromHost, fromHostFQDN) != RS_RET_OK) {
		/* we seem to have something malicous - at least we
		 * are now told to discard the connection request.
		 * Error message has been generated by cvthname.
		 */
		close (newConn);
		ABORT_FINALIZE(RS_RET_ERR); // TODO: better error code
		//was: return -1;
	}

	/* Here we check if a host is permitted to send us
	 * syslog messages. If it isn't, we do not further
	 * process the message but log a warning (if we are
	 * configured to do this).
	 * rgerhards, 2005-09-26
	 */
	if(!pThis->pIsPermittedHost((struct sockaddr*) &addr, (char*) fromHostFQDN, pThis->pUsr, pSess->pUsr)) {
		dbgprintf("%s is not an allowed sender\n", (char *) fromHostFQDN);
		if(option_DisallowWarning) {
			errno = 0;
			errmsg.LogError(NO_ERRCODE, "TCP message from disallowed sender %s discarded",
				   (char*)fromHost);
		}
		close(newConn);
		ABORT_FINALIZE(RS_RET_HOST_NOT_PERMITTED);
	}

	/* OK, we have an allowed sender, so let's continue, what
	 * means we can finally fill in the session object.
	 */
	CHKiRet(tcps_sess.SetHost(pSess, fromHost));
	CHKiRet(tcps_sess.SetSock(pSess, newConn));
	CHKiRet(tcps_sess.SetMsgIdx(pSess, 0));
	CHKiRet(tcps_sess.ConstructFinalize(pSess));

	/* check if we need to call our callback */
	if(pThis->pOnSessAccept != NULL) {
		CHKiRet(pThis->pOnSessAccept(pThis, pSess));
	}

	*ppSess = pSess;
	pThis->pSessions[iSess] = pSess;
	pSess = NULL;

finalize_it:
	if(iRet != RS_RET_OK) {
		if(pSess != NULL) {
			tcps_sess.Destruct(&pSess);
		}
		iSess = -1; // TODO: change this to be fully iRet compliant ;)
	}

	RETiRet;
}


/* This function is called to gather input.
 */
static rsRetVal
Run(tcpsrv_t *pThis)
{
	DEFiRet;
	int maxfds;
	int nfds;
	int i;
	int iTCPSess;
	fd_set readfds;
	tcps_sess_t *pNewSess;

	ISOBJ_TYPE_assert(pThis, tcpsrv);

	/* this is an endless loop - it is terminated when the thread is
	 * signalled to do so. This, however, is handled by the framework,
	 * right into the sleep below.
	 */
	while(1) {
	        maxfds = 0;
	        FD_ZERO (&readfds);

		/* Add the TCP listen sockets to the list of read descriptors.
	    	 */
		if(pThis->pSocksLstn != NULL && *pThis->pSocksLstn) {
			for (i = 0; i < *pThis->pSocksLstn; i++) {
				/* The if() below is theoretically not needed, but I leave it in
				 * so that a socket may become unsuable during execution. That
				 * feature is not yet supported by the current code base.
				 */
				if (pThis->pSocksLstn[i+1] != -1) {
					if(Debug)
						net.debugListenInfo(pThis->pSocksLstn[i+1], "TCP");
					FD_SET(pThis->pSocksLstn[i+1], &readfds);
					if(pThis->pSocksLstn[i+1]>maxfds) maxfds=pThis->pSocksLstn[i+1];
				}
			}
			/* do the sessions */
			iTCPSess = TCPSessGetNxtSess(pThis, -1);
			while(iTCPSess != -1) {
				int fdSess;
				fdSess = pThis->pSessions[iTCPSess]->sock; // TODO: NOT CLEAN!, use method
				dbgprintf("Adding TCP Session %d\n", fdSess);
				FD_SET(fdSess, &readfds);
				if (fdSess>maxfds) maxfds=fdSess;
				/* now get next... */
				iTCPSess = TCPSessGetNxtSess(pThis, iTCPSess);
			}
		}

		if(Debug) {
			// TODO: name in dbgprintf!
			dbgprintf("--------<TCPSRV> calling select, active file descriptors (max %d): ", maxfds);
			for (nfds = 0; nfds <= maxfds; ++nfds)
				if ( FD_ISSET(nfds, &readfds) )
					dbgprintf("%d ", nfds);
			dbgprintf("\n");
		}

		/* wait for io to become ready */
		nfds = select(maxfds+1, (fd_set *) &readfds, NULL, NULL, NULL);

		for (i = 0; i < *pThis->pSocksLstn; i++) {
			if (FD_ISSET(pThis->pSocksLstn[i+1], &readfds)) {
				dbgprintf("New connect on TCP inetd socket: #%d\n", pThis->pSocksLstn[i+1]);
RUNLOG_VAR("%p", &pNewSess);
				SessAccept(pThis, &pNewSess, pThis->pSocksLstn[i+1]);
				--nfds; /* indicate we have processed one */
			}
		}

		/* now check the sessions */
		iTCPSess = TCPSessGetNxtSess(pThis, -1);
		while(nfds && iTCPSess != -1) {
			int fdSess;
			int state;
			fdSess = pThis->pSessions[iTCPSess]->sock; // TODO: not clean, use method
			if(FD_ISSET(fdSess, &readfds)) {
				char buf[MAXLINE];
				dbgprintf("tcp session socket with new data: #%d\n", fdSess);

				/* Receive message */
				state = pThis->pRcvData(pThis->pSessions[iTCPSess], buf, sizeof(buf));
				if(state == 0) {
					pThis->pOnRegularClose(pThis->pSessions[iTCPSess]);
					tcps_sess.Destruct(&pThis->pSessions[iTCPSess]);
				} else if(state == -1) {
					errmsg.LogError(NO_ERRCODE, "TCP session %d will be closed, error ignored\n", fdSess);
					pThis->pOnErrClose(pThis->pSessions[iTCPSess]);
					tcps_sess.Destruct(&pThis->pSessions[iTCPSess]);
				} else {
					/* valid data received, process it! */
					if(tcps_sess.DataRcvd(pThis->pSessions[iTCPSess], buf, state) != RS_RET_OK) {
						/* in this case, something went awfully wrong.
						 * We are instructed to terminate the session.
						 */
						errmsg.LogError(NO_ERRCODE, "Tearing down TCP Session %d - see "
							    "previous messages for reason(s)\n", iTCPSess);
						pThis->pOnErrClose(pThis->pSessions[iTCPSess]);
						tcps_sess.Destruct(&pThis->pSessions[iTCPSess]);
					}
				}
				--nfds; /* indicate we have processed one */
			}
			iTCPSess = TCPSessGetNxtSess(pThis, iTCPSess);
		}
	}

	RETiRet;
}


/* Standard-Constructor
 */
BEGINobjConstruct(tcpsrv) /* be sure to specify the object type also in END macro! */
	pThis->pSocksLstn = NULL;
	pThis->iSessMax = 200; /* TODO: useful default ;) */
ENDobjConstruct(tcpsrv)


/* ConstructionFinalizer
 */
static rsRetVal
tcpsrvConstructFinalize(tcpsrv_t __attribute__((unused)) *pThis)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, tcpsrv);
	pThis->pSocksLstn = pThis->OpenLstnSocks(pThis);

	RETiRet;
}


/* destructor for the tcpsrv object */
BEGINobjDestruct(tcpsrv) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(tcpsrv)
	if(pThis->OnDestruct != NULL)
		pThis->OnDestruct(pThis->pUsr);

	deinit_tcp_listener(pThis);
ENDobjDestruct(tcpsrv)


/* debugprint for the tcpsrv object */
BEGINobjDebugPrint(tcpsrv) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDebugPrint(tcpsrv)
ENDobjDebugPrint(tcpsrv)

/* set functions */
static rsRetVal
SetCBIsPermittedHost(tcpsrv_t *pThis, int (*pCB)(struct sockaddr *addr, char *fromHostFQDN, void*, void*))
{
	DEFiRet;
	pThis->pIsPermittedHost = pCB;
	RETiRet;
}

static rsRetVal
SetCBRcvData(tcpsrv_t *pThis, int (*pRcvData)(tcps_sess_t*, char*, size_t))
{
	DEFiRet;
	pThis->pRcvData = pRcvData;
	RETiRet;
}

static rsRetVal
SetCBOnListenDeinit(tcpsrv_t *pThis, int (*pCB)(void*))
{
	DEFiRet;
	pThis->pOnListenDeinit = pCB;
	RETiRet;
}

static rsRetVal
SetCBOnSessAccept(tcpsrv_t *pThis, rsRetVal (*pCB)(tcpsrv_t*, tcps_sess_t*))
{
	DEFiRet;
	pThis->pOnSessAccept = pCB;
	RETiRet;
}

static rsRetVal
SetCBOnDestruct(tcpsrv_t *pThis, rsRetVal (*pCB)(void*))
{
	DEFiRet;
	pThis->OnDestruct = pCB;
	RETiRet;
}

static rsRetVal
SetCBOnSessConstructFinalize(tcpsrv_t *pThis, rsRetVal (*pCB)(void*))
{
	DEFiRet;
	pThis->OnSessConstructFinalize = pCB;
	RETiRet;
}

static rsRetVal
SetCBOnSessDestruct(tcpsrv_t *pThis, rsRetVal (*pCB)(void*))
{
	DEFiRet;
	pThis->pOnSessDestruct = pCB;
	RETiRet;
}

static rsRetVal
SetCBOnRegularClose(tcpsrv_t *pThis, rsRetVal (*pCB)(tcps_sess_t*))
{
	DEFiRet;
	pThis->pOnRegularClose = pCB;
	RETiRet;
}

static rsRetVal
SetCBOnErrClose(tcpsrv_t *pThis, rsRetVal (*pCB)(tcps_sess_t*))
{
	DEFiRet;
	pThis->pOnErrClose = pCB;
	RETiRet;
}

static rsRetVal
SetCBOpenLstnSocks(tcpsrv_t *pThis, int* (*pCB)(tcpsrv_t*))
{
	DEFiRet;
	pThis->OpenLstnSocks = pCB;
	RETiRet;
}

static rsRetVal
SetUsrP(tcpsrv_t *pThis, void *pUsr)
{
	DEFiRet;
	pThis->pUsr = pUsr;
	RETiRet;
}


/* queryInterface function
 * rgerhards, 2008-02-29
 */
BEGINobjQueryInterface(tcpsrv)
CODESTARTobjQueryInterface(tcpsrv)
	if(pIf->ifVersion != tcpsrvCURR_IF_VERSION) { /* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->DebugPrint = tcpsrvDebugPrint;
	pIf->Construct = tcpsrvConstruct;
	pIf->ConstructFinalize = tcpsrvConstructFinalize;
	pIf->Destruct = tcpsrvDestruct;

	pIf->SessAccept = SessAccept;
	pIf->configureTCPListen = configureTCPListen;
	pIf->create_tcp_socket = create_tcp_socket;
	pIf->Run = Run;

	pIf->SetUsrP = SetUsrP;
	pIf->SetCBIsPermittedHost = SetCBIsPermittedHost;
	pIf->SetCBOpenLstnSocks = SetCBOpenLstnSocks;
	pIf->SetCBRcvData = SetCBRcvData;
	pIf->SetCBOnListenDeinit = SetCBOnListenDeinit;
	pIf->SetCBOnSessAccept = SetCBOnSessAccept;
	pIf->SetCBOnSessConstructFinalize = SetCBOnSessConstructFinalize;
	pIf->SetCBOnSessDestruct = SetCBOnSessDestruct;
	pIf->SetCBOnDestruct = SetCBOnDestruct;
	pIf->SetCBOnRegularClose = SetCBOnRegularClose;
	pIf->SetCBOnErrClose = SetCBOnErrClose;

finalize_it:
ENDobjQueryInterface(tcpsrv)


/* exit our class
 * rgerhards, 2008-03-10
 */
BEGINObjClassExit(tcpsrv, OBJ_IS_LOADABLE_MODULE) /* CHANGE class also in END MACRO! */
CODESTARTObjClassExit(tcpsrv)
	/* release objects we no longer need */
	objRelease(tcps_sess, DONT_LOAD_LIB);
	objRelease(conf, CORE_COMPONENT);
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(net, LM_NET_FILENAME);
ENDObjClassExit(tcpsrv)


/* Initialize our class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-29
 */
BEGINObjClassInit(tcpsrv, 1, OBJ_IS_LOADABLE_MODULE) /* class, version - CHANGE class also in END MACRO! */
	/* request objects we use */
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(net, LM_NET_FILENAME));
	CHKiRet(objUse(tcps_sess, DONT_LOAD_LIB));
	CHKiRet(objUse(conf, CORE_COMPONENT));

	/* set our own handlers */
	OBJSetMethodHandler(objMethod_DEBUGPRINT, tcpsrvDebugPrint);
	OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, tcpsrvConstructFinalize);
ENDObjClassInit(tcpsrv)


/* --------------- here now comes the plumbing that makes as a library module --------------- */


BEGINmodExit
CODESTARTmodExit
	/* de-init in reverse order! */
	tcpsrvClassExit();
	tcps_sessClassExit();
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_LIB_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */

	/* Initialize all classes that are in our module - this includes ourselfs */
	CHKiRet(tcps_sessClassInit(pModInfo));
	CHKiRet(tcpsrvClassInit(pModInfo)); /* must be done after tcps_sess, as we use it */
ENDmodInit

/* vim:set ai:
 */
