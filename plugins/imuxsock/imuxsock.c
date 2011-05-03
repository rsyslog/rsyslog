/* imuxsock.c
 * This is the implementation of the Unix sockets input module.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * File begun on 2007-12-20 by RGerhards (extracted from syslogd.c)
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
#include "rsyslog.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/socket.h>
#include "dirty.h"
#include "cfsysline.h"
#include "unicode-helper.h"
#include "module-template.h"
#include "srUtils.h"
#include "errmsg.h"
#include "net.h"
#include "glbl.h"
#include "msg.h"
#include "parser.h"
#include "prop.h"
#include "debug.h"
#include "unlimited_select.h"
#include "sd-daemon.h"
#include "statsobj.h"
#include "datetime.h"
#include "hashtable.h"

MODULE_TYPE_INPUT
MODULE_TYPE_NOKEEP

/* defines */
#define MAXFUNIX	50
#ifndef _PATH_LOG
#ifdef BSD
#define _PATH_LOG	"/var/run/log"
#else
#define _PATH_LOG	"/dev/log"
#endif
#endif

/* emulate struct ucred for platforms that do not have it */
#ifndef HAVE_SCM_CREDENTIALS
struct ucred { int pid; };
#endif

/* handle some defines missing on more than one platform */
#ifndef SUN_LEN
#define SUN_LEN(su) \
   (sizeof(*(su)) - sizeof((su)->sun_path) + strlen((su)->sun_path))
#endif
/* Module static data */
DEF_IMOD_STATIC_DATA
DEFobjCurrIf(errmsg)
DEFobjCurrIf(glbl)
DEFobjCurrIf(prop)
DEFobjCurrIf(parser)
DEFobjCurrIf(datetime)
DEFobjCurrIf(statsobj)

statsobj_t *modStats;
STATSCOUNTER_DEF(ctrSubmit, mutCtrSubmit)
STATSCOUNTER_DEF(ctrLostRatelimit, mutCtrLostRatelimit)
STATSCOUNTER_DEF(ctrNumRatelimiters, mutCtrNumRatelimiters)

struct rs_ratelimit_state {
	unsigned short interval;
	unsigned short burst;
	unsigned done;
	unsigned missed;
	time_t begin;
};
typedef struct rs_ratelimit_state rs_ratelimit_state_t;


/* a very simple "hash function" for process IDs - we simply use the
 * pid itself: it is quite expected that all pids may log some time, but
 * from a collision point of view it is likely that long-running daemons 
 * start early and so will stay right in the top spots of the
 * collision list.
 */
static unsigned int
hash_from_key_fn(void *k)
{
	return((unsigned) *((pid_t*) k));
}

static int
key_equals_fn(void *key1, void *key2)
{
	return *((pid_t*) key1) == *((pid_t*) key2);
}


/* structure to describe a specific listener */
typedef struct lstn_s {
	uchar *sockName;	/* read-only after startup */
	prop_t *hostName;	/* host-name override - if set, use this instead of actual name */
	int fd;			/* read-only after startup */
	int flags;		/* should parser parse host name?  read-only after startup */
	int flowCtl;		/* flow control settings for this socket */
	int ratelimitInterval;
	int ratelimitBurst;
	intTiny ratelimitSev;	/* severity level (and below) for which rate-limiting shall apply */
	struct hashtable *ht;	/* our hashtable for rate-limiting */
	sbool bParseHost;	/* should parser parse host name?  read-only after startup */
	sbool bCreatePath;	/* auto-creation of socket directory? */
	sbool bUseCreds;	/* pull original creator credentials from socket */
	sbool bWritePid;	/* write original PID into tag */
} lstn_t;
static lstn_t listeners[MAXFUNIX];

static prop_t *pLocalHostIP = NULL;	/* there is only one global IP for all internally-generated messages */
static prop_t *pInputName = NULL;	/* our inputName currently is always "imudp", and this will hold it */
static int startIndexUxLocalSockets; /* process fd from that index on (used to
 				   * suppress local logging. rgerhards 2005-08-01
				   * read-only after startup
				   */
static int nfd = 1; /* number of Unix sockets open / read-only after startup */
static int sd_fds = 0;			/* number of systemd activated sockets */

