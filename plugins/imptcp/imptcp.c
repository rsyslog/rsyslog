/* imptcp.c
 * This is a native implementation of plain tcp. It is intentionally
 * duplicate work (imtcp). The intent is to gain very fast and simple
 * native ptcp support, utilizing the best interfaces Linux (no cross-
 * platform intended!) has to offer.
 *
 * Note that in this module we try out some new naming conventions,
 * so it may look a bit "different" from the other modules. We are
 * investigating if removing prefixes can help make code more readable.
 *
 * File begun on 2010-08-10 by RGerhards
 *
 * Copyright 2007-2010 Rainer Gerhards and Adiscon GmbH.
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
#	error imptcp requires OS support for epoll - can not build
	/* imptcp gains speed by using modern Linux capabilities. As such,
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

/* static data */
DEF_IMOD_STATIC_DATA
DEFobjCurrIf(glbl)
DEFobjCurrIf(net)
DEFobjCurrIf(prop)
DEFobjCurrIf(datetime)
DEFobjCurrIf(errmsg)
DEFobjCurrIf(ruleset)



/* config settings */
typedef struct configSettings_s {
	int bEmitMsgOnClose;		/* emit an informational message on close by remote peer */
	int iAddtlFrameDelim;		/* addtl frame delimiter, e.g. for netscreen, default none */
	uchar *pszInputName;		/* value for inputname property, NULL is OK and handled by core engine */
	uchar *lstnIP;			/* which IP we should listen on? */
	ruleset_t *pRuleset;		/* ruleset to bind listener to (use system default if unspecified) */
} configSettings_t;

static configSettings_t cs;

/* data elements describing our running config */
typedef struct ptcpsrv_s ptcpsrv_t;
typedef struct ptcplstn_s ptcplstn_t;
typedef struct ptcpsess_s ptcpsess_t;
typedef struct epolld_s epolld_t;

/* the ptcp server (listener) object
 * Note that the object contains support for forming a linked list
 * of them. It does not make sense to do this seperately.
 */
struct ptcpsrv_s {
	ptcpsrv_t *pNext;		/* linked list maintenance */
	uchar *port;			/* Port to listen to */
	uchar *lstnIP;			/* which IP we should listen on? */
	int bEmitMsgOnClose;
	int iAddtlFrameDelim;
	uchar *pszInputName;
	prop_t *pInputName;		/* InputName in (fast to process) property format */
	ruleset_t *pRuleset;
	ptcplstn_t *pLstn;		/* root of our listeners */
	ptcpsess_t *pSess;		/* root of our sessions */
};

/* the ptcp session object. Describes a single active session.
 * includes support for doubly-linked list.
 */
