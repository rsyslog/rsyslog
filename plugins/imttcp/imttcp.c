/* imttcp.c
 * This is an experimental plain tcp input module which follows the
 * multiple thread paradigm.
 *
 * WARNING
 * This module is unfinished. It seems to work, but there also seems to be a problem
 * if it is under large stress (e.g. tcpflood with more than 500 to 1000 concurrent
 * connections). I quickly put together this module after I worked on a larger paper
 * and read [1], which claims that using massively threaded applications is
 * preferrable to an event driven approach. So I put this to test, especially as
 * that would also lead to a much simpler programming paradigm. Unfortuantely, the
 * performance results are devastive: while there is a very slight speedup with 
 * a low connection number (close to the number of cores on the system), there
 * is a dramatic negative speedup if running with many threads. Even at only 50
 * connections, rsyslog is dramatically slower (80 seconds for the same workload
 * which was processed in 60 seconds with traditional imtcp or when running on
 * a single connection). At 1,000 connections, the run was *extremely* slow. So
 * this is definitely a dead-end. To be honest, Behren, condit and Brewer claim
 * that the problem lies in the current implementation of thread libraries.
 * As one cure, they propose user-level threads. However, as far as I could
 * find out, User-Level threads seem not to be much faster under Linux than
 * Kernel-Level threads (which I used in my approach). 
 *
 * Even more convincing is, from the rsyslog PoV, that there are clear reasons
 * why the highly threaded input must be slower:
 * o batch sizes are smaller, leading to much more overhead
 * o many more context switches are needed to switch between the various
 *   i/o handlers
 * o more OS API calls are required because in this model we get more
 *   frequent wakeups on new incoming data, so we have less data available
 *   to read at each instant
 * o more lock contention because many more threads compete on the
 *   main queue mutex
 *
 * All in all, this means that the approach is not the right one, at least
 * not for rsyslog (it may work better if the input can be processed
 * totally independent, but I have note evaluated this). So I will look into
 * an enhanced event-based model with a small set of input workers pulling
 * off data (I assume this is useful for e.g. TLS, as TLS transport is much
 * more computebound than other inputs, and this computation becomes a
 * limiting factor for the overall processing speed under some
 * circumstances - see [2]).
 *
 * For obvious reasons, I will not try to finish imttcp. However, I have
 * decided to leave it included in the source tree, so that
 * a) someone else can build on it, if he sees value in that
 * b) I may use it for some other tests in the future
 *
 * But if you intend to actually use this module unmodified, be prepared
 * for problems.
 *
 * [1] R. Von Behren, J. Condit, and E. Brewer. Why events are a bad idea
 *     (for high-concurrency servers). In Proceedings of the 9th conference on Hot
 *     Topics in Operating Systems-Volume 9, page 4. USENIX Association, 2003.
 *
 * [2] http://kb.monitorware.com/tls-limited-17800-messages-per-second-t10598.html
 *
 * File begun on 2011-01-24 by RGerhards
 *
 * Copyright 2007-2011 Rainer Gerhards and Adiscon GmbH.
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
#if !defined(HAVE_EPOLL_CREATE)
#	error imttcp requires OS support for epoll - can not build
	/* imttcp gains speed by using modern Linux capabilities. As such,
	 * it can only be build on platforms supporting the epoll API.
	 */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <ctype.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include "rsyslog.h"
#include "cfsysline.h"
#include "prop.h"
#include "dirty.h"
#include "module-template.h"
#include "unicode-helper.h"
#include "glbl.h"
#include "prop.h"
#include "errmsg.h"
#include "srUtils.h"
#include "datetime.h"
#include "ruleset.h"
#include "msg.h"
#include "net.h" /* for permittedPeers, may be removed when this is removed */

/* the define is from tcpsrv.h, we need to find a new (but easier!!!) abstraction layer some time ... */
#define TCPSRV_NO_ADDTL_DELIMITER -1 /* specifies that no additional delimiter is to be used in TCP framing */


MODULE_TYPE_INPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("imttcp")

/* static data */
DEF_IMOD_STATIC_DATA
DEFobjCurrIf(glbl)
DEFobjCurrIf(net)
DEFobjCurrIf(prop)
DEFobjCurrIf(datetime)
DEFobjCurrIf(errmsg)
DEFobjCurrIf(ruleset)



/* config settings */
struct modConfData_s {
	EMPTY_STRUCT;
};

typedef struct configSettings_s {
	int bEmitMsgOnClose;		/* emit an informational message on close by remote peer */
	int iAddtlFrameDelim;		/* addtl frame delimiter, e.g. for netscreen, default none */
	uchar *pszInputName;		/* value for inputname property, NULL is OK and handled by core engine */
	uchar *lstnIP;			/* which IP we should listen on? */
	ruleset_t *pRuleset;		/* ruleset to bind listener to (use system default if unspecified) */
} configSettings_t;

static configSettings_t cs;

