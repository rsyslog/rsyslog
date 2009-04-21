/* tcps_sess.c
 *
 * This implements a session of the tcpsrv object. For general
 * comments, see header of tcpsrv.c.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * File begun on 2008-03-01 by RGerhards (extracted from tcpsrv.c)
 *
 * Copyright 2007, 2008 Rainer Gerhards and Adiscon GmbH.
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
#include <errno.h>
#include <ctype.h>

#include "rsyslog.h"
#include "dirty.h"
#include "module-template.h"
#include "net.h"
#include "tcpsrv.h"
#include "tcps_sess.h"
#include "obj.h"
#include "errmsg.h"
#include "netstrm.h"
#include "msg.h"


/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(glbl)
DEFobjCurrIf(errmsg)
DEFobjCurrIf(netstrm)

static int iMaxLine; /* maximum size of a single message */

/* forward definitions */
static rsRetVal Close(tcps_sess_t *pThis);


/* Standard-Constructor */
BEGINobjConstruct(tcps_sess) /* be sure to specify the object type also in END macro! */
		pThis->iMsg = 0; /* just make sure... */
		pThis->bAtStrtOfFram = 1; /* indicate frame header expected */
		pThis->eFraming = TCP_FRAMING_OCTET_STUFFING; /* just make sure... */
		/* now allocate the message reception buffer */
		CHKmalloc(pThis->pMsg = (uchar*) malloc(sizeof(uchar) * iMaxLine + 1));
finalize_it:
ENDobjConstruct(tcps_sess)


/* ConstructionFinalizer
 */
static rsRetVal
tcps_sessConstructFinalize(tcps_sess_t __attribute__((unused)) *pThis)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, tcps_sess);
	if(pThis->pSrv->OnSessConstructFinalize != NULL) {
		CHKiRet(pThis->pSrv->OnSessConstructFinalize(&pThis->pUsr));
	}

finalize_it:
	RETiRet;
}


/* destructor for the tcps_sess object */
BEGINobjDestruct(tcps_sess) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(tcps_sess)
	if(pThis->pStrm != NULL)
		netstrm.Destruct(&pThis->pStrm);

	if(pThis->pSrv->pOnSessDestruct != NULL) {
		pThis->pSrv->pOnSessDestruct(&pThis->pUsr);
	}
	/* now destruct our own properties */
	if(pThis->fromHost != NULL)
		free(pThis->fromHost);
	if(pThis->fromHostIP != NULL)
		free(pThis->fromHostIP);
	if(pThis->pMsg != NULL)
		free(pThis->pMsg);
ENDobjDestruct(tcps_sess)


/* debugprint for the tcps_sess object */
BEGINobjDebugPrint(tcps_sess) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDebugPrint(tcps_sess)
ENDobjDebugPrint(tcps_sess)


/* set property functions */
/* set the hostname. Note that the caller *hands over* the string. That is,
 * the caller no longer controls it once SetHost() has received it. Most importantly,
 * the caller must not free it. -- rgerhards, 2008-04-24
 */
static rsRetVal
SetHost(tcps_sess_t *pThis, uchar *pszHost)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, tcps_sess);

	if(pThis->fromHost != NULL) {
		free(pThis->fromHost);
	}
	
	pThis->fromHost = pszHost;

	RETiRet;
}

/* set the remote host's IP. Note that the caller *hands over* the string. That is,
 * the caller no longer controls it once SetHostIP() has received it. Most importantly,
 * the caller must not free it. -- rgerhards, 2008-05-16
 */
static rsRetVal
SetHostIP(tcps_sess_t *pThis, uchar *pszHostIP)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, tcps_sess);

	if(pThis->fromHostIP != NULL) {
		free(pThis->fromHostIP);
	}
	
	pThis->fromHostIP = pszHostIP;

	RETiRet;
}

static rsRetVal
SetStrm(tcps_sess_t *pThis, netstrm_t *pStrm)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, tcps_sess);
	pThis->pStrm = pStrm;
	RETiRet;
}