/* config settings */
static int bOmitLocalLogging = 0;
static uchar *pLogSockName = NULL;
static uchar *pLogHostName = NULL;	/* host name to use with this socket */
static int bUseFlowCtl = 0;		/* use flow control or not (if yes, only LIGHT is used! */
static int bIgnoreTimestamp = 1;	/* ignore timestamps present in the incoming message? */
static int bWritePid = 0;		/* use credentials from recvmsg() and fixup PID in TAG */
static int bWritePidSysSock = 0;	/* use credentials from recvmsg() and fixup PID in TAG */
#define DFLT_bCreatePath 0
static int bCreatePath = DFLT_bCreatePath; /* auto-create socket path? */
#define DFLT_ratelimitInterval 5
static int ratelimitInterval = DFLT_ratelimitInterval;	/* interval in seconds, 0 = off */
static int ratelimitIntervalSysSock = DFLT_ratelimitInterval;
#define DFLT_ratelimitBurst 200
static int ratelimitBurst = DFLT_ratelimitBurst;	/* max nbr of messages in interval */
static int ratelimitBurstSysSock = DFLT_ratelimitBurst;	/* max nbr of messages in interval */
#define DFLT_ratelimitSeverity 1			/* do not rate-limit emergency messages */
static int ratelimitSeverity = DFLT_ratelimitSeverity;
static int ratelimitSeveritySysSock = DFLT_ratelimitSeverity;



static void 
initRatelimitState(struct rs_ratelimit_state *rs, unsigned short interval, unsigned short burst)
{
	rs->interval = interval;
	rs->burst = burst;
	rs->done = 0;
	rs->missed = 0;
	rs->begin = 0;
}


/* ratelimiting support, modelled after the linux kernel
 * returns 1 if message is within rate limit and shall be 
 * processed, 0 otherwise.
 * This implementation is NOT THREAD-SAFE and must not 
 * be called concurrently.
 */
static inline int
withinRatelimit(struct rs_ratelimit_state *rs, time_t tt, pid_t pid)
{
	int ret;
	uchar msgbuf[1024];

	if(rs->interval == 0) {
		ret = 1;
		goto finalize_it;
	}

	assert(rs->burst != 0);

	if(rs->begin == 0)
		rs->begin = tt;

	/* resume if we go out of out time window */
	if(tt > rs->begin + rs->interval) {
		if(rs->missed) {
			snprintf((char*)msgbuf, sizeof(msgbuf),
			         "imuxsock lost %u messages from pid %lu due to rate-limiting",
				 rs->missed, (unsigned long) pid);
			logmsgInternal(RS_RET_RATE_LIMITED, LOG_SYSLOG|LOG_INFO, msgbuf, 0);
			rs->missed = 0;
		}
		rs->begin = 0;
		rs->done = 0;
	}

	/* do actual limit check */
	if(rs->burst > rs->done) {
		rs->done++;
		ret = 1;
	} else {
		if(rs->missed == 0) {
			snprintf((char*)msgbuf, sizeof(msgbuf),
			         "imuxsock begins to drop messages from pid %lu due to rate-limiting",
				 (unsigned long) pid);
			logmsgInternal(RS_RET_RATE_LIMITED, LOG_SYSLOG|LOG_INFO, msgbuf, 0);
		}
		rs->missed++;
		ret = 0;
	}

finalize_it:
	return ret;
}


/* set the timestamp ignore / not ignore option for the system
 * log socket. This must be done separtely, as it is not added via a command
 * but present by default. -- rgerhards, 2008-03-06
 */
static rsRetVal setSystemLogTimestampIgnore(void __attribute__((unused)) *pVal, int iNewVal)
{
	DEFiRet;
	listeners[0].flags = iNewVal ? IGNDATE : NOFLAG;
	RETiRet;
}

/* set flowcontrol for the system log socket
 */
static rsRetVal setSystemLogFlowControl(void __attribute__((unused)) *pVal, int iNewVal)
{
	DEFiRet;
	listeners[0].flowCtl = iNewVal ? eFLOWCTL_LIGHT_DELAY : eFLOWCTL_NO_DELAY;
	RETiRet;
}

/* add an additional listen socket. Socket names are added
 * until the array is filled up. It is never reset, only at
 * module unload.
 * TODO: we should change the array to a list so that we
 * can support any number of listen socket names.
 * rgerhards, 2007-12-20
 * added capability to specify hostname for socket -- rgerhards, 2008-08-01
 */
