/* omfwd.c
 * This is the implementation of the build-in forwarding output module.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
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
#ifdef SYSLOG_INET
#include "rsyslog.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fnmatch.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#ifdef USE_PTHREADS
#include <pthread.h>
#else
#include <fcntl.h>
#endif
#include "syslogd.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "net.h"
#include "omfwd.h"
#include "template.h"
#include "msg.h"
#include "tcpsyslog.h"
#include "module-template.h"

#ifdef SYSLOG_INET
//#define INET_SUSPEND_TIME 60		/* equal to 1 minute 
#define INET_SUSPEND_TIME 2		/* equal to 1 minute 
					 * rgerhards, 2005-07-26: This was 3 minutes. As the
					 * same timer is used for tcp based syslog, we have
					 * reduced it. However, it might actually be worth
					 * thinking about a buffered tcp sender, which would be 
					 * a much better alternative. When that happens, this
					 * time here can be re-adjusted to 3 minutes (or,
					 * even better, made configurable).
					 */
#define INET_RETRY_MAX 30		/* maximum of retries for gethostbyname() */
	/* was 10, changed to 30 because we reduced INET_SUSPEND_TIME by one third. So
	 * this "fixes" some of implications of it (see comment on INET_SUSPEND_TIME).
	 * rgerhards, 2005-07-26
	 */
#endif

/* internal structures
 */
DEF_OMOD_STATIC_DATA

typedef struct _instanceData {
	char	f_hname[MAXHOSTNAMELEN+1];
	short	sock;			/* file descriptor */
	enum { /* TODO: we shoud revisit these definitions */
		eDestFORW,
		eDestFORW_SUSP,
		eDestFORW_UNKN
	} eDestState;
	int iRtryCnt;
	struct addrinfo *f_addr;
	int compressionLevel; /* 0 - no compression, else level for zlib */
	char *port;
	int protocol;
	TCPFRAMINGMODE tcp_framing;
#	define	FORW_UDP 0
#	define	FORW_TCP 1
	/* following fields for TCP-based delivery */
	enum TCPSendStatus {
		TCP_SEND_NOTCONNECTED = 0,
		TCP_SEND_CONNECTING = 1,
		TCP_SEND_READY = 2
	} status;
	char *savedMsg;
	int savedMsgLen; /* length of savedMsg in octets */
	time_t	ttSuspend;	/* time selector was suspended */
#	ifdef USE_PTHREADS
	pthread_mutex_t mtxTCPSend;
#	endif
} instanceData;


BEGINcreateInstance
CODESTARTcreateInstance
ENDcreateInstance


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURERepeatedMsgReduction)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINfreeInstance
CODESTARTfreeInstance
	switch (pData->eDestState) {
		case eDestFORW:
		case eDestFORW_SUSP:
			freeaddrinfo(pData->f_addr);
			/* fall through */
		case eDestFORW_UNKN:
			if(pData->port != NULL)
				free(pData->port);
			break;
	}
#	ifdef USE_PTHREADS
	/* delete any mutex objects, if present */
	if(pData->protocol == FORW_TCP) {
		pthread_mutex_destroy(&pData->mtxTCPSend);
	}
#	endif
ENDfreeInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	printf("%s", pData->f_hname);
ENDdbgPrintInstInfo

/* CODE FOR SENDING TCP MESSAGES */

/* get send status
 * rgerhards, 2005-10-24
 */
static void TCPSendSetStatus(instanceData *pData, enum TCPSendStatus iNewState)
{
	assert(pData != NULL);
	assert(pData->protocol == FORW_TCP);
	assert(   (iNewState == TCP_SEND_NOTCONNECTED)
	       || (iNewState == TCP_SEND_CONNECTING)
	       || (iNewState == TCP_SEND_READY));

	/* there can potentially be a race condition, so guard by mutex */
#	ifdef	USE_PTHREADS
		pthread_mutex_lock(&pData->mtxTCPSend);
#	endif
	pData->status = iNewState;
#	ifdef	USE_PTHREADS
		pthread_mutex_unlock(&pData->mtxTCPSend);
#	endif
}


