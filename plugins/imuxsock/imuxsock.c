/* imuxsock.c
 * This is the implementation of the Unix sockets input module.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * File begun on 2007-12-20 by RGerhards (extracted from syslogd.c)
 *
 * Copyright 2007 Rainer Gerhards and Adiscon GmbH.
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
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/un.h>
#include "dirty.h"
#include "cfsysline.h"
#include "module-template.h"
#include "srUtils.h"
#include "errmsg.h"
#include "net.h"
#include "glbl.h"
#include "msg.h"

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

static int startIndexUxLocalSockets; /* process funix from that index on (used to 
 				   * suppress local logging. rgerhards 2005-08-01
				   * read-only after startup
				   */
static int funixParseHost[MAXFUNIX] = { 0, }; /* should parser parse host name?  read-only after startup */
static int funixFlags[MAXFUNIX] = { IGNDATE, }; /* should parser parse host name?  read-only after startup */
static uchar *funixn[MAXFUNIX] = { (uchar*) _PATH_LOG }; /* read-only after startup */
static uchar *funixHName[MAXFUNIX] = { NULL, }; /* host-name override - if set, use this instead of actual name */
static int funixFlowCtl[MAXFUNIX] = { eFLOWCTL_NO_DELAY, }; /* flow control settings for this socket */
static int funix[MAXFUNIX] = { -1, }; /* read-only after startup */
static int nfunix = 1; /* number of Unix sockets open / read-only after startup */

/* config settings */
static int bOmitLocalLogging = 0;
static uchar *pLogSockName = NULL;
static uchar *pLogHostName = NULL;	/* host name to use with this socket */
static int bUseFlowCtl = 0;		/* use flow control or not (if yes, only LIGHT is used! */
static int bIgnoreTimestamp = 1; /* ignore timestamps present in the incoming message? */


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
static rsRetVal addLstnSocketName(void __attribute__((unused)) *pVal, uchar *pNewVal)
{
	if(nfunix < MAXFUNIX) {
		if(*pNewVal == ':') {
			funixParseHost[nfunix] = 1;
		}
		else {
			funixParseHost[nfunix] = 0;
		}
		funixHName[nfunix] = pLogHostName;
		pLogHostName = NULL; /* re-init for next, not freed because funixHName[] now owns it */
		funixFlowCtl[nfunix] = bUseFlowCtl ? eFLOWCTL_LIGHT_DELAY : eFLOWCTL_NO_DELAY;
		funixFlags[nfunix] = bIgnoreTimestamp ? IGNDATE : NOFLAG;
		funixn[nfunix++] = pNewVal;
	}
	else {
		errmsg.LogError(0, NO_ERRCODE, "Out of unix socket name descriptors, ignoring %s\n",
			 pNewVal);
	}

	return RS_RET_OK;
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
			free(funixHName[i]);
			funixHName[i] = NULL;
		}
	}
	
	return RS_RET_OK;
}