static rsRetVal
addLstnSocketName(void __attribute__((unused)) *pVal, uchar *pNewVal)
{
	DEFiRet;

	if(nfd < MAXFUNIX) {
		if(*pNewVal == ':') {
			listeners[nfd].bParseHost = 1;
		} else {
			listeners[nfd].bParseHost = 0;
		}
		CHKiRet(prop.Construct(&(listeners[nfd].hostName)));
		if(pLogHostName == NULL) {
			CHKiRet(prop.SetString(listeners[nfd].hostName, glbl.GetLocalHostName(), ustrlen(glbl.GetLocalHostName())));
		} else {
			CHKiRet(prop.SetString(listeners[nfd].hostName, pLogHostName, ustrlen(pLogHostName)));
			/* reset hostname for next socket */
			free(pLogHostName);
			pLogHostName = NULL;
		}
		CHKiRet(prop.ConstructFinalize(listeners[nfd].hostName));
		if(ratelimitInterval > 0) {
			if((listeners[nfd].ht = create_hashtable(100, hash_from_key_fn, key_equals_fn, NULL)) == NULL) {
				/* in this case, we simply turn of rate-limiting */
				dbgprintf("imuxsock: turning off rate limiting because we could not "
					  "create hash table\n");
				ratelimitInterval = 0;
			}
		}
		listeners[nfd].ratelimitInterval = ratelimitInterval;
		listeners[nfd].ratelimitBurst = ratelimitBurst;
		listeners[nfd].ratelimitSev = ratelimitSeverity;
		listeners[nfd].flowCtl = bUseFlowCtl ? eFLOWCTL_LIGHT_DELAY : eFLOWCTL_NO_DELAY;
		listeners[nfd].flags = bIgnoreTimestamp ? IGNDATE : NOFLAG;
		listeners[nfd].bCreatePath = bCreatePath;
		listeners[nfd].sockName = pNewVal;
		listeners[nfd].bUseCreds = (bWritePid || ratelimitInterval) ? 1 : 0;
		listeners[nfd].bWritePid = bWritePid;
		nfd++;
	} else {
		errmsg.LogError(0, NO_ERRCODE, "Out of unix socket name descriptors, ignoring %s\n",
			 pNewVal);
	}

finalize_it:
	RETiRet;
}


/* discard all log sockets except for "socket" 0. Data for it comes from
 * the constant memory pool - and if not, it is freeed via some other pointer.
 */
static rsRetVal discardLogSockets(void)
{
	int i;

        for (i = 1; i < nfd; i++) {
		if(listeners[i].sockName != NULL) {
			free(listeners[i].sockName);
			listeners[i].sockName = NULL;
		}
		if(listeners[i].hostName != NULL) {
			prop.Destruct(&(listeners[i].hostName));
		}
		if(listeners[i].ht != NULL) {
			hashtable_destroy(listeners[i].ht, 1); /* 1 => free all values automatically */
		}
	}

	return RS_RET_OK;
}


/* used to create a log socket if NOT passed in via systemd. 
 */
static inline rsRetVal
createLogSocket(lstn_t *pLstn)
{
	struct sockaddr_un sunx;
	DEFiRet;

	unlink((char*)pLstn->sockName);
	memset(&sunx, 0, sizeof(sunx));
	sunx.sun_family = AF_UNIX;
	if(pLstn->bCreatePath) {
		makeFileParentDirs((uchar*)pLstn->sockName, ustrlen(pLstn->sockName), 0755, -1, -1, 0);
	}
	strncpy(sunx.sun_path, (char*)pLstn->sockName, sizeof(sunx.sun_path));
	pLstn->fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if(pLstn->fd < 0 || bind(pLstn->fd, (struct sockaddr *) &sunx, SUN_LEN(&sunx)) < 0 ||
	    chmod((char*)pLstn->sockName, 0666) < 0) {
		errmsg.LogError(errno, NO_ERRCODE, "connot create '%s'", pLstn->sockName);
		dbgprintf("cannot create %s (%d).\n", pLstn->sockName, errno);
		close(pLstn->fd);
		pLstn->fd = -1;
		ABORT_FINALIZE(RS_RET_ERR_CRE_AFUX);
	}
finalize_it:
	RETiRet;
}


static inline rsRetVal
openLogSocket(lstn_t *pLstn)
{
	DEFiRet;
	int one;

	if(pLstn->sockName[0] == '\0')
		return -1;

	pLstn->fd = -1;

	if (sd_fds > 0) {
               /* Check if the current socket is a systemd activated one.
	        * If so, just use it.
		*/
		int fd;

		for (fd = SD_LISTEN_FDS_START; fd < SD_LISTEN_FDS_START + sd_fds; fd++) {
			if( sd_is_socket_unix(fd, SOCK_DGRAM, -1, (const char*) pLstn->sockName, 0) == 1) {
				/* ok, it matches -- just use as is */
				pLstn->fd = fd;

				dbgprintf("imuxsock: Acquired UNIX socket '%s' (fd %d) from systemd.\n",
					pLstn->sockName, pLstn->fd);
				break;
			}
			/*
			 * otherwise it either didn't matched *this* socket and
			 * we just continue to check the next one or there were
			 * an error and we will create a new socket bellow.
			 */
		}
	}

	if (pLstn->fd == -1) {
		CHKiRet(createLogSocket(pLstn));
	}

#	if HAVE_SCM_CREDENTIALS
	if(pLstn->bUseCreds) {
		one = 1;
		if(setsockopt(pLstn->fd, SOL_SOCKET, SO_PASSCRED, &one, (socklen_t) sizeof(one)) != 0) {
			errmsg.LogError(errno, NO_ERRCODE, "set SO_PASSCRED failed on '%s'", pLstn->sockName);
			pLstn->bUseCreds = 0;
		}
		if(setsockopt(pLstn->fd, SOL_SOCKET, SCM_CREDENTIALS, &one, sizeof(one)) != 0) {
			errmsg.LogError(errno, NO_ERRCODE, "set SCM_CREDENTIALS failed on '%s'", pLstn->sockName);
			pLstn->bUseCreds = 0;
		}
	}
#	else /* HAVE_SCM_CREDENTIALS */
	pLstn->bUseCreds = 0;
#	endif /* HAVE_SCM_CREDENTIALS */

finalize_it:
	if(iRet != RS_RET_OK) {
		close(pLstn->fd);
		pLstn->fd = -1;
	}

	RETiRet;
}


