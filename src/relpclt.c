/* The relp client.
 *
 * Copyright 2008 by Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of librelp.
 *
 * Librelp is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Librelp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Librelp.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 *
 * If the terms of the GPL are unsuitable for your needs, you may obtain
 * a commercial license from Adiscon. Contact sales@adiscon.com for further
 * details.
 *
 * ALL CONTRIBUTORS PLEASE NOTE that by sending contributions, you assign
 * your copyright to Adiscon GmbH, Germany. This is necessary to permit the
 * dual-licensing set forth here. Our apologies for this inconvenience, but
 * we sincerely believe that the dual-licensing model helps us provide great
 * free software while at the same time obtaining some funding for further
 * development.
 */
#include "config.h"
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include "relp.h"
#include "relpsess.h"
#include "relpclt.h"


/** Construct a RELP clt instance
 * This is the first thing that a caller must do before calling any
 * RELP function. The relp clt must only destructed after all RELP
 * operations have been finished.
 */
relpRetVal
relpCltConstruct(relpClt_t **ppThis, relpEngine_t *pEngine)
{
	relpClt_t *pThis;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	if((pThis = calloc(1, sizeof(relpClt_t))) == NULL) {
		ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}

	RELP_CORE_CONSTRUCTOR(pThis, Clt);
	pThis->pEngine = pEngine;

	*ppThis = pThis;

finalize_it:
	LEAVE_RELPFUNC;
}


/** Destruct a RELP clt instance
 */
relpRetVal
relpCltDestruct(relpClt_t **ppThis)
{
	relpClt_t *pThis;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	pThis = *ppThis;
	RELPOBJ_assert(pThis, Clt);

	if(pThis->pSess != NULL)
		relpSessDestruct(&pThis->pSess);

	/* done with de-init work, now free clt object itself */
	free(pThis);
	*ppThis = NULL;

	LEAVE_RELPFUNC;
}


/** open a relp session to a remote peer
 * remote servers parameters must already have been set.
 * rgerhards, 2008-03-19
 */
relpRetVal
relpCltConnect(relpClt_t *pThis, int protFamily, unsigned char *port, unsigned char *host)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Clt);

	CHKRet(relpSessConstruct(&pThis->pSess, pThis->pEngine, NULL));
	CHKRet(relpSessConnect(pThis->pSess, protFamily, port, host));

finalize_it:
	if(iRet != RELP_RET_OK) {
		if(pThis->pSess != NULL) {
			relpSessDestruct(&pThis->pSess);
		}
	}

	LEAVE_RELPFUNC;
}


/** Try to reconnect a broken session to the remote
 * server. The main difference to relpCltConnect() is that the
 * session object is already existing and session parameters (like
 * remote host) can not be changed.
 * rgerhards, 2008-03-23
 */
relpRetVal
relpCltReconnect(relpClt_t *pThis)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Clt);
	RELPOBJ_assert(pThis->pSess, Sess);

	CHKRet(relpSessTryReestablish(pThis->pSess));

finalize_it:
	LEAVE_RELPFUNC;
}


/* Send a syslog message through RELP. The session must be established.
 * The provided message buffer is not touched by this function. The caller
 * must free it if it is no longer needed.
 * rgerhards, 2008-03-20
 */
relpRetVal
relpCltSendSyslog(relpClt_t *pThis, unsigned char *pMsg, size_t lenMsg)
{

	ENTER_RELPFUNC;
	if(pThis == NULL || pThis->objID != eRelpObj_Clt)
		ABORT_FINALIZE(RELP_RET_INVALID_HDL);

	CHKRet(relpSessSendSyslog(pThis->pSess, pMsg, lenMsg));

finalize_it:
	LEAVE_RELPFUNC;
}
