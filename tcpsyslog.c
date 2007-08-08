/* tcpsyslog.c
 * This is the implementation of TCP-based syslog.
 *
 * File begun on 2007-07-20 by RGerhards (extracted from syslogd.c)
 * This file is under development and has not yet arrived at being fully
 * self-contained and a real object. So far, it is mostly an excerpt
 * of the "old" message code without any modifications. However, it
 * helps to have things at the right place one we go to the meat of it.
 *
 * Copyright 2007 Rainer Gerhards and Adiscon GmbH.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */
#include "config.h"
#include "rsyslog.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef USE_PTHREADS
#include <pthread.h>
#endif

#include "syslogd.h"
#include "syslogd-types.h"
#include "net.h"
#include "tcpsyslog.h"
/********************************************************************
 *                    ###  SYSLOG/TCP CODE ###
 * This is code for syslog/tcp. This code would belong to a separate
 * file - but I have put it here to avoid hassle with CVS. Over
 * time, I expect rsyslog to utilize liblogging for actual network
 * I/O. So the tcp code will be (re)moved some time. I don't like
 * to add a new file to cvs that I will push to the attic in just
 * a few weeks (month at most...). So I simply add the code here.
 *
 * Place no unrelated code between this comment and the
 * END tcp comment!
 *
 * 2005-07-04 RGerhards (Happy independence day to our US friends!)
 ********************************************************************/
#ifdef SYSLOG_INET

#define TCPSESS_MAX_DEFAULT 200 /* default for nbr of tcp sessions if no number is given */

static int iTCPSessMax =  TCPSESS_MAX_DEFAULT;	/* actual number of sessions */
char *TCPLstnPort = "0"; /* read-only after startup */
int bEnableTCP = 0; /* read-only after startup */
int  *sockTCPLstn = NULL; /* read-only after startup, modified by restart */
struct TCPSession *pTCPSessions;
/* The thread-safeness of the sesion table is doubtful */


/* configure TCP listener settings. This is called during command
 * line parsing. The argument following -t is supplied as an argument.
 * The format of this argument is
 * "<port-to-use>, <nbr-of-sessions>"
 * Typically, there is no whitespace between port and session number.
 * (but it may be...).
 * NOTE: you can not use dbgprintf() in here - the dbgprintf() system is
 * not yet initilized when this function is called.
 * rgerhards, 2007-06-21
 * We can also not use logerror(), as that system is also not yet
 * initialized... rgerhards, 2007-06-28
 */
void configureTCPListen(char *cOptarg)
{
	register int i;
	register char *pArg = cOptarg;

	assert(cOptarg != NULL);
	bEnableTCP = -1;	/* enable TCP listening */

	/* extract port */
	i = 0;
	while(isdigit((int) *pArg)) {
		i = i * 10 + *pArg++ - '0';
	}

	if( i >= 0 && i <= 65535) {
		TCPLstnPort = cOptarg;
	} else {
		fprintf(stderr, "rsyslogd: Invalid TCP listen port %d - changed to 514.\n", i);
		TCPLstnPort = "514";
	}

	/* number of sessions */
	if(*pArg == ','){
		*pArg = '\0'; /* hack: terminates port (see a few lines above, same buffer!) */
		++pArg;
		while(isspace((int) *pArg))
			++pArg;
		/* ok, here should be the number... */
		i = 0;
		while(isdigit((int) *pArg)) {
			i = i * 10 + *pArg++ - '0';
		}
		if(i > 1)
			iTCPSessMax = i;
		else {
			/* too small, need to adjust */
			fprintf(stderr,
				"rsyslogd: TCP session max configured to %d [-t %s] - changing to 1.\n",
				i, cOptarg);
			iTCPSessMax = 1;
		}
	} else if(*pArg == '\0') {
		/* use default for session number - that's already set...*/
		/*EMPTY BY INTENSION*/
	} else {
		fprintf(stderr, "rsyslogd: Invalid -t %s command line option.\n", cOptarg);
	}
}


/* Initialize the session table
 * returns 0 if OK, somewhat else otherwise
 */