/* find ratelimiter to use for this message. Currently, we use the
 * pid, but may change to cgroup later (probably via a config switch).
 * Returns NULL if not found or rate-limiting not activated for this
 * listener (the latter being a performance enhancement).
 */
static inline rsRetVal
findRatelimiter(lstn_t *pLstn, struct ucred *cred, rs_ratelimit_state_t **prl)
{
	rs_ratelimit_state_t *rl;
	int r;
	pid_t *keybuf;
	DEFiRet;

	if(cred == NULL)
		FINALIZE;
	if(pLstn->ratelimitInterval == 0) {
		*prl = NULL;
		FINALIZE;
	}

	rl = hashtable_search(pLstn->ht, &cred->pid);
	if(rl == NULL) {
		/* we need to add a new ratelimiter, process not seen before! */
		dbgprintf("imuxsock: no ratelimiter for pid %lu, creating one\n",
			  (unsigned long) cred->pid);
		STATSCOUNTER_INC(ctrNumRatelimiters, mutCtrNumRatelimiters);
		CHKmalloc(rl = malloc(sizeof(rs_ratelimit_state_t)));
		CHKmalloc(keybuf = malloc(sizeof(pid_t)));
		*keybuf = cred->pid;
		initRatelimitState(rl, pLstn->ratelimitInterval, pLstn->ratelimitBurst);
		r = hashtable_insert(pLstn->ht, keybuf, rl);
		if(r == 0)
			ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}

	*prl = rl;

finalize_it:
	RETiRet;
}


/* patch correct pid into tag. bufTAG MUST be CONF_TAG_MAXSIZE long!
 */
static inline void
fixPID(uchar *bufTAG, int *lenTag, struct ucred *cred)
{
	int i;
	char bufPID[16];
	int lenPID;

	if(cred == NULL)
		return;
	
	lenPID = snprintf(bufPID, sizeof(bufPID), "[%lu]:", (unsigned long) cred->pid);

	for(i = *lenTag ; i >= 0  && bufTAG[i] != '[' ; --i)
		/*JUST SKIP*/;

	if(i < 0)
		i = *lenTag - 1; /* go right at end of TAG, pid was not present (-1 for ':') */
	
	if(i + lenPID > CONF_TAG_MAXSIZE)
		return; /* do not touch, as things would break */

	memcpy(bufTAG + i, bufPID, lenPID);
	*lenTag = i + lenPID;
}


/* submit received message to the queue engine
 * We now parse the message according to expected format so that we
 * can also mangle it if necessary.
 */
