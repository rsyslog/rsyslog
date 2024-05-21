/* nsdsel_mbedtls.c
 *
 * An implementation of the nsd select() interface for Mbed TLS.
 *
 * Copyright (C) 2008-2016 Adiscon GmbH.
 * Copyright (C) 2023 CS Group.
 *
 * This file is part of the rsyslog runtime library.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *	 -or-
 *	 see COPYING.ASL20 in the source distribution
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
#include "nsd.h"
#include "nsd_mbedtls.h"
#include "nsd_ptcp.h"
#include "nsdsel_ptcp.h"
#include "nsdsel_mbedtls.h"

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(nsdsel_ptcp)

/* Standard-Constructor
 */
BEGINobjConstruct(nsdsel_mbedtls) /* be sure to specify the object type also in END macro! */
	iRet = nsdsel_ptcp.Construct(&pThis->pTcp);
ENDobjConstruct(nsdsel_mbedtls)


/* destructor for the nsdsel_mbedtls object */
BEGINobjDestruct(nsdsel_mbedtls) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(nsdsel_mbedtls)
	if(pThis->pTcp != NULL)
		nsdsel_ptcp.Destruct(&pThis->pTcp);
ENDobjDestruct(nsdsel_mbedtls)


/* Add a socket to the select set */
static rsRetVal
Add(nsdsel_t *pNsdsel, nsd_t *pNsd, nsdsel_waitOp_t waitOp)
{
	DEFiRet;
	nsdsel_mbedtls_t *pThis = (nsdsel_mbedtls_t*)pNsdsel;
	nsd_mbedtls_t *pNsdMbedtls = (nsd_mbedtls_t*)pNsd;

	ISOBJ_TYPE_assert(pThis, nsdsel_mbedtls);
	ISOBJ_TYPE_assert(pNsdMbedtls, nsd_mbedtls);
	DBGPRINTF("Add on nsd %p:\n", pNsdMbedtls);
	if(pNsdMbedtls->iMode == 1) {
		if(waitOp == NSDSEL_RD && mbedtls_ssl_get_bytes_avail(&(pNsdMbedtls->ssl))) {
			++pThis->iBufferRcvReady;
			dbgprintf("nsdsel_mbedtls: data already present in buffer, initiating "
				  "dummy select %p->iBufferRcvReady=%d\n",
				  pThis, pThis->iBufferRcvReady);
			FINALIZE;
		}
	}

	dbgprintf("nsdsel_mbedtls: reached end on nsd %p, calling nsdsel_ptcp.Add with waitOp %d... \n",
		  pNsdMbedtls, waitOp);
	/* if we reach this point, we need no special handling */
	CHKiRet(nsdsel_ptcp.Add(pThis->pTcp, pNsdMbedtls->pTcp, waitOp));

finalize_it:
	RETiRet;
}

/* perform the select()	 piNumReady returns how many descriptors are ready for IO
 * TODO: add timeout!
 */
static rsRetVal
Select(nsdsel_t *pNsdsel, int *piNumReady)
{
	DEFiRet;
	nsdsel_mbedtls_t *pThis = (nsdsel_mbedtls_t*)pNsdsel;

	ISOBJ_TYPE_assert(pThis, nsdsel_mbedtls);
	if(pThis->iBufferRcvReady > 0) {
		/* we still have data ready! */
		*piNumReady = pThis->iBufferRcvReady;
		dbgprintf("nsdsel_mbedtls: doing dummy select for %p->iBufferRcvReady=%d, data present\n",
			  pThis, pThis->iBufferRcvReady);
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
	nsdsel_mbedtls_t *pThis = (nsdsel_mbedtls_t*)pNsdsel;
	nsd_mbedtls_t *pNsdMbedtls = (nsd_mbedtls_t*)pNsd;

	ISOBJ_TYPE_assert(pThis, nsdsel_mbedtls);
	ISOBJ_TYPE_assert(pNsdMbedtls, nsd_mbedtls);
	if(pNsdMbedtls->iMode == 1) {
		if(waitOp == NSDSEL_RD && mbedtls_ssl_get_bytes_avail(&(pNsdMbedtls->ssl)) > 0) {
			*pbIsReady = 1;
			--pThis->iBufferRcvReady; /* one "pseudo-read" less */
			dbgprintf("nsdl_mbedtls: dummy read, decrementing %p->iBufRcvReady, now %d\n",
				  pThis, pThis->iBufferRcvReady);
			FINALIZE;
		}
	}

	/* now we must ensure that we do not fall back to PTCP if we have
	 * done a "dummy" select. In that case, we know when the predicate
	 * is not matched here, we do not have data available for this
	 * socket. -- rgerhards, 2010-11-20
	 */
	if(pThis->iBufferRcvReady) {
		dbgprintf("nsd_mbedtls: dummy read, %p->buffer not available for this FD\n", pThis);
		*pbIsReady = 0;
		FINALIZE;
	}

	CHKiRet(nsdsel_ptcp.IsReady(pThis->pTcp, pNsdMbedtls->pTcp, waitOp, pbIsReady));

finalize_it:
	RETiRet;
}


/* ------------------------------ end support for the select() interface ------------------------------ */


/* queryInterface function */
BEGINobjQueryInterface(nsdsel_mbedtls)
CODESTARTobjQueryInterface(nsdsel_mbedtls)
	if(pIf->ifVersion != nsdCURR_IF_VERSION) {/* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->Construct = (rsRetVal(*)(nsdsel_t**))nsdsel_mbedtlsConstruct;
	pIf->Destruct = (rsRetVal(*)(nsdsel_t**))nsdsel_mbedtlsDestruct;
	pIf->Add = Add;
	pIf->Select = Select;
	pIf->IsReady = IsReady;
finalize_it:
ENDobjQueryInterface(nsdsel_mbedtls)


/* exit our class
 */
BEGINObjClassExit(nsdsel_mbedtls, OBJ_IS_CORE_MODULE) /* CHANGE class also in END MACRO! */
CODESTARTObjClassExit(nsdsel_mbedtls)
	/* release objects we no longer need */
	objRelease(nsdsel_ptcp, LM_NSD_PTCP_FILENAME);
ENDObjClassExit(nsdsel_mbedtls)


/* Initialize the nsdsel_mbedtls class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINObjClassInit(nsdsel_mbedtls, 1, OBJ_IS_CORE_MODULE) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(nsdsel_ptcp, LM_NSD_PTCP_FILENAME));

/* set our own handlers */
ENDObjClassInit(nsdsel_mbedtls)