static rsRetVal
SetMsgIdx(tcps_sess_t *pThis, int idx)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, tcps_sess);
	pThis->iMsg = idx;
	RETiRet;
}


/* set our parent, the tcpsrv object */
static rsRetVal
SetTcpsrv(tcps_sess_t *pThis, tcpsrv_t *pSrv)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, tcps_sess);
	ISOBJ_TYPE_assert(pSrv, tcpsrv);
	pThis->pSrv = pSrv;
	RETiRet;
}


static rsRetVal
SetUsrP(tcps_sess_t *pThis, void *pUsr)
{
	DEFiRet;
	pThis->pUsr = pUsr;
	RETiRet;
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
static rsRetVal
PrepareClose(tcps_sess_t *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, tcps_sess);
	
	if(pThis->bAtStrtOfFram == 1) {
		/* this is how it should be. There is no unprocessed
		 * data left and such we have nothing to do. For simplicity
		 * reasons, we immediately return in that case.
		 */
		 FINALIZE;
	}

	/* we have some data left! */
	if(pThis->eFraming == TCP_FRAMING_OCTET_COUNTING) {
		/* In this case, we have an invalid frame count and thus
		 * generate an error message and discard the frame.
		 */
		errmsg.LogError(0, NO_ERRCODE, "Incomplete frame at end of stream in session %p - "
			    "ignoring extra data (a message may be lost).\n", pThis->pStrm);
		/* nothing more to do */
	} else { /* here, we have traditional framing. Missing LF at the end
		 * of message may occur. As such, we process the message in
		 * this case.
		 */
		dbgprintf("Extra data at end of stream in legacy syslog/tcp message - processing\n");
		parseAndSubmitMessage(pThis->fromHost, pThis->fromHostIP, pThis->pMsg, pThis->iMsg,
				      PARSE_HOSTNAME, eFLOWCTL_LIGHT_DELAY, pThis->pSrv->pszInputName, NULL, 0);
		pThis->bAtStrtOfFram = 1;
	}

finalize_it:
	RETiRet;
}


/* Closes a TCP session
 * No attention is paid to the return code
 * of close, so potential-double closes are not detected.
 */
static rsRetVal
Close(tcps_sess_t *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, tcps_sess);
	netstrm.Destruct(&pThis->pStrm);
	free(pThis->fromHost);
	pThis->fromHost = NULL; /* not really needed, but... */
	free(pThis->fromHostIP);
	pThis->fromHostIP = NULL; /* not really needed, but... */

	RETiRet;
}


/* process the data received. As TCP is stream based, we need to process the
 * data inside a state machine. The actual data received is passed in byte-by-byte
 * from DataRcvd, and this function here compiles messages from them and submits
 * the end result to the queue. Introducing this function fixes a long-term bug ;)
 * rgerhards, 2008-03-14
 */