static inline rsRetVal
SubmitMsg(uchar *pRcv, int lenRcv, lstn_t *pLstn, struct ucred *cred)
{
	msg_t *pMsg;
	int lenMsg;
	int offs;
	int i;
	uchar *parse;
	int pri;
	int facil;
	int sever;
	uchar bufParseTAG[CONF_TAG_MAXSIZE];
	struct syslogTime st;
	time_t tt;
	rs_ratelimit_state_t *ratelimiter = NULL;
	DEFiRet;

	/* TODO: handle format errors?? */
	/* we need to parse the pri first, because we need the severity for
	 * rate-limiting as well.
	 */
	parse = pRcv;
	lenMsg = lenRcv;
	offs = 1; /* '<' */
	
	parse++;
	pri = 0;
	while(offs < lenMsg && isdigit(*parse)) {
		pri = pri * 10 + *parse - '0';
		++parse;
		++offs;
	} 
	facil = LOG_FAC(pri);
	sever = LOG_PRI(pri);

	if(sever >= pLstn->ratelimitSev) {
		/* note: if cred == NULL, then ratelimiter == NULL as well! */
		findRatelimiter(pLstn, cred, &ratelimiter); /* ignore error, better so than others... */
	}

	datetime.getCurrTime(&st, &tt);
	if(ratelimiter != NULL && !withinRatelimit(ratelimiter, tt, cred->pid)) {
		STATSCOUNTER_INC(ctrLostRatelimit, mutCtrLostRatelimit);
		FINALIZE;
	}

	/* we now create our own message object and submit it to the queue */
	CHKiRet(msgConstructWithTime(&pMsg, &st, tt));
	MsgSetRawMsg(pMsg, (char*)pRcv, lenRcv);
	parser.SanitizeMsg(pMsg);
	lenMsg = pMsg->iLenRawMsg - offs;
	MsgSetInputName(pMsg, pInputName);
	MsgSetFlowControlType(pMsg, pLstn->flowCtl);

	pMsg->iFacility = facil;
	pMsg->iSeverity = sever;
	MsgSetAfterPRIOffs(pMsg, offs);

	parse++; lenMsg--; /* '>' */

	if(datetime.ParseTIMESTAMP3164(&(pMsg->tTIMESTAMP), &parse, &lenMsg) != RS_RET_OK) {
		DBGPRINTF("we have a problem, invalid timestamp in msg!\n");
	}

	/* pull tag */

	i = 0;
	while(lenMsg > 0 && *parse != ' ' && i < CONF_TAG_MAXSIZE) {
		bufParseTAG[i++] = *parse++;
		--lenMsg;
	}
	bufParseTAG[i] = '\0';	/* terminate string */
	if(pLstn->bWritePid)
		fixPID(bufParseTAG, &i, cred);
	MsgSetTAG(pMsg, bufParseTAG, i);

	MsgSetMSGoffs(pMsg, pMsg->iLenRawMsg - lenMsg);

	if(pLstn->bParseHost) {
		pMsg->msgFlags  = pLstn->flags | PARSE_HOSTNAME;
	} else {
		pMsg->msgFlags  = pLstn->flags;
	}

	MsgSetRcvFrom(pMsg, pLstn->hostName);
	CHKiRet(MsgSetRcvFromIP(pMsg, pLocalHostIP));
	CHKiRet(submitMsg(pMsg));

	STATSCOUNTER_INC(ctrSubmit, mutCtrSubmit);
finalize_it:
	RETiRet;
}


/* This function receives data from a socket indicated to be ready
 * to receive and submits the message received for processing.
 * rgerhards, 2007-12-20
 * Interface changed so that this function is passed the array index
 * of the socket which is to be processed. This eases access to the
 * growing number of properties. -- rgerhards, 2008-08-01
 */
static rsRetVal readSocket(lstn_t *pLstn)
{
	DEFiRet;
	int iRcvd;
	int iMaxLine;
	struct msghdr msgh;
	struct iovec msgiov;
#	if HAVE_SCM_CREDENTIALS
	struct cmsghdr *cm;
#	endif
	struct ucred *cred;
	uchar bufRcv[4096+1];
	char aux[128];
	uchar *pRcv = NULL; /* receive buffer */

	assert(pLstn->fd >= 0);

	iMaxLine = glbl.GetMaxLine();

	/* we optimize performance: if iMaxLine is below 4K (which it is in almost all
	 * cases, we use a fixed buffer on the stack. Only if it is higher, heap memory
	 * is used. We could use alloca() to achive a similar aspect, but there are so
	 * many issues with alloca() that I do not want to take that route.
	 * rgerhards, 2008-09-02
	 */
	if((size_t) iMaxLine < sizeof(bufRcv) - 1) {
		pRcv = bufRcv;
	} else {
		CHKmalloc(pRcv = (uchar*) MALLOC(sizeof(uchar) * (iMaxLine + 1)));
	}

	memset(&msgh, 0, sizeof(msgh));
	memset(&msgiov, 0, sizeof(msgiov));
#	if HAVE_SCM_CREDENTIALS
	if(pLstn->bUseCreds) {
		memset(&aux, 0, sizeof(aux));
		msgh.msg_control = aux;
		msgh.msg_controllen = sizeof(aux);
	}
#	endif
	msgiov.iov_base = pRcv;
	msgiov.iov_len = iMaxLine;
	msgh.msg_iov = &msgiov;
	msgh.msg_iovlen = 1;
	iRcvd = recvmsg(pLstn->fd, &msgh, MSG_DONTWAIT);
 
	dbgprintf("Message from UNIX socket: #%d\n", pLstn->fd);
	if(iRcvd > 0) {
		cred = NULL;
#		if HAVE_SCM_CREDENTIALS
		if(pLstn->bUseCreds) {
			dbgprintf("XXX: pre CM loop, length of control message %d\n", (int) msgh.msg_controllen);
			for (cm = CMSG_FIRSTHDR(&msgh); cm; cm = CMSG_NXTHDR(&msgh, cm)) {
				dbgprintf("XXX: in CM loop, %d, %d\n", cm->cmsg_level, cm->cmsg_type);
				if (cm->cmsg_level == SOL_SOCKET && cm->cmsg_type == SCM_CREDENTIALS) {
					cred = (struct ucred*) CMSG_DATA(cm);
					dbgprintf("XXX: got credentials pid %d\n", (int) cred->pid);
					break;
				}
			}
			dbgprintf("XXX: post CM loop\n");
		}
#		endif /* HAVE_SCM_CREDENTIALS */
		CHKiRet(SubmitMsg(pRcv, iRcvd, pLstn, cred));
	} else if(iRcvd < 0 && errno != EINTR) {
		char errStr[1024];
		rs_strerror_r(errno, errStr, sizeof(errStr));
		dbgprintf("UNIX socket error: %d = %s.\n", errno, errStr);
		errmsg.LogError(errno, NO_ERRCODE, "imuxsock: recvfrom UNIX");
	}

finalize_it:
	if(pRcv != NULL && (size_t) iMaxLine >= sizeof(bufRcv) - 1)
		free(pRcv);

	RETiRet;
}