/* set send status
 * rgerhards, 2005-10-24
 */
static enum TCPSendStatus TCPSendGetStatus(instanceData *pData)
{
	enum TCPSendStatus eState;
	assert(pData != NULL);
	assert(pData->protocol == FORW_TCP);

	/* there can potentially be a race condition, so guard by mutex */
#	ifdef	USE_PTHREADS
		pthread_mutex_lock(&pData->mtxTCPSend);
#	endif
	eState = pData->status;
#	ifdef	USE_PTHREADS
		pthread_mutex_unlock(&pData->mtxTCPSend);
#	endif

	return eState;
}


/* Initialize TCP sockets (for sender)
 * This is done once per selector line, if not yet initialized.
 */
static int TCPSendCreateSocket(instanceData *pData, struct addrinfo *addrDest)
{
	int fd;
	struct addrinfo *r; 
	
	assert(pData != NULL);
	
	r = addrDest;

	while(r != NULL) {
		fd = socket(r->ai_family, r->ai_socktype, r->ai_protocol);
		if (fd != -1) {
			/* We can not allow the TCP sender to block syslogd, at least
			 * not in a single-threaded design. That would cause rsyslogd to
			 * loose input messages - which obviously also would affect
			 * other selector lines, too. So we do set it to non-blocking and 
			 * handle the situation ourselfs (by discarding messages). IF we run
			 * dual-threaded, however, the situation is different: in this case,
			 * the receivers and the selector line processing are only loosely
			 * coupled via a memory buffer. Now, I think, we can afford the extra
			 * wait time. Thus, we enable blocking mode for TCP if we compile with
			 * pthreads.
			 * rgerhards, 2005-10-25
			 */
#	ifndef USE_PTHREADS
			/* set to nonblocking - rgerhards 2005-07-20 */
			fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
#	endif		
			if (connect (fd, r->ai_addr, r->ai_addrlen) != 0) {
				if(errno == EINPROGRESS) {
					/* this is normal - will complete during select */
					TCPSendSetStatus(pData, TCP_SEND_CONNECTING);
					return fd;
				} else {
					char errStr[1024];
					dbgprintf("create tcp connection failed, reason %s",
						strerror_r(errno, errStr, sizeof(errStr)));
				}

			}
			else {
				TCPSendSetStatus(pData, TCP_SEND_READY);
				return fd;
			}
			close(fd);
		}
		else {
			char errStr[1024];
			dbgprintf("couldn't create send socket, reason %s", strerror_r(errno, errStr, sizeof(errStr)));
		}		
		r = r->ai_next;
	}

	dbgprintf("no working socket could be obtained");

	return -1;
}

/* Sends a TCP message. It is first checked if the
 * session is open and, if not, it is opened. Then the send
 * is tried. If it fails, one silent re-try is made. If the send
 * fails again, an error status (-1) is returned. If all goes well,
 * 0 is returned. The TCP session is NOT torn down.
 * For now, EAGAIN is ignored (causing message loss) - but it is
 * hard to do something intelligent in this case. With this
 * implementation here, we can not block and/or defer. Things are
 * probably a bit better when we move to liblogging. The alternative
 * would be to enhance the current select server with buffering and
 * write descriptors. This seems not justified, given the expected
 * short life span of this code (and the unlikeliness of this event).
 * rgerhards 2005-07-06
 * This function is now expected to stay. Libloging won't be used for
 * that purpose. I have added the param "len", because it is known by the
 * caller and so safes us some time. Also, it MUST be given because there
 * may be NULs inside msg so that we can not rely on strlen(). Please note
 * that the restrictions outlined above do not existin in multi-threaded
 * mode, which we assume will now be most often used. So there is no
 * real issue with the potential message loss in single-threaded builds.
 * rgerhards, 2006-11-30
 * 
 * In order to support compressed messages via TCP, we must support an
 * octet-counting based framing (LF may be part of the compressed message).
 * We are now supporting the same mode that is available in IETF I-D
 * syslog-transport-tls-05 (current at the time of this writing). This also
 * eases things when we go ahead and implement that framing. I have now made
 * available two cases where this framing is used: either by explitely
 * specifying it in the config file or implicitely when sending a compressed
 * message. In the later case, compressed and uncompressed messages within
 * the same session have different framings. If it is explicitely set to
 * octet-counting, only this framing mode is used within the session.
 * rgerhards, 2006-12-07
 */