/* data elements describing our running config */
typedef struct ttcpsrv_s ttcpsrv_t;
typedef struct ttcplstn_s ttcplstn_t;
typedef struct ttcpsess_s ttcpsess_t;
typedef struct epolld_s epolld_t;

/* the ttcp server (listener) object
 * Note that the object contains support for forming a linked list
 * of them. It does not make sense to do this seperately.
 */
struct ttcpsrv_s {
	ttcpsrv_t *pNext;		/* linked list maintenance */
	uchar *port;			/* Port to listen to */
	uchar *lstnIP;			/* which IP we should listen on? */
	int bEmitMsgOnClose;
	int iAddtlFrameDelim;
	uchar *pszInputName;
	prop_t *pInputName;		/* InputName in (fast to process) property format */
	ruleset_t *pRuleset;
	ttcplstn_t *pLstn;		/* root of our listeners */
	ttcpsess_t *pSess;		/* root of our sessions */
	pthread_mutex_t mutSess;	/* mutex for session list updates */
};

/* the ttcp session object. Describes a single active session.
 * includes support for doubly-linked list.
 */
struct ttcpsess_s {
	ttcpsrv_t *pSrv;	/* our server */
	ttcpsess_t *prev, *next;
	int sock;
	pthread_t tid;
//--- from tcps_sess.h
	int iMsg;		 /* index of next char to store in msg */
	int bAtStrtOfFram;	/* are we at the very beginning of a new frame? */
	enum {
		eAtStrtFram,
		eInOctetCnt,
		eInMsg
	} inputState;		/* our current state */
	int iOctetsRemain;	/* Number of Octets remaining in message */
	TCPFRAMINGMODE eFraming;
	uchar *pMsg;		/* message (fragment) received */
	prop_t *peerName;	/* host name we received messages from */
	prop_t *peerIP;
//--- END from tcps_sess.h
};


/* the ttcp listener object. Describes a single active listener.
 */
struct ttcplstn_s {
	ttcpsrv_t *pSrv;	/* our server */
	ttcplstn_t *prev, *next;
	int sock;
	pthread_t tid;		/* ID of our listener thread */
};


/* type of object stored in epoll descriptor */
typedef enum {
	epolld_lstn,
	epolld_sess
} epolld_type_t;

/* an epoll descriptor. contains all information necessary to process
 * the result of epoll.
 */
struct epolld_s {
	epolld_type_t typ;
	void *ptr;
	struct epoll_event ev;
};


/* global data */
static ttcpsrv_t *pSrvRoot = NULL;
static int iMaxLine; /* maximum size of a single message */
pthread_attr_t sessThrdAttr;	/* Attribute for session threads; read only after startup */

/* forward definitions */
static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal);
static rsRetVal addLstn(ttcpsrv_t *pSrv, int sock);
static void * sessThrd(void *arg);


/* some simple constructors/destructors */
static void
destructSess(ttcpsess_t *pSess)
{
	free(pSess->pMsg);
	prop.Destruct(&pSess->peerName);
	prop.Destruct(&pSess->peerIP);
	/* TODO: make these inits compile-time switch depending: */
	pSess->pMsg = NULL;
	free(pSess);
}

static void
destructSrv(ttcpsrv_t *pSrv)
{
	prop.Destruct(&pSrv->pInputName);
	free(pSrv->port);
	free(pSrv);
}


/* common initialisation for new threads */
static inline void
initThrd(void)
{
	/* block all signals */
	sigset_t sigSet;
	sigfillset(&sigSet);
	pthread_sigmask(SIG_BLOCK, &sigSet, NULL);

	/* but ignore SIGTTN, which we (ab)use to signal the thread to shutdown -- rgerhards, 2009-07-20 */
	sigemptyset(&sigSet);
	sigaddset(&sigSet, SIGTTIN);
	pthread_sigmask(SIG_UNBLOCK, &sigSet, NULL);

}



/****************************************** TCP SUPPORT FUNCTIONS ***********************************/
/* We may later think about moving this into a helper library again. But the whole point
 * so far was to keep everything related close togehter. -- rgerhards, 2010-08-10
 */


/* Start up a server. That means all of its listeners are created.
 * Does NOT yet accept/process any incoming data (but binds ports). Hint: this
 * code is to be executed before dropping privileges.
 */
