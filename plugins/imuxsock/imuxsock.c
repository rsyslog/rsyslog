/* imuxsock.c
 * This is the implementation of the Unix sockets input module.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * File begun on 2007-12-20 by RGerhards (extracted from syslogd.c)
 *
 * Copyright 2007-2009 Rainer Gerhards and Adiscon GmbH.
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
#include "prop.h"
#include "debug.h"
#include "unlimited_select.h"
#include "sd-daemon.h"
#include "statsobj.h"
#include "datetime.h"

MODULE_TYPE_INPUT

/* defines */
#define MAXFUNIX	20
#ifndef _PATH_LOG
#ifdef BSD
#define _PATH_LOG	"/var/run/log"
#else
#define _PATH_LOG	"/dev/log"
#endif
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
DEFobjCurrIf(datetime)
DEFobjCurrIf(statsobj)

statsobj_t *modStats;
STATSCOUNTER_DEF(ctrSubmit, mutCtrSubmit)

static prop_t *pLocalHostIP = NULL;	/* there is only one global IP for all internally-generated messages */
static prop_t *pInputName = NULL;	/* our inputName currently is always "imudp", and this will hold it */
static int startIndexUxLocalSockets; /* process funix from that index on (used to
 				   * suppress local logging. rgerhards 2005-08-01
				   * read-only after startup
				   */
static int funixParseHost[MAXFUNIX] = { 0, }; /* should parser parse host name?  read-only after startup */
static int funixFlags[MAXFUNIX] = { IGNDATE, }; /* should parser parse host name?  read-only after startup */
static int funixCreateSockPath[MAXFUNIX] = { 0, }; /* auto-creation of socket directory? */
static uchar *funixn[MAXFUNIX] = { (uchar*) _PATH_LOG }; /* read-only after startup */
static prop_t *funixHName[MAXFUNIX] = { NULL, }; /* host-name override - if set, use this instead of actual name */
static int funixFlowCtl[MAXFUNIX] = { eFLOWCTL_NO_DELAY, }; /* flow control settings for this socket */
static int funix[MAXFUNIX] = { -1, }; /* read-only after startup */
static int nfunix = 1; /* number of Unix sockets open / read-only after startup */

/* config settings */
static int bOmitLocalLogging = 0;
static uchar *pLogSockName = NULL;
static uchar *pLogHostName = NULL;	/* host name to use with this socket */
static int bUseFlowCtl = 0;		/* use flow control or not (if yes, only LIGHT is used! */
static int bIgnoreTimestamp = 1; /* ignore timestamps present in the incoming message? */
#define DFLT_bCreateSockPath 0
static int bCreateSockPath = DFLT_bCreateSockPath; /* auto-create socket path? */


/* set the timestamp ignore / not ignore option for the system
 * log socket. This must be done separtely, as it is not added via a command
 * but present by default. -- rgerhards, 2008-03-06
 */
static rsRetVal setSystemLogTimestampIgnore(void __attribute__((unused)) *pVal, int iNewVal)
{
	DEFiRet;
	funixFlags[0] = iNewVal ? IGNDATE : NOFLAG;
	RETiRet;
}

/* set flowcontrol for the system log socket
 */
static rsRetVal setSystemLogFlowControl(void __attribute__((unused)) *pVal, int iNewVal)
{
	DEFiRet;
	funixFlowCtl[0] = iNewVal ? eFLOWCTL_LIGHT_DELAY : eFLOWCTL_NO_DELAY;
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

	if(nfunix < MAXFUNIX) {
		if(*pNewVal == ':') {
			funixParseHost[nfunix] = 1;
		} else {
			funixParseHost[nfunix] = 0;
		}
		CHKiRet(prop.Construct(&(funixHName[nfunix])));
		if(pLogHostName == NULL) {
			CHKiRet(prop.SetString(funixHName[nfunix], glbl.GetLocalHostName(), ustrlen(glbl.GetLocalHostName())));
		} else {
			CHKiRet(prop.SetString(funixHName[nfunix], pLogHostName, ustrlen(pLogHostName)));
			/* reset hostname for next socket */
			free(pLogHostName);
			pLogHostName = NULL;
		}
		CHKiRet(prop.ConstructFinalize(funixHName[nfunix]));
		funixFlowCtl[nfunix] = bUseFlowCtl ? eFLOWCTL_LIGHT_DELAY : eFLOWCTL_NO_DELAY;
		funixFlags[nfunix] = bIgnoreTimestamp ? IGNDATE : NOFLAG;
		funixCreateSockPath[nfunix] = bCreateSockPath;
		funixn[nfunix++] = pNewVal;
	} else {
		errmsg.LogError(0, NO_ERRCODE, "Out of unix socket name descriptors, ignoring %s\n",
			 pNewVal);
	}

finalize_it:
	RETiRet;
}