static int TCPSend(instanceData *pData, char *msg, size_t len)
{
	int retry = 0;
	int done = 0;
	int bIsCompressed;
	int lenSend;
	char *buf = NULL;	/* if this is non-NULL, it MUST be freed before return! */
	enum TCPSendStatus eState;
	TCPFRAMINGMODE framingToUse;

	assert(pData != NULL);
	assert(msg != NULL);
	assert(len > 0);

	bIsCompressed = *msg == 'z';	/* cache this, so that we can modify the message buffer */
	/* select framing for this record. If we have a compressed record, we always need to
	 * use octet counting because the data potentially contains all control characters
	 * including LF.
	 */
	framingToUse = bIsCompressed ? TCP_FRAMING_OCTET_COUNTING : pData->tcp_framing;

	do { /* try to send message */
		if(pData->sock <= 0) {
			/* we need to open the socket first */
			if((pData->sock = TCPSendCreateSocket(pData, pData->f_addr)) <= 0) {
				return -1;
			}
		}

		eState = TCPSendGetStatus(pData); /* cache info */

		if(eState == TCP_SEND_CONNECTING) {
			/* In this case, we save the buffer. If we have a
			 * system with few messages, that hopefully prevents
			 * message loss at all. However, we make no further attempts,
			 * just the first message is saved. So we only try this
			 * if there is not yet a saved message present.
			 * rgerhards 2005-07-20
			 */
			if(pData->savedMsg == NULL) {
				pData->savedMsg = malloc(len * sizeof(char));
				if(pData->savedMsg == NULL)
					return 0; /* nothing we can do... */
				memcpy(pData->savedMsg, msg, len);
				pData->savedMsgLen = len;
			}
			return 0;
		} else if(eState != TCP_SEND_READY)
			/* This here is debatable. For the time being, we
			 * accept the loss of a single message (e.g. during
			 * connection setup in favour of not messing with
			 * wait time and timeouts. The reason is that such
			 * things might otherwise cost us considerable message
			 * loss on the receiving side (even at a timeout set
			 * to just 1 second).  - rgerhards 2005-07-20
			 */
			return 0;

		/* now check if we need to add a line terminator. We need to
		 * copy the string in memory in this case, this is probably
		 * quicker than using writev and definitely quicker than doing
		 * two socket calls.
		 * rgerhards 2005-07-22
		 *//*
		 * Some messages already contain a \n character at the end
		 * of the message. We append one only if we there is not
		 * already one. This seems the best fit, though this also
		 * means the message does not arrive unaltered at the final
		 * destination. But in the spirit of legacy syslog, this is
		 * probably the best to do...
		 * rgerhards 2005-07-20
		 */

		/* Build frame based on selected framing */
		if(framingToUse == TCP_FRAMING_OCTET_STUFFING) {
			if((*(msg+len-1) != '\n')) {
				if(buf != NULL)
					free(buf);
				/* in the malloc below, we need to add 2 to the length. The
				 * reason is that we a) add one character and b) len does
				 * not take care of the '\0' byte. Up until today, it was just
				 * +1 , which caused rsyslogd to sometimes dump core.
				 * I have added this comment so that the logic is not accidently
				 * changed again. rgerhards, 2005-10-25
				 */
				if((buf = malloc((len + 2) * sizeof(char))) == NULL) {
					/* extreme mem shortage, try to solve
					 * as good as we can. No point in calling
					 * any alarms, they might as well run out
					 * of memory (the risk is very high, so we
					 * do NOT risk that). If we have a message of
					 * more than 1 byte (what I guess), we simply
					 * overwrite the last character.
					 * rgerhards 2005-07-22
					 */
					if(len > 1) {
						*(msg+len-1) = '\n';
					} else {
						/* we simply can not do anything in
						 * this case (its an error anyhow...).
						 */
					}
				} else {
					/* we got memory, so we can copy the message */
					memcpy(buf, msg, len); /* do not copy '\0' */
					*(buf+len) = '\n';
					*(buf+len+1) = '\0';
					msg = buf; /* use new one */
					++len; /* care for the \n */
				}
			}
		} else {
			/* Octect-Counting
			 * In this case, we need to always allocate a buffer. This is because
			 * we need to put a header in front of the message text
			 */
			char szLenBuf[16];
			int iLenBuf;

			/* important: the printf-mask is "%d<sp>" because there must be a
			 * space after the len!
			 *//* The chairs of the IETF syslog-sec WG have announced that it is
			 * consensus to do the octet count on the SYSLOG-MSG part only. I am
			 * now changing the code to reflect this. Hopefully, it will not change
			 * once again (there can no compatibility layer programmed for this).
			 * To be on the save side, I just comment the code out. I mark these
			 * comments with "IETF20061218".
			 * rgerhards, 2006-12-19
			 */
			iLenBuf = snprintf(szLenBuf, sizeof(szLenBuf)/sizeof(char), "%d ", (int) len);
			/* IETF20061218 iLenBuf =
			  snprintf(szLenBuf, sizeof(szLenBuf)/sizeof(char), "%d ", len + iLenBuf);*/

			if((buf = malloc((len + iLenBuf) * sizeof(char))) == NULL) {
			 	/* we are out of memory. This is an extreme situation. We do not
				 * call any alarm handlers because they most likely run out of mem,
				 * too. We are brave enough to call debug output, though. Other than
				 * that, there is nothing left to do. We can not sent the message (as
				 * in case of the other framing, because the message is incomplete.
				 * We could, however, send two chunks (header and text separate), but
				 * that would cause a lot of complexity in the code. So we think it
				 * is appropriate enough to just make sure we do not crash in this
				 * very unlikely case. For this, it is justified just to loose
				 * the message. Rgerhards, 2006-12-07
				 */
				 dbgprintf("Error: out of memory when building TCP octet-counted "
				         "frame. Message is lost, trying to continue.\n");
				return 0;
			}

			 memcpy(buf, szLenBuf, iLenBuf); /* header */
			 memcpy(buf + iLenBuf, msg, len); /* message */
			 len += iLenBuf;	/* new message size */
			 msg = buf;	/* set message buffer */
		}

		/* frame building complete, on to actual sending */

		lenSend = send(pData->sock, msg, len, 0);
		dbgprintf("TCP sent %d bytes, requested %d, msg: '%s'\n", lenSend, len,
			bIsCompressed ? "***compressed***" : msg);
		if((unsigned)lenSend == len) {
			/* all well */
			if(buf != NULL) {
				free(buf);
			}
			return 0;
		} else if(lenSend != -1) {
			/* no real error, could "just" not send everything... 
			 * For the time being, we ignore this...
			 * rgerhards, 2005-10-25
			 */
			dbgprintf("message not completely (tcp)send, ignoring %d\n", lenSend);
#			if USE_PTHREADS
			usleep(1000); /* experimental - might be benefitial in this situation */
#			endif
			if(buf != NULL)
				free(buf);
			return 0;
		}

		switch(errno) {
		case EMSGSIZE:
			dbgprintf("message not (tcp)send, too large\n");
			/* This is not a real error, so it is not flagged as one */
			if(buf != NULL)
				free(buf);
			return 0;
			break;
		case EINPROGRESS:
		case EAGAIN:
			dbgprintf("message not (tcp)send, would block\n");
#			if USE_PTHREADS
			usleep(1000); /* experimental - might be benefitial in this situation */
#			endif
			/* we loose this message, but that's better than loosing
			 * all ;)
			 */
			/* This is not a real error, so it is not flagged as one */
			if(buf != NULL)
				free(buf);
			return 0;
			break;
		default:
			dbgprintf("message not (tcp)send");
			break;
		}
	
		if(retry == 0) {
			++retry;
			/* try to recover */
			close(pData->sock);
			TCPSendSetStatus(pData, TCP_SEND_NOTCONNECTED);
			pData->sock = -1;
		} else {
			if(buf != NULL)
				free(buf);
			return -1;
		}
	} while(!done); /* warning: do ... while() */
	/*NOT REACHED*/

	if(buf != NULL)
		free(buf);
	return -1; /* only to avoid compiler warning! */
}


