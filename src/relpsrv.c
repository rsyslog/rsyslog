/* The relp server.
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
#include <string.h>
#include <assert.h>
#include "relp.h"
#include "relpsrv.h"
#include "tcp.h"


/** Construct a RELP srv instance
 * This is the first thing that a caller must do before calling any
 * RELP function. The relp srv must only destructed after all RELP
 * operations have been finished.
 */
relpRetVal
relpSrvConstruct(relpSrv_t **ppThis, relpEngine_t *pEngine)
{
	relpSrv_t *pThis;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	if((pThis = calloc(1, sizeof(relpSrv_t))) == NULL) {
		ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}

	RELP_CORE_CONSTRUCTOR(pThis, Srv);
	pThis->pEngine = pEngine;
	pThis->stateCmdSyslog = pEngine->stateCmdSyslog;

pEngine->dbgprint("relp server %p constructed\n", pThis);

	*ppThis = pThis;

finalize_it:
	LEAVE_RELPFUNC;
}


/** Destruct a RELP srv instance
 */
relpRetVal
relpSrvDestruct(relpSrv_t **ppThis)
{
	relpSrv_t *pThis;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	pThis = *ppThis;
	RELPOBJ_assert(pThis, Srv);

	if(pThis->pTcp != NULL)
		relpTcpDestruct(&pThis->pTcp);

	if(pThis->pLstnPort != NULL)
		free(pThis->pLstnPort);

	/* done with de-init work, now free srv object itself */
	free(pThis);
	*ppThis = NULL;

	LEAVE_RELPFUNC;
}


/* set the user pointer inside the RELP server. Whatever value the user
 * provides is accepted.
 * rgerhards, 2008-07-08
 */
relpRetVal
relpSrvSetUsrPtr(relpSrv_t *pThis, void *pUsr)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Srv);
	pThis->pUsr = pUsr;
	LEAVE_RELPFUNC;
}


/* set the listen port inside the relp server. If NULL is provided, the default port
 * is used. The provided string is always copied, it is the caller's duty to
 * free the passed-in string.
 * rgerhards, 2008-03-17
 */
relpRetVal
relpSrvSetLstnPort(relpSrv_t *pThis, unsigned char *pLstnPort)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Srv);

	/* first free old value */
	if(pThis->pLstnPort != NULL)
		free(pThis->pLstnPort);
	pThis->pLstnPort = NULL;

	if(pLstnPort != NULL) {
		if((pThis->pLstnPort = (unsigned char*) strdup((char*)pLstnPort)) == NULL)
			ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}
		
finalize_it:
	LEAVE_RELPFUNC;
}


/* start a relp server - the server object must have all properties set
 * rgerhards, 2008-03-17
 */
relpRetVal
relpSrvRun(relpSrv_t *pThis)
{
	relpTcp_t *pTcp;

	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Srv);

	CHKRet(relpTcpConstruct(&pTcp, pThis->pEngine));
	CHKRet(relpTcpLstnInit(pTcp, (pThis->pLstnPort == NULL) ? (unsigned char*) RELP_DFLT_PORT : pThis->pLstnPort));
		
	pThis->pTcp = pTcp;

finalize_it:
	if(iRet != RELP_RET_OK) {
		if(pThis->pTcp != NULL)
			relpTcpDestruct(&pTcp);
	}

	LEAVE_RELPFUNC;
}


/* Enable or disable a command. Note that a command can not be enabled once
 * it has been set to forbidden! There will be no error return state in this
 * case.
 * rgerhards, 2008-03-27
 */
relpRetVal
relpSrvSetEnableCmd(relpSrv_t *pThis, unsigned char *pszCmd, relpCmdEnaState_t stateCmd)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Srv);
	assert(pszCmd != NULL);

pThis->pEngine->dbgprint("SRV SetEnableCmd in syslog cmd state: %d\n", pThis->stateCmdSyslog);
	if(!strcmp((char*)pszCmd, "syslog")) {
		if(pThis->stateCmdSyslog != eRelpCmdState_Forbidden)
			pThis->stateCmdSyslog = stateCmd;
	} else {
		pThis->pEngine->dbgprint("tried to set unknown command '%s' to %d\n", pszCmd, stateCmd);
		ABORT_FINALIZE(RELP_RET_UNKNOWN_CMD);
	}

finalize_it:
pThis->pEngine->dbgprint("SRV SetEnableCmd out syslog cmd state: %d, iRet %d\n", pThis->stateCmdSyslog, iRet);
	LEAVE_RELPFUNC;
}