static int create_unix_socket(const char *path)
{
	struct sockaddr_un sunx;
	int fd;

	if (path[0] == '\0')
		return -1;

	unlink(path);

	memset(&sunx, 0, sizeof(sunx));
	sunx.sun_family = AF_UNIX;
	(void) strncpy(sunx.sun_path, path, sizeof(sunx.sun_path));
	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd < 0 || bind(fd, (struct sockaddr *) &sunx, SUN_LEN(&sunx)) < 0 ||
	    chmod(path, 0666) < 0) {
		errmsg.LogError(errno, NO_ERRCODE, "connot create '%s'", path);
		dbgprintf("cannot create %s (%d).\n", path, errno);
		close(fd);
		return -1;
	}
	return fd;
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
	uchar bufRcv[4096+1];
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
		CHKmalloc(pRcv = (uchar*) malloc(sizeof(uchar) * (iMaxLine + 1)));
	}

	iRcvd = recv(fd, pRcv, iMaxLine, 0);
	dbgprintf("Message from UNIX socket: #%d\n", fd);
	if (iRcvd > 0) {
		parseAndSubmitMessage(funixHName[iSock] == NULL ? glbl.GetLocalHostName() : funixHName[iSock],
				      (uchar*)"127.0.0.1", pRcv,
			 	      iRcvd, funixParseHost[iSock] ? (funixFlags[iSock] | PARSE_HOSTNAME) : funixFlags[iSock],
				      funixFlowCtl[iSock], (uchar*)"imuxsock", NULL, 0);
	} else if (iRcvd < 0 && errno != EINTR) {
		char errStr[1024];
		rs_strerror_r(errno, errStr, sizeof(errStr));
		dbgprintf("UNIX socket error: %d = %s.\n", errno, errStr);
		errmsg.LogError(errno, NO_ERRCODE, "recvfrom UNIX");
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
	fd_set readfds;
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
	        FD_ZERO (&readfds);
		/* Copy master connections */
		for (i = startIndexUxLocalSockets; i < nfunix; i++) {
			if (funix[i] != -1) {
				FD_SET(funix[i], &readfds);
				if (funix[i]>maxfds) maxfds=funix[i];
			}
		}

		if(Debug) {
			dbgprintf("--------imuxsock calling select, active file descriptors (max %d): ", maxfds);
			for (nfds= 0; nfds <= maxfds; ++nfds)
				if ( FD_ISSET(nfds, &readfds) )
					dbgprintf("%d ", nfds);
			dbgprintf("\n");
		}

		/* wait for io to become ready */
		nfds = select(maxfds+1, (fd_set *) &readfds, NULL, NULL, NULL);

		for (i = 0; i < nfunix && nfds > 0; i++) {
			if ((fd = funix[i]) != -1 && FD_ISSET(fd, &readfds)) {
				readSocket(fd, i);
				--nfds; /* indicate we have processed one */
			}
		}
	}

	RETiRet;
ENDrunInput


BEGINwillRun
CODESTARTwillRun
	register int i;

	/* first apply some config settings */
	startIndexUxLocalSockets = bOmitLocalLogging ? 1 : 0;
	if(pLogSockName != NULL)
		funixn[0] = pLogSockName;

	/* initialize and return if will run or not */
	for (i = startIndexUxLocalSockets ; i < nfunix ; i++) {
		if ((funix[i] = create_unix_socket((char*) funixn[i])) != -1)
			dbgprintf("Opened UNIX socket '%s' (fd %d).\n", funixn[i], funix[i]);
	}

	RETiRet;
ENDwillRun


BEGINafterRun
CODESTARTafterRun
	int i;
	/* do cleanup here */
	/* Close the UNIX sockets. */
        for (i = 0; i < nfunix; i++)
		if (funix[i] != -1)
			close(funix[i]);

	/* Clean-up files. */
        for (i = 0; i < nfunix; i++)
		if (funixn[i] && funix[i] != -1)
			unlink((char*) funixn[i]);
	/* free no longer needed string */
	if(pLogSockName != NULL)
		free(pLogSockName);
	if(pLogHostName != NULL) {
		free(pLogHostName);
	}

	discardFunixn();
	nfunix = 1;
ENDafterRun


BEGINmodExit
CODESTARTmodExit
	objRelease(glbl, CORE_COMPONENT);
	objRelease(errmsg, CORE_COMPONENT);
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_IMOD_QUERIES
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

	return RS_RET_OK;
}


BEGINmodInit()
	int i;
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(glbl, CORE_COMPONENT));

	dbgprintf("imuxsock version %s initializing\n", PACKAGE_VERSION);

	/* initialize funixn[] array */
	for(i = 1 ; i < MAXFUNIX ; ++i) {
		funixn[i] = NULL;
		funix[i]  = -1;
	}

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
ENDmodInit
/* vim:set ai:
 */