struct ptcpsess_s {
	ptcpsrv_t *pSrv;	/* our server */
	ptcpsess_t *prev, *next;
	int sock;
	epolld_t *epd;
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


/* the ptcp listener object. Describes a single active listener.
 */
struct ptcplstn_s {
	ptcpsrv_t *pSrv;	/* our server */
	ptcplstn_t *prev, *next;
	int sock;
	epolld_t *epd;
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
//static permittedPeers_t *pPermPeersRoot = NULL;
static ptcpsrv_t *pSrvRoot = NULL;
static int epollfd = -1;			/* (sole) descriptor for epoll */
static int iMaxLine; /* maximum size of a single message */
/* we use a single static receive buffer, as this module is not multi-threaded. Keeping
 * the buffer in the data segment is probably a little bit more efficient than on the stack
 * (but at least I can't believe it will ever be less efficient ;) -- rgerhards, 2010-08-10
 * Note that we do NOT (yet?) provide a config setting to set the buffer size. For usual
 * syslog traffic, it should be large enough. Also keep in mind that we run under a virtual
 * memory system, so if we do not use large parts of the buffer, that's no issue at
 * all -- it'll just use up address space. On the other hand, it would be silly to page in
 * or page out some data just to get space for the IO buffer.
 */
static char rcvBuf[128*1024];

/* forward definitions */
static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal);
static rsRetVal addLstn(ptcpsrv_t *pSrv, int sock);


/* some simple constructors/destructors */
static void
destructSess(ptcpsess_t *pSess)
{
	free(pSess->pMsg);
	free(pSess->epd);
	prop.Destruct(&pSess->peerName);
	prop.Destruct(&pSess->peerIP);
	/* TODO: make these inits compile-time switch depending: */
	pSess->pMsg = NULL;
	pSess->epd = NULL;
	free(pSess);
}

static void
destructSrv(ptcpsrv_t *pSrv)
{
	prop.Destruct(&pSrv->pInputName);
	free(pSrv->port);
	free(pSrv);
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
startupSrv(ptcpsrv_t *pSrv)
{
	DEFiRet;
        int error, maxs, on = 1;
	int sock = -1;
	int numSocks;
	int sockflags;
        struct addrinfo hints, *res = NULL, *r;
	uchar *lstnIP;

	lstnIP = pSrv->lstnIP == NULL ? UCHAR_CONSTANT("") : pSrv->lstnIP;

	DBGPRINTF("imptcp creating listen socket on server '%s', port %s\n", lstnIP, pSrv->port);

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

		/* We use non-blocking IO! */
		if((sockflags = fcntl(sock, F_GETFL)) != -1) {
			sockflags |= O_NONBLOCK;
			/* SETFL could fail too, so get it caught by the subsequent
			 * error check.
			 */
			sockflags = fcntl(sock, F_SETFL, sockflags);
		}
		if(sockflags == -1) {
			DBGPRINTF("error %d setting fcntl(O_NONBLOCK) on tcp socket", errno);
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
                        DBGPRINTF("error %d while binding tcp socket", errno);
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
	int sockflags;
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

	/* set the new socket to non-blocking IO */
	if((sockflags = fcntl(iNewSock, F_GETFL)) != -1) {
		sockflags |= O_NONBLOCK;
		/* SETFL could fail too, so get it caught by the subsequent
		 * error check.
		 */
		sockflags = fcntl(iNewSock, F_SETFL, sockflags);
	}
	if(sockflags == -1) {
		DBGPRINTF("error %d setting fcntl(O_NONBLOCK) on tcp socket %d", errno, iNewSock);
		ABORT_FINALIZE(RS_RET_IO_ERROR);
	}

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
doSubmitMsg(ptcpsess_t *pThis, struct syslogTime *stTime, time_t ttGenTime, multi_submit_t *pMultiSub)
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
processDataRcvd(ptcpsess_t *pThis, char c, struct syslogTime *stTime, time_t ttGenTime, multi_submit_t *pMultiSub)
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
DataRcvd(ptcpsess_t *pThis, char *pData, size_t iLen)
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


/* add socket to the epoll set
 */
static inline rsRetVal
addEPollSock(epolld_type_t typ, void *ptr, int sock, epolld_t **pEpd)
{
	DEFiRet;
	epolld_t *epd = NULL;

	CHKmalloc(epd = malloc(sizeof(epolld_t)));
	epd->typ = typ;
	epd->ptr = ptr;
	*pEpd = epd;
	epd->ev.events = EPOLLIN|EPOLLET;
	epd->ev.data.ptr = (void*) epd;

	if(epoll_ctl(epollfd, EPOLL_CTL_ADD, sock, &(epd->ev)) != 0) {
		char errStr[1024];
		int eno = errno;
		errmsg.LogError(0, RS_RET_EPOLL_CTL_FAILED, "os error (%d) during epoll ADD: %s",
			        eno, rs_strerror_r(eno, errStr, sizeof(errStr)));
		ABORT_FINALIZE(RS_RET_EPOLL_CTL_FAILED);
	}

	DBGPRINTF("imptcp: added socket %d to epoll[%d] set\n", sock, epollfd);

finalize_it:
	if(iRet != RS_RET_OK) {
		free(epd);
	}
	RETiRet;
}


/* remove a socket from the epoll set. Note that the epd parameter
 * is not really required -- it is used to satisfy older kernels where
 * epoll_ctl() required a non-NULL pointer even though the ptr is never used.
 * For simplicity, we supply the same pointer we had when we created the
 * event (it's simple because we have it at hand).
 */
static inline rsRetVal
removeEPollSock(int sock, epolld_t *epd)
{
	DEFiRet;

	DBGPRINTF("imptcp: removing socket %d from epoll[%d] set\n", sock, epollfd);

	if(epoll_ctl(epollfd, EPOLL_CTL_DEL, sock, &(epd->ev)) != 0) {
		char errStr[1024];
		int eno = errno;
		errmsg.LogError(0, RS_RET_EPOLL_CTL_FAILED, "os error (%d) during epoll DEL: %s",
			        eno, rs_strerror_r(eno, errStr, sizeof(errStr)));
		ABORT_FINALIZE(RS_RET_EPOLL_CTL_FAILED);
	}

finalize_it:
	RETiRet;
}


/* add a listener to the server 
 */
static rsRetVal
addLstn(ptcpsrv_t *pSrv, int sock)
{
	DEFiRet;
	ptcplstn_t *pLstn;

	CHKmalloc(pLstn = malloc(sizeof(ptcplstn_t)));
	pLstn->pSrv = pSrv;
	pLstn->sock = sock;

	/* add to start of server's listener list */
	pLstn->prev = NULL;
	pLstn->next = pSrv->pLstn;
	if(pSrv->pLstn != NULL)
		pSrv->pLstn->prev = pLstn;
	pSrv->pLstn = pLstn;

	iRet = addEPollSock(epolld_lstn, pLstn, sock, &pLstn->epd);

finalize_it:
	RETiRet;
}


/* add a session to the server 
 */
static rsRetVal
addSess(ptcpsrv_t *pSrv, int sock, prop_t *peerName, prop_t *peerIP)
{
	DEFiRet;
	ptcpsess_t *pSess = NULL;

	CHKmalloc(pSess = malloc(sizeof(ptcpsess_t)));
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
	pSess->next = pSrv->pSess;
	if(pSrv->pSess != NULL)
		pSrv->pSess->prev = pSess;
	pSrv->pSess = pSess;

	iRet = addEPollSock(epolld_sess, pSess, sock, &pSess->epd);

finalize_it:
	RETiRet;
}


/* close/remove a session
 * NOTE: we must first remove the fd from the epoll set and then close it -- else we
 * get an error "bad file descriptor" from epoll.
 */
static rsRetVal
closeSess(ptcpsess_t *pSess)
{
	int sock;
	DEFiRet;
	
	sock = pSess->sock;
	CHKiRet(removeEPollSock(sock, pSess->epd));
	close(sock);

	/* finally unlink session from structures */
//fprintf(stderr, "closing session %d next %p, prev %p\n", pSess->sock, pSess->next, pSess->prev);
//DBGPRINTF("imptcp: pSess->next %p\n", pSess->next);
//DBGPRINTF("imptcp: pSess->prev %p\n", pSess->prev);
	if(pSess->next != NULL)
		pSess->next->prev = pSess->prev;
	if(pSess->prev == NULL) {
		/* need to update root! */
		pSess->pSrv->pSess = pSess->next;
	} else {
		pSess->prev->next = pSess->next;
	}

	/* unlinked, now remove structure */
	destructSess(pSess);

finalize_it:
	DBGPRINTF("imtcp: session on socket %d closed with iRet %d.\n", sock, iRet);
	RETiRet;
}


#if 0
/* set permitted peer -- rgerhards, 2008-05-19
 */
static rsRetVal
setPermittedPeer(void __attribute__((unused)) *pVal, uchar *pszID)
{
	DEFiRet;
	CHKiRet(net.AddPermittedPeer(&pPermPeersRoot, pszID));
	free(pszID); /* no longer needed, but we need to free as of interface def */
finalize_it:
	RETiRet;
}
#endif


/* accept a new ruleset to bind. Checks if it exists and complains, if not */
static rsRetVal setRuleset(void __attribute__((unused)) *pVal, uchar *pszName)
{
	ruleset_t *pRuleset;
	rsRetVal localRet;
	DEFiRet;

	localRet = ruleset.GetRuleset(&pRuleset, pszName);
	if(localRet == RS_RET_NOT_FOUND) {
		errmsg.LogError(0, NO_ERRCODE, "error: ruleset '%s' not found - ignored", pszName);
	}
	CHKiRet(localRet);
	cs.pRuleset = pRuleset;
	DBGPRINTF("imptcp current bind ruleset %p: '%s'\n", pRuleset, pszName);

finalize_it:
	free(pszName); /* no longer needed */
	RETiRet;
}


static rsRetVal addTCPListener(void __attribute__((unused)) *pVal, uchar *pNewVal)
{
	DEFiRet;
	ptcpsrv_t *pSrv;

	CHKmalloc(pSrv = malloc(sizeof(ptcpsrv_t)));
	pSrv->pSess = NULL;
	pSrv->pLstn = NULL;
	pSrv->bEmitMsgOnClose = cs.bEmitMsgOnClose;
	pSrv->port = pNewVal;
	pSrv->iAddtlFrameDelim = cs.iAddtlFrameDelim;
	cs.pszInputName = NULL;	/* moved over to pSrv, we do not own */
	pSrv->lstnIP = cs.lstnIP;
	cs.lstnIP = NULL;	/* moved over to pSrv, we do not own */
	pSrv->pRuleset = cs.pRuleset;
	pSrv->pszInputName = (cs.pszInputName == NULL) ?  UCHAR_CONSTANT("imptcp") : cs.pszInputName;
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


/* start up all listeners 
 * This is a one-time stop once the module is set to start.
 */
static inline rsRetVal
startupServers()
{
	DEFiRet;
	ptcpsrv_t *pSrv;

	pSrv = pSrvRoot;
	while(pSrv != NULL) {
		DBGPRINTF("Starting up ptcp server for port %s, name '%s'\n", pSrv->port, pSrv->pszInputName);
		startupSrv(pSrv);
		pSrv = pSrv->pNext;
	}

	RETiRet;
}


/* process new activity on listener. This means we need to accept a new
 * connection.
 */
static inline rsRetVal
lstnActivity(ptcplstn_t *pLstn)
{
	int newSock;
	prop_t *peerName;
	prop_t *peerIP;
	rsRetVal localRet;
	DEFiRet;

	DBGPRINTF("imptcp: new connection on listen socket %d\n", pLstn->sock);
	while(1) {
		localRet = AcceptConnReq(pLstn->sock, &newSock, &peerName, &peerIP);
		if(localRet == RS_RET_NO_MORE_DATA)
			break;
		CHKiRet(localRet);
		CHKiRet(addSess(pLstn->pSrv, newSock, peerName, peerIP));
	}

finalize_it:
	RETiRet;
}


/* process new activity on session. This means we need to accept data
 * or close the session.
 */
static inline rsRetVal
sessActivity(ptcpsess_t *pSess)
{
	int lenRcv;
	int lenBuf;
	DEFiRet;

	DBGPRINTF("imptcp: new activity on session socket %d\n", pSess->sock);

	while(1) {
		lenBuf = sizeof(rcvBuf);
		lenRcv = recv(pSess->sock, rcvBuf, lenBuf, 0);

		if(lenRcv > 0) {
			/* have data, process it */
			DBGPRINTF("imtcp: data(%d) on socket %d: %s\n", lenBuf, pSess->sock, rcvBuf);
			CHKiRet(DataRcvd(pSess, rcvBuf, lenRcv));
		} else if (lenRcv == 0) {
			/* session was closed, do clean-up */
			if(pSess->pSrv->bEmitMsgOnClose) {
				uchar *peerName;
				int lenPeer;
				prop.GetString(pSess->peerName, &peerName, &lenPeer);
				errmsg.LogError(0, RS_RET_PEER_CLOSED_CONN, "imptcp session %d closed by remote peer %s.\n",
						pSess->sock, peerName);
			}
			CHKiRet(closeSess(pSess));
			break;
		} else {
			if(errno == EAGAIN || errno == EWOULDBLOCK)
				break;
			DBGPRINTF("imtcp: error on session socket %d - closed.\n", pSess->sock);
			closeSess(pSess); /* try clean-up by dropping session */
			break;
		}
	}

finalize_it:
	RETiRet;
}


/* This function is called to gather input.
 */
BEGINrunInput
	int i;
	int nfds;
	struct epoll_event events[1];
	epolld_t *epd;
CODESTARTrunInput
	DBGPRINTF("imptcp now beginning to process input data\n");
	/* v5 TODO: consentual termination mode */
	while(1) {
		DBGPRINTF("imptcp going on epoll_wait\n");
		nfds = epoll_wait(epollfd, events, sizeof(events)/sizeof(struct epoll_event), -1);
		for(i = 0 ; i < nfds ; ++i) { /* support for larger batches (later, TODO) */
			epd = (epolld_t*) events[i].data.ptr;
			switch(epd->typ) {
			case epolld_lstn:
				lstnActivity((ptcplstn_t *) epd->ptr);
				break;
			case epolld_sess:
				sessActivity((ptcpsess_t *) epd->ptr);
				break;
			default:
				errmsg.LogError(0, RS_RET_INTERNAL_ERROR,
					"error: invalid epolld_type_t %d after epoll", epd->typ);
				break;
			}
		}
	}
ENDrunInput


/* initialize and return if will run or not */
BEGINwillRun
CODESTARTwillRun
	/* first apply some config settings */
	//net.PrintAllowedSenders(2); /* TCP */
	iMaxLine = glbl.GetMaxLine(); /* get maximum size we currently support */

	if(pSrvRoot == NULL) {
		errmsg.LogError(0, RS_RET_NO_LSTN_DEFINED, "error: no ptcp server defined, module can not run.");
		ABORT_FINALIZE(RS_RET_NO_RUN);
	}

#if defined(EPOLL_CLOEXEC) && defined(HAVE_EPOLL_CREATE1)
	DBGPRINTF("imptcp uses epoll_create1()\n");
	epollfd = epoll_create1(EPOLL_CLOEXEC);
	if(epollfd < 0 && errno == ENOSYS)
#endif
	{
		DBGPRINTF("imptcp uses epoll_create()\n");
		/* reading the docs, the number of epoll events passed to
		 * epoll_create() seems not to be used at all in kernels. So
		 * we just provide "a" number, happens to be 10.
		 */
		epollfd = epoll_create(10);
	}

	if(epollfd < 0) {
		errmsg.LogError(0, RS_RET_EPOLL_CR_FAILED, "error: epoll_create() failed");
		ABORT_FINALIZE(RS_RET_NO_RUN);
	}

	/* start up servers, but do not yet read input data */
	CHKiRet(startupServers());
	DBGPRINTF("imptcp started up, but not yet receiving data\n");
finalize_it:
ENDwillRun


/* completely shut down a server, that means closing all of its
 * listeners and sessions.
 */
static inline void
shutdownSrv(ptcpsrv_t *pSrv)
{
	ptcplstn_t *pLstn, *lstnDel;
	ptcpsess_t *pSess, *sessDel;

	/* listeners */
	pLstn = pSrv->pLstn;
	while(pLstn != NULL) {
		close(pLstn->sock);
		lstnDel = pLstn;
		pLstn = pLstn->next;
		DBGPRINTF("imptcp shutdown listen socket %d\n", lstnDel->sock);
		free(lstnDel->epd);
		free(lstnDel);
	}

	/* sessions */
	pSess = pSrv->pSess;
	while(pSess != NULL) {
		close(pSess->sock);
		sessDel = pSess;
		pSess = pSess->next;
		DBGPRINTF("imptcp shutdown session socket %d\n", sessDel->sock);
		destructSess(sessDel);
	}
}


BEGINafterRun
	ptcpsrv_t *pSrv, *srvDel;
CODESTARTafterRun
	/* do cleanup here */
	//net.clearAllowedSenders(UCHAR_CONSTANT("TCP"));
	/* we need to close everything that is still open */
	pSrv = pSrvRoot;
	while(pSrv != NULL) {
		srvDel = pSrv;
		pSrv = pSrv->pNext;
		shutdownSrv(srvDel);
		destructSrv(srvDel);
	}

	close(epollfd);
ENDafterRun


BEGINmodExit
CODESTARTmodExit
#if 0
	if(pPermPeersRoot != NULL) {
		net.DestructPermittedPeers(&pPermPeersRoot);
	}
#endif

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

	/* register config file handlers */
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("inputptcpserverrun"), 0, eCmdHdlrGetWord,
				   addTCPListener, NULL, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("inputptcpservernotifyonconnectionclose"), 0,
				   eCmdHdlrBinary, NULL, &cs.bEmitMsgOnClose, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("inputptcpserveraddtlframedelimiter"), 0, eCmdHdlrInt,
				   NULL, &cs.iAddtlFrameDelim, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("inputptcpserverinputname"), 0,
				   eCmdHdlrGetWord, NULL, &cs.pszInputName, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("inputptcpserverlistenip"), 0,
				   eCmdHdlrGetWord, NULL, &cs.lstnIP, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("inputptcpserverbindruleset"), 0,
				   eCmdHdlrGetWord, setRuleset, NULL, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("resetconfigvariables"), 1, eCmdHdlrCustomHandler,
		resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit


/* vim:set ai:
 */
