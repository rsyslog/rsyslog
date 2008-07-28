/* imdiag.c
 * This is a diagnostics module, primarily meant for troubleshooting
 * and information about the runtime state of rsyslog. It is implemented
 * as an input plugin, because that interface best suits our needs
 * and also enables us to inject test messages (something not yet
 * implemented).
 *
 * File begun on 2008-07-25 by RGerhards
 *
 * Copyright 2008 Rainer Gerhards and Adiscon GmbH.
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
//#include "dirty.h"
#include "cfsysline.h"
#include "module-template.h"
#include "net.h"
#include "netstrm.h"
#include "errmsg.h"

MODULE_TYPE_INPUT

/* static data */
DEF_IMOD_STATIC_DATA
DEFobjCurrIf(net)
DEFobjCurrIf(netstrm)
DEFobjCurrIf(errmsg)

/* Module static data */
netstrms_t *pNS;	/**< pointer to network stream subsystem */
netstrm_t **ppLstn[10];	/**< our netstream listners */
int iLstnMax = 0; /**< max nbr of listeners currently supported */


/* config settings */


/* add a listen socket to our listen socket array. This is a callback
 * invoked from the netstrm class. -- rgerhards, 2008-04-23
 */
static rsRetVal
addTcpLstn(void *pUsr, netstrm_t *pLstn)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pLstn, netstrm);

	if(iLstnMax >= sizeof(ppLstn)/sizeof(netstrm_t))
		ABORT_FINALIZE(RS_RET_MAX_LSTN_REACHED);

	ppLstn[pThis->iLstnMax] = pLstn;
	++iLstnMax;

finalize_it:
	RETiRet;
}


/* initialize network stream subsystem */
static rsRetVal
initNetstrm(void)
{
	DEFiRet;

	/* prepare network stream subsystem */
	CHKiRet(netstrms.Construct(&pNS));
	CHKiRet(netstrms.SetDrvrMode(pNS, 0));	 /* always plain text */
	//CHKiRet(netstrms.SetDrvrAuthMode(pThis->pNS, pThis->pszDrvrAuthMode));
	//CHKiRet(netstrms.SetDrvrPermPeers(pThis->pNS, pThis->pPermPeers));
	// TODO: set driver!
	CHKiRet(netstrms.ConstructFinalize(pThis->pNS));

	/* set up listeners */
	CHKiRet(netstrm.LstnInit(pNS, NULL, addTcpLstn, "127.0.0.1", "44514", 1));

finalize_it:
	if(iRet != RS_RET_OK) {
		if(pThis->pNS != NULL)
			netstrms.Destruct(&pThis->pNS);
	}
	RETiRet;
}


/* This function is called to gather input. In our case, it is a bit abused
 * to drive the listener loop for the diagnostics code.
 */
BEGINrunInput
CODESTARTrunInput
ENDrunInput


/* initialize and return if will run or not */
BEGINwillRun
CODESTARTwillRun
	iRet = initNetstrm();
ENDwillRun


BEGINafterRun
CODESTARTafterRun
	/* do cleanup here */
	/* finally close our listen streams */
	for(i = 0 ; i < iLstnMax ; ++i) {
		netstrm.Destruct(ppLstn + i);
	}

	/* destruct netstream subsystem */
	netstrms.Destruct(pNS);
ENDafterRun


BEGINmodExit
CODESTARTmodExit
	/* release objects we used */
	objRelease(net, LM_NET_FILENAME);
	objRelease(netstrm, LM_NETSTRMS_FILENAME);
	objRelease(errmsg, CORE_COMPONENT);
ENDmodExit


static rsRetVal
resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
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
	CHKiRet(objUse(errmsg, CORE_COMPONENT));

#if 0
	/* register config file handlers */
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputtcpserverrun", 0, eCmdHdlrGetWord,
				   addTCPListener, NULL, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputtcpmaxsessions", 0, eCmdHdlrInt,
				   NULL, &iTCPSessMax, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputtcpserverstreamdrivermode", 0,
				   eCmdHdlrInt, NULL, &iStrmDrvrMode, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputtcpserverstreamdriverauthmode", 0,
				   eCmdHdlrGetWord, NULL, &pszStrmDrvrAuthMode, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputtcpserverstreamdriverpermittedpeer", 0,
				   eCmdHdlrGetWord, setPermittedPeer, NULL, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler,
		resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
#endif
ENDmodInit


/* vim:set ai:
 */