static rsRetVal
createSrv(ttcpsrv_t *pSrv)
{
	DEFiRet;
        int error, maxs, on = 1;
	int sock = -1;
	int numSocks;
        struct addrinfo hints, *res = NULL, *r;
	uchar *lstnIP;

	lstnIP = pSrv->lstnIP == NULL ? UCHAR_CONSTANT("") : pSrv->lstnIP;

	DBGPRINTF("imttcp creating listen socket on server '%s', port %s\n", lstnIP, pSrv->port);

        memset(&hints, 0, sizeof(hints));
        hints.ai_flags = AI_PASSIVE;
        hints.ai_family = glbl.GetDefPFFamily();
        hints.ai_socktype = SOCK_STREAM;

        error = getaddrinfo((char*)pSrv->lstnIP, (char*) pSrv->port, &hints, &res);
        if(error) {
		DBGPRINTF("error %d querying server '%s', port '%s'\n", error, pSrv->lstnIP, pSrv->port);
		ABORT_FINALIZE(RS_RET_INVALID_PORT);
	}

        /* Count max number of sockets we may open */
        for(maxs = 0, r = res; r != NULL ; r = r->ai_next, maxs++)
		/* EMPTY */;

        numSocks = 0;   /* num of sockets counter at start of array */
	for(r = res; r != NULL ; r = r->ai_next) {
               sock = socket(r->ai_family, r->ai_socktype, r->ai_protocol);
        	if(sock < 0) {
			if(!(r->ai_family == PF_INET6 && errno == EAFNOSUPPORT))
				DBGPRINTF("error %d creating tcp listen socket", errno);
				/* it is debatable if PF_INET with EAFNOSUPPORT should
				 * also be ignored...
				 */
                        continue;
                }

#ifdef IPV6_V6ONLY
                if(r->ai_family == AF_INET6) {
                	int iOn = 1;
			if(setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY,
			      (char *)&iOn, sizeof (iOn)) < 0) {
				close(sock);
				sock = -1;
				continue;
                	}
                }
#endif
       		if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &on, sizeof(on)) < 0 ) {
			DBGPRINTF("error %d setting tcp socket option\n", errno);
                        close(sock);
			sock = -1;
			continue;
		}

		/* We need to enable BSD compatibility. Otherwise an attacker
		 * could flood our log files by sending us tons of ICMP errors.
		 */
#ifndef BSD	
		if(net.should_use_so_bsdcompat()) {
			if (setsockopt(sock, SOL_SOCKET, SO_BSDCOMPAT,
					(char *) &on, sizeof(on)) < 0) {
				errmsg.LogError(errno, NO_ERRCODE, "TCP setsockopt(BSDCOMPAT)");
                                close(sock);
				sock = -1;
				continue;
			}
		}
#endif

	        if( (bind(sock, r->ai_addr, r->ai_addrlen) < 0)
#ifndef IPV6_V6ONLY
		     && (errno != EADDRINUSE)
#endif
	           ) {
			/* TODO: check if *we* bound the socket - else we *have* an error! */
			char errStr[1024];
			rs_strerror_r(errno, errStr, sizeof(errStr));
                        dbgprintf("error %d while binding tcp socket: %s\n", errno, errStr);
                	close(sock);
			sock = -1;
                        continue;
                }

		if(listen(sock, 511) < 0) {
			DBGPRINTF("tcp listen error %d, suspending\n", errno);
			close(sock);
			sock = -1;
			continue;
		}

		/* if we reach this point, we were able to obtain a valid socket, so we can
		 * create our listener object. -- rgerhards, 2010-08-10
		 */
		CHKiRet(addLstn(pSrv, sock));
		++numSocks;
	}

	if(numSocks != maxs)
		DBGPRINTF("We could initialize %d TCP listen sockets out of %d we received "
		 	  "- this may or may not be an error indication.\n", numSocks, maxs);

        if(numSocks == 0) {
		DBGPRINTF("No TCP listen sockets could successfully be initialized");
		ABORT_FINALIZE(RS_RET_COULD_NOT_BIND);
	}

finalize_it:
	if(res != NULL)
		freeaddrinfo(res);

	if(iRet != RS_RET_OK) {
		if(sock != -1)
			close(sock);
	}

	RETiRet;
}


/* Set pRemHost based on the address provided. This is to be called upon accept()ing
 * a connection request. It must be provided by the socket we received the
 * message on as well as a NI_MAXHOST size large character buffer for the FQDN.
 * Please see http://www.hmug.org/man/3/getnameinfo.php (under Caveats)
 * for some explanation of the code found below. If we detect a malicious
 * hostname, we return RS_RET_MALICIOUS_HNAME and let the caller decide
 * on how to deal with that.
 * rgerhards, 2008-03-31
 */
static rsRetVal
getPeerNames(prop_t **peerName, prop_t **peerIP, struct sockaddr *pAddr)
{
	int error;
	uchar szIP[NI_MAXHOST] = "";
	uchar szHname[NI_MAXHOST] = "";
	struct addrinfo hints, *res;
	
	DEFiRet;

        error = getnameinfo(pAddr, SALEN(pAddr), (char*)szIP, sizeof(szIP), NULL, 0, NI_NUMERICHOST);

        if(error) {
                DBGPRINTF("Malformed from address %s\n", gai_strerror(error));
		strcpy((char*)szHname, "???");
		strcpy((char*)szIP, "???");
		ABORT_FINALIZE(RS_RET_INVALID_HNAME);
	}

	if(!glbl.GetDisableDNS()) {
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
				DBGPRINTF("Malicious PTR record, IP = \"%s\" HOST = \"%s\"", szIP, szHname);
				iRet = RS_RET_MALICIOUS_HNAME;
			}
		} else {
			strcpy((char*)szHname, (char*)szIP);
		}
	} else {
		strcpy((char*)szHname, (char*)szIP);
	}

	/* We now have the names, so now let's allocate memory and store them permanently. */
	CHKiRet(prop.Construct(peerName));
	CHKiRet(prop.SetString(*peerName, szHname, ustrlen(szHname)));
	CHKiRet(prop.ConstructFinalize(*peerName));
	CHKiRet(prop.Construct(peerIP));
	CHKiRet(prop.SetString(*peerIP, szIP, ustrlen(szIP)));
	CHKiRet(prop.ConstructFinalize(*peerIP));