/* This function is called to gather input. */
BEGINrunInput
	int maxfds;
	int nfds;
	int i;
	int fd;
#ifdef USE_UNLIMITED_SELECT
        fd_set  *pReadfds = malloc(glbl.GetFdSetSize());
#else
        fd_set  readfds;
        fd_set *pReadfds = &readfds;
#endif

CODESTARTrunInput
	/* this is an endless loop - it is terminated when the thread is
	 * signalled to do so. This, however, is handled by the framework,
	 * right into the sleep below.
	 */
	while(1) {
		/* Add the Unix Domain Sockets to the list of read
		 * descriptors.
		 * rgerhards 2005-08-01: we must now check if there are
		 * any local sockets to listen to at all. If the -o option
		 * is given without -a, we do not need to listen at all..
		 */
	        maxfds = 0;
	        FD_ZERO (pReadfds);
		/* Copy master connections */
		for (i = startIndexUxLocalSockets; i < nfd; i++) {
			if (listeners[i].fd!= -1) {
				FD_SET(listeners[i].fd, pReadfds);
				if(listeners[i].fd > maxfds)
					maxfds=listeners[i].fd;
			}
		}

		if(Debug) {
			dbgprintf("--------imuxsock calling select, active file descriptors (max %d): ", maxfds);
			for (nfds= 0; nfds <= maxfds; ++nfds)
				if ( FD_ISSET(nfds, pReadfds) )
					dbgprintf("%d ", nfds);
			dbgprintf("\n");
		}

		/* wait for io to become ready */
		nfds = select(maxfds+1, (fd_set *) pReadfds, NULL, NULL, NULL);
		if(glbl.GetGlobalInputTermState() == 1)
			break; /* terminate input! */

		for (i = 0; i < nfd && nfds > 0; i++) {
			if(glbl.GetGlobalInputTermState() == 1)
				ABORT_FINALIZE(RS_RET_FORCE_TERM); /* terminate input! */
			if ((fd = listeners[i].fd) != -1 && FD_ISSET(fd, pReadfds)) {
				readSocket(&(listeners[i]));
				--nfds; /* indicate we have processed one */
			}
		}
	}

finalize_it:
	freeFdSet(pReadfds);
	RETiRet;
ENDrunInput


BEGINwillRun
CODESTARTwillRun
	register int i;
	int actSocks;

	/* first apply some config settings */
#	ifdef OS_SOLARIS
		/* under solaris, we must NEVER process the local log socket, because
		 * it is implemented there differently. If we used it, we would actually
		 * delete it and render the system partly unusable. So don't do that.
		 * rgerhards, 2010-03-26
		 */
		startIndexUxLocalSockets = 1;
#	else
		startIndexUxLocalSockets = bOmitLocalLogging ? 1 : 0;
