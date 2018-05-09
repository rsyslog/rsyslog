/* nsdsel_ossl.c
 *
 * An implementation of the nsd select() interface for OpenSSL.
 *
 * Copyright (C) 2018-2018 Adiscon GmbH.
 * Author: Andre Lorbach
 *
 * This file is part of the rsyslog runtime library.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *       -or-
 *       see COPYING.ASL20 in the source distribution
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "config.h"

#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/select.h>
#include <openssl/ssl.h>
#include <openssl/bio.h>

#include "rsyslog.h"
#include "module-template.h"
#include "obj.h"
#include "errmsg.h"
#include "nsd.h"
#include "nsd_ossl.h"
#include "nsd_ptcp.h"
#include "nsdsel_ptcp.h"
#include "nsdsel_ossl.h"

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(glbl)
DEFobjCurrIf(nsdsel_ptcp)

static rsRetVal
osslHasRcvInBuffer(nsd_ossl_t *pThis)
{
	/* we have a valid receive buffer one such is allocated and
	 * NOT exhausted!
	 */
	DBGPRINTF("hasRcvInBuffer on nsd %p: pszRcvBuf %p, lenRcvBuf %d\n", pThis,
		pThis->pszRcvBuf, pThis->lenRcvBuf);
	return(pThis->pszRcvBuf != NULL && pThis->lenRcvBuf != -1);
}


/* Standard-Constructor
 */
BEGINobjConstruct(nsdsel_ossl) /* be sure to specify the object type also in END macro! */
	iRet = nsdsel_ptcp.Construct(&pThis->pTcp);
ENDobjConstruct(nsdsel_ossl)


/* destructor for the nsdsel_ossl object */
BEGINobjDestruct(nsdsel_ossl) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(nsdsel_ossl)
	if(pThis->pTcp != NULL)
		nsdsel_ptcp.Destruct(&pThis->pTcp);
ENDobjDestruct(nsdsel_ossl)


/* Add a socket to the select set */
static rsRetVal
Add(nsdsel_t *pNsdsel, nsd_t *pNsd, nsdsel_waitOp_t waitOp)
{
	DEFiRet;
	nsdsel_ossl_t *pThis = (nsdsel_ossl_t*) pNsdsel;
	nsd_ossl_t *pNsdOSSL = (nsd_ossl_t*) pNsd;
	int iLastErr;

	ISOBJ_TYPE_assert(pThis, nsdsel_ossl);
	ISOBJ_TYPE_assert(pNsdOSSL, nsd_ossl);
	if(pNsdOSSL->iMode == 1) {
		if(waitOp == NSDSEL_RD && osslHasRcvInBuffer(pNsdOSSL)) {
			++pThis->iBufferRcvReady;
			dbgprintf("nsdsel_ossl: data already present in buffer, initiating "
				  "dummy select %p->iBufferRcvReady=%d\n",
				  pThis, pThis->iBufferRcvReady);
			FINALIZE;
		}
		if(pNsdOSSL->rtryCall != osslRtry_None) {
assert( 1 != 1 );
LogError(0, RS_RET_GNUTLS_ERR, "pNsdOSSL->rtryCall != osslRtry_None NOT IMPLEMENTED YET\n");

			iLastErr = SSL_get_error(pNsdOSSL->ssl,0);
			if(iLastErr == SSL_ERROR_WANT_READ) {
				CHKiRet(nsdsel_ptcp.Add(pThis->pTcp, pNsdOSSL->pTcp, NSDSEL_RD));
			} else {
				CHKiRet(nsdsel_ptcp.Add(pThis->pTcp, pNsdOSSL->pTcp, NSDSEL_WR));
			}
			FINALIZE;
		}
	}

	/* if we reach this point, we need no special handling */
	CHKiRet(nsdsel_ptcp.Add(pThis->pTcp, pNsdOSSL->pTcp, waitOp));

finalize_it:
	RETiRet;
}


/* perform the select()  piNumReady returns how many descriptors are ready for IO
 * TODO: add timeout!
 */
static rsRetVal
Select(nsdsel_t *pNsdsel, int *piNumReady)
{
	DEFiRet;
	nsdsel_ossl_t *pThis = (nsdsel_ossl_t*) pNsdsel;

	ISOBJ_TYPE_assert(pThis, nsdsel_ossl);
	if(pThis->iBufferRcvReady > 0) {
		/* we still have data ready! */
		*piNumReady = pThis->iBufferRcvReady;
		dbgprintf("nsdsel_ossl: doing dummy select, data present\n");
	} else {
		iRet = nsdsel_ptcp.Select(pThis->pTcp, piNumReady);
	}

	RETiRet;
}


/* retry an interrupted OSSL operation
 * rgerhards, 2008-04-30
 */
