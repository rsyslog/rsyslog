/* imdiag.c
 * This is a diagnostics module, primarily meant for troubleshooting
 * and information about the runtime state of rsyslog. It is implemented
 * as an input plugin, because that interface best suits our needs
 * and also enables us to inject test messages (something not yet
 * implemented).
 *
 * File begun on 2008-07-25 by RGerhards
 *
 * Copyright 2008, 2009 Rainer Gerhards and Adiscon GmbH.
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
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include "rsyslog.h"
#include "dirty.h"
#include "cfsysline.h"
#include "module-template.h"
#include "unicode-helper.h"
#include "net.h"
#include "netstrm.h"
#include "errmsg.h"
#include "tcpsrv.h"
#include "srUtils.h"
#include "net.h" /* for permittedPeers, may be removed when this is removed */

MODULE_TYPE_INPUT

/* static data */
DEF_IMOD_STATIC_DATA
DEFobjCurrIf(tcpsrv)
DEFobjCurrIf(tcps_sess)
DEFobjCurrIf(net)
DEFobjCurrIf(netstrm)
DEFobjCurrIf(errmsg)

/* Module static data */
static tcpsrv_t *pOurTcpsrv = NULL;  /* our TCP server(listener) TODO: change for multiple instances */
static permittedPeers_t *pPermPeersRoot = NULL;


/* config settings */
static int iTCPSessMax = 20; /* max number of sessions */
static int iStrmDrvrMode = 0; /* mode for stream driver, driver-dependent (0 mostly means plain tcp) */
static uchar *pszStrmDrvrAuthMode = NULL; /* authentication mode to use */
static uchar *pszInputName = NULL; /* value for inputname property, NULL is OK and handled by core engine */


/* callbacks */
/* this shall go into a specific ACL module! */
static int
isPermittedHost(struct sockaddr __attribute__((unused)) *addr, char __attribute__((unused)) *fromHostFQDN,
		void __attribute__((unused)) *pUsrSrv, void __attribute__((unused)) *pUsrSess)
{
	return 1;	/* TODO: implement ACLs ... or via some other way? */
}


static rsRetVal
doOpenLstnSocks(tcpsrv_t *pSrv)
{
	ISOBJ_TYPE_assert(pSrv, tcpsrv);
	return tcpsrv.create_tcp_socket(pSrv);
}


static rsRetVal
doRcvData(tcps_sess_t *pSess, char *buf, size_t lenBuf, ssize_t *piLenRcvd)
{
	DEFiRet;
	assert(pSess != NULL);
	assert(piLenRcvd != NULL);

	*piLenRcvd = lenBuf;
	CHKiRet(netstrm.Rcv(pSess->pStrm, (uchar*) buf, piLenRcvd));
finalize_it:
	RETiRet;
}

static rsRetVal
onRegularClose(tcps_sess_t *pSess)
{
	DEFiRet;
	assert(pSess != NULL);

	/* process any incomplete frames left over */
	tcps_sess.PrepareClose(pSess);
	/* Session closed */
	tcps_sess.Close(pSess);
	RETiRet;
}


static rsRetVal
onErrClose(tcps_sess_t *pSess)
{
	DEFiRet;
	assert(pSess != NULL);

	tcps_sess.Close(pSess);
	RETiRet;
}

/* ------------------------------ end callbacks ------------------------------ */


/* This function waits until the main queue is drained (size = 0)
 */
static rsRetVal
waitMainQEmpty(void)
{
	int iMsgQueueSize;
	DEFiRet;

	CHKiRet(diagGetMainMsgQSize(&iMsgQueueSize));
	while(iMsgQueueSize > 0) {
		srSleep(0,2);	/* wait a little bit */
		CHKiRet(diagGetMainMsgQSize(&iMsgQueueSize));
	}

finalize_it:
	RETiRet;
}


/* Function to handle received messages. This is our core function!
 * rgerhards, 2009-05-24
 */
static rsRetVal
OnMsgReceived(tcps_sess_t *pSess, uchar *pRcv, int iLenMsg)
{
	ssize_t len;
	int iMsgQueueSize;
	uchar *pszMsg;
	uchar buf[1024];
	DEFiRet;

	assert(pSess != NULL);
	assert(pRcv != NULL);

	/* NOTE: pRcv is NOT a C-String but rather an array of characters
	 * WITHOUT a termination \0 char. So we need to convert it to one
	 * before proceeding.
	 */
	CHKmalloc(pszMsg = malloc(sizeof(uchar) * (iLenMsg + 1)));
	memcpy(pszMsg, pRcv, iLenMsg);
	pszMsg[iLenMsg] = '\0';

	if(!ustrcmp(pszMsg, UCHAR_CONSTANT("GetMainMsgQueueSize"))) {
		CHKiRet(diagGetMainMsgQSize(&iMsgQueueSize));
		len = snprintf((char*)buf, sizeof(buf)/sizeof(uchar), "%d\n", iMsgQueueSize);
		CHKiRet(netstrm.Send(pSess->pStrm, buf, &len));
	} else if(!ustrcmp(pszMsg, UCHAR_CONSTANT("WaitMainQueueEmpty"))) {
		CHKiRet(waitMainQEmpty());
		len = snprintf((char*)buf, sizeof(buf)/sizeof(uchar), "mainqueue empty\n");
		CHKiRet(netstrm.Send(pSess->pStrm, buf, &len));
	} else {
		len = snprintf((char*)buf, sizeof(buf)/sizeof(uchar), "unkown command '%s'\n", pszMsg);
		CHKiRet(netstrm.Send(pSess->pStrm, buf, &len));
	}

finalize_it:
	RETiRet;
}


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