static int TCPSessInit(void)
{
	register int i;

	assert(pTCPSessions == NULL);
	dbgprintf("Allocating buffer for %d TCP sessions.\n", iTCPSessMax);
	if((pTCPSessions = (struct TCPSession *) malloc(sizeof(struct TCPSession) * iTCPSessMax))
	    == NULL) {
		dbgprintf("Error: TCPSessInit() could not alloc memory for TCP session table.\n");
		return(1);
	}

	for(i = 0 ; i < iTCPSessMax ; ++i) {
		pTCPSessions[i].sock = -1; /* no sock */
		pTCPSessions[i].iMsg = 0; /* just make sure... */
		pTCPSessions[i].bAtStrtOfFram = 1; /* indicate frame header expected */
		pTCPSessions[i].eFraming = TCP_FRAMING_OCTET_STUFFING; /* just make sure... */
	}
	return(0);
}


/* find a free spot in the session table. If the table
 * is full, -1 is returned, else the index of the free
 * entry (0 or higher).
 */
static int TCPSessFindFreeSpot(void)
{
	register int i;

	for(i = 0 ; i < iTCPSessMax ; ++i) {
		if(pTCPSessions[i].sock == -1)
			break;
	}

	return((i < iTCPSessMax) ? i : -1);
}


/* Get the next session index. Free session tables entries are
 * skipped. This function is provided the index of the last
 * session entry, or -1 if no previous entry was obtained. It
 * returns the index of the next session or -1, if there is no
 * further entry in the table. Please note that the initial call
 * might as well return -1, if there is no session at all in the
 * session table.
 */
int TCPSessGetNxtSess(int iCurr)
{
	register int i;

	for(i = iCurr + 1 ; i < iTCPSessMax ; ++i)
		if(pTCPSessions[i].sock != -1)
			break;

	return((i < iTCPSessMax) ? i : -1);
}


/* De-Initialize TCP listner sockets.
 * This function deinitializes everything, including freeing the
 * session table. No TCP listen receive operations are permitted
 * unless the subsystem is reinitialized.
 * rgerhards, 2007-06-21
 */