/* free the funixn[] socket names - needed as cleanup on several places
 * note that nfunix is NOT reset! funixn[0] is never freed, as it comes from
 * the constant memory pool - and if not, it is freeed via some other pointer.
 */
static rsRetVal discardFunixn(void)
{
	int i;

        for (i = 1; i < nfunix; i++) {
		if(funixn[i] != NULL) {
			free(funixn[i]);
			funixn[i] = NULL;
		}
		if(funixHName[i] != NULL) {
			prop.Destruct(&(funixHName[i]));
		}
	}

	return RS_RET_OK;
}


/* used to create a log socket if NOT passed in via systemd. 
 */
static inline rsRetVal
createLogSocket(const char *path, int bCreatePath, int *fd)
{
	struct sockaddr_un sunx;
	DEFiRet;

	unlink(path);
	memset(&sunx, 0, sizeof(sunx));
	sunx.sun_family = AF_UNIX;
	if(bCreatePath) {
		makeFileParentDirs((uchar*)path, strlen(path), 0755, -1, -1, 0);
	}
	strncpy(sunx.sun_path, path, sizeof(sunx.sun_path));
	*fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if(*fd < 0 || bind(*fd, (struct sockaddr *) &sunx, SUN_LEN(&sunx)) < 0 ||
	    chmod(path, 0666) < 0) {
		errmsg.LogError(errno, NO_ERRCODE, "connot create '%s'", path);
		dbgprintf("cannot create %s (%d).\n", path, errno);
		close(*fd);
		ABORT_FINALIZE(RS_RET_ERR_CRE_AFUX);
	}
finalize_it:
	RETiRet;
}