finalize_it:
	RETiRet;
}



/* accept an incoming connection request
 * rgerhards, 2008-04-22
 */
static rsRetVal
AcceptConnReq(int sock, int *newSock, prop_t **peerName, prop_t **peerIP)
{
	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof(addr);
	int iNewSock = -1;

	DEFiRet;

	iNewSock = accept(sock, (struct sockaddr*) &addr, &addrlen);
	if(iNewSock < 0) {
		if(errno == EAGAIN || errno == EWOULDBLOCK)
			ABORT_FINALIZE(RS_RET_NO_MORE_DATA);
		ABORT_FINALIZE(RS_RET_ACCEPT_ERR);
	}

	CHKiRet(getPeerNames(peerName, peerIP, (struct sockaddr*) &addr));

	*newSock = iNewSock;

finalize_it:
	if(iRet != RS_RET_OK) {
		/* the close may be redundant, but that doesn't hurt... */
		if(iNewSock != -1)
			close(iNewSock);
	}

	RETiRet;
}


/* This is a helper for submitting the message to the rsyslog core.
 * It does some common processing, including resetting the various
 * state variables to a "processed" state.
 * Note that this function is also called if we had a buffer overflow
 * due to a too-long message. So far, there is no indication this 
 * happened and it may be worth thinking about different handling
 * of this case (what obviously would require a change to this
 * function or some related code).
 * rgerhards, 2009-04-23
 * EXTRACT from tcps_sess.c
 */
static rsRetVal
doSubmitMsg(ttcpsess_t *pThis, struct syslogTime *stTime, time_t ttGenTime, multi_submit_t *pMultiSub)
{
	msg_t *pMsg;
	DEFiRet;

	if(pThis->iMsg == 0) {
		DBGPRINTF("discarding zero-sized message\n");
		FINALIZE;
	}

	/* we now create our own message object and submit it to the queue */
	CHKiRet(msgConstructWithTime(&pMsg, stTime, ttGenTime));
	MsgSetRawMsg(pMsg, (char*)pThis->pMsg, pThis->iMsg);
	MsgSetInputName(pMsg, pThis->pSrv->pInputName);
	MsgSetFlowControlType(pMsg, eFLOWCTL_LIGHT_DELAY);
	pMsg->msgFlags  = NEEDS_PARSING | PARSE_HOSTNAME;
	MsgSetRcvFrom(pMsg, pThis->peerName);
	CHKiRet(MsgSetRcvFromIP(pMsg, pThis->peerIP));
	MsgSetRuleset(pMsg, pThis->pSrv->pRuleset);

	if(pMultiSub == NULL) {
		CHKiRet(submitMsg(pMsg));
	} else {
		pMultiSub->ppMsgs[pMultiSub->nElem++] = pMsg;
		if(pMultiSub->nElem == pMultiSub->maxElem)
			CHKiRet(multiSubmitMsg(pMultiSub));
	}


finalize_it:
	/* reset status variables */
	pThis->bAtStrtOfFram = 1;
	pThis->iMsg = 0;

	RETiRet;
}


/* process the data received. As TCP is stream based, we need to process the
 * data inside a state machine. The actual data received is passed in byte-by-byte
 * from DataRcvd, and this function here compiles messages from them and submits
 * the end result to the queue. Introducing this function fixes a long-term bug ;)
 * rgerhards, 2008-03-14
 * EXTRACT from tcps_sess.c
 */
