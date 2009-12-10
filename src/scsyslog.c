/* The command handler for server-based "syslog" (after reception)
 *
 * This command is used to transfer syslog messages.
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
#include "cmdif.h"

/* process the command
 * rgerhards, 2008-03-17
 */
BEGINcommand(S, Syslog)
	ENTER_RELPFUNC;
	pSess->pEngine->dbgprint("in 'syslog' command handler\n");

	if(pSess->stateCmdSyslog != eRelpCmdState_Enabled) {
		relpSessSendResponse(pSess, pFrame->txnr, (unsigned char*) "500 command disabled", 20);
		ABORT_FINALIZE(RELP_RET_CMD_DISABLED);
	}

	/* only highest version callback is called */
	if(pSess->pEngine->onSyslogRcv2 != NULL) {
		iRet = pSess->pEngine->onSyslogRcv2(pSess->pSrv->pUsr, pSess->pTcp->pRemHostName,
					   	    pSess->pTcp->pRemHostIP, pFrame->pData, pFrame->lenData);
	} else if(pSess->pEngine->onSyslogRcv != NULL) {
		iRet = pSess->pEngine->onSyslogRcv(pSess->pTcp->pRemHostName, pSess->pTcp->pRemHostIP,
						    pFrame->pData, pFrame->lenData);
	} else {
		pSess->pEngine->dbgprint("error: no syslog reception callback is set, nothing done\n");
	}

	/* send response */
	CHKRet(relpSessSendResponse(pSess, pFrame->txnr, (unsigned char*) "200 OK", 6));
finalize_it:
ENDcommand