/* get the syslog forward port from selector_t. The passed in
 * struct must be one that is setup for forwarding.
 * rgerhards, 2007-06-28
 * We may change the implementation to try to lookup the port
 * if it is unspecified. So far, we use the IANA default auf 514.
 */
static char *getFwdSyslogPt(instanceData *pData)
{
	assert(pData != NULL);
	if(pData->port == NULL)
		return("514");
	else
		return(pData->port);
}


/* try to resume connection if it is not ready
 * rgerhards, 2007-08-02
 */
static rsRetVal doTryResume(instanceData *pData)
{
	DEFiRet;
	struct addrinfo *res;
	struct addrinfo hints;
	unsigned e;

	switch (pData->eDestState) {
	case eDestFORW_SUSP:
		iRet = RS_RET_OK; /* the actual check happens during doAction() only */
		pData->eDestState = eDestFORW;
		break;
		
	case eDestFORW_UNKN:
		/* The remote address is not yet known and needs to be obtained */
		dbgprintf(" %s\n", pData->f_hname);
		memset(&hints, 0, sizeof(hints));
		/* port must be numeric, because config file syntax requests this */
		/* TODO: this code is a duplicate from cfline() - we should later create
		 * a common function.
		 */
		hints.ai_flags = AI_NUMERICSERV;
		hints.ai_family = family;
		hints.ai_socktype = pData->protocol == FORW_UDP ? SOCK_DGRAM : SOCK_STREAM;
		if((e = getaddrinfo(pData->f_hname,
				    getFwdSyslogPt(pData), &hints, &res)) == 0) {
			dbgprintf("%s found, resuming.\n", pData->f_hname);
			pData->f_addr = res;
			pData->iRtryCnt = 0;
			pData->eDestState = eDestFORW;
		} else {
			iRet = RS_RET_SUSPENDED;
		}
		break;
	case eDestFORW:
		/* rgerhards, 2007-09-11: this can not happen, but I've included it to
		 * a) make the compiler happy, b) detect any logic errors */
		assert(0);
		break;
	}

	return iRet;
}


