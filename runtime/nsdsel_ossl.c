/* nsdsel_ossl.c
 *
 * An implementation of the nsd select() interface for OpenSSL.
 *
 * Copyright (C) 2018-2025 Adiscon GmbH.
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

#include "rsyslog.h"
#include "module-template.h"
#include "obj.h"
#include "errmsg.h"
#include "net_ossl.h"	// Include OpenSSL Helpers
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
/* // VERBOSE
dbgprintf("nsdsel_ossl: rtryOsslErr=%d ... \n", pNsdOSSL->rtryOsslErr);
*/
			if (pNsdOSSL->rtryOsslErr == SSL_ERROR_WANT_READ) {
				CHKiRet(nsdsel_ptcp.Add(pThis->pTcp, pNsdOSSL->pTcp, NSDSEL_RD));
				FINALIZE;
			} else if (pNsdOSSL->rtryOsslErr == SSL_ERROR_WANT_WRITE) {
				CHKiRet(nsdsel_ptcp.Add(pThis->pTcp, pNsdOSSL->pTcp, NSDSEL_WR));
				FINALIZE;
			} else {
				dbgprintf("nsdsel_ossl: rtryCall=%d, rtryOsslErr=%d, unexpected ... help?! ... \n",
					pNsdOSSL->rtryCall, pNsdOSSL->rtryOsslErr);
				ABORT_FINALIZE(RS_RET_NO_ERRCODE);
			}

		} else {
			dbgprintf("nsdsel_ossl: rtryCall=%d, nothing to do ... \n",
				pNsdOSSL->rtryCall);
		}
	}

	dbgprintf("nsdsel_ossl: reached end, calling nsdsel_ptcp.Add with waitOp %d... \n", waitOp);
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
			FINALIZE;
		}

		/* now we must ensure that we do not fall back to PTCP if we have
		 * done a "dummy" select. In that case, we know when the predicate
		 * is not matched here, we do not have data available for this
		 * socket. -- rgerhards, 2010-11-20
		 */
		if(pThis->iBufferRcvReady) {
			*pbIsReady = 0;
			FINALIZE;
		}
	}
/* // VERBOSE
dbgprintf("nsdl_ossl: IsReady before nsdsel_ptcp.IsReady for %p\n", pThis);
*/
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


/* exit our class */
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
