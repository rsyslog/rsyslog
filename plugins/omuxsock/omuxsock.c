/* omuxsock.c
 * This is the implementation of datgram unix domain socket forwarding.
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
 * Copyright 2010 Rainer Gerhards and Adiscon GmbH.
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
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include "conf.h"
#include "srUtils.h"
#include "template.h"
#include "msg.h"
#include "cfsysline.h"
#include "module-template.h"
#include "glbl.h"
#include "errmsg.h"
#include "unicode-helper.h"

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP

/* internal structures
 */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(errmsg)
DEFobjCurrIf(glbl)

#define INVLD_SOCK -1

typedef struct _instanceData {
	permittedPeers_t *pPermPeers;
	uchar *sockName;
	int sock;
	int bIsConnected;  /* are we connected to remote host? 0 - no, 1 - yes, UDP means addr resolved */
	struct sockaddr_un addr;
} instanceData;

/* config data */
static uchar *tplName = NULL; /* name of the default template to use */
static uchar *sockName = NULL; /* name of the default template to use */

static rsRetVal doTryResume(instanceData *pData);

/* Close socket.
 */
static inline rsRetVal
closeSocket(instanceData *pData)
{
	DEFiRet;
	if(pData->sock != INVLD_SOCK) {
		close(pData->sock);
		pData->sock = INVLD_SOCK;
	}
pData->bIsConnected = 0; // TODO: remove this variable altogether
	RETiRet;
}



BEGINcreateInstance
CODESTARTcreateInstance
	pData->sock = INVLD_SOCK;
ENDcreateInstance


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURERepeatedMsgReduction)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINfreeInstance
CODESTARTfreeInstance
	/* final cleanup */
	closeSocket(pData);
	free(pData->sockName);
ENDfreeInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	DBGPRINTF("%s", pData->sockName);
ENDdbgPrintInstInfo


/* Send a message via UDP
 * rgehards, 2007-12-20
 */
static rsRetVal sendMsg(instanceData *pData, char *msg, size_t len)
{
	DEFiRet;
	unsigned lenSent = 0;

	if(pData->sock == INVLD_SOCK) {
		CHKiRet(doTryResume(pData));
	}

	if(pData->sock != INVLD_SOCK) {
		/* we need to track if we have success sending to the remote
		 * peer. Success is indicated by at least one sendto() call
		 * succeeding. We track this be bSendSuccess. We can not simply
		 * rely on lsent, as a call might initially work, but a later
		 * call fails. Then, lsent has the error status, even though
		 * the sendto() succeeded. -- rgerhards, 2007-06-22
		 */
		lenSent = sendto(pData->sock, msg, len, 0, &pData->addr, sizeof(pData->addr));
		if(lenSent == len) {
			int eno = errno;
			char errStr[1024];
			DBGPRINTF("omuxsock suspending: sendto(), socket %d, error: %d = %s.\n",
				pData->sock, eno, rs_strerror_r(eno, errStr, sizeof(errStr)));
		}
	}

finalize_it:
	RETiRet;
}


/* open socket to remote system
 */
static inline rsRetVal
openSocket(instanceData *pData)
{
	DEFiRet;
	assert(pData->sock == INVLD_SOCK);

	if((pData->sock = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
		char errStr[1024];
		int eno = errno;
		DBGPRINTF("error %d creating AF_UNIX/SOCK_DGRAM: %s.\n",
			eno, rs_strerror_r(eno, errStr, sizeof(errStr)));
		pData->sock = INVLD_SOCK;
		ABORT_FINALIZE(RS_RET_NO_SOCKET);
		
	}

	/* set up server address structure */
	memset(&pData->addr, 0, sizeof(pData->addr));
        pData->addr.sun_family = AF_UNIX;
        strcpy(pData->addr.sun_path, (char*)pData->sockName);

finalize_it:
	if(iRet != RS_RET_OK) {
		closeSocket(pData);
	}
	RETiRet;
}



/* try to resume connection if it is not ready
 */
static rsRetVal doTryResume(instanceData *pData)
{
	DEFiRet;

	DBGPRINTF("omuxsock trying to resume\n");
	closeSocket(pData);
	iRet = openSocket(pData);

	if(iRet != RS_RET_OK) {
		iRet = RS_RET_SUSPENDED;
	}

	RETiRet;
}


BEGINtryResume
CODESTARTtryResume
	iRet = doTryResume(pData);
ENDtryResume

BEGINdoAction
	char *psz = NULL; /* temporary buffering */
	register unsigned l;
	int iMaxLine;
CODESTARTdoAction
	CHKiRet(doTryResume(pData));

	iMaxLine = glbl.GetMaxLine();

	DBGPRINTF(" omuxsock:%s\n", pData->sockName);

	psz = (char*) ppString[0];
	l = strlen((char*) psz);
	if((int) l > iMaxLine)
		l = iMaxLine;

	CHKiRet(sendMsg(pData, psz, l));

finalize_it:
ENDdoAction


BEGINparseSelectorAct
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)

	/* first check if this config line is actually for us */
	if(strncmp((char*) p, ":omuxsock:", sizeof(":omuxsock:") - 1)) {
		ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
	}

	/* ok, if we reach this point, we have something for us */
	p += sizeof(":omuxsock:") - 1; /* eat indicator sequence  (-1 because of '\0'!) */
	CHKiRet(createInstance(&pData));

	/* check if a non-standard template is to be applied */
	if(*(p-1) == ';')
		--p;
	CHKiRet(cflineParseTemplateName(&p, *ppOMSR, 0, 0, tplName == NULL ? UCHAR_CONSTANT("RSYSLOG_TraditionalForwardFormat")
									   : tplName ));
	
	if(sockName == NULL) {
		errmsg.LogError(0, RS_RET_NO_SOCK_CONFIGURED, "No output socket configured for omuxsock\n");
		ABORT_FINALIZE(RS_RET_NO_SOCK_CONFIGURED);
	}

	pData->sockName = sockName;
	sockName = NULL; /* pData is now owner and will fee it */

CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


/* a common function to free our configuration variables - used both on exit
 * and on $ResetConfig processing. -- rgerhards, 2008-05-16
 */
static inline void
freeConfigVars(void)
{
	free(tplName);
	tplName = NULL;
	free(sockName);
	sockName = NULL;
}


BEGINmodExit
CODESTARTmodExit
	/* release what we no longer need */
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(glbl, CORE_COMPONENT);

	freeConfigVars();
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
ENDqueryEtryPt


/* Reset config variables for this module to default values.
 * rgerhards, 2008-03-28
 */
static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	freeConfigVars();
	return RS_RET_OK;
}


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(errmsg, CORE_COMPONENT));

	CHKiRet(regCfSysLineHdlr((uchar *)"omuxsockdefaulttemplate", 0, eCmdHdlrGetWord, NULL, &tplName, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"omuxsocksocket", 0, eCmdHdlrGetWord, NULL, &sockName, NULL));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit

/* vim:set ai:
 */
