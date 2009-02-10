/* The command handler for "close"
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
 * free software while at the same time obtaining some development funding.
 */
#include "config.h"
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include "relp.h"
#include "cmdif.h"

/* process the command
 * rgerhards, 2008-03-21
 */
BEGINcommand(S, Close)
	ENTER_RELPFUNC;
	pSess->pEngine->dbgprint("in close command handler\n");

	/* and send it... */
	CHKRet(relpSessSendResponse(pSess, pFrame->txnr, (unsigned char*) "", 0));

	iRet = RELP_RET_SESSION_CLOSED; /* indicate close condition */

finalize_it:
ENDcommand