static inline rsRetVal
openLogSocket(const char *path, int bCreatePath, int *pfd)
{
	DEFiRet;
	int fd;
	int one;

	if (path[0] == '\0')
		return -1;

       if (strcmp(path, _PATH_LOG) == 0) {
               int r;

               /* System log socket code. Check whether an FD was passed in from systemd. If
                * so, it's the /dev/log socket, so use it. */

               r = sd_listen_fds(0);
               if (r < 0) {
                       errmsg.LogError(-r, NO_ERRCODE, "Failed to acquire systemd socket");
			ABORT_FINALIZE(RS_RET_ERR_CRE_AFUX);
               }

               if (r > 1) {
                       errmsg.LogError(EINVAL, NO_ERRCODE, "Wrong number of systemd sockets passed");
			ABORT_FINALIZE(RS_RET_ERR_CRE_AFUX);
               }

               if (r == 1) {
                       fd = SD_LISTEN_FDS_START;
                       r = sd_is_socket_unix(fd, SOCK_DGRAM, -1, _PATH_LOG, 0);
                       if (r < 0) {
                               errmsg.LogError(-r, NO_ERRCODE, "Failed to verify systemd socket type");
				ABORT_FINALIZE(RS_RET_ERR_CRE_AFUX);
                       }

                       if (!r) {
                               errmsg.LogError(EINVAL, NO_ERRCODE, "Passed systemd socket of wrong type");
				ABORT_FINALIZE(RS_RET_ERR_CRE_AFUX);
                       }
		} else {
			CHKiRet(createLogSocket(path, bCreatePath, &fd));
		}
	} else {
		CHKiRet(createLogSocket(path, bCreatePath, &fd));
	}

	one = 1;
dbgprintf("imuxsock: setting socket options!\n");
	if(setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &one, (socklen_t) sizeof(one)) != 0) {
		errmsg.LogError(errno, NO_ERRCODE, "set SCM_CREDENTIALS '%s'", path);
		close(fd);
		ABORT_FINALIZE(RS_RET_ERR_CRE_AFUX);
	}
	if(setsockopt(fd, SOL_SOCKET, SCM_CREDENTIALS, &one, sizeof(one)) != 0) {
		errmsg.LogError(errno, NO_ERRCODE, "set SCM_CREDENTIALS '%s'", path);
		close(fd);
		ABORT_FINALIZE(RS_RET_ERR_CRE_AFUX);
	}
	one = 1;
	if(setsockopt(fd, SOL_SOCKET, SO_TIMESTAMP, &one, sizeof(one)) != 0) {
		errmsg.LogError(errno, NO_ERRCODE, "set SO_TIMESTAMP '%s'", path);
		close(fd);
		return -1;
	}
	*pfd = fd;
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
	
	lenPID = snprintf(bufPID, sizeof(bufPID), "[%u]:", (unsigned) cred->pid);

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
SubmitMsg(uchar *pRcv, int lenRcv, int iSock, struct ucred *cred)
{
	msg_t *pMsg;
	int lenMsg;
	int i;
	uchar *parse;
	int pri;
	uchar bufParseTAG[CONF_TAG_MAXSIZE];
	DEFiRet;

	/* we now create our own message object and submit it to the queue */
	CHKiRet(msgConstruct(&pMsg));
	MsgSetRawMsg(pMsg, (char*)pRcv, lenRcv);
	MsgSetInputName(pMsg, pInputName);
	MsgSetFlowControlType(pMsg, funixFlowCtl[iSock]);

// TODO: handle format errors
	parse = pRcv;
	lenMsg = lenRcv;
	
	parse++; lenMsg--; /* '<' */
	pri = 0;
	while(lenMsg && isdigit(*parse)) {
		pri = pri * 10 + *parse - '0';
		++parse;
		--lenMsg;
	} 
	pMsg->iFacility = LOG_FAC(pri);
	pMsg->iSeverity = LOG_PRI(pri);
	MsgSetAfterPRIOffs(pMsg, lenRcv - lenMsg);
dbgprintf("imuxsock: submit: facil %d, sever %d\n", pMsg->iFacility, pMsg->iSeverity);

	parse++; lenMsg--; /* '>' */

dbgprintf("imuxsock: submit: stage 2\n");
	if(datetime.ParseTIMESTAMP3164(&(pMsg->tTIMESTAMP), &parse, &lenMsg) != RS_RET_OK) {
		dbgprintf("we have a problem, invalid timestamp in msg!\n");
	}

	/* pull tag */
dbgprintf("imuxsock: submit: stage 3\n");

	i = 0;
	while(lenMsg > 0 && *parse != ' ' && i < CONF_TAG_MAXSIZE) {
		bufParseTAG[i++] = *parse++;
		--lenMsg;
	}
	bufParseTAG[i] = '\0';	/* terminate string */
	fixPID(bufParseTAG, &i, cred);
	MsgSetTAG(pMsg, bufParseTAG, i);

	MsgSetMSGoffs(pMsg, lenRcv - lenMsg);

	// TODO: here we need to mangle the raw message if we need to
	// "fix up" the user pid. In the long term, it may make more sense
	// to add this (somehow) to the message object (problem: at this stage,
	// it is not fully available, eg no structured data). Alternatively, we
	// may use a custom parser for our messages, but that doesn't play too well
	// with the rest of the system.
	if(funixParseHost[iSock]) {
		pMsg->msgFlags  = funixFlags[iSock] | PARSE_HOSTNAME;
	} else {
		pMsg->msgFlags  = funixFlags[iSock];
	}

	MsgSetRcvFrom(pMsg, funixHName[iSock]);
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
static rsRetVal readSocket(int fd, int iSock)
{
	DEFiRet;
	int iRcvd;
	int iMaxLine;
	struct msghdr msgh;
	struct iovec msgiov;
	static int ratelimitErrmsg = 0; // TODO: atomic OPS
	struct cmsghdr *cm;
	struct ucred *cred;
	uchar bufRcv[4096+1];
	char aux[1024];
	uchar *pRcv = NULL; /* receive buffer */

	assert(iSock >= 0);

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
	memset(&aux, 0, sizeof(aux));
	msgiov.iov_base = pRcv;
	msgiov.iov_len = iMaxLine;
	msgh.msg_iov = &msgiov;
	msgh.msg_iovlen = 1;
	msgh.msg_control = aux;
	msgh.msg_controllen = sizeof(aux);
	iRcvd = recvmsg(fd, &msgh, MSG_DONTWAIT);
/*	iRcvd = recv(fd, pRcv, iMaxLine, 0);
 */
 
	dbgprintf("Message from UNIX socket: #%d\n", fd);
	if (iRcvd > 0) {
		ratelimitErrmsg = 0;


dbgprintf("XXX: pre CM loop, length of control message %d\n", msgh.msg_controllen);
	cred = NULL;
        for (cm = CMSG_FIRSTHDR(&msgh); cm; cm = CMSG_NXTHDR(&msgh, cm)) {
dbgprintf("XXX: in CM loop, %d, %d\n", cm->cmsg_level, cm->cmsg_type);
            if (cm->cmsg_level == SOL_SOCKET && cm->cmsg_type == SCM_CREDENTIALS) {
	    	cred = (struct ucred*) CMSG_DATA(cm);
	    	dbgprintf("XXX: got credentials pid %d\n", (int) cred->pid);
                //break;
            }
	 }
dbgprintf("XXX: post CM loop\n");



		CHKiRet(SubmitMsg(pRcv, iRcvd, iSock, cred));
	} else if (iRcvd < 0 && errno != EINTR && ratelimitErrmsg++ < 100) {
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
		for (i = startIndexUxLocalSockets; i < nfunix; i++) {
			if (funix[i] != -1) {
				FD_SET(funix[i], pReadfds);
				if (funix[i]>maxfds) maxfds=funix[i];
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

		for (i = 0; i < nfunix && nfds > 0; i++) {
			if(glbl.GetGlobalInputTermState() == 1)
				ABORT_FINALIZE(RS_RET_FORCE_TERM); /* terminate input! */
			if ((fd = funix[i]) != -1 && FD_ISSET(fd, pReadfds)) {
				readSocket(fd, i);
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
		funixn[0] = pLogSockName;

	/* initialize and return if will run or not */
	actSocks = 0;
	for (i = startIndexUxLocalSockets ; i < nfunix ; i++) {
		if(openLogSocket((char*) funixn[i], funixCreateSockPath[i], &(funix[i])) == RS_RET_OK) {
			++actSocks;
			dbgprintf("Opened UNIX socket '%s' (fd %d).\n", funixn[i], funix[i]);
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
       for (i = 0; i < nfunix; i++)
		if (funix[i] != -1)
			close(funix[i]);

       /* Clean-up files. If systemd passed us a socket it is
        * systemd's job to clean it up.*/
       if (sd_listen_fds(0) > 0)
               i = 1;
       else
               i = startIndexUxLocalSockets;
       for(; i < nfunix; i++)
		if (funixn[i] && funix[i] != -1)
			unlink((char*) funixn[i]);
	/* free no longer needed string */
	free(pLogSockName);
	free(pLogHostName);

	discardFunixn();
	nfunix = 1;

	if(pInputName != NULL)
		prop.Destruct(&pInputName);
ENDafterRun


BEGINmodExit
CODESTARTmodExit
	statsobj.Destruct(&modStats);

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

	discardFunixn();
	nfunix = 1;
	bIgnoreTimestamp = 1;
	bUseFlowCtl = 0;
	bCreateSockPath = DFLT_bCreateSockPath;

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

	dbgprintf("imuxsock version %s initializing\n", PACKAGE_VERSION);

	/* initialize funixn[] array */
	for(i = 1 ; i < MAXFUNIX ; ++i) {
		funixn[i] = NULL;
		funix[i]  = -1;
	}

	CHKiRet(prop.Construct(&pLocalHostIP));
	CHKiRet(prop.SetString(pLocalHostIP, UCHAR_CONSTANT("127.0.0.1"), sizeof("127.0.0.1") - 1));
	CHKiRet(prop.ConstructFinalize(pLocalHostIP));

	/* now init listen socket zero, the local log socket */
	CHKiRet(prop.Construct(&(funixHName[0])));
	CHKiRet(prop.SetString(funixHName[0], glbl.GetLocalHostName(), ustrlen(glbl.GetLocalHostName())));
	CHKiRet(prop.ConstructFinalize(funixHName[0]));

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
		NULL, &bCreateSockPath, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"addunixlistensocket", 0, eCmdHdlrGetWord,
		addLstnSocketName, NULL, STD_LOADABLE_MODULE_ID));
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
	
	/* support statistics gathering */
	CHKiRet(statsobj.Construct(&modStats));
	CHKiRet(statsobj.SetName(modStats, UCHAR_CONSTANT("imuxsock")));
	CHKiRet(statsobj.AddCounter(modStats, UCHAR_CONSTANT("submitted"),
		ctrType_IntCtr, &ctrSubmit));
	CHKiRet(statsobj.ConstructFinalize(modStats));

ENDmodInit
/* vim:set ai:
 */