BEGINtryResume
CODESTARTtryResume
	iRet = doTryResume(pData);
ENDtryResume

BEGINdoAction
	char *psz; /* temporary buffering */
	register unsigned l;
	struct addrinfo *r;
	int i;
	unsigned lsent = 0;
	int bSendSuccess;
CODESTARTdoAction
	switch (pData->eDestState) {
	case eDestFORW_SUSP:
		dbgprintf("internal error in omfwd.c, eDestFORW_SUSP in doAction()!\n");
		iRet = RS_RET_SUSPENDED;
		break;
		
	case eDestFORW_UNKN:
		dbgprintf("doAction eDestFORW_UNKN\n");
		iRet = doTryResume(pData);
		break;

	case eDestFORW:
		dbgprintf(" %s:%s/%s\n", pData->f_hname, getFwdSyslogPt(pData),
			 pData->protocol == FORW_UDP ? "udp" : "tcp");
		if ( 0) // TODO: think about this strcmp(getHOSTNAME(f->f_pMsg), LocalHostName) && NoHops )
		/* what we need to do is get the hostname as an additonal string (during parseSe..). Then,
		 * we can compare that string to LocalHostName. That way, we do not need to access the
		 * msgobject, and everything is clean. The question remains, though, if that functionality
		 * here actually makes sense or not. If we really need it, it might make more sense to compare
		 * the target IP address to the IP addresses of the local machene - that is a far better way of
		 * handling things than to relay on the error-prone hostname property.
		 * rgerhards, 2007-07-27
		 */
			dbgprintf("Not sending message to remote.\n");
		else {
			pData->ttSuspend = time(NULL);
			psz = (char*) ppString[0];
			l = strlen((char*) psz);
			if (l > MAXLINE)
				l = MAXLINE;

#			ifdef	USE_NETZIP
			/* Check if we should compress and, if so, do it. We also
			 * check if the message is large enough to justify compression.
			 * The smaller the message, the less likely is a gain in compression.
			 * To save CPU cycles, we do not try to compress very small messages.
			 * What "very small" means needs to be configured. Currently, it is
			 * hard-coded but this may be changed to a config parameter.
			 * rgerhards, 2006-11-30
			 */
			if(pData->compressionLevel && (l > MIN_SIZE_FOR_COMPRESS)) {
				Bytef out[MAXLINE+MAXLINE/100+12] = "z";
				uLongf destLen = sizeof(out) / sizeof(Bytef);
				uLong srcLen = l;
				int ret;
				ret = compress2((Bytef*) out+1, &destLen, (Bytef*) psz,
						srcLen, pData->compressionLevel);
				dbgprintf("Compressing message, length was %d now %d, return state  %d.\n",
					l, (int) destLen, ret);
				if(ret != Z_OK) {
					/* if we fail, we complain, but only in debug mode
					 * Otherwise, we are silent. In any case, we ignore the
					 * failed compression and just sent the uncompressed
					 * data, which is still valid. So this is probably the
					 * best course of action.
					 * rgerhards, 2006-11-30
					 */
					dbgprintf("Compression failed, sending uncompressed message\n");
				} else if(destLen+1 < l) {
					/* only use compression if there is a gain in using it! */
					dbgprintf("there is gain in compression, so we do it\n");
					psz = (char*) out;
					l = destLen + 1; /* take care for the "z" at message start! */
				}
				++destLen;
			}
#			endif

			if(pData->protocol == FORW_UDP) {
				/* forward via UDP */
	                        if(finet != NULL) {
					/* we need to track if we have success sending to the remote
					 * peer. Success is indicated by at least one sendto() call
					 * succeeding. We track this be bSendSuccess. We can not simply
					 * rely on lsent, as a call might initially work, but a later
					 * call fails. Then, lsent has the error status, even though
					 * the sendto() succeeded.
					 * rgerhards, 2007-06-22
					 */
					bSendSuccess = FALSE;
					for (r = pData->f_addr; r; r = r->ai_next) {
		                       		for (i = 0; i < *finet; i++) {
		                                       lsent = sendto(finet[i+1], psz, l, 0,
		                                                      r->ai_addr, r->ai_addrlen);
							if (lsent == l) {
						       		bSendSuccess = TRUE;
								break;
							} else {
								int eno = errno;
								char errStr[1024];
								dbgprintf("sendto() error: %d = %s.\n",
									eno, strerror_r(eno, errStr, sizeof(errStr)));
							}
		                                }
						if (lsent == l && !send_to_all)
	                         	               break;
					}
					/* finished looping */
	                                if (bSendSuccess == FALSE) {
		                                dbgprintf("error forwarding via udp, suspending\n");
						iRet = RS_RET_SUSPENDED;
					}
				}
			} else {
				/* forward via TCP */
				if(TCPSend(pData, psz, l) != 0) {
					/* error! */
					dbgprintf("error forwarding via tcp, suspending\n");
					iRet = RS_RET_SUSPENDED;
				}
			}
		}
		break;
	}