static inline rsRetVal
processDataRcvd(ttcpsess_t *pThis, char c, struct syslogTime *stTime, time_t ttGenTime, multi_submit_t *pMultiSub)
{
	DEFiRet;

	if(pThis->inputState == eAtStrtFram) {
		if(isdigit((int) c)) {
			pThis->inputState = eInOctetCnt;
			pThis->iOctetsRemain = 0;
			pThis->eFraming = TCP_FRAMING_OCTET_COUNTING;
		} else {
			pThis->inputState = eInMsg;
			pThis->eFraming = TCP_FRAMING_OCTET_STUFFING;
		}
	}

	if(pThis->inputState == eInOctetCnt) {
		if(isdigit(c)) {
			pThis->iOctetsRemain = pThis->iOctetsRemain * 10 + c - '0';
		} else { /* done with the octet count, so this must be the SP terminator */
			DBGPRINTF("TCP Message with octet-counter, size %d.\n", pThis->iOctetsRemain);
			if(c != ' ') {
				errmsg.LogError(0, NO_ERRCODE, "Framing Error in received TCP message: "
					    "delimiter is not SP but has ASCII value %d.\n", c);
			}
			if(pThis->iOctetsRemain < 1) {
				/* TODO: handle the case where the octet count is 0! */
				DBGPRINTF("Framing Error: invalid octet count\n");
				errmsg.LogError(0, NO_ERRCODE, "Framing Error in received TCP message: "
					    "invalid octet count %d.\n", pThis->iOctetsRemain);
			} else if(pThis->iOctetsRemain > iMaxLine) {
				/* while we can not do anything against it, we can at least log an indication
				 * that something went wrong) -- rgerhards, 2008-03-14
				 */
				DBGPRINTF("truncating message with %d octets - max msg size is %d\n",
					  pThis->iOctetsRemain, iMaxLine);
				errmsg.LogError(0, NO_ERRCODE, "received oversize message: size is %d bytes, "
					        "max msg size is %d, truncating...\n", pThis->iOctetsRemain, iMaxLine);
			}
			pThis->inputState = eInMsg;
		}
	} else {
		assert(pThis->inputState == eInMsg);
		if(pThis->iMsg >= iMaxLine) {
			/* emergency, we now need to flush, no matter if we are at end of message or not... */
			DBGPRINTF("error: message received is larger than max msg size, we split it\n");
			doSubmitMsg(pThis, stTime, ttGenTime, pMultiSub);
			/* we might think if it is better to ignore the rest of the
			 * message than to treat it as a new one. Maybe this is a good
			 * candidate for a configuration parameter...
			 * rgerhards, 2006-12-04
			 */
		}

		if((   (c == '\n')
		   || ((pThis->pSrv->iAddtlFrameDelim != TCPSRV_NO_ADDTL_DELIMITER) && (c == pThis->pSrv->iAddtlFrameDelim))
		   ) && pThis->eFraming == TCP_FRAMING_OCTET_STUFFING) { /* record delimiter? */
			doSubmitMsg(pThis, stTime, ttGenTime, pMultiSub);
			pThis->inputState = eAtStrtFram;
		} else {
			/* IMPORTANT: here we copy the actual frame content to the message - for BOTH framing modes!
			 * If we have a message that is larger than the max msg size, we truncate it. This is the best
			 * we can do in light of what the engine supports. -- rgerhards, 2008-03-14
			 */
			if(pThis->iMsg < iMaxLine) {
				*(pThis->pMsg + pThis->iMsg++) = c;
			}
		}

		if(pThis->eFraming == TCP_FRAMING_OCTET_COUNTING) {
			/* do we need to find end-of-frame via octet counting? */
			pThis->iOctetsRemain--;
			if(pThis->iOctetsRemain < 1) {
				/* we have end of frame! */
				doSubmitMsg(pThis, stTime, ttGenTime, pMultiSub);
				pThis->inputState = eAtStrtFram;
			}
		}
	}

	RETiRet;
}


/* Processes the data received via a TCP session. If there
 * is no other way to handle it, data is discarded.
 * Input parameter data is the data received, iLen is its
 * len as returned from recv(). iLen must be 1 or more (that
 * is errors must be handled by caller!). iTCPSess must be
 * the index of the TCP session that received the data.
 * rgerhards 2005-07-04
 * And another change while generalizing. We now return either
 * RS_RET_OK, which means the session should be kept open
 * or anything else, which means it must be closed.
 * rgerhards, 2008-03-01
 * As a performance optimization, we pick up the timestamp here. Acutally,
 * this *is* the *correct* reception step for all the data we received, because
 * we have just received a bunch of data! -- rgerhards, 2009-06-16
 * EXTRACT from tcps_sess.c
 */
#define NUM_MULTISUB 1024
static rsRetVal
DataRcvd(ttcpsess_t *pThis, char *pData, size_t iLen)
{
	multi_submit_t multiSub;
	msg_t *pMsgs[NUM_MULTISUB];
	struct syslogTime stTime;
	time_t ttGenTime;
	char *pEnd;
	DEFiRet;

	assert(pData != NULL);
	assert(iLen > 0);

	datetime.getCurrTime(&stTime, &ttGenTime);
	multiSub.ppMsgs = pMsgs;
	multiSub.maxElem = NUM_MULTISUB;
	multiSub.nElem = 0;

	 /* We now copy the message to the session buffer. */
	pEnd = pData + iLen; /* this is one off, which is intensional */

	while(pData < pEnd) {
		CHKiRet(processDataRcvd(pThis, *pData++, &stTime, ttGenTime, &multiSub));
	}

	if(multiSub.nElem > 0) {
		/* submit anything that was not yet submitted */
		CHKiRet(multiSubmitMsg(&multiSub));
	}

finalize_it:
	RETiRet;
}
#undef NUM_MULTISUB


/****************************************** --END-- TCP SUPPORT FUNCTIONS ***********************************/