static rsRetVal
processDataRcvd(tcps_sess_t *pThis, char c)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, tcps_sess);

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
			dbgprintf("TCP Message with octet-counter, size %d.\n", pThis->iOctetsRemain);
			if(c != ' ') {
				errmsg.LogError(0, NO_ERRCODE, "Framing Error in received TCP message: "
					    "delimiter is not SP but has ASCII value %d.\n", c);
			}
			if(pThis->iOctetsRemain < 1) {
				/* TODO: handle the case where the octet count is 0! */
				dbgprintf("Framing Error: invalid octet count\n");
				errmsg.LogError(0, NO_ERRCODE, "Framing Error in received TCP message: "
					    "invalid octet count %d.\n", pThis->iOctetsRemain);
			} else if(pThis->iOctetsRemain > iMaxLine) {
				/* while we can not do anything against it, we can at least log an indication
				 * that something went wrong) -- rgerhards, 2008-03-14
				 */
				dbgprintf("truncating message with %d octets - max msg size is %d\n",
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
			dbgprintf("error: message received is larger than max msg size, we split it\n");
			parseAndSubmitMessage(pThis->fromHost, pThis->fromHostIP, pThis->pMsg, pThis->iMsg,
				      	      PARSE_HOSTNAME, eFLOWCTL_LIGHT_DELAY, pThis->pSrv->pszInputName, NULL, 0);
			pThis->iMsg = 0;
			/* we might think if it is better to ignore the rest of the
			 * message than to treat it as a new one. Maybe this is a good
			 * candidate for a configuration parameter...
			 * rgerhards, 2006-12-04
			 */
		}

		if((   (c == '\n')
		   || ((pThis->pSrv->addtlFrameDelim != TCPSRV_NO_ADDTL_DELIMITER) && (c == pThis->pSrv->addtlFrameDelim))
		   ) && pThis->eFraming == TCP_FRAMING_OCTET_STUFFING) { /* record delimiter? */
			parseAndSubmitMessage(pThis->fromHost, pThis->fromHostIP, pThis->pMsg, pThis->iMsg,
				      	      PARSE_HOSTNAME, eFLOWCTL_LIGHT_DELAY, pThis->pSrv->pszInputName, NULL, 0);
			pThis->iMsg = 0;
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
				parseAndSubmitMessage(pThis->fromHost, pThis->fromHostIP, pThis->pMsg, pThis->iMsg,
				      	      PARSE_HOSTNAME, eFLOWCTL_LIGHT_DELAY, pThis->pSrv->pszInputName, NULL, 0);
				pThis->iMsg = 0;
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
 */
static rsRetVal
DataRcvd(tcps_sess_t *pThis, char *pData, size_t iLen)
{
	DEFiRet;
	char *pEnd;

	ISOBJ_TYPE_assert(pThis, tcps_sess);
	assert(pData != NULL);
	assert(iLen > 0);

	 /* We now copy the message to the session buffer. */
	pEnd = pData + iLen; /* this is one off, which is intensional */

	while(pData < pEnd) {
		CHKiRet(processDataRcvd(pThis, *pData++));
	}

finalize_it:
	RETiRet;
}


/* queryInterface function
 * rgerhards, 2008-02-29
 */
BEGINobjQueryInterface(tcps_sess)
CODESTARTobjQueryInterface(tcps_sess)
	if(pIf->ifVersion != tcps_sessCURR_IF_VERSION) { /* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->DebugPrint = tcps_sessDebugPrint;
	pIf->Construct = tcps_sessConstruct;
	pIf->ConstructFinalize = tcps_sessConstructFinalize;
	pIf->Destruct = tcps_sessDestruct;

	pIf->PrepareClose = PrepareClose;
	pIf->Close = Close;
	pIf->DataRcvd = DataRcvd;

	pIf->SetUsrP = SetUsrP;
	pIf->SetTcpsrv = SetTcpsrv;
	pIf->SetHost = SetHost;
	pIf->SetHostIP = SetHostIP;
	pIf->SetStrm = SetStrm;
	pIf->SetMsgIdx = SetMsgIdx;
finalize_it:
ENDobjQueryInterface(tcps_sess)


/* exit our class
 * rgerhards, 2008-03-10
 */
BEGINObjClassExit(tcps_sess, OBJ_IS_LOADABLE_MODULE) /* CHANGE class also in END MACRO! */
CODESTARTObjClassExit(tcps_sess)
	/* release objects we no longer need */
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(netstrm, LM_NETSTRMS_FILENAME);
ENDObjClassExit(tcps_sess)


/* Initialize our class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-29
 */
BEGINObjClassInit(tcps_sess, 1, OBJ_IS_CORE_MODULE) /* class, version - CHANGE class also in END MACRO! */
	/* request objects we use */
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(netstrm, LM_NETSTRMS_FILENAME));

	CHKiRet(objUse(glbl, CORE_COMPONENT));
	iMaxLine = glbl.GetMaxLine(); /* get maximum size we currently support */
	objRelease(glbl, CORE_COMPONENT);

	/* set our own handlers */
	OBJSetMethodHandler(objMethod_DEBUGPRINT, tcps_sessDebugPrint);
	OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, tcps_sessConstructFinalize);
ENDObjClassInit(tcps_sess)



/* vim:set ai:
 */