void deinit_tcp_listener(void)
{
	int iTCPSess;

	assert(pTCPSessions != NULL);
	/* close all TCP connections! */
	iTCPSess = TCPSessGetNxtSess(-1);
	while(iTCPSess != -1) {
		int fd;
		fd = pTCPSessions[iTCPSess].sock;
		dbgprintf("Closing TCP Session %d\n", fd);
		close(fd);
		/* now get next... */
		iTCPSess = TCPSessGetNxtSess(iTCPSess);
	}
	
	/* we are done with the session table - so get rid of it...
	*/
	free(pTCPSessions);
	pTCPSessions = NULL; /* just to make sure... */

	/* finally close the listen sockets themselfs */
	freeAllSockets(&sockTCPLstn);
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
int *create_tcp_socket(void)
{
        struct addrinfo hints, *res, *r;
        int error, maxs, *s, *socks, on = 1;

	if(!strcmp(TCPLstnPort, "0"))
		TCPLstnPort = "514";
		/* use default - we can not do service db update, because there is
		 * no IANA-assignment for syslog/tcp. In the long term, we might
		 * re-use RFC 3195 port of 601, but that would probably break to
		 * many existing configurations.
		 * rgerhards, 2007-06-28
		 */
        memset(&hints, 0, sizeof(hints));
        hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;
        hints.ai_family = family;
        hints.ai_socktype = SOCK_STREAM;

        error = getaddrinfo(NULL, TCPLstnPort, &hints, &res);
        if(error) {
               logerror((char*) gai_strerror(error));
	       return NULL;
	}

        /* Count max number of sockets we may open */
        for (maxs = 0, r = res; r != NULL ; r = r->ai_next, maxs++)
		/* EMPTY */;
        socks = malloc((maxs+1) * sizeof(int));
        if (socks == NULL) {
               logerror("couldn't allocate memory for TCP listen sockets, suspending TCP message reception.");
               freeaddrinfo(res);
               return NULL;
        }

        *socks = 0;   /* num of sockets counter at start of array */
        s = socks + 1;
	for (r = res; r != NULL ; r = r->ai_next) {
               *s = socket(r->ai_family, r->ai_socktype, r->ai_protocol);
        	if (*s < 0) {
			if(!(r->ai_family == PF_INET6 && errno == EAFNOSUPPORT))
				logerror("create_udp_socket(), socket");
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
			logerror("TCP setsockopt");
			close(*s);
			*s = -1;
			continue;
                	}
                }
#endif
       		if (setsockopt(*s, SOL_SOCKET, SO_REUSEADDR,
			       (char *) &on, sizeof(on)) < 0 ) {
			logerror("TCP setsockopt(REUSEADDR)");
                        close(*s);
			*s = -1;
			continue;
		}

		/* We need to enable BSD compatibility. Otherwise an attacker
		 * could flood our log files by sending us tons of ICMP errors.
		 */
#ifndef BSD	
		if (should_use_so_bsdcompat()) {
			if (setsockopt(*s, SOL_SOCKET, SO_BSDCOMPAT,
					(char *) &on, sizeof(on)) < 0) {
				logerror("TCP setsockopt(BSDCOMPAT)");
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
                        logerror("TCP bind");
                	close(*s);
			*s = -1;
                        continue;
                }

		if( listen(*s,iTCPSessMax / 10 + 5) < 0) {
			/* If the listen fails, it most probably fails because we ask
			 * for a too-large backlog. So in this case we first set back
			 * to a fixed, reasonable, limit that should work. Only if
			 * that fails, too, we give up.
			 */
			logerrorInt("listen with a backlog of %d failed - retrying with default of 32.",
				    iTCPSessMax / 10 + 5);
			if(listen(*s, 32) < 0) {
				logerror("TCP listen, suspending tcp inet");
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
		logerror("No TCP listen socket could successfully be initialized, "
			 "message reception via TCP disabled.\n");
        	free(socks);
		return(NULL);
	}

	/* OK, we had success. Now it is also time to
	 * initialize our connections
	 */
	if(TCPSessInit() != 0) {
		/* OK, we are in some trouble - we could not initialize the
		 * session table, so we can not continue. We need to free all
		 * we have assigned so far, because we can not really use it...
		 */
		logerror("Could not initialize TCP session table, suspending TCP message reception.");
		freeAllSockets(&socks); /* prevent a socket leak */
		return(NULL);
	}

	return(socks);
}


/* Accept new TCP connection; make entry in session table. If there
 * is no more space left in the connection table, the new TCP
 * connection is immediately dropped.
 */
void TCPSessAccept(int fd)
{
	int newConn;
	int iSess;
	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof(struct sockaddr_storage);
	size_t lenHostName;
	uchar fromHost[NI_MAXHOST];
	uchar fromHostFQDN[NI_MAXHOST];
	char *pBuf;

	newConn = accept(fd, (struct sockaddr*) &addr, &addrlen);
	if (newConn < 0) {
		logerror("tcp accept, ignoring error and connection request");
		return;
	}

	/* Add to session list */
	iSess = TCPSessFindFreeSpot();
	if(iSess == -1) {
		errno = 0;
		logerror("too many tcp sessions - dropping incoming request");
		close(newConn);
		return;
	}

	/* OK, we have a "good" index... */
	/* get the host name */
	if(cvthname(&addr, fromHost, fromHostFQDN) == 0) {
		/* we seem to have something malicous - at least we
		 * are now told to discard the connection request.
		 * Error message has been generated by cvthname.
		 */
		close (newConn);
		return;
	}

	/* Here we check if a host is permitted to send us
	 * syslog messages. If it isn't, we do not further
	 * process the message but log a warning (if we are
	 * configured to do this).
	 * rgerhards, 2005-09-26
	 */
	if(!isAllowedSender(pAllowedSenders_TCP, (struct sockaddr *)&addr, (char*)fromHostFQDN)) {
		if(option_DisallowWarning) {
			errno = 0;
			logerrorSz("TCP message from disallowed sender %s discarded",
				   (char*)fromHost);
		}
		close(newConn);
		return;
	}

	/* OK, we have an allowed sender, so let's continue */
	lenHostName = strlen((char*)fromHost) + 1; /* for \0 byte */
	if((pBuf = (char*) malloc(sizeof(char) * lenHostName)) == NULL) {
		glblHadMemShortage = 1;
		pTCPSessions[iSess].fromHost = "NO-MEMORY-FOR-HOSTNAME";
	} else {
		memcpy(pBuf, fromHost, lenHostName);
		pTCPSessions[iSess].fromHost = pBuf;
	}

	pTCPSessions[iSess].sock = newConn;
	pTCPSessions[iSess].iMsg = 0; /* init msg buffer! */
}


/* This should be called before a normal (non forced) close
 * of a TCP session. This function checks if there is any unprocessed
 * message left in the TCP stream. Such a message is probably a
 * fragement. If evrything goes well, we must be right at the
 * beginnig of a new frame without any data received from it. If
 * not, there is some kind of a framing error. I think I remember that
 * some legacy syslog/TCP implementations have non-LF terminated
 * messages at the end of the stream. For now, we allow this behaviour.
 * Later, it should probably become a configuration option.
 * rgerhards, 2006-12-07
 */
void TCPSessPrepareClose(int iTCPSess)
{
	if(iTCPSess < 0 || iTCPSess > iTCPSessMax) {
		errno = 0;
		logerror("internal error, trying to close an invalid TCP session!");
		return;
	}
	
	if(pTCPSessions[iTCPSess].bAtStrtOfFram == 1) {
		/* this is how it should be. There is no unprocessed
		 * data left and such we have nothing to do. For simplicity
		 * reasons, we immediately return in that case.
		 */
		 return;
	}

	/* we have some data left! */
	if(pTCPSessions[iTCPSess].eFraming == TCP_FRAMING_OCTET_COUNTING) {
		/* In this case, we have an invalid frame count and thus
		 * generate an error message and discard the frame.
		 */
		logerrorInt("Incomplete frame at end of stream in session %d - "
			    "ignoring extra data (a message may be lost).\n",
			    pTCPSessions[iTCPSess].sock);
		/* nothing more to do */
	} else { /* here, we have traditional framing. Missing LF at the end
		 * of message may occur. As such, we process the message in
		 * this case.
		 */
		dbgprintf("Extra data at end of stream in legacy syslog/tcp message - processing\n");
		printchopped(pTCPSessions[iTCPSess].fromHost, pTCPSessions[iTCPSess].msg,
			     pTCPSessions[iTCPSess].iMsg, pTCPSessions[iTCPSess].sock, 1);
		pTCPSessions[iTCPSess].bAtStrtOfFram = 1;
	}
}


/* Closes a TCP session and marks its slot in the session
 * table as unused. No attention is paid to the return code
 * of close, so potential-double closes are not detected.
 */
void TCPSessClose(int iSess)
{
	if(iSess < 0 || iSess > iTCPSessMax) {
		errno = 0;
		logerror("internal error, trying to close an invalid TCP session!");
		return;
	}

	close(pTCPSessions[iSess].sock);
	pTCPSessions[iSess].sock = -1;
	free(pTCPSessions[iSess].fromHost);
	pTCPSessions[iSess].fromHost = NULL; /* not really needed, but... */
}


/* Processes the data received via a TCP session. If there
 * is no other way to handle it, data is discarded.
 * Input parameter data is the data received, iLen is its
 * len as returned from recv(). iLen must be 1 or more (that
 * is errors must be handled by caller!). iTCPSess must be
 * the index of the TCP session that received the data.
 * rgerhards 2005-07-04
 * Changed this functions interface. We now return a status of
 * what shall happen with the session. This is information for
 * the caller. If 1 is returned, the session should remain open
 * and additional data be accepted. If we return 0, the TCP
 * session is to be closed by the caller. This functionality is
 * needed in order to support framing errors, from which there
 * is no recovery possible other than session termination and
 * re-establishment. The need for this functionality thus is
 * primarily rooted in support for -transport-tls I-D framing.
 * rgerhards, 2006-12-07
 */
int TCPSessDataRcvd(int iTCPSess, char *pData, int iLen)
{
	register int iMsg;
	char *pMsg;
	char *pEnd;
	assert(pData != NULL);
	assert(iLen > 0);
	assert(iTCPSess >= 0);
	assert(iTCPSess < iTCPSessMax);
	assert(pTCPSessions[iTCPSess].sock != -1);

	 /* We now copy the message to the session buffer. As
	  * it looks, we need to do this in any case because
	  * we might run into multiple messages inside a single
	  * buffer. Of course, we could think about optimizations,
	  * but as this code is to be replaced by liblogging, it
	  * probably doesn't make so much sense...
	  * rgerhards 2005-07-04
	  *
	  * Algo:
	  * - copy message to buffer until the first LF is found
	  * - printline() the buffer
	  * - continue with copying
	  */
	iMsg = pTCPSessions[iTCPSess].iMsg; /* copy for speed */
	pMsg = pTCPSessions[iTCPSess].msg; /* just a shortcut */
	pEnd = pData + iLen; /* this is one off, which is intensional */

	while(pData < pEnd) {
		/* Check if we are at a new frame */
		if(pTCPSessions[iTCPSess].bAtStrtOfFram) {
			/* we need to look at the message and detect
			 * the framing mode used
			 *//*
			 * Contrary to -transport-tls, we accept leading zeros in the message
			 * length. We do this in the spirit of "Be liberal in what you accept,
			 * and conservative in what you send". We expect that including leading
			 * zeros could be a common coding error.
			 * rgerhards, 2006-12-07
			 * The chairs of the IETF syslog-sec WG have announced that it is
			 * consensus to do the octet count on the SYSLOG-MSG part only. I am
			 * now changing the code to reflect this. Hopefully, it will not change
			 * once again (there can no compatibility layer programmed for this).
			 * To be on the save side, I just comment the code out. I mark these
			 * comments with "IETF20061218".
			 * rgerhards, 2006-12-19
			 */
			if(isdigit((int) *pData)) {
				int iCnt;	/* the frame count specified */
				pTCPSessions[iTCPSess].eFraming = TCP_FRAMING_OCTET_COUNTING;
				/* in this mode, we have OCTET-COUNT SP MSG - so we now need
				 * to extract the OCTET-COUNT and the SP and then extract
				 * the msg.
				 */
				iCnt = 0;
				/* IETF20061218 int iNbrOctets = 0; / * number of octets already consumed */
				while(isdigit((int) *pData)) {
					iCnt = iCnt * 10 + *pData - '0';
					/* IETF20061218 ++iNbrOctets; */
					++pData;
				}
				dbgprintf("TCP Message with octet-counter, size %d.\n", iCnt);
				if(*pData == ' ') {
					++pData;	/* skip over SP */
					/* IETF20061218 ++iNbrOctets; */
				} else {
					/* TODO: handle "invalid frame" case */
					logerrorInt("Framing Error in received TCP message: "
					            "delimiter is not SP but has ASCII value %d.\n",
						    *pData);
					return(0); /* unconditional error exit */
				}
				/* IETF20061218 pTCPSessions[iTCPSess].iOctetsRemain = iCnt - iNbrOctets; */
				pTCPSessions[iTCPSess].iOctetsRemain = iCnt;
				if(pTCPSessions[iTCPSess].iOctetsRemain < 1) {
					/* TODO: handle the case where the octet count is 0 or negative! */
					dbgprintf("Framing Error: invalid octet count\n");
					logerrorInt("Framing Error in received TCP message: "
					            "invalid octet count %d.\n",
				 		    pTCPSessions[iTCPSess].iOctetsRemain);
					return(0); /* unconditional error exit */
				}
			} else {
				pTCPSessions[iTCPSess].eFraming = TCP_FRAMING_OCTET_STUFFING;
				/* No need to do anything else here in this case */
			}
			pTCPSessions[iTCPSess].bAtStrtOfFram = 0; /* done frame header */
		}
	
		/* now copy message until end of record */

		if(iMsg >= MAXLINE) {
			/* emergency, we now need to flush, no matter if
			 * we are at end of message or not...
			 */
			printchopped(pTCPSessions[iTCPSess].fromHost, pMsg, iMsg,
			 	     pTCPSessions[iTCPSess].sock, 1);
			iMsg = 0;
			/* we might think if it is better to ignore the rest of the
		 	 * message than to treat it as a new one. Maybe this is a good
			 * candidate for a configuration parameter...
			 * rgerhards, 2006-12-04
			 */
		}

		if(*pData == '\n' &&
		   pTCPSessions[iTCPSess].eFraming == TCP_FRAMING_OCTET_STUFFING) { /* record delemiter? */
			printchopped(pTCPSessions[iTCPSess].fromHost, pMsg, iMsg,
				     pTCPSessions[iTCPSess].sock, 1);
			iMsg = 0;
			pTCPSessions[iTCPSess].bAtStrtOfFram = 1;
			++pData;
		} else {
			/* IMPORTANT: here we copy the actual frame content to the message! */
			*(pMsg + iMsg++) = *pData++;
		}

		if(pTCPSessions[iTCPSess].eFraming == TCP_FRAMING_OCTET_COUNTING) {
			/* do we need to find end-of-frame via octet counting? */
			pTCPSessions[iTCPSess].iOctetsRemain--;
			if(pTCPSessions[iTCPSess].iOctetsRemain < 1) {
				/* we have end of frame! */
				printchopped(pTCPSessions[iTCPSess].fromHost, pMsg, iMsg,
					     pTCPSessions[iTCPSess].sock, 1);
				iMsg = 0;
				pTCPSessions[iTCPSess].bAtStrtOfFram = 1;
			}
		}
	}

	pTCPSessions[iTCPSess].iMsg = iMsg; /* persist value */

	return(1);	/* successful return */
}


#endif
/********************************************************************
 *                  ###  END OF SYSLOG/TCP CODE ###
 ********************************************************************/


/*
 * vi:set ai:
 */