#	endif
	if(pLogSockName != NULL)
		listeners[0].sockName = pLogSockName;
	if(ratelimitIntervalSysSock > 0) {
		if((listeners[0].ht = create_hashtable(100, hash_from_key_fn, key_equals_fn, NULL)) == NULL) {
			/* in this case, we simply turn of rate-limiting */
			dbgprintf("imuxsock: turning off rate limiting because we could not "
				  "create hash table\n");
			ratelimitIntervalSysSock = 0;
		}
	}
	listeners[0].ratelimitInterval = ratelimitIntervalSysSock;
	listeners[0].ratelimitBurst = ratelimitBurstSysSock;
	listeners[0].ratelimitSev = ratelimitSeveritySysSock;
	listeners[0].bUseCreds = (bWritePidSysSock || ratelimitIntervalSysSock) ? 1 : 0;
	listeners[0].bWritePid = bWritePidSysSock;

	sd_fds = sd_listen_fds(0);
	if (sd_fds < 0) {
		errmsg.LogError(-sd_fds, NO_ERRCODE, "imuxsock: Failed to acquire systemd socket");
		ABORT_FINALIZE(RS_RET_ERR_CRE_AFUX);
	}

	/* initialize and return if will run or not */
	actSocks = 0;
	for (i = startIndexUxLocalSockets ; i < nfd ; i++) {
		if(openLogSocket(&(listeners[i])) == RS_RET_OK) {
			++actSocks;
			dbgprintf("imuxsock: Opened UNIX socket '%s' (fd %d).\n", listeners[i].sockName, listeners[i].fd);
		}
	}

	if(actSocks == 0) {
		errmsg.LogError(0, NO_ERRCODE, "imuxsock does not run because we could not aquire any socket\n");
		ABORT_FINALIZE(RS_RET_ERR);
	}

	/* we need to create the inputName property (only once during our lifetime) */
	CHKiRet(prop.Construct(&pInputName));
	CHKiRet(prop.SetString(pInputName, UCHAR_CONSTANT("imuxsock"), sizeof("imuxsock") - 1));
	CHKiRet(prop.ConstructFinalize(pInputName));

finalize_it:
ENDwillRun


BEGINafterRun
CODESTARTafterRun
	int i;
	/* do cleanup here */
	/* Close the UNIX sockets. */
       for (i = 0; i < nfd; i++)
		if (listeners[i].fd != -1)
			close(listeners[i].fd);

       /* Clean-up files. */
       for(i = startIndexUxLocalSockets; i < nfd; i++)
		if (listeners[i].sockName && listeners[i].fd != -1) {

			/* If systemd passed us a socket it is systemd's job to clean it up.
			 * Do not unlink it -- we will get same socket (node) from systemd
			 * e.g. on restart again.
			 */
			if (sd_fds > 0 &&
			    listeners[i].fd >= SD_LISTEN_FDS_START &&
			    listeners[i].fd <  SD_LISTEN_FDS_START + sd_fds)
				continue;

			DBGPRINTF("imuxsock: unlinking unix socket file[%d] %s\n", i, listeners[i].sockName);
			unlink((char*) listeners[i].sockName);
		}
	/* free no longer needed string */
	free(pLogSockName);
	free(pLogHostName);

	discardLogSockets();
	nfd = 1;

	if(pInputName != NULL)
		prop.Destruct(&pInputName);

ENDafterRun


BEGINmodExit
CODESTARTmodExit
	statsobj.Destruct(&modStats);

	objRelease(parser, CORE_COMPONENT);
	objRelease(glbl, CORE_COMPONENT);
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(prop, CORE_COMPONENT);
	objRelease(statsobj, CORE_COMPONENT);
	objRelease(datetime, CORE_COMPONENT);
ENDmodExit


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURENonCancelInputTermination)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_IMOD_QUERIES
CODEqueryEtryPt_IsCompatibleWithFeature_IF_OMOD_QUERIES
ENDqueryEtryPt

static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	bOmitLocalLogging = 0;
	if(pLogSockName != NULL) {
		free(pLogSockName);
		pLogSockName = NULL;
	}
	if(pLogHostName != NULL) {
		free(pLogHostName);
		pLogHostName = NULL;
	}

	discardLogSockets();
	nfd = 1;
	bIgnoreTimestamp = 1;
	bUseFlowCtl = 0;
	bWritePid = 0;
	bWritePidSysSock = 0;
	bCreatePath = DFLT_bCreatePath;
	ratelimitInterval = DFLT_ratelimitInterval;
	ratelimitIntervalSysSock = DFLT_ratelimitInterval;
	ratelimitBurst = DFLT_ratelimitBurst;
	ratelimitBurstSysSock = DFLT_ratelimitBurst;
	ratelimitSeverity = DFLT_ratelimitSeverity;
	ratelimitSeveritySysSock = DFLT_ratelimitSeverity;

	return RS_RET_OK;
}