static inline void
initConfigSettings(void)
{
	cs.bEmitMsgOnClose = 0;
	cs.iAddtlFrameDelim = TCPSRV_NO_ADDTL_DELIMITER;
	cs.pszInputName = NULL;
	cs.pRuleset = NULL;
	cs.lstnIP = NULL;
}


/* add a listener to the server 
 */
static rsRetVal
addLstn(ttcpsrv_t *pSrv, int sock)
{
	DEFiRet;
	ttcplstn_t *pLstn;

	CHKmalloc(pLstn = malloc(sizeof(ttcplstn_t)));
	pLstn->pSrv = pSrv;
	pLstn->sock = sock;

	/* add to start of server's listener list */
	pLstn->prev = NULL;
	pLstn->next = pSrv->pLstn;
	if(pSrv->pLstn != NULL)
		pSrv->pLstn->prev = pLstn;
	pSrv->pLstn = pLstn;

finalize_it:
	RETiRet;
}


/* add a session to the server 
 */
static rsRetVal
addSess(ttcpsrv_t *pSrv, int sock, prop_t *peerName, prop_t *peerIP)
{
	DEFiRet;
	ttcpsess_t *pSess = NULL;

	CHKmalloc(pSess = malloc(sizeof(ttcpsess_t)));
	CHKmalloc(pSess->pMsg = malloc(iMaxLine * sizeof(uchar)));
	pSess->pSrv = pSrv;
	pSess->sock = sock;
	pSess->inputState = eAtStrtFram;
	pSess->iMsg = 0;
	pSess->bAtStrtOfFram = 1;
	pSess->peerName = peerName;
	pSess->peerIP = peerIP;

	/* add to start of server's listener list */
	pSess->prev = NULL;
	pthread_mutex_lock(&pSrv->mutSess);
	pSess->next = pSrv->pSess;
	if(pSrv->pSess != NULL)
		pSrv->pSess->prev = pSess;
	pSrv->pSess = pSess;
	pthread_mutex_unlock(&pSrv->mutSess);

	/* finally run session handler */
	pthread_create(&pSess->tid, &sessThrdAttr, sessThrd, (void*) pSess);

finalize_it:
	RETiRet;
}


/* close/remove a session
 * NOTE: we must first remove the fd from the epoll set and then close it -- else we
 * get an error "bad file descriptor" from epoll.
 */
static inline rsRetVal
closeSess(ttcpsess_t *pSess)
{
	int sock;
	DEFiRet;
	
	sock = pSess->sock;
	close(sock);

	/* finally unlink session from structures */
	pthread_mutex_lock(&pSess->pSrv->mutSess);
	if(pSess->next != NULL)
		pSess->next->prev = pSess->prev;
	if(pSess->prev == NULL) {
		/* need to update root! */
		pSess->pSrv->pSess = pSess->next;
	} else {
		pSess->prev->next = pSess->next;
	}
	pthread_mutex_unlock(&pSess->pSrv->mutSess);

	/* unlinked, now remove structure */
	destructSess(pSess);

	RETiRet;
}


/* accept a new ruleset to bind. Checks if it exists and complains, if not */
static rsRetVal setRuleset(void __attribute__((unused)) *pVal, uchar *pszName)
{
	ruleset_t *pRuleset;
	rsRetVal localRet;
	DEFiRet;

	localRet = ruleset.GetRuleset(ourConf, &pRuleset, pszName);
	if(localRet == RS_RET_NOT_FOUND) {
		errmsg.LogError(0, NO_ERRCODE, "error: ruleset '%s' not found - ignored", pszName);
	}
	CHKiRet(localRet);
	cs.pRuleset = pRuleset;
	DBGPRINTF("imttcp current bind ruleset %p: '%s'\n", pRuleset, pszName);

finalize_it:
	free(pszName); /* no longer needed */
	RETiRet;
}


static rsRetVal addTCPListener(void __attribute__((unused)) *pVal, uchar *pNewVal)
{
	DEFiRet;
	ttcpsrv_t *pSrv;

	CHKmalloc(pSrv = malloc(sizeof(ttcpsrv_t)));
	pthread_mutex_init(&pSrv->mutSess, NULL);
	pSrv->pSess = NULL;
	pSrv->pLstn = NULL;
	pSrv->bEmitMsgOnClose = cs.bEmitMsgOnClose;
	pSrv->port = pNewVal;
	pSrv->iAddtlFrameDelim = cs.iAddtlFrameDelim;
	cs.pszInputName = NULL;	/* moved over to pSrv, we do not own */
	pSrv->lstnIP = cs.lstnIP;
	cs.lstnIP = NULL;	/* moved over to pSrv, we do not own */
	pSrv->pRuleset = cs.pRuleset;
	pSrv->pszInputName = (cs.pszInputName == NULL) ?  UCHAR_CONSTANT("imttcp") : cs.pszInputName;
	CHKiRet(prop.Construct(&pSrv->pInputName));
	CHKiRet(prop.SetString(pSrv->pInputName, pSrv->pszInputName, ustrlen(pSrv->pszInputName)));
	CHKiRet(prop.ConstructFinalize(pSrv->pInputName));

	/* add to linked list */
	pSrv->pNext = pSrvRoot;
	pSrvRoot = pSrv;

	/* all config vars are auto-reset -- this also is very useful with the
	 * new config format effort (v6).
	 */
	resetConfigVariables(NULL, NULL);

finalize_it:
	if(iRet != RS_RET_OK) {
		errmsg.LogError(0, NO_ERRCODE, "error %d trying to add listener", iRet);
	}
	RETiRet;
}