static rsRetVal
doRetry(nsd_ossl_t *pNsd)
{
	DEFiRet;
	int osslRet;

	dbgprintf("openssl requested retry of %d operation - executing\n", pNsd->rtryCall);

	/* We follow a common scheme here: first, we do the systen call and
	 * then we check the result. So far, the result is checked after the
	 * switch, because the result check is the same for all calls. Note that
	 * this may change once we deal with the read and write calls (but
	 * probably this becomes an issue only when we begin to work on TLS
	 * for relp). -- rgerhards, 2008-04-30
	 */
	switch(pNsd->rtryCall) {
		case osslRtry_handshake:
assert( 1 != 1 );
LogError(0, RS_RET_GNUTLS_ERR, "doRetry - osslRtry_handshake NOT IMPLEMENTED YET\n");
/*			gnuRet = gnutls_handshake(pNsd->sess);
			if(gnuRet == 0) {
				pNsd->rtryCall = osslRtry_None;
				// we got a handshake, now check authorization
				CHKiRet(osslChkPeerAuth(pNsd));
			}
*/
			break;
		case osslRtry_recv:
			dbgprintf("retrying ossl recv, nsd: %p\n", pNsd);
			CHKiRet(osslRecordRecv(pNsd));
			pNsd->rtryCall = osslRtry_None; /* we are done */
			osslRet = 0;
			break;
		case osslRtry_None:
		default:
			assert(0); /* this shall not happen! */
			dbgprintf("ERROR: pNsd->rtryCall invalid in nsdsel_ossl.c:%d\n", __LINE__);
			osslRet = 0; /* if it happens, we have at least a defined behaviour... ;) */
			break;
	}

/*
	if(osslRet == 0) {
		// we are done
		pNsd->rtryCall = osslRtry_None;
	} else if(osslRet != GNUTLS_E_AGAIN && osslRet != GNUTLS_E_INTERRUPTED) {
		uchar *pErr = osslStrerror(osslRet);
		LogError(0, RS_RET_GNUTLS_ERR, "unexpected GnuTLS error %d in %s:%d: %s\n",
		osslRet, __FILE__, __LINE__, pErr); \
		free(pErr);
		// we are also done... ;)
		pNsd->rtryCall = osslRtry_None;
		ABORT_FINALIZE(RS_RET_GNUTLS_ERR);
	}
	*/

finalize_it:
	if(iRet != RS_RET_OK && iRet != RS_RET_CLOSED && iRet != RS_RET_RETRY)
		pNsd->bAbortConn = 1; /* request abort */
	RETiRet;
}


/* check if a socket is ready for IO */
static rsRetVal
IsReady(nsdsel_t *pNsdsel, nsd_t *pNsd, nsdsel_waitOp_t waitOp, int *pbIsReady)
{
	DEFiRet;
	nsdsel_ossl_t *pThis = (nsdsel_ossl_t*) pNsdsel;
	nsd_ossl_t *pNsdOSSL = (nsd_ossl_t*) pNsd;

	ISOBJ_TYPE_assert(pThis, nsdsel_ossl);
	ISOBJ_TYPE_assert(pNsdOSSL, nsd_ossl);
	if(pNsdOSSL->iMode == 1) {
		if(waitOp == NSDSEL_RD && osslHasRcvInBuffer(pNsdOSSL)) {
			*pbIsReady = 1;
			--pThis->iBufferRcvReady; /* one "pseudo-read" less */
			dbgprintf("nsdl_ossl: dummy read, decermenting %p->iBufRcvReady, now %d\n",
				   pThis, pThis->iBufferRcvReady);
			FINALIZE;
		}
		if(pNsdOSSL->rtryCall == osslRtry_handshake) {
			CHKiRet(doRetry(pNsdOSSL));
			/* we used this up for our own internal processing, so the socket
			 * is not ready from the upper layer point of view.
			 */
			*pbIsReady = 0;
			FINALIZE;
		}
		else if(pNsdOSSL->rtryCall == osslRtry_recv) {
			iRet = doRetry(pNsdOSSL);
			if(iRet == RS_RET_OK) {
				*pbIsReady = 0;
				FINALIZE;
			}
		}

		/* now we must ensure that we do not fall back to PTCP if we have
		 * done a "dummy" select. In that case, we know when the predicate
		 * is not matched here, we do not have data available for this
		 * socket. -- rgerhards, 2010-11-20
		 */
		if(pThis->iBufferRcvReady) {
			dbgprintf("nsd_ossl: dummy read, buffer not available for this FD\n");
			*pbIsReady = 0;
			FINALIZE;
		}
	}

	CHKiRet(nsdsel_ptcp.IsReady(pThis->pTcp, pNsdOSSL->pTcp, waitOp, pbIsReady));

finalize_it:
	RETiRet;
}


/* ------------------------------ end support for the select() interface ------------------------------ */


/* queryInterface function */
BEGINobjQueryInterface(nsdsel_ossl)
CODESTARTobjQueryInterface(nsdsel_ossl)
	if(pIf->ifVersion != nsdCURR_IF_VERSION) {/* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->Construct = (rsRetVal(*)(nsdsel_t**)) nsdsel_osslConstruct;
	pIf->Destruct = (rsRetVal(*)(nsdsel_t**)) nsdsel_osslDestruct;
	pIf->Add = Add;
	pIf->Select = Select;
	pIf->IsReady = IsReady;
finalize_it:
ENDobjQueryInterface(nsdsel_ossl)


/* exit our class
 */
BEGINObjClassExit(nsdsel_ossl, OBJ_IS_CORE_MODULE) /* CHANGE class also in END MACRO! */
CODESTARTObjClassExit(nsdsel_ossl)
	/* release objects we no longer need */
	objRelease(glbl, CORE_COMPONENT);
	objRelease(nsdsel_ptcp, LM_NSD_PTCP_FILENAME);
ENDObjClassExit(nsdsel_ossl)


/* Initialize the nsdsel_ossl class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINObjClassInit(nsdsel_ossl, 1, OBJ_IS_CORE_MODULE) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(nsdsel_ptcp, LM_NSD_PTCP_FILENAME));

	/* set our own handlers */
ENDObjClassInit(nsdsel_ossl)
/* vi:set ai:
 */
