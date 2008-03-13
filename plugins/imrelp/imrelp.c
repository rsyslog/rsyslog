/* imtcp.c
 * This is the implementation of the TCP input module.
 *
 * File begun on 2007-12-21 by RGerhards (extracted from syslogd.c)
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
#include "tcpsrv.h"

MODULE_TYPE_INPUT

/* static data */
DEF_IMOD_STATIC_DATA
DEFobjCurrIf(tcpsrv)
DEFobjCurrIf(tcps_sess)
DEFobjCurrIf(net)

/* Module static data */
static tcpsrv_t *pOurTcpsrv = NULL;  /* our TCP server(listener) TODO: change for multiple instances */

/* config settings */
static int iTCPSessMax = 200; /* max number of sessions */


/* callbacks */
/* this shall go into a specific ACL module! */
static int
isPermittedHost(struct sockaddr *addr, char *fromHostFQDN, void __attribute__((unused)) *pUsrSrv,
	        void __attribute__((unused)) *pUsrSess)
{
	return net.isAllowedSender(net.pAllowedSenders_TCP, addr, fromHostFQDN);
}


static int*
doOpenLstnSocks(tcpsrv_t *pSrv)
{
	ISOBJ_TYPE_assert(pSrv, tcpsrv);
	return tcpsrv.create_tcp_socket(pSrv);
}


static int
doRcvData(tcps_sess_t *pSess, char *buf, size_t lenBuf)
{
	int state;
	assert(pSess != NULL);

	state = recv(pSess->sock, buf, lenBuf, 0);
	return state;
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


static rsRetVal addTCPListener(void __attribute__((unused)) *pVal, uchar *pNewVal)
{
	DEFiRet;
	if(pOurTcpsrv == NULL) {
		CHKiRet(tcpsrv.Construct(&pOurTcpsrv));
		CHKiRet(tcpsrv.SetCBIsPermittedHost(pOurTcpsrv, isPermittedHost));
		CHKiRet(tcpsrv.SetCBRcvData(pOurTcpsrv, doRcvData));
		CHKiRet(tcpsrv.SetCBOpenLstnSocks(pOurTcpsrv, doOpenLstnSocks));
		CHKiRet(tcpsrv.SetCBOnRegularClose(pOurTcpsrv, onRegularClose));
		CHKiRet(tcpsrv.SetCBOnErrClose(pOurTcpsrv, onErrClose));
		tcpsrv.configureTCPListen(pOurTcpsrv, (char *) pNewVal);
		CHKiRet(tcpsrv.ConstructFinalize(pOurTcpsrv));
	}

finalize_it:
	RETiRet;
}

/* This function is called to gather input.
 */
BEGINrunInput
CODESTARTrunInput
	/* TODO: we must be careful to start the listener here. Currently, tcpsrv.c seems to
	 * do that in ConstructFinalize
	 */
	iRet = tcpsrv.Run(pOurTcpsrv);
ENDrunInput


/* initialize and return if will run or not */
BEGINwillRun
CODESTARTwillRun
	/* first apply some config settings */
	net.PrintAllowedSenders(2); /* TCP */
	if(pOurTcpsrv == NULL)
		ABORT_FINALIZE(RS_RET_NO_RUN);
finalize_it:
ENDwillRun


BEGINafterRun
CODESTARTafterRun
	/* do cleanup here */
	if(net.pAllowedSenders_TCP != NULL) {
		net.clearAllowedSenders(net.pAllowedSenders_TCP);
		net.pAllowedSenders_TCP = NULL;
	}
ENDafterRun


BEGINmodExit
CODESTARTmodExit
	if(pOurTcpsrv != NULL)
		iRet = tcpsrv.Destruct(&pOurTcpsrv);

	/* release objects we used */
	objRelease(net, LM_NET_FILENAME);
	objRelease(tcps_sess, LM_TCPSRV_FILENAME);
	objRelease(tcpsrv, LM_TCPSRV_FILENAME);
ENDmodExit


static rsRetVal
resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	iTCPSessMax = 200;
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
	CHKiRet(objUse(tcps_sess, LM_TCPSRV_FILENAME));
	CHKiRet(objUse(tcpsrv, LM_TCPSRV_FILENAME));

	/* register config file handlers */
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputtcpserverrun", 0, eCmdHdlrGetWord,
				   addTCPListener, NULL, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputtcpmaxsessions", 0, eCmdHdlrInt,
				   NULL, &iTCPSessMax, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler,
		resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit


/* vim:set ai:
 */
