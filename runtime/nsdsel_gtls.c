/* nsdsel_gtls.c
 *
 * An implementation of the nsd select() interface for GnuTLS.
 * 
 * Copyright (C) 2008 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
 *
 * The rsyslog runtime library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The rsyslog runtime library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the rsyslog runtime library.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 * A copy of the LGPL can be found in the file "COPYING.LESSER" in this distribution.
 */
#include "config.h"

#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/select.h>
#include <gnutls/gnutls.h>

#include "rsyslog.h"
#include "module-template.h"
#include "obj.h"
#include "errmsg.h"
#include "nsd.h"
#include "nsd_gtls.h"
#include "nsdsel_ptcp.h"
#include "nsdsel_gtls.h"

MODULE_TYPE_LIB

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(errmsg)
DEFobjCurrIf(glbl)
DEFobjCurrIf(nsdsel_ptcp)


/* Standard-Constructor
 */
BEGINobjConstruct(nsdsel_gtls) /* be sure to specify the object type also in END macro! */
	iRet = nsdsel_ptcp.Construct(&pThis->pTcp);
ENDobjConstruct(nsdsel_gtls)


/* destructor for the nsdsel_gtls object */
BEGINobjDestruct(nsdsel_gtls) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(nsdsel_gtls)
	if(pThis->pTcp != NULL)
		nsdsel_ptcp.Destruct(&pThis->pTcp);
ENDobjDestruct(nsdsel_gtls)


/* Add a socket to the select set */
static rsRetVal
Add(nsdsel_t *pNsdsel, nsd_t *pNsd, nsdsel_waitOp_t waitOp)
{
	DEFiRet;
	nsdsel_gtls_t *pThis = (nsdsel_gtls_t*) pNsdsel;
	nsd_gtls_t *pNsdGTLS = (nsd_gtls_t*) pNsd;

	ISOBJ_TYPE_assert(pThis, nsdsel_gtls);
	ISOBJ_TYPE_assert(pNsdGTLS, nsd_gtls);
	iRet = nsdsel_ptcp.Add(pThis->pTcp, pNsdGTLS->pTcp, waitOp);
	RETiRet;
}


/* perform the select()  piNumReady returns how many descriptors are ready for IO 
 * TODO: add timeout!
 */
static rsRetVal
Select(nsdsel_t *pNsdsel, int *piNumReady)
{
	DEFiRet;
	nsdsel_gtls_t *pThis = (nsdsel_gtls_t*) pNsdsel;

	ISOBJ_TYPE_assert(pThis, nsdsel_gtls);
	iRet = nsdsel_ptcp.Select(pThis->pTcp, piNumReady);
	RETiRet;
}


/* check if a socket is ready for IO */
static rsRetVal
IsReady(nsdsel_t *pNsdsel, nsd_t *pNsd, nsdsel_waitOp_t waitOp, int *pbIsReady)
{
	DEFiRet;
	nsdsel_gtls_t *pThis = (nsdsel_gtls_t*) pNsdsel;
	nsd_gtls_t *pNsdGTLS = (nsd_gtls_t*) pNsd;

	ISOBJ_TYPE_assert(pThis, nsdsel_gtls);
	ISOBJ_TYPE_assert(pNsdGTLS, nsd_gtls);
	iRet = nsdsel_ptcp.IsReady(pThis->pTcp, pNsdGTLS->pTcp, waitOp, pbIsReady);
	RETiRet;
}


/* ------------------------------ end support for the select() interface ------------------------------ */


/* queryInterface function */
BEGINobjQueryInterface(nsdsel_gtls)
CODESTARTobjQueryInterface(nsdsel_gtls)
	if(pIf->ifVersion != nsdCURR_IF_VERSION) {/* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->Construct = (rsRetVal(*)(nsdsel_t**)) nsdsel_gtlsConstruct;
	pIf->Destruct = (rsRetVal(*)(nsdsel_t**)) nsdsel_gtlsDestruct;
	pIf->Add = Add;
	pIf->Select = Select;
	pIf->IsReady = IsReady;
finalize_it:
ENDobjQueryInterface(nsdsel_gtls)


/* exit our class
 */
BEGINObjClassExit(nsdsel_gtls, OBJ_IS_LOADABLE_MODULE) /* CHANGE class also in END MACRO! */
CODESTARTObjClassExit(nsdsel_gtls)
	/* release objects we no longer need */
	objRelease(glbl, CORE_COMPONENT);
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(nsdsel_ptcp, LM_NSDSEL_PTCP_FILENAME);
ENDObjClassExit(nsdsel_gtls)


/* Initialize the nsdsel_gtls class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINObjClassInit(nsdsel_gtls, 1, OBJ_IS_LOADABLE_MODULE) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(nsdsel_ptcp, LM_NSDSEL_PTCP_FILENAME));

	/* set our own handlers */
ENDObjClassInit(nsdsel_gtls)


/* --------------- here now comes the plumbing that makes as a library module --------------- */


BEGINmodExit
CODESTARTmodExit
	nsdsel_gtlsClassExit();
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_LIB_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */

	/* Initialize all classes that are in our module - this includes ourselfs */
	CHKiRet(nsdsel_gtlsClassInit(pModInfo)); /* must be done after tcps_sess, as we use it */
ENDmodInit
/* vi:set ai:
 */