ENDdoAction


BEGINparseSelectorAct
	uchar *q;
	int i;
        int error;
	int bErr;
        struct addrinfo hints, *res;
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	if(*p == '@') {
		if((iRet = createInstance(&pData)) != RS_RET_OK)
			goto finalize_it;
		++p; /* eat '@' */
		if(*p == '@') { /* indicator for TCP! */
			pData->protocol = FORW_TCP;
			++p; /* eat this '@', too */
			/* in this case, we also need a mutex... */
#			ifdef USE_PTHREADS
			pthread_mutex_init(&pData->mtxTCPSend, 0);
#			endif
		} else {
			pData->protocol = FORW_UDP;
		}
		/* we are now after the protocol indicator. Now check if we should
		 * use compression. We begin to use a new option format for this:
		 * @(option,option)host:port
		 * The first option defined is "z[0..9]" where the digit indicates
		 * the compression level. If it is not given, 9 (best compression) is
		 * assumed. An example action statement might be:
		 * @@(z5,o)127.0.0.1:1400  
		 * Which means send via TCP with medium (5) compresion (z) to the local
		 * host on port 1400. The '0' option means that octet-couting (as in
		 * IETF I-D syslog-transport-tls) is to be used for framing (this option
		 * applies to TCP-based syslog only and is ignored when specified with UDP).
		 * That is not yet implemented.
		 * rgerhards, 2006-12-07
		 */
		if(*p == '(') {
			/* at this position, it *must* be an option indicator */
			do {
				++p; /* eat '(' or ',' (depending on when called) */
				/* check options */
				if(*p == 'z') { /* compression */
#					ifdef USE_NETZIP
					++p; /* eat */
					if(isdigit((int) *p)) {
						int iLevel;
						iLevel = *p - '0';
						++p; /* eat */
						pData->compressionLevel = iLevel;
					} else {
						logerrorInt("Invalid compression level '%c' specified in "
						         "forwardig action - NOT turning on compression.",
							 *p);
					}
#					else
					logerror("Compression requested, but rsyslogd is not compiled "
					         "with compression support - request ignored.");
#					endif /* #ifdef USE_NETZIP */
				} else if(*p == 'o') { /* octet-couting based TCP framing? */
					++p; /* eat */
					/* no further options settable */
					pData->tcp_framing = TCP_FRAMING_OCTET_COUNTING;
				} else { /* invalid option! Just skip it... */
					logerrorInt("Invalid option %c in forwarding action - ignoring.", *p);
					++p; /* eat invalid option */
				}
				/* the option processing is done. We now do a generic skip
				 * to either the next option or the end of the option
				 * block.
				 */
				while(*p && *p != ')' && *p != ',')
					++p;	/* just skip it */
			} while(*p && *p == ','); /* Attention: do.. while() */
			if(*p == ')')
				++p; /* eat terminator, on to next */
			else
				/* we probably have end of string - leave it for the rest
				 * of the code to handle it (but warn the user)
				 */
				logerror("Option block not terminated in forwarding action.");
		}
		/* extract the host first (we do a trick - we replace the ';' or ':' with a '\0')
		 * now skip to port and then template name. rgerhards 2005-07-06
		 */
		for(q = p ; *p && *p != ';' && *p != ':' ; ++p)
		 	/* JUST SKIP */;

		pData->port = NULL;
		if(*p == ':') { /* process port */
			uchar * tmp;

			*p = '\0'; /* trick to obtain hostname (later)! */
			tmp = ++p;
			for(i=0 ; *p && isdigit((int) *p) ; ++p, ++i)
				/* SKIP AND COUNT */;
			pData->port = malloc(i + 1);
			if(pData->port == NULL) {
				logerror("Could not get memory to store syslog forwarding port, "
					 "using default port, results may not be what you intend\n");
				/* we leave f_forw.port set to NULL, this is then handled by
				 * getFwdSyslogPt().
				 */
			} else {
				memcpy(pData->port, tmp, i);
				*(pData->port + i) = '\0';
			}
		}
		
		/* now skip to template */
		bErr = 0;
		while(*p && *p != ';') {
			if(*p && *p != ';' && !isspace((int) *p)) {
				if(bErr == 0) { /* only 1 error msg! */
					bErr = 1;
					errno = 0;
					logerror("invalid selector line (port), probably not doing "
					         "what was intended");
				}
			}
			++p;
		}
	
		/* TODO: make this if go away! */
		if(*p == ';') {
			*p = '\0'; /* trick to obtain hostname (later)! */
			strcpy(pData->f_hname, (char*) q);
			*p = ';';
		} else
			strcpy(pData->f_hname, (char*) q);

		/* process template */
		if((iRet = cflineParseTemplateName(&p, *ppOMSR, 0, OMSR_NO_RQD_TPL_OPTS, (uchar*) " StdFwdFmt"))
		   != RS_RET_OK)
			goto finalize_it;

		/* first set the pData->eDestState */
		memset(&hints, 0, sizeof(hints));
		/* port must be numeric, because config file syntax requests this */
		hints.ai_flags = AI_NUMERICSERV;
		hints.ai_family = family;
		hints.ai_socktype = pData->protocol == FORW_UDP ? SOCK_DGRAM : SOCK_STREAM;
		if( (error = getaddrinfo(pData->f_hname, getFwdSyslogPt(pData), &hints, &res)) != 0) {
			pData->eDestState = eDestFORW_UNKN;
			pData->iRtryCnt = INET_RETRY_MAX;
			pData->ttSuspend = time(NULL);
		} else {
			pData->eDestState = eDestFORW;
			pData->f_addr = res;
		}

		/*
		 * Otherwise the host might be unknown due to an
		 * inaccessible nameserver (perhaps on the same
		 * host). We try to get the ip number later, like
		 * FORW_SUSP.
		 */
	} else {
		iRet = RS_RET_CONFLINE_UNPROCESSED;
	}

	/* TODO: do we need to call freeInstance if we failed - this is a general question for
	 * all output modules. I'll address it lates as the interface evolves. rgerhards, 2007-07-25
	 */
CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINneedUDPSocket
CODESTARTneedUDPSocket
	iRet = RS_RET_TRUE;
ENDneedUDPSocket


BEGINonSelectReadyWrite
CODESTARTonSelectReadyWrite
	dbgprintf("tcp send socket %d ready for writing.\n", pData->sock);
	TCPSendSetStatus(pData, TCP_SEND_READY);
	/* Send stored message (if any) */
	if(pData->savedMsg != NULL) {
		if(TCPSend(pData, pData->savedMsg,
			   pData->savedMsgLen) != 0) {
			/* error! */
			pData->eDestState = eDestFORW_SUSP;
			errno = 0;
			logerror("error forwarding via tcp, suspending...");
		}
		free(pData->savedMsg);
		pData->savedMsg = NULL;
	}
ENDonSelectReadyWrite


BEGINgetWriteFDForSelect
CODESTARTgetWriteFDForSelect
	if(   (pData->eDestState == eDestFORW)
	   && (pData->protocol == FORW_TCP)
	   && TCPSendGetStatus(pData) == TCP_SEND_CONNECTING) {
		*fd = pData->sock;
		iRet = RS_RET_OK;
	}
ENDgetWriteFDForSelect


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
ENDqueryEtryPt


BEGINmodInit(Fwd)
CODESTARTmodInit
	*ipIFVersProvided = 1; /* so far, we only support the initial definition */
CODEmodInit_QueryRegCFSLineHdlr
ENDmodInit

#endif /* #ifdef SYSLOG_INET */
/*
 * vi:set ai:
 */