BEGINmodInit()
	int i;
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(prop, CORE_COMPONENT));
	CHKiRet(objUse(statsobj, CORE_COMPONENT));
	CHKiRet(objUse(datetime, CORE_COMPONENT));
	CHKiRet(objUse(parser, CORE_COMPONENT));

	dbgprintf("imuxsock version %s initializing\n", PACKAGE_VERSION);

	/* init system log socket settings */
	listeners[0].flags = IGNDATE;
	listeners[0].sockName = UCHAR_CONSTANT(_PATH_LOG);
	listeners[0].hostName = NULL;
	listeners[0].flowCtl = eFLOWCTL_NO_DELAY;
	listeners[0].fd = -1;
	listeners[0].bParseHost = 0;
	listeners[0].bUseCreds = 0;
	listeners[0].bCreatePath = 0;

	/* initialize socket names */
	for(i = 1 ; i < MAXFUNIX ; ++i) {
		listeners[i].sockName = NULL;
		listeners[i].fd  = -1;
	}

	CHKiRet(prop.Construct(&pLocalHostIP));
	CHKiRet(prop.SetString(pLocalHostIP, UCHAR_CONSTANT("127.0.0.1"), sizeof("127.0.0.1") - 1));
	CHKiRet(prop.ConstructFinalize(pLocalHostIP));

	/* now init listen socket zero, the local log socket */
	CHKiRet(prop.Construct(&(listeners[0].hostName)));
	CHKiRet(prop.SetString(listeners[0].hostName, glbl.GetLocalHostName(), ustrlen(glbl.GetLocalHostName())));
	CHKiRet(prop.ConstructFinalize(listeners[0].hostName));

	/* register config file handlers */
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"omitlocallogging", 0, eCmdHdlrBinary,
		NULL, &bOmitLocalLogging, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputunixlistensocketignoremsgtimestamp", 0, eCmdHdlrBinary,
		NULL, &bIgnoreTimestamp, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"systemlogsocketname", 0, eCmdHdlrGetWord,
		NULL, &pLogSockName, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputunixlistensockethostname", 0, eCmdHdlrGetWord,
		NULL, &pLogHostName, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputunixlistensocketflowcontrol", 0, eCmdHdlrBinary,
		NULL, &bUseFlowCtl, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputunixlistensocketcreatepath", 0, eCmdHdlrBinary,
		NULL, &bCreatePath, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputunixlistensocketusepidfromsystem", 0, eCmdHdlrBinary,
		NULL, &bWritePid, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"addunixlistensocket", 0, eCmdHdlrGetWord,
		addLstnSocketName, NULL, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"imuxsockratelimitinterval", 0, eCmdHdlrInt,
		NULL, &ratelimitInterval, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"imuxsockratelimitburst", 0, eCmdHdlrInt,
		NULL, &ratelimitBurst, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"imuxsockratelimitseverity", 0, eCmdHdlrInt,
		NULL, &ratelimitSeverity, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler,
		resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
	/* the following one is a (dirty) trick: the system log socket is not added via
	 * an "addUnixListenSocket" config format. As such, it's properties can not be modified
	 * via $InputUnixListenSocket*". So we need to add a special directive
	 * for that. We should revisit all of that once we have the new config format...
	 * rgerhards, 2008-03-06
	 */
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"systemlogsocketignoremsgtimestamp", 0, eCmdHdlrBinary,
		setSystemLogTimestampIgnore, NULL, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"systemlogsocketflowcontrol", 0, eCmdHdlrBinary,
		setSystemLogFlowControl, NULL, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"systemlogusepidfromsystem", 0, eCmdHdlrBinary,
		NULL, &bWritePidSysSock, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"systemlogratelimitinterval", 0, eCmdHdlrInt,
		NULL, &ratelimitIntervalSysSock, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"systemlogratelimitburst", 0, eCmdHdlrInt,
		NULL, &ratelimitBurstSysSock, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"systemlogratelimitseverity", 0, eCmdHdlrInt,
		NULL, &ratelimitSeveritySysSock, STD_LOADABLE_MODULE_ID));
	
	/* support statistics gathering */
	CHKiRet(statsobj.Construct(&modStats));
	CHKiRet(statsobj.SetName(modStats, UCHAR_CONSTANT("imuxsock")));
	CHKiRet(statsobj.AddCounter(modStats, UCHAR_CONSTANT("submitted"),
		ctrType_IntCtr, &ctrSubmit));
	CHKiRet(statsobj.AddCounter(modStats, UCHAR_CONSTANT("ratelimit.discarded"),
		ctrType_IntCtr, &ctrLostRatelimit));
	CHKiRet(statsobj.AddCounter(modStats, UCHAR_CONSTANT("ratelimit.numratelimiters"),
		ctrType_IntCtr, &ctrNumRatelimiters));
	CHKiRet(statsobj.ConstructFinalize(modStats));

ENDmodInit
/* vim:set ai:
 */