/* create up all listeners 
 * This is a one-time stop once the module is set to start.
 */
static inline rsRetVal
createServers()
{
	DEFiRet;
	ttcpsrv_t *pSrv;

	pSrv = pSrvRoot;
	while(pSrv != NULL) {
		DBGPRINTF("Starting up ttcp server for port %s, name '%s'\n", pSrv->port, pSrv->pszInputName);
		createSrv(pSrv);
		pSrv = pSrv->pNext;
	}

	RETiRet;
}


/* This function implements the thread to be used for listeners.
 * The function terminates if shutdown is required.
 */
static void *
lstnThrd(void *arg)
{
	ttcplstn_t *pLstn = (ttcplstn_t *) arg;
	rsRetVal iRet = RS_RET_OK;
	int newSock;
	prop_t *peerName;
	prop_t *peerIP;
	rsRetVal localRet;

	initThrd();
	
	while(glbl.GetGlobalInputTermState() == 0) {
		localRet = AcceptConnReq(pLstn->sock, &newSock, &peerName, &peerIP);
		if(glbl.GetGlobalInputTermState() == 1)
			break; /* terminate input! */
		CHKiRet(localRet);
		DBGPRINTF("imttcp: new connection %d on listen socket %d\n", newSock, pLstn->sock);
		CHKiRet(addSess(pLstn->pSrv, newSock, peerName, peerIP));
	}

finalize_it:
	close(pLstn->sock);
	DBGPRINTF("imttcp shutdown listen socket %d\n", pLstn->sock);
	/* Note: we do NOT unlink the deleted session. While this sounds not 100% clean,
	 * it is fine with the current implementation as we will never reuse these elements.
	 * However, it make sense (and not cost notable performance) to do it "right"...
	 */
	return NULL;
}


/* This function implements the thread to be used for a session
 * The function terminates if shutdown is required.
 */
static void *
sessThrd(void *arg)
{
	ttcpsess_t *pSess = (ttcpsess_t*) arg;
	rsRetVal iRet = RS_RET_OK;
	int lenRcv;
	int lenBuf;
	char rcvBuf[64*1024];
	
	initThrd();
	
	while(glbl.GetGlobalInputTermState() == 0) {
		lenBuf = sizeof(rcvBuf);
		lenRcv = recv(pSess->sock, rcvBuf, lenBuf, 0);

		if(glbl.GetGlobalInputTermState() == 1)
			ABORT_FINALIZE(RS_RET_FORCE_TERM);

		if(lenRcv > 0) {
			/* have data, process it */
			DBGPRINTF("imttcp: data(%d) on socket %d: %s\n", lenRcv, pSess->sock, rcvBuf);
			CHKiRet(DataRcvd(pSess, rcvBuf, lenRcv));
		} else if (lenRcv == 0) {
			/* session was closed, do clean-up */
			if(pSess->pSrv->bEmitMsgOnClose) {
				uchar *peerName;
				int lenPeer;
				prop.GetString(pSess->peerName, &peerName, &lenPeer);
				errmsg.LogError(0, RS_RET_PEER_CLOSED_CONN, "imttcp session %d closed by remote peer %s.\n",
						pSess->sock, peerName);
			}
			break;
		} else {
			if(errno == EAGAIN)
				break;
			DBGPRINTF("imttcp: error on session socket %d - closing.\n", pSess->sock);
			break;
		}
	}

finalize_it:
	DBGPRINTF("imttcp: session thread terminates, socket %d\n", pSess->sock);
	closeSess(pSess); /* try clean-up by dropping session */
	return NULL;
}

/* startup all listeners 
 */
static inline rsRetVal
startupListeners()
{
	DEFiRet;
	ttcpsrv_t *pSrv;
	ttcplstn_t *pLstn;

	pSrv = pSrvRoot;
	while(pSrv != NULL) {
		for(pLstn = pSrv->pLstn ; pLstn != NULL ; pLstn = pLstn->next) {
			pthread_create(&pLstn->tid, NULL, lstnThrd, (void*) pLstn);
		}
		pSrv = pSrv->pNext;
	}

	RETiRet;
}


/* This function is called to gather input.
 */
BEGINrunInput
	struct timeval tvSelectTimeout;