static rsRetVal addTCPListener(void __attribute__((unused)) *pVal, uchar *pNewVal)
{
	DEFiRet;

	if(pOurTcpsrv == NULL) {
		CHKiRet(tcpsrv.Construct(&pOurTcpsrv));
		CHKiRet(tcpsrv.SetSessMax(pOurTcpsrv, iTCPSessMax));
		CHKiRet(tcpsrv.SetCBIsPermittedHost(pOurTcpsrv, isPermittedHost));
		CHKiRet(tcpsrv.SetCBRcvData(pOurTcpsrv, doRcvData));
		CHKiRet(tcpsrv.SetCBOpenLstnSocks(pOurTcpsrv, doOpenLstnSocks));
		CHKiRet(tcpsrv.SetCBOnRegularClose(pOurTcpsrv, onRegularClose));
		CHKiRet(tcpsrv.SetCBOnErrClose(pOurTcpsrv, onErrClose));
		CHKiRet(tcpsrv.SetDrvrMode(pOurTcpsrv, iStrmDrvrMode));
		CHKiRet(tcpsrv.SetOnMsgReceive(pOurTcpsrv, OnMsgReceived));
		/* now set optional params, but only if they were actually configured */
		if(pszStrmDrvrAuthMode != NULL) {
			CHKiRet(tcpsrv.SetDrvrAuthMode(pOurTcpsrv, pszStrmDrvrAuthMode));
		}
		if(pPermPeersRoot != NULL) {
			CHKiRet(tcpsrv.SetDrvrPermPeers(pOurTcpsrv, pPermPeersRoot));
		}
	}

	/* initialized, now add socket */
	CHKiRet(tcpsrv.SetInputName(pOurTcpsrv, pszInputName == NULL ?
						UCHAR_CONSTANT("imdiag") : pszInputName));
	tcpsrv.configureTCPListen(pOurTcpsrv, pNewVal);

finalize_it:
	if(iRet != RS_RET_OK) {
		errmsg.LogError(0, NO_ERRCODE, "error %d trying to add listener", iRet);
		if(pOurTcpsrv != NULL)
			tcpsrv.Destruct(&pOurTcpsrv);
	}
	RETiRet;
}

/* This function is called to gather input.
 */
BEGINrunInput
CODESTARTrunInput
	CHKiRet(tcpsrv.ConstructFinalize(pOurTcpsrv));
	iRet = tcpsrv.Run(pOurTcpsrv);
finalize_it:
ENDrunInput


/* initialize and return if will run or not */
BEGINwillRun
CODESTARTwillRun
	/* first apply some config settings */
	if(pOurTcpsrv == NULL)
		ABORT_FINALIZE(RS_RET_NO_RUN);
finalize_it:
ENDwillRun


BEGINafterRun
CODESTARTafterRun
	/* do cleanup here */
ENDafterRun


BEGINmodExit
CODESTARTmodExit
	if(pOurTcpsrv != NULL)
		iRet = tcpsrv.Destruct(&pOurTcpsrv);

	if(pPermPeersRoot != NULL) {
		net.DestructPermittedPeers(&pPermPeersRoot);
	}

	/* release objects we used */
	objRelease(net, LM_NET_FILENAME);
	objRelease(netstrm, LM_NETSTRMS_FILENAME);
	objRelease(tcps_sess, LM_TCPSRV_FILENAME);
	objRelease(tcpsrv, LM_TCPSRV_FILENAME);
	objRelease(errmsg, CORE_COMPONENT);
ENDmodExit


static rsRetVal
resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	iTCPSessMax = 200;
	iStrmDrvrMode = 0;
	free(pszInputName);
	pszInputName = NULL;
	if(pszStrmDrvrAuthMode != NULL) {
		free(pszStrmDrvrAuthMode);
		pszStrmDrvrAuthMode = NULL;
	}
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
	pOurTcpsrv = NULL;
	/* request objects we use */
	CHKiRet(objUse(net, LM_NET_FILENAME));
	CHKiRet(objUse(netstrm, LM_NETSTRMS_FILENAME));
	CHKiRet(objUse(tcps_sess, LM_TCPSRV_FILENAME));
	CHKiRet(objUse(tcpsrv, LM_TCPSRV_FILENAME));
	CHKiRet(objUse(errmsg, CORE_COMPONENT));

	/* register config file handlers */
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("imdiagserverrun"), 0, eCmdHdlrGetWord,
				   addTCPListener, NULL, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("imdiagmaxsessions"), 0, eCmdHdlrInt,
				   NULL, &iTCPSessMax, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("imdiagserverstreamdrivermode"), 0,
				   eCmdHdlrInt, NULL, &iStrmDrvrMode, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("imdiagserverstreamdriverauthmode"), 0,
				   eCmdHdlrGetWord, NULL, &pszStrmDrvrAuthMode, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("imdiagserverstreamdriverpermittedpeer"), 0,
				   eCmdHdlrGetWord, setPermittedPeer, NULL, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("imdiagserverinputname"), 0,
				   eCmdHdlrGetWord, NULL, &pszInputName, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr(UCHAR_CONSTANT("resetconfigvariables"), 1, eCmdHdlrCustomHandler,
		resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit


/* vim:set ai:
 */