CODESTARTrunInput
	DBGPRINTF("imttcp: now beginning to process input data\n");
	CHKiRet(startupListeners());

	// TODO: this loop is a quick hack, do it right!
	while(glbl.GetGlobalInputTermState() == 0) {
		tvSelectTimeout.tv_sec =  86400 /*1 day*/;
		tvSelectTimeout.tv_usec = 0;
		select(1, NULL, NULL, NULL, &tvSelectTimeout);
	}
finalize_it: ;
ENDrunInput


/* initialize and return if will run or not */
BEGINwillRun
CODESTARTwillRun
	/* first apply some config settings */
	iMaxLine = glbl.GetMaxLine(); /* get maximum size we currently support */

	if(pSrvRoot == NULL) {
		errmsg.LogError(0, RS_RET_NO_LSTN_DEFINED, "error: no ttcp server defined, module can not run.");
		ABORT_FINALIZE(RS_RET_NO_RUN);
	}

	/* start up servers, but do not yet read input data */
	CHKiRet(createServers());
	DBGPRINTF("imttcp started up, but not yet receiving data\n");
finalize_it:
ENDwillRun


/* completely shut down a server. All we need to do is unblock the
 * various session and listerner threads as they then check the termination
 * praedicate themselves.
 */
static inline void
shutdownSrv(ttcpsrv_t *pSrv)
{
	ttcplstn_t *pLstn;
	ttcplstn_t *pLstnDel;
	ttcpsess_t *pSess;
	pthread_t tid;

	pLstn = pSrv->pLstn;
	while(pLstn != NULL) {
		tid = pLstn->tid; /* pSess will be destructed! */
		pthread_kill(tid, SIGTTIN);
		DBGPRINTF("imttcp: termination request for listen thread %x\n", (unsigned) tid);
		pthread_join(tid, NULL);
		DBGPRINTF("imttcp: listen thread %x terminated \n", (unsigned) tid);
		pLstnDel = pLstn;
		pLstn = pLstn->next;
		free(pLstnDel);
	}

	pSess = pSrv->pSess;
	while(pSess != NULL) {
		tid = pSess->tid; /* pSess will be destructed! */
		pSess = pSess->next;
		pthread_kill(tid, SIGTTIN);
		DBGPRINTF("imttcp: termination request for session thread %x\n", (unsigned) tid);
		//pthread_join(tid, NULL);
		DBGPRINTF("imttcp: session thread %x terminated \n", (unsigned) tid);
	}
}


BEGINafterRun
	ttcpsrv_t *pSrv, *srvDel;
CODESTARTafterRun
	/* do cleanup here */
	/* we need to close everything that is still open */
	pSrv = pSrvRoot;
	while(pSrv != NULL) {
		srvDel = pSrv;
		pSrv = pSrv->pNext;
		shutdownSrv(srvDel);
		destructSrv(srvDel);
	}
ENDafterRun


BEGINmodExit
CODESTARTmodExit
	pthread_attr_destroy(&sessThrdAttr);

	/* release objects we used */
	objRelease(glbl, CORE_COMPONENT);
	objRelease(prop, CORE_COMPONENT);
	objRelease(net, LM_NET_FILENAME);
	objRelease(datetime, CORE_COMPONENT);
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(ruleset, CORE_COMPONENT);
ENDmodExit


static rsRetVal
resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	cs.bEmitMsgOnClose = 0;
	cs.iAddtlFrameDelim = TCPSRV_NO_ADDTL_DELIMITER;
	free(cs.pszInputName);
	cs.pszInputName = NULL;
	free(cs.lstnIP);
	cs.lstnIP = NULL;
	return RS_RET_OK;
}



BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_IMOD_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	initConfigSettings();
	/* request objects we use */
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(prop, CORE_COMPONENT));
	CHKiRet(objUse(net, LM_NET_FILENAME));
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(datetime, CORE_COMPONENT));
	CHKiRet(objUse(ruleset, CORE_COMPONENT));

	/* initialize "read-only" thread attributes */
	pthread_attr_init(&sessThrdAttr);
	pthread_attr_setdetachstate(&sessThrdAttr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setstacksize(&sessThrdAttr, 4096*1024);

	/* register config file handlers */
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("inputttcpserverrun"), 0, eCmdHdlrGetWord,
				   addTCPListener, NULL, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("inputttcpservernotifyonconnectionclose"), 0,
				   eCmdHdlrBinary, NULL, &cs.bEmitMsgOnClose, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("inputttcpserveraddtlframedelimiter"), 0, eCmdHdlrInt,
				   NULL, &cs.iAddtlFrameDelim, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("inputttcpserverinputname"), 0,
				   eCmdHdlrGetWord, NULL, &cs.pszInputName, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("inputttcpserverlistenip"), 0,
				   eCmdHdlrGetWord, NULL, &cs.lstnIP, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("inputttcpserverbindruleset"), 0,
				   eCmdHdlrGetWord, setRuleset, NULL, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("resetconfigvariables"), 1, eCmdHdlrCustomHandler,
		resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit


/* vim:set ai:
 */
